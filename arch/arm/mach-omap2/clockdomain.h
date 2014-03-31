/*
 * arch/arm/plat-omap/include/mach/clockdomain.h
 *
 * OMAP2/3 clockdomain framework functions
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 * Copyright (C) 2008-2011 Nokia Corporation
 *
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_OMAP2_CLOCKDOMAIN_H
#define __ARCH_ARM_MACH_OMAP2_CLOCKDOMAIN_H

#include <linux/init.h>
#include <linux/spinlock.h>

#include "powerdomain.h"
#include <plat/clock.h>
#include <plat/omap_hwmod.h>
#include <plat/cpu.h>

#define CLKDM_CAN_FORCE_SLEEP			(1 << 0)
#define CLKDM_CAN_FORCE_WAKEUP			(1 << 1)
#define CLKDM_CAN_ENABLE_AUTO			(1 << 2)
#define CLKDM_CAN_DISABLE_AUTO			(1 << 3)
#define CLKDM_NO_AUTODEPS			(1 << 4)

#define CLKDM_CAN_HWSUP		(CLKDM_CAN_ENABLE_AUTO | CLKDM_CAN_DISABLE_AUTO)
#define CLKDM_CAN_SWSUP		(CLKDM_CAN_FORCE_SLEEP | CLKDM_CAN_FORCE_WAKEUP)
#define CLKDM_CAN_HWSUP_SWSUP	(CLKDM_CAN_SWSUP | CLKDM_CAN_HWSUP)

struct clkdm_autodep {
	union {
		const char *name;
		struct clockdomain *ptr;
	} clkdm;
};

struct clkdm_dep {
	const char *clkdm_name;
	struct clockdomain *clkdm;
	atomic_t wkdep_usecount;
	atomic_t sleepdep_usecount;
};

#define _CLKDM_FLAG_HWSUP_ENABLED		BIT(0)

struct clockdomain {
	const char *name;
	union {
		const char *name;
		struct powerdomain *ptr;
	} pwrdm;
	const u16 clktrctrl_mask;
	const u8 flags;
	u8 _flags;
	const u8 dep_bit;
	const u8 prcm_partition;
	const s16 cm_inst;
	const u16 clkdm_offs;
	struct clkdm_dep *wkdep_srcs;
	struct clkdm_dep *sleepdep_srcs;
	atomic_t usecount;
	struct list_head node;
	spinlock_t lock;
};

struct clkdm_ops {
	int	(*clkdm_add_wkdep)(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
	int	(*clkdm_del_wkdep)(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
	int	(*clkdm_read_wkdep)(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
	int	(*clkdm_clear_all_wkdeps)(struct clockdomain *clkdm);
	int	(*clkdm_add_sleepdep)(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
	int	(*clkdm_del_sleepdep)(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
	int	(*clkdm_read_sleepdep)(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
	int	(*clkdm_clear_all_sleepdeps)(struct clockdomain *clkdm);
	int	(*clkdm_sleep)(struct clockdomain *clkdm);
	int	(*clkdm_wakeup)(struct clockdomain *clkdm);
	void	(*clkdm_allow_idle)(struct clockdomain *clkdm);
	void	(*clkdm_deny_idle)(struct clockdomain *clkdm);
	int	(*clkdm_clk_enable)(struct clockdomain *clkdm);
	int	(*clkdm_clk_disable)(struct clockdomain *clkdm);
};

int clkdm_register_platform_funcs(struct clkdm_ops *co);
int clkdm_register_autodeps(struct clkdm_autodep *ia);
int clkdm_register_clkdms(struct clockdomain **c);
int clkdm_complete_init(void);

struct clockdomain *clkdm_lookup(const char *name);

int clkdm_for_each(int (*fn)(struct clockdomain *clkdm, void *user),
			void *user);
struct powerdomain *clkdm_get_pwrdm(struct clockdomain *clkdm);

int clkdm_add_wkdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
int clkdm_del_wkdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
int clkdm_read_wkdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
int clkdm_clear_all_wkdeps(struct clockdomain *clkdm);
int clkdm_add_sleepdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
int clkdm_del_sleepdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
int clkdm_read_sleepdep(struct clockdomain *clkdm1, struct clockdomain *clkdm2);
int clkdm_clear_all_sleepdeps(struct clockdomain *clkdm);

void clkdm_allow_idle(struct clockdomain *clkdm);
void clkdm_deny_idle(struct clockdomain *clkdm);
bool clkdm_in_hwsup(struct clockdomain *clkdm);

int clkdm_wakeup(struct clockdomain *clkdm);
int clkdm_sleep(struct clockdomain *clkdm);

int clkdm_clk_enable(struct clockdomain *clkdm, struct clk *clk);
int clkdm_clk_disable(struct clockdomain *clkdm, struct clk *clk);
int clkdm_hwmod_enable(struct clockdomain *clkdm, struct omap_hwmod *oh);
int clkdm_hwmod_disable(struct clockdomain *clkdm, struct omap_hwmod *oh);

extern void __init omap242x_clockdomains_init(void);
extern void __init omap243x_clockdomains_init(void);
extern void __init omap3xxx_clockdomains_init(void);
extern void __init omap44xx_clockdomains_init(void);
extern void _clkdm_add_autodeps(struct clockdomain *clkdm);
extern void _clkdm_del_autodeps(struct clockdomain *clkdm);

extern struct clkdm_ops omap2_clkdm_operations;
extern struct clkdm_ops omap3_clkdm_operations;
extern struct clkdm_ops omap4_clkdm_operations;

extern struct clkdm_dep gfx_24xx_wkdeps[];
extern struct clkdm_dep dsp_24xx_wkdeps[];
extern struct clockdomain wkup_common_clkdm;
extern struct clockdomain prm_common_clkdm;
extern struct clockdomain cm_common_clkdm;

#endif
