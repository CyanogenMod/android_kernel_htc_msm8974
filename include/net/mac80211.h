/*
 * mac80211 <-> driver interface
 *
 * Copyright 2002-2005, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007-2010	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MAC80211_H
#define MAC80211_H

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <asm/unaligned.h>






struct device;

enum ieee80211_max_queues {
	IEEE80211_MAX_QUEUES =		4,
};

enum ieee80211_ac_numbers {
	IEEE80211_AC_VO		= 0,
	IEEE80211_AC_VI		= 1,
	IEEE80211_AC_BE		= 2,
	IEEE80211_AC_BK		= 3,
};
#define IEEE80211_NUM_ACS	4

struct ieee80211_tx_queue_params {
	u16 txop;
	u16 cw_min;
	u16 cw_max;
	u8 aifs;
	bool uapsd;
};

struct ieee80211_low_level_stats {
	unsigned int dot11ACKFailureCount;
	unsigned int dot11RTSFailureCount;
	unsigned int dot11FCSErrorCount;
	unsigned int dot11RTSSuccessCount;
};

enum ieee80211_bss_change {
	BSS_CHANGED_ASSOC		= 1<<0,
	BSS_CHANGED_ERP_CTS_PROT	= 1<<1,
	BSS_CHANGED_ERP_PREAMBLE	= 1<<2,
	BSS_CHANGED_ERP_SLOT		= 1<<3,
	BSS_CHANGED_HT			= 1<<4,
	BSS_CHANGED_BASIC_RATES		= 1<<5,
	BSS_CHANGED_BEACON_INT		= 1<<6,
	BSS_CHANGED_BSSID		= 1<<7,
	BSS_CHANGED_BEACON		= 1<<8,
	BSS_CHANGED_BEACON_ENABLED	= 1<<9,
	BSS_CHANGED_CQM			= 1<<10,
	BSS_CHANGED_IBSS		= 1<<11,
	BSS_CHANGED_ARP_FILTER		= 1<<12,
	BSS_CHANGED_QOS			= 1<<13,
	BSS_CHANGED_IDLE		= 1<<14,
	BSS_CHANGED_SSID		= 1<<15,
	BSS_CHANGED_AP_PROBE_RESP	= 1<<16,

	
};

#define IEEE80211_BSS_ARP_ADDR_LIST_LEN 4

enum ieee80211_rssi_event {
	RSSI_EVENT_HIGH,
	RSSI_EVENT_LOW,
};

struct ieee80211_bss_conf {
	const u8 *bssid;
	
	bool assoc, ibss_joined;
	u16 aid;
	
	bool use_cts_prot;
	bool use_short_preamble;
	bool use_short_slot;
	bool enable_beacon;
	u8 dtim_period;
	u16 beacon_int;
	u16 assoc_capability;
	u64 last_tsf;
	u32 basic_rates;
	int mcast_rate[IEEE80211_NUM_BANDS];
	u16 ht_operation_mode;
	s32 cqm_rssi_thold;
	u32 cqm_rssi_hyst;
	enum nl80211_channel_type channel_type;
	__be32 arp_addr_list[IEEE80211_BSS_ARP_ADDR_LIST_LEN];
	u8 arp_addr_cnt;
	bool arp_filter_enabled;
	bool qos;
	bool idle;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	size_t ssid_len;
	bool hidden_ssid;
};

enum mac80211_tx_control_flags {
	IEEE80211_TX_CTL_REQ_TX_STATUS		= BIT(0),
	IEEE80211_TX_CTL_ASSIGN_SEQ		= BIT(1),
	IEEE80211_TX_CTL_NO_ACK			= BIT(2),
	IEEE80211_TX_CTL_CLEAR_PS_FILT		= BIT(3),
	IEEE80211_TX_CTL_FIRST_FRAGMENT		= BIT(4),
	IEEE80211_TX_CTL_SEND_AFTER_DTIM	= BIT(5),
	IEEE80211_TX_CTL_AMPDU			= BIT(6),
	IEEE80211_TX_CTL_INJECTED		= BIT(7),
	IEEE80211_TX_STAT_TX_FILTERED		= BIT(8),
	IEEE80211_TX_STAT_ACK			= BIT(9),
	IEEE80211_TX_STAT_AMPDU			= BIT(10),
	IEEE80211_TX_STAT_AMPDU_NO_BACK		= BIT(11),
	IEEE80211_TX_CTL_RATE_CTRL_PROBE	= BIT(12),
	IEEE80211_TX_INTFL_NEED_TXPROCESSING	= BIT(14),
	IEEE80211_TX_INTFL_RETRIED		= BIT(15),
	IEEE80211_TX_INTFL_DONT_ENCRYPT		= BIT(16),
	IEEE80211_TX_CTL_NO_PS_BUFFER		= BIT(17),
	IEEE80211_TX_CTL_MORE_FRAMES		= BIT(18),
	IEEE80211_TX_INTFL_RETRANSMISSION	= BIT(19),
	
	IEEE80211_TX_INTFL_NL80211_FRAME_TX	= BIT(21),
	IEEE80211_TX_CTL_LDPC			= BIT(22),
	IEEE80211_TX_CTL_STBC			= BIT(23) | BIT(24),
	IEEE80211_TX_CTL_TX_OFFCHAN		= BIT(25),
	IEEE80211_TX_INTFL_TKIP_MIC_FAILURE	= BIT(26),
	IEEE80211_TX_CTL_NO_CCK_RATE		= BIT(27),
	IEEE80211_TX_STATUS_EOSP		= BIT(28),
	IEEE80211_TX_CTL_USE_MINRATE		= BIT(29),
	IEEE80211_TX_CTL_DONTFRAG		= BIT(30),
};

#define IEEE80211_TX_CTL_STBC_SHIFT		23

#define IEEE80211_TX_TEMPORARY_FLAGS (IEEE80211_TX_CTL_NO_ACK |		      \
	IEEE80211_TX_CTL_CLEAR_PS_FILT | IEEE80211_TX_CTL_FIRST_FRAGMENT |    \
	IEEE80211_TX_CTL_SEND_AFTER_DTIM | IEEE80211_TX_CTL_AMPDU |	      \
	IEEE80211_TX_STAT_TX_FILTERED |	IEEE80211_TX_STAT_ACK |		      \
	IEEE80211_TX_STAT_AMPDU | IEEE80211_TX_STAT_AMPDU_NO_BACK |	      \
	IEEE80211_TX_CTL_RATE_CTRL_PROBE | IEEE80211_TX_CTL_NO_PS_BUFFER |    \
	IEEE80211_TX_CTL_MORE_FRAMES | IEEE80211_TX_CTL_LDPC |		      \
	IEEE80211_TX_CTL_STBC | IEEE80211_TX_STATUS_EOSP)

enum mac80211_rate_control_flags {
	IEEE80211_TX_RC_USE_RTS_CTS		= BIT(0),
	IEEE80211_TX_RC_USE_CTS_PROTECT		= BIT(1),
	IEEE80211_TX_RC_USE_SHORT_PREAMBLE	= BIT(2),

	
	IEEE80211_TX_RC_MCS			= BIT(3),
	IEEE80211_TX_RC_GREEN_FIELD		= BIT(4),
	IEEE80211_TX_RC_40_MHZ_WIDTH		= BIT(5),
	IEEE80211_TX_RC_DUP_DATA		= BIT(6),
	IEEE80211_TX_RC_SHORT_GI		= BIT(7),
};


#define IEEE80211_TX_INFO_DRIVER_DATA_SIZE 40

#define IEEE80211_TX_INFO_RATE_DRIVER_DATA_SIZE 24

#define IEEE80211_TX_MAX_RATES	5

struct ieee80211_tx_rate {
	s8 idx;
	u8 count;
	u8 flags;
} __packed;

struct ieee80211_tx_info {
	
	u32 flags;
	u8 band;

	u8 antenna_sel_tx;

	u16 ack_frame_id;

	union {
		struct {
			union {
				
				struct {
					struct ieee80211_tx_rate rates[
						IEEE80211_TX_MAX_RATES];
					s8 rts_cts_rate_idx;
				};
				
				unsigned long jiffies;
			};
			
			struct ieee80211_vif *vif;
			struct ieee80211_key_conf *hw_key;
			struct ieee80211_sta *sta;
		} control;
		struct {
			struct ieee80211_tx_rate rates[IEEE80211_TX_MAX_RATES];
			u8 ampdu_ack_len;
			int ack_signal;
			u8 ampdu_len;
			
		} status;
		struct {
			struct ieee80211_tx_rate driver_rates[
				IEEE80211_TX_MAX_RATES];
			void *rate_driver_data[
				IEEE80211_TX_INFO_RATE_DRIVER_DATA_SIZE / sizeof(void *)];
		};
		void *driver_data[
			IEEE80211_TX_INFO_DRIVER_DATA_SIZE / sizeof(void *)];
	};
};

struct ieee80211_sched_scan_ies {
	u8 *ie[IEEE80211_NUM_BANDS];
	size_t len[IEEE80211_NUM_BANDS];
};

static inline struct ieee80211_tx_info *IEEE80211_SKB_CB(struct sk_buff *skb)
{
	return (struct ieee80211_tx_info *)skb->cb;
}

static inline struct ieee80211_rx_status *IEEE80211_SKB_RXCB(struct sk_buff *skb)
{
	return (struct ieee80211_rx_status *)skb->cb;
}

static inline void
ieee80211_tx_info_clear_status(struct ieee80211_tx_info *info)
{
	int i;

	BUILD_BUG_ON(offsetof(struct ieee80211_tx_info, status.rates) !=
		     offsetof(struct ieee80211_tx_info, control.rates));
	BUILD_BUG_ON(offsetof(struct ieee80211_tx_info, status.rates) !=
		     offsetof(struct ieee80211_tx_info, driver_rates));
	BUILD_BUG_ON(offsetof(struct ieee80211_tx_info, status.rates) != 8);
	
	for (i = 0; i < IEEE80211_TX_MAX_RATES; i++)
		info->status.rates[i].count = 0;

	BUILD_BUG_ON(
	    offsetof(struct ieee80211_tx_info, status.ampdu_ack_len) != 23);
	memset(&info->status.ampdu_ack_len, 0,
	       sizeof(struct ieee80211_tx_info) -
	       offsetof(struct ieee80211_tx_info, status.ampdu_ack_len));
}


enum mac80211_rx_flags {
	RX_FLAG_MMIC_ERROR	= 1<<0,
	RX_FLAG_DECRYPTED	= 1<<1,
	RX_FLAG_MMIC_STRIPPED	= 1<<3,
	RX_FLAG_IV_STRIPPED	= 1<<4,
	RX_FLAG_FAILED_FCS_CRC	= 1<<5,
	RX_FLAG_FAILED_PLCP_CRC = 1<<6,
	RX_FLAG_MACTIME_MPDU	= 1<<7,
	RX_FLAG_SHORTPRE	= 1<<8,
	RX_FLAG_HT		= 1<<9,
	RX_FLAG_40MHZ		= 1<<10,
	RX_FLAG_SHORT_GI	= 1<<11,
	RX_FLAG_NO_SIGNAL_VAL	= 1<<12,
};

struct ieee80211_rx_status {
	u64 mactime;
	enum ieee80211_band band;
	int freq;
	int signal;
	int antenna;
	int rate_idx;
	int flag;
	unsigned int rx_flags;
};

enum ieee80211_conf_flags {
	IEEE80211_CONF_MONITOR		= (1<<0),
	IEEE80211_CONF_PS		= (1<<1),
	IEEE80211_CONF_IDLE		= (1<<2),
	IEEE80211_CONF_OFFCHANNEL	= (1<<3),
};


enum ieee80211_conf_changed {
	IEEE80211_CONF_CHANGE_SMPS		= BIT(1),
	IEEE80211_CONF_CHANGE_LISTEN_INTERVAL	= BIT(2),
	IEEE80211_CONF_CHANGE_MONITOR		= BIT(3),
	IEEE80211_CONF_CHANGE_PS		= BIT(4),
	IEEE80211_CONF_CHANGE_POWER		= BIT(5),
	IEEE80211_CONF_CHANGE_CHANNEL		= BIT(6),
	IEEE80211_CONF_CHANGE_RETRY_LIMITS	= BIT(7),
	IEEE80211_CONF_CHANGE_IDLE		= BIT(8),
};

enum ieee80211_smps_mode {
	IEEE80211_SMPS_AUTOMATIC,
	IEEE80211_SMPS_OFF,
	IEEE80211_SMPS_STATIC,
	IEEE80211_SMPS_DYNAMIC,

	
	IEEE80211_SMPS_NUM_MODES,
};

struct ieee80211_conf {
	u32 flags;
	int power_level, dynamic_ps_timeout;
	int max_sleep_period;

	u16 listen_interval;
	u8 ps_dtim_period;

	u8 long_frame_max_tx_count, short_frame_max_tx_count;

	struct ieee80211_channel *channel;
	enum nl80211_channel_type channel_type;
	enum ieee80211_smps_mode smps_mode;
};

struct ieee80211_channel_switch {
	u64 timestamp;
	bool block_tx;
	struct ieee80211_channel *channel;
	u8 count;
};

enum ieee80211_vif_flags {
	IEEE80211_VIF_BEACON_FILTER		= BIT(0),
	IEEE80211_VIF_SUPPORTS_CQM_RSSI		= BIT(1),
};

struct ieee80211_vif {
	enum nl80211_iftype type;
	struct ieee80211_bss_conf bss_conf;
	u8 addr[ETH_ALEN];
	bool p2p;
	u32 driver_flags;
	
	u8 drv_priv[0] __attribute__((__aligned__(sizeof(void *))));
};

static inline bool ieee80211_vif_is_mesh(struct ieee80211_vif *vif)
{
#ifdef CONFIG_MAC80211_MESH
	return vif->type == NL80211_IFTYPE_MESH_POINT;
#endif
	return false;
}

enum ieee80211_key_flags {
	IEEE80211_KEY_FLAG_WMM_STA	= 1<<0,
	IEEE80211_KEY_FLAG_GENERATE_IV	= 1<<1,
	IEEE80211_KEY_FLAG_GENERATE_MMIC= 1<<2,
	IEEE80211_KEY_FLAG_PAIRWISE	= 1<<3,
	IEEE80211_KEY_FLAG_SW_MGMT	= 1<<4,
	IEEE80211_KEY_FLAG_PUT_IV_SPACE = 1<<5,
};

struct ieee80211_key_conf {
	u32 cipher;
	u8 icv_len;
	u8 iv_len;
	u8 hw_key_idx;
	u8 flags;
	s8 keyidx;
	u8 keylen;
	u8 key[0];
};

enum set_key_cmd {
	SET_KEY, DISABLE_KEY,
};

enum ieee80211_sta_state {
	
	IEEE80211_STA_NOTEXIST,
	IEEE80211_STA_NONE,
	IEEE80211_STA_AUTH,
	IEEE80211_STA_ASSOC,
	IEEE80211_STA_AUTHORIZED,
};

struct ieee80211_sta {
	u32 supp_rates[IEEE80211_NUM_BANDS];
	u8 addr[ETH_ALEN];
	u16 aid;
	struct ieee80211_sta_ht_cap ht_cap;
	bool wme;
	u8 uapsd_queues;
	u8 max_sp;

	
	u8 drv_priv[0] __attribute__((__aligned__(sizeof(void *))));
};

enum sta_notify_cmd {
	STA_NOTIFY_SLEEP, STA_NOTIFY_AWAKE,
};

enum ieee80211_hw_flags {
	IEEE80211_HW_HAS_RATE_CONTROL			= 1<<0,
	IEEE80211_HW_RX_INCLUDES_FCS			= 1<<1,
	IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING	= 1<<2,
	IEEE80211_HW_2GHZ_SHORT_SLOT_INCAPABLE		= 1<<3,
	IEEE80211_HW_2GHZ_SHORT_PREAMBLE_INCAPABLE	= 1<<4,
	IEEE80211_HW_SIGNAL_UNSPEC			= 1<<5,
	IEEE80211_HW_SIGNAL_DBM				= 1<<6,
	IEEE80211_HW_NEED_DTIM_PERIOD			= 1<<7,
	IEEE80211_HW_SPECTRUM_MGMT			= 1<<8,
	IEEE80211_HW_AMPDU_AGGREGATION			= 1<<9,
	IEEE80211_HW_SUPPORTS_PS			= 1<<10,
	IEEE80211_HW_PS_NULLFUNC_STACK			= 1<<11,
	IEEE80211_HW_SUPPORTS_DYNAMIC_PS		= 1<<12,
	IEEE80211_HW_MFP_CAPABLE			= 1<<13,
	
	IEEE80211_HW_SUPPORTS_STATIC_SMPS		= 1<<15,
	IEEE80211_HW_SUPPORTS_DYNAMIC_SMPS		= 1<<16,
	IEEE80211_HW_SUPPORTS_UAPSD			= 1<<17,
	IEEE80211_HW_REPORTS_TX_ACK_STATUS		= 1<<18,
	IEEE80211_HW_CONNECTION_MONITOR			= 1<<19,
	
	IEEE80211_HW_SUPPORTS_PER_STA_GTK		= 1<<21,
	IEEE80211_HW_AP_LINK_PS				= 1<<22,
	IEEE80211_HW_TX_AMPDU_SETUP_IN_HW		= 1<<23,
	IEEE80211_HW_SCAN_WHILE_IDLE			= 1<<24,
};

/**
 * struct ieee80211_hw - hardware information and state
 *
 * This structure contains the configuration and hardware
 * information for an 802.11 PHY.
 *
 * @wiphy: This points to the &struct wiphy allocated for this
 *	802.11 PHY. You must fill in the @perm_addr and @dev
 *	members of this structure using SET_IEEE80211_DEV()
 *	and SET_IEEE80211_PERM_ADDR(). Additionally, all supported
 *	bands (with channels, bitrates) are registered here.
 *
 * @conf: &struct ieee80211_conf, device configuration, don't use.
 *
 * @priv: pointer to private area that was allocated for driver use
 *	along with this structure.
 *
 * @flags: hardware flags, see &enum ieee80211_hw_flags.
 *
 * @extra_tx_headroom: headroom to reserve in each transmit skb
 *	for use by the driver (e.g. for transmit headers.)
 *
 * @channel_change_time: time (in microseconds) it takes to change channels.
 *
 * @max_signal: Maximum value for signal (rssi) in RX information, used
 *     only when @IEEE80211_HW_SIGNAL_UNSPEC or @IEEE80211_HW_SIGNAL_DB
 *
 * @max_listen_interval: max listen interval in units of beacon interval
 *     that HW supports
 *
 * @queues: number of available hardware transmit queues for
 *	data packets. WMM/QoS requires at least four, these
 *	queues need to have configurable access parameters.
 *
 * @rate_control_algorithm: rate control algorithm for this hardware.
 *	If unset (NULL), the default algorithm will be used. Must be
 *	set before calling ieee80211_register_hw().
 *
 * @vif_data_size: size (in bytes) of the drv_priv data area
 *	within &struct ieee80211_vif.
 * @sta_data_size: size (in bytes) of the drv_priv data area
 *	within &struct ieee80211_sta.
 *
 * @max_rates: maximum number of alternate rate retry stages the hw
 *	can handle.
 * @max_report_rates: maximum number of alternate rate retry stages
 *	the hw can report back.
 * @max_rate_tries: maximum number of tries for each stage
 *
 * @napi_weight: weight used for NAPI polling.  You must specify an
 *	appropriate value here if a napi_poll operation is provided
 *	by your driver.
 *
 * @max_rx_aggregation_subframes: maximum buffer size (number of
 *	sub-frames) to be used for A-MPDU block ack receiver
 *	aggregation.
 *	This is only relevant if the device has restrictions on the
 *	number of subframes, if it relies on mac80211 to do reordering
 *	it shouldn't be set.
 *
 * @max_tx_aggregation_subframes: maximum number of subframes in an
 *	aggregate an HT driver will transmit, used by the peer as a
 *	hint to size its reorder buffer.
 */
struct ieee80211_hw {
	struct ieee80211_conf conf;
	struct wiphy *wiphy;
	const char *rate_control_algorithm;
	void *priv;
	u32 flags;
	unsigned int extra_tx_headroom;
	int channel_change_time;
	int vif_data_size;
	int sta_data_size;
	int napi_weight;
	u16 queues;
	u16 max_listen_interval;
	s8 max_signal;
	u8 max_rates;
	u8 max_report_rates;
	u8 max_rate_tries;
	u8 max_rx_aggregation_subframes;
	u8 max_tx_aggregation_subframes;
};

struct ieee80211_hw *wiphy_to_ieee80211_hw(struct wiphy *wiphy);

static inline void SET_IEEE80211_DEV(struct ieee80211_hw *hw, struct device *dev)
{
	set_wiphy_dev(hw->wiphy, dev);
}

static inline void SET_IEEE80211_PERM_ADDR(struct ieee80211_hw *hw, u8 *addr)
{
	memcpy(hw->wiphy->perm_addr, addr, ETH_ALEN);
}

static inline struct ieee80211_rate *
ieee80211_get_tx_rate(const struct ieee80211_hw *hw,
		      const struct ieee80211_tx_info *c)
{
	if (WARN_ON_ONCE(c->control.rates[0].idx < 0))
		return NULL;
	return &hw->wiphy->bands[c->band]->bitrates[c->control.rates[0].idx];
}

static inline struct ieee80211_rate *
ieee80211_get_rts_cts_rate(const struct ieee80211_hw *hw,
			   const struct ieee80211_tx_info *c)
{
	if (c->control.rts_cts_rate_idx < 0)
		return NULL;
	return &hw->wiphy->bands[c->band]->bitrates[c->control.rts_cts_rate_idx];
}

static inline struct ieee80211_rate *
ieee80211_get_alt_retry_rate(const struct ieee80211_hw *hw,
			     const struct ieee80211_tx_info *c, int idx)
{
	if (c->control.rates[idx + 1].idx < 0)
		return NULL;
	return &hw->wiphy->bands[c->band]->bitrates[c->control.rates[idx + 1].idx];
}

void ieee80211_free_txskb(struct ieee80211_hw *hw, struct sk_buff *skb);







enum ieee80211_filter_flags {
	FIF_PROMISC_IN_BSS	= 1<<0,
	FIF_ALLMULTI		= 1<<1,
	FIF_FCSFAIL		= 1<<2,
	FIF_PLCPFAIL		= 1<<3,
	FIF_BCN_PRBRESP_PROMISC	= 1<<4,
	FIF_CONTROL		= 1<<5,
	FIF_OTHER_BSS		= 1<<6,
	FIF_PSPOLL		= 1<<7,
	FIF_PROBE_REQ		= 1<<8,
};

enum ieee80211_ampdu_mlme_action {
	IEEE80211_AMPDU_RX_START,
	IEEE80211_AMPDU_RX_STOP,
	IEEE80211_AMPDU_TX_START,
	IEEE80211_AMPDU_TX_STOP,
	IEEE80211_AMPDU_TX_OPERATIONAL,
};

enum ieee80211_frame_release_type {
	IEEE80211_FRAME_RELEASE_PSPOLL,
	IEEE80211_FRAME_RELEASE_UAPSD,
};

/**
 * struct ieee80211_ops - callbacks from mac80211 to the driver
 *
 * This structure contains various callbacks that the driver may
 * handle or, in some cases, must handle, for example to configure
 * the hardware to a new channel or to transmit a frame.
 *
 * @tx: Handler that 802.11 module calls for each transmitted frame.
 *	skb contains the buffer starting from the IEEE 802.11 header.
 *	The low-level driver should send the frame out based on
 *	configuration in the TX control data. This handler should,
 *	preferably, never fail and stop queues appropriately.
 *	This must be implemented if @tx_frags is not.
 *	Must be atomic.
 *
 * @tx_frags: Called to transmit multiple fragments of a single MSDU.
 *	This handler must consume all fragments, sending out some of
 *	them only is useless and it can't ask for some of them to be
 *	queued again. If the frame is not fragmented the queue has a
 *	single SKB only. To avoid issues with the networking stack
 *	when TX status is reported the frames should be removed from
 *	the skb queue.
 *	If this is used, the tx_info @vif and @sta pointers will be
 *	invalid -- you must not use them in that case.
 *	This must be implemented if @tx isn't.
 *	Must be atomic.
 *
 * @start: Called before the first netdevice attached to the hardware
 *	is enabled. This should turn on the hardware and must turn on
 *	frame reception (for possibly enabled monitor interfaces.)
 *	Returns negative error codes, these may be seen in userspace,
 *	or zero.
 *	When the device is started it should not have a MAC address
 *	to avoid acknowledging frames before a non-monitor device
 *	is added.
 *	Must be implemented and can sleep.
 *
 * @stop: Called after last netdevice attached to the hardware
 *	is disabled. This should turn off the hardware (at least
 *	it must turn off frame reception.)
 *	May be called right after add_interface if that rejects
 *	an interface. If you added any work onto the mac80211 workqueue
 *	you should ensure to cancel it on this callback.
 *	Must be implemented and can sleep.
 *
 * @suspend: Suspend the device; mac80211 itself will quiesce before and
 *	stop transmitting and doing any other configuration, and then
 *	ask the device to suspend. This is only invoked when WoWLAN is
 *	configured, otherwise the device is deconfigured completely and
 *	reconfigured at resume time.
 *	The driver may also impose special conditions under which it
 *	wants to use the "normal" suspend (deconfigure), say if it only
 *	supports WoWLAN when the device is associated. In this case, it
 *	must return 1 from this function.
 *
 * @resume: If WoWLAN was configured, this indicates that mac80211 is
 *	now resuming its operation, after this the device must be fully
 *	functional again. If this returns an error, the only way out is
 *	to also unregister the device. If it returns 1, then mac80211
 *	will also go through the regular complete restart on resume.
 *
 * @add_interface: Called when a netdevice attached to the hardware is
 *	enabled. Because it is not called for monitor mode devices, @start
 *	and @stop must be implemented.
 *	The driver should perform any initialization it needs before
 *	the device can be enabled. The initial configuration for the
 *	interface is given in the conf parameter.
 *	The callback may refuse to add an interface by returning a
 *	negative error code (which will be seen in userspace.)
 *	Must be implemented and can sleep.
 *
 * @change_interface: Called when a netdevice changes type. This callback
 *	is optional, but only if it is supported can interface types be
 *	switched while the interface is UP. The callback may sleep.
 *	Note that while an interface is being switched, it will not be
 *	found by the interface iteration callbacks.
 *
 * @remove_interface: Notifies a driver that an interface is going down.
 *	The @stop callback is called after this if it is the last interface
 *	and no monitor interfaces are present.
 *	When all interfaces are removed, the MAC address in the hardware
 *	must be cleared so the device no longer acknowledges packets,
 *	the mac_addr member of the conf structure is, however, set to the
 *	MAC address of the device going away.
 *	Hence, this callback must be implemented. It can sleep.
 *
 * @config: Handler for configuration requests. IEEE 802.11 code calls this
 *	function to change hardware configuration, e.g., channel.
 *	This function should never fail but returns a negative error code
 *	if it does. The callback can sleep.
 *
 * @bss_info_changed: Handler for configuration requests related to BSS
 *	parameters that may vary during BSS's lifespan, and may affect low
 *	level driver (e.g. assoc/disassoc status, erp parameters).
 *	This function should not be used if no BSS has been set, unless
 *	for association indication. The @changed parameter indicates which
 *	of the bss parameters has changed when a call is made. The callback
 *	can sleep.
 *
 * @prepare_multicast: Prepare for multicast filter configuration.
 *	This callback is optional, and its return value is passed
 *	to configure_filter(). This callback must be atomic.
 *
 * @configure_filter: Configure the device's RX filter.
 *	See the section "Frame filtering" for more information.
 *	This callback must be implemented and can sleep.
 *
 * @set_tim: Set TIM bit. mac80211 calls this function when a TIM bit
 * 	must be set or cleared for a given STA. Must be atomic.
 *
 * @set_key: See the section "Hardware crypto acceleration"
 *	This callback is only called between add_interface and
 *	remove_interface calls, i.e. while the given virtual interface
 *	is enabled.
 *	Returns a negative error code if the key can't be added.
 *	The callback can sleep.
 *
 * @update_tkip_key: See the section "Hardware crypto acceleration"
 * 	This callback will be called in the context of Rx. Called for drivers
 * 	which set IEEE80211_KEY_FLAG_TKIP_REQ_RX_P1_KEY.
 *	The callback must be atomic.
 *
 * @set_rekey_data: If the device supports GTK rekeying, for example while the
 *	host is suspended, it can assign this callback to retrieve the data
 *	necessary to do GTK rekeying, this is the KEK, KCK and replay counter.
 *	After rekeying was done it should (for example during resume) notify
 *	userspace of the new replay counter using ieee80211_gtk_rekey_notify().
 *
 * @hw_scan: Ask the hardware to service the scan request, no need to start
 *	the scan state machine in stack. The scan must honour the channel
 *	configuration done by the regulatory agent in the wiphy's
 *	registered bands. The hardware (or the driver) needs to make sure
 *	that power save is disabled.
 *	The @req ie/ie_len members are rewritten by mac80211 to contain the
 *	entire IEs after the SSID, so that drivers need not look at these
 *	at all but just send them after the SSID -- mac80211 includes the
 *	(extended) supported rates and HT information (where applicable).
 *	When the scan finishes, ieee80211_scan_completed() must be called;
 *	note that it also must be called when the scan cannot finish due to
 *	any error unless this callback returned a negative error code.
 *	The callback can sleep.
 *
 * @cancel_hw_scan: Ask the low-level tp cancel the active hw scan.
 *	The driver should ask the hardware to cancel the scan (if possible),
 *	but the scan will be completed only after the driver will call
 *	ieee80211_scan_completed().
 *	This callback is needed for wowlan, to prevent enqueueing a new
 *	scan_work after the low-level driver was already suspended.
 *	The callback can sleep.
 *
 * @sched_scan_start: Ask the hardware to start scanning repeatedly at
 *	specific intervals.  The driver must call the
 *	ieee80211_sched_scan_results() function whenever it finds results.
 *	This process will continue until sched_scan_stop is called.
 *
 * @sched_scan_stop: Tell the hardware to stop an ongoing scheduled scan.
 *
 * @sw_scan_start: Notifier function that is called just before a software scan
 *	is started. Can be NULL, if the driver doesn't need this notification.
 *	The callback can sleep.
 *
 * @sw_scan_complete: Notifier function that is called just after a
 *	software scan finished. Can be NULL, if the driver doesn't need
 *	this notification.
 *	The callback can sleep.
 *
 * @get_stats: Return low-level statistics.
 * 	Returns zero if statistics are available.
 *	The callback can sleep.
 *
 * @get_tkip_seq: If your device implements TKIP encryption in hardware this
 *	callback should be provided to read the TKIP transmit IVs (both IV32
 *	and IV16) for the given key from hardware.
 *	The callback must be atomic.
 *
 * @set_frag_threshold: Configuration of fragmentation threshold. Assign this
 *	if the device does fragmentation by itself; if this callback is
 *	implemented then the stack will not do fragmentation.
 *	The callback can sleep.
 *
 * @set_rts_threshold: Configuration of RTS threshold (if device needs it)
 *	The callback can sleep.
 *
 * @sta_add: Notifies low level driver about addition of an associated station,
 *	AP, IBSS/WDS/mesh peer etc. This callback can sleep.
 *
 * @sta_remove: Notifies low level driver about removal of an associated
 *	station, AP, IBSS/WDS/mesh peer etc. This callback can sleep.
 *
 * @sta_notify: Notifies low level driver about power state transition of an
 *	associated station, AP,  IBSS/WDS/mesh peer etc. For a VIF operating
 *	in AP mode, this callback will not be called when the flag
 *	%IEEE80211_HW_AP_LINK_PS is set. Must be atomic.
 *
 * @sta_state: Notifies low level driver about state transition of a
 *	station (which can be the AP, a client, IBSS/WDS/mesh peer etc.)
 *	This callback is mutually exclusive with @sta_add/@sta_remove.
 *	It must not fail for down transitions but may fail for transitions
 *	up the list of states.
 *	The callback can sleep.
 *
 * @conf_tx: Configure TX queue parameters (EDCF (aifs, cw_min, cw_max),
 *	bursting) for a hardware TX queue.
 *	Returns a negative error code on failure.
 *	The callback can sleep.
 *
 * @get_tsf: Get the current TSF timer value from firmware/hardware. Currently,
 *	this is only used for IBSS mode BSSID merging and debugging. Is not a
 *	required function.
 *	The callback can sleep.
 *
 * @set_tsf: Set the TSF timer to the specified value in the firmware/hardware.
 *      Currently, this is only used for IBSS mode debugging. Is not a
 *	required function.
 *	The callback can sleep.
 *
 * @reset_tsf: Reset the TSF timer and allow firmware/hardware to synchronize
 *	with other STAs in the IBSS. This is only used in IBSS mode. This
 *	function is optional if the firmware/hardware takes full care of
 *	TSF synchronization.
 *	The callback can sleep.
 *
 * @tx_last_beacon: Determine whether the last IBSS beacon was sent by us.
 *	This is needed only for IBSS mode and the result of this function is
 *	used to determine whether to reply to Probe Requests.
 *	Returns non-zero if this device sent the last beacon.
 *	The callback can sleep.
 *
 * @ampdu_action: Perform a certain A-MPDU action
 * 	The RA/TID combination determines the destination and TID we want
 * 	the ampdu action to be performed for. The action is defined through
 * 	ieee80211_ampdu_mlme_action. Starting sequence number (@ssn)
 * 	is the first frame we expect to perform the action on. Notice
 * 	that TX/RX_STOP can pass NULL for this parameter.
 *	The @buf_size parameter is only valid when the action is set to
 *	%IEEE80211_AMPDU_TX_OPERATIONAL and indicates the peer's reorder
 *	buffer size (number of subframes) for this session -- the driver
 *	may neither send aggregates containing more subframes than this
 *	nor send aggregates in a way that lost frames would exceed the
 *	buffer size. If just limiting the aggregate size, this would be
 *	possible with a buf_size of 8:
 *	 - TX: 1.....7
 *	 - RX:  2....7 (lost frame #1)
 *	 - TX:        8..1...
 *	which is invalid since #1 was now re-transmitted well past the
 *	buffer size of 8. Correct ways to retransmit #1 would be:
 *	 - TX:       1 or 18 or 81
 *	Even "189" would be wrong since 1 could be lost again.
 *
 *	Returns a negative error code on failure.
 *	The callback can sleep.
 *
 * @get_survey: Return per-channel survey information
 *
 * @rfkill_poll: Poll rfkill hardware state. If you need this, you also
 *	need to set wiphy->rfkill_poll to %true before registration,
 *	and need to call wiphy_rfkill_set_hw_state() in the callback.
 *	The callback can sleep.
 *
 * @set_coverage_class: Set slot time for given coverage class as specified
 *	in IEEE 802.11-2007 section 17.3.8.6 and modify ACK timeout
 *	accordingly. This callback is not required and may sleep.
 *
 * @testmode_cmd: Implement a cfg80211 test mode command.
 *	The callback can sleep.
 * @testmode_dump: Implement a cfg80211 test mode dump. The callback can sleep.
 *
 * @flush: Flush all pending frames from the hardware queue, making sure
 *	that the hardware queues are empty. If the parameter @drop is set
 *	to %true, pending frames may be dropped. The callback can sleep.
 *
 * @channel_switch: Drivers that need (or want) to offload the channel
 *	switch operation for CSAs received from the AP may implement this
 *	callback. They must then call ieee80211_chswitch_done() to indicate
 *	completion of the channel switch.
 *
 * @napi_poll: Poll Rx queue for incoming data frames.
 *
 * @set_antenna: Set antenna configuration (tx_ant, rx_ant) on the device.
 *	Parameters are bitmaps of allowed antennas to use for TX/RX. Drivers may
 *	reject TX/RX mask combinations they cannot support by returning -EINVAL
 *	(also see nl80211.h @NL80211_ATTR_WIPHY_ANTENNA_TX).
 *
 * @get_antenna: Get current antenna configuration from device (tx_ant, rx_ant).
 *
 * @remain_on_channel: Starts an off-channel period on the given channel, must
 *	call back to ieee80211_ready_on_channel() when on that channel. Note
 *	that normal channel traffic is not stopped as this is intended for hw
 *	offload. Frames to transmit on the off-channel channel are transmitted
 *	normally except for the %IEEE80211_TX_CTL_TX_OFFCHAN flag. When the
 *	duration (which will always be non-zero) expires, the driver must call
 *	ieee80211_remain_on_channel_expired(). This callback may sleep.
 * @cancel_remain_on_channel: Requests that an ongoing off-channel period is
 *	aborted before it expires. This callback may sleep.
 *
 * @set_ringparam: Set tx and rx ring sizes.
 *
 * @get_ringparam: Get tx and rx ring current and maximum sizes.
 *
 * @tx_frames_pending: Check if there is any pending frame in the hardware
 *	queues before entering power save.
 *
 * @set_bitrate_mask: Set a mask of rates to be used for rate control selection
 *	when transmitting a frame. Currently only legacy rates are handled.
 *	The callback can sleep.
 * @rssi_callback: Notify driver when the average RSSI goes above/below
 *	thresholds that were registered previously. The callback can sleep.
 *
 * @release_buffered_frames: Release buffered frames according to the given
 *	parameters. In the case where the driver buffers some frames for
 *	sleeping stations mac80211 will use this callback to tell the driver
 *	to release some frames, either for PS-poll or uAPSD.
 *	Note that if the @more_data paramter is %false the driver must check
 *	if there are more frames on the given TIDs, and if there are more than
 *	the frames being released then it must still set the more-data bit in
 *	the frame. If the @more_data parameter is %true, then of course the
 *	more-data bit must always be set.
 *	The @tids parameter tells the driver which TIDs to release frames
 *	from, for PS-poll it will always have only a single bit set.
 *	In the case this is used for a PS-poll initiated release, the
 *	@num_frames parameter will always be 1 so code can be shared. In
 *	this case the driver must also set %IEEE80211_TX_STATUS_EOSP flag
 *	on the TX status (and must report TX status) so that the PS-poll
 *	period is properly ended. This is used to avoid sending multiple
 *	responses for a retried PS-poll frame.
 *	In the case this is used for uAPSD, the @num_frames parameter may be
 *	bigger than one, but the driver may send fewer frames (it must send
 *	at least one, however). In this case it is also responsible for
 *	setting the EOSP flag in the QoS header of the frames. Also, when the
 *	service period ends, the driver must set %IEEE80211_TX_STATUS_EOSP
 *	on the last frame in the SP. Alternatively, it may call the function
 *	ieee80211_sta_eosp_irqsafe() to inform mac80211 of the end of the SP.
 *	This callback must be atomic.
 * @allow_buffered_frames: Prepare device to allow the given number of frames
 *	to go out to the given station. The frames will be sent by mac80211
 *	via the usual TX path after this call. The TX information for frames
 *	released will also have the %IEEE80211_TX_CTL_NO_PS_BUFFER flag set
 *	and the last one will also have %IEEE80211_TX_STATUS_EOSP set. In case
 *	frames from multiple TIDs are released and the driver might reorder
 *	them between the TIDs, it must set the %IEEE80211_TX_STATUS_EOSP flag
 *	on the last frame and clear it on all others and also handle the EOSP
 *	bit in the QoS header correctly. Alternatively, it can also call the
 *	ieee80211_sta_eosp_irqsafe() function.
 *	The @tids parameter is a bitmap and tells the driver which TIDs the
 *	frames will be on; it will at most have two bits set.
 *	This callback must be atomic.
 */
struct ieee80211_ops {
	void (*tx)(struct ieee80211_hw *hw, struct sk_buff *skb);
	void (*tx_frags)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			 struct ieee80211_sta *sta, struct sk_buff_head *skbs);
	int (*start)(struct ieee80211_hw *hw);
	void (*stop)(struct ieee80211_hw *hw);
#ifdef CONFIG_PM
	int (*suspend)(struct ieee80211_hw *hw, struct cfg80211_wowlan *wowlan);
	int (*resume)(struct ieee80211_hw *hw);
#endif
	int (*add_interface)(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif);
	int (*change_interface)(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				enum nl80211_iftype new_type, bool p2p);
	void (*remove_interface)(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif);
	int (*config)(struct ieee80211_hw *hw, u32 changed);
	void (*bss_info_changed)(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct ieee80211_bss_conf *info,
				 u32 changed);

	u64 (*prepare_multicast)(struct ieee80211_hw *hw,
				 struct netdev_hw_addr_list *mc_list);
	void (*configure_filter)(struct ieee80211_hw *hw,
				 unsigned int changed_flags,
				 unsigned int *total_flags,
				 u64 multicast);
	int (*set_tim)(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
		       bool set);
	int (*set_key)(struct ieee80211_hw *hw, enum set_key_cmd cmd,
		       struct ieee80211_vif *vif, struct ieee80211_sta *sta,
		       struct ieee80211_key_conf *key);
	void (*update_tkip_key)(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct ieee80211_key_conf *conf,
				struct ieee80211_sta *sta,
				u32 iv32, u16 *phase1key);
	void (*set_rekey_data)(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       struct cfg80211_gtk_rekey_data *data);
	int (*hw_scan)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		       struct cfg80211_scan_request *req);
	void (*cancel_hw_scan)(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif);
	int (*sched_scan_start)(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct cfg80211_sched_scan_request *req,
				struct ieee80211_sched_scan_ies *ies);
	void (*sched_scan_stop)(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif);
	void (*sw_scan_start)(struct ieee80211_hw *hw);
	void (*sw_scan_complete)(struct ieee80211_hw *hw);
	int (*get_stats)(struct ieee80211_hw *hw,
			 struct ieee80211_low_level_stats *stats);
	void (*get_tkip_seq)(struct ieee80211_hw *hw, u8 hw_key_idx,
			     u32 *iv32, u16 *iv16);
	int (*set_frag_threshold)(struct ieee80211_hw *hw, u32 value);
	int (*set_rts_threshold)(struct ieee80211_hw *hw, u32 value);
	int (*sta_add)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		       struct ieee80211_sta *sta);
	int (*sta_remove)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			  struct ieee80211_sta *sta);
	void (*sta_notify)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			enum sta_notify_cmd, struct ieee80211_sta *sta);
	int (*sta_state)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			 struct ieee80211_sta *sta,
			 enum ieee80211_sta_state old_state,
			 enum ieee80211_sta_state new_state);
	int (*conf_tx)(struct ieee80211_hw *hw,
		       struct ieee80211_vif *vif, u16 queue,
		       const struct ieee80211_tx_queue_params *params);
	u64 (*get_tsf)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	void (*set_tsf)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			u64 tsf);
	void (*reset_tsf)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	int (*tx_last_beacon)(struct ieee80211_hw *hw);
	int (*ampdu_action)(struct ieee80211_hw *hw,
			    struct ieee80211_vif *vif,
			    enum ieee80211_ampdu_mlme_action action,
			    struct ieee80211_sta *sta, u16 tid, u16 *ssn,
			    u8 buf_size);
	int (*get_survey)(struct ieee80211_hw *hw, int idx,
		struct survey_info *survey);
	void (*rfkill_poll)(struct ieee80211_hw *hw);
	void (*set_coverage_class)(struct ieee80211_hw *hw, u8 coverage_class);
#ifdef CONFIG_NL80211_TESTMODE
	int (*testmode_cmd)(struct ieee80211_hw *hw, void *data, int len);
	int (*testmode_dump)(struct ieee80211_hw *hw, struct sk_buff *skb,
			     struct netlink_callback *cb,
			     void *data, int len);
#endif
	void (*flush)(struct ieee80211_hw *hw, bool drop);
	void (*channel_switch)(struct ieee80211_hw *hw,
			       struct ieee80211_channel_switch *ch_switch);
	int (*napi_poll)(struct ieee80211_hw *hw, int budget);
	int (*set_antenna)(struct ieee80211_hw *hw, u32 tx_ant, u32 rx_ant);
	int (*get_antenna)(struct ieee80211_hw *hw, u32 *tx_ant, u32 *rx_ant);

	int (*remain_on_channel)(struct ieee80211_hw *hw,
				 struct ieee80211_channel *chan,
				 enum nl80211_channel_type channel_type,
				 int duration);
	int (*cancel_remain_on_channel)(struct ieee80211_hw *hw);
	int (*set_ringparam)(struct ieee80211_hw *hw, u32 tx, u32 rx);
	void (*get_ringparam)(struct ieee80211_hw *hw,
			      u32 *tx, u32 *tx_max, u32 *rx, u32 *rx_max);
	bool (*tx_frames_pending)(struct ieee80211_hw *hw);
	int (*set_bitrate_mask)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
				const struct cfg80211_bitrate_mask *mask);
	void (*rssi_callback)(struct ieee80211_hw *hw,
			      enum ieee80211_rssi_event rssi_event);

	void (*allow_buffered_frames)(struct ieee80211_hw *hw,
				      struct ieee80211_sta *sta,
				      u16 tids, int num_frames,
				      enum ieee80211_frame_release_type reason,
				      bool more_data);
	void (*release_buffered_frames)(struct ieee80211_hw *hw,
					struct ieee80211_sta *sta,
					u16 tids, int num_frames,
					enum ieee80211_frame_release_type reason,
					bool more_data);
};

struct ieee80211_hw *ieee80211_alloc_hw(size_t priv_data_len,
					const struct ieee80211_ops *ops);

int ieee80211_register_hw(struct ieee80211_hw *hw);

struct ieee80211_tpt_blink {
	int throughput;
	int blink_time;
};

enum ieee80211_tpt_led_trigger_flags {
	IEEE80211_TPT_LEDTRIG_FL_RADIO		= BIT(0),
	IEEE80211_TPT_LEDTRIG_FL_WORK		= BIT(1),
	IEEE80211_TPT_LEDTRIG_FL_CONNECTED	= BIT(2),
};

#ifdef CONFIG_MAC80211_LEDS
extern char *__ieee80211_get_tx_led_name(struct ieee80211_hw *hw);
extern char *__ieee80211_get_rx_led_name(struct ieee80211_hw *hw);
extern char *__ieee80211_get_assoc_led_name(struct ieee80211_hw *hw);
extern char *__ieee80211_get_radio_led_name(struct ieee80211_hw *hw);
extern char *__ieee80211_create_tpt_led_trigger(
				struct ieee80211_hw *hw, unsigned int flags,
				const struct ieee80211_tpt_blink *blink_table,
				unsigned int blink_table_len);
#endif
static inline char *ieee80211_get_tx_led_name(struct ieee80211_hw *hw)
{
#ifdef CONFIG_MAC80211_LEDS
	return __ieee80211_get_tx_led_name(hw);
#else
	return NULL;
#endif
}

static inline char *ieee80211_get_rx_led_name(struct ieee80211_hw *hw)
{
#ifdef CONFIG_MAC80211_LEDS
	return __ieee80211_get_rx_led_name(hw);
#else
	return NULL;
#endif
}

static inline char *ieee80211_get_assoc_led_name(struct ieee80211_hw *hw)
{
#ifdef CONFIG_MAC80211_LEDS
	return __ieee80211_get_assoc_led_name(hw);
#else
	return NULL;
#endif
}

static inline char *ieee80211_get_radio_led_name(struct ieee80211_hw *hw)
{
#ifdef CONFIG_MAC80211_LEDS
	return __ieee80211_get_radio_led_name(hw);
#else
	return NULL;
#endif
}

static inline char *
ieee80211_create_tpt_led_trigger(struct ieee80211_hw *hw, unsigned int flags,
				 const struct ieee80211_tpt_blink *blink_table,
				 unsigned int blink_table_len)
{
#ifdef CONFIG_MAC80211_LEDS
	return __ieee80211_create_tpt_led_trigger(hw, flags, blink_table,
						  blink_table_len);
#else
	return NULL;
#endif
}

void ieee80211_unregister_hw(struct ieee80211_hw *hw);

void ieee80211_free_hw(struct ieee80211_hw *hw);

void ieee80211_restart_hw(struct ieee80211_hw *hw);

void ieee80211_napi_schedule(struct ieee80211_hw *hw);

void ieee80211_napi_complete(struct ieee80211_hw *hw);

void ieee80211_rx(struct ieee80211_hw *hw, struct sk_buff *skb);

void ieee80211_rx_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb);

static inline void ieee80211_rx_ni(struct ieee80211_hw *hw,
				   struct sk_buff *skb)
{
	local_bh_disable();
	ieee80211_rx(hw, skb);
	local_bh_enable();
}

int ieee80211_sta_ps_transition(struct ieee80211_sta *sta, bool start);

static inline int ieee80211_sta_ps_transition_ni(struct ieee80211_sta *sta,
						  bool start)
{
	int ret;

	local_bh_disable();
	ret = ieee80211_sta_ps_transition(sta, start);
	local_bh_enable();

	return ret;
}

#define IEEE80211_TX_STATUS_HEADROOM	14

void ieee80211_sta_set_buffered(struct ieee80211_sta *sta,
				u8 tid, bool buffered);

void ieee80211_tx_status(struct ieee80211_hw *hw,
			 struct sk_buff *skb);

static inline void ieee80211_tx_status_ni(struct ieee80211_hw *hw,
					  struct sk_buff *skb)
{
	local_bh_disable();
	ieee80211_tx_status(hw, skb);
	local_bh_enable();
}

void ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw,
				 struct sk_buff *skb);

void ieee80211_report_low_ack(struct ieee80211_sta *sta, u32 num_packets);

struct sk_buff *ieee80211_beacon_get_tim(struct ieee80211_hw *hw,
					 struct ieee80211_vif *vif,
					 u16 *tim_offset, u16 *tim_length);

static inline struct sk_buff *ieee80211_beacon_get(struct ieee80211_hw *hw,
						   struct ieee80211_vif *vif)
{
	return ieee80211_beacon_get_tim(hw, vif, NULL, NULL);
}

struct sk_buff *ieee80211_proberesp_get(struct ieee80211_hw *hw,
					struct ieee80211_vif *vif);

struct sk_buff *ieee80211_pspoll_get(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif);

struct sk_buff *ieee80211_nullfunc_get(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif);

struct sk_buff *ieee80211_probereq_get(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       const u8 *ssid, size_t ssid_len,
				       const u8 *ie, size_t ie_len);

void ieee80211_rts_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		       const void *frame, size_t frame_len,
		       const struct ieee80211_tx_info *frame_txctl,
		       struct ieee80211_rts *rts);

__le16 ieee80211_rts_duration(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif, size_t frame_len,
			      const struct ieee80211_tx_info *frame_txctl);

void ieee80211_ctstoself_get(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     const void *frame, size_t frame_len,
			     const struct ieee80211_tx_info *frame_txctl,
			     struct ieee80211_cts *cts);

__le16 ieee80211_ctstoself_duration(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    size_t frame_len,
				    const struct ieee80211_tx_info *frame_txctl);

__le16 ieee80211_generic_frame_duration(struct ieee80211_hw *hw,
					struct ieee80211_vif *vif,
					size_t frame_len,
					struct ieee80211_rate *rate);

struct sk_buff *
ieee80211_get_buffered_bc(struct ieee80211_hw *hw, struct ieee80211_vif *vif);

/**
 * ieee80211_get_tkip_p1k_iv - get a TKIP phase 1 key for IV32
 *
 * This function returns the TKIP phase 1 key for the given IV32.
 *
 * @keyconf: the parameter passed with the set key
 * @iv32: IV32 to get the P1K for
 * @p1k: a buffer to which the key will be written, as 5 u16 values
 */
void ieee80211_get_tkip_p1k_iv(struct ieee80211_key_conf *keyconf,
			       u32 iv32, u16 *p1k);

/**
 * ieee80211_get_tkip_p1k - get a TKIP phase 1 key
 *
 * This function returns the TKIP phase 1 key for the IV32 taken
 * from the given packet.
 *
 * @keyconf: the parameter passed with the set key
 * @skb: the packet to take the IV32 value from that will be encrypted
 *	with this P1K
 * @p1k: a buffer to which the key will be written, as 5 u16 values
 */
static inline void ieee80211_get_tkip_p1k(struct ieee80211_key_conf *keyconf,
					  struct sk_buff *skb, u16 *p1k)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	const u8 *data = (u8 *)hdr + ieee80211_hdrlen(hdr->frame_control);
	u32 iv32 = get_unaligned_le32(&data[4]);

	ieee80211_get_tkip_p1k_iv(keyconf, iv32, p1k);
}

/**
 * ieee80211_get_tkip_rx_p1k - get a TKIP phase 1 key for RX
 *
 * This function returns the TKIP phase 1 key for the given IV32
 * and transmitter address.
 *
 * @keyconf: the parameter passed with the set key
 * @ta: TA that will be used with the key
 * @iv32: IV32 to get the P1K for
 * @p1k: a buffer to which the key will be written, as 5 u16 values
 */
void ieee80211_get_tkip_rx_p1k(struct ieee80211_key_conf *keyconf,
			       const u8 *ta, u32 iv32, u16 *p1k);

/**
 * ieee80211_get_tkip_p2k - get a TKIP phase 2 key
 *
 * This function computes the TKIP RC4 key for the IV values
 * in the packet.
 *
 * @keyconf: the parameter passed with the set key
 * @skb: the packet to take the IV32/IV16 values from that will be
 *	encrypted with this key
 * @p2k: a buffer to which the key will be written, 16 bytes
 */
void ieee80211_get_tkip_p2k(struct ieee80211_key_conf *keyconf,
			    struct sk_buff *skb, u8 *p2k);

struct ieee80211_key_seq {
	union {
		struct {
			u32 iv32;
			u16 iv16;
		} tkip;
		struct {
			u8 pn[6];
		} ccmp;
		struct {
			u8 pn[6];
		} aes_cmac;
	};
};

void ieee80211_get_key_tx_seq(struct ieee80211_key_conf *keyconf,
			      struct ieee80211_key_seq *seq);

void ieee80211_get_key_rx_seq(struct ieee80211_key_conf *keyconf,
			      int tid, struct ieee80211_key_seq *seq);

void ieee80211_gtk_rekey_notify(struct ieee80211_vif *vif, const u8 *bssid,
				const u8 *replay_ctr, gfp_t gfp);

void ieee80211_wake_queue(struct ieee80211_hw *hw, int queue);

void ieee80211_stop_queue(struct ieee80211_hw *hw, int queue);


int ieee80211_queue_stopped(struct ieee80211_hw *hw, int queue);

void ieee80211_stop_queues(struct ieee80211_hw *hw);

void ieee80211_wake_queues(struct ieee80211_hw *hw);

void ieee80211_scan_completed(struct ieee80211_hw *hw, bool aborted);

void ieee80211_sched_scan_results(struct ieee80211_hw *hw);

void ieee80211_sched_scan_stopped(struct ieee80211_hw *hw);

void ieee80211_iterate_active_interfaces(struct ieee80211_hw *hw,
					 void (*iterator)(void *data, u8 *mac,
						struct ieee80211_vif *vif),
					 void *data);

void ieee80211_iterate_active_interfaces_atomic(struct ieee80211_hw *hw,
						void (*iterator)(void *data,
						    u8 *mac,
						    struct ieee80211_vif *vif),
						void *data);

void ieee80211_queue_work(struct ieee80211_hw *hw, struct work_struct *work);

void ieee80211_queue_delayed_work(struct ieee80211_hw *hw,
				  struct delayed_work *dwork,
				  unsigned long delay);

int ieee80211_start_tx_ba_session(struct ieee80211_sta *sta, u16 tid,
				  u16 timeout);

void ieee80211_start_tx_ba_cb_irqsafe(struct ieee80211_vif *vif, const u8 *ra,
				      u16 tid);

int ieee80211_stop_tx_ba_session(struct ieee80211_sta *sta, u16 tid);

void ieee80211_stop_tx_ba_cb_irqsafe(struct ieee80211_vif *vif, const u8 *ra,
				     u16 tid);

struct ieee80211_sta *ieee80211_find_sta(struct ieee80211_vif *vif,
					 const u8 *addr);

struct ieee80211_sta *ieee80211_find_sta_by_ifaddr(struct ieee80211_hw *hw,
					       const u8 *addr,
					       const u8 *localaddr);

void ieee80211_sta_block_awake(struct ieee80211_hw *hw,
			       struct ieee80211_sta *pubsta, bool block);

void ieee80211_sta_eosp_irqsafe(struct ieee80211_sta *pubsta);

void ieee80211_iter_keys(struct ieee80211_hw *hw,
			 struct ieee80211_vif *vif,
			 void (*iter)(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      struct ieee80211_sta *sta,
				      struct ieee80211_key_conf *key,
				      void *data),
			 void *iter_data);

struct sk_buff *ieee80211_ap_probereq_get(struct ieee80211_hw *hw,
					  struct ieee80211_vif *vif);

void ieee80211_beacon_loss(struct ieee80211_vif *vif);

void ieee80211_connection_loss(struct ieee80211_vif *vif);

void ieee80211_resume_disconnect(struct ieee80211_vif *vif);

void ieee80211_disable_dyn_ps(struct ieee80211_vif *vif);

void ieee80211_enable_dyn_ps(struct ieee80211_vif *vif);

void ieee80211_cqm_rssi_notify(struct ieee80211_vif *vif,
			       enum nl80211_cqm_rssi_threshold_event rssi_event,
			       gfp_t gfp);

unsigned char ieee80211_get_operstate(struct ieee80211_vif *vif);

void ieee80211_chswitch_done(struct ieee80211_vif *vif, bool success);

void ieee80211_request_smps(struct ieee80211_vif *vif,
			    enum ieee80211_smps_mode smps_mode);

void ieee80211_key_removed(struct ieee80211_key_conf *key_conf);

void ieee80211_ready_on_channel(struct ieee80211_hw *hw);

void ieee80211_remain_on_channel_expired(struct ieee80211_hw *hw);

void ieee80211_stop_rx_ba_session(struct ieee80211_vif *vif, u16 ba_rx_bitmap,
				  const u8 *addr);

void ieee80211_send_bar(struct ieee80211_vif *vif, u8 *ra, u16 tid, u16 ssn);


enum rate_control_changed {
	IEEE80211_RC_HT_CHANGED		= BIT(0),
	IEEE80211_RC_SMPS_CHANGED	= BIT(1),
};

struct ieee80211_tx_rate_control {
	struct ieee80211_hw *hw;
	struct ieee80211_supported_band *sband;
	struct ieee80211_bss_conf *bss_conf;
	struct sk_buff *skb;
	struct ieee80211_tx_rate reported_rate;
	bool rts, short_preamble;
	u8 max_rate_idx;
	u32 rate_idx_mask;
	u8 rate_idx_mcs_mask[IEEE80211_HT_MCS_MASK_LEN];
	bool bss;
};

struct rate_control_ops {
	struct module *module;
	const char *name;
	void *(*alloc)(struct ieee80211_hw *hw, struct dentry *debugfsdir);
	void (*free)(void *priv);

	void *(*alloc_sta)(void *priv, struct ieee80211_sta *sta, gfp_t gfp);
	void (*rate_init)(void *priv, struct ieee80211_supported_band *sband,
			  struct ieee80211_sta *sta, void *priv_sta);
	void (*rate_update)(void *priv, struct ieee80211_supported_band *sband,
			    struct ieee80211_sta *sta,
			    void *priv_sta, u32 changed,
			    enum nl80211_channel_type oper_chan_type);
	void (*free_sta)(void *priv, struct ieee80211_sta *sta,
			 void *priv_sta);

	void (*tx_status)(void *priv, struct ieee80211_supported_band *sband,
			  struct ieee80211_sta *sta, void *priv_sta,
			  struct sk_buff *skb);
	void (*get_rate)(void *priv, struct ieee80211_sta *sta, void *priv_sta,
			 struct ieee80211_tx_rate_control *txrc);

	void (*add_sta_debugfs)(void *priv, void *priv_sta,
				struct dentry *dir);
	void (*remove_sta_debugfs)(void *priv, void *priv_sta);
};

static inline int rate_supported(struct ieee80211_sta *sta,
				 enum ieee80211_band band,
				 int index)
{
	return (sta == NULL || sta->supp_rates[band] & BIT(index));
}

bool rate_control_send_low(struct ieee80211_sta *sta,
			   void *priv_sta,
			   struct ieee80211_tx_rate_control *txrc);


static inline s8
rate_lowest_index(struct ieee80211_supported_band *sband,
		  struct ieee80211_sta *sta)
{
	int i;

	for (i = 0; i < sband->n_bitrates; i++)
		if (rate_supported(sta, sband->band, i))
			return i;

	
	WARN_ON_ONCE(1);

	
	return 0;
}

static inline
bool rate_usable_index_exists(struct ieee80211_supported_band *sband,
			      struct ieee80211_sta *sta)
{
	unsigned int i;

	for (i = 0; i < sband->n_bitrates; i++)
		if (rate_supported(sta, sband->band, i))
			return true;
	return false;
}

int ieee80211_rate_control_register(struct rate_control_ops *ops);
void ieee80211_rate_control_unregister(struct rate_control_ops *ops);

static inline bool
conf_is_ht20(struct ieee80211_conf *conf)
{
	return conf->channel_type == NL80211_CHAN_HT20;
}

static inline bool
conf_is_ht40_minus(struct ieee80211_conf *conf)
{
	return conf->channel_type == NL80211_CHAN_HT40MINUS;
}

static inline bool
conf_is_ht40_plus(struct ieee80211_conf *conf)
{
	return conf->channel_type == NL80211_CHAN_HT40PLUS;
}

static inline bool
conf_is_ht40(struct ieee80211_conf *conf)
{
	return conf_is_ht40_minus(conf) || conf_is_ht40_plus(conf);
}

static inline bool
conf_is_ht(struct ieee80211_conf *conf)
{
	return conf->channel_type != NL80211_CHAN_NO_HT;
}

static inline enum nl80211_iftype
ieee80211_iftype_p2p(enum nl80211_iftype type, bool p2p)
{
	if (p2p) {
		switch (type) {
		case NL80211_IFTYPE_STATION:
			return NL80211_IFTYPE_P2P_CLIENT;
		case NL80211_IFTYPE_AP:
			return NL80211_IFTYPE_P2P_GO;
		default:
			break;
		}
	}
	return type;
}

static inline enum nl80211_iftype
ieee80211_vif_type_p2p(struct ieee80211_vif *vif)
{
	return ieee80211_iftype_p2p(vif->type, vif->p2p);
}

void ieee80211_enable_rssi_reports(struct ieee80211_vif *vif,
				   int rssi_min_thold,
				   int rssi_max_thold);

void ieee80211_disable_rssi_reports(struct ieee80211_vif *vif);

int ieee80211_add_srates_ie(struct ieee80211_vif *vif, struct sk_buff *skb);

int ieee80211_add_ext_srates_ie(struct ieee80211_vif *vif,
				struct sk_buff *skb);
#endif 
