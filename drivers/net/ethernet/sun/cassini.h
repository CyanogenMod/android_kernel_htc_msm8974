/* $Id: cassini.h,v 1.16 2004/08/17 21:15:16 zaumen Exp $
 * cassini.h: Definitions for Sun Microsystems Cassini(+) ethernet driver.
 *
 * Copyright (C) 2004 Sun Microsystems Inc.
 * Copyright (c) 2003 Adrian Sun (asun@darksunrising.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * vendor id: 0x108E (Sun Microsystems, Inc.)
 * device id: 0xabba (Cassini)
 * revision ids: 0x01 = Cassini
 *               0x02 = Cassini rev 2
 *               0x10 = Cassini+
 *               0x11 = Cassini+ 0.2u
 *
 * vendor id: 0x100b (National Semiconductor)
 * device id: 0x0035 (DP83065/Saturn)
 * revision ids: 0x30 = Saturn B2
 *
 * rings are all offset from 0.
 *
 * there are two clock domains:
 * PCI:  33/66MHz clock
 * chip: 125MHz clock
 */

#ifndef _CASSINI_H
#define _CASSINI_H

#define CAS_ID_REV2          0x02
#define CAS_ID_REVPLUS       0x10
#define CAS_ID_REVPLUS02u    0x11
#define CAS_ID_REVSATURNB2   0x30


#define  REG_CAWR	               0x0004  
#define    CAWR_RX_DMA_WEIGHT_SHIFT    0
#define    CAWR_RX_DMA_WEIGHT_MASK     0x03    
#define    CAWR_TX_DMA_WEIGHT_SHIFT    2
#define    CAWR_TX_DMA_WEIGHT_MASK     0x0C    
#define    CAWR_RR_DIS                 0x10    

#define  REG_INF_BURST                 0x0008  
#define    INF_BURST_EN                0x1     

#define  REG_INTR_STATUS               0x000C  
#define    INTR_TX_INTME               0x00000001  
#define    INTR_TX_ALL                 0x00000002  
#define    INTR_TX_DONE                0x00000004  
#define    INTR_TX_TAG_ERROR           0x00000008  
#define    INTR_RX_DONE                0x00000010  
#define    INTR_RX_BUF_UNAVAIL         0x00000020  
#define    INTR_RX_TAG_ERROR           0x00000040  
#define    INTR_RX_COMP_FULL           0x00000080  
#define    INTR_RX_BUF_AE              0x00000100  
#define    INTR_RX_COMP_AF             0x00000200  
#define    INTR_RX_LEN_MISMATCH        0x00000400  
#define    INTR_SUMMARY                0x00001000  
#define    INTR_PCS_STATUS             0x00002000  
#define    INTR_TX_MAC_STATUS          0x00004000  
#define    INTR_RX_MAC_STATUS          0x00008000  
#define    INTR_MAC_CTRL_STATUS        0x00010000  
#define    INTR_MIF_STATUS             0x00020000  
#define    INTR_PCI_ERROR_STATUS       0x00040000  
#define    INTR_TX_COMP_3_MASK         0xFFF80000  
#define    INTR_TX_COMP_3_SHIFT        19
#define    INTR_ERROR_MASK (INTR_MIF_STATUS | INTR_PCI_ERROR_STATUS | \
                            INTR_PCS_STATUS | INTR_RX_LEN_MISMATCH | \
                            INTR_TX_MAC_STATUS | INTR_RX_MAC_STATUS | \
                            INTR_TX_TAG_ERROR | INTR_RX_TAG_ERROR | \
                            INTR_MAC_CTRL_STATUS)

#define  REG_INTR_MASK                 0x0010  

#define  REG_ALIAS_CLEAR               0x0014  
#define  REG_INTR_STATUS_ALIAS         0x001C  

#define  REG_PCI_ERR_STATUS            0x1000  
#define    PCI_ERR_BADACK              0x01    
#define    PCI_ERR_DTRTO               0x02    
#define    PCI_ERR_OTHER               0x04    
#define    PCI_ERR_BIM_DMA_WRITE       0x08    
#define    PCI_ERR_BIM_DMA_READ        0x10    
#define    PCI_ERR_BIM_DMA_TIMEOUT     0x20    

#define  REG_PCI_ERR_STATUS_MASK       0x1004  

#define  REG_BIM_CFG                0x1008  
#define    BIM_CFG_RESERVED0        0x001   
#define    BIM_CFG_RESERVED1        0x002   
#define    BIM_CFG_64BIT_DISABLE    0x004   
#define    BIM_CFG_66MHZ            0x008   
#define    BIM_CFG_32BIT            0x010   
#define    BIM_CFG_DPAR_INTR_ENABLE 0x020   
#define    BIM_CFG_RMA_INTR_ENABLE  0x040   
#define    BIM_CFG_RTA_INTR_ENABLE  0x080   
#define    BIM_CFG_RESERVED2        0x100   
#define    BIM_CFG_BIM_DISABLE      0x200   
#define    BIM_CFG_BIM_STATUS       0x400   
#define    BIM_CFG_PERROR_BLOCK     0x800  

#define  REG_BIM_DIAG                  0x100C  
#define    BIM_DIAG_MSTR_SM_MASK       0x3FFFFF00 
#define    BIM_DIAG_BRST_SM_MASK       0x7F    

#define  REG_SW_RESET                  0x1010  
#define    SW_RESET_TX                 0x00000001  
#define    SW_RESET_RX                 0x00000002  
#define    SW_RESET_RSTOUT             0x00000004  
#define    SW_RESET_BLOCK_PCS_SLINK    0x00000008  
#define    SW_RESET_BREQ_SM_MASK       0x00007F00  
#define    SW_RESET_PCIARB_SM_MASK     0x00070000  
#define    SW_RESET_RDPCI_SM_MASK      0x00300000  
#define    SW_RESET_RDARB_SM_MASK      0x00C00000  
#define    SW_RESET_WRPCI_SM_MASK      0x06000000  
#define    SW_RESET_WRARB_SM_MASK      0x38000000  

/* Cassini only. 64-bit register used to check PCI datapath. when read,
 * value written has both lower and upper 32-bit halves rotated to the right
 * one bit position. e.g., FFFFFFFF FFFFFFFF -> 7FFFFFFF 7FFFFFFF
 */
#define  REG_MINUS_BIM_DATAPATH_TEST   0x1018  

#define  REG_BIM_LOCAL_DEV_EN          0x1020  
#define    BIM_LOCAL_DEV_PAD           0x01    
#define    BIM_LOCAL_DEV_PROM          0x02    
#define    BIM_LOCAL_DEV_EXT           0x04    
#define    BIM_LOCAL_DEV_SOFT_0        0x08    
#define    BIM_LOCAL_DEV_SOFT_1        0x10    
#define    BIM_LOCAL_DEV_HW_RESET      0x20    

#define  REG_BIM_BUFFER_ADDR           0x1024  
#define    BIM_BUFFER_ADDR_MASK        0x3F    
#define    BIM_BUFFER_WR_SELECT        0x40    
#define  REG_BIM_BUFFER_DATA_LOW       0x1028  
#define  REG_BIM_BUFFER_DATA_HI        0x102C  

#define  REG_BIM_RAM_BIST              0x102C  
#define    BIM_RAM_BIST_RD_START       0x01    
#define    BIM_RAM_BIST_WR_START       0x02    
#define    BIM_RAM_BIST_RD_PASS        0x04    
#define    BIM_RAM_BIST_WR_PASS        0x08    
#define    BIM_RAM_BIST_RD_LOW_PASS    0x10    
#define    BIM_RAM_BIST_RD_HI_PASS     0x20    
#define    BIM_RAM_BIST_WR_LOW_PASS    0x40    
#define    BIM_RAM_BIST_WR_HI_PASS     0x80    

#define  REG_BIM_DIAG_MUX              0x1030  

#define  REG_PLUS_PROBE_MUX_SELECT     0x1034 
#define    PROBE_MUX_EN                0x80000000 
#define    PROBE_MUX_SUB_MUX_MASK      0x0000FF00 
#define    PROBE_MUX_SEL_HI_MASK       0x000000F0 
#define    PROBE_MUX_SEL_LOW_MASK      0x0000000F 

#define  REG_PLUS_INTR_MASK_1          0x1038 
#define  REG_PLUS_INTRN_MASK(x)       (REG_PLUS_INTR_MASK_1 + ((x) - 1)*16)
#define    INTR_RX_DONE_ALT              0x01
#define    INTR_RX_COMP_FULL_ALT         0x02
#define    INTR_RX_COMP_AF_ALT           0x04
#define    INTR_RX_BUF_UNAVAIL_1         0x08
#define    INTR_RX_BUF_AE_1              0x10 
#define    INTRN_MASK_RX_EN              0x80
#define    INTRN_MASK_CLEAR_ALL          (INTR_RX_DONE_ALT | \
                                          INTR_RX_COMP_FULL_ALT | \
                                          INTR_RX_COMP_AF_ALT | \
                                          INTR_RX_BUF_UNAVAIL_1 | \
                                          INTR_RX_BUF_AE_1)
#define  REG_PLUS_INTR_STATUS_1        0x103C 
#define  REG_PLUS_INTRN_STATUS(x)       (REG_PLUS_INTR_STATUS_1 + ((x) - 1)*16)
#define    INTR_STATUS_ALT_INTX_EN     0x80   

#define  REG_PLUS_ALIAS_CLEAR_1        0x1040 
#define  REG_PLUS_ALIASN_CLEAR(x)      (REG_PLUS_ALIAS_CLEAR_1 + ((x) - 1)*16)

#define  REG_PLUS_INTR_STATUS_ALIAS_1  0x1044 
#define  REG_PLUS_INTRN_STATUS_ALIAS(x) (REG_PLUS_INTR_STATUS_ALIAS_1 + ((x) - 1)*16)

#define REG_SATURN_PCFG               0x106c 

#define   SATURN_PCFG_TLA             0x00000001 
#define   SATURN_PCFG_FLA             0x00000002 
#define   SATURN_PCFG_CLA             0x00000004 
#define   SATURN_PCFG_LLA             0x00000008 
#define   SATURN_PCFG_RLA             0x00000010 
#define   SATURN_PCFG_PDS             0x00000020 
#define   SATURN_PCFG_MTP             0x00000080 
#define   SATURN_PCFG_GMO             0x00000100 
#define   SATURN_PCFG_FSI             0x00000200 
#define   SATURN_PCFG_LAD             0x00000800 


#define MAX_TX_RINGS_SHIFT            2
#define MAX_TX_RINGS                  (1 << MAX_TX_RINGS_SHIFT)
#define MAX_TX_RINGS_MASK             (MAX_TX_RINGS - 1)

#define  REG_TX_CFG                    0x2004  
#define    TX_CFG_DMA_EN               0x00000001  
#define    TX_CFG_FIFO_PIO_SEL         0x00000002  
#define    TX_CFG_DESC_RING0_MASK      0x0000003C  
#define    TX_CFG_DESC_RING0_SHIFT     2
#define    TX_CFG_DESC_RINGN_MASK(a)   (TX_CFG_DESC_RING0_MASK << (a)*4)
#define    TX_CFG_DESC_RINGN_SHIFT(a)  (TX_CFG_DESC_RING0_SHIFT + (a)*4)
#define    TX_CFG_PACED_MODE           0x00100000  
#define    TX_CFG_DMA_RDPIPE_DIS       0x01000000  
#define    TX_CFG_COMPWB_Q1            0x02000000  
#define    TX_CFG_COMPWB_Q2            0x04000000  
#define    TX_CFG_COMPWB_Q3            0x08000000  
#define    TX_CFG_COMPWB_Q4            0x10000000  
#define    TX_CFG_INTR_COMPWB_DIS      0x20000000  
#define    TX_CFG_CTX_SEL_MASK         0xC0000000  
#define    TX_CFG_CTX_SEL_SHIFT        30

#define  REG_TX_FIFO_WRITE_PTR         0x2014  
#define  REG_TX_FIFO_SHADOW_WRITE_PTR  0x2018  
#define  REG_TX_FIFO_READ_PTR          0x201C  
#define  REG_TX_FIFO_SHADOW_READ_PTR   0x2020  

#define  REG_TX_FIFO_PKT_CNT           0x2024  

#define  REG_TX_SM_1                   0x2028  
#define    TX_SM_1_CHAIN_MASK          0x000003FF   
#define    TX_SM_1_CSUM_MASK           0x00000C00   
#define    TX_SM_1_FIFO_LOAD_MASK      0x0003F000   
#define    TX_SM_1_FIFO_UNLOAD_MASK    0x003C0000   
#define    TX_SM_1_CACHE_MASK          0x03C00000   
#define    TX_SM_1_CBQ_ARB_MASK        0xF8000000   

#define  REG_TX_SM_2                   0x202C  
#define    TX_SM_2_COMP_WB_MASK        0x07    
#define	   TX_SM_2_SUB_LOAD_MASK       0x38    
#define	   TX_SM_2_KICK_MASK           0xC0    

#define  REG_TX_DATA_PTR_LOW           0x2030  
#define  REG_TX_DATA_PTR_HI            0x2034  

/* 13 bit registers written by driver w/ descriptor value that follows
 * last valid xmit descriptor. kick # and complete # values are used by
 * the xmit dma engine to control tx descr fetching. if > 1 valid
 * tx descr is available within the cache line being read, cassini will
 * internally cache up to 4 of them. 0 on reset. _KICK = rw, _COMP = ro.
 */
#define  REG_TX_KICK0                  0x2038  
#define  REG_TX_KICKN(x)               (REG_TX_KICK0 + (x)*4)
#define  REG_TX_COMP0                  0x2048  
#define  REG_TX_COMPN(x)               (REG_TX_COMP0 + (x)*4)

/* values of TX_COMPLETE_1-4 are written. each completion register
 * is 2bytes in size and contiguous. 8B allocation w/ 8B alignment.
 * NOTE: completion reg values are only written back prior to TX_INTME and
 * TX_ALL interrupts. at all other times, the most up-to-date index values
 * should be obtained from the REG_TX_COMPLETE_# registers.
 * here's the layout:
 * offset from base addr      completion # byte
 *           0                TX_COMPLETE_1_MSB
 *	     1                TX_COMPLETE_1_LSB
 *           2                TX_COMPLETE_2_MSB
 *	     3                TX_COMPLETE_2_LSB
 *           4                TX_COMPLETE_3_MSB
 *	     5                TX_COMPLETE_3_LSB
 *           6                TX_COMPLETE_4_MSB
 *	     7                TX_COMPLETE_4_LSB
 */
#define  TX_COMPWB_SIZE             8
#define  REG_TX_COMPWB_DB_LOW       0x2058  
#define  REG_TX_COMPWB_DB_HI        0x205C  
#define    TX_COMPWB_MSB_MASK       0x00000000000000FFULL
#define    TX_COMPWB_MSB_SHIFT      0
#define    TX_COMPWB_LSB_MASK       0x000000000000FF00ULL
#define    TX_COMPWB_LSB_SHIFT      8
#define    TX_COMPWB_NEXT(x)        ((x) >> 16)

#define  REG_TX_DB0_LOW         0x2060  
#define  REG_TX_DB0_HI          0x2064  
#define  REG_TX_DBN_LOW(x)      (REG_TX_DB0_LOW + (x)*8)
#define  REG_TX_DBN_HI(x)       (REG_TX_DB0_HI + (x)*8)

#define  REG_TX_MAXBURST_0             0x2080  
#define  REG_TX_MAXBURST_1             0x2084  
#define  REG_TX_MAXBURST_2             0x2088  
#define  REG_TX_MAXBURST_3             0x208C  

#define  REG_TX_FIFO_ADDR              0x2104  
#define  REG_TX_FIFO_TAG               0x2108  
#define  REG_TX_FIFO_DATA_LOW          0x210C  
#define  REG_TX_FIFO_DATA_HI_T1        0x2110  
#define  REG_TX_FIFO_DATA_HI_T0        0x2114  
#define  REG_TX_FIFO_SIZE              0x2118  

#define  REG_TX_RAMBIST                0x211C 
#define    TX_RAMBIST_STATE            0x01C0 
#define    TX_RAMBIST_RAM33A_PASS      0x0020 
#define    TX_RAMBIST_RAM32A_PASS      0x0010 
#define    TX_RAMBIST_RAM33B_PASS      0x0008 
#define    TX_RAMBIST_RAM32B_PASS      0x0004 
#define    TX_RAMBIST_SUMMARY          0x0002 
#define    TX_RAMBIST_START            0x0001 

#define MAX_RX_DESC_RINGS              2
#define MAX_RX_COMP_RINGS              4

#define  REG_RX_CFG                     0x4000  
#define    RX_CFG_DMA_EN                0x00000001 
#define    RX_CFG_DESC_RING_MASK        0x0000001E 
#define    RX_CFG_DESC_RING_SHIFT       1
#define    RX_CFG_COMP_RING_MASK        0x000001E0 
#define    RX_CFG_COMP_RING_SHIFT       5
#define    RX_CFG_BATCH_DIS             0x00000200 
#define    RX_CFG_SWIVEL_MASK           0x00001C00 
#define    RX_CFG_SWIVEL_SHIFT          10

#define    RX_CFG_DESC_RING1_MASK       0x000F0000 
#define    RX_CFG_DESC_RING1_SHIFT      16


#define  REG_RX_PAGE_SIZE               0x4004  
#define    RX_PAGE_SIZE_MASK            0x00000003 
#define    RX_PAGE_SIZE_SHIFT           0
#define    RX_PAGE_SIZE_MTU_COUNT_MASK  0x00007800 
#define    RX_PAGE_SIZE_MTU_COUNT_SHIFT 11
#define    RX_PAGE_SIZE_MTU_STRIDE_MASK 0x18000000 
#define    RX_PAGE_SIZE_MTU_STRIDE_SHIFT 27
#define    RX_PAGE_SIZE_MTU_OFF_MASK    0xC0000000 
#define    RX_PAGE_SIZE_MTU_OFF_SHIFT   30

#define  REG_RX_FIFO_WRITE_PTR             0x4008  
#define  REG_RX_FIFO_READ_PTR              0x400C  
#define  REG_RX_IPP_FIFO_SHADOW_WRITE_PTR  0x4010  
#define  REG_RX_IPP_FIFO_SHADOW_READ_PTR   0x4014  
#define  REG_RX_IPP_FIFO_READ_PTR          0x400C  

#define  REG_RX_DEBUG                      0x401C  
#define    RX_DEBUG_LOAD_STATE_MASK        0x0000000F 
#define    RX_DEBUG_LM_STATE_MASK          0x00000070 
#define    RX_DEBUG_FC_STATE_MASK          0x000000180 
#define    RX_DEBUG_DATA_STATE_MASK        0x000001E00 
#define    RX_DEBUG_DESC_STATE_MASK        0x0001E000 
#define    RX_DEBUG_INTR_READ_PTR_MASK     0x30000000 
#define    RX_DEBUG_INTR_WRITE_PTR_MASK    0xC0000000 

#define  REG_RX_PAUSE_THRESH               0x4020  
#define    RX_PAUSE_THRESH_QUANTUM         64
#define    RX_PAUSE_THRESH_OFF_MASK        0x000001FF 
#define    RX_PAUSE_THRESH_OFF_SHIFT       0
#define    RX_PAUSE_THRESH_ON_MASK         0x001FF000 
#define    RX_PAUSE_THRESH_ON_SHIFT        12

#define  REG_RX_KICK                    0x4024  

#define  REG_RX_DB_LOW                     0x4028  
#define  REG_RX_DB_HI                      0x402C  
#define  REG_RX_CB_LOW                     0x4030  
#define  REG_RX_CB_HI                      0x4034  
#define  REG_RX_COMP                       0x4038  

#define  REG_RX_COMP_HEAD                  0x403C  
#define  REG_RX_COMP_TAIL                  0x4040  

#define  REG_RX_BLANK                      0x4044  
#define    RX_BLANK_INTR_PKT_MASK          0x000001FF 
#define    RX_BLANK_INTR_PKT_SHIFT         0
#define    RX_BLANK_INTR_TIME_MASK         0x3FFFF000 
#define    RX_BLANK_INTR_TIME_SHIFT        12

#define  REG_RX_AE_THRESH                  0x4048  
#define    RX_AE_THRESH_FREE_MASK          0x00001FFF 
#define    RX_AE_THRESH_FREE_SHIFT         0
#define    RX_AE_THRESH_COMP_MASK          0x0FFFE000 
#define    RX_AE_THRESH_COMP_SHIFT         13

#define  REG_RX_RED                      0x404C  
#define    RX_RED_4K_6K_FIFO_MASK        0x000000FF 
#define    RX_RED_6K_8K_FIFO_MASK        0x0000FF00 
#define    RX_RED_8K_10K_FIFO_MASK       0x00FF0000 
#define    RX_RED_10K_12K_FIFO_MASK      0xFF000000 

#define  REG_RX_FIFO_FULLNESS              0x4050  
#define    RX_FIFO_FULLNESS_RX_FIFO_MASK   0x3FF80000 
#define    RX_FIFO_FULLNESS_IPP_FIFO_MASK  0x0007FF00 
#define    RX_FIFO_FULLNESS_RX_PKT_MASK    0x000000FF 
#define  REG_RX_IPP_PACKET_COUNT           0x4054  
#define  REG_RX_WORK_DMA_PTR_LOW           0x4058  
#define  REG_RX_WORK_DMA_PTR_HI            0x405C  

#define  REG_RX_BIST                       0x4060  
#define    RX_BIST_32A_PASS                0x80000000 
#define    RX_BIST_33A_PASS                0x40000000 
#define    RX_BIST_32B_PASS                0x20000000 
#define    RX_BIST_33B_PASS                0x10000000 
#define    RX_BIST_32C_PASS                0x08000000 
#define    RX_BIST_33C_PASS                0x04000000 
#define    RX_BIST_IPP_32A_PASS            0x02000000 
#define    RX_BIST_IPP_33A_PASS            0x01000000 
#define    RX_BIST_IPP_32B_PASS            0x00800000 
#define    RX_BIST_IPP_33B_PASS            0x00400000 
#define    RX_BIST_IPP_32C_PASS            0x00200000 
#define    RX_BIST_IPP_33C_PASS            0x00100000 
#define    RX_BIST_CTRL_32_PASS            0x00800000 
#define    RX_BIST_CTRL_33_PASS            0x00400000 
#define    RX_BIST_REAS_26A_PASS           0x00200000 
#define    RX_BIST_REAS_26B_PASS           0x00100000 
#define    RX_BIST_REAS_27_PASS            0x00080000 
#define    RX_BIST_STATE_MASK              0x00078000 
#define    RX_BIST_SUMMARY                 0x00000002 
#define    RX_BIST_START                   0x00000001 

#define  REG_RX_CTRL_FIFO_WRITE_PTR        0x4064  
#define  REG_RX_CTRL_FIFO_READ_PTR         0x4068  

#define  REG_RX_BLANK_ALIAS_READ           0x406C  
#define    RX_BAR_INTR_PACKET_MASK         0x000001FF 
#define    RX_BAR_INTR_TIME_MASK           0x3FFFF000 

#define  REG_RX_FIFO_ADDR                  0x4080  
#define  REG_RX_FIFO_TAG                   0x4084  
#define  REG_RX_FIFO_DATA_LOW              0x4088  
#define  REG_RX_FIFO_DATA_HI_T0            0x408C  
#define  REG_RX_FIFO_DATA_HI_T1            0x4090  

#define  REG_RX_CTRL_FIFO_ADDR             0x4094  
#define  REG_RX_CTRL_FIFO_DATA_LOW         0x4098  
#define  REG_RX_CTRL_FIFO_DATA_MID         0x409C  
#define  REG_RX_CTRL_FIFO_DATA_HI          0x4100  
#define    RX_CTRL_FIFO_DATA_HI_CTRL       0x0001  
#define    RX_CTRL_FIFO_DATA_HI_FLOW_MASK  0x007E  

#define  REG_RX_IPP_FIFO_ADDR              0x4104  
#define  REG_RX_IPP_FIFO_TAG               0x4108  
#define  REG_RX_IPP_FIFO_DATA_LOW          0x410C  
#define  REG_RX_IPP_FIFO_DATA_HI_T0        0x4110  
#define  REG_RX_IPP_FIFO_DATA_HI_T1        0x4114  

#define  REG_RX_HEADER_PAGE_PTR_LOW        0x4118  
#define  REG_RX_HEADER_PAGE_PTR_HI         0x411C  
#define  REG_RX_MTU_PAGE_PTR_LOW           0x4120  
#define  REG_RX_MTU_PAGE_PTR_HI            0x4124  

#define  REG_RX_TABLE_ADDR             0x4128  
#define    RX_TABLE_ADDR_MASK          0x0000003F 

#define  REG_RX_TABLE_DATA_LOW         0x412C  
#define  REG_RX_TABLE_DATA_MID         0x4130  
#define  REG_RX_TABLE_DATA_HI          0x4134  

#define  REG_PLUS_RX_DB1_LOW            0x4200  
#define  REG_PLUS_RX_DB1_HI             0x4204  
#define  REG_PLUS_RX_CB1_LOW            0x4208  
#define  REG_PLUS_RX_CB1_HI             0x420C  
#define  REG_PLUS_RX_CBN_LOW(x)        (REG_PLUS_RX_CB1_LOW + 8*((x) - 1))
#define  REG_PLUS_RX_CBN_HI(x)         (REG_PLUS_RX_CB1_HI + 8*((x) - 1))
#define  REG_PLUS_RX_KICK1             0x4220  
#define  REG_PLUS_RX_COMP1             0x4224  
#define  REG_PLUS_RX_COMP1_HEAD        0x4228  
#define  REG_PLUS_RX_COMP1_TAIL        0x422C  
#define  REG_PLUS_RX_COMPN_HEAD(x)    (REG_PLUS_RX_COMP1_HEAD + 8*((x) - 1))
#define  REG_PLUS_RX_COMPN_TAIL(x)    (REG_PLUS_RX_COMP1_TAIL + 8*((x) - 1))
#define  REG_PLUS_RX_AE1_THRESH        0x4240  
#define    RX_AE1_THRESH_FREE_MASK     RX_AE_THRESH_FREE_MASK
#define    RX_AE1_THRESH_FREE_SHIFT    RX_AE_THRESH_FREE_SHIFT


#define  REG_HP_CFG                       0x4140  
#define    HP_CFG_PARSE_EN                0x00000001 
#define    HP_CFG_NUM_CPU_MASK            0x000000FC 
#define    HP_CFG_NUM_CPU_SHIFT           2
#define    HP_CFG_SYN_INC_MASK            0x00000100 
#define    HP_CFG_TCP_THRESH_MASK         0x000FFE00 
#define    HP_CFG_TCP_THRESH_SHIFT        9

/* access to RX Instruction RAM. 5-bit register/counter holds addr
 * of 39 bit entry to be read/written. 32 LSB in _DATA_LOW. 7 MSB in _DATA_HI.
 * RX_DMA_EN must be 0 for RX instr PIO access. DATA_HI should be last access
 * of sequence.
 * DEFAULT: undefined
 */
#define  REG_HP_INSTR_RAM_ADDR             0x4144  
#define    HP_INSTR_RAM_ADDR_MASK          0x01F   
#define  REG_HP_INSTR_RAM_DATA_LOW         0x4148  
#define    HP_INSTR_RAM_LOW_OUTMASK_MASK   0x0000FFFF
#define    HP_INSTR_RAM_LOW_OUTMASK_SHIFT  0
#define    HP_INSTR_RAM_LOW_OUTSHIFT_MASK  0x000F0000
#define    HP_INSTR_RAM_LOW_OUTSHIFT_SHIFT 16
#define    HP_INSTR_RAM_LOW_OUTEN_MASK     0x00300000
#define    HP_INSTR_RAM_LOW_OUTEN_SHIFT    20
#define    HP_INSTR_RAM_LOW_OUTARG_MASK    0xFFC00000
#define    HP_INSTR_RAM_LOW_OUTARG_SHIFT   22
#define  REG_HP_INSTR_RAM_DATA_MID         0x414C  
#define    HP_INSTR_RAM_MID_OUTARG_MASK    0x00000003
#define    HP_INSTR_RAM_MID_OUTARG_SHIFT   0
#define    HP_INSTR_RAM_MID_OUTOP_MASK     0x0000003C
#define    HP_INSTR_RAM_MID_OUTOP_SHIFT    2
#define    HP_INSTR_RAM_MID_FNEXT_MASK     0x000007C0
#define    HP_INSTR_RAM_MID_FNEXT_SHIFT    6
#define    HP_INSTR_RAM_MID_FOFF_MASK      0x0003F800
#define    HP_INSTR_RAM_MID_FOFF_SHIFT     11
#define    HP_INSTR_RAM_MID_SNEXT_MASK     0x007C0000
#define    HP_INSTR_RAM_MID_SNEXT_SHIFT    18
#define    HP_INSTR_RAM_MID_SOFF_MASK      0x3F800000
#define    HP_INSTR_RAM_MID_SOFF_SHIFT     23
#define    HP_INSTR_RAM_MID_OP_MASK        0xC0000000
#define    HP_INSTR_RAM_MID_OP_SHIFT       30
#define  REG_HP_INSTR_RAM_DATA_HI          0x4150  
#define    HP_INSTR_RAM_HI_VAL_MASK        0x0000FFFF
#define    HP_INSTR_RAM_HI_VAL_SHIFT       0
#define    HP_INSTR_RAM_HI_MASK_MASK       0xFFFF0000
#define    HP_INSTR_RAM_HI_MASK_SHIFT      16

#define  REG_HP_DATA_RAM_FDB_ADDR          0x4154  
#define    HP_DATA_RAM_FDB_DATA_MASK       0x001F  
#define    HP_DATA_RAM_FDB_FDB_MASK        0x3F00  
#define  REG_HP_DATA_RAM_DATA              0x4158  

#define  REG_HP_FLOW_DB0                   0x415C  
#define  REG_HP_FLOW_DBN(x)                (REG_HP_FLOW_DB0 + (x)*4)

#define  REG_HP_STATE_MACHINE              0x418C  
#define  REG_HP_STATUS0                    0x4190  
#define    HP_STATUS0_SAP_MASK             0xFFFF0000 
#define    HP_STATUS0_L3_OFF_MASK          0x0000FE00 
#define    HP_STATUS0_LB_CPUNUM_MASK       0x000001F8 
#define    HP_STATUS0_HRP_OPCODE_MASK      0x00000007 

#define  REG_HP_STATUS1                    0x4194  
#define    HP_STATUS1_ACCUR2_MASK          0xE0000000 
#define    HP_STATUS1_FLOWID_MASK          0x1F800000 
#define    HP_STATUS1_TCP_OFF_MASK         0x007F0000 
#define    HP_STATUS1_TCP_SIZE_MASK        0x0000FFFF 

#define  REG_HP_STATUS2                    0x4198  
#define    HP_STATUS2_ACCUR2_MASK          0xF0000000 
#define    HP_STATUS2_CSUM_OFF_MASK        0x07F00000 
#define    HP_STATUS2_ACCUR1_MASK          0x000FE000 
#define    HP_STATUS2_FORCE_DROP           0x00001000 
#define    HP_STATUS2_BWO_REASSM           0x00000800 
#define    HP_STATUS2_JH_SPLIT_EN          0x00000400 
#define    HP_STATUS2_FORCE_TCP_NOCHECK    0x00000200 
#define    HP_STATUS2_DATA_MASK_ZERO       0x00000100 
#define    HP_STATUS2_FORCE_TCP_CHECK      0x00000080 
#define    HP_STATUS2_MASK_TCP_THRESH      0x00000040 
#define    HP_STATUS2_NO_ASSIST            0x00000020 
#define    HP_STATUS2_CTRL_PACKET_FLAG     0x00000010 
#define    HP_STATUS2_TCP_FLAG_CHECK       0x00000008 
#define    HP_STATUS2_SYN_FLAG             0x00000004 
#define    HP_STATUS2_TCP_CHECK            0x00000002 
#define    HP_STATUS2_TCP_NOCHECK          0x00000001 

#define  REG_HP_RAM_BIST                   0x419C  
#define    HP_RAM_BIST_HP_DATA_PASS        0x80000000 
#define    HP_RAM_BIST_HP_INSTR0_PASS      0x40000000 
#define    HP_RAM_BIST_HP_INSTR1_PASS      0x20000000 
#define    HP_RAM_BIST_HP_INSTR2_PASS      0x10000000 
#define    HP_RAM_BIST_FDBM_AGE0_PASS      0x08000000 
#define    HP_RAM_BIST_FDBM_AGE1_PASS      0x04000000 
#define    HP_RAM_BIST_FDBM_FLOWID00_PASS  0x02000000 
#define    HP_RAM_BIST_FDBM_FLOWID10_PASS  0x01000000 
#define    HP_RAM_BIST_FDBM_FLOWID20_PASS  0x00800000 
#define    HP_RAM_BIST_FDBM_FLOWID30_PASS  0x00400000 
#define    HP_RAM_BIST_FDBM_FLOWID01_PASS  0x00200000 
#define    HP_RAM_BIST_FDBM_FLOWID11_PASS  0x00100000 
#define    HP_RAM_BIST_FDBM_FLOWID21_PASS  0x00080000 
#define    HP_RAM_BIST_FDBM_FLOWID31_PASS  0x00040000 
#define    HP_RAM_BIST_FDBM_TCPSEQ_PASS    0x00020000 
#define    HP_RAM_BIST_SUMMARY             0x00000002 
#define    HP_RAM_BIST_START               0x00000001 


#define  REG_MAC_TX_RESET                  0x6000  
#define  REG_MAC_RX_RESET                  0x6004  
#define  REG_MAC_SEND_PAUSE                0x6008  
#define    MAC_SEND_PAUSE_TIME_MASK        0x0000FFFF 
#define    MAC_SEND_PAUSE_SEND             0x00010000 

#define  REG_MAC_TX_STATUS                 0x6010  
#define    MAC_TX_FRAME_XMIT               0x0001  
#define    MAC_TX_UNDERRUN                 0x0002  
#define    MAC_TX_MAX_PACKET_ERR           0x0004  
#define    MAC_TX_COLL_NORMAL              0x0008  
#define    MAC_TX_COLL_EXCESS              0x0010  
#define    MAC_TX_COLL_LATE                0x0020  
#define    MAC_TX_COLL_FIRST               0x0040  
#define    MAC_TX_DEFER_TIMER              0x0080  
#define    MAC_TX_PEAK_ATTEMPTS            0x0100  

#define  REG_MAC_RX_STATUS                 0x6014  
#define    MAC_RX_FRAME_RECV               0x0001  
#define    MAC_RX_OVERFLOW                 0x0002  
#define    MAC_RX_FRAME_COUNT              0x0004  
#define    MAC_RX_ALIGN_ERR                0x0008  
#define    MAC_RX_CRC_ERR                  0x0010  
#define    MAC_RX_LEN_ERR                  0x0020  
#define    MAC_RX_VIOL_ERR                 0x0040  

#define  REG_MAC_CTRL_STATUS               0x6018  
#define    MAC_CTRL_PAUSE_RECEIVED         0x00000001  
#define    MAC_CTRL_PAUSE_STATE            0x00000002  
#define    MAC_CTRL_NOPAUSE_STATE          0x00000004  
#define    MAC_CTRL_PAUSE_TIME_MASK        0xFFFF0000  

#define  REG_MAC_TX_MASK                   0x6020  
#define  REG_MAC_RX_MASK                   0x6024  
#define  REG_MAC_CTRL_MASK                 0x6028  

#define  REG_MAC_TX_CFG                 0x6030  
#define    MAC_TX_CFG_EN                0x0001  
#define    MAC_TX_CFG_IGNORE_CARRIER    0x0002  
#define    MAC_TX_CFG_IGNORE_COLL       0x0004  
#define    MAC_TX_CFG_IPG_EN            0x0008  
#define    MAC_TX_CFG_NEVER_GIVE_UP_EN  0x0010  
#define    MAC_TX_CFG_NEVER_GIVE_UP_LIM 0x0020  
#define    MAC_TX_CFG_NO_BACKOFF        0x0040  
#define    MAC_TX_CFG_SLOW_DOWN         0x0080  
#define    MAC_TX_CFG_NO_FCS            0x0100  
#define    MAC_TX_CFG_CARRIER_EXTEND    0x0200  

#define  REG_MAC_RX_CFG                 0x6034  
#define    MAC_RX_CFG_EN                0x0001  
#define    MAC_RX_CFG_STRIP_PAD         0x0002  
#define    MAC_RX_CFG_STRIP_FCS         0x0004  
#define    MAC_RX_CFG_PROMISC_EN        0x0008  
#define    MAC_RX_CFG_PROMISC_GROUP_EN  0x0010  
#define    MAC_RX_CFG_HASH_FILTER_EN    0x0020  
#define    MAC_RX_CFG_ADDR_FILTER_EN    0x0040  
#define    MAC_RX_CFG_DISABLE_DISCARD   0x0080  
#define    MAC_RX_CFG_CARRIER_EXTEND    0x0100  

#define  REG_MAC_CTRL_CFG               0x6038  
#define    MAC_CTRL_CFG_SEND_PAUSE_EN   0x0001  
#define    MAC_CTRL_CFG_RECV_PAUSE_EN   0x0002  
#define    MAC_CTRL_CFG_PASS_CTRL       0x0004  

#define  REG_MAC_XIF_CFG                0x603C  
#define    MAC_XIF_TX_MII_OUTPUT_EN        0x0001  
#define    MAC_XIF_MII_INT_LOOPBACK        0x0002  
#define    MAC_XIF_DISABLE_ECHO            0x0004  
#define    MAC_XIF_GMII_MODE               0x0008  
#define    MAC_XIF_MII_BUFFER_OUTPUT_EN    0x0010  
#define    MAC_XIF_LINK_LED                0x0020  
#define    MAC_XIF_FDPLX_LED               0x0040  

#define  REG_MAC_IPG0                      0x6040  
#define  REG_MAC_IPG1                      0x6044  
#define  REG_MAC_IPG2                      0x6048  
#define  REG_MAC_SLOT_TIME                 0x604C  
#define  REG_MAC_FRAMESIZE_MIN             0x6050  

#define  REG_MAC_FRAMESIZE_MAX             0x6054  
#define    MAC_FRAMESIZE_MAX_BURST_MASK    0x3FFF0000 
#define    MAC_FRAMESIZE_MAX_BURST_SHIFT   16
#define    MAC_FRAMESIZE_MAX_FRAME_MASK    0x00007FFF 
#define    MAC_FRAMESIZE_MAX_FRAME_SHIFT   0
#define  REG_MAC_PA_SIZE                   0x6058  
#define  REG_MAC_JAM_SIZE                  0x605C  
#define  REG_MAC_ATTEMPT_LIMIT             0x6060  
#define  REG_MAC_CTRL_TYPE                 0x6064  

#define  REG_MAC_ADDR0                     0x6080  
#define  REG_MAC_ADDRN(x)                  (REG_MAC_ADDR0 + (x)*4)
#define  REG_MAC_ADDR_FILTER0              0x614C  
#define  REG_MAC_ADDR_FILTER1              0x6150  
#define  REG_MAC_ADDR_FILTER2              0x6154  
#define  REG_MAC_ADDR_FILTER2_1_MASK       0x6158  
#define  REG_MAC_ADDR_FILTER0_MASK         0x615C  

#define  REG_MAC_HASH_TABLE0               0x6160  
#define  REG_MAC_HASH_TABLEN(x)            (REG_MAC_HASH_TABLE0 + (x)*4)

#define  REG_MAC_COLL_NORMAL               0x61A0 
#define  REG_MAC_COLL_FIRST                0x61A4 
#define  REG_MAC_COLL_EXCESS               0x61A8 
#define  REG_MAC_COLL_LATE                 0x61AC 
#define  REG_MAC_TIMER_DEFER               0x61B0 
#define  REG_MAC_ATTEMPTS_PEAK             0x61B4 
#define  REG_MAC_RECV_FRAME                0x61B8 
#define  REG_MAC_LEN_ERR                   0x61BC 
#define  REG_MAC_ALIGN_ERR                 0x61C0 
#define  REG_MAC_FCS_ERR                   0x61C4 
#define  REG_MAC_RX_CODE_ERR               0x61C8 

#define  REG_MAC_RANDOM_SEED               0x61CC 


#define  REG_MAC_STATE_MACHINE             0x61D0 
#define    MAC_SM_RLM_MASK                 0x07800000
#define    MAC_SM_RLM_SHIFT                23
#define    MAC_SM_RX_FC_MASK               0x00700000
#define    MAC_SM_RX_FC_SHIFT              20
#define    MAC_SM_TLM_MASK                 0x000F0000
#define    MAC_SM_TLM_SHIFT                16
#define    MAC_SM_ENCAP_SM_MASK            0x0000F000
#define    MAC_SM_ENCAP_SM_SHIFT           12
#define    MAC_SM_TX_REQ_MASK              0x00000C00
#define    MAC_SM_TX_REQ_SHIFT             10
#define    MAC_SM_TX_FC_MASK               0x000003C0
#define    MAC_SM_TX_FC_SHIFT              6
#define    MAC_SM_FIFO_WRITE_SEL_MASK      0x00000038
#define    MAC_SM_FIFO_WRITE_SEL_SHIFT     3
#define    MAC_SM_TX_FIFO_EMPTY_MASK       0x00000007
#define    MAC_SM_TX_FIFO_EMPTY_SHIFT      0

#define  REG_MIF_BIT_BANG_CLOCK            0x6200 
#define  REG_MIF_BIT_BANG_DATA             0x6204 
#define  REG_MIF_BIT_BANG_OUTPUT_EN        0x6208 

#define  REG_MIF_FRAME                     0x620C 
#define    MIF_FRAME_START_MASK            0xC0000000 
#define    MIF_FRAME_ST                    0x40000000 
#define    MIF_FRAME_OPCODE_MASK           0x30000000 
#define    MIF_FRAME_OP_READ               0x20000000 
#define    MIF_FRAME_OP_WRITE              0x10000000 
#define    MIF_FRAME_PHY_ADDR_MASK         0x0F800000 
#define    MIF_FRAME_PHY_ADDR_SHIFT        23
#define    MIF_FRAME_REG_ADDR_MASK         0x007C0000 /* register address.
							 when issuing an instr,
							 addr of register
							 to be read/written */
#define    MIF_FRAME_REG_ADDR_SHIFT        18
#define    MIF_FRAME_TURN_AROUND_MSB       0x00020000 
#define    MIF_FRAME_TURN_AROUND_LSB       0x00010000 
#define    MIF_FRAME_DATA_MASK             0x0000FFFF /* instruction payload
							 load with 16-bit data
							 to be written in
							 transceiver reg for a
							 write. doesn't matter
							 in a read. when
							 polling for
							 completion, field is
							 "don't care" for write
							 and 16-bit data
							 returned by the
							 transceiver for a
							 read (if valid bit
							 is set) */
#define  REG_MIF_CFG                    0x6210 
#define    MIF_CFG_PHY_SELECT           0x0001 
#define    MIF_CFG_POLL_EN              0x0002 
#define    MIF_CFG_BB_MODE              0x0004 
#define    MIF_CFG_POLL_REG_MASK        0x00F8 
#define    MIF_CFG_POLL_REG_SHIFT       3
#define    MIF_CFG_MDIO_0               0x0100 
#define    MIF_CFG_MDIO_1               0x0200 
#define    MIF_CFG_POLL_PHY_MASK        0x7C00 
#define    MIF_CFG_POLL_PHY_SHIFT       10

#define  REG_MIF_MASK                      0x6214 

#define  REG_MIF_STATUS                    0x6218 
#define    MIF_STATUS_POLL_DATA_MASK       0xFFFF0000 
#define    MIF_STATUS_POLL_DATA_SHIFT      16
#define    MIF_STATUS_POLL_STATUS_MASK     0x0000FFFF 
#define    MIF_STATUS_POLL_STATUS_SHIFT    0

#define  REG_MIF_STATE_MACHINE             0x621C 
#define    MIF_SM_CONTROL_MASK             0x07   
#define    MIF_SM_EXECUTION_MASK           0x60   


#define  REG_PCS_MII_CTRL                  0x9000 
#define    PCS_MII_CTRL_1000_SEL           0x0040 
#define    PCS_MII_CTRL_COLLISION_TEST     0x0080 
#define    PCS_MII_CTRL_DUPLEX             0x0100 
#define    PCS_MII_RESTART_AUTONEG         0x0200 
#define    PCS_MII_ISOLATE                 0x0400 
#define    PCS_MII_POWER_DOWN              0x0800 
#define    PCS_MII_AUTONEG_EN              0x1000 
#define    PCS_MII_10_100_SEL              0x2000 
#define    PCS_MII_RESET                   0x8000 

#define  REG_PCS_MII_STATUS                0x9004 
#define    PCS_MII_STATUS_EXTEND_CAP       0x0001 
#define    PCS_MII_STATUS_JABBER_DETECT    0x0002 
#define    PCS_MII_STATUS_LINK_STATUS      0x0004 
#define    PCS_MII_STATUS_AUTONEG_ABLE     0x0008 
#define    PCS_MII_STATUS_REMOTE_FAULT     0x0010 
#define    PCS_MII_STATUS_AUTONEG_COMP     0x0020 
#define    PCS_MII_STATUS_EXTEND_STATUS    0x0100 

#define  REG_PCS_MII_ADVERT                0x9008 
#define    PCS_MII_ADVERT_FD               0x0020  
#define    PCS_MII_ADVERT_HD               0x0040  
#define    PCS_MII_ADVERT_SYM_PAUSE        0x0080  
#define    PCS_MII_ADVERT_ASYM_PAUSE       0x0100  
#define    PCS_MII_ADVERT_RF_MASK          0x3000 
#define    PCS_MII_ADVERT_ACK              0x4000 
#define    PCS_MII_ADVERT_NEXT_PAGE        0x8000 

#define  REG_PCS_MII_LPA                   0x900C 
#define    PCS_MII_LPA_FD             PCS_MII_ADVERT_FD
#define    PCS_MII_LPA_HD             PCS_MII_ADVERT_HD
#define    PCS_MII_LPA_SYM_PAUSE      PCS_MII_ADVERT_SYM_PAUSE
#define    PCS_MII_LPA_ASYM_PAUSE     PCS_MII_ADVERT_ASYM_PAUSE
#define    PCS_MII_LPA_RF_MASK        PCS_MII_ADVERT_RF_MASK
#define    PCS_MII_LPA_ACK            PCS_MII_ADVERT_ACK
#define    PCS_MII_LPA_NEXT_PAGE      PCS_MII_ADVERT_NEXT_PAGE

#define  REG_PCS_CFG                       0x9010 
#define    PCS_CFG_EN                      0x01   
#define    PCS_CFG_SD_OVERRIDE             0x02   
#define    PCS_CFG_SD_ACTIVE_LOW           0x04   
#define    PCS_CFG_JITTER_STUDY_MASK       0x18   
#define    PCS_CFG_10MS_TIMER_OVERRIDE     0x20   

#define  REG_PCS_STATE_MACHINE             0x9014 
#define    PCS_SM_TX_STATE_MASK            0x0000000F 
#define    PCS_SM_RX_STATE_MASK            0x000000F0 
#define    PCS_SM_WORD_SYNC_STATE_MASK     0x00000700 
#define    PCS_SM_SEQ_DETECT_STATE_MASK    0x00001800 
#define    PCS_SM_LINK_STATE_MASK          0x0001E000
#define        SM_LINK_STATE_UP            0x00016000 

#define    PCS_SM_LOSS_LINK_C              0x00100000 
#define    PCS_SM_LOSS_LINK_SYNC           0x00200000 
#define    PCS_SM_LOSS_SIGNAL_DETECT       0x00400000 
#define    PCS_SM_NO_LINK_BREAKLINK        0x01000000 
#define    PCS_SM_NO_LINK_SERDES           0x02000000 
#define    PCS_SM_NO_LINK_C                0x04000000 
#define    PCS_SM_NO_LINK_SYNC             0x08000000 
#define    PCS_SM_NO_LINK_WAIT_C           0x10000000 
#define    PCS_SM_NO_LINK_NO_IDLE          0x20000000 

#define  REG_PCS_INTR_STATUS               0x9018 
#define    PCS_INTR_STATUS_LINK_CHANGE     0x04   

#define  REG_PCS_DATAPATH_MODE             0x9050 
#define    PCS_DATAPATH_MODE_MII           0x00 
#define    PCS_DATAPATH_MODE_SERDES        0x02 

#define  REG_PCS_SERDES_CTRL              0x9054 
#define    PCS_SERDES_CTRL_LOOPBACK       0x01   
#define    PCS_SERDES_CTRL_SYNCD_EN       0x02   
#define    PCS_SERDES_CTRL_LOCKREF       0x04   

#define  REG_PCS_SHARED_OUTPUT_SEL         0x9058 
#define    PCS_SOS_PROM_ADDR_MASK          0x0007

#define  REG_PCS_SERDES_STATE              0x905C 
#define    PCS_SERDES_STATE_MASK           0x03

#define  REG_PCS_PACKET_COUNT              0x9060 
#define    PCS_PACKET_COUNT_TX             0x000007FF 
#define    PCS_PACKET_COUNT_RX             0x07FF0000 

#define  REG_EXPANSION_ROM_RUN_START       0x100000 
#define  REG_EXPANSION_ROM_RUN_END         0x17FFFF

#define  REG_SECOND_LOCALBUS_START         0x180000 
#define  REG_SECOND_LOCALBUS_END           0x1FFFFF

#define  REG_ENTROPY_START                 REG_SECOND_LOCALBUS_START
#define  REG_ENTROPY_DATA                  (REG_ENTROPY_START + 0x00)
#define  REG_ENTROPY_STATUS                (REG_ENTROPY_START + 0x04)
#define      ENTROPY_STATUS_DRDY           0x01
#define      ENTROPY_STATUS_BUSY           0x02
#define      ENTROPY_STATUS_CIPHER         0x04
#define      ENTROPY_STATUS_BYPASS_MASK    0x18
#define  REG_ENTROPY_MODE                  (REG_ENTROPY_START + 0x05)
#define      ENTROPY_MODE_KEY_MASK         0x07
#define      ENTROPY_MODE_ENCRYPT          0x40
#define  REG_ENTROPY_RAND_REG              (REG_ENTROPY_START + 0x06)
#define  REG_ENTROPY_RESET                 (REG_ENTROPY_START + 0x07)
#define      ENTROPY_RESET_DES_IO          0x01
#define      ENTROPY_RESET_STC_MODE        0x02
#define      ENTROPY_RESET_KEY_CACHE       0x04
#define      ENTROPY_RESET_IV              0x08
#define  REG_ENTROPY_IV                    (REG_ENTROPY_START + 0x08)
#define  REG_ENTROPY_KEY0                  (REG_ENTROPY_START + 0x10)
#define  REG_ENTROPY_KEYN(x)               (REG_ENTROPY_KEY0 + 4*(x))

#define PHY_LUCENT_B0     0x00437421
#define   LUCENT_MII_REG      0x1F

#define PHY_NS_DP83065    0x20005c78
#define   DP83065_MII_MEM     0x16
#define   DP83065_MII_REGD    0x1D
#define   DP83065_MII_REGE    0x1E

#define PHY_BROADCOM_5411 0x00206071
#define PHY_BROADCOM_B0   0x00206050
#define   BROADCOM_MII_REG4   0x14
#define   BROADCOM_MII_REG5   0x15
#define   BROADCOM_MII_REG7   0x17
#define   BROADCOM_MII_REG8   0x18

#define   CAS_MII_ANNPTR          0x07
#define   CAS_MII_ANNPRR          0x08
#define   CAS_MII_1000_CTRL       0x09
#define   CAS_MII_1000_STATUS     0x0A
#define   CAS_MII_1000_EXTEND     0x0F

#define   CAS_BMSR_1000_EXTEND    0x0100 
#define   CAS_BMCR_SPEED1000      0x0040  

#define   CAS_ADVERTISE_1000HALF   0x0100
#define   CAS_ADVERTISE_1000FULL   0x0200
#define   CAS_ADVERTISE_PAUSE      0x0400
#define   CAS_ADVERTISE_ASYM_PAUSE 0x0800

#define   CAS_LPA_PAUSE	           CAS_ADVERTISE_PAUSE
#define   CAS_LPA_ASYM_PAUSE       CAS_ADVERTISE_ASYM_PAUSE

#define   CAS_LPA_1000HALF        0x0400
#define   CAS_LPA_1000FULL        0x0800

#define   CAS_EXTEND_1000XFULL    0x8000
#define   CAS_EXTEND_1000XHALF    0x4000
#define   CAS_EXTEND_1000TFULL    0x2000
#define   CAS_EXTEND_1000THALF    0x1000

typedef struct cas_hp_inst {
	const char *note;

	u16 mask, val;

	u8 op;
	u8 soff, snext;	
	u8 foff, fnext;	
	
	u8 outop;    

	u16 outarg;  
	u8 outenab;  
	u8 outshift; 
	u16 outmask;
} cas_hp_inst_t;

#define OP_EQ     0 
#define OP_LT     1 
#define OP_GT     2 
#define OP_NP     3 

#define	CL_REG	0
#define	LD_FID	1
#define	LD_SEQ	2
#define	LD_CTL	3
#define	LD_SAP	4
#define	LD_R1	5
#define	LD_L3	6
#define	LD_SUM	7
#define	LD_HDR	8
#define	IM_FID	9
#define	IM_SEQ	10
#define	IM_SAP	11
#define	IM_R1	12
#define	IM_CTL	13
#define	LD_LEN	14
#define	ST_FLG	15

#define S1_PCKT         0
#define S1_VLAN         1
#define S1_CFI          2
#define S1_8023         3
#define S1_LLC          4
#define S1_LLCc         5
#define S1_IPV4         6
#define S1_IPV4c        7
#define S1_IPV4F        8
#define S1_TCP44        9
#define S1_IPV6         10
#define S1_IPV6L        11
#define S1_IPV6c        12
#define S1_TCP64        13
#define S1_TCPSQ        14
#define S1_TCPFG        15
#define	S1_TCPHL	16
#define	S1_TCPHc	17
#define	S1_CLNP		18
#define	S1_CLNP2	19
#define	S1_DROP		20
#define	S2_HTTP		21
#define	S1_ESP4		22
#define	S1_AH4		23
#define	S1_ESP6		24
#define	S1_AH6		25

#define CAS_PROG_IP46TCP4_PREAMBLE \
{ "packet arrival?", 0xffff, 0x0000, OP_NP,  6, S1_VLAN,  0, S1_PCKT,  \
  CL_REG, 0x3ff, 1, 0x0, 0x0000}, \
{ "VLAN?", 0xffff, 0x8100, OP_EQ,  1, S1_CFI,   0, S1_8023,  \
  IM_CTL, 0x00a,  3, 0x0, 0xffff}, \
{ "CFI?", 0x1000, 0x1000, OP_EQ,  0, S1_DROP,  1, S1_8023, \
  CL_REG, 0x000,  0, 0x0, 0x0000}, \
{ "8023?", 0xffff, 0x0600, OP_LT,  1, S1_LLC,   0, S1_IPV4, \
  CL_REG, 0x000,  0, 0x0, 0x0000}, \
{ "LLC?", 0xffff, 0xaaaa, OP_EQ,  1, S1_LLCc,  0, S1_CLNP, \
  CL_REG, 0x000,  0, 0x0, 0x0000}, \
{ "LLCc?", 0xff00, 0x0300, OP_EQ,  2, S1_IPV4,  0, S1_CLNP, \
  CL_REG, 0x000,  0, 0x0, 0x0000}, \
{ "IPV4?", 0xffff, 0x0800, OP_EQ,  1, S1_IPV4c, 0, S1_IPV6, \
  LD_SAP, 0x100,  3, 0x0, 0xffff}, \
{ "IPV4 cont?", 0xff00, 0x4500, OP_EQ,  3, S1_IPV4F, 0, S1_CLNP, \
  LD_SUM, 0x00a,  1, 0x0, 0x0000}, \
{ "IPV4 frag?", 0x3fff, 0x0000, OP_EQ,  1, S1_TCP44, 0, S1_CLNP, \
  LD_LEN, 0x03e,  1, 0x0, 0xffff}, \
{ "TCP44?", 0x00ff, 0x0006, OP_EQ,  7, S1_TCPSQ, 0, S1_CLNP, \
  LD_FID, 0x182,  1, 0x0, 0xffff},  \
{ "IPV6?", 0xffff, 0x86dd, OP_EQ,  1, S1_IPV6L, 0, S1_CLNP,  \
  LD_SUM, 0x015,  1, 0x0, 0x0000}, \
{ "IPV6 len", 0xf000, 0x6000, OP_EQ,  0, S1_IPV6c, 0, S1_CLNP, \
  IM_R1,  0x128,  1, 0x0, 0xffff}, \
{ "IPV6 cont?", 0x0000, 0x0000, OP_EQ,  3, S1_TCP64, 0, S1_CLNP, \
  LD_FID, 0x484,  1, 0x0, 0xffff},  \
{ "TCP64?", 0xff00, 0x0600, OP_EQ, 18, S1_TCPSQ, 0, S1_CLNP, \
  LD_LEN, 0x03f,  1, 0x0, 0xffff}

#ifdef USE_HP_IP46TCP4
static cas_hp_inst_t cas_prog_ip46tcp4tab[] = {
	CAS_PROG_IP46TCP4_PREAMBLE,
	{ "TCP seq", 
	  0x0000, 0x0000, OP_EQ, 0, S1_TCPFG, 4, S1_TCPFG, LD_SEQ,
	  0x081,  3, 0x0, 0xffff}, 
	{ "TCP control flags", 0x0000, 0x0000, OP_EQ,  0, S1_TCPHL, 0,
	  S1_TCPHL, ST_FLG, 0x045,  3, 0x0, 0x002f}, 
	{ "TCP length", 0x0000, 0x0000, OP_EQ,  0, S1_TCPHc, 0,
	  S1_TCPHc, LD_R1,  0x205,  3, 0xB, 0xf000},
	{ "TCP length cont", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0,
	  S1_PCKT,  LD_HDR, 0x0ff,  3, 0x0, 0xffff},
	{ "Cleanup", 0x0000, 0x0000, OP_EQ,  0, S1_CLNP2,  0, S1_CLNP2,
	  IM_CTL, 0x001,  3, 0x0, 0x0001},
	{ "Cleanup 2", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  IM_CTL, 0x000,  0, 0x0, 0x0000},
	{ "Drop packet", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  IM_CTL, 0x080,  3, 0x0, 0xffff},
	{ NULL },
};
#ifdef HP_IP46TCP4_DEFAULT
#define CAS_HP_FIRMWARE               cas_prog_ip46tcp4tab
#endif
#endif

#ifdef USE_HP_IP46TCP4NOHTTP
static cas_hp_inst_t cas_prog_ip46tcp4nohttptab[] = {
	CAS_PROG_IP46TCP4_PREAMBLE,
	{ "TCP seq", 
	  0xFFFF, 0x0080, OP_EQ,  0, S2_HTTP,  0, S1_TCPFG, LD_SEQ,
	  0x081,  3, 0x0, 0xffff} , 
	{ "TCP control flags", 0xFFFF, 0x8080, OP_EQ,  0, S2_HTTP,  0,
	  S1_TCPHL, ST_FLG, 0x145,  2, 0x0, 0x002f, }, 
	{ "TCP length", 0x0000, 0x0000, OP_EQ,  0, S1_TCPHc, 0, S1_TCPHc,
	  LD_R1,  0x205,  3, 0xB, 0xf000},
	{ "TCP length cont", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  LD_HDR, 0x0ff,  3, 0x0, 0xffff},
	{ "Cleanup", 0x0000, 0x0000, OP_EQ,  0, S1_CLNP2,  0, S1_CLNP2,
	  IM_CTL, 0x001,  3, 0x0, 0x0001},
	{ "Cleanup 2", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  CL_REG, 0x002,  3, 0x0, 0x0000},
	{ "Drop packet", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  IM_CTL, 0x080,  3, 0x0, 0xffff},
	{ "No HTTP", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  IM_CTL, 0x044,  3, 0x0, 0xffff},
	{ NULL },
};
#ifdef HP_IP46TCP4NOHTTP_DEFAULT
#define CAS_HP_FIRMWARE               cas_prog_ip46tcp4nohttptab
#endif
#endif

#define	S3_IPV6c	11
#define	S3_TCP64	12
#define	S3_TCPSQ	13
#define	S3_TCPFG	14
#define	S3_TCPHL	15
#define	S3_TCPHc	16
#define	S3_FRAG		17
#define	S3_FOFF		18
#define	S3_CLNP		19

#ifdef USE_HP_IP4FRAG
static cas_hp_inst_t cas_prog_ip4fragtab[] = {
	{ "packet arrival?", 0xffff, 0x0000, OP_NP,  6, S1_VLAN,  0, S1_PCKT,
	  CL_REG, 0x3ff, 1, 0x0, 0x0000},
	{ "VLAN?", 0xffff, 0x8100, OP_EQ,  1, S1_CFI,   0, S1_8023,
	  IM_CTL, 0x00a,  3, 0x0, 0xffff},
	{ "CFI?", 0x1000, 0x1000, OP_EQ,  0, S3_CLNP,  1, S1_8023,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "8023?", 0xffff, 0x0600, OP_LT,  1, S1_LLC,   0, S1_IPV4,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "LLC?", 0xffff, 0xaaaa, OP_EQ,  1, S1_LLCc,  0, S3_CLNP,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "LLCc?",0xff00, 0x0300, OP_EQ,  2, S1_IPV4,  0, S3_CLNP,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "IPV4?", 0xffff, 0x0800, OP_EQ,  1, S1_IPV4c, 0, S1_IPV6,
	  LD_SAP, 0x100,  3, 0x0, 0xffff},
	{ "IPV4 cont?", 0xff00, 0x4500, OP_EQ,  3, S1_IPV4F, 0, S3_CLNP,
	  LD_SUM, 0x00a,  1, 0x0, 0x0000},
	{ "IPV4 frag?", 0x3fff, 0x0000, OP_EQ,  1, S1_TCP44, 0, S3_FRAG,
	  LD_LEN, 0x03e,  3, 0x0, 0xffff},
	{ "TCP44?", 0x00ff, 0x0006, OP_EQ,  7, S3_TCPSQ, 0, S3_CLNP,
	  LD_FID, 0x182,  3, 0x0, 0xffff}, 
	{ "IPV6?", 0xffff, 0x86dd, OP_EQ,  1, S3_IPV6c, 0, S3_CLNP,
	  LD_SUM, 0x015,  1, 0x0, 0x0000},
	{ "IPV6 cont?", 0xf000, 0x6000, OP_EQ,  3, S3_TCP64, 0, S3_CLNP,
	  LD_FID, 0x484,  1, 0x0, 0xffff}, 
	{ "TCP64?", 0xff00, 0x0600, OP_EQ, 18, S3_TCPSQ, 0, S3_CLNP,
	  LD_LEN, 0x03f,  1, 0x0, 0xffff},
	{ "TCP seq",	
	  0x0000, 0x0000, OP_EQ,  0, S3_TCPFG, 4, S3_TCPFG, LD_SEQ,
	  0x081,  3, 0x0, 0xffff}, 
	{ "TCP control flags", 0x0000, 0x0000, OP_EQ,  0, S3_TCPHL, 0,
	  S3_TCPHL, ST_FLG, 0x045,  3, 0x0, 0x002f}, 
	{ "TCP length", 0x0000, 0x0000, OP_EQ,  0, S3_TCPHc, 0, S3_TCPHc,
	  LD_R1,  0x205,  3, 0xB, 0xf000},
	{ "TCP length cont", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  LD_HDR, 0x0ff,  3, 0x0, 0xffff},
	{ "IP4 Fragment", 0x0000, 0x0000, OP_EQ,  0, S3_FOFF,  0, S3_FOFF,
	  LD_FID, 0x103,  3, 0x0, 0xffff}, 
	{ "IP4 frag offset", 0x0000, 0x0000, OP_EQ,  0, S3_FOFF,  0, S3_FOFF,
	  LD_SEQ, 0x040,  1, 0xD, 0xfff8},
	{ "Cleanup", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  IM_CTL, 0x001,  3, 0x0, 0x0001},
	{ NULL },
};
#ifdef HP_IP4FRAG_DEFAULT
#define CAS_HP_FIRMWARE               cas_prog_ip4fragtab
#endif
#endif

#ifdef USE_HP_IP46TCP4BATCH
static cas_hp_inst_t cas_prog_ip46tcp4batchtab[] = {
	CAS_PROG_IP46TCP4_PREAMBLE,
	{ "TCP seq",	
	  0x0000, 0x0000, OP_EQ,  0, S1_TCPFG, 0, S1_TCPFG, LD_SEQ,
	  0x081,  3, 0x0, 0xffff}, 
	{ "TCP control flags", 0x0000, 0x0000, OP_EQ,  0, S1_TCPHL, 0,
	  S1_TCPHL, ST_FLG, 0x000,  3, 0x0, 0x0000}, 
	{ "TCP length", 0x0000, 0x0000, OP_EQ,  0, S1_TCPHc, 0,
	  S1_TCPHc, LD_R1,  0x205,  3, 0xB, 0xf000},
	{ "TCP length cont", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0,
	  S1_PCKT,  IM_CTL, 0x040,  3, 0x0, 0xffff}, 
	{ "Cleanup", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  IM_CTL, 0x001,  3, 0x0, 0x0001},
	{ "Drop packet", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0,
	  S1_PCKT,  IM_CTL, 0x080,  3, 0x0, 0xffff},
	{ NULL },
};
#ifdef HP_IP46TCP4BATCH_DEFAULT
#define CAS_HP_FIRMWARE               cas_prog_ip46tcp4batchtab
#endif
#endif

#ifdef USE_HP_WORKAROUND
static cas_hp_inst_t  cas_prog_workaroundtab[] = {
	{ "packet arrival?", 0xffff, 0x0000, OP_NP,  6, S1_VLAN,  0,
	  S1_PCKT,  CL_REG, 0x3ff,  1, 0x0, 0x0000} ,
	{ "VLAN?", 0xffff, 0x8100, OP_EQ,  1, S1_CFI, 0, S1_8023,
	  IM_CTL, 0x04a,  3, 0x0, 0xffff},
	{ "CFI?", 0x1000, 0x1000, OP_EQ,  0, S1_CLNP,  1, S1_8023,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "8023?", 0xffff, 0x0600, OP_LT,  1, S1_LLC,   0, S1_IPV4,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "LLC?", 0xffff, 0xaaaa, OP_EQ,  1, S1_LLCc,  0, S1_CLNP,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "LLCc?", 0xff00, 0x0300, OP_EQ,  2, S1_IPV4,  0, S1_CLNP,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "IPV4?", 0xffff, 0x0800, OP_EQ,  1, S1_IPV4c, 0, S1_IPV6,
	  IM_SAP, 0x6AE,  3, 0x0, 0xffff},
	{ "IPV4 cont?", 0xff00, 0x4500, OP_EQ,  3, S1_IPV4F, 0, S1_CLNP,
	  LD_SUM, 0x00a,  1, 0x0, 0x0000},
	{ "IPV4 frag?", 0x3fff, 0x0000, OP_EQ,  1, S1_TCP44, 0, S1_CLNP,
	  LD_LEN, 0x03e,  1, 0x0, 0xffff},
	{ "TCP44?", 0x00ff, 0x0006, OP_EQ,  7, S1_TCPSQ, 0, S1_CLNP,
	  LD_FID, 0x182,  3, 0x0, 0xffff}, 
	{ "IPV6?", 0xffff, 0x86dd, OP_EQ,  1, S1_IPV6L, 0, S1_CLNP,
	  LD_SUM, 0x015,  1, 0x0, 0x0000},
	{ "IPV6 len", 0xf000, 0x6000, OP_EQ,  0, S1_IPV6c, 0, S1_CLNP,
	  IM_R1,  0x128,  1, 0x0, 0xffff},
	{ "IPV6 cont?", 0x0000, 0x0000, OP_EQ,  3, S1_TCP64, 0, S1_CLNP,
	  LD_FID, 0x484,  1, 0x0, 0xffff}, 
	{ "TCP64?", 0xff00, 0x0600, OP_EQ, 18, S1_TCPSQ, 0, S1_CLNP,
	  LD_LEN, 0x03f,  1, 0x0, 0xffff},
	{ "TCP seq",      
	  0x0000, 0x0000, OP_EQ,  0, S1_TCPFG, 4, S1_TCPFG, LD_SEQ,
	  0x081,  3, 0x0, 0xffff}, 
	{ "TCP control flags", 0x0000, 0x0000, OP_EQ,  0, S1_TCPHL, 0,
	  S1_TCPHL, ST_FLG, 0x045,  3, 0x0, 0x002f}, 
	{ "TCP length", 0x0000, 0x0000, OP_EQ,  0, S1_TCPHc, 0, S1_TCPHc,
	  LD_R1,  0x205,  3, 0xB, 0xf000},
	{ "TCP length cont", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0,
	  S1_PCKT,  LD_HDR, 0x0ff,  3, 0x0, 0xffff},
	{ "Cleanup", 0x0000, 0x0000, OP_EQ,  0, S1_CLNP2, 0, S1_CLNP2,
	  IM_SAP, 0x6AE,  3, 0x0, 0xffff} ,
	{ "Cleanup 2", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  IM_CTL, 0x001,  3, 0x0, 0x0001},
	{ NULL },
};
#ifdef HP_WORKAROUND_DEFAULT
#define CAS_HP_FIRMWARE               cas_prog_workaroundtab
#endif
#endif

#ifdef USE_HP_ENCRYPT
static cas_hp_inst_t  cas_prog_encryptiontab[] = {
	{ "packet arrival?", 0xffff, 0x0000, OP_NP,  6, S1_VLAN,  0,
	  S1_PCKT,  CL_REG, 0x3ff,  1, 0x0, 0x0000},
	{ "VLAN?", 0xffff, 0x8100, OP_EQ,  1, S1_CFI,   0, S1_8023,
	  IM_CTL, 0x00a,  3, 0x0, 0xffff},
#if 0
	00,
#endif
	{ "CFI?", 
	  0x1000, 0x1000, OP_EQ,  0, S1_CLNP,  1, S1_8023,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "8023?", 0xffff, 0x0600, OP_LT,  1, S1_LLC,   0, S1_IPV4,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "LLC?", 0xffff, 0xaaaa, OP_EQ,  1, S1_LLCc,  0, S1_CLNP,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "LLCc?", 0xff00, 0x0300, OP_EQ,  2, S1_IPV4,  0, S1_CLNP,
	  CL_REG, 0x000,  0, 0x0, 0x0000},
	{ "IPV4?", 0xffff, 0x0800, OP_EQ,  1, S1_IPV4c, 0, S1_IPV6,
	  LD_SAP, 0x100,  3, 0x0, 0xffff},
	{ "IPV4 cont?", 0xff00, 0x4500, OP_EQ,  3, S1_IPV4F, 0, S1_CLNP,
	  LD_SUM, 0x00a,  1, 0x0, 0x0000},
	{ "IPV4 frag?", 0x3fff, 0x0000, OP_EQ,  1, S1_TCP44, 0, S1_CLNP,
	  LD_LEN, 0x03e,  1, 0x0, 0xffff},
	{ "TCP44?", 0x00ff, 0x0006, OP_EQ,  7, S1_TCPSQ, 0, S1_ESP4,
	  LD_FID, 0x182,  1, 0x0, 0xffff}, 
	{ "IPV6?", 0xffff, 0x86dd, OP_EQ,  1, S1_IPV6L, 0, S1_CLNP,
	  LD_SUM, 0x015,  1, 0x0, 0x0000},
	{ "IPV6 len", 0xf000, 0x6000, OP_EQ,  0, S1_IPV6c, 0, S1_CLNP,
	  IM_R1,  0x128,  1, 0x0, 0xffff},
	{ "IPV6 cont?", 0x0000, 0x0000, OP_EQ,  3, S1_TCP64, 0, S1_CLNP,
	  LD_FID, 0x484,  1, 0x0, 0xffff}, 
	{ "TCP64?",
#if 0
#endif
	  0xff00, 0x0600, OP_EQ, 12, S1_TCPSQ, 0, S1_ESP6,  LD_LEN,
	  0x03f,  1, 0x0, 0xffff},
	{ "TCP seq", 
	  0xFFFF, 0x0080, OP_EQ,  0, S2_HTTP,  0, S1_TCPFG, LD_SEQ,
	  0x081,  3, 0x0, 0xffff}, 
	{ "TCP control flags", 0xFFFF, 0x8080, OP_EQ,  0, S2_HTTP,  0,
	  S1_TCPHL, ST_FLG, 0x145,  2, 0x0, 0x002f}, 
	{ "TCP length", 0x0000, 0x0000, OP_EQ,  0, S1_TCPHc, 0, S1_TCPHc,
	  LD_R1,  0x205,  3, 0xB, 0xf000} ,
	{ "TCP length cont", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0,
	  S1_PCKT,  LD_HDR, 0x0ff,  3, 0x0, 0xffff},
	{ "Cleanup", 0x0000, 0x0000, OP_EQ,  0, S1_CLNP2,  0, S1_CLNP2,
	  IM_CTL, 0x001,  3, 0x0, 0x0001},
	{ "Cleanup 2", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  CL_REG, 0x002,  3, 0x0, 0x0000},
	{ "Drop packet", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  IM_CTL, 0x080,  3, 0x0, 0xffff},
	{ "No HTTP", 0x0000, 0x0000, OP_EQ,  0, S1_PCKT,  0, S1_PCKT,
	  IM_CTL, 0x044,  3, 0x0, 0xffff},
	{ "IPV4 ESP encrypted?",  
	  0x00ff, 0x0032, OP_EQ,  0, S1_CLNP2, 0, S1_AH4, IM_CTL,
	  0x021, 1,  0x0, 0xffff},
	{ "IPV4 AH encrypted?",   
	  0x00ff, 0x0033, OP_EQ,  0, S1_CLNP2, 0, S1_CLNP, IM_CTL,
	  0x021, 1,  0x0, 0xffff},
	{ "IPV6 ESP encrypted?",  
#if 0
#endif
	  0xff00, 0x3200, OP_EQ,  0, S1_CLNP2, 0, S1_AH6, IM_CTL,
	  0x021, 1,  0x0, 0xffff},
	{ "IPV6 AH encrypted?",   
#if 0
#endif
	  0xff00, 0x3300, OP_EQ,  0, S1_CLNP2, 0, S1_CLNP, IM_CTL,
	  0x021, 1,  0x0, 0xffff},
	{ NULL },
};
#ifdef HP_ENCRYPT_DEFAULT
#define CAS_HP_FIRMWARE               cas_prog_encryptiontab
#endif
#endif

static cas_hp_inst_t cas_prog_null[] = { {NULL} };
#ifdef HP_NULL_DEFAULT
#define CAS_HP_FIRMWARE               cas_prog_null
#endif

#define   CAS_PHY_UNKNOWN       0x00
#define   CAS_PHY_SERDES        0x01
#define   CAS_PHY_MII_MDIO0     0x02
#define   CAS_PHY_MII_MDIO1     0x04
#define   CAS_PHY_MII(x)        ((x) & (CAS_PHY_MII_MDIO0 | CAS_PHY_MII_MDIO1))


#define DESC_RING_I_TO_S(x)  (32*(1 << (x)))
#define COMP_RING_I_TO_S(x)  (128*(1 << (x)))
#define TX_DESC_RING_INDEX 4  
#define RX_DESC_RING_INDEX 4  
#define RX_COMP_RING_INDEX 4  

#if (TX_DESC_RING_INDEX > 8) || (TX_DESC_RING_INDEX < 0)
#error TX_DESC_RING_INDEX must be between 0 and 8
#endif

#if (RX_DESC_RING_INDEX > 8) || (RX_DESC_RING_INDEX < 0)
#error RX_DESC_RING_INDEX must be between 0 and 8
#endif

#if (RX_COMP_RING_INDEX > 8) || (RX_COMP_RING_INDEX < 0)
#error RX_COMP_RING_INDEX must be between 0 and 8
#endif

#define N_TX_RINGS                    MAX_TX_RINGS      
#define N_TX_RINGS_MASK               MAX_TX_RINGS_MASK
#define N_RX_DESC_RINGS               MAX_RX_DESC_RINGS 
#define N_RX_COMP_RINGS               0x1 

#define N_RX_FLOWS                    64

#define TX_DESC_RING_SIZE  DESC_RING_I_TO_S(TX_DESC_RING_INDEX)
#define RX_DESC_RING_SIZE  DESC_RING_I_TO_S(RX_DESC_RING_INDEX)
#define RX_COMP_RING_SIZE  COMP_RING_I_TO_S(RX_COMP_RING_INDEX)
#define TX_DESC_RINGN_INDEX(x) TX_DESC_RING_INDEX
#define RX_DESC_RINGN_INDEX(x) RX_DESC_RING_INDEX
#define RX_COMP_RINGN_INDEX(x) RX_COMP_RING_INDEX
#define TX_DESC_RINGN_SIZE(x)  TX_DESC_RING_SIZE
#define RX_DESC_RINGN_SIZE(x)  RX_DESC_RING_SIZE
#define RX_COMP_RINGN_SIZE(x)  RX_COMP_RING_SIZE

#define CAS_BASE(x, y)                (((y) << (x ## _SHIFT)) & (x ## _MASK))
#define CAS_VAL(x, y)                 (((y) & (x ## _MASK)) >> (x ## _SHIFT))
#define CAS_TX_RINGN_BASE(y)          ((TX_DESC_RINGN_INDEX(y) << \
                                        TX_CFG_DESC_RINGN_SHIFT(y)) & \
                                        TX_CFG_DESC_RINGN_MASK(y))

#define CAS_MIN_PAGE_SHIFT            11 
#define CAS_JUMBO_PAGE_SHIFT          13 
#define CAS_MAX_PAGE_SHIFT            14 

#define TX_DESC_BUFLEN_MASK         0x0000000000003FFFULL 
#define TX_DESC_BUFLEN_SHIFT        0
#define TX_DESC_CSUM_START_MASK     0x00000000001F8000ULL 
#define TX_DESC_CSUM_START_SHIFT    15
#define TX_DESC_CSUM_STUFF_MASK     0x000000001FE00000ULL 
#define TX_DESC_CSUM_STUFF_SHIFT    21
#define TX_DESC_CSUM_EN             0x0000000020000000ULL 
#define TX_DESC_EOF                 0x0000000040000000ULL 
#define TX_DESC_SOF                 0x0000000080000000ULL 
#define TX_DESC_INTME               0x0000000100000000ULL 
#define TX_DESC_NO_CRC              0x0000000200000000ULL 
struct cas_tx_desc {
	__le64     control;
	__le64     buffer;
};

struct cas_rx_desc {
	__le64     index;
	__le64     buffer;
};

#define RX_COMP1_DATA_SIZE_MASK           0x0000000007FFE000ULL
#define RX_COMP1_DATA_SIZE_SHIFT          13
#define RX_COMP1_DATA_OFF_MASK            0x000001FFF8000000ULL
#define RX_COMP1_DATA_OFF_SHIFT           27
#define RX_COMP1_DATA_INDEX_MASK          0x007FFE0000000000ULL
#define RX_COMP1_DATA_INDEX_SHIFT         41
#define RX_COMP1_SKIP_MASK                0x0180000000000000ULL
#define RX_COMP1_SKIP_SHIFT               55
#define RX_COMP1_RELEASE_NEXT             0x0200000000000000ULL
#define RX_COMP1_SPLIT_PKT                0x0400000000000000ULL
#define RX_COMP1_RELEASE_FLOW             0x0800000000000000ULL
#define RX_COMP1_RELEASE_DATA             0x1000000000000000ULL
#define RX_COMP1_RELEASE_HDR              0x2000000000000000ULL
#define RX_COMP1_TYPE_MASK                0xC000000000000000ULL
#define RX_COMP1_TYPE_SHIFT               62

#define RX_COMP2_NEXT_INDEX_MASK          0x00000007FFE00000ULL
#define RX_COMP2_NEXT_INDEX_SHIFT         21
#define RX_COMP2_HDR_SIZE_MASK            0x00000FF800000000ULL
#define RX_COMP2_HDR_SIZE_SHIFT           35
#define RX_COMP2_HDR_OFF_MASK             0x0003F00000000000ULL
#define RX_COMP2_HDR_OFF_SHIFT            44
#define RX_COMP2_HDR_INDEX_MASK           0xFFFC000000000000ULL
#define RX_COMP2_HDR_INDEX_SHIFT          50

#define RX_COMP3_SMALL_PKT                0x0000000000000001ULL
#define RX_COMP3_JUMBO_PKT                0x0000000000000002ULL
#define RX_COMP3_JUMBO_HDR_SPLIT_EN       0x0000000000000004ULL
#define RX_COMP3_CSUM_START_MASK          0x000000000007F000ULL
#define RX_COMP3_CSUM_START_SHIFT         12
#define RX_COMP3_FLOWID_MASK              0x0000000001F80000ULL
#define RX_COMP3_FLOWID_SHIFT             19
#define RX_COMP3_OPCODE_MASK              0x000000000E000000ULL
#define RX_COMP3_OPCODE_SHIFT             25
#define RX_COMP3_FORCE_FLAG               0x0000000010000000ULL
#define RX_COMP3_NO_ASSIST                0x0000000020000000ULL
#define RX_COMP3_LOAD_BAL_MASK            0x000001F800000000ULL
#define RX_COMP3_LOAD_BAL_SHIFT           35
#define RX_PLUS_COMP3_ENC_PKT             0x0000020000000000ULL 
#define RX_COMP3_L3_HEAD_OFF_MASK         0x0000FE0000000000ULL 
#define RX_COMP3_L3_HEAD_OFF_SHIFT        41
#define RX_PLUS_COMP_L3_HEAD_OFF_MASK     0x0000FC0000000000ULL 
#define RX_PLUS_COMP_L3_HEAD_OFF_SHIFT    42
#define RX_COMP3_SAP_MASK                 0xFFFF000000000000ULL
#define RX_COMP3_SAP_SHIFT                48

#define RX_COMP4_TCP_CSUM_MASK            0x000000000000FFFFULL
#define RX_COMP4_TCP_CSUM_SHIFT           0
#define RX_COMP4_PKT_LEN_MASK             0x000000003FFF0000ULL
#define RX_COMP4_PKT_LEN_SHIFT            16
#define RX_COMP4_PERFECT_MATCH_MASK       0x00000003C0000000ULL
#define RX_COMP4_PERFECT_MATCH_SHIFT      30
#define RX_COMP4_ZERO                     0x0000080000000000ULL
#define RX_COMP4_HASH_VAL_MASK            0x0FFFF00000000000ULL
#define RX_COMP4_HASH_VAL_SHIFT           44
#define RX_COMP4_HASH_PASS                0x1000000000000000ULL
#define RX_COMP4_BAD                      0x4000000000000000ULL
#define RX_COMP4_LEN_MISMATCH             0x8000000000000000ULL

#define RX_INDEX_NUM_MASK                 0x0000000000000FFFULL
#define RX_INDEX_NUM_SHIFT                0
#define RX_INDEX_RING_MASK                0x0000000000001000ULL
#define RX_INDEX_RING_SHIFT               12
#define RX_INDEX_RELEASE                  0x0000000000002000ULL

struct cas_rx_comp {
	__le64     word1;
	__le64     word2;
	__le64     word3;
	__le64     word4;
};

enum link_state {
	link_down = 0,	
	link_aneg,	
	link_force_try,	
	link_force_ret,	
	link_force_ok,	
	link_up		
};

typedef struct cas_page {
	struct list_head list;
	struct page *buffer;
	dma_addr_t dma_addr;
	int used;
} cas_page_t;


#define INIT_BLOCK_TX           (TX_DESC_RING_SIZE)
#define INIT_BLOCK_RX_DESC      (RX_DESC_RING_SIZE)
#define INIT_BLOCK_RX_COMP      (RX_COMP_RING_SIZE)

struct cas_init_block {
	struct cas_rx_comp rxcs[N_RX_COMP_RINGS][INIT_BLOCK_RX_COMP];
	struct cas_rx_desc rxds[N_RX_DESC_RINGS][INIT_BLOCK_RX_DESC];
	struct cas_tx_desc txds[N_TX_RINGS][INIT_BLOCK_TX];
	__le64 tx_compwb;
};

#define TX_TINY_BUF_LEN    0x100
#define TX_TINY_BUF_BLOCK  ((INIT_BLOCK_TX + 1)*TX_TINY_BUF_LEN)

struct cas_tiny_count {
	int nbufs;
	int used;
};

struct cas {
	spinlock_t lock; 
	spinlock_t tx_lock[N_TX_RINGS]; 
	spinlock_t stat_lock[N_TX_RINGS + 1]; 
	spinlock_t rx_inuse_lock; 
	spinlock_t rx_spare_lock; 

	void __iomem *regs;
	int tx_new[N_TX_RINGS], tx_old[N_TX_RINGS];
	int rx_old[N_RX_DESC_RINGS];
	int rx_cur[N_RX_COMP_RINGS], rx_new[N_RX_COMP_RINGS];
	int rx_last[N_RX_DESC_RINGS];

	struct napi_struct napi;

	int hw_running;
	int opened;
	struct mutex pm_mutex; 

	struct cas_init_block *init_block;
	struct cas_tx_desc *init_txds[MAX_TX_RINGS];
	struct cas_rx_desc *init_rxds[MAX_RX_DESC_RINGS];
	struct cas_rx_comp *init_rxcs[MAX_RX_COMP_RINGS];

	struct sk_buff      *tx_skbs[N_TX_RINGS][TX_DESC_RING_SIZE];
	struct sk_buff_head  rx_flows[N_RX_FLOWS];
	cas_page_t          *rx_pages[N_RX_DESC_RINGS][RX_DESC_RING_SIZE];
	struct list_head     rx_spare_list, rx_inuse_list;
	int                  rx_spares_needed;

	struct cas_tiny_count tx_tiny_use[N_TX_RINGS][TX_DESC_RING_SIZE];
	u8 *tx_tiny_bufs[N_TX_RINGS];

	u32			msg_enable;

	
	struct net_device_stats net_stats[N_TX_RINGS + 1];

	u32			pci_cfg[64 >> 2];
	u8                      pci_revision;

	int                     phy_type;
	int			phy_addr;
	u32                     phy_id;
#define CAS_FLAG_1000MB_CAP     0x00000001
#define CAS_FLAG_REG_PLUS       0x00000002
#define CAS_FLAG_TARGET_ABORT   0x00000004
#define CAS_FLAG_SATURN         0x00000008
#define CAS_FLAG_RXD_POST_MASK  0x000000F0
#define CAS_FLAG_RXD_POST_SHIFT 4
#define CAS_FLAG_RXD_POST(x)    ((1 << (CAS_FLAG_RXD_POST_SHIFT + (x))) & \
                                 CAS_FLAG_RXD_POST_MASK)
#define CAS_FLAG_ENTROPY_DEV    0x00000100
#define CAS_FLAG_NO_HW_CSUM     0x00000200
	u32                     cas_flags;
	int                     packet_min; 
	int			tx_fifo_size;
	int			rx_fifo_size;
	int			rx_pause_off;
	int			rx_pause_on;
	int                     crc_size;      

	int                     pci_irq_INTC;
	int                     min_frame_size; 

	
	int                     page_size;
	int                     page_order;
	int                     mtu_stride;

	u32			mac_rx_cfg;

	
	int			link_cntl;
	int			link_fcntl;
	enum link_state		lstate;
	struct timer_list	link_timer;
	int			timer_ticks;
	struct work_struct	reset_task;
#if 0
	atomic_t		reset_task_pending;
#else
	atomic_t		reset_task_pending;
	atomic_t		reset_task_pending_mtu;
	atomic_t		reset_task_pending_spare;
	atomic_t		reset_task_pending_all;
#endif

	
#define LINK_TRANSITION_UNKNOWN 	0
#define LINK_TRANSITION_ON_FAILURE 	1
#define LINK_TRANSITION_STILL_FAILED 	2
#define LINK_TRANSITION_LINK_UP 	3
#define LINK_TRANSITION_LINK_CONFIG	4
#define LINK_TRANSITION_LINK_DOWN	5
#define LINK_TRANSITION_REQUESTED_RESET	6
	int			link_transition;
	int 			link_transition_jiffies_valid;
	unsigned long		link_transition_jiffies;

	
	u8 orig_cacheline_size;	
#define CAS_PREF_CACHELINE_SIZE	 0x20	

	
	int 			casreg_len; 
	u64			pause_entered;
	u16			pause_last_time_recvd;

	dma_addr_t block_dvma, tx_tiny_dvma[N_TX_RINGS];
	struct pci_dev *pdev;
	struct net_device *dev;
#if defined(CONFIG_OF)
	struct device_node	*of_node;
#endif

	
	u16			fw_load_addr;
	u32			fw_size;
	u8			*fw_data;
};

#define TX_DESC_NEXT(r, x)  (((x) + 1) & (TX_DESC_RINGN_SIZE(r) - 1))
#define RX_DESC_ENTRY(r, x) ((x) & (RX_DESC_RINGN_SIZE(r) - 1))
#define RX_COMP_ENTRY(r, x) ((x) & (RX_COMP_RINGN_SIZE(r) - 1))

#define TX_BUFF_COUNT(r, x, y)    ((x) <= (y) ? ((y) - (x)) : \
        (TX_DESC_RINGN_SIZE(r) - (x) + (y)))

#define TX_BUFFS_AVAIL(cp, i)	((cp)->tx_old[(i)] <= (cp)->tx_new[(i)] ? \
        (cp)->tx_old[(i)] + (TX_DESC_RINGN_SIZE(i) - 1) - (cp)->tx_new[(i)] : \
        (cp)->tx_old[(i)] - (cp)->tx_new[(i)] - 1)

#define CAS_ALIGN(addr, align) \
     (((unsigned long) (addr) + ((align) - 1UL)) & ~((align) - 1))

#define RX_FIFO_SIZE                  16384
#define EXPANSION_ROM_SIZE            65536

#define CAS_MC_EXACT_MATCH_SIZE       15
#define CAS_MC_HASH_SIZE              256
#define CAS_MC_HASH_MAX              (CAS_MC_EXACT_MATCH_SIZE + \
                                      CAS_MC_HASH_SIZE)

#define TX_TARGET_ABORT_LEN           0x20
#define RX_SWIVEL_OFF_VAL             0x2
#define RX_AE_FREEN_VAL(x)            (RX_DESC_RINGN_SIZE(x) >> 1)
#define RX_AE_COMP_VAL                (RX_COMP_RING_SIZE >> 1)
#define RX_BLANK_INTR_PKT_VAL         0x05
#define RX_BLANK_INTR_TIME_VAL        0x0F
#define HP_TCP_THRESH_VAL             1530 

#define RX_SPARE_COUNT                (RX_DESC_RING_SIZE >> 1)
#define RX_SPARE_RECOVER_VAL          (RX_SPARE_COUNT >> 2)

#endif 
