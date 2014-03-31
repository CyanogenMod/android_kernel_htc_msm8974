/*
 *  linux/arch/m68k/mm/memory.c
 *
 *  Copyright (C) 1995  Hamish Macdonald
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/gfp.h>

#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/traps.h>
#include <asm/machdep.h>


/* ++andreas: {get,free}_pointer_table rewritten to use unused fields from
   struct page instead of separately kmalloced struct.  Stolen from
   arch/sparc/mm/srmmu.c ... */

typedef struct list_head ptable_desc;
static LIST_HEAD(ptable_list);

#define PD_PTABLE(page) ((ptable_desc *)&(virt_to_page(page)->lru))
#define PD_PAGE(ptable) (list_entry(ptable, struct page, lru))
#define PD_MARKBITS(dp) (*(unsigned char *)&PD_PAGE(dp)->index)

#define PTABLE_SIZE (PTRS_PER_PMD * sizeof(pmd_t))

void __init init_pointer_table(unsigned long ptable)
{
	ptable_desc *dp;
	unsigned long page = ptable & PAGE_MASK;
	unsigned char mask = 1 << ((ptable - page)/PTABLE_SIZE);

	dp = PD_PTABLE(page);
	if (!(PD_MARKBITS(dp) & mask)) {
		PD_MARKBITS(dp) = 0xff;
		list_add(dp, &ptable_list);
	}

	PD_MARKBITS(dp) &= ~mask;
#ifdef DEBUG
	printk("init_pointer_table: %lx, %x\n", ptable, PD_MARKBITS(dp));
#endif

	
	PD_PAGE(dp)->flags &= ~(1 << PG_reserved);
	init_page_count(PD_PAGE(dp));

	return;
}

pmd_t *get_pointer_table (void)
{
	ptable_desc *dp = ptable_list.next;
	unsigned char mask = PD_MARKBITS (dp);
	unsigned char tmp;
	unsigned int off;

	if (mask == 0) {
		void *page;
		ptable_desc *new;

		if (!(page = (void *)get_zeroed_page(GFP_KERNEL)))
			return NULL;

		flush_tlb_kernel_page(page);
		nocache_page(page);

		new = PD_PTABLE(page);
		PD_MARKBITS(new) = 0xfe;
		list_add_tail(new, dp);

		return (pmd_t *)page;
	}

	for (tmp = 1, off = 0; (mask & tmp) == 0; tmp <<= 1, off += PTABLE_SIZE)
		;
	PD_MARKBITS(dp) = mask & ~tmp;
	if (!PD_MARKBITS(dp)) {
		
		list_move_tail(dp, &ptable_list);
	}
	return (pmd_t *) (page_address(PD_PAGE(dp)) + off);
}

int free_pointer_table (pmd_t *ptable)
{
	ptable_desc *dp;
	unsigned long page = (unsigned long)ptable & PAGE_MASK;
	unsigned char mask = 1 << (((unsigned long)ptable - page)/PTABLE_SIZE);

	dp = PD_PTABLE(page);
	if (PD_MARKBITS (dp) & mask)
		panic ("table already free!");

	PD_MARKBITS (dp) |= mask;

	if (PD_MARKBITS(dp) == 0xff) {
		
		list_del(dp);
		cache_page((void *)page);
		free_page (page);
		return 1;
	} else if (ptable_list.next != dp) {
		list_move(dp, &ptable_list);
	}
	return 0;
}

static inline void clear040(unsigned long paddr)
{
	asm volatile (
		"nop\n\t"
		".chip 68040\n\t"
		"cinvp %%bc,(%0)\n\t"
		".chip 68k"
		: : "a" (paddr));
}

static inline void cleari040(unsigned long paddr)
{
	asm volatile (
		"nop\n\t"
		".chip 68040\n\t"
		"cinvp %%ic,(%0)\n\t"
		".chip 68k"
		: : "a" (paddr));
}

static inline void push040(unsigned long paddr)
{
	asm volatile (
		"nop\n\t"
		".chip 68040\n\t"
		"cpushp %%bc,(%0)\n\t"
		".chip 68k"
		: : "a" (paddr));
}

static inline void pushcl040(unsigned long paddr)
{
	unsigned long flags;

	local_irq_save(flags);
	push040(paddr);
	if (CPU_IS_060)
		clear040(paddr);
	local_irq_restore(flags);
}



/*
 * cache_clear() semantics: Clear any cache entries for the area in question,
 * without writing back dirty entries first. This is useful if the data will
 * be overwritten anyway, e.g. by DMA to memory. The range is defined by a
 * _physical_ address.
 */

void cache_clear (unsigned long paddr, int len)
{
    if (CPU_IS_COLDFIRE) {
	flush_cf_bcache(0, DCACHE_MAX_ADDR);
    } else if (CPU_IS_040_OR_060) {
	int tmp;

	if ((tmp = -paddr & (PAGE_SIZE - 1))) {
	    pushcl040(paddr & PAGE_MASK);
	    if ((len -= tmp) <= 0)
		return;
	    paddr += tmp;
	}
	tmp = PAGE_SIZE;
	paddr &= PAGE_MASK;
	while ((len -= tmp) >= 0) {
	    clear040(paddr);
	    paddr += tmp;
	}
	if ((len += tmp))
	    
	    pushcl040(paddr);
    }
    else 
	asm volatile ("movec %/cacr,%/d0\n\t"
		      "oriw %0,%/d0\n\t"
		      "movec %/d0,%/cacr"
		      : : "i" (FLUSH_I_AND_D)
		      : "d0");
#ifdef CONFIG_M68K_L2_CACHE
    if(mach_l2_flush)
	mach_l2_flush(0);
#endif
}
EXPORT_SYMBOL(cache_clear);



void cache_push (unsigned long paddr, int len)
{
    if (CPU_IS_COLDFIRE) {
	flush_cf_bcache(0, DCACHE_MAX_ADDR);
    } else if (CPU_IS_040_OR_060) {
	int tmp = PAGE_SIZE;

	len += paddr & (PAGE_SIZE - 1);

	paddr &= PAGE_MASK;

	do {
	    push040(paddr);
	    paddr += tmp;
	} while ((len -= tmp) > 0);
    }
    /*
     * 68030/68020 have no writeback cache. On the other hand,
     * cache_push is actually a superset of cache_clear (the lines
     * get written back and invalidated), so we should make sure
     * to perform the corresponding actions. After all, this is getting
     * called in places where we've just loaded code, or whatever, so
     * flushing the icache is appropriate; flushing the dcache shouldn't
     * be required.
     */
    else 
	asm volatile ("movec %/cacr,%/d0\n\t"
		      "oriw %0,%/d0\n\t"
		      "movec %/d0,%/cacr"
		      : : "i" (FLUSH_I)
		      : "d0");
#ifdef CONFIG_M68K_L2_CACHE
    if(mach_l2_flush)
	mach_l2_flush(1);
#endif
}
EXPORT_SYMBOL(cache_push);

