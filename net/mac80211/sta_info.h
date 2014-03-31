/*
 * Copyright 2002-2005, Devicescape Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef STA_INFO_H
#define STA_INFO_H

#include <linux/list.h>
#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/workqueue.h>
#include <linux/average.h>
#include <linux/etherdevice.h>
#include "key.h"

enum ieee80211_sta_info_flags {
	WLAN_STA_AUTH,
	WLAN_STA_ASSOC,
	WLAN_STA_PS_STA,
	WLAN_STA_AUTHORIZED,
	WLAN_STA_SHORT_PREAMBLE,
	WLAN_STA_WME,
	WLAN_STA_WDS,
	WLAN_STA_CLEAR_PS_FILT,
	WLAN_STA_MFP,
	WLAN_STA_BLOCK_BA,
	WLAN_STA_PS_DRIVER,
	WLAN_STA_PSPOLL,
	WLAN_STA_TDLS_PEER,
	WLAN_STA_TDLS_PEER_AUTH,
	WLAN_STA_UAPSD,
	WLAN_STA_SP,
	WLAN_STA_4ADDR_EVENT,
	WLAN_STA_INSERTED,
	WLAN_STA_RATE_CONTROL,
};

#define STA_TID_NUM 16
#define ADDBA_RESP_INTERVAL HZ
#define HT_AGG_MAX_RETRIES		15
#define HT_AGG_BURST_RETRIES		3
#define HT_AGG_RETRIES_PERIOD		(15 * HZ)

#define HT_AGG_STATE_DRV_READY		0
#define HT_AGG_STATE_RESPONSE_RECEIVED	1
#define HT_AGG_STATE_OPERATIONAL	2
#define HT_AGG_STATE_STOPPING		3
#define HT_AGG_STATE_WANT_START		4
#define HT_AGG_STATE_WANT_STOP		5

struct tid_ampdu_tx {
	struct rcu_head rcu_head;
	struct timer_list session_timer;
	struct timer_list addba_resp_timer;
	struct sk_buff_head pending;
	unsigned long state;
	u16 timeout;
	u8 dialog_token;
	u8 stop_initiator;
	bool tx_stop;
	u8 buf_size;

	u16 failed_bar_ssn;
	bool bar_pending;
};

struct tid_ampdu_rx {
	struct rcu_head rcu_head;
	spinlock_t reorder_lock;
	struct sk_buff **reorder_buf;
	unsigned long *reorder_time;
	struct timer_list session_timer;
	struct timer_list reorder_timer;
	u16 head_seq_num;
	u16 stored_mpdu_num;
	u16 ssn;
	u16 buf_size;
	u16 timeout;
	u8 dialog_token;
};

struct sta_ampdu_mlme {
	struct mutex mtx;
	
	struct tid_ampdu_rx __rcu *tid_rx[STA_TID_NUM];
	unsigned long tid_rx_timer_expired[BITS_TO_LONGS(STA_TID_NUM)];
	unsigned long tid_rx_stop_requested[BITS_TO_LONGS(STA_TID_NUM)];
	
	struct work_struct work;
	struct tid_ampdu_tx __rcu *tid_tx[STA_TID_NUM];
	struct tid_ampdu_tx *tid_start_tx[STA_TID_NUM];
	unsigned long last_addba_req_time[STA_TID_NUM];
	u8 addba_req_num[STA_TID_NUM];
	u8 dialog_token_allocator;
};


struct sta_info {
	
	struct list_head list;
	struct sta_info __rcu *hnext;
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct ieee80211_key __rcu *gtk[NUM_DEFAULT_KEYS + NUM_DEFAULT_MGMT_KEYS];
	struct ieee80211_key __rcu *ptk;
	struct rate_control_ref *rate_ctrl;
	void *rate_ctrl_priv;
	spinlock_t lock;

	struct work_struct drv_unblock_wk;

	u16 listen_interval;

	bool dead;

	bool uploaded;

	enum ieee80211_sta_state sta_state;

	
	unsigned long _flags;

	struct sk_buff_head ps_tx_buf[IEEE80211_NUM_ACS];
	struct sk_buff_head tx_filtered[IEEE80211_NUM_ACS];
	unsigned long driver_buffered_tids;

	
	unsigned long rx_packets, rx_bytes;
	unsigned long wep_weak_iv_count;
	unsigned long last_rx;
	long last_connected;
	unsigned long num_duplicates;
	unsigned long rx_fragments;
	unsigned long rx_dropped;
	int last_signal;
	struct ewma avg_signal;
	
	__le16 last_seq_ctrl[NUM_RX_DATA_QUEUES + 1];

	
	unsigned long tx_filtered_count;
	unsigned long tx_retry_failed, tx_retry_count;
	
	unsigned int fail_avg;

	
	unsigned long tx_packets;
	unsigned long tx_bytes;
	unsigned long tx_fragments;
	struct ieee80211_tx_rate last_tx_rate;
	int last_rx_rate_idx;
	int last_rx_rate_flag;
	u16 tid_seq[IEEE80211_QOS_CTL_TID_MASK + 1];

	struct sta_ampdu_mlme ampdu_mlme;
	u8 timer_to_tid[STA_TID_NUM];

#ifdef CONFIG_MAC80211_MESH
	__le16 llid;
	__le16 plid;
	__le16 reason;
	u8 plink_retries;
	bool ignore_plink_timer;
	bool plink_timer_was_running;
	enum nl80211_plink_state plink_state;
	u32 plink_timeout;
	struct timer_list plink_timer;
#endif

#ifdef CONFIG_MAC80211_DEBUGFS
	struct sta_info_debugfsdentries {
		struct dentry *dir;
		bool add_has_run;
	} debugfs;
#endif

	unsigned int lost_packets;
	unsigned int beacon_loss_count;

	
	struct ieee80211_sta sta;
};

static inline enum nl80211_plink_state sta_plink_state(struct sta_info *sta)
{
#ifdef CONFIG_MAC80211_MESH
	return sta->plink_state;
#endif
	return NL80211_PLINK_LISTEN;
}

static inline void set_sta_flag(struct sta_info *sta,
				enum ieee80211_sta_info_flags flag)
{
	WARN_ON(flag == WLAN_STA_AUTH ||
		flag == WLAN_STA_ASSOC ||
		flag == WLAN_STA_AUTHORIZED);
	set_bit(flag, &sta->_flags);
}

static inline void clear_sta_flag(struct sta_info *sta,
				  enum ieee80211_sta_info_flags flag)
{
	WARN_ON(flag == WLAN_STA_AUTH ||
		flag == WLAN_STA_ASSOC ||
		flag == WLAN_STA_AUTHORIZED);
	clear_bit(flag, &sta->_flags);
}

static inline int test_sta_flag(struct sta_info *sta,
				enum ieee80211_sta_info_flags flag)
{
	return test_bit(flag, &sta->_flags);
}

static inline int test_and_clear_sta_flag(struct sta_info *sta,
					  enum ieee80211_sta_info_flags flag)
{
	WARN_ON(flag == WLAN_STA_AUTH ||
		flag == WLAN_STA_ASSOC ||
		flag == WLAN_STA_AUTHORIZED);
	return test_and_clear_bit(flag, &sta->_flags);
}

static inline int test_and_set_sta_flag(struct sta_info *sta,
					enum ieee80211_sta_info_flags flag)
{
	WARN_ON(flag == WLAN_STA_AUTH ||
		flag == WLAN_STA_ASSOC ||
		flag == WLAN_STA_AUTHORIZED);
	return test_and_set_bit(flag, &sta->_flags);
}

int sta_info_move_state(struct sta_info *sta,
			enum ieee80211_sta_state new_state);

static inline void sta_info_pre_move_state(struct sta_info *sta,
					   enum ieee80211_sta_state new_state)
{
	int ret;

	WARN_ON_ONCE(test_sta_flag(sta, WLAN_STA_INSERTED));

	ret = sta_info_move_state(sta, new_state);
	WARN_ON_ONCE(ret);
}


void ieee80211_assign_tid_tx(struct sta_info *sta, int tid,
			     struct tid_ampdu_tx *tid_tx);

static inline struct tid_ampdu_tx *
rcu_dereference_protected_tid_tx(struct sta_info *sta, int tid)
{
	return rcu_dereference_protected(sta->ampdu_mlme.tid_tx[tid],
					 lockdep_is_held(&sta->lock) ||
					 lockdep_is_held(&sta->ampdu_mlme.mtx));
}

#define STA_HASH_SIZE 256
#define STA_HASH(sta) (sta[5])


#define STA_MAX_TX_BUFFER	64

#define STA_TX_BUFFER_EXPIRE (10 * HZ)

#define STA_INFO_CLEANUP_INTERVAL (10 * HZ)

struct sta_info *sta_info_get(struct ieee80211_sub_if_data *sdata,
			      const u8 *addr);

struct sta_info *sta_info_get_bss(struct ieee80211_sub_if_data *sdata,
				  const u8 *addr);

static inline
void for_each_sta_info_type_check(struct ieee80211_local *local,
				  const u8 *addr,
				  struct sta_info *sta,
				  struct sta_info *nxt)
{
}

#define for_each_sta_info(local, _addr, _sta, nxt)			\
	for (						\
		_sta = rcu_dereference(local->sta_hash[STA_HASH(_addr)]),\
		nxt = _sta ? rcu_dereference(_sta->hnext) : NULL;	\
								\
		for_each_sta_info_type_check(local, (_addr), _sta, nxt),\
						\
		_sta;							\
							\
		_sta = nxt,						\
		nxt = _sta ? rcu_dereference(_sta->hnext) : NULL	\
	     )								\
			\
	if (compare_ether_addr(_sta->sta.addr, (_addr)) == 0)

struct sta_info *sta_info_get_by_idx(struct ieee80211_sub_if_data *sdata,
				     int idx);
struct sta_info *sta_info_alloc(struct ieee80211_sub_if_data *sdata,
				const u8 *addr, gfp_t gfp);

void sta_info_free(struct ieee80211_local *local, struct sta_info *sta);

int sta_info_insert(struct sta_info *sta);
int sta_info_insert_rcu(struct sta_info *sta) __acquires(RCU);

int __must_check __sta_info_destroy(struct sta_info *sta);
int sta_info_destroy_addr(struct ieee80211_sub_if_data *sdata,
			  const u8 *addr);
int sta_info_destroy_addr_bss(struct ieee80211_sub_if_data *sdata,
			      const u8 *addr);

void sta_info_recalc_tim(struct sta_info *sta);

void sta_info_init(struct ieee80211_local *local);
void sta_info_stop(struct ieee80211_local *local);
int sta_info_flush(struct ieee80211_local *local,
		   struct ieee80211_sub_if_data *sdata);
void sta_set_rate_info_tx(struct sta_info *sta,
			  const struct ieee80211_tx_rate *rate,
			  struct rate_info *rinfo);
void ieee80211_sta_expire(struct ieee80211_sub_if_data *sdata,
			  unsigned long exp_time);

void ieee80211_sta_ps_deliver_wakeup(struct sta_info *sta);
void ieee80211_sta_ps_deliver_poll_response(struct sta_info *sta);
void ieee80211_sta_ps_deliver_uapsd(struct sta_info *sta);

#endif 
