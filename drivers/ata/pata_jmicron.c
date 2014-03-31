
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

#define DRV_NAME	"pata_jmicron"
#define DRV_VERSION	"0.1.5"

typedef enum {
	PORT_PATA0 = 0,
	PORT_PATA1 = 1,
	PORT_SATA = 2,
} port_type;

static int jmicron_pre_reset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u32 control;
	u32 control5;
	int port_mask = 1<< (4 * ap->port_no);
	int port = ap->port_no;
	port_type port_map[2];

	
	pci_read_config_dword(pdev, 0x40, &control);
	if ((control & port_mask) == 0)
		return -ENOENT;

	if (control & (1 << 23)) {
		port_map[0] = PORT_SATA;
		port_map[1] = PORT_PATA0;
	} else {
		port_map[0] = PORT_SATA;
		port_map[1] = PORT_SATA;
	}

	pci_read_config_dword(pdev, 0x80, &control5);
	if (control5 & (1<<24))
		port_map[0] = PORT_PATA1;

	
	if (control & (1 << 22))
		port = port ^ 1;

	switch (port_map[port]) {
	case PORT_PATA0:
		if ((control & (1 << 5)) == 0)
			return -ENOENT;
		if (control & (1 << 3))	
			ap->cbl = ATA_CBL_PATA40;
		else
			ap->cbl = ATA_CBL_PATA80;
		break;
	case PORT_PATA1:
		
		if ((control5 & (1 << 21)) == 0)
			return -ENOENT;
		if (control5 & (1 << 19))	
			ap->cbl = ATA_CBL_PATA40;
		else
			ap->cbl = ATA_CBL_PATA80;
		break;
	case PORT_SATA:
		ap->cbl = ATA_CBL_SATA;
		break;
	}
	return ata_sff_prereset(link, deadline);
}


static struct scsi_host_template jmicron_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations jmicron_ops = {
	.inherits		= &ata_bmdma_port_ops,
	.prereset		= jmicron_pre_reset,
};



static int jmicron_init_one (struct pci_dev *pdev, const struct pci_device_id *id)
{
	static const struct ata_port_info info = {
		.flags	= ATA_FLAG_SLAVE_POSS,

		.pio_mask	= ATA_PIO4,
		.mwdma_mask	= ATA_MWDMA2,
		.udma_mask 	= ATA_UDMA5,

		.port_ops	= &jmicron_ops,
	};
	const struct ata_port_info *ppi[] = { &info, NULL };

	return ata_pci_bmdma_init_one(pdev, ppi, &jmicron_sht, NULL, 0);
}

static const struct pci_device_id jmicron_pci_tbl[] = {
	{ PCI_VENDOR_ID_JMICRON, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	  PCI_CLASS_STORAGE_IDE << 8, 0xffff00, 0 },
	{ }	
};

static struct pci_driver jmicron_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= jmicron_pci_tbl,
	.probe			= jmicron_init_one,
	.remove			= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend		= ata_pci_device_suspend,
	.resume			= ata_pci_device_resume,
#endif
};

static int __init jmicron_init(void)
{
	return pci_register_driver(&jmicron_pci_driver);
}

static void __exit jmicron_exit(void)
{
	pci_unregister_driver(&jmicron_pci_driver);
}

module_init(jmicron_init);
module_exit(jmicron_exit);

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("SCSI low-level driver for Jmicron PATA ports");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, jmicron_pci_tbl);
MODULE_VERSION(DRV_VERSION);

