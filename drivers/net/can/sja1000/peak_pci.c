/*
 * Copyright (C) 2007, 2011 Wolfgang Grandegger <wg@grandegger.com>
 * Copyright (C) 2012 Stephane Grosjean <s.grosjean@peak-system.com>
 *
 * Derived from the PCAN project file driver/src/pcan_pci.c:
 *
 * Copyright (C) 2001-2006  PEAK System-Technik GmbH
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
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/can.h>
#include <linux/can/dev.h>

#include "sja1000.h"

MODULE_AUTHOR("Wolfgang Grandegger <wg@grandegger.com>");
MODULE_DESCRIPTION("Socket-CAN driver for PEAK PCAN PCI family cards");
MODULE_SUPPORTED_DEVICE("PEAK PCAN PCI/PCIe/PCIeC miniPCI CAN cards");
MODULE_LICENSE("GPL v2");

#define DRV_NAME  "peak_pci"

struct peak_pciec_card;
struct peak_pci_chan {
	void __iomem *cfg_base;		
	struct net_device *prev_dev;	
	u16 icr_mask;			
	struct peak_pciec_card *pciec_card;	
};

#define PEAK_PCI_CAN_CLOCK	(16000000 / 2)

#define PEAK_PCI_CDR		(CDR_CBP | CDR_CLKOUT_MASK)
#define PEAK_PCI_OCR		OCR_TX0_PUSHPULL

#define PITA_ICR		0x00	
#define PITA_GPIOICR		0x18	
#define PITA_MISC		0x1C	

#define PEAK_PCI_CFG_SIZE	0x1000	
#define PEAK_PCI_CHAN_SIZE	0x0400	

#define PEAK_PCI_VENDOR_ID	0x001C	
#define PEAK_PCI_DEVICE_ID	0x0001	
#define PEAK_PCIEC_DEVICE_ID	0x0002	
#define PEAK_PCIE_DEVICE_ID	0x0003	
#define PEAK_MPCI_DEVICE_ID	0x0008	

#define PEAK_PCI_CHAN_MAX	4

static const u16 peak_pci_icr_masks[PEAK_PCI_CHAN_MAX] = {
	0x02, 0x01, 0x40, 0x80
};

static DEFINE_PCI_DEVICE_TABLE(peak_pci_tbl) = {
	{PEAK_PCI_VENDOR_ID, PEAK_PCI_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID,},
	{PEAK_PCI_VENDOR_ID, PEAK_PCIE_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID,},
	{PEAK_PCI_VENDOR_ID, PEAK_MPCI_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID,},
#ifdef CONFIG_CAN_PEAK_PCIEC
	{PEAK_PCI_VENDOR_ID, PEAK_PCIEC_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID,},
#endif
	{0,}
};

MODULE_DEVICE_TABLE(pci, peak_pci_tbl);

#ifdef CONFIG_CAN_PEAK_PCIEC

#define PITA_GPOUT		0x18	
#define PITA_GPIN		0x19	
#define PITA_GPOEN		0x1A	

#define PITA_GPIN_SCL		0x01	
#define PITA_GPIN_SDA		0x04	

#define PCA9553_1_SLAVEADDR	(0xC4 >> 1)

enum {
	PCA9553_LOW,
	PCA9553_HIGHZ,
	PCA9553_PWM0,
	PCA9553_PWM1
};

#define PCA9553_ON		PCA9553_LOW
#define PCA9553_OFF		PCA9553_HIGHZ
#define PCA9553_SLOW		PCA9553_PWM0
#define PCA9553_FAST		PCA9553_PWM1

#define PCA9553_LED(c)		(1 << (c))
#define PCA9553_LED_STATE(s, c)	((s) << ((c) << 1))

#define PCA9553_LED_ON(c)	PCA9553_LED_STATE(PCA9553_ON, c)
#define PCA9553_LED_OFF(c)	PCA9553_LED_STATE(PCA9553_OFF, c)
#define PCA9553_LED_SLOW(c)	PCA9553_LED_STATE(PCA9553_SLOW, c)
#define PCA9553_LED_FAST(c)	PCA9553_LED_STATE(PCA9553_FAST, c)
#define PCA9553_LED_MASK(c)	PCA9553_LED_STATE(0x03, c)

#define PCA9553_LED_OFF_ALL	(PCA9553_LED_OFF(0) | PCA9553_LED_OFF(1))

#define PCA9553_LS0_INIT	0x40 

struct peak_pciec_chan {
	struct net_device *netdev;
	unsigned long prev_rx_bytes;
	unsigned long prev_tx_bytes;
};

struct peak_pciec_card {
	void __iomem *cfg_base;		
	void __iomem *reg_base;		
	u8 led_cache;			

	
	struct i2c_algo_bit_data i2c_bit;
	struct i2c_adapter led_chip;
	struct delayed_work led_work;	
	int chan_count;
	struct peak_pciec_chan channel[PEAK_PCI_CHAN_MAX];
};

static void peak_pci_write_reg(const struct sja1000_priv *priv,
			       int port, u8 val);

static inline void pita_set_scl_highz(struct peak_pciec_card *card)
{
	u8 gp_outen = readb(card->cfg_base + PITA_GPOEN) & ~PITA_GPIN_SCL;
	writeb(gp_outen, card->cfg_base + PITA_GPOEN);
}

static inline void pita_set_sda_highz(struct peak_pciec_card *card)
{
	u8 gp_outen = readb(card->cfg_base + PITA_GPOEN) & ~PITA_GPIN_SDA;
	writeb(gp_outen, card->cfg_base + PITA_GPOEN);
}

static void peak_pciec_init_pita_gpio(struct peak_pciec_card *card)
{
	
	pita_set_scl_highz(card);
	pita_set_sda_highz(card);
}

static void pita_setsda(void *data, int state)
{
	struct peak_pciec_card *card = (struct peak_pciec_card *)data;
	u8 gp_out, gp_outen;

	
	gp_out = readb(card->cfg_base + PITA_GPOUT) & ~PITA_GPIN_SDA;
	writeb(gp_out, card->cfg_base + PITA_GPOUT);

	
	gp_outen = readb(card->cfg_base + PITA_GPOEN);
	if (state)
		gp_outen &= ~PITA_GPIN_SDA;
	else
		gp_outen |= PITA_GPIN_SDA;

	writeb(gp_outen, card->cfg_base + PITA_GPOEN);
}

static void pita_setscl(void *data, int state)
{
	struct peak_pciec_card *card = (struct peak_pciec_card *)data;
	u8 gp_out, gp_outen;

	
	gp_out = readb(card->cfg_base + PITA_GPOUT) & ~PITA_GPIN_SCL;
	writeb(gp_out, card->cfg_base + PITA_GPOUT);

	
	gp_outen = readb(card->cfg_base + PITA_GPOEN);
	if (state)
		gp_outen &= ~PITA_GPIN_SCL;
	else
		gp_outen |= PITA_GPIN_SCL;

	writeb(gp_outen, card->cfg_base + PITA_GPOEN);
}

static int pita_getsda(void *data)
{
	struct peak_pciec_card *card = (struct peak_pciec_card *)data;

	
	pita_set_sda_highz(card);

	return (readb(card->cfg_base + PITA_GPIN) & PITA_GPIN_SDA) ? 1 : 0;
}

static int pita_getscl(void *data)
{
	struct peak_pciec_card *card = (struct peak_pciec_card *)data;

	
	pita_set_scl_highz(card);

	return (readb(card->cfg_base + PITA_GPIN) & PITA_GPIN_SCL) ? 1 : 0;
}

static int peak_pciec_write_pca9553(struct peak_pciec_card *card,
				    u8 offset, u8 data)
{
	u8 buffer[2] = {
		offset,
		data
	};
	struct i2c_msg msg = {
		.addr = PCA9553_1_SLAVEADDR,
		.len = 2,
		.buf = buffer,
	};
	int ret;

	
	if ((offset == 5) && (data == card->led_cache))
		return 0;

	ret = i2c_transfer(&card->led_chip, &msg, 1);
	if (ret < 0)
		return ret;

	if (offset == 5)
		card->led_cache = data;

	return 0;
}

static void peak_pciec_led_work(struct work_struct *work)
{
	struct peak_pciec_card *card =
		container_of(work, struct peak_pciec_card, led_work.work);
	struct net_device *netdev;
	u8 new_led = card->led_cache;
	int i, up_count = 0;

	
	for (i = 0; i < card->chan_count; i++) {
		
		new_led &= ~PCA9553_LED_MASK(i);
		new_led |= PCA9553_LED_ON(i);

		netdev = card->channel[i].netdev;
		if (!netdev || !(netdev->flags & IFF_UP))
			continue;

		up_count++;

		
		new_led &= ~PCA9553_LED_MASK(i);
		new_led |= PCA9553_LED_SLOW(i);

		
		if (netdev->stats.rx_bytes != card->channel[i].prev_rx_bytes) {
			card->channel[i].prev_rx_bytes = netdev->stats.rx_bytes;
			new_led &= ~PCA9553_LED_MASK(i);
			new_led |= PCA9553_LED_FAST(i);
		}
		if (netdev->stats.tx_bytes != card->channel[i].prev_tx_bytes) {
			card->channel[i].prev_tx_bytes = netdev->stats.tx_bytes;
			new_led &= ~PCA9553_LED_MASK(i);
			new_led |= PCA9553_LED_FAST(i);
		}
	}

	
	peak_pciec_write_pca9553(card, 5, new_led);

	
	if (up_count)
		schedule_delayed_work(&card->led_work, HZ);
}

static void peak_pciec_set_leds(struct peak_pciec_card *card, u8 led_mask, u8 s)
{
	u8 new_led = card->led_cache;
	int i;

	
	for (i = 0; i < card->chan_count; i++)
		if (led_mask & PCA9553_LED(i)) {
			new_led &= ~PCA9553_LED_MASK(i);
			new_led |= PCA9553_LED_STATE(s, i);
		}

	
	peak_pciec_write_pca9553(card, 5, new_led);
}

static void peak_pciec_start_led_work(struct peak_pciec_card *card)
{
	if (!delayed_work_pending(&card->led_work))
		schedule_delayed_work(&card->led_work, HZ);
}

static void peak_pciec_stop_led_work(struct peak_pciec_card *card)
{
	cancel_delayed_work_sync(&card->led_work);
}

static int peak_pciec_init_leds(struct peak_pciec_card *card)
{
	int err;

	
	err = peak_pciec_write_pca9553(card, 1, 44 / 1);
	if (err)
		return err;

	
	err = peak_pciec_write_pca9553(card, 2, 0x80);
	if (err)
		return err;

	
	err = peak_pciec_write_pca9553(card, 3, 44 / 5);
	if (err)
		return err;

	
	err = peak_pciec_write_pca9553(card, 4, 0x80);
	if (err)
		return err;

	
	return peak_pciec_write_pca9553(card, 5, PCA9553_LS0_INIT);
}

static void peak_pciec_leds_exit(struct peak_pciec_card *card)
{
	
	peak_pciec_write_pca9553(card, 5, PCA9553_LED_OFF_ALL);
}

static void peak_pciec_write_reg(const struct sja1000_priv *priv,
				 int port, u8 val)
{
	struct peak_pci_chan *chan = priv->priv;
	struct peak_pciec_card *card = chan->pciec_card;
	int c = (priv->reg_base - card->reg_base) / PEAK_PCI_CHAN_SIZE;

	
	if (port == REG_MOD)
		switch (val) {
		case MOD_RM:
			
			peak_pciec_set_leds(card, PCA9553_LED(c), PCA9553_ON);
			break;
		case 0x00:
			
			peak_pciec_set_leds(card, PCA9553_LED(c), PCA9553_SLOW);
			peak_pciec_start_led_work(card);
			break;
		default:
			break;
		}

	
	peak_pci_write_reg(priv, port, val);
}

static struct i2c_algo_bit_data peak_pciec_i2c_bit_ops = {
	.setsda	= pita_setsda,
	.setscl	= pita_setscl,
	.getsda	= pita_getsda,
	.getscl	= pita_getscl,
	.udelay	= 10,
	.timeout = HZ,
};

static int peak_pciec_probe(struct pci_dev *pdev, struct net_device *dev)
{
	struct sja1000_priv *priv = netdev_priv(dev);
	struct peak_pci_chan *chan = priv->priv;
	struct peak_pciec_card *card;
	int err;

	
	if (chan->prev_dev) {
		struct sja1000_priv *prev_priv = netdev_priv(chan->prev_dev);
		struct peak_pci_chan *prev_chan = prev_priv->priv;

		card = prev_chan->pciec_card;
		if (!card)
			return -ENODEV;

	
	} else {
		
		card = kzalloc(sizeof(struct peak_pciec_card), GFP_KERNEL);
		if (!card) {
			dev_err(&pdev->dev,
				 "failed allocating memory for i2c chip\n");
			return -ENOMEM;
		}

		card->cfg_base = chan->cfg_base;
		card->reg_base = priv->reg_base;

		card->led_chip.owner = THIS_MODULE;
		card->led_chip.dev.parent = &pdev->dev;
		card->led_chip.algo_data = &card->i2c_bit;
		strncpy(card->led_chip.name, "peak_i2c",
			sizeof(card->led_chip.name));

		card->i2c_bit = peak_pciec_i2c_bit_ops;
		card->i2c_bit.udelay = 10;
		card->i2c_bit.timeout = HZ;
		card->i2c_bit.data = card;

		peak_pciec_init_pita_gpio(card);

		err = i2c_bit_add_bus(&card->led_chip);
		if (err) {
			dev_err(&pdev->dev, "i2c init failed\n");
			goto pciec_init_err_1;
		}

		err = peak_pciec_init_leds(card);
		if (err) {
			dev_err(&pdev->dev, "leds hardware init failed\n");
			goto pciec_init_err_2;
		}

		INIT_DELAYED_WORK(&card->led_work, peak_pciec_led_work);
		
		priv->write_reg = peak_pciec_write_reg;
	}

	chan->pciec_card = card;
	card->channel[card->chan_count++].netdev = dev;

	return 0;

pciec_init_err_2:
	i2c_del_adapter(&card->led_chip);

pciec_init_err_1:
	peak_pciec_init_pita_gpio(card);
	kfree(card);

	return err;
}

static void peak_pciec_remove(struct peak_pciec_card *card)
{
	peak_pciec_stop_led_work(card);
	peak_pciec_leds_exit(card);
	i2c_del_adapter(&card->led_chip);
	peak_pciec_init_pita_gpio(card);
	kfree(card);
}

#else 

static inline int peak_pciec_probe(struct pci_dev *pdev, struct net_device *dev)
{
	return -ENODEV;
}

static inline void peak_pciec_remove(struct peak_pciec_card *card)
{
}
#endif 

static u8 peak_pci_read_reg(const struct sja1000_priv *priv, int port)
{
	return readb(priv->reg_base + (port << 2));
}

static void peak_pci_write_reg(const struct sja1000_priv *priv,
			       int port, u8 val)
{
	writeb(val, priv->reg_base + (port << 2));
}

static void peak_pci_post_irq(const struct sja1000_priv *priv)
{
	struct peak_pci_chan *chan = priv->priv;
	u16 icr;

	
	icr = readw(chan->cfg_base + PITA_ICR);
	if (icr & chan->icr_mask)
		writew(chan->icr_mask, chan->cfg_base + PITA_ICR);
}

static int __devinit peak_pci_probe(struct pci_dev *pdev,
				    const struct pci_device_id *ent)
{
	struct sja1000_priv *priv;
	struct peak_pci_chan *chan;
	struct net_device *dev;
	void __iomem *cfg_base, *reg_base;
	u16 sub_sys_id, icr;
	int i, err, channels;

	err = pci_enable_device(pdev);
	if (err)
		return err;

	err = pci_request_regions(pdev, DRV_NAME);
	if (err)
		goto failure_disable_pci;

	err = pci_read_config_word(pdev, 0x2e, &sub_sys_id);
	if (err)
		goto failure_release_regions;

	dev_dbg(&pdev->dev, "probing device %04x:%04x:%04x\n",
		pdev->vendor, pdev->device, sub_sys_id);

	err = pci_write_config_word(pdev, 0x44, 0);
	if (err)
		goto failure_release_regions;

	if (sub_sys_id >= 12)
		channels = 4;
	else if (sub_sys_id >= 10)
		channels = 3;
	else if (sub_sys_id >= 4)
		channels = 2;
	else
		channels = 1;

	cfg_base = pci_iomap(pdev, 0, PEAK_PCI_CFG_SIZE);
	if (!cfg_base) {
		dev_err(&pdev->dev, "failed to map PCI resource #0\n");
		goto failure_release_regions;
	}

	reg_base = pci_iomap(pdev, 1, PEAK_PCI_CHAN_SIZE * channels);
	if (!reg_base) {
		dev_err(&pdev->dev, "failed to map PCI resource #1\n");
		goto failure_unmap_cfg_base;
	}

	
	writew(0x0005, cfg_base + PITA_GPIOICR + 2);
	
	writeb(0x00, cfg_base + PITA_GPIOICR);
	
	writeb(0x05, cfg_base + PITA_MISC + 3);
	mdelay(5);
	
	writeb(0x04, cfg_base + PITA_MISC + 3);

	icr = readw(cfg_base + PITA_ICR + 2);

	for (i = 0; i < channels; i++) {
		dev = alloc_sja1000dev(sizeof(struct peak_pci_chan));
		if (!dev) {
			err = -ENOMEM;
			goto failure_remove_channels;
		}

		priv = netdev_priv(dev);
		chan = priv->priv;

		chan->cfg_base = cfg_base;
		priv->reg_base = reg_base + i * PEAK_PCI_CHAN_SIZE;

		priv->read_reg = peak_pci_read_reg;
		priv->write_reg = peak_pci_write_reg;
		priv->post_irq = peak_pci_post_irq;

		priv->can.clock.freq = PEAK_PCI_CAN_CLOCK;
		priv->ocr = PEAK_PCI_OCR;
		priv->cdr = PEAK_PCI_CDR;
		
		if (channels == 1 || i > 0)
			priv->cdr |= CDR_CLK_OFF;

		
		priv->irq_flags = IRQF_SHARED;
		dev->irq = pdev->irq;

		chan->icr_mask = peak_pci_icr_masks[i];
		icr |= chan->icr_mask;

		SET_NETDEV_DEV(dev, &pdev->dev);

		
		chan->prev_dev = pci_get_drvdata(pdev);
		pci_set_drvdata(pdev, dev);

		if (pdev->device == PEAK_PCIEC_DEVICE_ID) {
			err = peak_pciec_probe(pdev, dev);
			if (err) {
				dev_err(&pdev->dev,
					"failed to probe device (err %d)\n",
					err);
				goto failure_free_dev;
			}
		}

		err = register_sja1000dev(dev);
		if (err) {
			dev_err(&pdev->dev, "failed to register device\n");
			goto failure_free_dev;
		}

		dev_info(&pdev->dev,
			 "%s at reg_base=0x%p cfg_base=0x%p irq=%d\n",
			 dev->name, priv->reg_base, chan->cfg_base, dev->irq);
	}

	
	writew(icr, cfg_base + PITA_ICR + 2);

	return 0;

failure_free_dev:
	pci_set_drvdata(pdev, chan->prev_dev);
	free_sja1000dev(dev);

failure_remove_channels:
	
	writew(0x0, cfg_base + PITA_ICR + 2);

	chan = NULL;
	for (dev = pci_get_drvdata(pdev); dev; dev = chan->prev_dev) {
		unregister_sja1000dev(dev);
		free_sja1000dev(dev);
		priv = netdev_priv(dev);
		chan = priv->priv;
	}

	
	if (chan && chan->pciec_card)
		peak_pciec_remove(chan->pciec_card);

	pci_iounmap(pdev, reg_base);

failure_unmap_cfg_base:
	pci_iounmap(pdev, cfg_base);

failure_release_regions:
	pci_release_regions(pdev);

failure_disable_pci:
	pci_disable_device(pdev);

	return err;
}

static void __devexit peak_pci_remove(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev); 
	struct sja1000_priv *priv = netdev_priv(dev);
	struct peak_pci_chan *chan = priv->priv;
	void __iomem *cfg_base = chan->cfg_base;
	void __iomem *reg_base = priv->reg_base;

	
	writew(0x0, cfg_base + PITA_ICR + 2);

	
	while (1) {
		dev_info(&pdev->dev, "removing device %s\n", dev->name);
		unregister_sja1000dev(dev);
		free_sja1000dev(dev);
		dev = chan->prev_dev;

		if (!dev) {
			
			if (chan->pciec_card)
				peak_pciec_remove(chan->pciec_card);
			break;
		}
		priv = netdev_priv(dev);
		chan = priv->priv;
	}

	pci_iounmap(pdev, reg_base);
	pci_iounmap(pdev, cfg_base);
	pci_release_regions(pdev);
	pci_disable_device(pdev);

	pci_set_drvdata(pdev, NULL);
}

static struct pci_driver peak_pci_driver = {
	.name = DRV_NAME,
	.id_table = peak_pci_tbl,
	.probe = peak_pci_probe,
	.remove = __devexit_p(peak_pci_remove),
};

static int __init peak_pci_init(void)
{
	return pci_register_driver(&peak_pci_driver);
}
module_init(peak_pci_init);

static void __exit peak_pci_exit(void)
{
	pci_unregister_driver(&peak_pci_driver);
}
module_exit(peak_pci_exit);
