/* (C) 1999-2003 Nemosoft Unv.
   (C) 2004-2006 Luc Saillard (luc@saillard.org)

   NOTE: this version of pwc is an unofficial (modified) release of pwc & pcwx
   driver and thus may have bugs that are not present in the original version.
   Please send bug reports and support requests to <luc@saillard.org>.
   The decompression routines have been implemented by reverse-engineering the
   Nemosoft binary pwcx module. Caveat emptor.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef PWC_H
#define PWC_H

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/errno.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-vmalloc.h>
#ifdef CONFIG_USB_PWC_INPUT_EVDEV
#include <linux/input.h>
#endif
#include "pwc-dec1.h"
#include "pwc-dec23.h"

#define PWC_VERSION	"10.0.15"
#define PWC_NAME 	"pwc"
#define PFX		PWC_NAME ": "


#define PWC_DEBUG_LEVEL_MODULE	(1<<0)
#define PWC_DEBUG_LEVEL_PROBE	(1<<1)
#define PWC_DEBUG_LEVEL_OPEN	(1<<2)
#define PWC_DEBUG_LEVEL_READ	(1<<3)
#define PWC_DEBUG_LEVEL_MEMORY	(1<<4)
#define PWC_DEBUG_LEVEL_FLOW	(1<<5)
#define PWC_DEBUG_LEVEL_SIZE	(1<<6)
#define PWC_DEBUG_LEVEL_IOCTL	(1<<7)
#define PWC_DEBUG_LEVEL_TRACE	(1<<8)

#define PWC_DEBUG_MODULE(fmt, args...) PWC_DEBUG(MODULE, fmt, ##args)
#define PWC_DEBUG_PROBE(fmt, args...) PWC_DEBUG(PROBE, fmt, ##args)
#define PWC_DEBUG_OPEN(fmt, args...) PWC_DEBUG(OPEN, fmt, ##args)
#define PWC_DEBUG_READ(fmt, args...) PWC_DEBUG(READ, fmt, ##args)
#define PWC_DEBUG_MEMORY(fmt, args...) PWC_DEBUG(MEMORY, fmt, ##args)
#define PWC_DEBUG_FLOW(fmt, args...) PWC_DEBUG(FLOW, fmt, ##args)
#define PWC_DEBUG_SIZE(fmt, args...) PWC_DEBUG(SIZE, fmt, ##args)
#define PWC_DEBUG_IOCTL(fmt, args...) PWC_DEBUG(IOCTL, fmt, ##args)
#define PWC_DEBUG_TRACE(fmt, args...) PWC_DEBUG(TRACE, fmt, ##args)


#ifdef CONFIG_USB_PWC_DEBUG

#define PWC_DEBUG_LEVEL	(PWC_DEBUG_LEVEL_MODULE)

#define PWC_DEBUG(level, fmt, args...) do {\
	if ((PWC_DEBUG_LEVEL_ ##level) & pwc_trace) \
		printk(KERN_DEBUG PFX fmt, ##args); \
	} while (0)

#define PWC_ERROR(fmt, args...) printk(KERN_ERR PFX fmt, ##args)
#define PWC_WARNING(fmt, args...) printk(KERN_WARNING PFX fmt, ##args)
#define PWC_INFO(fmt, args...) printk(KERN_INFO PFX fmt, ##args)
#define PWC_TRACE(fmt, args...) PWC_DEBUG(TRACE, fmt, ##args)

#else 

#define PWC_ERROR(fmt, args...) printk(KERN_ERR PFX fmt, ##args)
#define PWC_WARNING(fmt, args...) printk(KERN_WARNING PFX fmt, ##args)
#define PWC_INFO(fmt, args...) printk(KERN_INFO PFX fmt, ##args)
#define PWC_TRACE(fmt, args...) do { } while(0)
#define PWC_DEBUG(level, fmt, args...) do { } while(0)

#define pwc_trace 0

#endif

#define TOUCAM_HEADER_SIZE		8
#define TOUCAM_TRAILER_SIZE		4

#define FEATURE_MOTOR_PANTILT		0x0001
#define FEATURE_CODEC1			0x0002
#define FEATURE_CODEC2			0x0004

#define MAX_WIDTH		640
#define MAX_HEIGHT		480

#define FRAME_LOWMARK 5

#define MAX_ISO_BUFS		3
#define ISO_FRAMES_PER_DESC	10
#define ISO_MAX_FRAME_SIZE	960
#define ISO_BUFFER_SIZE 	(ISO_FRAMES_PER_DESC * ISO_MAX_FRAME_SIZE)

#define PWC_FRAME_SIZE 		(460800 + TOUCAM_HEADER_SIZE + TOUCAM_TRAILER_SIZE)

#define MIN_FRAMES		2
#define MAX_FRAMES		16

#define DEVICE_USE_CODEC1(x) ((x)<675)
#define DEVICE_USE_CODEC2(x) ((x)>=675 && (x)<700)
#define DEVICE_USE_CODEC3(x) ((x)>=700)
#define DEVICE_USE_CODEC23(x) ((x)>=675)

#define SET_LUM_CTL			0x01
#define GET_LUM_CTL			0x02
#define SET_CHROM_CTL			0x03
#define GET_CHROM_CTL			0x04
#define SET_STATUS_CTL			0x05
#define GET_STATUS_CTL			0x06
#define SET_EP_STREAM_CTL		0x07
#define GET_EP_STREAM_CTL		0x08
#define GET_XX_CTL			0x09
#define SET_XX_CTL			0x0A
#define GET_XY_CTL			0x0B
#define SET_XY_CTL			0x0C
#define SET_MPT_CTL			0x0D
#define GET_MPT_CTL			0x0E

#define AGC_MODE_FORMATTER			0x2000
#define PRESET_AGC_FORMATTER			0x2100
#define SHUTTER_MODE_FORMATTER			0x2200
#define PRESET_SHUTTER_FORMATTER		0x2300
#define PRESET_CONTOUR_FORMATTER		0x2400
#define AUTO_CONTOUR_FORMATTER			0x2500
#define BACK_LIGHT_COMPENSATION_FORMATTER	0x2600
#define CONTRAST_FORMATTER			0x2700
#define DYNAMIC_NOISE_CONTROL_FORMATTER		0x2800
#define FLICKERLESS_MODE_FORMATTER		0x2900
#define AE_CONTROL_SPEED			0x2A00
#define BRIGHTNESS_FORMATTER			0x2B00
#define GAMMA_FORMATTER				0x2C00

#define WB_MODE_FORMATTER			0x1000
#define AWB_CONTROL_SPEED_FORMATTER		0x1100
#define AWB_CONTROL_DELAY_FORMATTER		0x1200
#define PRESET_MANUAL_RED_GAIN_FORMATTER	0x1300
#define PRESET_MANUAL_BLUE_GAIN_FORMATTER	0x1400
#define COLOUR_MODE_FORMATTER			0x1500
#define SATURATION_MODE_FORMATTER1		0x1600
#define SATURATION_MODE_FORMATTER2		0x1700

#define SAVE_USER_DEFAULTS_FORMATTER		0x0200
#define RESTORE_USER_DEFAULTS_FORMATTER		0x0300
#define RESTORE_FACTORY_DEFAULTS_FORMATTER	0x0400
#define READ_AGC_FORMATTER			0x0500
#define READ_SHUTTER_FORMATTER			0x0600
#define READ_RED_GAIN_FORMATTER			0x0700
#define READ_BLUE_GAIN_FORMATTER		0x0800

#define PT_RELATIVE_CONTROL_FORMATTER		0x01
#define PT_RESET_CONTROL_FORMATTER		0x02
#define PT_STATUS_FORMATTER			0x03

#define PSZ_SQCIF	0x00
#define PSZ_QSIF	0x01
#define PSZ_QCIF	0x02
#define PSZ_SIF		0x03
#define PSZ_CIF		0x04
#define PSZ_VGA		0x05
#define PSZ_MAX		6

struct pwc_raw_frame {
	__le16 type;		
	__le16 vbandlength;	
	__u8   cmd[4];		
	__u8   rawframe[0];	
} __packed;

struct pwc_frame_buf
{
	struct vb2_buffer vb;	
	struct list_head list;
	void *data;
	int filled;		
};

struct pwc_device
{
	struct video_device vdev;
	struct v4l2_device v4l2_dev;

	
	struct usb_device *udev;
	struct mutex udevlock;

	
	int type;
	int release;		
	int features;		

	
	struct file *capt_file;	
	struct mutex capt_file_lock;
	int vendpoint;		
	int vcinterface;	
	int valternate;		
	int vframes;		
	int pixfmt;		
	int vframe_count;	
	int vmax_packet_size;	
	int vlast_packet_size;	
	int visoc_errors;	
	int vbandlength;	
	char vsync;		
	char vmirror;		
	char power_save;	

	unsigned char cmd_buf[13];
	unsigned char *ctrl_buf;

	struct urb *urbs[MAX_ISO_BUFS];
	char iso_init;

	
	struct vb2_queue vb_queue;
	struct list_head queued_bufs;
	spinlock_t queued_bufs_lock;

	struct pwc_frame_buf *fill_buf;

	int frame_header_size, frame_trailer_size;
	int frame_size;
	int frame_total_size;	
	int drop_frames;

	union {	
		struct pwc_dec1_private dec1;
		struct pwc_dec23_private dec23;
	};

	int image_mask;				
	int width, height;			

#ifdef CONFIG_USB_PWC_INPUT_EVDEV
	struct input_dev *button_dev;	
	char button_phys[64];
#endif

	
	struct v4l2_ctrl_handler	ctrl_handler;
	u16				saturation_fmt;
	struct v4l2_ctrl		*brightness;
	struct v4l2_ctrl		*contrast;
	struct v4l2_ctrl		*saturation;
	struct v4l2_ctrl		*gamma;
	struct {
		
		struct v4l2_ctrl	*auto_white_balance;
		struct v4l2_ctrl	*red_balance;
		struct v4l2_ctrl	*blue_balance;
		
		int			color_bal_valid;
		unsigned long		last_color_bal_update; 
		s32			last_red_balance;
		s32			last_blue_balance;
	};
	struct {
		
		struct v4l2_ctrl	*autogain;
		struct v4l2_ctrl	*gain;
		int			gain_valid;
		unsigned long		last_gain_update; 
		s32			last_gain;
	};
	struct {
		
		struct v4l2_ctrl	*exposure_auto;
		struct v4l2_ctrl	*exposure;
		int			exposure_valid;
		unsigned long		last_exposure_update; 
		s32			last_exposure;
	};
	struct v4l2_ctrl		*colorfx;
	struct {
		
		struct v4l2_ctrl	*autocontour;
		struct v4l2_ctrl	*contour;
	};
	struct v4l2_ctrl		*backlight;
	struct v4l2_ctrl		*flicker;
	struct v4l2_ctrl		*noise_reduction;
	struct v4l2_ctrl		*save_user;
	struct v4l2_ctrl		*restore_user;
	struct v4l2_ctrl		*restore_factory;
	struct v4l2_ctrl		*awb_speed;
	struct v4l2_ctrl		*awb_delay;
	struct {
		
		struct v4l2_ctrl	*motor_pan;
		struct v4l2_ctrl	*motor_tilt;
		struct v4l2_ctrl	*motor_pan_reset;
		struct v4l2_ctrl	*motor_tilt_reset;
	};
	
	struct v4l2_ctrl		*autogain_expo_cluster[3];
};

#ifdef CONFIG_USB_PWC_DEBUG
extern int pwc_trace;
#endif

int pwc_test_n_set_capt_file(struct pwc_device *pdev, struct file *file);

extern const int pwc_image_sizes[PSZ_MAX][2];

int pwc_get_size(struct pwc_device *pdev, int width, int height);
void pwc_construct(struct pwc_device *pdev);

extern int pwc_set_video_mode(struct pwc_device *pdev, int width, int height,
	int pixfmt, int frames, int *compression, int send_to_cam);
extern unsigned int pwc_get_fps(struct pwc_device *pdev, unsigned int index, unsigned int size);
extern int pwc_set_leds(struct pwc_device *pdev, int on_value, int off_value);
extern int pwc_get_cmos_sensor(struct pwc_device *pdev, int *sensor);
extern int send_control_msg(struct pwc_device *pdev,
			    u8 request, u16 value, void *buf, int buflen);

int pwc_get_u8_ctrl(struct pwc_device *pdev, u8 request, u16 value, int *data);
int pwc_set_u8_ctrl(struct pwc_device *pdev, u8 request, u16 value, u8 data);
int pwc_get_s8_ctrl(struct pwc_device *pdev, u8 request, u16 value, int *data);
#define pwc_set_s8_ctrl pwc_set_u8_ctrl
int pwc_get_u16_ctrl(struct pwc_device *pdev, u8 request, u16 value, int *dat);
int pwc_set_u16_ctrl(struct pwc_device *pdev, u8 request, u16 value, u16 data);
int pwc_button_ctrl(struct pwc_device *pdev, u16 value);
int pwc_init_controls(struct pwc_device *pdev);

extern void pwc_camera_power(struct pwc_device *pdev, int power);

extern const struct v4l2_ioctl_ops pwc_ioctl_ops;

int pwc_decompress(struct pwc_device *pdev, struct pwc_frame_buf *fbuf);

#endif
