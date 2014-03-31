/*
 *  comedi/drivers/das08.c
 *  DAS08 driver
 *
 *  COMEDI - Linux Control and Measurement Device Interface
 *  Copyright (C) 2000 David A. Schleef <ds@schleef.org>
 *  Copyright (C) 2001,2002,2003 Frank Mori Hess <fmhess@users.sourceforge.net>
 *  Copyright (C) 2004 Salvador E. Tropea <set@users.sf.net> <set@ieee.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************
 */


#include "../comedidev.h"

#include <linux/delay.h>

#include "comedi_pci.h"
#include "8255.h"
#include "das08.h"

#define DRV_NAME "das08"

#define PCI_VENDOR_ID_COMPUTERBOARDS 0x1307
#define PCI_DEVICE_ID_PCIDAS08 0x29
#define PCIDAS08_SIZE 0x54

#define INTCSR               0x4c
#define   INTR1_ENABLE         0x1
#define   INTR1_HIGH_POLARITY  0x2
#define   PCI_INTR_ENABLE      0x40
#define   INTR1_EDGE_TRIG      0x100	
#define CNTRL                0x50
#define   CNTRL_DIR            0x2
#define   CNTRL_INTR           0x4


#define DAS08_LSB		0
#define DAS08_MSB		1
#define DAS08_TRIG_12BIT	1
#define DAS08_STATUS		2
#define   DAS08_EOC			(1<<7)
#define   DAS08_IRQ			(1<<3)
#define   DAS08_IP(x)			(((x)>>4)&0x7)
#define DAS08_CONTROL		2
#define   DAS08_MUX_MASK	0x7
#define   DAS08_MUX(x)		((x) & DAS08_MUX_MASK)
#define   DAS08_INTE			(1<<3)
#define   DAS08_DO_MASK		0xf0
#define   DAS08_OP(x)		(((x) << 4) & DAS08_DO_MASK)


#define DAS08JR_DIO		3
#define DAS08JR_AO_LSB(x)	((x) ? 6 : 4)
#define DAS08JR_AO_MSB(x)	((x) ? 7 : 5)


#define DAS08AO_GAIN_CONTROL	3
#define DAS08AO_GAIN_STATUS	3

#define DAS08AO_AO_LSB(x)	((x) ? 0xa : 8)
#define DAS08AO_AO_MSB(x)	((x) ? 0xb : 9)
#define DAS08AO_AO_UPDATE	8


static int das08_ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data);
static int das08_di_rbits(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data);
static int das08_do_wbits(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data);
static int das08jr_di_rbits(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static int das08jr_do_wbits(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static int das08jr_ao_winsn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static int das08ao_ao_winsn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data);
static void i8254_set_mode_low(unsigned int base, int channel,
			       unsigned int mode);

static const struct comedi_lrange range_das08_pgl = { 9, {
							  BIP_RANGE(10),
							  BIP_RANGE(5),
							  BIP_RANGE(2.5),
							  BIP_RANGE(1.25),
							  BIP_RANGE(0.625),
							  UNI_RANGE(10),
							  UNI_RANGE(5),
							  UNI_RANGE(2.5),
							  UNI_RANGE(1.25)
							  }
};

static const struct comedi_lrange range_das08_pgh = { 12, {
							   BIP_RANGE(10),
							   BIP_RANGE(5),
							   BIP_RANGE(1),
							   BIP_RANGE(0.5),
							   BIP_RANGE(0.1),
							   BIP_RANGE(0.05),
							   BIP_RANGE(0.01),
							   BIP_RANGE(0.005),
							   UNI_RANGE(10),
							   UNI_RANGE(1),
							   UNI_RANGE(0.1),
							   UNI_RANGE(0.01),
							   }
};

static const struct comedi_lrange range_das08_pgm = { 9, {
							  BIP_RANGE(10),
							  BIP_RANGE(5),
							  BIP_RANGE(0.5),
							  BIP_RANGE(0.05),
							  BIP_RANGE(0.01),
							  UNI_RANGE(10),
							  UNI_RANGE(1),
							  UNI_RANGE(0.1),
							  UNI_RANGE(0.01)
							  }
};				

static const struct comedi_lrange *const das08_ai_lranges[] = {
	&range_unknown,
	&range_bipolar5,
	&range_das08_pgh,
	&range_das08_pgl,
	&range_das08_pgm,
};

static const int das08_pgh_gainlist[] = {
	8, 0, 10, 2, 12, 4, 14, 6, 1, 3, 5, 7
};
static const int das08_pgl_gainlist[] = { 8, 0, 2, 4, 6, 1, 3, 5, 7 };
static const int das08_pgm_gainlist[] = { 8, 0, 10, 12, 14, 9, 11, 13, 15 };

static const int *const das08_gainlists[] = {
	NULL,
	NULL,
	das08_pgh_gainlist,
	das08_pgl_gainlist,
	das08_pgm_gainlist,
};

static const struct das08_board_struct das08_boards[] = {
	{
	 .name = "isa-das08",	
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_pg_none,
	 .ai_encoding = das08_encode12,
	 .ao = NULL,
	 .ao_nbits = 12,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 4,
	 .i8255_offset = 8,
	 .i8254_offset = 4,
	 .iosize = 16,		
	 },
	{
	 .name = "das08-pgm",	
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_pgm,
	 .ai_encoding = das08_encode12,
	 .ao = NULL,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 4,
	 .i8255_offset = 0,
	 .i8254_offset = 0x04,
	 .iosize = 16,		
	 },
	{
	 .name = "das08-pgh",	
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_pgh,
	 .ai_encoding = das08_encode12,
	 .ao = NULL,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 4,
	 .i8255_offset = 0,
	 .i8254_offset = 0x04,
	 .iosize = 16,		
	 },
	{
	 .name = "das08-pgl",	
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_pgl,
	 .ai_encoding = das08_encode12,
	 .ao = NULL,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 4,
	 .i8255_offset = 0,
	 .i8254_offset = 0x04,
	 .iosize = 16,		
	 },
	{
	 .name = "das08-aoh",	
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_pgh,
	 .ai_encoding = das08_encode12,
	 .ao = das08ao_ao_winsn,	
	 .ao_nbits = 12,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 4,
	 .i8255_offset = 0x0c,
	 .i8254_offset = 0x04,
	 .iosize = 16,		
	 },
	{
	 .name = "das08-aol",	
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_pgl,
	 .ai_encoding = das08_encode12,
	 .ao = das08ao_ao_winsn,	
	 .ao_nbits = 12,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 4,
	 .i8255_offset = 0x0c,
	 .i8254_offset = 0x04,
	 .iosize = 16,		
	 },
	{
	 .name = "das08-aom",	
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_pgm,
	 .ai_encoding = das08_encode12,
	 .ao = das08ao_ao_winsn,	
	 .ao_nbits = 12,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 4,
	 .i8255_offset = 0x0c,
	 .i8254_offset = 0x04,
	 .iosize = 16,		
	 },
	{
	 .name = "das08/jr-ao",	
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_pg_none,
	 .ai_encoding = das08_encode12,
	 .ao = das08jr_ao_winsn,
	 .ao_nbits = 12,
	 .di = das08jr_di_rbits,
	 .do_ = das08jr_do_wbits,
	 .do_nchan = 8,
	 .i8255_offset = 0,
	 .i8254_offset = 0,
	 .iosize = 16,		
	 },
	{
	 .name = "das08jr-16-ao",	
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 16,
	 .ai_pg = das08_pg_none,
	 .ai_encoding = das08_encode12,
	 .ao = das08jr_ao_winsn,
	 .ao_nbits = 16,
	 .di = das08jr_di_rbits,
	 .do_ = das08jr_do_wbits,
	 .do_nchan = 8,
	 .i8255_offset = 0,
	 .i8254_offset = 0x04,
	 .iosize = 16,		
	 },
#ifdef CONFIG_COMEDI_PCI
	{
	 .name = "das08",	
	 .id = PCI_DEVICE_ID_PCIDAS08,
	 .bustype = pci,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_bipolar5,
	 .ai_encoding = das08_encode12,
	 .ao = NULL,
	 .ao_nbits = 0,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 4,
	 .i8255_offset = 0,
	 .i8254_offset = 4,
	 .iosize = 8,
	 },
#endif
	{
	 .name = "pc104-das08",
	 .bustype = pc104,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_pg_none,
	 .ai_encoding = das08_encode12,
	 .ao = NULL,
	 .ao_nbits = 0,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 4,
	 .i8255_offset = 0,
	 .i8254_offset = 4,
	 .iosize = 16,		
	 },
#if 0
	{
	 .name = "das08/f",
	 },
	{
	 .name = "das08jr",
	 },
#endif
	{
	 .name = "das08jr/16",
	 .bustype = isa,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 16,
	 .ai_pg = das08_pg_none,
	 .ai_encoding = das08_encode16,
	 .ao = NULL,
	 .ao_nbits = 0,
	 .di = das08jr_di_rbits,
	 .do_ = das08jr_do_wbits,
	 .do_nchan = 8,
	 .i8255_offset = 0,
	 .i8254_offset = 0,
	 .iosize = 16,		
	 },
#if 0
	{
	 .name = "das48-pga",	
	 },
	{
	 .name = "das08-pga-g2",	
	 },
#endif
};

#ifdef CONFIG_COMEDI_PCMCIA
struct das08_board_struct das08_cs_boards[NUM_DAS08_CS_BOARDS] = {
	{
	 .name = "pcm-das08",
	 .id = 0x0,		
	 .bustype = pcmcia,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_bipolar5,
	 .ai_encoding = das08_pcm_encode12,
	 .ao = NULL,
	 .ao_nbits = 0,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 3,
	 .i8255_offset = 0,
	 .i8254_offset = 0,
	 .iosize = 16,
	 },
	
	{
	 .name = "das08_cs",
	 .id = 0x0,		
	 .bustype = pcmcia,
	 .ai = das08_ai_rinsn,
	 .ai_nbits = 12,
	 .ai_pg = das08_bipolar5,
	 .ai_encoding = das08_pcm_encode12,
	 .ao = NULL,
	 .ao_nbits = 0,
	 .di = das08_di_rbits,
	 .do_ = das08_do_wbits,
	 .do_nchan = 3,
	 .i8255_offset = 0,
	 .i8254_offset = 0,
	 .iosize = 16,
	 },
};
#endif

#ifdef CONFIG_COMEDI_PCI
static DEFINE_PCI_DEVICE_TABLE(das08_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_COMPUTERBOARDS, PCI_DEVICE_ID_PCIDAS08) },
	{0}
};

MODULE_DEVICE_TABLE(pci, das08_pci_table);
#endif

#define devpriv ((struct das08_private_struct *)dev->private)
#define thisboard ((const struct das08_board_struct *)dev->board_ptr)

#define TIMEOUT 100000

static int das08_ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data)
{
	int i, n;
	int chan;
	int range;
	int lsb, msb;

	chan = CR_CHAN(insn->chanspec);
	range = CR_RANGE(insn->chanspec);

	
	inb(dev->iobase + DAS08_LSB);
	inb(dev->iobase + DAS08_MSB);

	
	
	spin_lock(&dev->spinlock);
	devpriv->do_mux_bits &= ~DAS08_MUX_MASK;
	devpriv->do_mux_bits |= DAS08_MUX(chan);
	outb(devpriv->do_mux_bits, dev->iobase + DAS08_CONTROL);
	spin_unlock(&dev->spinlock);

	if (s->range_table->length > 1) {
		
		range = CR_RANGE(insn->chanspec);
		outb(devpriv->pg_gainlist[range],
		     dev->iobase + DAS08AO_GAIN_CONTROL);
	}

	for (n = 0; n < insn->n; n++) {
		
		if (thisboard->ai_nbits == 16)
			if (inb(dev->iobase + DAS08_MSB) & 0x80)
				printk(KERN_INFO "das08: over-range\n");

		
		outb_p(0, dev->iobase + DAS08_TRIG_12BIT);

		for (i = 0; i < TIMEOUT; i++) {
			if (!(inb(dev->iobase + DAS08_STATUS) & DAS08_EOC))
				break;
		}
		if (i == TIMEOUT) {
			printk(KERN_ERR "das08: timeout\n");
			return -ETIME;
		}
		msb = inb(dev->iobase + DAS08_MSB);
		lsb = inb(dev->iobase + DAS08_LSB);
		if (thisboard->ai_encoding == das08_encode12) {
			data[n] = (lsb >> 4) | (msb << 4);
		} else if (thisboard->ai_encoding == das08_pcm_encode12) {
			data[n] = (msb << 8) + lsb;
		} else if (thisboard->ai_encoding == das08_encode16) {
			
			if (msb & 0x80)
				data[n] = (1 << 15) | lsb | ((msb & 0x7f) << 8);
			else
				data[n] = (1 << 15) - (lsb | (msb & 0x7f) << 8);
		} else {
			comedi_error(dev, "bug! unknown ai encoding");
			return -1;
		}
	}

	return n;
}

static int das08_di_rbits(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data)
{
	data[0] = 0;
	data[1] = DAS08_IP(inb(dev->iobase + DAS08_STATUS));

	return 2;
}

static int das08_do_wbits(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data)
{
	int wbits;

	
	wbits = (devpriv->do_mux_bits >> 4) & 0xf;
	
	wbits &= ~data[0];
	
	wbits |= data[0] & data[1];
	
	
	spin_lock(&dev->spinlock);
	devpriv->do_mux_bits &= ~DAS08_DO_MASK;
	devpriv->do_mux_bits |= DAS08_OP(wbits);
	outb(devpriv->do_mux_bits, dev->iobase + DAS08_CONTROL);
	spin_unlock(&dev->spinlock);

	data[1] = wbits;

	return 2;
}

static int das08jr_di_rbits(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	data[0] = 0;
	data[1] = inb(dev->iobase + DAS08JR_DIO);

	return 2;
}

static int das08jr_do_wbits(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	
	devpriv->do_bits &= ~data[0];
	
	devpriv->do_bits |= data[0] & data[1];
	outb(devpriv->do_bits, dev->iobase + DAS08JR_DIO);

	data[1] = devpriv->do_bits;

	return 2;
}

static int das08jr_ao_winsn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	int n;
	int lsb, msb;
	int chan;

	lsb = data[0] & 0xff;
	msb = (data[0] >> 8) & 0xf;

	chan = CR_CHAN(insn->chanspec);

	for (n = 0; n < insn->n; n++) {
#if 0
		outb(lsb, dev->iobase + devpriv->ao_offset_lsb[chan]);
		outb(msb, dev->iobase + devpriv->ao_offset_msb[chan]);
#else
		outb(lsb, dev->iobase + DAS08JR_AO_LSB(chan));
		outb(msb, dev->iobase + DAS08JR_AO_MSB(chan));
#endif

		
		inb(dev->iobase + DAS08JR_DIO);
	}

	return n;
}

static int das08ao_ao_winsn(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	int n;
	int lsb, msb;
	int chan;

	lsb = data[0] & 0xff;
	msb = (data[0] >> 8) & 0xf;

	chan = CR_CHAN(insn->chanspec);

	for (n = 0; n < insn->n; n++) {
#if 0
		outb(lsb, dev->iobase + devpriv->ao_offset_lsb[chan]);
		outb(msb, dev->iobase + devpriv->ao_offset_msb[chan]);
#else
		outb(lsb, dev->iobase + DAS08AO_AO_LSB(chan));
		outb(msb, dev->iobase + DAS08AO_AO_MSB(chan));
#endif

		
		inb(dev->iobase + DAS08AO_AO_UPDATE);
	}

	return n;
}

static unsigned int i8254_read_channel_low(unsigned int base, int chan)
{
	unsigned int msb, lsb;

	
	outb(chan << 6, base + I8254_CTRL);
	base += chan;
	lsb = inb(base);
	msb = inb(base);
	

	return lsb | (msb << 8);
}

static void i8254_write_channel_low(unsigned int base, int chan,
				    unsigned int value)
{
	unsigned int msb, lsb;

	lsb = value & 0xFF;
	msb = value >> 8;

	
	base += chan;
	
	
	outb(lsb, base);
	outb(msb, base);
	
}

static unsigned int i8254_read_channel(struct i8254_struct *st, int channel)
{
	int chan = st->logic2phys[channel];

	return i8254_read_channel_low(st->iobase, chan);
}

static void i8254_write_channel(struct i8254_struct *st, int channel,
				unsigned int value)
{
	int chan = st->logic2phys[channel];

	i8254_write_channel_low(st->iobase, chan, value);
}

static void i8254_initialize(struct i8254_struct *st)
{
	int i;
	for (i = 0; i < 3; ++i)
		i8254_set_mode_low(st->iobase, i, st->mode[i]);
}

static void i8254_set_mode_low(unsigned int base, int channel,
			       unsigned int mode)
{
	outb((channel << 6) | 0x30 | (mode & 0x0F), base + I8254_CTRL);
}

static void i8254_set_mode(struct i8254_struct *st, int channel,
			   unsigned int mode)
{
	int chan = st->logic2phys[channel];

	st->mode[chan] = mode;
	return i8254_set_mode_low(st->iobase, chan, mode);
}

static unsigned int i8254_read_status_low(unsigned int base, int channel)
{
	outb(0xE0 | (2 << channel), base + I8254_CTRL);
	return inb(base + channel);
}

static unsigned int i8254_read_status(struct i8254_struct *st, int channel)
{
	int chan = st->logic2phys[channel];

	return i8254_read_status_low(st->iobase, chan);
}

static int das08_counter_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data)
{
	int chan = insn->chanspec;

	
	data[0] = i8254_read_channel(&devpriv->i8254, chan);
	

	return 1;
}

static int das08_counter_write(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int chan = insn->chanspec;

	
	i8254_write_channel(&devpriv->i8254, chan, data[0]);

	return 1;
}

static int das08_counter_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int chan = insn->chanspec;

	if (insn->n != 2)
		return -EINVAL;

	switch (data[0]) {
	case INSN_CONFIG_SET_COUNTER_MODE:
		i8254_set_mode(&devpriv->i8254, chan, data[1]);
		break;
	case INSN_CONFIG_8254_READ_STATUS:
		data[1] = i8254_read_status(&devpriv->i8254, chan);
		break;
	default:
		return -EINVAL;
		break;
	}
	return 2;
}

static int das08_attach(struct comedi_device *dev, struct comedi_devconfig *it);

static struct comedi_driver driver_das08 = {
	.driver_name = DRV_NAME,
	.module = THIS_MODULE,
	.attach = das08_attach,
	.detach = das08_common_detach,
	.board_name = &das08_boards[0].name,
	.num_names = sizeof(das08_boards) / sizeof(struct das08_board_struct),
	.offset = sizeof(struct das08_board_struct),
};

int das08_common_attach(struct comedi_device *dev, unsigned long iobase)
{
	struct comedi_subdevice *s;
	int ret;

	
	if ((thisboard->bustype != pcmcia) && (thisboard->bustype != pci)) {
		printk(KERN_INFO " iobase 0x%lx\n", iobase);
		if (!request_region(iobase, thisboard->iosize, DRV_NAME)) {
			printk(KERN_ERR " I/O port conflict\n");
			return -EIO;
		}
	}
	dev->iobase = iobase;

	dev->board_name = thisboard->name;

	ret = alloc_subdevices(dev, 6);
	if (ret < 0)
		return ret;

	s = dev->subdevices + 0;
	
	if (thisboard->ai) {
		s->type = COMEDI_SUBD_AI;
		s->subdev_flags = SDF_READABLE | SDF_GROUND;
		s->n_chan = 8;
		s->maxdata = (1 << thisboard->ai_nbits) - 1;
		s->range_table = das08_ai_lranges[thisboard->ai_pg];
		s->insn_read = thisboard->ai;
		devpriv->pg_gainlist = das08_gainlists[thisboard->ai_pg];
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	s = dev->subdevices + 1;
	
	if (thisboard->ao) {
		s->type = COMEDI_SUBD_AO;
		s->subdev_flags = SDF_WRITABLE;
		s->n_chan = 2;
		s->maxdata = (1 << thisboard->ao_nbits) - 1;
		s->range_table = &range_bipolar5;
		s->insn_write = thisboard->ao;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	s = dev->subdevices + 2;
	
	if (thisboard->di) {
		s->type = COMEDI_SUBD_DI;
		s->subdev_flags = SDF_READABLE;
		s->n_chan = (thisboard->di == das08_di_rbits) ? 3 : 8;
		s->maxdata = 1;
		s->range_table = &range_digital;
		s->insn_bits = thisboard->di;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	s = dev->subdevices + 3;
	
	if (thisboard->do_) {
		s->type = COMEDI_SUBD_DO;
		s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
		s->n_chan = thisboard->do_nchan;
		s->maxdata = 1;
		s->range_table = &range_digital;
		s->insn_bits = thisboard->do_;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	s = dev->subdevices + 4;
	
	if (thisboard->i8255_offset != 0) {
		subdev_8255_init(dev, s, NULL, (unsigned long)(dev->iobase +
							       thisboard->
							       i8255_offset));
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	s = dev->subdevices + 5;
	
	if (thisboard->i8254_offset != 0) {
		s->type = COMEDI_SUBD_COUNTER;
		s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
		s->n_chan = 3;
		s->maxdata = 0xFFFF;
		s->insn_read = das08_counter_read;
		s->insn_write = das08_counter_write;
		s->insn_config = das08_counter_config;
		
		devpriv->i8254.channels = 3;
		devpriv->i8254.logic2phys[0] = 0;
		devpriv->i8254.logic2phys[1] = 1;
		devpriv->i8254.logic2phys[2] = 2;
		devpriv->i8254.iobase = iobase + thisboard->i8254_offset;
		devpriv->i8254.mode[0] =
		    devpriv->i8254.mode[1] =
		    devpriv->i8254.mode[2] = I8254_MODE0 | I8254_BINARY;
		i8254_initialize(&devpriv->i8254);
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(das08_common_attach);

static int das08_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	int ret;
	unsigned long iobase;
#ifdef CONFIG_COMEDI_PCI
	unsigned long pci_iobase = 0;
	struct pci_dev *pdev = NULL;
#endif

	ret = alloc_private(dev, sizeof(struct das08_private_struct));
	if (ret < 0)
		return ret;

	printk(KERN_INFO "comedi%d: das08: ", dev->minor);
	
	if (thisboard->bustype == pci) {
#ifdef CONFIG_COMEDI_PCI
		if (it->options[0] || it->options[1]) {
			printk("bus %i slot %i ",
			       it->options[0], it->options[1]);
		}
		printk("\n");
		
		for_each_pci_dev(pdev) {
			if (pdev->vendor == PCI_VENDOR_ID_COMPUTERBOARDS
			    && pdev->device == PCI_DEVICE_ID_PCIDAS08) {
				if (it->options[0] || it->options[1]) {
					if (pdev->bus->number == it->options[0]
					    && PCI_SLOT(pdev->devfn) ==
					    it->options[1]) {
						break;
					}
				} else {
					break;
				}
			}
		}
		if (!pdev) {
			printk(KERN_ERR "No pci das08 cards found\n");
			return -EIO;
		}
		devpriv->pdev = pdev;
		
		if (comedi_pci_enable(pdev, DRV_NAME)) {
			printk(KERN_ERR " Error enabling PCI device and "
						"requesting regions\n");
			return -EIO;
		}
		
		pci_iobase = pci_resource_start(pdev, 1);
		iobase = pci_resource_start(pdev, 2);
		printk(KERN_INFO "pcibase 0x%lx  iobase 0x%lx\n",
							pci_iobase, iobase);
		devpriv->pci_iobase = pci_iobase;
#if 0
		
		outb(CNTRL_INTR | CNTRL_DIR, pci_iobase + CNTRL);
		
		outw(INTR1_ENABLE | PCI_INTR_ENABLE, pci_iobase + INTCSR);
#endif
#else 
		printk(KERN_ERR "this driver has not been built with PCI support.\n");
		return -EINVAL;
#endif 
	} else {
		iobase = it->options[0];
	}
	printk(KERN_INFO "\n");

	return das08_common_attach(dev, iobase);
}


int das08_common_detach(struct comedi_device *dev)
{
	printk(KERN_INFO "comedi%d: das08: remove\n", dev->minor);

	if (dev->subdevices)
		subdev_8255_cleanup(dev, dev->subdevices + 4);

	
	if ((thisboard->bustype != pcmcia) && (thisboard->bustype != pci)) {
		if (dev->iobase)
			release_region(dev->iobase, thisboard->iosize);
	}
#ifdef CONFIG_COMEDI_PCI
	if (devpriv) {
		if (devpriv->pdev) {
			if (devpriv->pci_iobase)
				comedi_pci_disable(devpriv->pdev);

			pci_dev_put(devpriv->pdev);
		}
	}
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(das08_common_detach);

#ifdef CONFIG_COMEDI_PCI
static int __devinit driver_das08_pci_probe(struct pci_dev *dev,
					    const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, driver_das08.driver_name);
}

static void __devexit driver_das08_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_das08_pci_driver = {
	.id_table = das08_pci_table,
	.probe = &driver_das08_pci_probe,
	.remove = __devexit_p(&driver_das08_pci_remove)
};

static int __init driver_das08_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_das08);
	if (retval < 0)
		return retval;

	driver_das08_pci_driver.name = (char *)driver_das08.driver_name;
	return pci_register_driver(&driver_das08_pci_driver);
}

static void __exit driver_das08_cleanup_module(void)
{
	pci_unregister_driver(&driver_das08_pci_driver);
	comedi_driver_unregister(&driver_das08);
}

module_init(driver_das08_init_module);
module_exit(driver_das08_cleanup_module);
#else
static int __init driver_das08_init_module(void)
{
	return comedi_driver_register(&driver_das08);
}

static void __exit driver_das08_cleanup_module(void)
{
	comedi_driver_unregister(&driver_das08);
}

module_init(driver_das08_init_module);
module_exit(driver_das08_cleanup_module);
#endif

#ifdef CONFIG_COMEDI_PCMCIA
EXPORT_SYMBOL_GPL(das08_cs_boards);
#endif

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
