/* linux/arch/arm/plat-samsung/include/plat/gpio-cfg-helper.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * Samsung Platform - GPIO pin configuration helper definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#ifndef __PLAT_GPIO_CFG_HELPERS_H
#define __PLAT_GPIO_CFG_HELPERS_H __FILE__


static inline int samsung_gpio_do_setcfg(struct samsung_gpio_chip *chip,
					 unsigned int off, unsigned int config)
{
	return (chip->config->set_config)(chip, off, config);
}

static inline unsigned samsung_gpio_do_getcfg(struct samsung_gpio_chip *chip,
					      unsigned int off)
{
	return (chip->config->get_config)(chip, off);
}

static inline int samsung_gpio_do_setpull(struct samsung_gpio_chip *chip,
					  unsigned int off, samsung_gpio_pull_t pull)
{
	return (chip->config->set_pull)(chip, off, pull);
}

static inline samsung_gpio_pull_t samsung_gpio_do_getpull(struct samsung_gpio_chip *chip,
							  unsigned int off)
{
	return chip->config->get_pull(chip, off);
}


extern int s3c24xx_gpio_setpull_1up(struct samsung_gpio_chip *chip,
				    unsigned int off, samsung_gpio_pull_t pull);

extern int s3c24xx_gpio_setpull_1down(struct samsung_gpio_chip *chip,
				      unsigned int off, samsung_gpio_pull_t pull);

extern int samsung_gpio_setpull_updown(struct samsung_gpio_chip *chip,
				       unsigned int off, samsung_gpio_pull_t pull);

extern samsung_gpio_pull_t samsung_gpio_getpull_updown(struct samsung_gpio_chip *chip,
						       unsigned int off);

extern samsung_gpio_pull_t s3c24xx_gpio_getpull_1up(struct samsung_gpio_chip *chip,
						    unsigned int off);

extern samsung_gpio_pull_t s3c24xx_gpio_getpull_1down(struct samsung_gpio_chip *chip,
						      unsigned int off);

extern int s3c2443_gpio_setpull(struct samsung_gpio_chip *chip,
				unsigned int off, samsung_gpio_pull_t pull);

extern samsung_gpio_pull_t s3c2443_gpio_getpull(struct samsung_gpio_chip *chip,
						unsigned int off);

#endif 
