
/*
   comedi/drivers/das6402.c
   An experimental driver for Computerboards' DAS6402 I/O card

   Copyright (C) 1999 Oystein Svendsen <svendsen@pvv.org>

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

#define DAS6402_SIZE 16

#define N_WORDS (3000*64)

#define STOP    0
#define START   1

#define SCANL 0x3f00
#define BYTE unsigned char
#define WORD unsigned short

#define CLRINT 0x01
#define CLRXTR 0x02
#define CLRXIN 0x04
#define EXTEND 0x10
#define ARMED 0x20		
#define POSTMODE 0x40
#define MHZ 0x80		

#define IRQ (0x04 << 4)		
#define IRQV 10			

#define CONVSRC 0x03		
#define BURSTEN 0x04		
#define XINTE 0x08		
#define INTE 0x80		

#define TGEN 0x01		
#define TGSEL 0x02		
#define TGPOL 0x04		
#define PRETRIG 0x08		

#define EOB 0x0c
#define FIFOHFULL 0x08
#define GAIN 0x01
#define FIFONEPTY 0x04
#define MODE 0x10
#define SEM 0x20
#define BIP 0x40

#define M0 0x00
#define M2 0x04

#define	C0 0x00
#define	C1 0x40
#define	C2 0x80
#define	RWLH 0x30

static int das6402_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it);
static int das6402_detach(struct comedi_device *dev);
static struct comedi_driver driver_das6402 = {
	.driver_name = "das6402",
	.module = THIS_MODULE,
	.attach = das6402_attach,
	.detach = das6402_detach,
};

static int __init driver_das6402_init_module(void)
{
	return comedi_driver_register(&driver_das6402);
}

static void __exit driver_das6402_cleanup_module(void)
{
	comedi_driver_unregister(&driver_das6402);
}

module_init(driver_das6402_init_module);
module_exit(driver_das6402_cleanup_module);

struct das6402_private {
	int ai_bytes_to_read;

	int das6402_ignoreirq;
};
#define devpriv ((struct das6402_private *)dev->private)

static void das6402_ai_fifo_dregs(struct comedi_device *dev,
				  struct comedi_subdevice *s);

static void das6402_setcounter(struct comedi_device *dev)
{
	BYTE p;
	unsigned short ctrlwrd;

	
	p = M0 | C0 | RWLH;
	outb_p(p, dev->iobase + 15);
	ctrlwrd = 2000;
	p = (BYTE) (0xff & ctrlwrd);
	outb_p(p, dev->iobase + 12);
	p = (BYTE) (0xff & (ctrlwrd >> 8));
	outb_p(p, dev->iobase + 12);

	
	p = M2 | C1 | RWLH;
	outb_p(p, dev->iobase + 15);
	ctrlwrd = 10;
	p = (BYTE) (0xff & ctrlwrd);
	outb_p(p, dev->iobase + 13);
	p = (BYTE) (0xff & (ctrlwrd >> 8));
	outb_p(p, dev->iobase + 13);

	
	p = M2 | C2 | RWLH;
	outb_p(p, dev->iobase + 15);
	ctrlwrd = 1000;
	p = (BYTE) (0xff & ctrlwrd);
	outb_p(p, dev->iobase + 14);
	p = (BYTE) (0xff & (ctrlwrd >> 8));
	outb_p(p, dev->iobase + 14);
}

static irqreturn_t intr_handler(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->subdevices;

	if (!dev->attached || devpriv->das6402_ignoreirq) {
		dev_warn(dev->hw_dev, "BUG: spurious interrupt\n");
		return IRQ_HANDLED;
	}
#ifdef DEBUG
	printk("das6402: interrupt! das6402_irqcount=%i\n",
	       devpriv->das6402_irqcount);
	printk("das6402: iobase+2=%i\n", inw_p(dev->iobase + 2));
#endif

	das6402_ai_fifo_dregs(dev, s);

	if (s->async->buf_write_count >= devpriv->ai_bytes_to_read) {
		outw_p(SCANL, dev->iobase + 2);	
		outb(0x07, dev->iobase + 8);	
#ifdef DEBUG
		printk("das6402: Got %i samples\n\n",
		       devpriv->das6402_wordsread - diff);
#endif
		s->async->events |= COMEDI_CB_EOA;
		comedi_event(dev, s);
	}

	outb(0x01, dev->iobase + 8);	

	comedi_event(dev, s);
	return IRQ_HANDLED;
}

#if 0
static void das6402_ai_fifo_read(struct comedi_device *dev, short *data, int n)
{
	int i;

	for (i = 0; i < n; i++)
		data[i] = inw(dev->iobase);
}
#endif

static void das6402_ai_fifo_dregs(struct comedi_device *dev,
				  struct comedi_subdevice *s)
{
	while (1) {
		if (!(inb(dev->iobase + 8) & 0x01))
			return;
		comedi_buf_put(s->async, inw(dev->iobase));
	}
}

static int das6402_ai_cancel(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{

	devpriv->das6402_ignoreirq = 1;
	dev_dbg(dev->hw_dev, "Stopping acquisition\n");
	devpriv->das6402_ignoreirq = 1;
	outb_p(0x02, dev->iobase + 10);	
	outw_p(SCANL, dev->iobase + 2);	
	outb_p(0, dev->iobase + 9);	

	outw_p(SCANL, dev->iobase + 2);

	return 0;
}

#ifdef unused
static int das6402_ai_mode2(struct comedi_device *dev,
			    struct comedi_subdevice *s, comedi_trig * it)
{
	devpriv->das6402_ignoreirq = 1;
	dev_dbg(dev->hw_dev, "Starting acquisition\n");
	outb_p(0x03, dev->iobase + 10);	
	outw_p(SCANL, dev->iobase + 2);	
	outb_p(IRQ | CONVSRC | BURSTEN | INTE, dev->iobase + 9);

	devpriv->ai_bytes_to_read = it->n * sizeof(short);

	
	devpriv->das6402_ignoreirq = 0;

	outw_p(SCANL, dev->iobase + 2);

	return 0;
}
#endif

static int board_init(struct comedi_device *dev)
{
	BYTE b;

	devpriv->das6402_ignoreirq = 1;

	outb(0x07, dev->iobase + 8);

	
	outb_p(MODE, dev->iobase + 11);
	b = BIP | SEM | MODE | GAIN | FIFOHFULL;
	outb_p(b, dev->iobase + 11);

	
	outb_p(EXTEND, dev->iobase + 8);
	b = EXTEND | MHZ;
	outb_p(b, dev->iobase + 8);
	b = MHZ | CLRINT | CLRXTR | CLRXIN;
	outb_p(b, dev->iobase + 8);

	
	b = IRQ | CONVSRC | BURSTEN | INTE;
	outb_p(b, dev->iobase + 9);

	
	b = TGSEL | TGEN;
	outb_p(b, dev->iobase + 10);

	b = 0x07;
	outb_p(b, dev->iobase + 8);

	das6402_setcounter(dev);

	outw_p(SCANL, dev->iobase + 2);	

	devpriv->das6402_ignoreirq = 0;

	return 0;
}

static int das6402_detach(struct comedi_device *dev)
{
	if (dev->irq)
		free_irq(dev->irq, dev);
	if (dev->iobase)
		release_region(dev->iobase, DAS6402_SIZE);

	return 0;
}

static int das6402_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it)
{
	unsigned int irq;
	unsigned long iobase;
	int ret;
	struct comedi_subdevice *s;

	dev->board_name = "das6402";

	iobase = it->options[0];
	if (iobase == 0)
		iobase = 0x300;

	if (!request_region(iobase, DAS6402_SIZE, "das6402")) {
		dev_err(dev->hw_dev, "I/O port conflict\n");
		return -EIO;
	}
	dev->iobase = iobase;

	

	irq = it->options[0];
	dev_dbg(dev->hw_dev, "( irq = %u )\n", irq);
	ret = request_irq(irq, intr_handler, 0, "das6402", dev);
	if (ret < 0)
		return ret;

	dev->irq = irq;
	ret = alloc_private(dev, sizeof(struct das6402_private));
	if (ret < 0)
		return ret;

	ret = alloc_subdevices(dev, 1);
	if (ret < 0)
		return ret;

	
	s = dev->subdevices + 0;
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND;
	s->n_chan = 8;
	
	s->cancel = das6402_ai_cancel;
	s->maxdata = (1 << 12) - 1;
	s->len_chanlist = 16;	
	s->range_table = &range_unknown;

	board_init(dev);

	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
