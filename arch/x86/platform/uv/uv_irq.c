/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SGI UV IRQ functions
 *
 * Copyright (C) 2008 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/irq.h>

#include <asm/apic.h>
#include <asm/uv/uv_irq.h>
#include <asm/uv/uv_hub.h>

struct uv_irq_2_mmr_pnode{
	struct rb_node		list;
	unsigned long		offset;
	int			pnode;
	int			irq;
};

static DEFINE_SPINLOCK(uv_irq_lock);
static struct rb_root		uv_irq_root;

static int uv_set_irq_affinity(struct irq_data *, const struct cpumask *, bool);

static void uv_noop(struct irq_data *data) { }

static void uv_ack_apic(struct irq_data *data)
{
	ack_APIC_irq();
}

static struct irq_chip uv_irq_chip = {
	.name			= "UV-CORE",
	.irq_mask		= uv_noop,
	.irq_unmask		= uv_noop,
	.irq_eoi		= uv_ack_apic,
	.irq_set_affinity	= uv_set_irq_affinity,
};

static int uv_set_irq_2_mmr_info(int irq, unsigned long offset, unsigned blade)
{
	struct rb_node **link = &uv_irq_root.rb_node;
	struct rb_node *parent = NULL;
	struct uv_irq_2_mmr_pnode *n;
	struct uv_irq_2_mmr_pnode *e;
	unsigned long irqflags;

	n = kmalloc_node(sizeof(struct uv_irq_2_mmr_pnode), GFP_KERNEL,
				uv_blade_to_memory_nid(blade));
	if (!n)
		return -ENOMEM;

	n->irq = irq;
	n->offset = offset;
	n->pnode = uv_blade_to_pnode(blade);
	spin_lock_irqsave(&uv_irq_lock, irqflags);
	
	while (*link) {
		parent = *link;
		e = rb_entry(parent, struct uv_irq_2_mmr_pnode, list);

		if (unlikely(irq == e->irq)) {
			
			e->pnode = uv_blade_to_pnode(blade);
			e->offset = offset;
			spin_unlock_irqrestore(&uv_irq_lock, irqflags);
			kfree(n);
			return 0;
		}

		if (irq < e->irq)
			link = &(*link)->rb_left;
		else
			link = &(*link)->rb_right;
	}

	
	rb_link_node(&n->list, parent, link);
	rb_insert_color(&n->list, &uv_irq_root);

	spin_unlock_irqrestore(&uv_irq_lock, irqflags);
	return 0;
}

int uv_irq_2_mmr_info(int irq, unsigned long *offset, int *pnode)
{
	struct uv_irq_2_mmr_pnode *e;
	struct rb_node *n;
	unsigned long irqflags;

	spin_lock_irqsave(&uv_irq_lock, irqflags);
	n = uv_irq_root.rb_node;
	while (n) {
		e = rb_entry(n, struct uv_irq_2_mmr_pnode, list);

		if (e->irq == irq) {
			*offset = e->offset;
			*pnode = e->pnode;
			spin_unlock_irqrestore(&uv_irq_lock, irqflags);
			return 0;
		}

		if (irq < e->irq)
			n = n->rb_left;
		else
			n = n->rb_right;
	}
	spin_unlock_irqrestore(&uv_irq_lock, irqflags);
	return -1;
}

static int
arch_enable_uv_irq(char *irq_name, unsigned int irq, int cpu, int mmr_blade,
		       unsigned long mmr_offset, int limit)
{
	const struct cpumask *eligible_cpu = cpumask_of(cpu);
	struct irq_cfg *cfg = irq_get_chip_data(irq);
	unsigned long mmr_value;
	struct uv_IO_APIC_route_entry *entry;
	int mmr_pnode, err;

	BUILD_BUG_ON(sizeof(struct uv_IO_APIC_route_entry) !=
			sizeof(unsigned long));

	err = assign_irq_vector(irq, cfg, eligible_cpu);
	if (err != 0)
		return err;

	if (limit == UV_AFFINITY_CPU)
		irq_set_status_flags(irq, IRQ_NO_BALANCING);
	else
		irq_set_status_flags(irq, IRQ_MOVE_PCNTXT);

	irq_set_chip_and_handler_name(irq, &uv_irq_chip, handle_percpu_irq,
				      irq_name);

	mmr_value = 0;
	entry = (struct uv_IO_APIC_route_entry *)&mmr_value;
	entry->vector		= cfg->vector;
	entry->delivery_mode	= apic->irq_delivery_mode;
	entry->dest_mode	= apic->irq_dest_mode;
	entry->polarity		= 0;
	entry->trigger		= 0;
	entry->mask		= 0;
	entry->dest		= apic->cpu_mask_to_apicid(eligible_cpu);

	mmr_pnode = uv_blade_to_pnode(mmr_blade);
	uv_write_global_mmr64(mmr_pnode, mmr_offset, mmr_value);

	if (cfg->move_in_progress)
		send_cleanup_vector(cfg);

	return irq;
}

static void arch_disable_uv_irq(int mmr_pnode, unsigned long mmr_offset)
{
	unsigned long mmr_value;
	struct uv_IO_APIC_route_entry *entry;

	BUILD_BUG_ON(sizeof(struct uv_IO_APIC_route_entry) !=
			sizeof(unsigned long));

	mmr_value = 0;
	entry = (struct uv_IO_APIC_route_entry *)&mmr_value;
	entry->mask = 1;

	uv_write_global_mmr64(mmr_pnode, mmr_offset, mmr_value);
}

static int
uv_set_irq_affinity(struct irq_data *data, const struct cpumask *mask,
		    bool force)
{
	struct irq_cfg *cfg = data->chip_data;
	unsigned int dest;
	unsigned long mmr_value, mmr_offset;
	struct uv_IO_APIC_route_entry *entry;
	int mmr_pnode;

	if (__ioapic_set_affinity(data, mask, &dest))
		return -1;

	mmr_value = 0;
	entry = (struct uv_IO_APIC_route_entry *)&mmr_value;

	entry->vector		= cfg->vector;
	entry->delivery_mode	= apic->irq_delivery_mode;
	entry->dest_mode	= apic->irq_dest_mode;
	entry->polarity		= 0;
	entry->trigger		= 0;
	entry->mask		= 0;
	entry->dest		= dest;

	
	if (uv_irq_2_mmr_info(data->irq, &mmr_offset, &mmr_pnode))
		return -1;

	uv_write_global_mmr64(mmr_pnode, mmr_offset, mmr_value);

	if (cfg->move_in_progress)
		send_cleanup_vector(cfg);

	return 0;
}

int uv_setup_irq(char *irq_name, int cpu, int mmr_blade,
		 unsigned long mmr_offset, int limit)
{
	int irq, ret;

	irq = create_irq_nr(NR_IRQS_LEGACY, uv_blade_to_memory_nid(mmr_blade));

	if (irq <= 0)
		return -EBUSY;

	ret = arch_enable_uv_irq(irq_name, irq, cpu, mmr_blade, mmr_offset,
		limit);
	if (ret == irq)
		uv_set_irq_2_mmr_info(irq, mmr_offset, mmr_blade);
	else
		destroy_irq(irq);

	return ret;
}
EXPORT_SYMBOL_GPL(uv_setup_irq);

void uv_teardown_irq(unsigned int irq)
{
	struct uv_irq_2_mmr_pnode *e;
	struct rb_node *n;
	unsigned long irqflags;

	spin_lock_irqsave(&uv_irq_lock, irqflags);
	n = uv_irq_root.rb_node;
	while (n) {
		e = rb_entry(n, struct uv_irq_2_mmr_pnode, list);
		if (e->irq == irq) {
			arch_disable_uv_irq(e->pnode, e->offset);
			rb_erase(n, &uv_irq_root);
			kfree(e);
			break;
		}
		if (irq < e->irq)
			n = n->rb_left;
		else
			n = n->rb_right;
	}
	spin_unlock_irqrestore(&uv_irq_lock, irqflags);
	destroy_irq(irq);
}
EXPORT_SYMBOL_GPL(uv_teardown_irq);
