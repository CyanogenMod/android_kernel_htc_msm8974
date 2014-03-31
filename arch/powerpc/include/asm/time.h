/*
 * Common time prototypes and such for all ppc machines.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu) to merge
 * Paul Mackerras' version and mine for PReP and Pmac.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __POWERPC_TIME_H
#define __POWERPC_TIME_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/percpu.h>

#include <asm/processor.h>

extern unsigned long tb_ticks_per_jiffy;
extern unsigned long tb_ticks_per_usec;
extern unsigned long tb_ticks_per_sec;

struct rtc_time;
extern void to_tm(int tim, struct rtc_time * tm);
extern void GregorianDay(struct rtc_time *tm);

extern void generic_calibrate_decr(void);

extern void set_dec_cpu6(unsigned int val);

extern unsigned long ppc_proc_freq;
#define DEFAULT_PROC_FREQ	(DEFAULT_TB_FREQ * 8)
extern unsigned long ppc_tb_freq;
#define DEFAULT_TB_FREQ		125000000UL

struct div_result {
	u64 result_high;
	u64 result_low;
};

#ifdef CONFIG_6xx
#define __USE_RTC()	(!cpu_has_feature(CPU_FTR_USE_TB))
#else
#define __USE_RTC()	0
#endif

#ifdef CONFIG_PPC64

#define get_tbl		get_tb

#else

static inline unsigned long get_tbl(void)
{
#if defined(CONFIG_403GCX)
	unsigned long tbl;
	asm volatile("mfspr %0, 0x3dd" : "=r" (tbl));
	return tbl;
#else
	return mftbl();
#endif
}

static inline unsigned int get_tbu(void)
{
#ifdef CONFIG_403GCX
	unsigned int tbu;
	asm volatile("mfspr %0, 0x3dc" : "=r" (tbu));
	return tbu;
#else
	return mftbu();
#endif
}
#endif 

static inline unsigned int get_rtcl(void)
{
	unsigned int rtcl;

	asm volatile("mfrtcl %0" : "=r" (rtcl));
	return rtcl;
}

static inline u64 get_rtc(void)
{
	unsigned int hi, lo, hi2;

	do {
		asm volatile("mfrtcu %0; mfrtcl %1; mfrtcu %2"
			     : "=r" (hi), "=r" (lo), "=r" (hi2));
	} while (hi2 != hi);
	return (u64)hi * 1000000000 + lo;
}

#ifdef CONFIG_PPC64
static inline u64 get_tb(void)
{
	return mftb();
}
#else 
static inline u64 get_tb(void)
{
	unsigned int tbhi, tblo, tbhi2;

	do {
		tbhi = get_tbu();
		tblo = get_tbl();
		tbhi2 = get_tbu();
	} while (tbhi != tbhi2);

	return ((u64)tbhi << 32) | tblo;
}
#endif 

static inline u64 get_tb_or_rtc(void)
{
	return __USE_RTC() ? get_rtc() : get_tb();
}

static inline void set_tb(unsigned int upper, unsigned int lower)
{
	mtspr(SPRN_TBWL, 0);
	mtspr(SPRN_TBWU, upper);
	mtspr(SPRN_TBWL, lower);
}

static inline unsigned int get_dec(void)
{
#if defined(CONFIG_40x)
	return (mfspr(SPRN_PIT));
#else
	return (mfspr(SPRN_DEC));
#endif
}

static inline void set_dec(int val)
{
#if defined(CONFIG_40x)
	mtspr(SPRN_PIT, val);
#elif defined(CONFIG_8xx_CPU6)
	set_dec_cpu6(val - 1);
#else
#ifndef CONFIG_BOOKE
	--val;
#endif
	mtspr(SPRN_DEC, val);
#endif 
}

static inline unsigned long tb_ticks_since(unsigned long tstamp)
{
	if (__USE_RTC()) {
		int delta = get_rtcl() - (unsigned int) tstamp;
		return delta < 0 ? delta + 1000000000 : delta;
	}
	return get_tbl() - tstamp;
}

#define mulhwu(x,y) \
({unsigned z; asm ("mulhwu %0,%1,%2" : "=r" (z) : "r" (x), "r" (y)); z;})

#ifdef CONFIG_PPC64
#define mulhdu(x,y) \
({unsigned long z; asm ("mulhdu %0,%1,%2" : "=r" (z) : "r" (x), "r" (y)); z;})
#else
extern u64 mulhdu(u64, u64);
#endif

extern void div128_by_32(u64 dividend_high, u64 dividend_low,
			 unsigned divisor, struct div_result *dr);


struct cpu_usage {
        u64 current_tb;  
};

DECLARE_PER_CPU(struct cpu_usage, cpu_usage_array);

#if defined(CONFIG_VIRT_CPU_ACCOUNTING)
#define account_process_vtime(tsk)		account_process_tick(tsk, 0)
#else
#define account_process_vtime(tsk)		do { } while (0)
#endif

extern void secondary_cpu_time_init(void);

DECLARE_PER_CPU(u64, decrementers_next_tb);

#endif 
#endif 
