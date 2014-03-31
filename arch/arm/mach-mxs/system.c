/*
 * Copyright (C) 1999 ARM Limited
 * Copyright (C) 2000 Deep Blue Solutions Ltd
 * Copyright 2006-2007,2010 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2008 Juergen Beisert, kernel@pengutronix.de
 * Copyright 2009 Ilya Yanok, Emcraft Systems Ltd, yanok@emcraft.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>

#include <asm/proc-fns.h>
#include <asm/system_misc.h>

#include <mach/mxs.h>
#include <mach/common.h>

#define MX23_CLKCTRL_RESET_OFFSET	0x120
#define MX28_CLKCTRL_RESET_OFFSET	0x1e0
#define MXS_CLKCTRL_RESET_CHIP		(1 << 1)

#define MXS_MODULE_CLKGATE		(1 << 30)
#define MXS_MODULE_SFTRST		(1 << 31)

#define CLKCTRL_TIMEOUT		10	

static void __iomem *mxs_clkctrl_reset_addr;

void mxs_restart(char mode, const char *cmd)
{
	
	__mxs_setl(MXS_CLKCTRL_RESET_CHIP, mxs_clkctrl_reset_addr);

	pr_err("Failed to assert the chip reset\n");

	
	mdelay(50);

	
	soft_restart(0);
}

static int __init mxs_arch_reset_init(void)
{
	struct clk *clk;

	mxs_clkctrl_reset_addr = MXS_IO_ADDRESS(MXS_CLKCTRL_BASE_ADDR) +
				(cpu_is_mx23() ? MX23_CLKCTRL_RESET_OFFSET :
						 MX28_CLKCTRL_RESET_OFFSET);

	clk = clk_get_sys("rtc", NULL);
	if (!IS_ERR(clk))
		clk_prepare_enable(clk);

	return 0;
}
core_initcall(mxs_arch_reset_init);

static int clear_poll_bit(void __iomem *addr, u32 mask)
{
	int timeout = 0x400;

	
	__mxs_clrl(mask, addr);

	udelay(1);

	
	while ((__raw_readl(addr) & mask) && --timeout)
		;

	return !timeout;
}

int mxs_reset_block(void __iomem *reset_addr)
{
	int ret;
	int timeout = 0x400;

	
	ret = clear_poll_bit(reset_addr, MXS_MODULE_SFTRST);
	if (unlikely(ret))
		goto error;

	
	__mxs_clrl(MXS_MODULE_CLKGATE, reset_addr);

	
	__mxs_setl(MXS_MODULE_SFTRST, reset_addr);
	udelay(1);

	
	while ((!(__raw_readl(reset_addr) & MXS_MODULE_CLKGATE)) && --timeout)
		;
	if (unlikely(!timeout))
		goto error;

	
	ret = clear_poll_bit(reset_addr, MXS_MODULE_SFTRST);
	if (unlikely(ret))
		goto error;

	
	ret = clear_poll_bit(reset_addr, MXS_MODULE_CLKGATE);
	if (unlikely(ret))
		goto error;

	return 0;

error:
	pr_err("%s(%p): module reset timeout\n", __func__, reset_addr);
	return -ETIMEDOUT;
}
EXPORT_SYMBOL(mxs_reset_block);

int mxs_clkctrl_timeout(unsigned int reg_offset, unsigned int mask)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(CLKCTRL_TIMEOUT);
	while (readl_relaxed(MXS_IO_ADDRESS(MXS_CLKCTRL_BASE_ADDR)
						+ reg_offset) & mask) {
		if (time_after(jiffies, timeout)) {
			pr_err("Timeout at CLKCTRL + 0x%x\n", reg_offset);
			return -ETIMEDOUT;
		}
	}

	return 0;
}
