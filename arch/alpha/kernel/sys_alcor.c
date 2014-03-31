/*
 *	linux/arch/alpha/kernel/sys_alcor.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *
 * Code supporting the ALCOR and XLT (XL-300/366/433).
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/bitops.h>

#include <asm/ptrace.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/mmu_context.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/core_cia.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"


static unsigned long cached_irq_mask;

static inline void
alcor_update_irq_hw(unsigned long mask)
{
	*(vuip)GRU_INT_MASK = mask;
	mb();
}

static inline void
alcor_enable_irq(struct irq_data *d)
{
	alcor_update_irq_hw(cached_irq_mask |= 1UL << (d->irq - 16));
}

static void
alcor_disable_irq(struct irq_data *d)
{
	alcor_update_irq_hw(cached_irq_mask &= ~(1UL << (d->irq - 16)));
}

static void
alcor_mask_and_ack_irq(struct irq_data *d)
{
	alcor_disable_irq(d);

	
	*(vuip)GRU_INT_CLEAR = 1 << (d->irq - 16); mb();
	*(vuip)GRU_INT_CLEAR = 0; mb();
}

static void
alcor_isa_mask_and_ack_irq(struct irq_data *d)
{
	i8259a_mask_and_ack_irq(d);

	
	*(vuip)GRU_INT_CLEAR = 0x80000000; mb();
	*(vuip)GRU_INT_CLEAR = 0; mb();
}

static struct irq_chip alcor_irq_type = {
	.name		= "ALCOR",
	.irq_unmask	= alcor_enable_irq,
	.irq_mask	= alcor_disable_irq,
	.irq_mask_ack	= alcor_mask_and_ack_irq,
};

static void
alcor_device_interrupt(unsigned long vector)
{
	unsigned long pld;
	unsigned int i;

	
	pld = (*(vuip)GRU_INT_REQ) & GRU_INT_REQ_BITS;

	while (pld) {
		i = ffz(~pld);
		pld &= pld - 1; 
		if (i == 31) {
			isa_device_interrupt(vector);
		} else {
			handle_irq(16 + i);
		}
	}
}

static void __init
alcor_init_irq(void)
{
	long i;

	if (alpha_using_srm)
		alpha_mv.device_interrupt = srm_device_interrupt;

	*(vuip)GRU_INT_MASK  = 0; mb();			
	*(vuip)GRU_INT_EDGE  = 0; mb();			
	*(vuip)GRU_INT_HILO  = 0x80000000U; mb();	
	*(vuip)GRU_INT_CLEAR = 0; mb();			

	for (i = 16; i < 48; ++i) {
		if (i >= 16+20 && i <= 16+30)
			continue;
		irq_set_chip_and_handler(i, &alcor_irq_type, handle_level_irq);
		irq_set_status_flags(i, IRQ_LEVEL);
	}
	i8259a_irq_type.irq_ack = alcor_isa_mask_and_ack_irq;

	init_i8259a_irqs();
	common_init_isa_dma();

	setup_irq(16+31, &isa_cascade_irqaction);
}



static int __init
alcor_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[7][5] __initdata = {
		
		
		{16+13, 16+13, 16+13, 16+13, 16+13},	
		{ 16+8,  16+8,  16+9, 16+10, 16+11},	
		{16+16, 16+16, 16+17, 16+18, 16+19},	
		{16+12, 16+12, 16+13, 16+14, 16+15},	
		{   -1,    -1,    -1,    -1,    -1},	
		{ 16+0,  16+0,  16+1,  16+2,  16+3},	
		{ 16+4,  16+4,  16+5,  16+6,  16+7},	
	};
	const long min_idsel = 6, max_idsel = 12, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}

static void
alcor_kill_arch(int mode)
{
	cia_kill_arch(mode);

#ifndef ALPHA_RESTORE_SRM_SETUP
	switch(mode) {
	case LINUX_REBOOT_CMD_RESTART:
		
		if (alpha_using_srm) {
			*(vuip) GRU_RESET = 0x0000dead;
			mb();
		}
		break;
	case LINUX_REBOOT_CMD_HALT:
		break;
	case LINUX_REBOOT_CMD_POWER_OFF:
		break;
	}

	halt();
#endif
}

static void __init
alcor_init_pci(void)
{
	struct pci_dev *dev;

	cia_init_pci();

	dev = pci_get_device(PCI_VENDOR_ID_DEC,
			      PCI_DEVICE_ID_DEC_TULIP,
			      NULL);
	if (dev && dev->devfn == PCI_DEVFN(6,0)) {
		alpha_mv.sys.cia.gru_int_req_bits = XLT_GRU_INT_REQ_BITS; 
		printk(KERN_INFO "%s: Detected AS500 or XLT motherboard.\n",
		       __func__);
	}
	pci_dev_put(dev);
}



struct alpha_machine_vector alcor_mv __initmv = {
	.vector_name		= "Alcor",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_CIA_IO,
	.machine_check		= cia_machine_check,
	.max_isa_dma_address	= ALPHA_ALCOR_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= EISA_DEFAULT_IO_BASE,
	.min_mem_address	= CIA_DEFAULT_MEM_BASE,

	.nr_irqs		= 48,
	.device_interrupt	= alcor_device_interrupt,

	.init_arch		= cia_init_arch,
	.init_irq		= alcor_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= alcor_init_pci,
	.kill_arch		= alcor_kill_arch,
	.pci_map_irq		= alcor_map_irq,
	.pci_swizzle		= common_swizzle,

	.sys = { .cia = {
		.gru_int_req_bits = ALCOR_GRU_INT_REQ_BITS
	}}
};
ALIAS_MV(alcor)

struct alpha_machine_vector xlt_mv __initmv = {
	.vector_name		= "XLT",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_CIA_IO,
	.machine_check		= cia_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= EISA_DEFAULT_IO_BASE,
	.min_mem_address	= CIA_DEFAULT_MEM_BASE,

	.nr_irqs		= 48,
	.device_interrupt	= alcor_device_interrupt,

	.init_arch		= cia_init_arch,
	.init_irq		= alcor_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= alcor_init_pci,
	.kill_arch		= alcor_kill_arch,
	.pci_map_irq		= alcor_map_irq,
	.pci_swizzle		= common_swizzle,

	.sys = { .cia = {
		.gru_int_req_bits = XLT_GRU_INT_REQ_BITS
	}}
};

