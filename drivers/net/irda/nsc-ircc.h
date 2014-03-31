/*********************************************************************
 *                
 * Filename:      nsc-ircc.h
 * Version:       
 * Description:   
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Fri Nov 13 14:37:40 1998
 * Modified at:   Sun Jan 23 17:47:00 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-2000 Dag Brattli <dagb@cs.uit.no>
 *     Copyright (c) 1998 Lichen Wang, <lwang@actisys.com>
 *     Copyright (c) 1998 Actisys Corp., www.actisys.com
 *     All Rights Reserved
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *     
 ********************************************************************/

#ifndef NSC_IRCC_H
#define NSC_IRCC_H

#include <linux/time.h>

#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/types.h>
#include <asm/io.h>

#define NSC_FORCE_DONGLE_TYPE9	0x00000001

#define DMA_TX_MODE     0x08    
#define DMA_RX_MODE     0x04    

#define CFG_108_BAIC 0x00
#define CFG_108_CSRT 0x01
#define CFG_108_MCTL 0x02

#define CFG_338_FER  0x00
#define CFG_338_FAR  0x01
#define CFG_338_PTR  0x02
#define CFG_338_PNP0 0x1b
#define CFG_338_PNP1 0x1c
#define CFG_338_PNP3 0x4f

#define CFG_39X_LDN	0x07	
#define CFG_39X_SIOCF1	0x21	
#define CFG_39X_ACT	0x30	
#define CFG_39X_BASEH	0x60	
#define CFG_39X_BASEL	0x61	
#define CFG_39X_IRQNUM	0x70	
#define CFG_39X_IRQSEL	0x71	
#define CFG_39X_DMA0	0x74	
#define CFG_39X_DMA1	0x75	
#define CFG_39X_SPC	0xF0	

#define APEDCRC		0x02
#define ENBNKSEL	0x01

#define TXD             0x00 
#define RXD             0x00 

#define IER		0x01 
#define IER_RXHDL_IE    0x01 
#define IER_TXLDL_IE    0x02 
#define IER_LS_IE	0x04
#define IER_ETXURI      0x04 
#define IER_DMA_IE	0x10 
#define IER_TXEMP_IE    0x20
#define IER_SFIF_IE     0x40 
#define IER_TMR_IE      0x80 

#define FCR		0x02 
#define FCR_FIFO_EN     0x01 
#define FCR_RXSR        0x02 
#define FCR_TXSR        0x04 
#define FCR_RXTH	0x40 
#define FCR_TXTH	0x20 

#define EIR		0x02 
#define EIR_RXHDL_EV	0x01
#define EIR_TXLDL_EV    0x02
#define EIR_LS_EV	0x04
#define EIR_DMA_EV	0x10
#define EIR_TXEMP_EV	0x20
#define EIR_SFIF_EV     0x40
#define EIR_TMR_EV      0x80

#define LCR             0x03 
#define LCR_WLS_8       0x03 

#define BSR 	        0x03 
#define BSR_BKSE        0x80
#define BANK0 	        LCR_WLS_8 
#define BANK1	        0x80
#define BANK2	        0xe0
#define BANK3	        0xe4
#define BANK4	        0xe8
#define BANK5	        0xec
#define BANK6	        0xf0
#define BANK7     	0xf4

#define MCR		0x04 
#define MCR_MODE_MASK	~(0xd0)
#define MCR_UART        0x00
#define MCR_RESERVED  	0x20	
#define MCR_SHARP_IR    0x40
#define MCR_SIR         0x60
#define MCR_MIR  	0x80
#define MCR_FIR		0xa0
#define MCR_CEIR        0xb0
#define MCR_IR_PLS      0x10
#define MCR_DMA_EN	0x04
#define MCR_EN_IRQ	0x08
#define MCR_TX_DFR	0x08

#define LSR             0x05 
#define LSR_RXDA        0x01 
#define LSR_TXRDY       0x20 
#define LSR_TXEMP       0x40 

#define ASCR            0x07 
#define ASCR_RXF_TOUT   0x01 
#define ASCR_FEND_INF   0x02 
#define ASCR_S_EOT      0x04 
#define ASCT_RXBSY      0x20 
#define ASCR_TXUR       0x40 
#define ASCR_CTE        0x80 

#define BGDL            0x00 
#define BGDH            0x01 

#define ECR1		0x02 
#define ECR1_EXT_SL	0x01 
#define ECR1_DMANF	0x02 
#define ECR1_DMATH      0x04 
#define ECR1_DMASWP	0x08 

#define EXCR2		0x04
#define EXCR2_TFSIZ	0x01 
#define EXCR2_RFSIZ	0x04 

#define TXFLV           0x06 
#define RXFLV           0x07 

#define MID		0x00

#define TMRL            0x00 
#define TMRH            0x01 
#define IRCR1           0x02 
#define IRCR1_TMR_EN    0x01 

#define TFRLL		0x04
#define TFRLH		0x05
#define RFRLL		0x06
#define RFRLH		0x07

#define IRCR2           0x04 
#define IRCR2_MDRS      0x04 
#define IRCR2_FEND_MD   0x20 

#define FRM_ST          0x05 
#define FRM_ST_VLD      0x80 
#define FRM_ST_ERR_MSK  0x5f
#define FRM_ST_LOST_FR  0x40 
#define FRM_ST_MAX_LEN  0x10 
#define FRM_ST_PHY_ERR  0x08 
#define FRM_ST_BAD_CRC  0x04 
#define FRM_ST_OVR1     0x02 
#define FRM_ST_OVR2     0x01 

#define RFLFL           0x06
#define RFLFH           0x07

#define IR_CFG2		0x00
#define IR_CFG2_DIS_CRC	0x02

#define IRM_CR		0x07 
#define IRM_CR_IRX_MSL	0x40
#define IRM_CR_AF_MNT   0x80 

struct nsc_chip {
	char *name;          
	int cfg[3];          
	u_int8_t cid_index;  
	u_int8_t cid_value;  
	u_int8_t cid_mask;   

	
	int (*probe)(struct nsc_chip *chip, chipio_t *info);
	int (*init)(struct nsc_chip *chip, chipio_t *info);
};
typedef struct nsc_chip nsc_chip_t;

struct st_fifo_entry {
	int status;
	int len;
};

#define MAX_TX_WINDOW 7
#define MAX_RX_WINDOW 7

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

struct nsc_ircc_cb {
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

	struct timeval stamp;
	struct timeval now;

	spinlock_t lock;           
	
	__u32 new_speed;
	int index;                 

	struct platform_device *pldev;
};

static inline void switch_bank(int iobase, int bank)
{
		outb(bank, iobase+BSR);
}

#endif 
