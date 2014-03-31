/*
 *  linux/arch/parisc/mm/init.c
 *
 *  Copyright (C) 1995	Linus Torvalds
 *  Copyright 1999 SuSE GmbH
 *    changed by Philipp Rumpf
 *  Copyright 1999 Philipp Rumpf (prumpf@tux.org)
 *  Copyright 2004 Randolph Chung (tausq@debian.org)
 *  Copyright 2006-2007 Helge Deller (deller@gmx.de)
 *
 */


#include <linux/module.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/gfp.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pci.h>		
#include <linux/initrd.h>
#include <linux/swap.h>
#include <linux/unistd.h>
#include <linux/nodemask.h>	
#include <linux/pagemap.h>	

#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/tlb.h>
#include <asm/pdc_chassis.h>
#include <asm/mmzone.h>
#include <asm/sections.h>

extern int  data_start;

#ifdef CONFIG_DISCONTIGMEM
struct node_map_data node_data[MAX_NUMNODES] __read_mostly;
unsigned char pfnnid_map[PFNNID_MAP_MAX] __read_mostly;
#endif

static struct resource data_resource = {
	.name	= "Kernel data",
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM,
};

static struct resource code_resource = {
	.name	= "Kernel code",
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM,
};

static struct resource pdcdata_resource = {
	.name	= "PDC data (Page Zero)",
	.start	= 0,
	.end	= 0x9ff,
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM,
};

static struct resource sysram_resources[MAX_PHYSMEM_RANGES] __read_mostly;


physmem_range_t pmem_ranges[MAX_PHYSMEM_RANGES] __read_mostly;
int npmem_ranges __read_mostly;

#ifdef CONFIG_64BIT
#define MAX_MEM         (~0UL)
#else 
#define MAX_MEM         (3584U*1024U*1024U)
#endif 

static unsigned long mem_limit __read_mostly = MAX_MEM;

static void __init mem_limit_func(void)
{
	char *cp, *end;
	unsigned long limit;

	

	limit = MAX_MEM;
	for (cp = boot_command_line; *cp; ) {
		if (memcmp(cp, "mem=", 4) == 0) {
			cp += 4;
			limit = memparse(cp, &end);
			if (end != cp)
				break;
			cp = end;
		} else {
			while (*cp != ' ' && *cp)
				++cp;
			while (*cp == ' ')
				++cp;
		}
	}

	if (limit < mem_limit)
		mem_limit = limit;
}

#define MAX_GAP (0x40000000UL >> PAGE_SHIFT)

static void __init setup_bootmem(void)
{
	unsigned long bootmap_size;
	unsigned long mem_max;
	unsigned long bootmap_pages;
	unsigned long bootmap_start_pfn;
	unsigned long bootmap_pfn;
#ifndef CONFIG_DISCONTIGMEM
	physmem_range_t pmem_holes[MAX_PHYSMEM_RANGES - 1];
	int npmem_holes;
#endif
	int i, sysram_resource_count;

	disable_sr_hashing(); 


	for (i = 1; i < npmem_ranges; i++) {
		int j;

		for (j = i; j > 0; j--) {
			unsigned long tmp;

			if (pmem_ranges[j-1].start_pfn <
			    pmem_ranges[j].start_pfn) {

				break;
			}
			tmp = pmem_ranges[j-1].start_pfn;
			pmem_ranges[j-1].start_pfn = pmem_ranges[j].start_pfn;
			pmem_ranges[j].start_pfn = tmp;
			tmp = pmem_ranges[j-1].pages;
			pmem_ranges[j-1].pages = pmem_ranges[j].pages;
			pmem_ranges[j].pages = tmp;
		}
	}

#ifndef CONFIG_DISCONTIGMEM

	for (i = 1; i < npmem_ranges; i++) {
		if (pmem_ranges[i].start_pfn -
			(pmem_ranges[i-1].start_pfn +
			 pmem_ranges[i-1].pages) > MAX_GAP) {
			npmem_ranges = i;
			printk("Large gap in memory detected (%ld pages). "
			       "Consider turning on CONFIG_DISCONTIGMEM\n",
			       pmem_ranges[i].start_pfn -
			       (pmem_ranges[i-1].start_pfn +
			        pmem_ranges[i-1].pages));
			break;
		}
	}
#endif

	if (npmem_ranges > 1) {

		

		printk(KERN_INFO "Memory Ranges:\n");

		for (i = 0; i < npmem_ranges; i++) {
			unsigned long start;
			unsigned long size;

			size = (pmem_ranges[i].pages << PAGE_SHIFT);
			start = (pmem_ranges[i].start_pfn << PAGE_SHIFT);
			printk(KERN_INFO "%2d) Start 0x%016lx End 0x%016lx Size %6ld MB\n",
				i,start, start + (size - 1), size >> 20);
		}
	}

	sysram_resource_count = npmem_ranges;
	for (i = 0; i < sysram_resource_count; i++) {
		struct resource *res = &sysram_resources[i];
		res->name = "System RAM";
		res->start = pmem_ranges[i].start_pfn << PAGE_SHIFT;
		res->end = res->start + (pmem_ranges[i].pages << PAGE_SHIFT)-1;
		res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;
		request_resource(&iomem_resource, res);
	}

	/*
	 * For 32 bit kernels we limit the amount of memory we can
	 * support, in order to preserve enough kernel address space
	 * for other purposes. For 64 bit kernels we don't normally
	 * limit the memory, but this mechanism can be used to
	 * artificially limit the amount of memory (and it is written
	 * to work with multiple memory ranges).
	 */

	mem_limit_func();       

	mem_max = 0;
	num_physpages = 0;
	for (i = 0; i < npmem_ranges; i++) {
		unsigned long rsize;

		rsize = pmem_ranges[i].pages << PAGE_SHIFT;
		if ((mem_max + rsize) > mem_limit) {
			printk(KERN_WARNING "Memory truncated to %ld MB\n", mem_limit >> 20);
			if (mem_max == mem_limit)
				npmem_ranges = i;
			else {
				pmem_ranges[i].pages =   (mem_limit >> PAGE_SHIFT)
						       - (mem_max >> PAGE_SHIFT);
				npmem_ranges = i + 1;
				mem_max = mem_limit;
			}
	        num_physpages += pmem_ranges[i].pages;
			break;
		}
	    num_physpages += pmem_ranges[i].pages;
		mem_max += rsize;
	}

	printk(KERN_INFO "Total Memory: %ld MB\n",mem_max >> 20);

#ifndef CONFIG_DISCONTIGMEM
	

	{
		unsigned long end_pfn;
		unsigned long hole_pages;

		npmem_holes = 0;
		end_pfn = pmem_ranges[0].start_pfn + pmem_ranges[0].pages;
		for (i = 1; i < npmem_ranges; i++) {

			hole_pages = pmem_ranges[i].start_pfn - end_pfn;
			if (hole_pages) {
				pmem_holes[npmem_holes].start_pfn = end_pfn;
				pmem_holes[npmem_holes++].pages = hole_pages;
				end_pfn += hole_pages;
			}
			end_pfn += pmem_ranges[i].pages;
		}

		pmem_ranges[0].pages = end_pfn - pmem_ranges[0].start_pfn;
		npmem_ranges = 1;
	}
#endif

	bootmap_pages = 0;
	for (i = 0; i < npmem_ranges; i++)
		bootmap_pages += bootmem_bootmap_pages(pmem_ranges[i].pages);

	bootmap_start_pfn = PAGE_ALIGN(__pa((unsigned long) &_end)) >> PAGE_SHIFT;

#ifdef CONFIG_DISCONTIGMEM
	for (i = 0; i < MAX_PHYSMEM_RANGES; i++) {
		memset(NODE_DATA(i), 0, sizeof(pg_data_t));
		NODE_DATA(i)->bdata = &bootmem_node_data[i];
	}
	memset(pfnnid_map, 0xff, sizeof(pfnnid_map));

	for (i = 0; i < npmem_ranges; i++) {
		node_set_state(i, N_NORMAL_MEMORY);
		node_set_online(i);
	}
#endif


	bootmap_pfn = bootmap_start_pfn;
	max_pfn = 0;
	for (i = 0; i < npmem_ranges; i++) {
		unsigned long start_pfn;
		unsigned long npages;

		start_pfn = pmem_ranges[i].start_pfn;
		npages = pmem_ranges[i].pages;

		bootmap_size = init_bootmem_node(NODE_DATA(i),
						bootmap_pfn,
						start_pfn,
						(start_pfn + npages) );
		free_bootmem_node(NODE_DATA(i),
				  (start_pfn << PAGE_SHIFT),
				  (npages << PAGE_SHIFT) );
		bootmap_pfn += (bootmap_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
		if ((start_pfn + npages) > max_pfn)
			max_pfn = start_pfn + npages;
	}

	max_low_pfn = max_pfn;

	
	BUG_ON((bootmap_pfn - bootmap_start_pfn) != bootmap_pages);

	

#define PDC_CONSOLE_IO_IODC_SIZE 32768

	reserve_bootmem_node(NODE_DATA(0), 0UL,
			(unsigned long)(PAGE0->mem_free +
				PDC_CONSOLE_IO_IODC_SIZE), BOOTMEM_DEFAULT);
	reserve_bootmem_node(NODE_DATA(0), __pa((unsigned long)_text),
			(unsigned long)(_end - _text), BOOTMEM_DEFAULT);
	reserve_bootmem_node(NODE_DATA(0), (bootmap_start_pfn << PAGE_SHIFT),
			((bootmap_pfn - bootmap_start_pfn) << PAGE_SHIFT),
			BOOTMEM_DEFAULT);

#ifndef CONFIG_DISCONTIGMEM

	

	for (i = 0; i < npmem_holes; i++) {
		reserve_bootmem_node(NODE_DATA(0),
				(pmem_holes[i].start_pfn << PAGE_SHIFT),
				(pmem_holes[i].pages << PAGE_SHIFT),
				BOOTMEM_DEFAULT);
	}
#endif

#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start) {
		printk(KERN_INFO "initrd: %08lx-%08lx\n", initrd_start, initrd_end);
		if (__pa(initrd_start) < mem_max) {
			unsigned long initrd_reserve;

			if (__pa(initrd_end) > mem_max) {
				initrd_reserve = mem_max - __pa(initrd_start);
			} else {
				initrd_reserve = initrd_end - initrd_start;
			}
			initrd_below_start_ok = 1;
			printk(KERN_INFO "initrd: reserving %08lx-%08lx (mem_max %08lx)\n", __pa(initrd_start), __pa(initrd_start) + initrd_reserve, mem_max);

			reserve_bootmem_node(NODE_DATA(0), __pa(initrd_start),
					initrd_reserve, BOOTMEM_DEFAULT);
		}
	}
#endif

	data_resource.start =  virt_to_phys(&data_start);
	data_resource.end = virt_to_phys(_end) - 1;
	code_resource.start = virt_to_phys(_text);
	code_resource.end = virt_to_phys(&data_start)-1;

	for (i = 0; i < sysram_resource_count; i++) {
		struct resource *res = &sysram_resources[i];
		request_resource(res, &code_resource);
		request_resource(res, &data_resource);
	}
	request_resource(&sysram_resources[0], &pdcdata_resource);
}

static void __init map_pages(unsigned long start_vaddr,
			     unsigned long start_paddr, unsigned long size,
			     pgprot_t pgprot, int force)
{
	pgd_t *pg_dir;
	pmd_t *pmd;
	pte_t *pg_table;
	unsigned long end_paddr;
	unsigned long start_pmd;
	unsigned long start_pte;
	unsigned long tmp1;
	unsigned long tmp2;
	unsigned long address;
	unsigned long vaddr;
	unsigned long ro_start;
	unsigned long ro_end;
	unsigned long fv_addr;
	unsigned long gw_addr;
	extern const unsigned long fault_vector_20;
	extern void * const linux_gateway_page;

	ro_start = __pa((unsigned long)_text);
	ro_end   = __pa((unsigned long)&data_start);
	fv_addr  = __pa((unsigned long)&fault_vector_20) & PAGE_MASK;
	gw_addr  = __pa((unsigned long)&linux_gateway_page) & PAGE_MASK;

	end_paddr = start_paddr + size;

	pg_dir = pgd_offset_k(start_vaddr);

#if PTRS_PER_PMD == 1
	start_pmd = 0;
#else
	start_pmd = ((start_vaddr >> PMD_SHIFT) & (PTRS_PER_PMD - 1));
#endif
	start_pte = ((start_vaddr >> PAGE_SHIFT) & (PTRS_PER_PTE - 1));

	address = start_paddr;
	vaddr = start_vaddr;
	while (address < end_paddr) {
#if PTRS_PER_PMD == 1
		pmd = (pmd_t *)__pa(pg_dir);
#else
		pmd = (pmd_t *)pgd_address(*pg_dir);


		if (!pmd) {
			pmd = (pmd_t *) alloc_bootmem_low_pages_node(NODE_DATA(0), PAGE_SIZE << PMD_ORDER);
			pmd = (pmd_t *) __pa(pmd);
		}

		pgd_populate(NULL, pg_dir, __va(pmd));
#endif
		pg_dir++;

		

		pmd = (pmd_t *)__va(pmd) + start_pmd;
		for (tmp1 = start_pmd; tmp1 < PTRS_PER_PMD; tmp1++, pmd++) {


			pg_table = (pte_t *)pmd_address(*pmd);
			if (!pg_table) {
				pg_table = (pte_t *)
					alloc_bootmem_low_pages_node(NODE_DATA(0), PAGE_SIZE);
				pg_table = (pte_t *) __pa(pg_table);
			}

			pmd_populate_kernel(NULL, pmd, __va(pg_table));

			

			pg_table = (pte_t *) __va(pg_table) + start_pte;
			for (tmp2 = start_pte; tmp2 < PTRS_PER_PTE; tmp2++, pg_table++) {
				pte_t pte;

				if (force)
					pte =  __mk_pte(address, pgprot);
				else if (core_kernel_text(vaddr) &&
					 address != fv_addr)
					pte = __mk_pte(address, PAGE_KERNEL_EXEC);
				else
#if defined(CONFIG_PARISC_PAGE_SIZE_4KB)
				if (address >= ro_start && address < ro_end
							&& address != fv_addr
							&& address != gw_addr)
					pte = __mk_pte(address, PAGE_KERNEL_RO);
				else
#endif
					pte = __mk_pte(address, pgprot);

				if (address >= end_paddr) {
					if (force)
						break;
					else
						pte_val(pte) = 0;
				}

				set_pte(pg_table, pte);

				address += PAGE_SIZE;
				vaddr += PAGE_SIZE;
			}
			start_pte = 0;

			if (address >= end_paddr)
			    break;
		}
		start_pmd = 0;
	}
}

void free_initmem(void)
{
	unsigned long addr;
	unsigned long init_begin = (unsigned long)__init_begin;
	unsigned long init_end = (unsigned long)__init_end;

	map_pages(init_begin, __pa(init_begin), init_end - init_begin,
		  PAGE_KERNEL_RWX, 1);
	map_pages(init_begin, __pa(init_begin), init_end - init_begin,
		  PAGE_KERNEL, 1);

	
	__flush_tlb_range(0, init_begin, init_end);
	memset((void *)init_begin, 0x00, init_end - init_begin);
	flush_icache_range(init_begin, init_end);
	
	for (addr = init_begin; addr < init_end; addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		init_page_count(virt_to_page(addr));
		free_page(addr);
		num_physpages++;
		totalram_pages++;
	}

	
	pdc_chassis_send_status(PDC_CHASSIS_DIRECT_BCOMPLETE);
	
	printk(KERN_INFO "Freeing unused kernel memory: %luk freed\n",
		(init_end - init_begin) >> 10);
}


#ifdef CONFIG_DEBUG_RODATA
void mark_rodata_ro(void)
{
	printk (KERN_INFO "Write protecting the kernel read-only data: %luk\n",
		(unsigned long)(__end_rodata - __start_rodata) >> 10);
}
#endif



 
#if KERNEL_MAP_START < GATEWAY_PAGE_SIZE
#error KERNEL_MAP_START is in gateway reserved region
#endif
#define MAP_START (KERNEL_MAP_START)

#define VM_MAP_OFFSET  (32*1024)
#define SET_MAP_OFFSET(x) ((void *)(((unsigned long)(x) + VM_MAP_OFFSET) \
				     & ~(VM_MAP_OFFSET-1)))

void *parisc_vmalloc_start __read_mostly;
EXPORT_SYMBOL(parisc_vmalloc_start);

#ifdef CONFIG_PA11
unsigned long pcxl_dma_start __read_mostly;
#endif

void __init mem_init(void)
{
	int codesize, reservedpages, datasize, initsize;

	
	BUILD_BUG_ON(PTE_ENTRY_SIZE != sizeof(pte_t));
	BUILD_BUG_ON(PMD_ENTRY_SIZE != sizeof(pmd_t));
	BUILD_BUG_ON(PGD_ENTRY_SIZE != sizeof(pgd_t));
	BUILD_BUG_ON(PAGE_SHIFT + BITS_PER_PTE + BITS_PER_PMD + BITS_PER_PGD
			> BITS_PER_LONG);

	high_memory = __va((max_pfn << PAGE_SHIFT));

#ifndef CONFIG_DISCONTIGMEM
	max_mapnr = page_to_pfn(virt_to_page(high_memory - 1)) + 1;
	totalram_pages += free_all_bootmem();
#else
	{
		int i;

		for (i = 0; i < npmem_ranges; i++)
			totalram_pages += free_all_bootmem_node(NODE_DATA(i));
	}
#endif

	codesize = (unsigned long)_etext - (unsigned long)_text;
	datasize = (unsigned long)_edata - (unsigned long)_etext;
	initsize = (unsigned long)__init_end - (unsigned long)__init_begin;

	reservedpages = 0;
{
	unsigned long pfn;
#ifdef CONFIG_DISCONTIGMEM
	int i;

	for (i = 0; i < npmem_ranges; i++) {
		for (pfn = node_start_pfn(i); pfn < node_end_pfn(i); pfn++) {
			if (PageReserved(pfn_to_page(pfn)))
				reservedpages++;
		}
	}
#else 
	for (pfn = 0; pfn < max_pfn; pfn++) {
		if (PageReserved(pfn_to_page(pfn)))
			reservedpages++;
	}
#endif
}

#ifdef CONFIG_PA11
	if (hppa_dma_ops == &pcxl_dma_ops) {
		pcxl_dma_start = (unsigned long)SET_MAP_OFFSET(MAP_START);
		parisc_vmalloc_start = SET_MAP_OFFSET(pcxl_dma_start
						+ PCXL_DMA_MAP_SIZE);
	} else {
		pcxl_dma_start = 0;
		parisc_vmalloc_start = SET_MAP_OFFSET(MAP_START);
	}
#else
	parisc_vmalloc_start = SET_MAP_OFFSET(MAP_START);
#endif

	printk(KERN_INFO "Memory: %luk/%luk available (%dk kernel code, %dk reserved, %dk data, %dk init)\n",
		nr_free_pages() << (PAGE_SHIFT-10),
		num_physpages << (PAGE_SHIFT-10),
		codesize >> 10,
		reservedpages << (PAGE_SHIFT-10),
		datasize >> 10,
		initsize >> 10
	);

#ifdef CONFIG_DEBUG_KERNEL 
	printk("virtual kernel memory layout:\n"
	       "    vmalloc : 0x%p - 0x%p   (%4ld MB)\n"
	       "    memory  : 0x%p - 0x%p   (%4ld MB)\n"
	       "      .init : 0x%p - 0x%p   (%4ld kB)\n"
	       "      .data : 0x%p - 0x%p   (%4ld kB)\n"
	       "      .text : 0x%p - 0x%p   (%4ld kB)\n",

	       (void*)VMALLOC_START, (void*)VMALLOC_END,
	       (VMALLOC_END - VMALLOC_START) >> 20,

	       __va(0), high_memory,
	       ((unsigned long)high_memory - (unsigned long)__va(0)) >> 20,

	       __init_begin, __init_end,
	       ((unsigned long)__init_end - (unsigned long)__init_begin) >> 10,

	       _etext, _edata,
	       ((unsigned long)_edata - (unsigned long)_etext) >> 10,

	       _text, _etext,
	       ((unsigned long)_etext - (unsigned long)_text) >> 10);
#endif
}

unsigned long *empty_zero_page __read_mostly;
EXPORT_SYMBOL(empty_zero_page);

void show_mem(unsigned int filter)
{
	int i,free = 0,total = 0,reserved = 0;
	int shared = 0, cached = 0;

	printk(KERN_INFO "Mem-info:\n");
	show_free_areas(filter);
#ifndef CONFIG_DISCONTIGMEM
	i = max_mapnr;
	while (i-- > 0) {
		total++;
		if (PageReserved(mem_map+i))
			reserved++;
		else if (PageSwapCache(mem_map+i))
			cached++;
		else if (!page_count(&mem_map[i]))
			free++;
		else
			shared += page_count(&mem_map[i]) - 1;
	}
#else
	for (i = 0; i < npmem_ranges; i++) {
		int j;

		for (j = node_start_pfn(i); j < node_end_pfn(i); j++) {
			struct page *p;
			unsigned long flags;

			pgdat_resize_lock(NODE_DATA(i), &flags);
			p = nid_page_nr(i, j) - node_start_pfn(i);

			total++;
			if (PageReserved(p))
				reserved++;
			else if (PageSwapCache(p))
				cached++;
			else if (!page_count(p))
				free++;
			else
				shared += page_count(p) - 1;
			pgdat_resize_unlock(NODE_DATA(i), &flags);
        	}
	}
#endif
	printk(KERN_INFO "%d pages of RAM\n", total);
	printk(KERN_INFO "%d reserved pages\n", reserved);
	printk(KERN_INFO "%d pages shared\n", shared);
	printk(KERN_INFO "%d pages swap cached\n", cached);


#ifdef CONFIG_DISCONTIGMEM
	{
		struct zonelist *zl;
		int i, j;

		for (i = 0; i < npmem_ranges; i++) {
			zl = node_zonelist(i, 0);
			for (j = 0; j < MAX_NR_ZONES; j++) {
				struct zoneref *z;
				struct zone *zone;

				printk("Zone list for zone %d on node %d: ", j, i);
				for_each_zone_zonelist(zone, z, zl, j)
					printk("[%d/%s] ", zone_to_nid(zone),
								zone->name);
				printk("\n");
			}
		}
	}
#endif
}

static void __init pagetable_init(void)
{
	int range;

	

	for (range = 0; range < npmem_ranges; range++) {
		unsigned long start_paddr;
		unsigned long end_paddr;
		unsigned long size;

		start_paddr = pmem_ranges[range].start_pfn << PAGE_SHIFT;
		end_paddr = start_paddr + (pmem_ranges[range].pages << PAGE_SHIFT);
		size = pmem_ranges[range].pages << PAGE_SHIFT;

		map_pages((unsigned long)__va(start_paddr), start_paddr,
			  size, PAGE_KERNEL, 0);
	}

#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_end && initrd_end > mem_limit) {
		printk(KERN_INFO "initrd: mapping %08lx-%08lx\n", initrd_start, initrd_end);
		map_pages(initrd_start, __pa(initrd_start),
			  initrd_end - initrd_start, PAGE_KERNEL, 0);
	}
#endif

	empty_zero_page = alloc_bootmem_pages(PAGE_SIZE);
	memset(empty_zero_page, 0, PAGE_SIZE);
}

static void __init gateway_init(void)
{
	unsigned long linux_gateway_page_addr;
	extern void * const linux_gateway_page;

	linux_gateway_page_addr = LINUX_GATEWAY_ADDR & PAGE_MASK;


	map_pages(linux_gateway_page_addr, __pa(&linux_gateway_page),
		  PAGE_SIZE, PAGE_GATEWAY, 1);
}

#ifdef CONFIG_HPUX
void
map_hpux_gateway_page(struct task_struct *tsk, struct mm_struct *mm)
{
	pgd_t *pg_dir;
	pmd_t *pmd;
	pte_t *pg_table;
	unsigned long start_pmd;
	unsigned long start_pte;
	unsigned long address;
	unsigned long hpux_gw_page_addr;
	extern void * const hpux_gateway_page;

	hpux_gw_page_addr = HPUX_GATEWAY_ADDR & PAGE_MASK;


	pg_dir = pgd_offset(mm,hpux_gw_page_addr);

#if PTRS_PER_PMD == 1
	start_pmd = 0;
#else
	start_pmd = ((hpux_gw_page_addr >> PMD_SHIFT) & (PTRS_PER_PMD - 1));
#endif
	start_pte = ((hpux_gw_page_addr >> PAGE_SHIFT) & (PTRS_PER_PTE - 1));

	address = __pa(&hpux_gateway_page);
#if PTRS_PER_PMD == 1
	pmd = (pmd_t *)__pa(pg_dir);
#else
	pmd = (pmd_t *) pgd_address(*pg_dir);


	if (!pmd) {
		pmd = (pmd_t *) get_zeroed_page(GFP_KERNEL);
		pmd = (pmd_t *) __pa(pmd);
	}

	__pgd_val_set(*pg_dir, PxD_FLAG_PRESENT | PxD_FLAG_VALID | (unsigned long) pmd);
#endif
	

	pmd = (pmd_t *)__va(pmd) + start_pmd;


	pg_table = (pte_t *) pmd_address(*pmd);
	if (!pg_table)
		pg_table = (pte_t *) __pa(get_zeroed_page(GFP_KERNEL));

	__pmd_val_set(*pmd, PxD_FLAG_PRESENT | PxD_FLAG_VALID | (unsigned long) pg_table);

	

	pg_table = (pte_t *) __va(pg_table) + start_pte;
	set_pte(pg_table, __mk_pte(address, PAGE_GATEWAY));
}
EXPORT_SYMBOL(map_hpux_gateway_page);
#endif

void __init paging_init(void)
{
	int i;

	setup_bootmem();
	pagetable_init();
	gateway_init();
	flush_cache_all_local(); 
	flush_tlb_all_local(NULL);

	for (i = 0; i < npmem_ranges; i++) {
		unsigned long zones_size[MAX_NR_ZONES] = { 0, };

		zones_size[ZONE_NORMAL] = pmem_ranges[i].pages;

#ifdef CONFIG_DISCONTIGMEM
		{
		    int j;
		    for (j = (pmem_ranges[i].start_pfn >> PFNNID_SHIFT);
			 j <= ((pmem_ranges[i].start_pfn + pmem_ranges[i].pages) >> PFNNID_SHIFT);
			 j++) {
			pfnnid_map[j] = i;
		    }
		}
#endif

		free_area_init_node(i, zones_size,
				pmem_ranges[i].start_pfn, NULL);
	}
}

#ifdef CONFIG_PA20


#define NR_SPACE_IDS 262144

#else


#define NR_SPACE_IDS 32768

#endif  

#define RECYCLE_THRESHOLD (NR_SPACE_IDS / 2)
#define SID_ARRAY_SIZE  (NR_SPACE_IDS / (8 * sizeof(long)))

static unsigned long space_id[SID_ARRAY_SIZE] = { 1 }; 
static unsigned long dirty_space_id[SID_ARRAY_SIZE];
static unsigned long space_id_index;
static unsigned long free_space_ids = NR_SPACE_IDS - 1;
static unsigned long dirty_space_ids = 0;

static DEFINE_SPINLOCK(sid_lock);

unsigned long alloc_sid(void)
{
	unsigned long index;

	spin_lock(&sid_lock);

	if (free_space_ids == 0) {
		if (dirty_space_ids != 0) {
			spin_unlock(&sid_lock);
			flush_tlb_all(); 
			spin_lock(&sid_lock);
		}
		BUG_ON(free_space_ids == 0);
	}

	free_space_ids--;

	index = find_next_zero_bit(space_id, NR_SPACE_IDS, space_id_index);
	space_id[index >> SHIFT_PER_LONG] |= (1L << (index & (BITS_PER_LONG - 1)));
	space_id_index = index;

	spin_unlock(&sid_lock);

	return index << SPACEID_SHIFT;
}

void free_sid(unsigned long spaceid)
{
	unsigned long index = spaceid >> SPACEID_SHIFT;
	unsigned long *dirty_space_offset;

	dirty_space_offset = dirty_space_id + (index >> SHIFT_PER_LONG);
	index &= (BITS_PER_LONG - 1);

	spin_lock(&sid_lock);

	BUG_ON(*dirty_space_offset & (1L << index)); 

	*dirty_space_offset |= (1L << index);
	dirty_space_ids++;

	spin_unlock(&sid_lock);
}


#ifdef CONFIG_SMP
static void get_dirty_sids(unsigned long *ndirtyptr,unsigned long *dirty_array)
{
	int i;

	

	*ndirtyptr = dirty_space_ids;
	if (dirty_space_ids != 0) {
	    for (i = 0; i < SID_ARRAY_SIZE; i++) {
		dirty_array[i] = dirty_space_id[i];
		dirty_space_id[i] = 0;
	    }
	    dirty_space_ids = 0;
	}

	return;
}

static void recycle_sids(unsigned long ndirty,unsigned long *dirty_array)
{
	int i;

	

	if (ndirty != 0) {
		for (i = 0; i < SID_ARRAY_SIZE; i++) {
			space_id[i] ^= dirty_array[i];
		}

		free_space_ids += ndirty;
		space_id_index = 0;
	}
}

#else 

static void recycle_sids(void)
{
	int i;

	

	if (dirty_space_ids != 0) {
		for (i = 0; i < SID_ARRAY_SIZE; i++) {
			space_id[i] ^= dirty_space_id[i];
			dirty_space_id[i] = 0;
		}

		free_space_ids += dirty_space_ids;
		dirty_space_ids = 0;
		space_id_index = 0;
	}
}
#endif


#ifdef CONFIG_SMP

static unsigned long recycle_ndirty;
static unsigned long recycle_dirty_array[SID_ARRAY_SIZE];
static unsigned int recycle_inuse;

void flush_tlb_all(void)
{
	int do_recycle;

	do_recycle = 0;
	spin_lock(&sid_lock);
	if (dirty_space_ids > RECYCLE_THRESHOLD) {
	    BUG_ON(recycle_inuse);  
	    get_dirty_sids(&recycle_ndirty,recycle_dirty_array);
	    recycle_inuse++;
	    do_recycle++;
	}
	spin_unlock(&sid_lock);
	on_each_cpu(flush_tlb_all_local, NULL, 1);
	if (do_recycle) {
	    spin_lock(&sid_lock);
	    recycle_sids(recycle_ndirty,recycle_dirty_array);
	    recycle_inuse = 0;
	    spin_unlock(&sid_lock);
	}
}
#else
void flush_tlb_all(void)
{
	spin_lock(&sid_lock);
	flush_tlb_all_local(NULL);
	recycle_sids();
	spin_unlock(&sid_lock);
}
#endif

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	if (start >= end)
		return;
	printk(KERN_INFO "Freeing initrd memory: %ldk freed\n", (end - start) >> 10);
	for (; start < end; start += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(start));
		init_page_count(virt_to_page(start));
		free_page(start);
		num_physpages++;
		totalram_pages++;
	}
}
#endif
