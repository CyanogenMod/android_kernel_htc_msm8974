/*********************************************************************
 *                
 * Filename:      nsc-ircc.c
 * Version:       1.0
 * Description:   Driver for the NSC PC'108 and PC'338 IrDA chipsets
 * Status:        Stable.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sat Nov  7 21:43:15 1998
 * Modified at:   Wed Mar  1 11:29:34 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-2000 Dag Brattli <dagb@cs.uit.no>
 *     Copyright (c) 1998 Lichen Wang, <lwang@actisys.com>
 *     Copyright (c) 1998 Actisys Corp., www.actisys.com
 *     Copyright (c) 2000-2004 Jean Tourrilhes <jt@hpl.hp.com>
 *     All Rights Reserved
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *
 *     Notice that all functions that needs to access the chip in _any_
 *     way, must save BSR register on entry, and restore it on exit. 
 *     It is _very_ important to follow this policy!
 *
 *         __u8 bank;
 *     
 *         bank = inb(iobase+BSR);
 *  
 *         do_your_stuff_here();
 *
 *         outb(bank, iobase+BSR);
 *
 *    If you find bugs in this file, its very likely that the same bug
 *    will also be in w83977af_ir.c since the implementations are quite
 *    similar.
 *     
 ********************************************************************/

#include <linux/module.h>
#include <linux/gfp.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/rtnetlink.h>
#include <linux/dma-mapping.h>
#include <linux/pnp.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/byteorder.h>

#include <net/irda/wrapper.h>
#include <net/irda/irda.h>
#include <net/irda/irda_device.h>

#include "nsc-ircc.h"

#define CHIP_IO_EXTENT 8
#define BROKEN_DONGLE_ID

static char *driver_name = "nsc-ircc";

#define NSC_IRCC_DRIVER_NAME                  "nsc-ircc"
static int nsc_ircc_suspend(struct platform_device *dev, pm_message_t state);
static int nsc_ircc_resume(struct platform_device *dev);

static struct platform_driver nsc_ircc_driver = {
	.suspend	= nsc_ircc_suspend,
	.resume		= nsc_ircc_resume,
	.driver		= {
		.name	= NSC_IRCC_DRIVER_NAME,
	},
};

static int qos_mtt_bits = 0x07;  
static int dongle_id;

static unsigned int io[]  = { ~0, ~0, ~0, ~0, ~0 };
static unsigned int irq[] = {  0,  0,  0,  0,  0 };
static unsigned int dma[] = {  0,  0,  0,  0,  0 };

static int nsc_ircc_probe_108(nsc_chip_t *chip, chipio_t *info);
static int nsc_ircc_probe_338(nsc_chip_t *chip, chipio_t *info);
static int nsc_ircc_probe_39x(nsc_chip_t *chip, chipio_t *info);
static int nsc_ircc_init_108(nsc_chip_t *chip, chipio_t *info);
static int nsc_ircc_init_338(nsc_chip_t *chip, chipio_t *info);
static int nsc_ircc_init_39x(nsc_chip_t *chip, chipio_t *info);
#ifdef CONFIG_PNP
static int nsc_ircc_pnp_probe(struct pnp_dev *dev, const struct pnp_device_id *id);
#endif

static nsc_chip_t chips[] = {
	{ "PC87108", { 0x150, 0x398, 0xea }, 0x05, 0x10, 0xf0, 
	  nsc_ircc_probe_108, nsc_ircc_init_108 },
	{ "PC87338", { 0x398, 0x15c, 0x2e }, 0x08, 0xb0, 0xf8, 
	  nsc_ircc_probe_338, nsc_ircc_init_338 },
	
	{ "PC8738x", { 0x164e, 0x4e, 0x2e }, 0x20, 0xf4, 0xff,
	  nsc_ircc_probe_39x, nsc_ircc_init_39x },
	
	{ "PC8739x", { 0x2e, 0x4e, 0x0 }, 0x20, 0xea, 0xff, 
	  nsc_ircc_probe_39x, nsc_ircc_init_39x },
	
	{ "IBM-PC8738x", { 0x2e, 0x4e, 0x0 }, 0x20, 0xf4, 0xff,
	  nsc_ircc_probe_39x, nsc_ircc_init_39x },
	
	{ "IBM-PC8394T", { 0x2e, 0x4e, 0x0 }, 0x20, 0xf9, 0xff,
	  nsc_ircc_probe_39x, nsc_ircc_init_39x },
	{ NULL }
};

static struct nsc_ircc_cb *dev_self[] = { NULL, NULL, NULL, NULL, NULL };

static char *dongle_types[] = {
	"Differential serial interface",
	"Differential serial interface",
	"Reserved",
	"Reserved",
	"Sharp RY5HD01",
	"Reserved",
	"Single-ended serial interface",
	"Consumer-IR only",
	"HP HSDL-2300, HP HSDL-3600/HSDL-3610",
	"IBM31T1100 or Temic TFDS6000/TFDS6500",
	"Reserved",
	"Reserved",
	"HP HSDL-1100/HSDL-2100",
	"HP HSDL-1100/HSDL-2100",
	"Supports SIR Mode only",
	"No dongle connected",
};

static chipio_t pnp_info;
static const struct pnp_device_id nsc_ircc_pnp_table[] = {
	{ .id = "NSC6001", .driver_data = 0 },
	{ .id = "HWPC224", .driver_data = 0 },
	{ .id = "IBM0071", .driver_data = NSC_FORCE_DONGLE_TYPE9 },
	{ }
};

MODULE_DEVICE_TABLE(pnp, nsc_ircc_pnp_table);

static struct pnp_driver nsc_ircc_pnp_driver = {
#ifdef CONFIG_PNP
	.name = "nsc-ircc",
	.id_table = nsc_ircc_pnp_table,
	.probe = nsc_ircc_pnp_probe,
#endif
};

static int  nsc_ircc_open(chipio_t *info);
static int  nsc_ircc_close(struct nsc_ircc_cb *self);
static int  nsc_ircc_setup(chipio_t *info);
static void nsc_ircc_pio_receive(struct nsc_ircc_cb *self);
static int  nsc_ircc_dma_receive(struct nsc_ircc_cb *self); 
static int  nsc_ircc_dma_receive_complete(struct nsc_ircc_cb *self, int iobase);
static netdev_tx_t  nsc_ircc_hard_xmit_sir(struct sk_buff *skb,
						 struct net_device *dev);
static netdev_tx_t  nsc_ircc_hard_xmit_fir(struct sk_buff *skb,
						 struct net_device *dev);
static int  nsc_ircc_pio_write(int iobase, __u8 *buf, int len, int fifo_size);
static void nsc_ircc_dma_xmit(struct nsc_ircc_cb *self, int iobase);
static __u8 nsc_ircc_change_speed(struct nsc_ircc_cb *self, __u32 baud);
static int  nsc_ircc_is_receiving(struct nsc_ircc_cb *self);
static int  nsc_ircc_read_dongle_id (int iobase);
static void nsc_ircc_init_dongle_interface (int iobase, int dongle_id);

static int  nsc_ircc_net_open(struct net_device *dev);
static int  nsc_ircc_net_close(struct net_device *dev);
static int  nsc_ircc_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);

static int pnp_registered;
static int pnp_succeeded;

static int __init nsc_ircc_init(void)
{
	chipio_t info;
	nsc_chip_t *chip;
	int ret;
	int cfg_base;
	int cfg, id;
	int reg;
	int i = 0;

	ret = platform_driver_register(&nsc_ircc_driver);
        if (ret) {
                IRDA_ERROR("%s, Can't register driver!\n", driver_name);
                return ret;
        }

 	
	ret = pnp_register_driver(&nsc_ircc_pnp_driver);

 	if (!ret)
 		pnp_registered = 1;

	ret = -ENODEV;

	
	for (chip = chips; chip->name ; chip++) {
		IRDA_DEBUG(2, "%s(), Probing for %s ...\n", __func__,
			   chip->name);
		
		
		for (cfg = 0; cfg < ARRAY_SIZE(chip->cfg); cfg++) {
			cfg_base = chip->cfg[cfg];
			if (!cfg_base)
				continue;

			
			reg = inb(cfg_base);
			if (reg == 0xff) {
				IRDA_DEBUG(2, "%s() no chip at 0x%03x\n", __func__, cfg_base);
				continue;
			}
			
			
			outb(chip->cid_index, cfg_base);
			id = inb(cfg_base+1);
			if ((id & chip->cid_mask) == chip->cid_value) {
				IRDA_DEBUG(2, "%s() Found %s chip, revision=%d\n",
					   __func__, chip->name, id & ~chip->cid_mask);

				if (pnp_succeeded) {
					memset(&info, 0, sizeof(chipio_t));
					info.cfg_base = cfg_base;
					info.fir_base = pnp_info.fir_base;
					info.dma = pnp_info.dma;
					info.irq = pnp_info.irq;

					if (info.fir_base < 0x2000) {
						IRDA_MESSAGE("%s, chip->init\n", driver_name);
						chip->init(chip, &info);
					} else
						chip->probe(chip, &info);

					if (nsc_ircc_open(&info) >= 0)
						ret = 0;
				}

				if (ret) {
					IRDA_DEBUG(2, "%s, PnP init failed\n", driver_name);
					memset(&info, 0, sizeof(chipio_t));
					info.cfg_base = cfg_base;
					info.fir_base = io[i];
					info.dma = dma[i];
					info.irq = irq[i];

					if (io[i] < 0x2000) {
						chip->init(chip, &info);
					} else
						chip->probe(chip, &info);

					if (nsc_ircc_open(&info) >= 0)
						ret = 0;
				}
				i++;
			} else {
				IRDA_DEBUG(2, "%s(), Wrong chip id=0x%02x\n", __func__, id);
			}
		} 
	}

	if (ret) {
		platform_driver_unregister(&nsc_ircc_driver);
		pnp_unregister_driver(&nsc_ircc_pnp_driver);
		pnp_registered = 0;
	}

	return ret;
}

static void __exit nsc_ircc_cleanup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dev_self); i++) {
		if (dev_self[i])
			nsc_ircc_close(dev_self[i]);
	}

	platform_driver_unregister(&nsc_ircc_driver);

	if (pnp_registered)
 		pnp_unregister_driver(&nsc_ircc_pnp_driver);

	pnp_registered = 0;
}

static const struct net_device_ops nsc_ircc_sir_ops = {
	.ndo_open       = nsc_ircc_net_open,
	.ndo_stop       = nsc_ircc_net_close,
	.ndo_start_xmit = nsc_ircc_hard_xmit_sir,
	.ndo_do_ioctl   = nsc_ircc_net_ioctl,
};

static const struct net_device_ops nsc_ircc_fir_ops = {
	.ndo_open       = nsc_ircc_net_open,
	.ndo_stop       = nsc_ircc_net_close,
	.ndo_start_xmit = nsc_ircc_hard_xmit_fir,
	.ndo_do_ioctl   = nsc_ircc_net_ioctl,
};

static int __init nsc_ircc_open(chipio_t *info)
{
	struct net_device *dev;
	struct nsc_ircc_cb *self;
	void *ret;
	int err, chip_index;

	IRDA_DEBUG(2, "%s()\n", __func__);


 	for (chip_index = 0; chip_index < ARRAY_SIZE(dev_self); chip_index++) {
		if (!dev_self[chip_index])
			break;
	}

	if (chip_index == ARRAY_SIZE(dev_self)) {
		IRDA_ERROR("%s(), maximum number of supported chips reached!\n", __func__);
		return -ENOMEM;
	}

	IRDA_MESSAGE("%s, Found chip at base=0x%03x\n", driver_name,
		     info->cfg_base);

	if ((nsc_ircc_setup(info)) == -1)
		return -1;

	IRDA_MESSAGE("%s, driver loaded (Dag Brattli)\n", driver_name);

	dev = alloc_irdadev(sizeof(struct nsc_ircc_cb));
	if (dev == NULL) {
		IRDA_ERROR("%s(), can't allocate memory for "
			   "control block!\n", __func__);
		return -ENOMEM;
	}

	self = netdev_priv(dev);
	self->netdev = dev;
	spin_lock_init(&self->lock);
   
	
	dev_self[chip_index] = self;
	self->index = chip_index;

	
	self->io.cfg_base  = info->cfg_base;
	self->io.fir_base  = info->fir_base;
        self->io.irq       = info->irq;
        self->io.fir_ext   = CHIP_IO_EXTENT;
        self->io.dma       = info->dma;
        self->io.fifo_size = 32;
	
	
	ret = request_region(self->io.fir_base, self->io.fir_ext, driver_name);
	if (!ret) {
		IRDA_WARNING("%s(), can't get iobase of 0x%03x\n",
			     __func__, self->io.fir_base);
		err = -ENODEV;
		goto out1;
	}

	
	irda_init_max_qos_capabilies(&self->qos);
	
	
	self->qos.baud_rate.bits = IR_9600|IR_19200|IR_38400|IR_57600|
		IR_115200|IR_576000|IR_1152000 |(IR_4000000 << 8);
	
	self->qos.min_turn_time.bits = qos_mtt_bits;
	irda_qos_bits_to_value(&self->qos);
	
	
	self->rx_buff.truesize = 14384; 
	self->tx_buff.truesize = 14384;

	
	self->rx_buff.head =
		dma_alloc_coherent(NULL, self->rx_buff.truesize,
				   &self->rx_buff_dma, GFP_KERNEL);
	if (self->rx_buff.head == NULL) {
		err = -ENOMEM;
		goto out2;

	}
	memset(self->rx_buff.head, 0, self->rx_buff.truesize);
	
	self->tx_buff.head =
		dma_alloc_coherent(NULL, self->tx_buff.truesize,
				   &self->tx_buff_dma, GFP_KERNEL);
	if (self->tx_buff.head == NULL) {
		err = -ENOMEM;
		goto out3;
	}
	memset(self->tx_buff.head, 0, self->tx_buff.truesize);

	self->rx_buff.in_frame = FALSE;
	self->rx_buff.state = OUTSIDE_FRAME;
	self->tx_buff.data = self->tx_buff.head;
	self->rx_buff.data = self->rx_buff.head;
	
	
	self->tx_fifo.len = self->tx_fifo.ptr = self->tx_fifo.free = 0;
	self->tx_fifo.tail = self->tx_buff.head;

	
	dev->netdev_ops = &nsc_ircc_sir_ops;

	err = register_netdev(dev);
	if (err) {
		IRDA_ERROR("%s(), register_netdev() failed!\n", __func__);
		goto out4;
	}
	IRDA_MESSAGE("IrDA: Registered device %s\n", dev->name);

	
	if ((dongle_id <= 0) ||
	    (dongle_id >= ARRAY_SIZE(dongle_types))) {
		dongle_id = nsc_ircc_read_dongle_id(self->io.fir_base);
		
		IRDA_MESSAGE("%s, Found dongle: %s\n", driver_name,
			     dongle_types[dongle_id]);
	} else {
		IRDA_MESSAGE("%s, Using dongle: %s\n", driver_name,
			     dongle_types[dongle_id]);
	}
	
	self->io.dongle_id = dongle_id;
	nsc_ircc_init_dongle_interface(self->io.fir_base, dongle_id);

 	self->pldev = platform_device_register_simple(NSC_IRCC_DRIVER_NAME,
 						      self->index, NULL, 0);
 	if (IS_ERR(self->pldev)) {
 		err = PTR_ERR(self->pldev);
 		goto out5;
 	}
 	platform_set_drvdata(self->pldev, self);

	return chip_index;

 out5:
 	unregister_netdev(dev);
 out4:
	dma_free_coherent(NULL, self->tx_buff.truesize,
			  self->tx_buff.head, self->tx_buff_dma);
 out3:
	dma_free_coherent(NULL, self->rx_buff.truesize,
			  self->rx_buff.head, self->rx_buff_dma);
 out2:
	release_region(self->io.fir_base, self->io.fir_ext);
 out1:
	free_netdev(dev);
	dev_self[chip_index] = NULL;
	return err;
}

static int __exit nsc_ircc_close(struct nsc_ircc_cb *self)
{
	int iobase;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return -1;);

        iobase = self->io.fir_base;

	platform_device_unregister(self->pldev);

	
	unregister_netdev(self->netdev);

	
	IRDA_DEBUG(4, "%s(), Releasing Region %03x\n", 
		   __func__, self->io.fir_base);
	release_region(self->io.fir_base, self->io.fir_ext);

	if (self->tx_buff.head)
		dma_free_coherent(NULL, self->tx_buff.truesize,
				  self->tx_buff.head, self->tx_buff_dma);
	
	if (self->rx_buff.head)
		dma_free_coherent(NULL, self->rx_buff.truesize,
				  self->rx_buff.head, self->rx_buff_dma);

	dev_self[self->index] = NULL;
	free_netdev(self->netdev);
	
	return 0;
}

static int nsc_ircc_init_108(nsc_chip_t *chip, chipio_t *info)
{
	int cfg_base = info->cfg_base;
	__u8 temp=0;

	outb(2, cfg_base);      
	outb(0x00, cfg_base+1); 
	
	
	outb(CFG_108_BAIC, cfg_base);
	switch (info->fir_base) {
	case 0x3e8: outb(0x14, cfg_base+1); break;
	case 0x2e8: outb(0x15, cfg_base+1); break;
	case 0x3f8: outb(0x16, cfg_base+1); break;
	case 0x2f8: outb(0x17, cfg_base+1); break;
	default: IRDA_ERROR("%s(), invalid base_address", __func__);
	}
	
	
	switch (info->irq) {
	case 3:  temp = 0x01; break;
	case 4:  temp = 0x02; break;
	case 5:  temp = 0x03; break;
	case 7:  temp = 0x04; break;
	case 9:  temp = 0x05; break;
	case 11: temp = 0x06; break;
	case 15: temp = 0x07; break;
	default: IRDA_ERROR("%s(), invalid irq", __func__);
	}
	outb(CFG_108_CSRT, cfg_base);
	
	switch (info->dma) {	
	case 0: outb(0x08+temp, cfg_base+1); break;
	case 1: outb(0x10+temp, cfg_base+1); break;
	case 3: outb(0x18+temp, cfg_base+1); break;
	default: IRDA_ERROR("%s(), invalid dma", __func__);
	}
	
	outb(CFG_108_MCTL, cfg_base);      
	outb(0x03, cfg_base+1); 

	return 0;
}

static int nsc_ircc_probe_108(nsc_chip_t *chip, chipio_t *info) 
{
	int cfg_base = info->cfg_base;
	int reg;

	
	outb(CFG_108_BAIC, cfg_base);
	reg = inb(cfg_base+1);
	
	switch (reg & 0x03) {
	case 0:
		info->fir_base = 0x3e8;
		break;
	case 1:
		info->fir_base = 0x2e8;
		break;
	case 2:
		info->fir_base = 0x3f8;
		break;
	case 3:
		info->fir_base = 0x2f8;
		break;
	}
	info->sir_base = info->fir_base;
	IRDA_DEBUG(2, "%s(), probing fir_base=0x%03x\n", __func__,
		   info->fir_base);

	
	outb(CFG_108_CSRT, cfg_base);
	reg = inb(cfg_base+1);

	switch (reg & 0x07) {
	case 0:
		info->irq = -1;
		break;
	case 1:
		info->irq = 3;
		break;
	case 2:
		info->irq = 4;
		break;
	case 3:
		info->irq = 5;
		break;
	case 4:
		info->irq = 7;
		break;
	case 5:
		info->irq = 9;
		break;
	case 6:
		info->irq = 11;
		break;
	case 7:
		info->irq = 15;
		break;
	}
	IRDA_DEBUG(2, "%s(), probing irq=%d\n", __func__, info->irq);

	
	switch ((reg >> 3) & 0x03) {
	case 0:
		info->dma = -1;
		break;
	case 1:
		info->dma = 0;
		break;
	case 2:
		info->dma = 1;
		break;
	case 3:
		info->dma = 3;
		break;
	}
	IRDA_DEBUG(2, "%s(), probing dma=%d\n", __func__, info->dma);

	
	outb(CFG_108_MCTL, cfg_base);
	reg = inb(cfg_base+1);

	info->enabled = reg & 0x01;
	info->suspended = !((reg >> 1) & 0x01);

	return 0;
}

static int nsc_ircc_init_338(nsc_chip_t *chip, chipio_t *info) 
{
	
	
	return 0;
}

static int nsc_ircc_probe_338(nsc_chip_t *chip, chipio_t *info) 
{
	int cfg_base = info->cfg_base;
	int reg, com = 0;
	int pnp;

	
	outb(CFG_338_FER, cfg_base);
	reg = inb(cfg_base+1);

	info->enabled = (reg >> 2) & 0x01;

	
	outb(CFG_338_PNP0, cfg_base);
	reg = inb(cfg_base+1);
	
	pnp = (reg >> 3) & 0x01;
	if (pnp) {
		IRDA_DEBUG(2, "(), Chip is in PnP mode\n");
		outb(0x46, cfg_base);
		reg = (inb(cfg_base+1) & 0xfe) << 2;

		outb(0x47, cfg_base);
		reg |= ((inb(cfg_base+1) & 0xfc) << 8);

		info->fir_base = reg;
	} else {
		
		outb(CFG_338_FAR, cfg_base);
		reg = inb(cfg_base+1);
		
		switch ((reg >> 4) & 0x03) {
		case 0:
			info->fir_base = 0x3f8;
			break;
		case 1:
			info->fir_base = 0x2f8;
			break;
		case 2:
			com = 3;
			break;
		case 3:
			com = 4;
			break;
		}
		
		if (com) {
			switch ((reg >> 6) & 0x03) {
			case 0:
				if (com == 3)
					info->fir_base = 0x3e8;
				else
					info->fir_base = 0x2e8;
				break;
			case 1:
				if (com == 3)
					info->fir_base = 0x338;
				else
					info->fir_base = 0x238;
				break;
			case 2:
				if (com == 3)
					info->fir_base = 0x2e8;
				else
					info->fir_base = 0x2e0;
				break;
			case 3:
				if (com == 3)
					info->fir_base = 0x220;
				else
					info->fir_base = 0x228;
				break;
			}
		}
	}
	info->sir_base = info->fir_base;

	
	outb(CFG_338_PNP1, cfg_base);
	reg = inb(cfg_base+1);
	
	info->irq = reg >> 4;
	
	
	outb(CFG_338_PNP3, cfg_base);
	reg = inb(cfg_base+1);

	info->dma = (reg & 0x07) - 1;

	
	outb(CFG_338_PTR, cfg_base);
	reg = inb(cfg_base+1);

	info->suspended = reg & 0x01;

	return 0;
}


/*
 * Function nsc_ircc_init_39x (chip, info)
 *
 *    Now that we know it's a '39x (see probe below), we need to
 *    configure it so we can use it.
 *
 * The NSC '338 chip is a Super I/O chip with a "bank" architecture,
 * the configuration of the different functionality (serial, parallel,
 * floppy...) are each in a different bank (Logical Device Number).
 * The base address, irq and dma configuration registers are common
 * to all functionalities (index 0x30 to 0x7F).
 * There is only one configuration register specific to the
 * serial port, CFG_39X_SPC.
 * JeanII
 *
 * Note : this code was written by Jan Frey <janfrey@web.de>
 */
static int nsc_ircc_init_39x(nsc_chip_t *chip, chipio_t *info) 
{
	int cfg_base = info->cfg_base;
	int enabled;

	
	IRDA_DEBUG(2, "%s(): nsc_ircc_init_39x (user settings): "
		   "io=0x%04x, irq=%d, dma=%d\n", 
		   __func__, info->fir_base, info->irq, info->dma);

	
	outb(CFG_39X_LDN, cfg_base);
	outb(0x02, cfg_base+1);

	

	
	outb(CFG_39X_ACT, cfg_base);
	enabled = inb(cfg_base+1) & 0x01;
	
	if (!enabled) {
		
		outb(CFG_39X_SIOCF1, cfg_base);
		outb(0x01, cfg_base+1);
		
	}

	outb(CFG_39X_SPC, cfg_base);
	outb(0x82, cfg_base+1);

	return 0;
}

/*
 * Function nsc_ircc_probe_39x (chip, info)
 *
 *    Test if we really have a '39x chip at the given address
 *
 * Note : this code was written by Jan Frey <janfrey@web.de>
 */
static int nsc_ircc_probe_39x(nsc_chip_t *chip, chipio_t *info) 
{
	int cfg_base = info->cfg_base;
	int reg1, reg2, irq, irqt, dma1, dma2;
	int enabled, susp;

	IRDA_DEBUG(2, "%s(), nsc_ircc_probe_39x, base=%d\n",
		   __func__, cfg_base);


	
	outb(CFG_39X_LDN, cfg_base);
	outb(0x02, cfg_base+1);

	
	outb(CFG_39X_BASEH, cfg_base);
	reg1 = inb(cfg_base+1);
	outb(CFG_39X_BASEL, cfg_base);
	reg2 = inb(cfg_base+1);
	info->fir_base = (reg1 << 8) | reg2;

	outb(CFG_39X_IRQNUM, cfg_base);
	irq = inb(cfg_base+1);
	outb(CFG_39X_IRQSEL, cfg_base);
	irqt = inb(cfg_base+1);
	info->irq = irq;

	outb(CFG_39X_DMA0, cfg_base);
	dma1 = inb(cfg_base+1);
	outb(CFG_39X_DMA1, cfg_base);
	dma2 = inb(cfg_base+1);
	info->dma = dma1 -1;

	outb(CFG_39X_ACT, cfg_base);
	info->enabled = enabled = inb(cfg_base+1) & 0x01;
	
	outb(CFG_39X_SPC, cfg_base);
	susp = 1 - ((inb(cfg_base+1) & 0x02) >> 1);

	IRDA_DEBUG(2, "%s(): io=0x%02x%02x, irq=%d (type %d), rxdma=%d, txdma=%d, enabled=%d (suspended=%d)\n", __func__, reg1,reg2,irq,irqt,dma1,dma2,enabled,susp);

	

	
	outb(CFG_39X_ACT, cfg_base);
	enabled = inb(cfg_base+1) & 0x01;
	
	if (!enabled) {
		
		outb(CFG_39X_SIOCF1, cfg_base);
		outb(0x01, cfg_base+1);
		
	}

	outb(CFG_39X_SPC, cfg_base);
	outb(0x82, cfg_base+1);

	return 0;
}

#ifdef CONFIG_PNP
static int nsc_ircc_pnp_probe(struct pnp_dev *dev, const struct pnp_device_id *id)
{
	memset(&pnp_info, 0, sizeof(chipio_t));
	pnp_info.irq = -1;
	pnp_info.dma = -1;
	pnp_succeeded = 1;

	if (id->driver_data & NSC_FORCE_DONGLE_TYPE9)
		dongle_id = 0x9;


	if (pnp_port_valid(dev, 0) &&
		!(pnp_port_flags(dev, 0) & IORESOURCE_DISABLED))
		pnp_info.fir_base = pnp_port_start(dev, 0);

	if (pnp_irq_valid(dev, 0) &&
		!(pnp_irq_flags(dev, 0) & IORESOURCE_DISABLED))
		pnp_info.irq = pnp_irq(dev, 0);

	if (pnp_dma_valid(dev, 0) &&
		!(pnp_dma_flags(dev, 0) & IORESOURCE_DISABLED))
		pnp_info.dma = pnp_dma(dev, 0);

	IRDA_DEBUG(0, "%s() : From PnP, found firbase 0x%03X ; irq %d ; dma %d.\n",
		   __func__, pnp_info.fir_base, pnp_info.irq, pnp_info.dma);

	if((pnp_info.fir_base == 0) ||
	   (pnp_info.irq == -1) || (pnp_info.dma == -1)) {
		
		
		pnp_succeeded = 0;
	}

	return 0;
}
#endif

static int nsc_ircc_setup(chipio_t *info)
{
	int version;
	int iobase = info->fir_base;

	
	switch_bank(iobase, BANK3);
	version = inb(iobase+MID);

	IRDA_DEBUG(2, "%s() Driver %s Found chip version %02x\n",
		   __func__, driver_name, version);

	
	if (0x20 != (version & 0xf0)) {
		IRDA_ERROR("%s, Wrong chip version %02x\n",
			   driver_name, version);
		return -1;
	}

	
	switch_bank(iobase, BANK2);
	outb(ECR1_EXT_SL, iobase+ECR1);
	switch_bank(iobase, BANK0);
	
	
	switch_bank(iobase, BANK0);
	outb(FCR_RXTH|FCR_TXTH|FCR_TXSR|FCR_RXSR|FCR_FIFO_EN, iobase+FCR);

	outb(0x03, iobase+LCR); 	
	outb(MCR_SIR, iobase+MCR); 	

	
	switch_bank(iobase, BANK2);
	outb(EXCR2_RFSIZ|EXCR2_TFSIZ, iobase+EXCR2);

	
	switch_bank(iobase, BANK5);
 	outb(0x02, iobase+4);

	
	switch_bank(iobase, BANK6);
	outb(0x20, iobase+0); 
	outb(0x0a, iobase+1); 
	outb(0x0d, iobase+2); 
	outb(0x2a, iobase+4); 

	
	switch_bank(iobase, BANK0);
	outb(IER_RXHDL_IE, iobase+IER);

	return 0;
}

static int nsc_ircc_read_dongle_id (int iobase)
{
	int dongle_id;
	__u8 bank;

	bank = inb(iobase+BSR);

	
	switch_bank(iobase, BANK7);
	
	
	outb(0x00, iobase+7);
	
	
	udelay(50);
	
	
	dongle_id = inb(iobase+4) & 0x0f;

#ifdef BROKEN_DONGLE_ID
	if (dongle_id == 0x0a)
		dongle_id = 0x09;
#endif	
	
	switch_bank(iobase, BANK0);

	outb(bank, iobase+BSR);

	return dongle_id;
}

static void nsc_ircc_init_dongle_interface (int iobase, int dongle_id)
{
	int bank;

	
	bank = inb(iobase+BSR);

	
	switch_bank(iobase, BANK7);
	
	
	switch (dongle_id) {
	case 0x00: 
	case 0x01: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x02: 
	case 0x03: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x04: 
		break;
	case 0x05: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x06: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x07: 
		IRDA_DEBUG(0, "%s(), %s is not for IrDA mode\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x08: 
		IRDA_DEBUG(0, "%s(), %s\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x09: 
		outb(0x28, iobase+7); 
		break;
	case 0x0A: 
	case 0x0B: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x0C: 
	case 0x0D: 
		outb(0x48, iobase+7); 
		break;
	case 0x0E: 
		outb(0x28, iobase+7); 
		break;
	case 0x0F: 
		IRDA_DEBUG(0, "%s(), %s\n",
			   __func__, dongle_types[dongle_id]);

		switch_bank(iobase, BANK0);
		outb(0x62, iobase+MCR);
		break;
	default: 
		IRDA_DEBUG(0, "%s(), invalid dongle_id %#x", 
			   __func__, dongle_id);
	}
	
	
	outb(0x00, iobase+4);

	
	outb(bank, iobase+BSR);
	
} 

static void nsc_ircc_change_dongle_speed(int iobase, int speed, int dongle_id)
{
	__u8 bank;

	
	bank = inb(iobase+BSR);

	
	switch_bank(iobase, BANK7);
	
	
	switch (dongle_id) {
	case 0x00: 
	case 0x01: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x02: 
	case 0x03: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x04: 
		break;
	case 0x05: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x06: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x07: 
		IRDA_DEBUG(0, "%s(), %s is not for IrDA mode\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x08: 
		IRDA_DEBUG(0, "%s(), %s\n", 
			   __func__, dongle_types[dongle_id]);
		outb(0x00, iobase+4);
		if (speed > 115200)
			outb(0x01, iobase+4);
		break;
	case 0x09: 
		outb(0x01, iobase+4);

		if (speed == 4000000) {
			outb(0x81, iobase+4);
			outb(0x80, iobase+4);
		} else
			outb(0x00, iobase+4);
		break;
	case 0x0A: 
	case 0x0B: 
		IRDA_DEBUG(0, "%s(), %s not defined by irda yet\n",
			   __func__, dongle_types[dongle_id]);
		break;
	case 0x0C: 
	case 0x0D: 
		break;
	case 0x0E: 
		break;
	case 0x0F: 
		IRDA_DEBUG(0, "%s(), %s is not for IrDA mode\n",
			   __func__, dongle_types[dongle_id]);

		switch_bank(iobase, BANK0); 
		outb(0x62, iobase+MCR);
		break;
	default: 
		IRDA_DEBUG(0, "%s(), invalid data_rate\n", __func__);
	}
	
	outb(bank, iobase+BSR);
}

static __u8 nsc_ircc_change_speed(struct nsc_ircc_cb *self, __u32 speed)
{
	struct net_device *dev = self->netdev;
	__u8 mcr = MCR_SIR;
	int iobase; 
	__u8 bank;
	__u8 ier;                  

	IRDA_DEBUG(2, "%s(), speed=%d\n", __func__, speed);

	IRDA_ASSERT(self != NULL, return 0;);

	iobase = self->io.fir_base;

	
	self->io.speed = speed;

	
	bank = inb(iobase+BSR);

	
	switch_bank(iobase, BANK0);
	outb(0, iobase+IER);

	
	switch_bank(iobase, BANK2);

	outb(0x00, iobase+BGDH);
	switch (speed) {
	case 9600:   outb(0x0c, iobase+BGDL); break;
	case 19200:  outb(0x06, iobase+BGDL); break;
	case 38400:  outb(0x03, iobase+BGDL); break;
	case 57600:  outb(0x02, iobase+BGDL); break;
	case 115200: outb(0x01, iobase+BGDL); break;
	case 576000:
		switch_bank(iobase, BANK5);
		
		
		outb(inb(iobase+4) | 0x04, iobase+4);
	       
		mcr = MCR_MIR;
		IRDA_DEBUG(0, "%s(), handling baud of 576000\n", __func__);
		break;
	case 1152000:
		mcr = MCR_MIR;
		IRDA_DEBUG(0, "%s(), handling baud of 1152000\n", __func__);
		break;
	case 4000000:
		mcr = MCR_FIR;
		IRDA_DEBUG(0, "%s(), handling baud of 4000000\n", __func__);
		break;
	default:
		mcr = MCR_FIR;
		IRDA_DEBUG(0, "%s(), unknown baud rate of %d\n", 
			   __func__, speed);
		break;
	}

	
	switch_bank(iobase, BANK0);
	outb(mcr | MCR_TX_DFR, iobase+MCR);

	
	nsc_ircc_change_dongle_speed(iobase, speed, self->io.dongle_id);

	
	switch_bank(iobase, BANK0);
	outb(0x00, iobase+FCR);
	outb(FCR_FIFO_EN, iobase+FCR);
	outb(FCR_RXTH|     
	     FCR_TXTH|     
	     FCR_TXSR|     
	     FCR_RXSR|     
	     FCR_FIFO_EN,  
	     iobase+FCR);
	
	
	switch_bank(iobase, BANK2);
	outb(EXCR2_RFSIZ|EXCR2_TFSIZ, iobase+EXCR2);
	
	
	switch_bank(iobase, BANK0); 
	if (speed > 115200) {
		
		dev->netdev_ops = &nsc_ircc_fir_ops;
		ier = IER_SFIF_IE;
		nsc_ircc_dma_receive(self);
	} else {
		
		dev->netdev_ops = &nsc_ircc_sir_ops;
		ier = IER_RXHDL_IE;
	}
	
	outb(ier, iobase+IER);
    	
	
	outb(bank, iobase+BSR);

	
	return ier;
}

static netdev_tx_t nsc_ircc_hard_xmit_sir(struct sk_buff *skb,
						struct net_device *dev)
{
	struct nsc_ircc_cb *self;
	unsigned long flags;
	int iobase;
	__s32 speed;
	__u8 bank;
	
	self = netdev_priv(dev);

	IRDA_ASSERT(self != NULL, return NETDEV_TX_OK;);

	iobase = self->io.fir_base;

	netif_stop_queue(dev);
		
	
	spin_lock_irqsave(&self->lock, flags);
	
	
	speed = irda_get_next_speed(skb);
	if ((speed != self->io.speed) && (speed != -1)) {
		
		if (!skb->len) {
			if (self->io.direction == IO_RECV) {
				nsc_ircc_change_speed(self, speed); 
				netif_wake_queue(dev);
			} else {
				self->new_speed = speed;
			}
			dev->trans_start = jiffies;
			spin_unlock_irqrestore(&self->lock, flags);
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		} else
			self->new_speed = speed;
	}

	
	bank = inb(iobase+BSR);
	
	self->tx_buff.data = self->tx_buff.head;
	
	self->tx_buff.len = async_wrap_skb(skb, self->tx_buff.data, 
					   self->tx_buff.truesize);

	dev->stats.tx_bytes += self->tx_buff.len;
	
	
	switch_bank(iobase, BANK0);
	outb(IER_TXLDL_IE, iobase+IER);
	
	
	outb(bank, iobase+BSR);

	dev->trans_start = jiffies;
	spin_unlock_irqrestore(&self->lock, flags);

	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static netdev_tx_t nsc_ircc_hard_xmit_fir(struct sk_buff *skb,
						struct net_device *dev)
{
	struct nsc_ircc_cb *self;
	unsigned long flags;
	int iobase;
	__s32 speed;
	__u8 bank;
	int mtt, diff;
	
	self = netdev_priv(dev);
	iobase = self->io.fir_base;

	netif_stop_queue(dev);
	
	
	spin_lock_irqsave(&self->lock, flags);

	
	speed = irda_get_next_speed(skb);
	if ((speed != self->io.speed) && (speed != -1)) {
		
		if (!skb->len) {
			if(self->tx_fifo.len == 0) {
				nsc_ircc_change_speed(self, speed); 
				netif_wake_queue(dev);
			} else {
				self->new_speed = speed;
			}
			dev->trans_start = jiffies;
			spin_unlock_irqrestore(&self->lock, flags);
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		} else {
			
			self->new_speed = speed;
		}
	}

	
	bank = inb(iobase+BSR);

	
	self->tx_fifo.queue[self->tx_fifo.free].start = self->tx_fifo.tail;
	self->tx_fifo.queue[self->tx_fifo.free].len = skb->len;
	self->tx_fifo.tail += skb->len;

	dev->stats.tx_bytes += skb->len;

	skb_copy_from_linear_data(skb, self->tx_fifo.queue[self->tx_fifo.free].start,
		      skb->len);
	self->tx_fifo.len++;
	self->tx_fifo.free++;

	
	if (self->tx_fifo.len == 1) {
		
		mtt = irda_get_mtt(skb);
		if (mtt) {
			
			do_gettimeofday(&self->now);
			diff = self->now.tv_usec - self->stamp.tv_usec;
			if (diff < 0) 
				diff += 1000000;
			
			if (mtt > diff) {
				mtt -= diff;

				if (mtt > 125) {
					
					mtt = mtt / 125;
					
					
					switch_bank(iobase, BANK4);
					outb(mtt & 0xff, iobase+TMRL);
					outb((mtt >> 8) & 0x0f, iobase+TMRH);
					
					
					outb(IRCR1_TMR_EN, iobase+IRCR1);
					self->io.direction = IO_XMIT;
					
					
					switch_bank(iobase, BANK0);
					outb(IER_TMR_IE, iobase+IER);
					
					
					goto out; 
				} else
					udelay(mtt);
			}
		}		
		
		switch_bank(iobase, BANK0);
		outb(IER_DMA_IE, iobase+IER);

		
		nsc_ircc_dma_xmit(self, iobase);
	}
 out:
	if ((self->tx_fifo.free < MAX_TX_WINDOW) && (self->new_speed == 0))
		netif_wake_queue(self->netdev);

	
	outb(bank, iobase+BSR);

	dev->trans_start = jiffies;
	spin_unlock_irqrestore(&self->lock, flags);
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static void nsc_ircc_dma_xmit(struct nsc_ircc_cb *self, int iobase)
{
	int bsr;

	
	bsr = inb(iobase+BSR);

	
	switch_bank(iobase, BANK0);
	outb(inb(iobase+MCR) & ~MCR_DMA_EN, iobase+MCR);
	
	self->io.direction = IO_XMIT;
	
	 
	switch_bank(iobase, BANK2);
	outb(ECR1_DMASWP|ECR1_DMANF|ECR1_EXT_SL, iobase+ECR1);
	
	irda_setup_dma(self->io.dma, 
		       ((u8 *)self->tx_fifo.queue[self->tx_fifo.ptr].start -
			self->tx_buff.head) + self->tx_buff_dma,
		       self->tx_fifo.queue[self->tx_fifo.ptr].len, 
		       DMA_TX_MODE);

	
 	switch_bank(iobase, BANK0);	
	outb(inb(iobase+MCR)|MCR_TX_DFR|MCR_DMA_EN|MCR_IR_PLS, iobase+MCR);

	
	outb(bsr, iobase+BSR);
}

static int nsc_ircc_pio_write(int iobase, __u8 *buf, int len, int fifo_size)
{
	int actual = 0;
	__u8 bank;
	
	IRDA_DEBUG(4, "%s()\n", __func__);

	
	bank = inb(iobase+BSR);

	switch_bank(iobase, BANK0);
	if (!(inb_p(iobase+LSR) & LSR_TXEMP)) {
		IRDA_DEBUG(4, "%s(), warning, FIFO not empty yet!\n",
			   __func__);

		
		fifo_size -= 17;
	}

	
	while ((fifo_size-- > 0) && (actual < len)) {
		
		outb(buf[actual++], iobase+TXD);
	}
        
	IRDA_DEBUG(4, "%s(), fifo_size %d ; %d sent of %d\n", 
		   __func__, fifo_size, actual, len);
	
	
	outb(bank, iobase+BSR);

	return actual;
}

static int nsc_ircc_dma_xmit_complete(struct nsc_ircc_cb *self)
{
	int iobase;
	__u8 bank;
	int ret = TRUE;

	IRDA_DEBUG(2, "%s()\n", __func__);

	iobase = self->io.fir_base;

	
	bank = inb(iobase+BSR);

	
	switch_bank(iobase, BANK0);
        outb(inb(iobase+MCR) & ~MCR_DMA_EN, iobase+MCR);
	
	
	if (inb(iobase+ASCR) & ASCR_TXUR) {
		self->netdev->stats.tx_errors++;
		self->netdev->stats.tx_fifo_errors++;
		
		
		outb(ASCR_TXUR, iobase+ASCR);
	} else {
		self->netdev->stats.tx_packets++;
	}

	
	self->tx_fifo.ptr++;
	self->tx_fifo.len--;

	
	if (self->tx_fifo.len) {
		nsc_ircc_dma_xmit(self, iobase);
		
		
		ret = FALSE;
	} else {
		
		self->tx_fifo.len = self->tx_fifo.ptr = self->tx_fifo.free = 0;
		self->tx_fifo.tail = self->tx_buff.head;
	}

	if ((self->tx_fifo.free < MAX_TX_WINDOW) && (self->new_speed == 0)) {
		
		
		netif_wake_queue(self->netdev);
	}

	
	outb(bank, iobase+BSR);
	
	return ret;
}

static int nsc_ircc_dma_receive(struct nsc_ircc_cb *self) 
{
	int iobase;
	__u8 bsr;

	iobase = self->io.fir_base;

	
	self->tx_fifo.len = self->tx_fifo.ptr = self->tx_fifo.free = 0;
	self->tx_fifo.tail = self->tx_buff.head;

	
	bsr = inb(iobase+BSR);

	
	switch_bank(iobase, BANK0);
	outb(inb(iobase+MCR) & ~MCR_DMA_EN, iobase+MCR);

	
	switch_bank(iobase, BANK2);
	outb(ECR1_DMANF|ECR1_EXT_SL, iobase+ECR1);

	self->io.direction = IO_RECV;
	self->rx_buff.data = self->rx_buff.head;
	
	
	switch_bank(iobase, BANK0);
	outb(FCR_RXSR|FCR_FIFO_EN, iobase+FCR);

	self->st_fifo.len = self->st_fifo.pending_bytes = 0;
	self->st_fifo.tail = self->st_fifo.head = 0;
	
	irda_setup_dma(self->io.dma, self->rx_buff_dma, self->rx_buff.truesize,
		       DMA_RX_MODE);

	
	switch_bank(iobase, BANK0);
	outb(inb(iobase+MCR)|MCR_DMA_EN, iobase+MCR);

	
	outb(bsr, iobase+BSR);
	
	return 0;
}

static int nsc_ircc_dma_receive_complete(struct nsc_ircc_cb *self, int iobase)
{
	struct st_fifo *st_fifo;
	struct sk_buff *skb;
	__u8 status;
	__u8 bank;
	int len;

	st_fifo = &self->st_fifo;

	
	bank = inb(iobase+BSR);
	
	
	switch_bank(iobase, BANK5);
	while ((status = inb(iobase+FRM_ST)) & FRM_ST_VLD) {
		
		len = inb(iobase+RFLFL) | ((inb(iobase+RFLFH) & 0x1f) << 8);

		if (st_fifo->tail >= MAX_RX_WINDOW) {
			IRDA_DEBUG(0, "%s(), window is full!\n", __func__);
			continue;
		}
			
		st_fifo->entries[st_fifo->tail].status = status;
		st_fifo->entries[st_fifo->tail].len = len;
		st_fifo->pending_bytes += len;
		st_fifo->tail++;
		st_fifo->len++;
	}
	
	while (st_fifo->len > 0) {
		
		status = st_fifo->entries[st_fifo->head].status;
		len    = st_fifo->entries[st_fifo->head].len;
		st_fifo->pending_bytes -= len;
		st_fifo->head++;
		st_fifo->len--;

		
		if (status & FRM_ST_ERR_MSK) {
			if (status & FRM_ST_LOST_FR) {
				
				self->netdev->stats.rx_errors += len;
			} else {
				
				self->netdev->stats.rx_errors++;
				
				self->rx_buff.data += len;
			
				if (status & FRM_ST_MAX_LEN)
					self->netdev->stats.rx_length_errors++;
				
				if (status & FRM_ST_PHY_ERR) 
					self->netdev->stats.rx_frame_errors++;
				
				if (status & FRM_ST_BAD_CRC) 
					self->netdev->stats.rx_crc_errors++;
			}
			
			if (status & FRM_ST_OVR1)
				self->netdev->stats.rx_fifo_errors++;
			
			if (status & FRM_ST_OVR2)
				self->netdev->stats.rx_fifo_errors++;
		} else {
			if (st_fifo->pending_bytes < self->io.fifo_size) {
				switch_bank(iobase, BANK0);
				if (inb(iobase+LSR) & LSR_RXDA) {
					
					st_fifo->head--;
					st_fifo->len++;
					st_fifo->pending_bytes += len;
					st_fifo->entries[st_fifo->head].status = status;
					st_fifo->entries[st_fifo->head].len = len;
					switch_bank(iobase, BANK4);
					outb(0x02, iobase+TMRL); 
					outb(0x00, iobase+TMRH);

					
					outb(IRCR1_TMR_EN, iobase+IRCR1);

					
					outb(bank, iobase+BSR);
					
					return FALSE; 
				}
			}

			do_gettimeofday(&self->stamp);

			skb = dev_alloc_skb(len+1);
			if (skb == NULL)  {
				IRDA_WARNING("%s(), memory squeeze, "
					     "dropping frame.\n",
					     __func__);
				self->netdev->stats.rx_dropped++;

				
				outb(bank, iobase+BSR);

				return FALSE;
			}
			
			
			skb_reserve(skb, 1); 

			
			if (self->io.speed < 4000000) {
				skb_put(skb, len-2);
				skb_copy_to_linear_data(skb,
							self->rx_buff.data,
							len - 2);
			} else {
				skb_put(skb, len-4);
				skb_copy_to_linear_data(skb,
							self->rx_buff.data,
							len - 4);
			}

			
			self->rx_buff.data += len;
			self->netdev->stats.rx_bytes += len;
			self->netdev->stats.rx_packets++;

			skb->dev = self->netdev;
			skb_reset_mac_header(skb);
			skb->protocol = htons(ETH_P_IRDA);
			netif_rx(skb);
		}
	}
	
	outb(bank, iobase+BSR);

	return TRUE;
}

static void nsc_ircc_pio_receive(struct nsc_ircc_cb *self) 
{
	__u8 byte;
	int iobase;

	iobase = self->io.fir_base;
	
	
	do {
		byte = inb(iobase+RXD);
		async_unwrap_char(self->netdev, &self->netdev->stats,
				  &self->rx_buff, byte);
	} while (inb(iobase+LSR) & LSR_RXDA); 	
}

static void nsc_ircc_sir_interrupt(struct nsc_ircc_cb *self, int eir)
{
	int actual;

	
	if (eir & EIR_TXLDL_EV) {
		
		actual = nsc_ircc_pio_write(self->io.fir_base, 
					   self->tx_buff.data, 
					   self->tx_buff.len, 
					   self->io.fifo_size);
		self->tx_buff.data += actual;
		self->tx_buff.len  -= actual;
		
		self->io.direction = IO_XMIT;

		
		if (self->tx_buff.len > 0)
			self->ier = IER_TXLDL_IE;
		else { 

			self->netdev->stats.tx_packets++;
			netif_wake_queue(self->netdev);
			self->ier = IER_TXEMP_IE;
		}
			
	}
	
	if (eir & EIR_TXEMP_EV) {
		
		self->io.direction = IO_RECV;
		self->ier = IER_RXHDL_IE;
		if (self->new_speed) {
			IRDA_DEBUG(2, "%s(), Changing speed!\n", __func__);
			self->ier = nsc_ircc_change_speed(self,
							  self->new_speed);
			self->new_speed = 0;
			netif_wake_queue(self->netdev);

			
			if (self->io.speed > 115200) {
				
				return;
			}
		}
	}

	
	if (eir & EIR_RXHDL_EV) {
		nsc_ircc_pio_receive(self);

		
		self->ier = IER_RXHDL_IE;
	}
}

static void nsc_ircc_fir_interrupt(struct nsc_ircc_cb *self, int iobase, 
				   int eir)
{
	__u8 bank;

	bank = inb(iobase+BSR);
	
	
	if (eir & EIR_SFIF_EV) {
		
		if (nsc_ircc_dma_receive_complete(self, iobase)) {
			
			self->ier = IER_SFIF_IE;
		} else {
			self->ier = IER_SFIF_IE | IER_TMR_IE;
		}
	} else if (eir & EIR_TMR_EV) { 
		
		switch_bank(iobase, BANK4);
		outb(0, iobase+IRCR1);

		
		switch_bank(iobase, BANK0);
		outb(ASCR_CTE, iobase+ASCR);

		
		if (self->io.direction == IO_XMIT) {
			nsc_ircc_dma_xmit(self, iobase);

			
			self->ier = IER_DMA_IE;
		} else {
			
			if (nsc_ircc_dma_receive_complete(self, iobase)) {
				self->ier = IER_SFIF_IE;
			} else {
				self->ier = IER_SFIF_IE | IER_TMR_IE;
			}
		}
	} else if (eir & EIR_DMA_EV) {
		
		if (nsc_ircc_dma_xmit_complete(self)) {
			if(self->new_speed != 0) {
				self->ier = IER_TXEMP_IE;
			} else {
				if (irda_device_txqueue_empty(self->netdev)) {
					
					nsc_ircc_dma_receive(self);
					self->ier = IER_SFIF_IE;
				} else
					IRDA_WARNING("%s(), potential "
						     "Tx queue lockup !\n",
						     __func__);
			}
		} else {
			
			self->ier = IER_DMA_IE;
		}
	} else if (eir & EIR_TXEMP_EV) {
		self->ier = nsc_ircc_change_speed(self, self->new_speed);
		self->new_speed = 0;
		netif_wake_queue(self->netdev);
		
	}

	outb(bank, iobase+BSR);
}

static irqreturn_t nsc_ircc_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct nsc_ircc_cb *self;
	__u8 bsr, eir;
	int iobase;

	self = netdev_priv(dev);

	spin_lock(&self->lock);	

	iobase = self->io.fir_base;

	bsr = inb(iobase+BSR); 	

	switch_bank(iobase, BANK0);	
	self->ier = inb(iobase+IER); 
	eir = inb(iobase+EIR) & self->ier;  

	outb(0, iobase+IER); 
	
	if (eir) {
		
		if (self->io.speed > 115200)
			nsc_ircc_fir_interrupt(self, iobase, eir);
		else
			nsc_ircc_sir_interrupt(self, eir);
	}
	
	outb(self->ier, iobase+IER); 
	outb(bsr, iobase+BSR);       

	spin_unlock(&self->lock);
	return IRQ_RETVAL(eir);
}

static int nsc_ircc_is_receiving(struct nsc_ircc_cb *self)
{
	unsigned long flags;
	int status = FALSE;
	int iobase;
	__u8 bank;

	IRDA_ASSERT(self != NULL, return FALSE;);

	spin_lock_irqsave(&self->lock, flags);

	if (self->io.speed > 115200) {
		iobase = self->io.fir_base;

		
		bank = inb(iobase+BSR);
		switch_bank(iobase, BANK2);
		if ((inb(iobase+RXFLV) & 0x3f) != 0) {
			
			status =  TRUE;
		}
		outb(bank, iobase+BSR);
	} else 
		status = (self->rx_buff.state != OUTSIDE_FRAME);
	
	spin_unlock_irqrestore(&self->lock, flags);

	return status;
}

static int nsc_ircc_net_open(struct net_device *dev)
{
	struct nsc_ircc_cb *self;
	int iobase;
	char hwname[32];
	__u8 bank;
	
	IRDA_DEBUG(4, "%s()\n", __func__);
	
	IRDA_ASSERT(dev != NULL, return -1;);
	self = netdev_priv(dev);
	
	IRDA_ASSERT(self != NULL, return 0;);
	
	iobase = self->io.fir_base;
	
	if (request_irq(self->io.irq, nsc_ircc_interrupt, 0, dev->name, dev)) {
		IRDA_WARNING("%s, unable to allocate irq=%d\n",
			     driver_name, self->io.irq);
		return -EAGAIN;
	}
	if (request_dma(self->io.dma, dev->name)) {
		IRDA_WARNING("%s, unable to allocate dma=%d\n",
			     driver_name, self->io.dma);
		free_irq(self->io.irq, dev);
		return -EAGAIN;
	}
	
	
	bank = inb(iobase+BSR);
	
	
	switch_bank(iobase, BANK0);
	outb(IER_LS_IE | IER_RXHDL_IE, iobase+IER);

	
	outb(bank, iobase+BSR);

	
	netif_start_queue(dev);
	
	
	sprintf(hwname, "NSC-FIR @ 0x%03x", self->io.fir_base);

	self->irlap = irlap_open(dev, &self->qos, hwname);

	return 0;
}

static int nsc_ircc_net_close(struct net_device *dev)
{
	struct nsc_ircc_cb *self;
	int iobase;
	__u8 bank;

	IRDA_DEBUG(4, "%s()\n", __func__);
	
	IRDA_ASSERT(dev != NULL, return -1;);

	self = netdev_priv(dev);
	IRDA_ASSERT(self != NULL, return 0;);

	
	netif_stop_queue(dev);
	
	
	if (self->irlap)
		irlap_close(self->irlap);
	self->irlap = NULL;
	
	iobase = self->io.fir_base;

	disable_dma(self->io.dma);

	
	bank = inb(iobase+BSR);

	
	switch_bank(iobase, BANK0);
	outb(0, iobase+IER); 
       
	free_irq(self->io.irq, dev);
	free_dma(self->io.dma);

	
	outb(bank, iobase+BSR);

	return 0;
}

static int nsc_ircc_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	struct nsc_ircc_cb *self;
	unsigned long flags;
	int ret = 0;

	IRDA_ASSERT(dev != NULL, return -1;);

	self = netdev_priv(dev);

	IRDA_ASSERT(self != NULL, return -1;);

	IRDA_DEBUG(2, "%s(), %s, (cmd=0x%X)\n", __func__, dev->name, cmd);
	
	switch (cmd) {
	case SIOCSBANDWIDTH: 
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}
		spin_lock_irqsave(&self->lock, flags);
		nsc_ircc_change_speed(self, irq->ifr_baudrate);
		spin_unlock_irqrestore(&self->lock, flags);
		break;
	case SIOCSMEDIABUSY: 
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}
		irda_device_set_media_busy(self->netdev, TRUE);
		break;
	case SIOCGRECEIVING: 
		
		irq->ifr_receiving = nsc_ircc_is_receiving(self);
		break;
	default:
		ret = -EOPNOTSUPP;
	}
	return ret;
}

static int nsc_ircc_suspend(struct platform_device *dev, pm_message_t state)
{
     	struct nsc_ircc_cb *self = platform_get_drvdata(dev);
 	int bank;
	unsigned long flags;
 	int iobase = self->io.fir_base;

	if (self->io.suspended)
		return 0;

	IRDA_DEBUG(1, "%s, Suspending\n", driver_name);

	rtnl_lock();
	if (netif_running(self->netdev)) {
		netif_device_detach(self->netdev);
		spin_lock_irqsave(&self->lock, flags);
		
		bank = inb(iobase+BSR);

		
		switch_bank(iobase, BANK0);
		outb(0, iobase+IER);

		
		outb(bank, iobase+BSR);

		spin_unlock_irqrestore(&self->lock, flags);
		free_irq(self->io.irq, self->netdev);
		disable_dma(self->io.dma);
	}
	self->io.suspended = 1;
	rtnl_unlock();

	return 0;
}

static int nsc_ircc_resume(struct platform_device *dev)
{
 	struct nsc_ircc_cb *self = platform_get_drvdata(dev);
 	unsigned long flags;

	if (!self->io.suspended)
		return 0;

	IRDA_DEBUG(1, "%s, Waking up\n", driver_name);

	rtnl_lock();
	nsc_ircc_setup(&self->io);
	nsc_ircc_init_dongle_interface(self->io.fir_base, self->io.dongle_id);

	if (netif_running(self->netdev)) {
		if (request_irq(self->io.irq, nsc_ircc_interrupt, 0,
				self->netdev->name, self->netdev)) {
 		    	IRDA_WARNING("%s, unable to allocate irq=%d\n",
				     driver_name, self->io.irq);

			unregister_netdevice(self->netdev);
		} else {
			spin_lock_irqsave(&self->lock, flags);
			nsc_ircc_change_speed(self, self->io.speed);
			spin_unlock_irqrestore(&self->lock, flags);
			netif_device_attach(self->netdev);
		}

	} else {
		spin_lock_irqsave(&self->lock, flags);
		nsc_ircc_change_speed(self, 9600);
		spin_unlock_irqrestore(&self->lock, flags);
	}
	self->io.suspended = 0;
	rtnl_unlock();

 	return 0;
}

MODULE_AUTHOR("Dag Brattli <dagb@cs.uit.no>");
MODULE_DESCRIPTION("NSC IrDA Device Driver");
MODULE_LICENSE("GPL");


module_param(qos_mtt_bits, int, 0);
MODULE_PARM_DESC(qos_mtt_bits, "Minimum Turn Time");
module_param_array(io, int, NULL, 0);
MODULE_PARM_DESC(io, "Base I/O addresses");
module_param_array(irq, int, NULL, 0);
MODULE_PARM_DESC(irq, "IRQ lines");
module_param_array(dma, int, NULL, 0);
MODULE_PARM_DESC(dma, "DMA channels");
module_param(dongle_id, int, 0);
MODULE_PARM_DESC(dongle_id, "Type-id of used dongle");

module_init(nsc_ircc_init);
module_exit(nsc_ircc_cleanup);

