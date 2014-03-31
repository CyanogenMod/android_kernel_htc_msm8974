/*
 * isppreview.c
 *
 * TI OMAP3 ISP driver - Preview module
 *
 * Copyright (C) 2010 Nokia Corporation
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/device.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "isp.h"
#include "ispreg.h"
#include "isppreview.h"

static struct omap3isp_prev_rgbtorgb flr_rgb2rgb = {
	{	
		{0x01E2, 0x0F30, 0x0FEE},
		{0x0F9B, 0x01AC, 0x0FB9},
		{0x0FE0, 0x0EC0, 0x0260}
	},	
	{0x0000, 0x0000, 0x0000}
};

static struct omap3isp_prev_csc flr_prev_csc = {
	{	
		{66, 129, 25},
		{-38, -75, 112},
		{112, -94 , -18}
	},	
	{0x0, 0x0, 0x0}
};

#define FLR_CFA_GRADTHRS_HORZ	0x28
#define FLR_CFA_GRADTHRS_VERT	0x28

#define FLR_CSUP_GAIN		0x0D
#define FLR_CSUP_THRES		0xEB

#define FLR_NF_STRGTH		0x03

#define FLR_WBAL_DGAIN		0x100
#define FLR_WBAL_COEF		0x20

#define FLR_BLKADJ_BLUE		0x0
#define FLR_BLKADJ_GREEN	0x0
#define FLR_BLKADJ_RED		0x0

#define DEF_DETECT_CORRECT_VAL	0xe


#define PREV_MARGIN_LEFT	8
#define PREV_MARGIN_RIGHT	6
#define PREV_MARGIN_TOP		4
#define PREV_MARGIN_BOTTOM	4

#define PREV_MIN_IN_WIDTH	64
#define PREV_MIN_IN_HEIGHT	8
#define PREV_MAX_IN_HEIGHT	16384

#define PREV_MIN_OUT_WIDTH		0
#define PREV_MIN_OUT_HEIGHT		0
#define PREV_MAX_OUT_WIDTH_REV_1	1280
#define PREV_MAX_OUT_WIDTH_REV_2	3300
#define PREV_MAX_OUT_WIDTH_REV_15	4096


static u32 cfa_coef_table[] = {
#include "cfa_coef_table.h"
};

static u32 gamma_table[] = {
#include "gamma_table.h"
};

static u32 noise_filter_table[] = {
#include "noise_filter_table.h"
};

static u32 luma_enhance_table[] = {
#include "luma_enhance_table.h"
};

static void
preview_enable_invalaw(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_WIDTH | ISPPRV_PCR_INVALAW);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_WIDTH | ISPPRV_PCR_INVALAW);
}

static void
preview_enable_drkframe_capture(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DRKFCAP);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DRKFCAP);
}

static void
preview_enable_drkframe(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DRKFEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DRKFEN);
}

static void
preview_config_drkf_shadcomp(struct isp_prev_device *prev,
			     const void *scomp_shtval)
{
	struct isp_device *isp = to_isp_device(prev);
	const u32 *shtval = scomp_shtval;

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			ISPPRV_PCR_SCOMP_SFT_MASK,
			*shtval << ISPPRV_PCR_SCOMP_SFT_SHIFT);
}

static void
preview_enable_hmed(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_HMEDEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_HMEDEN);
}

static void
preview_config_hmed(struct isp_prev_device *prev, const void *prev_hmed)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_hmed *hmed = prev_hmed;

	isp_reg_writel(isp, (hmed->odddist == 1 ? 0 : ISPPRV_HMED_ODDDIST) |
		       (hmed->evendist == 1 ? 0 : ISPPRV_HMED_EVENDIST) |
		       (hmed->thres << ISPPRV_HMED_THRESHOLD_SHIFT),
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_HMED);
}

static void
preview_config_noisefilter(struct isp_prev_device *prev, const void *prev_nf)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_nf *nf = prev_nf;
	unsigned int i;

	isp_reg_writel(isp, nf->spread, OMAP3_ISP_IOMEM_PREV, ISPPRV_NF);
	isp_reg_writel(isp, ISPPRV_NF_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_NF_TBL_SIZE; i++) {
		isp_reg_writel(isp, nf->table[i],
			       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_DATA);
	}
}

static void
preview_config_dcor(struct isp_prev_device *prev, const void *prev_dcor)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_dcor *dcor = prev_dcor;

	isp_reg_writel(isp, dcor->detect_correct[0],
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CDC_THR0);
	isp_reg_writel(isp, dcor->detect_correct[1],
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CDC_THR1);
	isp_reg_writel(isp, dcor->detect_correct[2],
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CDC_THR2);
	isp_reg_writel(isp, dcor->detect_correct[3],
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CDC_THR3);
	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			ISPPRV_PCR_DCCOUP,
			dcor->couplet_mode_en ? ISPPRV_PCR_DCCOUP : 0);
}

static void
preview_config_cfa(struct isp_prev_device *prev, const void *prev_cfa)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_cfa *cfa = prev_cfa;
	unsigned int i;

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			ISPPRV_PCR_CFAFMT_MASK,
			cfa->format << ISPPRV_PCR_CFAFMT_SHIFT);

	isp_reg_writel(isp,
		(cfa->gradthrs_vert << ISPPRV_CFA_GRADTH_VER_SHIFT) |
		(cfa->gradthrs_horz << ISPPRV_CFA_GRADTH_HOR_SHIFT),
		OMAP3_ISP_IOMEM_PREV, ISPPRV_CFA);

	isp_reg_writel(isp, ISPPRV_CFA_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);

	for (i = 0; i < OMAP3ISP_PREV_CFA_TBL_SIZE; i++) {
		isp_reg_writel(isp, cfa->table[i],
			       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_DATA);
	}
}

static void
preview_config_gammacorrn(struct isp_prev_device *prev, const void *gtable)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_gtables *gt = gtable;
	unsigned int i;

	isp_reg_writel(isp, ISPPRV_REDGAMMA_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_GAMMA_TBL_SIZE; i++)
		isp_reg_writel(isp, gt->red[i], OMAP3_ISP_IOMEM_PREV,
			       ISPPRV_SET_TBL_DATA);

	isp_reg_writel(isp, ISPPRV_GREENGAMMA_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_GAMMA_TBL_SIZE; i++)
		isp_reg_writel(isp, gt->green[i], OMAP3_ISP_IOMEM_PREV,
			       ISPPRV_SET_TBL_DATA);

	isp_reg_writel(isp, ISPPRV_BLUEGAMMA_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_GAMMA_TBL_SIZE; i++)
		isp_reg_writel(isp, gt->blue[i], OMAP3_ISP_IOMEM_PREV,
			       ISPPRV_SET_TBL_DATA);
}

static void
preview_config_luma_enhancement(struct isp_prev_device *prev,
				const void *ytable)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_luma *yt = ytable;
	unsigned int i;

	isp_reg_writel(isp, ISPPRV_YENH_TABLE_ADDR,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_ADDR);
	for (i = 0; i < OMAP3ISP_PREV_YENH_TBL_SIZE; i++) {
		isp_reg_writel(isp, yt->table[i],
			       OMAP3_ISP_IOMEM_PREV, ISPPRV_SET_TBL_DATA);
	}
}

static void
preview_config_chroma_suppression(struct isp_prev_device *prev,
				  const void *csup)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_csup *cs = csup;

	isp_reg_writel(isp,
		       cs->gain | (cs->thres << ISPPRV_CSUP_THRES_SHIFT) |
		       (cs->hypf_en << ISPPRV_CSUP_HPYF_SHIFT),
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_CSUP);
}

static void
preview_enable_noisefilter(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_NFEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_NFEN);
}

static void
preview_enable_dcor(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DCOREN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_DCOREN);
}

static void
preview_enable_cfa(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_CFAEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_CFAEN);
}

static void
preview_enable_gammabypass(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_GAMMA_BYPASS);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_GAMMA_BYPASS);
}

static void
preview_enable_luma_enhancement(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_YNENHEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_YNENHEN);
}

static void
preview_enable_chroma_suppression(struct isp_prev_device *prev, u8 enable)
{
	struct isp_device *isp = to_isp_device(prev);

	if (enable)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SUPEN);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SUPEN);
}

static void
preview_config_whitebalance(struct isp_prev_device *prev, const void *prev_wbal)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_wbal *wbal = prev_wbal;
	u32 val;

	isp_reg_writel(isp, wbal->dgain, OMAP3_ISP_IOMEM_PREV, ISPPRV_WB_DGAIN);

	val = wbal->coef0 << ISPPRV_WBGAIN_COEF0_SHIFT;
	val |= wbal->coef1 << ISPPRV_WBGAIN_COEF1_SHIFT;
	val |= wbal->coef2 << ISPPRV_WBGAIN_COEF2_SHIFT;
	val |= wbal->coef3 << ISPPRV_WBGAIN_COEF3_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_WBGAIN);

	isp_reg_writel(isp,
		       ISPPRV_WBSEL_COEF0 << ISPPRV_WBSEL_N0_0_SHIFT |
		       ISPPRV_WBSEL_COEF1 << ISPPRV_WBSEL_N0_1_SHIFT |
		       ISPPRV_WBSEL_COEF0 << ISPPRV_WBSEL_N0_2_SHIFT |
		       ISPPRV_WBSEL_COEF1 << ISPPRV_WBSEL_N0_3_SHIFT |
		       ISPPRV_WBSEL_COEF2 << ISPPRV_WBSEL_N1_0_SHIFT |
		       ISPPRV_WBSEL_COEF3 << ISPPRV_WBSEL_N1_1_SHIFT |
		       ISPPRV_WBSEL_COEF2 << ISPPRV_WBSEL_N1_2_SHIFT |
		       ISPPRV_WBSEL_COEF3 << ISPPRV_WBSEL_N1_3_SHIFT |
		       ISPPRV_WBSEL_COEF0 << ISPPRV_WBSEL_N2_0_SHIFT |
		       ISPPRV_WBSEL_COEF1 << ISPPRV_WBSEL_N2_1_SHIFT |
		       ISPPRV_WBSEL_COEF0 << ISPPRV_WBSEL_N2_2_SHIFT |
		       ISPPRV_WBSEL_COEF1 << ISPPRV_WBSEL_N2_3_SHIFT |
		       ISPPRV_WBSEL_COEF2 << ISPPRV_WBSEL_N3_0_SHIFT |
		       ISPPRV_WBSEL_COEF3 << ISPPRV_WBSEL_N3_1_SHIFT |
		       ISPPRV_WBSEL_COEF2 << ISPPRV_WBSEL_N3_2_SHIFT |
		       ISPPRV_WBSEL_COEF3 << ISPPRV_WBSEL_N3_3_SHIFT,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_WBSEL);
}

static void
preview_config_blkadj(struct isp_prev_device *prev, const void *prev_blkadj)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_blkadj *blkadj = prev_blkadj;

	isp_reg_writel(isp, (blkadj->blue << ISPPRV_BLKADJOFF_B_SHIFT) |
		       (blkadj->green << ISPPRV_BLKADJOFF_G_SHIFT) |
		       (blkadj->red << ISPPRV_BLKADJOFF_R_SHIFT),
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_BLKADJOFF);
}

static void
preview_config_rgb_blending(struct isp_prev_device *prev, const void *rgb2rgb)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_rgbtorgb *rgbrgb = rgb2rgb;
	u32 val;

	val = (rgbrgb->matrix[0][0] & 0xfff) << ISPPRV_RGB_MAT1_MTX_RR_SHIFT;
	val |= (rgbrgb->matrix[0][1] & 0xfff) << ISPPRV_RGB_MAT1_MTX_GR_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT1);

	val = (rgbrgb->matrix[0][2] & 0xfff) << ISPPRV_RGB_MAT2_MTX_BR_SHIFT;
	val |= (rgbrgb->matrix[1][0] & 0xfff) << ISPPRV_RGB_MAT2_MTX_RG_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT2);

	val = (rgbrgb->matrix[1][1] & 0xfff) << ISPPRV_RGB_MAT3_MTX_GG_SHIFT;
	val |= (rgbrgb->matrix[1][2] & 0xfff) << ISPPRV_RGB_MAT3_MTX_BG_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT3);

	val = (rgbrgb->matrix[2][0] & 0xfff) << ISPPRV_RGB_MAT4_MTX_RB_SHIFT;
	val |= (rgbrgb->matrix[2][1] & 0xfff) << ISPPRV_RGB_MAT4_MTX_GB_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT4);

	val = (rgbrgb->matrix[2][2] & 0xfff) << ISPPRV_RGB_MAT5_MTX_BB_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_MAT5);

	val = (rgbrgb->offset[0] & 0x3ff) << ISPPRV_RGB_OFF1_MTX_OFFR_SHIFT;
	val |= (rgbrgb->offset[1] & 0x3ff) << ISPPRV_RGB_OFF1_MTX_OFFG_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_OFF1);

	val = (rgbrgb->offset[2] & 0x3ff) << ISPPRV_RGB_OFF2_MTX_OFFB_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_RGB_OFF2);
}

static void
preview_config_rgb_to_ycbcr(struct isp_prev_device *prev, const void *prev_csc)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_csc *csc = prev_csc;
	u32 val;

	val = (csc->matrix[0][0] & 0x3ff) << ISPPRV_CSC0_RY_SHIFT;
	val |= (csc->matrix[0][1] & 0x3ff) << ISPPRV_CSC0_GY_SHIFT;
	val |= (csc->matrix[0][2] & 0x3ff) << ISPPRV_CSC0_BY_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_CSC0);

	val = (csc->matrix[1][0] & 0x3ff) << ISPPRV_CSC1_RCB_SHIFT;
	val |= (csc->matrix[1][1] & 0x3ff) << ISPPRV_CSC1_GCB_SHIFT;
	val |= (csc->matrix[1][2] & 0x3ff) << ISPPRV_CSC1_BCB_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_CSC1);

	val = (csc->matrix[2][0] & 0x3ff) << ISPPRV_CSC2_RCR_SHIFT;
	val |= (csc->matrix[2][1] & 0x3ff) << ISPPRV_CSC2_GCR_SHIFT;
	val |= (csc->matrix[2][2] & 0x3ff) << ISPPRV_CSC2_BCR_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_CSC2);

	val = (csc->offset[0] & 0xff) << ISPPRV_CSC_OFFSET_Y_SHIFT;
	val |= (csc->offset[1] & 0xff) << ISPPRV_CSC_OFFSET_CB_SHIFT;
	val |= (csc->offset[2] & 0xff) << ISPPRV_CSC_OFFSET_CR_SHIFT;
	isp_reg_writel(isp, val, OMAP3_ISP_IOMEM_PREV, ISPPRV_CSC_OFFSET);
}

static void
preview_update_contrast(struct isp_prev_device *prev, u8 contrast)
{
	struct prev_params *params = &prev->params;

	if (params->contrast != (contrast * ISPPRV_CONTRAST_UNITS)) {
		params->contrast = contrast * ISPPRV_CONTRAST_UNITS;
		prev->update |= PREV_CONTRAST;
	}
}

static void
preview_config_contrast(struct isp_prev_device *prev, const void *params)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_CNT_BRT,
			0xff << ISPPRV_CNT_BRT_CNT_SHIFT,
			*(u8 *)params << ISPPRV_CNT_BRT_CNT_SHIFT);
}

static void
preview_update_brightness(struct isp_prev_device *prev, u8 brightness)
{
	struct prev_params *params = &prev->params;

	if (params->brightness != (brightness * ISPPRV_BRIGHT_UNITS)) {
		params->brightness = brightness * ISPPRV_BRIGHT_UNITS;
		prev->update |= PREV_BRIGHTNESS;
	}
}

static void
preview_config_brightness(struct isp_prev_device *prev, const void *params)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_CNT_BRT,
			0xff << ISPPRV_CNT_BRT_BRT_SHIFT,
			*(u8 *)params << ISPPRV_CNT_BRT_BRT_SHIFT);
}

static void
preview_config_yc_range(struct isp_prev_device *prev, const void *yclimit)
{
	struct isp_device *isp = to_isp_device(prev);
	const struct omap3isp_prev_yclimit *yc = yclimit;

	isp_reg_writel(isp,
		       yc->maxC << ISPPRV_SETUP_YC_MAXC_SHIFT |
		       yc->maxY << ISPPRV_SETUP_YC_MAXY_SHIFT |
		       yc->minC << ISPPRV_SETUP_YC_MINC_SHIFT |
		       yc->minY << ISPPRV_SETUP_YC_MINY_SHIFT,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_SETUP_YC);
}

struct preview_update {
	int cfg_bit;
	int feature_bit;
	void (*config)(struct isp_prev_device *, const void *);
	void (*enable)(struct isp_prev_device *, u8);
};

static struct preview_update update_attrs[] = {
	{OMAP3ISP_PREV_LUMAENH, PREV_LUMA_ENHANCE,
		preview_config_luma_enhancement,
		preview_enable_luma_enhancement},
	{OMAP3ISP_PREV_INVALAW, PREV_INVERSE_ALAW,
		NULL,
		preview_enable_invalaw},
	{OMAP3ISP_PREV_HRZ_MED, PREV_HORZ_MEDIAN_FILTER,
		preview_config_hmed,
		preview_enable_hmed},
	{OMAP3ISP_PREV_CFA, PREV_CFA,
		preview_config_cfa,
		preview_enable_cfa},
	{OMAP3ISP_PREV_CHROMA_SUPP, PREV_CHROMA_SUPPRESS,
		preview_config_chroma_suppression,
		preview_enable_chroma_suppression},
	{OMAP3ISP_PREV_WB, PREV_WB,
		preview_config_whitebalance,
		NULL},
	{OMAP3ISP_PREV_BLKADJ, PREV_BLKADJ,
		preview_config_blkadj,
		NULL},
	{OMAP3ISP_PREV_RGB2RGB, PREV_RGB2RGB,
		preview_config_rgb_blending,
		NULL},
	{OMAP3ISP_PREV_COLOR_CONV, PREV_COLOR_CONV,
		preview_config_rgb_to_ycbcr,
		NULL},
	{OMAP3ISP_PREV_YC_LIMIT, PREV_YCLIMITS,
		preview_config_yc_range,
		NULL},
	{OMAP3ISP_PREV_DEFECT_COR, PREV_DEFECT_COR,
		preview_config_dcor,
		preview_enable_dcor},
	{OMAP3ISP_PREV_GAMMABYPASS, PREV_GAMMA_BYPASS,
		NULL,
		preview_enable_gammabypass},
	{OMAP3ISP_PREV_DRK_FRM_CAPTURE, PREV_DARK_FRAME_CAPTURE,
		NULL,
		preview_enable_drkframe_capture},
	{OMAP3ISP_PREV_DRK_FRM_SUBTRACT, PREV_DARK_FRAME_SUBTRACT,
		NULL,
		preview_enable_drkframe},
	{OMAP3ISP_PREV_LENS_SHADING, PREV_LENS_SHADING,
		preview_config_drkf_shadcomp,
		preview_enable_drkframe},
	{OMAP3ISP_PREV_NF, PREV_NOISE_FILTER,
		preview_config_noisefilter,
		preview_enable_noisefilter},
	{OMAP3ISP_PREV_GAMMA, PREV_GAMMA,
		preview_config_gammacorrn,
		NULL},
	{-1, PREV_CONTRAST,
		preview_config_contrast,
		NULL},
	{-1, PREV_BRIGHTNESS,
		preview_config_brightness,
		NULL},
};

static u32
__preview_get_ptrs(struct prev_params *params, void **param,
		   struct omap3isp_prev_update_config *configs,
		   void __user **config, u32 bit)
{
#define CHKARG(cfgs, cfg, field)				\
	if (cfgs && cfg) {					\
		*(cfg) = (cfgs)->field;				\
	}

	switch (bit) {
	case PREV_HORZ_MEDIAN_FILTER:
		*param = &params->hmed;
		CHKARG(configs, config, hmed)
		return sizeof(params->hmed);
	case PREV_NOISE_FILTER:
		*param = &params->nf;
		CHKARG(configs, config, nf)
		return sizeof(params->nf);
		break;
	case PREV_CFA:
		*param = &params->cfa;
		CHKARG(configs, config, cfa)
		return sizeof(params->cfa);
	case PREV_LUMA_ENHANCE:
		*param = &params->luma;
		CHKARG(configs, config, luma)
		return sizeof(params->luma);
	case PREV_CHROMA_SUPPRESS:
		*param = &params->csup;
		CHKARG(configs, config, csup)
		return sizeof(params->csup);
	case PREV_DEFECT_COR:
		*param = &params->dcor;
		CHKARG(configs, config, dcor)
		return sizeof(params->dcor);
	case PREV_BLKADJ:
		*param = &params->blk_adj;
		CHKARG(configs, config, blkadj)
		return sizeof(params->blk_adj);
	case PREV_YCLIMITS:
		*param = &params->yclimit;
		CHKARG(configs, config, yclimit)
		return sizeof(params->yclimit);
	case PREV_RGB2RGB:
		*param = &params->rgb2rgb;
		CHKARG(configs, config, rgb2rgb)
		return sizeof(params->rgb2rgb);
	case PREV_COLOR_CONV:
		*param = &params->rgb2ycbcr;
		CHKARG(configs, config, csc)
		return sizeof(params->rgb2ycbcr);
	case PREV_WB:
		*param = &params->wbal;
		CHKARG(configs, config, wbal)
		return sizeof(params->wbal);
	case PREV_GAMMA:
		*param = &params->gamma;
		CHKARG(configs, config, gamma)
		return sizeof(params->gamma);
	case PREV_CONTRAST:
		*param = &params->contrast;
		return 0;
	case PREV_BRIGHTNESS:
		*param = &params->brightness;
		return 0;
	default:
		*param = NULL;
		*config = NULL;
		break;
	}
	return 0;
}

static int preview_config(struct isp_prev_device *prev,
			  struct omap3isp_prev_update_config *cfg)
{
	struct prev_params *params;
	struct preview_update *attr;
	int i, bit, rval = 0;

	params = &prev->params;

	if (prev->state != ISP_PIPELINE_STREAM_STOPPED) {
		unsigned long flags;

		spin_lock_irqsave(&prev->lock, flags);
		prev->shadow_update = 1;
		spin_unlock_irqrestore(&prev->lock, flags);
	}

	for (i = 0; i < ARRAY_SIZE(update_attrs); i++) {
		attr = &update_attrs[i];
		bit = 0;

		if (!(cfg->update & attr->cfg_bit))
			continue;

		bit = cfg->flag & attr->cfg_bit;
		if (bit) {
			void *to = NULL, __user *from = NULL;
			unsigned long sz = 0;

			sz = __preview_get_ptrs(params, &to, cfg, &from,
						   bit);
			if (to && from && sz) {
				if (copy_from_user(to, from, sz)) {
					rval = -EFAULT;
					break;
				}
			}
			params->features |= attr->feature_bit;
		} else {
			params->features &= ~attr->feature_bit;
		}

		prev->update |= attr->feature_bit;
	}

	prev->shadow_update = 0;
	return rval;
}

static void preview_setup_hw(struct isp_prev_device *prev)
{
	struct prev_params *params = &prev->params;
	struct preview_update *attr;
	int i, bit;
	void *param_ptr;

	for (i = 0; i < ARRAY_SIZE(update_attrs); i++) {
		attr = &update_attrs[i];

		if (!(prev->update & attr->feature_bit))
			continue;
		bit = params->features & attr->feature_bit;
		if (bit) {
			if (attr->config) {
				__preview_get_ptrs(params, &param_ptr, NULL,
						      NULL, bit);
				attr->config(prev, param_ptr);
			}
			if (attr->enable)
				attr->enable(prev, 1);
		} else
			if (attr->enable)
				attr->enable(prev, 0);

		prev->update &= ~attr->feature_bit;
	}
}

static void
preview_config_ycpos(struct isp_prev_device *prev,
		     enum v4l2_mbus_pixelcode pixelcode)
{
	struct isp_device *isp = to_isp_device(prev);
	enum preview_ycpos_mode mode;

	switch (pixelcode) {
	case V4L2_MBUS_FMT_YUYV8_1X16:
		mode = YCPOS_CrYCbY;
		break;
	case V4L2_MBUS_FMT_UYVY8_1X16:
		mode = YCPOS_YCrYCb;
		break;
	default:
		return;
	}

	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			ISPPRV_PCR_YCPOS_CrYCbY,
			mode << ISPPRV_PCR_YCPOS_SHIFT);
}

static void preview_config_averager(struct isp_prev_device *prev, u8 average)
{
	struct isp_device *isp = to_isp_device(prev);
	int reg = 0;

	if (prev->params.cfa.format == OMAP3ISP_CFAFMT_BAYER)
		reg = ISPPRV_AVE_EVENDIST_2 << ISPPRV_AVE_EVENDIST_SHIFT |
		      ISPPRV_AVE_ODDDIST_2 << ISPPRV_AVE_ODDDIST_SHIFT |
		      average;
	else if (prev->params.cfa.format == OMAP3ISP_CFAFMT_RGBFOVEON)
		reg = ISPPRV_AVE_EVENDIST_3 << ISPPRV_AVE_EVENDIST_SHIFT |
		      ISPPRV_AVE_ODDDIST_3 << ISPPRV_AVE_ODDDIST_SHIFT |
		      average;
	isp_reg_writel(isp, reg, OMAP3_ISP_IOMEM_PREV, ISPPRV_AVE);
}

static void preview_config_input_size(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);
	struct prev_params *params = &prev->params;
	unsigned int sph = prev->crop.left;
	unsigned int eph = prev->crop.left + prev->crop.width - 1;
	unsigned int slv = prev->crop.top;
	unsigned int elv = prev->crop.top + prev->crop.height - 1;

	if (params->features & PREV_CFA) {
		sph -= 2;
		eph += 2;
		slv -= 2;
		elv += 2;
	}
	if (params->features & (PREV_DEFECT_COR | PREV_NOISE_FILTER)) {
		sph -= 2;
		eph += 2;
		slv -= 2;
		elv += 2;
	}
	if (params->features & PREV_HORZ_MEDIAN_FILTER) {
		sph -= 2;
		eph += 2;
	}
	if (params->features & (PREV_CHROMA_SUPPRESS | PREV_LUMA_ENHANCE))
		sph -= 2;

	isp_reg_writel(isp, (sph << ISPPRV_HORZ_INFO_SPH_SHIFT) | eph,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_HORZ_INFO);
	isp_reg_writel(isp, (slv << ISPPRV_VERT_INFO_SLV_SHIFT) | elv,
		       OMAP3_ISP_IOMEM_PREV, ISPPRV_VERT_INFO);
}

static void
preview_config_inlineoffset(struct isp_prev_device *prev, u32 offset)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_writel(isp, offset & 0xffff, OMAP3_ISP_IOMEM_PREV,
		       ISPPRV_RADR_OFFSET);
}

static void preview_set_inaddr(struct isp_prev_device *prev, u32 addr)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_writel(isp, addr, OMAP3_ISP_IOMEM_PREV, ISPPRV_RSDR_ADDR);
}

static void preview_config_outlineoffset(struct isp_prev_device *prev,
				    u32 offset)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_writel(isp, offset & 0xffff, OMAP3_ISP_IOMEM_PREV,
		       ISPPRV_WADD_OFFSET);
}

/*
 * preview_set_outaddr - Sets the memory address to store output frame
 * @addr: 32bit memory address aligned on 32byte boundary.
 *
 * Configures the memory address to which the output frame is written.
 */
static void preview_set_outaddr(struct isp_prev_device *prev, u32 addr)
{
	struct isp_device *isp = to_isp_device(prev);

	isp_reg_writel(isp, addr, OMAP3_ISP_IOMEM_PREV, ISPPRV_WSDR_ADDR);
}

static void preview_adjust_bandwidth(struct isp_prev_device *prev)
{
	struct isp_pipeline *pipe = to_isp_pipeline(&prev->subdev.entity);
	struct isp_device *isp = to_isp_device(prev);
	const struct v4l2_mbus_framefmt *ifmt = &prev->formats[PREV_PAD_SINK];
	unsigned long l3_ick = pipe->l3_ick;
	struct v4l2_fract *timeperframe;
	unsigned int cycles_per_frame;
	unsigned int requests_per_frame;
	unsigned int cycles_per_request;
	unsigned int minimum;
	unsigned int maximum;
	unsigned int value;

	if (prev->input != PREVIEW_INPUT_MEMORY) {
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_SBL, ISPSBL_SDR_REQ_EXP,
			    ISPSBL_SDR_REQ_PRV_EXP_MASK);
		return;
	}

	cycles_per_request = div_u64((u64)l3_ick / 2 * 256 + pipe->max_rate - 1,
				     pipe->max_rate);
	minimum = DIV_ROUND_UP(cycles_per_request, 32);

	timeperframe = &pipe->max_timeperframe;

	requests_per_frame = DIV_ROUND_UP(ifmt->width * 2, 256) * ifmt->height;
	cycles_per_frame = div_u64((u64)l3_ick * timeperframe->numerator,
				   timeperframe->denominator);
	cycles_per_request = cycles_per_frame / requests_per_frame;

	maximum = cycles_per_request / 32;

	value = max(minimum, maximum);

	dev_dbg(isp->dev, "%s: cycles per request = %u\n", __func__, value);
	isp_reg_clr_set(isp, OMAP3_ISP_IOMEM_SBL, ISPSBL_SDR_REQ_EXP,
			ISPSBL_SDR_REQ_PRV_EXP_MASK,
			value << ISPSBL_SDR_REQ_PRV_EXP_SHIFT);
}

int omap3isp_preview_busy(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);

	return isp_reg_readl(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR)
		& ISPPRV_PCR_BUSY;
}

void omap3isp_preview_restore_context(struct isp_device *isp)
{
	isp->isp_prev.update = PREV_FEATURES_END - 1;
	preview_setup_hw(&isp->isp_prev);
}

#define PREV_PRINT_REGISTER(isp, name)\
	dev_dbg(isp->dev, "###PRV " #name "=0x%08x\n", \
		isp_reg_readl(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_##name))

static void preview_print_status(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);

	dev_dbg(isp->dev, "-------------Preview Register dump----------\n");

	PREV_PRINT_REGISTER(isp, PCR);
	PREV_PRINT_REGISTER(isp, HORZ_INFO);
	PREV_PRINT_REGISTER(isp, VERT_INFO);
	PREV_PRINT_REGISTER(isp, RSDR_ADDR);
	PREV_PRINT_REGISTER(isp, RADR_OFFSET);
	PREV_PRINT_REGISTER(isp, DSDR_ADDR);
	PREV_PRINT_REGISTER(isp, DRKF_OFFSET);
	PREV_PRINT_REGISTER(isp, WSDR_ADDR);
	PREV_PRINT_REGISTER(isp, WADD_OFFSET);
	PREV_PRINT_REGISTER(isp, AVE);
	PREV_PRINT_REGISTER(isp, HMED);
	PREV_PRINT_REGISTER(isp, NF);
	PREV_PRINT_REGISTER(isp, WB_DGAIN);
	PREV_PRINT_REGISTER(isp, WBGAIN);
	PREV_PRINT_REGISTER(isp, WBSEL);
	PREV_PRINT_REGISTER(isp, CFA);
	PREV_PRINT_REGISTER(isp, BLKADJOFF);
	PREV_PRINT_REGISTER(isp, RGB_MAT1);
	PREV_PRINT_REGISTER(isp, RGB_MAT2);
	PREV_PRINT_REGISTER(isp, RGB_MAT3);
	PREV_PRINT_REGISTER(isp, RGB_MAT4);
	PREV_PRINT_REGISTER(isp, RGB_MAT5);
	PREV_PRINT_REGISTER(isp, RGB_OFF1);
	PREV_PRINT_REGISTER(isp, RGB_OFF2);
	PREV_PRINT_REGISTER(isp, CSC0);
	PREV_PRINT_REGISTER(isp, CSC1);
	PREV_PRINT_REGISTER(isp, CSC2);
	PREV_PRINT_REGISTER(isp, CSC_OFFSET);
	PREV_PRINT_REGISTER(isp, CNT_BRT);
	PREV_PRINT_REGISTER(isp, CSUP);
	PREV_PRINT_REGISTER(isp, SETUP_YC);
	PREV_PRINT_REGISTER(isp, SET_TBL_ADDR);
	PREV_PRINT_REGISTER(isp, CDC_THR0);
	PREV_PRINT_REGISTER(isp, CDC_THR1);
	PREV_PRINT_REGISTER(isp, CDC_THR2);
	PREV_PRINT_REGISTER(isp, CDC_THR3);

	dev_dbg(isp->dev, "--------------------------------------------\n");
}

static void preview_init_params(struct isp_prev_device *prev)
{
	struct prev_params *params = &prev->params;
	int i = 0;

	
	params->contrast = ISPPRV_CONTRAST_DEF * ISPPRV_CONTRAST_UNITS;
	params->brightness = ISPPRV_BRIGHT_DEF * ISPPRV_BRIGHT_UNITS;
	params->cfa.format = OMAP3ISP_CFAFMT_BAYER;
	memcpy(params->cfa.table, cfa_coef_table,
	       sizeof(params->cfa.table));
	params->cfa.gradthrs_horz = FLR_CFA_GRADTHRS_HORZ;
	params->cfa.gradthrs_vert = FLR_CFA_GRADTHRS_VERT;
	params->csup.gain = FLR_CSUP_GAIN;
	params->csup.thres = FLR_CSUP_THRES;
	params->csup.hypf_en = 0;
	memcpy(params->luma.table, luma_enhance_table,
	       sizeof(params->luma.table));
	params->nf.spread = FLR_NF_STRGTH;
	memcpy(params->nf.table, noise_filter_table, sizeof(params->nf.table));
	params->dcor.couplet_mode_en = 1;
	for (i = 0; i < OMAP3ISP_PREV_DETECT_CORRECT_CHANNELS; i++)
		params->dcor.detect_correct[i] = DEF_DETECT_CORRECT_VAL;
	memcpy(params->gamma.blue, gamma_table, sizeof(params->gamma.blue));
	memcpy(params->gamma.green, gamma_table, sizeof(params->gamma.green));
	memcpy(params->gamma.red, gamma_table, sizeof(params->gamma.red));
	params->wbal.dgain = FLR_WBAL_DGAIN;
	params->wbal.coef0 = FLR_WBAL_COEF;
	params->wbal.coef1 = FLR_WBAL_COEF;
	params->wbal.coef2 = FLR_WBAL_COEF;
	params->wbal.coef3 = FLR_WBAL_COEF;
	params->blk_adj.red = FLR_BLKADJ_RED;
	params->blk_adj.green = FLR_BLKADJ_GREEN;
	params->blk_adj.blue = FLR_BLKADJ_BLUE;
	params->rgb2rgb = flr_rgb2rgb;
	params->rgb2ycbcr = flr_prev_csc;
	params->yclimit.minC = ISPPRV_YC_MIN;
	params->yclimit.maxC = ISPPRV_YC_MAX;
	params->yclimit.minY = ISPPRV_YC_MIN;
	params->yclimit.maxY = ISPPRV_YC_MAX;

	params->features = PREV_CFA | PREV_DEFECT_COR | PREV_NOISE_FILTER
			 | PREV_GAMMA | PREV_BLKADJ | PREV_YCLIMITS
			 | PREV_RGB2RGB | PREV_COLOR_CONV | PREV_WB
			 | PREV_BRIGHTNESS | PREV_CONTRAST;

	prev->update = PREV_FEATURES_END - 1;
}

static unsigned int preview_max_out_width(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);

	switch (isp->revision) {
	case ISP_REVISION_1_0:
		return PREV_MAX_OUT_WIDTH_REV_1;

	case ISP_REVISION_2_0:
	default:
		return PREV_MAX_OUT_WIDTH_REV_2;

	case ISP_REVISION_15_0:
		return PREV_MAX_OUT_WIDTH_REV_15;
	}
}

static void preview_configure(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);
	struct v4l2_mbus_framefmt *format;

	preview_setup_hw(prev);

	if (prev->output & PREVIEW_OUTPUT_MEMORY)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SDRPORT);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SDRPORT);

	if (prev->output & PREVIEW_OUTPUT_RESIZER)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_RSZPORT);
	else
		isp_reg_clr(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_RSZPORT);

	
	format = &prev->formats[PREV_PAD_SINK];

	preview_adjust_bandwidth(prev);

	preview_config_input_size(prev);

	if (prev->input == PREVIEW_INPUT_CCDC)
		preview_config_inlineoffset(prev, 0);
	else
		preview_config_inlineoffset(prev,
				ALIGN(format->width, 0x20) * 2);

	
	format = &prev->formats[PREV_PAD_SOURCE];

	if (prev->output & PREVIEW_OUTPUT_MEMORY)
		preview_config_outlineoffset(prev,
				ALIGN(format->width, 0x10) * 2);

	preview_config_averager(prev, 0);
	preview_config_ycpos(prev, format->code);
}


static void preview_enable_oneshot(struct isp_prev_device *prev)
{
	struct isp_device *isp = to_isp_device(prev);

	if (prev->input == PREVIEW_INPUT_MEMORY)
		isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
			    ISPPRV_PCR_SOURCE);

	isp_reg_set(isp, OMAP3_ISP_IOMEM_PREV, ISPPRV_PCR,
		    ISPPRV_PCR_EN | ISPPRV_PCR_ONESHOT);
}

void omap3isp_preview_isr_frame_sync(struct isp_prev_device *prev)
{
	if (prev->state == ISP_PIPELINE_STREAM_CONTINUOUS &&
	    prev->video_out.dmaqueue_flags & ISP_VIDEO_DMAQUEUE_QUEUED) {
		preview_enable_oneshot(prev);
		isp_video_dmaqueue_flags_clr(&prev->video_out);
	}
}

static void preview_isr_buffer(struct isp_prev_device *prev)
{
	struct isp_pipeline *pipe = to_isp_pipeline(&prev->subdev.entity);
	struct isp_buffer *buffer;
	int restart = 0;

	if (prev->input == PREVIEW_INPUT_MEMORY) {
		buffer = omap3isp_video_buffer_next(&prev->video_in);
		if (buffer != NULL)
			preview_set_inaddr(prev, buffer->isp_addr);
		pipe->state |= ISP_PIPELINE_IDLE_INPUT;
	}

	if (prev->output & PREVIEW_OUTPUT_MEMORY) {
		buffer = omap3isp_video_buffer_next(&prev->video_out);
		if (buffer != NULL) {
			preview_set_outaddr(prev, buffer->isp_addr);
			restart = 1;
		}
		pipe->state |= ISP_PIPELINE_IDLE_OUTPUT;
	}

	switch (prev->state) {
	case ISP_PIPELINE_STREAM_SINGLESHOT:
		if (isp_pipeline_ready(pipe))
			omap3isp_pipeline_set_stream(pipe,
						ISP_PIPELINE_STREAM_SINGLESHOT);
		break;

	case ISP_PIPELINE_STREAM_CONTINUOUS:
		if (restart)
			preview_enable_oneshot(prev);
		break;

	case ISP_PIPELINE_STREAM_STOPPED:
	default:
		return;
	}
}

void omap3isp_preview_isr(struct isp_prev_device *prev)
{
	unsigned long flags;

	if (omap3isp_module_sync_is_stopping(&prev->wait, &prev->stopping))
		return;

	spin_lock_irqsave(&prev->lock, flags);
	if (prev->shadow_update)
		goto done;

	preview_setup_hw(prev);
	preview_config_input_size(prev);

done:
	spin_unlock_irqrestore(&prev->lock, flags);

	if (prev->input == PREVIEW_INPUT_MEMORY ||
	    prev->output & PREVIEW_OUTPUT_MEMORY)
		preview_isr_buffer(prev);
	else if (prev->state == ISP_PIPELINE_STREAM_CONTINUOUS)
		preview_enable_oneshot(prev);
}


static int preview_video_queue(struct isp_video *video,
			       struct isp_buffer *buffer)
{
	struct isp_prev_device *prev = &video->isp->isp_prev;

	if (video->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		preview_set_inaddr(prev, buffer->isp_addr);

	if (video->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		preview_set_outaddr(prev, buffer->isp_addr);

	return 0;
}

static const struct isp_video_operations preview_video_ops = {
	.queue = preview_video_queue,
};


static int preview_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct isp_prev_device *prev =
		container_of(ctrl->handler, struct isp_prev_device, ctrls);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		preview_update_brightness(prev, ctrl->val);
		break;
	case V4L2_CID_CONTRAST:
		preview_update_contrast(prev, ctrl->val);
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops preview_ctrl_ops = {
	.s_ctrl = preview_s_ctrl,
};

static long preview_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_OMAP3ISP_PRV_CFG:
		return preview_config(prev, arg);

	default:
		return -ENOIOCTLCMD;
	}
}

static int preview_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);
	struct isp_video *video_out = &prev->video_out;
	struct isp_device *isp = to_isp_device(prev);
	struct device *dev = to_device(prev);
	unsigned long flags;

	if (prev->state == ISP_PIPELINE_STREAM_STOPPED) {
		if (enable == ISP_PIPELINE_STREAM_STOPPED)
			return 0;

		omap3isp_subclk_enable(isp, OMAP3_ISP_SUBCLK_PREVIEW);
		preview_configure(prev);
		atomic_set(&prev->stopping, 0);
		preview_print_status(prev);
	}

	switch (enable) {
	case ISP_PIPELINE_STREAM_CONTINUOUS:
		if (prev->output & PREVIEW_OUTPUT_MEMORY)
			omap3isp_sbl_enable(isp, OMAP3_ISP_SBL_PREVIEW_WRITE);

		if (video_out->dmaqueue_flags & ISP_VIDEO_DMAQUEUE_QUEUED ||
		    !(prev->output & PREVIEW_OUTPUT_MEMORY))
			preview_enable_oneshot(prev);

		isp_video_dmaqueue_flags_clr(video_out);
		break;

	case ISP_PIPELINE_STREAM_SINGLESHOT:
		if (prev->input == PREVIEW_INPUT_MEMORY)
			omap3isp_sbl_enable(isp, OMAP3_ISP_SBL_PREVIEW_READ);
		if (prev->output & PREVIEW_OUTPUT_MEMORY)
			omap3isp_sbl_enable(isp, OMAP3_ISP_SBL_PREVIEW_WRITE);

		preview_enable_oneshot(prev);
		break;

	case ISP_PIPELINE_STREAM_STOPPED:
		if (omap3isp_module_sync_idle(&sd->entity, &prev->wait,
					      &prev->stopping))
			dev_dbg(dev, "%s: stop timeout.\n", sd->name);
		spin_lock_irqsave(&prev->lock, flags);
		omap3isp_sbl_disable(isp, OMAP3_ISP_SBL_PREVIEW_READ);
		omap3isp_sbl_disable(isp, OMAP3_ISP_SBL_PREVIEW_WRITE);
		omap3isp_subclk_disable(isp, OMAP3_ISP_SUBCLK_PREVIEW);
		spin_unlock_irqrestore(&prev->lock, flags);
		isp_video_dmaqueue_flags_clr(video_out);
		break;
	}

	prev->state = enable;
	return 0;
}

static struct v4l2_mbus_framefmt *
__preview_get_format(struct isp_prev_device *prev, struct v4l2_subdev_fh *fh,
		     unsigned int pad, enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &prev->formats[pad];
}

static struct v4l2_rect *
__preview_get_crop(struct isp_prev_device *prev, struct v4l2_subdev_fh *fh,
		   enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_crop(fh, PREV_PAD_SINK);
	else
		return &prev->crop;
}

static const unsigned int preview_input_fmts[] = {
	V4L2_MBUS_FMT_SGRBG10_1X10,
	V4L2_MBUS_FMT_SRGGB10_1X10,
	V4L2_MBUS_FMT_SBGGR10_1X10,
	V4L2_MBUS_FMT_SGBRG10_1X10,
};

static const unsigned int preview_output_fmts[] = {
	V4L2_MBUS_FMT_UYVY8_1X16,
	V4L2_MBUS_FMT_YUYV8_1X16,
};

static void preview_try_format(struct isp_prev_device *prev,
			       struct v4l2_subdev_fh *fh, unsigned int pad,
			       struct v4l2_mbus_framefmt *fmt,
			       enum v4l2_subdev_format_whence which)
{
	enum v4l2_mbus_pixelcode pixelcode;
	struct v4l2_rect *crop;
	unsigned int i;

	switch (pad) {
	case PREV_PAD_SINK:
		if (prev->input == PREVIEW_INPUT_MEMORY) {
			fmt->width = clamp_t(u32, fmt->width, PREV_MIN_IN_WIDTH,
					     preview_max_out_width(prev));
			fmt->height = clamp_t(u32, fmt->height,
					      PREV_MIN_IN_HEIGHT,
					      PREV_MAX_IN_HEIGHT);
		}

		fmt->colorspace = V4L2_COLORSPACE_SRGB;

		for (i = 0; i < ARRAY_SIZE(preview_input_fmts); i++) {
			if (fmt->code == preview_input_fmts[i])
				break;
		}

		
		if (i >= ARRAY_SIZE(preview_input_fmts))
			fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;
		break;

	case PREV_PAD_SOURCE:
		pixelcode = fmt->code;
		*fmt = *__preview_get_format(prev, fh, PREV_PAD_SINK, which);

		switch (pixelcode) {
		case V4L2_MBUS_FMT_YUYV8_1X16:
		case V4L2_MBUS_FMT_UYVY8_1X16:
			fmt->code = pixelcode;
			break;

		default:
			fmt->code = V4L2_MBUS_FMT_YUYV8_1X16;
			break;
		}

		crop = __preview_get_crop(prev, fh, which);
		fmt->width = crop->width;
		fmt->height = crop->height;

		fmt->colorspace = V4L2_COLORSPACE_JPEG;
		break;
	}

	fmt->field = V4L2_FIELD_NONE;
}

static void preview_try_crop(struct isp_prev_device *prev,
			     const struct v4l2_mbus_framefmt *sink,
			     struct v4l2_rect *crop)
{
	unsigned int left = PREV_MARGIN_LEFT;
	unsigned int right = sink->width - PREV_MARGIN_RIGHT;
	unsigned int top = PREV_MARGIN_TOP;
	unsigned int bottom = sink->height - PREV_MARGIN_BOTTOM;

	if (prev->input == PREVIEW_INPUT_CCDC) {
		left += 2;
		right -= 2;
	}

	
	crop->left &= ~1;
	crop->top &= ~1;

	crop->left = clamp_t(u32, crop->left, left, right - PREV_MIN_OUT_WIDTH);
	crop->top = clamp_t(u32, crop->top, top, bottom - PREV_MIN_OUT_HEIGHT);
	crop->width = clamp_t(u32, crop->width, PREV_MIN_OUT_WIDTH,
			      right - crop->left);
	crop->height = clamp_t(u32, crop->height, PREV_MIN_OUT_HEIGHT,
			       bottom - crop->top);
}

static int preview_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	switch (code->pad) {
	case PREV_PAD_SINK:
		if (code->index >= ARRAY_SIZE(preview_input_fmts))
			return -EINVAL;

		code->code = preview_input_fmts[code->index];
		break;
	case PREV_PAD_SOURCE:
		if (code->index >= ARRAY_SIZE(preview_output_fmts))
			return -EINVAL;

		code->code = preview_output_fmts[code->index];
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int preview_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt format;

	if (fse->index != 0)
		return -EINVAL;

	format.code = fse->code;
	format.width = 1;
	format.height = 1;
	preview_try_format(prev, fh, fse->pad, &format, V4L2_SUBDEV_FORMAT_TRY);
	fse->min_width = format.width;
	fse->min_height = format.height;

	if (format.code != fse->code)
		return -EINVAL;

	format.code = fse->code;
	format.width = -1;
	format.height = -1;
	preview_try_format(prev, fh, fse->pad, &format, V4L2_SUBDEV_FORMAT_TRY);
	fse->max_width = format.width;
	fse->max_height = format.height;

	return 0;
}

static int preview_get_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			    struct v4l2_subdev_crop *crop)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);

	
	if (crop->pad != PREV_PAD_SINK)
		return -EINVAL;

	crop->rect = *__preview_get_crop(prev, fh, crop->which);
	return 0;
}

static int preview_set_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			    struct v4l2_subdev_crop *crop)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;

	
	if (crop->pad != PREV_PAD_SINK)
		return -EINVAL;

	
	if (prev->state != ISP_PIPELINE_STREAM_STOPPED)
		return -EBUSY;

	format = __preview_get_format(prev, fh, PREV_PAD_SINK, crop->which);
	preview_try_crop(prev, format, &crop->rect);
	*__preview_get_crop(prev, fh, crop->which) = crop->rect;

	
	format = __preview_get_format(prev, fh, PREV_PAD_SOURCE, crop->which);
	preview_try_format(prev, fh, PREV_PAD_SOURCE, format, crop->which);

	return 0;
}

static int preview_get_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			      struct v4l2_subdev_format *fmt)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;

	format = __preview_get_format(prev, fh, fmt->pad, fmt->which);
	if (format == NULL)
		return -EINVAL;

	fmt->format = *format;
	return 0;
}

static int preview_set_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			      struct v4l2_subdev_format *fmt)
{
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;

	format = __preview_get_format(prev, fh, fmt->pad, fmt->which);
	if (format == NULL)
		return -EINVAL;

	preview_try_format(prev, fh, fmt->pad, &fmt->format, fmt->which);
	*format = fmt->format;

	
	if (fmt->pad == PREV_PAD_SINK) {
		
		crop = __preview_get_crop(prev, fh, fmt->which);
		crop->left = 0;
		crop->top = 0;
		crop->width = fmt->format.width;
		crop->height = fmt->format.height;

		preview_try_crop(prev, &fmt->format, crop);

		
		format = __preview_get_format(prev, fh, PREV_PAD_SOURCE,
					      fmt->which);
		preview_try_format(prev, fh, PREV_PAD_SOURCE, format,
				   fmt->which);
	}

	return 0;
}

static int preview_init_formats(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;

	memset(&format, 0, sizeof(format));
	format.pad = PREV_PAD_SINK;
	format.which = fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = V4L2_MBUS_FMT_SGRBG10_1X10;
	format.format.width = 4096;
	format.format.height = 4096;
	preview_set_format(sd, fh, &format);

	return 0;
}

static const struct v4l2_subdev_core_ops preview_v4l2_core_ops = {
	.ioctl = preview_ioctl,
};

static const struct v4l2_subdev_video_ops preview_v4l2_video_ops = {
	.s_stream = preview_set_stream,
};

static const struct v4l2_subdev_pad_ops preview_v4l2_pad_ops = {
	.enum_mbus_code = preview_enum_mbus_code,
	.enum_frame_size = preview_enum_frame_size,
	.get_fmt = preview_get_format,
	.set_fmt = preview_set_format,
	.get_crop = preview_get_crop,
	.set_crop = preview_set_crop,
};

static const struct v4l2_subdev_ops preview_v4l2_ops = {
	.core = &preview_v4l2_core_ops,
	.video = &preview_v4l2_video_ops,
	.pad = &preview_v4l2_pad_ops,
};

static const struct v4l2_subdev_internal_ops preview_v4l2_internal_ops = {
	.open = preview_init_formats,
};


static int preview_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct isp_prev_device *prev = v4l2_get_subdevdata(sd);

	switch (local->index | media_entity_type(remote->entity)) {
	case PREV_PAD_SINK | MEDIA_ENT_T_DEVNODE:
		
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (prev->input == PREVIEW_INPUT_CCDC)
				return -EBUSY;
			prev->input = PREVIEW_INPUT_MEMORY;
		} else {
			if (prev->input == PREVIEW_INPUT_MEMORY)
				prev->input = PREVIEW_INPUT_NONE;
		}
		break;

	case PREV_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (prev->input == PREVIEW_INPUT_MEMORY)
				return -EBUSY;
			prev->input = PREVIEW_INPUT_CCDC;
		} else {
			if (prev->input == PREVIEW_INPUT_CCDC)
				prev->input = PREVIEW_INPUT_NONE;
		}
		break;


	case PREV_PAD_SOURCE | MEDIA_ENT_T_DEVNODE:
		
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (prev->output & ~PREVIEW_OUTPUT_MEMORY)
				return -EBUSY;
			prev->output |= PREVIEW_OUTPUT_MEMORY;
		} else {
			prev->output &= ~PREVIEW_OUTPUT_MEMORY;
		}
		break;

	case PREV_PAD_SOURCE | MEDIA_ENT_T_V4L2_SUBDEV:
		
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (prev->output & ~PREVIEW_OUTPUT_RESIZER)
				return -EBUSY;
			prev->output |= PREVIEW_OUTPUT_RESIZER;
		} else {
			prev->output &= ~PREVIEW_OUTPUT_RESIZER;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static const struct media_entity_operations preview_media_ops = {
	.link_setup = preview_link_setup,
};

void omap3isp_preview_unregister_entities(struct isp_prev_device *prev)
{
	v4l2_device_unregister_subdev(&prev->subdev);
	omap3isp_video_unregister(&prev->video_in);
	omap3isp_video_unregister(&prev->video_out);
}

int omap3isp_preview_register_entities(struct isp_prev_device *prev,
	struct v4l2_device *vdev)
{
	int ret;

	
	ret = v4l2_device_register_subdev(vdev, &prev->subdev);
	if (ret < 0)
		goto error;

	ret = omap3isp_video_register(&prev->video_in, vdev);
	if (ret < 0)
		goto error;

	ret = omap3isp_video_register(&prev->video_out, vdev);
	if (ret < 0)
		goto error;

	return 0;

error:
	omap3isp_preview_unregister_entities(prev);
	return ret;
}


static int preview_init_entities(struct isp_prev_device *prev)
{
	struct v4l2_subdev *sd = &prev->subdev;
	struct media_pad *pads = prev->pads;
	struct media_entity *me = &sd->entity;
	int ret;

	prev->input = PREVIEW_INPUT_NONE;

	v4l2_subdev_init(sd, &preview_v4l2_ops);
	sd->internal_ops = &preview_v4l2_internal_ops;
	strlcpy(sd->name, "OMAP3 ISP preview", sizeof(sd->name));
	sd->grp_id = 1 << 16;	
	v4l2_set_subdevdata(sd, prev);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	v4l2_ctrl_handler_init(&prev->ctrls, 2);
	v4l2_ctrl_new_std(&prev->ctrls, &preview_ctrl_ops, V4L2_CID_BRIGHTNESS,
			  ISPPRV_BRIGHT_LOW, ISPPRV_BRIGHT_HIGH,
			  ISPPRV_BRIGHT_STEP, ISPPRV_BRIGHT_DEF);
	v4l2_ctrl_new_std(&prev->ctrls, &preview_ctrl_ops, V4L2_CID_CONTRAST,
			  ISPPRV_CONTRAST_LOW, ISPPRV_CONTRAST_HIGH,
			  ISPPRV_CONTRAST_STEP, ISPPRV_CONTRAST_DEF);
	v4l2_ctrl_handler_setup(&prev->ctrls);
	sd->ctrl_handler = &prev->ctrls;

	pads[PREV_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	pads[PREV_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;

	me->ops = &preview_media_ops;
	ret = media_entity_init(me, PREV_PADS_NUM, pads, 0);
	if (ret < 0)
		return ret;

	preview_init_formats(sd, NULL);

	prev->video_in.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	prev->video_in.ops = &preview_video_ops;
	prev->video_in.isp = to_isp_device(prev);
	prev->video_in.capture_mem = PAGE_ALIGN(4096 * 4096) * 2 * 3;
	prev->video_in.bpl_alignment = 64;
	prev->video_out.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	prev->video_out.ops = &preview_video_ops;
	prev->video_out.isp = to_isp_device(prev);
	prev->video_out.capture_mem = PAGE_ALIGN(4096 * 4096) * 2 * 3;
	prev->video_out.bpl_alignment = 32;

	ret = omap3isp_video_init(&prev->video_in, "preview");
	if (ret < 0)
		goto error_video_in;

	ret = omap3isp_video_init(&prev->video_out, "preview");
	if (ret < 0)
		goto error_video_out;

	
	ret = media_entity_create_link(&prev->video_in.video.entity, 0,
			&prev->subdev.entity, PREV_PAD_SINK, 0);
	if (ret < 0)
		goto error_link;

	ret = media_entity_create_link(&prev->subdev.entity, PREV_PAD_SOURCE,
			&prev->video_out.video.entity, 0, 0);
	if (ret < 0)
		goto error_link;

	return 0;

error_link:
	omap3isp_video_cleanup(&prev->video_out);
error_video_out:
	omap3isp_video_cleanup(&prev->video_in);
error_video_in:
	media_entity_cleanup(&prev->subdev.entity);
	return ret;
}

int omap3isp_preview_init(struct isp_device *isp)
{
	struct isp_prev_device *prev = &isp->isp_prev;

	spin_lock_init(&prev->lock);
	init_waitqueue_head(&prev->wait);
	preview_init_params(prev);

	return preview_init_entities(prev);
}

void omap3isp_preview_cleanup(struct isp_device *isp)
{
	struct isp_prev_device *prev = &isp->isp_prev;

	v4l2_ctrl_handler_free(&prev->ctrls);
	omap3isp_video_cleanup(&prev->video_in);
	omap3isp_video_cleanup(&prev->video_out);
	media_entity_cleanup(&prev->subdev.entity);
}
