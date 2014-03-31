/*
 *	linux/arch/alpha/kernel/sys_dp264.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996, 1999 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *
 *	Modified by Christopher C. Chimelis, 2001 to
 *	add support for the addition of Shark to the
 *	Tsunami family.
 *
 * Code supporting the DP264 (EV6+TSUNAMI).
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
#include <asm/core_tsunami.h>
#include <asm/hwrpb.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"


static unsigned long cached_irq_mask;
static unsigned long cpu_irq_affinity[4] = { 0UL, 0UL, 0UL, 0UL };

DEFINE_SPINLOCK(dp264_irq_lock);

static void
tsunami_update_irq_hw(unsigned long mask)
{
	register tsunami_cchip *cchip = TSUNAMI_cchip;
	unsigned long isa_enable = 1UL << 55;
	register int bcpu = boot_cpuid;

#ifdef CONFIG_SMP
	volatile unsigned long *dim0, *dim1, *dim2, *dim3;
	unsigned long mask0, mask1, mask2, mask3, dummy;

	mask &= ~isa_enable;
	mask0 = mask & cpu_irq_affinity[0];
	mask1 = mask & cpu_irq_affinity[1];
	mask2 = mask & cpu_irq_affinity[2];
	mask3 = mask & cpu_irq_affinity[3];

	if (bcpu == 0) mask0 |= isa_enable;
	else if (bcpu == 1) mask1 |= isa_enable;
	else if (bcpu == 2) mask2 |= isa_enable;
	else mask3 |= isa_enable;

	dim0 = &cchip->dim0.csr;
	dim1 = &cchip->dim1.csr;
	dim2 = &cchip->dim2.csr;
	dim3 = &cchip->dim3.csr;
	if (!cpu_possible(0)) dim0 = &dummy;
	if (!cpu_possible(1)) dim1 = &dummy;
	if (!cpu_possible(2)) dim2 = &dummy;
	if (!cpu_possible(3)) dim3 = &dummy;

	*dim0 = mask0;
	*dim1 = mask1;
	*dim2 = mask2;
	*dim3 = mask3;
	mb();
	*dim0;
	*dim1;
	*dim2;
	*dim3;
#else
	volatile unsigned long *dimB;
	if (bcpu == 0) dimB = &cchip->dim0.csr;
	else if (bcpu == 1) dimB = &cchip->dim1.csr;
	else if (bcpu == 2) dimB = &cchip->dim2.csr;
	else dimB = &cchip->dim3.csr;

	*dimB = mask | isa_enable;
	mb();
	*dimB;
#endif
}

static void
dp264_enable_irq(struct irq_data *d)
{
	spin_lock(&dp264_irq_lock);
	cached_irq_mask |= 1UL << d->irq;
	tsunami_update_irq_hw(cached_irq_mask);
	spin_unlock(&dp264_irq_lock);
}

static void
dp264_disable_irq(struct irq_data *d)
{
	spin_lock(&dp264_irq_lock);
	cached_irq_mask &= ~(1UL << d->irq);
	tsunami_update_irq_hw(cached_irq_mask);
	spin_unlock(&dp264_irq_lock);
}

static void
clipper_enable_irq(struct irq_data *d)
{
	spin_lock(&dp264_irq_lock);
	cached_irq_mask |= 1UL << (d->irq - 16);
	tsunami_update_irq_hw(cached_irq_mask);
	spin_unlock(&dp264_irq_lock);
}

static void
clipper_disable_irq(struct irq_data *d)
{
	spin_lock(&dp264_irq_lock);
	cached_irq_mask &= ~(1UL << (d->irq - 16));
	tsunami_update_irq_hw(cached_irq_mask);
	spin_unlock(&dp264_irq_lock);
}

static void
cpu_set_irq_affinity(unsigned int irq, cpumask_t affinity)
{
	int cpu;

	for (cpu = 0; cpu < 4; cpu++) {
		unsigned long aff = cpu_irq_affinity[cpu];
		if (cpumask_test_cpu(cpu, &affinity))
			aff |= 1UL << irq;
		else
			aff &= ~(1UL << irq);
		cpu_irq_affinity[cpu] = aff;
	}
}

static int
dp264_set_affinity(struct irq_data *d, const struct cpumask *affinity,
		   bool force)
{
	spin_lock(&dp264_irq_lock);
	cpu_set_irq_affinity(d->irq, *affinity);
	tsunami_update_irq_hw(cached_irq_mask);
	spin_unlock(&dp264_irq_lock);

	return 0;
}

static int
clipper_set_affinity(struct irq_data *d, const struct cpumask *affinity,
		     bool force)
{
	spin_lock(&dp264_irq_lock);
	cpu_set_irq_affinity(d->irq - 16, *affinity);
	tsunami_update_irq_hw(cached_irq_mask);
	spin_unlock(&dp264_irq_lock);

	return 0;
}

static struct irq_chip dp264_irq_type = {
	.name			= "DP264",
	.irq_unmask		= dp264_enable_irq,
	.irq_mask		= dp264_disable_irq,
	.irq_mask_ack		= dp264_disable_irq,
	.irq_set_affinity	= dp264_set_affinity,
};

static struct irq_chip clipper_irq_type = {
	.name			= "CLIPPER",
	.irq_unmask		= clipper_enable_irq,
	.irq_mask		= clipper_disable_irq,
	.irq_mask_ack		= clipper_disable_irq,
	.irq_set_affinity	= clipper_set_affinity,
};

static void
dp264_device_interrupt(unsigned long vector)
{
#if 1
	printk("dp264_device_interrupt: NOT IMPLEMENTED YET!!\n");
#else
	unsigned long pld;
	unsigned int i;

	
	pld = TSUNAMI_cchip->dir0.csr;

	while (pld) {
		i = ffz(~pld);
		pld &= pld - 1; 
		if (i == 55)
			isa_device_interrupt(vector);
		else
			handle_irq(16 + i);
#if 0
		TSUNAMI_cchip->dir0.csr = 1UL << i; mb();
		tmp = TSUNAMI_cchip->dir0.csr;
#endif
	}
#endif
}

static void 
dp264_srm_device_interrupt(unsigned long vector)
{
	int irq;

	irq = (vector - 0x800) >> 4;

	if (irq >= 32)
		irq -= 16;

	handle_irq(irq);
}

static void 
clipper_srm_device_interrupt(unsigned long vector)
{
	int irq;

	irq = (vector - 0x800) >> 4;

	handle_irq(irq);
}

static void __init
init_tsunami_irqs(struct irq_chip * ops, int imin, int imax)
{
	long i;
	for (i = imin; i <= imax; ++i) {
		irq_set_chip_and_handler(i, ops, handle_level_irq);
		irq_set_status_flags(i, IRQ_LEVEL);
	}
}

static void __init
dp264_init_irq(void)
{
	outb(0, DMA1_RESET_REG);
	outb(0, DMA2_RESET_REG);
	outb(DMA_MODE_CASCADE, DMA2_MODE_REG);
	outb(0, DMA2_MASK_REG);

	if (alpha_using_srm)
		alpha_mv.device_interrupt = dp264_srm_device_interrupt;

	tsunami_update_irq_hw(0);

	init_i8259a_irqs();
	init_tsunami_irqs(&dp264_irq_type, 16, 47);
}

static void __init
clipper_init_irq(void)
{
	outb(0, DMA1_RESET_REG);
	outb(0, DMA2_RESET_REG);
	outb(DMA_MODE_CASCADE, DMA2_MODE_REG);
	outb(0, DMA2_MASK_REG);

	if (alpha_using_srm)
		alpha_mv.device_interrupt = clipper_srm_device_interrupt;

	tsunami_update_irq_hw(0);

	init_i8259a_irqs();
	init_tsunami_irqs(&clipper_irq_type, 24, 63);
}



static int __init
isa_irq_fixup(const struct pci_dev *dev, int irq)
{
	u8 irq8;

	if (irq > 0)
		return irq;

	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq8);

	return irq8 & 0xf;
}

static int __init
dp264_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[6][5] __initdata = {
		
		{    -1,    -1,    -1,    -1,    -1}, 
		{ 16+ 3, 16+ 3, 16+ 2, 16+ 2, 16+ 2}, 
		{ 16+15, 16+15, 16+14, 16+13, 16+12}, 
		{ 16+11, 16+11, 16+10, 16+ 9, 16+ 8}, 
		{ 16+ 7, 16+ 7, 16+ 6, 16+ 5, 16+ 4}, 
		{ 16+ 3, 16+ 3, 16+ 2, 16+ 1, 16+ 0}  
	};
	const long min_idsel = 5, max_idsel = 10, irqs_per_slot = 5;
	struct pci_controller *hose = dev->sysdata;
	int irq = COMMON_TABLE_LOOKUP;

	if (irq > 0)
		irq += 16 * hose->index;

	return isa_irq_fixup(dev, irq);
}

static int __init
monet_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[13][5] __initdata = {
		
		{    45,    45,    45,    45,    45}, 
		{    -1,    -1,    -1,    -1,    -1}, 
		{    -1,    -1,    -1,    -1,    -1}, 
		{    47,    47,    47,    47,    47}, 
		{    -1,    -1,    -1,    -1,    -1}, 
		{    -1,    -1,    -1,    -1,    -1}, 
#if 1
		{    28,    28,    29,    30,    31}, 
		{    24,    24,    25,    26,    27}, 
#else
		{    -1,    -1,    -1,    -1,    -1}, 
		{    -1,    -1,    -1,    -1,    -1}, 
#endif
		{    40,    40,    41,    42,    43}, 
		{    36,    36,    37,    38,    39}, 
		{    32,    32,    33,    34,    35}, 
		{    28,    28,    29,    30,    31}, 
		{    24,    24,    25,    26,    27}  
	};
	const long min_idsel = 3, max_idsel = 15, irqs_per_slot = 5;

	return isa_irq_fixup(dev, COMMON_TABLE_LOOKUP);
}

static u8 __init
monet_swizzle(struct pci_dev *dev, u8 *pinp)
{
	struct pci_controller *hose = dev->sysdata;
	int slot, pin = *pinp;

	if (!dev->bus->parent) {
		slot = PCI_SLOT(dev->devfn);
	}
	
	else if (hose->index == 1 && PCI_SLOT(dev->bus->self->devfn) == 8) {
		slot = PCI_SLOT(dev->devfn);
	} else {
		
		do {
			
			if (hose->index == 1 &&
			    PCI_SLOT(dev->bus->self->devfn) == 8) {
				slot = PCI_SLOT(dev->devfn);
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

static int __init
webbrick_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[13][5] __initdata = {
		
		{    -1,    -1,    -1,    -1,    -1}, 
		{    -1,    -1,    -1,    -1,    -1}, 
		{    29,    29,    29,    29,    29}, 
		{    -1,    -1,    -1,    -1,    -1}, 
		{    30,    30,    30,    30,    30}, 
		{    -1,    -1,    -1,    -1,    -1}, 
		{    -1,    -1,    -1,    -1,    -1}, 
		{    35,    35,    34,    33,    32}, 
		{    39,    39,    38,    37,    36}, 
		{    43,    43,    42,    41,    40}, 
		{    47,    47,    46,    45,    44}, 
	};
	const long min_idsel = 7, max_idsel = 17, irqs_per_slot = 5;

	return isa_irq_fixup(dev, COMMON_TABLE_LOOKUP);
}

static int __init
clipper_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[7][5] __initdata = {
		
		{ 16+ 8, 16+ 8, 16+ 9, 16+10, 16+11}, 
		{ 16+12, 16+12, 16+13, 16+14, 16+15}, 
		{ 16+16, 16+16, 16+17, 16+18, 16+19}, 
		{ 16+20, 16+20, 16+21, 16+22, 16+23}, 
		{ 16+24, 16+24, 16+25, 16+26, 16+27}, 
		{ 16+28, 16+28, 16+29, 16+30, 16+31}, 
		{    -1,    -1,    -1,    -1,    -1}  
	};
	const long min_idsel = 1, max_idsel = 7, irqs_per_slot = 5;
	struct pci_controller *hose = dev->sysdata;
	int irq = COMMON_TABLE_LOOKUP;

	if (irq > 0)
		irq += 16 * hose->index;

	return isa_irq_fixup(dev, irq);
}

static void __init
dp264_init_pci(void)
{
	common_init_pci();
	SMC669_Init(0);
	locate_and_init_vga(NULL);
}

static void __init
monet_init_pci(void)
{
	common_init_pci();
	SMC669_Init(1);
	es1888_init();
	locate_and_init_vga(NULL);
}

static void __init
clipper_init_pci(void)
{
	common_init_pci();
	locate_and_init_vga(NULL);
}

static void __init
webbrick_init_arch(void)
{
	tsunami_init_arch();

	
	hose_head->sg_isa->align_entry = 4;
	hose_head->sg_pci->align_entry = 4;
}



struct alpha_machine_vector dp264_mv __initmv = {
	.vector_name		= "DP264",
	DO_EV6_MMU,
	DO_DEFAULT_RTC,
	DO_TSUNAMI_IO,
	.machine_check		= tsunami_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= DEFAULT_MEM_BASE,
	.pci_dac_offset		= TSUNAMI_DAC_OFFSET,

	.nr_irqs		= 64,
	.device_interrupt	= dp264_device_interrupt,

	.init_arch		= tsunami_init_arch,
	.init_irq		= dp264_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= dp264_init_pci,
	.kill_arch		= tsunami_kill_arch,
	.pci_map_irq		= dp264_map_irq,
	.pci_swizzle		= common_swizzle,
};
ALIAS_MV(dp264)

struct alpha_machine_vector monet_mv __initmv = {
	.vector_name		= "Monet",
	DO_EV6_MMU,
	DO_DEFAULT_RTC,
	DO_TSUNAMI_IO,
	.machine_check		= tsunami_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= DEFAULT_MEM_BASE,
	.pci_dac_offset		= TSUNAMI_DAC_OFFSET,

	.nr_irqs		= 64,
	.device_interrupt	= dp264_device_interrupt,

	.init_arch		= tsunami_init_arch,
	.init_irq		= dp264_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= monet_init_pci,
	.kill_arch		= tsunami_kill_arch,
	.pci_map_irq		= monet_map_irq,
	.pci_swizzle		= monet_swizzle,
};

struct alpha_machine_vector webbrick_mv __initmv = {
	.vector_name		= "Webbrick",
	DO_EV6_MMU,
	DO_DEFAULT_RTC,
	DO_TSUNAMI_IO,
	.machine_check		= tsunami_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= DEFAULT_MEM_BASE,
	.pci_dac_offset		= TSUNAMI_DAC_OFFSET,

	.nr_irqs		= 64,
	.device_interrupt	= dp264_device_interrupt,

	.init_arch		= webbrick_init_arch,
	.init_irq		= dp264_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= common_init_pci,
	.kill_arch		= tsunami_kill_arch,
	.pci_map_irq		= webbrick_map_irq,
	.pci_swizzle		= common_swizzle,
};

struct alpha_machine_vector clipper_mv __initmv = {
	.vector_name		= "Clipper",
	DO_EV6_MMU,
	DO_DEFAULT_RTC,
	DO_TSUNAMI_IO,
	.machine_check		= tsunami_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= DEFAULT_MEM_BASE,
	.pci_dac_offset		= TSUNAMI_DAC_OFFSET,

	.nr_irqs		= 64,
	.device_interrupt	= dp264_device_interrupt,

	.init_arch		= tsunami_init_arch,
	.init_irq		= clipper_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= clipper_init_pci,
	.kill_arch		= tsunami_kill_arch,
	.pci_map_irq		= clipper_map_irq,
	.pci_swizzle		= common_swizzle,
};


struct alpha_machine_vector shark_mv __initmv = {
	.vector_name		= "Shark",
	DO_EV6_MMU,
	DO_DEFAULT_RTC,
	DO_TSUNAMI_IO,
	.machine_check		= tsunami_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= DEFAULT_MEM_BASE,
	.pci_dac_offset		= TSUNAMI_DAC_OFFSET,

	.nr_irqs		= 64,
	.device_interrupt	= dp264_device_interrupt,

	.init_arch		= tsunami_init_arch,
	.init_irq		= clipper_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= common_init_pci,
	.kill_arch		= tsunami_kill_arch,
	.pci_map_irq		= clipper_map_irq,
	.pci_swizzle		= common_swizzle,
};

