/*
 * Performance counter support for e500 family processors.
 *
 * Copyright 2008-2009 Paul Mackerras, IBM Corporation.
 * Copyright 2010 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/string.h>
#include <linux/perf_event.h>
#include <asm/reg.h>
#include <asm/cputable.h>

static int e500_generic_events[] = {
	[PERF_COUNT_HW_CPU_CYCLES] = 1,
	[PERF_COUNT_HW_INSTRUCTIONS] = 2,
	[PERF_COUNT_HW_CACHE_MISSES] = 41, 
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = 12,
	[PERF_COUNT_HW_BRANCH_MISSES] = 15,
};

#define C(x)	PERF_COUNT_HW_CACHE_##x

static int e500_cache_events[C(MAX)][C(OP_MAX)][C(RESULT_MAX)] = {
	[C(L1D)] = {		
		[C(OP_READ)] = {	27,		0	},
		[C(OP_WRITE)] = {	28,		0	},
		[C(OP_PREFETCH)] = {	29,		0	},
	},
	[C(L1I)] = {		
		[C(OP_READ)] = {	2,		60	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	0,		0	},
	},
	[C(LL)] = {		
		[C(OP_READ)] = {	0,		0	},
		[C(OP_WRITE)] = {	0,		0	},
		[C(OP_PREFETCH)] = {	0,		0	},
	},
	[C(DTLB)] = {		
		[C(OP_READ)] = {	26,		66	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(BPU)] = {		
		[C(OP_READ)] = {	12,		15 	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(NODE)] = {		
		[C(OP_READ)] = {	-1,		-1 	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
};

static int num_events = 128;

static u64 e500_xlate_event(u64 event_id)
{
	u32 event_low = (u32)event_id;
	u64 ret;

	if (event_low >= num_events)
		return 0;

	ret = FSL_EMB_EVENT_VALID;

	if (event_low >= 76 && event_low <= 81) {
		ret |= FSL_EMB_EVENT_RESTRICTED;
		ret |= event_id &
		       (FSL_EMB_EVENT_THRESHMUL | FSL_EMB_EVENT_THRESH);
	} else if (event_id &
	           (FSL_EMB_EVENT_THRESHMUL | FSL_EMB_EVENT_THRESH)) {
		
		return 0;
	}

	return ret;
}

static struct fsl_emb_pmu e500_pmu = {
	.name			= "e500 family",
	.n_counter		= 4,
	.n_restricted		= 2,
	.xlate_event		= e500_xlate_event,
	.n_generic		= ARRAY_SIZE(e500_generic_events),
	.generic_events		= e500_generic_events,
	.cache_events		= &e500_cache_events,
};

static int init_e500_pmu(void)
{
	if (!cur_cpu_spec->oprofile_cpu_type)
		return -ENODEV;

	if (!strcmp(cur_cpu_spec->oprofile_cpu_type, "ppc/e500mc"))
		num_events = 256;
	else if (strcmp(cur_cpu_spec->oprofile_cpu_type, "ppc/e500"))
		return -ENODEV;

	return register_fsl_emb_pmu(&e500_pmu);
}

early_initcall(init_e500_pmu);
