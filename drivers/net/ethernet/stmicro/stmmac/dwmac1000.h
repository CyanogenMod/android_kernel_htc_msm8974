/*******************************************************************************
  Copyright (C) 2007-2009  STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*******************************************************************************/

#include <linux/phy.h>
#include "common.h"

#define GMAC_CONTROL		0x00000000	
#define GMAC_FRAME_FILTER	0x00000004	
#define GMAC_HASH_HIGH		0x00000008	
#define GMAC_HASH_LOW		0x0000000c	
#define GMAC_MII_ADDR		0x00000010	
#define GMAC_MII_DATA		0x00000014	
#define GMAC_FLOW_CTRL		0x00000018	
#define GMAC_VLAN_TAG		0x0000001c	
#define GMAC_VERSION		0x00000020	
#define GMAC_WAKEUP_FILTER	0x00000028	

#define GMAC_INT_STATUS		0x00000038	
enum dwmac1000_irq_status {
	time_stamp_irq = 0x0200,
	mmc_rx_csum_offload_irq = 0x0080,
	mmc_tx_irq = 0x0040,
	mmc_rx_irq = 0x0020,
	mmc_irq = 0x0010,
	pmt_irq = 0x0008,
	pcs_ane_irq = 0x0004,
	pcs_link_irq = 0x0002,
	rgmii_irq = 0x0001,
};
#define GMAC_INT_MASK		0x0000003c	

#define GMAC_PMT		0x0000002c
enum power_event {
	pointer_reset = 0x80000000,
	global_unicast = 0x00000200,
	wake_up_rx_frame = 0x00000040,
	magic_frame = 0x00000020,
	wake_up_frame_en = 0x00000004,
	magic_pkt_en = 0x00000002,
	power_down = 0x00000001,
};

#define GMAC_ADDR_HIGH(reg)		(0x00000040+(reg * 8))
#define GMAC_ADDR_LOW(reg)		(0x00000044+(reg * 8))
#define GMAC_MAX_UNICAST_ADDRESSES	16

#define GMAC_AN_CTRL	0x000000c0	
#define GMAC_AN_STATUS	0x000000c4	
#define GMAC_ANE_ADV	0x000000c8	
#define GMAC_ANE_LINK	0x000000cc	
#define GMAC_ANE_EXP	0x000000d0	
#define GMAC_TBI	0x000000d4	
#define GMAC_GMII_STATUS 0x000000d8	

#define GMAC_CONTROL_TC	0x01000000	
#define GMAC_CONTROL_WD	0x00800000	
#define GMAC_CONTROL_JD	0x00400000	
#define GMAC_CONTROL_BE	0x00200000	
#define GMAC_CONTROL_JE	0x00100000	
enum inter_frame_gap {
	GMAC_CONTROL_IFG_88 = 0x00040000,
	GMAC_CONTROL_IFG_80 = 0x00020000,
	GMAC_CONTROL_IFG_40 = 0x000e0000,
};
#define GMAC_CONTROL_DCRS	0x00010000 
#define GMAC_CONTROL_PS		0x00008000 
#define GMAC_CONTROL_FES	0x00004000 
#define GMAC_CONTROL_DO		0x00002000 
#define GMAC_CONTROL_LM		0x00001000 
#define GMAC_CONTROL_DM		0x00000800 
#define GMAC_CONTROL_IPC	0x00000400 
#define GMAC_CONTROL_DR		0x00000200 
#define GMAC_CONTROL_LUD	0x00000100 
#define GMAC_CONTROL_ACS	0x00000080 
#define GMAC_CONTROL_DC		0x00000010 
#define GMAC_CONTROL_TE		0x00000008 
#define GMAC_CONTROL_RE		0x00000004 

#define GMAC_CORE_INIT (GMAC_CONTROL_JD | GMAC_CONTROL_PS | GMAC_CONTROL_ACS | \
			GMAC_CONTROL_JE | GMAC_CONTROL_BE)

#define GMAC_FRAME_FILTER_PR	0x00000001	
#define GMAC_FRAME_FILTER_HUC	0x00000002	
#define GMAC_FRAME_FILTER_HMC	0x00000004	
#define GMAC_FRAME_FILTER_DAIF	0x00000008	
#define GMAC_FRAME_FILTER_PM	0x00000010	
#define GMAC_FRAME_FILTER_DBF	0x00000020	
#define GMAC_FRAME_FILTER_SAIF	0x00000100	
#define GMAC_FRAME_FILTER_SAF	0x00000200	
#define GMAC_FRAME_FILTER_HPF	0x00000400	
#define GMAC_FRAME_FILTER_RA	0x80000000	
#define GMAC_MII_ADDR_WRITE	0x00000002	
#define GMAC_MII_ADDR_BUSY	0x00000001	
#define GMAC_FLOW_CTRL_PT_MASK	0xffff0000	
#define GMAC_FLOW_CTRL_PT_SHIFT	16
#define GMAC_FLOW_CTRL_RFE	0x00000004	
#define GMAC_FLOW_CTRL_TFE	0x00000002	
#define GMAC_FLOW_CTRL_FCB_BPA	0x00000001	

#define DMA_BUS_MODE_SFT_RESET	0x00000001	
#define DMA_BUS_MODE_DA		0x00000002	
#define DMA_BUS_MODE_DSL_MASK	0x0000007c	
#define DMA_BUS_MODE_DSL_SHIFT	2	
#define DMA_BUS_MODE_PBL_MASK	0x00003f00	
#define DMA_BUS_MODE_PBL_SHIFT	8

enum rx_tx_priority_ratio {
	double_ratio = 0x00004000,	
	triple_ratio = 0x00008000,	
	quadruple_ratio = 0x0000c000,	
};

#define DMA_BUS_MODE_FB		0x00010000	
#define DMA_BUS_MODE_RPBL_MASK	0x003e0000	
#define DMA_BUS_MODE_RPBL_SHIFT	17
#define DMA_BUS_MODE_USP	0x00800000
#define DMA_BUS_MODE_4PBL	0x01000000
#define DMA_BUS_MODE_AAL	0x02000000

#define DMA_HOST_TX_DESC	  0x00001048	
#define DMA_HOST_RX_DESC	  0x0000104c	
#define DMA_BUS_PR_RATIO_MASK	  0x0000c000	
#define DMA_BUS_PR_RATIO_SHIFT	  14
#define DMA_BUS_FB	  	  0x00010000	

#define DMA_CONTROL_DT		0x04000000 
#define DMA_CONTROL_RSF		0x02000000 
#define DMA_CONTROL_DFF		0x01000000 
enum rfa {
	act_full_minus_1 = 0x00800000,
	act_full_minus_2 = 0x00800200,
	act_full_minus_3 = 0x00800400,
	act_full_minus_4 = 0x00800600,
};
enum rfd {
	deac_full_minus_1 = 0x00400000,
	deac_full_minus_2 = 0x00400800,
	deac_full_minus_3 = 0x00401000,
	deac_full_minus_4 = 0x00401800,
};
#define DMA_CONTROL_TSF		0x00200000 

enum ttc_control {
	DMA_CONTROL_TTC_64 = 0x00000000,
	DMA_CONTROL_TTC_128 = 0x00004000,
	DMA_CONTROL_TTC_192 = 0x00008000,
	DMA_CONTROL_TTC_256 = 0x0000c000,
	DMA_CONTROL_TTC_40 = 0x00010000,
	DMA_CONTROL_TTC_32 = 0x00014000,
	DMA_CONTROL_TTC_24 = 0x00018000,
	DMA_CONTROL_TTC_16 = 0x0001c000,
};
#define DMA_CONTROL_TC_TX_MASK	0xfffe3fff

#define DMA_CONTROL_EFC		0x00000100
#define DMA_CONTROL_FEF		0x00000080
#define DMA_CONTROL_FUF		0x00000040

enum rtc_control {
	DMA_CONTROL_RTC_64 = 0x00000000,
	DMA_CONTROL_RTC_32 = 0x00000008,
	DMA_CONTROL_RTC_96 = 0x00000010,
	DMA_CONTROL_RTC_128 = 0x00000018,
};
#define DMA_CONTROL_TC_RX_MASK	0xffffffe7

#define DMA_CONTROL_OSF	0x00000004	

#define GMAC_MMC_CTRL      0x100
#define GMAC_MMC_RX_INTR   0x104
#define GMAC_MMC_TX_INTR   0x108
#define GMAC_MMC_RX_CSUM_OFFLOAD   0x208

extern const struct stmmac_dma_ops dwmac1000_dma_ops;
