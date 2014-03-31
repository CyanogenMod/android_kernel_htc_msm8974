/*
 * Samsung Platform - Keypad platform data definitions
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __PLAT_SAMSUNG_KEYPAD_H
#define __PLAT_SAMSUNG_KEYPAD_H

#include <linux/input/samsung-keypad.h>

extern void samsung_keypad_set_platdata(struct samsung_keypad_platdata *pd);

extern void samsung_keypad_cfg_gpio(unsigned int rows, unsigned int cols);

#endif 
