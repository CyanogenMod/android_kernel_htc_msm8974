/*
 * Apple Onboard Audio GPIO definitions
 *
 * Copyright 2006 Johannes Berg <johannes@sipsolutions.net>
 *
 * GPL v2, can be found in COPYING.
 */

#ifndef __AOA_GPIO_H
#define __AOA_GPIO_H
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <asm/prom.h>

typedef void (*notify_func_t)(void *data);

enum notify_type {
	AOA_NOTIFY_HEADPHONE,
	AOA_NOTIFY_LINE_IN,
	AOA_NOTIFY_LINE_OUT,
};

struct gpio_runtime;
struct gpio_methods {
	
	void (*init)(struct gpio_runtime *rt);
	void (*exit)(struct gpio_runtime *rt);

	
	void (*all_amps_off)(struct gpio_runtime *rt);
	
	void (*all_amps_restore)(struct gpio_runtime *rt);

	void (*set_headphone)(struct gpio_runtime *rt, int on);
	void (*set_speakers)(struct gpio_runtime *rt, int on);
	void (*set_lineout)(struct gpio_runtime *rt, int on);
	void (*set_master)(struct gpio_runtime *rt, int on);

	int (*get_headphone)(struct gpio_runtime *rt);
	int (*get_speakers)(struct gpio_runtime *rt);
	int (*get_lineout)(struct gpio_runtime *rt);
	int (*get_master)(struct gpio_runtime *rt);

	void (*set_hw_reset)(struct gpio_runtime *rt, int on);

	int (*set_notify)(struct gpio_runtime *rt,
			  enum notify_type type,
			  notify_func_t notify,
			  void *data);
	int (*get_detect)(struct gpio_runtime *rt,
			  enum notify_type type);
};

struct gpio_notification {
	struct delayed_work work;
	notify_func_t notify;
	void *data;
	void *gpio_private;
	struct mutex mutex;
};

struct gpio_runtime {
	
	struct device_node *node;
	
	struct gpio_methods *methods;
	
	int implementation_private;
	struct gpio_notification headphone_notify;
	struct gpio_notification line_in_notify;
	struct gpio_notification line_out_notify;
};

#endif 
