/*
 * Cell Broadband Engine OProfile Support
 *
 * (C) Copyright IBM Corporation 2006
 *
 * Authors: Maynard Johnson <maynardj@us.ibm.com>
 *	    Carl Love <carll@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/hrtimer.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <asm/cell-pmu.h>
#include <asm/time.h>
#include "pr_util.h"

#define SCALE_SHIFT 14

static u32 *samples;

int spu_prof_running;
static unsigned int profiling_interval;

#define NUM_SPU_BITS_TRBUF 16
#define SPUS_PER_TB_ENTRY   4

#define SPU_PC_MASK	     0xFFFF

DEFINE_SPINLOCK(oprof_spu_smpl_arry_lck);
unsigned long oprof_spu_smpl_arry_lck_flags;

void set_spu_profiling_frequency(unsigned int freq_khz, unsigned int cycles_reset)
{
	unsigned long ns_per_cyc;

	if (!freq_khz)
		freq_khz = ppc_proc_freq/1000;


	ns_per_cyc = (USEC_PER_SEC << SCALE_SHIFT)/freq_khz;
	profiling_interval = (ns_per_cyc * cycles_reset) >> SCALE_SHIFT;

}

static void spu_pc_extract(int cpu, int entry)
{
	
	u64 trace_buffer[2];
	u64 spu_mask;
	int spu;

	spu_mask = SPU_PC_MASK;


	cbe_read_trace_buffer(cpu, trace_buffer);

	for (spu = SPUS_PER_TB_ENTRY-1; spu >= 0; spu--) {
		samples[spu * TRACE_ARRAY_SIZE + entry]
			= (spu_mask & trace_buffer[0]) << 2;
		samples[(spu + SPUS_PER_TB_ENTRY) * TRACE_ARRAY_SIZE + entry]
			= (spu_mask & trace_buffer[1]) << 2;

		trace_buffer[0] = trace_buffer[0] >> NUM_SPU_BITS_TRBUF;
		trace_buffer[1] = trace_buffer[1] >> NUM_SPU_BITS_TRBUF;
	}
}

static int cell_spu_pc_collection(int cpu)
{
	u32 trace_addr;
	int entry;

	

	entry = 0;

	trace_addr = cbe_read_pm(cpu, trace_address);
	while (!(trace_addr & CBE_PM_TRACE_BUF_EMPTY)) {
		
		spu_pc_extract(cpu, entry);

		entry++;

		if (entry >= TRACE_ARRAY_SIZE)
			
			break;

		trace_addr = cbe_read_pm(cpu, trace_address);
	}

	return entry;
}


static enum hrtimer_restart profile_spus(struct hrtimer *timer)
{
	ktime_t kt;
	int cpu, node, k, num_samples, spu_num;

	if (!spu_prof_running)
		goto stop;

	for_each_online_cpu(cpu) {
		if (cbe_get_hw_thread_id(cpu))
			continue;

		node = cbe_cpu_to_node(cpu);

		spin_lock_irqsave(&oprof_spu_smpl_arry_lck,
				  oprof_spu_smpl_arry_lck_flags);
		num_samples = cell_spu_pc_collection(cpu);

		if (num_samples == 0) {
			spin_unlock_irqrestore(&oprof_spu_smpl_arry_lck,
					       oprof_spu_smpl_arry_lck_flags);
			continue;
		}

		for (k = 0; k < SPUS_PER_NODE; k++) {
			spu_num = k + (node * SPUS_PER_NODE);
			spu_sync_buffer(spu_num,
					samples + (k * TRACE_ARRAY_SIZE),
					num_samples);
		}

		spin_unlock_irqrestore(&oprof_spu_smpl_arry_lck,
				       oprof_spu_smpl_arry_lck_flags);

	}
	smp_wmb();	/* insure spu event buffer updates are written */
			

	kt = ktime_set(0, profiling_interval);
	if (!spu_prof_running)
		goto stop;
	hrtimer_forward(timer, timer->base->get_time(), kt);
	return HRTIMER_RESTART;

 stop:
	printk(KERN_INFO "SPU_PROF: spu-prof timer ending\n");
	return HRTIMER_NORESTART;
}

static struct hrtimer timer;
int start_spu_profiling_cycles(unsigned int cycles_reset)
{
	ktime_t kt;

	pr_debug("timer resolution: %lu\n", TICK_NSEC);
	kt = ktime_set(0, profiling_interval);
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_set_expires(&timer, kt);
	timer.function = profile_spus;

	
	samples = kzalloc(SPUS_PER_NODE *
			  TRACE_ARRAY_SIZE * sizeof(u32), GFP_KERNEL);

	if (!samples)
		return -ENOMEM;

	spu_prof_running = 1;
	hrtimer_start(&timer, kt, HRTIMER_MODE_REL);
	schedule_delayed_work(&spu_work, DEFAULT_TIMER_EXPIRE);

	return 0;
}

void start_spu_profiling_events(void)
{
	spu_prof_running = 1;
	schedule_delayed_work(&spu_work, DEFAULT_TIMER_EXPIRE);

	return;
}

void stop_spu_profiling_cycles(void)
{
	spu_prof_running = 0;
	hrtimer_cancel(&timer);
	kfree(samples);
	pr_debug("SPU_PROF: stop_spu_profiling_cycles issued\n");
}

void stop_spu_profiling_events(void)
{
	spu_prof_running = 0;
}
