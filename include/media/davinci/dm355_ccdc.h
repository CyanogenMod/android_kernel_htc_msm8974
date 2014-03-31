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
 */
#ifndef _DM355_CCDC_H
#define _DM355_CCDC_H
#include <media/davinci/ccdc_types.h>
#include <media/davinci/vpfe_types.h>

enum ccdc_sample_length {
	CCDC_SAMPLE_1PIXELS,
	CCDC_SAMPLE_2PIXELS,
	CCDC_SAMPLE_4PIXELS,
	CCDC_SAMPLE_8PIXELS,
	CCDC_SAMPLE_16PIXELS
};

enum ccdc_sample_line {
	CCDC_SAMPLE_1LINES,
	CCDC_SAMPLE_2LINES,
	CCDC_SAMPLE_4LINES,
	CCDC_SAMPLE_8LINES,
	CCDC_SAMPLE_16LINES
};

enum ccdc_gamma_width {
	CCDC_GAMMA_BITS_13_4,
	CCDC_GAMMA_BITS_12_3,
	CCDC_GAMMA_BITS_11_2,
	CCDC_GAMMA_BITS_10_1,
	CCDC_GAMMA_BITS_09_0
};

enum ccdc_colpats {
	CCDC_RED,
	CCDC_GREEN_RED,
	CCDC_GREEN_BLUE,
	CCDC_BLUE
};

struct ccdc_col_pat {
	enum ccdc_colpats olop;
	enum ccdc_colpats olep;
	enum ccdc_colpats elop;
	enum ccdc_colpats elep;
};

enum ccdc_datasft {
	CCDC_DATA_NO_SHIFT,
	CCDC_DATA_SHIFT_1BIT,
	CCDC_DATA_SHIFT_2BIT,
	CCDC_DATA_SHIFT_3BIT,
	CCDC_DATA_SHIFT_4BIT,
	CCDC_DATA_SHIFT_5BIT,
	CCDC_DATA_SHIFT_6BIT
};

enum ccdc_data_size {
	CCDC_DATA_16BITS,
	CCDC_DATA_15BITS,
	CCDC_DATA_14BITS,
	CCDC_DATA_13BITS,
	CCDC_DATA_12BITS,
	CCDC_DATA_11BITS,
	CCDC_DATA_10BITS,
	CCDC_DATA_8BITS
};
enum ccdc_mfilt1 {
	CCDC_NO_MEDIAN_FILTER1,
	CCDC_AVERAGE_FILTER1,
	CCDC_MEDIAN_FILTER1
};

enum ccdc_mfilt2 {
	CCDC_NO_MEDIAN_FILTER2,
	CCDC_AVERAGE_FILTER2,
	CCDC_MEDIAN_FILTER2
};

struct ccdc_a_law {
	
	unsigned char enable;
	
	enum ccdc_gamma_width gama_wd;
};

struct ccdc_black_clamp {
	
	unsigned char b_clamp_enable;
	
	enum ccdc_sample_length sample_pixel;
	
	enum ccdc_sample_line sample_ln;
	
	unsigned short start_pixel;
	
	unsigned short sgain;
	unsigned short dc_sub;
};

struct ccdc_black_compensation {
	
	unsigned char r;
	
	unsigned char gr;
	
	unsigned char b;
	
	unsigned char gb;
};

struct ccdc_float {
	int integer;
	unsigned int decimal;
};

#define CCDC_CSC_COEFF_TABLE_SIZE	16
struct ccdc_csc {
	unsigned char enable;
	struct ccdc_float coeff[CCDC_CSC_COEFF_TABLE_SIZE];
};

enum ccdc_vdf_csl {
	CCDC_VDF_NORMAL,
	CCDC_VDF_HORZ_INTERPOL_SAT,
	CCDC_VDF_HORZ_INTERPOL
};

enum ccdc_vdf_cuda {
	CCDC_VDF_WHOLE_LINE_CORRECT,
	CCDC_VDF_UPPER_DISABLE
};

enum ccdc_dfc_mwr {
	CCDC_DFC_MWR_WRITE_COMPLETE,
	CCDC_DFC_WRITE_REG
};

enum ccdc_dfc_mrd {
	CCDC_DFC_READ_COMPLETE,
	CCDC_DFC_READ_REG
};

enum ccdc_dfc_ma_rst {
	CCDC_DFC_INCR_ADDR,
	CCDC_DFC_CLR_ADDR
};

enum ccdc_dfc_mclr {
	CCDC_DFC_CLEAR_COMPLETE,
	CCDC_DFC_CLEAR
};

struct ccdc_dft_corr_ctl {
	enum ccdc_vdf_csl vdfcsl;
	enum ccdc_vdf_cuda vdfcuda;
	unsigned int vdflsft;
};

struct ccdc_dft_corr_mem_ctl {
	enum ccdc_dfc_mwr dfcmwr;
	enum ccdc_dfc_mrd dfcmrd;
	enum ccdc_dfc_ma_rst dfcmarst;
	enum ccdc_dfc_mclr dfcmclr;
};

#define CCDC_DFT_TABLE_SIZE	16
struct ccdc_vertical_dft {
	unsigned char ver_dft_en;
	unsigned char gen_dft_en;
	unsigned int saturation_ctl;
	struct ccdc_dft_corr_ctl dft_corr_ctl;
	struct ccdc_dft_corr_mem_ctl dft_corr_mem_ctl;
	int table_size;
	unsigned int dft_corr_horz[CCDC_DFT_TABLE_SIZE];
	unsigned int dft_corr_vert[CCDC_DFT_TABLE_SIZE];
	unsigned int dft_corr_sub1[CCDC_DFT_TABLE_SIZE];
	unsigned int dft_corr_sub2[CCDC_DFT_TABLE_SIZE];
	unsigned int dft_corr_sub3[CCDC_DFT_TABLE_SIZE];
};

struct ccdc_data_offset {
	unsigned char horz_offset;
	unsigned char vert_offset;
};

struct ccdc_config_params_raw {
	
	enum ccdc_datasft datasft;
	
	enum ccdc_data_size data_sz;
	
	enum ccdc_mfilt1 mfilt1;
	enum ccdc_mfilt2 mfilt2;
	
	unsigned char lpf_enable;
	
	int med_filt_thres;
	struct ccdc_data_offset data_offset;
	
	struct ccdc_a_law alaw;
	
	struct ccdc_black_clamp blk_clamp;
	
	struct ccdc_black_compensation blk_comp;
	
	struct ccdc_vertical_dft vertical_dft;
	
	struct ccdc_csc csc;
	
	struct ccdc_col_pat col_pat_field0;
	struct ccdc_col_pat col_pat_field1;
};

#ifdef __KERNEL__
#include <linux/io.h>

#define CCDC_WIN_PAL	{0, 0, 720, 576}
#define CCDC_WIN_VGA	{0, 0, 640, 480}

struct ccdc_params_ycbcr {
	
	enum ccdc_pixfmt pix_fmt;
	
	enum ccdc_frmfmt frm_fmt;
	
	struct v4l2_rect win;
	
	enum vpfe_pin_pol fid_pol;
	
	enum vpfe_pin_pol vd_pol;
	
	enum vpfe_pin_pol hd_pol;
	
	int bt656_enable;
	
	enum ccdc_pixorder pix_order;
	
	enum ccdc_buftype buf_type;
};

struct ccdc_gain {
	unsigned short r_ye;
	unsigned short gr_cy;
	unsigned short gb_g;
	unsigned short b_mg;
};

struct ccdc_params_raw {
	
	enum ccdc_pixfmt pix_fmt;
	
	enum ccdc_frmfmt frm_fmt;
	
	struct v4l2_rect win;
	
	enum vpfe_pin_pol fid_pol;
	
	enum vpfe_pin_pol vd_pol;
	
	enum vpfe_pin_pol hd_pol;
	
	enum ccdc_buftype buf_type;
	
	struct ccdc_gain gain;
	
	unsigned int ccdc_offset;
	
	unsigned char horz_flip_enable;
	unsigned char image_invert_enable;
	
	struct ccdc_config_params_raw config_params;
};

#endif
#endif				
