/*
 * Copyright (C) 2004 Intel Corporation <naveen.b.s@intel.com>
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * ACPI based HotPlug driver that supports Memory Hotplug
 * This driver fields notifications from firmware for memory add
 * and remove operations and alerts the VM of the affected memory
 * ranges.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/memory_hotplug.h>
#include <linux/slab.h>
#include <acpi/acpi_drivers.h>

#define ACPI_MEMORY_DEVICE_CLASS		"memory"
#define ACPI_MEMORY_DEVICE_HID			"PNP0C80"
#define ACPI_MEMORY_DEVICE_NAME			"Hotplug Mem Device"

#define _COMPONENT		ACPI_MEMORY_DEVICE_COMPONENT

#undef PREFIX
#define 	PREFIX		"ACPI:memory_hp:"

ACPI_MODULE_NAME("acpi_memhotplug");
MODULE_AUTHOR("Naveen B S <naveen.b.s@intel.com>");
MODULE_DESCRIPTION("Hotplug Mem Driver");
MODULE_LICENSE("GPL");

#define MEMORY_INVALID_STATE	0
#define MEMORY_POWER_ON_STATE	1
#define MEMORY_POWER_OFF_STATE	2

static int acpi_memory_device_add(struct acpi_device *device);
static int acpi_memory_device_remove(struct acpi_device *device, int type);

static const struct acpi_device_id memory_device_ids[] = {
	{ACPI_MEMORY_DEVICE_HID, 0},
	{"", 0},
};
MODULE_DEVICE_TABLE(acpi, memory_device_ids);

static struct acpi_driver acpi_memory_device_driver = {
	.name = "acpi_memhotplug",
	.class = ACPI_MEMORY_DEVICE_CLASS,
	.ids = memory_device_ids,
	.ops = {
		.add = acpi_memory_device_add,
		.remove = acpi_memory_device_remove,
		},
};

struct acpi_memory_info {
	struct list_head list;
	u64 start_addr;		
	u64 length;		
	unsigned short caching;	
	unsigned short write_protect;	
	unsigned int enabled:1;
};

struct acpi_memory_device {
	struct acpi_device * device;
	unsigned int state;	
	struct list_head res_list;
};

static int acpi_hotmem_initialized;

static acpi_status
acpi_memory_get_resource(struct acpi_resource *resource, void *context)
{
	struct acpi_memory_device *mem_device = context;
	struct acpi_resource_address64 address64;
	struct acpi_memory_info *info, *new;
	acpi_status status;

	status = acpi_resource_to_address64(resource, &address64);
	if (ACPI_FAILURE(status) ||
	    (address64.resource_type != ACPI_MEMORY_RANGE))
		return AE_OK;

	list_for_each_entry(info, &mem_device->res_list, list) {
		
		if ((info->caching == address64.info.mem.caching) &&
		    (info->write_protect == address64.info.mem.write_protect) &&
		    (info->start_addr + info->length == address64.minimum)) {
			info->length += address64.address_length;
			return AE_OK;
		}
	}

	new = kzalloc(sizeof(struct acpi_memory_info), GFP_KERNEL);
	if (!new)
		return AE_ERROR;

	INIT_LIST_HEAD(&new->list);
	new->caching = address64.info.mem.caching;
	new->write_protect = address64.info.mem.write_protect;
	new->start_addr = address64.minimum;
	new->length = address64.address_length;
	list_add_tail(&new->list, &mem_device->res_list);

	return AE_OK;
}

static int
acpi_memory_get_device_resources(struct acpi_memory_device *mem_device)
{
	acpi_status status;
	struct acpi_memory_info *info, *n;


	if (!list_empty(&mem_device->res_list))
		return 0;

	status = acpi_walk_resources(mem_device->device->handle, METHOD_NAME__CRS,
				     acpi_memory_get_resource, mem_device);
	if (ACPI_FAILURE(status)) {
		list_for_each_entry_safe(info, n, &mem_device->res_list, list)
			kfree(info);
		INIT_LIST_HEAD(&mem_device->res_list);
		return -EINVAL;
	}

	return 0;
}

static int
acpi_memory_get_device(acpi_handle handle,
		       struct acpi_memory_device **mem_device)
{
	acpi_status status;
	acpi_handle phandle;
	struct acpi_device *device = NULL;
	struct acpi_device *pdevice = NULL;
	int result;


	if (!acpi_bus_get_device(handle, &device) && device)
		goto end;

	status = acpi_get_parent(handle, &phandle);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "Cannot find acpi parent"));
		return -EINVAL;
	}

	
	result = acpi_bus_get_device(phandle, &pdevice);
	if (result) {
		printk(KERN_WARNING PREFIX "Cannot get acpi bus device");
		return -EINVAL;
	}

	result = acpi_bus_add(&device, pdevice, handle, ACPI_BUS_TYPE_DEVICE);
	if (result) {
		printk(KERN_WARNING PREFIX "Cannot add acpi bus");
		return -EINVAL;
	}

      end:
	*mem_device = acpi_driver_data(device);
	if (!(*mem_device)) {
		printk(KERN_ERR "\n driver data not found");
		return -ENODEV;
	}

	return 0;
}

static int acpi_memory_check_device(struct acpi_memory_device *mem_device)
{
	unsigned long long current_status;

	
	if (ACPI_FAILURE(acpi_evaluate_integer(mem_device->device->handle, "_STA",
					       NULL, &current_status)))
		return -ENODEV;
	if (!((current_status & ACPI_STA_DEVICE_PRESENT)
	      && (current_status & ACPI_STA_DEVICE_ENABLED)
	      && (current_status & ACPI_STA_DEVICE_FUNCTIONING)))
		return -ENODEV;

	return 0;
}

static int acpi_memory_enable_device(struct acpi_memory_device *mem_device)
{
	int result, num_enabled = 0;
	struct acpi_memory_info *info;
	int node;


	
	result = acpi_memory_get_device_resources(mem_device);
	if (result) {
		printk(KERN_ERR PREFIX "get_device_resources failed\n");
		mem_device->state = MEMORY_INVALID_STATE;
		return result;
	}

	node = acpi_get_node(mem_device->device->handle);
	list_for_each_entry(info, &mem_device->res_list, list) {
		if (info->enabled) { 
			num_enabled++;
			continue;
		}
		if (!info->length)
			continue;
		if (node < 0)
			node = memory_add_physaddr_to_nid(info->start_addr);

		result = add_memory(node, info->start_addr, info->length);
		if (result)
			continue;
		info->enabled = 1;
		num_enabled++;
	}
	if (!num_enabled) {
		printk(KERN_ERR PREFIX "add_memory failed\n");
		mem_device->state = MEMORY_INVALID_STATE;
		return -EINVAL;
	}
	return 0;
}

static int acpi_memory_powerdown_device(struct acpi_memory_device *mem_device)
{
	acpi_status status;
	struct acpi_object_list arg_list;
	union acpi_object arg;
	unsigned long long current_status;


	
	arg_list.count = 1;
	arg_list.pointer = &arg;
	arg.type = ACPI_TYPE_INTEGER;
	arg.integer.value = 1;
	status = acpi_evaluate_object(mem_device->device->handle,
				      "_EJ0", &arg_list, NULL);
	
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "_EJ0 failed"));
		return -ENODEV;
	}

	
	status = acpi_evaluate_integer(mem_device->device->handle, "_STA",
				       NULL, &current_status);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	
	if (current_status & ACPI_STA_DEVICE_ENABLED)
		return -EINVAL;

	return 0;
}

static int acpi_memory_disable_device(struct acpi_memory_device *mem_device)
{
	int result;
	struct acpi_memory_info *info, *n;


	list_for_each_entry_safe(info, n, &mem_device->res_list, list) {
		if (info->enabled) {
			result = remove_memory(info->start_addr, info->length);
			if (result)
				return result;
		}
		kfree(info);
	}

	
	result = acpi_memory_powerdown_device(mem_device);
	if (result) {
		
		mem_device->state = MEMORY_INVALID_STATE;
		return result;
	}

	mem_device->state = MEMORY_POWER_OFF_STATE;
	return result;
}

static void acpi_memory_device_notify(acpi_handle handle, u32 event, void *data)
{
	struct acpi_memory_device *mem_device;
	struct acpi_device *device;


	switch (event) {
	case ACPI_NOTIFY_BUS_CHECK:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "\nReceived BUS CHECK notification for device\n"));
		
	case ACPI_NOTIFY_DEVICE_CHECK:
		if (event == ACPI_NOTIFY_DEVICE_CHECK)
			ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					  "\nReceived DEVICE CHECK notification for device\n"));
		if (acpi_memory_get_device(handle, &mem_device)) {
			printk(KERN_ERR PREFIX "Cannot find driver data\n");
			return;
		}

		if (!acpi_memory_check_device(mem_device)) {
			if (acpi_memory_enable_device(mem_device))
				printk(KERN_ERR PREFIX
					    "Cannot enable memory device\n");
		}
		break;
	case ACPI_NOTIFY_EJECT_REQUEST:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "\nReceived EJECT REQUEST notification for device\n"));

		if (acpi_bus_get_device(handle, &device)) {
			printk(KERN_ERR PREFIX "Device doesn't exist\n");
			break;
		}
		mem_device = acpi_driver_data(device);
		if (!mem_device) {
			printk(KERN_ERR PREFIX "Driver Data is NULL\n");
			break;
		}

		if (acpi_memory_disable_device(mem_device))
			printk(KERN_ERR PREFIX
				    "Disable memory device\n");
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Unsupported event [0x%x]\n", event));
		break;
	}

	return;
}

static int acpi_memory_device_add(struct acpi_device *device)
{
	int result;
	struct acpi_memory_device *mem_device = NULL;


	if (!device)
		return -EINVAL;

	mem_device = kzalloc(sizeof(struct acpi_memory_device), GFP_KERNEL);
	if (!mem_device)
		return -ENOMEM;

	INIT_LIST_HEAD(&mem_device->res_list);
	mem_device->device = device;
	sprintf(acpi_device_name(device), "%s", ACPI_MEMORY_DEVICE_NAME);
	sprintf(acpi_device_class(device), "%s", ACPI_MEMORY_DEVICE_CLASS);
	device->driver_data = mem_device;

	
	result = acpi_memory_get_device_resources(mem_device);
	if (result) {
		kfree(mem_device);
		return result;
	}

	
	mem_device->state = MEMORY_POWER_ON_STATE;

	printk(KERN_DEBUG "%s \n", acpi_device_name(device));

	if (!acpi_hotmem_initialized)
		return 0;

	if (!acpi_memory_check_device(mem_device)) {
		
		result = acpi_memory_enable_device(mem_device);
		if (result)
			printk(KERN_ERR PREFIX
				"Error in acpi_memory_enable_device\n");
	}
	return result;
}

static int acpi_memory_device_remove(struct acpi_device *device, int type)
{
	struct acpi_memory_device *mem_device = NULL;


	if (!device || !acpi_driver_data(device))
		return -EINVAL;

	mem_device = acpi_driver_data(device);
	kfree(mem_device);

	return 0;
}

static acpi_status is_memory_device(acpi_handle handle)
{
	char *hardware_id;
	acpi_status status;
	struct acpi_device_info *info;

	status = acpi_get_object_info(handle, &info);
	if (ACPI_FAILURE(status))
		return status;

	if (!(info->valid & ACPI_VALID_HID)) {
		kfree(info);
		return AE_ERROR;
	}

	hardware_id = info->hardware_id.string;
	if ((hardware_id == NULL) ||
	    (strcmp(hardware_id, ACPI_MEMORY_DEVICE_HID)))
		status = AE_ERROR;

	kfree(info);
	return status;
}

static acpi_status
acpi_memory_register_notify_handler(acpi_handle handle,
				    u32 level, void *ctxt, void **retv)
{
	acpi_status status;


	status = is_memory_device(handle);
	if (ACPI_FAILURE(status))
		return AE_OK;	

	status = acpi_install_notify_handler(handle, ACPI_SYSTEM_NOTIFY,
					     acpi_memory_device_notify, NULL);
	
	return AE_OK;
}

static acpi_status
acpi_memory_deregister_notify_handler(acpi_handle handle,
				      u32 level, void *ctxt, void **retv)
{
	acpi_status status;


	status = is_memory_device(handle);
	if (ACPI_FAILURE(status))
		return AE_OK;	

	status = acpi_remove_notify_handler(handle,
					    ACPI_SYSTEM_NOTIFY,
					    acpi_memory_device_notify);

	return AE_OK;	
}

static int __init acpi_memory_device_init(void)
{
	int result;
	acpi_status status;


	result = acpi_bus_register_driver(&acpi_memory_device_driver);

	if (result < 0)
		return -ENODEV;

	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				     ACPI_UINT32_MAX,
				     acpi_memory_register_notify_handler, NULL,
				     NULL, NULL);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "walk_namespace failed"));
		acpi_bus_unregister_driver(&acpi_memory_device_driver);
		return -ENODEV;
	}

	acpi_hotmem_initialized = 1;
	return 0;
}

static void __exit acpi_memory_device_exit(void)
{
	acpi_status status;


	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				     ACPI_UINT32_MAX,
				     acpi_memory_deregister_notify_handler, NULL,
				     NULL, NULL);

	if (ACPI_FAILURE(status))
		ACPI_EXCEPTION((AE_INFO, status, "walk_namespace failed"));

	acpi_bus_unregister_driver(&acpi_memory_device_driver);

	return;
}

module_init(acpi_memory_device_init);
module_exit(acpi_memory_device_exit);
