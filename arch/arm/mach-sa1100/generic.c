/*
 * linux/arch/arm/mach-sa1100/generic.c
 *
 * Author: Nicolas Pitre
 *
 * Code common to all SA11x0 machines.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/pm.h>
#include <linux/cpufreq.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>

#include <video/sa1100fb.h>

#include <asm/div64.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>
#include <asm/irq.h>
#include <asm/system_misc.h>

#include <mach/hardware.h>
#include <mach/irqs.h>

#include "generic.h"

unsigned int reset_status;
EXPORT_SYMBOL(reset_status);

#define NR_FREQS	16

static const unsigned short cclk_frequency_100khz[NR_FREQS] = {
	 590,	
	 737,	
	 885,	
	1032,	
	1180,	
	1327,	
	1475,	
	1622,	
	1769,	
	1917,	
	2064,	
	2212,	
	2359,	
	2507,	
	2654,	
	2802	
};

unsigned int sa11x0_freq_to_ppcr(unsigned int khz)
{
	int i;

	khz /= 100;

	for (i = 0; i < NR_FREQS; i++)
		if (cclk_frequency_100khz[i] >= khz)
			break;

	return i;
}

unsigned int sa11x0_ppcr_to_freq(unsigned int idx)
{
	unsigned int freq = 0;
	if (idx < NR_FREQS)
		freq = cclk_frequency_100khz[idx] * 100;
	return freq;
}


int sa11x0_verify_speed(struct cpufreq_policy *policy)
{
	unsigned int tmp;
	if (policy->cpu)
		return -EINVAL;

	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq, policy->cpuinfo.max_freq);

	
	tmp = cclk_frequency_100khz[sa11x0_freq_to_ppcr(policy->min)] * 100;
	if (tmp > policy->max)
		policy->max = tmp;

	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq, policy->cpuinfo.max_freq);

	return 0;
}

unsigned int sa11x0_getspeed(unsigned int cpu)
{
	if (cpu)
		return 0;
	return cclk_frequency_100khz[PPCR & 0xf] * 100;
}

static void sa1100_power_off(void)
{
	mdelay(100);
	local_irq_disable();
	
	PCFR = (PCFR_OPDE | PCFR_FP | PCFR_FS);
	
	PWER = GFER = GRER = 1;
	PSPR = 0;
	
	PMCR = PMCR_SF;
}

void sa11x0_restart(char mode, const char *cmd)
{
	if (mode == 's') {
		
		soft_restart(0);
	} else {
		
		RSRR = RSRR_SWR;
	}
}

static void sa11x0_register_device(struct platform_device *dev, void *data)
{
	int err;
	dev->dev.platform_data = data;
	err = platform_device_register(dev);
	if (err)
		printk(KERN_ERR "Unable to register device %s: %d\n",
			dev->name, err);
}


static struct resource sa11x0udc_resources[] = {
	[0] = DEFINE_RES_MEM(__PREG(Ser0UDCCR), SZ_64K),
	[1] = DEFINE_RES_IRQ(IRQ_Ser0UDC),
};

static u64 sa11x0udc_dma_mask = 0xffffffffUL;

static struct platform_device sa11x0udc_device = {
	.name		= "sa11x0-udc",
	.id		= -1,
	.dev		= {
		.dma_mask = &sa11x0udc_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(sa11x0udc_resources),
	.resource	= sa11x0udc_resources,
};

static struct resource sa11x0uart1_resources[] = {
	[0] = DEFINE_RES_MEM(__PREG(Ser1UTCR0), SZ_64K),
	[1] = DEFINE_RES_IRQ(IRQ_Ser1UART),
};

static struct platform_device sa11x0uart1_device = {
	.name		= "sa11x0-uart",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(sa11x0uart1_resources),
	.resource	= sa11x0uart1_resources,
};

static struct resource sa11x0uart3_resources[] = {
	[0] = DEFINE_RES_MEM(__PREG(Ser3UTCR0), SZ_64K),
	[1] = DEFINE_RES_IRQ(IRQ_Ser3UART),
};

static struct platform_device sa11x0uart3_device = {
	.name		= "sa11x0-uart",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(sa11x0uart3_resources),
	.resource	= sa11x0uart3_resources,
};

static struct resource sa11x0mcp_resources[] = {
	[0] = DEFINE_RES_MEM(__PREG(Ser4MCCR0), SZ_64K),
	[1] = DEFINE_RES_MEM(__PREG(Ser4MCCR1), 4),
	[2] = DEFINE_RES_IRQ(IRQ_Ser4MCP),
};

static u64 sa11x0mcp_dma_mask = 0xffffffffUL;

static struct platform_device sa11x0mcp_device = {
	.name		= "sa11x0-mcp",
	.id		= -1,
	.dev = {
		.dma_mask = &sa11x0mcp_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(sa11x0mcp_resources),
	.resource	= sa11x0mcp_resources,
};

void __init sa11x0_ppc_configure_mcp(void)
{
	
	PPDR &= ~PPC_RXD4;
	PPDR |= PPC_TXD4 | PPC_SCLK | PPC_SFRM;
	PSDR |= PPC_RXD4;
	PSDR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);
	PPSR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);
}

void sa11x0_register_mcp(struct mcp_plat_data *data)
{
	sa11x0_register_device(&sa11x0mcp_device, data);
}

static struct resource sa11x0ssp_resources[] = {
	[0] = DEFINE_RES_MEM(0x80070000, SZ_64K),
	[1] = DEFINE_RES_IRQ(IRQ_Ser4SSP),
};

static u64 sa11x0ssp_dma_mask = 0xffffffffUL;

static struct platform_device sa11x0ssp_device = {
	.name		= "sa11x0-ssp",
	.id		= -1,
	.dev = {
		.dma_mask = &sa11x0ssp_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(sa11x0ssp_resources),
	.resource	= sa11x0ssp_resources,
};

static struct resource sa11x0fb_resources[] = {
	[0] = DEFINE_RES_MEM(0xb0100000, SZ_64K),
	[1] = DEFINE_RES_IRQ(IRQ_LCD),
};

static struct platform_device sa11x0fb_device = {
	.name		= "sa11x0-fb",
	.id		= -1,
	.dev = {
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(sa11x0fb_resources),
	.resource	= sa11x0fb_resources,
};

void sa11x0_register_lcd(struct sa1100fb_mach_info *inf)
{
	sa11x0_register_device(&sa11x0fb_device, inf);
}

static struct platform_device sa11x0pcmcia_device = {
	.name		= "sa11x0-pcmcia",
	.id		= -1,
};

static struct platform_device sa11x0mtd_device = {
	.name		= "sa1100-mtd",
	.id		= -1,
};

void sa11x0_register_mtd(struct flash_platform_data *flash,
			 struct resource *res, int nr)
{
	flash->name = "sa1100";
	sa11x0mtd_device.resource = res;
	sa11x0mtd_device.num_resources = nr;
	sa11x0_register_device(&sa11x0mtd_device, flash);
}

static struct resource sa11x0ir_resources[] = {
	DEFINE_RES_MEM(__PREG(Ser2UTCR0), 0x24),
	DEFINE_RES_MEM(__PREG(Ser2HSCR0), 0x1c),
	DEFINE_RES_MEM(__PREG(Ser2HSCR2), 0x04),
	DEFINE_RES_IRQ(IRQ_Ser2ICP),
};

static struct platform_device sa11x0ir_device = {
	.name		= "sa11x0-ir",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sa11x0ir_resources),
	.resource	= sa11x0ir_resources,
};

void sa11x0_register_irda(struct irda_platform_data *irda)
{
	sa11x0_register_device(&sa11x0ir_device, irda);
}

static struct resource sa1100_rtc_resources[] = {
	DEFINE_RES_MEM(0x90010000, 0x40),
	DEFINE_RES_IRQ_NAMED(IRQ_RTC1Hz, "rtc 1Hz"),
	DEFINE_RES_IRQ_NAMED(IRQ_RTCAlrm, "rtc alarm"),
};

static struct platform_device sa11x0rtc_device = {
	.name		= "sa1100-rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sa1100_rtc_resources),
	.resource	= sa1100_rtc_resources,
};

static struct resource sa11x0dma_resources[] = {
	DEFINE_RES_MEM(DMA_PHYS, DMA_SIZE),
	DEFINE_RES_IRQ(IRQ_DMA0),
	DEFINE_RES_IRQ(IRQ_DMA1),
	DEFINE_RES_IRQ(IRQ_DMA2),
	DEFINE_RES_IRQ(IRQ_DMA3),
	DEFINE_RES_IRQ(IRQ_DMA4),
	DEFINE_RES_IRQ(IRQ_DMA5),
};

static u64 sa11x0dma_dma_mask = DMA_BIT_MASK(32);

static struct platform_device sa11x0dma_device = {
	.name		= "sa11x0-dma",
	.id		= -1,
	.dev = {
		.dma_mask = &sa11x0dma_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(sa11x0dma_resources),
	.resource	= sa11x0dma_resources,
};

static struct platform_device *sa11x0_devices[] __initdata = {
	&sa11x0udc_device,
	&sa11x0uart1_device,
	&sa11x0uart3_device,
	&sa11x0ssp_device,
	&sa11x0pcmcia_device,
	&sa11x0rtc_device,
	&sa11x0dma_device,
};

static int __init sa1100_init(void)
{
	pm_power_off = sa1100_power_off;
	return platform_add_devices(sa11x0_devices, ARRAY_SIZE(sa11x0_devices));
}

arch_initcall(sa1100_init);



static struct map_desc standard_io_desc[] __initdata = {
	{	
		.virtual	=  0xf8000000,
		.pfn		= __phys_to_pfn(0x80000000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	}, {	
		.virtual	=  0xfa000000,
		.pfn		= __phys_to_pfn(0x90000000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	}, {	
		.virtual	=  0xfc000000,
		.pfn		= __phys_to_pfn(0xa0000000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	}, {	
		.virtual	=  0xfe000000,
		.pfn		= __phys_to_pfn(0xb0000000),
		.length		= 0x00200000,
		.type		= MT_DEVICE
	},
};

void __init sa1100_map_io(void)
{
	iotable_init(standard_io_desc, ARRAY_SIZE(standard_io_desc));
}

void sa1110_mb_disable(void)
{
	unsigned long flags;

	local_irq_save(flags);
	
	PGSR &= ~GPIO_MBGNT;
	GPCR = GPIO_MBGNT;
	GPDR = (GPDR & ~GPIO_MBREQ) | GPIO_MBGNT;

	GAFR &= ~(GPIO_MBGNT | GPIO_MBREQ);

	local_irq_restore(flags);
}

void sa1110_mb_enable(void)
{
	unsigned long flags;

	local_irq_save(flags);

	PGSR &= ~GPIO_MBGNT;
	GPCR = GPIO_MBGNT;
	GPDR = (GPDR & ~GPIO_MBREQ) | GPIO_MBGNT;

	GAFR |= (GPIO_MBGNT | GPIO_MBREQ);
	TUCR |= TUCR_MR;

	local_irq_restore(flags);
}

