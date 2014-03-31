/*
 * This file is part of wl1271
 *
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

#ifndef __CONF_H__
#define __CONF_H__

enum {
	CONF_HW_BIT_RATE_1MBPS   = BIT(0),
	CONF_HW_BIT_RATE_2MBPS   = BIT(1),
	CONF_HW_BIT_RATE_5_5MBPS = BIT(2),
	CONF_HW_BIT_RATE_6MBPS   = BIT(3),
	CONF_HW_BIT_RATE_9MBPS   = BIT(4),
	CONF_HW_BIT_RATE_11MBPS  = BIT(5),
	CONF_HW_BIT_RATE_12MBPS  = BIT(6),
	CONF_HW_BIT_RATE_18MBPS  = BIT(7),
	CONF_HW_BIT_RATE_22MBPS  = BIT(8),
	CONF_HW_BIT_RATE_24MBPS  = BIT(9),
	CONF_HW_BIT_RATE_36MBPS  = BIT(10),
	CONF_HW_BIT_RATE_48MBPS  = BIT(11),
	CONF_HW_BIT_RATE_54MBPS  = BIT(12),
	CONF_HW_BIT_RATE_MCS_0   = BIT(13),
	CONF_HW_BIT_RATE_MCS_1   = BIT(14),
	CONF_HW_BIT_RATE_MCS_2   = BIT(15),
	CONF_HW_BIT_RATE_MCS_3   = BIT(16),
	CONF_HW_BIT_RATE_MCS_4   = BIT(17),
	CONF_HW_BIT_RATE_MCS_5   = BIT(18),
	CONF_HW_BIT_RATE_MCS_6   = BIT(19),
	CONF_HW_BIT_RATE_MCS_7   = BIT(20)
};

enum {
	CONF_HW_RATE_INDEX_1MBPS   = 0,
	CONF_HW_RATE_INDEX_2MBPS   = 1,
	CONF_HW_RATE_INDEX_5_5MBPS = 2,
	CONF_HW_RATE_INDEX_6MBPS   = 3,
	CONF_HW_RATE_INDEX_9MBPS   = 4,
	CONF_HW_RATE_INDEX_11MBPS  = 5,
	CONF_HW_RATE_INDEX_12MBPS  = 6,
	CONF_HW_RATE_INDEX_18MBPS  = 7,
	CONF_HW_RATE_INDEX_22MBPS  = 8,
	CONF_HW_RATE_INDEX_24MBPS  = 9,
	CONF_HW_RATE_INDEX_36MBPS  = 10,
	CONF_HW_RATE_INDEX_48MBPS  = 11,
	CONF_HW_RATE_INDEX_54MBPS  = 12,
	CONF_HW_RATE_INDEX_MAX     = CONF_HW_RATE_INDEX_54MBPS,
};

enum {
	CONF_HW_RXTX_RATE_MCS7_SGI = 0,
	CONF_HW_RXTX_RATE_MCS7,
	CONF_HW_RXTX_RATE_MCS6,
	CONF_HW_RXTX_RATE_MCS5,
	CONF_HW_RXTX_RATE_MCS4,
	CONF_HW_RXTX_RATE_MCS3,
	CONF_HW_RXTX_RATE_MCS2,
	CONF_HW_RXTX_RATE_MCS1,
	CONF_HW_RXTX_RATE_MCS0,
	CONF_HW_RXTX_RATE_54,
	CONF_HW_RXTX_RATE_48,
	CONF_HW_RXTX_RATE_36,
	CONF_HW_RXTX_RATE_24,
	CONF_HW_RXTX_RATE_22,
	CONF_HW_RXTX_RATE_18,
	CONF_HW_RXTX_RATE_12,
	CONF_HW_RXTX_RATE_11,
	CONF_HW_RXTX_RATE_9,
	CONF_HW_RXTX_RATE_6,
	CONF_HW_RXTX_RATE_5_5,
	CONF_HW_RXTX_RATE_2,
	CONF_HW_RXTX_RATE_1,
	CONF_HW_RXTX_RATE_MAX,
	CONF_HW_RXTX_RATE_UNSUPPORTED = 0xff
};

#define CONF_HW_RXTX_RATE_MCS_MIN CONF_HW_RXTX_RATE_MCS7_SGI
#define CONF_HW_RXTX_RATE_MCS_MAX CONF_HW_RXTX_RATE_MCS0

enum {
	CONF_SG_DISABLE = 0,
	CONF_SG_PROTECTIVE,
	CONF_SG_OPPORTUNISTIC
};

enum {
	CONF_SG_ACL_BT_MASTER_MIN_BR = 0,
	CONF_SG_ACL_BT_MASTER_MAX_BR,

	CONF_SG_ACL_BT_SLAVE_MIN_BR,
	CONF_SG_ACL_BT_SLAVE_MAX_BR,

	CONF_SG_ACL_BT_MASTER_MIN_EDR,
	CONF_SG_ACL_BT_MASTER_MAX_EDR,

	CONF_SG_ACL_BT_SLAVE_MIN_EDR,
	CONF_SG_ACL_BT_SLAVE_MAX_EDR,

	CONF_SG_ACL_WLAN_PS_MASTER_BR,
	CONF_SG_ACL_WLAN_PS_SLAVE_BR,

	CONF_SG_ACL_WLAN_PS_MASTER_EDR,
	CONF_SG_ACL_WLAN_PS_SLAVE_EDR,

	
	CONF_SG_ACL_WLAN_ACTIVE_MASTER_MIN_BR,
	CONF_SG_ACL_WLAN_ACTIVE_MASTER_MAX_BR,
	CONF_SG_ACL_WLAN_ACTIVE_SLAVE_MIN_BR,
	CONF_SG_ACL_WLAN_ACTIVE_SLAVE_MAX_BR,
	CONF_SG_ACL_WLAN_ACTIVE_MASTER_MIN_EDR,
	CONF_SG_ACL_WLAN_ACTIVE_MASTER_MAX_EDR,
	CONF_SG_ACL_WLAN_ACTIVE_SLAVE_MIN_EDR,
	CONF_SG_ACL_WLAN_ACTIVE_SLAVE_MAX_EDR,

	CONF_SG_ACL_ACTIVE_SCAN_WLAN_BR,
	CONF_SG_ACL_ACTIVE_SCAN_WLAN_EDR,
	CONF_SG_ACL_PASSIVE_SCAN_BT_BR,
	CONF_SG_ACL_PASSIVE_SCAN_WLAN_BR,
	CONF_SG_ACL_PASSIVE_SCAN_BT_EDR,
	CONF_SG_ACL_PASSIVE_SCAN_WLAN_EDR,

	CONF_SG_AUTO_SCAN_PROBE_REQ,

	CONF_SG_ACTIVE_SCAN_DURATION_FACTOR_HV3,

	CONF_SG_ACTIVE_SCAN_DURATION_FACTOR_A2DP,

	CONF_SG_PASSIVE_SCAN_DURATION_FACTOR_A2DP_BR,

	CONF_SG_PASSIVE_SCAN_DURATION_FACTOR_A2DP_EDR,

	CONF_SG_PASSIVE_SCAN_DURATION_FACTOR_HV3,

	
	CONF_SG_CONSECUTIVE_HV3_IN_PASSIVE_SCAN,
	CONF_SG_BCN_HV3_COLLISION_THRESH_IN_PASSIVE_SCAN,
	CONF_SG_TX_RX_PROTECTION_BWIDTH_IN_PASSIVE_SCAN,

	CONF_SG_STA_FORCE_PS_IN_BT_SCO,

	CONF_SG_ANTENNA_CONFIGURATION,

	CONF_SG_BEACON_MISS_PERCENT,

	CONF_SG_DHCP_TIME,

	CONF_SG_RXT,


	CONF_SG_TXT,

	CONF_SG_ADAPTIVE_RXT_TXT,

	
	CONF_SG_GENERAL_USAGE_BIT_MAP,

	CONF_SG_HV3_MAX_SERVED,

	CONF_SG_PS_POLL_TIMEOUT,

	CONF_SG_UPSD_TIMEOUT,

	CONF_SG_CONSECUTIVE_CTS_THRESHOLD,
	CONF_SG_STA_RX_WINDOW_AFTER_DTIM,
	CONF_SG_STA_CONNECTION_PROTECTION_TIME,

	
	CONF_AP_BEACON_MISS_TX,
	CONF_AP_RX_WINDOW_AFTER_BEACON,
	CONF_AP_BEACON_WINDOW_INTERVAL,
	CONF_AP_CONNECTION_PROTECTION_TIME,
	CONF_AP_BT_ACL_VAL_BT_SERVE_TIME,
	CONF_AP_BT_ACL_VAL_WL_SERVE_TIME,

	
	CONF_SG_CTS_DILUTED_BAD_RX_PACKETS_TH,
	CONF_SG_CTS_CHOP_IN_DUAL_ANT_SCO_MASTER,

	CONF_SG_TEMP_PARAM_1,
	CONF_SG_TEMP_PARAM_2,
	CONF_SG_TEMP_PARAM_3,
	CONF_SG_TEMP_PARAM_4,
	CONF_SG_TEMP_PARAM_5,
	CONF_SG_TEMP_PARAM_6,
	CONF_SG_TEMP_PARAM_7,
	CONF_SG_TEMP_PARAM_8,
	CONF_SG_TEMP_PARAM_9,
	CONF_SG_TEMP_PARAM_10,

	CONF_SG_PARAMS_MAX,
	CONF_SG_PARAMS_ALL = 0xff
};

struct conf_sg_settings {
	u32 params[CONF_SG_PARAMS_MAX];
	u8 state;
};

enum conf_rx_queue_type {
	CONF_RX_QUEUE_TYPE_LOW_PRIORITY,  
	CONF_RX_QUEUE_TYPE_HIGH_PRIORITY, 
};

struct conf_rx_settings {
	u32 rx_msdu_life_time;

	u32 packet_detection_threshold;

	u16 ps_poll_timeout;
	u16 upsd_timeout;

	u16 rts_threshold;

	u16 rx_cca_threshold;

	u16 irq_blk_threshold;

	u16 irq_pkt_threshold;

	u16 irq_timeout;

	u8 queue_type;
};

#define CONF_TX_MAX_RATE_CLASSES       10

#define CONF_TX_RATE_MASK_UNSPECIFIED  0
#define CONF_TX_RATE_MASK_BASIC        (CONF_HW_BIT_RATE_1MBPS | \
					CONF_HW_BIT_RATE_2MBPS)
#define CONF_TX_RATE_RETRY_LIMIT       10

#define CONF_TX_RATE_MASK_BASIC_P2P    (CONF_HW_BIT_RATE_6MBPS | \
	CONF_HW_BIT_RATE_12MBPS | CONF_HW_BIT_RATE_24MBPS)

#define CONF_TX_AP_ENABLED_RATES       (CONF_HW_BIT_RATE_1MBPS | \
	CONF_HW_BIT_RATE_2MBPS | CONF_HW_BIT_RATE_5_5MBPS |      \
	CONF_HW_BIT_RATE_6MBPS | CONF_HW_BIT_RATE_9MBPS |        \
	CONF_HW_BIT_RATE_11MBPS | CONF_HW_BIT_RATE_12MBPS |      \
	CONF_HW_BIT_RATE_18MBPS | CONF_HW_BIT_RATE_24MBPS |      \
	CONF_HW_BIT_RATE_36MBPS | CONF_HW_BIT_RATE_48MBPS |      \
	CONF_HW_BIT_RATE_54MBPS)

#define CONF_TX_CCK_RATES  (CONF_HW_BIT_RATE_1MBPS |		\
	CONF_HW_BIT_RATE_2MBPS | CONF_HW_BIT_RATE_5_5MBPS |	\
	CONF_HW_BIT_RATE_11MBPS)

#define CONF_TX_OFDM_RATES (CONF_HW_BIT_RATE_6MBPS |             \
	CONF_HW_BIT_RATE_12MBPS | CONF_HW_BIT_RATE_24MBPS |      \
	CONF_HW_BIT_RATE_36MBPS | CONF_HW_BIT_RATE_48MBPS |      \
	CONF_HW_BIT_RATE_54MBPS)

#define CONF_TX_MCS_RATES (CONF_HW_BIT_RATE_MCS_0 |              \
	CONF_HW_BIT_RATE_MCS_1 | CONF_HW_BIT_RATE_MCS_2 |        \
	CONF_HW_BIT_RATE_MCS_3 | CONF_HW_BIT_RATE_MCS_4 |        \
	CONF_HW_BIT_RATE_MCS_5 | CONF_HW_BIT_RATE_MCS_6 |        \
	CONF_HW_BIT_RATE_MCS_7)

#define CONF_TX_AP_DEFAULT_MGMT_RATES  (CONF_HW_BIT_RATE_1MBPS | \
	CONF_HW_BIT_RATE_2MBPS | CONF_HW_BIT_RATE_5_5MBPS)

#define CONF_TX_IBSS_DEFAULT_RATES  (CONF_HW_BIT_RATE_1MBPS |       \
		CONF_HW_BIT_RATE_2MBPS | CONF_HW_BIT_RATE_5_5MBPS | \
		CONF_HW_BIT_RATE_11MBPS | CONF_TX_OFDM_RATES);

struct conf_tx_rate_class {

	u32 enabled_rates;

	u8 short_retry_limit;

	u8 long_retry_limit;

	u8 aflags;
};

#define CONF_TX_MAX_AC_COUNT 4

#define CONF_TX_AIFS_PIFS 1
#define CONF_TX_AIFS_DIFS 2


enum conf_tx_ac {
	CONF_TX_AC_BE = 0,         
	CONF_TX_AC_BK = 1,         
	CONF_TX_AC_VI = 2,         
	CONF_TX_AC_VO = 3,         
	CONF_TX_AC_CTS2SELF = 4,   
	CONF_TX_AC_ANY_TID = 0x1f
};

struct conf_tx_ac_category {
	u8 ac;

	u8 cw_min;

	u16 cw_max;

	u8 aifsn;

	u16 tx_op_limit;
};

#define CONF_TX_MAX_TID_COUNT 8

#define CONF_TX_BA_ENABLED_TID_BITMAP 0x3F

enum {
	CONF_CHANNEL_TYPE_DCF = 0,   
	CONF_CHANNEL_TYPE_EDCF = 1,  
	CONF_CHANNEL_TYPE_HCCA = 2,  
};

enum {
	CONF_PS_SCHEME_LEGACY = 0,
	CONF_PS_SCHEME_UPSD_TRIGGER = 1,
	CONF_PS_SCHEME_LEGACY_PSPOLL = 2,
	CONF_PS_SCHEME_SAPSD = 3,
};

enum {
	CONF_ACK_POLICY_LEGACY = 0,
	CONF_ACK_POLICY_NO_ACK = 1,
	CONF_ACK_POLICY_BLOCK = 2,
};


struct conf_tx_tid {
	u8 queue_id;
	u8 channel_type;
	u8 tsid;
	u8 ps_scheme;
	u8 ack_policy;
	u32 apsd_conf[2];
};

struct conf_tx_settings {
	u8 tx_energy_detection;

	struct conf_tx_rate_class sta_rc_conf;

	u8 ac_conf_count;
	struct conf_tx_ac_category ac_conf[CONF_TX_MAX_AC_COUNT];

	u8 max_tx_retries;

	u16 ap_aging_period;

	u8 tid_conf_count;
	struct conf_tx_tid tid_conf[CONF_TX_MAX_TID_COUNT];

	u16 frag_threshold;

	u16 tx_compl_timeout;

	u16 tx_compl_threshold;

	u32 basic_rate;

	u32 basic_rate_5;

	u8 tmpl_short_retry_limit;
	u8 tmpl_long_retry_limit;

	
	u32 tx_watchdog_timeout;
};

enum {
	CONF_WAKE_UP_EVENT_BEACON    = 0x01, 
	CONF_WAKE_UP_EVENT_DTIM      = 0x02, 
	CONF_WAKE_UP_EVENT_N_DTIM    = 0x04, 
	CONF_WAKE_UP_EVENT_N_BEACONS = 0x08, 
	CONF_WAKE_UP_EVENT_BITS_MASK = 0x0F
};

#define CONF_MAX_BCN_FILT_IE_COUNT 32

#define CONF_BCN_RULE_PASS_ON_CHANGE         BIT(0)
#define CONF_BCN_RULE_PASS_ON_APPEARANCE     BIT(1)

#define CONF_BCN_IE_OUI_LEN    3
#define CONF_BCN_IE_VER_LEN    2

struct conf_bcn_filt_rule {
	u8 ie;

	u8 rule;

	u8 oui[CONF_BCN_IE_OUI_LEN];

	u8 type;

	u8 version[CONF_BCN_IE_VER_LEN];
};

#define CONF_MAX_RSSI_SNR_TRIGGERS 8

enum {
	CONF_TRIG_METRIC_RSSI_BEACON = 0,
	CONF_TRIG_METRIC_RSSI_DATA,
	CONF_TRIG_METRIC_SNR_BEACON,
	CONF_TRIG_METRIC_SNR_DATA
};

enum {
	CONF_TRIG_EVENT_TYPE_LEVEL = 0,
	CONF_TRIG_EVENT_TYPE_EDGE
};

enum {
	CONF_TRIG_EVENT_DIR_LOW = 0,
	CONF_TRIG_EVENT_DIR_HIGH,
	CONF_TRIG_EVENT_DIR_BIDIR
};

struct conf_sig_weights {

	u8 rssi_bcn_avg_weight;

	u8 rssi_pkt_avg_weight;

	u8 snr_bcn_avg_weight;

	u8 snr_pkt_avg_weight;
};

enum conf_bcn_filt_mode {
	CONF_BCN_FILT_MODE_DISABLED = 0,
	CONF_BCN_FILT_MODE_ENABLED = 1
};

enum conf_bet_mode {
	CONF_BET_MODE_DISABLE = 0,
	CONF_BET_MODE_ENABLE = 1,
};

struct conf_conn_settings {
	u8 wake_up_event;

	u8 listen_interval;

	u8 suspend_wake_up_event;

	u8 suspend_listen_interval;

	enum conf_bcn_filt_mode bcn_filt_mode;

	u8 bcn_filt_ie_count;
	struct conf_bcn_filt_rule bcn_filt_ie[CONF_MAX_BCN_FILT_IE_COUNT];

	u32 synch_fail_thold;

	u32 bss_lose_timeout;

	u32 beacon_rx_timeout;

	u32 broadcast_timeout;

	u8 rx_broadcast_in_ps;

	u8 ps_poll_threshold;

	struct conf_sig_weights sig_weights;

	u8 bet_enable;

	u8 bet_max_consecutive;

	u8 psm_entry_retries;

	u8 psm_exit_retries;

	u8 psm_entry_nullfunc_retries;

	u16 dynamic_ps_timeout;

	u8 forced_ps;

	u32 keep_alive_interval;

	u8 max_listen_interval;
};

enum {
	CONF_REF_CLK_19_2_E,
	CONF_REF_CLK_26_E,
	CONF_REF_CLK_38_4_E,
	CONF_REF_CLK_52_E,
	CONF_REF_CLK_38_4_M_XTAL,
	CONF_REF_CLK_26_M_XTAL,
};

enum single_dual_band_enum {
	CONF_SINGLE_BAND,
	CONF_DUAL_BAND
};

#define CONF_RSSI_AND_PROCESS_COMPENSATION_SIZE 15
#define CONF_NUMBER_OF_SUB_BANDS_5  7
#define CONF_NUMBER_OF_RATE_GROUPS  6
#define CONF_NUMBER_OF_CHANNELS_2_4 14
#define CONF_NUMBER_OF_CHANNELS_5   35

struct conf_itrim_settings {
	
	u8 enable;

	
	u32 timeout;
};

struct conf_pm_config_settings {
	u32 host_clk_settling_time;

	bool host_fast_wakeup_support;
};

struct conf_roam_trigger_settings {
	u16 trigger_pacing;

	u8 avg_weight_rssi_beacon;

	u8 avg_weight_rssi_data;

	u8 avg_weight_snr_beacon;

	u8 avg_weight_snr_data;
};

struct conf_scan_settings {
	u32 min_dwell_time_active;

	u32 max_dwell_time_active;

	u32 min_dwell_time_passive;

	u32 max_dwell_time_passive;

	u16 num_probe_reqs;

	u32 split_scan_timeout;
};

struct conf_sched_scan_settings {
	
	u16 min_dwell_time_active;

	
	u16 max_dwell_time_active;

	
	u32 dwell_time_passive;

	
	u32 dwell_time_dfs;

	
	u8 num_probe_reqs;

	
	s8 rssi_threshold;

	
	s8 snr_threshold;
};

#define CONF_TX_PWR_COMPENSATION_LEN_2 7
#define CONF_TX_PWR_COMPENSATION_LEN_5 18

struct conf_rf_settings {
	u8 tx_per_channel_power_compensation_2[CONF_TX_PWR_COMPENSATION_LEN_2];

	u8 tx_per_channel_power_compensation_5[CONF_TX_PWR_COMPENSATION_LEN_5];
};

struct conf_ht_setting {
	u8 rx_ba_win_size;
	u8 tx_ba_win_size;
	u16 inactivity_timeout;

	
	u8 tx_ba_tid_bitmap;
};

struct conf_memory_settings {
	
	u8 num_stations;

	
	u8 ssid_profiles;

	
	u8 rx_block_num;

	
	u8 tx_min_block_num;

	
	u8 dynamic_memory;

	u8 min_req_tx_blocks;

	u8 min_req_rx_blocks;

	u8 tx_min;
};

struct conf_fm_coex {
	u8 enable;
	u8 swallow_period;
	u8 n_divider_fref_set_1;
	u8 n_divider_fref_set_2;
	u16 m_divider_fref_set_1;
	u16 m_divider_fref_set_2;
	u32 coex_pll_stabilization_time;
	u16 ldo_stabilization_time;
	u8 fm_disturbed_band_margin;
	u8 swallow_clk_diff;
};

struct conf_rx_streaming_settings {
	u32 duration;

	u8 queues;

	u8 interval;

	u8 always;
};

struct conf_fwlog {
	
	u8 mode;

	u8 mem_blocks;

	
	u8 severity;

	
	u8 timestamp;

	
	u8 output;

	
	u8 threshold;
};

#define ACX_RATE_MGMT_NUM_OF_RATES 13
struct conf_rate_policy_settings {
	u16 rate_retry_score;
	u16 per_add;
	u16 per_th1;
	u16 per_th2;
	u16 max_per;
	u8 inverse_curiosity_factor;
	u8 tx_fail_low_th;
	u8 tx_fail_high_th;
	u8 per_alpha_shift;
	u8 per_add_shift;
	u8 per_beta1_shift;
	u8 per_beta2_shift;
	u8 rate_check_up;
	u8 rate_check_down;
	u8 rate_retry_policy[ACX_RATE_MGMT_NUM_OF_RATES];
};

struct conf_hangover_settings {
	u32 recover_time;
	u8 hangover_period;
	u8 dynamic_mode;
	u8 early_termination_mode;
	u8 max_period;
	u8 min_period;
	u8 increase_delta;
	u8 decrease_delta;
	u8 quiet_time;
	u8 increase_time;
	u8 window_size;
};

struct conf_drv_settings {
	struct conf_sg_settings sg;
	struct conf_rx_settings rx;
	struct conf_tx_settings tx;
	struct conf_conn_settings conn;
	struct conf_itrim_settings itrim;
	struct conf_pm_config_settings pm_config;
	struct conf_roam_trigger_settings roam_trigger;
	struct conf_scan_settings scan;
	struct conf_sched_scan_settings sched_scan;
	struct conf_rf_settings rf;
	struct conf_ht_setting ht;
	struct conf_memory_settings mem_wl127x;
	struct conf_memory_settings mem_wl128x;
	struct conf_fm_coex fm_coex;
	struct conf_rx_streaming_settings rx_streaming;
	struct conf_fwlog fwlog;
	struct conf_rate_policy_settings rate;
	struct conf_hangover_settings hangover;
	u8 hci_io_ds;
};

#endif
