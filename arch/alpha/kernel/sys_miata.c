/*
 *	linux/arch/alpha/kernel/sys_miata.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999, 2000 Richard Henderson
 *
 * Code supporting the MIATA (EV56+PYXIS).
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/reboot.h>

#include <asm/ptrace.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/mmu_context.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/core_cia.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"


static void 
miata_srm_device_interrupt(unsigned long vector)
{
	int irq;

	irq = (vector - 0x800) >> 4;

	if (irq >= 16)
		irq = irq + 8;

	handle_irq(irq);
}

static void __init
miata_init_irq(void)
{
	if (alpha_using_srm)
		alpha_mv.device_interrupt = miata_srm_device_interrupt;

#if 0
	
	*(vulp)PYXIS_INT_HILO = 0x000000B2UL; mb();	
	*(vulp)PYXIS_RT_COUNT = 0UL; mb();		
#endif

	init_i8259a_irqs();

	init_pyxis_irqs(0x63b0000);

	common_init_isa_dma();
	setup_irq(16+2, &halt_switch_irqaction);	
	setup_irq(16+6, &timer_cascade_irqaction);
}



static int __init
miata_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
        static char irq_tab[18][5] __initdata = {
		
		{16+ 8, 16+ 8, 16+ 8, 16+ 8, 16+ 8},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{16+12, 16+12, 16+13, 16+14, 16+15},  
		{16+16, 16+16, 16+17, 16+18, 16+19},  
		
		{16+11, 16+11, 16+11, 16+11, 16+11},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{16+20, 16+20, 16+21, 16+22, 16+23},  
		{16+24, 16+24, 16+25, 16+26, 16+27},  
		{16+28, 16+28, 16+29, 16+30, 16+31},  
		
		{   -1,    -1,    -1,    -1,    -1},  
        };
	const long min_idsel = 3, max_idsel = 20, irqs_per_slot = 5;
	

	if((slot == 7) && (PCI_FUNC(dev->devfn) == 3)) {
		u8 irq=0;
		struct pci_dev *pdev = pci_get_slot(dev->bus, dev->devfn & ~7);
		if(pdev == NULL || pci_read_config_byte(pdev, 0x40,&irq) != PCIBIOS_SUCCESSFUL) {
			pci_dev_put(pdev);
			return -1;
		}
		else	{
			pci_dev_put(pdev);
			return irq;
		}
	}

	return COMMON_TABLE_LOOKUP;
}

static u8 __init
miata_swizzle(struct pci_dev *dev, u8 *pinp)
{
	int slot, pin = *pinp;

	if (dev->bus->number == 0) {
		slot = PCI_SLOT(dev->devfn);
	}		
	
	else if ((PCI_SLOT(dev->bus->self->devfn) == 8) ||
		 (PCI_SLOT(dev->bus->self->devfn) == 20)) {
		slot = PCI_SLOT(dev->devfn) + 9;
	}
	else 
	{
		
		do {
			if ((PCI_SLOT(dev->bus->self->devfn) == 8) ||
			    (PCI_SLOT(dev->bus->self->devfn) == 20)) {
				slot = PCI_SLOT(dev->devfn) + 9;
				break;
			}
			pin = pci_swizzle_interrupt_pin(dev, pin);

			
			dev = dev->bus->self;
			
			slot = PCI_SLOT(dev->devfn);
		} while (dev->bus->self);
	}
	*pinp = pin;
	return slot;
}

static void __init
miata_init_pci(void)
{
	cia_init_pci();
	SMC669_Init(0); 
	es1888_init();
}

static void
miata_kill_arch(int mode)
{
	cia_kill_arch(mode);

#ifndef ALPHA_RESTORE_SRM_SETUP
	switch(mode) {
	case LINUX_REBOOT_CMD_RESTART:
		 
		if (alpha_using_srm) {
			*(vuip) PYXIS_RESET = 0x0000dead; 
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



struct alpha_machine_vector miata_mv __initmv = {
	.vector_name		= "Miata",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_PYXIS_IO,
	.machine_check		= cia_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= DEFAULT_MEM_BASE,
	.pci_dac_offset		= PYXIS_DAC_OFFSET,

	.nr_irqs		= 48,
	.device_interrupt	= pyxis_device_interrupt,

	.init_arch		= pyxis_init_arch,
	.init_irq		= miata_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= miata_init_pci,
	.kill_arch		= miata_kill_arch,
	.pci_map_irq		= miata_map_irq,
	.pci_swizzle		= miata_swizzle,
};
ALIAS_MV(miata)
