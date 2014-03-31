/*
 * PCI Hot Plug Controller Skeleton Driver - 0.3
 *
 * Copyright (C) 2001,2003 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001,2003 IBM Corp.
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
 * This driver is to be used as a skeleton driver to show how to interface
 * with the pci hotplug core easily.
 *
 * Send feedback to <greg@kroah.com>
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/init.h>

#define SLOT_NAME_SIZE	10
struct slot {
	u8 number;
	struct hotplug_slot *hotplug_slot;
	struct list_head slot_list;
	char name[SLOT_NAME_SIZE];
};

static LIST_HEAD(slot_list);

#define MY_NAME	"pcihp_skeleton"

#define dbg(format, arg...)					\
	do {							\
		if (debug)					\
			printk (KERN_DEBUG "%s: " format "\n",	\
				MY_NAME , ## arg); 		\
	} while (0)
#define err(format, arg...) printk(KERN_ERR "%s: " format "\n", MY_NAME , ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format "\n", MY_NAME , ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format "\n", MY_NAME , ## arg)

static bool debug;
static int num_slots;

#define DRIVER_VERSION	"0.3"
#define DRIVER_AUTHOR	"Greg Kroah-Hartman <greg@kroah.com>"
#define DRIVER_DESC	"Hot Plug PCI Controller Skeleton Driver"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debugging mode enabled or not");

static int enable_slot		(struct hotplug_slot *slot);
static int disable_slot		(struct hotplug_slot *slot);
static int set_attention_status (struct hotplug_slot *slot, u8 value);
static int hardware_test	(struct hotplug_slot *slot, u32 value);
static int get_power_status	(struct hotplug_slot *slot, u8 *value);
static int get_attention_status	(struct hotplug_slot *slot, u8 *value);
static int get_latch_status	(struct hotplug_slot *slot, u8 *value);
static int get_adapter_status	(struct hotplug_slot *slot, u8 *value);

static struct hotplug_slot_ops skel_hotplug_slot_ops = {
	.enable_slot =		enable_slot,
	.disable_slot =		disable_slot,
	.set_attention_status =	set_attention_status,
	.hardware_test =	hardware_test,
	.get_power_status =	get_power_status,
	.get_attention_status =	get_attention_status,
	.get_latch_status =	get_latch_status,
	.get_adapter_status =	get_adapter_status,
};

static int enable_slot(struct hotplug_slot *hotplug_slot)
{
	struct slot *slot = hotplug_slot->private;
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __func__, hotplug_slot->name);


	return retval;
}

static int disable_slot(struct hotplug_slot *hotplug_slot)
{
	struct slot *slot = hotplug_slot->private;
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __func__, hotplug_slot->name);


	return retval;
}

static int set_attention_status(struct hotplug_slot *hotplug_slot, u8 status)
{
	struct slot *slot = hotplug_slot->private;
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __func__, hotplug_slot->name);

	switch (status) {
		case 0:
			break;

		case 1:
		default:
			break;
	}

	return retval;
}

static int hardware_test(struct hotplug_slot *hotplug_slot, u32 value)
{
	struct slot *slot = hotplug_slot->private;
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __func__, hotplug_slot->name);

	switch (value) {
		case 0:
			
			break;
		case 1:
			
			break;
	}

	return retval;
}

static int get_power_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = hotplug_slot->private;
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __func__, hotplug_slot->name);


	return retval;
}

static int get_attention_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = hotplug_slot->private;
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __func__, hotplug_slot->name);


	return retval;
}

static int get_latch_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = hotplug_slot->private;
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __func__, hotplug_slot->name);


	return retval;
}

static int get_adapter_status(struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = hotplug_slot->private;
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __func__, hotplug_slot->name);


	return retval;
}

static void release_slot(struct hotplug_slot *hotplug_slot)
{
	struct slot *slot = hotplug_slot->private;

	dbg("%s - physical_slot = %s\n", __func__, hotplug_slot->name);
	kfree(slot->hotplug_slot->info);
	kfree(slot->hotplug_slot);
	kfree(slot);
}

static void make_slot_name(struct slot *slot)
{
	snprintf(slot->hotplug_slot->name, SLOT_NAME_SIZE, "%d", slot->number);
}

static int __init init_slots(void)
{
	struct slot *slot;
	struct hotplug_slot *hotplug_slot;
	struct hotplug_slot_info *info;
	int retval = -ENOMEM;
	int i;

	for (i = 0; i < num_slots; ++i) {
		slot = kzalloc(sizeof(*slot), GFP_KERNEL);
		if (!slot)
			goto error;

		hotplug_slot = kzalloc(sizeof(*hotplug_slot), GFP_KERNEL);
		if (!hotplug_slot)
			goto error_slot;
		slot->hotplug_slot = hotplug_slot;

		info = kzalloc(sizeof(*info), GFP_KERNEL);
		if (!info)
			goto error_hpslot;
		hotplug_slot->info = info;

		slot->number = i;

		hotplug_slot->name = slot->name;
		hotplug_slot->private = slot;
		hotplug_slot->release = &release_slot;
		make_slot_name(slot);
		hotplug_slot->ops = &skel_hotplug_slot_ops;
		
		get_power_status(hotplug_slot, &info->power_status);
		get_attention_status(hotplug_slot, &info->attention_status);
		get_latch_status(hotplug_slot, &info->latch_status);
		get_adapter_status(hotplug_slot, &info->adapter_status);
		
		dbg("registering slot %d\n", i);
		retval = pci_hp_register(slot->hotplug_slot);
		if (retval) {
			err("pci_hp_register failed with error %d\n", retval);
			goto error_info;
		}

		
		list_add(&slot->slot_list, &slot_list);
	}

	return 0;
error_info:
	kfree(info);
error_hpslot:
	kfree(hotplug_slot);
error_slot:
	kfree(slot);
error:
	return retval;
}

static void __exit cleanup_slots(void)
{
	struct list_head *tmp;
	struct list_head *next;
	struct slot *slot;

	list_for_each_safe(tmp, next, &slot_list) {
		slot = list_entry(tmp, struct slot, slot_list);
		list_del(&slot->slot_list);
		pci_hp_deregister(slot->hotplug_slot);
	}
}
		
static int __init pcihp_skel_init(void)
{
	int retval;

	info(DRIVER_DESC " version: " DRIVER_VERSION "\n");
	num_slots = 5;

	return init_slots();
}

static void __exit pcihp_skel_exit(void)
{
	cleanup_slots();
}

module_init(pcihp_skel_init);
module_exit(pcihp_skel_exit);
