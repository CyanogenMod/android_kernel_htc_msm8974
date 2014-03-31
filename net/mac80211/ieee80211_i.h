/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007-2010	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IEEE80211_I_H
#define IEEE80211_I_H

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/if_ether.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <linux/leds.h>
#include <linux/idr.h>
#include <net/ieee80211_radiotap.h>
#include <net/cfg80211.h>
#include <net/mac80211.h>
#include "key.h"
#include "sta_info.h"

struct ieee80211_local;

#define AP_MAX_BC_BUFFER 128

#define TOTAL_MAX_TX_BUFFER 512

#define IEEE80211_ENCRYPT_HEADROOM 8
#define IEEE80211_ENCRYPT_TAILROOM 18

#define IEEE80211_FRAGMENT_MAX 4

#define TU_TO_EXP_TIME(x)	(jiffies + usecs_to_jiffies((x) * 1024))

#define IEEE80211_DEFAULT_UAPSD_QUEUES \
	(IEEE80211_WMM_IE_STA_QOSINFO_AC_BK |	\
	 IEEE80211_WMM_IE_STA_QOSINFO_AC_BE |	\
	 IEEE80211_WMM_IE_STA_QOSINFO_AC_VI |	\
	 IEEE80211_WMM_IE_STA_QOSINFO_AC_VO)

#define IEEE80211_DEFAULT_MAX_SP_LEN		\
	IEEE80211_WMM_IE_STA_QOSINFO_SP_ALL

struct ieee80211_fragment_entry {
	unsigned long first_frag_time;
	unsigned int seq;
	unsigned int rx_queue;
	unsigned int last_frag;
	unsigned int extra_len;
	struct sk_buff_head skb_list;
	int ccmp; 
	u8 last_pn[6]; 
};


struct ieee80211_bss {
	
	size_t ssid_len;
	u8 ssid[IEEE80211_MAX_SSID_LEN];

	u8 dtim_period;

	bool wmm_used;
	bool uapsd_supported;

	unsigned long last_probe_resp;

#ifdef CONFIG_MAC80211_MESH
	u8 *mesh_id;
	size_t mesh_id_len;
	u8 *mesh_cfg;
#endif

#define IEEE80211_MAX_SUPP_RATES 32
	u8 supp_rates[IEEE80211_MAX_SUPP_RATES];
	size_t supp_rates_len;

	bool has_erp_value;
	u8 erp_value;

	
	u8 corrupt_data;

	
	u8 valid_data;
};

enum ieee80211_bss_corrupt_data_flags {
	IEEE80211_BSS_CORRUPT_BEACON		= BIT(0),
	IEEE80211_BSS_CORRUPT_PROBE_RESP	= BIT(1)
};

enum ieee80211_bss_valid_data_flags {
	IEEE80211_BSS_VALID_DTIM		= BIT(0),
	IEEE80211_BSS_VALID_WMM			= BIT(1),
	IEEE80211_BSS_VALID_RATES		= BIT(2),
	IEEE80211_BSS_VALID_ERP			= BIT(3)
};

static inline u8 *bss_mesh_cfg(struct ieee80211_bss *bss)
{
#ifdef CONFIG_MAC80211_MESH
	return bss->mesh_cfg;
#endif
	return NULL;
}

static inline u8 *bss_mesh_id(struct ieee80211_bss *bss)
{
#ifdef CONFIG_MAC80211_MESH
	return bss->mesh_id;
#endif
	return NULL;
}

static inline u8 bss_mesh_id_len(struct ieee80211_bss *bss)
{
#ifdef CONFIG_MAC80211_MESH
	return bss->mesh_id_len;
#endif
	return 0;
}


typedef unsigned __bitwise__ ieee80211_tx_result;
#define TX_CONTINUE	((__force ieee80211_tx_result) 0u)
#define TX_DROP		((__force ieee80211_tx_result) 1u)
#define TX_QUEUED	((__force ieee80211_tx_result) 2u)

#define IEEE80211_TX_UNICAST		BIT(1)
#define IEEE80211_TX_PS_BUFFERED	BIT(2)

struct ieee80211_tx_data {
	struct sk_buff *skb;
	struct sk_buff_head skbs;
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;
	struct ieee80211_key *key;

	struct ieee80211_channel *channel;

	unsigned int flags;
};


typedef unsigned __bitwise__ ieee80211_rx_result;
#define RX_CONTINUE		((__force ieee80211_rx_result) 0u)
#define RX_DROP_UNUSABLE	((__force ieee80211_rx_result) 1u)
#define RX_DROP_MONITOR		((__force ieee80211_rx_result) 2u)
#define RX_QUEUED		((__force ieee80211_rx_result) 3u)

enum ieee80211_packet_rx_flags {
	IEEE80211_RX_IN_SCAN			= BIT(0),
	IEEE80211_RX_RA_MATCH			= BIT(1),
	IEEE80211_RX_FRAGMENTED			= BIT(2),
	IEEE80211_RX_AMSDU			= BIT(3),
	IEEE80211_RX_MALFORMED_ACTION_FRM	= BIT(4),
	IEEE80211_RX_DEFERRED_RELEASE		= BIT(5),
};

enum ieee80211_rx_flags {
	IEEE80211_RX_CMNTR		= BIT(0),
	IEEE80211_RX_BEACON_REPORTED	= BIT(1),
};

struct ieee80211_rx_data {
	struct sk_buff *skb;
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;
	struct ieee80211_key *key;

	unsigned int flags;

	int seqno_idx;

	int security_idx;

	u32 tkip_iv32;
	u16 tkip_iv16;
};

struct beacon_data {
	u8 *head, *tail;
	int head_len, tail_len;
	struct rcu_head rcu_head;
};

struct ieee80211_if_ap {
	struct beacon_data __rcu *beacon;
	struct sk_buff __rcu *probe_resp;

	struct list_head vlans;

	u8 tim[sizeof(unsigned long) * BITS_TO_LONGS(IEEE80211_MAX_AID + 1)];
	struct sk_buff_head ps_bc_buf;
	atomic_t num_sta_ps; 
	atomic_t num_sta_authorized; 
	int dtim_count;
	bool dtim_bc_mc;
};

struct ieee80211_if_wds {
	struct sta_info *sta;
	u8 remote_addr[ETH_ALEN];
};

struct ieee80211_if_vlan {
	struct list_head list;

	
	struct sta_info __rcu *sta;
};

struct mesh_stats {
	__u32 fwded_mcast;		
	__u32 fwded_unicast;		
	__u32 fwded_frames;		
	__u32 dropped_frames_ttl;	
	__u32 dropped_frames_no_route;	
	__u32 dropped_frames_congestion;
	atomic_t estab_plinks;
};

#define PREQ_Q_F_START		0x1
#define PREQ_Q_F_REFRESH	0x2
struct mesh_preq_queue {
	struct list_head list;
	u8 dst[ETH_ALEN];
	u8 flags;
};

enum ieee80211_work_type {
	IEEE80211_WORK_ABORT,
	IEEE80211_WORK_REMAIN_ON_CHANNEL,
	IEEE80211_WORK_OFFCHANNEL_TX,
};

enum work_done_result {
	WORK_DONE_DESTROY,
	WORK_DONE_REQUEUE,
};

struct ieee80211_work {
	struct list_head list;

	struct rcu_head rcu_head;

	struct ieee80211_sub_if_data *sdata;

	enum work_done_result (*done)(struct ieee80211_work *wk,
				      struct sk_buff *skb);

	struct ieee80211_channel *chan;
	enum nl80211_channel_type chan_type;

	unsigned long timeout;
	enum ieee80211_work_type type;

	bool started;

	union {
		struct {
			u32 duration;
		} remain;
		struct {
			struct sk_buff *frame;
			u32 wait;
			bool status;
		} offchan_tx;
	};

	size_t data_len;
	u8 data[];
};

enum ieee80211_sta_flags {
	IEEE80211_STA_BEACON_POLL	= BIT(0),
	IEEE80211_STA_CONNECTION_POLL	= BIT(1),
	IEEE80211_STA_CONTROL_PORT	= BIT(2),
	IEEE80211_STA_DISABLE_11N	= BIT(4),
	IEEE80211_STA_CSA_RECEIVED	= BIT(5),
	IEEE80211_STA_MFP_ENABLED	= BIT(6),
	IEEE80211_STA_UAPSD_ENABLED	= BIT(7),
	IEEE80211_STA_NULLFUNC_ACKED	= BIT(8),
	IEEE80211_STA_RESET_SIGNAL_AVE	= BIT(9),
	IEEE80211_STA_DISABLE_VHT	= BIT(11),
};

struct ieee80211_mgd_auth_data {
	struct cfg80211_bss *bss;
	unsigned long timeout;
	int tries;
	u16 algorithm, expected_transaction;

	u8 key[WLAN_KEY_LEN_WEP104];
	u8 key_len, key_idx;
	bool done;

	size_t ie_len;
	u8 ie[];
};

struct ieee80211_mgd_assoc_data {
	struct cfg80211_bss *bss;
	const u8 *supp_rates;
	const u8 *ht_information_ie;

	unsigned long timeout;
	int tries;

	u16 capability;
	u8 prev_bssid[ETH_ALEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ssid_len;
	u8 supp_rates_len;
	bool wmm, uapsd;
	bool have_beacon;
	bool sent_assoc;
	bool synced;

	size_t ie_len;
	u8 ie[];
};

struct ieee80211_if_managed {
	struct timer_list timer;
	struct timer_list conn_mon_timer;
	struct timer_list bcn_mon_timer;
	struct timer_list chswitch_timer;
	struct work_struct monitor_work;
	struct work_struct chswitch_work;
	struct work_struct beacon_connection_loss_work;

	unsigned long beacon_timeout;
	unsigned long probe_timeout;
	int probe_send_count;
	bool nullfunc_failed;

	struct mutex mtx;
	struct cfg80211_bss *associated;
	struct ieee80211_mgd_auth_data *auth_data;
	struct ieee80211_mgd_assoc_data *assoc_data;

	u8 bssid[ETH_ALEN];

	u16 aid;

	unsigned long timers_running; 
	bool powersave; 
	bool broken_ap; 
	enum ieee80211_smps_mode req_smps, 
				 ap_smps, 
				 driver_smps_mode; 

	struct work_struct request_smps_work;

	unsigned int flags;

	bool beacon_crc_valid;
	u32 beacon_crc;

	enum {
		IEEE80211_MFP_DISABLED,
		IEEE80211_MFP_OPTIONAL,
		IEEE80211_MFP_REQUIRED
	} mfp; 

	unsigned int uapsd_queues;

	unsigned int uapsd_max_sp_len;

	int wmm_last_param_set;

	u8 use_4addr;

	
	int last_beacon_signal;

	int ave_beacon_signal;

	unsigned int count_beacon_signal;

	int last_cqm_event_signal;

	int rssi_min_thold, rssi_max_thold;
	int last_ave_beacon_signal;

	struct ieee80211_ht_cap ht_capa; 
	struct ieee80211_ht_cap ht_capa_mask; 
};

struct ieee80211_if_ibss {
	struct timer_list timer;

	struct mutex mtx;

	unsigned long last_scan_completed;

	u32 basic_rates;

	bool timer_running;

	bool fixed_bssid;
	bool fixed_channel;
	bool privacy;

	bool control_port;

	u8 bssid[ETH_ALEN] __aligned(2);
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ssid_len, ie_len;
	u8 *ie;
	struct ieee80211_channel *channel;
	enum nl80211_channel_type channel_type;

	unsigned long ibss_join_req;
	
	struct sk_buff __rcu *presp;
	struct sk_buff *skb;

	spinlock_t incomplete_lock;
	struct list_head incomplete_stations;

	enum {
		IEEE80211_IBSS_MLME_SEARCH,
		IEEE80211_IBSS_MLME_JOINED,
	} state;
};

struct ieee80211_if_mesh {
	struct timer_list housekeeping_timer;
	struct timer_list mesh_path_timer;
	struct timer_list mesh_path_root_timer;

	unsigned long timers_running;

	unsigned long wrkq_flags;

	u8 mesh_id[IEEE80211_MAX_MESH_ID_LEN];
	size_t mesh_id_len;
	
	u8 mesh_pp_id;
	
	u8 mesh_pm_id;
	
	u8 mesh_cc_id;
	
	u8 mesh_sp_id;
	
	u8 mesh_auth_id;
	
	u32 sn;
	
	u32 preq_id;
	atomic_t mpaths;
	
	unsigned long last_sn_update;
	
	unsigned long next_perr;
	
	unsigned long last_preq;
	struct mesh_rmc *rmc;
	spinlock_t mesh_preq_queue_lock;
	struct mesh_preq_queue preq_queue;
	int preq_queue_len;
	struct mesh_stats mshstats;
	struct mesh_config mshcfg;
	u32 mesh_seqnum;
	bool accepting_plinks;
	int num_gates;
	const u8 *ie;
	u8 ie_len;
	enum {
		IEEE80211_MESH_SEC_NONE = 0x0,
		IEEE80211_MESH_SEC_AUTHED = 0x1,
		IEEE80211_MESH_SEC_SECURED = 0x2,
	} security;
};

#ifdef CONFIG_MAC80211_MESH
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name)	\
	do { (msh)->mshstats.name++; } while (0)
#else
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name) \
	do { } while (0)
#endif

enum ieee80211_sub_if_data_flags {
	IEEE80211_SDATA_ALLMULTI		= BIT(0),
	IEEE80211_SDATA_PROMISC			= BIT(1),
	IEEE80211_SDATA_OPERATING_GMODE		= BIT(2),
	IEEE80211_SDATA_DONT_BRIDGE_PACKETS	= BIT(3),
	IEEE80211_SDATA_DISCONNECT_RESUME	= BIT(4),
	IEEE80211_SDATA_IN_DRIVER		= BIT(5),
};

enum ieee80211_sdata_state_bits {
	SDATA_STATE_RUNNING,
	SDATA_STATE_OFFCHANNEL,
};

struct ieee80211_sub_if_data {
	struct list_head list;

	struct wireless_dev wdev;

	
	struct list_head key_list;

	
	int crypto_tx_tailroom_needed_cnt;

	struct net_device *dev;
	struct ieee80211_local *local;

	unsigned int flags;

	unsigned long state;

	int drop_unencrypted;

	char name[IFNAMSIZ];

	bool ht_opmode_valid;

	
	bool old_idle;

	
	struct ieee80211_fragment_entry	fragments[IEEE80211_FRAGMENT_MAX];
	unsigned int fragment_next;

	
	u16 noack_map;

	struct ieee80211_key __rcu *keys[NUM_DEFAULT_KEYS + NUM_DEFAULT_MGMT_KEYS];
	struct ieee80211_key __rcu *default_unicast_key;
	struct ieee80211_key __rcu *default_multicast_key;
	struct ieee80211_key __rcu *default_mgmt_key;

	u16 sequence_number;
	__be16 control_port_protocol;
	bool control_port_no_encrypt;

	struct ieee80211_tx_queue_params tx_conf[IEEE80211_MAX_QUEUES];

	struct work_struct work;
	struct sk_buff_head skb_queue;

	bool arp_filter_state;

	struct ieee80211_if_ap *bss;

	
	u32 rc_rateidx_mask[IEEE80211_NUM_BANDS];
	u8  rc_rateidx_mcs_mask[IEEE80211_NUM_BANDS][IEEE80211_HT_MCS_MASK_LEN];

	union {
		struct ieee80211_if_ap ap;
		struct ieee80211_if_wds wds;
		struct ieee80211_if_vlan vlan;
		struct ieee80211_if_managed mgd;
		struct ieee80211_if_ibss ibss;
		struct ieee80211_if_mesh mesh;
		u32 mntr_flags;
	} u;

#ifdef CONFIG_MAC80211_DEBUGFS
	struct {
		struct dentry *dir;
		struct dentry *subdir_stations;
		struct dentry *default_unicast_key;
		struct dentry *default_multicast_key;
		struct dentry *default_mgmt_key;
	} debugfs;
#endif
	
	struct ieee80211_vif vif;
};

static inline
struct ieee80211_sub_if_data *vif_to_sdata(struct ieee80211_vif *p)
{
	return container_of(p, struct ieee80211_sub_if_data, vif);
}

enum sdata_queue_type {
	IEEE80211_SDATA_QUEUE_TYPE_FRAME	= 0,
	IEEE80211_SDATA_QUEUE_AGG_START		= 1,
	IEEE80211_SDATA_QUEUE_AGG_STOP		= 2,
};

enum {
	IEEE80211_RX_MSG	= 1,
	IEEE80211_TX_STATUS_MSG	= 2,
	IEEE80211_EOSP_MSG	= 3,
};

struct skb_eosp_msg_data {
	u8 sta[ETH_ALEN], iface[ETH_ALEN];
};

enum queue_stop_reason {
	IEEE80211_QUEUE_STOP_REASON_DRIVER,
	IEEE80211_QUEUE_STOP_REASON_PS,
	IEEE80211_QUEUE_STOP_REASON_CSA,
	IEEE80211_QUEUE_STOP_REASON_AGGREGATION,
	IEEE80211_QUEUE_STOP_REASON_SUSPEND,
	IEEE80211_QUEUE_STOP_REASON_SKB_ADD,
	IEEE80211_QUEUE_STOP_REASON_CHTYPE_CHANGE,
};

#ifdef CONFIG_MAC80211_LEDS
struct tpt_led_trigger {
	struct led_trigger trig;
	char name[32];
	const struct ieee80211_tpt_blink *blink_table;
	unsigned int blink_table_len;
	struct timer_list timer;
	unsigned long prev_traffic;
	unsigned long tx_bytes, rx_bytes;
	unsigned int active, want;
	bool running;
};
#endif

enum {
	SCAN_SW_SCANNING,
	SCAN_HW_SCANNING,
	SCAN_COMPLETED,
	SCAN_ABORTED,
};

enum mac80211_scan_state {
	SCAN_DECISION,
	SCAN_SET_CHANNEL,
	SCAN_SEND_PROBE,
	SCAN_SUSPEND,
	SCAN_RESUME,
};

struct ieee80211_local {
	struct ieee80211_hw hw;

	const struct ieee80211_ops *ops;

	struct list_head work_list;
	struct timer_list work_timer;
	struct work_struct work_work;

	struct workqueue_struct *workqueue;

	unsigned long queue_stop_reasons[IEEE80211_MAX_QUEUES];
	
	spinlock_t queue_stop_reason_lock;

	int open_count;
	int monitors, cooked_mntrs;
	
	int fif_fcsfail, fif_plcpfail, fif_control, fif_other_bss, fif_pspoll,
	    fif_probe_req;
	int probe_req_reg;
	unsigned int filter_flags; 

	bool wiphy_ciphers_allocated;

	
	spinlock_t filter_lock;

	
	struct work_struct reconfig_filter;

	
	struct work_struct recalc_smps;

	
	struct netdev_hw_addr_list mc_list;

	bool tim_in_locked_section; 

	bool suspended;

	bool resuming;

	bool quiescing;

	
	bool started;

	
	bool wowlan;

	int tx_headroom; 

#define IEEE80211_IRQSAFE_QUEUE_LIMIT 128
	struct tasklet_struct tasklet;
	struct sk_buff_head skb_queue;
	struct sk_buff_head skb_queue_unreliable;

	struct sk_buff_head rx_skb_queue;
	bool running_rx_handler;	

	
	struct mutex sta_mtx;
	spinlock_t tim_lock;
	unsigned long num_sta;
	struct list_head sta_list;
	struct sta_info __rcu *sta_hash[STA_HASH_SIZE];
	struct timer_list sta_cleanup;
	int sta_generation;

	struct sk_buff_head pending[IEEE80211_MAX_QUEUES];
	struct tasklet_struct tx_pending_tasklet;

	atomic_t agg_queue_stop[IEEE80211_MAX_QUEUES];

	
	atomic_t iff_allmultis, iff_promiscs;

	struct rate_control_ref *rate_ctrl;

	struct crypto_cipher *wep_tx_tfm;
	struct crypto_cipher *wep_rx_tfm;
	u32 wep_iv;

	
	struct list_head interfaces;
	struct mutex iflist_mtx;

	struct mutex key_mtx;

	
	struct mutex mtx;

	
	unsigned long scanning;
	struct cfg80211_ssid scan_ssid;
	struct cfg80211_scan_request *int_scan_req;
	struct cfg80211_scan_request *scan_req, *hw_scan_req;
	struct ieee80211_channel *scan_channel;
	enum ieee80211_band hw_scan_band;
	int scan_channel_idx;
	int scan_ies_len;

	bool sched_scanning;
	struct ieee80211_sched_scan_ies sched_scan_ies;
	struct work_struct sched_scan_stopped_work;

	unsigned long leave_oper_channel_time;
	enum mac80211_scan_state next_scan_state;
	struct delayed_work scan_work;
	struct ieee80211_sub_if_data *scan_sdata;
	enum nl80211_channel_type _oper_channel_type;
	struct ieee80211_channel *oper_channel, *csa_channel;

	
	struct ieee80211_channel *tmp_channel;
	enum nl80211_channel_type tmp_channel_type;

	
	
	u32 dot11TransmittedFragmentCount;
	u32 dot11MulticastTransmittedFrameCount;
	u32 dot11FailedCount;
	u32 dot11RetryCount;
	u32 dot11MultipleRetryCount;
	u32 dot11FrameDuplicateCount;
	u32 dot11ReceivedFragmentCount;
	u32 dot11MulticastReceivedFrameCount;
	u32 dot11TransmittedFrameCount;

#ifdef CONFIG_MAC80211_LEDS
	int tx_led_counter, rx_led_counter;
	struct led_trigger *tx_led, *rx_led, *assoc_led, *radio_led;
	struct tpt_led_trigger *tpt_led_trigger;
	char tx_led_name[32], rx_led_name[32],
	     assoc_led_name[32], radio_led_name[32];
#endif

#ifdef CONFIG_MAC80211_DEBUG_COUNTERS
	
	unsigned int tx_handlers_drop;
	unsigned int tx_handlers_queued;
	unsigned int tx_handlers_drop_unencrypted;
	unsigned int tx_handlers_drop_fragment;
	unsigned int tx_handlers_drop_wep;
	unsigned int tx_handlers_drop_not_assoc;
	unsigned int tx_handlers_drop_unauth_port;
	unsigned int rx_handlers_drop;
	unsigned int rx_handlers_queued;
	unsigned int rx_handlers_drop_nullfunc;
	unsigned int rx_handlers_drop_defrag;
	unsigned int rx_handlers_drop_short;
	unsigned int rx_handlers_drop_passive_scan;
	unsigned int tx_expand_skb_head;
	unsigned int tx_expand_skb_head_cloned;
	unsigned int rx_expand_skb_head;
	unsigned int rx_expand_skb_head2;
	unsigned int rx_handlers_fragments;
	unsigned int tx_status_drop;
#define I802_DEBUG_INC(c) (c)++
#else 
#define I802_DEBUG_INC(c) do { } while (0)
#endif 


	int total_ps_buffered; 
	unsigned int wmm_acm; 

	bool pspolling;
	bool offchannel_ps_enabled;
	struct ieee80211_sub_if_data *ps_sdata;
	struct work_struct dynamic_ps_enable_work;
	struct work_struct dynamic_ps_disable_work;
	struct timer_list dynamic_ps_timer;
	struct notifier_block network_latency_notifier;
	struct notifier_block ifa_notifier;

	int dynamic_ps_forced_timeout;
	int dynamic_ps_user_timeout;
	bool disable_dynamic_ps;

	int user_power_level; 
	int power_constr_level; 

	enum ieee80211_smps_mode smps_mode;

	struct work_struct restart_work;

#ifdef CONFIG_MAC80211_DEBUGFS
	struct local_debugfsdentries {
		struct dentry *rcdir;
		struct dentry *keys;
	} debugfs;
#endif

	struct ieee80211_channel *hw_roc_channel;
	struct net_device *hw_roc_dev;
	struct sk_buff *hw_roc_skb, *hw_roc_skb_for_status;
	struct work_struct hw_roc_start, hw_roc_done;
	enum nl80211_channel_type hw_roc_channel_type;
	unsigned int hw_roc_duration;
	u32 hw_roc_cookie;
	bool hw_roc_for_tx;

	struct idr ack_status_frames;
	spinlock_t ack_status_lock;

	
	struct net_device napi_dev;

	struct napi_struct napi;
};

static inline struct ieee80211_sub_if_data *
IEEE80211_DEV_TO_SUB_IF(struct net_device *dev)
{
	return netdev_priv(dev);
}

struct ieee80211_ra_tid {
	u8 ra[ETH_ALEN];
	u16 tid;
};

struct ieee802_11_elems {
	u8 *ie_start;
	size_t total_len;

	
	u8 *ssid;
	u8 *supp_rates;
	u8 *fh_params;
	u8 *ds_params;
	u8 *cf_params;
	struct ieee80211_tim_ie *tim;
	u8 *ibss_params;
	u8 *challenge;
	u8 *wpa;
	u8 *rsn;
	u8 *erp_info;
	u8 *ext_supp_rates;
	u8 *wmm_info;
	u8 *wmm_param;
	struct ieee80211_ht_cap *ht_cap_elem;
	struct ieee80211_ht_info *ht_info_elem;
	struct ieee80211_meshconf_ie *mesh_config;
	u8 *mesh_id;
	u8 *peering;
	u8 *preq;
	u8 *prep;
	u8 *perr;
	struct ieee80211_rann_ie *rann;
	u8 *ch_switch_elem;
	u8 *country_elem;
	u8 *pwr_constr_elem;
	u8 *quiet_elem;	
	u8 *timeout_int;

	
	u8 ssid_len;
	u8 supp_rates_len;
	u8 fh_params_len;
	u8 ds_params_len;
	u8 cf_params_len;
	u8 tim_len;
	u8 ibss_params_len;
	u8 challenge_len;
	u8 wpa_len;
	u8 rsn_len;
	u8 erp_info_len;
	u8 ext_supp_rates_len;
	u8 wmm_info_len;
	u8 wmm_param_len;
	u8 mesh_id_len;
	u8 peering_len;
	u8 preq_len;
	u8 prep_len;
	u8 perr_len;
	u8 ch_switch_elem_len;
	u8 country_elem_len;
	u8 pwr_constr_elem_len;
	u8 quiet_elem_len;
	u8 num_of_quiet_elem;	
	u8 timeout_int_len;

	
	bool parse_error;
};

static inline struct ieee80211_local *hw_to_local(
	struct ieee80211_hw *hw)
{
	return container_of(hw, struct ieee80211_local, hw);
}


static inline int ieee80211_bssid_match(const u8 *raddr, const u8 *addr)
{
	return compare_ether_addr(raddr, addr) == 0 ||
	       is_broadcast_ether_addr(raddr);
}


int ieee80211_hw_config(struct ieee80211_local *local, u32 changed);
void ieee80211_tx_set_protected(struct ieee80211_tx_data *tx);
void ieee80211_bss_info_change_notify(struct ieee80211_sub_if_data *sdata,
				      u32 changed);
void ieee80211_configure_filter(struct ieee80211_local *local);
u32 ieee80211_reset_erp_info(struct ieee80211_sub_if_data *sdata);

void ieee80211_sta_setup_sdata(struct ieee80211_sub_if_data *sdata);
int ieee80211_mgd_auth(struct ieee80211_sub_if_data *sdata,
		       struct cfg80211_auth_request *req);
int ieee80211_mgd_assoc(struct ieee80211_sub_if_data *sdata,
			struct cfg80211_assoc_request *req);
int ieee80211_mgd_deauth(struct ieee80211_sub_if_data *sdata,
			 struct cfg80211_deauth_request *req);
int ieee80211_mgd_disassoc(struct ieee80211_sub_if_data *sdata,
			   struct cfg80211_disassoc_request *req);
void ieee80211_send_pspoll(struct ieee80211_local *local,
			   struct ieee80211_sub_if_data *sdata);
void ieee80211_recalc_ps(struct ieee80211_local *local, s32 latency);
int ieee80211_max_network_latency(struct notifier_block *nb,
				  unsigned long data, void *dummy);
int ieee80211_set_arp_filter(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_process_chanswitch(struct ieee80211_sub_if_data *sdata,
				      struct ieee80211_channel_sw_ie *sw_elem,
				      struct ieee80211_bss *bss,
				      u64 timestamp);
void ieee80211_sta_quiesce(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_restart(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
				  struct sk_buff *skb);
void ieee80211_sta_reset_beacon_monitor(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_reset_conn_monitor(struct ieee80211_sub_if_data *sdata);
void ieee80211_mgd_stop(struct ieee80211_sub_if_data *sdata);

void ieee80211_ibss_notify_scan_completed(struct ieee80211_local *local);
void ieee80211_ibss_setup_sdata(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_rx_no_sta(struct ieee80211_sub_if_data *sdata,
			      const u8 *bssid, const u8 *addr, u32 supp_rates);
int ieee80211_ibss_join(struct ieee80211_sub_if_data *sdata,
			struct cfg80211_ibss_params *params);
int ieee80211_ibss_leave(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_quiesce(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_restart(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
				   struct sk_buff *skb);

void ieee80211_mesh_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_mesh_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
				   struct sk_buff *skb);

void ieee80211_scan_work(struct work_struct *work);
int ieee80211_request_internal_scan(struct ieee80211_sub_if_data *sdata,
				    const u8 *ssid, u8 ssid_len,
				    struct ieee80211_channel *chan);
int ieee80211_request_scan(struct ieee80211_sub_if_data *sdata,
			   struct cfg80211_scan_request *req);
void ieee80211_scan_cancel(struct ieee80211_local *local);
ieee80211_rx_result
ieee80211_scan_rx(struct ieee80211_sub_if_data *sdata, struct sk_buff *skb);

void ieee80211_mlme_notify_scan_completed(struct ieee80211_local *local);
struct ieee80211_bss *
ieee80211_bss_info_update(struct ieee80211_local *local,
			  struct ieee80211_rx_status *rx_status,
			  struct ieee80211_mgmt *mgmt,
			  size_t len,
			  struct ieee802_11_elems *elems,
			  struct ieee80211_channel *channel,
			  bool beacon);
struct ieee80211_bss *
ieee80211_rx_bss_get(struct ieee80211_local *local, u8 *bssid, int freq,
		     u8 *ssid, u8 ssid_len);
void ieee80211_rx_bss_put(struct ieee80211_local *local,
			  struct ieee80211_bss *bss);

int ieee80211_request_sched_scan_start(struct ieee80211_sub_if_data *sdata,
				       struct cfg80211_sched_scan_request *req);
int ieee80211_request_sched_scan_stop(struct ieee80211_sub_if_data *sdata);
void ieee80211_sched_scan_stopped_work(struct work_struct *work);

void ieee80211_offchannel_stop_vifs(struct ieee80211_local *local,
				    bool offchannel_ps_enable);
void ieee80211_offchannel_return(struct ieee80211_local *local,
				 bool offchannel_ps_disable);
void ieee80211_hw_roc_setup(struct ieee80211_local *local);

int ieee80211_iface_init(void);
void ieee80211_iface_exit(void);
int ieee80211_if_add(struct ieee80211_local *local, const char *name,
		     struct net_device **new_dev, enum nl80211_iftype type,
		     struct vif_params *params);
int ieee80211_if_change_type(struct ieee80211_sub_if_data *sdata,
			     enum nl80211_iftype type);
void ieee80211_if_remove(struct ieee80211_sub_if_data *sdata);
void ieee80211_remove_interfaces(struct ieee80211_local *local);
u32 __ieee80211_recalc_idle(struct ieee80211_local *local);
void ieee80211_recalc_idle(struct ieee80211_local *local);
void ieee80211_adjust_monitor_flags(struct ieee80211_sub_if_data *sdata,
				    const int offset);

static inline bool ieee80211_sdata_running(struct ieee80211_sub_if_data *sdata)
{
	return test_bit(SDATA_STATE_RUNNING, &sdata->state);
}

void ieee80211_clear_tx_pending(struct ieee80211_local *local);
void ieee80211_tx_pending(unsigned long data);
netdev_tx_t ieee80211_monitor_start_xmit(struct sk_buff *skb,
					 struct net_device *dev);
netdev_tx_t ieee80211_subif_start_xmit(struct sk_buff *skb,
				       struct net_device *dev);

bool ieee80111_cfg_override_disables_ht40(struct ieee80211_sub_if_data *sdata);
void ieee80211_apply_htcap_overrides(struct ieee80211_sub_if_data *sdata,
				     struct ieee80211_sta_ht_cap *ht_cap);
void ieee80211_ht_cap_ie_to_sta_ht_cap(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_supported_band *sband,
				       struct ieee80211_ht_cap *ht_cap_ie,
				       struct ieee80211_sta_ht_cap *ht_cap);
void ieee80211_send_delba(struct ieee80211_sub_if_data *sdata,
			  const u8 *da, u16 tid,
			  u16 initiator, u16 reason_code);
int ieee80211_send_smps_action(struct ieee80211_sub_if_data *sdata,
			       enum ieee80211_smps_mode smps, const u8 *da,
			       const u8 *bssid);
void ieee80211_request_smps_work(struct work_struct *work);

void ___ieee80211_stop_rx_ba_session(struct sta_info *sta, u16 tid,
				     u16 initiator, u16 reason, bool stop);
void __ieee80211_stop_rx_ba_session(struct sta_info *sta, u16 tid,
				    u16 initiator, u16 reason, bool stop);
void ieee80211_sta_tear_down_BA_sessions(struct sta_info *sta, bool tx);
void ieee80211_process_delba(struct ieee80211_sub_if_data *sdata,
			     struct sta_info *sta,
			     struct ieee80211_mgmt *mgmt, size_t len);
void ieee80211_process_addba_resp(struct ieee80211_local *local,
				  struct sta_info *sta,
				  struct ieee80211_mgmt *mgmt,
				  size_t len);
void ieee80211_process_addba_request(struct ieee80211_local *local,
				     struct sta_info *sta,
				     struct ieee80211_mgmt *mgmt,
				     size_t len);

int __ieee80211_stop_tx_ba_session(struct sta_info *sta, u16 tid,
				   enum ieee80211_back_parties initiator,
				   bool tx);
int ___ieee80211_stop_tx_ba_session(struct sta_info *sta, u16 tid,
				    enum ieee80211_back_parties initiator,
				    bool tx);
void ieee80211_start_tx_ba_cb(struct ieee80211_vif *vif, u8 *ra, u16 tid);
void ieee80211_stop_tx_ba_cb(struct ieee80211_vif *vif, u8 *ra, u8 tid);
void ieee80211_ba_session_work(struct work_struct *work);
void ieee80211_tx_ba_session_handle_start(struct sta_info *sta, int tid);
void ieee80211_release_reorder_timeout(struct sta_info *sta, int tid);

void ieee80211_process_measurement_req(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_mgmt *mgmt,
				       size_t len);

int ieee80211_reconfig(struct ieee80211_local *local);
void ieee80211_stop_device(struct ieee80211_local *local);

#ifdef CONFIG_PM
int __ieee80211_suspend(struct ieee80211_hw *hw,
			struct cfg80211_wowlan *wowlan);

static inline int __ieee80211_resume(struct ieee80211_hw *hw)
{
	struct ieee80211_local *local = hw_to_local(hw);

	WARN(test_bit(SCAN_HW_SCANNING, &local->scanning),
		"%s: resume with hardware scan still in progress\n",
		wiphy_name(hw->wiphy));

	return ieee80211_reconfig(hw_to_local(hw));
}
#else
static inline int __ieee80211_suspend(struct ieee80211_hw *hw,
				      struct cfg80211_wowlan *wowlan)
{
	return 0;
}

static inline int __ieee80211_resume(struct ieee80211_hw *hw)
{
	return 0;
}
#endif

extern void *mac80211_wiphy_privid; 
u8 *ieee80211_get_bssid(struct ieee80211_hdr *hdr, size_t len,
			enum nl80211_iftype type);
int ieee80211_frame_duration(struct ieee80211_local *local, size_t len,
			     int rate, int erp, int short_preamble);
void mac80211_ev_michael_mic_failure(struct ieee80211_sub_if_data *sdata, int keyidx,
				     struct ieee80211_hdr *hdr, const u8 *tsc,
				     gfp_t gfp);
void ieee80211_set_wmm_default(struct ieee80211_sub_if_data *sdata,
			       bool bss_notify);
void ieee80211_xmit(struct ieee80211_sub_if_data *sdata, struct sk_buff *skb);

void ieee80211_tx_skb_tid(struct ieee80211_sub_if_data *sdata,
			  struct sk_buff *skb, int tid);
static void inline ieee80211_tx_skb(struct ieee80211_sub_if_data *sdata,
				    struct sk_buff *skb)
{
	
	ieee80211_tx_skb_tid(sdata, skb, 7);
}

void ieee802_11_parse_elems(u8 *start, size_t len,
			    struct ieee802_11_elems *elems);
u32 ieee802_11_parse_elems_crc(u8 *start, size_t len,
			       struct ieee802_11_elems *elems,
			       u64 filter, u32 crc);
u32 ieee80211_mandatory_rates(struct ieee80211_local *local,
			      enum ieee80211_band band);

void ieee80211_dynamic_ps_enable_work(struct work_struct *work);
void ieee80211_dynamic_ps_disable_work(struct work_struct *work);
void ieee80211_dynamic_ps_timer(unsigned long data);
void ieee80211_send_nullfunc(struct ieee80211_local *local,
			     struct ieee80211_sub_if_data *sdata,
			     int powersave);
void ieee80211_sta_rx_notify(struct ieee80211_sub_if_data *sdata,
			     struct ieee80211_hdr *hdr);
void ieee80211_sta_tx_notify(struct ieee80211_sub_if_data *sdata,
			     struct ieee80211_hdr *hdr, bool ack);
void ieee80211_beacon_connection_loss_work(struct work_struct *work);

void ieee80211_wake_queues_by_reason(struct ieee80211_hw *hw,
				     enum queue_stop_reason reason);
void ieee80211_stop_queues_by_reason(struct ieee80211_hw *hw,
				     enum queue_stop_reason reason);
void ieee80211_wake_queue_by_reason(struct ieee80211_hw *hw, int queue,
				    enum queue_stop_reason reason);
void ieee80211_stop_queue_by_reason(struct ieee80211_hw *hw, int queue,
				    enum queue_stop_reason reason);
void ieee80211_add_pending_skb(struct ieee80211_local *local,
			       struct sk_buff *skb);
void ieee80211_add_pending_skbs(struct ieee80211_local *local,
				struct sk_buff_head *skbs);
void ieee80211_add_pending_skbs_fn(struct ieee80211_local *local,
				   struct sk_buff_head *skbs,
				   void (*fn)(void *data), void *data);

void ieee80211_send_auth(struct ieee80211_sub_if_data *sdata,
			 u16 transaction, u16 auth_alg,
			 u8 *extra, size_t extra_len, const u8 *bssid,
			 const u8 *da, const u8 *key, u8 key_len, u8 key_idx);
int ieee80211_build_preq_ies(struct ieee80211_local *local, u8 *buffer,
			     const u8 *ie, size_t ie_len,
			     enum ieee80211_band band, u32 rate_mask,
			     u8 channel);
struct sk_buff *ieee80211_build_probe_req(struct ieee80211_sub_if_data *sdata,
					  u8 *dst, u32 ratemask,
					  const u8 *ssid, size_t ssid_len,
					  const u8 *ie, size_t ie_len,
					  bool directed);
void ieee80211_send_probe_req(struct ieee80211_sub_if_data *sdata, u8 *dst,
			      const u8 *ssid, size_t ssid_len,
			      const u8 *ie, size_t ie_len,
			      u32 ratemask, bool directed, bool no_cck);

void ieee80211_sta_def_wmm_params(struct ieee80211_sub_if_data *sdata,
				  const size_t supp_rates_len,
				  const u8 *supp_rates);
u32 ieee80211_sta_get_rates(struct ieee80211_local *local,
			    struct ieee802_11_elems *elems,
			    enum ieee80211_band band);
int __ieee80211_request_smps(struct ieee80211_sub_if_data *sdata,
			     enum ieee80211_smps_mode smps_mode);
void ieee80211_recalc_smps(struct ieee80211_local *local);

size_t ieee80211_ie_split(const u8 *ies, size_t ielen,
			  const u8 *ids, int n_ids, size_t offset);
size_t ieee80211_ie_split_vendor(const u8 *ies, size_t ielen, size_t offset);
u8 *ieee80211_ie_build_ht_cap(u8 *pos, struct ieee80211_sta_ht_cap *ht_cap,
			      u16 cap);
u8 *ieee80211_ie_build_ht_info(u8 *pos,
				struct ieee80211_sta_ht_cap *ht_cap,
				struct ieee80211_channel *channel,
				enum nl80211_channel_type channel_type);
u8 *ieee80211_ie_build_vht_cap(u8 *pos, struct ieee80211_sta_vht_cap *vht_cap,
			       u32 cap);

void ieee80211_work_init(struct ieee80211_local *local);
void ieee80211_add_work(struct ieee80211_work *wk);
void free_work(struct ieee80211_work *wk);
void ieee80211_work_purge(struct ieee80211_sub_if_data *sdata);
int ieee80211_wk_remain_on_channel(struct ieee80211_sub_if_data *sdata,
				   struct ieee80211_channel *chan,
				   enum nl80211_channel_type channel_type,
				   unsigned int duration, u64 *cookie);
int ieee80211_wk_cancel_remain_on_channel(
	struct ieee80211_sub_if_data *sdata, u64 cookie);

enum ieee80211_chan_mode {
	CHAN_MODE_UNDEFINED,
	CHAN_MODE_HOPPING,
	CHAN_MODE_FIXED,
};

enum ieee80211_chan_mode
ieee80211_get_channel_mode(struct ieee80211_local *local,
			   struct ieee80211_sub_if_data *ignore);
bool ieee80211_set_channel_type(struct ieee80211_local *local,
				struct ieee80211_sub_if_data *sdata,
				enum nl80211_channel_type chantype);
enum nl80211_channel_type
ieee80211_ht_info_to_channel_type(struct ieee80211_ht_info *ht_info);
enum nl80211_channel_type ieee80211_get_tx_channel_type(
					struct ieee80211_local *local,
					enum nl80211_channel_type channel_type);

#ifdef CONFIG_MAC80211_NOINLINE
#define debug_noinline noinline
#else
#define debug_noinline
#endif

#endif 
