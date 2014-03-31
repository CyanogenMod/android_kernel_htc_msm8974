/*
 *	PCI handling of I2O controller
 *
 * 	Copyright (C) 1999-2002	Red Hat Software
 *
 *	Written by Alan Cox, Building Number Three Ltd
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation; either version 2 of the License, or (at your
 *	option) any later version.
 *
 *	A lot of the I2O message side code from this is taken from the Red
 *	Creek RCPCI45 adapter driver by Red Creek Communications
 *
 *	Fixes/additions:
 *		Philipp Rumpf
 *		Juha Sievänen <Juha.Sievanen@cs.Helsinki.FI>
 *		Auvo Häkkinen <Auvo.Hakkinen@cs.Helsinki.FI>
 *		Deepak Saxena <deepak@plexity.net>
 *		Boji T Kannanthanam <boji.t.kannanthanam@intel.com>
 *		Alan Cox <alan@lxorguk.ukuu.org.uk>:
 *			Ported to Linux 2.5.
 *		Markus Lidel <Markus.Lidel@shadowconnect.com>:
 *			Minor fixes for 2.6.
 *		Markus Lidel <Markus.Lidel@shadowconnect.com>:
 *			Support for sysfs included.
 */

#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2o.h>
#include <linux/module.h>
#include "core.h"

#define OSM_DESCRIPTION	"I2O-subsystem"

static struct pci_device_id __devinitdata i2o_pci_ids[] = {
	{PCI_DEVICE_CLASS(PCI_CLASS_INTELLIGENT_I2O << 8, 0xffff00)},
	{PCI_DEVICE(PCI_VENDOR_ID_DPT, 0xa511)},
	{.vendor = PCI_VENDOR_ID_INTEL,.device = 0x1962,
	 .subvendor = PCI_VENDOR_ID_PROMISE,.subdevice = PCI_ANY_ID},
	{0}
};

static void i2o_pci_free(struct i2o_controller *c)
{
	struct device *dev;

	dev = &c->pdev->dev;

	i2o_dma_free(dev, &c->out_queue);
	i2o_dma_free(dev, &c->status_block);
	kfree(c->lct);
	i2o_dma_free(dev, &c->dlct);
	i2o_dma_free(dev, &c->hrt);
	i2o_dma_free(dev, &c->status);

	if (c->raptor && c->in_queue.virt)
		iounmap(c->in_queue.virt);

	if (c->base.virt)
		iounmap(c->base.virt);

	pci_release_regions(c->pdev);
}

static int __devinit i2o_pci_alloc(struct i2o_controller *c)
{
	struct pci_dev *pdev = c->pdev;
	struct device *dev = &pdev->dev;
	int i;

	if (pci_request_regions(pdev, OSM_DESCRIPTION)) {
		printk(KERN_ERR "%s: device already claimed\n", c->name);
		return -ENODEV;
	}

	for (i = 0; i < 6; i++) {
		
		if (!(pci_resource_flags(pdev, i) & IORESOURCE_IO)) {
			if (!c->base.phys) {
				c->base.phys = pci_resource_start(pdev, i);
				c->base.len = pci_resource_len(pdev, i);

				if (pdev->device == 0xa501) {
					if (pdev->subsystem_device >= 0xc032 &&
					    pdev->subsystem_device <= 0xc03b) {
						if (c->base.len > 0x400000)
							c->base.len = 0x400000;
					} else {
						if (c->base.len > 0x100000)
							c->base.len = 0x100000;
					}
				}
				if (!c->raptor)
					break;
			} else {
				c->in_queue.phys = pci_resource_start(pdev, i);
				c->in_queue.len = pci_resource_len(pdev, i);
				break;
			}
		}
	}

	if (i == 6) {
		printk(KERN_ERR "%s: I2O controller has no memory regions"
		       " defined.\n", c->name);
		i2o_pci_free(c);
		return -EINVAL;
	}

	
	if (c->raptor) {
		printk(KERN_INFO "%s: PCI I2O controller\n", c->name);
		printk(KERN_INFO "     BAR0 at 0x%08lX size=%ld\n",
		       (unsigned long)c->base.phys, (unsigned long)c->base.len);
		printk(KERN_INFO "     BAR1 at 0x%08lX size=%ld\n",
		       (unsigned long)c->in_queue.phys,
		       (unsigned long)c->in_queue.len);
	} else
		printk(KERN_INFO "%s: PCI I2O controller at %08lX size=%ld\n",
		       c->name, (unsigned long)c->base.phys,
		       (unsigned long)c->base.len);

	c->base.virt = ioremap_nocache(c->base.phys, c->base.len);
	if (!c->base.virt) {
		printk(KERN_ERR "%s: Unable to map controller.\n", c->name);
		i2o_pci_free(c);
		return -ENOMEM;
	}

	if (c->raptor) {
		c->in_queue.virt =
		    ioremap_nocache(c->in_queue.phys, c->in_queue.len);
		if (!c->in_queue.virt) {
			printk(KERN_ERR "%s: Unable to map controller.\n",
			       c->name);
			i2o_pci_free(c);
			return -ENOMEM;
		}
	} else
		c->in_queue = c->base;

	c->irq_status = c->base.virt + I2O_IRQ_STATUS;
	c->irq_mask = c->base.virt + I2O_IRQ_MASK;
	c->in_port = c->base.virt + I2O_IN_PORT;
	c->out_port = c->base.virt + I2O_OUT_PORT;

	
	if (pdev->vendor == PCI_VENDOR_ID_MOTOROLA && pdev->device == 0x18c0) {
		
		if (be32_to_cpu(readl(c->base.virt + 0x10000)) & 0x10000000) {
			printk(KERN_INFO "%s: MPC82XX needs CPU running to "
			       "service I2O.\n", c->name);
			i2o_pci_free(c);
			return -ENODEV;
		} else {
			c->irq_status += I2O_MOTOROLA_PORT_OFFSET;
			c->irq_mask += I2O_MOTOROLA_PORT_OFFSET;
			c->in_port += I2O_MOTOROLA_PORT_OFFSET;
			c->out_port += I2O_MOTOROLA_PORT_OFFSET;
			printk(KERN_INFO "%s: MPC82XX workarounds activated.\n",
			       c->name);
		}
	}

	if (i2o_dma_alloc(dev, &c->status, 8)) {
		i2o_pci_free(c);
		return -ENOMEM;
	}

	if (i2o_dma_alloc(dev, &c->hrt, sizeof(i2o_hrt))) {
		i2o_pci_free(c);
		return -ENOMEM;
	}

	if (i2o_dma_alloc(dev, &c->dlct, 8192)) {
		i2o_pci_free(c);
		return -ENOMEM;
	}

	if (i2o_dma_alloc(dev, &c->status_block, sizeof(i2o_status_block))) {
		i2o_pci_free(c);
		return -ENOMEM;
	}

	if (i2o_dma_alloc(dev, &c->out_queue,
		I2O_MAX_OUTBOUND_MSG_FRAMES * I2O_OUTBOUND_MSG_FRAME_SIZE *
				sizeof(u32))) {
		i2o_pci_free(c);
		return -ENOMEM;
	}

	pci_set_drvdata(pdev, c);

	return 0;
}

static irqreturn_t i2o_pci_interrupt(int irq, void *dev_id)
{
	struct i2o_controller *c = dev_id;
	u32 m;
	irqreturn_t rc = IRQ_NONE;

	while (readl(c->irq_status) & I2O_IRQ_OUTBOUND_POST) {
		m = readl(c->out_port);
		if (m == I2O_QUEUE_EMPTY) {
			m = readl(c->out_port);
			if (unlikely(m == I2O_QUEUE_EMPTY))
				break;
		}

		
		if (i2o_driver_dispatch(c, m))
			
			i2o_flush_reply(c, m);

		rc = IRQ_HANDLED;
	}

	return rc;
}

static int i2o_pci_irq_enable(struct i2o_controller *c)
{
	struct pci_dev *pdev = c->pdev;
	int rc;

	writel(0xffffffff, c->irq_mask);

	if (pdev->irq) {
		rc = request_irq(pdev->irq, i2o_pci_interrupt, IRQF_SHARED,
				 c->name, c);
		if (rc < 0) {
			printk(KERN_ERR "%s: unable to allocate interrupt %d."
			       "\n", c->name, pdev->irq);
			return rc;
		}
	}

	writel(0x00000000, c->irq_mask);

	printk(KERN_INFO "%s: Installed at IRQ %d\n", c->name, pdev->irq);

	return 0;
}

static void i2o_pci_irq_disable(struct i2o_controller *c)
{
	writel(0xffffffff, c->irq_mask);

	if (c->pdev->irq > 0)
		free_irq(c->pdev->irq, c);
}

static int __devinit i2o_pci_probe(struct pci_dev *pdev,
				   const struct pci_device_id *id)
{
	struct i2o_controller *c;
	int rc;
	struct pci_dev *i960 = NULL;

	printk(KERN_INFO "i2o: Checking for PCI I2O controllers...\n");

	if ((pdev->class & 0xff) > 1) {
		printk(KERN_WARNING "i2o: %s does not support I2O 1.5 "
		       "(skipping).\n", pci_name(pdev));
		return -ENODEV;
	}

	if ((rc = pci_enable_device(pdev))) {
		printk(KERN_WARNING "i2o: couldn't enable device %s\n",
		       pci_name(pdev));
		return rc;
	}

	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		printk(KERN_WARNING "i2o: no suitable DMA found for %s\n",
		       pci_name(pdev));
		rc = -ENODEV;
		goto disable;
	}

	pci_set_master(pdev);

	c = i2o_iop_alloc();
	if (IS_ERR(c)) {
		printk(KERN_ERR "i2o: couldn't allocate memory for %s\n",
		       pci_name(pdev));
		rc = PTR_ERR(c);
		goto disable;
	} else
		printk(KERN_INFO "%s: controller found (%s)\n", c->name,
		       pci_name(pdev));

	c->pdev = pdev;
	c->device.parent = &pdev->dev;

	
	if (pdev->vendor == PCI_VENDOR_ID_NCR && pdev->device == 0x0630) {
		c->short_req = 1;
		printk(KERN_INFO "%s: Symbios FC920 workarounds activated.\n",
		       c->name);
	}

	if (pdev->subsystem_vendor == PCI_VENDOR_ID_PROMISE) {
		i960 = pci_get_slot(c->pdev->bus,
				  PCI_DEVFN(PCI_SLOT(c->pdev->devfn), 0));

		if (i960) {
			pci_write_config_word(i960, 0x42, 0);
			pci_dev_put(i960);
		}

		c->promise = 1;
		c->limit_sectors = 1;
	}

	if (pdev->subsystem_vendor == PCI_VENDOR_ID_DPT)
		c->adaptec = 1;

	
	if (pdev->vendor == PCI_VENDOR_ID_DPT) {
		c->no_quiesce = 1;
		if (pdev->device == 0xa511)
			c->raptor = 1;

		if (pdev->subsystem_device == 0xc05a) {
			c->limit_sectors = 1;
			printk(KERN_INFO
			       "%s: limit sectors per request to %d\n", c->name,
			       I2O_MAX_SECTORS_LIMITED);
		}
#ifdef CONFIG_I2O_EXT_ADAPTEC_DMA64
		if (sizeof(dma_addr_t) > 4) {
			if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)))
				printk(KERN_INFO "%s: 64-bit DMA unavailable\n",
				       c->name);
			else {
				c->pae_support = 1;
				printk(KERN_INFO "%s: using 64-bit DMA\n",
				       c->name);
			}
		}
#endif
	}

	if ((rc = i2o_pci_alloc(c))) {
		printk(KERN_ERR "%s: DMA / IO allocation for I2O controller "
		       "failed\n", c->name);
		goto free_controller;
	}

	if (i2o_pci_irq_enable(c)) {
		printk(KERN_ERR "%s: unable to enable interrupts for I2O "
		       "controller\n", c->name);
		goto free_pci;
	}

	if ((rc = i2o_iop_add(c)))
		goto uninstall;

	if (i960)
		pci_write_config_word(i960, 0x42, 0x03ff);

	return 0;

      uninstall:
	i2o_pci_irq_disable(c);

      free_pci:
	i2o_pci_free(c);

      free_controller:
	i2o_iop_free(c);

      disable:
	pci_disable_device(pdev);

	return rc;
}

static void __devexit i2o_pci_remove(struct pci_dev *pdev)
{
	struct i2o_controller *c;
	c = pci_get_drvdata(pdev);

	i2o_iop_remove(c);
	i2o_pci_irq_disable(c);
	i2o_pci_free(c);

	pci_disable_device(pdev);

	printk(KERN_INFO "%s: Controller removed.\n", c->name);

	put_device(&c->device);
};

static struct pci_driver i2o_pci_driver = {
	.name = "PCI_I2O",
	.id_table = i2o_pci_ids,
	.probe = i2o_pci_probe,
	.remove = __devexit_p(i2o_pci_remove),
};

int __init i2o_pci_init(void)
{
	return pci_register_driver(&i2o_pci_driver);
};

void __exit i2o_pci_exit(void)
{
	pci_unregister_driver(&i2o_pci_driver);
};

MODULE_DEVICE_TABLE(pci, i2o_pci_ids);
