/*
 * Copyright (C) 2007-2008 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2006 Atmark Techno, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/mm.h> 
#include <linux/initrd.h>
#include <linux/pagemap.h>
#include <linux/pfn.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/export.h>

#include <asm/page.h>
#include <asm/mmu_context.h>
#include <asm/pgalloc.h>
#include <asm/sections.h>
#include <asm/tlb.h>
#include <asm/fixmap.h>

int mem_init_done;

#ifndef CONFIG_MMU
unsigned int __page_offset;
EXPORT_SYMBOL(__page_offset);

#else
static int init_bootmem_done;
#endif 

char *klimit = _end;

unsigned long memory_start;
EXPORT_SYMBOL(memory_start);
unsigned long memory_size;
EXPORT_SYMBOL(memory_size);
unsigned long lowmem_size;

#ifdef CONFIG_HIGHMEM
pte_t *kmap_pte;
EXPORT_SYMBOL(kmap_pte);
pgprot_t kmap_prot;
EXPORT_SYMBOL(kmap_prot);

static inline pte_t *virt_to_kpte(unsigned long vaddr)
{
	return pte_offset_kernel(pmd_offset(pgd_offset_k(vaddr),
			vaddr), vaddr);
}

static void __init highmem_init(void)
{
	pr_debug("%x\n", (u32)PKMAP_BASE);
	map_page(PKMAP_BASE, 0, 0);	
	pkmap_page_table = virt_to_kpte(PKMAP_BASE);

	kmap_pte = virt_to_kpte(__fix_to_virt(FIX_KMAP_BEGIN));
	kmap_prot = PAGE_KERNEL;
}

static unsigned long highmem_setup(void)
{
	unsigned long pfn;
	unsigned long reservedpages = 0;

	for (pfn = max_low_pfn; pfn < max_pfn; ++pfn) {
		struct page *page = pfn_to_page(pfn);

		
		if (memblock_is_reserved(pfn << PAGE_SHIFT))
			continue;
		ClearPageReserved(page);
		init_page_count(page);
		__free_page(page);
		totalhigh_pages++;
		reservedpages++;
	}
	totalram_pages += totalhigh_pages;
	printk(KERN_INFO "High memory: %luk\n",
					totalhigh_pages << (PAGE_SHIFT-10));

	return reservedpages;
}
#endif 

static void __init paging_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES];
#ifdef CONFIG_MMU
	int idx;

	
	for (idx = 0; idx < __end_of_fixed_addresses; idx++)
		clear_fixmap(idx);
#endif

	
	memset(zones_size, 0, sizeof(zones_size));

#ifdef CONFIG_HIGHMEM
	highmem_init();

	zones_size[ZONE_DMA] = max_low_pfn;
	zones_size[ZONE_HIGHMEM] = max_pfn;
#else
	zones_size[ZONE_DMA] = max_pfn;
#endif

	
	free_area_init_nodes(zones_size);
}

void __init setup_memory(void)
{
	unsigned long map_size;
	struct memblock_region *reg;

#ifndef CONFIG_MMU
	u32 kernel_align_start, kernel_align_size;

	
	for_each_memblock(memory, reg) {
		memory_start = (u32)reg->base;
		lowmem_size = reg->size;
		if ((memory_start <= (u32)_text) &&
			((u32)_text <= (memory_start + lowmem_size - 1))) {
			memory_size = lowmem_size;
			PAGE_OFFSET = memory_start;
			printk(KERN_INFO "%s: Main mem: 0x%x, "
				"size 0x%08x\n", __func__, (u32) memory_start,
					(u32) memory_size);
			break;
		}
	}

	if (!memory_start || !memory_size) {
		panic("%s: Missing memory setting 0x%08x, size=0x%08x\n",
			__func__, (u32) memory_start, (u32) memory_size);
	}

	
	kernel_align_start = PAGE_DOWN((u32)_text);
	
	kernel_align_size = PAGE_UP((u32)klimit) - kernel_align_start;
	printk(KERN_INFO "%s: kernel addr:0x%08x-0x%08x size=0x%08x\n",
		__func__, kernel_align_start, kernel_align_start
			+ kernel_align_size, kernel_align_size);
	memblock_reserve(kernel_align_start, kernel_align_size);
#endif

	
	min_low_pfn = memory_start >> PAGE_SHIFT; 
	
	num_physpages = max_mapnr = memory_size >> PAGE_SHIFT;
	max_low_pfn = ((u64)memory_start + (u64)lowmem_size) >> PAGE_SHIFT;
	max_pfn = ((u64)memory_start + (u64)memory_size) >> PAGE_SHIFT;

	printk(KERN_INFO "%s: max_mapnr: %#lx\n", __func__, max_mapnr);
	printk(KERN_INFO "%s: min_low_pfn: %#lx\n", __func__, min_low_pfn);
	printk(KERN_INFO "%s: max_low_pfn: %#lx\n", __func__, max_low_pfn);
	printk(KERN_INFO "%s: max_pfn: %#lx\n", __func__, max_pfn);

	map_size = init_bootmem_node(NODE_DATA(0),
		PFN_UP(TOPHYS((u32)klimit)), min_low_pfn, max_low_pfn);
	memblock_reserve(PFN_UP(TOPHYS((u32)klimit)) << PAGE_SHIFT, map_size);

	
	for_each_memblock(memory, reg) {
		unsigned long start_pfn, end_pfn;

		start_pfn = memblock_region_memory_base_pfn(reg);
		end_pfn = memblock_region_memory_end_pfn(reg);
		memblock_set_node(start_pfn << PAGE_SHIFT,
					(end_pfn - start_pfn) << PAGE_SHIFT, 0);
	}

	
	free_bootmem_with_active_regions(0, max_low_pfn);

	
	for_each_memblock(reserved, reg) {
		unsigned long top = reg->base + reg->size - 1;

		pr_debug("reserved - 0x%08x-0x%08x, %lx, %lx\n",
			 (u32) reg->base, (u32) reg->size, top,
						memory_start + lowmem_size - 1);

		if (top <= (memory_start + lowmem_size - 1)) {
			reserve_bootmem(reg->base, reg->size, BOOTMEM_DEFAULT);
		} else if (reg->base < (memory_start + lowmem_size - 1)) {
			unsigned long trunc_size = memory_start + lowmem_size -
								reg->base;
			reserve_bootmem(reg->base, trunc_size, BOOTMEM_DEFAULT);
		}
	}

	
	sparse_memory_present_with_active_regions(0);

#ifdef CONFIG_MMU
	init_bootmem_done = 1;
#endif
	paging_init();
}

void free_init_pages(char *what, unsigned long begin, unsigned long end)
{
	unsigned long addr;

	for (addr = begin; addr < end; addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		init_page_count(virt_to_page(addr));
		free_page(addr);
		totalram_pages++;
	}
	printk(KERN_INFO "Freeing %s: %ldk freed\n", what, (end - begin) >> 10);
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	int pages = 0;
	for (; start < end; start += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(start));
		init_page_count(virt_to_page(start));
		free_page(start);
		totalram_pages++;
		pages++;
	}
	printk(KERN_NOTICE "Freeing initrd memory: %dk freed\n",
					(int)(pages * (PAGE_SIZE / 1024)));
}
#endif

void free_initmem(void)
{
	free_init_pages("unused kernel memory",
			(unsigned long)(&__init_begin),
			(unsigned long)(&__init_end));
}

void __init mem_init(void)
{
	pg_data_t *pgdat;
	unsigned long reservedpages = 0, codesize, initsize, datasize, bsssize;

	high_memory = (void *)__va(memory_start + lowmem_size - 1);

	
	totalram_pages += free_all_bootmem();

	for_each_online_pgdat(pgdat) {
		unsigned long i;
		struct page *page;

		for (i = 0; i < pgdat->node_spanned_pages; i++) {
			if (!pfn_valid(pgdat->node_start_pfn + i))
				continue;
			page = pgdat_page_nr(pgdat, i);
			if (PageReserved(page))
				reservedpages++;
		}
	}

#ifdef CONFIG_HIGHMEM
	reservedpages -= highmem_setup();
#endif

	codesize = (unsigned long)&_sdata - (unsigned long)&_stext;
	datasize = (unsigned long)&_edata - (unsigned long)&_sdata;
	initsize = (unsigned long)&__init_end - (unsigned long)&__init_begin;
	bsssize = (unsigned long)&__bss_stop - (unsigned long)&__bss_start;

	pr_info("Memory: %luk/%luk available (%luk kernel code, "
		"%luk reserved, %luk data, %luk bss, %luk init)\n",
		nr_free_pages() << (PAGE_SHIFT-10),
		num_physpages << (PAGE_SHIFT-10),
		codesize >> 10,
		reservedpages << (PAGE_SHIFT-10),
		datasize >> 10,
		bsssize >> 10,
		initsize >> 10);

#ifdef CONFIG_MMU
	pr_info("Kernel virtual memory layout:\n");
	pr_info("  * 0x%08lx..0x%08lx  : fixmap\n", FIXADDR_START, FIXADDR_TOP);
#ifdef CONFIG_HIGHMEM
	pr_info("  * 0x%08lx..0x%08lx  : highmem PTEs\n",
		PKMAP_BASE, PKMAP_ADDR(LAST_PKMAP));
#endif 
	pr_info("  * 0x%08lx..0x%08lx  : early ioremap\n",
		ioremap_bot, ioremap_base);
	pr_info("  * 0x%08lx..0x%08lx  : vmalloc & ioremap\n",
		(unsigned long)VMALLOC_START, VMALLOC_END);
#endif
	mem_init_done = 1;
}

#ifndef CONFIG_MMU
int page_is_ram(unsigned long pfn)
{
	return __range_ok(pfn, 0);
}
#else
int page_is_ram(unsigned long pfn)
{
	return pfn < max_low_pfn;
}

static void mm_cmdline_setup(void)
{
	unsigned long maxmem = 0;
	char *p = cmd_line;

	
	p = strstr(cmd_line, "mem=");
	if (p) {
		p += 4;
		maxmem = memparse(p, &p);
		if (maxmem && memory_size > maxmem) {
			memory_size = maxmem;
			memblock.memory.regions[0].size = memory_size;
		}
	}
}

static void __init mmu_init_hw(void)
{
	__asm__ __volatile__ ("ori r11, r0, 0x10000000;" \
			"mts rzpr, r11;"
			: : : "r11");
}


asmlinkage void __init mmu_init(void)
{
	unsigned int kstart, ksize;

	if (!memblock.reserved.cnt) {
		printk(KERN_EMERG "Error memory count\n");
		machine_restart(NULL);
	}

	if ((u32) memblock.memory.regions[0].size < 0x400000) {
		printk(KERN_EMERG "Memory must be greater than 4MB\n");
		machine_restart(NULL);
	}

	if ((u32) memblock.memory.regions[0].size < kernel_tlb) {
		printk(KERN_EMERG "Kernel size is greater than memory node\n");
		machine_restart(NULL);
	}

	
	memory_start = (u32) memblock.memory.regions[0].base;
	lowmem_size = memory_size = (u32) memblock.memory.regions[0].size;

	if (lowmem_size > CONFIG_LOWMEM_SIZE) {
		lowmem_size = CONFIG_LOWMEM_SIZE;
#ifndef CONFIG_HIGHMEM
		memory_size = lowmem_size;
#endif
	}

	mm_cmdline_setup(); 

	kstart = __pa(CONFIG_KERNEL_START); 
	
	ksize = PAGE_ALIGN(((u32)_end - (u32)CONFIG_KERNEL_START));
	memblock_reserve(kstart, ksize);

#if defined(CONFIG_BLK_DEV_INITRD)
	
#endif 

	
	mmu_init_hw();

	
	mapin_ram();

	
#ifdef CONFIG_HIGHMEM
	ioremap_base = ioremap_bot = PKMAP_BASE;
#else
	ioremap_base = ioremap_bot = FIXADDR_START;
#endif

	
	mmu_context_init();

	
	memblock_set_current_limit(memory_start + lowmem_size - 1);
}

void __init *early_get_page(void)
{
	void *p;
	if (init_bootmem_done) {
		p = alloc_bootmem_pages(PAGE_SIZE);
	} else {
		p = __va(memblock_alloc_base(PAGE_SIZE, PAGE_SIZE,
					memory_start + kernel_tlb));
	}
	return p;
}

#endif 

void * __init_refok alloc_maybe_bootmem(size_t size, gfp_t mask)
{
	if (mem_init_done)
		return kmalloc(size, mask);
	else
		return alloc_bootmem(size);
}

void * __init_refok zalloc_maybe_bootmem(size_t size, gfp_t mask)
{
	void *p;

	if (mem_init_done)
		p = kzalloc(size, mask);
	else {
		p = alloc_bootmem(size);
		if (p)
			memset(p, 0, size);
	}
	return p;
}
