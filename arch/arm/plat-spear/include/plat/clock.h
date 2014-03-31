/*
 * arch/arm/plat-spear/include/plat/clock.h
 *
 * Clock framework definitions for SPEAr platform
 *
 * Copyright (C) 2009 ST Microelectronics
 * Viresh Kumar<viresh.kumar@st.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __PLAT_CLOCK_H
#define __PLAT_CLOCK_H

#include <linux/list.h>
#include <linux/clkdev.h>
#include <linux/types.h>

#define	ALWAYS_ENABLED		(1 << 0) 
#define	RESET_TO_ENABLE		(1 << 1) 
#define	ENABLED_ON_INIT		(1 << 2) 

struct clkops {
	int (*enable) (struct clk *);
	void (*disable) (struct clk *);
};

/**
 * struct pclk_info - parents info
 * @pclk: pointer to parent clk
 * @pclk_val: value to be written for selecting this parent
 */
struct pclk_info {
	struct clk *pclk;
	u8 pclk_val;
};

struct pclk_sel {
	struct pclk_info *pclk_info;
	u8 pclk_count;
	void __iomem *pclk_sel_reg;
	unsigned int pclk_sel_mask;
};

struct rate_config {
	void *tbls;
	u8 count;
	u8 default_index;
};

struct clk {
	unsigned int usage_count;
	unsigned int flags;
	unsigned long rate;
	void __iomem *en_reg;
	u8 en_reg_bit;
	const struct clkops *ops;
	int (*recalc) (struct clk *);
	int (*set_rate) (struct clk *, unsigned long rate);
	unsigned long (*calc_rate)(struct clk *, int index);
	struct rate_config rate_config;
	unsigned int div_factor;

	struct clk *pclk;
	struct pclk_sel *pclk_sel;
	unsigned int pclk_sel_shift;

	struct list_head children;
	struct list_head sibling;
	void *private_data;
#ifdef CONFIG_DEBUG_FS
	struct list_head node;
	struct clk_lookup *cl;
	struct dentry *dent;
#endif
};

struct pll_clk_masks {
	u32 mode_mask;
	u32 mode_shift;

	u32 norm_fdbk_m_mask;
	u32 norm_fdbk_m_shift;
	u32 dith_fdbk_m_mask;
	u32 dith_fdbk_m_shift;
	u32 div_p_mask;
	u32 div_p_shift;
	u32 div_n_mask;
	u32 div_n_shift;
};

struct pll_clk_config {
	void __iomem *mode_reg;
	void __iomem *cfg_reg;
	struct pll_clk_masks *masks;
};

struct pll_rate_tbl {
	u8 mode;
	u16 m;
	u8 n;
	u8 p;
};

struct bus_clk_masks {
	u32 mask;
	u32 shift;
};

struct bus_clk_config {
	void __iomem *reg;
	struct bus_clk_masks *masks;
};

struct bus_rate_tbl {
	u8 div;
};

struct aux_clk_masks {
	u32 eq_sel_mask;
	u32 eq_sel_shift;
	u32 eq1_mask;
	u32 eq2_mask;
	u32 xscale_sel_mask;
	u32 xscale_sel_shift;
	u32 yscale_sel_mask;
	u32 yscale_sel_shift;
};

struct aux_clk_config {
	void __iomem *synth_reg;
	struct aux_clk_masks *masks;
};

struct aux_rate_tbl {
	u16 xscale;
	u16 yscale;
	u8 eq;
};

struct gpt_clk_masks {
	u32 mscale_sel_mask;
	u32 mscale_sel_shift;
	u32 nscale_sel_mask;
	u32 nscale_sel_shift;
};

struct gpt_clk_config {
	void __iomem *synth_reg;
	struct gpt_clk_masks *masks;
};

struct gpt_rate_tbl {
	u16 mscale;
	u16 nscale;
};

struct clcd_synth_masks {
	u32 div_factor_mask;
	u32 div_factor_shift;
};

struct clcd_clk_config {
	void __iomem *synth_reg;
	struct clcd_synth_masks *masks;
};

struct clcd_rate_tbl {
	u16 div;
};

void __init clk_init(void);
void clk_register(struct clk_lookup *cl);
void recalc_root_clocks(void);

int follow_parent(struct clk *clk);
unsigned long pll_calc_rate(struct clk *clk, int index);
int pll_clk_recalc(struct clk *clk);
int pll_clk_set_rate(struct clk *clk, unsigned long desired_rate);
unsigned long bus_calc_rate(struct clk *clk, int index);
int bus_clk_recalc(struct clk *clk);
int bus_clk_set_rate(struct clk *clk, unsigned long desired_rate);
unsigned long gpt_calc_rate(struct clk *clk, int index);
int gpt_clk_recalc(struct clk *clk);
int gpt_clk_set_rate(struct clk *clk, unsigned long desired_rate);
unsigned long aux_calc_rate(struct clk *clk, int index);
int aux_clk_recalc(struct clk *clk);
int aux_clk_set_rate(struct clk *clk, unsigned long desired_rate);
unsigned long clcd_calc_rate(struct clk *clk, int index);
int clcd_clk_recalc(struct clk *clk);
int clcd_clk_set_rate(struct clk *clk, unsigned long desired_rate);

#endif 
