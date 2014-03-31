/*
 * Interface the pinctrl subsystem
 *
 * Copyright (C) 2011 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * This interface is used in the core to keep track of pins.
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __LINUX_PINCTRL_PINCTRL_H
#define __LINUX_PINCTRL_PINCTRL_H

#ifdef CONFIG_PINCTRL

#include <linux/radix-tree.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include "pinctrl-state.h"

struct device;
struct pinctrl_dev;
struct pinctrl_map;
struct pinmux_ops;
struct pinconf_ops;
struct gpio_chip;
struct device_node;

struct pinctrl_pin_desc {
	unsigned number;
	const char *name;
};

#define PINCTRL_PIN(a, b) { .number = a, .name = b }
#define PINCTRL_PIN_ANON(a) { .number = a }

struct pinctrl_gpio_range {
	struct list_head node;
	const char *name;
	unsigned int id;
	unsigned int base;
	unsigned int pin_base;
	unsigned int npins;
	struct gpio_chip *gc;
};

struct pinctrl_ops {
	int (*get_groups_count) (struct pinctrl_dev *pctldev);
	const char *(*get_group_name) (struct pinctrl_dev *pctldev,
				       unsigned selector);
	int (*get_group_pins) (struct pinctrl_dev *pctldev,
			       unsigned selector,
			       const unsigned **pins,
			       unsigned *num_pins);
	void (*pin_dbg_show) (struct pinctrl_dev *pctldev, struct seq_file *s,
			  unsigned offset);
	int (*dt_node_to_map) (struct pinctrl_dev *pctldev,
			       struct device_node *np_config,
			       struct pinctrl_map **map, unsigned *num_maps);
	void (*dt_free_map) (struct pinctrl_dev *pctldev,
			     struct pinctrl_map *map, unsigned num_maps);
};

struct pinctrl_desc {
	const char *name;
	struct pinctrl_pin_desc const *pins;
	unsigned int npins;
	struct pinctrl_ops *pctlops;
	struct pinmux_ops *pmxops;
	struct pinconf_ops *confops;
	struct module *owner;
};

extern struct pinctrl_dev *pinctrl_register(struct pinctrl_desc *pctldesc,
				struct device *dev, void *driver_data);
extern void pinctrl_unregister(struct pinctrl_dev *pctldev);
extern bool pin_is_valid(struct pinctrl_dev *pctldev, int pin);
extern void pinctrl_add_gpio_range(struct pinctrl_dev *pctldev,
				struct pinctrl_gpio_range *range);
extern void pinctrl_add_gpio_ranges(struct pinctrl_dev *pctldev,
				struct pinctrl_gpio_range *ranges,
				unsigned nranges);
extern void pinctrl_remove_gpio_range(struct pinctrl_dev *pctldev,
				struct pinctrl_gpio_range *range);

extern struct pinctrl_dev *pinctrl_find_and_add_gpio_range(const char *devname,
		struct pinctrl_gpio_range *range);
extern struct pinctrl_gpio_range *
pinctrl_find_gpio_range_from_pin(struct pinctrl_dev *pctldev,
				 unsigned int pin);

#ifdef CONFIG_OF
extern struct pinctrl_dev *of_pinctrl_get(struct device_node *np);
#else
static inline
struct pinctrl_dev *of_pinctrl_get(struct device_node *np)
{
	return NULL;
}
#endif 

extern const char *pinctrl_dev_get_name(struct pinctrl_dev *pctldev);
extern const char *pinctrl_dev_get_devname(struct pinctrl_dev *pctldev);
extern void *pinctrl_dev_get_drvdata(struct pinctrl_dev *pctldev);
#else

struct pinctrl_dev;

static inline bool pin_is_valid(struct pinctrl_dev *pctldev, int pin)
{
	return pin >= 0;
}

#endif 

#endif 
