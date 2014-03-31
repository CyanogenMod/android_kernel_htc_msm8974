/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5


#include <linux/module.h>
#include <linux/hrtimer.h>

#include "mvp.h"
#include "mvp_timer.h"
#include "actions.h"
#include "mvpkm_kernel.h"

static enum hrtimer_restart
MonitorTimerCB(struct hrtimer *timer)
{
	struct MvpkmVM *vm = container_of(timer, struct MvpkmVM,
					  monTimer.timer);

	Mvpkm_WakeGuest(vm, ACTION_TIMER);
	return HRTIMER_NORESTART;
}

void
MonitorTimer_Setup(struct MvpkmVM *vm)
{
	struct MonTimer *monTimer = &vm->monTimer;

	monTimer->vm = vm;
	hrtimer_init(&monTimer->timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	monTimer->timer.function = MonitorTimerCB;
}

void
MonitorTimer_Request(struct MonTimer *monTimer,
		     uint64 when64)
{
	if (when64) {
		ktime_t kt;

		kt = ns_to_ktime(when64);
		ASSERT_ON_COMPILE(MVP_TIMER_RATE64 == 1000000000);

		hrtimer_start(&monTimer->timer, kt, HRTIMER_MODE_ABS);
	} else {
		hrtimer_cancel(&monTimer->timer);
	}
}

