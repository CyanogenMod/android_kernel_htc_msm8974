
/* linux/arch/arm/plat-s3c/pm-gpio.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * S3C series GPIO PM code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <plat/gpio-core.h>
#include <plat/pm.h>


#define OFFS_CON	(0x00)
#define OFFS_DAT	(0x04)
#define OFFS_UP		(0x08)

static void samsung_gpio_pm_1bit_save(struct samsung_gpio_chip *chip)
{
	chip->pm_save[0] = __raw_readl(chip->base + OFFS_CON);
	chip->pm_save[1] = __raw_readl(chip->base + OFFS_DAT);
}

static void samsung_gpio_pm_1bit_resume(struct samsung_gpio_chip *chip)
{
	void __iomem *base = chip->base;
	u32 old_gpcon = __raw_readl(base + OFFS_CON);
	u32 old_gpdat = __raw_readl(base + OFFS_DAT);
	u32 gps_gpcon = chip->pm_save[0];
	u32 gps_gpdat = chip->pm_save[1];
	u32 gpcon;


	

	gpcon = old_gpcon | gps_gpcon;
	__raw_writel(gpcon, base + OFFS_CON);

	

	__raw_writel(gps_gpdat, base + OFFS_DAT);
	__raw_writel(gps_gpcon, base + OFFS_CON);

	S3C_PMDBG("%s: CON %08x => %08x, DAT %08x => %08x\n",
		  chip->chip.label, old_gpcon, gps_gpcon, old_gpdat, gps_gpdat);
}

struct samsung_gpio_pm samsung_gpio_pm_1bit = {
	.save	= samsung_gpio_pm_1bit_save,
	.resume = samsung_gpio_pm_1bit_resume,
};

static void samsung_gpio_pm_2bit_save(struct samsung_gpio_chip *chip)
{
	chip->pm_save[0] = __raw_readl(chip->base + OFFS_CON);
	chip->pm_save[1] = __raw_readl(chip->base + OFFS_DAT);
	chip->pm_save[2] = __raw_readl(chip->base + OFFS_UP);
}


static inline int is_sfn(unsigned long con)
{
	return con >= 2;
}


static inline int is_in(unsigned long con)
{
	return con == 0;
}


static inline int is_out(unsigned long con)
{
	return con == 1;
}

static void samsung_gpio_pm_2bit_resume(struct samsung_gpio_chip *chip)
{
	void __iomem *base = chip->base;
	u32 old_gpcon = __raw_readl(base + OFFS_CON);
	u32 old_gpdat = __raw_readl(base + OFFS_DAT);
	u32 gps_gpcon = chip->pm_save[0];
	u32 gps_gpdat = chip->pm_save[1];
	u32 gpcon, old, new, mask;
	u32 change_mask = 0x0;
	int nr;

	
	__raw_writel(chip->pm_save[2], base + OFFS_UP);


	for (nr = 0, mask = 0x03; nr < 32; nr += 2, mask <<= 2) {
		old = (old_gpcon & mask) >> nr;
		new = (gps_gpcon & mask) >> nr;

		

		if (old == new)
			continue;

		

		if (is_sfn(old) && is_sfn(new))
			continue;

		

		if (is_in(old) && is_out(new))
			continue;

		

		if (is_sfn(old) && is_out(new))
			continue;


		change_mask |= mask;
	}


	

	gpcon = old_gpcon & ~change_mask;
	gpcon |= gps_gpcon & change_mask;

	__raw_writel(gpcon, base + OFFS_CON);

	

	__raw_writel(gps_gpdat, base + OFFS_DAT);
	__raw_writel(gps_gpcon, base + OFFS_CON);

	S3C_PMDBG("%s: CON %08x => %08x, DAT %08x => %08x\n",
		  chip->chip.label, old_gpcon, gps_gpcon, old_gpdat, gps_gpdat);
}

struct samsung_gpio_pm samsung_gpio_pm_2bit = {
	.save	= samsung_gpio_pm_2bit_save,
	.resume = samsung_gpio_pm_2bit_resume,
};

#if defined(CONFIG_ARCH_S3C64XX) || defined(CONFIG_PLAT_S5P)
static void samsung_gpio_pm_4bit_save(struct samsung_gpio_chip *chip)
{
	chip->pm_save[1] = __raw_readl(chip->base + OFFS_CON);
	chip->pm_save[2] = __raw_readl(chip->base + OFFS_DAT);
	chip->pm_save[3] = __raw_readl(chip->base + OFFS_UP);

	if (chip->chip.ngpio > 8)
		chip->pm_save[0] = __raw_readl(chip->base - 4);
}

static u32 samsung_gpio_pm_4bit_mask(u32 old_gpcon, u32 gps_gpcon)
{
	u32 old, new, mask;
	u32 change_mask = 0x0;
	int nr;

	for (nr = 0, mask = 0x0f; nr < 16; nr += 4, mask <<= 4) {
		old = (old_gpcon & mask) >> nr;
		new = (gps_gpcon & mask) >> nr;

		

		if (old == new)
			continue;

		

		if (is_sfn(old) && is_sfn(new))
			continue;

		

		if (is_in(old) && is_out(new))
			continue;

		

		if (is_sfn(old) && is_out(new))
			continue;


		change_mask |= mask;
	}

	return change_mask;
}

static void samsung_gpio_pm_4bit_con(struct samsung_gpio_chip *chip, int index)
{
	void __iomem *con = chip->base + (index * 4);
	u32 old_gpcon = __raw_readl(con);
	u32 gps_gpcon = chip->pm_save[index + 1];
	u32 gpcon, mask;

	mask = samsung_gpio_pm_4bit_mask(old_gpcon, gps_gpcon);

	gpcon = old_gpcon & ~mask;
	gpcon |= gps_gpcon & mask;

	__raw_writel(gpcon, con);
}

static void samsung_gpio_pm_4bit_resume(struct samsung_gpio_chip *chip)
{
	void __iomem *base = chip->base;
	u32 old_gpcon[2];
	u32 old_gpdat = __raw_readl(base + OFFS_DAT);
	u32 gps_gpdat = chip->pm_save[2];

	

	old_gpcon[0] = 0;
	old_gpcon[1] = __raw_readl(base + OFFS_CON);

	samsung_gpio_pm_4bit_con(chip, 0);
	if (chip->chip.ngpio > 8) {
		old_gpcon[0] = __raw_readl(base - 4);
		samsung_gpio_pm_4bit_con(chip, -1);
	}

	

	__raw_writel(chip->pm_save[2], base + OFFS_DAT);
	__raw_writel(chip->pm_save[1], base + OFFS_CON);
	if (chip->chip.ngpio > 8)
		__raw_writel(chip->pm_save[0], base - 4);

	__raw_writel(chip->pm_save[2], base + OFFS_DAT);
	__raw_writel(chip->pm_save[3], base + OFFS_UP);

	if (chip->chip.ngpio > 8) {
		S3C_PMDBG("%s: CON4 %08x,%08x => %08x,%08x, DAT %08x => %08x\n",
			  chip->chip.label, old_gpcon[0], old_gpcon[1],
			  __raw_readl(base - 4),
			  __raw_readl(base + OFFS_CON),
			  old_gpdat, gps_gpdat);
	} else
		S3C_PMDBG("%s: CON4 %08x => %08x, DAT %08x => %08x\n",
			  chip->chip.label, old_gpcon[1],
			  __raw_readl(base + OFFS_CON),
			  old_gpdat, gps_gpdat);
}

struct samsung_gpio_pm samsung_gpio_pm_4bit = {
	.save	= samsung_gpio_pm_4bit_save,
	.resume = samsung_gpio_pm_4bit_resume,
};
#endif 

static void samsung_pm_save_gpio(struct samsung_gpio_chip *ourchip)
{
	struct samsung_gpio_pm *pm = ourchip->pm;

	if (pm == NULL || pm->save == NULL)
		S3C_PMDBG("%s: no pm for %s\n", __func__, ourchip->chip.label);
	else
		pm->save(ourchip);
}

void samsung_pm_save_gpios(void)
{
	struct samsung_gpio_chip *ourchip;
	unsigned int gpio_nr;

	for (gpio_nr = 0; gpio_nr < S3C_GPIO_END;) {
		ourchip = samsung_gpiolib_getchip(gpio_nr);
		if (!ourchip) {
			gpio_nr++;
			continue;
		}

		samsung_pm_save_gpio(ourchip);

		S3C_PMDBG("%s: save %08x,%08x,%08x,%08x\n",
			  ourchip->chip.label,
			  ourchip->pm_save[0],
			  ourchip->pm_save[1],
			  ourchip->pm_save[2],
			  ourchip->pm_save[3]);

		gpio_nr += ourchip->chip.ngpio;
		gpio_nr += CONFIG_S3C_GPIO_SPACE;
	}
}

static void samsung_pm_resume_gpio(struct samsung_gpio_chip *ourchip)
{
	struct samsung_gpio_pm *pm = ourchip->pm;

	if (pm == NULL || pm->resume == NULL)
		S3C_PMDBG("%s: no pm for %s\n", __func__, ourchip->chip.label);
	else
		pm->resume(ourchip);
}

void samsung_pm_restore_gpios(void)
{
	struct samsung_gpio_chip *ourchip;
	unsigned int gpio_nr;

	for (gpio_nr = 0; gpio_nr < S3C_GPIO_END;) {
		ourchip = samsung_gpiolib_getchip(gpio_nr);
		if (!ourchip) {
			gpio_nr++;
			continue;
		}

		samsung_pm_resume_gpio(ourchip);

		gpio_nr += ourchip->chip.ngpio;
		gpio_nr += CONFIG_S3C_GPIO_SPACE;
	}
}
