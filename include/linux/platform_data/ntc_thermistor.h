/*
 * ntc_thermistor.h - NTC Thermistors
 *
 *  Copyright (C) 2010 Samsung Electronics
 *  MyungJoo Ham <myungjoo.ham@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _LINUX_NTC_H
#define _LINUX_NTC_H

enum ntc_thermistor_type {
	TYPE_NCPXXWB473,
	TYPE_NCPXXWL333,
};

struct ntc_thermistor_platform_data {
	int (*read_uV)(void);
	unsigned int pullup_uV;

	unsigned int pullup_ohm;
	unsigned int pulldown_ohm;
	enum { NTC_CONNECTED_POSITIVE, NTC_CONNECTED_GROUND } connect;

	int (*read_ohm)(void);
};

#endif 
