/*******************************************************************************

  Intel(R) 82576 Virtual Function Linux driver
  Copyright(c) 2009 - 2012 Intel Corporation.

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


#ifndef _IGBVF_H_
#define _IGBVF_H_

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>

#include "vf.h"

struct igbvf_info;
struct igbvf_adapter;

#define IGBVF_START_ITR                    488 
#define IGBVF_4K_ITR                       980
#define IGBVF_20K_ITR                      196
#define IGBVF_70K_ITR                       56

enum latency_range {
	lowest_latency = 0,
	low_latency = 1,
	bulk_latency = 2,
	latency_invalid = 255
};


#define IGBVF_INT_MODE_LEGACY           0
#define IGBVF_INT_MODE_MSI              1
#define IGBVF_INT_MODE_MSIX             2

#define IGBVF_DEFAULT_TXD               256
#define IGBVF_MAX_TXD                   4096
#define IGBVF_MIN_TXD                   80

#define IGBVF_DEFAULT_RXD               256
#define IGBVF_MAX_RXD                   4096
#define IGBVF_MIN_RXD                   80

#define IGBVF_MIN_ITR_USECS             10 
#define IGBVF_MAX_ITR_USECS             10000 

#define IGBVF_RX_PTHRESH                16
#define IGBVF_RX_HTHRESH                8
#define IGBVF_RX_WTHRESH                1

#define MAXIMUM_ETHERNET_VLAN_SIZE      1522

#define IGBVF_FC_PAUSE_TIME             0x0680 

#define IGBVF_TX_QUEUE_WAKE             32
#define IGBVF_RX_BUFFER_WRITE           16 

#define AUTO_ALL_MODES                  0
#define IGBVF_EEPROM_APME               0x0400

#define IGBVF_MNG_VLAN_NONE             (-1)

#define PS_PAGE_BUFFERS                 (MAX_PS_BUFFERS - 1)

enum igbvf_boards {
	board_vf,
	board_i350_vf,
};

struct igbvf_queue_stats {
	u64 packets;
	u64 bytes;
};

struct igbvf_buffer {
	dma_addr_t dma;
	struct sk_buff *skb;
	union {
		
		struct {
			unsigned long time_stamp;
			u16 length;
			u16 next_to_watch;
			u16 mapped_as_page;
		};
		
		struct {
			struct page *page;
			u64 page_dma;
			unsigned int page_offset;
		};
	};
};

union igbvf_desc {
	union e1000_adv_rx_desc rx_desc;
	union e1000_adv_tx_desc tx_desc;
	struct e1000_adv_tx_context_desc tx_context_desc;
};

struct igbvf_ring {
	struct igbvf_adapter *adapter;  
	union igbvf_desc *desc;         
	dma_addr_t dma;                 
	unsigned int size;              
	unsigned int count;             

	u16 next_to_use;
	u16 next_to_clean;

	u16 head;
	u16 tail;

	
	struct igbvf_buffer *buffer_info;
	struct napi_struct napi;

	char name[IFNAMSIZ + 5];
	u32 eims_value;
	u32 itr_val;
	enum latency_range itr_range;
	u16 itr_register;
	int set_itr;

	struct sk_buff *rx_skb_top;

	struct igbvf_queue_stats stats;
};

struct igbvf_adapter {
	struct timer_list watchdog_timer;
	struct timer_list blink_timer;

	struct work_struct reset_task;
	struct work_struct watchdog_task;

	const struct igbvf_info *ei;

	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
	u32 bd_number;
	u32 rx_buffer_len;
	u32 polling_interval;
	u16 mng_vlan_id;
	u16 link_speed;
	u16 link_duplex;

	spinlock_t tx_queue_lock; 

	
	unsigned long state;

	
	u32 requested_itr; 
	u32 current_itr; 

	struct igbvf_ring *tx_ring 
	____cacheline_aligned_in_smp;

	unsigned int restart_queue;
	u32 txd_cmd;

	u32 tx_int_delay;
	u32 tx_abs_int_delay;

	unsigned int total_tx_bytes;
	unsigned int total_tx_packets;
	unsigned int total_rx_bytes;
	unsigned int total_rx_packets;

	
	u32 tx_timeout_count;
	u32 tx_fifo_head;
	u32 tx_head_addr;
	u32 tx_fifo_size;
	u32 tx_dma_failed;

	struct igbvf_ring *rx_ring;

	u32 rx_int_delay;
	u32 rx_abs_int_delay;

	
	u64 hw_csum_err;
	u64 hw_csum_good;
	u64 rx_hdr_split;
	u32 alloc_rx_buff_failed;
	u32 rx_dma_failed;

	unsigned int rx_ps_hdr_size;
	u32 max_frame_size;
	u32 min_frame_size;

	
	struct net_device *netdev;
	struct pci_dev *pdev;
	struct net_device_stats net_stats;
	spinlock_t stats_lock;      

	
	struct e1000_hw hw;

	struct e1000_vf_stats stats;
	u64 zero_base;

	struct igbvf_ring test_tx_ring;
	struct igbvf_ring test_rx_ring;
	u32 test_icr;

	u32 msg_enable;
	struct msix_entry *msix_entries;
	int int_mode;
	u32 eims_enable_mask;
	u32 eims_other;
	u32 int_counter0;
	u32 int_counter1;

	u32 eeprom_wol;
	u32 wol;
	u32 pba;

	bool fc_autoneg;

	unsigned long led_status;

	unsigned int flags;
	unsigned long last_reset;
};

struct igbvf_info {
	enum e1000_mac_type     mac;
	unsigned int            flags;
	u32                     pba;
	void                    (*init_ops)(struct e1000_hw *);
	s32                     (*get_variants)(struct igbvf_adapter *);
};

#define IGBVF_FLAG_RX_CSUM_DISABLED             (1 << 0)

#define IGBVF_RX_DESC_ADV(R, i)     \
	(&((((R).desc))[i].rx_desc))
#define IGBVF_TX_DESC_ADV(R, i)     \
	(&((((R).desc))[i].tx_desc))
#define IGBVF_TX_CTXTDESC_ADV(R, i) \
	(&((((R).desc))[i].tx_context_desc))

enum igbvf_state_t {
	__IGBVF_TESTING,
	__IGBVF_RESETTING,
	__IGBVF_DOWN
};

extern char igbvf_driver_name[];
extern const char igbvf_driver_version[];

extern void igbvf_check_options(struct igbvf_adapter *);
extern void igbvf_set_ethtool_ops(struct net_device *);

extern int igbvf_up(struct igbvf_adapter *);
extern void igbvf_down(struct igbvf_adapter *);
extern void igbvf_reinit_locked(struct igbvf_adapter *);
extern int igbvf_setup_rx_resources(struct igbvf_adapter *, struct igbvf_ring *);
extern int igbvf_setup_tx_resources(struct igbvf_adapter *, struct igbvf_ring *);
extern void igbvf_free_rx_resources(struct igbvf_ring *);
extern void igbvf_free_tx_resources(struct igbvf_ring *);
extern void igbvf_update_stats(struct igbvf_adapter *);

extern unsigned int copybreak;

#endif 
