/*
 *  linux/arch/arm/mach-nomadik/clock.c
 *
 *  Copyright (C) 2009 Alessandro Rubini
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include "clock.h"

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

int clk_enable(struct clk *clk)
{
	return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
}
EXPORT_SYMBOL(clk_disable);

static struct clk clk_24 = {
	.rate = 2400000,
};

static struct clk clk_48 = {
	.rate = 48 * 1000 * 1000,
};

static struct clk clk_default;

#define CLK(_clk, dev)				\
	{					\
		.clk		= _clk,		\
		.dev_id		= dev,		\
	}

static struct clk_lookup lookups[] = {
	{
		.con_id		= "apb_pclk",
		.clk		= &clk_default,
	},
	CLK(&clk_24, "mtu0"),
	CLK(&clk_24, "mtu1"),
	CLK(&clk_48, "uart0"),
	CLK(&clk_48, "uart1"),
	CLK(&clk_default, "gpio.0"),
	CLK(&clk_default, "gpio.1"),
	CLK(&clk_default, "gpio.2"),
	CLK(&clk_default, "gpio.3"),
	CLK(&clk_default, "rng"),
};

int __init clk_init(void)
{
	clkdev_add_table(lookups, ARRAY_SIZE(lookups));
	return 0;
}
