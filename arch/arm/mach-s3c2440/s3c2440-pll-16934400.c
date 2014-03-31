/* arch/arm/mach-s3c2440/s3c2440-pll-16934400.c
 *
 * Copyright (c) 2006-2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *	Vincent Sanders <vince@arm.linux.org.uk>
 *
 * S3C2440/S3C2442 CPU PLL tables (16.93444MHz Crystal)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <plat/cpu.h>
#include <plat/cpu-freq-core.h>

static struct cpufreq_frequency_table s3c2440_plls_169344[] __initdata = {
	{ .frequency = 78019200,	.index = PLLVAL(121, 5, 3), 	}, 	
	{ .frequency = 84067200,	.index = PLLVAL(131, 5, 3), 	}, 	
	{ .frequency = 90115200,	.index = PLLVAL(141, 5, 3), 	}, 	
	{ .frequency = 96163200,	.index = PLLVAL(151, 5, 3), 	}, 	
	{ .frequency = 102135600,	.index = PLLVAL(185, 6, 3), 	}, 	
	{ .frequency = 108259200,	.index = PLLVAL(171, 5, 3), 	}, 	
	{ .frequency = 114307200,	.index = PLLVAL(127, 3, 3), 	}, 	
	{ .frequency = 120234240,	.index = PLLVAL(134, 3, 3), 	}, 	
	{ .frequency = 126161280,	.index = PLLVAL(141, 3, 3), 	}, 	
	{ .frequency = 132088320,	.index = PLLVAL(148, 3, 3), 	}, 	
	{ .frequency = 138015360,	.index = PLLVAL(155, 3, 3), 	}, 	
	{ .frequency = 144789120,	.index = PLLVAL(163, 3, 3), 	}, 	
	{ .frequency = 150100363,	.index = PLLVAL(187, 9, 2), 	}, 	
	{ .frequency = 156038400,	.index = PLLVAL(121, 5, 2), 	}, 	
	{ .frequency = 162086400,	.index = PLLVAL(126, 5, 2), 	}, 	
	{ .frequency = 168134400,	.index = PLLVAL(131, 5, 2), 	}, 	
	{ .frequency = 174048000,	.index = PLLVAL(177, 7, 2), 	}, 	
	{ .frequency = 180230400,	.index = PLLVAL(141, 5, 2), 	}, 	
	{ .frequency = 186278400,	.index = PLLVAL(124, 4, 2), 	}, 	
	{ .frequency = 192326400,	.index = PLLVAL(151, 5, 2), 	}, 	
	{ .frequency = 198132480,	.index = PLLVAL(109, 3, 2), 	}, 	
	{ .frequency = 204271200,	.index = PLLVAL(185, 6, 2), 	}, 	
	{ .frequency = 210268800,	.index = PLLVAL(141, 4, 2), 	}, 	
	{ .frequency = 216518400,	.index = PLLVAL(171, 5, 2), 	}, 	
	{ .frequency = 222264000,	.index = PLLVAL(97, 2, 2), 	}, 	
	{ .frequency = 228614400,	.index = PLLVAL(127, 3, 2), 	}, 	
	{ .frequency = 234259200,	.index = PLLVAL(158, 4, 2), 	}, 	
	{ .frequency = 240468480,	.index = PLLVAL(134, 3, 2), 	}, 	
	{ .frequency = 246960000,	.index = PLLVAL(167, 4, 2), 	}, 	
	{ .frequency = 252322560,	.index = PLLVAL(141, 3, 2), 	}, 	
	{ .frequency = 258249600,	.index = PLLVAL(114, 2, 2), 	}, 	
	{ .frequency = 264176640,	.index = PLLVAL(148, 3, 2), 	}, 	
	{ .frequency = 270950400,	.index = PLLVAL(120, 2, 2), 	}, 	
	{ .frequency = 276030720,	.index = PLLVAL(155, 3, 2), 	}, 	
	{ .frequency = 282240000,	.index = PLLVAL(92, 1, 2), 	}, 	
	{ .frequency = 289578240,	.index = PLLVAL(163, 3, 2), 	}, 	
	{ .frequency = 294235200,	.index = PLLVAL(131, 2, 2), 	}, 	
	{ .frequency = 300200727,	.index = PLLVAL(187, 9, 1), 	}, 	
	{ .frequency = 306358690,	.index = PLLVAL(191, 9, 1), 	}, 	
	{ .frequency = 312076800,	.index = PLLVAL(121, 5, 1), 	}, 	
	{ .frequency = 318366720,	.index = PLLVAL(86, 3, 1), 	}, 	
	{ .frequency = 324172800,	.index = PLLVAL(126, 5, 1), 	}, 	
	{ .frequency = 330220800,	.index = PLLVAL(109, 4, 1), 	}, 	
	{ .frequency = 336268800,	.index = PLLVAL(131, 5, 1), 	}, 	
	{ .frequency = 342074880,	.index = PLLVAL(93, 3, 1), 	}, 	
	{ .frequency = 348096000,	.index = PLLVAL(177, 7, 1), 	}, 	
	{ .frequency = 355622400,	.index = PLLVAL(118, 4, 1), 	}, 	
	{ .frequency = 360460800,	.index = PLLVAL(141, 5, 1), 	}, 	
	{ .frequency = 366206400,	.index = PLLVAL(165, 6, 1), 	}, 	
	{ .frequency = 372556800,	.index = PLLVAL(124, 4, 1), 	}, 	
	{ .frequency = 378201600,	.index = PLLVAL(126, 4, 1), 	}, 	
	{ .frequency = 384652800,	.index = PLLVAL(151, 5, 1), 	}, 	
	{ .frequency = 391608000,	.index = PLLVAL(177, 6, 1), 	}, 	
	{ .frequency = 396264960,	.index = PLLVAL(109, 3, 1), 	}, 	
	{ .frequency = 402192000,	.index = PLLVAL(87, 2, 1), 	}, 	
};

static int s3c2440_plls169344_add(struct device *dev,
				  struct subsys_interface *sif)
{
	struct clk *xtal_clk;
	unsigned long xtal;

	xtal_clk = clk_get(NULL, "xtal");
	if (IS_ERR(xtal_clk))
		return PTR_ERR(xtal_clk);

	xtal = clk_get_rate(xtal_clk);
	clk_put(xtal_clk);

	if (xtal == 169344000) {
		printk(KERN_INFO "Using PLL table for 16.9344MHz crystal\n");
		return s3c_plltab_register(s3c2440_plls_169344,
					   ARRAY_SIZE(s3c2440_plls_169344));
	}

	return 0;
}

static struct subsys_interface s3c2440_plls169344_interface = {
	.name		= "s3c2440_plls169344",
	.subsys		= &s3c2440_subsys,
	.add_dev	= s3c2440_plls169344_add,
};

static int __init s3c2440_pll_16934400(void)
{
	return subsys_interface_register(&s3c2440_plls169344_interface);
}

arch_initcall(s3c2440_pll_16934400);

static struct subsys_interface s3c2442_plls169344_interface = {
	.name		= "s3c2442_plls169344",
	.subsys		= &s3c2442_subsys,
	.add_dev	= s3c2440_plls169344_add,
};

static int __init s3c2442_pll_16934400(void)
{
	return subsys_interface_register(&s3c2442_plls169344_interface);
}

arch_initcall(s3c2442_pll_16934400);
