/* atl2.h -- atl2 driver definitions
 *
 * Copyright(c) 2007 Atheros Corporation. All rights reserved.
 * Copyright(c) 2006 xiong huang <xiong.huang@atheros.com>
 * Copyright(c) 2007 Chris Snook <csnook@redhat.com>
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

#ifndef _ATL2_H_
#define _ATL2_H_

#include <linux/atomic.h>
#include <linux/netdevice.h>

#ifndef _ATL2_HW_H_
#define _ATL2_HW_H_

#ifndef _ATL2_OSDEP_H_
#define _ATL2_OSDEP_H_

#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>

#include "atlx.h"

#ifdef ETHTOOL_OPS_COMPAT
extern int ethtool_ioctl(struct ifreq *ifr);
#endif

#define PCI_COMMAND_REGISTER	PCI_COMMAND
#define CMD_MEM_WRT_INVALIDATE	PCI_COMMAND_INVALIDATE

#define ATL2_WRITE_REG(a, reg, value) (iowrite32((value), \
	((a)->hw_addr + (reg))))

#define ATL2_WRITE_FLUSH(a) (ioread32((a)->hw_addr))

#define ATL2_READ_REG(a, reg) (ioread32((a)->hw_addr + (reg)))

#define ATL2_WRITE_REGB(a, reg, value) (iowrite8((value), \
	((a)->hw_addr + (reg))))

#define ATL2_READ_REGB(a, reg) (ioread8((a)->hw_addr + (reg)))

#define ATL2_WRITE_REGW(a, reg, value) (iowrite16((value), \
	((a)->hw_addr + (reg))))

#define ATL2_READ_REGW(a, reg) (ioread16((a)->hw_addr + (reg)))

#define ATL2_WRITE_REG_ARRAY(a, reg, offset, value) \
	(iowrite32((value), (((a)->hw_addr + (reg)) + ((offset) << 2))))

#define ATL2_READ_REG_ARRAY(a, reg, offset) \
	(ioread32(((a)->hw_addr + (reg)) + ((offset) << 2)))

#endif 

struct atl2_adapter;
struct atl2_hw;

static s32 atl2_reset_hw(struct atl2_hw *hw);
static s32 atl2_read_mac_addr(struct atl2_hw *hw);
static s32 atl2_init_hw(struct atl2_hw *hw);
static s32 atl2_get_speed_and_duplex(struct atl2_hw *hw, u16 *speed,
	u16 *duplex);
static u32 atl2_hash_mc_addr(struct atl2_hw *hw, u8 *mc_addr);
static void atl2_hash_set(struct atl2_hw *hw, u32 hash_value);
static s32 atl2_read_phy_reg(struct atl2_hw *hw, u16 reg_addr, u16 *phy_data);
static s32 atl2_write_phy_reg(struct atl2_hw *hw, u32 reg_addr, u16 phy_data);
static void atl2_read_pci_cfg(struct atl2_hw *hw, u32 reg, u16 *value);
static void atl2_write_pci_cfg(struct atl2_hw *hw, u32 reg, u16 *value);
static void atl2_set_mac_addr(struct atl2_hw *hw);
static bool atl2_read_eeprom(struct atl2_hw *hw, u32 Offset, u32 *pValue);
static bool atl2_write_eeprom(struct atl2_hw *hw, u32 offset, u32 value);
static s32 atl2_phy_init(struct atl2_hw *hw);
static int atl2_check_eeprom_exist(struct atl2_hw *hw);
static void atl2_force_ps(struct atl2_hw *hw);


#define IDLE_STATUS_RXMAC	1	
#define IDLE_STATUS_TXMAC	2	
#define IDLE_STATUS_DMAR	8	
#define IDLE_STATUS_DMAW	4	

#define MDIO_WAIT_TIMES		10

#define MAC_CTRL_DBG_TX_BKPRESURE	0x100000	
#define MAC_CTRL_MACLP_CLK_PHY		0x8000000	
#define MAC_CTRL_HALF_LEFT_BUF_SHIFT	28
#define MAC_CTRL_HALF_LEFT_BUF_MASK	0xF		

#define REG_SRAM_TXRAM_END	0x1500	
#define REG_SRAM_RXRAM_END	0x1502	

#define REG_TXD_BASE_ADDR_LO	0x1544	
#define REG_TXD_MEM_SIZE	0x1548	
#define REG_TXS_BASE_ADDR_LO	0x154C	
#define REG_TXS_MEM_SIZE	0x1550	
#define REG_RXD_BASE_ADDR_LO	0x1554	
#define REG_RXD_BUF_NUM		0x1558	

#define REG_DMAR	0x1580
#define     DMAR_EN	0x1	

#define REG_TX_CUT_THRESH	0x1590	

#define REG_DMAW	0x15A0
#define     DMAW_EN	0x1

#define REG_PAUSE_ON_TH		0x15A8	
#define REG_PAUSE_OFF_TH	0x15AA	

#define REG_MB_TXD_WR_IDX	0x15f0	
#define REG_MB_RXD_RD_IDX	0x15F4	

#define ISR_TIMER	1	
#define ISR_MANUAL	2	
#define ISR_RXF_OV	4	
#define ISR_TXF_UR	8	
#define ISR_TXS_OV	0x10	
#define ISR_RXS_OV	0x20	
#define ISR_LINK_CHG	0x40	
#define ISR_HOST_TXD_UR	0x80
#define ISR_HOST_RXD_OV	0x100	
#define ISR_DMAR_TO_RST	0x200	
#define ISR_DMAW_TO_RST	0x400
#define ISR_PHY		0x800	
#define ISR_TS_UPDATE	0x10000	/* interrupt after new tx pkt status written
				 * to host */
#define ISR_RS_UPDATE	0x20000	/* interrupt ater new rx pkt status written
				 * to host. */
#define ISR_TX_EARLY	0x40000	

#define ISR_TX_EVENT (ISR_TXF_UR | ISR_TXS_OV | ISR_HOST_TXD_UR |\
	ISR_TS_UPDATE | ISR_TX_EARLY)
#define ISR_RX_EVENT (ISR_RXF_OV | ISR_RXS_OV | ISR_HOST_RXD_OV |\
	 ISR_RS_UPDATE)

#define IMR_NORMAL_MASK		(\
	\
	ISR_MANUAL		|\
	ISR_DMAR_TO_RST		|\
	ISR_DMAW_TO_RST		|\
	ISR_PHY			|\
	ISR_PHY_LINKDOWN	|\
	ISR_TS_UPDATE		|\
	ISR_RS_UPDATE)

#define REG_STS_RX_PAUSE	0x1700	
#define REG_STS_RXD_OV		0x1704	
#define REG_STS_RXS_OV		0x1708	
#define REG_STS_RX_FILTER	0x170C	


#define MII_SMARTSPEED	0x14
#define MII_DBG_ADDR	0x1D
#define MII_DBG_DATA	0x1E

#define PCI_REG_COMMAND		0x04
#define CMD_IO_SPACE		0x0001
#define CMD_MEMORY_SPACE	0x0002
#define CMD_BUS_MASTER		0x0004

#define MEDIA_TYPE_100M_FULL	1
#define MEDIA_TYPE_100M_HALF	2
#define MEDIA_TYPE_10M_FULL	3
#define MEDIA_TYPE_10M_HALF	4

#define AUTONEG_ADVERTISE_SPEED_DEFAULT	0x000F	

#define ENET_HEADER_SIZE		14
#define MAXIMUM_ETHERNET_FRAME_SIZE	1518	
#define MINIMUM_ETHERNET_FRAME_SIZE	64	
#define ETHERNET_FCS_SIZE		4
#define MAX_JUMBO_FRAME_SIZE		0x2000
#define VLAN_SIZE                                               4

struct tx_pkt_header {
	unsigned pkt_size:11;
	unsigned:4;			
	unsigned ins_vlan:1;		
	unsigned short vlan;		
};
#define TX_PKT_HEADER_SIZE_MASK		0x7FF
#define TX_PKT_HEADER_SIZE_SHIFT	0
#define TX_PKT_HEADER_INS_VLAN_MASK	0x1
#define TX_PKT_HEADER_INS_VLAN_SHIFT	15
#define TX_PKT_HEADER_VLAN_TAG_MASK	0xFFFF
#define TX_PKT_HEADER_VLAN_TAG_SHIFT	16

struct tx_pkt_status {
	unsigned pkt_size:11;
	unsigned:5;		
	unsigned ok:1;		
	unsigned bcast:1;	
	unsigned mcast:1;	
	unsigned pause:1;	
	unsigned ctrl:1;
	unsigned defer:1;    	
	unsigned exc_defer:1;
	unsigned single_col:1;
	unsigned multi_col:1;
	unsigned late_col:1;
	unsigned abort_col:1;
	unsigned underun:1;	
	unsigned:3;		
	unsigned update:1;	
};
#define TX_PKT_STATUS_SIZE_MASK		0x7FF
#define TX_PKT_STATUS_SIZE_SHIFT	0
#define TX_PKT_STATUS_OK_MASK		0x1
#define TX_PKT_STATUS_OK_SHIFT		16
#define TX_PKT_STATUS_BCAST_MASK	0x1
#define TX_PKT_STATUS_BCAST_SHIFT	17
#define TX_PKT_STATUS_MCAST_MASK	0x1
#define TX_PKT_STATUS_MCAST_SHIFT	18
#define TX_PKT_STATUS_PAUSE_MASK	0x1
#define TX_PKT_STATUS_PAUSE_SHIFT	19
#define TX_PKT_STATUS_CTRL_MASK		0x1
#define TX_PKT_STATUS_CTRL_SHIFT	20
#define TX_PKT_STATUS_DEFER_MASK	0x1
#define TX_PKT_STATUS_DEFER_SHIFT	21
#define TX_PKT_STATUS_EXC_DEFER_MASK	0x1
#define TX_PKT_STATUS_EXC_DEFER_SHIFT	22
#define TX_PKT_STATUS_SINGLE_COL_MASK	0x1
#define TX_PKT_STATUS_SINGLE_COL_SHIFT	23
#define TX_PKT_STATUS_MULTI_COL_MASK	0x1
#define TX_PKT_STATUS_MULTI_COL_SHIFT	24
#define TX_PKT_STATUS_LATE_COL_MASK	0x1
#define TX_PKT_STATUS_LATE_COL_SHIFT	25
#define TX_PKT_STATUS_ABORT_COL_MASK	0x1
#define TX_PKT_STATUS_ABORT_COL_SHIFT	26
#define TX_PKT_STATUS_UNDERRUN_MASK	0x1
#define TX_PKT_STATUS_UNDERRUN_SHIFT	27
#define TX_PKT_STATUS_UPDATE_MASK	0x1
#define TX_PKT_STATUS_UPDATE_SHIFT	31

struct rx_pkt_status {
	unsigned pkt_size:11;	
	unsigned:5;		
	unsigned ok:1;		
	unsigned bcast:1;	
	unsigned mcast:1;	
	unsigned pause:1;
	unsigned ctrl:1;
	unsigned crc:1;		
	unsigned code:1;	
	unsigned runt:1;	
	unsigned frag:1;	
	unsigned trunc:1;	
	unsigned align:1;	
	unsigned vlan:1;	
	unsigned:3;		
	unsigned update:1;
	unsigned short vtag;	
	unsigned:16;
};
#define RX_PKT_STATUS_SIZE_MASK		0x7FF
#define RX_PKT_STATUS_SIZE_SHIFT	0
#define RX_PKT_STATUS_OK_MASK		0x1
#define RX_PKT_STATUS_OK_SHIFT		16
#define RX_PKT_STATUS_BCAST_MASK	0x1
#define RX_PKT_STATUS_BCAST_SHIFT	17
#define RX_PKT_STATUS_MCAST_MASK	0x1
#define RX_PKT_STATUS_MCAST_SHIFT	18
#define RX_PKT_STATUS_PAUSE_MASK	0x1
#define RX_PKT_STATUS_PAUSE_SHIFT	19
#define RX_PKT_STATUS_CTRL_MASK		0x1
#define RX_PKT_STATUS_CTRL_SHIFT	20
#define RX_PKT_STATUS_CRC_MASK		0x1
#define RX_PKT_STATUS_CRC_SHIFT		21
#define RX_PKT_STATUS_CODE_MASK		0x1
#define RX_PKT_STATUS_CODE_SHIFT	22
#define RX_PKT_STATUS_RUNT_MASK		0x1
#define RX_PKT_STATUS_RUNT_SHIFT	23
#define RX_PKT_STATUS_FRAG_MASK		0x1
#define RX_PKT_STATUS_FRAG_SHIFT	24
#define RX_PKT_STATUS_TRUNK_MASK	0x1
#define RX_PKT_STATUS_TRUNK_SHIFT	25
#define RX_PKT_STATUS_ALIGN_MASK	0x1
#define RX_PKT_STATUS_ALIGN_SHIFT	26
#define RX_PKT_STATUS_VLAN_MASK		0x1
#define RX_PKT_STATUS_VLAN_SHIFT	27
#define RX_PKT_STATUS_UPDATE_MASK	0x1
#define RX_PKT_STATUS_UPDATE_SHIFT	31
#define RX_PKT_STATUS_VLAN_TAG_MASK	0xFFFF
#define RX_PKT_STATUS_VLAN_TAG_SHIFT	32

struct rx_desc {
	struct rx_pkt_status	status;
	unsigned char     	packet[1536-sizeof(struct rx_pkt_status)];
};

enum atl2_speed_duplex {
	atl2_10_half = 0,
	atl2_10_full = 1,
	atl2_100_half = 2,
	atl2_100_full = 3
};

struct atl2_spi_flash_dev {
	const char *manu_name;	
	
	u8 cmdWRSR;
	u8 cmdREAD;
	u8 cmdPROGRAM;
	u8 cmdWREN;
	u8 cmdWRDI;
	u8 cmdRDSR;
	u8 cmdRDID;
	u8 cmdSECTOR_ERASE;
	u8 cmdCHIP_ERASE;
};

struct atl2_hw {
	u8 __iomem *hw_addr;
	void *back;

	u8 preamble_len;
	u8 max_retry;          
	u8 jam_ipg;            
	u8 ipgt;               
	u8 min_ifg;            
	u8 ipgr1;              
	u8 ipgr2;              
	u8 retry_buf;          

	u16 fc_rxd_hi;
	u16 fc_rxd_lo;
	u16 lcol;              
	u16 max_frame_size;

	u16 MediaType;
	u16 autoneg_advertised;
	u16 pci_cmd_word;

	u16 mii_autoneg_adv_reg;

	u32 mem_rang;
	u32 txcw;
	u32 mc_filter_type;
	u32 num_mc_addrs;
	u32 collision_delta;
	u32 tx_packet_delta;
	u16 phy_spd_default;

	u16 device_id;
	u16 vendor_id;
	u16 subsystem_id;
	u16 subsystem_vendor_id;
	u8 revision_id;

	
	u8 flash_vendor;

	u8 dma_fairness;
	u8 mac_addr[ETH_ALEN];
	u8 perm_mac_addr[ETH_ALEN];

	
	
	bool phy_configured;
};

#endif 

struct atl2_ring_header {
    
    void *desc;
    
    dma_addr_t dma;
    
    unsigned int size;
};

struct atl2_adapter {
	
	struct net_device *netdev;
	struct pci_dev *pdev;
	u32 wol;
	u16 link_speed;
	u16 link_duplex;

	spinlock_t stats_lock;

	struct work_struct reset_task;
	struct work_struct link_chg_task;
	struct timer_list watchdog_timer;
	struct timer_list phy_config_timer;

	unsigned long cfg_phy;
	bool mac_disabled;

	
	dma_addr_t	ring_dma;
	void		*ring_vir_addr;
	int		ring_size;

	struct tx_pkt_header	*txd_ring;
	dma_addr_t	txd_dma;

	struct tx_pkt_status	*txs_ring;
	dma_addr_t	txs_dma;

	struct rx_desc	*rxd_ring;
	dma_addr_t	rxd_dma;

	u32 txd_ring_size;         
	u32 txs_ring_size;         
	u32 rxd_ring_size;         

	
	
	u32 txd_write_ptr;
	u32 txs_next_clear;
	u32 rxd_read_ptr;

	
	atomic_t txd_read_ptr;
	atomic_t txs_write_ptr;
	u32 rxd_write_ptr;

	
	u16 imt;
	
	u16 ict;

	unsigned long flags;
	
	u32 bd_number;     
	bool pci_using_64;
	bool have_msi;
	struct atl2_hw hw;

	u32 usr_cmd;
	
	
	u32 pci_state[16];

	u32 *config_space;
};

enum atl2_state_t {
	__ATL2_TESTING,
	__ATL2_RESETTING,
	__ATL2_DOWN
};

#endif 
