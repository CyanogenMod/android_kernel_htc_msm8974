/**
 *
 * Synaptics Register Mapped Interface (RMI4) - RMI Sensor Module Header.
 * Copyright (C) 2007 - 2011, Synaptics Incorporated
 *
 */
/*
 *
 * This file is licensed under the GPL2 license.
 *
 *############################################################################
 * GPL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 *############################################################################
 */

#include <linux/device.h>

#ifndef _RMI_SENSOR_H
#define _RMI_SENSOR_H

#include <linux/input/rmi_platformdata.h>

struct rmi_sensor_driver {
	struct module *module;
	struct device_driver drv;
	struct rmi_sensor_device *sensor_device;

	void (*attention)(struct rmi_phys_driver *physdrvr, int instance);
	int (*probe)(struct rmi_sensor_driver *sensor);
	void (*config)(struct rmi_sensor_driver *sensor);

	
	void (*dispatchIRQs)(struct rmi_sensor_driver *sensor,
		unsigned int irqStatus);

	void (*rmi_sensor_register_functions)(struct rmi_sensor_driver
							*sensor);

	unsigned int interruptRegisterCount;

	bool polling_required;

	
	
	struct rmi_phys_driver *rpd;

	struct hrtimer timer;
	struct work_struct work;
	bool workIsReady;


	struct list_head sensor_drivers; 

	struct list_head functions;     
	
	struct rmi_functiondata_list *perfunctiondata;
};

#define to_rmi_sensor_driver(drv) container_of(drv, \
		struct rmi_sensor_driver, drv);

struct rmi_sensor_device {
	struct rmi_sensor_driver *driver;
	struct device dev;

	struct list_head sensors; 
};

int rmi_sensor_register_device(struct rmi_sensor_device *dev, int index);
int rmi_sensor_register_driver(struct rmi_sensor_driver *driver);
int rmi_sensor_register_functions(struct rmi_sensor_driver *sensor);
bool rmi_polling_required(struct rmi_sensor_driver *sensor);

static inline void *rmi_sensor_get_functiondata(struct rmi_sensor_driver
		*driver, unsigned char function_index)
{
	int i;
	if (driver->perfunctiondata) {
		for (i = 0; i < driver->perfunctiondata->count; i++) {
			if (driver->perfunctiondata->functiondata[i].
					function_index == function_index)
				return driver->perfunctiondata->
					functiondata[i].data;
		}
	}
	return NULL;
}

#endif
