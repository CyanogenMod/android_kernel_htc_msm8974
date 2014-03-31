/*
 * OMAP2420 clock data
 *
 * Copyright (C) 2005-2009 Texas Instruments, Inc.
 * Copyright (C) 2004-2011 Nokia Corporation
 *
 * Contacts:
 * Richard Woodruff <r-woodruff2@ti.com>
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/list.h>

#include <plat/hardware.h>
#include <plat/clkdev_omap.h>

#include "iomap.h"
#include "clock.h"
#include "clock2xxx.h"
#include "opp2xxx.h"
#include "cm2xxx_3xxx.h"
#include "prm2xxx_3xxx.h"
#include "prm-regbits-24xx.h"
#include "cm-regbits-24xx.h"
#include "sdrc.h"
#include "control.h"

#define OMAP_CM_REGADDR                 OMAP2420_CM_REGADDR


static struct clk func_32k_ck = {
	.name		= "func_32k_ck",
	.ops		= &clkops_null,
	.rate		= 32768,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk secure_32k_ck = {
	.name		= "secure_32k_ck",
	.ops		= &clkops_null,
	.rate		= 32768,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk osc_ck = {		
	.name		= "osc_ck",
	.ops		= &clkops_oscck,
	.clkdm_name	= "wkup_clkdm",
	.recalc		= &omap2_osc_clk_recalc,
};

static struct clk sys_ck = {		
	.name		= "sys_ck",		
	.ops		= &clkops_null,
	.parent		= &osc_ck,
	.clkdm_name	= "wkup_clkdm",
	.recalc		= &omap2xxx_sys_clk_recalc,
};

static struct clk alt_ck = {		
	.name		= "alt_ck",
	.ops		= &clkops_null,
	.rate		= 54000000,
	.clkdm_name	= "wkup_clkdm",
};

static struct clk mcbsp_clks = {
	.name		= "mcbsp_clks",
	.ops		= &clkops_null,
};



static struct dpll_data dpll_dd = {
	.mult_div1_reg		= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.mult_mask		= OMAP24XX_DPLL_MULT_MASK,
	.div1_mask		= OMAP24XX_DPLL_DIV_MASK,
	.clk_bypass		= &sys_ck,
	.clk_ref		= &sys_ck,
	.control_reg		= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_mask		= OMAP24XX_EN_DPLL_MASK,
	.max_multiplier		= 1023,
	.min_divider		= 1,
	.max_divider		= 16,
};

static struct clk dpll_ck = {
	.name		= "dpll_ck",
	.ops		= &clkops_omap2xxx_dpll_ops,
	.parent		= &sys_ck,		
	.dpll_data	= &dpll_dd,
	.clkdm_name	= "wkup_clkdm",
	.recalc		= &omap2_dpllcore_recalc,
	.set_rate	= &omap2_reprogram_dpllcore,
};

static struct clk apll96_ck = {
	.name		= "apll96_ck",
	.ops		= &clkops_apll96,
	.parent		= &sys_ck,
	.rate		= 96000000,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP24XX_EN_96M_PLL_SHIFT,
};

static struct clk apll54_ck = {
	.name		= "apll54_ck",
	.ops		= &clkops_apll54,
	.parent		= &sys_ck,
	.rate		= 54000000,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKEN),
	.enable_bit	= OMAP24XX_EN_54M_PLL_SHIFT,
};



static const struct clksel_rate func_54m_apll54_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 },
};

static const struct clksel_rate func_54m_alt_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 },
};

static const struct clksel func_54m_clksel[] = {
	{ .parent = &apll54_ck, .rates = func_54m_apll54_rates, },
	{ .parent = &alt_ck,	.rates = func_54m_alt_rates, },
	{ .parent = NULL },
};

static struct clk func_54m_ck = {
	.name		= "func_54m_ck",
	.ops		= &clkops_null,
	.parent		= &apll54_ck,	
	.clkdm_name	= "wkup_clkdm",
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_54M_SOURCE_MASK,
	.clksel		= func_54m_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk core_ck = {
	.name		= "core_ck",
	.ops		= &clkops_null,
	.parent		= &dpll_ck,		
	.clkdm_name	= "wkup_clkdm",
	.recalc		= &followparent_recalc,
};

static struct clk func_96m_ck = {
	.name		= "func_96m_ck",
	.ops		= &clkops_null,
	.parent		= &apll96_ck,
	.clkdm_name	= "wkup_clkdm",
	.recalc		= &followparent_recalc,
};


static const struct clksel_rate func_48m_apll96_rates[] = {
	{ .div = 2, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 },
};

static const struct clksel_rate func_48m_alt_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 },
};

static const struct clksel func_48m_clksel[] = {
	{ .parent = &apll96_ck,	.rates = func_48m_apll96_rates },
	{ .parent = &alt_ck, .rates = func_48m_alt_rates },
	{ .parent = NULL }
};

static struct clk func_48m_ck = {
	.name		= "func_48m_ck",
	.ops		= &clkops_null,
	.parent		= &apll96_ck,	 
	.clkdm_name	= "wkup_clkdm",
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(PLL_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_48M_SOURCE_MASK,
	.clksel		= func_48m_clksel,
	.recalc		= &omap2_clksel_recalc,
	.round_rate	= &omap2_clksel_round_rate,
	.set_rate	= &omap2_clksel_set_rate
};

static struct clk func_12m_ck = {
	.name		= "func_12m_ck",
	.ops		= &clkops_null,
	.parent		= &func_48m_ck,
	.fixed_div	= 4,
	.clkdm_name	= "wkup_clkdm",
	.recalc		= &omap_fixed_divisor_recalc,
};

static struct clk wdt1_osc_ck = {
	.name		= "ck_wdt1_osc",
	.ops		= &clkops_null, 
	.parent		= &osc_ck,
	.recalc		= &followparent_recalc,
};

static const struct clksel_rate common_clkout_src_core_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate common_clkout_src_sys_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate common_clkout_src_96m_rates[] = {
	{ .div = 1, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate common_clkout_src_54m_rates[] = {
	{ .div = 1, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel common_clkout_src_clksel[] = {
	{ .parent = &core_ck,	  .rates = common_clkout_src_core_rates },
	{ .parent = &sys_ck,	  .rates = common_clkout_src_sys_rates },
	{ .parent = &func_96m_ck, .rates = common_clkout_src_96m_rates },
	{ .parent = &func_54m_ck, .rates = common_clkout_src_54m_rates },
	{ .parent = NULL }
};

static struct clk sys_clkout_src = {
	.name		= "sys_clkout_src",
	.ops		= &clkops_omap2_dflt,
	.parent		= &func_54m_ck,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.enable_bit	= OMAP24XX_CLKOUT_EN_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.clksel_mask	= OMAP24XX_CLKOUT_SOURCE_MASK,
	.clksel		= common_clkout_src_clksel,
	.recalc		= &omap2_clksel_recalc,
	.round_rate	= &omap2_clksel_round_rate,
	.set_rate	= &omap2_clksel_set_rate
};

static const struct clksel_rate common_clkout_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 8, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 16, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 0 },
};

static const struct clksel sys_clkout_clksel[] = {
	{ .parent = &sys_clkout_src, .rates = common_clkout_rates },
	{ .parent = NULL }
};

static struct clk sys_clkout = {
	.name		= "sys_clkout",
	.ops		= &clkops_null,
	.parent		= &sys_clkout_src,
	.clkdm_name	= "wkup_clkdm",
	.clksel_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.clksel_mask	= OMAP24XX_CLKOUT_DIV_MASK,
	.clksel		= sys_clkout_clksel,
	.recalc		= &omap2_clksel_recalc,
	.round_rate	= &omap2_clksel_round_rate,
	.set_rate	= &omap2_clksel_set_rate
};

static struct clk sys_clkout2_src = {
	.name		= "sys_clkout2_src",
	.ops		= &clkops_omap2_dflt,
	.parent		= &func_54m_ck,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.enable_bit	= OMAP2420_CLKOUT2_EN_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.clksel_mask	= OMAP2420_CLKOUT2_SOURCE_MASK,
	.clksel		= common_clkout_src_clksel,
	.recalc		= &omap2_clksel_recalc,
	.round_rate	= &omap2_clksel_round_rate,
	.set_rate	= &omap2_clksel_set_rate
};

static const struct clksel sys_clkout2_clksel[] = {
	{ .parent = &sys_clkout2_src, .rates = common_clkout_rates },
	{ .parent = NULL }
};

static struct clk sys_clkout2 = {
	.name		= "sys_clkout2",
	.ops		= &clkops_null,
	.parent		= &sys_clkout2_src,
	.clkdm_name	= "wkup_clkdm",
	.clksel_reg	= OMAP2420_PRCM_CLKOUT_CTRL,
	.clksel_mask	= OMAP2420_CLKOUT2_DIV_MASK,
	.clksel		= sys_clkout2_clksel,
	.recalc		= &omap2_clksel_recalc,
	.round_rate	= &omap2_clksel_round_rate,
	.set_rate	= &omap2_clksel_set_rate
};

static struct clk emul_ck = {
	.name		= "emul_ck",
	.ops		= &clkops_omap2_dflt,
	.parent		= &func_54m_ck,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP2420_PRCM_CLKEMUL_CTRL,
	.enable_bit	= OMAP24XX_EMULATION_EN_SHIFT,
	.recalc		= &followparent_recalc,

};

static const struct clksel_rate mpu_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_242X },
	{ .div = 6, .val = 6, .flags = RATE_IN_242X },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 0 },
};

static const struct clksel mpu_clksel[] = {
	{ .parent = &core_ck, .rates = mpu_core_rates },
	{ .parent = NULL }
};

static struct clk mpu_ck = {	
	.name		= "mpu_ck",
	.ops		= &clkops_null,
	.parent		= &core_ck,
	.clkdm_name	= "mpu_clkdm",
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(MPU_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP24XX_CLKSEL_MPU_MASK,
	.clksel		= mpu_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static const struct clksel_rate dsp_fck_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 3, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 6, .val = 6, .flags = RATE_IN_242X },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 12, .val = 12, .flags = RATE_IN_242X },
	{ .div = 0 },
};

static const struct clksel dsp_fck_clksel[] = {
	{ .parent = &core_ck, .rates = dsp_fck_core_rates },
	{ .parent = NULL }
};

static struct clk dsp_fck = {
	.name		= "dsp_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &core_ck,
	.clkdm_name	= "dsp_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_CM_FCLKEN_DSP_EN_DSP_SHIFT,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP24XX_CLKSEL_DSP_MASK,
	.clksel		= dsp_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static const struct clksel dsp_ick_clksel[] = {
	{ .parent = &dsp_fck, .rates = dsp_ick_rates },
	{ .parent = NULL }
};

static struct clk dsp_ick = {
	.name		= "dsp_ick",	 
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &dsp_fck,
	.clkdm_name	= "dsp_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP2420_EN_DSP_IPI_SHIFT,	      
	.clksel_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP24XX_CLKSEL_DSP_IF_MASK,
	.clksel		= dsp_ick_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk iva1_ifck = {
	.name		= "iva1_ifck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &core_ck,
	.clkdm_name	= "iva1_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP2420_EN_IVA_COP_SHIFT,
	.clksel_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP2420_CLKSEL_IVA_MASK,
	.clksel		= dsp_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk iva1_mpu_int_ifck = {
	.name		= "iva1_mpu_int_ifck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &iva1_ifck,
	.clkdm_name	= "iva1_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(OMAP24XX_DSP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP2420_EN_IVA_MPU_SHIFT,
	.fixed_div	= 2,
	.recalc		= &omap_fixed_divisor_recalc,
};

static const struct clksel_rate core_l3_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_242X },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 6, .val = 6, .flags = RATE_IN_24XX },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 12, .val = 12, .flags = RATE_IN_242X },
	{ .div = 16, .val = 16, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel core_l3_clksel[] = {
	{ .parent = &core_ck, .rates = core_l3_core_rates },
	{ .parent = NULL }
};

static struct clk core_l3_ck = {	
	.name		= "core_l3_ck",
	.ops		= &clkops_null,
	.parent		= &core_ck,
	.clkdm_name	= "core_l3_clkdm",
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_L3_MASK,
	.clksel		= core_l3_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static const struct clksel_rate usb_l4_ick_core_l3_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel usb_l4_ick_clksel[] = {
	{ .parent = &core_l3_ck, .rates = usb_l4_ick_core_l3_rates },
	{ .parent = NULL },
};

static struct clk usb_l4_ick = {	
	.name		= "usb_l4_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &core_l3_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP24XX_EN_USB_SHIFT,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_USB_MASK,
	.clksel		= usb_l4_ick_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static const struct clksel_rate l4_core_l3_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel l4_clksel[] = {
	{ .parent = &core_l3_ck, .rates = l4_core_l3_rates },
	{ .parent = NULL }
};

static struct clk l4_ck = {		
	.name		= "l4_ck",
	.ops		= &clkops_null,
	.parent		= &core_l3_ck,
	.clkdm_name	= "core_l4_clkdm",
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_L4_MASK,
	.clksel		= l4_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static const struct clksel_rate ssi_ssr_sst_fck_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 3, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 6, .val = 6, .flags = RATE_IN_242X },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel ssi_ssr_sst_fck_clksel[] = {
	{ .parent = &core_ck, .rates = ssi_ssr_sst_fck_core_rates },
	{ .parent = NULL }
};

static struct clk ssi_ssr_sst_fck = {
	.name		= "ssi_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &core_ck,
	.clkdm_name	= "core_l3_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_FCLKEN2),
	.enable_bit	= OMAP24XX_EN_SSI_SHIFT,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_SSI_MASK,
	.clksel		= ssi_ssr_sst_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk ssi_l4_ick = {
	.name		= "ssi_l4_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP24XX_EN_SSI_SHIFT,
	.recalc		= &followparent_recalc,
};



static const struct clksel gfx_fck_clksel[] = {
	{ .parent = &core_l3_ck, .rates = gfx_l3_rates },
	{ .parent = NULL },
};

static struct clk gfx_3d_fck = {
	.name		= "gfx_3d_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &core_l3_ck,
	.clkdm_name	= "gfx_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_3D_SHIFT,
	.clksel_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP_CLKSEL_GFX_MASK,
	.clksel		= gfx_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
	.round_rate	= &omap2_clksel_round_rate,
	.set_rate	= &omap2_clksel_set_rate
};

static struct clk gfx_2d_fck = {
	.name		= "gfx_2d_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &core_l3_ck,
	.clkdm_name	= "gfx_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_2D_SHIFT,
	.clksel_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_CLKSEL),
	.clksel_mask	= OMAP_CLKSEL_GFX_MASK,
	.clksel		= gfx_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gfx_ick = {
	.name		= "gfx_ick",		
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &core_l3_ck,
	.clkdm_name	= "gfx_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(GFX_MOD, CM_ICLKEN),
	.enable_bit	= OMAP_EN_GFX_SHIFT,
	.recalc		= &followparent_recalc,
};


static const struct clksel_rate dss1_fck_sys_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate dss1_fck_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 2, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 3, .val = 3, .flags = RATE_IN_24XX },
	{ .div = 4, .val = 4, .flags = RATE_IN_24XX },
	{ .div = 5, .val = 5, .flags = RATE_IN_24XX },
	{ .div = 6, .val = 6, .flags = RATE_IN_24XX },
	{ .div = 8, .val = 8, .flags = RATE_IN_24XX },
	{ .div = 9, .val = 9, .flags = RATE_IN_24XX },
	{ .div = 12, .val = 12, .flags = RATE_IN_24XX },
	{ .div = 16, .val = 16, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel dss1_fck_clksel[] = {
	{ .parent = &sys_ck,  .rates = dss1_fck_sys_rates },
	{ .parent = &core_ck, .rates = dss1_fck_core_rates },
	{ .parent = NULL },
};

static struct clk dss_ick = {		
	.name		= "dss_ick",
	.ops		= &clkops_omap2_iclk_dflt,
	.parent		= &l4_ck,	
	.clkdm_name	= "dss_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_DSS1_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk dss1_fck = {
	.name		= "dss1_fck",
	.ops		= &clkops_omap2_dflt,
	.parent		= &core_ck,		
	.clkdm_name	= "dss_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_DSS1_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_DSS1_MASK,
	.clksel		= dss1_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static const struct clksel_rate dss2_fck_sys_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate dss2_fck_48m_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel dss2_fck_clksel[] = {
	{ .parent = &sys_ck,	  .rates = dss2_fck_sys_rates },
	{ .parent = &func_48m_ck, .rates = dss2_fck_48m_rates },
	{ .parent = NULL }
};

static struct clk dss2_fck = {		
	.name		= "dss2_fck",
	.ops		= &clkops_omap2_dflt,
	.parent		= &sys_ck,		
	.clkdm_name	= "dss_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_DSS2_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_DSS2_MASK,
	.clksel		= dss2_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk dss_54m_fck = {	
	.name		= "dss_54m_fck",	
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_54m_ck,
	.clkdm_name	= "dss_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_TV_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk wu_l4_ick = {
	.name		= "wu_l4_ick",
	.ops		= &clkops_null,
	.parent		= &sys_ck,
	.clkdm_name	= "wkup_clkdm",
	.recalc		= &followparent_recalc,
};

static const struct clksel_rate gpt_alt_rates[] = {
	{ .div = 1, .val = 2, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel omap24xx_gpt_clksel[] = {
	{ .parent = &func_32k_ck, .rates = gpt_32k_rates },
	{ .parent = &sys_ck,	  .rates = gpt_sys_rates },
	{ .parent = &alt_ck,	  .rates = gpt_alt_rates },
	{ .parent = NULL },
};

static struct clk gpt1_ick = {
	.name		= "gpt1_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &wu_l4_ick,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_GPT1_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt1_fck = {
	.name		= "gpt1_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_GPT1_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT1_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
	.round_rate	= &omap2_clksel_round_rate,
	.set_rate	= &omap2_clksel_set_rate
};

static struct clk gpt2_ick = {
	.name		= "gpt2_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT2_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt2_fck = {
	.name		= "gpt2_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT2_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT2_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt3_ick = {
	.name		= "gpt3_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT3_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt3_fck = {
	.name		= "gpt3_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT3_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT3_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt4_ick = {
	.name		= "gpt4_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT4_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt4_fck = {
	.name		= "gpt4_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT4_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT4_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt5_ick = {
	.name		= "gpt5_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT5_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt5_fck = {
	.name		= "gpt5_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT5_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT5_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt6_ick = {
	.name		= "gpt6_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT6_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt6_fck = {
	.name		= "gpt6_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT6_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT6_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt7_ick = {
	.name		= "gpt7_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT7_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt7_fck = {
	.name		= "gpt7_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT7_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT7_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt8_ick = {
	.name		= "gpt8_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT8_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt8_fck = {
	.name		= "gpt8_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT8_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT8_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt9_ick = {
	.name		= "gpt9_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT9_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt9_fck = {
	.name		= "gpt9_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT9_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT9_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt10_ick = {
	.name		= "gpt10_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT10_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt10_fck = {
	.name		= "gpt10_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT10_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT10_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt11_ick = {
	.name		= "gpt11_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT11_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt11_fck = {
	.name		= "gpt11_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT11_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT11_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk gpt12_ick = {
	.name		= "gpt12_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT12_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpt12_fck = {
	.name		= "gpt12_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &secure_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_GPT12_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL2),
	.clksel_mask	= OMAP24XX_CLKSEL_GPT12_MASK,
	.clksel		= omap24xx_gpt_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk mcbsp1_ick = {
	.name		= "mcbsp1_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MCBSP1_SHIFT,
	.recalc		= &followparent_recalc,
};

static const struct clksel_rate common_mcbsp_96m_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel_rate common_mcbsp_mcbsp_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_24XX },
	{ .div = 0 }
};

static const struct clksel mcbsp_fck_clksel[] = {
	{ .parent = &func_96m_ck,  .rates = common_mcbsp_96m_rates },
	{ .parent = &mcbsp_clks,   .rates = common_mcbsp_mcbsp_rates },
	{ .parent = NULL }
};

static struct clk mcbsp1_fck = {
	.name		= "mcbsp1_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_96m_ck,
	.init		= &omap2_init_clksel_parent,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MCBSP1_SHIFT,
	.clksel_reg	= OMAP242X_CTRL_REGADDR(OMAP2_CONTROL_DEVCONF0),
	.clksel_mask	= OMAP2_MCBSP1_CLKS_MASK,
	.clksel		= mcbsp_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk mcbsp2_ick = {
	.name		= "mcbsp2_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MCBSP2_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mcbsp2_fck = {
	.name		= "mcbsp2_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_96m_ck,
	.init		= &omap2_init_clksel_parent,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MCBSP2_SHIFT,
	.clksel_reg	= OMAP242X_CTRL_REGADDR(OMAP2_CONTROL_DEVCONF0),
	.clksel_mask	= OMAP2_MCBSP2_CLKS_MASK,
	.clksel		= mcbsp_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk mcspi1_ick = {
	.name		= "mcspi1_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MCSPI1_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mcspi1_fck = {
	.name		= "mcspi1_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_48m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MCSPI1_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mcspi2_ick = {
	.name		= "mcspi2_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MCSPI2_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mcspi2_fck = {
	.name		= "mcspi2_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_48m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MCSPI2_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk uart1_ick = {
	.name		= "uart1_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_UART1_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk uart1_fck = {
	.name		= "uart1_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_48m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_UART1_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk uart2_ick = {
	.name		= "uart2_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_UART2_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk uart2_fck = {
	.name		= "uart2_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_48m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_UART2_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk uart3_ick = {
	.name		= "uart3_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN2),
	.enable_bit	= OMAP24XX_EN_UART3_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk uart3_fck = {
	.name		= "uart3_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_48m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_FCLKEN2),
	.enable_bit	= OMAP24XX_EN_UART3_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpios_ick = {
	.name		= "gpios_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &wu_l4_ick,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_GPIOS_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpios_fck = {
	.name		= "gpios_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_GPIOS_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mpu_wdt_ick = {
	.name		= "mpu_wdt_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &wu_l4_ick,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_MPU_WDT_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mpu_wdt_fck = {
	.name		= "mpu_wdt_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_FCLKEN),
	.enable_bit	= OMAP24XX_EN_MPU_WDT_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk sync_32k_ick = {
	.name		= "sync_32k_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &wu_l4_ick,
	.clkdm_name	= "wkup_clkdm",
	.flags		= ENABLE_ON_INIT,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_32KSYNC_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk wdt1_ick = {
	.name		= "wdt1_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &wu_l4_ick,
	.clkdm_name	= "wkup_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_WDT1_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk omapctrl_ick = {
	.name		= "omapctrl_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &wu_l4_ick,
	.clkdm_name	= "wkup_clkdm",
	.flags		= ENABLE_ON_INIT,
	.enable_reg	= OMAP_CM_REGADDR(WKUP_MOD, CM_ICLKEN),
	.enable_bit	= OMAP24XX_EN_OMAPCTRL_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk cam_ick = {
	.name		= "cam_ick",
	.ops		= &clkops_omap2_iclk_dflt,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_CAM_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk cam_fck = {
	.name		= "cam_fck",
	.ops		= &clkops_omap2_dflt,
	.parent		= &func_96m_ck,
	.clkdm_name	= "core_l3_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_CAM_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mailboxes_ick = {
	.name		= "mailboxes_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MAILBOXES_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk wdt4_ick = {
	.name		= "wdt4_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_WDT4_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk wdt4_fck = {
	.name		= "wdt4_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_WDT4_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk wdt3_ick = {
	.name		= "wdt3_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_WDT3_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk wdt3_fck = {
	.name		= "wdt3_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_32k_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_WDT3_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mspro_ick = {
	.name		= "mspro_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_MSPRO_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mspro_fck = {
	.name		= "mspro_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_96m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_MSPRO_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mmc_ick = {
	.name		= "mmc_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_MMC_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk mmc_fck = {
	.name		= "mmc_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_96m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_MMC_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk fac_ick = {
	.name		= "fac_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_FAC_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk fac_fck = {
	.name		= "fac_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_12m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_FAC_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk eac_ick = {
	.name		= "eac_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_EAC_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk eac_fck = {
	.name		= "eac_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_96m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_EAC_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk hdq_ick = {
	.name		= "hdq_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP24XX_EN_HDQ_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk hdq_fck = {
	.name		= "hdq_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_12m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP24XX_EN_HDQ_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk i2c2_ick = {
	.name		= "i2c2_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_I2C2_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk i2c2_fck = {
	.name		= "i2c2_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_12m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_I2C2_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk i2c1_ick = {
	.name		= "i2c1_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_I2C1_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk i2c1_fck = {
	.name		= "i2c1_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_12m_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_I2C1_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk gpmc_fck = {
	.name		= "gpmc_fck",
	.ops		= &clkops_omap2_iclk_idle_only,
	.parent		= &core_l3_ck,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "core_l3_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN3),
	.enable_bit	= OMAP24XX_AUTO_GPMC_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk sdma_fck = {
	.name		= "sdma_fck",
	.ops		= &clkops_null, 
	.parent		= &core_l3_ck,
	.clkdm_name	= "core_l3_clkdm",
	.recalc		= &followparent_recalc,
};

static struct clk sdma_ick = {
	.name		= "sdma_ick",
	.ops		= &clkops_omap2_iclk_idle_only,
	.parent		= &core_l3_ck,
	.clkdm_name	= "core_l3_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN3),
	.enable_bit	= OMAP24XX_AUTO_SDMA_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk sdrc_ick = {
	.name		= "sdrc_ick",
	.ops		= &clkops_omap2_iclk_idle_only,
	.parent		= &core_l3_ck,
	.flags		= ENABLE_ON_INIT,
	.clkdm_name	= "core_l3_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN3),
	.enable_bit	= OMAP24XX_AUTO_SDRC_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk vlynq_ick = {
	.name		= "vlynq_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &core_l3_ck,
	.clkdm_name	= "core_l3_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_ICLKEN1),
	.enable_bit	= OMAP2420_EN_VLYNQ_SHIFT,
	.recalc		= &followparent_recalc,
};

static const struct clksel_rate vlynq_fck_96m_rates[] = {
	{ .div = 1, .val = 0, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel_rate vlynq_fck_core_rates[] = {
	{ .div = 1, .val = 1, .flags = RATE_IN_242X },
	{ .div = 2, .val = 2, .flags = RATE_IN_242X },
	{ .div = 3, .val = 3, .flags = RATE_IN_242X },
	{ .div = 4, .val = 4, .flags = RATE_IN_242X },
	{ .div = 6, .val = 6, .flags = RATE_IN_242X },
	{ .div = 8, .val = 8, .flags = RATE_IN_242X },
	{ .div = 9, .val = 9, .flags = RATE_IN_242X },
	{ .div = 12, .val = 12, .flags = RATE_IN_242X },
	{ .div = 16, .val = 16, .flags = RATE_IN_242X },
	{ .div = 18, .val = 18, .flags = RATE_IN_242X },
	{ .div = 0 }
};

static const struct clksel vlynq_fck_clksel[] = {
	{ .parent = &func_96m_ck, .rates = vlynq_fck_96m_rates },
	{ .parent = &core_ck,	  .rates = vlynq_fck_core_rates },
	{ .parent = NULL }
};

static struct clk vlynq_fck = {
	.name		= "vlynq_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_96m_ck,
	.clkdm_name	= "core_l3_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_FCLKEN1),
	.enable_bit	= OMAP2420_EN_VLYNQ_SHIFT,
	.init		= &omap2_init_clksel_parent,
	.clksel_reg	= OMAP_CM_REGADDR(CORE_MOD, CM_CLKSEL1),
	.clksel_mask	= OMAP2420_CLKSEL_VLYNQ_MASK,
	.clksel		= vlynq_fck_clksel,
	.recalc		= &omap2_clksel_recalc,
};

static struct clk des_ick = {
	.name		= "des_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_DES_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk sha_ick = {
	.name		= "sha_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_SHA_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk rng_ick = {
	.name		= "rng_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_RNG_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk aes_ick = {
	.name		= "aes_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_AES_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk pka_ick = {
	.name		= "pka_ick",
	.ops		= &clkops_omap2_iclk_dflt_wait,
	.parent		= &l4_ck,
	.clkdm_name	= "core_l4_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_ICLKEN4),
	.enable_bit	= OMAP24XX_EN_PKA_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk usb_fck = {
	.name		= "usb_fck",
	.ops		= &clkops_omap2_dflt_wait,
	.parent		= &func_48m_ck,
	.clkdm_name	= "core_l3_clkdm",
	.enable_reg	= OMAP_CM_REGADDR(CORE_MOD, OMAP24XX_CM_FCLKEN2),
	.enable_bit	= OMAP24XX_EN_USB_SHIFT,
	.recalc		= &followparent_recalc,
};

static struct clk virt_prcm_set = {
	.name		= "virt_prcm_set",
	.ops		= &clkops_null,
	.parent		= &mpu_ck,	
	.recalc		= &omap2_table_mpu_recalc,	
	.set_rate	= &omap2_select_table_rate,
	.round_rate	= &omap2_round_to_table_rate,
};



static struct omap_clk omap2420_clks[] = {
	
	CLK(NULL,	"func_32k_ck",	&func_32k_ck,	CK_242X),
	CLK(NULL,	"secure_32k_ck", &secure_32k_ck, CK_242X),
	CLK(NULL,	"osc_ck",	&osc_ck,	CK_242X),
	CLK(NULL,	"sys_ck",	&sys_ck,	CK_242X),
	CLK(NULL,	"alt_ck",	&alt_ck,	CK_242X),
	CLK("omap-mcbsp.1",	"pad_fck",	&mcbsp_clks,	CK_242X),
	CLK("omap-mcbsp.2",	"pad_fck",	&mcbsp_clks,	CK_242X),
	CLK(NULL,	"mcbsp_clks",	&mcbsp_clks,	CK_242X),
	
	CLK(NULL,	"dpll_ck",	&dpll_ck,	CK_242X),
	CLK(NULL,	"apll96_ck",	&apll96_ck,	CK_242X),
	CLK(NULL,	"apll54_ck",	&apll54_ck,	CK_242X),
	
	CLK(NULL,	"func_54m_ck",	&func_54m_ck,	CK_242X),
	CLK(NULL,	"core_ck",	&core_ck,	CK_242X),
	CLK("omap-mcbsp.1",	"prcm_fck",	&func_96m_ck,	CK_242X),
	CLK("omap-mcbsp.2",	"prcm_fck",	&func_96m_ck,	CK_242X),
	CLK(NULL,	"func_96m_ck",	&func_96m_ck,	CK_242X),
	CLK(NULL,	"func_48m_ck",	&func_48m_ck,	CK_242X),
	CLK(NULL,	"func_12m_ck",	&func_12m_ck,	CK_242X),
	CLK(NULL,	"ck_wdt1_osc",	&wdt1_osc_ck,	CK_242X),
	CLK(NULL,	"sys_clkout_src", &sys_clkout_src, CK_242X),
	CLK(NULL,	"sys_clkout",	&sys_clkout,	CK_242X),
	CLK(NULL,	"sys_clkout2_src", &sys_clkout2_src, CK_242X),
	CLK(NULL,	"sys_clkout2",	&sys_clkout2,	CK_242X),
	CLK(NULL,	"emul_ck",	&emul_ck,	CK_242X),
	
	CLK(NULL,	"mpu_ck",	&mpu_ck,	CK_242X),
	
	CLK(NULL,	"dsp_fck",	&dsp_fck,	CK_242X),
	CLK(NULL,	"dsp_ick",	&dsp_ick,	CK_242X),
	CLK(NULL,	"iva1_ifck",	&iva1_ifck,	CK_242X),
	CLK(NULL,	"iva1_mpu_int_ifck", &iva1_mpu_int_ifck, CK_242X),
	
	CLK(NULL,	"gfx_3d_fck",	&gfx_3d_fck,	CK_242X),
	CLK(NULL,	"gfx_2d_fck",	&gfx_2d_fck,	CK_242X),
	CLK(NULL,	"gfx_ick",	&gfx_ick,	CK_242X),
	
	CLK("omapdss_dss",	"ick",		&dss_ick,	CK_242X),
	CLK(NULL,	"dss1_fck",		&dss1_fck,	CK_242X),
	CLK(NULL,	"dss2_fck",	&dss2_fck,	CK_242X),
	CLK(NULL,	"dss_54m_fck",	&dss_54m_fck,	CK_242X),
	
	CLK(NULL,	"core_l3_ck",	&core_l3_ck,	CK_242X),
	CLK(NULL,	"ssi_fck",	&ssi_ssr_sst_fck, CK_242X),
	CLK(NULL,	"usb_l4_ick",	&usb_l4_ick,	CK_242X),
	
	CLK(NULL,	"l4_ck",	&l4_ck,		CK_242X),
	CLK(NULL,	"ssi_l4_ick",	&ssi_l4_ick,	CK_242X),
	CLK(NULL,	"wu_l4_ick",	&wu_l4_ick,	CK_242X),
	
	CLK(NULL,	"virt_prcm_set", &virt_prcm_set, CK_242X),
	
	CLK(NULL,	"gpt1_ick",	&gpt1_ick,	CK_242X),
	CLK(NULL,	"gpt1_fck",	&gpt1_fck,	CK_242X),
	CLK(NULL,	"gpt2_ick",	&gpt2_ick,	CK_242X),
	CLK(NULL,	"gpt2_fck",	&gpt2_fck,	CK_242X),
	CLK(NULL,	"gpt3_ick",	&gpt3_ick,	CK_242X),
	CLK(NULL,	"gpt3_fck",	&gpt3_fck,	CK_242X),
	CLK(NULL,	"gpt4_ick",	&gpt4_ick,	CK_242X),
	CLK(NULL,	"gpt4_fck",	&gpt4_fck,	CK_242X),
	CLK(NULL,	"gpt5_ick",	&gpt5_ick,	CK_242X),
	CLK(NULL,	"gpt5_fck",	&gpt5_fck,	CK_242X),
	CLK(NULL,	"gpt6_ick",	&gpt6_ick,	CK_242X),
	CLK(NULL,	"gpt6_fck",	&gpt6_fck,	CK_242X),
	CLK(NULL,	"gpt7_ick",	&gpt7_ick,	CK_242X),
	CLK(NULL,	"gpt7_fck",	&gpt7_fck,	CK_242X),
	CLK(NULL,	"gpt8_ick",	&gpt8_ick,	CK_242X),
	CLK(NULL,	"gpt8_fck",	&gpt8_fck,	CK_242X),
	CLK(NULL,	"gpt9_ick",	&gpt9_ick,	CK_242X),
	CLK(NULL,	"gpt9_fck",	&gpt9_fck,	CK_242X),
	CLK(NULL,	"gpt10_ick",	&gpt10_ick,	CK_242X),
	CLK(NULL,	"gpt10_fck",	&gpt10_fck,	CK_242X),
	CLK(NULL,	"gpt11_ick",	&gpt11_ick,	CK_242X),
	CLK(NULL,	"gpt11_fck",	&gpt11_fck,	CK_242X),
	CLK(NULL,	"gpt12_ick",	&gpt12_ick,	CK_242X),
	CLK(NULL,	"gpt12_fck",	&gpt12_fck,	CK_242X),
	CLK("omap-mcbsp.1", "ick",	&mcbsp1_ick,	CK_242X),
	CLK(NULL,	"mcbsp1_fck",	&mcbsp1_fck,	CK_242X),
	CLK("omap-mcbsp.2", "ick",	&mcbsp2_ick,	CK_242X),
	CLK(NULL,	"mcbsp2_fck",	&mcbsp2_fck,	CK_242X),
	CLK("omap2_mcspi.1", "ick",	&mcspi1_ick,	CK_242X),
	CLK(NULL,	"mcspi1_fck",	&mcspi1_fck,	CK_242X),
	CLK("omap2_mcspi.2", "ick",	&mcspi2_ick,	CK_242X),
	CLK(NULL,	"mcspi2_fck",	&mcspi2_fck,	CK_242X),
	CLK(NULL,	"uart1_ick",	&uart1_ick,	CK_242X),
	CLK(NULL,	"uart1_fck",	&uart1_fck,	CK_242X),
	CLK(NULL,	"uart2_ick",	&uart2_ick,	CK_242X),
	CLK(NULL,	"uart2_fck",	&uart2_fck,	CK_242X),
	CLK(NULL,	"uart3_ick",	&uart3_ick,	CK_242X),
	CLK(NULL,	"uart3_fck",	&uart3_fck,	CK_242X),
	CLK(NULL,	"gpios_ick",	&gpios_ick,	CK_242X),
	CLK(NULL,	"gpios_fck",	&gpios_fck,	CK_242X),
	CLK("omap_wdt",	"ick",		&mpu_wdt_ick,	CK_242X),
	CLK(NULL,	"mpu_wdt_fck",	&mpu_wdt_fck,	CK_242X),
	CLK(NULL,	"sync_32k_ick",	&sync_32k_ick,	CK_242X),
	CLK(NULL,	"wdt1_ick",	&wdt1_ick,	CK_242X),
	CLK(NULL,	"omapctrl_ick",	&omapctrl_ick,	CK_242X),
	CLK("omap24xxcam", "fck",	&cam_fck,	CK_242X),
	CLK("omap24xxcam", "ick",	&cam_ick,	CK_242X),
	CLK(NULL,	"mailboxes_ick", &mailboxes_ick,	CK_242X),
	CLK(NULL,	"wdt4_ick",	&wdt4_ick,	CK_242X),
	CLK(NULL,	"wdt4_fck",	&wdt4_fck,	CK_242X),
	CLK(NULL,	"wdt3_ick",	&wdt3_ick,	CK_242X),
	CLK(NULL,	"wdt3_fck",	&wdt3_fck,	CK_242X),
	CLK(NULL,	"mspro_ick",	&mspro_ick,	CK_242X),
	CLK(NULL,	"mspro_fck",	&mspro_fck,	CK_242X),
	CLK("mmci-omap.0", "ick",	&mmc_ick,	CK_242X),
	CLK("mmci-omap.0", "fck",	&mmc_fck,	CK_242X),
	CLK(NULL,	"fac_ick",	&fac_ick,	CK_242X),
	CLK(NULL,	"fac_fck",	&fac_fck,	CK_242X),
	CLK(NULL,	"eac_ick",	&eac_ick,	CK_242X),
	CLK(NULL,	"eac_fck",	&eac_fck,	CK_242X),
	CLK("omap_hdq.0", "ick",	&hdq_ick,	CK_242X),
	CLK("omap_hdq.0", "fck",	&hdq_fck,	CK_242X),
	CLK("omap_i2c.1", "ick",	&i2c1_ick,	CK_242X),
	CLK(NULL,	"i2c1_fck",	&i2c1_fck,	CK_242X),
	CLK("omap_i2c.2", "ick",	&i2c2_ick,	CK_242X),
	CLK(NULL,	"i2c2_fck",	&i2c2_fck,	CK_242X),
	CLK(NULL,	"gpmc_fck",	&gpmc_fck,	CK_242X),
	CLK(NULL,	"sdma_fck",	&sdma_fck,	CK_242X),
	CLK(NULL,	"sdma_ick",	&sdma_ick,	CK_242X),
	CLK(NULL,	"sdrc_ick",	&sdrc_ick,	CK_242X),
	CLK(NULL,	"vlynq_ick",	&vlynq_ick,	CK_242X),
	CLK(NULL,	"vlynq_fck",	&vlynq_fck,	CK_242X),
	CLK(NULL,	"des_ick",	&des_ick,	CK_242X),
	CLK("omap-sham",	"ick",	&sha_ick,	CK_242X),
	CLK("omap_rng",	"ick",		&rng_ick,	CK_242X),
	CLK("omap-aes",	"ick",	&aes_ick,	CK_242X),
	CLK(NULL,	"pka_ick",	&pka_ick,	CK_242X),
	CLK(NULL,	"usb_fck",	&usb_fck,	CK_242X),
	CLK("musb-hdrc",	"fck",	&osc_ck,	CK_242X),
	CLK("omap_timer.1",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.2",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.3",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.4",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.5",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.6",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.7",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.8",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.9",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.10",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.11",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.12",	"32k_ck",	&func_32k_ck,	CK_243X),
	CLK("omap_timer.1",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.2",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.3",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.4",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.5",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.6",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.7",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.8",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.9",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.10",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.11",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.12",	"sys_ck",	&sys_ck,	CK_243X),
	CLK("omap_timer.1",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.2",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.3",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.4",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.5",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.6",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.7",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.8",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.9",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.10",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.11",	"alt_ck",	&alt_ck,	CK_243X),
	CLK("omap_timer.12",	"alt_ck",	&alt_ck,	CK_243X),
};


int __init omap2420_clk_init(void)
{
	const struct prcm_config *prcm;
	struct omap_clk *c;
	u32 clkrate;

	prcm_clksrc_ctrl = OMAP2420_PRCM_CLKSRC_CTRL;
	cm_idlest_pll = OMAP_CM_REGADDR(PLL_MOD, CM_IDLEST);
	cpu_mask = RATE_IN_242X;
	rate_table = omap2420_rate_table;

	clk_init(&omap2_clk_functions);

	for (c = omap2420_clks; c < omap2420_clks + ARRAY_SIZE(omap2420_clks);
	     c++)
		clk_preinit(c->lk.clk);

	osc_ck.rate = omap2_osc_clk_recalc(&osc_ck);
	propagate_rate(&osc_ck);
	sys_ck.rate = omap2xxx_sys_clk_recalc(&sys_ck);
	propagate_rate(&sys_ck);

	for (c = omap2420_clks; c < omap2420_clks + ARRAY_SIZE(omap2420_clks);
	     c++) {
		clkdev_add(&c->lk);
		clk_register(c->lk.clk);
		omap2_init_clk_clkdm(c->lk.clk);
	}

	
	omap_clk_disable_autoidle_all();

	
	clkrate = omap2xxx_clk_get_core_rate(&dpll_ck);
	for (prcm = rate_table; prcm->mpu_speed; prcm++) {
		if (!(prcm->flags & cpu_mask))
			continue;
		if (prcm->xtal_speed != sys_ck.rate)
			continue;
		if (prcm->dpll_speed <= clkrate)
			break;
	}
	curr_prcm_set = prcm;

	recalculate_root_clocks();

	pr_info("Clocking rate (Crystal/DPLL/MPU): %ld.%01ld/%ld/%ld MHz\n",
		(sys_ck.rate / 1000000), (sys_ck.rate / 100000) % 10,
		(dpll_ck.rate / 1000000), (mpu_ck.rate / 1000000)) ;

	clk_enable_init_clocks();

	
	vclk = clk_get(NULL, "virt_prcm_set");
	sclk = clk_get(NULL, "sys_ck");
	dclk = clk_get(NULL, "dpll_ck");

	return 0;
}

