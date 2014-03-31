/*
 * Copyright (C) ST-Ericsson SA 2011
 * Author: Lee Jones <lee.jones@linaro.org> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */
#ifndef __SOC_BUS_H
#define __SOC_BUS_H

#include <linux/device.h>

struct soc_device_attribute {
	const char *machine;
	const char *family;
	const char *revision;
	const char *soc_id;
};

struct soc_device *soc_device_register(
	struct soc_device_attribute *soc_plat_dev_attr);

void soc_device_unregister(struct soc_device *soc_dev);

struct device *soc_device_to_device(struct soc_device *soc);

#endif 
