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

#ifndef __CMD_H__
#define __CMD_H__

#include "wl12xx.h"

struct acx_header;

int wl1271_cmd_send(struct wl1271 *wl, u16 id, void *buf, size_t len,
		    size_t res_len);
int wl1271_cmd_general_parms(struct wl1271 *wl);
int wl128x_cmd_general_parms(struct wl1271 *wl);
int wl1271_cmd_radio_parms(struct wl1271 *wl);
int wl128x_cmd_radio_parms(struct wl1271 *wl);
int wl1271_cmd_ext_radio_parms(struct wl1271 *wl);
int wl12xx_cmd_role_enable(struct wl1271 *wl, u8 *addr, u8 role_type,
			   u8 *role_id);
int wl12xx_cmd_role_disable(struct wl1271 *wl, u8 *role_id);
int wl12xx_cmd_role_start_sta(struct wl1271 *wl, struct wl12xx_vif *wlvif);
int wl12xx_cmd_role_stop_sta(struct wl1271 *wl, struct wl12xx_vif *wlvif);
int wl12xx_cmd_role_start_ap(struct wl1271 *wl, struct wl12xx_vif *wlvif);
int wl12xx_cmd_role_stop_ap(struct wl1271 *wl, struct wl12xx_vif *wlvif);
int wl12xx_cmd_role_start_ibss(struct wl1271 *wl, struct wl12xx_vif *wlvif);
int wl12xx_start_dev(struct wl1271 *wl, struct wl12xx_vif *wlvif);
int wl12xx_stop_dev(struct wl1271 *wl, struct wl12xx_vif *wlvif);
int wl1271_cmd_test(struct wl1271 *wl, void *buf, size_t buf_len, u8 answer);
int wl1271_cmd_interrogate(struct wl1271 *wl, u16 id, void *buf, size_t len);
int wl1271_cmd_configure(struct wl1271 *wl, u16 id, void *buf, size_t len);
int wl1271_cmd_data_path(struct wl1271 *wl, bool enable);
int wl1271_cmd_ps_mode(struct wl1271 *wl, struct wl12xx_vif *wlvif,
		       u8 ps_mode, u16 auto_ps_timeout);
int wl1271_cmd_read_memory(struct wl1271 *wl, u32 addr, void *answer,
			   size_t len);
int wl1271_cmd_template_set(struct wl1271 *wl, u8 role_id,
			    u16 template_id, void *buf, size_t buf_len,
			    int index, u32 rates);
int wl12xx_cmd_build_null_data(struct wl1271 *wl, struct wl12xx_vif *wlvif);
int wl1271_cmd_build_ps_poll(struct wl1271 *wl, struct wl12xx_vif *wlvif,
			     u16 aid);
int wl12xx_cmd_build_probe_req(struct wl1271 *wl, struct wl12xx_vif *wlvif,
			       u8 role_id, u8 band,
			       const u8 *ssid, size_t ssid_len,
			       const u8 *ie, size_t ie_len);
struct sk_buff *wl1271_cmd_build_ap_probe_req(struct wl1271 *wl,
					      struct wl12xx_vif *wlvif,
					      struct sk_buff *skb);
int wl1271_cmd_build_arp_rsp(struct wl1271 *wl, struct wl12xx_vif *wlvif);
int wl1271_build_qos_null_data(struct wl1271 *wl, struct ieee80211_vif *vif);
int wl12xx_cmd_build_klv_null_data(struct wl1271 *wl,
				   struct wl12xx_vif *wlvif);
int wl12xx_cmd_set_default_wep_key(struct wl1271 *wl, u8 id, u8 hlid);
int wl1271_cmd_set_sta_key(struct wl1271 *wl, struct wl12xx_vif *wlvif,
			   u16 action, u8 id, u8 key_type,
			   u8 key_size, const u8 *key, const u8 *addr,
			   u32 tx_seq_32, u16 tx_seq_16);
int wl1271_cmd_set_ap_key(struct wl1271 *wl, struct wl12xx_vif *wlvif,
			  u16 action, u8 id, u8 key_type,
			  u8 key_size, const u8 *key, u8 hlid, u32 tx_seq_32,
			  u16 tx_seq_16);
int wl12xx_cmd_set_peer_state(struct wl1271 *wl, u8 hlid);
int wl12xx_roc(struct wl1271 *wl, struct wl12xx_vif *wlvif, u8 role_id);
int wl12xx_croc(struct wl1271 *wl, u8 role_id);
int wl12xx_cmd_add_peer(struct wl1271 *wl, struct wl12xx_vif *wlvif,
			struct ieee80211_sta *sta, u8 hlid);
int wl12xx_cmd_remove_peer(struct wl1271 *wl, u8 hlid);
int wl12xx_cmd_config_fwlog(struct wl1271 *wl);
int wl12xx_cmd_start_fwlog(struct wl1271 *wl);
int wl12xx_cmd_stop_fwlog(struct wl1271 *wl);
int wl12xx_cmd_channel_switch(struct wl1271 *wl,
			      struct wl12xx_vif *wlvif,
			      struct ieee80211_channel_switch *ch_switch);
int wl12xx_cmd_stop_channel_switch(struct wl1271 *wl);
int wl12xx_allocate_link(struct wl1271 *wl, struct wl12xx_vif *wlvif,
			 u8 *hlid);
void wl12xx_free_link(struct wl1271 *wl, struct wl12xx_vif *wlvif, u8 *hlid);

enum wl1271_commands {
	CMD_INTERROGATE	= 1, 
	CMD_CONFIGURE	= 2, 
	CMD_ENABLE_RX	= 3,
	CMD_ENABLE_TX	= 4,
	CMD_DISABLE_RX	= 5,
	CMD_DISABLE_TX	= 6,
	CMD_SCAN	= 7,
	CMD_STOP_SCAN	= 8,
	CMD_SET_KEYS	= 9,
	CMD_READ_MEMORY	= 10,
	CMD_WRITE_MEMORY	= 11,
	CMD_SET_TEMPLATE	= 12,
	CMD_TEST		= 13,
	CMD_NOISE_HIST		= 14,
	CMD_QUIET_ELEMENT_SET_STATE = 15,
	CMD_SET_BCN_MODE	= 16,

	CMD_MEASUREMENT		= 17,
	CMD_STOP_MEASUREMENT	= 18,
	CMD_SET_PS_MODE		= 19,
	CMD_CHANNEL_SWITCH	= 20,
	CMD_STOP_CHANNEL_SWICTH = 21,
	CMD_AP_DISCOVERY	= 22,
	CMD_STOP_AP_DISCOVERY	= 23,
	CMD_HEALTH_CHECK	= 24,
	CMD_DEBUG		= 25,
	CMD_TRIGGER_SCAN_TO	= 26,
	CMD_CONNECTION_SCAN_CFG	= 27,
	CMD_CONNECTION_SCAN_SSID_CFG	= 28,
	CMD_START_PERIODIC_SCAN	= 29,
	CMD_STOP_PERIODIC_SCAN	= 30,
	CMD_SET_PEER_STATE	= 31,
	CMD_REMAIN_ON_CHANNEL	= 32,
	CMD_CANCEL_REMAIN_ON_CHANNEL	= 33,
	CMD_CONFIG_FWLOGGER		= 34,
	CMD_START_FWLOGGER			= 35,
	CMD_STOP_FWLOGGER			= 36,

	
	CMD_ADD_PEER		= 37,
	CMD_REMOVE_PEER		= 38,

	
	CMD_ROLE_ENABLE		= 39,
	CMD_ROLE_DISABLE	= 40,
	CMD_ROLE_START		= 41,
	CMD_ROLE_STOP		= 42,

	
	CMD_START_RADAR_DETECTION	= 43,
	CMD_STOP_RADAR_DETECTION	= 44,

	
	CMD_WFD_START_DISCOVERY	= 45,
	CMD_WFD_STOP_DISCOVERY	= 46,
	CMD_WFD_ATTRIBUTE_CONFIG	= 47,
	CMD_NOP			= 48,
	CMD_LAST_COMMAND,

	MAX_COMMAND_ID = 0xFFFF,
};

#define MAX_CMD_PARAMS 572

enum {
	CMD_TEMPL_KLV_IDX_NULL_DATA = 0,
	CMD_TEMPL_KLV_IDX_MAX = 4
};

enum cmd_templ {
	CMD_TEMPL_NULL_DATA = 0,
	CMD_TEMPL_BEACON,
	CMD_TEMPL_CFG_PROBE_REQ_2_4,
	CMD_TEMPL_CFG_PROBE_REQ_5,
	CMD_TEMPL_PROBE_RESPONSE,
	CMD_TEMPL_QOS_NULL_DATA,
	CMD_TEMPL_PS_POLL,
	CMD_TEMPL_KLV,
	CMD_TEMPL_DISCONNECT,
	CMD_TEMPL_PROBE_REQ_2_4, 
	CMD_TEMPL_PROBE_REQ_5,   
	CMD_TEMPL_BAR,           
	CMD_TEMPL_CTS,           
	CMD_TEMPL_AP_BEACON,
	CMD_TEMPL_AP_PROBE_RESPONSE,
	CMD_TEMPL_ARP_RSP,
	CMD_TEMPL_DEAUTH_AP,
	CMD_TEMPL_TEMPORARY,
	CMD_TEMPL_LINK_MEASUREMENT_REPORT,

	CMD_TEMPL_MAX = 0xff
};

#define WL1271_COMMAND_TIMEOUT     2000
#define WL1271_CMD_TEMPL_DFLT_SIZE 252
#define WL1271_CMD_TEMPL_MAX_SIZE  512
#define WL1271_EVENT_TIMEOUT       750

struct wl1271_cmd_header {
	__le16 id;
	__le16 status;
	
	u8 data[0];
} __packed;

#define WL1271_CMD_MAX_PARAMS 572

struct wl1271_command {
	struct wl1271_cmd_header header;
	u8  parameters[WL1271_CMD_MAX_PARAMS];
} __packed;

enum {
	CMD_MAILBOX_IDLE		=  0,
	CMD_STATUS_SUCCESS		=  1,
	CMD_STATUS_UNKNOWN_CMD		=  2,
	CMD_STATUS_UNKNOWN_IE		=  3,
	CMD_STATUS_REJECT_MEAS_SG_ACTIVE	= 11,
	CMD_STATUS_RX_BUSY		= 13,
	CMD_STATUS_INVALID_PARAM		= 14,
	CMD_STATUS_TEMPLATE_TOO_LARGE		= 15,
	CMD_STATUS_OUT_OF_MEMORY		= 16,
	CMD_STATUS_STA_TABLE_FULL		= 17,
	CMD_STATUS_RADIO_ERROR		= 18,
	CMD_STATUS_WRONG_NESTING		= 19,
	CMD_STATUS_TIMEOUT		= 21, 
	CMD_STATUS_FW_RESET		= 22, 
	CMD_STATUS_TEMPLATE_OOM		= 23,
	CMD_STATUS_NO_RX_BA_SESSION	= 24,
	MAX_COMMAND_STATUS		= 0xff
};

#define CMDMBOX_HEADER_LEN 4
#define CMDMBOX_INFO_ELEM_HEADER_LEN 4

enum {
	BSS_TYPE_IBSS = 0,
	BSS_TYPE_STA_BSS = 2,
	BSS_TYPE_AP_BSS = 3,
	MAX_BSS_TYPE = 0xFF
};

#define WL1271_JOIN_CMD_CTRL_TX_FLUSH     0x80 
#define WL1271_JOIN_CMD_TX_SESSION_OFFSET 1
#define WL1271_JOIN_CMD_BSS_TYPE_5GHZ 0x10

struct wl12xx_cmd_role_enable {
	struct wl1271_cmd_header header;

	u8 role_id;
	u8 role_type;
	u8 mac_address[ETH_ALEN];
} __packed;

struct wl12xx_cmd_role_disable {
	struct wl1271_cmd_header header;

	u8 role_id;
	u8 padding[3];
} __packed;

enum wl12xx_band {
	WL12XX_BAND_2_4GHZ		= 0,
	WL12XX_BAND_5GHZ		= 1,
	WL12XX_BAND_JAPAN_4_9_GHZ	= 2,
	WL12XX_BAND_DEFAULT		= WL12XX_BAND_2_4GHZ,
	WL12XX_BAND_INVALID		= 0x7E,
	WL12XX_BAND_MAX_RADIO		= 0x7F,
};

struct wl12xx_cmd_role_start {
	struct wl1271_cmd_header header;

	u8 role_id;
	u8 band;
	u8 channel;
	u8 padding;

	union {
		struct {
			u8 hlid;
			u8 session;
			u8 padding_1[54];
		} __packed device;
		
		struct {
			u8 bssid[ETH_ALEN];
			u8 hlid; 
			u8 session;
			__le32 remote_rates; 

			__le32 basic_rate_set;
			__le32 local_rates; 

			u8 ssid_type;
			u8 ssid_len;
			u8 ssid[IEEE80211_MAX_SSID_LEN];

			__le16 beacon_interval; 
		} __packed sta;
		struct {
			u8 bssid[ETH_ALEN];
			u8 hlid; 
			u8 dtim_interval;
			__le32 remote_rates; 

			__le32 basic_rate_set;
			__le32 local_rates; 

			u8 ssid_type;
			u8 ssid_len;
			u8 ssid[IEEE80211_MAX_SSID_LEN];

			__le16 beacon_interval; 

			u8 padding_1[4];
		} __packed ibss;
		
		struct {
			__le16 aging_period; 
			u8 beacon_expiry; 
			u8 bss_index;
			
			u8 global_hlid;
			
			u8 broadcast_hlid;

			__le16 beacon_interval; 

			__le32 basic_rate_set;
			__le32 local_rates; 

			u8 dtim_interval;

			u8 ssid_type;
			u8 ssid_len;
			u8 ssid[IEEE80211_MAX_SSID_LEN];

			u8 reset_tsf;

			u8 padding_1[4];
		} __packed ap;
	};
} __packed;

struct wl12xx_cmd_role_stop {
	struct wl1271_cmd_header header;

	u8 role_id;
	u8 disc_type; 
	__le16 reason; 
} __packed;

struct cmd_enabledisable_path {
	struct wl1271_cmd_header header;

	u8 channel;
	u8 padding[3];
} __packed;

#define WL1271_RATE_AUTOMATIC  0

struct wl1271_cmd_template_set {
	struct wl1271_cmd_header header;

	u8 role_id;
	u8 template_type;
	__le16 len;
	u8 index;  
	u8 padding[3];

	__le32 enabled_rates;
	u8 short_retry_limit;
	u8 long_retry_limit;
	u8 aflags;
	u8 reserved;

	u8 template_data[WL1271_CMD_TEMPL_MAX_SIZE];
} __packed;

#define TIM_ELE_ID    5
#define PARTIAL_VBM_MAX    251

struct wl1271_tim {
	u8 identity;
	u8 length;
	u8 dtim_count;
	u8 dtim_period;
	u8 bitmap_ctrl;
	u8 pvb_field[PARTIAL_VBM_MAX]; 
} __packed;

enum wl1271_cmd_ps_mode {
	STATION_AUTO_PS_MODE,   
	STATION_ACTIVE_MODE,
	STATION_POWER_SAVE_MODE
};

struct wl1271_cmd_ps_params {
	struct wl1271_cmd_header header;

	u8 role_id;
	u8 ps_mode; 
	u16 auto_ps_timeout;
} __packed;

#define NUM_ACCESS_CATEGORIES_COPY 4

enum wl1271_cmd_key_action {
	KEY_ADD_OR_REPLACE = 1,
	KEY_REMOVE         = 2,
	KEY_SET_ID         = 3,
	MAX_KEY_ACTION     = 0xffff,
};

enum wl1271_cmd_lid_key_type {
	UNICAST_LID_TYPE     = 0,
	BROADCAST_LID_TYPE   = 1,
	WEP_DEFAULT_LID_TYPE = 2
};

enum wl1271_cmd_key_type {
	KEY_NONE = 0,
	KEY_WEP  = 1,
	KEY_TKIP = 2,
	KEY_AES  = 3,
	KEY_GEM  = 4,
};

struct wl1271_cmd_set_keys {
	struct wl1271_cmd_header header;

	u8 hlid;

	u8 lid_key_type;

	u8 key_id;
	u8 reserved_1;

	
	__le16 key_action;

	
	u8 key_size;

	
	u8 key_type;

	
	u8 key[MAX_KEY_SIZE];
	__le16 ac_seq_num16[NUM_ACCESS_CATEGORIES_COPY];
	__le32 ac_seq_num32[NUM_ACCESS_CATEGORIES_COPY];
} __packed;

struct wl1271_cmd_test_header {
	u8 id;
	u8 padding[3];
} __packed;

enum wl1271_channel_tune_bands {
	WL1271_CHANNEL_TUNE_BAND_2_4,
	WL1271_CHANNEL_TUNE_BAND_5,
	WL1271_CHANNEL_TUNE_BAND_4_9
};

#define WL1271_PD_REFERENCE_POINT_BAND_B_G  0

#define TEST_CMD_INI_FILE_RADIO_PARAM       0x19
#define TEST_CMD_INI_FILE_GENERAL_PARAM     0x1E
#define TEST_CMD_INI_FILE_RF_EXTENDED_PARAM 0x26

struct wl1271_general_parms_cmd {
	struct wl1271_cmd_header header;

	struct wl1271_cmd_test_header test;

	struct wl1271_ini_general_params general_params;

	u8 sr_debug_table[WL1271_INI_MAX_SMART_REFLEX_PARAM];
	u8 sr_sen_n_p;
	u8 sr_sen_n_p_gain;
	u8 sr_sen_nrn;
	u8 sr_sen_prn;
	u8 padding[3];
} __packed;

struct wl128x_general_parms_cmd {
	struct wl1271_cmd_header header;

	struct wl1271_cmd_test_header test;

	struct wl128x_ini_general_params general_params;

	u8 sr_debug_table[WL1271_INI_MAX_SMART_REFLEX_PARAM];
	u8 sr_sen_n_p;
	u8 sr_sen_n_p_gain;
	u8 sr_sen_nrn;
	u8 sr_sen_prn;
	u8 padding[3];
} __packed;

struct wl1271_radio_parms_cmd {
	struct wl1271_cmd_header header;

	struct wl1271_cmd_test_header test;

	
	struct wl1271_ini_band_params_2 static_params_2;
	struct wl1271_ini_band_params_5 static_params_5;

	
	struct wl1271_ini_fem_params_2 dyn_params_2;
	u8 padding2;
	struct wl1271_ini_fem_params_5 dyn_params_5;
	u8 padding3[2];
} __packed;

struct wl128x_radio_parms_cmd {
	struct wl1271_cmd_header header;

	struct wl1271_cmd_test_header test;

	
	struct wl128x_ini_band_params_2 static_params_2;
	struct wl128x_ini_band_params_5 static_params_5;

	u8 fem_vendor_and_options;

	
	struct wl128x_ini_fem_params_2 dyn_params_2;
	u8 padding2;
	struct wl128x_ini_fem_params_5 dyn_params_5;
} __packed;

struct wl1271_ext_radio_parms_cmd {
	struct wl1271_cmd_header header;

	struct wl1271_cmd_test_header test;

	u8 tx_per_channel_power_compensation_2[CONF_TX_PWR_COMPENSATION_LEN_2];
	u8 tx_per_channel_power_compensation_5[CONF_TX_PWR_COMPENSATION_LEN_5];
	u8 padding[3];
} __packed;

enum wl1271_disconnect_type {
	DISCONNECT_IMMEDIATE,
	DISCONNECT_DEAUTH,
	DISCONNECT_DISASSOC
};

#define WL1271_CMD_STA_STATE_CONNECTED  1

struct wl12xx_cmd_set_peer_state {
	struct wl1271_cmd_header header;

	u8 hlid;
	u8 state;
	u8 padding[2];
} __packed;

struct wl12xx_cmd_roc {
	struct wl1271_cmd_header header;

	u8 role_id;
	u8 channel;
	u8 band;
	u8 padding;
};

struct wl12xx_cmd_croc {
	struct wl1271_cmd_header header;

	u8 role_id;
	u8 padding[3];
};

enum wl12xx_ssid_type {
	WL12XX_SSID_TYPE_PUBLIC = 0,
	WL12XX_SSID_TYPE_HIDDEN = 1,
	WL12XX_SSID_TYPE_ANY = 2,
};

enum wl1271_psd_type {
	WL1271_PSD_LEGACY = 0,
	WL1271_PSD_UPSD_TRIGGER = 1,
	WL1271_PSD_LEGACY_PSPOLL = 2,
	WL1271_PSD_SAPSD = 3
};

struct wl12xx_cmd_add_peer {
	struct wl1271_cmd_header header;

	u8 addr[ETH_ALEN];
	u8 hlid;
	u8 aid;
	u8 psd_type[NUM_ACCESS_CATEGORIES_COPY];
	__le32 supported_rates;
	u8 bss_index;
	u8 sp_len;
	u8 wmm;
	u8 padding1;
} __packed;

struct wl12xx_cmd_remove_peer {
	struct wl1271_cmd_header header;

	u8 hlid;
	u8 reason_opcode;
	u8 send_deauth_flag;
	u8 padding1;
} __packed;

enum wl12xx_fwlogger_log_mode {
	WL12XX_FWLOG_CONTINUOUS,
	WL12XX_FWLOG_ON_DEMAND
};

enum wl12xx_fwlogger_timestamp {
	WL12XX_FWLOG_TIMESTAMP_DISABLED,
	WL12XX_FWLOG_TIMESTAMP_ENABLED
};

enum wl12xx_fwlogger_output {
	WL12XX_FWLOG_OUTPUT_NONE,
	WL12XX_FWLOG_OUTPUT_DBG_PINS,
	WL12XX_FWLOG_OUTPUT_HOST,
};

struct wl12xx_cmd_config_fwlog {
	struct wl1271_cmd_header header;

	
	u8 logger_mode;

	
	u8 log_severity;

	
	u8 timestamp;

	
	u8 output;

	
	u8 threshold;

	u8 padding[3];
} __packed;

struct wl12xx_cmd_start_fwlog {
	struct wl1271_cmd_header header;
} __packed;

struct wl12xx_cmd_stop_fwlog {
	struct wl1271_cmd_header header;
} __packed;

struct wl12xx_cmd_channel_switch {
	struct wl1271_cmd_header header;

	u8 role_id;

	
	u8 channel;
	
	u8 switch_time;
	
	u8 stop_tx;
	
	u8 post_switch_tx_disable;

	u8 padding[3];
} __packed;

struct wl12xx_cmd_stop_channel_switch {
	struct wl1271_cmd_header header;
} __packed;

#endif 
