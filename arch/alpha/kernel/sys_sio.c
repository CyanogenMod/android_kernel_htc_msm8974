/*
 *	linux/arch/alpha/kernel/sys_sio.c
 *
 *	Copyright (C) 1995 David A Rusling
 *	Copyright (C) 1996 Jay A Estabrook
 *	Copyright (C) 1998, 1999 Richard Henderson
 *
 * Code for all boards that route the PCI interrupts through the SIO
 * PCI/ISA bridge.  This includes Noname (AXPpci33), Multia (UDB),
 * Kenetics's Platform 2000, Avanti (AlphaStation), XL, and AlphaBook1.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/screen_info.h>

#include <asm/compiler.h>
#include <asm/ptrace.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/mmu_context.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/core_apecs.h>
#include <asm/core_lca.h>
#include <asm/tlbflush.h>

#include "proto.h"
#include "irq_impl.h"
#include "pci_impl.h"
#include "machvec_impl.h"
#include "pc873xx.h"

#if defined(ALPHA_RESTORE_SRM_SETUP)
struct 
{
	unsigned int orig_route_tab; 
} saved_config __attribute((common));
#endif


static void __init
sio_init_irq(void)
{
	if (alpha_using_srm)
		alpha_mv.device_interrupt = srm_device_interrupt;

	init_i8259a_irqs();
	common_init_isa_dma();
}

static inline void __init
alphabook1_init_arch(void)
{
	screen_info.orig_y = 37;
	screen_info.orig_video_cols = 100;
	screen_info.orig_video_lines = 37;

	lca_init_arch();
}



static void __init
sio_pci_route(void)
{
	unsigned int orig_route_tab;

	
	pci_bus_read_config_dword(pci_isa_hose->bus, PCI_DEVFN(7, 0), 0x60,
				  &orig_route_tab);
	printk("%s: PIRQ original 0x%x new 0x%x\n", __func__,
	       orig_route_tab, alpha_mv.sys.sio.route_tab);

#if defined(ALPHA_RESTORE_SRM_SETUP)
	saved_config.orig_route_tab = orig_route_tab;
#endif

	
	pci_bus_write_config_dword(pci_isa_hose->bus, PCI_DEVFN(7, 0), 0x60,
				   alpha_mv.sys.sio.route_tab);
}

static unsigned int __init
sio_collect_irq_levels(void)
{
	unsigned int level_bits = 0;
	struct pci_dev *dev = NULL;

	
	for_each_pci_dev(dev) {
		if ((dev->class >> 16 == PCI_BASE_CLASS_BRIDGE) &&
		    (dev->class >> 8 != PCI_CLASS_BRIDGE_PCMCIA))
			continue;

		if (dev->irq)
			level_bits |= (1 << dev->irq);
	}
	return level_bits;
}

static void __init
sio_fixup_irq_levels(unsigned int level_bits)
{
	unsigned int old_level_bits;

	old_level_bits = inb(0x4d0) | (inb(0x4d1) << 8);

	level_bits |= (old_level_bits & 0x71ff);

	outb((level_bits >> 0) & 0xff, 0x4d0);
	outb((level_bits >> 8) & 0xff, 0x4d1);
}

static inline int __init
noname_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[][5] __initdata = {
		
		{ 3,  3,  3,  3,  3},  
		{-1, -1, -1, -1, -1}, 
		{ 2,  2, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{ 0,  0,  2,  1,  0}, 
		{ 1,  1,  0,  2,  1}, 
		{ 2,  2,  1,  0,  2}, 
		{ 0,  0,  0,  0,  0}, 
	};
	const long min_idsel = 6, max_idsel = 14, irqs_per_slot = 5;
	int irq = COMMON_TABLE_LOOKUP, tmp;
	tmp = __kernel_extbl(alpha_mv.sys.sio.route_tab, irq);
	return irq >= 0 ? tmp : -1;
}

static inline int __init
p2k_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	static char irq_tab[][5] __initdata = {
		
		{ 0,  0, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{ 1,  1,  2,  3,  0}, 
		{ 2,  2,  3,  0,  1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{ 3,  3, -1, -1, -1}, 
	};
	const long min_idsel = 6, max_idsel = 12, irqs_per_slot = 5;
	int irq = COMMON_TABLE_LOOKUP, tmp;
	tmp = __kernel_extbl(alpha_mv.sys.sio.route_tab, irq);
	return irq >= 0 ? tmp : -1;
}

static inline void __init
noname_init_pci(void)
{
	common_init_pci();
	sio_pci_route();
	sio_fixup_irq_levels(sio_collect_irq_levels());

	if (pc873xx_probe() == -1) {
		printk(KERN_ERR "Probing for PC873xx Super IO chip failed.\n");
	} else {
		printk(KERN_INFO "Found %s Super IO chip at 0x%x\n",
			pc873xx_get_model(), pc873xx_get_base());


#if !defined(CONFIG_ALPHA_AVANTI)
		pc873xx_enable_ide();
#endif
		pc873xx_enable_epp19();
	}
}

static inline void __init
alphabook1_init_pci(void)
{
	struct pci_dev *dev;
	unsigned char orig, config;

	common_init_pci();
	sio_pci_route();


	dev = NULL;
	while ((dev = pci_get_device(PCI_VENDOR_ID_NCR, PCI_ANY_ID, dev))) {
		if (dev->device == PCI_DEVICE_ID_NCR_53C810
		    || dev->device == PCI_DEVICE_ID_NCR_53C815
		    || dev->device == PCI_DEVICE_ID_NCR_53C820
		    || dev->device == PCI_DEVICE_ID_NCR_53C825) {
			unsigned long io_port;
			unsigned char ctest4;

			io_port = dev->resource[0].start;
			ctest4 = inb(io_port+0x21);
			if (!(ctest4 & 0x80)) {
				printk("AlphaBook1 NCR init: setting"
				       " burst disable\n");
				outb(ctest4 | 0x80, io_port+0x21);
			}
                }
	}

	
	sio_fixup_irq_levels(0);

	
	outb(0x0f, 0x3ce); orig = inb(0x3cf);   
	outb(0x0f, 0x3ce); outb(0x05, 0x3cf);   
	outb(0x0b, 0x3ce); config = inb(0x3cf); 
	if ((config & 0xc0) != 0xc0) {
		printk("AlphaBook1 VGA init: setting 1Mb memory\n");
		config |= 0xc0;
		outb(0x0b, 0x3ce); outb(config, 0x3cf); 
	}
	outb(0x0f, 0x3ce); outb(orig, 0x3cf); 
}

void
sio_kill_arch(int mode)
{
#if defined(ALPHA_RESTORE_SRM_SETUP)
 	pci_bus_write_config_dword(pci_isa_hose->bus, PCI_DEVFN(7, 0), 0x60,
				   saved_config.orig_route_tab);
#endif
}



#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_BOOK1)
struct alpha_machine_vector alphabook1_mv __initmv = {
	.vector_name		= "AlphaBook1",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_LCA_IO,
	.machine_check		= lca_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 16,
	.device_interrupt	= isa_device_interrupt,

	.init_arch		= alphabook1_init_arch,
	.init_irq		= sio_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= alphabook1_init_pci,
	.kill_arch		= sio_kill_arch,
	.pci_map_irq		= noname_map_irq,
	.pci_swizzle		= common_swizzle,

	.sys = { .sio = {
		
		.route_tab	= 0x0e0f0a0a,
	}}
};
ALIAS_MV(alphabook1)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_AVANTI)
struct alpha_machine_vector avanti_mv __initmv = {
	.vector_name		= "Avanti",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_APECS_IO,
	.machine_check		= apecs_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 16,
	.device_interrupt	= isa_device_interrupt,

	.init_arch		= apecs_init_arch,
	.init_irq		= sio_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= noname_init_pci,
	.kill_arch		= sio_kill_arch,
	.pci_map_irq		= noname_map_irq,
	.pci_swizzle		= common_swizzle,

	.sys = { .sio = {
		.route_tab	= 0x0b0a050f, 
	}}
};
ALIAS_MV(avanti)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_NONAME)
struct alpha_machine_vector noname_mv __initmv = {
	.vector_name		= "Noname",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_LCA_IO,
	.machine_check		= lca_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 16,
	.device_interrupt	= srm_device_interrupt,

	.init_arch		= lca_init_arch,
	.init_irq		= sio_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= noname_init_pci,
	.kill_arch		= sio_kill_arch,
	.pci_map_irq		= noname_map_irq,
	.pci_swizzle		= common_swizzle,

	.sys = { .sio = {

		.route_tab	= 0x0b0a0f0d,
	}}
};
ALIAS_MV(noname)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_P2K)
struct alpha_machine_vector p2k_mv __initmv = {
	.vector_name		= "Platform2000",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_LCA_IO,
	.machine_check		= lca_machine_check,
	.max_isa_dma_address	= ALPHA_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= APECS_AND_LCA_DEFAULT_MEM_BASE,

	.nr_irqs		= 16,
	.device_interrupt	= srm_device_interrupt,

	.init_arch		= lca_init_arch,
	.init_irq		= sio_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= noname_init_pci,
	.kill_arch		= sio_kill_arch,
	.pci_map_irq		= p2k_map_irq,
	.pci_swizzle		= common_swizzle,

	.sys = { .sio = {
		.route_tab	= 0x0b0a090f,
	}}
};
ALIAS_MV(p2k)
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_XL)
struct alpha_machine_vector xl_mv __initmv = {
	.vector_name		= "XL",
	DO_EV4_MMU,
	DO_DEFAULT_RTC,
	DO_APECS_IO,
	.machine_check		= apecs_machine_check,
	.max_isa_dma_address	= ALPHA_XL_MAX_ISA_DMA_ADDRESS,
	.min_io_address		= DEFAULT_IO_BASE,
	.min_mem_address	= XL_DEFAULT_MEM_BASE,

	.nr_irqs		= 16,
	.device_interrupt	= isa_device_interrupt,

	.init_arch		= apecs_init_arch,
	.init_irq		= sio_init_irq,
	.init_rtc		= common_init_rtc,
	.init_pci		= noname_init_pci,
	.kill_arch		= sio_kill_arch,
	.pci_map_irq		= noname_map_irq,
	.pci_swizzle		= common_swizzle,

	.sys = { .sio = {
		.route_tab	= 0x0b0a090f,
	}}
};
ALIAS_MV(xl)
#endif
