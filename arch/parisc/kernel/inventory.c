/*
 * inventory.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Copyright (c) 1999 The Puffin Group (David Kennedy and Alex deVries)
 * Copyright (c) 2001 Matthew Wilcox for Hewlett-Packard
 *
 * These are the routines to discover what hardware exists in this box.
 * This task is complicated by there being 3 different ways of
 * performing an inventory, depending largely on the age of the box.
 * The recommended way to do this is to check to see whether the machine
 * is a `Snake' first, then try System Map, then try PAT.  We try System
 * Map before checking for a Snake -- this probably doesn't cause any
 * problems, but...
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/mmzone.h>
#include <asm/pdc.h>
#include <asm/pdcpat.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/parisc-device.h>

#undef DEBUG_PAT

int pdc_type __read_mostly = PDC_TYPE_ILLEGAL;

void __init setup_pdc(void)
{
	long status;
	unsigned int bus_id;
	struct pdc_system_map_mod_info module_result;
	struct pdc_module_path module_path;
	struct pdc_model model;
#ifdef CONFIG_64BIT
	struct pdc_pat_cell_num cell_info;
#endif

	

	printk(KERN_INFO "Determining PDC firmware type: ");

	status = pdc_system_map_find_mods(&module_result, &module_path, 0);
	if (status == PDC_OK) {
		pdc_type = PDC_TYPE_SYSTEM_MAP;
		printk("System Map.\n");
		return;
	}



#ifdef CONFIG_64BIT
	status = pdc_pat_cell_get_number(&cell_info);
	if (status == PDC_OK) {
		pdc_type = PDC_TYPE_PAT;
		printk("64 bit PAT.\n");
		return;
	}
#endif

	

	status = pdc_model_info(&model);

	bus_id = (model.hversion >> (4 + 7)) & 0x1f;

	switch (bus_id) {
	case 0x4:		
	case 0x6:		
	case 0x7:		
	case 0x8:		
	case 0xA:		
	case 0xC:		

		pdc_type = PDC_TYPE_SNAKE;
		printk("Snake.\n");
		return;

	default:		

		printk("Unsupported.\n");
		panic("If this is a 64-bit machine, please try a 64-bit kernel.\n");
	}
}

#define PDC_PAGE_ADJ_SHIFT (PAGE_SHIFT - 12) 

static void __init
set_pmem_entry(physmem_range_t *pmem_ptr, unsigned long start,
	       unsigned long pages4k)
{

	if (unlikely( ((start & (PAGE_SIZE - 1)) != 0)
	    || ((pages4k & ((1UL << PDC_PAGE_ADJ_SHIFT) - 1)) != 0) )) {

		panic("Memory range doesn't align with page size!\n");
	}

	pmem_ptr->start_pfn = (start >> PAGE_SHIFT);
	pmem_ptr->pages = (pages4k >> PDC_PAGE_ADJ_SHIFT);
}

static void __init pagezero_memconfig(void)
{
	unsigned long npages;


	npages = (PAGE_ALIGN(PAGE0->imm_max_mem) >> PAGE_SHIFT);
	set_pmem_entry(pmem_ranges,0UL,npages);
	npmem_ranges = 1;
}

#ifdef CONFIG_64BIT



static int __init 
pat_query_module(ulong pcell_loc, ulong mod_index)
{
	pdc_pat_cell_mod_maddr_block_t *pa_pdc_cell;
	unsigned long bytecnt;
	unsigned long temp;	
	long status;		
	struct parisc_device *dev;

	pa_pdc_cell = kmalloc(sizeof (*pa_pdc_cell), GFP_KERNEL);
	if (!pa_pdc_cell)
		panic("couldn't allocate memory for PDC_PAT_CELL!");

	
	status = pdc_pat_cell_module(&bytecnt, pcell_loc, mod_index,
				     PA_VIEW, pa_pdc_cell);

	if (status != PDC_OK) {
		
		return status;
	}

	temp = pa_pdc_cell->cba;
	dev = alloc_pa_dev(PAT_GET_CBA(temp), &(pa_pdc_cell->mod_path));
	if (!dev) {
		return PDC_OK;
	}

	

	dev->pcell_loc = pcell_loc;
	dev->mod_index = mod_index;

	
	
	dev->mod_info = pa_pdc_cell->mod_info;	
	dev->pmod_loc = pa_pdc_cell->mod_location;

	register_parisc_device(dev);	

#ifdef DEBUG_PAT
	pdc_pat_cell_mod_maddr_block_t io_pdc_cell;
	
	switch (PAT_GET_ENTITY(dev->mod_info)) {
		unsigned long i;

	case PAT_ENTITY_PROC:
		printk(KERN_DEBUG "PAT_ENTITY_PROC: id_eid 0x%lx\n",
			pa_pdc_cell->mod[0]);
		break;

	case PAT_ENTITY_MEM:
		printk(KERN_DEBUG 
			"PAT_ENTITY_MEM: amount 0x%lx min_gni_base 0x%lx min_gni_len 0x%lx\n",
			pa_pdc_cell->mod[0], pa_pdc_cell->mod[1],
			pa_pdc_cell->mod[2]);
		break;
	case PAT_ENTITY_CA:
		printk(KERN_DEBUG "PAT_ENTITY_CA: %ld\n", pcell_loc);
		break;

	case PAT_ENTITY_PBC:
		printk(KERN_DEBUG "PAT_ENTITY_PBC: ");
		goto print_ranges;

	case PAT_ENTITY_SBA:
		printk(KERN_DEBUG "PAT_ENTITY_SBA: ");
		goto print_ranges;

	case PAT_ENTITY_LBA:
		printk(KERN_DEBUG "PAT_ENTITY_LBA: ");

 print_ranges:
		pdc_pat_cell_module(&bytecnt, pcell_loc, mod_index,
				    IO_VIEW, &io_pdc_cell);
		printk(KERN_DEBUG "ranges %ld\n", pa_pdc_cell->mod[1]);
		for (i = 0; i < pa_pdc_cell->mod[1]; i++) {
			printk(KERN_DEBUG 
				"  PA_VIEW %ld: 0x%016lx 0x%016lx 0x%016lx\n", 
				i, pa_pdc_cell->mod[2 + i * 3],	
				pa_pdc_cell->mod[3 + i * 3],	
				pa_pdc_cell->mod[4 + i * 3]);	
			printk(KERN_DEBUG 
				"  IO_VIEW %ld: 0x%016lx 0x%016lx 0x%016lx\n", 
				i, io_pdc_cell->mod[2 + i * 3],	
				io_pdc_cell->mod[3 + i * 3],	
				io_pdc_cell->mod[4 + i * 3]);	
		}
		printk(KERN_DEBUG "\n");
		break;
	}
#endif 

	kfree(pa_pdc_cell);

	return PDC_OK;
}



#define PAT_MAX_RANGES (4 * MAX_PHYSMEM_RANGES)

static void __init pat_memconfig(void)
{
	unsigned long actual_len;
	struct pdc_pat_pd_addr_map_entry mem_table[PAT_MAX_RANGES+1];
	struct pdc_pat_pd_addr_map_entry *mtbl_ptr;
	physmem_range_t *pmem_ptr;
	long status;
	int entries;
	unsigned long length;
	int i;

	length = (PAT_MAX_RANGES + 1) * sizeof(struct pdc_pat_pd_addr_map_entry);

	status = pdc_pat_pd_get_addr_map(&actual_len, mem_table, length, 0L);

	if ((status != PDC_OK)
	    || ((actual_len % sizeof(struct pdc_pat_pd_addr_map_entry)) != 0)) {


		printk("\n\n\n");
		printk(KERN_WARNING "WARNING! Could not get full memory configuration. "
			"All memory may not be used!\n\n\n");
		pagezero_memconfig();
		return;
	}

	entries = actual_len / sizeof(struct pdc_pat_pd_addr_map_entry);

	if (entries > PAT_MAX_RANGES) {
		printk(KERN_WARNING "This Machine has more memory ranges than we support!\n");
		printk(KERN_WARNING "Some memory may not be used!\n");
	}


	npmem_ranges = 0;
	mtbl_ptr = mem_table;
	pmem_ptr = pmem_ranges; 
	for (i = 0; i < entries; i++,mtbl_ptr++) {
		if (   (mtbl_ptr->entry_type != PAT_MEMORY_DESCRIPTOR)
		    || (mtbl_ptr->memory_type != PAT_MEMTYPE_MEMORY)
		    || (mtbl_ptr->pages == 0)
		    || (   (mtbl_ptr->memory_usage != PAT_MEMUSE_GENERAL)
			&& (mtbl_ptr->memory_usage != PAT_MEMUSE_GI)
			&& (mtbl_ptr->memory_usage != PAT_MEMUSE_GNI) ) ) {

			continue;
		}

		if (npmem_ranges == MAX_PHYSMEM_RANGES) {
			printk(KERN_WARNING "This Machine has more memory ranges than we support!\n");
			printk(KERN_WARNING "Some memory will not be used!\n");
			break;
		}

		set_pmem_entry(pmem_ptr++,mtbl_ptr->paddr,mtbl_ptr->pages);
		npmem_ranges++;
	}
}

static int __init pat_inventory(void)
{
	int status;
	ulong mod_index = 0;
	struct pdc_pat_cell_num cell_info;

	status = pdc_pat_cell_get_number(&cell_info);
	if (status != PDC_OK) {
		return 0;
	}

#ifdef DEBUG_PAT
	printk(KERN_DEBUG "CELL_GET_NUMBER: 0x%lx 0x%lx\n", cell_info.cell_num, 
	       cell_info.cell_loc);
#endif

	while (PDC_OK == pat_query_module(cell_info.cell_loc, mod_index)) {
		mod_index++;
	}

	return mod_index;
}

static void __init sprockets_memconfig(void)
{
	struct pdc_memory_table_raddr r_addr;
	struct pdc_memory_table mem_table[MAX_PHYSMEM_RANGES];
	struct pdc_memory_table *mtbl_ptr;
	physmem_range_t *pmem_ptr;
	long status;
	int entries;
	int i;

	status = pdc_mem_mem_table(&r_addr,mem_table,
				(unsigned long)MAX_PHYSMEM_RANGES);

	if (status != PDC_OK) {


		pagezero_memconfig();
		return;
	}

	if (r_addr.entries_total > MAX_PHYSMEM_RANGES) {
		printk(KERN_WARNING "This Machine has more memory ranges than we support!\n");
		printk(KERN_WARNING "Some memory will not be used!\n");
	}

	entries = (int)r_addr.entries_returned;

	npmem_ranges = 0;
	mtbl_ptr = mem_table;
	pmem_ptr = pmem_ranges; 
	for (i = 0; i < entries; i++,mtbl_ptr++) {
		set_pmem_entry(pmem_ptr++,mtbl_ptr->paddr,mtbl_ptr->pages);
		npmem_ranges++;
	}
}

#else   

#define pat_inventory() do { } while (0)
#define pat_memconfig() do { } while (0)
#define sprockets_memconfig() pagezero_memconfig()

#endif	


#ifndef CONFIG_PA20


static struct parisc_device * __init
legacy_create_device(struct pdc_memory_map *r_addr,
		struct pdc_module_path *module_path)
{
	struct parisc_device *dev;
	int status = pdc_mem_map_hpa(r_addr, module_path);
	if (status != PDC_OK)
		return NULL;

	dev = alloc_pa_dev(r_addr->hpa, &module_path->path);
	if (dev == NULL)
		return NULL;

	register_parisc_device(dev);
	return dev;
}

static void __init snake_inventory(void)
{
	int mod;
	for (mod = 0; mod < 16; mod++) {
		struct parisc_device *dev;
		struct pdc_module_path module_path;
		struct pdc_memory_map r_addr;
		unsigned int func;

		memset(module_path.path.bc, 0xff, 6);
		module_path.path.mod = mod;
		dev = legacy_create_device(&r_addr, &module_path);
		if ((!dev) || (dev->id.hw_type != HPHW_BA))
			continue;

		memset(module_path.path.bc, 0xff, 4);
		module_path.path.bc[4] = mod;

		for (func = 0; func < 16; func++) {
			module_path.path.bc[5] = 0;
			module_path.path.mod = func;
			legacy_create_device(&r_addr, &module_path);
		}
	}
}

#else 
#define snake_inventory() do { } while (0)
#endif  


static void __init
add_system_map_addresses(struct parisc_device *dev, int num_addrs, 
			 int module_instance)
{
	int i;
	long status;
	struct pdc_system_map_addr_info addr_result;

	dev->addr = kmalloc(num_addrs * sizeof(unsigned long), GFP_KERNEL);
	if(!dev->addr) {
		printk(KERN_ERR "%s %s(): memory allocation failure\n",
		       __FILE__, __func__);
		return;
	}

	for(i = 1; i <= num_addrs; ++i) {
		status = pdc_system_map_find_addrs(&addr_result, 
						   module_instance, i);
		if(PDC_OK == status) {
			dev->addr[dev->num_addrs] = (unsigned long)addr_result.mod_addr;
			dev->num_addrs++;
		} else {
			printk(KERN_WARNING 
			       "Bad PDC_FIND_ADDRESS status return (%ld) for index %d\n",
			       status, i);
		}
	}
}

static void __init system_map_inventory(void)
{
	int i;
	long status = PDC_OK;
    
	for (i = 0; i < 256; i++) {
		struct parisc_device *dev;
		struct pdc_system_map_mod_info module_result;
		struct pdc_module_path module_path;

		status = pdc_system_map_find_mods(&module_result,
				&module_path, i);
		if ((status == PDC_BAD_PROC) || (status == PDC_NE_MOD))
			break;
		if (status != PDC_OK)
			continue;

		dev = alloc_pa_dev(module_result.mod_addr, &module_path.path);
		if (!dev)
			continue;
		
		register_parisc_device(dev);

		
		if (!module_result.add_addrs)
			continue;

		add_system_map_addresses(dev, module_result.add_addrs, i);
	}

	walk_central_bus();
	return;
}

void __init do_memory_inventory(void)
{
	switch (pdc_type) {

	case PDC_TYPE_PAT:
		pat_memconfig();
		break;

	case PDC_TYPE_SYSTEM_MAP:
		sprockets_memconfig();
		break;

	case PDC_TYPE_SNAKE:
		pagezero_memconfig();
		return;

	default:
		panic("Unknown PDC type!\n");
	}

	if (npmem_ranges == 0 || pmem_ranges[0].start_pfn != 0) {
		printk(KERN_WARNING "Bad memory configuration returned!\n");
		printk(KERN_WARNING "Some memory may not be used!\n");
		pagezero_memconfig();
	}
}

void __init do_device_inventory(void)
{
	printk(KERN_INFO "Searching for devices...\n");

	init_parisc_bus();

	switch (pdc_type) {

	case PDC_TYPE_PAT:
		pat_inventory();
		break;

	case PDC_TYPE_SYSTEM_MAP:
		system_map_inventory();
		break;

	case PDC_TYPE_SNAKE:
		snake_inventory();
		break;

	default:
		panic("Unknown PDC type!\n");
	}
	printk(KERN_INFO "Found devices:\n");
	print_parisc_devices();
}
