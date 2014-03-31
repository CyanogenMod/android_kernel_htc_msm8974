/*
 * OMAP36xx-specific clkops
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Copyright (C) 2010 Nokia Corporation
 *
 * Mike Turquette
 * Vijaykumar GN
 * Paul Walmsley
 *
 * Parts of this code are based on code written by
 * Richard Woodruff, Tony Lindgren, Tuukka Tikkanen, Karthik Dasu,
 * Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <plat/clock.h>

#include "clock.h"
#include "clock36xx.h"


static int omap36xx_pwrdn_clk_enable_with_hsdiv_restore(struct clk *clk)
{
	u32 dummy_v, orig_v, clksel_shift;
	int ret;

	
	ret = omap2_dflt_clk_enable(clk);

	
	if (!ret) {
		clksel_shift = __ffs(clk->parent->clksel_mask);
		orig_v = __raw_readl(clk->parent->clksel_reg);
		dummy_v = orig_v;

		
		dummy_v ^= (1 << clksel_shift);
		__raw_writel(dummy_v, clk->parent->clksel_reg);

		
		__raw_writel(orig_v, clk->parent->clksel_reg);
	}

	return ret;
}

const struct clkops clkops_omap36xx_pwrdn_with_hsdiv_wait_restore = {
	.enable		= omap36xx_pwrdn_clk_enable_with_hsdiv_restore,
	.disable	= omap2_dflt_clk_disable,
	.find_companion	= omap2_clk_dflt_find_companion,
	.find_idlest	= omap2_clk_dflt_find_idlest,
};
