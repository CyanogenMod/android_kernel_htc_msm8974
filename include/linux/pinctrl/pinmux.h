/*
 * Interface the pinmux subsystem
 *
 * Copyright (C) 2011 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * Based on bits of regulator core, gpio core and clk core
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __LINUX_PINCTRL_PINMUX_H
#define __LINUX_PINCTRL_PINMUX_H

#include <linux/list.h>
#include <linux/seq_file.h>
#include "pinctrl.h"

#ifdef CONFIG_PINMUX

struct pinctrl_dev;

struct pinmux_ops {
	int (*request) (struct pinctrl_dev *pctldev, unsigned offset);
	int (*free) (struct pinctrl_dev *pctldev, unsigned offset);
	int (*get_functions_count) (struct pinctrl_dev *pctldev);
	const char *(*get_function_name) (struct pinctrl_dev *pctldev,
					  unsigned selector);
	int (*get_function_groups) (struct pinctrl_dev *pctldev,
				  unsigned selector,
				  const char * const **groups,
				  unsigned * const num_groups);
	int (*enable) (struct pinctrl_dev *pctldev, unsigned func_selector,
		       unsigned group_selector);
	void (*disable) (struct pinctrl_dev *pctldev, unsigned func_selector,
			 unsigned group_selector);
	int (*gpio_request_enable) (struct pinctrl_dev *pctldev,
				    struct pinctrl_gpio_range *range,
				    unsigned offset);
	void (*gpio_disable_free) (struct pinctrl_dev *pctldev,
				   struct pinctrl_gpio_range *range,
				   unsigned offset);
	int (*gpio_set_direction) (struct pinctrl_dev *pctldev,
				   struct pinctrl_gpio_range *range,
				   unsigned offset,
				   bool input);
};

#endif 

#endif 
