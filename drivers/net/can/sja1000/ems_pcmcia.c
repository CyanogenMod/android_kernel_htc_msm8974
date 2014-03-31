/*
 * Copyright (C) 2008 Sebastian Haas (initial chardev implementation)
 * Copyright (C) 2010 Markus Plessing <plessing@ems-wuensche.com>
 * Rework for mainline by Oliver Hartkopp <socketcan@hartkopp.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the version 2 of the GNU General Public License
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include "sja1000.h"

#define DRV_NAME "ems_pcmcia"

MODULE_AUTHOR("Markus Plessing <plessing@ems-wuensche.com>");
MODULE_DESCRIPTION("Socket-CAN driver for EMS CPC-CARD cards");
MODULE_SUPPORTED_DEVICE("EMS CPC-CARD CAN card");
MODULE_LICENSE("GPL v2");

#define EMS_PCMCIA_MAX_CHAN 2

struct ems_pcmcia_card {
	int channels;
	struct pcmcia_device *pcmcia_dev;
	struct net_device *net_dev[EMS_PCMCIA_MAX_CHAN];
	void __iomem *base_addr;
};

#define EMS_PCMCIA_CAN_CLOCK (16000000 / 2)

#define EMS_PCMCIA_OCR (OCR_TX0_PUSHPULL | OCR_TX1_PUSHPULL)

#define EMS_PCMCIA_CDR (CDR_CBP | CDR_CLKOUT_MASK)
#define EMS_PCMCIA_MEM_SIZE 4096 
#define EMS_PCMCIA_CAN_BASE_OFFSET 0x100 
#define EMS_PCMCIA_CAN_CTRL_SIZE 0x80 

#define EMS_CMD_RESET 0x00 
#define EMS_CMD_MAP   0x03 
#define EMS_CMD_UMAP  0x02 

static struct pcmcia_device_id ems_pcmcia_tbl[] = {
	PCMCIA_DEVICE_PROD_ID123("EMS_T_W", "CPC-Card", "V2.0", 0xeab1ea23,
				 0xa338573f, 0xe4575800),
	PCMCIA_DEVICE_NULL,
};

MODULE_DEVICE_TABLE(pcmcia, ems_pcmcia_tbl);

static u8 ems_pcmcia_read_reg(const struct sja1000_priv *priv, int port)
{
	return readb(priv->reg_base + port);
}

static void ems_pcmcia_write_reg(const struct sja1000_priv *priv, int port,
				 u8 val)
{
	writeb(val, priv->reg_base + port);
}

static irqreturn_t ems_pcmcia_interrupt(int irq, void *dev_id)
{
	struct ems_pcmcia_card *card = dev_id;
	struct net_device *dev;
	irqreturn_t retval = IRQ_NONE;
	int i, again;

	
	if (readw(card->base_addr) != 0xAA55)
		return IRQ_HANDLED;

	do {
		again = 0;

		
		for (i = 0; i < card->channels; i++) {
			dev = card->net_dev[i];
			if (!dev)
				continue;

			if (sja1000_interrupt(irq, dev) == IRQ_HANDLED)
				again = 1;
		}
		
		if (again)
			retval = IRQ_HANDLED;

	} while (again);

	return retval;
}

static inline int ems_pcmcia_check_chan(struct sja1000_priv *priv)
{
	
	ems_pcmcia_write_reg(priv, REG_MOD, 1);
	ems_pcmcia_write_reg(priv, REG_CDR, CDR_PELICAN);

	
	if (ems_pcmcia_read_reg(priv, REG_CDR) == CDR_PELICAN)
		return 1;

	return 0;
}

static void ems_pcmcia_del_card(struct pcmcia_device *pdev)
{
	struct ems_pcmcia_card *card = pdev->priv;
	struct net_device *dev;
	int i;

	free_irq(pdev->irq, card);

	for (i = 0; i < card->channels; i++) {
		dev = card->net_dev[i];
		if (!dev)
			continue;

		printk(KERN_INFO "%s: removing %s on channel #%d\n",
		       DRV_NAME, dev->name, i);
		unregister_sja1000dev(dev);
		free_sja1000dev(dev);
	}

	writeb(EMS_CMD_UMAP, card->base_addr);
	iounmap(card->base_addr);
	kfree(card);

	pdev->priv = NULL;
}

static int __devinit ems_pcmcia_add_card(struct pcmcia_device *pdev,
					 unsigned long base)
{
	struct sja1000_priv *priv;
	struct net_device *dev;
	struct ems_pcmcia_card *card;
	int err, i;

	
	card = kzalloc(sizeof(struct ems_pcmcia_card), GFP_KERNEL);
	if (!card)
		return -ENOMEM;

	pdev->priv = card;
	card->channels = 0;

	card->base_addr = ioremap(base, EMS_PCMCIA_MEM_SIZE);
	if (!card->base_addr) {
		err = -ENOMEM;
		goto failure_cleanup;
	}

	
	if (readw(card->base_addr) != 0xAA55) {
		err = -ENODEV;
		goto failure_cleanup;
	}

	
	writeb(EMS_CMD_RESET, card->base_addr);

	
	writeb(EMS_CMD_MAP, card->base_addr);

	
	for (i = 0; i < EMS_PCMCIA_MAX_CHAN; i++) {
		dev = alloc_sja1000dev(0);
		if (!dev) {
			err = -ENOMEM;
			goto failure_cleanup;
		}

		card->net_dev[i] = dev;
		priv = netdev_priv(dev);
		priv->priv = card;
		SET_NETDEV_DEV(dev, &pdev->dev);

		priv->irq_flags = IRQF_SHARED;
		dev->irq = pdev->irq;
		priv->reg_base = card->base_addr + EMS_PCMCIA_CAN_BASE_OFFSET +
			(i * EMS_PCMCIA_CAN_CTRL_SIZE);

		
		if (ems_pcmcia_check_chan(priv)) {
			priv->read_reg  = ems_pcmcia_read_reg;
			priv->write_reg = ems_pcmcia_write_reg;
			priv->can.clock.freq = EMS_PCMCIA_CAN_CLOCK;
			priv->ocr = EMS_PCMCIA_OCR;
			priv->cdr = EMS_PCMCIA_CDR;
			priv->flags |= SJA1000_CUSTOM_IRQ_HANDLER;

			
			err = register_sja1000dev(dev);
			if (err) {
				free_sja1000dev(dev);
				goto failure_cleanup;
			}

			card->channels++;

			printk(KERN_INFO "%s: registered %s on channel "
			       "#%d at 0x%p, irq %d\n", DRV_NAME, dev->name,
			       i, priv->reg_base, dev->irq);
		} else
			free_sja1000dev(dev);
	}

	err = request_irq(dev->irq, &ems_pcmcia_interrupt, IRQF_SHARED,
			  DRV_NAME, card);
	if (!err)
		return 0;

failure_cleanup:
	ems_pcmcia_del_card(pdev);
	return err;
}

static int __devinit ems_pcmcia_probe(struct pcmcia_device *dev)
{
	int csval;

	
	dev->config_flags |= CONF_ENABLE_IRQ;
	dev->config_index = 1;
	dev->config_regs = PRESENT_OPTION;

	
	dev->resource[0]->end = 16;
	dev->resource[0]->flags |= IO_DATA_PATH_WIDTH_8;
	dev->resource[1]->end = 16;
	dev->resource[1]->flags |= IO_DATA_PATH_WIDTH_16;
	dev->io_lines = 5;

	
	dev->resource[2]->flags =
		(WIN_DATA_WIDTH_8 | WIN_MEMORY_TYPE_CM | WIN_ENABLE);
	dev->resource[2]->start = dev->resource[2]->end = 0;

	csval = pcmcia_request_window(dev, dev->resource[2], 0);
	if (csval) {
		dev_err(&dev->dev, "pcmcia_request_window failed (err=%d)\n",
			csval);
		return 0;
	}

	csval = pcmcia_map_mem_page(dev, dev->resource[2], dev->config_base);
	if (csval) {
		dev_err(&dev->dev, "pcmcia_map_mem_page failed (err=%d)\n",
			csval);
		return 0;
	}

	csval = pcmcia_enable_device(dev);
	if (csval) {
		dev_err(&dev->dev, "pcmcia_enable_device failed (err=%d)\n",
			csval);
		return 0;
	}

	ems_pcmcia_add_card(dev, dev->resource[2]->start);
	return 0;
}

static void ems_pcmcia_remove(struct pcmcia_device *dev)
{
	ems_pcmcia_del_card(dev);
	pcmcia_disable_device(dev);
}

static struct pcmcia_driver ems_pcmcia_driver = {
	.name = DRV_NAME,
	.probe = ems_pcmcia_probe,
	.remove = ems_pcmcia_remove,
	.id_table = ems_pcmcia_tbl,
};

static int __init ems_pcmcia_init(void)
{
	return pcmcia_register_driver(&ems_pcmcia_driver);
}
module_init(ems_pcmcia_init);

static void __exit ems_pcmcia_exit(void)
{
	pcmcia_unregister_driver(&ems_pcmcia_driver);
}
module_exit(ems_pcmcia_exit);
