/*
 * ACPI PCI Hot Plug Controller Driver
 *
 * Copyright (C) 1995,2001 Compaq Computer Corporation
 * Copyright (C) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001 IBM Corp.
 * Copyright (C) 2002 Hiroshi Aono (h-aono@ap.jp.nec.com)
 * Copyright (C) 2002,2003 Takayoshi Kochi (t-kochi@bq.jp.nec.com)
 * Copyright (C) 2002,2003 NEC Corporation
 * Copyright (C) 2003-2005 Matthew Wilcox (matthew.wilcox@hp.com)
 * Copyright (C) 2003-2005 Hewlett Packard
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
 * Send feedback to <gregkh@us.ibm.com>,
 *		    <t-kochi@bq.jp.nec.com>
 *
 */

#ifndef _ACPIPHP_H
#define _ACPIPHP_H

#include <linux/acpi.h>
#include <linux/mutex.h>
#include <linux/pci_hotplug.h>

#define dbg(format, arg...)					\
	do {							\
		if (acpiphp_debug)				\
			printk(KERN_DEBUG "%s: " format,	\
				MY_NAME , ## arg);		\
	} while (0)
#define err(format, arg...) printk(KERN_ERR "%s: " format, MY_NAME , ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format, MY_NAME , ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format, MY_NAME , ## arg)

struct acpiphp_bridge;
struct acpiphp_slot;

struct slot {
	struct hotplug_slot	*hotplug_slot;
	struct acpiphp_slot	*acpi_slot;
	struct hotplug_slot_info info;
};

static inline const char *slot_name(struct slot *slot)
{
	return hotplug_slot_name(slot->hotplug_slot);
}

struct acpiphp_bridge {
	struct list_head list;
	acpi_handle handle;
	struct acpiphp_slot *slots;

	
	struct acpiphp_func *func;

	int type;
	int nr_slots;

	u32 flags;

	
	struct pci_bus *pci_bus;

	
	struct pci_dev *pci_dev;

	spinlock_t res_lock;
};


struct acpiphp_slot {
	struct acpiphp_slot *next;
	struct acpiphp_bridge *bridge;	
	struct list_head funcs;		
	struct slot *slot;
	struct mutex crit_sect;

	u8		device;		

	unsigned long long sun;		
	u32		flags;		
};


struct acpiphp_func {
	struct acpiphp_slot *slot;	
	struct acpiphp_bridge *bridge;	

	struct list_head sibling;
	struct notifier_block nb;
	acpi_handle	handle;

	u8		function;	
	u32		flags;		
};

struct acpiphp_attention_info
{
	int (*set_attn)(struct hotplug_slot *slot, u8 status);
	int (*get_attn)(struct hotplug_slot *slot, u8 *status);
	struct module *owner;
};

#define ACPI_PCI_HOST_HID		"PNP0A03"

#define BRIDGE_TYPE_HOST		0
#define BRIDGE_TYPE_P2P			1

#define ACPI_STA_PRESENT		(0x00000001)
#define ACPI_STA_ENABLED		(0x00000002)
#define ACPI_STA_SHOW_IN_UI		(0x00000004)
#define ACPI_STA_FUNCTIONING		(0x00000008)
#define ACPI_STA_ALL			(0x0000000f)

#define BRIDGE_HAS_STA		(0x00000001)
#define BRIDGE_HAS_EJ0		(0x00000002)
#define BRIDGE_HAS_HPP		(0x00000004)
#define BRIDGE_HAS_PS0		(0x00000010)
#define BRIDGE_HAS_PS1		(0x00000020)
#define BRIDGE_HAS_PS2		(0x00000040)
#define BRIDGE_HAS_PS3		(0x00000080)


#define SLOT_POWEREDON		(0x00000001)
#define SLOT_ENABLED		(0x00000002)
#define SLOT_MULTIFUNCTION	(0x00000004)


#define FUNC_HAS_STA		(0x00000001)
#define FUNC_HAS_EJ0		(0x00000002)
#define FUNC_HAS_PS0		(0x00000010)
#define FUNC_HAS_PS1		(0x00000020)
#define FUNC_HAS_PS2		(0x00000040)
#define FUNC_HAS_PS3		(0x00000080)
#define FUNC_HAS_DCK            (0x00000100)


extern int acpiphp_register_attention(struct acpiphp_attention_info*info);
extern int acpiphp_unregister_attention(struct acpiphp_attention_info *info);
extern int acpiphp_register_hotplug_slot(struct acpiphp_slot *slot);
extern void acpiphp_unregister_hotplug_slot(struct acpiphp_slot *slot);

extern int acpiphp_glue_init (void);
extern void acpiphp_glue_exit (void);
extern int acpiphp_get_num_slots (void);
typedef int (*acpiphp_callback)(struct acpiphp_slot *slot, void *data);

extern int acpiphp_enable_slot (struct acpiphp_slot *slot);
extern int acpiphp_disable_slot (struct acpiphp_slot *slot);
extern int acpiphp_eject_slot (struct acpiphp_slot *slot);
extern u8 acpiphp_get_power_status (struct acpiphp_slot *slot);
extern u8 acpiphp_get_attention_status (struct acpiphp_slot *slot);
extern u8 acpiphp_get_latch_status (struct acpiphp_slot *slot);
extern u8 acpiphp_get_adapter_status (struct acpiphp_slot *slot);

extern int acpiphp_debug;

#endif 
