/*
 * Machine interface for the pinctrl subsystem.
 *
 * Copyright (C) 2011 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * Based on bits of regulator core, gpio core and clk core
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __LINUX_PINCTRL_MACHINE_H
#define __LINUX_PINCTRL_MACHINE_H

#include <linux/bug.h>

#include "pinctrl-state.h"

enum pinctrl_map_type {
	PIN_MAP_TYPE_INVALID,
	PIN_MAP_TYPE_DUMMY_STATE,
	PIN_MAP_TYPE_MUX_GROUP,
	PIN_MAP_TYPE_CONFIGS_PIN,
	PIN_MAP_TYPE_CONFIGS_GROUP,
};

struct pinctrl_map_mux {
	const char *group;
	const char *function;
};

struct pinctrl_map_configs {
	const char *group_or_pin;
	unsigned long *configs;
	unsigned num_configs;
};

struct pinctrl_map {
	const char *dev_name;
	const char *name;
	enum pinctrl_map_type type;
	const char *ctrl_dev_name;
	union {
		struct pinctrl_map_mux mux;
		struct pinctrl_map_configs configs;
	} data;
};


#define PIN_MAP_DUMMY_STATE(dev, state) \
	{								\
		.dev_name = dev,					\
		.name = state,						\
		.type = PIN_MAP_TYPE_DUMMY_STATE,			\
	}

#define PIN_MAP_MUX_GROUP(dev, state, pinctrl, grp, func)		\
	{								\
		.dev_name = dev,					\
		.name = state,						\
		.type = PIN_MAP_TYPE_MUX_GROUP,				\
		.ctrl_dev_name = pinctrl,				\
		.data.mux = {						\
			.group = grp,					\
			.function = func,				\
		},							\
	}

#define PIN_MAP_MUX_GROUP_DEFAULT(dev, pinctrl, grp, func)		\
	PIN_MAP_MUX_GROUP(dev, PINCTRL_STATE_DEFAULT, pinctrl, grp, func)

#define PIN_MAP_MUX_GROUP_HOG(dev, state, grp, func)			\
	PIN_MAP_MUX_GROUP(dev, state, dev, grp, func)

#define PIN_MAP_MUX_GROUP_HOG_DEFAULT(dev, grp, func)			\
	PIN_MAP_MUX_GROUP(dev, PINCTRL_STATE_DEFAULT, dev, grp, func)

#define PIN_MAP_CONFIGS_PIN(dev, state, pinctrl, pin, cfgs)		\
	{								\
		.dev_name = dev,					\
		.name = state,						\
		.type = PIN_MAP_TYPE_CONFIGS_PIN,			\
		.ctrl_dev_name = pinctrl,				\
		.data.configs = {					\
			.group_or_pin = pin,				\
			.configs = cfgs,				\
			.num_configs = ARRAY_SIZE(cfgs),		\
		},							\
	}

#define PIN_MAP_CONFIGS_PIN_DEFAULT(dev, pinctrl, pin, cfgs)		\
	PIN_MAP_CONFIGS_PIN(dev, PINCTRL_STATE_DEFAULT, pinctrl, pin, cfgs)

#define PIN_MAP_CONFIGS_PIN_HOG(dev, state, pin, cfgs)			\
	PIN_MAP_CONFIGS_PIN(dev, state, dev, pin, cfgs)

#define PIN_MAP_CONFIGS_PIN_HOG_DEFAULT(dev, pin, cfgs)			\
	PIN_MAP_CONFIGS_PIN(dev, PINCTRL_STATE_DEFAULT, dev, pin, cfgs)

#define PIN_MAP_CONFIGS_GROUP(dev, state, pinctrl, grp, cfgs)		\
	{								\
		.dev_name = dev,					\
		.name = state,						\
		.type = PIN_MAP_TYPE_CONFIGS_GROUP,			\
		.ctrl_dev_name = pinctrl,				\
		.data.configs = {					\
			.group_or_pin = grp,				\
			.configs = cfgs,				\
			.num_configs = ARRAY_SIZE(cfgs),		\
		},							\
	}

#define PIN_MAP_CONFIGS_GROUP_DEFAULT(dev, pinctrl, grp, cfgs)		\
	PIN_MAP_CONFIGS_GROUP(dev, PINCTRL_STATE_DEFAULT, pinctrl, grp, cfgs)

#define PIN_MAP_CONFIGS_GROUP_HOG(dev, state, grp, cfgs)		\
	PIN_MAP_CONFIGS_GROUP(dev, state, dev, grp, cfgs)

#define PIN_MAP_CONFIGS_GROUP_HOG_DEFAULT(dev, grp, cfgs)		\
	PIN_MAP_CONFIGS_GROUP(dev, PINCTRL_STATE_DEFAULT, dev, grp, cfgs)

#ifdef CONFIG_PINCTRL

extern int pinctrl_register_mappings(struct pinctrl_map const *map,
				unsigned num_maps);
extern void pinctrl_provide_dummies(void);
#else

static inline int pinctrl_register_mappings(struct pinctrl_map const *map,
					   unsigned num_maps)
{
	return 0;
}

static inline void pinctrl_provide_dummies(void)
{
}
#endif 
#endif
