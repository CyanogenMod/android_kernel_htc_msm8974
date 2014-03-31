/*
 * check TSC synchronization.
 *
 * Copyright (C) 2006, Red Hat, Inc., Ingo Molnar
 *
 * We check whether all boot CPUs have their TSC's synchronized,
 * print a warning if not and turn off the TSC clock-source.
 *
 * The warp-check is point-to-point between two CPUs, the CPU
 * initiating the bootup is the 'source CPU', the freshly booting
 * CPU is the 'target CPU'.
 *
 * Only two CPUs may participate - they can enter in any order.
 * ( The serial nature of the boot logic and the CPU hotplug lock
 *   protects against more than 2 CPUs entering this code. )
 */
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/nmi.h>
#include <asm/tsc.h>

static __cpuinitdata atomic_t start_count;
static __cpuinitdata atomic_t stop_count;

static __cpuinitdata arch_spinlock_t sync_lock = __ARCH_SPIN_LOCK_UNLOCKED;

static __cpuinitdata cycles_t last_tsc;
static __cpuinitdata cycles_t max_warp;
static __cpuinitdata int nr_warps;

static __cpuinit void check_tsc_warp(unsigned int timeout)
{
	cycles_t start, now, prev, end;
	int i;

	rdtsc_barrier();
	start = get_cycles();
	rdtsc_barrier();
	end = start + (cycles_t) tsc_khz * timeout;
	now = start;

	for (i = 0; ; i++) {
		arch_spin_lock(&sync_lock);
		prev = last_tsc;
		rdtsc_barrier();
		now = get_cycles();
		rdtsc_barrier();
		last_tsc = now;
		arch_spin_unlock(&sync_lock);

		if (unlikely(!(i & 7))) {
			if (now > end || i > 10000000)
				break;
			cpu_relax();
			touch_nmi_watchdog();
		}
		if (unlikely(prev > now)) {
			arch_spin_lock(&sync_lock);
			max_warp = max(max_warp, prev - now);
			nr_warps++;
			arch_spin_unlock(&sync_lock);
		}
	}
	WARN(!(now-start),
		"Warning: zero tsc calibration delta: %Ld [max: %Ld]\n",
			now-start, end-start);
}

static inline unsigned int loop_timeout(int cpu)
{
	return (cpumask_weight(cpu_core_mask(cpu)) > 1) ? 2 : 20;
}

void __cpuinit check_tsc_sync_source(int cpu)
{
	int cpus = 2;

	if (unsynchronized_tsc())
		return;

	if (tsc_clocksource_reliable) {
		if (cpu == (nr_cpu_ids-1) || system_state != SYSTEM_BOOTING)
			pr_info(
			"Skipped synchronization checks as TSC is reliable.\n");
		return;
	}

	atomic_set(&stop_count, 0);

	while (atomic_read(&start_count) != cpus-1)
		cpu_relax();
	atomic_inc(&start_count);

	check_tsc_warp(loop_timeout(cpu));

	while (atomic_read(&stop_count) != cpus-1)
		cpu_relax();

	if (nr_warps) {
		pr_warning("TSC synchronization [CPU#%d -> CPU#%d]:\n",
			smp_processor_id(), cpu);
		pr_warning("Measured %Ld cycles TSC warp between CPUs, "
			   "turning off TSC clock.\n", max_warp);
		mark_tsc_unstable("check_tsc_sync_source failed");
	} else {
		pr_debug("TSC synchronization [CPU#%d -> CPU#%d]: passed\n",
			smp_processor_id(), cpu);
	}

	atomic_set(&start_count, 0);
	nr_warps = 0;
	max_warp = 0;
	last_tsc = 0;

	atomic_inc(&stop_count);
}

void __cpuinit check_tsc_sync_target(void)
{
	int cpus = 2;

	if (unsynchronized_tsc() || tsc_clocksource_reliable)
		return;

	atomic_inc(&start_count);
	while (atomic_read(&start_count) != cpus)
		cpu_relax();

	check_tsc_warp(loop_timeout(smp_processor_id()));

	atomic_inc(&stop_count);

	while (atomic_read(&stop_count) != cpus)
		cpu_relax();
}
