/*
 * include/media/as3645a.h
 *
 * Copyright (C) 2008-2011 Nokia Corporation
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __AS3645A_H__
#define __AS3645A_H__

#include <media/v4l2-subdev.h>

#define AS3645A_NAME				"as3645a"
#define AS3645A_I2C_ADDR			(0x60 >> 1) 

#define AS3645A_FLASH_TIMEOUT_MIN		100000	
#define AS3645A_FLASH_TIMEOUT_MAX		850000
#define AS3645A_FLASH_TIMEOUT_STEP		50000

#define AS3645A_FLASH_INTENSITY_MIN		200	
#define AS3645A_FLASH_INTENSITY_MAX_1LED	500
#define AS3645A_FLASH_INTENSITY_MAX_2LEDS	400
#define AS3645A_FLASH_INTENSITY_STEP		20

#define AS3645A_TORCH_INTENSITY_MIN		20	
#define AS3645A_TORCH_INTENSITY_MAX		160
#define AS3645A_TORCH_INTENSITY_STEP		20

#define AS3645A_INDICATOR_INTENSITY_MIN		0	
#define AS3645A_INDICATOR_INTENSITY_MAX		10000
#define AS3645A_INDICATOR_INTENSITY_STEP	2500

struct as3645a_platform_data {
	int (*set_power)(struct v4l2_subdev *subdev, int on);
	unsigned int vref;
	unsigned int peak;
	bool ext_strobe;

	
	unsigned int flash_max_current;
	unsigned int torch_max_current;
	unsigned int timeout_max;
};

#endif 
