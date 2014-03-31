/*
 * SiS 300/540/630[S]/730[S],
 * SiS 315[E|PRO]/550/[M]650/651/[M]661[F|M]X/740/[M]741[GX]/330/[M]760[GX],
 * XGI V3XT/V5/V8, Z7
 * frame buffer driver for Linux kernels >= 2.4.14 and >=2.6.3
 *
 * 2D acceleration part
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Based on the X driver's sis300_accel.h which is
 *     Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
 * and sis310_accel.h which is
 *     Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
 *
 * Author:   Thomas Winischhofer <thomas@winischhofer.net>:
 *			(see http://www.winischhofer.net/
 *			for more information and updates)
 */

#ifndef _SISFB_ACCEL_H
#define _SISFB_ACCEL_H

#undef SISFB_USE_SPINLOCKS

#ifdef SISFB_USE_SPINLOCKS
#include <linux/spinlock.h>
#define CRITBEGIN  spin_lock_irqsave(&ivideo->lockaccel, critflags);
#define CRITEND	   spin_unlock_irqrestore(&ivideo->lockaccel, critflags);
#define CRITFLAGS  unsigned long critflags;
#else
#define CRITBEGIN
#define CRITEND
#define CRITFLAGS
#endif


#define PATREGSIZE      384  
#define BR(x)   (0x8200 | (x) << 2)
#define PBR(x)  (0x8300 | (x) << 2)

#define BITBLT                  0x00000000  
#define COLOREXP                0x00000001  
#define ENCOLOREXP              0x00000002  
#define MULTIPLE_SCANLINE       0x00000003  
#define LINE                    0x00000004  
#define TRAPAZOID_FILL          0x00000005  
#define TRANSPARENT_BITBLT      0x00000006  

#define ALPHA_BLEND		0x00000007  
#define A3D_FUNCTION		0x00000008  
#define	CLEAR_Z_BUFFER		0x00000009  
#define GRADIENT_FILL		0x0000000A  

#define SRCVIDEO                0x00000000  
#define SRCSYSTEM               0x00000010  
#define SRCCPUBLITBUF           SRCSYSTEM   
#define SRCAGP                  0x00000020  

#define PATFG                   0x00000000  
#define PATPATREG               0x00000040  
#define PATMONO                 0x00000080  

#define X_INC                   0x00010000
#define X_DEC                   0x00000000
#define Y_INC                   0x00020000
#define Y_DEC                   0x00000000

#define NOCLIP                  0x00000000
#define NOMERGECLIP             0x04000000
#define CLIPENABLE              0x00040000
#define CLIPWITHOUTMERGE        0x04040000

#define OPAQUE                  0x00000000
#define TRANSPARENT             0x00100000

#define DSTAGP                  0x02000000
#define DSTVIDEO                0x02000000

#define COLOR_TO_MONO		0x00100000
#define AA_TEXT			0x00200000

#define SRC_ADDR		0x8200
#define SRC_PITCH		0x8204
#define AGP_BASE		0x8206 
#define SRC_Y			0x8208
#define SRC_X			0x820A
#define DST_Y			0x820C
#define DST_X			0x820E
#define DST_ADDR		0x8210
#define DST_PITCH		0x8214
#define DST_HEIGHT		0x8216
#define RECT_WIDTH		0x8218
#define RECT_HEIGHT		0x821A
#define PAT_FGCOLOR		0x821C
#define PAT_BGCOLOR		0x8220
#define SRC_FGCOLOR		0x8224
#define SRC_BGCOLOR		0x8228
#define MONO_MASK		0x822C
#define LEFT_CLIP		0x8234
#define TOP_CLIP		0x8236
#define RIGHT_CLIP		0x8238
#define BOTTOM_CLIP		0x823A
#define COMMAND_READY		0x823C
#define FIRE_TRIGGER      	0x8240

#define PATTERN_REG		0x8300  

#define TRANS_DST_KEY_HIGH	PAT_FGCOLOR
#define TRANS_DST_KEY_LOW	PAT_BGCOLOR
#define TRANS_SRC_KEY_HIGH	SRC_FGCOLOR
#define TRANS_SRC_KEY_LOW	SRC_BGCOLOR

#define CmdQueLen ivideo->cmdqueuelength



#define SiS300Idle \
  { \
  	while((MMIO_IN16(ivideo->mmio_vbase, BR(16)+2) & 0xE000) != 0xE000){}; \
  	while((MMIO_IN16(ivideo->mmio_vbase, BR(16)+2) & 0xE000) != 0xE000){}; \
  	while((MMIO_IN16(ivideo->mmio_vbase, BR(16)+2) & 0xE000) != 0xE000){}; \
  	CmdQueLen = MMIO_IN16(ivideo->mmio_vbase, 0x8240); \
  }

#define SiS300SetupSRCBase(base) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(0), base);\
	CmdQueLen--;

#define SiS300SetupSRCPitch(pitch) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT16(ivideo->mmio_vbase, BR(1), pitch);\
	CmdQueLen--;

#define SiS300SetupSRCXY(x,y) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(2), (x)<<16 | (y) );\
	CmdQueLen--;

#define SiS300SetupDSTBase(base) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(4), base);\
	CmdQueLen--;

#define SiS300SetupDSTXY(x,y) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(3), (x)<<16 | (y) );\
	CmdQueLen--;

#define SiS300SetupDSTRect(x,y) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(5), (y)<<16 | (x) );\
	CmdQueLen--;

#define SiS300SetupDSTColorDepth(bpp) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT16(ivideo->mmio_vbase, BR(1)+2, bpp);\
	CmdQueLen--;

#define SiS300SetupRect(w,h) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(6), (h)<<16 | (w) );\
	CmdQueLen--;

#define SiS300SetupPATFG(color) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(7), color);\
	CmdQueLen--;

#define SiS300SetupPATBG(color) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(8), color);\
	CmdQueLen--;

#define SiS300SetupSRCFG(color) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(9), color);\
	CmdQueLen--;

#define SiS300SetupSRCBG(color) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(10), color);\
	CmdQueLen--;

#define SiS300SetupSRCTrans(color) \
	if(CmdQueLen <= 1) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, 0x8224, color);\
	MMIO_OUT32(ivideo->mmio_vbase, 0x8228, color);\
	CmdQueLen -= 2;

#define SiS300SetupDSTTrans(color) \
	if(CmdQueLen <= 1) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, 0x821C, color); \
	MMIO_OUT32(ivideo->mmio_vbase, 0x8220, color); \
	CmdQueLen -= 2;

#define SiS300SetupMONOPAT(p0,p1) \
	if(CmdQueLen <= 1) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(11), p0);\
	MMIO_OUT32(ivideo->mmio_vbase, BR(12), p1);\
	CmdQueLen -= 2;

#define SiS300SetupClipLT(left,top) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(13), ((left) & 0xFFFF) | (top)<<16 );\
	CmdQueLen--;

#define SiS300SetupClipRB(right,bottom) \
	if(CmdQueLen <= 0) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(14), ((right) & 0xFFFF) | (bottom)<<16 );\
	CmdQueLen--;

#define SiS300SetupROP(rop) \
	ivideo->CommandReg = (rop) << 8;

#define SiS300SetupCMDFlag(flags) \
	ivideo->CommandReg |= (flags);

#define SiS300DoCMD \
	if(CmdQueLen <= 1) SiS300Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, BR(15), ivideo->CommandReg); \
	MMIO_OUT32(ivideo->mmio_vbase, BR(16), 0);\
	CmdQueLen -= 2;



#define SiS310Idle \
  { \
  	while( (MMIO_IN16(ivideo->mmio_vbase, Q_STATUS+2) & 0x8000) != 0x8000){}; \
  	while( (MMIO_IN16(ivideo->mmio_vbase, Q_STATUS+2) & 0x8000) != 0x8000){}; \
	while( (MMIO_IN16(ivideo->mmio_vbase, Q_STATUS+2) & 0x8000) != 0x8000){}; \
  	while( (MMIO_IN16(ivideo->mmio_vbase, Q_STATUS+2) & 0x8000) != 0x8000){}; \
  	CmdQueLen = 0; \
  }

#define SiS310SetupSRCBase(base) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, SRC_ADDR, base);\
	CmdQueLen--;

#define SiS310SetupSRCPitch(pitch) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT16(ivideo->mmio_vbase, SRC_PITCH, pitch);\
	CmdQueLen--;

#define SiS310SetupSRCXY(x,y) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, SRC_Y, (x)<<16 | (y) );\
	CmdQueLen--;

#define SiS310SetupDSTBase(base) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, DST_ADDR, base);\
	CmdQueLen--;

#define SiS310SetupDSTXY(x,y) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, DST_Y, (x)<<16 | (y) );\
	CmdQueLen--;

#define SiS310SetupDSTRect(x,y) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, DST_PITCH, (y)<<16 | (x) );\
	CmdQueLen--;

#define SiS310SetupDSTColorDepth(bpp) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT16(ivideo->mmio_vbase, AGP_BASE, bpp);\
	CmdQueLen--;

#define SiS310SetupRect(w,h) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, RECT_WIDTH, (h)<<16 | (w) );\
	CmdQueLen--;

#define SiS310SetupPATFG(color) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, PAT_FGCOLOR, color);\
	CmdQueLen--;

#define SiS310SetupPATBG(color) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, PAT_BGCOLOR, color);\
	CmdQueLen--;

#define SiS310SetupSRCFG(color) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, SRC_FGCOLOR, color);\
	CmdQueLen--;

#define SiS310SetupSRCBG(color) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, SRC_BGCOLOR, color);\
	CmdQueLen--;

#define SiS310SetupSRCTrans(color) \
	if(CmdQueLen <= 1) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, TRANS_SRC_KEY_HIGH, color);\
	MMIO_OUT32(ivideo->mmio_vbase, TRANS_SRC_KEY_LOW, color);\
	CmdQueLen -= 2;

#define SiS310SetupDSTTrans(color) \
	if(CmdQueLen <= 1) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, TRANS_DST_KEY_HIGH, color); \
	MMIO_OUT32(ivideo->mmio_vbase, TRANS_DST_KEY_LOW, color); \
	CmdQueLen -= 2;

#define SiS310SetupMONOPAT(p0,p1) \
	if(CmdQueLen <= 1) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, MONO_MASK, p0);\
	MMIO_OUT32(ivideo->mmio_vbase, MONO_MASK+4, p1);\
	CmdQueLen -= 2;

#define SiS310SetupClipLT(left,top) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, LEFT_CLIP, ((left) & 0xFFFF) | (top)<<16 );\
	CmdQueLen--;

#define SiS310SetupClipRB(right,bottom) \
	if(CmdQueLen <= 0) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, RIGHT_CLIP, ((right) & 0xFFFF) | (bottom)<<16 );\
	CmdQueLen--;

#define SiS310SetupROP(rop) \
	ivideo->CommandReg = (rop) << 8;

#define SiS310SetupCMDFlag(flags) \
	ivideo->CommandReg |= (flags);

#define SiS310DoCMD \
	if(CmdQueLen <= 1) SiS310Idle;\
	MMIO_OUT32(ivideo->mmio_vbase, COMMAND_READY, ivideo->CommandReg); \
	MMIO_OUT32(ivideo->mmio_vbase, FIRE_TRIGGER, 0); \
	CmdQueLen -= 2;

int  sisfb_initaccel(struct sis_video_info *ivideo);
void sisfb_syncaccel(struct sis_video_info *ivideo);

int  fbcon_sis_sync(struct fb_info *info);
void fbcon_sis_fillrect(struct fb_info *info, const struct fb_fillrect *rect);
void fbcon_sis_copyarea(struct fb_info *info, const struct fb_copyarea *area);

#endif
