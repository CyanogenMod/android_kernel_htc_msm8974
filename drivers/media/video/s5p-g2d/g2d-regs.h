/*
 * Samsung S5P G2D - 2D Graphics Accelerator Driver
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Kamil Debski, <k.debski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */

#define SOFT_RESET_REG		0x0000	
#define INTEN_REG		0x0004	
#define INTC_PEND_REG		0x000C	
#define FIFO_STAT_REG		0x0010	
#define AXI_ID_MODE_REG		0x0014	
#define CACHECTL_REG		0x0018	
#define AXI_MODE_REG		0x001C	

#define BITBLT_START_REG	0x0100	
#define BITBLT_COMMAND_REG	0x0104	

#define ROTATE_REG		0x0200	
#define SRC_MSK_DIRECT_REG	0x0204	
#define DST_PAT_DIRECT_REG	0x0208	

#define SRC_SELECT_REG		0x0300	
#define SRC_BASE_ADDR_REG	0x0304	
#define SRC_STRIDE_REG		0x0308	
#define SRC_COLOR_MODE_REG	0x030C	
#define SRC_LEFT_TOP_REG	0x0310	
#define SRC_RIGHT_BOTTOM_REG	0x0314	

#define DST_SELECT_REG		0x0400	
#define DST_BASE_ADDR_REG	0x0404	
#define DST_STRIDE_REG		0x0408	
#define DST_COLOR_MODE_REG	0x040C	
#define DST_LEFT_TOP_REG	0x0410	
#define DST_RIGHT_BOTTOM_REG	0x0414	

#define PAT_BASE_ADDR_REG	0x0500	
#define PAT_SIZE_REG		0x0504	
#define PAT_COLOR_MODE_REG	0x0508	
#define PAT_OFFSET_REG		0x050C	
#define PAT_STRIDE_REG		0x0510	

#define MASK_BASE_ADDR_REG	0x0520	
#define MASK_STRIDE_REG		0x0524	

#define CW_LT_REG		0x0600	
#define CW_RB_REG		0x0604	

#define THIRD_OPERAND_REG	0x0610	
#define ROP4_REG		0x0614	
#define ALPHA_REG		0x0618	

#define FG_COLOR_REG		0x0700	
#define BG_COLOR_REG		0x0704	
#define BS_COLOR_REG		0x0708	

#define SRC_COLORKEY_CTRL_REG	0x0710	
#define SRC_COLORKEY_DR_MIN_REG	0x0714	
#define SRC_COLORKEY_DR_MAX_REG	0x0718	
#define DST_COLORKEY_CTRL_REG	0x071C	
#define DST_COLORKEY_DR_MIN_REG	0x0720	
#define DST_COLORKEY_DR_MAX_REG	0x0724	


#define ORDER_XRGB		0
#define ORDER_RGBX		1
#define ORDER_XBGR		2
#define ORDER_BGRX		3

#define MODE_XRGB_8888		0
#define MODE_ARGB_8888		1
#define MODE_RGB_565		2
#define MODE_XRGB_1555		3
#define MODE_ARGB_1555		4
#define MODE_XRGB_4444		5
#define MODE_ARGB_4444		6
#define MODE_PACKED_RGB_888	7

#define COLOR_MODE(o, m)	(((o) << 4) | (m))

#define ROP4_COPY		0xCCCC
#define ROP4_INVERT		0x3333

#define MAX_WIDTH		8000
#define MAX_HEIGHT		8000

#define G2D_TIMEOUT		500

#define DEFAULT_WIDTH		100
#define DEFAULT_HEIGHT		100

