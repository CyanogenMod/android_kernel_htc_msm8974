/*
 * Driver for 802.11b cards using RAM-loadable Symbol firmware, such as
 * Symbol Wireless Networker LA4137, CompactFlash cards by Socket
 * Communications and Intel PRO/Wireless 2011B.
 *
 * The driver implements Symbol firmware download.  The rest is handled
 * in hermes.c and main.c.
 *
 * Utilities for downloading the Symbol firmware are available at
 * http://sourceforge.net/projects/orinoco/
 *
 * Copyright (C) 2002-2005 Pavel Roskin <proski@gnu.org>
 * Portions based on orinoco_cs.c:
 *	Copyright (C) David Gibson, Linuxcare Australia
 * Portions based on Spectrum24tDnld.c from original spectrum24 driver:
 *	Copyright (C) Symbol Technologies.
 *
 * See copyright notice in file main.c.
 */

#define DRIVER_NAME "spectrum_cs"
#define PFX DRIVER_NAME ": "

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>

#include "orinoco.h"


MODULE_AUTHOR("Pavel Roskin <proski@gnu.org>");
MODULE_DESCRIPTION("Driver for Symbol Spectrum24 Trilogy cards with firmware downloader");
MODULE_LICENSE("Dual MPL/GPL");


static int ignore_cis_vcc; 
module_param(ignore_cis_vcc, int, 0);
MODULE_PARM_DESC(ignore_cis_vcc, "Allow voltage mismatch between card and socket");


struct orinoco_pccard {
	struct pcmcia_device	*p_dev;
};


static int spectrum_cs_config(struct pcmcia_device *link);
static void spectrum_cs_release(struct pcmcia_device *link);

#define HCR_RUN		0x07	
#define HCR_IDLE	0x0E	
#define HCR_MEM16	0x10	


/*
 * Reset the card using configuration registers COR and CCSR.
 * If IDLE is 1, stop the firmware, so that it can be safely rewritten.
 */
static int
spectrum_reset(struct pcmcia_device *link, int idle)
{
	int ret;
	u8 save_cor;
	u8 ccsr;

	
	if (!pcmcia_dev_present(link))
		return -ENODEV;

	
	ret = pcmcia_read_config_byte(link, CISREG_COR, &save_cor);
	if (ret)
		goto failed;

	
	ret = pcmcia_write_config_byte(link, CISREG_COR,
				(save_cor | COR_SOFT_RESET));
	if (ret)
		goto failed;
	udelay(1000);

	
	ret = pcmcia_read_config_byte(link, CISREG_CCSR, &ccsr);
	if (ret)
		goto failed;

	ccsr = (idle ? HCR_IDLE : HCR_RUN) | (ccsr & HCR_MEM16);
	ret = pcmcia_write_config_byte(link, CISREG_CCSR, ccsr);
	if (ret)
		goto failed;
	udelay(1000);

	
	ret = pcmcia_write_config_byte(link, CISREG_COR,
				(save_cor & ~COR_SOFT_RESET));
	if (ret)
		goto failed;
	udelay(1000);
	return 0;

failed:
	return -ENODEV;
}


static int
spectrum_cs_hard_reset(struct orinoco_private *priv)
{
	struct orinoco_pccard *card = priv->card;
	struct pcmcia_device *link = card->p_dev;

	
	spectrum_reset(link, 0);

	return 0;
}

static int
spectrum_cs_stop_firmware(struct orinoco_private *priv, int idle)
{
	struct orinoco_pccard *card = priv->card;
	struct pcmcia_device *link = card->p_dev;

	return spectrum_reset(link, idle);
}


static int
spectrum_cs_probe(struct pcmcia_device *link)
{
	struct orinoco_private *priv;
	struct orinoco_pccard *card;

	priv = alloc_orinocodev(sizeof(*card), &link->dev,
				spectrum_cs_hard_reset,
				spectrum_cs_stop_firmware);
	if (!priv)
		return -ENOMEM;
	card = priv->card;

	
	card->p_dev = link;
	link->priv = priv;

	return spectrum_cs_config(link);
}				

static void spectrum_cs_detach(struct pcmcia_device *link)
{
	struct orinoco_private *priv = link->priv;

	orinoco_if_del(priv);

	spectrum_cs_release(link);

	free_orinocodev(priv);
}				

static int spectrum_cs_config_check(struct pcmcia_device *p_dev,
				    void *priv_data)
{
	if (p_dev->config_index == 0)
		return -EINVAL;

	return pcmcia_request_io(p_dev);
};

static int
spectrum_cs_config(struct pcmcia_device *link)
{
	struct orinoco_private *priv = link->priv;
	struct hermes *hw = &priv->hw;
	int ret;
	void __iomem *mem;

	link->config_flags |= CONF_AUTO_SET_VPP | CONF_AUTO_CHECK_VCC |
		CONF_AUTO_SET_IO | CONF_ENABLE_IRQ;
	if (ignore_cis_vcc)
		link->config_flags &= ~CONF_AUTO_CHECK_VCC;
	ret = pcmcia_loop_config(link, spectrum_cs_config_check, NULL);
	if (ret) {
		if (!ignore_cis_vcc)
			printk(KERN_ERR PFX "GetNextTuple(): No matching "
			       "CIS configuration.  Maybe you need the "
			       "ignore_cis_vcc=1 parameter.\n");
		goto failed;
	}

	mem = ioport_map(link->resource[0]->start,
			resource_size(link->resource[0]));
	if (!mem)
		goto failed;

	hermes_struct_init(hw, mem, HERMES_16BIT_REGSPACING);
	hw->eeprom_pda = true;

	ret = pcmcia_request_irq(link, orinoco_interrupt);
	if (ret)
		goto failed;

	ret = pcmcia_enable_device(link);
	if (ret)
		goto failed;

	
	if (spectrum_cs_hard_reset(priv) != 0)
		goto failed;

	
	if (orinoco_init(priv) != 0) {
		printk(KERN_ERR PFX "orinoco_init() failed\n");
		goto failed;
	}

	
	if (orinoco_if_add(priv, link->resource[0]->start,
			   link->irq, NULL) != 0) {
		printk(KERN_ERR PFX "orinoco_if_add() failed\n");
		goto failed;
	}

	return 0;

 failed:
	spectrum_cs_release(link);
	return -ENODEV;
}				

static void
spectrum_cs_release(struct pcmcia_device *link)
{
	struct orinoco_private *priv = link->priv;
	unsigned long flags;

	priv->hw.ops->lock_irqsave(&priv->lock, &flags);
	priv->hw_unavailable++;
	priv->hw.ops->unlock_irqrestore(&priv->lock, &flags);

	pcmcia_disable_device(link);
	if (priv->hw.iobase)
		ioport_unmap(priv->hw.iobase);
}				


static int
spectrum_cs_suspend(struct pcmcia_device *link)
{
	struct orinoco_private *priv = link->priv;
	int err = 0;

	
	orinoco_down(priv);

	return err;
}

static int
spectrum_cs_resume(struct pcmcia_device *link)
{
	struct orinoco_private *priv = link->priv;
	int err = orinoco_up(priv);

	return err;
}



static const struct pcmcia_device_id spectrum_cs_ids[] = {
	PCMCIA_DEVICE_MANF_CARD(0x026c, 0x0001), 
	PCMCIA_DEVICE_MANF_CARD(0x0104, 0x0001), 
	PCMCIA_DEVICE_PROD_ID12("Intel", "PRO/Wireless LAN PC Card", 0x816cc815, 0x6fbf459a), 
	PCMCIA_DEVICE_NULL,
};
MODULE_DEVICE_TABLE(pcmcia, spectrum_cs_ids);

static struct pcmcia_driver orinoco_driver = {
	.owner		= THIS_MODULE,
	.name		= DRIVER_NAME,
	.probe		= spectrum_cs_probe,
	.remove		= spectrum_cs_detach,
	.suspend	= spectrum_cs_suspend,
	.resume		= spectrum_cs_resume,
	.id_table       = spectrum_cs_ids,
};

static int __init
init_spectrum_cs(void)
{
	return pcmcia_register_driver(&orinoco_driver);
}

static void __exit
exit_spectrum_cs(void)
{
	pcmcia_unregister_driver(&orinoco_driver);
}

module_init(init_spectrum_cs);
module_exit(exit_spectrum_cs);
