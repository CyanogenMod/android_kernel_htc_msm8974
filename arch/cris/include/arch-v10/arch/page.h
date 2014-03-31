#ifndef _CRIS_ARCH_PAGE_H
#define _CRIS_ARCH_PAGE_H


#ifdef __KERNEL__

#ifdef CONFIG_CRIS_LOW_MAP
#define PAGE_OFFSET		KSEG_6   
#else
#define PAGE_OFFSET		KSEG_C   
#endif


#ifdef CONFIG_CRIS_LOW_MAP
#define __pa(x)                 ((unsigned long)(x) & 0xdfffffff)
#define __va(x)                 ((void *)((unsigned long)(x) | 0x20000000))
#else
#define __pa(x)                 ((unsigned long)(x) & 0x7fffffff)
#define __va(x)                 ((void *)((unsigned long)(x) | 0x80000000))
#endif

#endif
#endif
