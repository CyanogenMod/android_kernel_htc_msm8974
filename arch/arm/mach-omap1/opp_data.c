/*
 *  linux/arch/arm/mach-omap1/opp_data.c
 *
 *  Copyright (C) 2004 - 2005 Nokia corporation
 *  Written by Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>
 *  Based on clocks.h by Tony Lindgren, Gordon McNutt and RidgeRun, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <plat/clkdev_omap.h>
#include "opp.h"

struct mpu_rate omap1_rate_table[] = {
	{ 216000000, 12000000, 216000000, 0x050d, 0x2910, 
			CK_1710 },
	{ 195000000, 13000000, 195000000, 0x050e, 0x2790, 
			CK_7XX },
	{ 192000000, 19200000, 192000000, 0x050f, 0x2510, 
			CK_16XX },
	{ 192000000, 12000000, 192000000, 0x050f, 0x2810, 
			CK_16XX },
	{  96000000, 12000000, 192000000, 0x055f, 0x2810, 
			CK_16XX },
	{  48000000, 12000000, 192000000, 0x0baf, 0x2810, 
			CK_16XX },
	{  24000000, 12000000, 192000000, 0x0fff, 0x2810, 
			CK_16XX },
	{ 182000000, 13000000, 182000000, 0x050e, 0x2710, 
			CK_7XX },
	{ 168000000, 12000000, 168000000, 0x010f, 0x2710, 
			CK_16XX|CK_7XX },
	{ 150000000, 12000000, 150000000, 0x010a, 0x2cb0, 
			CK_1510 },
	{ 120000000, 12000000, 120000000, 0x010a, 0x2510, 
			CK_16XX|CK_1510|CK_310|CK_7XX },
	{  96000000, 12000000,  96000000, 0x0005, 0x2410, 
			CK_16XX|CK_1510|CK_310|CK_7XX },
	{  60000000, 12000000,  60000000, 0x0005, 0x2290, 
			CK_16XX|CK_1510|CK_310|CK_7XX },
	{  30000000, 12000000,  60000000, 0x0555, 0x2290, 
			CK_16XX|CK_1510|CK_310|CK_7XX },
	{ 0, 0, 0, 0, 0 },
};

