/*
 *    pata_artop.c - ARTOP ATA controller driver
 *
 *	(C) 2006 Red Hat
 *	(C) 2007,2011 Bartlomiej Zolnierkiewicz
 *
 *    Based in part on drivers/ide/pci/aec62xx.c
 *	Copyright (C) 1999-2002	Andre Hedrick <andre@linux-ide.org>
 *	865/865R fixes for Macintosh card version from a patch to the old
 *		driver by Thibaut VARENE <varenet@parisc-linux.org>
 *	When setting the PCI latency we must set 0x80 or higher for burst
 *		performance Alessandro Zummo <alessandro.zummo@towertech.it>
 *
 *	TODO
 *	Investigate no_dsc on 850R
 *	Clock detect
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>
#include <linux/ata.h>

#define DRV_NAME	"pata_artop"
#define DRV_VERSION	"0.4.6"


static int clock = 0;


static int artop62x0_pre_reset(struct ata_link *link, unsigned long deadline)
{
	static const struct pci_bits artop_enable_bits[] = {
		{ 0x4AU, 1U, 0x02UL, 0x02UL },	
		{ 0x4AU, 1U, 0x04UL, 0x04UL },	
	};

	struct ata_port *ap = link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);

	
	if ((pdev->device & 1) &&
	    !pci_test_config_bits(pdev, &artop_enable_bits[ap->port_no]))
		return -ENOENT;

	return ata_sff_prereset(link, deadline);
}


static int artop6260_cable_detect(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u8 tmp;
	pci_read_config_byte(pdev, 0x49, &tmp);
	if (tmp & (1 << ap->port_no))
		return ATA_CBL_PATA40;
	return ATA_CBL_PATA80;
}


static void artop6210_load_piomode(struct ata_port *ap, struct ata_device *adev, unsigned int pio)
{
	struct pci_dev *pdev	= to_pci_dev(ap->host->dev);
	int dn = adev->devno + 2 * ap->port_no;
	const u16 timing[2][5] = {
		{ 0x0000, 0x000A, 0x0008, 0x0303, 0x0301 },
		{ 0x0700, 0x070A, 0x0708, 0x0403, 0x0401 }

	};
	
	pci_write_config_word(pdev, 0x40 + 2 * dn, timing[clock][pio]);
}


static void artop6210_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev	= to_pci_dev(ap->host->dev);
	int dn = adev->devno + 2 * ap->port_no;
	u8 ultra;

	artop6210_load_piomode(ap, adev, adev->pio_mode - XFER_PIO_0);

	
	pci_read_config_byte(pdev, 0x54, &ultra);
	ultra &= ~(3 << (2 * dn));
	pci_write_config_byte(pdev, 0x54, ultra);
}


static void artop6260_load_piomode (struct ata_port *ap, struct ata_device *adev, unsigned int pio)
{
	struct pci_dev *pdev	= to_pci_dev(ap->host->dev);
	int dn = adev->devno + 2 * ap->port_no;
	const u8 timing[2][5] = {
		{ 0x00, 0x0A, 0x08, 0x33, 0x31 },
		{ 0x70, 0x7A, 0x78, 0x43, 0x41 }

	};
	
	pci_write_config_byte(pdev, 0x40 + dn, timing[clock][pio]);
}


static void artop6260_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev	= to_pci_dev(ap->host->dev);
	u8 ultra;

	artop6260_load_piomode(ap, adev, adev->pio_mode - XFER_PIO_0);

	
	pci_read_config_byte(pdev, 0x44 + ap->port_no, &ultra);
	ultra &= ~(7 << (4  * adev->devno));	
	pci_write_config_byte(pdev, 0x44 + ap->port_no, ultra);
}


static void artop6210_set_dmamode (struct ata_port *ap, struct ata_device *adev)
{
	unsigned int pio;
	struct pci_dev *pdev	= to_pci_dev(ap->host->dev);
	int dn = adev->devno + 2 * ap->port_no;
	u8 ultra;

	if (adev->dma_mode == XFER_MW_DMA_0)
		pio = 1;
	else
		pio = 4;

	
	artop6210_load_piomode(ap, adev, pio);

	pci_read_config_byte(pdev, 0x54, &ultra);
	ultra &= ~(3 << (2 * dn));

	
	if (adev->dma_mode >= XFER_UDMA_0) {
		u8 mode = (adev->dma_mode - XFER_UDMA_0) + 1 - clock;
		if (mode == 0)
			mode = 1;
		ultra |= (mode << (2 * dn));
	}
	pci_write_config_byte(pdev, 0x54, ultra);
}


static void artop6260_set_dmamode (struct ata_port *ap, struct ata_device *adev)
{
	unsigned int pio	= adev->pio_mode - XFER_PIO_0;
	struct pci_dev *pdev	= to_pci_dev(ap->host->dev);
	u8 ultra;

	if (adev->dma_mode == XFER_MW_DMA_0)
		pio = 1;
	else
		pio = 4;

	
	artop6260_load_piomode(ap, adev, pio);

	
	pci_read_config_byte(pdev, 0x44 + ap->port_no, &ultra);
	ultra &= ~(7 << (4  * adev->devno));	
	if (adev->dma_mode >= XFER_UDMA_0) {
		u8 mode = adev->dma_mode - XFER_UDMA_0 + 1 - clock;
		if (mode == 0)
			mode = 1;
		ultra |= (mode << (4 * adev->devno));
	}
	pci_write_config_byte(pdev, 0x44 + ap->port_no, ultra);
}


static int artop6210_qc_defer(struct ata_queued_cmd *qc)
{
	struct ata_host *host = qc->ap->host;
	struct ata_port *alt = host->ports[1 ^ qc->ap->port_no];
	int rc;

	
	rc = ata_std_qc_defer(qc);
	if (rc != 0)
		return rc;

	if (alt && alt->qc_active)
		return	ATA_DEFER_PORT;
	return 0;
}

static struct scsi_host_template artop_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations artop6210_ops = {
	.inherits		= &ata_bmdma_port_ops,
	.cable_detect		= ata_cable_40wire,
	.set_piomode		= artop6210_set_piomode,
	.set_dmamode		= artop6210_set_dmamode,
	.prereset		= artop62x0_pre_reset,
	.qc_defer		= artop6210_qc_defer,
};

static struct ata_port_operations artop6260_ops = {
	.inherits		= &ata_bmdma_port_ops,
	.cable_detect		= artop6260_cable_detect,
	.set_piomode		= artop6260_set_piomode,
	.set_dmamode		= artop6260_set_dmamode,
	.prereset		= artop62x0_pre_reset,
};

static void atp8xx_fixup(struct pci_dev *pdev)
{
	if (pdev->device == 0x0005)
		
		pci_write_config_byte(pdev, 0x54, 0);
	else if (pdev->device == 0x0008 || pdev->device == 0x0009) {
		u8 reg;


		
		pci_read_config_byte(pdev, 0x49, &reg);
		pci_write_config_byte(pdev, 0x49, reg & ~0x30);

		pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &reg);
		if (reg <= 0x80)
			pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 0x90);

		
		pci_read_config_byte(pdev, 0x4a, &reg);
		pci_write_config_byte(pdev, 0x4a, (reg & ~0x01) | 0x80);
	}
}


static int artop_init_one (struct pci_dev *pdev, const struct pci_device_id *id)
{
	static const struct ata_port_info info_6210 = {
		.flags		= ATA_FLAG_SLAVE_POSS,
		.pio_mask	= ATA_PIO4,
		.mwdma_mask	= ATA_MWDMA2,
		.udma_mask 	= ATA_UDMA2,
		.port_ops	= &artop6210_ops,
	};
	static const struct ata_port_info info_626x = {
		.flags		= ATA_FLAG_SLAVE_POSS,
		.pio_mask	= ATA_PIO4,
		.mwdma_mask	= ATA_MWDMA2,
		.udma_mask 	= ATA_UDMA4,
		.port_ops	= &artop6260_ops,
	};
	static const struct ata_port_info info_628x = {
		.flags		= ATA_FLAG_SLAVE_POSS,
		.pio_mask	= ATA_PIO4,
		.mwdma_mask	= ATA_MWDMA2,
		.udma_mask 	= ATA_UDMA5,
		.port_ops	= &artop6260_ops,
	};
	static const struct ata_port_info info_628x_fast = {
		.flags		= ATA_FLAG_SLAVE_POSS,
		.pio_mask	= ATA_PIO4,
		.mwdma_mask	= ATA_MWDMA2,
		.udma_mask 	= ATA_UDMA6,
		.port_ops	= &artop6260_ops,
	};
	const struct ata_port_info *ppi[] = { NULL, NULL };
	int rc;

	ata_print_version_once(&pdev->dev, DRV_VERSION);

	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	if (id->driver_data == 0)	
		ppi[0] = &info_6210;
	else if (id->driver_data == 1)	
		ppi[0] = &info_626x;
	else if (id->driver_data == 2)	{ 
		unsigned long io = pci_resource_start(pdev, 4);

		ppi[0] = &info_628x;
		if (inb(io) & 0x10)
			ppi[0] = &info_628x_fast;
	}

	BUG_ON(ppi[0] == NULL);

	atp8xx_fixup(pdev);

	return ata_pci_bmdma_init_one(pdev, ppi, &artop_sht, NULL, 0);
}

static const struct pci_device_id artop_pci_tbl[] = {
	{ PCI_VDEVICE(ARTOP, 0x0005), 0 },
	{ PCI_VDEVICE(ARTOP, 0x0006), 1 },
	{ PCI_VDEVICE(ARTOP, 0x0007), 1 },
	{ PCI_VDEVICE(ARTOP, 0x0008), 2 },
	{ PCI_VDEVICE(ARTOP, 0x0009), 2 },

	{ }	
};

#ifdef CONFIG_PM
static int atp8xx_reinit_one(struct pci_dev *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	int rc;

	rc = ata_pci_device_do_resume(pdev);
	if (rc)
		return rc;

	atp8xx_fixup(pdev);

	ata_host_resume(host);
	return 0;
}
#endif

static struct pci_driver artop_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= artop_pci_tbl,
	.probe			= artop_init_one,
	.remove			= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend		= ata_pci_device_suspend,
	.resume			= atp8xx_reinit_one,
#endif
};

static int __init artop_init(void)
{
	return pci_register_driver(&artop_pci_driver);
}

static void __exit artop_exit(void)
{
	pci_unregister_driver(&artop_pci_driver);
}

module_init(artop_init);
module_exit(artop_exit);

MODULE_AUTHOR("Alan Cox, Bartlomiej Zolnierkiewicz");
MODULE_DESCRIPTION("SCSI low-level driver for ARTOP PATA");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, artop_pci_tbl);
MODULE_VERSION(DRV_VERSION);
