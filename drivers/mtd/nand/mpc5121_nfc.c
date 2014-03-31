/*
 * Copyright 2004-2008 Freescale Semiconductor, Inc.
 * Copyright 2009 Semihalf.
 *
 * Approved as OSADL project by a majority of OSADL members and funded
 * by OSADL membership fees in 2009;  for details see www.osadl.org.
 *
 * Based on original driver from Freescale Semiconductor
 * written by John Rigby <jrigby@freescale.com> on basis
 * of drivers/mtd/nand/mxc_nand.c. Reworked and extended
 * Piotr Ziecik <kosmo@semihalf.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/gfp.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include <asm/mpc5121.h>

#define NFC_MAIN_AREA(n)	((n) *  0x200)

#define NFC_SPARE_BUFFERS	8
#define NFC_SPARE_LEN		0x40
#define NFC_SPARE_AREA(n)	(0x1000 + ((n) * NFC_SPARE_LEN))

#define NFC_BUF_ADDR		0x1E04
#define NFC_FLASH_ADDR		0x1E06
#define NFC_FLASH_CMD		0x1E08
#define NFC_CONFIG		0x1E0A
#define NFC_ECC_STATUS1		0x1E0C
#define NFC_ECC_STATUS2		0x1E0E
#define NFC_SPAS		0x1E10
#define NFC_WRPROT		0x1E12
#define NFC_NF_WRPRST		0x1E18
#define NFC_CONFIG1		0x1E1A
#define NFC_CONFIG2		0x1E1C
#define NFC_UNLOCKSTART_BLK0	0x1E20
#define NFC_UNLOCKEND_BLK0	0x1E22
#define NFC_UNLOCKSTART_BLK1	0x1E24
#define NFC_UNLOCKEND_BLK1	0x1E26
#define NFC_UNLOCKSTART_BLK2	0x1E28
#define NFC_UNLOCKEND_BLK2	0x1E2A
#define NFC_UNLOCKSTART_BLK3	0x1E2C
#define NFC_UNLOCKEND_BLK3	0x1E2E

#define NFC_RBA_MASK		(7 << 0)
#define NFC_ACTIVE_CS_SHIFT	5
#define NFC_ACTIVE_CS_MASK	(3 << NFC_ACTIVE_CS_SHIFT)

#define NFC_BLS_UNLOCKED	(1 << 1)

#define NFC_ECC_4BIT		(1 << 0)
#define NFC_FULL_PAGE_DMA	(1 << 1)
#define NFC_SPARE_ONLY		(1 << 2)
#define NFC_ECC_ENABLE		(1 << 3)
#define NFC_INT_MASK		(1 << 4)
#define NFC_BIG_ENDIAN		(1 << 5)
#define NFC_RESET		(1 << 6)
#define NFC_CE			(1 << 7)
#define NFC_ONE_CYCLE		(1 << 8)
#define NFC_PPB_32		(0 << 9)
#define NFC_PPB_64		(1 << 9)
#define NFC_PPB_128		(2 << 9)
#define NFC_PPB_256		(3 << 9)
#define NFC_PPB_MASK		(3 << 9)
#define NFC_FULL_PAGE_INT	(1 << 11)

#define NFC_COMMAND		(1 << 0)
#define NFC_ADDRESS		(1 << 1)
#define NFC_INPUT		(1 << 2)
#define NFC_OUTPUT		(1 << 3)
#define NFC_ID			(1 << 4)
#define NFC_STATUS		(1 << 5)
#define NFC_CMD_FAIL		(1 << 15)
#define NFC_INT			(1 << 15)

#define NFC_WPC_LOCK_TIGHT	(1 << 0)
#define NFC_WPC_LOCK		(1 << 1)
#define NFC_WPC_UNLOCK		(1 << 2)

#define	DRV_NAME		"mpc5121_nfc"

#define NFC_RESET_TIMEOUT	1000		
#define NFC_TIMEOUT		(HZ / 10)	

struct mpc5121_nfc_prv {
	struct mtd_info		mtd;
	struct nand_chip	chip;
	int			irq;
	void __iomem		*regs;
	struct clk		*clk;
	wait_queue_head_t	irq_waitq;
	uint			column;
	int			spareonly;
	void __iomem		*csreg;
	struct device		*dev;
};

static void mpc5121_nfc_done(struct mtd_info *mtd);

static inline u16 nfc_read(struct mtd_info *mtd, uint reg)
{
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;

	return in_be16(prv->regs + reg);
}

static inline void nfc_write(struct mtd_info *mtd, uint reg, u16 val)
{
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;

	out_be16(prv->regs + reg, val);
}

static inline void nfc_set(struct mtd_info *mtd, uint reg, u16 bits)
{
	nfc_write(mtd, reg, nfc_read(mtd, reg) | bits);
}

static inline void nfc_clear(struct mtd_info *mtd, uint reg, u16 bits)
{
	nfc_write(mtd, reg, nfc_read(mtd, reg) & ~bits);
}

static inline void mpc5121_nfc_send_addr(struct mtd_info *mtd, u16 addr)
{
	nfc_write(mtd, NFC_FLASH_ADDR, addr);
	nfc_write(mtd, NFC_CONFIG2, NFC_ADDRESS);
	mpc5121_nfc_done(mtd);
}

static inline void mpc5121_nfc_send_cmd(struct mtd_info *mtd, u16 cmd)
{
	nfc_write(mtd, NFC_FLASH_CMD, cmd);
	nfc_write(mtd, NFC_CONFIG2, NFC_COMMAND);
	mpc5121_nfc_done(mtd);
}

static inline void mpc5121_nfc_send_prog_page(struct mtd_info *mtd)
{
	nfc_clear(mtd, NFC_BUF_ADDR, NFC_RBA_MASK);
	nfc_write(mtd, NFC_CONFIG2, NFC_INPUT);
	mpc5121_nfc_done(mtd);
}

static inline void mpc5121_nfc_send_read_page(struct mtd_info *mtd)
{
	nfc_clear(mtd, NFC_BUF_ADDR, NFC_RBA_MASK);
	nfc_write(mtd, NFC_CONFIG2, NFC_OUTPUT);
	mpc5121_nfc_done(mtd);
}

static inline void mpc5121_nfc_send_read_id(struct mtd_info *mtd)
{
	nfc_clear(mtd, NFC_BUF_ADDR, NFC_RBA_MASK);
	nfc_write(mtd, NFC_CONFIG2, NFC_ID);
	mpc5121_nfc_done(mtd);
}

static inline void mpc5121_nfc_send_read_status(struct mtd_info *mtd)
{
	nfc_clear(mtd, NFC_BUF_ADDR, NFC_RBA_MASK);
	nfc_write(mtd, NFC_CONFIG2, NFC_STATUS);
	mpc5121_nfc_done(mtd);
}

static irqreturn_t mpc5121_nfc_irq(int irq, void *data)
{
	struct mtd_info *mtd = data;
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;

	nfc_set(mtd, NFC_CONFIG1, NFC_INT_MASK);
	wake_up(&prv->irq_waitq);

	return IRQ_HANDLED;
}

static void mpc5121_nfc_done(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;
	int rv;

	if ((nfc_read(mtd, NFC_CONFIG2) & NFC_INT) == 0) {
		nfc_clear(mtd, NFC_CONFIG1, NFC_INT_MASK);
		rv = wait_event_timeout(prv->irq_waitq,
			(nfc_read(mtd, NFC_CONFIG2) & NFC_INT), NFC_TIMEOUT);

		if (!rv)
			dev_warn(prv->dev,
				"Timeout while waiting for interrupt.\n");
	}

	nfc_clear(mtd, NFC_CONFIG2, NFC_INT);
}

static void mpc5121_nfc_addr_cycle(struct mtd_info *mtd, int column, int page)
{
	struct nand_chip *chip = mtd->priv;
	u32 pagemask = chip->pagemask;

	if (column != -1) {
		mpc5121_nfc_send_addr(mtd, column);
		if (mtd->writesize > 512)
			mpc5121_nfc_send_addr(mtd, column >> 8);
	}

	if (page != -1) {
		do {
			mpc5121_nfc_send_addr(mtd, page & 0xFF);
			page >>= 8;
			pagemask >>= 8;
		} while (pagemask);
	}
}

static void mpc5121_nfc_select_chip(struct mtd_info *mtd, int chip)
{
	if (chip < 0) {
		nfc_clear(mtd, NFC_CONFIG1, NFC_CE);
		return;
	}

	nfc_clear(mtd, NFC_BUF_ADDR, NFC_ACTIVE_CS_MASK);
	nfc_set(mtd, NFC_BUF_ADDR, (chip << NFC_ACTIVE_CS_SHIFT) &
							NFC_ACTIVE_CS_MASK);
	nfc_set(mtd, NFC_CONFIG1, NFC_CE);
}

static int ads5121_chipselect_init(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;
	struct device_node *dn;

	dn = of_find_compatible_node(NULL, NULL, "fsl,mpc5121ads-cpld");
	if (dn) {
		prv->csreg = of_iomap(dn, 0);
		of_node_put(dn);
		if (!prv->csreg)
			return -ENOMEM;

		
		prv->csreg += 9;
		return 0;
	}

	return -EINVAL;
}

static void ads5121_select_chip(struct mtd_info *mtd, int chip)
{
	struct nand_chip *nand = mtd->priv;
	struct mpc5121_nfc_prv *prv = nand->priv;
	u8 v;

	v = in_8(prv->csreg);
	v |= 0x0F;

	if (chip >= 0) {
		mpc5121_nfc_select_chip(mtd, 0);
		v &= ~(1 << chip);
	} else
		mpc5121_nfc_select_chip(mtd, -1);

	out_8(prv->csreg, v);
}

static int mpc5121_nfc_dev_ready(struct mtd_info *mtd)
{
	return 1;
}

static void mpc5121_nfc_command(struct mtd_info *mtd, unsigned command,
							int column, int page)
{
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;

	prv->column = (column >= 0) ? column : 0;
	prv->spareonly = 0;

	switch (command) {
	case NAND_CMD_PAGEPROG:
		mpc5121_nfc_send_prog_page(mtd);
		break;
	case NAND_CMD_READ0:
		column = 0;
		break;

	case NAND_CMD_READ1:
		prv->column += 256;
		command = NAND_CMD_READ0;
		column = 0;
		break;

	case NAND_CMD_READOOB:
		prv->spareonly = 1;
		command = NAND_CMD_READ0;
		column = 0;
		break;

	case NAND_CMD_SEQIN:
		mpc5121_nfc_command(mtd, NAND_CMD_READ0, column, page);
		column = 0;
		break;

	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_READID:
	case NAND_CMD_STATUS:
		break;

	default:
		return;
	}

	mpc5121_nfc_send_cmd(mtd, command);
	mpc5121_nfc_addr_cycle(mtd, column, page);

	switch (command) {
	case NAND_CMD_READ0:
		if (mtd->writesize > 512)
			mpc5121_nfc_send_cmd(mtd, NAND_CMD_READSTART);
		mpc5121_nfc_send_read_page(mtd);
		break;

	case NAND_CMD_READID:
		mpc5121_nfc_send_read_id(mtd);
		break;

	case NAND_CMD_STATUS:
		mpc5121_nfc_send_read_status(mtd);
		if (chip->options & NAND_BUSWIDTH_16)
			prv->column = 1;
		else
			prv->column = 0;
		break;
	}
}

static void mpc5121_nfc_copy_spare(struct mtd_info *mtd, uint offset,
						u8 *buffer, uint size, int wr)
{
	struct nand_chip *nand = mtd->priv;
	struct mpc5121_nfc_prv *prv = nand->priv;
	uint o, s, sbsize, blksize;


	
	sbsize = (mtd->oobsize / (mtd->writesize / 512)) & ~1;

	while (size) {
		
		s = offset / sbsize;
		if (s > NFC_SPARE_BUFFERS - 1)
			s = NFC_SPARE_BUFFERS - 1;

		o = offset - (s * sbsize);
		blksize = min(sbsize - o, size);

		if (wr)
			memcpy_toio(prv->regs + NFC_SPARE_AREA(s) + o,
							buffer, blksize);
		else
			memcpy_fromio(buffer,
				prv->regs + NFC_SPARE_AREA(s) + o, blksize);

		buffer += blksize;
		offset += blksize;
		size -= blksize;
	};
}

static void mpc5121_nfc_buf_copy(struct mtd_info *mtd, u_char *buf, int len,
									int wr)
{
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;
	uint c = prv->column;
	uint l;

	
	if (prv->spareonly || c >= mtd->writesize) {
		
		if (c >= mtd->writesize)
			c -= mtd->writesize;

		prv->column += len;
		mpc5121_nfc_copy_spare(mtd, c, buf, len, wr);
		return;
	}

	l = min((uint)len, mtd->writesize - c);
	prv->column += l;

	if (wr)
		memcpy_toio(prv->regs + NFC_MAIN_AREA(0) + c, buf, l);
	else
		memcpy_fromio(buf, prv->regs + NFC_MAIN_AREA(0) + c, l);

	
	if (l != len) {
		buf += l;
		len -= l;
		mpc5121_nfc_buf_copy(mtd, buf, len, wr);
	}
}

static void mpc5121_nfc_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	mpc5121_nfc_buf_copy(mtd, buf, len, 0);
}

static void mpc5121_nfc_write_buf(struct mtd_info *mtd,
						const u_char *buf, int len)
{
	mpc5121_nfc_buf_copy(mtd, (u_char *)buf, len, 1);
}

static int mpc5121_nfc_verify_buf(struct mtd_info *mtd,
						const u_char *buf, int len)
{
	u_char tmp[256];
	uint bsize;

	while (len) {
		bsize = min(len, 256);
		mpc5121_nfc_read_buf(mtd, tmp, bsize);

		if (memcmp(buf, tmp, bsize))
			return 1;

		buf += bsize;
		len -= bsize;
	}

	return 0;
}

static u8 mpc5121_nfc_read_byte(struct mtd_info *mtd)
{
	u8 tmp;

	mpc5121_nfc_read_buf(mtd, &tmp, sizeof(tmp));

	return tmp;
}

static u16 mpc5121_nfc_read_word(struct mtd_info *mtd)
{
	u16 tmp;

	mpc5121_nfc_read_buf(mtd, (u_char *)&tmp, sizeof(tmp));

	return tmp;
}

static int mpc5121_nfc_read_hw_config(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;
	struct mpc512x_reset_module *rm;
	struct device_node *rmnode;
	uint rcw_pagesize = 0;
	uint rcw_sparesize = 0;
	uint rcw_width;
	uint rcwh;
	uint romloc, ps;
	int ret = 0;

	rmnode = of_find_compatible_node(NULL, NULL, "fsl,mpc5121-reset");
	if (!rmnode) {
		dev_err(prv->dev, "Missing 'fsl,mpc5121-reset' "
					"node in device tree!\n");
		return -ENODEV;
	}

	rm = of_iomap(rmnode, 0);
	if (!rm) {
		dev_err(prv->dev, "Error mapping reset module node!\n");
		ret = -EBUSY;
		goto out;
	}

	rcwh = in_be32(&rm->rcwhr);

	
	rcw_width = ((rcwh >> 6) & 0x1) ? 2 : 1;

	
	ps = (rcwh >> 7) & 0x1;

	
	romloc = (rcwh >> 21) & 0x3;

	
	switch ((ps << 2) | romloc) {
	case 0x00:
	case 0x01:
		rcw_pagesize = 512;
		rcw_sparesize = 16;
		break;
	case 0x02:
	case 0x03:
		rcw_pagesize = 4096;
		rcw_sparesize = 128;
		break;
	case 0x04:
	case 0x05:
		rcw_pagesize = 2048;
		rcw_sparesize = 64;
		break;
	case 0x06:
	case 0x07:
		rcw_pagesize = 4096;
		rcw_sparesize = 218;
		break;
	}

	mtd->writesize = rcw_pagesize;
	mtd->oobsize = rcw_sparesize;
	if (rcw_width == 2)
		chip->options |= NAND_BUSWIDTH_16;

	dev_notice(prv->dev, "Configured for "
				"%u-bit NAND, page size %u "
				"with %u spare.\n",
				rcw_width * 8, rcw_pagesize,
				rcw_sparesize);
	iounmap(rm);
out:
	of_node_put(rmnode);
	return ret;
}

static void mpc5121_nfc_free(struct device *dev, struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;

	if (prv->clk) {
		clk_disable(prv->clk);
		clk_put(prv->clk);
	}

	if (prv->csreg)
		iounmap(prv->csreg);
}

static int __devinit mpc5121_nfc_probe(struct platform_device *op)
{
	struct device_node *rootnode, *dn = op->dev.of_node;
	struct device *dev = &op->dev;
	struct mpc5121_nfc_prv *prv;
	struct resource res;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	unsigned long regs_paddr, regs_size;
	const __be32 *chips_no;
	int resettime = 0;
	int retval = 0;
	int rev, len;
	struct mtd_part_parser_data ppdata;

	rev = (mfspr(SPRN_SVR) >> 4) & 0xF;
	if ((rev != 2) && (rev != 3)) {
		dev_err(dev, "SoC revision %u is not supported!\n", rev);
		return -ENXIO;
	}

	prv = devm_kzalloc(dev, sizeof(*prv), GFP_KERNEL);
	if (!prv) {
		dev_err(dev, "Memory exhausted!\n");
		return -ENOMEM;
	}

	mtd = &prv->mtd;
	chip = &prv->chip;

	mtd->priv = chip;
	chip->priv = prv;
	prv->dev = dev;

	
	retval = mpc5121_nfc_read_hw_config(mtd);
	if (retval) {
		dev_err(dev, "Unable to read NFC config!\n");
		return retval;
	}

	prv->irq = irq_of_parse_and_map(dn, 0);
	if (prv->irq == NO_IRQ) {
		dev_err(dev, "Error mapping IRQ!\n");
		return -EINVAL;
	}

	retval = of_address_to_resource(dn, 0, &res);
	if (retval) {
		dev_err(dev, "Error parsing memory region!\n");
		return retval;
	}

	chips_no = of_get_property(dn, "chips", &len);
	if (!chips_no || len != sizeof(*chips_no)) {
		dev_err(dev, "Invalid/missing 'chips' property!\n");
		return -EINVAL;
	}

	regs_paddr = res.start;
	regs_size = resource_size(&res);

	if (!devm_request_mem_region(dev, regs_paddr, regs_size, DRV_NAME)) {
		dev_err(dev, "Error requesting memory region!\n");
		return -EBUSY;
	}

	prv->regs = devm_ioremap(dev, regs_paddr, regs_size);
	if (!prv->regs) {
		dev_err(dev, "Error mapping memory region!\n");
		return -ENOMEM;
	}

	mtd->name = "MPC5121 NAND";
	ppdata.of_node = dn;
	chip->dev_ready = mpc5121_nfc_dev_ready;
	chip->cmdfunc = mpc5121_nfc_command;
	chip->read_byte = mpc5121_nfc_read_byte;
	chip->read_word = mpc5121_nfc_read_word;
	chip->read_buf = mpc5121_nfc_read_buf;
	chip->write_buf = mpc5121_nfc_write_buf;
	chip->verify_buf = mpc5121_nfc_verify_buf;
	chip->select_chip = mpc5121_nfc_select_chip;
	chip->options = NAND_NO_AUTOINCR;
	chip->bbt_options = NAND_BBT_USE_FLASH;
	chip->ecc.mode = NAND_ECC_SOFT;

	
	rootnode = of_find_node_by_path("/");
	if (of_device_is_compatible(rootnode, "fsl,mpc5121ads")) {
		retval = ads5121_chipselect_init(mtd);
		if (retval) {
			dev_err(dev, "Chipselect init error!\n");
			of_node_put(rootnode);
			return retval;
		}

		chip->select_chip = ads5121_select_chip;
	}
	of_node_put(rootnode);

	
	prv->clk = clk_get(dev, "nfc_clk");
	if (IS_ERR(prv->clk)) {
		dev_err(dev, "Unable to acquire NFC clock!\n");
		retval = PTR_ERR(prv->clk);
		goto error;
	}

	clk_enable(prv->clk);

	
	nfc_set(mtd, NFC_CONFIG1, NFC_RESET);
	while (nfc_read(mtd, NFC_CONFIG1) & NFC_RESET) {
		if (resettime++ >= NFC_RESET_TIMEOUT) {
			dev_err(dev, "Timeout while resetting NFC!\n");
			retval = -EINVAL;
			goto error;
		}

		udelay(1);
	}

	
	nfc_write(mtd, NFC_CONFIG, NFC_BLS_UNLOCKED);

	
	nfc_write(mtd, NFC_UNLOCKSTART_BLK0, 0x0000);
	nfc_write(mtd, NFC_UNLOCKEND_BLK0, 0xFFFF);
	nfc_write(mtd, NFC_WRPROT, NFC_WPC_UNLOCK);

	nfc_write(mtd, NFC_CONFIG1, NFC_BIG_ENDIAN | NFC_INT_MASK |
							NFC_FULL_PAGE_INT);

	
	nfc_write(mtd, NFC_SPAS, mtd->oobsize >> 1);

	init_waitqueue_head(&prv->irq_waitq);
	retval = devm_request_irq(dev, prv->irq, &mpc5121_nfc_irq, 0, DRV_NAME,
									mtd);
	if (retval) {
		dev_err(dev, "Error requesting IRQ!\n");
		goto error;
	}

	
	if (nand_scan(mtd, be32_to_cpup(chips_no))) {
		dev_err(dev, "NAND Flash not found !\n");
		devm_free_irq(dev, prv->irq, mtd);
		retval = -ENXIO;
		goto error;
	}

	
	switch (mtd->erasesize / mtd->writesize) {
	case 32:
		nfc_set(mtd, NFC_CONFIG1, NFC_PPB_32);
		break;

	case 64:
		nfc_set(mtd, NFC_CONFIG1, NFC_PPB_64);
		break;

	case 128:
		nfc_set(mtd, NFC_CONFIG1, NFC_PPB_128);
		break;

	case 256:
		nfc_set(mtd, NFC_CONFIG1, NFC_PPB_256);
		break;

	default:
		dev_err(dev, "Unsupported NAND flash!\n");
		devm_free_irq(dev, prv->irq, mtd);
		retval = -ENXIO;
		goto error;
	}

	dev_set_drvdata(dev, mtd);

	
	retval = mtd_device_parse_register(mtd, NULL, &ppdata, NULL, 0);
	if (retval) {
		dev_err(dev, "Error adding MTD device!\n");
		devm_free_irq(dev, prv->irq, mtd);
		goto error;
	}

	return 0;
error:
	mpc5121_nfc_free(dev, mtd);
	return retval;
}

static int __devexit mpc5121_nfc_remove(struct platform_device *op)
{
	struct device *dev = &op->dev;
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct nand_chip *chip = mtd->priv;
	struct mpc5121_nfc_prv *prv = chip->priv;

	nand_release(mtd);
	devm_free_irq(dev, prv->irq, mtd);
	mpc5121_nfc_free(dev, mtd);

	return 0;
}

static struct of_device_id mpc5121_nfc_match[] __devinitdata = {
	{ .compatible = "fsl,mpc5121-nfc", },
	{},
};

static struct platform_driver mpc5121_nfc_driver = {
	.probe		= mpc5121_nfc_probe,
	.remove		= __devexit_p(mpc5121_nfc_remove),
	.driver		= {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mpc5121_nfc_match,
	},
};

module_platform_driver(mpc5121_nfc_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MPC5121 NAND MTD driver");
MODULE_LICENSE("GPL");
