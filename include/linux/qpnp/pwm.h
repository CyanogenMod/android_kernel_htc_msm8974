/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __QPNP_PWM_H__
#define __QPNP_PWM_H__

#include <linux/pwm.h>

#define PM_PWM_PERIOD_MIN			7
#define PM_PWM_PERIOD_MAX			(384 * USEC_PER_SEC)
#define PM_PWM_LUT_RAMP_STEP_TIME_MAX		499
#define PM_PWM_MAX_PAUSE_CNT			8191
#define PM_PWM_LUT_PAUSE_MAX \
	((PM_PWM_MAX_PAUSE_CNT - 1) * PM_PWM_LUT_RAMP_STEP_TIME_MAX) 

#define PM_PWM_LUT_LOOP			0x01
#define PM_PWM_LUT_RAMP_UP		0x02
#define PM_PWM_LUT_REVERSE		0x04
#define PM_PWM_LUT_PAUSE_HI_EN		0x08
#define PM_PWM_LUT_PAUSE_LO_EN		0x10

#define PM_PWM_LUT_NO_TABLE		0x20
#define PM_PWM_LUT_USE_RAW_VALUE	0x40


enum pm_pwm_size {
	PM_PWM_SIZE_6BIT =	6,
	PM_PWM_SIZE_9BIT =	9,
};

enum pm_pwm_clk {
	PM_PWM_CLK_1KHZ,
	PM_PWM_CLK_32KHZ,
	PM_PWM_CLK_19P2MHZ,
};

enum pm_pwm_pre_div {
	PM_PWM_PDIV_2,
	PM_PWM_PDIV_3,
	PM_PWM_PDIV_5,
	PM_PWM_PDIV_6,
};

struct pwm_period_config {
	enum pm_pwm_size	pwm_size;
	enum pm_pwm_clk		clk;
	enum pm_pwm_pre_div	pre_div;
	int			pre_div_exp;
};

struct pwm_duty_cycles {
	int *duty_pcts;
	int num_duty_pcts;
	int duty_ms;
	int start_idx;
};

int pwm_config_period(struct pwm_device *pwm,
			     struct pwm_period_config *pwm_p);

int pwm_config_pwm_value(struct pwm_device *pwm, int pwm_value);

enum pm_pwm_mode {
	PM_PWM_MODE_PWM,
	PM_PWM_MODE_LPG,
};

int pwm_change_mode(struct pwm_device *pwm, enum pm_pwm_mode mode);

struct lut_params {
	int start_idx;
	int idx_len;
	int lut_pause_hi;
	int lut_pause_lo;
	int ramp_step_ms;
	int flags;
};

int pwm_lut_config(struct pwm_device *pwm, int period_us,
		int duty_pct[], struct lut_params lut_params);

int pwm_config_us(struct pwm_device *pwm,
		int duty_us, int period_us);






void qpnp_led_set_for_key(int brightness_key);

#endif 
