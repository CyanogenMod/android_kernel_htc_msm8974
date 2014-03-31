/*
 * Blackfin Secure Digital Host (SDH) definitions
 *
 * Copyright 2008-2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __BFIN_SDH_H__
#define __BFIN_SDH_H__

struct bfin_sd_host {
	int dma_chan;
	int irq_int0;
	int irq_int1;
	u16 pin_req[7];
};

#define CMD_IDX            0x3f        
#define CMD_RSP            (1 << 6)    
#define CMD_L_RSP          (1 << 7)    
#define CMD_INT_E          (1 << 8)    
#define CMD_PEND_E         (1 << 9)    
#define CMD_E              (1 << 10)   

#define PWR_ON             0x3         
#define SD_CMD_OD          (1 << 6)    
#define ROD_CTL            (1 << 7)    

#define CLKDIV             0xff        
#define CLK_E              (1 << 8)    
#define PWR_SV_E           (1 << 9)    
#define CLKDIV_BYPASS      (1 << 10)   
#define WIDE_BUS           (1 << 11)   

#define RESP_CMD           0x3f        

#define DTX_E              (1 << 0)    
#define DTX_DIR            (1 << 1)    
#define DTX_MODE           (1 << 2)    
#define DTX_DMA_E          (1 << 3)    
#define DTX_BLK_LGTH       (0xf << 4)  

#define CMD_CRC_FAIL       (1 << 0)    
#define DAT_CRC_FAIL       (1 << 1)    
#define CMD_TIME_OUT       (1 << 2)    
#define DAT_TIME_OUT       (1 << 3)    
#define TX_UNDERRUN        (1 << 4)    
#define RX_OVERRUN         (1 << 5)    
#define CMD_RESP_END       (1 << 6)    
#define CMD_SENT           (1 << 7)    
#define DAT_END            (1 << 8)    
#define START_BIT_ERR      (1 << 9)    
#define DAT_BLK_END        (1 << 10)   
#define CMD_ACT            (1 << 11)   
#define TX_ACT             (1 << 12)   
#define RX_ACT             (1 << 13)   
#define TX_FIFO_STAT       (1 << 14)   
#define RX_FIFO_STAT       (1 << 15)   
#define TX_FIFO_FULL       (1 << 16)   
#define RX_FIFO_FULL       (1 << 17)   
#define TX_FIFO_ZERO       (1 << 18)   
#define RX_DAT_ZERO        (1 << 19)   
#define TX_DAT_RDY         (1 << 20)   
#define RX_FIFO_RDY        (1 << 21)   

#define CMD_CRC_FAIL_STAT  (1 << 0)    
#define DAT_CRC_FAIL_STAT  (1 << 1)    
#define CMD_TIMEOUT_STAT   (1 << 2)    
#define DAT_TIMEOUT_STAT   (1 << 3)    
#define TX_UNDERRUN_STAT   (1 << 4)    
#define RX_OVERRUN_STAT    (1 << 5)    
#define CMD_RESP_END_STAT  (1 << 6)    
#define CMD_SENT_STAT      (1 << 7)    
#define DAT_END_STAT       (1 << 8)    
#define START_BIT_ERR_STAT (1 << 9)    
#define DAT_BLK_END_STAT   (1 << 10)   

#define CMD_CRC_FAIL_MASK  (1 << 0)    
#define DAT_CRC_FAIL_MASK  (1 << 1)    
#define CMD_TIMEOUT_MASK   (1 << 2)    
#define DAT_TIMEOUT_MASK   (1 << 3)    
#define TX_UNDERRUN_MASK   (1 << 4)    
#define RX_OVERRUN_MASK    (1 << 5)    
#define CMD_RESP_END_MASK  (1 << 6)    
#define CMD_SENT_MASK      (1 << 7)    
#define DAT_END_MASK       (1 << 8)    
#define START_BIT_ERR_MASK (1 << 9)    
#define DAT_BLK_END_MASK   (1 << 10)   
#define CMD_ACT_MASK       (1 << 11)   
#define TX_ACT_MASK        (1 << 12)   
#define RX_ACT_MASK        (1 << 13)   
#define TX_FIFO_STAT_MASK  (1 << 14)   
#define RX_FIFO_STAT_MASK  (1 << 15)   
#define TX_FIFO_FULL_MASK  (1 << 16)   
#define RX_FIFO_FULL_MASK  (1 << 17)   
#define TX_FIFO_ZERO_MASK  (1 << 18)   
#define RX_DAT_ZERO_MASK   (1 << 19)   
#define TX_DAT_RDY_MASK    (1 << 20)   
#define RX_FIFO_RDY_MASK   (1 << 21)   

#define FIFO_COUNT         0x7fff      

#define SDIO_INT_DET       (1 << 1)    
#define SD_CARD_DET        (1 << 4)    

#define SDIO_MSK           (1 << 1)    
#define SCD_MSK            (1 << 6)    

#define CLKS_EN            (1 << 0)    
#define SD4E               (1 << 2)    
#define MWE                (1 << 3)    
#define SD_RST             (1 << 4)    
#define PUP_SDDAT          (1 << 5)    
#define PUP_SDDAT3         (1 << 6)    
#define PD_SDDAT3          (1 << 7)    

#define RWR                (1 << 0)    

#endif
