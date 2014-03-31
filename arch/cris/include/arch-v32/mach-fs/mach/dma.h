#ifndef _ASM_ARCH_CRIS_DMA_H
#define _ASM_ARCH_CRIS_DMA_H


#define MAX_DMA_CHANNELS	10

#define NETWORK_ETH0_TX_DMA_NBR 0	
#define NETWORK_ETH0 RX_DMA_NBR 1	

#define IO_PROC_DMA0_TX_DMA_NBR 2	
#define IO_PROC_DMA0_RX_DMA_NBR 3	

#define ATA_TX_DMA_NBR 2		
#define ATA_RX_DMA_NBR 3		

#define ASYNC_SER2_TX_DMA_NBR 2		
#define ASYNC_SER2_RX_DMA_NBR 3		

#define IO_PROC_DMA1_TX_DMA_NBR 4	
#define IO_PROC_DMA1_RX_DMA_NBR 5	

#define ASYNC_SER1_TX_DMA_NBR 4		
#define ASYNC_SER1_RX_DMA_NBR 5		

#define SYNC_SER0_TX_DMA_NBR 4		
#define SYNC_SER0_RX_DMA_NBR 5		

#define EXTDMA0_TX_DMA_NBR 6		
#define EXTDMA1_RX_DMA_NBR 7		

#define ASYNC_SER0_TX_DMA_NBR 6		
#define ASYNC_SER0_RX_DMA_NBR 7		

#define SYNC_SER1_TX_DMA_NBR 6		
#define SYNC_SER1_RX_DMA_NBR 7		

#define NETWORK_ETH1_TX_DMA_NBR 6	
#define NETWORK_ETH1_RX_DMA_NBR 7	

#define EXTDMA2_TX_DMA_NBR 8		
#define EXTDMA3_RX_DMA_NBR 9		

#define STRCOP_TX_DMA_NBR 8		
#define STRCOP_RX_DMA_NBR 9		

#define ASYNC_SER3_TX_DMA_NBR 8		
#define ASYNC_SER3_RX_DMA_NBR 9		

enum dma_owner {
  dma_eth0,
  dma_eth1,
  dma_iop0,
  dma_iop1,
  dma_ser0,
  dma_ser1,
  dma_ser2,
  dma_ser3,
  dma_sser0,
  dma_sser1,
  dma_ata,
  dma_strp,
  dma_ext0,
  dma_ext1,
  dma_ext2,
  dma_ext3
};

int crisv32_request_dma(unsigned int dmanr, const char *device_id,
			unsigned options, unsigned bandwidth,
			enum dma_owner owner);
void crisv32_free_dma(unsigned int dmanr);

#define DMA_VERBOSE_ON_ERROR 1
#define DMA_PANIC_ON_ERROR (2|DMA_VERBOSE_ON_ERROR)
#define DMA_INT_MEM 4

#endif 
