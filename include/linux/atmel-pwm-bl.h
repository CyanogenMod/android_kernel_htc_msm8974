/*
 * Copyright (C) 2007 Atmel Corporation
 *
 * Driver for the AT32AP700X PS/2 controller (PSIF).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef __INCLUDE_ATMEL_PWM_BL_H
#define __INCLUDE_ATMEL_PWM_BL_H

struct atmel_pwm_bl_platform_data {
	unsigned int pwm_channel;
	unsigned int pwm_frequency;
	unsigned int pwm_compare_max;
	unsigned int pwm_duty_max;
	unsigned int pwm_duty_min;
	unsigned int pwm_active_low;
	int gpio_on;
	unsigned int on_active_low;
};

#endif 
