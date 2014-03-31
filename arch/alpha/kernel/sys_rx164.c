/*
 *	linux/arch/alpha/kernel/sys_rx164.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *
 * Code supporting the RX164 (PCA56+POLARIS).
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
#include <asm/core_polaris.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"


static unsigned long cached_irq_mask;

static inline void
rx164_update_irq_hw(unsigned long mask)
{
	volatile unsigned int *irq_mask;

	irq_mask = (void *)(POLARIS_DENSE_CONFIG_BASE + 0x74);
	*irq_mask = mask;
	mb();
	*irq_mask;
}

static inline void
rx164_enable_irq(struct irq_data *d)
{
	rx164_update_irq_hw(cached_irq_mask |= 1UL << (d->irq - 16));
}

static void
rx164_disable_irq(struct irq_data *d)
{
	rx164_update_irq_hw(cached_irq_mask &= ~(1UL << (d->irq - 16)));
}

static struct irq_chip rx164_irq_type = {
	.name		= "RX164",
	.irq_unmask	= rx164_enable_irq,
	.irq_mask	= rx164_disable_irq,
	.irq_mask_ack	= rx164_disable_irq,
};

static void 
rx164_device_interrupt(unsigned long vector)
{
	unsigned long pld;
	volatile unsigned int *dirr;
	long i;

	dirr = (void *)(POLARIS_DENSE_CONFIG_BASE + 0x84);
	pld = *dirr;

	while (pld) {
		i = ffz(~pld);
		pld &= pld - 1; 
		if (i == 20) {
			isa_no_iack_sc_device_interrupt(vector);
		} else {
			handle_irq(16+i);
		}
	}
}

static void __init
rx164_init_irq(void)
{
	long i;

	rx164_update_irq_hw(0);
	for (i = 16; i < 40; ++i) {
		irq_set_chip_and_handler(i, &rx164_irq_type, handle_level_irq);
		irq_set_status_flags(i, IRQ_LEVEL);
	}

	init_i8259a_irqs();
	common_init_isa_dma();

	setup_irq(16+20, &isa_cascade_irqaction);
}




static int __init
rx164_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
#if 0
	static char irq_tab_pass1[6][5] __initdata = {
	  
	  { 16+3, 16+3, 16+8, 16+13, 16+18},      
	  { 16+5, 16+5, 16+10, 16+15, 16+20},     
	  { 16+4, 16+4, 16+9, 16+14, 16+19},      
	  { -1,     -1,    -1,    -1,   -1},      
	  { 16+2, 16+2, 16+7, 16+12, 16+17},      
	  { 16+1, 16+1, 16+6, 16+11, 16+16},      
	};
#else
	static char irq_tab[6][5] __initdata = {
	  
	  { 16+0, 16+0, 16+6, 16+11, 16+16},      
	  { 16+1, 16+1, 16+7, 16+12, 16+17},      
	  { -1,     -1,    -1,    -1,   -1},      
	  { 16+2, 16+2, 16+8, 16+13, 16+18},      
	  { 16+3, 16+3, 16+9, 16+14, 16+19},      
	  { 16+4, 16+4, 16+10, 16+15, 16+5},      
	};
#endif
	const long min_idsel = 5, max_idsel = 10, irqs_per_slot = 5;

	return COMMON_TABLE_LOOKUP;
}



struct alpha_machine_vector rx164_mv __initmv = {
	.vector_name		= "RX164",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_POLARIS_IO,
	.machine_check		= polaris_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= DEFAULT_MEM_BASE,

	.nr_irqs		= 40,
	.device_interrupt	= rx164_device_interrupt,

	.init_arch		= polaris_init_arch,
	.init_irq		= rx164_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= common_init_pci,
	.kill_arch		= NULL,
	.pci_map_irq		= rx164_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(rx164)
