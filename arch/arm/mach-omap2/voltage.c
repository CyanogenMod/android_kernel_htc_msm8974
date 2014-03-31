/*
 * OMAP3/OMAP4 Voltage Management Routines
 *
 * Author: Thara Gopinath	<thara@ti.com>
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 * Lesly A M <x0080970@ti.com>
 *
 * Copyright (C) 2008, 2011 Nokia Corporation
 * Kalle Jokiniemi
 * Paul Walmsley
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Thara Gopinath <thara@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include "common.h"

#include "prm-regbits-34xx.h"
#include "prm-regbits-44xx.h"
#include "prm44xx.h"
#include "prcm44xx.h"
#include "prminst44xx.h"
#include "control.h"

#include "voltage.h"
#include "powerdomain.h"

#include "vc.h"
#include "vp.h"

static LIST_HEAD(voltdm_list);

unsigned long voltdm_get_voltage(struct voltagedomain *voltdm)
{
	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return 0;
	}

	return voltdm->nominal_volt;
}

int voltdm_scale(struct voltagedomain *voltdm,
		 unsigned long target_volt)
{
	int ret;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return -EINVAL;
	}

	if (!voltdm->scale) {
		pr_err("%s: No voltage scale API registered for vdd_%s\n",
			__func__, voltdm->name);
		return -ENODATA;
	}

	ret = voltdm->scale(voltdm, target_volt);
	if (!ret)
		voltdm->nominal_volt = target_volt;

	return ret;
}

void voltdm_reset(struct voltagedomain *voltdm)
{
	unsigned long target_volt;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return;
	}

	target_volt = voltdm_get_voltage(voltdm);
	if (!target_volt) {
		pr_err("%s: unable to find current voltage for vdd_%s\n",
			__func__, voltdm->name);
		return;
	}

	voltdm_scale(voltdm, target_volt);
}

void omap_voltage_get_volttable(struct voltagedomain *voltdm,
				struct omap_volt_data **volt_data)
{
	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return;
	}

	*volt_data = voltdm->volt_data;
}

struct omap_volt_data *omap_voltage_get_voltdata(struct voltagedomain *voltdm,
						 unsigned long volt)
{
	int i;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!voltdm->volt_data) {
		pr_warning("%s: voltage table does not exist for vdd_%s\n",
			__func__, voltdm->name);
		return ERR_PTR(-ENODATA);
	}

	for (i = 0; voltdm->volt_data[i].volt_nominal != 0; i++) {
		if (voltdm->volt_data[i].volt_nominal == volt)
			return &voltdm->volt_data[i];
	}

	pr_notice("%s: Unable to match the current voltage with the voltage"
		"table for vdd_%s\n", __func__, voltdm->name);

	return ERR_PTR(-ENODATA);
}

int omap_voltage_register_pmic(struct voltagedomain *voltdm,
			       struct omap_voltdm_pmic *pmic)
{
	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return -EINVAL;
	}

	voltdm->pmic = pmic;

	return 0;
}

void omap_change_voltscale_method(struct voltagedomain *voltdm,
				  int voltscale_method)
{
	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return;
	}

	switch (voltscale_method) {
	case VOLTSCALE_VPFORCEUPDATE:
		voltdm->scale = omap_vp_forceupdate_scale;
		return;
	case VOLTSCALE_VCBYPASS:
		voltdm->scale = omap_vc_bypass_scale;
		return;
	default:
		pr_warning("%s: Trying to change the method of voltage scaling"
			"to an unsupported one!\n", __func__);
	}
}

int __init omap_voltage_late_init(void)
{
	struct voltagedomain *voltdm;

	if (list_empty(&voltdm_list)) {
		pr_err("%s: Voltage driver support not added\n",
			__func__);
		return -EINVAL;
	}

	list_for_each_entry(voltdm, &voltdm_list, node) {
		struct clk *sys_ck;

		if (!voltdm->scalable)
			continue;

		sys_ck = clk_get(NULL, voltdm->sys_clk.name);
		if (IS_ERR(sys_ck)) {
			pr_warning("%s: Could not get sys clk.\n", __func__);
			return -EINVAL;
		}
		voltdm->sys_clk.rate = clk_get_rate(sys_ck);
		WARN_ON(!voltdm->sys_clk.rate);
		clk_put(sys_ck);

		if (voltdm->vc) {
			voltdm->scale = omap_vc_bypass_scale;
			omap_vc_init_channel(voltdm);
		}

		if (voltdm->vp) {
			voltdm->scale = omap_vp_forceupdate_scale;
			omap_vp_init(voltdm);
		}
	}

	return 0;
}

static struct voltagedomain *_voltdm_lookup(const char *name)
{
	struct voltagedomain *voltdm, *temp_voltdm;

	voltdm = NULL;

	list_for_each_entry(temp_voltdm, &voltdm_list, node) {
		if (!strcmp(name, temp_voltdm->name)) {
			voltdm = temp_voltdm;
			break;
		}
	}

	return voltdm;
}

int voltdm_add_pwrdm(struct voltagedomain *voltdm, struct powerdomain *pwrdm)
{
	if (!voltdm || !pwrdm)
		return -EINVAL;

	pr_debug("voltagedomain: associating powerdomain %s with voltagedomain "
		 "%s\n", pwrdm->name, voltdm->name);

	list_add(&pwrdm->voltdm_node, &voltdm->pwrdm_list);

	return 0;
}

int voltdm_for_each_pwrdm(struct voltagedomain *voltdm,
			  int (*fn)(struct voltagedomain *voltdm,
				    struct powerdomain *pwrdm))
{
	struct powerdomain *pwrdm;
	int ret = 0;

	if (!fn)
		return -EINVAL;

	list_for_each_entry(pwrdm, &voltdm->pwrdm_list, voltdm_node)
		ret = (*fn)(voltdm, pwrdm);

	return ret;
}

int voltdm_for_each(int (*fn)(struct voltagedomain *voltdm, void *user),
		    void *user)
{
	struct voltagedomain *temp_voltdm;
	int ret = 0;

	if (!fn)
		return -EINVAL;

	list_for_each_entry(temp_voltdm, &voltdm_list, node) {
		ret = (*fn)(temp_voltdm, user);
		if (ret)
			break;
	}

	return ret;
}

static int _voltdm_register(struct voltagedomain *voltdm)
{
	if (!voltdm || !voltdm->name)
		return -EINVAL;

	INIT_LIST_HEAD(&voltdm->pwrdm_list);
	list_add(&voltdm->node, &voltdm_list);

	pr_debug("voltagedomain: registered %s\n", voltdm->name);

	return 0;
}

struct voltagedomain *voltdm_lookup(const char *name)
{
	struct voltagedomain *voltdm ;

	if (!name)
		return NULL;

	voltdm = _voltdm_lookup(name);

	return voltdm;
}

void voltdm_init(struct voltagedomain **voltdms)
{
	struct voltagedomain **v;

	if (voltdms) {
		for (v = voltdms; *v; v++)
			_voltdm_register(*v);
	}
}
