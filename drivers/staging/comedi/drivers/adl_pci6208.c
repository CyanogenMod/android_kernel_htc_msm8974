/*
    comedi/drivers/adl_pci6208.c

    Hardware driver for ADLink 6208 series cards:
	card	     | voltage output    | current output
	-------------+-------------------+---------------
	PCI-6208V    |  8 channels       | -
	PCI-6216V    | 16 channels       | -
	PCI-6208A    |  8 channels       | 8 channels

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

#define PCI6208_DRIVER_NAME	"adl_pci6208"

struct pci6208_board {
	const char *name;
	unsigned short dev_id;	
	int ao_chans;
	
};

static const struct pci6208_board pci6208_boards[] = {
	{
	 .name = "pci6208a",
	 .dev_id = 0x6208,
	 .ao_chans = 8
	 
	 }
};

static DEFINE_PCI_DEVICE_TABLE(pci6208_pci_table) = {
	
	
	{ PCI_DEVICE(PCI_VENDOR_ID_ADLINK, 0x6208) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, pci6208_pci_table);

#define thisboard ((const struct pci6208_board *)dev->board_ptr)

struct pci6208_private {
	int data;
	struct pci_dev *pci_dev;	
	unsigned int ao_readback[2];	
};

#define devpriv ((struct pci6208_private *)dev->private)

static int pci6208_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it);
static int pci6208_detach(struct comedi_device *dev);

static struct comedi_driver driver_pci6208 = {
	.driver_name = PCI6208_DRIVER_NAME,
	.module = THIS_MODULE,
	.attach = pci6208_attach,
	.detach = pci6208_detach,
};

static int __devinit driver_pci6208_pci_probe(struct pci_dev *dev,
					      const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, driver_pci6208.driver_name);
}

static void __devexit driver_pci6208_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_pci6208_pci_driver = {
	.id_table = pci6208_pci_table,
	.probe = &driver_pci6208_pci_probe,
	.remove = __devexit_p(&driver_pci6208_pci_remove)
};

static int __init driver_pci6208_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_pci6208);
	if (retval < 0)
		return retval;

	driver_pci6208_pci_driver.name = (char *)driver_pci6208.driver_name;
	return pci_register_driver(&driver_pci6208_pci_driver);
}

static void __exit driver_pci6208_cleanup_module(void)
{
	pci_unregister_driver(&driver_pci6208_pci_driver);
	comedi_driver_unregister(&driver_pci6208);
}

module_init(driver_pci6208_init_module);
module_exit(driver_pci6208_cleanup_module);

static int pci6208_find_device(struct comedi_device *dev, int bus, int slot);
static int
pci6208_pci_setup(struct pci_dev *pci_dev, unsigned long *io_base_ptr,
		  int dev_minor);

static int pci6208_ao_winsn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static int pci6208_ao_rinsn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);

static int pci6208_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int retval;
	unsigned long io_base;

	printk(KERN_INFO "comedi%d: pci6208: ", dev->minor);

	retval = alloc_private(dev, sizeof(struct pci6208_private));
	if (retval < 0)
		return retval;

	retval = pci6208_find_device(dev, it->options[0], it->options[1]);
	if (retval < 0)
		return retval;

	retval = pci6208_pci_setup(devpriv->pci_dev, &io_base, dev->minor);
	if (retval < 0)
		return retval;

	dev->iobase = io_base;
	dev->board_name = thisboard->name;

	if (alloc_subdevices(dev, 2) < 0)
		return -ENOMEM;

	s = dev->subdevices + 0;
	
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE;	
	s->n_chan = thisboard->ao_chans;
	s->maxdata = 0xffff;	
	s->range_table = &range_bipolar10;	
	s->insn_write = pci6208_ao_winsn;
	s->insn_read = pci6208_ao_rinsn;

	
	
	
	
	
	
	
	
	

	printk(KERN_INFO "attached\n");

	return 1;
}

static int pci6208_detach(struct comedi_device *dev)
{
	printk(KERN_INFO "comedi%d: pci6208: remove\n", dev->minor);

	if (devpriv && devpriv->pci_dev) {
		if (dev->iobase)
			comedi_pci_disable(devpriv->pci_dev);
		pci_dev_put(devpriv->pci_dev);
	}

	return 0;
}

static int pci6208_ao_winsn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	int i = 0, Data_Read;
	unsigned short chan = CR_CHAN(insn->chanspec);
	unsigned long invert = 1 << (16 - 1);
	unsigned long out_value;
	for (i = 0; i < insn->n; i++) {
		out_value = data[i] ^ invert;
		
		do {
			Data_Read = (inw(dev->iobase) & 1);
		} while (Data_Read);
		outw(out_value, dev->iobase + (0x02 * chan));
		devpriv->ao_readback[chan] = out_value;
	}

	/* return the number of samples read/written */
	return i;
}

static int pci6208_ao_rinsn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_readback[chan];

	return i;
}


		
		

	
	




	


static int pci6208_find_device(struct comedi_device *dev, int bus, int slot)
{
	struct pci_dev *pci_dev = NULL;
	int i;

	for_each_pci_dev(pci_dev) {
		if (pci_dev->vendor == PCI_VENDOR_ID_ADLINK) {
			for (i = 0; i < ARRAY_SIZE(pci6208_boards); i++) {
				if (pci6208_boards[i].dev_id ==
					pci_dev->device) {
					if ((bus != 0) || (slot != 0)) {
						if (pci_dev->bus->number
						    != bus ||
						    PCI_SLOT(pci_dev->devfn)
						    != slot) {
							continue;
						}
					}
					dev->board_ptr = pci6208_boards + i;
					goto found;
				}
			}
		}
	}

	printk(KERN_ERR "comedi%d: no supported board found! "
			"(req. bus/slot : %d/%d)\n",
			dev->minor, bus, slot);
	return -EIO;

found:
	printk("comedi%d: found %s (b:s:f=%d:%d:%d) , irq=%d\n",
	       dev->minor,
	       pci6208_boards[i].name,
	       pci_dev->bus->number,
	       PCI_SLOT(pci_dev->devfn),
	       PCI_FUNC(pci_dev->devfn), pci_dev->irq);

	
	
	
	

	devpriv->pci_dev = pci_dev;

	return 0;
}

static int
pci6208_pci_setup(struct pci_dev *pci_dev, unsigned long *io_base_ptr,
		  int dev_minor)
{
	unsigned long io_base, io_range, lcr_io_base, lcr_io_range;

	
	if (comedi_pci_enable(pci_dev, PCI6208_DRIVER_NAME) < 0) {
		printk(KERN_ERR "comedi%d: Failed to enable PCI device "
			"and request regions\n",
			dev_minor);
		return -EIO;
	}
	lcr_io_base = pci_resource_start(pci_dev, 1);
	lcr_io_range = pci_resource_len(pci_dev, 1);

	printk(KERN_INFO "comedi%d: local config registers at address"
			" 0x%4lx [0x%4lx]\n",
			dev_minor, lcr_io_base, lcr_io_range);

	
	io_base = pci_resource_start(pci_dev, 2);
	io_range = pci_resource_end(pci_dev, 2) - io_base + 1;

	printk("comedi%d: 6208 registers at address 0x%4lx [0x%4lx]\n",
	       dev_minor, io_base, io_range);

	*io_base_ptr = io_base;
	
	
	
	

	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
