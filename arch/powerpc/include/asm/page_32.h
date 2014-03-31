#ifndef _ASM_POWERPC_PAGE_32_H
#define _ASM_POWERPC_PAGE_32_H

#if defined(CONFIG_PHYSICAL_ALIGN) && (CONFIG_PHYSICAL_START != 0)
#if (CONFIG_PHYSICAL_START % CONFIG_PHYSICAL_ALIGN) != 0
#error "CONFIG_PHYSICAL_START must be a multiple of CONFIG_PHYSICAL_ALIGN"
#endif
#endif

#define VM_DATA_DEFAULT_FLAGS	VM_DATA_DEFAULT_FLAGS32

#ifdef CONFIG_NOT_COHERENT_CACHE
#define ARCH_DMA_MINALIGN	L1_CACHE_BYTES
#endif

#ifdef CONFIG_PTE_64BIT
#define PTE_FLAGS_OFFSET	4	
#else
#define PTE_FLAGS_OFFSET	0
#endif

#ifdef CONFIG_PPC_256K_PAGES
#define PTE_SHIFT	(PAGE_SHIFT - PTE_T_LOG2 - 2)	
#else
#define PTE_SHIFT	(PAGE_SHIFT - PTE_T_LOG2)	
#endif

#ifndef __ASSEMBLY__
#ifdef CONFIG_PTE_64BIT
typedef unsigned long long pte_basic_t;
#else
typedef unsigned long pte_basic_t;
#endif

struct page;
extern void clear_pages(void *page, int order);
static inline void clear_page(void *page) { clear_pages(page, 0); }
extern void copy_page(void *to, void *from);

#include <asm-generic/getorder.h>

#define PGD_T_LOG2	(__builtin_ffs(sizeof(pgd_t)) - 1)
#define PTE_T_LOG2	(__builtin_ffs(sizeof(pte_t)) - 1)

#endif 

#endif 
