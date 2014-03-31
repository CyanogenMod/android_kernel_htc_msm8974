/*
 *	linux/arch/alpha/kernel/sys_eb64p.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *
 * Code supporting the EB64+ and EB66.
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
#include <asm/core_lca.h>
#include <asm/hwrpb.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"


static unsigned int cached_irq_mask = -1;

static inline void
eb64p_update_irq_hw(unsigned int irq, unsigned long mask)
{
	outb(mask >> (irq >= 24 ? 24 : 16), (irq >= 24 ? 0x27 : 0x26));
}

static inline void
eb64p_enable_irq(struct irq_data *d)
{
	eb64p_update_irq_hw(d->irq, cached_irq_mask &= ~(1 << d->irq));
}

static void
eb64p_disable_irq(struct irq_data *d)
{
	eb64p_update_irq_hw(d->irq, cached_irq_mask |= 1 << d->irq);
}

static struct irq_chip eb64p_irq_type = {
	.name		= "EB64P",
	.irq_unmask	= eb64p_enable_irq,
	.irq_mask	= eb64p_disable_irq,
	.irq_mask_ack	= eb64p_disable_irq,
};

static void 
eb64p_device_interrupt(unsigned long vector)
{
	unsigned long pld;
	unsigned int i;

	
	pld = inb(0x26) | (inb(0x27) << 8);

	while (pld) {
		i = ffz(~pld);
		pld &= pld - 1;	

		if (i == 5) {
			isa_device_interrupt(vector);
		} else {
			handle_irq(16 + i);
		}
	}
}

static void __init
eb64p_init_irq(void)
{
	long i;

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_CABRIOLET)
	if (inw(0x806) != 0xffff) {
		extern struct alpha_machine_vector cabriolet_mv;

		printk("Detected Cabriolet: correcting HWRPB.\n");

		hwrpb->sys_variation |= 2L << 10;
		hwrpb_update_checksum(hwrpb);

		alpha_mv = cabriolet_mv;
		alpha_mv.init_irq();
		return;
	}
#endif 

	outb(0xff, 0x26);
	outb(0xff, 0x27);

	init_i8259a_irqs();

	for (i = 16; i < 32; ++i) {
		irq_set_chip_and_handler(i, &eb64p_irq_type, handle_level_irq);
		irq_set_status_flags(i, IRQ_LEVEL);
	}

	common_init_isa_dma();
	setup_irq(16+5, &isa_cascade_irqaction);
}


static int __init
eb64p_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[5][5] __initdata = {
		
		{16+7, 16+7, 16+7, 16+7,  16+7},  
		{16+0, 16+0, 16+2, 16+4,  16+9},  
		{16+1, 16+1, 16+3, 16+8, 16+10},  
		{  -1,   -1,   -1,   -1,    -1},  
		{16+6, 16+6, 16+6, 16+6,  16+6},  
	};
	const long min_idsel = 5, max_idsel = 9, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}



#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_EB64P)
struct alpha_machine_vector eb64p_mv __initmv = {
	.vector_name		= "EB64+",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_APECS_IO,
	.machine_check		= apecs_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 32,
	.device_interrupt	= eb64p_device_interrupt,

	.init_arch		= apecs_init_arch,
	.init_irq		= eb64p_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= common_init_pci,
	.kill_arch		= NULL,
	.pci_map_irq		= eb64p_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(eb64p)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_EB66)
struct alpha_machine_vector eb66_mv __initmv = {
	.vector_name		= "EB66",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_LCA_IO,
	.machine_check		= lca_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 32,
	.device_interrupt	= eb64p_device_interrupt,

	.init_arch		= lca_init_arch,
	.init_irq		= eb64p_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= common_init_pci,
	.pci_map_irq		= eb64p_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(eb66)
#endif
