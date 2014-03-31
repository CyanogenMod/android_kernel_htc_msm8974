/*
 *	linux/arch/alpha/kernel/sys_noritake.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *
 * Code supporting the NORITAKE (AlphaServer 1000A), 
 * CORELLE (AlphaServer 800), and ALCOR Primo (AlphaStation 600A).
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
noritake_update_irq_hw(int irq, int mask)
{
	int port = 0x54a;
	if (irq >= 32) {
	    mask >>= 16;
	    port = 0x54c;
	}
	outw(mask, port);
}

static void
noritake_enable_irq(struct irq_data *d)
{
	noritake_update_irq_hw(d->irq, cached_irq_mask |= 1 << (d->irq - 16));
}

static void
noritake_disable_irq(struct irq_data *d)
{
	noritake_update_irq_hw(d->irq, cached_irq_mask &= ~(1 << (d->irq - 16)));
}

static struct irq_chip noritake_irq_type = {
	.name		= "NORITAKE",
	.irq_unmask	= noritake_enable_irq,
	.irq_mask	= noritake_disable_irq,
	.irq_mask_ack	= noritake_disable_irq,
};

static void 
noritake_device_interrupt(unsigned long vector)
{
	unsigned long pld;
	unsigned int i;

	
	pld = (((unsigned long) inw(0x54c) << 32)
	       | ((unsigned long) inw(0x54a) << 16)
	       | ((unsigned long) inb(0xa0) << 8)
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

static void 
noritake_srm_device_interrupt(unsigned long vector)
{
	int irq;

	irq = (vector - 0x800) >> 4;

	if (irq >= 16)
		irq = irq + 1;

	handle_irq(irq);
}

static void __init
noritake_init_irq(void)
{
	long i;

	if (alpha_using_srm)
		alpha_mv.device_interrupt = noritake_srm_device_interrupt;

	outw(0, 0x54a);
	outw(0, 0x54c);

	for (i = 16; i < 48; ++i) {
		irq_set_chip_and_handler(i, &noritake_irq_type,
					 handle_level_irq);
		irq_set_status_flags(i, IRQ_LEVEL);
	}

	init_i8259a_irqs();
	common_init_isa_dma();
}



static int __init
noritake_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[15][5] __initdata = {
		
		
		{ 16+1,  16+1,  16+1,  16+1,  16+1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{ 16+2,  16+2,  16+3,  32+2,  32+3},  
		{ 16+4,  16+4,  16+5,  32+4,  32+5},  
		{ 16+6,  16+6,  16+7,  32+6,  32+7},  
		{ 16+8,  16+8,  16+9,  32+8,  32+9},  
		{ 16+1,  16+1,  16+1,  16+1,  16+1},  
		{ 16+8,  16+8,  16+9,  32+8,  32+9},  
		{16+10, 16+10, 16+11, 32+10, 32+11},  
		{16+12, 16+12, 16+13, 32+12, 32+13},  
		{16+14, 16+14, 16+15, 32+14, 32+15},  
	};
	const long min_idsel = 5, max_idsel = 19, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}

static u8 __init
noritake_swizzle(struct pci_dev *dev, u8 *pinp)
{
	int slot, pin = *pinp;

	if (dev->bus->number == 0) {
		slot = PCI_SLOT(dev->devfn);
	}
	
	else if (PCI_SLOT(dev->bus->self->devfn) == 8) {
		slot = PCI_SLOT(dev->devfn) + 15; 
	}
	else
	{
		
		do {
			if (PCI_SLOT(dev->bus->self->devfn) == 8) {
				slot = PCI_SLOT(dev->devfn) + 15;
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

#if defined(CONFIG_ALPHA_GENERIC) || !defined(CONFIG_ALPHA_PRIMO)
static void
noritake_apecs_machine_check(unsigned long vector, unsigned long la_ptr)
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
        process_mcheck_info(vector, la_ptr, "NORITAKE APECS",
                            (mcheck_expected(0)
                             && (code == MCHK_NO_DEVSEL
                                 || code == MCHK_NO_TABT)));
}
#endif



#if defined(CONFIG_ALPHA_GENERIC) || !defined(CONFIG_ALPHA_PRIMO)
struct alpha_machine_vector noritake_mv __initmv = {
	.vector_name		= "Noritake",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_APECS_IO,
	.machine_check		= noritake_apecs_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= EISA_DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 48,
	.device_interrupt	= noritake_device_interrupt,

	.init_arch		= apecs_init_arch,
	.init_irq		= noritake_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= common_init_pci,
	.pci_map_irq		= noritake_map_irq,
	.pci_swizzle		= noritake_swizzle,
};
ALIAS_MV(noritake)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_PRIMO)
struct alpha_machine_vector noritake_primo_mv __initmv = {
	.vector_name		= "Noritake-Primo",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_CIA_IO,
	.machine_check		= cia_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= EISA_DEFAULT_IO_BASE,
	.min_mem_address	= CIA_DEFAULT_MEM_BASE,

	.nr_irqs		= 48,
	.device_interrupt	= noritake_device_interrupt,

	.init_arch		= cia_init_arch,
	.init_irq		= noritake_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= cia_init_pci,
	.kill_arch		= cia_kill_arch,
	.pci_map_irq		= noritake_map_irq,
	.pci_swizzle		= noritake_swizzle,
};
ALIAS_MV(noritake_primo)
#endif
