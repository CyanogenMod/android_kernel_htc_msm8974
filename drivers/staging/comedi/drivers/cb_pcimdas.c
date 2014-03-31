/*
    comedi/drivers/cb_pcimdas.c
    Comedi driver for Computer Boards PCIM-DAS1602/16

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
/*
Driver: cb_pcimdas
Description: Measurement Computing PCI Migration series boards
Devices: [ComputerBoards] PCIM-DAS1602/16 (cb_pcimdas)
Author: Richard Bytheway
Updated: Wed, 13 Nov 2002 12:34:56 +0000
Status: experimental

Written to support the PCIM-DAS1602/16 on a 2.4 series kernel.

Configuration Options:
    [0] - PCI bus number
    [1] - PCI slot number

Developed from cb_pcidas and skel by Richard Bytheway (mocelet@sucs.org).
Only supports DIO, AO and simple AI in it's present form.
No interrupts, multi channel or FIFO AI, although the card looks like it could support this.
See http://www.mccdaq.com/PDFs/Manuals/pcim-das1602-16.pdf for more details.
*/

#include "../comedidev.h"

#include <linux/delay.h>
#include <linux/interrupt.h>

#include "comedi_pci.h"
#include "plx9052.h"
#include "8255.h"

#undef CBPCIMDAS_DEBUG

#define PCI_VENDOR_ID_COMPUTERBOARDS	0x1307


#define BADR0_SIZE 2		
#define BADR1_SIZE 4
#define BADR2_SIZE 6
#define BADR3_SIZE 16
#define BADR4_SIZE 4

#define ADC_TRIG 0
#define DAC0_OFFSET 2
#define DAC1_OFFSET 4

#define MUX_LIMITS 0
#define MAIN_CONN_DIO 1
#define ADC_STAT 2
#define ADC_CONV_STAT 3
#define ADC_INT 4
#define ADC_PACER 5
#define BURST_MODE 6
#define PROG_GAIN 7
#define CLK8254_1_DATA 8
#define CLK8254_2_DATA 9
#define CLK8254_3_DATA 10
#define CLK8254_CONTROL 11
#define USER_COUNTER 12
#define RESID_COUNT_H 13
#define RESID_COUNT_L 14

struct cb_pcimdas_board {
	const char *name;
	unsigned short device_id;
	int ai_se_chans;	
	int ai_diff_chans;	
	int ai_bits;		
	int ai_speed;		
	int ao_nchan;		
	int ao_bits;		
	int has_ao_fifo;	
	int ao_scan_speed;	
	int fifo_size;		
	int dio_bits;		
	int has_dio;		
	const struct comedi_lrange *ranges;
};

static const struct cb_pcimdas_board cb_pcimdas_boards[] = {
	{
	 .name = "PCIM-DAS1602/16",
	 .device_id = 0x56,
	 .ai_se_chans = 16,
	 .ai_diff_chans = 8,
	 .ai_bits = 16,
	 .ai_speed = 10000,	
	 .ao_nchan = 2,
	 .ao_bits = 12,
	 .has_ao_fifo = 0,	
	 .ao_scan_speed = 10000,
	 
	 .fifo_size = 1024,
	 .dio_bits = 24,
	 .has_dio = 1,
	 },
};

static DEFINE_PCI_DEVICE_TABLE(cb_pcimdas_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_COMPUTERBOARDS, 0x0056) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, cb_pcimdas_pci_table);

#define N_BOARDS 1		

#define thisboard ((const struct cb_pcimdas_board *)dev->board_ptr)

struct cb_pcimdas_private {
	int data;

	
	struct pci_dev *pci_dev;

	
	unsigned long BADR0;
	unsigned long BADR1;
	unsigned long BADR2;
	unsigned long BADR3;
	unsigned long BADR4;

	
	unsigned int ao_readback[2];

	
	unsigned short int port_a;	
	unsigned short int port_b;	
	unsigned short int port_c;	
	unsigned short int dio_mode;	

};

#define devpriv ((struct cb_pcimdas_private *)dev->private)

static int cb_pcimdas_attach(struct comedi_device *dev,
			     struct comedi_devconfig *it);
static int cb_pcimdas_detach(struct comedi_device *dev);
static struct comedi_driver driver_cb_pcimdas = {
	.driver_name = "cb_pcimdas",
	.module = THIS_MODULE,
	.attach = cb_pcimdas_attach,
	.detach = cb_pcimdas_detach,
};

static int cb_pcimdas_ai_rinsn(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
static int cb_pcimdas_ao_winsn(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
static int cb_pcimdas_ao_rinsn(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);

static int cb_pcimdas_attach(struct comedi_device *dev,
			     struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	struct pci_dev *pcidev = NULL;
	int index;
	

	if (alloc_private(dev, sizeof(struct cb_pcimdas_private)) < 0)
		return -ENOMEM;


	for_each_pci_dev(pcidev) {
		
		if (pcidev->vendor != PCI_VENDOR_ID_COMPUTERBOARDS)
			continue;
		
		for (index = 0; index < N_BOARDS; index++) {
			if (cb_pcimdas_boards[index].device_id !=
			    pcidev->device)
				continue;
			
			if (it->options[0] || it->options[1]) {
				
				if (pcidev->bus->number != it->options[0] ||
				    PCI_SLOT(pcidev->devfn) != it->options[1]) {
					continue;
				}
			}
			devpriv->pci_dev = pcidev;
			dev->board_ptr = cb_pcimdas_boards + index;
			goto found;
		}
	}

	dev_err(dev->hw_dev, "No supported ComputerBoards/MeasurementComputing card found on requested position\n");
	return -EIO;

found:

	dev_dbg(dev->hw_dev, "Found %s on bus %i, slot %i\n",
		cb_pcimdas_boards[index].name, pcidev->bus->number,
		PCI_SLOT(pcidev->devfn));

	
	switch (thisboard->device_id) {
	case 0x56:
		break;
	default:
		dev_dbg(dev->hw_dev, "THIS CARD IS UNSUPPORTED.\n"
			"PLEASE REPORT USAGE TO <mocelet@sucs.org>\n");
	}

	if (comedi_pci_enable(pcidev, "cb_pcimdas")) {
		dev_err(dev->hw_dev, "Failed to enable PCI device and request regions\n");
		return -EIO;
	}

	devpriv->BADR0 = pci_resource_start(devpriv->pci_dev, 0);
	devpriv->BADR1 = pci_resource_start(devpriv->pci_dev, 1);
	devpriv->BADR2 = pci_resource_start(devpriv->pci_dev, 2);
	devpriv->BADR3 = pci_resource_start(devpriv->pci_dev, 3);
	devpriv->BADR4 = pci_resource_start(devpriv->pci_dev, 4);

	dev_dbg(dev->hw_dev, "devpriv->BADR0 = 0x%lx\n", devpriv->BADR0);
	dev_dbg(dev->hw_dev, "devpriv->BADR1 = 0x%lx\n", devpriv->BADR1);
	dev_dbg(dev->hw_dev, "devpriv->BADR2 = 0x%lx\n", devpriv->BADR2);
	dev_dbg(dev->hw_dev, "devpriv->BADR3 = 0x%lx\n", devpriv->BADR3);
	dev_dbg(dev->hw_dev, "devpriv->BADR4 = 0x%lx\n", devpriv->BADR4);


	
	dev->board_name = thisboard->name;

	if (alloc_subdevices(dev, 3) < 0)
		return -ENOMEM;

	s = dev->subdevices + 0;
	
	
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND;
	s->n_chan = thisboard->ai_se_chans;
	s->maxdata = (1 << thisboard->ai_bits) - 1;
	s->range_table = &range_unknown;
	s->len_chanlist = 1;	
	
	s->insn_read = cb_pcimdas_ai_rinsn;

	s = dev->subdevices + 1;
	
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE;
	s->n_chan = thisboard->ao_nchan;
	s->maxdata = 1 << thisboard->ao_bits;
	s->range_table = &range_unknown;	
	s->insn_write = &cb_pcimdas_ao_winsn;
	s->insn_read = &cb_pcimdas_ao_rinsn;

	s = dev->subdevices + 2;
	
	if (thisboard->has_dio)
		subdev_8255_init(dev, s, NULL, devpriv->BADR4);
	else
		s->type = COMEDI_SUBD_UNUSED;

	return 1;
}

static int cb_pcimdas_detach(struct comedi_device *dev)
{
	if (devpriv) {
		dev_dbg(dev->hw_dev, "devpriv->BADR0 = 0x%lx\n",
			devpriv->BADR0);
		dev_dbg(dev->hw_dev, "devpriv->BADR1 = 0x%lx\n",
			devpriv->BADR1);
		dev_dbg(dev->hw_dev, "devpriv->BADR2 = 0x%lx\n",
			devpriv->BADR2);
		dev_dbg(dev->hw_dev, "devpriv->BADR3 = 0x%lx\n",
			devpriv->BADR3);
		dev_dbg(dev->hw_dev, "devpriv->BADR4 = 0x%lx\n",
			devpriv->BADR4);
	}

	if (dev->irq)
		free_irq(dev->irq, dev);
	if (devpriv) {
		if (devpriv->pci_dev) {
			if (devpriv->BADR0)
				comedi_pci_disable(devpriv->pci_dev);
			pci_dev_put(devpriv->pci_dev);
		}
	}

	return 0;
}

static int cb_pcimdas_ai_rinsn(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int n, i;
	unsigned int d;
	unsigned int busy;
	int chan = CR_CHAN(insn->chanspec);
	unsigned short chanlims;
	int maxchans;

	

	
	if ((inb(devpriv->BADR3 + 2) & 0x20) == 0)	
		maxchans = thisboard->ai_diff_chans;
	else
		maxchans = thisboard->ai_se_chans;

	if (chan > (maxchans - 1))
		return -ETIMEDOUT;	

	
	d = inb(devpriv->BADR3 + 5);
	if ((d & 0x03) > 0) {	
		d = d & 0xfd;
		outb(d, devpriv->BADR3 + 5);
	}
	outb(0x01, devpriv->BADR3 + 6);	
	outb(0x00, devpriv->BADR3 + 7);	

	
	chanlims = chan | (chan << 4);
	outb(chanlims, devpriv->BADR3 + 0);

	
	for (n = 0; n < insn->n; n++) {
		
		outw(0, devpriv->BADR2 + 0);

#define TIMEOUT 1000		
		

		
		for (i = 0; i < TIMEOUT; i++) {
			busy = inb(devpriv->BADR3 + 2) & 0x80;
			if (!busy)
				break;
		}
		if (i == TIMEOUT) {
			printk("timeout\n");
			return -ETIMEDOUT;
		}
		
		d = inw(devpriv->BADR2 + 0);

		
		

		data[n] = d;
	}

	/* return the number of samples read/written */
	return n;
}

static int cb_pcimdas_ao_winsn(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++) {
		switch (chan) {
		case 0:
			outw(data[i] & 0x0FFF, devpriv->BADR2 + DAC0_OFFSET);
			break;
		case 1:
			outw(data[i] & 0x0FFF, devpriv->BADR2 + DAC1_OFFSET);
			break;
		default:
			return -1;
		}
		devpriv->ao_readback[chan] = data[i];
	}

	/* return the number of samples read/written */
	return i;
}

static int cb_pcimdas_ao_rinsn(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_readback[chan];

	return i;
}

static int __devinit driver_cb_pcimdas_pci_probe(struct pci_dev *dev,
						 const struct pci_device_id
						 *ent)
{
	return comedi_pci_auto_config(dev, driver_cb_pcimdas.driver_name);
}

static void __devexit driver_cb_pcimdas_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_cb_pcimdas_pci_driver = {
	.id_table = cb_pcimdas_pci_table,
	.probe = &driver_cb_pcimdas_pci_probe,
	.remove = __devexit_p(&driver_cb_pcimdas_pci_remove)
};

static int __init driver_cb_pcimdas_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_cb_pcimdas);
	if (retval < 0)
		return retval;

	driver_cb_pcimdas_pci_driver.name =
	    (char *)driver_cb_pcimdas.driver_name;
	return pci_register_driver(&driver_cb_pcimdas_pci_driver);
}

static void __exit driver_cb_pcimdas_cleanup_module(void)
{
	pci_unregister_driver(&driver_cb_pcimdas_pci_driver);
	comedi_driver_unregister(&driver_cb_pcimdas);
}

module_init(driver_cb_pcimdas_init_module);
module_exit(driver_cb_pcimdas_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
