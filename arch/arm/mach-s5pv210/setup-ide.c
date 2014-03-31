/* linux/arch/arm/mach-s5pv210/setup-ide.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * S5PV210 setup information for IDE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/gpio.h>

#include <plat/gpio-cfg.h>

static void s5pv210_ide_cfg_gpios(unsigned int base, unsigned int nr)
{
	s3c_gpio_cfgrange_nopull(base, nr, S3C_GPIO_SFN(4));

	for (; nr > 0; nr--, base++)
		s5p_gpio_set_drvstr(base, S5P_GPIO_DRVSTR_LV4);
}

void s5pv210_ide_setup_gpio(void)
{
	
	s5pv210_ide_cfg_gpios(S5PV210_GPJ0(0), 8);

	
	s5pv210_ide_cfg_gpios(S5PV210_GPJ2(0), 8);

	
	s5pv210_ide_cfg_gpios(S5PV210_GPJ3(0), 8);

	
	s5pv210_ide_cfg_gpios(S5PV210_GPJ4(0), 4);
}
