/*
 * include/linux/platform_data/pxa_sdhci.h
 *
 * Copyright 2010 Marvell
 *	Zhangfei Gao <zhangfei.gao@marvell.com>
 *
 * PXA Platform - SDHCI platform data definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _PXA_SDHCI_H_
#define _PXA_SDHCI_H_

#define PXA_FLAG_ENABLE_CLOCK_GATING (1<<0)
#define PXA_FLAG_CARD_PERMANENT	(1<<1)
#define PXA_FLAG_SD_8_BIT_CAPABLE_SLOT (1<<2)

struct sdhci_pxa_platdata {
	unsigned int	flags;
	unsigned int	clk_delay_cycles;
	unsigned int	clk_delay_sel;
	bool		clk_delay_enable;
	unsigned int	ext_cd_gpio;
	bool		ext_cd_gpio_invert;
	unsigned int	max_speed;
	unsigned int	host_caps;
	unsigned int	quirks;
	unsigned int	pm_caps;
};

struct sdhci_pxa {
	u8	clk_enable;
	u8	power_mode;
};
#endif 
