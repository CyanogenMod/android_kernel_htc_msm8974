/*
 *      Low-level parallel-support for PC-style hardware integrated in the 
 *	LASI-Controller (on GSC-Bus) for HP-PARISC Workstations
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *	(C) 1999-2001 by Helge Deller <deller@gmx.de>
 *
 * 
 * based on parport_pc.c by 
 * 	    Grant Guenther <grant@torque.net>
 * 	    Phil Blundell <philb@gnu.org>
 *          Tim Waugh <tim@cyberelk.demon.co.uk>
 *	    Jose Renau <renau@acm.org>
 *          David Campbell
 *          Andrea Arcangeli
 */

#undef DEBUG	

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/sysctl.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/uaccess.h>
#include <asm/superio.h>

#include <linux/parport.h>
#include <asm/pdc.h>
#include <asm/parisc-device.h>
#include <asm/hardware.h>
#include "parport_gsc.h"


MODULE_AUTHOR("Helge Deller <deller@gmx.de>");
MODULE_DESCRIPTION("HP-PARISC PC-style parallel port driver");
MODULE_SUPPORTED_DEVICE("integrated PC-style parallel port");
MODULE_LICENSE("GPL");


static int clear_epp_timeout(struct parport *pb)
{
	unsigned char r;

	if (!(parport_gsc_read_status(pb) & 0x01))
		return 1;

	
	parport_gsc_read_status(pb);
	r = parport_gsc_read_status(pb);
	parport_writeb (r | 0x01, STATUS (pb)); 
	parport_writeb (r & 0xfe, STATUS (pb)); 
	r = parport_gsc_read_status(pb);

	return !(r & 0x01);
}


void parport_gsc_init_state(struct pardevice *dev, struct parport_state *s)
{
	s->u.pc.ctr = 0xc | (dev->irq_func ? 0x10 : 0x0);
}

void parport_gsc_save_state(struct parport *p, struct parport_state *s)
{
	s->u.pc.ctr = parport_readb (CONTROL (p));
}

void parport_gsc_restore_state(struct parport *p, struct parport_state *s)
{
	parport_writeb (s->u.pc.ctr, CONTROL (p));
}

struct parport_operations parport_gsc_ops = 
{
	.write_data	= parport_gsc_write_data,
	.read_data	= parport_gsc_read_data,

	.write_control	= parport_gsc_write_control,
	.read_control	= parport_gsc_read_control,
	.frob_control	= parport_gsc_frob_control,

	.read_status	= parport_gsc_read_status,

	.enable_irq	= parport_gsc_enable_irq,
	.disable_irq	= parport_gsc_disable_irq,

	.data_forward	= parport_gsc_data_forward,
	.data_reverse	= parport_gsc_data_reverse,

	.init_state	= parport_gsc_init_state,
	.save_state	= parport_gsc_save_state,
	.restore_state	= parport_gsc_restore_state,

	.epp_write_data	= parport_ieee1284_epp_write_data,
	.epp_read_data	= parport_ieee1284_epp_read_data,
	.epp_write_addr	= parport_ieee1284_epp_write_addr,
	.epp_read_addr	= parport_ieee1284_epp_read_addr,

	.ecp_write_data	= parport_ieee1284_ecp_write_data,
	.ecp_read_data	= parport_ieee1284_ecp_read_data,
	.ecp_write_addr	= parport_ieee1284_ecp_write_addr,

	.compat_write_data 	= parport_ieee1284_write_compat,
	.nibble_read_data	= parport_ieee1284_read_nibble,
	.byte_read_data		= parport_ieee1284_read_byte,

	.owner		= THIS_MODULE,
};


static int __devinit parport_SPP_supported(struct parport *pb)
{
	unsigned char r, w;

	clear_epp_timeout(pb);

	
	w = 0xc;
	parport_writeb (w, CONTROL (pb));

	r = parport_readb (CONTROL (pb));
	if ((r & 0xf) == w) {
		w = 0xe;
		parport_writeb (w, CONTROL (pb));
		r = parport_readb (CONTROL (pb));
		parport_writeb (0xc, CONTROL (pb));
		if ((r & 0xf) == w)
			return PARPORT_MODE_PCSPP;
	}

	w = 0xaa;
	parport_gsc_write_data (pb, w);
	r = parport_gsc_read_data (pb);
	if (r == w) {
		w = 0x55;
		parport_gsc_write_data (pb, w);
		r = parport_gsc_read_data (pb);
		if (r == w)
			return PARPORT_MODE_PCSPP;
	}

	return 0;
}


static int __devinit parport_PS2_supported(struct parport *pb)
{
	int ok = 0;
  
	clear_epp_timeout(pb);

	
	parport_gsc_data_reverse (pb);
	
	parport_gsc_write_data(pb, 0x55);
	if (parport_gsc_read_data(pb) != 0x55) ok++;

	parport_gsc_write_data(pb, 0xaa);
	if (parport_gsc_read_data(pb) != 0xaa) ok++;

	
	parport_gsc_data_forward (pb);

	if (ok) {
		pb->modes |= PARPORT_MODE_TRISTATE;
	} else {
		struct parport_gsc_private *priv = pb->private_data;
		priv->ctr_writable &= ~0x20;
	}

	return ok;
}



struct parport *__devinit parport_gsc_probe_port (unsigned long base,
						 unsigned long base_hi,
						 int irq, int dma,
						 struct pci_dev *dev)
{
	struct parport_gsc_private *priv;
	struct parport_operations *ops;
	struct parport tmp;
	struct parport *p = &tmp;

	priv = kzalloc (sizeof (struct parport_gsc_private), GFP_KERNEL);
	if (!priv) {
		printk (KERN_DEBUG "parport (0x%lx): no memory!\n", base);
		return NULL;
	}
	ops = kmalloc (sizeof (struct parport_operations), GFP_KERNEL);
	if (!ops) {
		printk (KERN_DEBUG "parport (0x%lx): no memory for ops!\n",
			base);
		kfree (priv);
		return NULL;
	}
	memcpy (ops, &parport_gsc_ops, sizeof (struct parport_operations));
	priv->ctr = 0xc;
	priv->ctr_writable = 0xff;
	priv->dma_buf = 0;
	priv->dma_handle = 0;
	priv->dev = dev;
	p->base = base;
	p->base_hi = base_hi;
	p->irq = irq;
	p->dma = dma;
	p->modes = PARPORT_MODE_PCSPP | PARPORT_MODE_SAFEININT;
	p->ops = ops;
	p->private_data = priv;
	p->physport = p;
	if (!parport_SPP_supported (p)) {
		
		kfree (priv);
		return NULL;
	}
	parport_PS2_supported (p);

	if (!(p = parport_register_port(base, PARPORT_IRQ_NONE,
					PARPORT_DMA_NONE, ops))) {
		kfree (priv);
		kfree (ops);
		return NULL;
	}

	p->base_hi = base_hi;
	p->modes = tmp.modes;
	p->size = (p->modes & PARPORT_MODE_EPP)?8:3;
	p->private_data = priv;

	printk(KERN_INFO "%s: PC-style at 0x%lx", p->name, p->base);
	p->irq = irq;
	if (p->irq == PARPORT_IRQ_AUTO) {
		p->irq = PARPORT_IRQ_NONE;
	}
	if (p->irq != PARPORT_IRQ_NONE) {
		printk(", irq %d", p->irq);

		if (p->dma == PARPORT_DMA_AUTO) {
			p->dma = PARPORT_DMA_NONE;
		}
	}
	if (p->dma == PARPORT_DMA_AUTO) 
		p->dma = PARPORT_DMA_NONE;

	printk(" [");
#define printmode(x) {if(p->modes&PARPORT_MODE_##x){printk("%s%s",f?",":"",#x);f++;}}
	{
		int f = 0;
		printmode(PCSPP);
		printmode(TRISTATE);
		printmode(COMPAT)
		printmode(EPP);
	}
#undef printmode
	printk("]\n");

	if (p->irq != PARPORT_IRQ_NONE) {
		if (request_irq (p->irq, parport_irq_handler,
				 0, p->name, p)) {
			printk (KERN_WARNING "%s: irq %d in use, "
				"resorting to polled operation\n",
				p->name, p->irq);
			p->irq = PARPORT_IRQ_NONE;
			p->dma = PARPORT_DMA_NONE;
		}
	}

	

	parport_gsc_write_data(p, 0);
	parport_gsc_data_forward (p);

	parport_announce_port (p);

	return p;
}


#define PARPORT_GSC_OFFSET 0x800

static int __devinitdata parport_count;

static int __devinit parport_init_chip(struct parisc_device *dev)
{
	struct parport *p;
	unsigned long port;

	if (!dev->irq) {
		printk(KERN_WARNING "IRQ not found for parallel device at 0x%llx\n",
			(unsigned long long)dev->hpa.start);
		return -ENODEV;
	}

	port = dev->hpa.start + PARPORT_GSC_OFFSET;
	
	if (boot_cpu_data.cpu_type > pcxt && !pdc_add_valid(port+4)) {

		
		printk("%s: initialize bidirectional-mode.\n", __func__);
		parport_writeb ( (0x10 + 0x20), port + 4);

	} else {
		printk("%s: enhanced parport-modes not supported.\n", __func__);
	}
	
	p = parport_gsc_probe_port(port, 0, dev->irq,
			 PARPORT_DMA_NONE, NULL);
	if (p)
		parport_count++;
	dev_set_drvdata(&dev->dev, p);

	return 0;
}

static int __devexit parport_remove_chip(struct parisc_device *dev)
{
	struct parport *p = dev_get_drvdata(&dev->dev);
	if (p) {
		struct parport_gsc_private *priv = p->private_data;
		struct parport_operations *ops = p->ops;
		parport_remove_port(p);
		if (p->dma != PARPORT_DMA_NONE)
			free_dma(p->dma);
		if (p->irq != PARPORT_IRQ_NONE)
			free_irq(p->irq, p);
		if (priv->dma_buf)
			pci_free_consistent(priv->dev, PAGE_SIZE,
					    priv->dma_buf,
					    priv->dma_handle);
		kfree (p->private_data);
		parport_put_port(p);
		kfree (ops); 
	}
	return 0;
}

static struct parisc_device_id parport_tbl[] = {
	{ HPHW_FIO, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x74 },
	{ 0, }
};

MODULE_DEVICE_TABLE(parisc, parport_tbl);

static struct parisc_driver parport_driver = {
	.name		= "Parallel",
	.id_table	= parport_tbl,
	.probe		= parport_init_chip,
	.remove		= __devexit_p(parport_remove_chip),
};

int __devinit parport_gsc_init(void)
{
	return register_parisc_driver(&parport_driver);
}

static void __devexit parport_gsc_exit(void)
{
	unregister_parisc_driver(&parport_driver);
}

module_init(parport_gsc_init);
module_exit(parport_gsc_exit);
