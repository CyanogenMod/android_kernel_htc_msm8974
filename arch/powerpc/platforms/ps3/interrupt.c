/*
 *  PS3 interrupt routines.
 *
 *  Copyright (C) 2006 Sony Computer Entertainment Inc.
 *  Copyright 2006 Sony Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/irq.h>

#include <asm/machdep.h>
#include <asm/udbg.h>
#include <asm/lv1call.h>
#include <asm/smp.h>

#include "platform.h"

#if defined(DEBUG)
#define DBG udbg_printf
#define FAIL udbg_printf
#else
#define DBG pr_devel
#define FAIL pr_debug
#endif


#define PS3_BMP_MINALIGN 64

struct ps3_bmp {
	struct {
		u64 status;
		u64 unused_1[3];
		unsigned long mask;
		u64 unused_2[3];
	};
};


struct ps3_private {
	struct ps3_bmp bmp __attribute__ ((aligned (PS3_BMP_MINALIGN)));
	spinlock_t bmp_lock;
	u64 ppe_id;
	u64 thread_id;
	unsigned long ipi_debug_brk_mask;
	unsigned long ipi_mask;
};

static DEFINE_PER_CPU(struct ps3_private, ps3_private);


static void ps3_chip_mask(struct irq_data *d)
{
	struct ps3_private *pd = irq_data_get_irq_chip_data(d);
	unsigned long flags;

	DBG("%s:%d: thread_id %llu, virq %d\n", __func__, __LINE__,
		pd->thread_id, d->irq);

	local_irq_save(flags);
	clear_bit(63 - d->irq, &pd->bmp.mask);
	lv1_did_update_interrupt_mask(pd->ppe_id, pd->thread_id);
	local_irq_restore(flags);
}


static void ps3_chip_unmask(struct irq_data *d)
{
	struct ps3_private *pd = irq_data_get_irq_chip_data(d);
	unsigned long flags;

	DBG("%s:%d: thread_id %llu, virq %d\n", __func__, __LINE__,
		pd->thread_id, d->irq);

	local_irq_save(flags);
	set_bit(63 - d->irq, &pd->bmp.mask);
	lv1_did_update_interrupt_mask(pd->ppe_id, pd->thread_id);
	local_irq_restore(flags);
}


static void ps3_chip_eoi(struct irq_data *d)
{
	const struct ps3_private *pd = irq_data_get_irq_chip_data(d);

	

	if (!test_bit(63 - d->irq, &pd->ipi_mask))
		lv1_end_of_interrupt_ext(pd->ppe_id, pd->thread_id, d->irq);
}


static struct irq_chip ps3_irq_chip = {
	.name = "ps3",
	.irq_mask = ps3_chip_mask,
	.irq_unmask = ps3_chip_unmask,
	.irq_eoi = ps3_chip_eoi,
};


static int ps3_virq_setup(enum ps3_cpu_binding cpu, unsigned long outlet,
			  unsigned int *virq)
{
	int result;
	struct ps3_private *pd;

	

	if (cpu == PS3_BINDING_CPU_ANY)
		cpu = 0;

	pd = &per_cpu(ps3_private, cpu);

	*virq = irq_create_mapping(NULL, outlet);

	if (*virq == NO_IRQ) {
		FAIL("%s:%d: irq_create_mapping failed: outlet %lu\n",
			__func__, __LINE__, outlet);
		result = -ENOMEM;
		goto fail_create;
	}

	DBG("%s:%d: outlet %lu => cpu %u, virq %u\n", __func__, __LINE__,
		outlet, cpu, *virq);

	result = irq_set_chip_data(*virq, pd);

	if (result) {
		FAIL("%s:%d: irq_set_chip_data failed\n",
			__func__, __LINE__);
		goto fail_set;
	}

	ps3_chip_mask(irq_get_irq_data(*virq));

	return result;

fail_set:
	irq_dispose_mapping(*virq);
fail_create:
	return result;
}


static int ps3_virq_destroy(unsigned int virq)
{
	const struct ps3_private *pd = irq_get_chip_data(virq);

	DBG("%s:%d: ppe_id %llu, thread_id %llu, virq %u\n", __func__,
		__LINE__, pd->ppe_id, pd->thread_id, virq);

	irq_set_chip_data(virq, NULL);
	irq_dispose_mapping(virq);

	DBG("%s:%d <-\n", __func__, __LINE__);
	return 0;
}


int ps3_irq_plug_setup(enum ps3_cpu_binding cpu, unsigned long outlet,
	unsigned int *virq)
{
	int result;
	struct ps3_private *pd;

	result = ps3_virq_setup(cpu, outlet, virq);

	if (result) {
		FAIL("%s:%d: ps3_virq_setup failed\n", __func__, __LINE__);
		goto fail_setup;
	}

	pd = irq_get_chip_data(*virq);

	

	result = lv1_connect_irq_plug_ext(pd->ppe_id, pd->thread_id, *virq,
		outlet, 0);

	if (result) {
		FAIL("%s:%d: lv1_connect_irq_plug_ext failed: %s\n",
		__func__, __LINE__, ps3_result(result));
		result = -EPERM;
		goto fail_connect;
	}

	return result;

fail_connect:
	ps3_virq_destroy(*virq);
fail_setup:
	return result;
}
EXPORT_SYMBOL_GPL(ps3_irq_plug_setup);


int ps3_irq_plug_destroy(unsigned int virq)
{
	int result;
	const struct ps3_private *pd = irq_get_chip_data(virq);

	DBG("%s:%d: ppe_id %llu, thread_id %llu, virq %u\n", __func__,
		__LINE__, pd->ppe_id, pd->thread_id, virq);

	ps3_chip_mask(irq_get_irq_data(virq));

	result = lv1_disconnect_irq_plug_ext(pd->ppe_id, pd->thread_id, virq);

	if (result)
		FAIL("%s:%d: lv1_disconnect_irq_plug_ext failed: %s\n",
		__func__, __LINE__, ps3_result(result));

	ps3_virq_destroy(virq);

	return result;
}
EXPORT_SYMBOL_GPL(ps3_irq_plug_destroy);


int ps3_event_receive_port_setup(enum ps3_cpu_binding cpu, unsigned int *virq)
{
	int result;
	u64 outlet;

	result = lv1_construct_event_receive_port(&outlet);

	if (result) {
		FAIL("%s:%d: lv1_construct_event_receive_port failed: %s\n",
			__func__, __LINE__, ps3_result(result));
		*virq = NO_IRQ;
		return result;
	}

	result = ps3_irq_plug_setup(cpu, outlet, virq);
	BUG_ON(result);

	return result;
}
EXPORT_SYMBOL_GPL(ps3_event_receive_port_setup);


int ps3_event_receive_port_destroy(unsigned int virq)
{
	int result;

	DBG(" -> %s:%d virq %u\n", __func__, __LINE__, virq);

	ps3_chip_mask(irq_get_irq_data(virq));

	result = lv1_destruct_event_receive_port(virq_to_hw(virq));

	if (result)
		FAIL("%s:%d: lv1_destruct_event_receive_port failed: %s\n",
			__func__, __LINE__, ps3_result(result));


	DBG(" <- %s:%d\n", __func__, __LINE__);
	return result;
}

int ps3_send_event_locally(unsigned int virq)
{
	return lv1_send_event_locally(virq_to_hw(virq));
}


int ps3_sb_event_receive_port_setup(struct ps3_system_bus_device *dev,
	enum ps3_cpu_binding cpu, unsigned int *virq)
{
	

	int result;

	result = ps3_event_receive_port_setup(cpu, virq);

	if (result)
		return result;

	result = lv1_connect_interrupt_event_receive_port(dev->bus_id,
		dev->dev_id, virq_to_hw(*virq), dev->interrupt_id);

	if (result) {
		FAIL("%s:%d: lv1_connect_interrupt_event_receive_port"
			" failed: %s\n", __func__, __LINE__,
			ps3_result(result));
		ps3_event_receive_port_destroy(*virq);
		*virq = NO_IRQ;
		return result;
	}

	DBG("%s:%d: interrupt_id %u, virq %u\n", __func__, __LINE__,
		dev->interrupt_id, *virq);

	return 0;
}
EXPORT_SYMBOL(ps3_sb_event_receive_port_setup);

int ps3_sb_event_receive_port_destroy(struct ps3_system_bus_device *dev,
	unsigned int virq)
{
	

	int result;

	DBG(" -> %s:%d: interrupt_id %u, virq %u\n", __func__, __LINE__,
		dev->interrupt_id, virq);

	result = lv1_disconnect_interrupt_event_receive_port(dev->bus_id,
		dev->dev_id, virq_to_hw(virq), dev->interrupt_id);

	if (result)
		FAIL("%s:%d: lv1_disconnect_interrupt_event_receive_port"
			" failed: %s\n", __func__, __LINE__,
			ps3_result(result));

	result = ps3_event_receive_port_destroy(virq);
	BUG_ON(result);


	result = ps3_virq_destroy(virq);
	BUG_ON(result);

	DBG(" <- %s:%d\n", __func__, __LINE__);
	return result;
}
EXPORT_SYMBOL(ps3_sb_event_receive_port_destroy);


int ps3_io_irq_setup(enum ps3_cpu_binding cpu, unsigned int interrupt_id,
	unsigned int *virq)
{
	int result;
	u64 outlet;

	result = lv1_construct_io_irq_outlet(interrupt_id, &outlet);

	if (result) {
		FAIL("%s:%d: lv1_construct_io_irq_outlet failed: %s\n",
			__func__, __LINE__, ps3_result(result));
		return result;
	}

	result = ps3_irq_plug_setup(cpu, outlet, virq);
	BUG_ON(result);

	return result;
}
EXPORT_SYMBOL_GPL(ps3_io_irq_setup);

int ps3_io_irq_destroy(unsigned int virq)
{
	int result;
	unsigned long outlet = virq_to_hw(virq);

	ps3_chip_mask(irq_get_irq_data(virq));


	result = ps3_irq_plug_destroy(virq);
	BUG_ON(result);

	result = lv1_destruct_io_irq_outlet(outlet);

	if (result)
		FAIL("%s:%d: lv1_destruct_io_irq_outlet failed: %s\n",
			__func__, __LINE__, ps3_result(result));

	return result;
}
EXPORT_SYMBOL_GPL(ps3_io_irq_destroy);


int ps3_vuart_irq_setup(enum ps3_cpu_binding cpu, void* virt_addr_bmp,
	unsigned int *virq)
{
	int result;
	u64 outlet;
	u64 lpar_addr;

	BUG_ON(!is_kernel_addr((u64)virt_addr_bmp));

	lpar_addr = ps3_mm_phys_to_lpar(__pa(virt_addr_bmp));

	result = lv1_configure_virtual_uart_irq(lpar_addr, &outlet);

	if (result) {
		FAIL("%s:%d: lv1_configure_virtual_uart_irq failed: %s\n",
			__func__, __LINE__, ps3_result(result));
		return result;
	}

	result = ps3_irq_plug_setup(cpu, outlet, virq);
	BUG_ON(result);

	return result;
}
EXPORT_SYMBOL_GPL(ps3_vuart_irq_setup);

int ps3_vuart_irq_destroy(unsigned int virq)
{
	int result;

	ps3_chip_mask(irq_get_irq_data(virq));
	result = lv1_deconfigure_virtual_uart_irq();

	if (result) {
		FAIL("%s:%d: lv1_configure_virtual_uart_irq failed: %s\n",
			__func__, __LINE__, ps3_result(result));
		return result;
	}

	result = ps3_irq_plug_destroy(virq);
	BUG_ON(result);

	return result;
}
EXPORT_SYMBOL_GPL(ps3_vuart_irq_destroy);


int ps3_spe_irq_setup(enum ps3_cpu_binding cpu, unsigned long spe_id,
	unsigned int class, unsigned int *virq)
{
	int result;
	u64 outlet;

	BUG_ON(class > 2);

	result = lv1_get_spe_irq_outlet(spe_id, class, &outlet);

	if (result) {
		FAIL("%s:%d: lv1_get_spe_irq_outlet failed: %s\n",
			__func__, __LINE__, ps3_result(result));
		return result;
	}

	result = ps3_irq_plug_setup(cpu, outlet, virq);
	BUG_ON(result);

	return result;
}

int ps3_spe_irq_destroy(unsigned int virq)
{
	int result;

	ps3_chip_mask(irq_get_irq_data(virq));

	result = ps3_irq_plug_destroy(virq);
	BUG_ON(result);

	return result;
}


#define PS3_INVALID_OUTLET ((irq_hw_number_t)-1)
#define PS3_PLUG_MAX 63

#if defined(DEBUG)
static void _dump_64_bmp(const char *header, const u64 *p, unsigned cpu,
	const char* func, int line)
{
	pr_debug("%s:%d: %s %u {%04llx_%04llx_%04llx_%04llx}\n",
		func, line, header, cpu,
		*p >> 48, (*p >> 32) & 0xffff, (*p >> 16) & 0xffff,
		*p & 0xffff);
}

static void __maybe_unused _dump_256_bmp(const char *header,
	const u64 *p, unsigned cpu, const char* func, int line)
{
	pr_debug("%s:%d: %s %u {%016llx:%016llx:%016llx:%016llx}\n",
		func, line, header, cpu, p[0], p[1], p[2], p[3]);
}

#define dump_bmp(_x) _dump_bmp(_x, __func__, __LINE__)
static void _dump_bmp(struct ps3_private* pd, const char* func, int line)
{
	unsigned long flags;

	spin_lock_irqsave(&pd->bmp_lock, flags);
	_dump_64_bmp("stat", &pd->bmp.status, pd->thread_id, func, line);
	_dump_64_bmp("mask", (u64*)&pd->bmp.mask, pd->thread_id, func, line);
	spin_unlock_irqrestore(&pd->bmp_lock, flags);
}

#define dump_mask(_x) _dump_mask(_x, __func__, __LINE__)
static void __maybe_unused _dump_mask(struct ps3_private *pd,
	const char* func, int line)
{
	unsigned long flags;

	spin_lock_irqsave(&pd->bmp_lock, flags);
	_dump_64_bmp("mask", (u64*)&pd->bmp.mask, pd->thread_id, func, line);
	spin_unlock_irqrestore(&pd->bmp_lock, flags);
}
#else
static void dump_bmp(struct ps3_private* pd) {};
#endif 

static int ps3_host_map(struct irq_domain *h, unsigned int virq,
	irq_hw_number_t hwirq)
{
	DBG("%s:%d: hwirq %lu, virq %u\n", __func__, __LINE__, hwirq,
		virq);

	irq_set_chip_and_handler(virq, &ps3_irq_chip, handle_fasteoi_irq);

	return 0;
}

static int ps3_host_match(struct irq_domain *h, struct device_node *np)
{
	
	return 1;
}

static const struct irq_domain_ops ps3_host_ops = {
	.map = ps3_host_map,
	.match = ps3_host_match,
};

void __init ps3_register_ipi_debug_brk(unsigned int cpu, unsigned int virq)
{
	struct ps3_private *pd = &per_cpu(ps3_private, cpu);

	set_bit(63 - virq, &pd->ipi_debug_brk_mask);

	DBG("%s:%d: cpu %u, virq %u, mask %lxh\n", __func__, __LINE__,
		cpu, virq, pd->ipi_debug_brk_mask);
}

void __init ps3_register_ipi_irq(unsigned int cpu, unsigned int virq)
{
	struct ps3_private *pd = &per_cpu(ps3_private, cpu);

	set_bit(63 - virq, &pd->ipi_mask);

	DBG("%s:%d: cpu %u, virq %u, ipi_mask %lxh\n", __func__, __LINE__,
		cpu, virq, pd->ipi_mask);
}

static unsigned int ps3_get_irq(void)
{
	struct ps3_private *pd = &__get_cpu_var(ps3_private);
	u64 x = (pd->bmp.status & pd->bmp.mask);
	unsigned int plug;

	

	if (x & pd->ipi_debug_brk_mask)
		x &= pd->ipi_debug_brk_mask;

	asm volatile("cntlzd %0,%1" : "=r" (plug) : "r" (x));
	plug &= 0x3f;

	if (unlikely(plug == NO_IRQ)) {
		DBG("%s:%d: no plug found: thread_id %llu\n", __func__,
			__LINE__, pd->thread_id);
		dump_bmp(&per_cpu(ps3_private, 0));
		dump_bmp(&per_cpu(ps3_private, 1));
		return NO_IRQ;
	}

#if defined(DEBUG)
	if (unlikely(plug < NUM_ISA_INTERRUPTS || plug > PS3_PLUG_MAX)) {
		dump_bmp(&per_cpu(ps3_private, 0));
		dump_bmp(&per_cpu(ps3_private, 1));
		BUG();
	}
#endif

	

	if (test_bit(63 - plug, &pd->ipi_mask))
		lv1_end_of_interrupt_ext(pd->ppe_id, pd->thread_id, plug);

	return plug;
}

void __init ps3_init_IRQ(void)
{
	int result;
	unsigned cpu;
	struct irq_domain *host;

	host = irq_domain_add_nomap(NULL, PS3_PLUG_MAX + 1, &ps3_host_ops, NULL);
	irq_set_default_host(host);

	for_each_possible_cpu(cpu) {
		struct ps3_private *pd = &per_cpu(ps3_private, cpu);

		lv1_get_logical_ppe_id(&pd->ppe_id);
		pd->thread_id = get_hard_smp_processor_id(cpu);
		spin_lock_init(&pd->bmp_lock);

		DBG("%s:%d: ppe_id %llu, thread_id %llu, bmp %lxh\n",
			__func__, __LINE__, pd->ppe_id, pd->thread_id,
			ps3_mm_phys_to_lpar(__pa(&pd->bmp)));

		result = lv1_configure_irq_state_bitmap(pd->ppe_id,
			pd->thread_id, ps3_mm_phys_to_lpar(__pa(&pd->bmp)));

		if (result)
			FAIL("%s:%d: lv1_configure_irq_state_bitmap failed:"
				" %s\n", __func__, __LINE__,
				ps3_result(result));
	}

	ppc_md.get_irq = ps3_get_irq;
}

void ps3_shutdown_IRQ(int cpu)
{
	int result;
	u64 ppe_id;
	u64 thread_id = get_hard_smp_processor_id(cpu);

	lv1_get_logical_ppe_id(&ppe_id);
	result = lv1_configure_irq_state_bitmap(ppe_id, thread_id, 0);

	DBG("%s:%d: lv1_configure_irq_state_bitmap (%llu:%llu/%d) %s\n", __func__,
		__LINE__, ppe_id, thread_id, cpu, ps3_result(result));
}
