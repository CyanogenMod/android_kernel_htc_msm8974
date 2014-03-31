/*
 *	linux/arch/alpha/kernel/sys_mikasa.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *
 * Code supporting the MIKASA (AlphaServer 1000).
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/bitops.h>

#include <asm/ptrace.h>
#include <asm/mce.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/mmu_context.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/core_apecs.h>
#include <asm/core_cia.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"


static int cached_irq_mask;

static inline void
mikasa_update_irq_hw(int mask)
{
	outw(mask, 0x536);
}

static inline void
mikasa_enable_irq(struct irq_data *d)
{
	mikasa_update_irq_hw(cached_irq_mask |= 1 << (d->irq - 16));
}

static void
mikasa_disable_irq(struct irq_data *d)
{
	mikasa_update_irq_hw(cached_irq_mask &= ~(1 << (d->irq - 16)));
}

static struct irq_chip mikasa_irq_type = {
	.name		= "MIKASA",
	.irq_unmask	= mikasa_enable_irq,
	.irq_mask	= mikasa_disable_irq,
	.irq_mask_ack	= mikasa_disable_irq,
};

static void 
mikasa_device_interrupt(unsigned long vector)
{
	unsigned long pld;
	unsigned int i;

	
	pld = (((~inw(0x534) & 0x0000ffffUL) << 16)
	       | (((unsigned long) inb(0xa0)) << 8)
	       | inb(0x20));

	while (pld) {
		i = ffz(~pld);
		pld &= pld - 1; 
		if (i < 16) {
			isa_device_interrupt(vector);
		} else {
			handle_irq(i);
		}
	}
}

static void __init
mikasa_init_irq(void)
{
	long i;

	if (alpha_using_srm)
		alpha_mv.device_interrupt = srm_device_interrupt;

	mikasa_update_irq_hw(0);

	for (i = 16; i < 32; ++i) {
		irq_set_chip_and_handler(i, &mikasa_irq_type,
					 handle_level_irq);
		irq_set_status_flags(i, IRQ_LEVEL);
	}

	init_i8259a_irqs();
	common_init_isa_dma();
}



static int __init
mikasa_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[8][5] __initdata = {
		
		{16+12, 16+12, 16+12, 16+12, 16+12},	
		{   -1,    -1,    -1,    -1,    -1},	
		{   -1,    -1,    -1,    -1,    -1},	
		{   -1,    -1,    -1,    -1,    -1},	
		{   -1,    -1,    -1,    -1,    -1},	
		{ 16+0,  16+0,  16+1,  16+2,  16+3},	
		{ 16+4,  16+4,  16+5,  16+6,  16+7},	
		{ 16+8,  16+8,  16+9, 16+10, 16+11},	
	};
	const long min_idsel = 6, max_idsel = 13, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}


#if defined(CONFIG_ALPHA_GENERIC) || !defined(CONFIG_ALPHA_PRIMO)
static void
mikasa_apecs_machine_check(unsigned long vector, unsigned long la_ptr)
{
#define MCHK_NO_DEVSEL 0x205U
#define MCHK_NO_TABT 0x204U

	struct el_common *mchk_header;
	unsigned int code;

	mchk_header = (struct el_common *)la_ptr;

	
	mb();
	mb(); 
	draina();
	apecs_pci_clr_err();
	wrmces(0x7);
	mb();

	code = mchk_header->code;
	process_mcheck_info(vector, la_ptr, "MIKASA APECS",
			    (mcheck_expected(0)
			     && (code == MCHK_NO_DEVSEL
			         || code == MCHK_NO_TABT)));
}
#endif



#if defined(CONFIG_ALPHA_GENERIC) || !defined(CONFIG_ALPHA_PRIMO)
struct alpha_machine_vector mikasa_mv __initmv = {
	.vector_name		= "Mikasa",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_APECS_IO,
	.machine_check		= mikasa_apecs_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 32,
	.device_interrupt	= mikasa_device_interrupt,

	.init_arch		= apecs_init_arch,
	.init_irq		= mikasa_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= common_init_pci,
	.pci_map_irq		= mikasa_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(mikasa)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_PRIMO)
struct alpha_machine_vector mikasa_primo_mv __initmv = {
	.vector_name		= "Mikasa-Primo",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_CIA_IO,
	.machine_check		= cia_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= CIA_DEFAULT_MEM_BASE,

	.nr_irqs		= 32,
	.device_interrupt	= mikasa_device_interrupt,

	.init_arch		= cia_init_arch,
	.init_irq		= mikasa_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= cia_init_pci,
	.kill_arch		= cia_kill_arch,
	.pci_map_irq		= mikasa_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(mikasa_primo)
#endif
