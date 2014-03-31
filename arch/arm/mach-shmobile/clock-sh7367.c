/*
 * SH7367 clock framework support
 *
 * Copyright (C) 2010  Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/sh_clk.h>
#include <linux/clkdev.h>
#include <mach/common.h>

#define RTFRQCR    0xe6150000
#define SYFRQCR    0xe6150004
#define CMFRQCR    0xe61500E0
#define VCLKCR1    0xe6150008
#define VCLKCR2    0xe615000C
#define VCLKCR3    0xe615001C
#define SCLKACR    0xe6150010
#define SCLKBCR    0xe6150014
#define SUBUSBCKCR 0xe6158080
#define SPUCKCR    0xe6150084
#define MSUCKCR    0xe6150088
#define MVI3CKCR   0xe6150090
#define VOUCKCR    0xe6150094
#define MFCK1CR    0xe6150098
#define MFCK2CR    0xe615009C
#define PLLC1CR    0xe6150028
#define PLLC2CR    0xe615002C
#define RTMSTPCR0  0xe6158030
#define RTMSTPCR2  0xe6158038
#define SYMSTPCR0  0xe6158040
#define SYMSTPCR2  0xe6158048
#define CMMSTPCR0  0xe615804c

static struct clk r_clk = {
	.rate           = 32768,
};

struct clk sh7367_extalb1_clk = {
	.rate		= 26666666,
};

struct clk sh7367_extal2_clk = {
	.rate		= 48000000,
};

static unsigned long div2_recalc(struct clk *clk)
{
	return clk->parent->rate / 2;
}

static struct sh_clk_ops div2_clk_ops = {
	.recalc		= div2_recalc,
};

static struct clk extalb1_div2_clk = {
	.ops		= &div2_clk_ops,
	.parent		= &sh7367_extalb1_clk,
};

static struct clk extal2_div2_clk = {
	.ops		= &div2_clk_ops,
	.parent		= &sh7367_extal2_clk,
};

static unsigned long pllc1_recalc(struct clk *clk)
{
	unsigned long mult = 1;

	if (__raw_readl(PLLC1CR) & (1 << 14))
		mult = (((__raw_readl(RTFRQCR) >> 24) & 0x3f) + 1) * 2;

	return clk->parent->rate * mult;
}

static struct sh_clk_ops pllc1_clk_ops = {
	.recalc		= pllc1_recalc,
};

static struct clk pllc1_clk = {
	.ops		= &pllc1_clk_ops,
	.flags		= CLK_ENABLE_ON_INIT,
	.parent		= &extalb1_div2_clk,
};

static struct clk pllc1_div2_clk = {
	.ops		= &div2_clk_ops,
	.parent		= &pllc1_clk,
};

static unsigned long pllc2_recalc(struct clk *clk)
{
	unsigned long mult = 1;

	if (__raw_readl(PLLC2CR) & (1 << 31))
		mult = (((__raw_readl(PLLC2CR) >> 24) & 0x3f) + 1) * 2;

	return clk->parent->rate * mult;
}

static struct sh_clk_ops pllc2_clk_ops = {
	.recalc		= pllc2_recalc,
};

static struct clk pllc2_clk = {
	.ops		= &pllc2_clk_ops,
	.flags		= CLK_ENABLE_ON_INIT,
	.parent		= &extalb1_div2_clk,
};

static struct clk *main_clks[] = {
	&r_clk,
	&sh7367_extalb1_clk,
	&sh7367_extal2_clk,
	&extalb1_div2_clk,
	&extal2_div2_clk,
	&pllc1_clk,
	&pllc1_div2_clk,
	&pllc2_clk,
};

static void div4_kick(struct clk *clk)
{
	unsigned long value;

	
	value = __raw_readl(SYFRQCR);
	value |= (1 << 31);
	__raw_writel(value, SYFRQCR);
}

static int divisors[] = { 2, 3, 4, 6, 8, 12, 16, 18,
			  24, 32, 36, 48, 0, 72, 0, 0 };

static struct clk_div_mult_table div4_div_mult_table = {
	.divisors = divisors,
	.nr_divisors = ARRAY_SIZE(divisors),
};

static struct clk_div4_table div4_table = {
	.div_mult_table = &div4_div_mult_table,
	.kick = div4_kick,
};

enum { DIV4_I, DIV4_G, DIV4_S, DIV4_B,
       DIV4_ZX, DIV4_ZT, DIV4_Z, DIV4_ZD, DIV4_HP,
       DIV4_ZS, DIV4_ZB, DIV4_ZB3, DIV4_CP, DIV4_NR };

#define DIV4(_reg, _bit, _mask, _flags) \
  SH_CLK_DIV4(&pllc1_clk, _reg, _bit, _mask, _flags)

static struct clk div4_clks[DIV4_NR] = {
	[DIV4_I] = DIV4(RTFRQCR, 20, 0x6fff, CLK_ENABLE_ON_INIT),
	[DIV4_G] = DIV4(RTFRQCR, 16, 0x6fff, CLK_ENABLE_ON_INIT),
	[DIV4_S] = DIV4(RTFRQCR, 12, 0x6fff, CLK_ENABLE_ON_INIT),
	[DIV4_B] = DIV4(RTFRQCR, 8, 0x6fff, CLK_ENABLE_ON_INIT),
	[DIV4_ZX] = DIV4(SYFRQCR, 20, 0x6fff, 0),
	[DIV4_ZT] = DIV4(SYFRQCR, 16, 0x6fff, 0),
	[DIV4_Z] = DIV4(SYFRQCR, 12, 0x6fff, 0),
	[DIV4_ZD] = DIV4(SYFRQCR, 8, 0x6fff, 0),
	[DIV4_HP] = DIV4(SYFRQCR, 4, 0x6fff, 0),
	[DIV4_ZS] = DIV4(CMFRQCR, 12, 0x6fff, 0),
	[DIV4_ZB] = DIV4(CMFRQCR, 8, 0x6fff, 0),
	[DIV4_ZB3] = DIV4(CMFRQCR, 4, 0x6fff, 0),
	[DIV4_CP] = DIV4(CMFRQCR, 0, 0x6fff, 0),
};

enum { DIV6_SUB, DIV6_SIUA, DIV6_SIUB, DIV6_MSU, DIV6_SPU,
       DIV6_MVI3, DIV6_MF1, DIV6_MF2,
       DIV6_VCK1, DIV6_VCK2, DIV6_VCK3, DIV6_VOU,
       DIV6_NR };

static struct clk div6_clks[DIV6_NR] = {
	[DIV6_SUB] = SH_CLK_DIV6(&sh7367_extal2_clk, SUBUSBCKCR, 0),
	[DIV6_SIUA] = SH_CLK_DIV6(&pllc1_div2_clk, SCLKACR, 0),
	[DIV6_SIUB] = SH_CLK_DIV6(&pllc1_div2_clk, SCLKBCR, 0),
	[DIV6_MSU] = SH_CLK_DIV6(&pllc1_div2_clk, MSUCKCR, 0),
	[DIV6_SPU] = SH_CLK_DIV6(&pllc1_div2_clk, SPUCKCR, 0),
	[DIV6_MVI3] = SH_CLK_DIV6(&pllc1_div2_clk, MVI3CKCR, 0),
	[DIV6_MF1] = SH_CLK_DIV6(&pllc1_div2_clk, MFCK1CR, 0),
	[DIV6_MF2] = SH_CLK_DIV6(&pllc1_div2_clk, MFCK2CR, 0),
	[DIV6_VCK1] = SH_CLK_DIV6(&pllc1_div2_clk, VCLKCR1, 0),
	[DIV6_VCK2] = SH_CLK_DIV6(&pllc1_div2_clk, VCLKCR2, 0),
	[DIV6_VCK3] = SH_CLK_DIV6(&pllc1_div2_clk, VCLKCR3, 0),
	[DIV6_VOU] = SH_CLK_DIV6(&pllc1_div2_clk, VOUCKCR, 0),
};

enum { RTMSTP001,
       RTMSTP231, RTMSTP230, RTMSTP229, RTMSTP228, RTMSTP226,
       RTMSTP216, RTMSTP206, RTMSTP205, RTMSTP201,
       SYMSTP023, SYMSTP007, SYMSTP006, SYMSTP004,
       SYMSTP003, SYMSTP002, SYMSTP001, SYMSTP000,
       SYMSTP231, SYMSTP229, SYMSTP225, SYMSTP223, SYMSTP222,
       SYMSTP215, SYMSTP214, SYMSTP213, SYMSTP211,
       CMMSTP003,
       MSTP_NR };

#define MSTP(_parent, _reg, _bit, _flags) \
  SH_CLK_MSTP32(_parent, _reg, _bit, _flags)

static struct clk mstp_clks[MSTP_NR] = {
	[RTMSTP001] = MSTP(&div6_clks[DIV6_SUB], RTMSTPCR0, 1, 0), 
	[RTMSTP231] = MSTP(&div4_clks[DIV4_B], RTMSTPCR2, 31, 0), 
	[RTMSTP230] = MSTP(&div4_clks[DIV4_B], RTMSTPCR2, 30, 0), 
	[RTMSTP229] = MSTP(&div4_clks[DIV4_B], RTMSTPCR2, 29, 0), 
	[RTMSTP228] = MSTP(&div4_clks[DIV4_B], RTMSTPCR2, 28, 0), 
	[RTMSTP226] = MSTP(&div4_clks[DIV4_B], RTMSTPCR2, 26, 0), 
	[RTMSTP216] = MSTP(&div6_clks[DIV6_SUB], RTMSTPCR2, 16, 0), 
	[RTMSTP206] = MSTP(&div4_clks[DIV4_B], RTMSTPCR2, 6, 0), 
	[RTMSTP205] = MSTP(&div6_clks[DIV6_VOU], RTMSTPCR2, 5, 0), 
	[RTMSTP201] = MSTP(&div4_clks[DIV4_B], RTMSTPCR2, 1, 0), 
	[SYMSTP023] = MSTP(&div6_clks[DIV6_SPU], SYMSTPCR0, 23, 0), 
	[SYMSTP007] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR0, 7, 0), 
	[SYMSTP006] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR0, 6, 0), 
	[SYMSTP004] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR0, 4, 0), 
	[SYMSTP003] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR0, 3, 0), 
	[SYMSTP002] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR0, 2, 0), 
	[SYMSTP001] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR0, 1, 0), 
	[SYMSTP000] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR0, 0, 0), 
	[SYMSTP231] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR2, 31, 0), 
	[SYMSTP229] = MSTP(&r_clk, SYMSTPCR2, 29, 0), 
	[SYMSTP225] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR2, 25, 0), 
	[SYMSTP223] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR2, 23, 0), 
	[SYMSTP222] = MSTP(&div6_clks[DIV6_SUB], SYMSTPCR2, 22, 0), 
	[SYMSTP215] = MSTP(&div4_clks[DIV4_HP], SYMSTPCR2, 15, 0), 
	[SYMSTP214] = MSTP(&div4_clks[DIV4_HP], SYMSTPCR2, 14, 0), 
	[SYMSTP213] = MSTP(&div4_clks[DIV4_HP], SYMSTPCR2, 13, 0), 
	[SYMSTP211] = MSTP(&div4_clks[DIV4_HP], SYMSTPCR2, 11, 0), 
	[CMMSTP003] = MSTP(&r_clk, CMMSTPCR0, 3, 0), 
};

static struct clk_lookup lookups[] = {
	
	CLKDEV_CON_ID("r_clk", &r_clk),
	CLKDEV_CON_ID("extalb1", &sh7367_extalb1_clk),
	CLKDEV_CON_ID("extal2", &sh7367_extal2_clk),
	CLKDEV_CON_ID("extalb1_div2_clk", &extalb1_div2_clk),
	CLKDEV_CON_ID("extal2_div2_clk", &extal2_div2_clk),
	CLKDEV_CON_ID("pllc1_clk", &pllc1_clk),
	CLKDEV_CON_ID("pllc1_div2_clk", &pllc1_div2_clk),
	CLKDEV_CON_ID("pllc2_clk", &pllc2_clk),

	
	CLKDEV_CON_ID("i_clk", &div4_clks[DIV4_I]),
	CLKDEV_CON_ID("g_clk", &div4_clks[DIV4_G]),
	CLKDEV_CON_ID("b_clk", &div4_clks[DIV4_B]),
	CLKDEV_CON_ID("zx_clk", &div4_clks[DIV4_ZX]),
	CLKDEV_CON_ID("zt_clk", &div4_clks[DIV4_ZT]),
	CLKDEV_CON_ID("z_clk", &div4_clks[DIV4_Z]),
	CLKDEV_CON_ID("zd_clk", &div4_clks[DIV4_ZD]),
	CLKDEV_CON_ID("hp_clk", &div4_clks[DIV4_HP]),
	CLKDEV_CON_ID("zs_clk", &div4_clks[DIV4_ZS]),
	CLKDEV_CON_ID("zb_clk", &div4_clks[DIV4_ZB]),
	CLKDEV_CON_ID("zb3_clk", &div4_clks[DIV4_ZB3]),
	CLKDEV_CON_ID("cp_clk", &div4_clks[DIV4_CP]),

	
	CLKDEV_CON_ID("sub_clk", &div6_clks[DIV6_SUB]),
	CLKDEV_CON_ID("siua_clk", &div6_clks[DIV6_SIUA]),
	CLKDEV_CON_ID("siub_clk", &div6_clks[DIV6_SIUB]),
	CLKDEV_CON_ID("msu_clk", &div6_clks[DIV6_MSU]),
	CLKDEV_CON_ID("spu_clk", &div6_clks[DIV6_SPU]),
	CLKDEV_CON_ID("mvi3_clk", &div6_clks[DIV6_MVI3]),
	CLKDEV_CON_ID("mf1_clk", &div6_clks[DIV6_MF1]),
	CLKDEV_CON_ID("mf2_clk", &div6_clks[DIV6_MF2]),
	CLKDEV_CON_ID("vck1_clk", &div6_clks[DIV6_VCK1]),
	CLKDEV_CON_ID("vck2_clk", &div6_clks[DIV6_VCK2]),
	CLKDEV_CON_ID("vck3_clk", &div6_clks[DIV6_VCK3]),
	CLKDEV_CON_ID("vou_clk", &div6_clks[DIV6_VOU]),

	
	CLKDEV_DEV_ID("i2c-sh_mobile.2", &mstp_clks[RTMSTP001]), 
	CLKDEV_DEV_ID("uio_pdrv_genirq.4", &mstp_clks[RTMSTP231]), 
	CLKDEV_DEV_ID("uio_pdrv_genirq.3", &mstp_clks[RTMSTP230]), 
	CLKDEV_DEV_ID("uio_pdrv_genirq.2", &mstp_clks[RTMSTP229]), 
	CLKDEV_DEV_ID("uio_pdrv_genirq.1", &mstp_clks[RTMSTP228]), 
	CLKDEV_DEV_ID("uio_pdrv_genirq.5", &mstp_clks[RTMSTP226]), 
	CLKDEV_DEV_ID("i2c-sh_mobile.0", &mstp_clks[RTMSTP216]), 
	CLKDEV_DEV_ID("uio_pdrv_genirq.6", &mstp_clks[RTMSTP206]), 
	CLKDEV_DEV_ID("sh-vou", &mstp_clks[RTMSTP205]), 
	CLKDEV_DEV_ID("uio_pdrv_genirq.0", &mstp_clks[RTMSTP201]), 
	CLKDEV_DEV_ID("uio_pdrv_genirq.7", &mstp_clks[SYMSTP023]), 
	CLKDEV_DEV_ID("sh-sci.5", &mstp_clks[SYMSTP007]), 
	CLKDEV_DEV_ID("sh-sci.6", &mstp_clks[SYMSTP006]), 
	CLKDEV_DEV_ID("sh-sci.0", &mstp_clks[SYMSTP004]), 
	CLKDEV_DEV_ID("sh-sci.1", &mstp_clks[SYMSTP003]), 
	CLKDEV_DEV_ID("sh-sci.2", &mstp_clks[SYMSTP002]), 
	CLKDEV_DEV_ID("sh-sci.3", &mstp_clks[SYMSTP001]), 
	CLKDEV_DEV_ID("sh-sci.4", &mstp_clks[SYMSTP000]), 
	CLKDEV_DEV_ID("sh_siu", &mstp_clks[SYMSTP231]), 
	CLKDEV_DEV_ID("sh_cmt.10", &mstp_clks[SYMSTP229]), 
	CLKDEV_DEV_ID("sh_irda", &mstp_clks[SYMSTP225]), 
	CLKDEV_DEV_ID("i2c-sh_mobile.1", &mstp_clks[SYMSTP223]), 
	CLKDEV_DEV_ID("r8a66597_hcd.0", &mstp_clks[SYMSTP222]), 
	CLKDEV_DEV_ID("r8a66597_udc.0", &mstp_clks[SYMSTP222]), 
	CLKDEV_DEV_ID("sh_flctl", &mstp_clks[SYMSTP215]), 
	CLKDEV_DEV_ID("sh_mobile_sdhi.0", &mstp_clks[SYMSTP214]), 
	CLKDEV_DEV_ID("sh_mobile_sdhi.1", &mstp_clks[SYMSTP213]), 
	CLKDEV_DEV_ID("sh_mobile_sdhi.2", &mstp_clks[SYMSTP211]), 
	CLKDEV_DEV_ID("sh_keysc.0", &mstp_clks[CMMSTP003]), 
};

void __init sh7367_clock_init(void)
{
	int k, ret = 0;

	for (k = 0; !ret && (k < ARRAY_SIZE(main_clks)); k++)
		ret = clk_register(main_clks[k]);

	if (!ret)
		ret = sh_clk_div4_register(div4_clks, DIV4_NR, &div4_table);

	if (!ret)
		ret = sh_clk_div6_register(div6_clks, DIV6_NR);

	if (!ret)
		ret = sh_clk_mstp32_register(mstp_clks, MSTP_NR);

	clkdev_add_table(lookups, ARRAY_SIZE(lookups));

	if (!ret)
		shmobile_clk_init();
	else
		panic("failed to setup sh7367 clocks\n");
}
