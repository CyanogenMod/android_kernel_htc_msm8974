
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

#define DRV_NAME	"pata_marvell"
#define DRV_VERSION	"0.1.6"


static int marvell_pata_active(struct pci_dev *pdev)
{
	int i;
	u32 devices;
	void __iomem *barp;

	
	if (pdev->device != 0x6145)
		return 1;

	barp = pci_iomap(pdev, 5, 0x10);
	if (barp == NULL)
		return -ENOMEM;

	printk("BAR5:");
	for(i = 0; i <= 0x0F; i++)
		printk("%02X:%02X ", i, ioread8(barp + i));
	printk("\n");

	devices = ioread32(barp + 0x0C);
	pci_iounmap(pdev, barp);

	if (devices & 0x10)
		return 1;
	return 0;
}


static int marvell_pre_reset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);

	if (pdev->device == 0x6145 && ap->port_no == 0 &&
		!marvell_pata_active(pdev))	
			return -ENOENT;

	return ata_sff_prereset(link, deadline);
}

static int marvell_cable_detect(struct ata_port *ap)
{
	
	switch(ap->port_no)
	{
	case 0:
		if (ioread8(ap->ioaddr.bmdma_addr + 1) & 1)
			return ATA_CBL_PATA40;
		return ATA_CBL_PATA80;
	case 1: 
		return ATA_CBL_SATA;
	}

	BUG();
	return 0;	
}


static struct scsi_host_template marvell_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations marvell_ops = {
	.inherits		= &ata_bmdma_port_ops,
	.cable_detect		= marvell_cable_detect,
	.prereset		= marvell_pre_reset,
};



static int marvell_init_one (struct pci_dev *pdev, const struct pci_device_id *id)
{
	static const struct ata_port_info info = {
		.flags		= ATA_FLAG_SLAVE_POSS,

		.pio_mask	= ATA_PIO4,
		.mwdma_mask	= ATA_MWDMA2,
		.udma_mask 	= ATA_UDMA5,

		.port_ops	= &marvell_ops,
	};
	static const struct ata_port_info info_sata = {
		
		.flags		= ATA_FLAG_SLAVE_POSS,

		.pio_mask	= ATA_PIO4,
		.mwdma_mask	= ATA_MWDMA2,
		.udma_mask 	= ATA_UDMA6,

		.port_ops	= &marvell_ops,
	};
	const struct ata_port_info *ppi[] = { &info, &info_sata };

	if (pdev->device == 0x6101)
		ppi[1] = &ata_dummy_port_info;

#if defined(CONFIG_SATA_AHCI) || defined(CONFIG_SATA_AHCI_MODULE)
	if (!marvell_pata_active(pdev)) {
		printk(KERN_INFO DRV_NAME ": PATA port not active, deferring to AHCI driver.\n");
		return -ENODEV;
	}
#endif
	return ata_pci_bmdma_init_one(pdev, ppi, &marvell_sht, NULL, 0);
}

static const struct pci_device_id marvell_pci_tbl[] = {
	{ PCI_DEVICE(0x11AB, 0x6101), },
	{ PCI_DEVICE(0x11AB, 0x6121), },
	{ PCI_DEVICE(0x11AB, 0x6123), },
	{ PCI_DEVICE(0x11AB, 0x6145), },
	{ PCI_DEVICE(0x1B4B, 0x91A0), },
	{ PCI_DEVICE(0x1B4B, 0x91A4), },

	{ }	
};

static struct pci_driver marvell_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= marvell_pci_tbl,
	.probe			= marvell_init_one,
	.remove			= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend		= ata_pci_device_suspend,
	.resume			= ata_pci_device_resume,
#endif
};

static int __init marvell_init(void)
{
	return pci_register_driver(&marvell_pci_driver);
}

static void __exit marvell_exit(void)
{
	pci_unregister_driver(&marvell_pci_driver);
}

module_init(marvell_init);
module_exit(marvell_exit);

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("SCSI low-level driver for Marvell ATA in legacy mode");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, marvell_pci_tbl);
MODULE_VERSION(DRV_VERSION);

