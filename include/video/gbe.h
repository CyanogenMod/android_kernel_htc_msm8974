/*
 * include/video/gbe.h -- SGI GBE (Graphics Back End)
 *
 * Copyright (C) 1999 Silicon Graphics, Inc. (Jeffrey Newquist)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 */

#ifndef __GBE_H__
#define __GBE_H__

struct sgi_gbe {
	volatile uint32_t ctrlstat;	
	volatile uint32_t dotclock;	
	volatile uint32_t i2c;		
	volatile uint32_t sysclk;	
	volatile uint32_t i2cfp;	
	volatile uint32_t id;		
	volatile uint32_t config;       
	volatile uint32_t bist;         
	uint32_t _pad0[0x010000/4 - 8];
	volatile uint32_t vt_xy;	
	volatile uint32_t vt_xymax;	
	volatile uint32_t vt_vsync;	
	volatile uint32_t vt_hsync;	
	volatile uint32_t vt_vblank;	
	volatile uint32_t vt_hblank;	
	volatile uint32_t vt_flags;	
	volatile uint32_t vt_f2rf_lock;	
	volatile uint32_t vt_intr01;	
	volatile uint32_t vt_intr23;	
	volatile uint32_t fp_hdrv;	
	volatile uint32_t fp_vdrv;	
	volatile uint32_t fp_de;	
	volatile uint32_t vt_hpixen;	
	volatile uint32_t vt_vpixen;	
	volatile uint32_t vt_hcmap;	
	volatile uint32_t vt_vcmap;	
	volatile uint32_t did_start_xy;	
	volatile uint32_t crs_start_xy;	
	volatile uint32_t vc_start_xy;	
	uint32_t _pad1[0xffb0/4];
	volatile uint32_t ovr_width_tile;
	volatile uint32_t ovr_inhwctrl;	
	volatile uint32_t ovr_control;	
	uint32_t _pad2[0xfff4/4];
	volatile uint32_t frm_size_tile;
	volatile uint32_t frm_size_pixel;
	volatile uint32_t frm_inhwctrl;	
	volatile uint32_t frm_control;	
	uint32_t _pad3[0xfff0/4];
	volatile uint32_t did_inhwctrl;	
	volatile uint32_t did_control;	
	uint32_t _pad4[0x7ff8/4];
	volatile uint32_t mode_regs[32];
	uint32_t _pad5[0x7f80/4];
	volatile uint32_t cmap[6144];	
	uint32_t _pad6[0x2000/4];
	volatile uint32_t cm_fifo;	
	uint32_t _pad7[0x7ffc/4];
	volatile uint32_t gmap[256];	
	uint32_t _pad8[0x7c00/4];
	volatile uint32_t gmap10[1024];	
	uint32_t _pad9[0x7000/4];
	volatile uint32_t crs_pos;	
	volatile uint32_t crs_ctl;	
	volatile uint32_t crs_cmap[3];	
	uint32_t _pad10[0x7fec/4];
	volatile uint32_t crs_glyph[64];
	uint32_t _pad11[0x7f00/4];
	volatile uint32_t vc_0;	
	volatile uint32_t vc_1;	
	volatile uint32_t vc_2;	
	volatile uint32_t vc_3;	
	volatile uint32_t vc_4;	
	volatile uint32_t vc_5;	
	volatile uint32_t vc_6;	
	volatile uint32_t vc_7;	
	volatile uint32_t vc_8;	
};

#define MASK(msb, lsb)		\
	( (((u32)1<<((msb)-(lsb)+1))-1) << (lsb) )
#define GET(v, msb, lsb)	\
	( ((u32)(v) & MASK(msb,lsb)) >> (lsb) )
#define SET(v, f, msb, lsb)	\
	( (v) = ((v)&~MASK(msb,lsb)) | (( (u32)(f)<<(lsb) ) & MASK(msb,lsb)) )

#define GET_GBE_FIELD(reg, field, v)		\
	GET((v), GBE_##reg##_##field##_MSB, GBE_##reg##_##field##_LSB)
#define SET_GBE_FIELD(reg, field, v, f)		\
	SET((v), (f), GBE_##reg##_##field##_MSB, GBE_##reg##_##field##_LSB)

#define GBE_CTRLSTAT_CHIPID_MSB		 3
#define GBE_CTRLSTAT_CHIPID_LSB		 0
#define GBE_CTRLSTAT_SENSE_N_MSB	 4
#define GBE_CTRLSTAT_SENSE_N_LSB	 4
#define GBE_CTRLSTAT_PCLKSEL_MSB	29
#define GBE_CTRLSTAT_PCLKSEL_LSB	28

#define GBE_DOTCLK_M_MSB		 7
#define GBE_DOTCLK_M_LSB		 0
#define GBE_DOTCLK_N_MSB		13
#define GBE_DOTCLK_N_LSB		 8
#define GBE_DOTCLK_P_MSB		15
#define GBE_DOTCLK_P_LSB		14
#define GBE_DOTCLK_RUN_MSB		20
#define GBE_DOTCLK_RUN_LSB		20

#define GBE_VT_XY_Y_MSB		23
#define GBE_VT_XY_Y_LSB		12
#define GBE_VT_XY_X_MSB		11
#define GBE_VT_XY_X_LSB		 0
#define GBE_VT_XY_FREEZE_MSB		31
#define GBE_VT_XY_FREEZE_LSB		31

#define GBE_FP_VDRV_ON_MSB	23
#define GBE_FP_VDRV_ON_LSB	12
#define GBE_FP_VDRV_OFF_MSB	11
#define GBE_FP_VDRV_OFF_LSB	0

#define GBE_FP_HDRV_ON_MSB	23
#define GBE_FP_HDRV_ON_LSB	12
#define GBE_FP_HDRV_OFF_MSB	11
#define GBE_FP_HDRV_OFF_LSB	0

#define GBE_FP_DE_ON_MSB		23
#define GBE_FP_DE_ON_LSB		12
#define GBE_FP_DE_OFF_MSB		11
#define GBE_FP_DE_OFF_LSB		0

#define GBE_VT_VSYNC_VSYNC_ON_MSB	23
#define GBE_VT_VSYNC_VSYNC_ON_LSB	12
#define GBE_VT_VSYNC_VSYNC_OFF_MSB	11
#define GBE_VT_VSYNC_VSYNC_OFF_LSB	 0

#define GBE_VT_HSYNC_HSYNC_ON_MSB	23
#define GBE_VT_HSYNC_HSYNC_ON_LSB	12
#define GBE_VT_HSYNC_HSYNC_OFF_MSB	11
#define GBE_VT_HSYNC_HSYNC_OFF_LSB	 0

#define GBE_VT_VBLANK_VBLANK_ON_MSB	23
#define GBE_VT_VBLANK_VBLANK_ON_LSB	12
#define GBE_VT_VBLANK_VBLANK_OFF_MSB	11
#define GBE_VT_VBLANK_VBLANK_OFF_LSB	 0

#define GBE_VT_HBLANK_HBLANK_ON_MSB	23
#define GBE_VT_HBLANK_HBLANK_ON_LSB	12
#define GBE_VT_HBLANK_HBLANK_OFF_MSB	11
#define GBE_VT_HBLANK_HBLANK_OFF_LSB	 0

#define GBE_VT_FLAGS_F2RF_HIGH_MSB	 6
#define GBE_VT_FLAGS_F2RF_HIGH_LSB	 6
#define GBE_VT_FLAGS_SYNC_LOW_MSB	 5
#define GBE_VT_FLAGS_SYNC_LOW_LSB	 5
#define GBE_VT_FLAGS_SYNC_HIGH_MSB	 4
#define GBE_VT_FLAGS_SYNC_HIGH_LSB	 4
#define GBE_VT_FLAGS_HDRV_LOW_MSB	 3
#define GBE_VT_FLAGS_HDRV_LOW_LSB	 3
#define GBE_VT_FLAGS_HDRV_INVERT_MSB	 2
#define GBE_VT_FLAGS_HDRV_INVERT_LSB	 2
#define GBE_VT_FLAGS_VDRV_LOW_MSB	 1
#define GBE_VT_FLAGS_VDRV_LOW_LSB	 1
#define GBE_VT_FLAGS_VDRV_INVERT_MSB	 0
#define GBE_VT_FLAGS_VDRV_INVERT_LSB	 0

#define GBE_VT_VCMAP_VCMAP_ON_MSB	23
#define GBE_VT_VCMAP_VCMAP_ON_LSB	12
#define GBE_VT_VCMAP_VCMAP_OFF_MSB	11
#define GBE_VT_VCMAP_VCMAP_OFF_LSB	 0

#define GBE_VT_HCMAP_HCMAP_ON_MSB	23
#define GBE_VT_HCMAP_HCMAP_ON_LSB	12
#define GBE_VT_HCMAP_HCMAP_OFF_MSB	11
#define GBE_VT_HCMAP_HCMAP_OFF_LSB	 0

#define GBE_VT_XYMAX_MAXX_MSB	11
#define GBE_VT_XYMAX_MAXX_LSB	 0
#define GBE_VT_XYMAX_MAXY_MSB	23
#define GBE_VT_XYMAX_MAXY_LSB	12

#define GBE_VT_HPIXEN_HPIXEN_ON_MSB	23
#define GBE_VT_HPIXEN_HPIXEN_ON_LSB	12
#define GBE_VT_HPIXEN_HPIXEN_OFF_MSB	11
#define GBE_VT_HPIXEN_HPIXEN_OFF_LSB	 0

#define GBE_VT_VPIXEN_VPIXEN_ON_MSB	23
#define GBE_VT_VPIXEN_VPIXEN_ON_LSB	12
#define GBE_VT_VPIXEN_VPIXEN_OFF_MSB	11
#define GBE_VT_VPIXEN_VPIXEN_OFF_LSB	 0

#define GBE_OVR_CONTROL_OVR_DMA_ENABLE_MSB	 0
#define GBE_OVR_CONTROL_OVR_DMA_ENABLE_LSB	 0

#define GBE_OVR_INHWCTRL_OVR_DMA_ENABLE_MSB	 0
#define GBE_OVR_INHWCTRL_OVR_DMA_ENABLE_LSB	 0

#define GBE_OVR_WIDTH_TILE_OVR_FIFO_RESET_MSB	13
#define GBE_OVR_WIDTH_TILE_OVR_FIFO_RESET_LSB	13

#define GBE_FRM_CONTROL_FRM_DMA_ENABLE_MSB	 0
#define GBE_FRM_CONTROL_FRM_DMA_ENABLE_LSB	 0
#define GBE_FRM_CONTROL_FRM_TILE_PTR_MSB	31
#define GBE_FRM_CONTROL_FRM_TILE_PTR_LSB	 9
#define GBE_FRM_CONTROL_FRM_LINEAR_MSB		 1
#define GBE_FRM_CONTROL_FRM_LINEAR_LSB		 1

#define GBE_FRM_INHWCTRL_FRM_DMA_ENABLE_MSB	 0
#define GBE_FRM_INHWCTRL_FRM_DMA_ENABLE_LSB	 0

#define GBE_FRM_SIZE_TILE_FRM_WIDTH_TILE_MSB	12
#define GBE_FRM_SIZE_TILE_FRM_WIDTH_TILE_LSB	 5
#define GBE_FRM_SIZE_TILE_FRM_RHS_MSB		 4
#define GBE_FRM_SIZE_TILE_FRM_RHS_LSB		 0
#define GBE_FRM_SIZE_TILE_FRM_DEPTH_MSB		14
#define GBE_FRM_SIZE_TILE_FRM_DEPTH_LSB		13
#define GBE_FRM_SIZE_TILE_FRM_FIFO_RESET_MSB	15
#define GBE_FRM_SIZE_TILE_FRM_FIFO_RESET_LSB	15

#define GBE_FRM_SIZE_PIXEL_FB_HEIGHT_PIX_MSB	31
#define GBE_FRM_SIZE_PIXEL_FB_HEIGHT_PIX_LSB	16

#define GBE_DID_CONTROL_DID_DMA_ENABLE_MSB	 0
#define GBE_DID_CONTROL_DID_DMA_ENABLE_LSB	 0
#define GBE_DID_INHWCTRL_DID_DMA_ENABLE_MSB	 0
#define GBE_DID_INHWCTRL_DID_DMA_ENABLE_LSB	 0

#define GBE_DID_START_XY_DID_STARTY_MSB		23
#define GBE_DID_START_XY_DID_STARTY_LSB		12
#define GBE_DID_START_XY_DID_STARTX_MSB		11
#define GBE_DID_START_XY_DID_STARTX_LSB		 0

#define GBE_CRS_START_XY_CRS_STARTY_MSB		23
#define GBE_CRS_START_XY_CRS_STARTY_LSB		12
#define GBE_CRS_START_XY_CRS_STARTX_MSB		11
#define GBE_CRS_START_XY_CRS_STARTX_LSB		 0

#define GBE_WID_AUX_MSB		12
#define GBE_WID_AUX_LSB		11
#define GBE_WID_GAMMA_MSB	10
#define GBE_WID_GAMMA_LSB	10
#define GBE_WID_CM_MSB		 9
#define GBE_WID_CM_LSB		 5
#define GBE_WID_TYP_MSB		 4
#define GBE_WID_TYP_LSB		 2
#define GBE_WID_BUF_MSB		 1
#define GBE_WID_BUF_LSB		 0

#define GBE_VC_START_XY_VC_STARTY_MSB	23
#define GBE_VC_START_XY_VC_STARTY_LSB	12
#define GBE_VC_START_XY_VC_STARTX_MSB	11
#define GBE_VC_START_XY_VC_STARTX_LSB	 0


#define GBE_FRM_DEPTH_8		0
#define GBE_FRM_DEPTH_16	1
#define GBE_FRM_DEPTH_32	2

#define GBE_CMODE_I8		0
#define GBE_CMODE_I12		1
#define GBE_CMODE_RG3B2		2
#define GBE_CMODE_RGB4		3
#define GBE_CMODE_ARGB5		4
#define GBE_CMODE_RGB8		5
#define GBE_CMODE_RGBA5		6
#define GBE_CMODE_RGB10		7

#define GBE_BMODE_BOTH		3

#define GBE_CRS_MAGIC		54
#define GBE_PIXEN_MAGIC_ON	19
#define GBE_PIXEN_MAGIC_OFF	 2

#define GBE_TLB_SIZE		128



struct gbe_timing_info {
	int flags;
	short width;		
	short height;
	int fields_sec;		
	int cfreq;		
	short htotal;		
	short hblank_start;	
	short hblank_end;	
	short hsync_start;	
	short hsync_end;	
	short vtotal;		
	short vblank_start;	
	short vblank_end;	
	short vsync_start;	
	short vsync_end;	
	short pll_m;		
	short pll_n;		
	short pll_p;		
};


#define GBE_VOF_UNKNOWNMON	1
#define GBE_VOF_STEREO		2
#define GBE_VOF_DO_GENSYNC	4	
#define GBE_VOF_SYNC_ON_GREEN	8	
#define GBE_VOF_FLATPANEL	0x1000	
#define GBE_VOF_MAGICKEY	0x2000	

#endif		
