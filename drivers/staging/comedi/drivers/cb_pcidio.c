/*
    comedi/drivers/cb_pcidio.c
    A Comedi driver for PCI-DIO24H & PCI-DIO48H of ComputerBoards (currently MeasurementComputing)

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 2000 David A. Schleef <ds@schleef.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "../comedidev.h"
#include "comedi_pci.h"
#include "8255.h"

#define PCI_VENDOR_ID_CB	0x1307

struct pcidio_board {
	const char *name;	
	int dev_id;
	int n_8255;		

	
	int pcicontroler_badrindex;
	int dioregs_badrindex;
};

static const struct pcidio_board pcidio_boards[] = {
	{
	 .name = "pci-dio24",
	 .dev_id = 0x0028,
	 .n_8255 = 1,
	 .pcicontroler_badrindex = 1,
	 .dioregs_badrindex = 2,
	 },
	{
	 .name = "pci-dio24h",
	 .dev_id = 0x0014,
	 .n_8255 = 1,
	 .pcicontroler_badrindex = 1,
	 .dioregs_badrindex = 2,
	 },
	{
	 .name = "pci-dio48h",
	 .dev_id = 0x000b,
	 .n_8255 = 2,
	 .pcicontroler_badrindex = 0,
	 .dioregs_badrindex = 1,
	 },
};

static DEFINE_PCI_DEVICE_TABLE(pcidio_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_CB, 0x0028) },
	{ PCI_DEVICE(PCI_VENDOR_ID_CB, 0x0014) },
	{ PCI_DEVICE(PCI_VENDOR_ID_CB, 0x000b) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, pcidio_pci_table);

#define thisboard ((const struct pcidio_board *)dev->board_ptr)

struct pcidio_private {
	int data;		

	
	struct pci_dev *pci_dev;

	
	unsigned int do_readback[4];	

	unsigned long dio_reg_base;	
};

#define devpriv ((struct pcidio_private *)dev->private)

static int pcidio_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int pcidio_detach(struct comedi_device *dev);
static struct comedi_driver driver_cb_pcidio = {
	.driver_name = "cb_pcidio",
	.module = THIS_MODULE,
	.attach = pcidio_attach,
	.detach = pcidio_detach,




};


static int pcidio_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct pci_dev *pcidev = NULL;
	int index;
	int i;

	if (alloc_private(dev, sizeof(struct pcidio_private)) < 0)
		return -ENOMEM;

	for_each_pci_dev(pcidev) {
		
		if (pcidev->vendor != PCI_VENDOR_ID_CB)
			continue;
		
		for (index = 0; index < ARRAY_SIZE(pcidio_boards); index++) {
			if (pcidio_boards[index].dev_id != pcidev->device)
				continue;

			
			if (it->options[0] || it->options[1]) {
				
				if (pcidev->bus->number != it->options[0] ||
				    PCI_SLOT(pcidev->devfn) != it->options[1]) {
					continue;
				}
			}
			dev->board_ptr = pcidio_boards + index;
			goto found;
		}
	}

	dev_err(dev->hw_dev, "No supported ComputerBoards/MeasurementComputing card found on requested position\n");
	return -EIO;

found:

	dev->board_name = thisboard->name;

	devpriv->pci_dev = pcidev;
	dev_dbg(dev->hw_dev, "Found %s on bus %i, slot %i\n", thisboard->name,
		devpriv->pci_dev->bus->number,
		PCI_SLOT(devpriv->pci_dev->devfn));
	if (comedi_pci_enable(pcidev, thisboard->name))
		return -EIO;

	devpriv->dio_reg_base
	    =
	    pci_resource_start(devpriv->pci_dev,
			       pcidio_boards[index].dioregs_badrindex);

	if (alloc_subdevices(dev, thisboard->n_8255) < 0)
		return -ENOMEM;

	for (i = 0; i < thisboard->n_8255; i++) {
		subdev_8255_init(dev, dev->subdevices + i,
				 NULL, devpriv->dio_reg_base + i * 4);
		dev_dbg(dev->hw_dev, "subdev %d: base = 0x%lx\n", i,
			devpriv->dio_reg_base + i * 4);
	}

	return 1;
}

static int pcidio_detach(struct comedi_device *dev)
{
	if (devpriv) {
		if (devpriv->pci_dev) {
			if (devpriv->dio_reg_base)
				comedi_pci_disable(devpriv->pci_dev);
			pci_dev_put(devpriv->pci_dev);
		}
	}
	if (dev->subdevices) {
		int i;
		for (i = 0; i < thisboard->n_8255; i++)
			subdev_8255_cleanup(dev, dev->subdevices + i);
	}
	return 0;
}

static int __devinit driver_cb_pcidio_pci_probe(struct pci_dev *dev,
						const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, driver_cb_pcidio.driver_name);
}

static void __devexit driver_cb_pcidio_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_cb_pcidio_pci_driver = {
	.id_table = pcidio_pci_table,
	.probe = &driver_cb_pcidio_pci_probe,
	.remove = __devexit_p(&driver_cb_pcidio_pci_remove)
};

static int __init driver_cb_pcidio_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_cb_pcidio);
	if (retval < 0)
		return retval;

	driver_cb_pcidio_pci_driver.name = (char *)driver_cb_pcidio.driver_name;
	return pci_register_driver(&driver_cb_pcidio_pci_driver);
}

static void __exit driver_cb_pcidio_cleanup_module(void)
{
	pci_unregister_driver(&driver_cb_pcidio_pci_driver);
	comedi_driver_unregister(&driver_cb_pcidio);
}

module_init(driver_cb_pcidio_init_module);
module_exit(driver_cb_pcidio_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
