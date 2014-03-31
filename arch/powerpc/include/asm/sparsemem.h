#ifndef _ASM_POWERPC_SPARSEMEM_H
#define _ASM_POWERPC_SPARSEMEM_H 1
#ifdef __KERNEL__

#ifdef CONFIG_SPARSEMEM
#define SECTION_SIZE_BITS       24

#define MAX_PHYSADDR_BITS       44
#define MAX_PHYSMEM_BITS        44

#endif 

#ifdef CONFIG_MEMORY_HOTPLUG
extern int create_section_mapping(unsigned long start, unsigned long end);
extern int remove_section_mapping(unsigned long start, unsigned long end);
#ifdef CONFIG_NUMA
extern int hot_add_scn_to_nid(unsigned long scn_addr);
#else
static inline int hot_add_scn_to_nid(unsigned long scn_addr)
{
	return 0;
}
#endif 
#endif 

#endif 
#endif 
