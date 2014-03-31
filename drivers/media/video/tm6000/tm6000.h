/*
 *  tm6000.h - driver for TM5600/TM6000/TM6010 USB video capture devices
 *
 *  Copyright (C) 2006-2007 Mauro Carvalho Chehab <mchehab@infradead.org>
 *
 *  Copyright (C) 2007 Michel Ludwig <michel.ludwig@gmail.com>
 *	- DVB-T support
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation version 2
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/videobuf-vmalloc.h>
#include "tm6000-usb-isoc.h"
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <media/v4l2-device.h>

#include <linux/dvb/frontend.h>
#include "dvb_demux.h"
#include "dvb_frontend.h"
#include "dmxdev.h"

#define TM6000_VERSION KERNEL_VERSION(0, 0, 2)

enum tm6000_itype {
	TM6000_INPUT_TV	= 1,
	TM6000_INPUT_COMPOSITE1,
	TM6000_INPUT_COMPOSITE2,
	TM6000_INPUT_SVIDEO,
	TM6000_INPUT_DVB,
	TM6000_INPUT_RADIO,
};

enum tm6000_mux {
	TM6000_VMUX_VIDEO_A = 1,
	TM6000_VMUX_VIDEO_B,
	TM6000_VMUX_VIDEO_AB,
	TM6000_AMUX_ADC1,
	TM6000_AMUX_ADC2,
	TM6000_AMUX_SIF1,
	TM6000_AMUX_SIF2,
	TM6000_AMUX_I2S,
};

enum tm6000_devtype {
	TM6000 = 0,
	TM5600,
	TM6010,
};

struct tm6000_input {
	enum tm6000_itype	type;
	enum tm6000_mux		vmux;
	enum tm6000_mux		amux;
	unsigned int		v_gpio;
	unsigned int		a_gpio;
};


struct tm6000_fmt {
	char  *name;
	u32   fourcc;          
	int   depth;
};

struct tm6000_buffer {
	
	struct videobuf_buffer vb;

	struct tm6000_fmt      *fmt;
};

struct tm6000_dmaqueue {
	struct list_head       active;
	struct list_head       queued;

	
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	
	int                        frame;
	int                        ini_jiffies;
};

enum tm6000_core_state {
	DEV_INITIALIZED   = 0x01,
	DEV_DISCONNECTED  = 0x02,
	DEV_MISCONFIGURED = 0x04,
};

enum tm6000_io_method {
	IO_NONE,
	IO_READ,
	IO_MMAP,
};

enum tm6000_mode {
	TM6000_MODE_UNKNOWN = 0,
	TM6000_MODE_ANALOG,
	TM6000_MODE_DIGITAL,
};

struct tm6000_gpio {
	int		tuner_reset;
	int		tuner_on;
	int		demod_reset;
	int		demod_on;
	int		power_led;
	int		dvb_led;
	int		ir;
};

struct tm6000_capabilities {
	unsigned int    has_tuner:1;
	unsigned int    has_tda9874:1;
	unsigned int    has_dvb:1;
	unsigned int    has_zl10353:1;
	unsigned int    has_eeprom:1;
	unsigned int    has_remote:1;
	unsigned int    has_radio:1;
};

struct tm6000_dvb {
	struct dvb_adapter	adapter;
	struct dvb_demux	demux;
	struct dvb_frontend	*frontend;
	struct dmxdev		dmxdev;
	unsigned int		streams;
	struct urb		*bulk_urb;
	struct mutex		mutex;
};

struct snd_tm6000_card {
	struct snd_card			*card;
	spinlock_t			reg_lock;
	struct tm6000_core		*core;
	struct snd_pcm_substream	*substream;

	
	unsigned			buf_pos;
	unsigned			period_pos;
};

struct tm6000_endpoint {
	struct usb_host_endpoint	*endp;
	__u8				bInterfaceNumber;
	__u8				bAlternateSetting;
	unsigned			maxsize;
};

#define TM6000_QUIRK_NO_USB_DELAY (1 << 0)

struct tm6000_core {
	
	char				name[30];	
	int				model;		
	int				devno;		
	enum tm6000_devtype		dev_type;	
	unsigned char			eedata[256];	
	unsigned			eedata_size;	

	v4l2_std_id                     norm;           
	int				width, height;	

	enum tm6000_core_state		state;

	
	struct tm6000_capabilities	caps;

	
        struct work_struct		request_module_wk;

	
	int				tuner_type;		
	int				tuner_addr;		

	struct tm6000_gpio		gpio;

	char				*ir_codes;

	__u8				radio;

	
	int				demod_addr;	

	int				audio_bitrate;
	
	struct i2c_adapter		i2c_adap;
	struct i2c_client		i2c_client;


	
	struct list_head		devlist;

	
	int				users;

	
	struct tm6000_fh		*resources;	
	bool				is_res_read;

	struct video_device		*vfd;
	struct video_device		*radio_dev;
	struct tm6000_dmaqueue		vidq;
	struct v4l2_device		v4l2_dev;

	int				input;
	struct tm6000_input		vinput[3];	
	struct tm6000_input		rinput;		

	int				freq;
	unsigned int			fourcc;

	enum tm6000_mode		mode;

	int				ctl_mute;             
	int				ctl_volume;
	int				amode;

	
	struct tm6000_dvb		*dvb;

	
	struct snd_tm6000_card		*adev;
	struct work_struct		wq_trigger;   
	atomic_t			stream_started;  

	struct tm6000_IR		*ir;

	
	struct mutex			lock;
	struct mutex			usb_lock;

	
	struct usb_device		*udev;		

	struct tm6000_endpoint		bulk_in, bulk_out, isoc_in, isoc_out;
	struct tm6000_endpoint		int_in, int_out;

	
	int				scaler;

		
	struct usb_isoc_ctl          isoc_ctl;

	spinlock_t                   slock;

	unsigned long quirks;
};

enum tm6000_ops_type {
	TM6000_AUDIO = 0x10,
	TM6000_DVB = 0x20,
};

struct tm6000_ops {
	struct list_head	next;
	char			*name;
	enum tm6000_ops_type	type;
	int (*init)(struct tm6000_core *);
	int (*fini)(struct tm6000_core *);
	int (*fillbuf)(struct tm6000_core *, char *buf, int size);
};

struct tm6000_fh {
	struct tm6000_core           *dev;
	unsigned int                 radio;

	
	struct tm6000_fmt            *fmt;
	unsigned int                 width, height;
	struct videobuf_queue        vb_vidq;

	enum v4l2_buf_type           type;
};

#define TM6000_STD	(V4L2_STD_PAL|V4L2_STD_PAL_N|V4L2_STD_PAL_Nc|    \
			V4L2_STD_PAL_M|V4L2_STD_PAL_60|V4L2_STD_NTSC_M| \
			V4L2_STD_NTSC_M_JP|V4L2_STD_SECAM)


int tm6000_tuner_callback(void *ptr, int component, int command, int arg);
int tm6000_xc5000_callback(void *ptr, int component, int command, int arg);
int tm6000_cards_setup(struct tm6000_core *dev);
void tm6000_flash_led(struct tm6000_core *dev, u8 state);


int tm6000_read_write_usb(struct tm6000_core *dev, u8 reqtype, u8 req,
			   u16 value, u16 index, u8 *buf, u16 len);
int tm6000_get_reg(struct tm6000_core *dev, u8 req, u16 value, u16 index);
int tm6000_get_reg16(struct tm6000_core *dev, u8 req, u16 value, u16 index);
int tm6000_get_reg32(struct tm6000_core *dev, u8 req, u16 value, u16 index);
int tm6000_set_reg(struct tm6000_core *dev, u8 req, u16 value, u16 index);
int tm6000_set_reg_mask(struct tm6000_core *dev, u8 req, u16 value,
						u16 index, u16 mask);
int tm6000_i2c_reset(struct tm6000_core *dev, u16 tsleep);
int tm6000_init(struct tm6000_core *dev);
int tm6000_reset(struct tm6000_core *dev);

int tm6000_init_analog_mode(struct tm6000_core *dev);
int tm6000_init_digital_mode(struct tm6000_core *dev);
int tm6000_set_audio_bitrate(struct tm6000_core *dev, int bitrate);
int tm6000_set_audio_rinput(struct tm6000_core *dev);
int tm6000_tvaudio_set_mute(struct tm6000_core *dev, u8 mute);
void tm6000_set_volume(struct tm6000_core *dev, int vol);

int tm6000_v4l2_register(struct tm6000_core *dev);
int tm6000_v4l2_unregister(struct tm6000_core *dev);
int tm6000_v4l2_exit(void);
void tm6000_set_fourcc_format(struct tm6000_core *dev);

void tm6000_remove_from_devlist(struct tm6000_core *dev);
void tm6000_add_into_devlist(struct tm6000_core *dev);
int tm6000_register_extension(struct tm6000_ops *ops);
void tm6000_unregister_extension(struct tm6000_ops *ops);
void tm6000_init_extension(struct tm6000_core *dev);
void tm6000_close_extension(struct tm6000_core *dev);
int tm6000_call_fillbuf(struct tm6000_core *dev, enum tm6000_ops_type type,
			char *buf, int size);


void tm6000_get_std_res(struct tm6000_core *dev);
int tm6000_set_standard(struct tm6000_core *dev);

int tm6000_i2c_register(struct tm6000_core *dev);
int tm6000_i2c_unregister(struct tm6000_core *dev);


int tm6000_v4l2_mmap(struct file *filp, struct vm_area_struct *vma);

int tm6000_vidioc_streamon(struct file *file, void *priv,
			   enum v4l2_buf_type i);
int tm6000_vidioc_streamoff(struct file *file, void *priv,
			    enum v4l2_buf_type i);
int tm6000_vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *rb);
int tm6000_vidioc_querybuf(struct file *file, void *priv,
			   struct v4l2_buffer *b);
int tm6000_vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *b);
int tm6000_vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *b);
ssize_t tm6000_v4l2_read(struct file *filp, char __user * buf, size_t count,
			 loff_t *f_pos);
unsigned int tm6000_v4l2_poll(struct file *file,
			      struct poll_table_struct *wait);
int tm6000_queue_init(struct tm6000_core *dev);


int tm6000_ir_init(struct tm6000_core *dev);
int tm6000_ir_fini(struct tm6000_core *dev);
void tm6000_ir_wait(struct tm6000_core *dev, u8 state);
int tm6000_ir_int_start(struct tm6000_core *dev);
void tm6000_ir_int_stop(struct tm6000_core *dev);


extern int tm6000_debug;

#define dprintk(dev, level, fmt, arg...) do {\
	if (tm6000_debug & level) \
		printk(KERN_INFO "(%lu) %s %s :"fmt, jiffies, \
			 dev->name, __func__ , ##arg); } while (0)

#define V4L2_DEBUG_REG		0x0004
#define V4L2_DEBUG_I2C		0x0008
#define V4L2_DEBUG_QUEUE	0x0010
#define V4L2_DEBUG_ISOC		0x0020
#define V4L2_DEBUG_RES_LOCK	0x0040	
#define V4L2_DEBUG_OPEN		0x0080	

#define tm6000_err(fmt, arg...) do {\
	printk(KERN_ERR "tm6000 %s :"fmt, \
		__func__ , ##arg); } while (0)
