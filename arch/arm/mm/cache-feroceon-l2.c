/*
 * arch/arm/mm/cache-feroceon-l2.c - Feroceon L2 cache controller support
 *
 * Copyright (C) 2008 Marvell Semiconductor
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * References:
 * - Unified Layer 2 Cache for Feroceon CPU Cores,
 *   Document ID MV-S104858-00, Rev. A, October 23 2007.
 */

#include <linux/init.h>
#include <linux/highmem.h>
#include <asm/cacheflush.h>
#include <asm/cp15.h>
#include <plat/cache-feroceon-l2.h>


static inline unsigned long l2_get_va(unsigned long paddr)
{
#ifdef CONFIG_HIGHMEM
	void *vaddr = kmap_atomic_pfn(paddr >> PAGE_SHIFT);
	return (unsigned long)vaddr + (paddr & ~PAGE_MASK);
#else
	return __phys_to_virt(paddr);
#endif
}

static inline void l2_put_va(unsigned long vaddr)
{
#ifdef CONFIG_HIGHMEM
	kunmap_atomic((void *)vaddr);
#endif
}

static inline void l2_clean_pa(unsigned long addr)
{
	__asm__("mcr p15, 1, %0, c15, c9, 3" : : "r" (addr));
}

static inline void l2_clean_pa_range(unsigned long start, unsigned long end)
{
	unsigned long va_start, va_end, flags;

	BUG_ON((start ^ end) >> PAGE_SHIFT);

	va_start = l2_get_va(start);
	va_end = va_start + (end - start);
	raw_local_irq_save(flags);
	__asm__("mcr p15, 1, %0, c15, c9, 4\n\t"
		"mcr p15, 1, %1, c15, c9, 5"
		: : "r" (va_start), "r" (va_end));
	raw_local_irq_restore(flags);
	l2_put_va(va_start);
}

static inline void l2_clean_inv_pa(unsigned long addr)
{
	__asm__("mcr p15, 1, %0, c15, c10, 3" : : "r" (addr));
}

static inline void l2_inv_pa(unsigned long addr)
{
	__asm__("mcr p15, 1, %0, c15, c11, 3" : : "r" (addr));
}

static inline void l2_inv_pa_range(unsigned long start, unsigned long end)
{
	unsigned long va_start, va_end, flags;

	BUG_ON((start ^ end) >> PAGE_SHIFT);

	va_start = l2_get_va(start);
	va_end = va_start + (end - start);
	raw_local_irq_save(flags);
	__asm__("mcr p15, 1, %0, c15, c11, 4\n\t"
		"mcr p15, 1, %1, c15, c11, 5"
		: : "r" (va_start), "r" (va_end));
	raw_local_irq_restore(flags);
	l2_put_va(va_start);
}

static inline void l2_inv_all(void)
{
	__asm__("mcr p15, 1, %0, c15, c11, 0" : : "r" (0));
}

#define CACHE_LINE_SIZE		32
#define MAX_RANGE_SIZE		1024

static int l2_wt_override;

static unsigned long calc_range_end(unsigned long start, unsigned long end)
{
	unsigned long range_end;

	BUG_ON(start & (CACHE_LINE_SIZE - 1));
	BUG_ON(end & (CACHE_LINE_SIZE - 1));

	range_end = end;

	if (range_end > start + MAX_RANGE_SIZE)
		range_end = start + MAX_RANGE_SIZE;

	if (range_end > (start | (PAGE_SIZE - 1)) + 1)
		range_end = (start | (PAGE_SIZE - 1)) + 1;

	return range_end;
}

static void feroceon_l2_inv_range(unsigned long start, unsigned long end)
{
	if (start & (CACHE_LINE_SIZE - 1)) {
		l2_clean_inv_pa(start & ~(CACHE_LINE_SIZE - 1));
		start = (start | (CACHE_LINE_SIZE - 1)) + 1;
	}

	if (start < end && end & (CACHE_LINE_SIZE - 1)) {
		l2_clean_inv_pa(end & ~(CACHE_LINE_SIZE - 1));
		end &= ~(CACHE_LINE_SIZE - 1);
	}

	while (start < end) {
		unsigned long range_end = calc_range_end(start, end);
		l2_inv_pa_range(start, range_end - CACHE_LINE_SIZE);
		start = range_end;
	}

	dsb();
}

static void feroceon_l2_clean_range(unsigned long start, unsigned long end)
{
	if (!l2_wt_override) {
		start &= ~(CACHE_LINE_SIZE - 1);
		end = (end + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
		while (start != end) {
			unsigned long range_end = calc_range_end(start, end);
			l2_clean_pa_range(start, range_end - CACHE_LINE_SIZE);
			start = range_end;
		}
	}

	dsb();
}

static void feroceon_l2_flush_range(unsigned long start, unsigned long end)
{
	start &= ~(CACHE_LINE_SIZE - 1);
	end = (end + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
	while (start != end) {
		unsigned long range_end = calc_range_end(start, end);
		if (!l2_wt_override)
			l2_clean_pa_range(start, range_end - CACHE_LINE_SIZE);
		l2_inv_pa_range(start, range_end - CACHE_LINE_SIZE);
		start = range_end;
	}

	dsb();
}


static int __init flush_and_disable_dcache(void)
{
	u32 cr;

	cr = get_cr();
	if (cr & CR_C) {
		unsigned long flags;

		raw_local_irq_save(flags);
		flush_cache_all();
		set_cr(cr & ~CR_C);
		raw_local_irq_restore(flags);
		return 1;
	}
	return 0;
}

static void __init enable_dcache(void)
{
	u32 cr;

	cr = get_cr();
	set_cr(cr | CR_C);
}

static void __init __invalidate_icache(void)
{
	__asm__("mcr p15, 0, %0, c7, c5, 0" : : "r" (0));
}

static int __init invalidate_and_disable_icache(void)
{
	u32 cr;

	cr = get_cr();
	if (cr & CR_I) {
		set_cr(cr & ~CR_I);
		__invalidate_icache();
		return 1;
	}
	return 0;
}

static void __init enable_icache(void)
{
	u32 cr;

	cr = get_cr();
	set_cr(cr | CR_I);
}

static inline u32 read_extra_features(void)
{
	u32 u;

	__asm__("mrc p15, 1, %0, c15, c1, 0" : "=r" (u));

	return u;
}

static inline void write_extra_features(u32 u)
{
	__asm__("mcr p15, 1, %0, c15, c1, 0" : : "r" (u));
}

static void __init disable_l2_prefetch(void)
{
	u32 u;

	u = read_extra_features();
	if (!(u & 0x01000000)) {
		printk(KERN_INFO "Feroceon L2: Disabling L2 prefetch.\n");
		write_extra_features(u | 0x01000000);
	}
}

static void __init enable_l2(void)
{
	u32 u;

	u = read_extra_features();
	if (!(u & 0x00400000)) {
		int i, d;

		printk(KERN_INFO "Feroceon L2: Enabling L2\n");

		d = flush_and_disable_dcache();
		i = invalidate_and_disable_icache();
		l2_inv_all();
		write_extra_features(u | 0x00400000);
		if (i)
			enable_icache();
		if (d)
			enable_dcache();
	}
}

void __init feroceon_l2_init(int __l2_wt_override)
{
	l2_wt_override = __l2_wt_override;

	disable_l2_prefetch();

	outer_cache.inv_range = feroceon_l2_inv_range;
	outer_cache.clean_range = feroceon_l2_clean_range;
	outer_cache.flush_range = feroceon_l2_flush_range;

	enable_l2();

	printk(KERN_INFO "Feroceon L2: Cache support initialised%s.\n",
			 l2_wt_override ? ", in WT override mode" : "");
}
