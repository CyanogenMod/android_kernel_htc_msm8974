/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function $01 support for sensor
 * control and configuration.
 *
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/param.h>
#include <linux/input/rmi_platformdata.h>
#include <linux/module.h>

#include "rmi.h"
#include "rmi_drvr.h"
#include "rmi_bus.h"
#include "rmi_sensor.h"
#include "rmi_function.h"
#include "rmi_f01.h"

#define F01_CONFIGURED (1 << 7)
#define NONSTANDARD_REPORT_RATE (1 << 6)

#define F01_RESET 1
#define F01_SHUTDOWN (1 << 1)

#define F01_UNCONFIGURED (1 << 7)
#define F01_FLASH_PROGRAMMING_MODE (1 << 6)
#define F01_STATUS_MASK 0x0F

struct f01_instance_data {
	struct rmi_F01_control *controlRegisters;
	struct rmi_F01_data *dataRegisters;
	struct rmi_F01_query *query_registers;

	bool nonstandard_report_rate;
};

static ssize_t rmi_fn_01_productinfo_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_01_productinfo_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(productinfo, 0444, rmi_fn_01_productinfo_show, rmi_fn_01_productinfo_store);     

static ssize_t rmi_fn_01_productid_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_01_productid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(productid, 0444, rmi_fn_01_productid_show, rmi_fn_01_productid_store);     

static ssize_t rmi_fn_01_manufacturer_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_01_manufacturer_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(manufacturer, 0444, rmi_fn_01_manufacturer_show, rmi_fn_01_manufacturer_store);     

static ssize_t rmi_fn_01_datecode_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_01_datecode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(datecode, 0444, rmi_fn_01_datecode_show, rmi_fn_01_datecode_store);     

static ssize_t rmi_fn_01_reportrate_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_01_reportrate_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(reportrate, 0644, rmi_fn_01_reportrate_show, rmi_fn_01_reportrate_store);     

static ssize_t rmi_fn_01_reset_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_01_reset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(reset, 0200, rmi_fn_01_reset_show, rmi_fn_01_reset_store);     

static ssize_t rmi_fn_01_testerid_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_01_testerid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(testerid, 0444, rmi_fn_01_testerid_show, rmi_fn_01_testerid_store);     

static ssize_t rmi_fn_01_serialnumber_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_01_serialnumber_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(serialnumber, 0444, rmi_fn_01_serialnumber_show, rmi_fn_01_serialnumber_store);     

static int set_report_rate(struct rmi_function_info *function_info, bool nonstandard)
{
	if (nonstandard) {
		return rmi_set_bits(function_info->sensor, function_info->funcDescriptor.controlBaseAddr, NONSTANDARD_REPORT_RATE);
	} else {
		return rmi_set_bits(function_info->sensor, function_info->funcDescriptor.controlBaseAddr, NONSTANDARD_REPORT_RATE);
	}
}

void FN_01_inthandler(struct rmi_function_info *rmifninfo,
	unsigned int assertedIRQs)
{
	struct f01_instance_data *instanceData = (struct f01_instance_data *) rmifninfo->fndata;

	printk(KERN_DEBUG "%s: Read device status.", __func__);

	if (rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.dataBaseAddr,
		&instanceData->dataRegisters->deviceStatus, 1)) {
		printk(KERN_ERR "%s : Could not read F01 device status.\n",
			__func__);
	}
	printk(KERN_INFO "%s: read device status register.  Value 0x%02X.", __func__, instanceData->dataRegisters->deviceStatus);

	if (instanceData->dataRegisters->deviceStatus & F01_UNCONFIGURED) {
		printk(KERN_INFO "%s: ++++ Device reset detected.", __func__);
	}
}
EXPORT_SYMBOL(FN_01_inthandler);

void FN_01_attention(struct rmi_function_info *rmifninfo)
{
	struct f01_instance_data *instanceData = (struct f01_instance_data *) rmifninfo->fndata;

	if (rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.dataBaseAddr+1,
		instanceData->dataRegisters->irqs, 1)) {
		printk(KERN_ERR "%s : Could not read interrupt status registers at 0x%02x\n",
			__func__, rmifninfo->funcDescriptor.dataBaseAddr);
		return;
	}

	if (instanceData->dataRegisters->irqs[0] & instanceData->controlRegisters->interruptEnable[0]) {
		
		rmifninfo->sensor->dispatchIRQs(rmifninfo->sensor,
			instanceData->dataRegisters->irqs[0]);
	}

}
EXPORT_SYMBOL(FN_01_attention);

int FN_01_config(struct rmi_function_info *rmifninfo)
{
	int retval = 0;
	struct f01_instance_data *instance_data = rmifninfo->fndata;

	printk(KERN_DEBUG "%s: RMI4 function $01 config\n", __func__);

	retval = rmi_set_bits(rmifninfo->sensor, rmifninfo->funcDescriptor.controlBaseAddr, F01_CONFIGURED);
	if (retval)
		printk(KERN_WARNING "%s: failed to set configured bit, errno = %d.",
				__func__, retval);

	if (instance_data->nonstandard_report_rate) {
		retval = set_report_rate(rmifninfo, true);
		if (!retval)
			printk(KERN_WARNING "%s: failed to configure report rate, errno = %d.",
					__func__, retval);
	}

	

	return retval;
}
EXPORT_SYMBOL(FN_01_config);

int FN_01_init(struct rmi_function_device *function_device)
{
	int retval;
	struct rmi_f01_functiondata *functiondata = rmi_sensor_get_functiondata(function_device->sensor, RMI_F01_INDEX);
	struct f01_instance_data *instance_data = function_device->rfi->fndata;

	pr_debug("%s: RMI4 function $01 init\n", __func__);

	if (functiondata) {
		instance_data->nonstandard_report_rate = functiondata->nonstandard_report_rate;
	}

	retval = device_create_file(&function_device->dev, &dev_attr_productinfo);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create productinfo.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_productid);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create productid.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_manufacturer);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create manufacturer.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_datecode);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create datecode.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_reportrate);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create reportrate.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_reset);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create reset.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_serialnumber);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create serialnumber.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_testerid);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create testerid.", __func__);
		return retval;
	}

	return 0;
}
EXPORT_SYMBOL(FN_01_init);

int FN_01_detect(struct rmi_function_info *rmifninfo,
	struct rmi_function_descriptor *fndescr, unsigned int interruptCount)
{
	int i;
	int InterruptOffset;
	int retval = 0;
	struct f01_instance_data *instanceData = NULL;
	struct rmi_F01_control *controlRegisters = NULL;
	struct rmi_F01_data *dataRegisters = NULL;
	struct rmi_F01_query *query_registers = NULL;
	unsigned char query_buffer[21];

	pr_debug("%s: RMI4 function $01 detect\n", __func__);

	rmifninfo->funcDescriptor.queryBaseAddr = fndescr->queryBaseAddr;
	rmifninfo->funcDescriptor.commandBaseAddr = fndescr->commandBaseAddr;
	rmifninfo->funcDescriptor.controlBaseAddr = fndescr->controlBaseAddr;
	rmifninfo->funcDescriptor.dataBaseAddr = fndescr->dataBaseAddr;
	rmifninfo->funcDescriptor.interruptSrcCnt = fndescr->interruptSrcCnt;
	rmifninfo->funcDescriptor.functionNum = fndescr->functionNum;

	rmifninfo->numSources = fndescr->interruptSrcCnt;

	
	instanceData = kzalloc(sizeof(*instanceData), GFP_KERNEL);
	if (!instanceData) {
		printk(KERN_ERR "%s: Error allocating memory for F01 context data.\n", __func__);
		retval = -ENOMEM;
		goto error_exit;
	}
	query_registers = kzalloc(sizeof(*query_registers), GFP_KERNEL);
	if (!query_registers) {
		printk(KERN_ERR "%s: Error allocating memory for F01 query registers.\n", __func__);
		retval = -ENOMEM;
		goto error_exit;
	}
	instanceData->query_registers = query_registers;

	
	retval = rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.queryBaseAddr,
		query_buffer, 21);
	if (retval) {
		printk(KERN_ERR "%s : Could not read F01 query registers at 0x%02x. Error %d.\n",
			__func__, rmifninfo->funcDescriptor.queryBaseAddr, retval);
		
	}
	query_registers->mfgid = query_buffer[0];
	query_registers->properties = query_buffer[1];
	query_registers->prod_info[0] = query_buffer[2] & 0x7F;
	query_registers->prod_info[1] = query_buffer[3] & 0x7F;
	query_registers->date_code[0] = query_buffer[4] & 0x1F;
	query_registers->date_code[1] = query_buffer[5] & 0x0F;
	query_registers->date_code[2] = query_buffer[6] & 0x1F;
	query_registers->tester_id = (((unsigned short) query_buffer[7] & 0x7F) << 7) | (query_buffer[8] & 0x7F);
	query_registers->serial_num = (((unsigned short) query_buffer[9] & 0x7F) << 7) | (query_buffer[10] & 0x7F);
	memcpy(query_registers->prod_id, &query_buffer[11], 10);

	printk(KERN_DEBUG "%s: RMI4 Protocol Function $01 Query information, rmifninfo->funcDescriptor.queryBaseAddr = %d\n", __func__, rmifninfo->funcDescriptor.queryBaseAddr);
	printk(KERN_DEBUG "%s: Manufacturer ID: %d %s\n", __func__,
		query_registers->mfgid, query_registers->mfgid == 1 ? "(Synaptics)" : "");
	printk(KERN_DEBUG "%s: Product Properties: 0x%x\n",
		__func__, query_registers->properties);
	printk(KERN_DEBUG "%s: Product Info: 0x%x 0x%x\n",
		__func__, query_registers->prod_info[0], query_registers->prod_info[1]);
	printk(KERN_DEBUG "%s: Date Code: Year : %d Month: %d Day: %d\n",
		__func__, query_registers->date_code[0], query_registers->date_code[1],
		query_registers->date_code[2]);
	printk(KERN_DEBUG "%s: Tester ID: %d\n", __func__, query_registers->tester_id);
	printk(KERN_DEBUG "%s: Serial Number: 0x%x\n",
		__func__, query_registers->serial_num);
	printk(KERN_DEBUG "%s: Product ID: %s\n", __func__, query_registers->prod_id);

	controlRegisters = kzalloc(sizeof(*controlRegisters), GFP_KERNEL);
	if (!controlRegisters) {
		printk(KERN_ERR "%s: Error allocating memory for F01 control registers.\n", __func__);
		retval = -ENOMEM;
		goto error_exit;
	}
	instanceData->controlRegisters = controlRegisters;
	retval = rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.controlBaseAddr,
		(char *)instanceData->controlRegisters, sizeof(struct rmi_F01_control));
	if (retval) {
		printk(KERN_ERR "%s : Could not read F01 control registers at 0x%02x. Error %d.\n",
			__func__, rmifninfo->funcDescriptor.controlBaseAddr, retval);
	}

	dataRegisters = kzalloc(sizeof(*dataRegisters), GFP_KERNEL);
	if (!dataRegisters) {
		printk(KERN_ERR "%s: Error allocating memory for F01 data registers.\n", __func__);
		retval = -ENOMEM;
		goto error_exit;
	}
	instanceData->dataRegisters = dataRegisters;
	rmifninfo->fndata = instanceData;

	rmifninfo->interruptRegister = interruptCount/8;

	InterruptOffset = interruptCount % 8;

	for (i = InterruptOffset;
		i < ((fndescr->interruptSrcCnt & 0x7) + InterruptOffset);
		i++) {
			rmifninfo->interruptMask |= 1 << i;
	}

	return retval;

error_exit:
	kfree(instanceData);
	kfree(query_registers);
	kfree(controlRegisters);
	kfree(dataRegisters);
	return retval;
}
EXPORT_SYMBOL(FN_01_detect);

static ssize_t rmi_fn_01_productinfo_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f01_instance_data *instance_data = (struct f01_instance_data *)fn->rfi->fndata;

	if (instance_data && instance_data->query_registers && instance_data->query_registers->prod_info)
		return sprintf(buf, "0x%02X 0x%02X\n", instance_data->query_registers->prod_info[0], instance_data->query_registers->prod_info[1]);

	return sprintf(buf, "unknown");
}

static ssize_t rmi_fn_01_productinfo_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -EPERM;
}


static ssize_t rmi_fn_01_productid_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f01_instance_data *instance_data = (struct f01_instance_data *)fn->rfi->fndata;

	if (instance_data && instance_data->query_registers && instance_data->query_registers->prod_id)
		return sprintf(buf, "%s\n", instance_data->query_registers->prod_id);

	return sprintf(buf, "unknown");
}

static ssize_t rmi_fn_01_productid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t rmi_fn_01_manufacturer_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f01_instance_data *instance_data = (struct f01_instance_data *)fn->rfi->fndata;

	if (instance_data && instance_data->query_registers)
		return sprintf(buf, "0x%02X\n", instance_data->query_registers->mfgid);

	return sprintf(buf, "unknown");
}

static ssize_t rmi_fn_01_manufacturer_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t rmi_fn_01_datecode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f01_instance_data *instance_data = (struct f01_instance_data *)fn->rfi->fndata;

	if (instance_data && instance_data->query_registers && instance_data->query_registers->date_code)
		return sprintf(buf, "20%02u-%02u-%02u\n", instance_data->query_registers->date_code[0], instance_data->query_registers->date_code[1], instance_data->query_registers->date_code[2]);

	return sprintf(buf, "unknown");
}

static ssize_t rmi_fn_01_datecode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t rmi_fn_01_reportrate_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f01_instance_data *instance_data = (struct f01_instance_data *)fn->rfi->fndata;

	if (instance_data && instance_data->query_registers && instance_data->query_registers->date_code)
		return sprintf(buf, "%d\n", instance_data->nonstandard_report_rate);

	return sprintf(buf, "unknown");
}

static ssize_t rmi_fn_01_reportrate_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f01_instance_data *instance_data = (struct f01_instance_data *)fn->rfi->fndata;
	unsigned int new_rate;
	int retval;

	printk(KERN_DEBUG "%s: Report rate set to %s", __func__, buf);

	if (sscanf(buf, "%u", &new_rate) != 1)
		return -EINVAL;
	if (new_rate < 0 || new_rate > 1)
		return -EINVAL;
	instance_data->nonstandard_report_rate = new_rate;

	retval = set_report_rate(fn->rfi, new_rate);
	if (retval < 0) {
		printk(KERN_ERR "%s: failed to set report rate bit, error = %d.", __func__, retval);
		return retval;
	}

	return count;
}

static ssize_t rmi_fn_01_reset_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return -EPERM;
}

static ssize_t rmi_fn_01_reset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	unsigned int reset;
	int retval;

	printk(KERN_INFO "%s: Reset written with %s", __func__, buf);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;
	if (reset < 0 || reset > 1)
		return -EINVAL;

	
	if (reset) {
		retval = rmi_set_bits(fn->sensor, fn->rfi->funcDescriptor.commandBaseAddr, F01_RESET);
		if (retval < 0) {
			printk(KERN_ERR "%s: failed to issue reset command, error = %d.", __func__, retval);
			return retval;
		}
	}

	return count;
}

static ssize_t rmi_fn_01_serialnumber_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f01_instance_data *instance_data = (struct f01_instance_data *)fn->rfi->fndata;

	if (instance_data && instance_data->query_registers)
		return sprintf(buf, "%u\n", instance_data->query_registers->serial_num);

	return sprintf(buf, "unknown");
}

static ssize_t rmi_fn_01_serialnumber_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t rmi_fn_01_testerid_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f01_instance_data *instance_data = (struct f01_instance_data *)fn->rfi->fndata;

	if (instance_data && instance_data->query_registers)
		return sprintf(buf, "%u\n", instance_data->query_registers->tester_id);

	return sprintf(buf, "unknown");
}

static ssize_t rmi_fn_01_testerid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -EPERM;
}
