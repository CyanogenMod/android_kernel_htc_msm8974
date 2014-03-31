/*
 * max1586.h  --  Voltage regulation for the Maxim 1586
 *
 * Copyright (C) 2008 Robert Jarzmik
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

#ifndef REGULATOR_MAX1586
#define REGULATOR_MAX1586

#include <linux/regulator/machine.h>

#define MAX1586_V3 0
#define MAX1586_V6 1

#define MAX1586_GAIN_NO_R24   1000000  
#define MAX1586_GAIN_R24_3k32 1051098  
#define MAX1586_GAIN_R24_5k11 1078648  
#define MAX1586_GAIN_R24_7k5  1115432  

struct max1586_subdev_data {
	int				id;
	char				*name;
	struct regulator_init_data	*platform_data;
};

struct max1586_platform_data {
	int num_subdevs;
	struct max1586_subdev_data *subdevs;
	int v3_gain;
};

#endif
