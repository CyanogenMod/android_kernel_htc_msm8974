/*
 * Interface the pinconfig portions of the pinctrl subsystem
 *
 * Copyright (C) 2011 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * This interface is used in the core to keep track of pins.
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __LINUX_PINCTRL_PINCONF_H
#define __LINUX_PINCTRL_PINCONF_H

#ifdef CONFIG_PINCONF

struct pinctrl_dev;
struct seq_file;

struct pinconf_ops {
#ifdef CONFIG_GENERIC_PINCONF
	bool is_generic;
#endif
	int (*pin_config_get) (struct pinctrl_dev *pctldev,
			       unsigned pin,
			       unsigned long *config);
	int (*pin_config_set) (struct pinctrl_dev *pctldev,
			       unsigned pin,
			       unsigned long config);
	int (*pin_config_group_get) (struct pinctrl_dev *pctldev,
				     unsigned selector,
				     unsigned long *config);
	int (*pin_config_group_set) (struct pinctrl_dev *pctldev,
				     unsigned selector,
				     unsigned long config);
	void (*pin_config_dbg_show) (struct pinctrl_dev *pctldev,
				     struct seq_file *s,
				     unsigned offset);
	void (*pin_config_group_dbg_show) (struct pinctrl_dev *pctldev,
					   struct seq_file *s,
					   unsigned selector);
	void (*pin_config_config_dbg_show) (struct pinctrl_dev *pctldev,
					    struct seq_file *s,
					    unsigned long config);
};

#endif

#endif 
