/*
 * Per-device information from the pin control system.
 * This is the stuff that get included into the device
 * core.
 *
 * Copyright (C) 2012 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * This interface is used in the core to keep track of pins.
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef PINCTRL_DEVINFO_H
#define PINCTRL_DEVINFO_H

#ifdef CONFIG_PINCTRL

#include <linux/pinctrl/consumer.h>

struct dev_pin_info {
	struct pinctrl *p;
	struct pinctrl_state *default_state;
};

extern int pinctrl_bind_pins(struct device *dev);

#else


static inline int pinctrl_bind_pins(struct device *dev)
{
	return 0;
}

#endif 
#endif 
