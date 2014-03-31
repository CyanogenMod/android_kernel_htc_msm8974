/*******************************************************************************
  MAC 10/100 Header File

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

#define MAC_CONTROL	0x00000000	
#define MAC_ADDR_HIGH	0x00000004	
#define MAC_ADDR_LOW	0x00000008	
#define MAC_HASH_HIGH	0x0000000c	
#define MAC_HASH_LOW	0x00000010	
#define MAC_MII_ADDR	0x00000014	
#define MAC_MII_DATA	0x00000018	
#define MAC_FLOW_CTRL	0x0000001c	
#define MAC_VLAN1	0x00000020	
#define MAC_VLAN2	0x00000024	

#define MAC_CONTROL_RA	0x80000000	
#define MAC_CONTROL_BLE	0x40000000	
#define MAC_CONTROL_HBD	0x10000000	
#define MAC_CONTROL_PS	0x08000000	
#define MAC_CONTROL_DRO	0x00800000	
#define MAC_CONTROL_EXT_LOOPBACK 0x00400000	
#define MAC_CONTROL_OM	0x00200000	
#define MAC_CONTROL_F	0x00100000	
#define MAC_CONTROL_PM	0x00080000	
#define MAC_CONTROL_PR	0x00040000	
#define MAC_CONTROL_IF	0x00020000	
#define MAC_CONTROL_PB	0x00010000	
#define MAC_CONTROL_HO	0x00008000	
#define MAC_CONTROL_HP	0x00002000	
#define MAC_CONTROL_LCC	0x00001000	
#define MAC_CONTROL_DBF	0x00000800	
#define MAC_CONTROL_DRTY	0x00000400	
#define MAC_CONTROL_ASTP	0x00000100	
#define MAC_CONTROL_BOLMT_10	0x00000000	
#define MAC_CONTROL_BOLMT_8	0x00000040	
#define MAC_CONTROL_BOLMT_4	0x00000080	
#define MAC_CONTROL_BOLMT_1	0x000000c0	
#define MAC_CONTROL_DC		0x00000020	
#define MAC_CONTROL_TE		0x00000008	
#define MAC_CONTROL_RE		0x00000004	

#define MAC_CORE_INIT (MAC_CONTROL_HBD | MAC_CONTROL_ASTP)

#define MAC_FLOW_CTRL_PT_MASK	0xffff0000	
#define MAC_FLOW_CTRL_PT_SHIFT	16
#define MAC_FLOW_CTRL_PASS	0x00000004	
#define MAC_FLOW_CTRL_ENABLE	0x00000002	
#define MAC_FLOW_CTRL_PAUSE	0x00000001	

#define MAC_MII_ADDR_WRITE	0x00000002	
#define MAC_MII_ADDR_BUSY	0x00000001	


#define DMA_BUS_MODE_DBO	0x00100000	
#define DMA_BUS_MODE_BLE	0x00000080	
#define DMA_BUS_MODE_PBL_MASK	0x00003f00	
#define DMA_BUS_MODE_PBL_SHIFT	8
#define DMA_BUS_MODE_DSL_MASK	0x0000007c	
#define DMA_BUS_MODE_DSL_SHIFT	2	
#define DMA_BUS_MODE_BAR_BUS	0x00000002	
#define DMA_BUS_MODE_SFT_RESET	0x00000001	
#define DMA_BUS_MODE_DEFAULT	0x00000000

#define DMA_CONTROL_SF		0x00200000	

enum ttc_control {
	DMA_CONTROL_TTC_DEFAULT = 0x00000000,	
	DMA_CONTROL_TTC_64 = 0x00004000,	
	DMA_CONTROL_TTC_128 = 0x00008000,	
	DMA_CONTROL_TTC_256 = 0x0000c000,	
	DMA_CONTROL_TTC_18 = 0x00400000,	
	DMA_CONTROL_TTC_24 = 0x00404000,	
	DMA_CONTROL_TTC_32 = 0x00408000,	
	DMA_CONTROL_TTC_40 = 0x0040c000,	
	DMA_CONTROL_SE = 0x00000008,	
	DMA_CONTROL_OSF = 0x00000004,	
};

#define DMA_MISSED_FRAME_OVE	0x10000000	
#define DMA_MISSED_FRAME_OVE_CNTR 0x0ffe0000	
#define DMA_MISSED_FRAME_OVE_M	0x00010000	
#define DMA_MISSED_FRAME_M_CNTR	0x0000ffff	

extern const struct stmmac_dma_ops dwmac100_dma_ops;
