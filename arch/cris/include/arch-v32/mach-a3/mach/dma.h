#ifndef _ASM_ARCH_CRIS_DMA_H
#define _ASM_ARCH_CRIS_DMA_H


#define MAX_DMA_CHANNELS	12 

#define NETWORK_ETH_TX_DMA_NBR 0        
#define NETWORK_ETH_RX_DMA_NBR 1        

#define IO_PROC_DMA_TX_DMA_NBR 4        
#define IO_PROC_DMA_RX_DMA_NBR 5        

#define ASYNC_SER3_TX_DMA_NBR 2         
#define ASYNC_SER3_RX_DMA_NBR 3         

#define ASYNC_SER2_TX_DMA_NBR 6         
#define ASYNC_SER2_RX_DMA_NBR 7         

#define ASYNC_SER1_TX_DMA_NBR 4         
#define ASYNC_SER1_RX_DMA_NBR 5         

#define SYNC_SER_TX_DMA_NBR 6           
#define SYNC_SER_RX_DMA_NBR 7           

#define ASYNC_SER0_TX_DMA_NBR 0         
#define ASYNC_SER0_RX_DMA_NBR 1         

#define STRCOP_TX_DMA_NBR 2             
#define STRCOP_RX_DMA_NBR 3             

#define dma_eth0 dma_eth
#define dma_eth1 dma_eth

enum dma_owner {
	dma_eth,
	dma_ser0,
	dma_ser1,
	dma_ser2,
	dma_ser3,
	dma_ser4,
	dma_iop,
	dma_sser,
	dma_strp,
	dma_h264,
	dma_jpeg
};

int crisv32_request_dma(unsigned int dmanr, const char *device_id,
	unsigned options, unsigned bandwidth, enum dma_owner owner);
void crisv32_free_dma(unsigned int dmanr);

#define DMA_VERBOSE_ON_ERROR 1
#define DMA_PANIC_ON_ERROR (2|DMA_VERBOSE_ON_ERROR)
#define DMA_INT_MEM 4

#endif 
