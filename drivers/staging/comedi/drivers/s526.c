/*
    comedi/drivers/s526.c
    Sensoray s526 Comedi driver

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

#include "../comedidev.h"
#include <linux/ioport.h>
#include <asm/byteorder.h>

#define S526_SIZE 64

#define S526_START_AI_CONV	0
#define S526_AI_READ		0

#define S526_IOSIZE 0x40
#define S526_NUM_PORTS 27

#define REG_TCR 0x00
#define REG_WDC 0x02
#define REG_DAC 0x04
#define REG_ADC 0x06
#define REG_ADD 0x08
#define REG_DIO 0x0A
#define REG_IER 0x0C
#define REG_ISR 0x0E
#define REG_MSC 0x10
#define REG_C0L 0x12
#define REG_C0H 0x14
#define REG_C0M 0x16
#define REG_C0C 0x18
#define REG_C1L 0x1A
#define REG_C1H 0x1C
#define REG_C1M 0x1E
#define REG_C1C 0x20
#define REG_C2L 0x22
#define REG_C2H 0x24
#define REG_C2M 0x26
#define REG_C2C 0x28
#define REG_C3L 0x2A
#define REG_C3H 0x2C
#define REG_C3M 0x2E
#define REG_C3C 0x30
#define REG_EED 0x32
#define REG_EEC 0x34

static const int s526_ports[] = {
	REG_TCR,
	REG_WDC,
	REG_DAC,
	REG_ADC,
	REG_ADD,
	REG_DIO,
	REG_IER,
	REG_ISR,
	REG_MSC,
	REG_C0L,
	REG_C0H,
	REG_C0M,
	REG_C0C,
	REG_C1L,
	REG_C1H,
	REG_C1M,
	REG_C1C,
	REG_C2L,
	REG_C2H,
	REG_C2M,
	REG_C2C,
	REG_C3L,
	REG_C3H,
	REG_C3M,
	REG_C3C,
	REG_EED,
	REG_EEC
};

struct counter_mode_register_t {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned short coutSource:1;
	unsigned short coutPolarity:1;
	unsigned short autoLoadResetRcap:3;
	unsigned short hwCtEnableSource:2;
	unsigned short ctEnableCtrl:2;
	unsigned short clockSource:2;
	unsigned short countDir:1;
	unsigned short countDirCtrl:1;
	unsigned short outputRegLatchCtrl:1;
	unsigned short preloadRegSel:1;
	unsigned short reserved:1;
 #elif defined(__BIG_ENDIAN_BITFIELD)
	unsigned short reserved:1;
	unsigned short preloadRegSel:1;
	unsigned short outputRegLatchCtrl:1;
	unsigned short countDirCtrl:1;
	unsigned short countDir:1;
	unsigned short clockSource:2;
	unsigned short ctEnableCtrl:2;
	unsigned short hwCtEnableSource:2;
	unsigned short autoLoadResetRcap:3;
	unsigned short coutPolarity:1;
	unsigned short coutSource:1;
#else
#error Unknown bit field order
#endif
};

union cmReg {
	struct counter_mode_register_t reg;
	unsigned short value;
};

#define MAX_GPCT_CONFIG_DATA 6

enum S526_GPCT_APP_CLASS {
	CountingAndTimeMeasurement,
	SinglePulseGeneration,
	PulseTrainGeneration,
	PositionMeasurement,
	Miscellaneous
};

struct s526GPCTConfig {
	enum S526_GPCT_APP_CLASS app;
	int data[MAX_GPCT_CONFIG_DATA];
};

struct s526_board {
	const char *name;
	int gpct_chans;
	int gpct_bits;
	int ad_chans;
	int ad_bits;
	int da_chans;
	int da_bits;
	int have_dio;
};

static const struct s526_board s526_boards[] = {
	{
	 .name = "s526",
	 .gpct_chans = 4,
	 .gpct_bits = 24,
	 .ad_chans = 8,
	 .ad_bits = 16,
	 .da_chans = 4,
	 .da_bits = 16,
	 .have_dio = 1,
	 }
};

#define ADDR_REG(reg) (dev->iobase + (reg))
#define ADDR_CHAN_REG(reg, chan) (dev->iobase + (reg) + (chan) * 8)

#define thisboard ((const struct s526_board *)dev->board_ptr)

struct s526_private {

	int data;

	
	struct pci_dev *pci_dev;

	
	unsigned int ao_readback[2];

	struct s526GPCTConfig s526_gpct_config[4];
	unsigned short s526_ai_config;
};

#define devpriv ((struct s526_private *)dev->private)

static int s526_attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int s526_detach(struct comedi_device *dev);
static struct comedi_driver driver_s526 = {
	.driver_name = "s526",
	.module = THIS_MODULE,
	.attach = s526_attach,
	.detach = s526_detach,
	.board_name = &s526_boards[0].name,
	.offset = sizeof(struct s526_board),
	.num_names = ARRAY_SIZE(s526_boards),
};

static int s526_gpct_rinsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data);
static int s526_gpct_insn_config(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data);
static int s526_gpct_winsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data);
static int s526_ai_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
static int s526_ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data);
static int s526_ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data);
static int s526_ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data);
static int s526_dio_insn_bits(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
static int s526_dio_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data);

static int s526_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int iobase;
	int i, n;
	union cmReg cmReg;

	printk(KERN_INFO "comedi%d: s526: ", dev->minor);

	iobase = it->options[0];
	if (!iobase || !request_region(iobase, S526_IOSIZE, thisboard->name)) {
		comedi_error(dev, "I/O port conflict");
		return -EIO;
	}
	dev->iobase = iobase;

	printk("iobase=0x%lx\n", dev->iobase);


	dev->board_ptr = &s526_boards[0];

	dev->board_name = thisboard->name;

	if (alloc_private(dev, sizeof(struct s526_private)) < 0)
		return -ENOMEM;

	dev->n_subdevices = 4;
	if (alloc_subdevices(dev, dev->n_subdevices) < 0)
		return -ENOMEM;

	s = dev->subdevices + 0;
	
	s->type = COMEDI_SUBD_COUNTER;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE | SDF_LSAMPL;
	
	s->n_chan = thisboard->gpct_chans;
	s->maxdata = 0x00ffffff;	
	s->insn_read = s526_gpct_rinsn;
	s->insn_config = s526_gpct_insn_config;
	s->insn_write = s526_gpct_winsn;

	
	
	

	s = dev->subdevices + 1;
	
	
	s->type = COMEDI_SUBD_AI;
	
	s->subdev_flags = SDF_READABLE | SDF_DIFF;
	
	
	s->n_chan = 10;
	s->maxdata = 0xffff;
	s->range_table = &range_bipolar10;
	s->len_chanlist = 16;	
	s->insn_read = s526_ai_rinsn;
	s->insn_config = s526_ai_insn_config;

	s = dev->subdevices + 2;
	
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE;
	s->n_chan = 4;
	s->maxdata = 0xffff;
	s->range_table = &range_bipolar10;
	s->insn_write = s526_ao_winsn;
	s->insn_read = s526_ao_rinsn;

	s = dev->subdevices + 3;
	
	if (thisboard->have_dio) {
		s->type = COMEDI_SUBD_DIO;
		s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
		s->n_chan = 8;
		s->maxdata = 1;
		s->range_table = &range_digital;
		s->insn_bits = s526_dio_insn_bits;
		s->insn_config = s526_dio_insn_config;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	printk(KERN_INFO "attached\n");

	return 1;

#if 0
	
	
	cmReg.reg.coutSource = 0;	
	cmReg.reg.coutPolarity = 1;	
	cmReg.reg.autoLoadResetRcap = 1;
	cmReg.reg.hwCtEnableSource = 3;	
	cmReg.reg.ctEnableCtrl = 2;	
	cmReg.reg.clockSource = 2;	
	cmReg.reg.countDir = 1;	
	cmReg.reg.countDirCtrl = 1;	
	cmReg.reg.outputRegLatchCtrl = 0;	
	cmReg.reg.preloadRegSel = 0;	
	cmReg.reg.reserved = 0;

	outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, subdev_channel));

	outw(0x0001, ADDR_CHAN_REG(REG_C0H, subdev_channel));
	outw(0x3C68, ADDR_CHAN_REG(REG_C0L, subdev_channel));

	
	outw(0x8000, ADDR_CHAN_REG(REG_C0C, subdev_channel));
	
	outw(0x4000, ADDR_CHAN_REG(REG_C0C, subdev_channel));
	
	outw(0x0008, ADDR_CHAN_REG(REG_C0C, subdev_channel));

#else

	
	cmReg.reg.coutSource = 0;	
	cmReg.reg.coutPolarity = 0;	
	cmReg.reg.autoLoadResetRcap = 0;	
	cmReg.reg.hwCtEnableSource = 2;	
	cmReg.reg.ctEnableCtrl = 1;	
	cmReg.reg.clockSource = 3;	
	cmReg.reg.countDir = 0;	
	cmReg.reg.countDirCtrl = 0;	
	cmReg.reg.outputRegLatchCtrl = 0;	
	cmReg.reg.preloadRegSel = 0;	
	cmReg.reg.reserved = 0;

	n = 0;
	printk(KERN_INFO "Mode reg=0x%04x, 0x%04lx\n",
		cmReg.value, ADDR_CHAN_REG(REG_C0M, n));
	outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, n));
	udelay(1000);
	printk(KERN_INFO "Read back mode reg=0x%04x\n",
		inw(ADDR_CHAN_REG(REG_C0M, n)));

	

	

	

	
	if (cmReg.reg.autoLoadResetRcap == 0) {
		
		outw(0x8000, ADDR_CHAN_REG(REG_C0C, n));
		
		outw(0x4000, ADDR_CHAN_REG(REG_C0C, n));
	}

	outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, n));
	udelay(1000);
	printk(KERN_INFO "Read back mode reg=0x%04x\n",
			inw(ADDR_CHAN_REG(REG_C0M, n)));

#endif
	printk(KERN_INFO "Current registres:\n");

	for (i = 0; i < S526_NUM_PORTS; i++) {
		printk(KERN_INFO "0x%02lx: 0x%04x\n",
			ADDR_REG(s526_ports[i]), inw(ADDR_REG(s526_ports[i])));
	}
	return 1;
}

static int s526_detach(struct comedi_device *dev)
{
	printk(KERN_INFO "comedi%d: s526: remove\n", dev->minor);

	if (dev->iobase > 0)
		release_region(dev->iobase, S526_IOSIZE);

	return 0;
}

static int s526_gpct_rinsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data)
{
	int i;			
	int counter_channel = CR_CHAN(insn->chanspec);
	unsigned short datalow;
	unsigned short datahigh;

	
	if (insn->n <= 0) {
		printk(KERN_ERR "s526: INSN_READ: n should be > 0\n");
		return -EINVAL;
	}
	
	for (i = 0; i < insn->n; i++) {
		datalow = inw(ADDR_CHAN_REG(REG_C0L, counter_channel));
		datahigh = inw(ADDR_CHAN_REG(REG_C0H, counter_channel));
		data[i] = (int)(datahigh & 0x00FF);
		data[i] = (data[i] << 16) | (datalow & 0xFFFF);
	}
	return i;
}

static int s526_gpct_insn_config(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data)
{
	int subdev_channel = CR_CHAN(insn->chanspec);	
	int i;
	short value;
	union cmReg cmReg;


	for (i = 0; i < MAX_GPCT_CONFIG_DATA; i++) {
		devpriv->s526_gpct_config[subdev_channel].data[i] =
		    insn->data[i];
	}

	
	
	switch (insn->data[0]) {
	case INSN_CONFIG_GPCT_QUADRATURE_ENCODER:
		printk(KERN_INFO "s526: GPCT_INSN_CONFIG: Configuring Encoder\n");
		devpriv->s526_gpct_config[subdev_channel].app =
		    PositionMeasurement;

#if 0
		
		
		cmReg.reg.coutSource = 0;	
		cmReg.reg.coutPolarity = 1;	
		cmReg.reg.autoLoadResetRcap = 0;
		cmReg.reg.hwCtEnableSource = 3;	
		cmReg.reg.ctEnableCtrl = 2;	
		cmReg.reg.clockSource = 2;	
		cmReg.reg.countDir = 1;	
		cmReg.reg.countDirCtrl = 1;	
		cmReg.reg.outputRegLatchCtrl = 0;	
		cmReg.reg.preloadRegSel = 0;	
		cmReg.reg.reserved = 0;

		outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, subdev_channel));

		outw(0x0001, ADDR_CHAN_REG(REG_C0H, subdev_channel));
		outw(0x3C68, ADDR_CHAN_REG(REG_C0L, subdev_channel));

		
		outw(0x8000, ADDR_CHAN_REG(REG_C0C, subdev_channel));
		
		outw(0x4000, ADDR_CHAN_REG(REG_C0C, subdev_channel));

		
		outw(0x0008, ADDR_CHAN_REG(REG_C0C, subdev_channel));

#endif

#if 1
		
		cmReg.value = insn->data[1] & 0xFFFF;

		outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, subdev_channel));

		
		if (cmReg.reg.autoLoadResetRcap == 0) {
			
			outw(0x8000, ADDR_CHAN_REG(REG_C0C, subdev_channel));
		}
#else
		
		cmReg.reg.countDirCtrl = 0;

		
		if (insn->data[1] == GPCT_X2)
			cmReg.reg.clockSource = 1;
		else if (insn->data[1] == GPCT_X4)
			cmReg.reg.clockSource = 2;
		else
			cmReg.reg.clockSource = 0;

		
		
		if (insn->data[3] == GPCT_RESET_COUNTER_ON_INDEX)
			
			cmReg.reg.autoLoadResetRcap = 4;

		
		cmReg.value = (short)(insn->data[1] & 0xFFFF);
		outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, subdev_channel));

		
		value = (short)((insn->data[2] >> 16) & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0H, subdev_channel));

		
		value = (short)(insn->data[2] & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0L, subdev_channel));

		
		if (insn->data[3] != 0) {
			value = (short)(insn->data[3] & 0xFFFF);
			outw(value, ADDR_CHAN_REG(REG_C0C, subdev_channel));
		}
		
		if (cmReg.reg.autoLoadResetRcap == 0) {
			
			outw(0x8000, ADDR_CHAN_REG(REG_C0C, subdev_channel));
			
			outw(0x4000, ADDR_CHAN_REG(REG_C0C, subdev_channel));
		}
#endif
		break;

	case INSN_CONFIG_GPCT_SINGLE_PULSE_GENERATOR:
		printk(KERN_INFO "s526: GPCT_INSN_CONFIG: Configuring SPG\n");
		devpriv->s526_gpct_config[subdev_channel].app =
		    SinglePulseGeneration;

		
		cmReg.value = (short)(insn->data[1] & 0xFFFF);
		cmReg.reg.preloadRegSel = 0;	
		outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, subdev_channel));

		
		value = (short)((insn->data[2] >> 16) & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0H, subdev_channel));

		
		value = (short)(insn->data[2] & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0L, subdev_channel));

		
		cmReg.value = (short)(insn->data[1] & 0xFFFF);
		cmReg.reg.preloadRegSel = 1;	
		outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, subdev_channel));

		
		value = (short)((insn->data[3] >> 16) & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0H, subdev_channel));

		
		value = (short)(insn->data[3] & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0L, subdev_channel));

		
		if (insn->data[4] != 0) {
			value = (short)(insn->data[4] & 0xFFFF);
			outw(value, ADDR_CHAN_REG(REG_C0C, subdev_channel));
		}
		break;

	case INSN_CONFIG_GPCT_PULSE_TRAIN_GENERATOR:
		printk(KERN_INFO "s526: GPCT_INSN_CONFIG: Configuring PTG\n");
		devpriv->s526_gpct_config[subdev_channel].app =
		    PulseTrainGeneration;

		
		cmReg.value = (short)(insn->data[1] & 0xFFFF);
		cmReg.reg.preloadRegSel = 0;	
		outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, subdev_channel));

		
		value = (short)((insn->data[2] >> 16) & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0H, subdev_channel));

		
		value = (short)(insn->data[2] & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0L, subdev_channel));

		
		cmReg.value = (short)(insn->data[1] & 0xFFFF);
		cmReg.reg.preloadRegSel = 1;	
		outw(cmReg.value, ADDR_CHAN_REG(REG_C0M, subdev_channel));

		
		value = (short)((insn->data[3] >> 16) & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0H, subdev_channel));

		
		value = (short)(insn->data[3] & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0L, subdev_channel));

		
		if (insn->data[4] != 0) {
			value = (short)(insn->data[4] & 0xFFFF);
			outw(value, ADDR_CHAN_REG(REG_C0C, subdev_channel));
		}
		break;

	default:
		printk(KERN_ERR "s526: unsupported GPCT_insn_config\n");
		return -EINVAL;
		break;
	}

	return insn->n;
}

static int s526_gpct_winsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data)
{
	int subdev_channel = CR_CHAN(insn->chanspec);	
	short value;
	union cmReg cmReg;

	printk(KERN_INFO "s526: GPCT_INSN_WRITE on channel %d\n",
					subdev_channel);
	cmReg.value = inw(ADDR_CHAN_REG(REG_C0M, subdev_channel));
	printk(KERN_INFO "s526: Counter Mode Register: %x\n", cmReg.value);
	
	switch (devpriv->s526_gpct_config[subdev_channel].app) {
	case PositionMeasurement:
		printk(KERN_INFO "S526: INSN_WRITE: PM\n");
		outw(0xFFFF & ((*data) >> 16), ADDR_CHAN_REG(REG_C0H,
							     subdev_channel));
		outw(0xFFFF & (*data), ADDR_CHAN_REG(REG_C0L, subdev_channel));
		break;

	case SinglePulseGeneration:
		printk(KERN_INFO "S526: INSN_WRITE: SPG\n");
		outw(0xFFFF & ((*data) >> 16), ADDR_CHAN_REG(REG_C0H,
							     subdev_channel));
		outw(0xFFFF & (*data), ADDR_CHAN_REG(REG_C0L, subdev_channel));
		break;

	case PulseTrainGeneration:
		printk(KERN_INFO "S526: INSN_WRITE: PTG\n");
		if ((insn->data[1] > insn->data[0]) && (insn->data[0] > 0)) {
			(devpriv->s526_gpct_config[subdev_channel]).data[0] =
			    insn->data[0];
			(devpriv->s526_gpct_config[subdev_channel]).data[1] =
			    insn->data[1];
		} else {
			printk(KERN_ERR "s526: INSN_WRITE: PTG: Problem with Pulse params -> %d %d\n",
				insn->data[0], insn->data[1]);
			return -EINVAL;
		}

		value = (short)((*data >> 16) & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0H, subdev_channel));
		value = (short)(*data & 0xFFFF);
		outw(value, ADDR_CHAN_REG(REG_C0L, subdev_channel));
		break;
	default:		
		printk
		    ("s526: INSN_WRITE: Functionality %d not implemented yet\n",
		     devpriv->s526_gpct_config[subdev_channel].app);
		return -EINVAL;
		break;
	}
	/*  return the number of samples written */
	return insn->n;
}

#define ISR_ADC_DONE 0x4
static int s526_ai_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int result = -EINVAL;

	if (insn->n < 1)
		return result;

	result = insn->n;


	
	outw(ISR_ADC_DONE, ADDR_REG(REG_IER));
	devpriv->s526_ai_config = (data[0] & 0x3FF) << 5;
	if (data[1] > 0)
		devpriv->s526_ai_config |= 0x8000;	

	devpriv->s526_ai_config |= 0x0001;	

	return result;
}

static int s526_ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data)
{
	int n, i;
	int chan = CR_CHAN(insn->chanspec);
	unsigned short value;
	unsigned int d;
	unsigned int status;

	value = (devpriv->s526_ai_config & 0x8000) |
	    ((1 << 5) << chan) | (chan << 1) | 0x0001;

	
	for (n = 0; n < insn->n; n++) {
		
		outw(value, ADDR_REG(REG_ADC));

#define TIMEOUT 100
		
		for (i = 0; i < TIMEOUT; i++) {
			status = inw(ADDR_REG(REG_ISR));
			if (status & ISR_ADC_DONE) {
				outw(ISR_ADC_DONE, ADDR_REG(REG_ISR));
				break;
			}
		}
		if (i == TIMEOUT) {
			printk(KERN_ERR "s526: ADC(0x%04x) timeout\n",
			       inw(ADDR_REG(REG_ISR)));
			return -ETIMEDOUT;
		}

		
		d = inw(ADDR_REG(REG_ADD));

		
		data[n] = d ^ 0x8000;
	}

	/* return the number of samples read/written */
	return n;
}

static int s526_ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);
	unsigned short val;

	val = chan << 1;
	outw(val, ADDR_REG(REG_DAC));

	for (i = 0; i < insn->n; i++) {
		
		
		outw(data[i], ADDR_REG(REG_ADD));
		devpriv->ao_readback[chan] = data[i];
		outw(val + 1, ADDR_REG(REG_DAC)); 
	}

	/* return the number of samples read/written */
	return i;
}

static int s526_ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_readback[chan];

	return i;
}

static int s526_dio_insn_bits(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data)
{
	if (insn->n != 2)
		return -EINVAL;

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= data[0] & data[1];
		
		outw(s->state, ADDR_REG(REG_DIO));
	}

	data[1] = inw(ADDR_REG(REG_DIO)) & 0xFF; 
	

	return 2;
}

static int s526_dio_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int chan = CR_CHAN(insn->chanspec);
	int group, mask;

	printk(KERN_INFO "S526 DIO insn_config\n");


	group = chan >> 2;
	mask = 0xF << (group << 2);
	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
		
		s->state |= 1 << (group + 10);
		s->io_bits |= mask;
		break;
	case INSN_CONFIG_DIO_INPUT:
		s->state &= ~(1 << (group + 10)); 
		s->io_bits &= ~mask;
		break;
	case INSN_CONFIG_DIO_QUERY:
		data[1] = (s->io_bits & mask) ? COMEDI_OUTPUT : COMEDI_INPUT;
		return insn->n;
	default:
		return -EINVAL;
	}
	outw(s->state, ADDR_REG(REG_DIO));

	return 1;
}

static int __init driver_s526_init_module(void)
{
	return comedi_driver_register(&driver_s526);
}

static void __exit driver_s526_cleanup_module(void)
{
	comedi_driver_unregister(&driver_s526);
}

module_init(driver_s526_init_module);
module_exit(driver_s526_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
