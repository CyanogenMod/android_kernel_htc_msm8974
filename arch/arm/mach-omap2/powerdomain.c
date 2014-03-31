/*
 * OMAP powerdomain control
 *
 * Copyright (C) 2007-2008, 2011 Texas Instruments, Inc.
 * Copyright (C) 2007-2011 Nokia Corporation
 *
 * Written by Paul Walmsley
 * Added OMAP4 specific support by Abhijit Pagare <abhijitpagare@ti.com>
 * State counting code by Tero Kristo <tero.kristo@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <trace/events/power.h>

#include "cm2xxx_3xxx.h"
#include "prcm44xx.h"
#include "cm44xx.h"
#include "prm2xxx_3xxx.h"
#include "prm44xx.h"

#include <asm/cpu.h>
#include <plat/cpu.h>
#include "powerdomain.h"
#include "clockdomain.h"
#include <plat/prcm.h>

#include "pm.h"

#define PWRDM_TRACE_STATES_FLAG	(1<<31)

enum {
	PWRDM_STATE_NOW = 0,
	PWRDM_STATE_PREV,
};


static LIST_HEAD(pwrdm_list);

static struct pwrdm_ops *arch_pwrdm;


static struct powerdomain *_pwrdm_lookup(const char *name)
{
	struct powerdomain *pwrdm, *temp_pwrdm;

	pwrdm = NULL;

	list_for_each_entry(temp_pwrdm, &pwrdm_list, node) {
		if (!strcmp(name, temp_pwrdm->name)) {
			pwrdm = temp_pwrdm;
			break;
		}
	}

	return pwrdm;
}

static int _pwrdm_register(struct powerdomain *pwrdm)
{
	int i;
	struct voltagedomain *voltdm;

	if (!pwrdm || !pwrdm->name)
		return -EINVAL;

	if (cpu_is_omap44xx() &&
	    pwrdm->prcm_partition == OMAP4430_INVALID_PRCM_PARTITION) {
		pr_err("powerdomain: %s: missing OMAP4 PRCM partition ID\n",
		       pwrdm->name);
		return -EINVAL;
	}

	if (_pwrdm_lookup(pwrdm->name))
		return -EEXIST;

	voltdm = voltdm_lookup(pwrdm->voltdm.name);
	if (!voltdm) {
		pr_err("powerdomain: %s: voltagedomain %s does not exist\n",
		       pwrdm->name, pwrdm->voltdm.name);
		return -EINVAL;
	}
	pwrdm->voltdm.ptr = voltdm;
	INIT_LIST_HEAD(&pwrdm->voltdm_node);
	voltdm_add_pwrdm(voltdm, pwrdm);

	list_add(&pwrdm->node, &pwrdm_list);

	
	for (i = 0; i < PWRDM_MAX_PWRSTS; i++)
		pwrdm->state_counter[i] = 0;

	pwrdm->ret_logic_off_counter = 0;
	for (i = 0; i < pwrdm->banks; i++)
		pwrdm->ret_mem_off_counter[i] = 0;

	pwrdm_wait_transition(pwrdm);
	pwrdm->state = pwrdm_read_pwrst(pwrdm);
	pwrdm->state_counter[pwrdm->state] = 1;

	pr_debug("powerdomain: registered %s\n", pwrdm->name);

	return 0;
}

static void _update_logic_membank_counters(struct powerdomain *pwrdm)
{
	int i;
	u8 prev_logic_pwrst, prev_mem_pwrst;

	prev_logic_pwrst = pwrdm_read_prev_logic_pwrst(pwrdm);
	if ((pwrdm->pwrsts_logic_ret == PWRSTS_OFF_RET) &&
	    (prev_logic_pwrst == PWRDM_POWER_OFF))
		pwrdm->ret_logic_off_counter++;

	for (i = 0; i < pwrdm->banks; i++) {
		prev_mem_pwrst = pwrdm_read_prev_mem_pwrst(pwrdm, i);

		if ((pwrdm->pwrsts_mem_ret[i] == PWRSTS_OFF_RET) &&
		    (prev_mem_pwrst == PWRDM_POWER_OFF))
			pwrdm->ret_mem_off_counter[i]++;
	}
}

static int _pwrdm_state_switch(struct powerdomain *pwrdm, int flag)
{

	int prev, state, trace_state = 0;

	if (pwrdm == NULL)
		return -EINVAL;

	state = pwrdm_read_pwrst(pwrdm);

	switch (flag) {
	case PWRDM_STATE_NOW:
		prev = pwrdm->state;
		break;
	case PWRDM_STATE_PREV:
		prev = pwrdm_read_prev_pwrst(pwrdm);
		if (pwrdm->state != prev)
			pwrdm->state_counter[prev]++;
		if (prev == PWRDM_POWER_RET)
			_update_logic_membank_counters(pwrdm);
		if (state != prev) {
			trace_state = (PWRDM_TRACE_STATES_FLAG |
				       ((state & OMAP_POWERSTATE_MASK) << 8) |
				       ((prev & OMAP_POWERSTATE_MASK) << 0));
			trace_power_domain_target(pwrdm->name, trace_state,
						  smp_processor_id());
		}
		break;
	default:
		return -EINVAL;
	}

	if (state != prev)
		pwrdm->state_counter[state]++;

	pm_dbg_update_time(pwrdm, prev);

	pwrdm->state = state;

	return 0;
}

static int _pwrdm_pre_transition_cb(struct powerdomain *pwrdm, void *unused)
{
	pwrdm_clear_all_prev_pwrst(pwrdm);
	_pwrdm_state_switch(pwrdm, PWRDM_STATE_NOW);
	return 0;
}

static int _pwrdm_post_transition_cb(struct powerdomain *pwrdm, void *unused)
{
	_pwrdm_state_switch(pwrdm, PWRDM_STATE_PREV);
	return 0;
}


int pwrdm_register_platform_funcs(struct pwrdm_ops *po)
{
	if (!po)
		return -EINVAL;

	if (arch_pwrdm)
		return -EEXIST;

	arch_pwrdm = po;

	return 0;
}

int pwrdm_register_pwrdms(struct powerdomain **ps)
{
	struct powerdomain **p = NULL;

	if (!arch_pwrdm)
		return -EEXIST;

	if (!ps)
		return -EINVAL;

	for (p = ps; *p; p++)
		_pwrdm_register(*p);

	return 0;
}

int pwrdm_complete_init(void)
{
	struct powerdomain *temp_p;

	if (list_empty(&pwrdm_list))
		return -EACCES;

	list_for_each_entry(temp_p, &pwrdm_list, node)
		pwrdm_set_next_pwrst(temp_p, PWRDM_POWER_ON);

	return 0;
}

struct powerdomain *pwrdm_lookup(const char *name)
{
	struct powerdomain *pwrdm;

	if (!name)
		return NULL;

	pwrdm = _pwrdm_lookup(name);

	return pwrdm;
}

int pwrdm_for_each(int (*fn)(struct powerdomain *pwrdm, void *user),
		   void *user)
{
	struct powerdomain *temp_pwrdm;
	int ret = 0;

	if (!fn)
		return -EINVAL;

	list_for_each_entry(temp_pwrdm, &pwrdm_list, node) {
		ret = (*fn)(temp_pwrdm, user);
		if (ret)
			break;
	}

	return ret;
}

int pwrdm_add_clkdm(struct powerdomain *pwrdm, struct clockdomain *clkdm)
{
	int i;
	int ret = -EINVAL;

	if (!pwrdm || !clkdm)
		return -EINVAL;

	pr_debug("powerdomain: associating clockdomain %s with powerdomain "
		 "%s\n", clkdm->name, pwrdm->name);

	for (i = 0; i < PWRDM_MAX_CLKDMS; i++) {
		if (!pwrdm->pwrdm_clkdms[i])
			break;
#ifdef DEBUG
		if (pwrdm->pwrdm_clkdms[i] == clkdm) {
			ret = -EINVAL;
			goto pac_exit;
		}
#endif
	}

	if (i == PWRDM_MAX_CLKDMS) {
		pr_debug("powerdomain: increase PWRDM_MAX_CLKDMS for "
			 "pwrdm %s clkdm %s\n", pwrdm->name, clkdm->name);
		WARN_ON(1);
		ret = -ENOMEM;
		goto pac_exit;
	}

	pwrdm->pwrdm_clkdms[i] = clkdm;

	ret = 0;

pac_exit:
	return ret;
}

int pwrdm_del_clkdm(struct powerdomain *pwrdm, struct clockdomain *clkdm)
{
	int ret = -EINVAL;
	int i;

	if (!pwrdm || !clkdm)
		return -EINVAL;

	pr_debug("powerdomain: dissociating clockdomain %s from powerdomain "
		 "%s\n", clkdm->name, pwrdm->name);

	for (i = 0; i < PWRDM_MAX_CLKDMS; i++)
		if (pwrdm->pwrdm_clkdms[i] == clkdm)
			break;

	if (i == PWRDM_MAX_CLKDMS) {
		pr_debug("powerdomain: clkdm %s not associated with pwrdm "
			 "%s ?!\n", clkdm->name, pwrdm->name);
		ret = -ENOENT;
		goto pdc_exit;
	}

	pwrdm->pwrdm_clkdms[i] = NULL;

	ret = 0;

pdc_exit:
	return ret;
}

int pwrdm_for_each_clkdm(struct powerdomain *pwrdm,
			 int (*fn)(struct powerdomain *pwrdm,
				   struct clockdomain *clkdm))
{
	int ret = 0;
	int i;

	if (!fn)
		return -EINVAL;

	for (i = 0; i < PWRDM_MAX_CLKDMS && !ret; i++)
		ret = (*fn)(pwrdm, pwrdm->pwrdm_clkdms[i]);

	return ret;
}

struct voltagedomain *pwrdm_get_voltdm(struct powerdomain *pwrdm)
{
	return pwrdm->voltdm.ptr;
}

int pwrdm_get_mem_bank_count(struct powerdomain *pwrdm)
{
	if (!pwrdm)
		return -EINVAL;

	return pwrdm->banks;
}

int pwrdm_set_next_pwrst(struct powerdomain *pwrdm, u8 pwrst)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (!(pwrdm->pwrsts & (1 << pwrst)))
		return -EINVAL;

	pr_debug("powerdomain: setting next powerstate for %s to %0x\n",
		 pwrdm->name, pwrst);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_next_pwrst) {
		
		trace_power_domain_target(pwrdm->name, pwrst,
					  smp_processor_id());
		
		ret = arch_pwrdm->pwrdm_set_next_pwrst(pwrdm, pwrst);
	}

	return ret;
}

int pwrdm_read_next_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_next_pwrst)
		ret = arch_pwrdm->pwrdm_read_next_pwrst(pwrdm);

	return ret;
}

int pwrdm_read_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_pwrst)
		ret = arch_pwrdm->pwrdm_read_pwrst(pwrdm);

	return ret;
}

int pwrdm_read_prev_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_prev_pwrst)
		ret = arch_pwrdm->pwrdm_read_prev_pwrst(pwrdm);

	return ret;
}

int pwrdm_set_logic_retst(struct powerdomain *pwrdm, u8 pwrst)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (!(pwrdm->pwrsts_logic_ret & (1 << pwrst)))
		return -EINVAL;

	pr_debug("powerdomain: setting next logic powerstate for %s to %0x\n",
		 pwrdm->name, pwrst);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_logic_retst)
		ret = arch_pwrdm->pwrdm_set_logic_retst(pwrdm, pwrst);

	return ret;
}

int pwrdm_set_mem_onst(struct powerdomain *pwrdm, u8 bank, u8 pwrst)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->banks < (bank + 1))
		return -EEXIST;

	if (!(pwrdm->pwrsts_mem_on[bank] & (1 << pwrst)))
		return -EINVAL;

	pr_debug("powerdomain: setting next memory powerstate for domain %s "
		 "bank %0x while pwrdm-ON to %0x\n", pwrdm->name, bank, pwrst);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_mem_onst)
		ret = arch_pwrdm->pwrdm_set_mem_onst(pwrdm, bank, pwrst);

	return ret;
}

int pwrdm_set_mem_retst(struct powerdomain *pwrdm, u8 bank, u8 pwrst)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->banks < (bank + 1))
		return -EEXIST;

	if (!(pwrdm->pwrsts_mem_ret[bank] & (1 << pwrst)))
		return -EINVAL;

	pr_debug("powerdomain: setting next memory powerstate for domain %s "
		 "bank %0x while pwrdm-RET to %0x\n", pwrdm->name, bank, pwrst);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_mem_retst)
		ret = arch_pwrdm->pwrdm_set_mem_retst(pwrdm, bank, pwrst);

	return ret;
}

int pwrdm_read_logic_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_logic_pwrst)
		ret = arch_pwrdm->pwrdm_read_logic_pwrst(pwrdm);

	return ret;
}

int pwrdm_read_prev_logic_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_prev_logic_pwrst)
		ret = arch_pwrdm->pwrdm_read_prev_logic_pwrst(pwrdm);

	return ret;
}

int pwrdm_read_logic_retst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_logic_retst)
		ret = arch_pwrdm->pwrdm_read_logic_retst(pwrdm);

	return ret;
}

int pwrdm_read_mem_pwrst(struct powerdomain *pwrdm, u8 bank)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (pwrdm->banks < (bank + 1))
		return ret;

	if (pwrdm->flags & PWRDM_HAS_MPU_QUIRK)
		bank = 1;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_mem_pwrst)
		ret = arch_pwrdm->pwrdm_read_mem_pwrst(pwrdm, bank);

	return ret;
}

int pwrdm_read_prev_mem_pwrst(struct powerdomain *pwrdm, u8 bank)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (pwrdm->banks < (bank + 1))
		return ret;

	if (pwrdm->flags & PWRDM_HAS_MPU_QUIRK)
		bank = 1;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_prev_mem_pwrst)
		ret = arch_pwrdm->pwrdm_read_prev_mem_pwrst(pwrdm, bank);

	return ret;
}

int pwrdm_read_mem_retst(struct powerdomain *pwrdm, u8 bank)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (pwrdm->banks < (bank + 1))
		return ret;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_mem_retst)
		ret = arch_pwrdm->pwrdm_read_mem_retst(pwrdm, bank);

	return ret;
}

int pwrdm_clear_all_prev_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;


	pr_debug("powerdomain: clearing previous power state reg for %s\n",
		 pwrdm->name);

	if (arch_pwrdm && arch_pwrdm->pwrdm_clear_all_prev_pwrst)
		ret = arch_pwrdm->pwrdm_clear_all_prev_pwrst(pwrdm);

	return ret;
}

int pwrdm_enable_hdwr_sar(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (!(pwrdm->flags & PWRDM_HAS_HDWR_SAR))
		return ret;

	pr_debug("powerdomain: %s: setting SAVEANDRESTORE bit\n",
		 pwrdm->name);

	if (arch_pwrdm && arch_pwrdm->pwrdm_enable_hdwr_sar)
		ret = arch_pwrdm->pwrdm_enable_hdwr_sar(pwrdm);

	return ret;
}

int pwrdm_disable_hdwr_sar(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (!(pwrdm->flags & PWRDM_HAS_HDWR_SAR))
		return ret;

	pr_debug("powerdomain: %s: clearing SAVEANDRESTORE bit\n",
		 pwrdm->name);

	if (arch_pwrdm && arch_pwrdm->pwrdm_disable_hdwr_sar)
		ret = arch_pwrdm->pwrdm_disable_hdwr_sar(pwrdm);

	return ret;
}

bool pwrdm_has_hdwr_sar(struct powerdomain *pwrdm)
{
	return (pwrdm && pwrdm->flags & PWRDM_HAS_HDWR_SAR) ? 1 : 0;
}

int pwrdm_set_lowpwrstchange(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (!(pwrdm->flags & PWRDM_HAS_LOWPOWERSTATECHANGE))
		return -EINVAL;

	pr_debug("powerdomain: %s: setting LOWPOWERSTATECHANGE bit\n",
		 pwrdm->name);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_lowpwrstchange)
		ret = arch_pwrdm->pwrdm_set_lowpwrstchange(pwrdm);

	return ret;
}

int pwrdm_wait_transition(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_wait_transition)
		ret = arch_pwrdm->pwrdm_wait_transition(pwrdm);

	return ret;
}

int pwrdm_state_switch(struct powerdomain *pwrdm)
{
	int ret;

	ret = pwrdm_wait_transition(pwrdm);
	if (!ret)
		ret = _pwrdm_state_switch(pwrdm, PWRDM_STATE_NOW);

	return ret;
}

int pwrdm_clkdm_state_switch(struct clockdomain *clkdm)
{
	if (clkdm != NULL && clkdm->pwrdm.ptr != NULL) {
		pwrdm_wait_transition(clkdm->pwrdm.ptr);
		return pwrdm_state_switch(clkdm->pwrdm.ptr);
	}

	return -EINVAL;
}

int pwrdm_pre_transition(void)
{
	pwrdm_for_each(_pwrdm_pre_transition_cb, NULL);
	return 0;
}

int pwrdm_post_transition(void)
{
	pwrdm_for_each(_pwrdm_post_transition_cb, NULL);
	return 0;
}

int pwrdm_get_context_loss_count(struct powerdomain *pwrdm)
{
	int i, count;

	if (!pwrdm) {
		WARN(1, "powerdomain: %s: pwrdm is null\n", __func__);
		return -ENODEV;
	}

	count = pwrdm->state_counter[PWRDM_POWER_OFF];
	count += pwrdm->ret_logic_off_counter;

	for (i = 0; i < pwrdm->banks; i++)
		count += pwrdm->ret_mem_off_counter[i];

	count &= INT_MAX;

	pr_debug("powerdomain: %s: context loss count = %d\n",
		 pwrdm->name, count);

	return count;
}

bool pwrdm_can_ever_lose_context(struct powerdomain *pwrdm)
{
	int i;

	if (IS_ERR_OR_NULL(pwrdm)) {
		pr_debug("powerdomain: %s: invalid powerdomain pointer\n",
			 __func__);
		return 1;
	}

	if (pwrdm->pwrsts & PWRSTS_OFF)
		return 1;

	if (pwrdm->pwrsts & PWRSTS_RET) {
		if (pwrdm->pwrsts_logic_ret & PWRSTS_OFF)
			return 1;

		for (i = 0; i < pwrdm->banks; i++)
			if (pwrdm->pwrsts_mem_ret[i] & PWRSTS_OFF)
				return 1;
	}

	for (i = 0; i < pwrdm->banks; i++)
		if (pwrdm->pwrsts_mem_on[i] & PWRSTS_OFF)
			return 1;

	return 0;
}
