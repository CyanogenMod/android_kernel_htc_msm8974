/*
 * This file is part of wl1271
 *
 * Copyright (C) 1998-2009 Texas Instruments. All rights reserved.
 * Copyright (C) 2009 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __TX_H__
#define __TX_H__

#define TX_HW_BLOCK_SPARE_DEFAULT        1
#define TX_HW_BLOCK_SIZE                 252

#define TX_HW_MGMT_PKT_LIFETIME_TU       2000
#define TX_HW_AP_MODE_PKT_LIFETIME_TU    8000

#define TX_HW_ATTR_SAVE_RETRIES          BIT(0)
#define TX_HW_ATTR_HEADER_PAD            BIT(1)
#define TX_HW_ATTR_SESSION_COUNTER       (BIT(2) | BIT(3) | BIT(4))
#define TX_HW_ATTR_RATE_POLICY           (BIT(5) | BIT(6) | BIT(7) | \
					  BIT(8) | BIT(9))
#define TX_HW_ATTR_LAST_WORD_PAD         (BIT(10) | BIT(11))
#define TX_HW_ATTR_TX_CMPLT_REQ          BIT(12)
#define TX_HW_ATTR_TX_DUMMY_REQ          BIT(13)
#define TX_HW_ATTR_HOST_ENCRYPT          BIT(14)

#define TX_HW_ATTR_OFST_SAVE_RETRIES     0
#define TX_HW_ATTR_OFST_HEADER_PAD       1
#define TX_HW_ATTR_OFST_SESSION_COUNTER  2
#define TX_HW_ATTR_OFST_RATE_POLICY      5
#define TX_HW_ATTR_OFST_LAST_WORD_PAD    10
#define TX_HW_ATTR_OFST_TX_CMPLT_REQ     12

#define TX_HW_RESULT_QUEUE_LEN           16
#define TX_HW_RESULT_QUEUE_LEN_MASK      0xf

#define WL1271_TX_ALIGN_TO 4
#define WL1271_EXTRA_SPACE_TKIP 4
#define WL1271_EXTRA_SPACE_AES  8
#define WL1271_EXTRA_SPACE_MAX  8

#define WL1271_TID_MGMT 7

struct wl127x_tx_mem {
	u8 extra_blocks;
	u8 total_mem_blocks;
} __packed;

struct wl128x_tx_mem {
	u8 total_mem_blocks;
	u8 extra_bytes;
} __packed;

#define WL12XX_BUS_BLOCK_SIZE min(512u,	\
	    (1u << (8 * sizeof(((struct wl128x_tx_mem *) 0)->extra_bytes))))

struct wl1271_tx_hw_descr {
	
	__le16 length;
	union {
		struct wl127x_tx_mem wl127x_mem;
		struct wl128x_tx_mem wl128x_mem;
	} __packed;
	
	__le32 start_time;
	__le16 life_time;
	
	__le16 tx_attr;
	
	u8 id;
	
	u8 tid;
	
	u8 hlid;
	u8 reserved;
} __packed;

enum wl1271_tx_hw_res_status {
	TX_SUCCESS          = 0,
	TX_HW_ERROR         = 1,
	TX_DISABLED         = 2,
	TX_RETRY_EXCEEDED   = 3,
	TX_TIMEOUT          = 4,
	TX_KEY_NOT_FOUND    = 5,
	TX_PEER_NOT_FOUND   = 6,
	TX_SESSION_MISMATCH = 7,
	TX_LINK_NOT_VALID   = 8,
};

struct wl1271_tx_hw_res_descr {
	
	u8 id;
	u8 status;
	
	__le16 medium_usage;
	
	__le32 fw_handling_time;
	__le32 medium_delay;
	
	u8 tx_security_sequence_number_lsb;
	
	u8 ack_failures;
	u8 rate_class_index;
	
	u8 spare;
} __packed;

struct wl1271_tx_hw_res_if {
	__le32 tx_result_fw_counter;
	__le32 tx_result_host_counter;
	struct wl1271_tx_hw_res_descr tx_results_queue[TX_HW_RESULT_QUEUE_LEN];
} __packed;

static inline int wl1271_tx_get_queue(int queue)
{
	switch (queue) {
	case 0:
		return CONF_TX_AC_VO;
	case 1:
		return CONF_TX_AC_VI;
	case 2:
		return CONF_TX_AC_BE;
	case 3:
		return CONF_TX_AC_BK;
	default:
		return CONF_TX_AC_BE;
	}
}

static inline int wl1271_tx_get_mac80211_queue(int queue)
{
	switch (queue) {
	case CONF_TX_AC_VO:
		return 0;
	case CONF_TX_AC_VI:
		return 1;
	case CONF_TX_AC_BE:
		return 2;
	case CONF_TX_AC_BK:
		return 3;
	default:
		return 2;
	}
}

static inline int wl1271_tx_total_queue_count(struct wl1271 *wl)
{
	int i, count = 0;

	for (i = 0; i < NUM_TX_QUEUES; i++)
		count += wl->tx_queue_count[i];

	return count;
}

void wl1271_tx_work(struct work_struct *work);
void wl1271_tx_work_locked(struct wl1271 *wl);
void wl1271_tx_complete(struct wl1271 *wl);
void wl12xx_tx_reset_wlvif(struct wl1271 *wl, struct wl12xx_vif *wlvif);
void wl12xx_tx_reset(struct wl1271 *wl, bool reset_tx_queues);
void wl1271_tx_flush(struct wl1271 *wl);
u8 wl1271_rate_to_idx(int rate, enum ieee80211_band band);
u32 wl1271_tx_enabled_rates_get(struct wl1271 *wl, u32 rate_set,
				enum ieee80211_band rate_band);
u32 wl1271_tx_min_rate_get(struct wl1271 *wl, u32 rate_set);
u8 wl12xx_tx_get_hlid_ap(struct wl1271 *wl, struct wl12xx_vif *wlvif,
			 struct sk_buff *skb);
u8 wl12xx_tx_get_hlid(struct wl1271 *wl, struct wl12xx_vif *wlvif,
		      struct sk_buff *skb);
void wl1271_tx_reset_link_queues(struct wl1271 *wl, u8 hlid);
void wl1271_handle_tx_low_watermark(struct wl1271 *wl);
bool wl12xx_is_dummy_packet(struct wl1271 *wl, struct sk_buff *skb);
void wl12xx_rearm_rx_streaming(struct wl1271 *wl, unsigned long *active_hlids);

void wl1271_free_sta(struct wl1271 *wl, struct wl12xx_vif *wlvif, u8 hlid);
void wl12xx_rearm_tx_watchdog_locked(struct wl1271 *wl);

#endif
