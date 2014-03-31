/*
 *  linux/drivers/mtd/onenand/onenand_base.c
 *
 *  Copyright © 2005-2009 Samsung Electronics
 *  Copyright © 2007 Nokia Corporation
 *
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *
 *  Credits:
 *	Adrian Hunter <ext-adrian.hunter@nokia.com>:
 *	auto-placement support, read-while load support, various fixes
 *
 *	Vishak G <vishak.g at samsung.com>, Rohit Hagargundgi <h.rohit at samsung.com>
 *	Flex-OneNAND support
 *	Amul Kumar Saha <amul.saha at samsung.com>
 *	OTP support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/onenand.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>

#define MB_ERASE_MIN_BLK_COUNT 2
#define MB_ERASE_MAX_BLK_COUNT 64

static int flex_bdry[MAX_DIES * 2] = { -1, 0, -1, 0 };

module_param_array(flex_bdry, int, NULL, 0400);
MODULE_PARM_DESC(flex_bdry,	"SLC Boundary information for Flex-OneNAND"
				"Syntax:flex_bdry=DIE_BDRY,LOCK,..."
				"DIE_BDRY: SLC boundary of the die"
				"LOCK: Locking information for SLC boundary"
				"    : 0->Set boundary in unlocked status"
				"    : 1->Set boundary in locked status");

static int otp;

module_param(otp, int, 0400);
MODULE_PARM_DESC(otp,	"Corresponding behaviour of OneNAND in OTP"
			"Syntax : otp=LOCK_TYPE"
			"LOCK_TYPE : Keys issued, for specific OTP Lock type"
			"	   : 0 -> Default (No Blocks Locked)"
			"	   : 1 -> OTP Block lock"
			"	   : 2 -> 1st Block lock"
			"	   : 3 -> BOTH OTP Block and 1st Block lock");

static struct nand_ecclayout flexonenand_oob_128 = {
	.eccbytes	= 64,
	.eccpos		= {
		6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
		38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
		54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
		70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
		86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
		102, 103, 104, 105
		},
	.oobfree	= {
		{2, 4}, {18, 4}, {34, 4}, {50, 4},
		{66, 4}, {82, 4}, {98, 4}, {114, 4}
	}
};

static struct nand_ecclayout onenand_oob_128 = {
	.eccbytes	= 64,
	.eccpos		= {
		7, 8, 9, 10, 11, 12, 13, 14, 15,
		23, 24, 25, 26, 27, 28, 29, 30, 31,
		39, 40, 41, 42, 43, 44, 45, 46, 47,
		55, 56, 57, 58, 59, 60, 61, 62, 63,
		71, 72, 73, 74, 75, 76, 77, 78, 79,
		87, 88, 89, 90, 91, 92, 93, 94, 95,
		103, 104, 105, 106, 107, 108, 109, 110, 111,
		119
	},
	.oobfree	= {
		{2, 3}, {18, 3}, {34, 3}, {50, 3},
		{66, 3}, {82, 3}, {98, 3}, {114, 3}
	}
};

static struct nand_ecclayout onenand_oob_64 = {
	.eccbytes	= 20,
	.eccpos		= {
		8, 9, 10, 11, 12,
		24, 25, 26, 27, 28,
		40, 41, 42, 43, 44,
		56, 57, 58, 59, 60,
		},
	.oobfree	= {
		{2, 3}, {14, 2}, {18, 3}, {30, 2},
		{34, 3}, {46, 2}, {50, 3}, {62, 2}
	}
};

static struct nand_ecclayout onenand_oob_32 = {
	.eccbytes	= 10,
	.eccpos		= {
		8, 9, 10, 11, 12,
		24, 25, 26, 27, 28,
		},
	.oobfree	= { {2, 3}, {14, 2}, {18, 3}, {30, 2} }
};

static const unsigned char ffchars[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	
};

static unsigned short onenand_readw(void __iomem *addr)
{
	return readw(addr);
}

static void onenand_writew(unsigned short value, void __iomem *addr)
{
	writew(value, addr);
}

static int onenand_block_address(struct onenand_chip *this, int block)
{
	
	if (block & this->density_mask)
		return ONENAND_DDP_CHIP1 | (block ^ this->density_mask);

	return block;
}

static int onenand_bufferram_address(struct onenand_chip *this, int block)
{
	
	if (block & this->density_mask)
		return ONENAND_DDP_CHIP1;

	return ONENAND_DDP_CHIP0;
}

static int onenand_page_address(int page, int sector)
{
	
	int fpa, fsa;

	fpa = page & ONENAND_FPA_MASK;
	fsa = sector & ONENAND_FSA_MASK;

	return ((fpa << ONENAND_FPA_SHIFT) | fsa);
}

static int onenand_buffer_address(int dataram1, int sectors, int count)
{
	int bsa, bsc;

	
	bsa = sectors & ONENAND_BSA_MASK;

	if (dataram1)
		bsa |= ONENAND_BSA_DATARAM1;	
	else
		bsa |= ONENAND_BSA_DATARAM0;	

	
	bsc = count & ONENAND_BSC_MASK;

	return ((bsa << ONENAND_BSA_SHIFT) | bsc);
}

static unsigned flexonenand_block(struct onenand_chip *this, loff_t addr)
{
	unsigned boundary, blk, die = 0;

	if (ONENAND_IS_DDP(this) && addr >= this->diesize[0]) {
		die = 1;
		addr -= this->diesize[0];
	}

	boundary = this->boundary[die];

	blk = addr >> (this->erase_shift - 1);
	if (blk > boundary)
		blk = (blk + boundary + 1) >> 1;

	blk += die ? this->density_mask : 0;
	return blk;
}

inline unsigned onenand_block(struct onenand_chip *this, loff_t addr)
{
	if (!FLEXONENAND(this))
		return addr >> this->erase_shift;
	return flexonenand_block(this, addr);
}

static loff_t flexonenand_addr(struct onenand_chip *this, int block)
{
	loff_t ofs = 0;
	int die = 0, boundary;

	if (ONENAND_IS_DDP(this) && block >= this->density_mask) {
		block -= this->density_mask;
		die = 1;
		ofs = this->diesize[0];
	}

	boundary = this->boundary[die];
	ofs += (loff_t)block << (this->erase_shift - 1);
	if (block > (boundary + 1))
		ofs += (loff_t)(block - boundary - 1) << (this->erase_shift - 1);
	return ofs;
}

loff_t onenand_addr(struct onenand_chip *this, int block)
{
	if (!FLEXONENAND(this))
		return (loff_t)block << this->erase_shift;
	return flexonenand_addr(this, block);
}
EXPORT_SYMBOL(onenand_addr);

static inline int onenand_get_density(int dev_id)
{
	int density = dev_id >> ONENAND_DEVICE_DENSITY_SHIFT;
	return (density & ONENAND_DEVICE_DENSITY_MASK);
}

int flexonenand_region(struct mtd_info *mtd, loff_t addr)
{
	int i;

	for (i = 0; i < mtd->numeraseregions; i++)
		if (addr < mtd->eraseregions[i].offset)
			break;
	return i - 1;
}
EXPORT_SYMBOL(flexonenand_region);

static int onenand_command(struct mtd_info *mtd, int cmd, loff_t addr, size_t len)
{
	struct onenand_chip *this = mtd->priv;
	int value, block, page;

	
	switch (cmd) {
	case ONENAND_CMD_UNLOCK:
	case ONENAND_CMD_LOCK:
	case ONENAND_CMD_LOCK_TIGHT:
	case ONENAND_CMD_UNLOCK_ALL:
		block = -1;
		page = -1;
		break;

	case FLEXONENAND_CMD_PI_ACCESS:
		
		block = addr * this->density_mask;
		page = -1;
		break;

	case ONENAND_CMD_ERASE:
	case ONENAND_CMD_MULTIBLOCK_ERASE:
	case ONENAND_CMD_ERASE_VERIFY:
	case ONENAND_CMD_BUFFERRAM:
	case ONENAND_CMD_OTP_ACCESS:
		block = onenand_block(this, addr);
		page = -1;
		break;

	case FLEXONENAND_CMD_READ_PI:
		cmd = ONENAND_CMD_READ;
		block = addr * this->density_mask;
		page = 0;
		break;

	default:
		block = onenand_block(this, addr);
		if (FLEXONENAND(this))
			page = (int) (addr - onenand_addr(this, block))>>\
				this->page_shift;
		else
			page = (int) (addr >> this->page_shift);
		if (ONENAND_IS_2PLANE(this)) {
			
			block &= ~1;
			
			if (addr & this->writesize)
				block++;
			page >>= 1;
		}
		page &= this->page_mask;
		break;
	}

	
	if (cmd == ONENAND_CMD_BUFFERRAM) {
		
		value = onenand_bufferram_address(this, block);
		this->write_word(value, this->base + ONENAND_REG_START_ADDRESS2);

		if (ONENAND_IS_2PLANE(this) || ONENAND_IS_4KB_PAGE(this))
			
			ONENAND_SET_BUFFERRAM0(this);
		else
			
			ONENAND_SET_NEXT_BUFFERRAM(this);

		return 0;
	}

	if (block != -1) {
		
		value = onenand_block_address(this, block);
		this->write_word(value, this->base + ONENAND_REG_START_ADDRESS1);

		
		value = onenand_bufferram_address(this, block);
		this->write_word(value, this->base + ONENAND_REG_START_ADDRESS2);
	}

	if (page != -1) {
		
		int sectors = 0, count = 0;
		int dataram;

		switch (cmd) {
		case FLEXONENAND_CMD_RECOVER_LSB:
		case ONENAND_CMD_READ:
		case ONENAND_CMD_READOOB:
			if (ONENAND_IS_4KB_PAGE(this))
				
				dataram = ONENAND_SET_BUFFERRAM0(this);
			else
				dataram = ONENAND_SET_NEXT_BUFFERRAM(this);
			break;

		default:
			if (ONENAND_IS_2PLANE(this) && cmd == ONENAND_CMD_PROG)
				cmd = ONENAND_CMD_2X_PROG;
			dataram = ONENAND_CURRENT_BUFFERRAM(this);
			break;
		}

		
		value = onenand_page_address(page, sectors);
		this->write_word(value, this->base + ONENAND_REG_START_ADDRESS8);

		
		value = onenand_buffer_address(dataram, sectors, count);
		this->write_word(value, this->base + ONENAND_REG_START_BUFFER);
	}

	
	this->write_word(ONENAND_INT_CLEAR, this->base + ONENAND_REG_INTERRUPT);

	
	this->write_word(cmd, this->base + ONENAND_REG_COMMAND);

	return 0;
}

static inline int onenand_read_ecc(struct onenand_chip *this)
{
	int ecc, i, result = 0;

	if (!FLEXONENAND(this) && !ONENAND_IS_4KB_PAGE(this))
		return this->read_word(this->base + ONENAND_REG_ECC_STATUS);

	for (i = 0; i < 4; i++) {
		ecc = this->read_word(this->base + ONENAND_REG_ECC_STATUS + i*2);
		if (likely(!ecc))
			continue;
		if (ecc & FLEXONENAND_UNCORRECTABLE_ERROR)
			return ONENAND_ECC_2BIT_ALL;
		else
			result = ONENAND_ECC_1BIT_ALL;
	}

	return result;
}

static int onenand_wait(struct mtd_info *mtd, int state)
{
	struct onenand_chip * this = mtd->priv;
	unsigned long timeout;
	unsigned int flags = ONENAND_INT_MASTER;
	unsigned int interrupt = 0;
	unsigned int ctrl;

	
	timeout = jiffies + msecs_to_jiffies(20);
	while (time_before(jiffies, timeout)) {
		interrupt = this->read_word(this->base + ONENAND_REG_INTERRUPT);

		if (interrupt & flags)
			break;

		if (state != FL_READING && state != FL_PREPARING_ERASE)
			cond_resched();
	}
	
	interrupt = this->read_word(this->base + ONENAND_REG_INTERRUPT);

	ctrl = this->read_word(this->base + ONENAND_REG_CTRL_STATUS);

	if (interrupt & ONENAND_INT_READ) {
		int ecc = onenand_read_ecc(this);
		if (ecc) {
			if (ecc & ONENAND_ECC_2BIT_ALL) {
				printk(KERN_ERR "%s: ECC error = 0x%04x\n",
					__func__, ecc);
				mtd->ecc_stats.failed++;
				return -EBADMSG;
			} else if (ecc & ONENAND_ECC_1BIT_ALL) {
				printk(KERN_DEBUG "%s: correctable ECC error = 0x%04x\n",
					__func__, ecc);
				mtd->ecc_stats.corrected++;
			}
		}
	} else if (state == FL_READING) {
		printk(KERN_ERR "%s: read timeout! ctrl=0x%04x intr=0x%04x\n",
			__func__, ctrl, interrupt);
		return -EIO;
	}

	if (state == FL_PREPARING_ERASE && !(interrupt & ONENAND_INT_ERASE)) {
		printk(KERN_ERR "%s: mb erase timeout! ctrl=0x%04x intr=0x%04x\n",
		       __func__, ctrl, interrupt);
		return -EIO;
	}

	if (!(interrupt & ONENAND_INT_MASTER)) {
		printk(KERN_ERR "%s: timeout! ctrl=0x%04x intr=0x%04x\n",
		       __func__, ctrl, interrupt);
		return -EIO;
	}

	
	if (ctrl & ONENAND_CTRL_ERROR) {
		printk(KERN_ERR "%s: controller error = 0x%04x\n",
			__func__, ctrl);
		if (ctrl & ONENAND_CTRL_LOCK)
			printk(KERN_ERR "%s: it's locked error.\n", __func__);
		return -EIO;
	}

	return 0;
}

static irqreturn_t onenand_interrupt(int irq, void *data)
{
	struct onenand_chip *this = data;

	
	if (!this->complete.done)
		complete(&this->complete);

	return IRQ_HANDLED;
}

static int onenand_interrupt_wait(struct mtd_info *mtd, int state)
{
	struct onenand_chip *this = mtd->priv;

	wait_for_completion(&this->complete);

	return onenand_wait(mtd, state);
}

static int onenand_try_interrupt_wait(struct mtd_info *mtd, int state)
{
	struct onenand_chip *this = mtd->priv;
	unsigned long remain, timeout;

	
	this->wait = onenand_interrupt_wait;

	timeout = msecs_to_jiffies(100);
	remain = wait_for_completion_timeout(&this->complete, timeout);
	if (!remain) {
		printk(KERN_INFO "OneNAND: There's no interrupt. "
				"We use the normal wait\n");

		
		free_irq(this->irq, this);

		this->wait = onenand_wait;
	}

	return onenand_wait(mtd, state);
}

static void onenand_setup_wait(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;
	int syscfg;

	init_completion(&this->complete);

	if (this->irq <= 0) {
		this->wait = onenand_wait;
		return;
	}

	if (request_irq(this->irq, &onenand_interrupt,
				IRQF_SHARED, "onenand", this)) {
		
		this->wait = onenand_wait;
		return;
	}

	
	syscfg = this->read_word(this->base + ONENAND_REG_SYS_CFG1);
	syscfg |= ONENAND_SYS_CFG1_IOBE;
	this->write_word(syscfg, this->base + ONENAND_REG_SYS_CFG1);

	this->wait = onenand_try_interrupt_wait;
}

static inline int onenand_bufferram_offset(struct mtd_info *mtd, int area)
{
	struct onenand_chip *this = mtd->priv;

	if (ONENAND_CURRENT_BUFFERRAM(this)) {
		
		if (area == ONENAND_DATARAM)
			return this->writesize;
		if (area == ONENAND_SPARERAM)
			return mtd->oobsize;
	}

	return 0;
}

static int onenand_read_bufferram(struct mtd_info *mtd, int area,
		unsigned char *buffer, int offset, size_t count)
{
	struct onenand_chip *this = mtd->priv;
	void __iomem *bufferram;

	bufferram = this->base + area;

	bufferram += onenand_bufferram_offset(mtd, area);

	if (ONENAND_CHECK_BYTE_ACCESS(count)) {
		unsigned short word;

		
		count--;

		
		word = this->read_word(bufferram + offset + count);
		buffer[count] = (word & 0xff);
	}

	memcpy(buffer, bufferram + offset, count);

	return 0;
}

static int onenand_sync_read_bufferram(struct mtd_info *mtd, int area,
		unsigned char *buffer, int offset, size_t count)
{
	struct onenand_chip *this = mtd->priv;
	void __iomem *bufferram;

	bufferram = this->base + area;

	bufferram += onenand_bufferram_offset(mtd, area);

	this->mmcontrol(mtd, ONENAND_SYS_CFG1_SYNC_READ);

	if (ONENAND_CHECK_BYTE_ACCESS(count)) {
		unsigned short word;

		
		count--;

		
		word = this->read_word(bufferram + offset + count);
		buffer[count] = (word & 0xff);
	}

	memcpy(buffer, bufferram + offset, count);

	this->mmcontrol(mtd, 0);

	return 0;
}

static int onenand_write_bufferram(struct mtd_info *mtd, int area,
		const unsigned char *buffer, int offset, size_t count)
{
	struct onenand_chip *this = mtd->priv;
	void __iomem *bufferram;

	bufferram = this->base + area;

	bufferram += onenand_bufferram_offset(mtd, area);

	if (ONENAND_CHECK_BYTE_ACCESS(count)) {
		unsigned short word;
		int byte_offset;

		
		count--;

		
		byte_offset = offset + count;

		
		word = this->read_word(bufferram + byte_offset);
		word = (word & ~0xff) | buffer[count];
		this->write_word(word, bufferram + byte_offset);
	}

	memcpy(bufferram + offset, buffer, count);

	return 0;
}

static int onenand_get_2x_blockpage(struct mtd_info *mtd, loff_t addr)
{
	struct onenand_chip *this = mtd->priv;
	int blockpage, block, page;

	
	block = (int) (addr >> this->erase_shift) & ~1;
	
	if (addr & this->writesize)
		block++;
	page = (int) (addr >> (this->page_shift + 1)) & this->page_mask;
	blockpage = (block << 7) | page;

	return blockpage;
}

static int onenand_check_bufferram(struct mtd_info *mtd, loff_t addr)
{
	struct onenand_chip *this = mtd->priv;
	int blockpage, found = 0;
	unsigned int i;

	if (ONENAND_IS_2PLANE(this))
		blockpage = onenand_get_2x_blockpage(mtd, addr);
	else
		blockpage = (int) (addr >> this->page_shift);

	
	i = ONENAND_CURRENT_BUFFERRAM(this);
	if (this->bufferram[i].blockpage == blockpage)
		found = 1;
	else {
		
		i = ONENAND_NEXT_BUFFERRAM(this);
		if (this->bufferram[i].blockpage == blockpage) {
			ONENAND_SET_NEXT_BUFFERRAM(this);
			found = 1;
		}
	}

	if (found && ONENAND_IS_DDP(this)) {
		
		int block = onenand_block(this, addr);
		int value = onenand_bufferram_address(this, block);
		this->write_word(value, this->base + ONENAND_REG_START_ADDRESS2);
	}

	return found;
}

static void onenand_update_bufferram(struct mtd_info *mtd, loff_t addr,
		int valid)
{
	struct onenand_chip *this = mtd->priv;
	int blockpage;
	unsigned int i;

	if (ONENAND_IS_2PLANE(this))
		blockpage = onenand_get_2x_blockpage(mtd, addr);
	else
		blockpage = (int) (addr >> this->page_shift);

	
	i = ONENAND_NEXT_BUFFERRAM(this);
	if (this->bufferram[i].blockpage == blockpage)
		this->bufferram[i].blockpage = -1;

	
	i = ONENAND_CURRENT_BUFFERRAM(this);
	if (valid)
		this->bufferram[i].blockpage = blockpage;
	else
		this->bufferram[i].blockpage = -1;
}

static void onenand_invalidate_bufferram(struct mtd_info *mtd, loff_t addr,
		unsigned int len)
{
	struct onenand_chip *this = mtd->priv;
	int i;
	loff_t end_addr = addr + len;

	
	for (i = 0; i < MAX_BUFFERRAM; i++) {
		loff_t buf_addr = this->bufferram[i].blockpage << this->page_shift;
		if (buf_addr >= addr && buf_addr < end_addr)
			this->bufferram[i].blockpage = -1;
	}
}

static int onenand_get_device(struct mtd_info *mtd, int new_state)
{
	struct onenand_chip *this = mtd->priv;
	DECLARE_WAITQUEUE(wait, current);

	while (1) {
		spin_lock(&this->chip_lock);
		if (this->state == FL_READY) {
			this->state = new_state;
			spin_unlock(&this->chip_lock);
			if (new_state != FL_PM_SUSPENDED && this->enable)
				this->enable(mtd);
			break;
		}
		if (new_state == FL_PM_SUSPENDED) {
			spin_unlock(&this->chip_lock);
			return (this->state == FL_PM_SUSPENDED) ? 0 : -EAGAIN;
		}
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&this->wq, &wait);
		spin_unlock(&this->chip_lock);
		schedule();
		remove_wait_queue(&this->wq, &wait);
	}

	return 0;
}

static void onenand_release_device(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;

	if (this->state != FL_PM_SUSPENDED && this->disable)
		this->disable(mtd);
	
	spin_lock(&this->chip_lock);
	this->state = FL_READY;
	wake_up(&this->wq);
	spin_unlock(&this->chip_lock);
}

static int onenand_transfer_auto_oob(struct mtd_info *mtd, uint8_t *buf, int column,
				int thislen)
{
	struct onenand_chip *this = mtd->priv;
	struct nand_oobfree *free;
	int readcol = column;
	int readend = column + thislen;
	int lastgap = 0;
	unsigned int i;
	uint8_t *oob_buf = this->oob_buf;

	free = this->ecclayout->oobfree;
	for (i = 0; i < MTD_MAX_OOBFREE_ENTRIES && free->length; i++, free++) {
		if (readcol >= lastgap)
			readcol += free->offset - lastgap;
		if (readend >= lastgap)
			readend += free->offset - lastgap;
		lastgap = free->offset + free->length;
	}
	this->read_bufferram(mtd, ONENAND_SPARERAM, oob_buf, 0, mtd->oobsize);
	free = this->ecclayout->oobfree;
	for (i = 0; i < MTD_MAX_OOBFREE_ENTRIES && free->length; i++, free++) {
		int free_end = free->offset + free->length;
		if (free->offset < readend && free_end > readcol) {
			int st = max_t(int,free->offset,readcol);
			int ed = min_t(int,free_end,readend);
			int n = ed - st;
			memcpy(buf, oob_buf + st, n);
			buf += n;
		} else if (column == 0)
			break;
	}
	return 0;
}

static int onenand_recover_lsb(struct mtd_info *mtd, loff_t addr, int status)
{
	struct onenand_chip *this = mtd->priv;
	int i;

	
	if (!FLEXONENAND(this))
		return status;

	
	if (!mtd_is_eccerr(status) && status != ONENAND_BBT_READ_ECC_ERROR)
		return status;

	
	i = flexonenand_region(mtd, addr);
	if (mtd->eraseregions[i].erasesize < (1 << this->erase_shift))
		return status;

	printk(KERN_INFO "%s: Attempting to recover from uncorrectable read\n",
		__func__);
	mtd->ecc_stats.failed--;

	
	this->command(mtd, FLEXONENAND_CMD_RECOVER_LSB, addr, this->writesize);
	return this->wait(mtd, FL_READING);
}

static int onenand_mlc_read_ops_nolock(struct mtd_info *mtd, loff_t from,
				struct mtd_oob_ops *ops)
{
	struct onenand_chip *this = mtd->priv;
	struct mtd_ecc_stats stats;
	size_t len = ops->len;
	size_t ooblen = ops->ooblen;
	u_char *buf = ops->datbuf;
	u_char *oobbuf = ops->oobbuf;
	int read = 0, column, thislen;
	int oobread = 0, oobcolumn, thisooblen, oobsize;
	int ret = 0;
	int writesize = this->writesize;

	pr_debug("%s: from = 0x%08x, len = %i\n", __func__, (unsigned int)from,
			(int)len);

	if (ops->mode == MTD_OPS_AUTO_OOB)
		oobsize = this->ecclayout->oobavail;
	else
		oobsize = mtd->oobsize;

	oobcolumn = from & (mtd->oobsize - 1);

	
	if (from + len > mtd->size) {
		printk(KERN_ERR "%s: Attempt read beyond end of device\n",
			__func__);
		ops->retlen = 0;
		ops->oobretlen = 0;
		return -EINVAL;
	}

	stats = mtd->ecc_stats;

	while (read < len) {
		cond_resched();

		thislen = min_t(int, writesize, len - read);

		column = from & (writesize - 1);
		if (column + thislen > writesize)
			thislen = writesize - column;

		if (!onenand_check_bufferram(mtd, from)) {
			this->command(mtd, ONENAND_CMD_READ, from, writesize);

			ret = this->wait(mtd, FL_READING);
			if (unlikely(ret))
				ret = onenand_recover_lsb(mtd, from, ret);
			onenand_update_bufferram(mtd, from, !ret);
			if (mtd_is_eccerr(ret))
				ret = 0;
			if (ret)
				break;
		}

		this->read_bufferram(mtd, ONENAND_DATARAM, buf, column, thislen);
		if (oobbuf) {
			thisooblen = oobsize - oobcolumn;
			thisooblen = min_t(int, thisooblen, ooblen - oobread);

			if (ops->mode == MTD_OPS_AUTO_OOB)
				onenand_transfer_auto_oob(mtd, oobbuf, oobcolumn, thisooblen);
			else
				this->read_bufferram(mtd, ONENAND_SPARERAM, oobbuf, oobcolumn, thisooblen);
			oobread += thisooblen;
			oobbuf += thisooblen;
			oobcolumn = 0;
		}

		read += thislen;
		if (read == len)
			break;

		from += thislen;
		buf += thislen;
	}

	ops->retlen = read;
	ops->oobretlen = oobread;

	if (ret)
		return ret;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return mtd->ecc_stats.corrected - stats.corrected ? -EUCLEAN : 0;
}

static int onenand_read_ops_nolock(struct mtd_info *mtd, loff_t from,
				struct mtd_oob_ops *ops)
{
	struct onenand_chip *this = mtd->priv;
	struct mtd_ecc_stats stats;
	size_t len = ops->len;
	size_t ooblen = ops->ooblen;
	u_char *buf = ops->datbuf;
	u_char *oobbuf = ops->oobbuf;
	int read = 0, column, thislen;
	int oobread = 0, oobcolumn, thisooblen, oobsize;
	int ret = 0, boundary = 0;
	int writesize = this->writesize;

	pr_debug("%s: from = 0x%08x, len = %i\n", __func__, (unsigned int)from,
			(int)len);

	if (ops->mode == MTD_OPS_AUTO_OOB)
		oobsize = this->ecclayout->oobavail;
	else
		oobsize = mtd->oobsize;

	oobcolumn = from & (mtd->oobsize - 1);

	
	if ((from + len) > mtd->size) {
		printk(KERN_ERR "%s: Attempt read beyond end of device\n",
			__func__);
		ops->retlen = 0;
		ops->oobretlen = 0;
		return -EINVAL;
	}

	stats = mtd->ecc_stats;

 	

 	
 	if (read < len) {
 		if (!onenand_check_bufferram(mtd, from)) {
			this->command(mtd, ONENAND_CMD_READ, from, writesize);
 			ret = this->wait(mtd, FL_READING);
 			onenand_update_bufferram(mtd, from, !ret);
			if (mtd_is_eccerr(ret))
				ret = 0;
 		}
 	}

	thislen = min_t(int, writesize, len - read);
	column = from & (writesize - 1);
	if (column + thislen > writesize)
		thislen = writesize - column;

 	while (!ret) {
 		
 		from += thislen;
 		if (read + thislen < len) {
			this->command(mtd, ONENAND_CMD_READ, from, writesize);
 			if (ONENAND_IS_DDP(this) &&
 			    unlikely(from == (this->chipsize >> 1))) {
 				this->write_word(ONENAND_DDP_CHIP0, this->base + ONENAND_REG_START_ADDRESS2);
 				boundary = 1;
 			} else
 				boundary = 0;
 			ONENAND_SET_PREV_BUFFERRAM(this);
 		}
 		
 		this->read_bufferram(mtd, ONENAND_DATARAM, buf, column, thislen);

		
		if (oobbuf) {
			thisooblen = oobsize - oobcolumn;
			thisooblen = min_t(int, thisooblen, ooblen - oobread);

			if (ops->mode == MTD_OPS_AUTO_OOB)
				onenand_transfer_auto_oob(mtd, oobbuf, oobcolumn, thisooblen);
			else
				this->read_bufferram(mtd, ONENAND_SPARERAM, oobbuf, oobcolumn, thisooblen);
			oobread += thisooblen;
			oobbuf += thisooblen;
			oobcolumn = 0;
		}

 		
 		read += thislen;
 		if (read == len)
 			break;
 		
 		if (unlikely(boundary))
 			this->write_word(ONENAND_DDP_CHIP1, this->base + ONENAND_REG_START_ADDRESS2);
 		ONENAND_SET_NEXT_BUFFERRAM(this);
 		buf += thislen;
		thislen = min_t(int, writesize, len - read);
 		column = 0;
 		cond_resched();
 		
 		ret = this->wait(mtd, FL_READING);
 		onenand_update_bufferram(mtd, from, !ret);
		if (mtd_is_eccerr(ret))
			ret = 0;
 	}

	ops->retlen = read;
	ops->oobretlen = oobread;

	if (ret)
		return ret;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return mtd->ecc_stats.corrected - stats.corrected ? -EUCLEAN : 0;
}

static int onenand_read_oob_nolock(struct mtd_info *mtd, loff_t from,
			struct mtd_oob_ops *ops)
{
	struct onenand_chip *this = mtd->priv;
	struct mtd_ecc_stats stats;
	int read = 0, thislen, column, oobsize;
	size_t len = ops->ooblen;
	unsigned int mode = ops->mode;
	u_char *buf = ops->oobbuf;
	int ret = 0, readcmd;

	from += ops->ooboffs;

	pr_debug("%s: from = 0x%08x, len = %i\n", __func__, (unsigned int)from,
			(int)len);

	
	ops->oobretlen = 0;

	if (mode == MTD_OPS_AUTO_OOB)
		oobsize = this->ecclayout->oobavail;
	else
		oobsize = mtd->oobsize;

	column = from & (mtd->oobsize - 1);

	if (unlikely(column >= oobsize)) {
		printk(KERN_ERR "%s: Attempted to start read outside oob\n",
			__func__);
		return -EINVAL;
	}

	
	if (unlikely(from >= mtd->size ||
		     column + len > ((mtd->size >> this->page_shift) -
				     (from >> this->page_shift)) * oobsize)) {
		printk(KERN_ERR "%s: Attempted to read beyond end of device\n",
			__func__);
		return -EINVAL;
	}

	stats = mtd->ecc_stats;

	readcmd = ONENAND_IS_4KB_PAGE(this) ? ONENAND_CMD_READ : ONENAND_CMD_READOOB;

	while (read < len) {
		cond_resched();

		thislen = oobsize - column;
		thislen = min_t(int, thislen, len);

		this->command(mtd, readcmd, from, mtd->oobsize);

		onenand_update_bufferram(mtd, from, 0);

		ret = this->wait(mtd, FL_READING);
		if (unlikely(ret))
			ret = onenand_recover_lsb(mtd, from, ret);

		if (ret && !mtd_is_eccerr(ret)) {
			printk(KERN_ERR "%s: read failed = 0x%x\n",
				__func__, ret);
			break;
		}

		if (mode == MTD_OPS_AUTO_OOB)
			onenand_transfer_auto_oob(mtd, buf, column, thislen);
		else
			this->read_bufferram(mtd, ONENAND_SPARERAM, buf, column, thislen);

		read += thislen;

		if (read == len)
			break;

		buf += thislen;

		
		if (read < len) {
			
			from += mtd->writesize;
			column = 0;
		}
	}

	ops->oobretlen = read;

	if (ret)
		return ret;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return 0;
}

static int onenand_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
	struct onenand_chip *this = mtd->priv;
	struct mtd_oob_ops ops = {
		.len	= len,
		.ooblen	= 0,
		.datbuf	= buf,
		.oobbuf	= NULL,
	};
	int ret;

	onenand_get_device(mtd, FL_READING);
	ret = ONENAND_IS_4KB_PAGE(this) ?
		onenand_mlc_read_ops_nolock(mtd, from, &ops) :
		onenand_read_ops_nolock(mtd, from, &ops);
	onenand_release_device(mtd);

	*retlen = ops.retlen;
	return ret;
}

static int onenand_read_oob(struct mtd_info *mtd, loff_t from,
			    struct mtd_oob_ops *ops)
{
	struct onenand_chip *this = mtd->priv;
	int ret;

	switch (ops->mode) {
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_AUTO_OOB:
		break;
	case MTD_OPS_RAW:
		
	default:
		return -EINVAL;
	}

	onenand_get_device(mtd, FL_READING);
	if (ops->datbuf)
		ret = ONENAND_IS_4KB_PAGE(this) ?
			onenand_mlc_read_ops_nolock(mtd, from, ops) :
			onenand_read_ops_nolock(mtd, from, ops);
	else
		ret = onenand_read_oob_nolock(mtd, from, ops);
	onenand_release_device(mtd);

	return ret;
}

static int onenand_bbt_wait(struct mtd_info *mtd, int state)
{
	struct onenand_chip *this = mtd->priv;
	unsigned long timeout;
	unsigned int interrupt, ctrl, ecc, addr1, addr8;

	
	timeout = jiffies + msecs_to_jiffies(20);
	while (time_before(jiffies, timeout)) {
		interrupt = this->read_word(this->base + ONENAND_REG_INTERRUPT);
		if (interrupt & ONENAND_INT_MASTER)
			break;
	}
	
	interrupt = this->read_word(this->base + ONENAND_REG_INTERRUPT);
	ctrl = this->read_word(this->base + ONENAND_REG_CTRL_STATUS);
	addr1 = this->read_word(this->base + ONENAND_REG_START_ADDRESS1);
	addr8 = this->read_word(this->base + ONENAND_REG_START_ADDRESS8);

	if (interrupt & ONENAND_INT_READ) {
		ecc = onenand_read_ecc(this);
		if (ecc & ONENAND_ECC_2BIT_ALL) {
			printk(KERN_DEBUG "%s: ecc 0x%04x ctrl 0x%04x "
			       "intr 0x%04x addr1 %#x addr8 %#x\n",
			       __func__, ecc, ctrl, interrupt, addr1, addr8);
			return ONENAND_BBT_READ_ECC_ERROR;
		}
	} else {
		printk(KERN_ERR "%s: read timeout! ctrl 0x%04x "
		       "intr 0x%04x addr1 %#x addr8 %#x\n",
		       __func__, ctrl, interrupt, addr1, addr8);
		return ONENAND_BBT_READ_FATAL_ERROR;
	}

	
	if (ctrl & ONENAND_CTRL_ERROR) {
		printk(KERN_DEBUG "%s: ctrl 0x%04x intr 0x%04x addr1 %#x "
		       "addr8 %#x\n", __func__, ctrl, interrupt, addr1, addr8);
		return ONENAND_BBT_READ_ERROR;
	}

	return 0;
}

int onenand_bbt_read_oob(struct mtd_info *mtd, loff_t from, 
			    struct mtd_oob_ops *ops)
{
	struct onenand_chip *this = mtd->priv;
	int read = 0, thislen, column;
	int ret = 0, readcmd;
	size_t len = ops->ooblen;
	u_char *buf = ops->oobbuf;

	pr_debug("%s: from = 0x%08x, len = %zi\n", __func__, (unsigned int)from,
			len);

	
	ops->oobretlen = 0;

	
	if (unlikely((from + len) > mtd->size)) {
		printk(KERN_ERR "%s: Attempt read beyond end of device\n",
			__func__);
		return ONENAND_BBT_READ_FATAL_ERROR;
	}

	
	onenand_get_device(mtd, FL_READING);

	column = from & (mtd->oobsize - 1);

	readcmd = ONENAND_IS_4KB_PAGE(this) ? ONENAND_CMD_READ : ONENAND_CMD_READOOB;

	while (read < len) {
		cond_resched();

		thislen = mtd->oobsize - column;
		thislen = min_t(int, thislen, len);

		this->command(mtd, readcmd, from, mtd->oobsize);

		onenand_update_bufferram(mtd, from, 0);

		ret = this->bbt_wait(mtd, FL_READING);
		if (unlikely(ret))
			ret = onenand_recover_lsb(mtd, from, ret);

		if (ret)
			break;

		this->read_bufferram(mtd, ONENAND_SPARERAM, buf, column, thislen);
		read += thislen;
		if (read == len)
			break;

		buf += thislen;

		
		if (read < len) {
			
			from += this->writesize;
			column = 0;
		}
	}

	
	onenand_release_device(mtd);

	ops->oobretlen = read;
	return ret;
}

#ifdef CONFIG_MTD_ONENAND_VERIFY_WRITE
static int onenand_verify_oob(struct mtd_info *mtd, const u_char *buf, loff_t to)
{
	struct onenand_chip *this = mtd->priv;
	u_char *oob_buf = this->oob_buf;
	int status, i, readcmd;

	readcmd = ONENAND_IS_4KB_PAGE(this) ? ONENAND_CMD_READ : ONENAND_CMD_READOOB;

	this->command(mtd, readcmd, to, mtd->oobsize);
	onenand_update_bufferram(mtd, to, 0);
	status = this->wait(mtd, FL_READING);
	if (status)
		return status;

	this->read_bufferram(mtd, ONENAND_SPARERAM, oob_buf, 0, mtd->oobsize);
	for (i = 0; i < mtd->oobsize; i++)
		if (buf[i] != 0xFF && buf[i] != oob_buf[i])
			return -EBADMSG;

	return 0;
}

static int onenand_verify(struct mtd_info *mtd, const u_char *buf, loff_t addr, size_t len)
{
	struct onenand_chip *this = mtd->priv;
	int ret = 0;
	int thislen, column;

	column = addr & (this->writesize - 1);

	while (len != 0) {
		thislen = min_t(int, this->writesize - column, len);

		this->command(mtd, ONENAND_CMD_READ, addr, this->writesize);

		onenand_update_bufferram(mtd, addr, 0);

		ret = this->wait(mtd, FL_READING);
		if (ret)
			return ret;

		onenand_update_bufferram(mtd, addr, 1);

		this->read_bufferram(mtd, ONENAND_DATARAM, this->verify_buf, 0, mtd->writesize);

		if (memcmp(buf, this->verify_buf + column, thislen))
			return -EBADMSG;

		len -= thislen;
		buf += thislen;
		addr += thislen;
		column = 0;
	}

	return 0;
}
#else
#define onenand_verify(...)		(0)
#define onenand_verify_oob(...)		(0)
#endif

#define NOTALIGNED(x)	((x & (this->subpagesize - 1)) != 0)

static void onenand_panic_wait(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;
	unsigned int interrupt;
	int i;
	
	for (i = 0; i < 2000; i++) {
		interrupt = this->read_word(this->base + ONENAND_REG_INTERRUPT);
		if (interrupt & ONENAND_INT_MASTER)
			break;
		udelay(10);
	}
}

/**
 * onenand_panic_write - [MTD Interface] write buffer to FLASH in a panic context
 * @param mtd		MTD device structure
 * @param to		offset to write to
 * @param len		number of bytes to write
 * @param retlen	pointer to variable to store the number of written bytes
 * @param buf		the data to write
 *
 * Write with ECC
 */
static int onenand_panic_write(struct mtd_info *mtd, loff_t to, size_t len,
			 size_t *retlen, const u_char *buf)
{
	struct onenand_chip *this = mtd->priv;
	int column, subpage;
	int written = 0;
	int ret = 0;

	if (this->state == FL_PM_SUSPENDED)
		return -EBUSY;

	
	onenand_panic_wait(mtd);

	pr_debug("%s: to = 0x%08x, len = %i\n", __func__, (unsigned int)to,
			(int)len);

	
        if (unlikely(NOTALIGNED(to) || NOTALIGNED(len))) {
		printk(KERN_ERR "%s: Attempt to write not page aligned data\n",
			__func__);
                return -EINVAL;
        }

	column = to & (mtd->writesize - 1);

	
	while (written < len) {
		int thislen = min_t(int, mtd->writesize - column, len - written);
		u_char *wbuf = (u_char *) buf;

		this->command(mtd, ONENAND_CMD_BUFFERRAM, to, thislen);

		
		subpage = thislen < mtd->writesize;
		if (subpage) {
			memset(this->page_buf, 0xff, mtd->writesize);
			memcpy(this->page_buf + column, buf, thislen);
			wbuf = this->page_buf;
		}

		this->write_bufferram(mtd, ONENAND_DATARAM, wbuf, 0, mtd->writesize);
		this->write_bufferram(mtd, ONENAND_SPARERAM, ffchars, 0, mtd->oobsize);

		this->command(mtd, ONENAND_CMD_PROG, to, mtd->writesize);

		onenand_panic_wait(mtd);

		
		onenand_update_bufferram(mtd, to, !ret && !subpage);
		if (ONENAND_IS_2PLANE(this)) {
			ONENAND_SET_BUFFERRAM1(this);
			onenand_update_bufferram(mtd, to + this->writesize, !ret && !subpage);
		}

		if (ret) {
			printk(KERN_ERR "%s: write failed %d\n", __func__, ret);
			break;
		}

		written += thislen;

		if (written == len)
			break;

		column = 0;
		to += thislen;
		buf += thislen;
	}

	*retlen = written;
	return ret;
}

static int onenand_fill_auto_oob(struct mtd_info *mtd, u_char *oob_buf,
				  const u_char *buf, int column, int thislen)
{
	struct onenand_chip *this = mtd->priv;
	struct nand_oobfree *free;
	int writecol = column;
	int writeend = column + thislen;
	int lastgap = 0;
	unsigned int i;

	free = this->ecclayout->oobfree;
	for (i = 0; i < MTD_MAX_OOBFREE_ENTRIES && free->length; i++, free++) {
		if (writecol >= lastgap)
			writecol += free->offset - lastgap;
		if (writeend >= lastgap)
			writeend += free->offset - lastgap;
		lastgap = free->offset + free->length;
	}
	free = this->ecclayout->oobfree;
	for (i = 0; i < MTD_MAX_OOBFREE_ENTRIES && free->length; i++, free++) {
		int free_end = free->offset + free->length;
		if (free->offset < writeend && free_end > writecol) {
			int st = max_t(int,free->offset,writecol);
			int ed = min_t(int,free_end,writeend);
			int n = ed - st;
			memcpy(oob_buf + st, buf, n);
			buf += n;
		} else if (column == 0)
			break;
	}
	return 0;
}

static int onenand_write_ops_nolock(struct mtd_info *mtd, loff_t to,
				struct mtd_oob_ops *ops)
{
	struct onenand_chip *this = mtd->priv;
	int written = 0, column, thislen = 0, subpage = 0;
	int prev = 0, prevlen = 0, prev_subpage = 0, first = 1;
	int oobwritten = 0, oobcolumn, thisooblen, oobsize;
	size_t len = ops->len;
	size_t ooblen = ops->ooblen;
	const u_char *buf = ops->datbuf;
	const u_char *oob = ops->oobbuf;
	u_char *oobbuf;
	int ret = 0, cmd;

	pr_debug("%s: to = 0x%08x, len = %i\n", __func__, (unsigned int)to,
			(int)len);

	
	ops->retlen = 0;
	ops->oobretlen = 0;

	
        if (unlikely(NOTALIGNED(to) || NOTALIGNED(len))) {
		printk(KERN_ERR "%s: Attempt to write not page aligned data\n",
			__func__);
                return -EINVAL;
        }

	
	if (!len)
		return 0;

	if (ops->mode == MTD_OPS_AUTO_OOB)
		oobsize = this->ecclayout->oobavail;
	else
		oobsize = mtd->oobsize;

	oobcolumn = to & (mtd->oobsize - 1);

	column = to & (mtd->writesize - 1);

	
	while (1) {
		if (written < len) {
			u_char *wbuf = (u_char *) buf;

			thislen = min_t(int, mtd->writesize - column, len - written);
			thisooblen = min_t(int, oobsize - oobcolumn, ooblen - oobwritten);

			cond_resched();

			this->command(mtd, ONENAND_CMD_BUFFERRAM, to, thislen);

			
			subpage = thislen < mtd->writesize;
			if (subpage) {
				memset(this->page_buf, 0xff, mtd->writesize);
				memcpy(this->page_buf + column, buf, thislen);
				wbuf = this->page_buf;
			}

			this->write_bufferram(mtd, ONENAND_DATARAM, wbuf, 0, mtd->writesize);

			if (oob) {
				oobbuf = this->oob_buf;

				memset(oobbuf, 0xff, mtd->oobsize);
				if (ops->mode == MTD_OPS_AUTO_OOB)
					onenand_fill_auto_oob(mtd, oobbuf, oob, oobcolumn, thisooblen);
				else
					memcpy(oobbuf + oobcolumn, oob, thisooblen);

				oobwritten += thisooblen;
				oob += thisooblen;
				oobcolumn = 0;
			} else
				oobbuf = (u_char *) ffchars;

			this->write_bufferram(mtd, ONENAND_SPARERAM, oobbuf, 0, mtd->oobsize);
		} else
			ONENAND_SET_NEXT_BUFFERRAM(this);

		if (!ONENAND_IS_2PLANE(this) && !ONENAND_IS_4KB_PAGE(this) && !first) {
			ONENAND_SET_PREV_BUFFERRAM(this);

			ret = this->wait(mtd, FL_WRITING);

			
			onenand_update_bufferram(mtd, prev, !ret && !prev_subpage);
			if (ret) {
				written -= prevlen;
				printk(KERN_ERR "%s: write failed %d\n",
					__func__, ret);
				break;
			}

			if (written == len) {
				
				ret = onenand_verify(mtd, buf - len, to - len, len);
				if (ret)
					printk(KERN_ERR "%s: verify failed %d\n",
						__func__, ret);
				break;
			}

			ONENAND_SET_NEXT_BUFFERRAM(this);
		}

		this->ongoing = 0;
		cmd = ONENAND_CMD_PROG;

		
		if (ONENAND_IS_CACHE_PROGRAM(this) &&
		    likely(onenand_block(this, to) != 0) &&
		    ONENAND_IS_4KB_PAGE(this) &&
		    ((written + thislen) < len)) {
			cmd = ONENAND_CMD_2X_CACHE_PROG;
			this->ongoing = 1;
		}

		this->command(mtd, cmd, to, mtd->writesize);

		if (ONENAND_IS_2PLANE(this) || ONENAND_IS_4KB_PAGE(this)) {
			ret = this->wait(mtd, FL_WRITING);

			
			onenand_update_bufferram(mtd, to, !ret && !subpage);
			if (ret) {
				printk(KERN_ERR "%s: write failed %d\n",
					__func__, ret);
				break;
			}

			
			ret = onenand_verify(mtd, buf, to, thislen);
			if (ret) {
				printk(KERN_ERR "%s: verify failed %d\n",
					__func__, ret);
				break;
			}

			written += thislen;

			if (written == len)
				break;

		} else
			written += thislen;

		column = 0;
		prev_subpage = subpage;
		prev = to;
		prevlen = thislen;
		to += thislen;
		buf += thislen;
		first = 0;
	}

	
	if (written != len)
		onenand_invalidate_bufferram(mtd, 0, -1);

	ops->retlen = written;
	ops->oobretlen = oobwritten;

	return ret;
}


/**
 * onenand_write_oob_nolock - [INTERN] OneNAND write out-of-band
 * @param mtd		MTD device structure
 * @param to		offset to write to
 * @param len		number of bytes to write
 * @param retlen	pointer to variable to store the number of written bytes
 * @param buf		the data to write
 * @param mode		operation mode
 *
 * OneNAND write out-of-band
 */
static int onenand_write_oob_nolock(struct mtd_info *mtd, loff_t to,
				    struct mtd_oob_ops *ops)
{
	struct onenand_chip *this = mtd->priv;
	int column, ret = 0, oobsize;
	int written = 0, oobcmd;
	u_char *oobbuf;
	size_t len = ops->ooblen;
	const u_char *buf = ops->oobbuf;
	unsigned int mode = ops->mode;

	to += ops->ooboffs;

	pr_debug("%s: to = 0x%08x, len = %i\n", __func__, (unsigned int)to,
			(int)len);

	
	ops->oobretlen = 0;

	if (mode == MTD_OPS_AUTO_OOB)
		oobsize = this->ecclayout->oobavail;
	else
		oobsize = mtd->oobsize;

	column = to & (mtd->oobsize - 1);

	if (unlikely(column >= oobsize)) {
		printk(KERN_ERR "%s: Attempted to start write outside oob\n",
			__func__);
		return -EINVAL;
	}

	
	if (unlikely(column + len > oobsize)) {
		printk(KERN_ERR "%s: Attempt to write past end of page\n",
			__func__);
		return -EINVAL;
	}

	
	if (unlikely(to >= mtd->size ||
		     column + len > ((mtd->size >> this->page_shift) -
				     (to >> this->page_shift)) * oobsize)) {
		printk(KERN_ERR "%s: Attempted to write past end of device\n",
		       __func__);
		return -EINVAL;
	}

	oobbuf = this->oob_buf;

	oobcmd = ONENAND_IS_4KB_PAGE(this) ? ONENAND_CMD_PROG : ONENAND_CMD_PROGOOB;

	
	while (written < len) {
		int thislen = min_t(int, oobsize, len - written);

		cond_resched();

		this->command(mtd, ONENAND_CMD_BUFFERRAM, to, mtd->oobsize);

		memset(oobbuf, 0xff, mtd->oobsize);
		if (mode == MTD_OPS_AUTO_OOB)
			onenand_fill_auto_oob(mtd, oobbuf, buf, column, thislen);
		else
			memcpy(oobbuf + column, buf, thislen);
		this->write_bufferram(mtd, ONENAND_SPARERAM, oobbuf, 0, mtd->oobsize);

		if (ONENAND_IS_4KB_PAGE(this)) {
			
			memset(this->page_buf, 0xff, mtd->writesize);
			this->write_bufferram(mtd, ONENAND_DATARAM,
					 this->page_buf, 0, mtd->writesize);
		}

		this->command(mtd, oobcmd, to, mtd->oobsize);

		onenand_update_bufferram(mtd, to, 0);
		if (ONENAND_IS_2PLANE(this)) {
			ONENAND_SET_BUFFERRAM1(this);
			onenand_update_bufferram(mtd, to + this->writesize, 0);
		}

		ret = this->wait(mtd, FL_WRITING);
		if (ret) {
			printk(KERN_ERR "%s: write failed %d\n", __func__, ret);
			break;
		}

		ret = onenand_verify_oob(mtd, oobbuf, to);
		if (ret) {
			printk(KERN_ERR "%s: verify failed %d\n",
				__func__, ret);
			break;
		}

		written += thislen;
		if (written == len)
			break;

		to += mtd->writesize;
		buf += thislen;
		column = 0;
	}

	ops->oobretlen = written;

	return ret;
}

/**
 * onenand_write - [MTD Interface] write buffer to FLASH
 * @param mtd		MTD device structure
 * @param to		offset to write to
 * @param len		number of bytes to write
 * @param retlen	pointer to variable to store the number of written bytes
 * @param buf		the data to write
 *
 * Write with ECC
 */
static int onenand_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	struct mtd_oob_ops ops = {
		.len	= len,
		.ooblen	= 0,
		.datbuf	= (u_char *) buf,
		.oobbuf	= NULL,
	};
	int ret;

	onenand_get_device(mtd, FL_WRITING);
	ret = onenand_write_ops_nolock(mtd, to, &ops);
	onenand_release_device(mtd);

	*retlen = ops.retlen;
	return ret;
}

static int onenand_write_oob(struct mtd_info *mtd, loff_t to,
			     struct mtd_oob_ops *ops)
{
	int ret;

	switch (ops->mode) {
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_AUTO_OOB:
		break;
	case MTD_OPS_RAW:
		
	default:
		return -EINVAL;
	}

	onenand_get_device(mtd, FL_WRITING);
	if (ops->datbuf)
		ret = onenand_write_ops_nolock(mtd, to, ops);
	else
		ret = onenand_write_oob_nolock(mtd, to, ops);
	onenand_release_device(mtd);

	return ret;
}

static int onenand_block_isbad_nolock(struct mtd_info *mtd, loff_t ofs, int allowbbt)
{
	struct onenand_chip *this = mtd->priv;
	struct bbm_info *bbm = this->bbm;

	
	return bbm->isbad_bbt(mtd, ofs, allowbbt);
}


static int onenand_multiblock_erase_verify(struct mtd_info *mtd,
					   struct erase_info *instr)
{
	struct onenand_chip *this = mtd->priv;
	loff_t addr = instr->addr;
	int len = instr->len;
	unsigned int block_size = (1 << this->erase_shift);
	int ret = 0;

	while (len) {
		this->command(mtd, ONENAND_CMD_ERASE_VERIFY, addr, block_size);
		ret = this->wait(mtd, FL_VERIFYING_ERASE);
		if (ret) {
			printk(KERN_ERR "%s: Failed verify, block %d\n",
			       __func__, onenand_block(this, addr));
			instr->state = MTD_ERASE_FAILED;
			instr->fail_addr = addr;
			return -1;
		}
		len -= block_size;
		addr += block_size;
	}
	return 0;
}

static int onenand_multiblock_erase(struct mtd_info *mtd,
				    struct erase_info *instr,
				    unsigned int block_size)
{
	struct onenand_chip *this = mtd->priv;
	loff_t addr = instr->addr;
	int len = instr->len;
	int eb_count = 0;
	int ret = 0;
	int bdry_block = 0;

	instr->state = MTD_ERASING;

	if (ONENAND_IS_DDP(this)) {
		loff_t bdry_addr = this->chipsize >> 1;
		if (addr < bdry_addr && (addr + len) > bdry_addr)
			bdry_block = bdry_addr >> this->erase_shift;
	}

	
	while (len) {
		
		if (onenand_block_isbad_nolock(mtd, addr, 0)) {
			printk(KERN_WARNING "%s: attempt to erase a bad block "
			       "at addr 0x%012llx\n",
			       __func__, (unsigned long long) addr);
			instr->state = MTD_ERASE_FAILED;
			return -EIO;
		}
		len -= block_size;
		addr += block_size;
	}

	len = instr->len;
	addr = instr->addr;

	
	while (len) {
		struct erase_info verify_instr = *instr;
		int max_eb_count = MB_ERASE_MAX_BLK_COUNT;

		verify_instr.addr = addr;
		verify_instr.len = 0;

		
		if (bdry_block) {
			int this_block = (addr >> this->erase_shift);

			if (this_block < bdry_block) {
				max_eb_count = min(max_eb_count,
						   (bdry_block - this_block));
			}
		}

		eb_count = 0;

		while (len > block_size && eb_count < (max_eb_count - 1)) {
			this->command(mtd, ONENAND_CMD_MULTIBLOCK_ERASE,
				      addr, block_size);
			onenand_invalidate_bufferram(mtd, addr, block_size);

			ret = this->wait(mtd, FL_PREPARING_ERASE);
			if (ret) {
				printk(KERN_ERR "%s: Failed multiblock erase, "
				       "block %d\n", __func__,
				       onenand_block(this, addr));
				instr->state = MTD_ERASE_FAILED;
				instr->fail_addr = MTD_FAIL_ADDR_UNKNOWN;
				return -EIO;
			}

			len -= block_size;
			addr += block_size;
			eb_count++;
		}

		
		cond_resched();
		this->command(mtd, ONENAND_CMD_ERASE, addr, block_size);
		onenand_invalidate_bufferram(mtd, addr, block_size);

		ret = this->wait(mtd, FL_ERASING);
		
		if (ret) {
			printk(KERN_ERR "%s: Failed erase, block %d\n",
			       __func__, onenand_block(this, addr));
			instr->state = MTD_ERASE_FAILED;
			instr->fail_addr = MTD_FAIL_ADDR_UNKNOWN;
			return -EIO;
		}

		len -= block_size;
		addr += block_size;
		eb_count++;

		
		verify_instr.len = eb_count * block_size;
		if (onenand_multiblock_erase_verify(mtd, &verify_instr)) {
			instr->state = verify_instr.state;
			instr->fail_addr = verify_instr.fail_addr;
			return -EIO;
		}

	}
	return 0;
}


static int onenand_block_by_block_erase(struct mtd_info *mtd,
					struct erase_info *instr,
					struct mtd_erase_region_info *region,
					unsigned int block_size)
{
	struct onenand_chip *this = mtd->priv;
	loff_t addr = instr->addr;
	int len = instr->len;
	loff_t region_end = 0;
	int ret = 0;

	if (region) {
		
		region_end = region->offset + region->erasesize * region->numblocks;
	}

	instr->state = MTD_ERASING;

	
	while (len) {
		cond_resched();

		
		if (onenand_block_isbad_nolock(mtd, addr, 0)) {
			printk(KERN_WARNING "%s: attempt to erase a bad block "
					"at addr 0x%012llx\n",
					__func__, (unsigned long long) addr);
			instr->state = MTD_ERASE_FAILED;
			return -EIO;
		}

		this->command(mtd, ONENAND_CMD_ERASE, addr, block_size);

		onenand_invalidate_bufferram(mtd, addr, block_size);

		ret = this->wait(mtd, FL_ERASING);
		
		if (ret) {
			printk(KERN_ERR "%s: Failed erase, block %d\n",
				__func__, onenand_block(this, addr));
			instr->state = MTD_ERASE_FAILED;
			instr->fail_addr = addr;
			return -EIO;
		}

		len -= block_size;
		addr += block_size;

		if (region && addr == region_end) {
			if (!len)
				break;
			region++;

			block_size = region->erasesize;
			region_end = region->offset + region->erasesize * region->numblocks;

			if (len & (block_size - 1)) {
				
				printk(KERN_ERR "%s: Unaligned address\n",
					__func__);
				return -EIO;
			}
		}
	}
	return 0;
}

static int onenand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct onenand_chip *this = mtd->priv;
	unsigned int block_size;
	loff_t addr = instr->addr;
	loff_t len = instr->len;
	int ret = 0;
	struct mtd_erase_region_info *region = NULL;
	loff_t region_offset = 0;

	pr_debug("%s: start=0x%012llx, len=%llu\n", __func__,
			(unsigned long long)instr->addr,
			(unsigned long long)instr->len);

	if (FLEXONENAND(this)) {
		
		int i = flexonenand_region(mtd, addr);

		region = &mtd->eraseregions[i];
		block_size = region->erasesize;

		region_offset = region->offset;
	} else
		block_size = 1 << this->erase_shift;

	
	if (unlikely((addr - region_offset) & (block_size - 1))) {
		printk(KERN_ERR "%s: Unaligned address\n", __func__);
		return -EINVAL;
	}

	
	if (unlikely(len & (block_size - 1))) {
		printk(KERN_ERR "%s: Length not block aligned\n", __func__);
		return -EINVAL;
	}

	
	onenand_get_device(mtd, FL_ERASING);

	if (ONENAND_IS_4KB_PAGE(this) || region ||
	    instr->len < MB_ERASE_MIN_BLK_COUNT * block_size) {
		
		ret = onenand_block_by_block_erase(mtd, instr,
						   region, block_size);
	} else {
		ret = onenand_multiblock_erase(mtd, instr, block_size);
	}

	
	onenand_release_device(mtd);

	
	if (!ret) {
		instr->state = MTD_ERASE_DONE;
		mtd_erase_callback(instr);
	}

	return ret;
}

static void onenand_sync(struct mtd_info *mtd)
{
	pr_debug("%s: called\n", __func__);

	
	onenand_get_device(mtd, FL_SYNCING);

	
	onenand_release_device(mtd);
}

static int onenand_block_isbad(struct mtd_info *mtd, loff_t ofs)
{
	int ret;

	
	if (ofs > mtd->size)
		return -EINVAL;

	onenand_get_device(mtd, FL_READING);
	ret = onenand_block_isbad_nolock(mtd, ofs, 0);
	onenand_release_device(mtd);
	return ret;
}

static int onenand_default_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct onenand_chip *this = mtd->priv;
	struct bbm_info *bbm = this->bbm;
	u_char buf[2] = {0, 0};
	struct mtd_oob_ops ops = {
		.mode = MTD_OPS_PLACE_OOB,
		.ooblen = 2,
		.oobbuf = buf,
		.ooboffs = 0,
	};
	int block;

	
	block = onenand_block(this, ofs);
        if (bbm->bbt)
                bbm->bbt[block >> 2] |= 0x01 << ((block & 0x03) << 1);

        
        ofs += mtd->oobsize + (bbm->badblockpos & ~0x01);
	return onenand_write_oob_nolock(mtd, ofs, &ops);
}

static int onenand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	int ret;

	ret = onenand_block_isbad(mtd, ofs);
	if (ret) {
		
		if (ret > 0)
			return 0;
		return ret;
	}

	onenand_get_device(mtd, FL_WRITING);
	ret = mtd_block_markbad(mtd, ofs);
	onenand_release_device(mtd);
	return ret;
}

static int onenand_do_lock_cmd(struct mtd_info *mtd, loff_t ofs, size_t len, int cmd)
{
	struct onenand_chip *this = mtd->priv;
	int start, end, block, value, status;
	int wp_status_mask;

	start = onenand_block(this, ofs);
	end = onenand_block(this, ofs + len) - 1;

	if (cmd == ONENAND_CMD_LOCK)
		wp_status_mask = ONENAND_WP_LS;
	else
		wp_status_mask = ONENAND_WP_US;

	
	if (this->options & ONENAND_HAS_CONT_LOCK) {
		
		this->write_word(start, this->base + ONENAND_REG_START_BLOCK_ADDRESS);
		
		this->write_word(end, this->base +  ONENAND_REG_END_BLOCK_ADDRESS);
		
		this->command(mtd, cmd, 0, 0);

		
		this->wait(mtd, FL_LOCKING);

		
		while (this->read_word(this->base + ONENAND_REG_CTRL_STATUS)
		    & ONENAND_CTRL_ONGO)
			continue;

		
		status = this->read_word(this->base + ONENAND_REG_WP_STATUS);
		if (!(status & wp_status_mask))
			printk(KERN_ERR "%s: wp status = 0x%x\n",
				__func__, status);

		return 0;
	}

	
	for (block = start; block < end + 1; block++) {
		
		value = onenand_block_address(this, block);
		this->write_word(value, this->base + ONENAND_REG_START_ADDRESS1);
		
		value = onenand_bufferram_address(this, block);
		this->write_word(value, this->base + ONENAND_REG_START_ADDRESS2);
		
		this->write_word(block, this->base + ONENAND_REG_START_BLOCK_ADDRESS);
		
		this->command(mtd, cmd, 0, 0);

		
		this->wait(mtd, FL_LOCKING);

		
		while (this->read_word(this->base + ONENAND_REG_CTRL_STATUS)
		    & ONENAND_CTRL_ONGO)
			continue;

		
		status = this->read_word(this->base + ONENAND_REG_WP_STATUS);
		if (!(status & wp_status_mask))
			printk(KERN_ERR "%s: block = %d, wp status = 0x%x\n",
				__func__, block, status);
	}

	return 0;
}

static int onenand_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	int ret;

	onenand_get_device(mtd, FL_LOCKING);
	ret = onenand_do_lock_cmd(mtd, ofs, len, ONENAND_CMD_LOCK);
	onenand_release_device(mtd);
	return ret;
}

static int onenand_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	int ret;

	onenand_get_device(mtd, FL_LOCKING);
	ret = onenand_do_lock_cmd(mtd, ofs, len, ONENAND_CMD_UNLOCK);
	onenand_release_device(mtd);
	return ret;
}

static int onenand_check_lock_status(struct onenand_chip *this)
{
	unsigned int value, block, status;
	unsigned int end;

	end = this->chipsize >> this->erase_shift;
	for (block = 0; block < end; block++) {
		
		value = onenand_block_address(this, block);
		this->write_word(value, this->base + ONENAND_REG_START_ADDRESS1);
		
		value = onenand_bufferram_address(this, block);
		this->write_word(value, this->base + ONENAND_REG_START_ADDRESS2);
		
		this->write_word(block, this->base + ONENAND_REG_START_BLOCK_ADDRESS);

		
		status = this->read_word(this->base + ONENAND_REG_WP_STATUS);
		if (!(status & ONENAND_WP_US)) {
			printk(KERN_ERR "%s: block = %d, wp status = 0x%x\n",
				__func__, block, status);
			return 0;
		}
	}

	return 1;
}

static void onenand_unlock_all(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;
	loff_t ofs = 0;
	loff_t len = mtd->size;

	if (this->options & ONENAND_HAS_UNLOCK_ALL) {
		
		this->write_word(0, this->base + ONENAND_REG_START_BLOCK_ADDRESS);
		
		this->command(mtd, ONENAND_CMD_UNLOCK_ALL, 0, 0);

		
		this->wait(mtd, FL_LOCKING);

		
		while (this->read_word(this->base + ONENAND_REG_CTRL_STATUS)
		    & ONENAND_CTRL_ONGO)
			continue;

		
		if (this->options & ONENAND_SKIP_UNLOCK_CHECK)
			return;

		
		if (onenand_check_lock_status(this))
			return;

		
		if (ONENAND_IS_DDP(this) && !FLEXONENAND(this)) {
			
			ofs = this->chipsize >> 1;
			len = this->chipsize >> 1;
		}
	}

	onenand_do_lock_cmd(mtd, ofs, len, ONENAND_CMD_UNLOCK);
}

#ifdef CONFIG_MTD_ONENAND_OTP

static int onenand_otp_command(struct mtd_info *mtd, int cmd, loff_t addr,
				size_t len)
{
	struct onenand_chip *this = mtd->priv;
	int value, block, page;

	
	switch (cmd) {
	case ONENAND_CMD_OTP_ACCESS:
		block = (int) (addr >> this->erase_shift);
		page = -1;
		break;

	default:
		block = (int) (addr >> this->erase_shift);
		page = (int) (addr >> this->page_shift);

		if (ONENAND_IS_2PLANE(this)) {
			
			block &= ~1;
			
			if (addr & this->writesize)
				block++;
			page >>= 1;
		}
		page &= this->page_mask;
		break;
	}

	if (block != -1) {
		
		value = onenand_block_address(this, block);
		this->write_word(value, this->base +
				ONENAND_REG_START_ADDRESS1);
	}

	if (page != -1) {
		
		int sectors = 4, count = 4;
		int dataram;

		switch (cmd) {
		default:
			if (ONENAND_IS_2PLANE(this) && cmd == ONENAND_CMD_PROG)
				cmd = ONENAND_CMD_2X_PROG;
			dataram = ONENAND_CURRENT_BUFFERRAM(this);
			break;
		}

		
		value = onenand_page_address(page, sectors);
		this->write_word(value, this->base +
				ONENAND_REG_START_ADDRESS8);

		
		value = onenand_buffer_address(dataram, sectors, count);
		this->write_word(value, this->base + ONENAND_REG_START_BUFFER);
	}

	
	this->write_word(ONENAND_INT_CLEAR, this->base + ONENAND_REG_INTERRUPT);

	
	this->write_word(cmd, this->base + ONENAND_REG_COMMAND);

	return 0;
}

/**
 * onenand_otp_write_oob_nolock - [INTERN] OneNAND write out-of-band, specific to OTP
 * @param mtd		MTD device structure
 * @param to		offset to write to
 * @param len		number of bytes to write
 * @param retlen	pointer to variable to store the number of written bytes
 * @param buf		the data to write
 *
 * OneNAND write out-of-band only for OTP
 */
static int onenand_otp_write_oob_nolock(struct mtd_info *mtd, loff_t to,
				    struct mtd_oob_ops *ops)
{
	struct onenand_chip *this = mtd->priv;
	int column, ret = 0, oobsize;
	int written = 0;
	u_char *oobbuf;
	size_t len = ops->ooblen;
	const u_char *buf = ops->oobbuf;
	int block, value, status;

	to += ops->ooboffs;

	
	ops->oobretlen = 0;

	oobsize = mtd->oobsize;

	column = to & (mtd->oobsize - 1);

	oobbuf = this->oob_buf;

	
	while (written < len) {
		int thislen = min_t(int, oobsize, len - written);

		cond_resched();

		block = (int) (to >> this->erase_shift);

		value = onenand_block_address(this, block);
		this->write_word(value, this->base +
				ONENAND_REG_START_ADDRESS1);


		value = onenand_bufferram_address(this, block);
		this->write_word(value, this->base +
				ONENAND_REG_START_ADDRESS2);
		ONENAND_SET_NEXT_BUFFERRAM(this);

		this->command(mtd, ONENAND_CMD_OTP_ACCESS, 0, 0);
		this->wait(mtd, FL_OTPING);

		memcpy(oobbuf + column, buf, thislen);

		this->write_bufferram(mtd, ONENAND_SPARERAM,
					oobbuf, 0, mtd->oobsize);

		onenand_otp_command(mtd, ONENAND_CMD_PROGOOB, to, mtd->oobsize);
		onenand_update_bufferram(mtd, to, 0);
		if (ONENAND_IS_2PLANE(this)) {
			ONENAND_SET_BUFFERRAM1(this);
			onenand_update_bufferram(mtd, to + this->writesize, 0);
		}

		ret = this->wait(mtd, FL_WRITING);
		if (ret) {
			printk(KERN_ERR "%s: write failed %d\n", __func__, ret);
			break;
		}

		
		this->command(mtd, ONENAND_CMD_RESET, 0, 0);
		this->wait(mtd, FL_RESETING);

		status = this->read_word(this->base + ONENAND_REG_CTRL_STATUS);
		status &= 0x60;

		if (status == 0x60) {
			printk(KERN_DEBUG "\nBLOCK\tSTATUS\n");
			printk(KERN_DEBUG "1st Block\tLOCKED\n");
			printk(KERN_DEBUG "OTP Block\tLOCKED\n");
		} else if (status == 0x20) {
			printk(KERN_DEBUG "\nBLOCK\tSTATUS\n");
			printk(KERN_DEBUG "1st Block\tLOCKED\n");
			printk(KERN_DEBUG "OTP Block\tUN-LOCKED\n");
		} else if (status == 0x40) {
			printk(KERN_DEBUG "\nBLOCK\tSTATUS\n");
			printk(KERN_DEBUG "1st Block\tUN-LOCKED\n");
			printk(KERN_DEBUG "OTP Block\tLOCKED\n");
		} else {
			printk(KERN_DEBUG "Reboot to check\n");
		}

		written += thislen;
		if (written == len)
			break;

		to += mtd->writesize;
		buf += thislen;
		column = 0;
	}

	ops->oobretlen = written;

	return ret;
}

typedef int (*otp_op_t)(struct mtd_info *mtd, loff_t form, size_t len,
		size_t *retlen, u_char *buf);

static int do_otp_read(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char *buf)
{
	struct onenand_chip *this = mtd->priv;
	struct mtd_oob_ops ops = {
		.len	= len,
		.ooblen	= 0,
		.datbuf	= buf,
		.oobbuf	= NULL,
	};
	int ret;

	
	this->command(mtd, ONENAND_CMD_OTP_ACCESS, 0, 0);
	this->wait(mtd, FL_OTPING);

	ret = ONENAND_IS_4KB_PAGE(this) ?
		onenand_mlc_read_ops_nolock(mtd, from, &ops) :
		onenand_read_ops_nolock(mtd, from, &ops);

	
	this->command(mtd, ONENAND_CMD_RESET, 0, 0);
	this->wait(mtd, FL_RESETING);

	return ret;
}

static int do_otp_write(struct mtd_info *mtd, loff_t to, size_t len,
		size_t *retlen, u_char *buf)
{
	struct onenand_chip *this = mtd->priv;
	unsigned char *pbuf = buf;
	int ret;
	struct mtd_oob_ops ops;

	
	if (len < mtd->writesize) {
		memcpy(this->page_buf, buf, len);
		memset(this->page_buf + len, 0xff, mtd->writesize - len);
		pbuf = this->page_buf;
		len = mtd->writesize;
	}

	
	this->command(mtd, ONENAND_CMD_OTP_ACCESS, 0, 0);
	this->wait(mtd, FL_OTPING);

	ops.len = len;
	ops.ooblen = 0;
	ops.datbuf = pbuf;
	ops.oobbuf = NULL;
	ret = onenand_write_ops_nolock(mtd, to, &ops);
	*retlen = ops.retlen;

	
	this->command(mtd, ONENAND_CMD_RESET, 0, 0);
	this->wait(mtd, FL_RESETING);

	return ret;
}

static int do_otp_lock(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char *buf)
{
	struct onenand_chip *this = mtd->priv;
	struct mtd_oob_ops ops;
	int ret;

	if (FLEXONENAND(this)) {

		
		this->command(mtd, ONENAND_CMD_OTP_ACCESS, 0, 0);
		this->wait(mtd, FL_OTPING);
		ops.len = mtd->writesize;
		ops.ooblen = 0;
		ops.datbuf = buf;
		ops.oobbuf = NULL;
		ret = onenand_write_ops_nolock(mtd, mtd->writesize * 49, &ops);
		*retlen = ops.retlen;

		
		this->command(mtd, ONENAND_CMD_RESET, 0, 0);
		this->wait(mtd, FL_RESETING);
	} else {
		ops.mode = MTD_OPS_PLACE_OOB;
		ops.ooblen = len;
		ops.oobbuf = buf;
		ops.ooboffs = 0;
		ret = onenand_otp_write_oob_nolock(mtd, from, &ops);
		*retlen = ops.oobretlen;
	}

	return ret;
}

static int onenand_otp_walk(struct mtd_info *mtd, loff_t from, size_t len,
			size_t *retlen, u_char *buf,
			otp_op_t action, int mode)
{
	struct onenand_chip *this = mtd->priv;
	int otp_pages;
	int density;
	int ret = 0;

	*retlen = 0;

	density = onenand_get_density(this->device_id);
	if (density < ONENAND_DEVICE_DENSITY_512Mb)
		otp_pages = 20;
	else
		otp_pages = 50;

	if (mode == MTD_OTP_FACTORY) {
		from += mtd->writesize * otp_pages;
		otp_pages = ONENAND_PAGES_PER_BLOCK - otp_pages;
	}

	
	if (mode == MTD_OTP_USER) {
		if (mtd->writesize * otp_pages < from + len)
			return 0;
	} else {
		if (mtd->writesize * otp_pages <  len)
			return 0;
	}

	onenand_get_device(mtd, FL_OTPING);
	while (len > 0 && otp_pages > 0) {
		if (!action) {	
			struct otp_info *otpinfo;

			len -= sizeof(struct otp_info);
			if (len <= 0) {
				ret = -ENOSPC;
				break;
			}

			otpinfo = (struct otp_info *) buf;
			otpinfo->start = from;
			otpinfo->length = mtd->writesize;
			otpinfo->locked = 0;

			from += mtd->writesize;
			buf += sizeof(struct otp_info);
			*retlen += sizeof(struct otp_info);
		} else {
			size_t tmp_retlen;

			ret = action(mtd, from, len, &tmp_retlen, buf);

			buf += tmp_retlen;
			len -= tmp_retlen;
			*retlen += tmp_retlen;

			if (ret)
				break;
		}
		otp_pages--;
	}
	onenand_release_device(mtd);

	return ret;
}

static int onenand_get_fact_prot_info(struct mtd_info *mtd,
			struct otp_info *buf, size_t len)
{
	size_t retlen;
	int ret;

	ret = onenand_otp_walk(mtd, 0, len, &retlen, (u_char *) buf, NULL, MTD_OTP_FACTORY);

	return ret ? : retlen;
}

static int onenand_read_fact_prot_reg(struct mtd_info *mtd, loff_t from,
			size_t len, size_t *retlen, u_char *buf)
{
	return onenand_otp_walk(mtd, from, len, retlen, buf, do_otp_read, MTD_OTP_FACTORY);
}

static int onenand_get_user_prot_info(struct mtd_info *mtd,
			struct otp_info *buf, size_t len)
{
	size_t retlen;
	int ret;

	ret = onenand_otp_walk(mtd, 0, len, &retlen, (u_char *) buf, NULL, MTD_OTP_USER);

	return ret ? : retlen;
}

static int onenand_read_user_prot_reg(struct mtd_info *mtd, loff_t from,
			size_t len, size_t *retlen, u_char *buf)
{
	return onenand_otp_walk(mtd, from, len, retlen, buf, do_otp_read, MTD_OTP_USER);
}

static int onenand_write_user_prot_reg(struct mtd_info *mtd, loff_t from,
			size_t len, size_t *retlen, u_char *buf)
{
	return onenand_otp_walk(mtd, from, len, retlen, buf, do_otp_write, MTD_OTP_USER);
}

static int onenand_lock_user_prot_reg(struct mtd_info *mtd, loff_t from,
			size_t len)
{
	struct onenand_chip *this = mtd->priv;
	u_char *buf = FLEXONENAND(this) ? this->page_buf : this->oob_buf;
	size_t retlen;
	int ret;
	unsigned int otp_lock_offset = ONENAND_OTP_LOCK_OFFSET;

	memset(buf, 0xff, FLEXONENAND(this) ? this->writesize
						 : mtd->oobsize);

	from = 0;
	len = FLEXONENAND(this) ? mtd->writesize : 16;

	if (FLEXONENAND(this))
		otp_lock_offset = FLEXONENAND_OTP_LOCK_OFFSET;

	
	if (otp == 1)
		buf[otp_lock_offset] = 0xFC;
	else if (otp == 2)
		buf[otp_lock_offset] = 0xF3;
	else if (otp == 3)
		buf[otp_lock_offset] = 0xF0;
	else if (otp != 0)
		printk(KERN_DEBUG "[OneNAND] Invalid option selected for OTP\n");

	ret = onenand_otp_walk(mtd, from, len, &retlen, buf, do_otp_lock, MTD_OTP_USER);

	return ret ? : retlen;
}

#endif	

static void onenand_check_features(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;
	unsigned int density, process, numbufs;

	
	density = onenand_get_density(this->device_id);
	process = this->version_id >> ONENAND_VERSION_PROCESS_SHIFT;
	numbufs = this->read_word(this->base + ONENAND_REG_NUM_BUFFERS) >> 8;

	
	switch (density) {
	case ONENAND_DEVICE_DENSITY_4Gb:
		if (ONENAND_IS_DDP(this))
			this->options |= ONENAND_HAS_2PLANE;
		else if (numbufs == 1) {
			this->options |= ONENAND_HAS_4KB_PAGE;
			this->options |= ONENAND_HAS_CACHE_PROGRAM;
			if ((this->version_id & 0xf) == 0xe)
				this->options |= ONENAND_HAS_NOP_1;
		}

	case ONENAND_DEVICE_DENSITY_2Gb:
		
		if (!ONENAND_IS_DDP(this))
			this->options |= ONENAND_HAS_2PLANE;
		this->options |= ONENAND_HAS_UNLOCK_ALL;

	case ONENAND_DEVICE_DENSITY_1Gb:
		
		if (process)
			this->options |= ONENAND_HAS_UNLOCK_ALL;
		break;

	default:
		
		if (!process)
			this->options |= ONENAND_HAS_CONT_LOCK;
		break;
	}

	
	if (ONENAND_IS_MLC(this))
		this->options |= ONENAND_HAS_4KB_PAGE;

	if (ONENAND_IS_4KB_PAGE(this))
		this->options &= ~ONENAND_HAS_2PLANE;

	if (FLEXONENAND(this)) {
		this->options &= ~ONENAND_HAS_CONT_LOCK;
		this->options |= ONENAND_HAS_UNLOCK_ALL;
	}

	if (this->options & ONENAND_HAS_CONT_LOCK)
		printk(KERN_DEBUG "Lock scheme is Continuous Lock\n");
	if (this->options & ONENAND_HAS_UNLOCK_ALL)
		printk(KERN_DEBUG "Chip support all block unlock\n");
	if (this->options & ONENAND_HAS_2PLANE)
		printk(KERN_DEBUG "Chip has 2 plane\n");
	if (this->options & ONENAND_HAS_4KB_PAGE)
		printk(KERN_DEBUG "Chip has 4KiB pagesize\n");
	if (this->options & ONENAND_HAS_CACHE_PROGRAM)
		printk(KERN_DEBUG "Chip has cache program feature\n");
}

static void onenand_print_device_info(int device, int version)
{
	int vcc, demuxed, ddp, density, flexonenand;

        vcc = device & ONENAND_DEVICE_VCC_MASK;
        demuxed = device & ONENAND_DEVICE_IS_DEMUX;
        ddp = device & ONENAND_DEVICE_IS_DDP;
        density = onenand_get_density(device);
	flexonenand = device & DEVICE_IS_FLEXONENAND;
	printk(KERN_INFO "%s%sOneNAND%s %dMB %sV 16-bit (0x%02x)\n",
		demuxed ? "" : "Muxed ",
		flexonenand ? "Flex-" : "",
                ddp ? "(DDP)" : "",
                (16 << density),
                vcc ? "2.65/3.3" : "1.8",
                device);
	printk(KERN_INFO "OneNAND version = 0x%04x\n", version);
}

static const struct onenand_manufacturers onenand_manuf_ids[] = {
        {ONENAND_MFR_SAMSUNG, "Samsung"},
	{ONENAND_MFR_NUMONYX, "Numonyx"},
};

static int onenand_check_maf(int manuf)
{
	int size = ARRAY_SIZE(onenand_manuf_ids);
	char *name;
        int i;

	for (i = 0; i < size; i++)
                if (manuf == onenand_manuf_ids[i].id)
                        break;

	if (i < size)
		name = onenand_manuf_ids[i].name;
	else
		name = "Unknown";

	printk(KERN_DEBUG "OneNAND Manufacturer: %s (0x%0x)\n", name, manuf);

	return (i == size);
}

static int flexonenand_get_boundary(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;
	unsigned die, bdry;
	int ret, syscfg, locked;

	
	syscfg = this->read_word(this->base + ONENAND_REG_SYS_CFG1);
	this->write_word((syscfg | 0x0100), this->base + ONENAND_REG_SYS_CFG1);

	for (die = 0; die < this->dies; die++) {
		this->command(mtd, FLEXONENAND_CMD_PI_ACCESS, die, 0);
		this->wait(mtd, FL_SYNCING);

		this->command(mtd, FLEXONENAND_CMD_READ_PI, die, 0);
		ret = this->wait(mtd, FL_READING);

		bdry = this->read_word(this->base + ONENAND_DATARAM);
		if ((bdry >> FLEXONENAND_PI_UNLOCK_SHIFT) == 3)
			locked = 0;
		else
			locked = 1;
		this->boundary[die] = bdry & FLEXONENAND_PI_MASK;

		this->command(mtd, ONENAND_CMD_RESET, 0, 0);
		ret = this->wait(mtd, FL_RESETING);

		printk(KERN_INFO "Die %d boundary: %d%s\n", die,
		       this->boundary[die], locked ? "(Locked)" : "(Unlocked)");
	}

	
	this->write_word(syscfg, this->base + ONENAND_REG_SYS_CFG1);
	return 0;
}

static void flexonenand_get_size(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;
	int die, i, eraseshift, density;
	int blksperdie, maxbdry;
	loff_t ofs;

	density = onenand_get_density(this->device_id);
	blksperdie = ((loff_t)(16 << density) << 20) >> (this->erase_shift);
	blksperdie >>= ONENAND_IS_DDP(this) ? 1 : 0;
	maxbdry = blksperdie - 1;
	eraseshift = this->erase_shift - 1;

	mtd->numeraseregions = this->dies << 1;

	
	flexonenand_get_boundary(mtd);
	die = ofs = 0;
	i = -1;
	for (; die < this->dies; die++) {
		if (!die || this->boundary[die-1] != maxbdry) {
			i++;
			mtd->eraseregions[i].offset = ofs;
			mtd->eraseregions[i].erasesize = 1 << eraseshift;
			mtd->eraseregions[i].numblocks =
							this->boundary[die] + 1;
			ofs += mtd->eraseregions[i].numblocks << eraseshift;
			eraseshift++;
		} else {
			mtd->numeraseregions -= 1;
			mtd->eraseregions[i].numblocks +=
							this->boundary[die] + 1;
			ofs += (this->boundary[die] + 1) << (eraseshift - 1);
		}
		if (this->boundary[die] != maxbdry) {
			i++;
			mtd->eraseregions[i].offset = ofs;
			mtd->eraseregions[i].erasesize = 1 << eraseshift;
			mtd->eraseregions[i].numblocks = maxbdry ^
							 this->boundary[die];
			ofs += mtd->eraseregions[i].numblocks << eraseshift;
			eraseshift--;
		} else
			mtd->numeraseregions -= 1;
	}

	
	mtd->erasesize = 1 << this->erase_shift;
	if (mtd->numeraseregions == 1)
		mtd->erasesize >>= 1;

	printk(KERN_INFO "Device has %d eraseregions\n", mtd->numeraseregions);
	for (i = 0; i < mtd->numeraseregions; i++)
		printk(KERN_INFO "[offset: 0x%08x, erasesize: 0x%05x,"
			" numblocks: %04u]\n",
			(unsigned int) mtd->eraseregions[i].offset,
			mtd->eraseregions[i].erasesize,
			mtd->eraseregions[i].numblocks);

	for (die = 0, mtd->size = 0; die < this->dies; die++) {
		this->diesize[die] = (loff_t)blksperdie << this->erase_shift;
		this->diesize[die] -= (loff_t)(this->boundary[die] + 1)
						 << (this->erase_shift - 1);
		mtd->size += this->diesize[die];
	}
}

static int flexonenand_check_blocks_erased(struct mtd_info *mtd, int start, int end)
{
	struct onenand_chip *this = mtd->priv;
	int i, ret;
	int block;
	struct mtd_oob_ops ops = {
		.mode = MTD_OPS_PLACE_OOB,
		.ooboffs = 0,
		.ooblen	= mtd->oobsize,
		.datbuf	= NULL,
		.oobbuf	= this->oob_buf,
	};
	loff_t addr;

	printk(KERN_DEBUG "Check blocks from %d to %d\n", start, end);

	for (block = start; block <= end; block++) {
		addr = flexonenand_addr(this, block);
		if (onenand_block_isbad_nolock(mtd, addr, 0))
			continue;

		ret = onenand_read_oob_nolock(mtd, addr, &ops);
		if (ret)
			return ret;

		for (i = 0; i < mtd->oobsize; i++)
			if (this->oob_buf[i] != 0xff)
				break;

		if (i != mtd->oobsize) {
			printk(KERN_WARNING "%s: Block %d not erased.\n",
				__func__, block);
			return 1;
		}
	}

	return 0;
}

int flexonenand_set_boundary(struct mtd_info *mtd, int die,
				    int boundary, int lock)
{
	struct onenand_chip *this = mtd->priv;
	int ret, density, blksperdie, old, new, thisboundary;
	loff_t addr;

	
	if (die && (!ONENAND_IS_DDP(this)))
		return 0;

	
	if (boundary < 0 || boundary == this->boundary[die])
		return 0;

	density = onenand_get_density(this->device_id);
	blksperdie = ((16 << density) << 20) >> this->erase_shift;
	blksperdie >>= ONENAND_IS_DDP(this) ? 1 : 0;

	if (boundary >= blksperdie) {
		printk(KERN_ERR "%s: Invalid boundary value. "
				"Boundary not changed.\n", __func__);
		return -EINVAL;
	}

	
	old = this->boundary[die] + (die * this->density_mask);
	new = boundary + (die * this->density_mask);
	ret = flexonenand_check_blocks_erased(mtd, min(old, new) + 1, max(old, new));
	if (ret) {
		printk(KERN_ERR "%s: Please erase blocks "
				"before boundary change\n", __func__);
		return ret;
	}

	this->command(mtd, FLEXONENAND_CMD_PI_ACCESS, die, 0);
	this->wait(mtd, FL_SYNCING);

	
	this->command(mtd, FLEXONENAND_CMD_READ_PI, die, 0);
	ret = this->wait(mtd, FL_READING);

	thisboundary = this->read_word(this->base + ONENAND_DATARAM);
	if ((thisboundary >> FLEXONENAND_PI_UNLOCK_SHIFT) != 3) {
		printk(KERN_ERR "%s: boundary locked\n", __func__);
		ret = 1;
		goto out;
	}

	printk(KERN_INFO "Changing die %d boundary: %d%s\n",
			die, boundary, lock ? "(Locked)" : "(Unlocked)");

	addr = die ? this->diesize[0] : 0;

	boundary &= FLEXONENAND_PI_MASK;
	boundary |= lock ? 0 : (3 << FLEXONENAND_PI_UNLOCK_SHIFT);

	this->command(mtd, ONENAND_CMD_ERASE, addr, 0);
	ret = this->wait(mtd, FL_ERASING);
	if (ret) {
		printk(KERN_ERR "%s: Failed PI erase for Die %d\n",
		       __func__, die);
		goto out;
	}

	this->write_word(boundary, this->base + ONENAND_DATARAM);
	this->command(mtd, ONENAND_CMD_PROG, addr, 0);
	ret = this->wait(mtd, FL_WRITING);
	if (ret) {
		printk(KERN_ERR "%s: Failed PI write for Die %d\n",
			__func__, die);
		goto out;
	}

	this->command(mtd, FLEXONENAND_CMD_PI_UPDATE, die, 0);
	ret = this->wait(mtd, FL_WRITING);
out:
	this->write_word(ONENAND_CMD_RESET, this->base + ONENAND_REG_COMMAND);
	this->wait(mtd, FL_RESETING);
	if (!ret)
		
		flexonenand_get_size(mtd);

	return ret;
}

static int onenand_chip_probe(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;
	int bram_maf_id, bram_dev_id, maf_id, dev_id;
	int syscfg;

	
	syscfg = this->read_word(this->base + ONENAND_REG_SYS_CFG1);
	
	this->write_word((syscfg & ~ONENAND_SYS_CFG1_SYNC_READ & ~ONENAND_SYS_CFG1_SYNC_WRITE), this->base + ONENAND_REG_SYS_CFG1);

	
	this->write_word(ONENAND_CMD_READID, this->base + ONENAND_BOOTRAM);

	
	bram_maf_id = this->read_word(this->base + ONENAND_BOOTRAM + 0x0);
	bram_dev_id = this->read_word(this->base + ONENAND_BOOTRAM + 0x2);

	
	this->write_word(ONENAND_CMD_RESET, this->base + ONENAND_BOOTRAM);
	
	this->wait(mtd, FL_RESETING);

	
	this->write_word(syscfg, this->base + ONENAND_REG_SYS_CFG1);

	
	if (onenand_check_maf(bram_maf_id))
		return -ENXIO;

	
	maf_id = this->read_word(this->base + ONENAND_REG_MANUFACTURER_ID);
	dev_id = this->read_word(this->base + ONENAND_REG_DEVICE_ID);

	
	if (maf_id != bram_maf_id || dev_id != bram_dev_id)
		return -ENXIO;

	return 0;
}

static int onenand_probe(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;
	int maf_id, dev_id, ver_id;
	int density;
	int ret;

	ret = this->chip_probe(mtd);
	if (ret)
		return ret;

	
	maf_id = this->read_word(this->base + ONENAND_REG_MANUFACTURER_ID);
	dev_id = this->read_word(this->base + ONENAND_REG_DEVICE_ID);
	ver_id = this->read_word(this->base + ONENAND_REG_VERSION_ID);
	this->technology = this->read_word(this->base + ONENAND_REG_TECHNOLOGY);

	
	onenand_print_device_info(dev_id, ver_id);
	this->device_id = dev_id;
	this->version_id = ver_id;

	
	onenand_check_features(mtd);

	density = onenand_get_density(dev_id);
	if (FLEXONENAND(this)) {
		this->dies = ONENAND_IS_DDP(this) ? 2 : 1;
		
		mtd->numeraseregions = this->dies << 1;
		mtd->eraseregions = kzalloc(sizeof(struct mtd_erase_region_info)
					* (this->dies << 1), GFP_KERNEL);
		if (!mtd->eraseregions)
			return -ENOMEM;
	}

	this->chipsize = (16 << density) << 20;

	
	
	mtd->writesize = this->read_word(this->base + ONENAND_REG_DATA_BUFFER_SIZE);
	
	if (ONENAND_IS_4KB_PAGE(this))
		mtd->writesize <<= 1;

	mtd->oobsize = mtd->writesize >> 5;
	
	mtd->erasesize = mtd->writesize << 6;
	if (FLEXONENAND(this))
		mtd->erasesize <<= 1;

	this->erase_shift = ffs(mtd->erasesize) - 1;
	this->page_shift = ffs(mtd->writesize) - 1;
	this->page_mask = (1 << (this->erase_shift - this->page_shift)) - 1;
	
	if (ONENAND_IS_DDP(this))
		this->density_mask = this->chipsize >> (this->erase_shift + 1);
	
	this->writesize = mtd->writesize;

	

	if (FLEXONENAND(this))
		flexonenand_get_size(mtd);
	else
		mtd->size = this->chipsize;

	if (ONENAND_IS_2PLANE(this)) {
		mtd->writesize <<= 1;
		mtd->erasesize <<= 1;
	}

	return 0;
}

static int onenand_suspend(struct mtd_info *mtd)
{
	return onenand_get_device(mtd, FL_PM_SUSPENDED);
}

static void onenand_resume(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;

	if (this->state == FL_PM_SUSPENDED)
		onenand_release_device(mtd);
	else
		printk(KERN_ERR "%s: resume() called for the chip which is not "
				"in suspended state\n", __func__);
}

int onenand_scan(struct mtd_info *mtd, int maxchips)
{
	int i, ret;
	struct onenand_chip *this = mtd->priv;

	if (!this->read_word)
		this->read_word = onenand_readw;
	if (!this->write_word)
		this->write_word = onenand_writew;

	if (!this->command)
		this->command = onenand_command;
	if (!this->wait)
		onenand_setup_wait(mtd);
	if (!this->bbt_wait)
		this->bbt_wait = onenand_bbt_wait;
	if (!this->unlock_all)
		this->unlock_all = onenand_unlock_all;

	if (!this->chip_probe)
		this->chip_probe = onenand_chip_probe;

	if (!this->read_bufferram)
		this->read_bufferram = onenand_read_bufferram;
	if (!this->write_bufferram)
		this->write_bufferram = onenand_write_bufferram;

	if (!this->block_markbad)
		this->block_markbad = onenand_default_block_markbad;
	if (!this->scan_bbt)
		this->scan_bbt = onenand_default_bbt;

	if (onenand_probe(mtd))
		return -ENXIO;

	
	if (this->mmcontrol) {
		printk(KERN_INFO "OneNAND Sync. Burst Read support\n");
		this->read_bufferram = onenand_sync_read_bufferram;
	}

	
	if (!this->page_buf) {
		this->page_buf = kzalloc(mtd->writesize, GFP_KERNEL);
		if (!this->page_buf) {
			printk(KERN_ERR "%s: Can't allocate page_buf\n",
				__func__);
			return -ENOMEM;
		}
#ifdef CONFIG_MTD_ONENAND_VERIFY_WRITE
		this->verify_buf = kzalloc(mtd->writesize, GFP_KERNEL);
		if (!this->verify_buf) {
			kfree(this->page_buf);
			return -ENOMEM;
		}
#endif
		this->options |= ONENAND_PAGEBUF_ALLOC;
	}
	if (!this->oob_buf) {
		this->oob_buf = kzalloc(mtd->oobsize, GFP_KERNEL);
		if (!this->oob_buf) {
			printk(KERN_ERR "%s: Can't allocate oob_buf\n",
				__func__);
			if (this->options & ONENAND_PAGEBUF_ALLOC) {
				this->options &= ~ONENAND_PAGEBUF_ALLOC;
				kfree(this->page_buf);
			}
			return -ENOMEM;
		}
		this->options |= ONENAND_OOBBUF_ALLOC;
	}

	this->state = FL_READY;
	init_waitqueue_head(&this->wq);
	spin_lock_init(&this->chip_lock);

	switch (mtd->oobsize) {
	case 128:
		if (FLEXONENAND(this)) {
			this->ecclayout = &flexonenand_oob_128;
			mtd->subpage_sft = 0;
		} else {
			this->ecclayout = &onenand_oob_128;
			mtd->subpage_sft = 2;
		}
		if (ONENAND_IS_NOP_1(this))
			mtd->subpage_sft = 0;
		break;
	case 64:
		this->ecclayout = &onenand_oob_64;
		mtd->subpage_sft = 2;
		break;

	case 32:
		this->ecclayout = &onenand_oob_32;
		mtd->subpage_sft = 1;
		break;

	default:
		printk(KERN_WARNING "%s: No OOB scheme defined for oobsize %d\n",
			__func__, mtd->oobsize);
		mtd->subpage_sft = 0;
		
		this->ecclayout = &onenand_oob_32;
		break;
	}

	this->subpagesize = mtd->writesize >> mtd->subpage_sft;

	this->ecclayout->oobavail = 0;
	for (i = 0; i < MTD_MAX_OOBFREE_ENTRIES &&
	    this->ecclayout->oobfree[i].length; i++)
		this->ecclayout->oobavail +=
			this->ecclayout->oobfree[i].length;
	mtd->oobavail = this->ecclayout->oobavail;

	mtd->ecclayout = this->ecclayout;
	mtd->ecc_strength = 1;

	
	mtd->type = ONENAND_IS_MLC(this) ? MTD_MLCNANDFLASH : MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->_erase = onenand_erase;
	mtd->_point = NULL;
	mtd->_unpoint = NULL;
	mtd->_read = onenand_read;
	mtd->_write = onenand_write;
	mtd->_read_oob = onenand_read_oob;
	mtd->_write_oob = onenand_write_oob;
	mtd->_panic_write = onenand_panic_write;
#ifdef CONFIG_MTD_ONENAND_OTP
	mtd->_get_fact_prot_info = onenand_get_fact_prot_info;
	mtd->_read_fact_prot_reg = onenand_read_fact_prot_reg;
	mtd->_get_user_prot_info = onenand_get_user_prot_info;
	mtd->_read_user_prot_reg = onenand_read_user_prot_reg;
	mtd->_write_user_prot_reg = onenand_write_user_prot_reg;
	mtd->_lock_user_prot_reg = onenand_lock_user_prot_reg;
#endif
	mtd->_sync = onenand_sync;
	mtd->_lock = onenand_lock;
	mtd->_unlock = onenand_unlock;
	mtd->_suspend = onenand_suspend;
	mtd->_resume = onenand_resume;
	mtd->_block_isbad = onenand_block_isbad;
	mtd->_block_markbad = onenand_block_markbad;
	mtd->owner = THIS_MODULE;
	mtd->writebufsize = mtd->writesize;

	
	if (!(this->options & ONENAND_SKIP_INITIAL_UNLOCKING))
		this->unlock_all(mtd);

	ret = this->scan_bbt(mtd);
	if ((!FLEXONENAND(this)) || ret)
		return ret;

	
	for (i = 0; i < MAX_DIES; i++)
		flexonenand_set_boundary(mtd, i, flex_bdry[2 * i],
						 flex_bdry[(2 * i) + 1]);

	return 0;
}

void onenand_release(struct mtd_info *mtd)
{
	struct onenand_chip *this = mtd->priv;

	
	mtd_device_unregister(mtd);

	
	if (this->bbm) {
		struct bbm_info *bbm = this->bbm;
		kfree(bbm->bbt);
		kfree(this->bbm);
	}
	
	if (this->options & ONENAND_PAGEBUF_ALLOC) {
		kfree(this->page_buf);
#ifdef CONFIG_MTD_ONENAND_VERIFY_WRITE
		kfree(this->verify_buf);
#endif
	}
	if (this->options & ONENAND_OOBBUF_ALLOC)
		kfree(this->oob_buf);
	kfree(mtd->eraseregions);
}

EXPORT_SYMBOL_GPL(onenand_scan);
EXPORT_SYMBOL_GPL(onenand_release);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kyungmin Park <kyungmin.park@samsung.com>");
MODULE_DESCRIPTION("Generic OneNAND flash driver code");
