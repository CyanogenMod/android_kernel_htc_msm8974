/*
 * Copyright (C) 2000			Andre Hedrick <andre@linux-ide.org>
 * Copyright (C) 2000			Mark Lord <mlord@pobox.com>
 * Copyright (C) 2007			Bartlomiej Zolnierkiewicz
 *
 * May be copied or modified under the terms of the GNU General Public License
 *
 * Development of this chipset driver was funded
 * by the nice folks at National Semiconductor.
 *
 * Documentation:
 *	CS5530 documentation available from National Semiconductor.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ide.h>

#include <asm/io.h>

#define DRV_NAME "cs5530"

static unsigned int cs5530_pio_timings[2][5] = {
	{0x00009172, 0x00012171, 0x00020080, 0x00032010, 0x00040010},
	{0xd1329172, 0x71212171, 0x30200080, 0x20102010, 0x00100010}
};

#define CS5530_BAD_PIO(timings) (((timings)&~0x80000000)==0x0000e132)
#define CS5530_BASEREG(hwif)	(((hwif)->dma_base & ~0xf) + ((hwif)->channel ? 0x30 : 0x20))


static void cs5530_set_pio_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
	unsigned long basereg = CS5530_BASEREG(hwif);
	unsigned int format = (inl(basereg + 4) >> 31) & 1;
	const u8 pio = drive->pio_mode - XFER_PIO_0;

	outl(cs5530_pio_timings[format][pio], basereg + ((drive->dn & 1)<<3));
}


static u8 cs5530_udma_filter(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	ide_drive_t *mate = ide_get_pair_dev(drive);
	u16 *mateid;
	u8 mask = hwif->ultra_mask;

	if (mate == NULL)
		goto out;
	mateid = mate->id;

	if (ata_id_has_dma(mateid) && __ide_dma_bad_drive(mate) == 0) {
		if ((mateid[ATA_ID_FIELD_VALID] & 4) &&
		    (mateid[ATA_ID_UDMA_MODES] & 7))
			goto out;
		if (mateid[ATA_ID_MWDMA_MODES] & 7)
			mask = 0;
	}
out:
	return mask;
}

static void cs5530_set_dma_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
	unsigned long basereg;
	unsigned int reg, timings = 0;

	switch (drive->dma_mode) {
		case XFER_UDMA_0:	timings = 0x00921250; break;
		case XFER_UDMA_1:	timings = 0x00911140; break;
		case XFER_UDMA_2:	timings = 0x00911030; break;
		case XFER_MW_DMA_0:	timings = 0x00077771; break;
		case XFER_MW_DMA_1:	timings = 0x00012121; break;
		case XFER_MW_DMA_2:	timings = 0x00002020; break;
	}
	basereg = CS5530_BASEREG(hwif);
	reg = inl(basereg + 4);			
	timings |= reg & 0x80000000;		
	if ((drive-> dn & 1) == 0) {		
		outl(timings, basereg + 4);	
	} else {
		if (timings & 0x00100000)
			reg |=  0x00100000;	
		else
			reg &= ~0x00100000;	
		outl(reg, basereg + 4);		
		outl(timings, basereg + 12);	
	}
}


static int init_chipset_cs5530(struct pci_dev *dev)
{
	struct pci_dev *master_0 = NULL, *cs5530_0 = NULL;

	if (pci_resource_start(dev, 4) == 0)
		return -EFAULT;

	dev = NULL;
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
		goto out;
	}
	if (!cs5530_0) {
		printk(KERN_ERR DRV_NAME ": unable to locate CS5530 LEGACY function\n");
		goto out;
	}


	pci_set_master(cs5530_0);
	pci_try_set_mwi(cs5530_0);


	pci_write_config_byte(cs5530_0, PCI_CACHE_LINE_SIZE, 0x04);


	pci_write_config_word(cs5530_0, 0xd0, 0x5006);


	pci_write_config_byte(master_0, 0x40, 0x1e);


	pci_write_config_byte(master_0, 0x41, 0x14);


	pci_write_config_byte(master_0, 0x42, 0x00);
	pci_write_config_byte(master_0, 0x43, 0xc1);

out:
	pci_dev_put(master_0);
	pci_dev_put(cs5530_0);
	return 0;
}


static void __devinit init_hwif_cs5530 (ide_hwif_t *hwif)
{
	unsigned long basereg;
	u32 d0_timings;

	basereg = CS5530_BASEREG(hwif);
	d0_timings = inl(basereg + 0);
	if (CS5530_BAD_PIO(d0_timings))
		outl(cs5530_pio_timings[(d0_timings >> 31) & 1][0], basereg + 0);
	if (CS5530_BAD_PIO(inl(basereg + 8)))
		outl(cs5530_pio_timings[(d0_timings >> 31) & 1][0], basereg + 8);
}

static const struct ide_port_ops cs5530_port_ops = {
	.set_pio_mode		= cs5530_set_pio_mode,
	.set_dma_mode		= cs5530_set_dma_mode,
	.udma_filter		= cs5530_udma_filter,
};

static const struct ide_port_info cs5530_chipset __devinitdata = {
	.name		= DRV_NAME,
	.init_chipset	= init_chipset_cs5530,
	.init_hwif	= init_hwif_cs5530,
	.port_ops	= &cs5530_port_ops,
	.host_flags	= IDE_HFLAG_SERIALIZE |
			  IDE_HFLAG_POST_SET_MODE,
	.pio_mask	= ATA_PIO4,
	.mwdma_mask	= ATA_MWDMA2,
	.udma_mask	= ATA_UDMA2,
};

static int __devinit cs5530_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	return ide_pci_init_one(dev, &cs5530_chipset, NULL);
}

static const struct pci_device_id cs5530_pci_tbl[] = {
	{ PCI_VDEVICE(CYRIX, PCI_DEVICE_ID_CYRIX_5530_IDE), 0 },
	{ 0, },
};
MODULE_DEVICE_TABLE(pci, cs5530_pci_tbl);

static struct pci_driver cs5530_pci_driver = {
	.name		= "CS5530 IDE",
	.id_table	= cs5530_pci_tbl,
	.probe		= cs5530_init_one,
	.remove		= ide_pci_remove,
	.suspend	= ide_pci_suspend,
	.resume		= ide_pci_resume,
};

static int __init cs5530_ide_init(void)
{
	return ide_pci_register_driver(&cs5530_pci_driver);
}

static void __exit cs5530_ide_exit(void)
{
	pci_unregister_driver(&cs5530_pci_driver);
}

module_init(cs5530_ide_init);
module_exit(cs5530_ide_exit);

MODULE_AUTHOR("Mark Lord");
MODULE_DESCRIPTION("PCI driver module for Cyrix/NS 5530 IDE");
MODULE_LICENSE("GPL");
