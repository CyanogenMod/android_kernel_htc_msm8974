/*
 *  linux/arch/arm/mach-footbridge/common.c
 *
 *  Copyright (C) 1998-2000 Russell King, Dave Gilbert.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/spinlock.h>
 
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <asm/system_misc.h>
#include <asm/hardware/dec21285.h>

#include <asm/mach/irq.h>
#include <asm/mach/map.h>

#include "common.h"

unsigned int mem_fclk_21285 = 50000000;

EXPORT_SYMBOL(mem_fclk_21285);

static int __init early_fclk(char *arg)
{
	mem_fclk_21285 = simple_strtoul(arg, NULL, 0);
	return 0;
}

early_param("mem_fclk_21285", early_fclk);

static int __init parse_tag_memclk(const struct tag *tag)
{
	mem_fclk_21285 = tag->u.memclk.fmemclk;
	return 0;
}

__tagtable(ATAG_MEMCLK, parse_tag_memclk);

static const int fb_irq_mask[] = {
	IRQ_MASK_UART_RX,	
	IRQ_MASK_UART_TX,	
	IRQ_MASK_TIMER1,	
	IRQ_MASK_TIMER2,	
	IRQ_MASK_TIMER3,	
	IRQ_MASK_IN0,		
	IRQ_MASK_IN1,		
	IRQ_MASK_IN2,		
	IRQ_MASK_IN3,		
	IRQ_MASK_DOORBELLHOST,	
	IRQ_MASK_DMA1,		
	IRQ_MASK_DMA2,		
	IRQ_MASK_PCI,		
	IRQ_MASK_SDRAMPARITY,	
	IRQ_MASK_I2OINPOST,	
	IRQ_MASK_PCI_ABORT,	
	IRQ_MASK_PCI_SERR,	
	IRQ_MASK_DISCARD_TIMER,	
	IRQ_MASK_PCI_DPERR,	
	IRQ_MASK_PCI_PERR,	
};

static void fb_mask_irq(struct irq_data *d)
{
	*CSR_IRQ_DISABLE = fb_irq_mask[_DC21285_INR(d->irq)];
}

static void fb_unmask_irq(struct irq_data *d)
{
	*CSR_IRQ_ENABLE = fb_irq_mask[_DC21285_INR(d->irq)];
}

static struct irq_chip fb_chip = {
	.irq_ack	= fb_mask_irq,
	.irq_mask	= fb_mask_irq,
	.irq_unmask	= fb_unmask_irq,
};

static void __init __fb_init_irq(void)
{
	unsigned int irq;

	*CSR_IRQ_DISABLE = -1;
	*CSR_FIQ_DISABLE = -1;

	for (irq = _DC21285_IRQ(0); irq < _DC21285_IRQ(20); irq++) {
		irq_set_chip_and_handler(irq, &fb_chip, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
}

void __init footbridge_init_irq(void)
{
	__fb_init_irq();

	if (!footbridge_cfn_mode())
		return;

	if (machine_is_ebsa285())
		isa_init_irq(IRQ_PCI);

	if (machine_is_cats())
		isa_init_irq(IRQ_IN2);

	if (machine_is_netwinder())
		isa_init_irq(IRQ_IN3);
}

static struct map_desc fb_common_io_desc[] __initdata = {
	{
		.virtual	= ARMCSR_BASE,
		.pfn		= __phys_to_pfn(DC21285_ARMCSR_BASE),
		.length		= ARMCSR_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= XBUS_BASE,
		.pfn		= __phys_to_pfn(0x40000000),
		.length		= XBUS_SIZE,
		.type		= MT_DEVICE,
	}
};

static struct map_desc ebsa285_host_io_desc[] __initdata = {
#if defined(CONFIG_ARCH_FOOTBRIDGE) && defined(CONFIG_FOOTBRIDGE_HOST)
	{
		.virtual	= PCIMEM_BASE,
		.pfn		= __phys_to_pfn(DC21285_PCI_MEM),
		.length		= PCIMEM_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= PCICFG0_BASE,
		.pfn		= __phys_to_pfn(DC21285_PCI_TYPE_0_CONFIG),
		.length		= PCICFG0_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= PCICFG1_BASE,
		.pfn		= __phys_to_pfn(DC21285_PCI_TYPE_1_CONFIG),
		.length		= PCICFG1_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= PCIIACK_BASE,
		.pfn		= __phys_to_pfn(DC21285_PCI_IACK),
		.length		= PCIIACK_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= PCIO_BASE,
		.pfn		= __phys_to_pfn(DC21285_PCI_IO),
		.length		= PCIO_SIZE,
		.type		= MT_DEVICE,
	},
#endif
};

void __init footbridge_map_io(void)
{
	iotable_init(fb_common_io_desc, ARRAY_SIZE(fb_common_io_desc));

	if (footbridge_cfn_mode())
		iotable_init(ebsa285_host_io_desc, ARRAY_SIZE(ebsa285_host_io_desc));
}

void footbridge_restart(char mode, const char *cmd)
{
	if (mode == 's') {
		
		soft_restart(0x41000000);
	} else {
		*CSR_SA110_CNTL &= ~(1 << 13);
		*CSR_TIMER4_CNTL = TIMER_CNTL_ENABLE |
				   TIMER_CNTL_AUTORELOAD |
				   TIMER_CNTL_DIV16;
		*CSR_TIMER4_LOAD = 0x2;
		*CSR_TIMER4_CLR  = 0;
		*CSR_SA110_CNTL |= (1 << 13);
	}
}

#ifdef CONFIG_FOOTBRIDGE_ADDIN

static inline unsigned long fb_bus_sdram_offset(void)
{
	return *CSR_PCISDRAMBASE & 0xfffffff0;
}

unsigned long __virt_to_bus(unsigned long res)
{
	WARN_ON(res < PAGE_OFFSET || res >= (unsigned long)high_memory);

	return res + (fb_bus_sdram_offset() - PAGE_OFFSET);
}
EXPORT_SYMBOL(__virt_to_bus);

unsigned long __bus_to_virt(unsigned long res)
{
	res = res - (fb_bus_sdram_offset() - PAGE_OFFSET);

	WARN_ON(res < PAGE_OFFSET || res >= (unsigned long)high_memory);

	return res;
}
EXPORT_SYMBOL(__bus_to_virt);

unsigned long __pfn_to_bus(unsigned long pfn)
{
	return __pfn_to_phys(pfn) + (fb_bus_sdram_offset() - PHYS_OFFSET);
}
EXPORT_SYMBOL(__pfn_to_bus);

unsigned long __bus_to_pfn(unsigned long bus)
{
	return __phys_to_pfn(bus - (fb_bus_sdram_offset() - PHYS_OFFSET));
}
EXPORT_SYMBOL(__bus_to_pfn);

#endif
