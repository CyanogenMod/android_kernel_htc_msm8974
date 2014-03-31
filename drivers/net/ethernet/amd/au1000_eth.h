/*
 *
 * Alchemy Au1x00 ethernet driver include file
 *
 * Author: Pete Popov <ppopov@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 *
 */


#define MAC_IOSIZE 0x10000
#define NUM_RX_DMA 4       
#define NUM_TX_DMA 4       

#define NUM_RX_BUFFS 4
#define NUM_TX_BUFFS 4
#define MAX_BUF_SIZE 2048

#define ETH_TX_TIMEOUT (HZ/4)
#define MAC_MIN_PKT_SIZE 64

#define MULTICAST_FILTER_LIMIT 64

struct db_dest {
	struct db_dest *pnext;
	u32 *vaddr;
	dma_addr_t dma_addr;
};

struct tx_dma {
	u32 status;
	u32 buff_stat;
	u32 len;
	u32 pad;
};

struct rx_dma {
	u32 status;
	u32 buff_stat;
	u32 pad[2];
};


struct mac_reg {
	u32 control;
	u32 mac_addr_high;
	u32 mac_addr_low;
	u32 multi_hash_high;
	u32 multi_hash_low;
	u32 mii_control;
	u32 mii_data;
	u32 flow_control;
	u32 vlan1_tag;
	u32 vlan2_tag;
};


struct au1000_private {
	struct db_dest *pDBfree;
	struct db_dest db[NUM_RX_BUFFS+NUM_TX_BUFFS];
	struct rx_dma *rx_dma_ring[NUM_RX_DMA];
	struct tx_dma *tx_dma_ring[NUM_TX_DMA];
	struct db_dest *rx_db_inuse[NUM_RX_DMA];
	struct db_dest *tx_db_inuse[NUM_TX_DMA];
	u32 rx_head;
	u32 tx_head;
	u32 tx_tail;
	u32 tx_full;

	int mac_id;

	int mac_enabled;       

	int old_link;          
	int old_speed;
	int old_duplex;

	struct phy_device *phy_dev;
	struct mii_bus *mii_bus;

	
	int phy_static_config;
	int phy_search_highest_addr;
	int phy1_search_mac0;

	int phy_addr;
	int phy_busid;
	int phy_irq;

	struct mac_reg *mac;  
	u32 *enable;     
	void __iomem *macdma;	
	u32 vaddr;                
	dma_addr_t dma_addr;      

	spinlock_t lock;       

	u32 msg_enable;
};
