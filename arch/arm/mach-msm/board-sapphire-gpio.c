/* arch/arm/mach-msm/board-sapphire-gpio.c
 * Copyright (C) 2007-2009 HTC Corporation.
 * Author: Thomas Tsai <thomas_tsai@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/sysdev.h>

#include <linux/io.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>

#include "gpio_chip.h"
#include "board-sapphire.h"

#ifdef DEBUG_SAPPHIRE_GPIO
#define DBG(fmt, arg...) printk(KERN_INFO "%s: " fmt "\n", __func__, ## arg)
#else
#define DBG(fmt, arg...) do {} while (0)
#endif

#define	SAPPHIRE_CPLD_INT_STATUS	(SAPPHIRE_CPLD_BASE + 0x0E)
#define	SAPPHIRE_CPLD_INT_LEVEL		(SAPPHIRE_CPLD_BASE + 0x08)
#define	SAPPHIRE_CPLD_INT_MASK		(SAPPHIRE_CPLD_BASE + 0x0C)

static const int _g_CPLD_MISCn_Offset[] = {	0x0A,		
						0x00,		
						0x02,		
						0x04,		
						0x06};		
static const int _g_INT_BANK_Offset[][3] = {{0x0E, 0x08, 0x0C} };

static uint8_t sapphire_cpld_initdata[4]  = {
	[0] = 0x80, 
	[1] = 0x34, 
	[3] = 0x04, 
};

static uint8_t sapphire_int_mask[] = {
	[0] = 0xfb, 
};

static uint8_t sapphire_sleep_int_mask[] = {
	[0] = 0x00,	
};

static int sapphire_suspended;

static int sapphire_gpio_read(struct gpio_chip *chip, unsigned n)
{
	if (n < SAPPHIRE_GPIO_INT_B0_BASE)	
		return !!(readb(CPLD_GPIO_REG(n)) & CPLD_GPIO_BIT_POS_MASK(n));
	else if (n <= SAPPHIRE_GPIO_END)	
		return !!(readb(CPLD_INT_LEVEL_REG_G(n)) &
						CPLD_GPIO_BIT_POS_MASK(n));
	return 0;
}

int sapphire_gpio_write(struct gpio_chip *chip, unsigned n, unsigned on)
{
	unsigned long flags;
	uint8_t reg_val;
	if (n > SAPPHIRE_GPIO_END)
		return -1;

	local_irq_save(flags);
	reg_val = readb(CPLD_GPIO_REG(n));
	if (on)
		reg_val |= CPLD_GPIO_BIT_POS_MASK(n);
	else
		reg_val &= ~CPLD_GPIO_BIT_POS_MASK(n);
	writeb(reg_val, CPLD_GPIO_REG(n));

	DBG("gpio=%d, l=0x%x\r\n", n, readb(SAPPHIRE_CPLD_INT_LEVEL));

	local_irq_restore(flags);

	return 0;
}

static int sapphire_gpio_configure(struct gpio_chip *chip, unsigned int gpio,
				   unsigned long flags)
{
	if (flags & (GPIOF_OUTPUT_LOW | GPIOF_OUTPUT_HIGH))
		sapphire_gpio_write(chip, gpio, flags & GPIOF_OUTPUT_HIGH);

	DBG("gpio=%d, l=0x%x\r\n", gpio, readb(SAPPHIRE_CPLD_INT_LEVEL));

	return 0;
}

static int sapphire_gpio_get_irq_num(struct gpio_chip *chip, unsigned int gpio,
				unsigned int *irqp, unsigned long *irqnumflagsp)
{
	DBG("gpio=%d, l=0x%x\r\n", gpio, readb(SAPPHIRE_CPLD_INT_LEVEL));
	DBG("SAPPHIRE_GPIO_INT_B0_BASE=%d, SAPPHIRE_GPIO_LAST_INT=%d\r\n",
	    SAPPHIRE_GPIO_INT_B0_BASE, SAPPHIRE_GPIO_LAST_INT);
	if ((gpio < SAPPHIRE_GPIO_INT_B0_BASE) ||
	     (gpio > SAPPHIRE_GPIO_LAST_INT))
		return -ENOENT;
	*irqp = SAPPHIRE_GPIO_TO_INT(gpio);
	DBG("*irqp=%d\r\n", *irqp);
	if (irqnumflagsp)
		*irqnumflagsp = 0;
	return 0;
}

static void sapphire_gpio_irq_ack(unsigned int irq)
{
	
	writeb(SAPPHIRE_INT_BIT_MASK(irq), CPLD_INT_STATUS_REG(irq));
}

static void sapphire_gpio_irq_enable(unsigned int irq)
{
	unsigned long flags;
	uint8_t reg_val;

	local_irq_save(flags);	

	reg_val = readb(CPLD_INT_MASK_REG(irq)) | SAPPHIRE_INT_BIT_MASK(irq);
	DBG("(irq=%d,0x%x, 0x%x)\r\n", irq, CPLD_INT_MASK_REG(irq),
	    SAPPHIRE_INT_BIT_MASK(irq));
	DBG("sapphire_suspended=%d\r\n", sapphire_suspended);
	if (!sapphire_suspended)
		writeb(reg_val, CPLD_INT_MASK_REG(irq));

	reg_val = readb(CPLD_INT_MASK_REG(irq));
	DBG("reg_val= 0x%x\r\n", reg_val);
	DBG("l=0x%x\r\n", readb(SAPPHIRE_CPLD_INT_LEVEL));

	local_irq_restore(flags); 
}

static void sapphire_gpio_irq_disable(unsigned int irq)
{
	unsigned long flags;
	uint8_t reg_val;

	local_irq_save(flags);
	reg_val = readb(CPLD_INT_MASK_REG(irq)) & ~SAPPHIRE_INT_BIT_MASK(irq);
	

	DBG("(%d,0x%x, 0x%x, 0x%x)\r\n", irq, reg_val, CPLD_INT_MASK_REG(irq),
	    SAPPHIRE_INT_BIT_MASK(irq));
	DBG("sapphire_suspended=%d\r\n", sapphire_suspended);
	if (!sapphire_suspended)
		writeb(reg_val, CPLD_INT_MASK_REG(irq));

	reg_val = readb(CPLD_INT_MASK_REG(irq));
	DBG("reg_val= 0x%x\r\n", reg_val);
	DBG("l=0x%x\r\n", readb(SAPPHIRE_CPLD_INT_LEVEL));

	local_irq_restore(flags);
}

int sapphire_gpio_irq_set_wake(unsigned int irq, unsigned int on)
{
	unsigned long flags;
	uint8_t mask = SAPPHIRE_INT_BIT_MASK(irq);

	local_irq_save(flags);

	if (on)	
		sapphire_sleep_int_mask[CPLD_INT_TO_BANK(irq)] |= mask;
	else	
		sapphire_sleep_int_mask[CPLD_INT_TO_BANK(irq)] &= ~mask;
	local_irq_restore(flags);
	return 0;
}

static void sapphire_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	int j;
	unsigned v;
	int int_base = SAPPHIRE_INT_START;

	v = readb(SAPPHIRE_CPLD_INT_STATUS);	

	for (j = 0; j < 8 ; j++) {	
		if (v & (1U << j)) {	
			DBG("generic_handle_irq j=0x%x\r\n", j);
			generic_handle_irq(int_base + j);
		}
	}

	desc->chip->ack(irq);	
	DBG("irq=%d, l=0x%x\r\n", irq, readb(SAPPHIRE_CPLD_INT_LEVEL));
}

static int sapphire_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	sapphire_suspended = 1;
	
	sapphire_int_mask[0] = readb(SAPPHIRE_CPLD_BASE +
					SAPPHIRE_GPIO_INT_B0_MASK_REG);

	
	writeb(sapphire_sleep_int_mask[0],
	       SAPPHIRE_CPLD_BASE +  SAPPHIRE_GPIO_INT_B0_MASK_REG);

	return 0;
}

int sapphire_sysdev_resume(struct sys_device *dev)
{
	
	writeb(sapphire_int_mask[0], SAPPHIRE_CPLD_BASE +
					SAPPHIRE_GPIO_INT_B0_MASK_REG);
	sapphire_suspended = 0;
	return 0;
}

static struct irq_chip sapphire_gpio_irq_chip = {
	.name      = "sapphiregpio",
	.ack       = sapphire_gpio_irq_ack,
	.mask      = sapphire_gpio_irq_disable,	
	.unmask    = sapphire_gpio_irq_enable,	
	.set_wake  = sapphire_gpio_irq_set_wake,
	
};

static struct gpio_chip sapphire_gpio_chip = {
	.start = SAPPHIRE_GPIO_START,
	.end = SAPPHIRE_GPIO_END,
	.configure = sapphire_gpio_configure,
	.get_irq_num = sapphire_gpio_get_irq_num,
	.read = sapphire_gpio_read,
	.write = sapphire_gpio_write,
};

struct sysdev_class sapphire_sysdev_class = {
	.name = "sapphiregpio_irq",
	.suspend = sapphire_sysdev_suspend,
	.resume = sapphire_sysdev_resume,
};

static struct sys_device sapphire_irq_device = {
	.cls    = &sapphire_sysdev_class,
};

int sapphire_init_gpio(void)
{
	int i;
	if (!machine_is_sapphire())
		return 0;

	DBG("%d,%d\r\n", SAPPHIRE_INT_START, SAPPHIRE_INT_END);
	DBG("NR_MSM_IRQS=%d, NR_GPIO_IRQS=%d\r\n", NR_MSM_IRQS, NR_GPIO_IRQS);
	for (i = SAPPHIRE_INT_START; i <= SAPPHIRE_INT_END; i++) {
		set_irq_chip(i, &sapphire_gpio_irq_chip);
		set_irq_handler(i, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
	}

	register_gpio_chip(&sapphire_gpio_chip);

	
	set_irq_type(MSM_GPIO_TO_INT(17), IRQF_TRIGGER_HIGH);
	set_irq_chained_handler(MSM_GPIO_TO_INT(17), sapphire_gpio_irq_handler);
	set_irq_wake(MSM_GPIO_TO_INT(17), 1);

	if (sysdev_class_register(&sapphire_sysdev_class) == 0)
		sysdev_register(&sapphire_irq_device);

	return 0;
}

int sapphire_init_cpld(unsigned int sys_rev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sapphire_cpld_initdata); i++)
		writeb(sapphire_cpld_initdata[i], SAPPHIRE_CPLD_BASE + i * 2);
	return 0;
}

postcore_initcall(sapphire_init_gpio);
