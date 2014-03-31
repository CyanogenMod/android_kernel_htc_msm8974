/*
    comedi/drivers/ni_daq_dio24.c
    Driver for National Instruments PCMCIA DAQ-Card DIO-24
    Copyright (C) 2002 Daniel Vecino Castel <dvecino@able.es>

    PCMCIA crap at end of file is adapted from dummy_cs.c 1.31 2001/08/24 12:13:13
    from the pcmcia package.
    The initial developer of the pcmcia dummy_cs.c code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

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

************************************************************************
*/

			    
#undef LABPC_DEBUG

#include <linux/interrupt.h>
#include <linux/slab.h>
#include "../comedidev.h"

#include <linux/ioport.h>

#include "8255.h"

#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>

static struct pcmcia_device *pcmcia_cur_dev;

#define DIO24_SIZE 4		

static int dio24_attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int dio24_detach(struct comedi_device *dev);

enum dio24_bustype { pcmcia_bustype };

struct dio24_board_struct {
	const char *name;
	int device_id;		
	enum dio24_bustype bustype;	
	int have_dio;		
	
	unsigned int (*read_byte) (unsigned int address);
	void (*write_byte) (unsigned int byte, unsigned int address);
};

static const struct dio24_board_struct dio24_boards[] = {
	{
	 .name = "daqcard-dio24",
	 .device_id = 0x475c,	
	 .bustype = pcmcia_bustype,
	 .have_dio = 1,
	 },
	{
	 .name = "ni_daq_dio24",
	 .device_id = 0x475c,	
	 .bustype = pcmcia_bustype,
	 .have_dio = 1,
	 },
};

#define thisboard ((const struct dio24_board_struct *)dev->board_ptr)

struct dio24_private {

	int data;		
};

#define devpriv ((struct dio24_private *)dev->private)

static struct comedi_driver driver_dio24 = {
	.driver_name = "ni_daq_dio24",
	.module = THIS_MODULE,
	.attach = dio24_attach,
	.detach = dio24_detach,
	.num_names = ARRAY_SIZE(dio24_boards),
	.board_name = &dio24_boards[0].name,
	.offset = sizeof(struct dio24_board_struct),
};

static int dio24_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	unsigned long iobase = 0;
#ifdef incomplete
	unsigned int irq = 0;
#endif
	struct pcmcia_device *link;

	
	if (alloc_private(dev, sizeof(struct dio24_private)) < 0)
		return -ENOMEM;

	
	switch (thisboard->bustype) {
	case pcmcia_bustype:
		link = pcmcia_cur_dev;	
		if (!link)
			return -EIO;
		iobase = link->resource[0]->start;
#ifdef incomplete
		irq = link->irq;
#endif
		break;
	default:
		pr_err("bug! couldn't determine board type\n");
		return -EINVAL;
		break;
	}
	pr_debug("comedi%d: ni_daq_dio24: %s, io 0x%lx", dev->minor,
		 thisboard->name, iobase);
#ifdef incomplete
	if (irq)
		pr_debug("irq %u\n", irq);
#endif

	if (iobase == 0) {
		pr_err("io base address is zero!\n");
		return -EINVAL;
	}

	dev->iobase = iobase;

#ifdef incomplete
	
	dev->irq = irq;
#endif

	dev->board_name = thisboard->name;

	if (alloc_subdevices(dev, 1) < 0)
		return -ENOMEM;

	
	s = dev->subdevices + 0;
	subdev_8255_init(dev, s, NULL, dev->iobase);

	return 0;
};

static int dio24_detach(struct comedi_device *dev)
{
	dev_info(dev->hw_dev, "comedi%d: ni_daq_dio24: remove\n", dev->minor);

	if (dev->subdevices)
		subdev_8255_cleanup(dev, dev->subdevices + 0);

	if (thisboard->bustype != pcmcia_bustype && dev->iobase)
		release_region(dev->iobase, DIO24_SIZE);
	if (dev->irq)
		free_irq(dev->irq, dev);

	return 0;
};

static void dio24_config(struct pcmcia_device *link);
static void dio24_release(struct pcmcia_device *link);
static int dio24_cs_suspend(struct pcmcia_device *p_dev);
static int dio24_cs_resume(struct pcmcia_device *p_dev);

static int dio24_cs_attach(struct pcmcia_device *);
static void dio24_cs_detach(struct pcmcia_device *);

struct local_info_t {
	struct pcmcia_device *link;
	int stop;
	struct bus_operations *bus;
};

static int dio24_cs_attach(struct pcmcia_device *link)
{
	struct local_info_t *local;

	printk(KERN_INFO "ni_daq_dio24: HOLA SOY YO - CS-attach!\n");

	dev_dbg(&link->dev, "dio24_cs_attach()\n");

	
	local = kzalloc(sizeof(struct local_info_t), GFP_KERNEL);
	if (!local)
		return -ENOMEM;
	local->link = link;
	link->priv = local;

	pcmcia_cur_dev = link;

	dio24_config(link);

	return 0;
}				

static void dio24_cs_detach(struct pcmcia_device *link)
{

	printk(KERN_INFO "ni_daq_dio24: HOLA SOY YO - cs-detach!\n");

	dev_dbg(&link->dev, "dio24_cs_detach\n");

	((struct local_info_t *)link->priv)->stop = 1;
	dio24_release(link);

	
	kfree(link->priv);

}				

static int dio24_pcmcia_config_loop(struct pcmcia_device *p_dev,
				void *priv_data)
{
	if (p_dev->config_index == 0)
		return -EINVAL;

	return pcmcia_request_io(p_dev);
}

static void dio24_config(struct pcmcia_device *link)
{
	int ret;

	printk(KERN_INFO "ni_daq_dio24: HOLA SOY YO! - config\n");

	dev_dbg(&link->dev, "dio24_config\n");

	link->config_flags |= CONF_ENABLE_IRQ | CONF_AUTO_AUDIO |
		CONF_AUTO_SET_IO;

	ret = pcmcia_loop_config(link, dio24_pcmcia_config_loop, NULL);
	if (ret) {
		dev_warn(&link->dev, "no configuration found\n");
		goto failed;
	}

	if (!link->irq)
		goto failed;

	ret = pcmcia_enable_device(link);
	if (ret)
		goto failed;

	return;

failed:
	printk(KERN_INFO "Fallo");
	dio24_release(link);

}				

static void dio24_release(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "dio24_release\n");

	pcmcia_disable_device(link);
}				

static int dio24_cs_suspend(struct pcmcia_device *link)
{
	struct local_info_t *local = link->priv;

	
	local->stop = 1;
	return 0;
}				

static int dio24_cs_resume(struct pcmcia_device *link)
{
	struct local_info_t *local = link->priv;

	local->stop = 0;
	return 0;
}				


static const struct pcmcia_device_id dio24_cs_ids[] = {
	
	PCMCIA_DEVICE_MANF_CARD(0x010b, 0x475c),	
	PCMCIA_DEVICE_NULL
};

MODULE_DEVICE_TABLE(pcmcia, dio24_cs_ids);
MODULE_AUTHOR("Daniel Vecino Castel <dvecino@able.es>");
MODULE_DESCRIPTION("Comedi driver for National Instruments "
		   "PCMCIA DAQ-Card DIO-24");
MODULE_LICENSE("GPL");

struct pcmcia_driver dio24_cs_driver = {
	.probe = dio24_cs_attach,
	.remove = dio24_cs_detach,
	.suspend = dio24_cs_suspend,
	.resume = dio24_cs_resume,
	.id_table = dio24_cs_ids,
	.owner = THIS_MODULE,
	.name = "ni_daq_dio24",
};

static int __init init_dio24_cs(void)
{
	printk("ni_daq_dio24: HOLA SOY YO!\n");
	pcmcia_register_driver(&dio24_cs_driver);
	return 0;
}

static void __exit exit_dio24_cs(void)
{
	pcmcia_unregister_driver(&dio24_cs_driver);
}

int __init init_module(void)
{
	int ret;

	ret = init_dio24_cs();
	if (ret < 0)
		return ret;

	return comedi_driver_register(&driver_dio24);
}

void __exit cleanup_module(void)
{
	exit_dio24_cs();
	comedi_driver_unregister(&driver_dio24);
}
