/* MN10300 Memory management initialisation
 *
 * Copyright (C) 2007 Matsushita Electric Industrial Co., Ltd.
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Modified by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/bootmem.h>
#include <linux/gfp.h>

#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/dma.h>
#include <asm/tlb.h>
#include <asm/sections.h>

unsigned long highstart_pfn, highend_pfn;

#ifdef CONFIG_MN10300_HAS_ATOMIC_OPS_UNIT
static struct vm_struct user_iomap_vm;
#endif

void __init paging_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES] = {0,};
	pte_t *ppte;
	int loop;

	memset(swapper_pg_dir, 0, sizeof(swapper_pg_dir));
	memset(kernel_vmalloc_ptes, 0, sizeof(kernel_vmalloc_ptes));

	
	ppte = kernel_vmalloc_ptes;
	for (loop = VMALLOC_START / (PAGE_SIZE * PTRS_PER_PTE);
	     loop < VMALLOC_END / (PAGE_SIZE * PTRS_PER_PTE);
	     loop++
	     ) {
		set_pgd(swapper_pg_dir + loop, __pgd(__pa(ppte) | _PAGE_TABLE));
		ppte += PAGE_SIZE / sizeof(pte_t);
	}

	
	zones_size[ZONE_NORMAL] =
		contig_page_data.bdata->node_low_pfn -
		contig_page_data.bdata->node_min_pfn;

	
	free_area_init(zones_size);

#ifdef CONFIG_MN10300_HAS_ATOMIC_OPS_UNIT
	user_iomap_vm.flags = VM_USERMAP;
	user_iomap_vm.size = 1 << PAGE_SHIFT;
	vm_area_register_early(&user_iomap_vm, PAGE_SIZE);
	ppte = kernel_vmalloc_ptes;
	set_pte(ppte, pfn_pte(USER_ATOMIC_OPS_PAGE_ADDR >> PAGE_SHIFT,
			      PAGE_USERIO));
#endif

	local_flush_tlb_all();
}

void __init mem_init(void)
{
	int codesize, reservedpages, datasize, initsize;
	int tmp;

	BUG_ON(!mem_map);

#define START_PFN	(contig_page_data.bdata->node_min_pfn)
#define MAX_LOW_PFN	(contig_page_data.bdata->node_low_pfn)

	max_mapnr = num_physpages = MAX_LOW_PFN - START_PFN;
	high_memory = (void *) __va(MAX_LOW_PFN * PAGE_SIZE);

	
	memset(empty_zero_page, 0, PAGE_SIZE);

	
	totalram_pages += free_all_bootmem();

	reservedpages = 0;
	for (tmp = 0; tmp < num_physpages; tmp++)
		if (PageReserved(&mem_map[tmp]))
			reservedpages++;

	codesize =  (unsigned long) &_etext - (unsigned long) &_stext;
	datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;

	printk(KERN_INFO
	       "Memory: %luk/%luk available"
	       " (%dk kernel code, %dk reserved, %dk data, %dk init,"
	       " %ldk highmem)\n",
	       nr_free_pages() << (PAGE_SHIFT - 10),
	       max_mapnr << (PAGE_SHIFT - 10),
	       codesize >> 10,
	       reservedpages << (PAGE_SHIFT - 10),
	       datasize >> 10,
	       initsize >> 10,
	       totalhigh_pages << (PAGE_SHIFT - 10));
}

void free_init_pages(char *what, unsigned long begin, unsigned long end)
{
	unsigned long addr;

	for (addr = begin; addr < end; addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		init_page_count(virt_to_page(addr));
		memset((void *) addr, 0xcc, PAGE_SIZE);
		free_page(addr);
		totalram_pages++;
	}
	printk(KERN_INFO "Freeing %s: %ldk freed\n", what, (end - begin) >> 10);
}

void free_initmem(void)
{
	free_init_pages("unused kernel memory",
			(unsigned long) &__init_begin,
			(unsigned long) &__init_end);
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	free_init_pages("initrd memory", start, end);
}
#endif
