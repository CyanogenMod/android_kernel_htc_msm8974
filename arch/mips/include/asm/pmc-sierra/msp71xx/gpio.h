/*
 * include/asm-mips/pmc-sierra/msp71xx/gpio.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @author Patrick Glass <patrickglass@gmail.com>
 */

#ifndef __PMC_MSP71XX_GPIO_H
#define __PMC_MSP71XX_GPIO_H

#define ARCH_NR_GPIOS (28 + (3 * 8))

#include <asm-generic/gpio.h>

#define gpio_get_value	__gpio_get_value
#define gpio_set_value	__gpio_set_value
#define gpio_cansleep	__gpio_cansleep

extern void msp71xx_init_gpio(void);
extern void msp71xx_init_gpio_extended(void);
extern int msp71xx_set_output_drive(unsigned gpio, int value);

static inline int gpio_set_output_drive(unsigned gpio, int value)
{
	return msp71xx_set_output_drive(gpio, value);
}

static inline int gpio_to_irq(unsigned gpio)
{
	return -EINVAL;
}

static inline int irq_to_gpio(unsigned irq)
{
	return -EINVAL;
}

#endif 
