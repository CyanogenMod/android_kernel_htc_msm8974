/*
 * dspdrv.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Interface to allocate and free bridge resources.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/types.h>
#include <dspbridge/host_os.h>

#include <dspbridge/dbdefs.h>

#include <dspbridge/drv.h>
#include <dspbridge/dev.h>
#include <dspbridge/dspapi.h>

#include <dspbridge/mgr.h>

#include <dspbridge/dspdrv.h>

u32 dsp_init(u32 *init_status)
{
	char dev_node[MAXREGPATHLENGTH] = "TIOMAP1510";
	int status = -EPERM;
	struct drv_object *drv_obj = NULL;
	u32 device_node;
	u32 device_node_string;

	if (!api_init())
		goto func_cont;

	status = drv_create(&drv_obj);
	if (status) {
		api_exit();
		goto func_cont;
	}

	
	
	status = drv_request_resources((u32) &dev_node, &device_node_string);
	if (!status) {
		
		status = dev_start_device((struct cfg_devnode *)
					  device_node_string);
		if (status)
			(void)drv_release_resources
			    ((u32) device_node_string, drv_obj);
	} else {
		dev_dbg(bridge, "%s: drv_request_resources Failed\n", __func__);
		status = -EPERM;
	}

	
	if (status) {
		for (device_node = drv_get_first_dev_extension();
		     device_node != 0;
		     device_node = drv_get_next_dev_extension(device_node)) {
			(void)dev_remove_device((struct cfg_devnode *)
						device_node);
			(void)drv_release_resources((u32) device_node, drv_obj);
		}
		
		(void)drv_destroy(drv_obj);
		drv_obj = NULL;
		api_exit();
		dev_dbg(bridge, "%s: Logical device failed init\n", __func__);
	}			
func_cont:
	
	if (!status) {
		(void)api_init_complete2();
	} else {
		dev_dbg(bridge, "%s: Failed\n", __func__);
	}			
	*init_status = status;
	
	return (u32) drv_obj;
}

bool dsp_deinit(u32 device_context)
{
	bool ret = true;
	u32 device_node;
	struct mgr_object *mgr_obj = NULL;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	while ((device_node = drv_get_first_dev_extension()) != 0) {
		(void)dev_remove_device((struct cfg_devnode *)device_node);

		(void)drv_release_resources((u32) device_node,
					(struct drv_object *)device_context);
	}

	(void)drv_destroy((struct drv_object *)device_context);

	if (drv_datap && drv_datap->mgr_object) {
		mgr_obj = drv_datap->mgr_object;
		(void)mgr_destroy(mgr_obj);
	} else {
		pr_err("%s: Failed to retrieve the object handle\n", __func__);
	}

	api_exit();

	return ret;
}
