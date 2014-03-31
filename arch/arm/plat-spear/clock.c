/*
 * arch/arm/plat-spear/clock.c
 *
 * Clock framework for SPEAr platform
 *
 * Copyright (C) 2009 ST Microelectronics
 * Viresh Kumar<viresh.kumar@st.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <plat/clock.h>

static DEFINE_SPINLOCK(clocks_lock);
static LIST_HEAD(root_clks);
#ifdef CONFIG_DEBUG_FS
static LIST_HEAD(clocks);
#endif

static void propagate_rate(struct clk *, int on_init);
#ifdef CONFIG_DEBUG_FS
static int clk_debugfs_reparent(struct clk *);
#endif

static int generic_clk_enable(struct clk *clk)
{
	unsigned int val;

	if (!clk->en_reg)
		return -EFAULT;

	val = readl(clk->en_reg);
	if (unlikely(clk->flags & RESET_TO_ENABLE))
		val &= ~(1 << clk->en_reg_bit);
	else
		val |= 1 << clk->en_reg_bit;

	writel(val, clk->en_reg);

	return 0;
}

static void generic_clk_disable(struct clk *clk)
{
	unsigned int val;

	if (!clk->en_reg)
		return;

	val = readl(clk->en_reg);
	if (unlikely(clk->flags & RESET_TO_ENABLE))
		val |= 1 << clk->en_reg_bit;
	else
		val &= ~(1 << clk->en_reg_bit);

	writel(val, clk->en_reg);
}

static struct clkops generic_clkops = {
	.enable = generic_clk_enable,
	.disable = generic_clk_disable,
};

static struct pclk_info *pclk_info_get(struct clk *clk)
{
	unsigned int val, i;
	struct pclk_info *info = NULL;

	val = (readl(clk->pclk_sel->pclk_sel_reg) >> clk->pclk_sel_shift)
		& clk->pclk_sel->pclk_sel_mask;

	for (i = 0; i < clk->pclk_sel->pclk_count; i++) {
		if (clk->pclk_sel->pclk_info[i].pclk_val == val)
			info = &clk->pclk_sel->pclk_info[i];
	}

	return info;
}

static void clk_reparent(struct clk *clk, struct pclk_info *pclk_info)
{
	unsigned long flags;

	spin_lock_irqsave(&clocks_lock, flags);
	list_del(&clk->sibling);
	list_add(&clk->sibling, &pclk_info->pclk->children);

	clk->pclk = pclk_info->pclk;
	spin_unlock_irqrestore(&clocks_lock, flags);

#ifdef CONFIG_DEBUG_FS
	clk_debugfs_reparent(clk);
#endif
}

static void do_clk_disable(struct clk *clk)
{
	if (!clk)
		return;

	if (!clk->usage_count) {
		WARN_ON(1);
		return;
	}

	clk->usage_count--;

	if (clk->usage_count == 0) {
		if (clk->pclk)
			do_clk_disable(clk->pclk);

		if (clk->ops && clk->ops->disable)
			clk->ops->disable(clk);
	}
}

static int do_clk_enable(struct clk *clk)
{
	int ret = 0;

	if (!clk)
		return -EFAULT;

	if (clk->usage_count == 0) {
		if (clk->pclk) {
			ret = do_clk_enable(clk->pclk);
			if (ret)
				goto err;
		}
		if (clk->ops && clk->ops->enable) {
			ret = clk->ops->enable(clk);
			if (ret) {
				if (clk->pclk)
					do_clk_disable(clk->pclk);
				goto err;
			}
		}
		if (clk->recalc) {
			ret = clk->recalc(clk);
			if (ret)
				goto err;
		}
	}
	clk->usage_count++;
err:
	return ret;
}

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&clocks_lock, flags);
	ret = do_clk_enable(clk);
	spin_unlock_irqrestore(&clocks_lock, flags);
	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&clocks_lock, flags);
	do_clk_disable(clk);
	spin_unlock_irqrestore(&clocks_lock, flags);
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	unsigned long flags, rate;

	spin_lock_irqsave(&clocks_lock, flags);
	rate = clk->rate;
	spin_unlock_irqrestore(&clocks_lock, flags);

	return rate;
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int i, found = 0, val = 0;
	unsigned long flags;

	if (!clk || !parent)
		return -EFAULT;
	if (clk->pclk == parent)
		return 0;
	if (!clk->pclk_sel)
		return -EPERM;

	
	for (i = 0; i < clk->pclk_sel->pclk_count; i++) {
		if (clk->pclk_sel->pclk_info[i].pclk == parent) {
			found = 1;
			break;
		}
	}

	if (!found)
		return -EINVAL;

	spin_lock_irqsave(&clocks_lock, flags);
	
	val = readl(clk->pclk_sel->pclk_sel_reg);
	val &= ~(clk->pclk_sel->pclk_sel_mask << clk->pclk_sel_shift);
	val |= clk->pclk_sel->pclk_info[i].pclk_val << clk->pclk_sel_shift;
	writel(val, clk->pclk_sel->pclk_sel_reg);
	spin_unlock_irqrestore(&clocks_lock, flags);

	
	clk_reparent(clk, &clk->pclk_sel->pclk_info[i]);

	propagate_rate(clk, 0);
	return 0;
}
EXPORT_SYMBOL(clk_set_parent);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	int ret = -EINVAL;

	if (!clk || !rate)
		return -EFAULT;

	if (clk->set_rate) {
		spin_lock_irqsave(&clocks_lock, flags);
		ret = clk->set_rate(clk, rate);
		if (!ret)
			
			propagate_rate(clk, 0);
		spin_unlock_irqrestore(&clocks_lock, flags);
	} else if (clk->pclk) {
		u32 mult = clk->div_factor ? clk->div_factor : 1;
		ret = clk_set_rate(clk->pclk, mult * rate);
	}

	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

void clk_register(struct clk_lookup *cl)
{
	struct clk *clk;
	unsigned long flags;

	if (!cl || !cl->clk)
		return;
	clk = cl->clk;

	spin_lock_irqsave(&clocks_lock, flags);

	INIT_LIST_HEAD(&clk->children);
	if (clk->flags & ALWAYS_ENABLED)
		clk->ops = NULL;
	else if (!clk->ops)
		clk->ops = &generic_clkops;

	
	if (!clk->pclk && !clk->pclk_sel) {
		list_add(&clk->sibling, &root_clks);
	} else if (clk->pclk && !clk->pclk_sel) {
		
		list_add(&clk->sibling, &clk->pclk->children);
	} else {
		
		struct pclk_info *pclk_info;

		pclk_info = pclk_info_get(clk);
		if (!pclk_info) {
			pr_err("CLKDEV: invalid pclk info of clk with"
					" %s dev_id and %s con_id\n",
					cl->dev_id, cl->con_id);
		} else {
			clk->pclk = pclk_info->pclk;
			list_add(&clk->sibling, &pclk_info->pclk->children);
		}
	}

	spin_unlock_irqrestore(&clocks_lock, flags);

	
#ifdef CONFIG_DEBUG_FS
	list_add(&clk->node, &clocks);
	clk->cl = cl;
#endif

	
	clkdev_add(cl);
}

void propagate_rate(struct clk *pclk, int on_init)
{
	struct clk *clk, *_temp;
	int ret = 0;

	list_for_each_entry_safe(clk, _temp, &pclk->children, sibling) {
		if (clk->recalc) {
			ret = clk->recalc(clk);
			if (ret && clk->set_rate)
				clk->set_rate(clk, 0);
		}
		propagate_rate(clk, on_init);

		if (!on_init)
			continue;

		
		if (clk->flags & ENABLED_ON_INIT)
			do_clk_enable(clk);
	}
}

/**
 * round_rate_index - return closest programmable rate index in rate_config tbl
 * @clk: ptr to clock structure
 * @drate: desired rate
 * @rate: final rate will be returned in this variable only.
 *
 * Finds index in rate_config for highest clk rate which is less than
 * requested rate. If there is no clk rate lesser than requested rate then
 * -EINVAL is returned. This routine assumes that rate_config is written
 * in incrementing order of clk rates.
 * If drate passed is zero then default rate is programmed.
 */
static int
round_rate_index(struct clk *clk, unsigned long drate, unsigned long *rate)
{
	unsigned long tmp = 0, prev_rate = 0;
	int index;

	if (!clk->calc_rate)
		return -EFAULT;

	if (!drate)
		return -EINVAL;

	for (index = 0; index < clk->rate_config.count; index++) {
		prev_rate = tmp;
		tmp = clk->calc_rate(clk, index);
		if (drate < tmp) {
			index--;
			break;
		}
	}
	
	if (index < 0) {
		index = -EINVAL;
		*rate = 0;
	} else if (index == clk->rate_config.count) {
		
		index = clk->rate_config.count - 1;
		*rate = tmp;
	} else
		*rate = prev_rate;

	return index;
}

long clk_round_rate(struct clk *clk, unsigned long drate)
{
	long rate = 0;
	int index;

	if (!clk->calc_rate) {
		u32 mult;
		if (!clk->pclk)
			return clk->rate;

		mult = clk->div_factor ? clk->div_factor : 1;
		return clk_round_rate(clk->pclk, mult * drate) / mult;
	}

	index = round_rate_index(clk, drate, &rate);
	if (index >= 0)
		return rate;
	else
		return index;
}
EXPORT_SYMBOL(clk_round_rate);


unsigned long pll_calc_rate(struct clk *clk, int index)
{
	unsigned long rate = clk->pclk->rate;
	struct pll_rate_tbl *tbls = clk->rate_config.tbls;
	unsigned int mode;

	mode = tbls[index].mode ? 256 : 1;
	return (((2 * rate / 10000) * tbls[index].m) /
			(mode * tbls[index].n * (1 << tbls[index].p))) * 10000;
}

int pll_clk_recalc(struct clk *clk)
{
	struct pll_clk_config *config = clk->private_data;
	unsigned int num = 2, den = 0, val, mode = 0;

	mode = (readl(config->mode_reg) >> config->masks->mode_shift) &
		config->masks->mode_mask;

	val = readl(config->cfg_reg);
	
	den = (val >> config->masks->div_p_shift) & config->masks->div_p_mask;
	den = 1 << den;
	den *= (val >> config->masks->div_n_shift) & config->masks->div_n_mask;

	
	if (!mode) {
		
		num *= (val >> config->masks->norm_fdbk_m_shift) &
			config->masks->norm_fdbk_m_mask;
	} else {
		
		num *= (val >> config->masks->dith_fdbk_m_shift) &
			config->masks->dith_fdbk_m_mask;
		den *= 256;
	}

	if (!den)
		return -EINVAL;

	clk->rate = (((clk->pclk->rate/10000) * num) / den) * 10000;
	return 0;
}

int pll_clk_set_rate(struct clk *clk, unsigned long desired_rate)
{
	struct pll_rate_tbl *tbls = clk->rate_config.tbls;
	struct pll_clk_config *config = clk->private_data;
	unsigned long val, rate;
	int i;

	i = round_rate_index(clk, desired_rate, &rate);
	if (i < 0)
		return i;

	val = readl(config->mode_reg) &
		~(config->masks->mode_mask << config->masks->mode_shift);
	val |= (tbls[i].mode & config->masks->mode_mask) <<
		config->masks->mode_shift;
	writel(val, config->mode_reg);

	val = readl(config->cfg_reg) &
		~(config->masks->div_p_mask << config->masks->div_p_shift);
	val |= (tbls[i].p & config->masks->div_p_mask) <<
		config->masks->div_p_shift;
	val &= ~(config->masks->div_n_mask << config->masks->div_n_shift);
	val |= (tbls[i].n & config->masks->div_n_mask) <<
		config->masks->div_n_shift;
	val &= ~(config->masks->dith_fdbk_m_mask <<
			config->masks->dith_fdbk_m_shift);
	if (tbls[i].mode)
		val |= (tbls[i].m & config->masks->dith_fdbk_m_mask) <<
			config->masks->dith_fdbk_m_shift;
	else
		val |= (tbls[i].m & config->masks->norm_fdbk_m_mask) <<
			config->masks->norm_fdbk_m_shift;

	writel(val, config->cfg_reg);

	clk->rate = rate;

	return 0;
}

unsigned long bus_calc_rate(struct clk *clk, int index)
{
	unsigned long rate = clk->pclk->rate;
	struct bus_rate_tbl *tbls = clk->rate_config.tbls;

	return rate / (tbls[index].div + 1);
}

int bus_clk_recalc(struct clk *clk)
{
	struct bus_clk_config *config = clk->private_data;
	unsigned int div;

	div = ((readl(config->reg) >> config->masks->shift) &
			config->masks->mask) + 1;

	if (!div)
		return -EINVAL;

	clk->rate = (unsigned long)clk->pclk->rate / div;
	return 0;
}

int bus_clk_set_rate(struct clk *clk, unsigned long desired_rate)
{
	struct bus_rate_tbl *tbls = clk->rate_config.tbls;
	struct bus_clk_config *config = clk->private_data;
	unsigned long val, rate;
	int i;

	i = round_rate_index(clk, desired_rate, &rate);
	if (i < 0)
		return i;

	val = readl(config->reg) &
		~(config->masks->mask << config->masks->shift);
	val |= (tbls[i].div & config->masks->mask) << config->masks->shift;
	writel(val, config->reg);

	clk->rate = rate;

	return 0;
}

unsigned long aux_calc_rate(struct clk *clk, int index)
{
	unsigned long rate = clk->pclk->rate;
	struct aux_rate_tbl *tbls = clk->rate_config.tbls;
	u8 eq = tbls[index].eq ? 1 : 2;

	return (((rate/10000) * tbls[index].xscale) /
			(tbls[index].yscale * eq)) * 10000;
}

int aux_clk_recalc(struct clk *clk)
{
	struct aux_clk_config *config = clk->private_data;
	unsigned int num = 1, den = 1, val, eqn;

	val = readl(config->synth_reg);

	eqn = (val >> config->masks->eq_sel_shift) &
		config->masks->eq_sel_mask;
	if (eqn == config->masks->eq1_mask)
		den *= 2;

	
	num = (val >> config->masks->xscale_sel_shift) &
		config->masks->xscale_sel_mask;

	
	den *= (val >> config->masks->yscale_sel_shift) &
		config->masks->yscale_sel_mask;

	if (!den)
		return -EINVAL;

	clk->rate = (((clk->pclk->rate/10000) * num) / den) * 10000;
	return 0;
}

int aux_clk_set_rate(struct clk *clk, unsigned long desired_rate)
{
	struct aux_rate_tbl *tbls = clk->rate_config.tbls;
	struct aux_clk_config *config = clk->private_data;
	unsigned long val, rate;
	int i;

	i = round_rate_index(clk, desired_rate, &rate);
	if (i < 0)
		return i;

	val = readl(config->synth_reg) &
		~(config->masks->eq_sel_mask << config->masks->eq_sel_shift);
	val |= (tbls[i].eq & config->masks->eq_sel_mask) <<
		config->masks->eq_sel_shift;
	val &= ~(config->masks->xscale_sel_mask <<
			config->masks->xscale_sel_shift);
	val |= (tbls[i].xscale & config->masks->xscale_sel_mask) <<
		config->masks->xscale_sel_shift;
	val &= ~(config->masks->yscale_sel_mask <<
			config->masks->yscale_sel_shift);
	val |= (tbls[i].yscale & config->masks->yscale_sel_mask) <<
		config->masks->yscale_sel_shift;
	writel(val, config->synth_reg);

	clk->rate = rate;

	return 0;
}

unsigned long gpt_calc_rate(struct clk *clk, int index)
{
	unsigned long rate = clk->pclk->rate;
	struct gpt_rate_tbl *tbls = clk->rate_config.tbls;

	return rate / ((1 << (tbls[index].nscale + 1)) *
			(tbls[index].mscale + 1));
}

int gpt_clk_recalc(struct clk *clk)
{
	struct gpt_clk_config *config = clk->private_data;
	unsigned int div = 1, val;

	val = readl(config->synth_reg);
	div += (val >> config->masks->mscale_sel_shift) &
		config->masks->mscale_sel_mask;
	div *= 1 << (((val >> config->masks->nscale_sel_shift) &
				config->masks->nscale_sel_mask) + 1);

	if (!div)
		return -EINVAL;

	clk->rate = (unsigned long)clk->pclk->rate / div;
	return 0;
}

int gpt_clk_set_rate(struct clk *clk, unsigned long desired_rate)
{
	struct gpt_rate_tbl *tbls = clk->rate_config.tbls;
	struct gpt_clk_config *config = clk->private_data;
	unsigned long val, rate;
	int i;

	i = round_rate_index(clk, desired_rate, &rate);
	if (i < 0)
		return i;

	val = readl(config->synth_reg) & ~(config->masks->mscale_sel_mask <<
			config->masks->mscale_sel_shift);
	val |= (tbls[i].mscale & config->masks->mscale_sel_mask) <<
		config->masks->mscale_sel_shift;
	val &= ~(config->masks->nscale_sel_mask <<
			config->masks->nscale_sel_shift);
	val |= (tbls[i].nscale & config->masks->nscale_sel_mask) <<
		config->masks->nscale_sel_shift;
	writel(val, config->synth_reg);

	clk->rate = rate;

	return 0;
}

unsigned long clcd_calc_rate(struct clk *clk, int index)
{
	unsigned long rate = clk->pclk->rate;
	struct clcd_rate_tbl *tbls = clk->rate_config.tbls;

	rate /= 1000;
	rate <<= 12;
	rate /= (2 * tbls[index].div);
	rate >>= 12;
	rate *= 1000;

	return rate;
}

int clcd_clk_recalc(struct clk *clk)
{
	struct clcd_clk_config *config = clk->private_data;
	unsigned int div = 1;
	unsigned long prate;
	unsigned int val;

	val = readl(config->synth_reg);
	div = (val >> config->masks->div_factor_shift) &
		config->masks->div_factor_mask;

	if (!div)
		return -EINVAL;

	prate = clk->pclk->rate / 1000; 

	clk->rate = (((unsigned long)prate << 12) / (2 * div)) >> 12;
	clk->rate *= 1000;
	return 0;
}

int clcd_clk_set_rate(struct clk *clk, unsigned long desired_rate)
{
	struct clcd_rate_tbl *tbls = clk->rate_config.tbls;
	struct clcd_clk_config *config = clk->private_data;
	unsigned long val, rate;
	int i;

	i = round_rate_index(clk, desired_rate, &rate);
	if (i < 0)
		return i;

	val = readl(config->synth_reg) & ~(config->masks->div_factor_mask <<
			config->masks->div_factor_shift);
	val |= (tbls[i].div & config->masks->div_factor_mask) <<
		config->masks->div_factor_shift;
	writel(val, config->synth_reg);

	clk->rate = rate;

	return 0;
}

int follow_parent(struct clk *clk)
{
	unsigned int div_factor = (clk->div_factor < 1) ? 1 : clk->div_factor;

	clk->rate = clk->pclk->rate/div_factor;
	return 0;
}

void recalc_root_clocks(void)
{
	struct clk *pclk;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&clocks_lock, flags);
	list_for_each_entry(pclk, &root_clks, sibling) {
		if (pclk->recalc) {
			ret = pclk->recalc(pclk);
			if (ret && pclk->set_rate)
				pclk->set_rate(pclk, 0);
		}
		propagate_rate(pclk, 1);
		
		if (pclk->flags & ENABLED_ON_INIT)
			do_clk_enable(pclk);
	}
	spin_unlock_irqrestore(&clocks_lock, flags);
}

void __init clk_init(void)
{
	recalc_root_clocks();
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *clk_debugfs_root;
static int clk_debugfs_register_one(struct clk *c)
{
	int err;
	struct dentry *d;
	struct clk *pa = c->pclk;
	char s[255];
	char *p = s;

	if (c) {
		if (c->cl->con_id)
			p += sprintf(p, "%s", c->cl->con_id);
		if (c->cl->dev_id)
			p += sprintf(p, "%s", c->cl->dev_id);
	}
	d = debugfs_create_dir(s, pa ? pa->dent : clk_debugfs_root);
	if (!d)
		return -ENOMEM;
	c->dent = d;

	d = debugfs_create_u32("usage_count", S_IRUGO, c->dent,
			(u32 *)&c->usage_count);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	d = debugfs_create_u32("rate", S_IRUGO, c->dent, (u32 *)&c->rate);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	d = debugfs_create_x32("flags", S_IRUGO, c->dent, (u32 *)&c->flags);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}
	return 0;

err_out:
	debugfs_remove_recursive(c->dent);
	return err;
}

static int clk_debugfs_register(struct clk *c)
{
	int err;
	struct clk *pa = c->pclk;

	if (pa && !pa->dent) {
		err = clk_debugfs_register(pa);
		if (err)
			return err;
	}

	if (!c->dent) {
		err = clk_debugfs_register_one(c);
		if (err)
			return err;
	}
	return 0;
}

static int __init clk_debugfs_init(void)
{
	struct clk *c;
	struct dentry *d;
	int err;

	d = debugfs_create_dir("clock", NULL);
	if (!d)
		return -ENOMEM;
	clk_debugfs_root = d;

	list_for_each_entry(c, &clocks, node) {
		err = clk_debugfs_register(c);
		if (err)
			goto err_out;
	}
	return 0;
err_out:
	debugfs_remove_recursive(clk_debugfs_root);
	return err;
}
late_initcall(clk_debugfs_init);

static int clk_debugfs_reparent(struct clk *c)
{
	debugfs_remove(c->dent);
	return clk_debugfs_register_one(c);
}
#endif 
