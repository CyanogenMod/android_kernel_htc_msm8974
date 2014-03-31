/*
 *  Copyright (C) 2009-2010, Lars-Peter Clausen <lars@metafoo.de>
 *      PCF50633 backlight device driver
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __LINUX_MFD_PCF50633_BACKLIGHT
#define __LINUX_MFD_PCF50633_BACKLIGHT

struct pcf50633_bl_platform_data {
	unsigned int	default_brightness;
	unsigned int	default_brightness_limit;
	uint8_t		ramp_time;
};


struct pcf50633;

int pcf50633_bl_set_brightness_limit(struct pcf50633 *pcf, unsigned int limit);

#endif

