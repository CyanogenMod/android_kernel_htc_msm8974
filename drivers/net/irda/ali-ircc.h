/*********************************************************************
 *                
 * Filename:      ali-ircc.h
 * Version:       0.5
 * Description:   Driver for the ALI M1535D and M1543C FIR Controller
 * Status:        Experimental.
 * Author:        Benjamin Kong <benjamin_kong@ali.com.tw>
 * Created at:    2000/10/16 03:46PM
 * Modified at:   2001/1/3 02:56PM
 * Modified by:   Benjamin Kong <benjamin_kong@ali.com.tw>
 * 
 *     Copyright (c) 2000 Benjamin Kong <benjamin_kong@ali.com.tw>
 *     All Rights Reserved
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 ********************************************************************/

#ifndef ALI_IRCC_H
#define ALI_IRCC_H

#include <linux/time.h>

#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/types.h>
#include <asm/io.h>


#define BANK0		0x20
#define BANK1		0x21
#define BANK2		0x22
#define BANK3		0x23

#define FIR_MCR		0x07	

#define FIR_DR		0x00	 
#define FIR_IER		0x01	
#define FIR_IIR		0x02	
#define FIR_LCR_A	0x03	
#define FIR_LCR_B	0x04	
#define FIR_LSR		0x05	
#define FIR_BSR		0x06	


	
	#define	IER_FIFO	0x10		
	#define	IER_TIMER	0x20 	 
	#define	IER_EOM		0x40	
	#define IER_ACT		0x80	
	
	
	#define IIR_FIFO	0x10	
	#define IIR_TIMER	0x20	
	#define IIR_EOM		0x40	
	#define IIR_ACT		0x80		
	
	
	#define LCR_A_FIFO_RESET 0x80	

	
	#define	LCR_B_BW	0x10	
	#define LCR_B_SIP	0x20	
	#define	LCR_B_TX_MODE 	0x40	
	#define LCR_B_RX_MODE	0x80	
	
		
	#define LSR_FIR_LSA	0x00	
	#define LSR_FRAME_ABORT	0x08	
	#define LSR_CRC_ERROR	0x10	
	#define LSR_SIZE_ERROR	0x20	
	#define LSR_FRAME_ERROR	0x40	
	#define LSR_FIFO_UR	0x80	
	#define LSR_FIFO_OR	0x80	
		
	
	#define BSR_FIFO_NOT_EMPTY	0x80	
	
#define	FIR_CR		0x00 	
#define FIR_FIFO_TR	0x01   	
#define FIR_DMA_TR	0x02	
#define FIR_TIMER_IIR	0x03	
#define FIR_FIFO_FR	0x03	
#define FIR_FIFO_RAR	0x04 	
#define FIR_FIFO_WAR	0x05	
#define FIR_TR		0x06	

	
	#define CR_DMA_EN	0x01	
	#define CR_DMA_BURST	0x02	
	#define CR_TIMER_EN 	0x08	
	
	
	#define TIMER_IIR_500	0x00	
	#define TIMER_IIR_1ms	0x01	
	#define TIMER_IIR_2ms	0x02	
	#define TIMER_IIR_4ms	0x03	
	
#define FIR_IRDA_CR	0x00	
#define FIR_BOF_CR	0x01	
#define FIR_BW_CR	0x02	
#define FIR_TX_DSR_HI	0x03	
#define FIR_TX_DSR_LO	0x04	
#define FIR_RX_DSR_HI	0x05	
#define FIR_RX_DSR_LO	0x06	
	
	
	#define IRDA_CR_HDLC1152 0x80	
	#define IRDA_CR_CRC	0X40	
	#define IRDA_CR_HDLC	0x20	
	#define IRDA_CR_HP_MODE 0x10	
	#define IRDA_CR_SD_ST	0x08	
	#define IRDA_CR_FIR_SIN 0x04	
	#define IRDA_CR_ITTX_0	0x02	
	#define IRDA_CR_ITTX_1	0x03	
	
#define FIR_ID_VR	0x00	
#define FIR_MODULE_CR	0x01	
#define FIR_IO_BASE_HI	0x02	
#define FIR_IO_BASE_LO	0x03	
#define FIR_IRQ_CR	0x04	
#define FIR_DMA_CR	0x05	

struct ali_chip {
	char *name;
	int cfg[2];
	unsigned char entr1;
	unsigned char entr2;
	unsigned char cid_index;
	unsigned char cid_value;
	int (*probe)(struct ali_chip *chip, chipio_t *info);
	int (*init)(struct ali_chip *chip, chipio_t *info); 
};
typedef struct ali_chip ali_chip_t;


#define DMA_TX_MODE     0x08    
#define DMA_RX_MODE     0x04    

#define MAX_TX_WINDOW 	7
#define MAX_RX_WINDOW 	7

#define TX_FIFO_Threshold	8
#define RX_FIFO_Threshold	1
#define TX_DMA_Threshold	1
#define RX_DMA_Threshold	1


struct st_fifo_entry {
	int status;
	int len;
};

struct st_fifo {
	struct st_fifo_entry entries[MAX_RX_WINDOW];
	int pending_bytes;
	int head;
	int tail;
	int len;
};

struct frame_cb {
	void *start; 
	int len;     
};

struct tx_fifo {
	struct frame_cb queue[MAX_TX_WINDOW]; 
	int             ptr;                  
	int             len;                  
	int             free;                 
	void           *tail;                 
};

struct ali_ircc_cb {

	struct st_fifo st_fifo;    
	struct tx_fifo tx_fifo;    

	struct net_device *netdev;     
	
	struct irlap_cb *irlap;    
	struct qos_info qos;       
	
	chipio_t io;               
	iobuff_t tx_buff;          
	iobuff_t rx_buff;          
	dma_addr_t tx_buff_dma;
	dma_addr_t rx_buff_dma;

	__u8 ier;                  
	
	__u8 InterruptID;	   	
	__u8 BusStatus;		   	
	__u8 LineStatus;	   	
	
	unsigned char rcvFramesOverflow;
		
	struct timeval stamp;
	struct timeval now;

	spinlock_t lock;           
	
	__u32 new_speed;
	int index;                 
	
	unsigned char fifo_opti_buf;
};

static inline void switch_bank(int iobase, int bank)
{
		outb(bank, iobase+FIR_MCR);
}

#endif 
