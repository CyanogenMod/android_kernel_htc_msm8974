/*
 * drivers/mtd/nand/sharpsl.c
 *
 *  Copyright (C) 2004 Richard Purdie
 *  Copyright (C) 2008 Dmitry Baryshkov
 *
 *  Based on Sharp's NAND driver sharp_sl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/genhd.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/sharpsl.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>

struct sharpsl_nand {
	struct mtd_info		mtd;
	struct nand_chip	chip;

	void __iomem		*io;
};

#define mtd_to_sharpsl(_mtd)	container_of(_mtd, struct sharpsl_nand, mtd)

#define ECCLPLB		0x00	
#define ECCLPUB		0x04	
#define ECCCP		0x08	
#define ECCCNTR		0x0C	
#define ECCCLRR		0x10	
#define FLASHIO		0x14	
#define FLASHCTL	0x18	

#define FLRYBY		(1 << 5)
#define FLCE1		(1 << 4)
#define FLWP		(1 << 3)
#define FLALE		(1 << 2)
#define FLCLE		(1 << 1)
#define FLCE0		(1 << 0)

static void sharpsl_nand_hwcontrol(struct mtd_info *mtd, int cmd,
				   unsigned int ctrl)
{
	struct sharpsl_nand *sharpsl = mtd_to_sharpsl(mtd);
	struct nand_chip *chip = mtd->priv;

	if (ctrl & NAND_CTRL_CHANGE) {
		unsigned char bits = ctrl & 0x07;

		bits |= (ctrl & 0x01) << 4;

		bits ^= 0x11;

		writeb((readb(sharpsl->io + FLASHCTL) & ~0x17) | bits, sharpsl->io + FLASHCTL);
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, chip->IO_ADDR_W);
}

static int sharpsl_nand_dev_ready(struct mtd_info *mtd)
{
	struct sharpsl_nand *sharpsl = mtd_to_sharpsl(mtd);
	return !((readb(sharpsl->io + FLASHCTL) & FLRYBY) == 0);
}

static void sharpsl_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct sharpsl_nand *sharpsl = mtd_to_sharpsl(mtd);
	writeb(0, sharpsl->io + ECCCLRR);
}

static int sharpsl_nand_calculate_ecc(struct mtd_info *mtd, const u_char * dat, u_char * ecc_code)
{
	struct sharpsl_nand *sharpsl = mtd_to_sharpsl(mtd);
	ecc_code[0] = ~readb(sharpsl->io + ECCLPUB);
	ecc_code[1] = ~readb(sharpsl->io + ECCLPLB);
	ecc_code[2] = (~readb(sharpsl->io + ECCCP) << 2) | 0x03;
	return readb(sharpsl->io + ECCCNTR) != 0;
}

static int __devinit sharpsl_nand_probe(struct platform_device *pdev)
{
	struct nand_chip *this;
	struct resource *r;
	int err = 0;
	struct sharpsl_nand *sharpsl;
	struct sharpsl_nand_platform_data *data = pdev->dev.platform_data;

	if (!data) {
		dev_err(&pdev->dev, "no platform data!\n");
		return -EINVAL;
	}

	
	sharpsl = kzalloc(sizeof(struct sharpsl_nand), GFP_KERNEL);
	if (!sharpsl) {
		printk("Unable to allocate SharpSL NAND MTD device structure.\n");
		return -ENOMEM;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "no io memory resource defined!\n");
		err = -ENODEV;
		goto err_get_res;
	}

	
	sharpsl->io = ioremap(r->start, resource_size(r));
	if (!sharpsl->io) {
		printk("ioremap to access Sharp SL NAND chip failed\n");
		err = -EIO;
		goto err_ioremap;
	}

	
	this = (struct nand_chip *)(&sharpsl->chip);

	
	sharpsl->mtd.priv = this;
	sharpsl->mtd.owner = THIS_MODULE;

	platform_set_drvdata(pdev, sharpsl);

	writeb(readb(sharpsl->io + FLASHCTL) | FLWP, sharpsl->io + FLASHCTL);

	
	this->IO_ADDR_R = sharpsl->io + FLASHIO;
	this->IO_ADDR_W = sharpsl->io + FLASHIO;
	
	this->cmd_ctrl = sharpsl_nand_hwcontrol;
	this->dev_ready = sharpsl_nand_dev_ready;
	
	this->chip_delay = 15;
	
	this->ecc.mode = NAND_ECC_HW;
	this->ecc.size = 256;
	this->ecc.bytes = 3;
	this->ecc.strength = 1;
	this->badblock_pattern = data->badblock_pattern;
	this->ecc.layout = data->ecc_layout;
	this->ecc.hwctl = sharpsl_nand_enable_hwecc;
	this->ecc.calculate = sharpsl_nand_calculate_ecc;
	this->ecc.correct = nand_correct_data;

	
	err = nand_scan(&sharpsl->mtd, 1);
	if (err)
		goto err_scan;

	
	sharpsl->mtd.name = "sharpsl-nand";

	err = mtd_device_parse_register(&sharpsl->mtd, NULL, NULL,
					data->partitions, data->nr_partitions);
	if (err)
		goto err_add;

	
	return 0;

err_add:
	nand_release(&sharpsl->mtd);

err_scan:
	platform_set_drvdata(pdev, NULL);
	iounmap(sharpsl->io);
err_ioremap:
err_get_res:
	kfree(sharpsl);
	return err;
}

static int __devexit sharpsl_nand_remove(struct platform_device *pdev)
{
	struct sharpsl_nand *sharpsl = platform_get_drvdata(pdev);

	
	nand_release(&sharpsl->mtd);

	platform_set_drvdata(pdev, NULL);

	iounmap(sharpsl->io);

	
	kfree(sharpsl);

	return 0;
}

static struct platform_driver sharpsl_nand_driver = {
	.driver = {
		.name	= "sharpsl-nand",
		.owner	= THIS_MODULE,
	},
	.probe		= sharpsl_nand_probe,
	.remove		= __devexit_p(sharpsl_nand_remove),
};

module_platform_driver(sharpsl_nand_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richard Purdie <rpurdie@rpsys.net>");
MODULE_DESCRIPTION("Device specific logic for NAND flash on Sharp SL-C7xx Series");
