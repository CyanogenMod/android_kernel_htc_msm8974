/*
 * TI DaVinci GPIO Support
 *
 * Copyright (c) 2006-2007 David Brownell
 * Copyright (c) 2007, MontaVista Software, Inc. <source@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>

#include <asm/mach/irq.h>

struct davinci_gpio_regs {
	u32	dir;
	u32	out_data;
	u32	set_data;
	u32	clr_data;
	u32	in_data;
	u32	set_rising;
	u32	clr_rising;
	u32	set_falling;
	u32	clr_falling;
	u32	intstat;
};

#define chip2controller(chip)	\
	container_of(chip, struct davinci_gpio_controller, chip)

static struct davinci_gpio_controller chips[DIV_ROUND_UP(DAVINCI_N_GPIO, 32)];
static void __iomem *gpio_base;

static struct davinci_gpio_regs __iomem __init *gpio2regs(unsigned gpio)
{
	void __iomem *ptr;

	if (gpio < 32 * 1)
		ptr = gpio_base + 0x10;
	else if (gpio < 32 * 2)
		ptr = gpio_base + 0x38;
	else if (gpio < 32 * 3)
		ptr = gpio_base + 0x60;
	else if (gpio < 32 * 4)
		ptr = gpio_base + 0x88;
	else if (gpio < 32 * 5)
		ptr = gpio_base + 0xb0;
	else
		ptr = NULL;
	return ptr;
}

static inline struct davinci_gpio_regs __iomem *irq2regs(int irq)
{
	struct davinci_gpio_regs __iomem *g;

	g = (__force struct davinci_gpio_regs __iomem *)irq_get_chip_data(irq);

	return g;
}

static int __init davinci_gpio_irq_setup(void);


static inline int __davinci_direction(struct gpio_chip *chip,
			unsigned offset, bool out, int value)
{
	struct davinci_gpio_controller *d = chip2controller(chip);
	struct davinci_gpio_regs __iomem *g = d->regs;
	unsigned long flags;
	u32 temp;
	u32 mask = 1 << offset;

	spin_lock_irqsave(&d->lock, flags);
	temp = __raw_readl(&g->dir);
	if (out) {
		temp &= ~mask;
		__raw_writel(mask, value ? &g->set_data : &g->clr_data);
	} else {
		temp |= mask;
	}
	__raw_writel(temp, &g->dir);
	spin_unlock_irqrestore(&d->lock, flags);

	return 0;
}

static int davinci_direction_in(struct gpio_chip *chip, unsigned offset)
{
	return __davinci_direction(chip, offset, false, 0);
}

static int
davinci_direction_out(struct gpio_chip *chip, unsigned offset, int value)
{
	return __davinci_direction(chip, offset, true, value);
}

static int davinci_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct davinci_gpio_controller *d = chip2controller(chip);
	struct davinci_gpio_regs __iomem *g = d->regs;

	return (1 << offset) & __raw_readl(&g->in_data);
}

static void
davinci_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct davinci_gpio_controller *d = chip2controller(chip);
	struct davinci_gpio_regs __iomem *g = d->regs;

	__raw_writel((1 << offset), value ? &g->set_data : &g->clr_data);
}

static int __init davinci_gpio_setup(void)
{
	int i, base;
	unsigned ngpio;
	struct davinci_soc_info *soc_info = &davinci_soc_info;
	struct davinci_gpio_regs *regs;

	if (soc_info->gpio_type != GPIO_TYPE_DAVINCI)
		return 0;

	ngpio = soc_info->gpio_num;
	if (ngpio == 0) {
		pr_err("GPIO setup:  how many GPIOs?\n");
		return -EINVAL;
	}

	if (WARN_ON(DAVINCI_N_GPIO < ngpio))
		ngpio = DAVINCI_N_GPIO;

	gpio_base = ioremap(soc_info->gpio_base, SZ_4K);
	if (WARN_ON(!gpio_base))
		return -ENOMEM;

	for (i = 0, base = 0; base < ngpio; i++, base += 32) {
		chips[i].chip.label = "DaVinci";

		chips[i].chip.direction_input = davinci_direction_in;
		chips[i].chip.get = davinci_gpio_get;
		chips[i].chip.direction_output = davinci_direction_out;
		chips[i].chip.set = davinci_gpio_set;

		chips[i].chip.base = base;
		chips[i].chip.ngpio = ngpio - base;
		if (chips[i].chip.ngpio > 32)
			chips[i].chip.ngpio = 32;

		spin_lock_init(&chips[i].lock);

		regs = gpio2regs(base);
		chips[i].regs = regs;
		chips[i].set_data = &regs->set_data;
		chips[i].clr_data = &regs->clr_data;
		chips[i].in_data = &regs->in_data;

		gpiochip_add(&chips[i].chip);
	}

	soc_info->gpio_ctlrs = chips;
	soc_info->gpio_ctlrs_num = DIV_ROUND_UP(ngpio, 32);

	davinci_gpio_irq_setup();
	return 0;
}
pure_initcall(davinci_gpio_setup);


static void gpio_irq_disable(struct irq_data *d)
{
	struct davinci_gpio_regs __iomem *g = irq2regs(d->irq);
	u32 mask = (u32) irq_data_get_irq_handler_data(d);

	__raw_writel(mask, &g->clr_falling);
	__raw_writel(mask, &g->clr_rising);
}

static void gpio_irq_enable(struct irq_data *d)
{
	struct davinci_gpio_regs __iomem *g = irq2regs(d->irq);
	u32 mask = (u32) irq_data_get_irq_handler_data(d);
	unsigned status = irqd_get_trigger_type(d);

	status &= IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING;
	if (!status)
		status = IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING;

	if (status & IRQ_TYPE_EDGE_FALLING)
		__raw_writel(mask, &g->set_falling);
	if (status & IRQ_TYPE_EDGE_RISING)
		__raw_writel(mask, &g->set_rising);
}

static int gpio_irq_type(struct irq_data *d, unsigned trigger)
{
	if (trigger & ~(IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING))
		return -EINVAL;

	return 0;
}

static struct irq_chip gpio_irqchip = {
	.name		= "GPIO",
	.irq_enable	= gpio_irq_enable,
	.irq_disable	= gpio_irq_disable,
	.irq_set_type	= gpio_irq_type,
	.flags		= IRQCHIP_SET_TYPE_MASKED,
};

static void
gpio_irq_handler(unsigned irq, struct irq_desc *desc)
{
	struct davinci_gpio_regs __iomem *g;
	u32 mask = 0xffff;
	struct davinci_gpio_controller *d;

	d = (struct davinci_gpio_controller *)irq_desc_get_handler_data(desc);
	g = (struct davinci_gpio_regs __iomem *)d->regs;

	
	if (irq & 1)
		mask <<= 16;

	
	desc->irq_data.chip->irq_mask(&desc->irq_data);
	desc->irq_data.chip->irq_ack(&desc->irq_data);
	while (1) {
		u32		status;
		int		n;
		int		res;

		
		status = __raw_readl(&g->intstat) & mask;
		if (!status)
			break;
		__raw_writel(status, &g->intstat);

		
		n = d->irq_base;
		if (irq & 1) {
			n += 16;
			status >>= 16;
		}

		while (status) {
			res = ffs(status);
			n += res;
			generic_handle_irq(n - 1);
			status >>= res;
		}
	}
	desc->irq_data.chip->irq_unmask(&desc->irq_data);
	
}

static int gpio_to_irq_banked(struct gpio_chip *chip, unsigned offset)
{
	struct davinci_gpio_controller *d = chip2controller(chip);

	if (d->irq_base >= 0)
		return d->irq_base + offset;
	else
		return -ENODEV;
}

static int gpio_to_irq_unbanked(struct gpio_chip *chip, unsigned offset)
{
	struct davinci_soc_info *soc_info = &davinci_soc_info;

	if (offset < soc_info->gpio_unbanked)
		return soc_info->gpio_irq + offset;
	else
		return -ENODEV;
}

static int gpio_irq_type_unbanked(struct irq_data *data, unsigned trigger)
{
	struct davinci_gpio_controller *d;
	struct davinci_gpio_regs __iomem *g;
	struct davinci_soc_info *soc_info = &davinci_soc_info;
	u32 mask;

	d = (struct davinci_gpio_controller *)data->handler_data;
	g = (struct davinci_gpio_regs __iomem *)d->regs;
	mask = __gpio_mask(data->irq - soc_info->gpio_irq);

	if (trigger & ~(IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING))
		return -EINVAL;

	__raw_writel(mask, (trigger & IRQ_TYPE_EDGE_FALLING)
		     ? &g->set_falling : &g->clr_falling);
	__raw_writel(mask, (trigger & IRQ_TYPE_EDGE_RISING)
		     ? &g->set_rising : &g->clr_rising);

	return 0;
}


static int __init davinci_gpio_irq_setup(void)
{
	unsigned	gpio, irq, bank;
	struct clk	*clk;
	u32		binten = 0;
	unsigned	ngpio, bank_irq;
	struct davinci_soc_info *soc_info = &davinci_soc_info;
	struct davinci_gpio_regs	__iomem *g;

	ngpio = soc_info->gpio_num;

	bank_irq = soc_info->gpio_irq;
	if (bank_irq == 0) {
		printk(KERN_ERR "Don't know first GPIO bank IRQ.\n");
		return -EINVAL;
	}

	clk = clk_get(NULL, "gpio");
	if (IS_ERR(clk)) {
		printk(KERN_ERR "Error %ld getting gpio clock?\n",
		       PTR_ERR(clk));
		return PTR_ERR(clk);
	}
	clk_enable(clk);

	for (gpio = 0, bank = 0; gpio < ngpio; bank++, gpio += 32) {
		chips[bank].chip.to_irq = gpio_to_irq_banked;
		chips[bank].irq_base = soc_info->gpio_unbanked
			? -EINVAL
			: (soc_info->intc_irq_num + gpio);
	}

	if (soc_info->gpio_unbanked) {
		static struct irq_chip_type gpio_unbanked;

		
		chips[0].chip.to_irq = gpio_to_irq_unbanked;
		binten = BIT(0);

		
		irq = bank_irq;
		gpio_unbanked = *container_of(irq_get_chip(irq),
					      struct irq_chip_type, chip);
		gpio_unbanked.chip.name = "GPIO-AINTC";
		gpio_unbanked.chip.irq_set_type = gpio_irq_type_unbanked;

		
		g = gpio2regs(0);
		__raw_writel(~0, &g->set_falling);
		__raw_writel(~0, &g->set_rising);

		
		for (gpio = 0; gpio < soc_info->gpio_unbanked; gpio++, irq++) {
			irq_set_chip(irq, &gpio_unbanked.chip);
			irq_set_handler_data(irq, &chips[gpio / 32]);
			irq_set_status_flags(irq, IRQ_TYPE_EDGE_BOTH);
		}

		goto done;
	}

	for (gpio = 0, irq = gpio_to_irq(0), bank = 0;
			gpio < ngpio;
			bank++, bank_irq++) {
		unsigned		i;

		
		g = gpio2regs(gpio);
		__raw_writel(~0, &g->clr_falling);
		__raw_writel(~0, &g->clr_rising);

		
		irq_set_chained_handler(bank_irq, gpio_irq_handler);

		irq_set_handler_data(bank_irq, &chips[gpio / 32]);

		for (i = 0; i < 16 && gpio < ngpio; i++, irq++, gpio++) {
			irq_set_chip(irq, &gpio_irqchip);
			irq_set_chip_data(irq, (__force void *)g);
			irq_set_handler_data(irq, (void *)__gpio_mask(gpio));
			irq_set_handler(irq, handle_simple_irq);
			set_irq_flags(irq, IRQF_VALID);
		}

		binten |= BIT(bank);
	}

done:
	__raw_writel(binten, gpio_base + 0x08);

	printk(KERN_INFO "DaVinci: %d gpio irqs\n", irq - gpio_to_irq(0));

	return 0;
}
