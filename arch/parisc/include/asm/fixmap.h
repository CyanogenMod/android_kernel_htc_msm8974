#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H


#define TMPALIAS_MAP_START	((__PAGE_OFFSET) - 16*1024*1024)
#define KERNEL_MAP_START	(GATEWAY_PAGE_SIZE)
#define KERNEL_MAP_END		(TMPALIAS_MAP_START)

#ifndef __ASSEMBLY__
extern void *parisc_vmalloc_start;
#define PCXL_DMA_MAP_SIZE	(8*1024*1024)
#define VMALLOC_START		((unsigned long)parisc_vmalloc_start)
#define VMALLOC_END		(KERNEL_MAP_END)
#endif 

#endif 
