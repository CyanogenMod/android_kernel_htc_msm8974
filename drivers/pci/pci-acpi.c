/*
 * File:	pci-acpi.c
 * Purpose:	Provide PCI support in ACPI
 *
 * Copyright (C) 2005 David Shaohua Li <shaohua.li@intel.com>
 * Copyright (C) 2004 Tom Long Nguyen <tom.l.nguyen@intel.com>
 * Copyright (C) 2004 Intel Corp.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/pci-aspm.h>
#include <acpi/acpi.h>
#include <acpi/acpi_bus.h>

#include <linux/pci-acpi.h>
#include <linux/pm_runtime.h>
#include "pci.h"

static DEFINE_MUTEX(pci_acpi_pm_notify_mtx);

static void pci_acpi_wake_bus(acpi_handle handle, u32 event, void *context)
{
	struct pci_bus *pci_bus = context;

	if (event == ACPI_NOTIFY_DEVICE_WAKE && pci_bus)
		pci_pme_wakeup_bus(pci_bus);
}

static void pci_acpi_wake_dev(acpi_handle handle, u32 event, void *context)
{
	struct pci_dev *pci_dev = context;

	if (event != ACPI_NOTIFY_DEVICE_WAKE || !pci_dev)
		return;

	if (!pci_dev->pm_cap || !pci_dev->pme_support
	     || pci_check_pme_status(pci_dev)) {
		if (pci_dev->pme_poll)
			pci_dev->pme_poll = false;

		pci_wakeup_event(pci_dev);
		pm_runtime_resume(&pci_dev->dev);
	}

	if (pci_dev->subordinate)
		pci_pme_wakeup_bus(pci_dev->subordinate);
}

static acpi_status add_pm_notifier(struct acpi_device *dev,
				   acpi_notify_handler handler,
				   void *context)
{
	acpi_status status = AE_ALREADY_EXISTS;

	mutex_lock(&pci_acpi_pm_notify_mtx);

	if (dev->wakeup.flags.notifier_present)
		goto out;

	status = acpi_install_notify_handler(dev->handle,
					     ACPI_SYSTEM_NOTIFY,
					     handler, context);
	if (ACPI_FAILURE(status))
		goto out;

	dev->wakeup.flags.notifier_present = true;

 out:
	mutex_unlock(&pci_acpi_pm_notify_mtx);
	return status;
}

static acpi_status remove_pm_notifier(struct acpi_device *dev,
				      acpi_notify_handler handler)
{
	acpi_status status = AE_BAD_PARAMETER;

	mutex_lock(&pci_acpi_pm_notify_mtx);

	if (!dev->wakeup.flags.notifier_present)
		goto out;

	status = acpi_remove_notify_handler(dev->handle,
					    ACPI_SYSTEM_NOTIFY,
					    handler);
	if (ACPI_FAILURE(status))
		goto out;

	dev->wakeup.flags.notifier_present = false;

 out:
	mutex_unlock(&pci_acpi_pm_notify_mtx);
	return status;
}

acpi_status pci_acpi_add_bus_pm_notifier(struct acpi_device *dev,
					 struct pci_bus *pci_bus)
{
	return add_pm_notifier(dev, pci_acpi_wake_bus, pci_bus);
}

acpi_status pci_acpi_remove_bus_pm_notifier(struct acpi_device *dev)
{
	return remove_pm_notifier(dev, pci_acpi_wake_bus);
}

acpi_status pci_acpi_add_pm_notifier(struct acpi_device *dev,
				     struct pci_dev *pci_dev)
{
	return add_pm_notifier(dev, pci_acpi_wake_dev, pci_dev);
}

acpi_status pci_acpi_remove_pm_notifier(struct acpi_device *dev)
{
	return remove_pm_notifier(dev, pci_acpi_wake_dev);
}


static pci_power_t acpi_pci_choose_state(struct pci_dev *pdev)
{
	int acpi_state;

	acpi_state = acpi_pm_device_sleep_state(&pdev->dev, NULL);
	if (acpi_state < 0)
		return PCI_POWER_ERROR;

	switch (acpi_state) {
	case ACPI_STATE_D0:
		return PCI_D0;
	case ACPI_STATE_D1:
		return PCI_D1;
	case ACPI_STATE_D2:
		return PCI_D2;
	case ACPI_STATE_D3_HOT:
		return PCI_D3hot;
	case ACPI_STATE_D3_COLD:
		return PCI_D3cold;
	}
	return PCI_POWER_ERROR;
}

static bool acpi_pci_power_manageable(struct pci_dev *dev)
{
	acpi_handle handle = DEVICE_ACPI_HANDLE(&dev->dev);

	return handle ? acpi_bus_power_manageable(handle) : false;
}

static int acpi_pci_set_power_state(struct pci_dev *dev, pci_power_t state)
{
	acpi_handle handle = DEVICE_ACPI_HANDLE(&dev->dev);
	acpi_handle tmp;
	static const u8 state_conv[] = {
		[PCI_D0] = ACPI_STATE_D0,
		[PCI_D1] = ACPI_STATE_D1,
		[PCI_D2] = ACPI_STATE_D2,
		[PCI_D3hot] = ACPI_STATE_D3,
		[PCI_D3cold] = ACPI_STATE_D3
	};
	int error = -EINVAL;

	
	if (!handle || ACPI_SUCCESS(acpi_get_handle(handle, "_EJ0", &tmp)))
		return -ENODEV;

	switch (state) {
	case PCI_D0:
	case PCI_D1:
	case PCI_D2:
	case PCI_D3hot:
	case PCI_D3cold:
		error = acpi_bus_set_power(handle, state_conv[state]);
	}

	if (!error)
		dev_printk(KERN_INFO, &dev->dev,
				"power state changed by ACPI to D%d\n", state);

	return error;
}

static bool acpi_pci_can_wakeup(struct pci_dev *dev)
{
	acpi_handle handle = DEVICE_ACPI_HANDLE(&dev->dev);

	return handle ? acpi_bus_can_wakeup(handle) : false;
}

static void acpi_pci_propagate_wakeup_enable(struct pci_bus *bus, bool enable)
{
	while (bus->parent) {
		if (!acpi_pm_device_sleep_wake(&bus->self->dev, enable))
			return;
		bus = bus->parent;
	}

	
	if (bus->bridge)
		acpi_pm_device_sleep_wake(bus->bridge, enable);
}

static int acpi_pci_sleep_wake(struct pci_dev *dev, bool enable)
{
	if (acpi_pci_can_wakeup(dev))
		return acpi_pm_device_sleep_wake(&dev->dev, enable);

	acpi_pci_propagate_wakeup_enable(dev->bus, enable);
	return 0;
}

static void acpi_pci_propagate_run_wake(struct pci_bus *bus, bool enable)
{
	while (bus->parent) {
		struct pci_dev *bridge = bus->self;

		if (bridge->pme_interrupt)
			return;
		if (!acpi_pm_device_run_wake(&bridge->dev, enable))
			return;
		bus = bus->parent;
	}

	
	if (bus->bridge)
		acpi_pm_device_run_wake(bus->bridge, enable);
}

static int acpi_pci_run_wake(struct pci_dev *dev, bool enable)
{
	if (dev->pme_interrupt)
		return 0;

	if (!acpi_pm_device_run_wake(&dev->dev, enable))
		return 0;

	acpi_pci_propagate_run_wake(dev->bus, enable);
	return 0;
}

static struct pci_platform_pm_ops acpi_pci_platform_pm = {
	.is_manageable = acpi_pci_power_manageable,
	.set_state = acpi_pci_set_power_state,
	.choose_state = acpi_pci_choose_state,
	.can_wakeup = acpi_pci_can_wakeup,
	.sleep_wake = acpi_pci_sleep_wake,
	.run_wake = acpi_pci_run_wake,
};

static int acpi_pci_find_device(struct device *dev, acpi_handle *handle)
{
	struct pci_dev * pci_dev;
	u64	addr;

	pci_dev = to_pci_dev(dev);
	
	addr = (PCI_SLOT(pci_dev->devfn) << 16) | PCI_FUNC(pci_dev->devfn);
	*handle = acpi_get_child(DEVICE_ACPI_HANDLE(dev->parent), addr);
	if (!*handle)
		return -ENODEV;
	return 0;
}

static int acpi_pci_find_root_bridge(struct device *dev, acpi_handle *handle)
{
	int num;
	unsigned int seg, bus;

	num = sscanf(dev_name(dev), "pci%04x:%02x", &seg, &bus);
	if (num != 2)
		return -ENODEV;
	*handle = acpi_get_pci_rootbridge_handle(seg, bus);
	if (!*handle)
		return -ENODEV;
	return 0;
}

static struct acpi_bus_type acpi_pci_bus = {
	.bus = &pci_bus_type,
	.find_device = acpi_pci_find_device,
	.find_bridge = acpi_pci_find_root_bridge,
};

static int __init acpi_pci_init(void)
{
	int ret;

	if (acpi_gbl_FADT.boot_flags & ACPI_FADT_NO_MSI) {
		printk(KERN_INFO"ACPI FADT declares the system doesn't support MSI, so disable it\n");
		pci_no_msi();
	}

	if (acpi_gbl_FADT.boot_flags & ACPI_FADT_NO_ASPM) {
		printk(KERN_INFO"ACPI FADT declares the system doesn't support PCIe ASPM, so disable it\n");
		pcie_no_aspm();
	}

	ret = register_acpi_bus_type(&acpi_pci_bus);
	if (ret)
		return 0;
	pci_set_platform_pm(&acpi_pci_platform_pm);
	return 0;
}
arch_initcall(acpi_pci_init);
