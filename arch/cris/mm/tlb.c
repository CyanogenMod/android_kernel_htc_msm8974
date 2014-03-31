/*
 *  linux/arch/cris/mm/tlb.c
 *
 *  Copyright (C) 2000, 2001  Axis Communications AB
 *  
 *  Authors:   Bjorn Wesen (bjornw@axis.com)
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/tlb.h>

#define D(x)


struct mm_struct *page_id_map[NUM_PAGEID];
static int map_replace_ptr = 1;  


static inline void
alloc_context(struct mm_struct *mm)
{
	struct mm_struct *old_mm;

	D(printk("tlb: alloc context %d (%p)\n", map_replace_ptr, mm));

	

	old_mm = page_id_map[map_replace_ptr];

	if(old_mm) {
		flush_tlb_mm(old_mm);

		old_mm->context.page_id = NO_CONTEXT;
	}

	

	mm->context.page_id = map_replace_ptr;
	page_id_map[map_replace_ptr] = mm;

	map_replace_ptr++;

	if(map_replace_ptr == INVALID_PAGEID)
		map_replace_ptr = 0;         	
}


void
get_mmu_context(struct mm_struct *mm)
{
	if(mm->context.page_id == NO_CONTEXT)
		alloc_context(mm);
}


void
destroy_context(struct mm_struct *mm)
{
	if(mm->context.page_id != NO_CONTEXT) {
		D(printk("destroy_context %d (%p)\n", mm->context.page_id, mm));
		flush_tlb_mm(mm);  
		page_id_map[mm->context.page_id] = NULL;
	}
}


void __init
tlb_init(void)
{
	int i;

	

	for (i = 1; i < ARRAY_SIZE(page_id_map); i++)
		page_id_map[i] = NULL;
	
	

	flush_tlb_all();

	

	page_id_map[0] = &init_mm;
}
