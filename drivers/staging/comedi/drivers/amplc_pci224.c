/*
    comedi/drivers/amplc_pci224.c
    Driver for Amplicon PCI224 and PCI234 AO boards.

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

#include "comedi_fc.h"
#include "8253.h"

#define DRIVER_NAME	"amplc_pci224"

#define PCI_VENDOR_ID_AMPLICON 0x14dc
#define PCI_DEVICE_ID_AMPLICON_PCI224 0x0007
#define PCI_DEVICE_ID_AMPLICON_PCI234 0x0008
#define PCI_DEVICE_ID_INVALID 0xffff

#define PCI224_IO1_SIZE	0x20	
#define PCI224_Z2_CT0	0x14	
#define PCI224_Z2_CT1	0x15	
#define PCI224_Z2_CT2	0x16	
#define PCI224_Z2_CTC	0x17	
#define PCI224_ZCLK_SCE	0x1A	
#define PCI224_ZGAT_SCE	0x1D	
#define PCI224_INT_SCE	0x1E	
				

#define PCI224_IO2_SIZE	0x10	
#define PCI224_DACDATA	0x00	
#define PCI224_SOFTTRIG	0x00	
#define PCI224_DACCON	0x02	
#define PCI224_FIFOSIZ	0x04	
#define PCI224_DACCEN	0x06	

#define PCI224_DACCON_TRIG_MASK		(7 << 0)
#define PCI224_DACCON_TRIG_NONE		(0 << 0)	
#define PCI224_DACCON_TRIG_SW		(1 << 0)	
#define PCI224_DACCON_TRIG_EXTP		(2 << 0)	
#define PCI224_DACCON_TRIG_EXTN		(3 << 0)	
#define PCI224_DACCON_TRIG_Z2CT0	(4 << 0)	
#define PCI224_DACCON_TRIG_Z2CT1	(5 << 0)	
#define PCI224_DACCON_TRIG_Z2CT2	(6 << 0)	
#define PCI224_DACCON_POLAR_MASK	(1 << 3)
#define PCI224_DACCON_POLAR_UNI		(0 << 3)	
#define PCI224_DACCON_POLAR_BI		(1 << 3)	
#define PCI224_DACCON_VREF_MASK		(3 << 4)
#define PCI224_DACCON_VREF_1_25		(0 << 4)	
#define PCI224_DACCON_VREF_2_5		(1 << 4)	
#define PCI224_DACCON_VREF_5		(2 << 4)	
#define PCI224_DACCON_VREF_10		(3 << 4)	
#define PCI224_DACCON_FIFOWRAP		(1 << 7)
#define PCI224_DACCON_FIFOENAB		(1 << 8)
#define PCI224_DACCON_FIFOINTR_MASK	(7 << 9)
#define PCI224_DACCON_FIFOINTR_EMPTY	(0 << 9)	
#define PCI224_DACCON_FIFOINTR_NEMPTY	(1 << 9)	
#define PCI224_DACCON_FIFOINTR_NHALF	(2 << 9)	
#define PCI224_DACCON_FIFOINTR_HALF	(3 << 9)	
#define PCI224_DACCON_FIFOINTR_NFULL	(4 << 9)	
#define PCI224_DACCON_FIFOINTR_FULL	(5 << 9)	
#define PCI224_DACCON_FIFOFL_MASK	(7 << 12)
#define PCI224_DACCON_FIFOFL_EMPTY	(1 << 12)	
#define PCI224_DACCON_FIFOFL_ONETOHALF	(0 << 12)	
#define PCI224_DACCON_FIFOFL_HALFTOFULL	(4 << 12)	
#define PCI224_DACCON_FIFOFL_FULL	(6 << 12)	
#define PCI224_DACCON_BUSY		(1 << 15)
#define PCI224_DACCON_FIFORESET		(1 << 12)
#define PCI224_DACCON_GLOBALRESET	(1 << 13)

#define PCI224_FIFO_SIZE	4096

/*
 * DAC FIFO guaranteed minimum room available, depending on reported fill level.
 * The maximum room available depends on the reported fill level and how much
 * has been written!
 */
#define PCI224_FIFO_ROOM_EMPTY		PCI224_FIFO_SIZE
#define PCI224_FIFO_ROOM_ONETOHALF	(PCI224_FIFO_SIZE / 2)
#define PCI224_FIFO_ROOM_HALFTOFULL	1
#define PCI224_FIFO_ROOM_FULL		0

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


#define PCI224_INTR_EXT		0x01	
#define PCI224_INTR_DAC		0x04	
#define PCI224_INTR_Z2CT1	0x20	

#define PCI224_INTR_EDGE_BITS	(PCI224_INTR_EXT | PCI224_INTR_Z2CT1)
#define PCI224_INTR_LEVEL_BITS	PCI224_INTR_DACFIFO


#define COMBINE(old, new, mask)	(((old) & ~(mask)) | ((new) & (mask)))

#define NULLFUNC	0

#define THISCPU		smp_processor_id()

#define AO_CMD_STARTED	0


static const struct comedi_lrange range_pci224_internal = {
	8,
	{
	 BIP_RANGE(10),
	 BIP_RANGE(5),
	 BIP_RANGE(2.5),
	 BIP_RANGE(1.25),
	 UNI_RANGE(10),
	 UNI_RANGE(5),
	 UNI_RANGE(2.5),
	 UNI_RANGE(1.25),
	 }
};

static const unsigned short hwrange_pci224_internal[8] = {
	PCI224_DACCON_POLAR_BI | PCI224_DACCON_VREF_10,
	PCI224_DACCON_POLAR_BI | PCI224_DACCON_VREF_5,
	PCI224_DACCON_POLAR_BI | PCI224_DACCON_VREF_2_5,
	PCI224_DACCON_POLAR_BI | PCI224_DACCON_VREF_1_25,
	PCI224_DACCON_POLAR_UNI | PCI224_DACCON_VREF_10,
	PCI224_DACCON_POLAR_UNI | PCI224_DACCON_VREF_5,
	PCI224_DACCON_POLAR_UNI | PCI224_DACCON_VREF_2_5,
	PCI224_DACCON_POLAR_UNI | PCI224_DACCON_VREF_1_25,
};

static const struct comedi_lrange range_pci224_external = {
	2,
	{
	 RANGE_ext(-1, 1),	
	 RANGE_ext(0, 1),	
	 }
};

static const unsigned short hwrange_pci224_external[2] = {
	PCI224_DACCON_POLAR_BI,
	PCI224_DACCON_POLAR_UNI,
};

static const struct comedi_lrange range_pci234_ext2 = {
	1,
	{
	 RANGE_ext(-2, 2),
	 }
};

static const struct comedi_lrange range_pci234_ext = {
	1,
	{
	 RANGE_ext(-1, 1),
	 }
};

static const unsigned short hwrange_pci234[1] = {
	PCI224_DACCON_POLAR_BI,	
};


enum pci224_model { any_model, pci224_model, pci234_model };

struct pci224_board {
	const char *name;
	unsigned short devid;
	enum pci224_model model;
	unsigned int ao_chans;
	unsigned int ao_bits;
};

static const struct pci224_board pci224_boards[] = {
	{
	 .name = "pci224",
	 .devid = PCI_DEVICE_ID_AMPLICON_PCI224,
	 .model = pci224_model,
	 .ao_chans = 16,
	 .ao_bits = 12,
	 },
	{
	 .name = "pci234",
	 .devid = PCI_DEVICE_ID_AMPLICON_PCI234,
	 .model = pci234_model,
	 .ao_chans = 4,
	 .ao_bits = 16,
	 },
	{
	 .name = DRIVER_NAME,
	 .devid = PCI_DEVICE_ID_INVALID,
	 .model = any_model,	
	 },
};


static DEFINE_PCI_DEVICE_TABLE(pci224_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMPLICON, PCI_DEVICE_ID_AMPLICON_PCI224) },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMPLICON, PCI_DEVICE_ID_AMPLICON_PCI234) },
	{0}
};

MODULE_DEVICE_TABLE(pci, pci224_pci_table);

#define thisboard ((struct pci224_board *)dev->board_ptr)

struct pci224_private {
	struct pci_dev *pci_dev;	
	const unsigned short *hwrange;
	unsigned long iobase1;
	unsigned long state;
	spinlock_t ao_spinlock;
	unsigned int *ao_readback;
	short *ao_scan_vals;
	unsigned char *ao_scan_order;
	int intr_cpuid;
	short intr_running;
	unsigned short daccon;
	unsigned int cached_div1;
	unsigned int cached_div2;
	unsigned int ao_stop_count;
	short ao_stop_continuous;
	unsigned short ao_enab;	
	unsigned char intsce;
};

#define devpriv ((struct pci224_private *)dev->private)

static int pci224_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int pci224_detach(struct comedi_device *dev);
static struct comedi_driver driver_amplc_pci224 = {
	.driver_name = DRIVER_NAME,
	.module = THIS_MODULE,
	.attach = pci224_attach,
	.detach = pci224_detach,
	.board_name = &pci224_boards[0].name,
	.offset = sizeof(struct pci224_board),
	.num_names = ARRAY_SIZE(pci224_boards),
};

static int __devinit driver_amplc_pci224_pci_probe(struct pci_dev *dev,
						   const struct pci_device_id
						   *ent)
{
	return comedi_pci_auto_config(dev, driver_amplc_pci224.driver_name);
}

static void __devexit driver_amplc_pci224_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_amplc_pci224_pci_driver = {
	.id_table = pci224_pci_table,
	.probe = &driver_amplc_pci224_pci_probe,
	.remove = __devexit_p(&driver_amplc_pci224_pci_remove)
};

static int __init driver_amplc_pci224_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_amplc_pci224);
	if (retval < 0)
		return retval;

	driver_amplc_pci224_pci_driver.name =
	    (char *)driver_amplc_pci224.driver_name;
	return pci_register_driver(&driver_amplc_pci224_pci_driver);
}

static void __exit driver_amplc_pci224_cleanup_module(void)
{
	pci_unregister_driver(&driver_amplc_pci224_pci_driver);
	comedi_driver_unregister(&driver_amplc_pci224);
}

module_init(driver_amplc_pci224_init_module);
module_exit(driver_amplc_pci224_cleanup_module);

static void
pci224_ao_set_data(struct comedi_device *dev, int chan, int range,
		   unsigned int data)
{
	unsigned short mangled;

	
	devpriv->ao_readback[chan] = data;
	
	outw(1 << chan, dev->iobase + PCI224_DACCEN);
	
	devpriv->daccon = COMBINE(devpriv->daccon, devpriv->hwrange[range],
				  (PCI224_DACCON_POLAR_MASK |
				   PCI224_DACCON_VREF_MASK));
	outw(devpriv->daccon | PCI224_DACCON_FIFORESET,
	     dev->iobase + PCI224_DACCON);
	mangled = (unsigned short)data << (16 - thisboard->ao_bits);
	if ((devpriv->daccon & PCI224_DACCON_POLAR_MASK) ==
	    PCI224_DACCON_POLAR_BI) {
		mangled ^= 0x8000;
	}
	
	outw(mangled, dev->iobase + PCI224_DACDATA);
	
	inw(dev->iobase + PCI224_SOFTTRIG);
}

static int
pci224_ao_insn_write(struct comedi_device *dev, struct comedi_subdevice *s,
		     struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan, range;

	
	chan = CR_CHAN(insn->chanspec);
	range = CR_RANGE(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		pci224_ao_set_data(dev, chan, range, data[i]);

	return i;
}

/*
 * 'insn_read' function for AO subdevice.
 *
 * N.B. The value read will not be valid if the DAC channel has
 * never been written successfully since the device was attached
 * or since the channel has been used by an AO streaming write
 * command.
 */
static int
pci224_ao_insn_read(struct comedi_device *dev, struct comedi_subdevice *s,
		    struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan;

	chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_readback[chan];


	return i;
}

static void
pci224_cascade_ns_to_timer(int osc_base, unsigned int *d1, unsigned int *d2,
			   unsigned int *nanosec, int round_mode)
{
	i8253_cascade_ns_to_timer(osc_base, d1, d2, nanosec, round_mode);
}

static void pci224_ao_stop(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	unsigned long flags;

	if (!test_and_clear_bit(AO_CMD_STARTED, &devpriv->state))
		return;


	spin_lock_irqsave(&devpriv->ao_spinlock, flags);
	
	devpriv->intsce = 0;
	outb(0, devpriv->iobase1 + PCI224_INT_SCE);
	while (devpriv->intr_running && devpriv->intr_cpuid != THISCPU) {
		spin_unlock_irqrestore(&devpriv->ao_spinlock, flags);
		spin_lock_irqsave(&devpriv->ao_spinlock, flags);
	}
	spin_unlock_irqrestore(&devpriv->ao_spinlock, flags);
	
	outw(0, dev->iobase + PCI224_DACCEN);	
	devpriv->daccon = COMBINE(devpriv->daccon,
				  PCI224_DACCON_TRIG_SW |
				  PCI224_DACCON_FIFOINTR_EMPTY,
				  PCI224_DACCON_TRIG_MASK |
				  PCI224_DACCON_FIFOINTR_MASK);
	outw(devpriv->daccon | PCI224_DACCON_FIFORESET,
	     dev->iobase + PCI224_DACCON);
}

static void pci224_ao_start(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned long flags;

	set_bit(AO_CMD_STARTED, &devpriv->state);
	if (!devpriv->ao_stop_continuous && devpriv->ao_stop_count == 0) {
		
		pci224_ao_stop(dev, s);
		s->async->events |= COMEDI_CB_EOA;
		comedi_event(dev, s);
	} else {
		
		spin_lock_irqsave(&devpriv->ao_spinlock, flags);
		if (cmd->stop_src == TRIG_EXT)
			devpriv->intsce = PCI224_INTR_EXT | PCI224_INTR_DAC;
		else
			devpriv->intsce = PCI224_INTR_DAC;

		outb(devpriv->intsce, devpriv->iobase1 + PCI224_INT_SCE);
		spin_unlock_irqrestore(&devpriv->ao_spinlock, flags);
	}
}

static void pci224_ao_handle_fifo(struct comedi_device *dev,
				  struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int num_scans;
	unsigned int room;
	unsigned short dacstat;
	unsigned int i, n;
	unsigned int bytes_per_scan;

	if (cmd->chanlist_len) {
		bytes_per_scan = cmd->chanlist_len * sizeof(short);
	} else {
		
		bytes_per_scan = sizeof(short);
	}
	
	num_scans = comedi_buf_read_n_available(s->async) / bytes_per_scan;
	if (!devpriv->ao_stop_continuous) {
		
		if (num_scans > devpriv->ao_stop_count)
			num_scans = devpriv->ao_stop_count;

	}

	
	dacstat = inw(dev->iobase + PCI224_DACCON);
	switch (dacstat & PCI224_DACCON_FIFOFL_MASK) {
	case PCI224_DACCON_FIFOFL_EMPTY:
		room = PCI224_FIFO_ROOM_EMPTY;
		if (!devpriv->ao_stop_continuous && devpriv->ao_stop_count == 0) {
			
			pci224_ao_stop(dev, s);
			s->async->events |= COMEDI_CB_EOA;
			comedi_event(dev, s);
			return;
		}
		break;
	case PCI224_DACCON_FIFOFL_ONETOHALF:
		room = PCI224_FIFO_ROOM_ONETOHALF;
		break;
	case PCI224_DACCON_FIFOFL_HALFTOFULL:
		room = PCI224_FIFO_ROOM_HALFTOFULL;
		break;
	default:
		room = PCI224_FIFO_ROOM_FULL;
		break;
	}
	if (room >= PCI224_FIFO_ROOM_ONETOHALF) {
		
		if (num_scans == 0) {
			
			pci224_ao_stop(dev, s);
			s->async->events |= COMEDI_CB_OVERFLOW;
			printk(KERN_ERR "comedi%d: "
			       "AO buffer underrun\n", dev->minor);
		}
	}
	
	if (cmd->chanlist_len)
		room /= cmd->chanlist_len;

	
	if (num_scans > room)
		num_scans = room;

	
	for (n = 0; n < num_scans; n++) {
		cfc_read_array_from_buffer(s, &devpriv->ao_scan_vals[0],
					   bytes_per_scan);
		for (i = 0; i < cmd->chanlist_len; i++) {
			outw(devpriv->ao_scan_vals[devpriv->ao_scan_order[i]],
			     dev->iobase + PCI224_DACDATA);
		}
	}
	if (!devpriv->ao_stop_continuous) {
		devpriv->ao_stop_count -= num_scans;
		if (devpriv->ao_stop_count == 0) {
			devpriv->daccon = COMBINE(devpriv->daccon,
						  PCI224_DACCON_FIFOINTR_EMPTY,
						  PCI224_DACCON_FIFOINTR_MASK);
			outw(devpriv->daccon, dev->iobase + PCI224_DACCON);
		}
	}
	if ((devpriv->daccon & PCI224_DACCON_TRIG_MASK) ==
	    PCI224_DACCON_TRIG_NONE) {
		unsigned short trig;

		/*
		 * This is the initial DAC FIFO interrupt at the
		 * start of the acquisition.  The DAC's scan trigger
		 * has been set to 'none' up until now.
		 *
		 * Now that data has been written to the FIFO, the
		 * DAC's scan trigger source can be set to the
		 * correct value.
		 *
		 * BUG: The first scan will be triggered immediately
		 * if the scan trigger source is at logic level 1.
		 */
		if (cmd->scan_begin_src == TRIG_TIMER) {
			trig = PCI224_DACCON_TRIG_Z2CT0;
		} else {
			
			if (cmd->scan_begin_arg & CR_INVERT)
				trig = PCI224_DACCON_TRIG_EXTN;
			else
				trig = PCI224_DACCON_TRIG_EXTP;

		}
		devpriv->daccon = COMBINE(devpriv->daccon, trig,
					  PCI224_DACCON_TRIG_MASK);
		outw(devpriv->daccon, dev->iobase + PCI224_DACCON);
	}
	if (s->async->events)
		comedi_event(dev, s);

}

static int
pci224_ao_inttrig_start(struct comedi_device *dev, struct comedi_subdevice *s,
			unsigned int trignum)
{
	if (trignum != 0)
		return -EINVAL;

	s->async->inttrig = NULLFUNC;
	pci224_ao_start(dev, s);

	return 1;
}

#define MAX_SCAN_PERIOD		0xFFFFFFFFU
#define MIN_SCAN_PERIOD		2500
#define CONVERT_PERIOD		625

static int
pci224_ao_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
		  struct comedi_cmd *cmd)
{
	int err = 0;
	unsigned int tmp;

	

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_INT | TRIG_EXT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_EXT | TRIG_TIMER;
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
	cmd->stop_src &= TRIG_COUNT | TRIG_EXT | TRIG_NONE;
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

	tmp = 0;
	if (cmd->start_src & TRIG_EXT)
		tmp++;
	if (cmd->scan_begin_src & TRIG_EXT)
		tmp++;
	if (cmd->stop_src & TRIG_EXT)
		tmp++;
	if (tmp > 1)
		err++;

	if (err)
		return 2;

	

	switch (cmd->start_src) {
	case TRIG_INT:
		if (cmd->start_arg != 0) {
			cmd->start_arg = 0;
			err++;
		}
		break;
	case TRIG_EXT:
		
		if ((cmd->start_arg & ~CR_FLAGS_MASK) != 0) {
			cmd->start_arg = COMBINE(cmd->start_arg, 0,
						 ~CR_FLAGS_MASK);
			err++;
		}
		
		if ((cmd->start_arg & CR_FLAGS_MASK & ~CR_EDGE) != 0) {
			cmd->start_arg = COMBINE(cmd->start_arg, 0,
						 CR_FLAGS_MASK & ~CR_EDGE);
			err++;
		}
		break;
	}

	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:
		if (cmd->scan_begin_arg > MAX_SCAN_PERIOD) {
			cmd->scan_begin_arg = MAX_SCAN_PERIOD;
			err++;
		}
		tmp = cmd->chanlist_len * CONVERT_PERIOD;
		if (tmp < MIN_SCAN_PERIOD)
			tmp = MIN_SCAN_PERIOD;

		if (cmd->scan_begin_arg < tmp) {
			cmd->scan_begin_arg = tmp;
			err++;
		}
		break;
	case TRIG_EXT:
		
		if ((cmd->scan_begin_arg & ~CR_FLAGS_MASK) != 0) {
			cmd->scan_begin_arg = COMBINE(cmd->scan_begin_arg, 0,
						      ~CR_FLAGS_MASK);
			err++;
		}
		
		if ((cmd->scan_begin_arg & CR_FLAGS_MASK &
		     ~(CR_EDGE | CR_INVERT)) != 0) {
			cmd->scan_begin_arg = COMBINE(cmd->scan_begin_arg, 0,
						      CR_FLAGS_MASK & ~(CR_EDGE
									|
									CR_INVERT));
			err++;
		}
		break;
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
	case TRIG_EXT:
		
		if ((cmd->stop_arg & ~CR_FLAGS_MASK) != 0) {
			cmd->stop_arg = COMBINE(cmd->stop_arg, 0,
						~CR_FLAGS_MASK);
			err++;
		}
		
		if ((cmd->stop_arg & CR_FLAGS_MASK & ~CR_EDGE) != 0) {
			cmd->stop_arg = COMBINE(cmd->stop_arg, 0,
						CR_FLAGS_MASK & ~CR_EDGE);
		}
		break;
	case TRIG_NONE:
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
		break;
	}

	if (err)
		return 3;

	

	if (cmd->scan_begin_src == TRIG_TIMER) {
		unsigned int div1, div2, round;
		int round_mode = cmd->flags & TRIG_ROUND_MASK;

		tmp = cmd->scan_begin_arg;
		
		switch (round_mode) {
		case TRIG_ROUND_NEAREST:
		default:
			round = TIMEBASE_10MHZ / 2;
			break;
		case TRIG_ROUND_DOWN:
			round = 0;
			break;
		case TRIG_ROUND_UP:
			round = TIMEBASE_10MHZ - 1;
			break;
		}
		
		div2 = cmd->scan_begin_arg / TIMEBASE_10MHZ;
		div2 += (round + cmd->scan_begin_arg % TIMEBASE_10MHZ) /
		    TIMEBASE_10MHZ;
		if (div2 <= 0x10000) {
			
			if (div2 < 2)
				div2 = 2;
			cmd->scan_begin_arg = div2 * TIMEBASE_10MHZ;
			if (cmd->scan_begin_arg < div2 ||
			    cmd->scan_begin_arg < TIMEBASE_10MHZ) {
				
				cmd->scan_begin_arg = MAX_SCAN_PERIOD;
			}
		} else {
			
			div1 = devpriv->cached_div1;
			div2 = devpriv->cached_div2;
			pci224_cascade_ns_to_timer(TIMEBASE_10MHZ, &div1, &div2,
						   &cmd->scan_begin_arg,
						   round_mode);
			devpriv->cached_div1 = div1;
			devpriv->cached_div2 = div2;
		}
		if (tmp != cmd->scan_begin_arg)
			err++;

	}

	if (err)
		return 4;

	

	if (cmd->chanlist && (cmd->chanlist_len > 0)) {
		unsigned int range;
		enum { range_err = 1, dupchan_err = 2, };
		unsigned errors;
		unsigned int n;
		unsigned int ch;

		range = CR_RANGE(cmd->chanlist[0]);
		errors = 0;
		tmp = 0;
		for (n = 0; n < cmd->chanlist_len; n++) {
			ch = CR_CHAN(cmd->chanlist[n]);
			if (tmp & (1U << ch))
				errors |= dupchan_err;

			tmp |= (1U << ch);
			if (CR_RANGE(cmd->chanlist[n]) != range)
				errors |= range_err;

		}
		if (errors) {
			if (errors & dupchan_err) {
				DPRINTK("comedi%d: " DRIVER_NAME
					": ao_cmdtest: "
					"entries in chanlist must contain no "
					"duplicate channels\n", dev->minor);
			}
			if (errors & range_err) {
				DPRINTK("comedi%d: " DRIVER_NAME
					": ao_cmdtest: "
					"entries in chanlist must all have "
					"the same range index\n", dev->minor);
			}
			err++;
		}
	}

	if (err)
		return 5;

	return 0;
}

static int pci224_ao_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	int range;
	unsigned int i, j;
	unsigned int ch;
	unsigned int rank;
	unsigned long flags;

	
	if (cmd->chanlist == NULL || cmd->chanlist_len == 0)
		return -EINVAL;


	
	devpriv->ao_enab = 0;

	for (i = 0; i < cmd->chanlist_len; i++) {
		ch = CR_CHAN(cmd->chanlist[i]);
		devpriv->ao_enab |= 1U << ch;
		rank = 0;
		for (j = 0; j < cmd->chanlist_len; j++) {
			if (CR_CHAN(cmd->chanlist[j]) < ch)
				rank++;

		}
		devpriv->ao_scan_order[rank] = i;
	}

	
	outw(devpriv->ao_enab, dev->iobase + PCI224_DACCEN);

	
	range = CR_RANGE(cmd->chanlist[0]);

	devpriv->daccon = COMBINE(devpriv->daccon,
				  (devpriv->
				   hwrange[range] | PCI224_DACCON_TRIG_NONE |
				   PCI224_DACCON_FIFOINTR_NHALF),
				  (PCI224_DACCON_POLAR_MASK |
				   PCI224_DACCON_VREF_MASK |
				   PCI224_DACCON_TRIG_MASK |
				   PCI224_DACCON_FIFOINTR_MASK));
	outw(devpriv->daccon | PCI224_DACCON_FIFORESET,
	     dev->iobase + PCI224_DACCON);

	if (cmd->scan_begin_src == TRIG_TIMER) {
		unsigned int div1, div2, round;
		unsigned int ns = cmd->scan_begin_arg;
		int round_mode = cmd->flags & TRIG_ROUND_MASK;

		
		switch (round_mode) {
		case TRIG_ROUND_NEAREST:
		default:
			round = TIMEBASE_10MHZ / 2;
			break;
		case TRIG_ROUND_DOWN:
			round = 0;
			break;
		case TRIG_ROUND_UP:
			round = TIMEBASE_10MHZ - 1;
			break;
		}
		
		div2 = cmd->scan_begin_arg / TIMEBASE_10MHZ;
		div2 += (round + cmd->scan_begin_arg % TIMEBASE_10MHZ) /
		    TIMEBASE_10MHZ;
		if (div2 <= 0x10000) {
			
			if (div2 < 2)
				div2 = 2;
			div2 &= 0xffff;
			div1 = 1;	
		} else {
			
			div1 = devpriv->cached_div1;
			div2 = devpriv->cached_div2;
			pci224_cascade_ns_to_timer(TIMEBASE_10MHZ, &div1, &div2,
						   &ns, round_mode);
		}

		
		outb(GAT_CONFIG(0, GAT_VCC),
		     devpriv->iobase1 + PCI224_ZGAT_SCE);
		if (div1 == 1) {
			
			outb(CLK_CONFIG(0, CLK_10MHZ),
			     devpriv->iobase1 + PCI224_ZCLK_SCE);
		} else {
			
			
			outb(GAT_CONFIG(2, GAT_VCC),
			     devpriv->iobase1 + PCI224_ZGAT_SCE);
			
			outb(CLK_CONFIG(2, CLK_10MHZ),
			     devpriv->iobase1 + PCI224_ZCLK_SCE);
			
			i8254_load(devpriv->iobase1 + PCI224_Z2_CT0, 0,
				   2, div1, 2);
			
			outb(CLK_CONFIG(0, CLK_OUTNM1),
			     devpriv->iobase1 + PCI224_ZCLK_SCE);
		}
		
		i8254_load(devpriv->iobase1 + PCI224_Z2_CT0, 0, 0, div2, 2);
	}

	switch (cmd->stop_src) {
	case TRIG_COUNT:
		
		devpriv->ao_stop_continuous = 0;
		devpriv->ao_stop_count = cmd->stop_arg;
		break;
	default:
		
		devpriv->ao_stop_continuous = 1;
		devpriv->ao_stop_count = 0;
		break;
	}

	switch (cmd->start_src) {
	case TRIG_INT:
		spin_lock_irqsave(&devpriv->ao_spinlock, flags);
		s->async->inttrig = &pci224_ao_inttrig_start;
		spin_unlock_irqrestore(&devpriv->ao_spinlock, flags);
		break;
	case TRIG_EXT:
		
		spin_lock_irqsave(&devpriv->ao_spinlock, flags);
		devpriv->intsce |= PCI224_INTR_EXT;
		outb(devpriv->intsce, devpriv->iobase1 + PCI224_INT_SCE);
		spin_unlock_irqrestore(&devpriv->ao_spinlock, flags);
		break;
	}

	return 0;
}

static int pci224_ao_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	pci224_ao_stop(dev, s);
	return 0;
}

static void
pci224_ao_munge(struct comedi_device *dev, struct comedi_subdevice *s,
		void *data, unsigned int num_bytes, unsigned int chan_index)
{
	struct comedi_async *async = s->async;
	short *array = data;
	unsigned int length = num_bytes / sizeof(*array);
	unsigned int offset;
	unsigned int shift;
	unsigned int i;

	
	shift = 16 - thisboard->ao_bits;
	
	if ((devpriv->hwrange[CR_RANGE(async->cmd.chanlist[0])] &
	     PCI224_DACCON_POLAR_MASK) == PCI224_DACCON_POLAR_UNI) {
		
		offset = 0;
	} else {
		
		offset = 32768;
	}
	
	for (i = 0; i < length; i++)
		array[i] = (array[i] << shift) - offset;

}

static irqreturn_t pci224_interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = &dev->subdevices[0];
	struct comedi_cmd *cmd;
	unsigned char intstat, valid_intstat;
	unsigned char curenab;
	int retval = 0;
	unsigned long flags;

	intstat = inb(devpriv->iobase1 + PCI224_INT_SCE) & 0x3F;
	if (intstat) {
		retval = 1;
		spin_lock_irqsave(&devpriv->ao_spinlock, flags);
		valid_intstat = devpriv->intsce & intstat;
		
		curenab = devpriv->intsce & ~intstat;
		outb(curenab, devpriv->iobase1 + PCI224_INT_SCE);
		devpriv->intr_running = 1;
		devpriv->intr_cpuid = THISCPU;
		spin_unlock_irqrestore(&devpriv->ao_spinlock, flags);
		if (valid_intstat != 0) {
			cmd = &s->async->cmd;
			if (valid_intstat & PCI224_INTR_EXT) {
				devpriv->intsce &= ~PCI224_INTR_EXT;
				if (cmd->start_src == TRIG_EXT)
					pci224_ao_start(dev, s);
				else if (cmd->stop_src == TRIG_EXT)
					pci224_ao_stop(dev, s);

			}
			if (valid_intstat & PCI224_INTR_DAC)
				pci224_ao_handle_fifo(dev, s);

		}
		
		spin_lock_irqsave(&devpriv->ao_spinlock, flags);
		if (curenab != devpriv->intsce) {
			outb(devpriv->intsce,
			     devpriv->iobase1 + PCI224_INT_SCE);
		}
		devpriv->intr_running = 0;
		spin_unlock_irqrestore(&devpriv->ao_spinlock, flags);
	}
	return IRQ_RETVAL(retval);
}

static int
pci224_find_pci(struct comedi_device *dev, int bus, int slot,
		struct pci_dev **pci_dev_p)
{
	struct pci_dev *pci_dev = NULL;

	*pci_dev_p = NULL;

	
	for (pci_dev = pci_get_device(PCI_VENDOR_ID_AMPLICON, PCI_ANY_ID, NULL);
	     pci_dev != NULL;
	     pci_dev = pci_get_device(PCI_VENDOR_ID_AMPLICON, PCI_ANY_ID,
				      pci_dev)) {
		
		if (bus || slot) {
			if (bus != pci_dev->bus->number
			    || slot != PCI_SLOT(pci_dev->devfn))
				continue;
		}
		if (thisboard->model == any_model) {
			
			int i;

			for (i = 0; i < ARRAY_SIZE(pci224_boards); i++) {
				if (pci_dev->device == pci224_boards[i].devid) {
					
					dev->board_ptr = &pci224_boards[i];
					break;
				}
			}
			if (i == ARRAY_SIZE(pci224_boards))
				continue;
		} else {
			
			if (thisboard->devid != pci_dev->device)
				continue;
		}

		
		*pci_dev_p = pci_dev;
		return 0;
	}
	
	if (bus || slot) {
		printk(KERN_ERR "comedi%d: error! "
		       "no %s found at pci %02x:%02x!\n",
		       dev->minor, thisboard->name, bus, slot);
	} else {
		printk(KERN_ERR "comedi%d: error! no %s found!\n",
		       dev->minor, thisboard->name);
	}
	return -EIO;
}

static int pci224_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	struct pci_dev *pci_dev;
	unsigned int irq;
	int bus = 0, slot = 0;
	unsigned n;
	int ret;

	printk(KERN_DEBUG "comedi%d: %s: attach\n", dev->minor, DRIVER_NAME);

	bus = it->options[0];
	slot = it->options[1];
	ret = alloc_private(dev, sizeof(struct pci224_private));
	if (ret < 0) {
		printk(KERN_ERR "comedi%d: error! out of memory!\n",
		       dev->minor);
		return ret;
	}

	ret = pci224_find_pci(dev, bus, slot, &pci_dev);
	if (ret < 0)
		return ret;

	devpriv->pci_dev = pci_dev;
	ret = comedi_pci_enable(pci_dev, DRIVER_NAME);
	if (ret < 0) {
		printk(KERN_ERR
		       "comedi%d: error! cannot enable PCI device "
		       "and request regions!\n", dev->minor);
		return ret;
	}
	spin_lock_init(&devpriv->ao_spinlock);

	devpriv->iobase1 = pci_resource_start(pci_dev, 2);
	dev->iobase = pci_resource_start(pci_dev, 3);
	irq = pci_dev->irq;

	
	devpriv->ao_readback = kmalloc(sizeof(devpriv->ao_readback[0]) *
				       thisboard->ao_chans, GFP_KERNEL);
	if (!devpriv->ao_readback)
		return -ENOMEM;


	
	devpriv->ao_scan_vals = kmalloc(sizeof(devpriv->ao_scan_vals[0]) *
					thisboard->ao_chans, GFP_KERNEL);
	if (!devpriv->ao_scan_vals)
		return -ENOMEM;


	
	devpriv->ao_scan_order = kmalloc(sizeof(devpriv->ao_scan_order[0]) *
					 thisboard->ao_chans, GFP_KERNEL);
	if (!devpriv->ao_scan_order)
		return -ENOMEM;


	
	devpriv->intsce = 0;
	outb(0, devpriv->iobase1 + PCI224_INT_SCE);

	
	outw(PCI224_DACCON_GLOBALRESET, dev->iobase + PCI224_DACCON);
	outw(0, dev->iobase + PCI224_DACCEN);
	outw(0, dev->iobase + PCI224_FIFOSIZ);
	devpriv->daccon = (PCI224_DACCON_TRIG_SW | PCI224_DACCON_POLAR_BI |
			   PCI224_DACCON_FIFOENAB |
			   PCI224_DACCON_FIFOINTR_EMPTY);
	outw(devpriv->daccon | PCI224_DACCON_FIFORESET,
	     dev->iobase + PCI224_DACCON);

	
	ret = alloc_subdevices(dev, 1);
	if (ret < 0) {
		printk(KERN_ERR "comedi%d: error! out of memory!\n",
		       dev->minor);
		return ret;
	}

	s = dev->subdevices + 0;
	
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE | SDF_GROUND | SDF_CMD_WRITE;
	s->n_chan = thisboard->ao_chans;
	s->maxdata = (1 << thisboard->ao_bits) - 1;
	s->insn_write = &pci224_ao_insn_write;
	s->insn_read = &pci224_ao_insn_read;
	s->len_chanlist = s->n_chan;

	dev->write_subdev = s;
	s->do_cmd = &pci224_ao_cmd;
	s->do_cmdtest = &pci224_ao_cmdtest;
	s->cancel = &pci224_ao_cancel;
	s->munge = &pci224_ao_munge;

	
	if (thisboard->model == pci234_model) {
		
		const struct comedi_lrange **range_table_list;

		s->range_table_list = range_table_list =
		    kmalloc(sizeof(struct comedi_lrange *) * s->n_chan,
			    GFP_KERNEL);
		if (!s->range_table_list)
			return -ENOMEM;

		for (n = 2; n < 3 + s->n_chan; n++) {
			if (it->options[n] < 0 || it->options[n] > 1) {
				printk(KERN_WARNING "comedi%d: %s: warning! "
				       "bad options[%u]=%d\n",
				       dev->minor, DRIVER_NAME, n,
				       it->options[n]);
			}
		}
		for (n = 0; n < s->n_chan; n++) {
			if (n < COMEDI_NDEVCONFOPTS - 3 &&
			    it->options[3 + n] == 1) {
				if (it->options[2] == 1)
					range_table_list[n] = &range_pci234_ext;
				else
					range_table_list[n] = &range_bipolar5;

			} else {
				if (it->options[2] == 1) {
					range_table_list[n] =
					    &range_pci234_ext2;
				} else {
					range_table_list[n] = &range_bipolar10;
				}
			}
		}
		devpriv->hwrange = hwrange_pci234;
	} else {
		
		if (it->options[2] == 1) {
			s->range_table = &range_pci224_external;
			devpriv->hwrange = hwrange_pci224_external;
		} else {
			if (it->options[2] != 0) {
				printk(KERN_WARNING "comedi%d: %s: warning! "
				       "bad options[2]=%d\n",
				       dev->minor, DRIVER_NAME, it->options[2]);
			}
			s->range_table = &range_pci224_internal;
			devpriv->hwrange = hwrange_pci224_internal;
		}
	}

	dev->board_name = thisboard->name;

	if (irq) {
		ret = request_irq(irq, pci224_interrupt, IRQF_SHARED,
				  DRIVER_NAME, dev);
		if (ret < 0) {
			printk(KERN_ERR "comedi%d: error! "
			       "unable to allocate irq %u\n", dev->minor, irq);
			return ret;
		} else {
			dev->irq = irq;
		}
	}

	printk(KERN_INFO "comedi%d: %s ", dev->minor, dev->board_name);
	printk("(pci %s) ", pci_name(pci_dev));
	if (irq)
		printk("(irq %u%s) ", irq, (dev->irq ? "" : " UNAVAILABLE"));
	else
		printk("(no irq) ");


	printk("attached\n");

	return 1;
}

static int pci224_detach(struct comedi_device *dev)
{
	printk(KERN_DEBUG "comedi%d: %s: detach\n", dev->minor, DRIVER_NAME);

	if (dev->irq)
		free_irq(dev->irq, dev);

	if (dev->subdevices) {
		struct comedi_subdevice *s;

		s = dev->subdevices + 0;
		
		kfree(s->range_table_list);
	}
	if (devpriv) {
		kfree(devpriv->ao_readback);
		kfree(devpriv->ao_scan_vals);
		kfree(devpriv->ao_scan_order);
		if (devpriv->pci_dev) {
			if (dev->iobase)
				comedi_pci_disable(devpriv->pci_dev);

			pci_dev_put(devpriv->pci_dev);
		}
	}
	if (dev->board_name) {
		printk(KERN_INFO "comedi%d: %s removed\n",
		       dev->minor, dev->board_name);
	}

	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
