#ifndef __ASM_GENERIC_MMU_H
#define __ASM_GENERIC_MMU_H

#ifndef __ASSEMBLY__
typedef struct {
	struct vm_list_struct	*vmlist;
	unsigned long		end_brk;
} mm_context_t;
#endif

#endif 
