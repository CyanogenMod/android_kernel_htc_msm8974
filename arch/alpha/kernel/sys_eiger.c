/*
 *	linux/arch/alpha/kernel/sys_eiger.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996, 1999 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *	Copyright (C) 1999 Iain Grant
 *
 * Code supporting the EIGER (EV6+TSUNAMI).
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/bitops.h>

#include <asm/ptrace.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/mmu_context.h>
#include <asm/io.h>
#include <asm/pci.h>
#include <asm/pgtable.h>
#include <asm/core_tsunami.h>
#include <asm/hwrpb.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"



static unsigned long cached_irq_mask[2] = { -1, -1 };

static inline void
eiger_update_irq_hw(unsigned long irq, unsigned long mask)
{
	int regaddr;

	mask = (irq >= 64 ? mask << 16 : mask >> ((irq - 16) & 0x30));
	regaddr = 0x510 + (((irq - 16) >> 2) & 0x0c);
	outl(mask & 0xffff0000UL, regaddr);
}

static inline void
eiger_enable_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;
	unsigned long mask;
	mask = (cached_irq_mask[irq >= 64] &= ~(1UL << (irq & 63)));
	eiger_update_irq_hw(irq, mask);
}

static void
eiger_disable_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;
	unsigned long mask;
	mask = (cached_irq_mask[irq >= 64] |= 1UL << (irq & 63));
	eiger_update_irq_hw(irq, mask);
}

static struct irq_chip eiger_irq_type = {
	.name		= "EIGER",
	.irq_unmask	= eiger_enable_irq,
	.irq_mask	= eiger_disable_irq,
	.irq_mask_ack	= eiger_disable_irq,
};

static void
eiger_device_interrupt(unsigned long vector)
{
	unsigned intstatus;


	intstatus = inw(0x500) & 15;
	if (intstatus) {

		if (intstatus & 8) handle_irq(16+3);
		if (intstatus & 4) handle_irq(16+2);
		if (intstatus & 2) handle_irq(16+1);
		if (intstatus & 1) handle_irq(16+0);
	} else {
		isa_device_interrupt(vector);
	}
}

static void
eiger_srm_device_interrupt(unsigned long vector)
{
	int irq = (vector - 0x800) >> 4;
	handle_irq(irq);
}

static void __init
eiger_init_irq(void)
{
	long i;

	outb(0, DMA1_RESET_REG);
	outb(0, DMA2_RESET_REG);
	outb(DMA_MODE_CASCADE, DMA2_MODE_REG);
	outb(0, DMA2_MASK_REG);

	if (alpha_using_srm)
		alpha_mv.device_interrupt = eiger_srm_device_interrupt;

	for (i = 16; i < 128; i += 16)
		eiger_update_irq_hw(i, -1);

	init_i8259a_irqs();

	for (i = 16; i < 128; ++i) {
		irq_set_chip_and_handler(i, &eiger_irq_type, handle_level_irq);
		irq_set_status_flags(i, IRQ_LEVEL);
	}
}

static int __init
eiger_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	u8 irq_orig;


	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq_orig);

	return irq_orig - 0x80;
}

static u8 __init
eiger_swizzle(struct pci_dev *dev, u8 *pinp)
{
	struct pci_controller *hose = dev->sysdata;
	int slot, pin = *pinp;
	int bridge_count = 0;

	
	int backplane = inw(0x502) & 0x0f;

	switch (backplane)
	{
	   case 0x00: bridge_count = 0; break; 
	   case 0x01: bridge_count = 1; break; 
	   case 0x03: bridge_count = 2; break; 
	   case 0x07: bridge_count = 3; break; 
	   case 0x0f: bridge_count = 4; break; 
	};

	slot = PCI_SLOT(dev->devfn);
	while (dev->bus->self) {
		
		if (hose->index == 0
		    && (PCI_SLOT(dev->bus->self->devfn)
			> 20 - bridge_count)) {
			slot = PCI_SLOT(dev->devfn);
			break;
		}
		
		pin = pci_swizzle_interrupt_pin(dev, pin);

		
		dev = dev->bus->self;
	}
	*pinp = pin;
	return slot;
}


struct alpha_machine_vector eiger_mv __initmv = {
	.vector_name		= "Eiger",
	DO_EV6_MMU,
	DO_DEFAULT_RTC,
	DO_TSUNAMI_IO,
	.machine_check		= tsunami_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= DEFAULT_MEM_BASE,
	.pci_dac_offset		= TSUNAMI_DAC_OFFSET,

	.nr_irqs		= 128,
	.device_interrupt	= eiger_device_interrupt,

	.init_arch		= tsunami_init_arch,
	.init_irq		= eiger_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= common_init_pci,
	.kill_arch		= tsunami_kill_arch,
	.pci_map_irq		= eiger_map_irq,
	.pci_swizzle		= eiger_swizzle,
};
ALIAS_MV(eiger)
