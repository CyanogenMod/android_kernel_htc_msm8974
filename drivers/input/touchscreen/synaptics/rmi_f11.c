/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function $11 support for 2D.
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
#include <linux/input/rmi_platformdata.h>
#include <linux/module.h>

#include "rmi.h"
#include "rmi_drvr.h"
#include "rmi_bus.h"
#include "rmi_sensor.h"
#include "rmi_function.h"
#include "rmi_f11.h"

static int sensorMaxX;
static int sensorMaxY;

struct f11_instance_data {
	struct rmi_F11_device_query *deviceInfo;
	struct rmi_F11_sensor_query *sensorInfo;
	struct rmi_F11_control *controlRegisters;
	int button_height;
	unsigned char fingerDataBufferSize;
	unsigned char absDataOffset;
	unsigned char absDataSize;
	unsigned char relDataOffset;
	unsigned char gestureDataOffset;
	unsigned char *fingerDataBuffer;
		
		
	bool wasdown;
	unsigned int oldX;
	unsigned int oldY;
		
	bool flipX;
	bool flipY;
	int offsetX;
	int offsetY;
	int clipXLow;
	int clipXHigh;
	int clipYLow;
	int clipYHigh;
	bool swap_axes;
	bool relReport;
};

enum f11_finger_state {
	F11_NO_FINGER = 0,
	F11_PRESENT = 1,
	F11_INACCURATE = 2,
	F11_RESERVED = 3
};

static ssize_t rmi_fn_11_flip_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_11_flip_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(flip, 0664, rmi_fn_11_flip_show, rmi_fn_11_flip_store);     

static ssize_t rmi_fn_11_clip_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_11_clip_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(clip, 0664, rmi_fn_11_clip_show, rmi_fn_11_clip_store);     

static ssize_t rmi_fn_11_offset_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_11_offset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(offset, 0664, rmi_fn_11_offset_show, rmi_fn_11_offset_store);     

static ssize_t rmi_fn_11_swap_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_11_swap_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(swap, 0664, rmi_fn_11_swap_show, rmi_fn_11_swap_store);     

static ssize_t rmi_fn_11_relreport_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_11_relreport_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(relreport, 0664, rmi_fn_11_relreport_show, rmi_fn_11_relreport_store);     

static ssize_t rmi_fn_11_maxPos_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_11_maxPos_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(maxPos, 0664, rmi_fn_11_maxPos_show, rmi_fn_11_maxPos_store);     


static void FN_11_relreport(struct rmi_function_info *rmifninfo);



void FN_11_inthandler(struct rmi_function_info *rmifninfo,
	unsigned int assertedIRQs)
{
	
	int fingerDownCount;
	int finger;
	struct rmi_function_device *function_device;
	struct f11_instance_data *instanceData;

	instanceData = (struct f11_instance_data *) rmifninfo->fndata;

	fingerDownCount = 0;
	function_device = rmifninfo->function_device;

	

	if (rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.dataBaseAddr,
		instanceData->fingerDataBuffer, instanceData->fingerDataBufferSize)) {
		printk(KERN_ERR "%s: Failed to read finger data registers.\n", __func__);
		return;
	}

	
	for (finger = 0; finger < instanceData->sensorInfo->numberOfFingers; finger++) {
		int reg;
		int fingerShift;
		int fingerStatus;

		
		reg = finger/4;
		
		fingerShift = (finger % 4) * 2;
		fingerStatus = (instanceData->fingerDataBuffer[reg] >> fingerShift) & 3;

		if (fingerStatus == F11_PRESENT || fingerStatus == F11_INACCURATE) {
			fingerDownCount++;
			instanceData->wasdown = true;
		}
	}
	input_report_key(function_device->input,
			BTN_TOUCH, !!fingerDownCount);

	for (finger = 0; finger < instanceData->sensorInfo->numberOfFingers; finger++) {
		int reg;
		int fingerShift;
		int fingerStatus;
		int X = 0, Y = 0, Z = 0, Wy = 0, Wx = 0;

		
		reg = finger/4;
		
		fingerShift = (finger % 4) * 2;
		fingerStatus = (instanceData->fingerDataBuffer[reg] >> fingerShift) & 3;

		if (fingerStatus == F11_PRESENT || fingerStatus == F11_INACCURATE) {

			if (instanceData->sensorInfo->hasAbs) {
				int maxX = instanceData->controlRegisters->sensorMaxXPos;
				int maxY = instanceData->controlRegisters->sensorMaxYPos;
				reg = instanceData->absDataOffset + (finger * instanceData->absDataSize);
				X = (instanceData->fingerDataBuffer[reg] << 4) & 0x0ff0;
				X |= (instanceData->fingerDataBuffer[reg+2] & 0x0f);
				Y = (instanceData->fingerDataBuffer[reg+1] << 4) & 0x0ff0;
				Y |= ((instanceData->fingerDataBuffer[reg+2] & 0xf0) >> 4) & 0x0f;
				if (instanceData->swap_axes) {
					int temp = X;
					X = Y;
					Y = temp;
					maxX = instanceData->controlRegisters->sensorMaxYPos;
					maxY = instanceData->controlRegisters->sensorMaxXPos;
				}
				if (instanceData->flipX)
					X = max(maxX-X, 0);
				X = X - instanceData->offsetX;
				X = min(max(X, instanceData->clipXLow), instanceData->clipXHigh);
				if (instanceData->flipY)
					Y = max(maxY-Y, 0);
				Y = Y - instanceData->offsetY;
				Y = min(max(Y, instanceData->clipYLow), instanceData->clipYHigh);

				Wy =  (instanceData->fingerDataBuffer[reg+3] >> 4) & 0x0f;
				Wx = instanceData->fingerDataBuffer[reg+3] & 0x0f;
				if (instanceData->swap_axes) {
					int temp = Wx;
					Wx = Wy;
					Wy = temp;
				}

				Z = instanceData->fingerDataBuffer[reg+4];

				if (fingerDownCount == 1) {
					instanceData->oldX = X;
					instanceData->oldY = Y;
					input_report_abs(function_device->input, ABS_X, X);
					input_report_abs(function_device->input, ABS_Y, Y);
					input_report_abs(function_device->input, ABS_PRESSURE, Z);
					input_report_abs(function_device->input, ABS_TOOL_WIDTH,
							max(Wx, Wy));

				} else {
					
				}
#ifdef CONFIG_SYNA_MULTI_TOUCH
				
				input_report_abs(function_device->input,
							ABS_MT_PRESSURE, Z);
				input_report_abs(function_device->input, ABS_MT_POSITION_X, X);
				input_report_abs(function_device->input, ABS_MT_POSITION_Y, Y);

				
				
				
				input_report_abs(function_device->input, ABS_MT_TRACKING_ID,
						finger);
				
				input_mt_sync(function_device->input);
#endif
			}
		}
	}

	
	if ((fingerDownCount == 0) && instanceData->wasdown) {
		instanceData->wasdown = false;

#ifdef CONFIG_SYNA_MULTI_TOUCH
		input_report_abs(function_device->input, ABS_MT_PRESSURE, 0);
		input_report_key(function_device->input, BTN_TOUCH, 0);
		input_mt_sync(function_device->input);
#endif
	}

	FN_11_relreport(rmifninfo);
	input_sync(function_device->input); 

}
EXPORT_SYMBOL(FN_11_inthandler);

static void FN_11_relreport(struct rmi_function_info *rmifninfo)
{
	struct f11_instance_data *instanceData;
	struct rmi_function_device *function_device;
	signed char X, Y;
	unsigned short fn11DataBaseAddr;

	instanceData = (struct f11_instance_data *) rmifninfo->fndata;

	if (instanceData->sensorInfo->hasRel && instanceData->relReport) {
		int reg = instanceData->relDataOffset;

		function_device = rmifninfo->function_device;

		fn11DataBaseAddr = rmifninfo->funcDescriptor.dataBaseAddr;
		
		X = instanceData->fingerDataBuffer[reg];
		Y = instanceData->fingerDataBuffer[reg+1];
		if (instanceData->swap_axes) {
			signed char temp = X;
			X = Y;
			Y = temp;
		}
		if (instanceData->flipX) {
			X = -X;
		}
		if (instanceData->flipY) {
			Y = -Y;
		}
		X = (signed char) min(127, max(-128, (int) X));
		Y = (signed char) min(127, max(-128, (int) Y));

		input_report_rel(function_device->input, REL_X, X);
		input_report_rel(function_device->input, REL_Y, Y);
	}
}

int FN_11_config(struct rmi_function_info *rmifninfo)
{
	unsigned char data[14];
	int retval = 0;

	pr_debug("%s: RMI4 function $11 config\n", __func__);

	

	retval = rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.queryBaseAddr,
		data, 9);
	if (retval) {
		printk(KERN_ERR "%s: RMI4 function $11 config:"
			"Could not read function query registers 0x%x\n",
			__func__, rmifninfo->funcDescriptor.queryBaseAddr);
	} else {
		pr_debug("%s:  Number of Fingers:   %d\n",
				__func__, data[1] & 7);
		pr_debug("%s:  Is Configurable:     %d\n",
				__func__, data[1] & (1 << 7) ? 1 : 0);
		pr_debug("%s:  Has Gestures:        %d\n",
				__func__, data[1] & (1 << 5) ? 1 : 0);
		pr_debug("%s:  Has Absolute:        %d\n",
				__func__, data[1] & (1 << 4) ? 1 : 0);
		pr_debug("%s:  Has Relative:        %d\n",
				__func__, data[1] & (1 << 3) ? 1 : 0);

		pr_debug("%s:  Number X Electrodes: %d\n",
				__func__, data[2] & 0x1f);
		pr_debug("%s:  Number Y Electrodes: %d\n",
				__func__, data[3] & 0x1f);
		pr_debug("%s:  Maximum Electrodes:  %d\n",
				__func__, data[4] & 0x1f);

		pr_debug("%s:  Absolute Data Size:  %d\n",
				__func__, data[5] & 3);

		pr_debug("%s:  Has XY Dist:         %d\n",
				__func__, data[7] & (1 << 7) ? 1 : 0);
		pr_debug("%s:  Has Pinch:           %d\n",
				__func__, data[7] & (1 << 6) ? 1 : 0);
		pr_debug("%s:  Has Press:           %d\n",
				__func__, data[7] & (1 << 5) ? 1 : 0);
		pr_debug("%s:  Has Flick:           %d\n",
				__func__, data[7] & (1 << 4) ? 1 : 0);
		pr_debug("%s:  Has Early Tap:       %d\n",
				__func__, data[7] & (1 << 3) ? 1 : 0);
		pr_debug("%s:  Has Double Tap:      %d\n",
				__func__, data[7] & (1 << 2) ? 1 : 0);
		pr_debug("%s:  Has Tap and Hold:    %d\n",
				__func__, data[7] & (1 << 1) ? 1 : 0);
		pr_debug("%s:  Has Tap:             %d\n",
				__func__, data[7] & 1 ? 1 : 0);
		pr_debug("%s:  Has Palm Detect:     %d\n",
				__func__, data[8] & 1 ? 1 : 0);
		pr_debug("%s:  Has Rotate:          %d\n",
				__func__, data[8] & (1 << 1) ? 1 : 0);

		retval = rmi_read_multiple(rmifninfo->sensor,
				rmifninfo->funcDescriptor.controlBaseAddr, data, 14);
		if (retval) {
			printk(KERN_ERR "%s: RMI4 function $11 config:"
				"Could not read control registers 0x%x\n",
				__func__, rmifninfo->funcDescriptor.controlBaseAddr);
			return retval;
		}

		
		sensorMaxX = ((data[6] & 0x1f) << 8) | ((data[7] & 0xff) << 0);
		sensorMaxY = ((data[8] & 0x1f) << 8) | ((data[9] & 0xff) << 0);

		pr_debug("%s:  Sensor Max X:  %d\n", __func__, sensorMaxX);
		pr_debug("%s:  Sensor Max Y:  %d\n", __func__, sensorMaxY);
	}

	return retval;
}
EXPORT_SYMBOL(FN_11_config);

static void f11_set_abs_params(struct rmi_function_device *function_device)
{
	struct f11_instance_data *instance_data = function_device->rfi->fndata;
	int xMin = instance_data->clipXLow;
	int xMax = min((int) instance_data->controlRegisters->sensorMaxXPos, instance_data->clipXHigh);
	int yMin = instance_data->clipYLow;
	int yMax = min((int) instance_data->controlRegisters->sensorMaxYPos, instance_data->clipYHigh) - instance_data->button_height;
	if (instance_data->swap_axes) {
		int temp = xMin;
		xMin = yMin;
		yMin = temp;
		temp = xMax;
		xMax = yMax;
		yMax = temp;
	}
	printk(KERN_DEBUG "%s: Set ranges X=[%d..%d] Y=[%d..%d].", __func__, xMin, xMax, yMin, yMax);
	input_set_abs_params(function_device->input, ABS_X, xMin, xMax,
		0, 0);
	input_set_abs_params(function_device->input, ABS_Y, yMin, yMax,
		0, 0);
	input_set_abs_params(function_device->input, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(function_device->input, ABS_TOOL_WIDTH, 0, 15, 0, 0);

#ifdef CONFIG_SYNA_MULTI_TOUCH
	input_set_abs_params(function_device->input, ABS_MT_PRESSURE,
							0, 15, 0, 0);
	input_set_abs_params(function_device->input, ABS_MT_ORIENTATION, 0, 1, 0, 0);
	input_set_abs_params(function_device->input, ABS_MT_TRACKING_ID,
							0, 10, 0, 0);
	input_set_abs_params(function_device->input, ABS_MT_POSITION_X, xMin, xMax,
		0, 0);
	input_set_abs_params(function_device->input, ABS_MT_POSITION_Y, yMin, yMax,
		0, 0);
#endif
}

int FN_11_init(struct rmi_function_device *function_device)
{
	struct f11_instance_data *instanceData = function_device->rfi->fndata;
	int retval = 0;
	struct rmi_f11_functiondata *functiondata = rmi_sensor_get_functiondata(function_device->sensor, RMI_F11_INDEX);
	printk(KERN_DEBUG "%s: RMI4 F11 init", __func__);

	instanceData->flipX = false;
	instanceData->flipY = false;
	instanceData->swap_axes = false;
	instanceData->relReport = true;
	instanceData->offsetX = instanceData->offsetY = 0;
	instanceData->clipXLow = instanceData->clipYLow = 0;
	
	instanceData->clipXHigh = instanceData->clipYHigh = 65536;

	if (functiondata) {
		printk(KERN_DEBUG "%s: found F11 per function platformdata.", __func__);
		instanceData->flipX = functiondata->flipX;
		instanceData->flipY = functiondata->flipY;
		instanceData->button_height = functiondata->button_height;
		instanceData->swap_axes = functiondata->swap_axes;
		if (functiondata->offset) {
			instanceData->offsetX = functiondata->offset->x;
			instanceData->offsetY = functiondata->offset->y;
		}
		if (functiondata->clipX) {
			if (functiondata->clipX->min >= functiondata->clipX->max) {
				printk(KERN_WARNING "%s: Clip X min (%d) >= X clip max (%d) - ignored.",
					   __func__, functiondata->clipX->min, functiondata->clipX->max);
			} else {
				instanceData->clipXLow = functiondata->clipX->min;
				instanceData->clipXHigh = functiondata->clipX->max;
			}
		}
		if (functiondata->clipY) {
			if (functiondata->clipY->min >= functiondata->clipY->max) {
				printk(KERN_WARNING "%s: Clip Y min (%d) >= Y clip max (%d) - ignored.",
					   __func__, functiondata->clipY->min, functiondata->clipY->max);
			} else {
				instanceData->clipYLow = functiondata->clipY->min;
				instanceData->clipYHigh = functiondata->clipY->max;
			}
		}
	}

	
	set_bit(EV_ABS, function_device->input->evbit);
	set_bit(EV_SYN, function_device->input->evbit);
	set_bit(EV_KEY, function_device->input->evbit);
	set_bit(BTN_TOUCH, function_device->input->keybit);
	set_bit(KEY_OK, function_device->input->keybit);

	f11_set_abs_params(function_device);

	printk(KERN_DEBUG "%s: Creating sysfs files.", __func__);
	retval = device_create_file(&function_device->dev, &dev_attr_flip);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create flip.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_clip);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create clip.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_offset);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create offset.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_swap);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create swap.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_relreport);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create relreport.", __func__);
		return retval;
	}
	retval = device_create_file(&function_device->dev, &dev_attr_maxPos);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create maxPos.", __func__);
		return retval;
	}

	return 0;
}
EXPORT_SYMBOL(FN_11_init);

int FN_11_detect(struct rmi_function_info *rmifninfo,
	struct rmi_function_descriptor *fndescr, unsigned int interruptCount)
{
	unsigned char fn11Queries[12];   
	unsigned char fn11Control[12];   
	int i;
	unsigned short fn11InterruptOffset;
	unsigned char fn11AbsDataBlockSize;
	int fn11HasPinch, fn11HasFlick, fn11HasTap;
	int fn11HasTapAndHold, fn11HasDoubleTap;
	int fn11HasEarlyTap, fn11HasPress;
	int fn11HasPalmDetect, fn11HasRotate;
	int fn11HasRel;
	unsigned char f11_egr_0, f11_egr_1;
	unsigned int fn11AllDataBlockSize;
	int retval = 0;
	struct f11_instance_data *instanceData;

	printk(KERN_DEBUG "%s: RMI4 F11 detect\n", __func__);

	instanceData = kzalloc(sizeof(struct f11_instance_data), GFP_KERNEL);
	if (!instanceData) {
		printk(KERN_ERR "%s: Error allocating F11 instance data.\n", __func__);
		return -ENOMEM;
	}
	instanceData->deviceInfo = kzalloc(sizeof(struct rmi_F11_device_query), GFP_KERNEL);
	if (!instanceData->deviceInfo) {
		printk(KERN_ERR "%s: Error allocating F11 device query.\n", __func__);
		return -ENOMEM;
	}
	instanceData->sensorInfo = kzalloc(sizeof(struct rmi_F11_sensor_query), GFP_KERNEL);
	if (!instanceData->sensorInfo) {
		printk(KERN_ERR "%s: Error allocating F11 sensor query.\n", __func__);
		return -ENOMEM;
	}
	rmifninfo->fndata = instanceData;

	rmifninfo->funcDescriptor.queryBaseAddr = fndescr->queryBaseAddr;
	rmifninfo->funcDescriptor.commandBaseAddr = fndescr->commandBaseAddr;
	rmifninfo->funcDescriptor.controlBaseAddr = fndescr->controlBaseAddr;
	rmifninfo->funcDescriptor.dataBaseAddr = fndescr->dataBaseAddr;
	rmifninfo->funcDescriptor.interruptSrcCnt = fndescr->interruptSrcCnt;
	rmifninfo->funcDescriptor.functionNum = fndescr->functionNum;

	rmifninfo->numSources = fndescr->interruptSrcCnt;

	retval = rmi_read_multiple(rmifninfo->sensor, fndescr->queryBaseAddr, fn11Queries,
			sizeof(fn11Queries));
	if (retval) {
		printk(KERN_ERR "%s: RMI4 function $11 detect: "
			"Could not read function query registers 0x%x\n",
			__func__,  rmifninfo->funcDescriptor.queryBaseAddr);
		return retval;
	}

	
	instanceData->deviceInfo->hasQuery9 = (fn11Queries[0] & 0x04) != 0;
	instanceData->deviceInfo->numberOfSensors = (fn11Queries[0] & 0x07) + 1;
	printk(KERN_DEBUG "%s: F11 device - %d sensors.  Query 9? %d.", __func__, instanceData->deviceInfo->numberOfSensors, instanceData->deviceInfo->hasQuery9);

	
	instanceData->sensorInfo->numberOfFingers = 2; 
	if ((fn11Queries[1] & 0x7) <= 4)
		
		instanceData->sensorInfo->numberOfFingers = (fn11Queries[1] & 0x7) + 1;
	else {
		if ((fn11Queries[1] & 0x7) == 5)
			instanceData->sensorInfo->numberOfFingers = 10;
	}
	instanceData->sensorInfo->configurable = (fn11Queries[1] & 0x80) != 0;
	instanceData->sensorInfo->hasSensitivityAdjust = (fn11Queries[1] & 0x40) != 0;
	instanceData->sensorInfo->hasGestures = (fn11Queries[1] & 0x20) != 0;
	instanceData->sensorInfo->hasAbs = (fn11Queries[1] & 0x10) != 0;
	instanceData->sensorInfo->hasRel = (fn11Queries[1] & 0x08) != 0;
	instanceData->sensorInfo->absDataSize = fn11Queries[5] & 0x03;
	printk(KERN_DEBUG "%s: Number of fingers: %d.", __func__, instanceData->sensorInfo->numberOfFingers);

	rmifninfo->interruptRegister = interruptCount/8;

	fn11InterruptOffset = interruptCount % 8;

	for (i = fn11InterruptOffset;
			i < ((fndescr->interruptSrcCnt & 0x7) + fn11InterruptOffset);
			i++)
		rmifninfo->interruptMask |= 1 << i;

	
	instanceData->fingerDataBufferSize = (instanceData->sensorInfo->numberOfFingers + 3) / 4;
	
	fn11AbsDataBlockSize = 5;
	if (instanceData->sensorInfo->absDataSize != 0)
		printk(KERN_WARNING "%s: Unrecognized abs data size %d ignored.", __func__, instanceData->sensorInfo->absDataSize);
	if (instanceData->sensorInfo->hasAbs) {
		instanceData->absDataSize = fn11AbsDataBlockSize;
		instanceData->absDataOffset = instanceData->fingerDataBufferSize;
		instanceData->fingerDataBufferSize += instanceData->sensorInfo->numberOfFingers * fn11AbsDataBlockSize;
	}
	if (instanceData->sensorInfo->hasRel) {
		instanceData->relDataOffset = ((instanceData->sensorInfo->numberOfFingers + 3) / 4) +
			
			(fn11AbsDataBlockSize * instanceData->sensorInfo->numberOfFingers);
		instanceData->fingerDataBufferSize += instanceData->sensorInfo->numberOfFingers * 2;
	}
	if (instanceData->sensorInfo->hasGestures) {
		instanceData->gestureDataOffset = instanceData->fingerDataBufferSize;
		printk(KERN_WARNING "%s: WARNING Need to correctly compute gesture data location.", __func__);
	}

	f11_egr_0 = fn11Queries[7];
	f11_egr_1 = fn11Queries[8];

	fn11HasPinch = f11_egr_0 & 0x40;
	fn11HasFlick = f11_egr_0 & 0x10;
	fn11HasTap = f11_egr_0 & 0x01;
	fn11HasTapAndHold = f11_egr_0 & 0x02;
	fn11HasDoubleTap = f11_egr_0 & 0x04;
	fn11HasEarlyTap = f11_egr_0 & 0x08;
	fn11HasPress = f11_egr_0 & 0x20;
	fn11HasPalmDetect = f11_egr_1 & 0x01;
	fn11HasRotate = f11_egr_1 & 0x02;
	fn11HasRel = fn11Queries[1] & 0x08;

	fn11AllDataBlockSize =
		
		((instanceData->sensorInfo->numberOfFingers + 3) / 4) +
		
		(fn11AbsDataBlockSize * instanceData->sensorInfo->numberOfFingers) +
		
		2 * fn11HasRel +
		!!(f11_egr_0) +
		(f11_egr_0 || f11_egr_1) +
		!!(fn11HasPinch | fn11HasFlick) +
		2 * !!(fn11HasFlick);
	instanceData->fingerDataBuffer = kcalloc(instanceData->fingerDataBufferSize, sizeof(unsigned char), GFP_KERNEL);
	if (!instanceData->fingerDataBuffer) {
		printk(KERN_ERR "%s: Failed to allocate finger data buffer.", __func__);
		return -ENOMEM;
	}

	
	instanceData->controlRegisters = kzalloc(sizeof(struct rmi_F11_control), GFP_KERNEL);
	if (!instanceData->controlRegisters) {
		printk(KERN_ERR "%s: Error allocating F11 control registers.\n", __func__);
		return -ENOMEM;
	}
	retval = rmi_read_multiple(rmifninfo->sensor, fndescr->controlBaseAddr,
		fn11Control, sizeof(fn11Control));
	if (retval) {
		printk(KERN_ERR "%s: Failed to read F11 control registers.", __func__);
		return retval;
	}
	instanceData->controlRegisters->sensorMaxXPos = (((int) fn11Control[7] & 0x0F) << 8) + fn11Control[6];
	instanceData->controlRegisters->sensorMaxYPos = (((int) fn11Control[9] & 0x0F) << 8) + fn11Control[8];
	printk(KERN_DEBUG "%s: Max X %d Max Y %d", __func__, instanceData->controlRegisters->sensorMaxXPos, instanceData->controlRegisters->sensorMaxYPos);
	return 0;
}
EXPORT_SYMBOL(FN_11_detect);

static ssize_t rmi_fn_11_maxPos_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;

	return sprintf(buf, "%u %u\n", instance_data->controlRegisters->sensorMaxXPos, instance_data->controlRegisters->sensorMaxYPos);
}

static ssize_t rmi_fn_11_maxPos_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t rmi_fn_11_flip_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;

	return sprintf(buf, "%u %u\n", instance_data->flipX, instance_data->flipY);
}

static ssize_t rmi_fn_11_flip_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;
	unsigned int newX, newY;

	printk(KERN_DEBUG "%s: Flip set to %s", __func__, buf);

	if (sscanf(buf, "%u %u", &newX, &newY) != 2)
		return -EINVAL;
	if (newX < 0 || newX > 1 || newY < 0 || newY > 1)
		return -EINVAL;
	instance_data->flipX = newX;
	instance_data->flipY = newY;

	return count;
}

static ssize_t rmi_fn_11_swap_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", instance_data->swap_axes);
}

static ssize_t rmi_fn_11_swap_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;
	unsigned int newSwap;

	printk(KERN_DEBUG "%s: Swap set to %s", __func__, buf);

	if (sscanf(buf, "%u", &newSwap) != 1)
		return -EINVAL;
	if (newSwap < 0 || newSwap > 1)
		return -EINVAL;
	instance_data->swap_axes = newSwap;

	f11_set_abs_params(fn);

	return count;
}

static ssize_t rmi_fn_11_relreport_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;

	return sprintf(buf, "%u \n", instance_data->relReport);
}

static ssize_t rmi_fn_11_relreport_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;
	unsigned int relRep;

	printk(KERN_DEBUG "%s: relReport set to %s", __func__, buf);
	if (sscanf(buf, "%u", &relRep) != 1)
		return -EINVAL;
	if (relRep < 0 || relRep > 1)
		return -EINVAL;
	instance_data->relReport = relRep;

	return count;
}

static ssize_t rmi_fn_11_offset_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;

	return sprintf(buf, "%d %d\n", instance_data->offsetX, instance_data->offsetY);
}

static ssize_t rmi_fn_11_offset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;
	int newX, newY;

	printk(KERN_DEBUG "%s: Offset set to %s", __func__, buf);

	if (sscanf(buf, "%d %d", &newX, &newY) != 2)
		return -EINVAL;
	instance_data->offsetX = newX;
	instance_data->offsetY = newY;

	return count;
}

static ssize_t rmi_fn_11_clip_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;

	return sprintf(buf, "%u %u %u %u\n",
				   instance_data->clipXLow, instance_data->clipXHigh,
				   instance_data->clipYLow, instance_data->clipYHigh);
}

static ssize_t rmi_fn_11_clip_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f11_instance_data *instance_data = (struct f11_instance_data *)fn->rfi->fndata;
	unsigned int newXLow, newXHigh, newYLow, newYHigh;

	printk(KERN_DEBUG "%s: Clip set to %s", __func__, buf);

	if (sscanf(buf, "%u %u %u %u", &newXLow, &newXHigh, &newYLow, &newYHigh) != 4)
		return -EINVAL;
	if (newXLow < 0 || newXLow >= newXHigh || newYLow < 0 || newYLow >= newYHigh)
		return -EINVAL;
	instance_data->clipXLow = newXLow;
	instance_data->clipXHigh = newXHigh;
	instance_data->clipYLow = newYLow;
	instance_data->clipYHigh = newYHigh;

	f11_set_abs_params(fn);

	return count;
}
