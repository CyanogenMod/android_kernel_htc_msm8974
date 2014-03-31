#ifndef _ASM_CRIS_SVINTO_H
#define _ASM_CRIS_SVINTO_H

#include "sv_addr_ag.h"

extern unsigned int genconfig_shadow; 


enum {                          
	d_eol      = (1 << 0),  
	d_eop      = (1 << 1),  
	d_wait     = (1 << 2),  
	d_int      = (1 << 3),  
	d_txerr    = (1 << 4),  
	d_stop     = (1 << 4),  
	d_ecp      = (1 << 4),  
	d_pri      = (1 << 5),  
	d_alignerr = (1 << 6),  
	d_crcerr   = (1 << 7)   
};


typedef struct etrax_dma_descr {
	unsigned short sw_len;                
	unsigned short ctrl;                  
	unsigned long  next;                  
	unsigned long  buf;                   
	unsigned short hw_len;                
	unsigned char  status;                
	unsigned char  fifo_len;              
} etrax_dma_descr;


#define RESET_DMA_NUM( n ) \
  *R_DMA_CH##n##_CMD = IO_STATE( R_DMA_CH0_CMD, cmd, reset )

#define RESET_DMA( n ) RESET_DMA_NUM( n )


#define WAIT_DMA_NUM( n ) \
  while( (*R_DMA_CH##n##_CMD & IO_MASK( R_DMA_CH0_CMD, cmd )) != \
         IO_STATE( R_DMA_CH0_CMD, cmd, hold ) )

#define WAIT_DMA( n ) WAIT_DMA_NUM( n )

extern void prepare_rx_descriptor(struct etrax_dma_descr *desc);
extern void flush_etrax_cache(void);

#endif
