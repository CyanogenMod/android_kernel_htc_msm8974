
#ifndef _ICP_MULTI_H_
#define _ICP_MULTI_H_

#include "../comedidev.h"
#include "comedi_pci.h"


struct pcilst_struct {
	struct pcilst_struct *next;
	int used;
	struct pci_dev *pcidev;
	unsigned short vendor;
	unsigned short device;
	unsigned char pci_bus;
	unsigned char pci_slot;
	unsigned char pci_func;
	resource_size_t io_addr[5];
	unsigned int irq;
};

struct pcilst_struct *inova_devices;


static void pci_card_list_init(unsigned short pci_vendor, char display);
static void pci_card_list_cleanup(unsigned short pci_vendor);
static struct pcilst_struct *find_free_pci_card_by_device(unsigned short
							  vendor_id,
							  unsigned short
							  device_id);
static int find_free_pci_card_by_position(unsigned short vendor_id,
					  unsigned short device_id,
					  unsigned short pci_bus,
					  unsigned short pci_slot,
					  struct pcilst_struct **card);
static struct pcilst_struct *select_and_alloc_pci_card(unsigned short vendor_id,
						       unsigned short device_id,
						       unsigned short pci_bus,
						       unsigned short pci_slot);

static int pci_card_alloc(struct pcilst_struct *amcc);
static int pci_card_free(struct pcilst_struct *amcc);
static void pci_card_list_display(void);
static int pci_card_data(struct pcilst_struct *amcc,
			 unsigned char *pci_bus, unsigned char *pci_slot,
			 unsigned char *pci_func, resource_size_t * io_addr,
			 unsigned int *irq);


static void pci_card_list_init(unsigned short pci_vendor, char display)
{
	struct pci_dev *pcidev = NULL;
	struct pcilst_struct *inova, *last;
	int i;

	inova_devices = NULL;
	last = NULL;

	for_each_pci_dev(pcidev) {
		if (pcidev->vendor == pci_vendor) {
			inova = kzalloc(sizeof(*inova), GFP_KERNEL);
			if (!inova) {
				printk
				    ("icp_multi: pci_card_list_init: allocation failed\n");
				pci_dev_put(pcidev);
				break;
			}

			inova->pcidev = pci_dev_get(pcidev);
			if (last) {
				last->next = inova;
			} else {
				inova_devices = inova;
			}
			last = inova;

			inova->vendor = pcidev->vendor;
			inova->device = pcidev->device;
			inova->pci_bus = pcidev->bus->number;
			inova->pci_slot = PCI_SLOT(pcidev->devfn);
			inova->pci_func = PCI_FUNC(pcidev->devfn);
			for (i = 0; i < 5; i++)
				inova->io_addr[i] =
				    pci_resource_start(pcidev, i);
			inova->irq = pcidev->irq;
		}
	}

	if (display)
		pci_card_list_display();
}

static void pci_card_list_cleanup(unsigned short pci_vendor)
{
	struct pcilst_struct *inova, *next;

	for (inova = inova_devices; inova; inova = next) {
		next = inova->next;
		pci_dev_put(inova->pcidev);
		kfree(inova);
	}

	inova_devices = NULL;
}

static struct pcilst_struct *find_free_pci_card_by_device(unsigned short
							  vendor_id,
							  unsigned short
							  device_id)
{
	struct pcilst_struct *inova, *next;

	for (inova = inova_devices; inova; inova = next) {
		next = inova->next;
		if ((!inova->used) && (inova->device == device_id)
		    && (inova->vendor == vendor_id))
			return inova;

	}

	return NULL;
}

static int find_free_pci_card_by_position(unsigned short vendor_id,
					  unsigned short device_id,
					  unsigned short pci_bus,
					  unsigned short pci_slot,
					  struct pcilst_struct **card)
{
	struct pcilst_struct *inova, *next;

	*card = NULL;
	for (inova = inova_devices; inova; inova = next) {
		next = inova->next;
		if ((inova->vendor == vendor_id) && (inova->device == device_id)
		    && (inova->pci_bus == pci_bus)
		    && (inova->pci_slot == pci_slot)) {
			if (!(inova->used)) {
				*card = inova;
				return 0;	
			} else {
				return 2;	
			}
		}
	}

	return 1;		
}

static int pci_card_alloc(struct pcilst_struct *inova)
{
	int i;

	if (!inova) {
		printk(" - BUG!! inova is NULL!\n");
		return -1;
	}

	if (inova->used)
		return 1;
	if (comedi_pci_enable(inova->pcidev, "icp_multi")) {
		printk(" - Can't enable PCI device and request regions!\n");
		return -1;
	}
	
	for (i = 0; i < 5; i++)
		inova->io_addr[i] = pci_resource_start(inova->pcidev, i);
	inova->irq = inova->pcidev->irq;
	inova->used = 1;
	return 0;
}

static int pci_card_free(struct pcilst_struct *inova)
{
	if (!inova)
		return -1;

	if (!inova->used)
		return 1;
	inova->used = 0;
	comedi_pci_disable(inova->pcidev);
	return 0;
}

static void pci_card_list_display(void)
{
	struct pcilst_struct *inova, *next;

	printk("Anne's List of pci cards\n");
	printk("bus:slot:func vendor device io_inova io_daq irq used\n");

	for (inova = inova_devices; inova; inova = next) {
		next = inova->next;
		printk
		    ("%2d   %2d   %2d  0x%4x 0x%4x   0x%8llx 0x%8llx  %2u  %2d\n",
		     inova->pci_bus, inova->pci_slot, inova->pci_func,
		     inova->vendor, inova->device,
		     (unsigned long long)inova->io_addr[0],
		     (unsigned long long)inova->io_addr[2], inova->irq,
		     inova->used);

	}
}

static int pci_card_data(struct pcilst_struct *inova,
			 unsigned char *pci_bus, unsigned char *pci_slot,
			 unsigned char *pci_func, resource_size_t * io_addr,
			 unsigned int *irq)
{
	int i;

	if (!inova)
		return -1;
	*pci_bus = inova->pci_bus;
	*pci_slot = inova->pci_slot;
	*pci_func = inova->pci_func;
	for (i = 0; i < 5; i++)
		io_addr[i] = inova->io_addr[i];
	*irq = inova->irq;
	return 0;
}

static struct pcilst_struct *select_and_alloc_pci_card(unsigned short vendor_id,
						       unsigned short device_id,
						       unsigned short pci_bus,
						       unsigned short pci_slot)
{
	struct pcilst_struct *card;
	int err;

	if ((pci_bus < 1) & (pci_slot < 1)) {	

		card = find_free_pci_card_by_device(vendor_id, device_id);
		if (card == NULL) {
			printk(" - Unused card not found in system!\n");
			return NULL;
		}
	} else {
		switch (find_free_pci_card_by_position(vendor_id, device_id,
						       pci_bus, pci_slot,
						       &card)) {
		case 1:
			printk
			    (" - Card not found on requested position b:s %d:%d!\n",
			     pci_bus, pci_slot);
			return NULL;
		case 2:
			printk
			    (" - Card on requested position is used b:s %d:%d!\n",
			     pci_bus, pci_slot);
			return NULL;
		}
	}

	err = pci_card_alloc(card);
	if (err != 0) {
		if (err > 0)
			printk(" - Can't allocate card!\n");
		
		return NULL;
	}

	return card;
}

#endif
