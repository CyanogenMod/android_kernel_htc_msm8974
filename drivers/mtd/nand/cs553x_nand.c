/*
 * drivers/mtd/nand/cs553x_nand.c
 *
 * (C) 2005, 2006 Red Hat Inc.
 *
 * Author: David Woodhouse <dwmw2@infradead.org>
 *	   Tom Sylla <tom.sylla@amd.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash controller found on
 *   the AMD CS5535/CS5536 companion chipsets for the Geode processor.
 *   mtd-id for command line partitioning is cs553x_nand_cs[0-3]
 *   where 0-3 reflects the chip select for NAND.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/msr.h>
#include <asm/io.h>

#define NR_CS553X_CONTROLLERS	4

#define MSR_DIVIL_GLD_CAP	0x51400000	
#define CAP_CS5535		0x2df000ULL
#define CAP_CS5536		0x5df500ULL

#define MSR_NANDF_DATA		0x5140001b	
#define MSR_NANDF_CTL		0x5140001c	
#define MSR_NANDF_RSVD		0x5140001d	

#define MSR_DIVIL_LBAR_FLSH0	0x51400010	
#define MSR_DIVIL_LBAR_FLSH1	0x51400011	
#define MSR_DIVIL_LBAR_FLSH2	0x51400012	
#define MSR_DIVIL_LBAR_FLSH3	0x51400013	
	
#define FLSH_LBAR_EN		(1ULL<<32)
#define FLSH_NOR_NAND		(1ULL<<33)	
#define FLSH_MEM_IO		(1ULL<<34)	
	
	

#define MSR_DIVIL_BALL_OPTS	0x51400015
#define PIN_OPT_IDE		(1<<0)	

#define MM_NAND_DATA		0x00	
#define MM_NAND_CTL		0x800	
#define MM_NAND_IO		0x801	
#define MM_NAND_STS		0x810
#define MM_NAND_ECC_LSB		0x811
#define MM_NAND_ECC_MSB		0x812
#define MM_NAND_ECC_COL		0x813
#define MM_NAND_LAC		0x814
#define MM_NAND_ECC_CTL		0x815

#define IO_NAND_DATA		0x00	
#define IO_NAND_CTL		0x04
#define IO_NAND_IO		0x05
#define IO_NAND_STS		0x06
#define IO_NAND_ECC_CTL		0x08
#define IO_NAND_ECC_LSB		0x09
#define IO_NAND_ECC_MSB		0x0a
#define IO_NAND_ECC_COL		0x0b
#define IO_NAND_LAC		0x0c

#define CS_NAND_CTL_DIST_EN	(1<<4)	
#define CS_NAND_CTL_RDY_INT_MASK	(1<<3)	
#define CS_NAND_CTL_ALE		(1<<2)
#define CS_NAND_CTL_CLE		(1<<1)
#define CS_NAND_CTL_CE		(1<<0)	

#define CS_NAND_STS_FLASH_RDY	(1<<3)
#define CS_NAND_CTLR_BUSY	(1<<2)
#define CS_NAND_CMD_COMP	(1<<1)
#define CS_NAND_DIST_ST		(1<<0)

#define CS_NAND_ECC_PARITY	(1<<2)
#define CS_NAND_ECC_CLRECC	(1<<1)
#define CS_NAND_ECC_ENECC	(1<<0)

static void cs553x_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *this = mtd->priv;

	while (unlikely(len > 0x800)) {
		memcpy_fromio(buf, this->IO_ADDR_R, 0x800);
		buf += 0x800;
		len -= 0x800;
	}
	memcpy_fromio(buf, this->IO_ADDR_R, len);
}

static void cs553x_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nand_chip *this = mtd->priv;

	while (unlikely(len > 0x800)) {
		memcpy_toio(this->IO_ADDR_R, buf, 0x800);
		buf += 0x800;
		len -= 0x800;
	}
	memcpy_toio(this->IO_ADDR_R, buf, len);
}

static unsigned char cs553x_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	return readb(this->IO_ADDR_R);
}

static void cs553x_write_byte(struct mtd_info *mtd, u_char byte)
{
	struct nand_chip *this = mtd->priv;
	int i = 100000;

	while (i && readb(this->IO_ADDR_R + MM_NAND_STS) & CS_NAND_CTLR_BUSY) {
		udelay(1);
		i--;
	}
	writeb(byte, this->IO_ADDR_W + 0x801);
}

static void cs553x_hwcontrol(struct mtd_info *mtd, int cmd,
			     unsigned int ctrl)
{
	struct nand_chip *this = mtd->priv;
	void __iomem *mmio_base = this->IO_ADDR_R;
	if (ctrl & NAND_CTRL_CHANGE) {
		unsigned char ctl = (ctrl & ~NAND_CTRL_CHANGE ) ^ 0x01;
		writeb(ctl, mmio_base + MM_NAND_CTL);
	}
	if (cmd != NAND_CMD_NONE)
		cs553x_write_byte(mtd, cmd);
}

static int cs553x_device_ready(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	void __iomem *mmio_base = this->IO_ADDR_R;
	unsigned char foo = readb(mmio_base + MM_NAND_STS);

	return (foo & CS_NAND_STS_FLASH_RDY) && !(foo & CS_NAND_CTLR_BUSY);
}

static void cs_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct nand_chip *this = mtd->priv;
	void __iomem *mmio_base = this->IO_ADDR_R;

	writeb(0x07, mmio_base + MM_NAND_ECC_CTL);
}

static int cs_calculate_ecc(struct mtd_info *mtd, const u_char *dat, u_char *ecc_code)
{
	uint32_t ecc;
	struct nand_chip *this = mtd->priv;
	void __iomem *mmio_base = this->IO_ADDR_R;

	ecc = readl(mmio_base + MM_NAND_STS);

	ecc_code[1] = ecc >> 8;
	ecc_code[0] = ecc >> 16;
	ecc_code[2] = ecc >> 24;
	return 0;
}

static struct mtd_info *cs553x_mtd[4];

static int __init cs553x_init_one(int cs, int mmio, unsigned long adr)
{
	int err = 0;
	struct nand_chip *this;
	struct mtd_info *new_mtd;

	printk(KERN_NOTICE "Probing CS553x NAND controller CS#%d at %sIO 0x%08lx\n", cs, mmio?"MM":"P", adr);

	if (!mmio) {
		printk(KERN_NOTICE "PIO mode not yet implemented for CS553X NAND controller\n");
		return -ENXIO;
	}

	
	new_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	if (!new_mtd) {
		printk(KERN_WARNING "Unable to allocate CS553X NAND MTD device structure.\n");
		err = -ENOMEM;
		goto out;
	}

	
	this = (struct nand_chip *)(&new_mtd[1]);

	
	memset(new_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	
	new_mtd->priv = this;
	new_mtd->owner = THIS_MODULE;

	
	this->IO_ADDR_R = this->IO_ADDR_W = ioremap(adr, 4096);
	if (!this->IO_ADDR_R) {
		printk(KERN_WARNING "ioremap cs553x NAND @0x%08lx failed\n", adr);
		err = -EIO;
		goto out_mtd;
	}

	this->cmd_ctrl = cs553x_hwcontrol;
	this->dev_ready = cs553x_device_ready;
	this->read_byte = cs553x_read_byte;
	this->read_buf = cs553x_read_buf;
	this->write_buf = cs553x_write_buf;

	this->chip_delay = 0;

	this->ecc.mode = NAND_ECC_HW;
	this->ecc.size = 256;
	this->ecc.bytes = 3;
	this->ecc.hwctl  = cs_enable_hwecc;
	this->ecc.calculate = cs_calculate_ecc;
	this->ecc.correct  = nand_correct_data;

	
	this->bbt_options = NAND_BBT_USE_FLASH;
	this->options = NAND_NO_AUTOINCR;

	
	if (nand_scan(new_mtd, 1)) {
		err = -ENXIO;
		goto out_ior;
	}

	this->ecc.strength = 1;

	new_mtd->name = kasprintf(GFP_KERNEL, "cs553x_nand_cs%d", cs);

	cs553x_mtd[cs] = new_mtd;
	goto out;

out_ior:
	iounmap(this->IO_ADDR_R);
out_mtd:
	kfree(new_mtd);
out:
	return err;
}

static int is_geode(void)
{
	
	if (boot_cpu_data.x86_vendor == X86_VENDOR_AMD &&
	    boot_cpu_data.x86 == 5 &&
	    boot_cpu_data.x86_model == 10)
		return 1; 

	if ((boot_cpu_data.x86_vendor == X86_VENDOR_NSC ||
	     boot_cpu_data.x86_vendor == X86_VENDOR_CYRIX) &&
	    boot_cpu_data.x86 == 5 &&
	    boot_cpu_data.x86_model == 5)
		return 1; 

	return 0;
}

static int __init cs553x_init(void)
{
	int err = -ENXIO;
	int i;
	uint64_t val;

	
	if (!is_geode())
		return -ENXIO;

	
	rdmsrl(MSR_DIVIL_GLD_CAP, val);
	val &= ~0xFFULL;
	if (val != CAP_CS5535 && val != CAP_CS5536)
		return -ENXIO;

	
	rdmsrl(MSR_DIVIL_BALL_OPTS, val);
	if (val & PIN_OPT_IDE) {
		printk(KERN_INFO "CS553x NAND controller: Flash I/O not enabled in MSR_DIVIL_BALL_OPTS.\n");
		return -ENXIO;
	}

	for (i = 0; i < NR_CS553X_CONTROLLERS; i++) {
		rdmsrl(MSR_DIVIL_LBAR_FLSH0 + i, val);

		if ((val & (FLSH_LBAR_EN|FLSH_NOR_NAND)) == (FLSH_LBAR_EN|FLSH_NOR_NAND))
			err = cs553x_init_one(i, !!(val & FLSH_MEM_IO), val & 0xFFFFFFFF);
	}

	for (i = 0; i < NR_CS553X_CONTROLLERS; i++) {
		if (cs553x_mtd[i]) {
			
			mtd_device_parse_register(cs553x_mtd[i], NULL, NULL,
						  NULL, 0);
			err = 0;
		}
	}

	return err;
}

module_init(cs553x_init);

static void __exit cs553x_cleanup(void)
{
	int i;

	for (i = 0; i < NR_CS553X_CONTROLLERS; i++) {
		struct mtd_info *mtd = cs553x_mtd[i];
		struct nand_chip *this;
		void __iomem *mmio_base;

		if (!mtd)
			continue;

		this = cs553x_mtd[i]->priv;
		mmio_base = this->IO_ADDR_R;

		
		nand_release(cs553x_mtd[i]);
		kfree(cs553x_mtd[i]->name);
		cs553x_mtd[i] = NULL;

		
		iounmap(mmio_base);

		
		kfree(mtd);
	}
}

module_exit(cs553x_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("NAND controller driver for AMD CS5535/CS5536 companion chip");
