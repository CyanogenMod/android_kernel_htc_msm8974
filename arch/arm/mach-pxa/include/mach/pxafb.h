/*
 *  arch/arm/mach-pxa/include/mach/pxafb.h
 *
 *  Support for the xscale frame buffer.
 *
 *  Author:     Jean-Frederic Clere
 *  Created:    Sep 22, 2003
 *  Copyright:  jfclere@sinix.net
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/fb.h>
#include <mach/regs-lcd.h>

#define LCD_CONN_TYPE(_x)	((_x) & 0x0f)
#define LCD_CONN_WIDTH(_x)	(((_x) >> 4) & 0x1f)

#define LCD_TYPE_MASK		0xf
#define LCD_TYPE_UNKNOWN	0
#define LCD_TYPE_MONO_STN	1
#define LCD_TYPE_MONO_DSTN	2
#define LCD_TYPE_COLOR_STN	3
#define LCD_TYPE_COLOR_DSTN	4
#define LCD_TYPE_COLOR_TFT	5
#define LCD_TYPE_SMART_PANEL	6
#define LCD_TYPE_MAX		7

#define LCD_MONO_STN_4BPP	((4  << 4) | LCD_TYPE_MONO_STN)
#define LCD_MONO_STN_8BPP	((8  << 4) | LCD_TYPE_MONO_STN)
#define LCD_MONO_DSTN_8BPP	((8  << 4) | LCD_TYPE_MONO_DSTN)
#define LCD_COLOR_STN_8BPP	((8  << 4) | LCD_TYPE_COLOR_STN)
#define LCD_COLOR_DSTN_16BPP	((16 << 4) | LCD_TYPE_COLOR_DSTN)
#define LCD_COLOR_TFT_8BPP	((8  << 4) | LCD_TYPE_COLOR_TFT)
#define LCD_COLOR_TFT_16BPP	((16 << 4) | LCD_TYPE_COLOR_TFT)
#define LCD_COLOR_TFT_18BPP	((18 << 4) | LCD_TYPE_COLOR_TFT)
#define LCD_SMART_PANEL_8BPP	((8  << 4) | LCD_TYPE_SMART_PANEL)
#define LCD_SMART_PANEL_16BPP	((16 << 4) | LCD_TYPE_SMART_PANEL)
#define LCD_SMART_PANEL_18BPP	((18 << 4) | LCD_TYPE_SMART_PANEL)

#define LCD_AC_BIAS_FREQ(x)	(((x) & 0xff) << 10)
#define LCD_BIAS_ACTIVE_HIGH	(0 << 18)
#define LCD_BIAS_ACTIVE_LOW	(1 << 18)
#define LCD_PCLK_EDGE_RISE	(0 << 19)
#define LCD_PCLK_EDGE_FALL	(1 << 19)
#define LCD_ALTERNATE_MAPPING	(1 << 20)

struct pxafb_mode_info {
	u_long		pixclock;

	u_short		xres;
	u_short		yres;

	u_char		bpp;
	u_int		cmap_greyscale:1,
			depth:8,
			transparency:1,
			unused:22;

	
	u_char		hsync_len;
	u_char		left_margin;
	u_char		right_margin;

	u_char		vsync_len;
	u_char		upper_margin;
	u_char		lower_margin;
	u_char		sync;

	unsigned	a0csrd_set_hld;	
	unsigned	a0cswr_set_hld;	
	unsigned	wr_pulse_width;	
	unsigned	rd_pulse_width;	
	unsigned	cmd_inh_time;	
	unsigned	op_hold_time;	
};

struct pxafb_mach_info {
	struct pxafb_mode_info *modes;
	unsigned int num_modes;

	unsigned int	lcd_conn;
	unsigned long	video_mem_size;

	u_int		fixed_modes:1,
			cmap_inverse:1,
			cmap_static:1,
			acceleration_enabled:1,
			unused:28;

	u_int		lccr0;
	u_int		lccr3;
	u_int		lccr4;
	void (*pxafb_backlight_power)(int);
	void (*pxafb_lcd_power)(int, struct fb_var_screeninfo *);
	void (*smart_update)(struct fb_info *);
};

void pxa_set_fb_info(struct device *, struct pxafb_mach_info *);
unsigned long pxafb_get_hsync_time(struct device *dev);

#ifdef CONFIG_FB_PXA_SMARTPANEL
extern int pxafb_smart_queue(struct fb_info *info, uint16_t *cmds, int);
extern int pxafb_smart_flush(struct fb_info *info);
#else
static inline int pxafb_smart_queue(struct fb_info *info,
				    uint16_t *cmds, int n)
{
	return 0;
}

static inline int pxafb_smart_flush(struct fb_info *info)
{
	return 0;
}
#endif
