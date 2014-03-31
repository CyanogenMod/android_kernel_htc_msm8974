/*
 * Apple Onboard Audio definitions
 *
 * Copyright 2006 Johannes Berg <johannes@sipsolutions.net>
 *
 * GPL v2, can be found in COPYING.
 */

#ifndef __AOA_H
#define __AOA_H
#include <asm/prom.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/asound.h>
#include <sound/control.h>
#include "aoa-gpio.h"
#include "soundbus/soundbus.h"

#define MAX_CODEC_NAME_LEN	32

struct aoa_codec {
	char	name[MAX_CODEC_NAME_LEN];

	struct module *owner;

	int (*init)(struct aoa_codec *codec);

	void (*exit)(struct aoa_codec *codec);

	struct device_node *node;

	struct soundbus_dev *soundbus_dev;

	struct gpio_runtime *gpio;

	u32 connected;

	
	void *fabric_data;

	
	struct list_head list;
	struct aoa_fabric *fabric;
};

extern int
aoa_codec_register(struct aoa_codec *codec);
extern void
aoa_codec_unregister(struct aoa_codec *codec);

#define MAX_LAYOUT_NAME_LEN	32

struct aoa_fabric {
	char	name[MAX_LAYOUT_NAME_LEN];

	struct module *owner;

	int (*found_codec)(struct aoa_codec *codec);
	void (*remove_codec)(struct aoa_codec *codec);
	void (*attached_codec)(struct aoa_codec *codec);
};

extern int
aoa_fabric_register(struct aoa_fabric *fabric, struct device *dev);

extern void
aoa_fabric_unregister(struct aoa_fabric *fabric);

extern void
aoa_fabric_unlink_codec(struct aoa_codec *codec);

struct aoa_card {
	struct snd_card *alsa_card;
};
        
extern int aoa_snd_device_new(snd_device_type_t type,
	void * device_data, struct snd_device_ops * ops);
extern struct snd_card *aoa_get_card(void);
extern int aoa_snd_ctl_add(struct snd_kcontrol* control);

extern struct gpio_methods *pmf_gpio_methods;
extern struct gpio_methods *ftr_gpio_methods;

#endif 
