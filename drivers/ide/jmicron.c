
/*
 * Copyright (C) 2006		Red Hat
 *
 *  May be copied or modified under the terms of the GNU General Public License
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/ide.h>
#include <linux/init.h>

#define DRV_NAME "jmicron"

typedef enum {
	PORT_PATA0 = 0,
	PORT_PATA1 = 1,
	PORT_SATA = 2,
} port_type;


static u8 jmicron_cable_detect(ide_hwif_t *hwif)
{
	struct pci_dev *pdev = to_pci_dev(hwif->dev);

	u32 control;
	u32 control5;

	int port = hwif->channel;
	port_type port_map[2];

	pci_read_config_dword(pdev, 0x40, &control);

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
		if (control & (1 << 3))	
			return ATA_CBL_PATA40;
		return ATA_CBL_PATA80;
	case PORT_PATA1:
		if (control5 & (1 << 19))	
			return ATA_CBL_PATA40;
		return ATA_CBL_PATA80;
	case PORT_SATA:
		break;
	}
	
	return ATA_CBL_PATA80;
}

static void jmicron_set_pio_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
}


static void jmicron_set_dma_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
}

static const struct ide_port_ops jmicron_port_ops = {
	.set_pio_mode		= jmicron_set_pio_mode,
	.set_dma_mode		= jmicron_set_dma_mode,
	.cable_detect		= jmicron_cable_detect,
};

static const struct ide_port_info jmicron_chipset __devinitdata = {
	.name		= DRV_NAME,
	.enablebits	= { { 0x40, 0x01, 0x01 }, { 0x40, 0x10, 0x10 } },
	.port_ops	= &jmicron_port_ops,
	.pio_mask	= ATA_PIO5,
	.mwdma_mask	= ATA_MWDMA2,
	.udma_mask	= ATA_UDMA6,
};


static int __devinit jmicron_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	return ide_pci_init_one(dev, &jmicron_chipset, NULL);
}

static struct pci_device_id jmicron_pci_tbl[] = {
#if !defined(CONFIG_ATA) && !defined(CONFIG_ATA_MODULE)
	{ PCI_VDEVICE(JMICRON, PCI_DEVICE_ID_JMICRON_JMB361) },
	{ PCI_VDEVICE(JMICRON, PCI_DEVICE_ID_JMICRON_JMB363) },
	{ PCI_VDEVICE(JMICRON, PCI_DEVICE_ID_JMICRON_JMB365) },
	{ PCI_VDEVICE(JMICRON, PCI_DEVICE_ID_JMICRON_JMB366) },
	{ PCI_VDEVICE(JMICRON, PCI_DEVICE_ID_JMICRON_JMB368) },
#endif
	{ PCI_VENDOR_ID_JMICRON, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	  PCI_CLASS_STORAGE_IDE << 8, 0xffff00, 0 },
	{ 0, },
};

MODULE_DEVICE_TABLE(pci, jmicron_pci_tbl);

static struct pci_driver jmicron_pci_driver = {
	.name		= "JMicron IDE",
	.id_table	= jmicron_pci_tbl,
	.probe		= jmicron_init_one,
	.remove		= ide_pci_remove,
	.suspend	= ide_pci_suspend,
	.resume		= ide_pci_resume,
};

static int __init jmicron_ide_init(void)
{
	return ide_pci_register_driver(&jmicron_pci_driver);
}

static void __exit jmicron_ide_exit(void)
{
	pci_unregister_driver(&jmicron_pci_driver);
}

module_init(jmicron_ide_init);
module_exit(jmicron_ide_exit);

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("PCI driver module for the JMicron in legacy modes");
MODULE_LICENSE("GPL");
