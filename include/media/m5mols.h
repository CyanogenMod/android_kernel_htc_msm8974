/*
 * Driver header for M-5MOLS 8M Pixel camera sensor with ISP
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * Author: HeungJun Kim <riverful.kim@samsung.com>
 *
 * Copyright (C) 2009 Samsung Electronics Co., Ltd.
 * Author: Dongsoo Nathaniel Kim <dongsoo45.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEDIA_M5MOLS_H
#define MEDIA_M5MOLS_H

struct m5mols_platform_data {
	int gpio_reset;
	u8 reset_polarity;
	int (*set_power)(struct device *dev, int on);
};

#endif	
