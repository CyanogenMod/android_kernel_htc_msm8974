/*
 *  pata_sch.c - Intel SCH PATA controllers
 *
 *  Copyright (c) 2008 Alek Du <alek.du@intel.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License 2 as published
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
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
#include <linux/dmi.h>

#define DRV_NAME	"pata_sch"
#define DRV_VERSION	"0.2"

enum {
	D0TIM	= 0x80,		
	D1TIM	= 0x84,		
	PM	= 0x07,		
	MDM	= (0x03 << 8),	
	UDM	= (0x07 << 16), 
	PPE	= (1 << 30),	
	USD	= (1 << 31),	
};

static int sch_init_one(struct pci_dev *pdev,
			 const struct pci_device_id *ent);
static void sch_set_piomode(struct ata_port *ap, struct ata_device *adev);
static void sch_set_dmamode(struct ata_port *ap, struct ata_device *adev);

static const struct pci_device_id sch_pci_tbl[] = {
	
	{ PCI_VDEVICE(INTEL, PCI_DEVICE_ID_INTEL_SCH_IDE), 0 },
	{ }	
};

static struct pci_driver sch_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= sch_pci_tbl,
	.probe			= sch_init_one,
	.remove			= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend		= ata_pci_device_suspend,
	.resume			= ata_pci_device_resume,
#endif
};

static struct scsi_host_template sch_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations sch_pata_ops = {
	.inherits		= &ata_bmdma_port_ops,
	.cable_detect		= ata_cable_unknown,
	.set_piomode		= sch_set_piomode,
	.set_dmamode		= sch_set_dmamode,
};

static struct ata_port_info sch_port_info = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	.mwdma_mask	= ATA_MWDMA2,
	.udma_mask	= ATA_UDMA5,
	.port_ops	= &sch_pata_ops,
};

MODULE_AUTHOR("Alek Du <alek.du@intel.com>");
MODULE_DESCRIPTION("SCSI low-level driver for Intel SCH PATA controllers");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, sch_pci_tbl);
MODULE_VERSION(DRV_VERSION);


static void sch_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	unsigned int pio	= adev->pio_mode - XFER_PIO_0;
	struct pci_dev *dev	= to_pci_dev(ap->host->dev);
	unsigned int port	= adev->devno ? D1TIM : D0TIM;
	unsigned int data;

	pci_read_config_dword(dev, port, &data);
	
	
	data &= ~(PM | PPE);
	data |= pio;
	
	if (adev->class == ATA_DEV_ATA)
		data |= PPE;
	pci_write_config_dword(dev, port, data);
}


static void sch_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	unsigned int dma_mode	= adev->dma_mode;
	struct pci_dev *dev	= to_pci_dev(ap->host->dev);
	unsigned int port	= adev->devno ? D1TIM : D0TIM;
	unsigned int data;

	pci_read_config_dword(dev, port, &data);
	
	if (dma_mode >= XFER_UDMA_0) {
		
		data |= USD;
		data &= ~UDM;
		data |= (dma_mode - XFER_UDMA_0) << 16;
	} else { 
		data &= ~(USD | MDM);
		data |= (dma_mode - XFER_MW_DMA_0) << 8;
	}
	pci_write_config_dword(dev, port, data);
}


static int __devinit sch_init_one(struct pci_dev *pdev,
				   const struct pci_device_id *ent)
{
	const struct ata_port_info *ppi[] = { &sch_port_info, NULL };

	ata_print_version_once(&pdev->dev, DRV_VERSION);

	return ata_pci_bmdma_init_one(pdev, ppi, &sch_sht, NULL, 0);
}

static int __init sch_init(void)
{
	return pci_register_driver(&sch_pci_driver);
}

static void __exit sch_exit(void)
{
	pci_unregister_driver(&sch_pci_driver);
}

module_init(sch_init);
module_exit(sch_exit);
