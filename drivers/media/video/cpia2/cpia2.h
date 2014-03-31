/****************************************************************************
 *
 *  Filename: cpia2.h
 *
 *  Copyright 2001, STMicrolectronics, Inc.
 *
 *  Contact:  steve.miller@st.com
 *
 *  Description:
 *     This is a USB driver for CPiA2 based video cameras.
 *
 *     This driver is modelled on the cpia usb driver by
 *     Jochen Scharrlach and Johannes Erdfeldt.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ****************************************************************************/

#ifndef __CPIA2_H__
#define __CPIA2_H__

#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <linux/usb.h>
#include <linux/poll.h>

#include "cpia2dev.h"
#include "cpia2_registers.h"



#define ALLOW_CORRUPT 0		

#define XFER_ISOC 0
#define XFER_BULK 1

#define USBIF_CMDONLY 0
#define USBIF_BULK 1
#define USBIF_ISO_1 2	
#define USBIF_ISO_2 3	
#define USBIF_ISO_3 4	
#define USBIF_ISO_4 5	
#define USBIF_ISO_5 6	
#define USBIF_ISO_6 7	

#define NEVER_FLICKER   0
#define ANTI_FLICKER_ON 1
#define FLICKER_60      60
#define FLICKER_50      50

#define DEBUG_NONE          0
#define DEBUG_REG           0x00000001
#define DEBUG_DUMP_PATCH    0x00000002
#define DEBUG_DUMP_REGS     0x00000004

enum {
	VIDEOSIZE_VGA = 0,	
	VIDEOSIZE_CIF,		
	VIDEOSIZE_QVGA,		
	VIDEOSIZE_QCIF,		
	VIDEOSIZE_288_216,
	VIDEOSIZE_256_192,
	VIDEOSIZE_224_168,
	VIDEOSIZE_192_144,
};

#define STV_IMAGE_CIF_ROWS    288
#define STV_IMAGE_CIF_COLS    352

#define STV_IMAGE_QCIF_ROWS   144
#define STV_IMAGE_QCIF_COLS   176

#define STV_IMAGE_VGA_ROWS    480
#define STV_IMAGE_VGA_COLS    640

#define STV_IMAGE_QVGA_ROWS   240
#define STV_IMAGE_QVGA_COLS   320

#define JPEG_MARKER_COM (1<<6)	

enum sensors {
	CPIA2_SENSOR_410,
	CPIA2_SENSOR_500
};

#define  CPIA2_ASIC_672 0x67

#define  DEVICE_STV_672   0x0001
#define  DEVICE_STV_676   0x0002

enum frame_status {
	FRAME_EMPTY,
	FRAME_READING,		
	FRAME_READY,		
	FRAME_ERROR,
};

enum {
	CAMERAACCESS_SYSTEM = 0,
	CAMERAACCESS_VC,
	CAMERAACCESS_VP,
	CAMERAACCESS_IDATA
};

#define CAMERAACCESS_TYPE_BLOCK    0x00
#define CAMERAACCESS_TYPE_RANDOM   0x04
#define CAMERAACCESS_TYPE_MASK     0x08
#define CAMERAACCESS_TYPE_REPEAT   0x0C

#define TRANSFER_READ 0
#define TRANSFER_WRITE 1

#define DEFAULT_ALT   USBIF_ISO_6
#define DEFAULT_BRIGHTNESS 0x46
#define DEFAULT_CONTRAST 0x93
#define DEFAULT_SATURATION 0x7f
#define DEFAULT_TARGET_KB 0x30

#define HI_POWER_MODE CPIA2_SYSTEM_CONTROL_HIGH_POWER
#define LO_POWER_MODE CPIA2_SYSTEM_CONTROL_LOW_POWER


enum {
	CPIA2_CMD_NONE = 0,
	CPIA2_CMD_GET_VERSION,
	CPIA2_CMD_GET_PNP_ID,
	CPIA2_CMD_GET_ASIC_TYPE,
	CPIA2_CMD_GET_SENSOR,
	CPIA2_CMD_GET_VP_DEVICE,
	CPIA2_CMD_GET_VP_BRIGHTNESS,
	CPIA2_CMD_SET_VP_BRIGHTNESS,
	CPIA2_CMD_GET_CONTRAST,
	CPIA2_CMD_SET_CONTRAST,
	CPIA2_CMD_GET_VP_SATURATION,
	CPIA2_CMD_SET_VP_SATURATION,
	CPIA2_CMD_GET_VP_GPIO_DIRECTION,
	CPIA2_CMD_SET_VP_GPIO_DIRECTION,
	CPIA2_CMD_GET_VP_GPIO_DATA,
	CPIA2_CMD_SET_VP_GPIO_DATA,
	CPIA2_CMD_GET_VC_MP_GPIO_DIRECTION,
	CPIA2_CMD_SET_VC_MP_GPIO_DIRECTION,
	CPIA2_CMD_GET_VC_MP_GPIO_DATA,
	CPIA2_CMD_SET_VC_MP_GPIO_DATA,
	CPIA2_CMD_ENABLE_PACKET_CTRL,
	CPIA2_CMD_GET_FLICKER_MODES,
	CPIA2_CMD_SET_FLICKER_MODES,
	CPIA2_CMD_RESET_FIFO,	
	CPIA2_CMD_SET_HI_POWER,
	CPIA2_CMD_SET_LOW_POWER,
	CPIA2_CMD_CLEAR_V2W_ERR,
	CPIA2_CMD_SET_USER_MODE,
	CPIA2_CMD_GET_USER_MODE,
	CPIA2_CMD_FRAMERATE_REQ,
	CPIA2_CMD_SET_COMPRESSION_STATE,
	CPIA2_CMD_GET_WAKEUP,
	CPIA2_CMD_SET_WAKEUP,
	CPIA2_CMD_GET_PW_CONTROL,
	CPIA2_CMD_SET_PW_CONTROL,
	CPIA2_CMD_GET_SYSTEM_CTRL,
	CPIA2_CMD_SET_SYSTEM_CTRL,
	CPIA2_CMD_GET_VP_SYSTEM_STATE,
	CPIA2_CMD_GET_VP_SYSTEM_CTRL,
	CPIA2_CMD_SET_VP_SYSTEM_CTRL,
	CPIA2_CMD_GET_VP_EXP_MODES,
	CPIA2_CMD_SET_VP_EXP_MODES,
	CPIA2_CMD_GET_DEVICE_CONFIG,
	CPIA2_CMD_SET_DEVICE_CONFIG,
	CPIA2_CMD_SET_SERIAL_ADDR,
	CPIA2_CMD_SET_SENSOR_CR1,
	CPIA2_CMD_GET_VC_CONTROL,
	CPIA2_CMD_SET_VC_CONTROL,
	CPIA2_CMD_SET_TARGET_KB,
	CPIA2_CMD_SET_DEF_JPEG_OPT,
	CPIA2_CMD_REHASH_VP4,
	CPIA2_CMD_GET_USER_EFFECTS,
	CPIA2_CMD_SET_USER_EFFECTS
};

enum user_cmd {
	COMMAND_NONE = 0x00000001,
	COMMAND_SET_FPS = 0x00000002,
	COMMAND_SET_COLOR_PARAMS = 0x00000004,
	COMMAND_GET_COLOR_PARAMS = 0x00000008,
	COMMAND_SET_FORMAT = 0x00000010,	
	COMMAND_SET_FLICKER = 0x00000020
};

#define CAMACC_CIF      0x01
#define CAMACC_VGA      0x02
#define CAMACC_QCIF     0x04
#define CAMACC_QVGA     0x08


struct cpia2_register {
	u8 index;
	u8 value;
};

struct cpia2_reg_mask {
	u8 index;
	u8 and_mask;
	u8 or_mask;
	u8 fill;
};

struct cpia2_command {
	u32 command;
	u8 req_mode;		
	u8 reg_count;
	u8 direction;
	u8 start;
	union reg_types {
		struct cpia2_register registers[32];
		struct cpia2_reg_mask masks[16];
		u8 block_data[64];
		u8 *patch_data;	
	} buffer;
};

struct camera_params {
	struct {
		u8 firmware_revision_hi; 
		u8 firmware_revision_lo;
		u8 asic_id;	
		u8 asic_rev;
		u8 vp_device_hi;	
		u8 vp_device_lo;
		u8 sensor_flags;
		u8 sensor_rev;
	} version;

	struct {
		u32 device_type;     
		u16 vendor;
		u16 product;
		u16 device_revision;
	} pnp_id;

	struct {
		u8 brightness;	
		u8 contrast;	
		u8 saturation;	
	} color_params;

	struct {
		u8 cam_register;
		u8 flicker_mode_req;	
		int mains_frequency;
	} flicker_control;

	struct {
		u8 jpeg_options;
		u8 creep_period;
		u8 user_squeeze;
		u8 inhibit_htables;
	} compression;

	struct {
		u8 ohsize;	
		u8 ovsize;
		u8 hcrop;	
		u8 vcrop;
		u8 hphase;	
		u8 vphase;
		u8 hispan;
		u8 vispan;
		u8 hicrop;
		u8 vicrop;
		u8 hifraction;
		u8 vifraction;
	} image_size;

	struct {
		int width;	
		int height;	
	} roi;

	struct {
		u8 video_mode;
		u8 frame_rate;
		u8 video_size;	
		u8 gpio_direction;
		u8 gpio_data;
		u8 system_ctrl;
		u8 system_state;
		u8 lowlight_boost;	
		u8 device_config;
		u8 exposure_modes;
		u8 user_effects;
	} vp_params;

	struct {
		u8 pw_control;
		u8 wakeup;
		u8 vc_control;
		u8 vc_mp_direction;
		u8 vc_mp_data;
		u8 target_kb;
	} vc_params;

	struct {
		u8 power_mode;
		u8 system_ctrl;
		u8 stream_mode;	
		u8 allow_corrupt;
	} camera_state;
};

#define NUM_SBUF    2

struct cpia2_sbuf {
	char *data;
	struct urb *urb;
};

struct framebuf {
	struct timeval timestamp;
	unsigned long seq;
	int num;
	int length;
	int max_length;
	volatile enum frame_status status;
	u8 *data;
	struct framebuf *next;
};

struct cpia2_fh {
	enum v4l2_priority prio;
	u8 mmapped;
};

struct camera_data {
	
	struct mutex v4l2_lock;	
	struct v4l2_prio_state prio;

	
	volatile int present;	
	int open_count;		
	int first_image_seen;
	u8 mains_freq;		
	enum sensors sensor_type;
	u8 flush;
	u8 mmapped;
	int streaming;		
	int xfer_mode;		
	struct camera_params params;	

	
	int video_size;			
	struct video_device *vdev;	
	u32 width;
	u32 height;			
	__u32 pixelformat;       

	
	struct usb_device *dev;
	unsigned char iface;
	unsigned int cur_alt;
	unsigned int old_alt;
	struct cpia2_sbuf sbuf[NUM_SBUF];	

	wait_queue_head_t wq_stream;

	
	u32 frame_size;
	int num_frames;
	unsigned long frame_count;
	u8 *frame_buffer;	
	struct framebuf *buffers;
	struct framebuf * volatile curbuff;
	struct framebuf *workbuff;

	
	int APPn;		/* Number of APP segment to be written, must be 0..15 */
	int APP_len;		
	char APP_data[60];	

	int COM_len;		
	char COM_data[60];	
};

int cpia2_register_camera(struct camera_data *cam);
void cpia2_unregister_camera(struct camera_data *cam);

int cpia2_reset_camera(struct camera_data *cam);
int cpia2_set_low_power(struct camera_data *cam);
void cpia2_dbg_dump_registers(struct camera_data *cam);
int cpia2_match_video_size(int width, int height);
void cpia2_set_camera_state(struct camera_data *cam);
void cpia2_save_camera_state(struct camera_data *cam);
void cpia2_set_color_params(struct camera_data *cam);
void cpia2_set_brightness(struct camera_data *cam, unsigned char value);
void cpia2_set_contrast(struct camera_data *cam, unsigned char value);
void cpia2_set_saturation(struct camera_data *cam, unsigned char value);
int cpia2_set_flicker_mode(struct camera_data *cam, int mode);
void cpia2_set_format(struct camera_data *cam);
int cpia2_send_command(struct camera_data *cam, struct cpia2_command *cmd);
int cpia2_do_command(struct camera_data *cam,
		     unsigned int command,
		     unsigned char direction, unsigned char param);
struct camera_data *cpia2_init_camera_struct(void);
int cpia2_init_camera(struct camera_data *cam);
int cpia2_allocate_buffers(struct camera_data *cam);
void cpia2_free_buffers(struct camera_data *cam);
long cpia2_read(struct camera_data *cam,
		char __user *buf, unsigned long count, int noblock);
unsigned int cpia2_poll(struct camera_data *cam,
			struct file *filp, poll_table *wait);
int cpia2_remap_buffer(struct camera_data *cam, struct vm_area_struct *vma);
void cpia2_set_property_flip(struct camera_data *cam, int prop_val);
void cpia2_set_property_mirror(struct camera_data *cam, int prop_val);
int cpia2_set_target_kb(struct camera_data *cam, unsigned char value);
int cpia2_set_gpio(struct camera_data *cam, unsigned char setting);
int cpia2_set_fps(struct camera_data *cam, int framerate);

int cpia2_usb_init(void);
void cpia2_usb_cleanup(void);
int cpia2_usb_transfer_cmd(struct camera_data *cam, void *registers,
			   u8 request, u8 start, u8 count, u8 direction);
int cpia2_usb_stream_start(struct camera_data *cam, unsigned int alternate);
int cpia2_usb_stream_stop(struct camera_data *cam);
int cpia2_usb_stream_pause(struct camera_data *cam);
int cpia2_usb_stream_resume(struct camera_data *cam);
int cpia2_usb_change_streaming_alternate(struct camera_data *cam,
					 unsigned int alt);


#ifdef _CPIA2_DEBUG_
#define ALOG(lev, fmt, args...) printk(lev "%s:%d %s(): " fmt, __FILE__, __LINE__, __func__, ## args)
#define LOG(fmt, args...) ALOG(KERN_INFO, fmt, ## args)
#define ERR(fmt, args...) ALOG(KERN_ERR, fmt, ## args)
#define DBG(fmt, args...) ALOG(KERN_DEBUG, fmt, ## args)
#else
#define ALOG(fmt,args...) printk(fmt,##args)
#define LOG(fmt,args...) ALOG(KERN_INFO "cpia2: "fmt,##args)
#define ERR(fmt,args...) ALOG(KERN_ERR "cpia2: "fmt,##args)
#define DBG(fmn,args...) do {} while(0)
#endif
#define KINFO(fmt, args...) printk(KERN_INFO fmt,##args)

#endif
