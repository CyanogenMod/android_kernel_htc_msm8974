/*
 * omap_hwmod implementation for OMAP2/3/4
 *
 * Copyright (C) 2009-2011 Nokia Corporation
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Paul Walmsley, Benoît Cousson, Kevin Hilman
 *
 * Created in collaboration with (alphabetical order): Thara Gopinath,
 * Tony Lindgren, Rajendra Nayak, Vikram Pandita, Sakari Poussa, Anand
 * Sawant, Santosh Shilimkar, Richard Woodruff
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Introduction
 * ------------
 * One way to view an OMAP SoC is as a collection of largely unrelated
 * IP blocks connected by interconnects.  The IP blocks include
 * devices such as ARM processors, audio serial interfaces, UARTs,
 * etc.  Some of these devices, like the DSP, are created by TI;
 * others, like the SGX, largely originate from external vendors.  In
 * TI's documentation, on-chip devices are referred to as "OMAP
 * modules."  Some of these IP blocks are identical across several
 * OMAP versions.  Others are revised frequently.
 *
 * These OMAP modules are tied together by various interconnects.
 * Most of the address and data flow between modules is via OCP-based
 * interconnects such as the L3 and L4 buses; but there are other
 * interconnects that distribute the hardware clock tree, handle idle
 * and reset signaling, supply power, and connect the modules to
 * various pads or balls on the OMAP package.
 *
 * OMAP hwmod provides a consistent way to describe the on-chip
 * hardware blocks and their integration into the rest of the chip.
 * This description can be automatically generated from the TI
 * hardware database.  OMAP hwmod provides a standard, consistent API
 * to reset, enable, idle, and disable these hardware blocks.  And
 * hwmod provides a way for other core code, such as the Linux device
 * code or the OMAP power management and address space mapping code,
 * to query the hardware database.
 *
 * Using hwmod
 * -----------
 * Drivers won't call hwmod functions directly.  That is done by the
 * omap_device code, and in rare occasions, by custom integration code
 * in arch/arm/ *omap*.  The omap_device code includes functions to
 * build a struct platform_device using omap_hwmod data, and that is
 * currently how hwmod data is communicated to drivers and to the
 * Linux driver model.  Most drivers will call omap_hwmod functions only
 * indirectly, via pm_runtime*() functions.
 *
 * From a layering perspective, here is where the OMAP hwmod code
 * fits into the kernel software stack:
 *
 *            +-------------------------------+
 *            |      Device driver code       |
 *            |      (e.g., drivers/)         |
 *            +-------------------------------+
 *            |      Linux driver model       |
 *            |     (platform_device /        |
 *            |  platform_driver data/code)   |
 *            +-------------------------------+
 *            | OMAP core-driver integration  |
 *            |(arch/arm/mach-omap2/devices.c)|
 *            +-------------------------------+
 *            |      omap_device code         |
 *            | (../plat-omap/omap_device.c)  |
 *            +-------------------------------+
 *   ---->    |    omap_hwmod code/data       |    <-----
 *            | (../mach-omap2/omap_hwmod*)   |
 *            +-------------------------------+
 *            | OMAP clock/PRCM/register fns  |
 *            | (__raw_{read,write}l, clk*)   |
 *            +-------------------------------+
 *
 * Device drivers should not contain any OMAP-specific code or data in
 * them.  They should only contain code to operate the IP block that
 * the driver is responsible for.  This is because these IP blocks can
 * also appear in other SoCs, either from TI (such as DaVinci) or from
 * other manufacturers; and drivers should be reusable across other
 * platforms.
 *
 * The OMAP hwmod code also will attempt to reset and idle all on-chip
 * devices upon boot.  The goal here is for the kernel to be
 * completely self-reliant and independent from bootloaders.  This is
 * to ensure a repeatable configuration, both to ensure consistent
 * runtime behavior, and to make it easier for others to reproduce
 * bugs.
 *
 * OMAP module activity states
 * ---------------------------
 * The hwmod code considers modules to be in one of several activity
 * states.  IP blocks start out in an UNKNOWN state, then once they
 * are registered via the hwmod code, proceed to the REGISTERED state.
 * Once their clock names are resolved to clock pointers, the module
 * enters the CLKS_INITED state; and finally, once the module has been
 * reset and the integration registers programmed, the INITIALIZED state
 * is entered.  The hwmod code will then place the module into either
 * the IDLE state to save power, or in the case of a critical system
 * module, the ENABLED state.
 *
 * OMAP core integration code can then call omap_hwmod*() functions
 * directly to move the module between the IDLE, ENABLED, and DISABLED
 * states, as needed.  This is done during both the PM idle loop, and
 * in the OMAP core integration code's implementation of the PM runtime
 * functions.
 *
 * References
 * ----------
 * This is a partial list.
 * - OMAP2420 Multimedia Processor Silicon Revision 2.1.1, 2.2 (SWPU064)
 * - OMAP2430 Multimedia Device POP Silicon Revision 2.1 (SWPU090)
 * - OMAP34xx Multimedia Device Silicon Revision 3.1 (SWPU108)
 * - OMAP4430 Multimedia Device Silicon Revision 1.0 (SWPU140)
 * - Open Core Protocol Specification 2.2
 *
 * To do:
 * - handle IO mapping
 * - bus throughput & module latency measurement code
 *
 * XXX add tests at the beginning of each function to ensure the hwmod is
 * in the appropriate state
 * XXX error return values should be checked to ensure that they are
 * appropriate
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "common.h"
#include <plat/cpu.h>
#include "clockdomain.h"
#include "powerdomain.h"
#include <plat/clock.h>
#include <plat/omap_hwmod.h>
#include <plat/prcm.h>

#include "cm2xxx_3xxx.h"
#include "cminst44xx.h"
#include "prm2xxx_3xxx.h"
#include "prm44xx.h"
#include "prminst44xx.h"
#include "mux.h"

#define MAX_MODULE_SOFTRESET_WAIT	10000

#define MPU_INITIATOR_NAME		"mpu"

static LIST_HEAD(omap_hwmod_list);

static struct omap_hwmod *mpu_oh;



static int _update_sysc_cache(struct omap_hwmod *oh)
{
	if (!oh->class->sysc) {
		WARN(1, "omap_hwmod: %s: cannot read OCP_SYSCONFIG: not defined on hwmod's class\n", oh->name);
		return -EINVAL;
	}

	

	oh->_sysc_cache = omap_hwmod_read(oh, oh->class->sysc->sysc_offs);

	if (!(oh->class->sysc->sysc_flags & SYSC_NO_CACHE))
		oh->_int_flags |= _HWMOD_SYSCONFIG_LOADED;

	return 0;
}

static void _write_sysconfig(u32 v, struct omap_hwmod *oh)
{
	if (!oh->class->sysc) {
		WARN(1, "omap_hwmod: %s: cannot write OCP_SYSCONFIG: not defined on hwmod's class\n", oh->name);
		return;
	}

	

	
	oh->_sysc_cache = v;
	omap_hwmod_write(v, oh, oh->class->sysc->sysc_offs);
}

static int _set_master_standbymode(struct omap_hwmod *oh, u8 standbymode,
				   u32 *v)
{
	u32 mstandby_mask;
	u8 mstandby_shift;

	if (!oh->class->sysc ||
	    !(oh->class->sysc->sysc_flags & SYSC_HAS_MIDLEMODE))
		return -EINVAL;

	if (!oh->class->sysc->sysc_fields) {
		WARN(1, "omap_hwmod: %s: offset struct for sysconfig not provided in class\n", oh->name);
		return -EINVAL;
	}

	mstandby_shift = oh->class->sysc->sysc_fields->midle_shift;
	mstandby_mask = (0x3 << mstandby_shift);

	*v &= ~mstandby_mask;
	*v |= __ffs(standbymode) << mstandby_shift;

	return 0;
}

static int _set_slave_idlemode(struct omap_hwmod *oh, u8 idlemode, u32 *v)
{
	u32 sidle_mask;
	u8 sidle_shift;

	if (!oh->class->sysc ||
	    !(oh->class->sysc->sysc_flags & SYSC_HAS_SIDLEMODE))
		return -EINVAL;

	if (!oh->class->sysc->sysc_fields) {
		WARN(1, "omap_hwmod: %s: offset struct for sysconfig not provided in class\n", oh->name);
		return -EINVAL;
	}

	sidle_shift = oh->class->sysc->sysc_fields->sidle_shift;
	sidle_mask = (0x3 << sidle_shift);

	*v &= ~sidle_mask;
	*v |= __ffs(idlemode) << sidle_shift;

	return 0;
}

static int _set_clockactivity(struct omap_hwmod *oh, u8 clockact, u32 *v)
{
	u32 clkact_mask;
	u8  clkact_shift;

	if (!oh->class->sysc ||
	    !(oh->class->sysc->sysc_flags & SYSC_HAS_CLOCKACTIVITY))
		return -EINVAL;

	if (!oh->class->sysc->sysc_fields) {
		WARN(1, "omap_hwmod: %s: offset struct for sysconfig not provided in class\n", oh->name);
		return -EINVAL;
	}

	clkact_shift = oh->class->sysc->sysc_fields->clkact_shift;
	clkact_mask = (0x3 << clkact_shift);

	*v &= ~clkact_mask;
	*v |= clockact << clkact_shift;

	return 0;
}

static int _set_softreset(struct omap_hwmod *oh, u32 *v)
{
	u32 softrst_mask;

	if (!oh->class->sysc ||
	    !(oh->class->sysc->sysc_flags & SYSC_HAS_SOFTRESET))
		return -EINVAL;

	if (!oh->class->sysc->sysc_fields) {
		WARN(1, "omap_hwmod: %s: offset struct for sysconfig not provided in class\n", oh->name);
		return -EINVAL;
	}

	softrst_mask = (0x1 << oh->class->sysc->sysc_fields->srst_shift);

	*v |= softrst_mask;

	return 0;
}

static int _set_module_autoidle(struct omap_hwmod *oh, u8 autoidle,
				u32 *v)
{
	u32 autoidle_mask;
	u8 autoidle_shift;

	if (!oh->class->sysc ||
	    !(oh->class->sysc->sysc_flags & SYSC_HAS_AUTOIDLE))
		return -EINVAL;

	if (!oh->class->sysc->sysc_fields) {
		WARN(1, "omap_hwmod: %s: offset struct for sysconfig not provided in class\n", oh->name);
		return -EINVAL;
	}

	autoidle_shift = oh->class->sysc->sysc_fields->autoidle_shift;
	autoidle_mask = (0x1 << autoidle_shift);

	*v &= ~autoidle_mask;
	*v |= autoidle << autoidle_shift;

	return 0;
}

/**
 * _set_idle_ioring_wakeup - enable/disable IO pad wakeup on hwmod idle for mux
 * @oh: struct omap_hwmod *
 * @set_wake: bool value indicating to set (true) or clear (false) wakeup enable
 *
 * Set or clear the I/O pad wakeup flag in the mux entries for the
 * hwmod @oh.  This function changes the @oh->mux->pads_dynamic array
 * in memory.  If the hwmod is currently idled, and the new idle
 * values don't match the previous ones, this function will also
 * update the SCM PADCTRL registers.  Otherwise, if the hwmod is not
 * currently idled, this function won't touch the hardware: the new
 * mux settings are written to the SCM PADCTRL registers when the
 * hwmod is idled.  No return value.
 */
static void _set_idle_ioring_wakeup(struct omap_hwmod *oh, bool set_wake)
{
	struct omap_device_pad *pad;
	bool change = false;
	u16 prev_idle;
	int j;

	if (!oh->mux || !oh->mux->enabled)
		return;

	for (j = 0; j < oh->mux->nr_pads_dynamic; j++) {
		pad = oh->mux->pads_dynamic[j];

		if (!(pad->flags & OMAP_DEVICE_PAD_WAKEUP))
			continue;

		prev_idle = pad->idle;

		if (set_wake)
			pad->idle |= OMAP_WAKEUP_EN;
		else
			pad->idle &= ~OMAP_WAKEUP_EN;

		if (prev_idle != pad->idle)
			change = true;
	}

	if (change && oh->_state == _HWMOD_STATE_IDLE)
		omap_hwmod_mux(oh->mux, _HWMOD_STATE_IDLE);
}

static int _enable_wakeup(struct omap_hwmod *oh, u32 *v)
{
	if (!oh->class->sysc ||
	    !((oh->class->sysc->sysc_flags & SYSC_HAS_ENAWAKEUP) ||
	      (oh->class->sysc->idlemodes & SIDLE_SMART_WKUP) ||
	      (oh->class->sysc->idlemodes & MSTANDBY_SMART_WKUP)))
		return -EINVAL;

	if (!oh->class->sysc->sysc_fields) {
		WARN(1, "omap_hwmod: %s: offset struct for sysconfig not provided in class\n", oh->name);
		return -EINVAL;
	}

	if (oh->class->sysc->sysc_flags & SYSC_HAS_ENAWAKEUP)
		*v |= 0x1 << oh->class->sysc->sysc_fields->enwkup_shift;

	if (oh->class->sysc->idlemodes & SIDLE_SMART_WKUP)
		_set_slave_idlemode(oh, HWMOD_IDLEMODE_SMART_WKUP, v);
	if (oh->class->sysc->idlemodes & MSTANDBY_SMART_WKUP)
		_set_master_standbymode(oh, HWMOD_IDLEMODE_SMART_WKUP, v);

	

	oh->_int_flags |= _HWMOD_WAKEUP_ENABLED;

	return 0;
}

static int _disable_wakeup(struct omap_hwmod *oh, u32 *v)
{
	if (!oh->class->sysc ||
	    !((oh->class->sysc->sysc_flags & SYSC_HAS_ENAWAKEUP) ||
	      (oh->class->sysc->idlemodes & SIDLE_SMART_WKUP) ||
	      (oh->class->sysc->idlemodes & MSTANDBY_SMART_WKUP)))
		return -EINVAL;

	if (!oh->class->sysc->sysc_fields) {
		WARN(1, "omap_hwmod: %s: offset struct for sysconfig not provided in class\n", oh->name);
		return -EINVAL;
	}

	if (oh->class->sysc->sysc_flags & SYSC_HAS_ENAWAKEUP)
		*v &= ~(0x1 << oh->class->sysc->sysc_fields->enwkup_shift);

	if (oh->class->sysc->idlemodes & SIDLE_SMART_WKUP)
		_set_slave_idlemode(oh, HWMOD_IDLEMODE_SMART, v);
	if (oh->class->sysc->idlemodes & MSTANDBY_SMART_WKUP)
		_set_master_standbymode(oh, HWMOD_IDLEMODE_SMART_WKUP, v);

	

	oh->_int_flags &= ~_HWMOD_WAKEUP_ENABLED;

	return 0;
}

static int _add_initiator_dep(struct omap_hwmod *oh, struct omap_hwmod *init_oh)
{
	if (!oh->_clk)
		return -EINVAL;

	if (oh->_clk->clkdm && oh->_clk->clkdm->flags & CLKDM_NO_AUTODEPS)
		return 0;

	return clkdm_add_sleepdep(oh->_clk->clkdm, init_oh->_clk->clkdm);
}

static int _del_initiator_dep(struct omap_hwmod *oh, struct omap_hwmod *init_oh)
{
	if (!oh->_clk)
		return -EINVAL;

	if (oh->_clk->clkdm && oh->_clk->clkdm->flags & CLKDM_NO_AUTODEPS)
		return 0;

	return clkdm_del_sleepdep(oh->_clk->clkdm, init_oh->_clk->clkdm);
}

static int _init_main_clk(struct omap_hwmod *oh)
{
	int ret = 0;

	if (!oh->main_clk)
		return 0;

	oh->_clk = omap_clk_get_by_name(oh->main_clk);
	if (!oh->_clk) {
		pr_warning("omap_hwmod: %s: cannot clk_get main_clk %s\n",
			   oh->name, oh->main_clk);
		return -EINVAL;
	}

	if (!oh->_clk->clkdm)
		pr_warning("omap_hwmod: %s: missing clockdomain for %s.\n",
			   oh->main_clk, oh->_clk->name);

	return ret;
}

static int _init_interface_clks(struct omap_hwmod *oh)
{
	struct clk *c;
	int i;
	int ret = 0;

	if (oh->slaves_cnt == 0)
		return 0;

	for (i = 0; i < oh->slaves_cnt; i++) {
		struct omap_hwmod_ocp_if *os = oh->slaves[i];

		if (!os->clk)
			continue;

		c = omap_clk_get_by_name(os->clk);
		if (!c) {
			pr_warning("omap_hwmod: %s: cannot clk_get interface_clk %s\n",
				   oh->name, os->clk);
			ret = -EINVAL;
		}
		os->_clk = c;
	}

	return ret;
}

static int _init_opt_clks(struct omap_hwmod *oh)
{
	struct omap_hwmod_opt_clk *oc;
	struct clk *c;
	int i;
	int ret = 0;

	for (i = oh->opt_clks_cnt, oc = oh->opt_clks; i > 0; i--, oc++) {
		c = omap_clk_get_by_name(oc->clk);
		if (!c) {
			pr_warning("omap_hwmod: %s: cannot clk_get opt_clk %s\n",
				   oh->name, oc->clk);
			ret = -EINVAL;
		}
		oc->_clk = c;
	}

	return ret;
}

static int _enable_clocks(struct omap_hwmod *oh)
{
	int i;

	pr_debug("omap_hwmod: %s: enabling clocks\n", oh->name);

	if (oh->_clk)
		clk_enable(oh->_clk);

	if (oh->slaves_cnt > 0) {
		for (i = 0; i < oh->slaves_cnt; i++) {
			struct omap_hwmod_ocp_if *os = oh->slaves[i];
			struct clk *c = os->_clk;

			if (c && (os->flags & OCPIF_SWSUP_IDLE))
				clk_enable(c);
		}
	}

	

	return 0;
}

static int _disable_clocks(struct omap_hwmod *oh)
{
	int i;

	pr_debug("omap_hwmod: %s: disabling clocks\n", oh->name);

	if (oh->_clk)
		clk_disable(oh->_clk);

	if (oh->slaves_cnt > 0) {
		for (i = 0; i < oh->slaves_cnt; i++) {
			struct omap_hwmod_ocp_if *os = oh->slaves[i];
			struct clk *c = os->_clk;

			if (c && (os->flags & OCPIF_SWSUP_IDLE))
				clk_disable(c);
		}
	}

	

	return 0;
}

static void _enable_optional_clocks(struct omap_hwmod *oh)
{
	struct omap_hwmod_opt_clk *oc;
	int i;

	pr_debug("omap_hwmod: %s: enabling optional clocks\n", oh->name);

	for (i = oh->opt_clks_cnt, oc = oh->opt_clks; i > 0; i--, oc++)
		if (oc->_clk) {
			pr_debug("omap_hwmod: enable %s:%s\n", oc->role,
				 oc->_clk->name);
			clk_enable(oc->_clk);
		}
}

static void _disable_optional_clocks(struct omap_hwmod *oh)
{
	struct omap_hwmod_opt_clk *oc;
	int i;

	pr_debug("omap_hwmod: %s: disabling optional clocks\n", oh->name);

	for (i = oh->opt_clks_cnt, oc = oh->opt_clks; i > 0; i--, oc++)
		if (oc->_clk) {
			pr_debug("omap_hwmod: disable %s:%s\n", oc->role,
				 oc->_clk->name);
			clk_disable(oc->_clk);
		}
}

static void _enable_module(struct omap_hwmod *oh)
{
	
	if (cpu_is_omap24xx() || cpu_is_omap34xx())
		return;

	if (!oh->clkdm || !oh->prcm.omap4.modulemode)
		return;

	pr_debug("omap_hwmod: %s: _enable_module: %d\n",
		 oh->name, oh->prcm.omap4.modulemode);

	omap4_cminst_module_enable(oh->prcm.omap4.modulemode,
				   oh->clkdm->prcm_partition,
				   oh->clkdm->cm_inst,
				   oh->clkdm->clkdm_offs,
				   oh->prcm.omap4.clkctrl_offs);
}

static int _omap4_wait_target_disable(struct omap_hwmod *oh)
{
	if (!cpu_is_omap44xx())
		return 0;

	if (!oh)
		return -EINVAL;

	if (oh->_int_flags & _HWMOD_NO_MPU_PORT)
		return 0;

	if (oh->flags & HWMOD_NO_IDLEST)
		return 0;

	return omap4_cminst_wait_module_idle(oh->clkdm->prcm_partition,
					     oh->clkdm->cm_inst,
					     oh->clkdm->clkdm_offs,
					     oh->prcm.omap4.clkctrl_offs);
}

static int _omap4_disable_module(struct omap_hwmod *oh)
{
	int v;

	
	if (!cpu_is_omap44xx())
		return -EINVAL;

	if (!oh->clkdm || !oh->prcm.omap4.modulemode)
		return -EINVAL;

	pr_debug("omap_hwmod: %s: %s\n", oh->name, __func__);

	omap4_cminst_module_disable(oh->clkdm->prcm_partition,
				    oh->clkdm->cm_inst,
				    oh->clkdm->clkdm_offs,
				    oh->prcm.omap4.clkctrl_offs);

	v = _omap4_wait_target_disable(oh);
	if (v)
		pr_warn("omap_hwmod: %s: _wait_target_disable failed\n",
			oh->name);

	return 0;
}

static int _count_mpu_irqs(struct omap_hwmod *oh)
{
	struct omap_hwmod_irq_info *ohii;
	int i = 0;

	if (!oh || !oh->mpu_irqs)
		return 0;

	do {
		ohii = &oh->mpu_irqs[i++];
	} while (ohii->irq != -1);

	return i-1;
}

static int _count_sdma_reqs(struct omap_hwmod *oh)
{
	struct omap_hwmod_dma_info *ohdi;
	int i = 0;

	if (!oh || !oh->sdma_reqs)
		return 0;

	do {
		ohdi = &oh->sdma_reqs[i++];
	} while (ohdi->dma_req != -1);

	return i-1;
}

static int _count_ocp_if_addr_spaces(struct omap_hwmod_ocp_if *os)
{
	struct omap_hwmod_addr_space *mem;
	int i = 0;

	if (!os || !os->addr)
		return 0;

	do {
		mem = &os->addr[i++];
	} while (mem->pa_start != mem->pa_end);

	return i-1;
}

static int __init _find_mpu_port_index(struct omap_hwmod *oh)
{
	int i;
	int found = 0;

	if (!oh || oh->slaves_cnt == 0)
		return -EINVAL;

	for (i = 0; i < oh->slaves_cnt; i++) {
		struct omap_hwmod_ocp_if *os = oh->slaves[i];

		if (os->user & OCP_USER_MPU) {
			found = 1;
			break;
		}
	}

	if (found)
		pr_debug("omap_hwmod: %s: MPU OCP slave port ID  %d\n",
			 oh->name, i);
	else
		pr_debug("omap_hwmod: %s: no MPU OCP slave port found\n",
			 oh->name);

	return (found) ? i : -EINVAL;
}

static void __iomem * __init _find_mpu_rt_base(struct omap_hwmod *oh, u8 index)
{
	struct omap_hwmod_ocp_if *os;
	struct omap_hwmod_addr_space *mem;
	int i = 0, found = 0;
	void __iomem *va_start;

	if (!oh || oh->slaves_cnt == 0)
		return NULL;

	os = oh->slaves[index];

	if (!os->addr)
		return NULL;

	do {
		mem = &os->addr[i++];
		if (mem->flags & ADDR_TYPE_RT)
			found = 1;
	} while (!found && mem->pa_start != mem->pa_end);

	if (found) {
		va_start = ioremap(mem->pa_start, mem->pa_end - mem->pa_start);
		if (!va_start) {
			pr_err("omap_hwmod: %s: Could not ioremap\n", oh->name);
			return NULL;
		}
		pr_debug("omap_hwmod: %s: MPU register target at va %p\n",
			 oh->name, va_start);
	} else {
		pr_debug("omap_hwmod: %s: no MPU register target found\n",
			 oh->name);
	}

	return (found) ? va_start : NULL;
}

static void _enable_sysc(struct omap_hwmod *oh)
{
	u8 idlemode, sf;
	u32 v;

	if (!oh->class->sysc)
		return;

	v = oh->_sysc_cache;
	sf = oh->class->sysc->sysc_flags;

	if (sf & SYSC_HAS_SIDLEMODE) {
		idlemode = (oh->flags & HWMOD_SWSUP_SIDLE) ?
			HWMOD_IDLEMODE_NO : HWMOD_IDLEMODE_SMART;
		_set_slave_idlemode(oh, idlemode, &v);
	}

	if (sf & SYSC_HAS_MIDLEMODE) {
		if (oh->flags & HWMOD_SWSUP_MSTANDBY) {
			idlemode = HWMOD_IDLEMODE_NO;
		} else {
			if (sf & SYSC_HAS_ENAWAKEUP)
				_enable_wakeup(oh, &v);
			if (oh->class->sysc->idlemodes & MSTANDBY_SMART_WKUP)
				idlemode = HWMOD_IDLEMODE_SMART_WKUP;
			else
				idlemode = HWMOD_IDLEMODE_SMART;
		}
		_set_master_standbymode(oh, idlemode, &v);
	}

	if ((oh->flags & HWMOD_SET_DEFAULT_CLOCKACT) &&
	    (sf & SYSC_HAS_CLOCKACTIVITY))
		_set_clockactivity(oh, oh->class->sysc->clockact, &v);

	
	if ((sf & SYSC_HAS_SIDLEMODE) && !(oh->flags & HWMOD_SWSUP_SIDLE))
		_enable_wakeup(oh, &v);

	_write_sysconfig(v, oh);

	if (sf & SYSC_HAS_AUTOIDLE) {
		idlemode = (oh->flags & HWMOD_NO_OCP_AUTOIDLE) ?
			0 : 1;
		_set_module_autoidle(oh, idlemode, &v);
		_write_sysconfig(v, oh);
	}
}

static void _idle_sysc(struct omap_hwmod *oh)
{
	u8 idlemode, sf;
	u32 v;

	if (!oh->class->sysc)
		return;

	v = oh->_sysc_cache;
	sf = oh->class->sysc->sysc_flags;

	if (sf & SYSC_HAS_SIDLEMODE) {
		idlemode = (oh->flags & HWMOD_SWSUP_SIDLE) ?
			HWMOD_IDLEMODE_FORCE : HWMOD_IDLEMODE_SMART;
		_set_slave_idlemode(oh, idlemode, &v);
	}

	if (sf & SYSC_HAS_MIDLEMODE) {
		if (oh->flags & HWMOD_SWSUP_MSTANDBY) {
			idlemode = HWMOD_IDLEMODE_FORCE;
		} else {
			if (sf & SYSC_HAS_ENAWAKEUP)
				_enable_wakeup(oh, &v);
			if (oh->class->sysc->idlemodes & MSTANDBY_SMART_WKUP)
				idlemode = HWMOD_IDLEMODE_SMART_WKUP;
			else
				idlemode = HWMOD_IDLEMODE_SMART;
		}
		_set_master_standbymode(oh, idlemode, &v);
	}

	
	if ((sf & SYSC_HAS_SIDLEMODE) && !(oh->flags & HWMOD_SWSUP_SIDLE))
		_enable_wakeup(oh, &v);

	_write_sysconfig(v, oh);
}

static void _shutdown_sysc(struct omap_hwmod *oh)
{
	u32 v;
	u8 sf;

	if (!oh->class->sysc)
		return;

	v = oh->_sysc_cache;
	sf = oh->class->sysc->sysc_flags;

	if (sf & SYSC_HAS_SIDLEMODE)
		_set_slave_idlemode(oh, HWMOD_IDLEMODE_FORCE, &v);

	if (sf & SYSC_HAS_MIDLEMODE)
		_set_master_standbymode(oh, HWMOD_IDLEMODE_FORCE, &v);

	if (sf & SYSC_HAS_AUTOIDLE)
		_set_module_autoidle(oh, 1, &v);

	_write_sysconfig(v, oh);
}

static struct omap_hwmod *_lookup(const char *name)
{
	struct omap_hwmod *oh, *temp_oh;

	oh = NULL;

	list_for_each_entry(temp_oh, &omap_hwmod_list, node) {
		if (!strcmp(name, temp_oh->name)) {
			oh = temp_oh;
			break;
		}
	}

	return oh;
}
static int _init_clkdm(struct omap_hwmod *oh)
{
	if (cpu_is_omap24xx() || cpu_is_omap34xx())
		return 0;

	if (!oh->clkdm_name) {
		pr_warning("omap_hwmod: %s: no clkdm_name\n", oh->name);
		return -EINVAL;
	}

	oh->clkdm = clkdm_lookup(oh->clkdm_name);
	if (!oh->clkdm) {
		pr_warning("omap_hwmod: %s: could not associate to clkdm %s\n",
			oh->name, oh->clkdm_name);
		return -EINVAL;
	}

	pr_debug("omap_hwmod: %s: associated to clkdm %s\n",
		oh->name, oh->clkdm_name);

	return 0;
}

static int _init_clocks(struct omap_hwmod *oh, void *data)
{
	int ret = 0;

	if (oh->_state != _HWMOD_STATE_REGISTERED)
		return 0;

	pr_debug("omap_hwmod: %s: looking up clocks\n", oh->name);

	ret |= _init_main_clk(oh);
	ret |= _init_interface_clks(oh);
	ret |= _init_opt_clks(oh);
	ret |= _init_clkdm(oh);

	if (!ret)
		oh->_state = _HWMOD_STATE_CLKS_INITED;
	else
		pr_warning("omap_hwmod: %s: cannot _init_clocks\n", oh->name);

	return ret;
}

static int _wait_target_ready(struct omap_hwmod *oh)
{
	struct omap_hwmod_ocp_if *os;
	int ret;

	if (!oh)
		return -EINVAL;

	if (oh->_int_flags & _HWMOD_NO_MPU_PORT)
		return 0;

	os = oh->slaves[oh->_mpu_port_index];

	if (oh->flags & HWMOD_NO_IDLEST)
		return 0;

	

	

	if (cpu_is_omap24xx() || cpu_is_omap34xx()) {
		ret = omap2_cm_wait_module_ready(oh->prcm.omap2.module_offs,
						 oh->prcm.omap2.idlest_reg_id,
						 oh->prcm.omap2.idlest_idle_bit);
	} else if (cpu_is_omap44xx()) {
		if (!oh->clkdm)
			return -EINVAL;

		ret = omap4_cminst_wait_module_ready(oh->clkdm->prcm_partition,
						     oh->clkdm->cm_inst,
						     oh->clkdm->clkdm_offs,
						     oh->prcm.omap4.clkctrl_offs);
	} else {
		BUG();
	};

	return ret;
}

static u8 _lookup_hardreset(struct omap_hwmod *oh, const char *name,
			    struct omap_hwmod_rst_info *ohri)
{
	int i;

	for (i = 0; i < oh->rst_lines_cnt; i++) {
		const char *rst_line = oh->rst_lines[i].name;
		if (!strcmp(rst_line, name)) {
			ohri->rst_shift = oh->rst_lines[i].rst_shift;
			ohri->st_shift = oh->rst_lines[i].st_shift;
			pr_debug("omap_hwmod: %s: %s: %s: rst %d st %d\n",
				 oh->name, __func__, rst_line, ohri->rst_shift,
				 ohri->st_shift);

			return 0;
		}
	}

	return -ENOENT;
}

static int _assert_hardreset(struct omap_hwmod *oh, const char *name)
{
	struct omap_hwmod_rst_info ohri;
	u8 ret;

	if (!oh)
		return -EINVAL;

	ret = _lookup_hardreset(oh, name, &ohri);
	if (IS_ERR_VALUE(ret))
		return ret;

	if (cpu_is_omap24xx() || cpu_is_omap34xx())
		return omap2_prm_assert_hardreset(oh->prcm.omap2.module_offs,
						  ohri.rst_shift);
	else if (cpu_is_omap44xx())
		return omap4_prminst_assert_hardreset(ohri.rst_shift,
				  oh->clkdm->pwrdm.ptr->prcm_partition,
				  oh->clkdm->pwrdm.ptr->prcm_offs,
				  oh->prcm.omap4.rstctrl_offs);
	else
		return -EINVAL;
}

static int _deassert_hardreset(struct omap_hwmod *oh, const char *name)
{
	struct omap_hwmod_rst_info ohri;
	int ret;

	if (!oh)
		return -EINVAL;

	ret = _lookup_hardreset(oh, name, &ohri);
	if (IS_ERR_VALUE(ret))
		return ret;

	if (cpu_is_omap24xx() || cpu_is_omap34xx()) {
		ret = omap2_prm_deassert_hardreset(oh->prcm.omap2.module_offs,
						   ohri.rst_shift,
						   ohri.st_shift);
	} else if (cpu_is_omap44xx()) {
		if (ohri.st_shift)
			pr_err("omap_hwmod: %s: %s: hwmod data error: OMAP4 does not support st_shift\n",
			       oh->name, name);
		ret = omap4_prminst_deassert_hardreset(ohri.rst_shift,
				  oh->clkdm->pwrdm.ptr->prcm_partition,
				  oh->clkdm->pwrdm.ptr->prcm_offs,
				  oh->prcm.omap4.rstctrl_offs);
	} else {
		return -EINVAL;
	}

	if (ret == -EBUSY)
		pr_warning("omap_hwmod: %s: failed to hardreset\n", oh->name);

	return ret;
}

static int _read_hardreset(struct omap_hwmod *oh, const char *name)
{
	struct omap_hwmod_rst_info ohri;
	u8 ret;

	if (!oh)
		return -EINVAL;

	ret = _lookup_hardreset(oh, name, &ohri);
	if (IS_ERR_VALUE(ret))
		return ret;

	if (cpu_is_omap24xx() || cpu_is_omap34xx()) {
		return omap2_prm_is_hardreset_asserted(oh->prcm.omap2.module_offs,
						       ohri.st_shift);
	} else if (cpu_is_omap44xx()) {
		return omap4_prminst_is_hardreset_asserted(ohri.rst_shift,
				  oh->clkdm->pwrdm.ptr->prcm_partition,
				  oh->clkdm->pwrdm.ptr->prcm_offs,
				  oh->prcm.omap4.rstctrl_offs);
	} else {
		return -EINVAL;
	}
}

static int _ocp_softreset(struct omap_hwmod *oh)
{
	u32 v, softrst_mask;
	int c = 0;
	int ret = 0;

	if (!oh->class->sysc ||
	    !(oh->class->sysc->sysc_flags & SYSC_HAS_SOFTRESET))
		return -EINVAL;

	
	if (oh->_state != _HWMOD_STATE_ENABLED) {
		pr_warning("omap_hwmod: %s: reset can only be entered from "
			   "enabled state\n", oh->name);
		return -EINVAL;
	}

	
	if (oh->flags & HWMOD_CONTROL_OPT_CLKS_IN_RESET)
		_enable_optional_clocks(oh);

	pr_debug("omap_hwmod: %s: resetting via OCP SOFTRESET\n", oh->name);

	v = oh->_sysc_cache;
	ret = _set_softreset(oh, &v);
	if (ret)
		goto dis_opt_clks;
	_write_sysconfig(v, oh);

	if (oh->class->sysc->srst_udelay)
		udelay(oh->class->sysc->srst_udelay);

	if (oh->class->sysc->sysc_flags & SYSS_HAS_RESET_STATUS)
		omap_test_timeout((omap_hwmod_read(oh,
						    oh->class->sysc->syss_offs)
				   & SYSS_RESETDONE_MASK),
				  MAX_MODULE_SOFTRESET_WAIT, c);
	else if (oh->class->sysc->sysc_flags & SYSC_HAS_RESET_STATUS) {
		softrst_mask = (0x1 << oh->class->sysc->sysc_fields->srst_shift);
		omap_test_timeout(!(omap_hwmod_read(oh,
						     oh->class->sysc->sysc_offs)
				   & softrst_mask),
				  MAX_MODULE_SOFTRESET_WAIT, c);
	}

	if (c == MAX_MODULE_SOFTRESET_WAIT)
		pr_warning("omap_hwmod: %s: softreset failed (waited %d usec)\n",
			   oh->name, MAX_MODULE_SOFTRESET_WAIT);
	else
		pr_debug("omap_hwmod: %s: softreset in %d usec\n", oh->name, c);


	ret = (c == MAX_MODULE_SOFTRESET_WAIT) ? -ETIMEDOUT : 0;

dis_opt_clks:
	if (oh->flags & HWMOD_CONTROL_OPT_CLKS_IN_RESET)
		_disable_optional_clocks(oh);

	return ret;
}

static int _reset(struct omap_hwmod *oh)
{
	int ret;

	pr_debug("omap_hwmod: %s: resetting\n", oh->name);

	ret = (oh->class->reset) ? oh->class->reset(oh) : _ocp_softreset(oh);

	if (oh->class->sysc) {
		_update_sysc_cache(oh);
		_enable_sysc(oh);
	}

	return ret;
}

static int _enable(struct omap_hwmod *oh)
{
	int r;
	int hwsup = 0;

	pr_debug("omap_hwmod: %s: enabling\n", oh->name);

	if (oh->_int_flags & _HWMOD_SKIP_ENABLE) {
		if (oh->mux)
			omap_hwmod_mux(oh->mux, _HWMOD_STATE_ENABLED);

		oh->_int_flags &= ~_HWMOD_SKIP_ENABLE;
		return 0;
	}

	if (oh->_state != _HWMOD_STATE_INITIALIZED &&
	    oh->_state != _HWMOD_STATE_IDLE &&
	    oh->_state != _HWMOD_STATE_DISABLED) {
		WARN(1, "omap_hwmod: %s: enabled state can only be entered from initialized, idle, or disabled state\n",
			oh->name);
		return -EINVAL;
	}


	if ((oh->_state == _HWMOD_STATE_INITIALIZED ||
	     oh->_state == _HWMOD_STATE_DISABLED) && oh->rst_lines_cnt == 1)
		_deassert_hardreset(oh, oh->rst_lines[0].name);

	
	if (oh->mux && (!oh->mux->enabled ||
			((oh->_state == _HWMOD_STATE_IDLE) &&
			 oh->mux->pads_dynamic)))
		omap_hwmod_mux(oh->mux, _HWMOD_STATE_ENABLED);

	_add_initiator_dep(oh, mpu_oh);

	if (oh->clkdm) {
		hwsup = clkdm_in_hwsup(oh->clkdm);
		r = clkdm_hwmod_enable(oh->clkdm, oh);
		if (r) {
			WARN(1, "omap_hwmod: %s: could not enable clockdomain %s: %d\n",
			     oh->name, oh->clkdm->name, r);
			return r;
		}
	}

	_enable_clocks(oh);
	_enable_module(oh);

	r = _wait_target_ready(oh);
	if (!r) {
		if (oh->clkdm && hwsup)
			clkdm_allow_idle(oh->clkdm);

		oh->_state = _HWMOD_STATE_ENABLED;

		
		if (oh->class->sysc) {
			if (!(oh->_int_flags & _HWMOD_SYSCONFIG_LOADED))
				_update_sysc_cache(oh);
			_enable_sysc(oh);
		}
	} else {
		_disable_clocks(oh);
		pr_debug("omap_hwmod: %s: _wait_target_ready: %d\n",
			 oh->name, r);

		if (oh->clkdm)
			clkdm_hwmod_disable(oh->clkdm, oh);
	}

	return r;
}

static int _idle(struct omap_hwmod *oh)
{
	pr_debug("omap_hwmod: %s: idling\n", oh->name);

	if (oh->_state != _HWMOD_STATE_ENABLED) {
		WARN(1, "omap_hwmod: %s: idle state can only be entered from enabled state\n",
			oh->name);
		return -EINVAL;
	}

	if (oh->class->sysc)
		_idle_sysc(oh);
	_del_initiator_dep(oh, mpu_oh);

	_omap4_disable_module(oh);

	_disable_clocks(oh);
	if (oh->clkdm)
		clkdm_hwmod_disable(oh->clkdm, oh);

	
	if (oh->mux && oh->mux->pads_dynamic)
		omap_hwmod_mux(oh->mux, _HWMOD_STATE_IDLE);

	oh->_state = _HWMOD_STATE_IDLE;

	return 0;
}

int omap_hwmod_set_ocp_autoidle(struct omap_hwmod *oh, u8 autoidle)
{
	u32 v;
	int retval = 0;
	unsigned long flags;

	if (!oh || oh->_state != _HWMOD_STATE_ENABLED)
		return -EINVAL;

	spin_lock_irqsave(&oh->_lock, flags);

	v = oh->_sysc_cache;

	retval = _set_module_autoidle(oh, autoidle, &v);

	if (!retval)
		_write_sysconfig(v, oh);

	spin_unlock_irqrestore(&oh->_lock, flags);

	return retval;
}

static int _shutdown(struct omap_hwmod *oh)
{
	int ret;
	u8 prev_state;

	if (oh->_state != _HWMOD_STATE_IDLE &&
	    oh->_state != _HWMOD_STATE_ENABLED) {
		WARN(1, "omap_hwmod: %s: disabled state can only be entered from idle, or enabled state\n",
			oh->name);
		return -EINVAL;
	}

	pr_debug("omap_hwmod: %s: disabling\n", oh->name);

	if (oh->class->pre_shutdown) {
		prev_state = oh->_state;
		if (oh->_state == _HWMOD_STATE_IDLE)
			_enable(oh);
		ret = oh->class->pre_shutdown(oh);
		if (ret) {
			if (prev_state == _HWMOD_STATE_IDLE)
				_idle(oh);
			return ret;
		}
	}

	if (oh->class->sysc) {
		if (oh->_state == _HWMOD_STATE_IDLE)
			_enable(oh);
		_shutdown_sysc(oh);
	}

	
	if (oh->_state == _HWMOD_STATE_ENABLED) {
		_del_initiator_dep(oh, mpu_oh);
		
		_omap4_disable_module(oh);
		_disable_clocks(oh);
		if (oh->clkdm)
			clkdm_hwmod_disable(oh->clkdm, oh);
	}
	

	if (oh->rst_lines_cnt == 1)
		_assert_hardreset(oh, oh->rst_lines[0].name);

	
	if (oh->mux)
		omap_hwmod_mux(oh->mux, _HWMOD_STATE_DISABLED);

	oh->_state = _HWMOD_STATE_DISABLED;

	return 0;
}

static int _setup(struct omap_hwmod *oh, void *data)
{
	int i, r;
	u8 postsetup_state;

	if (oh->_state != _HWMOD_STATE_CLKS_INITED)
		return 0;

	
	if (oh->slaves_cnt > 0) {
		for (i = 0; i < oh->slaves_cnt; i++) {
			struct omap_hwmod_ocp_if *os = oh->slaves[i];
			struct clk *c = os->_clk;

			if (!c)
				continue;

			if (os->flags & OCPIF_SWSUP_IDLE) {
				
			} else {
				
				clk_enable(c);
			}
		}
	}

	oh->_state = _HWMOD_STATE_INITIALIZED;

	if ((oh->flags & HWMOD_INIT_NO_RESET) && oh->rst_lines_cnt == 1)
		return 0;

	r = _enable(oh);
	if (r) {
		pr_warning("omap_hwmod: %s: cannot be enabled (%d)\n",
			   oh->name, oh->_state);
		return 0;
	}

	if (!(oh->flags & HWMOD_INIT_NO_RESET))
		_reset(oh);

	postsetup_state = oh->_postsetup_state;
	if (postsetup_state == _HWMOD_STATE_UNKNOWN)
		postsetup_state = _HWMOD_STATE_ENABLED;

	if ((oh->flags & HWMOD_INIT_NO_IDLE) &&
	    (postsetup_state == _HWMOD_STATE_IDLE)) {
		oh->_int_flags |= _HWMOD_SKIP_ENABLE;
		postsetup_state = _HWMOD_STATE_ENABLED;
	}

	if (postsetup_state == _HWMOD_STATE_IDLE)
		_idle(oh);
	else if (postsetup_state == _HWMOD_STATE_DISABLED)
		_shutdown(oh);
	else if (postsetup_state != _HWMOD_STATE_ENABLED)
		WARN(1, "hwmod: %s: unknown postsetup state %d! defaulting to enabled\n",
		     oh->name, postsetup_state);

	return 0;
}

static int __init _register(struct omap_hwmod *oh)
{
	int ms_id;

	if (!oh || !oh->name || !oh->class || !oh->class->name ||
	    (oh->_state != _HWMOD_STATE_UNKNOWN))
		return -EINVAL;

	pr_debug("omap_hwmod: %s: registering\n", oh->name);

	if (_lookup(oh->name))
		return -EEXIST;

	ms_id = _find_mpu_port_index(oh);
	if (!IS_ERR_VALUE(ms_id))
		oh->_mpu_port_index = ms_id;
	else
		oh->_int_flags |= _HWMOD_NO_MPU_PORT;

	list_add_tail(&oh->node, &omap_hwmod_list);

	spin_lock_init(&oh->_lock);

	oh->_state = _HWMOD_STATE_REGISTERED;

	if (!strcmp(oh->name, MPU_INITIATOR_NAME))
		mpu_oh = oh;

	return 0;
}



u32 omap_hwmod_read(struct omap_hwmod *oh, u16 reg_offs)
{
	if (oh->flags & HWMOD_16BIT_REG)
		return __raw_readw(oh->_mpu_rt_va + reg_offs);
	else
		return __raw_readl(oh->_mpu_rt_va + reg_offs);
}

void omap_hwmod_write(u32 v, struct omap_hwmod *oh, u16 reg_offs)
{
	if (oh->flags & HWMOD_16BIT_REG)
		__raw_writew(v, oh->_mpu_rt_va + reg_offs);
	else
		__raw_writel(v, oh->_mpu_rt_va + reg_offs);
}

int omap_hwmod_softreset(struct omap_hwmod *oh)
{
	u32 v;
	int ret;

	if (!oh || !(oh->_sysc_cache))
		return -EINVAL;

	v = oh->_sysc_cache;
	ret = _set_softreset(oh, &v);
	if (ret)
		goto error;
	_write_sysconfig(v, oh);

error:
	return ret;
}

int omap_hwmod_set_slave_idlemode(struct omap_hwmod *oh, u8 idlemode)
{
	u32 v;
	int retval = 0;

	if (!oh)
		return -EINVAL;

	v = oh->_sysc_cache;

	retval = _set_slave_idlemode(oh, idlemode, &v);
	if (!retval)
		_write_sysconfig(v, oh);

	return retval;
}

struct omap_hwmod *omap_hwmod_lookup(const char *name)
{
	struct omap_hwmod *oh;

	if (!name)
		return NULL;

	oh = _lookup(name);

	return oh;
}

int omap_hwmod_for_each(int (*fn)(struct omap_hwmod *oh, void *data),
			void *data)
{
	struct omap_hwmod *temp_oh;
	int ret = 0;

	if (!fn)
		return -EINVAL;

	list_for_each_entry(temp_oh, &omap_hwmod_list, node) {
		ret = (*fn)(temp_oh, data);
		if (ret)
			break;
	}

	return ret;
}

int __init omap_hwmod_register(struct omap_hwmod **ohs)
{
	int r, i;

	if (!ohs)
		return 0;

	i = 0;
	do {
		r = _register(ohs[i]);
		WARN(r, "omap_hwmod: %s: _register returned %d\n", ohs[i]->name,
		     r);
	} while (ohs[++i]);

	return 0;
}

static int __init _populate_mpu_rt_base(struct omap_hwmod *oh, void *data)
{
	if (oh->_state != _HWMOD_STATE_REGISTERED)
		return 0;

	if (oh->_int_flags & _HWMOD_NO_MPU_PORT)
		return 0;

	oh->_mpu_rt_va = _find_mpu_rt_base(oh, oh->_mpu_port_index);

	return 0;
}

int __init omap_hwmod_setup_one(const char *oh_name)
{
	struct omap_hwmod *oh;
	int r;

	pr_debug("omap_hwmod: %s: %s\n", oh_name, __func__);

	if (!mpu_oh) {
		pr_err("omap_hwmod: %s: cannot setup_one: MPU initiator hwmod %s not yet registered\n",
		       oh_name, MPU_INITIATOR_NAME);
		return -EINVAL;
	}

	oh = _lookup(oh_name);
	if (!oh) {
		WARN(1, "omap_hwmod: %s: hwmod not yet registered\n", oh_name);
		return -EINVAL;
	}

	if (mpu_oh->_state == _HWMOD_STATE_REGISTERED && oh != mpu_oh)
		omap_hwmod_setup_one(MPU_INITIATOR_NAME);

	r = _populate_mpu_rt_base(oh, NULL);
	if (IS_ERR_VALUE(r)) {
		WARN(1, "omap_hwmod: %s: couldn't set mpu_rt_base\n", oh_name);
		return -EINVAL;
	}

	r = _init_clocks(oh, NULL);
	if (IS_ERR_VALUE(r)) {
		WARN(1, "omap_hwmod: %s: couldn't init clocks\n", oh_name);
		return -EINVAL;
	}

	_setup(oh, NULL);

	return 0;
}

static int __init omap_hwmod_setup_all(void)
{
	int r;

	if (!mpu_oh) {
		pr_err("omap_hwmod: %s: MPU initiator hwmod %s not yet registered\n",
		       __func__, MPU_INITIATOR_NAME);
		return -EINVAL;
	}

	r = omap_hwmod_for_each(_populate_mpu_rt_base, NULL);

	r = omap_hwmod_for_each(_init_clocks, NULL);
	WARN(IS_ERR_VALUE(r),
	     "omap_hwmod: %s: _init_clocks failed\n", __func__);

	omap_hwmod_for_each(_setup, NULL);

	return 0;
}
core_initcall(omap_hwmod_setup_all);

int omap_hwmod_enable(struct omap_hwmod *oh)
{
	int r;
	unsigned long flags;

	if (!oh)
		return -EINVAL;

	spin_lock_irqsave(&oh->_lock, flags);
	r = _enable(oh);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return r;
}

int omap_hwmod_idle(struct omap_hwmod *oh)
{
	unsigned long flags;

	if (!oh)
		return -EINVAL;

	spin_lock_irqsave(&oh->_lock, flags);
	_idle(oh);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return 0;
}

int omap_hwmod_shutdown(struct omap_hwmod *oh)
{
	unsigned long flags;

	if (!oh)
		return -EINVAL;

	spin_lock_irqsave(&oh->_lock, flags);
	_shutdown(oh);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return 0;
}

int omap_hwmod_enable_clocks(struct omap_hwmod *oh)
{
	unsigned long flags;

	spin_lock_irqsave(&oh->_lock, flags);
	_enable_clocks(oh);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return 0;
}

int omap_hwmod_disable_clocks(struct omap_hwmod *oh)
{
	unsigned long flags;

	spin_lock_irqsave(&oh->_lock, flags);
	_disable_clocks(oh);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return 0;
}

void omap_hwmod_ocp_barrier(struct omap_hwmod *oh)
{
	BUG_ON(!oh);

	if (!oh->class->sysc || !oh->class->sysc->sysc_flags) {
		WARN(1, "omap_device: %s: OCP barrier impossible due to device configuration\n",
			oh->name);
		return;
	}

	omap_hwmod_read(oh, oh->class->sysc->sysc_offs);
}

int omap_hwmod_reset(struct omap_hwmod *oh)
{
	int r;
	unsigned long flags;

	if (!oh)
		return -EINVAL;

	spin_lock_irqsave(&oh->_lock, flags);
	r = _reset(oh);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return r;
}

int omap_hwmod_count_resources(struct omap_hwmod *oh)
{
	int ret, i;

	ret = _count_mpu_irqs(oh) + _count_sdma_reqs(oh);

	for (i = 0; i < oh->slaves_cnt; i++)
		ret += _count_ocp_if_addr_spaces(oh->slaves[i]);

	return ret;
}

int omap_hwmod_fill_resources(struct omap_hwmod *oh, struct resource *res)
{
	int i, j, mpu_irqs_cnt, sdma_reqs_cnt;
	int r = 0;

	

	mpu_irqs_cnt = _count_mpu_irqs(oh);
	for (i = 0; i < mpu_irqs_cnt; i++) {
		(res + r)->name = (oh->mpu_irqs + i)->name;
		(res + r)->start = (oh->mpu_irqs + i)->irq;
		(res + r)->end = (oh->mpu_irqs + i)->irq;
		(res + r)->flags = IORESOURCE_IRQ;
		r++;
	}

	sdma_reqs_cnt = _count_sdma_reqs(oh);
	for (i = 0; i < sdma_reqs_cnt; i++) {
		(res + r)->name = (oh->sdma_reqs + i)->name;
		(res + r)->start = (oh->sdma_reqs + i)->dma_req;
		(res + r)->end = (oh->sdma_reqs + i)->dma_req;
		(res + r)->flags = IORESOURCE_DMA;
		r++;
	}

	for (i = 0; i < oh->slaves_cnt; i++) {
		struct omap_hwmod_ocp_if *os;
		int addr_cnt;

		os = oh->slaves[i];
		addr_cnt = _count_ocp_if_addr_spaces(os);

		for (j = 0; j < addr_cnt; j++) {
			(res + r)->name = (os->addr + j)->name;
			(res + r)->start = (os->addr + j)->pa_start;
			(res + r)->end = (os->addr + j)->pa_end;
			(res + r)->flags = IORESOURCE_MEM;
			r++;
		}
	}

	return r;
}

struct powerdomain *omap_hwmod_get_pwrdm(struct omap_hwmod *oh)
{
	struct clk *c;

	if (!oh)
		return NULL;

	if (oh->_clk) {
		c = oh->_clk;
	} else {
		if (oh->_int_flags & _HWMOD_NO_MPU_PORT)
			return NULL;
		c = oh->slaves[oh->_mpu_port_index]->_clk;
	}

	if (!c->clkdm)
		return NULL;

	return c->clkdm->pwrdm.ptr;

}

void __iomem *omap_hwmod_get_mpu_rt_va(struct omap_hwmod *oh)
{
	if (!oh)
		return NULL;

	if (oh->_int_flags & _HWMOD_NO_MPU_PORT)
		return NULL;

	if (oh->_state == _HWMOD_STATE_UNKNOWN)
		return NULL;

	return oh->_mpu_rt_va;
}

int omap_hwmod_add_initiator_dep(struct omap_hwmod *oh,
				 struct omap_hwmod *init_oh)
{
	return _add_initiator_dep(oh, init_oh);
}


int omap_hwmod_del_initiator_dep(struct omap_hwmod *oh,
				 struct omap_hwmod *init_oh)
{
	return _del_initiator_dep(oh, init_oh);
}

int omap_hwmod_enable_wakeup(struct omap_hwmod *oh)
{
	unsigned long flags;
	u32 v;

	spin_lock_irqsave(&oh->_lock, flags);

	if (oh->class->sysc &&
	    (oh->class->sysc->sysc_flags & SYSC_HAS_ENAWAKEUP)) {
		v = oh->_sysc_cache;
		_enable_wakeup(oh, &v);
		_write_sysconfig(v, oh);
	}

	_set_idle_ioring_wakeup(oh, true);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return 0;
}

int omap_hwmod_disable_wakeup(struct omap_hwmod *oh)
{
	unsigned long flags;
	u32 v;

	spin_lock_irqsave(&oh->_lock, flags);

	if (oh->class->sysc &&
	    (oh->class->sysc->sysc_flags & SYSC_HAS_ENAWAKEUP)) {
		v = oh->_sysc_cache;
		_disable_wakeup(oh, &v);
		_write_sysconfig(v, oh);
	}

	_set_idle_ioring_wakeup(oh, false);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return 0;
}

int omap_hwmod_assert_hardreset(struct omap_hwmod *oh, const char *name)
{
	int ret;
	unsigned long flags;

	if (!oh)
		return -EINVAL;

	spin_lock_irqsave(&oh->_lock, flags);
	ret = _assert_hardreset(oh, name);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return ret;
}

int omap_hwmod_deassert_hardreset(struct omap_hwmod *oh, const char *name)
{
	int ret;
	unsigned long flags;

	if (!oh)
		return -EINVAL;

	spin_lock_irqsave(&oh->_lock, flags);
	ret = _deassert_hardreset(oh, name);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return ret;
}

int omap_hwmod_read_hardreset(struct omap_hwmod *oh, const char *name)
{
	int ret;
	unsigned long flags;

	if (!oh)
		return -EINVAL;

	spin_lock_irqsave(&oh->_lock, flags);
	ret = _read_hardreset(oh, name);
	spin_unlock_irqrestore(&oh->_lock, flags);

	return ret;
}


int omap_hwmod_for_each_by_class(const char *classname,
				 int (*fn)(struct omap_hwmod *oh,
					   void *user),
				 void *user)
{
	struct omap_hwmod *temp_oh;
	int ret = 0;

	if (!classname || !fn)
		return -EINVAL;

	pr_debug("omap_hwmod: %s: looking for modules of class %s\n",
		 __func__, classname);

	list_for_each_entry(temp_oh, &omap_hwmod_list, node) {
		if (!strcmp(temp_oh->class->name, classname)) {
			pr_debug("omap_hwmod: %s: %s: calling callback fn\n",
				 __func__, temp_oh->name);
			ret = (*fn)(temp_oh, user);
			if (ret)
				break;
		}
	}

	if (ret)
		pr_debug("omap_hwmod: %s: iterator terminated early: %d\n",
			 __func__, ret);

	return ret;
}

int omap_hwmod_set_postsetup_state(struct omap_hwmod *oh, u8 state)
{
	int ret;
	unsigned long flags;

	if (!oh)
		return -EINVAL;

	if (state != _HWMOD_STATE_DISABLED &&
	    state != _HWMOD_STATE_ENABLED &&
	    state != _HWMOD_STATE_IDLE)
		return -EINVAL;

	spin_lock_irqsave(&oh->_lock, flags);

	if (oh->_state != _HWMOD_STATE_REGISTERED) {
		ret = -EINVAL;
		goto ohsps_unlock;
	}

	oh->_postsetup_state = state;
	ret = 0;

ohsps_unlock:
	spin_unlock_irqrestore(&oh->_lock, flags);

	return ret;
}

int omap_hwmod_get_context_loss_count(struct omap_hwmod *oh)
{
	struct powerdomain *pwrdm;
	int ret = 0;

	pwrdm = omap_hwmod_get_pwrdm(oh);
	if (pwrdm)
		ret = pwrdm_get_context_loss_count(pwrdm);

	return ret;
}

int omap_hwmod_no_setup_reset(struct omap_hwmod *oh)
{
	if (!oh)
		return -EINVAL;

	if (oh->_state != _HWMOD_STATE_REGISTERED) {
		pr_err("omap_hwmod: %s: cannot prevent setup reset; in wrong state\n",
			oh->name);
		return -EINVAL;
	}

	oh->flags |= HWMOD_INIT_NO_RESET;

	return 0;
}

int omap_hwmod_pad_route_irq(struct omap_hwmod *oh, int pad_idx, int irq_idx)
{
	int nr_irqs;

	might_sleep();

	if (!oh || !oh->mux || !oh->mpu_irqs || pad_idx < 0 ||
	    pad_idx >= oh->mux->nr_pads_dynamic)
		return -EINVAL;

	
	for (nr_irqs = 0; oh->mpu_irqs[nr_irqs].irq >= 0; nr_irqs++)
		;

	if (irq_idx >= nr_irqs)
		return -EINVAL;

	if (!oh->mux->irqs) {
		
		oh->mux->irqs = kzalloc(sizeof(int) * oh->mux->nr_pads_dynamic,
			GFP_KERNEL);
		if (!oh->mux->irqs)
			return -ENOMEM;
	}
	oh->mux->irqs[pad_idx] = irq_idx;

	return 0;
}
