/*
 * Copyright(c) 2008 - 2009 Atheros Corporation. All rights reserved.
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

#ifndef _ATL1C_H_
#define _ATL1C_H_

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/mii.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/tcp.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/workqueue.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>

#include "atl1c_hw.h"

#define AT_WUFC_LNKC 0x00000001 
#define AT_WUFC_MAG  0x00000002 
#define AT_WUFC_EX   0x00000004 
#define AT_WUFC_MC   0x00000008 
#define AT_WUFC_BC   0x00000010 

#define AT_VLAN_TO_TAG(_vlan, _tag)	   \
	_tag =  ((((_vlan) >> 8) & 0xFF)  |\
		 (((_vlan) & 0xFF) << 8))

#define AT_TAG_TO_VLAN(_tag, _vlan) 	 \
	_vlan = ((((_tag) >> 8) & 0xFF) |\
		(((_tag) & 0xFF) << 8))

#define SPEED_0		   0xffff
#define HALF_DUPLEX        1
#define FULL_DUPLEX        2

#define AT_RX_BUF_SIZE		(ETH_FRAME_LEN + VLAN_HLEN + ETH_FCS_LEN)
#define MAX_JUMBO_FRAME_SIZE	(6*1024)
#define MAX_TSO_FRAME_SIZE      (7*1024)
#define MAX_TX_OFFLOAD_THRESH	(9*1024)

#define AT_MAX_RECEIVE_QUEUE    4
#define AT_DEF_RECEIVE_QUEUE	1
#define AT_MAX_TRANSMIT_QUEUE	2

#define AT_DMA_HI_ADDR_MASK     0xffffffff00000000ULL
#define AT_DMA_LO_ADDR_MASK     0x00000000ffffffffULL

#define AT_TX_WATCHDOG  (5 * HZ)
#define AT_MAX_INT_WORK		5
#define AT_TWSI_EEPROM_TIMEOUT 	100
#define AT_HW_MAX_IDLE_DELAY 	10
#define AT_SUSPEND_LINK_TIMEOUT 100

#define AT_ASPM_L0S_TIMER	6
#define AT_ASPM_L1_TIMER	12
#define AT_LCKDET_TIMER		12

#define ATL1C_PCIE_L0S_L1_DISABLE 	0x01
#define ATL1C_PCIE_PHY_RESET		0x02

#define ATL1C_ASPM_L0s_ENABLE		0x0001
#define ATL1C_ASPM_L1_ENABLE		0x0002

#define AT_REGS_LEN	(75 * sizeof(u32))
#define AT_EEPROM_LEN 	512

#define ATL1C_GET_DESC(R, i, type)	(&(((type *)((R)->desc))[i]))
#define ATL1C_RFD_DESC(R, i)	ATL1C_GET_DESC(R, i, struct atl1c_rx_free_desc)
#define ATL1C_TPD_DESC(R, i)	ATL1C_GET_DESC(R, i, struct atl1c_tpd_desc)
#define ATL1C_RRD_DESC(R, i)	ATL1C_GET_DESC(R, i, struct atl1c_recv_ret_status)

#define TPD_L4HDR_OFFSET_MASK	0x00FF
#define TPD_L4HDR_OFFSET_SHIFT	0

#define TPD_TCPHDR_OFFSET_MASK	0x00FF
#define TPD_TCPHDR_OFFSET_SHIFT	0

#define TPD_PLOADOFFSET_MASK	0x00FF
#define TPD_PLOADOFFSET_SHIFT	0

#define TPD_CCSUM_EN_MASK	0x0001
#define TPD_CCSUM_EN_SHIFT	8
#define TPD_IP_CSUM_MASK	0x0001
#define TPD_IP_CSUM_SHIFT	9
#define TPD_TCP_CSUM_MASK	0x0001
#define TPD_TCP_CSUM_SHIFT	10
#define TPD_UDP_CSUM_MASK	0x0001
#define TPD_UDP_CSUM_SHIFT	11
#define TPD_LSO_EN_MASK		0x0001	
#define TPD_LSO_EN_SHIFT	12
#define TPD_LSO_VER_MASK	0x0001
#define TPD_LSO_VER_SHIFT	13 	
#define TPD_CON_VTAG_MASK	0x0001
#define TPD_CON_VTAG_SHIFT	14
#define TPD_INS_VTAG_MASK	0x0001
#define TPD_INS_VTAG_SHIFT	15
#define TPD_IPV4_PACKET_MASK	0x0001  
#define TPD_IPV4_PACKET_SHIFT	16
#define TPD_ETH_TYPE_MASK	0x0001
#define TPD_ETH_TYPE_SHIFT	17	

#define TPD_CCSUM_OFFSET_MASK	0x00FF
#define TPD_CCSUM_OFFSET_SHIFT	18
#define TPD_CCSUM_EPAD_MASK	0x0001
#define TPD_CCSUM_EPAD_SHIFT	30

#define TPD_MSS_MASK            0x1FFF
#define TPD_MSS_SHIFT		18

#define TPD_EOP_MASK		0x0001
#define TPD_EOP_SHIFT		31

struct atl1c_tpd_desc {
	__le16	buffer_len; 
	__le16	vlan_tag;
	__le32	word1;
	__le64	buffer_addr;
};

struct atl1c_tpd_ext_desc {
	u32 reservd_0;
	__le32 word1;
	__le32 pkt_len;
	u32 reservd_1;
};
#define RRS_RX_CSUM_MASK	0xFFFF
#define RRS_RX_CSUM_SHIFT	0
#define RRS_RX_RFD_CNT_MASK	0x000F
#define RRS_RX_RFD_CNT_SHIFT	16
#define RRS_RX_RFD_INDEX_MASK	0x0FFF
#define RRS_RX_RFD_INDEX_SHIFT	20

#define RRS_HEAD_LEN_MASK	0x00FF
#define RRS_HEAD_LEN_SHIFT	0
#define RRS_HDS_TYPE_MASK	0x0003
#define RRS_HDS_TYPE_SHIFT	8
#define RRS_CPU_NUM_MASK	0x0003
#define	RRS_CPU_NUM_SHIFT	10
#define RRS_HASH_FLG_MASK	0x000F
#define RRS_HASH_FLG_SHIFT	12

#define RRS_HDS_TYPE_HEAD	1
#define RRS_HDS_TYPE_DATA	2

#define RRS_IS_NO_HDS_TYPE(flag) \
	((((flag) >> (RRS_HDS_TYPE_SHIFT)) & RRS_HDS_TYPE_MASK) == 0)

#define RRS_IS_HDS_HEAD(flag) \
	((((flag) >> (RRS_HDS_TYPE_SHIFT)) & RRS_HDS_TYPE_MASK) == \
			RRS_HDS_TYPE_HEAD)

#define RRS_IS_HDS_DATA(flag) \
	((((flag) >> (RRS_HDS_TYPE_SHIFT)) & RRS_HDS_TYPE_MASK) == \
			RRS_HDS_TYPE_DATA)

#define RRS_PKT_SIZE_MASK	0x3FFF
#define RRS_PKT_SIZE_SHIFT	0
#define RRS_ERR_L4_CSUM_MASK	0x0001
#define RRS_ERR_L4_CSUM_SHIFT	14
#define RRS_ERR_IP_CSUM_MASK	0x0001
#define RRS_ERR_IP_CSUM_SHIFT	15
#define RRS_VLAN_INS_MASK	0x0001
#define RRS_VLAN_INS_SHIFT	16
#define RRS_PROT_ID_MASK	0x0007
#define RRS_PROT_ID_SHIFT	17
#define RRS_RX_ERR_SUM_MASK	0x0001
#define RRS_RX_ERR_SUM_SHIFT	20
#define RRS_RX_ERR_CRC_MASK	0x0001
#define RRS_RX_ERR_CRC_SHIFT	21
#define RRS_RX_ERR_FAE_MASK	0x0001
#define RRS_RX_ERR_FAE_SHIFT	22
#define RRS_RX_ERR_TRUNC_MASK	0x0001
#define RRS_RX_ERR_TRUNC_SHIFT	23
#define RRS_RX_ERR_RUNC_MASK	0x0001
#define RRS_RX_ERR_RUNC_SHIFT	24
#define RRS_RX_ERR_ICMP_MASK	0x0001
#define RRS_RX_ERR_ICMP_SHIFT	25
#define RRS_PACKET_BCAST_MASK	0x0001
#define RRS_PACKET_BCAST_SHIFT	26
#define RRS_PACKET_MCAST_MASK	0x0001
#define RRS_PACKET_MCAST_SHIFT	27
#define RRS_PACKET_TYPE_MASK	0x0001
#define RRS_PACKET_TYPE_SHIFT	28
#define RRS_FIFO_FULL_MASK	0x0001
#define RRS_FIFO_FULL_SHIFT	29
#define RRS_802_3_LEN_ERR_MASK 	0x0001
#define RRS_802_3_LEN_ERR_SHIFT 30
#define RRS_RXD_UPDATED_MASK	0x0001
#define RRS_RXD_UPDATED_SHIFT	31

#define RRS_ERR_L4_CSUM         0x00004000
#define RRS_ERR_IP_CSUM         0x00008000
#define RRS_VLAN_INS            0x00010000
#define RRS_RX_ERR_SUM          0x00100000
#define RRS_RX_ERR_CRC          0x00200000
#define RRS_802_3_LEN_ERR	0x40000000
#define RRS_RXD_UPDATED		0x80000000

#define RRS_PACKET_TYPE_802_3  	1
#define RRS_PACKET_TYPE_ETH	0
#define RRS_PACKET_IS_ETH(word) \
	((((word) >> RRS_PACKET_TYPE_SHIFT) & RRS_PACKET_TYPE_MASK) == \
			RRS_PACKET_TYPE_ETH)
#define RRS_RXD_IS_VALID(word) \
	((((word) >> RRS_RXD_UPDATED_SHIFT) & RRS_RXD_UPDATED_MASK) == 1)

#define RRS_PACKET_PROT_IS_IPV4_ONLY(word) \
	((((word) >> RRS_PROT_ID_SHIFT) & RRS_PROT_ID_MASK) == 1)
#define RRS_PACKET_PROT_IS_IPV6_ONLY(word) \
	((((word) >> RRS_PROT_ID_SHIFT) & RRS_PROT_ID_MASK) == 6)

struct atl1c_recv_ret_status {
	__le32  word0;
	__le32	rss_hash;
	__le16	vlan_tag;
	__le16	flag;
	__le32	word3;
};

struct atl1c_rx_free_desc {
	__le64	buffer_addr;
};

enum atl1c_dma_order {
	atl1c_dma_ord_in = 1,
	atl1c_dma_ord_enh = 2,
	atl1c_dma_ord_out = 4
};

enum atl1c_dma_rcb {
	atl1c_rcb_64 = 0,
	atl1c_rcb_128 = 1
};

enum atl1c_mac_speed {
	atl1c_mac_speed_0 = 0,
	atl1c_mac_speed_10_100 = 1,
	atl1c_mac_speed_1000 = 2
};

enum atl1c_dma_req_block {
	atl1c_dma_req_128 = 0,
	atl1c_dma_req_256 = 1,
	atl1c_dma_req_512 = 2,
	atl1c_dma_req_1024 = 3,
	atl1c_dma_req_2048 = 4,
	atl1c_dma_req_4096 = 5
};

enum atl1c_rss_mode {
	atl1c_rss_mode_disable = 0,
	atl1c_rss_sig_que = 1,
	atl1c_rss_mul_que_sig_int = 2,
	atl1c_rss_mul_que_mul_int = 4,
};

enum atl1c_rss_type {
	atl1c_rss_disable = 0,
	atl1c_rss_ipv4 = 1,
	atl1c_rss_ipv4_tcp = 2,
	atl1c_rss_ipv6 = 4,
	atl1c_rss_ipv6_tcp = 8
};

enum atl1c_nic_type {
	athr_l1c = 0,
	athr_l2c = 1,
	athr_l2c_b,
	athr_l2c_b2,
	athr_l1d,
	athr_l1d_2,
};

enum atl1c_trans_queue {
	atl1c_trans_normal = 0,
	atl1c_trans_high = 1
};

struct atl1c_hw_stats {
	
	unsigned long rx_ok;		
	unsigned long rx_bcast;		
	unsigned long rx_mcast;		
	unsigned long rx_pause;		
	unsigned long rx_ctrl;		
	unsigned long rx_fcs_err;	
	unsigned long rx_len_err;	
	unsigned long rx_byte_cnt;	
	unsigned long rx_runt;		
	unsigned long rx_frag;		
	unsigned long rx_sz_64;		
	unsigned long rx_sz_65_127;	
	unsigned long rx_sz_128_255;	
	unsigned long rx_sz_256_511;	
	unsigned long rx_sz_512_1023;	
	unsigned long rx_sz_1024_1518;	
	unsigned long rx_sz_1519_max;	
	unsigned long rx_sz_ov;		
	unsigned long rx_rxf_ov;	
	unsigned long rx_rrd_ov;	
	unsigned long rx_align_err;	
	unsigned long rx_bcast_byte_cnt; 
	unsigned long rx_mcast_byte_cnt; 
	unsigned long rx_err_addr;	

	
	unsigned long tx_ok;		
	unsigned long tx_bcast;		
	unsigned long tx_mcast;		
	unsigned long tx_pause;		
	unsigned long tx_exc_defer;	
	unsigned long tx_ctrl;		
	unsigned long tx_defer;		
	unsigned long tx_byte_cnt;	
	unsigned long tx_sz_64;		
	unsigned long tx_sz_65_127;	
	unsigned long tx_sz_128_255;	
	unsigned long tx_sz_256_511;	
	unsigned long tx_sz_512_1023;	
	unsigned long tx_sz_1024_1518;	
	unsigned long tx_sz_1519_max;	
	unsigned long tx_1_col;		
	unsigned long tx_2_col;		
	unsigned long tx_late_col;	
	unsigned long tx_abort_col;	
	unsigned long tx_underrun;	
	unsigned long tx_rd_eop;	/* The number of times that read beyond the EOP into the next frame area when TRD was not written timely */
	unsigned long tx_len_err;	
	unsigned long tx_trunc;		
	unsigned long tx_bcast_byte;	
	unsigned long tx_mcast_byte;	
};

struct atl1c_hw {
	u8 __iomem      *hw_addr;            
	struct atl1c_adapter *adapter;
	enum atl1c_nic_type  nic_type;
	enum atl1c_dma_order dma_order;
	enum atl1c_dma_rcb   rcb_value;
	enum atl1c_dma_req_block dmar_block;
	enum atl1c_dma_req_block dmaw_block;

	u16 device_id;
	u16 vendor_id;
	u16 subsystem_id;
	u16 subsystem_vendor_id;
	u8 revision_id;
	u16 phy_id1;
	u16 phy_id2;

	u32 intr_mask;
	u8 dmaw_dly_cnt;
	u8 dmar_dly_cnt;

	u8 preamble_len;
	u16 max_frame_size;
	u16 min_frame_size;

	enum atl1c_mac_speed mac_speed;
	bool mac_duplex;
	bool hibernate;
	u16 media_type;
#define MEDIA_TYPE_AUTO_SENSOR  0
#define MEDIA_TYPE_100M_FULL    1
#define MEDIA_TYPE_100M_HALF    2
#define MEDIA_TYPE_10M_FULL     3
#define MEDIA_TYPE_10M_HALF     4

	u16 autoneg_advertised;
	u16 mii_autoneg_adv_reg;
	u16 mii_1000t_ctrl_reg;

	u16 tx_imt;	
	u16 rx_imt;	
	u16 ict;        
	u16 ctrl_flags;
#define ATL1C_INTR_CLEAR_ON_READ	0x0001
#define ATL1C_INTR_MODRT_ENABLE	 	0x0002
#define ATL1C_CMB_ENABLE		0x0004
#define ATL1C_SMB_ENABLE		0x0010
#define ATL1C_TXQ_MODE_ENHANCE		0x0020
#define ATL1C_RX_IPV6_CHKSUM		0x0040
#define ATL1C_ASPM_L0S_SUPPORT		0x0080
#define ATL1C_ASPM_L1_SUPPORT		0x0100
#define ATL1C_ASPM_CTRL_MON		0x0200
#define ATL1C_HIB_DISABLE		0x0400
#define ATL1C_APS_MODE_ENABLE           0x0800
#define ATL1C_LINK_EXT_SYNC             0x1000
#define ATL1C_CLK_GATING_EN             0x2000
#define ATL1C_FPGA_VERSION              0x8000
	u16 link_cap_flags;
#define ATL1C_LINK_CAP_1000M		0x0001
	u16 cmb_tpd;
	u16 cmb_rrd;
	u16 cmb_rx_timer; 
	u16 cmb_tx_timer;
	u32 smb_timer;

	u16 rrd_thresh; 
	u16 tpd_thresh;
	u8 tpd_burst;   
	u8 rfd_burst;
	enum atl1c_rss_type rss_type;
	enum atl1c_rss_mode rss_mode;
	u8 rss_hash_bits;
	u32 base_cpu;
	u32 indirect_tab;
	u8 mac_addr[ETH_ALEN];
	u8 perm_mac_addr[ETH_ALEN];

	bool phy_configured;
	bool re_autoneg;
	bool emi_ca;
};

struct atl1c_ring_header {
	void *desc;		
	dma_addr_t dma;		
	unsigned int size;	
};

struct atl1c_buffer {
	struct sk_buff *skb;	
	u16 length;		
	u16 flags;		
#define ATL1C_BUFFER_FREE		0x0001
#define ATL1C_BUFFER_BUSY		0x0002
#define ATL1C_BUFFER_STATE_MASK		0x0003

#define ATL1C_PCIMAP_SINGLE		0x0004
#define ATL1C_PCIMAP_PAGE		0x0008
#define ATL1C_PCIMAP_TYPE_MASK		0x000C

#define ATL1C_PCIMAP_TODEVICE		0x0010
#define ATL1C_PCIMAP_FROMDEVICE		0x0020
#define ATL1C_PCIMAP_DIRECTION_MASK	0x0030
	dma_addr_t dma;
};

#define ATL1C_SET_BUFFER_STATE(buff, state) do {	\
	((buff)->flags) &= ~ATL1C_BUFFER_STATE_MASK;	\
	((buff)->flags) |= (state);			\
	} while (0)

#define ATL1C_SET_PCIMAP_TYPE(buff, type, direction) do {	\
	((buff)->flags) &= ~ATL1C_PCIMAP_TYPE_MASK;		\
	((buff)->flags) |= (type);				\
	((buff)->flags) &= ~ATL1C_PCIMAP_DIRECTION_MASK;	\
	((buff)->flags) |= (direction);				\
	} while (0)

struct atl1c_tpd_ring {
	void *desc;		
	dma_addr_t dma;		
	u16 size;		
	u16 count;		
	u16 next_to_use; 	
	atomic_t next_to_clean;
	struct atl1c_buffer *buffer_info;
};

struct atl1c_rfd_ring {
	void *desc;		
	dma_addr_t dma;		
	u16 size;		
	u16 count;		
	u16 next_to_use;
	u16 next_to_clean;
	struct atl1c_buffer *buffer_info;
};

struct atl1c_rrd_ring {
	void *desc;		
	dma_addr_t dma;		
	u16 size;		
	u16 count;		
	u16 next_to_use;
	u16 next_to_clean;
};

struct atl1c_cmb {
	void *cmb;
	dma_addr_t dma;
};

struct atl1c_smb {
	void *smb;
	dma_addr_t dma;
};

struct atl1c_adapter {
	struct net_device   *netdev;
	struct pci_dev      *pdev;
	struct napi_struct  napi;
	struct atl1c_hw        hw;
	struct atl1c_hw_stats  hw_stats;
	struct mii_if_info  mii;    
	u16 rx_buffer_len;

	unsigned long flags;
#define __AT_TESTING        0x0001
#define __AT_RESETTING      0x0002
#define __AT_DOWN           0x0003
	unsigned long work_event;
#define	ATL1C_WORK_EVENT_RESET		0
#define	ATL1C_WORK_EVENT_LINK_CHANGE	1
	u32 msg_enable;

	bool have_msi;
	u32 wol;
	u16 link_speed;
	u16 link_duplex;

	spinlock_t mdio_lock;
	spinlock_t tx_lock;
	atomic_t irq_sem;

	struct work_struct common_task;
	struct timer_list watchdog_timer;
	struct timer_list phy_config_timer;

	
	struct atl1c_ring_header ring_header;
	struct atl1c_tpd_ring tpd_ring[AT_MAX_TRANSMIT_QUEUE];
	struct atl1c_rfd_ring rfd_ring[AT_MAX_RECEIVE_QUEUE];
	struct atl1c_rrd_ring rrd_ring[AT_MAX_RECEIVE_QUEUE];
	struct atl1c_cmb cmb;
	struct atl1c_smb smb;
	int num_rx_queues;
	u32 bd_number;     
};

#define AT_WRITE_REG(a, reg, value) ( \
		writel((value), ((a)->hw_addr + reg)))

#define AT_WRITE_FLUSH(a) (\
		readl((a)->hw_addr))

#define AT_READ_REG(a, reg, pdata) do {					\
		if (unlikely((a)->hibernate)) {				\
			readl((a)->hw_addr + reg);			\
			*(u32 *)pdata = readl((a)->hw_addr + reg);	\
		} else {						\
			*(u32 *)pdata = readl((a)->hw_addr + reg);	\
		}							\
	} while (0)

#define AT_WRITE_REGB(a, reg, value) (\
		writeb((value), ((a)->hw_addr + reg)))

#define AT_READ_REGB(a, reg) (\
		readb((a)->hw_addr + reg))

#define AT_WRITE_REGW(a, reg, value) (\
		writew((value), ((a)->hw_addr + reg)))

#define AT_READ_REGW(a, reg) (\
		readw((a)->hw_addr + reg))

#define AT_WRITE_REG_ARRAY(a, reg, offset, value) ( \
		writel((value), (((a)->hw_addr + reg) + ((offset) << 2))))

#define AT_READ_REG_ARRAY(a, reg, offset) ( \
		readl(((a)->hw_addr + reg) + ((offset) << 2)))

extern char atl1c_driver_name[];
extern char atl1c_driver_version[];

extern void atl1c_reinit_locked(struct atl1c_adapter *adapter);
extern s32 atl1c_reset_hw(struct atl1c_hw *hw);
extern void atl1c_set_ethtool_ops(struct net_device *netdev);
#endif 
