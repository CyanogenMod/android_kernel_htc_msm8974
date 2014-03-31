/*
 * Memory subsystem initialization for Hexagon
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <asm/atomic.h>
#include <linux/highmem.h>
#include <asm/tlb.h>
#include <asm/sections.h>
#include <asm/vm_mmu.h>

#define bootmem_startpg (PFN_UP(((unsigned long) _end) - PAGE_OFFSET))

unsigned long bootmem_lastpg;  

int max_kernel_seg = 0x303;

unsigned long zero_page_mask;

unsigned long highstart_pfn, highend_pfn;

DEFINE_PER_CPU(struct mmu_gather, mmu_gathers);

unsigned long _dflt_cache_att = CACHEDEF;

DEFINE_SPINLOCK(kmap_gen_lock);

unsigned long long kmap_generation;

void __init mem_init(void)
{
	
	totalram_pages += free_all_bootmem();
	num_physpages = bootmem_lastpg;	

	printk(KERN_INFO "totalram_pages = %ld\n", totalram_pages);


	init_mm.context.ptbase = __pa(init_mm.pgd);
}

void __init_refok free_initmem(void)
{
}

void free_initrd_mem(unsigned long start, unsigned long end)
{
}

void sync_icache_dcache(pte_t pte)
{
	unsigned long addr;
	struct page *page;

	page = pte_page(pte);
	addr = (unsigned long) page_address(page);

	__vmcache_idsync(addr, PAGE_SIZE);
}

void __init paging_init(void)
{
	unsigned long zones_sizes[MAX_NR_ZONES] = {0, };


	zones_sizes[ZONE_NORMAL] = max_low_pfn;

	free_area_init(zones_sizes);  

	high_memory = (void *)((bootmem_lastpg + 1) << PAGE_SHIFT);
}

#ifndef DMA_RESERVE
#define DMA_RESERVE		(4)
#endif

#define DMA_CHUNKSIZE		(1<<22)
#define DMA_RESERVED_BYTES	(DMA_RESERVE * DMA_CHUNKSIZE)

static int __init early_mem(char *p)
{
	unsigned long size;
	char *endp;

	size = memparse(p, &endp);

	bootmem_lastpg = PFN_DOWN(size);

	return 0;
}
early_param("mem", early_mem);

size_t hexagon_coherent_pool_size = (size_t) (DMA_RESERVE << 22);

void __init setup_arch_memory(void)
{
	int bootmap_size;
	
	u32 *segtable = (u32 *) &swapper_pg_dir[0];
	u32 *segtable_end;


	
	bootmem_lastpg = PFN_DOWN((bootmem_lastpg << PAGE_SHIFT) &
		~((BIG_KERNEL_PAGE_SIZE) - 1));

	bootmap_size = init_bootmem(bootmem_startpg, bootmem_lastpg -
				    PFN_DOWN(DMA_RESERVED_BYTES));

	printk(KERN_INFO "bootmem_startpg:  0x%08lx\n", bootmem_startpg);
	printk(KERN_INFO "bootmem_lastpg:  0x%08lx\n", bootmem_lastpg);
	printk(KERN_INFO "bootmap_size:  %d\n", bootmap_size);
	printk(KERN_INFO "max_low_pfn:  0x%08lx\n", max_low_pfn);


	
	segtable = segtable + (PAGE_OFFSET >> 22);

	
	segtable_end = segtable + (1<<(30-22));

	
	segtable += bootmem_lastpg >> (22-PAGE_SHIFT);

	{
	    int i;

	    for (i = 1 ; i <= DMA_RESERVE ; i++)
		segtable[-i] = ((segtable[-i] & __HVM_PTE_PGMASK_4MB)
				| __HVM_PTE_R | __HVM_PTE_W | __HVM_PTE_X
				| __HEXAGON_C_UNC << 6
				| __HVM_PDE_S_4MB);
	}

	printk(KERN_INFO "clearing segtable from %p to %p\n", segtable,
		segtable_end);
	while (segtable < (segtable_end-8))
		*(segtable++) = __HVM_PDE_S_INVALID;
	

	printk(KERN_INFO "segtable = %p (should be equal to _K_io_map)\n",
		segtable);

#if 0
	
	printk(KERN_INFO "&_K_init_devicetable = 0x%08x\n",
		(unsigned long) _K_init_devicetable-PAGE_OFFSET);
	*segtable = ((u32) (unsigned long) _K_init_devicetable-PAGE_OFFSET) |
		__HVM_PDE_S_4KB;
	printk(KERN_INFO "*segtable = 0x%08x\n", *segtable);
#endif

	free_bootmem(PFN_PHYS(bootmem_startpg)+bootmap_size,
		     PFN_PHYS(bootmem_lastpg - bootmem_startpg) - bootmap_size -
		     DMA_RESERVED_BYTES);

	printk(KERN_INFO "PAGE_SIZE=%lu\n", PAGE_SIZE);
	paging_init();  

}
