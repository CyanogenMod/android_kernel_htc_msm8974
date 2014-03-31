/*
 * Blackfin LCD Framebuffer driver SHARP LQ035Q1DH02
 *
 * Copyright 2008-2009 Analog Devices Inc.
 * Licensed under the GPL-2 or later.
 */

#ifndef BFIN_LQ035Q1_H
#define BFIN_LQ035Q1_H

#define LQ035_RL	(0 << 8)	
#define LQ035_LR	(1 << 8)	
#define LQ035_TB	(1 << 9)	
#define LQ035_BT	(0 << 9)	
#define LQ035_BGR	(1 << 11)	
#define LQ035_RGB	(0 << 11)	
#define LQ035_NORM	(1 << 13)	
#define LQ035_REV	(0 << 13)	


#define USE_RGB565_16_BIT_PPI	1
#define USE_RGB565_8_BIT_PPI	2
#define USE_RGB888_8_BIT_PPI	3

struct bfin_lq035q1fb_disp_info {

	unsigned	mode;
	unsigned	ppi_mode;
	
	int		use_bl;
	unsigned 	gpio_bl;
};

#endif 
