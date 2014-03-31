#ifndef _ASM_IA64_MMAN_H
#define _ASM_IA64_MMAN_H


#include <asm-generic/mman.h>

#define MAP_GROWSUP	0x0200		

#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#define arch_mmap_check	ia64_mmap_check
int ia64_mmap_check(unsigned long addr, unsigned long len,
		unsigned long flags);
#endif
#endif

#endif 
