/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

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

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IXGBE_H_
#define _IXGBE_H_

#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/cpumask.h>
#include <linux/aer.h>
#include <linux/if_vlan.h>

#include "ixgbe_type.h"
#include "ixgbe_common.h"
#include "ixgbe_dcb.h"
#if defined(CONFIG_FCOE) || defined(CONFIG_FCOE_MODULE)
#define IXGBE_FCOE
#include "ixgbe_fcoe.h"
#endif 
#ifdef CONFIG_IXGBE_DCA
#include <linux/dca.h>
#endif

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define IXGBE_DEFAULT_TXD		    512
#define IXGBE_DEFAULT_TX_WORK		    256
#define IXGBE_MAX_TXD			   4096
#define IXGBE_MIN_TXD			     64

#define IXGBE_DEFAULT_RXD		    512
#define IXGBE_MAX_RXD			   4096
#define IXGBE_MIN_RXD			     64

#define IXGBE_MIN_FCRTL			   0x40
#define IXGBE_MAX_FCRTL			0x7FF80
#define IXGBE_MIN_FCRTH			  0x600
#define IXGBE_MAX_FCRTH			0x7FFF0
#define IXGBE_DEFAULT_FCPAUSE		 0xFFFF
#define IXGBE_MIN_FCPAUSE		      0
#define IXGBE_MAX_FCPAUSE		 0xFFFF

#define IXGBE_RXBUFFER_512   512    
#define IXGBE_MAX_RXBUFFER  16384  

#define IXGBE_RX_HDR_SIZE IXGBE_RXBUFFER_512

#define MAXIMUM_ETHERNET_VLAN_SIZE (ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)

#define IXGBE_RX_BUFFER_WRITE	16	

#define IXGBE_TX_FLAGS_CSUM		(u32)(1)
#define IXGBE_TX_FLAGS_HW_VLAN		(u32)(1 << 1)
#define IXGBE_TX_FLAGS_SW_VLAN		(u32)(1 << 2)
#define IXGBE_TX_FLAGS_TSO		(u32)(1 << 3)
#define IXGBE_TX_FLAGS_IPV4		(u32)(1 << 4)
#define IXGBE_TX_FLAGS_FCOE		(u32)(1 << 5)
#define IXGBE_TX_FLAGS_FSO		(u32)(1 << 6)
#define IXGBE_TX_FLAGS_TXSW		(u32)(1 << 7)
#define IXGBE_TX_FLAGS_VLAN_MASK	0xffff0000
#define IXGBE_TX_FLAGS_VLAN_PRIO_MASK	0xe0000000
#define IXGBE_TX_FLAGS_VLAN_PRIO_SHIFT  29
#define IXGBE_TX_FLAGS_VLAN_SHIFT	16

#define IXGBE_MAX_VF_MC_ENTRIES         30
#define IXGBE_MAX_VF_FUNCTIONS          64
#define IXGBE_MAX_VFTA_ENTRIES          128
#define MAX_EMULATION_MAC_ADDRS         16
#define IXGBE_MAX_PF_MACVLANS           15
#define VMDQ_P(p)   ((p) + adapter->num_vfs)
#define IXGBE_82599_VF_DEVICE_ID        0x10ED
#define IXGBE_X540_VF_DEVICE_ID         0x1515

struct vf_data_storage {
	unsigned char vf_mac_addresses[ETH_ALEN];
	u16 vf_mc_hashes[IXGBE_MAX_VF_MC_ENTRIES];
	u16 num_vf_mc_hashes;
	u16 default_vf_vlan_id;
	u16 vlans_enabled;
	bool clear_to_send;
	bool pf_set_mac;
	u16 pf_vlan; 
	u16 pf_qos;
	u16 tx_rate;
	u16 vlan_count;
	u8 spoofchk_enabled;
	struct pci_dev *vfdev;
};

struct vf_macvlans {
	struct list_head l;
	int vf;
	int rar_entry;
	bool free;
	bool is_macvlan;
	u8 vf_macvlan[ETH_ALEN];
};

#define IXGBE_MAX_TXD_PWR	14
#define IXGBE_MAX_DATA_PER_TXD	(1 << IXGBE_MAX_TXD_PWR)

#define TXD_USE_COUNT(S) DIV_ROUND_UP((S), IXGBE_MAX_DATA_PER_TXD)
#define DESC_NEEDED ((MAX_SKB_FRAGS * TXD_USE_COUNT(PAGE_SIZE)) + 4)

struct ixgbe_tx_buffer {
	union ixgbe_adv_tx_desc *next_to_watch;
	unsigned long time_stamp;
	struct sk_buff *skb;
	unsigned int bytecount;
	unsigned short gso_segs;
	__be16 protocol;
	DEFINE_DMA_UNMAP_ADDR(dma);
	DEFINE_DMA_UNMAP_LEN(len);
	u32 tx_flags;
};

struct ixgbe_rx_buffer {
	struct sk_buff *skb;
	dma_addr_t dma;
	struct page *page;
	unsigned int page_offset;
};

struct ixgbe_queue_stats {
	u64 packets;
	u64 bytes;
};

struct ixgbe_tx_queue_stats {
	u64 restart_queue;
	u64 tx_busy;
	u64 tx_done_old;
};

struct ixgbe_rx_queue_stats {
	u64 rsc_count;
	u64 rsc_flush;
	u64 non_eop_descs;
	u64 alloc_rx_page_failed;
	u64 alloc_rx_buff_failed;
	u64 csum_err;
};

enum ixgbe_ring_state_t {
	__IXGBE_TX_FDIR_INIT_DONE,
	__IXGBE_TX_DETECT_HANG,
	__IXGBE_HANG_CHECK_ARMED,
	__IXGBE_RX_RSC_ENABLED,
	__IXGBE_RX_CSUM_UDP_ZERO_ERR,
	__IXGBE_RX_FCOE_BUFSZ,
};

#define check_for_tx_hang(ring) \
	test_bit(__IXGBE_TX_DETECT_HANG, &(ring)->state)
#define set_check_for_tx_hang(ring) \
	set_bit(__IXGBE_TX_DETECT_HANG, &(ring)->state)
#define clear_check_for_tx_hang(ring) \
	clear_bit(__IXGBE_TX_DETECT_HANG, &(ring)->state)
#define ring_is_rsc_enabled(ring) \
	test_bit(__IXGBE_RX_RSC_ENABLED, &(ring)->state)
#define set_ring_rsc_enabled(ring) \
	set_bit(__IXGBE_RX_RSC_ENABLED, &(ring)->state)
#define clear_ring_rsc_enabled(ring) \
	clear_bit(__IXGBE_RX_RSC_ENABLED, &(ring)->state)
struct ixgbe_ring {
	struct ixgbe_ring *next;	
	struct ixgbe_q_vector *q_vector; 
	struct net_device *netdev;	
	struct device *dev;		
	void *desc;			
	union {
		struct ixgbe_tx_buffer *tx_buffer_info;
		struct ixgbe_rx_buffer *rx_buffer_info;
	};
	unsigned long state;
	u8 __iomem *tail;
	dma_addr_t dma;			
	unsigned int size;		

	u16 count;			

	u8 queue_index; 
	u8 reg_idx;			
	u16 next_to_use;
	u16 next_to_clean;

	union {
		u16 next_to_alloc;
		struct {
			u8 atr_sample_rate;
			u8 atr_count;
		};
	};

	u8 dcb_tc;
	struct ixgbe_queue_stats stats;
	struct u64_stats_sync syncp;
	union {
		struct ixgbe_tx_queue_stats tx_stats;
		struct ixgbe_rx_queue_stats rx_stats;
	};
} ____cacheline_internodealigned_in_smp;

enum ixgbe_ring_f_enum {
	RING_F_NONE = 0,
	RING_F_VMDQ,  
	RING_F_RSS,
	RING_F_FDIR,
#ifdef IXGBE_FCOE
	RING_F_FCOE,
#endif 

	RING_F_ARRAY_SIZE      
};

#define IXGBE_MAX_RSS_INDICES  16
#define IXGBE_MAX_VMDQ_INDICES 64
#define IXGBE_MAX_FDIR_INDICES 64
#ifdef IXGBE_FCOE
#define IXGBE_MAX_FCOE_INDICES  8
#define MAX_RX_QUEUES (IXGBE_MAX_FDIR_INDICES + IXGBE_MAX_FCOE_INDICES)
#define MAX_TX_QUEUES (IXGBE_MAX_FDIR_INDICES + IXGBE_MAX_FCOE_INDICES)
#else
#define MAX_RX_QUEUES IXGBE_MAX_FDIR_INDICES
#define MAX_TX_QUEUES IXGBE_MAX_FDIR_INDICES
#endif 
struct ixgbe_ring_feature {
	int indices;
	int mask;
} ____cacheline_internodealigned_in_smp;

#if defined(IXGBE_FCOE) && (PAGE_SIZE < 8192)
static inline unsigned int ixgbe_rx_pg_order(struct ixgbe_ring *ring)
{
	return test_bit(__IXGBE_RX_FCOE_BUFSZ, &ring->state) ? 1 : 0;
}
#else
#define ixgbe_rx_pg_order(_ring) 0
#endif
#define ixgbe_rx_pg_size(_ring) (PAGE_SIZE << ixgbe_rx_pg_order(_ring))
#define ixgbe_rx_bufsz(_ring) ((PAGE_SIZE / 2) << ixgbe_rx_pg_order(_ring))

struct ixgbe_ring_container {
	struct ixgbe_ring *ring;	
	unsigned int total_bytes;	
	unsigned int total_packets;	
	u16 work_limit;			
	u8 count;			
	u8 itr;				
};

#define ixgbe_for_each_ring(pos, head) \
	for (pos = (head).ring; pos != NULL; pos = pos->next)

#define MAX_RX_PACKET_BUFFERS ((adapter->flags & IXGBE_FLAG_DCB_ENABLED) \
                              ? 8 : 1)
#define MAX_TX_PACKET_BUFFERS MAX_RX_PACKET_BUFFERS

struct ixgbe_q_vector {
	struct ixgbe_adapter *adapter;
#ifdef CONFIG_IXGBE_DCA
	int cpu;	    
#endif
	u16 v_idx;		
	u16 itr;		/* Interrupt throttle rate written to EITR */
	struct ixgbe_ring_container rx, tx;

	struct napi_struct napi;
	cpumask_t affinity_mask;
	int numa_node;
	struct rcu_head rcu;	
	char name[IFNAMSIZ + 9];

	
	struct ixgbe_ring ring[0] ____cacheline_internodealigned_in_smp;
};

#define IXGBE_MIN_RSC_ITR	24
#define IXGBE_100K_ITR		40
#define IXGBE_20K_ITR		200
#define IXGBE_10K_ITR		400
#define IXGBE_8K_ITR		500

static inline __le32 ixgbe_test_staterr(union ixgbe_adv_rx_desc *rx_desc,
					const u32 stat_err_bits)
{
	return rx_desc->wb.upper.status_error & cpu_to_le32(stat_err_bits);
}

static inline u16 ixgbe_desc_unused(struct ixgbe_ring *ring)
{
	u16 ntc = ring->next_to_clean;
	u16 ntu = ring->next_to_use;

	return ((ntc > ntu) ? 0 : ring->count) + ntc - ntu - 1;
}

#define IXGBE_RX_DESC(R, i)	    \
	(&(((union ixgbe_adv_rx_desc *)((R)->desc))[i]))
#define IXGBE_TX_DESC(R, i)	    \
	(&(((union ixgbe_adv_tx_desc *)((R)->desc))[i]))
#define IXGBE_TX_CTXTDESC(R, i)	    \
	(&(((struct ixgbe_adv_tx_context_desc *)((R)->desc))[i]))

#define IXGBE_MAX_JUMBO_FRAME_SIZE        16128
#ifdef IXGBE_FCOE
#define IXGBE_FCOE_JUMBO_FRAME_SIZE       3072
#endif 

#define OTHER_VECTOR 1
#define NON_Q_VECTORS (OTHER_VECTOR)

#define MAX_MSIX_VECTORS_82599 64
#define MAX_MSIX_Q_VECTORS_82599 64
#define MAX_MSIX_VECTORS_82598 18
#define MAX_MSIX_Q_VECTORS_82598 16

#define MAX_MSIX_Q_VECTORS MAX_MSIX_Q_VECTORS_82599
#define MAX_MSIX_COUNT MAX_MSIX_VECTORS_82599

#define MIN_MSIX_Q_VECTORS 1
#define MIN_MSIX_COUNT (MIN_MSIX_Q_VECTORS + NON_Q_VECTORS)

#define IXGBE_TRY_LINK_TIMEOUT (4 * HZ)

struct ixgbe_adapter {
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
	
	struct net_device *netdev;
	struct pci_dev *pdev;

	unsigned long state;

	u32 flags;
#define IXGBE_FLAG_MSI_CAPABLE                  (u32)(1 << 1)
#define IXGBE_FLAG_MSI_ENABLED                  (u32)(1 << 2)
#define IXGBE_FLAG_MSIX_CAPABLE                 (u32)(1 << 3)
#define IXGBE_FLAG_MSIX_ENABLED                 (u32)(1 << 4)
#define IXGBE_FLAG_RX_1BUF_CAPABLE              (u32)(1 << 6)
#define IXGBE_FLAG_RX_PS_CAPABLE                (u32)(1 << 7)
#define IXGBE_FLAG_RX_PS_ENABLED                (u32)(1 << 8)
#define IXGBE_FLAG_IN_NETPOLL                   (u32)(1 << 9)
#define IXGBE_FLAG_DCA_ENABLED                  (u32)(1 << 10)
#define IXGBE_FLAG_DCA_CAPABLE                  (u32)(1 << 11)
#define IXGBE_FLAG_IMIR_ENABLED                 (u32)(1 << 12)
#define IXGBE_FLAG_MQ_CAPABLE                   (u32)(1 << 13)
#define IXGBE_FLAG_DCB_ENABLED                  (u32)(1 << 14)
#define IXGBE_FLAG_RSS_ENABLED                  (u32)(1 << 16)
#define IXGBE_FLAG_RSS_CAPABLE                  (u32)(1 << 17)
#define IXGBE_FLAG_VMDQ_CAPABLE                 (u32)(1 << 18)
#define IXGBE_FLAG_VMDQ_ENABLED                 (u32)(1 << 19)
#define IXGBE_FLAG_FAN_FAIL_CAPABLE             (u32)(1 << 20)
#define IXGBE_FLAG_NEED_LINK_UPDATE             (u32)(1 << 22)
#define IXGBE_FLAG_NEED_LINK_CONFIG             (u32)(1 << 23)
#define IXGBE_FLAG_FDIR_HASH_CAPABLE            (u32)(1 << 24)
#define IXGBE_FLAG_FDIR_PERFECT_CAPABLE         (u32)(1 << 25)
#define IXGBE_FLAG_FCOE_CAPABLE                 (u32)(1 << 26)
#define IXGBE_FLAG_FCOE_ENABLED                 (u32)(1 << 27)
#define IXGBE_FLAG_SRIOV_CAPABLE                (u32)(1 << 28)
#define IXGBE_FLAG_SRIOV_ENABLED                (u32)(1 << 29)

	u32 flags2;
#define IXGBE_FLAG2_RSC_CAPABLE                 (u32)(1)
#define IXGBE_FLAG2_RSC_ENABLED                 (u32)(1 << 1)
#define IXGBE_FLAG2_TEMP_SENSOR_CAPABLE         (u32)(1 << 2)
#define IXGBE_FLAG2_TEMP_SENSOR_EVENT           (u32)(1 << 3)
#define IXGBE_FLAG2_SEARCH_FOR_SFP              (u32)(1 << 4)
#define IXGBE_FLAG2_SFP_NEEDS_RESET             (u32)(1 << 5)
#define IXGBE_FLAG2_RESET_REQUESTED             (u32)(1 << 6)
#define IXGBE_FLAG2_FDIR_REQUIRES_REINIT        (u32)(1 << 7)
#define IXGBE_FLAG2_RSS_FIELD_IPV4_UDP		(u32)(1 << 8)
#define IXGBE_FLAG2_RSS_FIELD_IPV6_UDP		(u32)(1 << 9)

	
	int num_tx_queues;
	u16 tx_itr_setting;
	u16 tx_work_limit;

	
	int num_rx_queues;
	u16 rx_itr_setting;

	
	struct ixgbe_ring *tx_ring[MAX_TX_QUEUES] ____cacheline_aligned_in_smp;

	u64 restart_queue;
	u64 lsc_int;
	u32 tx_timeout_count;

	
	struct ixgbe_ring *rx_ring[MAX_RX_QUEUES];
	int num_rx_pools;		
	int num_rx_queues_per_pool;	
	u64 hw_csum_rx_error;
	u64 hw_rx_no_dma_resources;
	u64 rsc_total_count;
	u64 rsc_total_flush;
	u64 non_eop_descs;
	u32 alloc_rx_page_failed;
	u32 alloc_rx_buff_failed;

	struct ixgbe_q_vector *q_vector[MAX_MSIX_Q_VECTORS];

	
	struct ieee_pfc *ixgbe_ieee_pfc;
	struct ieee_ets *ixgbe_ieee_ets;
	struct ixgbe_dcb_config dcb_cfg;
	struct ixgbe_dcb_config temp_dcb_cfg;
	u8 dcb_set_bitmap;
	u8 dcbx_cap;
	enum ixgbe_fc_mode last_lfc_mode;

	int num_msix_vectors;
	int max_msix_q_vectors;         
	struct ixgbe_ring_feature ring_feature[RING_F_ARRAY_SIZE];
	struct msix_entry *msix_entries;

	u32 test_icr;
	struct ixgbe_ring test_tx_ring;
	struct ixgbe_ring test_rx_ring;

	
	struct ixgbe_hw hw;
	u16 msg_enable;
	struct ixgbe_hw_stats stats;

	u64 tx_busy;
	unsigned int tx_ring_count;
	unsigned int rx_ring_count;

	u32 link_speed;
	bool link_up;
	unsigned long link_check_timeout;

	struct timer_list service_timer;
	struct work_struct service_task;

	struct hlist_head fdir_filter_list;
	unsigned long fdir_overflow; 
	union ixgbe_atr_input fdir_mask;
	int fdir_filter_count;
	u32 fdir_pballoc;
	u32 atr_sample_rate;
	spinlock_t fdir_perfect_lock;

#ifdef IXGBE_FCOE
	struct ixgbe_fcoe fcoe;
#endif 
	u32 wol;

	u16 bd_number;

	u16 eeprom_verh;
	u16 eeprom_verl;
	u16 eeprom_cap;

	u32 interrupt_event;
	u32 led_reg;

	
	DECLARE_BITMAP(active_vfs, IXGBE_MAX_VF_FUNCTIONS);
	unsigned int num_vfs;
	struct vf_data_storage *vfinfo;
	int vf_rate_link_speed;
	struct vf_macvlans vf_mvs;
	struct vf_macvlans *mv_list;

	u32 timer_event_accumulator;
	u32 vferr_refcount;
};

struct ixgbe_fdir_filter {
	struct hlist_node fdir_node;
	union ixgbe_atr_input filter;
	u16 sw_idx;
	u16 action;
};

enum ixgbe_state_t {
	__IXGBE_TESTING,
	__IXGBE_RESETTING,
	__IXGBE_DOWN,
	__IXGBE_SERVICE_SCHED,
	__IXGBE_IN_SFP_INIT,
};

struct ixgbe_cb {
	union {				
		struct sk_buff *head;
		struct sk_buff *tail;
	};
	dma_addr_t dma;
	u16 append_cnt;
	bool page_released;
};
#define IXGBE_CB(skb) ((struct ixgbe_cb *)(skb)->cb)

enum ixgbe_boards {
	board_82598,
	board_82599,
	board_X540,
};

extern struct ixgbe_info ixgbe_82598_info;
extern struct ixgbe_info ixgbe_82599_info;
extern struct ixgbe_info ixgbe_X540_info;
#ifdef CONFIG_IXGBE_DCB
extern const struct dcbnl_rtnl_ops dcbnl_ops;
#endif

extern char ixgbe_driver_name[];
extern const char ixgbe_driver_version[];
#ifdef IXGBE_FCOE
extern char ixgbe_default_device_descr[];
#endif 

extern void ixgbe_up(struct ixgbe_adapter *adapter);
extern void ixgbe_down(struct ixgbe_adapter *adapter);
extern void ixgbe_reinit_locked(struct ixgbe_adapter *adapter);
extern void ixgbe_reset(struct ixgbe_adapter *adapter);
extern void ixgbe_set_ethtool_ops(struct net_device *netdev);
extern int ixgbe_setup_rx_resources(struct ixgbe_ring *);
extern int ixgbe_setup_tx_resources(struct ixgbe_ring *);
extern void ixgbe_free_rx_resources(struct ixgbe_ring *);
extern void ixgbe_free_tx_resources(struct ixgbe_ring *);
extern void ixgbe_configure_rx_ring(struct ixgbe_adapter *,struct ixgbe_ring *);
extern void ixgbe_configure_tx_ring(struct ixgbe_adapter *,struct ixgbe_ring *);
extern void ixgbe_disable_rx_queue(struct ixgbe_adapter *adapter,
				   struct ixgbe_ring *);
extern void ixgbe_update_stats(struct ixgbe_adapter *adapter);
extern int ixgbe_init_interrupt_scheme(struct ixgbe_adapter *adapter);
extern void ixgbe_clear_interrupt_scheme(struct ixgbe_adapter *adapter);
extern netdev_tx_t ixgbe_xmit_frame_ring(struct sk_buff *,
					 struct ixgbe_adapter *,
					 struct ixgbe_ring *);
extern void ixgbe_unmap_and_free_tx_resource(struct ixgbe_ring *,
                                             struct ixgbe_tx_buffer *);
extern void ixgbe_alloc_rx_buffers(struct ixgbe_ring *, u16);
extern void ixgbe_write_eitr(struct ixgbe_q_vector *);
extern int ixgbe_poll(struct napi_struct *napi, int budget);
extern int ethtool_ioctl(struct ifreq *ifr);
extern s32 ixgbe_reinit_fdir_tables_82599(struct ixgbe_hw *hw);
extern s32 ixgbe_init_fdir_signature_82599(struct ixgbe_hw *hw, u32 fdirctrl);
extern s32 ixgbe_init_fdir_perfect_82599(struct ixgbe_hw *hw, u32 fdirctrl);
extern s32 ixgbe_fdir_add_signature_filter_82599(struct ixgbe_hw *hw,
						 union ixgbe_atr_hash_dword input,
						 union ixgbe_atr_hash_dword common,
                                                 u8 queue);
extern s32 ixgbe_fdir_set_input_mask_82599(struct ixgbe_hw *hw,
					   union ixgbe_atr_input *input_mask);
extern s32 ixgbe_fdir_write_perfect_filter_82599(struct ixgbe_hw *hw,
						 union ixgbe_atr_input *input,
						 u16 soft_id, u8 queue);
extern s32 ixgbe_fdir_erase_perfect_filter_82599(struct ixgbe_hw *hw,
						 union ixgbe_atr_input *input,
						 u16 soft_id);
extern void ixgbe_atr_compute_perfect_hash_82599(union ixgbe_atr_input *input,
						 union ixgbe_atr_input *mask);
extern void ixgbe_set_rx_mode(struct net_device *netdev);
#ifdef CONFIG_IXGBE_DCB
extern int ixgbe_setup_tc(struct net_device *dev, u8 tc);
#endif
extern void ixgbe_tx_ctxtdesc(struct ixgbe_ring *, u32, u32, u32, u32);
extern void ixgbe_do_reset(struct net_device *netdev);
#ifdef IXGBE_FCOE
extern void ixgbe_configure_fcoe(struct ixgbe_adapter *adapter);
extern int ixgbe_fso(struct ixgbe_ring *tx_ring,
		     struct ixgbe_tx_buffer *first,
		     u8 *hdr_len);
extern void ixgbe_cleanup_fcoe(struct ixgbe_adapter *adapter);
extern int ixgbe_fcoe_ddp(struct ixgbe_adapter *adapter,
			  union ixgbe_adv_rx_desc *rx_desc,
			  struct sk_buff *skb);
extern int ixgbe_fcoe_ddp_get(struct net_device *netdev, u16 xid,
                              struct scatterlist *sgl, unsigned int sgc);
extern int ixgbe_fcoe_ddp_target(struct net_device *netdev, u16 xid,
				 struct scatterlist *sgl, unsigned int sgc);
extern int ixgbe_fcoe_ddp_put(struct net_device *netdev, u16 xid);
extern int ixgbe_fcoe_enable(struct net_device *netdev);
extern int ixgbe_fcoe_disable(struct net_device *netdev);
#ifdef CONFIG_IXGBE_DCB
extern u8 ixgbe_fcoe_getapp(struct ixgbe_adapter *adapter);
extern u8 ixgbe_fcoe_setapp(struct ixgbe_adapter *adapter, u8 up);
#endif 
extern int ixgbe_fcoe_get_wwn(struct net_device *netdev, u64 *wwn, int type);
extern int ixgbe_fcoe_get_hbainfo(struct net_device *netdev,
				  struct netdev_fcoe_hbainfo *info);
#endif 

static inline struct netdev_queue *txring_txq(const struct ixgbe_ring *ring)
{
	return netdev_get_tx_queue(ring->netdev, ring->queue_index);
}

#endif 
