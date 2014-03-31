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
#include <linux/notifier.h>
#include <linux/cpufreq.h>
#include <linux/smp.h>

#include "mvp.h"
#include "cpufreq_kernel.h"
#include "mvpkm_kernel.h"
#include "mvp_timer.h"


static uint32
GetCpuFrequency(unsigned int cpu)
{
	unsigned int counterKHZ;

#ifdef CONFIG_CPU_FREQ
	counterKHZ = cpufreq_quick_get(cpu);
#elif defined(MVP_HOST_BOARD_ve)
	KNOWN_BUG(MVP-143);
	counterKHZ = 125e3;

	printk_once(KERN_INFO "mvpkm: CPU_FREQ not available, " \
		    "forcing TSC to %d kHz\n", counterKHZ);
#elif defined(MVP_HOST_BOARD_panda)
	counterKHZ = 1e6;
#else
#error "host TSC frequency unknown."
#endif

	return counterKHZ;
}

static void
TscToRate64(uint32 cpuFreq,
	    struct TscToRate64Cb *ttr)
{
	uint32 shift;
	uint64 mult;


	 
	 cpuFreq *= 1000;

	 
	shift = 31 + CLZ(MVP_TIMER_RATE64) - CLZ(cpuFreq);
	mult = MVP_TIMER_RATE64;
	mult <<= shift;
	do_div(mult, cpuFreq);

	
	ASSERT(mult < (1ULL<<32));

	
	ttr->mult = mult;
	ttr->shift = shift;
}

int
CpuFreqUpdate(unsigned int *freq,
	      struct TscToRate64Cb *ttr)
{
	unsigned int cur = GetCpuFrequency(smp_processor_id());
	int ret = (cur != *freq);

	if (ret) {
		if (cur) {
			TscToRate64(cur, ttr);
		} else {

			ttr->mult = 1;
			ttr->shift = 64;
		}
		*freq = cur;
	}

	return ret;
}

static void
CpuFreqNop(void *info)
{
}

static int
CpuFreqNotifier(struct notifier_block *nb,
		unsigned long val,
		void *data)
{
	struct cpufreq_freqs *freq = data;


	if (freq->old != freq->new &&
	    val == CPUFREQ_POSTCHANGE &&
	    cpumask_test_cpu(freq->cpu, &inMonitor))
		smp_call_function_single(freq->cpu, CpuFreqNop, NULL, false);

	return NOTIFY_OK;
}

static struct notifier_block cpuFreqNotifierBlock = {
	.notifier_call = CpuFreqNotifier
};

void
CpuFreq_Init(void)
{
	int ret;

	
	ret = cpufreq_register_notifier(&cpuFreqNotifierBlock,
					CPUFREQ_TRANSITION_NOTIFIER);
	FATAL_IF(ret < 0);
}

void
CpuFreq_Exit(void)
{
	cpufreq_unregister_notifier(&cpuFreqNotifierBlock,
				    CPUFREQ_TRANSITION_NOTIFIER);
}
