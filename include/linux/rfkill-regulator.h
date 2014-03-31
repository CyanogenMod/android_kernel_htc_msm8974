/*
 * rfkill-regulator.c - Regulator consumer driver for rfkill
 *
 * Copyright (C) 2009  Guiming Zhuo <gmzhuo@gmail.com>
 * Copyright (C) 2011  Antonio Ospite <ospite@studenti.unina.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LINUX_RFKILL_REGULATOR_H
#define __LINUX_RFKILL_REGULATOR_H


#include <linux/rfkill.h>

struct rfkill_regulator_platform_data {
	char *name;             
	enum rfkill_type type;  
};

#endif 
