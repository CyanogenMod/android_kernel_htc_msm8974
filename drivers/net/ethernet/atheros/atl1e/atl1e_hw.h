/*
 * Copyright(c) 2007 Atheros Corporation. All rights reserved.
 *
 * Derived from Intel e1000 driver
 * Copyright(c) 1999 - 2005 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _ATHL1E_HW_H_
#define _ATHL1E_HW_H_

#include <linux/types.h>
#include <linux/mii.h>

struct atl1e_adapter;
struct atl1e_hw;

s32 atl1e_reset_hw(struct atl1e_hw *hw);
s32 atl1e_read_mac_addr(struct atl1e_hw *hw);
s32 atl1e_init_hw(struct atl1e_hw *hw);
s32 atl1e_phy_commit(struct atl1e_hw *hw);
s32 atl1e_get_speed_and_duplex(struct atl1e_hw *hw, u16 *speed, u16 *duplex);
u32 atl1e_auto_get_fc(struct atl1e_adapter *adapter, u16 duplex);
u32 atl1e_hash_mc_addr(struct atl1e_hw *hw, u8 *mc_addr);
void atl1e_hash_set(struct atl1e_hw *hw, u32 hash_value);
s32 atl1e_read_phy_reg(struct atl1e_hw *hw, u16 reg_addr, u16 *phy_data);
s32 atl1e_write_phy_reg(struct atl1e_hw *hw, u32 reg_addr, u16 phy_data);
s32 atl1e_validate_mdi_setting(struct atl1e_hw *hw);
void atl1e_hw_set_mac_addr(struct atl1e_hw *hw);
bool atl1e_read_eeprom(struct atl1e_hw *hw, u32 offset, u32 *p_value);
bool atl1e_write_eeprom(struct atl1e_hw *hw, u32 offset, u32 value);
s32 atl1e_phy_enter_power_saving(struct atl1e_hw *hw);
s32 atl1e_phy_leave_power_saving(struct atl1e_hw *hw);
s32 atl1e_phy_init(struct atl1e_hw *hw);
int atl1e_check_eeprom_exist(struct atl1e_hw *hw);
void atl1e_force_ps(struct atl1e_hw *hw);
s32 atl1e_restart_autoneg(struct atl1e_hw *hw);

#define REG_PM_CTRLSTAT             0x44

#define REG_PCIE_CAP_LIST           0x58

#define REG_DEVICE_CAP              0x5C
#define     DEVICE_CAP_MAX_PAYLOAD_MASK     0x7
#define     DEVICE_CAP_MAX_PAYLOAD_SHIFT    0

#define REG_DEVICE_CTRL             0x60
#define     DEVICE_CTRL_MAX_PAYLOAD_MASK    0x7
#define     DEVICE_CTRL_MAX_PAYLOAD_SHIFT   5
#define     DEVICE_CTRL_MAX_RREQ_SZ_MASK    0x7
#define     DEVICE_CTRL_MAX_RREQ_SZ_SHIFT   12

#define REG_VPD_CAP                 0x6C
#define     VPD_CAP_ID_MASK                 0xff
#define     VPD_CAP_ID_SHIFT                0
#define     VPD_CAP_NEXT_PTR_MASK           0xFF
#define     VPD_CAP_NEXT_PTR_SHIFT          8
#define     VPD_CAP_VPD_ADDR_MASK           0x7FFF
#define     VPD_CAP_VPD_ADDR_SHIFT          16
#define     VPD_CAP_VPD_FLAG                0x80000000

#define REG_VPD_DATA                0x70

#define REG_SPI_FLASH_CTRL          0x200
#define     SPI_FLASH_CTRL_STS_NON_RDY      0x1
#define     SPI_FLASH_CTRL_STS_WEN          0x2
#define     SPI_FLASH_CTRL_STS_WPEN         0x80
#define     SPI_FLASH_CTRL_DEV_STS_MASK     0xFF
#define     SPI_FLASH_CTRL_DEV_STS_SHIFT    0
#define     SPI_FLASH_CTRL_INS_MASK         0x7
#define     SPI_FLASH_CTRL_INS_SHIFT        8
#define     SPI_FLASH_CTRL_START            0x800
#define     SPI_FLASH_CTRL_EN_VPD           0x2000
#define     SPI_FLASH_CTRL_LDSTART          0x8000
#define     SPI_FLASH_CTRL_CS_HI_MASK       0x3
#define     SPI_FLASH_CTRL_CS_HI_SHIFT      16
#define     SPI_FLASH_CTRL_CS_HOLD_MASK     0x3
#define     SPI_FLASH_CTRL_CS_HOLD_SHIFT    18
#define     SPI_FLASH_CTRL_CLK_LO_MASK      0x3
#define     SPI_FLASH_CTRL_CLK_LO_SHIFT     20
#define     SPI_FLASH_CTRL_CLK_HI_MASK      0x3
#define     SPI_FLASH_CTRL_CLK_HI_SHIFT     22
#define     SPI_FLASH_CTRL_CS_SETUP_MASK    0x3
#define     SPI_FLASH_CTRL_CS_SETUP_SHIFT   24
#define     SPI_FLASH_CTRL_EROM_PGSZ_MASK   0x3
#define     SPI_FLASH_CTRL_EROM_PGSZ_SHIFT  26
#define     SPI_FLASH_CTRL_WAIT_READY       0x10000000

#define REG_SPI_ADDR                0x204

#define REG_SPI_DATA                0x208

#define REG_SPI_FLASH_CONFIG        0x20C
#define     SPI_FLASH_CONFIG_LD_ADDR_MASK   0xFFFFFF
#define     SPI_FLASH_CONFIG_LD_ADDR_SHIFT  0
#define     SPI_FLASH_CONFIG_VPD_ADDR_MASK  0x3
#define     SPI_FLASH_CONFIG_VPD_ADDR_SHIFT 24
#define     SPI_FLASH_CONFIG_LD_EXIST       0x4000000


#define REG_SPI_FLASH_OP_PROGRAM    0x210
#define REG_SPI_FLASH_OP_SC_ERASE   0x211
#define REG_SPI_FLASH_OP_CHIP_ERASE 0x212
#define REG_SPI_FLASH_OP_RDID       0x213
#define REG_SPI_FLASH_OP_WREN       0x214
#define REG_SPI_FLASH_OP_RDSR       0x215
#define REG_SPI_FLASH_OP_WRSR       0x216
#define REG_SPI_FLASH_OP_READ       0x217

#define REG_TWSI_CTRL               0x218
#define     TWSI_CTRL_LD_OFFSET_MASK        0xFF
#define     TWSI_CTRL_LD_OFFSET_SHIFT       0
#define     TWSI_CTRL_LD_SLV_ADDR_MASK      0x7
#define     TWSI_CTRL_LD_SLV_ADDR_SHIFT     8
#define     TWSI_CTRL_SW_LDSTART            0x800
#define     TWSI_CTRL_HW_LDSTART            0x1000
#define     TWSI_CTRL_SMB_SLV_ADDR_MASK     0x0x7F
#define     TWSI_CTRL_SMB_SLV_ADDR_SHIFT    15
#define     TWSI_CTRL_LD_EXIST              0x400000
#define     TWSI_CTRL_READ_FREQ_SEL_MASK    0x3
#define     TWSI_CTRL_READ_FREQ_SEL_SHIFT   23
#define     TWSI_CTRL_FREQ_SEL_100K         0
#define     TWSI_CTRL_FREQ_SEL_200K         1
#define     TWSI_CTRL_FREQ_SEL_300K         2
#define     TWSI_CTRL_FREQ_SEL_400K         3
#define     TWSI_CTRL_SMB_SLV_ADDR
#define     TWSI_CTRL_WRITE_FREQ_SEL_MASK   0x3
#define     TWSI_CTRL_WRITE_FREQ_SEL_SHIFT  24


#define REG_PCIE_DEV_MISC_CTRL      0x21C
#define     PCIE_DEV_MISC_CTRL_EXT_PIPE     0x2
#define     PCIE_DEV_MISC_CTRL_RETRY_BUFDIS 0x1
#define     PCIE_DEV_MISC_CTRL_SPIROM_EXIST 0x4
#define     PCIE_DEV_MISC_CTRL_SERDES_ENDIAN    0x8
#define     PCIE_DEV_MISC_CTRL_SERDES_SEL_DIN   0x10

#define REG_PCIE_PHYMISC	    0x1000
#define PCIE_PHYMISC_FORCE_RCV_DET	0x4

#define REG_LTSSM_TEST_MODE         0x12FC
#define         LTSSM_TEST_MODE_DEF     0xE000

#define REG_MASTER_CTRL             0x1400
#define     MASTER_CTRL_SOFT_RST            0x1
#define     MASTER_CTRL_MTIMER_EN           0x2
#define     MASTER_CTRL_ITIMER_EN           0x4
#define     MASTER_CTRL_MANUAL_INT          0x8
#define     MASTER_CTRL_ITIMER2_EN          0x20
#define     MASTER_CTRL_INT_RDCLR           0x40
#define     MASTER_CTRL_LED_MODE	    0x200
#define     MASTER_CTRL_REV_NUM_SHIFT       16
#define     MASTER_CTRL_REV_NUM_MASK        0xff
#define     MASTER_CTRL_DEV_ID_SHIFT        24
#define     MASTER_CTRL_DEV_ID_MASK         0xff

#define REG_MANUAL_TIMER_INIT       0x1404


#define REG_IRQ_MODU_TIMER_INIT     0x1408   
#define REG_IRQ_MODU_TIMER2_INIT    0x140A   


#define REG_GPHY_CTRL               0x140C
#define     GPHY_CTRL_EXT_RESET         1
#define     GPHY_CTRL_PIPE_MOD          2
#define     GPHY_CTRL_TEST_MODE_MASK    3
#define     GPHY_CTRL_TEST_MODE_SHIFT   2
#define     GPHY_CTRL_BERT_START        0x10
#define     GPHY_CTRL_GATE_25M_EN       0x20
#define     GPHY_CTRL_LPW_EXIT          0x40
#define     GPHY_CTRL_PHY_IDDQ          0x80
#define     GPHY_CTRL_PHY_IDDQ_DIS      0x100
#define     GPHY_CTRL_PCLK_SEL_DIS      0x200
#define     GPHY_CTRL_HIB_EN            0x400
#define     GPHY_CTRL_HIB_PULSE         0x800
#define     GPHY_CTRL_SEL_ANA_RST       0x1000
#define     GPHY_CTRL_PHY_PLL_ON        0x2000
#define     GPHY_CTRL_PWDOWN_HW		0x4000
#define     GPHY_CTRL_DEFAULT (\
		GPHY_CTRL_PHY_PLL_ON	|\
		GPHY_CTRL_SEL_ANA_RST	|\
		GPHY_CTRL_HIB_PULSE	|\
		GPHY_CTRL_HIB_EN)

#define     GPHY_CTRL_PW_WOL_DIS (\
		GPHY_CTRL_PHY_PLL_ON	|\
		GPHY_CTRL_SEL_ANA_RST	|\
		GPHY_CTRL_HIB_PULSE	|\
		GPHY_CTRL_HIB_EN	|\
		GPHY_CTRL_PWDOWN_HW	|\
		GPHY_CTRL_PCLK_SEL_DIS	|\
		GPHY_CTRL_PHY_IDDQ)

#define REG_CMBDISDMA_TIMER         0x140E


#define REG_IDLE_STATUS  	0x1410
#define     IDLE_STATUS_RXMAC       1    
#define     IDLE_STATUS_TXMAC       2    
#define     IDLE_STATUS_RXQ         4    
#define     IDLE_STATUS_TXQ         8    
#define     IDLE_STATUS_DMAR        0x10 
#define     IDLE_STATUS_DMAW        0x20 
#define     IDLE_STATUS_SMB         0x40 
#define     IDLE_STATUS_CMB         0x80 

#define REG_MDIO_CTRL           0x1414
#define     MDIO_DATA_MASK          0xffff  
#define     MDIO_DATA_SHIFT         0       
#define     MDIO_REG_ADDR_MASK      0x1f    
#define     MDIO_REG_ADDR_SHIFT     16
#define     MDIO_RW                 0x200000      
#define     MDIO_SUP_PREAMBLE       0x400000      
#define     MDIO_START              0x800000      
#define     MDIO_CLK_SEL_SHIFT      24
#define     MDIO_CLK_25_4           0
#define     MDIO_CLK_25_6           2
#define     MDIO_CLK_25_8           3
#define     MDIO_CLK_25_10          4
#define     MDIO_CLK_25_14          5
#define     MDIO_CLK_25_20          6
#define     MDIO_CLK_25_28          7
#define     MDIO_BUSY               0x8000000
#define     MDIO_AP_EN              0x10000000
#define MDIO_WAIT_TIMES         10

#define REG_PHY_STATUS           0x1418
#define     PHY_STATUS_100M	      0x20000
#define     PHY_STATUS_EMI_CA	      0x40000

#define REG_BIST0_CTRL              0x141c
#define     BIST0_NOW                   0x1 
#define     BIST0_SRAM_FAIL             0x2 
#define     BIST0_FUSE_FLAG             0x4 

#define REG_BIST1_CTRL              0x1420
#define     BIST1_NOW                   0x1 
#define     BIST1_SRAM_FAIL             0x2 
#define     BIST1_FUSE_FLAG             0x4

#define REG_SERDES_LOCK             0x1424
#define     SERDES_LOCK_DETECT          1  
#define     SERDES_LOCK_DETECT_EN       2  

#define REG_MAC_CTRL                0x1480
#define     MAC_CTRL_TX_EN              1  
#define     MAC_CTRL_RX_EN              2  
#define     MAC_CTRL_TX_FLOW            4  
#define     MAC_CTRL_RX_FLOW            8  
#define     MAC_CTRL_LOOPBACK           0x10      
#define     MAC_CTRL_DUPLX              0x20      
#define     MAC_CTRL_ADD_CRC            0x40      
#define     MAC_CTRL_PAD                0x80      
#define     MAC_CTRL_LENCHK             0x100     
#define     MAC_CTRL_HUGE_EN            0x200     
#define     MAC_CTRL_PRMLEN_SHIFT       10        
#define     MAC_CTRL_PRMLEN_MASK        0xf
#define     MAC_CTRL_RMV_VLAN           0x4000    
#define     MAC_CTRL_PROMIS_EN          0x8000    
#define     MAC_CTRL_TX_PAUSE           0x10000   
#define     MAC_CTRL_SCNT               0x20000   
#define     MAC_CTRL_SRST_TX            0x40000   
#define     MAC_CTRL_TX_SIMURST         0x80000   
#define     MAC_CTRL_SPEED_SHIFT        20        
#define     MAC_CTRL_SPEED_MASK         0x300000
#define     MAC_CTRL_SPEED_1000         2
#define     MAC_CTRL_SPEED_10_100       1
#define     MAC_CTRL_DBG_TX_BKPRESURE   0x400000  
#define     MAC_CTRL_TX_HUGE            0x800000  
#define     MAC_CTRL_RX_CHKSUM_EN       0x1000000 
#define     MAC_CTRL_MC_ALL_EN          0x2000000 
#define     MAC_CTRL_BC_EN              0x4000000 
#define     MAC_CTRL_DBG                0x8000000 

#define REG_MAC_IPG_IFG             0x1484
#define     MAC_IPG_IFG_IPGT_SHIFT      0     
#define     MAC_IPG_IFG_IPGT_MASK       0x7f
#define     MAC_IPG_IFG_MIFG_SHIFT      8     
#define     MAC_IPG_IFG_MIFG_MASK       0xff  
#define     MAC_IPG_IFG_IPGR1_SHIFT     16    
#define     MAC_IPG_IFG_IPGR1_MASK      0x7f
#define     MAC_IPG_IFG_IPGR2_SHIFT     24    
#define     MAC_IPG_IFG_IPGR2_MASK      0x7f

#define REG_MAC_STA_ADDR            0x1488

#define REG_RX_HASH_TABLE           0x1490


#define REG_MAC_HALF_DUPLX_CTRL     0x1498
#define     MAC_HALF_DUPLX_CTRL_LCOL_SHIFT   0      
#define     MAC_HALF_DUPLX_CTRL_LCOL_MASK    0x3ff
#define     MAC_HALF_DUPLX_CTRL_RETRY_SHIFT  12     
#define     MAC_HALF_DUPLX_CTRL_RETRY_MASK   0xf
#define     MAC_HALF_DUPLX_CTRL_EXC_DEF_EN   0x10000 
#define     MAC_HALF_DUPLX_CTRL_NO_BACK_C    0x20000 
#define     MAC_HALF_DUPLX_CTRL_NO_BACK_P    0x40000 
#define     MAC_HALF_DUPLX_CTRL_ABEBE        0x80000 
#define     MAC_HALF_DUPLX_CTRL_ABEBT_SHIFT  20      
#define     MAC_HALF_DUPLX_CTRL_ABEBT_MASK   0xf
#define     MAC_HALF_DUPLX_CTRL_JAMIPG_SHIFT 24      
#define     MAC_HALF_DUPLX_CTRL_JAMIPG_MASK  0xf     

#define REG_MTU                     0x149c

#define REG_WOL_CTRL                0x14a0
#define     WOL_PATTERN_EN                  0x00000001
#define     WOL_PATTERN_PME_EN              0x00000002
#define     WOL_MAGIC_EN                    0x00000004
#define     WOL_MAGIC_PME_EN                0x00000008
#define     WOL_LINK_CHG_EN                 0x00000010
#define     WOL_LINK_CHG_PME_EN             0x00000020
#define     WOL_PATTERN_ST                  0x00000100
#define     WOL_MAGIC_ST                    0x00000200
#define     WOL_LINKCHG_ST                  0x00000400
#define     WOL_CLK_SWITCH_EN               0x00008000
#define     WOL_PT0_EN                      0x00010000
#define     WOL_PT1_EN                      0x00020000
#define     WOL_PT2_EN                      0x00040000
#define     WOL_PT3_EN                      0x00080000
#define     WOL_PT4_EN                      0x00100000
#define     WOL_PT5_EN                      0x00200000
#define     WOL_PT6_EN                      0x00400000
#define REG_WOL_PATTERN_LEN         0x14a4
#define     WOL_PT_LEN_MASK                 0x7f
#define     WOL_PT0_LEN_SHIFT               0
#define     WOL_PT1_LEN_SHIFT               8
#define     WOL_PT2_LEN_SHIFT               16
#define     WOL_PT3_LEN_SHIFT               24
#define     WOL_PT4_LEN_SHIFT               0
#define     WOL_PT5_LEN_SHIFT               8
#define     WOL_PT6_LEN_SHIFT               16

#define REG_SRAM_TRD_ADDR           0x1518
#define REG_SRAM_TRD_LEN            0x151C
#define REG_SRAM_RXF_ADDR           0x1520
#define REG_SRAM_RXF_LEN            0x1524
#define REG_SRAM_TXF_ADDR           0x1528
#define REG_SRAM_TXF_LEN            0x152C
#define REG_SRAM_TCPH_ADDR          0x1530
#define REG_SRAM_PKTH_ADDR          0x1532

#define REG_LOAD_PTR                0x1534  


#define REG_RXF3_BASE_ADDR_HI           0x153C
#define REG_DESC_BASE_ADDR_HI           0x1540
#define REG_RXF0_BASE_ADDR_HI           0x1540 
#define REG_HOST_RXF0_PAGE0_LO          0x1544
#define REG_HOST_RXF0_PAGE1_LO          0x1548
#define REG_TPD_BASE_ADDR_LO            0x154C
#define REG_RXF1_BASE_ADDR_HI           0x1550
#define REG_RXF2_BASE_ADDR_HI           0x1554
#define REG_HOST_RXFPAGE_SIZE           0x1558
#define REG_TPD_RING_SIZE               0x155C
#define REG_RSS_KEY0                    0x14B0
#define REG_RSS_KEY1                    0x14B4
#define REG_RSS_KEY2                    0x14B8
#define REG_RSS_KEY3                    0x14BC
#define REG_RSS_KEY4                    0x14C0
#define REG_RSS_KEY5                    0x14C4
#define REG_RSS_KEY6                    0x14C8
#define REG_RSS_KEY7                    0x14CC
#define REG_RSS_KEY8                    0x14D0
#define REG_RSS_KEY9                    0x14D4
#define REG_IDT_TABLE4                  0x14E0
#define REG_IDT_TABLE5                  0x14E4
#define REG_IDT_TABLE6                  0x14E8
#define REG_IDT_TABLE7                  0x14EC
#define REG_IDT_TABLE0                  0x1560
#define REG_IDT_TABLE1                  0x1564
#define REG_IDT_TABLE2                  0x1568
#define REG_IDT_TABLE3                  0x156C
#define REG_IDT_TABLE                   REG_IDT_TABLE0
#define REG_RSS_HASH_VALUE              0x1570
#define REG_RSS_HASH_FLAG               0x1574
#define REG_BASE_CPU_NUMBER             0x157C


#define REG_TXQ_CTRL                0x1580
#define     TXQ_CTRL_NUM_TPD_BURST_MASK     0xF
#define     TXQ_CTRL_NUM_TPD_BURST_SHIFT    0
#define     TXQ_CTRL_EN                     0x20  
#define     TXQ_CTRL_ENH_MODE               0x40  
#define     TXQ_CTRL_TXF_BURST_NUM_SHIFT    16    
#define     TXQ_CTRL_TXF_BURST_NUM_MASK     0xffff

#define REG_TX_EARLY_TH                     0x1584 
#define     TX_TX_EARLY_TH_MASK             0x7ff
#define     TX_TX_EARLY_TH_SHIFT            0


#define REG_RXQ_CTRL                0x15A0
#define         RXQ_CTRL_PBA_ALIGN_32                   0   
#define         RXQ_CTRL_PBA_ALIGN_64                   1
#define         RXQ_CTRL_PBA_ALIGN_128                  2
#define         RXQ_CTRL_PBA_ALIGN_256                  3
#define         RXQ_CTRL_Q1_EN				0x10
#define         RXQ_CTRL_Q2_EN				0x20
#define         RXQ_CTRL_Q3_EN				0x40
#define         RXQ_CTRL_IPV6_XSUM_VERIFY_EN		0x80
#define         RXQ_CTRL_HASH_TLEN_SHIFT                8
#define         RXQ_CTRL_HASH_TLEN_MASK                 0xFF
#define         RXQ_CTRL_HASH_TYPE_IPV4                 0x10000
#define         RXQ_CTRL_HASH_TYPE_IPV4_TCP             0x20000
#define         RXQ_CTRL_HASH_TYPE_IPV6                 0x40000
#define         RXQ_CTRL_HASH_TYPE_IPV6_TCP             0x80000
#define         RXQ_CTRL_RSS_MODE_DISABLE               0
#define         RXQ_CTRL_RSS_MODE_SQSINT                0x4000000
#define         RXQ_CTRL_RSS_MODE_MQUESINT              0x8000000
#define         RXQ_CTRL_RSS_MODE_MQUEMINT              0xC000000
#define         RXQ_CTRL_NIP_QUEUE_SEL_TBL              0x10000000
#define         RXQ_CTRL_HASH_ENABLE                    0x20000000
#define         RXQ_CTRL_CUT_THRU_EN                    0x40000000
#define         RXQ_CTRL_EN                             0x80000000

#define REG_RXQ_JMBOSZ_RRDTIM       0x15A4
#define         RXQ_JMBOSZ_TH_MASK      0x7ff
#define         RXQ_JMBOSZ_TH_SHIFT         0  
#define         RXQ_JMBO_LKAH_MASK          0xf
#define         RXQ_JMBO_LKAH_SHIFT         11

#define REG_RXQ_RXF_PAUSE_THRESH    0x15A8
#define     RXQ_RXF_PAUSE_TH_HI_SHIFT       0
#define     RXQ_RXF_PAUSE_TH_HI_MASK        0xfff
#define     RXQ_RXF_PAUSE_TH_LO_SHIFT       16
#define     RXQ_RXF_PAUSE_TH_LO_MASK        0xfff


#define REG_DMA_CTRL                0x15C0
#define     DMA_CTRL_DMAR_IN_ORDER          0x1
#define     DMA_CTRL_DMAR_ENH_ORDER         0x2
#define     DMA_CTRL_DMAR_OUT_ORDER         0x4
#define     DMA_CTRL_RCB_VALUE              0x8
#define     DMA_CTRL_DMAR_BURST_LEN_SHIFT   4
#define     DMA_CTRL_DMAR_BURST_LEN_MASK    7
#define     DMA_CTRL_DMAW_BURST_LEN_SHIFT   7
#define     DMA_CTRL_DMAW_BURST_LEN_MASK    7
#define     DMA_CTRL_DMAR_REQ_PRI           0x400
#define     DMA_CTRL_DMAR_DLY_CNT_MASK      0x1F
#define     DMA_CTRL_DMAR_DLY_CNT_SHIFT     11
#define     DMA_CTRL_DMAW_DLY_CNT_MASK      0xF
#define     DMA_CTRL_DMAW_DLY_CNT_SHIFT     16
#define     DMA_CTRL_TXCMB_EN               0x100000
#define     DMA_CTRL_RXCMB_EN				0x200000


#define REG_SMB_STAT_TIMER                      0x15C4
#define REG_TRIG_RRD_THRESH                     0x15CA
#define REG_TRIG_TPD_THRESH                     0x15C8
#define REG_TRIG_TXTIMER                        0x15CC
#define REG_TRIG_RXTIMER                        0x15CE

#define REG_HOST_RXF1_PAGE0_LO                  0x15D0
#define REG_HOST_RXF1_PAGE1_LO                  0x15D4
#define REG_HOST_RXF2_PAGE0_LO                  0x15D8
#define REG_HOST_RXF2_PAGE1_LO                  0x15DC
#define REG_HOST_RXF3_PAGE0_LO                  0x15E0
#define REG_HOST_RXF3_PAGE1_LO                  0x15E4

#define REG_MB_RXF1_RADDR                       0x15B4
#define REG_MB_RXF2_RADDR                       0x15B8
#define REG_MB_RXF3_RADDR                       0x15BC
#define REG_MB_TPD_PROD_IDX                     0x15F0

#define REG_HOST_RXF0_PAGE0_VLD     0x15F4
#define     HOST_RXF_VALID              1
#define     HOST_RXF_PAGENO_SHIFT       1
#define     HOST_RXF_PAGENO_MASK        0x7F
#define REG_HOST_RXF0_PAGE1_VLD     0x15F5
#define REG_HOST_RXF1_PAGE0_VLD     0x15F6
#define REG_HOST_RXF1_PAGE1_VLD     0x15F7
#define REG_HOST_RXF2_PAGE0_VLD     0x15F8
#define REG_HOST_RXF2_PAGE1_VLD     0x15F9
#define REG_HOST_RXF3_PAGE0_VLD     0x15FA
#define REG_HOST_RXF3_PAGE1_VLD     0x15FB

#define REG_ISR    0x1600
#define  ISR_SMB   		1
#define  ISR_TIMER		2       
#define  ISR_MANUAL         	4
#define  ISR_HW_RXF_OV          8        
#define  ISR_HOST_RXF0_OV       0x10
#define  ISR_HOST_RXF1_OV       0x20
#define  ISR_HOST_RXF2_OV       0x40
#define  ISR_HOST_RXF3_OV       0x80
#define  ISR_TXF_UN             0x100
#define  ISR_RX0_PAGE_FULL      0x200
#define  ISR_DMAR_TO_RST        0x400
#define  ISR_DMAW_TO_RST        0x800
#define  ISR_GPHY               0x1000
#define  ISR_TX_CREDIT          0x2000
#define  ISR_GPHY_LPW           0x4000    
#define  ISR_RX_PKT             0x10000   
#define  ISR_TX_PKT             0x20000   
#define  ISR_TX_DMA             0x40000
#define  ISR_RX_PKT_1           0x80000
#define  ISR_RX_PKT_2           0x100000
#define  ISR_RX_PKT_3           0x200000
#define  ISR_MAC_RX             0x400000
#define  ISR_MAC_TX             0x800000
#define  ISR_UR_DETECTED        0x1000000
#define  ISR_FERR_DETECTED      0x2000000
#define  ISR_NFERR_DETECTED     0x4000000
#define  ISR_CERR_DETECTED      0x8000000
#define  ISR_PHY_LINKDOWN       0x10000000
#define  ISR_DIS_INT            0x80000000


#define REG_IMR 0x1604


#define IMR_NORMAL_MASK (\
		ISR_SMB	        |\
		ISR_TXF_UN      |\
		ISR_HW_RXF_OV   |\
		ISR_HOST_RXF0_OV|\
		ISR_MANUAL      |\
		ISR_GPHY        |\
		ISR_GPHY_LPW    |\
		ISR_DMAR_TO_RST |\
		ISR_DMAW_TO_RST |\
		ISR_PHY_LINKDOWN|\
		ISR_RX_PKT      |\
		ISR_TX_PKT)

#define ISR_TX_EVENT (ISR_TXF_UN | ISR_TX_PKT)
#define ISR_RX_EVENT (ISR_HOST_RXF0_OV | ISR_HW_RXF_OV | ISR_RX_PKT)

#define REG_MAC_RX_STATUS_BIN 0x1700
#define REG_MAC_RX_STATUS_END 0x175c
#define REG_MAC_TX_STATUS_BIN 0x1760
#define REG_MAC_TX_STATUS_END 0x17c0

#define REG_HOST_RXF0_PAGEOFF 0x1800
#define REG_TPD_CONS_IDX      0x1804
#define REG_HOST_RXF1_PAGEOFF 0x1808
#define REG_HOST_RXF2_PAGEOFF 0x180C
#define REG_HOST_RXF3_PAGEOFF 0x1810

#define REG_HOST_RXF0_MB0_LO  0x1820
#define REG_HOST_RXF0_MB1_LO  0x1824
#define REG_HOST_RXF1_MB0_LO  0x1828
#define REG_HOST_RXF1_MB1_LO  0x182C
#define REG_HOST_RXF2_MB0_LO  0x1830
#define REG_HOST_RXF2_MB1_LO  0x1834
#define REG_HOST_RXF3_MB0_LO  0x1838
#define REG_HOST_RXF3_MB1_LO  0x183C

#define REG_HOST_TX_CMB_LO    0x1840
#define REG_HOST_SMB_ADDR_LO  0x1844

#define REG_DEBUG_DATA0 0x1900
#define REG_DEBUG_DATA1 0x1904

#define MII_AT001_PSCR                  0x10
#define MII_AT001_PSSR                  0x11
#define MII_INT_CTRL                    0x12
#define MII_INT_STATUS                  0x13
#define MII_SMARTSPEED                  0x14
#define MII_LBRERROR                    0x18
#define MII_RESV2                       0x1a

#define MII_DBG_ADDR			0x1D
#define MII_DBG_DATA			0x1E

#define MII_AR_DEFAULT_CAP_MASK                 0

#define MII_AT001_CR_1000T_SPEED_MASK \
	(ADVERTISE_1000FULL | ADVERTISE_1000HALF)
#define MII_AT001_CR_1000T_DEFAULT_CAP_MASK	MII_AT001_CR_1000T_SPEED_MASK

#define MII_AT001_PSCR_JABBER_DISABLE           0x0001  
#define MII_AT001_PSCR_POLARITY_REVERSAL        0x0002  
#define MII_AT001_PSCR_SQE_TEST                 0x0004  
#define MII_AT001_PSCR_MAC_POWERDOWN            0x0008
#define MII_AT001_PSCR_CLK125_DISABLE           0x0010  
#define MII_AT001_PSCR_MDI_MANUAL_MODE          0x0000  
#define MII_AT001_PSCR_MDIX_MANUAL_MODE         0x0020  
#define MII_AT001_PSCR_AUTO_X_1000T             0x0040  
#define MII_AT001_PSCR_AUTO_X_MODE              0x0060  
#define MII_AT001_PSCR_10BT_EXT_DIST_ENABLE     0x0080
#define MII_AT001_PSCR_MII_5BIT_ENABLE          0x0100
#define MII_AT001_PSCR_SCRAMBLER_DISABLE        0x0200  
#define MII_AT001_PSCR_FORCE_LINK_GOOD          0x0400  
#define MII_AT001_PSCR_ASSERT_CRS_ON_TX         0x0800  
#define MII_AT001_PSCR_POLARITY_REVERSAL_SHIFT    1
#define MII_AT001_PSCR_AUTO_X_MODE_SHIFT          5
#define MII_AT001_PSCR_10BT_EXT_DIST_ENABLE_SHIFT 7
#define MII_AT001_PSSR_SPD_DPLX_RESOLVED        0x0800  
#define MII_AT001_PSSR_DPLX                     0x2000  
#define MII_AT001_PSSR_SPEED                    0xC000  
#define MII_AT001_PSSR_10MBS                    0x0000  
#define MII_AT001_PSSR_100MBS                   0x4000  
#define MII_AT001_PSSR_1000MBS                  0x8000  

#endif 
