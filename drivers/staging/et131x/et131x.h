/*
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *   http://www.agere.com
 *
 * SOFTWARE LICENSE
 *
 * This software is provided subject to the following terms and conditions,
 * which you should read carefully before using the software.  Using this
 * software indicates your acceptance of these terms and conditions.  If you do
 * not agree with these terms and conditions, do not use the software.
 *
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *
 * Redistribution and use in source or binary forms, with or without
 * modifications, are permitted provided that the following conditions are met:
 *
 * . Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following Disclaimer as comments in the code as
 *    well as in the documentation and/or other materials provided with the
 *    distribution.
 *
 * . Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following Disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * . Neither the name of Agere Systems Inc. nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Disclaimer
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  ANY
 * USE, MODIFICATION OR DISTRIBUTION OF THIS SOFTWARE IS SOLELY AT THE USERS OWN
 * RISK. IN NO EVENT SHALL AGERE SYSTEMS INC. OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, INCLUDING, BUT NOT LIMITED TO, CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#define DRIVER_NAME "et131x"
#define DRIVER_VERSION "v2.0"


#define LBCIF_DWORD0_GROUP       0xAC
#define LBCIF_DWORD1_GROUP       0xB0

#define LBCIF_ADDRESS_REGISTER   0xAC
#define LBCIF_DATA_REGISTER      0xB0
#define LBCIF_CONTROL_REGISTER   0xB1
#define LBCIF_STATUS_REGISTER    0xB2

#define LBCIF_CONTROL_SEQUENTIAL_READ   0x01
#define LBCIF_CONTROL_PAGE_WRITE        0x02
#define LBCIF_CONTROL_EEPROM_RELOAD     0x08
#define LBCIF_CONTROL_TWO_BYTE_ADDR     0x20
#define LBCIF_CONTROL_I2C_WRITE         0x40
#define LBCIF_CONTROL_LBCIF_ENABLE      0x80

#define LBCIF_STATUS_PHY_QUEUE_AVAIL    0x01
#define LBCIF_STATUS_I2C_IDLE           0x02
#define LBCIF_STATUS_ACK_ERROR          0x04
#define LBCIF_STATUS_GENERAL_ERROR      0x08
#define LBCIF_STATUS_CHECKSUM_ERROR     0x40
#define LBCIF_STATUS_EEPROM_PRESENT     0x80




#define ET_PM_PHY_SW_COMA		0x40
#define ET_PMCSR_INIT			0x38


#define	ET_INTR_TXDMA_ISR	0x00000008
#define ET_INTR_TXDMA_ERR	0x00000010
#define ET_INTR_RXDMA_XFR_DONE	0x00000020
#define ET_INTR_RXDMA_FB_R0_LOW	0x00000040
#define ET_INTR_RXDMA_FB_R1_LOW	0x00000080
#define ET_INTR_RXDMA_STAT_LOW	0x00000100
#define ET_INTR_RXDMA_ERR	0x00000200
#define ET_INTR_WATCHDOG	0x00004000
#define ET_INTR_WOL		0x00008000
#define ET_INTR_PHY		0x00010000
#define ET_INTR_TXMAC		0x00020000
#define ET_INTR_RXMAC		0x00040000
#define ET_INTR_MAC_STAT	0x00080000
#define ET_INTR_SLV_TIMEOUT	0x00100000





#define ET_MSI_VECTOR	0x0000001F
#define ET_MSI_TC	0x00070000


#define ET_LOOP_MAC	0x00000001
#define ET_LOOP_DMA	0x00000002

struct global_regs {				
	u32 txq_start_addr;			
	u32 txq_end_addr;			
	u32 rxq_start_addr;			
	u32 rxq_end_addr;			
	u32 pm_csr;				
	u32 unused;				
	u32 int_status;				
	u32 int_mask;				
	u32 int_alias_clr_en;			
	u32 int_status_alias;			
	u32 sw_reset;				
	u32 slv_timer;				
	u32 msi_config;				
	u32 loopback;				
	u32 watchdog_timer;			
};




#define ET_TXDMA_CSR_HALT	0x00000001
#define ET_TXDMA_DROP_TLP	0x00000002
#define ET_TXDMA_CACHE_THRS	0x000000F0
#define ET_TXDMA_CACHE_SHIFT	4
#define ET_TXDMA_SNGL_EPKT	0x00000100
#define ET_TXDMA_CLASS		0x00001E00




#define ET_DMA12_MASK		0x0FFF	
#define ET_DMA12_WRAP		0x1000
#define ET_DMA10_MASK		0x03FF	
#define ET_DMA10_WRAP		0x0400
#define ET_DMA4_MASK		0x000F	
#define ET_DMA4_WRAP		0x0010

#define INDEX12(x)	((x) & ET_DMA12_MASK)
#define INDEX10(x)	((x) & ET_DMA10_MASK)
#define INDEX4(x)	((x) & ET_DMA4_MASK)


struct txdma_regs {			
	u32 csr;			
	u32 pr_base_hi;			
	u32 pr_base_lo;			
	u32 pr_num_des;			
	u32 txq_wr_addr;		
	u32 txq_wr_addr_ext;		
	u32 txq_rd_addr;		
	u32 dma_wb_base_hi;		
	u32 dma_wb_base_lo;		
	u32 service_request;		
	u32 service_complete;		
	u32 cache_rd_index;		
	u32 cache_wr_index;		
	u32 tx_dma_error;		
	u32 desc_abort_cnt;		
	u32 payload_abort_cnt;		
	u32 writeback_abort_cnt;	
	u32 desc_timeout_cnt;		
	u32 payload_timeout_cnt;	
	u32 writeback_timeout_cnt;	
	u32 desc_error_cnt;		
	u32 payload_error_cnt;		
	u32 writeback_error_cnt;	
	u32 dropped_tlp_cnt;		
	u32 new_service_complete;	
	u32 ethernet_packet_cnt;	
};

































struct rxdma_regs {					
	u32 csr;					
	u32 dma_wb_base_lo;				
	u32 dma_wb_base_hi;				
	u32 num_pkt_done;				
	u32 max_pkt_time;				
	u32 rxq_rd_addr;				
	u32 rxq_rd_addr_ext;				
	u32 rxq_wr_addr;				
	u32 psr_base_lo;				
	u32 psr_base_hi;				
	u32 psr_num_des;				
	u32 psr_avail_offset;				
	u32 psr_full_offset;				
	u32 psr_access_index;				
	u32 psr_min_des;				
	u32 fbr0_base_lo;				
	u32 fbr0_base_hi;				
	u32 fbr0_num_des;				
	u32 fbr0_avail_offset;				
	u32 fbr0_full_offset;				
	u32 fbr0_rd_index;				
	u32 fbr0_min_des;				
	u32 fbr1_base_lo;				
	u32 fbr1_base_hi;				
	u32 fbr1_num_des;				
	u32 fbr1_avail_offset;				
	u32 fbr1_full_offset;				
	u32 fbr1_rd_index;				
	u32 fbr1_min_des;				
};













struct txmac_regs {			
	u32 ctl;			
	u32 shadow_ptr;			
	u32 err_cnt;			
	u32 max_fill;			
	u32 cf_param;			
	u32 tx_test;			
	u32 err;			
	u32 err_int;			
	u32 bp_ctrl;			
};








#define ET_WOL_LO_SA3_SHIFT 24
#define ET_WOL_LO_SA4_SHIFT 16
#define ET_WOL_LO_SA5_SHIFT 8


#define ET_WOL_HI_SA1_SHIFT 8



#define ET_UNI_PF_ADDR1_3_SHIFT 24
#define ET_UNI_PF_ADDR1_4_SHIFT 16
#define ET_UNI_PF_ADDR1_5_SHIFT 8


#define ET_UNI_PF_ADDR2_3_SHIFT 24
#define ET_UNI_PF_ADDR2_4_SHIFT 16
#define ET_UNI_PF_ADDR2_5_SHIFT 8


#define ET_UNI_PF_ADDR2_1_SHIFT 24
#define ET_UNI_PF_ADDR2_2_SHIFT 16
#define ET_UNI_PF_ADDR1_1_SHIFT 8










struct rxmac_regs {					
	u32 ctrl;					
	u32 crc0;					
	u32 crc12;					
	u32 crc34;					
	u32 sa_lo;					
	u32 sa_hi;					
	u32 mask0_word0;				
	u32 mask0_word1;				
	u32 mask0_word2;				
	u32 mask0_word3;				
	u32 mask1_word0;				
	u32 mask1_word1;				
	u32 mask1_word2;				
	u32 mask1_word3;				
	u32 mask2_word0;				
	u32 mask2_word1;				
	u32 mask2_word2;				
	u32 mask2_word3;				
	u32 mask3_word0;				
	u32 mask3_word1;				
	u32 mask3_word2;				
	u32 mask3_word3;				
	u32 mask4_word0;				
	u32 mask4_word1;				
	u32 mask4_word2;				
	u32 mask4_word3;				
	u32 uni_pf_addr1;				
	u32 uni_pf_addr2;				
	u32 uni_pf_addr3;				
	u32 multi_hash1;				
	u32 multi_hash2;				
	u32 multi_hash3;				
	u32 multi_hash4;				
	u32 pf_ctrl;					
	u32 mcif_ctrl_max_seg;				
	u32 mcif_water_mark;				
	u32 rxq_diag;					
	u32 space_avail;				

	u32 mif_ctrl;					
	u32 err_reg;					
};





#define CFG1_LOOPBACK	0x00000100
#define CFG1_RX_FLOW	0x00000020
#define CFG1_TX_FLOW	0x00000010
#define CFG1_RX_ENABLE	0x00000004
#define CFG1_TX_ENABLE	0x00000001
#define CFG1_WAIT	0x0000000A	










#define MII_ADDR(phy, reg)	((phy) << 8 | (reg))




#define MGMT_BUSY	0x00000001	
#define MGMT_WAIT	0x00000005	




#define ET_MAC_STATION_ADDR1_OC6_SHIFT 24
#define ET_MAC_STATION_ADDR1_OC5_SHIFT 16
#define ET_MAC_STATION_ADDR1_OC4_SHIFT 8


#define ET_MAC_STATION_ADDR2_OC2_SHIFT 24
#define ET_MAC_STATION_ADDR2_OC1_SHIFT 16

struct mac_regs {					
	u32 cfg1;					
	u32 cfg2;					
	u32 ipg;					
	u32 hfdp;					
	u32 max_fm_len;					
	u32 rsv1;					
	u32 rsv2;					
	u32 mac_test;					
	u32 mii_mgmt_cfg;				
	u32 mii_mgmt_cmd;				
	u32 mii_mgmt_addr;				
	u32 mii_mgmt_ctrl;				
	u32 mii_mgmt_stat;				
	u32 mii_mgmt_indicator;				
	u32 if_ctrl;					
	u32 if_stat;					
	u32 station_addr_1;				
	u32 station_addr_2;				
};





struct macstat_regs {			
	u32 pad[32];			

	
	u32 txrx_0_64_byte_frames;	

	
	u32 txrx_65_127_byte_frames;	

	
	u32 txrx_128_255_byte_frames;	

	
	u32 txrx_256_511_byte_frames;	

	
	u32 txrx_512_1023_byte_frames;	

	
	u32 txrx_1024_1518_byte_frames;	

	
	u32 txrx_1519_1522_gvln_frames;	

	
	u32 rx_bytes;			

	
	u32 rx_packets;			

	
	u32 rx_fcs_errs;		

	
	u32 rx_multicast_packets;	

	
	u32 rx_broadcast_packets;	

	
	u32 rx_control_frames;		

	
	u32 rx_pause_frames;		

	
	u32 rx_unknown_opcodes;		

	
	u32 rx_align_errs;		

	
	u32 rx_frame_len_errs;		

	
	u32 rx_code_errs;		

	
	u32 rx_carrier_sense_errs;	

	
	u32 rx_undersize_packets;	

	
	u32 rx_oversize_packets;	

	
	u32 rx_fragment_packets;	

	
	u32 rx_jabbers;			

	
	u32 rx_drops;			

	
	u32 tx_bytes;			

	
	u32 tx_packets;			

	
	u32 tx_multicast_packets;	

	
	u32 tx_broadcast_packets;	

	
	u32 tx_pause_frames;		

	
	u32 tx_deferred;		

	
	u32 tx_excessive_deferred;	

	
	u32 tx_single_collisions;	

	
	u32 tx_multiple_collisions;	

	
	u32 tx_late_collisions;		

	
	u32 tx_excessive_collisions;	

	
	u32 tx_total_collisions;	

	
	u32 tx_pause_honored_frames;	

	
	u32 tx_drops;			

	
	u32 tx_jabbers;			

	
	u32 tx_fcs_errs;		

	
	u32 tx_control_frames;		

	
	u32 tx_oversize_frames;		

	
	u32 tx_undersize_frames;	

	
	u32 tx_fragments;		

	
	u32 carry_reg1;			

	
	u32 carry_reg2;			

	
	u32 carry_reg1_mask;		

	
	u32 carry_reg2_mask;		
};




#define ET_MMC_ENABLE		1
#define ET_MMC_ARB_DISABLE	2
#define ET_MMC_RXMAC_DISABLE	4
#define ET_MMC_TXMAC_DISABLE	8
#define ET_MMC_TXDMA_DISABLE	16
#define ET_MMC_RXDMA_DISABLE	32
#define ET_MMC_FORCE_CE		64


#define ET_SRAM_REQ_ACCESS	1
#define ET_SRAM_WR_ACCESS	2
#define ET_SRAM_IS_CTRL		4


struct mmc_regs {		
	u32 mmc_ctrl;		
	u32 sram_access;	
	u32 sram_word1;		
	u32 sram_word2;		
	u32 sram_word3;		
	u32 sram_word4;		
};



struct address_map {
	struct global_regs global;
	
	u8 unused_global[4096 - sizeof(struct global_regs)];
	struct txdma_regs txdma;
	
	u8 unused_txdma[4096 - sizeof(struct txdma_regs)];
	struct rxdma_regs rxdma;
	
	u8 unused_rxdma[4096 - sizeof(struct rxdma_regs)];
	struct txmac_regs txmac;
	
	u8 unused_txmac[4096 - sizeof(struct txmac_regs)];
	struct rxmac_regs rxmac;
	
	u8 unused_rxmac[4096 - sizeof(struct rxmac_regs)];
	struct mac_regs mac;
	
	u8 unused_mac[4096 - sizeof(struct mac_regs)];
	struct macstat_regs macstat;
	
	u8 unused_mac_stat[4096 - sizeof(struct macstat_regs)];
	struct mmc_regs mmc;
	
	u8 unused_mmc[4096 - sizeof(struct mmc_regs)];
	
	u8 unused_[1015808];

	u8 unused_exp_rom[4096];	
	u8 unused__[524288];	
};


#define PHY_INDEX_REG              0x10
#define PHY_DATA_REG               0x11
#define PHY_MPHY_CONTROL_REG       0x12

#define PHY_LOOPBACK_CONTROL       0x13	
					
#define PHY_REGISTER_MGMT_CONTROL  0x15	
#define PHY_CONFIG                 0x16	
#define PHY_PHY_CONTROL            0x17	
#define PHY_INTERRUPT_MASK         0x18	
#define PHY_INTERRUPT_STATUS       0x19	
#define PHY_PHY_STATUS             0x1A	
#define PHY_LED_1                  0x1B	
#define PHY_LED_2                  0x1C	
					
					

#define ET_1000BT_MSTR_SLV 0x4000






#define ET_PHY_CONFIG_TX_FIFO_DEPTH	0x3000

#define ET_PHY_CONFIG_FIFO_DEPTH_8	0x0000
#define ET_PHY_CONFIG_FIFO_DEPTH_16	0x1000
#define ET_PHY_CONFIG_FIFO_DEPTH_32	0x2000
#define ET_PHY_CONFIG_FIFO_DEPTH_64	0x3000



#define ET_PHY_INT_MASK_AUTONEGSTAT	0x0100
#define ET_PHY_INT_MASK_LINKSTAT	0x0004
#define ET_PHY_INT_MASK_ENABLE		0x0001


#define ET_PHY_AUTONEG_STATUS	0x1000
#define ET_PHY_POLARITY_STATUS	0x0400
#define ET_PHY_SPEED_STATUS	0x0300
#define ET_PHY_DUPLEX_STATUS	0x0080
#define ET_PHY_LSTATUS		0x0040
#define ET_PHY_AUTONEG_ENABLE	0x0020


#define ET_LED2_LED_LINK	0xF000
#define ET_LED2_LED_TXRX	0x0F00
#define ET_LED2_LED_100TX	0x00F0
#define ET_LED2_LED_1000T	0x000F

#define LED_VAL_1000BT			0x0
#define LED_VAL_100BTX			0x1
#define LED_VAL_10BT			0x2
#define LED_VAL_1000BT_100BTX		0x3 
#define LED_VAL_LINKON			0x4
#define LED_VAL_TX			0x5
#define LED_VAL_RX			0x6
#define LED_VAL_TXRX			0x7 
#define LED_VAL_DUPLEXFULL		0x8
#define LED_VAL_COLLISION		0x9
#define LED_VAL_LINKON_ACTIVE		0xA 
#define LED_VAL_LINKON_RECV		0xB 
#define LED_VAL_DUPLEXFULL_COLLISION	0xC 
#define LED_VAL_BLINK			0xD
#define LED_VAL_ON			0xE
#define LED_VAL_OFF			0xF

#define LED_LINK_SHIFT			12
#define LED_TXRX_SHIFT			8
#define LED_100TX_SHIFT			4



#define TRUEPHY_BIT_CLEAR               0
#define TRUEPHY_BIT_SET                 1
#define TRUEPHY_BIT_READ                2

#ifndef TRUEPHY_READ
#define TRUEPHY_READ                    0
#define TRUEPHY_WRITE                   1
#define TRUEPHY_MASK                    2
#endif

#define TRUEPHY_CFG_SLAVE               0
#define TRUEPHY_CFG_MASTER              1

#define TRUEPHY_MDI                     0
#define TRUEPHY_MDIX                    1
#define TRUEPHY_AUTO_MDI_MDIX           2

#define TRUEPHY_POLARITY_NORMAL         0
#define TRUEPHY_POLARITY_INVERTED       1

#define TRUEPHY_ANEG_NOT_COMPLETE       0
#define TRUEPHY_ANEG_COMPLETE           1
#define TRUEPHY_ANEG_DISABLED           2

#define TRUEPHY_ADV_DUPLEX_NONE         0x00
#define TRUEPHY_ADV_DUPLEX_FULL         0x01
#define TRUEPHY_ADV_DUPLEX_HALF         0x02
#define TRUEPHY_ADV_DUPLEX_BOTH     \
	(TRUEPHY_ADV_DUPLEX_FULL | TRUEPHY_ADV_DUPLEX_HALF)

