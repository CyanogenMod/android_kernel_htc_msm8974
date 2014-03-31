/*
 * Copyright (C) 2008-2009 Texas Instruments Inc
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
 * Image Sensor Interface (ISIF) driver
 *
 * This driver is for configuring the ISIF IP available on DM365 or any other
 * TI SoCs. This is used for capturing yuv or bayer video or image data
 * from a decoder or sensor. This IP is similar to the CCDC IP on DM355
 * and DM6446, but with enhanced or additional ip blocks. The driver
 * configures the ISIF upon commands from the vpfe bridge driver through
 * ccdc_hw_device interface.
 *
 * TODO: 1) Raw bayer parameter settings and bayer capture
 *	 2) Add support for control ioctl
 */
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>

#include <mach/mux.h>

#include <media/davinci/isif.h>
#include <media/davinci/vpss.h>

#include "isif_regs.h"
#include "ccdc_hw_device.h"

static struct isif_config_params_raw isif_config_defaults = {
	.linearize = {
		.en = 0,
		.corr_shft = ISIF_NO_SHIFT,
		.scale_fact = {1, 0},
	},
	.df_csc = {
		.df_or_csc = 0,
		.csc = {
			.en = 0,
		},
	},
	.dfc = {
		.en = 0,
	},
	.bclamp = {
		.en = 0,
	},
	.gain_offset = {
		.gain = {
			.r_ye = {1, 0},
			.gr_cy = {1, 0},
			.gb_g = {1, 0},
			.b_mg = {1, 0},
		},
	},
	.culling = {
		.hcpat_odd = 0xff,
		.hcpat_even = 0xff,
		.vcpat = 0xff,
	},
	.compress = {
		.alg = ISIF_ALAW,
	},
};

static struct isif_oper_config {
	struct device *dev;
	enum vpfe_hw_if_type if_type;
	struct isif_ycbcr_config ycbcr;
	struct isif_params_raw bayer;
	enum isif_data_pack data_pack;
	
	struct clk *mclk;
	
	void __iomem *base_addr;
	
	void __iomem *linear_tbl0_addr;
	
	void __iomem *linear_tbl1_addr;
} isif_cfg = {
	.ycbcr = {
		.pix_fmt = CCDC_PIXFMT_YCBCR_8BIT,
		.frm_fmt = CCDC_FRMFMT_INTERLACED,
		.win = ISIF_WIN_NTSC,
		.fid_pol = VPFE_PINPOL_POSITIVE,
		.vd_pol = VPFE_PINPOL_POSITIVE,
		.hd_pol = VPFE_PINPOL_POSITIVE,
		.pix_order = CCDC_PIXORDER_CBYCRY,
		.buf_type = CCDC_BUFTYPE_FLD_INTERLEAVED,
	},
	.bayer = {
		.pix_fmt = CCDC_PIXFMT_RAW,
		.frm_fmt = CCDC_FRMFMT_PROGRESSIVE,
		.win = ISIF_WIN_VGA,
		.fid_pol = VPFE_PINPOL_POSITIVE,
		.vd_pol = VPFE_PINPOL_POSITIVE,
		.hd_pol = VPFE_PINPOL_POSITIVE,
		.gain = {
			.r_ye = {1, 0},
			.gr_cy = {1, 0},
			.gb_g = {1, 0},
			.b_mg = {1, 0},
		},
		.cfa_pat = ISIF_CFA_PAT_MOSAIC,
		.data_msb = ISIF_BIT_MSB_11,
		.config_params = {
			.data_shift = ISIF_NO_SHIFT,
			.col_pat_field0 = {
				.olop = ISIF_GREEN_BLUE,
				.olep = ISIF_BLUE,
				.elop = ISIF_RED,
				.elep = ISIF_GREEN_RED,
			},
			.col_pat_field1 = {
				.olop = ISIF_GREEN_BLUE,
				.olep = ISIF_BLUE,
				.elop = ISIF_RED,
				.elep = ISIF_GREEN_RED,
			},
			.test_pat_gen = 0,
		},
	},
	.data_pack = ISIF_DATA_PACK8,
};

static const u32 isif_raw_bayer_pix_formats[] = {
	V4L2_PIX_FMT_SBGGR8, V4L2_PIX_FMT_SBGGR16};

static const u32 isif_raw_yuv_pix_formats[] = {
	V4L2_PIX_FMT_UYVY, V4L2_PIX_FMT_YUYV};

static inline u32 regr(u32 offset)
{
	return __raw_readl(isif_cfg.base_addr + offset);
}

static inline void regw(u32 val, u32 offset)
{
	__raw_writel(val, isif_cfg.base_addr + offset);
}

static inline u32 reg_modify(u32 mask, u32 val, u32 offset)
{
	u32 new_val = (regr(offset) & ~mask) | (val & mask);

	regw(new_val, offset);
	return new_val;
}

static inline void regw_lin_tbl(u32 val, u32 offset, int i)
{
	if (!i)
		__raw_writel(val, isif_cfg.linear_tbl0_addr + offset);
	else
		__raw_writel(val, isif_cfg.linear_tbl1_addr + offset);
}

static void isif_disable_all_modules(void)
{
	
	regw(0, CLAMPCFG);
	
	regw(0, DFCCTL);
	
	regw(0, CSCCTL);
	
	regw(0, LINCFG0);
	
}

static void isif_enable(int en)
{
	if (!en) {
		
		isif_disable_all_modules();
		msleep(100);
	}
	reg_modify(ISIF_SYNCEN_VDHDEN_MASK, en, SYNCEN);
}

static void isif_enable_output_to_sdram(int en)
{
	reg_modify(ISIF_SYNCEN_WEN_MASK, en << ISIF_SYNCEN_WEN_SHIFT, SYNCEN);
}

static void isif_config_culling(struct isif_cul *cul)
{
	u32 val;

	
	val = (cul->hcpat_even << CULL_PAT_EVEN_LINE_SHIFT) | cul->hcpat_odd;
	regw(val, CULH);

	
	regw(cul->vcpat, CULV);

	
	reg_modify(ISIF_LPF_MASK << ISIF_LPF_SHIFT,
		  cul->en_lpf << ISIF_LPF_SHIFT, MODESET);
}

static void isif_config_gain_offset(void)
{
	struct isif_gain_offsets_adj *gain_off_p =
		&isif_cfg.bayer.config_params.gain_offset;
	u32 val;

	val = (!!gain_off_p->gain_sdram_en << GAIN_SDRAM_EN_SHIFT) |
	      (!!gain_off_p->gain_ipipe_en << GAIN_IPIPE_EN_SHIFT) |
	      (!!gain_off_p->gain_h3a_en << GAIN_H3A_EN_SHIFT) |
	      (!!gain_off_p->offset_sdram_en << OFST_SDRAM_EN_SHIFT) |
	      (!!gain_off_p->offset_ipipe_en << OFST_IPIPE_EN_SHIFT) |
	      (!!gain_off_p->offset_h3a_en << OFST_H3A_EN_SHIFT);

	reg_modify(GAIN_OFFSET_EN_MASK, val, CGAMMAWD);

	val = (gain_off_p->gain.r_ye.integer << GAIN_INTEGER_SHIFT) |
	       gain_off_p->gain.r_ye.decimal;
	regw(val, CRGAIN);

	val = (gain_off_p->gain.gr_cy.integer << GAIN_INTEGER_SHIFT) |
	       gain_off_p->gain.gr_cy.decimal;
	regw(val, CGRGAIN);

	val = (gain_off_p->gain.gb_g.integer << GAIN_INTEGER_SHIFT) |
	       gain_off_p->gain.gb_g.decimal;
	regw(val, CGBGAIN);

	val = (gain_off_p->gain.b_mg.integer << GAIN_INTEGER_SHIFT) |
	       gain_off_p->gain.b_mg.decimal;
	regw(val, CBGAIN);

	regw(gain_off_p->offset, COFSTA);
}

static void isif_restore_defaults(void)
{
	enum vpss_ccdc_source_sel source = VPSS_CCDCIN;

	dev_dbg(isif_cfg.dev, "\nstarting isif_restore_defaults...");
	isif_cfg.bayer.config_params = isif_config_defaults;
	
	vpss_enable_clock(VPSS_CCDC_CLOCK, 1);
	vpss_enable_clock(VPSS_IPIPEIF_CLOCK, 1);
	vpss_enable_clock(VPSS_BL_CLOCK, 1);
	
	isif_config_gain_offset();
	vpss_select_ccdc_source(source);
	dev_dbg(isif_cfg.dev, "\nEnd of isif_restore_defaults...");
}

static int isif_open(struct device *device)
{
	isif_restore_defaults();
	return 0;
}

static void isif_setwin(struct v4l2_rect *image_win,
			enum ccdc_frmfmt frm_fmt, int ppc)
{
	int horz_start, horz_nr_pixels;
	int vert_start, vert_nr_lines;
	int mid_img = 0;

	dev_dbg(isif_cfg.dev, "\nStarting isif_setwin...");
	horz_start = image_win->left << (ppc - 1);
	horz_nr_pixels = ((image_win->width) << (ppc - 1)) - 1;

	
	regw(horz_start & START_PX_HOR_MASK, SPH);
	regw(horz_nr_pixels & NUM_PX_HOR_MASK, LNH);
	vert_start = image_win->top;

	if (frm_fmt == CCDC_FRMFMT_INTERLACED) {
		vert_nr_lines = (image_win->height >> 1) - 1;
		vert_start >>= 1;
		
		vert_start += 1;
	} else {
		
		vert_start += 1;
		vert_nr_lines = image_win->height - 1;
		
		mid_img = vert_start + (image_win->height / 2);
		regw(mid_img, VDINT1);
	}

	regw(0, VDINT0);
	regw(vert_start & START_VER_ONE_MASK, SLV0);
	regw(vert_start & START_VER_TWO_MASK, SLV1);
	regw(vert_nr_lines & NUM_LINES_VER, LNV);
}

static void isif_config_bclamp(struct isif_black_clamp *bc)
{
	u32 val;

	regw(bc->dc_offset, CLDCOFST);

	if (bc->en) {
		val = bc->bc_mode_color << ISIF_BC_MODE_COLOR_SHIFT;

		
		val = val | 1 | (bc->horz.mode << ISIF_HORZ_BC_MODE_SHIFT);

		regw(val, CLAMPCFG);

		if (bc->horz.mode != ISIF_HORZ_BC_DISABLE) {
			val = bc->horz.win_count_calc |
			      ((!!bc->horz.base_win_sel_calc) <<
				ISIF_HORZ_BC_WIN_SEL_SHIFT) |
			      ((!!bc->horz.clamp_pix_limit) <<
				ISIF_HORZ_BC_PIX_LIMIT_SHIFT) |
			      (bc->horz.win_h_sz_calc <<
				ISIF_HORZ_BC_WIN_H_SIZE_SHIFT) |
			      (bc->horz.win_v_sz_calc <<
				ISIF_HORZ_BC_WIN_V_SIZE_SHIFT);
			regw(val, CLHWIN0);

			regw(bc->horz.win_start_h_calc, CLHWIN1);
			regw(bc->horz.win_start_v_calc, CLHWIN2);
		}

		

		
		val |=
		(bc->vert.reset_val_sel << ISIF_VERT_BC_RST_VAL_SEL_SHIFT) |
		(bc->vert.line_ave_coef << ISIF_VERT_BC_LINE_AVE_COEF_SHIFT);
		regw(val, CLVWIN0);

		
		regw(bc->vert.ob_start_h, CLVWIN1);
		
		regw(bc->vert.ob_start_v, CLVWIN2);
		
		regw(bc->vert.ob_v_sz_calc, CLVWIN3);
		
		regw(bc->vert_start_sub, CLSV);
	}
}

static void isif_config_linearization(struct isif_linearize *linearize)
{
	u32 val, i;

	if (!linearize->en) {
		regw(0, LINCFG0);
		return;
	}

	
	val = (linearize->corr_shft << ISIF_LIN_CORRSFT_SHIFT) | 1;
	regw(val, LINCFG0);

	
	val = ((!!linearize->scale_fact.integer) <<
	       ISIF_LIN_SCALE_FACT_INTEG_SHIFT) |
	       linearize->scale_fact.decimal;
	regw(val, LINCFG1);

	for (i = 0; i < ISIF_LINEAR_TAB_SIZE; i++) {
		if (i % 2)
			regw_lin_tbl(linearize->table[i], ((i >> 1) << 2), 1);
		else
			regw_lin_tbl(linearize->table[i], ((i >> 1) << 2), 0);
	}
}

static int isif_config_dfc(struct isif_dfc *vdfc)
{
	
	u32 val, count, retries = loops_per_jiffy / (4000/HZ);
	int i;

	if (!vdfc->en)
		return 0;

	
	val = (vdfc->corr_mode << ISIF_VDFC_CORR_MOD_SHIFT);

	
	if (vdfc->corr_whole_line)
		val |= 1 << ISIF_VDFC_CORR_WHOLE_LN_SHIFT;

	
	val |= vdfc->def_level_shift << ISIF_VDFC_LEVEL_SHFT_SHIFT;

	regw(val, DFCCTL);

	
	regw(vdfc->def_sat_level, VDFSATLV);

	regw(vdfc->table[0].pos_vert, DFCMEM0);
	regw(vdfc->table[0].pos_horz, DFCMEM1);
	if (vdfc->corr_mode == ISIF_VDFC_NORMAL ||
	    vdfc->corr_mode == ISIF_VDFC_HORZ_INTERPOL_IF_SAT) {
		regw(vdfc->table[0].level_at_pos, DFCMEM2);
		regw(vdfc->table[0].level_up_pixels, DFCMEM3);
		regw(vdfc->table[0].level_low_pixels, DFCMEM4);
	}

	
	val = regr(DFCMEMCTL) | (1 << ISIF_DFCMEMCTL_DFCMARST_SHIFT) | 1;
	regw(val, DFCMEMCTL);

	count = retries;
	while (count && (regr(DFCMEMCTL) & 0x1))
		count--;

	if (!count) {
		dev_dbg(isif_cfg.dev, "defect table write timeout !!!\n");
		return -1;
	}

	for (i = 1; i < vdfc->num_vdefects; i++) {
		regw(vdfc->table[i].pos_vert, DFCMEM0);
		regw(vdfc->table[i].pos_horz, DFCMEM1);
		if (vdfc->corr_mode == ISIF_VDFC_NORMAL ||
		    vdfc->corr_mode == ISIF_VDFC_HORZ_INTERPOL_IF_SAT) {
			regw(vdfc->table[i].level_at_pos, DFCMEM2);
			regw(vdfc->table[i].level_up_pixels, DFCMEM3);
			regw(vdfc->table[i].level_low_pixels, DFCMEM4);
		}
		val = regr(DFCMEMCTL);
		
		val &= ~BIT(ISIF_DFCMEMCTL_DFCMARST_SHIFT);
		val |= 1;
		regw(val, DFCMEMCTL);

		count = retries;
		while (count && (regr(DFCMEMCTL) & 0x1))
			count--;

		if (!count) {
			dev_err(isif_cfg.dev,
				"defect table write timeout !!!\n");
			return -1;
		}
	}
	if (vdfc->num_vdefects < ISIF_VDFC_TABLE_SIZE) {
		
		regw(0, DFCMEM0);
		regw(0x1FFF, DFCMEM1);
		regw(1, DFCMEMCTL);
	}

	
	reg_modify((1 << ISIF_VDFC_EN_SHIFT), (1 << ISIF_VDFC_EN_SHIFT),
		   DFCCTL);
	return 0;
}

static void isif_config_csc(struct isif_df_csc *df_csc)
{
	u32 val1 = 0, val2 = 0, i;

	if (!df_csc->csc.en) {
		regw(0, CSCCTL);
		return;
	}
	for (i = 0; i < ISIF_CSC_NUM_COEFF; i++) {
		if ((i % 2) == 0) {
			
			val1 = (df_csc->csc.coeff[i].integer <<
				ISIF_CSC_COEF_INTEG_SHIFT) |
				df_csc->csc.coeff[i].decimal;
		} else {

			
			val2 = (df_csc->csc.coeff[i].integer <<
				ISIF_CSC_COEF_INTEG_SHIFT) |
				df_csc->csc.coeff[i].decimal;
			val2 <<= ISIF_CSCM_MSB_SHIFT;
			val2 |= val1;
			regw(val2, (CSCM0 + ((i - 1) << 1)));
		}
	}

	
	regw(df_csc->start_pix, FMTSPH);
	regw(df_csc->num_pixels, FMTLNH);
	regw(df_csc->start_line, FMTSLV);
	regw(df_csc->num_lines, FMTLNV);

	
	regw(1, CSCCTL);
}

static int isif_config_raw(void)
{
	struct isif_params_raw *params = &isif_cfg.bayer;
	struct isif_config_params_raw *module_params =
		&isif_cfg.bayer.config_params;
	struct vpss_pg_frame_size frame_size;
	struct vpss_sync_pol sync;
	u32 val;

	dev_dbg(isif_cfg.dev, "\nStarting isif_config_raw..\n");


	val = ISIF_YCINSWP_RAW | ISIF_CCDCFG_FIDMD_LATCH_VSYNC |
		ISIF_CCDCFG_WENLOG_AND | ISIF_CCDCFG_TRGSEL_WEN |
		ISIF_CCDCFG_EXTRG_DISABLE | isif_cfg.data_pack;

	dev_dbg(isif_cfg.dev, "Writing 0x%x to ...CCDCFG \n", val);
	regw(val, CCDCFG);


	val = ISIF_VDHDOUT_INPUT | (params->vd_pol << ISIF_VD_POL_SHIFT) |
		(params->hd_pol << ISIF_HD_POL_SHIFT) |
		(params->fid_pol << ISIF_FID_POL_SHIFT) |
		(ISIF_DATAPOL_NORMAL << ISIF_DATAPOL_SHIFT) |
		(ISIF_EXWEN_DISABLE << ISIF_EXWEN_SHIFT) |
		(params->frm_fmt << ISIF_FRM_FMT_SHIFT) |
		(params->pix_fmt << ISIF_INPUT_SHIFT) |
		(params->config_params.data_shift << ISIF_DATASFT_SHIFT);

	regw(val, MODESET);
	dev_dbg(isif_cfg.dev, "Writing 0x%x to MODESET...\n", val);

	val = params->cfa_pat << ISIF_GAMMAWD_CFA_SHIFT;

	
	if (module_params->compress.alg == ISIF_ALAW)
		val |= ISIF_ALAW_ENABLE;

	val |= (params->data_msb << ISIF_ALAW_GAMA_WD_SHIFT);
	regw(val, CGAMMAWD);

	
	if (module_params->compress.alg == ISIF_DPCM) {
		val =  BIT(ISIF_DPCM_EN_SHIFT) |
		       (module_params->compress.pred <<
		       ISIF_DPCM_PREDICTOR_SHIFT);
	}

	regw(val, MISC);

	
	isif_config_gain_offset();

	
	val = (params->config_params.col_pat_field0.olop) |
	      (params->config_params.col_pat_field0.olep << 2) |
	      (params->config_params.col_pat_field0.elop << 4) |
	      (params->config_params.col_pat_field0.elep << 6) |
	      (params->config_params.col_pat_field1.olop << 8) |
	      (params->config_params.col_pat_field1.olep << 10) |
	      (params->config_params.col_pat_field1.elop << 12) |
	      (params->config_params.col_pat_field1.elep << 14);
	regw(val, CCOLP);
	dev_dbg(isif_cfg.dev, "Writing %x to CCOLP ...\n", val);

	
	val = (!!params->horz_flip_en) << ISIF_HSIZE_FLIP_SHIFT;

	
	if (isif_cfg.data_pack == ISIF_PACK_8BIT)
		val |= ((params->win.width + 31) >> 5);
	else if (isif_cfg.data_pack == ISIF_PACK_12BIT)
		val |= (((params->win.width +
		       (params->win.width >> 2)) + 31) >> 5);
	else
		val |= (((params->win.width * 2) + 31) >> 5);
	regw(val, HSIZE);

	
	if (params->frm_fmt == CCDC_FRMFMT_INTERLACED) {
		if (params->image_invert_en) {
			
			regw(0x4B6D, SDOFST);
			dev_dbg(isif_cfg.dev, "Writing 0x4B6D to SDOFST...\n");
		} else {
			
			regw(0x0B6D, SDOFST);
			dev_dbg(isif_cfg.dev, "Writing 0x0B6D to SDOFST...\n");
		}
	} else if (params->frm_fmt == CCDC_FRMFMT_PROGRESSIVE) {
		if (params->image_invert_en) {
			
			regw(0x4000, SDOFST);
			dev_dbg(isif_cfg.dev, "Writing 0x4000 to SDOFST...\n");
		} else {
			
			regw(0x0000, SDOFST);
			dev_dbg(isif_cfg.dev, "Writing 0x0000 to SDOFST...\n");
		}
	}

	
	isif_setwin(&params->win, params->frm_fmt, 1);

	
	isif_config_bclamp(&module_params->bclamp);

	
	if (isif_config_dfc(&module_params->dfc) < 0)
		return -EFAULT;

	if (!module_params->df_csc.df_or_csc)
		
		isif_config_csc(&module_params->df_csc);

	isif_config_linearization(&module_params->linearize);

	
	isif_config_culling(&module_params->culling);

	
	regw(module_params->horz_offset, DATAHOFST);
	regw(module_params->vert_offset, DATAVOFST);

	
	if (params->config_params.test_pat_gen) {
		
		sync.ccdpg_hdpol = params->hd_pol;
		sync.ccdpg_vdpol = params->vd_pol;
		dm365_vpss_set_sync_pol(sync);
		frame_size.hlpfr = isif_cfg.bayer.win.width;
		frame_size.pplen = isif_cfg.bayer.win.height;
		dm365_vpss_set_pg_frame_size(frame_size);
		vpss_select_ccdc_source(VPSS_PGLPBK);
	}

	dev_dbg(isif_cfg.dev, "\nEnd of isif_config_ycbcr...\n");
	return 0;
}

static int isif_set_buftype(enum ccdc_buftype buf_type)
{
	if (isif_cfg.if_type == VPFE_RAW_BAYER)
		isif_cfg.bayer.buf_type = buf_type;
	else
		isif_cfg.ycbcr.buf_type = buf_type;

	return 0;

}
static enum ccdc_buftype isif_get_buftype(void)
{
	if (isif_cfg.if_type == VPFE_RAW_BAYER)
		return isif_cfg.bayer.buf_type;

	return isif_cfg.ycbcr.buf_type;
}

static int isif_enum_pix(u32 *pix, int i)
{
	int ret = -EINVAL;

	if (isif_cfg.if_type == VPFE_RAW_BAYER) {
		if (i < ARRAY_SIZE(isif_raw_bayer_pix_formats)) {
			*pix = isif_raw_bayer_pix_formats[i];
			ret = 0;
		}
	} else {
		if (i < ARRAY_SIZE(isif_raw_yuv_pix_formats)) {
			*pix = isif_raw_yuv_pix_formats[i];
			ret = 0;
		}
	}

	return ret;
}

static int isif_set_pixel_format(unsigned int pixfmt)
{
	if (isif_cfg.if_type == VPFE_RAW_BAYER) {
		if (pixfmt == V4L2_PIX_FMT_SBGGR8) {
			if ((isif_cfg.bayer.config_params.compress.alg !=
			     ISIF_ALAW) &&
			    (isif_cfg.bayer.config_params.compress.alg !=
			     ISIF_DPCM)) {
				dev_dbg(isif_cfg.dev,
					"Either configure A-Law or DPCM\n");
				return -EINVAL;
			}
			isif_cfg.data_pack = ISIF_PACK_8BIT;
		} else if (pixfmt == V4L2_PIX_FMT_SBGGR16) {
			isif_cfg.bayer.config_params.compress.alg =
					ISIF_NO_COMPRESSION;
			isif_cfg.data_pack = ISIF_PACK_16BIT;
		} else
			return -EINVAL;
		isif_cfg.bayer.pix_fmt = CCDC_PIXFMT_RAW;
	} else {
		if (pixfmt == V4L2_PIX_FMT_YUYV)
			isif_cfg.ycbcr.pix_order = CCDC_PIXORDER_YCBYCR;
		else if (pixfmt == V4L2_PIX_FMT_UYVY)
			isif_cfg.ycbcr.pix_order = CCDC_PIXORDER_CBYCRY;
		else
			return -EINVAL;
		isif_cfg.data_pack = ISIF_PACK_8BIT;
	}
	return 0;
}

static u32 isif_get_pixel_format(void)
{
	u32 pixfmt;

	if (isif_cfg.if_type == VPFE_RAW_BAYER)
		if (isif_cfg.bayer.config_params.compress.alg == ISIF_ALAW ||
		    isif_cfg.bayer.config_params.compress.alg == ISIF_DPCM)
			pixfmt = V4L2_PIX_FMT_SBGGR8;
		else
			pixfmt = V4L2_PIX_FMT_SBGGR16;
	else {
		if (isif_cfg.ycbcr.pix_order == CCDC_PIXORDER_YCBYCR)
			pixfmt = V4L2_PIX_FMT_YUYV;
		else
			pixfmt = V4L2_PIX_FMT_UYVY;
	}
	return pixfmt;
}

static int isif_set_image_window(struct v4l2_rect *win)
{
	if (isif_cfg.if_type == VPFE_RAW_BAYER) {
		isif_cfg.bayer.win.top = win->top;
		isif_cfg.bayer.win.left = win->left;
		isif_cfg.bayer.win.width = win->width;
		isif_cfg.bayer.win.height = win->height;
	} else {
		isif_cfg.ycbcr.win.top = win->top;
		isif_cfg.ycbcr.win.left = win->left;
		isif_cfg.ycbcr.win.width = win->width;
		isif_cfg.ycbcr.win.height = win->height;
	}
	return 0;
}

static void isif_get_image_window(struct v4l2_rect *win)
{
	if (isif_cfg.if_type == VPFE_RAW_BAYER)
		*win = isif_cfg.bayer.win;
	else
		*win = isif_cfg.ycbcr.win;
}

static unsigned int isif_get_line_length(void)
{
	unsigned int len;

	if (isif_cfg.if_type == VPFE_RAW_BAYER) {
		if (isif_cfg.data_pack == ISIF_PACK_8BIT)
			len = ((isif_cfg.bayer.win.width));
		else if (isif_cfg.data_pack == ISIF_PACK_12BIT)
			len = (((isif_cfg.bayer.win.width * 2) +
				 (isif_cfg.bayer.win.width >> 2)));
		else
			len = (((isif_cfg.bayer.win.width * 2)));
	} else
		len = (((isif_cfg.ycbcr.win.width * 2)));
	return ALIGN(len, 32);
}

static int isif_set_frame_format(enum ccdc_frmfmt frm_fmt)
{
	if (isif_cfg.if_type == VPFE_RAW_BAYER)
		isif_cfg.bayer.frm_fmt = frm_fmt;
	else
		isif_cfg.ycbcr.frm_fmt = frm_fmt;
	return 0;
}
static enum ccdc_frmfmt isif_get_frame_format(void)
{
	if (isif_cfg.if_type == VPFE_RAW_BAYER)
		return isif_cfg.bayer.frm_fmt;
	return isif_cfg.ycbcr.frm_fmt;
}

static int isif_getfid(void)
{
	return (regr(MODESET) >> 15) & 0x1;
}

static void isif_setfbaddr(unsigned long addr)
{
	regw((addr >> 21) & 0x07ff, CADU);
	regw((addr >> 5) & 0x0ffff, CADL);
}

static int isif_set_hw_if_params(struct vpfe_hw_if_param *params)
{
	isif_cfg.if_type = params->if_type;

	switch (params->if_type) {
	case VPFE_BT656:
	case VPFE_BT656_10BIT:
	case VPFE_YCBCR_SYNC_8:
		isif_cfg.ycbcr.pix_fmt = CCDC_PIXFMT_YCBCR_8BIT;
		isif_cfg.ycbcr.pix_order = CCDC_PIXORDER_CBYCRY;
		break;
	case VPFE_BT1120:
	case VPFE_YCBCR_SYNC_16:
		isif_cfg.ycbcr.pix_fmt = CCDC_PIXFMT_YCBCR_16BIT;
		isif_cfg.ycbcr.pix_order = CCDC_PIXORDER_CBYCRY;
		break;
	case VPFE_RAW_BAYER:
		isif_cfg.bayer.pix_fmt = CCDC_PIXFMT_RAW;
		break;
	default:
		dev_dbg(isif_cfg.dev, "Invalid interface type\n");
		return -EINVAL;
	}

	return 0;
}

static int isif_config_ycbcr(void)
{
	struct isif_ycbcr_config *params = &isif_cfg.ycbcr;
	struct vpss_pg_frame_size frame_size;
	u32 modeset = 0, ccdcfg = 0;
	struct vpss_sync_pol sync;

	dev_dbg(isif_cfg.dev, "\nStarting isif_config_ycbcr...");

	
	modeset = modeset | (params->pix_fmt << ISIF_INPUT_SHIFT) |
		  (params->frm_fmt << ISIF_FRM_FMT_SHIFT) |
		  (params->fid_pol << ISIF_FID_POL_SHIFT) |
		  (params->hd_pol << ISIF_HD_POL_SHIFT) |
		  (params->vd_pol << ISIF_VD_POL_SHIFT);

	
	switch (isif_cfg.if_type) {
	case VPFE_BT656:
		if (params->pix_fmt != CCDC_PIXFMT_YCBCR_8BIT) {
			dev_dbg(isif_cfg.dev, "Invalid pix_fmt(input mode)\n");
			return -EINVAL;
		}
		modeset |= (VPFE_PINPOL_NEGATIVE << ISIF_VD_POL_SHIFT);
		regw(3, REC656IF);
		ccdcfg = ccdcfg | ISIF_DATA_PACK8 | ISIF_YCINSWP_YCBCR;
		break;
	case VPFE_BT656_10BIT:
		if (params->pix_fmt != CCDC_PIXFMT_YCBCR_8BIT) {
			dev_dbg(isif_cfg.dev, "Invalid pix_fmt(input mode)\n");
			return -EINVAL;
		}
		
		regw(3, REC656IF);
		
		ccdcfg = ccdcfg | ISIF_DATA_PACK8 | ISIF_YCINSWP_YCBCR |
			ISIF_BW656_ENABLE;
		break;
	case VPFE_BT1120:
		if (params->pix_fmt != CCDC_PIXFMT_YCBCR_16BIT) {
			dev_dbg(isif_cfg.dev, "Invalid pix_fmt(input mode)\n");
			return -EINVAL;
		}
		regw(3, REC656IF);
		break;

	case VPFE_YCBCR_SYNC_8:
		ccdcfg |= ISIF_DATA_PACK8;
		ccdcfg |= ISIF_YCINSWP_YCBCR;
		if (params->pix_fmt != CCDC_PIXFMT_YCBCR_8BIT) {
			dev_dbg(isif_cfg.dev, "Invalid pix_fmt(input mode)\n");
			return -EINVAL;
		}
		break;
	case VPFE_YCBCR_SYNC_16:
		if (params->pix_fmt != CCDC_PIXFMT_YCBCR_16BIT) {
			dev_dbg(isif_cfg.dev, "Invalid pix_fmt(input mode)\n");
			return -EINVAL;
		}
		break;
	default:
		
		dev_dbg(isif_cfg.dev, "Invalid interface type\n");
		return -EINVAL;
	}

	regw(modeset, MODESET);

	
	ccdcfg |= params->pix_order << ISIF_PIX_ORDER_SHIFT;

	regw(ccdcfg, CCDCFG);

	
	if ((isif_cfg.if_type == VPFE_BT1120) ||
	    (isif_cfg.if_type == VPFE_YCBCR_SYNC_16))
		isif_setwin(&params->win, params->frm_fmt, 1);
	else
		isif_setwin(&params->win, params->frm_fmt, 2);

	regw(((((params->win.width * 2) + 31) & 0xffffffe0) >> 5), HSIZE);

	
	if ((params->frm_fmt == CCDC_FRMFMT_INTERLACED) &&
	    (params->buf_type == CCDC_BUFTYPE_FLD_INTERLEAVED))
		
		regw(0x00000249, SDOFST);

	
	if (isif_cfg.bayer.config_params.test_pat_gen) {
		sync.ccdpg_hdpol = params->hd_pol;
		sync.ccdpg_vdpol = params->vd_pol;
		dm365_vpss_set_sync_pol(sync);
		dm365_vpss_set_pg_frame_size(frame_size);
	}
	return 0;
}

static int isif_configure(void)
{
	if (isif_cfg.if_type == VPFE_RAW_BAYER)
		return isif_config_raw();
	return isif_config_ycbcr();
}

static int isif_close(struct device *device)
{
	
	isif_cfg.bayer.config_params = isif_config_defaults;
	return 0;
}

static struct ccdc_hw_device isif_hw_dev = {
	.name = "ISIF",
	.owner = THIS_MODULE,
	.hw_ops = {
		.open = isif_open,
		.close = isif_close,
		.enable = isif_enable,
		.enable_out_to_sdram = isif_enable_output_to_sdram,
		.set_hw_if_params = isif_set_hw_if_params,
		.configure = isif_configure,
		.set_buftype = isif_set_buftype,
		.get_buftype = isif_get_buftype,
		.enum_pix = isif_enum_pix,
		.set_pixel_format = isif_set_pixel_format,
		.get_pixel_format = isif_get_pixel_format,
		.set_frame_format = isif_set_frame_format,
		.get_frame_format = isif_get_frame_format,
		.set_image_window = isif_set_image_window,
		.get_image_window = isif_get_image_window,
		.get_line_length = isif_get_line_length,
		.setfbaddr = isif_setfbaddr,
		.getfid = isif_getfid,
	},
};

static int __init isif_probe(struct platform_device *pdev)
{
	void (*setup_pinmux)(void);
	struct resource	*res;
	void *__iomem addr;
	int status = 0, i;

	status = vpfe_register_ccdc_device(&isif_hw_dev);
	if (status < 0)
		return status;

	
	isif_cfg.mclk = clk_get(&pdev->dev, "master");
	if (IS_ERR(isif_cfg.mclk)) {
		status = PTR_ERR(isif_cfg.mclk);
		goto fail_mclk;
	}
	if (clk_enable(isif_cfg.mclk)) {
		status = -ENODEV;
		goto fail_mclk;
	}

	
	if (NULL == pdev->dev.platform_data) {
		status = -ENODEV;
		goto fail_mclk;
	}
	setup_pinmux = pdev->dev.platform_data;
	setup_pinmux();

	i = 0;
	
	while (i < 3) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			status = -ENODEV;
			goto fail_nobase_res;
		}
		res = request_mem_region(res->start, resource_size(res),
					 res->name);
		if (!res) {
			status = -EBUSY;
			goto fail_nobase_res;
		}
		addr = ioremap_nocache(res->start, resource_size(res));
		if (!addr) {
			status = -ENOMEM;
			goto fail_base_iomap;
		}
		switch (i) {
		case 0:
			
			isif_cfg.base_addr = addr;
			break;
		case 1:
			
			isif_cfg.linear_tbl0_addr = addr;
			break;
		default:
			
			isif_cfg.linear_tbl1_addr = addr;
			break;
		}
		i++;
	}
	isif_cfg.dev = &pdev->dev;

	printk(KERN_NOTICE "%s is registered with vpfe.\n",
		isif_hw_dev.name);
	return 0;
fail_base_iomap:
	release_mem_region(res->start, resource_size(res));
	i--;
fail_nobase_res:
	if (isif_cfg.base_addr)
		iounmap(isif_cfg.base_addr);
	if (isif_cfg.linear_tbl0_addr)
		iounmap(isif_cfg.linear_tbl0_addr);

	while (i >= 0) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		release_mem_region(res->start, resource_size(res));
		i--;
	}
fail_mclk:
	clk_put(isif_cfg.mclk);
	vpfe_unregister_ccdc_device(&isif_hw_dev);
	return status;
}

static int isif_remove(struct platform_device *pdev)
{
	struct resource	*res;
	int i = 0;

	iounmap(isif_cfg.base_addr);
	iounmap(isif_cfg.linear_tbl0_addr);
	iounmap(isif_cfg.linear_tbl1_addr);
	while (i < 3) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res)
			release_mem_region(res->start, resource_size(res));
		i++;
	}
	vpfe_unregister_ccdc_device(&isif_hw_dev);
	return 0;
}

static struct platform_driver isif_driver = {
	.driver = {
		.name	= "isif",
		.owner = THIS_MODULE,
	},
	.remove = __devexit_p(isif_remove),
	.probe = isif_probe,
};

module_platform_driver(isif_driver);

MODULE_LICENSE("GPL");
