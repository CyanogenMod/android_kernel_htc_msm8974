/*
 * include/linux/gpio-fan.h
 *
 * Platform data structure for GPIO fan driver
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __LINUX_GPIO_FAN_H
#define __LINUX_GPIO_FAN_H

struct gpio_fan_alarm {
	unsigned	gpio;
	unsigned	active_low;
};

struct gpio_fan_speed {
	int rpm;
	int ctrl_val;
};

struct gpio_fan_platform_data {
	int			num_ctrl;
	unsigned		*ctrl;	
	struct gpio_fan_alarm	*alarm;	
	int			num_speed;
	struct gpio_fan_speed	*speed;
};

#endif 
