/*
 * Freescale LBC and UPM routines.
 *
 * Copyright © 2007-2008  MontaVista Software, Inc.
 * Copyright © 2010 Freescale Semiconductor
 *
 * Author: Anton Vorontsov <avorontsov@ru.mvista.com>
 * Author: Jack Lan <Jack.Lan@freescale.com>
 * Author: Roy Zang <tie-fei.zang@freescale.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <asm/prom.h>
#include <asm/fsl_lbc.h>

static spinlock_t fsl_lbc_lock = __SPIN_LOCK_UNLOCKED(fsl_lbc_lock);
struct fsl_lbc_ctrl *fsl_lbc_ctrl_dev;
EXPORT_SYMBOL(fsl_lbc_ctrl_dev);

u32 fsl_lbc_addr(phys_addr_t addr_base)
{
	struct device_node *np = fsl_lbc_ctrl_dev->dev->of_node;
	u32 addr = addr_base & 0xffff8000;

	if (of_device_is_compatible(np, "fsl,elbc"))
		return addr;

	return addr | ((addr_base & 0x300000000ull) >> 19);
}
EXPORT_SYMBOL(fsl_lbc_addr);

int fsl_lbc_find(phys_addr_t addr_base)
{
	int i;
	struct fsl_lbc_regs __iomem *lbc;

	if (!fsl_lbc_ctrl_dev || !fsl_lbc_ctrl_dev->regs)
		return -ENODEV;

	lbc = fsl_lbc_ctrl_dev->regs;
	for (i = 0; i < ARRAY_SIZE(lbc->bank); i++) {
		__be32 br = in_be32(&lbc->bank[i].br);
		__be32 or = in_be32(&lbc->bank[i].or);

		if (br & BR_V && (br & or & BR_BA) == fsl_lbc_addr(addr_base))
			return i;
	}

	return -ENOENT;
}
EXPORT_SYMBOL(fsl_lbc_find);

int fsl_upm_find(phys_addr_t addr_base, struct fsl_upm *upm)
{
	int bank;
	__be32 br;
	struct fsl_lbc_regs __iomem *lbc;

	bank = fsl_lbc_find(addr_base);
	if (bank < 0)
		return bank;

	if (!fsl_lbc_ctrl_dev || !fsl_lbc_ctrl_dev->regs)
		return -ENODEV;

	lbc = fsl_lbc_ctrl_dev->regs;
	br = in_be32(&lbc->bank[bank].br);

	switch (br & BR_MSEL) {
	case BR_MS_UPMA:
		upm->mxmr = &lbc->mamr;
		break;
	case BR_MS_UPMB:
		upm->mxmr = &lbc->mbmr;
		break;
	case BR_MS_UPMC:
		upm->mxmr = &lbc->mcmr;
		break;
	default:
		return -EINVAL;
	}

	switch (br & BR_PS) {
	case BR_PS_8:
		upm->width = 8;
		break;
	case BR_PS_16:
		upm->width = 16;
		break;
	case BR_PS_32:
		upm->width = 32;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(fsl_upm_find);

int fsl_upm_run_pattern(struct fsl_upm *upm, void __iomem *io_base, u32 mar)
{
	int ret = 0;
	unsigned long flags;

	if (!fsl_lbc_ctrl_dev || !fsl_lbc_ctrl_dev->regs)
		return -ENODEV;

	spin_lock_irqsave(&fsl_lbc_lock, flags);

	out_be32(&fsl_lbc_ctrl_dev->regs->mar, mar);

	switch (upm->width) {
	case 8:
		out_8(io_base, 0x0);
		break;
	case 16:
		out_be16(io_base, 0x0);
		break;
	case 32:
		out_be32(io_base, 0x0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&fsl_lbc_lock, flags);

	return ret;
}
EXPORT_SYMBOL(fsl_upm_run_pattern);

static int __devinit fsl_lbc_ctrl_init(struct fsl_lbc_ctrl *ctrl,
				       struct device_node *node)
{
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;

	
	setbits32(&lbc->ltesr, LTESR_CLEAR);
	out_be32(&lbc->lteatr, 0);
	out_be32(&lbc->ltear, 0);
	out_be32(&lbc->lteccr, LTECCR_CLEAR);
	out_be32(&lbc->ltedr, LTEDR_ENABLE);

	
	if (of_device_is_compatible(node, "fsl,elbc"))
		clrsetbits_be32(&lbc->lbcr, LBCR_BMT, LBCR_BMTPS);

	return 0;
}


static irqreturn_t fsl_lbc_ctrl_irq(int irqno, void *data)
{
	struct fsl_lbc_ctrl *ctrl = data;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;
	u32 status;

	status = in_be32(&lbc->ltesr);
	if (!status)
		return IRQ_NONE;

	out_be32(&lbc->ltesr, LTESR_CLEAR);
	out_be32(&lbc->lteatr, 0);
	out_be32(&lbc->ltear, 0);
	ctrl->irq_status = status;

	if (status & LTESR_BM)
		dev_err(ctrl->dev, "Local bus monitor time-out: "
			"LTESR 0x%08X\n", status);
	if (status & LTESR_WP)
		dev_err(ctrl->dev, "Write protect error: "
			"LTESR 0x%08X\n", status);
	if (status & LTESR_ATMW)
		dev_err(ctrl->dev, "Atomic write error: "
			"LTESR 0x%08X\n", status);
	if (status & LTESR_ATMR)
		dev_err(ctrl->dev, "Atomic read error: "
			"LTESR 0x%08X\n", status);
	if (status & LTESR_CS)
		dev_err(ctrl->dev, "Chip select error: "
			"LTESR 0x%08X\n", status);
	if (status & LTESR_UPM)
		;
	if (status & LTESR_FCT) {
		dev_err(ctrl->dev, "FCM command time-out: "
			"LTESR 0x%08X\n", status);
		smp_wmb();
		wake_up(&ctrl->irq_wait);
	}
	if (status & LTESR_PAR) {
		dev_err(ctrl->dev, "Parity or Uncorrectable ECC error: "
			"LTESR 0x%08X\n", status);
		smp_wmb();
		wake_up(&ctrl->irq_wait);
	}
	if (status & LTESR_CC) {
		smp_wmb();
		wake_up(&ctrl->irq_wait);
	}
	if (status & ~LTESR_MASK)
		dev_err(ctrl->dev, "Unknown error: "
			"LTESR 0x%08X\n", status);
	return IRQ_HANDLED;
}


static int __devinit fsl_lbc_ctrl_probe(struct platform_device *dev)
{
	int ret;

	if (!dev->dev.of_node) {
		dev_err(&dev->dev, "Device OF-Node is NULL");
		return -EFAULT;
	}

	fsl_lbc_ctrl_dev = kzalloc(sizeof(*fsl_lbc_ctrl_dev), GFP_KERNEL);
	if (!fsl_lbc_ctrl_dev)
		return -ENOMEM;

	dev_set_drvdata(&dev->dev, fsl_lbc_ctrl_dev);

	spin_lock_init(&fsl_lbc_ctrl_dev->lock);
	init_waitqueue_head(&fsl_lbc_ctrl_dev->irq_wait);

	fsl_lbc_ctrl_dev->regs = of_iomap(dev->dev.of_node, 0);
	if (!fsl_lbc_ctrl_dev->regs) {
		dev_err(&dev->dev, "failed to get memory region\n");
		ret = -ENODEV;
		goto err;
	}

	fsl_lbc_ctrl_dev->irq = irq_of_parse_and_map(dev->dev.of_node, 0);
	if (fsl_lbc_ctrl_dev->irq == NO_IRQ) {
		dev_err(&dev->dev, "failed to get irq resource\n");
		ret = -ENODEV;
		goto err;
	}

	fsl_lbc_ctrl_dev->dev = &dev->dev;

	ret = fsl_lbc_ctrl_init(fsl_lbc_ctrl_dev, dev->dev.of_node);
	if (ret < 0)
		goto err;

	ret = request_irq(fsl_lbc_ctrl_dev->irq, fsl_lbc_ctrl_irq, 0,
				"fsl-lbc", fsl_lbc_ctrl_dev);
	if (ret != 0) {
		dev_err(&dev->dev, "failed to install irq (%d)\n",
			fsl_lbc_ctrl_dev->irq);
		ret = fsl_lbc_ctrl_dev->irq;
		goto err;
	}

	
	out_be32(&fsl_lbc_ctrl_dev->regs->lteir, LTEIR_ENABLE);

	return 0;

err:
	iounmap(fsl_lbc_ctrl_dev->regs);
	kfree(fsl_lbc_ctrl_dev);
	fsl_lbc_ctrl_dev = NULL;
	return ret;
}

#ifdef CONFIG_SUSPEND

static int fsl_lbc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct fsl_lbc_ctrl *ctrl = dev_get_drvdata(&pdev->dev);
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;

	ctrl->saved_regs = kmalloc(sizeof(struct fsl_lbc_regs), GFP_KERNEL);
	if (!ctrl->saved_regs)
		return -ENOMEM;

	_memcpy_fromio(ctrl->saved_regs, lbc, sizeof(struct fsl_lbc_regs));
	return 0;
}

static int fsl_lbc_resume(struct platform_device *pdev)
{
	struct fsl_lbc_ctrl *ctrl = dev_get_drvdata(&pdev->dev);
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;

	if (ctrl->saved_regs) {
		_memcpy_toio(lbc, ctrl->saved_regs,
				sizeof(struct fsl_lbc_regs));
		kfree(ctrl->saved_regs);
		ctrl->saved_regs = NULL;
	}
	return 0;
}
#endif 

static const struct of_device_id fsl_lbc_match[] = {
	{ .compatible = "fsl,elbc", },
	{ .compatible = "fsl,pq3-localbus", },
	{ .compatible = "fsl,pq2-localbus", },
	{ .compatible = "fsl,pq2pro-localbus", },
	{},
};

static struct platform_driver fsl_lbc_ctrl_driver = {
	.driver = {
		.name = "fsl-lbc",
		.of_match_table = fsl_lbc_match,
	},
	.probe = fsl_lbc_ctrl_probe,
#ifdef CONFIG_SUSPEND
	.suspend     = fsl_lbc_suspend,
	.resume      = fsl_lbc_resume,
#endif
};

static int __init fsl_lbc_init(void)
{
	return platform_driver_register(&fsl_lbc_ctrl_driver);
}
module_init(fsl_lbc_init);
