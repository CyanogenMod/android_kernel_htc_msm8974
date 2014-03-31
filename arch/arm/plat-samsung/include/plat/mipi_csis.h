/*
 * Copyright (C) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * S5P series MIPI CSI slave device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PLAT_SAMSUNG_MIPI_CSIS_H_
#define __PLAT_SAMSUNG_MIPI_CSIS_H_ __FILE__

struct platform_device;

struct s5p_platform_mipi_csis {
	unsigned long clk_rate;
	u8 lanes;
	u8 alignment;
	u8 hs_settle;
	bool fixed_phy_vdd;
	int (*phy_enable)(struct platform_device *pdev, bool on);
};

int s5p_csis_phy_enable(struct platform_device *pdev, bool on);

#endif 
