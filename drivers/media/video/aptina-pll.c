/*
 * Aptina Sensor PLL Configuration
 *
 * Copyright (C) 2012 Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/device.h>
#include <linux/gcd.h>
#include <linux/kernel.h>
#include <linux/lcm.h>
#include <linux/module.h>

#include "aptina-pll.h"

int aptina_pll_calculate(struct device *dev,
			 const struct aptina_pll_limits *limits,
			 struct aptina_pll *pll)
{
	unsigned int mf_min;
	unsigned int mf_max;
	unsigned int p1_min;
	unsigned int p1_max;
	unsigned int p1;
	unsigned int div;

	dev_dbg(dev, "PLL: ext clock %u pix clock %u\n",
		pll->ext_clock, pll->pix_clock);

	if (pll->ext_clock < limits->ext_clock_min ||
	    pll->ext_clock > limits->ext_clock_max) {
		dev_err(dev, "pll: invalid external clock frequency.\n");
		return -EINVAL;
	}

	if (pll->pix_clock == 0 || pll->pix_clock > limits->pix_clock_max) {
		dev_err(dev, "pll: invalid pixel clock frequency.\n");
		return -EINVAL;
	}

	
	div = gcd(pll->pix_clock, pll->ext_clock);
	pll->m = pll->pix_clock / div;
	div = pll->ext_clock / div;

	mf_min = DIV_ROUND_UP(limits->m_min, pll->m);
	mf_min = max(mf_min, limits->out_clock_min /
		     (pll->ext_clock / limits->n_min * pll->m));
	mf_min = max(mf_min, limits->n_min * limits->p1_min / div);
	mf_max = limits->m_max / pll->m;
	mf_max = min(mf_max, limits->out_clock_max /
		    (pll->ext_clock / limits->n_max * pll->m));
	mf_max = min(mf_max, DIV_ROUND_UP(limits->n_max * limits->p1_max, div));

	dev_dbg(dev, "pll: mf min %u max %u\n", mf_min, mf_max);
	if (mf_min > mf_max) {
		dev_err(dev, "pll: no valid combined N*P1 divisor.\n");
		return -EINVAL;
	}

	if (limits->p1_min == 0) {
		dev_err(dev, "pll: P1 minimum value must be >0.\n");
		return -EINVAL;
	}

	p1_min = max(limits->p1_min, DIV_ROUND_UP(limits->out_clock_min * div,
		     pll->ext_clock * pll->m));
	p1_max = min(limits->p1_max, limits->out_clock_max * div /
		     (pll->ext_clock * pll->m));

	for (p1 = p1_max & ~1; p1 >= p1_min; p1 -= 2) {
		unsigned int mf_inc = p1 / gcd(div, p1);
		unsigned int mf_high;
		unsigned int mf_low;

		mf_low = max(roundup(mf_min, mf_inc),
			     DIV_ROUND_UP(pll->ext_clock * p1,
			       limits->int_clock_max * div));
		mf_high = min(mf_max, pll->ext_clock * p1 /
			      (limits->int_clock_min * div));

		if (mf_low > mf_high)
			continue;

		pll->n = div * mf_low / p1;
		pll->m *= mf_low;
		pll->p1 = p1;
		dev_dbg(dev, "PLL: N %u M %u P1 %u\n", pll->n, pll->m, pll->p1);
		return 0;
	}

	dev_err(dev, "pll: no valid N and P1 divisors found.\n");
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(aptina_pll_calculate);

MODULE_DESCRIPTION("Aptina PLL Helpers");
MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_LICENSE("GPL v2");
