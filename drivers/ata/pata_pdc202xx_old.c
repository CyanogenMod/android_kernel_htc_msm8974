
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>

#define DRV_NAME "pata_pdc202xx_old"
#define DRV_VERSION "0.4.3"

static int pdc2026x_cable_detect(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u16 cis;

	pci_read_config_word(pdev, 0x50, &cis);
	if (cis & (1 << (10 + ap->port_no)))
		return ATA_CBL_PATA40;
	return ATA_CBL_PATA80;
}

static void pdc202xx_exec_command(struct ata_port *ap,
				  const struct ata_taskfile *tf)
{
	DPRINTK("ata%u: cmd 0x%X\n", ap->print_id, tf->command);

	iowrite8(tf->command, ap->ioaddr.command_addr);
	ndelay(400);
}

static bool pdc202xx_irq_check(struct ata_port *ap)
{
	struct pci_dev *pdev	= to_pci_dev(ap->host->dev);
	unsigned long master	= pci_resource_start(pdev, 4);
	u8 sc1d			= inb(master + 0x1d);

	if (ap->port_no) {
		return sc1d & 0x40;
	} else	{
		return sc1d & 0x04;
	}
}


static void pdc202xx_configure_piomode(struct ata_port *ap, struct ata_device *adev, int pio)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int port = 0x60 + 8 * ap->port_no + 4 * adev->devno;
	static u16 pio_timing[5] = {
		0x0913, 0x050C , 0x0308, 0x0206, 0x0104
	};
	u8 r_ap, r_bp;

	pci_read_config_byte(pdev, port, &r_ap);
	pci_read_config_byte(pdev, port + 1, &r_bp);
	r_ap &= ~0x3F;	
	r_bp &= ~0x1F;
	r_ap |= (pio_timing[pio] >> 8);
	r_bp |= (pio_timing[pio] & 0xFF);

	if (ata_pio_need_iordy(adev))
		r_ap |= 0x20;	
	if (adev->class == ATA_DEV_ATA)
		r_ap |= 0x10;	
	pci_write_config_byte(pdev, port, r_ap);
	pci_write_config_byte(pdev, port + 1, r_bp);
}


static void pdc202xx_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	pdc202xx_configure_piomode(ap, adev, adev->pio_mode - XFER_PIO_0);
}


static void pdc202xx_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	int port = 0x60 + 8 * ap->port_no + 4 * adev->devno;
	static u8 udma_timing[6][2] = {
		{ 0x60, 0x03 },	
		{ 0x40, 0x02 },
		{ 0x20, 0x01 },
		{ 0x40, 0x02 },	
		{ 0x20, 0x01 },
		{ 0x20, 0x01 }
	};
	static u8 mdma_timing[3][2] = {
		{ 0xe0, 0x0f },
		{ 0x60, 0x04 },
		{ 0x60, 0x03 },
	};
	u8 r_bp, r_cp;

	pci_read_config_byte(pdev, port + 1, &r_bp);
	pci_read_config_byte(pdev, port + 2, &r_cp);

	r_bp &= ~0xE0;
	r_cp &= ~0x0F;

	if (adev->dma_mode >= XFER_UDMA_0) {
		int speed = adev->dma_mode - XFER_UDMA_0;
		r_bp |= udma_timing[speed][0];
		r_cp |= udma_timing[speed][1];

	} else {
		int speed = adev->dma_mode - XFER_MW_DMA_0;
		r_bp |= mdma_timing[speed][0];
		r_cp |= mdma_timing[speed][1];
	}
	pci_write_config_byte(pdev, port + 1, r_bp);
	pci_write_config_byte(pdev, port + 2, r_cp);

}


static void pdc2026x_bmdma_start(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_device *adev = qc->dev;
	struct ata_taskfile *tf = &qc->tf;
	int sel66 = ap->port_no ? 0x08: 0x02;

	void __iomem *master = ap->host->ports[0]->ioaddr.bmdma_addr;
	void __iomem *clock = master + 0x11;
	void __iomem *atapi_reg = master + 0x20 + (4 * ap->port_no);

	u32 len;

	
	if (adev->dma_mode > XFER_UDMA_2)
		iowrite8(ioread8(clock) | sel66, clock);
	else
		iowrite8(ioread8(clock) & ~sel66, clock);

	pdc202xx_set_dmamode(ap, qc->dev);

	
	if ((tf->flags & ATA_TFLAG_LBA48) ||  tf->protocol == ATAPI_PROT_DMA) {
		len = qc->nbytes / 2;

		if (tf->flags & ATA_TFLAG_WRITE)
			len |= 0x06000000;
		else
			len |= 0x05000000;

		iowrite32(len, atapi_reg);
	}

	
	ata_bmdma_start(qc);
}


static void pdc2026x_bmdma_stop(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_device *adev = qc->dev;
	struct ata_taskfile *tf = &qc->tf;

	int sel66 = ap->port_no ? 0x08: 0x02;
	
	void __iomem *master = ap->host->ports[0]->ioaddr.bmdma_addr;
	void __iomem *clock = master + 0x11;
	void __iomem *atapi_reg = master + 0x20 + (4 * ap->port_no);

	
	if (tf->protocol == ATAPI_PROT_DMA || (tf->flags & ATA_TFLAG_LBA48)) {
		iowrite32(0, atapi_reg);
		iowrite8(ioread8(clock) & ~sel66, clock);
	}
	
	if (adev->dma_mode > XFER_UDMA_2)
		iowrite8(ioread8(clock) & ~sel66, clock);
	ata_bmdma_stop(qc);
	pdc202xx_set_piomode(ap, adev);
}


static void pdc2026x_dev_config(struct ata_device *adev)
{
	adev->max_sectors = 256;
}

static int pdc2026x_port_start(struct ata_port *ap)
{
	void __iomem *bmdma = ap->ioaddr.bmdma_addr;
	if (bmdma) {
		
		u8 burst = ioread8(bmdma + 0x1f);
		iowrite8(burst | 0x01, bmdma + 0x1f);
	}
	return ata_bmdma_port_start(ap);
}


static int pdc2026x_check_atapi_dma(struct ata_queued_cmd *qc)
{
	return 1;
}

static struct scsi_host_template pdc202xx_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations pdc2024x_port_ops = {
	.inherits		= &ata_bmdma_port_ops,

	.cable_detect		= ata_cable_40wire,
	.set_piomode		= pdc202xx_set_piomode,
	.set_dmamode		= pdc202xx_set_dmamode,

	.sff_exec_command	= pdc202xx_exec_command,
	.sff_irq_check		= pdc202xx_irq_check,
};

static struct ata_port_operations pdc2026x_port_ops = {
	.inherits		= &pdc2024x_port_ops,

	.check_atapi_dma	= pdc2026x_check_atapi_dma,
	.bmdma_start		= pdc2026x_bmdma_start,
	.bmdma_stop		= pdc2026x_bmdma_stop,

	.cable_detect		= pdc2026x_cable_detect,
	.dev_config		= pdc2026x_dev_config,

	.port_start		= pdc2026x_port_start,

	.sff_exec_command	= pdc202xx_exec_command,
	.sff_irq_check		= pdc202xx_irq_check,
};

static int pdc202xx_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	static const struct ata_port_info info[3] = {
		{
			.flags = ATA_FLAG_SLAVE_POSS,
			.pio_mask = ATA_PIO4,
			.mwdma_mask = ATA_MWDMA2,
			.udma_mask = ATA_UDMA2,
			.port_ops = &pdc2024x_port_ops
		},
		{
			.flags = ATA_FLAG_SLAVE_POSS,
			.pio_mask = ATA_PIO4,
			.mwdma_mask = ATA_MWDMA2,
			.udma_mask = ATA_UDMA4,
			.port_ops = &pdc2026x_port_ops
		},
		{
			.flags = ATA_FLAG_SLAVE_POSS,
			.pio_mask = ATA_PIO4,
			.mwdma_mask = ATA_MWDMA2,
			.udma_mask = ATA_UDMA5,
			.port_ops = &pdc2026x_port_ops
		}

	};
	const struct ata_port_info *ppi[] = { &info[id->driver_data], NULL };

	if (dev->device == PCI_DEVICE_ID_PROMISE_20265) {
		struct pci_dev *bridge = dev->bus->self;
		
		if (bridge && bridge->vendor == PCI_VENDOR_ID_INTEL) {
			if (bridge->device == PCI_DEVICE_ID_INTEL_I960)
				return -ENODEV;
			if (bridge->device == PCI_DEVICE_ID_INTEL_I960RM)
				return -ENODEV;
		}
	}
	return ata_pci_bmdma_init_one(dev, ppi, &pdc202xx_sht, NULL, 0);
}

static const struct pci_device_id pdc202xx[] = {
	{ PCI_VDEVICE(PROMISE, PCI_DEVICE_ID_PROMISE_20246), 0 },
	{ PCI_VDEVICE(PROMISE, PCI_DEVICE_ID_PROMISE_20262), 1 },
	{ PCI_VDEVICE(PROMISE, PCI_DEVICE_ID_PROMISE_20263), 1 },
	{ PCI_VDEVICE(PROMISE, PCI_DEVICE_ID_PROMISE_20265), 2 },
	{ PCI_VDEVICE(PROMISE, PCI_DEVICE_ID_PROMISE_20267), 2 },

	{ },
};

static struct pci_driver pdc202xx_pci_driver = {
	.name 		= DRV_NAME,
	.id_table	= pdc202xx,
	.probe 		= pdc202xx_init_one,
	.remove		= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend	= ata_pci_device_suspend,
	.resume		= ata_pci_device_resume,
#endif
};

static int __init pdc202xx_init(void)
{
	return pci_register_driver(&pdc202xx_pci_driver);
}

static void __exit pdc202xx_exit(void)
{
	pci_unregister_driver(&pdc202xx_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for Promise 2024x and 20262-20267");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, pdc202xx);
MODULE_VERSION(DRV_VERSION);

module_init(pdc202xx_init);
module_exit(pdc202xx_exit);
