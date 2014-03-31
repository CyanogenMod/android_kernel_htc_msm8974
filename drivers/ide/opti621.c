/*
 *  Copyright (C) 1996-1998  Linus Torvalds & authors (see below)
 */


#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ide.h>

#include <asm/io.h>

#define DRV_NAME "opti621"

#define READ_REG 0	
#define WRITE_REG 1	
#define CNTRL_REG 3	
#define STRAP_REG 5	
#define MISC_REG 6	

static int reg_base;

static DEFINE_SPINLOCK(opti621_lock);

static void write_reg(u8 value, int reg)
{
	inw(reg_base + 1);
	inw(reg_base + 1);
	outb(3, reg_base + 2);
	outb(value, reg_base + reg);
	outb(0x83, reg_base + 2);
}

static u8 read_reg(int reg)
{
	u8 ret = 0;

	inw(reg_base + 1);
	inw(reg_base + 1);
	outb(3, reg_base + 2);
	ret = inb(reg_base + reg);
	outb(0x83, reg_base + 2);

	return ret;
}

static void opti621_set_pio_mode(ide_hwif_t *hwif, ide_drive_t *drive)
{
	ide_drive_t *pair = ide_get_pair_dev(drive);
	unsigned long flags;
	unsigned long mode = drive->pio_mode, pair_mode;
	const u8 pio = mode - XFER_PIO_0;
	u8 tim, misc, addr_pio = pio, clk;

	
	static const u8 addr_timings[2][5] = {
		{ 0x20, 0x10, 0x00, 0x00, 0x00 },	
		{ 0x10, 0x10, 0x00, 0x00, 0x00 },	
	};
	static const u8 data_rec_timings[2][5] = {
		{ 0x5b, 0x45, 0x32, 0x21, 0x20 },	
		{ 0x48, 0x34, 0x21, 0x10, 0x10 }	
	};

	ide_set_drivedata(drive, (void *)mode);

	if (pair) {
		pair_mode = (unsigned long)ide_get_drivedata(pair);
		if (pair_mode && pair_mode < mode)
			addr_pio = pair_mode - XFER_PIO_0;
	}

	spin_lock_irqsave(&opti621_lock, flags);

	reg_base = hwif->io_ports.data_addr;

	
	outb(0xc0, reg_base + CNTRL_REG);
	
	outb(0xff, reg_base + 5);
	
	(void)inb(reg_base + CNTRL_REG);
	
	read_reg(CNTRL_REG);

	
	clk = read_reg(STRAP_REG) & 1;

	printk(KERN_INFO "%s: CLK = %d MHz\n", hwif->name, clk ? 25 : 33);

	tim  = data_rec_timings[clk][pio];
	misc = addr_timings[clk][addr_pio];

	
	write_reg(drive->dn & 1, MISC_REG);
	
	write_reg(tim, READ_REG);
	
	write_reg(tim, WRITE_REG);

	
	
	write_reg(0x85, CNTRL_REG);

	
	
	write_reg(misc, MISC_REG);

	spin_unlock_irqrestore(&opti621_lock, flags);
}

static const struct ide_port_ops opti621_port_ops = {
	.set_pio_mode		= opti621_set_pio_mode,
};

static const struct ide_port_info opti621_chipset __devinitdata = {
	.name		= DRV_NAME,
	.enablebits	= { {0x45, 0x80, 0x00}, {0x40, 0x08, 0x00} },
	.port_ops	= &opti621_port_ops,
	.host_flags	= IDE_HFLAG_NO_DMA,
	.pio_mask	= ATA_PIO4,
};

static int __devinit opti621_init_one(struct pci_dev *dev, const struct pci_device_id *id)
{
	return ide_pci_init_one(dev, &opti621_chipset, NULL);
}

static const struct pci_device_id opti621_pci_tbl[] = {
	{ PCI_VDEVICE(OPTI, PCI_DEVICE_ID_OPTI_82C621), 0 },
	{ PCI_VDEVICE(OPTI, PCI_DEVICE_ID_OPTI_82C825), 0 },
	{ 0, },
};
MODULE_DEVICE_TABLE(pci, opti621_pci_tbl);

static struct pci_driver opti621_pci_driver = {
	.name		= "Opti621_IDE",
	.id_table	= opti621_pci_tbl,
	.probe		= opti621_init_one,
	.remove		= ide_pci_remove,
	.suspend	= ide_pci_suspend,
	.resume		= ide_pci_resume,
};

static int __init opti621_ide_init(void)
{
	return ide_pci_register_driver(&opti621_pci_driver);
}

static void __exit opti621_ide_exit(void)
{
	pci_unregister_driver(&opti621_pci_driver);
}

module_init(opti621_ide_init);
module_exit(opti621_ide_exit);

MODULE_AUTHOR("Jaromir Koutek, Jan Harkes, Mark Lord");
MODULE_DESCRIPTION("PCI driver module for Opti621 IDE");
MODULE_LICENSE("GPL");
