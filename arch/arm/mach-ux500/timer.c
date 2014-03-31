/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 * Author: Mattias Wallin <mattias.wallin@stericsson.com> for ST-Ericsson
 */
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/clksrc-dbx500-prcmu.h>
#include <linux/of.h>

#include <asm/smp_twd.h>

#include <plat/mtu.h>

#include <mach/setup.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#ifdef CONFIG_HAVE_ARM_TWD
static DEFINE_TWD_LOCAL_TIMER(u5500_twd_local_timer,
			      U5500_TWD_BASE, IRQ_LOCALTIMER);
static DEFINE_TWD_LOCAL_TIMER(u8500_twd_local_timer,
			      U8500_TWD_BASE, IRQ_LOCALTIMER);

static void __init ux500_twd_init(void)
{
	struct twd_local_timer *twd_local_timer;
	int err;

	twd_local_timer = cpu_is_u5500() ? &u5500_twd_local_timer :
					   &u8500_twd_local_timer;

	if (of_have_populated_dt())
		twd_local_timer_of_register();
	else {
		err = twd_local_timer_register(twd_local_timer);
		if (err)
			pr_err("twd_local_timer_register failed %d\n", err);
	}
}
#else
#define ux500_twd_init()	do { } while(0)
#endif

static void __init ux500_timer_init(void)
{
	void __iomem *mtu_timer_base;
	void __iomem *prcmu_timer_base;

	if (cpu_is_u5500()) {
		mtu_timer_base = __io_address(U5500_MTU0_BASE);
		prcmu_timer_base = __io_address(U5500_PRCMU_TIMER_3_BASE);
	} else if (cpu_is_u8500()) {
		mtu_timer_base = __io_address(U8500_MTU0_BASE);
		prcmu_timer_base = __io_address(U8500_PRCMU_TIMER_4_BASE);
	} else {
		ux500_unknown_soc();
	}


	nmdk_timer_init(mtu_timer_base);
	clksrc_dbx500_prcmu_init(prcmu_timer_base);
	ux500_twd_init();
}

static void ux500_timer_reset(void)
{
	nmdk_clkevt_reset();
	nmdk_clksrc_reset();
}

struct sys_timer ux500_timer = {
	.init		= ux500_timer_init,
	.resume		= ux500_timer_reset,
};
