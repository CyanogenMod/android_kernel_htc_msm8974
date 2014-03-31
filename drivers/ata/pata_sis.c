/*
 *    pata_sis.c - SiS ATA driver
 *
 *	(C) 2005 Red Hat
 *	(C) 2007,2009 Bartlomiej Zolnierkiewicz
 *
 *    Based upon linux/drivers/ide/pci/sis5513.c
 * Copyright (C) 1999-2000	Andre Hedrick <andre@linux-ide.org>
 * Copyright (C) 2002		Lionel Bouton <Lionel.Bouton@inet6.fr>, Maintainer
 * Copyright (C) 2003		Vojtech Pavlik <vojtech@suse.cz>
 * SiS Taiwan		: for direct support and hardware.
 * Daniela Engert	: for initial ATA100 advices and numerous others.
 * John Fremlin, Manfred Spraul, Dave Morgan, Peter Kjellerstedt	:
 *			  for checking code correctness, providing patches.
 * Original tests and design on the SiS620 chipset.
 * ATA100 tests and design on the SiS735 chipset.
 * ATA16/33 support from specs
 * ATA133 support for SiS961/962 by L.C. Chang <lcchang@sis.com.tw>
 *
 *
 *	TODO
 *	Check MWDMA on drives that don't support MWDMA speed pio cycles ?
 *	More Testing
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
#include "sis.h"

#define DRV_NAME	"pata_sis"
#define DRV_VERSION	"0.5.2"

struct sis_chipset {
	u16 device;				
	const struct ata_port_info *info;	
};

struct sis_laptop {
	u16 device;
	u16 subvendor;
	u16 subdevice;
};

static const struct sis_laptop sis_laptop[] = {
	
	{ 0x5513, 0x1043, 0x1107 },	
	{ 0x5513, 0x1734, 0x105F },	
	{ 0x5513, 0x1071, 0x8640 },	
	
	{ 0, }
};

static int sis_short_ata40(struct pci_dev *dev)
{
	const struct sis_laptop *lap = &sis_laptop[0];

	while (lap->device) {
		if (lap->device == dev->device &&
		    lap->subvendor == dev->subsystem_vendor &&
		    lap->subdevice == dev->subsystem_device)
			return 1;
		lap++;
	}

	return 0;
}


static int sis_old_port_base(struct ata_device *adev)
{
	return 0x40 + (4 * adev->link->ap->port_no) + (2 * adev->devno);
}


static int sis_port_base(struct ata_device *adev)
{
	struct ata_port *ap = adev->link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int port = 0x40;
	u32 reg54;

	
	pci_read_config_dword(pdev, 0x54, &reg54);
	if (reg54 & 0x40000000)
		port = 0x70;

	return port + (8 * ap->port_no) + (4 * adev->devno);
}


static int sis_133_cable_detect(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u16 tmp;

	
	pci_read_config_word(pdev, 0x50 + 2 * ap->port_no, &tmp);
	if ((tmp & 0x8000) && !sis_short_ata40(pdev))
		return ATA_CBL_PATA40;
	return ATA_CBL_PATA80;
}


static int sis_66_cable_detect(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u8 tmp;

	
	pci_read_config_byte(pdev, 0x48, &tmp);
	tmp >>= ap->port_no;
	if ((tmp & 0x10) && !sis_short_ata40(pdev))
		return ATA_CBL_PATA40;
	return ATA_CBL_PATA80;
}



static int sis_pre_reset(struct ata_link *link, unsigned long deadline)
{
	static const struct pci_bits sis_enable_bits[] = {
		{ 0x4aU, 1U, 0x02UL, 0x02UL },	
		{ 0x4aU, 1U, 0x04UL, 0x04UL },	
	};

	struct ata_port *ap = link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);

	if (!pci_test_config_bits(pdev, &sis_enable_bits[ap->port_no]))
		return -ENOENT;

	pci_write_config_byte(pdev, 0x4B, 0);
	return ata_sff_prereset(link, deadline);
}



static void sis_set_fifo(struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u8 fifoctrl;
	u8 mask = 0x11;

	mask <<= (2 * ap->port_no);
	mask <<= adev->devno;

	
	pci_read_config_byte(pdev, 0x4B, &fifoctrl);
	fifoctrl &= ~mask;

	
	if (adev->class == ATA_DEV_ATA)
		fifoctrl |= mask;
	pci_write_config_byte(pdev, 0x4B, fifoctrl);
}


static void sis_old_set_piomode (struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int port = sis_old_port_base(adev);
	u8 t1, t2;
	int speed = adev->pio_mode - XFER_PIO_0;

	static const u8 active[]   = { 0x00, 0x07, 0x04, 0x03, 0x01 };
	static const u8 recovery[] = { 0x00, 0x06, 0x04, 0x03, 0x03 };

	sis_set_fifo(ap, adev);

	pci_read_config_byte(pdev, port, &t1);
	pci_read_config_byte(pdev, port + 1, &t2);

	t1 &= ~0x0F;	
	t2 &= ~0x07;

	t1 |= active[speed];
	t2 |= recovery[speed];

	pci_write_config_byte(pdev, port, t1);
	pci_write_config_byte(pdev, port + 1, t2);
}


static void sis_100_set_piomode (struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int port = sis_old_port_base(adev);
	int speed = adev->pio_mode - XFER_PIO_0;

	static const u8 actrec[] = { 0x00, 0x67, 0x44, 0x33, 0x31 };

	sis_set_fifo(ap, adev);

	pci_write_config_byte(pdev, port, actrec[speed]);
}


static void sis_133_set_piomode (struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int port;
	u32 t1;
	int speed = adev->pio_mode - XFER_PIO_0;

	static const u32 timing133[] = {
		0x28269000,	
		0x0C266000,
		0x04263000,
		0x0C0A3000,
		0x05093000
	};
	static const u32 timing100[] = {
		0x1E1C6000,	
		0x091C4000,
		0x031C2000,
		0x09072000,
		0x04062000
	};

	sis_set_fifo(ap, adev);

	port = sis_port_base(adev);
	pci_read_config_dword(pdev, port, &t1);
	t1 &= 0xC0C00FFF;	

	if (t1 & 0x08)		
		t1 |= timing133[speed];
	else
		t1 |= timing100[speed];
	pci_write_config_byte(pdev, port, t1);
}


static void sis_old_set_dmamode (struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int speed = adev->dma_mode - XFER_MW_DMA_0;
	int drive_pci = sis_old_port_base(adev);
	u16 timing;

	static const u16 mwdma_bits[] = { 0x008, 0x302, 0x301 };
	static const u16 udma_bits[]  = { 0xE000, 0xC000, 0xA000 };

	pci_read_config_word(pdev, drive_pci, &timing);

	if (adev->dma_mode < XFER_UDMA_0) {
		timing &= ~0x870F;
		timing |= mwdma_bits[speed];
	} else {
		
		speed = adev->dma_mode - XFER_UDMA_0;
		timing &= ~0x6000;
		timing |= udma_bits[speed];
	}
	pci_write_config_word(pdev, drive_pci, timing);
}


static void sis_66_set_dmamode (struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int speed = adev->dma_mode - XFER_MW_DMA_0;
	int drive_pci = sis_old_port_base(adev);
	u16 timing;

	
	static const u16 mwdma_bits[] = { 0x008, 0x302, 0x301 };
	static const u16 udma_bits[]  = { 0xF000, 0xD000, 0xB000, 0xA000, 0x9000, 0x8000 };

	pci_read_config_word(pdev, drive_pci, &timing);

	if (adev->dma_mode < XFER_UDMA_0) {
		timing &= ~0x870F;
		timing |= mwdma_bits[speed];
	} else {
		
		speed = adev->dma_mode - XFER_UDMA_0;
		timing &= ~0xF000;
		timing |= udma_bits[speed];
	}
	pci_write_config_word(pdev, drive_pci, timing);
}


static void sis_100_set_dmamode (struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int speed = adev->dma_mode - XFER_MW_DMA_0;
	int drive_pci = sis_old_port_base(adev);
	u8 timing;

	static const u8 udma_bits[]  = { 0x8B, 0x87, 0x85, 0x83, 0x82, 0x81};

	pci_read_config_byte(pdev, drive_pci + 1, &timing);

	if (adev->dma_mode < XFER_UDMA_0) {
		
	} else {
		
		speed = adev->dma_mode - XFER_UDMA_0;
		timing &= ~0x8F;
		timing |= udma_bits[speed];
	}
	pci_write_config_byte(pdev, drive_pci + 1, timing);
}


static void sis_133_early_set_dmamode (struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int speed = adev->dma_mode - XFER_MW_DMA_0;
	int drive_pci = sis_old_port_base(adev);
	u8 timing;
	
	static const u8 udma_bits[]  = { 0x8F, 0x8A, 0x87, 0x85, 0x83, 0x82, 0x81};

	pci_read_config_byte(pdev, drive_pci + 1, &timing);

	if (adev->dma_mode < XFER_UDMA_0) {
		
	} else {
		
		speed = adev->dma_mode - XFER_UDMA_0;
		timing &= ~0x8F;
		timing |= udma_bits[speed];
	}
	pci_write_config_byte(pdev, drive_pci + 1, timing);
}


static void sis_133_set_dmamode (struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int port;
	u32 t1;

	port = sis_port_base(adev);
	pci_read_config_dword(pdev, port, &t1);

	if (adev->dma_mode < XFER_UDMA_0) {
		
		static const u32 timing_u100[] = { 0x19154000, 0x06072000, 0x04062000 };
		static const u32 timing_u133[] = { 0x221C6000, 0x0C0A3000, 0x05093000 };
		int speed = adev->dma_mode - XFER_MW_DMA_0;

		t1 &= 0xC0C00FFF;
		
		t1 &= ~0x00000004;
		if (t1 & 0x08)
			t1 |= timing_u133[speed];
		else
			t1 |= timing_u100[speed];
	} else {
		
		static const u32 timing_u100[] = { 0x6B0, 0x470, 0x350, 0x140, 0x120, 0x110, 0x000 };
		static const u32 timing_u133[] = { 0x9F0, 0x6A0, 0x470, 0x250, 0x230, 0x220, 0x210 };
		int speed = adev->dma_mode - XFER_UDMA_0;

		t1 &= ~0x00000FF0;
		
		t1 |= 0x00000004;
		if (t1 & 0x08)
			t1 |= timing_u133[speed];
		else
			t1 |= timing_u100[speed];
	}
	pci_write_config_dword(pdev, port, t1);
}


static unsigned long sis_133_mode_filter(struct ata_device *adev, unsigned long mask)
{
	struct ata_port *ap = adev->link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int port = sis_port_base(adev);
	u32 t1;

	pci_read_config_dword(pdev, port, &t1);
	
	if (!(t1 & 0x08))
		mask &= ~(0xC0 << ATA_SHIFT_UDMA);
	return mask;
}

static struct scsi_host_template sis_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations sis_133_for_sata_ops = {
	.inherits		= &ata_bmdma_port_ops,
	.set_piomode		= sis_133_set_piomode,
	.set_dmamode		= sis_133_set_dmamode,
	.cable_detect		= sis_133_cable_detect,
};

static struct ata_port_operations sis_base_ops = {
	.inherits		= &ata_bmdma_port_ops,
	.prereset		= sis_pre_reset,
};

static struct ata_port_operations sis_133_ops = {
	.inherits		= &sis_base_ops,
	.set_piomode		= sis_133_set_piomode,
	.set_dmamode		= sis_133_set_dmamode,
	.cable_detect		= sis_133_cable_detect,
	.mode_filter		= sis_133_mode_filter,
};

static struct ata_port_operations sis_133_early_ops = {
	.inherits		= &sis_base_ops,
	.set_piomode		= sis_100_set_piomode,
	.set_dmamode		= sis_133_early_set_dmamode,
	.cable_detect		= sis_66_cable_detect,
};

static struct ata_port_operations sis_100_ops = {
	.inherits		= &sis_base_ops,
	.set_piomode		= sis_100_set_piomode,
	.set_dmamode		= sis_100_set_dmamode,
	.cable_detect		= sis_66_cable_detect,
};

static struct ata_port_operations sis_66_ops = {
	.inherits		= &sis_base_ops,
	.set_piomode		= sis_old_set_piomode,
	.set_dmamode		= sis_66_set_dmamode,
	.cable_detect		= sis_66_cable_detect,
};

static struct ata_port_operations sis_old_ops = {
	.inherits		= &sis_base_ops,
	.set_piomode		= sis_old_set_piomode,
	.set_dmamode		= sis_old_set_dmamode,
	.cable_detect		= ata_cable_40wire,
};

static const struct ata_port_info sis_info = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	.mwdma_mask	= ATA_MWDMA2,
	
	.port_ops	= &sis_old_ops,
};
static const struct ata_port_info sis_info33 = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	.mwdma_mask	= ATA_MWDMA2,
	.udma_mask	= ATA_UDMA2,
	.port_ops	= &sis_old_ops,
};
static const struct ata_port_info sis_info66 = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	
	.udma_mask	= ATA_UDMA4,
	.port_ops	= &sis_66_ops,
};
static const struct ata_port_info sis_info100 = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	
	.udma_mask	= ATA_UDMA5,
	.port_ops	= &sis_100_ops,
};
static const struct ata_port_info sis_info100_early = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	
	.udma_mask	= ATA_UDMA5,
	.port_ops	= &sis_66_ops,
};
static const struct ata_port_info sis_info133 = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	.mwdma_mask	= ATA_MWDMA2,
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &sis_133_ops,
};
const struct ata_port_info sis_info133_for_sata = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &sis_133_for_sata_ops,
};
static const struct ata_port_info sis_info133_early = {
	.flags		= ATA_FLAG_SLAVE_POSS,
	.pio_mask	= ATA_PIO4,
	
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &sis_133_early_ops,
};

EXPORT_SYMBOL_GPL(sis_info133_for_sata);

static void sis_fixup(struct pci_dev *pdev, struct sis_chipset *sis)
{
	u16 regw;
	u8 reg;

	if (sis->info == &sis_info133) {
		pci_read_config_word(pdev, 0x50, &regw);
		if (regw & 0x08)
			pci_write_config_word(pdev, 0x50, regw & ~0x08);
		pci_read_config_word(pdev, 0x52, &regw);
		if (regw & 0x08)
			pci_write_config_word(pdev, 0x52, regw & ~0x08);
		return;
	}

	if (sis->info == &sis_info133_early || sis->info == &sis_info100) {
		
		pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 0x80);
		
		pci_read_config_byte(pdev, 0x49, &reg);
		if (!(reg & 0x01))
			pci_write_config_byte(pdev, 0x49, reg | 0x01);
		return;
	}

	if (sis->info == &sis_info66 || sis->info == &sis_info100_early) {
		
		pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 0x80);
		
		pci_read_config_byte(pdev, 0x52, &reg);
		if (!(reg & 0x04))
			pci_write_config_byte(pdev, 0x52, reg | 0x04);
		return;
	}

	if (sis->info == &sis_info33) {
		pci_read_config_byte(pdev, PCI_CLASS_PROG, &reg);
		if (( reg & 0x0F ) != 0x00)
			pci_write_config_byte(pdev, PCI_CLASS_PROG, reg & 0xF0);
		
	}

	if (sis->info == &sis_info || sis->info == &sis_info33) {
		pci_read_config_byte(pdev, 0x52, &reg);
		if (!(reg & 0x08))
			pci_write_config_byte(pdev, 0x52, reg|0x08);
		return;
	}

	BUG();
}


static int sis_init_one (struct pci_dev *pdev, const struct pci_device_id *ent)
{
	const struct ata_port_info *ppi[] = { NULL, NULL };
	struct pci_dev *host = NULL;
	struct sis_chipset *chipset = NULL;
	struct sis_chipset *sets;
	int rc;

	static struct sis_chipset sis_chipsets[] = {

		{ 0x0968, &sis_info133 },
		{ 0x0966, &sis_info133 },
		{ 0x0965, &sis_info133 },
		{ 0x0745, &sis_info100 },
		{ 0x0735, &sis_info100 },
		{ 0x0733, &sis_info100 },
		{ 0x0635, &sis_info100 },
		{ 0x0633, &sis_info100 },

		{ 0x0730, &sis_info100_early },	
		{ 0x0550, &sis_info100_early },	

		{ 0x0640, &sis_info66 },
		{ 0x0630, &sis_info66 },
		{ 0x0620, &sis_info66 },
		{ 0x0540, &sis_info66 },
		{ 0x0530, &sis_info66 },

		{ 0x5600, &sis_info33 },
		{ 0x5598, &sis_info33 },
		{ 0x5597, &sis_info33 },
		{ 0x5591, &sis_info33 },
		{ 0x5582, &sis_info33 },
		{ 0x5581, &sis_info33 },

		{ 0x5596, &sis_info },
		{ 0x5571, &sis_info },
		{ 0x5517, &sis_info },
		{ 0x5511, &sis_info },

		{0}
	};
	static struct sis_chipset sis133_early = {
		0x0, &sis_info133_early
	};
	static struct sis_chipset sis133 = {
		0x0, &sis_info133
	};
	static struct sis_chipset sis100_early = {
		0x0, &sis_info100_early
	};
	static struct sis_chipset sis100 = {
		0x0, &sis_info100
	};

	ata_print_version_once(&pdev->dev, DRV_VERSION);

	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	
	for (sets = &sis_chipsets[0]; sets->device; sets++) {
		host = pci_get_device(PCI_VENDOR_ID_SI, sets->device, NULL);
		if (host != NULL) {
			chipset = sets;			
			if (sets->device == 0x630) {	
				if (host->revision >= 0x30)	
					chipset = &sis100_early;
			}
			break;
		}
	}

	
	if (chipset == NULL) {
		
		u32 idemisc;
		u16 trueid;


		pci_read_config_dword(pdev, 0x54, &idemisc);
		pci_write_config_dword(pdev, 0x54, idemisc & 0x7fffffff);
		pci_read_config_word(pdev, PCI_DEVICE_ID, &trueid);
		pci_write_config_dword(pdev, 0x54, idemisc);

		switch(trueid) {
		case 0x5518:	
			dev_info(&pdev->dev,
				 "SiS 962/963 MuTIOL IDE UDMA133 controller\n");
			chipset = &sis133;
			if ((idemisc & 0x40000000) == 0) {
				pci_write_config_dword(pdev, 0x54, idemisc | 0x40000000);
				dev_info(&pdev->dev,
					 "Switching to 5513 register mapping\n");
			}
			break;
		case 0x0180:	
			chipset = &sis133;
			break;
		case 0x1180:	
			chipset = &sis133;
			break;
		}
	}

	
	if (chipset == NULL) {
		struct pci_dev *lpc_bridge;
		u16 trueid;
		u8 prefctl;
		u8 idecfg;

		
		pci_read_config_byte(pdev, 0x4a, &idecfg);
		pci_write_config_byte(pdev, 0x4a, idecfg | 0x10);
		pci_read_config_word(pdev, PCI_DEVICE_ID, &trueid);
		pci_write_config_byte(pdev, 0x4a, idecfg);

		switch(trueid) {
		case 0x5517:
			lpc_bridge = pci_get_slot(pdev->bus, 0x10); 
			if (lpc_bridge == NULL)
				break;
			pci_read_config_byte(pdev, 0x49, &prefctl);
			pci_dev_put(lpc_bridge);

			if (lpc_bridge->revision == 0x10 && (prefctl & 0x80)) {
				chipset = &sis133_early;
				break;
			}
			chipset = &sis100;
			break;
		}
	}
	pci_dev_put(host);

	
	if (chipset == NULL)
		return -ENODEV;

	ppi[0] = chipset->info;

	sis_fixup(pdev, chipset);

	return ata_pci_bmdma_init_one(pdev, ppi, &sis_sht, chipset, 0);
}

#ifdef CONFIG_PM
static int sis_reinit_one(struct pci_dev *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	int rc;

	rc = ata_pci_device_do_resume(pdev);
	if (rc)
		return rc;

	sis_fixup(pdev, host->private_data);

	ata_host_resume(host);
	return 0;
}
#endif

static const struct pci_device_id sis_pci_tbl[] = {
	{ PCI_VDEVICE(SI, 0x5513), },	
	{ PCI_VDEVICE(SI, 0x5518), },	
	{ PCI_VDEVICE(SI, 0x1180), },	

	{ }
};

static struct pci_driver sis_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= sis_pci_tbl,
	.probe			= sis_init_one,
	.remove			= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend		= ata_pci_device_suspend,
	.resume			= sis_reinit_one,
#endif
};

static int __init sis_init(void)
{
	return pci_register_driver(&sis_pci_driver);
}

static void __exit sis_exit(void)
{
	pci_unregister_driver(&sis_pci_driver);
}

module_init(sis_init);
module_exit(sis_exit);

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("SCSI low-level driver for SiS ATA");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, sis_pci_tbl);
MODULE_VERSION(DRV_VERSION);

