/*********************************************************************
 *                
 * Filename:      w83977af_ir.h
 * Version:       
 * Description:   
 * Status:        Experimental.
 * Author:        Paul VanderSpek
 * Created at:    Thu Nov 19 13:55:34 1998
 * Modified at:   Tue Jan 11 13:08:19 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-2000 Dag Brattli, All Rights Reserved.
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

#ifndef W83977AF_IR_H
#define W83977AF_IR_H

#include <asm/io.h>
#include <linux/types.h>

#define ENBNKSEL	0x01
#define APEDCRC		0x02
#define TXW4C           0x04
#define RXW4C           0x08

#define RBR             0x00 
#define TBR             0x00 

#define ICR		0x01 
#define ICR_ERBRI       0x01 
#define ICR_ETBREI      0x02 
#define ICR_EUSRI	0x04
#define ICR_EHSRI       0x04
#define ICR_ETXURI      0x04 
#define ICR_EDMAI	0x10 
#define ICR_ETXTHI      0x20 
#define ICR_EFSFI       0x40 
#define ICR_ETMRI       0x80 

#define UFR		0x02 
#define UFR_EN_FIFO     0x01 
#define UFR_RXF_RST     0x02 
#define UFR_TXF_RST     0x04 
#define UFR_RXTL	0x80 
#define UFR_TXTL	0x20 

#define ISR		0x02 
#define ISR_RXTH_I	0x01 
#define ISR_TXEMP_I     0x02 
#define ISR_FEND_I	0x04
#define ISR_DMA_I	0x10
#define ISR_TXTH_I	0x20 
#define ISR_FSF_I       0x40
#define ISR_TMR_I       0x80 

#define UCR             0x03 
#define UCR_DLS8        0x03 

#define SSR 	        0x03 
#define SET0 	        UCR_DLS8        
#define SET1	        (0x80|UCR_DLS8) 
#define SET2	        0xE0
#define SET3	        0xE4
#define SET4	        0xE8
#define SET5	        0xEC
#define SET6	        0xF0
#define SET7	        0xF4

#define HCR		0x04
#define HCR_MODE_MASK	~(0xD0)
#define HCR_SIR         0x60
#define HCR_MIR_576  	0x20	
#define HCR_MIR_1152	0x80
#define HCR_FIR		0xA0
#define HCR_EN_DMA	0x04
#define HCR_EN_IRQ	0x08
#define HCR_TX_WT	0x08

#define USR             0x05 
#define USR_RDR         0x01 
#define USR_TSRE        0x40 

#define AUDR            0x07
#define AUDR_SFEND      0x08 
#define AUDR_RXBSY      0x20 
#define AUDR_UNDR       0x40 

#define ABLL            0x00 
#define ABHL            0x01 

#define ADCR1		0x02
#define ADCR1_ADV_SL	0x01	
#define ADCR1_D_CHSW	0x08	
#define ADCR1_DMA_F	0x02

#define ADCR2		0x04
#define ADCR2_TXFS32	0x01
#define ADCR2_RXFS32	0x04

#define RXFDTH          0x07

#define AUID		0x00

#define TMRL            0x00 
#define TMRH            0x01 

#define IR_MSL          0x02 
#define IR_MSL_EN_TMR   0x01 

#define TFRLL		0x04 
#define TFRLH		0x05 
#define RFRLL		0x06 
#define RFRLH		0x07 


#define FS_FO           0x05 
#define FS_FO_FSFDR     0x80 
#define FS_FO_LST_FR    0x40 
#define FS_FO_MX_LEX    0x10 
#define FS_FO_PHY_ERR   0x08 
#define FS_FO_CRC_ERR   0x04 
#define FS_FO_RX_OV     0x02 
#define FS_FO_FSF_OV    0x01 
#define FS_FO_ERR_MSK   0x5f 

#define RFLFL           0x06
#define RFLFH           0x07

#define IR_CFG2		0x00
#define IR_CFG2_DIS_CRC	0x02

#define IRM_CR		0x07 
#define IRM_CR_IRX_MSL	0x40
#define IRM_CR_AF_MNT   0x80 

struct st_fifo_entry {
	int status;
	int len;
};

struct st_fifo {
	struct st_fifo_entry entries[10];
	int head;
	int tail;
	int len;
};

struct w83977af_ir {
	struct st_fifo st_fifo;

	int tx_buff_offsets[10]; 
	int tx_len;          

	struct net_device *netdev; 
	
	struct irlap_cb    *irlap; 
	struct qos_info     qos;   
	
	chipio_t io;               
	iobuff_t tx_buff;          
	iobuff_t rx_buff;          
	dma_addr_t tx_buff_dma;
	dma_addr_t rx_buff_dma;

	spinlock_t lock;           
	
	__u32 new_speed;
};

static inline void switch_bank( int iobase, int set)
{
	outb(set, iobase+SSR);
}

#endif
