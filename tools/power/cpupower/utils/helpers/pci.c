#if defined(__i386__) || defined(__x86_64__)

#include <helpers/helpers.h>

struct pci_dev *pci_acc_init(struct pci_access **pacc, int domain, int bus,
			     int slot, int func, int vendor, int dev)
{
	struct pci_filter filter_nb_link = { domain, bus, slot, func,
					     vendor, dev };
	struct pci_dev *device;

	*pacc = pci_alloc();
	if (*pacc == NULL)
		return NULL;

	pci_init(*pacc);
	pci_scan_bus(*pacc);

	for (device = (*pacc)->devices; device; device = device->next) {
		if (pci_filter_match(&filter_nb_link, device))
			return device;
	}
	pci_cleanup(*pacc);
	return NULL;
}

struct pci_dev *pci_slot_func_init(struct pci_access **pacc, int slot,
				       int func)
{
	return pci_acc_init(pacc, 0, 0, slot, func, -1, -1);
}

#endif 
