/*
 * Copyright (C) 2006-2009 Texas Instruments Inc
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
 * CCDC hardware module for DM6446
 * ------------------------------
 *
 * This module is for configuring CCD controller of DM6446 VPFE to capture
 * Raw yuv or Bayer RGB data from a decoder. CCDC has several modules
 * such as Defect Pixel Correction, Color Space Conversion etc to
 * pre-process the Raw Bayer RGB data, before writing it to SDRAM. This
 * module also allows application to configure individual
 * module parameters through VPFE_CMD_S_CCDC_RAW_PARAMS IOCTL.
 * To do so, application includes dm644x_ccdc.h and vpfe_capture.h header
 * files.  The setparams() API is called by vpfe_capture driver
 * to configure module parameters. This file is named DM644x so that other
 * variants such DM6443 may be supported using the same module.
 *
 * TODO: Test Raw bayer parameter settings and bayer capture
 * 	 Split module parameter structure to module specific ioctl structs
 * 	 investigate if enum used for user space type definition
 * 	 to be replaced by #defines or integer
 */
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/videodev2.h>
#include <linux/gfp.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>

#include <media/davinci/dm644x_ccdc.h>
#include <media/davinci/vpss.h>

#include "dm644x_ccdc_regs.h"
#include "ccdc_hw_device.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CCDC Driver for DM6446");
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
		.config_params = {
			.data_sz = CCDC_DATA_10BITS,
		},
	},
	.ycbcr = {
		.pix_fmt = CCDC_PIXFMT_YCBCR_8BIT,
		.frm_fmt = CCDC_FRMFMT_INTERLACED,
		.win = CCDC_WIN_PAL,
		.fid_pol = VPFE_PINPOL_POSITIVE,
		.vd_pol = VPFE_PINPOL_POSITIVE,
		.hd_pol = VPFE_PINPOL_POSITIVE,
		.bt656_enable = 1,
		.pix_order = CCDC_PIXORDER_CBYCRY,
		.buf_type = CCDC_BUFTYPE_FLD_INTERLEAVED
	},
};

#define CCDC_MAX_RAW_YUV_FORMATS	2

static u32 ccdc_raw_bayer_pix_formats[] =
	{V4L2_PIX_FMT_SBGGR8, V4L2_PIX_FMT_SBGGR16};

static u32 ccdc_raw_yuv_pix_formats[] =
	{V4L2_PIX_FMT_UYVY, V4L2_PIX_FMT_YUYV};

static u32 ccdc_ctx[CCDC_REG_END / sizeof(u32)];

static inline u32 regr(u32 offset)
{
	return __raw_readl(ccdc_cfg.base_addr + offset);
}

static inline void regw(u32 val, u32 offset)
{
	__raw_writel(val, ccdc_cfg.base_addr + offset);
}

static void ccdc_enable(int flag)
{
	regw(flag, CCDC_PCR);
}

static void ccdc_enable_vport(int flag)
{
	if (flag)
		
		regw(CCDC_ENABLE_VIDEO_PORT, CCDC_FMTCFG);
	else
		regw(CCDC_DISABLE_VIDEO_PORT, CCDC_FMTCFG);
}

void ccdc_setwin(struct v4l2_rect *image_win,
		enum ccdc_frmfmt frm_fmt,
		int ppc)
{
	int horz_start, horz_nr_pixels;
	int vert_start, vert_nr_lines;
	int val = 0, mid_img = 0;

	dev_dbg(ccdc_cfg.dev, "\nStarting ccdc_setwin...");
	horz_start = image_win->left << (ppc - 1);
	horz_nr_pixels = (image_win->width << (ppc - 1)) - 1;
	regw((horz_start << CCDC_HORZ_INFO_SPH_SHIFT) | horz_nr_pixels,
	     CCDC_HORZ_INFO);

	vert_start = image_win->top;

	if (frm_fmt == CCDC_FRMFMT_INTERLACED) {
		vert_nr_lines = (image_win->height >> 1) - 1;
		vert_start >>= 1;
		
		vert_start += 1;
		
		val = (vert_start << CCDC_VDINT_VDINT0_SHIFT);
		regw(val, CCDC_VDINT);

	} else {
		
		vert_start += 1;
		vert_nr_lines = image_win->height - 1;
		mid_img = vert_start + (image_win->height / 2);
		val = (vert_start << CCDC_VDINT_VDINT0_SHIFT) |
		    (mid_img & CCDC_VDINT_VDINT1_MASK);
		regw(val, CCDC_VDINT);

	}
	regw((vert_start << CCDC_VERT_START_SLV0_SHIFT) | vert_start,
	     CCDC_VERT_START);
	regw(vert_nr_lines, CCDC_VERT_LINES);
	dev_dbg(ccdc_cfg.dev, "\nEnd of ccdc_setwin...");
}

static void ccdc_readregs(void)
{
	unsigned int val = 0;

	val = regr(CCDC_ALAW);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to ALAW...\n", val);
	val = regr(CCDC_CLAMP);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to CLAMP...\n", val);
	val = regr(CCDC_DCSUB);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to DCSUB...\n", val);
	val = regr(CCDC_BLKCMP);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to BLKCMP...\n", val);
	val = regr(CCDC_FPC_ADDR);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to FPC_ADDR...\n", val);
	val = regr(CCDC_FPC);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to FPC...\n", val);
	val = regr(CCDC_FMTCFG);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to FMTCFG...\n", val);
	val = regr(CCDC_COLPTN);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to COLPTN...\n", val);
	val = regr(CCDC_FMT_HORZ);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to FMT_HORZ...\n", val);
	val = regr(CCDC_FMT_VERT);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to FMT_VERT...\n", val);
	val = regr(CCDC_HSIZE_OFF);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to HSIZE_OFF...\n", val);
	val = regr(CCDC_SDOFST);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to SDOFST...\n", val);
	val = regr(CCDC_VP_OUT);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to VP_OUT...\n", val);
	val = regr(CCDC_SYN_MODE);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to SYN_MODE...\n", val);
	val = regr(CCDC_HORZ_INFO);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to HORZ_INFO...\n", val);
	val = regr(CCDC_VERT_START);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to VERT_START...\n", val);
	val = regr(CCDC_VERT_LINES);
	dev_notice(ccdc_cfg.dev, "\nReading 0x%x to VERT_LINES...\n", val);
}

static int validate_ccdc_param(struct ccdc_config_params_raw *ccdcparam)
{
	if (ccdcparam->alaw.enable) {
		if ((ccdcparam->alaw.gama_wd > CCDC_GAMMA_BITS_09_0) ||
		    (ccdcparam->alaw.gama_wd < CCDC_GAMMA_BITS_15_6) ||
		    (ccdcparam->alaw.gama_wd < ccdcparam->data_sz)) {
			dev_dbg(ccdc_cfg.dev, "\nInvalid data line select");
			return -1;
		}
	}
	return 0;
}

static int ccdc_update_raw_params(struct ccdc_config_params_raw *raw_params)
{
	struct ccdc_config_params_raw *config_params =
				&ccdc_cfg.bayer.config_params;
	unsigned int *fpc_virtaddr = NULL;
	unsigned int *fpc_physaddr = NULL;

	memcpy(config_params, raw_params, sizeof(*raw_params));
	if (!config_params->fault_pxl.enable)
		return 0;

	fpc_physaddr = (unsigned int *)config_params->fault_pxl.fpc_table_addr;
	fpc_virtaddr = (unsigned int *)phys_to_virt(
				(unsigned long)fpc_physaddr);
	if (raw_params->fault_pxl.fp_num != config_params->fault_pxl.fp_num) {
		if (fpc_physaddr != NULL) {
			free_pages((unsigned long)fpc_physaddr,
				   get_order
				   (config_params->fault_pxl.fp_num *
				   FP_NUM_BYTES));
		}

		
		fpc_virtaddr =
			(unsigned int *)__get_free_pages(GFP_KERNEL | GFP_DMA,
							 get_order(raw_params->
							 fault_pxl.fp_num *
							 FP_NUM_BYTES));

		if (fpc_virtaddr == NULL) {
			dev_dbg(ccdc_cfg.dev,
				"\nUnable to allocate memory for FPC");
			return -EFAULT;
		}
		fpc_physaddr =
		    (unsigned int *)virt_to_phys((void *)fpc_virtaddr);
	}

	
	config_params->fault_pxl.fp_num = raw_params->fault_pxl.fp_num;
	if (copy_from_user(fpc_virtaddr,
			(void __user *)raw_params->fault_pxl.fpc_table_addr,
			config_params->fault_pxl.fp_num * FP_NUM_BYTES)) {
		dev_dbg(ccdc_cfg.dev, "\n copy_from_user failed");
		return -EFAULT;
	}
	config_params->fault_pxl.fpc_table_addr = (unsigned int)fpc_physaddr;
	return 0;
}

static int ccdc_close(struct device *dev)
{
	struct ccdc_config_params_raw *config_params =
				&ccdc_cfg.bayer.config_params;
	unsigned int *fpc_physaddr = NULL, *fpc_virtaddr = NULL;

	fpc_physaddr = (unsigned int *)config_params->fault_pxl.fpc_table_addr;

	if (fpc_physaddr != NULL) {
		fpc_virtaddr = (unsigned int *)
		    phys_to_virt((unsigned long)fpc_physaddr);
		free_pages((unsigned long)fpc_virtaddr,
			   get_order(config_params->fault_pxl.fp_num *
			   FP_NUM_BYTES));
	}
	return 0;
}

static void ccdc_restore_defaults(void)
{
	int i;

	
	ccdc_enable(0);
	
	for (i = 4; i <= 0x94; i += 4)
		regw(0,  i);
	regw(CCDC_NO_CULLING, CCDC_CULLING);
	regw(CCDC_GAMMA_BITS_11_2, CCDC_ALAW);
}

static int ccdc_open(struct device *device)
{
	ccdc_restore_defaults();
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		ccdc_enable_vport(1);
	return 0;
}

static void ccdc_sbl_reset(void)
{
	vpss_clear_wbl_overflow(VPSS_PCR_CCDC_WBL_O);
}

static int ccdc_set_params(void __user *params)
{
	struct ccdc_config_params_raw ccdc_raw_params;
	int x;

	if (ccdc_cfg.if_type != VPFE_RAW_BAYER)
		return -EINVAL;

	x = copy_from_user(&ccdc_raw_params, params, sizeof(ccdc_raw_params));
	if (x) {
		dev_dbg(ccdc_cfg.dev, "ccdc_set_params: error in copying"
			   "ccdc params, %d\n", x);
		return -EFAULT;
	}

	if (!validate_ccdc_param(&ccdc_raw_params)) {
		if (!ccdc_update_raw_params(&ccdc_raw_params))
			return 0;
	}
	return -EINVAL;
}

void ccdc_config_ycbcr(void)
{
	struct ccdc_params_ycbcr *params = &ccdc_cfg.ycbcr;
	u32 syn_mode;

	dev_dbg(ccdc_cfg.dev, "\nStarting ccdc_config_ycbcr...");
	ccdc_restore_defaults();

	syn_mode = (((params->pix_fmt & CCDC_SYN_MODE_INPMOD_MASK) <<
		    CCDC_SYN_MODE_INPMOD_SHIFT) |
		    ((params->frm_fmt & CCDC_SYN_FLDMODE_MASK) <<
		    CCDC_SYN_FLDMODE_SHIFT) | CCDC_VDHDEN_ENABLE |
		    CCDC_WEN_ENABLE | CCDC_DATA_PACK_ENABLE);

	
	if (params->bt656_enable) {
		regw(CCDC_REC656IF_BT656_EN, CCDC_REC656IF);

		syn_mode |= CCDC_SYN_MODE_VD_POL_NEGATIVE;
		if (ccdc_cfg.if_type == VPFE_BT656_10BIT)
			syn_mode |= CCDC_SYN_MODE_10BITS;
		else
			syn_mode |= CCDC_SYN_MODE_8BITS;
	} else {
		
		syn_mode |= (((params->fid_pol & CCDC_FID_POL_MASK) <<
			     CCDC_FID_POL_SHIFT) |
			     ((params->hd_pol & CCDC_HD_POL_MASK) <<
			     CCDC_HD_POL_SHIFT) |
			     ((params->vd_pol & CCDC_VD_POL_MASK) <<
			     CCDC_VD_POL_SHIFT));
	}
	regw(syn_mode, CCDC_SYN_MODE);

	
	ccdc_setwin(&params->win, params->frm_fmt, 2);

	if (ccdc_cfg.if_type == VPFE_BT656_10BIT)
		regw((params->pix_order << CCDC_CCDCFG_Y8POS_SHIFT) |
			CCDC_LATCH_ON_VSYNC_DISABLE | CCDC_CCDCFG_BW656_10BIT,
			CCDC_CCDCFG);
	else
		regw((params->pix_order << CCDC_CCDCFG_Y8POS_SHIFT) |
			CCDC_LATCH_ON_VSYNC_DISABLE, CCDC_CCDCFG);

	regw(((params->win.width * 2  + 31) & ~0x1f), CCDC_HSIZE_OFF);

	
	if (params->buf_type == CCDC_BUFTYPE_FLD_INTERLEAVED)
		
		regw(CCDC_SDOFST_FIELD_INTERLEAVED, CCDC_SDOFST);

	ccdc_sbl_reset();
	dev_dbg(ccdc_cfg.dev, "\nEnd of ccdc_config_ycbcr...\n");
}

static void ccdc_config_black_clamp(struct ccdc_black_clamp *bclamp)
{
	u32 val;

	if (!bclamp->enable) {
		
		val = (bclamp->dc_sub) & CCDC_BLK_DC_SUB_MASK;
		regw(val, CCDC_DCSUB);
		dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to DCSUB...\n", val);
		regw(CCDC_CLAMP_DEFAULT_VAL, CCDC_CLAMP);
		dev_dbg(ccdc_cfg.dev, "\nWriting 0x0000 to CLAMP...\n");
		return;
	}
	val = ((bclamp->sgain & CCDC_BLK_SGAIN_MASK) |
	       ((bclamp->start_pixel & CCDC_BLK_ST_PXL_MASK) <<
		CCDC_BLK_ST_PXL_SHIFT) |
	       ((bclamp->sample_ln & CCDC_BLK_SAMPLE_LINE_MASK) <<
		CCDC_BLK_SAMPLE_LINE_SHIFT) |
	       ((bclamp->sample_pixel & CCDC_BLK_SAMPLE_LN_MASK) <<
		CCDC_BLK_SAMPLE_LN_SHIFT) | CCDC_BLK_CLAMP_ENABLE);
	regw(val, CCDC_CLAMP);
	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to CLAMP...\n", val);
	
	regw(CCDC_DCSUB_DEFAULT_VAL, CCDC_DCSUB);
	dev_dbg(ccdc_cfg.dev, "\nWriting 0x00000000 to DCSUB...\n");
}

static void ccdc_config_black_compense(struct ccdc_black_compensation *bcomp)
{
	u32 val;

	val = ((bcomp->b & CCDC_BLK_COMP_MASK) |
	      ((bcomp->gb & CCDC_BLK_COMP_MASK) <<
	       CCDC_BLK_COMP_GB_COMP_SHIFT) |
	      ((bcomp->gr & CCDC_BLK_COMP_MASK) <<
	       CCDC_BLK_COMP_GR_COMP_SHIFT) |
	      ((bcomp->r & CCDC_BLK_COMP_MASK) <<
	       CCDC_BLK_COMP_R_COMP_SHIFT));
	regw(val, CCDC_BLKCMP);
}

static void ccdc_config_fpc(struct ccdc_fault_pixel *fpc)
{
	u32 val;

	
	val = CCDC_FPC_DISABLE;
	regw(val, CCDC_FPC);

	if (!fpc->enable)
		return;

	
	regw(fpc->fpc_table_addr, CCDC_FPC_ADDR);
	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to FPC_ADDR...\n",
		       (fpc->fpc_table_addr));
	
	val = fpc->fp_num & CCDC_FPC_FPC_NUM_MASK;
	regw(val, CCDC_FPC);

	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to FPC...\n", val);
	
	val = regr(CCDC_FPC) | CCDC_FPC_ENABLE;
	regw(val, CCDC_FPC);
	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to FPC...\n", val);
}

void ccdc_config_raw(void)
{
	struct ccdc_params_raw *params = &ccdc_cfg.bayer;
	struct ccdc_config_params_raw *config_params =
				&ccdc_cfg.bayer.config_params;
	unsigned int syn_mode = 0;
	unsigned int val;

	dev_dbg(ccdc_cfg.dev, "\nStarting ccdc_config_raw...");

	
	ccdc_restore_defaults();

	
	regw(CCDC_LATCH_ON_VSYNC_DISABLE, CCDC_CCDCFG);

	syn_mode =
		(((params->vd_pol & CCDC_VD_POL_MASK) << CCDC_VD_POL_SHIFT) |
		((params->hd_pol & CCDC_HD_POL_MASK) << CCDC_HD_POL_SHIFT) |
		((params->fid_pol & CCDC_FID_POL_MASK) << CCDC_FID_POL_SHIFT) |
		((params->frm_fmt & CCDC_FRM_FMT_MASK) << CCDC_FRM_FMT_SHIFT) |
		((config_params->data_sz & CCDC_DATA_SZ_MASK) <<
		CCDC_DATA_SZ_SHIFT) |
		((params->pix_fmt & CCDC_PIX_FMT_MASK) << CCDC_PIX_FMT_SHIFT) |
		CCDC_WEN_ENABLE | CCDC_VDHDEN_ENABLE);

	
	if (config_params->alaw.enable) {
		val = ((config_params->alaw.gama_wd &
		      CCDC_ALAW_GAMA_WD_MASK) | CCDC_ALAW_ENABLE);
		regw(val, CCDC_ALAW);
		dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to ALAW...\n", val);
	}

	
	ccdc_setwin(&params->win, params->frm_fmt, CCDC_PPC_RAW);

	
	ccdc_config_black_clamp(&config_params->blk_clamp);

	
	ccdc_config_black_compense(&config_params->blk_comp);

	
	ccdc_config_fpc(&config_params->fault_pxl);

	
	if ((config_params->data_sz == CCDC_DATA_8BITS) ||
	     config_params->alaw.enable)
		syn_mode |= CCDC_DATA_PACK_ENABLE;

#ifdef CONFIG_DM644X_VIDEO_PORT_ENABLE
	
	val = CCDC_ENABLE_VIDEO_PORT;
#else
	
	val = CCDC_DISABLE_VIDEO_PORT;
#endif

	if (config_params->data_sz == CCDC_DATA_8BITS)
		val |= (CCDC_DATA_10BITS & CCDC_FMTCFG_VPIN_MASK)
		    << CCDC_FMTCFG_VPIN_SHIFT;
	else
		val |= (config_params->data_sz & CCDC_FMTCFG_VPIN_MASK)
		    << CCDC_FMTCFG_VPIN_SHIFT;
	
	regw(val, CCDC_FMTCFG);

	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to FMTCFG...\n", val);
	
	regw(CCDC_COLPTN_VAL, CCDC_COLPTN);

	dev_dbg(ccdc_cfg.dev, "\nWriting 0xBB11BB11 to COLPTN...\n");
	val = ((params->win.left & CCDC_FMT_HORZ_FMTSPH_MASK) <<
	      CCDC_FMT_HORZ_FMTSPH_SHIFT) |
	      (params->win.width & CCDC_FMT_HORZ_FMTLNH_MASK);
	regw(val, CCDC_FMT_HORZ);

	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to FMT_HORZ...\n", val);
	val = (params->win.top & CCDC_FMT_VERT_FMTSLV_MASK)
	    << CCDC_FMT_VERT_FMTSLV_SHIFT;
	if (params->frm_fmt == CCDC_FRMFMT_PROGRESSIVE)
		val |= (params->win.height) & CCDC_FMT_VERT_FMTLNV_MASK;
	else
		val |= (params->win.height >> 1) & CCDC_FMT_VERT_FMTLNV_MASK;

	dev_dbg(ccdc_cfg.dev, "\nparams->win.height  0x%x ...\n",
	       params->win.height);
	regw(val, CCDC_FMT_VERT);

	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to FMT_VERT...\n", val);

	dev_dbg(ccdc_cfg.dev, "\nbelow regw(val, FMT_VERT)...");

	if ((config_params->data_sz == CCDC_DATA_8BITS) ||
	    config_params->alaw.enable)
		regw((params->win.width + CCDC_32BYTE_ALIGN_VAL) &
		    CCDC_HSIZE_OFF_MASK, CCDC_HSIZE_OFF);
	else
		
		regw(((params->win.width * CCDC_TWO_BYTES_PER_PIXEL) +
		    CCDC_32BYTE_ALIGN_VAL) & CCDC_HSIZE_OFF_MASK,
		    CCDC_HSIZE_OFF);

	
	if (params->frm_fmt == CCDC_FRMFMT_INTERLACED) {
		if (params->image_invert_enable) {
			
			regw(CCDC_INTERLACED_IMAGE_INVERT, CCDC_SDOFST);
			dev_dbg(ccdc_cfg.dev, "\nWriting 0x4B6D to SDOFST..\n");
		}

		else {
			
			regw(CCDC_INTERLACED_NO_IMAGE_INVERT, CCDC_SDOFST);
			dev_dbg(ccdc_cfg.dev, "\nWriting 0x0249 to SDOFST..\n");
		}
	} else if (params->frm_fmt == CCDC_FRMFMT_PROGRESSIVE) {
		regw(CCDC_PROGRESSIVE_NO_IMAGE_INVERT, CCDC_SDOFST);
		dev_dbg(ccdc_cfg.dev, "\nWriting 0x0000 to SDOFST...\n");
	}

	if (params->frm_fmt == CCDC_FRMFMT_PROGRESSIVE)
		val = (((params->win.height - 1) & CCDC_VP_OUT_VERT_NUM_MASK))
		    << CCDC_VP_OUT_VERT_NUM_SHIFT;
	else
		val =
		    ((((params->win.height >> CCDC_INTERLACED_HEIGHT_SHIFT) -
		     1) & CCDC_VP_OUT_VERT_NUM_MASK)) <<
		    CCDC_VP_OUT_VERT_NUM_SHIFT;

	val |= ((((params->win.width))) & CCDC_VP_OUT_HORZ_NUM_MASK)
	    << CCDC_VP_OUT_HORZ_NUM_SHIFT;
	val |= (params->win.left) & CCDC_VP_OUT_HORZ_ST_MASK;
	regw(val, CCDC_VP_OUT);

	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to VP_OUT...\n", val);
	regw(syn_mode, CCDC_SYN_MODE);
	dev_dbg(ccdc_cfg.dev, "\nWriting 0x%x to SYN_MODE...\n", syn_mode);

	ccdc_sbl_reset();
	dev_dbg(ccdc_cfg.dev, "\nend of ccdc_config_raw...");
	ccdc_readregs();
}

static int ccdc_configure(void)
{
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER)
		ccdc_config_raw();
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
	if (ccdc_cfg.if_type == VPFE_RAW_BAYER) {
		ccdc_cfg.bayer.pix_fmt = CCDC_PIXFMT_RAW;
		if (pixfmt == V4L2_PIX_FMT_SBGGR8)
			ccdc_cfg.bayer.config_params.alaw.enable = 1;
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
	return (regr(CCDC_SYN_MODE) >> 15) & 1;
}

static inline void ccdc_setfbaddr(unsigned long addr)
{
	regw(addr & 0xffffffe0, CCDC_SDR_ADDR);
}

static int ccdc_set_hw_if_params(struct vpfe_hw_if_param *params)
{
	ccdc_cfg.if_type = params->if_type;

	switch (params->if_type) {
	case VPFE_BT656:
	case VPFE_YCBCR_SYNC_16:
	case VPFE_YCBCR_SYNC_8:
	case VPFE_BT656_10BIT:
		ccdc_cfg.ycbcr.vd_pol = params->vdpol;
		ccdc_cfg.ycbcr.hd_pol = params->hdpol;
		break;
	default:
		
		return -EINVAL;
	}
	return 0;
}

static void ccdc_save_context(void)
{
	ccdc_ctx[CCDC_PCR >> 2] = regr(CCDC_PCR);
	ccdc_ctx[CCDC_SYN_MODE >> 2] = regr(CCDC_SYN_MODE);
	ccdc_ctx[CCDC_HD_VD_WID >> 2] = regr(CCDC_HD_VD_WID);
	ccdc_ctx[CCDC_PIX_LINES >> 2] = regr(CCDC_PIX_LINES);
	ccdc_ctx[CCDC_HORZ_INFO >> 2] = regr(CCDC_HORZ_INFO);
	ccdc_ctx[CCDC_VERT_START >> 2] = regr(CCDC_VERT_START);
	ccdc_ctx[CCDC_VERT_LINES >> 2] = regr(CCDC_VERT_LINES);
	ccdc_ctx[CCDC_CULLING >> 2] = regr(CCDC_CULLING);
	ccdc_ctx[CCDC_HSIZE_OFF >> 2] = regr(CCDC_HSIZE_OFF);
	ccdc_ctx[CCDC_SDOFST >> 2] = regr(CCDC_SDOFST);
	ccdc_ctx[CCDC_SDR_ADDR >> 2] = regr(CCDC_SDR_ADDR);
	ccdc_ctx[CCDC_CLAMP >> 2] = regr(CCDC_CLAMP);
	ccdc_ctx[CCDC_DCSUB >> 2] = regr(CCDC_DCSUB);
	ccdc_ctx[CCDC_COLPTN >> 2] = regr(CCDC_COLPTN);
	ccdc_ctx[CCDC_BLKCMP >> 2] = regr(CCDC_BLKCMP);
	ccdc_ctx[CCDC_FPC >> 2] = regr(CCDC_FPC);
	ccdc_ctx[CCDC_FPC_ADDR >> 2] = regr(CCDC_FPC_ADDR);
	ccdc_ctx[CCDC_VDINT >> 2] = regr(CCDC_VDINT);
	ccdc_ctx[CCDC_ALAW >> 2] = regr(CCDC_ALAW);
	ccdc_ctx[CCDC_REC656IF >> 2] = regr(CCDC_REC656IF);
	ccdc_ctx[CCDC_CCDCFG >> 2] = regr(CCDC_CCDCFG);
	ccdc_ctx[CCDC_FMTCFG >> 2] = regr(CCDC_FMTCFG);
	ccdc_ctx[CCDC_FMT_HORZ >> 2] = regr(CCDC_FMT_HORZ);
	ccdc_ctx[CCDC_FMT_VERT >> 2] = regr(CCDC_FMT_VERT);
	ccdc_ctx[CCDC_FMT_ADDR0 >> 2] = regr(CCDC_FMT_ADDR0);
	ccdc_ctx[CCDC_FMT_ADDR1 >> 2] = regr(CCDC_FMT_ADDR1);
	ccdc_ctx[CCDC_FMT_ADDR2 >> 2] = regr(CCDC_FMT_ADDR2);
	ccdc_ctx[CCDC_FMT_ADDR3 >> 2] = regr(CCDC_FMT_ADDR3);
	ccdc_ctx[CCDC_FMT_ADDR4 >> 2] = regr(CCDC_FMT_ADDR4);
	ccdc_ctx[CCDC_FMT_ADDR5 >> 2] = regr(CCDC_FMT_ADDR5);
	ccdc_ctx[CCDC_FMT_ADDR6 >> 2] = regr(CCDC_FMT_ADDR6);
	ccdc_ctx[CCDC_FMT_ADDR7 >> 2] = regr(CCDC_FMT_ADDR7);
	ccdc_ctx[CCDC_PRGEVEN_0 >> 2] = regr(CCDC_PRGEVEN_0);
	ccdc_ctx[CCDC_PRGEVEN_1 >> 2] = regr(CCDC_PRGEVEN_1);
	ccdc_ctx[CCDC_PRGODD_0 >> 2] = regr(CCDC_PRGODD_0);
	ccdc_ctx[CCDC_PRGODD_1 >> 2] = regr(CCDC_PRGODD_1);
	ccdc_ctx[CCDC_VP_OUT >> 2] = regr(CCDC_VP_OUT);
}

static void ccdc_restore_context(void)
{
	regw(ccdc_ctx[CCDC_SYN_MODE >> 2], CCDC_SYN_MODE);
	regw(ccdc_ctx[CCDC_HD_VD_WID >> 2], CCDC_HD_VD_WID);
	regw(ccdc_ctx[CCDC_PIX_LINES >> 2], CCDC_PIX_LINES);
	regw(ccdc_ctx[CCDC_HORZ_INFO >> 2], CCDC_HORZ_INFO);
	regw(ccdc_ctx[CCDC_VERT_START >> 2], CCDC_VERT_START);
	regw(ccdc_ctx[CCDC_VERT_LINES >> 2], CCDC_VERT_LINES);
	regw(ccdc_ctx[CCDC_CULLING >> 2], CCDC_CULLING);
	regw(ccdc_ctx[CCDC_HSIZE_OFF >> 2], CCDC_HSIZE_OFF);
	regw(ccdc_ctx[CCDC_SDOFST >> 2], CCDC_SDOFST);
	regw(ccdc_ctx[CCDC_SDR_ADDR >> 2], CCDC_SDR_ADDR);
	regw(ccdc_ctx[CCDC_CLAMP >> 2], CCDC_CLAMP);
	regw(ccdc_ctx[CCDC_DCSUB >> 2], CCDC_DCSUB);
	regw(ccdc_ctx[CCDC_COLPTN >> 2], CCDC_COLPTN);
	regw(ccdc_ctx[CCDC_BLKCMP >> 2], CCDC_BLKCMP);
	regw(ccdc_ctx[CCDC_FPC >> 2], CCDC_FPC);
	regw(ccdc_ctx[CCDC_FPC_ADDR >> 2], CCDC_FPC_ADDR);
	regw(ccdc_ctx[CCDC_VDINT >> 2], CCDC_VDINT);
	regw(ccdc_ctx[CCDC_ALAW >> 2], CCDC_ALAW);
	regw(ccdc_ctx[CCDC_REC656IF >> 2], CCDC_REC656IF);
	regw(ccdc_ctx[CCDC_CCDCFG >> 2], CCDC_CCDCFG);
	regw(ccdc_ctx[CCDC_FMTCFG >> 2], CCDC_FMTCFG);
	regw(ccdc_ctx[CCDC_FMT_HORZ >> 2], CCDC_FMT_HORZ);
	regw(ccdc_ctx[CCDC_FMT_VERT >> 2], CCDC_FMT_VERT);
	regw(ccdc_ctx[CCDC_FMT_ADDR0 >> 2], CCDC_FMT_ADDR0);
	regw(ccdc_ctx[CCDC_FMT_ADDR1 >> 2], CCDC_FMT_ADDR1);
	regw(ccdc_ctx[CCDC_FMT_ADDR2 >> 2], CCDC_FMT_ADDR2);
	regw(ccdc_ctx[CCDC_FMT_ADDR3 >> 2], CCDC_FMT_ADDR3);
	regw(ccdc_ctx[CCDC_FMT_ADDR4 >> 2], CCDC_FMT_ADDR4);
	regw(ccdc_ctx[CCDC_FMT_ADDR5 >> 2], CCDC_FMT_ADDR5);
	regw(ccdc_ctx[CCDC_FMT_ADDR6 >> 2], CCDC_FMT_ADDR6);
	regw(ccdc_ctx[CCDC_FMT_ADDR7 >> 2], CCDC_FMT_ADDR7);
	regw(ccdc_ctx[CCDC_PRGEVEN_0 >> 2], CCDC_PRGEVEN_0);
	regw(ccdc_ctx[CCDC_PRGEVEN_1 >> 2], CCDC_PRGEVEN_1);
	regw(ccdc_ctx[CCDC_PRGODD_0 >> 2], CCDC_PRGODD_0);
	regw(ccdc_ctx[CCDC_PRGODD_1 >> 2], CCDC_PRGODD_1);
	regw(ccdc_ctx[CCDC_VP_OUT >> 2], CCDC_VP_OUT);
	regw(ccdc_ctx[CCDC_PCR >> 2], CCDC_PCR);
}
static struct ccdc_hw_device ccdc_hw_dev = {
	.name = "DM6446 CCDC",
	.owner = THIS_MODULE,
	.hw_ops = {
		.open = ccdc_open,
		.close = ccdc_close,
		.reset = ccdc_sbl_reset,
		.enable = ccdc_enable,
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

static int __init dm644x_ccdc_probe(struct platform_device *pdev)
{
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

static int dm644x_ccdc_remove(struct platform_device *pdev)
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

static int dm644x_ccdc_suspend(struct device *dev)
{
	
	ccdc_save_context();
	
	ccdc_enable(0);
	
	clk_disable(ccdc_cfg.mclk);
	clk_disable(ccdc_cfg.sclk);

	return 0;
}

static int dm644x_ccdc_resume(struct device *dev)
{
	
	clk_enable(ccdc_cfg.mclk);
	clk_enable(ccdc_cfg.sclk);
	
	ccdc_restore_context();

	return 0;
}

static const struct dev_pm_ops dm644x_ccdc_pm_ops = {
	.suspend = dm644x_ccdc_suspend,
	.resume = dm644x_ccdc_resume,
};

static struct platform_driver dm644x_ccdc_driver = {
	.driver = {
		.name	= "dm644x_ccdc",
		.owner = THIS_MODULE,
		.pm = &dm644x_ccdc_pm_ops,
	},
	.remove = __devexit_p(dm644x_ccdc_remove),
	.probe = dm644x_ccdc_probe,
};

module_platform_driver(dm644x_ccdc_driver);
