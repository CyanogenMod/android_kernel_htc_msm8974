/*
 * Performance counter support for POWER4 (GP) and POWER4+ (GQ) processors.
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

#define PM_PMC_SH	12	
#define PM_PMC_MSK	0xf
#define PM_UNIT_SH	8	
#define PM_UNIT_MSK	0xf
#define PM_LOWER_SH	6
#define PM_LOWER_MSK	1
#define PM_LOWER_MSKS	0x40
#define PM_BYTE_SH	4	
#define PM_BYTE_MSK	3
#define PM_PMCSEL_MSK	7

#define PM_FPU		1
#define PM_ISU1		2
#define PM_IFU		3
#define PM_IDU0		4
#define PM_ISU1_ALT	6
#define PM_ISU2		7
#define PM_IFU_ALT	8
#define PM_LSU0		9
#define PM_LSU1		0xc
#define PM_GPS		0xf

#define MMCR0_PMC1SEL_SH	8
#define MMCR0_PMC2SEL_SH	1
#define MMCR_PMCSEL_MSK		0x1f

#define MMCR1_TTM0SEL_SH	62
#define MMCR1_TTC0SEL_SH	61
#define MMCR1_TTM1SEL_SH	59
#define MMCR1_TTC1SEL_SH	58
#define MMCR1_TTM2SEL_SH	56
#define MMCR1_TTC2SEL_SH	55
#define MMCR1_TTM3SEL_SH	53
#define MMCR1_TTC3SEL_SH	52
#define MMCR1_TTMSEL_MSK	3
#define MMCR1_TD_CP_DBG0SEL_SH	50
#define MMCR1_TD_CP_DBG1SEL_SH	48
#define MMCR1_TD_CP_DBG2SEL_SH	46
#define MMCR1_TD_CP_DBG3SEL_SH	44
#define MMCR1_DEBUG0SEL_SH	43
#define MMCR1_DEBUG1SEL_SH	42
#define MMCR1_DEBUG2SEL_SH	41
#define MMCR1_DEBUG3SEL_SH	40
#define MMCR1_PMC1_ADDER_SEL_SH	39
#define MMCR1_PMC2_ADDER_SEL_SH	38
#define MMCR1_PMC6_ADDER_SEL_SH	37
#define MMCR1_PMC5_ADDER_SEL_SH	36
#define MMCR1_PMC8_ADDER_SEL_SH	35
#define MMCR1_PMC7_ADDER_SEL_SH	34
#define MMCR1_PMC3_ADDER_SEL_SH	33
#define MMCR1_PMC4_ADDER_SEL_SH	32
#define MMCR1_PMC3SEL_SH	27
#define MMCR1_PMC4SEL_SH	22
#define MMCR1_PMC5SEL_SH	17
#define MMCR1_PMC6SEL_SH	12
#define MMCR1_PMC7SEL_SH	7
#define MMCR1_PMC8SEL_SH	2	

static short mmcr1_adder_bits[8] = {
	MMCR1_PMC1_ADDER_SEL_SH,
	MMCR1_PMC2_ADDER_SEL_SH,
	MMCR1_PMC3_ADDER_SEL_SH,
	MMCR1_PMC4_ADDER_SEL_SH,
	MMCR1_PMC5_ADDER_SEL_SH,
	MMCR1_PMC6_ADDER_SEL_SH,
	MMCR1_PMC7_ADDER_SEL_SH,
	MMCR1_PMC8_ADDER_SEL_SH
};

#define MMCRA_PMC8SEL0_SH	17	


static struct unitinfo {
	unsigned long	value, mask;
	int		unit;
	int		lowerbit;
} p4_unitinfo[16] = {
	[PM_FPU]  = { 0x44000000000000ul, 0x88000000000000ul, PM_FPU, 0 },
	[PM_ISU1] = { 0x20080000000000ul, 0x88000000000000ul, PM_ISU1, 0 },
	[PM_ISU1_ALT] =
		    { 0x20080000000000ul, 0x88000000000000ul, PM_ISU1, 0 },
	[PM_IFU]  = { 0x02200000000000ul, 0x08820000000000ul, PM_IFU, 41 },
	[PM_IFU_ALT] =
		    { 0x02200000000000ul, 0x08820000000000ul, PM_IFU, 41 },
	[PM_IDU0] = { 0x10100000000000ul, 0x80840000000000ul, PM_IDU0, 1 },
	[PM_ISU2] = { 0x10140000000000ul, 0x80840000000000ul, PM_ISU2, 0 },
	[PM_LSU0] = { 0x01400000000000ul, 0x08800000000000ul, PM_LSU0, 0 },
	[PM_LSU1] = { 0x00000000000000ul, 0x00010000000000ul, PM_LSU1, 40 },
	[PM_GPS]  = { 0x00000000000000ul, 0x00000000000000ul, PM_GPS, 0 }
};

static unsigned char direct_marked_event[8] = {
	(1<<2) | (1<<3),	
	(1<<3) | (1<<5),	
	(1<<3),			
	(1<<4) | (1<<5),	
	(1<<4) | (1<<5),	
	(1<<3) | (1<<4) | (1<<5),
		
	(1<<4) | (1<<5),	
	(1<<4),			
};

static int p4_marked_instr_event(u64 event)
{
	int pmc, psel, unit, byte, bit;
	unsigned int mask;

	pmc = (event >> PM_PMC_SH) & PM_PMC_MSK;
	psel = event & PM_PMCSEL_MSK;
	if (pmc) {
		if (direct_marked_event[pmc - 1] & (1 << psel))
			return 1;
		if (psel == 0)		
			bit = (pmc <= 4)? pmc - 1: 8 - pmc;
		else if (psel == 6)	
			bit = 4;
		else
			return 0;
	} else
		bit = psel;

	byte = (event >> PM_BYTE_SH) & PM_BYTE_MSK;
	unit = (event >> PM_UNIT_SH) & PM_UNIT_MSK;
	mask = 0;
	switch (unit) {
	case PM_LSU1:
		if (event & PM_LOWER_MSKS)
			mask = 1 << 28;		
		else
			mask = 6 << 24;		
		break;
	case PM_LSU0:
		
		mask = 0x083dff00;
	}
	return (mask >> (byte * 8 + bit)) & 1;
}

static int p4_get_constraint(u64 event, unsigned long *maskp,
			     unsigned long *valp)
{
	int pmc, byte, unit, lower, sh;
	unsigned long mask = 0, value = 0;
	int grp = -1;

	pmc = (event >> PM_PMC_SH) & PM_PMC_MSK;
	if (pmc) {
		if (pmc > 8)
			return -1;
		sh = (pmc - 1) * 2;
		mask |= 2 << sh;
		value |= 1 << sh;
		grp = ((pmc - 1) >> 1) & 1;
	}
	unit = (event >> PM_UNIT_SH) & PM_UNIT_MSK;
	byte = (event >> PM_BYTE_SH) & PM_BYTE_MSK;
	if (unit) {
		lower = (event >> PM_LOWER_SH) & PM_LOWER_MSK;

		if (!pmc)
			grp = byte & 1;

		if (!p4_unitinfo[unit].unit)
			return -1;
		mask  |= p4_unitinfo[unit].mask;
		value |= p4_unitinfo[unit].value;
		sh = p4_unitinfo[unit].lowerbit;
		if (sh > 1)
			value |= (unsigned long)lower << sh;
		else if (lower != sh)
			return -1;
		unit = p4_unitinfo[unit].unit;

		
		mask  |= 0xfULL << (28 - 4 * byte);
		value |= (unsigned long)unit << (28 - 4 * byte);
	}
	if (grp == 0) {
		
		mask  |= 0x8000000000ull;
		value |= 0x1000000000ull;
	} else {
		
		mask  |= 0x800000000ull;
		value |= 0x100000000ull;
	}

	
	if (p4_marked_instr_event(event)) {
		mask  |= 1ull << 56;
		value |= 1ull << 56;
	}

	
	if (pmc && (event & PM_PMCSEL_MSK) == 6 && byte == 2)
		mask  |= 1ull << 56;

	*maskp = mask;
	*valp = value;
	return 0;
}

static unsigned int ppc_inst_cmpl[] = {
	0x1001, 0x4001, 0x6001, 0x7001, 0x8001
};

static int p4_get_alternatives(u64 event, unsigned int flags, u64 alt[])
{
	int i, j, na;

	alt[0] = event;
	na = 1;

	
	if (event == 0x8003 || event == 0x0224) {
		alt[1] = event ^ (0x8003 ^ 0x0224);
		return 2;
	}

	
	if (event == 0x0c13 || event == 0x0c23) {
		alt[1] = event ^ (0x0c13 ^ 0x0c23);
		return 2;
	}

	
	for (i = 0; i < ARRAY_SIZE(ppc_inst_cmpl); ++i) {
		if (event == ppc_inst_cmpl[i]) {
			for (j = 0; j < ARRAY_SIZE(ppc_inst_cmpl); ++j)
				if (j != i)
					alt[na++] = ppc_inst_cmpl[j];
			break;
		}
	}

	return na;
}

static int p4_compute_mmcr(u64 event[], int n_ev,
			   unsigned int hwc[], unsigned long mmcr[])
{
	unsigned long mmcr0 = 0, mmcr1 = 0, mmcra = 0;
	unsigned int pmc, unit, byte, psel, lower;
	unsigned int ttm, grp;
	unsigned int pmc_inuse = 0;
	unsigned int pmc_grp_use[2];
	unsigned char busbyte[4];
	unsigned char unituse[16];
	unsigned int unitlower = 0;
	int i;

	if (n_ev > 8)
		return -1;

	
	pmc_grp_use[0] = pmc_grp_use[1] = 0;
	memset(busbyte, 0, sizeof(busbyte));
	memset(unituse, 0, sizeof(unituse));
	for (i = 0; i < n_ev; ++i) {
		pmc = (event[i] >> PM_PMC_SH) & PM_PMC_MSK;
		if (pmc) {
			if (pmc_inuse & (1 << (pmc - 1)))
				return -1;
			pmc_inuse |= 1 << (pmc - 1);
			
			++pmc_grp_use[((pmc - 1) >> 1) & 1];
		}
		unit = (event[i] >> PM_UNIT_SH) & PM_UNIT_MSK;
		byte = (event[i] >> PM_BYTE_SH) & PM_BYTE_MSK;
		lower = (event[i] >> PM_LOWER_SH) & PM_LOWER_MSK;
		if (unit) {
			if (!pmc)
				++pmc_grp_use[byte & 1];
			if (unit == 6 || unit == 8)
				
				unit = (unit >> 1) - 1;
			if (busbyte[byte] && busbyte[byte] != unit)
				return -1;
			busbyte[byte] = unit;
			lower <<= unit;
			if (unituse[unit] && lower != (unitlower & lower))
				return -1;
			unituse[unit] = 1;
			unitlower |= lower;
		}
	}
	if (pmc_grp_use[0] > 4 || pmc_grp_use[1] > 4)
		return -1;

	if (unituse[2] & (unituse[1] | (unituse[3] & unituse[9]))) {
		unituse[6] = 1;		
		unituse[2] = 0;
	}
	if (unituse[3] & (unituse[1] | unituse[2])) {
		unituse[8] = 1;		
		unituse[3] = 0;
		unitlower = (unitlower & ~8) | ((unitlower & 8) << 5);
	}
	
	if (unituse[1] + unituse[2] + unituse[3] > 1 ||
	    unituse[4] + unituse[6] + unituse[7] > 1 ||
	    unituse[8] + unituse[9] > 1 ||
	    (unituse[5] | unituse[10] | unituse[11] |
	     unituse[13] | unituse[14]))
		return -1;

	
	mmcr1 |= (unsigned long)(unituse[3] * 2 + unituse[2])
		<< MMCR1_TTM0SEL_SH;
	mmcr1 |= (unsigned long)(unituse[7] * 3 + unituse[6] * 2)
		<< MMCR1_TTM1SEL_SH;
	mmcr1 |= (unsigned long)unituse[9] << MMCR1_TTM2SEL_SH;

	
	if (unitlower & 0xe)
		mmcr1 |= 1ull << MMCR1_TTC0SEL_SH;
	if (unitlower & 0xf0)
		mmcr1 |= 1ull << MMCR1_TTC1SEL_SH;
	if (unitlower & 0xf00)
		mmcr1 |= 1ull << MMCR1_TTC2SEL_SH;
	if (unitlower & 0x7000)
		mmcr1 |= 1ull << MMCR1_TTC3SEL_SH;

	
	for (byte = 0; byte < 4; ++byte) {
		unit = busbyte[byte];
		if (!unit)
			continue;
		if (unit == 0xf) {
			
			mmcr1 |= 1ull << (MMCR1_DEBUG0SEL_SH - byte);
		} else {
			if (!unituse[unit])
				ttm = unit - 1;		
			else
				ttm = unit >> 2;
			mmcr1 |= (unsigned long)ttm
				<< (MMCR1_TD_CP_DBG0SEL_SH - 2 * byte);
		}
	}

	
	for (i = 0; i < n_ev; ++i) {
		pmc = (event[i] >> PM_PMC_SH) & PM_PMC_MSK;
		unit = (event[i] >> PM_UNIT_SH) & PM_UNIT_MSK;
		byte = (event[i] >> PM_BYTE_SH) & PM_BYTE_MSK;
		psel = event[i] & PM_PMCSEL_MSK;
		if (!pmc) {
			
			if (unit)
				psel |= 0x10 | ((byte & 2) << 2);
			for (pmc = 0; pmc < 8; ++pmc) {
				if (pmc_inuse & (1 << pmc))
					continue;
				grp = (pmc >> 1) & 1;
				if (unit) {
					if (grp == (byte & 1))
						break;
				} else if (pmc_grp_use[grp] < 4) {
					++pmc_grp_use[grp];
					break;
				}
			}
			pmc_inuse |= 1 << pmc;
		} else {
			
			--pmc;
			if (psel == 0 && (byte & 2))
				
				mmcr1 |= 1ull << mmcr1_adder_bits[pmc];
			else if (psel == 6 && byte == 3)
				
				mmcra |= MMCRA_SAMPLE_ENABLE;
			psel |= 8;
		}
		if (pmc <= 1)
			mmcr0 |= psel << (MMCR0_PMC1SEL_SH - 7 * pmc);
		else
			mmcr1 |= psel << (MMCR1_PMC3SEL_SH - 5 * (pmc - 2));
		if (pmc == 7)	
			mmcra |= (psel & 1) << MMCRA_PMC8SEL0_SH;
		hwc[i] = pmc;
		if (p4_marked_instr_event(event[i]))
			mmcra |= MMCRA_SAMPLE_ENABLE;
	}

	if (pmc_inuse & 1)
		mmcr0 |= MMCR0_PMC1CE;
	if (pmc_inuse & 0xfe)
		mmcr0 |= MMCR0_PMCjCE;

	mmcra |= 0x2000;	

	
	mmcr[0] = mmcr0;
	mmcr[1] = mmcr1;
	mmcr[2] = mmcra;
	return 0;
}

static void p4_disable_pmc(unsigned int pmc, unsigned long mmcr[])
{
	if (pmc <= 1) {
		mmcr[0] &= ~(0x1fUL << (MMCR0_PMC1SEL_SH - 7 * pmc));
	} else {
		mmcr[1] &= ~(0x1fUL << (MMCR1_PMC3SEL_SH - 5 * (pmc - 2)));
		if (pmc == 7)
			mmcr[2] &= ~(1UL << MMCRA_PMC8SEL0_SH);
	}
}

static int p4_generic_events[] = {
	[PERF_COUNT_HW_CPU_CYCLES]		= 7,
	[PERF_COUNT_HW_INSTRUCTIONS]		= 0x1001,
	[PERF_COUNT_HW_CACHE_REFERENCES]	= 0x8c10, 
	[PERF_COUNT_HW_CACHE_MISSES]		= 0x3c10, 
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS]	= 0x330,  
	[PERF_COUNT_HW_BRANCH_MISSES]		= 0x331,  
};

#define C(x)	PERF_COUNT_HW_CACHE_##x

static int power4_cache_events[C(MAX)][C(OP_MAX)][C(RESULT_MAX)] = {
	[C(L1D)] = {		
		[C(OP_READ)] = {	0x8c10,		0x3c10	},
		[C(OP_WRITE)] = {	0x7c10,		0xc13	},
		[C(OP_PREFETCH)] = {	0xc35,		0	},
	},
	[C(L1I)] = {		
		[C(OP_READ)] = {	0,		0	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	0,		0	},
	},
	[C(LL)] = {		
		[C(OP_READ)] = {	0,		0	},
		[C(OP_WRITE)] = {	0,		0	},
		[C(OP_PREFETCH)] = {	0xc34,		0	},
	},
	[C(DTLB)] = {		
		[C(OP_READ)] = {	0,		0x904	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(ITLB)] = {		
		[C(OP_READ)] = {	0,		0x900	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(BPU)] = {		
		[C(OP_READ)] = {	0x330,		0x331	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(NODE)] = {		
		[C(OP_READ)] = {	-1,		-1	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
};

static struct power_pmu power4_pmu = {
	.name			= "POWER4/4+",
	.n_counter		= 8,
	.max_alternatives	= 5,
	.add_fields		= 0x0000001100005555ul,
	.test_adder		= 0x0011083300000000ul,
	.compute_mmcr		= p4_compute_mmcr,
	.get_constraint		= p4_get_constraint,
	.get_alternatives	= p4_get_alternatives,
	.disable_pmc		= p4_disable_pmc,
	.n_generic		= ARRAY_SIZE(p4_generic_events),
	.generic_events		= p4_generic_events,
	.cache_events		= &power4_cache_events,
	.flags			= PPMU_NO_SIPR | PPMU_NO_CONT_SAMPLING,
};

static int __init init_power4_pmu(void)
{
	if (!cur_cpu_spec->oprofile_cpu_type ||
	    strcmp(cur_cpu_spec->oprofile_cpu_type, "ppc64/power4"))
		return -ENODEV;

	return register_power_pmu(&power4_pmu);
}

early_initcall(init_power4_pmu);
