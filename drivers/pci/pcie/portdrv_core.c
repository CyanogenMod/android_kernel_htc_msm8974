/*
 * File:	portdrv_core.c
 * Purpose:	PCI Express Port Bus Driver's Core Functions
 *
 * Copyright (C) 2004 Intel
 * Copyright (C) Tom Long Nguyen (tom.l.nguyen@intel.com)
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/pcieport_if.h>
#include <linux/aer.h>

#include "../pci.h"
#include "portdrv.h"

bool pciehp_msi_disabled;

static int __init pciehp_setup(char *str)
{
	if (!strncmp(str, "nomsi", 5))
		pciehp_msi_disabled = true;

	return 1;
}
__setup("pcie_hp=", pciehp_setup);

static void release_pcie_device(struct device *dev)
{
	kfree(to_pcie_device(dev));
}

static int pcie_port_msix_add_entry(
	struct msix_entry *entries, int new_entry, int nr_entries)
{
	int j;

	for (j = 0; j < nr_entries; j++)
		if (entries[j].entry == new_entry)
			return j;

	entries[j].entry = new_entry;
	return j;
}

static int pcie_port_enable_msix(struct pci_dev *dev, int *vectors, int mask)
{
	struct msix_entry *msix_entries;
	int idx[PCIE_PORT_DEVICE_MAXSERVICES];
	int nr_entries, status, pos, i, nvec;
	u16 reg16;
	u32 reg32;

	nr_entries = pci_msix_table_size(dev);
	if (!nr_entries)
		return -EINVAL;
	if (nr_entries > PCIE_PORT_MAX_MSIX_ENTRIES)
		nr_entries = PCIE_PORT_MAX_MSIX_ENTRIES;

	msix_entries = kzalloc(sizeof(*msix_entries) * nr_entries, GFP_KERNEL);
	if (!msix_entries)
		return -ENOMEM;

	for (i = 0; i < nr_entries; i++)
		msix_entries[i].entry = i;

	status = pci_enable_msix(dev, msix_entries, nr_entries);
	if (status)
		goto Exit;

	for (i = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++)
		idx[i] = -1;
	status = -EIO;
	nvec = 0;

	if (mask & (PCIE_PORT_SERVICE_PME | PCIE_PORT_SERVICE_HP)) {
		int entry;

		pos = pci_pcie_cap(dev);
		pci_read_config_word(dev, pos + PCI_EXP_FLAGS, &reg16);
		entry = (reg16 & PCI_EXP_FLAGS_IRQ) >> 9;
		if (entry >= nr_entries)
			goto Error;

		i = pcie_port_msix_add_entry(msix_entries, entry, nvec);
		if (i == nvec)
			nvec++;

		idx[PCIE_PORT_SERVICE_PME_SHIFT] = i;
		idx[PCIE_PORT_SERVICE_HP_SHIFT] = i;
	}

	if (mask & PCIE_PORT_SERVICE_AER) {
		int entry;

		pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
		pci_read_config_dword(dev, pos + PCI_ERR_ROOT_STATUS, &reg32);
		entry = reg32 >> 27;
		if (entry >= nr_entries)
			goto Error;

		i = pcie_port_msix_add_entry(msix_entries, entry, nvec);
		if (i == nvec)
			nvec++;

		idx[PCIE_PORT_SERVICE_AER_SHIFT] = i;
	}

	if (nvec == nr_entries) {
		status = 0;
	} else {
		
		pci_disable_msix(dev);

		
		status = pci_enable_msix(dev, msix_entries, nvec);
		if (status)
			goto Exit;
	}

	for (i = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++)
		vectors[i] = idx[i] >= 0 ? msix_entries[idx[i]].vector : -1;

 Exit:
	kfree(msix_entries);
	return status;

 Error:
	pci_disable_msix(dev);
	goto Exit;
}

static int init_service_irqs(struct pci_dev *dev, int *irqs, int mask)
{
	int i, irq = -1;

	
	if (((mask & PCIE_PORT_SERVICE_PME) && pcie_pme_no_msi()) ||
	    ((mask & PCIE_PORT_SERVICE_HP) && pciehp_no_msi())) {
		if (dev->pin)
			irq = dev->irq;
		goto no_msi;
	}

	
	if (!pcie_port_enable_msix(dev, irqs, mask))
		return 0;

	
	if (!pci_enable_msi(dev) || dev->pin)
		irq = dev->irq;

 no_msi:
	for (i = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++)
		irqs[i] = irq;
	irqs[PCIE_PORT_SERVICE_VC_SHIFT] = -1;

	if (irq < 0)
		return -ENODEV;
	return 0;
}

static void cleanup_service_irqs(struct pci_dev *dev)
{
	if (dev->msix_enabled)
		pci_disable_msix(dev);
	else if (dev->msi_enabled)
		pci_disable_msi(dev);
}

static int get_port_device_capability(struct pci_dev *dev)
{
	int services = 0, pos;
	u16 reg16;
	u32 reg32;
	int cap_mask;
	int err;

	if (pcie_ports_disabled)
		return 0;

	err = pcie_port_platform_notify(dev, &cap_mask);
	if (!pcie_ports_auto) {
		cap_mask = PCIE_PORT_SERVICE_PME | PCIE_PORT_SERVICE_HP
				| PCIE_PORT_SERVICE_VC;
		if (pci_aer_available())
			cap_mask |= PCIE_PORT_SERVICE_AER;
	} else if (err) {
			return 0;
	}

	pos = pci_pcie_cap(dev);
	pci_read_config_word(dev, pos + PCI_EXP_FLAGS, &reg16);
	
	if ((cap_mask & PCIE_PORT_SERVICE_HP) && (reg16 & PCI_EXP_FLAGS_SLOT)) {
		pci_read_config_dword(dev, pos + PCI_EXP_SLTCAP, &reg32);
		if (reg32 & PCI_EXP_SLTCAP_HPC) {
			services |= PCIE_PORT_SERVICE_HP;
			pos += PCI_EXP_SLTCTL;
			pci_read_config_word(dev, pos, &reg16);
			reg16 &= ~(PCI_EXP_SLTCTL_CCIE | PCI_EXP_SLTCTL_HPIE);
			pci_write_config_word(dev, pos, reg16);
		}
	}
	
	if ((cap_mask & PCIE_PORT_SERVICE_AER)
	    && pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR)) {
		services |= PCIE_PORT_SERVICE_AER;
		pci_disable_pcie_error_reporting(dev);
	}
	
	if (pci_find_ext_capability(dev, PCI_EXT_CAP_ID_VC))
		services |= PCIE_PORT_SERVICE_VC;
	
	if ((cap_mask & PCIE_PORT_SERVICE_PME)
	    && dev->pcie_type == PCI_EXP_TYPE_ROOT_PORT) {
		services |= PCIE_PORT_SERVICE_PME;
		pcie_pme_interrupt_enable(dev, false);
	}

	return services;
}

static int pcie_device_init(struct pci_dev *pdev, int service, int irq)
{
	int retval;
	struct pcie_device *pcie;
	struct device *device;

	pcie = kzalloc(sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;
	pcie->port = pdev;
	pcie->irq = irq;
	pcie->service = service;

	
	device = &pcie->device;
	device->bus = &pcie_port_bus_type;
	device->release = release_pcie_device;	
	dev_set_name(device, "%s:pcie%02x",
		     pci_name(pdev),
		     get_descriptor_id(pdev->pcie_type, service));
	device->parent = &pdev->dev;
	device_enable_async_suspend(device);

	retval = device_register(device);
	if (retval)
		kfree(pcie);
	else
		get_device(device);
	return retval;
}

int pcie_port_device_register(struct pci_dev *dev)
{
	int status, capabilities, i, nr_service;
	int irqs[PCIE_PORT_DEVICE_MAXSERVICES];

	
	status = pci_enable_device(dev);
	if (status)
		return status;

	
	capabilities = get_port_device_capability(dev);
	if (!capabilities)
		return 0;

	pci_set_master(dev);
	status = init_service_irqs(dev, irqs, capabilities);
	if (status) {
		capabilities &= PCIE_PORT_SERVICE_VC;
		if (!capabilities)
			goto error_disable;
	}

	
	status = -ENODEV;
	nr_service = 0;
	for (i = 0; i < PCIE_PORT_DEVICE_MAXSERVICES; i++) {
		int service = 1 << i;
		if (!(capabilities & service))
			continue;
		if (!pcie_device_init(dev, service, irqs[i]))
			nr_service++;
	}
	if (!nr_service)
		goto error_cleanup_irqs;

	return 0;

error_cleanup_irqs:
	cleanup_service_irqs(dev);
error_disable:
	pci_disable_device(dev);
	return status;
}

#ifdef CONFIG_PM
static int suspend_iter(struct device *dev, void *data)
{
	struct pcie_port_service_driver *service_driver;

	if ((dev->bus == &pcie_port_bus_type) && dev->driver) {
		service_driver = to_service_driver(dev->driver);
		if (service_driver->suspend)
			service_driver->suspend(to_pcie_device(dev));
	}
	return 0;
}

int pcie_port_device_suspend(struct device *dev)
{
	return device_for_each_child(dev, NULL, suspend_iter);
}

static int resume_iter(struct device *dev, void *data)
{
	struct pcie_port_service_driver *service_driver;

	if ((dev->bus == &pcie_port_bus_type) &&
	    (dev->driver)) {
		service_driver = to_service_driver(dev->driver);
		if (service_driver->resume)
			service_driver->resume(to_pcie_device(dev));
	}
	return 0;
}

int pcie_port_device_resume(struct device *dev)
{
	return device_for_each_child(dev, NULL, resume_iter);
}
#endif 

static int remove_iter(struct device *dev, void *data)
{
	if (dev->bus == &pcie_port_bus_type) {
		put_device(dev);
		device_unregister(dev);
	}
	return 0;
}

void pcie_port_device_remove(struct pci_dev *dev)
{
	device_for_each_child(&dev->dev, NULL, remove_iter);
	cleanup_service_irqs(dev);
	pci_disable_device(dev);
}

static int pcie_port_probe_service(struct device *dev)
{
	struct pcie_device *pciedev;
	struct pcie_port_service_driver *driver;
	int status;

	if (!dev || !dev->driver)
		return -ENODEV;

	driver = to_service_driver(dev->driver);
	if (!driver || !driver->probe)
		return -ENODEV;

	pciedev = to_pcie_device(dev);
	status = driver->probe(pciedev);
	if (!status) {
		dev_printk(KERN_DEBUG, dev, "service driver %s loaded\n",
			driver->name);
		get_device(dev);
	}
	return status;
}

static int pcie_port_remove_service(struct device *dev)
{
	struct pcie_device *pciedev;
	struct pcie_port_service_driver *driver;

	if (!dev || !dev->driver)
		return 0;

	pciedev = to_pcie_device(dev);
	driver = to_service_driver(dev->driver);
	if (driver && driver->remove) {
		dev_printk(KERN_DEBUG, dev, "unloading service driver %s\n",
			driver->name);
		driver->remove(pciedev);
		put_device(dev);
	}
	return 0;
}

static void pcie_port_shutdown_service(struct device *dev) {}

int pcie_port_service_register(struct pcie_port_service_driver *new)
{
	if (pcie_ports_disabled)
		return -ENODEV;

	new->driver.name = (char *)new->name;
	new->driver.bus = &pcie_port_bus_type;
	new->driver.probe = pcie_port_probe_service;
	new->driver.remove = pcie_port_remove_service;
	new->driver.shutdown = pcie_port_shutdown_service;

	return driver_register(&new->driver);
}
EXPORT_SYMBOL(pcie_port_service_register);

void pcie_port_service_unregister(struct pcie_port_service_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(pcie_port_service_unregister);
