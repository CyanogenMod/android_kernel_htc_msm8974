#include <linux/pci.h>
#include <linux/module.h>
#include <linux/pci-aspm.h>
#include "pci.h"

static void pci_free_resources(struct pci_dev *dev)
{
	int i;

 	msi_remove_pci_irq_vectors(dev);

	pci_cleanup_rom(dev);
	for (i = 0; i < PCI_NUM_RESOURCES; i++) {
		struct resource *res = dev->resource + i;
		if (res->parent)
			release_resource(res);
	}
}

static void pci_stop_dev(struct pci_dev *dev)
{
	if (dev->is_added) {
		pci_proc_detach_device(dev);
		pci_remove_sysfs_dev_files(dev);
		device_unregister(&dev->dev);
		dev->is_added = 0;
	}

	if (dev->bus->self)
		pcie_aspm_exit_link_state(dev);
}

static void pci_destroy_dev(struct pci_dev *dev)
{
	down_write(&pci_bus_sem);
	list_del(&dev->bus_list);
	dev->bus_list.next = dev->bus_list.prev = NULL;
	up_write(&pci_bus_sem);

	pci_free_resources(dev);
	pci_dev_put(dev);
}

#if 0
int pci_remove_device_safe(struct pci_dev *dev)
{
	if (pci_dev_driver(dev))
		return -EBUSY;
	pci_destroy_dev(dev);
	return 0;
}
#endif  

void pci_remove_bus(struct pci_bus *pci_bus)
{
	pci_proc_detach_bus(pci_bus);

	down_write(&pci_bus_sem);
	list_del(&pci_bus->node);
	up_write(&pci_bus_sem);
	if (!pci_bus->is_added)
		return;

	pci_remove_legacy_files(pci_bus);
	device_unregister(&pci_bus->dev);
}
EXPORT_SYMBOL(pci_remove_bus);

static void __pci_remove_behind_bridge(struct pci_dev *dev);
void __pci_remove_bus_device(struct pci_dev *dev)
{
	if (dev->subordinate) {
		struct pci_bus *b = dev->subordinate;

		__pci_remove_behind_bridge(dev);
		pci_remove_bus(b);
		dev->subordinate = NULL;
	}

	pci_destroy_dev(dev);
}
EXPORT_SYMBOL(__pci_remove_bus_device);

void pci_stop_and_remove_bus_device(struct pci_dev *dev)
{
	pci_stop_bus_device(dev);
	__pci_remove_bus_device(dev);
}

static void __pci_remove_behind_bridge(struct pci_dev *dev)
{
	struct list_head *l, *n;

	if (dev->subordinate)
		list_for_each_safe(l, n, &dev->subordinate->devices)
			__pci_remove_bus_device(pci_dev_b(l));
}

static void pci_stop_behind_bridge(struct pci_dev *dev)
{
	struct list_head *l, *n;

	if (dev->subordinate)
		list_for_each_safe(l, n, &dev->subordinate->devices)
			pci_stop_bus_device(pci_dev_b(l));
}

void pci_stop_and_remove_behind_bridge(struct pci_dev *dev)
{
	pci_stop_behind_bridge(dev);
	__pci_remove_behind_bridge(dev);
}

static void pci_stop_bus_devices(struct pci_bus *bus)
{
	struct list_head *l, *n;

	list_for_each_prev_safe(l, n, &bus->devices) {
		struct pci_dev *dev = pci_dev_b(l);
		pci_stop_bus_device(dev);
	}
}

void pci_stop_bus_device(struct pci_dev *dev)
{
	if (dev->subordinate)
		pci_stop_bus_devices(dev->subordinate);

	pci_stop_dev(dev);
}

EXPORT_SYMBOL(pci_stop_and_remove_bus_device);
EXPORT_SYMBOL(pci_stop_and_remove_behind_bridge);
EXPORT_SYMBOL_GPL(pci_stop_bus_device);
