/*
 *  drivers/mtd/nand.c
 *
 *  Overview:
 *   This is the generic MTD driver for NAND flash devices. It should be
 *   capable of working with almost all NAND chips currently available.
 *   Basic support for AG-AND chips is provided.
 *
 *	Additional technical information is available on
 *	http://www.linux-mtd.infradead.org/doc/nand.html
 *
 *  Copyright (C) 2000 Steven J. Hill (sjhill@realitydiluted.com)
 *		  2002-2006 Thomas Gleixner (tglx@linutronix.de)
 *
 *  Credits:
 *	David Woodhouse for adding multichip support
 *
 *	Aleph One Ltd. and Toby Churchill Ltd. for supporting the
 *	rework for 2K page size chips
 *
 *  TODO:
 *	Enable cached programming for 2k page size chips
 *	Check, if mtd->ecctype should be set to MTD_ECC_HW
 *	if we have HW ECC support.
 *	The AG-AND chips have nice features for speed improvement,
 *	which are not supported yet. Read / program 4 pages in one go.
 *	BBT table is not serialized, has to be fixed
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/nand_bch.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/leds.h>
#include <linux/io.h>
#include <linux/mtd/partitions.h>

static struct nand_ecclayout nand_oob_8 = {
	.eccbytes = 3,
	.eccpos = {0, 1, 2},
	.oobfree = {
		{.offset = 3,
		 .length = 2},
		{.offset = 6,
		 .length = 2} }
};

static struct nand_ecclayout nand_oob_16 = {
	.eccbytes = 6,
	.eccpos = {0, 1, 2, 3, 6, 7},
	.oobfree = {
		{.offset = 8,
		 . length = 8} }
};

static struct nand_ecclayout nand_oob_64 = {
	.eccbytes = 24,
	.eccpos = {
		   40, 41, 42, 43, 44, 45, 46, 47,
		   48, 49, 50, 51, 52, 53, 54, 55,
		   56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = {
		{.offset = 2,
		 .length = 38} }
};

static struct nand_ecclayout nand_oob_128 = {
	.eccbytes = 48,
	.eccpos = {
		   80, 81, 82, 83, 84, 85, 86, 87,
		   88, 89, 90, 91, 92, 93, 94, 95,
		   96, 97, 98, 99, 100, 101, 102, 103,
		   104, 105, 106, 107, 108, 109, 110, 111,
		   112, 113, 114, 115, 116, 117, 118, 119,
		   120, 121, 122, 123, 124, 125, 126, 127},
	.oobfree = {
		{.offset = 2,
		 .length = 78} }
};

static int nand_get_device(struct nand_chip *chip, struct mtd_info *mtd,
			   int new_state);

static int nand_do_write_oob(struct mtd_info *mtd, loff_t to,
			     struct mtd_oob_ops *ops);

DEFINE_LED_TRIGGER(nand_led_trigger);

static int check_offs_len(struct mtd_info *mtd,
					loff_t ofs, uint64_t len)
{
	struct nand_chip *chip = mtd->priv;
	int ret = 0;

	
	if (ofs & ((1 << chip->phys_erase_shift) - 1)) {
		pr_debug("%s: unaligned address\n", __func__);
		ret = -EINVAL;
	}

	
	if (len & ((1 << chip->phys_erase_shift) - 1)) {
		pr_debug("%s: length not block aligned\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static void nand_release_device(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	
	chip->select_chip(mtd, -1);

	
	spin_lock(&chip->controller->lock);
	chip->controller->active = NULL;
	chip->state = FL_READY;
	wake_up(&chip->controller->wq);
	spin_unlock(&chip->controller->lock);
}

static uint8_t nand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	return readb(chip->IO_ADDR_R);
}

static uint8_t nand_read_byte16(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	return (uint8_t) cpu_to_le16(readw(chip->IO_ADDR_R));
}

static u16 nand_read_word(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	return readw(chip->IO_ADDR_R);
}

static void nand_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct nand_chip *chip = mtd->priv;

	switch (chipnr) {
	case -1:
		chip->cmd_ctrl(mtd, NAND_CMD_NONE, 0 | NAND_CTRL_CHANGE);
		break;
	case 0:
		break;

	default:
		BUG();
	}
}

static void nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	for (i = 0; i < len; i++)
		writeb(buf[i], chip->IO_ADDR_W);
}

static void nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	for (i = 0; i < len; i++)
		buf[i] = readb(chip->IO_ADDR_R);
}

static int nand_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	for (i = 0; i < len; i++)
		if (buf[i] != readb(chip->IO_ADDR_R))
			return -EFAULT;
	return 0;
}

static void nand_write_buf16(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;

	for (i = 0; i < len; i++)
		writew(p[i], chip->IO_ADDR_W);

}

static void nand_read_buf16(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;

	for (i = 0; i < len; i++)
		p[i] = readw(chip->IO_ADDR_R);
}

static int nand_verify_buf16(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;

	for (i = 0; i < len; i++)
		if (p[i] != readw(chip->IO_ADDR_R))
			return -EFAULT;

	return 0;
}

static int nand_block_bad(struct mtd_info *mtd, loff_t ofs, int getchip)
{
	int page, chipnr, res = 0, i = 0;
	struct nand_chip *chip = mtd->priv;
	u16 bad;

	if (chip->bbt_options & NAND_BBT_SCANLASTPAGE)
		ofs += mtd->erasesize - mtd->writesize;

	page = (int)(ofs >> chip->page_shift) & chip->pagemask;

	if (getchip) {
		chipnr = (int)(ofs >> chip->chip_shift);

		nand_get_device(chip, mtd, FL_READING);

		
		chip->select_chip(mtd, chipnr);
	}

	do {
		if (chip->options & NAND_BUSWIDTH_16) {
			chip->cmdfunc(mtd, NAND_CMD_READOOB,
					chip->badblockpos & 0xFE, page);
			bad = cpu_to_le16(chip->read_word(mtd));
			if (chip->badblockpos & 0x1)
				bad >>= 8;
			else
				bad &= 0xFF;
		} else {
			chip->cmdfunc(mtd, NAND_CMD_READOOB, chip->badblockpos,
					page);
			bad = chip->read_byte(mtd);
		}

		if (likely(chip->badblockbits == 8))
			res = bad != 0xFF;
		else
			res = hweight8(bad) < chip->badblockbits;
		ofs += mtd->writesize;
		page = (int)(ofs >> chip->page_shift) & chip->pagemask;
		i++;
	} while (!res && i < 2 && (chip->bbt_options & NAND_BBT_SCAN2NDPAGE));

	if (getchip)
		nand_release_device(mtd);

	return res;
}

/**
 * nand_default_block_markbad - [DEFAULT] mark a block bad
 * @mtd: MTD device structure
 * @ofs: offset from device start
 *
 * This is the default implementation, which can be overridden by a hardware
 * specific driver. We try operations in the following order, according to our
 * bbt_options (NAND_BBT_NO_OOB_BBM and NAND_BBT_USE_FLASH):
 *  (1) erase the affected block, to allow OOB marker to be written cleanly
 *  (2) update in-memory BBT
 *  (3) write bad block marker to OOB area of affected block
 *  (4) update flash-based BBT
 * Note that we retain the first error encountered in (3) or (4), finish the
 * procedures, and dump the error in the end.
*/
static int nand_default_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	uint8_t buf[2] = { 0, 0 };
	int block, res, ret = 0, i = 0;
	int write_oob = !(chip->bbt_options & NAND_BBT_NO_OOB_BBM);

	if (write_oob) {
		struct erase_info einfo;

		
		memset(&einfo, 0, sizeof(einfo));
		einfo.mtd = mtd;
		einfo.addr = ofs;
		einfo.len = 1 << chip->phys_erase_shift;
		nand_erase_nand(mtd, &einfo, 0);
	}

	
	block = (int)(ofs >> chip->bbt_erase_shift);
	
	if (chip->bbt)
		chip->bbt[block >> 2] |= 0x01 << ((block & 0x03) << 1);

	
	if (write_oob) {
		struct mtd_oob_ops ops;
		loff_t wr_ofs = ofs;

		nand_get_device(chip, mtd, FL_WRITING);

		ops.datbuf = NULL;
		ops.oobbuf = buf;
		ops.ooboffs = chip->badblockpos;
		if (chip->options & NAND_BUSWIDTH_16) {
			ops.ooboffs &= ~0x01;
			ops.len = ops.ooblen = 2;
		} else {
			ops.len = ops.ooblen = 1;
		}
		ops.mode = MTD_OPS_PLACE_OOB;

		
		if (chip->bbt_options & NAND_BBT_SCANLASTPAGE)
			wr_ofs += mtd->erasesize - mtd->writesize;
		do {
			res = nand_do_write_oob(mtd, wr_ofs, &ops);
			if (!ret)
				ret = res;

			i++;
			wr_ofs += mtd->writesize;
		} while ((chip->bbt_options & NAND_BBT_SCAN2NDPAGE) && i < 2);

		nand_release_device(mtd);
	}

	
	if (chip->bbt_options & NAND_BBT_USE_FLASH) {
		res = nand_update_bbt(mtd, ofs);
		if (!ret)
			ret = res;
	}

	if (!ret)
		mtd->ecc_stats.badblocks++;

	return ret;
}

static int nand_check_wp(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	
	if (chip->options & NAND_BROKEN_XD)
		return 0;

	
	chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);
	return (chip->read_byte(mtd) & NAND_STATUS_WP) ? 0 : 1;
}

static int nand_block_checkbad(struct mtd_info *mtd, loff_t ofs, int getchip,
			       int allowbbt)
{
	struct nand_chip *chip = mtd->priv;

	if (!chip->bbt)
		return chip->block_bad(mtd, ofs, getchip);

	
	return nand_isbad_bbt(mtd, ofs, allowbbt);
}

static void panic_nand_wait_ready(struct mtd_info *mtd, unsigned long timeo)
{
	struct nand_chip *chip = mtd->priv;
	int i;

	
	for (i = 0; i < timeo; i++) {
		if (chip->dev_ready(mtd))
			break;
		touch_softlockup_watchdog();
		mdelay(1);
	}
}

void nand_wait_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	unsigned long timeo = jiffies + 2;

	
	if (in_interrupt() || oops_in_progress)
		return panic_nand_wait_ready(mtd, 400);

	led_trigger_event(nand_led_trigger, LED_FULL);
	
	do {
		if (chip->dev_ready(mtd))
			break;
		touch_softlockup_watchdog();
	} while (time_before(jiffies, timeo));
	led_trigger_event(nand_led_trigger, LED_OFF);
}
EXPORT_SYMBOL_GPL(nand_wait_ready);

static void nand_command(struct mtd_info *mtd, unsigned int command,
			 int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;
	int ctrl = NAND_CTRL_CLE | NAND_CTRL_CHANGE;

	
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
		chip->cmd_ctrl(mtd, readcmd, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
	}
	chip->cmd_ctrl(mtd, command, ctrl);

	
	ctrl = NAND_CTRL_ALE | NAND_CTRL_CHANGE;
	
	if (column != -1) {
		
		if (chip->options & NAND_BUSWIDTH_16)
			column >>= 1;
		chip->cmd_ctrl(mtd, column, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
	}
	if (page_addr != -1) {
		chip->cmd_ctrl(mtd, page_addr, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
		chip->cmd_ctrl(mtd, page_addr >> 8, ctrl);
		
		if (chip->chipsize > (32 << 20))
			chip->cmd_ctrl(mtd, page_addr >> 16, ctrl);
	}
	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	switch (command) {

	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_STATUS:
		return;

	case NAND_CMD_RESET:
		if (chip->dev_ready)
			break;
		udelay(chip->chip_delay);
		chip->cmd_ctrl(mtd, NAND_CMD_STATUS,
			       NAND_CTRL_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd,
			       NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);
		while (!(chip->read_byte(mtd) & NAND_STATUS_READY))
				;
		return;

		
	default:
		if (!chip->dev_ready) {
			udelay(chip->chip_delay);
			return;
		}
	}
	ndelay(100);

	nand_wait_ready(mtd);
}

static void nand_command_lp(struct mtd_info *mtd, unsigned int command,
			    int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;

	
	if (command == NAND_CMD_READOOB) {
		column += mtd->writesize;
		command = NAND_CMD_READ0;
	}

	
	chip->cmd_ctrl(mtd, command & 0xff,
		       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

	if (column != -1 || page_addr != -1) {
		int ctrl = NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE;

		
		if (column != -1) {
			
			if (chip->options & NAND_BUSWIDTH_16)
				column >>= 1;
			chip->cmd_ctrl(mtd, column, ctrl);
			ctrl &= ~NAND_CTRL_CHANGE;
			chip->cmd_ctrl(mtd, column >> 8, ctrl);
		}
		if (page_addr != -1) {
			chip->cmd_ctrl(mtd, page_addr, ctrl);
			chip->cmd_ctrl(mtd, page_addr >> 8,
				       NAND_NCE | NAND_ALE);
			
			if (chip->chipsize > (128 << 20))
				chip->cmd_ctrl(mtd, page_addr >> 16,
					       NAND_NCE | NAND_ALE);
		}
	}
	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	switch (command) {

	case NAND_CMD_CACHEDPROG:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_RNDIN:
	case NAND_CMD_STATUS:
	case NAND_CMD_DEPLETE1:
		return;

	case NAND_CMD_STATUS_ERROR:
	case NAND_CMD_STATUS_ERROR0:
	case NAND_CMD_STATUS_ERROR1:
	case NAND_CMD_STATUS_ERROR2:
	case NAND_CMD_STATUS_ERROR3:
		
		udelay(chip->chip_delay);
		return;

	case NAND_CMD_RESET:
		if (chip->dev_ready)
			break;
		udelay(chip->chip_delay);
		chip->cmd_ctrl(mtd, NAND_CMD_STATUS,
			       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE,
			       NAND_NCE | NAND_CTRL_CHANGE);
		while (!(chip->read_byte(mtd) & NAND_STATUS_READY))
				;
		return;

	case NAND_CMD_RNDOUT:
		
		chip->cmd_ctrl(mtd, NAND_CMD_RNDOUTSTART,
			       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE,
			       NAND_NCE | NAND_CTRL_CHANGE);
		return;

	case NAND_CMD_READ0:
		chip->cmd_ctrl(mtd, NAND_CMD_READSTART,
			       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE,
			       NAND_NCE | NAND_CTRL_CHANGE);

		
	default:
		if (!chip->dev_ready) {
			udelay(chip->chip_delay);
			return;
		}
	}

	ndelay(100);

	nand_wait_ready(mtd);
}

static void panic_nand_get_device(struct nand_chip *chip,
		      struct mtd_info *mtd, int new_state)
{
	
	chip->controller->active = chip;
	chip->state = new_state;
}

static int
nand_get_device(struct nand_chip *chip, struct mtd_info *mtd, int new_state)
{
	spinlock_t *lock = &chip->controller->lock;
	wait_queue_head_t *wq = &chip->controller->wq;
	DECLARE_WAITQUEUE(wait, current);
retry:
	spin_lock(lock);

	
	if (!chip->controller->active)
		chip->controller->active = chip;

	if (chip->controller->active == chip && chip->state == FL_READY) {
		chip->state = new_state;
		spin_unlock(lock);
		return 0;
	}
	if (new_state == FL_PM_SUSPENDED) {
		if (chip->controller->active->state == FL_PM_SUSPENDED) {
			chip->state = FL_PM_SUSPENDED;
			spin_unlock(lock);
			return 0;
		}
	}
	set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(wq, &wait);
	spin_unlock(lock);
	schedule();
	remove_wait_queue(wq, &wait);
	goto retry;
}

static void panic_nand_wait(struct mtd_info *mtd, struct nand_chip *chip,
			    unsigned long timeo)
{
	int i;
	for (i = 0; i < timeo; i++) {
		if (chip->dev_ready) {
			if (chip->dev_ready(mtd))
				break;
		} else {
			if (chip->read_byte(mtd) & NAND_STATUS_READY)
				break;
		}
		mdelay(1);
	}
}

static int nand_wait(struct mtd_info *mtd, struct nand_chip *chip)
{

	unsigned long timeo = jiffies;
	int status, state = chip->state;

	if (state == FL_ERASING)
		timeo += (HZ * 400) / 1000;
	else
		timeo += (HZ * 20) / 1000;

	led_trigger_event(nand_led_trigger, LED_FULL);

	ndelay(100);

	if ((state == FL_ERASING) && (chip->options & NAND_IS_AND))
		chip->cmdfunc(mtd, NAND_CMD_STATUS_MULTI, -1, -1);
	else
		chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);

	if (in_interrupt() || oops_in_progress)
		panic_nand_wait(mtd, chip, timeo);
	else {
		while (time_before(jiffies, timeo)) {
			if (chip->dev_ready) {
				if (chip->dev_ready(mtd))
					break;
			} else {
				if (chip->read_byte(mtd) & NAND_STATUS_READY)
					break;
			}
			cond_resched();
		}
	}
	led_trigger_event(nand_led_trigger, LED_OFF);

	status = (int)chip->read_byte(mtd);
	return status;
}

static int __nand_unlock(struct mtd_info *mtd, loff_t ofs,
					uint64_t len, int invert)
{
	int ret = 0;
	int status, page;
	struct nand_chip *chip = mtd->priv;

	
	page = ofs >> chip->page_shift;
	chip->cmdfunc(mtd, NAND_CMD_UNLOCK1, -1, page & chip->pagemask);

	
	page = (ofs + len) >> chip->page_shift;
	chip->cmdfunc(mtd, NAND_CMD_UNLOCK2, -1,
				(page | invert) & chip->pagemask);

	
	status = chip->waitfunc(mtd, chip);
	
	if (status & 0x01) {
		pr_debug("%s: error status = 0x%08x\n",
					__func__, status);
		ret = -EIO;
	}

	return ret;
}

int nand_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	int ret = 0;
	int chipnr;
	struct nand_chip *chip = mtd->priv;

	pr_debug("%s: start = 0x%012llx, len = %llu\n",
			__func__, (unsigned long long)ofs, len);

	if (check_offs_len(mtd, ofs, len))
		ret = -EINVAL;

	
	if (ofs + len == mtd->size)
		len -= mtd->erasesize;

	nand_get_device(chip, mtd, FL_UNLOCKING);

	
	chipnr = ofs >> chip->chip_shift;

	chip->select_chip(mtd, chipnr);

	
	if (nand_check_wp(mtd)) {
		pr_debug("%s: device is write protected!\n",
					__func__);
		ret = -EIO;
		goto out;
	}

	ret = __nand_unlock(mtd, ofs, len, 0);

out:
	nand_release_device(mtd);

	return ret;
}
EXPORT_SYMBOL(nand_unlock);

int nand_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	int ret = 0;
	int chipnr, status, page;
	struct nand_chip *chip = mtd->priv;

	pr_debug("%s: start = 0x%012llx, len = %llu\n",
			__func__, (unsigned long long)ofs, len);

	if (check_offs_len(mtd, ofs, len))
		ret = -EINVAL;

	nand_get_device(chip, mtd, FL_LOCKING);

	
	chipnr = ofs >> chip->chip_shift;

	chip->select_chip(mtd, chipnr);

	
	if (nand_check_wp(mtd)) {
		pr_debug("%s: device is write protected!\n",
					__func__);
		status = MTD_ERASE_FAILED;
		ret = -EIO;
		goto out;
	}

	
	page = ofs >> chip->page_shift;
	chip->cmdfunc(mtd, NAND_CMD_LOCK, -1, page & chip->pagemask);

	
	status = chip->waitfunc(mtd, chip);
	
	if (status & 0x01) {
		pr_debug("%s: error status = 0x%08x\n",
					__func__, status);
		ret = -EIO;
		goto out;
	}

	ret = __nand_unlock(mtd, ofs, len, 0x1);

out:
	nand_release_device(mtd);

	return ret;
}
EXPORT_SYMBOL(nand_lock);

static int nand_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
			      uint8_t *buf, int page)
{
	chip->read_buf(mtd, buf, mtd->writesize);
	chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);
	return 0;
}

static int nand_read_page_raw_syndrome(struct mtd_info *mtd,
					struct nand_chip *chip,
					uint8_t *buf, int page)
{
	int eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	uint8_t *oob = chip->oob_poi;
	int steps, size;

	for (steps = chip->ecc.steps; steps > 0; steps--) {
		chip->read_buf(mtd, buf, eccsize);
		buf += eccsize;

		if (chip->ecc.prepad) {
			chip->read_buf(mtd, oob, chip->ecc.prepad);
			oob += chip->ecc.prepad;
		}

		chip->read_buf(mtd, oob, eccbytes);
		oob += eccbytes;

		if (chip->ecc.postpad) {
			chip->read_buf(mtd, oob, chip->ecc.postpad);
			oob += chip->ecc.postpad;
		}
	}

	size = mtd->oobsize - (oob - chip->oob_poi);
	if (size)
		chip->read_buf(mtd, oob, size);

	return 0;
}

static int nand_read_page_swecc(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int page)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	chip->ecc.read_page_raw(mtd, chip, buf, page);

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);

	for (i = 0; i < chip->ecc.total; i++)
		ecc_code[i] = chip->oob_poi[eccpos[i]];

	eccsteps = chip->ecc.steps;
	p = buf;

	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		stat = chip->ecc.correct(mtd, p, &ecc_code[i], &ecc_calc[i]);
		if (stat < 0)
			mtd->ecc_stats.failed++;
		else
			mtd->ecc_stats.corrected += stat;
	}
	return 0;
}

static int nand_read_subpage(struct mtd_info *mtd, struct nand_chip *chip,
			uint32_t data_offs, uint32_t readlen, uint8_t *bufpoi)
{
	int start_step, end_step, num_steps;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	uint8_t *p;
	int data_col_addr, i, gaps = 0;
	int datafrag_len, eccfrag_len, aligned_len, aligned_pos;
	int busw = (chip->options & NAND_BUSWIDTH_16) ? 2 : 1;
	int index = 0;

	
	start_step = data_offs / chip->ecc.size;
	end_step = (data_offs + readlen - 1) / chip->ecc.size;
	num_steps = end_step - start_step + 1;

	
	datafrag_len = num_steps * chip->ecc.size;
	eccfrag_len = num_steps * chip->ecc.bytes;

	data_col_addr = start_step * chip->ecc.size;
	
	if (data_col_addr != 0)
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, data_col_addr, -1);

	p = bufpoi + data_col_addr;
	chip->read_buf(mtd, p, datafrag_len);

	
	for (i = 0; i < eccfrag_len ; i += chip->ecc.bytes, p += chip->ecc.size)
		chip->ecc.calculate(mtd, p, &chip->buffers->ecccalc[i]);

	for (i = 0; i < eccfrag_len - 1; i++) {
		if (eccpos[i + start_step * chip->ecc.bytes] + 1 !=
			eccpos[i + start_step * chip->ecc.bytes + 1]) {
			gaps = 1;
			break;
		}
	}
	if (gaps) {
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, mtd->writesize, -1);
		chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);
	} else {
		index = start_step * chip->ecc.bytes;

		aligned_pos = eccpos[index] & ~(busw - 1);
		aligned_len = eccfrag_len;
		if (eccpos[index] & (busw - 1))
			aligned_len++;
		if (eccpos[index + (num_steps * chip->ecc.bytes)] & (busw - 1))
			aligned_len++;

		chip->cmdfunc(mtd, NAND_CMD_RNDOUT,
					mtd->writesize + aligned_pos, -1);
		chip->read_buf(mtd, &chip->oob_poi[aligned_pos], aligned_len);
	}

	for (i = 0; i < eccfrag_len; i++)
		chip->buffers->ecccode[i] = chip->oob_poi[eccpos[i + index]];

	p = bufpoi + data_col_addr;
	for (i = 0; i < eccfrag_len ; i += chip->ecc.bytes, p += chip->ecc.size) {
		int stat;

		stat = chip->ecc.correct(mtd, p,
			&chip->buffers->ecccode[i], &chip->buffers->ecccalc[i]);
		if (stat < 0)
			mtd->ecc_stats.failed++;
		else
			mtd->ecc_stats.corrected += stat;
	}
	return 0;
}

static int nand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int page)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->ecc.hwctl(mtd, NAND_ECC_READ);
		chip->read_buf(mtd, p, eccsize);
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);
	}
	chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);

	for (i = 0; i < chip->ecc.total; i++)
		ecc_code[i] = chip->oob_poi[eccpos[i]];

	eccsteps = chip->ecc.steps;
	p = buf;

	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		stat = chip->ecc.correct(mtd, p, &ecc_code[i], &ecc_calc[i]);
		if (stat < 0)
			mtd->ecc_stats.failed++;
		else
			mtd->ecc_stats.corrected += stat;
	}
	return 0;
}

static int nand_read_page_hwecc_oob_first(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int page)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	uint8_t *ecc_calc = chip->buffers->ecccalc;

	
	chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page);
	chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);
	chip->cmdfunc(mtd, NAND_CMD_READ0, 0, page);

	for (i = 0; i < chip->ecc.total; i++)
		ecc_code[i] = chip->oob_poi[eccpos[i]];

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		chip->ecc.hwctl(mtd, NAND_ECC_READ);
		chip->read_buf(mtd, p, eccsize);
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);

		stat = chip->ecc.correct(mtd, p, &ecc_code[i], NULL);
		if (stat < 0)
			mtd->ecc_stats.failed++;
		else
			mtd->ecc_stats.corrected += stat;
	}
	return 0;
}

static int nand_read_page_syndrome(struct mtd_info *mtd, struct nand_chip *chip,
				   uint8_t *buf, int page)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *oob = chip->oob_poi;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		chip->ecc.hwctl(mtd, NAND_ECC_READ);
		chip->read_buf(mtd, p, eccsize);

		if (chip->ecc.prepad) {
			chip->read_buf(mtd, oob, chip->ecc.prepad);
			oob += chip->ecc.prepad;
		}

		chip->ecc.hwctl(mtd, NAND_ECC_READSYN);
		chip->read_buf(mtd, oob, eccbytes);
		stat = chip->ecc.correct(mtd, p, oob, NULL);

		if (stat < 0)
			mtd->ecc_stats.failed++;
		else
			mtd->ecc_stats.corrected += stat;

		oob += eccbytes;

		if (chip->ecc.postpad) {
			chip->read_buf(mtd, oob, chip->ecc.postpad);
			oob += chip->ecc.postpad;
		}
	}

	
	i = mtd->oobsize - (oob - chip->oob_poi);
	if (i)
		chip->read_buf(mtd, oob, i);

	return 0;
}

static uint8_t *nand_transfer_oob(struct nand_chip *chip, uint8_t *oob,
				  struct mtd_oob_ops *ops, size_t len)
{
	switch (ops->mode) {

	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_RAW:
		memcpy(oob, chip->oob_poi + ops->ooboffs, len);
		return oob + len;

	case MTD_OPS_AUTO_OOB: {
		struct nand_oobfree *free = chip->ecc.layout->oobfree;
		uint32_t boffs = 0, roffs = ops->ooboffs;
		size_t bytes = 0;

		for (; free->length && len; free++, len -= bytes) {
			
			if (unlikely(roffs)) {
				if (roffs >= free->length) {
					roffs -= free->length;
					continue;
				}
				boffs = free->offset + roffs;
				bytes = min_t(size_t, len,
					      (free->length - roffs));
				roffs = 0;
			} else {
				bytes = min_t(size_t, len, free->length);
				boffs = free->offset;
			}
			memcpy(oob, chip->oob_poi + boffs, bytes);
			oob += bytes;
		}
		return oob;
	}
	default:
		BUG();
	}
	return NULL;
}

static int nand_do_read_ops(struct mtd_info *mtd, loff_t from,
			    struct mtd_oob_ops *ops)
{
	int chipnr, page, realpage, col, bytes, aligned;
	struct nand_chip *chip = mtd->priv;
	struct mtd_ecc_stats stats;
	int blkcheck = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;
	int sndcmd = 1;
	int ret = 0;
	uint32_t readlen = ops->len;
	uint32_t oobreadlen = ops->ooblen;
	uint32_t max_oobsize = ops->mode == MTD_OPS_AUTO_OOB ?
		mtd->oobavail : mtd->oobsize;

	uint8_t *bufpoi, *oob, *buf;

	stats = mtd->ecc_stats;

	chipnr = (int)(from >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	col = (int)(from & (mtd->writesize - 1));

	buf = ops->datbuf;
	oob = ops->oobbuf;

	while (1) {
		bytes = min(mtd->writesize - col, readlen);
		aligned = (bytes == mtd->writesize);

		
		if (realpage != chip->pagebuf || oob) {
			bufpoi = aligned ? buf : chip->buffers->databuf;

			if (likely(sndcmd)) {
				chip->cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);
				sndcmd = 0;
			}

			
			if (unlikely(ops->mode == MTD_OPS_RAW))
				ret = chip->ecc.read_page_raw(mtd, chip,
							      bufpoi, page);
			else if (!aligned && NAND_SUBPAGE_READ(chip) && !oob)
				ret = chip->ecc.read_subpage(mtd, chip,
							col, bytes, bufpoi);
			else
				ret = chip->ecc.read_page(mtd, chip, bufpoi,
							  page);
			if (ret < 0) {
				if (!aligned)
					
					chip->pagebuf = -1;
				break;
			}

			
			if (!aligned) {
				if (!NAND_SUBPAGE_READ(chip) && !oob &&
				    !(mtd->ecc_stats.failed - stats.failed) &&
				    (ops->mode != MTD_OPS_RAW))
					chip->pagebuf = realpage;
				else
					
					chip->pagebuf = -1;
				memcpy(buf, chip->buffers->databuf + col, bytes);
			}

			buf += bytes;

			if (unlikely(oob)) {

				int toread = min(oobreadlen, max_oobsize);

				if (toread) {
					oob = nand_transfer_oob(chip,
						oob, ops, toread);
					oobreadlen -= toread;
				}
			}

			if (!(chip->options & NAND_NO_READRDY)) {
				if (!chip->dev_ready)
					udelay(chip->chip_delay);
				else
					nand_wait_ready(mtd);
			}
		} else {
			memcpy(buf, chip->buffers->databuf + col, bytes);
			buf += bytes;
		}

		readlen -= bytes;

		if (!readlen)
			break;

		
		col = 0;
		
		realpage++;

		page = realpage & chip->pagemask;
		
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}

		if (!NAND_CANAUTOINCR(chip) || !(page & blkcheck))
			sndcmd = 1;
	}

	ops->retlen = ops->len - (size_t) readlen;
	if (oob)
		ops->oobretlen = ops->ooblen - oobreadlen;

	if (ret)
		return ret;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return  mtd->ecc_stats.corrected - stats.corrected ? -EUCLEAN : 0;
}

static int nand_read(struct mtd_info *mtd, loff_t from, size_t len,
		     size_t *retlen, uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	struct mtd_oob_ops ops;
	int ret;

	nand_get_device(chip, mtd, FL_READING);
	ops.len = len;
	ops.datbuf = buf;
	ops.oobbuf = NULL;
	ops.mode = 0;
	ret = nand_do_read_ops(mtd, from, &ops);
	*retlen = ops.retlen;
	nand_release_device(mtd);
	return ret;
}

static int nand_read_oob_std(struct mtd_info *mtd, struct nand_chip *chip,
			     int page, int sndcmd)
{
	if (sndcmd) {
		chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page);
		sndcmd = 0;
	}
	chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);
	return sndcmd;
}

static int nand_read_oob_syndrome(struct mtd_info *mtd, struct nand_chip *chip,
				  int page, int sndcmd)
{
	uint8_t *buf = chip->oob_poi;
	int length = mtd->oobsize;
	int chunk = chip->ecc.bytes + chip->ecc.prepad + chip->ecc.postpad;
	int eccsize = chip->ecc.size;
	uint8_t *bufpoi = buf;
	int i, toread, sndrnd = 0, pos;

	chip->cmdfunc(mtd, NAND_CMD_READ0, chip->ecc.size, page);
	for (i = 0; i < chip->ecc.steps; i++) {
		if (sndrnd) {
			pos = eccsize + i * (eccsize + chunk);
			if (mtd->writesize > 512)
				chip->cmdfunc(mtd, NAND_CMD_RNDOUT, pos, -1);
			else
				chip->cmdfunc(mtd, NAND_CMD_READ0, pos, page);
		} else
			sndrnd = 1;
		toread = min_t(int, length, chunk);
		chip->read_buf(mtd, bufpoi, toread);
		bufpoi += toread;
		length -= toread;
	}
	if (length > 0)
		chip->read_buf(mtd, bufpoi, length);

	return 1;
}

static int nand_write_oob_std(struct mtd_info *mtd, struct nand_chip *chip,
			      int page)
{
	int status = 0;
	const uint8_t *buf = chip->oob_poi;
	int length = mtd->oobsize;

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, mtd->writesize, page);
	chip->write_buf(mtd, buf, length);
	
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	status = chip->waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}

static int nand_write_oob_syndrome(struct mtd_info *mtd,
				   struct nand_chip *chip, int page)
{
	int chunk = chip->ecc.bytes + chip->ecc.prepad + chip->ecc.postpad;
	int eccsize = chip->ecc.size, length = mtd->oobsize;
	int i, len, pos, status = 0, sndcmd = 0, steps = chip->ecc.steps;
	const uint8_t *bufpoi = chip->oob_poi;

	if (!chip->ecc.prepad && !chip->ecc.postpad) {
		pos = steps * (eccsize + chunk);
		steps = 0;
	} else
		pos = eccsize;

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, pos, page);
	for (i = 0; i < steps; i++) {
		if (sndcmd) {
			if (mtd->writesize <= 512) {
				uint32_t fill = 0xFFFFFFFF;

				len = eccsize;
				while (len > 0) {
					int num = min_t(int, len, 4);
					chip->write_buf(mtd, (uint8_t *)&fill,
							num);
					len -= num;
				}
			} else {
				pos = eccsize + i * (eccsize + chunk);
				chip->cmdfunc(mtd, NAND_CMD_RNDIN, pos, -1);
			}
		} else
			sndcmd = 1;
		len = min_t(int, length, chunk);
		chip->write_buf(mtd, bufpoi, len);
		bufpoi += len;
		length -= len;
	}
	if (length > 0)
		chip->write_buf(mtd, bufpoi, length);

	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);
	status = chip->waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}

static int nand_do_read_oob(struct mtd_info *mtd, loff_t from,
			    struct mtd_oob_ops *ops)
{
	int page, realpage, chipnr, sndcmd = 1;
	struct nand_chip *chip = mtd->priv;
	struct mtd_ecc_stats stats;
	int blkcheck = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;
	int readlen = ops->ooblen;
	int len;
	uint8_t *buf = ops->oobbuf;

	pr_debug("%s: from = 0x%08Lx, len = %i\n",
			__func__, (unsigned long long)from, readlen);

	stats = mtd->ecc_stats;

	if (ops->mode == MTD_OPS_AUTO_OOB)
		len = chip->ecc.layout->oobavail;
	else
		len = mtd->oobsize;

	if (unlikely(ops->ooboffs >= len)) {
		pr_debug("%s: attempt to start read outside oob\n",
				__func__);
		return -EINVAL;
	}

	
	if (unlikely(from >= mtd->size ||
		     ops->ooboffs + readlen > ((mtd->size >> chip->page_shift) -
					(from >> chip->page_shift)) * len)) {
		pr_debug("%s: attempt to read beyond end of device\n",
				__func__);
		return -EINVAL;
	}

	chipnr = (int)(from >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	
	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	while (1) {
		if (ops->mode == MTD_OPS_RAW)
			sndcmd = chip->ecc.read_oob_raw(mtd, chip, page, sndcmd);
		else
			sndcmd = chip->ecc.read_oob(mtd, chip, page, sndcmd);

		len = min(len, readlen);
		buf = nand_transfer_oob(chip, buf, ops, len);

		if (!(chip->options & NAND_NO_READRDY)) {
			if (!chip->dev_ready)
				udelay(chip->chip_delay);
			else
				nand_wait_ready(mtd);
		}

		readlen -= len;
		if (!readlen)
			break;

		
		realpage++;

		page = realpage & chip->pagemask;
		
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}

		if (!NAND_CANAUTOINCR(chip) || !(page & blkcheck))
			sndcmd = 1;
	}

	ops->oobretlen = ops->ooblen;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return  mtd->ecc_stats.corrected - stats.corrected ? -EUCLEAN : 0;
}

static int nand_read_oob(struct mtd_info *mtd, loff_t from,
			 struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;
	int ret = -ENOTSUPP;

	ops->retlen = 0;

	
	if (ops->datbuf && (from + ops->len) > mtd->size) {
		pr_debug("%s: attempt to read beyond end of device\n",
				__func__);
		return -EINVAL;
	}

	nand_get_device(chip, mtd, FL_READING);

	switch (ops->mode) {
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_AUTO_OOB:
	case MTD_OPS_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = nand_do_read_oob(mtd, from, ops);
	else
		ret = nand_do_read_ops(mtd, from, ops);

out:
	nand_release_device(mtd);
	return ret;
}


static void nand_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				const uint8_t *buf)
{
	chip->write_buf(mtd, buf, mtd->writesize);
	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
}

static void nand_write_page_raw_syndrome(struct mtd_info *mtd,
					struct nand_chip *chip,
					const uint8_t *buf)
{
	int eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	uint8_t *oob = chip->oob_poi;
	int steps, size;

	for (steps = chip->ecc.steps; steps > 0; steps--) {
		chip->write_buf(mtd, buf, eccsize);
		buf += eccsize;

		if (chip->ecc.prepad) {
			chip->write_buf(mtd, oob, chip->ecc.prepad);
			oob += chip->ecc.prepad;
		}

		chip->read_buf(mtd, oob, eccbytes);
		oob += eccbytes;

		if (chip->ecc.postpad) {
			chip->write_buf(mtd, oob, chip->ecc.postpad);
			oob += chip->ecc.postpad;
		}
	}

	size = mtd->oobsize - (oob - chip->oob_poi);
	if (size)
		chip->write_buf(mtd, oob, size);
}
static void nand_write_page_swecc(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	const uint8_t *p = buf;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);

	for (i = 0; i < chip->ecc.total; i++)
		chip->oob_poi[eccpos[i]] = ecc_calc[i];

	chip->ecc.write_page_raw(mtd, chip, buf);
}

static void nand_write_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	const uint8_t *p = buf;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
		chip->write_buf(mtd, p, eccsize);
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);
	}

	for (i = 0; i < chip->ecc.total; i++)
		chip->oob_poi[eccpos[i]] = ecc_calc[i];

	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
}

static void nand_write_page_syndrome(struct mtd_info *mtd,
				    struct nand_chip *chip, const uint8_t *buf)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	const uint8_t *p = buf;
	uint8_t *oob = chip->oob_poi;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {

		chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
		chip->write_buf(mtd, p, eccsize);

		if (chip->ecc.prepad) {
			chip->write_buf(mtd, oob, chip->ecc.prepad);
			oob += chip->ecc.prepad;
		}

		chip->ecc.calculate(mtd, p, oob);
		chip->write_buf(mtd, oob, eccbytes);
		oob += eccbytes;

		if (chip->ecc.postpad) {
			chip->write_buf(mtd, oob, chip->ecc.postpad);
			oob += chip->ecc.postpad;
		}
	}

	
	i = mtd->oobsize - (oob - chip->oob_poi);
	if (i)
		chip->write_buf(mtd, oob, i);
}

static int nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
			   const uint8_t *buf, int page, int cached, int raw)
{
	int status;

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, 0x00, page);

	if (unlikely(raw))
		chip->ecc.write_page_raw(mtd, chip, buf);
	else
		chip->ecc.write_page(mtd, chip, buf);

	cached = 0;

	if (!cached || !(chip->options & NAND_CACHEPRG)) {

		chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);
		status = chip->waitfunc(mtd, chip);
		if ((status & NAND_STATUS_FAIL) && (chip->errstat))
			status = chip->errstat(mtd, chip, FL_WRITING, status,
					       page);

		if (status & NAND_STATUS_FAIL)
			return -EIO;
	} else {
		chip->cmdfunc(mtd, NAND_CMD_CACHEDPROG, -1, -1);
		status = chip->waitfunc(mtd, chip);
	}

#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	
	chip->cmdfunc(mtd, NAND_CMD_READ0, 0, page);

	if (chip->verify_buf(mtd, buf, mtd->writesize))
		return -EIO;
#endif
	return 0;
}

static uint8_t *nand_fill_oob(struct mtd_info *mtd, uint8_t *oob, size_t len,
			      struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;

	memset(chip->oob_poi, 0xff, mtd->oobsize);

	switch (ops->mode) {

	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_RAW:
		memcpy(chip->oob_poi + ops->ooboffs, oob, len);
		return oob + len;

	case MTD_OPS_AUTO_OOB: {
		struct nand_oobfree *free = chip->ecc.layout->oobfree;
		uint32_t boffs = 0, woffs = ops->ooboffs;
		size_t bytes = 0;

		for (; free->length && len; free++, len -= bytes) {
			
			if (unlikely(woffs)) {
				if (woffs >= free->length) {
					woffs -= free->length;
					continue;
				}
				boffs = free->offset + woffs;
				bytes = min_t(size_t, len,
					      (free->length - woffs));
				woffs = 0;
			} else {
				bytes = min_t(size_t, len, free->length);
				boffs = free->offset;
			}
			memcpy(chip->oob_poi + boffs, oob, bytes);
			oob += bytes;
		}
		return oob;
	}
	default:
		BUG();
	}
	return NULL;
}

#define NOTALIGNED(x)	((x & (chip->subpagesize - 1)) != 0)

static int nand_do_write_ops(struct mtd_info *mtd, loff_t to,
			     struct mtd_oob_ops *ops)
{
	int chipnr, realpage, page, blockmask, column;
	struct nand_chip *chip = mtd->priv;
	uint32_t writelen = ops->len;

	uint32_t oobwritelen = ops->ooblen;
	uint32_t oobmaxlen = ops->mode == MTD_OPS_AUTO_OOB ?
				mtd->oobavail : mtd->oobsize;

	uint8_t *oob = ops->oobbuf;
	uint8_t *buf = ops->datbuf;
	int ret, subpage;

	ops->retlen = 0;
	if (!writelen)
		return 0;

	
	if (NOTALIGNED(to) || NOTALIGNED(ops->len)) {
		pr_notice("%s: attempt to write non page aligned data\n",
			   __func__);
		return -EINVAL;
	}

	column = to & (mtd->writesize - 1);
	subpage = column || (writelen & (mtd->writesize - 1));

	if (subpage && oob)
		return -EINVAL;

	chipnr = (int)(to >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	
	if (nand_check_wp(mtd))
		return -EIO;

	realpage = (int)(to >> chip->page_shift);
	page = realpage & chip->pagemask;
	blockmask = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;

	
	if (to <= (chip->pagebuf << chip->page_shift) &&
	    (chip->pagebuf << chip->page_shift) < (to + ops->len))
		chip->pagebuf = -1;

	
	if (oob && ops->ooboffs && (ops->ooboffs + ops->ooblen > oobmaxlen))
		return -EINVAL;

	while (1) {
		int bytes = mtd->writesize;
		int cached = writelen > bytes && page != blockmask;
		uint8_t *wbuf = buf;

		
		if (unlikely(column || writelen < (mtd->writesize - 1))) {
			cached = 0;
			bytes = min_t(int, bytes - column, (int) writelen);
			chip->pagebuf = -1;
			memset(chip->buffers->databuf, 0xff, mtd->writesize);
			memcpy(&chip->buffers->databuf[column], buf, bytes);
			wbuf = chip->buffers->databuf;
		}

		if (unlikely(oob)) {
			size_t len = min(oobwritelen, oobmaxlen);
			oob = nand_fill_oob(mtd, oob, len, ops);
			oobwritelen -= len;
		} else {
			
			memset(chip->oob_poi, 0xff, mtd->oobsize);
		}

		ret = chip->write_page(mtd, chip, wbuf, page, cached,
				       (ops->mode == MTD_OPS_RAW));
		if (ret)
			break;

		writelen -= bytes;
		if (!writelen)
			break;

		column = 0;
		buf += bytes;
		realpage++;

		page = realpage & chip->pagemask;
		
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}
	}

	ops->retlen = ops->len - writelen;
	if (unlikely(oob))
		ops->oobretlen = ops->ooblen;
	return ret;
}

/**
 * panic_nand_write - [MTD Interface] NAND write with ECC
 * @mtd: MTD device structure
 * @to: offset to write to
 * @len: number of bytes to write
 * @retlen: pointer to variable to store the number of written bytes
 * @buf: the data to write
 *
 * NAND write with ECC. Used when performing writes in interrupt context, this
 * may for example be called by mtdoops when writing an oops while in panic.
 */
static int panic_nand_write(struct mtd_info *mtd, loff_t to, size_t len,
			    size_t *retlen, const uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	struct mtd_oob_ops ops;
	int ret;

	
	panic_nand_wait(mtd, chip, 400);

	
	panic_nand_get_device(chip, mtd, FL_WRITING);

	ops.len = len;
	ops.datbuf = (uint8_t *)buf;
	ops.oobbuf = NULL;
	ops.mode = 0;

	ret = nand_do_write_ops(mtd, to, &ops);

	*retlen = ops.retlen;
	return ret;
}

/**
 * nand_write - [MTD Interface] NAND write with ECC
 * @mtd: MTD device structure
 * @to: offset to write to
 * @len: number of bytes to write
 * @retlen: pointer to variable to store the number of written bytes
 * @buf: the data to write
 *
 * NAND write with ECC.
 */
static int nand_write(struct mtd_info *mtd, loff_t to, size_t len,
			  size_t *retlen, const uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	struct mtd_oob_ops ops;
	int ret;

	nand_get_device(chip, mtd, FL_WRITING);
	ops.len = len;
	ops.datbuf = (uint8_t *)buf;
	ops.oobbuf = NULL;
	ops.mode = 0;
	ret = nand_do_write_ops(mtd, to, &ops);
	*retlen = ops.retlen;
	nand_release_device(mtd);
	return ret;
}

static int nand_do_write_oob(struct mtd_info *mtd, loff_t to,
			     struct mtd_oob_ops *ops)
{
	int chipnr, page, status, len;
	struct nand_chip *chip = mtd->priv;

	pr_debug("%s: to = 0x%08x, len = %i\n",
			 __func__, (unsigned int)to, (int)ops->ooblen);

	if (ops->mode == MTD_OPS_AUTO_OOB)
		len = chip->ecc.layout->oobavail;
	else
		len = mtd->oobsize;

	
	if ((ops->ooboffs + ops->ooblen) > len) {
		pr_debug("%s: attempt to write past end of page\n",
				__func__);
		return -EINVAL;
	}

	if (unlikely(ops->ooboffs >= len)) {
		pr_debug("%s: attempt to start write outside oob\n",
				__func__);
		return -EINVAL;
	}

	
	if (unlikely(to >= mtd->size ||
		     ops->ooboffs + ops->ooblen >
			((mtd->size >> chip->page_shift) -
			 (to >> chip->page_shift)) * len)) {
		pr_debug("%s: attempt to write beyond end of device\n",
				__func__);
		return -EINVAL;
	}

	chipnr = (int)(to >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	
	page = (int)(to >> chip->page_shift);

	chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);

	
	if (nand_check_wp(mtd))
		return -EROFS;

	
	if (page == chip->pagebuf)
		chip->pagebuf = -1;

	nand_fill_oob(mtd, ops->oobbuf, ops->ooblen, ops);

	if (ops->mode == MTD_OPS_RAW)
		status = chip->ecc.write_oob_raw(mtd, chip, page & chip->pagemask);
	else
		status = chip->ecc.write_oob(mtd, chip, page & chip->pagemask);

	if (status)
		return status;

	ops->oobretlen = ops->ooblen;

	return 0;
}

static int nand_write_oob(struct mtd_info *mtd, loff_t to,
			  struct mtd_oob_ops *ops)
{
	struct nand_chip *chip = mtd->priv;
	int ret = -ENOTSUPP;

	ops->retlen = 0;

	
	if (ops->datbuf && (to + ops->len) > mtd->size) {
		pr_debug("%s: attempt to write beyond end of device\n",
				__func__);
		return -EINVAL;
	}

	nand_get_device(chip, mtd, FL_WRITING);

	switch (ops->mode) {
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_AUTO_OOB:
	case MTD_OPS_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = nand_do_write_oob(mtd, to, ops);
	else
		ret = nand_do_write_ops(mtd, to, ops);

out:
	nand_release_device(mtd);
	return ret;
}

static void single_erase_cmd(struct mtd_info *mtd, int page)
{
	struct nand_chip *chip = mtd->priv;
	
	chip->cmdfunc(mtd, NAND_CMD_ERASE1, -1, page);
	chip->cmdfunc(mtd, NAND_CMD_ERASE2, -1, -1);
}

static void multi_erase_cmd(struct mtd_info *mtd, int page)
{
	struct nand_chip *chip = mtd->priv;
	
	chip->cmdfunc(mtd, NAND_CMD_ERASE1, -1, page++);
	chip->cmdfunc(mtd, NAND_CMD_ERASE1, -1, page++);
	chip->cmdfunc(mtd, NAND_CMD_ERASE1, -1, page++);
	chip->cmdfunc(mtd, NAND_CMD_ERASE1, -1, page);
	chip->cmdfunc(mtd, NAND_CMD_ERASE2, -1, -1);
}

static int nand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	return nand_erase_nand(mtd, instr, 0);
}

#define BBT_PAGE_MASK	0xffffff3f
int nand_erase_nand(struct mtd_info *mtd, struct erase_info *instr,
		    int allowbbt)
{
	int page, status, pages_per_block, ret, chipnr;
	struct nand_chip *chip = mtd->priv;
	loff_t rewrite_bbt[NAND_MAX_CHIPS] = {0};
	unsigned int bbt_masked_page = 0xffffffff;
	loff_t len;

	pr_debug("%s: start = 0x%012llx, len = %llu\n",
			__func__, (unsigned long long)instr->addr,
			(unsigned long long)instr->len);

	if (check_offs_len(mtd, instr->addr, instr->len))
		return -EINVAL;

	
	nand_get_device(chip, mtd, FL_ERASING);

	
	page = (int)(instr->addr >> chip->page_shift);
	chipnr = (int)(instr->addr >> chip->chip_shift);

	
	pages_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);

	
	chip->select_chip(mtd, chipnr);

	
	if (nand_check_wp(mtd)) {
		pr_debug("%s: device is write protected!\n",
				__func__);
		instr->state = MTD_ERASE_FAILED;
		goto erase_exit;
	}

	/*
	 * If BBT requires refresh, set the BBT page mask to see if the BBT
	 * should be rewritten. Otherwise the mask is set to 0xffffffff which
	 * can not be matched. This is also done when the bbt is actually
	 * erased to avoid recursive updates.
	 */
	if (chip->options & BBT_AUTO_REFRESH && !allowbbt)
		bbt_masked_page = chip->bbt_td->pages[chipnr] & BBT_PAGE_MASK;

	
	len = instr->len;

	instr->state = MTD_ERASING;

	while (len) {
		
		if (nand_block_checkbad(mtd, ((loff_t) page) <<
					chip->page_shift, 0, allowbbt)) {
			pr_warn("%s: attempt to erase a bad block at page 0x%08x\n",
				    __func__, page);
			instr->state = MTD_ERASE_FAILED;
			goto erase_exit;
		}

		if (page <= chip->pagebuf && chip->pagebuf <
		    (page + pages_per_block))
			chip->pagebuf = -1;

		chip->erase_cmd(mtd, page & chip->pagemask);

		status = chip->waitfunc(mtd, chip);

		if ((status & NAND_STATUS_FAIL) && (chip->errstat))
			status = chip->errstat(mtd, chip, FL_ERASING,
					       status, page);

		
		if (status & NAND_STATUS_FAIL) {
			pr_debug("%s: failed erase, page 0x%08x\n",
					__func__, page);
			instr->state = MTD_ERASE_FAILED;
			instr->fail_addr =
				((loff_t)page << chip->page_shift);
			goto erase_exit;
		}

		if (bbt_masked_page != 0xffffffff &&
		    (page & BBT_PAGE_MASK) == bbt_masked_page)
			    rewrite_bbt[chipnr] =
					((loff_t)page << chip->page_shift);

		
		len -= (1 << chip->phys_erase_shift);
		page += pages_per_block;

		
		if (len && !(page & chip->pagemask)) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);

			/*
			 * If BBT requires refresh and BBT-PERCHIP, set the BBT
			 * page mask to see if this BBT should be rewritten.
			 */
			if (bbt_masked_page != 0xffffffff &&
			    (chip->bbt_td->options & NAND_BBT_PERCHIP))
				bbt_masked_page = chip->bbt_td->pages[chipnr] &
					BBT_PAGE_MASK;
		}
	}
	instr->state = MTD_ERASE_DONE;

erase_exit:

	ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;

	
	nand_release_device(mtd);

	
	if (!ret)
		mtd_erase_callback(instr);

	if (bbt_masked_page == 0xffffffff || ret)
		return ret;

	for (chipnr = 0; chipnr < chip->numchips; chipnr++) {
		if (!rewrite_bbt[chipnr])
			continue;
		
		pr_debug("%s: nand_update_bbt (%d:0x%0llx 0x%0x)\n",
				__func__, chipnr, rewrite_bbt[chipnr],
				chip->bbt_td->pages[chipnr]);
		nand_update_bbt(mtd, rewrite_bbt[chipnr]);
	}

	
	return ret;
}

static void nand_sync(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	pr_debug("%s: called\n", __func__);

	
	nand_get_device(chip, mtd, FL_SYNCING);
	
	nand_release_device(mtd);
}

static int nand_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	return nand_block_checkbad(mtd, offs, 1, 0);
}

static int nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	int ret;

	ret = nand_block_isbad(mtd, ofs);
	if (ret) {
		
		if (ret > 0)
			return 0;
		return ret;
	}

	return chip->block_markbad(mtd, ofs);
}

static int nand_suspend(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	return nand_get_device(chip, mtd, FL_PM_SUSPENDED);
}

static void nand_resume(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	if (chip->state == FL_PM_SUSPENDED)
		nand_release_device(mtd);
	else
		pr_err("%s called for a chip which is not in suspended state\n",
			__func__);
}

static void nand_set_defaults(struct nand_chip *chip, int busw)
{
	
	if (!chip->chip_delay)
		chip->chip_delay = 20;

	
	if (chip->cmdfunc == NULL)
		chip->cmdfunc = nand_command;

	
	if (chip->waitfunc == NULL)
		chip->waitfunc = nand_wait;

	if (!chip->select_chip)
		chip->select_chip = nand_select_chip;
	if (!chip->read_byte)
		chip->read_byte = busw ? nand_read_byte16 : nand_read_byte;
	if (!chip->read_word)
		chip->read_word = nand_read_word;
	if (!chip->block_bad)
		chip->block_bad = nand_block_bad;
	if (!chip->block_markbad)
		chip->block_markbad = nand_default_block_markbad;
	if (!chip->write_buf)
		chip->write_buf = busw ? nand_write_buf16 : nand_write_buf;
	if (!chip->read_buf)
		chip->read_buf = busw ? nand_read_buf16 : nand_read_buf;
	if (!chip->verify_buf)
		chip->verify_buf = busw ? nand_verify_buf16 : nand_verify_buf;
	if (!chip->scan_bbt)
		chip->scan_bbt = nand_default_bbt;

	if (!chip->controller) {
		chip->controller = &chip->hwcontrol;
		spin_lock_init(&chip->controller->lock);
		init_waitqueue_head(&chip->controller->wq);
	}

}

static void sanitize_string(uint8_t *s, size_t len)
{
	ssize_t i;

	
	s[len - 1] = 0;

	
	for (i = 0; i < len - 1; i++) {
		if (s[i] < ' ' || s[i] > 127)
			s[i] = '?';
	}

	
	strim(s);
}

static u16 onfi_crc16(u16 crc, u8 const *p, size_t len)
{
	int i;
	while (len--) {
		crc ^= *p++ << 8;
		for (i = 0; i < 8; i++)
			crc = (crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0);
	}

	return crc;
}

static int nand_flash_detect_onfi(struct mtd_info *mtd, struct nand_chip *chip,
					int *busw)
{
	struct nand_onfi_params *p = &chip->onfi_params;
	int i;
	int val;

	
	chip->cmdfunc(mtd, NAND_CMD_READID, 0x20, -1);
	if (chip->read_byte(mtd) != 'O' || chip->read_byte(mtd) != 'N' ||
		chip->read_byte(mtd) != 'F' || chip->read_byte(mtd) != 'I')
		return 0;

	chip->cmdfunc(mtd, NAND_CMD_PARAM, 0, -1);
	for (i = 0; i < 3; i++) {
		chip->read_buf(mtd, (uint8_t *)p, sizeof(*p));
		if (onfi_crc16(ONFI_CRC_BASE, (uint8_t *)p, 254) ==
				le16_to_cpu(p->crc)) {
			pr_info("ONFI param page %d valid\n", i);
			break;
		}
	}

	if (i == 3)
		return 0;

	
	val = le16_to_cpu(p->revision);
	if (val & (1 << 5))
		chip->onfi_version = 23;
	else if (val & (1 << 4))
		chip->onfi_version = 22;
	else if (val & (1 << 3))
		chip->onfi_version = 21;
	else if (val & (1 << 2))
		chip->onfi_version = 20;
	else if (val & (1 << 1))
		chip->onfi_version = 10;
	else
		chip->onfi_version = 0;

	if (!chip->onfi_version) {
		pr_info("%s: unsupported ONFI version: %d\n", __func__, val);
		return 0;
	}

	sanitize_string(p->manufacturer, sizeof(p->manufacturer));
	sanitize_string(p->model, sizeof(p->model));
	if (!mtd->name)
		mtd->name = p->model;
	mtd->writesize = le32_to_cpu(p->byte_per_page);
	mtd->erasesize = le32_to_cpu(p->pages_per_block) * mtd->writesize;
	mtd->oobsize = le16_to_cpu(p->spare_bytes_per_page);
	chip->chipsize = le32_to_cpu(p->blocks_per_lun);
	chip->chipsize *= (uint64_t)mtd->erasesize * p->lun_count;
	*busw = 0;
	if (le16_to_cpu(p->features) & 1)
		*busw = NAND_BUSWIDTH_16;

	chip->options &= ~NAND_CHIPOPTIONS_MSK;
	chip->options |= (NAND_NO_READRDY |
			NAND_NO_AUTOINCR) & NAND_CHIPOPTIONS_MSK;

	pr_info("ONFI flash detected\n");
	return 1;
}

static struct nand_flash_dev *nand_get_flash_type(struct mtd_info *mtd,
						  struct nand_chip *chip,
						  int busw,
						  int *maf_id, int *dev_id,
						  struct nand_flash_dev *type)
{
	int i, maf_idx;
	u8 id_data[8];
	int ret;

	
	chip->select_chip(mtd, 0);

	chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);

	
	chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);

	
	*maf_id = chip->read_byte(mtd);
	*dev_id = chip->read_byte(mtd);


	chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);

	for (i = 0; i < 2; i++)
		id_data[i] = chip->read_byte(mtd);

	if (id_data[0] != *maf_id || id_data[1] != *dev_id) {
		pr_info("%s: second ID read did not match "
			"%02x,%02x against %02x,%02x\n", __func__,
			*maf_id, *dev_id, id_data[0], id_data[1]);
		return ERR_PTR(-ENODEV);
	}

	if (!type)
		type = nand_flash_ids;

	for (; type->name != NULL; type++)
		if (*dev_id == type->id)
			break;

	chip->onfi_version = 0;
	if (!type->name || !type->pagesize) {
		
		ret = nand_flash_detect_onfi(mtd, chip, &busw);
		if (ret)
			goto ident_done;
	}

	chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);

	

	for (i = 0; i < 8; i++)
		id_data[i] = chip->read_byte(mtd);

	if (!type->name)
		return ERR_PTR(-ENODEV);

	if (!mtd->name)
		mtd->name = type->name;

	chip->chipsize = (uint64_t)type->chipsize << 20;

	if (!type->pagesize && chip->init_size) {
		
		busw = chip->init_size(mtd, chip, id_data);
	} else if (!type->pagesize) {
		int extid;
		
		chip->cellinfo = id_data[2];
		
		extid = id_data[3];

		if (id_data[0] == id_data[6] && id_data[1] == id_data[7] &&
				id_data[0] == NAND_MFR_SAMSUNG &&
				(chip->cellinfo & NAND_CI_CELLTYPE_MSK) &&
				id_data[5] != 0x00) {
			
			mtd->writesize = 2048 << (extid & 0x03);
			extid >>= 2;
			
			switch (extid & 0x03) {
			case 1:
				mtd->oobsize = 128;
				break;
			case 2:
				mtd->oobsize = 218;
				break;
			case 3:
				mtd->oobsize = 400;
				break;
			default:
				mtd->oobsize = 436;
				break;
			}
			extid >>= 2;
			
			mtd->erasesize = (128 * 1024) <<
				(((extid >> 1) & 0x04) | (extid & 0x03));
			busw = 0;
		} else {
			
			mtd->writesize = 1024 << (extid & 0x03);
			extid >>= 2;
			
			mtd->oobsize = (8 << (extid & 0x01)) *
				(mtd->writesize >> 9);
			extid >>= 2;
			
			mtd->erasesize = (64 * 1024) << (extid & 0x03);
			extid >>= 2;
			
			busw = (extid & 0x01) ? NAND_BUSWIDTH_16 : 0;
		}
	} else {
		mtd->erasesize = type->erasesize;
		mtd->writesize = type->pagesize;
		mtd->oobsize = mtd->writesize / 32;
		busw = type->options & NAND_BUSWIDTH_16;

		if (*maf_id == NAND_MFR_AMD && id_data[4] != 0x00 &&
				id_data[5] == 0x00 && id_data[6] == 0x00 &&
				id_data[7] == 0x00 && mtd->writesize == 512) {
			mtd->erasesize = 128 * 1024;
			mtd->erasesize <<= ((id_data[3] & 0x03) << 1);
		}
	}
	
	chip->options &= ~NAND_CHIPOPTIONS_MSK;
	chip->options |= type->options & NAND_CHIPOPTIONS_MSK;

	if (*maf_id != NAND_MFR_SAMSUNG && !type->pagesize)
		chip->options &= ~NAND_SAMSUNG_LP_OPTIONS;
ident_done:

	chip->options |= NAND_NO_AUTOINCR;

	
	for (maf_idx = 0; nand_manuf_ids[maf_idx].id != 0x0; maf_idx++) {
		if (nand_manuf_ids[maf_idx].id == *maf_id)
			break;
	}

	if (busw != (chip->options & NAND_BUSWIDTH_16)) {
		pr_info("NAND device: Manufacturer ID:"
			" 0x%02x, Chip ID: 0x%02x (%s %s)\n", *maf_id,
			*dev_id, nand_manuf_ids[maf_idx].name, mtd->name);
		pr_warn("NAND bus width %d instead %d bit\n",
			   (chip->options & NAND_BUSWIDTH_16) ? 16 : 8,
			   busw ? 16 : 8);
		return ERR_PTR(-EINVAL);
	}

	
	chip->page_shift = ffs(mtd->writesize) - 1;
	
	chip->pagemask = (chip->chipsize >> chip->page_shift) - 1;

	chip->bbt_erase_shift = chip->phys_erase_shift =
		ffs(mtd->erasesize) - 1;
	if (chip->chipsize & 0xffffffff)
		chip->chip_shift = ffs((unsigned)chip->chipsize) - 1;
	else {
		chip->chip_shift = ffs((unsigned)(chip->chipsize >> 32));
		chip->chip_shift += 32 - 1;
	}

	chip->badblockbits = 8;

	
	if (mtd->writesize > 512 || (busw & NAND_BUSWIDTH_16))
		chip->badblockpos = NAND_LARGE_BADBLOCK_POS;
	else
		chip->badblockpos = NAND_SMALL_BADBLOCK_POS;

	if ((chip->cellinfo & NAND_CI_CELLTYPE_MSK) &&
			(*maf_id == NAND_MFR_SAMSUNG ||
			 *maf_id == NAND_MFR_HYNIX))
		chip->bbt_options |= NAND_BBT_SCANLASTPAGE;
	else if ((!(chip->cellinfo & NAND_CI_CELLTYPE_MSK) &&
				(*maf_id == NAND_MFR_SAMSUNG ||
				 *maf_id == NAND_MFR_HYNIX ||
				 *maf_id == NAND_MFR_TOSHIBA ||
				 *maf_id == NAND_MFR_AMD ||
				 *maf_id == NAND_MFR_MACRONIX)) ||
			(mtd->writesize == 2048 &&
			 *maf_id == NAND_MFR_MICRON))
		chip->bbt_options |= NAND_BBT_SCAN2NDPAGE;

	
	if (chip->options & NAND_4PAGE_ARRAY)
		chip->erase_cmd = multi_erase_cmd;
	else
		chip->erase_cmd = single_erase_cmd;

	
	if (mtd->writesize > 512 && chip->cmdfunc == nand_command)
		chip->cmdfunc = nand_command_lp;

	pr_info("NAND device: Manufacturer ID:"
		" 0x%02x, Chip ID: 0x%02x (%s %s)\n", *maf_id, *dev_id,
		nand_manuf_ids[maf_idx].name,
		chip->onfi_version ? chip->onfi_params.model : type->name);

	return type;
}

int nand_scan_ident(struct mtd_info *mtd, int maxchips,
		    struct nand_flash_dev *table)
{
	int i, busw, nand_maf_id, nand_dev_id;
	struct nand_chip *chip = mtd->priv;
	struct nand_flash_dev *type;

	
	busw = chip->options & NAND_BUSWIDTH_16;
	
	nand_set_defaults(chip, busw);

	
	type = nand_get_flash_type(mtd, chip, busw,
				&nand_maf_id, &nand_dev_id, table);

	if (IS_ERR(type)) {
		if (!(chip->options & NAND_SCAN_SILENT_NODEV))
			pr_warn("No NAND device found\n");
		chip->select_chip(mtd, -1);
		return PTR_ERR(type);
	}

	
	for (i = 1; i < maxchips; i++) {
		chip->select_chip(mtd, i);
		
		chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);
		
		chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);
		
		if (nand_maf_id != chip->read_byte(mtd) ||
		    nand_dev_id != chip->read_byte(mtd))
			break;
	}
	if (i > 1)
		pr_info("%d NAND chips detected\n", i);

	
	chip->numchips = i;
	mtd->size = i * chip->chipsize;

	return 0;
}
EXPORT_SYMBOL(nand_scan_ident);


int nand_scan_tail(struct mtd_info *mtd)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	
	BUG_ON((chip->bbt_options & NAND_BBT_NO_OOB_BBM) &&
			!(chip->bbt_options & NAND_BBT_USE_FLASH));

	if (!(chip->options & NAND_OWN_BUFFERS))
		chip->buffers = kmalloc(sizeof(*chip->buffers), GFP_KERNEL);
	if (!chip->buffers)
		return -ENOMEM;

	
	chip->oob_poi = chip->buffers->databuf + mtd->writesize;

	if (!chip->ecc.layout && (chip->ecc.mode != NAND_ECC_SOFT_BCH)) {
		switch (mtd->oobsize) {
		case 8:
			chip->ecc.layout = &nand_oob_8;
			break;
		case 16:
			chip->ecc.layout = &nand_oob_16;
			break;
		case 64:
			chip->ecc.layout = &nand_oob_64;
			break;
		case 128:
			chip->ecc.layout = &nand_oob_128;
			break;
		default:
			pr_warn("No oob scheme defined for oobsize %d\n",
				   mtd->oobsize);
			BUG();
		}
	}

	if (!chip->write_page)
		chip->write_page = nand_write_page;


	switch (chip->ecc.mode) {
	case NAND_ECC_HW_OOB_FIRST:
		
		if (!chip->ecc.calculate || !chip->ecc.correct ||
		     !chip->ecc.hwctl) {
			pr_warn("No ECC functions supplied; "
				   "hardware ECC not possible\n");
			BUG();
		}
		if (!chip->ecc.read_page)
			chip->ecc.read_page = nand_read_page_hwecc_oob_first;

	case NAND_ECC_HW:
		
		if (!chip->ecc.read_page)
			chip->ecc.read_page = nand_read_page_hwecc;
		if (!chip->ecc.write_page)
			chip->ecc.write_page = nand_write_page_hwecc;
		if (!chip->ecc.read_page_raw)
			chip->ecc.read_page_raw = nand_read_page_raw;
		if (!chip->ecc.write_page_raw)
			chip->ecc.write_page_raw = nand_write_page_raw;
		if (!chip->ecc.read_oob)
			chip->ecc.read_oob = nand_read_oob_std;
		if (!chip->ecc.write_oob)
			chip->ecc.write_oob = nand_write_oob_std;

	case NAND_ECC_HW_SYNDROME:
		if ((!chip->ecc.calculate || !chip->ecc.correct ||
		     !chip->ecc.hwctl) &&
		    (!chip->ecc.read_page ||
		     chip->ecc.read_page == nand_read_page_hwecc ||
		     !chip->ecc.write_page ||
		     chip->ecc.write_page == nand_write_page_hwecc)) {
			pr_warn("No ECC functions supplied; "
				   "hardware ECC not possible\n");
			BUG();
		}
		
		if (!chip->ecc.read_page)
			chip->ecc.read_page = nand_read_page_syndrome;
		if (!chip->ecc.write_page)
			chip->ecc.write_page = nand_write_page_syndrome;
		if (!chip->ecc.read_page_raw)
			chip->ecc.read_page_raw = nand_read_page_raw_syndrome;
		if (!chip->ecc.write_page_raw)
			chip->ecc.write_page_raw = nand_write_page_raw_syndrome;
		if (!chip->ecc.read_oob)
			chip->ecc.read_oob = nand_read_oob_syndrome;
		if (!chip->ecc.write_oob)
			chip->ecc.write_oob = nand_write_oob_syndrome;

		if (mtd->writesize >= chip->ecc.size)
			break;
		pr_warn("%d byte HW ECC not possible on "
			   "%d byte page size, fallback to SW ECC\n",
			   chip->ecc.size, mtd->writesize);
		chip->ecc.mode = NAND_ECC_SOFT;

	case NAND_ECC_SOFT:
		chip->ecc.calculate = nand_calculate_ecc;
		chip->ecc.correct = nand_correct_data;
		chip->ecc.read_page = nand_read_page_swecc;
		chip->ecc.read_subpage = nand_read_subpage;
		chip->ecc.write_page = nand_write_page_swecc;
		chip->ecc.read_page_raw = nand_read_page_raw;
		chip->ecc.write_page_raw = nand_write_page_raw;
		chip->ecc.read_oob = nand_read_oob_std;
		chip->ecc.write_oob = nand_write_oob_std;
		if (!chip->ecc.size)
			chip->ecc.size = 256;
		chip->ecc.bytes = 3;
		chip->ecc.strength = 1;
		break;

	case NAND_ECC_SOFT_BCH:
		if (!mtd_nand_has_bch()) {
			pr_warn("CONFIG_MTD_ECC_BCH not enabled\n");
			BUG();
		}
		chip->ecc.calculate = nand_bch_calculate_ecc;
		chip->ecc.correct = nand_bch_correct_data;
		chip->ecc.read_page = nand_read_page_swecc;
		chip->ecc.read_subpage = nand_read_subpage;
		chip->ecc.write_page = nand_write_page_swecc;
		chip->ecc.read_page_raw = nand_read_page_raw;
		chip->ecc.write_page_raw = nand_write_page_raw;
		chip->ecc.read_oob = nand_read_oob_std;
		chip->ecc.write_oob = nand_write_oob_std;
		if (!chip->ecc.size && (mtd->oobsize >= 64)) {
			chip->ecc.size = 512;
			chip->ecc.bytes = 7;
		}
		chip->ecc.priv = nand_bch_init(mtd,
					       chip->ecc.size,
					       chip->ecc.bytes,
					       &chip->ecc.layout);
		if (!chip->ecc.priv) {
			pr_warn("BCH ECC initialization failed!\n");
			BUG();
		}
		chip->ecc.strength =
			chip->ecc.bytes*8 / fls(8*chip->ecc.size);
		break;

	case NAND_ECC_NONE:
		pr_warn("NAND_ECC_NONE selected by board driver. "
			   "This is not recommended!\n");
		chip->ecc.read_page = nand_read_page_raw;
		chip->ecc.write_page = nand_write_page_raw;
		chip->ecc.read_oob = nand_read_oob_std;
		chip->ecc.read_page_raw = nand_read_page_raw;
		chip->ecc.write_page_raw = nand_write_page_raw;
		chip->ecc.write_oob = nand_write_oob_std;
		chip->ecc.size = mtd->writesize;
		chip->ecc.bytes = 0;
		chip->ecc.strength = 0;
		break;

	default:
		pr_warn("Invalid NAND_ECC_MODE %d\n", chip->ecc.mode);
		BUG();
	}

	
	if (!chip->ecc.read_oob_raw)
		chip->ecc.read_oob_raw = chip->ecc.read_oob;
	if (!chip->ecc.write_oob_raw)
		chip->ecc.write_oob_raw = chip->ecc.write_oob;

	chip->ecc.layout->oobavail = 0;
	for (i = 0; chip->ecc.layout->oobfree[i].length
			&& i < ARRAY_SIZE(chip->ecc.layout->oobfree); i++)
		chip->ecc.layout->oobavail +=
			chip->ecc.layout->oobfree[i].length;
	mtd->oobavail = chip->ecc.layout->oobavail;

	chip->ecc.steps = mtd->writesize / chip->ecc.size;
	if (chip->ecc.steps * chip->ecc.size != mtd->writesize) {
		pr_warn("Invalid ECC parameters\n");
		BUG();
	}
	chip->ecc.total = chip->ecc.steps * chip->ecc.bytes;

	
	if (!(chip->options & NAND_NO_SUBPAGE_WRITE) &&
	    !(chip->cellinfo & NAND_CI_CELLTYPE_MSK)) {
		switch (chip->ecc.steps) {
		case 2:
			mtd->subpage_sft = 1;
			break;
		case 4:
		case 8:
		case 16:
			mtd->subpage_sft = 2;
			break;
		}
	}
	chip->subpagesize = mtd->writesize >> mtd->subpage_sft;

	
	chip->state = FL_READY;

	
	chip->select_chip(mtd, -1);

	
	chip->pagebuf = -1;

	
	mtd->type = MTD_NANDFLASH;
	mtd->flags = (chip->options & NAND_ROM) ? MTD_CAP_ROM :
						MTD_CAP_NANDFLASH;
	mtd->_erase = nand_erase;
	mtd->_point = NULL;
	mtd->_unpoint = NULL;
	mtd->_read = nand_read;
	mtd->_write = nand_write;
	mtd->_panic_write = panic_nand_write;
	mtd->_read_oob = nand_read_oob;
	mtd->_write_oob = nand_write_oob;
	mtd->_sync = nand_sync;
	mtd->_lock = NULL;
	mtd->_unlock = NULL;
	mtd->_suspend = nand_suspend;
	mtd->_resume = nand_resume;
	mtd->_block_isbad = nand_block_isbad;
	mtd->_block_markbad = nand_block_markbad;
	mtd->writebufsize = mtd->writesize;

	
	mtd->ecclayout = chip->ecc.layout;
	mtd->ecc_strength = chip->ecc.strength * chip->ecc.steps;

	
	if (chip->options & NAND_SKIP_BBTSCAN)
		return 0;

	
	return chip->scan_bbt(mtd);
}
EXPORT_SYMBOL(nand_scan_tail);

#ifdef MODULE
#define caller_is_module() (1)
#else
#define caller_is_module() \
	is_module_text_address((unsigned long)__builtin_return_address(0))
#endif

int nand_scan(struct mtd_info *mtd, int maxchips)
{
	int ret;

	
	if (!mtd->owner && caller_is_module()) {
		pr_crit("%s called with NULL mtd->owner!\n", __func__);
		BUG();
	}

	ret = nand_scan_ident(mtd, maxchips, NULL);
	if (!ret)
		ret = nand_scan_tail(mtd);
	return ret;
}
EXPORT_SYMBOL(nand_scan);

void nand_release(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	if (chip->ecc.mode == NAND_ECC_SOFT_BCH)
		nand_bch_free((struct nand_bch_control *)chip->ecc.priv);

	mtd_device_unregister(mtd);

	
	kfree(chip->bbt);
	if (!(chip->options & NAND_OWN_BUFFERS))
		kfree(chip->buffers);

	
	if (chip->badblock_pattern && chip->badblock_pattern->options
			& NAND_BBT_DYNAMICSTRUCT)
		kfree(chip->badblock_pattern);
}
EXPORT_SYMBOL_GPL(nand_release);

static int __init nand_base_init(void)
{
	led_trigger_register_simple("nand-disk", &nand_led_trigger);
	return 0;
}

static void __exit nand_base_exit(void)
{
	led_trigger_unregister_simple(nand_led_trigger);
}

module_init(nand_base_init);
module_exit(nand_base_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven J. Hill <sjhill@realitydiluted.com>");
MODULE_AUTHOR("Thomas Gleixner <tglx@linutronix.de>");
MODULE_DESCRIPTION("Generic NAND flash driver code");
