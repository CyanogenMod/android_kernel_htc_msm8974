/* linux/arch/arm/mach-s3c2443/clock.c
 *
 * Copyright (c) 2007, 2010 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2443 Clock control support
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <linux/init.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/serial_core.h>
#include <linux/io.h>

#include <asm/mach/map.h>

#include <mach/hardware.h>

#include <mach/regs-s3c2443-clock.h>

#include <plat/cpu-freq.h>

#include <plat/s3c2443.h>
#include <plat/clock.h>
#include <plat/clock-clksrc.h>
#include <plat/cpu.h>




static unsigned int armdiv[16] = {
	[S3C2443_CLKDIV0_ARMDIV_1 >> S3C2443_CLKDIV0_ARMDIV_SHIFT]	= 1,
	[S3C2443_CLKDIV0_ARMDIV_2 >> S3C2443_CLKDIV0_ARMDIV_SHIFT]	= 2,
	[S3C2443_CLKDIV0_ARMDIV_3 >> S3C2443_CLKDIV0_ARMDIV_SHIFT]	= 3,
	[S3C2443_CLKDIV0_ARMDIV_4 >> S3C2443_CLKDIV0_ARMDIV_SHIFT]	= 4,
	[S3C2443_CLKDIV0_ARMDIV_6 >> S3C2443_CLKDIV0_ARMDIV_SHIFT]	= 6,
	[S3C2443_CLKDIV0_ARMDIV_8 >> S3C2443_CLKDIV0_ARMDIV_SHIFT]	= 8,
	[S3C2443_CLKDIV0_ARMDIV_12 >> S3C2443_CLKDIV0_ARMDIV_SHIFT]	= 12,
	[S3C2443_CLKDIV0_ARMDIV_16 >> S3C2443_CLKDIV0_ARMDIV_SHIFT]	= 16,
};


static struct clksrc_clk clk_hsspi = {
	.clk	= {
		.name		= "hsspi-if",
		.parent		= &clk_esysclk.clk,
		.ctrlbit	= S3C2443_SCLKCON_HSSPICLK,
		.enable		= s3c2443_clkcon_enable_s,
	},
	.reg_div = { .reg = S3C2443_CLKDIV1, .size = 2, .shift = 4 },
};



static struct clksrc_clk clk_hsmmc_div = {
	.clk	= {
		.name		= "hsmmc-div",
		.devname	= "s3c-sdhci.1",
		.parent		= &clk_esysclk.clk,
	},
	.reg_div = { .reg = S3C2443_CLKDIV1, .size = 2, .shift = 6 },
};

static int s3c2443_setparent_hsmmc(struct clk *clk, struct clk *parent)
{
	unsigned long clksrc = __raw_readl(S3C2443_SCLKCON);

	clksrc &= ~(S3C2443_SCLKCON_HSMMCCLK_EXT |
		    S3C2443_SCLKCON_HSMMCCLK_EPLL);

	if (parent == &clk_epll)
		clksrc |= S3C2443_SCLKCON_HSMMCCLK_EPLL;
	else if (parent == &clk_ext)
		clksrc |= S3C2443_SCLKCON_HSMMCCLK_EXT;
	else
		return -EINVAL;

	if (clk->usage > 0) {
		__raw_writel(clksrc, S3C2443_SCLKCON);
	}

	clk->parent = parent;
	return 0;
}

static int s3c2443_enable_hsmmc(struct clk *clk, int enable)
{
	return s3c2443_setparent_hsmmc(clk, clk->parent);
}

static struct clk clk_hsmmc = {
	.name		= "hsmmc-if",
	.devname	= "s3c-sdhci.1",
	.parent		= &clk_hsmmc_div.clk,
	.enable		= s3c2443_enable_hsmmc,
	.ops		= &(struct clk_ops) {
		.set_parent	= s3c2443_setparent_hsmmc,
	},
};


static struct clk init_clocks_off[] = {
	{
		.name		= "sdi",
		.parent		= &clk_p,
		.enable		= s3c2443_clkcon_enable_p,
		.ctrlbit	= S3C2443_PCLKCON_SDI,
	}, {
		.name		= "spi",
		.devname	= "s3c2410-spi.0",
		.parent		= &clk_p,
		.enable		= s3c2443_clkcon_enable_p,
		.ctrlbit	= S3C2443_PCLKCON_SPI0,
	}, {
		.name		= "spi",
		.devname	= "s3c2410-spi.1",
		.parent		= &clk_p,
		.enable		= s3c2443_clkcon_enable_p,
		.ctrlbit	= S3C2443_PCLKCON_SPI1,
	}
};


static struct clksrc_clk *clksrcs[] __initdata = {
	&clk_hsspi,
	&clk_hsmmc_div,
};

static struct clk *clks[] __initdata = {
	&clk_hsmmc,
};

void __init s3c2443_init_clocks(int xtal)
{
	unsigned long epllcon = __raw_readl(S3C2443_EPLLCON);
	int ptr;

	clk_epll.rate = s3c2443_get_epll(epllcon, xtal);
	clk_epll.parent = &clk_epllref.clk;

	s3c2443_common_init_clocks(xtal, s3c2443_get_mpll,
				   armdiv, ARRAY_SIZE(armdiv),
				   S3C2443_CLKDIV0_ARMDIV_MASK);

	s3c24xx_register_clocks(clks, ARRAY_SIZE(clks));

	for (ptr = 0; ptr < ARRAY_SIZE(clksrcs); ptr++)
		s3c_register_clksrc(clksrcs[ptr], 1);


	

	s3c_register_clocks(init_clocks_off, ARRAY_SIZE(init_clocks_off));
	s3c_disable_clocks(init_clocks_off, ARRAY_SIZE(init_clocks_off));

	s3c_pwmclk_init();
}
