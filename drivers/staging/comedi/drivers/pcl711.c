/*
   comedi/drivers/pcl711.c
   hardware driver for PC-LabCard PCL-711 and AdSys ACL-8112
   and compatibles

   COMEDI - Linux Control and Measurement Device Interface
   Copyright (C) 1998 David A. Schleef <ds@schleef.org>
   Janne Jalkanen <jalkanen@cs.hut.fi>
   Eric Bunn <ebu@cs.hut.fi>

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
#include "../comedidev.h"

#include <linux/ioport.h>
#include <linux/delay.h>

#include "8253.h"

#define PCL711_SIZE 16

#define PCL711_CTR0 0
#define PCL711_CTR1 1
#define PCL711_CTR2 2
#define PCL711_CTRCTL 3
#define PCL711_AD_LO 4
#define PCL711_DA0_LO 4
#define PCL711_AD_HI 5
#define PCL711_DA0_HI 5
#define PCL711_DI_LO 6
#define PCL711_DA1_LO 6
#define PCL711_DI_HI 7
#define PCL711_DA1_HI 7
#define PCL711_CLRINTR 8
#define PCL711_GAIN 9
#define PCL711_MUX 10
#define PCL711_MODE 11
#define PCL711_SOFTTRIG 12
#define PCL711_DO_LO 13
#define PCL711_DO_HI 14

static const struct comedi_lrange range_pcl711b_ai = { 5, {
							   BIP_RANGE(5),
							   BIP_RANGE(2.5),
							   BIP_RANGE(1.25),
							   BIP_RANGE(0.625),
							   BIP_RANGE(0.3125)
							   }
};

static const struct comedi_lrange range_acl8112hg_ai = { 12, {
							      BIP_RANGE(5),
							      BIP_RANGE(0.5),
							      BIP_RANGE(0.05),
							      BIP_RANGE(0.005),
							      UNI_RANGE(10),
							      UNI_RANGE(1),
							      UNI_RANGE(0.1),
							      UNI_RANGE(0.01),
							      BIP_RANGE(10),
							      BIP_RANGE(1),
							      BIP_RANGE(0.1),
							      BIP_RANGE(0.01)
							      }
};

static const struct comedi_lrange range_acl8112dg_ai = { 9, {
							     BIP_RANGE(5),
							     BIP_RANGE(2.5),
							     BIP_RANGE(1.25),
							     BIP_RANGE(0.625),
							     UNI_RANGE(10),
							     UNI_RANGE(5),
							     UNI_RANGE(2.5),
							     UNI_RANGE(1.25),
							     BIP_RANGE(10)
							     }
};


#define PCL711_TIMEOUT 100
#define PCL711_DRDY 0x10

static const int i8253_osc_base = 500;	

struct pcl711_board {

	const char *name;
	int is_pcl711b;
	int is_8112;
	int is_dg;
	int n_ranges;
	int n_aichan;
	int n_aochan;
	int maxirq;
	const struct comedi_lrange *ai_range_type;
};

static const struct pcl711_board boardtypes[] = {
	{"pcl711", 0, 0, 0, 5, 8, 1, 0, &range_bipolar5},
	{"pcl711b", 1, 0, 0, 5, 8, 1, 7, &range_pcl711b_ai},
	{"acl8112hg", 0, 1, 0, 12, 16, 2, 15, &range_acl8112hg_ai},
	{"acl8112dg", 0, 1, 1, 9, 16, 2, 15, &range_acl8112dg_ai},
};

#define n_boardtypes (sizeof(boardtypes)/sizeof(struct pcl711_board))
#define this_board ((const struct pcl711_board *)dev->board_ptr)

static int pcl711_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int pcl711_detach(struct comedi_device *dev);
static struct comedi_driver driver_pcl711 = {
	.driver_name = "pcl711",
	.module = THIS_MODULE,
	.attach = pcl711_attach,
	.detach = pcl711_detach,
	.board_name = &boardtypes[0].name,
	.num_names = n_boardtypes,
	.offset = sizeof(struct pcl711_board),
};

static int __init driver_pcl711_init_module(void)
{
	return comedi_driver_register(&driver_pcl711);
}

static void __exit driver_pcl711_cleanup_module(void)
{
	comedi_driver_unregister(&driver_pcl711);
}

module_init(driver_pcl711_init_module);
module_exit(driver_pcl711_cleanup_module);

struct pcl711_private {

	int board;
	int adchan;
	int ntrig;
	int aip[8];
	int mode;
	unsigned int ao_readback[2];
	unsigned int divisor1;
	unsigned int divisor2;
};

#define devpriv ((struct pcl711_private *)dev->private)

static irqreturn_t pcl711_interrupt(int irq, void *d)
{
	int lo, hi;
	int data;
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->subdevices + 0;

	if (!dev->attached) {
		comedi_error(dev, "spurious interrupt");
		return IRQ_HANDLED;
	}

	hi = inb(dev->iobase + PCL711_AD_HI);
	lo = inb(dev->iobase + PCL711_AD_LO);
	outb(0, dev->iobase + PCL711_CLRINTR);

	data = (hi << 8) | lo;

	
	if (!(--devpriv->ntrig)) {
		if (this_board->is_8112)
			outb(1, dev->iobase + PCL711_MODE);
		else
			outb(0, dev->iobase + PCL711_MODE);

		s->async->events |= COMEDI_CB_EOA;
	}
	comedi_event(dev, s);
	return IRQ_HANDLED;
}

static void pcl711_set_changain(struct comedi_device *dev, int chan)
{
	int chan_register;

	outb(CR_RANGE(chan), dev->iobase + PCL711_GAIN);

	chan_register = CR_CHAN(chan);

	if (this_board->is_8112) {

		/*
		 *  Set the correct channel.  The two channel banks are switched
		 *  using the mask value.
		 *  NB: To use differential channels, you should use
		 *  mask = 0x30, but I haven't written the support for this
		 *  yet. /JJ
		 */

		if (chan_register >= 8)
			chan_register = 0x20 | (chan_register & 0x7);
		else
			chan_register |= 0x10;
	} else {
		outb(chan_register, dev->iobase + PCL711_MUX);
	}
}

static int pcl711_ai_insn(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data)
{
	int i, n;
	int hi, lo;

	pcl711_set_changain(dev, insn->chanspec);

	for (n = 0; n < insn->n; n++) {
		outb(1, dev->iobase + PCL711_MODE);

		if (!this_board->is_8112)
			outb(0, dev->iobase + PCL711_SOFTTRIG);

		i = PCL711_TIMEOUT;
		while (--i) {
			hi = inb(dev->iobase + PCL711_AD_HI);
			if (!(hi & PCL711_DRDY))
				goto ok;
			udelay(1);
		}
		printk(KERN_ERR "comedi%d: pcl711: A/D timeout\n", dev->minor);
		return -ETIME;

ok:
		lo = inb(dev->iobase + PCL711_AD_LO);

		data[n] = ((hi & 0xf) << 8) | lo;
	}

	return n;
}

static int pcl711_ai_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int tmp;
	int err = 0;

	
	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER | TRIG_EXT;
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
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	

	if (cmd->scan_begin_src != TRIG_TIMER &&
	    cmd->scan_begin_src != TRIG_EXT)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}
	if (cmd->scan_begin_src == TRIG_EXT) {
		if (cmd->scan_begin_arg != 0) {
			cmd->scan_begin_arg = 0;
			err++;
		}
	} else {
#define MAX_SPEED 1000
#define TIMER_BASE 100
		if (cmd->scan_begin_arg < MAX_SPEED) {
			cmd->scan_begin_arg = MAX_SPEED;
			err++;
		}
	}
	if (cmd->convert_arg != 0) {
		cmd->convert_arg = 0;
		err++;
	}
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
	if (cmd->stop_src == TRIG_NONE) {
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	} else {
		
	}

	if (err)
		return 3;

	

	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		i8253_cascade_ns_to_timer_2div(TIMER_BASE,
					       &devpriv->divisor1,
					       &devpriv->divisor2,
					       &cmd->scan_begin_arg,
					       cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->scan_begin_arg)
			err++;
	}

	if (err)
		return 4;

	return 0;
}

static int pcl711_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	int timer1, timer2;
	struct comedi_cmd *cmd = &s->async->cmd;

	pcl711_set_changain(dev, cmd->chanlist[0]);

	if (cmd->scan_begin_src == TRIG_TIMER) {

		timer1 = timer2 = 0;
		i8253_cascade_ns_to_timer(i8253_osc_base, &timer1, &timer2,
					  &cmd->scan_begin_arg,
					  TRIG_ROUND_NEAREST);

		outb(0x74, dev->iobase + PCL711_CTRCTL);
		outb(timer1 & 0xff, dev->iobase + PCL711_CTR1);
		outb((timer1 >> 8) & 0xff, dev->iobase + PCL711_CTR1);
		outb(0xb4, dev->iobase + PCL711_CTRCTL);
		outb(timer2 & 0xff, dev->iobase + PCL711_CTR2);
		outb((timer2 >> 8) & 0xff, dev->iobase + PCL711_CTR2);

		
		outb(0, dev->iobase + PCL711_CLRINTR);

		outb(devpriv->mode | 6, dev->iobase + PCL711_MODE);
	} else {
		
		outb(devpriv->mode | 3, dev->iobase + PCL711_MODE);
	}

	return 0;
}

static int pcl711_ao_insn(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data)
{
	int n;
	int chan = CR_CHAN(insn->chanspec);

	for (n = 0; n < insn->n; n++) {
		outb((data[n] & 0xff),
		     dev->iobase + (chan ? PCL711_DA1_LO : PCL711_DA0_LO));
		outb((data[n] >> 8),
		     dev->iobase + (chan ? PCL711_DA1_HI : PCL711_DA0_HI));

		devpriv->ao_readback[chan] = data[n];
	}

	return n;
}

static int pcl711_ao_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int n;
	int chan = CR_CHAN(insn->chanspec);

	for (n = 0; n < insn->n; n++)
		data[n] = devpriv->ao_readback[chan];

	return n;

}

static int pcl711_di_insn_bits(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	if (insn->n != 2)
		return -EINVAL;

	data[1] = inb(dev->iobase + PCL711_DI_LO) |
	    (inb(dev->iobase + PCL711_DI_HI) << 8);

	return 2;
}

static int pcl711_do_insn_bits(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	if (insn->n != 2)
		return -EINVAL;

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= data[0] & data[1];
	}
	if (data[0] & 0x00ff)
		outb(s->state & 0xff, dev->iobase + PCL711_DO_LO);
	if (data[0] & 0xff00)
		outb((s->state >> 8), dev->iobase + PCL711_DO_HI);

	data[1] = s->state;

	return 2;
}

static int pcl711_detach(struct comedi_device *dev)
{
	printk(KERN_INFO "comedi%d: pcl711: remove\n", dev->minor);

	if (dev->irq)
		free_irq(dev->irq, dev);

	if (dev->iobase)
		release_region(dev->iobase, PCL711_SIZE);

	return 0;
}

static int pcl711_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	int ret;
	unsigned long iobase;
	unsigned int irq;
	struct comedi_subdevice *s;

	

	iobase = it->options[0];
	printk(KERN_INFO "comedi%d: pcl711: 0x%04lx ", dev->minor, iobase);
	if (!request_region(iobase, PCL711_SIZE, "pcl711")) {
		printk("I/O port conflict\n");
		return -EIO;
	}
	dev->iobase = iobase;

	

	
	dev->board_name = this_board->name;

	
	irq = it->options[1];
	if (irq > this_board->maxirq) {
		printk(KERN_ERR "irq out of range\n");
		return -EINVAL;
	}
	if (irq) {
		if (request_irq(irq, pcl711_interrupt, 0, "pcl711", dev)) {
			printk(KERN_ERR "unable to allocate irq %u\n", irq);
			return -EINVAL;
		} else {
			printk(KERN_INFO "( irq = %u )\n", irq);
		}
	}
	dev->irq = irq;

	ret = alloc_subdevices(dev, 4);
	if (ret < 0)
		return ret;

	ret = alloc_private(dev, sizeof(struct pcl711_private));
	if (ret < 0)
		return ret;

	s = dev->subdevices + 0;
	
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND;
	s->n_chan = this_board->n_aichan;
	s->maxdata = 0xfff;
	s->len_chanlist = 1;
	s->range_table = this_board->ai_range_type;
	s->insn_read = pcl711_ai_insn;
	if (irq) {
		dev->read_subdev = s;
		s->subdev_flags |= SDF_CMD_READ;
		s->do_cmdtest = pcl711_ai_cmdtest;
		s->do_cmd = pcl711_ai_cmd;
	}

	s++;
	
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE;
	s->n_chan = this_board->n_aochan;
	s->maxdata = 0xfff;
	s->len_chanlist = 1;
	s->range_table = &range_bipolar5;
	s->insn_write = pcl711_ao_insn;
	s->insn_read = pcl711_ao_insn_read;

	s++;
	
	s->type = COMEDI_SUBD_DI;
	s->subdev_flags = SDF_READABLE;
	s->n_chan = 16;
	s->maxdata = 1;
	s->len_chanlist = 16;
	s->range_table = &range_digital;
	s->insn_bits = pcl711_di_insn_bits;

	s++;
	
	s->type = COMEDI_SUBD_DO;
	s->subdev_flags = SDF_WRITABLE;
	s->n_chan = 16;
	s->maxdata = 1;
	s->len_chanlist = 16;
	s->range_table = &range_digital;
	s->state = 0;
	s->insn_bits = pcl711_do_insn_bits;

	if (this_board->is_pcl711b)
		devpriv->mode = (dev->irq << 4);

	
	outb(0, dev->iobase + PCL711_DA0_LO);
	outb(0, dev->iobase + PCL711_DA0_HI);
	outb(0, dev->iobase + PCL711_DA1_LO);
	outb(0, dev->iobase + PCL711_DA1_HI);

	printk(KERN_INFO "\n");

	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
