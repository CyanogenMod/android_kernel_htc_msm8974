/*
 *	linux/arch/alpha/kernel/sys_sable.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *
 * Code supporting the Sable, Sable-Gamma, and Lynx systems.
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
#include <asm/core_t2.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"

DEFINE_SPINLOCK(sable_lynx_irq_lock);

typedef struct irq_swizzle_struct
{
	char irq_to_mask[64];
	char mask_to_irq[64];

	
	unsigned long shadow_mask;

	void (*update_irq_hw)(unsigned long bit, unsigned long mask);
	void (*ack_irq_hw)(unsigned long bit);

} irq_swizzle_t;

static irq_swizzle_t *sable_lynx_irq_swizzle;

static void sable_lynx_init_irq(int nr_of_irqs);

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_SABLE)


static void
sable_update_irq_hw(unsigned long bit, unsigned long mask)
{
	int port = 0x537;

	if (bit >= 16) {
		port = 0x53d;
		mask >>= 16;
	} else if (bit >= 8) {
		port = 0x53b;
		mask >>= 8;
	}

	outb(mask, port);
}

static void
sable_ack_irq_hw(unsigned long bit)
{
	int port, val1, val2;

	if (bit >= 16) {
		port = 0x53c;
		val1 = 0xE0 | (bit - 16);
		val2 = 0xE0 | 4;
	} else if (bit >= 8) {
		port = 0x53a;
		val1 = 0xE0 | (bit - 8);
		val2 = 0xE0 | 3;
	} else {
		port = 0x536;
		val1 = 0xE0 | (bit - 0);
		val2 = 0xE0 | 1;
	}

	outb(val1, port);	
	outb(val2, 0x534);	
}

static irq_swizzle_t sable_irq_swizzle = {
	{
		-1,  6, -1,  8, 15, 12,  7,  9,	
		-1, 16, 17, 18,  3, -1, 21, 22,	
		-1, -1, -1, -1, -1, -1, -1, -1,	
		-1, -1, -1, -1, -1, -1, -1, -1,	
		 2,  1,  0,  4,  5, -1, -1, -1,	
		-1, -1, -1, -1, -1, -1, -1, -1,	
		-1, -1, -1, -1, -1, -1, -1, -1,	
		-1, -1, -1, -1, -1, -1, -1, -1 	
	},
	{
		34, 33, 32, 12, 35, 36,  1,  6,	
		 3,  7, -1, -1,  5, -1, -1,  4,	
		 9, 10, 11, -1, -1, 14, 15, -1,	
		-1, -1, -1, -1, -1, -1, -1, -1,	
		-1, -1, -1, -1, -1, -1, -1, -1,	
		-1, -1, -1, -1, -1, -1, -1, -1,	
		-1, -1, -1, -1, -1, -1, -1, -1,	
		-1, -1, -1, -1, -1, -1, -1, -1	
	},
	-1,
	sable_update_irq_hw,
	sable_ack_irq_hw
};

static void __init
sable_init_irq(void)
{
	outb(-1, 0x537);	
	outb(-1, 0x53b);	
	outb(-1, 0x53d);	
	outb(0x44, 0x535);	

	sable_lynx_irq_swizzle = &sable_irq_swizzle;
	sable_lynx_init_irq(40);
}


static int __init
sable_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[9][5] __initdata = {
		
		{ 32+0,  32+0,  32+0,  32+0,  32+0},  
		{ 32+1,  32+1,  32+1,  32+1,  32+1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{ 32+2,  32+2,  32+2,  32+2,  32+2},  
		{ 32+3,  32+3,  32+3,  32+3,  32+3},  
		{ 32+4,  32+4,  32+4,  32+4,  32+4}   
	};
	long min_idsel = 0, max_idsel = 8, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}
#endif 

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_LYNX)


static void
lynx_update_irq_hw(unsigned long bit, unsigned long mask)
{
	*(vulp)T2_AIR = 0x40;
	mb();
	*(vulp)T2_AIR; 
	mb();
	*(vulp)T2_DIR = mask;    
	mb();
	mb();
}

static void
lynx_ack_irq_hw(unsigned long bit)
{
	*(vulp)T2_VAR = (u_long) bit;
	mb();
	mb();
}

static irq_swizzle_t lynx_irq_swizzle = {
	{ 
		-1,  6, -1,  8, 15, 12,  7,  9,	
		-1, 16, 17, 18,  3, -1, 21, 22,	
		-1, -1, -1, -1, -1, -1, -1, -1,	
		-1, -1, -1, -1, 28, -1, -1, -1,	
		32, 33, 34, 35, 36, 37, 38, 39,	
		40, 41, 42, 43, 44, 45, 46, 47,	
		48, 49, 50, 51, 52, 53, 54, 55,	
		56, 57, 58, 59, 60, 61, 62, 63	
	},
	{ 
		-1, -1, -1, 12, -1, -1,  1,  6,	
		 3,  7, -1, -1,  5, -1, -1,  4,	
		 9, 10, 11, -1, -1, 14, 15, -1,	
		-1, -1, -1, -1, 28, -1, -1, -1,	
		32, 33, 34, 35, 36, 37, 38, 39,	
		40, 41, 42, 43, 44, 45, 46, 47,	
		48, 49, 50, 51, 52, 53, 54, 55,	
		56, 57, 58, 59, 60, 61, 62, 63	
	},
	-1,
	lynx_update_irq_hw,
	lynx_ack_irq_hw
};

static void __init
lynx_init_irq(void)
{
	sable_lynx_irq_swizzle = &lynx_irq_swizzle;
	sable_lynx_init_irq(64);
}


static int __init
lynx_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[19][5] __initdata = {
		
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   28,    28,    28,    28,    28},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   32,    32,    33,    34,    35},  
		{   36,    36,    37,    38,    39},  
		{   40,    40,    41,    42,    43},  
		{   44,    44,    45,    46,    47},  
		{   -1,    -1,    -1,    -1,    -1},  
		
		{   -1,    -1,    -1,    -1,    -1},  
		{   28,    28,    28,    28,    28},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   -1,    -1,    -1,    -1,    -1},  
		{   48,    48,    49,    50,    51},  
		{   52,    52,    53,    54,    55},  
		{   56,    56,    57,    58,    59},  
		{   60,    60,    61,    62,    63}   
	};
	const long min_idsel = 2, max_idsel = 20, irqs_per_slot = 5;
	return COMMON_TABLE_LOOKUP;
}

static u8 __init
lynx_swizzle(struct pci_dev *dev, u8 *pinp)
{
	int slot, pin = *pinp;

	if (dev->bus->number == 0) {
		slot = PCI_SLOT(dev->devfn);
	}
	
	else if (PCI_SLOT(dev->bus->self->devfn) == 3) {
		slot = PCI_SLOT(dev->devfn) + 11;
	}
	else
	{
		
		do {
			if (PCI_SLOT(dev->bus->self->devfn) == 3) {
				slot = PCI_SLOT(dev->devfn) + 11;
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

#endif 


static inline void
sable_lynx_enable_irq(struct irq_data *d)
{
	unsigned long bit, mask;

	bit = sable_lynx_irq_swizzle->irq_to_mask[d->irq];
	spin_lock(&sable_lynx_irq_lock);
	mask = sable_lynx_irq_swizzle->shadow_mask &= ~(1UL << bit);
	sable_lynx_irq_swizzle->update_irq_hw(bit, mask);
	spin_unlock(&sable_lynx_irq_lock);
#if 0
	printk("%s: mask 0x%lx bit 0x%lx irq 0x%x\n",
	       __func__, mask, bit, irq);
#endif
}

static void
sable_lynx_disable_irq(struct irq_data *d)
{
	unsigned long bit, mask;

	bit = sable_lynx_irq_swizzle->irq_to_mask[d->irq];
	spin_lock(&sable_lynx_irq_lock);
	mask = sable_lynx_irq_swizzle->shadow_mask |= 1UL << bit;
	sable_lynx_irq_swizzle->update_irq_hw(bit, mask);
	spin_unlock(&sable_lynx_irq_lock);
#if 0
	printk("%s: mask 0x%lx bit 0x%lx irq 0x%x\n",
	       __func__, mask, bit, irq);
#endif
}

static void
sable_lynx_mask_and_ack_irq(struct irq_data *d)
{
	unsigned long bit, mask;

	bit = sable_lynx_irq_swizzle->irq_to_mask[d->irq];
	spin_lock(&sable_lynx_irq_lock);
	mask = sable_lynx_irq_swizzle->shadow_mask |= 1UL << bit;
	sable_lynx_irq_swizzle->update_irq_hw(bit, mask);
	sable_lynx_irq_swizzle->ack_irq_hw(bit);
	spin_unlock(&sable_lynx_irq_lock);
}

static struct irq_chip sable_lynx_irq_type = {
	.name		= "SABLE/LYNX",
	.irq_unmask	= sable_lynx_enable_irq,
	.irq_mask	= sable_lynx_disable_irq,
	.irq_mask_ack	= sable_lynx_mask_and_ack_irq,
};

static void 
sable_lynx_srm_device_interrupt(unsigned long vector)
{

	int bit, irq;

	bit = (vector - 0x800) >> 4;
	irq = sable_lynx_irq_swizzle->mask_to_irq[bit];
#if 0
	printk("%s: vector 0x%lx bit 0x%x irq 0x%x\n",
	       __func__, vector, bit, irq);
#endif
	handle_irq(irq);
}

static void __init
sable_lynx_init_irq(int nr_of_irqs)
{
	long i;

	for (i = 0; i < nr_of_irqs; ++i) {
		irq_set_chip_and_handler(i, &sable_lynx_irq_type,
					 handle_level_irq);
		irq_set_status_flags(i, IRQ_LEVEL);
	}

	common_init_isa_dma();
}

static void __init
sable_lynx_init_pci(void)
{
	common_init_pci();
}


#if defined(CONFIG_ALPHA_GENERIC) || \
    (defined(CONFIG_ALPHA_SABLE) && !defined(CONFIG_ALPHA_GAMMA))
#undef GAMMA_BIAS
#define GAMMA_BIAS 0
struct alpha_machine_vector sable_mv __initmv = {
	.vector_name		= "Sable",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_T2_IO,
	.machine_check		= t2_machine_check,
	.max_isa_dma_address	= ALPHA_SABLE_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= EISA_DEFAULT_IO_BASE,
	.min_mem_address	= T2_DEFAULT_MEM_BASE,

	.nr_irqs		= 40,
	.device_interrupt	= sable_lynx_srm_device_interrupt,

	.init_arch		= t2_init_arch,
	.init_irq		= sable_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= sable_lynx_init_pci,
	.kill_arch		= t2_kill_arch,
	.pci_map_irq		= sable_map_irq,
	.pci_swizzle		= common_swizzle,

	.sys = { .t2 = {
	    .gamma_bias		= 0
	} }
};
ALIAS_MV(sable)
#endif 

#if defined(CONFIG_ALPHA_GENERIC) || \
    (defined(CONFIG_ALPHA_SABLE) && defined(CONFIG_ALPHA_GAMMA))
#undef GAMMA_BIAS
#define GAMMA_BIAS _GAMMA_BIAS
struct alpha_machine_vector sable_gamma_mv __initmv = {
	.vector_name		= "Sable-Gamma",
	DO_EV5_MMU,
	DO_DEFAULT_RTC,
	DO_T2_IO,
	.machine_check		= t2_machine_check,
	.max_isa_dma_address	= ALPHA_SABLE_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= EISA_DEFAULT_IO_BASE,
	.min_mem_address	= T2_DEFAULT_MEM_BASE,

	.nr_irqs		= 40,
	.device_interrupt	= sable_lynx_srm_device_interrupt,

	.init_arch		= t2_init_arch,
	.init_irq		= sable_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= sable_lynx_init_pci,
	.kill_arch		= t2_kill_arch,
	.pci_map_irq		= sable_map_irq,
	.pci_swizzle		= common_swizzle,

	.sys = { .t2 = {
	    .gamma_bias		= _GAMMA_BIAS
	} }
};
ALIAS_MV(sable_gamma)
#endif 

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_LYNX)
#undef GAMMA_BIAS
#define GAMMA_BIAS _GAMMA_BIAS
struct alpha_machine_vector lynx_mv __initmv = {
	.vector_name		= "Lynx",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_T2_IO,
	.machine_check		= t2_machine_check,
	.max_isa_dma_address	= ALPHA_SABLE_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= EISA_DEFAULT_IO_BASE,
	.min_mem_address	= T2_DEFAULT_MEM_BASE,

	.nr_irqs		= 64,
	.device_interrupt	= sable_lynx_srm_device_interrupt,

	.init_arch		= t2_init_arch,
	.init_irq		= lynx_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= sable_lynx_init_pci,
	.kill_arch		= t2_kill_arch,
	.pci_map_irq		= lynx_map_irq,
	.pci_swizzle		= lynx_swizzle,

	.sys = { .t2 = {
	    .gamma_bias		= _GAMMA_BIAS
	} }
};
ALIAS_MV(lynx)
#endif 
