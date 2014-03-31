/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <linux/ktime.h>
#include <linux/wakelock.h>
#include <linux/time.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>

#include "xlog.h"

#ifdef CONFIG_MDSS_DUMP_MDP_UNDERRUN

#define XENTRY	256

struct tlog {
	u32 tick;
	const char *name;
	u32 data0;
	u32 data1;
	u32 data2;
	u32 data3;
	u32 data4;
	u32 data5;
};

struct klog {
	struct tlog logs[XENTRY];
	int first;
	int last;
	int init;
} llist;

static spinlock_t xlock;

void xlog(const char *name, u32 data0, u32 data1, u32 data2, u32 data3, u32 data4, u32 data5)
{
	unsigned long flags;
	struct tlog *log;
	ktime_t time;

	if (llist.init == 0) {
		spin_lock_init(&xlock);
		llist.init = 1;
	}

	spin_lock_irqsave(&xlock, flags);

	time = ktime_get();

	log = &llist.logs[llist.first];
	log->tick = (u32)ktime_to_us(time);
	log->name = name;
	log->data0 = data0;
	log->data1 = data1;
	log->data2 = data2;
	log->data3 = data3;
	log->data4 = data4;
	llist.last = llist.first;
	llist.first++;
	llist.first %= XENTRY;

	spin_unlock_irqrestore(&xlock, flags);
}

void xlog_dump(void)
{
	int i, n;
	unsigned long flags;
	struct tlog *log;

	spin_lock_irqsave(&xlock, flags);
	i = llist.first;
	for (n = 0 ; n < XENTRY; n++) {
		log = &llist.logs[i++];
		pr_err("%-32s => %08d: %x %x %x %x %x %x\n", log->name, log->tick, (int)log->data0, (int)log->data1, (int)log->data2, (int)log->data3, (int)log->data4, (int)log->data5);
		i %= XENTRY;
	}
	spin_unlock_irqrestore(&xlock, flags);

}

void xlog_tout_handler(const char *name,  int dump_dsi, int dump_mdp, int dump_mdp_debug_bus, int dead)
{
	if (dump_dsi)
		dsi_dump_reg();

	if (dump_mdp)
		mdp_dump_reg();

	if (dump_mdp_debug_bus)
		mdp_debug_bus();

	xlog(name, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff);
	xlog_dump();

	if(dead)
		panic(name);
}

#endif
