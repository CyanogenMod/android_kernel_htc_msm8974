/*
    comedi/drivers/ni_labpc_cs.c
    Driver for National Instruments daqcard-1200 boards
    Copyright (C) 2001, 2002, 2003 Frank Mori Hess <fmhess@users.sourceforge.net>

    PCMCIA crap is adapted from dummy_cs.c 1.31 2001/08/24 12:13:13
    from the pcmcia package.
    The initial developer of the pcmcia dummy_cs.c code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.

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

#include "../comedidev.h"

#include <linux/delay.h>
#include <linux/slab.h>

#include "8253.h"
#include "8255.h"
#include "comedi_fc.h"
#include "ni_labpc.h"

#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>

static struct pcmcia_device *pcmcia_cur_dev;

static int labpc_attach(struct comedi_device *dev, struct comedi_devconfig *it);

static const struct labpc_board_struct labpc_cs_boards[] = {
	{
	 .name = "daqcard-1200",
	 .device_id = 0x103,	
	 .ai_speed = 10000,
	 .bustype = pcmcia_bustype,
	 .register_layout = labpc_1200_layout,
	 .has_ao = 1,
	 .ai_range_table = &range_labpc_1200_ai,
	 .ai_range_code = labpc_1200_ai_gain_bits,
	 .ai_range_is_unipolar = labpc_1200_is_unipolar,
	 .ai_scan_up = 0,
	 .memory_mapped_io = 0,
	 },
	
	{
	 .name = "ni_labpc_cs",
	 .device_id = 0x103,
	 .ai_speed = 10000,
	 .bustype = pcmcia_bustype,
	 .register_layout = labpc_1200_layout,
	 .has_ao = 1,
	 .ai_range_table = &range_labpc_1200_ai,
	 .ai_range_code = labpc_1200_ai_gain_bits,
	 .ai_range_is_unipolar = labpc_1200_is_unipolar,
	 .ai_scan_up = 0,
	 .memory_mapped_io = 0,
	 },
};

#define thisboard ((const struct labpc_board_struct *)dev->board_ptr)

static struct comedi_driver driver_labpc_cs = {
	.driver_name = "ni_labpc_cs",
	.module = THIS_MODULE,
	.attach = &labpc_attach,
	.detach = &labpc_common_detach,
	.num_names = ARRAY_SIZE(labpc_cs_boards),
	.board_name = &labpc_cs_boards[0].name,
	.offset = sizeof(struct labpc_board_struct),
};

static int labpc_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	unsigned long iobase = 0;
	unsigned int irq = 0;
	struct pcmcia_device *link;

	
	if (alloc_private(dev, sizeof(struct labpc_private)) < 0)
		return -ENOMEM;

	
	switch (thisboard->bustype) {
	case pcmcia_bustype:
		link = pcmcia_cur_dev;	
		if (!link)
			return -EIO;
		iobase = link->resource[0]->start;
		irq = link->irq;
		break;
	default:
		pr_err("bug! couldn't determine board type\n");
		return -EINVAL;
		break;
	}
	return labpc_common_attach(dev, iobase, irq, 0);
}

static void labpc_config(struct pcmcia_device *link);
static void labpc_release(struct pcmcia_device *link);
static int labpc_cs_suspend(struct pcmcia_device *p_dev);
static int labpc_cs_resume(struct pcmcia_device *p_dev);

static int labpc_cs_attach(struct pcmcia_device *);
static void labpc_cs_detach(struct pcmcia_device *);

struct local_info_t {
	struct pcmcia_device *link;
	int stop;
	struct bus_operations *bus;
};

static int labpc_cs_attach(struct pcmcia_device *link)
{
	struct local_info_t *local;

	dev_dbg(&link->dev, "labpc_cs_attach()\n");

	
	local = kzalloc(sizeof(struct local_info_t), GFP_KERNEL);
	if (!local)
		return -ENOMEM;
	local->link = link;
	link->priv = local;

	pcmcia_cur_dev = link;

	labpc_config(link);

	return 0;
}				

static void labpc_cs_detach(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "labpc_cs_detach\n");

	((struct local_info_t *)link->priv)->stop = 1;
	labpc_release(link);

	
	kfree(link->priv);

}				

static int labpc_pcmcia_config_loop(struct pcmcia_device *p_dev,
				void *priv_data)
{
	if (p_dev->config_index == 0)
		return -EINVAL;

	return pcmcia_request_io(p_dev);
}


static void labpc_config(struct pcmcia_device *link)
{
	int ret;

	dev_dbg(&link->dev, "labpc_config\n");

	link->config_flags |= CONF_ENABLE_IRQ | CONF_ENABLE_PULSE_IRQ |
		CONF_AUTO_AUDIO | CONF_AUTO_SET_IO;

	ret = pcmcia_loop_config(link, labpc_pcmcia_config_loop, NULL);
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
	labpc_release(link);

}				

static void labpc_release(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "labpc_release\n");

	pcmcia_disable_device(link);
}				

static int labpc_cs_suspend(struct pcmcia_device *link)
{
	struct local_info_t *local = link->priv;

	
	local->stop = 1;
	return 0;
}				

static int labpc_cs_resume(struct pcmcia_device *link)
{
	struct local_info_t *local = link->priv;

	local->stop = 0;
	return 0;
}				

static const struct pcmcia_device_id labpc_cs_ids[] = {
	
	PCMCIA_DEVICE_MANF_CARD(0x010b, 0x0103),	
	PCMCIA_DEVICE_NULL
};

MODULE_DEVICE_TABLE(pcmcia, labpc_cs_ids);
MODULE_AUTHOR("Frank Mori Hess <fmhess@users.sourceforge.net>");
MODULE_DESCRIPTION("Comedi driver for National Instruments Lab-PC");
MODULE_LICENSE("GPL");

struct pcmcia_driver labpc_cs_driver = {
	.probe = labpc_cs_attach,
	.remove = labpc_cs_detach,
	.suspend = labpc_cs_suspend,
	.resume = labpc_cs_resume,
	.id_table = labpc_cs_ids,
	.owner = THIS_MODULE,
	.name = "daqcard-1200",
};

static int __init init_labpc_cs(void)
{
	pcmcia_register_driver(&labpc_cs_driver);
	return 0;
}

static void __exit exit_labpc_cs(void)
{
	pcmcia_unregister_driver(&labpc_cs_driver);
}

int __init labpc_init_module(void)
{
	int ret;

	ret = init_labpc_cs();
	if (ret < 0)
		return ret;

	return comedi_driver_register(&driver_labpc_cs);
}

void __exit labpc_exit_module(void)
{
	exit_labpc_cs();
	comedi_driver_unregister(&driver_labpc_cs);
}

module_init(labpc_init_module);
module_exit(labpc_exit_module);
