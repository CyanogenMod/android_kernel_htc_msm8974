/*
 * pata-cs5530.c 	- CS5530 PATA for new ATA layer
 *			  (C) 2005 Red Hat Inc
 *
 * based upon cs5530.c by Mark Lord.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Loosely based on the piix & svwks drivers.
 *
 * Documentation:
 *	Available from AMD web site.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>
#include <linux/dmi.h>

#define DRV_NAME	"pata_cs5530"
#define DRV_VERSION	"0.7.4"

static void __iomem *cs5530_port_base(struct ata_port *ap)
{
	unsigned long bmdma = (unsigned long)ap->ioaddr.bmdma_addr;

	return (void __iomem *)((bmdma & ~0x0F) + 0x20 + 0x10 * ap->port_no);
}


static void cs5530_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	static const unsigned int cs5530_pio_timings[2][5] = {
		{0x00009172, 0x00012171, 0x00020080, 0x00032010, 0x00040010},
		{0xd1329172, 0x71212171, 0x30200080, 0x20102010, 0x00100010}
	};
	void __iomem *base = cs5530_port_base(ap);
	u32 tuning;
	int format;

	
	tuning = ioread32(base + 0x04);
	format = (tuning & 0x80000000UL) ? 1 : 0;

	
	if (adev->devno)
		base += 0x08;

	iowrite32(cs5530_pio_timings[format][adev->pio_mode - XFER_PIO_0], base);
}


static void cs5530_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	void __iomem *base = cs5530_port_base(ap);
	u32 tuning, timing = 0;
	u8 reg;

	
	tuning = ioread32(base + 0x04);

	switch(adev->dma_mode) {
		case XFER_UDMA_0:
			timing  = 0x00921250;break;
		case XFER_UDMA_1:
			timing  = 0x00911140;break;
		case XFER_UDMA_2:
			timing  = 0x00911030;break;
		case XFER_MW_DMA_0:
			timing  = 0x00077771;break;
		case XFER_MW_DMA_1:
			timing  = 0x00012121;break;
		case XFER_MW_DMA_2:
			timing  = 0x00002020;break;
		default:
			BUG();
	}
	
	timing |= (tuning & 0x80000000UL);
	if (adev->devno == 0) 
		iowrite32(timing, base + 0x04);
	else {
		if (timing & 0x00100000)
			tuning |= 0x00100000;	
		else
			tuning &= ~0x00100000;	
		iowrite32(tuning, base + 0x04);
		iowrite32(timing, base + 0x0C);
	}

	
	reg = ioread8(ap->ioaddr.bmdma_addr + ATA_DMA_STATUS);
	reg |= (1 << (5 + adev->devno));
	iowrite8(reg, ap->ioaddr.bmdma_addr + ATA_DMA_STATUS);

	

	ap->private_data = adev;
}


static unsigned int cs5530_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_device *adev = qc->dev;
	struct ata_device *prev = ap->private_data;

	
	if (ata_dma_enabled(adev) && adev != prev && prev != NULL) {
		
		if ((ata_using_udma(adev) && !ata_using_udma(prev)) ||
		    (ata_using_udma(prev) && !ata_using_udma(adev)))
		    	
		    	cs5530_set_dmamode(ap, adev);
	}

	return ata_bmdma_qc_issue(qc);
}

static struct scsi_host_template cs5530_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
	.sg_tablesize	= LIBATA_DUMB_MAX_PRD,
};

static struct ata_port_operations cs5530_port_ops = {
	.inherits	= &ata_bmdma_port_ops,

	.qc_prep 	= ata_bmdma_dumb_qc_prep,
	.qc_issue	= cs5530_qc_issue,

	.cable_detect	= ata_cable_40wire,
	.set_piomode	= cs5530_set_piomode,
	.set_dmamode	= cs5530_set_dmamode,
};

static const struct dmi_system_id palmax_dmi_table[] = {
	{
		.ident = "Palmax PD1100",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Cyrix"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Caddis"),
		},
	},
	{ }
};

static int cs5530_is_palmax(void)
{
	if (dmi_check_system(palmax_dmi_table)) {
		printk(KERN_INFO "Palmax PD1100: Disabling DMA on docking port.\n");
		return 1;
	}
	return 0;
}



static int cs5530_init_chip(void)
{
	struct pci_dev *master_0 = NULL, *cs5530_0 = NULL, *dev = NULL;

	while ((dev = pci_get_device(PCI_VENDOR_ID_CYRIX, PCI_ANY_ID, dev)) != NULL) {
		switch (dev->device) {
			case PCI_DEVICE_ID_CYRIX_PCI_MASTER:
				master_0 = pci_dev_get(dev);
				break;
			case PCI_DEVICE_ID_CYRIX_5530_LEGACY:
				cs5530_0 = pci_dev_get(dev);
				break;
		}
	}
	if (!master_0) {
		printk(KERN_ERR DRV_NAME ": unable to locate PCI MASTER function\n");
		goto fail_put;
	}
	if (!cs5530_0) {
		printk(KERN_ERR DRV_NAME ": unable to locate CS5530 LEGACY function\n");
		goto fail_put;
	}

	pci_set_master(cs5530_0);
	pci_try_set_mwi(cs5530_0);


	pci_write_config_byte(cs5530_0, PCI_CACHE_LINE_SIZE, 0x04);


	pci_write_config_word(cs5530_0, 0xd0, 0x5006);


	pci_write_config_byte(master_0, 0x40, 0x1e);


	pci_write_config_byte(master_0, 0x41, 0x14);


	pci_write_config_byte(master_0, 0x42, 0x00);
	pci_write_config_byte(master_0, 0x43, 0xc1);

	pci_dev_put(master_0);
	pci_dev_put(cs5530_0);
	return 0;
fail_put:
	if (master_0)
		pci_dev_put(master_0);
	if (cs5530_0)
		pci_dev_put(cs5530_0);
	return -ENODEV;
}


static int cs5530_init_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	static const struct ata_port_info info = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA2,
		.port_ops = &cs5530_port_ops
	};
	
	static const struct ata_port_info info_palmax_secondary = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.port_ops = &cs5530_port_ops
	};
	const struct ata_port_info *ppi[] = { &info, NULL };
	int rc;

	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	
	if (cs5530_init_chip())
		return -ENODEV;

	if (cs5530_is_palmax())
		ppi[1] = &info_palmax_secondary;

	
	return ata_pci_bmdma_init_one(pdev, ppi, &cs5530_sht, NULL, 0);
}

#ifdef CONFIG_PM
static int cs5530_reinit_one(struct pci_dev *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	int rc;

	rc = ata_pci_device_do_resume(pdev);
	if (rc)
		return rc;

	
	if (cs5530_init_chip())
		return -EIO;

	ata_host_resume(host);
	return 0;
}
#endif 

static const struct pci_device_id cs5530[] = {
	{ PCI_VDEVICE(CYRIX, PCI_DEVICE_ID_CYRIX_5530_IDE), },

	{ },
};

static struct pci_driver cs5530_pci_driver = {
	.name 		= DRV_NAME,
	.id_table	= cs5530,
	.probe 		= cs5530_init_one,
	.remove		= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend	= ata_pci_device_suspend,
	.resume		= cs5530_reinit_one,
#endif
};

static int __init cs5530_init(void)
{
	return pci_register_driver(&cs5530_pci_driver);
}

static void __exit cs5530_exit(void)
{
	pci_unregister_driver(&cs5530_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for the Cyrix/NS/AMD 5530");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, cs5530);
MODULE_VERSION(DRV_VERSION);

module_init(cs5530_init);
module_exit(cs5530_exit);
