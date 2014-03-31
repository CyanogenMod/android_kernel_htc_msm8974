/*
 * Copyright (C) 2007-2009 Texas Instruments Inc
 * Copyright (C) 2007 MontaVista Software, Inc.
 *
 * Andy Lowe (alowe@mvista.com), MontaVista Software
 * - Initial version
 * Murali Karicheri (mkaricheri@gmail.com), Texas Instruments Ltd.
 * - ported to sub device interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2..
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
 */
#ifndef _OSD_H
#define _OSD_H

#include <media/davinci/vpbe_types.h>

#define VPBE_OSD_SUBDEV_NAME "vpbe-osd"

enum osd_layer {
	WIN_OSD0,
	WIN_VID0,
	WIN_OSD1,
	WIN_VID1,
};

enum osd_win_layer {
	OSDWIN_OSD0,
	OSDWIN_OSD1,
};

enum osd_pix_format {
	PIXFMT_1BPP = 0,
	PIXFMT_2BPP,
	PIXFMT_4BPP,
	PIXFMT_8BPP,
	PIXFMT_RGB565,
	PIXFMT_YCbCrI,
	PIXFMT_RGB888,
	PIXFMT_YCrCbI,
	PIXFMT_NV12,
	PIXFMT_OSD_ATTR,
};

enum osd_h_exp_ratio {
	H_EXP_OFF,
	H_EXP_9_OVER_8,
	H_EXP_3_OVER_2,
};

enum osd_v_exp_ratio {
	V_EXP_OFF,
	V_EXP_6_OVER_5,
};

enum osd_zoom_factor {
	ZOOM_X1,
	ZOOM_X2,
	ZOOM_X4,
};

enum osd_clut {
	ROM_CLUT,
	RAM_CLUT,
};

enum osd_rom_clut {
	ROM_CLUT0,
	ROM_CLUT1,
};

enum osd_blending_factor {
	OSD_0_VID_8,
	OSD_1_VID_7,
	OSD_2_VID_6,
	OSD_3_VID_5,
	OSD_4_VID_4,
	OSD_5_VID_3,
	OSD_6_VID_2,
	OSD_8_VID_0,
};

enum osd_blink_interval {
	BLINK_X1,
	BLINK_X2,
	BLINK_X3,
	BLINK_X4,
};

enum osd_cursor_h_width {
	H_WIDTH_1,
	H_WIDTH_4,
	H_WIDTH_8,
	H_WIDTH_12,
	H_WIDTH_16,
	H_WIDTH_20,
	H_WIDTH_24,
	H_WIDTH_28,
};

enum osd_cursor_v_width {
	V_WIDTH_1,
	V_WIDTH_2,
	V_WIDTH_4,
	V_WIDTH_6,
	V_WIDTH_8,
	V_WIDTH_10,
	V_WIDTH_12,
	V_WIDTH_14,
};

struct osd_cursor_config {
	unsigned xsize;
	unsigned ysize;
	unsigned xpos;
	unsigned ypos;
	int interlaced;
	enum osd_cursor_h_width h_width;
	enum osd_cursor_v_width v_width;
	enum osd_clut clut;
	unsigned char clut_index;
};

struct osd_layer_config {
	enum osd_pix_format pixfmt;
	unsigned line_length;
	unsigned xsize;
	unsigned ysize;
	unsigned xpos;
	unsigned ypos;
	int interlaced;
};

struct osd_window_state {
	int is_allocated;
	int is_enabled;
	unsigned long fb_base_phys;
	enum osd_zoom_factor h_zoom;
	enum osd_zoom_factor v_zoom;
	struct osd_layer_config lconfig;
};

struct osd_osdwin_state {
	enum osd_clut clut;
	enum osd_blending_factor blend;
	int colorkey_blending;
	unsigned colorkey;
	int rec601_attenuation;
	
	unsigned char palette_map[16];
};

struct osd_cursor_state {
	int is_enabled;
	struct osd_cursor_config config;
};

struct osd_state;

struct vpbe_osd_ops {
	int (*initialize)(struct osd_state *sd);
	int (*request_layer)(struct osd_state *sd, enum osd_layer layer);
	void (*release_layer)(struct osd_state *sd, enum osd_layer layer);
	int (*enable_layer)(struct osd_state *sd, enum osd_layer layer,
			    int otherwin);
	void (*disable_layer)(struct osd_state *sd, enum osd_layer layer);
	int (*set_layer_config)(struct osd_state *sd, enum osd_layer layer,
				struct osd_layer_config *lconfig);
	void (*get_layer_config)(struct osd_state *sd, enum osd_layer layer,
				 struct osd_layer_config *lconfig);
	void (*start_layer)(struct osd_state *sd, enum osd_layer layer,
			    unsigned long fb_base_phys,
			    unsigned long cbcr_ofst);
	void (*set_left_margin)(struct osd_state *sd, u32 val);
	void (*set_top_margin)(struct osd_state *sd, u32 val);
	void (*set_interpolation_filter)(struct osd_state *sd, int filter);
	int (*set_vid_expansion)(struct osd_state *sd,
					enum osd_h_exp_ratio h_exp,
					enum osd_v_exp_ratio v_exp);
	void (*get_vid_expansion)(struct osd_state *sd,
					enum osd_h_exp_ratio *h_exp,
					enum osd_v_exp_ratio *v_exp);
	void (*set_zoom)(struct osd_state *sd, enum osd_layer layer,
				enum osd_zoom_factor h_zoom,
				enum osd_zoom_factor v_zoom);
};

struct osd_state {
	enum vpbe_version vpbe_type;
	spinlock_t lock;
	struct device *dev;
	dma_addr_t osd_base_phys;
	unsigned long osd_base;
	unsigned long osd_size;
	
	int pingpong;
	int interpolation_filter;
	int field_inversion;
	enum osd_h_exp_ratio osd_h_exp;
	enum osd_v_exp_ratio osd_v_exp;
	enum osd_h_exp_ratio vid_h_exp;
	enum osd_v_exp_ratio vid_v_exp;
	enum osd_clut backg_clut;
	unsigned backg_clut_index;
	enum osd_rom_clut rom_clut;
	int is_blinking;
	
	enum osd_blink_interval blink;
	
	enum osd_pix_format yc_pixfmt;
	
	unsigned char clut_ram[256][3];
	struct osd_cursor_state cursor;
	
	struct osd_window_state win[4];
	
	struct osd_osdwin_state osdwin[2];
	
	struct vpbe_osd_ops ops;
};

struct osd_platform_data {
	enum vpbe_version vpbe_type;
	int  field_inv_wa_enable;
};

#endif
