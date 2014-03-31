
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>

#define DRV_NAME "pata_sl82c105"
#define DRV_VERSION "0.3.3"

enum {
	CTRL_IDE_IRQB	=	(1 << 30),
	CTRL_IDE_IRQA   =	(1 << 28),
	CTRL_LEGIRQ     =	(1 << 11),
	CTRL_P1F16      =	(1 << 5),
	CTRL_P1EN       =	(1 << 4),
	CTRL_P0F16      =	(1 << 1),
	CTRL_P0EN       =	(1 << 0)
};


static int sl82c105_pre_reset(struct ata_link *link, unsigned long deadline)
{
	static const struct pci_bits sl82c105_enable_bits[] = {
		{ 0x40, 1, 0x01, 0x01 },
		{ 0x40, 1, 0x10, 0x10 }
	};
	struct ata_port *ap = link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);

	if (ap->port_no && !pci_test_config_bits(pdev, &sl82c105_enable_bits[ap->port_no]))
		return -ENOENT;
	return ata_sff_prereset(link, deadline);
}



static void sl82c105_configure_piomode(struct ata_port *ap, struct ata_device *adev, int pio)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	static u16 pio_timing[5] = {
		0x50D, 0x407, 0x304, 0x242, 0x240
	};
	u16 dummy;
	int timing = 0x44 + (8 * ap->port_no) + (4 * adev->devno);

	pci_write_config_word(pdev, timing, pio_timing[pio]);
	
	pci_read_config_word(pdev, timing, &dummy);
}


static void sl82c105_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	sl82c105_configure_piomode(ap, adev, adev->pio_mode - XFER_PIO_0);
}


static void sl82c105_configure_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	static u16 dma_timing[3] = {
		0x707, 0x201, 0x200
	};
	u16 dummy;
	int timing = 0x44 + (8 * ap->port_no) + (4 * adev->devno);
	int dma = adev->dma_mode - XFER_MW_DMA_0;

	pci_write_config_word(pdev, timing, dma_timing[dma]);
	
	pci_read_config_word(pdev, timing, &dummy);
}


static void sl82c105_reset_engine(struct ata_port *ap)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u16 val;

	pci_read_config_word(pdev, 0x7E, &val);
	pci_write_config_word(pdev, 0x7E, val | 4);
	pci_write_config_word(pdev, 0x7E, val & ~4);
}


static void sl82c105_bmdma_start(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;

	udelay(100);
	sl82c105_reset_engine(ap);
	udelay(100);

	
	sl82c105_configure_dmamode(ap, qc->dev);
	
	ata_bmdma_start(qc);
}


static void sl82c105_bmdma_stop(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;

	ata_bmdma_stop(qc);
	sl82c105_reset_engine(ap);
	udelay(100);

	sl82c105_set_piomode(ap, qc->dev);
}


static int sl82c105_qc_defer(struct ata_queued_cmd *qc)
{
	struct ata_host *host = qc->ap->host;
	struct ata_port *alt = host->ports[1 ^ qc->ap->port_no];
	int rc;

	
	rc = ata_std_qc_defer(qc);
	if (rc != 0)
		return rc;

	if (alt && alt->qc_active)
		return	ATA_DEFER_PORT;
	return 0;
}

static bool sl82c105_sff_irq_check(struct ata_port *ap)
{
	struct pci_dev *pdev	= to_pci_dev(ap->host->dev);
	u32 val, mask		= ap->port_no ? CTRL_IDE_IRQB : CTRL_IDE_IRQA;

	pci_read_config_dword(pdev, 0x40, &val);

	return val & mask;
}

static struct scsi_host_template sl82c105_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations sl82c105_port_ops = {
	.inherits	= &ata_bmdma_port_ops,
	.qc_defer	= sl82c105_qc_defer,
	.bmdma_start 	= sl82c105_bmdma_start,
	.bmdma_stop	= sl82c105_bmdma_stop,
	.cable_detect	= ata_cable_40wire,
	.set_piomode	= sl82c105_set_piomode,
	.prereset	= sl82c105_pre_reset,
	.sff_irq_check	= sl82c105_sff_irq_check,
};


static int sl82c105_bridge_revision(struct pci_dev *pdev)
{
	struct pci_dev *bridge;

	bridge = pci_get_slot(pdev->bus,
			       PCI_DEVFN(PCI_SLOT(pdev->devfn), 0));
	if (!bridge)
		return -1;

	if (bridge->vendor != PCI_VENDOR_ID_WINBOND ||
	    bridge->device != PCI_DEVICE_ID_WINBOND_83C553 ||
	    bridge->class >> 8 != PCI_CLASS_BRIDGE_ISA) {
	    	pci_dev_put(bridge);
		return -1;
	}
	pci_dev_put(bridge);
	return bridge->revision;
}

static void sl82c105_fixup(struct pci_dev *pdev)
{
	u32 val;

	pci_read_config_dword(pdev, 0x40, &val);
	val |= CTRL_P0EN | CTRL_P0F16 | CTRL_P1F16;
	pci_write_config_dword(pdev, 0x40, val);
}

static int sl82c105_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	static const struct ata_port_info info_dma = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.mwdma_mask = ATA_MWDMA2,
		.port_ops = &sl82c105_port_ops
	};
	static const struct ata_port_info info_early = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = ATA_PIO4,
		.port_ops = &sl82c105_port_ops
	};
	
	const struct ata_port_info *ppi[] = { &info_early,
					       NULL };
	int rev;
	int rc;

	rc = pcim_enable_device(dev);
	if (rc)
		return rc;

	rev = sl82c105_bridge_revision(dev);

	if (rev == -1)
		dev_warn(&dev->dev,
			 "pata_sl82c105: Unable to find bridge, disabling DMA\n");
	else if (rev <= 5)
		dev_warn(&dev->dev,
			 "pata_sl82c105: Early bridge revision, no DMA available\n");
	else
		ppi[0] = &info_dma;

	sl82c105_fixup(dev);

	return ata_pci_bmdma_init_one(dev, ppi, &sl82c105_sht, NULL, 0);
}

#ifdef CONFIG_PM
static int sl82c105_reinit_one(struct pci_dev *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	int rc;

	rc = ata_pci_device_do_resume(pdev);
	if (rc)
		return rc;

	sl82c105_fixup(pdev);

	ata_host_resume(host);
	return 0;
}
#endif

static const struct pci_device_id sl82c105[] = {
	{ PCI_VDEVICE(WINBOND, PCI_DEVICE_ID_WINBOND_82C105), },

	{ },
};

static struct pci_driver sl82c105_pci_driver = {
	.name 		= DRV_NAME,
	.id_table	= sl82c105,
	.probe 		= sl82c105_init_one,
	.remove		= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend	= ata_pci_device_suspend,
	.resume		= sl82c105_reinit_one,
#endif
};

static int __init sl82c105_init(void)
{
	return pci_register_driver(&sl82c105_pci_driver);
}

static void __exit sl82c105_exit(void)
{
	pci_unregister_driver(&sl82c105_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for Sl82c105");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, sl82c105);
MODULE_VERSION(DRV_VERSION);

module_init(sl82c105_init);
module_exit(sl82c105_exit);
