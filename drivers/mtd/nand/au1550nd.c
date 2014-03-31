/*
 *  drivers/mtd/nand/au1550nd.c
 *
 *  Copyright (C) 2004 Embedded Edge, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-au1x00/au1550nd.h>


struct au1550nd_ctx {
	struct mtd_info info;
	struct nand_chip chip;

	int cs;
	void __iomem *base;
	void (*write_byte)(struct mtd_info *, u_char);
};

static u_char au_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	u_char ret = readb(this->IO_ADDR_R);
	au_sync();
	return ret;
}

static void au_write_byte(struct mtd_info *mtd, u_char byte)
{
	struct nand_chip *this = mtd->priv;
	writeb(byte, this->IO_ADDR_W);
	au_sync();
}

static u_char au_read_byte16(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	u_char ret = (u_char) cpu_to_le16(readw(this->IO_ADDR_R));
	au_sync();
	return ret;
}

static void au_write_byte16(struct mtd_info *mtd, u_char byte)
{
	struct nand_chip *this = mtd->priv;
	writew(le16_to_cpu((u16) byte), this->IO_ADDR_W);
	au_sync();
}

static u16 au_read_word(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	u16 ret = readw(this->IO_ADDR_R);
	au_sync();
	return ret;
}

static void au_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++) {
		writeb(buf[i], this->IO_ADDR_W);
		au_sync();
	}
}

static void au_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++) {
		buf[i] = readb(this->IO_ADDR_R);
		au_sync();
	}
}

static int au_verify_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++) {
		if (buf[i] != readb(this->IO_ADDR_R))
			return -EFAULT;
		au_sync();
	}

	return 0;
}

static void au_write_buf16(struct mtd_info *mtd, const u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;

	for (i = 0; i < len; i++) {
		writew(p[i], this->IO_ADDR_W);
		au_sync();
	}

}

static void au_read_buf16(struct mtd_info *mtd, u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;

	for (i = 0; i < len; i++) {
		p[i] = readw(this->IO_ADDR_R);
		au_sync();
	}
}

static int au_verify_buf16(struct mtd_info *mtd, const u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;

	for (i = 0; i < len; i++) {
		if (p[i] != readw(this->IO_ADDR_R))
			return -EFAULT;
		au_sync();
	}
	return 0;
}

#define NAND_CTL_SETNCE		1
#define NAND_CTL_CLRNCE		2
#define NAND_CTL_SETCLE		3
#define NAND_CTL_CLRCLE		4
#define NAND_CTL_SETALE		5
#define NAND_CTL_CLRALE		6

static void au1550_hwcontrol(struct mtd_info *mtd, int cmd)
{
	struct au1550nd_ctx *ctx = container_of(mtd, struct au1550nd_ctx, info);
	struct nand_chip *this = mtd->priv;

	switch (cmd) {

	case NAND_CTL_SETCLE:
		this->IO_ADDR_W = ctx->base + MEM_STNAND_CMD;
		break;

	case NAND_CTL_CLRCLE:
		this->IO_ADDR_W = ctx->base + MEM_STNAND_DATA;
		break;

	case NAND_CTL_SETALE:
		this->IO_ADDR_W = ctx->base + MEM_STNAND_ADDR;
		break;

	case NAND_CTL_CLRALE:
		this->IO_ADDR_W = ctx->base + MEM_STNAND_DATA;
		udelay(1);
		break;

	case NAND_CTL_SETNCE:
		
		au_writel((1 << (4 + ctx->cs)), MEM_STNDCTL);
		break;

	case NAND_CTL_CLRNCE:
		
		au_writel(0, MEM_STNDCTL);
		break;
	}

	this->IO_ADDR_R = this->IO_ADDR_W;

	
	au_sync();
}

int au1550_device_ready(struct mtd_info *mtd)
{
	int ret = (au_readl(MEM_STSTAT) & 0x1) ? 1 : 0;
	au_sync();
	return ret;
}

static void au1550_select_chip(struct mtd_info *mtd, int chip)
{
}

static void au1550_command(struct mtd_info *mtd, unsigned command, int column, int page_addr)
{
	struct au1550nd_ctx *ctx = container_of(mtd, struct au1550nd_ctx, info);
	struct nand_chip *this = mtd->priv;
	int ce_override = 0, i;
	unsigned long flags = 0;

	
	au1550_hwcontrol(mtd, NAND_CTL_SETCLE);
	if (command == NAND_CMD_SEQIN) {
		int readcmd;

		if (column >= mtd->writesize) {
			
			column -= mtd->writesize;
			readcmd = NAND_CMD_READOOB;
		} else if (column < 256) {
			
			readcmd = NAND_CMD_READ0;
		} else {
			column -= 256;
			readcmd = NAND_CMD_READ1;
		}
		ctx->write_byte(mtd, readcmd);
	}
	ctx->write_byte(mtd, command);

	
	au1550_hwcontrol(mtd, NAND_CTL_CLRCLE);

	if (column != -1 || page_addr != -1) {
		au1550_hwcontrol(mtd, NAND_CTL_SETALE);

		
		if (column != -1) {
			
			if (this->options & NAND_BUSWIDTH_16)
				column >>= 1;
			ctx->write_byte(mtd, column);
		}
		if (page_addr != -1) {
			ctx->write_byte(mtd, (u8)(page_addr & 0xff));

			if (command == NAND_CMD_READ0 ||
			    command == NAND_CMD_READ1 ||
			    command == NAND_CMD_READOOB) {
				/*
				 * NAND controller will release -CE after
				 * the last address byte is written, so we'll
				 * have to forcibly assert it. No interrupts
				 * are allowed while we do this as we don't
				 * want the NOR flash or PCMCIA drivers to
				 * steal our precious bytes of data...
				 */
				ce_override = 1;
				local_irq_save(flags);
				au1550_hwcontrol(mtd, NAND_CTL_SETNCE);
			}

			ctx->write_byte(mtd, (u8)(page_addr >> 8));

			
			if (this->chipsize > (32 << 20))
				ctx->write_byte(mtd,
						((page_addr >> 16) & 0x0f));
		}
		
		au1550_hwcontrol(mtd, NAND_CTL_CLRALE);
	}

	switch (command) {

	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_STATUS:
		return;

	case NAND_CMD_RESET:
		break;

	case NAND_CMD_READ0:
	case NAND_CMD_READ1:
	case NAND_CMD_READOOB:
		
		if (unlikely(!ce_override))
			break;

		
		ndelay(100);
		
		for (i = this->chip_delay; !this->dev_ready(mtd) && i > 0; --i)
			udelay(1);

		
		au1550_hwcontrol(mtd, NAND_CTL_CLRNCE);
		local_irq_restore(flags);
		return;
	}
	
	ndelay(100);

	while(!this->dev_ready(mtd));
}

static int __devinit find_nand_cs(unsigned long nand_base)
{
	void __iomem *base =
			(void __iomem *)KSEG1ADDR(AU1000_STATIC_MEM_PHYS_ADDR);
	unsigned long addr, staddr, start, mask, end;
	int i;

	for (i = 0; i < 4; i++) {
		addr = 0x1000 + (i * 0x10);			
		staddr = __raw_readl(base + addr + 0x08);	
		
		start = (staddr << 4) & 0xfffc0000;
		mask = (staddr << 18) & 0xfffc0000;
		end = (start | (start - 1)) & ~(start ^ mask);
		if ((nand_base >= start) && (nand_base < end))
			return i;
	}

	return -ENODEV;
}

static int __devinit au1550nd_probe(struct platform_device *pdev)
{
	struct au1550nd_platdata *pd;
	struct au1550nd_ctx *ctx;
	struct nand_chip *this;
	struct resource *r;
	int ret, cs;

	pd = pdev->dev.platform_data;
	if (!pd) {
		dev_err(&pdev->dev, "missing platform data\n");
		return -ENODEV;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		dev_err(&pdev->dev, "no memory for NAND context\n");
		return -ENOMEM;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "no NAND memory resource\n");
		ret = -ENODEV;
		goto out1;
	}
	if (request_mem_region(r->start, resource_size(r), "au1550-nand")) {
		dev_err(&pdev->dev, "cannot claim NAND memory area\n");
		ret = -ENOMEM;
		goto out1;
	}

	ctx->base = ioremap_nocache(r->start, 0x1000);
	if (!ctx->base) {
		dev_err(&pdev->dev, "cannot remap NAND memory area\n");
		ret = -ENODEV;
		goto out2;
	}

	this = &ctx->chip;
	ctx->info.priv = this;
	ctx->info.owner = THIS_MODULE;

	
	cs = find_nand_cs(r->start);
	if (cs < 0) {
		dev_err(&pdev->dev, "cannot detect NAND chipselect\n");
		ret = -ENODEV;
		goto out3;
	}
	ctx->cs = cs;

	this->dev_ready = au1550_device_ready;
	this->select_chip = au1550_select_chip;
	this->cmdfunc = au1550_command;

	
	this->chip_delay = 30;
	this->ecc.mode = NAND_ECC_SOFT;

	this->options = NAND_NO_AUTOINCR;

	if (pd->devwidth)
		this->options |= NAND_BUSWIDTH_16;

	this->read_byte = (pd->devwidth) ? au_read_byte16 : au_read_byte;
	ctx->write_byte = (pd->devwidth) ? au_write_byte16 : au_write_byte;
	this->read_word = au_read_word;
	this->write_buf = (pd->devwidth) ? au_write_buf16 : au_write_buf;
	this->read_buf = (pd->devwidth) ? au_read_buf16 : au_read_buf;
	this->verify_buf = (pd->devwidth) ? au_verify_buf16 : au_verify_buf;

	ret = nand_scan(&ctx->info, 1);
	if (ret) {
		dev_err(&pdev->dev, "NAND scan failed with %d\n", ret);
		goto out3;
	}

	mtd_device_register(&ctx->info, pd->parts, pd->num_parts);

	return 0;

out3:
	iounmap(ctx->base);
out2:
	release_mem_region(r->start, resource_size(r));
out1:
	kfree(ctx);
	return ret;
}

static int __devexit au1550nd_remove(struct platform_device *pdev)
{
	struct au1550nd_ctx *ctx = platform_get_drvdata(pdev);
	struct resource *r = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	nand_release(&ctx->info);
	iounmap(ctx->base);
	release_mem_region(r->start, 0x1000);
	kfree(ctx);
	return 0;
}

static struct platform_driver au1550nd_driver = {
	.driver = {
		.name	= "au1550-nand",
		.owner	= THIS_MODULE,
	},
	.probe		= au1550nd_probe,
	.remove		= __devexit_p(au1550nd_remove),
};

module_platform_driver(au1550nd_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Edge, LLC");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on Pb1550 board");
