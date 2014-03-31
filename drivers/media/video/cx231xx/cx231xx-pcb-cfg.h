/*
   cx231xx-pcb-cfg.h - driver for Conexant
		Cx23100/101/102 USB video capture devices

   Copyright (C) 2008 <srinivasa.deevi at conexant dot com>

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _PCB_CONFIG_H_
#define _PCB_CONFIG_H_

#include <linux/init.h>
#include <linux/module.h>

#define CLASS_DEFAULT       0xFF

enum VENDOR_REQUEST_TYPE {
	
	VRT_SET_I2C0 = 0x0,
	VRT_SET_I2C1 = 0x1,
	VRT_SET_I2C2 = 0x2,
	VRT_GET_I2C0 = 0x4,
	VRT_GET_I2C1 = 0x5,
	VRT_GET_I2C2 = 0x6,

	
	VRT_SET_GPIO = 0x8,
	VRT_GET_GPIO = 0x9,

	
	VRT_SET_GPIE = 0xA,
	VRT_GET_GPIE = 0xB,

	
	VRT_SET_REGISTER = 0xC,
	VRT_GET_REGISTER = 0xD,

	
	VRT_GET_EXTCID_DESC = 0xFF,
};

enum BYTE_ENABLE_MASK {
	ENABLE_ONE_BYTE = 0x1,
	ENABLE_TWE_BYTE = 0x3,
	ENABLE_THREE_BYTE = 0x7,
	ENABLE_FOUR_BYTE = 0xF,
};

#define SPEED_MASK      0x1
enum USB_SPEED{
	FULL_SPEED = 0x0,	
	HIGH_SPEED = 0x1	
};

enum _true_false{
	FALSE = 0,
	TRUE = 1
};

#define TS_MASK         0x6
enum TS_PORT{
	NO_TS_PORT = 0x0,	
	TS1_PORT = 0x4,		
	TS1_TS2_PORT = 0x6,	
	TS1_EXT_CLOCK = 0x6,	
	TS1VIP_TS2_PORT = 0x2	
};

#define EAVP_MASK       0x8
enum EAV_PRESENT{
	NO_EXTERNAL_AV = 0x0,	
	EXTERNAL_AV = 0x8	
};

#define ATM_MASK        0x30
enum AT_MODE{
	DIF_TUNER = 0x30,	
	BASEBAND_SOUND = 0x20,	
	NO_TUNER = 0x10		
};

#define PWR_SEL_MASK    0x40
enum POWE_TYPE{
	SELF_POWER = 0x0,	
	BUS_POWER = 0x40	
};

enum USB_POWE_TYPE{
	USB_SELF_POWER = 0,
	USB_BUS_POWER
};

#define BO_0_MASK       0x80
enum AVDEC_STATUS{
	AVDEC_DISABLE = 0x0,	
	AVDEC_ENABLE = 0x80	
};

#define BO_1_MASK       0x100

#define BUSPOWER_MASK   0xC4	
#define SELFPOWER_MASK  0x86

#define NOT_DECIDE_YET  0xFE
#define NOT_SUPPORTED   0xFF

#define MOD_DIGITAL     0x1
#define MOD_ANALOG      0x2
#define MOD_DIF         0x4
#define MOD_EXTERNAL    0x8
#define CAP_ALL_MOD     0x0f

#define SOURCE_DIGITAL          0x1
#define SOURCE_ANALOG           0x2
#define SOURCE_DIF              0x4
#define SOURCE_EXTERNAL         0x8
#define SOURCE_TS_BDA			0x10
#define SOURCE_TS_ENCODE		0x20
#define SOURCE_TS_EXTERNAL   	0x40

struct INTERFACE_INFO {
	u8 interrupt_index;
	u8 ts1_index;
	u8 ts2_index;
	u8 audio_index;
	u8 video_index;
	u8 vanc_index;		
	u8 hanc_index;		
	u8 ir_index;
};

enum INDEX_INTERFACE_INFO{
	INDEX_INTERRUPT = 0x0,
	INDEX_TS1,
	INDEX_TS2,
	INDEX_AUDIO,
	INDEX_VIDEO,
	INDEX_VANC,
	INDEX_HANC,
	INDEX_IR,
};

struct CONFIG_INFO {
	u8 config_index;
	struct INTERFACE_INFO interface_info;
};

struct pcb_config {
	u8 index;
	u8 type;		
	u8 speed;		
	u8 mode;		
	u32 ts1_source;		
	u32 ts2_source;
	u32 analog_source;
	u8 digital_index;	
	u8 analog_index;	
	u8 dif_index;		
	u8 external_index;	
	u8 config_num;		
	struct CONFIG_INFO hs_config_info[3];
	struct CONFIG_INFO fs_config_info[3];
};

enum INDEX_PCB_CONFIG{
	INDEX_SELFPOWER_DIGITAL_ONLY = 0x0,
	INDEX_SELFPOWER_DUAL_DIGITAL,
	INDEX_SELFPOWER_ANALOG_ONLY,
	INDEX_SELFPOWER_DUAL,
	INDEX_SELFPOWER_TRIPLE,
	INDEX_SELFPOWER_COMPRESSOR,
	INDEX_BUSPOWER_DIGITAL_ONLY,
	INDEX_BUSPOWER_ANALOG_ONLY,
	INDEX_BUSPOWER_DIF_ONLY,
	INDEX_BUSPOWER_EXTERNAL_ONLY,
	INDEX_BUSPOWER_EXTERNAL_ANALOG,
	INDEX_BUSPOWER_EXTERNAL_DIF,
	INDEX_BUSPOWER_EXTERNAL_DIGITAL,
	INDEX_BUSPOWER_DIGITAL_ANALOG,
	INDEX_BUSPOWER_DIGITAL_DIF,
	INDEX_BUSPOWER_DIGITAL_ANALOG_EXTERNAL,
	INDEX_BUSPOWER_DIGITAL_DIF_EXTERNAL,
};

struct cx231xx;

u32 initialize_cx231xx(struct cx231xx *p_dev);

#endif
