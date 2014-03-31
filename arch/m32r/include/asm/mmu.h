#ifndef _ASM_M32R_MMU_H
#define _ASM_M32R_MMU_H

#if !defined(CONFIG_MMU)

typedef struct {
	unsigned long		end_brk;
} mm_context_t;

#else 

#ifndef CONFIG_SMP
typedef unsigned long mm_context_t;
#else
typedef unsigned long mm_context_t[NR_CPUS];
#endif

#endif 

#endif 
