
#ifndef I7300_IDLE_H
#define I7300_IDLE_H

#include <linux/pci.h>

#define IOAT_BUS 0
#define IOAT_DEVFN PCI_DEVFN(8, 0)
#define MEMCTL_BUS 0
#define MEMCTL_DEVFN PCI_DEVFN(16, 1)

struct fbd_ioat {
	unsigned int vendor;
	unsigned int ioat_dev;
	unsigned int enabled;
};


static const struct fbd_ioat fbd_ioat_list[] = {
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_IOAT_CNB, 1},
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_IOAT, 0},
	{0, 0}
};

static const struct pci_device_id pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_FBD_CNB) },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_5000_ERR) },
	{ } 
};

static inline int i7300_idle_platform_probe(struct pci_dev **fbd_dev,
						struct pci_dev **ioat_dev,
						int enable_all)
{
	int i;
	struct pci_dev *memdev, *dmadev;

	memdev = pci_get_bus_and_slot(MEMCTL_BUS, MEMCTL_DEVFN);
	if (!memdev)
		return -ENODEV;

	for (i = 0; pci_tbl[i].vendor != 0; i++) {
		if (memdev->vendor == pci_tbl[i].vendor &&
		    memdev->device == pci_tbl[i].device) {
			break;
		}
	}
	if (pci_tbl[i].vendor == 0)
		return -ENODEV;

	dmadev = pci_get_bus_and_slot(IOAT_BUS, IOAT_DEVFN);
	if (!dmadev)
		return -ENODEV;

	for (i = 0; fbd_ioat_list[i].vendor != 0; i++) {
		if (dmadev->vendor == fbd_ioat_list[i].vendor &&
		    dmadev->device == fbd_ioat_list[i].ioat_dev) {
			if (!(fbd_ioat_list[i].enabled || enable_all))
				continue;
			if (fbd_dev)
				*fbd_dev = memdev;
			if (ioat_dev)
				*ioat_dev = dmadev;

			return 0;
		}
	}
	return -ENODEV;
}

#endif
