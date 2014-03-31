/*
 * VIDEO MOTION CODECs internal API for video devices
 *
 * Interface for MJPEG (and maybe later MPEG/WAVELETS) codec's
 * bound to a master device.
 *
 * (c) 2002 Wolfgang Scherr <scherr@net4you.at>
 *
 * $Id: videocodec.h,v 1.1.2.4 2003/01/14 21:15:03 rbultje Exp $
 *
 * ------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * ------------------------------------------------------------------------
 */










#ifndef __LINUX_VIDEOCODEC_H
#define __LINUX_VIDEOCODEC_H

#include <linux/videodev2.h>

#define CODEC_DO_COMPRESSION 0
#define CODEC_DO_EXPANSION   1

#define CODEC_FLAG_JPEG      0x00000001L	
#define CODEC_FLAG_MPEG      0x00000002L	
#define CODEC_FLAG_DIVX      0x00000004L	
#define CODEC_FLAG_WAVELET   0x00000008L	
					  

#define CODEC_FLAG_MAGIC     0x00000800L	
#define CODEC_FLAG_HARDWARE  0x00001000L	
#define CODEC_FLAG_VFE       0x00002000L	
#define CODEC_FLAG_ENCODER   0x00004000L	
#define CODEC_FLAG_DECODER   0x00008000L	
#define CODEC_FLAG_NEEDIRQ   0x00010000L	
#define CODEC_FLAG_RDWRPIC   0x00020000L	

#define CODEC_MODE_BJPG      0x0001	
#define CODEC_MODE_LJPG      0x0002	
#define CODEC_MODE_MPEG1     0x0003	
#define CODEC_MODE_MPEG2     0x0004	
#define CODEC_MODE_MPEG4     0x0005	
#define CODEC_MODE_MSDIVX    0x0006	
#define CODEC_MODE_ODIVX     0x0007	
#define CODEC_MODE_WAVELET   0x0008	

#define CODEC_TYPE_NONE    0
#define CODEC_TYPE_L64702  1
#define CODEC_TYPE_ZR36050 2
#define CODEC_TYPE_ZR36016 3
#define CODEC_TYPE_ZR36060 4

#define CODEC_G_STATUS         0x0000	
#define CODEC_S_CODEC_MODE     0x0001	
#define CODEC_G_CODEC_MODE     0x8001
#define CODEC_S_VFE            0x0002	
#define CODEC_G_VFE            0x8002
#define CODEC_S_MMAP           0x0003	

#define CODEC_S_JPEG_TDS_BYTE  0x0010	
#define CODEC_G_JPEG_TDS_BYTE  0x8010
#define CODEC_S_JPEG_SCALE     0x0011	
#define CODEC_G_JPEG_SCALE     0x8011
#define CODEC_S_JPEG_HDT_DATA  0x0018	
#define CODEC_G_JPEG_HDT_DATA  0x8018
#define CODEC_S_JPEG_QDT_DATA  0x0019	
#define CODEC_G_JPEG_QDT_DATA  0x8019
#define CODEC_S_JPEG_APP_DATA  0x001A	
#define CODEC_G_JPEG_APP_DATA  0x801A
#define CODEC_S_JPEG_COM_DATA  0x001B	
#define CODEC_G_JPEG_COM_DATA  0x801B

#define CODEC_S_PRIVATE        0x1000	
#define CODEC_G_PRIVATE        0x9000

#define CODEC_G_FLAG           0x8000	

#define CODEC_TRANSFER_KERNEL 0	
#define CODEC_TRANSFER_USER   1	



struct vfe_polarity {
	unsigned int vsync_pol:1;
	unsigned int hsync_pol:1;
	unsigned int field_pol:1;
	unsigned int blank_pol:1;
	unsigned int subimg_pol:1;
	unsigned int poe_pol:1;
	unsigned int pvalid_pol:1;
	unsigned int vclk_pol:1;
};

struct vfe_settings {
	__u32 x, y;		
	__u32 width, height;	
	__u16 decimation;	
	__u16 flags;		
	__u16 quality;		
};

struct tvnorm {
	u16 Wt, Wa, HStart, HSyncStart, Ht, Ha, VStart;
};

struct jpeg_com_marker {
	int len; 
	char data[60];
};

struct jpeg_app_marker {
	int appn; 
	int len; 
	char data[60];
};

struct videocodec {
	struct module *owner;
	
	char name[32];
	unsigned long magic;	
	unsigned long flags;	
	unsigned int type;	

	

	struct videocodec_master *master_data;

	

	void *data;		

	
	int (*setup) (struct videocodec * codec);
	int (*unset) (struct videocodec * codec);

	
	
	int (*set_mode) (struct videocodec * codec,
			 int mode);
	
	int (*set_video) (struct videocodec * codec,
			  struct tvnorm * norm,
			  struct vfe_settings * cap,
			  struct vfe_polarity * pol);
	
	int (*control) (struct videocodec * codec,
			int type,
			int size,
			void *data);

	
	
	int (*setup_interrupt) (struct videocodec * codec,
				long mode);
	int (*handle_interrupt) (struct videocodec * codec,
				 int source,
				 long flag);
	
	long (*put_image) (struct videocodec * codec,
			   int tr_type,
			   int block,
			   long *fr_num,
			   long *flag,
			   long size,
			   void *buf);
	long (*get_image) (struct videocodec * codec,
			   int tr_type,
			   int block,
			   long *fr_num,
			   long *flag,
			   long size,
			   void *buf);
};

struct videocodec_master {
	
	char name[32];
	unsigned long magic;	
	unsigned long flags;	
	unsigned int type;	

	void *data;		

	 __u32(*readreg) (struct videocodec * codec,
			  __u16 reg);
	void (*writereg) (struct videocodec * codec,
			  __u16 reg,
			  __u32 value);
};



extern struct videocodec *videocodec_attach(struct videocodec_master *);
extern int videocodec_detach(struct videocodec *);

extern int videocodec_register(const struct videocodec *);
extern int videocodec_unregister(const struct videocodec *);


#endif				
