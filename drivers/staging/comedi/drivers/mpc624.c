/*
    comedi/drivers/mpc624.c
    Hardware driver for a Micro/sys inc. MPC-624 PC/104 board

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
#include <linux/delay.h>

#define MPC624_SIZE             16

#define MPC624_MASTER_CONTROL	0 
#define MPC624_GNMUXCH          1 
#define MPC624_ADC              2 
#define MPC624_EE               3 
#define MPC624_LEDS             4 
#define MPC624_DIO              5 
#define MPC624_IRQ_MASK         6 

#define MPC624_ADBUSY           (1<<5)
#define MPC624_ADSDO            (1<<4)
#define MPC624_ADFO             (1<<3)
#define MPC624_ADCS             (1<<2)
#define MPC624_ADSCK            (1<<1)
#define MPC624_ADSDI            (1<<0)

#define MPC624_OSR4             (1<<31)
#define MPC624_OSR3             (1<<30)
#define MPC624_OSR2             (1<<29)
#define MPC624_OSR1             (1<<28)
#define MPC624_OSR0             (1<<27)

#define MPC624_EOC_BIT          (1<<31)
#define MPC624_DMY_BIT          (1<<30)
#define MPC624_SGN_BIT          (1<<29)


#define MPC624_SPEED_3_52_kHz (MPC624_OSR4 | MPC624_OSR0)
#define MPC624_SPEED_1_76_kHz (MPC624_OSR4 | MPC624_OSR1)
#define MPC624_SPEED_880_Hz   (MPC624_OSR4 | MPC624_OSR1 | MPC624_OSR0)
#define MPC624_SPEED_440_Hz   (MPC624_OSR4 | MPC624_OSR2)
#define MPC624_SPEED_220_Hz   (MPC624_OSR4 | MPC624_OSR2 | MPC624_OSR0)
#define MPC624_SPEED_110_Hz   (MPC624_OSR4 | MPC624_OSR2 | MPC624_OSR1)
#define MPC624_SPEED_55_Hz \
	(MPC624_OSR4 | MPC624_OSR2 | MPC624_OSR1 | MPC624_OSR0)
#define MPC624_SPEED_27_5_Hz  (MPC624_OSR4 | MPC624_OSR3)
#define MPC624_SPEED_13_75_Hz (MPC624_OSR4 | MPC624_OSR3 | MPC624_OSR0)
#define MPC624_SPEED_6_875_Hz \
	(MPC624_OSR4 | MPC624_OSR3 | MPC624_OSR2 | MPC624_OSR1 | MPC624_OSR0)
struct skel_private {

	
	unsigned long int ulConvertionRate;
};

#define devpriv ((struct skel_private *)dev->private)
static const struct comedi_lrange range_mpc624_bipolar1 = {
	1,
	{
	 
	 BIP_RANGE(2.02)
	 }
};

static const struct comedi_lrange range_mpc624_bipolar10 = {
	1,
	{
	 
	 BIP_RANGE(20.2)
	 }
};

static int mpc624_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int mpc624_detach(struct comedi_device *dev);
static struct comedi_driver driver_mpc624 = {
	.driver_name = "mpc624",
	.module = THIS_MODULE,
	.attach = mpc624_attach,
	.detach = mpc624_detach
};

static int mpc624_ai_rinsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data);
static int mpc624_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	unsigned long iobase;

	iobase = it->options[0];
	printk(KERN_INFO "comedi%d: mpc624 [0x%04lx, ", dev->minor, iobase);
	if (request_region(iobase, MPC624_SIZE, "mpc624") == NULL) {
		printk(KERN_ERR "I/O port(s) in use\n");
		return -EIO;
	}

	dev->iobase = iobase;
	dev->board_name = "mpc624";

	
	if (alloc_private(dev, sizeof(struct skel_private)) < 0)
		return -ENOMEM;

	switch (it->options[1]) {
	case 0:
		devpriv->ulConvertionRate = MPC624_SPEED_3_52_kHz;
		printk(KERN_INFO "3.52 kHz, ");
		break;
	case 1:
		devpriv->ulConvertionRate = MPC624_SPEED_1_76_kHz;
		printk(KERN_INFO "1.76 kHz, ");
		break;
	case 2:
		devpriv->ulConvertionRate = MPC624_SPEED_880_Hz;
		printk(KERN_INFO "880 Hz, ");
		break;
	case 3:
		devpriv->ulConvertionRate = MPC624_SPEED_440_Hz;
		printk(KERN_INFO "440 Hz, ");
		break;
	case 4:
		devpriv->ulConvertionRate = MPC624_SPEED_220_Hz;
		printk(KERN_INFO "220 Hz, ");
		break;
	case 5:
		devpriv->ulConvertionRate = MPC624_SPEED_110_Hz;
		printk(KERN_INFO "110 Hz, ");
		break;
	case 6:
		devpriv->ulConvertionRate = MPC624_SPEED_55_Hz;
		printk(KERN_INFO "55 Hz, ");
		break;
	case 7:
		devpriv->ulConvertionRate = MPC624_SPEED_27_5_Hz;
		printk(KERN_INFO "27.5 Hz, ");
		break;
	case 8:
		devpriv->ulConvertionRate = MPC624_SPEED_13_75_Hz;
		printk(KERN_INFO "13.75 Hz, ");
		break;
	case 9:
		devpriv->ulConvertionRate = MPC624_SPEED_6_875_Hz;
		printk(KERN_INFO "6.875 Hz, ");
		break;
	default:
		printk
		    (KERN_ERR "illegal conversion rate setting!"
			" Valid numbers are 0..9. Using 9 => 6.875 Hz, ");
		devpriv->ulConvertionRate = MPC624_SPEED_3_52_kHz;
	}

	
	if (alloc_subdevices(dev, 1) < 0)
		return -ENOMEM;

	s = dev->subdevices + 0;
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_DIFF;
	s->n_chan = 8;
	switch (it->options[1]) {
	default:
		s->maxdata = 0x3FFFFFFF;
		printk(KERN_INFO "30 bit, ");
	}

	switch (it->options[1]) {
	case 0:
		s->range_table = &range_mpc624_bipolar1;
		printk(KERN_INFO "1.01V]: ");
		break;
	default:
		s->range_table = &range_mpc624_bipolar10;
		printk(KERN_INFO "10.1V]: ");
	}
	s->len_chanlist = 1;
	s->insn_read = mpc624_ai_rinsn;

	printk(KERN_INFO "attached\n");

	return 1;
}

static int mpc624_detach(struct comedi_device *dev)
{
	printk(KERN_INFO "comedi%d: mpc624: remove\n", dev->minor);

	if (dev->iobase)
		release_region(dev->iobase, MPC624_SIZE);

	return 0;
}

#define TIMEOUT 200

static int mpc624_ai_rinsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data)
{
	int n, i;
	unsigned long int data_in, data_out;
	unsigned char ucPort;

	outb(insn->chanspec, dev->iobase + MPC624_GNMUXCH);
	if (!insn->n) {
		printk(KERN_INFO "MPC624: Warning, no data to acquire\n");
		return 0;
	}

	for (n = 0; n < insn->n; n++) {
		
		outb(MPC624_ADSCK, dev->iobase + MPC624_ADC);
		udelay(1);
		outb(MPC624_ADCS | MPC624_ADSCK, dev->iobase + MPC624_ADC);
		udelay(1);
		outb(0, dev->iobase + MPC624_ADC);
		udelay(1);

		
		for (i = 0; i < TIMEOUT; i++) {
			ucPort = inb(dev->iobase + MPC624_ADC);
			if (ucPort & MPC624_ADBUSY)
				udelay(1000);
			else
				break;
		}
		if (i == TIMEOUT) {
			printk(KERN_ERR "MPC624: timeout (%dms)\n", TIMEOUT);
			data[n] = 0;
			return -ETIMEDOUT;
		}
		
		data_in = 0;
		data_out = devpriv->ulConvertionRate;
		udelay(1);
		for (i = 0; i < 32; i++) {
			
			outb(0, dev->iobase + MPC624_ADC);
			udelay(1);

			if (data_out & (1 << 31)) { 
				
				outb(MPC624_ADSDI, dev->iobase + MPC624_ADC);
				udelay(1);
				
				outb(MPC624_ADSCK | MPC624_ADSDI,
				     dev->iobase + MPC624_ADC);
			} else {	

				
				outb(0, dev->iobase + MPC624_ADC);
				udelay(1);
				
				outb(MPC624_ADSCK, dev->iobase + MPC624_ADC);
			}
			
			udelay(1);
			data_in <<= 1;
			data_in |=
			    (inb(dev->iobase + MPC624_ADC) & MPC624_ADSDO) >> 4;
			udelay(1);

			data_out <<= 1;
		}


		if (data_in & MPC624_EOC_BIT)
			printk(KERN_INFO "MPC624:EOC bit is set (data_in=%lu)!",
			       data_in);
		if (data_in & MPC624_DMY_BIT)
			printk(KERN_INFO "MPC624:DMY bit is set (data_in=%lu)!",
			       data_in);
		if (data_in & MPC624_SGN_BIT) {	
			data_in &= 0x3FFFFFFF;
			data[n] = data_in;
		} else { 
			data_in |= MPC624_SGN_BIT;
			data_in = ~data_in;
			data_in += 1;
			data_in &= ~(MPC624_EOC_BIT | MPC624_DMY_BIT);
			
			data_in = 0x20000000 - data_in;
			data[n] = data_in;
		}
	}

	/*  Return the number of samples read/written */
	return n;
}

static int __init driver_mpc624_init_module(void)
{
	return comedi_driver_register(&driver_mpc624);
}

static void __exit driver_mpc624_cleanup_module(void)
{
	comedi_driver_unregister(&driver_mpc624);
}

module_init(driver_mpc624_init_module);
module_exit(driver_mpc624_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
