/*
 *  linux/arch/arm/mm/copypage-xsc3.S
 *
 *  Copyright (C) 2004 Intel Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Adapted for 3rd gen XScale core, no more mini-dcache
 * Author: Matt Gilbert (matthew.m.gilbert@intel.com)
 */
#include <linux/init.h>
#include <linux/highmem.h>


static void __naked
xsc3_mc_copy_user_page(void *kto, const void *kfrom)
{
	asm("\
	stmfd	sp!, {r4, r5, lr}		\n\
	mov	lr, %2				\n\
						\n\
	pld	[r1, #0]			\n\
	pld	[r1, #32]			\n\
1:	pld	[r1, #64]			\n\
	pld	[r1, #96]			\n\
						\n\
2:	ldrd	r2, [r1], #8			\n\
	mov	ip, r0				\n\
	ldrd	r4, [r1], #8			\n\
	mcr	p15, 0, ip, c7, c6, 1		@ invalidate\n\
	strd	r2, [r0], #8			\n\
	ldrd	r2, [r1], #8			\n\
	strd	r4, [r0], #8			\n\
	ldrd	r4, [r1], #8			\n\
	strd	r2, [r0], #8			\n\
	strd	r4, [r0], #8			\n\
	ldrd	r2, [r1], #8			\n\
	mov	ip, r0				\n\
	ldrd	r4, [r1], #8			\n\
	mcr	p15, 0, ip, c7, c6, 1		@ invalidate\n\
	strd	r2, [r0], #8			\n\
	ldrd	r2, [r1], #8			\n\
	subs	lr, lr, #1			\n\
	strd	r4, [r0], #8			\n\
	ldrd	r4, [r1], #8			\n\
	strd	r2, [r0], #8			\n\
	strd	r4, [r0], #8			\n\
	bgt	1b				\n\
	beq	2b				\n\
						\n\
	ldmfd	sp!, {r4, r5, pc}"
	:
	: "r" (kto), "r" (kfrom), "I" (PAGE_SIZE / 64 - 1));
}

void xsc3_mc_copy_user_highpage(struct page *to, struct page *from,
	unsigned long vaddr, struct vm_area_struct *vma)
{
	void *kto, *kfrom;

	kto = kmap_atomic(to);
	kfrom = kmap_atomic(from);
	flush_cache_page(vma, vaddr, page_to_pfn(from));
	xsc3_mc_copy_user_page(kto, kfrom);
	kunmap_atomic(kfrom);
	kunmap_atomic(kto);
}

void xsc3_mc_clear_user_highpage(struct page *page, unsigned long vaddr)
{
	void *ptr, *kaddr = kmap_atomic(page);
	asm volatile ("\
	mov	r1, %2				\n\
	mov	r2, #0				\n\
	mov	r3, #0				\n\
1:	mcr	p15, 0, %0, c7, c6, 1		@ invalidate line\n\
	strd	r2, [%0], #8			\n\
	strd	r2, [%0], #8			\n\
	strd	r2, [%0], #8			\n\
	strd	r2, [%0], #8			\n\
	subs	r1, r1, #1			\n\
	bne	1b"
	: "=r" (ptr)
	: "0" (kaddr), "I" (PAGE_SIZE / 32)
	: "r1", "r2", "r3");
	kunmap_atomic(kaddr);
}

struct cpu_user_fns xsc3_mc_user_fns __initdata = {
	.cpu_clear_user_highpage = xsc3_mc_clear_user_highpage,
	.cpu_copy_user_highpage	= xsc3_mc_copy_user_highpage,
};
