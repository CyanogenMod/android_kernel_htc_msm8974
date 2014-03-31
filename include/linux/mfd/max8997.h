/*
 * max8997.h - Driver for the Maxim 8997/8966
 *
 *  Copyright (C) 2009-2010 Samsung Electrnoics
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
 *
 * This driver is based on max8998.h
 *
 * MAX8997 has PMIC, MUIC, HAPTIC, RTC, FLASH, and Fuel Gauge devices.
 * Except Fuel Gauge, every device shares the same I2C bus and included in
 * this mfd driver. Although the fuel gauge is included in the chip, it is
 * excluded from the driver because a) it has a different I2C bus from
 * others and b) it can be enabled simply by using MAX17042 driver.
 */

#ifndef __LINUX_MFD_MAX8998_H
#define __LINUX_MFD_MAX8998_H

#include <linux/regulator/consumer.h>

enum max8998_regulators {
	MAX8997_LDO1 = 0,
	MAX8997_LDO2,
	MAX8997_LDO3,
	MAX8997_LDO4,
	MAX8997_LDO5,
	MAX8997_LDO6,
	MAX8997_LDO7,
	MAX8997_LDO8,
	MAX8997_LDO9,
	MAX8997_LDO10,
	MAX8997_LDO11,
	MAX8997_LDO12,
	MAX8997_LDO13,
	MAX8997_LDO14,
	MAX8997_LDO15,
	MAX8997_LDO16,
	MAX8997_LDO17,
	MAX8997_LDO18,
	MAX8997_LDO21,
	MAX8997_BUCK1,
	MAX8997_BUCK2,
	MAX8997_BUCK3,
	MAX8997_BUCK4,
	MAX8997_BUCK5,
	MAX8997_BUCK6,
	MAX8997_BUCK7,
	MAX8997_EN32KHZ_AP,
	MAX8997_EN32KHZ_CP,
	MAX8997_ENVICHG,
	MAX8997_ESAFEOUT1,
	MAX8997_ESAFEOUT2,
	MAX8997_CHARGER_CV, 
	MAX8997_CHARGER, 
	MAX8997_CHARGER_TOPOFF, 

	MAX8997_REG_MAX,
};

struct max8997_regulator_data {
	int id;
	struct regulator_init_data *initdata;
};

enum max8997_muic_usb_type {
	MAX8997_USB_HOST,
	MAX8997_USB_DEVICE,
};

enum max8997_muic_charger_type {
	MAX8997_CHARGER_TYPE_NONE = 0,
	MAX8997_CHARGER_TYPE_USB,
	MAX8997_CHARGER_TYPE_DOWNSTREAM_PORT,
	MAX8997_CHARGER_TYPE_DEDICATED_CHG,
	MAX8997_CHARGER_TYPE_500MA,
	MAX8997_CHARGER_TYPE_1A,
	MAX8997_CHARGER_TYPE_DEAD_BATTERY = 7,
};

struct max8997_muic_reg_data {
	u8 addr;
	u8 data;
};

struct max8997_muic_platform_data {
	void (*usb_callback)(enum max8997_muic_usb_type usb_type,
		bool attached);
	void (*charger_callback)(bool attached,
		enum max8997_muic_charger_type charger_type);
	void (*deskdock_callback) (bool attached);
	void (*cardock_callback) (bool attached);
	void (*mhl_callback) (bool attached);
	void (*uart_callback) (bool attached);

	struct max8997_muic_reg_data *init_data;
	int num_init_data;
};

enum max8997_haptic_motor_type {
	MAX8997_HAPTIC_ERM,
	MAX8997_HAPTIC_LRA,
};

enum max8997_haptic_pulse_mode {
	MAX8997_EXTERNAL_MODE,
	MAX8997_INTERNAL_MODE,
};

enum max8997_haptic_pwm_divisor {
	MAX8997_PWM_DIVISOR_32,
	MAX8997_PWM_DIVISOR_64,
	MAX8997_PWM_DIVISOR_128,
	MAX8997_PWM_DIVISOR_256,
};

struct max8997_haptic_platform_data {
	unsigned int pwm_channel_id;
	unsigned int pwm_period;

	enum max8997_haptic_motor_type type;
	enum max8997_haptic_pulse_mode mode;
	enum max8997_haptic_pwm_divisor pwm_divisor;

	unsigned int internal_mode_pattern;
	unsigned int pattern_cycle;
	unsigned int pattern_signal_period;
};

enum max8997_led_mode {
	MAX8997_NONE,
	MAX8997_FLASH_MODE,
	MAX8997_MOVIE_MODE,
	MAX8997_FLASH_PIN_CONTROL_MODE,
	MAX8997_MOVIE_PIN_CONTROL_MODE,
};

struct max8997_led_platform_data {
	enum max8997_led_mode mode[2];
	u8 brightness[2];
};

struct max8997_platform_data {
	
	int irq_base;
	int ono;
	int wakeup;

	
	struct max8997_regulator_data *regulators;
	int num_regulators;

	bool ignore_gpiodvs_side_effect;
	int buck125_gpios[3]; 
	int buck125_default_idx; 
	unsigned int buck1_voltage[8]; 
	bool buck1_gpiodvs;
	unsigned int buck2_voltage[8];
	bool buck2_gpiodvs;
	unsigned int buck5_voltage[8];
	bool buck5_gpiodvs;

	
	
	int eoc_mA; 
	
	int timeout; 

	
	struct max8997_muic_platform_data *muic_pdata;

	
	struct max8997_haptic_platform_data *haptic_pdata;

	
	
	struct max8997_led_platform_data *led_pdata;
};

#endif 
