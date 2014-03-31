/*
 * Performance event support - PowerPC classic/server specific definitions.
 *
 * Copyright 2008-2009 Paul Mackerras, IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/types.h>
#include <asm/hw_irq.h>

#define MAX_HWEVENTS		8
#define MAX_EVENT_ALTERNATIVES	8
#define MAX_LIMITED_HWCOUNTERS	2

struct power_pmu {
	const char	*name;
	int		n_counter;
	int		max_alternatives;
	unsigned long	add_fields;
	unsigned long	test_adder;
	int		(*compute_mmcr)(u64 events[], int n_ev,
				unsigned int hwc[], unsigned long mmcr[]);
	int		(*get_constraint)(u64 event_id, unsigned long *mskp,
				unsigned long *valp);
	int		(*get_alternatives)(u64 event_id, unsigned int flags,
				u64 alt[]);
	void		(*disable_pmc)(unsigned int pmc, unsigned long mmcr[]);
	int		(*limited_pmc_event)(u64 event_id);
	u32		flags;
	int		n_generic;
	int		*generic_events;
	int		(*cache_events)[PERF_COUNT_HW_CACHE_MAX]
			       [PERF_COUNT_HW_CACHE_OP_MAX]
			       [PERF_COUNT_HW_CACHE_RESULT_MAX];
};

#define PPMU_LIMITED_PMC5_6	1	
#define PPMU_ALT_SIPR		2	
#define PPMU_NO_SIPR		4	
#define PPMU_NO_CONT_SAMPLING	8	

#define PPMU_LIMITED_PMC_OK	1	
#define PPMU_LIMITED_PMC_REQD	2	
#define PPMU_ONLY_COUNT_RUN	4	

extern int register_power_pmu(struct power_pmu *);

struct pt_regs;
extern unsigned long perf_misc_flags(struct pt_regs *regs);
extern unsigned long perf_instruction_pointer(struct pt_regs *regs);

#ifdef CONFIG_PPC_PERF_CTRS
#define perf_misc_flags(regs)	perf_misc_flags(regs)
#endif

