/*
 *  linux/drivers/video/kyro/kryo.h
 *
 *  Copyright (C) 2002 STMicroelectronics
 *  Copyright (C) 2004 Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#ifndef _KYRO_H
#define _KYRO_H

struct kyrofb_info {
	void __iomem *regbase;

	u32 palette[16];
	u32 HTot;	
	u32 HFP;	
	u32 HST;	
	u32 HBP;	
	s32 HSP;		
	u32 VTot;	
	u32 VFP;	
	u32 VST;	
	u32 VBP;	
	s32 VSP;		
	u32 XRES;	
	u32 YRES;	
	u32 VFREQ;	
	u32 PIXCLK;	
	u32 HCLK;	

	
	u8 PIXDEPTH;

#ifdef CONFIG_MTRR
	int mtrr_handle;
#endif
};

extern int kyro_dev_init(void);
extern void kyro_dev_reset(void);

extern unsigned char *kyro_dev_physical_fb_ptr(void);
extern unsigned char *kyro_dev_virtual_fb_ptr(void);
extern void *kyro_dev_physical_regs_ptr(void);
extern void *kyro_dev_virtual_regs_ptr(void);
extern unsigned int kyro_dev_fb_size(void);
extern unsigned int kyro_dev_regs_size(void);

extern u32 kyro_dev_overlay_offset(void);

#define KYRO_IOC_MAGIC 'k'

#define KYRO_IOCTL_OVERLAY_CREATE       _IO(KYRO_IOC_MAGIC, 0)
#define KYRO_IOCTL_OVERLAY_VIEWPORT_SET _IO(KYRO_IOC_MAGIC, 1)
#define KYRO_IOCTL_SET_VIDEO_MODE       _IO(KYRO_IOC_MAGIC, 2)
#define KYRO_IOCTL_UVSTRIDE             _IO(KYRO_IOC_MAGIC, 3)
#define KYRO_IOCTL_OVERLAY_OFFSET       _IO(KYRO_IOC_MAGIC, 4)
#define KYRO_IOCTL_STRIDE               _IO(KYRO_IOC_MAGIC, 5)

typedef struct _OVERLAY_CREATE {
	u32 ulWidth;
	u32 ulHeight;
	int bLinear;
} overlay_create;

typedef struct _OVERLAY_VIEWPORT_SET {
	u32 xOrgin;
	u32 yOrgin;
	u32 xSize;
	u32 ySize;
} overlay_viewport_set;

typedef struct _SET_VIDEO_MODE {
	u32 ulWidth;
	u32 ulHeight;
	u32 ulScan;
	u8 displayDepth;
	int bLinear;
} set_video_mode;

#endif 
