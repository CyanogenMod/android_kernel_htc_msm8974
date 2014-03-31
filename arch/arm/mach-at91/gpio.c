/*
 * linux/arch/arm/mach-at91/gpio.c
 *
 * Copyright (C) 2005 HP Labs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/irqdomain.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>

#include <mach/hardware.h>
#include <mach/at91_pio.h>

#include "generic.h"

struct at91_gpio_chip {
	struct gpio_chip	chip;
	struct at91_gpio_chip	*next;		
	int			pioc_hwirq;	
	int			pioc_virq;	
	int			pioc_idx;	
	void __iomem		*regbase;	
	struct clk		*clock;		
	struct irq_domain	*domain;	
};

#define to_at91_gpio_chip(c) container_of(c, struct at91_gpio_chip, chip)

static void at91_gpiolib_dbg_show(struct seq_file *s, struct gpio_chip *chip);
static void at91_gpiolib_set(struct gpio_chip *chip, unsigned offset, int val);
static int at91_gpiolib_get(struct gpio_chip *chip, unsigned offset);
static int at91_gpiolib_direction_output(struct gpio_chip *chip,
					 unsigned offset, int val);
static int at91_gpiolib_direction_input(struct gpio_chip *chip,
					unsigned offset);
static int at91_gpiolib_to_irq(struct gpio_chip *chip, unsigned offset);

#define AT91_GPIO_CHIP(name, nr_gpio)					\
	{								\
		.chip = {						\
			.label		  = name,			\
			.direction_input  = at91_gpiolib_direction_input, \
			.direction_output = at91_gpiolib_direction_output, \
			.get		  = at91_gpiolib_get,		\
			.set		  = at91_gpiolib_set,		\
			.dbg_show	  = at91_gpiolib_dbg_show,	\
			.to_irq		  = at91_gpiolib_to_irq,	\
			.ngpio		  = nr_gpio,			\
		},							\
	}

static struct at91_gpio_chip gpio_chip[] = {
	AT91_GPIO_CHIP("pioA", 32),
	AT91_GPIO_CHIP("pioB", 32),
	AT91_GPIO_CHIP("pioC", 32),
	AT91_GPIO_CHIP("pioD", 32),
	AT91_GPIO_CHIP("pioE", 32),
};

static int gpio_banks;
static unsigned long at91_gpio_caps;

#define AT91_GPIO_CAP_PIO3	(1 <<  0)

#define has_pio3()	(at91_gpio_caps & AT91_GPIO_CAP_PIO3)


static inline void __iomem *pin_to_controller(unsigned pin)
{
	pin /= 32;
	if (likely(pin < gpio_banks))
		return gpio_chip[pin].regbase;

	return NULL;
}

static inline unsigned pin_to_mask(unsigned pin)
{
	return 1 << (pin % 32);
}


static char peripheral_function(void __iomem *pio, unsigned mask)
{
	char	ret = 'X';
	u8	select;

	if (pio) {
		if (has_pio3()) {
			select = !!(__raw_readl(pio + PIO_ABCDSR1) & mask);
			select |= (!!(__raw_readl(pio + PIO_ABCDSR2) & mask) << 1);
			ret = 'A' + select;
		} else {
			ret = __raw_readl(pio + PIO_ABSR) & mask ?
							'B' : 'A';
		}
	}

	return ret;
}




int __init_or_module at91_set_GPIO_periph(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;
	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	__raw_writel(mask, pio + PIO_PER);
	return 0;
}
EXPORT_SYMBOL(at91_set_GPIO_periph);


int __init_or_module at91_set_A_periph(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	if (has_pio3()) {
		__raw_writel(__raw_readl(pio + PIO_ABCDSR1) & ~mask,
							pio + PIO_ABCDSR1);
		__raw_writel(__raw_readl(pio + PIO_ABCDSR2) & ~mask,
							pio + PIO_ABCDSR2);
	} else {
		__raw_writel(mask, pio + PIO_ASR);
	}
	__raw_writel(mask, pio + PIO_PDR);
	return 0;
}
EXPORT_SYMBOL(at91_set_A_periph);


int __init_or_module at91_set_B_periph(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	if (has_pio3()) {
		__raw_writel(__raw_readl(pio + PIO_ABCDSR1) | mask,
							pio + PIO_ABCDSR1);
		__raw_writel(__raw_readl(pio + PIO_ABCDSR2) & ~mask,
							pio + PIO_ABCDSR2);
	} else {
		__raw_writel(mask, pio + PIO_BSR);
	}
	__raw_writel(mask, pio + PIO_PDR);
	return 0;
}
EXPORT_SYMBOL(at91_set_B_periph);


int __init_or_module at91_set_C_periph(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio || !has_pio3())
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	__raw_writel(__raw_readl(pio + PIO_ABCDSR1) & ~mask, pio + PIO_ABCDSR1);
	__raw_writel(__raw_readl(pio + PIO_ABCDSR2) | mask, pio + PIO_ABCDSR2);
	__raw_writel(mask, pio + PIO_PDR);
	return 0;
}
EXPORT_SYMBOL(at91_set_C_periph);


int __init_or_module at91_set_D_periph(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio || !has_pio3())
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	__raw_writel(__raw_readl(pio + PIO_ABCDSR1) | mask, pio + PIO_ABCDSR1);
	__raw_writel(__raw_readl(pio + PIO_ABCDSR2) | mask, pio + PIO_ABCDSR2);
	__raw_writel(mask, pio + PIO_PDR);
	return 0;
}
EXPORT_SYMBOL(at91_set_D_periph);


int __init_or_module at91_set_gpio_input(unsigned pin, int use_pullup)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + (use_pullup ? PIO_PUER : PIO_PUDR));
	__raw_writel(mask, pio + PIO_ODR);
	__raw_writel(mask, pio + PIO_PER);
	return 0;
}
EXPORT_SYMBOL(at91_set_gpio_input);


int __init_or_module at91_set_gpio_output(unsigned pin, int value)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + PIO_IDR);
	__raw_writel(mask, pio + PIO_PUDR);
	__raw_writel(mask, pio + (value ? PIO_SODR : PIO_CODR));
	__raw_writel(mask, pio + PIO_OER);
	__raw_writel(mask, pio + PIO_PER);
	return 0;
}
EXPORT_SYMBOL(at91_set_gpio_output);


int __init_or_module at91_set_deglitch(unsigned pin, int is_on)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	if (has_pio3() && is_on)
		__raw_writel(mask, pio + PIO_IFSCDR);
	__raw_writel(mask, pio + (is_on ? PIO_IFER : PIO_IFDR));
	return 0;
}
EXPORT_SYMBOL(at91_set_deglitch);

int __init_or_module at91_set_debounce(unsigned pin, int is_on, int div)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio || !has_pio3())
		return -EINVAL;

	if (is_on) {
		__raw_writel(mask, pio + PIO_IFSCER);
		__raw_writel(div & PIO_SCDR_DIV, pio + PIO_SCDR);
		__raw_writel(mask, pio + PIO_IFER);
	} else {
		__raw_writel(mask, pio + PIO_IFDR);
	}
	return 0;
}
EXPORT_SYMBOL(at91_set_debounce);

int __init_or_module at91_set_multi_drive(unsigned pin, int is_on)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;

	__raw_writel(mask, pio + (is_on ? PIO_MDER : PIO_MDDR));
	return 0;
}
EXPORT_SYMBOL(at91_set_multi_drive);

int __init_or_module at91_set_pulldown(unsigned pin, int is_on)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio || !has_pio3())
		return -EINVAL;

	
	__raw_writel(mask, pio + PIO_PUDR);
	__raw_writel(mask, pio + (is_on ? PIO_PPDER : PIO_PPDDR));
	return 0;
}
EXPORT_SYMBOL(at91_set_pulldown);

int __init_or_module at91_disable_schmitt_trig(unsigned pin)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio || !has_pio3())
		return -EINVAL;

	__raw_writel(__raw_readl(pio + PIO_SCHMITT) | mask, pio + PIO_SCHMITT);
	return 0;
}
EXPORT_SYMBOL(at91_disable_schmitt_trig);

int at91_set_gpio_value(unsigned pin, int value)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);

	if (!pio)
		return -EINVAL;
	__raw_writel(mask, pio + (value ? PIO_SODR : PIO_CODR));
	return 0;
}
EXPORT_SYMBOL(at91_set_gpio_value);


int at91_get_gpio_value(unsigned pin)
{
	void __iomem	*pio = pin_to_controller(pin);
	unsigned	mask = pin_to_mask(pin);
	u32		pdsr;

	if (!pio)
		return -EINVAL;
	pdsr = __raw_readl(pio + PIO_PDSR);
	return (pdsr & mask) != 0;
}
EXPORT_SYMBOL(at91_get_gpio_value);


#ifdef CONFIG_PM

static u32 wakeups[MAX_GPIO_BANKS];
static u32 backups[MAX_GPIO_BANKS];

static int gpio_irq_set_wake(struct irq_data *d, unsigned state)
{
	struct at91_gpio_chip *at91_gpio = irq_data_get_irq_chip_data(d);
	unsigned	mask = 1 << d->hwirq;
	unsigned	bank = at91_gpio->pioc_idx;

	if (unlikely(bank >= MAX_GPIO_BANKS))
		return -EINVAL;

	if (state)
		wakeups[bank] |= mask;
	else
		wakeups[bank] &= ~mask;

	irq_set_irq_wake(at91_gpio->pioc_virq, state);

	return 0;
}

void at91_gpio_suspend(void)
{
	int i;

	for (i = 0; i < gpio_banks; i++) {
		void __iomem	*pio = gpio_chip[i].regbase;

		backups[i] = __raw_readl(pio + PIO_IMR);
		__raw_writel(backups[i], pio + PIO_IDR);
		__raw_writel(wakeups[i], pio + PIO_IER);

		if (!wakeups[i]) {
			clk_unprepare(gpio_chip[i].clock);
			clk_disable(gpio_chip[i].clock);
		} else {
#ifdef CONFIG_PM_DEBUG
			printk(KERN_DEBUG "GPIO-%c may wake for %08x\n", 'A'+i, wakeups[i]);
#endif
		}
	}
}

void at91_gpio_resume(void)
{
	int i;

	for (i = 0; i < gpio_banks; i++) {
		void __iomem	*pio = gpio_chip[i].regbase;

		if (!wakeups[i]) {
			if (clk_prepare(gpio_chip[i].clock) == 0)
				clk_enable(gpio_chip[i].clock);
		}

		__raw_writel(wakeups[i], pio + PIO_IDR);
		__raw_writel(backups[i], pio + PIO_IER);
	}
}

#else
#define gpio_irq_set_wake	NULL
#endif



static void gpio_irq_mask(struct irq_data *d)
{
	struct at91_gpio_chip *at91_gpio = irq_data_get_irq_chip_data(d);
	void __iomem	*pio = at91_gpio->regbase;
	unsigned	mask = 1 << d->hwirq;

	if (pio)
		__raw_writel(mask, pio + PIO_IDR);
}

static void gpio_irq_unmask(struct irq_data *d)
{
	struct at91_gpio_chip *at91_gpio = irq_data_get_irq_chip_data(d);
	void __iomem	*pio = at91_gpio->regbase;
	unsigned	mask = 1 << d->hwirq;

	if (pio)
		__raw_writel(mask, pio + PIO_IER);
}

static int gpio_irq_type(struct irq_data *d, unsigned type)
{
	switch (type) {
	case IRQ_TYPE_NONE:
	case IRQ_TYPE_EDGE_BOTH:
		return 0;
	default:
		return -EINVAL;
	}
}

static int alt_gpio_irq_type(struct irq_data *d, unsigned type)
{
	struct at91_gpio_chip *at91_gpio = irq_data_get_irq_chip_data(d);
	void __iomem	*pio = at91_gpio->regbase;
	unsigned	mask = 1 << d->hwirq;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		__raw_writel(mask, pio + PIO_ESR);
		__raw_writel(mask, pio + PIO_REHLSR);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		__raw_writel(mask, pio + PIO_ESR);
		__raw_writel(mask, pio + PIO_FELLSR);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		__raw_writel(mask, pio + PIO_LSR);
		__raw_writel(mask, pio + PIO_FELLSR);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		__raw_writel(mask, pio + PIO_LSR);
		__raw_writel(mask, pio + PIO_REHLSR);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		__raw_writel(mask, pio + PIO_AIMDR);
		return 0;
	case IRQ_TYPE_NONE:
	default:
		pr_warn("AT91: No type for irq %d\n", gpio_to_irq(d->irq));
		return -EINVAL;
	}

	
	__raw_writel(mask, pio + PIO_AIMER);

	return 0;
}

static struct irq_chip gpio_irqchip = {
	.name		= "GPIO",
	.irq_disable	= gpio_irq_mask,
	.irq_mask	= gpio_irq_mask,
	.irq_unmask	= gpio_irq_unmask,
	
	.irq_set_wake	= gpio_irq_set_wake,
};

static void gpio_irq_handler(unsigned irq, struct irq_desc *desc)
{
	struct irq_data *idata = irq_desc_get_irq_data(desc);
	struct irq_chip *chip = irq_data_get_irq_chip(idata);
	struct at91_gpio_chip *at91_gpio = irq_data_get_irq_chip_data(idata);
	void __iomem	*pio = at91_gpio->regbase;
	unsigned long	isr;
	int		n;

	
	chip->irq_ack(idata);
	for (;;) {
		isr = __raw_readl(pio + PIO_ISR) & __raw_readl(pio + PIO_IMR);
		if (!isr) {
			if (!at91_gpio->next)
				break;
			at91_gpio = at91_gpio->next;
			pio = at91_gpio->regbase;
			continue;
		}

		n = find_first_bit(&isr, BITS_PER_LONG);
		while (n < BITS_PER_LONG) {
			generic_handle_irq(irq_find_mapping(at91_gpio->domain, n));
			n = find_next_bit(&isr, BITS_PER_LONG, n + 1);
		}
	}
	chip->irq_unmask(idata);
	
}


#ifdef CONFIG_DEBUG_FS

static void gpio_printf(struct seq_file *s, void __iomem *pio, unsigned mask)
{
	char	*trigger = NULL;
	char	*polarity = NULL;

	if (__raw_readl(pio + PIO_IMR) & mask) {
		if (!has_pio3() || !(__raw_readl(pio + PIO_AIMMR) & mask )) {
			trigger = "edge";
			polarity = "both";
		} else {
			if (__raw_readl(pio + PIO_ELSR) & mask) {
				trigger = "level";
				polarity = __raw_readl(pio + PIO_FRLHSR) & mask ?
					"high" : "low";
			} else {
				trigger = "edge";
				polarity = __raw_readl(pio + PIO_FRLHSR) & mask ?
						"rising" : "falling";
			}
		}
		seq_printf(s, "IRQ:%s-%s\t", trigger, polarity);
	} else {
		seq_printf(s, "GPIO:%s\t\t",
				__raw_readl(pio + PIO_PDSR) & mask ? "1" : "0");
	}
}

static int at91_gpio_show(struct seq_file *s, void *unused)
{
	int bank, j;

	
	seq_printf(s, "Pin\t");
	for (bank = 0; bank < gpio_banks; bank++) {
		seq_printf(s, "PIO%c\t\t", 'A' + bank);
	};
	seq_printf(s, "\n\n");

	
	for (j = 0; j < 32; j++) {
		seq_printf(s, "%i:\t", j);

		for (bank = 0; bank < gpio_banks; bank++) {
			unsigned	pin  = (32 * bank) + j;
			void __iomem	*pio = pin_to_controller(pin);
			unsigned	mask = pin_to_mask(pin);

			if (__raw_readl(pio + PIO_PSR) & mask)
				gpio_printf(s, pio, mask);
			else
				seq_printf(s, "%c\t\t",
						peripheral_function(pio, mask));
		}

		seq_printf(s, "\n");
	}

	return 0;
}

static int at91_gpio_open(struct inode *inode, struct file *file)
{
	return single_open(file, at91_gpio_show, NULL);
}

static const struct file_operations at91_gpio_operations = {
	.open		= at91_gpio_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init at91_gpio_debugfs_init(void)
{
	
	(void) debugfs_create_file("at91_gpio", S_IFREG | S_IRUGO, NULL, NULL, &at91_gpio_operations);
	return 0;
}
postcore_initcall(at91_gpio_debugfs_init);

#endif


static struct lock_class_key gpio_lock_class;

#if defined(CONFIG_OF)
static int at91_gpio_irq_map(struct irq_domain *h, unsigned int virq,
							irq_hw_number_t hw)
{
	struct at91_gpio_chip	*at91_gpio = h->host_data;

	irq_set_lockdep_class(virq, &gpio_lock_class);

	irq_set_chip_and_handler(virq, &gpio_irqchip,
				 handle_simple_irq);
	set_irq_flags(virq, IRQF_VALID);
	irq_set_chip_data(virq, at91_gpio);

	return 0;
}

static struct irq_domain_ops at91_gpio_ops = {
	.map	= at91_gpio_irq_map,
	.xlate	= irq_domain_xlate_twocell,
};

int __init at91_gpio_of_irq_setup(struct device_node *node,
				     struct device_node *parent)
{
	struct at91_gpio_chip	*prev = NULL;
	int			alias_idx = of_alias_get_id(node, "gpio");
	struct at91_gpio_chip	*at91_gpio = &gpio_chip[alias_idx];

	
	if (has_pio3())
		gpio_irqchip.irq_set_type = alt_gpio_irq_type;
	else
		gpio_irqchip.irq_set_type = gpio_irq_type;

	
	__raw_writel(~0, at91_gpio->regbase + PIO_IDR);

	
	at91_gpio->domain = irq_domain_add_linear(node, at91_gpio->chip.ngpio,
						&at91_gpio_ops, at91_gpio);
	if (!at91_gpio->domain)
		panic("at91_gpio.%d: couldn't allocate irq domain (DT).\n",
			at91_gpio->pioc_idx);

	
	if (at91_gpio->pioc_idx)
		prev = &gpio_chip[at91_gpio->pioc_idx - 1];

	if (prev && prev->next == at91_gpio)
		return 0;

	at91_gpio->pioc_virq = irq_create_mapping(irq_find_host(parent),
							at91_gpio->pioc_hwirq);
	irq_set_chip_data(at91_gpio->pioc_virq, at91_gpio);
	irq_set_chained_handler(at91_gpio->pioc_virq, gpio_irq_handler);

	return 0;
}
#else
int __init at91_gpio_of_irq_setup(struct device_node *node,
				     struct device_node *parent)
{
	return -EINVAL;
}
#endif

static void __init at91_gpio_irqdomain(struct at91_gpio_chip *at91_gpio)
{
	int irq_base;

	irq_base = irq_alloc_descs(-1, 0, at91_gpio->chip.ngpio, 0);
	if (irq_base < 0)
		panic("at91_gpio.%d: error %d: couldn't allocate IRQ numbers.\n",
			at91_gpio->pioc_idx, irq_base);
	at91_gpio->domain = irq_domain_add_legacy(NULL, at91_gpio->chip.ngpio,
						  irq_base, 0,
						  &irq_domain_simple_ops, NULL);
	if (!at91_gpio->domain)
		panic("at91_gpio.%d: couldn't allocate irq domain.\n",
			at91_gpio->pioc_idx);
}

void __init at91_gpio_irq_setup(void)
{
	unsigned		pioc;
	int			gpio_irqnbr = 0;
	struct at91_gpio_chip	*this, *prev;

	
	if (has_pio3())
		gpio_irqchip.irq_set_type = alt_gpio_irq_type;
	else
		gpio_irqchip.irq_set_type = gpio_irq_type;

	for (pioc = 0, this = gpio_chip, prev = NULL;
			pioc++ < gpio_banks;
			prev = this, this++) {
		int offset;

		__raw_writel(~0, this->regbase + PIO_IDR);

		
		at91_gpio_irqdomain(this);

		for (offset = 0; offset < this->chip.ngpio; offset++) {
			unsigned int virq = irq_find_mapping(this->domain, offset);
			irq_set_lockdep_class(virq, &gpio_lock_class);

			irq_set_chip_and_handler(virq, &gpio_irqchip,
						 handle_simple_irq);
			set_irq_flags(virq, IRQF_VALID);
			irq_set_chip_data(virq, this);

			gpio_irqnbr++;
		}

		if (prev && prev->next == this)
			continue;

		this->pioc_virq = irq_create_mapping(NULL, this->pioc_hwirq);
		irq_set_chip_data(this->pioc_virq, this);
		irq_set_chained_handler(this->pioc_virq, gpio_irq_handler);
	}
	pr_info("AT91: %d gpio irqs in %d banks\n", gpio_irqnbr, gpio_banks);
}

static int at91_gpiolib_direction_input(struct gpio_chip *chip,
					unsigned offset)
{
	struct at91_gpio_chip *at91_gpio = to_at91_gpio_chip(chip);
	void __iomem *pio = at91_gpio->regbase;
	unsigned mask = 1 << offset;

	__raw_writel(mask, pio + PIO_ODR);
	return 0;
}

static int at91_gpiolib_direction_output(struct gpio_chip *chip,
					 unsigned offset, int val)
{
	struct at91_gpio_chip *at91_gpio = to_at91_gpio_chip(chip);
	void __iomem *pio = at91_gpio->regbase;
	unsigned mask = 1 << offset;

	__raw_writel(mask, pio + (val ? PIO_SODR : PIO_CODR));
	__raw_writel(mask, pio + PIO_OER);
	return 0;
}

static int at91_gpiolib_get(struct gpio_chip *chip, unsigned offset)
{
	struct at91_gpio_chip *at91_gpio = to_at91_gpio_chip(chip);
	void __iomem *pio = at91_gpio->regbase;
	unsigned mask = 1 << offset;
	u32 pdsr;

	pdsr = __raw_readl(pio + PIO_PDSR);
	return (pdsr & mask) != 0;
}

static void at91_gpiolib_set(struct gpio_chip *chip, unsigned offset, int val)
{
	struct at91_gpio_chip *at91_gpio = to_at91_gpio_chip(chip);
	void __iomem *pio = at91_gpio->regbase;
	unsigned mask = 1 << offset;

	__raw_writel(mask, pio + (val ? PIO_SODR : PIO_CODR));
}

static void at91_gpiolib_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	int i;

	for (i = 0; i < chip->ngpio; i++) {
		unsigned pin = chip->base + i;
		void __iomem *pio = pin_to_controller(pin);
		unsigned mask = pin_to_mask(pin);
		const char *gpio_label;

		gpio_label = gpiochip_is_requested(chip, i);
		if (gpio_label) {
			seq_printf(s, "[%s] GPIO%s%d: ",
				   gpio_label, chip->label, i);
			if (__raw_readl(pio + PIO_PSR) & mask)
				seq_printf(s, "[gpio] %s\n",
					   at91_get_gpio_value(pin) ?
					   "set" : "clear");
			else
				seq_printf(s, "[periph %c]\n",
					   peripheral_function(pio, mask));
		}
	}
}

static int at91_gpiolib_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct at91_gpio_chip *at91_gpio = to_at91_gpio_chip(chip);
	int virq;

	if (offset < chip->ngpio)
		virq = irq_create_mapping(at91_gpio->domain, offset);
	else
		virq = -ENXIO;

	dev_dbg(chip->dev, "%s: request IRQ for GPIO %d, return %d\n",
				chip->label, offset + chip->base, virq);
	return virq;
}

static int __init at91_gpio_setup_clk(int idx)
{
	struct at91_gpio_chip *at91_gpio = &gpio_chip[idx];

	
	at91_gpio->clock = clk_get_sys(NULL, at91_gpio->chip.label);
	if (IS_ERR(at91_gpio->clock)) {
		pr_err("at91_gpio.%d, failed to get clock, ignoring.\n", idx);
		goto err;
	}

	if (clk_prepare(at91_gpio->clock))
		goto clk_prep_err;

	
	if (clk_enable(at91_gpio->clock)) {
		pr_err("at91_gpio.%d, failed to enable clock, ignoring.\n", idx);
		goto clk_err;
	}

	return 0;

clk_err:
	clk_unprepare(at91_gpio->clock);
clk_prep_err:
	clk_put(at91_gpio->clock);
err:
	return -EINVAL;
}

#ifdef CONFIG_OF_GPIO
static void __init of_at91_gpio_init_one(struct device_node *np)
{
	int alias_idx;
	struct at91_gpio_chip *at91_gpio;

	if (!np)
		return;

	alias_idx = of_alias_get_id(np, "gpio");
	if (alias_idx >= MAX_GPIO_BANKS) {
		pr_err("at91_gpio, failed alias idx(%d) > MAX_GPIO_BANKS(%d), ignoring.\n",
						alias_idx, MAX_GPIO_BANKS);
		return;
	}

	at91_gpio = &gpio_chip[alias_idx];
	at91_gpio->chip.base = alias_idx * at91_gpio->chip.ngpio;

	at91_gpio->regbase = of_iomap(np, 0);
	if (!at91_gpio->regbase) {
		pr_err("at91_gpio.%d, failed to map registers, ignoring.\n",
								alias_idx);
		return;
	}

	
	if (of_property_read_u32(np, "interrupts", &at91_gpio->pioc_hwirq)) {
		pr_err("at91_gpio.%d, failed to get interrupts property, ignoring.\n",
								alias_idx);
		goto ioremap_err;
	}

	
	if (of_device_is_compatible(np, "atmel,at91sam9x5-gpio"))
		at91_gpio_caps |= AT91_GPIO_CAP_PIO3;

	
	if (at91_gpio_setup_clk(alias_idx))
		goto ioremap_err;

	at91_gpio->chip.of_node = np;
	gpio_banks = max(gpio_banks, alias_idx + 1);
	at91_gpio->pioc_idx = alias_idx;
	return;

ioremap_err:
	iounmap(at91_gpio->regbase);
}

static int __init of_at91_gpio_init(void)
{
	struct device_node *np = NULL;

	for_each_compatible_node(np, NULL, "atmel,at91rm9200-gpio")
		of_at91_gpio_init_one(np);

	return gpio_banks > 0 ? 0 : -EINVAL;
}
#else
static int __init of_at91_gpio_init(void)
{
	return -EINVAL;
}
#endif

static void __init at91_gpio_init_one(int idx, u32 regbase, int pioc_hwirq)
{
	struct at91_gpio_chip *at91_gpio = &gpio_chip[idx];

	at91_gpio->chip.base = idx * at91_gpio->chip.ngpio;
	at91_gpio->pioc_hwirq = pioc_hwirq;
	at91_gpio->pioc_idx = idx;

	at91_gpio->regbase = ioremap(regbase, 512);
	if (!at91_gpio->regbase) {
		pr_err("at91_gpio.%d, failed to map registers, ignoring.\n", idx);
		return;
	}

	if (at91_gpio_setup_clk(idx))
		goto ioremap_err;

	gpio_banks = max(gpio_banks, idx + 1);
	return;

ioremap_err:
	iounmap(at91_gpio->regbase);
}

void __init at91_gpio_init(struct at91_gpio_bank *data, int nr_banks)
{
	unsigned i;
	struct at91_gpio_chip *at91_gpio, *last = NULL;

	BUG_ON(nr_banks > MAX_GPIO_BANKS);

	if (of_at91_gpio_init() < 0) {
		
		for (i = 0; i < nr_banks; i++)
			at91_gpio_init_one(i, data[i].regbase, data[i].id);
	}

	for (i = 0; i < gpio_banks; i++) {
		at91_gpio = &gpio_chip[i];

		if (last && last->pioc_hwirq == at91_gpio->pioc_hwirq)
			last->next = at91_gpio;
		last = at91_gpio;

		gpiochip_add(&at91_gpio->chip);
	}
}
