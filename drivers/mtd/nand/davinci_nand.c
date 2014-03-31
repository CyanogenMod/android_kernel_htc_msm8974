/*
 * davinci_nand.c - NAND Flash Driver for DaVinci family chips
 *
 * Copyright © 2006 Texas Instruments.
 *
 * Port to 2.6.23 Copyright © 2008 by:
 *   Sander Huijsen <Shuijsen@optelecom-nkf.com>
 *   Troy Kisky <troy.kisky@boundarydevices.com>
 *   Dirk Behme <Dirk.Behme@gmail.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/slab.h>

#include <mach/nand.h>
#include <mach/aemif.h>

struct davinci_nand_info {
	struct mtd_info		mtd;
	struct nand_chip	chip;
	struct nand_ecclayout	ecclayout;

	struct device		*dev;
	struct clk		*clk;

	bool			is_readmode;

	void __iomem		*base;
	void __iomem		*vaddr;

	uint32_t		ioaddr;
	uint32_t		current_cs;

	uint32_t		mask_chipsel;
	uint32_t		mask_ale;
	uint32_t		mask_cle;

	uint32_t		core_chipsel;

	struct davinci_aemif_timing	*timing;
};

static DEFINE_SPINLOCK(davinci_nand_lock);
static bool ecc4_busy;

#define to_davinci_nand(m) container_of(m, struct davinci_nand_info, mtd)


static inline unsigned int davinci_nand_readl(struct davinci_nand_info *info,
		int offset)
{
	return __raw_readl(info->base + offset);
}

static inline void davinci_nand_writel(struct davinci_nand_info *info,
		int offset, unsigned long value)
{
	__raw_writel(value, info->base + offset);
}



static void nand_davinci_hwcontrol(struct mtd_info *mtd, int cmd,
				   unsigned int ctrl)
{
	struct davinci_nand_info	*info = to_davinci_nand(mtd);
	uint32_t			addr = info->current_cs;
	struct nand_chip		*nand = mtd->priv;

	
	if (ctrl & NAND_CTRL_CHANGE) {
		if ((ctrl & NAND_CTRL_CLE) == NAND_CTRL_CLE)
			addr |= info->mask_cle;
		else if ((ctrl & NAND_CTRL_ALE) == NAND_CTRL_ALE)
			addr |= info->mask_ale;

		nand->IO_ADDR_W = (void __iomem __force *)addr;
	}

	if (cmd != NAND_CMD_NONE)
		iowrite8(cmd, nand->IO_ADDR_W);
}

static void nand_davinci_select_chip(struct mtd_info *mtd, int chip)
{
	struct davinci_nand_info	*info = to_davinci_nand(mtd);
	uint32_t			addr = info->ioaddr;

	
	if (chip > 0)
		addr |= info->mask_chipsel;
	info->current_cs = addr;

	info->chip.IO_ADDR_W = (void __iomem __force *)addr;
	info->chip.IO_ADDR_R = info->chip.IO_ADDR_W;
}



static inline uint32_t nand_davinci_readecc_1bit(struct mtd_info *mtd)
{
	struct davinci_nand_info *info = to_davinci_nand(mtd);

	return davinci_nand_readl(info, NANDF1ECC_OFFSET
			+ 4 * info->core_chipsel);
}

static void nand_davinci_hwctl_1bit(struct mtd_info *mtd, int mode)
{
	struct davinci_nand_info *info;
	uint32_t nandcfr;
	unsigned long flags;

	info = to_davinci_nand(mtd);

	
	nand_davinci_readecc_1bit(mtd);

	spin_lock_irqsave(&davinci_nand_lock, flags);

	
	nandcfr = davinci_nand_readl(info, NANDFCR_OFFSET);
	nandcfr |= BIT(8 + info->core_chipsel);
	davinci_nand_writel(info, NANDFCR_OFFSET, nandcfr);

	spin_unlock_irqrestore(&davinci_nand_lock, flags);
}

static int nand_davinci_calculate_1bit(struct mtd_info *mtd,
				      const u_char *dat, u_char *ecc_code)
{
	unsigned int ecc_val = nand_davinci_readecc_1bit(mtd);
	unsigned int ecc24 = (ecc_val & 0x0fff) | ((ecc_val & 0x0fff0000) >> 4);

	
	ecc24 = ~ecc24;
	ecc_code[0] = (u_char)(ecc24);
	ecc_code[1] = (u_char)(ecc24 >> 8);
	ecc_code[2] = (u_char)(ecc24 >> 16);

	return 0;
}

static int nand_davinci_correct_1bit(struct mtd_info *mtd, u_char *dat,
				     u_char *read_ecc, u_char *calc_ecc)
{
	struct nand_chip *chip = mtd->priv;
	uint32_t eccNand = read_ecc[0] | (read_ecc[1] << 8) |
					  (read_ecc[2] << 16);
	uint32_t eccCalc = calc_ecc[0] | (calc_ecc[1] << 8) |
					  (calc_ecc[2] << 16);
	uint32_t diff = eccCalc ^ eccNand;

	if (diff) {
		if ((((diff >> 12) ^ diff) & 0xfff) == 0xfff) {
			
			if ((diff >> (12 + 3)) < chip->ecc.size) {
				dat[diff >> (12 + 3)] ^= BIT((diff >> 12) & 7);
				return 1;
			} else {
				return -1;
			}
		} else if (!(diff & (diff - 1))) {
			return 1;
		} else {
			
			return -1;
		}

	}
	return 0;
}



static void nand_davinci_hwctl_4bit(struct mtd_info *mtd, int mode)
{
	struct davinci_nand_info *info = to_davinci_nand(mtd);
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&davinci_nand_lock, flags);

	
	val = davinci_nand_readl(info, NANDFCR_OFFSET);
	val &= ~(0x03 << 4);
	val |= (info->core_chipsel << 4) | BIT(12);
	davinci_nand_writel(info, NANDFCR_OFFSET, val);

	info->is_readmode = (mode == NAND_ECC_READ);

	spin_unlock_irqrestore(&davinci_nand_lock, flags);
}

static void
nand_davinci_readecc_4bit(struct davinci_nand_info *info, u32 code[4])
{
	const u32 mask = 0x03ff03ff;

	code[0] = davinci_nand_readl(info, NAND_4BIT_ECC1_OFFSET) & mask;
	code[1] = davinci_nand_readl(info, NAND_4BIT_ECC2_OFFSET) & mask;
	code[2] = davinci_nand_readl(info, NAND_4BIT_ECC3_OFFSET) & mask;
	code[3] = davinci_nand_readl(info, NAND_4BIT_ECC4_OFFSET) & mask;
}

/* Terminate read ECC; or return ECC (as bytes) of data written to NAND. */
static int nand_davinci_calculate_4bit(struct mtd_info *mtd,
		const u_char *dat, u_char *ecc_code)
{
	struct davinci_nand_info *info = to_davinci_nand(mtd);
	u32 raw_ecc[4], *p;
	unsigned i;

	if (info->is_readmode) {
		davinci_nand_readl(info, NAND_4BIT_ECC1_OFFSET);
		return 0;
	}

	nand_davinci_readecc_4bit(info, raw_ecc);
	for (i = 0, p = raw_ecc; i < 2; i++, p += 2) {
		*ecc_code++ =   p[0]        & 0xff;
		*ecc_code++ = ((p[0] >>  8) & 0x03) | ((p[0] >> 14) & 0xfc);
		*ecc_code++ = ((p[0] >> 22) & 0x0f) | ((p[1] <<  4) & 0xf0);
		*ecc_code++ = ((p[1] >>  4) & 0x3f) | ((p[1] >> 10) & 0xc0);
		*ecc_code++ =  (p[1] >> 18) & 0xff;
	}

	return 0;
}

/* Correct up to 4 bits in data we just read, using state left in the
 * hardware plus the ecc_code computed when it was first written.
 */
static int nand_davinci_correct_4bit(struct mtd_info *mtd,
		u_char *data, u_char *ecc_code, u_char *null)
{
	int i;
	struct davinci_nand_info *info = to_davinci_nand(mtd);
	unsigned short ecc10[8];
	unsigned short *ecc16;
	u32 syndrome[4];
	u32 ecc_state;
	unsigned num_errors, corrected;
	unsigned long timeo;

	
	for (i = 0; i < 10; i++) {
		if (ecc_code[i] != 0xff)
			goto compare;
	}
	return 0;

compare:
	if (WARN_ON(0x01 & (unsigned) ecc_code))
		return -EINVAL;
	ecc16 = (unsigned short *)ecc_code;

	ecc10[0] =  (ecc16[0] >>  0) & 0x3ff;
	ecc10[1] = ((ecc16[0] >> 10) & 0x3f) | ((ecc16[1] << 6) & 0x3c0);
	ecc10[2] =  (ecc16[1] >>  4) & 0x3ff;
	ecc10[3] = ((ecc16[1] >> 14) & 0x3)  | ((ecc16[2] << 2) & 0x3fc);
	ecc10[4] =  (ecc16[2] >>  8)         | ((ecc16[3] << 8) & 0x300);
	ecc10[5] =  (ecc16[3] >>  2) & 0x3ff;
	ecc10[6] = ((ecc16[3] >> 12) & 0xf)  | ((ecc16[4] << 4) & 0x3f0);
	ecc10[7] =  (ecc16[4] >>  6) & 0x3ff;

	
	for (i = 7; i >= 0; i--)
		davinci_nand_writel(info, NAND_4BIT_ECC_LOAD_OFFSET, ecc10[i]);

	davinci_nand_readl(info, NANDFSR_OFFSET);
	nand_davinci_readecc_4bit(info, syndrome);
	if (!(syndrome[0] | syndrome[1] | syndrome[2] | syndrome[3]))
		return 0;

	davinci_nand_readl(info, NAND_ERR_ADD1_OFFSET);

	davinci_nand_writel(info, NANDFCR_OFFSET,
			davinci_nand_readl(info, NANDFCR_OFFSET) | BIT(13));

	timeo = jiffies + usecs_to_jiffies(100);
	do {
		ecc_state = (davinci_nand_readl(info,
				NANDFSR_OFFSET) >> 8) & 0x0f;
		cpu_relax();
	} while ((ecc_state < 4) && time_before(jiffies, timeo));

	for (;;) {
		u32	fsr = davinci_nand_readl(info, NANDFSR_OFFSET);

		switch ((fsr >> 8) & 0x0f) {
		case 0:		
			davinci_nand_readl(info, NAND_ERR_ERRVAL1_OFFSET);
			return 0;
		case 1:		
			davinci_nand_readl(info, NAND_ERR_ERRVAL1_OFFSET);
			return -EIO;
		case 2:		
		case 3:
			num_errors = 1 + ((fsr >> 16) & 0x03);
			goto correct;
		default:	
			cpu_relax();
			continue;
		}
	}

correct:
	
	for (i = 0, corrected = 0; i < num_errors; i++) {
		int error_address, error_value;

		if (i > 1) {
			error_address = davinci_nand_readl(info,
						NAND_ERR_ADD2_OFFSET);
			error_value = davinci_nand_readl(info,
						NAND_ERR_ERRVAL2_OFFSET);
		} else {
			error_address = davinci_nand_readl(info,
						NAND_ERR_ADD1_OFFSET);
			error_value = davinci_nand_readl(info,
						NAND_ERR_ERRVAL1_OFFSET);
		}

		if (i & 1) {
			error_address >>= 16;
			error_value >>= 16;
		}
		error_address &= 0x3ff;
		error_address = (512 + 7) - error_address;

		if (error_address < 512) {
			data[error_address] ^= error_value;
			corrected++;
		}
	}

	return corrected;
}


static void nand_davinci_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd->priv;

	if ((0x03 & ((unsigned)buf)) == 0 && (0x03 & len) == 0)
		ioread32_rep(chip->IO_ADDR_R, buf, len >> 2);
	else if ((0x01 & ((unsigned)buf)) == 0 && (0x01 & len) == 0)
		ioread16_rep(chip->IO_ADDR_R, buf, len >> 1);
	else
		ioread8_rep(chip->IO_ADDR_R, buf, len);
}

static void nand_davinci_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd->priv;

	if ((0x03 & ((unsigned)buf)) == 0 && (0x03 & len) == 0)
		iowrite32_rep(chip->IO_ADDR_R, buf, len >> 2);
	else if ((0x01 & ((unsigned)buf)) == 0 && (0x01 & len) == 0)
		iowrite16_rep(chip->IO_ADDR_R, buf, len >> 1);
	else
		iowrite8_rep(chip->IO_ADDR_R, buf, len);
}

static int nand_davinci_dev_ready(struct mtd_info *mtd)
{
	struct davinci_nand_info *info = to_davinci_nand(mtd);

	return davinci_nand_readl(info, NANDFSR_OFFSET) & BIT(0);
}


static struct nand_ecclayout hwecc4_small __initconst = {
	.eccbytes = 10,
	.eccpos = { 0, 1, 2, 3, 4,
		
		6, 7,
		13, 14, 15, },
	.oobfree = {
		{.offset = 8, .length = 5, },
		{.offset = 16, },
	},
};

static struct nand_ecclayout hwecc4_2048 __initconst = {
	.eccbytes = 40,
	.eccpos = {
		
		24, 25, 26, 27, 28, 29,	30, 31, 32, 33,
		34, 35, 36, 37, 38, 39,	40, 41, 42, 43,
		44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
		54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
		},
	.oobfree = {
		
		{.offset = 2, .length = 22, },
		
		
	},
};

static int __init nand_davinci_probe(struct platform_device *pdev)
{
	struct davinci_nand_pdata	*pdata = pdev->dev.platform_data;
	struct davinci_nand_info	*info;
	struct resource			*res1;
	struct resource			*res2;
	void __iomem			*vaddr;
	void __iomem			*base;
	int				ret;
	uint32_t			val;
	nand_ecc_modes_t		ecc_mode;

	
	if (!pdata)
		return -ENODEV;

	
	if (pdev->id < 0 || pdev->id > 3)
		return -ENODEV;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "unable to allocate memory\n");
		ret = -ENOMEM;
		goto err_nomem;
	}

	platform_set_drvdata(pdev, info);

	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res2 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res1 || !res2) {
		dev_err(&pdev->dev, "resource missing\n");
		ret = -EINVAL;
		goto err_nomem;
	}

	vaddr = ioremap(res1->start, resource_size(res1));
	base = ioremap(res2->start, resource_size(res2));
	if (!vaddr || !base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -EINVAL;
		goto err_ioremap;
	}

	info->dev		= &pdev->dev;
	info->base		= base;
	info->vaddr		= vaddr;

	info->mtd.priv		= &info->chip;
	info->mtd.name		= dev_name(&pdev->dev);
	info->mtd.owner		= THIS_MODULE;

	info->mtd.dev.parent	= &pdev->dev;

	info->chip.IO_ADDR_R	= vaddr;
	info->chip.IO_ADDR_W	= vaddr;
	info->chip.chip_delay	= 0;
	info->chip.select_chip	= nand_davinci_select_chip;

	
	info->chip.bbt_options	= pdata->bbt_options;
	
	info->chip.options	= pdata->options;
	info->chip.bbt_td	= pdata->bbt_td;
	info->chip.bbt_md	= pdata->bbt_md;
	info->timing		= pdata->timing;

	info->ioaddr		= (uint32_t __force) vaddr;

	info->current_cs	= info->ioaddr;
	info->core_chipsel	= pdev->id;
	info->mask_chipsel	= pdata->mask_chipsel;

	
	info->mask_ale		= pdata->mask_ale ? : MASK_ALE;
	info->mask_cle		= pdata->mask_cle ? : MASK_CLE;

	
	info->chip.cmd_ctrl	= nand_davinci_hwcontrol;
	info->chip.dev_ready	= nand_davinci_dev_ready;

	
	info->chip.read_buf     = nand_davinci_read_buf;
	info->chip.write_buf    = nand_davinci_write_buf;

	
	ecc_mode		= pdata->ecc_mode;

	ret = -EINVAL;
	switch (ecc_mode) {
	case NAND_ECC_NONE:
	case NAND_ECC_SOFT:
		pdata->ecc_bits = 0;
		break;
	case NAND_ECC_HW:
		if (pdata->ecc_bits == 4) {

			
			spin_lock_irq(&davinci_nand_lock);
			if (ecc4_busy)
				ret = -EBUSY;
			else
				ecc4_busy = true;
			spin_unlock_irq(&davinci_nand_lock);

			if (ret == -EBUSY)
				goto err_ecc;

			info->chip.ecc.calculate = nand_davinci_calculate_4bit;
			info->chip.ecc.correct = nand_davinci_correct_4bit;
			info->chip.ecc.hwctl = nand_davinci_hwctl_4bit;
			info->chip.ecc.bytes = 10;
		} else {
			info->chip.ecc.calculate = nand_davinci_calculate_1bit;
			info->chip.ecc.correct = nand_davinci_correct_1bit;
			info->chip.ecc.hwctl = nand_davinci_hwctl_1bit;
			info->chip.ecc.bytes = 3;
		}
		info->chip.ecc.size = 512;
		info->chip.ecc.strength = pdata->ecc_bits;
		break;
	default:
		ret = -EINVAL;
		goto err_ecc;
	}
	info->chip.ecc.mode = ecc_mode;

	info->clk = clk_get(&pdev->dev, "aemif");
	if (IS_ERR(info->clk)) {
		ret = PTR_ERR(info->clk);
		dev_dbg(&pdev->dev, "unable to get AEMIF clock, err %d\n", ret);
		goto err_clk;
	}

	ret = clk_enable(info->clk);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "unable to enable AEMIF clock, err %d\n",
			ret);
		goto err_clk_enable;
	}

	val = davinci_nand_readl(info, A1CR_OFFSET + info->core_chipsel * 4);

	
	val &= ~(ACR_ASIZE_MASK | ACR_EW_MASK | ACR_SS_MASK);
	if (info->chip.options & NAND_BUSWIDTH_16)
		val |= 0x1;

	davinci_nand_writel(info, A1CR_OFFSET + info->core_chipsel * 4, val);

	ret = 0;
	if (info->timing)
		ret = davinci_aemif_setup_timing(info->timing, info->base,
							info->core_chipsel);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "NAND timing values setup fail\n");
		goto err_timing;
	}

	spin_lock_irq(&davinci_nand_lock);

	
	val = davinci_nand_readl(info, NANDFCR_OFFSET);
	val |= BIT(info->core_chipsel);
	davinci_nand_writel(info, NANDFCR_OFFSET, val);

	spin_unlock_irq(&davinci_nand_lock);

	
	ret = nand_scan_ident(&info->mtd, pdata->mask_chipsel ? 2 : 1, NULL);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "no NAND chip(s) found\n");
		goto err_scan;
	}

	if (pdata->ecc_bits == 4) {
		int	chunks = info->mtd.writesize / 512;

		if (!chunks || info->mtd.oobsize < 16) {
			dev_dbg(&pdev->dev, "too small\n");
			ret = -EINVAL;
			goto err_scan;
		}

		if (chunks == 1) {
			info->ecclayout = hwecc4_small;
			info->ecclayout.oobfree[1].length =
				info->mtd.oobsize - 16;
			goto syndrome_done;
		}
		if (chunks == 4) {
			info->ecclayout = hwecc4_2048;
			info->chip.ecc.mode = NAND_ECC_HW_OOB_FIRST;
			goto syndrome_done;
		}

		dev_warn(&pdev->dev, "no 4-bit ECC support yet "
				"for 4KiB-page NAND\n");
		ret = -EIO;
		goto err_scan;

syndrome_done:
		info->chip.ecc.layout = &info->ecclayout;
	}

	ret = nand_scan_tail(&info->mtd);
	if (ret < 0)
		goto err_scan;

	ret = mtd_device_parse_register(&info->mtd, NULL, NULL, pdata->parts,
					pdata->nr_parts);

	if (ret < 0)
		goto err_scan;

	val = davinci_nand_readl(info, NRCSR_OFFSET);
	dev_info(&pdev->dev, "controller rev. %d.%d\n",
	       (val >> 8) & 0xff, val & 0xff);

	return 0;

err_scan:
err_timing:
	clk_disable(info->clk);

err_clk_enable:
	clk_put(info->clk);

	spin_lock_irq(&davinci_nand_lock);
	if (ecc_mode == NAND_ECC_HW_SYNDROME)
		ecc4_busy = false;
	spin_unlock_irq(&davinci_nand_lock);

err_ecc:
err_clk:
err_ioremap:
	if (base)
		iounmap(base);
	if (vaddr)
		iounmap(vaddr);

err_nomem:
	kfree(info);
	return ret;
}

static int __exit nand_davinci_remove(struct platform_device *pdev)
{
	struct davinci_nand_info *info = platform_get_drvdata(pdev);

	spin_lock_irq(&davinci_nand_lock);
	if (info->chip.ecc.mode == NAND_ECC_HW_SYNDROME)
		ecc4_busy = false;
	spin_unlock_irq(&davinci_nand_lock);

	iounmap(info->base);
	iounmap(info->vaddr);

	nand_release(&info->mtd);

	clk_disable(info->clk);
	clk_put(info->clk);

	kfree(info);

	return 0;
}

static struct platform_driver nand_davinci_driver = {
	.remove		= __exit_p(nand_davinci_remove),
	.driver		= {
		.name	= "davinci_nand",
	},
};
MODULE_ALIAS("platform:davinci_nand");

static int __init nand_davinci_init(void)
{
	return platform_driver_probe(&nand_davinci_driver, nand_davinci_probe);
}
module_init(nand_davinci_init);

static void __exit nand_davinci_exit(void)
{
	platform_driver_unregister(&nand_davinci_driver);
}
module_exit(nand_davinci_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("Davinci NAND flash driver");

