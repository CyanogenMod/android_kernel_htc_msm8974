/*
 * Copyright 2005-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2008 Juergen Beisert, kernel@pengutronix.de
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __MACH_MXS_CLOCK_H__
#define __MACH_MXS_CLOCK_H__

#ifndef __ASSEMBLY__
#include <linux/list.h>

struct module;

struct clk {
	int id;
	
	struct clk *parent;
	
	__s8 usecount;
	
	u8 enable_shift;
	
	void __iomem *enable_reg;
	u32 flags;
	
	unsigned long (*get_rate) (struct clk *);
	int (*set_rate) (struct clk *, unsigned long);
	unsigned long (*round_rate) (struct clk *, unsigned long);
	int (*enable) (struct clk *);
	void (*disable) (struct clk *);
	
	int (*set_parent) (struct clk *, struct clk *);
};

int clk_register(struct clk *clk);
void clk_unregister(struct clk *clk);

#endif 
#endif 
