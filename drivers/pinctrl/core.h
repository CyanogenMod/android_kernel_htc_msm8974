/*
 * Core private header for the pin control subsystem
 *
 * Copyright (C) 2011 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/machine.h>

struct pinctrl_gpio_range;

struct pinctrl_dev {
	struct list_head node;
	struct pinctrl_desc *desc;
	struct radix_tree_root pin_desc_tree;
	struct list_head gpio_ranges;
	struct device *dev;
	struct module *owner;
	void *driver_data;
	struct pinctrl *p;
	struct pinctrl_state *hog_default;
	struct pinctrl_state *hog_sleep;
#ifdef CONFIG_DEBUG_FS
	struct dentry *device_root;
#endif
};

struct pinctrl {
	struct list_head node;
	struct device *dev;
	struct list_head states;
	struct pinctrl_state *state;
	struct list_head dt_maps;
	struct kref users;
};

struct pinctrl_state {
	struct list_head node;
	const char *name;
	struct list_head settings;
};

struct pinctrl_setting_mux {
	unsigned group;
	unsigned func;
};

struct pinctrl_setting_configs {
	unsigned group_or_pin;
	unsigned long *configs;
	unsigned num_configs;
};

struct pinctrl_setting {
	struct list_head node;
	enum pinctrl_map_type type;
	struct pinctrl_dev *pctldev;
	const char *dev_name;
	union {
		struct pinctrl_setting_mux mux;
		struct pinctrl_setting_configs configs;
	} data;
};

struct pin_desc {
	struct pinctrl_dev *pctldev;
	const char *name;
	bool dynamic_name;
	
#ifdef CONFIG_PINMUX
	unsigned mux_usecount;
	const char *mux_owner;
	const struct pinctrl_setting_mux *mux_setting;
	const char *gpio_owner;
#endif
};

struct pinctrl_maps {
	struct list_head node;
	struct pinctrl_map const *maps;
	unsigned num_maps;
};

struct pinctrl_dev *get_pinctrl_dev_from_devname(const char *dev_name);
int pin_get_from_name(struct pinctrl_dev *pctldev, const char *name);
const char *pin_get_name(struct pinctrl_dev *pctldev, const unsigned pin);
int pinctrl_get_group_selector(struct pinctrl_dev *pctldev,
			       const char *pin_group);

static inline struct pin_desc *pin_desc_get(struct pinctrl_dev *pctldev,
					    unsigned int pin)
{
	return radix_tree_lookup(&pctldev->pin_desc_tree, pin);
}

int pinctrl_register_map(struct pinctrl_map const *maps, unsigned num_maps,
			 bool dup, bool locked);
void pinctrl_unregister_map(struct pinctrl_map const *map);

extern int pinctrl_force_sleep(struct pinctrl_dev *pctldev);
extern int pinctrl_force_default(struct pinctrl_dev *pctldev);

extern struct mutex pinctrl_mutex;
extern struct list_head pinctrldev_list;
extern struct list_head pinctrl_maps;

#define for_each_maps(_maps_node_, _i_, _map_) \
	list_for_each_entry(_maps_node_, &pinctrl_maps, node) \
		for (_i_ = 0, _map_ = &_maps_node_->maps[_i_]; \
			_i_ < _maps_node_->num_maps; \
			_i_++, _map_ = &_maps_node_->maps[_i_])
