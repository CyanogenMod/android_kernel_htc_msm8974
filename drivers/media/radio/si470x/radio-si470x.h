/*
 *  drivers/media/radio/si470x/radio-si470x.h
 *
 *  Driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 *  Copyright (c) 2009 Tobias Lorenz <tobias.lorenz@gmx.net>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#define DRIVER_NAME "radio-si470x"


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <asm/unaligned.h>



#define RADIO_REGISTER_SIZE	2	
#define RADIO_REGISTER_NUM	16	
#define RDS_REGISTER_NUM	6	

#define DEVICEID		0	
#define DEVICEID_PN		0xf000	
#define DEVICEID_MFGID		0x0fff	

#define CHIPID			1	
#define CHIPID_REV		0xfc00	
#define CHIPID_DEV		0x0200	
#define CHIPID_FIRMWARE		0x01ff	

#define POWERCFG		2	
#define POWERCFG_DSMUTE		0x8000	
#define POWERCFG_DMUTE		0x4000	
#define POWERCFG_MONO		0x2000	
#define POWERCFG_RDSM		0x0800	
#define POWERCFG_SKMODE		0x0400	
#define POWERCFG_SEEKUP		0x0200	
#define POWERCFG_SEEK		0x0100	
#define POWERCFG_DISABLE	0x0040	
#define POWERCFG_ENABLE		0x0001	

#define CHANNEL			3	
#define CHANNEL_TUNE		0x8000	
#define CHANNEL_CHAN		0x03ff	

#define SYSCONFIG1		4	
#define SYSCONFIG1_RDSIEN	0x8000	
#define SYSCONFIG1_STCIEN	0x4000	
#define SYSCONFIG1_RDS		0x1000	
#define SYSCONFIG1_DE		0x0800	
#define SYSCONFIG1_AGCD		0x0400	
#define SYSCONFIG1_BLNDADJ	0x00c0	
#define SYSCONFIG1_GPIO3	0x0030	
#define SYSCONFIG1_GPIO2	0x000c	
#define SYSCONFIG1_GPIO1	0x0003	

#define SYSCONFIG2		5	
#define SYSCONFIG2_SEEKTH	0xff00	
#define SYSCONFIG2_BAND		0x0080	
#define SYSCONFIG2_SPACE	0x0030	
#define SYSCONFIG2_VOLUME	0x000f	

#define SYSCONFIG3		6	
#define SYSCONFIG3_SMUTER	0xc000	
#define SYSCONFIG3_SMUTEA	0x3000	
#define SYSCONFIG3_SKSNR	0x00f0	
#define SYSCONFIG3_SKCNT	0x000f	

#define TEST1			7	
#define TEST1_AHIZEN		0x4000	

#define TEST2			8	

#define BOOTCONFIG		9	

#define STATUSRSSI		10	
#define STATUSRSSI_RDSR		0x8000	
#define STATUSRSSI_STC		0x4000	
#define STATUSRSSI_SF		0x2000	
#define STATUSRSSI_AFCRL	0x1000	
#define STATUSRSSI_RDSS		0x0800	
#define STATUSRSSI_BLERA	0x0600	
#define STATUSRSSI_ST		0x0100	
#define STATUSRSSI_RSSI		0x00ff	

#define READCHAN		11	
#define READCHAN_BLERB		0xc000	
#define READCHAN_BLERC		0x3000	
#define READCHAN_BLERD		0x0c00	
#define READCHAN_READCHAN	0x03ff	

#define RDSA			12	
#define RDSA_RDSA		0xffff	

#define RDSB			13	
#define RDSB_RDSB		0xffff	

#define RDSC			14	
#define RDSC_RDSC		0xffff	

#define RDSD			15	
#define RDSD_RDSD		0xffff	




struct si470x_device {
	struct video_device *videodev;

	
	unsigned int users;

	
	unsigned short registers[RADIO_REGISTER_NUM];

	
	wait_queue_head_t read_queue;
	struct mutex lock;		
	unsigned char *buffer;		
	unsigned int buf_size;
	unsigned int rd_index;
	unsigned int wr_index;

	struct completion completion;
	bool stci_enabled;		

#if defined(CONFIG_USB_SI470X) || defined(CONFIG_USB_SI470X_MODULE)
	
	struct usb_device *usbdev;
	struct usb_interface *intf;

	
	char *int_in_buffer;
	struct usb_endpoint_descriptor *int_in_endpoint;
	struct urb *int_in_urb;
	int int_in_running;

	
	unsigned char software_version;
	unsigned char hardware_version;

	
	unsigned char disconnected;
#endif

#if defined(CONFIG_I2C_SI470X) || defined(CONFIG_I2C_SI470X_MODULE)
	struct i2c_client *client;
#endif
};




#define RADIO_FW_VERSION	15




#define FREQ_MUL (1000000 / 62.5)



extern struct video_device si470x_viddev_template;
int si470x_get_register(struct si470x_device *radio, int regnr);
int si470x_set_register(struct si470x_device *radio, int regnr);
int si470x_disconnect_check(struct si470x_device *radio);
int si470x_set_freq(struct si470x_device *radio, unsigned int freq);
int si470x_start(struct si470x_device *radio);
int si470x_stop(struct si470x_device *radio);
int si470x_fops_open(struct file *file);
int si470x_fops_release(struct file *file);
int si470x_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability);
