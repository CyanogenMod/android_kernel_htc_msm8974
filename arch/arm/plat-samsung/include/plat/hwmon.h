/* linux/arch/arm/plat-s3c/include/plat/hwmon.h
 *
 * Copyright 2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * S3C - HWMon interface for ADC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_ADC_HWMON_H
#define __ASM_ARCH_ADC_HWMON_H __FILE__

struct s3c_hwmon_chcfg {
	const char	*name;
	unsigned int	mult;
	unsigned int	div;
};

struct s3c_hwmon_pdata {
	struct s3c_hwmon_chcfg	*in[8];
};

extern void __init s3c_hwmon_set_platdata(struct s3c_hwmon_pdata *pd);

#endif 

