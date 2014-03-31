/*
 *  drivers/mtd/nand/spia.c
 *
 *  Copyright (C) 2000 Steven J. Hill (sjhill@realitydiluted.com)
 *
 *
 *	10-29-2001 TG	change to support hardwarespecific access
 *			to controllines	(due to change in nand.c)
 *			page_cache added
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   SPIA board which utilizes the Toshiba TC58V64AFT part. This is
 *   a 64Mibit (8MiB x 8 bits) NAND flash device.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

static struct mtd_info *spia_mtd = NULL;

#define SPIA_IO_BASE	0xd0000000	
#define SPIA_FIO_BASE	0xf0000000	
#define SPIA_PEDR	0x0080	
#define SPIA_PEDDR	0x00c0	


static int spia_io_base = SPIA_IO_BASE;
static int spia_fio_base = SPIA_FIO_BASE;
static int spia_pedr = SPIA_PEDR;
static int spia_peddr = SPIA_PEDDR;

module_param(spia_io_base, int, 0);
module_param(spia_fio_base, int, 0);
module_param(spia_pedr, int, 0);
module_param(spia_peddr, int, 0);

static const struct mtd_partition partition_info[] = {
	{
	 .name = "SPIA flash partition 1",
	 .offset = 0,
	 .size = 2 * 1024 * 1024},
	{
	 .name = "SPIA flash partition 2",
	 .offset = 2 * 1024 * 1024,
	 .size = 6 * 1024 * 1024}
};

#define NUM_PARTITIONS 2

static void spia_hwcontrol(struct mtd_info *mtd, int cmd)
{
	struct nand_chip *chip = mtd->priv;

	if (ctrl & NAND_CTRL_CHANGE) {
		void __iomem *addr = spia_io_base + spia_pedr;
		unsigned char bits;

		bits = (ctrl & NAND_CNE) << 2;
		bits |= (ctrl & NAND_CLE | NAND_ALE) >> 1;
		writeb((readb(addr) & ~0x7) | bits, addr);
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, chip->IO_ADDR_W);
}

static int __init spia_init(void)
{
	struct nand_chip *this;

	
	spia_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	if (!spia_mtd) {
		printk("Unable to allocate SPIA NAND MTD device structure.\n");
		return -ENOMEM;
	}

	
	this = (struct nand_chip *)(&spia_mtd[1]);

	
	memset(spia_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	
	spia_mtd->priv = this;
	spia_mtd->owner = THIS_MODULE;

	(*(volatile unsigned char *)(spia_io_base + spia_peddr)) = 0x07;

	
	this->IO_ADDR_R = (void __iomem *)spia_fio_base;
	this->IO_ADDR_W = (void __iomem *)spia_fio_base;
	
	this->cmd_ctrl = spia_hwcontrol;
	
	this->chip_delay = 15;

	
	if (nand_scan(spia_mtd, 1)) {
		kfree(spia_mtd);
		return -ENXIO;
	}

	
	mtd_device_register(spia_mtd, partition_info, NUM_PARTITIONS);

	
	return 0;
}

module_init(spia_init);

static void __exit spia_cleanup(void)
{
	
	nand_release(spia_mtd);

	
	kfree(spia_mtd);
}

module_exit(spia_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven J. Hill <sjhill@realitydiluted.com");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on SPIA board");
