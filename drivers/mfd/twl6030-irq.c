/*
 * twl6030-irq.c - TWL6030 irq support
 *
 * Copyright (C) 2005-2009 Texas Instruments, Inc.
 *
 * Modifications to defer interrupt handling to a kernel thread:
 * Copyright (C) 2006 MontaVista Software, Inc.
 *
 * Based on tlv320aic23.c:
 * Copyright (c) by Kai Svahn <kai.svahn@nokia.com>
 *
 * Code cleanup and modifications to IRQ handler.
 * by syed khasim <x0khasim@ti.com>
 *
 * TWL6030 specific code and IRQ handling changes by
 * Jagadeesh Bhaskar Pakaravoor <j-pakaravoor@ti.com>
 * Balaji T K <balajitk@ti.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/i2c/twl.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/irqdomain.h>

#include "twl-core.h"

#define TWL6030_NR_IRQS    20

static int twl6030_interrupt_mapping[24] = {
	PWR_INTR_OFFSET,	
	PWR_INTR_OFFSET,	
	PWR_INTR_OFFSET,	
	RTC_INTR_OFFSET,	
	RTC_INTR_OFFSET,	
	HOTDIE_INTR_OFFSET,	
	SMPSLDO_INTR_OFFSET,	
	SMPSLDO_INTR_OFFSET,	

	SMPSLDO_INTR_OFFSET,	
	BATDETECT_INTR_OFFSET,	
	SIMDETECT_INTR_OFFSET,	
	MMCDETECT_INTR_OFFSET,	
	RSV_INTR_OFFSET,  	
	MADC_INTR_OFFSET,	
	MADC_INTR_OFFSET,	
	GASGAUGE_INTR_OFFSET,	

	USBOTG_INTR_OFFSET,	
	USBOTG_INTR_OFFSET,	
	USBOTG_INTR_OFFSET,	
	USB_PRES_INTR_OFFSET,	
	CHARGER_INTR_OFFSET,	
	CHARGERFAULT_INTR_OFFSET,	
	CHARGERFAULT_INTR_OFFSET,	
	RSV_INTR_OFFSET,	
};

static unsigned twl6030_irq_base;
static int twl_irq;
static bool twl_irq_wake_enabled;

static struct completion irq_event;
static atomic_t twl6030_wakeirqs = ATOMIC_INIT(0);

static int twl6030_irq_pm_notifier(struct notifier_block *notifier,
				   unsigned long pm_event, void *unused)
{
	int chained_wakeups;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		chained_wakeups = atomic_read(&twl6030_wakeirqs);

		if (chained_wakeups && !twl_irq_wake_enabled) {
			if (enable_irq_wake(twl_irq))
				pr_err("twl6030 IRQ wake enable failed\n");
			else
				twl_irq_wake_enabled = true;
		} else if (!chained_wakeups && twl_irq_wake_enabled) {
			disable_irq_wake(twl_irq);
			twl_irq_wake_enabled = false;
		}

		disable_irq(twl_irq);
		break;

	case PM_POST_SUSPEND:
		enable_irq(twl_irq);
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block twl6030_irq_pm_notifier_block = {
	.notifier_call = twl6030_irq_pm_notifier,
};

static int twl6030_irq_thread(void *data)
{
	long irq = (long)data;
	static unsigned i2c_errors;
	static const unsigned max_i2c_errors = 100;
	int ret;

	while (!kthread_should_stop()) {
		int i;
		union {
		u8 bytes[4];
		u32 int_sts;
		} sts;

		
		wait_for_completion_interruptible(&irq_event);

		
		ret = twl_i2c_read(TWL_MODULE_PIH, sts.bytes,
				REG_INT_STS_A, 3);
		if (ret) {
			pr_warning("twl6030: I2C error %d reading PIH ISR\n",
					ret);
			if (++i2c_errors >= max_i2c_errors) {
				printk(KERN_ERR "Maximum I2C error count"
						" exceeded.  Terminating %s.\n",
						__func__);
				break;
			}
			complete(&irq_event);
			continue;
		}



		sts.bytes[3] = 0; 

		if (sts.bytes[2] & 0x10)
			sts.bytes[2] |= 0x08;

		for (i = 0; sts.int_sts; sts.int_sts >>= 1, i++) {
			local_irq_disable();
			if (sts.int_sts & 0x1) {
				int module_irq = twl6030_irq_base +
					twl6030_interrupt_mapping[i];
				generic_handle_irq(module_irq);

			}
		local_irq_enable();
		}

		/*
		 * NOTE:
		 * Simulation confirms that documentation is wrong w.r.t the
		 * interrupt status clear operation. A single *byte* write to
		 * any one of STS_A to STS_C register results in all three
		 * STS registers being reset. Since it does not matter which
		 * value is written, all three registers are cleared on a
		 * single byte write, so we just use 0x0 to clear.
		 */
		ret = twl_i2c_write_u8(TWL_MODULE_PIH, 0x00, REG_INT_STS_A);
		if (ret)
			pr_warning("twl6030: I2C error in clearing PIH ISR\n");

		enable_irq(irq);
	}

	return 0;
}

static irqreturn_t handle_twl6030_pih(int irq, void *devid)
{
	disable_irq_nosync(irq);
	complete(devid);
	return IRQ_HANDLED;
}


static inline void activate_irq(int irq)
{
#ifdef CONFIG_ARM
	set_irq_flags(irq, IRQF_VALID);
#else
	
	irq_set_noprobe(irq);
#endif
}

static int twl6030_irq_set_wake(struct irq_data *d, unsigned int on)
{
	if (on)
		atomic_inc(&twl6030_wakeirqs);
	else
		atomic_dec(&twl6030_wakeirqs);

	return 0;
}

int twl6030_interrupt_unmask(u8 bit_mask, u8 offset)
{
	int ret;
	u8 unmask_value;
	ret = twl_i2c_read_u8(TWL_MODULE_PIH, &unmask_value,
			REG_INT_STS_A + offset);
	unmask_value &= (~(bit_mask));
	ret |= twl_i2c_write_u8(TWL_MODULE_PIH, unmask_value,
			REG_INT_STS_A + offset); 
	return ret;
}
EXPORT_SYMBOL(twl6030_interrupt_unmask);

int twl6030_interrupt_mask(u8 bit_mask, u8 offset)
{
	int ret;
	u8 mask_value;
	ret = twl_i2c_read_u8(TWL_MODULE_PIH, &mask_value,
			REG_INT_STS_A + offset);
	mask_value |= (bit_mask);
	ret |= twl_i2c_write_u8(TWL_MODULE_PIH, mask_value,
			REG_INT_STS_A + offset); 
	return ret;
}
EXPORT_SYMBOL(twl6030_interrupt_mask);

int twl6030_mmc_card_detect_config(void)
{
	int ret;
	u8 reg_val = 0;

	
	twl6030_interrupt_unmask(TWL6030_MMCDETECT_INT_MASK,
						REG_INT_MSK_LINE_B);
	twl6030_interrupt_unmask(TWL6030_MMCDETECT_INT_MASK,
						REG_INT_MSK_STS_B);
	ret = twl_i2c_read_u8(TWL6030_MODULE_ID0, &reg_val, TWL6030_MMCCTRL);
	if (ret < 0) {
		pr_err("twl6030: Failed to read MMCCTRL, error %d\n", ret);
		return ret;
	}
	reg_val &= ~VMMC_AUTO_OFF;
	reg_val |= SW_FC;
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0, reg_val, TWL6030_MMCCTRL);
	if (ret < 0) {
		pr_err("twl6030: Failed to write MMCCTRL, error %d\n", ret);
		return ret;
	}

	
	ret = twl_i2c_read_u8(TWL6030_MODULE_ID0, &reg_val,
						TWL6030_CFG_INPUT_PUPD3);
	if (ret < 0) {
		pr_err("twl6030: Failed to read CFG_INPUT_PUPD3, error %d\n",
									ret);
		return ret;
	}
	reg_val &= ~(MMC_PU | MMC_PD);
	ret = twl_i2c_write_u8(TWL6030_MODULE_ID0, reg_val,
						TWL6030_CFG_INPUT_PUPD3);
	if (ret < 0) {
		pr_err("twl6030: Failed to write CFG_INPUT_PUPD3, error %d\n",
									ret);
		return ret;
	}

	return twl6030_irq_base + MMCDETECT_INTR_OFFSET;
}
EXPORT_SYMBOL(twl6030_mmc_card_detect_config);

int twl6030_mmc_card_detect(struct device *dev, int slot)
{
	int ret = -EIO;
	u8 read_reg = 0;
	struct platform_device *pdev = to_platform_device(dev);

	if (pdev->id) {
		pr_err("Unknown MMC controller %d in %s\n", pdev->id, __func__);
		return ret;
	}
	ret = twl_i2c_read_u8(TWL6030_MODULE_ID0, &read_reg,
						TWL6030_MMCCTRL);
	if (ret >= 0)
		ret = read_reg & STS_MMC;
	return ret;
}
EXPORT_SYMBOL(twl6030_mmc_card_detect);

int twl6030_init_irq(struct device *dev, int irq_num)
{
	struct			device_node *node = dev->of_node;
	int			nr_irqs, irq_base, irq_end;
	struct task_struct	*task;
	static struct irq_chip  twl6030_irq_chip;
	int			status = 0;
	int			i;
	u8			mask[4];

	nr_irqs = TWL6030_NR_IRQS;

	irq_base = irq_alloc_descs(-1, 0, nr_irqs, 0);
	if (IS_ERR_VALUE(irq_base)) {
		dev_err(dev, "Fail to allocate IRQ descs\n");
		return irq_base;
	}

	irq_domain_add_legacy(node, nr_irqs, irq_base, 0,
			      &irq_domain_simple_ops, NULL);

	irq_end = irq_base + nr_irqs;

	mask[1] = 0xFF;
	mask[2] = 0xFF;
	mask[3] = 0xFF;

	
	twl_i2c_write(TWL_MODULE_PIH, &mask[0], REG_INT_MSK_LINE_A, 3);
	
	twl_i2c_write(TWL_MODULE_PIH, &mask[0], REG_INT_MSK_STS_A, 3);
	
	twl_i2c_write(TWL_MODULE_PIH, &mask[0], REG_INT_STS_A, 3);

	twl6030_irq_base = irq_base;

	twl6030_irq_chip = dummy_irq_chip;
	twl6030_irq_chip.name = "twl6030";
	twl6030_irq_chip.irq_set_type = NULL;
	twl6030_irq_chip.irq_set_wake = twl6030_irq_set_wake;

	for (i = irq_base; i < irq_end; i++) {
		irq_set_chip_and_handler(i, &twl6030_irq_chip,
					 handle_simple_irq);
		irq_set_chip_data(i, (void *)irq_num);
		activate_irq(i);
	}

	dev_info(dev, "PIH (irq %d) chaining IRQs %d..%d\n",
			irq_num, irq_base, irq_end);

	
	init_completion(&irq_event);

	status = request_irq(irq_num, handle_twl6030_pih, 0, "TWL6030-PIH",
			     &irq_event);
	if (status < 0) {
		dev_err(dev, "could not claim irq %d: %d\n", irq_num, status);
		goto fail_irq;
	}

	task = kthread_run(twl6030_irq_thread, (void *)irq_num, "twl6030-irq");
	if (IS_ERR(task)) {
		dev_err(dev, "could not create irq %d thread!\n", irq_num);
		status = PTR_ERR(task);
		goto fail_kthread;
	}

	twl_irq = irq_num;
	register_pm_notifier(&twl6030_irq_pm_notifier_block);
	return irq_base;

fail_kthread:
	free_irq(irq_num, &irq_event);

fail_irq:
	for (i = irq_base; i < irq_end; i++)
		irq_set_chip_and_handler(i, NULL, NULL);

	return status;
}

int twl6030_exit_irq(void)
{
	unregister_pm_notifier(&twl6030_irq_pm_notifier_block);

	if (twl6030_irq_base) {
		pr_err("twl6030: can't yet clean up IRQs?\n");
		return -ENOSYS;
	}
	return 0;
}

