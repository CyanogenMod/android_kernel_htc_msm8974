/*
 *	linux/include/video/pmagb-b-fb.h
 *
 *	TURBOchannel PMAGB-B Smart Frame Buffer (SFB) card support,
 *	Copyright (C) 1999, 2000, 2001 by
 *	Michael Engel <engel@unix-ag.org> and
 *	Karsten Merker <merker@linuxtag.org>
 *	Copyright (c) 2005  Maciej W. Rozycki
 *
 *	This file is subject to the terms and conditions of the GNU General
 *	Public License.  See the file COPYING in the main directory of this
 *	archive for more details.
 */

#define PMAGB_B_ROM		0x000000	
#define PMAGB_B_SFB		0x100000	
#define PMAGB_B_GP0		0x140000	
#define PMAGB_B_GP1		0x180000	
#define PMAGB_B_BT459		0x1c0000	
#define PMAGB_B_FBMEM		0x200000	
#define PMAGB_B_SIZE		0x400000	

#define SFB_REG_VID_HOR		0x64		
#define SFB_REG_VID_VER		0x68		
#define SFB_REG_VID_BASE	0x6c		
#define SFB_REG_TCCLK_COUNT	0x78		
#define SFB_REG_VIDCLK_COUNT	0x7c		

#define SFB_VID_HOR_BP_SHIFT	0x15		
#define SFB_VID_HOR_BP_MASK	0x7f
#define SFB_VID_HOR_SYN_SHIFT	0x0e		
#define SFB_VID_HOR_SYN_MASK	0x7f
#define SFB_VID_HOR_FP_SHIFT	0x09		
#define SFB_VID_HOR_FP_MASK	0x1f
#define SFB_VID_HOR_PIX_SHIFT	0x00		
#define SFB_VID_HOR_PIX_MASK	0x1ff

#define SFB_VID_VER_BP_SHIFT	0x16		
#define SFB_VID_VER_BP_MASK	0x3f
#define SFB_VID_VER_SYN_SHIFT	0x10		
#define SFB_VID_VER_SYN_MASK	0x3f
#define SFB_VID_VER_FP_SHIFT	0x0b		
#define SFB_VID_VER_FP_MASK	0x1f
#define SFB_VID_VER_SL_SHIFT	0x00		
#define SFB_VID_VER_SL_MASK	0x7ff

#define SFB_VID_BASE_MASK	0x1ff		

#define BT459_ADDR_LO		0x0		
#define BT459_ADDR_HI		0x4		
#define BT459_DATA		0x8		
#define BT459_CMAP		0xc		
