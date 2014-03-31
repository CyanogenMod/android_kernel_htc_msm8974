/*
    comedi/drivers/pcmmio.c
    Driver for Winsystems PC-104 based multifunction IO board.

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 2007 Calin A. Culianu <calin@ajvar.org>

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
#include "pcm_common.h"
#include <linux/pci.h>		

#define CHANS_PER_PORT   8
#define PORTS_PER_ASIC   6
#define INTR_PORTS_PER_ASIC   3
#define MAX_CHANS_PER_SUBDEV 24	
#define PORTS_PER_SUBDEV (MAX_CHANS_PER_SUBDEV/CHANS_PER_PORT)
#define CHANS_PER_ASIC (CHANS_PER_PORT*PORTS_PER_ASIC)
#define INTR_CHANS_PER_ASIC 24
#define INTR_PORTS_PER_SUBDEV (INTR_CHANS_PER_ASIC/CHANS_PER_PORT)
#define MAX_DIO_CHANS   (PORTS_PER_ASIC*1*CHANS_PER_PORT)
#define MAX_ASICS       (MAX_DIO_CHANS/CHANS_PER_ASIC)
#define SDEV_NO ((int)(s - dev->subdevices))
#define CALC_N_DIO_SUBDEVS(nchans) ((nchans)/MAX_CHANS_PER_SUBDEV + (!!((nchans)%MAX_CHANS_PER_SUBDEV)) )
#define ASIC_IOSIZE (0x0B)
#define PCMMIO48_IOSIZE ASIC_IOSIZE

#define REG_PORT0 0x0
#define REG_PORT1 0x1
#define REG_PORT2 0x2
#define REG_PORT3 0x3
#define REG_PORT4 0x4
#define REG_PORT5 0x5
#define REG_INT_PENDING 0x6
#define REG_PAGELOCK 0x7	
#define REG_POL0 0x8
#define REG_POL1 0x9
#define REG_POL2 0xA
#define REG_ENAB0 0x8
#define REG_ENAB1 0x9
#define REG_ENAB2 0xA
#define REG_INT_ID0 0x8
#define REG_INT_ID1 0x9
#define REG_INT_ID2 0xA

#define NUM_PAGED_REGS 3
#define NUM_PAGES 4
#define FIRST_PAGED_REG 0x8
#define REG_PAGE_BITOFFSET 6
#define REG_LOCK_BITOFFSET 0
#define REG_PAGE_MASK (~((0x1<<REG_PAGE_BITOFFSET)-1))
#define REG_LOCK_MASK (~(REG_PAGE_MASK))
#define PAGE_POL 1
#define PAGE_ENAB 2
#define PAGE_INT_ID 3

static int ai_rinsn(struct comedi_device *, struct comedi_subdevice *,
		    struct comedi_insn *, unsigned int *);
static int ao_rinsn(struct comedi_device *, struct comedi_subdevice *,
		    struct comedi_insn *, unsigned int *);
static int ao_winsn(struct comedi_device *, struct comedi_subdevice *,
		    struct comedi_insn *, unsigned int *);

struct pcmmio_board {
	const char *name;
	const int dio_num_asics;
	const int dio_num_ports;
	const int total_iosize;
	const int ai_bits;
	const int ao_bits;
	const int n_ai_chans;
	const int n_ao_chans;
	const struct comedi_lrange *ai_range_table, *ao_range_table;
	int (*ai_rinsn) (struct comedi_device *dev,
			struct comedi_subdevice *s,
			struct comedi_insn *insn,
			unsigned int *data);
	int (*ao_rinsn) (struct comedi_device *dev,
			struct comedi_subdevice *s,
			struct comedi_insn *insn,
			unsigned int *data);
	int (*ao_winsn) (struct comedi_device *dev,
			struct comedi_subdevice *s,
			struct comedi_insn *insn,
			unsigned int *data);
};

static const struct comedi_lrange ranges_ai = {
	4, {RANGE(-5., 5.), RANGE(-10., 10.), RANGE(0., 5.), RANGE(0., 10.)}
};

static const struct comedi_lrange ranges_ao = {
	6, {RANGE(0., 5.), RANGE(0., 10.), RANGE(-5., 5.), RANGE(-10., 10.),
	  RANGE(-2.5, 2.5), RANGE(-2.5, 7.5)}
};

static const struct pcmmio_board pcmmio_boards[] = {
	{
	 .name = "pcmmio",
	 .dio_num_asics = 1,
	 .dio_num_ports = 6,
	 .total_iosize = 32,
	 .ai_bits = 16,
	 .ao_bits = 16,
	 .n_ai_chans = 16,
	 .n_ao_chans = 8,
	 .ai_range_table = &ranges_ai,
	 .ao_range_table = &ranges_ao,
	 .ai_rinsn = ai_rinsn,
	 .ao_rinsn = ao_rinsn,
	 .ao_winsn = ao_winsn},
};

#define thisboard ((const struct pcmmio_board *)dev->board_ptr)

struct pcmmio_subdev_private {

	union {
		unsigned long iobases[PORTS_PER_SUBDEV];

		
		unsigned long iobase;
	};
	union {
		struct {

			
			struct {
				int asic;
				int first_chan;
				int num_asic_chans;
				int asic_chan;
				int enabled_mask;
				int active;
				int stop_count;
				int continuous;
				spinlock_t spinlock;
			} intr;
		} dio;
		struct {
			/* the last unsigned int data written */
			unsigned int shadow_samples[8];
		} ao;
	};
};

struct pcmmio_private {
	
	struct {
		unsigned char pagelock;	
		
		unsigned char pol[NUM_PAGED_REGS];
		
		unsigned char enab[NUM_PAGED_REGS];
		int num;
		unsigned long iobase;
		unsigned int irq;
		spinlock_t spinlock;
	} asics[MAX_ASICS];
	struct pcmmio_subdev_private *sprivs;
};

#define devpriv ((struct pcmmio_private *)dev->private)
#define subpriv ((struct pcmmio_subdev_private *)s->private)
static int pcmmio_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int pcmmio_detach(struct comedi_device *dev);

static struct comedi_driver driver = {
	.driver_name = "pcmmio",
	.module = THIS_MODULE,
	.attach = pcmmio_attach,
	.detach = pcmmio_detach,
	.board_name = &pcmmio_boards[0].name,
	.offset = sizeof(struct pcmmio_board),
	.num_names = ARRAY_SIZE(pcmmio_boards),
};

static int pcmmio_dio_insn_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data);
static int pcmmio_dio_insn_config(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data);

static irqreturn_t interrupt_pcmmio(int irq, void *d);
static void pcmmio_stop_intr(struct comedi_device *, struct comedi_subdevice *);
static int pcmmio_cancel(struct comedi_device *dev, struct comedi_subdevice *s);
static int pcmmio_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int pcmmio_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_cmd *cmd);

static void init_asics(struct comedi_device *dev);
static void switch_page(struct comedi_device *dev, int asic, int page);
#ifdef notused
static void lock_port(struct comedi_device *dev, int asic, int port);
static void unlock_port(struct comedi_device *dev, int asic, int port);
#endif

static int pcmmio_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int sdev_no, chans_left, n_dio_subdevs, n_subdevs, port, asic,
	    thisasic_chanct = 0;
	unsigned long iobase;
	unsigned int irq[MAX_ASICS];

	iobase = it->options[0];
	irq[0] = it->options[1];

	printk(KERN_INFO "comedi%d: %s: io: %lx attaching...\n", dev->minor,
			driver.driver_name, iobase);

	dev->iobase = iobase;

	if (!iobase || !request_region(iobase,
				       thisboard->total_iosize,
				       driver.driver_name)) {
		printk(KERN_ERR "comedi%d: I/O port conflict\n", dev->minor);
		return -EIO;
	}

	dev->board_name = thisboard->name;

	if (alloc_private(dev, sizeof(struct pcmmio_private)) < 0) {
		printk(KERN_ERR "comedi%d: cannot allocate private data structure\n",
				dev->minor);
		return -ENOMEM;
	}

	for (asic = 0; asic < MAX_ASICS; ++asic) {
		devpriv->asics[asic].num = asic;
		devpriv->asics[asic].iobase =
		    dev->iobase + 16 + asic * ASIC_IOSIZE;
		devpriv->asics[asic].irq = 0;
		spin_lock_init(&devpriv->asics[asic].spinlock);
	}

	chans_left = CHANS_PER_ASIC * thisboard->dio_num_asics;
	n_dio_subdevs = CALC_N_DIO_SUBDEVS(chans_left);
	n_subdevs = n_dio_subdevs + 2;
	devpriv->sprivs =
	    kcalloc(n_subdevs, sizeof(struct pcmmio_subdev_private),
		    GFP_KERNEL);
	if (!devpriv->sprivs) {
		printk(KERN_ERR "comedi%d: cannot allocate subdevice private data structures\n",
				dev->minor);
		return -ENOMEM;
	}
	if (alloc_subdevices(dev, n_subdevs) < 0) {
		printk(KERN_ERR "comedi%d: cannot allocate subdevice data structures\n",
				dev->minor);
		return -ENOMEM;
	}

	
	sdev_no = 0;
	s = dev->subdevices + sdev_no;
	s->private = devpriv->sprivs + sdev_no;
	s->maxdata = (1 << thisboard->ai_bits) - 1;
	s->range_table = thisboard->ai_range_table;
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_DIFF;
	s->type = COMEDI_SUBD_AI;
	s->n_chan = thisboard->n_ai_chans;
	s->len_chanlist = s->n_chan;
	s->insn_read = thisboard->ai_rinsn;
	subpriv->iobase = dev->iobase + 0;
	
	outb(0, subpriv->iobase + 3);
	outb(0, subpriv->iobase + 4 + 3);

	
	++sdev_no;
	s = dev->subdevices + sdev_no;
	s->private = devpriv->sprivs + sdev_no;
	s->maxdata = (1 << thisboard->ao_bits) - 1;
	s->range_table = thisboard->ao_range_table;
	s->subdev_flags = SDF_READABLE;
	s->type = COMEDI_SUBD_AO;
	s->n_chan = thisboard->n_ao_chans;
	s->len_chanlist = s->n_chan;
	s->insn_read = thisboard->ao_rinsn;
	s->insn_write = thisboard->ao_winsn;
	subpriv->iobase = dev->iobase + 8;
	
	outb(0, subpriv->iobase + 3);
	outb(0, subpriv->iobase + 4 + 3);

	++sdev_no;
	port = 0;
	asic = 0;
	for (; sdev_no < (int)dev->n_subdevices; ++sdev_no) {
		int byte_no;

		s = dev->subdevices + sdev_no;
		s->private = devpriv->sprivs + sdev_no;
		s->maxdata = 1;
		s->range_table = &range_digital;
		s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
		s->type = COMEDI_SUBD_DIO;
		s->insn_bits = pcmmio_dio_insn_bits;
		s->insn_config = pcmmio_dio_insn_config;
		s->n_chan = min(chans_left, MAX_CHANS_PER_SUBDEV);
		subpriv->dio.intr.asic = -1;
		subpriv->dio.intr.first_chan = -1;
		subpriv->dio.intr.asic_chan = -1;
		subpriv->dio.intr.num_asic_chans = -1;
		subpriv->dio.intr.active = 0;
		s->len_chanlist = 1;

		for (byte_no = 0; byte_no < PORTS_PER_SUBDEV; ++byte_no, ++port) {
			if (port >= PORTS_PER_ASIC) {
				port = 0;
				++asic;
				thisasic_chanct = 0;
			}
			subpriv->iobases[byte_no] =
			    devpriv->asics[asic].iobase + port;

			if (thisasic_chanct <
			    CHANS_PER_PORT * INTR_PORTS_PER_ASIC
			    && subpriv->dio.intr.asic < 0) {
				subpriv->dio.intr.asic = asic;
				subpriv->dio.intr.active = 0;
				subpriv->dio.intr.stop_count = 0;
				subpriv->dio.intr.first_chan = byte_no * 8;
				subpriv->dio.intr.asic_chan = thisasic_chanct;
				subpriv->dio.intr.num_asic_chans =
				    s->n_chan - subpriv->dio.intr.first_chan;
				s->cancel = pcmmio_cancel;
				s->do_cmd = pcmmio_cmd;
				s->do_cmdtest = pcmmio_cmdtest;
				s->len_chanlist =
				    subpriv->dio.intr.num_asic_chans;
			}
			thisasic_chanct += CHANS_PER_PORT;
		}
		spin_lock_init(&subpriv->dio.intr.spinlock);

		chans_left -= s->n_chan;

		if (!chans_left) {
			asic = 0;
			port = 0;
		}

	}

	init_asics(dev);	

	for (asic = 0; irq[0] && asic < MAX_ASICS; ++asic) {
		if (irq[asic]
		    && request_irq(irq[asic], interrupt_pcmmio,
				   IRQF_SHARED, thisboard->name, dev)) {
			int i;
			
			for (i = asic - 1; i >= 0; --i) {
				free_irq(irq[i], dev);
				devpriv->asics[i].irq = irq[i] = 0;
			}
			irq[asic] = 0;
		}
		devpriv->asics[asic].irq = irq[asic];
	}

	dev->irq = irq[0];	

	if (irq[0]) {
		printk(KERN_DEBUG "comedi%d: irq: %u\n", dev->minor, irq[0]);
		if (thisboard->dio_num_asics == 2 && irq[1])
			printk(KERN_DEBUG "comedi%d: second ASIC irq: %u\n",
					dev->minor, irq[1]);
	} else {
		printk(KERN_INFO "comedi%d: (IRQ mode disabled)\n", dev->minor);
	}

	printk(KERN_INFO "comedi%d: attached\n", dev->minor);

	return 1;
}

static int pcmmio_detach(struct comedi_device *dev)
{
	int i;

	printk(KERN_INFO "comedi%d: %s: remove\n", dev->minor, driver.driver_name);
	if (dev->iobase)
		release_region(dev->iobase, thisboard->total_iosize);

	for (i = 0; i < MAX_ASICS; ++i) {
		if (devpriv && devpriv->asics[i].irq)
			free_irq(devpriv->asics[i].irq, dev);
	}

	if (devpriv && devpriv->sprivs)
		kfree(devpriv->sprivs);

	return 0;
}

static int pcmmio_dio_insn_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int byte_no;
	if (insn->n != 2)
		return -EINVAL;



#ifdef DAMMIT_ITS_BROKEN
	
	printk(KERN_DEBUG "write mask: %08x  data: %08x\n", data[0], data[1]);
#endif

	s->state = 0;

	for (byte_no = 0; byte_no < s->n_chan / CHANS_PER_PORT; ++byte_no) {
		
		unsigned long ioaddr = subpriv->iobases[byte_no],
		    
		    offset = byte_no * 8;
		
		unsigned char byte = 0,
		    
		    write_mask_byte = (data[0] >> offset) & 0xff,
		    
		    data_byte = (data[1] >> offset) & 0xff;

		byte = inb(ioaddr);	

#ifdef DAMMIT_ITS_BROKEN
		
		printk
		    (KERN_DEBUG "byte %d wmb %02x db %02x offset %02d io %04x,"
		     " data_in %02x ", byte_no, (unsigned)write_mask_byte,
		     (unsigned)data_byte, offset, ioaddr, (unsigned)byte);
#endif

		if (write_mask_byte) {
			
			byte &= ~write_mask_byte;
			
			byte |= ~data_byte & write_mask_byte;
			
			outb(byte, ioaddr);
		}
#ifdef DAMMIT_ITS_BROKEN
		
		printk("data_out_byte %02x\n", (unsigned)byte);
#endif
		
		s->state |= ((unsigned int)byte) << offset;
	}

	
	data[1] = ~s->state;

#ifdef DAMMIT_ITS_BROKEN
	
	printk(KERN_DEBUG "s->state %08x data_out %08x\n", s->state, data[1]);
#endif

	return 2;
}

static int pcmmio_dio_insn_config(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	int chan = CR_CHAN(insn->chanspec), byte_no = chan / 8, bit_no =
	    chan % 8;
	unsigned long ioaddr;
	unsigned char byte;

	
	ioaddr = subpriv->iobases[byte_no];


	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
		s->io_bits |= 1 << chan;
		break;
	case INSN_CONFIG_DIO_INPUT:
		byte = inb(ioaddr);
		byte &= ~(1 << bit_no);
				

		outb(byte, ioaddr);

		
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

static void init_asics(struct comedi_device *dev)
{				
	int asic;

	for (asic = 0; asic < thisboard->dio_num_asics; ++asic) {
		int port, page;
		unsigned long baseaddr = devpriv->asics[asic].iobase;

		switch_page(dev, asic, 0);	

		
		for (port = 0; port < PORTS_PER_ASIC; ++port)
			outb(0, baseaddr + REG_PORT0 + port);

		
		for (page = 1; page < NUM_PAGES; ++page) {
			int reg;
			
			switch_page(dev, asic, page);
			for (reg = FIRST_PAGED_REG;
			     reg < FIRST_PAGED_REG + NUM_PAGED_REGS; ++reg)
				outb(0, baseaddr + reg);
		}

		
		

		
		switch_page(dev, asic, 0);
	}
}

static void switch_page(struct comedi_device *dev, int asic, int page)
{
	if (asic < 0 || asic >= thisboard->dio_num_asics)
		return;		
	if (page < 0 || page >= NUM_PAGES)
		return;		

	devpriv->asics[asic].pagelock &= ~REG_PAGE_MASK;
	devpriv->asics[asic].pagelock |= page << REG_PAGE_BITOFFSET;

	
	outb(devpriv->asics[asic].pagelock,
	     devpriv->asics[asic].iobase + REG_PAGELOCK);
}

#ifdef notused
static void lock_port(struct comedi_device *dev, int asic, int port)
{
	if (asic < 0 || asic >= thisboard->dio_num_asics)
		return;		
	if (port < 0 || port >= PORTS_PER_ASIC)
		return;		

	devpriv->asics[asic].pagelock |= 0x1 << port;
	
	outb(devpriv->asics[asic].pagelock,
	     devpriv->asics[asic].iobase + REG_PAGELOCK);
	return;
}

static void unlock_port(struct comedi_device *dev, int asic, int port)
{
	if (asic < 0 || asic >= thisboard->dio_num_asics)
		return;		
	if (port < 0 || port >= PORTS_PER_ASIC)
		return;		
	devpriv->asics[asic].pagelock &= ~(0x1 << port) | REG_LOCK_MASK;
	
	outb(devpriv->asics[asic].pagelock,
	     devpriv->asics[asic].iobase + REG_PAGELOCK);
}
#endif 

static irqreturn_t interrupt_pcmmio(int irq, void *d)
{
	int asic, got1 = 0;
	struct comedi_device *dev = (struct comedi_device *)d;

	for (asic = 0; asic < MAX_ASICS; ++asic) {
		if (irq == devpriv->asics[asic].irq) {
			unsigned long flags;
			unsigned triggered = 0;
			unsigned long iobase = devpriv->asics[asic].iobase;
			
			unsigned char int_pend;

			spin_lock_irqsave(&devpriv->asics[asic].spinlock,
					  flags);

			int_pend = inb(iobase + REG_INT_PENDING) & 0x07;

			if (int_pend) {
				int port;
				for (port = 0; port < INTR_PORTS_PER_ASIC;
				     ++port) {
					if (int_pend & (0x1 << port)) {
						unsigned char
						    io_lines_with_edges = 0;
						switch_page(dev, asic,
							    PAGE_INT_ID);
						io_lines_with_edges =
						    inb(iobase +
							REG_INT_ID0 + port);

						if (io_lines_with_edges)
							outb(0, iobase +
							     REG_INT_ID0 +
							     port);

						triggered |=
						    io_lines_with_edges <<
						    port * 8;
					}
				}

				++got1;
			}

			spin_unlock_irqrestore(&devpriv->asics[asic].spinlock,
					       flags);

			if (triggered) {
				struct comedi_subdevice *s;
				printk
				    (KERN_DEBUG "got edge detect interrupt %d asic %d which_chans: %06x\n",
				     irq, asic, triggered);
				for (s = dev->subdevices + 2;
				     s < dev->subdevices + dev->n_subdevices;
				     ++s) {
					if (subpriv->dio.intr.asic == asic) {
						unsigned long flags;
						unsigned oldevents;

						spin_lock_irqsave(&subpriv->dio.
								  intr.spinlock,
								  flags);

						oldevents = s->async->events;

						if (subpriv->dio.intr.active) {
							unsigned mytrig =
							    ((triggered >>
							      subpriv->dio.intr.asic_chan)
							     &
							     ((0x1 << subpriv->
							       dio.intr.
							       num_asic_chans) -
							      1)) << subpriv->
							    dio.intr.first_chan;
							if (mytrig &
							    subpriv->dio.
							    intr.enabled_mask) {
								unsigned int val
								    = 0;
								unsigned int n,
								    ch, len;

								len =
								    s->
								    async->cmd.chanlist_len;
								for (n = 0;
								     n < len;
								     n++) {
									ch = CR_CHAN(s->async->cmd.chanlist[n]);
									if (mytrig & (1U << ch))
										val |= (1U << n);
								}
								
								if (comedi_buf_put(s->async, ((short *)&val)[0])
								    &&
								    comedi_buf_put
								    (s->async,
								     ((short *)
								      &val)[1])) {
									s->async->events |= (COMEDI_CB_BLOCK | COMEDI_CB_EOS);
								} else {
									
									
									pcmmio_stop_intr
									    (dev,
									     s);
								}

								
								if (!subpriv->dio.intr.continuous) {
									
									if (subpriv->dio.intr.stop_count > 0) {
										subpriv->dio.intr.stop_count--;
										if (subpriv->dio.intr.stop_count == 0) {
											s->async->events |= COMEDI_CB_EOA;
											
											pcmmio_stop_intr
											    (dev,
											     s);
										}
									}
								}
							}
						}

						spin_unlock_irqrestore
						    (&subpriv->dio.intr.
						     spinlock, flags);

						if (oldevents !=
						    s->async->events) {
							comedi_event(dev, s);
						}

					}

				}
			}

		}
	}
	if (!got1)
		return IRQ_NONE;	
	return IRQ_HANDLED;
}

static void pcmmio_stop_intr(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	int nports, firstport, asic, port;

	asic = subpriv->dio.intr.asic;
	if (asic < 0)
		return;		

	subpriv->dio.intr.enabled_mask = 0;
	subpriv->dio.intr.active = 0;
	s->async->inttrig = 0;
	nports = subpriv->dio.intr.num_asic_chans / CHANS_PER_PORT;
	firstport = subpriv->dio.intr.asic_chan / CHANS_PER_PORT;
	switch_page(dev, asic, PAGE_ENAB);
	for (port = firstport; port < firstport + nports; ++port) {
		
		outb(0, devpriv->asics[asic].iobase + REG_ENAB0 + port);
	}
}

static int pcmmio_start_intr(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	if (!subpriv->dio.intr.continuous && subpriv->dio.intr.stop_count == 0) {
		
		s->async->events |= COMEDI_CB_EOA;
		subpriv->dio.intr.active = 0;
		return 1;
	} else {
		unsigned bits = 0, pol_bits = 0, n;
		int nports, firstport, asic, port;
		struct comedi_cmd *cmd = &s->async->cmd;

		asic = subpriv->dio.intr.asic;
		if (asic < 0)
			return 1;	
		subpriv->dio.intr.enabled_mask = 0;
		subpriv->dio.intr.active = 1;
		nports = subpriv->dio.intr.num_asic_chans / CHANS_PER_PORT;
		firstport = subpriv->dio.intr.asic_chan / CHANS_PER_PORT;
		if (cmd->chanlist) {
			for (n = 0; n < cmd->chanlist_len; n++) {
				bits |= (1U << CR_CHAN(cmd->chanlist[n]));
				pol_bits |= (CR_AREF(cmd->chanlist[n])
					     || CR_RANGE(cmd->
							 chanlist[n]) ? 1U : 0U)
				    << CR_CHAN(cmd->chanlist[n]);
			}
		}
		bits &= ((0x1 << subpriv->dio.intr.num_asic_chans) -
			 1) << subpriv->dio.intr.first_chan;
		subpriv->dio.intr.enabled_mask = bits;

		{
			unsigned char b;
			outb(1 << 4, dev->iobase + 3);
			
			b = dev->irq & ((1 << 4) - 1);
			outb(b, dev->iobase + 2);
			
		}

		switch_page(dev, asic, PAGE_ENAB);
		for (port = firstport; port < firstport + nports; ++port) {
			unsigned enab =
			    bits >> (subpriv->dio.intr.first_chan + (port -
								     firstport)
				     * 8) & 0xff, pol =
			    pol_bits >> (subpriv->dio.intr.first_chan +
					 (port - firstport) * 8) & 0xff;
			
			outb(enab,
			     devpriv->asics[asic].iobase + REG_ENAB0 + port);
			switch_page(dev, asic, PAGE_POL);
			outb(pol,
			     devpriv->asics[asic].iobase + REG_ENAB0 + port);
		}
	}
	return 0;
}

static int pcmmio_cancel(struct comedi_device *dev, struct comedi_subdevice *s)
{
	unsigned long flags;

	spin_lock_irqsave(&subpriv->dio.intr.spinlock, flags);
	if (subpriv->dio.intr.active)
		pcmmio_stop_intr(dev, s);
	spin_unlock_irqrestore(&subpriv->dio.intr.spinlock, flags);

	return 0;
}

static int
pcmmio_inttrig_start_intr(struct comedi_device *dev, struct comedi_subdevice *s,
			  unsigned int trignum)
{
	unsigned long flags;
	int event = 0;

	if (trignum != 0)
		return -EINVAL;

	spin_lock_irqsave(&subpriv->dio.intr.spinlock, flags);
	s->async->inttrig = 0;
	if (subpriv->dio.intr.active)
		event = pcmmio_start_intr(dev, s);
	spin_unlock_irqrestore(&subpriv->dio.intr.spinlock, flags);

	if (event)
		comedi_event(dev, s);

	return 1;
}

static int pcmmio_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned long flags;
	int event = 0;

	spin_lock_irqsave(&subpriv->dio.intr.spinlock, flags);
	subpriv->dio.intr.active = 1;

	
	switch (cmd->stop_src) {
	case TRIG_COUNT:
		subpriv->dio.intr.continuous = 0;
		subpriv->dio.intr.stop_count = cmd->stop_arg;
		break;
	default:
		
		subpriv->dio.intr.continuous = 1;
		subpriv->dio.intr.stop_count = 0;
		break;
	}

	
	switch (cmd->start_src) {
	case TRIG_INT:
		s->async->inttrig = pcmmio_inttrig_start_intr;
		break;
	default:
		
		event = pcmmio_start_intr(dev, s);
		break;
	}
	spin_unlock_irqrestore(&subpriv->dio.intr.spinlock, flags);

	if (event)
		comedi_event(dev, s);

	return 0;
}

static int
pcmmio_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
	       struct comedi_cmd *cmd)
{
	return comedi_pcm_cmdtest(dev, s, cmd);
}

static int adc_wait_ready(unsigned long iobase)
{
	unsigned long retry = 100000;
	while (retry--)
		if (inb(iobase + 3) & 0x80)
			return 0;
	return 1;
}

static int ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data)
{
	int n;
	unsigned long iobase = subpriv->iobase;


	
	for (n = 0; n < insn->n; n++) {
		unsigned chan = CR_CHAN(insn->chanspec), range =
		    CR_RANGE(insn->chanspec), aref = CR_AREF(insn->chanspec);
		unsigned char command_byte = 0;
		unsigned iooffset = 0;
		short sample, adc_adjust = 0;

		if (chan > 7)
			chan -= 8, iooffset = 4;	

		if (aref != AREF_DIFF) {
			aref = AREF_GROUND;
			command_byte |= 1 << 7;	
		}
		if (range < 2)
			adc_adjust = 0x8000;	

		if (chan % 2) {
			command_byte |= 1 << 6;	
		}

		
		command_byte |= ((chan / 2) & 0x3) << 4;

		
		command_byte |= (range & 0x3) << 2;

		
		
		outb(command_byte, iobase + iooffset + 2);

		
		adc_wait_ready(iobase + iooffset);

		
		outb(command_byte, iobase + iooffset + 2);

		adc_wait_ready(iobase + iooffset);

		
		sample = inb(iobase + iooffset + 0);

		
		sample |= inb(iobase + iooffset + 1) << 8;
		sample += adc_adjust;	
		data[n] = sample;
	}
	/* return the number of samples read/written */
	return n;
}

static int ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data)
{
	int n;
	for (n = 0; n < insn->n; n++) {
		unsigned chan = CR_CHAN(insn->chanspec);
		if (chan < s->n_chan)
			data[n] = subpriv->ao.shadow_samples[chan];
	}
	return n;
}

static int wait_dac_ready(unsigned long iobase)
{
	unsigned long retry = 100000L;


	while (retry--) {
		if (inb(iobase + 3) & 0x80)
			return 0;

	}
	return 1;
}

static int ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data)
{
	int n;
	unsigned iobase = subpriv->iobase, iooffset = 0;

	for (n = 0; n < insn->n; n++) {
		unsigned chan = CR_CHAN(insn->chanspec), range =
		    CR_RANGE(insn->chanspec);
		if (chan < s->n_chan) {
			unsigned char command_byte = 0, range_byte =
			    range & ((1 << 4) - 1);
			if (chan >= 4)
				chan -= 4, iooffset += 4;
			
			outb(range_byte, iobase + iooffset + 0);
			outb(0, iobase + iooffset + 1);

			
			command_byte = (chan << 1) | 0x60;
			outb(command_byte, iobase + iooffset + 2);

			wait_dac_ready(iobase + iooffset);

			
			outb(data[n] & 0xff, iobase + iooffset + 0);

			
			outb((data[n] >> 8) & 0xff, iobase + iooffset + 1);

			command_byte = 0x70 | (chan << 1);
			
			outb(command_byte, iobase + iooffset + 2);

			wait_dac_ready(iobase + iooffset);

			
			subpriv->ao.shadow_samples[chan] = data[n];
		}
	}
	return n;
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
