/*
 *  linux/drivers/mfd/ucb1x00-core.c
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 *  The UCB1x00 core driver provides basic services for handling IO,
 *  the ADC, interrupts, and accessing registers.  It is designed
 *  such that everything goes through this layer, thereby providing
 *  a consistent locking methodology, as well as allowing the drivers
 *  to be used on other non-MCP-enabled hardware platforms.
 *
 *  Note that all locks are private to this file.  Nothing else may
 *  touch them.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/mfd/ucb1x00.h>
#include <linux/pm.h>
#include <linux/gpio.h>

static DEFINE_MUTEX(ucb1x00_mutex);
static LIST_HEAD(ucb1x00_drivers);
static LIST_HEAD(ucb1x00_devices);

void ucb1x00_io_set_dir(struct ucb1x00 *ucb, unsigned int in, unsigned int out)
{
	unsigned long flags;

	spin_lock_irqsave(&ucb->io_lock, flags);
	ucb->io_dir |= out;
	ucb->io_dir &= ~in;

	ucb1x00_reg_write(ucb, UCB_IO_DIR, ucb->io_dir);
	spin_unlock_irqrestore(&ucb->io_lock, flags);
}

void ucb1x00_io_write(struct ucb1x00 *ucb, unsigned int set, unsigned int clear)
{
	unsigned long flags;

	spin_lock_irqsave(&ucb->io_lock, flags);
	ucb->io_out |= set;
	ucb->io_out &= ~clear;

	ucb1x00_reg_write(ucb, UCB_IO_DATA, ucb->io_out);
	spin_unlock_irqrestore(&ucb->io_lock, flags);
}

unsigned int ucb1x00_io_read(struct ucb1x00 *ucb)
{
	return ucb1x00_reg_read(ucb, UCB_IO_DATA);
}

static void ucb1x00_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct ucb1x00 *ucb = container_of(chip, struct ucb1x00, gpio);
	unsigned long flags;

	spin_lock_irqsave(&ucb->io_lock, flags);
	if (value)
		ucb->io_out |= 1 << offset;
	else
		ucb->io_out &= ~(1 << offset);

	ucb1x00_enable(ucb);
	ucb1x00_reg_write(ucb, UCB_IO_DATA, ucb->io_out);
	ucb1x00_disable(ucb);
	spin_unlock_irqrestore(&ucb->io_lock, flags);
}

static int ucb1x00_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct ucb1x00 *ucb = container_of(chip, struct ucb1x00, gpio);
	unsigned val;

	ucb1x00_enable(ucb);
	val = ucb1x00_reg_read(ucb, UCB_IO_DATA);
	ucb1x00_disable(ucb);

	return val & (1 << offset);
}

static int ucb1x00_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct ucb1x00 *ucb = container_of(chip, struct ucb1x00, gpio);
	unsigned long flags;

	spin_lock_irqsave(&ucb->io_lock, flags);
	ucb->io_dir &= ~(1 << offset);
	ucb1x00_enable(ucb);
	ucb1x00_reg_write(ucb, UCB_IO_DIR, ucb->io_dir);
	ucb1x00_disable(ucb);
	spin_unlock_irqrestore(&ucb->io_lock, flags);

	return 0;
}

static int ucb1x00_gpio_direction_output(struct gpio_chip *chip, unsigned offset
		, int value)
{
	struct ucb1x00 *ucb = container_of(chip, struct ucb1x00, gpio);
	unsigned long flags;
	unsigned old, mask = 1 << offset;

	spin_lock_irqsave(&ucb->io_lock, flags);
	old = ucb->io_out;
	if (value)
		ucb->io_out |= mask;
	else
		ucb->io_out &= ~mask;

	ucb1x00_enable(ucb);
	if (old != ucb->io_out)
		ucb1x00_reg_write(ucb, UCB_IO_DATA, ucb->io_out);

	if (!(ucb->io_dir & mask)) {
		ucb->io_dir |= mask;
		ucb1x00_reg_write(ucb, UCB_IO_DIR, ucb->io_dir);
	}
	ucb1x00_disable(ucb);
	spin_unlock_irqrestore(&ucb->io_lock, flags);

	return 0;
}

static int ucb1x00_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct ucb1x00 *ucb = container_of(chip, struct ucb1x00, gpio);

	return ucb->irq_base > 0 ? ucb->irq_base + offset : -ENXIO;
}


void ucb1x00_adc_enable(struct ucb1x00 *ucb)
{
	mutex_lock(&ucb->adc_mutex);

	ucb->adc_cr |= UCB_ADC_ENA;

	ucb1x00_enable(ucb);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr);
}

unsigned int ucb1x00_adc_read(struct ucb1x00 *ucb, int adc_channel, int sync)
{
	unsigned int val;

	if (sync)
		adc_channel |= UCB_ADC_SYNC_ENA;

	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr | adc_channel);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr | adc_channel | UCB_ADC_START);

	for (;;) {
		val = ucb1x00_reg_read(ucb, UCB_ADC_DATA);
		if (val & UCB_ADC_DAT_VAL)
			break;
		
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
	}

	return UCB_ADC_DAT(val);
}

void ucb1x00_adc_disable(struct ucb1x00 *ucb)
{
	ucb->adc_cr &= ~UCB_ADC_ENA;
	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr);
	ucb1x00_disable(ucb);

	mutex_unlock(&ucb->adc_mutex);
}

static void ucb1x00_irq(unsigned int irq, struct irq_desc *desc)
{
	struct ucb1x00 *ucb = irq_desc_get_handler_data(desc);
	unsigned int isr, i;

	ucb1x00_enable(ucb);
	isr = ucb1x00_reg_read(ucb, UCB_IE_STATUS);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, isr);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0);

	for (i = 0; i < 16 && isr; i++, isr >>= 1, irq++)
		if (isr & 1)
			generic_handle_irq(ucb->irq_base + i);
	ucb1x00_disable(ucb);
}

static void ucb1x00_irq_update(struct ucb1x00 *ucb, unsigned mask)
{
	ucb1x00_enable(ucb);
	if (ucb->irq_ris_enbl & mask)
		ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl &
				  ucb->irq_mask);
	if (ucb->irq_fal_enbl & mask)
		ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl &
				  ucb->irq_mask);
	ucb1x00_disable(ucb);
}

static void ucb1x00_irq_noop(struct irq_data *data)
{
}

static void ucb1x00_irq_mask(struct irq_data *data)
{
	struct ucb1x00 *ucb = irq_data_get_irq_chip_data(data);
	unsigned mask = 1 << (data->irq - ucb->irq_base);

	raw_spin_lock(&ucb->irq_lock);
	ucb->irq_mask &= ~mask;
	ucb1x00_irq_update(ucb, mask);
	raw_spin_unlock(&ucb->irq_lock);
}

static void ucb1x00_irq_unmask(struct irq_data *data)
{
	struct ucb1x00 *ucb = irq_data_get_irq_chip_data(data);
	unsigned mask = 1 << (data->irq - ucb->irq_base);

	raw_spin_lock(&ucb->irq_lock);
	ucb->irq_mask |= mask;
	ucb1x00_irq_update(ucb, mask);
	raw_spin_unlock(&ucb->irq_lock);
}

static int ucb1x00_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct ucb1x00 *ucb = irq_data_get_irq_chip_data(data);
	unsigned mask = 1 << (data->irq - ucb->irq_base);

	raw_spin_lock(&ucb->irq_lock);
	if (type & IRQ_TYPE_EDGE_RISING)
		ucb->irq_ris_enbl |= mask;
	else
		ucb->irq_ris_enbl &= ~mask;

	if (type & IRQ_TYPE_EDGE_FALLING)
		ucb->irq_fal_enbl |= mask;
	else
		ucb->irq_fal_enbl &= ~mask;
	if (ucb->irq_mask & mask) {
		ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl &
				  ucb->irq_mask);
		ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl &
				  ucb->irq_mask);
	}
	raw_spin_unlock(&ucb->irq_lock);

	return 0;
}

static int ucb1x00_irq_set_wake(struct irq_data *data, unsigned int on)
{
	struct ucb1x00 *ucb = irq_data_get_irq_chip_data(data);
	struct ucb1x00_plat_data *pdata = ucb->mcp->attached_device.platform_data;
	unsigned mask = 1 << (data->irq - ucb->irq_base);

	if (!pdata || !pdata->can_wakeup)
		return -EINVAL;

	raw_spin_lock(&ucb->irq_lock);
	if (on)
		ucb->irq_wake |= mask;
	else
		ucb->irq_wake &= ~mask;
	raw_spin_unlock(&ucb->irq_lock);

	return 0;
}

static struct irq_chip ucb1x00_irqchip = {
	.name = "ucb1x00",
	.irq_ack = ucb1x00_irq_noop,
	.irq_mask = ucb1x00_irq_mask,
	.irq_unmask = ucb1x00_irq_unmask,
	.irq_set_type = ucb1x00_irq_set_type,
	.irq_set_wake = ucb1x00_irq_set_wake,
};

static int ucb1x00_add_dev(struct ucb1x00 *ucb, struct ucb1x00_driver *drv)
{
	struct ucb1x00_dev *dev;
	int ret = -ENOMEM;

	dev = kmalloc(sizeof(struct ucb1x00_dev), GFP_KERNEL);
	if (dev) {
		dev->ucb = ucb;
		dev->drv = drv;

		ret = drv->add(dev);

		if (ret == 0) {
			list_add_tail(&dev->dev_node, &ucb->devs);
			list_add_tail(&dev->drv_node, &drv->devs);
		} else {
			kfree(dev);
		}
	}
	return ret;
}

static void ucb1x00_remove_dev(struct ucb1x00_dev *dev)
{
	dev->drv->remove(dev);
	list_del(&dev->dev_node);
	list_del(&dev->drv_node);
	kfree(dev);
}

static int ucb1x00_detect_irq(struct ucb1x00 *ucb)
{
	unsigned long mask;

	mask = probe_irq_on();
	if (!mask) {
		probe_irq_off(mask);
		return NO_IRQ;
	}

	ucb1x00_reg_write(ucb, UCB_IE_RIS, UCB_IE_ADC);
	ucb1x00_reg_write(ucb, UCB_IE_FAL, UCB_IE_ADC);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0xffff);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0);

	ucb1x00_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA | UCB_ADC_START);

	while ((ucb1x00_reg_read(ucb, UCB_ADC_DATA) & UCB_ADC_DAT_VAL) == 0);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, 0);

	ucb1x00_reg_write(ucb, UCB_IE_RIS, 0);
	ucb1x00_reg_write(ucb, UCB_IE_FAL, 0);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0xffff);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0);

	return probe_irq_off(mask);
}

static void ucb1x00_release(struct device *dev)
{
	struct ucb1x00 *ucb = classdev_to_ucb1x00(dev);
	kfree(ucb);
}

static struct class ucb1x00_class = {
	.name		= "ucb1x00",
	.dev_release	= ucb1x00_release,
};

static int ucb1x00_probe(struct mcp *mcp)
{
	struct ucb1x00_plat_data *pdata = mcp->attached_device.platform_data;
	struct ucb1x00_driver *drv;
	struct ucb1x00 *ucb;
	unsigned id, i, irq_base;
	int ret = -ENODEV;

	
	if (pdata && pdata->reset)
		pdata->reset(UCB_RST_PROBE);

	mcp_enable(mcp);
	id = mcp_reg_read(mcp, UCB_ID);
	mcp_disable(mcp);

	if (id != UCB_ID_1200 && id != UCB_ID_1300 && id != UCB_ID_TC35143) {
		printk(KERN_WARNING "UCB1x00 ID not found: %04x\n", id);
		goto out;
	}

	ucb = kzalloc(sizeof(struct ucb1x00), GFP_KERNEL);
	ret = -ENOMEM;
	if (!ucb)
		goto out;

	device_initialize(&ucb->dev);
	ucb->dev.class = &ucb1x00_class;
	ucb->dev.parent = &mcp->attached_device;
	dev_set_name(&ucb->dev, "ucb1x00");

	raw_spin_lock_init(&ucb->irq_lock);
	spin_lock_init(&ucb->io_lock);
	mutex_init(&ucb->adc_mutex);

	ucb->id  = id;
	ucb->mcp = mcp;

	ret = device_add(&ucb->dev);
	if (ret)
		goto err_dev_add;

	ucb1x00_enable(ucb);
	ucb->irq = ucb1x00_detect_irq(ucb);
	ucb1x00_disable(ucb);
	if (ucb->irq == NO_IRQ) {
		dev_err(&ucb->dev, "IRQ probe failed\n");
		ret = -ENODEV;
		goto err_no_irq;
	}

	ucb->gpio.base = -1;
	irq_base = pdata ? pdata->irq_base : 0;
	ucb->irq_base = irq_alloc_descs(-1, irq_base, 16, -1);
	if (ucb->irq_base < 0) {
		dev_err(&ucb->dev, "unable to allocate 16 irqs: %d\n",
			ucb->irq_base);
		goto err_irq_alloc;
	}

	for (i = 0; i < 16; i++) {
		unsigned irq = ucb->irq_base + i;

		irq_set_chip_and_handler(irq, &ucb1x00_irqchip, handle_edge_irq);
		irq_set_chip_data(irq, ucb);
		set_irq_flags(irq, IRQF_VALID | IRQ_NOREQUEST);
	}

	irq_set_irq_type(ucb->irq, IRQ_TYPE_EDGE_RISING);
	irq_set_handler_data(ucb->irq, ucb);
	irq_set_chained_handler(ucb->irq, ucb1x00_irq);

	if (pdata && pdata->gpio_base) {
		ucb->gpio.label = dev_name(&ucb->dev);
		ucb->gpio.dev = &ucb->dev;
		ucb->gpio.owner = THIS_MODULE;
		ucb->gpio.base = pdata->gpio_base;
		ucb->gpio.ngpio = 10;
		ucb->gpio.set = ucb1x00_gpio_set;
		ucb->gpio.get = ucb1x00_gpio_get;
		ucb->gpio.direction_input = ucb1x00_gpio_direction_input;
		ucb->gpio.direction_output = ucb1x00_gpio_direction_output;
		ucb->gpio.to_irq = ucb1x00_to_irq;
		ret = gpiochip_add(&ucb->gpio);
		if (ret)
			goto err_gpio_add;
	} else
		dev_info(&ucb->dev, "gpio_base not set so no gpiolib support");

	mcp_set_drvdata(mcp, ucb);

	if (pdata)
		device_set_wakeup_capable(&ucb->dev, pdata->can_wakeup);

	INIT_LIST_HEAD(&ucb->devs);
	mutex_lock(&ucb1x00_mutex);
	list_add_tail(&ucb->node, &ucb1x00_devices);
	list_for_each_entry(drv, &ucb1x00_drivers, node) {
		ucb1x00_add_dev(ucb, drv);
	}
	mutex_unlock(&ucb1x00_mutex);

	return ret;

 err_gpio_add:
	irq_set_chained_handler(ucb->irq, NULL);
 err_irq_alloc:
	if (ucb->irq_base > 0)
		irq_free_descs(ucb->irq_base, 16);
 err_no_irq:
	device_del(&ucb->dev);
 err_dev_add:
	put_device(&ucb->dev);
 out:
	if (pdata && pdata->reset)
		pdata->reset(UCB_RST_PROBE_FAIL);
	return ret;
}

static void ucb1x00_remove(struct mcp *mcp)
{
	struct ucb1x00_plat_data *pdata = mcp->attached_device.platform_data;
	struct ucb1x00 *ucb = mcp_get_drvdata(mcp);
	struct list_head *l, *n;
	int ret;

	mutex_lock(&ucb1x00_mutex);
	list_del(&ucb->node);
	list_for_each_safe(l, n, &ucb->devs) {
		struct ucb1x00_dev *dev = list_entry(l, struct ucb1x00_dev, dev_node);
		ucb1x00_remove_dev(dev);
	}
	mutex_unlock(&ucb1x00_mutex);

	if (ucb->gpio.base != -1) {
		ret = gpiochip_remove(&ucb->gpio);
		if (ret)
			dev_err(&ucb->dev, "Can't remove gpio chip: %d\n", ret);
	}

	irq_set_chained_handler(ucb->irq, NULL);
	irq_free_descs(ucb->irq_base, 16);
	device_unregister(&ucb->dev);

	if (pdata && pdata->reset)
		pdata->reset(UCB_RST_REMOVE);
}

int ucb1x00_register_driver(struct ucb1x00_driver *drv)
{
	struct ucb1x00 *ucb;

	INIT_LIST_HEAD(&drv->devs);
	mutex_lock(&ucb1x00_mutex);
	list_add_tail(&drv->node, &ucb1x00_drivers);
	list_for_each_entry(ucb, &ucb1x00_devices, node) {
		ucb1x00_add_dev(ucb, drv);
	}
	mutex_unlock(&ucb1x00_mutex);
	return 0;
}

void ucb1x00_unregister_driver(struct ucb1x00_driver *drv)
{
	struct list_head *n, *l;

	mutex_lock(&ucb1x00_mutex);
	list_del(&drv->node);
	list_for_each_safe(l, n, &drv->devs) {
		struct ucb1x00_dev *dev = list_entry(l, struct ucb1x00_dev, drv_node);
		ucb1x00_remove_dev(dev);
	}
	mutex_unlock(&ucb1x00_mutex);
}

static int ucb1x00_suspend(struct device *dev)
{
	struct ucb1x00_plat_data *pdata = dev->platform_data;
	struct ucb1x00 *ucb = dev_get_drvdata(dev);
	struct ucb1x00_dev *udev;

	mutex_lock(&ucb1x00_mutex);
	list_for_each_entry(udev, &ucb->devs, dev_node) {
		if (udev->drv->suspend)
			udev->drv->suspend(udev);
	}
	mutex_unlock(&ucb1x00_mutex);

	if (ucb->irq_wake) {
		unsigned long flags;

		raw_spin_lock_irqsave(&ucb->irq_lock, flags);
		ucb1x00_enable(ucb);
		ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl &
				  ucb->irq_wake);
		ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl &
				  ucb->irq_wake);
		ucb1x00_disable(ucb);
		raw_spin_unlock_irqrestore(&ucb->irq_lock, flags);

		enable_irq_wake(ucb->irq);
	} else if (pdata && pdata->reset)
		pdata->reset(UCB_RST_SUSPEND);

	return 0;
}

static int ucb1x00_resume(struct device *dev)
{
	struct ucb1x00_plat_data *pdata = dev->platform_data;
	struct ucb1x00 *ucb = dev_get_drvdata(dev);
	struct ucb1x00_dev *udev;

	if (!ucb->irq_wake && pdata && pdata->reset)
		pdata->reset(UCB_RST_RESUME);

	ucb1x00_enable(ucb);
	ucb1x00_reg_write(ucb, UCB_IO_DATA, ucb->io_out);
	ucb1x00_reg_write(ucb, UCB_IO_DIR, ucb->io_dir);

	if (ucb->irq_wake) {
		unsigned long flags;

		raw_spin_lock_irqsave(&ucb->irq_lock, flags);
		ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl &
				  ucb->irq_mask);
		ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl &
				  ucb->irq_mask);
		raw_spin_unlock_irqrestore(&ucb->irq_lock, flags);

		disable_irq_wake(ucb->irq);
	}
	ucb1x00_disable(ucb);

	mutex_lock(&ucb1x00_mutex);
	list_for_each_entry(udev, &ucb->devs, dev_node) {
		if (udev->drv->resume)
			udev->drv->resume(udev);
	}
	mutex_unlock(&ucb1x00_mutex);
	return 0;
}

static const struct dev_pm_ops ucb1x00_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ucb1x00_suspend, ucb1x00_resume)
};

static struct mcp_driver ucb1x00_driver = {
	.drv		= {
		.name	= "ucb1x00",
		.owner	= THIS_MODULE,
		.pm	= &ucb1x00_pm_ops,
	},
	.probe		= ucb1x00_probe,
	.remove		= ucb1x00_remove,
};

static int __init ucb1x00_init(void)
{
	int ret = class_register(&ucb1x00_class);
	if (ret == 0) {
		ret = mcp_driver_register(&ucb1x00_driver);
		if (ret)
			class_unregister(&ucb1x00_class);
	}
	return ret;
}

static void __exit ucb1x00_exit(void)
{
	mcp_driver_unregister(&ucb1x00_driver);
	class_unregister(&ucb1x00_class);
}

module_init(ucb1x00_init);
module_exit(ucb1x00_exit);

EXPORT_SYMBOL(ucb1x00_io_set_dir);
EXPORT_SYMBOL(ucb1x00_io_write);
EXPORT_SYMBOL(ucb1x00_io_read);

EXPORT_SYMBOL(ucb1x00_adc_enable);
EXPORT_SYMBOL(ucb1x00_adc_read);
EXPORT_SYMBOL(ucb1x00_adc_disable);

EXPORT_SYMBOL(ucb1x00_register_driver);
EXPORT_SYMBOL(ucb1x00_unregister_driver);

MODULE_ALIAS("mcp:ucb1x00");
MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("UCB1x00 core driver");
MODULE_LICENSE("GPL");
