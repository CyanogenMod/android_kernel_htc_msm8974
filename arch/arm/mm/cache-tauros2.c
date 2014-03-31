/*
 * arch/arm/mm/cache-tauros2.c - Tauros2 L2 cache controller support
 *
 * Copyright (C) 2008 Marvell Semiconductor
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * References:
 * - PJ1 CPU Core Datasheet,
 *   Document ID MV-S104837-01, Rev 0.7, January 24 2008.
 * - PJ4 CPU Core Datasheet,
 *   Document ID MV-S105190-00, Rev 0.7, March 14 2008.
 */

#include <linux/init.h>
#include <asm/cacheflush.h>
#include <asm/cp15.h>
#include <asm/hardware/cache-tauros2.h>


#if __LINUX_ARM_ARCH__ < 7
static inline void tauros2_clean_pa(unsigned long addr)
{
	__asm__("mcr p15, 1, %0, c7, c11, 3" : : "r" (addr));
}

static inline void tauros2_clean_inv_pa(unsigned long addr)
{
	__asm__("mcr p15, 1, %0, c7, c15, 3" : : "r" (addr));
}

static inline void tauros2_inv_pa(unsigned long addr)
{
	__asm__("mcr p15, 1, %0, c7, c7, 3" : : "r" (addr));
}


#define CACHE_LINE_SIZE		32

static void tauros2_inv_range(unsigned long start, unsigned long end)
{
	if (start & (CACHE_LINE_SIZE - 1)) {
		tauros2_clean_inv_pa(start & ~(CACHE_LINE_SIZE - 1));
		start = (start | (CACHE_LINE_SIZE - 1)) + 1;
	}

	if (end & (CACHE_LINE_SIZE - 1)) {
		tauros2_clean_inv_pa(end & ~(CACHE_LINE_SIZE - 1));
		end &= ~(CACHE_LINE_SIZE - 1);
	}

	while (start < end) {
		tauros2_inv_pa(start);
		start += CACHE_LINE_SIZE;
	}

	dsb();
}

static void tauros2_clean_range(unsigned long start, unsigned long end)
{
	start &= ~(CACHE_LINE_SIZE - 1);
	while (start < end) {
		tauros2_clean_pa(start);
		start += CACHE_LINE_SIZE;
	}

	dsb();
}

static void tauros2_flush_range(unsigned long start, unsigned long end)
{
	start &= ~(CACHE_LINE_SIZE - 1);
	while (start < end) {
		tauros2_clean_inv_pa(start);
		start += CACHE_LINE_SIZE;
	}

	dsb();
}
#endif

static inline u32 __init read_extra_features(void)
{
	u32 u;

	__asm__("mrc p15, 1, %0, c15, c1, 0" : "=r" (u));

	return u;
}

static inline void __init write_extra_features(u32 u)
{
	__asm__("mcr p15, 1, %0, c15, c1, 0" : : "r" (u));
}

static void __init disable_l2_prefetch(void)
{
	u32 u;

	u = read_extra_features();
	if (!(u & 0x01000000)) {
		printk(KERN_INFO "Tauros2: Disabling L2 prefetch.\n");
		write_extra_features(u | 0x01000000);
	}
}

static inline int __init cpuid_scheme(void)
{
	extern int processor_id;

	return !!((processor_id & 0x000f0000) == 0x000f0000);
}

static inline u32 __init read_mmfr3(void)
{
	u32 mmfr3;

	__asm__("mrc p15, 0, %0, c0, c1, 7\n" : "=r" (mmfr3));

	return mmfr3;
}

static inline u32 __init read_actlr(void)
{
	u32 actlr;

	__asm__("mrc p15, 0, %0, c1, c0, 1\n" : "=r" (actlr));

	return actlr;
}

static inline void __init write_actlr(u32 actlr)
{
	__asm__("mcr p15, 0, %0, c1, c0, 1\n" : : "r" (actlr));
}

void __init tauros2_init(void)
{
	extern int processor_id;
	char *mode;

	disable_l2_prefetch();

#ifdef CONFIG_CPU_32v5
	if ((processor_id & 0xff0f0000) == 0x56050000) {
		u32 feat;

		feat = read_extra_features();
		if (!(feat & 0x00400000)) {
			printk(KERN_INFO "Tauros2: Enabling L2 cache.\n");
			write_extra_features(feat | 0x00400000);
		}

		mode = "ARMv5";
		outer_cache.inv_range = tauros2_inv_range;
		outer_cache.clean_range = tauros2_clean_range;
		outer_cache.flush_range = tauros2_flush_range;
	}
#endif

#ifdef CONFIG_CPU_32v6
	if (cpuid_scheme() && (read_mmfr3() & 0xf) == 0) {
		if (!(get_cr() & 0x04000000)) {
			printk(KERN_INFO "Tauros2: Enabling L2 cache.\n");
			adjust_cr(0x04000000, 0x04000000);
		}

		mode = "ARMv6";
		outer_cache.inv_range = tauros2_inv_range;
		outer_cache.clean_range = tauros2_clean_range;
		outer_cache.flush_range = tauros2_flush_range;
	}
#endif

#ifdef CONFIG_CPU_32v7
	if (cpuid_scheme() && (read_mmfr3() & 0xf) == 1) {
		u32 actlr;

		actlr = read_actlr();
		if (!(actlr & 0x00000002)) {
			printk(KERN_INFO "Tauros2: Enabling L2 cache.\n");
			write_actlr(actlr | 0x00000002);
		}

		mode = "ARMv7";
	}
#endif

	if (mode == NULL) {
		printk(KERN_CRIT "Tauros2: Unable to detect CPU mode.\n");
		return;
	}

	printk(KERN_INFO "Tauros2: L2 cache support initialised "
			 "in %s mode.\n", mode);
}
