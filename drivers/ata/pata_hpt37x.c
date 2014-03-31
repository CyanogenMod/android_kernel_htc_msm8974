/*
 * Libata driver for the highpoint 37x and 30x UDMA66 ATA controllers.
 *
 * This driver is heavily based upon:
 *
 * linux/drivers/ide/pci/hpt366.c		Version 0.36	April 25, 2003
 *
 * Copyright (C) 1999-2003		Andre Hedrick <andre@linux-ide.org>
 * Portions Copyright (C) 2001	        Sun Microsystems, Inc.
 * Portions Copyright (C) 2003		Red Hat Inc
 * Portions Copyright (C) 2005-2010	MontaVista Software, Inc.
 *
 * TODO
 *	Look into engine reset on timeout errors. Should not be	required.
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

#define DRV_NAME	"pata_hpt37x"
#define DRV_VERSION	"0.6.23"

struct hpt_clock {
	u8	xfer_speed;
	u32	timing;
};

struct hpt_chip {
	const char *name;
	unsigned int base;
	struct hpt_clock const *clocks[4];
};


static struct hpt_clock hpt37x_timings_33[] = {
	{ XFER_UDMA_6,		0x12446231 },	
	{ XFER_UDMA_5,		0x12446231 },
	{ XFER_UDMA_4,		0x12446231 },
	{ XFER_UDMA_3,		0x126c6231 },
	{ XFER_UDMA_2,		0x12486231 },
	{ XFER_UDMA_1,		0x124c6233 },
	{ XFER_UDMA_0,		0x12506297 },

	{ XFER_MW_DMA_2,	0x22406c31 },
	{ XFER_MW_DMA_1,	0x22406c33 },
	{ XFER_MW_DMA_0,	0x22406c97 },

	{ XFER_PIO_4,		0x06414e31 },
	{ XFER_PIO_3,		0x06414e42 },
	{ XFER_PIO_2,		0x06414e53 },
	{ XFER_PIO_1,		0x06814e93 },
	{ XFER_PIO_0,		0x06814ea7 }
};

static struct hpt_clock hpt37x_timings_50[] = {
	{ XFER_UDMA_6,		0x12848242 },
	{ XFER_UDMA_5,		0x12848242 },
	{ XFER_UDMA_4,		0x12ac8242 },
	{ XFER_UDMA_3,		0x128c8242 },
	{ XFER_UDMA_2,		0x120c8242 },
	{ XFER_UDMA_1,		0x12148254 },
	{ XFER_UDMA_0,		0x121882ea },

	{ XFER_MW_DMA_2,	0x22808242 },
	{ XFER_MW_DMA_1,	0x22808254 },
	{ XFER_MW_DMA_0,	0x228082ea },

	{ XFER_PIO_4,		0x0a81f442 },
	{ XFER_PIO_3,		0x0a81f443 },
	{ XFER_PIO_2,		0x0a81f454 },
	{ XFER_PIO_1,		0x0ac1f465 },
	{ XFER_PIO_0,		0x0ac1f48a }
};

static struct hpt_clock hpt37x_timings_66[] = {
	{ XFER_UDMA_6,		0x1c869c62 },
	{ XFER_UDMA_5,		0x1cae9c62 },	
	{ XFER_UDMA_4,		0x1c8a9c62 },
	{ XFER_UDMA_3,		0x1c8e9c62 },
	{ XFER_UDMA_2,		0x1c929c62 },
	{ XFER_UDMA_1,		0x1c9a9c62 },
	{ XFER_UDMA_0,		0x1c829c62 },

	{ XFER_MW_DMA_2,	0x2c829c62 },
	{ XFER_MW_DMA_1,	0x2c829c66 },
	{ XFER_MW_DMA_0,	0x2c829d2e },

	{ XFER_PIO_4,		0x0c829c62 },
	{ XFER_PIO_3,		0x0c829c84 },
	{ XFER_PIO_2,		0x0c829ca6 },
	{ XFER_PIO_1,		0x0d029d26 },
	{ XFER_PIO_0,		0x0d029d5e }
};


static const struct hpt_chip hpt370 = {
	"HPT370",
	48,
	{
		hpt37x_timings_33,
		NULL,
		NULL,
		NULL
	}
};

static const struct hpt_chip hpt370a = {
	"HPT370A",
	48,
	{
		hpt37x_timings_33,
		NULL,
		hpt37x_timings_50,
		NULL
	}
};

static const struct hpt_chip hpt372 = {
	"HPT372",
	55,
	{
		hpt37x_timings_33,
		NULL,
		hpt37x_timings_50,
		hpt37x_timings_66
	}
};

static const struct hpt_chip hpt302 = {
	"HPT302",
	66,
	{
		hpt37x_timings_33,
		NULL,
		hpt37x_timings_50,
		hpt37x_timings_66
	}
};

static const struct hpt_chip hpt371 = {
	"HPT371",
	66,
	{
		hpt37x_timings_33,
		NULL,
		hpt37x_timings_50,
		hpt37x_timings_66
	}
};

static const struct hpt_chip hpt372a = {
	"HPT372A",
	66,
	{
		hpt37x_timings_33,
		NULL,
		hpt37x_timings_50,
		hpt37x_timings_66
	}
};

static const struct hpt_chip hpt374 = {
	"HPT374",
	48,
	{
		hpt37x_timings_33,
		NULL,
		NULL,
		NULL
	}
};


static u32 hpt37x_find_mode(struct ata_port *ap, int speed)
{
	struct hpt_clock *clocks = ap->host->private_data;

	while (clocks->xfer_speed) {
		if (clocks->xfer_speed == speed)
			return clocks->timing;
		clocks++;
	}
	BUG();
	return 0xffffffffU;	
}

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

static const char * const bad_ata100_5[] = {
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


static unsigned long hpt370_filter(struct ata_device *adev, unsigned long mask)
{
	if (adev->class == ATA_DEV_ATA) {
		if (hpt_dma_blacklisted(adev, "UDMA", bad_ata33))
			mask &= ~ATA_MASK_UDMA;
		if (hpt_dma_blacklisted(adev, "UDMA100", bad_ata100_5))
			mask &= ~(0xE0 << ATA_SHIFT_UDMA);
	}
	return mask;
}


static unsigned long hpt370a_filter(struct ata_device *adev, unsigned long mask)
{
	if (adev->class == ATA_DEV_ATA) {
		if (hpt_dma_blacklisted(adev, "UDMA100", bad_ata100_5))
			mask &= ~(0xE0 << ATA_SHIFT_UDMA);
	}
	return mask;
}

static unsigned long hpt372_filter(struct ata_device *adev, unsigned long mask)
{
	if (ata_id_is_sata(adev->id))
		mask &= ~((0xE << ATA_SHIFT_UDMA) | ATA_MASK_MWDMA);

	return mask;
}


static int hpt37x_cable_detect(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u8 scr2, ata66;

	pci_read_config_byte(pdev, 0x5B, &scr2);
	pci_write_config_byte(pdev, 0x5B, scr2 & ~0x01);

	udelay(10); 

	
	pci_read_config_byte(pdev, 0x5A, &ata66);
	
	pci_write_config_byte(pdev, 0x5B, scr2);

	if (ata66 & (2 >> ap->port_no))
		return ATA_CBL_PATA40;
	else
		return ATA_CBL_PATA80;
}


static int hpt374_fn1_cable_detect(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	unsigned int mcrbase = 0x50 + 4 * ap->port_no;
	u16 mcr3;
	u8 ata66;

	
	pci_read_config_word(pdev, mcrbase + 2, &mcr3);
	
	pci_write_config_word(pdev, mcrbase + 2, mcr3 | 0x8000);
	pci_read_config_byte(pdev, 0x5A, &ata66);
	
	pci_write_config_word(pdev, mcrbase + 2, mcr3);

	if (ata66 & (2 >> ap->port_no))
		return ATA_CBL_PATA40;
	else
		return ATA_CBL_PATA80;
}


static int hpt37x_pre_reset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	static const struct pci_bits hpt37x_enable_bits[] = {
		{ 0x50, 1, 0x04, 0x04 },
		{ 0x54, 1, 0x04, 0x04 }
	};

	if (!pci_test_config_bits(pdev, &hpt37x_enable_bits[ap->port_no]))
		return -ENOENT;

	
	pci_write_config_byte(pdev, 0x50 + 4 * ap->port_no, 0x37);
	udelay(100);

	return ata_sff_prereset(link, deadline);
}

static void hpt370_set_mode(struct ata_port *ap, struct ata_device *adev,
			    u8 mode)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u32 addr1, addr2;
	u32 reg, timing, mask;
	u8 fast;

	addr1 = 0x40 + 4 * (adev->devno + 2 * ap->port_no);
	addr2 = 0x51 + 4 * ap->port_no;

	
	pci_read_config_byte(pdev, addr2, &fast);
	fast &= ~0x02;
	fast |= 0x01;
	pci_write_config_byte(pdev, addr2, fast);

	
	if (mode < XFER_MW_DMA_0)
		mask = 0xcfc3ffff;
	else if (mode < XFER_UDMA_0)
		mask = 0x31c001ff;
	else
		mask = 0x303c0000;

	timing = hpt37x_find_mode(ap, mode);

	pci_read_config_dword(pdev, addr1, &reg);
	reg = (reg & ~mask) | (timing & mask);
	pci_write_config_dword(pdev, addr1, reg);
}

static void hpt370_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	hpt370_set_mode(ap, adev, adev->pio_mode);
}


static void hpt370_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	hpt370_set_mode(ap, adev, adev->dma_mode);
}


static void hpt370_bmdma_stop(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	void __iomem *bmdma = ap->ioaddr.bmdma_addr;
	u8 dma_stat = ioread8(bmdma + ATA_DMA_STATUS);
	u8 dma_cmd;

	if (dma_stat & ATA_DMA_ACTIVE) {
		udelay(20);
		dma_stat = ioread8(bmdma + ATA_DMA_STATUS);
	}
	if (dma_stat & ATA_DMA_ACTIVE) {
		
		pci_write_config_byte(pdev, 0x50 + 4 * ap->port_no, 0x37);
		udelay(10);
		
		dma_cmd = ioread8(bmdma + ATA_DMA_CMD);
		iowrite8(dma_cmd & ~ATA_DMA_START, bmdma + ATA_DMA_CMD);
		
		dma_stat = ioread8(bmdma + ATA_DMA_STATUS);
		iowrite8(dma_stat | ATA_DMA_INTR | ATA_DMA_ERR,
			 bmdma + ATA_DMA_STATUS);
		
		pci_write_config_byte(pdev, 0x50 + 4 * ap->port_no, 0x37);
		udelay(10);
	}
	ata_bmdma_stop(qc);
}

static void hpt372_set_mode(struct ata_port *ap, struct ata_device *adev,
			    u8 mode)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u32 addr1, addr2;
	u32 reg, timing, mask;
	u8 fast;

	addr1 = 0x40 + 4 * (adev->devno + 2 * ap->port_no);
	addr2 = 0x51 + 4 * ap->port_no;

	
	pci_read_config_byte(pdev, addr2, &fast);
	fast &= ~0x07;
	pci_write_config_byte(pdev, addr2, fast);

	
	if (mode < XFER_MW_DMA_0)
		mask = 0xcfc3ffff;
	else if (mode < XFER_UDMA_0)
		mask = 0x31c001ff;
	else
		mask = 0x303c0000;

	timing = hpt37x_find_mode(ap, mode);

	pci_read_config_dword(pdev, addr1, &reg);
	reg = (reg & ~mask) | (timing & mask);
	pci_write_config_dword(pdev, addr1, reg);
}


static void hpt372_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	hpt372_set_mode(ap, adev, adev->pio_mode);
}


static void hpt372_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	hpt372_set_mode(ap, adev, adev->dma_mode);
}


static void hpt37x_bmdma_stop(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int mscreg = 0x50 + 4 * ap->port_no;
	u8 bwsr_stat, msc_stat;

	pci_read_config_byte(pdev, 0x6A, &bwsr_stat);
	pci_read_config_byte(pdev, mscreg, &msc_stat);
	if (bwsr_stat & (1 << ap->port_no))
		pci_write_config_byte(pdev, mscreg, msc_stat | 0x30);
	ata_bmdma_stop(qc);
}


static struct scsi_host_template hpt37x_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};


static struct ata_port_operations hpt370_port_ops = {
	.inherits	= &ata_bmdma_port_ops,

	.bmdma_stop	= hpt370_bmdma_stop,

	.mode_filter	= hpt370_filter,
	.cable_detect	= hpt37x_cable_detect,
	.set_piomode	= hpt370_set_piomode,
	.set_dmamode	= hpt370_set_dmamode,
	.prereset	= hpt37x_pre_reset,
};


static struct ata_port_operations hpt370a_port_ops = {
	.inherits	= &hpt370_port_ops,
	.mode_filter	= hpt370a_filter,
};


static struct ata_port_operations hpt302_port_ops = {
	.inherits	= &ata_bmdma_port_ops,

	.bmdma_stop	= hpt37x_bmdma_stop,

	.cable_detect	= hpt37x_cable_detect,
	.set_piomode	= hpt372_set_piomode,
	.set_dmamode	= hpt372_set_dmamode,
	.prereset	= hpt37x_pre_reset,
};


static struct ata_port_operations hpt372_port_ops = {
	.inherits	= &hpt302_port_ops,
	.mode_filter	= hpt372_filter,
};


static struct ata_port_operations hpt374_fn1_port_ops = {
	.inherits	= &hpt372_port_ops,
	.cable_detect	= hpt374_fn1_cable_detect,
};


static int hpt37x_clock_slot(unsigned int freq, unsigned int base)
{
	unsigned int f = (base * freq) / 192;	
	if (f < 40)
		return 0;	
	if (f < 45)
		return 1;	
	if (f < 55)
		return 2;	
	return 3;		
}


static int hpt37x_calibrate_dpll(struct pci_dev *dev)
{
	u8 reg5b;
	u32 reg5c;
	int tries;

	for (tries = 0; tries < 0x5000; tries++) {
		udelay(50);
		pci_read_config_byte(dev, 0x5b, &reg5b);
		if (reg5b & 0x80) {
			
			for (tries = 0; tries < 0x1000; tries++) {
				pci_read_config_byte(dev, 0x5b, &reg5b);
				
				if ((reg5b & 0x80) == 0)
					return 0;
			}
			
			pci_read_config_dword(dev, 0x5c, &reg5c);
			pci_write_config_dword(dev, 0x5c, reg5c & ~0x100);
			return 1;
		}
	}
	
	return 0;
}

static u32 hpt374_read_freq(struct pci_dev *pdev)
{
	u32 freq;
	unsigned long io_base = pci_resource_start(pdev, 4);

	if (PCI_FUNC(pdev->devfn) & 1) {
		struct pci_dev *pdev_0;

		pdev_0 = pci_get_slot(pdev->bus, pdev->devfn - 1);
		
		if (pdev_0 == NULL)
			return 0;
		io_base = pci_resource_start(pdev_0, 4);
		freq = inl(io_base + 0x90);
		pci_dev_put(pdev_0);
	} else
		freq = inl(io_base + 0x90);
	return freq;
}


static int hpt37x_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	
	static const struct ata_port_info info_hpt370 = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA5,
		.port_ops = &hpt370_port_ops
	};
	
	static const struct ata_port_info info_hpt370a = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA5,
		.port_ops = &hpt370a_port_ops
	};
	
	static const struct ata_port_info info_hpt370_33 = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA4,
		.port_ops = &hpt370_port_ops
	};
	
	static const struct ata_port_info info_hpt370a_33 = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA4,
		.port_ops = &hpt370a_port_ops
	};
	
	static const struct ata_port_info info_hpt372 = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA6,
		.port_ops = &hpt372_port_ops
	};
	
	static const struct ata_port_info info_hpt302 = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA6,
		.port_ops = &hpt302_port_ops
	};
	
	static const struct ata_port_info info_hpt374_fn0 = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA5,
		.port_ops = &hpt372_port_ops
	};
	static const struct ata_port_info info_hpt374_fn1 = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA5,
		.port_ops = &hpt374_fn1_port_ops
	};

	static const int MHz[4] = { 33, 40, 50, 66 };
	void *private_data = NULL;
	const struct ata_port_info *ppi[] = { NULL, NULL };
	u8 rev = dev->revision;
	u8 irqmask;
	u8 mcr1;
	u32 freq;
	int prefer_dpll = 1;

	unsigned long iobase = pci_resource_start(dev, 4);

	const struct hpt_chip *chip_table;
	int clock_slot;
	int rc;

	rc = pcim_enable_device(dev);
	if (rc)
		return rc;

	switch (dev->device) {
	case PCI_DEVICE_ID_TTI_HPT366:
		
		
		if (rev < 3)
			return -ENODEV;
		
		if (rev == 6)
			return -ENODEV;

		switch (rev) {
		case 3:
			ppi[0] = &info_hpt370;
			chip_table = &hpt370;
			prefer_dpll = 0;
			break;
		case 4:
			ppi[0] = &info_hpt370a;
			chip_table = &hpt370a;
			prefer_dpll = 0;
			break;
		case 5:
			ppi[0] = &info_hpt372;
			chip_table = &hpt372;
			break;
		default:
			pr_err("Unknown HPT366 subtype, please report (%d)\n",
			       rev);
			return -ENODEV;
		}
		break;
	case PCI_DEVICE_ID_TTI_HPT372:
		
		if (rev >= 2)
			return -ENODEV;
		ppi[0] = &info_hpt372;
		chip_table = &hpt372a;
		break;
	case PCI_DEVICE_ID_TTI_HPT302:
		
		if (rev > 1)
			return -ENODEV;
		ppi[0] = &info_hpt302;
		
		chip_table = &hpt302;
		break;
	case PCI_DEVICE_ID_TTI_HPT371:
		if (rev > 1)
			return -ENODEV;
		ppi[0] = &info_hpt302;
		chip_table = &hpt371;
		pci_read_config_byte(dev, 0x50, &mcr1);
		mcr1 &= ~0x04;
		pci_write_config_byte(dev, 0x50, mcr1);
		break;
	case PCI_DEVICE_ID_TTI_HPT374:
		chip_table = &hpt374;
		if (!(PCI_FUNC(dev->devfn) & 1))
			*ppi = &info_hpt374_fn0;
		else
			*ppi = &info_hpt374_fn1;
		break;
	default:
		pr_err("PCI table is bogus, please report (%d)\n", dev->device);
		return -ENODEV;
	}
	

	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, (L1_CACHE_BYTES / 4));
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x78);
	pci_write_config_byte(dev, PCI_MIN_GNT, 0x08);
	pci_write_config_byte(dev, PCI_MAX_LAT, 0x08);

	pci_read_config_byte(dev, 0x5A, &irqmask);
	irqmask &= ~0x10;
	pci_write_config_byte(dev, 0x5a, irqmask);


	pci_write_config_byte(dev, 0x5b, 0x23);

	if (chip_table == &hpt372a)
		outb(0x0e, iobase + 0x9c);


	if (chip_table == &hpt374) {
		freq = hpt374_read_freq(dev);
		if (freq == 0)
			return -ENODEV;
	} else
		freq = inl(iobase + 0x90);

	if ((freq >> 12) != 0xABCDE) {
		int i;
		u8 sr;
		u32 total = 0;

		pr_warn("BIOS has not set timing clocks\n");

		
		for (i = 0; i < 128; i++) {
			pci_read_config_byte(dev, 0x78, &sr);
			total += sr & 0x1FF;
			udelay(15);
		}
		freq = total / 128;
	}
	freq &= 0x1FF;


	clock_slot = hpt37x_clock_slot(freq, chip_table->base);
	if (chip_table->clocks[clock_slot] == NULL || prefer_dpll) {
		unsigned int f_low, f_high;
		int dpll, adjust;

		
		dpll = (ppi[0]->udma_mask & 0xC0) ? 3 : 2;

		f_low = (MHz[clock_slot] * 48) / MHz[dpll];
		f_high = f_low + 2;
		if (clock_slot > 1)
			f_high += 2;

		
		pci_write_config_byte(dev, 0x5b, 0x21);
		pci_write_config_dword(dev, 0x5C,
				       (f_high << 16) | f_low | 0x100);

		for (adjust = 0; adjust < 8; adjust++) {
			if (hpt37x_calibrate_dpll(dev))
				break;
			if (adjust & 1)
				f_low -= adjust >> 1;
			else
				f_high += adjust >> 1;
			pci_write_config_dword(dev, 0x5C,
					       (f_high << 16) | f_low | 0x100);
		}
		if (adjust == 8) {
			pr_err("DPLL did not stabilize!\n");
			return -ENODEV;
		}
		if (dpll == 3)
			private_data = (void *)hpt37x_timings_66;
		else
			private_data = (void *)hpt37x_timings_50;

		pr_info("bus clock %dMHz, using %dMHz DPLL\n",
			MHz[clock_slot], MHz[dpll]);
	} else {
		private_data = (void *)chip_table->clocks[clock_slot];

		if (clock_slot < 2 && ppi[0] == &info_hpt370)
			ppi[0] = &info_hpt370_33;
		if (clock_slot < 2 && ppi[0] == &info_hpt370a)
			ppi[0] = &info_hpt370a_33;

		pr_info("%s using %dMHz bus clock\n",
			chip_table->name, MHz[clock_slot]);
	}

	
	return ata_pci_bmdma_init_one(dev, ppi, &hpt37x_sht, private_data, 0);
}

static const struct pci_device_id hpt37x[] = {
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT366), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT371), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT372), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT374), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT302), },

	{ },
};

static struct pci_driver hpt37x_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= hpt37x,
	.probe		= hpt37x_init_one,
	.remove		= ata_pci_remove_one
};

static int __init hpt37x_init(void)
{
	return pci_register_driver(&hpt37x_pci_driver);
}

static void __exit hpt37x_exit(void)
{
	pci_unregister_driver(&hpt37x_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for the Highpoint HPT37x/30x");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, hpt37x);
MODULE_VERSION(DRV_VERSION);

module_init(hpt37x_init);
module_exit(hpt37x_exit);
