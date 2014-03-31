#ifndef _ASM_X86_MMU_H
#define _ASM_X86_MMU_H

#include <linux/spinlock.h>
#include <linux/mutex.h>

typedef struct {
	void *ldt;
	int size;

#ifdef CONFIG_X86_64
	
	unsigned short ia32_compat;
#endif

	struct mutex lock;
	void *vdso;
} mm_context_t;

#ifdef CONFIG_SMP
void leave_mm(int cpu);
#else
static inline void leave_mm(int cpu)
{
}
#endif

#endif 
