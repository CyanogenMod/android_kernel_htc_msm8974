/*
 * Hardware performance events for the Alpha.
 *
 * We implement HW counts on the EV67 and subsequent CPUs only.
 *
 * (C) 2010 Michael J. Cree
 *
 * Somewhat based on the Sparc code, and to a lesser extent the PowerPC and
 * ARM code, which are copyright by their respective authors.
 */

#include <linux/perf_event.h>
#include <linux/kprobes.h>
#include <linux/kernel.h>
#include <linux/kdebug.h>
#include <linux/mutex.h>
#include <linux/init.h>

#include <asm/hwrpb.h>
#include <linux/atomic.h>
#include <asm/irq.h>
#include <asm/irq_regs.h>
#include <asm/pal.h>
#include <asm/wrperfmon.h>
#include <asm/hw_irq.h>


#define MAX_HWEVENTS 3
#define PMC_NO_INDEX -1

struct cpu_hw_events {
	int			enabled;
	
	int			n_events;
	
	int			n_added;
	
	struct perf_event	*event[MAX_HWEVENTS];
	
	unsigned long		evtype[MAX_HWEVENTS];
	int			current_idx[MAX_HWEVENTS];
	
	unsigned long		config;
	
	unsigned long		idx_mask;
};
DEFINE_PER_CPU(struct cpu_hw_events, cpu_hw_events);



struct alpha_pmu_t {
	
	const int *event_map;
	
	int  max_events;
	
	int  num_pmcs;
	int  pmc_count_shift[MAX_HWEVENTS];
	unsigned long pmc_count_mask[MAX_HWEVENTS];
	
	unsigned long pmc_max_period[MAX_HWEVENTS];
	/*
	 * The maximum value that may be written to the counter due to
	 * hardware restrictions is pmc_max_period - pmc_left.
	 */
	long pmc_left[3];
	 
	int (*check_constraints)(struct perf_event **, unsigned long *, int);
};

static const struct alpha_pmu_t *alpha_pmu;


#define HW_OP_UNSUPPORTED -1


enum ev67_pmc_event_type {
	EV67_CYCLES = 1,
	EV67_INSTRUCTIONS,
	EV67_BCACHEMISS,
	EV67_MBOXREPLAY,
	EV67_LAST_ET
};
#define EV67_NUM_EVENT_TYPES (EV67_LAST_ET-EV67_CYCLES)


static const int ev67_perfmon_event_map[] = {
	[PERF_COUNT_HW_CPU_CYCLES]	 = EV67_CYCLES,
	[PERF_COUNT_HW_INSTRUCTIONS]	 = EV67_INSTRUCTIONS,
	[PERF_COUNT_HW_CACHE_REFERENCES] = HW_OP_UNSUPPORTED,
	[PERF_COUNT_HW_CACHE_MISSES]	 = EV67_BCACHEMISS,
};

struct ev67_mapping_t {
	int config;
	int idx;
};

static const struct ev67_mapping_t ev67_mapping[] = {
	{EV67_PCTR_INSTR_CYCLES, 1},	 
	{EV67_PCTR_INSTR_CYCLES, 0},	 
	{EV67_PCTR_INSTR_BCACHEMISS, 1}, 
	{EV67_PCTR_CYCLES_MBOX, 1}	 
};


static int ev67_check_constraints(struct perf_event **event,
				unsigned long *evtype, int n_ev)
{
	int idx0;
	unsigned long config;

	idx0 = ev67_mapping[evtype[0]-1].idx;
	config = ev67_mapping[evtype[0]-1].config;
	if (n_ev == 1)
		goto success;

	BUG_ON(n_ev != 2);

	if (evtype[0] == EV67_MBOXREPLAY || evtype[1] == EV67_MBOXREPLAY) {
		
		idx0 = (evtype[0] == EV67_MBOXREPLAY) ? 1 : 0;
		
		if (evtype[idx0] == EV67_CYCLES) {
			config = EV67_PCTR_CYCLES_MBOX;
			goto success;
		}
	}

	if (evtype[0] == EV67_BCACHEMISS || evtype[1] == EV67_BCACHEMISS) {
		
		idx0 = (evtype[0] == EV67_BCACHEMISS) ? 1 : 0;
		
		if (evtype[idx0] == EV67_INSTRUCTIONS) {
			config = EV67_PCTR_INSTR_BCACHEMISS;
			goto success;
		}
	}

	if (evtype[0] == EV67_INSTRUCTIONS || evtype[1] == EV67_INSTRUCTIONS) {
		
		idx0 = (evtype[0] == EV67_INSTRUCTIONS) ? 0 : 1;
		
		if (evtype[idx0^1] == EV67_CYCLES) {
			config = EV67_PCTR_INSTR_CYCLES;
			goto success;
		}
	}

	
	return -1;

success:
	event[0]->hw.idx = idx0;
	event[0]->hw.config_base = config;
	if (n_ev == 2) {
		event[1]->hw.idx = idx0 ^ 1;
		event[1]->hw.config_base = config;
	}
	return 0;
}


static const struct alpha_pmu_t ev67_pmu = {
	.event_map = ev67_perfmon_event_map,
	.max_events = ARRAY_SIZE(ev67_perfmon_event_map),
	.num_pmcs = 2,
	.pmc_count_shift = {EV67_PCTR_0_COUNT_SHIFT, EV67_PCTR_1_COUNT_SHIFT, 0},
	.pmc_count_mask = {EV67_PCTR_0_COUNT_MASK,  EV67_PCTR_1_COUNT_MASK,  0},
	.pmc_max_period = {(1UL<<20) - 1, (1UL<<20) - 1, 0},
	.pmc_left = {16, 4, 0},
	.check_constraints = ev67_check_constraints
};



static inline void alpha_write_pmc(int idx, unsigned long val)
{
	val &= alpha_pmu->pmc_count_mask[idx];
	val <<= alpha_pmu->pmc_count_shift[idx];
	val |= (1<<idx);
	wrperfmon(PERFMON_CMD_WRITE, val);
}

static inline unsigned long alpha_read_pmc(int idx)
{
	unsigned long val;

	val = wrperfmon(PERFMON_CMD_READ, 0);
	val >>= alpha_pmu->pmc_count_shift[idx];
	val &= alpha_pmu->pmc_count_mask[idx];
	return val;
}

static int alpha_perf_event_set_period(struct perf_event *event,
				struct hw_perf_event *hwc, int idx)
{
	long left = local64_read(&hwc->period_left);
	long period = hwc->sample_period;
	int ret = 0;

	if (unlikely(left <= -period)) {
		left = period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (unlikely(left <= 0)) {
		left += period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	/*
	 * Hardware restrictions require that the counters must not be
	 * written with values that are too close to the maximum period.
	 */
	if (unlikely(left < alpha_pmu->pmc_left[idx]))
		left = alpha_pmu->pmc_left[idx];

	if (left > (long)alpha_pmu->pmc_max_period[idx])
		left = alpha_pmu->pmc_max_period[idx];

	local64_set(&hwc->prev_count, (unsigned long)(-left));

	alpha_write_pmc(idx, (unsigned long)(-left));

	perf_event_update_userpage(event);

	return ret;
}


static unsigned long alpha_perf_event_update(struct perf_event *event,
					struct hw_perf_event *hwc, int idx, long ovf)
{
	long prev_raw_count, new_raw_count;
	long delta;

again:
	prev_raw_count = local64_read(&hwc->prev_count);
	new_raw_count = alpha_read_pmc(idx);

	if (local64_cmpxchg(&hwc->prev_count, prev_raw_count,
			     new_raw_count) != prev_raw_count)
		goto again;

	delta = (new_raw_count - (prev_raw_count & alpha_pmu->pmc_count_mask[idx])) + ovf;

	if (unlikely(delta < 0)) {
		delta += alpha_pmu->pmc_max_period[idx] + 1;
	}

	local64_add(delta, &event->count);
	local64_sub(delta, &hwc->period_left);

	return new_raw_count;
}


static int collect_events(struct perf_event *group, int max_count,
			  struct perf_event *event[], unsigned long *evtype,
			  int *current_idx)
{
	struct perf_event *pe;
	int n = 0;

	if (!is_software_event(group)) {
		if (n >= max_count)
			return -1;
		event[n] = group;
		evtype[n] = group->hw.event_base;
		current_idx[n++] = PMC_NO_INDEX;
	}
	list_for_each_entry(pe, &group->sibling_list, group_entry) {
		if (!is_software_event(pe) && pe->state != PERF_EVENT_STATE_OFF) {
			if (n >= max_count)
				return -1;
			event[n] = pe;
			evtype[n] = pe->hw.event_base;
			current_idx[n++] = PMC_NO_INDEX;
		}
	}
	return n;
}



static int alpha_check_constraints(struct perf_event **events,
				   unsigned long *evtypes, int n_ev)
{

	
	if (n_ev == 0)
		return 0;

	if (n_ev > alpha_pmu->num_pmcs)
		return -1;

	return alpha_pmu->check_constraints(events, evtypes, n_ev);
}


static void maybe_change_configuration(struct cpu_hw_events *cpuc)
{
	int j;

	if (cpuc->n_added == 0)
		return;

	
	for (j = 0; j < cpuc->n_events; j++) {
		struct perf_event *pe = cpuc->event[j];

		if (cpuc->current_idx[j] != PMC_NO_INDEX &&
			cpuc->current_idx[j] != pe->hw.idx) {
			alpha_perf_event_update(pe, &pe->hw, cpuc->current_idx[j], 0);
			cpuc->current_idx[j] = PMC_NO_INDEX;
		}
	}

	
	cpuc->idx_mask = 0;
	for (j = 0; j < cpuc->n_events; j++) {
		struct perf_event *pe = cpuc->event[j];
		struct hw_perf_event *hwc = &pe->hw;
		int idx = hwc->idx;

		if (cpuc->current_idx[j] == PMC_NO_INDEX) {
			alpha_perf_event_set_period(pe, hwc, idx);
			cpuc->current_idx[j] = idx;
		}

		if (!(hwc->state & PERF_HES_STOPPED))
			cpuc->idx_mask |= (1<<cpuc->current_idx[j]);
	}
	cpuc->config = cpuc->event[0]->hw.config_base;
}



static int alpha_pmu_add(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;
	int n0;
	int ret;
	unsigned long irq_flags;

	perf_pmu_disable(event->pmu);
	local_irq_save(irq_flags);

	
	ret = -EAGAIN;

	
	n0 = cpuc->n_events;
	if (n0 < alpha_pmu->num_pmcs) {
		cpuc->event[n0] = event;
		cpuc->evtype[n0] = event->hw.event_base;
		cpuc->current_idx[n0] = PMC_NO_INDEX;

		if (!alpha_check_constraints(cpuc->event, cpuc->evtype, n0+1)) {
			cpuc->n_events++;
			cpuc->n_added++;
			ret = 0;
		}
	}

	hwc->state = PERF_HES_UPTODATE;
	if (!(flags & PERF_EF_START))
		hwc->state |= PERF_HES_STOPPED;

	local_irq_restore(irq_flags);
	perf_pmu_enable(event->pmu);

	return ret;
}



static void alpha_pmu_del(struct perf_event *event, int flags)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;
	unsigned long irq_flags;
	int j;

	perf_pmu_disable(event->pmu);
	local_irq_save(irq_flags);

	for (j = 0; j < cpuc->n_events; j++) {
		if (event == cpuc->event[j]) {
			int idx = cpuc->current_idx[j];

			while (++j < cpuc->n_events) {
				cpuc->event[j - 1] = cpuc->event[j];
				cpuc->evtype[j - 1] = cpuc->evtype[j];
				cpuc->current_idx[j - 1] =
					cpuc->current_idx[j];
			}

			
			alpha_perf_event_update(event, hwc, idx, 0);
			perf_event_update_userpage(event);

			cpuc->idx_mask &= ~(1UL<<idx);
			cpuc->n_events--;
			break;
		}
	}

	local_irq_restore(irq_flags);
	perf_pmu_enable(event->pmu);
}


static void alpha_pmu_read(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;

	alpha_perf_event_update(event, hwc, hwc->idx, 0);
}


static void alpha_pmu_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (!(hwc->state & PERF_HES_STOPPED)) {
		cpuc->idx_mask &= ~(1UL<<hwc->idx);
		hwc->state |= PERF_HES_STOPPED;
	}

	if ((flags & PERF_EF_UPDATE) && !(hwc->state & PERF_HES_UPTODATE)) {
		alpha_perf_event_update(event, hwc, hwc->idx, 0);
		hwc->state |= PERF_HES_UPTODATE;
	}

	if (cpuc->enabled)
		wrperfmon(PERFMON_CMD_DISABLE, (1UL<<hwc->idx));
}


static void alpha_pmu_start(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (WARN_ON_ONCE(!(hwc->state & PERF_HES_STOPPED)))
		return;

	if (flags & PERF_EF_RELOAD) {
		WARN_ON_ONCE(!(hwc->state & PERF_HES_UPTODATE));
		alpha_perf_event_set_period(event, hwc, hwc->idx);
	}

	hwc->state = 0;

	cpuc->idx_mask |= 1UL<<hwc->idx;
	if (cpuc->enabled)
		wrperfmon(PERFMON_CMD_ENABLE, (1UL<<hwc->idx));
}


static int supported_cpu(void)
{
	struct percpu_struct *cpu;
	unsigned long cputype;

	
	cpu = (struct percpu_struct *)((char *)hwrpb + hwrpb->processor_offset);
	cputype = cpu->type & 0xffffffff;
	
	return (cputype >= EV67_CPU) && (cputype <= EV69_CPU);
}



static void hw_perf_event_destroy(struct perf_event *event)
{
	
	return;
}



static int __hw_perf_event_init(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	struct hw_perf_event *hwc = &event->hw;
	struct perf_event *evts[MAX_HWEVENTS];
	unsigned long evtypes[MAX_HWEVENTS];
	int idx_rubbish_bin[MAX_HWEVENTS];
	int ev;
	int n;

	if (attr->type == PERF_TYPE_HARDWARE) {
		if (attr->config >= alpha_pmu->max_events)
			return -EINVAL;
		ev = alpha_pmu->event_map[attr->config];
	} else if (attr->type == PERF_TYPE_HW_CACHE) {
		return -EOPNOTSUPP;
	} else if (attr->type == PERF_TYPE_RAW) {
		ev = attr->config & 0xff;
	} else {
		return -EOPNOTSUPP;
	}

	if (ev < 0) {
		return ev;
	}

	
	if (attr->exclude_kernel || attr->exclude_user
			|| attr->exclude_hv || attr->exclude_idle) {
		return -EPERM;
	}

	hwc->event_base = ev;

	n = 0;
	if (event->group_leader != event) {
		n = collect_events(event->group_leader,
				alpha_pmu->num_pmcs - 1,
				evts, evtypes, idx_rubbish_bin);
		if (n < 0)
			return -EINVAL;
	}
	evtypes[n] = hwc->event_base;
	evts[n] = event;

	if (alpha_check_constraints(evts, evtypes, n + 1))
		return -EINVAL;

	
	hwc->config_base = 0;
	hwc->idx = PMC_NO_INDEX;

	event->destroy = hw_perf_event_destroy;


	if (!hwc->sample_period) {
		hwc->sample_period = alpha_pmu->pmc_max_period[0];
		hwc->last_period = hwc->sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
	}

	return 0;
}

static int alpha_pmu_event_init(struct perf_event *event)
{
	int err;

	
	if (has_branch_stack(event))
		return -EOPNOTSUPP;

	switch (event->attr.type) {
	case PERF_TYPE_RAW:
	case PERF_TYPE_HARDWARE:
	case PERF_TYPE_HW_CACHE:
		break;

	default:
		return -ENOENT;
	}

	if (!alpha_pmu)
		return -ENODEV;

	
	err = __hw_perf_event_init(event);

	return err;
}

static void alpha_pmu_enable(struct pmu *pmu)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (cpuc->enabled)
		return;

	cpuc->enabled = 1;
	barrier();

	if (cpuc->n_events > 0) {
		
		maybe_change_configuration(cpuc);

		
		wrperfmon(PERFMON_CMD_LOGGING_OPTIONS, EV67_PCTR_MODE_AGGREGATE);
		wrperfmon(PERFMON_CMD_DESIRED_EVENTS, cpuc->config);
		wrperfmon(PERFMON_CMD_ENABLE, cpuc->idx_mask);
	}
}



static void alpha_pmu_disable(struct pmu *pmu)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (!cpuc->enabled)
		return;

	cpuc->enabled = 0;
	cpuc->n_added = 0;

	wrperfmon(PERFMON_CMD_DISABLE, cpuc->idx_mask);
}

static struct pmu pmu = {
	.pmu_enable	= alpha_pmu_enable,
	.pmu_disable	= alpha_pmu_disable,
	.event_init	= alpha_pmu_event_init,
	.add		= alpha_pmu_add,
	.del		= alpha_pmu_del,
	.start		= alpha_pmu_start,
	.stop		= alpha_pmu_stop,
	.read		= alpha_pmu_read,
};


void perf_event_print_debug(void)
{
	unsigned long flags;
	unsigned long pcr;
	int pcr0, pcr1;
	int cpu;

	if (!supported_cpu())
		return;

	local_irq_save(flags);

	cpu = smp_processor_id();

	pcr = wrperfmon(PERFMON_CMD_READ, 0);
	pcr0 = (pcr >> alpha_pmu->pmc_count_shift[0]) & alpha_pmu->pmc_count_mask[0];
	pcr1 = (pcr >> alpha_pmu->pmc_count_shift[1]) & alpha_pmu->pmc_count_mask[1];

	pr_info("CPU#%d: PCTR0[%06x] PCTR1[%06x]\n", cpu, pcr0, pcr1);

	local_irq_restore(flags);
}


static void alpha_perf_event_irq_handler(unsigned long la_ptr,
					struct pt_regs *regs)
{
	struct cpu_hw_events *cpuc;
	struct perf_sample_data data;
	struct perf_event *event;
	struct hw_perf_event *hwc;
	int idx, j;

	__get_cpu_var(irq_pmi_count)++;
	cpuc = &__get_cpu_var(cpu_hw_events);

	wrperfmon(PERFMON_CMD_DISABLE, cpuc->idx_mask);

	
	if (unlikely(la_ptr >= alpha_pmu->num_pmcs)) {
		
		irq_err_count++;
		pr_warning("PMI: silly index %ld\n", la_ptr);
		wrperfmon(PERFMON_CMD_ENABLE, cpuc->idx_mask);
		return;
	}

	idx = la_ptr;

	perf_sample_data_init(&data, 0);
	for (j = 0; j < cpuc->n_events; j++) {
		if (cpuc->current_idx[j] == idx)
			break;
	}

	if (unlikely(j == cpuc->n_events)) {
		
		wrperfmon(PERFMON_CMD_ENABLE, cpuc->idx_mask);
		return;
	}

	event = cpuc->event[j];

	if (unlikely(!event)) {
		
		irq_err_count++;
		pr_warning("PMI: No event at index %d!\n", idx);
		wrperfmon(PERFMON_CMD_ENABLE, cpuc->idx_mask);
		return;
	}

	hwc = &event->hw;
	alpha_perf_event_update(event, hwc, idx, alpha_pmu->pmc_max_period[idx]+1);
	data.period = event->hw.last_period;

	if (alpha_perf_event_set_period(event, hwc, idx)) {
		if (perf_event_overflow(event, &data, regs)) {
			alpha_pmu_stop(event, 0);
		}
	}
	wrperfmon(PERFMON_CMD_ENABLE, cpuc->idx_mask);

	return;
}



int __init init_hw_perf_events(void)
{
	pr_info("Performance events: ");

	if (!supported_cpu()) {
		pr_cont("No support for your CPU.\n");
		return 0;
	}

	pr_cont("Supported CPU type!\n");

	

	perf_irq = alpha_perf_event_irq_handler;

	
	alpha_pmu = &ev67_pmu;

	perf_pmu_register(&pmu, "cpu", PERF_TYPE_RAW);

	return 0;
}
early_initcall(init_hw_perf_events);
