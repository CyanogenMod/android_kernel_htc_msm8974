/* linux/arch/arm/plat-s3c/include/plat/gpio-cfg.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C Platform - GPIO pin configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/



#ifndef __PLAT_GPIO_CFG_H
#define __PLAT_GPIO_CFG_H __FILE__

#include<linux/types.h>

typedef unsigned int __bitwise__ samsung_gpio_pull_t;
typedef unsigned int __bitwise__ s5p_gpio_drvstr_t;

struct samsung_gpio_chip;

struct samsung_gpio_cfg {
	unsigned int	cfg_eint;

	samsung_gpio_pull_t	(*get_pull)(struct samsung_gpio_chip *chip, unsigned offs);
	int		(*set_pull)(struct samsung_gpio_chip *chip, unsigned offs,
				    samsung_gpio_pull_t pull);

	unsigned (*get_config)(struct samsung_gpio_chip *chip, unsigned offs);
	int	 (*set_config)(struct samsung_gpio_chip *chip, unsigned offs,
			       unsigned config);
};

#define S3C_GPIO_SPECIAL_MARK	(0xfffffff0)
#define S3C_GPIO_SPECIAL(x)	(S3C_GPIO_SPECIAL_MARK | (x))

#define S3C_GPIO_INPUT	(S3C_GPIO_SPECIAL(0))
#define S3C_GPIO_OUTPUT	(S3C_GPIO_SPECIAL(1))
#define S3C_GPIO_SFN(x)	(S3C_GPIO_SPECIAL(x))

#define samsung_gpio_is_cfg_special(_cfg) \
	(((_cfg) & S3C_GPIO_SPECIAL_MARK) == S3C_GPIO_SPECIAL_MARK)

extern int s3c_gpio_cfgpin(unsigned int pin, unsigned int to);

extern unsigned s3c_gpio_getcfg(unsigned int pin);

extern int s3c_gpio_cfgpin_range(unsigned int start, unsigned int nr,
				 unsigned int cfg);

#define S3C_GPIO_PULL_NONE	((__force samsung_gpio_pull_t)0x00)
#define S3C_GPIO_PULL_DOWN	((__force samsung_gpio_pull_t)0x01)
#define S3C_GPIO_PULL_UP	((__force samsung_gpio_pull_t)0x02)

extern int s3c_gpio_setpull(unsigned int pin, samsung_gpio_pull_t pull);

extern samsung_gpio_pull_t s3c_gpio_getpull(unsigned int pin);


extern int s3c_gpio_cfgall_range(unsigned int start, unsigned int nr,
				 unsigned int cfg, samsung_gpio_pull_t pull);

static inline int s3c_gpio_cfgrange_nopull(unsigned int pin, unsigned int size,
					   unsigned int cfg)
{
	return s3c_gpio_cfgall_range(pin, size, cfg, S3C_GPIO_PULL_NONE);
}

#define S5P_GPIO_DRVSTR_LV1	((__force s5p_gpio_drvstr_t)0x0)
#define S5P_GPIO_DRVSTR_LV2	((__force s5p_gpio_drvstr_t)0x2)
#define S5P_GPIO_DRVSTR_LV3	((__force s5p_gpio_drvstr_t)0x1)
#define S5P_GPIO_DRVSTR_LV4	((__force s5p_gpio_drvstr_t)0x3)

extern s5p_gpio_drvstr_t s5p_gpio_get_drvstr(unsigned int pin);

extern int s5p_gpio_set_drvstr(unsigned int pin, s5p_gpio_drvstr_t drvstr);

extern int s5p_register_gpio_interrupt(int pin);

#ifdef CONFIG_S5P_GPIO_INT
extern int s5p_register_gpioint_bank(int chain_irq, int start, int nr_groups);
#else
#define s5p_register_gpioint_bank(chain_irq, start, nr_groups) do { } while (0)
#endif

#endif 
