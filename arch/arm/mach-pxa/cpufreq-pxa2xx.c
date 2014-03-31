/*
 *  linux/arch/arm/mach-pxa/cpufreq-pxa2xx.c
 *
 *  Copyright (C) 2002,2003 Intrinsyc Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 *   31-Jul-2002 : Initial version [FB]
 *   29-Jan-2003 : added PXA255 support [FB]
 *   20-Apr-2003 : ported to v2.5 (Dustin McIntire, Sensoria Corp.)
 *
 * Note:
 *   This driver may change the memory bus clock rate, but will not do any
 *   platform specific access timing changes... for example if you have flash
 *   memory connected to CS0, you will need to register a platform specific
 *   notifier which will adjust the memory access strobes to maintain a
 *   minimum strobe width.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>

#include <mach/pxa2xx-regs.h>
#include <mach/smemc.h>

#ifdef DEBUG
static unsigned int freq_debug;
module_param(freq_debug, uint, 0);
MODULE_PARM_DESC(freq_debug, "Set the debug messages to on=1/off=0");
#else
#define freq_debug  0
#endif

static struct regulator *vcc_core;

static unsigned int pxa27x_maxfreq;
module_param(pxa27x_maxfreq, uint, 0);
MODULE_PARM_DESC(pxa27x_maxfreq, "Set the pxa27x maxfreq in MHz"
		 "(typically 624=>pxa270, 416=>pxa271, 520=>pxa272)");

typedef struct {
	unsigned int khz;
	unsigned int membus;
	unsigned int cccr;
	unsigned int div2;
	unsigned int cclkcfg;
	int vmin;
	int vmax;
} pxa_freqs_t;

#define SDRAM_TREF	64	
static unsigned int sdram_rows;

#define CCLKCFG_TURBO		0x1
#define CCLKCFG_FCS		0x2
#define CCLKCFG_HALFTURBO	0x4
#define CCLKCFG_FASTBUS		0x8
#define MDREFR_DB2_MASK		(MDREFR_K2DB2 | MDREFR_K1DB2)
#define MDREFR_DRI_MASK		0xFFF

#define MDCNFG_DRAC2(mdcnfg) (((mdcnfg) >> 21) & 0x3)
#define MDCNFG_DRAC0(mdcnfg) (((mdcnfg) >> 5) & 0x3)

#define CCLKCFG			CCLKCFG_TURBO | CCLKCFG_FCS

static pxa_freqs_t pxa255_run_freqs[] =
{
	
	{ 99500,  99500, 0x121, 1,  CCLKCFG, -1, -1},	
	{132700, 132700, 0x123, 1,  CCLKCFG, -1, -1},	
	{199100,  99500, 0x141, 0,  CCLKCFG, -1, -1},	
	{265400, 132700, 0x143, 1,  CCLKCFG, -1, -1},	
	{331800, 165900, 0x145, 1,  CCLKCFG, -1, -1},	
	{398100,  99500, 0x161, 0,  CCLKCFG, -1, -1},	
};

static pxa_freqs_t pxa255_turbo_freqs[] =
{
	
	{ 99500, 99500,  0x121, 1,  CCLKCFG, -1, -1},	
	{199100, 99500,  0x221, 0,  CCLKCFG, -1, -1},	
	{298500, 99500,  0x321, 0,  CCLKCFG, -1, -1},	
	{298600, 99500,  0x1c1, 0,  CCLKCFG, -1, -1},	
	{398100, 99500,  0x241, 0,  CCLKCFG, -1, -1},	
};

#define NUM_PXA25x_RUN_FREQS ARRAY_SIZE(pxa255_run_freqs)
#define NUM_PXA25x_TURBO_FREQS ARRAY_SIZE(pxa255_turbo_freqs)

static struct cpufreq_frequency_table
	pxa255_run_freq_table[NUM_PXA25x_RUN_FREQS+1];
static struct cpufreq_frequency_table
	pxa255_turbo_freq_table[NUM_PXA25x_TURBO_FREQS+1];

static unsigned int pxa255_turbo_table;
module_param(pxa255_turbo_table, uint, 0);
MODULE_PARM_DESC(pxa255_turbo_table, "Selects the frequency table (0 = run table, !0 = turbo table)");

#define PXA27x_CCCR(A, L, N2) (A << 25 | N2 << 7 | L)
#define CCLKCFG2(B, HT, T) \
  (CCLKCFG_FCS | \
   ((B)  ? CCLKCFG_FASTBUS : 0) | \
   ((HT) ? CCLKCFG_HALFTURBO : 0) | \
   ((T)  ? CCLKCFG_TURBO : 0))

static pxa_freqs_t pxa27x_freqs[] = {
	{104000, 104000, PXA27x_CCCR(1,	 8, 2), 0, CCLKCFG2(1, 0, 1),  900000, 1705000 },
	{156000, 104000, PXA27x_CCCR(1,	 8, 3), 0, CCLKCFG2(1, 0, 1), 1000000, 1705000 },
	{208000, 208000, PXA27x_CCCR(0, 16, 2), 1, CCLKCFG2(0, 0, 1), 1180000, 1705000 },
	{312000, 208000, PXA27x_CCCR(1, 16, 3), 1, CCLKCFG2(1, 0, 1), 1250000, 1705000 },
	{416000, 208000, PXA27x_CCCR(1, 16, 4), 1, CCLKCFG2(1, 0, 1), 1350000, 1705000 },
	{520000, 208000, PXA27x_CCCR(1, 16, 5), 1, CCLKCFG2(1, 0, 1), 1450000, 1705000 },
	{624000, 208000, PXA27x_CCCR(1, 16, 6), 1, CCLKCFG2(1, 0, 1), 1550000, 1705000 }
};

#define NUM_PXA27x_FREQS ARRAY_SIZE(pxa27x_freqs)
static struct cpufreq_frequency_table
	pxa27x_freq_table[NUM_PXA27x_FREQS+1];

extern unsigned get_clk_frequency_khz(int info);

#ifdef CONFIG_REGULATOR

static int pxa_cpufreq_change_voltage(pxa_freqs_t *pxa_freq)
{
	int ret = 0;
	int vmin, vmax;

	if (!cpu_is_pxa27x())
		return 0;

	vmin = pxa_freq->vmin;
	vmax = pxa_freq->vmax;
	if ((vmin == -1) || (vmax == -1))
		return 0;

	ret = regulator_set_voltage(vcc_core, vmin, vmax);
	if (ret)
		pr_err("cpufreq: Failed to set vcc_core in [%dmV..%dmV]\n",
		       vmin, vmax);
	return ret;
}

static __init void pxa_cpufreq_init_voltages(void)
{
	vcc_core = regulator_get(NULL, "vcc_core");
	if (IS_ERR(vcc_core)) {
		pr_info("cpufreq: Didn't find vcc_core regulator\n");
		vcc_core = NULL;
	} else {
		pr_info("cpufreq: Found vcc_core regulator\n");
	}
}
#else
static int pxa_cpufreq_change_voltage(pxa_freqs_t *pxa_freq)
{
	return 0;
}

static __init void pxa_cpufreq_init_voltages(void) { }
#endif

static void find_freq_tables(struct cpufreq_frequency_table **freq_table,
			     pxa_freqs_t **pxa_freqs)
{
	if (cpu_is_pxa25x()) {
		if (!pxa255_turbo_table) {
			*pxa_freqs = pxa255_run_freqs;
			*freq_table = pxa255_run_freq_table;
		} else {
			*pxa_freqs = pxa255_turbo_freqs;
			*freq_table = pxa255_turbo_freq_table;
		}
	}
	if (cpu_is_pxa27x()) {
		*pxa_freqs = pxa27x_freqs;
		*freq_table = pxa27x_freq_table;
	}
}

static void pxa27x_guess_max_freq(void)
{
	if (!pxa27x_maxfreq) {
		pxa27x_maxfreq = 416000;
		printk(KERN_INFO "PXA CPU 27x max frequency not defined "
		       "(pxa27x_maxfreq), assuming pxa271 with %dkHz maxfreq\n",
		       pxa27x_maxfreq);
	} else {
		pxa27x_maxfreq *= 1000;
	}
}

static void init_sdram_rows(void)
{
	uint32_t mdcnfg = __raw_readl(MDCNFG);
	unsigned int drac2 = 0, drac0 = 0;

	if (mdcnfg & (MDCNFG_DE2 | MDCNFG_DE3))
		drac2 = MDCNFG_DRAC2(mdcnfg);

	if (mdcnfg & (MDCNFG_DE0 | MDCNFG_DE1))
		drac0 = MDCNFG_DRAC0(mdcnfg);

	sdram_rows = 1 << (11 + max(drac0, drac2));
}

static u32 mdrefr_dri(unsigned int freq)
{
	u32 interval = freq * SDRAM_TREF / sdram_rows;

	return (interval - (cpu_is_pxa27x() ? 31 : 0)) / 32;
}

static int pxa_verify_policy(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *pxa_freqs_table;
	pxa_freqs_t *pxa_freqs;
	int ret;

	find_freq_tables(&pxa_freqs_table, &pxa_freqs);
	ret = cpufreq_frequency_table_verify(policy, pxa_freqs_table);

	if (freq_debug)
		pr_debug("Verified CPU policy: %dKhz min to %dKhz max\n",
			 policy->min, policy->max);

	return ret;
}

static unsigned int pxa_cpufreq_get(unsigned int cpu)
{
	return get_clk_frequency_khz(0);
}

static int pxa_set_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_frequency_table *pxa_freqs_table;
	pxa_freqs_t *pxa_freq_settings;
	struct cpufreq_freqs freqs;
	unsigned int idx;
	unsigned long flags;
	unsigned int new_freq_cpu, new_freq_mem;
	unsigned int unused, preset_mdrefr, postset_mdrefr, cclkcfg;
	int ret = 0;

	
	find_freq_tables(&pxa_freqs_table, &pxa_freq_settings);

	
	if (cpufreq_frequency_table_target(policy, pxa_freqs_table,
					   target_freq, relation, &idx)) {
		return -EINVAL;
	}

	new_freq_cpu = pxa_freq_settings[idx].khz;
	new_freq_mem = pxa_freq_settings[idx].membus;
	freqs.old = policy->cur;
	freqs.new = new_freq_cpu;
	freqs.cpu = policy->cpu;

	if (freq_debug)
		pr_debug("Changing CPU frequency to %d Mhz, (SDRAM %d Mhz)\n",
			 freqs.new / 1000, (pxa_freq_settings[idx].div2) ?
			 (new_freq_mem / 2000) : (new_freq_mem / 1000));

	if (vcc_core && freqs.new > freqs.old)
		ret = pxa_cpufreq_change_voltage(&pxa_freq_settings[idx]);
	if (ret)
		return ret;
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	preset_mdrefr = postset_mdrefr = __raw_readl(MDREFR);
	if ((preset_mdrefr & MDREFR_DRI_MASK) > mdrefr_dri(new_freq_mem)) {
		preset_mdrefr = (preset_mdrefr & ~MDREFR_DRI_MASK);
		preset_mdrefr |= mdrefr_dri(new_freq_mem);
	}
	postset_mdrefr =
		(postset_mdrefr & ~MDREFR_DRI_MASK) | mdrefr_dri(new_freq_mem);

	if (pxa_freq_settings[idx].div2) {
		preset_mdrefr  |= MDREFR_DB2_MASK;
		postset_mdrefr |= MDREFR_DB2_MASK;
	} else {
		postset_mdrefr &= ~MDREFR_DB2_MASK;
	}

	local_irq_save(flags);

	
	CCCR = pxa_freq_settings[idx].cccr;
	cclkcfg = pxa_freq_settings[idx].cclkcfg;

	asm volatile("							\n\
		ldr	r4, [%1]		/* load MDREFR */	\n\
		b	2f						\n\
		.align	5						\n\
1:									\n\
		str	%3, [%1]		/* preset the MDREFR */	\n\
		mcr	p14, 0, %2, c6, c0, 0	/* set CCLKCFG[FCS] */	\n\
		str	%4, [%1]		/* postset the MDREFR */ \n\
									\n\
		b	3f						\n\
2:		b	1b						\n\
3:		nop							\n\
	  "
		     : "=&r" (unused)
		     : "r" (MDREFR), "r" (cclkcfg),
		       "r" (preset_mdrefr), "r" (postset_mdrefr)
		     : "r4", "r5");
	local_irq_restore(flags);

	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	if (vcc_core && freqs.new < freqs.old)
		ret = pxa_cpufreq_change_voltage(&pxa_freq_settings[idx]);

	return 0;
}

static int pxa_cpufreq_init(struct cpufreq_policy *policy)
{
	int i;
	unsigned int freq;
	struct cpufreq_frequency_table *pxa255_freq_table;
	pxa_freqs_t *pxa255_freqs;

	
	if (cpu_is_pxa27x())
		pxa27x_guess_max_freq();

	pxa_cpufreq_init_voltages();

	init_sdram_rows();

	
	policy->cpuinfo.transition_latency = 1000; 
	policy->cur = get_clk_frequency_khz(0);	   
	policy->min = policy->max = policy->cur;

	
	for (i = 0; i < NUM_PXA25x_RUN_FREQS; i++) {
		pxa255_run_freq_table[i].frequency = pxa255_run_freqs[i].khz;
		pxa255_run_freq_table[i].index = i;
	}
	pxa255_run_freq_table[i].frequency = CPUFREQ_TABLE_END;

	
	for (i = 0; i < NUM_PXA25x_TURBO_FREQS; i++) {
		pxa255_turbo_freq_table[i].frequency =
			pxa255_turbo_freqs[i].khz;
		pxa255_turbo_freq_table[i].index = i;
	}
	pxa255_turbo_freq_table[i].frequency = CPUFREQ_TABLE_END;

	pxa255_turbo_table = !!pxa255_turbo_table;

	
	for (i = 0; i < NUM_PXA27x_FREQS; i++) {
		freq = pxa27x_freqs[i].khz;
		if (freq > pxa27x_maxfreq)
			break;
		pxa27x_freq_table[i].frequency = freq;
		pxa27x_freq_table[i].index = i;
	}
	pxa27x_freq_table[i].index = i;
	pxa27x_freq_table[i].frequency = CPUFREQ_TABLE_END;

	if (cpu_is_pxa25x()) {
		find_freq_tables(&pxa255_freq_table, &pxa255_freqs);
		pr_info("PXA255 cpufreq using %s frequency table\n",
			pxa255_turbo_table ? "turbo" : "run");
		cpufreq_frequency_table_cpuinfo(policy, pxa255_freq_table);
	}
	else if (cpu_is_pxa27x())
		cpufreq_frequency_table_cpuinfo(policy, pxa27x_freq_table);

	printk(KERN_INFO "PXA CPU frequency change support initialized\n");

	return 0;
}

static struct cpufreq_driver pxa_cpufreq_driver = {
	.verify	= pxa_verify_policy,
	.target	= pxa_set_target,
	.init	= pxa_cpufreq_init,
	.get	= pxa_cpufreq_get,
	.name	= "PXA2xx",
};

static int __init pxa_cpu_init(void)
{
	int ret = -ENODEV;
	if (cpu_is_pxa25x() || cpu_is_pxa27x())
		ret = cpufreq_register_driver(&pxa_cpufreq_driver);
	return ret;
}

static void __exit pxa_cpu_exit(void)
{
	cpufreq_unregister_driver(&pxa_cpufreq_driver);
}


MODULE_AUTHOR("Intrinsyc Software Inc.");
MODULE_DESCRIPTION("CPU frequency changing driver for the PXA architecture");
MODULE_LICENSE("GPL");
module_init(pxa_cpu_init);
module_exit(pxa_cpu_exit);
