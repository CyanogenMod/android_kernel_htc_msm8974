/*
 *	linux/arch/alpha/kernel/sys_cabriolet.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999, 2000 Richard Henderson
 *
 * Code supporting the Cabriolet (AlphaPC64), EB66+, and EB164,
 * PC164 and LX164.
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
#include <asm/pgtable.h>
#include <asm/core_apecs.h>
#include <asm/core_cia.h>
#include <asm/core_lca.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"
#include "pc873xx.h"

static unsigned long cached_irq_mask = ~0UL;

static inline void
cabriolet_update_irq_hw(unsigned int irq, unsigned long mask)
{
	int ofs = (irq - 16) / 8;
	outb(mask >> (16 + ofs * 8), 0x804 + ofs);
}

static inline void
cabriolet_enable_irq(struct irq_data *d)
{
	cabriolet_update_irq_hw(d->irq, cached_irq_mask &= ~(1UL << d->irq));
}

static void
cabriolet_disable_irq(struct irq_data *d)
{
	cabriolet_update_irq_hw(d->irq, cached_irq_mask |= 1UL << d->irq);
}

static struct irq_chip cabriolet_irq_type = {
	.name		= "CABRIOLET",
	.irq_unmask	= cabriolet_enable_irq,
	.irq_mask	= cabriolet_disable_irq,
	.irq_mask_ack	= cabriolet_disable_irq,
};

static void 
cabriolet_device_interrupt(unsigned long v)
{
	unsigned long pld;
	unsigned int i;

	
	pld = inb(0x804) | (inb(0x805) << 8) | (inb(0x806) << 16);

	while (pld) {
		i = ffz(~pld);
		pld &= pld - 1;	
		if (i == 4) {
			isa_device_interrupt(v);
		} else {
			handle_irq(16 + i);
		}
	}
}

static void __init
common_init_irq(void (*srm_dev_int)(unsigned long v))
{
	init_i8259a_irqs();

	if (alpha_using_srm) {
		alpha_mv.device_interrupt = srm_dev_int;
		init_srm_irqs(35, 0);
	}
	else {
		long i;

		outb(0xff, 0x804);
		outb(0xff, 0x805);
		outb(0xff, 0x806);

		for (i = 16; i < 35; ++i) {
			irq_set_chip_and_handler(i, &cabriolet_irq_type,
						 handle_level_irq);
			irq_set_status_flags(i, IRQ_LEVEL);
		}
	}

	common_init_isa_dma();
	setup_irq(16+4, &isa_cascade_irqaction);
}

#ifndef CONFIG_ALPHA_PC164
static void __init
cabriolet_init_irq(void)
{
	common_init_irq(srm_device_interrupt);
}
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_PC164)

static void
pc164_srm_device_interrupt(unsigned long v)
{
	__min_ipl = getipl();
	srm_device_interrupt(v);
	__min_ipl = 0;
}

static void
pc164_device_interrupt(unsigned long v)
{
	__min_ipl = getipl();
	cabriolet_device_interrupt(v);
	__min_ipl = 0;
}

static void __init
pc164_init_irq(void)
{
	common_init_irq(pc164_srm_device_interrupt);
}
#endif


static inline int __init
eb66p_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[5][5] __initdata = {
		
		{16+0, 16+0, 16+5,  16+9, 16+13},  
		{16+1, 16+1, 16+6, 16+10, 16+14},  
		{  -1,   -1,   -1,    -1,    -1},  
		{16+2, 16+2, 16+7, 16+11, 16+15},  
		{16+3, 16+3, 16+8, 16+12,  16+6}   
	};
	const long min_idsel = 6, max_idsel = 10, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}



static inline int __init
cabriolet_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[5][5] __initdata = {
		
		{ 16+2, 16+2, 16+7, 16+11, 16+15}, 
		{ 16+0, 16+0, 16+5,  16+9, 16+13}, 
		{ 16+1, 16+1, 16+6, 16+10, 16+14}, 
		{   -1,   -1,   -1,    -1,    -1}, 
		{ 16+3, 16+3, 16+8, 16+12, 16+16}  
	};
	const long min_idsel = 5, max_idsel = 9, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}

static inline void __init
cabriolet_enable_ide(void)
{
	if (pc873xx_probe() == -1) {
		printk(KERN_ERR "Probing for PC873xx Super IO chip failed.\n");
	 } else {
		printk(KERN_INFO "Found %s Super IO chip at 0x%x\n",
			pc873xx_get_model(), pc873xx_get_base());

		pc873xx_enable_ide();
	}
}

static inline void __init
cabriolet_init_pci(void)
{
	common_init_pci();
	cabriolet_enable_ide();
}

static inline void __init
cia_cab_init_pci(void)
{
	cia_init_pci();
	cabriolet_enable_ide();
}


static inline int __init
alphapc164_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[7][5] __initdata = {
		
		{ 16+2, 16+2, 16+9,  16+13, 16+17}, 
		{ 16+0, 16+0, 16+7,  16+11, 16+15}, 
		{ 16+1, 16+1, 16+8,  16+12, 16+16}, 
		{   -1,   -1,   -1,    -1,    -1},  
		{ 16+3, 16+3, 16+10, 16+14, 16+18}, 
		{ 16+6, 16+6, 16+6,  16+6,  16+6},  
		{ 16+5, 16+5, 16+5,  16+5,  16+5}   
	};
	const long min_idsel = 5, max_idsel = 11, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}

static inline void __init
alphapc164_init_pci(void)
{
	cia_init_pci();
	SMC93x_Init();
}



#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_CABRIOLET)
struct alpha_machine_vector cabriolet_mv __initmv = {
	.vector_name		= "Cabriolet",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_APECS_IO,
	.machine_check		= apecs_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 35,
	.device_interrupt	= cabriolet_device_interrupt,

	.init_arch		= apecs_init_arch,
	.init_irq		= cabriolet_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= cabriolet_init_pci,
	.pci_map_irq		= cabriolet_map_irq,
	.pci_swizzle		= common_swizzle,
};
#ifndef CONFIG_ALPHA_EB64P
ALIAS_MV(cabriolet)
#endif
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_EB164)
struct alpha_machine_vector eb164_mv __initmv = {
	.vector_name		= "EB164",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_CIA_IO,
	.machine_check		= cia_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= CIA_DEFAULT_MEM_BASE,

	.nr_irqs		= 35,
	.device_interrupt	= cabriolet_device_interrupt,

	.init_arch		= cia_init_arch,
	.init_irq		= cabriolet_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= cia_cab_init_pci,
	.kill_arch		= cia_kill_arch,
	.pci_map_irq		= cabriolet_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(eb164)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_EB66P)
struct alpha_machine_vector eb66p_mv __initmv = {
	.vector_name		= "EB66+",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_LCA_IO,
	.machine_check		= lca_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 35,
	.device_interrupt	= cabriolet_device_interrupt,

	.init_arch		= lca_init_arch,
	.init_irq		= cabriolet_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= cabriolet_init_pci,
	.pci_map_irq		= eb66p_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(eb66p)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_LX164)
struct alpha_machine_vector lx164_mv __initmv = {
	.vector_name		= "LX164",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_PYXIS_IO,
	.machine_check		= cia_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= DEFAULT_MEM_BASE,
	.pci_dac_offset		= PYXIS_DAC_OFFSET,

	.nr_irqs		= 35,
	.device_interrupt	= cabriolet_device_interrupt,

	.init_arch		= pyxis_init_arch,
	.init_irq		= cabriolet_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= alphapc164_init_pci,
	.kill_arch		= cia_kill_arch,
	.pci_map_irq		= alphapc164_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(lx164)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_PC164)
struct alpha_machine_vector pc164_mv __initmv = {
	.vector_name		= "PC164",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_CIA_IO,
	.machine_check		= cia_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= CIA_DEFAULT_MEM_BASE,

	.nr_irqs		= 35,
	.device_interrupt	= pc164_device_interrupt,

	.init_arch		= cia_init_arch,
	.init_irq		= pc164_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= alphapc164_init_pci,
	.kill_arch		= cia_kill_arch,
	.pci_map_irq		= alphapc164_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(pc164)
#endif
