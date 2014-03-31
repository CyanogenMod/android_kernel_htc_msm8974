/*
 * OMAP2+ DMA driver
 *
 * Copyright (C) 2003 - 2008 Nokia Corporation
 * Author: Juha Yrjölä <juha.yrjola@nokia.com>
 * DMA channel linking for 1610 by Samuel Ortiz <samuel.ortiz@nokia.com>
 * Graphics DMA and LCD DMA graphics tranformations
 * by Imre Deak <imre.deak@nokia.com>
 * OMAP2/3 support Copyright (C) 2004-2007 Texas Instruments, Inc.
 * Some functions based on earlier dma-omap.c Copyright (C) 2001 RidgeRun, Inc.
 *
 * Copyright (C) 2009 Texas Instruments
 * Added OMAP4 support - Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
 * Converted DMA library into platform driver
 *	- G, Manjunath Kondaiah <manjugk@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>

#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/dma.h>

#define OMAP2_DMA_STRIDE	0x60

static u32 errata;
static u8 dma_stride;

static struct omap_dma_dev_attr *d;

static enum omap_reg_offsets dma_common_ch_start, dma_common_ch_end;

static u16 reg_map[] = {
	[REVISION]		= 0x00,
	[GCR]			= 0x78,
	[IRQSTATUS_L0]		= 0x08,
	[IRQSTATUS_L1]		= 0x0c,
	[IRQSTATUS_L2]		= 0x10,
	[IRQSTATUS_L3]		= 0x14,
	[IRQENABLE_L0]		= 0x18,
	[IRQENABLE_L1]		= 0x1c,
	[IRQENABLE_L2]		= 0x20,
	[IRQENABLE_L3]		= 0x24,
	[SYSSTATUS]		= 0x28,
	[OCP_SYSCONFIG]		= 0x2c,
	[CAPS_0]		= 0x64,
	[CAPS_2]		= 0x6c,
	[CAPS_3]		= 0x70,
	[CAPS_4]		= 0x74,

	
	[CCR]			= 0x80,
	[CLNK_CTRL]		= 0x84,
	[CICR]			= 0x88,
	[CSR]			= 0x8c,
	[CSDP]			= 0x90,
	[CEN]			= 0x94,
	[CFN]			= 0x98,
	[CSEI]			= 0xa4,
	[CSFI]			= 0xa8,
	[CDEI]			= 0xac,
	[CDFI]			= 0xb0,
	[CSAC]			= 0xb4,
	[CDAC]			= 0xb8,

	
	[CSSA]			= 0x9c,
	[CDSA]			= 0xa0,
	[CCEN]			= 0xbc,
	[CCFN]			= 0xc0,
	[COLOR]			= 0xc4,

	
	[CDP]			= 0xd0,
	[CNDP]			= 0xd4,
	[CCDN]			= 0xd8,
};

static void __iomem *dma_base;
static inline void dma_write(u32 val, int reg, int lch)
{
	u8  stride;
	u32 offset;

	stride = (reg >= dma_common_ch_start) ? dma_stride : 0;
	offset = reg_map[reg] + (stride * lch);
	__raw_writel(val, dma_base + offset);
}

static inline u32 dma_read(int reg, int lch)
{
	u8 stride;
	u32 offset, val;

	stride = (reg >= dma_common_ch_start) ? dma_stride : 0;
	offset = reg_map[reg] + (stride * lch);
	val = __raw_readl(dma_base + offset);
	return val;
}

static inline void omap2_disable_irq_lch(int lch)
{
	u32 val;

	val = dma_read(IRQENABLE_L0, lch);
	val &= ~(1 << lch);
	dma_write(val, IRQENABLE_L0, lch);
}

static void omap2_clear_dma(int lch)
{
	int i = dma_common_ch_start;

	for (; i <= dma_common_ch_end; i += 1)
		dma_write(0, i, lch);
}

static void omap2_show_dma_caps(void)
{
	u8 revision = dma_read(REVISION, 0) & 0xff;
	printk(KERN_INFO "OMAP DMA hardware revision %d.%d\n",
				revision >> 4, revision & 0xf);
	return;
}

static u32 configure_dma_errata(void)
{

	if (cpu_is_omap2420() || (cpu_is_omap2430() &&
				(omap_type() == OMAP2430_REV_ES1_0))) {

		SET_DMA_ERRATA(DMA_ERRATA_IFRAME_BUFFERING);
		SET_DMA_ERRATA(DMA_ERRATA_PARALLEL_CHANNELS);
	}

	if (cpu_class_is_omap2())
		SET_DMA_ERRATA(DMA_ERRATA_i378);

	if (cpu_is_omap34xx())
		SET_DMA_ERRATA(DMA_ERRATA_i541);

	if (omap_type() == OMAP3430_REV_ES1_0)
		SET_DMA_ERRATA(DMA_ERRATA_i88);

	SET_DMA_ERRATA(DMA_ERRATA_3_3);

	if (cpu_is_omap34xx() && (omap_type() != OMAP2_DEVICE_TYPE_GP))
		SET_DMA_ERRATA(DMA_ROMCODE_BUG);

	return errata;
}

static int __init omap2_system_dma_init_dev(struct omap_hwmod *oh, void *unused)
{
	struct platform_device			*pdev;
	struct omap_system_dma_plat_info	*p;
	struct resource				*mem;
	char					*name = "omap_dma_system";

	dma_stride		= OMAP2_DMA_STRIDE;
	dma_common_ch_start	= CSDP;
	if (cpu_is_omap3630() || cpu_is_omap44xx())
		dma_common_ch_end = CCDN;
	else
		dma_common_ch_end = CCFN;

	p = kzalloc(sizeof(struct omap_system_dma_plat_info), GFP_KERNEL);
	if (!p) {
		pr_err("%s: Unable to allocate pdata for %s:%s\n",
			__func__, name, oh->name);
		return -ENOMEM;
	}

	p->dma_attr		= (struct omap_dma_dev_attr *)oh->dev_attr;
	p->disable_irq_lch	= omap2_disable_irq_lch;
	p->show_dma_caps	= omap2_show_dma_caps;
	p->clear_dma		= omap2_clear_dma;
	p->dma_write		= dma_write;
	p->dma_read		= dma_read;

	p->clear_lch_regs	= NULL;

	p->errata		= configure_dma_errata();

	pdev = omap_device_build(name, 0, oh, p, sizeof(*p), NULL, 0, 0);
	kfree(p);
	if (IS_ERR(pdev)) {
		pr_err("%s: Can't build omap_device for %s:%s.\n",
			__func__, name, oh->name);
		return PTR_ERR(pdev);
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "%s: no mem resource\n", __func__);
		return -EINVAL;
	}
	dma_base = ioremap(mem->start, resource_size(mem));
	if (!dma_base) {
		dev_err(&pdev->dev, "%s: ioremap fail\n", __func__);
		return -ENOMEM;
	}

	d = oh->dev_attr;
	d->chan = kzalloc(sizeof(struct omap_dma_lch) *
					(d->lch_count), GFP_KERNEL);

	if (!d->chan) {
		dev_err(&pdev->dev, "%s: kzalloc fail\n", __func__);
		return -ENOMEM;
	}
	return 0;
}

static int __init omap2_system_dma_init(void)
{
	return omap_hwmod_for_each_by_class("dma",
			omap2_system_dma_init_dev, NULL);
}
arch_initcall(omap2_system_dma_init);
