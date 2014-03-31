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
 * isif header file
 */
#ifndef _ISIF_H
#define _ISIF_H

#include <media/davinci/ccdc_types.h>
#include <media/davinci/vpfe_types.h>

struct isif_float_8 {
	
	__u8 integer;
	
	__u8 decimal;
};

struct isif_float_16 {
	
	__u16 integer;
	
	__u16 decimal;
};

struct isif_vdfc_entry {
	
	__u16 pos_vert;
	
	__u16 pos_horz;
	__u8 level_at_pos;
	__u8 level_up_pixels;
	__u8 level_low_pixels;
};

#define ISIF_VDFC_TABLE_SIZE		8
struct isif_dfc {
	
	__u8 en;
	
#define	ISIF_VDFC_NORMAL		0
#define ISIF_VDFC_HORZ_INTERPOL_IF_SAT	1
	
#define	ISIF_VDFC_HORZ_INTERPOL		2
	
	__u8 corr_mode;
	
	__u8 corr_whole_line;
#define ISIF_VDFC_NO_SHIFT		0
#define ISIF_VDFC_SHIFT_1		1
#define ISIF_VDFC_SHIFT_2		2
#define ISIF_VDFC_SHIFT_3		3
#define ISIF_VDFC_SHIFT_4		4
	__u8 def_level_shift;
	
	__u16 def_sat_level;
	
	__u16 num_vdefects;
	
	struct isif_vdfc_entry table[ISIF_VDFC_TABLE_SIZE];
};

struct isif_horz_bclamp {

	
#define	ISIF_HORZ_BC_DISABLE		0
#define ISIF_HORZ_BC_CLAMP_CALC_ENABLED	1
#define ISIF_HORZ_BC_CLAMP_NOT_UPDATED	2
	
	__u8 mode;
	__u8 clamp_pix_limit;
	
#define	ISIF_SEL_MOST_LEFT_WIN		0
	
#define ISIF_SEL_MOST_RIGHT_WIN		1
	
	__u8 base_win_sel_calc;
	
	__u8 win_count_calc;
	
	__u16 win_start_h_calc;
	
	__u16 win_start_v_calc;
#define ISIF_HORZ_BC_SZ_H_2PIXELS	0
#define ISIF_HORZ_BC_SZ_H_4PIXELS	1
#define ISIF_HORZ_BC_SZ_H_8PIXELS	2
#define ISIF_HORZ_BC_SZ_H_16PIXELS	3
	
	__u8 win_h_sz_calc;
#define ISIF_HORZ_BC_SZ_V_32PIXELS	0
#define ISIF_HORZ_BC_SZ_V_64PIXELS	1
#define	ISIF_HORZ_BC_SZ_V_128PIXELS	2
#define ISIF_HORZ_BC_SZ_V_256PIXELS	3
	
	__u8 win_v_sz_calc;
};

struct isif_vert_bclamp {
	
#define	ISIF_VERT_BC_USE_HORZ_CLAMP_VAL		0
	
#define	ISIF_VERT_BC_USE_CONFIG_CLAMP_VAL	1
	
#define	ISIF_VERT_BC_NO_UPDATE			2
	__u8 reset_val_sel;
	
	__u8 line_ave_coef;
	
	__u16 ob_v_sz_calc;
	
	__u16 ob_start_h;
	
	__u16 ob_start_v;
};

struct isif_black_clamp {
	__u16 dc_offset;
	__u8 en;
	__u8 bc_mode_color;
	
	__u16 vert_start_sub;
	
	struct isif_horz_bclamp horz;
	
	struct isif_vert_bclamp vert;
};

#define ISIF_CSC_NUM_COEFF	16
struct isif_color_space_conv {
	
	__u8 en;
	struct isif_float_8 coeff[ISIF_CSC_NUM_COEFF];
};


struct isif_black_comp {
	
	__s8 r_comp;
	
	__s8 gr_comp;
	
	__s8 b_comp;
	
	__s8 gb_comp;
};

struct isif_gain {
	
	struct isif_float_16 r_ye;
	
	struct isif_float_16 gr_cy;
	
	struct isif_float_16 gb_g;
	
	struct isif_float_16 b_mg;
};

#define ISIF_LINEAR_TAB_SIZE	192
struct isif_linearize {
	
	__u8 en;
	
	__u8 corr_shft;
	
	struct isif_float_16 scale_fact;
	
	__u16 table[ISIF_LINEAR_TAB_SIZE];
};

#define ISIF_RED	0
#define	ISIF_GREEN_RED	1
#define ISIF_GREEN_BLUE	2
#define ISIF_BLUE	3
struct isif_col_pat {
	__u8 olop;
	__u8 olep;
	__u8 elop;
	__u8 elep;
};

struct isif_fmtplen {
	__u16 plen0;
	__u16 plen1;
	__u16 plen2;
	__u16 plen3;
};

struct isif_fmt_cfg {
#define ISIF_SPLIT		0
#define ISIF_COMBINE		1
	
	__u8 fmtmode;
	
	__u8 ln_alter_en;
#define ISIF_1LINE		0
#define	ISIF_2LINES		1
#define	ISIF_3LINES		2
#define	ISIF_4LINES		3
	
	__u8 lnum;
	
	__u8 addrinc;
};

struct isif_fmt_addr_ptr {
	
	__u32 init_addr;
	
#define ISIF_1STLINE		0
#define	ISIF_2NDLINE		1
#define	ISIF_3RDLINE		2
#define	ISIF_4THLINE		3
	__u8 out_line;
};

struct isif_fmtpgm_ap {
	
	__u8 pgm_aptr;
	
	__u8 pgmupdt;
};

struct isif_data_formatter {
	
	__u8 en;
	
	struct isif_fmt_cfg cfg;
	
	struct isif_fmtplen plen;
	
	__u16 fmtrlen;
	
	__u16 fmthcnt;
	
	struct isif_fmt_addr_ptr fmtaddr_ptr[16];
	
	__u8 pgm_en[32];
	
	struct isif_fmtpgm_ap fmtpgm_ap[32];
};

struct isif_df_csc {
	
	__u8 df_or_csc;
	
	struct isif_color_space_conv csc;
	
	struct isif_data_formatter df;
	
	__u32 start_pix;
	
	__u32 num_pixels;
	
	__u32 start_line;
	
	__u32 num_lines;
};

struct isif_gain_offsets_adj {
	
	struct isif_gain gain;
	
	__u16 offset;
	
	__u8 gain_sdram_en;
	
	__u8 gain_ipipe_en;
	
	__u8 gain_h3a_en;
	
	__u8 offset_sdram_en;
	
	__u8 offset_ipipe_en;
	
	__u8 offset_h3a_en;
};

struct isif_cul {
	
	__u8 hcpat_odd;
	
	__u8 hcpat_even;
	
	__u8 vcpat;
	
	__u8 en_lpf;
};

struct isif_compress {
#define ISIF_ALAW		0
#define ISIF_DPCM		1
#define ISIF_NO_COMPRESSION	2
	
	__u8 alg;
	
#define ISIF_DPCM_PRED1		0
	
#define ISIF_DPCM_PRED2		1
	
	__u8 pred;
};

struct isif_config_params_raw {
	
	struct isif_linearize linearize;
	
	struct isif_df_csc df_csc;
	
	struct isif_dfc dfc;
	
	struct isif_black_clamp bclamp;
	
	struct isif_gain_offsets_adj gain_offset;
	
	struct isif_cul culling;
	
	struct isif_compress compress;
	
	__u16 horz_offset;
	
	__u16 vert_offset;
	
	struct isif_col_pat col_pat_field0;
	
	struct isif_col_pat col_pat_field1;
#define ISIF_NO_SHIFT		0
#define	ISIF_1BIT_SHIFT		1
#define	ISIF_2BIT_SHIFT		2
#define	ISIF_3BIT_SHIFT		3
#define	ISIF_4BIT_SHIFT		4
#define ISIF_5BIT_SHIFT		5
#define ISIF_6BIT_SHIFT		6
	
	__u8 data_shift;
	
	__u8 test_pat_gen;
};

#ifdef __KERNEL__
struct isif_ycbcr_config {
	
	enum ccdc_pixfmt pix_fmt;
	
	enum ccdc_frmfmt frm_fmt;
	
	struct v4l2_rect win;
	
	enum vpfe_pin_pol fid_pol;
	
	enum vpfe_pin_pol vd_pol;
	
	enum vpfe_pin_pol hd_pol;
	
	enum ccdc_pixorder pix_order;
	
	enum ccdc_buftype buf_type;
};

enum isif_data_msb {
	ISIF_BIT_MSB_15,
	ISIF_BIT_MSB_14,
	ISIF_BIT_MSB_13,
	ISIF_BIT_MSB_12,
	ISIF_BIT_MSB_11,
	ISIF_BIT_MSB_10,
	ISIF_BIT_MSB_9,
	ISIF_BIT_MSB_8,
	ISIF_BIT_MSB_7
};

enum isif_cfa_pattern {
	ISIF_CFA_PAT_MOSAIC,
	ISIF_CFA_PAT_STRIPE
};

struct isif_params_raw {
	
	enum ccdc_pixfmt pix_fmt;
	
	enum ccdc_frmfmt frm_fmt;
	
	struct v4l2_rect win;
	
	enum vpfe_pin_pol fid_pol;
	
	enum vpfe_pin_pol vd_pol;
	
	enum vpfe_pin_pol hd_pol;
	
	enum ccdc_buftype buf_type;
	
	struct isif_gain gain;
	
	enum isif_cfa_pattern cfa_pat;
	
	enum isif_data_msb data_msb;
	
	unsigned char horz_flip_en;
	
	unsigned char image_invert_en;

	
	struct isif_config_params_raw config_params;
};

enum isif_data_pack {
	ISIF_PACK_16BIT,
	ISIF_PACK_12BIT,
	ISIF_PACK_8BIT
};

#define ISIF_WIN_NTSC				{0, 0, 720, 480}
#define ISIF_WIN_VGA				{0, 0, 640, 480}

#endif
#endif
