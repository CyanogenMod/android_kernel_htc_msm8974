/*
 * OMAP3517/3505-specific clock framework functions
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Copyright (C) 2011 Nokia Corporation
 *
 * Ranjith Lohithakshan
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
#include "clock3517.h"
#include "cm2xxx_3xxx.h"
#include "cm-regbits-34xx.h"

#define AM35XX_IPSS_ICK_MASK			0xF
#define AM35XX_IPSS_ICK_EN_ACK_OFFSET 		0x4
#define AM35XX_IPSS_ICK_FCK_OFFSET		0x8
#define AM35XX_IPSS_CLK_IDLEST_VAL		0

static void am35xx_clk_find_idlest(struct clk *clk,
					    void __iomem **idlest_reg,
					    u8 *idlest_bit,
					    u8 *idlest_val)
{
	*idlest_reg = (__force void __iomem *)(clk->enable_reg);
	*idlest_bit = clk->enable_bit + AM35XX_IPSS_ICK_EN_ACK_OFFSET;
	*idlest_val = AM35XX_IPSS_CLK_IDLEST_VAL;
}

static void am35xx_clk_find_companion(struct clk *clk, void __iomem **other_reg,
					    u8 *other_bit)
{
	*other_reg = (__force void __iomem *)(clk->enable_reg);
	if (clk->enable_bit & AM35XX_IPSS_ICK_MASK)
		*other_bit = clk->enable_bit + AM35XX_IPSS_ICK_FCK_OFFSET;
	else
		*other_bit = clk->enable_bit - AM35XX_IPSS_ICK_FCK_OFFSET;
}

const struct clkops clkops_am35xx_ipss_module_wait = {
	.enable		= omap2_dflt_clk_enable,
	.disable	= omap2_dflt_clk_disable,
	.find_idlest	= am35xx_clk_find_idlest,
	.find_companion	= am35xx_clk_find_companion,
};

static void am35xx_clk_ipss_find_idlest(struct clk *clk,
					    void __iomem **idlest_reg,
					    u8 *idlest_bit,
					    u8 *idlest_val)
{
	u32 r;

	r = (((__force u32)clk->enable_reg & ~0xf0) | 0x20);
	*idlest_reg = (__force void __iomem *)r;
	*idlest_bit = AM35XX_ST_IPSS_SHIFT;
	*idlest_val = OMAP34XX_CM_IDLEST_VAL;
}

const struct clkops clkops_am35xx_ipss_wait = {
	.enable		= omap2_dflt_clk_enable,
	.disable	= omap2_dflt_clk_disable,
	.find_idlest	= am35xx_clk_ipss_find_idlest,
	.find_companion	= omap2_clk_dflt_find_companion,
	.allow_idle	= omap2_clkt_iclk_allow_idle,
	.deny_idle	= omap2_clkt_iclk_deny_idle,
};


