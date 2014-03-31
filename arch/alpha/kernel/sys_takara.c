/*
 *	linux/arch/alpha/kernel/sys_takara.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *
 * Code supporting the TAKARA.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>

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
#include "pc873xx.h"

static unsigned long cached_irq_mask[2] = { -1, -1 };

static inline void
takara_update_irq_hw(unsigned long irq, unsigned long mask)
{
	int regaddr;

	mask = (irq >= 64 ? mask << 16 : mask >> ((irq - 16) & 0x30));
	regaddr = 0x510 + (((irq - 16) >> 2) & 0x0c);
	outl(mask & 0xffff0000UL, regaddr);
}

static inline void
takara_enable_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;
	unsigned long mask;
	mask = (cached_irq_mask[irq >= 64] &= ~(1UL << (irq & 63)));
	takara_update_irq_hw(irq, mask);
}

static void
takara_disable_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;
	unsigned long mask;
	mask = (cached_irq_mask[irq >= 64] |= 1UL << (irq & 63));
	takara_update_irq_hw(irq, mask);
}

static struct irq_chip takara_irq_type = {
	.name		= "TAKARA",
	.irq_unmask	= takara_enable_irq,
	.irq_mask	= takara_disable_irq,
	.irq_mask_ack	= takara_disable_irq,
};

static void
takara_device_interrupt(unsigned long vector)
{
	unsigned intstatus;


	intstatus = inw(0x500) & 15;
	if (intstatus) {

		if (intstatus & 8) handle_irq(16+3);
		if (intstatus & 4) handle_irq(16+2);
		if (intstatus & 2) handle_irq(16+1);
		if (intstatus & 1) handle_irq(16+0);
	} else {
		isa_device_interrupt (vector);
	}
}

static void 
takara_srm_device_interrupt(unsigned long vector)
{
	int irq = (vector - 0x800) >> 4;
	handle_irq(irq);
}

static void __init
takara_init_irq(void)
{
	long i;

	init_i8259a_irqs();

	if (alpha_using_srm) {
		alpha_mv.device_interrupt = takara_srm_device_interrupt;
	} else {
		unsigned int ctlreg = inl(0x500);

		
		ctlreg &= ~0x8000;
		outl(ctlreg, 0x500);

		
		ctlreg = 0x05107c00;
		outl(ctlreg, 0x500);
	}

	for (i = 16; i < 128; i += 16)
		takara_update_irq_hw(i, -1);

	for (i = 16; i < 128; ++i) {
		irq_set_chip_and_handler(i, &takara_irq_type,
					 handle_level_irq);
		irq_set_status_flags(i, IRQ_LEVEL);
	}

	common_init_isa_dma();
}



static int __init
takara_map_irq_srm(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[15][5] __initdata = {
		{ 16+3, 16+3, 16+3, 16+3, 16+3},   
		{ 16+2, 16+2, 16+2, 16+2, 16+2},   
		{ 16+1, 16+1, 16+1, 16+1, 16+1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		
		{   12,   12,   13,   14,   15},   
		{    8,    8,    9,   19,   11},   
		{    4,    4,    5,    6,    7},   
		{    0,    0,    1,    2,    3},   
		{   -1,   -1,   -1,   -1,   -1},   
		{64+ 0, 64+0, 64+1, 64+2, 64+3},   
		{48+ 0, 48+0, 48+1, 48+2, 48+3},   
		{32+ 0, 32+0, 32+1, 32+2, 32+3},   
		{16+ 0, 16+0, 16+1, 16+2, 16+3},   
	};
	const long min_idsel = 6, max_idsel = 20, irqs_per_slot = 5;
        int irq = COMMON_TABLE_LOOKUP;
	if (irq >= 0 && irq < 16) {
		
		unsigned int busslot = PCI_SLOT(dev->bus->self->devfn);
		irq += irq_tab[busslot-min_idsel][0];
	}
	return irq;
}

static int __init
takara_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[15][5] __initdata = {
		{ 16+3, 16+3, 16+3, 16+3, 16+3},   
		{ 16+2, 16+2, 16+2, 16+2, 16+2},   
		{ 16+1, 16+1, 16+1, 16+1, 16+1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{   -1,   -1,   -1,   -1,   -1},   
		{ 16+3, 16+3, 16+3, 16+3, 16+3},   
		{ 16+2, 16+2, 16+2, 16+2, 16+2},   
		{ 16+1, 16+1, 16+1, 16+1, 16+1},   
	};
	const long min_idsel = 6, max_idsel = 20, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}

static u8 __init
takara_swizzle(struct pci_dev *dev, u8 *pinp)
{
	int slot = PCI_SLOT(dev->devfn);
	int pin = *pinp;
	unsigned int ctlreg = inl(0x500);
	unsigned int busslot;

	if (!dev->bus->self)
		return slot;

	busslot = PCI_SLOT(dev->bus->self->devfn);
	
	if (dev->bus->number != 0
	    && busslot > 16
	    && ((1<<(36-busslot)) & ctlreg)) {
		if (pin == 1)
			pin += (20 - busslot);
		else {
			printk(KERN_WARNING "takara_swizzle: can only "
			       "handle cards with INTA IRQ pin.\n");
		}
	} else {
		
		printk(KERN_WARNING "takara_swizzle: cannot handle "
		       "card-bridge behind builtin bridge yet.\n");
	}

	*pinp = pin;
	return slot;
}

static void __init
takara_init_pci(void)
{
	if (alpha_using_srm)
		alpha_mv.pci_map_irq = takara_map_irq_srm;

	cia_init_pci();

	if (pc873xx_probe() == -1) {
		printk(KERN_ERR "Probing for PC873xx Super IO chip failed.\n");
	} else {
		printk(KERN_INFO "Found %s Super IO chip at 0x%x\n",
			pc873xx_get_model(), pc873xx_get_base());
		pc873xx_enable_ide();
	}
}



struct alpha_machine_vector takara_mv __initmv = {
	.vector_name		= "Takara",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_CIA_IO,
	.machine_check		= cia_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= CIA_DEFAULT_MEM_BASE,

	.nr_irqs		= 128,
	.device_interrupt	= takara_device_interrupt,

	.init_arch		= cia_init_arch,
	.init_irq		= takara_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= takara_init_pci,
	.kill_arch		= cia_kill_arch,
	.pci_map_irq		= takara_map_irq,
	.pci_swizzle		= takara_swizzle,
};
ALIAS_MV(takara)
