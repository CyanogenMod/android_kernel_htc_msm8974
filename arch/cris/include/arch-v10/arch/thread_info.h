#ifndef _ASM_ARCH_THREAD_INFO_H
#define _ASM_ARCH_THREAD_INFO_H

static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
        __asm__("and.d $sp,%0; ":"=r" (ti) : "0" (~8191UL));
        return ti;
}

#endif
