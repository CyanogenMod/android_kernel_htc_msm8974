/*
 * OMAP MPUSS low power code
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * OMAP4430 MPUSS mainly consists of dual Cortex-A9 with per-CPU
 * Local timer and Watchdog, GIC, SCU, PL310 L2 cache controller,
 * CPU0 and CPU1 LPRM modules.
 * CPU0, CPU1 and MPUSS each have there own power domain and
 * hence multiple low power combinations of MPUSS are possible.
 *
 * The CPU0 and CPU1 can't support Closed switch Retention (CSWR)
 * because the mode is not supported by hw constraints of dormant
 * mode. While waking up from the dormant mode, a reset  signal
 * to the Cortex-A9 processor must be asserted by the external
 * power controller.
 *
 * With architectural inputs and hardware recommendations, only
 * below modes are supported from power gain vs latency point of view.
 *
 *	CPU0		CPU1		MPUSS
 *	----------------------------------------------
 *	ON		ON		ON
 *	ON(Inactive)	OFF		ON(Inactive)
 *	OFF		OFF		CSWR
 *	OFF		OFF		OSWR
 *	OFF		OFF		OFF(Device OFF *TBD)
 *	----------------------------------------------
 *
 * Note: CPU0 is the master core and it is the last CPU to go down
 * and first to wake-up when MPUSS low power states are excercised
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/smp.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/smp_scu.h>
#include <asm/pgalloc.h>
#include <asm/suspend.h>
#include <asm/hardware/cache-l2x0.h>

#include <plat/omap44xx.h>

#include "common.h"
#include "omap4-sar-layout.h"
#include "pm.h"
#include "prcm_mpu44xx.h"
#include "prminst44xx.h"
#include "prcm44xx.h"
#include "prm44xx.h"
#include "prm-regbits-44xx.h"

#ifdef CONFIG_SMP

struct omap4_cpu_pm_info {
	struct powerdomain *pwrdm;
	void __iomem *scu_sar_addr;
	void __iomem *wkup_sar_addr;
	void __iomem *l2x0_sar_addr;
};

static DEFINE_PER_CPU(struct omap4_cpu_pm_info, omap4_pm_info);
static struct powerdomain *mpuss_pd;
static void __iomem *sar_base;

static inline void set_cpu_wakeup_addr(unsigned int cpu_id, u32 addr)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	__raw_writel(addr, pm_info->wkup_sar_addr);
}

static inline void set_cpu_next_pwrst(unsigned int cpu_id,
				unsigned int power_state)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	pwrdm_set_next_pwrst(pm_info->pwrdm, power_state);
}

static inline unsigned int read_cpu_prev_pwrst(unsigned int cpu_id)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	return pwrdm_read_prev_pwrst(pm_info->pwrdm);
}

static inline void clear_cpu_prev_pwrst(unsigned int cpu_id)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	pwrdm_clear_all_prev_pwrst(pm_info->pwrdm);
}

static void scu_pwrst_prepare(unsigned int cpu_id, unsigned int cpu_state)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);
	u32 scu_pwr_st;

	switch (cpu_state) {
	case PWRDM_POWER_RET:
		scu_pwr_st = SCU_PM_DORMANT;
		break;
	case PWRDM_POWER_OFF:
		scu_pwr_st = SCU_PM_POWEROFF;
		break;
	case PWRDM_POWER_ON:
	case PWRDM_POWER_INACTIVE:
	default:
		scu_pwr_st = SCU_PM_NORMAL;
		break;
	}

	__raw_writel(scu_pwr_st, pm_info->scu_sar_addr);
}

static inline void mpuss_clear_prev_logic_pwrst(void)
{
	u32 reg;

	reg = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_MPU_INST, OMAP4_RM_MPU_MPU_CONTEXT_OFFSET);
	omap4_prminst_write_inst_reg(reg, OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_MPU_INST, OMAP4_RM_MPU_MPU_CONTEXT_OFFSET);
}

static inline void cpu_clear_prev_logic_pwrst(unsigned int cpu_id)
{
	u32 reg;

	if (cpu_id) {
		reg = omap4_prcm_mpu_read_inst_reg(OMAP4430_PRCM_MPU_CPU1_INST,
					OMAP4_RM_CPU1_CPU1_CONTEXT_OFFSET);
		omap4_prcm_mpu_write_inst_reg(reg, OMAP4430_PRCM_MPU_CPU1_INST,
					OMAP4_RM_CPU1_CPU1_CONTEXT_OFFSET);
	} else {
		reg = omap4_prcm_mpu_read_inst_reg(OMAP4430_PRCM_MPU_CPU0_INST,
					OMAP4_RM_CPU0_CPU0_CONTEXT_OFFSET);
		omap4_prcm_mpu_write_inst_reg(reg, OMAP4430_PRCM_MPU_CPU0_INST,
					OMAP4_RM_CPU0_CPU0_CONTEXT_OFFSET);
	}
}

u32 omap4_mpuss_read_prev_context_state(void)
{
	u32 reg;

	reg = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_MPU_INST, OMAP4_RM_MPU_MPU_CONTEXT_OFFSET);
	reg &= OMAP4430_LOSTCONTEXT_DFF_MASK;
	return reg;
}

static void l2x0_pwrst_prepare(unsigned int cpu_id, unsigned int save_state)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	__raw_writel(save_state, pm_info->l2x0_sar_addr);
}

#ifdef CONFIG_CACHE_L2X0
static void save_l2x0_context(void)
{
	u32 val;
	void __iomem *l2x0_base = omap4_get_l2cache_base();

	val = __raw_readl(l2x0_base + L2X0_AUX_CTRL);
	__raw_writel(val, sar_base + L2X0_AUXCTRL_OFFSET);
	val = __raw_readl(l2x0_base + L2X0_PREFETCH_CTRL);
	__raw_writel(val, sar_base + L2X0_PREFETCH_CTRL_OFFSET);
}
#else
static void save_l2x0_context(void)
{}
#endif

int omap4_enter_lowpower(unsigned int cpu, unsigned int power_state)
{
	unsigned int save_state = 0;
	unsigned int wakeup_cpu;

	if (omap_rev() == OMAP4430_REV_ES1_0)
		return -ENXIO;

	switch (power_state) {
	case PWRDM_POWER_ON:
	case PWRDM_POWER_INACTIVE:
		save_state = 0;
		break;
	case PWRDM_POWER_OFF:
		save_state = 1;
		break;
	case PWRDM_POWER_RET:
	default:
		WARN_ON(1);
		return -ENXIO;
	}

	pwrdm_pre_transition();

	mpuss_clear_prev_logic_pwrst();
	if ((pwrdm_read_next_pwrst(mpuss_pd) == PWRDM_POWER_RET) &&
		(pwrdm_read_logic_retst(mpuss_pd) == PWRDM_POWER_OFF))
		save_state = 2;

	cpu_clear_prev_logic_pwrst(cpu);
	set_cpu_next_pwrst(cpu, power_state);
	set_cpu_wakeup_addr(cpu, virt_to_phys(omap4_cpu_resume));
	scu_pwrst_prepare(cpu, power_state);
	l2x0_pwrst_prepare(cpu, save_state);

	cpu_suspend(save_state, omap4_finish_suspend);

	wakeup_cpu = smp_processor_id();
	set_cpu_next_pwrst(wakeup_cpu, PWRDM_POWER_ON);

	pwrdm_post_transition();

	return 0;
}

int __cpuinit omap4_hotplug_cpu(unsigned int cpu, unsigned int power_state)
{
	unsigned int cpu_state = 0;

	if (omap_rev() == OMAP4430_REV_ES1_0)
		return -ENXIO;

	if (power_state == PWRDM_POWER_OFF)
		cpu_state = 1;

	clear_cpu_prev_pwrst(cpu);
	set_cpu_next_pwrst(cpu, power_state);
	set_cpu_wakeup_addr(cpu, virt_to_phys(omap_secondary_startup));
	scu_pwrst_prepare(cpu, power_state);

	omap4_finish_suspend(cpu_state);

	set_cpu_next_pwrst(cpu, PWRDM_POWER_ON);
	return 0;
}


int __init omap4_mpuss_init(void)
{
	struct omap4_cpu_pm_info *pm_info;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN(1, "Power Management not supported on OMAP4430 ES1.0\n");
		return -ENODEV;
	}

	sar_base = omap4_get_sar_ram_base();

	
	pm_info = &per_cpu(omap4_pm_info, 0x0);
	pm_info->scu_sar_addr = sar_base + SCU_OFFSET0;
	pm_info->wkup_sar_addr = sar_base + CPU0_WAKEUP_NS_PA_ADDR_OFFSET;
	pm_info->l2x0_sar_addr = sar_base + L2X0_SAVE_OFFSET0;
	pm_info->pwrdm = pwrdm_lookup("cpu0_pwrdm");
	if (!pm_info->pwrdm) {
		pr_err("Lookup failed for CPU0 pwrdm\n");
		return -ENODEV;
	}

	
	pwrdm_clear_all_prev_pwrst(pm_info->pwrdm);
	cpu_clear_prev_logic_pwrst(0);

	
	pwrdm_set_next_pwrst(pm_info->pwrdm, PWRDM_POWER_ON);

	pm_info = &per_cpu(omap4_pm_info, 0x1);
	pm_info->scu_sar_addr = sar_base + SCU_OFFSET1;
	pm_info->wkup_sar_addr = sar_base + CPU1_WAKEUP_NS_PA_ADDR_OFFSET;
	pm_info->l2x0_sar_addr = sar_base + L2X0_SAVE_OFFSET1;
	pm_info->pwrdm = pwrdm_lookup("cpu1_pwrdm");
	if (!pm_info->pwrdm) {
		pr_err("Lookup failed for CPU1 pwrdm\n");
		return -ENODEV;
	}

	
	pwrdm_clear_all_prev_pwrst(pm_info->pwrdm);
	cpu_clear_prev_logic_pwrst(1);

	
	pwrdm_set_next_pwrst(pm_info->pwrdm, PWRDM_POWER_ON);

	mpuss_pd = pwrdm_lookup("mpu_pwrdm");
	if (!mpuss_pd) {
		pr_err("Failed to lookup MPUSS power domain\n");
		return -ENODEV;
	}
	pwrdm_clear_all_prev_pwrst(mpuss_pd);
	mpuss_clear_prev_logic_pwrst();

	
	if (omap_type() != OMAP2_DEVICE_TYPE_GP)
		__raw_writel(1, sar_base + OMAP_TYPE_OFFSET);
	else
		__raw_writel(0, sar_base + OMAP_TYPE_OFFSET);

	save_l2x0_context();

	return 0;
}

#endif
