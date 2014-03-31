/*
 * Libata driver for the highpoint 366 and 368 UDMA66 ATA controllers.
 *
 * This driver is heavily based upon:
 *
 * linux/drivers/ide/pci/hpt366.c		Version 0.36	April 25, 2003
 *
 * Copyright (C) 1999-2003		Andre Hedrick <andre@linux-ide.org>
 * Portions Copyright (C) 2001	        Sun Microsystems, Inc.
 * Portions Copyright (C) 2003		Red Hat Inc
 *
 *
 * TODO
 *	Look into engine reset on timeout errors. Should not be required.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>

#define DRV_NAME	"pata_hpt366"
#define DRV_VERSION	"0.6.11"

struct hpt_clock {
	u8	xfer_mode;
	u32	timing;
};


static const struct hpt_clock hpt366_40[] = {
	{	XFER_UDMA_4,	0x900fd943	},
	{	XFER_UDMA_3,	0x900ad943	},
	{	XFER_UDMA_2,	0x900bd943	},
	{	XFER_UDMA_1,	0x9008d943	},
	{	XFER_UDMA_0,	0x9008d943	},

	{	XFER_MW_DMA_2,	0xa008d943	},
	{	XFER_MW_DMA_1,	0xa010d955	},
	{	XFER_MW_DMA_0,	0xa010d9fc	},

	{	XFER_PIO_4,	0xc008d963	},
	{	XFER_PIO_3,	0xc010d974	},
	{	XFER_PIO_2,	0xc010d997	},
	{	XFER_PIO_1,	0xc010d9c7	},
	{	XFER_PIO_0,	0xc018d9d9	},
	{	0,		0x0120d9d9	}
};

static const struct hpt_clock hpt366_33[] = {
	{	XFER_UDMA_4,	0x90c9a731	},
	{	XFER_UDMA_3,	0x90cfa731	},
	{	XFER_UDMA_2,	0x90caa731	},
	{	XFER_UDMA_1,	0x90cba731	},
	{	XFER_UDMA_0,	0x90c8a731	},

	{	XFER_MW_DMA_2,	0xa0c8a731	},
	{	XFER_MW_DMA_1,	0xa0c8a732	},	
	{	XFER_MW_DMA_0,	0xa0c8a797	},

	{	XFER_PIO_4,	0xc0c8a731	},
	{	XFER_PIO_3,	0xc0c8a742	},
	{	XFER_PIO_2,	0xc0d0a753	},
	{	XFER_PIO_1,	0xc0d0a7a3	},	
	{	XFER_PIO_0,	0xc0d0a7aa	},	
	{	0,		0x0120a7a7	}
};

static const struct hpt_clock hpt366_25[] = {
	{	XFER_UDMA_4,	0x90c98521	},
	{	XFER_UDMA_3,	0x90cf8521	},
	{	XFER_UDMA_2,	0x90cf8521	},
	{	XFER_UDMA_1,	0x90cb8521	},
	{	XFER_UDMA_0,	0x90cb8521	},

	{	XFER_MW_DMA_2,	0xa0ca8521	},
	{	XFER_MW_DMA_1,	0xa0ca8532	},
	{	XFER_MW_DMA_0,	0xa0ca8575	},

	{	XFER_PIO_4,	0xc0ca8521	},
	{	XFER_PIO_3,	0xc0ca8532	},
	{	XFER_PIO_2,	0xc0ca8542	},
	{	XFER_PIO_1,	0xc0d08572	},
	{	XFER_PIO_0,	0xc0d08585	},
	{	0,		0x01208585	}
};


static u32 hpt36x_find_mode(struct ata_port *ap, int speed)
{
	struct hpt_clock *clocks = ap->host->private_data;

	while (clocks->xfer_mode) {
		if (clocks->xfer_mode == speed)
			return clocks->timing;
		clocks++;
	}
	BUG();
	return 0xffffffffU;	
}

static const char * const bad_ata33[] = {
	"Maxtor 92720U8", "Maxtor 92040U6", "Maxtor 91360U4", "Maxtor 91020U3",
	"Maxtor 90845U3", "Maxtor 90650U2",
	"Maxtor 91360D8", "Maxtor 91190D7", "Maxtor 91020D6", "Maxtor 90845D5",
	"Maxtor 90680D4", "Maxtor 90510D3", "Maxtor 90340D2",
	"Maxtor 91152D8", "Maxtor 91008D7", "Maxtor 90845D6", "Maxtor 90840D6",
	"Maxtor 90720D5", "Maxtor 90648D5", "Maxtor 90576D4",
	"Maxtor 90510D4",
	"Maxtor 90432D3", "Maxtor 90288D2", "Maxtor 90256D2",
	"Maxtor 91000D8", "Maxtor 90910D8", "Maxtor 90875D7", "Maxtor 90840D7",
	"Maxtor 90750D6", "Maxtor 90625D5", "Maxtor 90500D4",
	"Maxtor 91728D8", "Maxtor 91512D7", "Maxtor 91303D6", "Maxtor 91080D5",
	"Maxtor 90845D4", "Maxtor 90680D4", "Maxtor 90648D3", "Maxtor 90432D2",
	NULL
};

static const char * const bad_ata66_4[] = {
	"IBM-DTLA-307075",
	"IBM-DTLA-307060",
	"IBM-DTLA-307045",
	"IBM-DTLA-307030",
	"IBM-DTLA-307020",
	"IBM-DTLA-307015",
	"IBM-DTLA-305040",
	"IBM-DTLA-305030",
	"IBM-DTLA-305020",
	"IC35L010AVER07-0",
	"IC35L020AVER07-0",
	"IC35L030AVER07-0",
	"IC35L040AVER07-0",
	"IC35L060AVER07-0",
	"WDC AC310200R",
	NULL
};

static const char * const bad_ata66_3[] = {
	"WDC AC310200R",
	NULL
};

static int hpt_dma_blacklisted(const struct ata_device *dev, char *modestr,
			       const char * const list[])
{
	unsigned char model_num[ATA_ID_PROD_LEN + 1];
	int i = 0;

	ata_id_c_string(dev->id, model_num, ATA_ID_PROD, sizeof(model_num));

	while (list[i] != NULL) {
		if (!strcmp(list[i], model_num)) {
			pr_warn("%s is not supported for %s\n",
				modestr, list[i]);
			return 1;
		}
		i++;
	}
	return 0;
}


static unsigned long hpt366_filter(struct ata_device *adev, unsigned long mask)
{
	if (adev->class == ATA_DEV_ATA) {
		if (hpt_dma_blacklisted(adev, "UDMA",  bad_ata33))
			mask &= ~ATA_MASK_UDMA;
		if (hpt_dma_blacklisted(adev, "UDMA3", bad_ata66_3))
			mask &= ~(0xF8 << ATA_SHIFT_UDMA);
		if (hpt_dma_blacklisted(adev, "UDMA4", bad_ata66_4))
			mask &= ~(0xF0 << ATA_SHIFT_UDMA);
	} else if (adev->class == ATA_DEV_ATAPI)
		mask &= ~(ATA_MASK_MWDMA | ATA_MASK_UDMA);

	return mask;
}

static int hpt36x_cable_detect(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u8 ata66;

	pci_read_config_byte(pdev, 0x5A, &ata66);
	if (ata66 & 2)
		return ATA_CBL_PATA40;
	return ATA_CBL_PATA80;
}

static void hpt366_set_mode(struct ata_port *ap, struct ata_device *adev,
			    u8 mode)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u32 addr = 0x40 + 4 * adev->devno;
	u32 mask, reg, t;

	
	if (mode < XFER_MW_DMA_0)
		mask = 0xc1f8ffff;
	else if (mode < XFER_UDMA_0)
		mask = 0x303800ff;
	else
		mask = 0x30070000;

	t = hpt36x_find_mode(ap, mode);

	pci_read_config_dword(pdev, addr, &reg);
	reg = ((reg & ~mask) | (t & mask)) & ~0xc0000000;
	pci_write_config_dword(pdev, addr, reg);
}


static void hpt366_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	hpt366_set_mode(ap, adev, adev->pio_mode);
}


static void hpt366_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	hpt366_set_mode(ap, adev, adev->dma_mode);
}

static struct scsi_host_template hpt36x_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};


static struct ata_port_operations hpt366_port_ops = {
	.inherits	= &ata_bmdma_port_ops,
	.cable_detect	= hpt36x_cable_detect,
	.mode_filter	= hpt366_filter,
	.set_piomode	= hpt366_set_piomode,
	.set_dmamode	= hpt366_set_dmamode,
};


static void hpt36x_init_chipset(struct pci_dev *dev)
{
	u8 drive_fast;

	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, (L1_CACHE_BYTES / 4));
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x78);
	pci_write_config_byte(dev, PCI_MIN_GNT, 0x08);
	pci_write_config_byte(dev, PCI_MAX_LAT, 0x08);

	pci_read_config_byte(dev, 0x51, &drive_fast);
	if (drive_fast & 0x80)
		pci_write_config_byte(dev, 0x51, drive_fast & ~0x80);
}


static int hpt36x_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	static const struct ata_port_info info_hpt366 = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA4,
		.port_ops = &hpt366_port_ops
	};
	const struct ata_port_info *ppi[] = { &info_hpt366, NULL };

	void *hpriv = NULL;
	u32 reg1;
	int rc;

	rc = pcim_enable_device(dev);
	if (rc)
		return rc;

	
	
	if (dev->revision > 2)
		return -ENODEV;

	hpt36x_init_chipset(dev);

	pci_read_config_dword(dev, 0x40,  &reg1);

	
	
	switch ((reg1 & 0x700) >> 8) {
	case 9:
		hpriv = &hpt366_40;
		break;
	case 5:
		hpriv = &hpt366_25;
		break;
	default:
		hpriv = &hpt366_33;
		break;
	}
	
	return ata_pci_bmdma_init_one(dev, ppi, &hpt36x_sht, hpriv, 0);
}

#ifdef CONFIG_PM
static int hpt36x_reinit_one(struct pci_dev *dev)
{
	struct ata_host *host = dev_get_drvdata(&dev->dev);
	int rc;

	rc = ata_pci_device_do_resume(dev);
	if (rc)
		return rc;
	hpt36x_init_chipset(dev);
	ata_host_resume(host);
	return 0;
}
#endif

static const struct pci_device_id hpt36x[] = {
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT366), },
	{ },
};

static struct pci_driver hpt36x_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= hpt36x,
	.probe		= hpt36x_init_one,
	.remove		= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend	= ata_pci_device_suspend,
	.resume		= hpt36x_reinit_one,
#endif
};

static int __init hpt36x_init(void)
{
	return pci_register_driver(&hpt36x_pci_driver);
}

static void __exit hpt36x_exit(void)
{
	pci_unregister_driver(&hpt36x_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for the Highpoint HPT366/368");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, hpt36x);
MODULE_VERSION(DRV_VERSION);

module_init(hpt36x_init);
module_exit(hpt36x_exit);
