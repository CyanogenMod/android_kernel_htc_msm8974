/*
 *  arch/arm/plat-omap/include/mach/keypad.h
 *
 *  Copyright (C) 2006 Komal Shah <komal_shah802003@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ASMARM_ARCH_KEYPAD_H
#define ASMARM_ARCH_KEYPAD_H

#ifndef CONFIG_ARCH_OMAP1
#warning Please update the board to use matrix-keypad driver
#define omap_readw(reg)		0
#define omap_writew(val, reg)	do {} while (0)
#endif
#include <linux/input/matrix_keypad.h>

struct omap_kp_platform_data {
	int rows;
	int cols;
	const struct matrix_keymap_data *keymap_data;
	bool rep;
	unsigned long delay;
	bool dbounce;
	
	unsigned int *row_gpios;
	unsigned int *col_gpios;
};

#define GROUP_SHIFT	14
#define GROUP_0		(0 << GROUP_SHIFT)
#define GROUP_1		(1 << GROUP_SHIFT)
#define GROUP_2		(2 << GROUP_SHIFT)
#define GROUP_3		(3 << GROUP_SHIFT)
#define GROUP_MASK	GROUP_3
#if KEY_MAX & GROUP_MASK
#error Group bits in conflict with keynum bits
#endif


#endif

