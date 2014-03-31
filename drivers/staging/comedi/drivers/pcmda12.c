/*
    comedi/drivers/pcmda12.c
    Driver for Winsystems PC-104 based PCM-D/A-12 8-channel AO board.

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 2006 Calin A. Culianu <calin@ajvar.org>

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

#include <linux/pci.h>		

#define SDEV_NO ((int)(s - dev->subdevices))
#define CHANS 8
#define IOSIZE 16
#define LSB(x) ((unsigned char)((x) & 0xff))
#define MSB(x) ((unsigned char)((((unsigned short)(x))>>8) & 0xff))
#define LSB_PORT(chan) (dev->iobase + (chan)*2)
#define MSB_PORT(chan) (LSB_PORT(chan)+1)
#define BITS 12

struct pcmda12_board {
	const char *name;
};

static const struct comedi_lrange pcmda12_ranges = {
	3,
	{
	 UNI_RANGE(5), UNI_RANGE(10), BIP_RANGE(5)
	 }
};

static const struct pcmda12_board pcmda12_boards[] = {
	{
	 .name = "pcmda12",
	 },
};

#define thisboard ((const struct pcmda12_board *)dev->board_ptr)

struct pcmda12_private {

	unsigned int ao_readback[CHANS];
	int simultaneous_xfer_mode;
};

#define devpriv ((struct pcmda12_private *)(dev->private))

static int pcmda12_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it);
static int pcmda12_detach(struct comedi_device *dev);

static void zero_chans(struct comedi_device *dev);

static struct comedi_driver driver = {
	.driver_name = "pcmda12",
	.module = THIS_MODULE,
	.attach = pcmda12_attach,
	.detach = pcmda12_detach,
	.board_name = &pcmda12_boards[0].name,
	.offset = sizeof(struct pcmda12_board),
	.num_names = ARRAY_SIZE(pcmda12_boards),
};

static int ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data);
static int ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data);

static int pcmda12_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	unsigned long iobase;

	iobase = it->options[0];
	printk(KERN_INFO
	       "comedi%d: %s: io: %lx %s ", dev->minor, driver.driver_name,
	       iobase, it->options[1] ? "simultaneous xfer mode enabled" : "");

	if (!request_region(iobase, IOSIZE, driver.driver_name)) {
		printk("I/O port conflict\n");
		return -EIO;
	}
	dev->iobase = iobase;

	dev->board_name = thisboard->name;

	if (alloc_private(dev, sizeof(struct pcmda12_private)) < 0) {
		printk(KERN_ERR "cannot allocate private data structure\n");
		return -ENOMEM;
	}

	devpriv->simultaneous_xfer_mode = it->options[1];

	if (alloc_subdevices(dev, 1) < 0) {
		printk(KERN_ERR "cannot allocate subdevice data structures\n");
		return -ENOMEM;
	}

	s = dev->subdevices;
	s->private = NULL;
	s->maxdata = (0x1 << BITS) - 1;
	s->range_table = &pcmda12_ranges;
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = CHANS;
	s->insn_write = &ao_winsn;
	s->insn_read = &ao_rinsn;

	zero_chans(dev);	

	printk(KERN_INFO "attached\n");

	return 1;
}

static int pcmda12_detach(struct comedi_device *dev)
{
	printk(KERN_INFO
	       "comedi%d: %s: remove\n", dev->minor, driver.driver_name);
	if (dev->iobase)
		release_region(dev->iobase, IOSIZE);
	return 0;
}

static void zero_chans(struct comedi_device *dev)
{				
	int i;
	for (i = 0; i < CHANS; ++i) {
		outb(0, LSB_PORT(i));
		outb(0, MSB_PORT(i));
	}
	inb(LSB_PORT(0));	
}

static int ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; ++i) {


		
		
		outb(LSB(data[i]), LSB_PORT(chan));
		
		outb(MSB(data[i]), MSB_PORT(chan));

		
		devpriv->ao_readback[chan] = data[i];

		if (!devpriv->simultaneous_xfer_mode)
			inb(LSB_PORT(chan));
	}

	/* return the number of samples written */
	return i;
}

static int ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++) {
		if (devpriv->simultaneous_xfer_mode)
			inb(LSB_PORT(chan));
		
		data[i] = devpriv->ao_readback[chan];
	}

	return i;
}

static int __init driver_init_module(void)
{
	return comedi_driver_register(&driver);
}

static void __exit driver_cleanup_module(void)
{
	comedi_driver_unregister(&driver);
}

module_init(driver_init_module);
module_exit(driver_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
