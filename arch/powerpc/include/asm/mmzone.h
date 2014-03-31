/*
 * Written by Kanoj Sarcar (kanoj@sgi.com) Aug 99
 *
 * PowerPC64 port:
 * Copyright (C) 2002 Anton Blanchard, IBM Corp.
 */
#ifndef _ASM_MMZONE_H_
#define _ASM_MMZONE_H_
#ifdef __KERNEL__

#include <linux/cpumask.h>


#ifdef CONFIG_NEED_MULTIPLE_NODES

extern struct pglist_data *node_data[];
#define NODE_DATA(nid)		(node_data[nid])


extern int numa_cpu_lookup_table[];
extern cpumask_var_t node_to_cpumask_map[];
#ifdef CONFIG_MEMORY_HOTPLUG
extern unsigned long max_pfn;
u64 memory_hotplug_max(void);
#else
#define memory_hotplug_max() memblock_end_of_DRAM()
#endif

#else
#define memory_hotplug_max() memblock_end_of_DRAM()
#endif 

#endif 
#endif 
