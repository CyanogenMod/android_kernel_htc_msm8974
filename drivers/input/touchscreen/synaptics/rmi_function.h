/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function Device Header File.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
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

#ifndef _RMI_FUNCTION_H
#define _RMI_FUNCTION_H

#include <linux/input.h>
#include <linux/device.h>


struct rmi_function_info {
	struct rmi_sensor_driver *sensor;

	struct rmi_function_device *function_device;

	unsigned char functionNum;

	
	unsigned char numSources;

	
	unsigned char dataRegBlockSize;

	unsigned char interruptRegister;
	unsigned char interruptMask;

	struct rmi_function_descriptor funcDescriptor;

	
	void *fndata;

	struct list_head link;
};


struct rmi_functions {
	unsigned char functionNum;

	void (*inthandler)(struct rmi_function_info *rfi, unsigned int assertedIRQs);

	int (*config)(struct rmi_function_info *rmifninfo);
	int (*init)(struct rmi_function_device *function_device);
	int (*detect)(struct rmi_function_info *rmifninfo,
		struct rmi_function_descriptor *fndescr,
		unsigned int interruptCount);
	void (*attention)(struct rmi_function_info *rmifninfo);


	struct list_head link;
};


typedef void(*inthandlerFuncPtr)(struct rmi_function_info *rfi, unsigned int assertedIRQs);
typedef int(*configFuncPtr)(struct rmi_function_info *rmifninfo);
typedef int(*initFuncPtr)(struct rmi_function_device *function_device);
typedef int(*detectFuncPtr)(struct rmi_function_info *rmifninfo,
		struct rmi_function_descriptor *fndescr,
		unsigned int interruptCount);
typedef	void (*attnFuncPtr)(struct rmi_function_info *rmifninfo);

struct rmi_functions_data {
	int functionNumber;
	inthandlerFuncPtr inthandlerFn;
	configFuncPtr configFn;
	initFuncPtr initFn;
	detectFuncPtr detectFn;
	attnFuncPtr attnFn;
};


struct rmi_functions *rmi_find_function(int functionNum);
int rmi_functions_init(struct input_dev *inputdev);

struct rmi_function_driver {
	struct module *module;
	struct device_driver drv;

	int (*probe)(struct rmi_function_driver *function);
	void (*config)(struct rmi_function_driver *function);

	unsigned short functionQueryBaseAddr; 
	unsigned short functionControlBaseAddr;
	unsigned short functionCommandBaseAddr;
	unsigned short functionDataBaseAddr;
	unsigned int interruptRegisterOffset; 
	unsigned int interruptMask;

	
	
	
	struct rmi_phys_driver *rpd;

	struct list_head function_drivers; 
};

struct rmi_function_device {
	struct rmi_function_driver *function;
	struct device dev;
	struct input_dev *input;
	struct rmi_sensor_driver *sensor; 

	struct rmi_functions *rmi_funcs;
	struct rmi_function_info *rfi;

	unsigned int irqRegisterSet;

	unsigned int irqMask;

	struct list_head functions; 
};

int rmi_function_register_device(struct rmi_function_device *dev, int fnNumber);

#endif
