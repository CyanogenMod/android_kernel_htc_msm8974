/* linux/arch/arm/plat-samsung/include/plat/clock-clksrc.h
 *
 * Parts taken from arch/arm/plat-s3c64xx/clock.c
 *	Copyright 2008 Openmoko, Inc.
 *	Copyright 2008 Simtec Electronics
 *		Ben Dooks <ben@simtec.co.uk>
 *		http://armlinux.simtec.co.uk/
 *
 * Copyright 2009 Ben Dooks <ben-linux@fluff.org>
 * Copyright 2009 Harald Welte
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct clksrc_sources {
	unsigned int	nr_sources;
	struct clk	**sources;
};

struct clksrc_reg {
	void __iomem		*reg;
	unsigned short		shift;
	unsigned short		size;
};

struct clksrc_clk {
	struct clk		clk;
	struct clksrc_sources	*sources;

	struct clksrc_reg	reg_src;
	struct clksrc_reg	reg_div;
};

extern void s3c_set_clksrc(struct clksrc_clk *clk, bool announce);

extern void s3c_register_clksrc(struct clksrc_clk *srcs, int size);
