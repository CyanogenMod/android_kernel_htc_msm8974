/*
 * Header file for TI DA8XX LCD controller platform data.
 *
 * Copyright (C) 2008-2009 MontaVista Software Inc.
 * Copyright (C) 2008-2009 Texas Instruments Inc
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef DA8XX_FB_H
#define DA8XX_FB_H

enum panel_type {
	QVGA = 0
};

enum panel_shade {
	MONOCHROME = 0,
	COLOR_ACTIVE,
	COLOR_PASSIVE,
};

enum raster_load_mode {
	LOAD_DATA = 1,
	LOAD_PALETTE,
};

struct display_panel {
	enum panel_type panel_type; 
	int max_bpp;
	int min_bpp;
	enum panel_shade panel_shade;
};

struct da8xx_lcdc_platform_data {
	const char manu_name[10];
	void *controller_data;
	const char type[25];
	void (*panel_power_ctrl)(int);
};

struct lcd_ctrl_config {
	const struct display_panel *p_disp_panel;

	
	int ac_bias;

	
	int ac_bias_intrpt;

	
	int dma_burst_sz;

	
	int bpp;

	
	int fdd;

	
	unsigned char tft_alt_mode;

	
	unsigned char stn_565_mode;

	
	unsigned char mono_8bit_mode;

	
	unsigned char invert_line_clock;

	
	unsigned char invert_frm_clock;

	
	unsigned char sync_edge;

	
	unsigned char sync_ctrl;

	
	unsigned char raster_order;
};

struct lcd_sync_arg {
	int back_porch;
	int front_porch;
	int pulse_width;
};

#define FBIOGET_CONTRAST	_IOR('F', 1, int)
#define FBIOPUT_CONTRAST	_IOW('F', 2, int)
#define FBIGET_BRIGHTNESS	_IOR('F', 3, int)
#define FBIPUT_BRIGHTNESS	_IOW('F', 3, int)
#define FBIGET_COLOR		_IOR('F', 5, int)
#define FBIPUT_COLOR		_IOW('F', 6, int)
#define FBIPUT_HSYNC		_IOW('F', 9, int)
#define FBIPUT_VSYNC		_IOW('F', 10, int)

#endif  

