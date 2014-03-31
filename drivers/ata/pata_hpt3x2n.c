/*
 * Libata driver for the HighPoint 371N, 372N, and 302N UDMA66 ATA controllers.
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
 *
 * TODO
 *	Work out best PLL policy
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

#define DRV_NAME	"pata_hpt3x2n"
#define DRV_VERSION	"0.3.15"

enum {
	HPT_PCI_FAST	=	(1 << 31),
	PCI66		=	(1 << 1),
	USE_DPLL	=	(1 << 0)
};

struct hpt_clock {
	u8	xfer_speed;
	u32	timing;
};

struct hpt_chip {
	const char *name;
	struct hpt_clock *clocks[3];
};



static struct hpt_clock hpt3x2n_clocks[] = {
	{	XFER_UDMA_7,	0x1c869c62	},
	{	XFER_UDMA_6,	0x1c869c62	},
	{	XFER_UDMA_5,	0x1c8a9c62	},
	{	XFER_UDMA_4,	0x1c8a9c62	},
	{	XFER_UDMA_3,	0x1c8e9c62	},
	{	XFER_UDMA_2,	0x1c929c62	},
	{	XFER_UDMA_1,	0x1c9a9c62	},
	{	XFER_UDMA_0,	0x1c829c62	},

	{	XFER_MW_DMA_2,	0x2c829c62	},
	{	XFER_MW_DMA_1,	0x2c829c66	},
	{	XFER_MW_DMA_0,	0x2c829d2e	},

	{	XFER_PIO_4,	0x0c829c62	},
	{	XFER_PIO_3,	0x0c829c84	},
	{	XFER_PIO_2,	0x0c829ca6	},
	{	XFER_PIO_1,	0x0d029d26	},
	{	XFER_PIO_0,	0x0d029d5e	},
};


static u32 hpt3x2n_find_mode(struct ata_port *ap, int speed)
{
	struct hpt_clock *clocks = hpt3x2n_clocks;

	while (clocks->xfer_speed) {
		if (clocks->xfer_speed == speed)
			return clocks->timing;
		clocks++;
	}
	BUG();
	return 0xffffffffU;	
}

static unsigned long hpt372n_filter(struct ata_device *adev, unsigned long mask)
{
	if (ata_id_is_sata(adev->id))
		mask &= ~((0xE << ATA_SHIFT_UDMA) | ATA_MASK_MWDMA);

	return mask;
}


static int hpt3x2n_cable_detect(struct ata_port *ap)
{
	u8 scr2, ata66;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);

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


static int hpt3x2n_pre_reset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);

	
	pci_write_config_byte(pdev, 0x50 + 4 * ap->port_no, 0x37);
	udelay(100);

	return ata_sff_prereset(link, deadline);
}

static void hpt3x2n_set_mode(struct ata_port *ap, struct ata_device *adev,
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

	timing = hpt3x2n_find_mode(ap, mode);

	pci_read_config_dword(pdev, addr1, &reg);
	reg = (reg & ~mask) | (timing & mask);
	pci_write_config_dword(pdev, addr1, reg);
}


static void hpt3x2n_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	hpt3x2n_set_mode(ap, adev, adev->pio_mode);
}


static void hpt3x2n_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	hpt3x2n_set_mode(ap, adev, adev->dma_mode);
}


static void hpt3x2n_bmdma_stop(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int mscreg = 0x50 + 2 * ap->port_no;
	u8 bwsr_stat, msc_stat;

	pci_read_config_byte(pdev, 0x6A, &bwsr_stat);
	pci_read_config_byte(pdev, mscreg, &msc_stat);
	if (bwsr_stat & (1 << ap->port_no))
		pci_write_config_byte(pdev, mscreg, msc_stat | 0x30);
	ata_bmdma_stop(qc);
}


static void hpt3x2n_set_clock(struct ata_port *ap, int source)
{
	void __iomem *bmdma = ap->ioaddr.bmdma_addr - ap->port_no * 8;

	
	iowrite8(0x80, bmdma+0x73);
	iowrite8(0x80, bmdma+0x77);

	
	iowrite8(source, bmdma+0x7B);
	iowrite8(0xC0, bmdma+0x79);

	
	iowrite8(ioread8(bmdma+0x70) | 0x32, bmdma+0x70);
	iowrite8(ioread8(bmdma+0x74) | 0x32, bmdma+0x74);

	
	iowrite8(0x00, bmdma+0x79);

	
	iowrite8(0x00, bmdma+0x73);
	iowrite8(0x00, bmdma+0x77);
}

static int hpt3x2n_use_dpll(struct ata_port *ap, int writing)
{
	long flags = (long)ap->host->private_data;

	
	if (writing)
		return USE_DPLL;	
	if (flags & PCI66)
		return USE_DPLL;	
	return 0;
}

static int hpt3x2n_qc_defer(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_port *alt = ap->host->ports[ap->port_no ^ 1];
	int rc, flags = (long)ap->host->private_data;
	int dpll = hpt3x2n_use_dpll(ap, qc->tf.flags & ATA_TFLAG_WRITE);

	
	rc = ata_std_qc_defer(qc);
	if (rc != 0)
		return rc;

	if ((flags & USE_DPLL) != dpll && alt->qc_active)
		return ATA_DEFER_PORT;
	return 0;
}

static unsigned int hpt3x2n_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	int flags = (long)ap->host->private_data;
	int dpll = hpt3x2n_use_dpll(ap, qc->tf.flags & ATA_TFLAG_WRITE);

	if ((flags & USE_DPLL) != dpll) {
		flags &= ~USE_DPLL;
		flags |= dpll;
		ap->host->private_data = (void *)(long)flags;

		hpt3x2n_set_clock(ap, dpll ? 0x21 : 0x23);
	}
	return ata_bmdma_qc_issue(qc);
}

static struct scsi_host_template hpt3x2n_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};


static struct ata_port_operations hpt3xxn_port_ops = {
	.inherits	= &ata_bmdma_port_ops,

	.bmdma_stop	= hpt3x2n_bmdma_stop,

	.qc_defer	= hpt3x2n_qc_defer,
	.qc_issue	= hpt3x2n_qc_issue,

	.cable_detect	= hpt3x2n_cable_detect,
	.set_piomode	= hpt3x2n_set_piomode,
	.set_dmamode	= hpt3x2n_set_dmamode,
	.prereset	= hpt3x2n_pre_reset,
};


static struct ata_port_operations hpt372n_port_ops = {
	.inherits	= &hpt3xxn_port_ops,
	.mode_filter	= &hpt372n_filter,
};


static int hpt3xn_calibrate_dpll(struct pci_dev *dev)
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

static int hpt3x2n_pci_clock(struct pci_dev *pdev)
{
	unsigned long freq;
	u32 fcnt;
	unsigned long iobase = pci_resource_start(pdev, 4);

	fcnt = inl(iobase + 0x90);	
	if ((fcnt >> 12) != 0xABCDE) {
		int i;
		u16 sr;
		u32 total = 0;

		pr_warn("BIOS clock data not set\n");

		
		for (i = 0; i < 128; i++) {
			pci_read_config_word(pdev, 0x78, &sr);
			total += sr & 0x1FF;
			udelay(15);
		}
		fcnt = total / 128;
	}
	fcnt &= 0x1FF;

	freq = (fcnt * 77) / 192;

	
	if (freq < 40)
		return 33;
	if (freq < 45)
		return 40;
	if (freq < 55)
		return 50;
	return 66;
}


static int hpt3x2n_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	
	static const struct ata_port_info info_hpt372n = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA6,
		.port_ops = &hpt372n_port_ops
	};
	
	static const struct ata_port_info info_hpt3xxn = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.udma_mask = ATA_UDMA6,
		.port_ops = &hpt3xxn_port_ops
	};
	const struct ata_port_info *ppi[] = { &info_hpt3xxn, NULL };
	u8 rev = dev->revision;
	u8 irqmask;
	unsigned int pci_mhz;
	unsigned int f_low, f_high;
	int adjust;
	unsigned long iobase = pci_resource_start(dev, 4);
	void *hpriv = (void *)USE_DPLL;
	int rc;

	rc = pcim_enable_device(dev);
	if (rc)
		return rc;

	switch (dev->device) {
	case PCI_DEVICE_ID_TTI_HPT366:
		
		if (rev < 6)
			return -ENODEV;
		goto hpt372n;
	case PCI_DEVICE_ID_TTI_HPT371:
		
		if (rev < 2)
			return -ENODEV;
		break;
	case PCI_DEVICE_ID_TTI_HPT372:
		
		if (rev < 2)
			return -ENODEV;
		goto hpt372n;
	case PCI_DEVICE_ID_TTI_HPT302:
		
		if (rev < 2)
			return -ENODEV;
		break;
	case PCI_DEVICE_ID_TTI_HPT372N:
hpt372n:
		ppi[0] = &info_hpt372n;
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

	if (dev->device == PCI_DEVICE_ID_TTI_HPT371) {
		u8 mcr1;
		pci_read_config_byte(dev, 0x50, &mcr1);
		mcr1 &= ~0x04;
		pci_write_config_byte(dev, 0x50, mcr1);
	}


	pci_mhz = hpt3x2n_pci_clock(dev);

	f_low = (pci_mhz * 48) / 66;	
	f_high = f_low + 2;		

	pci_write_config_dword(dev, 0x5C, (f_high << 16) | f_low | 0x100);
	
	pci_write_config_byte(dev, 0x5B, 0x21);

	
	for (adjust = 0; adjust < 8; adjust++) {
		if (hpt3xn_calibrate_dpll(dev))
			break;
		pci_write_config_dword(dev, 0x5C, (f_high << 16) | f_low);
	}
	if (adjust == 8) {
		pr_err("DPLL did not stabilize!\n");
		return -ENODEV;
	}

	pr_info("bus clock %dMHz, using 66MHz DPLL\n", pci_mhz);

	if (pci_mhz > 60)
		hpriv = (void *)(PCI66 | USE_DPLL);

	if (dev->device == PCI_DEVICE_ID_TTI_HPT371)
		outb(inb(iobase + 0x9c) | 0x04, iobase + 0x9c);

	
	return ata_pci_bmdma_init_one(dev, ppi, &hpt3x2n_sht, hpriv, 0);
}

static const struct pci_device_id hpt3x2n[] = {
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT366), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT371), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT372), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT302), },
	{ PCI_VDEVICE(TTI, PCI_DEVICE_ID_TTI_HPT372N), },

	{ },
};

static struct pci_driver hpt3x2n_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= hpt3x2n,
	.probe		= hpt3x2n_init_one,
	.remove		= ata_pci_remove_one
};

static int __init hpt3x2n_init(void)
{
	return pci_register_driver(&hpt3x2n_pci_driver);
}

static void __exit hpt3x2n_exit(void)
{
	pci_unregister_driver(&hpt3x2n_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for the Highpoint HPT3xxN");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, hpt3x2n);
MODULE_VERSION(DRV_VERSION);

module_init(hpt3x2n_init);
module_exit(hpt3x2n_exit);
