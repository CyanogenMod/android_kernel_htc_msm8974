/*
 * Handlers for board audio hooks, splitted from bttv-cards
 *
 * Copyright (c) 2006 Mauro Carvalho Chehab (mchehab@infradead.org)
 * This code is placed under the terms of the GNU General Public License
 */

#include "bttv-audio-hook.h"

#include <linux/delay.h>


void winview_volume(struct bttv *btv, __u16 volume)
{
	
	int bits_out, loops, vol, data;

	
	vol = 32 - ((volume>>11));
	
	bits_out = (PT2254_DBS_IN_2>>(vol%5));
	
	bits_out |= (PT2254_DBS_IN_10>>(vol/5));
	bits_out |= PT2254_L_CHANNEL | PT2254_R_CHANNEL;
	data = gpio_read();
	data &= ~(WINVIEW_PT2254_CLK| WINVIEW_PT2254_DATA|
		  WINVIEW_PT2254_STROBE);
	for (loops = 17; loops >= 0 ; loops--) {
		if (bits_out & (1<<loops))
			data |=  WINVIEW_PT2254_DATA;
		else
			data &= ~WINVIEW_PT2254_DATA;
		gpio_write(data);
		udelay(5);
		data |= WINVIEW_PT2254_CLK;
		gpio_write(data);
		udelay(5);
		data &= ~WINVIEW_PT2254_CLK;
		gpio_write(data);
	}
	data |=  WINVIEW_PT2254_STROBE;
	data &= ~WINVIEW_PT2254_DATA;
	gpio_write(data);
	udelay(10);
	data &= ~WINVIEW_PT2254_STROBE;
	gpio_write(data);
}


void gvbctv3pci_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	unsigned int con = 0;

	if (set) {
		gpio_inout(0x300, 0x300);
		if (t->audmode & V4L2_TUNER_MODE_LANG1)
			con = 0x000;
		if (t->audmode & V4L2_TUNER_MODE_LANG2)
			con = 0x300;
		if (t->audmode & V4L2_TUNER_MODE_STEREO)
			con = 0x200;
		gpio_bits(0x300, con);
	} else {
		t->audmode = V4L2_TUNER_MODE_STEREO |
			  V4L2_TUNER_MODE_LANG1  | V4L2_TUNER_MODE_LANG2;
	}
}

void gvbctv5pci_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	unsigned int val, con;

	if (btv->radio_user)
		return;

	val = gpio_read();
	if (set) {
		con = 0x000;
		if (t->audmode & V4L2_TUNER_MODE_LANG2) {
			if (t->audmode & V4L2_TUNER_MODE_LANG1) {
				
				con = 0x100;
			}
			else {
				
				con = 0x300;
			}
		}
		if (con != (val & 0x300)) {
			gpio_bits(0x300, con);
			if (bttv_gpio)
				bttv_gpio_tracking(btv,"gvbctv5pci");
		}
	} else {
		switch (val & 0x70) {
		  case 0x10:
			t->rxsubchans = V4L2_TUNER_SUB_LANG1 |  V4L2_TUNER_SUB_LANG2;
			break;
		  case 0x30:
			t->rxsubchans = V4L2_TUNER_SUB_LANG2;
			break;
		  case 0x50:
			t->rxsubchans = V4L2_TUNER_SUB_LANG1;
			break;
		  case 0x60:
			t->rxsubchans = V4L2_TUNER_SUB_STEREO;
			break;
		  case 0x70:
			t->rxsubchans = V4L2_TUNER_SUB_MONO;
			break;
		  default:
			t->rxsubchans = V4L2_TUNER_SUB_MONO |
					 V4L2_TUNER_SUB_STEREO |
					 V4L2_TUNER_SUB_LANG1 |
					 V4L2_TUNER_SUB_LANG2;
		}
		t->audmode = V4L2_TUNER_MODE_STEREO |
			  V4L2_TUNER_MODE_LANG1  | V4L2_TUNER_MODE_LANG2;
	}
}


void avermedia_tvphone_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	int val = 0;

	if (set) {
		if (t->audmode & V4L2_TUNER_MODE_LANG2)   
			val = 0x02;
		if (t->audmode & V4L2_TUNER_MODE_STEREO)
			val = 0x01;
		if (val) {
			gpio_bits(0x03,val);
			if (bttv_gpio)
				bttv_gpio_tracking(btv,"avermedia");
		}
	} else {
		t->audmode = V4L2_TUNER_MODE_MONO | V4L2_TUNER_MODE_STEREO |
			V4L2_TUNER_MODE_LANG1;
		return;
	}
}


void avermedia_tv_stereo_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	int val = 0;

	if (set) {
		if (t->audmode & V4L2_TUNER_MODE_LANG2)   
			val = 0x01;
		if (t->audmode & V4L2_TUNER_MODE_STEREO)  
			val = 0x02;
		btaor(val, ~0x03, BT848_GPIO_DATA);
		if (bttv_gpio)
			bttv_gpio_tracking(btv,"avermedia");
	} else {
		t->audmode = V4L2_TUNER_MODE_MONO | V4L2_TUNER_MODE_STEREO |
			V4L2_TUNER_MODE_LANG1 | V4L2_TUNER_MODE_LANG2;
		return;
	}
}


void lt9415_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	int val = 0;

	if (gpio_read() & 0x4000) {
		t->audmode = V4L2_TUNER_MODE_MONO;
		return;
	}

	if (set) {
		if (t->audmode & V4L2_TUNER_MODE_LANG2)  
			val = 0x0080;
		if (t->audmode & V4L2_TUNER_MODE_STEREO) 
			val = 0x0880;
		if ((t->audmode & V4L2_TUNER_MODE_LANG1) ||
		    (t->audmode & V4L2_TUNER_MODE_MONO))
			val = 0;
		gpio_bits(0x0880, val);
		if (bttv_gpio)
			bttv_gpio_tracking(btv,"lt9415");
	} else {
		
		t->audmode = V4L2_TUNER_MODE_MONO | V4L2_TUNER_MODE_STEREO |
			V4L2_TUNER_MODE_LANG1 | V4L2_TUNER_MODE_LANG2;
		return;
	}
}

void terratv_audio(struct bttv *btv,  struct v4l2_tuner *t, int set)
{
	unsigned int con = 0;

	if (set) {
		gpio_inout(0x180000,0x180000);
		if (t->audmode & V4L2_TUNER_MODE_LANG2)
			con = 0x080000;
		if (t->audmode & V4L2_TUNER_MODE_STEREO)
			con = 0x180000;
		gpio_bits(0x180000, con);
		if (bttv_gpio)
			bttv_gpio_tracking(btv,"terratv");
	} else {
		t->audmode = V4L2_TUNER_MODE_MONO | V4L2_TUNER_MODE_STEREO |
			V4L2_TUNER_MODE_LANG1 | V4L2_TUNER_MODE_LANG2;
	}
}


void winfast2000_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	unsigned long val = 0;

	if (set) {
		
		if (t->audmode & V4L2_TUNER_MODE_MONO)		
			val = 0x420000;
		if (t->audmode & V4L2_TUNER_MODE_LANG1)	
			val = 0x420000;
		if (t->audmode & V4L2_TUNER_MODE_LANG2)	
			val = 0x410000;
		if (t->audmode & V4L2_TUNER_MODE_STEREO)	
			val = 0x020000;
		if (val) {
			gpio_bits(0x430000, val);
			if (bttv_gpio)
				bttv_gpio_tracking(btv,"winfast2000");
		}
	} else {
		t->audmode = V4L2_TUNER_MODE_MONO | V4L2_TUNER_MODE_STEREO |
			  V4L2_TUNER_MODE_LANG1 | V4L2_TUNER_MODE_LANG2;
	}
}

void pvbt878p9b_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	unsigned int val = 0;

	if (btv->radio_user)
		return;

	if (set) {
		if (t->audmode & V4L2_TUNER_MODE_MONO)	{
			val = 0x01;
		}
		if ((t->audmode & (V4L2_TUNER_MODE_LANG1 | V4L2_TUNER_MODE_LANG2))
		    || (t->audmode & V4L2_TUNER_MODE_STEREO)) {
			val = 0x02;
		}
		if (val) {
			gpio_bits(0x03,val);
			if (bttv_gpio)
				bttv_gpio_tracking(btv,"pvbt878p9b");
		}
	} else {
		t->audmode = V4L2_TUNER_MODE_MONO | V4L2_TUNER_MODE_STEREO |
			V4L2_TUNER_MODE_LANG1 | V4L2_TUNER_MODE_LANG2;
	}
}

void fv2000s_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	unsigned int val = 0xffff;

	if (btv->radio_user)
		return;

	if (set) {
		if (t->audmode & V4L2_TUNER_MODE_MONO)	{
			val = 0x0000;
		}
		if ((t->audmode & (V4L2_TUNER_MODE_LANG1 | V4L2_TUNER_MODE_LANG2))
		    || (t->audmode & V4L2_TUNER_MODE_STEREO)) {
			val = 0x1080; 
		}
		if (val != 0xffff) {
			gpio_bits(0x1800, val);
			if (bttv_gpio)
				bttv_gpio_tracking(btv,"fv2000s");
		}
	} else {
		t->audmode = V4L2_TUNER_MODE_MONO | V4L2_TUNER_MODE_STEREO |
			V4L2_TUNER_MODE_LANG1 | V4L2_TUNER_MODE_LANG2;
	}
}

void windvr_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	unsigned long val = 0;

	if (set) {
		if (t->audmode & V4L2_TUNER_MODE_MONO)
			val = 0x040000;
		if (t->audmode & V4L2_TUNER_MODE_LANG1)
			val = 0;
		if (t->audmode & V4L2_TUNER_MODE_LANG2)
			val = 0x100000;
		if (t->audmode & V4L2_TUNER_MODE_STEREO)
			val = 0;
		if (val) {
			gpio_bits(0x140000, val);
			if (bttv_gpio)
				bttv_gpio_tracking(btv,"windvr");
		}
	} else {
		t->audmode = V4L2_TUNER_MODE_MONO | V4L2_TUNER_MODE_STEREO |
			  V4L2_TUNER_MODE_LANG1 | V4L2_TUNER_MODE_LANG2;
	}
}

void adtvk503_audio(struct bttv *btv, struct v4l2_tuner *t, int set)
{
	unsigned int con = 0xffffff;

	

	if (set) {
		
		if (t->audmode & V4L2_TUNER_MODE_LANG1)
			con = 0x00000000;
		if (t->audmode & V4L2_TUNER_MODE_LANG2)
			con = 0x00180000;
		if (t->audmode & V4L2_TUNER_MODE_STEREO)
			con = 0x00000000;
		if (t->audmode & V4L2_TUNER_MODE_MONO)
			con = 0x00060000;
		if (con != 0xffffff) {
			gpio_bits(0x1e0000,con);
			if (bttv_gpio)
				bttv_gpio_tracking(btv, "adtvk503");
		}
	} else {
		t->audmode = V4L2_TUNER_MODE_MONO | V4L2_TUNER_MODE_STEREO |
			  V4L2_TUNER_MODE_LANG1  | V4L2_TUNER_MODE_LANG2;
	}
}
