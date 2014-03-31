/*
 * drivers/video/geode/video_cs5530.c
 *   -- CS5530 video device
 *
 * Copyright (C) 2005 Arcom Control Systems Ltd.
 *
 * Based on AMD's original 2.4 driver:
 *   Copyright (C) 2004 Advanced Micro Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/fb.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/delay.h>

#include "geodefb.h"
#include "video_cs5530.h"

struct cs5530_pll_entry {
	long pixclock; 
	u32 pll_value;
};

static const struct cs5530_pll_entry cs5530_pll_table[] = {
	{ 39721, 0x31C45801, }, 
	{ 35308, 0x20E36802, }, 
	{ 31746, 0x33915801, }, 
	{ 27777, 0x31EC4801, }, 
	{ 26666, 0x21E22801, }, 
	{ 25000, 0x33088801, }, 
	{ 22271, 0x33E22801, }, 
	{ 20202, 0x336C4801, }, 
	{ 20000, 0x23088801, }, 
	{ 19860, 0x23088801, }, 
	{ 18518, 0x3708A801, }, 
	{ 17777, 0x23E36802, }, 
	{ 17733, 0x23E36802, }, 
	{ 17653, 0x23E36802, }, 
	{ 16949, 0x37C45801, }, 
	{ 15873, 0x23EC4801, }, 
	{ 15384, 0x37911801, }, 
	{ 14814, 0x37963803, }, 
	{ 14124, 0x37058803, }, 
	{ 13888, 0x3710C805, }, 
	{ 13333, 0x37E22801, }, 
	{ 12698, 0x27915801, }, 
	{ 12500, 0x37D8D802, }, 
	{ 11135, 0x27588802, }, 
	{ 10582, 0x27EC4802, }, 
	{ 10101, 0x27AC6803, }, 
	{ 10000, 0x27088801, }, 
	{  9259, 0x2710C805, }, 
	{  8888, 0x27E36802, }, 
	{  7692, 0x27C58803, }, 
	{  7407, 0x27316803, }, 
	{  6349, 0x2F915801, }, 
	{  6172, 0x2F08A801, }, 
	{  5714, 0x2FB11802, }, 
	{  5291, 0x2FEC4802, }, 
	{  4950, 0x2F963803, }, 
	{  4310, 0x2FB1B802, }, 
};

static void cs5530_set_dclk_frequency(struct fb_info *info)
{
	struct geodefb_par *par = info->par;
	int i;
	u32 value;
	long min, diff;

	
	value = cs5530_pll_table[0].pll_value;
	min = cs5530_pll_table[0].pixclock - info->var.pixclock;
	if (min < 0) min = -min;
	for (i = 1; i < ARRAY_SIZE(cs5530_pll_table); i++) {
		diff = cs5530_pll_table[i].pixclock - info->var.pixclock;
		if (diff < 0L) diff = -diff;
		if (diff < min) {
			min = diff;
			value = cs5530_pll_table[i].pll_value;
		}
	}

	writel(value, par->vid_regs + CS5530_DOT_CLK_CONFIG);
	writel(value | 0x80000100, par->vid_regs + CS5530_DOT_CLK_CONFIG); 
	udelay(500); 
	writel(value & 0x7FFFFFFF, par->vid_regs + CS5530_DOT_CLK_CONFIG); 
	writel(value & 0x7FFFFEFF, par->vid_regs + CS5530_DOT_CLK_CONFIG); 
}

static void cs5530_configure_display(struct fb_info *info)
{
	struct geodefb_par *par = info->par;
	u32 dcfg;

	dcfg = readl(par->vid_regs + CS5530_DISPLAY_CONFIG);

	
	dcfg &= ~(CS5530_DCFG_CRT_SYNC_SKW_MASK | CS5530_DCFG_PWR_SEQ_DLY_MASK
		  | CS5530_DCFG_CRT_HSYNC_POL   | CS5530_DCFG_CRT_VSYNC_POL
		  | CS5530_DCFG_FP_PWR_EN       | CS5530_DCFG_FP_DATA_EN
		  | CS5530_DCFG_DAC_PWR_EN      | CS5530_DCFG_VSYNC_EN
		  | CS5530_DCFG_HSYNC_EN);

	
	dcfg |= (CS5530_DCFG_CRT_SYNC_SKW_INIT | CS5530_DCFG_PWR_SEQ_DLY_INIT
		 | CS5530_DCFG_GV_PAL_BYP);

	
	if (par->enable_crt) {
		dcfg |= CS5530_DCFG_DAC_PWR_EN;
		dcfg |= CS5530_DCFG_HSYNC_EN | CS5530_DCFG_VSYNC_EN;
	}
	
	if (par->panel_x > 0) {
		dcfg |= CS5530_DCFG_FP_PWR_EN;
		dcfg |= CS5530_DCFG_FP_DATA_EN;
	}

	
	if (info->var.sync & FB_SYNC_HOR_HIGH_ACT)
		dcfg |= CS5530_DCFG_CRT_HSYNC_POL;
	if (info->var.sync & FB_SYNC_VERT_HIGH_ACT)
		dcfg |= CS5530_DCFG_CRT_VSYNC_POL;

	writel(dcfg, par->vid_regs + CS5530_DISPLAY_CONFIG);
}

static int cs5530_blank_display(struct fb_info *info, int blank_mode)
{
	struct geodefb_par *par = info->par;
	u32 dcfg;
	int blank, hsync, vsync;

	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		blank = 0; hsync = 1; vsync = 1;
		break;
	case FB_BLANK_NORMAL:
		blank = 1; hsync = 1; vsync = 1;
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		blank = 1; hsync = 1; vsync = 0;
		break;
	case FB_BLANK_HSYNC_SUSPEND:
		blank = 1; hsync = 0; vsync = 1;
		break;
	case FB_BLANK_POWERDOWN:
		blank = 1; hsync = 0; vsync = 0;
		break;
	default:
		return -EINVAL;
	}

	dcfg = readl(par->vid_regs + CS5530_DISPLAY_CONFIG);

	dcfg &= ~(CS5530_DCFG_DAC_BL_EN | CS5530_DCFG_DAC_PWR_EN
		  | CS5530_DCFG_HSYNC_EN | CS5530_DCFG_VSYNC_EN
		  | CS5530_DCFG_FP_DATA_EN | CS5530_DCFG_FP_PWR_EN);

	if (par->enable_crt) {
		if (!blank)
			dcfg |= CS5530_DCFG_DAC_BL_EN | CS5530_DCFG_DAC_PWR_EN;
		if (hsync)
			dcfg |= CS5530_DCFG_HSYNC_EN;
		if (vsync)
			dcfg |= CS5530_DCFG_VSYNC_EN;
	}
	if (par->panel_x > 0) {
		if (!blank)
			dcfg |= CS5530_DCFG_FP_DATA_EN;
		if (hsync && vsync)
			dcfg |= CS5530_DCFG_FP_PWR_EN;
	}

	writel(dcfg, par->vid_regs + CS5530_DISPLAY_CONFIG);

	return 0;
}

struct geode_vid_ops cs5530_vid_ops = {
	.set_dclk          = cs5530_set_dclk_frequency,
	.configure_display = cs5530_configure_display,
	.blank_display     = cs5530_blank_display,
};
