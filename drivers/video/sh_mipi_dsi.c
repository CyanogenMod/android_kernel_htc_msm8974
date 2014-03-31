/*
 * Renesas SH-mobile MIPI DSI support
 *
 * Copyright (C) 2010 Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */

#include <linux/bitmap.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/module.h>

#include <video/mipi_display.h>
#include <video/sh_mipi_dsi.h>
#include <video/sh_mobile_lcdc.h>

#include "sh_mobile_lcdcfb.h"

#define SYSCTRL		0x0000
#define SYSCONF		0x0004
#define TIMSET		0x0008
#define RESREQSET0	0x0018
#define RESREQSET1	0x001c
#define HSTTOVSET	0x0020
#define LPRTOVSET	0x0024
#define TATOVSET	0x0028
#define PRTOVSET	0x002c
#define DSICTRL		0x0030
#define DSIINTE		0x0060
#define PHYCTRL		0x0070

#define DTCTR		0x0000
#define VMCTR1		0x0020
#define VMCTR2		0x0024
#define VMLEN1		0x0028
#define VMLEN2		0x002c
#define CMTSRTREQ	0x0070
#define CMTSRTCTR	0x00d0

#define MAX_SH_MIPI_DSI 2

struct sh_mipi {
	struct sh_mobile_lcdc_entity entity;

	void __iomem	*base;
	void __iomem	*linkbase;
	struct clk	*dsit_clk;
	struct platform_device *pdev;
};

#define to_sh_mipi(e)	container_of(e, struct sh_mipi, entity)

static struct sh_mipi *mipi_dsi[MAX_SH_MIPI_DSI];

static DEFINE_MUTEX(array_lock);

static struct sh_mipi *sh_mipi_by_handle(int handle)
{
	if (handle >= ARRAY_SIZE(mipi_dsi) || handle < 0)
		return NULL;

	return mipi_dsi[handle];
}

static int sh_mipi_send_short(struct sh_mipi *mipi, u8 dsi_cmd,
			      u8 cmd, u8 param)
{
	u32 data = (dsi_cmd << 24) | (cmd << 16) | (param << 8);
	int cnt = 100;

	
	iowrite32(1 | data, mipi->linkbase + CMTSRTCTR);
	iowrite32(1, mipi->linkbase + CMTSRTREQ);

	while ((ioread32(mipi->linkbase + CMTSRTREQ) & 1) && --cnt)
		udelay(1);

	return cnt ? 0 : -ETIMEDOUT;
}

#define LCD_CHAN2MIPI(c) ((c) < LCDC_CHAN_MAINLCD || (c) > LCDC_CHAN_SUBLCD ? \
				-EINVAL : (c) - 1)

static int sh_mipi_dcs(int handle, u8 cmd)
{
	struct sh_mipi *mipi = sh_mipi_by_handle(LCD_CHAN2MIPI(handle));
	if (!mipi)
		return -ENODEV;
	return sh_mipi_send_short(mipi, MIPI_DSI_DCS_SHORT_WRITE, cmd, 0);
}

static int sh_mipi_dcs_param(int handle, u8 cmd, u8 param)
{
	struct sh_mipi *mipi = sh_mipi_by_handle(LCD_CHAN2MIPI(handle));
	if (!mipi)
		return -ENODEV;
	return sh_mipi_send_short(mipi, MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd,
				  param);
}

static void sh_mipi_dsi_enable(struct sh_mipi *mipi, bool enable)
{
	iowrite32(0x00000002 | enable, mipi->linkbase + DTCTR);
}

static void sh_mipi_shutdown(struct platform_device *pdev)
{
	struct sh_mipi *mipi = to_sh_mipi(platform_get_drvdata(pdev));

	sh_mipi_dsi_enable(mipi, false);
}

static int __init sh_mipi_setup(struct sh_mipi *mipi,
				struct sh_mipi_dsi_info *pdata)
{
	void __iomem *base = mipi->base;
	struct sh_mobile_lcdc_chan_cfg *ch = pdata->lcd_chan;
	u32 pctype, datatype, pixfmt, linelength, vmctr2;
	u32 tmp, top, bottom, delay, div;
	bool yuv;
	int bpp;

	switch (pdata->data_format) {
	case MIPI_RGB888:
		pctype = 0;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_24;
		pixfmt = MIPI_DCS_PIXEL_FMT_24BIT;
		linelength = ch->lcd_modes[0].xres * 3;
		yuv = false;
		break;
	case MIPI_RGB565:
		pctype = 1;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_16;
		pixfmt = MIPI_DCS_PIXEL_FMT_16BIT;
		linelength = ch->lcd_modes[0].xres * 2;
		yuv = false;
		break;
	case MIPI_RGB666_LP:
		pctype = 2;
		datatype = MIPI_DSI_PIXEL_STREAM_3BYTE_18;
		pixfmt = MIPI_DCS_PIXEL_FMT_24BIT;
		linelength = ch->lcd_modes[0].xres * 3;
		yuv = false;
		break;
	case MIPI_RGB666:
		pctype = 3;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_18;
		pixfmt = MIPI_DCS_PIXEL_FMT_18BIT;
		linelength = (ch->lcd_modes[0].xres * 18 + 7) / 8;
		yuv = false;
		break;
	case MIPI_BGR888:
		pctype = 8;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_24;
		pixfmt = MIPI_DCS_PIXEL_FMT_24BIT;
		linelength = ch->lcd_modes[0].xres * 3;
		yuv = false;
		break;
	case MIPI_BGR565:
		pctype = 9;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_16;
		pixfmt = MIPI_DCS_PIXEL_FMT_16BIT;
		linelength = ch->lcd_modes[0].xres * 2;
		yuv = false;
		break;
	case MIPI_BGR666_LP:
		pctype = 0xa;
		datatype = MIPI_DSI_PIXEL_STREAM_3BYTE_18;
		pixfmt = MIPI_DCS_PIXEL_FMT_24BIT;
		linelength = ch->lcd_modes[0].xres * 3;
		yuv = false;
		break;
	case MIPI_BGR666:
		pctype = 0xb;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_18;
		pixfmt = MIPI_DCS_PIXEL_FMT_18BIT;
		linelength = (ch->lcd_modes[0].xres * 18 + 7) / 8;
		yuv = false;
		break;
	case MIPI_YUYV:
		pctype = 4;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR16;
		pixfmt = MIPI_DCS_PIXEL_FMT_16BIT;
		linelength = ch->lcd_modes[0].xres * 2;
		yuv = true;
		break;
	case MIPI_UYVY:
		pctype = 5;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR16;
		pixfmt = MIPI_DCS_PIXEL_FMT_16BIT;
		linelength = ch->lcd_modes[0].xres * 2;
		yuv = true;
		break;
	case MIPI_YUV420_L:
		pctype = 6;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR12;
		pixfmt = MIPI_DCS_PIXEL_FMT_12BIT;
		linelength = (ch->lcd_modes[0].xres * 12 + 7) / 8;
		yuv = true;
		break;
	case MIPI_YUV420:
		pctype = 7;
		datatype = MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR12;
		pixfmt = MIPI_DCS_PIXEL_FMT_12BIT;
		
		linelength = (ch->lcd_modes[0].xres + 1) / 2;
		yuv = true;
		break;
	default:
		return -EINVAL;
	}

	if ((yuv && ch->interface_type != YUV422) ||
	    (!yuv && ch->interface_type != RGB24))
		return -EINVAL;

	if (!pdata->lane)
		return -EINVAL;

	
	iowrite32(0x00000001, base + SYSCTRL);
	
	udelay(50);
	iowrite32(0x00000000, base + SYSCTRL);

	

	iowrite32(0x70003332, base + TIMSET);
	
	iowrite32(0x00000000, base + RESREQSET0);
	
	iowrite32(0x00000100, base + RESREQSET1);
	
	iowrite32(0x0fffffff, base + HSTTOVSET);
	
	iowrite32(0x0fffffff, base + LPRTOVSET);
	
	iowrite32(0x0fffffff, base + TATOVSET);
	
	iowrite32(0x0fffffff, base + PRTOVSET);
	
	iowrite32(0, base + DSIINTE);
	
	iowrite32(0x00000001, base + PHYCTRL);
	udelay(200);
	
	iowrite32(0x03070001 | pdata->phyctrl, base + PHYCTRL);

	bitmap_fill((unsigned long *)&tmp, pdata->lane);
	tmp |= 0x00003700;
	iowrite32(tmp, base + SYSCONF);

	

	iowrite32(0x00000006, mipi->linkbase + DTCTR);
	
	iowrite32((ch->lcd_modes[0].vsync_len << pdata->vsynw_offset) |
		  (pdata->clksrc << 16) | (pctype << 12) | datatype,
		  mipi->linkbase + VMCTR1);

	vmctr2 = 0;
	if (pdata->flags & SH_MIPI_DSI_VSEE)
		vmctr2 |= 1 << 23;
	if (pdata->flags & SH_MIPI_DSI_HSEE)
		vmctr2 |= 1 << 22;
	if (pdata->flags & SH_MIPI_DSI_HSAE)
		vmctr2 |= 1 << 21;
	if (pdata->flags & SH_MIPI_DSI_BL2E)
		vmctr2 |= 1 << 17;
	if (pdata->flags & SH_MIPI_DSI_HSABM)
		vmctr2 |= 1 << 5;
	if (pdata->flags & SH_MIPI_DSI_HBPBM)
		vmctr2 |= 1 << 4;
	if (pdata->flags & SH_MIPI_DSI_HFPBM)
		vmctr2 |= 1 << 3;
	iowrite32(vmctr2, mipi->linkbase + VMCTR2);

	top = linelength << 16; 
	bottom = 0x00000001;
	if (pdata->flags & SH_MIPI_DSI_HSABM) 
		bottom = (pdata->lane * ch->lcd_modes[0].hsync_len) - 10;
	iowrite32(top | bottom , mipi->linkbase + VMLEN1);

	top	= 0x00010000;
	bottom	= 0x00000001;
	delay	= 0;

	div = 1;	
	if (pdata->flags & SH_MIPI_DSI_HS4divCLK)
		div = 2;

	if (pdata->flags & SH_MIPI_DSI_HFPBM) {	
		top = ch->lcd_modes[0].hsync_len + ch->lcd_modes[0].left_margin;
		top = ((pdata->lane * top / div) - 10) << 16;
	}
	if (pdata->flags & SH_MIPI_DSI_HBPBM) { 
		bottom = ch->lcd_modes[0].right_margin;
		bottom = (pdata->lane * bottom / div) - 12;
	}

	bpp = linelength / ch->lcd_modes[0].xres; 
	if ((pdata->lane / div) > bpp) {
		tmp = ch->lcd_modes[0].xres / bpp; 
		tmp = ch->lcd_modes[0].xres - tmp; 
		delay = (pdata->lane * tmp);
	}

	iowrite32(top | (bottom + delay) , mipi->linkbase + VMLEN2);

	msleep(5);

	

	
	sh_mipi_dcs(ch->chan, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(120);
	sh_mipi_dcs_param(ch->chan, MIPI_DCS_SET_ADDRESS_MODE, 0x00);
	
	sh_mipi_dcs_param(ch->chan, MIPI_DCS_SET_PIXEL_FORMAT,
			  pixfmt << 4);
	sh_mipi_dcs(ch->chan, MIPI_DCS_SET_DISPLAY_ON);

	
	iowrite32(0x00000f00, base + DSICTRL);

	return 0;
}

static int mipi_display_on(struct sh_mobile_lcdc_entity *entity)
{
	struct sh_mipi *mipi = to_sh_mipi(entity);
	struct sh_mipi_dsi_info *pdata = mipi->pdev->dev.platform_data;
	int ret;

	pm_runtime_get_sync(&mipi->pdev->dev);

	ret = pdata->set_dot_clock(mipi->pdev, mipi->base, 1);
	if (ret < 0)
		goto mipi_display_on_fail1;

	ret = sh_mipi_setup(mipi, pdata);
	if (ret < 0)
		goto mipi_display_on_fail2;

	sh_mipi_dsi_enable(mipi, true);

	return SH_MOBILE_LCDC_DISPLAY_CONNECTED;

mipi_display_on_fail1:
	pm_runtime_put_sync(&mipi->pdev->dev);
mipi_display_on_fail2:
	pdata->set_dot_clock(mipi->pdev, mipi->base, 0);

	return ret;
}

static void mipi_display_off(struct sh_mobile_lcdc_entity *entity)
{
	struct sh_mipi *mipi = to_sh_mipi(entity);
	struct sh_mipi_dsi_info *pdata = mipi->pdev->dev.platform_data;

	sh_mipi_dsi_enable(mipi, false);

	pdata->set_dot_clock(mipi->pdev, mipi->base, 0);

	pm_runtime_put_sync(&mipi->pdev->dev);
}

static const struct sh_mobile_lcdc_entity_ops mipi_ops = {
	.display_on = mipi_display_on,
	.display_off = mipi_display_off,
};

static int __init sh_mipi_probe(struct platform_device *pdev)
{
	struct sh_mipi *mipi;
	struct sh_mipi_dsi_info *pdata = pdev->dev.platform_data;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct resource *res2 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	unsigned long rate, f_current;
	int idx = pdev->id, ret;

	if (!res || !res2 || idx >= ARRAY_SIZE(mipi_dsi) || !pdata)
		return -ENODEV;

	if (!pdata->set_dot_clock)
		return -EINVAL;

	mutex_lock(&array_lock);
	if (idx < 0)
		for (idx = 0; idx < ARRAY_SIZE(mipi_dsi) && mipi_dsi[idx]; idx++)
			;

	if (idx == ARRAY_SIZE(mipi_dsi)) {
		ret = -EBUSY;
		goto efindslot;
	}

	mipi = kzalloc(sizeof(*mipi), GFP_KERNEL);
	if (!mipi) {
		ret = -ENOMEM;
		goto ealloc;
	}

	mipi->entity.owner = THIS_MODULE;
	mipi->entity.ops = &mipi_ops;

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "MIPI register region already claimed\n");
		ret = -EBUSY;
		goto ereqreg;
	}

	mipi->base = ioremap(res->start, resource_size(res));
	if (!mipi->base) {
		ret = -ENOMEM;
		goto emap;
	}

	if (!request_mem_region(res2->start, resource_size(res2), pdev->name)) {
		dev_err(&pdev->dev, "MIPI register region 2 already claimed\n");
		ret = -EBUSY;
		goto ereqreg2;
	}

	mipi->linkbase = ioremap(res2->start, resource_size(res2));
	if (!mipi->linkbase) {
		ret = -ENOMEM;
		goto emap2;
	}

	mipi->pdev = pdev;

	mipi->dsit_clk = clk_get(&pdev->dev, "dsit_clk");
	if (IS_ERR(mipi->dsit_clk)) {
		ret = PTR_ERR(mipi->dsit_clk);
		goto eclktget;
	}

	f_current = clk_get_rate(mipi->dsit_clk);
	
	rate = clk_round_rate(mipi->dsit_clk, 80000000);
	if (rate > 0 && rate != f_current)
		ret = clk_set_rate(mipi->dsit_clk, rate);
	else
		ret = rate;
	if (ret < 0)
		goto esettrate;

	dev_dbg(&pdev->dev, "DSI-T clk %lu -> %lu\n", f_current, rate);

	ret = clk_enable(mipi->dsit_clk);
	if (ret < 0)
		goto eclkton;

	mipi_dsi[idx] = mipi;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_resume(&pdev->dev);

	mutex_unlock(&array_lock);
	platform_set_drvdata(pdev, &mipi->entity);

	return 0;

eclkton:
esettrate:
	clk_put(mipi->dsit_clk);
eclktget:
	iounmap(mipi->linkbase);
emap2:
	release_mem_region(res2->start, resource_size(res2));
ereqreg2:
	iounmap(mipi->base);
emap:
	release_mem_region(res->start, resource_size(res));
ereqreg:
	kfree(mipi);
ealloc:
efindslot:
	mutex_unlock(&array_lock);

	return ret;
}

static int __exit sh_mipi_remove(struct platform_device *pdev)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct resource *res2 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	struct sh_mipi *mipi = to_sh_mipi(platform_get_drvdata(pdev));
	int i, ret;

	mutex_lock(&array_lock);

	for (i = 0; i < ARRAY_SIZE(mipi_dsi) && mipi_dsi[i] != mipi; i++)
		;

	if (i == ARRAY_SIZE(mipi_dsi)) {
		ret = -EINVAL;
	} else {
		ret = 0;
		mipi_dsi[i] = NULL;
	}

	mutex_unlock(&array_lock);

	if (ret < 0)
		return ret;

	pm_runtime_disable(&pdev->dev);
	clk_disable(mipi->dsit_clk);
	clk_put(mipi->dsit_clk);

	iounmap(mipi->linkbase);
	if (res2)
		release_mem_region(res2->start, resource_size(res2));
	iounmap(mipi->base);
	if (res)
		release_mem_region(res->start, resource_size(res));
	platform_set_drvdata(pdev, NULL);
	kfree(mipi);

	return 0;
}

static struct platform_driver sh_mipi_driver = {
	.remove		= __exit_p(sh_mipi_remove),
	.shutdown	= sh_mipi_shutdown,
	.driver = {
		.name	= "sh-mipi-dsi",
	},
};

static int __init sh_mipi_init(void)
{
	return platform_driver_probe(&sh_mipi_driver, sh_mipi_probe);
}
module_init(sh_mipi_init);

static void __exit sh_mipi_exit(void)
{
	platform_driver_unregister(&sh_mipi_driver);
}
module_exit(sh_mipi_exit);

MODULE_AUTHOR("Guennadi Liakhovetski <g.liakhovetski@gmx.de>");
MODULE_DESCRIPTION("SuperH / ARM-shmobile MIPI DSI driver");
MODULE_LICENSE("GPL v2");
