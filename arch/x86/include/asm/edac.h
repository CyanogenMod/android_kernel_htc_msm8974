#ifndef _ASM_X86_EDAC_H
#define _ASM_X86_EDAC_H


static inline void atomic_scrub(void *va, u32 size)
{
	u32 i, *virt_addr = va;

	for (i = 0; i < size / 4; i++, virt_addr++)
		asm volatile("lock; addl $0, %0"::"m" (*virt_addr));
}

#endif 
