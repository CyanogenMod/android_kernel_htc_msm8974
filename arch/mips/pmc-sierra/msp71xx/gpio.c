/*
 * Generic PMC MSP71xx GPIO handling. These base gpio are controlled by two
 * types of registers. The data register sets the output level when in output
 * mode and when in input mode will contain the value at the input. The config
 * register sets the various modes for each gpio.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @author Patrick Glass <patrickglass@gmail.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#define MSP71XX_CFG_OFFSET(gpio)	(4 * (gpio))
#define CONF_MASK			0x0F
#define MSP71XX_GPIO_INPUT		0x01
#define MSP71XX_GPIO_OUTPUT		0x08

#define MSP71XX_GPIO_BASE		0x0B8400000L

#define to_msp71xx_gpio_chip(c) container_of(c, struct msp71xx_gpio_chip, chip)

static spinlock_t gpio_lock;

struct msp71xx_gpio_chip {
	struct gpio_chip chip;
	void __iomem *data_reg;
	void __iomem *config_reg;
	void __iomem *out_drive_reg;
};

static int msp71xx_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct msp71xx_gpio_chip *msp_chip = to_msp71xx_gpio_chip(chip);

	return __raw_readl(msp_chip->data_reg) & (1 << offset);
}

static void msp71xx_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct msp71xx_gpio_chip *msp_chip = to_msp71xx_gpio_chip(chip);
	unsigned long flags;
	u32 data;

	spin_lock_irqsave(&gpio_lock, flags);

	data = __raw_readl(msp_chip->data_reg);
	if (value)
		data |= (1 << offset);
	else
		data &= ~(1 << offset);
	__raw_writel(data, msp_chip->data_reg);

	spin_unlock_irqrestore(&gpio_lock, flags);
}

static int msp71xx_set_gpio_mode(struct gpio_chip *chip,
				 unsigned offset, int mode)
{
	struct msp71xx_gpio_chip *msp_chip = to_msp71xx_gpio_chip(chip);
	const unsigned bit_offset = MSP71XX_CFG_OFFSET(offset);
	unsigned long flags;
	u32 cfg;

	spin_lock_irqsave(&gpio_lock, flags);

	cfg = __raw_readl(msp_chip->config_reg);
	cfg &= ~(CONF_MASK << bit_offset);
	cfg |= (mode << bit_offset);
	__raw_writel(cfg, msp_chip->config_reg);

	spin_unlock_irqrestore(&gpio_lock, flags);

	return 0;
}

static int msp71xx_direction_output(struct gpio_chip *chip,
				    unsigned offset, int value)
{
	msp71xx_gpio_set(chip, offset, value);

	return msp71xx_set_gpio_mode(chip, offset, MSP71XX_GPIO_OUTPUT);
}

static int msp71xx_direction_input(struct gpio_chip *chip, unsigned offset)
{
	return msp71xx_set_gpio_mode(chip, offset, MSP71XX_GPIO_INPUT);
}

int msp71xx_set_output_drive(unsigned gpio, int value)
{
	unsigned long flags;
	u32 data;

	if (gpio > 15 || gpio < 0)
		return -EINVAL;

	spin_lock_irqsave(&gpio_lock, flags);

	data = __raw_readl((void __iomem *)(MSP71XX_GPIO_BASE + 0x190));
	if (value)
		data |= (1 << gpio);
	else
		data &= ~(1 << gpio);
	__raw_writel(data, (void __iomem *)(MSP71XX_GPIO_BASE + 0x190));

	spin_unlock_irqrestore(&gpio_lock, flags);

	return 0;
}
EXPORT_SYMBOL(msp71xx_set_output_drive);

#define MSP71XX_GPIO_BANK(name, dr, cr, base_gpio, num_gpio) \
{ \
	.chip = { \
		.label		  = name, \
		.direction_input  = msp71xx_direction_input, \
		.direction_output = msp71xx_direction_output, \
		.get		  = msp71xx_gpio_get, \
		.set		  = msp71xx_gpio_set, \
		.base		  = base_gpio, \
		.ngpio		  = num_gpio \
	}, \
	.data_reg	= (void __iomem *)(MSP71XX_GPIO_BASE + dr), \
	.config_reg	= (void __iomem *)(MSP71XX_GPIO_BASE + cr), \
	.out_drive_reg	= (void __iomem *)(MSP71XX_GPIO_BASE + 0x190), \
}

static struct msp71xx_gpio_chip msp71xx_gpio_banks[] = {

	MSP71XX_GPIO_BANK("GPIO_1_0", 0x170, 0x180, 0, 2),
	MSP71XX_GPIO_BANK("GPIO_5_2", 0x174, 0x184, 2, 4),
	MSP71XX_GPIO_BANK("GPIO_9_6", 0x178, 0x188, 6, 4),
	MSP71XX_GPIO_BANK("GPIO_15_10", 0x17C, 0x18C, 10, 6),
};

void __init msp71xx_init_gpio(void)
{
	int i;

	spin_lock_init(&gpio_lock);

	for (i = 0; i < ARRAY_SIZE(msp71xx_gpio_banks); i++)
		gpiochip_add(&msp71xx_gpio_banks[i].chip);
}
