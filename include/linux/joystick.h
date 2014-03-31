#ifndef _LINUX_JOYSTICK_H
#define _LINUX_JOYSTICK_H

/*
 *  Copyright (C) 1996-2000 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
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
 * 
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <linux/types.h>
#include <linux/input.h>


#define JS_VERSION		0x020100


#define JS_EVENT_BUTTON		0x01	
#define JS_EVENT_AXIS		0x02	
#define JS_EVENT_INIT		0x80	

struct js_event {
	__u32 time;	
	__s16 value;	
	__u8 type;	
	__u8 number;	
};


#define JSIOCGVERSION		_IOR('j', 0x01, __u32)				

#define JSIOCGAXES		_IOR('j', 0x11, __u8)				
#define JSIOCGBUTTONS		_IOR('j', 0x12, __u8)				
#define JSIOCGNAME(len)		_IOC(_IOC_READ, 'j', 0x13, len)			

#define JSIOCSCORR		_IOW('j', 0x21, struct js_corr)			
#define JSIOCGCORR		_IOR('j', 0x22, struct js_corr)			

#define JSIOCSAXMAP		_IOW('j', 0x31, __u8[ABS_CNT])			
#define JSIOCGAXMAP		_IOR('j', 0x32, __u8[ABS_CNT])			
#define JSIOCSBTNMAP		_IOW('j', 0x33, __u16[KEY_MAX - BTN_MISC + 1])	
#define JSIOCGBTNMAP		_IOR('j', 0x34, __u16[KEY_MAX - BTN_MISC + 1])	


#define JS_CORR_NONE		0x00	
#define JS_CORR_BROKEN		0x01	

struct js_corr {
	__s32 coef[8];
	__s16 prec;
	__u16 type;
};


#define JS_RETURN		sizeof(struct JS_DATA_TYPE)
#define JS_TRUE			1
#define JS_FALSE		0
#define JS_X_0			0x01
#define JS_Y_0			0x02
#define JS_X_1			0x04
#define JS_Y_1			0x08
#define JS_MAX			2

#define JS_DEF_TIMEOUT		0x1300
#define JS_DEF_CORR		0
#define JS_DEF_TIMELIMIT	10L

#define JS_SET_CAL		1
#define JS_GET_CAL		2
#define JS_SET_TIMEOUT		3
#define JS_GET_TIMEOUT		4
#define JS_SET_TIMELIMIT	5
#define JS_GET_TIMELIMIT	6
#define JS_GET_ALL		7
#define JS_SET_ALL		8

struct JS_DATA_TYPE {
	__s32 buttons;
	__s32 x;
	__s32 y;
};

struct JS_DATA_SAVE_TYPE_32 {
	__s32 JS_TIMEOUT;
	__s32 BUSY;
	__s32 JS_EXPIRETIME;
	__s32 JS_TIMELIMIT;
	struct JS_DATA_TYPE JS_SAVE;
	struct JS_DATA_TYPE JS_CORR;
};

struct JS_DATA_SAVE_TYPE_64 {
	__s32 JS_TIMEOUT;
	__s32 BUSY;
	__s64 JS_EXPIRETIME;
	__s64 JS_TIMELIMIT;
	struct JS_DATA_TYPE JS_SAVE;
	struct JS_DATA_TYPE JS_CORR;
};

#ifdef __KERNEL__
#if BITS_PER_LONG == 64
#define JS_DATA_SAVE_TYPE JS_DATA_SAVE_TYPE_64
#elif BITS_PER_LONG == 32
#define JS_DATA_SAVE_TYPE JS_DATA_SAVE_TYPE_32
#else
#error Unexpected BITS_PER_LONG
#endif
#endif

#endif 
