/*
 *  linux/include/linux/clk-provider.h
 *
 *  Copyright (c) 2010-2011 Jeremy Kerr <jeremy.kerr@canonical.com>
 *  Copyright (C) 2011-2012 Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_CLK_PROVIDER_H
#define __LINUX_CLK_PROVIDER_H

#include <linux/clk.h>

#ifdef CONFIG_COMMON_CLK

struct clk_hw {
	struct clk *clk;
};

#define CLK_SET_RATE_GATE	BIT(0) 
#define CLK_SET_PARENT_GATE	BIT(1) 
#define CLK_SET_RATE_PARENT	BIT(2) 
#define CLK_IGNORE_UNUSED	BIT(3) 
#define CLK_IS_ROOT		BIT(4) 

struct clk_ops {
	int		(*prepare)(struct clk_hw *hw);
	void		(*unprepare)(struct clk_hw *hw);
	int		(*enable)(struct clk_hw *hw);
	void		(*disable)(struct clk_hw *hw);
	int		(*is_enabled)(struct clk_hw *hw);
	unsigned long	(*recalc_rate)(struct clk_hw *hw,
					unsigned long parent_rate);
	long		(*round_rate)(struct clk_hw *hw, unsigned long,
					unsigned long *);
	int		(*set_parent)(struct clk_hw *hw, u8 index);
	u8		(*get_parent)(struct clk_hw *hw);
	int		(*set_rate)(struct clk_hw *hw, unsigned long);
	void		(*init)(struct clk_hw *hw);
};


struct clk_fixed_rate {
	struct		clk_hw hw;
	unsigned long	fixed_rate;
	u8		flags;
};

struct clk *clk_register_fixed_rate(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		unsigned long fixed_rate);

struct clk_gate {
	struct clk_hw hw;
	void __iomem	*reg;
	u8		bit_idx;
	u8		flags;
	spinlock_t	*lock;
	char		*parent[1];
};

#define CLK_GATE_SET_TO_DISABLE		BIT(0)

struct clk *clk_register_gate(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *reg, u8 bit_idx,
		u8 clk_gate_flags, spinlock_t *lock);

struct clk_divider {
	struct clk_hw	hw;
	void __iomem	*reg;
	u8		shift;
	u8		width;
	u8		flags;
	spinlock_t	*lock;
	char		*parent[1];
};

#define CLK_DIVIDER_ONE_BASED		BIT(0)
#define CLK_DIVIDER_POWER_OF_TWO	BIT(1)

struct clk *clk_register_divider(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *reg, u8 shift, u8 width,
		u8 clk_divider_flags, spinlock_t *lock);

struct clk_mux {
	struct clk_hw	hw;
	void __iomem	*reg;
	u8		shift;
	u8		width;
	u8		flags;
	spinlock_t	*lock;
};

#define CLK_MUX_INDEX_ONE		BIT(0)
#define CLK_MUX_INDEX_BIT		BIT(1)

struct clk *clk_register_mux(struct device *dev, const char *name,
		char **parent_names, u8 num_parents, unsigned long flags,
		void __iomem *reg, u8 shift, u8 width,
		u8 clk_mux_flags, spinlock_t *lock);

struct clk *clk_register(struct device *dev, const char *name,
		const struct clk_ops *ops, struct clk_hw *hw,
		char **parent_names, u8 num_parents, unsigned long flags);

const char *__clk_get_name(struct clk *clk);
struct clk_hw *__clk_get_hw(struct clk *clk);
u8 __clk_get_num_parents(struct clk *clk);
struct clk *__clk_get_parent(struct clk *clk);
inline int __clk_get_enable_count(struct clk *clk);
inline int __clk_get_prepare_count(struct clk *clk);
unsigned long __clk_get_rate(struct clk *clk);
unsigned long __clk_get_flags(struct clk *clk);
int __clk_is_enabled(struct clk *clk);
struct clk *__clk_lookup(const char *name);

int __clk_prepare(struct clk *clk);
void __clk_unprepare(struct clk *clk);
void __clk_reparent(struct clk *clk, struct clk *new_parent);
unsigned long __clk_round_rate(struct clk *clk, unsigned long rate);

#endif 
#endif 
