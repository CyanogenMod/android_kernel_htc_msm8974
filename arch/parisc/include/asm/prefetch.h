
#ifndef __ASM_PARISC_PREFETCH_H
#define __ASM_PARISC_PREFETCH_H

#ifndef __ASSEMBLY__
#ifdef CONFIG_PREFETCH

#define ARCH_HAS_PREFETCH
static inline void prefetch(const void *addr)
{
	__asm__(
#ifndef CONFIG_PA20
		
		"	extrw,u,= %0,31,32,%%r0\n"
#endif
		"	ldw 0(%0), %%r0" : : "r" (addr));
}

#ifdef CONFIG_PA20
#define ARCH_HAS_PREFETCHW
static inline void prefetchw(const void *addr)
{
	__asm__("ldd 0(%0), %%r0" : : "r" (addr));
}
#endif 

#endif 
#endif 

#endif 
