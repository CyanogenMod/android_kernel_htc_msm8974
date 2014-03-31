/*
 * Performance counter support for POWER7 processors.
 *
 * Copyright 2009 Paul Mackerras, IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <linux/string.h>
#include <asm/reg.h>
#include <asm/cputable.h>

#define PM_PMC_SH	16	
#define PM_PMC_MSK	0xf
#define PM_PMC_MSKS	(PM_PMC_MSK << PM_PMC_SH)
#define PM_UNIT_SH	12	
#define PM_UNIT_MSK	0xf
#define PM_COMBINE_SH	11	
#define PM_COMBINE_MSK	1
#define PM_COMBINE_MSKS	0x800
#define PM_L2SEL_SH	8	
#define PM_L2SEL_MSK	7
#define PM_PMCSEL_MSK	0xff

#define MMCR1_TTM0SEL_SH	60
#define MMCR1_TTM1SEL_SH	56
#define MMCR1_TTM2SEL_SH	52
#define MMCR1_TTM3SEL_SH	48
#define MMCR1_TTMSEL_MSK	0xf
#define MMCR1_L2SEL_SH		45
#define MMCR1_L2SEL_MSK		7
#define MMCR1_PMC1_COMBINE_SH	35
#define MMCR1_PMC2_COMBINE_SH	34
#define MMCR1_PMC3_COMBINE_SH	33
#define MMCR1_PMC4_COMBINE_SH	32
#define MMCR1_PMC1SEL_SH	24
#define MMCR1_PMC2SEL_SH	16
#define MMCR1_PMC3SEL_SH	8
#define MMCR1_PMC4SEL_SH	0
#define MMCR1_PMCSEL_SH(n)	(MMCR1_PMC1SEL_SH - (n) * 8)
#define MMCR1_PMCSEL_MSK	0xff


static int power7_get_constraint(u64 event, unsigned long *maskp,
				 unsigned long *valp)
{
	int pmc, sh;
	unsigned long mask = 0, value = 0;

	pmc = (event >> PM_PMC_SH) & PM_PMC_MSK;
	if (pmc) {
		if (pmc > 6)
			return -1;
		sh = (pmc - 1) * 2;
		mask |= 2 << sh;
		value |= 1 << sh;
		if (pmc >= 5 && !(event == 0x500fa || event == 0x600f4))
			return -1;
	}
	if (pmc < 5) {
		
		mask  |= 0x8000;
		value |= 0x1000;
	}
	*maskp = mask;
	*valp = value;
	return 0;
}

#define MAX_ALT	2	

static const unsigned int event_alternatives[][MAX_ALT] = {
	{ 0x200f2, 0x300f2 },		
	{ 0x200f4, 0x600f4 },		
	{ 0x400fa, 0x500fa },		
};

static int find_alternative(u64 event)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(event_alternatives); ++i) {
		if (event < event_alternatives[i][0])
			break;
		for (j = 0; j < MAX_ALT && event_alternatives[i][j]; ++j)
			if (event == event_alternatives[i][j])
				return i;
	}
	return -1;
}

static s64 find_alternative_decode(u64 event)
{
	int pmc, psel;

	
	pmc = (event >> PM_PMC_SH) & PM_PMC_MSK;
	psel = event & PM_PMCSEL_MSK;
	if ((pmc == 2 || pmc == 4) && (psel & ~7) == 0x40)
		return event - (1 << PM_PMC_SH) + 8;
	if ((pmc == 1 || pmc == 3) && (psel & ~7) == 0x48)
		return event + (1 << PM_PMC_SH) - 8;
	return -1;
}

static int power7_get_alternatives(u64 event, unsigned int flags, u64 alt[])
{
	int i, j, nalt = 1;
	s64 ae;

	alt[0] = event;
	nalt = 1;
	i = find_alternative(event);
	if (i >= 0) {
		for (j = 0; j < MAX_ALT; ++j) {
			ae = event_alternatives[i][j];
			if (ae && ae != event)
				alt[nalt++] = ae;
		}
	} else {
		ae = find_alternative_decode(event);
		if (ae > 0)
			alt[nalt++] = ae;
	}

	if (flags & PPMU_ONLY_COUNT_RUN) {
		j = nalt;
		for (i = 0; i < nalt; ++i) {
			switch (alt[i]) {
			case 0x1e:	
				alt[j++] = 0x600f4;	
				break;
			case 0x600f4:	
				alt[j++] = 0x1e;
				break;
			case 0x2:	
				alt[j++] = 0x500fa;	
				break;
			case 0x500fa:	
				alt[j++] = 0x2;	
				break;
			}
		}
		nalt = j;
	}

	return nalt;
}

static int power7_marked_instr_event(u64 event)
{
	int pmc, psel;
	int unit;

	pmc = (event >> PM_PMC_SH) & PM_PMC_MSK;
	unit = (event >> PM_UNIT_SH) & PM_UNIT_MSK;
	psel = event & PM_PMCSEL_MSK & ~1;	
	if (pmc >= 5)
		return 0;

	switch (psel >> 4) {
	case 2:
		return pmc == 2 || pmc == 4;
	case 3:
		if (psel == 0x3c)
			return pmc == 1;
		if (psel == 0x3e)
			return pmc != 2;
		return 1;
	case 4:
	case 5:
		return unit == 0xd;
	case 6:
		if (psel == 0x64)
			return pmc >= 3;
	case 8:
		return unit == 0xd;
	}
	return 0;
}

static int power7_compute_mmcr(u64 event[], int n_ev,
			       unsigned int hwc[], unsigned long mmcr[])
{
	unsigned long mmcr1 = 0;
	unsigned long mmcra = MMCRA_SDAR_DCACHE_MISS | MMCRA_SDAR_ERAT_MISS;
	unsigned int pmc, unit, combine, l2sel, psel;
	unsigned int pmc_inuse = 0;
	int i;

	
	for (i = 0; i < n_ev; ++i) {
		pmc = (event[i] >> PM_PMC_SH) & PM_PMC_MSK;
		if (pmc) {
			if (pmc > 6)
				return -1;
			if (pmc_inuse & (1 << (pmc - 1)))
				return -1;
			pmc_inuse |= 1 << (pmc - 1);
		}
	}

	
	for (i = 0; i < n_ev; ++i) {
		pmc = (event[i] >> PM_PMC_SH) & PM_PMC_MSK;
		unit = (event[i] >> PM_UNIT_SH) & PM_UNIT_MSK;
		combine = (event[i] >> PM_COMBINE_SH) & PM_COMBINE_MSK;
		l2sel = (event[i] >> PM_L2SEL_SH) & PM_L2SEL_MSK;
		psel = event[i] & PM_PMCSEL_MSK;
		if (!pmc) {
			
			for (pmc = 0; pmc < 4; ++pmc) {
				if (!(pmc_inuse & (1 << pmc)))
					break;
			}
			if (pmc >= 4)
				return -1;
			pmc_inuse |= 1 << pmc;
		} else {
			
			--pmc;
		}
		if (pmc <= 3) {
			mmcr1 |= (unsigned long) unit
				<< (MMCR1_TTM0SEL_SH - 4 * pmc);
			mmcr1 |= (unsigned long) combine
				<< (MMCR1_PMC1_COMBINE_SH - pmc);
			mmcr1 |= psel << MMCR1_PMCSEL_SH(pmc);
			if (unit == 6)	
				mmcr1 |= (unsigned long) l2sel
					<< MMCR1_L2SEL_SH;
		}
		if (power7_marked_instr_event(event[i]))
			mmcra |= MMCRA_SAMPLE_ENABLE;
		hwc[i] = pmc;
	}

	
	mmcr[0] = 0;
	if (pmc_inuse & 1)
		mmcr[0] = MMCR0_PMC1CE;
	if (pmc_inuse & 0x3e)
		mmcr[0] |= MMCR0_PMCjCE;
	mmcr[1] = mmcr1;
	mmcr[2] = mmcra;
	return 0;
}

static void power7_disable_pmc(unsigned int pmc, unsigned long mmcr[])
{
	if (pmc <= 3)
		mmcr[1] &= ~(0xffUL << MMCR1_PMCSEL_SH(pmc));
}

static int power7_generic_events[] = {
	[PERF_COUNT_HW_CPU_CYCLES] = 0x1e,
	[PERF_COUNT_HW_STALLED_CYCLES_FRONTEND] = 0x100f8, 
	[PERF_COUNT_HW_STALLED_CYCLES_BACKEND] = 0x4000a,  
	[PERF_COUNT_HW_INSTRUCTIONS] = 2,
	[PERF_COUNT_HW_CACHE_REFERENCES] = 0xc880,	
	[PERF_COUNT_HW_CACHE_MISSES] = 0x400f0,		
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = 0x10068,	
	[PERF_COUNT_HW_BRANCH_MISSES] = 0x400f6,	
};

#define C(x)	PERF_COUNT_HW_CACHE_##x

static int power7_cache_events[C(MAX)][C(OP_MAX)][C(RESULT_MAX)] = {
	[C(L1D)] = {		
		[C(OP_READ)] = {	0xc880,		0x400f0	},
		[C(OP_WRITE)] = {	0,		0x300f0	},
		[C(OP_PREFETCH)] = {	0xd8b8,		0	},
	},
	[C(L1I)] = {		
		[C(OP_READ)] = {	0,		0x200fc	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	0x408a,		0	},
	},
	[C(LL)] = {		
		[C(OP_READ)] = {	0x16080,	0x26080	},
		[C(OP_WRITE)] = {	0x16082,	0x26082	},
		[C(OP_PREFETCH)] = {	0,		0	},
	},
	[C(DTLB)] = {		
		[C(OP_READ)] = {	0,		0x300fc	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(ITLB)] = {		
		[C(OP_READ)] = {	0,		0x400fc	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(BPU)] = {		
		[C(OP_READ)] = {	0x10068,	0x400f6	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(NODE)] = {		
		[C(OP_READ)] = {	-1,		-1	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
};

static struct power_pmu power7_pmu = {
	.name			= "POWER7",
	.n_counter		= 6,
	.max_alternatives	= MAX_ALT + 1,
	.add_fields		= 0x1555ul,
	.test_adder		= 0x3000ul,
	.compute_mmcr		= power7_compute_mmcr,
	.get_constraint		= power7_get_constraint,
	.get_alternatives	= power7_get_alternatives,
	.disable_pmc		= power7_disable_pmc,
	.flags			= PPMU_ALT_SIPR,
	.n_generic		= ARRAY_SIZE(power7_generic_events),
	.generic_events		= power7_generic_events,
	.cache_events		= &power7_cache_events,
};

static int __init init_power7_pmu(void)
{
	if (!cur_cpu_spec->oprofile_cpu_type ||
	    strcmp(cur_cpu_spec->oprofile_cpu_type, "ppc64/power7"))
		return -ENODEV;

	return register_power_pmu(&power7_pmu);
}

early_initcall(init_power7_pmu);
