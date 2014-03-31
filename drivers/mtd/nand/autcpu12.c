/*
 *  drivers/mtd/autcpu12.c
 *
 *  Copyright (c) 2002 Thomas Gleixner <tgxl@linutronix.de>
 *
 *  Derived from drivers/mtd/spia.c
 *	 Copyright (C) 2000 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   autronix autcpu12 board, which is a SmartMediaCard. It supports
 *   16MiB, 32MiB and 64MiB cards.
 *
 *
 *	02-12-2002 TG	Cleanup of module params
 *
 *	02-20-2002 TG	adjusted for different rd/wr address support
 *			added support for read device ready/busy line
 *			added page_cache
 *
 *	10-06-2002 TG	128K card support added
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <asm/sizes.h>
#include <mach/autcpu12.h>

static struct mtd_info *autcpu12_mtd = NULL;
static void __iomem *autcpu12_fio_base;

static struct mtd_partition partition_info16k[] = {
	{ .name		= "AUTCPU12 flash partition 1",
	  .offset	= 0,
	  .size		= 8 * SZ_1M },
	{ .name		= "AUTCPU12 flash partition 2",
	  .offset	= 8 * SZ_1M,
	  .size		= 8 * SZ_1M },
};

static struct mtd_partition partition_info32k[] = {
	{ .name		= "AUTCPU12 flash partition 1",
	  .offset	= 0,
	  .size		= 8 * SZ_1M },
	{ .name		= "AUTCPU12 flash partition 2",
	  .offset	= 8 * SZ_1M,
	  .size		= 24 * SZ_1M },
};

static struct mtd_partition partition_info64k[] = {
	{ .name		= "AUTCPU12 flash partition 1",
	  .offset	= 0,
	  .size		= 16 * SZ_1M },
	{ .name		= "AUTCPU12 flash partition 2",
	  .offset	= 16 * SZ_1M,
	  .size		= 48 * SZ_1M },
};

static struct mtd_partition partition_info128k[] = {
	{ .name		= "AUTCPU12 flash partition 1",
	  .offset	= 0,
	  .size		= 16 * SZ_1M },
	{ .name		= "AUTCPU12 flash partition 2",
	  .offset	= 16 * SZ_1M,
	  .size		= 112 * SZ_1M },
};

#define NUM_PARTITIONS16K 2
#define NUM_PARTITIONS32K 2
#define NUM_PARTITIONS64K 2
#define NUM_PARTITIONS128K 2
static void autcpu12_hwcontrol(struct mtd_info *mtd, int cmd,
			       unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;

	if (ctrl & NAND_CTRL_CHANGE) {
		void __iomem *addr;
		unsigned char bits;

		addr = CS89712_VIRT_BASE + AUTCPU12_SMC_PORT_OFFSET;
		bits = (ctrl & NAND_CLE) << 4;
		bits |= (ctrl & NAND_ALE) << 2;
		writeb((readb(addr) & ~0x30) | bits, addr);

		addr = autcpu12_fio_base + AUTCPU12_SMC_SELECT_OFFSET;
		writeb((readb(addr) & ~0x1) | (ctrl & NAND_NCE), addr);
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, chip->IO_ADDR_W);
}

int autcpu12_device_ready(struct mtd_info *mtd)
{
	void __iomem *addr = CS89712_VIRT_BASE + AUTCPU12_SMC_PORT_OFFSET;

	return readb(addr) & AUTCPU12_SMC_RDY;
}

static int __init autcpu12_init(void)
{
	struct nand_chip *this;
	int err = 0;

	
	autcpu12_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip),
			       GFP_KERNEL);
	if (!autcpu12_mtd) {
		printk("Unable to allocate AUTCPU12 NAND MTD device structure.\n");
		err = -ENOMEM;
		goto out;
	}

	
	autcpu12_fio_base = ioremap(AUTCPU12_PHYS_SMC, SZ_1K);
	if (!autcpu12_fio_base) {
		printk("Ioremap autcpu12 SmartMedia Card failed\n");
		err = -EIO;
		goto out_mtd;
	}

	
	this = (struct nand_chip *)(&autcpu12_mtd[1]);

	
	memset(autcpu12_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	
	autcpu12_mtd->priv = this;
	autcpu12_mtd->owner = THIS_MODULE;

	
	this->IO_ADDR_R = autcpu12_fio_base;
	this->IO_ADDR_W = autcpu12_fio_base;
	this->cmd_ctrl = autcpu12_hwcontrol;
	this->dev_ready = autcpu12_device_ready;
	
	this->chip_delay = 20;
	this->ecc.mode = NAND_ECC_SOFT;

	
	this->bbt_options = NAND_BBT_USE_FLASH;

	
	if (nand_scan(autcpu12_mtd, 1)) {
		err = -ENXIO;
		goto out_ior;
	}

	
	switch (autcpu12_mtd->size) {
		case SZ_16M:
			mtd_device_register(autcpu12_mtd, partition_info16k,
					    NUM_PARTITIONS16K);
			break;
		case SZ_32M:
			mtd_device_register(autcpu12_mtd, partition_info32k,
					    NUM_PARTITIONS32K);
			break;
		case SZ_64M:
			mtd_device_register(autcpu12_mtd, partition_info64k,
					    NUM_PARTITIONS64K);
			break;
		case SZ_128M:
			mtd_device_register(autcpu12_mtd, partition_info128k,
					    NUM_PARTITIONS128K);
			break;
		default:
			printk("Unsupported SmartMedia device\n");
			err = -ENXIO;
			goto out_ior;
	}
	goto out;

 out_ior:
	iounmap(autcpu12_fio_base);
 out_mtd:
	kfree(autcpu12_mtd);
 out:
	return err;
}

module_init(autcpu12_init);

static void __exit autcpu12_cleanup(void)
{
	
	nand_release(autcpu12_mtd);

	
	iounmap(autcpu12_fio_base);

	
	kfree(autcpu12_mtd);
}

module_exit(autcpu12_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thomas Gleixner <tglx@linutronix.de>");
MODULE_DESCRIPTION("Glue layer for SmartMediaCard on autronix autcpu12");
