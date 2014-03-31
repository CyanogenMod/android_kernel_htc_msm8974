/*
 * PCIe Native PME support
 *
 * Copyright (C) 2007 - 2009 Intel Corp
 * Copyright (C) 2007 - 2009 Shaohua Li <shaohua.li@intel.com>
 * Copyright (C) 2009 Rafael J. Wysocki <rjw@sisk.pl>, Novell Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License V2.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/pcieport_if.h>
#include <linux/acpi.h>
#include <linux/pci-acpi.h>
#include <linux/pm_runtime.h>

#include "../pci.h"
#include "portdrv.h"

bool pcie_pme_msi_disabled;

static int __init pcie_pme_setup(char *str)
{
	if (!strncmp(str, "nomsi", 5))
		pcie_pme_msi_disabled = true;

	return 1;
}
__setup("pcie_pme=", pcie_pme_setup);

struct pcie_pme_service_data {
	spinlock_t lock;
	struct pcie_device *srv;
	struct work_struct work;
	bool noirq; 
};

void pcie_pme_interrupt_enable(struct pci_dev *dev, bool enable)
{
	int rtctl_pos;
	u16 rtctl;

	rtctl_pos = pci_pcie_cap(dev) + PCI_EXP_RTCTL;

	pci_read_config_word(dev, rtctl_pos, &rtctl);
	if (enable)
		rtctl |= PCI_EXP_RTCTL_PMEIE;
	else
		rtctl &= ~PCI_EXP_RTCTL_PMEIE;
	pci_write_config_word(dev, rtctl_pos, rtctl);
}

static bool pcie_pme_walk_bus(struct pci_bus *bus)
{
	struct pci_dev *dev;
	bool ret = false;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		
		if (!pci_is_pcie(dev) && pci_check_pme_status(dev)) {
			if (dev->pme_poll)
				dev->pme_poll = false;

			pci_wakeup_event(dev);
			pm_request_resume(&dev->dev);
			ret = true;
		}

		if (dev->subordinate && pcie_pme_walk_bus(dev->subordinate))
			ret = true;
	}

	return ret;
}

static bool pcie_pme_from_pci_bridge(struct pci_bus *bus, u8 devfn)
{
	struct pci_dev *dev;
	bool found = false;

	if (devfn)
		return false;

	dev = pci_dev_get(bus->self);
	if (!dev)
		return false;

	if (pci_is_pcie(dev) && dev->pcie_type == PCI_EXP_TYPE_PCI_BRIDGE) {
		down_read(&pci_bus_sem);
		if (pcie_pme_walk_bus(bus))
			found = true;
		up_read(&pci_bus_sem);
	}

	pci_dev_put(dev);
	return found;
}

static void pcie_pme_handle_request(struct pci_dev *port, u16 req_id)
{
	u8 busnr = req_id >> 8, devfn = req_id & 0xff;
	struct pci_bus *bus;
	struct pci_dev *dev;
	bool found = false;

	
	if (port->devfn == devfn && port->bus->number == busnr) {
		if (port->pme_poll)
			port->pme_poll = false;

		if (pci_check_pme_status(port)) {
			pm_request_resume(&port->dev);
			found = true;
		} else {
			down_read(&pci_bus_sem);
			found = pcie_pme_walk_bus(port->subordinate);
			up_read(&pci_bus_sem);
		}
		goto out;
	}

	
	bus = pci_find_bus(pci_domain_nr(port->bus), busnr);
	if (!bus)
		goto out;

	
	found = pcie_pme_from_pci_bridge(bus, devfn);
	if (found)
		goto out;

	
	down_read(&pci_bus_sem);
	list_for_each_entry(dev, &bus->devices, bus_list) {
		pci_dev_get(dev);
		if (dev->devfn == devfn) {
			found = true;
			break;
		}
		pci_dev_put(dev);
	}
	up_read(&pci_bus_sem);

	if (found) {
		
		found = pci_check_pme_status(dev);
		if (found) {
			if (dev->pme_poll)
				dev->pme_poll = false;

			pci_wakeup_event(dev);
			pm_request_resume(&dev->dev);
		}
		pci_dev_put(dev);
	} else if (devfn) {
		dev_dbg(&port->dev, "PME interrupt generated for "
			"non-existent device %02x:%02x.%d\n",
			busnr, PCI_SLOT(devfn), PCI_FUNC(devfn));
		found = pcie_pme_from_pci_bridge(bus, 0);
	}

 out:
	if (!found)
		dev_dbg(&port->dev, "Spurious native PME interrupt!\n");
}

static void pcie_pme_work_fn(struct work_struct *work)
{
	struct pcie_pme_service_data *data =
			container_of(work, struct pcie_pme_service_data, work);
	struct pci_dev *port = data->srv->port;
	int rtsta_pos;
	u32 rtsta;

	rtsta_pos = pci_pcie_cap(port) + PCI_EXP_RTSTA;

	spin_lock_irq(&data->lock);

	for (;;) {
		if (data->noirq)
			break;

		pci_read_config_dword(port, rtsta_pos, &rtsta);
		if (rtsta & PCI_EXP_RTSTA_PME) {
			pcie_clear_root_pme_status(port);

			spin_unlock_irq(&data->lock);
			pcie_pme_handle_request(port, rtsta & 0xffff);
			spin_lock_irq(&data->lock);

			continue;
		}

		
		if (!(rtsta & PCI_EXP_RTSTA_PENDING))
			break;

		spin_unlock_irq(&data->lock);
		cpu_relax();
		spin_lock_irq(&data->lock);
	}

	if (!data->noirq)
		pcie_pme_interrupt_enable(port, true);

	spin_unlock_irq(&data->lock);
}

static irqreturn_t pcie_pme_irq(int irq, void *context)
{
	struct pci_dev *port;
	struct pcie_pme_service_data *data;
	int rtsta_pos;
	u32 rtsta;
	unsigned long flags;

	port = ((struct pcie_device *)context)->port;
	data = get_service_data((struct pcie_device *)context);

	rtsta_pos = pci_pcie_cap(port) + PCI_EXP_RTSTA;

	spin_lock_irqsave(&data->lock, flags);
	pci_read_config_dword(port, rtsta_pos, &rtsta);

	if (!(rtsta & PCI_EXP_RTSTA_PME)) {
		spin_unlock_irqrestore(&data->lock, flags);
		return IRQ_NONE;
	}

	pcie_pme_interrupt_enable(port, false);
	spin_unlock_irqrestore(&data->lock, flags);

	
	schedule_work(&data->work);

	return IRQ_HANDLED;
}

static int pcie_pme_set_native(struct pci_dev *dev, void *ign)
{
	dev_info(&dev->dev, "Signaling PME through PCIe PME interrupt\n");

	device_set_run_wake(&dev->dev, true);
	dev->pme_interrupt = true;
	return 0;
}

static void pcie_pme_mark_devices(struct pci_dev *port)
{
	pcie_pme_set_native(port, NULL);
	if (port->subordinate) {
		pci_walk_bus(port->subordinate, pcie_pme_set_native, NULL);
	} else {
		struct pci_bus *bus = port->bus;
		struct pci_dev *dev;

		
		if (port->pcie_type != PCI_EXP_TYPE_RC_EC || !bus)
			return;

		down_read(&pci_bus_sem);
		list_for_each_entry(dev, &bus->devices, bus_list)
			if (pci_is_pcie(dev)
			    && dev->pcie_type == PCI_EXP_TYPE_RC_END)
				pcie_pme_set_native(dev, NULL);
		up_read(&pci_bus_sem);
	}
}

static int pcie_pme_probe(struct pcie_device *srv)
{
	struct pci_dev *port;
	struct pcie_pme_service_data *data;
	int ret;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	spin_lock_init(&data->lock);
	INIT_WORK(&data->work, pcie_pme_work_fn);
	data->srv = srv;
	set_service_data(srv, data);

	port = srv->port;
	pcie_pme_interrupt_enable(port, false);
	pcie_clear_root_pme_status(port);

	ret = request_irq(srv->irq, pcie_pme_irq, IRQF_SHARED, "PCIe PME", srv);
	if (ret) {
		kfree(data);
	} else {
		pcie_pme_mark_devices(port);
		pcie_pme_interrupt_enable(port, true);
	}

	return ret;
}

static int pcie_pme_suspend(struct pcie_device *srv)
{
	struct pcie_pme_service_data *data = get_service_data(srv);
	struct pci_dev *port = srv->port;

	spin_lock_irq(&data->lock);
	pcie_pme_interrupt_enable(port, false);
	pcie_clear_root_pme_status(port);
	data->noirq = true;
	spin_unlock_irq(&data->lock);

	synchronize_irq(srv->irq);

	return 0;
}

static int pcie_pme_resume(struct pcie_device *srv)
{
	struct pcie_pme_service_data *data = get_service_data(srv);
	struct pci_dev *port = srv->port;

	spin_lock_irq(&data->lock);
	data->noirq = false;
	pcie_clear_root_pme_status(port);
	pcie_pme_interrupt_enable(port, true);
	spin_unlock_irq(&data->lock);

	return 0;
}

static void pcie_pme_remove(struct pcie_device *srv)
{
	pcie_pme_suspend(srv);
	free_irq(srv->irq, srv);
	kfree(get_service_data(srv));
}

static struct pcie_port_service_driver pcie_pme_driver = {
	.name		= "pcie_pme",
	.port_type 	= PCI_EXP_TYPE_ROOT_PORT,
	.service 	= PCIE_PORT_SERVICE_PME,

	.probe		= pcie_pme_probe,
	.suspend	= pcie_pme_suspend,
	.resume		= pcie_pme_resume,
	.remove		= pcie_pme_remove,
};

static int __init pcie_pme_service_init(void)
{
	return pcie_port_service_register(&pcie_pme_driver);
}

module_init(pcie_pme_service_init);
