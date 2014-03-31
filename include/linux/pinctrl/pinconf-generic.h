/*
 * Interface the generic pinconfig portions of the pinctrl subsystem
 *
 * Copyright (C) 2011 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * This interface is used in the core to keep track of pins.
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __LINUX_PINCTRL_PINCONF_GENERIC_H
#define __LINUX_PINCTRL_PINCONF_GENERIC_H

#ifdef CONFIG_GENERIC_PINCONF

enum pin_config_param {
	PIN_CONFIG_BIAS_DISABLE,
	PIN_CONFIG_BIAS_HIGH_IMPEDANCE,
	PIN_CONFIG_BIAS_BUS_HOLD,
	PIN_CONFIG_BIAS_PULL_UP,
	PIN_CONFIG_BIAS_PULL_DOWN,
	PIN_CONFIG_BIAS_PULL_PIN_DEFAULT,
	PIN_CONFIG_DRIVE_PUSH_PULL,
	PIN_CONFIG_DRIVE_OPEN_DRAIN,
	PIN_CONFIG_DRIVE_OPEN_SOURCE,
	PIN_CONFIG_DRIVE_STRENGTH,
	PIN_CONFIG_INPUT_SCHMITT_ENABLE,
	PIN_CONFIG_INPUT_SCHMITT,
	PIN_CONFIG_INPUT_DEBOUNCE,
	PIN_CONFIG_POWER_SOURCE,
	PIN_CONFIG_SLEW_RATE,
	PIN_CONFIG_LOW_POWER_MODE,
	PIN_CONFIG_OUTPUT,
	PIN_CONFIG_END = 0x7FFF,
};

#define PIN_CONF_PACKED(p, a) ((a << 16) | ((unsigned long) p & 0xffffUL))


static inline enum pin_config_param pinconf_to_config_param(unsigned long config)
{
	return (enum pin_config_param) (config & 0xffffUL);
}

static inline u16 pinconf_to_config_argument(unsigned long config)
{
	return (enum pin_config_param) ((config >> 16) & 0xffffUL);
}

static inline unsigned long pinconf_to_config_packed(enum pin_config_param param,
						     u16 argument)
{
	return PIN_CONF_PACKED(param, argument);
}

#endif 

#endif 
