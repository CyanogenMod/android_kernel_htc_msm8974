/*
 * Copyright (C) 2005-2009 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * CCDC hardware module for DM355
 * ------------------------------
 *
 * This module is for configuring DM355 CCD controller of VPFE to capture
 * Raw yuv or Bayer RGB data from a decoder. CCDC has several modules
 * such as Defect Pixel Correction, Color Space Conversion etc to
 * pre-process the Bayer RGB data, before writing it to SDRAM. This
 * module also allows application to configure individual
 * module parameters through VPFE_CMD_S_CCDC_RAW_PARAMS IOCTL.
 * To do so, application include dm355_ccdc.h and vpfe_capture.h header
 * files. The setparams() API is called by vpfe_capture driver
 * to configure module parameters
 *
 * TODO: 1) Raw bayer parameter settings and bayer capture
 * 	 2) Split module parameter structure to module specific ioctl structs
 *	 3) add support for lense shading correction
 *	 4) investigate if enum used for user space type definition
 * 	    to be replaced by #defines or integer
 */
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>

#include <media/davinci/dm355_ccdc.h>
#include <media/davinci/vpss.h>

#include "dm355_ccdc_regs.h"
#include "ccdc_hw_device.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CCDC Driver for DM355");
MODULE_AUTHOR("Texas Instruments");

static struct ccdc_oper_config {
	struct device *dev;
	
	enum vpfe_hw_if_type if_type;
	
	struct ccdc_params_raw bayer;
	
	struct ccdc_params_ycbcr ycbcr;
	
	struct clk *mclk;
	
	struct clk *sclk;
	
	void __iomem *base_addr;
} ccdc_cfg = {
	
	.bayer = {
		.pix_fmt = CCDC_PIXFMT_RAW,
		.frm_fmt = CCDC_FRMFMT_PROGRESSIVE,
		.win = CCDC_WIN_VGA,
		.fid_pol = VPFE_PINPOL_POSITIVE,
		.vd_pol = VPFE_PINPOL_POSITIVE,
		.hd_pol = VPFE_PINPOL_POSITIVE,
		.gain = {
			.r_ye = 256,
			.gb_g = 256,
			.gr_cy = 256,
			.b_mg = 256
		},
		.config_params = {
			.datasft = 2,
			.mfilt1 = CCDC_NO_MEDIAN_FILTER1,
			.mfilt2 = CCDC_NO_MEDIAN_FILTER2,
			.alaw = {
				.gama_wd = 2,
			},
			.blk_clamp = {
				.sample_pixel = 1,
				.dc_sub = 25
			},
			.col_pat_field0 = {
				.olop = CCDC_GREEN_BLUE,
				.olep = CCDC_BLUE,
				.elop = CCDC_RED,
				.elep = CCDC_GREEN_RED
			},
			.col_pat_field1 = {
				.olop = CCDC_GREEN_BLUE,
				.olep = CCDC_BLUE,
				.elop = CCDC_RED,
				.elep = CCDC_GREEN_RED
			},
		},
	},
	
	.ycbcr = {
		.win = CCDC_WIN_PAL,
		.pix_fmt = CCDC_PIXFMT_YCBCR_8BIT,
		.frm_fmt = CCDC_FRMFMT_INTERLACED,
		.fid_pol = VPFE_PINPOL_POSITIVE,
		.vd_pol = VPFE_PINPOL_POSITIVE,
		.hd_pol = VPFE_PINPOL_POSITIVE,
		.bt656_enable = 1,
		.pix_order = CCDC_PIXORDER_CBYCRY,
		.buf_type = CCDC_BUFTYPE_FLD_INTERLEAVED
	},
};


static u32 ccdc_raw_bayer_pix_formats[] =
		{V4L2_PIX_FMT_SBGGR8, V4L2_PIX_FMT_SBGGR16};

static u32 ccdc_raw_yuv_pix_formats[] =
		{V4L2_PIX_FMT_UYVY, V4L2_PIX_FMT_YUYV};

static inline u32 regr(u32 offset)
{
	return __raw_readl(ccdc_cfg.base_addr + offset);
}

static inline void regw(u32 val, u32 offset)
{
	__raw_writel(val, ccdc_cfg.base_addr + offset);
}

static void ccdc_enable(int en)
{
	unsigned int temp;
	temp = regr(SYNCEN);
	temp &= (~CCDC_SYNCEN_VDHDEN_MASK);
	temp |= (en & CCDC_SYNCEN_VDHDEN_MASK);
	regw(temp, SYNCEN);
}

static void ccdc_enable_output_to_sdram(int en)
{
	unsigned int temp;
	temp = regr(SYNCEN);
	temp &= (~(CCDC_SYNCEN_WEN_MASK));
	temp |= ((en << CCDC_SYNCEN_WEN_SHIFT) & CCDC_SYNCEN_WEN_MASK);
	regw(temp, SYNCEN);
}

static void ccdc_config_gain_offset(void)
{
	
	regw(ccdc_cfg.bayer.gain.r_ye, RYEGAIN);
	regw(ccdc_cfg.bayer.gain.gr_cy, GRCYGAIN);
	regw(ccdc_cfg.bayer.gain.gb_g, GBGGAIN);
	regw(ccdc_cfg.bayer.gain.b_mg, BMGGAIN);
	
	regw(ccdc_cfg.bayer.ccdc_offset, OFFSET);
}

static int ccdc_restore_defaults(void)
{
	int i;

	dev_dbg(ccdc_cfg.dev, "\nstarting ccdc_restore_defaults...");
	
	for (i = 0; i <= CCDC_REG_LAST; i += 4)
		regw(0, i);

	
	regw(MODESET_DEFAULT, MODESET);
	
	regw(CULH_DEFAULT, CULH);
	regw(CULV_DEFAULT, CULV);
	
	ccdc_cfg.bayer.gain.r_ye = GAIN_DEFAULT;
	ccdc_cfg.bayer.gain.gb_g = GAIN_DEFAULT;
	ccdc_cfg.bayer.gain.gr_cy = GAIN_DEFAULT;
	ccdc_cfg.bayer.gain.b_mg = GAIN_DEFAULT;
	ccdc_config_gain_offset();
	regw(OUTCLIP_DEFAULT, OUTCLIP);
	regw(LSCCFG2_DEFAULT, LSCCFG2);
	
	if (vpss_select_ccdc_source(VPSS_CCDCIN)) {
		dev_dbg(ccdc_cfg.dev, "\ncouldn't select ccdc input source");
		return -EFAULT;
	}
	
	if (vpss_enable_clock(VPSS_CCDC_CLOCK, 1) < 0) {
		dev_dbg(ccdc_cfg.dev, "\ncouldn't enable ccdc clock");
		return -EFAULT;
	}
	dev_dbg(ccdc_cfg.dev, "\nEnd of ccdc_restore_defaults...");
	return 0;
}

static int ccdc_open(struct device *device)
{
	return ccdc_restore_defaults();
}

static int ccdc_close(struct device *device)
{
	
	vpss_enable_clock(VPSS_CCDC_CLOCK, 0);
	
	return 0;
}
static void ccdc_setwin(struct v4l2_rect *image_win,
			enum ccdc_frmfmt frm_fmt, int ppc)
{
	int horz_start, horz_nr_pixels;
	int vert_start, vert_nr_lines;
	int mid_img = 0;

	dev_dbg(ccdc_cfg.dev, "\nStarting ccdc_setwin...");

	horz_start = image_win->left << (ppc - 1);
	horz_nr_pixels = ((image_win->width) << (ppc - 1)) - 1;

	
	regw(horz_start, SPH);
	regw(horz_nr_pixels, NPH);
	vert_start = image_win->top;

	if (frm_fmt == CCDC_FRMFMT_INTERLACED) {
		vert_nr_lines = (image_win->height >> 1) - 1;
		vert_start >>= 1;
		
		vert_start += 1;
		
		regw(vert_start, VDINT0);
	} else {
		
		vert_start += 1;
		vert_nr_lines = image_win->height - 1;
		
		mid_img = vert_start + (image_win->height / 2);
		regw(vert_start, VDINT0);
		regw(mid_img, VDINT1);
	}
	regw(vert_start & CCDC_START_VER_ONE_MASK, SLV0);
	regw(vert_start & CCDC_START_VER_TWO_MASK, SLV1);
	regw(vert_nr_lines & CCDC_NUM_LINES_VER, NLV);
	dev_dbg(ccdc_cfg.dev, "\nEnd of ccdc_setwin...");
}

static int validate_ccdc_param(struct ccdc_config_params_raw *ccdcparam)
{
	if (ccdcparam->datasft < CCDC_DATA_NO_SHIFT ||
	    ccdcparam->datasft > CCDC_DATA_SHIFT_6BIT) {
		dev_dbg(ccdc_cfg.dev, "Invalid value of data shift\n");
		return -EINVAL;
	}

	if (ccdcparam->mfilt1 < CCDC_NO_MEDIAN_FILTER1 ||
	    ccdcparam->mfilt1 > CCDC_MEDIAN_FILTER1) {
		dev_dbg(ccdc_cfg.dev, "Invalid value of median filter1\n");
		return -EINVAL;
	}

	if (ccdcparam->mfilt2 < CCDC_NO_MEDIAN_FILTER2 ||
	    ccdcparam->mfilt2 > CCDC_MEDIAN_FILTER2) {
		dev_dbg(ccdc_cfg.dev, "Invalid value of median filter2\n");
		return -EINVAL;
	}

	if ((ccdcparam->med_filt_thres < 0) ||
	   (ccdcparam->med_filt_thres > CCDC_MED_FILT_THRESH)) {
		dev_dbg(ccdc_cfg.dev,
			"Invalid value of median filter threshold\n");
		return -EINVAL;
	}

	if (ccdcparam->data_sz < CCDC_DATA_16BITS ||
	    ccdcparam->data_sz > CCDC_DATA_8BITS) {
		dev_dbg(ccdc_cfg.dev, "Invalid value of data size\n");
		return -EINVAL;
	}

	if (ccdcparam->alaw.enable) {
		if (ccdcparam->alaw.gama_wd < CCDC_GAMMA_BITS_13_4 ||
		    ccdcparam->alaw.gama_wd > CCDC_GAMMA_BITS_09_0) {
			dev_dbg(ccdc_cfg.dev, "Invalid value of ALAW\n");
			return -EINVAL;
		}
	}

	if (ccdcparam->blk_clamp.b_clamp_enable) {
		if (ccdcparam->blk_clamp.sample_pixel < CCDC_SAMPLE_1PIXELS ||
		    ccdcparam->blk_clamp.sample_pixel > CCDC_SAMPLE_16PIXELS) {
			dev_dbg(ccdc_cfg.dev,
				"Invalid value of sample pixel\n");
			return -EINVAL;
		}
		if (ccdcparam->blk_clamp.sample_ln < CCDC_SAMPLE_1LINES ||
		    ccdcparam->blk_clamp.sample_ln > CCDC_SAMPLE_16LINES) {
			dev_dbg(ccdc_cfg.dev,
				"Invalid value of sample lines\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int ccdc_set_params(void __user *params)
{
	struct ccdc_config_params_raw ccdc_raw_params;
	int x;

	
	if (ccdc_cfg.if_type != VPFE_RAW_BAYER)
		return -EINVAL;

	x = copy_from_user(&ccdc_raw_params, params, sizeof(ccdc_raw_params));
	if (x) {
		dev_dbg(ccdc_cfg.dev, "ccdc_set_params: error in copying ccdc"
			"params, %d\n", x);
		return -EFAULT;
	}

	if (!validate_ccdc_param(&ccdc_raw_params)) {
		memcpy(&ccdc_cfg.bayer.config_params,
			&ccdc_raw_params,
			sizeof(ccdc_raw_params));
		return 0;
	}
	return -EINVAL;
}

static void ccdc_config_ycbcr(void)
{
	struct ccdc_params_ycbcr *params = &ccdc_cfg.ycbcr;
	u32 temp;

	
	dev_dbg(ccdc_cfg.dev, "\nStarting ccdc_config_ycbcr...");
	ccdc_restore_defaults();

	
	temp = (((params->pix_fmt & CCDC_INPUT_MODE_MASK) <<
		CCDC_INPUT_MODE_SHIFT) |
		((params->frm_fmt & CCDC_FRM_FMT_MASK) <<
		CCDC_FRM_FMT_SHIFT));

	
	if (params->bt656_enable) {
		regw(CCDC_REC656IF_BT656_EN, REC656IF);
		temp |= CCDC_VD_POL_NEGATIVE;
	} else {		
		temp |= (((params->fid_pol & CCDC_FID_POL_MASK) <<
			CCDC_FID_POL_SHIFT) |
			((params->hd_pol & CCDC_HD_POL_MASK) <<
			CCDC_HD_POL_SHIFT) |
			((params->vd_pol & CCDC_VD_POL_MASK) <<
			CCDC_VD_POL_SHIFT));
	}

	
	temp |= CCDC_DATA_PACK_ENABLE;

	regw(temp, MODESET);

	
	ccdc_setwin(&params->win, params->frm_fmt, 2);

	
	temp = (params->pix_order << CCDC_Y8POS_SHIFT);
	temp |= CCDC_LATCH_ON_VSYNC_DISABLE | CCDC_CCDCFG_FIDMD_NO_LATCH_VSYNC;
	regw(temp, CCDCFG);

	regw(((params->win.width * 2 + 31) >> 5), HSIZE);

	
	if (params->buf_type == CCDC_BUFTYPE_FLD_INTERLEAVED) {
		
		regw(CCDC_SDOFST_FIELD_INTERLEAVED, SDOFST);
	}

	dev_dbg(ccdc_cfg.dev, "\nEnd of ccdc_config_ycbcr...\n");
}

static void ccdc_config_black_clamp(struct ccdc_black_clamp *bclamp)
{
	u32 val;

	if (!bclamp->b_clamp_enable) {
		
		regw(bclamp->dc_sub & CCDC_BLK_DC_SUB_MASK, DCSUB);
		regw(0x0000, CLAMP);
		return;
	}
	
	val = (bclamp->start_pixel & CCDC_BLK_ST_PXL_MASK) |
	      ((bclamp->sample_pixel & CCDC_BLK_SAMPLE_LN_MASK) <<
		CCDC_BLK_SAMPLE_LN_SHIFT) | CCDC_BLK_CLAMP_ENABLE;
	regw(val, CLAMP);

	
	val = (bclamp->sample_ln & CCDC_NUM_LINE_CALC_MASK)
			<< CCDC_NUM_LINE_CALC_SHIFT;
	regw(val, DCSUB);
}

static void ccdc_config_black_compense(struct ccdc_black_compensation *bcomp)
{
	u32 val;

	val = (bcomp->b & CCDC_BLK_COMP_MASK) |
		((bcomp->gb & CCDC_BLK_COMP_MASK) <<
		CCDC_BLK_COMP_GB_COMP_SHIFT);
	regw(val, BLKCMP1);

	val = ((bcomp->gr & CCDC_BLK_COMP_MASK) <<
		CCDC_BLK_COMP_GR_COMP_SHIFT) |
		((bcomp->r & CCDC_BLK_COMP_MASK) <<
		CCDC_BLK_COMP_R_COMP_SHIFT);
	regw(val, BLKCMP0);
}

int ccdc_write_dfc_entry(int index, struct ccdc_vertical_dft *dfc)
{
#define DFC_WRITE_WAIT_COUNT	1000
	u32 val, count = DFC_WRITE_WAIT_COUNT;

	regw(dfc->dft_corr_vert[index], DFCMEM0);
	regw(dfc->dft_corr_horz[index], DFCMEM1);
	regw(dfc->dft_corr_sub1[index], DFCMEM2);
	regw(dfc->dft_corr_sub2[index], DFCMEM3);
	regw(dfc->dft_corr_sub3[index], DFCMEM4);
	
	val = regr(DFCMEMCTL) | CCDC_DFCMEMCTL_DFCMWR_MASK;
	regw(val, DFCMEMCTL);

	while (regr(DFCMEMCTL) & CCDC_DFCMEMCTL_DFCMWR_MASK)
		count--;

	if (count) {
		dev_err(ccdc_cfg.dev, "defect table write timeout !!!\n");
		return -1;
	}
	return 0;
}

static int ccdc_config_vdfc(struct ccdc_vertical_dft *dfc)
{
	u32 val;
	int i;

	
	val = dfc->gen_dft_en & CCDC_DFCCTL_GDFCEN_MASK;

	
	if (!dfc->ver_dft_en) {
		
		regw(val, DFCCTL);
		return 0;
	}

	if (dfc->table_size > CCDC_DFT_TABLE_SIZE)
		return -EINVAL;

	val |= CCDC_DFCCTL_VDFC_DISABLE;
	val |= (dfc->dft_corr_ctl.vdfcsl & CCDC_DFCCTL_VDFCSL_MASK) <<
		CCDC_DFCCTL_VDFCSL_SHIFT;
	val |= (dfc->dft_corr_ctl.vdfcuda & CCDC_DFCCTL_VDFCUDA_MASK) <<
		CCDC_DFCCTL_VDFCUDA_SHIFT;
	val |= (dfc->dft_corr_ctl.vdflsft & CCDC_DFCCTL_VDFLSFT_MASK) <<
		CCDC_DFCCTL_VDFLSFT_SHIFT;
	regw(val , DFCCTL);

	
	val = CCDC_DFCMEMCTL_DFCMARST_MASK << CCDC_DFCMEMCTL_DFCMARST_SHIFT;

	
	for (i = 0; i < dfc->table_size; i++) {
		
		if (i != 0)
			val = CCDC_DFCMEMCTL_INC_ADDR;
		regw(val, DFCMEMCTL);
		if (ccdc_write_dfc_entry(i, dfc) < 0)
			return -EFAULT;
	}

	
	regw(dfc->saturation_ctl & CCDC_VDC_DFCVSAT_MASK, DFCVSAT);
	val = regr(DFCCTL) | (CCDC_DFCCTL_VDFCEN_MASK <<
			CCDC_DFCCTL_VDFCEN_SHIFT);
	regw(val, DFCCTL);
	return 0;
}

static void ccdc_config_csc(struct ccdc_csc *csc)
{
	u32 val1, val2;
	int i;

	if (!csc->enable)
		return;

	
	regw(CCDC_CSC_ENABLE, CSCCTL);

	
	for (i = 0; i < CCDC_CSC_COEFF_TABLE_SIZE; i++) {
		if ((i % 2) == 0) {
			
			val1 = (csc->coeff[i].integer &
				CCDC_CSC_COEF_INTEG_MASK)
				<< CCDC_CSC_COEF_INTEG_SHIFT;
			val1 |= (((csc->coeff[i].decimal &
				CCDC_CSC_COEF_DECIMAL_MASK) *
				CCDC_CSC_DEC_MAX) / 100);
		} else {

			
			val2 = (csc->coeff[i].integer &
				CCDC_CSC_COEF_INTEG_MASK)
				<< CCDC_CSC_COEF_INTEG_SHIFT;
			val2 |= (((csc->coeff[i].decimal &
				 CCDC_CSC_COEF_DECIMAL_MASK) *
				 CCDC_CSC_DEC_MAX) / 100);
			val2 <<= CCDC_CSCM_MSB_SHIFT;
			val2 |= val1;
			regw(val2, (CSCM0 + ((i - 1) << 1)));
		}
	}
}

static void ccdc_config_color_patterns(struct ccdc_col_pat *pat0,
				       struct ccdc_col_pat *pat1)
{
	u32 val;

	val = (pat0->olop | (pat0->olep << 2) | (pat0->elop << 4) |
		(pat0->elep << 6) | (pat1->olop << 8) | (pat1->olep << 10) |
		(pat1->elop << 12) | (pat1->elep << 14));
	regw(val, COLPTN);
}

static int ccdc_config_raw(void)
{
	struct ccdc_params_raw *params = &ccdc_cfg.bayer;
	struct ccdc_config_params_raw *config_params =
					&ccdc_cfg.bayer.config_params;
	unsigned int val;

	dev_dbg(ccdc_cfg.dev, "\nStarting ccdc_config_raw...");

	
	ccdc_restore_defaults();

	regw(CCDC_YCINSWP_RAW | CCDC_CCDCFG_FIDMD_LATCH_VSYNC |
	     CCDC_CCDCFG_WENLOG_AND | CCDC_CCDCFG_TRGSEL_WEN |
	     CCDC_CCDCFG_EXTRG_DISABLE | CCDC_LATCH_ON_VSYNC_DISABLE, CCDCFG);

	val = (CCDC_VDHDOUT_INPUT | CCDC_RAW_IP_MODE | CCDC_DATAPOL_NORMAL |
		CCDC_EXWEN_DISABLE);

	val |= (((params->vd_pol & CCDC_VD_POL_MASK) << CCDC_VD_POL_SHIFT) |
		((params->hd_pol & CCDC_HD_POL_MASK) << CCDC_HD_POL_SHIFT) |
		((params->fid_pol & CCDC_FID_POL_MASK) << CCDC_FID_POL_SHIFT) |
		((params->frm_fmt & CCDC_FRM_FMT_MASK) << CCDC_FRM_FMT_SHIFT) |
		((params->pix_fmt & CCDC_PIX_FMT_MASK) << CCDC_PIX_FMT_SHIFT));

	
	if ((config_params->data_sz == CCDC_DATA_8BITS) ||
	     config_params->alaw.enable)
		val |= CCDC_DATA_PACK_ENABLE;

	
	if (config_params->lpf_enable)
		val |= (config_params->lpf_enable & CCDC_LPF_MASK) <<
			CCDC_LPF_SHIFT;

	
	val |= (config_params->datasft & CCDC_DATASFT_MASK) <<
		CCDC_DATASFT_SHIFT;
	regw(val , MODESET);
	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to MODESET...\n", val);

	
	regw((config_params->med_filt_thres) & CCDC_MED_FILT_THRESH, MEDFILT);

	
	val = CCDC_GAMMA_BITS_11_2 << CCDC_GAMMAWD_INPUT_SHIFT |
		CCDC_CFA_MOSAIC;

	
	if (config_params->alaw.enable) {
		val |= (CCDC_ALAW_ENABLE |
			((config_params->alaw.gama_wd &
			CCDC_ALAW_GAMA_WD_MASK) <<
			CCDC_GAMMAWD_INPUT_SHIFT));
	}

	
	val |= ((config_params->mfilt1 << CCDC_MFILT1_SHIFT) |
		(config_params->mfilt2 << CCDC_MFILT2_SHIFT));

	regw(val, GAMMAWD);
	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to GAMMAWD...\n", val);

	
	ccdc_setwin(&params->win, params->frm_fmt, 1);

	
	ccdc_config_black_clamp(&config_params->blk_clamp);

	
	ccdc_config_black_compense(&config_params->blk_comp);

	
	if (ccdc_config_vdfc(&config_params->vertical_dft) < 0)
		return -EFAULT;

	
	ccdc_config_csc(&config_params->csc);

	
	ccdc_config_color_patterns(&config_params->col_pat_field0,
				   &config_params->col_pat_field1);

	
	ccdc_config_gain_offset();

	dev_dbg(ccdc_cfg.dev, "\nWriting %x to COLPTN...\n", val);

	
	val = (config_params->data_offset.horz_offset & CCDC_DATAOFST_MASK) <<
		CCDC_DATAOFST_H_SHIFT;
	val |= (config_params->data_offset.vert_offset & CCDC_DATAOFST_MASK) <<
		CCDC_DATAOFST_V_SHIFT;
	regw(val, DATAOFST);

	
	val = (params->horz_flip_enable & CCDC_HSIZE_FLIP_MASK) <<
		CCDC_HSIZE_FLIP_SHIFT;

	
	if ((config_params->data_sz == CCDC_DATA_8BITS) ||
	     config_params->alaw.enable) {
		val |= (((params->win.width) + 31) >> 5) &
			CCDC_HSIZE_VAL_MASK;

		
		dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to HSIZE...\n",
		       (((params->win.width) + 31) >> 5) &
			CCDC_HSIZE_VAL_MASK);
	} else {
		
		val |= (((params->win.width * 2) + 31) >> 5) &
			CCDC_HSIZE_VAL_MASK;

		dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to HSIZE...\n",
		       (((params->win.width * 2) + 31) >> 5) &
			CCDC_HSIZE_VAL_MASK);
	}
	regw(val, HSIZE);

	
	if (params->frm_fmt == CCDC_FRMFMT_INTERLACED) {
		if (params->image_invert_enable) {
			
			regw(CCDC_SDOFST_INTERLACE_INVERSE, SDOFST);
			dev_dbg(ccdc_cfg.dev, "\nWriting %x to SDOFST...\n",
				CCDC_SDOFST_INTERLACE_INVERSE);
		} else {
			
			regw(CCDC_SDOFST_INTERLACE_NORMAL, SDOFST);
			dev_dbg(ccdc_cfg.dev, "\nWriting %x to SDOFST...\n",
				CCDC_SDOFST_INTERLACE_NORMAL);
		}
	} else if (params->frm_fmt == CCDC_FRMFMT_PROGRESSIVE) {
		if (params->image_invert_enable) {
			
			regw(CCDC_SDOFST_PROGRESSIVE_INVERSE, SDOFST);
			dev_dbg(ccdc_cfg.dev, "\nWriting %x to SDOFST...\n",
				CCDC_SDOFST_PROGRESSIVE_INVERSE);
		} else {
			
			regw(CCDC_SDOFST_PROGRESSIVE_NORMAL, SDOFST);
			dev_dbg(ccdc_cfg.dev, "\nWriting %x to SDOFST...\n",
				CCDC_SDOFST_PROGRESSIVE_NORMAL);
		}
	}
	dev_dbg(ccdc_cfg.dev, "\nend of ccdc_config_raw...");
	return 0;
}

static int ccdc_configure(void)
{
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		return ccdc_config_raw();
	else
		ccdc_config_ycbcr();
	return 0;
}

static int ccdc_set_buftype(enum ccdc_buftype buf_type)
{
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		ccdc_cfg.bayer.buf_type = buf_type;
	else
		ccdc_cfg.ycbcr.buf_type = buf_type;
	return 0;
}
static enum ccdc_buftype ccdc_get_buftype(void)
{
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		return ccdc_cfg.bayer.buf_type;
	return ccdc_cfg.ycbcr.buf_type;
}

static int ccdc_enum_pix(u32 *pix, int i)
{
	int ret = -EINVAL;
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER) {
		if (i < ARRAY_SIZE(ccdc_raw_bayer_pix_formats)) {
			*pix = ccdc_raw_bayer_pix_formats[i];
			ret = 0;
		}
	} else {
		if (i < ARRAY_SIZE(ccdc_raw_yuv_pix_formats)) {
			*pix = ccdc_raw_yuv_pix_formats[i];
			ret = 0;
		}
	}
	return ret;
}

static int ccdc_set_pixel_format(u32 pixfmt)
{
	struct ccdc_a_law *alaw = &ccdc_cfg.bayer.config_params.alaw;

	if (ccdc_cfg.if_type == VPFE_RAW_BAYER) {
		ccdc_cfg.bayer.pix_fmt = CCDC_PIXFMT_RAW;
		if (pixfmt == V4L2_PIX_FMT_SBGGR8)
			alaw->enable = 1;
		else if (pixfmt != V4L2_PIX_FMT_SBGGR16)
			return -EINVAL;
	} else {
		if (pixfmt == V4L2_PIX_FMT_YUYV)
			ccdc_cfg.ycbcr.pix_order = CCDC_PIXORDER_YCBYCR;
		else if (pixfmt == V4L2_PIX_FMT_UYVY)
			ccdc_cfg.ycbcr.pix_order = CCDC_PIXORDER_CBYCRY;
		else
			return -EINVAL;
	}
	return 0;
}
static u32 ccdc_get_pixel_format(void)
{
	struct ccdc_a_law *alaw = &ccdc_cfg.bayer.config_params.alaw;
	u32 pixfmt;

	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		if (alaw->enable)
			pixfmt = V4L2_PIX_FMT_SBGGR8;
		else
			pixfmt = V4L2_PIX_FMT_SBGGR16;
	else {
		if (ccdc_cfg.ycbcr.pix_order == CCDC_PIXORDER_YCBYCR)
			pixfmt = V4L2_PIX_FMT_YUYV;
		else
			pixfmt = V4L2_PIX_FMT_UYVY;
	}
	return pixfmt;
}
static int ccdc_set_image_window(struct v4l2_rect *win)
{
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		ccdc_cfg.bayer.win = *win;
	else
		ccdc_cfg.ycbcr.win = *win;
	return 0;
}

static void ccdc_get_image_window(struct v4l2_rect *win)
{
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		*win = ccdc_cfg.bayer.win;
	else
		*win = ccdc_cfg.ycbcr.win;
}

static unsigned int ccdc_get_line_length(void)
{
	struct ccdc_config_params_raw *config_params =
				&ccdc_cfg.bayer.config_params;
	unsigned int len;

	if (ccdc_cfg.if_type == VPFE_RAW_BAYER) {
		if ((config_params->alaw.enable) ||
		    (config_params->data_sz == CCDC_DATA_8BITS))
			len = ccdc_cfg.bayer.win.width;
		else
			len = ccdc_cfg.bayer.win.width * 2;
	} else
		len = ccdc_cfg.ycbcr.win.width * 2;
	return ALIGN(len, 32);
}

static int ccdc_set_frame_format(enum ccdc_frmfmt frm_fmt)
{
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		ccdc_cfg.bayer.frm_fmt = frm_fmt;
	else
		ccdc_cfg.ycbcr.frm_fmt = frm_fmt;
	return 0;
}

static enum ccdc_frmfmt ccdc_get_frame_format(void)
{
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		return ccdc_cfg.bayer.frm_fmt;
	else
		return ccdc_cfg.ycbcr.frm_fmt;
}

static int ccdc_getfid(void)
{
	return  (regr(MODESET) >> 15) & 1;
}

static inline void ccdc_setfbaddr(unsigned long addr)
{
	regw((addr >> 21) & 0x007f, STADRH);
	regw((addr >> 5) & 0x0ffff, STADRL);
}

static int ccdc_set_hw_if_params(struct vpfe_hw_if_param *params)
{
	ccdc_cfg.if_type = params->if_type;

	switch (params->if_type) {
	case VPFE_BT656:
	case VPFE_YCBCR_SYNC_16:
	case VPFE_YCBCR_SYNC_8:
		ccdc_cfg.ycbcr.vd_pol = params->vdpol;
		ccdc_cfg.ycbcr.hd_pol = params->hdpol;
		break;
	default:
		
		return -EINVAL;
	}
	return 0;
}

static struct ccdc_hw_device ccdc_hw_dev = {
	.name = "DM355 CCDC",
	.owner = THIS_MODULE,
	.hw_ops = {
		.open = ccdc_open,
		.close = ccdc_close,
		.enable = ccdc_enable,
		.enable_out_to_sdram = ccdc_enable_output_to_sdram,
		.set_hw_if_params = ccdc_set_hw_if_params,
		.set_params = ccdc_set_params,
		.configure = ccdc_configure,
		.set_buftype = ccdc_set_buftype,
		.get_buftype = ccdc_get_buftype,
		.enum_pix = ccdc_enum_pix,
		.set_pixel_format = ccdc_set_pixel_format,
		.get_pixel_format = ccdc_get_pixel_format,
		.set_frame_format = ccdc_set_frame_format,
		.get_frame_format = ccdc_get_frame_format,
		.set_image_window = ccdc_set_image_window,
		.get_image_window = ccdc_get_image_window,
		.get_line_length = ccdc_get_line_length,
		.setfbaddr = ccdc_setfbaddr,
		.getfid = ccdc_getfid,
	},
};

static int __init dm355_ccdc_probe(struct platform_device *pdev)
{
	void (*setup_pinmux)(void);
	struct resource	*res;
	int status = 0;

	status = vpfe_register_ccdc_device(&ccdc_hw_dev);
	if (status < 0)
		return status;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		status = -ENODEV;
		goto fail_nores;
	}

	res = request_mem_region(res->start, resource_size(res), res->name);
	if (!res) {
		status = -EBUSY;
		goto fail_nores;
	}

	ccdc_cfg.base_addr = ioremap_nocache(res->start, resource_size(res));
	if (!ccdc_cfg.base_addr) {
		status = -ENOMEM;
		goto fail_nomem;
	}

	
	ccdc_cfg.mclk = clk_get(&pdev->dev, "master");
	if (IS_ERR(ccdc_cfg.mclk)) {
		status = PTR_ERR(ccdc_cfg.mclk);
		goto fail_nomap;
	}
	if (clk_enable(ccdc_cfg.mclk)) {
		status = -ENODEV;
		goto fail_mclk;
	}

	
	ccdc_cfg.sclk = clk_get(&pdev->dev, "slave");
	if (IS_ERR(ccdc_cfg.sclk)) {
		status = PTR_ERR(ccdc_cfg.sclk);
		goto fail_mclk;
	}
	if (clk_enable(ccdc_cfg.sclk)) {
		status = -ENODEV;
		goto fail_sclk;
	}

	
	if (NULL == pdev->dev.platform_data) {
		status = -ENODEV;
		goto fail_sclk;
	}
	setup_pinmux = pdev->dev.platform_data;
	setup_pinmux();
	ccdc_cfg.dev = &pdev->dev;
	printk(KERN_NOTICE "%s is registered with vpfe.\n", ccdc_hw_dev.name);
	return 0;
fail_sclk:
	clk_put(ccdc_cfg.sclk);
fail_mclk:
	clk_put(ccdc_cfg.mclk);
fail_nomap:
	iounmap(ccdc_cfg.base_addr);
fail_nomem:
	release_mem_region(res->start, resource_size(res));
fail_nores:
	vpfe_unregister_ccdc_device(&ccdc_hw_dev);
	return status;
}

static int dm355_ccdc_remove(struct platform_device *pdev)
{
	struct resource	*res;

	clk_put(ccdc_cfg.mclk);
	clk_put(ccdc_cfg.sclk);
	iounmap(ccdc_cfg.base_addr);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));
	vpfe_unregister_ccdc_device(&ccdc_hw_dev);
	return 0;
}

static struct platform_driver dm355_ccdc_driver = {
	.driver = {
		.name	= "dm355_ccdc",
		.owner = THIS_MODULE,
	},
	.remove = __devexit_p(dm355_ccdc_remove),
	.probe = dm355_ccdc_probe,
};

module_platform_driver(dm355_ccdc_driver);
