/*
 * include/media/si4713.h
 *
 * Board related data definitions for Si4713 i2c device driver.
 *
 * Copyright (c) 2009 Nokia Corporation
 * Contact: Eduardo Valentin <eduardo.valentin@nokia.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#ifndef SI4713_H
#define SI4713_H

#define SI4713_I2C_ADDR_BUSEN_HIGH	0x63
#define SI4713_I2C_ADDR_BUSEN_LOW	0x11

struct si4713_platform_data {
	int gpio_reset; 
};

struct si4713_rnl {
	__u32 index;		
	__u32 frequency;	
	__s32 rnl;		
	__u32 reserved[4];	
};

#define SI4713_IOC_MEASURE_RNL	_IOWR('V', BASE_VIDIOC_PRIVATE + 0, \
						struct si4713_rnl)

#endif 
