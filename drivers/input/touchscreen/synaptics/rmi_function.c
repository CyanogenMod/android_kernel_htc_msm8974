/**
 * Synaptics Register Mapped Interface (RMI4) - RMI Function Module.
 * Copyright (C) 2007 - 2011, Synaptics Incorporated
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *#############################################################################
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
 *#############################################################################
 */

static const char functionname[10] = "fn";

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include "rmi_drvr.h"
#include "rmi_function.h"
#include "rmi_bus.h"
#include "rmi_sensor.h"
#include "rmi_f01.h"
#include "rmi_f05.h"
#include "rmi_f11.h"
#include "rmi_f19.h"
#include "rmi_f34.h"

#define rmi4_num_supported_data_src_fns 5

static LIST_HEAD(fns_list);
static DEFINE_MUTEX(fns_mutex);

static struct rmi_functions_data
	rmi4_supported_data_src_functions[rmi4_num_supported_data_src_fns] = {
	
	{.functionNumber = 0x11, .inthandlerFn = FN_11_inthandler, .configFn = FN_11_config, .initFn = FN_11_init, .detectFn = FN_11_detect, .attnFn = NULL},
	
	{.functionNumber = 0x01, .inthandlerFn = FN_01_inthandler, .configFn = FN_01_config, .initFn = FN_01_init, .detectFn = FN_01_detect, .attnFn = FN_01_attention},
	
	{.functionNumber = 0x05, .inthandlerFn = FN_05_inthandler, .configFn = FN_05_config, .initFn = FN_05_init, .detectFn = FN_05_detect, .attnFn = NULL},
	
	{.functionNumber = 0x19, .inthandlerFn = FN_19_inthandler, .configFn = FN_19_config, .initFn = FN_19_init, .detectFn = FN_19_detect, .attnFn = NULL},
	
	{.functionNumber = 0x34, .inthandlerFn = FN_34_inthandler, .configFn = FN_34_config, .initFn = FN_34_init, .detectFn = FN_34_detect, .attnFn = FN_34_attention},
};


struct rmi_functions *rmi_find_function(int functionNum)
{
	struct rmi_functions *fn;
	bool found = false;

	list_for_each_entry(fn, &fns_list, link) {
		if (functionNum == fn->functionNum) {
			found = true;
			break;
		}
	}

	if (!found)
		return NULL;
	else
		return fn;
}
EXPORT_SYMBOL(rmi_find_function);


static void rmi_function_config(struct rmi_function_device *function)
{
	printk(KERN_DEBUG "%s: rmi_function_config", __func__);

}

#if 0 
static int rmi_function_probe(struct rmi_function_driver *function)
{
	struct rmi_phys_driver *rpd;

	rpd = function->rpd;

	if (!rpd) {
		printk(KERN_ERR "%s: Invalid rmi physical driver - null ptr.", __func__);
		return 0;
	}

	return 1;
}
#endif

static int rmi_function_suspend(struct device *dev, pm_message_t state)
{
	printk(KERN_INFO "%s: function suspend called.", __func__);
	return 0;
}

static int rmi_function_resume(struct device *dev)
{
	printk(KERN_INFO "%s: function resume called.", __func__);
	return 0;
}

int rmi_function_register_driver(struct rmi_function_driver *drv, int fnNumber)
{
	int retval;
	char *drvrname;

	printk(KERN_INFO "%s: Registering function driver for F%02x.\n", __func__, fnNumber);

	retval = 0;

	
	drvrname = kzalloc(sizeof(functionname) + 4, GFP_KERNEL);
	if (!drvrname) {
		printk(KERN_ERR "%s: Error allocating memeory for rmi_function_driver name.\n", __func__);
		return -ENOMEM;
	}
	sprintf(drvrname, "fn%02x", fnNumber);

	drv->drv.name = drvrname;
	drv->module = drv->drv.owner;

	drv->drv.suspend = rmi_function_suspend;
	drv->drv.resume = rmi_function_resume;

	
	retval = driver_register(&drv->drv);
	if (retval) {
		printk(KERN_ERR "%s: Failed driver_register %d\n",
			__func__, retval);
	}

	return retval;
}
EXPORT_SYMBOL(rmi_function_register_driver);

void rmi_function_unregister_driver(struct rmi_function_driver *drv)
{
	printk(KERN_INFO "%s: Unregistering function driver.\n", __func__);

	driver_unregister(&drv->drv);
}
EXPORT_SYMBOL(rmi_function_unregister_driver);

int rmi_function_register_device(struct rmi_function_device *function_device, int fnNumber)
{
	struct input_dev *input;
	int retval;

	printk(KERN_INFO "%s: Registering function device for F%02x.\n", __func__, fnNumber);

	retval = 0;

	
	dev_set_name(&function_device->dev, "%sfn%02x", function_device->sensor->drv.name, fnNumber);
	dev_set_drvdata(&function_device->dev, function_device);
	retval = device_register(&function_device->dev);
	if (retval) {
		printk(KERN_ERR "%s:  Failed device_register for function device.\n",
			__func__);
		return retval;
	}

	input = input_allocate_device();
	if (input == NULL) {
		printk(KERN_ERR "%s:  Failed to allocate memory for a "
			"new input device.\n",
			__func__);
		return -ENOMEM;
	}

	input->name = dev_name(&function_device->dev);
	input->phys = "rmi_function";
	function_device->input = input;


	
	function_device->rmi_funcs->init(function_device);

	retval = input_register_device(input);

	if (retval) {
		printk(KERN_ERR "%s:  Failed input_register_device.\n",
			__func__);
		return retval;
	}


	rmi_function_config(function_device);

	return retval;
}
EXPORT_SYMBOL(rmi_function_register_device);

void rmi_function_unregister_device(struct rmi_function_device *dev)
{
	printk(KERN_INFO "%s: Unregistering function device.n", __func__);

	input_unregister_device(dev->input);
	device_unregister(&dev->dev);
}
EXPORT_SYMBOL(rmi_function_unregister_device);

static int __init rmi_function_init(void)
{
	struct rmi_functions_data *rmi4_fn;
	int i;

	printk(KERN_DEBUG "%s: RMI Function Init\n", __func__);


	for (i = 0; i < rmi4_num_supported_data_src_fns; i++) {
		
		struct rmi_functions *fn = kzalloc(sizeof(*fn), GFP_KERNEL);
		if (!fn) {
			printk(KERN_ERR "%s: could not allocate memory "
				"for rmi_function struct for function 0x%x\n",
				__func__,
				rmi4_supported_data_src_functions[i].functionNumber);
			return -ENOMEM;
		} else {

			rmi4_fn = &rmi4_supported_data_src_functions[i];
			fn->functionNum = rmi4_fn->functionNumber;
			fn->inthandler = rmi4_fn->inthandlerFn;
			fn->config = rmi4_fn->configFn;
			fn->init =   rmi4_fn->initFn;
			fn->detect = rmi4_fn->detectFn;
			fn->attention = rmi4_fn->attnFn;

			
			mutex_lock(&fns_mutex);
			list_add_tail(&fn->link, &fns_list);
			mutex_unlock(&fns_mutex);
		}
	}

	return 0;
}

static void __exit rmi_function_exit(void)
{
	printk(KERN_DEBUG "%s: RMI Function Exit\n", __func__);
}


module_init(rmi_function_init);
module_exit(rmi_function_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("RMI4 Function Driver");
MODULE_LICENSE("GPL");
