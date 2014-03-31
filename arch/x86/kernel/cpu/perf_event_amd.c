#include <linux/perf_event.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/apicdef.h>

#include "perf_event.h"

static __initconst const u64 amd_hw_cache_event_ids
				[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX] =
{
 [ C(L1D) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x0040, 
		[ C(RESULT_MISS)   ] = 0x0141, 
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = 0x0142, 
		[ C(RESULT_MISS)   ] = 0,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0x0267, 
		[ C(RESULT_MISS)   ] = 0x0167, 
	},
 },
 [ C(L1I ) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x0080, 
		[ C(RESULT_MISS)   ] = 0x0081, 
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0x014B, 
		[ C(RESULT_MISS)   ] = 0,
	},
 },
 [ C(LL  ) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x037D, 
		[ C(RESULT_MISS)   ] = 0x037E, 
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = 0x017F, 
		[ C(RESULT_MISS)   ] = 0,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0,
		[ C(RESULT_MISS)   ] = 0,
	},
 },
 [ C(DTLB) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x0040, 
		[ C(RESULT_MISS)   ] = 0x0746, 
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = 0,
		[ C(RESULT_MISS)   ] = 0,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0,
		[ C(RESULT_MISS)   ] = 0,
	},
 },
 [ C(ITLB) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x0080, 
		[ C(RESULT_MISS)   ] = 0x0385, 
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
 },
 [ C(BPU ) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x00c2, 
		[ C(RESULT_MISS)   ] = 0x00c3, 
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
 },
 [ C(NODE) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0xb8e9, 
		[ C(RESULT_MISS)   ] = 0x98e9, 
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
 },
};

static const u64 amd_perfmon_event_map[] =
{
  [PERF_COUNT_HW_CPU_CYCLES]			= 0x0076,
  [PERF_COUNT_HW_INSTRUCTIONS]			= 0x00c0,
  [PERF_COUNT_HW_CACHE_REFERENCES]		= 0x0080,
  [PERF_COUNT_HW_CACHE_MISSES]			= 0x0081,
  [PERF_COUNT_HW_BRANCH_INSTRUCTIONS]		= 0x00c2,
  [PERF_COUNT_HW_BRANCH_MISSES]			= 0x00c3,
  [PERF_COUNT_HW_STALLED_CYCLES_FRONTEND]	= 0x00d0, 
  [PERF_COUNT_HW_STALLED_CYCLES_BACKEND]	= 0x00d1, 
};

static u64 amd_pmu_event_map(int hw_event)
{
	return amd_perfmon_event_map[hw_event];
}

static int amd_pmu_hw_config(struct perf_event *event)
{
	int ret = x86_pmu_hw_config(event);

	if (ret)
		return ret;

	if (has_branch_stack(event))
		return -EOPNOTSUPP;

	if (event->attr.exclude_host && event->attr.exclude_guest)
		event->hw.config &= ~(ARCH_PERFMON_EVENTSEL_USR |
				      ARCH_PERFMON_EVENTSEL_OS);
	else if (event->attr.exclude_host)
		event->hw.config |= AMD_PERFMON_EVENTSEL_GUESTONLY;
	else if (event->attr.exclude_guest)
		event->hw.config |= AMD_PERFMON_EVENTSEL_HOSTONLY;

	if (event->attr.type != PERF_TYPE_RAW)
		return 0;

	event->hw.config |= event->attr.config & AMD64_RAW_EVENT_MASK;

	return 0;
}

static inline unsigned int amd_get_event_code(struct hw_perf_event *hwc)
{
	return ((hwc->config >> 24) & 0x0f00) | (hwc->config & 0x00ff);
}

static inline int amd_is_nb_event(struct hw_perf_event *hwc)
{
	return (hwc->config & 0xe0) == 0xe0;
}

static inline int amd_has_nb(struct cpu_hw_events *cpuc)
{
	struct amd_nb *nb = cpuc->amd_nb;

	return nb && nb->nb_id != -1;
}

static void amd_put_event_constraints(struct cpu_hw_events *cpuc,
				      struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct amd_nb *nb = cpuc->amd_nb;
	int i;

	if (!(amd_has_nb(cpuc) && amd_is_nb_event(hwc)))
		return;

	for (i = 0; i < x86_pmu.num_counters; i++) {
		if (nb->owners[i] == event) {
			cmpxchg(nb->owners+i, event, NULL);
			break;
		}
	}
}

static struct event_constraint *
amd_get_event_constraints(struct cpu_hw_events *cpuc, struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct amd_nb *nb = cpuc->amd_nb;
	struct perf_event *old = NULL;
	int max = x86_pmu.num_counters;
	int i, j, k = -1;

	if (!(amd_has_nb(cpuc) && amd_is_nb_event(hwc)))
		return &unconstrained;

	for (i = 0; i < max; i++) {
		if (k == -1 && !nb->owners[i])
			k = i;

		
		if (nb->owners[i] == event)
			goto done;
	}
	if (hwc->idx != -1) {
		
		i = hwc->idx;
	} else if (k != -1) {
		
		i = k;
	} else {
		i = 0;
	}
	j = i;
	do {
		old = cmpxchg(nb->owners+i, NULL, event);
		if (!old)
			break;
		if (++i == max)
			i = 0;
	} while (i != j);
done:
	if (!old)
		return &nb->event_constraints[i];

	return &emptyconstraint;
}

static struct amd_nb *amd_alloc_nb(int cpu)
{
	struct amd_nb *nb;
	int i;

	nb = kmalloc_node(sizeof(struct amd_nb), GFP_KERNEL | __GFP_ZERO,
			  cpu_to_node(cpu));
	if (!nb)
		return NULL;

	nb->nb_id = -1;

	for (i = 0; i < x86_pmu.num_counters; i++) {
		__set_bit(i, nb->event_constraints[i].idxmsk);
		nb->event_constraints[i].weight = 1;
	}
	return nb;
}

static int amd_pmu_cpu_prepare(int cpu)
{
	struct cpu_hw_events *cpuc = &per_cpu(cpu_hw_events, cpu);

	WARN_ON_ONCE(cpuc->amd_nb);

	if (boot_cpu_data.x86_max_cores < 2)
		return NOTIFY_OK;

	cpuc->amd_nb = amd_alloc_nb(cpu);
	if (!cpuc->amd_nb)
		return NOTIFY_BAD;

	return NOTIFY_OK;
}

static void amd_pmu_cpu_starting(int cpu)
{
	struct cpu_hw_events *cpuc = &per_cpu(cpu_hw_events, cpu);
	struct amd_nb *nb;
	int i, nb_id;

	cpuc->perf_ctr_virt_mask = AMD_PERFMON_EVENTSEL_HOSTONLY;

	if (boot_cpu_data.x86_max_cores < 2 || boot_cpu_data.x86 == 0x15)
		return;

	nb_id = amd_get_nb_id(cpu);
	WARN_ON_ONCE(nb_id == BAD_APICID);

	for_each_online_cpu(i) {
		nb = per_cpu(cpu_hw_events, i).amd_nb;
		if (WARN_ON_ONCE(!nb))
			continue;

		if (nb->nb_id == nb_id) {
			cpuc->kfree_on_online = cpuc->amd_nb;
			cpuc->amd_nb = nb;
			break;
		}
	}

	cpuc->amd_nb->nb_id = nb_id;
	cpuc->amd_nb->refcnt++;
}

static void amd_pmu_cpu_dead(int cpu)
{
	struct cpu_hw_events *cpuhw;

	if (boot_cpu_data.x86_max_cores < 2)
		return;

	cpuhw = &per_cpu(cpu_hw_events, cpu);

	if (cpuhw->amd_nb) {
		struct amd_nb *nb = cpuhw->amd_nb;

		if (nb->nb_id == -1 || --nb->refcnt == 0)
			kfree(nb);

		cpuhw->amd_nb = NULL;
	}
}

PMU_FORMAT_ATTR(event,	"config:0-7,32-35");
PMU_FORMAT_ATTR(umask,	"config:8-15"	);
PMU_FORMAT_ATTR(edge,	"config:18"	);
PMU_FORMAT_ATTR(inv,	"config:23"	);
PMU_FORMAT_ATTR(cmask,	"config:24-31"	);

static struct attribute *amd_format_attr[] = {
	&format_attr_event.attr,
	&format_attr_umask.attr,
	&format_attr_edge.attr,
	&format_attr_inv.attr,
	&format_attr_cmask.attr,
	NULL,
};

static __initconst const struct x86_pmu amd_pmu = {
	.name			= "AMD",
	.handle_irq		= x86_pmu_handle_irq,
	.disable_all		= x86_pmu_disable_all,
	.enable_all		= x86_pmu_enable_all,
	.enable			= x86_pmu_enable_event,
	.disable		= x86_pmu_disable_event,
	.hw_config		= amd_pmu_hw_config,
	.schedule_events	= x86_schedule_events,
	.eventsel		= MSR_K7_EVNTSEL0,
	.perfctr		= MSR_K7_PERFCTR0,
	.event_map		= amd_pmu_event_map,
	.max_events		= ARRAY_SIZE(amd_perfmon_event_map),
	.num_counters		= AMD64_NUM_COUNTERS,
	.cntval_bits		= 48,
	.cntval_mask		= (1ULL << 48) - 1,
	.apic			= 1,
	
	.max_period		= (1ULL << 47) - 1,
	.get_event_constraints	= amd_get_event_constraints,
	.put_event_constraints	= amd_put_event_constraints,

	.format_attrs		= amd_format_attr,

	.cpu_prepare		= amd_pmu_cpu_prepare,
	.cpu_starting		= amd_pmu_cpu_starting,
	.cpu_dead		= amd_pmu_cpu_dead,
};


#define AMD_EVENT_TYPE_MASK	0x000000F0ULL

#define AMD_EVENT_FP		0x00000000ULL ... 0x00000010ULL
#define AMD_EVENT_LS		0x00000020ULL ... 0x00000030ULL
#define AMD_EVENT_DC		0x00000040ULL ... 0x00000050ULL
#define AMD_EVENT_CU		0x00000060ULL ... 0x00000070ULL
#define AMD_EVENT_IC_DE		0x00000080ULL ... 0x00000090ULL
#define AMD_EVENT_EX_LS		0x000000C0ULL
#define AMD_EVENT_DE		0x000000D0ULL
#define AMD_EVENT_NB		0x000000E0ULL ... 0x000000F0ULL


static struct event_constraint amd_f15_PMC0  = EVENT_CONSTRAINT(0, 0x01, 0);
static struct event_constraint amd_f15_PMC20 = EVENT_CONSTRAINT(0, 0x07, 0);
static struct event_constraint amd_f15_PMC3  = EVENT_CONSTRAINT(0, 0x08, 0);
static struct event_constraint amd_f15_PMC30 = EVENT_CONSTRAINT_OVERLAP(0, 0x09, 0);
static struct event_constraint amd_f15_PMC50 = EVENT_CONSTRAINT(0, 0x3F, 0);
static struct event_constraint amd_f15_PMC53 = EVENT_CONSTRAINT(0, 0x38, 0);

static struct event_constraint *
amd_get_event_constraints_f15h(struct cpu_hw_events *cpuc, struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	unsigned int event_code = amd_get_event_code(hwc);

	switch (event_code & AMD_EVENT_TYPE_MASK) {
	case AMD_EVENT_FP:
		switch (event_code) {
		case 0x000:
			if (!(hwc->config & 0x0000F000ULL))
				break;
			if (!(hwc->config & 0x00000F00ULL))
				break;
			return &amd_f15_PMC3;
		case 0x004:
			if (hweight_long(hwc->config & ARCH_PERFMON_EVENTSEL_UMASK) <= 1)
				break;
			return &amd_f15_PMC3;
		case 0x003:
		case 0x00B:
		case 0x00D:
			return &amd_f15_PMC3;
		}
		return &amd_f15_PMC53;
	case AMD_EVENT_LS:
	case AMD_EVENT_DC:
	case AMD_EVENT_EX_LS:
		switch (event_code) {
		case 0x023:
		case 0x043:
		case 0x045:
		case 0x046:
		case 0x054:
		case 0x055:
			return &amd_f15_PMC20;
		case 0x02D:
			return &amd_f15_PMC3;
		case 0x02E:
			return &amd_f15_PMC30;
		default:
			return &amd_f15_PMC50;
		}
	case AMD_EVENT_CU:
	case AMD_EVENT_IC_DE:
	case AMD_EVENT_DE:
		switch (event_code) {
		case 0x08F:
		case 0x187:
		case 0x188:
			return &amd_f15_PMC0;
		case 0x0DB ... 0x0DF:
		case 0x1D6:
		case 0x1D8:
			return &amd_f15_PMC50;
		default:
			return &amd_f15_PMC20;
		}
	case AMD_EVENT_NB:
		
		return &emptyconstraint;
	default:
		return &emptyconstraint;
	}
}

static __initconst const struct x86_pmu amd_pmu_f15h = {
	.name			= "AMD Family 15h",
	.handle_irq		= x86_pmu_handle_irq,
	.disable_all		= x86_pmu_disable_all,
	.enable_all		= x86_pmu_enable_all,
	.enable			= x86_pmu_enable_event,
	.disable		= x86_pmu_disable_event,
	.hw_config		= amd_pmu_hw_config,
	.schedule_events	= x86_schedule_events,
	.eventsel		= MSR_F15H_PERF_CTL,
	.perfctr		= MSR_F15H_PERF_CTR,
	.event_map		= amd_pmu_event_map,
	.max_events		= ARRAY_SIZE(amd_perfmon_event_map),
	.num_counters		= AMD64_NUM_COUNTERS_F15H,
	.cntval_bits		= 48,
	.cntval_mask		= (1ULL << 48) - 1,
	.apic			= 1,
	
	.max_period		= (1ULL << 47) - 1,
	.get_event_constraints	= amd_get_event_constraints_f15h,
	
#if 0
	.put_event_constraints	= amd_put_event_constraints,

	.cpu_prepare		= amd_pmu_cpu_prepare,
	.cpu_dead		= amd_pmu_cpu_dead,
#endif
	.cpu_starting		= amd_pmu_cpu_starting,
	.format_attrs		= amd_format_attr,
};

__init int amd_pmu_init(void)
{
	
	if (boot_cpu_data.x86 < 6)
		return -ENODEV;

	switch (boot_cpu_data.x86) {
	case 0x15:
		if (!cpu_has_perfctr_core)
			return -ENODEV;
		x86_pmu = amd_pmu_f15h;
		break;
	default:
		if (cpu_has_perfctr_core)
			return -ENODEV;
		x86_pmu = amd_pmu;
		break;
	}

	
	memcpy(hw_cache_event_ids, amd_hw_cache_event_ids,
	       sizeof(hw_cache_event_ids));

	return 0;
}

void amd_pmu_enable_virt(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	cpuc->perf_ctr_virt_mask = 0;

	
	x86_pmu_disable_all();
	x86_pmu_enable_all(0);
}
EXPORT_SYMBOL_GPL(amd_pmu_enable_virt);

void amd_pmu_disable_virt(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	cpuc->perf_ctr_virt_mask = AMD_PERFMON_EVENTSEL_HOSTONLY;

	
	x86_pmu_disable_all();
	x86_pmu_enable_all(0);
}
EXPORT_SYMBOL_GPL(amd_pmu_disable_virt);
