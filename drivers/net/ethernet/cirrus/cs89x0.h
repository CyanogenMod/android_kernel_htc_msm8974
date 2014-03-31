/*  Copyright, 1988-1992, Russell Nelson, Crynwr Software

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 1.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   */


#define PP_ChipID 0x0000	
				
				

#define PP_ISAIOB 0x0020	
#define PP_CS8900_ISAINT 0x0022	
#define PP_CS8920_ISAINT 0x0370	
#define PP_CS8900_ISADMA 0x0024	
#define PP_CS8920_ISADMA 0x0374	
#define PP_ISASOF 0x0026	
#define PP_DmaFrameCnt 0x0028	
#define PP_DmaByteCnt 0x002A	
#define PP_CS8900_ISAMemB 0x002C	
#define PP_CS8920_ISAMemB 0x0348 

#define PP_ISABootBase 0x0030	
#define PP_ISABootMask 0x0034	

#define PP_EECMD 0x0040		
#define PP_EEData 0x0042	
#define PP_DebugReg 0x0044	

#define PP_RxCFG 0x0102		
#define PP_RxCTL 0x0104		
#define PP_TxCFG 0x0106		
#define PP_TxCMD 0x0108		
#define PP_BufCFG 0x010A	
#define PP_LineCTL 0x0112	
#define PP_SelfCTL 0x0114	
#define PP_BusCTL 0x0116	
#define PP_TestCTL 0x0118	
#define PP_AutoNegCTL 0x011C	

#define PP_ISQ 0x0120		
#define PP_RxEvent 0x0124	
#define PP_TxEvent 0x0128	
#define PP_BufEvent 0x012C	
#define PP_RxMiss 0x0130	
#define PP_TxCol 0x0132		
#define PP_LineST 0x0134	
#define PP_SelfST 0x0136	
#define PP_BusST 0x0138		
#define PP_TDR 0x013C		
#define PP_AutoNegST 0x013E	
#define PP_TxCommand 0x0144	
#define PP_TxLength 0x0146	
#define PP_LAF 0x0150		
#define PP_IA 0x0158		

#define PP_RxStatus 0x0400	
#define PP_RxLength 0x0402	
#define PP_RxFrame 0x0404	
#define PP_TxFrame 0x0A00	

#define DEFAULTIOBASE 0x0300
#define FIRST_IO 0x020C		
#define LAST_IO 0x037C		
#define ADD_MASK 0x3000		
#define ADD_SIG 0x3000		

#ifdef CONFIG_MAC
#define LCSLOTBASE 0xfee00000
#define MMIOBASE 0x40000
#endif

#define CHIP_EISA_ID_SIG 0x630E   
#define CHIP_EISA_ID_SIG_STR "0x630E"

#ifdef IBMEIPKT
#define EISA_ID_SIG 0x4D24	
#define PART_NO_SIG 0x1010	
#define MONGOOSE_BIT 0x0000	
#else
#define EISA_ID_SIG 0x630E	
#define PART_NO_SIG 0x4000	
#define MONGOOSE_BIT 0x2000	
#endif

#define PRODUCT_ID_ADD 0x0002   

#define REG_TYPE_MASK 0x001F

#define ERSE_WR_ENBL 0x00F0
#define ERSE_WR_DISABLE 0x0000

#define RX_BUF_CFG 0x0003
#define RX_CONTROL 0x0005
#define TX_CFG 0x0007
#define TX_COMMAND 0x0009
#define BUF_CFG 0x000B
#define LINE_CONTROL 0x0013
#define SELF_CONTROL 0x0015
#define BUS_CONTROL 0x0017
#define TEST_CONTROL 0x0019

#define RX_EVENT 0x0004
#define TX_EVENT 0x0008
#define BUF_EVENT 0x000C
#define RX_MISS_COUNT 0x0010
#define TX_COL_COUNT 0x0012
#define LINE_STATUS 0x0014
#define SELF_STATUS 0x0016
#define BUS_STATUS 0x0018
#define TDR 0x001C

#define SKIP_1 0x0040
#define RX_STREAM_ENBL 0x0080
#define RX_OK_ENBL 0x0100
#define RX_DMA_ONLY 0x0200
#define AUTO_RX_DMA 0x0400
#define BUFFER_CRC 0x0800
#define RX_CRC_ERROR_ENBL 0x1000
#define RX_RUNT_ENBL 0x2000
#define RX_EXTRA_DATA_ENBL 0x4000

#define RX_IA_HASH_ACCEPT 0x0040
#define RX_PROM_ACCEPT 0x0080
#define RX_OK_ACCEPT 0x0100
#define RX_MULTCAST_ACCEPT 0x0200
#define RX_IA_ACCEPT 0x0400
#define RX_BROADCAST_ACCEPT 0x0800
#define RX_BAD_CRC_ACCEPT 0x1000
#define RX_RUNT_ACCEPT 0x2000
#define RX_EXTRA_DATA_ACCEPT 0x4000
#define RX_ALL_ACCEPT (RX_PROM_ACCEPT|RX_BAD_CRC_ACCEPT|RX_RUNT_ACCEPT|RX_EXTRA_DATA_ACCEPT)
#define DEF_RX_ACCEPT (RX_IA_ACCEPT | RX_BROADCAST_ACCEPT | RX_OK_ACCEPT)

#define TX_LOST_CRS_ENBL 0x0040
#define TX_SQE_ERROR_ENBL 0x0080
#define TX_OK_ENBL 0x0100
#define TX_LATE_COL_ENBL 0x0200
#define TX_JBR_ENBL 0x0400
#define TX_ANY_COL_ENBL 0x0800
#define TX_16_COL_ENBL 0x8000

#define TX_START_4_BYTES 0x0000
#define TX_START_64_BYTES 0x0040
#define TX_START_128_BYTES 0x0080
#define TX_START_ALL_BYTES 0x00C0
#define TX_FORCE 0x0100
#define TX_ONE_COL 0x0200
#define TX_TWO_PART_DEFF_DISABLE 0x0400
#define TX_NO_CRC 0x1000
#define TX_RUNT 0x2000

#define GENERATE_SW_INTERRUPT 0x0040
#define RX_DMA_ENBL 0x0080
#define READY_FOR_TX_ENBL 0x0100
#define TX_UNDERRUN_ENBL 0x0200
#define RX_MISS_ENBL 0x0400
#define RX_128_BYTE_ENBL 0x0800
#define TX_COL_COUNT_OVRFLOW_ENBL 0x1000
#define RX_MISS_COUNT_OVRFLOW_ENBL 0x2000
#define RX_DEST_MATCH_ENBL 0x8000

#define SERIAL_RX_ON 0x0040
#define SERIAL_TX_ON 0x0080
#define AUI_ONLY 0x0100
#define AUTO_AUI_10BASET 0x0200
#define MODIFIED_BACKOFF 0x0800
#define NO_AUTO_POLARITY 0x1000
#define TWO_PART_DEFDIS 0x2000
#define LOW_RX_SQUELCH 0x4000

#define POWER_ON_RESET 0x0040
#define SW_STOP 0x0100
#define SLEEP_ON 0x0200
#define AUTO_WAKEUP 0x0400
#define HCB0_ENBL 0x1000
#define HCB1_ENBL 0x2000
#define HCB0 0x4000
#define HCB1 0x8000

#define RESET_RX_DMA 0x0040
#define MEMORY_ON 0x0400
#define DMA_BURST_MODE 0x0800
#define IO_CHANNEL_READY_ON 0x1000
#define RX_DMA_SIZE_64K 0x2000
#define ENABLE_IRQ 0x8000

#define LINK_OFF 0x0080
#define ENDEC_LOOPBACK 0x0200
#define AUI_LOOPBACK 0x0400
#define BACKOFF_OFF 0x0800
#define FDX_8900 0x4000
#define FAST_TEST 0x8000

#define RX_IA_HASHED 0x0040
#define RX_DRIBBLE 0x0080
#define RX_OK 0x0100
#define RX_HASHED 0x0200
#define RX_IA 0x0400
#define RX_BROADCAST 0x0800
#define RX_CRC_ERROR 0x1000
#define RX_RUNT 0x2000
#define RX_EXTRA_DATA 0x4000

#define HASH_INDEX_MASK 0x0FC00

#define TX_LOST_CRS 0x0040
#define TX_SQE_ERROR 0x0080
#define TX_OK 0x0100
#define TX_LATE_COL 0x0200
#define TX_JBR 0x0400
#define TX_16_COL 0x8000
#define TX_SEND_OK_BITS (TX_OK|TX_LOST_CRS)
#define TX_COL_COUNT_MASK 0x7800

#define SW_INTERRUPT 0x0040
#define RX_DMA 0x0080
#define READY_FOR_TX 0x0100
#define TX_UNDERRUN 0x0200
#define RX_MISS 0x0400
#define RX_128_BYTE 0x0800
#define TX_COL_OVRFLW 0x1000
#define RX_MISS_OVRFLW 0x2000
#define RX_DEST_MATCH 0x8000

#define LINK_OK 0x0080
#define AUI_ON 0x0100
#define TENBASET_ON 0x0200
#define POLARITY_OK 0x1000
#define CRS_OK 0x4000

#define ACTIVE_33V 0x0040
#define INIT_DONE 0x0080
#define SI_BUSY 0x0100
#define EEPROM_PRESENT 0x0200
#define EEPROM_OK 0x0400
#define EL_PRESENT 0x0800
#define EE_SIZE_64 0x1000

#define TX_BID_ERROR 0x0080
#define READY_FOR_TX_NOW 0x0100

#define RE_NEG_NOW 0x0040
#define ALLOW_FDX 0x0080
#define AUTO_NEG_ENABLE 0x0100
#define NLP_ENABLE 0x0200
#define FORCE_FDX 0x8000
#define AUTO_NEG_BITS (FORCE_FDX|NLP_ENABLE|AUTO_NEG_ENABLE)
#define AUTO_NEG_MASK (FORCE_FDX|NLP_ENABLE|AUTO_NEG_ENABLE|ALLOW_FDX|RE_NEG_NOW)

#define AUTO_NEG_BUSY 0x0080
#define FLP_LINK 0x0100
#define FLP_LINK_GOOD 0x0800
#define LINK_FAULT 0x1000
#define HDX_ACTIVE 0x4000
#define FDX_ACTIVE 0x8000

#define ISQ_RECEIVER_EVENT 0x04
#define ISQ_TRANSMITTER_EVENT 0x08
#define ISQ_BUFFER_EVENT 0x0c
#define ISQ_RX_MISS_EVENT 0x10
#define ISQ_TX_COL_EVENT 0x12

#define ISQ_EVENT_MASK 0x003F   
#define ISQ_HIST 16		
#define AUTOINCREMENT 0x8000	

#define TXRXBUFSIZE 0x0600
#define RXDMABUFSIZE 0x8000
#define RXDMASIZE 0x4000
#define TXRX_LENGTH_MASK 0x07FF

#define RCV_WITH_RXON	1       
#define RCV_COUNTS	2       
#define RCV_PONG	4       
#define RCV_DONG	8       
#define RCV_POLLING	0x10	
#define RCV_ISQ		0x20	
#define RCV_AUTO_DMA	0x100	
#define RCV_DMA		0x200	
#define RCV_DMA_ALL	0x400	
#define RCV_FIXED_DATA	0x800	
#define RCV_IO		0x1000	
#define RCV_MEMORY	0x2000	

#define RAM_SIZE	0x1000       
#define PKT_START PP_TxFrame  

#define RX_FRAME_PORT	0x0000
#define TX_FRAME_PORT RX_FRAME_PORT
#define TX_CMD_PORT	0x0004
#define TX_NOW		0x0000       
#define TX_AFTER_381	0x0040       
#define TX_AFTER_ALL	0x00c0       
#define TX_LEN_PORT	0x0006
#define ISQ_PORT	0x0008
#define ADD_PORT	0x000A
#define DATA_PORT	0x000C

#define EEPROM_WRITE_EN		0x00F0
#define EEPROM_WRITE_DIS	0x0000
#define EEPROM_WRITE_CMD	0x0100
#define EEPROM_READ_CMD		0x0200

#define RBUF_EVENT_LOW	0   
#define RBUF_EVENT_HIGH	1   
#define RBUF_LEN_LOW	2   
#define RBUF_LEN_HI	3   
#define RBUF_HEAD_LEN	4   

#define CHIP_READ 0x1   
#define DMA_READ 0x2   

#ifdef CSDEBUG
#define BIOS_START_SEG 0x00000
#define BIOS_OFFSET_INC 0x0010
#else
#define BIOS_START_SEG 0x0c000
#define BIOS_OFFSET_INC 0x0200
#endif

#define BIOS_LAST_OFFSET 0x0fc00

#define ISA_CNF_OFFSET 0x6
#define TX_CTL_OFFSET (ISA_CNF_OFFSET + 8)			
#define AUTO_NEG_CNF_OFFSET (ISA_CNF_OFFSET + 8)		

  
  
  
  
#define EE_FORCE_FDX  0x8000
#define EE_NLP_ENABLE 0x0200
#define EE_AUTO_NEG_ENABLE 0x0100
#define EE_ALLOW_FDX 0x0080
#define EE_AUTO_NEG_CNF_MASK (EE_FORCE_FDX|EE_NLP_ENABLE|EE_AUTO_NEG_ENABLE|EE_ALLOW_FDX)

#define IMM_BIT 0x0040		

#define ADAPTER_CNF_OFFSET (AUTO_NEG_CNF_OFFSET + 2)
#define A_CNF_10B_T 0x0001
#define A_CNF_AUI 0x0002
#define A_CNF_10B_2 0x0004
#define A_CNF_MEDIA_TYPE 0x0070
#define A_CNF_MEDIA_AUTO 0x0070
#define A_CNF_MEDIA_10B_T 0x0020
#define A_CNF_MEDIA_AUI 0x0040
#define A_CNF_MEDIA_10B_2 0x0010
#define A_CNF_DC_DC_POLARITY 0x0080
#define A_CNF_NO_AUTO_POLARITY 0x2000
#define A_CNF_LOW_RX_SQUELCH 0x4000
#define A_CNF_EXTND_10B_2 0x8000

#define PACKET_PAGE_OFFSET 0x8

#define INT_NO_MASK 0x000F
#define DMA_NO_MASK 0x0070
#define ISA_DMA_SIZE 0x0200
#define ISA_AUTO_RxDMA 0x0400
#define ISA_RxDMA 0x0800
#define DMA_BURST 0x1000
#define STREAM_TRANSFER 0x2000
#define ANY_ISA_DMA (ISA_AUTO_RxDMA | ISA_RxDMA)

#define DMA_BASE 0x00     
#define DMA_BASE_2 0x0C0    

#define DMA_STAT 0x0D0    
#define DMA_MASK 0x0D4    
#define DMA_MODE 0x0D6    
#define DMA_RESETFF 0x0D8    

#define DMA_DISABLE 0x04     
#define DMA_ENABLE 0x00     
#define DMA_RX_MODE 0x14
#define DMA_TX_MODE 0x18

#define DMA_SIZE (16*1024) 

#define CS8900 0x0000
#define CS8920 0x4000
#define CS8920M 0x6000
#define REVISON_BITS 0x1F00
#define EEVER_NUMBER 0x12
#define CHKSUM_LEN 0x14
#define CHKSUM_VAL 0x0000
#define START_EEPROM_DATA 0x001c 
#define IRQ_MAP_EEPROM_DATA 0x0046 
#define IRQ_MAP_LEN 0x0004 
#define PNP_IRQ_FRMT 0x0022 
#define CS8900_IRQ_MAP 0x1c20 

#define CS8920_NO_INTS 0x0F   

#define PNP_ADD_PORT 0x0279
#define PNP_WRITE_PORT 0x0A79

#define GET_PNP_ISA_STRUCT 0x40
#define PNP_ISA_STRUCT_LEN 0x06
#define PNP_CSN_CNT_OFF 0x01
#define PNP_RD_PORT_OFF 0x02
#define PNP_FUNCTION_OK 0x00
#define PNP_WAKE 0x03
#define PNP_RSRC_DATA 0x04
#define PNP_RSRC_READY 0x01
#define PNP_STATUS 0x05
#define PNP_ACTIVATE 0x30
#define PNP_CNF_IO_H 0x60
#define PNP_CNF_IO_L 0x61
#define PNP_CNF_INT 0x70
#define PNP_CNF_DMA 0x74
#define PNP_CNF_MEM 0x48

#define BIT0 1
#define BIT15 0x8000

