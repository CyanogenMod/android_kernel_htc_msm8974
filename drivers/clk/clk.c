/*
 * Copyright (C) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
 * Copyright (C) 2011-2012 Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Standard functionality for the common clock API.  See Documentation/clk.txt
 */

#include <linux/clk-private.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/slab.h>

static DEFINE_SPINLOCK(enable_lock);
static DEFINE_MUTEX(prepare_lock);

static HLIST_HEAD(clk_root_list);
static HLIST_HEAD(clk_orphan_list);
static LIST_HEAD(clk_notifier_list);


#ifdef CONFIG_COMMON_CLK_DEBUG
#include <linux/debugfs.h>

static struct dentry *rootdir;
static struct dentry *orphandir;
static int inited = 0;

static int clk_debug_create_one(struct clk *clk, struct dentry *pdentry)
{
	struct dentry *d;
	int ret = -ENOMEM;

	if (!clk || !pdentry) {
		ret = -EINVAL;
		goto out;
	}

	d = debugfs_create_dir(clk->name, pdentry);
	if (!d)
		goto out;

	clk->dentry = d;

	d = debugfs_create_u32("clk_rate", S_IRUGO, clk->dentry,
			(u32 *)&clk->rate);
	if (!d)
		goto err_out;

	d = debugfs_create_x32("clk_flags", S_IRUGO, clk->dentry,
			(u32 *)&clk->flags);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("clk_prepare_count", S_IRUGO, clk->dentry,
			(u32 *)&clk->prepare_count);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("clk_enable_count", S_IRUGO, clk->dentry,
			(u32 *)&clk->enable_count);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("clk_notifier_count", S_IRUGO, clk->dentry,
			(u32 *)&clk->notifier_count);
	if (!d)
		goto err_out;

	ret = 0;
	goto out;

err_out:
	debugfs_remove(clk->dentry);
out:
	return ret;
}

static int clk_debug_create_subtree(struct clk *clk, struct dentry *pdentry)
{
	struct clk *child;
	struct hlist_node *tmp;
	int ret = -EINVAL;;

	if (!clk || !pdentry)
		goto out;

	ret = clk_debug_create_one(clk, pdentry);

	if (ret)
		goto out;

	hlist_for_each_entry(child, tmp, &clk->children, child_node)
		clk_debug_create_subtree(child, clk->dentry);

	ret = 0;
out:
	return ret;
}

static int clk_debug_register(struct clk *clk)
{
	struct clk *parent;
	struct dentry *pdentry;
	int ret = 0;

	if (!inited)
		goto out;

	parent = clk->parent;

	if (!parent)
		if (clk->flags & CLK_IS_ROOT)
			pdentry = rootdir;
		else
			pdentry = orphandir;
	else
		if (parent->dentry)
			pdentry = parent->dentry;
		else
			goto out;

	ret = clk_debug_create_subtree(clk, pdentry);

out:
	return ret;
}

static int __init clk_debug_init(void)
{
	struct clk *clk;
	struct hlist_node *tmp;

	rootdir = debugfs_create_dir("clk", NULL);

	if (!rootdir)
		return -ENOMEM;

	orphandir = debugfs_create_dir("orphans", rootdir);

	if (!orphandir)
		return -ENOMEM;

	mutex_lock(&prepare_lock);

	hlist_for_each_entry(clk, tmp, &clk_root_list, child_node)
		clk_debug_create_subtree(clk, rootdir);

	hlist_for_each_entry(clk, tmp, &clk_orphan_list, child_node)
		clk_debug_create_subtree(clk, orphandir);

	inited = 1;

	mutex_unlock(&prepare_lock);

	return 0;
}
late_initcall(clk_debug_init);
#else
static inline int clk_debug_register(struct clk *clk) { return 0; }
#endif 

#ifdef CONFIG_COMMON_CLK_DISABLE_UNUSED
static void clk_disable_unused_subtree(struct clk *clk)
{
	struct clk *child;
	struct hlist_node *tmp;
	unsigned long flags;

	if (!clk)
		goto out;

	hlist_for_each_entry(child, tmp, &clk->children, child_node)
		clk_disable_unused_subtree(child);

	spin_lock_irqsave(&enable_lock, flags);

	if (clk->enable_count)
		goto unlock_out;

	if (clk->flags & CLK_IGNORE_UNUSED)
		goto unlock_out;

	if (__clk_is_enabled(clk) && clk->ops->disable)
		clk->ops->disable(clk->hw);

unlock_out:
	spin_unlock_irqrestore(&enable_lock, flags);

out:
	return;
}

static int clk_disable_unused(void)
{
	struct clk *clk;
	struct hlist_node *tmp;

	mutex_lock(&prepare_lock);

	hlist_for_each_entry(clk, tmp, &clk_root_list, child_node)
		clk_disable_unused_subtree(clk);

	hlist_for_each_entry(clk, tmp, &clk_orphan_list, child_node)
		clk_disable_unused_subtree(clk);

	mutex_unlock(&prepare_lock);

	return 0;
}
late_initcall(clk_disable_unused);
#else
static inline int clk_disable_unused(struct clk *clk) { return 0; }
#endif 


inline const char *__clk_get_name(struct clk *clk)
{
	return !clk ? NULL : clk->name;
}

inline struct clk_hw *__clk_get_hw(struct clk *clk)
{
	return !clk ? NULL : clk->hw;
}

inline u8 __clk_get_num_parents(struct clk *clk)
{
	return !clk ? -EINVAL : clk->num_parents;
}

inline struct clk *__clk_get_parent(struct clk *clk)
{
	return !clk ? NULL : clk->parent;
}

inline int __clk_get_enable_count(struct clk *clk)
{
	return !clk ? -EINVAL : clk->enable_count;
}

inline int __clk_get_prepare_count(struct clk *clk)
{
	return !clk ? -EINVAL : clk->prepare_count;
}

unsigned long __clk_get_rate(struct clk *clk)
{
	unsigned long ret;

	if (!clk) {
		ret = -EINVAL;
		goto out;
	}

	ret = clk->rate;

	if (clk->flags & CLK_IS_ROOT)
		goto out;

	if (!clk->parent)
		ret = -ENODEV;

out:
	return ret;
}

inline unsigned long __clk_get_flags(struct clk *clk)
{
	return !clk ? -EINVAL : clk->flags;
}

int __clk_is_enabled(struct clk *clk)
{
	int ret;

	if (!clk)
		return -EINVAL;

	if (!clk->ops->is_enabled) {
		ret = clk->enable_count ? 1 : 0;
		goto out;
	}

	ret = clk->ops->is_enabled(clk->hw);
out:
	return ret;
}

static struct clk *__clk_lookup_subtree(const char *name, struct clk *clk)
{
	struct clk *child;
	struct clk *ret;
	struct hlist_node *tmp;

	if (!strcmp(clk->name, name))
		return clk;

	hlist_for_each_entry(child, tmp, &clk->children, child_node) {
		ret = __clk_lookup_subtree(name, child);
		if (ret)
			return ret;
	}

	return NULL;
}

struct clk *__clk_lookup(const char *name)
{
	struct clk *root_clk;
	struct clk *ret;
	struct hlist_node *tmp;

	if (!name)
		return NULL;

	
	hlist_for_each_entry(root_clk, tmp, &clk_root_list, child_node) {
		ret = __clk_lookup_subtree(name, root_clk);
		if (ret)
			return ret;
	}

	
	hlist_for_each_entry(root_clk, tmp, &clk_orphan_list, child_node) {
		ret = __clk_lookup_subtree(name, root_clk);
		if (ret)
			return ret;
	}

	return NULL;
}


void __clk_unprepare(struct clk *clk)
{
	if (!clk)
		return;

	if (WARN_ON(clk->prepare_count == 0))
		return;

	if (--clk->prepare_count > 0)
		return;

	WARN_ON(clk->enable_count > 0);

	if (clk->ops->unprepare)
		clk->ops->unprepare(clk->hw);

	__clk_unprepare(clk->parent);
}

void clk_unprepare(struct clk *clk)
{
	mutex_lock(&prepare_lock);
	__clk_unprepare(clk);
	mutex_unlock(&prepare_lock);
}
EXPORT_SYMBOL_GPL(clk_unprepare);

int __clk_prepare(struct clk *clk)
{
	int ret = 0;

	if (!clk)
		return 0;

	if (clk->prepare_count == 0) {
		ret = __clk_prepare(clk->parent);
		if (ret)
			return ret;

		if (clk->ops->prepare) {
			ret = clk->ops->prepare(clk->hw);
			if (ret) {
				__clk_unprepare(clk->parent);
				return ret;
			}
		}
	}

	clk->prepare_count++;

	return 0;
}

int clk_prepare(struct clk *clk)
{
	int ret;

	mutex_lock(&prepare_lock);
	ret = __clk_prepare(clk);
	mutex_unlock(&prepare_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_prepare);

static void __clk_disable(struct clk *clk)
{
	if (!clk)
		return;

	if (WARN_ON(clk->enable_count == 0))
		return;

	if (--clk->enable_count > 0)
		return;

	if (clk->ops->disable)
		clk->ops->disable(clk->hw);

	__clk_disable(clk->parent);
}

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&enable_lock, flags);
	__clk_disable(clk);
	spin_unlock_irqrestore(&enable_lock, flags);
}
EXPORT_SYMBOL_GPL(clk_disable);

static int __clk_enable(struct clk *clk)
{
	int ret = 0;

	if (!clk)
		return 0;

	if (WARN_ON(clk->prepare_count == 0))
		return -ESHUTDOWN;

	if (clk->enable_count == 0) {
		ret = __clk_enable(clk->parent);

		if (ret)
			return ret;

		if (clk->ops->enable) {
			ret = clk->ops->enable(clk->hw);
			if (ret) {
				__clk_disable(clk->parent);
				return ret;
			}
		}
	}

	clk->enable_count++;
	return 0;
}

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&enable_lock, flags);
	ret = __clk_enable(clk);
	spin_unlock_irqrestore(&enable_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_enable);

unsigned long clk_get_rate(struct clk *clk)
{
	unsigned long rate;

	mutex_lock(&prepare_lock);
	rate = __clk_get_rate(clk);
	mutex_unlock(&prepare_lock);

	return rate;
}
EXPORT_SYMBOL_GPL(clk_get_rate);

unsigned long __clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long unused;

	if (!clk)
		return -EINVAL;

	if (!clk->ops->round_rate)
		return clk->rate;

	if (clk->flags & CLK_SET_RATE_PARENT)
		return clk->ops->round_rate(clk->hw, rate, &unused);
	else
		return clk->ops->round_rate(clk->hw, rate, NULL);
}

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long ret;

	mutex_lock(&prepare_lock);
	ret = __clk_round_rate(clk, rate);
	mutex_unlock(&prepare_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_round_rate);

static int __clk_notify(struct clk *clk, unsigned long msg,
		unsigned long old_rate, unsigned long new_rate)
{
	struct clk_notifier *cn;
	struct clk_notifier_data cnd;
	int ret = NOTIFY_DONE;

	cnd.clk = clk;
	cnd.old_rate = old_rate;
	cnd.new_rate = new_rate;

	list_for_each_entry(cn, &clk_notifier_list, node) {
		if (cn->clk == clk) {
			ret = srcu_notifier_call_chain(&cn->notifier_head, msg,
					&cnd);
			break;
		}
	}

	return ret;
}

static void __clk_recalc_rates(struct clk *clk, unsigned long msg)
{
	unsigned long old_rate;
	unsigned long parent_rate = 0;
	struct hlist_node *tmp;
	struct clk *child;

	old_rate = clk->rate;

	if (clk->parent)
		parent_rate = clk->parent->rate;

	if (clk->ops->recalc_rate)
		clk->rate = clk->ops->recalc_rate(clk->hw, parent_rate);
	else
		clk->rate = parent_rate;

	if (clk->notifier_count && msg)
		__clk_notify(clk, msg, old_rate, clk->rate);

	hlist_for_each_entry(child, tmp, &clk->children, child_node)
		__clk_recalc_rates(child, msg);
}

static int __clk_speculate_rates(struct clk *clk, unsigned long parent_rate)
{
	struct hlist_node *tmp;
	struct clk *child;
	unsigned long new_rate;
	int ret = NOTIFY_DONE;

	if (clk->ops->recalc_rate)
		new_rate = clk->ops->recalc_rate(clk->hw, parent_rate);
	else
		new_rate = parent_rate;

	
	if (clk->notifier_count)
		ret = __clk_notify(clk, PRE_RATE_CHANGE, clk->rate, new_rate);

	if (ret == NOTIFY_BAD)
		goto out;

	hlist_for_each_entry(child, tmp, &clk->children, child_node) {
		ret = __clk_speculate_rates(child, new_rate);
		if (ret == NOTIFY_BAD)
			break;
	}

out:
	return ret;
}

static void clk_calc_subtree(struct clk *clk, unsigned long new_rate)
{
	struct clk *child;
	struct hlist_node *tmp;

	clk->new_rate = new_rate;

	hlist_for_each_entry(child, tmp, &clk->children, child_node) {
		if (child->ops->recalc_rate)
			child->new_rate = child->ops->recalc_rate(child->hw, new_rate);
		else
			child->new_rate = new_rate;
		clk_calc_subtree(child, child->new_rate);
	}
}

static struct clk *clk_calc_new_rates(struct clk *clk, unsigned long rate)
{
	struct clk *top = clk;
	unsigned long best_parent_rate = clk->parent->rate;
	unsigned long new_rate;

	if (!clk->ops->round_rate && !(clk->flags & CLK_SET_RATE_PARENT)) {
		clk->new_rate = clk->rate;
		return NULL;
	}

	if (!clk->ops->round_rate && (clk->flags & CLK_SET_RATE_PARENT)) {
		top = clk_calc_new_rates(clk->parent, rate);
		new_rate = clk->new_rate = clk->parent->new_rate;

		goto out;
	}

	if (clk->flags & CLK_SET_RATE_PARENT)
		new_rate = clk->ops->round_rate(clk->hw, rate, &best_parent_rate);
	else
		new_rate = clk->ops->round_rate(clk->hw, rate, NULL);

	if (best_parent_rate != clk->parent->rate) {
		top = clk_calc_new_rates(clk->parent, best_parent_rate);

		goto out;
	}

out:
	clk_calc_subtree(clk, new_rate);

	return top;
}

static struct clk *clk_propagate_rate_change(struct clk *clk, unsigned long event)
{
	struct hlist_node *tmp;
	struct clk *child, *fail_clk = NULL;
	int ret = NOTIFY_DONE;

	if (clk->rate == clk->new_rate)
		return 0;

	if (clk->notifier_count) {
		ret = __clk_notify(clk, event, clk->rate, clk->new_rate);
		if (ret == NOTIFY_BAD)
			fail_clk = clk;
	}

	hlist_for_each_entry(child, tmp, &clk->children, child_node) {
		clk = clk_propagate_rate_change(child, event);
		if (clk)
			fail_clk = clk;
	}

	return fail_clk;
}

static void clk_change_rate(struct clk *clk)
{
	struct clk *child;
	unsigned long old_rate;
	struct hlist_node *tmp;

	old_rate = clk->rate;

	if (clk->ops->set_rate)
		clk->ops->set_rate(clk->hw, clk->new_rate);

	if (clk->ops->recalc_rate)
		clk->rate = clk->ops->recalc_rate(clk->hw,
				clk->parent->rate);
	else
		clk->rate = clk->parent->rate;

	if (clk->notifier_count && old_rate != clk->rate)
		__clk_notify(clk, POST_RATE_CHANGE, old_rate, clk->rate);

	hlist_for_each_entry(child, tmp, &clk->children, child_node)
		clk_change_rate(child);
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	struct clk *top, *fail_clk;
	int ret = 0;

	
	mutex_lock(&prepare_lock);

	
	if (rate == clk->rate)
		goto out;

	
	top = clk_calc_new_rates(clk, rate);
	if (!top) {
		ret = -EINVAL;
		goto out;
	}

	
	fail_clk = clk_propagate_rate_change(top, PRE_RATE_CHANGE);
	if (fail_clk) {
		pr_warn("%s: failed to set %s rate\n", __func__,
				fail_clk->name);
		clk_propagate_rate_change(top, ABORT_RATE_CHANGE);
		ret = -EBUSY;
		goto out;
	}

	
	clk_change_rate(top);

	mutex_unlock(&prepare_lock);

	return 0;
out:
	mutex_unlock(&prepare_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate);

struct clk *clk_get_parent(struct clk *clk)
{
	struct clk *parent;

	mutex_lock(&prepare_lock);
	parent = __clk_get_parent(clk);
	mutex_unlock(&prepare_lock);

	return parent;
}
EXPORT_SYMBOL_GPL(clk_get_parent);

static struct clk *__clk_init_parent(struct clk *clk)
{
	struct clk *ret = NULL;
	u8 index;

	

	if (!clk->num_parents)
		goto out;

	if (clk->num_parents == 1) {
		if (IS_ERR_OR_NULL(clk->parent))
			ret = clk->parent = __clk_lookup(clk->parent_names[0]);
		ret = clk->parent;
		goto out;
	}

	if (!clk->ops->get_parent) {
		WARN(!clk->ops->get_parent,
			"%s: multi-parent clocks must implement .get_parent\n",
			__func__);
		goto out;
	};


	index = clk->ops->get_parent(clk->hw);

	if (!clk->parents)
		clk->parents =
			kmalloc((sizeof(struct clk*) * clk->num_parents),
					GFP_KERNEL);

	if (!clk->parents)
		ret = __clk_lookup(clk->parent_names[index]);
	else if (!clk->parents[index])
		ret = clk->parents[index] =
			__clk_lookup(clk->parent_names[index]);
	else
		ret = clk->parents[index];

out:
	return ret;
}

void __clk_reparent(struct clk *clk, struct clk *new_parent)
{
#ifdef CONFIG_COMMON_CLK_DEBUG
	struct dentry *d;
	struct dentry *new_parent_d;
#endif

	if (!clk || !new_parent)
		return;

	hlist_del(&clk->child_node);

	if (new_parent)
		hlist_add_head(&clk->child_node, &new_parent->children);
	else
		hlist_add_head(&clk->child_node, &clk_orphan_list);

#ifdef CONFIG_COMMON_CLK_DEBUG
	if (!inited)
		goto out;

	if (new_parent)
		new_parent_d = new_parent->dentry;
	else
		new_parent_d = orphandir;

	d = debugfs_rename(clk->dentry->d_parent, clk->dentry,
			new_parent_d, clk->name);
	if (d)
		clk->dentry = d;
	else
		pr_debug("%s: failed to rename debugfs entry for %s\n",
				__func__, clk->name);
out:
#endif

	clk->parent = new_parent;

	__clk_recalc_rates(clk, POST_RATE_CHANGE);
}

static int __clk_set_parent(struct clk *clk, struct clk *parent)
{
	struct clk *old_parent;
	unsigned long flags;
	int ret = -EINVAL;
	u8 i;

	old_parent = clk->parent;

	
	for (i = 0; i < clk->num_parents; i++)
		if (clk->parents[i] == parent)
			break;

	if (i == clk->num_parents)
		for (i = 0; i < clk->num_parents; i++)
			if (!strcmp(clk->parent_names[i], parent->name)) {
				clk->parents[i] = __clk_lookup(parent->name);
				break;
			}

	if (i == clk->num_parents) {
		pr_debug("%s: clock %s is not a possible parent of clock %s\n",
				__func__, parent->name, clk->name);
		goto out;
	}

	
	if (clk->prepare_count)
		__clk_prepare(parent);

	
	spin_lock_irqsave(&enable_lock, flags);
	if (clk->enable_count)
		__clk_enable(parent);
	spin_unlock_irqrestore(&enable_lock, flags);

	
	ret = clk->ops->set_parent(clk->hw, i);

	
	spin_lock_irqsave(&enable_lock, flags);
	if (clk->enable_count)
		__clk_disable(old_parent);
	spin_unlock_irqrestore(&enable_lock, flags);

	if (clk->prepare_count)
		__clk_unprepare(old_parent);

out:
	return ret;
}

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = 0;

	if (!clk || !clk->ops)
		return -EINVAL;

	if (!clk->ops->set_parent)
		return -ENOSYS;

	
	mutex_lock(&prepare_lock);

	if (clk->parent == parent)
		goto out;

	
	if (clk->notifier_count)
		ret = __clk_speculate_rates(clk, parent->rate);

	
	if (ret == NOTIFY_STOP)
		goto out;

	
	if ((clk->flags & CLK_SET_PARENT_GATE) && clk->prepare_count)
		ret = -EBUSY;
	else
		ret = __clk_set_parent(clk, parent);

	
	if (ret) {
		__clk_recalc_rates(clk, ABORT_RATE_CHANGE);
		goto out;
	}

	
	__clk_reparent(clk, parent);

out:
	mutex_unlock(&prepare_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_parent);

void __clk_init(struct device *dev, struct clk *clk)
{
	int i;
	struct clk *orphan;
	struct hlist_node *tmp, *tmp2;

	if (!clk)
		return;

	mutex_lock(&prepare_lock);

	
	if (__clk_lookup(clk->name))
		goto out;

	
	for (i = 0; i < clk->num_parents; i++)
		WARN(!clk->parent_names[i],
				"%s: invalid NULL in %s's .parent_names\n",
				__func__, clk->name);

	if (clk->num_parents && !clk->parents) {
		clk->parents = kmalloc((sizeof(struct clk*) * clk->num_parents),
				GFP_KERNEL);
		if (clk->parents)
			for (i = 0; i < clk->num_parents; i++)
				clk->parents[i] =
					__clk_lookup(clk->parent_names[i]);
	}

	clk->parent = __clk_init_parent(clk);

	if (clk->parent)
		hlist_add_head(&clk->child_node,
				&clk->parent->children);
	else if (clk->flags & CLK_IS_ROOT)
		hlist_add_head(&clk->child_node, &clk_root_list);
	else
		hlist_add_head(&clk->child_node, &clk_orphan_list);

	if (clk->ops->recalc_rate)
		clk->rate = clk->ops->recalc_rate(clk->hw,
				__clk_get_rate(clk->parent));
	else if (clk->parent)
		clk->rate = clk->parent->rate;
	else
		clk->rate = 0;

	hlist_for_each_entry_safe(orphan, tmp, tmp2, &clk_orphan_list, child_node)
		for (i = 0; i < orphan->num_parents; i++)
			if (!strcmp(clk->name, orphan->parent_names[i])) {
				__clk_reparent(orphan, clk);
				break;
			}

	if (clk->ops->init)
		clk->ops->init(clk->hw);

	clk_debug_register(clk);

out:
	mutex_unlock(&prepare_lock);

	return;
}

struct clk *clk_register(struct device *dev, const char *name,
		const struct clk_ops *ops, struct clk_hw *hw,
		char **parent_names, u8 num_parents, unsigned long flags)
{
	struct clk *clk;

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return NULL;

	clk->name = name;
	clk->ops = ops;
	clk->hw = hw;
	clk->flags = flags;
	clk->parent_names = parent_names;
	clk->num_parents = num_parents;
	hw->clk = clk;

	__clk_init(dev, clk);

	return clk;
}
EXPORT_SYMBOL_GPL(clk_register);


int clk_notifier_register(struct clk *clk, struct notifier_block *nb)
{
	struct clk_notifier *cn;
	int ret = -ENOMEM;

	if (!clk || !nb)
		return -EINVAL;

	mutex_lock(&prepare_lock);

	
	list_for_each_entry(cn, &clk_notifier_list, node)
		if (cn->clk == clk)
			break;

	
	if (cn->clk != clk) {
		cn = kzalloc(sizeof(struct clk_notifier), GFP_KERNEL);
		if (!cn)
			goto out;

		cn->clk = clk;
		srcu_init_notifier_head(&cn->notifier_head);

		list_add(&cn->node, &clk_notifier_list);
	}

	ret = srcu_notifier_chain_register(&cn->notifier_head, nb);

	clk->notifier_count++;

out:
	mutex_unlock(&prepare_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_notifier_register);

int clk_notifier_unregister(struct clk *clk, struct notifier_block *nb)
{
	struct clk_notifier *cn = NULL;
	int ret = -EINVAL;

	if (!clk || !nb)
		return -EINVAL;

	mutex_lock(&prepare_lock);

	list_for_each_entry(cn, &clk_notifier_list, node)
		if (cn->clk == clk)
			break;

	if (cn->clk == clk) {
		ret = srcu_notifier_chain_unregister(&cn->notifier_head, nb);

		clk->notifier_count--;

		
		if (!cn->notifier_head.head) {
			srcu_cleanup_notifier_head(&cn->notifier_head);
			kfree(cn);
		}

	} else {
		ret = -ENOENT;
	}

	mutex_unlock(&prepare_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_notifier_unregister);
