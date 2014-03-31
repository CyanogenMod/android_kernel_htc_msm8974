/*
 * Copyright (C) 2011 Kionix, Inc.
 * Written by Chris Hudson <chudson@kionix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __KXTJ9_H__
#define __KXTJ9_H__

#define KXTJ9_I2C_ADDR		0x0F

struct kxtj9_platform_data {
	unsigned int min_interval;	
	unsigned int init_interval;	

	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	bool negate_x;
	bool negate_y;
	bool negate_z;

	
	
	#define RES_8BIT		0
	#define RES_12BIT		(1 << 6)
	u8 res_ctl;
	
	#define KXTJ9_G_2G		0
	#define KXTJ9_G_4G		(1 << 3)
	#define KXTJ9_G_8G		(1 << 4)
	u8 g_range;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
};
#endif  
