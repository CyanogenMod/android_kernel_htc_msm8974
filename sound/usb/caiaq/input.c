/*
 *   Copyright (c) 2006,2007 Daniel Mack, Tim Ruetz
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <sound/core.h>
#include <sound/pcm.h>

#include "device.h"
#include "input.h"

static unsigned short keycode_ak1[] =  { KEY_C, KEY_B, KEY_A };
static unsigned short keycode_rk2[] =  { KEY_1, KEY_2, KEY_3, KEY_4,
					 KEY_5, KEY_6, KEY_7 };
static unsigned short keycode_rk3[] =  { KEY_1, KEY_2, KEY_3, KEY_4,
					 KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };

static unsigned short keycode_kore[] = {
	KEY_FN_F1,      
	KEY_FN_F7,      
	KEY_FN_F2,      
	KEY_FN_F3,      
	KEY_FN_F4,      
	KEY_FN_F5,      
	KEY_FN_F6,      
	KEY_FN_F8,      
	KEY_RIGHT,
	KEY_DOWN,
	KEY_UP,
	KEY_LEFT,
	KEY_SOUND,      
	KEY_RECORD,
	KEY_PLAYPAUSE,
	KEY_STOP,
	BTN_4,          
	BTN_3,
	BTN_2,
	BTN_1,
	BTN_8,
	BTN_7,
	BTN_6,
	BTN_5,
	KEY_BRL_DOT4,   
	KEY_BRL_DOT3,
	KEY_BRL_DOT2,
	KEY_BRL_DOT1,
	KEY_BRL_DOT8,
	KEY_BRL_DOT7,
	KEY_BRL_DOT6,
	KEY_BRL_DOT5
};

#define MASCHINE_BUTTONS   (42)
#define MASCHINE_BUTTON(X) ((X) + BTN_MISC)
#define MASCHINE_PADS      (16)
#define MASCHINE_PAD(X)    ((X) + ABS_PRESSURE)

static unsigned short keycode_maschine[] = {
	MASCHINE_BUTTON(40), 
	MASCHINE_BUTTON(39), 
	MASCHINE_BUTTON(38), 
	MASCHINE_BUTTON(37), 
	MASCHINE_BUTTON(36), 
	MASCHINE_BUTTON(35), 
	MASCHINE_BUTTON(34), 
	MASCHINE_BUTTON(33), 
	KEY_RESERVED, 

	MASCHINE_BUTTON(30), 
	MASCHINE_BUTTON(31), 
	MASCHINE_BUTTON(32), 
	MASCHINE_BUTTON(28), 
	MASCHINE_BUTTON(27), 
	MASCHINE_BUTTON(26), 
	MASCHINE_BUTTON(25), 

	MASCHINE_BUTTON(21), 
	MASCHINE_BUTTON(22), 
	MASCHINE_BUTTON(23), 
	MASCHINE_BUTTON(24), 
	MASCHINE_BUTTON(20), 
	MASCHINE_BUTTON(19), 
	MASCHINE_BUTTON(18), 
	MASCHINE_BUTTON(17), 

	MASCHINE_BUTTON(0),  
	MASCHINE_BUTTON(2),  
	MASCHINE_BUTTON(4),  
	MASCHINE_BUTTON(6),  
	MASCHINE_BUTTON(7),  
	MASCHINE_BUTTON(5),  
	MASCHINE_BUTTON(3),  
	MASCHINE_BUTTON(1),  

	MASCHINE_BUTTON(15), 
	MASCHINE_BUTTON(14),
	MASCHINE_BUTTON(13),
	MASCHINE_BUTTON(12),
	MASCHINE_BUTTON(11),
	MASCHINE_BUTTON(10),
	MASCHINE_BUTTON(9),
	MASCHINE_BUTTON(8),

	MASCHINE_BUTTON(16), 
	MASCHINE_BUTTON(29)  
};

#define KONTROLX1_INPUTS	(40)
#define KONTROLS4_BUTTONS	(12 * 8)
#define KONTROLS4_AXIS		(46)

#define KONTROLS4_BUTTON(X)	((X) + BTN_MISC)
#define KONTROLS4_ABS(X)	((X) + ABS_HAT0X)

#define DEG90		(range / 2)
#define DEG180		(range)
#define DEG270		(DEG90 + DEG180)
#define DEG360		(DEG180 * 2)
#define HIGH_PEAK	(268)
#define LOW_PEAK	(-7)

static unsigned int decode_erp(unsigned char a, unsigned char b)
{
	int weight_a, weight_b;
	int pos_a, pos_b;
	int ret;
	int range = HIGH_PEAK - LOW_PEAK;
	int mid_value = (HIGH_PEAK + LOW_PEAK) / 2;

	weight_b = abs(mid_value - a) - (range / 2 - 100) / 2;

	if (weight_b < 0)
		weight_b = 0;

	if (weight_b > 100)
		weight_b = 100;

	weight_a = 100 - weight_b;

	if (a < mid_value) {
		
		pos_b = b - LOW_PEAK + DEG270;
		if (pos_b >= DEG360)
			pos_b -= DEG360;
	} else
		
		pos_b = HIGH_PEAK - b + DEG90;


	if (b > mid_value)
		
		pos_a = a - LOW_PEAK;
	else
		
		pos_a = HIGH_PEAK - a + DEG180;

	
	
	ret = pos_a * weight_a + pos_b * weight_b;

	
	ret *= 10;
	ret /= DEG360;

	if (ret < 0)
		ret += 1000;

	if (ret >= 1000)
		ret -= 1000;

	return ret;
}

#undef DEG90
#undef DEG180
#undef DEG270
#undef DEG360
#undef HIGH_PEAK
#undef LOW_PEAK

static inline void snd_caiaq_input_report_abs(struct snd_usb_caiaqdev *dev,
					      int axis, const unsigned char *buf,
					      int offset)
{
	input_report_abs(dev->input_dev, axis,
			 (buf[offset * 2] << 8) | buf[offset * 2 + 1]);
}

static void snd_caiaq_input_read_analog(struct snd_usb_caiaqdev *dev,
					const unsigned char *buf,
					unsigned int len)
{
	struct input_dev *input_dev = dev->input_dev;

	switch (dev->chip.usb_id) {
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_RIGKONTROL2):
		snd_caiaq_input_report_abs(dev, ABS_X, buf, 2);
		snd_caiaq_input_report_abs(dev, ABS_Y, buf, 0);
		snd_caiaq_input_report_abs(dev, ABS_Z, buf, 1);
		break;
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_RIGKONTROL3):
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_KORECONTROLLER):
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_KORECONTROLLER2):
		snd_caiaq_input_report_abs(dev, ABS_X, buf, 0);
		snd_caiaq_input_report_abs(dev, ABS_Y, buf, 1);
		snd_caiaq_input_report_abs(dev, ABS_Z, buf, 2);
		break;
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLX1):
		snd_caiaq_input_report_abs(dev, ABS_HAT0X, buf, 4);
		snd_caiaq_input_report_abs(dev, ABS_HAT0Y, buf, 2);
		snd_caiaq_input_report_abs(dev, ABS_HAT1X, buf, 6);
		snd_caiaq_input_report_abs(dev, ABS_HAT1Y, buf, 1);
		snd_caiaq_input_report_abs(dev, ABS_HAT2X, buf, 7);
		snd_caiaq_input_report_abs(dev, ABS_HAT2Y, buf, 0);
		snd_caiaq_input_report_abs(dev, ABS_HAT3X, buf, 5);
		snd_caiaq_input_report_abs(dev, ABS_HAT3Y, buf, 3);
		break;
	}

	input_sync(input_dev);
}

static void snd_caiaq_input_read_erp(struct snd_usb_caiaqdev *dev,
				     const char *buf, unsigned int len)
{
	struct input_dev *input_dev = dev->input_dev;
	int i;

	switch (dev->chip.usb_id) {
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_AK1):
		i = decode_erp(buf[0], buf[1]);
		input_report_abs(input_dev, ABS_X, i);
		input_sync(input_dev);
		break;
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_KORECONTROLLER):
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_KORECONTROLLER2):
		i = decode_erp(buf[7], buf[5]);
		input_report_abs(input_dev, ABS_HAT0X, i);
		i = decode_erp(buf[12], buf[14]);
		input_report_abs(input_dev, ABS_HAT0Y, i);
		i = decode_erp(buf[15], buf[13]);
		input_report_abs(input_dev, ABS_HAT1X, i);
		i = decode_erp(buf[0], buf[2]);
		input_report_abs(input_dev, ABS_HAT1Y, i);
		i = decode_erp(buf[3], buf[1]);
		input_report_abs(input_dev, ABS_HAT2X, i);
		i = decode_erp(buf[8], buf[10]);
		input_report_abs(input_dev, ABS_HAT2Y, i);
		i = decode_erp(buf[11], buf[9]);
		input_report_abs(input_dev, ABS_HAT3X, i);
		i = decode_erp(buf[4], buf[6]);
		input_report_abs(input_dev, ABS_HAT3Y, i);
		input_sync(input_dev);
		break;

	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_MASCHINECONTROLLER):
		
		input_report_abs(input_dev, ABS_HAT0X, decode_erp(buf[21], buf[20]));
		input_report_abs(input_dev, ABS_HAT0Y, decode_erp(buf[15], buf[14]));
		input_report_abs(input_dev, ABS_HAT1X, decode_erp(buf[9],  buf[8]));
		input_report_abs(input_dev, ABS_HAT1Y, decode_erp(buf[3],  buf[2]));

		
		input_report_abs(input_dev, ABS_HAT2X, decode_erp(buf[19], buf[18]));
		input_report_abs(input_dev, ABS_HAT2Y, decode_erp(buf[13], buf[12]));
		input_report_abs(input_dev, ABS_HAT3X, decode_erp(buf[7],  buf[6]));
		input_report_abs(input_dev, ABS_HAT3Y, decode_erp(buf[1],  buf[0]));

		
		input_report_abs(input_dev, ABS_RX, decode_erp(buf[17], buf[16]));
		
		input_report_abs(input_dev, ABS_RY, decode_erp(buf[11], buf[10]));
		
		input_report_abs(input_dev, ABS_RZ, decode_erp(buf[5],  buf[4]));

		input_sync(input_dev);
		break;
	}
}

static void snd_caiaq_input_read_io(struct snd_usb_caiaqdev *dev,
				    unsigned char *buf, unsigned int len)
{
	struct input_dev *input_dev = dev->input_dev;
	unsigned short *keycode = input_dev->keycode;
	int i;

	if (!keycode)
		return;

	if (input_dev->id.product == USB_PID_RIGKONTROL2)
		for (i = 0; i < len; i++)
			buf[i] = ~buf[i];

	for (i = 0; i < input_dev->keycodemax && i < len * 8; i++)
		input_report_key(input_dev, keycode[i],
				 buf[i / 8] & (1 << (i % 8)));

	switch (dev->chip.usb_id) {
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_KORECONTROLLER):
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_KORECONTROLLER2):
		input_report_abs(dev->input_dev, ABS_MISC, 255 - buf[4]);
		break;
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLX1):
		
		input_report_abs(dev->input_dev, ABS_X, buf[5] & 0xf);
		input_report_abs(dev->input_dev, ABS_Y, buf[5] >> 4);
		input_report_abs(dev->input_dev, ABS_Z, buf[6] & 0xf);
		input_report_abs(dev->input_dev, ABS_MISC, buf[6] >> 4);
		break;
	}

	input_sync(input_dev);
}

#define TKS4_MSGBLOCK_SIZE	16

static void snd_usb_caiaq_tks4_dispatch(struct snd_usb_caiaqdev *dev,
					const unsigned char *buf,
					unsigned int len)
{
	while (len) {
		unsigned int i, block_id = (buf[0] << 8) | buf[1];

		switch (block_id) {
		case 0:
			
			for (i = 0; i < KONTROLS4_BUTTONS; i++)
				input_report_key(dev->input_dev, KONTROLS4_BUTTON(i),
						 (buf[4 + (i / 8)] >> (i % 8)) & 1);
			break;

		case 1:
			
			input_report_abs(dev->input_dev, KONTROLS4_ABS(36), buf[9] | ((buf[8] & 0x3) << 8));
			
			input_report_abs(dev->input_dev, KONTROLS4_ABS(37), buf[13] | ((buf[12] & 0x3) << 8));

			
			input_report_abs(dev->input_dev, KONTROLS4_ABS(38), buf[3] & 0xf);
			input_report_abs(dev->input_dev, KONTROLS4_ABS(39), buf[4] >> 4);
			input_report_abs(dev->input_dev, KONTROLS4_ABS(40), buf[4] & 0xf);
			input_report_abs(dev->input_dev, KONTROLS4_ABS(41), buf[5] >> 4);
			input_report_abs(dev->input_dev, KONTROLS4_ABS(42), buf[5] & 0xf);
			input_report_abs(dev->input_dev, KONTROLS4_ABS(43), buf[6] >> 4);
			input_report_abs(dev->input_dev, KONTROLS4_ABS(44), buf[6] & 0xf);
			input_report_abs(dev->input_dev, KONTROLS4_ABS(45), buf[7] >> 4);
			input_report_abs(dev->input_dev, KONTROLS4_ABS(46), buf[7] & 0xf);

			break;
		case 2:
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(0), buf, 1);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(1), buf, 2);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(2), buf, 3);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(3), buf, 4);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(4), buf, 6);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(7), buf, 7);

			break;

		case 3:
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(6), buf, 3);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(5), buf, 4);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(8), buf, 6);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(9), buf, 7);

			break;

		case 4:
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(10), buf, 1);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(11), buf, 2);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(12), buf, 3);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(13), buf, 4);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(14), buf, 5);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(15), buf, 6);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(16), buf, 7);

			break;

		case 5:
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(17), buf, 1);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(18), buf, 2);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(19), buf, 3);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(20), buf, 4);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(21), buf, 5);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(22), buf, 6);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(23), buf, 7);

			break;

		case 6:
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(24), buf, 1);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(25), buf, 2);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(26), buf, 3);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(27), buf, 4);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(28), buf, 5);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(29), buf, 6);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(30), buf, 7);

			break;

		case 7:
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(31), buf, 1);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(32), buf, 2);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(33), buf, 3);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(34), buf, 4);
			
			snd_caiaq_input_report_abs(dev, KONTROLS4_ABS(35), buf, 5);

			break;

		default:
			debug("%s(): bogus block (id %d)\n",
				__func__, block_id);
			return;
		}

		len -= TKS4_MSGBLOCK_SIZE;
		buf += TKS4_MSGBLOCK_SIZE;
	}

	input_sync(dev->input_dev);
}

#define MASCHINE_MSGBLOCK_SIZE 2

static void snd_usb_caiaq_maschine_dispatch(struct snd_usb_caiaqdev *dev,
					const unsigned char *buf,
					unsigned int len)
{
	unsigned int i, pad_id;
	uint16_t pressure;

	for (i = 0; i < MASCHINE_PADS; i++) {
		pressure = be16_to_cpu(buf[i * 2] << 8 | buf[(i * 2) + 1]);
		pad_id = pressure >> 12;

		input_report_abs(dev->input_dev, MASCHINE_PAD(pad_id), pressure & 0xfff);
	}

	input_sync(dev->input_dev);
}

static void snd_usb_caiaq_ep4_reply_dispatch(struct urb *urb)
{
	struct snd_usb_caiaqdev *dev = urb->context;
	unsigned char *buf = urb->transfer_buffer;
	int ret;

	if (urb->status || !dev || urb != dev->ep4_in_urb)
		return;

	switch (dev->chip.usb_id) {
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLX1):
		if (urb->actual_length < 24)
			goto requeue;

		if (buf[0] & 0x3)
			snd_caiaq_input_read_io(dev, buf + 1, 7);

		if (buf[0] & 0x4)
			snd_caiaq_input_read_analog(dev, buf + 8, 16);

		break;

	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLS4):
		snd_usb_caiaq_tks4_dispatch(dev, buf, urb->actual_length);
		break;

	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_MASCHINECONTROLLER):
		if (urb->actual_length < (MASCHINE_PADS * MASCHINE_MSGBLOCK_SIZE))
			goto requeue;

		snd_usb_caiaq_maschine_dispatch(dev, buf, urb->actual_length);
		break;
	}

requeue:
	dev->ep4_in_urb->actual_length = 0;
	ret = usb_submit_urb(dev->ep4_in_urb, GFP_ATOMIC);
	if (ret < 0)
		log("unable to submit urb. OOM!?\n");
}

static int snd_usb_caiaq_input_open(struct input_dev *idev)
{
	struct snd_usb_caiaqdev *dev = input_get_drvdata(idev);

	if (!dev)
		return -EINVAL;

	switch (dev->chip.usb_id) {
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLX1):
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLS4):
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_MASCHINECONTROLLER):
		if (usb_submit_urb(dev->ep4_in_urb, GFP_KERNEL) != 0)
			return -EIO;
		break;
	}

	return 0;
}

static void snd_usb_caiaq_input_close(struct input_dev *idev)
{
	struct snd_usb_caiaqdev *dev = input_get_drvdata(idev);

	if (!dev)
		return;

	switch (dev->chip.usb_id) {
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLX1):
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLS4):
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_MASCHINECONTROLLER):
		usb_kill_urb(dev->ep4_in_urb);
		break;
	}
}

void snd_usb_caiaq_input_dispatch(struct snd_usb_caiaqdev *dev,
				  char *buf,
				  unsigned int len)
{
	if (!dev->input_dev || len < 1)
		return;

	switch (buf[0]) {
	case EP1_CMD_READ_ANALOG:
		snd_caiaq_input_read_analog(dev, buf + 1, len - 1);
		break;
	case EP1_CMD_READ_ERP:
		snd_caiaq_input_read_erp(dev, buf + 1, len - 1);
		break;
	case EP1_CMD_READ_IO:
		snd_caiaq_input_read_io(dev, buf + 1, len - 1);
		break;
	}
}

int snd_usb_caiaq_input_init(struct snd_usb_caiaqdev *dev)
{
	struct usb_device *usb_dev = dev->chip.dev;
	struct input_dev *input;
	int i, ret = 0;

	input = input_allocate_device();
	if (!input)
		return -ENOMEM;

	usb_make_path(usb_dev, dev->phys, sizeof(dev->phys));
	strlcat(dev->phys, "/input0", sizeof(dev->phys));

	input->name = dev->product_name;
	input->phys = dev->phys;
	usb_to_input_id(usb_dev, &input->id);
	input->dev.parent = &usb_dev->dev;

	input_set_drvdata(input, dev);

	switch (dev->chip.usb_id) {
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_RIGKONTROL2):
		input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		input->absbit[0] = BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) |
			BIT_MASK(ABS_Z);
		BUILD_BUG_ON(sizeof(dev->keycode) < sizeof(keycode_rk2));
		memcpy(dev->keycode, keycode_rk2, sizeof(keycode_rk2));
		input->keycodemax = ARRAY_SIZE(keycode_rk2);
		input_set_abs_params(input, ABS_X, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_Y, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_Z, 0, 4096, 0, 10);
		snd_usb_caiaq_set_auto_msg(dev, 1, 10, 0);
		break;
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_RIGKONTROL3):
		input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		input->absbit[0] = BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) |
			BIT_MASK(ABS_Z);
		BUILD_BUG_ON(sizeof(dev->keycode) < sizeof(keycode_rk3));
		memcpy(dev->keycode, keycode_rk3, sizeof(keycode_rk3));
		input->keycodemax = ARRAY_SIZE(keycode_rk3);
		input_set_abs_params(input, ABS_X, 0, 1024, 0, 10);
		input_set_abs_params(input, ABS_Y, 0, 1024, 0, 10);
		input_set_abs_params(input, ABS_Z, 0, 1024, 0, 10);
		snd_usb_caiaq_set_auto_msg(dev, 1, 10, 0);
		break;
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_AK1):
		input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		input->absbit[0] = BIT_MASK(ABS_X);
		BUILD_BUG_ON(sizeof(dev->keycode) < sizeof(keycode_ak1));
		memcpy(dev->keycode, keycode_ak1, sizeof(keycode_ak1));
		input->keycodemax = ARRAY_SIZE(keycode_ak1);
		input_set_abs_params(input, ABS_X, 0, 999, 0, 10);
		snd_usb_caiaq_set_auto_msg(dev, 1, 0, 5);
		break;
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_KORECONTROLLER):
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_KORECONTROLLER2):
		input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		input->absbit[0] = BIT_MASK(ABS_HAT0X) | BIT_MASK(ABS_HAT0Y) |
				   BIT_MASK(ABS_HAT1X) | BIT_MASK(ABS_HAT1Y) |
				   BIT_MASK(ABS_HAT2X) | BIT_MASK(ABS_HAT2Y) |
				   BIT_MASK(ABS_HAT3X) | BIT_MASK(ABS_HAT3Y) |
				   BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) |
				   BIT_MASK(ABS_Z);
		input->absbit[BIT_WORD(ABS_MISC)] |= BIT_MASK(ABS_MISC);
		BUILD_BUG_ON(sizeof(dev->keycode) < sizeof(keycode_kore));
		memcpy(dev->keycode, keycode_kore, sizeof(keycode_kore));
		input->keycodemax = ARRAY_SIZE(keycode_kore);
		input_set_abs_params(input, ABS_HAT0X, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT0Y, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT1X, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT1Y, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT2X, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT2Y, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT3X, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT3Y, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_X, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_Y, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_Z, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_MISC, 0, 255, 0, 1);
		snd_usb_caiaq_set_auto_msg(dev, 1, 10, 5);
		break;
	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLX1):
		input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		input->absbit[0] = BIT_MASK(ABS_HAT0X) | BIT_MASK(ABS_HAT0Y) |
				   BIT_MASK(ABS_HAT1X) | BIT_MASK(ABS_HAT1Y) |
				   BIT_MASK(ABS_HAT2X) | BIT_MASK(ABS_HAT2Y) |
				   BIT_MASK(ABS_HAT3X) | BIT_MASK(ABS_HAT3Y) |
				   BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) |
				   BIT_MASK(ABS_Z);
		input->absbit[BIT_WORD(ABS_MISC)] |= BIT_MASK(ABS_MISC);
		BUILD_BUG_ON(sizeof(dev->keycode) < KONTROLX1_INPUTS);
		for (i = 0; i < KONTROLX1_INPUTS; i++)
			dev->keycode[i] = BTN_MISC + i;
		input->keycodemax = KONTROLX1_INPUTS;

		
		input_set_abs_params(input, ABS_HAT0X, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_HAT0Y, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_HAT1X, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_HAT1Y, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_HAT2X, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_HAT2Y, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_HAT3X, 0, 4096, 0, 10);
		input_set_abs_params(input, ABS_HAT3Y, 0, 4096, 0, 10);

		
		input_set_abs_params(input, ABS_X, 0, 0xf, 0, 1);
		input_set_abs_params(input, ABS_Y, 0, 0xf, 0, 1);
		input_set_abs_params(input, ABS_Z, 0, 0xf, 0, 1);
		input_set_abs_params(input, ABS_MISC, 0, 0xf, 0, 1);

		dev->ep4_in_urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!dev->ep4_in_urb) {
			ret = -ENOMEM;
			goto exit_free_idev;
		}

		usb_fill_bulk_urb(dev->ep4_in_urb, usb_dev,
				  usb_rcvbulkpipe(usb_dev, 0x4),
				  dev->ep4_in_buf, EP4_BUFSIZE,
				  snd_usb_caiaq_ep4_reply_dispatch, dev);

		snd_usb_caiaq_set_auto_msg(dev, 1, 10, 5);

		break;

	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_TRAKTORKONTROLS4):
		input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		BUILD_BUG_ON(sizeof(dev->keycode) < KONTROLS4_BUTTONS);
		for (i = 0; i < KONTROLS4_BUTTONS; i++)
			dev->keycode[i] = KONTROLS4_BUTTON(i);
		input->keycodemax = KONTROLS4_BUTTONS;

		for (i = 0; i < KONTROLS4_AXIS; i++) {
			int axis = KONTROLS4_ABS(i);
			input->absbit[BIT_WORD(axis)] |= BIT_MASK(axis);
		}

		
		for (i = 0; i < 36; i++)
			input_set_abs_params(input, KONTROLS4_ABS(i), 0, 0xfff, 0, 10);

		
		input_set_abs_params(input, KONTROLS4_ABS(36), 0, 0x3ff, 0, 1);
		input_set_abs_params(input, KONTROLS4_ABS(37), 0, 0x3ff, 0, 1);

		
		for (i = 0; i < 9; i++)
			input_set_abs_params(input, KONTROLS4_ABS(38+i), 0, 0xf, 0, 1);

		dev->ep4_in_urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!dev->ep4_in_urb) {
			ret = -ENOMEM;
			goto exit_free_idev;
		}

		usb_fill_bulk_urb(dev->ep4_in_urb, usb_dev,
				  usb_rcvbulkpipe(usb_dev, 0x4),
				  dev->ep4_in_buf, EP4_BUFSIZE,
				  snd_usb_caiaq_ep4_reply_dispatch, dev);

		snd_usb_caiaq_set_auto_msg(dev, 1, 10, 5);

		break;

	case USB_ID(USB_VID_NATIVEINSTRUMENTS, USB_PID_MASCHINECONTROLLER):
		input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		input->absbit[0] = BIT_MASK(ABS_HAT0X) | BIT_MASK(ABS_HAT0Y) |
			BIT_MASK(ABS_HAT1X) | BIT_MASK(ABS_HAT1Y) |
			BIT_MASK(ABS_HAT2X) | BIT_MASK(ABS_HAT2Y) |
			BIT_MASK(ABS_HAT3X) | BIT_MASK(ABS_HAT3Y) |
			BIT_MASK(ABS_RX) | BIT_MASK(ABS_RY) |
			BIT_MASK(ABS_RZ);

		BUILD_BUG_ON(sizeof(dev->keycode) < sizeof(keycode_maschine));
		memcpy(dev->keycode, keycode_maschine, sizeof(keycode_maschine));
		input->keycodemax = ARRAY_SIZE(keycode_maschine);

		for (i = 0; i < MASCHINE_PADS; i++) {
			input->absbit[0] |= MASCHINE_PAD(i);
			input_set_abs_params(input, MASCHINE_PAD(i), 0, 0xfff, 5, 10);
		}

		input_set_abs_params(input, ABS_HAT0X, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT0Y, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT1X, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT1Y, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT2X, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT2Y, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT3X, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_HAT3Y, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_RX, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_RY, 0, 999, 0, 10);
		input_set_abs_params(input, ABS_RZ, 0, 999, 0, 10);

		dev->ep4_in_urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!dev->ep4_in_urb) {
			ret = -ENOMEM;
			goto exit_free_idev;
		}

		usb_fill_bulk_urb(dev->ep4_in_urb, usb_dev,
				  usb_rcvbulkpipe(usb_dev, 0x4),
				  dev->ep4_in_buf, EP4_BUFSIZE,
				  snd_usb_caiaq_ep4_reply_dispatch, dev);

		snd_usb_caiaq_set_auto_msg(dev, 1, 10, 5);
		break;

	default:
		
		goto exit_free_idev;
	}

	input->open = snd_usb_caiaq_input_open;
	input->close = snd_usb_caiaq_input_close;
	input->keycode = dev->keycode;
	input->keycodesize = sizeof(unsigned short);
	for (i = 0; i < input->keycodemax; i++)
		__set_bit(dev->keycode[i], input->keybit);

	dev->input_dev = input;

	ret = input_register_device(input);
	if (ret < 0)
		goto exit_free_idev;

	return 0;

exit_free_idev:
	input_free_device(input);
	dev->input_dev = NULL;
	return ret;
}

void snd_usb_caiaq_input_free(struct snd_usb_caiaqdev *dev)
{
	if (!dev || !dev->input_dev)
		return;

	usb_kill_urb(dev->ep4_in_urb);
	usb_free_urb(dev->ep4_in_urb);
	dev->ep4_in_urb = NULL;

	input_unregister_device(dev->input_dev);
	dev->input_dev = NULL;
}
