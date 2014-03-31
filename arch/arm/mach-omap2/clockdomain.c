/*
 * OMAP2/3/4 clockdomain framework functions
 *
 * Copyright (C) 2008-2011 Texas Instruments, Inc.
 * Copyright (C) 2008-2011 Nokia Corporation
 *
 * Written by Paul Walmsley and Jouni Högander
 * Added OMAP4 specific support by Abhijit Pagare <abhijitpagare@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/limits.h>
#include <linux/err.h>

#include <linux/io.h>

#include <linux/bitops.h>

#include <plat/clock.h>
#include "clockdomain.h"

static LIST_HEAD(clkdm_list);

static struct clkdm_autodep *autodeps;

static struct clkdm_ops *arch_clkdm;


static struct clockdomain *_clkdm_lookup(const char *name)
{
	struct clockdomain *clkdm, *temp_clkdm;

	if (!name)
		return NULL;

	clkdm = NULL;

	list_for_each_entry(temp_clkdm, &clkdm_list, node) {
		if (!strcmp(name, temp_clkdm->name)) {
			clkdm = temp_clkdm;
			break;
		}
	}

	return clkdm;
}

static int _clkdm_register(struct clockdomain *clkdm)
{
	struct powerdomain *pwrdm;

	if (!clkdm || !clkdm->name)
		return -EINVAL;

	pwrdm = pwrdm_lookup(clkdm->pwrdm.name);
	if (!pwrdm) {
		pr_err("clockdomain: %s: powerdomain %s does not exist\n",
			clkdm->name, clkdm->pwrdm.name);
		return -EINVAL;
	}
	clkdm->pwrdm.ptr = pwrdm;

	
	if (_clkdm_lookup(clkdm->name))
		return -EEXIST;

	list_add(&clkdm->node, &clkdm_list);

	pwrdm_add_clkdm(pwrdm, clkdm);

	spin_lock_init(&clkdm->lock);

	pr_debug("clockdomain: registered %s\n", clkdm->name);

	return 0;
}

static struct clkdm_dep *_clkdm_deps_lookup(struct clockdomain *clkdm,
					    struct clkdm_dep *deps)
{
	struct clkdm_dep *cd;

	if (!clkdm || !deps)
		return ERR_PTR(-EINVAL);

	for (cd = deps; cd->clkdm_name; cd++) {
		if (!cd->clkdm && cd->clkdm_name)
			cd->clkdm = _clkdm_lookup(cd->clkdm_name);

		if (cd->clkdm == clkdm)
			break;
	}

	if (!cd->clkdm_name)
		return ERR_PTR(-ENOENT);

	return cd;
}

static void _autodep_lookup(struct clkdm_autodep *autodep)
{
	struct clockdomain *clkdm;

	if (!autodep)
		return;

	clkdm = clkdm_lookup(autodep->clkdm.name);
	if (!clkdm) {
		pr_err("clockdomain: autodeps: clockdomain %s does not exist\n",
			 autodep->clkdm.name);
		clkdm = ERR_PTR(-ENOENT);
	}
	autodep->clkdm.ptr = clkdm;
}

void _clkdm_add_autodeps(struct clockdomain *clkdm)
{
	struct clkdm_autodep *autodep;

	if (!autodeps || clkdm->flags & CLKDM_NO_AUTODEPS)
		return;

	for (autodep = autodeps; autodep->clkdm.ptr; autodep++) {
		if (IS_ERR(autodep->clkdm.ptr))
			continue;

		pr_debug("clockdomain: adding %s sleepdep/wkdep for "
			 "clkdm %s\n", autodep->clkdm.ptr->name,
			 clkdm->name);

		clkdm_add_sleepdep(clkdm, autodep->clkdm.ptr);
		clkdm_add_wkdep(clkdm, autodep->clkdm.ptr);
	}
}

void _clkdm_del_autodeps(struct clockdomain *clkdm)
{
	struct clkdm_autodep *autodep;

	if (!autodeps || clkdm->flags & CLKDM_NO_AUTODEPS)
		return;

	for (autodep = autodeps; autodep->clkdm.ptr; autodep++) {
		if (IS_ERR(autodep->clkdm.ptr))
			continue;

		pr_debug("clockdomain: removing %s sleepdep/wkdep for "
			 "clkdm %s\n", autodep->clkdm.ptr->name,
			 clkdm->name);

		clkdm_del_sleepdep(clkdm, autodep->clkdm.ptr);
		clkdm_del_wkdep(clkdm, autodep->clkdm.ptr);
	}
}

static void _resolve_clkdm_deps(struct clockdomain *clkdm,
				struct clkdm_dep *clkdm_deps)
{
	struct clkdm_dep *cd;

	for (cd = clkdm_deps; cd && cd->clkdm_name; cd++) {
		if (cd->clkdm)
			continue;
		cd->clkdm = _clkdm_lookup(cd->clkdm_name);

		WARN(!cd->clkdm, "clockdomain: %s: could not find clkdm %s while resolving dependencies - should never happen",
		     clkdm->name, cd->clkdm_name);
	}
}


int clkdm_register_platform_funcs(struct clkdm_ops *co)
{
	if (!co)
		return -EINVAL;

	if (arch_clkdm)
		return -EEXIST;

	arch_clkdm = co;

	return 0;
};

int clkdm_register_clkdms(struct clockdomain **cs)
{
	struct clockdomain **c = NULL;

	if (!arch_clkdm)
		return -EACCES;

	if (!cs)
		return -EINVAL;

	for (c = cs; *c; c++)
		_clkdm_register(*c);

	return 0;
}

int clkdm_register_autodeps(struct clkdm_autodep *ia)
{
	struct clkdm_autodep *a = NULL;

	if (list_empty(&clkdm_list))
		return -EACCES;

	if (!ia)
		return -EINVAL;

	if (autodeps)
		return -EEXIST;

	autodeps = ia;
	for (a = autodeps; a->clkdm.ptr; a++)
		_autodep_lookup(a);

	return 0;
}

int clkdm_complete_init(void)
{
	struct clockdomain *clkdm;

	if (list_empty(&clkdm_list))
		return -EACCES;

	list_for_each_entry(clkdm, &clkdm_list, node) {
		if (clkdm->flags & CLKDM_CAN_FORCE_WAKEUP)
			clkdm_wakeup(clkdm);
		else if (clkdm->flags & CLKDM_CAN_DISABLE_AUTO)
			clkdm_deny_idle(clkdm);

		_resolve_clkdm_deps(clkdm, clkdm->wkdep_srcs);
		clkdm_clear_all_wkdeps(clkdm);

		_resolve_clkdm_deps(clkdm, clkdm->sleepdep_srcs);
		clkdm_clear_all_sleepdeps(clkdm);
	}

	return 0;
}

struct clockdomain *clkdm_lookup(const char *name)
{
	struct clockdomain *clkdm, *temp_clkdm;

	if (!name)
		return NULL;

	clkdm = NULL;

	list_for_each_entry(temp_clkdm, &clkdm_list, node) {
		if (!strcmp(name, temp_clkdm->name)) {
			clkdm = temp_clkdm;
			break;
		}
	}

	return clkdm;
}

int clkdm_for_each(int (*fn)(struct clockdomain *clkdm, void *user),
			void *user)
{
	struct clockdomain *clkdm;
	int ret = 0;

	if (!fn)
		return -EINVAL;

	list_for_each_entry(clkdm, &clkdm_list, node) {
		ret = (*fn)(clkdm, user);
		if (ret)
			break;
	}

	return ret;
}


struct powerdomain *clkdm_get_pwrdm(struct clockdomain *clkdm)
{
	if (!clkdm)
		return NULL;

	return clkdm->pwrdm.ptr;
}



int clkdm_add_wkdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2)
{
	struct clkdm_dep *cd;
	int ret = 0;

	if (!clkdm1 || !clkdm2)
		return -EINVAL;

	cd = _clkdm_deps_lookup(clkdm2, clkdm1->wkdep_srcs);
	if (IS_ERR(cd))
		ret = PTR_ERR(cd);

	if (!arch_clkdm || !arch_clkdm->clkdm_add_wkdep)
		ret = -EINVAL;

	if (ret) {
		pr_debug("clockdomain: hardware cannot set/clear wake up of "
			 "%s when %s wakes up\n", clkdm1->name, clkdm2->name);
		return ret;
	}

	if (atomic_inc_return(&cd->wkdep_usecount) == 1) {
		pr_debug("clockdomain: hardware will wake up %s when %s wakes "
			 "up\n", clkdm1->name, clkdm2->name);

		ret = arch_clkdm->clkdm_add_wkdep(clkdm1, clkdm2);
	}

	return ret;
}

int clkdm_del_wkdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2)
{
	struct clkdm_dep *cd;
	int ret = 0;

	if (!clkdm1 || !clkdm2)
		return -EINVAL;

	cd = _clkdm_deps_lookup(clkdm2, clkdm1->wkdep_srcs);
	if (IS_ERR(cd))
		ret = PTR_ERR(cd);

	if (!arch_clkdm || !arch_clkdm->clkdm_del_wkdep)
		ret = -EINVAL;

	if (ret) {
		pr_debug("clockdomain: hardware cannot set/clear wake up of "
			 "%s when %s wakes up\n", clkdm1->name, clkdm2->name);
		return ret;
	}

	if (atomic_dec_return(&cd->wkdep_usecount) == 0) {
		pr_debug("clockdomain: hardware will no longer wake up %s "
			 "after %s wakes up\n", clkdm1->name, clkdm2->name);

		ret = arch_clkdm->clkdm_del_wkdep(clkdm1, clkdm2);
	}

	return ret;
}

int clkdm_read_wkdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2)
{
	struct clkdm_dep *cd;
	int ret = 0;

	if (!clkdm1 || !clkdm2)
		return -EINVAL;

	cd = _clkdm_deps_lookup(clkdm2, clkdm1->wkdep_srcs);
	if (IS_ERR(cd))
		ret = PTR_ERR(cd);

	if (!arch_clkdm || !arch_clkdm->clkdm_read_wkdep)
		ret = -EINVAL;

	if (ret) {
		pr_debug("clockdomain: hardware cannot set/clear wake up of "
			 "%s when %s wakes up\n", clkdm1->name, clkdm2->name);
		return ret;
	}

	
	return arch_clkdm->clkdm_read_wkdep(clkdm1, clkdm2);
}

int clkdm_clear_all_wkdeps(struct clockdomain *clkdm)
{
	if (!clkdm)
		return -EINVAL;

	if (!arch_clkdm || !arch_clkdm->clkdm_clear_all_wkdeps)
		return -EINVAL;

	return arch_clkdm->clkdm_clear_all_wkdeps(clkdm);
}

int clkdm_add_sleepdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2)
{
	struct clkdm_dep *cd;
	int ret = 0;

	if (!clkdm1 || !clkdm2)
		return -EINVAL;

	cd = _clkdm_deps_lookup(clkdm2, clkdm1->sleepdep_srcs);
	if (IS_ERR(cd))
		ret = PTR_ERR(cd);

	if (!arch_clkdm || !arch_clkdm->clkdm_add_sleepdep)
		ret = -EINVAL;

	if (ret) {
		pr_debug("clockdomain: hardware cannot set/clear sleep "
			 "dependency affecting %s from %s\n", clkdm1->name,
			 clkdm2->name);
		return ret;
	}

	if (atomic_inc_return(&cd->sleepdep_usecount) == 1) {
		pr_debug("clockdomain: will prevent %s from sleeping if %s "
			 "is active\n", clkdm1->name, clkdm2->name);

		ret = arch_clkdm->clkdm_add_sleepdep(clkdm1, clkdm2);
	}

	return ret;
}

int clkdm_del_sleepdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2)
{
	struct clkdm_dep *cd;
	int ret = 0;

	if (!clkdm1 || !clkdm2)
		return -EINVAL;

	cd = _clkdm_deps_lookup(clkdm2, clkdm1->sleepdep_srcs);
	if (IS_ERR(cd))
		ret = PTR_ERR(cd);

	if (!arch_clkdm || !arch_clkdm->clkdm_del_sleepdep)
		ret = -EINVAL;

	if (ret) {
		pr_debug("clockdomain: hardware cannot set/clear sleep "
			 "dependency affecting %s from %s\n", clkdm1->name,
			 clkdm2->name);
		return ret;
	}

	if (atomic_dec_return(&cd->sleepdep_usecount) == 0) {
		pr_debug("clockdomain: will no longer prevent %s from "
			 "sleeping if %s is active\n", clkdm1->name,
			 clkdm2->name);

		ret = arch_clkdm->clkdm_del_sleepdep(clkdm1, clkdm2);
	}

	return ret;
}

int clkdm_read_sleepdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2)
{
	struct clkdm_dep *cd;
	int ret = 0;

	if (!clkdm1 || !clkdm2)
		return -EINVAL;

	cd = _clkdm_deps_lookup(clkdm2, clkdm1->sleepdep_srcs);
	if (IS_ERR(cd))
		ret = PTR_ERR(cd);

	if (!arch_clkdm || !arch_clkdm->clkdm_read_sleepdep)
		ret = -EINVAL;

	if (ret) {
		pr_debug("clockdomain: hardware cannot set/clear sleep "
			 "dependency affecting %s from %s\n", clkdm1->name,
			 clkdm2->name);
		return ret;
	}

	
	return arch_clkdm->clkdm_read_sleepdep(clkdm1, clkdm2);
}

int clkdm_clear_all_sleepdeps(struct clockdomain *clkdm)
{
	if (!clkdm)
		return -EINVAL;

	if (!arch_clkdm || !arch_clkdm->clkdm_clear_all_sleepdeps)
		return -EINVAL;

	return arch_clkdm->clkdm_clear_all_sleepdeps(clkdm);
}

int clkdm_sleep(struct clockdomain *clkdm)
{
	int ret;
	unsigned long flags;

	if (!clkdm)
		return -EINVAL;

	if (!(clkdm->flags & CLKDM_CAN_FORCE_SLEEP)) {
		pr_debug("clockdomain: %s does not support forcing "
			 "sleep via software\n", clkdm->name);
		return -EINVAL;
	}

	if (!arch_clkdm || !arch_clkdm->clkdm_sleep)
		return -EINVAL;

	pr_debug("clockdomain: forcing sleep on %s\n", clkdm->name);

	spin_lock_irqsave(&clkdm->lock, flags);
	clkdm->_flags &= ~_CLKDM_FLAG_HWSUP_ENABLED;
	ret = arch_clkdm->clkdm_sleep(clkdm);
	spin_unlock_irqrestore(&clkdm->lock, flags);
	return ret;
}

int clkdm_wakeup(struct clockdomain *clkdm)
{
	int ret;
	unsigned long flags;

	if (!clkdm)
		return -EINVAL;

	if (!(clkdm->flags & CLKDM_CAN_FORCE_WAKEUP)) {
		pr_debug("clockdomain: %s does not support forcing "
			 "wakeup via software\n", clkdm->name);
		return -EINVAL;
	}

	if (!arch_clkdm || !arch_clkdm->clkdm_wakeup)
		return -EINVAL;

	pr_debug("clockdomain: forcing wakeup on %s\n", clkdm->name);

	spin_lock_irqsave(&clkdm->lock, flags);
	clkdm->_flags &= ~_CLKDM_FLAG_HWSUP_ENABLED;
	ret = arch_clkdm->clkdm_wakeup(clkdm);
	ret |= pwrdm_state_switch(clkdm->pwrdm.ptr);
	spin_unlock_irqrestore(&clkdm->lock, flags);
	return ret;
}

void clkdm_allow_idle(struct clockdomain *clkdm)
{
	unsigned long flags;

	if (!clkdm)
		return;

	if (!(clkdm->flags & CLKDM_CAN_ENABLE_AUTO)) {
		pr_debug("clock: automatic idle transitions cannot be enabled "
			 "on clockdomain %s\n", clkdm->name);
		return;
	}

	if (!arch_clkdm || !arch_clkdm->clkdm_allow_idle)
		return;

	pr_debug("clockdomain: enabling automatic idle transitions for %s\n",
		 clkdm->name);

	spin_lock_irqsave(&clkdm->lock, flags);
	clkdm->_flags |= _CLKDM_FLAG_HWSUP_ENABLED;
	arch_clkdm->clkdm_allow_idle(clkdm);
	pwrdm_clkdm_state_switch(clkdm);
	spin_unlock_irqrestore(&clkdm->lock, flags);
}

void clkdm_deny_idle(struct clockdomain *clkdm)
{
	unsigned long flags;

	if (!clkdm)
		return;

	if (!(clkdm->flags & CLKDM_CAN_DISABLE_AUTO)) {
		pr_debug("clockdomain: automatic idle transitions cannot be "
			 "disabled on %s\n", clkdm->name);
		return;
	}

	if (!arch_clkdm || !arch_clkdm->clkdm_deny_idle)
		return;

	pr_debug("clockdomain: disabling automatic idle transitions for %s\n",
		 clkdm->name);

	spin_lock_irqsave(&clkdm->lock, flags);
	clkdm->_flags &= ~_CLKDM_FLAG_HWSUP_ENABLED;
	arch_clkdm->clkdm_deny_idle(clkdm);
	pwrdm_state_switch(clkdm->pwrdm.ptr);
	spin_unlock_irqrestore(&clkdm->lock, flags);
}

bool clkdm_in_hwsup(struct clockdomain *clkdm)
{
	bool ret;
	unsigned long flags;

	if (!clkdm)
		return false;

	spin_lock_irqsave(&clkdm->lock, flags);
	ret = (clkdm->_flags & _CLKDM_FLAG_HWSUP_ENABLED) ? true : false;
	spin_unlock_irqrestore(&clkdm->lock, flags);

	return ret;
}


static int _clkdm_clk_hwmod_enable(struct clockdomain *clkdm)
{
	unsigned long flags;

	if (!clkdm || !arch_clkdm || !arch_clkdm->clkdm_clk_enable)
		return -EINVAL;

	if ((atomic_inc_return(&clkdm->usecount) > 1) && autodeps)
		return 0;

	spin_lock_irqsave(&clkdm->lock, flags);
	arch_clkdm->clkdm_clk_enable(clkdm);
	pwrdm_wait_transition(clkdm->pwrdm.ptr);
	pwrdm_clkdm_state_switch(clkdm);
	spin_unlock_irqrestore(&clkdm->lock, flags);

	pr_debug("clockdomain: clkdm %s: enabled\n", clkdm->name);

	return 0;
}

static int _clkdm_clk_hwmod_disable(struct clockdomain *clkdm)
{
	unsigned long flags;

	if (!clkdm || !arch_clkdm || !arch_clkdm->clkdm_clk_disable)
		return -EINVAL;

	if (atomic_read(&clkdm->usecount) == 0) {
		WARN_ON(1); 
		return -ERANGE;
	}

	if (atomic_dec_return(&clkdm->usecount) > 0)
		return 0;

	spin_lock_irqsave(&clkdm->lock, flags);
	arch_clkdm->clkdm_clk_disable(clkdm);
	pwrdm_clkdm_state_switch(clkdm);
	spin_unlock_irqrestore(&clkdm->lock, flags);

	pr_debug("clockdomain: clkdm %s: disabled\n", clkdm->name);

	return 0;
}

/**
 * clkdm_clk_enable - add an enabled downstream clock to this clkdm
 * @clkdm: struct clockdomain *
 * @clk: struct clk * of the enabled downstream clock
 *
 * Increment the usecount of the clockdomain @clkdm and ensure that it
 * is awake before @clk is enabled.  Intended to be called by
 * clk_enable() code.  If the clockdomain is in software-supervised
 * idle mode, force the clockdomain to wake.  If the clockdomain is in
 * hardware-supervised idle mode, add clkdm-pwrdm autodependencies, to
 * ensure that devices in the clockdomain can be read from/written to
 * by on-chip processors.  Returns -EINVAL if passed null pointers;
 * returns 0 upon success or if the clockdomain is in hwsup idle mode.
 */
int clkdm_clk_enable(struct clockdomain *clkdm, struct clk *clk)
{

	if (!clk)
		return -EINVAL;

	return _clkdm_clk_hwmod_enable(clkdm);
}

int clkdm_clk_disable(struct clockdomain *clkdm, struct clk *clk)
{

	if (!clk)
		return -EINVAL;

	return _clkdm_clk_hwmod_disable(clkdm);
}

/**
 * clkdm_hwmod_enable - add an enabled downstream hwmod to this clkdm
 * @clkdm: struct clockdomain *
 * @oh: struct omap_hwmod * of the enabled downstream hwmod
 *
 * Increment the usecount of the clockdomain @clkdm and ensure that it
 * is awake before @oh is enabled. Intended to be called by
 * module_enable() code.
 * If the clockdomain is in software-supervised idle mode, force the
 * clockdomain to wake.  If the clockdomain is in hardware-supervised idle
 * mode, add clkdm-pwrdm autodependencies, to ensure that devices in the
 * clockdomain can be read from/written to by on-chip processors.
 * Returns -EINVAL if passed null pointers;
 * returns 0 upon success or if the clockdomain is in hwsup idle mode.
 */
int clkdm_hwmod_enable(struct clockdomain *clkdm, struct omap_hwmod *oh)
{
	
	if (cpu_is_omap24xx() || cpu_is_omap34xx())
		return 0;


	if (!oh)
		return -EINVAL;

	return _clkdm_clk_hwmod_enable(clkdm);
}

int clkdm_hwmod_disable(struct clockdomain *clkdm, struct omap_hwmod *oh)
{
	
	if (cpu_is_omap24xx() || cpu_is_omap34xx())
		return 0;


	if (!oh)
		return -EINVAL;

	return _clkdm_clk_hwmod_disable(clkdm);
}

