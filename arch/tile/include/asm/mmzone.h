/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_MMZONE_H
#define _ASM_TILE_MMZONE_H

extern struct pglist_data node_data[];
#define NODE_DATA(nid)	(&node_data[nid])

extern void get_memcfg_numa(void);

#ifdef CONFIG_DISCONTIGMEM

#include <asm/page.h>

extern int highbits_to_node[];

static inline int pfn_to_nid(unsigned long pfn)
{
	return highbits_to_node[__pfn_to_highbits(pfn)];
}

#define kern_addr_valid(kaddr)	virt_addr_valid((void *)kaddr)

static inline int pfn_valid(int pfn)
{
	int nid = pfn_to_nid(pfn);

	if (nid >= 0)
		return (pfn < node_end_pfn(nid));
	return 0;
}

extern unsigned long node_start_pfn[];
extern unsigned long node_end_pfn[];
extern unsigned long node_memmap_pfn[];
extern unsigned long node_percpu_pfn[];
extern unsigned long node_free_pfn[];
#ifdef CONFIG_HIGHMEM
extern unsigned long node_lowmem_end_pfn[];
#endif
#ifdef CONFIG_PCI
extern unsigned long pci_reserve_start_pfn;
extern unsigned long pci_reserve_end_pfn;
#endif

#endif 

#endif 
