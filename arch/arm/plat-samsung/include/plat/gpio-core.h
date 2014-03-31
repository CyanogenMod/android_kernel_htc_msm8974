/* linux/arch/arm/plat-s3c/include/plat/gpio-core.h
 *
 * Copyright 2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C Platform - GPIO core
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#define GPIOCON_OFF	(0x00)
#define GPIODAT_OFF	(0x04)

#define con_4bit_shift(__off) ((__off) * 4)


struct samsung_gpio_chip;

struct samsung_gpio_pm {
	void (*save)(struct samsung_gpio_chip *chip);
	void (*resume)(struct samsung_gpio_chip *chip);
};

struct samsung_gpio_cfg;

struct samsung_gpio_chip {
	struct gpio_chip	chip;
	struct samsung_gpio_cfg	*config;
	struct samsung_gpio_pm	*pm;
	void __iomem		*base;
	int			irq_base;
	int			group;
	spinlock_t		 lock;
#ifdef CONFIG_PM
	u32			pm_save[4];
#endif
};

static inline struct samsung_gpio_chip *to_samsung_gpio(struct gpio_chip *gpc)
{
	return container_of(gpc, struct samsung_gpio_chip, chip);
}

extern int samsung_gpiolib_to_irq(struct gpio_chip *chip, unsigned int offset);

extern struct samsung_gpio_cfg s3c24xx_gpiocfg_default;

#ifdef CONFIG_S3C_GPIO_TRACK
extern struct samsung_gpio_chip *s3c_gpios[S3C_GPIO_END];

static inline struct samsung_gpio_chip *samsung_gpiolib_getchip(unsigned int chip)
{
	return (chip < S3C_GPIO_END) ? s3c_gpios[chip] : NULL;
}
#else

#include <mach/gpio-track.h>

static inline void s3c_gpiolib_track(struct samsung_gpio_chip *chip) { }
#endif

#ifdef CONFIG_PM
extern struct samsung_gpio_pm samsung_gpio_pm_1bit;
extern struct samsung_gpio_pm samsung_gpio_pm_2bit;
extern struct samsung_gpio_pm samsung_gpio_pm_4bit;
#define __gpio_pm(x) x
#else
#define samsung_gpio_pm_1bit NULL
#define samsung_gpio_pm_2bit NULL
#define samsung_gpio_pm_4bit NULL
#define __gpio_pm(x) NULL

#endif 

#define samsung_gpio_lock(_oc, _fl) spin_lock_irqsave(&(_oc)->lock, _fl)
#define samsung_gpio_unlock(_oc, _fl) spin_unlock_irqrestore(&(_oc)->lock, _fl)
