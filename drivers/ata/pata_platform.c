/*
 * Generic platform device PATA driver
 *
 * Copyright (C) 2006 - 2007  Paul Mundt
 *
 * Based on pata_pcmcia:
 *
 *   Copyright 2005-2006 Red Hat Inc, all rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <scsi/scsi_host.h>
#include <linux/ata.h>
#include <linux/libata.h>
#include <linux/platform_device.h>
#include <linux/ata_platform.h>

#define DRV_NAME "pata_platform"
#define DRV_VERSION "1.2"

static int pio_mask = 1;

static int pata_platform_set_mode(struct ata_link *link, struct ata_device **unused)
{
	struct ata_device *dev;

	ata_for_each_dev(dev, link, ENABLED) {
		
		dev->pio_mode = dev->xfer_mode = XFER_PIO_0;
		dev->xfer_shift = ATA_SHIFT_PIO;
		dev->flags |= ATA_DFLAG_PIO;
		ata_dev_info(dev, "configured for PIO\n");
	}
	return 0;
}

static struct scsi_host_template pata_platform_sht = {
	ATA_PIO_SHT(DRV_NAME),
};

static struct ata_port_operations pata_platform_port_ops = {
	.inherits		= &ata_sff_port_ops,
	.sff_data_xfer		= ata_sff_data_xfer_noirq,
	.cable_detect		= ata_cable_unknown,
	.set_mode		= pata_platform_set_mode,
};

static void pata_platform_setup_port(struct ata_ioports *ioaddr,
				     unsigned int shift)
{
	
	ioaddr->data_addr	= ioaddr->cmd_addr + (ATA_REG_DATA    << shift);
	ioaddr->error_addr	= ioaddr->cmd_addr + (ATA_REG_ERR     << shift);
	ioaddr->feature_addr	= ioaddr->cmd_addr + (ATA_REG_FEATURE << shift);
	ioaddr->nsect_addr	= ioaddr->cmd_addr + (ATA_REG_NSECT   << shift);
	ioaddr->lbal_addr	= ioaddr->cmd_addr + (ATA_REG_LBAL    << shift);
	ioaddr->lbam_addr	= ioaddr->cmd_addr + (ATA_REG_LBAM    << shift);
	ioaddr->lbah_addr	= ioaddr->cmd_addr + (ATA_REG_LBAH    << shift);
	ioaddr->device_addr	= ioaddr->cmd_addr + (ATA_REG_DEVICE  << shift);
	ioaddr->status_addr	= ioaddr->cmd_addr + (ATA_REG_STATUS  << shift);
	ioaddr->command_addr	= ioaddr->cmd_addr + (ATA_REG_CMD     << shift);
}

int __devinit __pata_platform_probe(struct device *dev,
				    struct resource *io_res,
				    struct resource *ctl_res,
				    struct resource *irq_res,
				    unsigned int ioport_shift,
				    int __pio_mask)
{
	struct ata_host *host;
	struct ata_port *ap;
	unsigned int mmio;
	int irq = 0;
	int irq_flags = 0;

	mmio = (( io_res->flags == IORESOURCE_MEM) &&
		(ctl_res->flags == IORESOURCE_MEM));

	if (irq_res && irq_res->start > 0) {
		irq = irq_res->start;
		irq_flags = irq_res->flags;
	}

	host = ata_host_alloc(dev, 1);
	if (!host)
		return -ENOMEM;
	ap = host->ports[0];

	ap->ops = &pata_platform_port_ops;
	ap->pio_mask = __pio_mask;
	ap->flags |= ATA_FLAG_SLAVE_POSS;

	if (!irq) {
		ap->flags |= ATA_FLAG_PIO_POLLING;
		ata_port_desc(ap, "no IRQ, using PIO polling");
	}

	if (mmio) {
		ap->ioaddr.cmd_addr = devm_ioremap(dev, io_res->start,
				resource_size(io_res));
		ap->ioaddr.ctl_addr = devm_ioremap(dev, ctl_res->start,
				resource_size(ctl_res));
	} else {
		ap->ioaddr.cmd_addr = devm_ioport_map(dev, io_res->start,
				resource_size(io_res));
		ap->ioaddr.ctl_addr = devm_ioport_map(dev, ctl_res->start,
				resource_size(ctl_res));
	}
	if (!ap->ioaddr.cmd_addr || !ap->ioaddr.ctl_addr) {
		dev_err(dev, "failed to map IO/CTL base\n");
		return -ENOMEM;
	}

	ap->ioaddr.altstatus_addr = ap->ioaddr.ctl_addr;

	pata_platform_setup_port(&ap->ioaddr, ioport_shift);

	ata_port_desc(ap, "%s cmd 0x%llx ctl 0x%llx", mmio ? "mmio" : "ioport",
		      (unsigned long long)io_res->start,
		      (unsigned long long)ctl_res->start);

	
	return ata_host_activate(host, irq, irq ? ata_sff_interrupt : NULL,
				 irq_flags, &pata_platform_sht);
}
EXPORT_SYMBOL_GPL(__pata_platform_probe);

int __pata_platform_remove(struct device *dev)
{
	struct ata_host *host = dev_get_drvdata(dev);

	ata_host_detach(host);

	return 0;
}
EXPORT_SYMBOL_GPL(__pata_platform_remove);

static int __devinit pata_platform_probe(struct platform_device *pdev)
{
	struct resource *io_res;
	struct resource *ctl_res;
	struct resource *irq_res;
	struct pata_platform_info *pp_info = pdev->dev.platform_data;

	if ((pdev->num_resources != 3) && (pdev->num_resources != 2)) {
		dev_err(&pdev->dev, "invalid number of resources\n");
		return -EINVAL;
	}

	io_res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (io_res == NULL) {
		io_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (unlikely(io_res == NULL))
			return -EINVAL;
	}

	ctl_res = platform_get_resource(pdev, IORESOURCE_IO, 1);
	if (ctl_res == NULL) {
		ctl_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (unlikely(ctl_res == NULL))
			return -EINVAL;
	}

	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq_res)
		irq_res->flags = pp_info ? pp_info->irq_flags : 0;

	return __pata_platform_probe(&pdev->dev, io_res, ctl_res, irq_res,
				     pp_info ? pp_info->ioport_shift : 0,
				     pio_mask);
}

static int __devexit pata_platform_remove(struct platform_device *pdev)
{
	return __pata_platform_remove(&pdev->dev);
}

static struct platform_driver pata_platform_driver = {
	.probe		= pata_platform_probe,
	.remove		= __devexit_p(pata_platform_remove),
	.driver = {
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
	},
};

module_platform_driver(pata_platform_driver);

module_param(pio_mask, int, 0);

MODULE_AUTHOR("Paul Mundt");
MODULE_DESCRIPTION("low-level driver for platform device ATA");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:" DRV_NAME);
