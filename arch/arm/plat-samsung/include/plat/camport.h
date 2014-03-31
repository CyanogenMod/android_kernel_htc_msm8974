/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * S5P series camera interface helper functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PLAT_SAMSUNG_CAMPORT_H_
#define __PLAT_SAMSUNG_CAMPORT_H_ __FILE__

enum s5p_camport_id {
	S5P_CAMPORT_A,
	S5P_CAMPORT_B,
};

int s5pv210_fimc_setup_gpio(enum s5p_camport_id id);
int exynos4_fimc_setup_gpio(enum s5p_camport_id id);

#endif 
