
#ifndef _ASM_DMA_H
#define _ASM_DMA_H

#include <arch/dma.h>


#define MAX_DMA_ADDRESS PAGE_OFFSET


#ifdef CONFIG_PCI
extern int isa_dma_bridge_buggy;
#else
#define isa_dma_bridge_buggy 	(0)
#endif

#endif 
