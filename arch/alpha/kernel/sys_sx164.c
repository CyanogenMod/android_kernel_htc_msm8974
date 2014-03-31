/*
 *	linux/arch/alpha/kernel/sys_sx164.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999, 2000 Richard Henderson
 *
 * Code supporting the SX164 (PCA56+PYXIS).
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
#include <asm/core_cia.h>
#include <asm/hwrpb.h>
#include <asm/tlbflush.h>
#include <asm/special_insns.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"


static void __init
sx164_init_irq(void)
{
	outb(0, DMA1_RESET_REG);
	outb(0, DMA2_RESET_REG);
	outb(DMA_MODE_CASCADE, DMA2_MODE_REG);
	outb(0, DMA2_MASK_REG);

	if (alpha_using_srm)
		alpha_mv.device_interrupt = srm_device_interrupt;

	init_i8259a_irqs();

	if (alpha_using_srm)
		init_srm_irqs(40, 0x3f0000);
	else
		init_pyxis_irqs(0xff00003f0000UL);

	setup_irq(16+6, &timer_cascade_irqaction);
}


static int __init
sx164_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[5][5] __initdata = {
		
		{ 16+ 9, 16+ 9, 16+13, 16+17, 16+21}, 
		{ 16+11, 16+11, 16+15, 16+19, 16+23}, 
		{ 16+10, 16+10, 16+14, 16+18, 16+22}, 
		{    -1,    -1,    -1,	  -1,    -1}, 
		{ 16+ 8, 16+ 8, 16+12, 16+16, 16+20}  
	};
	const long min_idsel = 5, max_idsel = 9, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}

static void __init
sx164_init_pci(void)
{
	cia_init_pci();
	SMC669_Init(0);
}

static void __init
sx164_init_arch(void)
{
	struct percpu_struct *cpu = (struct percpu_struct*)
		((char*)hwrpb + hwrpb->processor_offset);

	if (amask(AMASK_MAX) != 0
	    && alpha_using_srm
	    && (cpu->pal_revision & 0xffff) <= 0x117) {
		__asm__ __volatile__(
		"lda	$16,8($31)\n"
		"call_pal 9\n"		
		".long  0x64000118\n\n"	
		"ldah	$16,(1<<(19-16))($31)\n"
		"or	$0,$16,$0\n"	
		".long  0x74000118\n"	
		"lda	$16,9($31)\n"
		"call_pal 9"		
		: : : "$0", "$16");
		printk("PCA56 MVI set enabled\n");
	}

	pyxis_init_arch();
}


struct alpha_machine_vector sx164_mv __initmv = {
	.vector_name		= "SX164",
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

	.init_arch		= sx164_init_arch,
	.init_irq		= sx164_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= sx164_init_pci,
	.kill_arch		= cia_kill_arch,
	.pci_map_irq		= sx164_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(sx164)
