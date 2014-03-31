#ifndef __ASM_GENERIC_DMA_H
#define __ASM_GENERIC_DMA_H
#define MAX_DMA_ADDRESS PAGE_OFFSET

extern int request_dma(unsigned int dmanr, const char *device_id);
extern void free_dma(unsigned int dmanr);

#endif 
