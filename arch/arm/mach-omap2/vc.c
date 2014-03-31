/*
 * OMAP Voltage Controller (VC) interface
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/bug.h>

#include <plat/cpu.h>

#include "voltage.h"
#include "vc.h"
#include "prm-regbits-34xx.h"
#include "prm-regbits-44xx.h"
#include "prm44xx.h"

struct omap_vc_channel_cfg {
	u8 sa;
	u8 rav;
	u8 rac;
	u8 racen;
	u8 cmd;
};

static struct omap_vc_channel_cfg vc_default_channel_cfg = {
	.sa    = BIT(0),
	.rav   = BIT(1),
	.rac   = BIT(2),
	.racen = BIT(3),
	.cmd   = BIT(4),
};

static struct omap_vc_channel_cfg vc_mutant_channel_cfg = {
	.sa    = BIT(0),
	.rav   = BIT(2),
	.rac   = BIT(3),
	.racen = BIT(4),
	.cmd   = BIT(1),
};

static struct omap_vc_channel_cfg *vc_cfg_bits;
#define CFG_CHANNEL_MASK 0x1f

static int omap_vc_config_channel(struct voltagedomain *voltdm)
{
	struct omap_vc_channel *vc = voltdm->vc;

	if (vc->flags & OMAP_VC_CHANNEL_DEFAULT)
		vc->cfg_channel &= vc_cfg_bits->racen;

	voltdm->rmw(CFG_CHANNEL_MASK << vc->cfg_channel_sa_shift,
		    vc->cfg_channel << vc->cfg_channel_sa_shift,
		    vc->cfg_channel_reg);

	return 0;
}

int omap_vc_pre_scale(struct voltagedomain *voltdm,
		      unsigned long target_volt,
		      u8 *target_vsel, u8 *current_vsel)
{
	struct omap_vc_channel *vc = voltdm->vc;
	u32 vc_cmdval;

	
	if (!voltdm->pmic) {
		pr_err("%s: Insufficient pmic info to scale the vdd_%s\n",
			__func__, voltdm->name);
		return -EINVAL;
	}

	if (!voltdm->pmic->uv_to_vsel) {
		pr_err("%s: PMIC function to convert voltage in uV to"
			"vsel not registered. Hence unable to scale voltage"
			"for vdd_%s\n", __func__, voltdm->name);
		return -ENODATA;
	}

	if (!voltdm->read || !voltdm->write) {
		pr_err("%s: No read/write API for accessing vdd_%s regs\n",
			__func__, voltdm->name);
		return -EINVAL;
	}

	*target_vsel = voltdm->pmic->uv_to_vsel(target_volt);
	*current_vsel = voltdm->pmic->uv_to_vsel(voltdm->nominal_volt);

	
	vc_cmdval = voltdm->read(vc->cmdval_reg);
	vc_cmdval &= ~vc->common->cmd_on_mask;
	vc_cmdval |= (*target_vsel << vc->common->cmd_on_shift);
	voltdm->write(vc_cmdval, vc->cmdval_reg);

	omap_vp_update_errorgain(voltdm, target_volt);

	return 0;
}

void omap_vc_post_scale(struct voltagedomain *voltdm,
			unsigned long target_volt,
			u8 target_vsel, u8 current_vsel)
{
	u32 smps_steps = 0, smps_delay = 0;

	smps_steps = abs(target_vsel - current_vsel);
	
	smps_delay = ((smps_steps * voltdm->pmic->step_size) /
			voltdm->pmic->slew_rate) + 2;
	udelay(smps_delay);
}

int omap_vc_bypass_scale(struct voltagedomain *voltdm,
			 unsigned long target_volt)
{
	struct omap_vc_channel *vc = voltdm->vc;
	u32 loop_cnt = 0, retries_cnt = 0;
	u32 vc_valid, vc_bypass_val_reg, vc_bypass_value;
	u8 target_vsel, current_vsel;
	int ret;

	ret = omap_vc_pre_scale(voltdm, target_volt, &target_vsel, &current_vsel);
	if (ret)
		return ret;

	vc_valid = vc->common->valid;
	vc_bypass_val_reg = vc->common->bypass_val_reg;
	vc_bypass_value = (target_vsel << vc->common->data_shift) |
		(vc->volt_reg_addr << vc->common->regaddr_shift) |
		(vc->i2c_slave_addr << vc->common->slaveaddr_shift);

	voltdm->write(vc_bypass_value, vc_bypass_val_reg);
	voltdm->write(vc_bypass_value | vc_valid, vc_bypass_val_reg);

	vc_bypass_value = voltdm->read(vc_bypass_val_reg);
	while (!(vc_bypass_value & vc_valid)) {
		loop_cnt++;

		if (retries_cnt > 10) {
			pr_warning("%s: Retry count exceeded\n", __func__);
			return -ETIMEDOUT;
		}

		if (loop_cnt > 50) {
			retries_cnt++;
			loop_cnt = 0;
			udelay(10);
		}
		vc_bypass_value = voltdm->read(vc_bypass_val_reg);
	}

	omap_vc_post_scale(voltdm, target_volt, target_vsel, current_vsel);
	return 0;
}

static void __init omap3_vfsm_init(struct voltagedomain *voltdm)
{
	voltdm->write(OMAP3_CLKSETUP, OMAP3_PRM_CLKSETUP_OFFSET);
	voltdm->write(OMAP3_VOLTOFFSET, OMAP3_PRM_VOLTOFFSET_OFFSET);
	voltdm->write(OMAP3_VOLTSETUP2, OMAP3_PRM_VOLTSETUP2_OFFSET);
}

static void __init omap3_vc_init_channel(struct voltagedomain *voltdm)
{
	static bool is_initialized;

	if (is_initialized)
		return;

	omap3_vfsm_init(voltdm);

	is_initialized = true;
}


static void __init omap4_vc_init_channel(struct voltagedomain *voltdm)
{
	static bool is_initialized;
	u32 vc_val;

	if (is_initialized)
		return;

	
	vc_val = (0x60 << OMAP4430_SCLL_SHIFT | 0x26 << OMAP4430_SCLH_SHIFT);
	voltdm->write(vc_val, OMAP4_PRM_VC_CFG_I2C_CLK_OFFSET);

	is_initialized = true;
}

static void __init omap_vc_i2c_init(struct voltagedomain *voltdm)
{
	struct omap_vc_channel *vc = voltdm->vc;
	static bool initialized;
	static bool i2c_high_speed;
	u8 mcode;

	if (initialized) {
		if (voltdm->pmic->i2c_high_speed != i2c_high_speed)
			pr_warn("%s: I2C config for vdd_%s does not match other channels (%u).",
				__func__, voltdm->name, i2c_high_speed);
		return;
	}

	i2c_high_speed = voltdm->pmic->i2c_high_speed;
	if (i2c_high_speed)
		voltdm->rmw(vc->common->i2c_cfg_hsen_mask,
			    vc->common->i2c_cfg_hsen_mask,
			    vc->common->i2c_cfg_reg);

	mcode = voltdm->pmic->i2c_mcode;
	if (mcode)
		voltdm->rmw(vc->common->i2c_mcode_mask,
			    mcode << __ffs(vc->common->i2c_mcode_mask),
			    vc->common->i2c_cfg_reg);

	initialized = true;
}

void __init omap_vc_init_channel(struct voltagedomain *voltdm)
{
	struct omap_vc_channel *vc = voltdm->vc;
	u8 on_vsel, onlp_vsel, ret_vsel, off_vsel;
	u32 val;

	if (!voltdm->pmic || !voltdm->pmic->uv_to_vsel) {
		pr_err("%s: No PMIC info for vdd_%s\n", __func__, voltdm->name);
		return;
	}

	if (!voltdm->read || !voltdm->write) {
		pr_err("%s: No read/write API for accessing vdd_%s regs\n",
			__func__, voltdm->name);
		return;
	}

	vc->cfg_channel = 0;
	if (vc->flags & OMAP_VC_CHANNEL_CFG_MUTANT)
		vc_cfg_bits = &vc_mutant_channel_cfg;
	else
		vc_cfg_bits = &vc_default_channel_cfg;

	
	vc->i2c_slave_addr = voltdm->pmic->i2c_slave_addr;
	vc->volt_reg_addr = voltdm->pmic->volt_reg_addr;
	vc->cmd_reg_addr = voltdm->pmic->cmd_reg_addr;
	vc->setup_time = voltdm->pmic->volt_setup_time;

	
	voltdm->rmw(vc->smps_sa_mask,
		    vc->i2c_slave_addr << __ffs(vc->smps_sa_mask),
		    vc->smps_sa_reg);
	vc->cfg_channel |= vc_cfg_bits->sa;

	voltdm->rmw(vc->smps_volra_mask,
		    vc->volt_reg_addr << __ffs(vc->smps_volra_mask),
		    vc->smps_volra_reg);
	vc->cfg_channel |= vc_cfg_bits->rav;

	if (vc->cmd_reg_addr) {
		voltdm->rmw(vc->smps_cmdra_mask,
			    vc->cmd_reg_addr << __ffs(vc->smps_cmdra_mask),
			    vc->smps_cmdra_reg);
		vc->cfg_channel |= vc_cfg_bits->rac | vc_cfg_bits->racen;
	}

	
	on_vsel = voltdm->pmic->uv_to_vsel(voltdm->pmic->on_volt);
	onlp_vsel = voltdm->pmic->uv_to_vsel(voltdm->pmic->onlp_volt);
	ret_vsel = voltdm->pmic->uv_to_vsel(voltdm->pmic->ret_volt);
	off_vsel = voltdm->pmic->uv_to_vsel(voltdm->pmic->off_volt);
	val = ((on_vsel << vc->common->cmd_on_shift) |
	       (onlp_vsel << vc->common->cmd_onlp_shift) |
	       (ret_vsel << vc->common->cmd_ret_shift) |
	       (off_vsel << vc->common->cmd_off_shift));
	voltdm->write(val, vc->cmdval_reg);
	vc->cfg_channel |= vc_cfg_bits->cmd;

	
	omap_vc_config_channel(voltdm);

	
	voltdm->rmw(voltdm->vfsm->voltsetup_mask,
		    vc->setup_time << __ffs(voltdm->vfsm->voltsetup_mask),
		    voltdm->vfsm->voltsetup_reg);

	omap_vc_i2c_init(voltdm);

	if (cpu_is_omap34xx())
		omap3_vc_init_channel(voltdm);
	else if (cpu_is_omap44xx())
		omap4_vc_init_channel(voltdm);
}

