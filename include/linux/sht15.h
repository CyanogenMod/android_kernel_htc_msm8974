/*
 * sht15.h - support for the SHT15 Temperature and Humidity Sensor
 *
 * Copyright (c) 2009 Jonathan Cameron
 *
 * Copyright (c) 2007 Wouter Horre
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * For further information, see the Documentation/hwmon/sht15 file.
 */

struct sht15_platform_data {
	int gpio_data;
	int gpio_sck;
	int supply_mv;
	bool checksum;
	bool no_otp_reload;
	bool low_resolution;
};

