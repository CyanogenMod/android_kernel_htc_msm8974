/*
 * Hauppauge HD PVR USB driver
 *
 * Copyright (C) 2008      Janne Grunau (j@jannau.net)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 */

#include <linux/usb.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/videodev2.h>

#include <media/v4l2-device.h>
#include <media/ir-kbd-i2c.h>

#define HDPVR_MAX 8
#define HDPVR_I2C_MAX_SIZE 128

#define HD_PVR_VENDOR_ID	0x2040
#define HD_PVR_PRODUCT_ID	0x4900
#define HD_PVR_PRODUCT_ID1	0x4901
#define HD_PVR_PRODUCT_ID2	0x4902
#define HD_PVR_PRODUCT_ID4	0x4903
#define HD_PVR_PRODUCT_ID3	0x4982

#define UNSET    (-1U)

#define NUM_BUFFERS 64

#define HDPVR_FIRMWARE_VERSION		0x08
#define HDPVR_FIRMWARE_VERSION_AC3	0x0d
#define HDPVR_FIRMWARE_VERSION_0X12	0x12
#define HDPVR_FIRMWARE_VERSION_0X15	0x15


extern int hdpvr_debug;

#define MSG_INFO	1
#define MSG_BUFFER	2

struct hdpvr_options {
	u8	video_std;
	u8	video_input;
	u8	audio_input;
	u8	bitrate;	
	u8	peak_bitrate;	
	u8	bitrate_mode;
	u8	gop_mode;
	enum v4l2_mpeg_audio_encoding	audio_codec;
	u8	brightness;
	u8	contrast;
	u8	hue;
	u8	saturation;
	u8	sharpness;
};

struct hdpvr_device {
	
	struct video_device	*video_dev;
	
	struct usb_device	*udev;
	
	struct v4l2_device	v4l2_dev;

	
	size_t			bulk_in_size;
	
	__u8			bulk_in_endpointAddr;

	
	__u8			status;
	
	uint			open_count;

	
	struct hdpvr_options	options;

	uint			flags;

	
	struct mutex		io_mutex;
	
	struct list_head	free_buff_list;
	
	struct list_head	rec_buff_list;
	
	wait_queue_head_t	wait_buffer;
	
	wait_queue_head_t	wait_data;
	
	struct workqueue_struct	*workqueue;
	
	struct work_struct	worker;

	
	struct i2c_adapter	i2c_adapter;
	
	struct mutex		i2c_mutex;
	
	char			i2c_buf[HDPVR_I2C_MAX_SIZE];

	
	struct IR_i2c_init_data	ir_i2c_init_data;

	
	struct mutex		usbc_mutex;
	u8			*usbc_buf;
	u8			fw_ver;
};

static inline struct hdpvr_device *to_hdpvr_dev(struct v4l2_device *v4l2_dev)
{
	return container_of(v4l2_dev, struct hdpvr_device, v4l2_dev);
}


struct hdpvr_buffer {
	struct list_head	buff_list;

	struct urb		*urb;

	struct hdpvr_device	*dev;

	uint			pos;

	__u8			status;
};


struct hdpvr_video_info {
	u16	width;
	u16	height;
	u8	fps;
};

enum {
	STATUS_UNINITIALIZED	= 0,
	STATUS_IDLE,
	STATUS_STARTING,
	STATUS_SHUTTING_DOWN,
	STATUS_STREAMING,
	STATUS_ERROR,
	STATUS_DISCONNECTED,
};

enum {
	HDPVR_FLAG_AC3_CAP = 1,
};

enum {
	BUFSTAT_UNINITIALIZED = 0,
	BUFSTAT_AVAILABLE,
	BUFSTAT_INPROGRESS,
	BUFSTAT_READY,
};

#define CTRL_START_STREAMING_VALUE	0x0700
#define CTRL_STOP_STREAMING_VALUE	0x0800
#define CTRL_BITRATE_VALUE		0x1000
#define CTRL_BITRATE_MODE_VALUE		0x1200
#define CTRL_GOP_MODE_VALUE		0x1300
#define CTRL_VIDEO_INPUT_VALUE		0x1500
#define CTRL_VIDEO_STD_TYPE		0x1700
#define CTRL_AUDIO_INPUT_VALUE		0x2500
#define CTRL_BRIGHTNESS			0x2900
#define CTRL_CONTRAST			0x2a00
#define CTRL_HUE			0x2b00
#define CTRL_SATURATION			0x2c00
#define CTRL_SHARPNESS			0x2d00
#define CTRL_LOW_PASS_FILTER_VALUE	0x3100

#define CTRL_DEFAULT_INDEX		0x0003








	








enum hdpvr_video_std {
	HDPVR_60HZ = 0,
	HDPVR_50HZ,
};

enum hdpvr_video_input {
	HDPVR_COMPONENT = 0,
	HDPVR_SVIDEO,
	HDPVR_COMPOSITE,
	HDPVR_VIDEO_INPUTS
};

enum hdpvr_audio_inputs {
	HDPVR_RCA_BACK = 0,
	HDPVR_RCA_FRONT,
	HDPVR_SPDIF,
	HDPVR_AUDIO_INPUTS
};

enum hdpvr_bitrate_mode {
	HDPVR_CONSTANT = 1,
	HDPVR_VARIABLE_PEAK,
	HDPVR_VARIABLE_AVERAGE,
};

enum hdpvr_gop_mode {
	HDPVR_ADVANCED_IDR_GOP = 0,
	HDPVR_SIMPLE_IDR_GOP,
	HDPVR_ADVANCED_NOIDR_GOP,
	HDPVR_SIMPLE_NOIDR_GOP,
};

void hdpvr_delete(struct hdpvr_device *dev);

int hdpvr_set_options(struct hdpvr_device *dev);

int hdpvr_set_bitrate(struct hdpvr_device *dev);

int hdpvr_set_audio(struct hdpvr_device *dev, u8 input,
		    enum v4l2_mpeg_audio_encoding codec);

int hdpvr_config_call(struct hdpvr_device *dev, uint value,
		      unsigned char valbuf);

struct hdpvr_video_info *get_video_info(struct hdpvr_device *dev);

int get_input_lines_info(struct hdpvr_device *dev);


int hdpvr_register_videodev(struct hdpvr_device *dev, struct device *parent,
			    int devnumber);

int hdpvr_cancel_queue(struct hdpvr_device *dev);

int hdpvr_register_i2c_adapter(struct hdpvr_device *dev);

struct i2c_client *hdpvr_register_ir_rx_i2c(struct hdpvr_device *dev);
struct i2c_client *hdpvr_register_ir_tx_i2c(struct hdpvr_device *dev);

int hdpvr_free_buffers(struct hdpvr_device *dev);
int hdpvr_alloc_buffers(struct hdpvr_device *dev, uint count);
