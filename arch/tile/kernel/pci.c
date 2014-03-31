/*
 * Copyright 2011 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/bootmem.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/export.h>

#include <asm/processor.h>
#include <asm/sections.h>
#include <asm/byteorder.h>
#include <asm/hv_driver.h>
#include <hv/drv_pcie_rc_intf.h>



int __write_once tile_plx_gen1;

static struct pci_controller controllers[TILE_NUM_PCIE];
static int num_controllers;
static int pci_scan_flags[TILE_NUM_PCIE];

static struct pci_ops tile_cfg_ops;


resource_size_t pcibios_align_resource(void *data, const struct resource *res,
			    resource_size_t size, resource_size_t align)
{
	return res->start;
}
EXPORT_SYMBOL(pcibios_align_resource);

static int __devinit tile_pcie_open(int controller_id, int config_type)
{
	char filename[32];
	int fd;

	sprintf(filename, "pcie/%d/config%d", controller_id, config_type);

	fd = hv_dev_open((HV_VirtAddr)filename, 0);

	return fd;
}


static int __devinit tile_init_irqs(int controller_id,
				 struct pci_controller *controller)
{
	char filename[32];
	int fd;
	int ret;
	int x;
	struct pcie_rc_config rc_config;

	sprintf(filename, "pcie/%d/ctl", controller_id);
	fd = hv_dev_open((HV_VirtAddr)filename, 0);
	if (fd < 0) {
		pr_err("PCI: hv_dev_open(%s) failed\n", filename);
		return -1;
	}
	ret = hv_dev_pread(fd, 0, (HV_VirtAddr)(&rc_config),
			   sizeof(rc_config), PCIE_RC_CONFIG_MASK_OFF);
	hv_dev_close(fd);
	if (ret != sizeof(rc_config)) {
		pr_err("PCI: wanted %zd bytes, got %d\n",
		       sizeof(rc_config), ret);
		return -1;
	}
	
	controller->irq_base = rc_config.intr;

	for (x = 0; x < 4; x++)
		tile_irq_activate(rc_config.intr + x,
				  TILE_IRQ_HW_CLEAR);

	if (rc_config.plx_gen1)
		controller->plx_gen1 = 1;

	return 0;
}

int __init tile_pci_init(void)
{
	int i;

	pr_info("PCI: Searching for controllers...\n");

	
	num_controllers = 0;

	

	for (i = 0; i < TILE_NUM_PCIE; i++) {
		if (pci_scan_flags[i] == 0) {
			int hv_cfg_fd0 = -1;
			int hv_cfg_fd1 = -1;
			int hv_mem_fd = -1;
			char name[32];
			struct pci_controller *controller;

			hv_cfg_fd0 = tile_pcie_open(i, 0);
			if (hv_cfg_fd0 < 0)
				continue;
			hv_cfg_fd1 = tile_pcie_open(i, 1);
			if (hv_cfg_fd1 < 0) {
				pr_err("PCI: Couldn't open config fd to HV "
				    "for controller %d\n", i);
				goto err_cont;
			}

			sprintf(name, "pcie/%d/mem", i);
			hv_mem_fd = hv_dev_open((HV_VirtAddr)name, 0);
			if (hv_mem_fd < 0) {
				pr_err("PCI: Could not open mem fd to HV!\n");
				goto err_cont;
			}

			pr_info("PCI: Found PCI controller #%d\n", i);

			controller = &controllers[i];

			controller->index = i;
			controller->hv_cfg_fd[0] = hv_cfg_fd0;
			controller->hv_cfg_fd[1] = hv_cfg_fd1;
			controller->hv_mem_fd = hv_mem_fd;
			controller->first_busno = 0;
			controller->last_busno = 0xff;
			controller->ops = &tile_cfg_ops;

			num_controllers++;
			continue;

err_cont:
			if (hv_cfg_fd0 >= 0)
				hv_dev_close(hv_cfg_fd0);
			if (hv_cfg_fd1 >= 0)
				hv_dev_close(hv_cfg_fd1);
			if (hv_mem_fd >= 0)
				hv_dev_close(hv_mem_fd);
			continue;
		}
	}

	for (i = 0; i < num_controllers; i++) {
		struct pci_controller *controller = &controllers[i];

		if (controller->plx_gen1)
			tile_plx_gen1 = 1;
	}

	return num_controllers;
}

static int tile_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct pci_controller *controller =
		(struct pci_controller *)dev->sysdata;
	return (pin - 1) + controller->irq_base;
}


static void __devinit fixup_read_and_payload_sizes(void)
{
	struct pci_dev *dev = NULL;
	int smallest_max_payload = 0x1; 
	int max_read_size = 0x2; 
	u16 new_values;

	
	while ((dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev)) != NULL) {
		int pcie_caps_offset;
		u32 devcap;
		int max_payload;

		pcie_caps_offset = pci_find_capability(dev, PCI_CAP_ID_EXP);
		if (pcie_caps_offset == 0)
			continue;

		pci_read_config_dword(dev, pcie_caps_offset + PCI_EXP_DEVCAP,
				      &devcap);
		max_payload = devcap & PCI_EXP_DEVCAP_PAYLOAD;
		if (max_payload < smallest_max_payload)
			smallest_max_payload = max_payload;
	}

	
	new_values = (max_read_size << 12) | (smallest_max_payload << 5);
	while ((dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev)) != NULL) {
		int pcie_caps_offset;
		u16 devctl;

		pcie_caps_offset = pci_find_capability(dev, PCI_CAP_ID_EXP);
		if (pcie_caps_offset == 0)
			continue;

		pci_read_config_word(dev, pcie_caps_offset + PCI_EXP_DEVCTL,
				     &devctl);
		devctl &= ~(PCI_EXP_DEVCTL_PAYLOAD | PCI_EXP_DEVCTL_READRQ);
		devctl |= new_values;
		pci_write_config_word(dev, pcie_caps_offset + PCI_EXP_DEVCTL,
				      devctl);
	}
}


int __init pcibios_init(void)
{
	int i;

	pr_info("PCI: Probing PCI hardware\n");

	mdelay(250);

	
	for (i = 0; i < TILE_NUM_PCIE; i++) {
		if (pci_scan_flags[i] == 0 && controllers[i].ops != NULL) {
			struct pci_controller *controller = &controllers[i];
			struct pci_bus *bus;

			if (tile_init_irqs(i, controller)) {
				pr_err("PCI: Could not initialize IRQs\n");
				continue;
			}

			pr_info("PCI: initializing controller #%d\n", i);

			bus = pci_scan_bus(0, controller->ops, controller);
			controller->root_bus = bus;
			controller->last_busno = bus->subordinate;
		}
	}

	
	pci_fixup_irqs(pci_common_swizzle, tile_map_irq);

	pci_assign_unassigned_resources();

	
	fixup_read_and_payload_sizes();

	
	for (i = 0; i < TILE_NUM_PCIE; i++) {
		if (pci_scan_flags[i] == 0 && controllers[i].ops != NULL) {
			struct pci_bus *root_bus = controllers[i].root_bus;
			struct pci_bus *next_bus;
			struct pci_dev *dev;

			list_for_each_entry(dev, &root_bus->devices, bus_list) {
				if ((dev->class >> 8) == PCI_CLASS_BRIDGE_PCI &&
					(PCI_SLOT(dev->devfn) == 0)) {
					next_bus = dev->subordinate;
					controllers[i].mem_resources[0] =
						*next_bus->resource[0];
					controllers[i].mem_resources[1] =
						 *next_bus->resource[1];
					controllers[i].mem_resources[2] =
						 *next_bus->resource[2];

					
					pci_scan_flags[i] = 1;

					break;
				}
			}
		}
	}

	return 0;
}
subsys_initcall(pcibios_init);

void __devinit pcibios_fixup_bus(struct pci_bus *bus)
{
	
}

void pcibios_set_master(struct pci_dev *dev)
{
	
}

char __devinit *pcibios_setup(char *str)
{
	
	return str;
}

void __devinit pcibios_update_irq(struct pci_dev *dev, int irq)
{
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	u16 cmd, old_cmd;
	u8 header_type;
	int i;
	struct resource *r;

	pci_read_config_byte(dev, PCI_HEADER_TYPE, &header_type);

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {
		cmd |= PCI_COMMAND_IO;
		cmd |= PCI_COMMAND_MEMORY;
	} else {
		for (i = 0; i < 6; i++) {
			r = &dev->resource[i];
			if (r->flags & IORESOURCE_UNSET) {
				pr_err("PCI: Device %s not available "
				       "because of resource collisions\n",
				       pci_name(dev));
				return -EINVAL;
			}
			if (r->flags & IORESOURCE_IO)
				cmd |= PCI_COMMAND_IO;
			if (r->flags & IORESOURCE_MEM)
				cmd |= PCI_COMMAND_MEMORY;
		}
	}

	if (cmd != old_cmd)
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	return 0;
}



static int __devinit tile_cfg_read(struct pci_bus *bus,
				   unsigned int devfn,
				   int offset,
				   int size,
				   u32 *val)
{
	struct pci_controller *controller = bus->sysdata;
	int busnum = bus->number & 0xff;
	int slot = (devfn >> 3) & 0x1f;
	int function = devfn & 0x7;
	u32 addr;
	int config_mode = 1;

	if (busnum == 0) {
		if (slot) {
			*val = 0xFFFFFFFF;
			return 0;
		}
		config_mode = 0;
	}

	addr = busnum << 20;		
	addr |= slot << 15;		
	addr |= function << 12;		
	addr |= (offset & 0xFFF);	

	return hv_dev_pread(controller->hv_cfg_fd[config_mode], 0,
			    (HV_VirtAddr)(val), size, addr);
}


static int __devinit tile_cfg_write(struct pci_bus *bus,
				    unsigned int devfn,
				    int offset,
				    int size,
				    u32 val)
{
	struct pci_controller *controller = bus->sysdata;
	int busnum = bus->number & 0xff;
	int slot = (devfn >> 3) & 0x1f;
	int function = devfn & 0x7;
	u32 addr;
	int config_mode = 1;
	HV_VirtAddr valp = (HV_VirtAddr)&val;

	if (busnum == 0) {
		if (slot)
			return 0;
		config_mode = 0;
	}

	addr = busnum << 20;		
	addr |= slot << 15;		
	addr |= function << 12;		
	addr |= (offset & 0xFFF);	

#ifdef __BIG_ENDIAN
	
	valp += 4 - size;
#endif

	return hv_dev_pwrite(controller->hv_cfg_fd[config_mode], 0,
			     valp, size, addr);
}


static struct pci_ops tile_cfg_ops = {
	.read =         tile_cfg_read,
	.write =        tile_cfg_write,
};


#define TILE_READ(size, type)						\
type _tile_read##size(unsigned long addr)				\
{									\
	type val;							\
	int idx = 0;							\
	if (addr > controllers[0].mem_resources[1].end &&		\
	    addr > controllers[0].mem_resources[2].end)			\
		idx = 1;                                                \
	if (hv_dev_pread(controllers[idx].hv_mem_fd, 0,			\
			 (HV_VirtAddr)(&val), sizeof(type), addr))	\
		pr_err("PCI: read %zd bytes at 0x%lX failed\n",		\
		       sizeof(type), addr);				\
	return val;							\
}									\
EXPORT_SYMBOL(_tile_read##size)

TILE_READ(b, u8);
TILE_READ(w, u16);
TILE_READ(l, u32);
TILE_READ(q, u64);

#define TILE_WRITE(size, type)						\
void _tile_write##size(type val, unsigned long addr)			\
{									\
	int idx = 0;							\
	if (addr > controllers[0].mem_resources[1].end &&		\
	    addr > controllers[0].mem_resources[2].end)			\
		idx = 1;                                                \
	if (hv_dev_pwrite(controllers[idx].hv_mem_fd, 0,		\
			  (HV_VirtAddr)(&val), sizeof(type), addr))	\
		pr_err("PCI: write %zd bytes at 0x%lX failed\n",	\
		       sizeof(type), addr);				\
}									\
EXPORT_SYMBOL(_tile_write##size)

TILE_WRITE(b, u8);
TILE_WRITE(w, u16);
TILE_WRITE(l, u32);
TILE_WRITE(q, u64);
