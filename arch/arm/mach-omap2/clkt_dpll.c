/*
 * OMAP2/3/4 DPLL clock functions
 *
 * Copyright (C) 2005-2008 Texas Instruments, Inc.
 * Copyright (C) 2004-2010 Nokia Corporation
 *
 * Contacts:
 * Richard Woodruff <r-woodruff2@ti.com>
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <asm/div64.h>

#include <plat/clock.h>
#include <plat/cpu.h>

#include "clock.h"
#include "cm-regbits-24xx.h"
#include "cm-regbits-34xx.h"

#define DPLL_MIN_MULTIPLIER		2
#define DPLL_MIN_DIVIDER		1

#define DPLL_MULT_UNDERFLOW		-1

#define DPLL_SCALE_FACTOR		64
#define DPLL_SCALE_BASE			2
#define DPLL_ROUNDING_VAL		((DPLL_SCALE_BASE / 2) * \
					 (DPLL_SCALE_FACTOR / DPLL_SCALE_BASE))

#define OMAP3430_DPLL_FINT_BAND1_MIN	750000
#define OMAP3430_DPLL_FINT_BAND1_MAX	2100000
#define OMAP3430_DPLL_FINT_BAND2_MIN	7500000
#define OMAP3430_DPLL_FINT_BAND2_MAX	21000000

#define OMAP3PLUS_DPLL_FINT_JTYPE_MIN	500000
#define OMAP3PLUS_DPLL_FINT_JTYPE_MAX	2500000
#define OMAP3PLUS_DPLL_FINT_MIN		32000
#define OMAP3PLUS_DPLL_FINT_MAX		52000000

#define DPLL_FINT_UNDERFLOW		-1
#define DPLL_FINT_INVALID		-2


static int _dpll_test_fint(struct clk *clk, u8 n)
{
	struct dpll_data *dd;
	long fint, fint_min, fint_max;
	int ret = 0;

	dd = clk->dpll_data;

	
	fint = clk->parent->rate / n;

	if (cpu_is_omap24xx()) {
		
		WARN(1, "No fint limits available for OMAP2!\n");
		return DPLL_FINT_INVALID;
	} else if (cpu_is_omap3430()) {
		fint_min = OMAP3430_DPLL_FINT_BAND1_MIN;
		fint_max = OMAP3430_DPLL_FINT_BAND2_MAX;
	} else if (dd->flags & DPLL_J_TYPE) {
		fint_min = OMAP3PLUS_DPLL_FINT_JTYPE_MIN;
		fint_max = OMAP3PLUS_DPLL_FINT_JTYPE_MAX;
	} else {
		fint_min = OMAP3PLUS_DPLL_FINT_MIN;
		fint_max = OMAP3PLUS_DPLL_FINT_MAX;
	}

	if (fint < fint_min) {
		pr_debug("rejecting n=%d due to Fint failure, "
			 "lowering max_divider\n", n);
		dd->max_divider = n;
		ret = DPLL_FINT_UNDERFLOW;
	} else if (fint > fint_max) {
		pr_debug("rejecting n=%d due to Fint failure, "
			 "boosting min_divider\n", n);
		dd->min_divider = n;
		ret = DPLL_FINT_INVALID;
	} else if (cpu_is_omap3430() && fint > OMAP3430_DPLL_FINT_BAND1_MAX &&
		   fint < OMAP3430_DPLL_FINT_BAND2_MIN) {
		pr_debug("rejecting n=%d due to Fint failure\n", n);
		ret = DPLL_FINT_INVALID;
	}

	return ret;
}

static unsigned long _dpll_compute_new_rate(unsigned long parent_rate,
					    unsigned int m, unsigned int n)
{
	unsigned long long num;

	num = (unsigned long long)parent_rate * m;
	do_div(num, n);
	return num;
}

static int _dpll_test_mult(int *m, int n, unsigned long *new_rate,
			   unsigned long target_rate,
			   unsigned long parent_rate)
{
	int r = 0, carry = 0;

	
	if (*m % DPLL_SCALE_FACTOR >= DPLL_ROUNDING_VAL)
		carry = 1;
	*m = (*m / DPLL_SCALE_FACTOR) + carry;

	*new_rate = _dpll_compute_new_rate(parent_rate, *m, n);
	if (*new_rate > target_rate) {
		(*m)--;
		*new_rate = 0;
	}

	
	if (*m < DPLL_MIN_MULTIPLIER) {
		*m = DPLL_MIN_MULTIPLIER;
		*new_rate = 0;
		r = DPLL_MULT_UNDERFLOW;
	}

	if (*new_rate == 0)
		*new_rate = _dpll_compute_new_rate(parent_rate, *m, n);

	return r;
}


void omap2_init_dpll_parent(struct clk *clk)
{
	u32 v;
	struct dpll_data *dd;

	dd = clk->dpll_data;
	if (!dd)
		return;

	v = __raw_readl(dd->control_reg);
	v &= dd->enable_mask;
	v >>= __ffs(dd->enable_mask);

	
	if (cpu_is_omap24xx()) {
		if (v == OMAP2XXX_EN_DPLL_LPBYPASS ||
		    v == OMAP2XXX_EN_DPLL_FRBYPASS)
			clk_reparent(clk, dd->clk_bypass);
	} else if (cpu_is_omap34xx()) {
		if (v == OMAP3XXX_EN_DPLL_LPBYPASS ||
		    v == OMAP3XXX_EN_DPLL_FRBYPASS)
			clk_reparent(clk, dd->clk_bypass);
	} else if (cpu_is_omap44xx()) {
		if (v == OMAP4XXX_EN_DPLL_LPBYPASS ||
		    v == OMAP4XXX_EN_DPLL_FRBYPASS ||
		    v == OMAP4XXX_EN_DPLL_MNBYPASS)
			clk_reparent(clk, dd->clk_bypass);
	}
	return;
}

u32 omap2_get_dpll_rate(struct clk *clk)
{
	long long dpll_clk;
	u32 dpll_mult, dpll_div, v;
	struct dpll_data *dd;

	dd = clk->dpll_data;
	if (!dd)
		return 0;

	
	v = __raw_readl(dd->control_reg);
	v &= dd->enable_mask;
	v >>= __ffs(dd->enable_mask);

	if (cpu_is_omap24xx()) {
		if (v == OMAP2XXX_EN_DPLL_LPBYPASS ||
		    v == OMAP2XXX_EN_DPLL_FRBYPASS)
			return dd->clk_bypass->rate;
	} else if (cpu_is_omap34xx()) {
		if (v == OMAP3XXX_EN_DPLL_LPBYPASS ||
		    v == OMAP3XXX_EN_DPLL_FRBYPASS)
			return dd->clk_bypass->rate;
	} else if (cpu_is_omap44xx()) {
		if (v == OMAP4XXX_EN_DPLL_LPBYPASS ||
		    v == OMAP4XXX_EN_DPLL_FRBYPASS ||
		    v == OMAP4XXX_EN_DPLL_MNBYPASS)
			return dd->clk_bypass->rate;
	}

	v = __raw_readl(dd->mult_div1_reg);
	dpll_mult = v & dd->mult_mask;
	dpll_mult >>= __ffs(dd->mult_mask);
	dpll_div = v & dd->div1_mask;
	dpll_div >>= __ffs(dd->div1_mask);

	dpll_clk = (long long)dd->clk_ref->rate * dpll_mult;
	do_div(dpll_clk, dpll_div + 1);

	return dpll_clk;
}


long omap2_dpll_round_rate(struct clk *clk, unsigned long target_rate)
{
	int m, n, r, scaled_max_m;
	unsigned long scaled_rt_rp;
	unsigned long new_rate = 0;
	struct dpll_data *dd;

	if (!clk || !clk->dpll_data)
		return ~0;

	dd = clk->dpll_data;

	pr_debug("clock: %s: starting DPLL round_rate, target rate %ld\n",
		 clk->name, target_rate);

	scaled_rt_rp = target_rate / (dd->clk_ref->rate / DPLL_SCALE_FACTOR);
	scaled_max_m = dd->max_multiplier * DPLL_SCALE_FACTOR;

	dd->last_rounded_rate = 0;

	for (n = dd->min_divider; n <= dd->max_divider; n++) {

		
		r = _dpll_test_fint(clk, n);
		if (r == DPLL_FINT_UNDERFLOW)
			break;
		else if (r == DPLL_FINT_INVALID)
			continue;

		
		m = scaled_rt_rp * n;

		if (m > scaled_max_m)
			break;

		r = _dpll_test_mult(&m, n, &new_rate, target_rate,
				    dd->clk_ref->rate);

		
		if (r == DPLL_MULT_UNDERFLOW)
			continue;

		pr_debug("clock: %s: m = %d: n = %d: new_rate = %ld\n",
			 clk->name, m, n, new_rate);

		if (target_rate == new_rate) {
			dd->last_rounded_m = m;
			dd->last_rounded_n = n;
			dd->last_rounded_rate = target_rate;
			break;
		}
	}

	if (target_rate != new_rate) {
		pr_debug("clock: %s: cannot round to rate %ld\n", clk->name,
			 target_rate);
		return ~0;
	}

	return target_rate;
}

