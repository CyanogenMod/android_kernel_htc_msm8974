/*
 *  Support for the interrupt controllers found on Power Macintosh,
 *  currently Apple's "Grand Central" interrupt controller in all
 *  it's incarnations. OpenPIC support used on newer machines is
 *  in a separate file
 *
 *  Copyright (C) 1997 Paul Mackerras (paulus@samba.org)
 *  Copyright (C) 2005 Benjamin Herrenschmidt (benh@kernel.crashing.org)
 *                     IBM, Corp.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/adb.h>
#include <linux/pmu.h>

#include <asm/sections.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/time.h>
#include <asm/pmac_feature.h>
#include <asm/mpic.h>
#include <asm/xmon.h>

#include "pmac.h"

#ifdef CONFIG_PPC32
struct pmac_irq_hw {
        unsigned int    event;
        unsigned int    enable;
        unsigned int    ack;
        unsigned int    level;
};

unsigned int of_irq_workarounds;
struct device_node *of_irq_dflt_pic;

static volatile struct pmac_irq_hw __iomem *pmac_irq_hw[4];

static int max_irqs;
static int max_real_irqs;

static DEFINE_RAW_SPINLOCK(pmac_pic_lock);

static DECLARE_BITMAP(ppc_lost_interrupts, 128);
static DECLARE_BITMAP(ppc_cached_irq_mask, 128);
static int pmac_irq_cascade = -1;
static struct irq_domain *pmac_pic_host;

static void __pmac_retrigger(unsigned int irq_nr)
{
	if (irq_nr >= max_real_irqs && pmac_irq_cascade > 0) {
		__set_bit(irq_nr, ppc_lost_interrupts);
		irq_nr = pmac_irq_cascade;
		mb();
	}
	if (!__test_and_set_bit(irq_nr, ppc_lost_interrupts)) {
		atomic_inc(&ppc_n_lost_interrupts);
		set_dec(1);
	}
}

static void pmac_mask_and_ack_irq(struct irq_data *d)
{
	unsigned int src = irqd_to_hwirq(d);
        unsigned long bit = 1UL << (src & 0x1f);
        int i = src >> 5;
        unsigned long flags;

	raw_spin_lock_irqsave(&pmac_pic_lock, flags);
        __clear_bit(src, ppc_cached_irq_mask);
        if (__test_and_clear_bit(src, ppc_lost_interrupts))
                atomic_dec(&ppc_n_lost_interrupts);
        out_le32(&pmac_irq_hw[i]->enable, ppc_cached_irq_mask[i]);
        out_le32(&pmac_irq_hw[i]->ack, bit);
        do {
                mb();
        } while((in_le32(&pmac_irq_hw[i]->enable) & bit)
                != (ppc_cached_irq_mask[i] & bit));
	raw_spin_unlock_irqrestore(&pmac_pic_lock, flags);
}

static void pmac_ack_irq(struct irq_data *d)
{
	unsigned int src = irqd_to_hwirq(d);
        unsigned long bit = 1UL << (src & 0x1f);
        int i = src >> 5;
        unsigned long flags;

	raw_spin_lock_irqsave(&pmac_pic_lock, flags);
	if (__test_and_clear_bit(src, ppc_lost_interrupts))
                atomic_dec(&ppc_n_lost_interrupts);
        out_le32(&pmac_irq_hw[i]->ack, bit);
        (void)in_le32(&pmac_irq_hw[i]->ack);
	raw_spin_unlock_irqrestore(&pmac_pic_lock, flags);
}

static void __pmac_set_irq_mask(unsigned int irq_nr, int nokicklost)
{
        unsigned long bit = 1UL << (irq_nr & 0x1f);
        int i = irq_nr >> 5;

        if ((unsigned)irq_nr >= max_irqs)
                return;

        
        out_le32(&pmac_irq_hw[i]->enable, ppc_cached_irq_mask[i]);

        do {
                mb();
        } while((in_le32(&pmac_irq_hw[i]->enable) & bit)
                != (ppc_cached_irq_mask[i] & bit));

        if (bit & ppc_cached_irq_mask[i] & in_le32(&pmac_irq_hw[i]->level))
		__pmac_retrigger(irq_nr);
}

static unsigned int pmac_startup_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int src = irqd_to_hwirq(d);
        unsigned long bit = 1UL << (src & 0x1f);
        int i = src >> 5;

	raw_spin_lock_irqsave(&pmac_pic_lock, flags);
	if (!irqd_is_level_type(d))
		out_le32(&pmac_irq_hw[i]->ack, bit);
        __set_bit(src, ppc_cached_irq_mask);
        __pmac_set_irq_mask(src, 0);
	raw_spin_unlock_irqrestore(&pmac_pic_lock, flags);

	return 0;
}

static void pmac_mask_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int src = irqd_to_hwirq(d);

	raw_spin_lock_irqsave(&pmac_pic_lock, flags);
        __clear_bit(src, ppc_cached_irq_mask);
        __pmac_set_irq_mask(src, 1);
	raw_spin_unlock_irqrestore(&pmac_pic_lock, flags);
}

static void pmac_unmask_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int src = irqd_to_hwirq(d);

	raw_spin_lock_irqsave(&pmac_pic_lock, flags);
	__set_bit(src, ppc_cached_irq_mask);
        __pmac_set_irq_mask(src, 0);
	raw_spin_unlock_irqrestore(&pmac_pic_lock, flags);
}

static int pmac_retrigger(struct irq_data *d)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&pmac_pic_lock, flags);
	__pmac_retrigger(irqd_to_hwirq(d));
	raw_spin_unlock_irqrestore(&pmac_pic_lock, flags);
	return 1;
}

static struct irq_chip pmac_pic = {
	.name		= "PMAC-PIC",
	.irq_startup	= pmac_startup_irq,
	.irq_mask	= pmac_mask_irq,
	.irq_ack	= pmac_ack_irq,
	.irq_mask_ack	= pmac_mask_and_ack_irq,
	.irq_unmask	= pmac_unmask_irq,
	.irq_retrigger	= pmac_retrigger,
};

static irqreturn_t gatwick_action(int cpl, void *dev_id)
{
	unsigned long flags;
	int irq, bits;
	int rc = IRQ_NONE;

	raw_spin_lock_irqsave(&pmac_pic_lock, flags);
	for (irq = max_irqs; (irq -= 32) >= max_real_irqs; ) {
		int i = irq >> 5;
		bits = in_le32(&pmac_irq_hw[i]->event) | ppc_lost_interrupts[i];
		bits |= in_le32(&pmac_irq_hw[i]->level);
		bits &= ppc_cached_irq_mask[i];
		if (bits == 0)
			continue;
		irq += __ilog2(bits);
		raw_spin_unlock_irqrestore(&pmac_pic_lock, flags);
		generic_handle_irq(irq);
		raw_spin_lock_irqsave(&pmac_pic_lock, flags);
		rc = IRQ_HANDLED;
	}
	raw_spin_unlock_irqrestore(&pmac_pic_lock, flags);
	return rc;
}

static unsigned int pmac_pic_get_irq(void)
{
	int irq;
	unsigned long bits = 0;
	unsigned long flags;

#ifdef CONFIG_PPC_PMAC32_PSURGE
	
	if (smp_processor_id() != 0) {
		return  psurge_secondary_virq;
        }
#endif 
	raw_spin_lock_irqsave(&pmac_pic_lock, flags);
	for (irq = max_real_irqs; (irq -= 32) >= 0; ) {
		int i = irq >> 5;
		bits = in_le32(&pmac_irq_hw[i]->event) | ppc_lost_interrupts[i];
		bits |= in_le32(&pmac_irq_hw[i]->level);
		bits &= ppc_cached_irq_mask[i];
		if (bits == 0)
			continue;
		irq += __ilog2(bits);
		break;
	}
	raw_spin_unlock_irqrestore(&pmac_pic_lock, flags);
	if (unlikely(irq < 0))
		return NO_IRQ;
	return irq_linear_revmap(pmac_pic_host, irq);
}

#ifdef CONFIG_XMON
static struct irqaction xmon_action = {
	.handler	= xmon_irq,
	.flags		= 0,
	.name		= "NMI - XMON"
};
#endif

static struct irqaction gatwick_cascade_action = {
	.handler	= gatwick_action,
	.name		= "cascade",
};

static int pmac_pic_host_match(struct irq_domain *h, struct device_node *node)
{
	
	return 1;
}

static int pmac_pic_host_map(struct irq_domain *h, unsigned int virq,
			     irq_hw_number_t hw)
{
	if (hw >= max_irqs)
		return -EINVAL;

	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &pmac_pic, handle_level_irq);
	return 0;
}

static const struct irq_domain_ops pmac_pic_host_ops = {
	.match = pmac_pic_host_match,
	.map = pmac_pic_host_map,
	.xlate = irq_domain_xlate_onecell,
};

static void __init pmac_pic_probe_oldstyle(void)
{
        int i;
        struct device_node *master = NULL;
	struct device_node *slave = NULL;
	u8 __iomem *addr;
	struct resource r;

	
	ppc_md.get_irq = pmac_pic_get_irq;


	if ((master = of_find_node_by_name(NULL, "gc")) != NULL) {
		max_irqs = max_real_irqs = 32;
	} else if ((master = of_find_node_by_name(NULL, "ohare")) != NULL) {
		max_irqs = max_real_irqs = 32;
		
		slave = of_find_node_by_name(NULL, "pci106b,7");
		if (slave)
			max_irqs = 64;
	} else if ((master = of_find_node_by_name(NULL, "mac-io")) != NULL) {
		max_irqs = max_real_irqs = 64;

		
		slave = of_find_node_by_name(master, "mac-io");

		
		if (of_device_is_compatible(master, "gatwick")) {
			struct device_node *tmp;
			BUG_ON(slave == NULL);
			tmp = master;
			master = slave;
			slave = tmp;
		}

		
		if (slave)
			max_irqs = 128;
	}
	BUG_ON(master == NULL);

	pmac_pic_host = irq_domain_add_linear(master, max_irqs,
					      &pmac_pic_host_ops, NULL);
	BUG_ON(pmac_pic_host == NULL);
	irq_set_default_host(pmac_pic_host);

	
	BUG_ON(of_address_to_resource(master, 0, &r));

	
	addr = (u8 __iomem *) ioremap(r.start, 0x40);
	i = 0;
	pmac_irq_hw[i++] = (volatile struct pmac_irq_hw __iomem *)
		(addr + 0x20);
	if (max_real_irqs > 32)
		pmac_irq_hw[i++] = (volatile struct pmac_irq_hw __iomem *)
			(addr + 0x10);
	of_node_put(master);

	printk(KERN_INFO "irq: Found primary Apple PIC %s for %d irqs\n",
	       master->full_name, max_real_irqs);

	
	if (slave && !of_address_to_resource(slave, 0, &r)) {
		addr = (u8 __iomem *)ioremap(r.start, 0x40);
		pmac_irq_hw[i++] = (volatile struct pmac_irq_hw __iomem *)
			(addr + 0x20);
		if (max_irqs > 64)
			pmac_irq_hw[i++] =
				(volatile struct pmac_irq_hw __iomem *)
				(addr + 0x10);
		pmac_irq_cascade = irq_of_parse_and_map(slave, 0);

		printk(KERN_INFO "irq: Found slave Apple PIC %s for %d irqs"
		       " cascade: %d\n", slave->full_name,
		       max_irqs - max_real_irqs, pmac_irq_cascade);
	}
	of_node_put(slave);

	
	for (i = 0; i * 32 < max_irqs; ++i)
		out_le32(&pmac_irq_hw[i]->enable, 0);

	
	if (slave && pmac_irq_cascade != NO_IRQ)
		setup_irq(pmac_irq_cascade, &gatwick_cascade_action);

	printk(KERN_INFO "irq: System has %d possible interrupts\n", max_irqs);
#ifdef CONFIG_XMON
	setup_irq(irq_create_mapping(NULL, 20), &xmon_action);
#endif
}

int of_irq_map_oldworld(struct device_node *device, int index,
			struct of_irq *out_irq)
{
	const u32 *ints = NULL;
	int intlen;

	while (device) {
		ints = of_get_property(device, "AAPL,interrupts", &intlen);
		if (ints != NULL)
			break;
		device = device->parent;
		if (device && strcmp(device->type, "pci") != 0)
			break;
	}
	if (ints == NULL)
		return -EINVAL;
	intlen /= sizeof(u32);

	if (index >= intlen)
		return -EINVAL;

	out_irq->controller = NULL;
	out_irq->specifier[0] = ints[index];
	out_irq->size = 1;

	return 0;
}
#endif 

static void __init pmac_pic_setup_mpic_nmi(struct mpic *mpic)
{
#if defined(CONFIG_XMON) && defined(CONFIG_PPC32)
	struct device_node* pswitch;
	int nmi_irq;

	pswitch = of_find_node_by_name(NULL, "programmer-switch");
	if (pswitch) {
		nmi_irq = irq_of_parse_and_map(pswitch, 0);
		if (nmi_irq != NO_IRQ) {
			mpic_irq_set_priority(nmi_irq, 9);
			setup_irq(nmi_irq, &xmon_action);
		}
		of_node_put(pswitch);
	}
#endif	
}

static struct mpic * __init pmac_setup_one_mpic(struct device_node *np,
						int master)
{
	const char *name = master ? " MPIC 1   " : " MPIC 2   ";
	struct mpic *mpic;
	unsigned int flags = master ? 0 : MPIC_SECONDARY;

	pmac_call_feature(PMAC_FTR_ENABLE_MPIC, np, 0, 0);

	if (of_get_property(np, "big-endian", NULL))
		flags |= MPIC_BIG_ENDIAN;

	if (master && (flags & MPIC_BIG_ENDIAN))
		flags |= MPIC_U3_HT_IRQS;

	mpic = mpic_alloc(np, 0, flags, 0, 0, name);
	if (mpic == NULL)
		return NULL;

	mpic_init(mpic);

	return mpic;
 }

static int __init pmac_pic_probe_mpic(void)
{
	struct mpic *mpic1, *mpic2;
	struct device_node *np, *master = NULL, *slave = NULL;

	
	for (np = NULL; (np = of_find_node_by_type(np, "open-pic"))
		     != NULL;) {
		if (master == NULL &&
		    of_get_property(np, "interrupts", NULL) == NULL)
			master = of_node_get(np);
		else if (slave == NULL)
			slave = of_node_get(np);
		if (master && slave)
			break;
	}

	
	if (master == NULL && slave != NULL) {
		master = slave;
		slave = NULL;
	}

	
	if (master == NULL)
		return -ENODEV;

	
	ppc_md.get_irq = mpic_get_irq;

	
	mpic1 = pmac_setup_one_mpic(master, 1);
	BUG_ON(mpic1 == NULL);

	
	pmac_pic_setup_mpic_nmi(mpic1);

	of_node_put(master);

	
	if (slave) {
		mpic2 = pmac_setup_one_mpic(slave, 0);
		if (mpic2 == NULL)
			printk(KERN_ERR "Failed to setup slave MPIC\n");
		of_node_put(slave);
	}

	return 0;
}


void __init pmac_pic_init(void)
{
#ifdef CONFIG_PPC32
	if (!pmac_newworld)
		of_irq_workarounds |= OF_IMAP_OLDWORLD_MAC;
	if (of_get_property(of_chosen, "linux,bootx", NULL) != NULL)
		of_irq_workarounds |= OF_IMAP_NO_PHANDLE;

	if (pmac_newworld && (of_irq_workarounds & OF_IMAP_NO_PHANDLE)) {
		struct device_node *np;

		for_each_node_with_property(np, "interrupt-controller") {
			
			if (strcmp(np->name, "chosen") == 0)
				continue;
			if (strcmp(np->name, "AppleKiwi") == 0)
				continue;
			
			of_irq_dflt_pic = np;
			break;
		}
	}
#endif 

	if (pmac_pic_probe_mpic() == 0)
		return;

#ifdef CONFIG_PPC32
	pmac_pic_probe_oldstyle();
#endif
}

#if defined(CONFIG_PM) && defined(CONFIG_PPC32)
unsigned long sleep_save_mask[2];

static int pmacpic_find_viaint(void)
{
	int viaint = -1;

#ifdef CONFIG_ADB_PMU
	struct device_node *np;

	if (pmu_get_model() != PMU_OHARE_BASED)
		goto not_found;
	np = of_find_node_by_name(NULL, "via-pmu");
	if (np == NULL)
		goto not_found;
	viaint = irq_of_parse_and_map(np, 0);

not_found:
#endif 
	return viaint;
}

static int pmacpic_suspend(void)
{
	int viaint = pmacpic_find_viaint();

	sleep_save_mask[0] = ppc_cached_irq_mask[0];
	sleep_save_mask[1] = ppc_cached_irq_mask[1];
	ppc_cached_irq_mask[0] = 0;
	ppc_cached_irq_mask[1] = 0;
	if (viaint > 0)
		set_bit(viaint, ppc_cached_irq_mask);
	out_le32(&pmac_irq_hw[0]->enable, ppc_cached_irq_mask[0]);
	if (max_real_irqs > 32)
		out_le32(&pmac_irq_hw[1]->enable, ppc_cached_irq_mask[1]);
	(void)in_le32(&pmac_irq_hw[0]->event);
	
	mb();
        (void)in_le32(&pmac_irq_hw[0]->enable);

        return 0;
}

static void pmacpic_resume(void)
{
	int i;

	out_le32(&pmac_irq_hw[0]->enable, 0);
	if (max_real_irqs > 32)
		out_le32(&pmac_irq_hw[1]->enable, 0);
	mb();
	for (i = 0; i < max_real_irqs; ++i)
		if (test_bit(i, sleep_save_mask))
			pmac_unmask_irq(irq_get_irq_data(i));
}

static struct syscore_ops pmacpic_syscore_ops = {
	.suspend	= pmacpic_suspend,
	.resume		= pmacpic_resume,
};

static int __init init_pmacpic_syscore(void)
{
	if (pmac_irq_hw[0])
		register_syscore_ops(&pmacpic_syscore_ops);
	return 0;
}

machine_subsys_initcall(powermac, init_pmacpic_syscore);

#endif 
