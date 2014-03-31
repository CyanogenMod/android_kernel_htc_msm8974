/*
 * Copyright © 2004 Texas Instruments, Jian Zhang <jzhang@ti.com>
 * Copyright © 2004 Micron Technology Inc.
 * Copyright © 2004 David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <plat/dma.h>
#include <plat/gpmc.h>
#include <plat/nand.h>

#define	DRIVER_NAME	"omap2-nand"
#define	OMAP_NAND_TIMEOUT_MS	5000

#define NAND_Ecc_P1e		(1 << 0)
#define NAND_Ecc_P2e		(1 << 1)
#define NAND_Ecc_P4e		(1 << 2)
#define NAND_Ecc_P8e		(1 << 3)
#define NAND_Ecc_P16e		(1 << 4)
#define NAND_Ecc_P32e		(1 << 5)
#define NAND_Ecc_P64e		(1 << 6)
#define NAND_Ecc_P128e		(1 << 7)
#define NAND_Ecc_P256e		(1 << 8)
#define NAND_Ecc_P512e		(1 << 9)
#define NAND_Ecc_P1024e		(1 << 10)
#define NAND_Ecc_P2048e		(1 << 11)

#define NAND_Ecc_P1o		(1 << 16)
#define NAND_Ecc_P2o		(1 << 17)
#define NAND_Ecc_P4o		(1 << 18)
#define NAND_Ecc_P8o		(1 << 19)
#define NAND_Ecc_P16o		(1 << 20)
#define NAND_Ecc_P32o		(1 << 21)
#define NAND_Ecc_P64o		(1 << 22)
#define NAND_Ecc_P128o		(1 << 23)
#define NAND_Ecc_P256o		(1 << 24)
#define NAND_Ecc_P512o		(1 << 25)
#define NAND_Ecc_P1024o		(1 << 26)
#define NAND_Ecc_P2048o		(1 << 27)

#define TF(value)	(value ? 1 : 0)

#define P2048e(a)	(TF(a & NAND_Ecc_P2048e)	<< 0)
#define P2048o(a)	(TF(a & NAND_Ecc_P2048o)	<< 1)
#define P1e(a)		(TF(a & NAND_Ecc_P1e)		<< 2)
#define P1o(a)		(TF(a & NAND_Ecc_P1o)		<< 3)
#define P2e(a)		(TF(a & NAND_Ecc_P2e)		<< 4)
#define P2o(a)		(TF(a & NAND_Ecc_P2o)		<< 5)
#define P4e(a)		(TF(a & NAND_Ecc_P4e)		<< 6)
#define P4o(a)		(TF(a & NAND_Ecc_P4o)		<< 7)

#define P8e(a)		(TF(a & NAND_Ecc_P8e)		<< 0)
#define P8o(a)		(TF(a & NAND_Ecc_P8o)		<< 1)
#define P16e(a)		(TF(a & NAND_Ecc_P16e)		<< 2)
#define P16o(a)		(TF(a & NAND_Ecc_P16o)		<< 3)
#define P32e(a)		(TF(a & NAND_Ecc_P32e)		<< 4)
#define P32o(a)		(TF(a & NAND_Ecc_P32o)		<< 5)
#define P64e(a)		(TF(a & NAND_Ecc_P64e)		<< 6)
#define P64o(a)		(TF(a & NAND_Ecc_P64o)		<< 7)

#define P128e(a)	(TF(a & NAND_Ecc_P128e)		<< 0)
#define P128o(a)	(TF(a & NAND_Ecc_P128o)		<< 1)
#define P256e(a)	(TF(a & NAND_Ecc_P256e)		<< 2)
#define P256o(a)	(TF(a & NAND_Ecc_P256o)		<< 3)
#define P512e(a)	(TF(a & NAND_Ecc_P512e)		<< 4)
#define P512o(a)	(TF(a & NAND_Ecc_P512o)		<< 5)
#define P1024e(a)	(TF(a & NAND_Ecc_P1024e)	<< 6)
#define P1024o(a)	(TF(a & NAND_Ecc_P1024o)	<< 7)

#define P8e_s(a)	(TF(a & NAND_Ecc_P8e)		<< 0)
#define P8o_s(a)	(TF(a & NAND_Ecc_P8o)		<< 1)
#define P16e_s(a)	(TF(a & NAND_Ecc_P16e)		<< 2)
#define P16o_s(a)	(TF(a & NAND_Ecc_P16o)		<< 3)
#define P1e_s(a)	(TF(a & NAND_Ecc_P1e)		<< 4)
#define P1o_s(a)	(TF(a & NAND_Ecc_P1o)		<< 5)
#define P2e_s(a)	(TF(a & NAND_Ecc_P2e)		<< 6)
#define P2o_s(a)	(TF(a & NAND_Ecc_P2o)		<< 7)

#define P4e_s(a)	(TF(a & NAND_Ecc_P4e)		<< 0)
#define P4o_s(a)	(TF(a & NAND_Ecc_P4o)		<< 1)

static struct nand_ecclayout omap_oobinfo;
static uint8_t scan_ff_pattern[] = { 0xff };
static struct nand_bbt_descr bb_descrip_flashbased = {
	.options = NAND_BBT_SCANEMPTY | NAND_BBT_SCANALLPAGES,
	.offs = 0,
	.len = 1,
	.pattern = scan_ff_pattern,
};


struct omap_nand_info {
	struct nand_hw_control		controller;
	struct omap_nand_platform_data	*pdata;
	struct mtd_info			mtd;
	struct nand_chip		nand;
	struct platform_device		*pdev;

	int				gpmc_cs;
	unsigned long			phys_base;
	struct completion		comp;
	int				dma_ch;
	int				gpmc_irq;
	enum {
		OMAP_NAND_IO_READ = 0,	
		OMAP_NAND_IO_WRITE,	
	} iomode;
	u_char				*buf;
	int					buf_len;
};

static void omap_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct omap_nand_info *info = container_of(mtd,
					struct omap_nand_info, mtd);

	if (cmd != NAND_CMD_NONE) {
		if (ctrl & NAND_CLE)
			gpmc_nand_write(info->gpmc_cs, GPMC_NAND_COMMAND, cmd);

		else if (ctrl & NAND_ALE)
			gpmc_nand_write(info->gpmc_cs, GPMC_NAND_ADDRESS, cmd);

		else 
			gpmc_nand_write(info->gpmc_cs, GPMC_NAND_DATA, cmd);
	}
}

static void omap_read_buf8(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *nand = mtd->priv;

	ioread8_rep(nand->IO_ADDR_R, buf, len);
}

static void omap_write_buf8(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct omap_nand_info *info = container_of(mtd,
						struct omap_nand_info, mtd);
	u_char *p = (u_char *)buf;
	u32	status = 0;

	while (len--) {
		iowrite8(*p++, info->nand.IO_ADDR_W);
		
		do {
			status = gpmc_read_status(GPMC_STATUS_BUFFER);
		} while (!status);
	}
}

static void omap_read_buf16(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *nand = mtd->priv;

	ioread16_rep(nand->IO_ADDR_R, buf, len / 2);
}

static void omap_write_buf16(struct mtd_info *mtd, const u_char * buf, int len)
{
	struct omap_nand_info *info = container_of(mtd,
						struct omap_nand_info, mtd);
	u16 *p = (u16 *) buf;
	u32	status = 0;
	
	len >>= 1;

	while (len--) {
		iowrite16(*p++, info->nand.IO_ADDR_W);
		
		do {
			status = gpmc_read_status(GPMC_STATUS_BUFFER);
		} while (!status);
	}
}

static void omap_read_buf_pref(struct mtd_info *mtd, u_char *buf, int len)
{
	struct omap_nand_info *info = container_of(mtd,
						struct omap_nand_info, mtd);
	uint32_t r_count = 0;
	int ret = 0;
	u32 *p = (u32 *)buf;

	
	if (len % 4) {
		if (info->nand.options & NAND_BUSWIDTH_16)
			omap_read_buf16(mtd, buf, len % 4);
		else
			omap_read_buf8(mtd, buf, len % 4);
		p = (u32 *) (buf + len % 4);
		len -= len % 4;
	}

	
	ret = gpmc_prefetch_enable(info->gpmc_cs,
			PREFETCH_FIFOTHRESHOLD_MAX, 0x0, len, 0x0);
	if (ret) {
		
		if (info->nand.options & NAND_BUSWIDTH_16)
			omap_read_buf16(mtd, (u_char *)p, len);
		else
			omap_read_buf8(mtd, (u_char *)p, len);
	} else {
		do {
			r_count = gpmc_read_status(GPMC_PREFETCH_FIFO_CNT);
			r_count = r_count >> 2;
			ioread32_rep(info->nand.IO_ADDR_R, p, r_count);
			p += r_count;
			len -= r_count << 2;
		} while (len);
		
		gpmc_prefetch_reset(info->gpmc_cs);
	}
}

static void omap_write_buf_pref(struct mtd_info *mtd,
					const u_char *buf, int len)
{
	struct omap_nand_info *info = container_of(mtd,
						struct omap_nand_info, mtd);
	uint32_t w_count = 0;
	int i = 0, ret = 0;
	u16 *p = (u16 *)buf;
	unsigned long tim, limit;

	
	if (len % 2 != 0) {
		writeb(*buf, info->nand.IO_ADDR_W);
		p = (u16 *)(buf + 1);
		len--;
	}

	
	ret = gpmc_prefetch_enable(info->gpmc_cs,
			PREFETCH_FIFOTHRESHOLD_MAX, 0x0, len, 0x1);
	if (ret) {
		
		if (info->nand.options & NAND_BUSWIDTH_16)
			omap_write_buf16(mtd, (u_char *)p, len);
		else
			omap_write_buf8(mtd, (u_char *)p, len);
	} else {
		while (len) {
			w_count = gpmc_read_status(GPMC_PREFETCH_FIFO_CNT);
			w_count = w_count >> 1;
			for (i = 0; (i < w_count) && len; i++, len -= 2)
				iowrite16(*p++, info->nand.IO_ADDR_W);
		}
		
		tim = 0;
		limit = (loops_per_jiffy *
					msecs_to_jiffies(OMAP_NAND_TIMEOUT_MS));
		while (gpmc_read_status(GPMC_PREFETCH_COUNT) && (tim++ < limit))
			cpu_relax();

		
		gpmc_prefetch_reset(info->gpmc_cs);
	}
}

static void omap_nand_dma_cb(int lch, u16 ch_status, void *data)
{
	complete((struct completion *) data);
}

static inline int omap_nand_dma_transfer(struct mtd_info *mtd, void *addr,
					unsigned int len, int is_write)
{
	struct omap_nand_info *info = container_of(mtd,
					struct omap_nand_info, mtd);
	enum dma_data_direction dir = is_write ? DMA_TO_DEVICE :
							DMA_FROM_DEVICE;
	dma_addr_t dma_addr;
	int ret;
	unsigned long tim, limit;

	int buf_len = len >> 6;

	if (addr >= high_memory) {
		struct page *p1;

		if (((size_t)addr & PAGE_MASK) !=
			((size_t)(addr + len - 1) & PAGE_MASK))
			goto out_copy;
		p1 = vmalloc_to_page(addr);
		if (!p1)
			goto out_copy;
		addr = page_address(p1) + ((size_t)addr & ~PAGE_MASK);
	}

	dma_addr = dma_map_single(&info->pdev->dev, addr, len, dir);
	if (dma_mapping_error(&info->pdev->dev, dma_addr)) {
		dev_err(&info->pdev->dev,
			"Couldn't DMA map a %d byte buffer\n", len);
		goto out_copy;
	}

	if (is_write) {
	    omap_set_dma_dest_params(info->dma_ch, 0, OMAP_DMA_AMODE_CONSTANT,
						info->phys_base, 0, 0);
	    omap_set_dma_src_params(info->dma_ch, 0, OMAP_DMA_AMODE_POST_INC,
							dma_addr, 0, 0);
	    omap_set_dma_transfer_params(info->dma_ch, OMAP_DMA_DATA_TYPE_S32,
					0x10, buf_len, OMAP_DMA_SYNC_FRAME,
					OMAP24XX_DMA_GPMC, OMAP_DMA_DST_SYNC);
	} else {
	    omap_set_dma_src_params(info->dma_ch, 0, OMAP_DMA_AMODE_CONSTANT,
						info->phys_base, 0, 0);
	    omap_set_dma_dest_params(info->dma_ch, 0, OMAP_DMA_AMODE_POST_INC,
							dma_addr, 0, 0);
	    omap_set_dma_transfer_params(info->dma_ch, OMAP_DMA_DATA_TYPE_S32,
					0x10, buf_len, OMAP_DMA_SYNC_FRAME,
					OMAP24XX_DMA_GPMC, OMAP_DMA_SRC_SYNC);
	}
	
	ret = gpmc_prefetch_enable(info->gpmc_cs,
			PREFETCH_FIFOTHRESHOLD_MAX, 0x1, len, is_write);
	if (ret)
		
		goto out_copy;

	init_completion(&info->comp);

	omap_start_dma(info->dma_ch);

	
	wait_for_completion(&info->comp);
	tim = 0;
	limit = (loops_per_jiffy * msecs_to_jiffies(OMAP_NAND_TIMEOUT_MS));
	while (gpmc_read_status(GPMC_PREFETCH_COUNT) && (tim++ < limit))
		cpu_relax();

	
	gpmc_prefetch_reset(info->gpmc_cs);

	dma_unmap_single(&info->pdev->dev, dma_addr, len, dir);
	return 0;

out_copy:
	if (info->nand.options & NAND_BUSWIDTH_16)
		is_write == 0 ? omap_read_buf16(mtd, (u_char *) addr, len)
			: omap_write_buf16(mtd, (u_char *) addr, len);
	else
		is_write == 0 ? omap_read_buf8(mtd, (u_char *) addr, len)
			: omap_write_buf8(mtd, (u_char *) addr, len);
	return 0;
}

static void omap_read_buf_dma_pref(struct mtd_info *mtd, u_char *buf, int len)
{
	if (len <= mtd->oobsize)
		omap_read_buf_pref(mtd, buf, len);
	else
		
		omap_nand_dma_transfer(mtd, buf, len, 0x0);
}

static void omap_write_buf_dma_pref(struct mtd_info *mtd,
					const u_char *buf, int len)
{
	if (len <= mtd->oobsize)
		omap_write_buf_pref(mtd, buf, len);
	else
		
		omap_nand_dma_transfer(mtd, (u_char *) buf, len, 0x1);
}

static irqreturn_t omap_nand_irq(int this_irq, void *dev)
{
	struct omap_nand_info *info = (struct omap_nand_info *) dev;
	u32 bytes;
	u32 irq_stat;

	irq_stat = gpmc_read_status(GPMC_GET_IRQ_STATUS);
	bytes = gpmc_read_status(GPMC_PREFETCH_FIFO_CNT);
	bytes = bytes  & 0xFFFC; 
	if (info->iomode == OMAP_NAND_IO_WRITE) { 
		if (irq_stat & 0x2)
			goto done;

		if (info->buf_len && (info->buf_len < bytes))
			bytes = info->buf_len;
		else if (!info->buf_len)
			bytes = 0;
		iowrite32_rep(info->nand.IO_ADDR_W,
						(u32 *)info->buf, bytes >> 2);
		info->buf = info->buf + bytes;
		info->buf_len -= bytes;

	} else {
		ioread32_rep(info->nand.IO_ADDR_R,
						(u32 *)info->buf, bytes >> 2);
		info->buf = info->buf + bytes;

		if (irq_stat & 0x2)
			goto done;
	}
	gpmc_cs_configure(info->gpmc_cs, GPMC_SET_IRQ_STATUS, irq_stat);

	return IRQ_HANDLED;

done:
	complete(&info->comp);
	
	gpmc_cs_configure(info->gpmc_cs, GPMC_ENABLE_IRQ, 0);

	
	gpmc_cs_configure(info->gpmc_cs, GPMC_SET_IRQ_STATUS, irq_stat);

	return IRQ_HANDLED;
}

static void omap_read_buf_irq_pref(struct mtd_info *mtd, u_char *buf, int len)
{
	struct omap_nand_info *info = container_of(mtd,
						struct omap_nand_info, mtd);
	int ret = 0;

	if (len <= mtd->oobsize) {
		omap_read_buf_pref(mtd, buf, len);
		return;
	}

	info->iomode = OMAP_NAND_IO_READ;
	info->buf = buf;
	init_completion(&info->comp);

	
	ret = gpmc_prefetch_enable(info->gpmc_cs,
			PREFETCH_FIFOTHRESHOLD_MAX/2, 0x0, len, 0x0);
	if (ret)
		
		goto out_copy;

	info->buf_len = len;
	
	gpmc_cs_configure(info->gpmc_cs, GPMC_ENABLE_IRQ,
		(GPMC_IRQ_FIFOEVENTENABLE | GPMC_IRQ_COUNT_EVENT));

	
	wait_for_completion(&info->comp);

	
	gpmc_prefetch_reset(info->gpmc_cs);
	return;

out_copy:
	if (info->nand.options & NAND_BUSWIDTH_16)
		omap_read_buf16(mtd, buf, len);
	else
		omap_read_buf8(mtd, buf, len);
}

static void omap_write_buf_irq_pref(struct mtd_info *mtd,
					const u_char *buf, int len)
{
	struct omap_nand_info *info = container_of(mtd,
						struct omap_nand_info, mtd);
	int ret = 0;
	unsigned long tim, limit;

	if (len <= mtd->oobsize) {
		omap_write_buf_pref(mtd, buf, len);
		return;
	}

	info->iomode = OMAP_NAND_IO_WRITE;
	info->buf = (u_char *) buf;
	init_completion(&info->comp);

	
	ret = gpmc_prefetch_enable(info->gpmc_cs,
			(PREFETCH_FIFOTHRESHOLD_MAX * 3) / 8, 0x0, len, 0x1);
	if (ret)
		
		goto out_copy;

	info->buf_len = len;
	
	gpmc_cs_configure(info->gpmc_cs, GPMC_ENABLE_IRQ,
			(GPMC_IRQ_FIFOEVENTENABLE | GPMC_IRQ_COUNT_EVENT));

	
	wait_for_completion(&info->comp);
	
	tim = 0;
	limit = (loops_per_jiffy *  msecs_to_jiffies(OMAP_NAND_TIMEOUT_MS));
	while (gpmc_read_status(GPMC_PREFETCH_COUNT) && (tim++ < limit))
		cpu_relax();

	
	gpmc_prefetch_reset(info->gpmc_cs);
	return;

out_copy:
	if (info->nand.options & NAND_BUSWIDTH_16)
		omap_write_buf16(mtd, buf, len);
	else
		omap_write_buf8(mtd, buf, len);
}

static int omap_verify_buf(struct mtd_info *mtd, const u_char * buf, int len)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	u16 *p = (u16 *) buf;

	len >>= 1;
	while (len--) {
		if (*p++ != cpu_to_le16(readw(info->nand.IO_ADDR_R)))
			return -EFAULT;
	}

	return 0;
}

static void gen_true_ecc(u8 *ecc_buf)
{
	u32 tmp = ecc_buf[0] | (ecc_buf[1] << 16) |
		((ecc_buf[2] & 0xF0) << 20) | ((ecc_buf[2] & 0x0F) << 8);

	ecc_buf[0] = ~(P64o(tmp) | P64e(tmp) | P32o(tmp) | P32e(tmp) |
			P16o(tmp) | P16e(tmp) | P8o(tmp) | P8e(tmp));
	ecc_buf[1] = ~(P1024o(tmp) | P1024e(tmp) | P512o(tmp) | P512e(tmp) |
			P256o(tmp) | P256e(tmp) | P128o(tmp) | P128e(tmp));
	ecc_buf[2] = ~(P4o(tmp) | P4e(tmp) | P2o(tmp) | P2e(tmp) | P1o(tmp) |
			P1e(tmp) | P2048o(tmp) | P2048e(tmp));
}

static int omap_compare_ecc(u8 *ecc_data1,	
			    u8 *ecc_data2,	
			    u8 *page_data)
{
	uint	i;
	u8	tmp0_bit[8], tmp1_bit[8], tmp2_bit[8];
	u8	comp0_bit[8], comp1_bit[8], comp2_bit[8];
	u8	ecc_bit[24];
	u8	ecc_sum = 0;
	u8	find_bit = 0;
	uint	find_byte = 0;
	int	isEccFF;

	isEccFF = ((*(u32 *)ecc_data1 & 0xFFFFFF) == 0xFFFFFF);

	gen_true_ecc(ecc_data1);
	gen_true_ecc(ecc_data2);

	for (i = 0; i <= 2; i++) {
		*(ecc_data1 + i) = ~(*(ecc_data1 + i));
		*(ecc_data2 + i) = ~(*(ecc_data2 + i));
	}

	for (i = 0; i < 8; i++) {
		tmp0_bit[i]     = *ecc_data1 % 2;
		*ecc_data1	= *ecc_data1 / 2;
	}

	for (i = 0; i < 8; i++) {
		tmp1_bit[i]	 = *(ecc_data1 + 1) % 2;
		*(ecc_data1 + 1) = *(ecc_data1 + 1) / 2;
	}

	for (i = 0; i < 8; i++) {
		tmp2_bit[i]	 = *(ecc_data1 + 2) % 2;
		*(ecc_data1 + 2) = *(ecc_data1 + 2) / 2;
	}

	for (i = 0; i < 8; i++) {
		comp0_bit[i]     = *ecc_data2 % 2;
		*ecc_data2       = *ecc_data2 / 2;
	}

	for (i = 0; i < 8; i++) {
		comp1_bit[i]     = *(ecc_data2 + 1) % 2;
		*(ecc_data2 + 1) = *(ecc_data2 + 1) / 2;
	}

	for (i = 0; i < 8; i++) {
		comp2_bit[i]     = *(ecc_data2 + 2) % 2;
		*(ecc_data2 + 2) = *(ecc_data2 + 2) / 2;
	}

	for (i = 0; i < 6; i++)
		ecc_bit[i] = tmp2_bit[i + 2] ^ comp2_bit[i + 2];

	for (i = 0; i < 8; i++)
		ecc_bit[i + 6] = tmp0_bit[i] ^ comp0_bit[i];

	for (i = 0; i < 8; i++)
		ecc_bit[i + 14] = tmp1_bit[i] ^ comp1_bit[i];

	ecc_bit[22] = tmp2_bit[0] ^ comp2_bit[0];
	ecc_bit[23] = tmp2_bit[1] ^ comp2_bit[1];

	for (i = 0; i < 24; i++)
		ecc_sum += ecc_bit[i];

	switch (ecc_sum) {
	case 0:
		return 0;

	case 1:
		
		pr_debug("ECC UNCORRECTED_ERROR 1\n");
		return -1;

	case 11:
		
		pr_debug("ECC UNCORRECTED_ERROR B\n");
		return -1;

	case 12:
		
		find_byte = (ecc_bit[23] << 8) +
			    (ecc_bit[21] << 7) +
			    (ecc_bit[19] << 6) +
			    (ecc_bit[17] << 5) +
			    (ecc_bit[15] << 4) +
			    (ecc_bit[13] << 3) +
			    (ecc_bit[11] << 2) +
			    (ecc_bit[9]  << 1) +
			    ecc_bit[7];

		find_bit = (ecc_bit[5] << 2) + (ecc_bit[3] << 1) + ecc_bit[1];

		pr_debug("Correcting single bit ECC error at offset: "
				"%d, bit: %d\n", find_byte, find_bit);

		page_data[find_byte] ^= (1 << find_bit);

		return 1;
	default:
		if (isEccFF) {
			if (ecc_data2[0] == 0 &&
			    ecc_data2[1] == 0 &&
			    ecc_data2[2] == 0)
				return 0;
		}
		pr_debug("UNCORRECTED_ERROR default\n");
		return -1;
	}
}

static int omap_correct_data(struct mtd_info *mtd, u_char *dat,
				u_char *read_ecc, u_char *calc_ecc)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	int blockCnt = 0, i = 0, ret = 0;
	int stat = 0;

	
	if ((info->nand.ecc.mode == NAND_ECC_HW) &&
			(info->nand.ecc.size  == 2048))
		blockCnt = 4;
	else
		blockCnt = 1;

	for (i = 0; i < blockCnt; i++) {
		if (memcmp(read_ecc, calc_ecc, 3) != 0) {
			ret = omap_compare_ecc(read_ecc, calc_ecc, dat);
			if (ret < 0)
				return ret;
			
			stat += ret;
		}
		read_ecc += 3;
		calc_ecc += 3;
		dat      += 512;
	}
	return stat;
}

static int omap_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
				u_char *ecc_code)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	return gpmc_calculate_ecc(info->gpmc_cs, dat, ecc_code);
}

static void omap_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	struct nand_chip *chip = mtd->priv;
	unsigned int dev_width = (chip->options & NAND_BUSWIDTH_16) ? 1 : 0;

	gpmc_enable_hwecc(info->gpmc_cs, mode, dev_width, info->nand.ecc.size);
}

static int omap_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct nand_chip *this = mtd->priv;
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);
	unsigned long timeo = jiffies;
	int status = NAND_STATUS_FAIL, state = this->state;

	if (state == FL_ERASING)
		timeo += (HZ * 400) / 1000;
	else
		timeo += (HZ * 20) / 1000;

	gpmc_nand_write(info->gpmc_cs,
			GPMC_NAND_COMMAND, (NAND_CMD_STATUS & 0xFF));
	while (time_before(jiffies, timeo)) {
		status = gpmc_nand_read(info->gpmc_cs, GPMC_NAND_DATA);
		if (status & NAND_STATUS_READY)
			break;
		cond_resched();
	}
	return status;
}

static int omap_dev_ready(struct mtd_info *mtd)
{
	unsigned int val = 0;
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);

	val = gpmc_read_status(GPMC_GET_IRQ_STATUS);
	if ((val & 0x100) == 0x100) {
		
		val |= 0x100;
		val &= ~(0x0);
		gpmc_cs_configure(info->gpmc_cs, GPMC_SET_IRQ_STATUS, val);
	} else {
		unsigned int cnt = 0;
		while (cnt++ < 0x1FF) {
			if  ((val & 0x100) == 0x100)
				return 0;
			val = gpmc_read_status(GPMC_GET_IRQ_STATUS);
		}
	}

	return 1;
}

static int __devinit omap_nand_probe(struct platform_device *pdev)
{
	struct omap_nand_info		*info;
	struct omap_nand_platform_data	*pdata;
	int				err;
	int				i, offset;

	pdata = pdev->dev.platform_data;
	if (pdata == NULL) {
		dev_err(&pdev->dev, "platform data missing\n");
		return -ENODEV;
	}

	info = kzalloc(sizeof(struct omap_nand_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);

	spin_lock_init(&info->controller.lock);
	init_waitqueue_head(&info->controller.wq);

	info->pdev = pdev;

	info->gpmc_cs		= pdata->cs;
	info->phys_base		= pdata->phys_base;

	info->mtd.priv		= &info->nand;
	info->mtd.name		= dev_name(&pdev->dev);
	info->mtd.owner		= THIS_MODULE;

	info->nand.options	= pdata->devsize;
	info->nand.options	|= NAND_SKIP_BBTSCAN;

	
	gpmc_cs_configure(info->gpmc_cs, GPMC_CONFIG_WP, 0);

	if (!request_mem_region(info->phys_base, NAND_IO_SIZE,
				pdev->dev.driver->name)) {
		err = -EBUSY;
		goto out_free_info;
	}

	info->nand.IO_ADDR_R = ioremap(info->phys_base, NAND_IO_SIZE);
	if (!info->nand.IO_ADDR_R) {
		err = -ENOMEM;
		goto out_release_mem_region;
	}

	info->nand.controller = &info->controller;

	info->nand.IO_ADDR_W = info->nand.IO_ADDR_R;
	info->nand.cmd_ctrl  = omap_hwcontrol;

	if (pdata->dev_ready) {
		info->nand.dev_ready = omap_dev_ready;
		info->nand.chip_delay = 0;
	} else {
		info->nand.waitfunc = omap_wait;
		info->nand.chip_delay = 50;
	}

	switch (pdata->xfer_type) {
	case NAND_OMAP_PREFETCH_POLLED:
		info->nand.read_buf   = omap_read_buf_pref;
		info->nand.write_buf  = omap_write_buf_pref;
		break;

	case NAND_OMAP_POLLED:
		if (info->nand.options & NAND_BUSWIDTH_16) {
			info->nand.read_buf   = omap_read_buf16;
			info->nand.write_buf  = omap_write_buf16;
		} else {
			info->nand.read_buf   = omap_read_buf8;
			info->nand.write_buf  = omap_write_buf8;
		}
		break;

	case NAND_OMAP_PREFETCH_DMA:
		err = omap_request_dma(OMAP24XX_DMA_GPMC, "NAND",
				omap_nand_dma_cb, &info->comp, &info->dma_ch);
		if (err < 0) {
			info->dma_ch = -1;
			dev_err(&pdev->dev, "DMA request failed!\n");
			goto out_release_mem_region;
		} else {
			omap_set_dma_dest_burst_mode(info->dma_ch,
					OMAP_DMA_DATA_BURST_16);
			omap_set_dma_src_burst_mode(info->dma_ch,
					OMAP_DMA_DATA_BURST_16);

			info->nand.read_buf   = omap_read_buf_dma_pref;
			info->nand.write_buf  = omap_write_buf_dma_pref;
		}
		break;

	case NAND_OMAP_PREFETCH_IRQ:
		err = request_irq(pdata->gpmc_irq,
				omap_nand_irq, IRQF_SHARED, "gpmc-nand", info);
		if (err) {
			dev_err(&pdev->dev, "requesting irq(%d) error:%d",
							pdata->gpmc_irq, err);
			goto out_release_mem_region;
		} else {
			info->gpmc_irq	     = pdata->gpmc_irq;
			info->nand.read_buf  = omap_read_buf_irq_pref;
			info->nand.write_buf = omap_write_buf_irq_pref;
		}
		break;

	default:
		dev_err(&pdev->dev,
			"xfer_type(%d) not supported!\n", pdata->xfer_type);
		err = -EINVAL;
		goto out_release_mem_region;
	}

	info->nand.verify_buf = omap_verify_buf;

	
	if (pdata->ecc_opt == OMAP_ECC_HAMMING_CODE_DEFAULT)
		info->nand.ecc.mode = NAND_ECC_SOFT;
	else if ((pdata->ecc_opt == OMAP_ECC_HAMMING_CODE_HW) ||
		(pdata->ecc_opt == OMAP_ECC_HAMMING_CODE_HW_ROMCODE)) {
		info->nand.ecc.bytes            = 3;
		info->nand.ecc.size             = 512;
		info->nand.ecc.strength         = 1;
		info->nand.ecc.calculate        = omap_calculate_ecc;
		info->nand.ecc.hwctl            = omap_enable_hwecc;
		info->nand.ecc.correct          = omap_correct_data;
		info->nand.ecc.mode             = NAND_ECC_HW;
	}

	if (nand_scan_ident(&info->mtd, 1, NULL)) {
		info->nand.options ^= NAND_BUSWIDTH_16;
		if (nand_scan_ident(&info->mtd, 1, NULL)) {
			err = -ENXIO;
			goto out_release_mem_region;
		}
	}

	
	if (pdata->ecc_opt == OMAP_ECC_HAMMING_CODE_HW_ROMCODE) {

		if (info->nand.options & NAND_BUSWIDTH_16)
			offset = 2;
		else {
			offset = 1;
			info->nand.badblock_pattern = &bb_descrip_flashbased;
		}
		omap_oobinfo.eccbytes = 3 * (info->mtd.oobsize/16);
		for (i = 0; i < omap_oobinfo.eccbytes; i++)
			omap_oobinfo.eccpos[i] = i+offset;

		omap_oobinfo.oobfree->offset = offset + omap_oobinfo.eccbytes;
		omap_oobinfo.oobfree->length = info->mtd.oobsize -
					(offset + omap_oobinfo.eccbytes);

		info->nand.ecc.layout = &omap_oobinfo;
	}

	
	if (nand_scan_tail(&info->mtd)) {
		err = -ENXIO;
		goto out_release_mem_region;
	}

	mtd_device_parse_register(&info->mtd, NULL, NULL, pdata->parts,
				  pdata->nr_parts);

	platform_set_drvdata(pdev, &info->mtd);

	return 0;

out_release_mem_region:
	release_mem_region(info->phys_base, NAND_IO_SIZE);
out_free_info:
	kfree(info);

	return err;
}

static int omap_nand_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd = platform_get_drvdata(pdev);
	struct omap_nand_info *info = container_of(mtd, struct omap_nand_info,
							mtd);

	platform_set_drvdata(pdev, NULL);
	if (info->dma_ch != -1)
		omap_free_dma(info->dma_ch);

	if (info->gpmc_irq)
		free_irq(info->gpmc_irq, info);

	
	nand_release(&info->mtd);
	iounmap(info->nand.IO_ADDR_R);
	kfree(&info->mtd);
	return 0;
}

static struct platform_driver omap_nand_driver = {
	.probe		= omap_nand_probe,
	.remove		= omap_nand_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(omap_nand_driver);

MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Glue layer for NAND flash on TI OMAP boards");
