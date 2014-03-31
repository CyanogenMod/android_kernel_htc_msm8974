/*
 * Zoran zr36057/zr36067 PCI controller driver, for the
 * Pinnacle/Miro DC10/DC10+/DC30/DC30+, Iomega Buz, Linux
 * Media Labs LML33/LML33R10.
 *
 * This part handles device access (PCI/I2C/codec/...)
 *
 * Copyright (C) 2000 Serguei Miridonov <mirsev@cicese.mx>
 *
 * Currently maintained by:
 *   Ronald Bultje    <rbultje@ronald.bitfreak.net>
 *   Laurent Pinchart <laurent.pinchart@skynet.be>
 *   Mailinglist      <mjpeg-users@lists.sf.net>
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
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <linux/spinlock.h>
#include <linux/sem.h>

#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/wait.h>

#include <asm/byteorder.h>
#include <asm/io.h>

#include "videocodec.h"
#include "zoran.h"
#include "zoran_device.h"
#include "zoran_card.h"

#define IRQ_MASK ( ZR36057_ISR_GIRQ0 | \
		   ZR36057_ISR_GIRQ1 | \
		   ZR36057_ISR_JPEGRepIRQ )

static bool lml33dpath;		

module_param(lml33dpath, bool, 0644);
MODULE_PARM_DESC(lml33dpath,
		 "Use digital path capture mode (on LML33 cards)");

static void
zr36057_init_vfe (struct zoran *zr);



void
GPIO (struct zoran *zr,
      int           bit,
      unsigned int  value)
{
	u32 reg;
	u32 mask;

	mask = (1 << (24 + bit)) & 0xff000000;
	reg = btread(ZR36057_GPPGCR1) & ~mask;
	if (value) {
		reg |= mask;
	}
	btwrite(reg, ZR36057_GPPGCR1);
	udelay(1);
}


int
post_office_wait (struct zoran *zr)
{
	u32 por;

	while ((por = btread(ZR36057_POR)) & ZR36057_POR_POPen) {
		
	}
	if ((por & ZR36057_POR_POTime) && !zr->card.gws_not_connected) {
		
		dprintk(1, KERN_INFO "%s: pop timeout %08x\n", ZR_DEVNAME(zr),
			por);
		return -1;
	}

	return 0;
}

int
post_office_write (struct zoran *zr,
		   unsigned int  guest,
		   unsigned int  reg,
		   unsigned int  value)
{
	u32 por;

	por =
	    ZR36057_POR_PODir | ZR36057_POR_POTime | ((guest & 7) << 20) |
	    ((reg & 7) << 16) | (value & 0xFF);
	btwrite(por, ZR36057_POR);

	return post_office_wait(zr);
}

int
post_office_read (struct zoran *zr,
		  unsigned int  guest,
		  unsigned int  reg)
{
	u32 por;

	por = ZR36057_POR_POTime | ((guest & 7) << 20) | ((reg & 7) << 16);
	btwrite(por, ZR36057_POR);
	if (post_office_wait(zr) < 0) {
		return -1;
	}

	return btread(ZR36057_POR) & 0xFF;
}


static void
dump_guests (struct zoran *zr)
{
	if (zr36067_debug > 2) {
		int i, guest[8];

		for (i = 1; i < 8; i++) {	
			guest[i] = post_office_read(zr, i, 0);
		}

		printk(KERN_INFO "%s: Guests:", ZR_DEVNAME(zr));

		for (i = 1; i < 8; i++) {
			printk(" 0x%02x", guest[i]);
		}
		printk("\n");
	}
}

static inline unsigned long
get_time (void)
{
	struct timeval tv;

	do_gettimeofday(&tv);
	return (1000000 * tv.tv_sec + tv.tv_usec);
}

void
detect_guest_activity (struct zoran *zr)
{
	int timeout, i, j, res, guest[8], guest0[8], change[8][3];
	unsigned long t0, t1;

	dump_guests(zr);
	printk(KERN_INFO "%s: Detecting guests activity, please wait...\n",
	       ZR_DEVNAME(zr));
	for (i = 1; i < 8; i++) {	
		guest0[i] = guest[i] = post_office_read(zr, i, 0);
	}

	timeout = 0;
	j = 0;
	t0 = get_time();
	while (timeout < 10000) {
		udelay(10);
		timeout++;
		for (i = 1; (i < 8) && (j < 8); i++) {
			res = post_office_read(zr, i, 0);
			if (res != guest[i]) {
				t1 = get_time();
				change[j][0] = (t1 - t0);
				t0 = t1;
				change[j][1] = i;
				change[j][2] = res;
				j++;
				guest[i] = res;
			}
		}
		if (j >= 8)
			break;
	}
	printk(KERN_INFO "%s: Guests:", ZR_DEVNAME(zr));

	for (i = 1; i < 8; i++) {
		printk(" 0x%02x", guest0[i]);
	}
	printk("\n");
	if (j == 0) {
		printk(KERN_INFO "%s: No activity detected.\n", ZR_DEVNAME(zr));
		return;
	}
	for (i = 0; i < j; i++) {
		printk(KERN_INFO "%s: %6d: %d => 0x%02x\n", ZR_DEVNAME(zr),
		       change[i][0], change[i][1], change[i][2]);
	}
}


void
jpeg_codec_sleep (struct zoran *zr,
		  int           sleep)
{
	GPIO(zr, zr->card.gpio[ZR_GPIO_JPEG_SLEEP], !sleep);
	if (!sleep) {
		dprintk(3,
			KERN_DEBUG
			"%s: jpeg_codec_sleep() - wake GPIO=0x%08x\n",
			ZR_DEVNAME(zr), btread(ZR36057_GPPGCR1));
		udelay(500);
	} else {
		dprintk(3,
			KERN_DEBUG
			"%s: jpeg_codec_sleep() - sleep GPIO=0x%08x\n",
			ZR_DEVNAME(zr), btread(ZR36057_GPPGCR1));
		udelay(2);
	}
}

int
jpeg_codec_reset (struct zoran *zr)
{
	
	jpeg_codec_sleep(zr, 0);

	if (zr->card.gpcs[GPCS_JPEG_RESET] != 0xff) {
		post_office_write(zr, zr->card.gpcs[GPCS_JPEG_RESET], 0,
				  0);
		udelay(2);
	} else {
		GPIO(zr, zr->card.gpio[ZR_GPIO_JPEG_RESET], 0);
		udelay(2);
		GPIO(zr, zr->card.gpio[ZR_GPIO_JPEG_RESET], 1);
		udelay(2);
	}

	return 0;
}


static void
zr36057_adjust_vfe (struct zoran          *zr,
		    enum zoran_codec_mode  mode)
{
	u32 reg;

	switch (mode) {
	case BUZ_MODE_MOTION_DECOMPRESS:
		btand(~ZR36057_VFESPFR_ExtFl, ZR36057_VFESPFR);
		reg = btread(ZR36057_VFEHCR);
		if ((reg & (1 << 10)) && zr->card.type != LML33R10) {
			reg += ((1 << 10) | 1);
		}
		btwrite(reg, ZR36057_VFEHCR);
		break;
	case BUZ_MODE_MOTION_COMPRESS:
	case BUZ_MODE_IDLE:
	default:
		if ((zr->norm & V4L2_STD_NTSC) ||
		    (zr->card.type == LML33R10 &&
		     (zr->norm & V4L2_STD_PAL)))
			btand(~ZR36057_VFESPFR_ExtFl, ZR36057_VFESPFR);
		else
			btor(ZR36057_VFESPFR_ExtFl, ZR36057_VFESPFR);
		reg = btread(ZR36057_VFEHCR);
		if (!(reg & (1 << 10)) && zr->card.type != LML33R10) {
			reg -= ((1 << 10) | 1);
		}
		btwrite(reg, ZR36057_VFEHCR);
		break;
	}
}


static void
zr36057_set_vfe (struct zoran              *zr,
		 int                        video_width,
		 int                        video_height,
		 const struct zoran_format *format)
{
	struct tvnorm *tvn;
	unsigned HStart, HEnd, VStart, VEnd;
	unsigned DispMode;
	unsigned VidWinWid, VidWinHt;
	unsigned hcrop1, hcrop2, vcrop1, vcrop2;
	unsigned Wa, We, Ha, He;
	unsigned X, Y, HorDcm, VerDcm;
	u32 reg;
	unsigned mask_line_size;

	tvn = zr->timing;

	Wa = tvn->Wa;
	Ha = tvn->Ha;

	dprintk(2, KERN_INFO "%s: set_vfe() - width = %d, height = %d\n",
		ZR_DEVNAME(zr), video_width, video_height);

	if (video_width < BUZ_MIN_WIDTH ||
	    video_height < BUZ_MIN_HEIGHT ||
	    video_width > Wa || video_height > Ha) {
		dprintk(1, KERN_ERR "%s: set_vfe: w=%d h=%d not valid\n",
			ZR_DEVNAME(zr), video_width, video_height);
		return;
	}

	

	
	VidWinWid = video_width;
	X = DIV_ROUND_UP(VidWinWid * 64, tvn->Wa);
	We = (VidWinWid * 64) / X;
	HorDcm = 64 - X;
	hcrop1 = 2 * ((tvn->Wa - We) / 4);
	hcrop2 = tvn->Wa - We - hcrop1;
	HStart = tvn->HStart ? tvn->HStart : 1;
	HEnd = HStart + tvn->Wa - 1;
	HStart += hcrop1;
	HEnd -= hcrop2;
	reg = ((HStart & ZR36057_VFEHCR_Hmask) << ZR36057_VFEHCR_HStart)
	    | ((HEnd & ZR36057_VFEHCR_Hmask) << ZR36057_VFEHCR_HEnd);
	if (zr->card.vfe_pol.hsync_pol)
		reg |= ZR36057_VFEHCR_HSPol;
	btwrite(reg, ZR36057_VFEHCR);

	
	DispMode = !(video_height > BUZ_MAX_HEIGHT / 2);
	VidWinHt = DispMode ? video_height : video_height / 2;
	Y = DIV_ROUND_UP(VidWinHt * 64 * 2, tvn->Ha);
	He = (VidWinHt * 64) / Y;
	VerDcm = 64 - Y;
	vcrop1 = (tvn->Ha / 2 - He) / 2;
	vcrop2 = tvn->Ha / 2 - He - vcrop1;
	VStart = tvn->VStart;
	VEnd = VStart + tvn->Ha / 2;	
	VStart += vcrop1;
	VEnd -= vcrop2;
	reg = ((VStart & ZR36057_VFEVCR_Vmask) << ZR36057_VFEVCR_VStart)
	    | ((VEnd & ZR36057_VFEVCR_Vmask) << ZR36057_VFEVCR_VEnd);
	if (zr->card.vfe_pol.vsync_pol)
		reg |= ZR36057_VFEVCR_VSPol;
	btwrite(reg, ZR36057_VFEVCR);

	
	reg = 0;
	reg |= (HorDcm << ZR36057_VFESPFR_HorDcm);
	reg |= (VerDcm << ZR36057_VFESPFR_VerDcm);
	reg |= (DispMode << ZR36057_VFESPFR_DispMode);
	
	
	if (!(zr->norm & V4L2_STD_NTSC))
		reg |= ZR36057_VFESPFR_ExtFl;	
	reg |= ZR36057_VFESPFR_TopField;
	if (HorDcm >= 48) {
		reg |= 3 << ZR36057_VFESPFR_HFilter;	
	} else if (HorDcm >= 32) {
		reg |= 2 << ZR36057_VFESPFR_HFilter;	
	} else if (HorDcm >= 16) {
		reg |= 1 << ZR36057_VFESPFR_HFilter;	
	}
	reg |= format->vfespfr;
	btwrite(reg, ZR36057_VFESPFR);

	
	reg = (16 << ZR36057_VDCR_MinPix)
	    | (VidWinHt << ZR36057_VDCR_VidWinHt)
	    | (VidWinWid << ZR36057_VDCR_VidWinWid);
	if (pci_pci_problems & PCIPCI_TRITON)
		
		reg &= ~ZR36057_VDCR_Triton;
	else
		reg |= ZR36057_VDCR_Triton;
	btwrite(reg, ZR36057_VDCR);

	
	if (zr->overlay_mask) {
		

		mask_line_size = (BUZ_MAX_WIDTH + 31) / 32;
		reg = virt_to_bus(zr->overlay_mask);
		btwrite(reg, ZR36057_MMTR);
		reg = virt_to_bus(zr->overlay_mask + mask_line_size);
		btwrite(reg, ZR36057_MMBR);
		reg =
		    mask_line_size - (zr->overlay_settings.width +
				      31) / 32;
		if (DispMode == 0)
			reg += mask_line_size;
		reg <<= ZR36057_OCR_MaskStride;
		btwrite(reg, ZR36057_OCR);
	}

	zr36057_adjust_vfe(zr, zr->codec_mode);
}


void
zr36057_overlay (struct zoran *zr,
		 int           on)
{
	u32 reg;

	if (on) {
		
		btand(~ZR36057_VDCR_VidEn, ZR36057_VDCR);	

		zr36057_set_vfe(zr,
				zr->overlay_settings.width,
				zr->overlay_settings.height,
				zr->overlay_settings.format);


		
		reg = (long) zr->vbuf_base +
		    zr->overlay_settings.x *
		    ((zr->overlay_settings.format->depth + 7) / 8) +
		    zr->overlay_settings.y *
		    zr->vbuf_bytesperline;
		btwrite(reg, ZR36057_VDTR);
		if (reg & 3)
			dprintk(1,
				KERN_ERR
				"%s: zr36057_overlay() - video_address not aligned\n",
				ZR_DEVNAME(zr));
		if (zr->overlay_settings.height > BUZ_MAX_HEIGHT / 2)
			reg += zr->vbuf_bytesperline;
		btwrite(reg, ZR36057_VDBR);

		
		reg = zr->vbuf_bytesperline -
		    zr->overlay_settings.width *
		    ((zr->overlay_settings.format->depth + 7) / 8);
		if (zr->overlay_settings.height > BUZ_MAX_HEIGHT / 2)
			reg += zr->vbuf_bytesperline;
		if (reg & 3)
			dprintk(1,
				KERN_ERR
				"%s: zr36057_overlay() - video_stride not aligned\n",
				ZR_DEVNAME(zr));
		reg = (reg << ZR36057_VSSFGR_DispStride);
		reg |= ZR36057_VSSFGR_VidOvf;	
		btwrite(reg, ZR36057_VSSFGR);

		
		if (zr->overlay_settings.clipcount > 0)
			btor(ZR36057_OCR_OvlEnable, ZR36057_OCR);

		
		btor(ZR36057_VDCR_VidEn, ZR36057_VDCR);
	} else {
		
		btand(~ZR36057_VDCR_VidEn, ZR36057_VDCR);
	}
}


void write_overlay_mask(struct zoran_fh *fh, struct v4l2_clip *vp, int count)
{
	struct zoran *zr = fh->zr;
	unsigned mask_line_size = (BUZ_MAX_WIDTH + 31) / 32;
	u32 *mask;
	int x, y, width, height;
	unsigned i, j, k;
	u32 reg;

	
	memset(fh->overlay_mask, ~0, mask_line_size * 4 * BUZ_MAX_HEIGHT);
	reg = 0;

	for (i = 0; i < count; ++i) {
		
		x = vp[i].c.left;
		y = vp[i].c.top;
		width = vp[i].c.width;
		height = vp[i].c.height;

		
		if (x < 0) {
			width += x;
			x = 0;
		}
		if (y < 0) {
			height += y;
			y = 0;
		}
		if (x + width > fh->overlay_settings.width) {
			width = fh->overlay_settings.width - x;
		}
		if (y + height > fh->overlay_settings.height) {
			height = fh->overlay_settings.height - y;
		}

		
		if (height <= 0) {
			continue;
		}
		if (width <= 0) {
			continue;
		}

		
		for (j = 0; j < height; ++j) {
			
			
			mask = fh->overlay_mask + (y + j) * mask_line_size;
			for (k = 0; k < width; ++k) {
				mask[(x + k) / 32] &=
				    ~((u32) 1 << (x + k) % 32);
			}
		}
	}
}


void
zr36057_set_memgrab (struct zoran *zr,
		     int           mode)
{
	if (mode) {
		if (btread(ZR36057_VSSFGR) & ZR36057_VSSFGR_SnapShot)
			dprintk(1,
				KERN_WARNING
				"%s: zr36057_set_memgrab(1) with SnapShot on!?\n",
				ZR_DEVNAME(zr));

		
		btwrite(IRQ_MASK, ZR36057_ISR);	
		btor(zr->card.vsync_int, ZR36057_ICR);	

		
		btor(ZR36057_VSSFGR_SnapShot, ZR36057_VSSFGR);

		
		zr36057_set_vfe(zr, zr->v4l_settings.width,
				zr->v4l_settings.height,
				zr->v4l_settings.format);

		zr->v4l_memgrab_active = 1;
	} else {
		
		btand(~zr->card.vsync_int, ZR36057_ICR);	

		zr->v4l_memgrab_active = 0;
		zr->v4l_grab_frame = NO_GRAB_ACTIVE;

		
		if (zr->v4l_overlay_active) {
			zr36057_overlay(zr, 1);
		} else {
			btand(~ZR36057_VDCR_VidEn, ZR36057_VDCR);
			btand(~ZR36057_VSSFGR_SnapShot, ZR36057_VSSFGR);
		}
	}
}

int
wait_grab_pending (struct zoran *zr)
{
	unsigned long flags;

	

	if (!zr->v4l_memgrab_active)
		return 0;

	wait_event_interruptible(zr->v4l_capq,
			(zr->v4l_pend_tail == zr->v4l_pend_head));
	if (signal_pending(current))
		return -ERESTARTSYS;

	spin_lock_irqsave(&zr->spinlock, flags);
	zr36057_set_memgrab(zr, 0);
	spin_unlock_irqrestore(&zr->spinlock, flags);

	return 0;
}


static inline void
set_frame (struct zoran *zr,
	   int           val)
{
	GPIO(zr, zr->card.gpio[ZR_GPIO_JPEG_FRAME], val);
}

static void
set_videobus_dir (struct zoran *zr,
		  int           val)
{
	switch (zr->card.type) {
	case LML33:
	case LML33R10:
		if (lml33dpath == 0)
			GPIO(zr, 5, val);
		else
			GPIO(zr, 5, 1);
		break;
	default:
		GPIO(zr, zr->card.gpio[ZR_GPIO_VID_DIR],
		     zr->card.gpio_pol[ZR_GPIO_VID_DIR] ? !val : val);
		break;
	}
}

static void
init_jpeg_queue (struct zoran *zr)
{
	int i;

	
	zr->jpg_que_head = 0;
	zr->jpg_dma_head = 0;
	zr->jpg_dma_tail = 0;
	zr->jpg_que_tail = 0;
	zr->jpg_seq_num = 0;
	zr->JPEG_error = 0;
	zr->num_errors = 0;
	zr->jpg_err_seq = 0;
	zr->jpg_err_shift = 0;
	zr->jpg_queued_num = 0;
	for (i = 0; i < zr->jpg_buffers.num_buffers; i++) {
		zr->jpg_buffers.buffer[i].state = BUZ_STATE_USER;	
	}
	for (i = 0; i < BUZ_NUM_STAT_COM; i++) {
		zr->stat_com[i] = cpu_to_le32(1);	
	}
}

static void
zr36057_set_jpg (struct zoran          *zr,
		 enum zoran_codec_mode  mode)
{
	struct tvnorm *tvn;
	u32 reg;

	tvn = zr->timing;

	
	btwrite(0, ZR36057_JPC);

	
	switch (mode) {

	case BUZ_MODE_MOTION_COMPRESS:
	default:
		reg = ZR36057_JMC_MJPGCmpMode;
		break;

	case BUZ_MODE_MOTION_DECOMPRESS:
		reg = ZR36057_JMC_MJPGExpMode;
		reg |= ZR36057_JMC_SyncMstr;
		
		
		break;

	case BUZ_MODE_STILL_COMPRESS:
		reg = ZR36057_JMC_JPGCmpMode;
		break;

	case BUZ_MODE_STILL_DECOMPRESS:
		reg = ZR36057_JMC_JPGExpMode;
		break;

	}
	reg |= ZR36057_JMC_JPG;
	if (zr->jpg_settings.field_per_buff == 1)
		reg |= ZR36057_JMC_Fld_per_buff;
	btwrite(reg, ZR36057_JMC);

	
	btor(ZR36057_VFEVCR_VSPol, ZR36057_VFEVCR);
	reg = (6 << ZR36057_VSP_VsyncSize) |
	      (tvn->Ht << ZR36057_VSP_FrmTot);
	btwrite(reg, ZR36057_VSP);
	reg = ((zr->jpg_settings.img_y + tvn->VStart) << ZR36057_FVAP_NAY) |
	      (zr->jpg_settings.img_height << ZR36057_FVAP_PAY);
	btwrite(reg, ZR36057_FVAP);

	
	if (zr->card.vfe_pol.hsync_pol)
		btor(ZR36057_VFEHCR_HSPol, ZR36057_VFEHCR);
	else
		btand(~ZR36057_VFEHCR_HSPol, ZR36057_VFEHCR);
	reg = ((tvn->HSyncStart) << ZR36057_HSP_HsyncStart) |
	      (tvn->Wt << ZR36057_HSP_LineTot);
	btwrite(reg, ZR36057_HSP);
	reg = ((zr->jpg_settings.img_x +
		tvn->HStart + 4) << ZR36057_FHAP_NAX) |
	      (zr->jpg_settings.img_width << ZR36057_FHAP_PAX);
	btwrite(reg, ZR36057_FHAP);

	
	if (zr->jpg_settings.odd_even)
		reg = ZR36057_FPP_Odd_Even;
	else
		reg = 0;

	btwrite(reg, ZR36057_FPP);

	
	

	
	reg = virt_to_bus(zr->stat_com);
	btwrite(reg, ZR36057_JCBA);

	
	
	switch (mode) {

	case BUZ_MODE_STILL_COMPRESS:
	case BUZ_MODE_MOTION_COMPRESS:
		if (zr->card.type != BUZ)
			reg = 140;
		else
			reg = 60;
		break;

	case BUZ_MODE_STILL_DECOMPRESS:
	case BUZ_MODE_MOTION_DECOMPRESS:
		reg = 20;
		break;

	default:
		reg = 80;
		break;

	}
	btwrite(reg, ZR36057_JCFT);
	zr36057_adjust_vfe(zr, mode);

}

void
print_interrupts (struct zoran *zr)
{
	int res, noerr = 0;

	printk(KERN_INFO "%s: interrupts received:", ZR_DEVNAME(zr));
	if ((res = zr->field_counter) < -1 || res > 1) {
		printk(" FD:%d", res);
	}
	if ((res = zr->intr_counter_GIRQ1) != 0) {
		printk(" GIRQ1:%d", res);
		noerr++;
	}
	if ((res = zr->intr_counter_GIRQ0) != 0) {
		printk(" GIRQ0:%d", res);
		noerr++;
	}
	if ((res = zr->intr_counter_CodRepIRQ) != 0) {
		printk(" CodRepIRQ:%d", res);
		noerr++;
	}
	if ((res = zr->intr_counter_JPEGRepIRQ) != 0) {
		printk(" JPEGRepIRQ:%d", res);
		noerr++;
	}
	if (zr->JPEG_max_missed) {
		printk(" JPEG delays: max=%d min=%d", zr->JPEG_max_missed,
		       zr->JPEG_min_missed);
	}
	if (zr->END_event_missed) {
		printk(" ENDs missed: %d", zr->END_event_missed);
	}
	
	printk(" queue_state=%ld/%ld/%ld/%ld", zr->jpg_que_tail,
	       zr->jpg_dma_tail, zr->jpg_dma_head, zr->jpg_que_head);
	
	if (!noerr) {
		printk(": no interrupts detected.");
	}
	printk("\n");
}

void
clear_interrupt_counters (struct zoran *zr)
{
	zr->intr_counter_GIRQ1 = 0;
	zr->intr_counter_GIRQ0 = 0;
	zr->intr_counter_CodRepIRQ = 0;
	zr->intr_counter_JPEGRepIRQ = 0;
	zr->field_counter = 0;
	zr->IRQ1_in = 0;
	zr->IRQ1_out = 0;
	zr->JPEG_in = 0;
	zr->JPEG_out = 0;
	zr->JPEG_0 = 0;
	zr->JPEG_1 = 0;
	zr->END_event_missed = 0;
	zr->JPEG_missed = 0;
	zr->JPEG_max_missed = 0;
	zr->JPEG_min_missed = 0x7fffffff;
}

static u32
count_reset_interrupt (struct zoran *zr)
{
	u32 isr;

	if ((isr = btread(ZR36057_ISR) & 0x78000000)) {
		if (isr & ZR36057_ISR_GIRQ1) {
			btwrite(ZR36057_ISR_GIRQ1, ZR36057_ISR);
			zr->intr_counter_GIRQ1++;
		}
		if (isr & ZR36057_ISR_GIRQ0) {
			btwrite(ZR36057_ISR_GIRQ0, ZR36057_ISR);
			zr->intr_counter_GIRQ0++;
		}
		if (isr & ZR36057_ISR_CodRepIRQ) {
			btwrite(ZR36057_ISR_CodRepIRQ, ZR36057_ISR);
			zr->intr_counter_CodRepIRQ++;
		}
		if (isr & ZR36057_ISR_JPEGRepIRQ) {
			btwrite(ZR36057_ISR_JPEGRepIRQ, ZR36057_ISR);
			zr->intr_counter_JPEGRepIRQ++;
		}
	}
	return isr;
}

void
jpeg_start (struct zoran *zr)
{
	int reg;

	zr->frame_num = 0;

	
	btwrite(ZR36057_JPC_P_Reset, ZR36057_JPC);
	
	btand(~ZR36057_MCTCR_CFlush, ZR36057_MCTCR);
	
	btor(ZR36057_JPC_CodTrnsEn, ZR36057_JPC);

	
	btwrite(IRQ_MASK, ZR36057_ISR);
	
	btwrite(zr->card.jpeg_int |
			ZR36057_ICR_JPEGRepIRQ |
			ZR36057_ICR_IntPinEn,
		ZR36057_ICR);

	set_frame(zr, 0);	

	
	reg = (zr->card.gpcs[1] << ZR36057_JCGI_JPEGuestID) |
	       (0 << ZR36057_JCGI_JPEGuestReg);
	btwrite(reg, ZR36057_JCGI);

	if (zr->card.video_vfe == CODEC_TYPE_ZR36016 &&
	    zr->card.video_codec == CODEC_TYPE_ZR36050) {
		
		if (zr->vfe)
			zr36016_write(zr->vfe, 0, 1);

		
		post_office_write(zr, 0, 0, 0);
	}

	
	btor(ZR36057_JPC_Active, ZR36057_JPC);

	
	btor(ZR36057_JMC_Go_en, ZR36057_JMC);
	udelay(30);

	set_frame(zr, 1);	

	dprintk(3, KERN_DEBUG "%s: jpeg_start\n", ZR_DEVNAME(zr));
}

void
zr36057_enable_jpg (struct zoran          *zr,
		    enum zoran_codec_mode  mode)
{
	struct vfe_settings cap;
	int field_size =
	    zr->jpg_buffers.buffer_size / zr->jpg_settings.field_per_buff;

	zr->codec_mode = mode;

	cap.x = zr->jpg_settings.img_x;
	cap.y = zr->jpg_settings.img_y;
	cap.width = zr->jpg_settings.img_width;
	cap.height = zr->jpg_settings.img_height;
	cap.decimation =
	    zr->jpg_settings.HorDcm | (zr->jpg_settings.VerDcm << 8);
	cap.quality = zr->jpg_settings.jpg_comp.quality;

	switch (mode) {

	case BUZ_MODE_MOTION_COMPRESS: {
		struct jpeg_app_marker app;
		struct jpeg_com_marker com;

		set_videobus_dir(zr, 0);
		decoder_call(zr, video, s_stream, 1);
		encoder_call(zr, video, s_routing, 0, 0, 0);

		
		jpeg_codec_sleep(zr, 0);

		
		app.appn = zr->jpg_settings.jpg_comp.APPn;
		app.len = zr->jpg_settings.jpg_comp.APP_len;
		memcpy(app.data, zr->jpg_settings.jpg_comp.APP_data, 60);
		zr->codec->control(zr->codec, CODEC_S_JPEG_APP_DATA,
				   sizeof(struct jpeg_app_marker), &app);

		com.len = zr->jpg_settings.jpg_comp.COM_len;
		memcpy(com.data, zr->jpg_settings.jpg_comp.COM_data, 60);
		zr->codec->control(zr->codec, CODEC_S_JPEG_COM_DATA,
				   sizeof(struct jpeg_com_marker), &com);

		
		zr->codec->control(zr->codec, CODEC_S_JPEG_TDS_BYTE,
				   sizeof(int), &field_size);
		zr->codec->set_video(zr->codec, zr->timing, &cap,
				     &zr->card.vfe_pol);
		zr->codec->set_mode(zr->codec, CODEC_DO_COMPRESSION);

		
		if (zr->vfe) {
			zr->vfe->control(zr->vfe, CODEC_S_JPEG_TDS_BYTE,
					 sizeof(int), &field_size);
			zr->vfe->set_video(zr->vfe, zr->timing, &cap,
					   &zr->card.vfe_pol);
			zr->vfe->set_mode(zr->vfe, CODEC_DO_COMPRESSION);
		}

		init_jpeg_queue(zr);
		zr36057_set_jpg(zr, mode);	

		clear_interrupt_counters(zr);
		dprintk(2, KERN_INFO "%s: enable_jpg(MOTION_COMPRESS)\n",
			ZR_DEVNAME(zr));
		break;
	}

	case BUZ_MODE_MOTION_DECOMPRESS:
		decoder_call(zr, video, s_stream, 0);
		set_videobus_dir(zr, 1);
		encoder_call(zr, video, s_routing, 1, 0, 0);

		
		jpeg_codec_sleep(zr, 0);
		
		if (zr->vfe) {
			zr->vfe->set_video(zr->vfe, zr->timing, &cap,
					   &zr->card.vfe_pol);
			zr->vfe->set_mode(zr->vfe, CODEC_DO_EXPANSION);
		}
		
		zr->codec->set_video(zr->codec, zr->timing, &cap,
				     &zr->card.vfe_pol);
		zr->codec->set_mode(zr->codec, CODEC_DO_EXPANSION);

		init_jpeg_queue(zr);
		zr36057_set_jpg(zr, mode);	

		clear_interrupt_counters(zr);
		dprintk(2, KERN_INFO "%s: enable_jpg(MOTION_DECOMPRESS)\n",
			ZR_DEVNAME(zr));
		break;

	case BUZ_MODE_IDLE:
	default:
		
		btand(~(zr->card.jpeg_int | ZR36057_ICR_JPEGRepIRQ),
		      ZR36057_ICR);
		btwrite(zr->card.jpeg_int | ZR36057_ICR_JPEGRepIRQ,
			ZR36057_ISR);
		btand(~ZR36057_JMC_Go_en, ZR36057_JMC);	

		msleep(50);

		set_videobus_dir(zr, 0);
		set_frame(zr, 1);	
		btor(ZR36057_MCTCR_CFlush, ZR36057_MCTCR);	
		btwrite(0, ZR36057_JPC);	
		btand(~ZR36057_JMC_VFIFO_FB, ZR36057_JMC);
		btand(~ZR36057_JMC_SyncMstr, ZR36057_JMC);
		jpeg_codec_reset(zr);
		jpeg_codec_sleep(zr, 1);
		zr36057_adjust_vfe(zr, mode);

		decoder_call(zr, video, s_stream, 1);
		encoder_call(zr, video, s_routing, 0, 0, 0);

		dprintk(2, KERN_INFO "%s: enable_jpg(IDLE)\n", ZR_DEVNAME(zr));
		break;

	}
}

void
zoran_feed_stat_com (struct zoran *zr)
{
	

	int frame, i, max_stat_com;

	max_stat_com =
	    (zr->jpg_settings.TmpDcm ==
	     1) ? BUZ_NUM_STAT_COM : (BUZ_NUM_STAT_COM >> 1);

	while ((zr->jpg_dma_head - zr->jpg_dma_tail) < max_stat_com &&
	       zr->jpg_dma_head < zr->jpg_que_head) {

		frame = zr->jpg_pend[zr->jpg_dma_head & BUZ_MASK_FRAME];
		if (zr->jpg_settings.TmpDcm == 1) {
			
			i = (zr->jpg_dma_head -
			     zr->jpg_err_shift) & BUZ_MASK_STAT_COM;
			if (!(zr->stat_com[i] & cpu_to_le32(1)))
				break;
			zr->stat_com[i] =
			    cpu_to_le32(zr->jpg_buffers.buffer[frame].jpg.frag_tab_bus);
		} else {
			
			i = ((zr->jpg_dma_head -
			      zr->jpg_err_shift) & 1) * 2;
			if (!(zr->stat_com[i] & cpu_to_le32(1)))
				break;
			zr->stat_com[i] =
			    cpu_to_le32(zr->jpg_buffers.buffer[frame].jpg.frag_tab_bus);
			zr->stat_com[i + 1] =
			    cpu_to_le32(zr->jpg_buffers.buffer[frame].jpg.frag_tab_bus);
		}
		zr->jpg_buffers.buffer[frame].state = BUZ_STATE_DMA;
		zr->jpg_dma_head++;

	}
	if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS)
		zr->jpg_queued_num++;
}

static void
zoran_reap_stat_com (struct zoran *zr)
{
	

	int i;
	u32 stat_com;
	unsigned int seq;
	unsigned int dif;
	struct zoran_buffer *buffer;
	int frame;


	if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS) {
		zr->jpg_seq_num++;
	}
	while (zr->jpg_dma_tail < zr->jpg_dma_head) {
		if (zr->jpg_settings.TmpDcm == 1)
			i = (zr->jpg_dma_tail -
			     zr->jpg_err_shift) & BUZ_MASK_STAT_COM;
		else
			i = ((zr->jpg_dma_tail -
			      zr->jpg_err_shift) & 1) * 2 + 1;

		stat_com = le32_to_cpu(zr->stat_com[i]);

		if ((stat_com & 1) == 0) {
			return;
		}
		frame = zr->jpg_pend[zr->jpg_dma_tail & BUZ_MASK_FRAME];
		buffer = &zr->jpg_buffers.buffer[frame];
		do_gettimeofday(&buffer->bs.timestamp);

		if (zr->codec_mode == BUZ_MODE_MOTION_COMPRESS) {
			buffer->bs.length = (stat_com & 0x7fffff) >> 1;

			

			seq = ((stat_com >> 24) + zr->jpg_err_seq) & 0xff;
			dif = (seq - zr->jpg_seq_num) & 0xff;
			zr->jpg_seq_num += dif;
		} else {
			buffer->bs.length = 0;
		}
		buffer->bs.seq =
		    zr->jpg_settings.TmpDcm ==
		    2 ? (zr->jpg_seq_num >> 1) : zr->jpg_seq_num;
		buffer->state = BUZ_STATE_DONE;

		zr->jpg_dma_tail++;
	}
}

static void zoran_restart(struct zoran *zr)
{
	
	unsigned int status = 0;
	int mode;

	if (zr->codec_mode == BUZ_MODE_MOTION_COMPRESS) {
		decoder_call(zr, video, g_input_status, &status);
		mode = CODEC_DO_COMPRESSION;
	} else {
		status = V4L2_IN_ST_NO_SIGNAL;
		mode = CODEC_DO_EXPANSION;
	}
	if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS ||
	    !(status & V4L2_IN_ST_NO_SIGNAL)) {
		
		jpeg_codec_reset(zr);
		zr->codec->set_mode(zr->codec, mode);
		zr36057_set_jpg(zr, zr->codec_mode);
		jpeg_start(zr);

		if (zr->num_errors <= 8)
			dprintk(2, KERN_INFO "%s: Restart\n",
				ZR_DEVNAME(zr));

		zr->JPEG_missed = 0;
		zr->JPEG_error = 2;
		
	}
}

static void
error_handler (struct zoran *zr,
	       u32           astat,
	       u32           stat)
{
	int i;

	
	if (zr->codec_mode != BUZ_MODE_MOTION_COMPRESS &&
	    zr->codec_mode != BUZ_MODE_MOTION_DECOMPRESS) {
		return;
	}

	if ((stat & 1) == 0 &&
	    zr->codec_mode == BUZ_MODE_MOTION_COMPRESS &&
	    zr->jpg_dma_tail - zr->jpg_que_tail >= zr->jpg_buffers.num_buffers) {
		
		zoran_reap_stat_com(zr);
		zoran_feed_stat_com(zr);
		wake_up_interruptible(&zr->jpg_capq);
		zr->JPEG_missed = 0;
		return;
	}

	if (zr->JPEG_error == 1) {
		zoran_restart(zr);
		return;
	}

	btand(~ZR36057_JMC_Go_en, ZR36057_JMC);
	udelay(1);
	stat = stat | (post_office_read(zr, 7, 0) & 3) << 8;
	btwrite(0, ZR36057_JPC);
	btor(ZR36057_MCTCR_CFlush, ZR36057_MCTCR);
	jpeg_codec_reset(zr);
	jpeg_codec_sleep(zr, 1);
	zr->JPEG_error = 1;
	zr->num_errors++;

	
	if (zr36067_debug > 1 && zr->num_errors <= 8) {
		long frame;
		int j;

		frame = zr->jpg_pend[zr->jpg_dma_tail & BUZ_MASK_FRAME];
		printk(KERN_ERR
		       "%s: JPEG error stat=0x%08x(0x%08x) queue_state=%ld/%ld/%ld/%ld seq=%ld frame=%ld. Codec stopped. ",
		       ZR_DEVNAME(zr), stat, zr->last_isr,
		       zr->jpg_que_tail, zr->jpg_dma_tail,
		       zr->jpg_dma_head, zr->jpg_que_head,
		       zr->jpg_seq_num, frame);
		printk(KERN_INFO "stat_com frames:");
		for (j = 0; j < BUZ_NUM_STAT_COM; j++) {
			for (i = 0; i < zr->jpg_buffers.num_buffers; i++) {
				if (le32_to_cpu(zr->stat_com[j]) == zr->jpg_buffers.buffer[i].jpg.frag_tab_bus)
					printk(KERN_CONT "% d->%d", j, i);
			}
		}
		printk(KERN_CONT "\n");
	}
	
	if (zr->jpg_settings.TmpDcm == 1)
		i = (zr->jpg_dma_tail - zr->jpg_err_shift) & BUZ_MASK_STAT_COM;
	else
		i = ((zr->jpg_dma_tail - zr->jpg_err_shift) & 1) * 2;
	if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS) {
		
		zr->stat_com[i] |= cpu_to_le32(1);
		if (zr->jpg_settings.TmpDcm != 1)
			zr->stat_com[i + 1] |= cpu_to_le32(1);
		
		zoran_reap_stat_com(zr);
		zoran_feed_stat_com(zr);
		wake_up_interruptible(&zr->jpg_capq);
		
		if (zr->jpg_settings.TmpDcm == 1)
			i = (zr->jpg_dma_tail - zr->jpg_err_shift) & BUZ_MASK_STAT_COM;
		else
			i = ((zr->jpg_dma_tail - zr->jpg_err_shift) & 1) * 2;
	}
	if (i) {
		
		int j;
		__le32 bus_addr[BUZ_NUM_STAT_COM];

		memcpy(bus_addr, zr->stat_com, sizeof(bus_addr));

		for (j = 0; j < BUZ_NUM_STAT_COM; j++)
			zr->stat_com[j] = bus_addr[(i + j) & BUZ_MASK_STAT_COM];

		zr->jpg_err_shift += i;
		zr->jpg_err_shift &= BUZ_MASK_STAT_COM;
	}
	if (zr->codec_mode == BUZ_MODE_MOTION_COMPRESS)
		zr->jpg_err_seq = zr->jpg_seq_num;	
	zoran_restart(zr);
}

irqreturn_t
zoran_irq (int             irq,
	   void           *dev_id)
{
	u32 stat, astat;
	int count;
	struct zoran *zr;
	unsigned long flags;

	zr = dev_id;
	count = 0;

	if (zr->testing) {
		
		spin_lock_irqsave(&zr->spinlock, flags);
		while ((stat = count_reset_interrupt(zr))) {
			if (count++ > 100) {
				btand(~ZR36057_ICR_IntPinEn, ZR36057_ICR);
				dprintk(1,
					KERN_ERR
					"%s: IRQ lockup while testing, isr=0x%08x, cleared int mask\n",
					ZR_DEVNAME(zr), stat);
				wake_up_interruptible(&zr->test_q);
			}
		}
		zr->last_isr = stat;
		spin_unlock_irqrestore(&zr->spinlock, flags);
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&zr->spinlock, flags);
	while (1) {
		
		stat = count_reset_interrupt(zr);
		astat = stat & IRQ_MASK;
		if (!astat) {
			break;
		}
		dprintk(4,
			KERN_DEBUG
			"zoran_irq: astat: 0x%08x, mask: 0x%08x\n",
			astat, btread(ZR36057_ICR));
		if (astat & zr->card.vsync_int) {	

			if (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS ||
			    zr->codec_mode == BUZ_MODE_MOTION_COMPRESS) {
				
				zr->JPEG_missed++;
			}
			

			if (zr->v4l_memgrab_active) {
				
				if ((btread(ZR36057_VSSFGR) & ZR36057_VSSFGR_SnapShot) == 0)
					dprintk(1,
						KERN_WARNING
						"%s: BuzIRQ with SnapShot off ???\n",
						ZR_DEVNAME(zr));

				if (zr->v4l_grab_frame != NO_GRAB_ACTIVE) {
					
					if ((btread(ZR36057_VSSFGR) & ZR36057_VSSFGR_FrameGrab) == 0) {
						

						zr->v4l_buffers.buffer[zr->v4l_grab_frame].state = BUZ_STATE_DONE;
						zr->v4l_buffers.buffer[zr->v4l_grab_frame].bs.seq = zr->v4l_grab_seq;
						do_gettimeofday(&zr->v4l_buffers.buffer[zr->v4l_grab_frame].bs.timestamp);
						zr->v4l_grab_frame = NO_GRAB_ACTIVE;
						zr->v4l_pend_tail++;
					}
				}

				if (zr->v4l_grab_frame == NO_GRAB_ACTIVE)
					wake_up_interruptible(&zr->v4l_capq);

				

				if (zr->v4l_grab_frame == NO_GRAB_ACTIVE &&
				    zr->v4l_pend_tail != zr->v4l_pend_head) {
					int frame = zr->v4l_pend[zr->v4l_pend_tail & V4L_MASK_FRAME];
					u32 reg;

					zr->v4l_grab_frame = frame;

					

					

					reg = zr->v4l_buffers.buffer[frame].v4l.fbuffer_bus;
					btwrite(reg, ZR36057_VDTR);
					if (zr->v4l_settings.height > BUZ_MAX_HEIGHT / 2)
						reg += zr->v4l_settings.bytesperline;
					btwrite(reg, ZR36057_VDBR);

					
					reg = 0;
					if (zr->v4l_settings.height > BUZ_MAX_HEIGHT / 2)
						reg += zr->v4l_settings.bytesperline;
					reg = (reg << ZR36057_VSSFGR_DispStride);
					reg |= ZR36057_VSSFGR_VidOvf;
					reg |= ZR36057_VSSFGR_SnapShot;
					reg |= ZR36057_VSSFGR_FrameGrab;
					btwrite(reg, ZR36057_VSSFGR);

					btor(ZR36057_VDCR_VidEn,
					     ZR36057_VDCR);
				}
			}

			zr->v4l_grab_seq++;
		}
#if (IRQ_MASK & ZR36057_ISR_CodRepIRQ)
		if (astat & ZR36057_ISR_CodRepIRQ) {
			zr->intr_counter_CodRepIRQ++;
			IDEBUG(printk(KERN_DEBUG "%s: ZR36057_ISR_CodRepIRQ\n",
				ZR_DEVNAME(zr)));
			btand(~ZR36057_ICR_CodRepIRQ, ZR36057_ICR);
		}
#endif				

#if (IRQ_MASK & ZR36057_ISR_JPEGRepIRQ)
		if ((astat & ZR36057_ISR_JPEGRepIRQ) &&
		    (zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS ||
		     zr->codec_mode == BUZ_MODE_MOTION_COMPRESS)) {
			if (zr36067_debug > 1 && (!zr->frame_num || zr->JPEG_error)) {
				char sv[BUZ_NUM_STAT_COM + 1];
				int i;

				printk(KERN_INFO
				       "%s: first frame ready: state=0x%08x odd_even=%d field_per_buff=%d delay=%d\n",
				       ZR_DEVNAME(zr), stat,
				       zr->jpg_settings.odd_even,
				       zr->jpg_settings.field_per_buff,
				       zr->JPEG_missed);

				for (i = 0; i < BUZ_NUM_STAT_COM; i++)
					sv[i] = le32_to_cpu(zr->stat_com[i]) & 1 ? '1' : '0';
				sv[BUZ_NUM_STAT_COM] = 0;
				printk(KERN_INFO
				       "%s: stat_com=%s queue_state=%ld/%ld/%ld/%ld\n",
				       ZR_DEVNAME(zr), sv,
				       zr->jpg_que_tail,
				       zr->jpg_dma_tail,
				       zr->jpg_dma_head,
				       zr->jpg_que_head);
			} else {
				
				if (zr->JPEG_missed > zr->JPEG_max_missed)
					zr->JPEG_max_missed = zr->JPEG_missed;
				if (zr->JPEG_missed < zr->JPEG_min_missed)
					zr->JPEG_min_missed = zr->JPEG_missed;
			}

			if (zr36067_debug > 2 && zr->frame_num < 6) {
				int i;

				printk(KERN_INFO "%s: seq=%ld stat_com:",
				       ZR_DEVNAME(zr), zr->jpg_seq_num);
				for (i = 0; i < 4; i++) {
					printk(KERN_CONT " %08x",
					       le32_to_cpu(zr->stat_com[i]));
				}
				printk(KERN_CONT "\n");
			}
			zr->frame_num++;
			zr->JPEG_missed = 0;
			zr->JPEG_error = 0;
			zoran_reap_stat_com(zr);
			zoran_feed_stat_com(zr);
			wake_up_interruptible(&zr->jpg_capq);
		}
#endif				

		
		if ((astat & zr->card.jpeg_int) ||
		    zr->JPEG_missed > 25 ||
		    zr->JPEG_error == 1	||
		    ((zr->codec_mode == BUZ_MODE_MOTION_DECOMPRESS) &&
		     (zr->frame_num && (zr->JPEG_missed > zr->jpg_settings.field_per_buff)))) {
			error_handler(zr, astat, stat);
		}

		count++;
		if (count > 10) {
			dprintk(2, KERN_WARNING "%s: irq loop %d\n",
				ZR_DEVNAME(zr), count);
			if (count > 20) {
				btand(~ZR36057_ICR_IntPinEn, ZR36057_ICR);
				dprintk(2,
					KERN_ERR
					"%s: IRQ lockup, cleared int mask\n",
					ZR_DEVNAME(zr));
				break;
			}
		}
		zr->last_isr = stat;
	}
	spin_unlock_irqrestore(&zr->spinlock, flags);

	return IRQ_HANDLED;
}

void
zoran_set_pci_master (struct zoran *zr,
		      int           set_master)
{
	if (set_master) {
		pci_set_master(zr->pci_dev);
	} else {
		u16 command;

		pci_read_config_word(zr->pci_dev, PCI_COMMAND, &command);
		command &= ~PCI_COMMAND_MASTER;
		pci_write_config_word(zr->pci_dev, PCI_COMMAND, command);
	}
}

void
zoran_init_hardware (struct zoran *zr)
{
	
	zoran_set_pci_master(zr, 1);

	
	if (zr->card.init) {
		zr->card.init(zr);
	}

	decoder_call(zr, core, init, 0);
	decoder_call(zr, core, s_std, zr->norm);
	decoder_call(zr, video, s_routing,
		zr->card.input[zr->input].muxsel, 0, 0);

	encoder_call(zr, core, init, 0);
	encoder_call(zr, video, s_std_output, zr->norm);
	encoder_call(zr, video, s_routing, 0, 0, 0);

	
	jpeg_codec_sleep(zr, 1);
	jpeg_codec_sleep(zr, 0);


	
	
	
	 			
	    zr36057_init_vfe(zr);

	zr36057_enable_jpg(zr, BUZ_MODE_IDLE);

	btwrite(IRQ_MASK, ZR36057_ISR);	
}

void
zr36057_restart (struct zoran *zr)
{
	btwrite(0, ZR36057_SPGPPCR);
	mdelay(1);
	btor(ZR36057_SPGPPCR_SoftReset, ZR36057_SPGPPCR);
	mdelay(1);

	
	btwrite(0, ZR36057_JPC);
	
	btwrite(ZR36057_SPGPPCR_SoftReset | 0, ZR36057_SPGPPCR);

	
	btwrite((0x81 << 24) | 0x8888, ZR36057_GPPGCR1);
}


static void
zr36057_init_vfe (struct zoran *zr)
{
	u32 reg;

	reg = btread(ZR36057_VFESPFR);
	reg |= ZR36057_VFESPFR_LittleEndian;
	reg &= ~ZR36057_VFESPFR_VCLKPol;
	reg |= ZR36057_VFESPFR_ExtFl;
	reg |= ZR36057_VFESPFR_TopField;
	btwrite(reg, ZR36057_VFESPFR);
	reg = btread(ZR36057_VDCR);
	if (pci_pci_problems & PCIPCI_TRITON)
		
		reg &= ~ZR36057_VDCR_Triton;
	else
		reg |= ZR36057_VDCR_Triton;
	btwrite(reg, ZR36057_VDCR);
}
