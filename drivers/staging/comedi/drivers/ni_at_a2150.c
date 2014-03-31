/*
    comedi/drivers/ni_at_a2150.c
    Driver for National Instruments AT-A2150 boards
    Copyright (C) 2001, 2002 Frank Mori Hess <fmhess@users.sourceforge.net>

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

************************************************************************
*/

#include <linux/interrupt.h>
#include <linux/slab.h>
#include "../comedidev.h"

#include <linux/ioport.h>
#include <linux/io.h>
#include <asm/dma.h>

#include "8253.h"
#include "comedi_fc.h"

#define A2150_SIZE           28
#define A2150_DMA_BUFFER_SIZE	0xff00	

#undef A2150_DEBUG		

#define CONFIG_REG		0x0
#define   CHANNEL_BITS(x)		((x) & 0x7)
#define   CHANNEL_MASK		0x7
#define   CLOCK_SELECT_BITS(x)		(((x) & 0x3) << 3)
#define   CLOCK_DIVISOR_BITS(x)		(((x) & 0x3) << 5)
#define   CLOCK_MASK		(0xf << 3)
#define   ENABLE0_BIT		0x80	
#define   ENABLE1_BIT		0x100	
#define   AC0_BIT		0x200	
#define   AC1_BIT		0x400	
#define   APD_BIT		0x800	
#define   DPD_BIT		0x1000	
#define TRIGGER_REG		0x2	
#define   POST_TRIGGER_BITS		0x2
#define   DELAY_TRIGGER_BITS		0x3
#define   HW_TRIG_EN		0x10	
#define FIFO_START_REG		0x6	
#define FIFO_RESET_REG		0x8	
#define FIFO_DATA_REG		0xa	
#define DMA_TC_CLEAR_REG		0xe	
#define STATUS_REG		0x12	
#define   FNE_BIT		0x1	
#define   OVFL_BIT		0x8	
#define   EDAQ_BIT		0x10	
#define   DCAL_BIT		0x20	
#define   INTR_BIT		0x40	
#define   DMA_TC_BIT		0x80	
#define   ID_BITS(x)	(((x) >> 8) & 0x3)
#define IRQ_DMA_CNTRL_REG		0x12	
#define   DMA_CHAN_BITS(x)		((x) & 0x7)	
#define   DMA_EN_BIT		0x8	
#define   IRQ_LVL_BITS(x)		(((x) & 0xf) << 4)	
#define   FIFO_INTR_EN_BIT		0x100	
#define   FIFO_INTR_FHF_BIT		0x200	
#define   DMA_INTR_EN_BIT 		0x800	
#define   DMA_DEM_EN_BIT	0x1000	
#define I8253_BASE_REG		0x14
#define I8253_MODE_REG		0x17
#define   HW_COUNT_DISABLE		0x30	

struct a2150_board {
	const char *name;
	int clock[4];		
	int num_clocks;		
	int ai_speed;		
};

static const struct comedi_lrange range_a2150 = {
	1,
	{
	 RANGE(-2.828, 2.828),
	 }
};

enum { a2150_c, a2150_s };
static const struct a2150_board a2150_boards[] = {
	{
	 .name = "at-a2150c",
	 .clock = {31250, 22676, 20833, 19531},
	 .num_clocks = 4,
	 .ai_speed = 19531,
	 },
	{
	 .name = "at-a2150s",
	 .clock = {62500, 50000, 41667, 0},
	 .num_clocks = 3,
	 .ai_speed = 41667,
	 },
};

#define thisboard ((const struct a2150_board *)dev->board_ptr)

struct a2150_private {

	volatile unsigned int count;	
	unsigned int dma;	
	s16 *dma_buffer;	
	unsigned int dma_transfer_size;	
	int irq_dma_bits;	
	int config_bits;	
};

#define devpriv ((struct a2150_private *)dev->private)

static int a2150_attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int a2150_detach(struct comedi_device *dev);
static int a2150_cancel(struct comedi_device *dev, struct comedi_subdevice *s);

static struct comedi_driver driver_a2150 = {
	.driver_name = "ni_at_a2150",
	.module = THIS_MODULE,
	.attach = a2150_attach,
	.detach = a2150_detach,
};

static irqreturn_t a2150_interrupt(int irq, void *d);
static int a2150_ai_cmdtest(struct comedi_device *dev,
			    struct comedi_subdevice *s, struct comedi_cmd *cmd);
static int a2150_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int a2150_ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data);
static int a2150_get_timing(struct comedi_device *dev, unsigned int *period,
			    int flags);
static int a2150_probe(struct comedi_device *dev);
static int a2150_set_chanlist(struct comedi_device *dev,
			      unsigned int start_channel,
			      unsigned int num_channels);
static int __init driver_a2150_init_module(void)
{
	return comedi_driver_register(&driver_a2150);
}

static void __exit driver_a2150_cleanup_module(void)
{
	comedi_driver_unregister(&driver_a2150);
}

module_init(driver_a2150_init_module);
module_exit(driver_a2150_cleanup_module);

#ifdef A2150_DEBUG

static void ni_dump_regs(struct comedi_device *dev)
{
	printk("config bits 0x%x\n", devpriv->config_bits);
	printk("irq dma bits 0x%x\n", devpriv->irq_dma_bits);
	printk("status bits 0x%x\n", inw(dev->iobase + STATUS_REG));
}

#endif

static irqreturn_t a2150_interrupt(int irq, void *d)
{
	int i;
	int status;
	unsigned long flags;
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->read_subdev;
	struct comedi_async *async;
	struct comedi_cmd *cmd;
	unsigned int max_points, num_points, residue, leftover;
	short dpnt;
	static const int sample_size = sizeof(devpriv->dma_buffer[0]);

	if (dev->attached == 0) {
		comedi_error(dev, "premature interrupt");
		return IRQ_HANDLED;
	}
	
	async = s->async;
	async->events = 0;
	cmd = &async->cmd;

	status = inw(dev->iobase + STATUS_REG);

	if ((status & INTR_BIT) == 0) {
		comedi_error(dev, "spurious interrupt");
		return IRQ_NONE;
	}

	if (status & OVFL_BIT) {
		comedi_error(dev, "fifo overflow");
		a2150_cancel(dev, s);
		async->events |= COMEDI_CB_ERROR | COMEDI_CB_EOA;
	}

	if ((status & DMA_TC_BIT) == 0) {
		comedi_error(dev, "caught non-dma interrupt?  Aborting.");
		a2150_cancel(dev, s);
		async->events |= COMEDI_CB_ERROR | COMEDI_CB_EOA;
		comedi_event(dev, s);
		return IRQ_HANDLED;
	}

	flags = claim_dma_lock();
	disable_dma(devpriv->dma);
	clear_dma_ff(devpriv->dma);

	
	max_points = devpriv->dma_transfer_size / sample_size;
	residue = get_dma_residue(devpriv->dma) / sample_size;
	num_points = max_points - residue;
	if (devpriv->count < num_points && cmd->stop_src == TRIG_COUNT)
		num_points = devpriv->count;

	
	leftover = 0;
	if (cmd->stop_src == TRIG_NONE) {
		leftover = devpriv->dma_transfer_size / sample_size;
	} else if (devpriv->count > max_points) {
		leftover = devpriv->count - max_points;
		if (leftover > max_points)
			leftover = max_points;
	}
	if (residue)
		leftover = 0;

	for (i = 0; i < num_points; i++) {
		
		dpnt = devpriv->dma_buffer[i];
		
		dpnt ^= 0x8000;
		cfc_write_to_buffer(s, dpnt);
		if (cmd->stop_src == TRIG_COUNT) {
			if (--devpriv->count == 0) {	
				a2150_cancel(dev, s);
				async->events |= COMEDI_CB_EOA;
				break;
			}
		}
	}
	
	if (leftover) {
		set_dma_addr(devpriv->dma, virt_to_bus(devpriv->dma_buffer));
		set_dma_count(devpriv->dma, leftover * sample_size);
		enable_dma(devpriv->dma);
	}
	release_dma_lock(flags);

	async->events |= COMEDI_CB_BLOCK;

	comedi_event(dev, s);

	
	outw(0x00, dev->iobase + DMA_TC_CLEAR_REG);

	return IRQ_HANDLED;
}

static int a2150_probe(struct comedi_device *dev)
{
	int status = inw(dev->iobase + STATUS_REG);
	return ID_BITS(status);
}

static int a2150_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	unsigned long iobase = it->options[0];
	unsigned int irq = it->options[1];
	unsigned int dma = it->options[2];
	static const int timeout = 2000;
	int i;

	printk("comedi%d: %s: io 0x%lx", dev->minor, driver_a2150.driver_name,
	       iobase);
	if (irq) {
		printk(", irq %u", irq);
	} else {
		printk(", no irq");
	}
	if (dma) {
		printk(", dma %u", dma);
	} else {
		printk(", no dma");
	}
	printk("\n");

	
	if (alloc_private(dev, sizeof(struct a2150_private)) < 0)
		return -ENOMEM;

	if (iobase == 0) {
		printk(" io base address required\n");
		return -EINVAL;
	}

	
	if (!request_region(iobase, A2150_SIZE, driver_a2150.driver_name)) {
		printk(" I/O port conflict\n");
		return -EIO;
	}
	dev->iobase = iobase;

	
	if (irq) {
		
		if (irq < 3 || irq == 8 || irq == 13 || irq > 15) {
			printk(" invalid irq line %u\n", irq);
			return -EINVAL;
		}
		if (request_irq(irq, a2150_interrupt, 0,
				driver_a2150.driver_name, dev)) {
			printk("unable to allocate irq %u\n", irq);
			return -EINVAL;
		}
		devpriv->irq_dma_bits |= IRQ_LVL_BITS(irq);
		dev->irq = irq;
	}
	
	if (dma) {
		if (dma == 4 || dma > 7) {
			printk(" invalid dma channel %u\n", dma);
			return -EINVAL;
		}
		if (request_dma(dma, driver_a2150.driver_name)) {
			printk(" failed to allocate dma channel %u\n", dma);
			return -EINVAL;
		}
		devpriv->dma = dma;
		devpriv->dma_buffer =
		    kmalloc(A2150_DMA_BUFFER_SIZE, GFP_KERNEL | GFP_DMA);
		if (devpriv->dma_buffer == NULL)
			return -ENOMEM;

		disable_dma(dma);
		set_dma_mode(dma, DMA_MODE_READ);

		devpriv->irq_dma_bits |= DMA_CHAN_BITS(dma);
	}

	dev->board_ptr = a2150_boards + a2150_probe(dev);
	dev->board_name = thisboard->name;

	if (alloc_subdevices(dev, 1) < 0)
		return -ENOMEM;

	
	s = dev->subdevices + 0;
	dev->read_subdev = s;
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_OTHER | SDF_CMD_READ;
	s->n_chan = 4;
	s->len_chanlist = 4;
	s->maxdata = 0xffff;
	s->range_table = &range_a2150;
	s->do_cmd = a2150_ai_cmd;
	s->do_cmdtest = a2150_ai_cmdtest;
	s->insn_read = a2150_ai_rinsn;
	s->cancel = a2150_cancel;

	outw(HW_COUNT_DISABLE, dev->iobase + I8253_MODE_REG);

	
	outw(devpriv->irq_dma_bits, dev->iobase + IRQ_DMA_CNTRL_REG);

	
	outw_p(DPD_BIT | APD_BIT, dev->iobase + CONFIG_REG);
	outw_p(DPD_BIT, dev->iobase + CONFIG_REG);
	
	devpriv->config_bits = 0;
	outw(devpriv->config_bits, dev->iobase + CONFIG_REG);
	
	for (i = 0; i < timeout; i++) {
		if ((DCAL_BIT & inw(dev->iobase + STATUS_REG)) == 0)
			break;
		udelay(1000);
	}
	if (i == timeout) {
		printk
		    (" timed out waiting for offset calibration to complete\n");
		return -ETIME;
	}
	devpriv->config_bits |= ENABLE0_BIT | ENABLE1_BIT;
	outw(devpriv->config_bits, dev->iobase + CONFIG_REG);

	return 0;
};

static int a2150_detach(struct comedi_device *dev)
{
	printk("comedi%d: %s: remove\n", dev->minor, driver_a2150.driver_name);

	
	if (dev->iobase) {
		
		outw(APD_BIT | DPD_BIT, dev->iobase + CONFIG_REG);
		release_region(dev->iobase, A2150_SIZE);
	}

	if (dev->irq)
		free_irq(dev->irq, dev);
	if (devpriv) {
		if (devpriv->dma)
			free_dma(devpriv->dma);
		kfree(devpriv->dma_buffer);
	}

	return 0;
};

static int a2150_cancel(struct comedi_device *dev, struct comedi_subdevice *s)
{
	
	devpriv->irq_dma_bits &= ~DMA_INTR_EN_BIT & ~DMA_EN_BIT;
	outw(devpriv->irq_dma_bits, dev->iobase + IRQ_DMA_CNTRL_REG);

	
	disable_dma(devpriv->dma);

	
	outw(0, dev->iobase + FIFO_RESET_REG);

	return 0;
}

static int a2150_ai_cmdtest(struct comedi_device *dev,
			    struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;
	int startChan;
	int i;

	

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_EXT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER;
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

	

	if (cmd->start_src != TRIG_NOW && cmd->start_src != TRIG_EXT)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}
	if (cmd->convert_src == TRIG_TIMER) {
		if (cmd->convert_arg < thisboard->ai_speed) {
			cmd->convert_arg = thisboard->ai_speed;
			err++;
		}
	}
	if (!cmd->chanlist_len) {
		cmd->chanlist_len = 1;
		err++;
	}
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
	if (cmd->stop_src == TRIG_COUNT) {
		if (!cmd->stop_arg) {
			cmd->stop_arg = 1;
			err++;
		}
	} else {		
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	

	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		a2150_get_timing(dev, &cmd->scan_begin_arg, cmd->flags);
		if (tmp != cmd->scan_begin_arg)
			err++;
	}

	if (err)
		return 4;

	
	if (cmd->chanlist) {
		startChan = CR_CHAN(cmd->chanlist[0]);
		for (i = 1; i < cmd->chanlist_len; i++) {
			if (CR_CHAN(cmd->chanlist[i]) != (startChan + i)) {
				comedi_error(dev,
					     "entries in chanlist must be consecutive channels, counting upwards\n");
				err++;
			}
		}
		if (cmd->chanlist_len == 2 && CR_CHAN(cmd->chanlist[0]) == 1) {
			comedi_error(dev,
				     "length 2 chanlist must be channels 0,1 or channels 2,3");
			err++;
		}
		if (cmd->chanlist_len == 3) {
			comedi_error(dev,
				     "chanlist must have 1,2 or 4 channels");
			err++;
		}
		if (CR_AREF(cmd->chanlist[0]) != CR_AREF(cmd->chanlist[1]) ||
		    CR_AREF(cmd->chanlist[2]) != CR_AREF(cmd->chanlist[3])) {
			comedi_error(dev,
				     "channels 0/1 and 2/3 must have the same analog reference");
			err++;
		}
	}

	if (err)
		return 5;

	return 0;
}

static int a2150_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned long lock_flags;
	unsigned int old_config_bits = devpriv->config_bits;
	unsigned int trigger_bits;

	if (!dev->irq || !devpriv->dma) {
		comedi_error(dev,
			     " irq and dma required, cannot do hardware conversions");
		return -1;
	}
	if (cmd->flags & TRIG_RT) {
		comedi_error(dev,
			     " dma incompatible with hard real-time interrupt (TRIG_RT), aborting");
		return -1;
	}
	
	outw(0, dev->iobase + FIFO_RESET_REG);

	
	if (a2150_set_chanlist(dev, CR_CHAN(cmd->chanlist[0]),
			       cmd->chanlist_len) < 0)
		return -1;

	
	if (CR_AREF(cmd->chanlist[0]) == AREF_OTHER)
		devpriv->config_bits |= AC0_BIT;
	else
		devpriv->config_bits &= ~AC0_BIT;
	if (CR_AREF(cmd->chanlist[2]) == AREF_OTHER)
		devpriv->config_bits |= AC1_BIT;
	else
		devpriv->config_bits &= ~AC1_BIT;

	
	a2150_get_timing(dev, &cmd->scan_begin_arg, cmd->flags);

	
	outw(devpriv->config_bits, dev->iobase + CONFIG_REG);

	
	devpriv->count = cmd->stop_arg * cmd->chanlist_len;

	
	lock_flags = claim_dma_lock();
	disable_dma(devpriv->dma);
	clear_dma_ff(devpriv->dma);
	set_dma_addr(devpriv->dma, virt_to_bus(devpriv->dma_buffer));
	
#define ONE_THIRD_SECOND 333333333
	devpriv->dma_transfer_size =
	    sizeof(devpriv->dma_buffer[0]) * cmd->chanlist_len *
	    ONE_THIRD_SECOND / cmd->scan_begin_arg;
	if (devpriv->dma_transfer_size > A2150_DMA_BUFFER_SIZE)
		devpriv->dma_transfer_size = A2150_DMA_BUFFER_SIZE;
	if (devpriv->dma_transfer_size < sizeof(devpriv->dma_buffer[0]))
		devpriv->dma_transfer_size = sizeof(devpriv->dma_buffer[0]);
	devpriv->dma_transfer_size -=
	    devpriv->dma_transfer_size % sizeof(devpriv->dma_buffer[0]);
	set_dma_count(devpriv->dma, devpriv->dma_transfer_size);
	enable_dma(devpriv->dma);
	release_dma_lock(lock_flags);

	outw(0x00, dev->iobase + DMA_TC_CLEAR_REG);

	
	devpriv->irq_dma_bits |= DMA_INTR_EN_BIT | DMA_EN_BIT;
	outw(devpriv->irq_dma_bits, dev->iobase + IRQ_DMA_CNTRL_REG);

	
	i8254_load(dev->iobase + I8253_BASE_REG, 0, 2, 72, 0);

	
	trigger_bits = 0;
	
	if (cmd->start_src == TRIG_NOW &&
	    (old_config_bits & CLOCK_MASK) !=
	    (devpriv->config_bits & CLOCK_MASK)) {
		
		trigger_bits |= DELAY_TRIGGER_BITS;
	} else {
		
		trigger_bits |= POST_TRIGGER_BITS;
	}
	
	if (cmd->start_src == TRIG_EXT) {
		trigger_bits |= HW_TRIG_EN;
	} else if (cmd->start_src == TRIG_OTHER) {
		
		comedi_error(dev, "you shouldn't see this?");
	}
	
	outw(trigger_bits, dev->iobase + TRIGGER_REG);

	
	if (cmd->start_src == TRIG_NOW)
		outw(0, dev->iobase + FIFO_START_REG);
#ifdef A2150_DEBUG
	ni_dump_regs(dev);
#endif

	return 0;
}

static int a2150_ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_insn *insn, unsigned int *data)
{
	unsigned int i, n;
	static const int timeout = 100000;
	static const int filter_delay = 36;

	
	outw(0, dev->iobase + FIFO_RESET_REG);

	
	if (a2150_set_chanlist(dev, CR_CHAN(insn->chanspec), 1) < 0)
		return -1;

	
	devpriv->config_bits &= ~AC0_BIT;
	devpriv->config_bits &= ~AC1_BIT;

	
	outw(devpriv->config_bits, dev->iobase + CONFIG_REG);

	
	devpriv->irq_dma_bits &= ~DMA_INTR_EN_BIT & ~DMA_EN_BIT;
	outw(devpriv->irq_dma_bits, dev->iobase + IRQ_DMA_CNTRL_REG);

	
	outw(0, dev->iobase + TRIGGER_REG);

	
	outw(0, dev->iobase + FIFO_START_REG);

	
	for (n = 0; n < filter_delay; n++) {
		for (i = 0; i < timeout; i++) {
			if (inw(dev->iobase + STATUS_REG) & FNE_BIT)
				break;
			udelay(1);
		}
		if (i == timeout) {
			comedi_error(dev, "timeout");
			return -ETIME;
		}
		inw(dev->iobase + FIFO_DATA_REG);
	}

	
	for (n = 0; n < insn->n; n++) {
		for (i = 0; i < timeout; i++) {
			if (inw(dev->iobase + STATUS_REG) & FNE_BIT)
				break;
			udelay(1);
		}
		if (i == timeout) {
			comedi_error(dev, "timeout");
			return -ETIME;
		}
#ifdef A2150_DEBUG
		ni_dump_regs(dev);
#endif
		data[n] = inw(dev->iobase + FIFO_DATA_REG);
#ifdef A2150_DEBUG
		printk(" data is %i\n", data[n]);
#endif
		data[n] ^= 0x8000;
	}

	
	outw(0, dev->iobase + FIFO_RESET_REG);

	return n;
}

static int a2150_get_timing(struct comedi_device *dev, unsigned int *period,
			    int flags)
{
	int lub, glb, temp;
	int lub_divisor_shift, lub_index, glb_divisor_shift, glb_index;
	int i, j;

	
	lub_divisor_shift = 3;
	lub_index = 0;
	lub = thisboard->clock[lub_index] * (1 << lub_divisor_shift);
	glb_divisor_shift = 0;
	glb_index = thisboard->num_clocks - 1;
	glb = thisboard->clock[glb_index] * (1 << glb_divisor_shift);

	
	if (*period < glb)
		*period = glb;
	if (*period > lub)
		*period = lub;

	
	for (i = 0; i < 4; i++) {
		
		for (j = 0; j < thisboard->num_clocks; j++) {
			
			temp = thisboard->clock[j] * (1 << i);
			
			if (temp < lub && temp >= *period) {
				lub_divisor_shift = i;
				lub_index = j;
				lub = temp;
			}
			if (temp > glb && temp <= *period) {
				glb_divisor_shift = i;
				glb_index = j;
				glb = temp;
			}
		}
	}
	flags &= TRIG_ROUND_MASK;
	switch (flags) {
	case TRIG_ROUND_NEAREST:
	default:
		
		if (lub - *period < *period - glb)
			*period = lub;
		else
			*period = glb;
		break;
	case TRIG_ROUND_UP:
		*period = lub;
		break;
	case TRIG_ROUND_DOWN:
		*period = glb;
		break;
	}

	
	devpriv->config_bits &= ~CLOCK_MASK;
	if (*period == lub) {
		devpriv->config_bits |=
		    CLOCK_SELECT_BITS(lub_index) |
		    CLOCK_DIVISOR_BITS(lub_divisor_shift);
	} else {
		devpriv->config_bits |=
		    CLOCK_SELECT_BITS(glb_index) |
		    CLOCK_DIVISOR_BITS(glb_divisor_shift);
	}

	return 0;
}

static int a2150_set_chanlist(struct comedi_device *dev,
			      unsigned int start_channel,
			      unsigned int num_channels)
{
	if (start_channel + num_channels > 4)
		return -1;

	devpriv->config_bits &= ~CHANNEL_MASK;

	switch (num_channels) {
	case 1:
		devpriv->config_bits |= CHANNEL_BITS(0x4 | start_channel);
		break;
	case 2:
		if (start_channel == 0) {
			devpriv->config_bits |= CHANNEL_BITS(0x2);
		} else if (start_channel == 2) {
			devpriv->config_bits |= CHANNEL_BITS(0x3);
		} else {
			return -1;
		}
		break;
	case 4:
		devpriv->config_bits |= CHANNEL_BITS(0x1);
		break;
	default:
		return -1;
		break;
	}

	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
