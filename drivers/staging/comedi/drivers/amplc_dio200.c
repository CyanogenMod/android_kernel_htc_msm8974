/*
    comedi/drivers/amplc_dio200.c
    Driver for Amplicon PC272E and PCI272 DIO boards.
    (Support for other boards in Amplicon 200 series may be added at
    a later date, e.g. PCI215.)

    Copyright (C) 2005 MEV Ltd. <http://www.mev.co.uk/>

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 1998,2000 David A. Schleef <ds@schleef.org>

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

#include <linux/interrupt.h>
#include <linux/slab.h>

#include "../comedidev.h"

#include "comedi_pci.h"

#include "8255.h"
#include "8253.h"

#define DIO200_DRIVER_NAME	"amplc_dio200"

#define PCI_VENDOR_ID_AMPLICON 0x14dc
#define PCI_DEVICE_ID_AMPLICON_PCI272 0x000a
#define PCI_DEVICE_ID_AMPLICON_PCI215 0x000b
#define PCI_DEVICE_ID_INVALID 0xffff

#define DIO200_IO_SIZE		0x20
#define DIO200_XCLK_SCE		0x18	
#define DIO200_YCLK_SCE		0x19	
#define DIO200_ZCLK_SCE		0x1a	
#define DIO200_XGAT_SCE		0x1b	
#define DIO200_YGAT_SCE		0x1c	
#define DIO200_ZGAT_SCE		0x1d	
#define DIO200_INT_SCE		0x1e	

#define CLK_SCE(which, chan, source) (((which) << 5) | ((chan) << 3) | (source))
#define GAT_SCE(which, chan, source) (((which) << 5) | ((chan) << 3) | (source))

static const unsigned clock_period[8] = {
	0,			
	100,			
	1000,			
	10000,			
	100000,			
	1000000,		
	0,			
	0			
};


enum dio200_bustype { isa_bustype, pci_bustype };

enum dio200_model {
	pc212e_model,
	pc214e_model,
	pc215e_model, pci215_model,
	pc218e_model,
	pc272e_model, pci272_model,
	anypci_model
};

enum dio200_layout {
	pc212_layout,
	pc214_layout,
	pc215_layout,
	pc218_layout,
	pc272_layout
};

struct dio200_board {
	const char *name;
	unsigned short devid;
	enum dio200_bustype bustype;
	enum dio200_model model;
	enum dio200_layout layout;
};

static const struct dio200_board dio200_boards[] = {
	{
	 .name = "pc212e",
	 .bustype = isa_bustype,
	 .model = pc212e_model,
	 .layout = pc212_layout,
	 },
	{
	 .name = "pc214e",
	 .bustype = isa_bustype,
	 .model = pc214e_model,
	 .layout = pc214_layout,
	 },
	{
	 .name = "pc215e",
	 .bustype = isa_bustype,
	 .model = pc215e_model,
	 .layout = pc215_layout,
	 },
#ifdef CONFIG_COMEDI_PCI
	{
	 .name = "pci215",
	 .devid = PCI_DEVICE_ID_AMPLICON_PCI215,
	 .bustype = pci_bustype,
	 .model = pci215_model,
	 .layout = pc215_layout,
	 },
#endif
	{
	 .name = "pc218e",
	 .bustype = isa_bustype,
	 .model = pc218e_model,
	 .layout = pc218_layout,
	 },
	{
	 .name = "pc272e",
	 .bustype = isa_bustype,
	 .model = pc272e_model,
	 .layout = pc272_layout,
	 },
#ifdef CONFIG_COMEDI_PCI
	{
	 .name = "pci272",
	 .devid = PCI_DEVICE_ID_AMPLICON_PCI272,
	 .bustype = pci_bustype,
	 .model = pci272_model,
	 .layout = pc272_layout,
	 },
#endif
#ifdef CONFIG_COMEDI_PCI
	{
	 .name = DIO200_DRIVER_NAME,
	 .devid = PCI_DEVICE_ID_INVALID,
	 .bustype = pci_bustype,
	 .model = anypci_model,	
	 },
#endif
};


enum dio200_sdtype { sd_none, sd_intr, sd_8255, sd_8254 };

#define DIO200_MAX_SUBDEVS	7
#define DIO200_MAX_ISNS		6

struct dio200_layout_struct {
	unsigned short n_subdevs;	
	unsigned char sdtype[DIO200_MAX_SUBDEVS];	
	unsigned char sdinfo[DIO200_MAX_SUBDEVS];	
	char has_int_sce;	
	char has_clk_gat_sce;	
};

static const struct dio200_layout_struct dio200_layouts[] = {
	[pc212_layout] = {
			  .n_subdevs = 6,
			  .sdtype = {sd_8255, sd_8254, sd_8254, sd_8254,
				     sd_8254,
				     sd_intr},
			  .sdinfo = {0x00, 0x08, 0x0C, 0x10, 0x14,
				     0x3F},
			  .has_int_sce = 1,
			  .has_clk_gat_sce = 1,
			  },
	[pc214_layout] = {
			  .n_subdevs = 4,
			  .sdtype = {sd_8255, sd_8255, sd_8254,
				     sd_intr},
			  .sdinfo = {0x00, 0x08, 0x10, 0x01},
			  .has_int_sce = 0,
			  .has_clk_gat_sce = 0,
			  },
	[pc215_layout] = {
			  .n_subdevs = 5,
			  .sdtype = {sd_8255, sd_8255, sd_8254,
				     sd_8254,
				     sd_intr},
			  .sdinfo = {0x00, 0x08, 0x10, 0x14, 0x3F},
			  .has_int_sce = 1,
			  .has_clk_gat_sce = 1,
			  },
	[pc218_layout] = {
			  .n_subdevs = 7,
			  .sdtype = {sd_8254, sd_8254, sd_8255, sd_8254,
				     sd_8254,
				     sd_intr},
			  .sdinfo = {0x00, 0x04, 0x08, 0x0C, 0x10,
				     0x14,
				     0x3F},
			  .has_int_sce = 1,
			  .has_clk_gat_sce = 1,
			  },
	[pc272_layout] = {
			  .n_subdevs = 4,
			  .sdtype = {sd_8255, sd_8255, sd_8255,
				     sd_intr},
			  .sdinfo = {0x00, 0x08, 0x10, 0x3F},
			  .has_int_sce = 1,
			  .has_clk_gat_sce = 0,
			  },
};


#ifdef CONFIG_COMEDI_PCI
static DEFINE_PCI_DEVICE_TABLE(dio200_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMPLICON, PCI_DEVICE_ID_AMPLICON_PCI215) },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMPLICON, PCI_DEVICE_ID_AMPLICON_PCI272) },
	{0}
};

MODULE_DEVICE_TABLE(pci, dio200_pci_table);
#endif 

#define thisboard ((const struct dio200_board *)dev->board_ptr)
#define thislayout (&dio200_layouts[((struct dio200_board *) \
		    dev->board_ptr)->layout])

struct dio200_private {
#ifdef CONFIG_COMEDI_PCI
	struct pci_dev *pci_dev;	
#endif
	int intr_sd;
};

#define devpriv ((struct dio200_private *)dev->private)

struct dio200_subdev_8254 {
	unsigned long iobase;	
	unsigned long clk_sce_iobase;	
	unsigned long gat_sce_iobase;	
	int which;		
	int has_clk_gat_sce;
	unsigned clock_src[3];	
	unsigned gate_src[3];	
	spinlock_t spinlock;
};

struct dio200_subdev_intr {
	unsigned long iobase;
	spinlock_t spinlock;
	int active;
	int has_int_sce;
	unsigned int valid_isns;
	unsigned int enabled_isns;
	unsigned int stopcount;
	int continuous;
};

static int dio200_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int dio200_detach(struct comedi_device *dev);
static struct comedi_driver driver_amplc_dio200 = {
	.driver_name = DIO200_DRIVER_NAME,
	.module = THIS_MODULE,
	.attach = dio200_attach,
	.detach = dio200_detach,
	.board_name = &dio200_boards[0].name,
	.offset = sizeof(struct dio200_board),
	.num_names = ARRAY_SIZE(dio200_boards),
};

#ifdef CONFIG_COMEDI_PCI
static int __devinit driver_amplc_dio200_pci_probe(struct pci_dev *dev,
						   const struct pci_device_id
						   *ent)
{
	return comedi_pci_auto_config(dev, driver_amplc_dio200.driver_name);
}

static void __devexit driver_amplc_dio200_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_amplc_dio200_pci_driver = {
	.id_table = dio200_pci_table,
	.probe = &driver_amplc_dio200_pci_probe,
	.remove = __devexit_p(&driver_amplc_dio200_pci_remove)
};

static int __init driver_amplc_dio200_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_amplc_dio200);
	if (retval < 0)
		return retval;

	driver_amplc_dio200_pci_driver.name =
	    (char *)driver_amplc_dio200.driver_name;
	return pci_register_driver(&driver_amplc_dio200_pci_driver);
}

static void __exit driver_amplc_dio200_cleanup_module(void)
{
	pci_unregister_driver(&driver_amplc_dio200_pci_driver);
	comedi_driver_unregister(&driver_amplc_dio200);
}

module_init(driver_amplc_dio200_init_module);
module_exit(driver_amplc_dio200_cleanup_module);
#else
static int __init driver_amplc_dio200_init_module(void)
{
	return comedi_driver_register(&driver_amplc_dio200);
}

static void __exit driver_amplc_dio200_cleanup_module(void)
{
	comedi_driver_unregister(&driver_amplc_dio200);
}

module_init(driver_amplc_dio200_init_module);
module_exit(driver_amplc_dio200_cleanup_module);
#endif

#ifdef CONFIG_COMEDI_PCI
static int
dio200_find_pci(struct comedi_device *dev, int bus, int slot,
		struct pci_dev **pci_dev_p)
{
	struct pci_dev *pci_dev = NULL;

	*pci_dev_p = NULL;

	
	for (pci_dev = pci_get_device(PCI_VENDOR_ID_AMPLICON, PCI_ANY_ID, NULL);
	     pci_dev != NULL;
	     pci_dev = pci_get_device(PCI_VENDOR_ID_AMPLICON,
				      PCI_ANY_ID, pci_dev)) {
		
		if (bus || slot) {
			if (bus != pci_dev->bus->number
			    || slot != PCI_SLOT(pci_dev->devfn))
				continue;
		}
		if (thisboard->model == anypci_model) {
			
			int i;

			for (i = 0; i < ARRAY_SIZE(dio200_boards); i++) {
				if (dio200_boards[i].bustype != pci_bustype)
					continue;
				if (pci_dev->device == dio200_boards[i].devid) {
					
					dev->board_ptr = &dio200_boards[i];
					break;
				}
			}
			if (i == ARRAY_SIZE(dio200_boards))
				continue;
		} else {
			
			if (pci_dev->device != thisboard->devid)
				continue;
		}

		
		*pci_dev_p = pci_dev;
		return 0;
	}
	
	if (bus || slot) {
		printk(KERN_ERR
		       "comedi%d: error! no %s found at pci %02x:%02x!\n",
		       dev->minor, thisboard->name, bus, slot);
	} else {
		printk(KERN_ERR "comedi%d: error! no %s found!\n",
		       dev->minor, thisboard->name);
	}
	return -EIO;
}
#endif

static int
dio200_request_region(unsigned minor, unsigned long from, unsigned long extent)
{
	if (!from || !request_region(from, extent, DIO200_DRIVER_NAME)) {
		printk(KERN_ERR "comedi%d: I/O port conflict (%#lx,%lu)!\n",
		       minor, from, extent);
		return -EIO;
	}
	return 0;
}

static int
dio200_subdev_intr_insn_bits(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data)
{
	struct dio200_subdev_intr *subpriv = s->private;

	if (subpriv->has_int_sce) {
		
		data[1] = inb(subpriv->iobase) & subpriv->valid_isns;
	} else {
		
		data[0] = 0;
	}

	return 2;
}

static void dio200_stop_intr(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	struct dio200_subdev_intr *subpriv = s->private;

	subpriv->active = 0;
	subpriv->enabled_isns = 0;
	if (subpriv->has_int_sce)
		outb(0, subpriv->iobase);
}

static int dio200_start_intr(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	unsigned int n;
	unsigned isn_bits;
	struct dio200_subdev_intr *subpriv = s->private;
	struct comedi_cmd *cmd = &s->async->cmd;
	int retval = 0;

	if (!subpriv->continuous && subpriv->stopcount == 0) {
		
		s->async->events |= COMEDI_CB_EOA;
		subpriv->active = 0;
		retval = 1;
	} else {
		
		isn_bits = 0;
		if (cmd->chanlist) {
			for (n = 0; n < cmd->chanlist_len; n++)
				isn_bits |= (1U << CR_CHAN(cmd->chanlist[n]));
		}
		isn_bits &= subpriv->valid_isns;
		
		subpriv->enabled_isns = isn_bits;
		if (subpriv->has_int_sce)
			outb(isn_bits, subpriv->iobase);
	}

	return retval;
}

static int
dio200_inttrig_start_intr(struct comedi_device *dev, struct comedi_subdevice *s,
			  unsigned int trignum)
{
	struct dio200_subdev_intr *subpriv;
	unsigned long flags;
	int event = 0;

	if (trignum != 0)
		return -EINVAL;

	subpriv = s->private;

	spin_lock_irqsave(&subpriv->spinlock, flags);
	s->async->inttrig = NULL;
	if (subpriv->active)
		event = dio200_start_intr(dev, s);

	spin_unlock_irqrestore(&subpriv->spinlock, flags);

	if (event)
		comedi_event(dev, s);

	return 1;
}

static int dio200_handle_read_intr(struct comedi_device *dev,
				   struct comedi_subdevice *s)
{
	struct dio200_subdev_intr *subpriv = s->private;
	unsigned triggered;
	unsigned intstat;
	unsigned cur_enabled;
	unsigned int oldevents;
	unsigned long flags;

	triggered = 0;

	spin_lock_irqsave(&subpriv->spinlock, flags);
	oldevents = s->async->events;
	if (subpriv->has_int_sce) {
		cur_enabled = subpriv->enabled_isns;
		while ((intstat = (inb(subpriv->iobase) & subpriv->valid_isns
				   & ~triggered)) != 0) {
			triggered |= intstat;
			cur_enabled &= ~triggered;
			outb(cur_enabled, subpriv->iobase);
		}
	} else {
		triggered = subpriv->enabled_isns;
	}

	if (triggered) {
		cur_enabled = subpriv->enabled_isns;
		if (subpriv->has_int_sce)
			outb(cur_enabled, subpriv->iobase);

		if (subpriv->active) {
			if (triggered & subpriv->enabled_isns) {
				
				short val;
				unsigned int n, ch, len;

				val = 0;
				len = s->async->cmd.chanlist_len;
				for (n = 0; n < len; n++) {
					ch = CR_CHAN(s->async->cmd.chanlist[n]);
					if (triggered & (1U << ch))
						val |= (1U << n);
				}
				
				if (comedi_buf_put(s->async, val)) {
					s->async->events |= (COMEDI_CB_BLOCK |
							     COMEDI_CB_EOS);
				} else {
					
					dio200_stop_intr(dev, s);
					s->async->events |= COMEDI_CB_ERROR
					    | COMEDI_CB_OVERFLOW;
					comedi_error(dev, "buffer overflow");
				}

				
				if (!subpriv->continuous) {
					
					if (subpriv->stopcount > 0) {
						subpriv->stopcount--;
						if (subpriv->stopcount == 0) {
							s->async->events |=
							    COMEDI_CB_EOA;
							dio200_stop_intr(dev,
									 s);
						}
					}
				}
			}
		}
	}
	spin_unlock_irqrestore(&subpriv->spinlock, flags);

	if (oldevents != s->async->events)
		comedi_event(dev, s);

	return (triggered != 0);
}

static int dio200_subdev_intr_cancel(struct comedi_device *dev,
				     struct comedi_subdevice *s)
{
	struct dio200_subdev_intr *subpriv = s->private;
	unsigned long flags;

	spin_lock_irqsave(&subpriv->spinlock, flags);
	if (subpriv->active)
		dio200_stop_intr(dev, s);

	spin_unlock_irqrestore(&subpriv->spinlock, flags);

	return 0;
}

static int
dio200_subdev_intr_cmdtest(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	unsigned int tmp;

	

	tmp = cmd->start_src;
	cmd->start_src &= (TRIG_NOW | TRIG_INT);
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_EXT;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_NOW;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= (TRIG_COUNT | TRIG_NONE);
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;


	
	if ((cmd->start_src & (cmd->start_src - 1)) != 0)
		err++;
	if ((cmd->scan_begin_src & (cmd->scan_begin_src - 1)) != 0)
		err++;
	if ((cmd->convert_src & (cmd->convert_src - 1)) != 0)
		err++;
	if ((cmd->scan_end_src & (cmd->scan_end_src - 1)) != 0)
		err++;
	if ((cmd->stop_src & (cmd->stop_src - 1)) != 0)
		err++;

	if (err)
		return 2;

	

	
	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}

	
	if (cmd->scan_begin_arg != 0) {
		cmd->scan_begin_arg = 0;
		err++;
	}

	
	if (cmd->convert_arg != 0) {
		cmd->convert_arg = 0;
		err++;
	}

	
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}

	switch (cmd->stop_src) {
	case TRIG_COUNT:
		
		break;
	case TRIG_NONE:
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
		break;
	default:
		break;
	}

	if (err)
		return 3;

	

	

	return 0;
}

static int dio200_subdev_intr_cmd(struct comedi_device *dev,
				  struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	struct dio200_subdev_intr *subpriv = s->private;
	unsigned long flags;
	int event = 0;

	spin_lock_irqsave(&subpriv->spinlock, flags);
	subpriv->active = 1;

	
	switch (cmd->stop_src) {
	case TRIG_COUNT:
		subpriv->continuous = 0;
		subpriv->stopcount = cmd->stop_arg;
		break;
	default:
		
		subpriv->continuous = 1;
		subpriv->stopcount = 0;
		break;
	}

	
	switch (cmd->start_src) {
	case TRIG_INT:
		s->async->inttrig = dio200_inttrig_start_intr;
		break;
	default:
		
		event = dio200_start_intr(dev, s);
		break;
	}
	spin_unlock_irqrestore(&subpriv->spinlock, flags);

	if (event)
		comedi_event(dev, s);

	return 0;
}

static int
dio200_subdev_intr_init(struct comedi_device *dev, struct comedi_subdevice *s,
			unsigned long iobase, unsigned valid_isns,
			int has_int_sce)
{
	struct dio200_subdev_intr *subpriv;

	subpriv = kzalloc(sizeof(*subpriv), GFP_KERNEL);
	if (!subpriv) {
		printk(KERN_ERR "comedi%d: error! out of memory!\n",
		       dev->minor);
		return -ENOMEM;
	}
	subpriv->iobase = iobase;
	subpriv->has_int_sce = has_int_sce;
	subpriv->valid_isns = valid_isns;
	spin_lock_init(&subpriv->spinlock);

	if (has_int_sce)
		outb(0, subpriv->iobase);	

	s->private = subpriv;
	s->type = COMEDI_SUBD_DI;
	s->subdev_flags = SDF_READABLE | SDF_CMD_READ;
	if (has_int_sce) {
		s->n_chan = DIO200_MAX_ISNS;
		s->len_chanlist = DIO200_MAX_ISNS;
	} else {
		
		s->n_chan = 1;
		s->len_chanlist = 1;
	}
	s->range_table = &range_digital;
	s->maxdata = 1;
	s->insn_bits = dio200_subdev_intr_insn_bits;
	s->do_cmdtest = dio200_subdev_intr_cmdtest;
	s->do_cmd = dio200_subdev_intr_cmd;
	s->cancel = dio200_subdev_intr_cancel;

	return 0;
}

static void
dio200_subdev_intr_cleanup(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	struct dio200_subdev_intr *subpriv = s->private;
	kfree(subpriv);
}

static irqreturn_t dio200_interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	int handled;

	if (!dev->attached)
		return IRQ_NONE;

	if (devpriv->intr_sd >= 0) {
		handled = dio200_handle_read_intr(dev,
						  dev->subdevices +
						  devpriv->intr_sd);
	} else {
		handled = 0;
	}

	return IRQ_RETVAL(handled);
}

static int
dio200_subdev_8254_read(struct comedi_device *dev, struct comedi_subdevice *s,
			struct comedi_insn *insn, unsigned int *data)
{
	struct dio200_subdev_8254 *subpriv = s->private;
	int chan = CR_CHAN(insn->chanspec);
	unsigned long flags;

	spin_lock_irqsave(&subpriv->spinlock, flags);
	data[0] = i8254_read(subpriv->iobase, 0, chan);
	spin_unlock_irqrestore(&subpriv->spinlock, flags);

	return 1;
}

static int
dio200_subdev_8254_write(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data)
{
	struct dio200_subdev_8254 *subpriv = s->private;
	int chan = CR_CHAN(insn->chanspec);
	unsigned long flags;

	spin_lock_irqsave(&subpriv->spinlock, flags);
	i8254_write(subpriv->iobase, 0, chan, data[0]);
	spin_unlock_irqrestore(&subpriv->spinlock, flags);

	return 1;
}

static int
dio200_set_gate_src(struct dio200_subdev_8254 *subpriv,
		    unsigned int counter_number, unsigned int gate_src)
{
	unsigned char byte;

	if (!subpriv->has_clk_gat_sce)
		return -1;
	if (counter_number > 2)
		return -1;
	if (gate_src > 7)
		return -1;

	subpriv->gate_src[counter_number] = gate_src;
	byte = GAT_SCE(subpriv->which, counter_number, gate_src);
	outb(byte, subpriv->gat_sce_iobase);

	return 0;
}

static int
dio200_get_gate_src(struct dio200_subdev_8254 *subpriv,
		    unsigned int counter_number)
{
	if (!subpriv->has_clk_gat_sce)
		return -1;
	if (counter_number > 2)
		return -1;

	return subpriv->gate_src[counter_number];
}

static int
dio200_set_clock_src(struct dio200_subdev_8254 *subpriv,
		     unsigned int counter_number, unsigned int clock_src)
{
	unsigned char byte;

	if (!subpriv->has_clk_gat_sce)
		return -1;
	if (counter_number > 2)
		return -1;
	if (clock_src > 7)
		return -1;

	subpriv->clock_src[counter_number] = clock_src;
	byte = CLK_SCE(subpriv->which, counter_number, clock_src);
	outb(byte, subpriv->clk_sce_iobase);

	return 0;
}

static int
dio200_get_clock_src(struct dio200_subdev_8254 *subpriv,
		     unsigned int counter_number, unsigned int *period_ns)
{
	unsigned clock_src;

	if (!subpriv->has_clk_gat_sce)
		return -1;
	if (counter_number > 2)
		return -1;

	clock_src = subpriv->clock_src[counter_number];
	*period_ns = clock_period[clock_src];
	return clock_src;
}

static int
dio200_subdev_8254_config(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data)
{
	struct dio200_subdev_8254 *subpriv = s->private;
	int ret = 0;
	int chan = CR_CHAN(insn->chanspec);
	unsigned long flags;

	spin_lock_irqsave(&subpriv->spinlock, flags);
	switch (data[0]) {
	case INSN_CONFIG_SET_COUNTER_MODE:
		ret = i8254_set_mode(subpriv->iobase, 0, chan, data[1]);
		if (ret < 0)
			ret = -EINVAL;
		break;
	case INSN_CONFIG_8254_READ_STATUS:
		data[1] = i8254_status(subpriv->iobase, 0, chan);
		break;
	case INSN_CONFIG_SET_GATE_SRC:
		ret = dio200_set_gate_src(subpriv, chan, data[2]);
		if (ret < 0)
			ret = -EINVAL;
		break;
	case INSN_CONFIG_GET_GATE_SRC:
		ret = dio200_get_gate_src(subpriv, chan);
		if (ret < 0) {
			ret = -EINVAL;
			break;
		}
		data[2] = ret;
		break;
	case INSN_CONFIG_SET_CLOCK_SRC:
		ret = dio200_set_clock_src(subpriv, chan, data[1]);
		if (ret < 0)
			ret = -EINVAL;
		break;
	case INSN_CONFIG_GET_CLOCK_SRC:
		ret = dio200_get_clock_src(subpriv, chan, &data[2]);
		if (ret < 0) {
			ret = -EINVAL;
			break;
		}
		data[1] = ret;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	spin_unlock_irqrestore(&subpriv->spinlock, flags);
	return ret < 0 ? ret : insn->n;
}

static int
dio200_subdev_8254_init(struct comedi_device *dev, struct comedi_subdevice *s,
			unsigned long iobase, unsigned offset,
			int has_clk_gat_sce)
{
	struct dio200_subdev_8254 *subpriv;
	unsigned int chan;

	subpriv = kzalloc(sizeof(*subpriv), GFP_KERNEL);
	if (!subpriv) {
		printk(KERN_ERR "comedi%d: error! out of memory!\n",
		       dev->minor);
		return -ENOMEM;
	}

	s->private = subpriv;
	s->type = COMEDI_SUBD_COUNTER;
	s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
	s->n_chan = 3;
	s->maxdata = 0xFFFF;
	s->insn_read = dio200_subdev_8254_read;
	s->insn_write = dio200_subdev_8254_write;
	s->insn_config = dio200_subdev_8254_config;

	spin_lock_init(&subpriv->spinlock);
	subpriv->iobase = offset + iobase;
	subpriv->has_clk_gat_sce = has_clk_gat_sce;
	if (has_clk_gat_sce) {
		subpriv->clk_sce_iobase =
		    DIO200_XCLK_SCE + (offset >> 3) + iobase;
		subpriv->gat_sce_iobase =
		    DIO200_XGAT_SCE + (offset >> 3) + iobase;
		subpriv->which = (offset >> 2) & 1;
	}

	
	for (chan = 0; chan < 3; chan++) {
		i8254_set_mode(subpriv->iobase, 0, chan,
			       I8254_MODE0 | I8254_BINARY);
		if (subpriv->has_clk_gat_sce) {
			
			dio200_set_gate_src(subpriv, chan, 0);
			
			dio200_set_clock_src(subpriv, chan, 0);
		}
	}

	return 0;
}

static void
dio200_subdev_8254_cleanup(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	struct dio200_subdev_intr *subpriv = s->private;
	kfree(subpriv);
}

static int dio200_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	unsigned long iobase = 0;
	unsigned int irq = 0;
#ifdef CONFIG_COMEDI_PCI
	struct pci_dev *pci_dev = NULL;
	int bus = 0, slot = 0;
#endif
	const struct dio200_layout_struct *layout;
	int share_irq = 0;
	int sdx;
	unsigned n;
	int ret;

	printk(KERN_DEBUG "comedi%d: %s: attach\n", dev->minor,
	       DIO200_DRIVER_NAME);

	ret = alloc_private(dev, sizeof(struct dio200_private));
	if (ret < 0) {
		printk(KERN_ERR "comedi%d: error! out of memory!\n",
		       dev->minor);
		return ret;
	}

	
	switch (thisboard->bustype) {
	case isa_bustype:
		iobase = it->options[0];
		irq = it->options[1];
		share_irq = 0;
		break;
#ifdef CONFIG_COMEDI_PCI
	case pci_bustype:
		bus = it->options[0];
		slot = it->options[1];
		share_irq = 1;

		ret = dio200_find_pci(dev, bus, slot, &pci_dev);
		if (ret < 0)
			return ret;
		devpriv->pci_dev = pci_dev;
		break;
#endif
	default:
		printk(KERN_ERR
		       "comedi%d: %s: BUG! cannot determine board type!\n",
		       dev->minor, DIO200_DRIVER_NAME);
		return -EINVAL;
		break;
	}

	devpriv->intr_sd = -1;

	
#ifdef CONFIG_COMEDI_PCI
	if (pci_dev) {
		ret = comedi_pci_enable(pci_dev, DIO200_DRIVER_NAME);
		if (ret < 0) {
			printk(KERN_ERR
			       "comedi%d: error! cannot enable PCI device and request regions!\n",
			       dev->minor);
			return ret;
		}
		iobase = pci_resource_start(pci_dev, 2);
		irq = pci_dev->irq;
	} else
#endif
	{
		ret = dio200_request_region(dev->minor, iobase, DIO200_IO_SIZE);
		if (ret < 0)
			return ret;
	}
	dev->iobase = iobase;

	layout = thislayout;

	ret = alloc_subdevices(dev, layout->n_subdevs);
	if (ret < 0) {
		printk(KERN_ERR "comedi%d: error! out of memory!\n",
		       dev->minor);
		return ret;
	}

	for (n = 0; n < dev->n_subdevices; n++) {
		s = &dev->subdevices[n];
		switch (layout->sdtype[n]) {
		case sd_8254:
			
			ret = dio200_subdev_8254_init(dev, s, iobase,
						      layout->sdinfo[n],
						      layout->has_clk_gat_sce);
			if (ret < 0)
				return ret;

			break;
		case sd_8255:
			
			ret = subdev_8255_init(dev, s, NULL,
					       iobase + layout->sdinfo[n]);
			if (ret < 0)
				return ret;

			break;
		case sd_intr:
			
			if (irq) {
				ret = dio200_subdev_intr_init(dev, s,
							      iobase +
							      DIO200_INT_SCE,
							      layout->sdinfo[n],
							      layout->
							      has_int_sce);
				if (ret < 0)
					return ret;

				devpriv->intr_sd = n;
			} else {
				s->type = COMEDI_SUBD_UNUSED;
			}
			break;
		default:
			s->type = COMEDI_SUBD_UNUSED;
			break;
		}
	}

	sdx = devpriv->intr_sd;
	if (sdx >= 0 && sdx < dev->n_subdevices)
		dev->read_subdev = &dev->subdevices[sdx];

	dev->board_name = thisboard->name;

	if (irq) {
		unsigned long flags = share_irq ? IRQF_SHARED : 0;

		if (request_irq(irq, dio200_interrupt, flags,
				DIO200_DRIVER_NAME, dev) >= 0) {
			dev->irq = irq;
		} else {
			printk(KERN_WARNING
			       "comedi%d: warning! irq %u unavailable!\n",
			       dev->minor, irq);
		}
	}

	printk(KERN_INFO "comedi%d: %s ", dev->minor, dev->board_name);
	if (thisboard->bustype == isa_bustype) {
		printk("(base %#lx) ", iobase);
	} else {
#ifdef CONFIG_COMEDI_PCI
		printk("(pci %s) ", pci_name(pci_dev));
#endif
	}
	if (irq)
		printk("(irq %u%s) ", irq, (dev->irq ? "" : " UNAVAILABLE"));
	else
		printk("(no irq) ");

	printk("attached\n");

	return 1;
}

static int dio200_detach(struct comedi_device *dev)
{
	const struct dio200_layout_struct *layout;
	unsigned n;

	printk(KERN_DEBUG "comedi%d: %s: detach\n", dev->minor,
	       DIO200_DRIVER_NAME);

	if (dev->irq)
		free_irq(dev->irq, dev);
	if (dev->subdevices) {
		layout = thislayout;
		for (n = 0; n < dev->n_subdevices; n++) {
			struct comedi_subdevice *s = &dev->subdevices[n];
			switch (layout->sdtype[n]) {
			case sd_8254:
				dio200_subdev_8254_cleanup(dev, s);
				break;
			case sd_8255:
				subdev_8255_cleanup(dev, s);
				break;
			case sd_intr:
				dio200_subdev_intr_cleanup(dev, s);
				break;
			default:
				break;
			}
		}
	}
	if (devpriv) {
#ifdef CONFIG_COMEDI_PCI
		if (devpriv->pci_dev) {
			if (dev->iobase)
				comedi_pci_disable(devpriv->pci_dev);
			pci_dev_put(devpriv->pci_dev);
		} else
#endif
		{
			if (dev->iobase)
				release_region(dev->iobase, DIO200_IO_SIZE);
		}
	}
	if (dev->board_name)
		printk(KERN_INFO "comedi%d: %s removed\n",
		       dev->minor, dev->board_name);

	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
