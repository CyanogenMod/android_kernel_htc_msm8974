
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>

#define DRV_NAME "pata_mpiix"
#define DRV_VERSION "0.7.7"

enum {
	IDETIM = 0x6C,		
	IORDY = (1 << 1),
	PPE = (1 << 2),
	FTIM = (1 << 0),
	ENABLED = (1 << 15),
	SECONDARY = (1 << 14)
};

static int mpiix_pre_reset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	static const struct pci_bits mpiix_enable_bits = { 0x6D, 1, 0x80, 0x80 };

	if (!pci_test_config_bits(pdev, &mpiix_enable_bits))
		return -ENOENT;

	return ata_sff_prereset(link, deadline);
}


static void mpiix_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	int control = 0;
	int pio = adev->pio_mode - XFER_PIO_0;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u16 idetim;
	static const	 
	u8 timings[][2]	= { { 0, 0 },
			    { 0, 0 },
			    { 1, 0 },
			    { 2, 1 },
			    { 2, 3 }, };

	pci_read_config_word(pdev, IDETIM, &idetim);

	
	if (adev->class == ATA_DEV_ATA)
		control |= PPE;		
	if (ata_pio_need_iordy(adev))
		control |= IORDY;
	if (pio > 1)
		control |= FTIM;	

	
	idetim &= 0xCCEE;
	idetim &= ~(0x07  << (4 * adev->devno));
	idetim |= control << (4 * adev->devno);

	idetim |= (timings[pio][0] << 12) | (timings[pio][1] << 8);
	pci_write_config_word(pdev, IDETIM, idetim);

	ap->private_data = adev;
}


static unsigned int mpiix_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_device *adev = qc->dev;


	if (adev->pio_mode && adev != ap->private_data)
		mpiix_set_piomode(ap, adev);

	return ata_sff_qc_issue(qc);
}

static struct scsi_host_template mpiix_sht = {
	ATA_PIO_SHT(DRV_NAME),
};

static struct ata_port_operations mpiix_port_ops = {
	.inherits	= &ata_sff_port_ops,
	.qc_issue	= mpiix_qc_issue,
	.cable_detect	= ata_cable_40wire,
	.set_piomode	= mpiix_set_piomode,
	.prereset	= mpiix_pre_reset,
	.sff_data_xfer	= ata_sff_data_xfer32,
};

static int mpiix_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	
	struct ata_host *host;
	struct ata_port *ap;
	void __iomem *cmd_addr, *ctl_addr;
	u16 idetim;
	int cmd, ctl, irq;

	ata_print_version_once(&dev->dev, DRV_VERSION);

	host = ata_host_alloc(&dev->dev, 1);
	if (!host)
		return -ENOMEM;
	ap = host->ports[0];


	pci_read_config_word(dev, IDETIM, &idetim);
	if (!(idetim & ENABLED))
		return -ENODEV;

	
	if (!(idetim & SECONDARY)) {
		cmd = 0x1F0;
		ctl = 0x3F6;
		irq = 14;
	} else {
		cmd = 0x170;
		ctl = 0x376;
		irq = 15;
	}

	cmd_addr = devm_ioport_map(&dev->dev, cmd, 8);
	ctl_addr = devm_ioport_map(&dev->dev, ctl, 1);
	if (!cmd_addr || !ctl_addr)
		return -ENOMEM;

	ata_port_desc(ap, "cmd 0x%x ctl 0x%x", cmd, ctl);


	ap->ops = &mpiix_port_ops;
	ap->pio_mask = ATA_PIO4;
	ap->flags |= ATA_FLAG_SLAVE_POSS;

	ap->ioaddr.cmd_addr = cmd_addr;
	ap->ioaddr.ctl_addr = ctl_addr;
	ap->ioaddr.altstatus_addr = ctl_addr;

	
	ata_sff_std_ports(&ap->ioaddr);

	
	return ata_host_activate(host, irq, ata_sff_interrupt, IRQF_SHARED,
				 &mpiix_sht);
}

static const struct pci_device_id mpiix[] = {
	{ PCI_VDEVICE(INTEL, PCI_DEVICE_ID_INTEL_82371MX), },

	{ },
};

static struct pci_driver mpiix_pci_driver = {
	.name 		= DRV_NAME,
	.id_table	= mpiix,
	.probe 		= mpiix_init_one,
	.remove		= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend	= ata_pci_device_suspend,
	.resume		= ata_pci_device_resume,
#endif
};

static int __init mpiix_init(void)
{
	return pci_register_driver(&mpiix_pci_driver);
}

static void __exit mpiix_exit(void)
{
	pci_unregister_driver(&mpiix_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for Intel MPIIX");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, mpiix);
MODULE_VERSION(DRV_VERSION);

module_init(mpiix_init);
module_exit(mpiix_exit);
