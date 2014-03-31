/*
 * coupled.c - helper functions to enter the same idle state on multiple cpus
 *
 * Copyright (c) 2011 Google, Inc.
 *
 * Author: Colin Cross <ccross@android.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "cpuidle.h"

/**
 * DOC: Coupled cpuidle states
 *
 * On some ARM SMP SoCs (OMAP4460, Tegra 2, and probably more), the
 * cpus cannot be independently powered down, either due to
 * sequencing restrictions (on Tegra 2, cpu 0 must be the last to
 * power down), or due to HW bugs (on OMAP4460, a cpu powering up
 * will corrupt the gic state unless the other cpu runs a work
 * around).  Each cpu has a power state that it can enter without
 * coordinating with the other cpu (usually Wait For Interrupt, or
 * WFI), and one or more "coupled" power states that affect blocks
 * shared between the cpus (L2 cache, interrupt controller, and
 * sometimes the whole SoC).  Entering a coupled power state must
 * be tightly controlled on both cpus.
 *
 * This file implements a solution, where each cpu will wait in the
 * WFI state until all cpus are ready to enter a coupled state, at
 * which point the coupled state function will be called on all
 * cpus at approximately the same time.
 *
 * Once all cpus are ready to enter idle, they are woken by an smp
 * cross call.  At this point, there is a chance that one of the
 * cpus will find work to do, and choose not to enter idle.  A
 * final pass is needed to guarantee that all cpus will call the
 * power state enter function at the same time.  During this pass,
 * each cpu will increment the ready counter, and continue once the
 * ready counter matches the number of online coupled cpus.  If any
 * cpu exits idle, the other cpus will decrement their counter and
 * retry.
 *
 * requested_state stores the deepest coupled idle state each cpu
 * is ready for.  It is assumed that the states are indexed from
 * shallowest (highest power, lowest exit latency) to deepest
 * (lowest power, highest exit latency).  The requested_state
 * variable is not locked.  It is only written from the cpu that
 * it stores (or by the on/offlining cpu if that cpu is offline),
 * and only read after all the cpus are ready for the coupled idle
 * state are are no longer updating it.
 *
 * Three atomic counters are used.  alive_count tracks the number
 * of cpus in the coupled set that are currently or soon will be
 * online.  waiting_count tracks the number of cpus that are in
 * the waiting loop, in the ready loop, or in the coupled idle state.
 * ready_count tracks the number of cpus that are in the ready loop
 * or in the coupled idle state.
 *
 * To use coupled cpuidle states, a cpuidle driver must:
 *
 *    Set struct cpuidle_device.coupled_cpus to the mask of all
 *    coupled cpus, usually the same as cpu_possible_mask if all cpus
 *    are part of the same cluster.  The coupled_cpus mask must be
 *    set in the struct cpuidle_device for each cpu.
 *
 *    Set struct cpuidle_device.safe_state to a state that is not a
 *    coupled state.  This is usually WFI.
 *
 *    Set CPUIDLE_FLAG_COUPLED in struct cpuidle_state.flags for each
 *    state that affects multiple cpus.
 *
 *    Provide a struct cpuidle_state.enter function for each state
 *    that affects multiple cpus.  This function is guaranteed to be
 *    called on all cpus at approximately the same time.  The driver
 *    should ensure that the cpus all abort together if any cpu tries
 *    to abort once the function is called.  The function should return
 *    with interrupts still disabled.
 */

struct cpuidle_coupled {
	cpumask_t coupled_cpus;
	int requested_state[NR_CPUS];
	atomic_t ready_waiting_counts;
	int online_count;
	int refcnt;
	int prevent;
};

#define WAITING_BITS 16
#define MAX_WAITING_CPUS (1 << WAITING_BITS)
#define WAITING_MASK (MAX_WAITING_CPUS - 1)
#define READY_MASK (~WAITING_MASK)

#define CPUIDLE_COUPLED_NOT_IDLE	(-1)

static DEFINE_MUTEX(cpuidle_coupled_lock);
static DEFINE_PER_CPU(struct call_single_data, cpuidle_coupled_poke_cb);

static cpumask_t cpuidle_coupled_poked_mask;

void cpuidle_coupled_parallel_barrier(struct cpuidle_device *dev, atomic_t *a)
{
	int n = dev->coupled->online_count;

	smp_mb__before_atomic_inc();
	atomic_inc(a);

	while (atomic_read(a) < n)
		cpu_relax();

	if (atomic_inc_return(a) == n * 2) {
		atomic_set(a, 0);
		return;
	}

	while (atomic_read(a) > n)
		cpu_relax();
}

bool cpuidle_state_is_coupled(struct cpuidle_device *dev,
	struct cpuidle_driver *drv, int state)
{
	return drv->states[state].flags & CPUIDLE_FLAG_COUPLED;
}

static inline void cpuidle_coupled_set_ready(struct cpuidle_coupled *coupled)
{
	atomic_add(MAX_WAITING_CPUS, &coupled->ready_waiting_counts);
}

static
inline int cpuidle_coupled_set_not_ready(struct cpuidle_coupled *coupled)
{
	int all;
	int ret;

	all = coupled->online_count || (coupled->online_count << WAITING_BITS);
	ret = atomic_add_unless(&coupled->ready_waiting_counts,
		-MAX_WAITING_CPUS, all);

	return ret ? 0 : -EINVAL;
}

static inline int cpuidle_coupled_no_cpus_ready(struct cpuidle_coupled *coupled)
{
	int r = atomic_read(&coupled->ready_waiting_counts) >> WAITING_BITS;
	return r == 0;
}

static inline bool cpuidle_coupled_cpus_ready(struct cpuidle_coupled *coupled)
{
	int r = atomic_read(&coupled->ready_waiting_counts) >> WAITING_BITS;
	return r == coupled->online_count;
}

static inline bool cpuidle_coupled_cpus_waiting(struct cpuidle_coupled *coupled)
{
	int w = atomic_read(&coupled->ready_waiting_counts) & WAITING_MASK;
	return w == coupled->online_count;
}

static inline int cpuidle_coupled_no_cpus_waiting(struct cpuidle_coupled *coupled)
{
	int w = atomic_read(&coupled->ready_waiting_counts) & WAITING_MASK;
	return w == 0;
}

static inline int cpuidle_coupled_get_state(struct cpuidle_device *dev,
		struct cpuidle_coupled *coupled)
{
	int i;
	int state = INT_MAX;

	smp_rmb();

	for_each_cpu_mask(i, coupled->coupled_cpus)
		if (cpu_online(i) && coupled->requested_state[i] < state)
			state = coupled->requested_state[i];

	return state;
}

static void cpuidle_coupled_poked(void *info)
{
	int cpu = (unsigned long)info;
	cpumask_clear_cpu(cpu, &cpuidle_coupled_poked_mask);
}

static void cpuidle_coupled_poke(int cpu)
{
	struct call_single_data *csd = &per_cpu(cpuidle_coupled_poke_cb, cpu);

	if (!cpumask_test_and_set_cpu(cpu, &cpuidle_coupled_poked_mask))
		__smp_call_function_single(cpu, csd, 0);
}

static void cpuidle_coupled_poke_others(int this_cpu,
		struct cpuidle_coupled *coupled)
{
	int cpu;

	for_each_cpu_mask(cpu, coupled->coupled_cpus)
		if (cpu != this_cpu && cpu_online(cpu))
			cpuidle_coupled_poke(cpu);
}

static void cpuidle_coupled_set_waiting(int cpu,
		struct cpuidle_coupled *coupled, int next_state)
{
	int w;

	coupled->requested_state[cpu] = next_state;

	w = atomic_inc_return(&coupled->ready_waiting_counts) & WAITING_MASK;
	if (w == coupled->online_count)
		cpuidle_coupled_poke_others(cpu, coupled);
}

static void cpuidle_coupled_set_not_waiting(int cpu,
		struct cpuidle_coupled *coupled)
{
	atomic_dec(&coupled->ready_waiting_counts);

	coupled->requested_state[cpu] = CPUIDLE_COUPLED_NOT_IDLE;
}

static void cpuidle_coupled_set_done(int cpu, struct cpuidle_coupled *coupled)
{
	cpuidle_coupled_set_not_waiting(cpu, coupled);
	atomic_sub(MAX_WAITING_CPUS, &coupled->ready_waiting_counts);
}

static int cpuidle_coupled_clear_pokes(int cpu)
{
	local_irq_enable();
	while (cpumask_test_cpu(cpu, &cpuidle_coupled_poked_mask))
		cpu_relax();
	local_irq_disable();

	return need_resched() ? -EINTR : 0;
}

int cpuidle_enter_state_coupled(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int next_state)
{
	int entered_state = -1;
	struct cpuidle_coupled *coupled = dev->coupled;

	if (!coupled)
		return -EINVAL;

	while (coupled->prevent) {
		if (cpuidle_coupled_clear_pokes(dev->cpu)) {
			local_irq_enable();
			return entered_state;
		}
		entered_state = cpuidle_enter_state(dev, drv,
			dev->safe_state_index);
	}

	
	smp_rmb();

	cpuidle_coupled_set_waiting(dev->cpu, coupled, next_state);

retry:
	while (!cpuidle_coupled_cpus_waiting(coupled)) {
		if (cpuidle_coupled_clear_pokes(dev->cpu)) {
			cpuidle_coupled_set_not_waiting(dev->cpu, coupled);
			goto out;
		}

		if (coupled->prevent) {
			cpuidle_coupled_set_not_waiting(dev->cpu, coupled);
			goto out;
		}

		entered_state = cpuidle_enter_state(dev, drv,
			dev->safe_state_index);
	}

	if (cpuidle_coupled_clear_pokes(dev->cpu)) {
		cpuidle_coupled_set_not_waiting(dev->cpu, coupled);
		goto out;
	}


	cpuidle_coupled_set_ready(coupled);
	while (!cpuidle_coupled_cpus_ready(coupled)) {
		
		if (!cpuidle_coupled_cpus_waiting(coupled))
			if (!cpuidle_coupled_set_not_ready(coupled))
				goto retry;

		cpu_relax();
	}

	
	next_state = cpuidle_coupled_get_state(dev, coupled);

	entered_state = cpuidle_enter_state(dev, drv, next_state);

	cpuidle_coupled_set_done(dev->cpu, coupled);

out:
	local_irq_enable();

	while (!cpuidle_coupled_no_cpus_ready(coupled))
		cpu_relax();

	return entered_state;
}

static void cpuidle_coupled_update_online_cpus(struct cpuidle_coupled *coupled)
{
	cpumask_t cpus;
	cpumask_and(&cpus, cpu_online_mask, &coupled->coupled_cpus);
	coupled->online_count = cpumask_weight(&cpus);
}

int cpuidle_coupled_register_device(struct cpuidle_device *dev)
{
	int cpu;
	struct cpuidle_device *other_dev;
	struct call_single_data *csd;
	struct cpuidle_coupled *coupled;

	if (cpumask_empty(&dev->coupled_cpus))
		return 0;

	for_each_cpu_mask(cpu, dev->coupled_cpus) {
		other_dev = per_cpu(cpuidle_devices, cpu);
		if (other_dev && other_dev->coupled) {
			coupled = other_dev->coupled;
			goto have_coupled;
		}
	}

	
	coupled = kzalloc(sizeof(struct cpuidle_coupled), GFP_KERNEL);
	if (!coupled)
		return -ENOMEM;

	coupled->coupled_cpus = dev->coupled_cpus;

have_coupled:
	dev->coupled = coupled;
	if (WARN_ON(!cpumask_equal(&dev->coupled_cpus, &coupled->coupled_cpus)))
		coupled->prevent++;

	cpuidle_coupled_update_online_cpus(coupled);

	coupled->refcnt++;

	csd = &per_cpu(cpuidle_coupled_poke_cb, dev->cpu);
	csd->func = cpuidle_coupled_poked;
	csd->info = (void *)(unsigned long)dev->cpu;

	return 0;
}

void cpuidle_coupled_unregister_device(struct cpuidle_device *dev)
{
	struct cpuidle_coupled *coupled = dev->coupled;

	if (cpumask_empty(&dev->coupled_cpus))
		return;

	if (--coupled->refcnt)
		kfree(coupled);
	dev->coupled = NULL;
}

static void cpuidle_coupled_prevent_idle(struct cpuidle_coupled *coupled)
{
	int cpu = get_cpu();

	
	coupled->prevent++;
	cpuidle_coupled_poke_others(cpu, coupled);
	put_cpu();
	while (!cpuidle_coupled_no_cpus_waiting(coupled))
		cpu_relax();
}

static void cpuidle_coupled_allow_idle(struct cpuidle_coupled *coupled)
{
	int cpu = get_cpu();

	smp_wmb();
	coupled->prevent--;
	
	cpuidle_coupled_poke_others(cpu, coupled);
	put_cpu();
}

static int cpuidle_coupled_cpu_notify(struct notifier_block *nb,
		unsigned long action, void *hcpu)
{
	int cpu = (unsigned long)hcpu;
	struct cpuidle_device *dev;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_UP_PREPARE:
	case CPU_DOWN_PREPARE:
	case CPU_ONLINE:
	case CPU_DEAD:
	case CPU_UP_CANCELED:
	case CPU_DOWN_FAILED:
		break;
	default:
		return NOTIFY_OK;
	}

	mutex_lock(&cpuidle_lock);

	dev = per_cpu(cpuidle_devices, cpu);
	if (!dev->coupled)
		goto out;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_UP_PREPARE:
	case CPU_DOWN_PREPARE:
		cpuidle_coupled_prevent_idle(dev->coupled);
		break;
	case CPU_ONLINE:
	case CPU_DEAD:
		cpuidle_coupled_update_online_cpus(dev->coupled);
		
	case CPU_UP_CANCELED:
	case CPU_DOWN_FAILED:
		cpuidle_coupled_allow_idle(dev->coupled);
		break;
	}

out:
	mutex_unlock(&cpuidle_lock);
	return NOTIFY_OK;
}

static struct notifier_block cpuidle_coupled_cpu_notifier = {
	.notifier_call = cpuidle_coupled_cpu_notify,
};

static int __init cpuidle_coupled_init(void)
{
	return register_cpu_notifier(&cpuidle_coupled_cpu_notifier);
}
core_initcall(cpuidle_coupled_init);
