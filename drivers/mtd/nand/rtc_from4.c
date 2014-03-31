/*
 *  drivers/mtd/nand/rtc_from4.c
 *
 *  Copyright (C) 2004  Red Hat, Inc.
 *
 *  Derived from drivers/mtd/nand/spia.c
 *       Copyright (C) 2000 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Overview:
 *   This is a device driver for the AG-AND flash device found on the
 *   Renesas Technology Corp. Flash ROM 4-slot interface board (FROM_BOARD4),
 *   which utilizes the Renesas HN29V1G91T-30 part.
 *   This chip is a 1 GBibit (128MiB x 8 bits) AG-AND flash device.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/rslib.h>
#include <linux/bitrev.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

static struct mtd_info *rtc_from4_mtd = NULL;

#define RTC_FROM4_MAX_CHIPS	2

#define SH77X9_BCR1	((volatile unsigned short *)(0xFFFFFF60))
#define SH77X9_BCR2	((volatile unsigned short *)(0xFFFFFF62))
#define SH77X9_WCR1	((volatile unsigned short *)(0xFFFFFF64))
#define SH77X9_WCR2	((volatile unsigned short *)(0xFFFFFF66))
#define SH77X9_MCR	((volatile unsigned short *)(0xFFFFFF68))
#define SH77X9_PCR	((volatile unsigned short *)(0xFFFFFF6C))
#define SH77X9_FRQCR	((volatile unsigned short *)(0xFFFFFF80))

#define RTC_FROM4_FIO_BASE	0x14000000

#define RTC_FROM4_CLE		(1 << 5)
#define RTC_FROM4_ALE		(1 << 4)

#define RTC_FROM4_NAND_ADDR_SLOT3	(0x00800000)
#define RTC_FROM4_NAND_ADDR_SLOT4	(0x00C00000)
#define RTC_FROM4_NAND_ADDR_FPGA	(0x01000000)
#define RTC_FROM4_NAND_ADDR_MASK	(RTC_FROM4_NAND_ADDR_SLOT3 | RTC_FROM4_NAND_ADDR_SLOT4 | RTC_FROM4_NAND_ADDR_FPGA)

#define RTC_FROM4_FPGA_SR		(RTC_FROM4_NAND_ADDR_FPGA | 0x00000002)
#define RTC_FROM4_DEVICE_READY		0x0001


#define RTC_FROM4_RS_ECC_CTL		(RTC_FROM4_NAND_ADDR_FPGA | 0x00000050)
#define RTC_FROM4_RS_ECC_CTL_CLR	(1 << 7)
#define RTC_FROM4_RS_ECC_CTL_GEN	(1 << 6)
#define RTC_FROM4_RS_ECC_CTL_FD_E	(1 << 5)

#define RTC_FROM4_RS_ECC		(RTC_FROM4_NAND_ADDR_FPGA | 0x00000060)
#define RTC_FROM4_RS_ECCN		(RTC_FROM4_NAND_ADDR_FPGA | 0x00000080)

#define RTC_FROM4_RS_ECC_CHK		(RTC_FROM4_NAND_ADDR_FPGA | 0x00000070)
#define RTC_FROM4_RS_ECC_CHK_ERROR	(1 << 7)

#define ERR_STAT_ECC_AVAILABLE		0x20

#define RTC_FROM4_HWECC	1

#define RTC_FROM4_NO_VIRTBLOCKS	0

static void __iomem *rtc_from4_fio_base = (void *)P2SEGADDR(RTC_FROM4_FIO_BASE);

static const struct mtd_partition partition_info[] = {
	{
	 .name = "Renesas flash partition 1",
	 .offset = 0,
	 .size = MTDPART_SIZ_FULL},
};

#define NUM_PARTITIONS 1

static uint8_t bbt_pattern[] = { 'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = { '1', 't', 'b', 'B' };

static struct nand_bbt_descr rtc_from4_bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 40,
	.len = 4,
	.veroffs = 44,
	.maxblocks = 4,
	.pattern = bbt_pattern
};

static struct nand_bbt_descr rtc_from4_bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 40,
	.len = 4,
	.veroffs = 44,
	.maxblocks = 4,
	.pattern = mirror_pattern
};

#ifdef RTC_FROM4_HWECC

static struct rs_control *rs_decoder;

static struct nand_ecclayout rtc_from4_nand_oobinfo = {
	.eccbytes = 32,
	.eccpos = {
		   0, 1, 2, 3, 4, 5, 6, 7,
		   8, 9, 10, 11, 12, 13, 14, 15,
		   16, 17, 18, 19, 20, 21, 22, 23,
		   24, 25, 26, 27, 28, 29, 30, 31},
	.oobfree = {{32, 32}}
};

#endif

static void rtc_from4_hwcontrol(struct mtd_info *mtd, int cmd,
				unsigned int ctrl)
{
	struct nand_chip *chip = (mtd->priv);

	if (cmd == NAND_CMD_NONE)
		return;

	if (ctrl & NAND_CLE)
		writeb(cmd, chip->IO_ADDR_W | RTC_FROM4_CLE);
	else
		writeb(cmd, chip->IO_ADDR_W | RTC_FROM4_ALE);
}

static void rtc_from4_nand_select_chip(struct mtd_info *mtd, int chip)
{
	struct nand_chip *this = mtd->priv;

	this->IO_ADDR_R = (void __iomem *)((unsigned long)this->IO_ADDR_R & ~RTC_FROM4_NAND_ADDR_MASK);
	this->IO_ADDR_W = (void __iomem *)((unsigned long)this->IO_ADDR_W & ~RTC_FROM4_NAND_ADDR_MASK);

	switch (chip) {

	case 0:		
		this->IO_ADDR_R = (void __iomem *)((unsigned long)this->IO_ADDR_R | RTC_FROM4_NAND_ADDR_SLOT3);
		this->IO_ADDR_W = (void __iomem *)((unsigned long)this->IO_ADDR_W | RTC_FROM4_NAND_ADDR_SLOT3);
		break;
	case 1:		
		this->IO_ADDR_R = (void __iomem *)((unsigned long)this->IO_ADDR_R | RTC_FROM4_NAND_ADDR_SLOT4);
		this->IO_ADDR_W = (void __iomem *)((unsigned long)this->IO_ADDR_W | RTC_FROM4_NAND_ADDR_SLOT4);
		break;

	}
}

static int rtc_from4_nand_device_ready(struct mtd_info *mtd)
{
	unsigned short status;

	status = *((volatile unsigned short *)(rtc_from4_fio_base + RTC_FROM4_FPGA_SR));

	return (status & RTC_FROM4_DEVICE_READY);

}

static void deplete(struct mtd_info *mtd, int chip)
{
	struct nand_chip *this = mtd->priv;

	
	while (!this->dev_ready(mtd)) ;

	this->select_chip(mtd, chip);

	
	this->cmdfunc(mtd, NAND_CMD_DEPLETE1, 0x0000, 0x0000);
	this->cmdfunc(mtd, NAND_CMD_DEPLETE2, -1, -1);

	
	this->cmdfunc(mtd, NAND_CMD_DEPLETE1, 0x0000, 0x0004);
	this->cmdfunc(mtd, NAND_CMD_DEPLETE2, -1, -1);

}

#ifdef RTC_FROM4_HWECC
static void rtc_from4_enable_hwecc(struct mtd_info *mtd, int mode)
{
	volatile unsigned short *rs_ecc_ctl = (volatile unsigned short *)(rtc_from4_fio_base + RTC_FROM4_RS_ECC_CTL);
	unsigned short status;

	switch (mode) {
	case NAND_ECC_READ:
		status = RTC_FROM4_RS_ECC_CTL_CLR | RTC_FROM4_RS_ECC_CTL_FD_E;

		*rs_ecc_ctl = status;
		break;

	case NAND_ECC_READSYN:
		status = 0x00;

		*rs_ecc_ctl = status;
		break;

	case NAND_ECC_WRITE:
		status = RTC_FROM4_RS_ECC_CTL_CLR | RTC_FROM4_RS_ECC_CTL_GEN | RTC_FROM4_RS_ECC_CTL_FD_E;

		*rs_ecc_ctl = status;
		break;

	default:
		BUG();
		break;
	}

}

static void rtc_from4_calculate_ecc(struct mtd_info *mtd, const u_char *dat, u_char *ecc_code)
{
	volatile unsigned short *rs_eccn = (volatile unsigned short *)(rtc_from4_fio_base + RTC_FROM4_RS_ECCN);
	unsigned short value;
	int i;

	for (i = 0; i < 8; i++) {
		value = *rs_eccn;
		ecc_code[i] = (unsigned char)value;
		rs_eccn++;
	}
	ecc_code[7] |= 0x0f;	
}

static int rtc_from4_correct_data(struct mtd_info *mtd, const u_char *buf, u_char *ecc1, u_char *ecc2)
{
	int i, j, res;
	unsigned short status;
	uint16_t par[6], syn[6];
	uint8_t ecc[8];
	volatile unsigned short *rs_ecc;

	status = *((volatile unsigned short *)(rtc_from4_fio_base + RTC_FROM4_RS_ECC_CHK));

	if (!(status & RTC_FROM4_RS_ECC_CHK_ERROR)) {
		return 0;
	}

	
	rs_ecc = (volatile unsigned short *)(rtc_from4_fio_base + RTC_FROM4_RS_ECC);
	for (i = 0; i < 8; i++) {
		ecc[i] = bitrev8(*rs_ecc);
		rs_ecc++;
	}

	
	par[5] = rs_decoder->index_of[(((uint16_t) ecc[0] >> 0) & 0x0ff) | (((uint16_t) ecc[1] << 8) & 0x300)];
	par[4] = rs_decoder->index_of[(((uint16_t) ecc[1] >> 2) & 0x03f) | (((uint16_t) ecc[2] << 6) & 0x3c0)];
	par[3] = rs_decoder->index_of[(((uint16_t) ecc[2] >> 4) & 0x00f) | (((uint16_t) ecc[3] << 4) & 0x3f0)];
	par[2] = rs_decoder->index_of[(((uint16_t) ecc[3] >> 6) & 0x003) | (((uint16_t) ecc[4] << 2) & 0x3fc)];
	par[1] = rs_decoder->index_of[(((uint16_t) ecc[5] >> 0) & 0x0ff) | (((uint16_t) ecc[6] << 8) & 0x300)];
	par[0] = (((uint16_t) ecc[6] >> 2) & 0x03f) | (((uint16_t) ecc[7] << 6) & 0x3c0);

	
	for (i = 0; i < 6; i++) {
		syn[i] = par[0];
		for (j = 1; j < 6; j++)
			if (par[j] != rs_decoder->nn)
				syn[i] ^= rs_decoder->alpha_to[rs_modnn(rs_decoder, par[j] + i * j)];

		
		syn[i] = rs_decoder->index_of[syn[i]];
	}

	
	res = decode_rs8(rs_decoder, (uint8_t *) buf, par, 512, syn, 0, NULL, 0xff, NULL);
	if (res > 0) {
		pr_debug("rtc_from4_correct_data: " "ECC corrected %d errors on read\n", res);
	}
	return res;
}

static int rtc_from4_errstat(struct mtd_info *mtd, struct nand_chip *this,
			     int state, int status, int page)
{
	int er_stat = 0;
	int rtn, retlen;
	size_t len;
	uint8_t *buf;
	int i;

	this->cmdfunc(mtd, NAND_CMD_STATUS_CLEAR, -1, -1);

	if (state == FL_ERASING) {

		for (i = 0; i < 4; i++) {
			if (!(status & 1 << (i + 1)))
				continue;
			this->cmdfunc(mtd, (NAND_CMD_STATUS_ERROR + i + 1),
				      -1, -1);
			rtn = this->read_byte(mtd);
			this->cmdfunc(mtd, NAND_CMD_STATUS_RESET, -1, -1);

			
			if (!(rtn & ERR_STAT_ECC_AVAILABLE))
				er_stat |= 1 << (i + 1);
		}

	} else if (state == FL_WRITING) {

		unsigned long corrected = mtd->ecc_stats.corrected;

		
		this->cmdfunc(mtd, NAND_CMD_STATUS_ERROR, -1, -1);
		rtn = this->read_byte(mtd);
		this->cmdfunc(mtd, NAND_CMD_STATUS_RESET, -1, -1);

		if (!(rtn & ERR_STAT_ECC_AVAILABLE)) {
			
			er_stat |= 1 << 1;
			goto out;
		}

		len = mtd->writesize;
		buf = kmalloc(len, GFP_KERNEL);
		if (!buf) {
			er_stat = 1;
			goto out;
		}

		
		rtn = nand_do_read(mtd, page, len, &retlen, buf);

		
		if (rtn || (mtd->ecc_stats.corrected - corrected) > 1)
			er_stat |= 1 << 1;
		kfree(buf);
	}
out:
	rtn = status;
	if (er_stat == 0) {	
		rtn = (status & ~NAND_STATUS_FAIL);	
	}

	return rtn;
}
#endif

static int __init rtc_from4_init(void)
{
	struct nand_chip *this;
	unsigned short bcr1, bcr2, wcr2;
	int i;
	int ret;

	
	rtc_from4_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	if (!rtc_from4_mtd) {
		printk("Unable to allocate Renesas NAND MTD device structure.\n");
		return -ENOMEM;
	}

	
	this = (struct nand_chip *)(&rtc_from4_mtd[1]);

	
	memset(rtc_from4_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	
	rtc_from4_mtd->priv = this;
	rtc_from4_mtd->owner = THIS_MODULE;

	
	bcr1 = *SH77X9_BCR1 & ~0x0002;
	bcr1 |= 0x0002;
	*SH77X9_BCR1 = bcr1;

	
	bcr2 = *SH77X9_BCR2 & ~0x0c00;
	bcr2 |= 0x0800;
	*SH77X9_BCR2 = bcr2;

	
	wcr2 = *SH77X9_WCR2 & ~0x1c00;
	wcr2 |= 0x1c00;
	*SH77X9_WCR2 = wcr2;

	
	this->IO_ADDR_R = rtc_from4_fio_base;
	this->IO_ADDR_W = rtc_from4_fio_base;
	
	this->cmd_ctrl = rtc_from4_hwcontrol;
	
	this->select_chip = rtc_from4_nand_select_chip;
	
	this->chip_delay = 100;
	
	this->dev_ready = rtc_from4_nand_device_ready;

#ifdef RTC_FROM4_HWECC
	printk(KERN_INFO "rtc_from4_init: using hardware ECC detection.\n");

	this->ecc.mode = NAND_ECC_HW_SYNDROME;
	this->ecc.size = 512;
	this->ecc.bytes = 8;
	this->ecc.strength = 3;
	
	this->errstat = rtc_from4_errstat;
	
	this->ecc.layout = &rtc_from4_nand_oobinfo;
	this->ecc.hwctl = rtc_from4_enable_hwecc;
	this->ecc.calculate = rtc_from4_calculate_ecc;
	this->ecc.correct = rtc_from4_correct_data;

	rs_decoder = init_rs(10, 0x409, 0, 1, 6);
	if (!rs_decoder) {
		printk(KERN_ERR "Could not create a RS decoder\n");
		ret = -ENOMEM;
		goto err_1;
	}
#else
	printk(KERN_INFO "rtc_from4_init: using software ECC detection.\n");

	this->ecc.mode = NAND_ECC_SOFT;
#endif

	
	this->bbt_td = &rtc_from4_bbt_main_descr;
	this->bbt_md = &rtc_from4_bbt_mirror_descr;

	
	if (nand_scan(rtc_from4_mtd, RTC_FROM4_MAX_CHIPS)) {
		ret = -ENXIO;
		goto err_2;
	}

	
	for (i = 0; i < this->numchips; i++) {
		deplete(rtc_from4_mtd, i);
	}

#if RTC_FROM4_NO_VIRTBLOCKS
	
	
	
	rtc_from4_mtd->flags |= MTD_NO_VIRTBLOCKS;
#endif

	
	ret = mtd_device_register(rtc_from4_mtd, partition_info,
				  NUM_PARTITIONS);
	if (ret)
		goto err_3;

	
	return 0;
err_3:
	nand_release(rtc_from4_mtd);
err_2:
	free_rs(rs_decoder);
err_1:
	kfree(rtc_from4_mtd);
	return ret;
}

module_init(rtc_from4_init);

static void __exit rtc_from4_cleanup(void)
{
	
	nand_release(rtc_from4_mtd);

	
	kfree(rtc_from4_mtd);

#ifdef RTC_FROM4_HWECC
	
	if (rs_decoder) {
		free_rs(rs_decoder);
	}
#endif
}

module_exit(rtc_from4_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("d.marlin <dmarlin@redhat.com");
MODULE_DESCRIPTION("Board-specific glue layer for AG-AND flash on Renesas FROM_BOARD4");
