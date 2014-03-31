/* Copyright (C) 2005-2006 by Texas Instruments */

#ifndef _CPPI_DMA_H_
#define _CPPI_DMA_H_

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/dmapool.h>

#include "musb_dma.h"
#include "musb_core.h"



#include "davinci.h"



struct cppi_tx_stateram {
	u32 tx_head;			
	u32 tx_buf;
	u32 tx_current;			
	u32 tx_buf_current;
	u32 tx_info;			
	u32 tx_rem_len;
	u32 tx_dummy;			
	u32 tx_complete;
};

struct cppi_rx_stateram {
	u32 rx_skipbytes;
	u32 rx_head;
	u32 rx_sop;			
	u32 rx_current;			
	u32 rx_buf_current;
	u32 rx_len_len;
	u32 rx_cnt_cnt;
	u32 rx_complete;
};

#define CPPI_SOP_SET	((u32)(1 << 31))
#define CPPI_EOP_SET	((u32)(1 << 30))
#define CPPI_OWN_SET	((u32)(1 << 29))	
#define CPPI_EOQ_MASK	((u32)(1 << 28))
#define CPPI_ZERO_SET	((u32)(1 << 23))	
#define CPPI_RXABT_MASK	((u32)(1 << 19))	

#define CPPI_RECV_PKTLEN_MASK 0xFFFF
#define CPPI_BUFFER_LEN_MASK 0xFFFF

#define CPPI_TEAR_READY ((u32)(1 << 31))


#define	CPPI_DESCRIPTOR_ALIGN	16	

struct cppi_descriptor {
	
	u32		hw_next;	
	u32		hw_bufp;	
	u32		hw_off_len;	
	u32		hw_options;	

	struct cppi_descriptor *next;
	dma_addr_t	dma;		
	u32		buflen;		
} __attribute__ ((aligned(CPPI_DESCRIPTOR_ALIGN)));


struct cppi;

struct cppi_channel {
	struct dma_channel	channel;

	
	struct cppi		*controller;

	
	struct musb_hw_ep	*hw_ep;
	bool			transmit;
	u8			index;

	
	u8			is_rndis;

	
	dma_addr_t		buf_dma;
	u32			buf_len;
	u32			maxpacket;
	u32			offset;		

	void __iomem		*state_ram;	

	struct cppi_descriptor	*freelist;

	
	struct cppi_descriptor	*head;
	struct cppi_descriptor	*tail;
	struct cppi_descriptor	*last_processed;

	struct list_head	tx_complete;
};

struct cppi {
	struct dma_controller		controller;
	struct musb			*musb;
	void __iomem			*mregs;		
	void __iomem			*tibase;	

	int				irq;

	struct cppi_channel		tx[4];
	struct cppi_channel		rx[4];

	struct dma_pool			*pool;

	struct list_head		tx_complete;
};

extern irqreturn_t cppi_interrupt(int, void *);

#endif				
