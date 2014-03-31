 /*
    comedi/drivers/amplc_pci230.c
    Driver for Amplicon PCI230 and PCI260 Multifunction I/O boards.

    Copyright (C) 2001 Allan Willcox <allanwillcox@ozemail.com.au>

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

#include <linux/delay.h>
#include <linux/interrupt.h>

#include "comedi_pci.h"
#include "8253.h"
#include "8255.h"

#define PCI_VENDOR_ID_AMPLICON 0x14dc
#define PCI_DEVICE_ID_PCI230 0x0000
#define PCI_DEVICE_ID_PCI260 0x0006
#define PCI_DEVICE_ID_INVALID 0xffff

#define PCI230_IO1_SIZE 32	
#define PCI230_IO2_SIZE 16	

#define PCI230_PPI_X_BASE	0x00	
#define PCI230_PPI_X_A		0x00	
#define PCI230_PPI_X_B		0x01	
#define PCI230_PPI_X_C		0x02	
#define PCI230_PPI_X_CMD	0x03	
#define PCI230_Z2_CT_BASE	0x14	
#define PCI230_Z2_CT0		0x14	
#define PCI230_Z2_CT1		0x15	
#define PCI230_Z2_CT2		0x16	
#define PCI230_Z2_CTC		0x17	
#define PCI230_ZCLK_SCE		0x1A	
#define PCI230_ZGAT_SCE		0x1D	
#define PCI230_INT_SCE		0x1E	
#define PCI230_INT_STAT		0x1E	

#define PCI230_DACCON		0x00	
#define PCI230_DACOUT1		0x02	
#define PCI230_DACOUT2		0x04	
#define PCI230_ADCDATA		0x08	
#define PCI230_ADCSWTRIG	0x08	
#define PCI230_ADCCON		0x0A	
#define PCI230_ADCEN		0x0C	
#define PCI230_ADCG		0x0E	
#define PCI230P_ADCTRIG		0x10	
#define PCI230P_ADCTH		0x12	
#define PCI230P_ADCFFTH		0x14	
#define PCI230P_ADCFFLEV	0x16	
#define PCI230P_ADCPTSC		0x18	
#define PCI230P_ADCHYST		0x1A	
#define PCI230P_EXTFUNC		0x1C	
#define PCI230P_HWVER		0x1E	
#define PCI230P2_DACDATA	0x02	
#define PCI230P2_DACSWTRIG	0x02	
#define PCI230P2_DACEN		0x06	

#define PCI230_DAC_SETTLE 5	
				
#define PCI230_ADC_SETTLE 1	
#define PCI230_MUX_SETTLE 10	
				

#define PCI230_DAC_OR_UNI		(0<<0)	
#define PCI230_DAC_OR_BIP		(1<<0)	
#define PCI230_DAC_OR_MASK		(1<<0)
#define PCI230P2_DAC_FIFO_EN		(1<<8)	
#define PCI230P2_DAC_TRIG_NONE		(0<<2)	
#define PCI230P2_DAC_TRIG_SW		(1<<2)	
#define PCI230P2_DAC_TRIG_EXTP		(2<<2)	
#define PCI230P2_DAC_TRIG_EXTN		(3<<2)	
#define PCI230P2_DAC_TRIG_Z2CT0		(4<<2)	
#define PCI230P2_DAC_TRIG_Z2CT1		(5<<2)	
#define PCI230P2_DAC_TRIG_Z2CT2		(6<<2)	
#define PCI230P2_DAC_TRIG_MASK		(7<<2)
#define PCI230P2_DAC_FIFO_WRAP		(1<<7)	
#define PCI230P2_DAC_INT_FIFO_EMPTY	(0<<9)	
#define PCI230P2_DAC_INT_FIFO_NEMPTY	(1<<9)
#define PCI230P2_DAC_INT_FIFO_NHALF	(2<<9)	
#define PCI230P2_DAC_INT_FIFO_HALF	(3<<9)
#define PCI230P2_DAC_INT_FIFO_NFULL	(4<<9)	
#define PCI230P2_DAC_INT_FIFO_FULL	(5<<9)
#define PCI230P2_DAC_INT_FIFO_MASK	(7<<9)

#define PCI230_DAC_BUSY			(1<<1)	
#define PCI230P2_DAC_FIFO_UNDERRUN_LATCHED	(1<<5)	
#define PCI230P2_DAC_FIFO_EMPTY		(1<<13)	
#define PCI230P2_DAC_FIFO_FULL		(1<<14)	
#define PCI230P2_DAC_FIFO_HALF		(1<<15)	

#define PCI230P2_DAC_FIFO_UNDERRUN_CLEAR	(1<<5)	
#define PCI230P2_DAC_FIFO_RESET		(1<<12)	

#define PCI230P2_DAC_FIFOLEVEL_HALF	512
#define PCI230P2_DAC_FIFOLEVEL_FULL	1024
#define PCI230P2_DAC_FIFOROOM_EMPTY		PCI230P2_DAC_FIFOLEVEL_FULL
#define PCI230P2_DAC_FIFOROOM_ONETOHALF		\
	(PCI230P2_DAC_FIFOLEVEL_FULL - PCI230P2_DAC_FIFOLEVEL_HALF)
#define PCI230P2_DAC_FIFOROOM_HALFTOFULL	1
#define PCI230P2_DAC_FIFOROOM_FULL		0

#define PCI230_ADC_TRIG_NONE		(0<<0)	
#define PCI230_ADC_TRIG_SW		(1<<0)	
#define PCI230_ADC_TRIG_EXTP		(2<<0)	
#define PCI230_ADC_TRIG_EXTN		(3<<0)	
#define PCI230_ADC_TRIG_Z2CT0		(4<<0)	
#define PCI230_ADC_TRIG_Z2CT1		(5<<0)	
#define PCI230_ADC_TRIG_Z2CT2		(6<<0)	
#define PCI230_ADC_TRIG_MASK		(7<<0)
#define PCI230_ADC_IR_UNI		(0<<3)	
#define PCI230_ADC_IR_BIP		(1<<3)	
#define PCI230_ADC_IR_MASK		(1<<3)
#define PCI230_ADC_IM_SE		(0<<4)	
#define PCI230_ADC_IM_DIF		(1<<4)	
#define PCI230_ADC_IM_MASK		(1<<4)
#define PCI230_ADC_FIFO_EN		(1<<8)	
#define PCI230_ADC_INT_FIFO_EMPTY	(0<<9)
#define PCI230_ADC_INT_FIFO_NEMPTY	(1<<9)	
#define PCI230_ADC_INT_FIFO_NHALF	(2<<9)
#define PCI230_ADC_INT_FIFO_HALF	(3<<9)	
#define PCI230_ADC_INT_FIFO_NFULL	(4<<9)
#define PCI230_ADC_INT_FIFO_FULL	(5<<9)	
#define PCI230P_ADC_INT_FIFO_THRESH	(7<<9)	
#define PCI230_ADC_INT_FIFO_MASK	(7<<9)

#define PCI230_ADC_FIFO_RESET		(1<<12)	
#define PCI230_ADC_GLOB_RESET		(1<<13)	

#define PCI230_ADC_BUSY			(1<<15)	
#define PCI230_ADC_FIFO_EMPTY		(1<<12)	
#define PCI230_ADC_FIFO_FULL		(1<<13)	
#define PCI230_ADC_FIFO_HALF		(1<<14)	
#define PCI230_ADC_FIFO_FULL_LATCHED	(1<<5)	

#define PCI230_ADC_FIFOLEVEL_HALFFULL	2049	
#define PCI230_ADC_FIFOLEVEL_FULL	4096	

#define PCI230_ADC_CONV			0xffff

#define PCI230P_EXTFUNC_GAT_EXTTRIG	(1<<0)
			
#define PCI230P2_EXTFUNC_DACFIFO	(1<<1)
			

#define CLK_CLK		0	
#define CLK_10MHZ	1	
#define CLK_1MHZ	2	
#define CLK_100KHZ	3	
#define CLK_10KHZ	4	
#define CLK_1KHZ	5	
#define CLK_OUTNM1	6	
#define CLK_EXT		7	
#define CLK_CONFIG(chan, src)	((((chan) & 3) << 3) | ((src) & 7))
#define TIMEBASE_10MHZ		100
#define TIMEBASE_1MHZ		1000
#define TIMEBASE_100KHZ		10000
#define TIMEBASE_10KHZ		100000
#define TIMEBASE_1KHZ		1000000

#define GAT_VCC		0	
#define GAT_GND		1	
#define GAT_EXT		2	
#define GAT_NOUTNM2	3	
#define GAT_CONFIG(chan, src)	((((chan) & 3) << 3) | ((src) & 7))


#define PCI230_INT_DISABLE		0
#define PCI230_INT_PPI_C0		(1<<0)
#define PCI230_INT_PPI_C3		(1<<1)
#define PCI230_INT_ADC			(1<<2)
#define PCI230_INT_ZCLK_CT1		(1<<5)
#define PCI230P2_INT_DAC		(1<<4)

#define PCI230_TEST_BIT(val, n)	((val>>n)&1)
			

enum {
	RES_Z2CT0,		
	RES_Z2CT1,		
	RES_Z2CT2,		
	NUM_RESOURCES		
};

enum {
	OWNER_NONE,		
	OWNER_AICMD,		
	OWNER_AOCMD		
};


#define COMBINE(old, new, mask)	(((old) & ~(mask)) | ((new) & (mask)))

#define NULLFUNC	0

#define THISCPU		smp_processor_id()

#define AI_CMD_STARTED	0
#define AO_CMD_STARTED	1


struct pci230_board {
	const char *name;
	unsigned short id;
	int ai_chans;
	int ai_bits;
	int ao_chans;
	int ao_bits;
	int have_dio;
	unsigned int min_hwver;	
};
static const struct pci230_board pci230_boards[] = {
	{
	 .name = "pci230+",
	 .id = PCI_DEVICE_ID_PCI230,
	 .ai_chans = 16,
	 .ai_bits = 16,
	 .ao_chans = 2,
	 .ao_bits = 12,
	 .have_dio = 1,
	 .min_hwver = 1,
	 },
	{
	 .name = "pci260+",
	 .id = PCI_DEVICE_ID_PCI260,
	 .ai_chans = 16,
	 .ai_bits = 16,
	 .ao_chans = 0,
	 .ao_bits = 0,
	 .have_dio = 0,
	 .min_hwver = 1,
	 },
	{
	 .name = "pci230",
	 .id = PCI_DEVICE_ID_PCI230,
	 .ai_chans = 16,
	 .ai_bits = 12,
	 .ao_chans = 2,
	 .ao_bits = 12,
	 .have_dio = 1,
	 },
	{
	 .name = "pci260",
	 .id = PCI_DEVICE_ID_PCI260,
	 .ai_chans = 16,
	 .ai_bits = 12,
	 .ao_chans = 0,
	 .ao_bits = 0,
	 .have_dio = 0,
	 },
	{
	 .name = "amplc_pci230",	
	 .id = PCI_DEVICE_ID_INVALID,
	 },
};

static DEFINE_PCI_DEVICE_TABLE(pci230_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMPLICON, PCI_DEVICE_ID_PCI230) },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMPLICON, PCI_DEVICE_ID_PCI260) },
	{0}
};

MODULE_DEVICE_TABLE(pci, pci230_pci_table);
#define n_pci230_boards ARRAY_SIZE(pci230_boards)
#define thisboard ((const struct pci230_board *)dev->board_ptr)

struct pci230_private {
	struct pci_dev *pci_dev;
	spinlock_t isr_spinlock;	
	spinlock_t res_spinlock;	
	spinlock_t ai_stop_spinlock;	
	spinlock_t ao_stop_spinlock;	
	unsigned long state;	
	unsigned long iobase1;	
	unsigned int ao_readback[2];	
	unsigned int ai_scan_count;	
	unsigned int ai_scan_pos;	
	unsigned int ao_scan_count;	
	int intr_cpuid;		
	unsigned short hwver;	
	unsigned short adccon;	
	unsigned short daccon;	
	unsigned short adcfifothresh;	
	unsigned short adcg;	
	unsigned char int_en;	
	unsigned char ai_continuous;	
	unsigned char ao_continuous;	
	unsigned char ai_bipolar;	
	unsigned char ao_bipolar;	
	unsigned char ier;	
	unsigned char intr_running;	
	unsigned char res_owner[NUM_RESOURCES];	
};

#define devpriv ((struct pci230_private *)dev->private)

static const unsigned int pci230_timebase[8] = {
	[CLK_10MHZ] = TIMEBASE_10MHZ,
	[CLK_1MHZ] = TIMEBASE_1MHZ,
	[CLK_100KHZ] = TIMEBASE_100KHZ,
	[CLK_10KHZ] = TIMEBASE_10KHZ,
	[CLK_1KHZ] = TIMEBASE_1KHZ,
};

static const struct comedi_lrange pci230_ai_range = { 7, {
							  BIP_RANGE(10),
							  BIP_RANGE(5),
							  BIP_RANGE(2.5),
							  BIP_RANGE(1.25),
							  UNI_RANGE(10),
							  UNI_RANGE(5),
							  UNI_RANGE(2.5)
							  }
};

static const unsigned char pci230_ai_gain[7] = { 0, 1, 2, 3, 1, 2, 3 };

static const unsigned char pci230_ai_bipolar[7] = { 1, 1, 1, 1, 0, 0, 0 };

static const struct comedi_lrange pci230_ao_range = { 2, {
							  UNI_RANGE(10),
							  BIP_RANGE(10)
							  }
};

static const unsigned char pci230_ao_bipolar[2] = { 0, 1 };

static int pci230_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int pci230_detach(struct comedi_device *dev);
static struct comedi_driver driver_amplc_pci230 = {
	.driver_name = "amplc_pci230",
	.module = THIS_MODULE,
	.attach = pci230_attach,
	.detach = pci230_detach,
	.board_name = &pci230_boards[0].name,
	.offset = sizeof(pci230_boards[0]),
	.num_names = ARRAY_SIZE(pci230_boards),
};

static int __devinit driver_amplc_pci230_pci_probe(struct pci_dev *dev,
						   const struct pci_device_id
						   *ent)
{
	return comedi_pci_auto_config(dev, driver_amplc_pci230.driver_name);
}

static void __devexit driver_amplc_pci230_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_amplc_pci230_pci_driver = {
	.id_table = pci230_pci_table,
	.probe = &driver_amplc_pci230_pci_probe,
	.remove = __devexit_p(&driver_amplc_pci230_pci_remove)
};

static int __init driver_amplc_pci230_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_amplc_pci230);
	if (retval < 0)
		return retval;

	driver_amplc_pci230_pci_driver.name =
	    (char *)driver_amplc_pci230.driver_name;
	return pci_register_driver(&driver_amplc_pci230_pci_driver);
}

static void __exit driver_amplc_pci230_cleanup_module(void)
{
	pci_unregister_driver(&driver_amplc_pci230_pci_driver);
	comedi_driver_unregister(&driver_amplc_pci230);
}

module_init(driver_amplc_pci230_init_module);
module_exit(driver_amplc_pci230_cleanup_module);

static int pci230_ai_rinsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data);
static int pci230_ao_winsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data);
static int pci230_ao_rinsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data);
static void pci230_ct_setup_ns_mode(struct comedi_device *dev, unsigned int ct,
				    unsigned int mode, uint64_t ns,
				    unsigned int round);
static void pci230_ns_to_single_timer(unsigned int *ns, unsigned int round);
static void pci230_cancel_ct(struct comedi_device *dev, unsigned int ct);
static irqreturn_t pci230_interrupt(int irq, void *d);
static int pci230_ao_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_cmd *cmd);
static int pci230_ao_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int pci230_ao_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s);
static void pci230_ao_stop(struct comedi_device *dev,
			   struct comedi_subdevice *s);
static void pci230_handle_ao_nofifo(struct comedi_device *dev,
				    struct comedi_subdevice *s);
static int pci230_handle_ao_fifo(struct comedi_device *dev,
				 struct comedi_subdevice *s);
static int pci230_ai_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_cmd *cmd);
static int pci230_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int pci230_ai_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s);
static void pci230_ai_stop(struct comedi_device *dev,
			   struct comedi_subdevice *s);
static void pci230_handle_ai(struct comedi_device *dev,
			     struct comedi_subdevice *s);

static short pci230_ai_read(struct comedi_device *dev)
{
	
	short data = (short)inw(dev->iobase + PCI230_ADCDATA);

	
	data = data >> (16 - thisboard->ai_bits);

	if (devpriv->ai_bipolar)
		data ^= 1 << (thisboard->ai_bits - 1);

	return data;
}

static inline unsigned short pci230_ao_mangle_datum(struct comedi_device *dev,
						    short datum)
{
	if (devpriv->ao_bipolar)
		datum ^= 1 << (thisboard->ao_bits - 1);


	
	datum <<= (16 - thisboard->ao_bits);
	return (unsigned short)datum;
}

static inline void pci230_ao_write_nofifo(struct comedi_device *dev,
					  short datum, unsigned int chan)
{
	
	devpriv->ao_readback[chan] = datum;

	
	outw(pci230_ao_mangle_datum(dev, datum), dev->iobase + (((chan) == 0)
								? PCI230_DACOUT1
								:
								PCI230_DACOUT2));
}

static inline void pci230_ao_write_fifo(struct comedi_device *dev, short datum,
					unsigned int chan)
{
	
	devpriv->ao_readback[chan] = datum;

	
	outw(pci230_ao_mangle_datum(dev, datum),
	     dev->iobase + PCI230P2_DACDATA);
}

static int pci230_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	unsigned long iobase1, iobase2;
	
	struct pci_dev *pci_dev = NULL;
	int i = 0, irq_hdl, rc;

	printk("comedi%d: amplc_pci230: attach %s %d,%d\n", dev->minor,
	       thisboard->name, it->options[0], it->options[1]);

	if ((alloc_private(dev, sizeof(struct pci230_private))) < 0)
		return -ENOMEM;

	spin_lock_init(&devpriv->isr_spinlock);
	spin_lock_init(&devpriv->res_spinlock);
	spin_lock_init(&devpriv->ai_stop_spinlock);
	spin_lock_init(&devpriv->ao_stop_spinlock);
	
	for_each_pci_dev(pci_dev) {
		if (it->options[0] || it->options[1]) {
			
			if (it->options[0] != pci_dev->bus->number ||
			    it->options[1] != PCI_SLOT(pci_dev->devfn))
				continue;
		}
		if (pci_dev->vendor != PCI_VENDOR_ID_AMPLICON)
			continue;
		if (thisboard->id == PCI_DEVICE_ID_INVALID) {
			for (i = 0; i < n_pci230_boards; i++) {
				if (pci_dev->device == pci230_boards[i].id) {
					if (pci230_boards[i].min_hwver > 0) {
						if (pci_resource_len(pci_dev, 3)
						    < 32) {
							
							continue;
						}
					}
					
					dev->board_ptr = &pci230_boards[i];
					break;
				}
			}
			if (i < n_pci230_boards)
				break;
		} else {
			if (thisboard->id == pci_dev->device) {
				
				if (thisboard->min_hwver > 0) {
					if (pci_resource_len(pci_dev, 3) < 32) {
						
						continue;
					}
					break;
				} else {
					break;
				}
			}
		}
	}
	if (!pci_dev) {
		printk("comedi%d: No %s card found\n", dev->minor,
		       thisboard->name);
		return -EIO;
	}
	devpriv->pci_dev = pci_dev;

	dev->board_name = thisboard->name;

	
	if (comedi_pci_enable(pci_dev, "amplc_pci230") < 0) {
		printk("comedi%d: failed to enable PCI device "
		       "and request regions\n", dev->minor);
		return -EIO;
	}

	iobase1 = pci_resource_start(pci_dev, 2);
	iobase2 = pci_resource_start(pci_dev, 3);

	printk("comedi%d: %s I/O region 1 0x%04lx I/O region 2 0x%04lx\n",
	       dev->minor, dev->board_name, iobase1, iobase2);

	devpriv->iobase1 = iobase1;
	dev->iobase = iobase2;

	
	devpriv->daccon = inw(dev->iobase + PCI230_DACCON) & PCI230_DAC_OR_MASK;

	if (pci_resource_len(pci_dev, 3) >= 32) {
		unsigned short extfunc = 0;

		devpriv->hwver = inw(dev->iobase + PCI230P_HWVER);
		if (devpriv->hwver < thisboard->min_hwver) {
			printk("comedi%d: %s - bad hardware version "
			       "- got %u, need %u\n", dev->minor,
			       dev->board_name, devpriv->hwver,
			       thisboard->min_hwver);
			return -EIO;
		}
		if (devpriv->hwver > 0) {
			if (!thisboard->have_dio) {
				extfunc |= PCI230P_EXTFUNC_GAT_EXTTRIG;
			}
			if ((thisboard->ao_chans > 0)
			    && (devpriv->hwver >= 2)) {
				
				extfunc |= PCI230P2_EXTFUNC_DACFIFO;
			}
		}
		outw(extfunc, dev->iobase + PCI230P_EXTFUNC);
		if ((extfunc & PCI230P2_EXTFUNC_DACFIFO) != 0) {
			outw(devpriv->daccon | PCI230P2_DAC_FIFO_EN
			     | PCI230P2_DAC_FIFO_RESET,
			     dev->iobase + PCI230_DACCON);
			
			outw(0, dev->iobase + PCI230P2_DACEN);
			
			outw(devpriv->daccon, dev->iobase + PCI230_DACCON);
		}
	}

	
	outb(0, devpriv->iobase1 + PCI230_INT_SCE);

	
	devpriv->adcg = 0;
	devpriv->adccon = PCI230_ADC_TRIG_NONE | PCI230_ADC_IM_SE
	    | PCI230_ADC_IR_BIP;
	outw(1 << 0, dev->iobase + PCI230_ADCEN);
	outw(devpriv->adcg, dev->iobase + PCI230_ADCG);
	outw(devpriv->adccon | PCI230_ADC_FIFO_RESET,
	     dev->iobase + PCI230_ADCCON);

	
	irq_hdl = request_irq(devpriv->pci_dev->irq, pci230_interrupt,
			      IRQF_SHARED, "amplc_pci230", dev);
	if (irq_hdl < 0) {
		printk("comedi%d: unable to register irq, "
		       "commands will not be available %d\n", dev->minor,
		       devpriv->pci_dev->irq);
	} else {
		dev->irq = devpriv->pci_dev->irq;
		printk("comedi%d: registered irq %u\n", dev->minor,
		       devpriv->pci_dev->irq);
	}

	if (alloc_subdevices(dev, 3) < 0)
		return -ENOMEM;

	s = dev->subdevices + 0;
	
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_DIFF | SDF_GROUND;
	s->n_chan = thisboard->ai_chans;
	s->maxdata = (1 << thisboard->ai_bits) - 1;
	s->range_table = &pci230_ai_range;
	s->insn_read = &pci230_ai_rinsn;
	s->len_chanlist = 256;	
	
	if (irq_hdl == 0) {
		dev->read_subdev = s;
		s->subdev_flags |= SDF_CMD_READ;
		s->do_cmd = &pci230_ai_cmd;
		s->do_cmdtest = &pci230_ai_cmdtest;
		s->cancel = pci230_ai_cancel;
	}

	s = dev->subdevices + 1;
	
	if (thisboard->ao_chans > 0) {
		s->type = COMEDI_SUBD_AO;
		s->subdev_flags = SDF_WRITABLE | SDF_GROUND;
		s->n_chan = thisboard->ao_chans;
		s->maxdata = (1 << thisboard->ao_bits) - 1;
		s->range_table = &pci230_ao_range;
		s->insn_write = &pci230_ao_winsn;
		s->insn_read = &pci230_ao_rinsn;
		s->len_chanlist = thisboard->ao_chans;
		if (irq_hdl == 0) {
			dev->write_subdev = s;
			s->subdev_flags |= SDF_CMD_WRITE;
			s->do_cmd = &pci230_ao_cmd;
			s->do_cmdtest = &pci230_ao_cmdtest;
			s->cancel = pci230_ao_cancel;
		}
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	s = dev->subdevices + 2;
	
	if (thisboard->have_dio) {
		rc = subdev_8255_init(dev, s, NULL,
				      (devpriv->iobase1 + PCI230_PPI_X_BASE));
		if (rc < 0)
			return rc;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	printk("comedi%d: attached\n", dev->minor);

	return 1;
}

static int pci230_detach(struct comedi_device *dev)
{
	printk("comedi%d: amplc_pci230: remove\n", dev->minor);

	if (dev->subdevices && thisboard->have_dio)
		
		subdev_8255_cleanup(dev, dev->subdevices + 2);

	if (dev->irq)
		free_irq(dev->irq, dev);

	if (devpriv) {
		if (devpriv->pci_dev) {
			if (dev->iobase)
				comedi_pci_disable(devpriv->pci_dev);

			pci_dev_put(devpriv->pci_dev);
		}
	}

	return 0;
}

static int get_resources(struct comedi_device *dev, unsigned int res_mask,
			 unsigned char owner)
{
	int ok;
	unsigned int i;
	unsigned int b;
	unsigned int claimed;
	unsigned long irqflags;

	ok = 1;
	claimed = 0;
	spin_lock_irqsave(&devpriv->res_spinlock, irqflags);
	for (b = 1, i = 0; (i < NUM_RESOURCES)
	     && (res_mask != 0); b <<= 1, i++) {
		if ((res_mask & b) != 0) {
			res_mask &= ~b;
			if (devpriv->res_owner[i] == OWNER_NONE) {
				devpriv->res_owner[i] = owner;
				claimed |= b;
			} else if (devpriv->res_owner[i] != owner) {
				for (b = 1, i = 0; claimed != 0; b <<= 1, i++) {
					if ((claimed & b) != 0) {
						devpriv->res_owner[i]
						    = OWNER_NONE;
						claimed &= ~b;
					}
				}
				ok = 0;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&devpriv->res_spinlock, irqflags);
	return ok;
}

static inline int get_one_resource(struct comedi_device *dev,
				   unsigned int resource, unsigned char owner)
{
	return get_resources(dev, (1U << resource), owner);
}

static void put_resources(struct comedi_device *dev, unsigned int res_mask,
			  unsigned char owner)
{
	unsigned int i;
	unsigned int b;
	unsigned long irqflags;

	spin_lock_irqsave(&devpriv->res_spinlock, irqflags);
	for (b = 1, i = 0; (i < NUM_RESOURCES)
	     && (res_mask != 0); b <<= 1, i++) {
		if ((res_mask & b) != 0) {
			res_mask &= ~b;
			if (devpriv->res_owner[i] == owner)
				devpriv->res_owner[i] = OWNER_NONE;

		}
	}
	spin_unlock_irqrestore(&devpriv->res_spinlock, irqflags);
}

static inline void put_one_resource(struct comedi_device *dev,
				    unsigned int resource, unsigned char owner)
{
	put_resources(dev, (1U << resource), owner);
}

static inline void put_all_resources(struct comedi_device *dev,
				     unsigned char owner)
{
	put_resources(dev, (1U << NUM_RESOURCES) - 1, owner);
}

static int pci230_ai_rinsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data)
{
	unsigned int n, i;
	unsigned int chan, range, aref;
	unsigned int gainshift;
	unsigned int status;
	unsigned short adccon, adcen;

	
	chan = CR_CHAN(insn->chanspec);
	range = CR_RANGE(insn->chanspec);
	aref = CR_AREF(insn->chanspec);
	if (aref == AREF_DIFF) {
		
		if (chan >= s->n_chan / 2) {
			DPRINTK("comedi%d: amplc_pci230: ai_rinsn: "
				"differential channel number out of range "
				"0 to %u\n", dev->minor, (s->n_chan / 2) - 1);
			return -EINVAL;
		}
	}

	adccon = PCI230_ADC_TRIG_Z2CT2 | PCI230_ADC_FIFO_EN;
	
	i8254_set_mode(devpriv->iobase1 + PCI230_Z2_CT_BASE, 0, 2, I8254_MODE0);
	devpriv->ai_bipolar = pci230_ai_bipolar[range];
	if (aref == AREF_DIFF) {
		
		gainshift = chan * 2;
		if (devpriv->hwver == 0) {
			adcen = 3 << gainshift;
		} else {
			adcen = 1 << gainshift;
		}
		adccon |= PCI230_ADC_IM_DIF;
	} else {
		
		adcen = 1 << chan;
		gainshift = chan & ~1;
		adccon |= PCI230_ADC_IM_SE;
	}
	devpriv->adcg = (devpriv->adcg & ~(3 << gainshift))
	    | (pci230_ai_gain[range] << gainshift);
	if (devpriv->ai_bipolar)
		adccon |= PCI230_ADC_IR_BIP;
	else
		adccon |= PCI230_ADC_IR_UNI;


	outw(adcen, dev->iobase + PCI230_ADCEN);

	
	outw(devpriv->adcg, dev->iobase + PCI230_ADCG);

	
	devpriv->adccon = adccon;
	outw(adccon | PCI230_ADC_FIFO_RESET, dev->iobase + PCI230_ADCCON);

	
	for (n = 0; n < insn->n; n++) {
		i8254_set_mode(devpriv->iobase1 + PCI230_Z2_CT_BASE, 0, 2,
			       I8254_MODE0);
		i8254_set_mode(devpriv->iobase1 + PCI230_Z2_CT_BASE, 0, 2,
			       I8254_MODE1);

#define TIMEOUT 100
		
		for (i = 0; i < TIMEOUT; i++) {
			status = inw(dev->iobase + PCI230_ADCCON);
			if (!(status & PCI230_ADC_FIFO_EMPTY))
				break;
			udelay(1);
		}
		if (i == TIMEOUT) {
			printk("timeout\n");
			return -ETIMEDOUT;
		}

		
		data[n] = pci230_ai_read(dev);
	}

	/* return the number of samples read/written */
	return n;
}

static int pci230_ao_winsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data)
{
	int i;
	int chan, range;

	
	chan = CR_CHAN(insn->chanspec);
	range = CR_RANGE(insn->chanspec);

	devpriv->ao_bipolar = pci230_ao_bipolar[range];
	outw(range, dev->iobase + PCI230_DACCON);

	for (i = 0; i < insn->n; i++) {
		
		pci230_ao_write_nofifo(dev, data[i], chan);
	}

	/* return the number of samples read/written */
	return i;
}

static int pci230_ao_rinsn(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_insn *insn,
			   unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_readback[chan];

	return i;
}

static int pci230_ao_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	unsigned int tmp;



	tmp = cmd->start_src;
	cmd->start_src &= TRIG_INT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	if ((thisboard->min_hwver > 0) && (devpriv->hwver >= 2)) {
		cmd->scan_begin_src &= TRIG_TIMER | TRIG_INT | TRIG_EXT;
	} else {
		cmd->scan_begin_src &= TRIG_TIMER | TRIG_INT;
	}
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
#define MAX_SPEED_AO	8000	
#define MIN_SPEED_AO	4294967295u	

	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:
		if (cmd->scan_begin_arg < MAX_SPEED_AO) {
			cmd->scan_begin_arg = MAX_SPEED_AO;
			err++;
		}
		if (cmd->scan_begin_arg > MIN_SPEED_AO) {
			cmd->scan_begin_arg = MIN_SPEED_AO;
			err++;
		}
		break;
	case TRIG_EXT:
		
		
		if ((cmd->scan_begin_arg & ~CR_FLAGS_MASK) != 0) {
			cmd->scan_begin_arg = COMBINE(cmd->scan_begin_arg, 0,
						      ~CR_FLAGS_MASK);
			err++;
		}
		if ((cmd->scan_begin_arg
		     & (CR_FLAGS_MASK & ~(CR_EDGE | CR_INVERT))) != 0) {
			cmd->scan_begin_arg =
			    COMBINE(cmd->scan_begin_arg, 0,
				    CR_FLAGS_MASK & ~(CR_EDGE | CR_INVERT));
			err++;
		}
		break;
	default:
		if (cmd->scan_begin_arg != 0) {
			cmd->scan_begin_arg = 0;
			err++;
		}
		break;
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
	}

	if (err)
		return 3;


	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		pci230_ns_to_single_timer(&cmd->scan_begin_arg,
					  cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->scan_begin_arg)
			err++;
	}

	if (err)
		return 4;

	

	if (cmd->chanlist && cmd->chanlist_len > 0) {
		enum {
			seq_err = (1 << 0),
			range_err = (1 << 1)
		};
		unsigned int errors;
		unsigned int n;
		unsigned int chan, prev_chan;
		unsigned int range, first_range;

		prev_chan = CR_CHAN(cmd->chanlist[0]);
		first_range = CR_RANGE(cmd->chanlist[0]);
		errors = 0;
		for (n = 1; n < cmd->chanlist_len; n++) {
			chan = CR_CHAN(cmd->chanlist[n]);
			range = CR_RANGE(cmd->chanlist[n]);
			
			if (chan < prev_chan)
				errors |= seq_err;

			
			if (range != first_range)
				errors |= range_err;

			prev_chan = chan;
		}
		if (errors != 0) {
			err++;
			if ((errors & seq_err) != 0) {
				DPRINTK("comedi%d: amplc_pci230: ao_cmdtest: "
					"channel numbers must increase\n",
					dev->minor);
			}
			if ((errors & range_err) != 0) {
				DPRINTK("comedi%d: amplc_pci230: ao_cmdtest: "
					"channels must have the same range\n",
					dev->minor);
			}
		}
	}

	if (err)
		return 5;

	return 0;
}

static int pci230_ao_inttrig_scan_begin(struct comedi_device *dev,
					struct comedi_subdevice *s,
					unsigned int trig_num)
{
	unsigned long irqflags;

	if (trig_num != 0)
		return -EINVAL;

	spin_lock_irqsave(&devpriv->ao_stop_spinlock, irqflags);
	if (test_bit(AO_CMD_STARTED, &devpriv->state)) {
		
		if (devpriv->hwver < 2) {
			
			spin_unlock_irqrestore(&devpriv->ao_stop_spinlock,
					       irqflags);
			pci230_handle_ao_nofifo(dev, s);
			comedi_event(dev, s);
		} else {
			
			
			inw(dev->iobase + PCI230P2_DACSWTRIG);
			spin_unlock_irqrestore(&devpriv->ao_stop_spinlock,
					       irqflags);
		}
		
		
		udelay(8);
	}

	return 1;
}

static void pci230_ao_start(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned long irqflags;

	set_bit(AO_CMD_STARTED, &devpriv->state);
	if (!devpriv->ao_continuous && (devpriv->ao_scan_count == 0)) {
		
		async->events |= COMEDI_CB_EOA;
		pci230_ao_stop(dev, s);
		comedi_event(dev, s);
	} else {
		if (devpriv->hwver >= 2) {
			
			unsigned short scantrig;
			int run;

			
			run = pci230_handle_ao_fifo(dev, s);
			comedi_event(dev, s);
			if (!run) {
				
				return;
			}
			
			switch (cmd->scan_begin_src) {
			case TRIG_TIMER:
				scantrig = PCI230P2_DAC_TRIG_Z2CT1;
				break;
			case TRIG_EXT:
				
				if ((cmd->scan_begin_arg & CR_INVERT) == 0) {
					
					scantrig = PCI230P2_DAC_TRIG_EXTP;
				} else {
					
					scantrig = PCI230P2_DAC_TRIG_EXTN;
				}
				break;
			case TRIG_INT:
				scantrig = PCI230P2_DAC_TRIG_SW;
				break;
			default:
				
				scantrig = PCI230P2_DAC_TRIG_NONE;
				break;
			}
			devpriv->daccon = (devpriv->daccon
					   & ~PCI230P2_DAC_TRIG_MASK) |
			    scantrig;
			outw(devpriv->daccon, dev->iobase + PCI230_DACCON);

		}
		switch (cmd->scan_begin_src) {
		case TRIG_TIMER:
			if (devpriv->hwver < 2) {
				
				
				spin_lock_irqsave(&devpriv->isr_spinlock,
						  irqflags);
				devpriv->int_en |= PCI230_INT_ZCLK_CT1;
				devpriv->ier |= PCI230_INT_ZCLK_CT1;
				outb(devpriv->ier,
				     devpriv->iobase1 + PCI230_INT_SCE);
				spin_unlock_irqrestore(&devpriv->isr_spinlock,
						       irqflags);
			}
			
			outb(GAT_CONFIG(1, GAT_VCC),
			     devpriv->iobase1 + PCI230_ZGAT_SCE);
			break;
		case TRIG_INT:
			async->inttrig = pci230_ao_inttrig_scan_begin;
			break;
		}
		if (devpriv->hwver >= 2) {
			
			spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
			devpriv->int_en |= PCI230P2_INT_DAC;
			devpriv->ier |= PCI230P2_INT_DAC;
			outb(devpriv->ier, devpriv->iobase1 + PCI230_INT_SCE);
			spin_unlock_irqrestore(&devpriv->isr_spinlock,
					       irqflags);
		}
	}
}

static int pci230_ao_inttrig_start(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   unsigned int trig_num)
{
	if (trig_num != 0)
		return -EINVAL;

	s->async->inttrig = NULLFUNC;
	pci230_ao_start(dev, s);

	return 1;
}

static int pci230_ao_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	unsigned short daccon;
	unsigned int range;

	
	struct comedi_cmd *cmd = &s->async->cmd;

	if (cmd->scan_begin_src == TRIG_TIMER) {
		
		if (!get_one_resource(dev, RES_Z2CT1, OWNER_AOCMD))
			return -EBUSY;

	}

	
	if (cmd->stop_src == TRIG_COUNT) {
		devpriv->ao_scan_count = cmd->stop_arg;
		devpriv->ao_continuous = 0;
	} else {
		
		devpriv->ao_scan_count = 0;
		devpriv->ao_continuous = 1;
	}

	range = CR_RANGE(cmd->chanlist[0]);
	devpriv->ao_bipolar = pci230_ao_bipolar[range];
	daccon = devpriv->ao_bipolar ? PCI230_DAC_OR_BIP : PCI230_DAC_OR_UNI;
	
	if (devpriv->hwver >= 2) {
		unsigned short dacen;
		unsigned int i;

		dacen = 0;
		for (i = 0; i < cmd->chanlist_len; i++)
			dacen |= 1 << CR_CHAN(cmd->chanlist[i]);

		
		outw(dacen, dev->iobase + PCI230P2_DACEN);
		daccon |= PCI230P2_DAC_FIFO_EN | PCI230P2_DAC_FIFO_RESET
		    | PCI230P2_DAC_FIFO_UNDERRUN_CLEAR
		    | PCI230P2_DAC_TRIG_NONE | PCI230P2_DAC_INT_FIFO_NHALF;
	}

	
	outw(daccon, dev->iobase + PCI230_DACCON);
	
	devpriv->daccon = daccon
	    & ~(PCI230P2_DAC_FIFO_RESET | PCI230P2_DAC_FIFO_UNDERRUN_CLEAR);

	if (cmd->scan_begin_src == TRIG_TIMER) {
		
		
		
		outb(GAT_CONFIG(1, GAT_GND),
		     devpriv->iobase1 + PCI230_ZGAT_SCE);
		pci230_ct_setup_ns_mode(dev, 1, I8254_MODE3,
					cmd->scan_begin_arg,
					cmd->flags & TRIG_ROUND_MASK);
	}

	
	s->async->inttrig = pci230_ao_inttrig_start;

	return 0;
}

static int pci230_ai_check_scan_period(struct comedi_cmd *cmd)
{
	unsigned int min_scan_period, chanlist_len;
	int err = 0;

	chanlist_len = cmd->chanlist_len;
	if (cmd->chanlist_len == 0)
		chanlist_len = 1;

	min_scan_period = chanlist_len * cmd->convert_arg;
	if ((min_scan_period < chanlist_len)
	    || (min_scan_period < cmd->convert_arg)) {
		
		min_scan_period = UINT_MAX;
		err++;
	}
	if (cmd->scan_begin_arg < min_scan_period) {
		cmd->scan_begin_arg = min_scan_period;
		err++;
	}

	return !err;
}

static int pci230_ai_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	unsigned int tmp;



	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_INT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	if ((thisboard->have_dio) || (thisboard->min_hwver > 0)) {
		cmd->scan_begin_src &= TRIG_FOLLOW | TRIG_TIMER | TRIG_INT
		    | TRIG_EXT;
	} else {
		cmd->scan_begin_src &= TRIG_FOLLOW | TRIG_TIMER | TRIG_INT;
	}
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_TIMER | TRIG_INT | TRIG_EXT;
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

	if ((cmd->scan_begin_src != TRIG_FOLLOW)
	    && (cmd->convert_src != TRIG_TIMER))
		err++;

	if (err)
		return 2;


	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}
#define MAX_SPEED_AI_SE		3200	
#define MAX_SPEED_AI_DIFF	8000	
#define MAX_SPEED_AI_PLUS	4000	
#define MIN_SPEED_AI	4294967295u	

	if (cmd->convert_src == TRIG_TIMER) {
		unsigned int max_speed_ai;

		if (devpriv->hwver == 0) {
			if (cmd->chanlist && (cmd->chanlist_len > 0)) {
				
				if (CR_AREF(cmd->chanlist[0]) == AREF_DIFF)
					max_speed_ai = MAX_SPEED_AI_DIFF;
				else
					max_speed_ai = MAX_SPEED_AI_SE;

			} else {
				
				max_speed_ai = MAX_SPEED_AI_SE;
			}
		} else {
			
			max_speed_ai = MAX_SPEED_AI_PLUS;
		}

		if (cmd->convert_arg < max_speed_ai) {
			cmd->convert_arg = max_speed_ai;
			err++;
		}
		if (cmd->convert_arg > MIN_SPEED_AI) {
			cmd->convert_arg = MIN_SPEED_AI;
			err++;
		}
	} else if (cmd->convert_src == TRIG_EXT) {
		if ((cmd->convert_arg & CR_FLAGS_MASK) != 0) {
			
			if ((cmd->convert_arg & ~CR_FLAGS_MASK) != 0) {
				cmd->convert_arg = COMBINE(cmd->convert_arg, 0,
							   ~CR_FLAGS_MASK);
				err++;
			}
			if ((cmd->convert_arg & (CR_FLAGS_MASK & ~CR_INVERT))
			    != CR_EDGE) {
				
				cmd->convert_arg =
				    COMBINE(cmd->start_arg, (CR_EDGE | 0),
					    CR_FLAGS_MASK & ~CR_INVERT);
				err++;
			}
		} else {
			
			
			
			if (cmd->convert_arg > 1) {
				
				cmd->convert_arg = 1;
				err++;
			}
		}
	} else {
		if (cmd->convert_arg != 0) {
			cmd->convert_arg = 0;
			err++;
		}
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
	}

	if (cmd->scan_begin_src == TRIG_EXT) {
		if ((cmd->scan_begin_arg & ~CR_FLAGS_MASK) != 0) {
			cmd->scan_begin_arg = COMBINE(cmd->scan_begin_arg, 0,
						      ~CR_FLAGS_MASK);
			err++;
		}
		
		if ((cmd->scan_begin_arg & CR_FLAGS_MASK & ~CR_EDGE) != 0) {
			cmd->scan_begin_arg = COMBINE(cmd->scan_begin_arg, 0,
						      CR_FLAGS_MASK & ~CR_EDGE);
			err++;
		}
	} else if (cmd->scan_begin_src == TRIG_TIMER) {
		
		if (!pci230_ai_check_scan_period(cmd))
			err++;

	} else {
		if (cmd->scan_begin_arg != 0) {
			cmd->scan_begin_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;


	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		pci230_ns_to_single_timer(&cmd->convert_arg,
					  cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->convert_arg)
			err++;
	}

	if (cmd->scan_begin_src == TRIG_TIMER) {
		
		tmp = cmd->scan_begin_arg;
		pci230_ns_to_single_timer(&cmd->scan_begin_arg,
					  cmd->flags & TRIG_ROUND_MASK);
		if (!pci230_ai_check_scan_period(cmd)) {
			
			pci230_ns_to_single_timer(&cmd->scan_begin_arg,
						  TRIG_ROUND_UP);
			pci230_ai_check_scan_period(cmd);
		}
		if (tmp != cmd->scan_begin_arg)
			err++;
	}

	if (err)
		return 4;

	

	if (cmd->chanlist && cmd->chanlist_len > 0) {
		enum {
			seq_err = 1 << 0,
			rangepair_err = 1 << 1,
			polarity_err = 1 << 2,
			aref_err = 1 << 3,
			diffchan_err = 1 << 4,
			buggy_chan0_err = 1 << 5
		};
		unsigned int errors;
		unsigned int chan, prev_chan;
		unsigned int range, prev_range;
		unsigned int polarity, prev_polarity;
		unsigned int aref, prev_aref;
		unsigned int subseq_len;
		unsigned int n;

		subseq_len = 0;
		errors = 0;
		prev_chan = prev_aref = prev_range = prev_polarity = 0;
		for (n = 0; n < cmd->chanlist_len; n++) {
			chan = CR_CHAN(cmd->chanlist[n]);
			range = CR_RANGE(cmd->chanlist[n]);
			aref = CR_AREF(cmd->chanlist[n]);
			polarity = pci230_ai_bipolar[range];
			if ((aref == AREF_DIFF)
			    && (chan >= (s->n_chan / 2))) {
				errors |= diffchan_err;
			}
			if (n > 0) {
				if ((chan <= prev_chan)
				    && (subseq_len == 0)) {
					subseq_len = n;
				}
				if ((subseq_len > 0)
				    && (cmd->chanlist[n] !=
					cmd->chanlist[n % subseq_len])) {
					errors |= seq_err;
				}
				
				if (aref != prev_aref)
					errors |= aref_err;

				
				if (polarity != prev_polarity)
					errors |= polarity_err;

				if ((aref != AREF_DIFF)
				    && (((chan ^ prev_chan) & ~1) == 0)
				    && (range != prev_range)) {
					errors |= rangepair_err;
				}
			}
			prev_chan = chan;
			prev_range = range;
			prev_aref = aref;
			prev_polarity = polarity;
		}
		if (subseq_len == 0) {
			
			subseq_len = n;
		}
		if ((n % subseq_len) != 0)
			errors |= seq_err;

		if ((devpriv->hwver > 0) && (devpriv->hwver < 4)) {
			if ((subseq_len > 1)
			    && (CR_CHAN(cmd->chanlist[0]) != 0)) {
				errors |= buggy_chan0_err;
			}
		}
		if (errors != 0) {
			err++;
			if ((errors & seq_err) != 0) {
				DPRINTK("comedi%d: amplc_pci230: ai_cmdtest: "
					"channel numbers must increase or "
					"sequence must repeat exactly\n",
					dev->minor);
			}
			if ((errors & rangepair_err) != 0) {
				DPRINTK("comedi%d: amplc_pci230: ai_cmdtest: "
					"single-ended channel pairs must "
					"have the same range\n", dev->minor);
			}
			if ((errors & polarity_err) != 0) {
				DPRINTK("comedi%d: amplc_pci230: ai_cmdtest: "
					"channel sequence ranges must be all "
					"bipolar or all unipolar\n",
					dev->minor);
			}
			if ((errors & aref_err) != 0) {
				DPRINTK("comedi%d: amplc_pci230: ai_cmdtest: "
					"channel sequence analogue references "
					"must be all the same (single-ended "
					"or differential)\n", dev->minor);
			}
			if ((errors & diffchan_err) != 0) {
				DPRINTK("comedi%d: amplc_pci230: ai_cmdtest: "
					"differential channel number out of "
					"range 0 to %u\n", dev->minor,
					(s->n_chan / 2) - 1);
			}
			if ((errors & buggy_chan0_err) != 0) {
				
				printk("comedi: comedi%d: amplc_pci230: "
				       "ai_cmdtest: Buggy PCI230+/260+ "
				       "h/w version %u requires first channel "
				       "of multi-channel sequence to be 0 "
				       "(corrected in h/w version 4)\n",
				       dev->minor, devpriv->hwver);
			}
		}
	}

	if (err)
		return 5;

	return 0;
}

static void pci230_ai_update_fifo_trigger_level(struct comedi_device *dev,
						struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int scanlen = cmd->scan_end_arg;
	unsigned int wake;
	unsigned short triglev;
	unsigned short adccon;

	if ((cmd->flags & TRIG_WAKE_EOS) != 0) {
		
		wake = scanlen - devpriv->ai_scan_pos;
	} else {
		if (devpriv->ai_continuous
		    || (devpriv->ai_scan_count >= PCI230_ADC_FIFOLEVEL_HALFFULL)
		    || (scanlen >= PCI230_ADC_FIFOLEVEL_HALFFULL)) {
			wake = PCI230_ADC_FIFOLEVEL_HALFFULL;
		} else {
			wake = (devpriv->ai_scan_count * scanlen)
			    - devpriv->ai_scan_pos;
		}
	}
	if (wake >= PCI230_ADC_FIFOLEVEL_HALFFULL) {
		triglev = PCI230_ADC_INT_FIFO_HALF;
	} else {
		if ((wake > 1) && (devpriv->hwver > 0)) {
			
			if (devpriv->adcfifothresh != wake) {
				devpriv->adcfifothresh = wake;
				outw(wake, dev->iobase + PCI230P_ADCFFTH);
			}
			triglev = PCI230P_ADC_INT_FIFO_THRESH;
		} else {
			triglev = PCI230_ADC_INT_FIFO_NEMPTY;
		}
	}
	adccon = (devpriv->adccon & ~PCI230_ADC_INT_FIFO_MASK) | triglev;
	if (adccon != devpriv->adccon) {
		devpriv->adccon = adccon;
		outw(adccon, dev->iobase + PCI230_ADCCON);
	}
}

static int pci230_ai_inttrig_convert(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     unsigned int trig_num)
{
	unsigned long irqflags;

	if (trig_num != 0)
		return -EINVAL;

	spin_lock_irqsave(&devpriv->ai_stop_spinlock, irqflags);
	if (test_bit(AI_CMD_STARTED, &devpriv->state)) {
		unsigned int delayus;

		i8254_set_mode(devpriv->iobase1 + PCI230_Z2_CT_BASE, 0, 2,
			       I8254_MODE0);
		i8254_set_mode(devpriv->iobase1 + PCI230_Z2_CT_BASE, 0, 2,
			       I8254_MODE1);
		if (((devpriv->adccon & PCI230_ADC_IM_MASK)
		     == PCI230_ADC_IM_DIF)
		    && (devpriv->hwver == 0)) {
			
			delayus = 8;
		} else {
			
			delayus = 4;
		}
		spin_unlock_irqrestore(&devpriv->ai_stop_spinlock, irqflags);
		udelay(delayus);
	} else {
		spin_unlock_irqrestore(&devpriv->ai_stop_spinlock, irqflags);
	}

	return 1;
}

static int pci230_ai_inttrig_scan_begin(struct comedi_device *dev,
					struct comedi_subdevice *s,
					unsigned int trig_num)
{
	unsigned long irqflags;
	unsigned char zgat;

	if (trig_num != 0)
		return -EINVAL;

	spin_lock_irqsave(&devpriv->ai_stop_spinlock, irqflags);
	if (test_bit(AI_CMD_STARTED, &devpriv->state)) {
		
		zgat = GAT_CONFIG(0, GAT_GND);
		outb(zgat, devpriv->iobase1 + PCI230_ZGAT_SCE);
		zgat = GAT_CONFIG(0, GAT_VCC);
		outb(zgat, devpriv->iobase1 + PCI230_ZGAT_SCE);
	}
	spin_unlock_irqrestore(&devpriv->ai_stop_spinlock, irqflags);

	return 1;
}

static void pci230_ai_start(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	unsigned long irqflags;
	unsigned short conv;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;

	set_bit(AI_CMD_STARTED, &devpriv->state);
	if (!devpriv->ai_continuous && (devpriv->ai_scan_count == 0)) {
		
		async->events |= COMEDI_CB_EOA;
		pci230_ai_stop(dev, s);
		comedi_event(dev, s);
	} else {
		
		spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
		devpriv->int_en |= PCI230_INT_ADC;
		devpriv->ier |= PCI230_INT_ADC;
		outb(devpriv->ier, devpriv->iobase1 + PCI230_INT_SCE);
		spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);

		switch (cmd->convert_src) {
		default:
			conv = PCI230_ADC_TRIG_NONE;
			break;
		case TRIG_TIMER:
			
			conv = PCI230_ADC_TRIG_Z2CT2;
			break;
		case TRIG_EXT:
			if ((cmd->convert_arg & CR_EDGE) != 0) {
				if ((cmd->convert_arg & CR_INVERT) == 0) {
					
					conv = PCI230_ADC_TRIG_EXTP;
				} else {
					
					conv = PCI230_ADC_TRIG_EXTN;
				}
			} else {
				
				if (cmd->convert_arg != 0) {
					
					conv = PCI230_ADC_TRIG_EXTP;
				} else {
					
					conv = PCI230_ADC_TRIG_EXTN;
				}
			}
			break;
		case TRIG_INT:
			conv = PCI230_ADC_TRIG_Z2CT2;
			break;
		}
		devpriv->adccon = (devpriv->adccon & ~PCI230_ADC_TRIG_MASK)
		    | conv;
		outw(devpriv->adccon, dev->iobase + PCI230_ADCCON);
		if (cmd->convert_src == TRIG_INT)
			async->inttrig = pci230_ai_inttrig_convert;

		pci230_ai_update_fifo_trigger_level(dev, s);
		if (cmd->convert_src == TRIG_TIMER) {
			
			unsigned char zgat;

			if (cmd->scan_begin_src != TRIG_FOLLOW) {
				zgat = GAT_CONFIG(2, GAT_NOUTNM2);
			} else {
				zgat = GAT_CONFIG(2, GAT_VCC);
			}
			outb(zgat, devpriv->iobase1 + PCI230_ZGAT_SCE);
			if (cmd->scan_begin_src != TRIG_FOLLOW) {
				
				switch (cmd->scan_begin_src) {
				default:
					zgat = GAT_CONFIG(0, GAT_VCC);
					break;
				case TRIG_EXT:
					zgat = GAT_CONFIG(0, GAT_EXT);
					break;
				case TRIG_TIMER:
					zgat = GAT_CONFIG(0, GAT_NOUTNM2);
					break;
				case TRIG_INT:
					zgat = GAT_CONFIG(0, GAT_VCC);
					break;
				}
				outb(zgat, devpriv->iobase1 + PCI230_ZGAT_SCE);
				switch (cmd->scan_begin_src) {
				case TRIG_TIMER:
					zgat = GAT_CONFIG(1, GAT_VCC);
					outb(zgat, devpriv->iobase1
					     + PCI230_ZGAT_SCE);
					break;
				case TRIG_INT:
					async->inttrig =
					    pci230_ai_inttrig_scan_begin;
					break;
				}
			}
		} else if (cmd->convert_src != TRIG_INT) {
			
			put_one_resource(dev, RES_Z2CT2, OWNER_AICMD);
		}
	}
}

static int pci230_ai_inttrig_start(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   unsigned int trig_num)
{
	if (trig_num != 0)
		return -EINVAL;

	s->async->inttrig = NULLFUNC;
	pci230_ai_start(dev, s);

	return 1;
}

static int pci230_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	unsigned int i, chan, range, diff;
	unsigned int res_mask;
	unsigned short adccon, adcen;
	unsigned char zgat;

	
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;

	res_mask = 0;
	res_mask |= (1U << RES_Z2CT2);
	if (cmd->scan_begin_src != TRIG_FOLLOW) {
		
		res_mask |= (1U << RES_Z2CT0);
		if (cmd->scan_begin_src == TRIG_TIMER) {
			
			res_mask |= (1U << RES_Z2CT1);
		}
	}
	
	if (!get_resources(dev, res_mask, OWNER_AICMD))
		return -EBUSY;


	
	if (cmd->stop_src == TRIG_COUNT) {
		devpriv->ai_scan_count = cmd->stop_arg;
		devpriv->ai_continuous = 0;
	} else {
		
		devpriv->ai_scan_count = 0;
		devpriv->ai_continuous = 1;
	}
	devpriv->ai_scan_pos = 0;	


	adccon = PCI230_ADC_FIFO_EN;
	adcen = 0;

	if (CR_AREF(cmd->chanlist[0]) == AREF_DIFF) {
		
		diff = 1;
		adccon |= PCI230_ADC_IM_DIF;
	} else {
		
		diff = 0;
		adccon |= PCI230_ADC_IM_SE;
	}

	range = CR_RANGE(cmd->chanlist[0]);
	devpriv->ai_bipolar = pci230_ai_bipolar[range];
	if (devpriv->ai_bipolar)
		adccon |= PCI230_ADC_IR_BIP;
	else
		adccon |= PCI230_ADC_IR_UNI;

	for (i = 0; i < cmd->chanlist_len; i++) {
		unsigned int gainshift;

		chan = CR_CHAN(cmd->chanlist[i]);
		range = CR_RANGE(cmd->chanlist[i]);
		if (diff) {
			gainshift = 2 * chan;
			if (devpriv->hwver == 0) {
				adcen |= 3 << gainshift;
			} else {
				adcen |= 1 << gainshift;
			}
		} else {
			gainshift = (chan & ~1);
			adcen |= 1 << chan;
		}
		devpriv->adcg = (devpriv->adcg & ~(3 << gainshift))
		    | (pci230_ai_gain[range] << gainshift);
	}

	
	outw(adcen, dev->iobase + PCI230_ADCEN);

	
	outw(devpriv->adcg, dev->iobase + PCI230_ADCG);

	i8254_set_mode(devpriv->iobase1 + PCI230_Z2_CT_BASE, 0, 2, I8254_MODE1);

	adccon |= PCI230_ADC_INT_FIFO_FULL | PCI230_ADC_TRIG_Z2CT2;

	devpriv->adccon = adccon;
	outw(adccon | PCI230_ADC_FIFO_RESET, dev->iobase + PCI230_ADCCON);

	
	udelay(25);

	
	outw(adccon | PCI230_ADC_FIFO_RESET, dev->iobase + PCI230_ADCCON);

	if (cmd->convert_src == TRIG_TIMER) {
		zgat = GAT_CONFIG(2, GAT_GND);
		outb(zgat, devpriv->iobase1 + PCI230_ZGAT_SCE);
		
		pci230_ct_setup_ns_mode(dev, 2, I8254_MODE3, cmd->convert_arg,
					cmd->flags & TRIG_ROUND_MASK);
		if (cmd->scan_begin_src != TRIG_FOLLOW) {
			zgat = GAT_CONFIG(0, GAT_VCC);
			outb(zgat, devpriv->iobase1 + PCI230_ZGAT_SCE);
			pci230_ct_setup_ns_mode(dev, 0, I8254_MODE1,
						((uint64_t) cmd->convert_arg
						 * cmd->scan_end_arg),
						TRIG_ROUND_UP);
			if (cmd->scan_begin_src == TRIG_TIMER) {
				zgat = GAT_CONFIG(1, GAT_GND);
				outb(zgat, devpriv->iobase1 + PCI230_ZGAT_SCE);
				pci230_ct_setup_ns_mode(dev, 1, I8254_MODE3,
							cmd->scan_begin_arg,
							cmd->
							flags &
							TRIG_ROUND_MASK);
			}
		}
	}

	if (cmd->start_src == TRIG_INT) {
		s->async->inttrig = pci230_ai_inttrig_start;
	} else {
		
		pci230_ai_start(dev, s);
	}

	return 0;
}

static unsigned int divide_ns(uint64_t ns, unsigned int timebase,
			      unsigned int round_mode)
{
	uint64_t div;
	unsigned int rem;

	div = ns;
	rem = do_div(div, timebase);
	round_mode &= TRIG_ROUND_MASK;
	switch (round_mode) {
	default:
	case TRIG_ROUND_NEAREST:
		div += (rem + (timebase / 2)) / timebase;
		break;
	case TRIG_ROUND_DOWN:
		break;
	case TRIG_ROUND_UP:
		div += (rem + timebase - 1) / timebase;
		break;
	}
	return div > UINT_MAX ? UINT_MAX : (unsigned int)div;
}

static unsigned int pci230_choose_clk_count(uint64_t ns, unsigned int *count,
					    unsigned int round_mode)
{
	unsigned int clk_src, cnt;

	for (clk_src = CLK_10MHZ;; clk_src++) {
		cnt = divide_ns(ns, pci230_timebase[clk_src], round_mode);
		if ((cnt <= 65536) || (clk_src == CLK_1KHZ))
			break;

	}
	*count = cnt;
	return clk_src;
}

static void pci230_ns_to_single_timer(unsigned int *ns, unsigned int round)
{
	unsigned int count;
	unsigned int clk_src;

	clk_src = pci230_choose_clk_count(*ns, &count, round);
	*ns = count * pci230_timebase[clk_src];
	return;
}

static void pci230_ct_setup_ns_mode(struct comedi_device *dev, unsigned int ct,
				    unsigned int mode, uint64_t ns,
				    unsigned int round)
{
	unsigned int clk_src;
	unsigned int count;

	
	i8254_set_mode(devpriv->iobase1 + PCI230_Z2_CT_BASE, 0, ct, mode);
	
	clk_src = pci230_choose_clk_count(ns, &count, round);
	
	outb(CLK_CONFIG(ct, clk_src), devpriv->iobase1 + PCI230_ZCLK_SCE);
	
	if (count >= 65536)
		count = 0;

	i8254_write(devpriv->iobase1 + PCI230_Z2_CT_BASE, 0, ct, count);
}

static void pci230_cancel_ct(struct comedi_device *dev, unsigned int ct)
{
	i8254_set_mode(devpriv->iobase1 + PCI230_Z2_CT_BASE, 0, ct,
		       I8254_MODE1);
	/* Counter ct, 8254 mode 1, initial count not written. */
}

static irqreturn_t pci230_interrupt(int irq, void *d)
{
	unsigned char status_int, valid_status_int;
	struct comedi_device *dev = (struct comedi_device *)d;
	struct comedi_subdevice *s;
	unsigned long irqflags;

	
	status_int = inb(devpriv->iobase1 + PCI230_INT_STAT);

	if (status_int == PCI230_INT_DISABLE)
		return IRQ_NONE;


	spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	valid_status_int = devpriv->int_en & status_int;
	devpriv->ier = devpriv->int_en & ~status_int;
	outb(devpriv->ier, devpriv->iobase1 + PCI230_INT_SCE);
	devpriv->intr_running = 1;
	devpriv->intr_cpuid = THISCPU;
	spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);


	if ((valid_status_int & PCI230_INT_ZCLK_CT1) != 0) {
		s = dev->write_subdev;
		pci230_handle_ao_nofifo(dev, s);
		comedi_event(dev, s);
	}

	if ((valid_status_int & PCI230P2_INT_DAC) != 0) {
		s = dev->write_subdev;
		pci230_handle_ao_fifo(dev, s);
		comedi_event(dev, s);
	}

	if ((valid_status_int & PCI230_INT_ADC) != 0) {
		s = dev->read_subdev;
		pci230_handle_ai(dev, s);
		comedi_event(dev, s);
	}

	
	spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	if (devpriv->ier != devpriv->int_en) {
		devpriv->ier = devpriv->int_en;
		outb(devpriv->ier, devpriv->iobase1 + PCI230_INT_SCE);
	}
	devpriv->intr_running = 0;
	spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);

	return IRQ_HANDLED;
}

static void pci230_handle_ao_nofifo(struct comedi_device *dev,
				    struct comedi_subdevice *s)
{
	short data;
	int i, ret;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;

	if (!devpriv->ao_continuous && (devpriv->ao_scan_count == 0))
		return;


	for (i = 0; i < cmd->chanlist_len; i++) {
		
		ret = comedi_buf_get(s->async, &data);
		if (ret == 0) {
			s->async->events |= COMEDI_CB_OVERFLOW;
			pci230_ao_stop(dev, s);
			comedi_error(dev, "AO buffer underrun");
			return;
		}
		
		pci230_ao_write_nofifo(dev, data, CR_CHAN(cmd->chanlist[i]));
	}

	async->events |= COMEDI_CB_BLOCK | COMEDI_CB_EOS;
	if (!devpriv->ao_continuous) {
		devpriv->ao_scan_count--;
		if (devpriv->ao_scan_count == 0) {
			
			async->events |= COMEDI_CB_EOA;
			pci230_ao_stop(dev, s);
		}
	}
}

static int pci230_handle_ao_fifo(struct comedi_device *dev,
				 struct comedi_subdevice *s)
{
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned int num_scans;
	unsigned int room;
	unsigned short dacstat;
	unsigned int i, n;
	unsigned int bytes_per_scan;
	unsigned int events = 0;
	int running;

	
	dacstat = inw(dev->iobase + PCI230_DACCON);

	
	bytes_per_scan = cmd->chanlist_len * sizeof(short);
	num_scans = comedi_buf_read_n_available(async) / bytes_per_scan;
	if (!devpriv->ao_continuous) {
		
		if (num_scans > devpriv->ao_scan_count)
			num_scans = devpriv->ao_scan_count;

		if (devpriv->ao_scan_count == 0) {
			
			events |= COMEDI_CB_EOA;
		}
	}
	if (events == 0) {
		
		if ((dacstat & PCI230P2_DAC_FIFO_UNDERRUN_LATCHED) != 0) {
			comedi_error(dev, "AO FIFO underrun");
			events |= COMEDI_CB_OVERFLOW | COMEDI_CB_ERROR;
		}
		if ((num_scans == 0)
		    && ((dacstat & PCI230P2_DAC_FIFO_HALF) == 0)) {
			comedi_error(dev, "AO buffer underrun");
			events |= COMEDI_CB_OVERFLOW | COMEDI_CB_ERROR;
		}
	}
	if (events == 0) {
		
		if ((dacstat & PCI230P2_DAC_FIFO_FULL) != 0)
			room = PCI230P2_DAC_FIFOROOM_FULL;
		else if ((dacstat & PCI230P2_DAC_FIFO_HALF) != 0)
			room = PCI230P2_DAC_FIFOROOM_HALFTOFULL;
		else if ((dacstat & PCI230P2_DAC_FIFO_EMPTY) != 0)
			room = PCI230P2_DAC_FIFOROOM_EMPTY;
		else
			room = PCI230P2_DAC_FIFOROOM_ONETOHALF;

		
		room /= cmd->chanlist_len;
		
		if (num_scans > room)
			num_scans = room;

		
		for (n = 0; n < num_scans; n++) {
			for (i = 0; i < cmd->chanlist_len; i++) {
				short datum;

				comedi_buf_get(async, &datum);
				pci230_ao_write_fifo(dev, datum,
						     CR_CHAN(cmd->chanlist[i]));
			}
		}
		events |= COMEDI_CB_EOS | COMEDI_CB_BLOCK;
		if (!devpriv->ao_continuous) {
			devpriv->ao_scan_count -= num_scans;
			if (devpriv->ao_scan_count == 0) {
				/* All data for the command has been written
				 * to FIFO.  Set FIFO interrupt trigger level
				 * to 'empty'. */
				devpriv->daccon = (devpriv->daccon
						   &
						   ~PCI230P2_DAC_INT_FIFO_MASK)
				    | PCI230P2_DAC_INT_FIFO_EMPTY;
				outw(devpriv->daccon,
				     dev->iobase + PCI230_DACCON);
			}
		}
		
		dacstat = inw(dev->iobase + PCI230_DACCON);
		if ((dacstat & PCI230P2_DAC_FIFO_UNDERRUN_LATCHED) != 0) {
			comedi_error(dev, "AO FIFO underrun");
			events |= COMEDI_CB_OVERFLOW | COMEDI_CB_ERROR;
		}
	}
	if ((events & (COMEDI_CB_EOA | COMEDI_CB_ERROR | COMEDI_CB_OVERFLOW))
	    != 0) {
		
		pci230_ao_stop(dev, s);
		running = 0;
	} else {
		running = 1;
	}
	async->events |= events;
	return running;
}

static void pci230_handle_ai(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	unsigned int events = 0;
	unsigned int status_fifo;
	unsigned int i;
	unsigned int todo;
	unsigned int fifoamount;
	struct comedi_async *async = s->async;
	unsigned int scanlen = async->cmd.scan_end_arg;

	
	if (devpriv->ai_continuous) {
		todo = PCI230_ADC_FIFOLEVEL_HALFFULL;
	} else if (devpriv->ai_scan_count == 0) {
		todo = 0;
	} else if ((devpriv->ai_scan_count > PCI230_ADC_FIFOLEVEL_HALFFULL)
		   || (scanlen > PCI230_ADC_FIFOLEVEL_HALFFULL)) {
		todo = PCI230_ADC_FIFOLEVEL_HALFFULL;
	} else {
		todo = (devpriv->ai_scan_count * scanlen)
		    - devpriv->ai_scan_pos;
		if (todo > PCI230_ADC_FIFOLEVEL_HALFFULL)
			todo = PCI230_ADC_FIFOLEVEL_HALFFULL;

	}

	if (todo == 0)
		return;


	fifoamount = 0;
	for (i = 0; i < todo; i++) {
		if (fifoamount == 0) {
			
			status_fifo = inw(dev->iobase + PCI230_ADCCON);

			if ((status_fifo & PCI230_ADC_FIFO_FULL_LATCHED) != 0) {
				comedi_error(dev, "AI FIFO overrun");
				events |= COMEDI_CB_OVERFLOW | COMEDI_CB_ERROR;
				break;
			} else if ((status_fifo & PCI230_ADC_FIFO_EMPTY) != 0) {
				
				break;
			} else if ((status_fifo & PCI230_ADC_FIFO_HALF) != 0) {
				
				fifoamount = PCI230_ADC_FIFOLEVEL_HALFFULL;
			} else {
				
				if (devpriv->hwver > 0) {
					
					fifoamount = inw(dev->iobase
							 + PCI230P_ADCFFLEV);
					if (fifoamount == 0) {
						
						break;
					}
				} else {
					fifoamount = 1;
				}
			}
		}

		
		if (comedi_buf_put(async, pci230_ai_read(dev)) == 0) {
			events |= COMEDI_CB_ERROR | COMEDI_CB_OVERFLOW;
			comedi_error(dev, "AI buffer overflow");
			break;
		}
		fifoamount--;
		devpriv->ai_scan_pos++;
		if (devpriv->ai_scan_pos == scanlen) {
			
			devpriv->ai_scan_pos = 0;
			devpriv->ai_scan_count--;
			async->events |= COMEDI_CB_EOS;
		}
	}

	if (!devpriv->ai_continuous && (devpriv->ai_scan_count == 0)) {
		
		events |= COMEDI_CB_EOA;
	} else {
		
		events |= COMEDI_CB_BLOCK;
	}
	async->events |= events;

	if ((async->events & (COMEDI_CB_EOA | COMEDI_CB_ERROR |
			      COMEDI_CB_OVERFLOW)) != 0) {
		
		pci230_ai_stop(dev, s);
	} else {
		
		pci230_ai_update_fifo_trigger_level(dev, s);
	}
}

static void pci230_ao_stop(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	unsigned long irqflags;
	unsigned char intsrc;
	int started;
	struct comedi_cmd *cmd;

	spin_lock_irqsave(&devpriv->ao_stop_spinlock, irqflags);
	started = test_and_clear_bit(AO_CMD_STARTED, &devpriv->state);
	spin_unlock_irqrestore(&devpriv->ao_stop_spinlock, irqflags);
	if (!started)
		return;


	cmd = &s->async->cmd;
	if (cmd->scan_begin_src == TRIG_TIMER) {
		
		pci230_cancel_ct(dev, 1);
	}

	
	if (devpriv->hwver < 2) {
		
		intsrc = PCI230_INT_ZCLK_CT1;
	} else {
		
		intsrc = PCI230P2_INT_DAC;
	}
	spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	devpriv->int_en &= ~intsrc;
	while (devpriv->intr_running && devpriv->intr_cpuid != THISCPU) {
		spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);
		spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	}
	if (devpriv->ier != devpriv->int_en) {
		devpriv->ier = devpriv->int_en;
		outb(devpriv->ier, devpriv->iobase1 + PCI230_INT_SCE);
	}
	spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);

	if (devpriv->hwver >= 2) {
		devpriv->daccon &= PCI230_DAC_OR_MASK;
		outw(devpriv->daccon | PCI230P2_DAC_FIFO_RESET
		     | PCI230P2_DAC_FIFO_UNDERRUN_CLEAR,
		     dev->iobase + PCI230_DACCON);
	}

	
	put_all_resources(dev, OWNER_AOCMD);
}

static int pci230_ao_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	pci230_ao_stop(dev, s);
	return 0;
}

static void pci230_ai_stop(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	unsigned long irqflags;
	struct comedi_cmd *cmd;
	int started;

	spin_lock_irqsave(&devpriv->ai_stop_spinlock, irqflags);
	started = test_and_clear_bit(AI_CMD_STARTED, &devpriv->state);
	spin_unlock_irqrestore(&devpriv->ai_stop_spinlock, irqflags);
	if (!started)
		return;


	cmd = &s->async->cmd;
	if (cmd->convert_src == TRIG_TIMER) {
		
		pci230_cancel_ct(dev, 2);
	}
	if (cmd->scan_begin_src != TRIG_FOLLOW) {
		
		pci230_cancel_ct(dev, 0);
	}

	spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	devpriv->int_en &= ~PCI230_INT_ADC;
	while (devpriv->intr_running && devpriv->intr_cpuid != THISCPU) {
		spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);
		spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	}
	if (devpriv->ier != devpriv->int_en) {
		devpriv->ier = devpriv->int_en;
		outb(devpriv->ier, devpriv->iobase1 + PCI230_INT_SCE);
	}
	spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);

	devpriv->adccon = (devpriv->adccon & (PCI230_ADC_IR_MASK
					      | PCI230_ADC_IM_MASK)) |
	    PCI230_ADC_TRIG_NONE;
	outw(devpriv->adccon | PCI230_ADC_FIFO_RESET,
	     dev->iobase + PCI230_ADCCON);

	
	put_all_resources(dev, OWNER_AICMD);
}

static int pci230_ai_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	pci230_ai_stop(dev, s);
	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
