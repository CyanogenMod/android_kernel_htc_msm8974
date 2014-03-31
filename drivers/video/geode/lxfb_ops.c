/* Geode LX framebuffer driver
 *
 * Copyright (C) 2006-2007, Advanced Micro Devices,Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/cs5535.h>

#include "lxfb.h"



static const struct {
  unsigned int pllval;
  unsigned int freq;
} pll_table[] = {
  { 0x000131AC,   6231 },
  { 0x0001215D,   6294 },
  { 0x00011087,   6750 },
  { 0x0001216C,   7081 },
  { 0x0001218D,   7140 },
  { 0x000110C9,   7800 },
  { 0x00013147,   7875 },
  { 0x000110A7,   8258 },
  { 0x00012159,   8778 },
  { 0x00014249,   8875 },
  { 0x00010057,   9000 },
  { 0x0001219A,   9472 },
  { 0x00012158,   9792 },
  { 0x00010045,  10000 },
  { 0x00010089,  10791 },
  { 0x000110E7,  11225 },
  { 0x00012136,  11430 },
  { 0x00013207,  12375 },
  { 0x00012187,  12500 },
  { 0x00014286,  14063 },
  { 0x000110E5,  15016 },
  { 0x00014214,  16250 },
  { 0x00011105,  17045 },
  { 0x000131E4,  18563 },
  { 0x00013183,  18750 },
  { 0x00014284,  19688 },
  { 0x00011104,  20400 },
  { 0x00016363,  23625 },
  { 0x000031AC,  24923 },
  { 0x0000215D,  25175 },
  { 0x00001087,  27000 },
  { 0x0000216C,  28322 },
  { 0x0000218D,  28560 },
  { 0x000010C9,  31200 },
  { 0x00003147,  31500 },
  { 0x000010A7,  33032 },
  { 0x00002159,  35112 },
  { 0x00004249,  35500 },
  { 0x00000057,  36000 },
  { 0x0000219A,  37889 },
  { 0x00002158,  39168 },
  { 0x00000045,  40000 },
  { 0x00000089,  43163 },
  { 0x000010E7,  44900 },
  { 0x00002136,  45720 },
  { 0x00003207,  49500 },
  { 0x00002187,  50000 },
  { 0x00004286,  56250 },
  { 0x000010E5,  60065 },
  { 0x00004214,  65000 },
  { 0x00001105,  68179 },
  { 0x000031E4,  74250 },
  { 0x00003183,  75000 },
  { 0x00004284,  78750 },
  { 0x00001104,  81600 },
  { 0x00006363,  94500 },
  { 0x00005303,  97520 },
  { 0x00002183, 100187 },
  { 0x00002122, 101420 },
  { 0x00001081, 108000 },
  { 0x00006201, 113310 },
  { 0x00000041, 119650 },
  { 0x000041A1, 129600 },
  { 0x00002182, 133500 },
  { 0x000041B1, 135000 },
  { 0x00000051, 144000 },
  { 0x000041E1, 148500 },
  { 0x000062D1, 157500 },
  { 0x000031A1, 162000 },
  { 0x00000061, 169203 },
  { 0x00004231, 172800 },
  { 0x00002151, 175500 },
  { 0x000052E1, 189000 },
  { 0x00000071, 192000 },
  { 0x00003201, 198000 },
  { 0x00004291, 202500 },
  { 0x00001101, 204750 },
  { 0x00007481, 218250 },
  { 0x00004170, 229500 },
  { 0x00006210, 234000 },
  { 0x00003140, 251182 },
  { 0x00006250, 261000 },
  { 0x000041C0, 278400 },
  { 0x00005220, 280640 },
  { 0x00000050, 288000 },
  { 0x000041E0, 297000 },
  { 0x00002130, 320207 }
};


static void lx_set_dotpll(u32 pllval)
{
	u32 dotpll_lo, dotpll_hi;
	int i;

	rdmsr(MSR_GLCP_DOTPLL, dotpll_lo, dotpll_hi);

	if ((dotpll_lo & MSR_GLCP_DOTPLL_LOCK) && (dotpll_hi == pllval))
		return;

	dotpll_hi = pllval;
	dotpll_lo &= ~(MSR_GLCP_DOTPLL_BYPASS | MSR_GLCP_DOTPLL_HALFPIX);
	dotpll_lo |= MSR_GLCP_DOTPLL_DOTRESET;

	wrmsr(MSR_GLCP_DOTPLL, dotpll_lo, dotpll_hi);

	

	udelay(100);

	

	for (i = 0; i < 1000; i++) {
		rdmsr(MSR_GLCP_DOTPLL, dotpll_lo, dotpll_hi);
		if (dotpll_lo & MSR_GLCP_DOTPLL_LOCK)
			break;
	}

	

	dotpll_lo &= ~MSR_GLCP_DOTPLL_DOTRESET;
	wrmsr(MSR_GLCP_DOTPLL, dotpll_lo, dotpll_hi);
}


static void lx_set_clock(struct fb_info *info)
{
	unsigned int diff, min, best = 0;
	unsigned int freq, i;

	freq = (unsigned int) (1000000000 / info->var.pixclock);

	min = abs(pll_table[0].freq - freq);

	for (i = 0; i < ARRAY_SIZE(pll_table); i++) {
		diff = abs(pll_table[i].freq - freq);
		if (diff < min) {
			min = diff;
			best = i;
		}
	}

	lx_set_dotpll(pll_table[best].pllval & 0x00017FFF);
}

static void lx_graphics_disable(struct fb_info *info)
{
	struct lxfb_par *par = info->par;
	unsigned int val, gcfg;

	

	write_vp(par, VP_A1T, 0);
	write_vp(par, VP_A2T, 0);
	write_vp(par, VP_A3T, 0);

	
	val = read_dc(par, DC_GENERAL_CFG) & ~(DC_GENERAL_CFG_VGAE |
			DC_GENERAL_CFG_VIDE);

	write_dc(par, DC_GENERAL_CFG, val);

	val = read_vp(par, VP_VCFG) & ~VP_VCFG_VID_EN;
	write_vp(par, VP_VCFG, val);

	write_dc(par, DC_IRQ, DC_IRQ_MASK | DC_IRQ_VIP_VSYNC_LOSS_IRQ_MASK |
			DC_IRQ_STATUS | DC_IRQ_VIP_VSYNC_IRQ_STATUS);

	val = read_dc(par, DC_GENLK_CTL) & ~DC_GENLK_CTL_GENLK_EN;
	write_dc(par, DC_GENLK_CTL, val);

	val = read_dc(par, DC_CLR_KEY);
	write_dc(par, DC_CLR_KEY, val & ~DC_CLR_KEY_CLR_KEY_EN);

	
	write_fp(par, FP_PM, read_fp(par, FP_PM) & ~FP_PM_P);

	val = read_vp(par, VP_MISC) | VP_MISC_DACPWRDN;
	write_vp(par, VP_MISC, val);

	

	val = read_vp(par, VP_DCFG);
	write_vp(par, VP_DCFG, val & ~(VP_DCFG_CRT_EN | VP_DCFG_HSYNC_EN |
			VP_DCFG_VSYNC_EN | VP_DCFG_DAC_BL_EN));

	gcfg = read_dc(par, DC_GENERAL_CFG);
	gcfg &= ~(DC_GENERAL_CFG_CMPE | DC_GENERAL_CFG_DECE);
	write_dc(par, DC_GENERAL_CFG, gcfg);

	
	val = read_dc(par, DC_DISPLAY_CFG);
	val &= ~DC_DISPLAY_CFG_TGEN;
	write_dc(par, DC_DISPLAY_CFG, val);

	
	udelay(1000);

	

	gcfg &= ~DC_GENERAL_CFG_DFLE;
	write_dc(par, DC_GENERAL_CFG, gcfg);

	

	do {
		val = read_gp(par, GP_BLT_STATUS);
	} while ((val & GP_BLT_STATUS_PB) || !(val & GP_BLT_STATUS_CE));
}

static void lx_graphics_enable(struct fb_info *info)
{
	struct lxfb_par *par = info->par;
	u32 temp, config;

	
	write_vp(par, VP_VRR, 0);

	

	config = read_vp(par, VP_DCFG);

	config &= ~(VP_DCFG_CRT_SYNC_SKW | VP_DCFG_PWR_SEQ_DELAY |
			VP_DCFG_CRT_HSYNC_POL | VP_DCFG_CRT_VSYNC_POL);

	config |= (VP_DCFG_CRT_SYNC_SKW_DEFAULT | VP_DCFG_PWR_SEQ_DELAY_DEFAULT
			| VP_DCFG_GV_GAM);

	if (info->var.sync & FB_SYNC_HOR_HIGH_ACT)
		config |= VP_DCFG_CRT_HSYNC_POL;

	if (info->var.sync & FB_SYNC_VERT_HIGH_ACT)
		config |= VP_DCFG_CRT_VSYNC_POL;

	if (par->output & OUTPUT_PANEL) {
		u32 msrlo, msrhi;

		write_fp(par, FP_PT1, 0);
		temp = FP_PT2_SCRC;

		if (!(info->var.sync & FB_SYNC_HOR_HIGH_ACT))
			temp |= FP_PT2_HSP;

		if (!(info->var.sync & FB_SYNC_VERT_HIGH_ACT))
			temp |= FP_PT2_VSP;

		write_fp(par, FP_PT2, temp);
		write_fp(par, FP_DFC, FP_DFC_BC);

		msrlo = MSR_LX_MSR_PADSEL_TFT_SEL_LOW;
		msrhi = MSR_LX_MSR_PADSEL_TFT_SEL_HIGH;

		wrmsr(MSR_LX_MSR_PADSEL, msrlo, msrhi);
	}

	if (par->output & OUTPUT_CRT) {
		config |= VP_DCFG_CRT_EN | VP_DCFG_HSYNC_EN |
				VP_DCFG_VSYNC_EN | VP_DCFG_DAC_BL_EN;
	}

	write_vp(par, VP_DCFG, config);

	

	if (par->output & OUTPUT_CRT) {
		temp = read_vp(par, VP_MISC);
		temp &= ~(VP_MISC_DACPWRDN | VP_MISC_APWRDN);
		write_vp(par, VP_MISC, temp);
	}

	
	if (par->output & OUTPUT_PANEL)
		write_fp(par, FP_PM, read_fp(par, FP_PM) | FP_PM_P);
}

unsigned int lx_framebuffer_size(void)
{
	unsigned int val;

	if (!cs5535_has_vsa2()) {
		uint32_t hi, lo;

		
		rdmsr(MSR_GLIU_P2D_RO0, lo, hi);

		
		val = ((hi & 0xff) << 12) | ((lo & 0xfff00000) >> 20);
		
		val -= (lo & 0x000fffff);
		val += 1;

		
		return (val << 12);
	}

	
	
	

	outw(VSA_VR_UNLOCK, VSA_VRC_INDEX);
	outw(VSA_VR_MEM_SIZE, VSA_VRC_INDEX);

	val = (unsigned int)(inw(VSA_VRC_DATA)) & 0xFE;
	return (val << 20);
}

void lx_set_mode(struct fb_info *info)
{
	struct lxfb_par *par = info->par;
	u64 msrval;

	unsigned int max, dv, val, size;

	unsigned int gcfg, dcfg;
	int hactive, hblankstart, hsyncstart, hsyncend, hblankend, htotal;
	int vactive, vblankstart, vsyncstart, vsyncend, vblankend, vtotal;

	
	write_dc(par, DC_UNLOCK, DC_UNLOCK_UNLOCK);

	lx_graphics_disable(info);

	lx_set_clock(info);

	

	rdmsrl(MSR_LX_GLD_MSR_CONFIG, msrval);
	msrval &= ~MSR_LX_GLD_MSR_CONFIG_FMT;

	if (par->output & OUTPUT_PANEL) {
		msrval |= MSR_LX_GLD_MSR_CONFIG_FMT_FP;

		if (par->output & OUTPUT_CRT)
			msrval |= MSR_LX_GLD_MSR_CONFIG_FPC;
		else
			msrval &= ~MSR_LX_GLD_MSR_CONFIG_FPC;
	} else
		msrval |= MSR_LX_GLD_MSR_CONFIG_FMT_CRT;

	wrmsrl(MSR_LX_GLD_MSR_CONFIG, msrval);

	
	

	write_dc(par, DC_FB_ST_OFFSET, 0);
	write_dc(par, DC_CB_ST_OFFSET, 0);
	write_dc(par, DC_CURS_ST_OFFSET, 0);

	
	

	val = read_dc(par, DC_GENLK_CTL);
	val &= ~(DC_GENLK_CTL_ALPHA_FLICK_EN | DC_GENLK_CTL_FLICK_EN |
			DC_GENLK_CTL_FLICK_SEL_MASK);

	

	write_dc(par, DC_GFX_SCALE, (0x4000 << 16) | 0x4000);
	write_dc(par, DC_IRQ_FILT_CTL, 0);
	write_dc(par, DC_GENLK_CTL, val);

	

	if (info->fix.line_length > 4096)
		dv = DC_DV_CTL_DV_LINE_SIZE_8K;
	else if (info->fix.line_length > 2048)
		dv = DC_DV_CTL_DV_LINE_SIZE_4K;
	else if (info->fix.line_length > 1024)
		dv = DC_DV_CTL_DV_LINE_SIZE_2K;
	else
		dv = DC_DV_CTL_DV_LINE_SIZE_1K;

	max = info->fix.line_length * info->var.yres;
	max = (max + 0x3FF) & 0xFFFFFC00;

	write_dc(par, DC_DV_TOP, max | DC_DV_TOP_DV_TOP_EN);

	val = read_dc(par, DC_DV_CTL) & ~DC_DV_CTL_DV_LINE_SIZE;
	write_dc(par, DC_DV_CTL, val | dv);

	size = info->var.xres * (info->var.bits_per_pixel >> 3);

	write_dc(par, DC_GFX_PITCH, info->fix.line_length >> 3);
	write_dc(par, DC_LINE_SIZE, (size + 7) >> 3);

	

	rdmsrl(MSR_LX_SPARE_MSR, msrval);

	msrval &= ~(MSR_LX_SPARE_MSR_DIS_CFIFO_HGO
			| MSR_LX_SPARE_MSR_VFIFO_ARB_SEL
			| MSR_LX_SPARE_MSR_LOAD_WM_LPEN_M
			| MSR_LX_SPARE_MSR_WM_LPEN_OVRD);
	msrval |= MSR_LX_SPARE_MSR_DIS_VIFO_WM |
			MSR_LX_SPARE_MSR_DIS_INIT_V_PRI;
	wrmsrl(MSR_LX_SPARE_MSR, msrval);

	gcfg = DC_GENERAL_CFG_DFLE;   
	gcfg |= (0x6 << DC_GENERAL_CFG_DFHPSL_SHIFT) | 
			(0xb << DC_GENERAL_CFG_DFHPEL_SHIFT);
	gcfg |= DC_GENERAL_CFG_FDTY;  

	dcfg  = DC_DISPLAY_CFG_VDEN;  
	dcfg |= DC_DISPLAY_CFG_GDEN;  
	dcfg |= DC_DISPLAY_CFG_TGEN;  
	dcfg |= DC_DISPLAY_CFG_TRUP;  
	dcfg |= DC_DISPLAY_CFG_PALB;  
	dcfg |= DC_DISPLAY_CFG_VISL;
	dcfg |= DC_DISPLAY_CFG_DCEN;  

	

	switch (info->var.bits_per_pixel) {
	case 8:
		dcfg |= DC_DISPLAY_CFG_DISP_MODE_8BPP;
		break;

	case 16:
		dcfg |= DC_DISPLAY_CFG_DISP_MODE_16BPP;
		break;

	case 32:
	case 24:
		dcfg |= DC_DISPLAY_CFG_DISP_MODE_24BPP;
		break;
	}

	

	hactive = info->var.xres;
	hblankstart = hactive;
	hsyncstart = hblankstart + info->var.right_margin;
	hsyncend =  hsyncstart + info->var.hsync_len;
	hblankend = hsyncend + info->var.left_margin;
	htotal = hblankend;

	vactive = info->var.yres;
	vblankstart = vactive;
	vsyncstart = vblankstart + info->var.lower_margin;
	vsyncend =  vsyncstart + info->var.vsync_len;
	vblankend = vsyncend + info->var.upper_margin;
	vtotal = vblankend;

	write_dc(par, DC_H_ACTIVE_TIMING, (hactive - 1) | ((htotal - 1) << 16));
	write_dc(par, DC_H_BLANK_TIMING,
			(hblankstart - 1) | ((hblankend - 1) << 16));
	write_dc(par, DC_H_SYNC_TIMING,
			(hsyncstart - 1) | ((hsyncend - 1) << 16));

	write_dc(par, DC_V_ACTIVE_TIMING, (vactive - 1) | ((vtotal - 1) << 16));
	write_dc(par, DC_V_BLANK_TIMING,
			(vblankstart - 1) | ((vblankend - 1) << 16));
	write_dc(par, DC_V_SYNC_TIMING,
			(vsyncstart - 1) | ((vsyncend - 1) << 16));

	write_dc(par, DC_FB_ACTIVE,
			(info->var.xres - 1) << 16 | (info->var.yres - 1));

	
	lx_graphics_enable(info);

	
	write_dc(par, DC_DISPLAY_CFG, dcfg);
	write_dc(par, DC_ARB_CFG, 0);
	write_dc(par, DC_GENERAL_CFG, gcfg);

	
	write_dc(par, DC_UNLOCK, DC_UNLOCK_LOCK);
}

void lx_set_palette_reg(struct fb_info *info, unsigned regno,
			unsigned red, unsigned green, unsigned blue)
{
	struct lxfb_par *par = info->par;
	int val;

	

	val  = (red   << 8) & 0xff0000;
	val |= (green)      & 0x00ff00;
	val |= (blue  >> 8) & 0x0000ff;

	write_dc(par, DC_PAL_ADDRESS, regno);
	write_dc(par, DC_PAL_DATA, val);
}

int lx_blank_display(struct fb_info *info, int blank_mode)
{
	struct lxfb_par *par = info->par;
	u32 dcfg, misc, fp_pm;
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

	dcfg = read_vp(par, VP_DCFG);
	dcfg &= ~(VP_DCFG_DAC_BL_EN | VP_DCFG_HSYNC_EN | VP_DCFG_VSYNC_EN |
			VP_DCFG_CRT_EN);
	if (!blank)
		dcfg |= VP_DCFG_DAC_BL_EN | VP_DCFG_CRT_EN;
	if (hsync)
		dcfg |= VP_DCFG_HSYNC_EN;
	if (vsync)
		dcfg |= VP_DCFG_VSYNC_EN;

	write_vp(par, VP_DCFG, dcfg);

	misc = read_vp(par, VP_MISC);

	if (vsync && hsync)
		misc &= ~VP_MISC_DACPWRDN;
	else
		misc |= VP_MISC_DACPWRDN;

	write_vp(par, VP_MISC, misc);

	

	if (par->output & OUTPUT_PANEL) {
		fp_pm = read_fp(par, FP_PM);
		if (blank_mode == FB_BLANK_POWERDOWN)
			fp_pm &= ~FP_PM_P;
		else
			fp_pm |= FP_PM_P;
		write_fp(par, FP_PM, fp_pm);
	}

	return 0;
}

#ifdef CONFIG_PM

static void lx_save_regs(struct lxfb_par *par)
{
	uint32_t filt;
	int i;

	
	do {
		i = read_gp(par, GP_BLT_STATUS);
	} while ((i & GP_BLT_STATUS_PB) || !(i & GP_BLT_STATUS_CE));

	
	rdmsrl(MSR_LX_MSR_PADSEL, par->msr.padsel);
	rdmsrl(MSR_GLCP_DOTPLL, par->msr.dotpll);
	rdmsrl(MSR_LX_GLD_MSR_CONFIG, par->msr.dfglcfg);
	rdmsrl(MSR_LX_SPARE_MSR, par->msr.dcspare);

	write_dc(par, DC_UNLOCK, DC_UNLOCK_UNLOCK);

	
	memcpy(par->gp, par->gp_regs, sizeof(par->gp));
	memcpy(par->dc, par->dc_regs, sizeof(par->dc));
	memcpy(par->vp, par->vp_regs, sizeof(par->vp));
	memcpy(par->fp, par->vp_regs + VP_FP_START, sizeof(par->fp));

	
	write_dc(par, DC_PAL_ADDRESS, 0);
	for (i = 0; i < ARRAY_SIZE(par->dc_pal); i++)
		par->dc_pal[i] = read_dc(par, DC_PAL_DATA);

	
	write_vp(par, VP_PAR, 0);
	for (i = 0; i < ARRAY_SIZE(par->vp_pal); i++)
		par->vp_pal[i] = read_vp(par, VP_PDR);

	
	filt = par->dc[DC_IRQ_FILT_CTL] | DC_IRQ_FILT_CTL_H_FILT_SEL;
	for (i = 0; i < ARRAY_SIZE(par->hcoeff); i += 2) {
		write_dc(par, DC_IRQ_FILT_CTL, (filt & 0xffffff00) | i);
		par->hcoeff[i] = read_dc(par, DC_FILT_COEFF1);
		par->hcoeff[i + 1] = read_dc(par, DC_FILT_COEFF2);
	}

	
	filt &= ~DC_IRQ_FILT_CTL_H_FILT_SEL;
	for (i = 0; i < ARRAY_SIZE(par->vcoeff); i++) {
		write_dc(par, DC_IRQ_FILT_CTL, (filt & 0xffffff00) | i);
		par->vcoeff[i] = read_dc(par, DC_FILT_COEFF1);
	}

	
	memcpy(par->vp_coeff, par->vp_regs + VP_VCR, sizeof(par->vp_coeff));
}

static void lx_restore_gfx_proc(struct lxfb_par *par)
{
	int i;

	
	write_gp(par, GP_RASTER_MODE, par->gp[GP_RASTER_MODE]);

	for (i = 0; i < ARRAY_SIZE(par->gp); i++) {
		switch (i) {
		case GP_RASTER_MODE:
		case GP_VECTOR_MODE:
		case GP_BLT_MODE:
		case GP_BLT_STATUS:
		case GP_HST_SRC:
			
		case GP_LUT_INDEX:
		case GP_LUT_DATA:
			
			break;

		default:
			write_gp(par, i, par->gp[i]);
		}
	}
}

static void lx_restore_display_ctlr(struct lxfb_par *par)
{
	uint32_t filt;
	int i;

	wrmsrl(MSR_LX_SPARE_MSR, par->msr.dcspare);

	for (i = 0; i < ARRAY_SIZE(par->dc); i++) {
		switch (i) {
		case DC_UNLOCK:
			
			write_dc(par, DC_UNLOCK, DC_UNLOCK_UNLOCK);
			break;

		case DC_GENERAL_CFG:
		case DC_DISPLAY_CFG:
			
			write_dc(par, i, 0);
			break;

		case DC_DV_CTL:
			
			write_dc(par, i, par->dc[i] | DC_DV_CTL_CLEAR_DV_RAM);

		case DC_RSVD_1:
		case DC_RSVD_2:
		case DC_RSVD_3:
		case DC_LINE_CNT:
		case DC_PAL_ADDRESS:
		case DC_PAL_DATA:
		case DC_DFIFO_DIAG:
		case DC_CFIFO_DIAG:
		case DC_FILT_COEFF1:
		case DC_FILT_COEFF2:
		case DC_RSVD_4:
		case DC_RSVD_5:
			
			break;

		default:
			write_dc(par, i, par->dc[i]);
		}
	}

	
	write_dc(par, DC_PAL_ADDRESS, 0);
	for (i = 0; i < ARRAY_SIZE(par->dc_pal); i++)
		write_dc(par, DC_PAL_DATA, par->dc_pal[i]);

	
	filt = par->dc[DC_IRQ_FILT_CTL] | DC_IRQ_FILT_CTL_H_FILT_SEL;
	for (i = 0; i < ARRAY_SIZE(par->hcoeff); i += 2) {
		write_dc(par, DC_IRQ_FILT_CTL, (filt & 0xffffff00) | i);
		write_dc(par, DC_FILT_COEFF1, par->hcoeff[i]);
		write_dc(par, DC_FILT_COEFF2, par->hcoeff[i + 1]);
	}

	
	filt &= ~DC_IRQ_FILT_CTL_H_FILT_SEL;
	for (i = 0; i < ARRAY_SIZE(par->vcoeff); i++) {
		write_dc(par, DC_IRQ_FILT_CTL, (filt & 0xffffff00) | i);
		write_dc(par, DC_FILT_COEFF1, par->vcoeff[i]);
	}
}

static void lx_restore_video_proc(struct lxfb_par *par)
{
	int i;

	wrmsrl(MSR_LX_GLD_MSR_CONFIG, par->msr.dfglcfg);
	wrmsrl(MSR_LX_MSR_PADSEL, par->msr.padsel);

	for (i = 0; i < ARRAY_SIZE(par->vp); i++) {
		switch (i) {
		case VP_VCFG:
		case VP_DCFG:
		case VP_PAR:
		case VP_PDR:
		case VP_CCS:
		case VP_RSVD_0:
		 
		case VP_RSVD_1:
		case VP_CRC32:
			
			break;

		default:
			write_vp(par, i, par->vp[i]);
		}
	}

	
	write_vp(par, VP_PAR, 0);
	for (i = 0; i < ARRAY_SIZE(par->vp_pal); i++)
		write_vp(par, VP_PDR, par->vp_pal[i]);

	
	memcpy(par->vp_regs + VP_VCR, par->vp_coeff, sizeof(par->vp_coeff));
}

static void lx_restore_regs(struct lxfb_par *par)
{
	int i;

	lx_set_dotpll((u32) (par->msr.dotpll >> 32));
	lx_restore_gfx_proc(par);
	lx_restore_display_ctlr(par);
	lx_restore_video_proc(par);

	
	for (i = 0; i < ARRAY_SIZE(par->fp); i++) {
		switch (i) {
		case FP_PM:
		case FP_RSVD_0:
		case FP_RSVD_1:
		case FP_RSVD_2:
		case FP_RSVD_3:
		case FP_RSVD_4:
			
			break;

		default:
			write_fp(par, i, par->fp[i]);
		}
	}

	
	if (par->fp[FP_PM] & FP_PM_P) {
		
		if (!(read_fp(par, FP_PM) &
				(FP_PM_PANEL_ON|FP_PM_PANEL_PWR_UP)))
			write_fp(par, FP_PM, par->fp[FP_PM]);
	} else {
		
		if (!(read_fp(par, FP_PM) &
				(FP_PM_PANEL_OFF|FP_PM_PANEL_PWR_DOWN)))
			write_fp(par, FP_PM, par->fp[FP_PM]);
	}

	
	write_vp(par, VP_VCFG, par->vp[VP_VCFG]);
	write_vp(par, VP_DCFG, par->vp[VP_DCFG]);
	write_dc(par, DC_DISPLAY_CFG, par->dc[DC_DISPLAY_CFG]);
	
	write_dc(par, DC_GENERAL_CFG, par->dc[DC_GENERAL_CFG]);

	
	write_dc(par, DC_UNLOCK, DC_UNLOCK_LOCK);
}

int lx_powerdown(struct fb_info *info)
{
	struct lxfb_par *par = info->par;

	if (par->powered_down)
		return 0;

	lx_save_regs(par);
	lx_graphics_disable(info);

	par->powered_down = 1;
	return 0;
}

int lx_powerup(struct fb_info *info)
{
	struct lxfb_par *par = info->par;

	if (!par->powered_down)
		return 0;

	lx_restore_regs(par);

	par->powered_down = 0;
	return 0;
}

#endif
