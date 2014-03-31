/*
 * Geode GX video processor device.
 *
 *   Copyright (C) 2006 Arcom Control Systems Ltd.
 *
 *   Portions from AMD's original 2.4 driver:
 *     Copyright (C) 2004 Advanced Micro Devices, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the
 *   Free Software Foundation; either version 2 of the License, or (at your
 *   option) any later version.
 */
#include <linux/fb.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/msr.h>
#include <linux/cs5535.h>

#include "gxfb.h"


struct gx_pll_entry {
	long pixclock; 
	u32 sys_rstpll_bits;
	u32 dotpll_value;
};

#define POSTDIV3 ((u32)MSR_GLCP_SYS_RSTPLL_DOTPOSTDIV3)
#define PREMULT2 ((u32)MSR_GLCP_SYS_RSTPLL_DOTPREMULT2)
#define PREDIV2  ((u32)MSR_GLCP_SYS_RSTPLL_DOTPOSTDIV3)

static const struct gx_pll_entry gx_pll_table_48MHz[] = {
	{ 40123, POSTDIV3,	    0x00000BF2 },	
	{ 39721, 0,		    0x00000037 },	
	{ 35308, POSTDIV3|PREMULT2, 0x00000B1A },	
	{ 31746, POSTDIV3,	    0x000002D2 },	
	{ 27777, POSTDIV3|PREMULT2, 0x00000FE2 },	
	{ 26666, POSTDIV3,	    0x0000057A },	
	{ 25000, POSTDIV3,	    0x0000030A },	
	{ 22271, 0,		    0x00000063 },	
	{ 20202, 0,		    0x0000054B },	
	{ 20000, 0,		    0x0000026E },	
	{ 19860, PREMULT2,	    0x00000037 },	
	{ 18518, POSTDIV3|PREMULT2, 0x00000B0D },	
	{ 17777, 0,		    0x00000577 },	
	{ 17733, 0,		    0x000007F7 },	
	{ 17653, 0,		    0x0000057B },	
	{ 16949, PREMULT2,	    0x00000707 },	
	{ 15873, POSTDIV3|PREMULT2, 0x00000B39 },	
	{ 15384, POSTDIV3|PREMULT2, 0x00000B45 },	
	{ 14814, POSTDIV3|PREMULT2, 0x00000FC1 },	
	{ 14124, POSTDIV3,	    0x00000561 },	
	{ 13888, POSTDIV3,	    0x000007E1 },	
	{ 13426, PREMULT2,	    0x00000F4A },	
	{ 13333, 0,		    0x00000052 },	
	{ 12698, 0,		    0x00000056 },	
	{ 12500, POSTDIV3|PREMULT2, 0x00000709 },	
	{ 11135, PREMULT2,	    0x00000262 },	
	{ 10582, 0,		    0x000002D2 },	
	{ 10101, PREMULT2,	    0x00000B4A },	
	{ 10000, PREMULT2,	    0x00000036 },	
	{  9259, 0,		    0x000007E2 },	
	{  8888, 0,		    0x000007F6 },	
	{  7692, POSTDIV3|PREMULT2, 0x00000FB0 },	
	{  7407, POSTDIV3|PREMULT2, 0x00000B50 },	
	{  6349, 0,		    0x00000055 },	
	{  6172, 0,		    0x000009C1 },	
	{  5787, PREMULT2,	    0x0000002D },	
	{  5698, 0,		    0x000002C1 },	
	{  5291, 0,		    0x000002D1 },	
	{  4938, 0,		    0x00000551 },	
	{  4357, 0,		    0x0000057D },	
};

static const struct gx_pll_entry gx_pll_table_14MHz[] = {
	{ 39721, 0, 0x00000037 },	
	{ 35308, 0, 0x00000B7B },	
	{ 31746, 0, 0x000004D3 },	
	{ 27777, 0, 0x00000BE3 },	
	{ 26666, 0, 0x0000074F },	
	{ 25000, 0, 0x0000050B },	
	{ 22271, 0, 0x00000063 },	
	{ 20202, 0, 0x0000054B },	
	{ 20000, 0, 0x0000026E },	
	{ 19860, 0, 0x000007C3 },	
	{ 18518, 0, 0x000007E3 },	
	{ 17777, 0, 0x00000577 },	
	{ 17733, 0, 0x000002FB },	
	{ 17653, 0, 0x0000057B },	
	{ 16949, 0, 0x0000058B },	
	{ 15873, 0, 0x0000095E },	
	{ 15384, 0, 0x0000096A },	
	{ 14814, 0, 0x00000BC2 },	
	{ 14124, 0, 0x0000098A },	
	{ 13888, 0, 0x00000BE2 },	
	{ 13333, 0, 0x00000052 },	
	{ 12698, 0, 0x00000056 },	
	{ 12500, 0, 0x0000050A },	
	{ 11135, 0, 0x0000078E },	
	{ 10582, 0, 0x000002D2 },	
	{ 10101, 0, 0x000011F6 },	
	{ 10000, 0, 0x0000054E },	
	{  9259, 0, 0x000007E2 },	
	{  8888, 0, 0x000002FA },	
	{  7692, 0, 0x00000BB1 },	
	{  7407, 0, 0x00000975 },	
	{  6349, 0, 0x00000055 },	
	{  6172, 0, 0x000009C1 },	
	{  5698, 0, 0x000002C1 },	
	{  5291, 0, 0x00000539 },	
	{  4938, 0, 0x00000551 },	
	{  4357, 0, 0x0000057D },	
};

void gx_set_dclk_frequency(struct fb_info *info)
{
	const struct gx_pll_entry *pll_table;
	int pll_table_len;
	int i, best_i;
	long min, diff;
	u64 dotpll, sys_rstpll;
	int timeout = 1000;

	
	if (cpu_data(0).x86_mask == 1) {
		pll_table = gx_pll_table_14MHz;
		pll_table_len = ARRAY_SIZE(gx_pll_table_14MHz);
	} else {
		pll_table = gx_pll_table_48MHz;
		pll_table_len = ARRAY_SIZE(gx_pll_table_48MHz);
	}

	
	best_i = 0;
	min = abs(pll_table[0].pixclock - info->var.pixclock);
	for (i = 1; i < pll_table_len; i++) {
		diff = abs(pll_table[i].pixclock - info->var.pixclock);
		if (diff < min) {
			min = diff;
			best_i = i;
		}
	}

	rdmsrl(MSR_GLCP_SYS_RSTPLL, sys_rstpll);
	rdmsrl(MSR_GLCP_DOTPLL, dotpll);

	
	dotpll &= 0x00000000ffffffffull;
	dotpll |= (u64)pll_table[best_i].dotpll_value << 32;
	dotpll |= MSR_GLCP_DOTPLL_DOTRESET;
	dotpll &= ~MSR_GLCP_DOTPLL_BYPASS;

	wrmsrl(MSR_GLCP_DOTPLL, dotpll);

	
	sys_rstpll &= ~( MSR_GLCP_SYS_RSTPLL_DOTPREDIV2
			 | MSR_GLCP_SYS_RSTPLL_DOTPREMULT2
			 | MSR_GLCP_SYS_RSTPLL_DOTPOSTDIV3 );
	sys_rstpll |= pll_table[best_i].sys_rstpll_bits;

	wrmsrl(MSR_GLCP_SYS_RSTPLL, sys_rstpll);

	
	dotpll &= ~(MSR_GLCP_DOTPLL_DOTRESET);
	wrmsrl(MSR_GLCP_DOTPLL, dotpll);

	
	do {
		rdmsrl(MSR_GLCP_DOTPLL, dotpll);
	} while (timeout-- && !(dotpll & MSR_GLCP_DOTPLL_LOCK));
}

static void
gx_configure_tft(struct fb_info *info)
{
	struct gxfb_par *par = info->par;
	unsigned long val;
	unsigned long fp;

	

	rdmsrl(MSR_GX_MSR_PADSEL, val);
	val &= ~MSR_GX_MSR_PADSEL_MASK;
	val |= MSR_GX_MSR_PADSEL_TFT;
	wrmsrl(MSR_GX_MSR_PADSEL, val);

	

	fp = read_fp(par, FP_PM);
	fp &= ~FP_PM_P;
	write_fp(par, FP_PM, fp);

	

	fp = read_fp(par, FP_PT1);
	fp &= FP_PT1_VSIZE_MASK;
	fp |= info->var.yres << FP_PT1_VSIZE_SHIFT;
	write_fp(par, FP_PT1, fp);

	
	

	fp = 0x0F100000;

	

	if (!(info->var.sync & FB_SYNC_VERT_HIGH_ACT))
		fp |= FP_PT2_VSP;

	if (!(info->var.sync & FB_SYNC_HOR_HIGH_ACT))
		fp |= FP_PT2_HSP;

	write_fp(par, FP_PT2, fp);

	
	write_fp(par, FP_DFC, FP_DFC_NFI);

	

	fp = read_vp(par, VP_DCFG);
	fp |= VP_DCFG_FP_PWR_EN | VP_DCFG_FP_DATA_EN;
	write_vp(par, VP_DCFG, fp);

	

	fp = read_fp(par, FP_PM);
	fp |= FP_PM_P;
	write_fp(par, FP_PM, fp);
}

void gx_configure_display(struct fb_info *info)
{
	struct gxfb_par *par = info->par;
	u32 dcfg, misc;

	
	dcfg = read_vp(par, VP_DCFG);

	
	dcfg &= ~(VP_DCFG_VSYNC_EN | VP_DCFG_HSYNC_EN);
	write_vp(par, VP_DCFG, dcfg);

	
	dcfg &= ~(VP_DCFG_CRT_SYNC_SKW
		  | VP_DCFG_CRT_HSYNC_POL   | VP_DCFG_CRT_VSYNC_POL
		  | VP_DCFG_VSYNC_EN        | VP_DCFG_HSYNC_EN);

	
	dcfg |= VP_DCFG_CRT_SYNC_SKW_DEFAULT;

	
	dcfg |= VP_DCFG_HSYNC_EN | VP_DCFG_VSYNC_EN;

	misc = read_vp(par, VP_MISC);

	
	misc |= VP_MISC_GAM_EN;

	if (par->enable_crt) {

		
		misc &= ~(VP_MISC_APWRDN | VP_MISC_DACPWRDN);
		write_vp(par, VP_MISC, misc);

		if (!(info->var.sync & FB_SYNC_HOR_HIGH_ACT))
			dcfg |= VP_DCFG_CRT_HSYNC_POL;
		if (!(info->var.sync & FB_SYNC_VERT_HIGH_ACT))
			dcfg |= VP_DCFG_CRT_VSYNC_POL;
	} else {
		
		misc |= (VP_MISC_APWRDN | VP_MISC_DACPWRDN);
		write_vp(par, VP_MISC, misc);
	}

	
	

	dcfg |= VP_DCFG_CRT_EN | VP_DCFG_DAC_BL_EN;

	

	write_vp(par, VP_DCFG, dcfg);

	

	if (par->enable_crt == 0)
		gx_configure_tft(info);
}

int gx_blank_display(struct fb_info *info, int blank_mode)
{
	struct gxfb_par *par = info->par;
	u32 dcfg, fp_pm;
	int blank, hsync, vsync, crt;

	
	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		blank = 0; hsync = 1; vsync = 1; crt = 1;
		break;
	case FB_BLANK_NORMAL:
		blank = 1; hsync = 1; vsync = 1; crt = 1;
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		blank = 1; hsync = 1; vsync = 0; crt = 1;
		break;
	case FB_BLANK_HSYNC_SUSPEND:
		blank = 1; hsync = 0; vsync = 1; crt = 1;
		break;
	case FB_BLANK_POWERDOWN:
		blank = 1; hsync = 0; vsync = 0; crt = 0;
		break;
	default:
		return -EINVAL;
	}
	dcfg = read_vp(par, VP_DCFG);
	dcfg &= ~(VP_DCFG_DAC_BL_EN | VP_DCFG_HSYNC_EN | VP_DCFG_VSYNC_EN |
			VP_DCFG_CRT_EN);
	if (!blank)
		dcfg |= VP_DCFG_DAC_BL_EN;
	if (hsync)
		dcfg |= VP_DCFG_HSYNC_EN;
	if (vsync)
		dcfg |= VP_DCFG_VSYNC_EN;
	if (crt)
		dcfg |= VP_DCFG_CRT_EN;
	write_vp(par, VP_DCFG, dcfg);

	

	if (par->enable_crt == 0) {
		fp_pm = read_fp(par, FP_PM);
		if (blank_mode == FB_BLANK_POWERDOWN)
			fp_pm &= ~FP_PM_P;
		else
			fp_pm |= FP_PM_P;
		write_fp(par, FP_PM, fp_pm);
	}

	return 0;
}
