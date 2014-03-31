/*
 * linux/include/video/vga.h -- standard VGA chipset interaction
 *
 * Copyright 1999 Jeff Garzik <jgarzik@pobox.com>
 * 
 * Copyright history from vga16fb.c:
 *	Copyright 1999 Ben Pfaff and Petr Vandrovec
 *	Based on VGA info at http://www.osdever.net/FreeVGA/home.htm 
 *	Based on VESA framebuffer (c) 1998 Gerd Knorr
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.  
 *
 */

#ifndef __linux_video_vga_h__
#define __linux_video_vga_h__

#include <linux/types.h>
#include <asm/io.h>
#ifndef CONFIG_AMIGA
#include <asm/vga.h>
#else
#undef inb_p
#undef inw_p
#undef outb_p
#undef outw
#undef readb
#undef writeb
#undef writew
#define inb_p(port)	0
#define inw_p(port)	0
#define outb_p(port, val)	do { } while (0)
#define outw(port, val)		do { } while (0)
#define readb		z_readb
#define writeb		z_writeb
#define writew		z_writew
#endif
#include <asm/byteorder.h>


/* Some of the code below is taken from SVGAlib.  The original,
   unmodified copyright notice for that code is below. */

/* Multi-chipset support Copyright 1993 Harm Hanemaayer */
/* partially copyrighted (C) 1993 by Hartmut Schirmer */

#define VGA_CRT_DC  	0x3D5	
#define VGA_CRT_DM  	0x3B5	
#define VGA_ATT_R   	0x3C1	
#define VGA_ATT_W   	0x3C0	
#define VGA_GFX_D   	0x3CF	
#define VGA_SEQ_D   	0x3C5	
#define VGA_MIS_R   	0x3CC	
#define VGA_MIS_W   	0x3C2	
#define VGA_FTC_R	0x3CA	
#define VGA_IS1_RC  	0x3DA	
#define VGA_IS1_RM  	0x3BA	
#define VGA_PEL_D   	0x3C9	
#define VGA_PEL_MSK 	0x3C6	

#define EGA_GFX_E0	0x3CC	
#define EGA_GFX_E1	0x3CA	

#define VGA_CRT_IC  	0x3D4	
#define VGA_CRT_IM  	0x3B4	
#define VGA_ATT_IW  	0x3C0	
#define VGA_GFX_I   	0x3CE	
#define VGA_SEQ_I   	0x3C4	
#define VGA_PEL_IW  	0x3C8	
#define VGA_PEL_IR  	0x3C7	

#define VGA_CRT_C   	0x19	
#define VGA_ATT_C   	0x15	
#define VGA_GFX_C   	0x09	
#define VGA_SEQ_C   	0x05	
#define VGA_MIS_C   	0x01	

#define VGA_MIS_COLOR		0x01
#define VGA_MIS_ENB_MEM_ACCESS	0x02
#define VGA_MIS_DCLK_28322_720	0x04
#define VGA_MIS_ENB_PLL_LOAD	(0x04 | 0x08)
#define VGA_MIS_SEL_HIGH_PAGE	0x20

#define VGA_CRTC_H_TOTAL	0
#define VGA_CRTC_H_DISP		1
#define VGA_CRTC_H_BLANK_START	2
#define VGA_CRTC_H_BLANK_END	3
#define VGA_CRTC_H_SYNC_START	4
#define VGA_CRTC_H_SYNC_END	5
#define VGA_CRTC_V_TOTAL	6
#define VGA_CRTC_OVERFLOW	7
#define VGA_CRTC_PRESET_ROW	8
#define VGA_CRTC_MAX_SCAN	9
#define VGA_CRTC_CURSOR_START	0x0A
#define VGA_CRTC_CURSOR_END	0x0B
#define VGA_CRTC_START_HI	0x0C
#define VGA_CRTC_START_LO	0x0D
#define VGA_CRTC_CURSOR_HI	0x0E
#define VGA_CRTC_CURSOR_LO	0x0F
#define VGA_CRTC_V_SYNC_START	0x10
#define VGA_CRTC_V_SYNC_END	0x11
#define VGA_CRTC_V_DISP_END	0x12
#define VGA_CRTC_OFFSET		0x13
#define VGA_CRTC_UNDERLINE	0x14
#define VGA_CRTC_V_BLANK_START	0x15
#define VGA_CRTC_V_BLANK_END	0x16
#define VGA_CRTC_MODE		0x17
#define VGA_CRTC_LINE_COMPARE	0x18
#define VGA_CRTC_REGS		VGA_CRT_C

#define VGA_CR11_LOCK_CR0_CR7	0x80 
#define VGA_CR17_H_V_SIGNALS_ENABLED 0x80

#define VGA_ATC_PALETTE0	0x00
#define VGA_ATC_PALETTE1	0x01
#define VGA_ATC_PALETTE2	0x02
#define VGA_ATC_PALETTE3	0x03
#define VGA_ATC_PALETTE4	0x04
#define VGA_ATC_PALETTE5	0x05
#define VGA_ATC_PALETTE6	0x06
#define VGA_ATC_PALETTE7	0x07
#define VGA_ATC_PALETTE8	0x08
#define VGA_ATC_PALETTE9	0x09
#define VGA_ATC_PALETTEA	0x0A
#define VGA_ATC_PALETTEB	0x0B
#define VGA_ATC_PALETTEC	0x0C
#define VGA_ATC_PALETTED	0x0D
#define VGA_ATC_PALETTEE	0x0E
#define VGA_ATC_PALETTEF	0x0F
#define VGA_ATC_MODE		0x10
#define VGA_ATC_OVERSCAN	0x11
#define VGA_ATC_PLANE_ENABLE	0x12
#define VGA_ATC_PEL		0x13
#define VGA_ATC_COLOR_PAGE	0x14

#define VGA_AR_ENABLE_DISPLAY	0x20

#define VGA_SEQ_RESET		0x00
#define VGA_SEQ_CLOCK_MODE	0x01
#define VGA_SEQ_PLANE_WRITE	0x02
#define VGA_SEQ_CHARACTER_MAP	0x03
#define VGA_SEQ_MEMORY_MODE	0x04

#define VGA_SR01_CHAR_CLK_8DOTS	0x01 
#define VGA_SR01_SCREEN_OFF	0x20 
#define VGA_SR02_ALL_PLANES	0x0F 
#define VGA_SR04_EXT_MEM	0x02 
#define VGA_SR04_SEQ_MODE	0x04 
#define VGA_SR04_CHN_4M		0x08 

#define VGA_GFX_SR_VALUE	0x00
#define VGA_GFX_SR_ENABLE	0x01
#define VGA_GFX_COMPARE_VALUE	0x02
#define VGA_GFX_DATA_ROTATE	0x03
#define VGA_GFX_PLANE_READ	0x04
#define VGA_GFX_MODE		0x05
#define VGA_GFX_MISC		0x06
#define VGA_GFX_COMPARE_MASK	0x07
#define VGA_GFX_BIT_MASK	0x08

#define VGA_GR06_GRAPHICS_MODE	0x01

#define VGA_OUT16VAL(v, r)       (((v) << 8) | (r))

#ifdef __LITTLE_ENDIAN
#define VGA_OUTW_WRITE
#endif

#define VGA_SAVE_FONT0 1  
#define VGA_SAVE_FONT1 2  
#define VGA_SAVE_TEXT  4  
#define VGA_SAVE_FONTS 7  
#define VGA_SAVE_MODE  8  
#define VGA_SAVE_CMAP  16 

struct vgastate {
	void __iomem *vgabase;	
	unsigned long membase;	
	__u32 memsize;		
	__u32 flags;		
	__u32 depth;		
	__u32 num_attr;		
	__u32 num_crtc;		
	__u32 num_gfx;		
	__u32 num_seq;		
	void *vidstate;
};	

extern int save_vga(struct vgastate *state);
extern int restore_vga(struct vgastate *state);

 
static inline unsigned char vga_io_r (unsigned short port)
{
	return inb_p(port);
}

static inline void vga_io_w (unsigned short port, unsigned char val)
{
	outb_p(val, port);
}

static inline void vga_io_w_fast (unsigned short port, unsigned char reg,
				  unsigned char val)
{
	outw(VGA_OUT16VAL (val, reg), port);
}

static inline unsigned char vga_mm_r (void __iomem *regbase, unsigned short port)
{
	return readb (regbase + port);
}

static inline void vga_mm_w (void __iomem *regbase, unsigned short port, unsigned char val)
{
	writeb (val, regbase + port);
}

static inline void vga_mm_w_fast (void __iomem *regbase, unsigned short port,
				  unsigned char reg, unsigned char val)
{
	writew (VGA_OUT16VAL (val, reg), regbase + port);
}

static inline unsigned char vga_r (void __iomem *regbase, unsigned short port)
{
	if (regbase)
		return vga_mm_r (regbase, port);
	else
		return vga_io_r (port);
}

static inline void vga_w (void __iomem *regbase, unsigned short port, unsigned char val)
{
	if (regbase)
		vga_mm_w (regbase, port, val);
	else
		vga_io_w (port, val);
}


static inline void vga_w_fast (void __iomem *regbase, unsigned short port,
			       unsigned char reg, unsigned char val)
{
	if (regbase)
		vga_mm_w_fast (regbase, port, reg, val);
	else
		vga_io_w_fast (port, reg, val);
}


 
static inline unsigned char vga_rcrt (void __iomem *regbase, unsigned char reg)
{
        vga_w (regbase, VGA_CRT_IC, reg);
        return vga_r (regbase, VGA_CRT_DC);
}

static inline void vga_wcrt (void __iomem *regbase, unsigned char reg, unsigned char val)
{
#ifdef VGA_OUTW_WRITE
	vga_w_fast (regbase, VGA_CRT_IC, reg, val);
#else
        vga_w (regbase, VGA_CRT_IC, reg);
        vga_w (regbase, VGA_CRT_DC, val);
#endif 
}

static inline unsigned char vga_io_rcrt (unsigned char reg)
{
        vga_io_w (VGA_CRT_IC, reg);
        return vga_io_r (VGA_CRT_DC);
}

static inline void vga_io_wcrt (unsigned char reg, unsigned char val)
{
#ifdef VGA_OUTW_WRITE
	vga_io_w_fast (VGA_CRT_IC, reg, val);
#else
        vga_io_w (VGA_CRT_IC, reg);
        vga_io_w (VGA_CRT_DC, val);
#endif 
}

static inline unsigned char vga_mm_rcrt (void __iomem *regbase, unsigned char reg)
{
        vga_mm_w (regbase, VGA_CRT_IC, reg);
        return vga_mm_r (regbase, VGA_CRT_DC);
}

static inline void vga_mm_wcrt (void __iomem *regbase, unsigned char reg, unsigned char val)
{
#ifdef VGA_OUTW_WRITE
	vga_mm_w_fast (regbase, VGA_CRT_IC, reg, val);
#else
        vga_mm_w (regbase, VGA_CRT_IC, reg);
        vga_mm_w (regbase, VGA_CRT_DC, val);
#endif 
}


 
static inline unsigned char vga_rseq (void __iomem *regbase, unsigned char reg)
{
        vga_w (regbase, VGA_SEQ_I, reg);
        return vga_r (regbase, VGA_SEQ_D);
}

static inline void vga_wseq (void __iomem *regbase, unsigned char reg, unsigned char val)
{
#ifdef VGA_OUTW_WRITE
	vga_w_fast (regbase, VGA_SEQ_I, reg, val);
#else
        vga_w (regbase, VGA_SEQ_I, reg);
        vga_w (regbase, VGA_SEQ_D, val);
#endif 
}

static inline unsigned char vga_io_rseq (unsigned char reg)
{
        vga_io_w (VGA_SEQ_I, reg);
        return vga_io_r (VGA_SEQ_D);
}

static inline void vga_io_wseq (unsigned char reg, unsigned char val)
{
#ifdef VGA_OUTW_WRITE
	vga_io_w_fast (VGA_SEQ_I, reg, val);
#else
        vga_io_w (VGA_SEQ_I, reg);
        vga_io_w (VGA_SEQ_D, val);
#endif 
}

static inline unsigned char vga_mm_rseq (void __iomem *regbase, unsigned char reg)
{
        vga_mm_w (regbase, VGA_SEQ_I, reg);
        return vga_mm_r (regbase, VGA_SEQ_D);
}

static inline void vga_mm_wseq (void __iomem *regbase, unsigned char reg, unsigned char val)
{
#ifdef VGA_OUTW_WRITE
	vga_mm_w_fast (regbase, VGA_SEQ_I, reg, val);
#else
        vga_mm_w (regbase, VGA_SEQ_I, reg);
        vga_mm_w (regbase, VGA_SEQ_D, val);
#endif 
}

 
static inline unsigned char vga_rgfx (void __iomem *regbase, unsigned char reg)
{
        vga_w (regbase, VGA_GFX_I, reg);
        return vga_r (regbase, VGA_GFX_D);
}

static inline void vga_wgfx (void __iomem *regbase, unsigned char reg, unsigned char val)
{
#ifdef VGA_OUTW_WRITE
	vga_w_fast (regbase, VGA_GFX_I, reg, val);
#else
        vga_w (regbase, VGA_GFX_I, reg);
        vga_w (regbase, VGA_GFX_D, val);
#endif 
}

static inline unsigned char vga_io_rgfx (unsigned char reg)
{
        vga_io_w (VGA_GFX_I, reg);
        return vga_io_r (VGA_GFX_D);
}

static inline void vga_io_wgfx (unsigned char reg, unsigned char val)
{
#ifdef VGA_OUTW_WRITE
	vga_io_w_fast (VGA_GFX_I, reg, val);
#else
        vga_io_w (VGA_GFX_I, reg);
        vga_io_w (VGA_GFX_D, val);
#endif 
}

static inline unsigned char vga_mm_rgfx (void __iomem *regbase, unsigned char reg)
{
        vga_mm_w (regbase, VGA_GFX_I, reg);
        return vga_mm_r (regbase, VGA_GFX_D);
}

static inline void vga_mm_wgfx (void __iomem *regbase, unsigned char reg, unsigned char val)
{
#ifdef VGA_OUTW_WRITE
	vga_mm_w_fast (regbase, VGA_GFX_I, reg, val);
#else
        vga_mm_w (regbase, VGA_GFX_I, reg);
        vga_mm_w (regbase, VGA_GFX_D, val);
#endif 
}


 
static inline unsigned char vga_rattr (void __iomem *regbase, unsigned char reg)
{
        vga_w (regbase, VGA_ATT_IW, reg);
        return vga_r (regbase, VGA_ATT_R);
}

static inline void vga_wattr (void __iomem *regbase, unsigned char reg, unsigned char val)
{
        vga_w (regbase, VGA_ATT_IW, reg);
        vga_w (regbase, VGA_ATT_W, val);
}

static inline unsigned char vga_io_rattr (unsigned char reg)
{
        vga_io_w (VGA_ATT_IW, reg);
        return vga_io_r (VGA_ATT_R);
}

static inline void vga_io_wattr (unsigned char reg, unsigned char val)
{
        vga_io_w (VGA_ATT_IW, reg);
        vga_io_w (VGA_ATT_W, val);
}

static inline unsigned char vga_mm_rattr (void __iomem *regbase, unsigned char reg)
{
        vga_mm_w (regbase, VGA_ATT_IW, reg);
        return vga_mm_r (regbase, VGA_ATT_R);
}

static inline void vga_mm_wattr (void __iomem *regbase, unsigned char reg, unsigned char val)
{
        vga_mm_w (regbase, VGA_ATT_IW, reg);
        vga_mm_w (regbase, VGA_ATT_W, val);
}

#endif 
