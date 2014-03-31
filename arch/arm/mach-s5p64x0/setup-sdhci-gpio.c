/* linux/arch/arm/mach-s5p64x0/setup-sdhci-gpio.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5P64X0 - Helper functions for setting up SDHCI device(s) GPIO (HSMMC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>

#include <plat/gpio-cfg.h>
#include <plat/sdhci.h>
#include <plat/cpu.h>

void s5p64x0_setup_sdhci0_cfg_gpio(struct platform_device *dev, int width)
{
	struct s3c_sdhci_platdata *pdata = dev->dev.platform_data;

	
	if (soc_is_s5p6450())
		s3c_gpio_cfgrange_nopull(S5P6450_GPG(0), 2 + width,
					 S3C_GPIO_SFN(2));
	else
		s3c_gpio_cfgrange_nopull(S5P6440_GPG(0), 2 + width,
					 S3C_GPIO_SFN(2));

	
	if (pdata->cd_type == S3C_SDHCI_CD_INTERNAL) {
		if (soc_is_s5p6450()) {
			s3c_gpio_setpull(S5P6450_GPG(6), S3C_GPIO_PULL_UP);
			s3c_gpio_cfgpin(S5P6450_GPG(6), S3C_GPIO_SFN(2));
		} else {
			s3c_gpio_setpull(S5P6440_GPG(6), S3C_GPIO_PULL_UP);
			s3c_gpio_cfgpin(S5P6440_GPG(6), S3C_GPIO_SFN(2));
		}
	}
}

void s5p64x0_setup_sdhci1_cfg_gpio(struct platform_device *dev, int width)
{
	struct s3c_sdhci_platdata *pdata = dev->dev.platform_data;

	
	if (soc_is_s5p6450())
		s3c_gpio_cfgrange_nopull(S5P6450_GPH(0), 2, S3C_GPIO_SFN(2));
	else
		s3c_gpio_cfgrange_nopull(S5P6440_GPH(0), 2 , S3C_GPIO_SFN(2));

	switch (width) {
	case 8:
		
		if (soc_is_s5p6450())
			s3c_gpio_cfgrange_nopull(S5P6450_GPH(6), 4,
						 S3C_GPIO_SFN(2));
		else
			s3c_gpio_cfgrange_nopull(S5P6440_GPH(6), 4,
						 S3C_GPIO_SFN(2));
	case 4:
		
		if (soc_is_s5p6450())
			s3c_gpio_cfgrange_nopull(S5P6450_GPH(2), 4,
						 S3C_GPIO_SFN(2));
		else
			s3c_gpio_cfgrange_nopull(S5P6440_GPH(2), 4,
						 S3C_GPIO_SFN(2));
	default:
		break;
	}

	
	if (pdata->cd_type == S3C_SDHCI_CD_INTERNAL) {
		if (soc_is_s5p6450()) {
			s3c_gpio_setpull(S5P6450_GPG(6), S3C_GPIO_PULL_UP);
			s3c_gpio_cfgpin(S5P6450_GPG(6), S3C_GPIO_SFN(3));
		} else {
			s3c_gpio_setpull(S5P6440_GPG(6), S3C_GPIO_PULL_UP);
			s3c_gpio_cfgpin(S5P6440_GPG(6), S3C_GPIO_SFN(3));
		}
	}
}

void s5p6440_setup_sdhci2_cfg_gpio(struct platform_device *dev, int width)
{
	
	s3c_gpio_cfgrange_nopull(S5P6440_GPC(4), 2, S3C_GPIO_SFN(3));

	
	s3c_gpio_cfgrange_nopull(S5P6440_GPH(6), 4, S3C_GPIO_SFN(3));
}

void s5p6450_setup_sdhci2_cfg_gpio(struct platform_device *dev, int width)
{
	
	s3c_gpio_cfgrange_nopull(S5P6450_GPG(7), 2 + width, S3C_GPIO_SFN(3));
}
