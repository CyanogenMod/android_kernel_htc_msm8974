/*
 * Copyright (C) 2004		Red Hat
 * Copyright (C) 2007		Bartlomiej Zolnierkiewicz
 *
 *  May be copied or modified under the terms of the GNU General Public License
 *  Based in part on the ITE vendor provided SCSI driver.
 *
 *  Documentation:
 *	Datasheet is freely available, some other documents under NDA.
 *
 *  The ITE8212 isn't exactly a standard IDE controller. It has two
 *  modes. In pass through mode then it is an IDE controller. In its smart
 *  mode its actually quite a capable hardware raid controller disguised
 *  as an IDE controller. Smart mode only understands DMA read/write and
 *  identify, none of the fancier commands apply. The IT8211 is identical
 *  in other respects but lacks the raid mode.
 *
 *  Errata:
 *  o	Rev 0x10 also requires master/slave hold the same DMA timings and
 *	cannot do ATAPI MWDMA.
 *  o	The identify data for raid volumes lacks CHS info (technically ok)
 *	but also fails to set the LBA28 and other bits. We fix these in
 *	the IDE probe quirk code.
 *  o	If you write LBA48 sized I/O's (ie > 256 sector) in smart mode
 *	raid then the controller firmware dies
 *  o	Smart mode without RAID doesn't clear all the necessary identify
 *	bits to reduce the command set to the one used
 *
 *  This has a few impacts on the driver
 *  - In pass through mode we do all the work you would expect
 *  - In smart mode the clocking set up is done by the controller generally
 *    but we must watch the other limits and filter.
 *  - There are a few extra vendor commands that actually talk to the
 *    controller but only work PIO with no IRQ.
 *
 *  Vendor areas of the identify block in smart mode are used for the
 *  timing and policy set up. Each HDD in raid mode also has a serial
 *  block on the disk. The hardware extra commands are get/set chip status,
 *  rebuild, get rebuild status.
 *
 *  In Linux the driver supports pass through mode as if the device was
 *  just another IDE controller. If the smart mode is running then
 *  volumes are managed by the controller firmware and each IDE "disk"
 *  is a raid volume. Even more cute - the controller can do automated
 *  hotplug and rebuild.
 *
 *  The pass through controller itself is a little demented. It has a
 *  flaw that it has a single set of PIO/MWDMA timings per channel so
 *  non UDMA devices restrict each others performance. It also has a
 *  single clock source per channel so mixed UDMA100/133 performance
 *  isn't perfect and we have to pick a clock. Thankfully none of this
 *  matters in smart mode. ATAPI DMA is not currently supported.
 *
 *  It seems the smart mode is a win for RAID1/RAID10 but otherwise not.
 *
 *  TODO
 *	-	ATAPI UDMA is ok but not MWDMA it seems
 *	-	RAID configuration ioctls
 *	-	Move to libata once it grows up
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/ide.h>
#include <linux/init.h>

#define DRV_NAME "it821x"

#define QUIRK_VORTEX86 1

struct it821x_dev
{
	unsigned int smart:1,		
		timing10:1;		
	u8	clock_mode;		
	u8	want[2][2];		
	u16	pio[2];			
	u16	mwdma[2];		
	u16	udma[2];		
	u16	quirks;
};

#define ATA_66		0
#define ATA_50		1
#define ATA_ANY		2

#define UDMA_OFF	0
#define MWDMA_OFF	0


static int it8212_noraid;


static void it821x_program(ide_drive_t *drive, u16 timing)
{
	ide_hwif_t *hwif = drive->hwif;
	struct pci_dev *dev = to_pci_dev(hwif->dev);
	struct it821x_dev *itdev = ide_get_hwifdata(hwif);
	int channel = hwif->channel;
	u8 conf;

	
	if(itdev->clock_mode == ATA_66)
		conf = timing >> 8;
	else
		conf = timing & 0xFF;

	pci_write_config_byte(dev, 0x54 + 4 * channel, conf);
}


static void it821x_program_udma(ide_drive_t *drive, u16 timing)
{
	ide_hwif_t *hwif = drive->hwif;
	struct pci_dev *dev = to_pci_dev(hwif->dev);
	struct it821x_dev *itdev = ide_get_hwifdata(hwif);
	int channel = hwif->channel;
	u8 unit = drive->dn & 1, conf;

	
	if(itdev->clock_mode == ATA_66)
		conf = timing >> 8;
	else
		conf = timing & 0xFF;

	if (itdev->timing10 == 0)
		pci_write_config_byte(dev, 0x56 + 4 * channel + unit, conf);
	else {
		pci_write_config_byte(dev, 0x56 + 4 * channel, conf);
		pci_write_config_byte(dev, 0x56 + 4 * channel + 1, conf);
	}
}


static void it821x_clock_strategy(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	struct pci_dev *dev = to_pci_dev(hwif->dev);
	struct it821x_dev *itdev = ide_get_hwifdata(hwif);
	ide_drive_t *pair = ide_get_pair_dev(drive);
	int clock, altclock, sel = 0;
	u8 unit = drive->dn & 1, v;

	if(itdev->want[0][0] > itdev->want[1][0]) {
		clock = itdev->want[0][1];
		altclock = itdev->want[1][1];
	} else {
		clock = itdev->want[1][1];
		altclock = itdev->want[0][1];
	}

	if (clock == ATA_ANY)
		clock = altclock;

	
	if(clock == ATA_ANY)
		return;
	
	if(clock == itdev->clock_mode)
		return;

	
	if(clock == ATA_66)
		itdev->clock_mode = ATA_66;
	else {
		itdev->clock_mode = ATA_50;
		sel = 1;
	}

	pci_read_config_byte(dev, 0x50, &v);
	v &= ~(1 << (1 + hwif->channel));
	v |= sel << (1 + hwif->channel);
	pci_write_config_byte(dev, 0x50, v);

	if(pair && itdev->udma[1-unit] != UDMA_OFF) {
		it821x_program_udma(pair, itdev->udma[1-unit]);
		it821x_program(pair, itdev->pio[1-unit]);
	}
	if(itdev->udma[unit] != UDMA_OFF) {
		it821x_program_udma(drive, itdev->udma[unit]);
		it821x_program(drive, itdev->pio[unit]);
	}
}


static void it821x_set_pio_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
	struct it821x_dev *itdev = ide_get_hwifdata(hwif);
	ide_drive_t *pair = ide_get_pair_dev(drive);
	const u8 pio = drive->pio_mode - XFER_PIO_0;
	u8 unit = drive->dn & 1, set_pio = pio;

	
	static u16 pio_timings[]= { 0xAA88, 0xA382, 0xA181, 0x3332, 0x3121 };
	static u8 pio_want[]    = { ATA_66, ATA_66, ATA_66, ATA_66, ATA_ANY };

	if (pair) {
		u8 pair_pio = pair->pio_mode - XFER_PIO_0;
		
		if (pair_pio < set_pio)
			set_pio = pair_pio;
	}

	
	itdev->want[unit][1] = pio_want[set_pio];
	itdev->want[unit][0] = 1;	
	itdev->pio[unit] = pio_timings[set_pio];
	it821x_clock_strategy(drive);
	it821x_program(drive, itdev->pio[unit]);
}


static void it821x_tune_mwdma(ide_drive_t *drive, u8 mode_wanted)
{
	ide_hwif_t *hwif = drive->hwif;
	struct pci_dev *dev = to_pci_dev(hwif->dev);
	struct it821x_dev *itdev = (void *)ide_get_hwifdata(hwif);
	u8 unit = drive->dn & 1, channel = hwif->channel, conf;

	static u16 dma[]	= { 0x8866, 0x3222, 0x3121 };
	static u8 mwdma_want[]	= { ATA_ANY, ATA_66, ATA_ANY };

	itdev->want[unit][1] = mwdma_want[mode_wanted];
	itdev->want[unit][0] = 2;	
	itdev->mwdma[unit] = dma[mode_wanted];
	itdev->udma[unit] = UDMA_OFF;

	
	pci_read_config_byte(dev, 0x50, &conf);
	if (itdev->timing10)
		conf |= channel ? 0x60: 0x18;
	else
		conf |= 1 << (3 + 2 * channel + unit);
	pci_write_config_byte(dev, 0x50, conf);

	it821x_clock_strategy(drive);
	
	
}


static void it821x_tune_udma(ide_drive_t *drive, u8 mode_wanted)
{
	ide_hwif_t *hwif = drive->hwif;
	struct pci_dev *dev = to_pci_dev(hwif->dev);
	struct it821x_dev *itdev = ide_get_hwifdata(hwif);
	u8 unit = drive->dn & 1, channel = hwif->channel, conf;

	static u16 udma[]	= { 0x4433, 0x4231, 0x3121, 0x2121, 0x1111, 0x2211, 0x1111 };
	static u8 udma_want[]	= { ATA_ANY, ATA_50, ATA_ANY, ATA_66, ATA_66, ATA_50, ATA_66 };

	itdev->want[unit][1] = udma_want[mode_wanted];
	itdev->want[unit][0] = 3;	
	itdev->mwdma[unit] = MWDMA_OFF;
	itdev->udma[unit] = udma[mode_wanted];
	if(mode_wanted >= 5)
		itdev->udma[unit] |= 0x8080;	

	
	pci_read_config_byte(dev, 0x50, &conf);
	if (itdev->timing10)
		conf &= channel ? 0x9F: 0xE7;
	else
		conf &= ~ (1 << (3 + 2 * channel + unit));
	pci_write_config_byte(dev, 0x50, conf);

	it821x_clock_strategy(drive);
	it821x_program_udma(drive, itdev->udma[unit]);

}


static void it821x_dma_start(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	struct it821x_dev *itdev = ide_get_hwifdata(hwif);
	u8 unit = drive->dn & 1;

	if(itdev->mwdma[unit] != MWDMA_OFF)
		it821x_program(drive, itdev->mwdma[unit]);
	else if(itdev->udma[unit] != UDMA_OFF && itdev->timing10)
		it821x_program_udma(drive, itdev->udma[unit]);
	ide_dma_start(drive);
}


static int it821x_dma_end(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	struct it821x_dev *itdev = ide_get_hwifdata(hwif);
	int ret = ide_dma_end(drive);
	u8 unit = drive->dn & 1;

	if(itdev->mwdma[unit] != MWDMA_OFF)
		it821x_program(drive, itdev->pio[unit]);
	return ret;
}


static void it821x_set_dma_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
	const u8 speed = drive->dma_mode;

	if (speed >= XFER_UDMA_0 && speed <= XFER_UDMA_6)
		it821x_tune_udma(drive, speed - XFER_UDMA_0);
	else if (speed >= XFER_MW_DMA_0 && speed <= XFER_MW_DMA_2)
		it821x_tune_mwdma(drive, speed - XFER_MW_DMA_0);
}


static u8 it821x_cable_detect(ide_hwif_t *hwif)
{
	
	return ATA_CBL_PATA80;
}


static void it821x_quirkproc(ide_drive_t *drive)
{
	struct it821x_dev *itdev = ide_get_hwifdata(drive->hwif);
	u16 *id = drive->id;

	if (!itdev->smart) {
		drive->dev_flags |= IDE_DFLAG_UNMASK;
	} else {

		
		if (strstr((char *)&id[ATA_ID_PROD],
			   "Integrated Technology Express")) {
			id[ATA_ID_CAPABILITY]    |= (3 << 8); 
			id[ATA_ID_COMMAND_SET_2] |= 0x0400;   
			id[ATA_ID_CFS_ENABLE_2]  |= 0x0400;   
			
			printk(KERN_INFO "%s: IT8212 %sRAID %d volume",
				drive->name, id[147] ? "Bootable " : "",
				id[ATA_ID_CSFO]);
			if (id[ATA_ID_CSFO] != 1)
				printk(KERN_CONT "(%dK stripe)", id[146]);
			printk(KERN_CONT ".\n");
		} else {
			id[ATA_ID_FIELD_VALID]	 &= 3;
			id[ATA_ID_QUEUE_DEPTH]	  = 0;
			id[ATA_ID_COMMAND_SET_1]  = 0;
			id[ATA_ID_COMMAND_SET_2] &= 0xC400;
			id[ATA_ID_CFSSE]	 &= 0xC000;
			id[ATA_ID_CFS_ENABLE_1]	  = 0;
			id[ATA_ID_CFS_ENABLE_2]	 &= 0xC400;
			id[ATA_ID_CSF_DEFAULT]	 &= 0xC000;
			id[127]			  = 0;
			id[ATA_ID_DLF]		  = 0;
			id[ATA_ID_CSFO]		  = 0;
			id[ATA_ID_CFA_POWER]	  = 0;
			printk(KERN_INFO "%s: Performing identify fixups.\n",
				drive->name);
		}

		if (ata_id_has_dma(id)) {
			id[ATA_ID_MWDMA_MODES] |= 0x0101;
			drive->current_speed = XFER_MW_DMA_0;
		}
	}

}

static struct ide_dma_ops it821x_pass_through_dma_ops = {
	.dma_host_set		= ide_dma_host_set,
	.dma_setup		= ide_dma_setup,
	.dma_start		= it821x_dma_start,
	.dma_end		= it821x_dma_end,
	.dma_test_irq		= ide_dma_test_irq,
	.dma_lost_irq		= ide_dma_lost_irq,
	.dma_timer_expiry	= ide_dma_sff_timer_expiry,
	.dma_sff_read_status	= ide_dma_sff_read_status,
};


static void __devinit init_hwif_it821x(ide_hwif_t *hwif)
{
	struct pci_dev *dev = to_pci_dev(hwif->dev);
	struct ide_host *host = pci_get_drvdata(dev);
	struct it821x_dev *itdevs = host->host_priv;
	struct it821x_dev *idev = itdevs + hwif->channel;
	u8 conf;

	ide_set_hwifdata(hwif, idev);

	pci_read_config_byte(dev, 0x50, &conf);
	if (conf & 1) {
		idev->smart = 1;
		hwif->host_flags |= IDE_HFLAG_NO_ATAPI_DMA;
		hwif->rqsize = 256;
	}

	
	if (conf & (1 << (1 + hwif->channel)))
		idev->clock_mode = ATA_50;
	else
		idev->clock_mode = ATA_66;

	idev->want[0][1] = ATA_ANY;
	idev->want[1][1] = ATA_ANY;


	if (dev->revision == 0x10) {
		idev->timing10 = 1;
		hwif->host_flags |= IDE_HFLAG_NO_ATAPI_DMA;
		if (idev->smart == 0)
			printk(KERN_WARNING DRV_NAME " %s: revision 0x10, "
				"workarounds activated\n", pci_name(dev));
	}

	if (idev->smart == 0) {
		
		hwif->dma_ops = &it821x_pass_through_dma_ops;
	} else
		hwif->host_flags |= IDE_HFLAG_NO_SET_MODE;

	if (hwif->dma_base == 0)
		return;

	hwif->ultra_mask = ATA_UDMA6;
	hwif->mwdma_mask = ATA_MWDMA2;

	
	if (idev->quirks & QUIRK_VORTEX86) {
		if (dev->revision == 0x11)
			hwif->ultra_mask = 0;
	}
}

static void it8212_disable_raid(struct pci_dev *dev)
{
	
	pci_write_config_byte(dev, 0x5E, 0x01);

	
	pci_write_config_byte(dev, 0x50, 0x00);
	pci_write_config_word(dev, PCI_COMMAND,
			      PCI_COMMAND_PARITY | PCI_COMMAND_IO |
			      PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	pci_write_config_word(dev, 0x40, 0xA0F3);

	pci_write_config_dword(dev,0x4C, 0x02040204);
	pci_write_config_byte(dev, 0x42, 0x36);
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x20);
}

static int init_chipset_it821x(struct pci_dev *dev)
{
	u8 conf;
	static char *mode[2] = { "pass through", "smart" };

	
	if (it8212_noraid) {
		printk(KERN_INFO DRV_NAME " %s: forcing bypass mode\n",
			pci_name(dev));
		it8212_disable_raid(dev);
	}
	pci_read_config_byte(dev, 0x50, &conf);
	printk(KERN_INFO DRV_NAME " %s: controller in %s mode\n",
		pci_name(dev), mode[conf & 1]);
	return 0;
}

static const struct ide_port_ops it821x_port_ops = {
	
	.set_pio_mode		= it821x_set_pio_mode,
	.set_dma_mode		= it821x_set_dma_mode,
	.quirkproc		= it821x_quirkproc,
	.cable_detect		= it821x_cable_detect,
};

static const struct ide_port_info it821x_chipset __devinitdata = {
	.name		= DRV_NAME,
	.init_chipset	= init_chipset_it821x,
	.init_hwif	= init_hwif_it821x,
	.port_ops	= &it821x_port_ops,
	.pio_mask	= ATA_PIO4,
};


static int __devinit it821x_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct it821x_dev *itdevs;
	int rc;

	itdevs = kzalloc(2 * sizeof(*itdevs), GFP_KERNEL);
	if (itdevs == NULL) {
		printk(KERN_ERR DRV_NAME " %s: out of memory\n", pci_name(dev));
		return -ENOMEM;
	}

	itdevs->quirks = id->driver_data;

	rc = ide_pci_init_one(dev, &it821x_chipset, itdevs);
	if (rc)
		kfree(itdevs);

	return rc;
}

static void __devexit it821x_remove(struct pci_dev *dev)
{
	struct ide_host *host = pci_get_drvdata(dev);
	struct it821x_dev *itdevs = host->host_priv;

	ide_pci_remove(dev);
	kfree(itdevs);
}

static const struct pci_device_id it821x_pci_tbl[] = {
	{ PCI_VDEVICE(ITE, PCI_DEVICE_ID_ITE_8211), 0 },
	{ PCI_VDEVICE(ITE, PCI_DEVICE_ID_ITE_8212), 0 },
	{ PCI_VDEVICE(RDC, PCI_DEVICE_ID_RDC_D1010), QUIRK_VORTEX86 },
	{ 0, },
};

MODULE_DEVICE_TABLE(pci, it821x_pci_tbl);

static struct pci_driver it821x_pci_driver = {
	.name		= "ITE821x IDE",
	.id_table	= it821x_pci_tbl,
	.probe		= it821x_init_one,
	.remove		= __devexit_p(it821x_remove),
	.suspend	= ide_pci_suspend,
	.resume		= ide_pci_resume,
};

static int __init it821x_ide_init(void)
{
	return ide_pci_register_driver(&it821x_pci_driver);
}

static void __exit it821x_ide_exit(void)
{
	pci_unregister_driver(&it821x_pci_driver);
}

module_init(it821x_ide_init);
module_exit(it821x_ide_exit);

module_param_named(noraid, it8212_noraid, int, S_IRUGO);
MODULE_PARM_DESC(noraid, "Force card into bypass mode");

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("PCI driver module for the ITE 821x");
MODULE_LICENSE("GPL");
