/*
 * Written by Pat Gaughen (gone@us.ibm.com) Mar 2002
 *
 */

#ifndef _ASM_X86_MMZONE_32_H
#define _ASM_X86_MMZONE_32_H

#include <asm/smp.h>

#ifdef CONFIG_NUMA
extern struct pglist_data *node_data[];
#define NODE_DATA(nid)	(node_data[nid])

#include <asm/numaq.h>

extern void resume_map_numa_kva(pgd_t *pgd);

#else 

static inline void resume_map_numa_kva(pgd_t *pgd) {}

#endif 

#ifdef CONFIG_DISCONTIGMEM

#define MAX_NR_PAGES 16777216
#define MAX_SECTIONS 1024
#define PAGES_PER_SECTION (MAX_NR_PAGES/MAX_SECTIONS)

extern s8 physnode_map[];

static inline int pfn_to_nid(unsigned long pfn)
{
#ifdef CONFIG_NUMA
	return((int) physnode_map[(pfn) / PAGES_PER_SECTION]);
#else
	return 0;
#endif
}

static inline int pfn_valid(int pfn)
{
	int nid = pfn_to_nid(pfn);

	if (nid >= 0)
		return (pfn < node_end_pfn(nid));
	return 0;
}

#define early_pfn_valid(pfn)	pfn_valid((pfn))

#endif 

#ifdef CONFIG_NEED_MULTIPLE_NODES
#define bootmem_arch_preferred_node(__bdata, size, align, goal, limit)	\
	(NODE_DATA(0)->bdata)
#endif 

#endif 
