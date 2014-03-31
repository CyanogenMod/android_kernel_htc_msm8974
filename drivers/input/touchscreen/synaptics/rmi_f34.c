/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function $34 support for sensor
 * firmware reflashing.
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
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/input.h>
#include <linux/sysfs.h>
#include <linux/math64.h>
#include <linux/module.h>
#include "rmi_drvr.h"
#include "rmi_bus.h"
#include "rmi_sensor.h"
#include "rmi_function.h"
#include "rmi_f34.h"

struct rmi_fn_34_data {
	unsigned char   status;
	unsigned char   cmd;
	unsigned short  bootloaderid;
	unsigned short  blocksize;
};


static ssize_t rmi_fn_34_status_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_34_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);


static ssize_t rmi_fn_34_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_34_cmd_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t rmi_fn_34_data_read(struct file *,
				struct kobject *kobj,
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count);

static ssize_t rmi_fn_34_data_write(struct file *,
				struct kobject *kobj,
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count);

static ssize_t rmi_fn_34_bootloaderid_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_34_bootloaderid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t rmi_fn_34_blocksize_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_34_blocksize_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(status, 0444, rmi_fn_34_status_show, rmi_fn_34_status_store);  
DEVICE_ATTR(cmd, 0664, rmi_fn_34_cmd_show, rmi_fn_34_cmd_store);     
DEVICE_ATTR(bootloaderid, 0644, rmi_fn_34_bootloaderid_show, rmi_fn_34_bootloaderid_store); 
DEVICE_ATTR(blocksize, 0444, rmi_fn_34_blocksize_show, rmi_fn_34_blocksize_store);    


struct bin_attribute dev_attr_data = {
	.attr = {
		.name = "data",
		.mode = 0644
	},
	.size = 0,
	.read = rmi_fn_34_data_read,
	.write = rmi_fn_34_data_write,
};

void copyEndianAgnostic(unsigned char *dest, unsigned short src)
{
	dest[0] = src%0x100;
	dest[1] = src/0x100;
}

void FN_34_inthandler(struct rmi_function_info *rmifninfo,
	unsigned int assertedIRQs)
{
	unsigned int status;
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)rmifninfo->fndata;

	
	
	if (rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.dataBaseAddr+3,
		(unsigned char *)&status, 1)) {
		printk(KERN_ERR "%s : Could not read status from 0x%x\n",
			__func__, rmifninfo->funcDescriptor.dataBaseAddr+3);
		status = 0xff; 
	}

	
	fn34data->status = status & 0xf0; 
}
EXPORT_SYMBOL(FN_34_inthandler);

void FN_34_attention(struct rmi_function_info *rmifninfo)
{

}
EXPORT_SYMBOL(FN_34_attention);

int FN_34_config(struct rmi_function_info *rmifninfo)
{
	pr_debug("%s: RMI4 function $34 config\n", __func__);
	return 0;
}
EXPORT_SYMBOL(FN_34_config);


int FN_34_init(struct rmi_function_device *function_device)
{
	int retval = 0;
	unsigned char uData[2];
	struct rmi_function_info *rmifninfo = function_device->rfi;
	struct rmi_fn_34_data *fn34data;

	pr_debug("%s: RMI4 function $34 init\n", __func__);

	
	fn34data = kzalloc(sizeof(struct rmi_fn_34_data), GFP_KERNEL);
	if (!fn34data) {
		printk(KERN_ERR "%s: Error allocating memeory for rmi_fn_34_data.\n", __func__);
		return -ENOMEM;
	}
	rmifninfo->fndata = (void *)fn34data;

	
	if (sysfs_create_file(&function_device->dev.kobj, &dev_attr_bootloaderid.attr) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 bootloaderid.\n", __func__);
		return -ENODEV;
	}

	
	if (sysfs_create_file(&function_device->dev.kobj, &dev_attr_blocksize.attr) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 blocksize.\n", __func__);
		return -ENODEV;
	}

	
	retval = rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.queryBaseAddr,
		uData, 2);
	if (retval) {
		printk(KERN_ERR "%s : Could not read bootloaderid from 0x%x\n",
			__func__, function_device->function->functionQueryBaseAddr);
		return retval;
	}
	
	fn34data->bootloaderid = (unsigned int)uData[0] + (unsigned int)uData[1]*0x100;

	retval = rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.queryBaseAddr+3,
		uData, 2);
	if (retval) {
		printk(KERN_ERR "%s : Could not read block size from 0x%x\n",
			__func__, rmifninfo->funcDescriptor.queryBaseAddr+3);
		return retval;
	}
	
	fn34data->blocksize = (unsigned int)uData[0] + (unsigned int)uData[1]*0x100;

	
	if (sysfs_create_file(&function_device->dev.kobj, &dev_attr_status.attr) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 status.\n", __func__);
		return -ENODEV;
	}

	
	if (sysfs_create_file(&function_device->dev.kobj, &dev_attr_cmd.attr) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 cmd.\n", __func__);
		return -ENODEV;
	}

	
	if (sysfs_create_bin_file(&function_device->dev.kobj, &dev_attr_data) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 data.\n", __func__);
		return -ENODEV;
	}

	return retval;
}
EXPORT_SYMBOL(FN_34_init);

int FN_34_detect(struct rmi_function_info *rmifninfo,
	struct rmi_function_descriptor *fndescr, unsigned int interruptCount)
{
	int i;
	int InterruptOffset;
	int retval = 0;

	pr_debug("%s: RMI4 function $34 detect\n", __func__);
	if (rmifninfo->sensor == NULL) {
		printk(KERN_ERR "%s: NULL sensor passed in!", __func__);
		return -EINVAL;
	}

	rmifninfo->funcDescriptor.queryBaseAddr = fndescr->queryBaseAddr;
	rmifninfo->funcDescriptor.commandBaseAddr = fndescr->commandBaseAddr;
	rmifninfo->funcDescriptor.controlBaseAddr = fndescr->controlBaseAddr;
	rmifninfo->funcDescriptor.dataBaseAddr = fndescr->dataBaseAddr;
	rmifninfo->funcDescriptor.interruptSrcCnt = fndescr->interruptSrcCnt;
	rmifninfo->funcDescriptor.functionNum = fndescr->functionNum;

	rmifninfo->numSources = fndescr->interruptSrcCnt;

	rmifninfo->interruptRegister = interruptCount/8;

	InterruptOffset = interruptCount % 8;

	for (i = InterruptOffset;
		i < ((fndescr->interruptSrcCnt & 0x7) + InterruptOffset);
		i++) {
			rmifninfo->interruptMask |= 1 << i;
	}

	return retval;
}
EXPORT_SYMBOL(FN_34_detect);

static ssize_t rmi_fn_34_bootloaderid_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", fn34data->bootloaderid);
}

static ssize_t rmi_fn_34_bootloaderid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int error;
	unsigned long val;
	unsigned char uData[2];
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	
	error = strict_strtoul(buf, 10, &val);

	if (error)
		return error;

	fn34data->bootloaderid = val;

	copyEndianAgnostic(uData, (unsigned short)val);
	error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr,
		uData, 2);
	if (error) {
		printk(KERN_ERR "%s : Could not write bootloader id to 0x%x\n",
			__func__, fn->function->functionDataBaseAddr);
		return error;
	}

	return count;
}

static ssize_t rmi_fn_34_blocksize_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", fn34data->blocksize);
}

static ssize_t rmi_fn_34_blocksize_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{

	return -EPERM;
}

static ssize_t rmi_fn_34_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", fn34data->status);
}

static ssize_t rmi_fn_34_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t rmi_fn_34_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", fn34data->cmd);
}

static ssize_t rmi_fn_34_cmd_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;
	unsigned long val;
	unsigned char cmd;
	int error;

	
	error = strict_strtoul(buf, 10, &val);

	if (error)
		return error;

	fn34data->cmd = val;

	switch (val) {
	case ENABLE_FLASH_PROG:
		cmd = 0x0F;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Flash Program Enable cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case ERASE_ALL:
		cmd = 0x03;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Erase All cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case ERASE_CONFIG:
		cmd = 0x07;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Erase Configuration cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case WRITE_FW_BLOCK:
		cmd = 0x02;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Write Firmware Block cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
		return error;
		}
		break;

	case WRITE_CONFIG_BLOCK:
		cmd = 0x06;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Write Config Block cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case READ_CONFIG_BLOCK:
		cmd = 0x05;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Read Config Block cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case DISABLE_FLASH_PROG:
		cmd = 0x01;
		break;

	default:
		pr_debug("%s: RMI4 function $34 - unknown command.\n", __func__);
		break;
	}

	return count;
}

static ssize_t rmi_fn_34_data_read(struct file * filp,
				struct kobject *kobj,
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	int error;

	

	
	
	
	error = rmi_read_multiple(fn->sensor, fn->function->functionDataBaseAddr+pos,
		(unsigned char *)buf, count);
	if (error) {
		printk(KERN_ERR "%s : Could not read data from 0x%llx\n",
			__func__, fn->function->functionDataBaseAddr+pos);
		return error;
	}

	return count;
}

static ssize_t rmi_fn_34_data_write(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;
	unsigned int blocknum;
	int error;

	
	
	

	

	unsigned int remainder;
	div_u64_rem(pos, fn34data->blocksize, &remainder);
	if (remainder) {
		printk(KERN_ERR "%s : Invalid byte offset of %llx leads to invalid block number.\n",
			__func__, pos);
		return -EINVAL;
	}

	blocknum = div_u64(pos, fn34data->blocksize);

	
	error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr,
		(unsigned char *)&blocknum, 2);
	if (error) {
		printk(KERN_ERR "%s : Could not write block number to 0x%x\n",
			__func__, fn->function->functionDataBaseAddr);
		return error;
	}

	
	if (count) {
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+2,
			(unsigned char *)buf, count);
		if (error) {
			printk(KERN_ERR "%s : Could not write block data to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+2);
			return error;
		}
	}

	return count;
}
