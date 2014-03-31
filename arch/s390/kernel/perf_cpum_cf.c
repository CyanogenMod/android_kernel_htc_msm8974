/*
 * Performance event support for s390x - CPU-measurement Counter Facility
 *
 *  Copyright IBM Corp. 2012
 *  Author(s): Hendrik Brueckner <brueckner@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2 only)
 * as published by the Free Software Foundation.
 */
#define KMSG_COMPONENT	"cpum_cf"
#define pr_fmt(fmt)	KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/perf_event.h>
#include <linux/percpu.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/export.h>
#include <asm/ctl_reg.h>
#include <asm/irq.h>
#include <asm/cpu_mf.h>

enum cpumf_ctr_set {
	
	CPUMF_CTR_SET_BASIC   = 0,
	CPUMF_CTR_SET_USER    = 1,
	CPUMF_CTR_SET_CRYPTO  = 2,
	CPUMF_CTR_SET_EXT     = 3,

	
	CPUMF_CTR_SET_MAX,
};

#define CPUMF_LCCTL_ENABLE_SHIFT    16
#define CPUMF_LCCTL_ACTCTL_SHIFT     0
static const u64 cpumf_state_ctl[CPUMF_CTR_SET_MAX] = {
	[CPUMF_CTR_SET_BASIC]	= 0x02,
	[CPUMF_CTR_SET_USER]	= 0x04,
	[CPUMF_CTR_SET_CRYPTO]	= 0x08,
	[CPUMF_CTR_SET_EXT]	= 0x01,
};

static void ctr_set_enable(u64 *state, int ctr_set)
{
	*state |= cpumf_state_ctl[ctr_set] << CPUMF_LCCTL_ENABLE_SHIFT;
}
static void ctr_set_disable(u64 *state, int ctr_set)
{
	*state &= ~(cpumf_state_ctl[ctr_set] << CPUMF_LCCTL_ENABLE_SHIFT);
}
static void ctr_set_start(u64 *state, int ctr_set)
{
	*state |= cpumf_state_ctl[ctr_set] << CPUMF_LCCTL_ACTCTL_SHIFT;
}
static void ctr_set_stop(u64 *state, int ctr_set)
{
	*state &= ~(cpumf_state_ctl[ctr_set] << CPUMF_LCCTL_ACTCTL_SHIFT);
}

struct cpu_hw_events {
	struct cpumf_ctr_info	info;
	atomic_t		ctr_set[CPUMF_CTR_SET_MAX];
	u64			state, tx_state;
	unsigned int		flags;
};
static DEFINE_PER_CPU(struct cpu_hw_events, cpu_hw_events) = {
	.ctr_set = {
		[CPUMF_CTR_SET_BASIC]  = ATOMIC_INIT(0),
		[CPUMF_CTR_SET_USER]   = ATOMIC_INIT(0),
		[CPUMF_CTR_SET_CRYPTO] = ATOMIC_INIT(0),
		[CPUMF_CTR_SET_EXT]    = ATOMIC_INIT(0),
	},
	.state = 0,
	.flags = 0,
};

static int get_counter_set(u64 event)
{
	int set = -1;

	if (event < 32)
		set = CPUMF_CTR_SET_BASIC;
	else if (event < 64)
		set = CPUMF_CTR_SET_USER;
	else if (event < 128)
		set = CPUMF_CTR_SET_CRYPTO;
	else if (event < 160)
		set = CPUMF_CTR_SET_EXT;

	return set;
}

static int validate_event(const struct hw_perf_event *hwc)
{
	switch (hwc->config_base) {
	case CPUMF_CTR_SET_BASIC:
	case CPUMF_CTR_SET_USER:
	case CPUMF_CTR_SET_CRYPTO:
	case CPUMF_CTR_SET_EXT:
		
		if ((hwc->config >=  6 && hwc->config <=  31) ||
		    (hwc->config >= 38 && hwc->config <=  63) ||
		    (hwc->config >= 80 && hwc->config <= 127))
			return -EOPNOTSUPP;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int validate_ctr_version(const struct hw_perf_event *hwc)
{
	struct cpu_hw_events *cpuhw;
	int err = 0;

	cpuhw = &get_cpu_var(cpu_hw_events);

	
	switch (hwc->config_base) {
	case CPUMF_CTR_SET_BASIC:
	case CPUMF_CTR_SET_USER:
		if (cpuhw->info.cfvn < 1)
			err = -EOPNOTSUPP;
		break;
	case CPUMF_CTR_SET_CRYPTO:
	case CPUMF_CTR_SET_EXT:
		if (cpuhw->info.csvn < 1)
			err = -EOPNOTSUPP;
		break;
	}

	put_cpu_var(cpu_hw_events);
	return err;
}

static int validate_ctr_auth(const struct hw_perf_event *hwc)
{
	struct cpu_hw_events *cpuhw;
	u64 ctrs_state;
	int err = 0;

	cpuhw = &get_cpu_var(cpu_hw_events);

	
	ctrs_state = cpumf_state_ctl[hwc->config_base];
	if (!(ctrs_state & cpuhw->info.auth_ctl))
		err = -EPERM;

	put_cpu_var(cpu_hw_events);
	return err;
}

static void cpumf_pmu_enable(struct pmu *pmu)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);
	int err;

	if (cpuhw->flags & PMU_F_ENABLED)
		return;

	err = lcctl(cpuhw->state);
	if (err) {
		pr_err("Enabling the performance measuring unit "
		       "failed with rc=%x\n", err);
		return;
	}

	cpuhw->flags |= PMU_F_ENABLED;
}

static void cpumf_pmu_disable(struct pmu *pmu)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);
	int err;
	u64 inactive;

	if (!(cpuhw->flags & PMU_F_ENABLED))
		return;

	inactive = cpuhw->state & ~((1 << CPUMF_LCCTL_ENABLE_SHIFT) - 1);
	err = lcctl(inactive);
	if (err) {
		pr_err("Disabling the performance measuring unit "
		       "failed with rc=%x\n", err);
		return;
	}

	cpuhw->flags &= ~PMU_F_ENABLED;
}


static atomic_t num_events = ATOMIC_INIT(0);
static DEFINE_MUTEX(pmc_reserve_mutex);

static void cpumf_measurement_alert(struct ext_code ext_code,
				    unsigned int alert, unsigned long unused)
{
	struct cpu_hw_events *cpuhw;

	if (!(alert & CPU_MF_INT_CF_MASK))
		return;

	kstat_cpu(smp_processor_id()).irqs[EXTINT_CPM]++;
	cpuhw = &__get_cpu_var(cpu_hw_events);

	if (!(cpuhw->flags & PMU_F_RESERVED))
		return;

	
	if (alert & CPU_MF_INT_CF_CACA)
		qctri(&cpuhw->info);

	
	if (alert & CPU_MF_INT_CF_LCDA)
		pr_err("CPU[%i] Counter data was lost\n", smp_processor_id());
}

#define PMC_INIT      0
#define PMC_RELEASE   1
static void setup_pmc_cpu(void *flags)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);

	switch (*((int *) flags)) {
	case PMC_INIT:
		memset(&cpuhw->info, 0, sizeof(cpuhw->info));
		qctri(&cpuhw->info);
		cpuhw->flags |= PMU_F_RESERVED;
		break;

	case PMC_RELEASE:
		cpuhw->flags &= ~PMU_F_RESERVED;
		break;
	}

	
	lcctl(0);
}

static int reserve_pmc_hardware(void)
{
	int flags = PMC_INIT;

	on_each_cpu(setup_pmc_cpu, &flags, 1);
	measurement_alert_subclass_register();

	return 0;
}

static void release_pmc_hardware(void)
{
	int flags = PMC_RELEASE;

	on_each_cpu(setup_pmc_cpu, &flags, 1);
	measurement_alert_subclass_unregister();
}

static void hw_perf_event_destroy(struct perf_event *event)
{
	if (!atomic_add_unless(&num_events, -1, 1)) {
		mutex_lock(&pmc_reserve_mutex);
		if (atomic_dec_return(&num_events) == 0)
			release_pmc_hardware();
		mutex_unlock(&pmc_reserve_mutex);
	}
}

static const int cpumf_generic_events_basic[] = {
	[PERF_COUNT_HW_CPU_CYCLES]	    = 0,
	[PERF_COUNT_HW_INSTRUCTIONS]	    = 1,
	[PERF_COUNT_HW_CACHE_REFERENCES]    = -1,
	[PERF_COUNT_HW_CACHE_MISSES]	    = -1,
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = -1,
	[PERF_COUNT_HW_BRANCH_MISSES]	    = -1,
	[PERF_COUNT_HW_BUS_CYCLES]	    = -1,
};
static const int cpumf_generic_events_user[] = {
	[PERF_COUNT_HW_CPU_CYCLES]	    = 32,
	[PERF_COUNT_HW_INSTRUCTIONS]	    = 33,
	[PERF_COUNT_HW_CACHE_REFERENCES]    = -1,
	[PERF_COUNT_HW_CACHE_MISSES]	    = -1,
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = -1,
	[PERF_COUNT_HW_BRANCH_MISSES]	    = -1,
	[PERF_COUNT_HW_BUS_CYCLES]	    = -1,
};

static int __hw_perf_event_init(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	struct hw_perf_event *hwc = &event->hw;
	int err;
	u64 ev;

	switch (attr->type) {
	case PERF_TYPE_RAW:
		if (attr->exclude_kernel || attr->exclude_user ||
		    attr->exclude_hv)
			return -EOPNOTSUPP;
		ev = attr->config;
		break;

	case PERF_TYPE_HARDWARE:
		ev = attr->config;
		
		if (!attr->exclude_user && attr->exclude_kernel) {
			if (ev >= ARRAY_SIZE(cpumf_generic_events_user))
				return -EOPNOTSUPP;
			ev = cpumf_generic_events_user[ev];

		
		} else if (!attr->exclude_kernel && attr->exclude_user) {
			return -EOPNOTSUPP;

		
		} else {
			if (ev >= ARRAY_SIZE(cpumf_generic_events_basic))
				return -EOPNOTSUPP;
			ev = cpumf_generic_events_basic[ev];
		}
		break;

	default:
		return -ENOENT;
	}

	if (ev == -1)
		return -ENOENT;

	if (ev >= PERF_CPUM_CF_MAX_CTR)
		return -EINVAL;

	if (hwc->sample_period)
		return -EINVAL;

	hwc->config = ev;
	hwc->config_base = get_counter_set(ev);

	err = validate_event(hwc);
	if (err)
		return err;

	
	if (!atomic_inc_not_zero(&num_events)) {
		mutex_lock(&pmc_reserve_mutex);
		if (atomic_read(&num_events) == 0 && reserve_pmc_hardware())
			err = -EBUSY;
		else
			atomic_inc(&num_events);
		mutex_unlock(&pmc_reserve_mutex);
	}
	event->destroy = hw_perf_event_destroy;

	
	err = validate_ctr_auth(hwc);
	if (!err)
		err = validate_ctr_version(hwc);

	return err;
}

static int cpumf_pmu_event_init(struct perf_event *event)
{
	int err;

	switch (event->attr.type) {
	case PERF_TYPE_HARDWARE:
	case PERF_TYPE_HW_CACHE:
	case PERF_TYPE_RAW:
		err = __hw_perf_event_init(event);
		break;
	default:
		return -ENOENT;
	}

	if (unlikely(err) && event->destroy)
		event->destroy(event);

	return err;
}

static int hw_perf_event_reset(struct perf_event *event)
{
	u64 prev, new;
	int err;

	do {
		prev = local64_read(&event->hw.prev_count);
		err = ecctr(event->hw.config, &new);
		if (err) {
			if (err != 3)
				break;
			new = 0;
		}
	} while (local64_cmpxchg(&event->hw.prev_count, prev, new) != prev);

	return err;
}

static int hw_perf_event_update(struct perf_event *event)
{
	u64 prev, new, delta;
	int err;

	do {
		prev = local64_read(&event->hw.prev_count);
		err = ecctr(event->hw.config, &new);
		if (err)
			goto out;
	} while (local64_cmpxchg(&event->hw.prev_count, prev, new) != prev);

	delta = (prev <= new) ? new - prev
			      : (-1ULL - prev) + new + 1;	 
	local64_add(delta, &event->count);
out:
	return err;
}

static void cpumf_pmu_read(struct perf_event *event)
{
	if (event->hw.state & PERF_HES_STOPPED)
		return;

	hw_perf_event_update(event);
}

static void cpumf_pmu_start(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;

	if (WARN_ON_ONCE(!(hwc->state & PERF_HES_STOPPED)))
		return;

	if (WARN_ON_ONCE(hwc->config == -1))
		return;

	if (flags & PERF_EF_RELOAD)
		WARN_ON_ONCE(!(hwc->state & PERF_HES_UPTODATE));

	hwc->state = 0;

	
	ctr_set_enable(&cpuhw->state, hwc->config_base);
	ctr_set_start(&cpuhw->state, hwc->config_base);

	hw_perf_event_reset(event);

	
	atomic_inc(&cpuhw->ctr_set[hwc->config_base]);
}

static void cpumf_pmu_stop(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;

	if (!(hwc->state & PERF_HES_STOPPED)) {
		if (!atomic_dec_return(&cpuhw->ctr_set[hwc->config_base]))
			ctr_set_stop(&cpuhw->state, hwc->config_base);
		event->hw.state |= PERF_HES_STOPPED;
	}

	if ((flags & PERF_EF_UPDATE) && !(hwc->state & PERF_HES_UPTODATE)) {
		hw_perf_event_update(event);
		event->hw.state |= PERF_HES_UPTODATE;
	}
}

static int cpumf_pmu_add(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);

	if (!(cpuhw->flags & PERF_EVENT_TXN))
		if (validate_ctr_auth(&event->hw))
			return -EPERM;

	ctr_set_enable(&cpuhw->state, event->hw.config_base);
	event->hw.state = PERF_HES_UPTODATE | PERF_HES_STOPPED;

	if (flags & PERF_EF_START)
		cpumf_pmu_start(event, PERF_EF_RELOAD);

	perf_event_update_userpage(event);

	return 0;
}

static void cpumf_pmu_del(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);

	cpumf_pmu_stop(event, PERF_EF_UPDATE);

	if (!atomic_read(&cpuhw->ctr_set[event->hw.config_base]))
		ctr_set_disable(&cpuhw->state, event->hw.config_base);

	perf_event_update_userpage(event);
}

static void cpumf_pmu_start_txn(struct pmu *pmu)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);

	perf_pmu_disable(pmu);
	cpuhw->flags |= PERF_EVENT_TXN;
	cpuhw->tx_state = cpuhw->state;
}

static void cpumf_pmu_cancel_txn(struct pmu *pmu)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);

	WARN_ON(cpuhw->tx_state != cpuhw->state);

	cpuhw->flags &= ~PERF_EVENT_TXN;
	perf_pmu_enable(pmu);
}

static int cpumf_pmu_commit_txn(struct pmu *pmu)
{
	struct cpu_hw_events *cpuhw = &__get_cpu_var(cpu_hw_events);
	u64 state;

	
	state = cpuhw->state & ~((1 << CPUMF_LCCTL_ENABLE_SHIFT) - 1);
	state >>= CPUMF_LCCTL_ENABLE_SHIFT;
	if ((state & cpuhw->info.auth_ctl) != state)
		return -EPERM;

	cpuhw->flags &= ~PERF_EVENT_TXN;
	perf_pmu_enable(pmu);
	return 0;
}

static struct pmu cpumf_pmu = {
	.pmu_enable   = cpumf_pmu_enable,
	.pmu_disable  = cpumf_pmu_disable,
	.event_init   = cpumf_pmu_event_init,
	.add	      = cpumf_pmu_add,
	.del	      = cpumf_pmu_del,
	.start	      = cpumf_pmu_start,
	.stop	      = cpumf_pmu_stop,
	.read	      = cpumf_pmu_read,
	.start_txn    = cpumf_pmu_start_txn,
	.commit_txn   = cpumf_pmu_commit_txn,
	.cancel_txn   = cpumf_pmu_cancel_txn,
};

static int __cpuinit cpumf_pmu_notifier(struct notifier_block *self,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (long) hcpu;
	int flags;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_ONLINE:
		flags = PMC_INIT;
		smp_call_function_single(cpu, setup_pmc_cpu, &flags, 1);
		break;
	case CPU_DOWN_PREPARE:
		flags = PMC_RELEASE;
		smp_call_function_single(cpu, setup_pmc_cpu, &flags, 1);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int __init cpumf_pmu_init(void)
{
	int rc;

	if (!cpum_cf_avail())
		return -ENODEV;

	ctl_clear_bit(0, 48);

	
	rc = register_external_interrupt(0x1407, cpumf_measurement_alert);
	if (rc) {
		pr_err("Registering for CPU-measurement alerts "
		       "failed with rc=%i\n", rc);
		goto out;
	}

	rc = perf_pmu_register(&cpumf_pmu, "cpum_cf", PERF_TYPE_RAW);
	if (rc) {
		pr_err("Registering the cpum_cf PMU failed with rc=%i\n", rc);
		unregister_external_interrupt(0x1407, cpumf_measurement_alert);
		goto out;
	}
	perf_cpu_notifier(cpumf_pmu_notifier);
out:
	return rc;
}
early_initcall(cpumf_pmu_init);
