/* Freescale Enhanced Local Bus Controller NAND driver
 *
 * Copyright © 2006-2007, 2010 Freescale Semiconductor
 *
 * Authors: Nick Spence <nick.spence@freescale.com>,
 *          Scott Wood <scottwood@freescale.com>
 *          Jack Lan <jack.lan@freescale.com>
 *          Roy Zang <tie-fei.zang@freescale.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>
#include <asm/fsl_lbc.h>

#define MAX_BANKS 8
#define ERR_BYTE 0xFF 
#define FCM_TIMEOUT_MSECS 500 


struct fsl_elbc_mtd {
	struct mtd_info mtd;
	struct nand_chip chip;
	struct fsl_lbc_ctrl *ctrl;

	struct device *dev;
	int bank;               
	u8 __iomem *vbase;      
	int page_size;          
	unsigned int fmr;       
};


struct fsl_elbc_fcm_ctrl {
	struct nand_hw_control controller;
	struct fsl_elbc_mtd *chips[MAX_BANKS];

	u8 __iomem *addr;        
	unsigned int page;       /* Last page written to / read from      */
	unsigned int read_bytes; 
	unsigned int column;     
	unsigned int index;      
	unsigned int status;     
	unsigned int mdr;        
	unsigned int use_mdr;    
	unsigned int oob;        
	unsigned int counter;	 
};


static struct nand_ecclayout fsl_elbc_oob_sp_eccm0 = {
	.eccbytes = 3,
	.eccpos = {6, 7, 8},
	.oobfree = { {0, 5}, {9, 7} },
};

static struct nand_ecclayout fsl_elbc_oob_sp_eccm1 = {
	.eccbytes = 3,
	.eccpos = {8, 9, 10},
	.oobfree = { {0, 5}, {6, 2}, {11, 5} },
};

static struct nand_ecclayout fsl_elbc_oob_lp_eccm0 = {
	.eccbytes = 12,
	.eccpos = {6, 7, 8, 22, 23, 24, 38, 39, 40, 54, 55, 56},
	.oobfree = { {1, 5}, {9, 13}, {25, 13}, {41, 13}, {57, 7} },
};

static struct nand_ecclayout fsl_elbc_oob_lp_eccm1 = {
	.eccbytes = 12,
	.eccpos = {8, 9, 10, 24, 25, 26, 40, 41, 42, 56, 57, 58},
	.oobfree = { {1, 7}, {11, 13}, {27, 13}, {43, 13}, {59, 5} },
};

static u8 scan_ff_pattern[] = { 0xff, };

static struct nand_bbt_descr largepage_memorybased = {
	.options = 0,
	.offs = 0,
	.len = 1,
	.pattern = scan_ff_pattern,
};

static u8 bbt_pattern[] = {'B', 'b', 't', '0' };
static u8 mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		   NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	11,
	.len = 4,
	.veroffs = 15,
	.maxblocks = 4,
	.pattern = bbt_pattern,
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		   NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	11,
	.len = 4,
	.veroffs = 15,
	.maxblocks = 4,
	.pattern = mirror_pattern,
};


static void set_addr(struct mtd_info *mtd, int column, int page_addr, int oob)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = ctrl->nand;
	int buf_num;

	elbc_fcm_ctrl->page = page_addr;

	if (priv->page_size) {
		out_be32(&lbc->fbar, page_addr >> 6);
		out_be32(&lbc->fpar,
		         ((page_addr << FPAR_LP_PI_SHIFT) & FPAR_LP_PI) |
		         (oob ? FPAR_LP_MS : 0) | column);
		buf_num = (page_addr & 1) << 2;
	} else {
		out_be32(&lbc->fbar, page_addr >> 5);
		out_be32(&lbc->fpar,
		         ((page_addr << FPAR_SP_PI_SHIFT) & FPAR_SP_PI) |
		         (oob ? FPAR_SP_MS : 0) | column);
		buf_num = page_addr & 7;
	}

	elbc_fcm_ctrl->addr = priv->vbase + buf_num * 1024;
	elbc_fcm_ctrl->index = column;

	
	if (oob)
		elbc_fcm_ctrl->index += priv->page_size ? 2048 : 512;

	dev_vdbg(priv->dev, "set_addr: bank=%d, "
			    "elbc_fcm_ctrl->addr=0x%p (0x%p), "
	                    "index %x, pes %d ps %d\n",
		 buf_num, elbc_fcm_ctrl->addr, priv->vbase,
		 elbc_fcm_ctrl->index,
	         chip->phys_erase_shift, chip->page_shift);
}

static int fsl_elbc_run_command(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = ctrl->nand;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;

	
	out_be32(&lbc->fmr, priv->fmr | 3);
	if (elbc_fcm_ctrl->use_mdr)
		out_be32(&lbc->mdr, elbc_fcm_ctrl->mdr);

	dev_vdbg(priv->dev,
	         "fsl_elbc_run_command: fmr=%08x fir=%08x fcr=%08x\n",
	         in_be32(&lbc->fmr), in_be32(&lbc->fir), in_be32(&lbc->fcr));
	dev_vdbg(priv->dev,
	         "fsl_elbc_run_command: fbar=%08x fpar=%08x "
	         "fbcr=%08x bank=%d\n",
	         in_be32(&lbc->fbar), in_be32(&lbc->fpar),
	         in_be32(&lbc->fbcr), priv->bank);

	ctrl->irq_status = 0;
	
	out_be32(&lbc->lsor, priv->bank);

	
	wait_event_timeout(ctrl->irq_wait, ctrl->irq_status,
	                   FCM_TIMEOUT_MSECS * HZ/1000);
	elbc_fcm_ctrl->status = ctrl->irq_status;
	
	if (elbc_fcm_ctrl->use_mdr)
		elbc_fcm_ctrl->mdr = in_be32(&lbc->mdr);

	elbc_fcm_ctrl->use_mdr = 0;

	if (elbc_fcm_ctrl->status != LTESR_CC) {
		dev_info(priv->dev,
		         "command failed: fir %x fcr %x status %x mdr %x\n",
		         in_be32(&lbc->fir), in_be32(&lbc->fcr),
			 elbc_fcm_ctrl->status, elbc_fcm_ctrl->mdr);
		return -EIO;
	}

	if (chip->ecc.mode != NAND_ECC_HW)
		return 0;

	if (elbc_fcm_ctrl->read_bytes == mtd->writesize + mtd->oobsize) {
		uint32_t lteccr = in_be32(&lbc->lteccr);
		if (lteccr & 0x000F000F)
			out_be32(&lbc->lteccr, 0x000F000F); 
		if (lteccr & 0x000F0000)
			mtd->ecc_stats.corrected++;
	}

	return 0;
}

static void fsl_elbc_do_read(struct nand_chip *chip, int oob)
{
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;

	if (priv->page_size) {
		out_be32(&lbc->fir,
		         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		         (FIR_OP_CA  << FIR_OP1_SHIFT) |
		         (FIR_OP_PA  << FIR_OP2_SHIFT) |
		         (FIR_OP_CM1 << FIR_OP3_SHIFT) |
		         (FIR_OP_RBW << FIR_OP4_SHIFT));

		out_be32(&lbc->fcr, (NAND_CMD_READ0 << FCR_CMD0_SHIFT) |
		                    (NAND_CMD_READSTART << FCR_CMD1_SHIFT));
	} else {
		out_be32(&lbc->fir,
		         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		         (FIR_OP_CA  << FIR_OP1_SHIFT) |
		         (FIR_OP_PA  << FIR_OP2_SHIFT) |
		         (FIR_OP_RBW << FIR_OP3_SHIFT));

		if (oob)
			out_be32(&lbc->fcr, NAND_CMD_READOOB << FCR_CMD0_SHIFT);
		else
			out_be32(&lbc->fcr, NAND_CMD_READ0 << FCR_CMD0_SHIFT);
	}
}

static void fsl_elbc_cmdfunc(struct mtd_info *mtd, unsigned int command,
                             int column, int page_addr)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = ctrl->nand;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;

	elbc_fcm_ctrl->use_mdr = 0;

	
	elbc_fcm_ctrl->read_bytes = 0;
	if (command != NAND_CMD_PAGEPROG)
		elbc_fcm_ctrl->index = 0;

	switch (command) {
	
	case NAND_CMD_READ1:
		column += 256;

	
	case NAND_CMD_READ0:
		dev_dbg(priv->dev,
		        "fsl_elbc_cmdfunc: NAND_CMD_READ0, page_addr:"
		        " 0x%x, column: 0x%x.\n", page_addr, column);


		out_be32(&lbc->fbcr, 0); 
		set_addr(mtd, 0, page_addr, 0);

		elbc_fcm_ctrl->read_bytes = mtd->writesize + mtd->oobsize;
		elbc_fcm_ctrl->index += column;

		fsl_elbc_do_read(chip, 0);
		fsl_elbc_run_command(mtd);
		return;

	
	case NAND_CMD_READOOB:
		dev_vdbg(priv->dev,
		         "fsl_elbc_cmdfunc: NAND_CMD_READOOB, page_addr:"
			 " 0x%x, column: 0x%x.\n", page_addr, column);

		out_be32(&lbc->fbcr, mtd->oobsize - column);
		set_addr(mtd, column, page_addr, 1);

		elbc_fcm_ctrl->read_bytes = mtd->writesize + mtd->oobsize;

		fsl_elbc_do_read(chip, 1);
		fsl_elbc_run_command(mtd);
		return;

	case NAND_CMD_READID:
	case NAND_CMD_PARAM:
		dev_vdbg(priv->dev, "fsl_elbc_cmdfunc: NAND_CMD %x\n", command);

		out_be32(&lbc->fir, (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		                    (FIR_OP_UA  << FIR_OP1_SHIFT) |
		                    (FIR_OP_RBW << FIR_OP2_SHIFT));
		out_be32(&lbc->fcr, command << FCR_CMD0_SHIFT);
		out_be32(&lbc->fbcr, 256);
		elbc_fcm_ctrl->read_bytes = 256;
		elbc_fcm_ctrl->use_mdr = 1;
		elbc_fcm_ctrl->mdr = column;
		set_addr(mtd, 0, 0, 0);
		fsl_elbc_run_command(mtd);
		return;

	
	case NAND_CMD_ERASE1:
		dev_vdbg(priv->dev,
		         "fsl_elbc_cmdfunc: NAND_CMD_ERASE1, "
		         "page_addr: 0x%x.\n", page_addr);
		set_addr(mtd, 0, page_addr, 0);
		return;

	
	case NAND_CMD_ERASE2:
		dev_vdbg(priv->dev, "fsl_elbc_cmdfunc: NAND_CMD_ERASE2.\n");

		out_be32(&lbc->fir,
		         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		         (FIR_OP_PA  << FIR_OP1_SHIFT) |
		         (FIR_OP_CM2 << FIR_OP2_SHIFT) |
		         (FIR_OP_CW1 << FIR_OP3_SHIFT) |
		         (FIR_OP_RS  << FIR_OP4_SHIFT));

		out_be32(&lbc->fcr,
		         (NAND_CMD_ERASE1 << FCR_CMD0_SHIFT) |
		         (NAND_CMD_STATUS << FCR_CMD1_SHIFT) |
		         (NAND_CMD_ERASE2 << FCR_CMD2_SHIFT));

		out_be32(&lbc->fbcr, 0);
		elbc_fcm_ctrl->read_bytes = 0;
		elbc_fcm_ctrl->use_mdr = 1;

		fsl_elbc_run_command(mtd);
		return;

	
	case NAND_CMD_SEQIN: {
		__be32 fcr;
		dev_vdbg(priv->dev,
			 "fsl_elbc_cmdfunc: NAND_CMD_SEQIN/PAGE_PROG, "
		         "page_addr: 0x%x, column: 0x%x.\n",
		         page_addr, column);

		elbc_fcm_ctrl->column = column;
		elbc_fcm_ctrl->use_mdr = 1;

		if (column >= mtd->writesize) {
			
			column -= mtd->writesize;
			elbc_fcm_ctrl->oob = 1;
		} else {
			WARN_ON(column != 0);
			elbc_fcm_ctrl->oob = 0;
		}

		fcr = (NAND_CMD_STATUS   << FCR_CMD1_SHIFT) |
		      (NAND_CMD_SEQIN    << FCR_CMD2_SHIFT) |
		      (NAND_CMD_PAGEPROG << FCR_CMD3_SHIFT);

		if (priv->page_size) {
			out_be32(&lbc->fir,
			         (FIR_OP_CM2 << FIR_OP0_SHIFT) |
			         (FIR_OP_CA  << FIR_OP1_SHIFT) |
			         (FIR_OP_PA  << FIR_OP2_SHIFT) |
			         (FIR_OP_WB  << FIR_OP3_SHIFT) |
			         (FIR_OP_CM3 << FIR_OP4_SHIFT) |
			         (FIR_OP_CW1 << FIR_OP5_SHIFT) |
			         (FIR_OP_RS  << FIR_OP6_SHIFT));
		} else {
			out_be32(&lbc->fir,
			         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
			         (FIR_OP_CM2 << FIR_OP1_SHIFT) |
			         (FIR_OP_CA  << FIR_OP2_SHIFT) |
			         (FIR_OP_PA  << FIR_OP3_SHIFT) |
			         (FIR_OP_WB  << FIR_OP4_SHIFT) |
			         (FIR_OP_CM3 << FIR_OP5_SHIFT) |
			         (FIR_OP_CW1 << FIR_OP6_SHIFT) |
			         (FIR_OP_RS  << FIR_OP7_SHIFT));

			if (elbc_fcm_ctrl->oob)
				
				fcr |= NAND_CMD_READOOB << FCR_CMD0_SHIFT;
			else
				
				fcr |= NAND_CMD_READ0 << FCR_CMD0_SHIFT;
		}

		out_be32(&lbc->fcr, fcr);
		set_addr(mtd, column, page_addr, elbc_fcm_ctrl->oob);
		return;
	}

	
	case NAND_CMD_PAGEPROG: {
		dev_vdbg(priv->dev,
		         "fsl_elbc_cmdfunc: NAND_CMD_PAGEPROG "
			 "writing %d bytes.\n", elbc_fcm_ctrl->index);

		if (elbc_fcm_ctrl->oob || elbc_fcm_ctrl->column != 0 ||
		    elbc_fcm_ctrl->index != mtd->writesize + mtd->oobsize)
			out_be32(&lbc->fbcr,
				elbc_fcm_ctrl->index - elbc_fcm_ctrl->column);
		else
			out_be32(&lbc->fbcr, 0);

		fsl_elbc_run_command(mtd);
		return;
	}

	
	
	case NAND_CMD_STATUS:
		out_be32(&lbc->fir,
		         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		         (FIR_OP_RBW << FIR_OP1_SHIFT));
		out_be32(&lbc->fcr, NAND_CMD_STATUS << FCR_CMD0_SHIFT);
		out_be32(&lbc->fbcr, 1);
		set_addr(mtd, 0, 0, 0);
		elbc_fcm_ctrl->read_bytes = 1;

		fsl_elbc_run_command(mtd);

		setbits8(elbc_fcm_ctrl->addr, NAND_STATUS_WP);
		return;

	
	case NAND_CMD_RESET:
		dev_dbg(priv->dev, "fsl_elbc_cmdfunc: NAND_CMD_RESET.\n");
		out_be32(&lbc->fir, FIR_OP_CM0 << FIR_OP0_SHIFT);
		out_be32(&lbc->fcr, NAND_CMD_RESET << FCR_CMD0_SHIFT);
		fsl_elbc_run_command(mtd);
		return;

	default:
		dev_err(priv->dev,
		        "fsl_elbc_cmdfunc: error, unsupported command 0x%x.\n",
		        command);
	}
}

static void fsl_elbc_select_chip(struct mtd_info *mtd, int chip)
{
}

static void fsl_elbc_write_buf(struct mtd_info *mtd, const u8 *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;
	unsigned int bufsize = mtd->writesize + mtd->oobsize;

	if (len <= 0) {
		dev_err(priv->dev, "write_buf of %d bytes", len);
		elbc_fcm_ctrl->status = 0;
		return;
	}

	if ((unsigned int)len > bufsize - elbc_fcm_ctrl->index) {
		dev_err(priv->dev,
		        "write_buf beyond end of buffer "
		        "(%d requested, %u available)\n",
			len, bufsize - elbc_fcm_ctrl->index);
		len = bufsize - elbc_fcm_ctrl->index;
	}

	memcpy_toio(&elbc_fcm_ctrl->addr[elbc_fcm_ctrl->index], buf, len);
	in_8(&elbc_fcm_ctrl->addr[elbc_fcm_ctrl->index] + len - 1);

	elbc_fcm_ctrl->index += len;
}

static u8 fsl_elbc_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;

	
	if (elbc_fcm_ctrl->index < elbc_fcm_ctrl->read_bytes)
		return in_8(&elbc_fcm_ctrl->addr[elbc_fcm_ctrl->index++]);

	dev_err(priv->dev, "read_byte beyond end of buffer\n");
	return ERR_BYTE;
}

static void fsl_elbc_read_buf(struct mtd_info *mtd, u8 *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;
	int avail;

	if (len < 0)
		return;

	avail = min((unsigned int)len,
			elbc_fcm_ctrl->read_bytes - elbc_fcm_ctrl->index);
	memcpy_fromio(buf, &elbc_fcm_ctrl->addr[elbc_fcm_ctrl->index], avail);
	elbc_fcm_ctrl->index += avail;

	if (len > avail)
		dev_err(priv->dev,
		        "read_buf beyond end of buffer "
		        "(%d requested, %d available)\n",
		        len, avail);
}

static int fsl_elbc_verify_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;
	int i;

	if (len < 0) {
		dev_err(priv->dev, "write_buf of %d bytes", len);
		return -EINVAL;
	}

	if ((unsigned int)len >
			elbc_fcm_ctrl->read_bytes - elbc_fcm_ctrl->index) {
		dev_err(priv->dev,
			"verify_buf beyond end of buffer "
			"(%d requested, %u available)\n",
			len, elbc_fcm_ctrl->read_bytes - elbc_fcm_ctrl->index);

		elbc_fcm_ctrl->index = elbc_fcm_ctrl->read_bytes;
		return -EINVAL;
	}

	for (i = 0; i < len; i++)
		if (in_8(&elbc_fcm_ctrl->addr[elbc_fcm_ctrl->index + i])
				!= buf[i])
			break;

	elbc_fcm_ctrl->index += len;
	return i == len && elbc_fcm_ctrl->status == LTESR_CC ? 0 : -EIO;
}

static int fsl_elbc_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;

	if (elbc_fcm_ctrl->status != LTESR_CC)
		return NAND_STATUS_FAIL;

	return (elbc_fcm_ctrl->mdr & 0xff) | NAND_STATUS_WP;
}

static int fsl_elbc_chip_init_tail(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_elbc_mtd *priv = chip->priv;
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;
	unsigned int al;

	
	al = 0;
	if (chip->pagemask & 0xffff0000)
		al++;
	if (chip->pagemask & 0xff000000)
		al++;

	priv->fmr |= al << FMR_AL_SHIFT;

	dev_dbg(priv->dev, "fsl_elbc_init: nand->numchips = %d\n",
	        chip->numchips);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->chipsize = %lld\n",
	        chip->chipsize);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->pagemask = %8x\n",
	        chip->pagemask);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->chip_delay = %d\n",
	        chip->chip_delay);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->badblockpos = %d\n",
	        chip->badblockpos);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->chip_shift = %d\n",
	        chip->chip_shift);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->page_shift = %d\n",
	        chip->page_shift);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->phys_erase_shift = %d\n",
	        chip->phys_erase_shift);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecclayout = %p\n",
	        chip->ecclayout);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecc.mode = %d\n",
	        chip->ecc.mode);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecc.steps = %d\n",
	        chip->ecc.steps);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecc.bytes = %d\n",
	        chip->ecc.bytes);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecc.total = %d\n",
	        chip->ecc.total);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecc.layout = %p\n",
	        chip->ecc.layout);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->flags = %08x\n", mtd->flags);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->size = %lld\n", mtd->size);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->erasesize = %d\n",
	        mtd->erasesize);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->writesize = %d\n",
	        mtd->writesize);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->oobsize = %d\n",
	        mtd->oobsize);

	
	if (mtd->writesize == 512) {
		priv->page_size = 0;
		clrbits32(&lbc->bank[priv->bank].or, OR_FCM_PGS);
	} else if (mtd->writesize == 2048) {
		priv->page_size = 1;
		setbits32(&lbc->bank[priv->bank].or, OR_FCM_PGS);
		
		if ((in_be32(&lbc->bank[priv->bank].br) & BR_DECC) ==
		    BR_DECC_CHK_GEN) {
			chip->ecc.size = 512;
			chip->ecc.layout = (priv->fmr & FMR_ECCM) ?
			                   &fsl_elbc_oob_lp_eccm1 :
			                   &fsl_elbc_oob_lp_eccm0;
			chip->badblock_pattern = &largepage_memorybased;
		}
	} else {
		dev_err(priv->dev,
		        "fsl_elbc_init: page size %d is not supported\n",
		        mtd->writesize);
		return -1;
	}

	return 0;
}

static int fsl_elbc_read_page(struct mtd_info *mtd,
                              struct nand_chip *chip,
			      uint8_t *buf,
			      int page)
{
	fsl_elbc_read_buf(mtd, buf, mtd->writesize);
	fsl_elbc_read_buf(mtd, chip->oob_poi, mtd->oobsize);

	if (fsl_elbc_wait(mtd, chip) & NAND_STATUS_FAIL)
		mtd->ecc_stats.failed++;

	return 0;
}

static void fsl_elbc_write_page(struct mtd_info *mtd,
                                struct nand_chip *chip,
                                const uint8_t *buf)
{
	fsl_elbc_write_buf(mtd, buf, mtd->writesize);
	fsl_elbc_write_buf(mtd, chip->oob_poi, mtd->oobsize);
}

static int fsl_elbc_chip_init(struct fsl_elbc_mtd *priv)
{
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = ctrl->nand;
	struct nand_chip *chip = &priv->chip;

	dev_dbg(priv->dev, "eLBC Set Information for bank %d\n", priv->bank);

	
	priv->mtd.priv = chip;
	priv->mtd.owner = THIS_MODULE;

	
	priv->fmr = 15 << FMR_CWTO_SHIFT;
	if (in_be32(&lbc->bank[priv->bank].or) & OR_FCM_PGS)
		priv->fmr |= FMR_ECCM;

	
	
	chip->read_byte = fsl_elbc_read_byte;
	chip->write_buf = fsl_elbc_write_buf;
	chip->read_buf = fsl_elbc_read_buf;
	chip->verify_buf = fsl_elbc_verify_buf;
	chip->select_chip = fsl_elbc_select_chip;
	chip->cmdfunc = fsl_elbc_cmdfunc;
	chip->waitfunc = fsl_elbc_wait;

	chip->bbt_td = &bbt_main_descr;
	chip->bbt_md = &bbt_mirror_descr;

	
	chip->options = NAND_NO_READRDY | NAND_NO_AUTOINCR;
	chip->bbt_options = NAND_BBT_USE_FLASH;

	chip->controller = &elbc_fcm_ctrl->controller;
	chip->priv = priv;

	chip->ecc.read_page = fsl_elbc_read_page;
	chip->ecc.write_page = fsl_elbc_write_page;

	
	if ((in_be32(&lbc->bank[priv->bank].br) & BR_DECC) ==
	    BR_DECC_CHK_GEN) {
		chip->ecc.mode = NAND_ECC_HW;
		
		chip->ecc.layout = (priv->fmr & FMR_ECCM) ?
				&fsl_elbc_oob_sp_eccm1 : &fsl_elbc_oob_sp_eccm0;
		chip->ecc.size = 512;
		chip->ecc.bytes = 3;
		chip->ecc.strength = 1;
	} else {
		
		chip->ecc.mode = NAND_ECC_SOFT;
	}

	return 0;
}

static int fsl_elbc_chip_remove(struct fsl_elbc_mtd *priv)
{
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;
	nand_release(&priv->mtd);

	kfree(priv->mtd.name);

	if (priv->vbase)
		iounmap(priv->vbase);

	elbc_fcm_ctrl->chips[priv->bank] = NULL;
	kfree(priv);
	return 0;
}

static DEFINE_MUTEX(fsl_elbc_nand_mutex);

static int __devinit fsl_elbc_nand_probe(struct platform_device *pdev)
{
	struct fsl_lbc_regs __iomem *lbc;
	struct fsl_elbc_mtd *priv;
	struct resource res;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl;
	static const char *part_probe_types[]
		= { "cmdlinepart", "RedBoot", "ofpart", NULL };
	int ret;
	int bank;
	struct device *dev;
	struct device_node *node = pdev->dev.of_node;
	struct mtd_part_parser_data ppdata;

	ppdata.of_node = pdev->dev.of_node;
	if (!fsl_lbc_ctrl_dev || !fsl_lbc_ctrl_dev->regs)
		return -ENODEV;
	lbc = fsl_lbc_ctrl_dev->regs;
	dev = fsl_lbc_ctrl_dev->dev;

	
	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		dev_err(dev, "failed to get resource\n");
		return ret;
	}

	
	for (bank = 0; bank < MAX_BANKS; bank++)
		if ((in_be32(&lbc->bank[bank].br) & BR_V) &&
		    (in_be32(&lbc->bank[bank].br) & BR_MSEL) == BR_MS_FCM &&
		    (in_be32(&lbc->bank[bank].br) &
		     in_be32(&lbc->bank[bank].or) & BR_BA)
		     == fsl_lbc_addr(res.start))
			break;

	if (bank >= MAX_BANKS) {
		dev_err(dev, "address did not match any chip selects\n");
		return -ENODEV;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mutex_lock(&fsl_elbc_nand_mutex);
	if (!fsl_lbc_ctrl_dev->nand) {
		elbc_fcm_ctrl = kzalloc(sizeof(*elbc_fcm_ctrl), GFP_KERNEL);
		if (!elbc_fcm_ctrl) {
			dev_err(dev, "failed to allocate memory\n");
			mutex_unlock(&fsl_elbc_nand_mutex);
			ret = -ENOMEM;
			goto err;
		}
		elbc_fcm_ctrl->counter++;

		spin_lock_init(&elbc_fcm_ctrl->controller.lock);
		init_waitqueue_head(&elbc_fcm_ctrl->controller.wq);
		fsl_lbc_ctrl_dev->nand = elbc_fcm_ctrl;
	} else {
		elbc_fcm_ctrl = fsl_lbc_ctrl_dev->nand;
	}
	mutex_unlock(&fsl_elbc_nand_mutex);

	elbc_fcm_ctrl->chips[bank] = priv;
	priv->bank = bank;
	priv->ctrl = fsl_lbc_ctrl_dev;
	priv->dev = dev;

	priv->vbase = ioremap(res.start, resource_size(&res));
	if (!priv->vbase) {
		dev_err(dev, "failed to map chip region\n");
		ret = -ENOMEM;
		goto err;
	}

	priv->mtd.name = kasprintf(GFP_KERNEL, "%x.flash", (unsigned)res.start);
	if (!priv->mtd.name) {
		ret = -ENOMEM;
		goto err;
	}

	ret = fsl_elbc_chip_init(priv);
	if (ret)
		goto err;

	ret = nand_scan_ident(&priv->mtd, 1, NULL);
	if (ret)
		goto err;

	ret = fsl_elbc_chip_init_tail(&priv->mtd);
	if (ret)
		goto err;

	ret = nand_scan_tail(&priv->mtd);
	if (ret)
		goto err;

	mtd_device_parse_register(&priv->mtd, part_probe_types, &ppdata,
				  NULL, 0);

	printk(KERN_INFO "eLBC NAND device at 0x%llx, bank %d\n",
	       (unsigned long long)res.start, priv->bank);
	return 0;

err:
	fsl_elbc_chip_remove(priv);
	return ret;
}

static int fsl_elbc_nand_remove(struct platform_device *pdev)
{
	int i;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = fsl_lbc_ctrl_dev->nand;
	for (i = 0; i < MAX_BANKS; i++)
		if (elbc_fcm_ctrl->chips[i])
			fsl_elbc_chip_remove(elbc_fcm_ctrl->chips[i]);

	mutex_lock(&fsl_elbc_nand_mutex);
	elbc_fcm_ctrl->counter--;
	if (!elbc_fcm_ctrl->counter) {
		fsl_lbc_ctrl_dev->nand = NULL;
		kfree(elbc_fcm_ctrl);
	}
	mutex_unlock(&fsl_elbc_nand_mutex);

	return 0;

}

static const struct of_device_id fsl_elbc_nand_match[] = {
	{ .compatible = "fsl,elbc-fcm-nand", },
	{}
};

static struct platform_driver fsl_elbc_nand_driver = {
	.driver = {
		.name = "fsl,elbc-fcm-nand",
		.owner = THIS_MODULE,
		.of_match_table = fsl_elbc_nand_match,
	},
	.probe = fsl_elbc_nand_probe,
	.remove = fsl_elbc_nand_remove,
};

module_platform_driver(fsl_elbc_nand_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Freescale");
MODULE_DESCRIPTION("Freescale Enhanced Local Bus Controller MTD NAND driver");
