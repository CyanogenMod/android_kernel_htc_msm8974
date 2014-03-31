/*
    comedi/drivers/skel.c
    Skeleton code for a Comedi driver

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
Driver: skel
Description: Skeleton driver, an example for driver writers
Devices:
Author: ds
Updated: Mon, 18 Mar 2002 15:34:01 -0800
Status: works

This driver is a documented example on how Comedi drivers are
written.

Configuration Options:
  none
*/


#include "../comedidev.h"

#include <linux/pci.h>		


#define SKEL_SIZE 0

#define SKEL_START_AI_CONV	0
#define SKEL_AI_READ		0

struct skel_board {
	const char *name;
	int ai_chans;
	int ai_bits;
	int have_dio;
};

static const struct skel_board skel_boards[] = {
	{
	 .name = "skel-100",
	 .ai_chans = 16,
	 .ai_bits = 12,
	 .have_dio = 1,
	 },
	{
	 .name = "skel-200",
	 .ai_chans = 8,
	 .ai_bits = 16,
	 .have_dio = 0,
	 },
};

#define PCI_VENDOR_ID_SKEL 0xdafe
static DEFINE_PCI_DEVICE_TABLE(skel_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_SKEL, 0x0100) },
	{ PCI_DEVICE(PCI_VENDOR_ID_SKEL, 0x0200) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, skel_pci_table);

#define thisboard ((const struct skel_board *)dev->board_ptr)

struct skel_private {

	int data;

	
	struct pci_dev *pci_dev;

	
	unsigned int ao_readback[2];
};

#define devpriv ((struct skel_private *)dev->private)

static int skel_attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int skel_detach(struct comedi_device *dev);
static struct comedi_driver driver_skel = {
	.driver_name = "dummy",
	.module = THIS_MODULE,
	.attach = skel_attach,
	.detach = skel_detach,
	.board_name = &skel_boards[0].name,
	.offset = sizeof(struct skel_board),
	.num_names = ARRAY_SIZE(skel_boards),
};

static int skel_ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data);
static int skel_ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data);
static int skel_ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data);
static int skel_dio_insn_bits(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
static int skel_dio_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data);
static int skel_ai_cmdtest(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_cmd *cmd);
static int skel_ns_to_timer(unsigned int *ns, int round);

static int skel_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;

	pr_info("comedi%d: skel: ", dev->minor);

	

	dev->board_name = thisboard->name;

	if (alloc_private(dev, sizeof(struct skel_private)) < 0)
		return -ENOMEM;

	if (alloc_subdevices(dev, 3) < 0)
		return -ENOMEM;

	s = dev->subdevices + 0;
	
	
	s->type = COMEDI_SUBD_AI;
	
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_DIFF;
	s->n_chan = thisboard->ai_chans;
	s->maxdata = (1 << thisboard->ai_bits) - 1;
	s->range_table = &range_bipolar10;
	s->len_chanlist = 16;	
	s->insn_read = skel_ai_rinsn;
	s->do_cmdtest = skel_ai_cmdtest;

	s = dev->subdevices + 1;
	
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE;
	s->n_chan = 1;
	s->maxdata = 0xffff;
	s->range_table = &range_bipolar5;
	s->insn_write = skel_ao_winsn;
	s->insn_read = skel_ao_rinsn;

	s = dev->subdevices + 2;
	
	if (thisboard->have_dio) {
		s->type = COMEDI_SUBD_DIO;
		s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
		s->n_chan = 16;
		s->maxdata = 1;
		s->range_table = &range_digital;
		s->insn_bits = skel_dio_insn_bits;
		s->insn_config = skel_dio_insn_config;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	pr_info("attached\n");

	return 0;
}

static int skel_detach(struct comedi_device *dev)
{
	pr_info("comedi%d: skel: remove\n", dev->minor);

	return 0;
}

static int skel_ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data)
{
	int n, i;
	unsigned int d;
	unsigned int status;

	

	
	

	

	
	for (n = 0; n < insn->n; n++) {
		
		

#define TIMEOUT 100
		
		for (i = 0; i < TIMEOUT; i++) {
			status = 1;
			
			if (status)
				break;
		}
		if (i == TIMEOUT) {
			pr_info("timeout\n");
			return -ETIMEDOUT;
		}

		
		
		d = 0;

		
		d ^= 1 << (thisboard->ai_bits - 1);

		data[n] = d;
	}

	/* return the number of samples read/written */
	return n;
}

static int skel_ai_cmdtest(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;


	

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER | TRIG_EXT;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_TIMER | TRIG_EXT;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;


	
	if (cmd->scan_begin_src != TRIG_TIMER &&
	    cmd->scan_begin_src != TRIG_EXT)
		err++;
	if (cmd->convert_src != TRIG_TIMER && cmd->convert_src != TRIG_EXT)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}
#define MAX_SPEED	10000	
#define MIN_SPEED	1000000000	

	if (cmd->scan_begin_src == TRIG_TIMER) {
		if (cmd->scan_begin_arg < MAX_SPEED) {
			cmd->scan_begin_arg = MAX_SPEED;
			err++;
		}
		if (cmd->scan_begin_arg > MIN_SPEED) {
			cmd->scan_begin_arg = MIN_SPEED;
			err++;
		}
	} else {
		
		
		
		if (cmd->scan_begin_arg > 9) {
			cmd->scan_begin_arg = 9;
			err++;
		}
	}
	if (cmd->convert_src == TRIG_TIMER) {
		if (cmd->convert_arg < MAX_SPEED) {
			cmd->convert_arg = MAX_SPEED;
			err++;
		}
		if (cmd->convert_arg > MIN_SPEED) {
			cmd->convert_arg = MIN_SPEED;
			err++;
		}
	} else {
		
		
		if (cmd->convert_arg > 9) {
			cmd->convert_arg = 9;
			err++;
		}
	}

	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
	if (cmd->stop_src == TRIG_COUNT) {
		if (cmd->stop_arg > 0x00ffffff) {
			cmd->stop_arg = 0x00ffffff;
			err++;
		}
	} else {
		
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	

	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		skel_ns_to_timer(&cmd->scan_begin_arg,
				 cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->scan_begin_arg)
			err++;
	}
	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		skel_ns_to_timer(&cmd->convert_arg,
				 cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->convert_arg)
			err++;
		if (cmd->scan_begin_src == TRIG_TIMER &&
		    cmd->scan_begin_arg <
		    cmd->convert_arg * cmd->scan_end_arg) {
			cmd->scan_begin_arg =
			    cmd->convert_arg * cmd->scan_end_arg;
			err++;
		}
	}

	if (err)
		return 4;

	return 0;
}

static int skel_ns_to_timer(unsigned int *ns, int round)
{
	

	return *ns;
}

static int skel_ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	pr_info("skel_ao_winsn\n");
	for (i = 0; i < insn->n; i++) {
		
		
		devpriv->ao_readback[chan] = data[i];
	}

	/* return the number of samples read/written */
	return i;
}

static int skel_ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_readback[chan];

	return i;
}

static int skel_dio_insn_bits(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data)
{
	if (insn->n != 2)
		return -EINVAL;

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= data[0] & data[1];
		
		
	}

	
	

	return 2;
}

static int skel_dio_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int chan = CR_CHAN(insn->chanspec);

	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
		s->io_bits |= 1 << chan;
		break;
	case INSN_CONFIG_DIO_INPUT:
		s->io_bits &= ~(1 << chan);
		break;
	case INSN_CONFIG_DIO_QUERY:
		data[1] =
		    (s->io_bits & (1 << chan)) ? COMEDI_OUTPUT : COMEDI_INPUT;
		return insn->n;
		break;
	default:
		return -EINVAL;
		break;
	}
	

	return insn->n;
}

#ifdef CONFIG_COMEDI_PCI
static int __devinit driver_skel_pci_probe(struct pci_dev *dev,
					   const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, driver_skel.driver_name);
}

static void __devexit driver_skel_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_skel_pci_driver = {
	.id_table = skel_pci_table,
	.probe = &driver_skel_pci_probe,
	.remove = __devexit_p(&driver_skel_pci_remove)
};

static int __init driver_skel_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_skel);
	if (retval < 0)
		return retval;

	driver_skel_pci_driver.name = (char *)driver_skel.driver_name;
	return pci_register_driver(&driver_skel_pci_driver);
}

static void __exit driver_skel_cleanup_module(void)
{
	pci_unregister_driver(&driver_skel_pci_driver);
	comedi_driver_unregister(&driver_skel);
}

module_init(driver_skel_init_module);
module_exit(driver_skel_cleanup_module);
#else
static int __init driver_skel_init_module(void)
{
	return comedi_driver_register(&driver_skel);
}

static void __exit driver_skel_cleanup_module(void)
{
	comedi_driver_unregister(&driver_skel);
}

module_init(driver_skel_init_module);
module_exit(driver_skel_cleanup_module);
#endif

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
