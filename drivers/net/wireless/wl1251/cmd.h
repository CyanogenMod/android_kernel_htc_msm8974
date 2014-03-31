/*
 * This file is part of wl1251
 *
 * Copyright (c) 1998-2007 Texas Instruments Incorporated
 * Copyright (C) 2008 Nokia Corporation
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

#ifndef __WL1251_CMD_H__
#define __WL1251_CMD_H__

#include "wl1251.h"

#include <net/cfg80211.h>

struct acx_header;

int wl1251_cmd_send(struct wl1251 *wl, u16 type, void *buf, size_t buf_len);
int wl1251_cmd_test(struct wl1251 *wl, void *buf, size_t buf_len, u8 answer);
int wl1251_cmd_interrogate(struct wl1251 *wl, u16 id, void *buf, size_t len);
int wl1251_cmd_configure(struct wl1251 *wl, u16 id, void *buf, size_t len);
int wl1251_cmd_vbm(struct wl1251 *wl, u8 identity,
		   void *bitmap, u16 bitmap_len, u8 bitmap_control);
int wl1251_cmd_data_path(struct wl1251 *wl, u8 channel, bool enable);
int wl1251_cmd_join(struct wl1251 *wl, u8 bss_type, u8 channel,
		    u16 beacon_interval, u8 dtim_interval);
int wl1251_cmd_ps_mode(struct wl1251 *wl, u8 ps_mode);
int wl1251_cmd_read_memory(struct wl1251 *wl, u32 addr, void *answer,
			   size_t len);
int wl1251_cmd_template_set(struct wl1251 *wl, u16 cmd_id,
			    void *buf, size_t buf_len);
int wl1251_cmd_scan(struct wl1251 *wl, u8 *ssid, size_t ssid_len,
		    struct ieee80211_channel *channels[],
		    unsigned int n_channels, unsigned int n_probes);
int wl1251_cmd_trigger_scan_to(struct wl1251 *wl, u32 timeout);

#define WL1251_COMMAND_TIMEOUT 2000

enum wl1251_commands {
	CMD_RESET           = 0,
	CMD_INTERROGATE     = 1,    
	CMD_CONFIGURE       = 2,    
	CMD_ENABLE_RX       = 3,
	CMD_ENABLE_TX       = 4,
	CMD_DISABLE_RX      = 5,
	CMD_DISABLE_TX      = 6,
	CMD_SCAN            = 8,
	CMD_STOP_SCAN       = 9,
	CMD_VBM             = 10,
	CMD_START_JOIN      = 11,
	CMD_SET_KEYS        = 12,
	CMD_READ_MEMORY     = 13,
	CMD_WRITE_MEMORY    = 14,
	CMD_BEACON          = 19,
	CMD_PROBE_RESP      = 20,
	CMD_NULL_DATA       = 21,
	CMD_PROBE_REQ       = 22,
	CMD_TEST            = 23,
	CMD_RADIO_CALIBRATE     = 25,   
	CMD_ENABLE_RX_PATH      = 27,   
	CMD_NOISE_HIST      = 28,
	CMD_RX_RESET        = 29,
	CMD_PS_POLL         = 30,
	CMD_QOS_NULL_DATA   = 31,
	CMD_LNA_CONTROL     = 32,
	CMD_SET_BCN_MODE    = 33,
	CMD_MEASUREMENT      = 34,
	CMD_STOP_MEASUREMENT = 35,
	CMD_DISCONNECT       = 36,
	CMD_SET_PS_MODE      = 37,
	CMD_CHANNEL_SWITCH   = 38,
	CMD_STOP_CHANNEL_SWICTH = 39,
	CMD_AP_DISCOVERY     = 40,
	CMD_STOP_AP_DISCOVERY = 41,
	CMD_SPS_SCAN = 42,
	CMD_STOP_SPS_SCAN = 43,
	CMD_HEALTH_CHECK     = 45,
	CMD_DEBUG            = 46,
	CMD_TRIGGER_SCAN_TO  = 47,

	NUM_COMMANDS,
	MAX_COMMAND_ID = 0xFFFF,
};

#define MAX_CMD_PARAMS 572

struct wl1251_cmd_header {
	u16 id;
	u16 status;
	
	u8 data[0];
} __packed;

struct  wl1251_command {
	struct wl1251_cmd_header header;
	u8  parameters[MAX_CMD_PARAMS];
} __packed;

enum {
	CMD_MAILBOX_IDLE              		=  0,
	CMD_STATUS_SUCCESS            		=  1,
	CMD_STATUS_UNKNOWN_CMD        		=  2,
	CMD_STATUS_UNKNOWN_IE         		=  3,
	CMD_STATUS_REJECT_MEAS_SG_ACTIVE 	= 11,
	CMD_STATUS_RX_BUSY            		= 13,
	CMD_STATUS_INVALID_PARAM      		= 14,
	CMD_STATUS_TEMPLATE_TOO_LARGE 		= 15,
	CMD_STATUS_OUT_OF_MEMORY      		= 16,
	CMD_STATUS_STA_TABLE_FULL     		= 17,
	CMD_STATUS_RADIO_ERROR        		= 18,
	CMD_STATUS_WRONG_NESTING      		= 19,
	CMD_STATUS_TIMEOUT            		= 21, 
	CMD_STATUS_FW_RESET           		= 22, 
	MAX_COMMAND_STATUS            		= 0xff
};


#define MAX_READ_SIZE 256

struct cmd_read_write_memory {
	struct wl1251_cmd_header header;

	
	u32 addr;

	u32 size;

	/* The actual value read from or written to the Wilink. The source
	   of this field is the Host in WRITE command or the Wilink in READ
	   command. */
	u8 value[MAX_READ_SIZE];
} __packed;

#define CMDMBOX_HEADER_LEN 4
#define CMDMBOX_INFO_ELEM_HEADER_LEN 4

#define WL1251_SCAN_MIN_DURATION 30000
#define WL1251_SCAN_MAX_DURATION 60000

#define WL1251_SCAN_NUM_PROBES 3

struct wl1251_scan_parameters {
	__le32 rx_config_options;
	__le32 rx_filter_options;

	__le16 scan_options;

	
	u8 num_channels;

	
	u8 num_probe_requests;

	
	__le16 tx_rate;

	u8 tid_trigger;
	u8 ssid_len;
	u8 ssid[32];

} __packed;

struct wl1251_scan_ch_parameters {
	__le32 min_duration; 
	__le32 max_duration; 
	u32 bssid_lsb;
	u16 bssid_msb;

	u8 early_termination;

	u8 tx_power_att;
	u8 channel;
	u8 pad[3];
} __packed;

#define SCAN_MAX_NUM_OF_CHANNELS 16

struct wl1251_cmd_scan {
	struct wl1251_cmd_header header;

	struct wl1251_scan_parameters params;
	struct wl1251_scan_ch_parameters channels[SCAN_MAX_NUM_OF_CHANNELS];
} __packed;

enum {
	BSS_TYPE_IBSS = 0,
	BSS_TYPE_STA_BSS = 2,
	BSS_TYPE_AP_BSS = 3,
	MAX_BSS_TYPE = 0xFF
};

#define JOIN_CMD_CTRL_TX_FLUSH             0x80 
#define JOIN_CMD_CTRL_EARLY_WAKEUP_ENABLE  0x01 


struct cmd_join {
	struct wl1251_cmd_header header;

	u32 bssid_lsb;
	u16 bssid_msb;
	u16 beacon_interval; 
	u32 rx_config_options;
	u32 rx_filter_options;

	u16 basic_rate_set;
	u8 dtim_interval;
	u8 tx_ctrl_frame_rate; 
	u8 tx_ctrl_frame_mod;  
	u8 bss_type;
	u8 channel;
	u8 ssid_len;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ctrl; 
	u8 tx_mgt_frame_rate; 
	u8 tx_mgt_frame_mod;  
	u8 reserved;
} __packed;

struct cmd_enabledisable_path {
	struct wl1251_cmd_header header;

	u8 channel;
	u8 padding[3];
} __packed;

#define WL1251_MAX_TEMPLATE_SIZE 300

struct wl1251_cmd_packet_template {
	struct wl1251_cmd_header header;

	__le16 size;
	u8 data[0];
} __packed;

#define TIM_ELE_ID    5
#define PARTIAL_VBM_MAX    251

struct wl1251_tim {
	u8 identity;
	u8 length;
	u8 dtim_count;
	u8 dtim_period;
	u8 bitmap_ctrl;
	u8 pvb_field[PARTIAL_VBM_MAX]; 
} __packed;

struct wl1251_cmd_vbm_update {
	struct wl1251_cmd_header header;
	__le16 len;
	u8  padding[2];
	struct wl1251_tim tim;
} __packed;

enum wl1251_cmd_ps_mode {
	CHIP_ACTIVE_MODE,
	CHIP_POWER_SAVE_MODE
};

struct wl1251_cmd_ps_params {
	struct wl1251_cmd_header header;

	u8 ps_mode; 
	u8 send_null_data; 
	u8 retries; 

	u8 hang_over_period;
	u16 null_data_rate;
	u8 pad[2];
} __packed;

struct wl1251_cmd_trigger_scan_to {
	struct wl1251_cmd_header header;

	u32 timeout;
} __packed;

#define NUM_ACCESS_CATEGORIES_COPY 4
#define MAX_KEY_SIZE 32

#define DF_ENCRYPTION_DISABLE      0x01
#define DF_SNIFF_MODE_ENABLE       0x80

enum wl1251_cmd_key_action {
	KEY_ADD_OR_REPLACE = 1,
	KEY_REMOVE         = 2,
	KEY_SET_ID         = 3,
	MAX_KEY_ACTION     = 0xffff,
};

enum wl1251_cmd_key_type {
	KEY_WEP_DEFAULT       = 0,
	KEY_WEP_ADDR          = 1,
	KEY_AES_GROUP         = 4,
	KEY_AES_PAIRWISE      = 5,
	KEY_WEP_GROUP         = 6,
	KEY_TKIP_MIC_GROUP    = 10,
	KEY_TKIP_MIC_PAIRWISE = 11,
};


struct wl1251_cmd_set_keys {
	struct wl1251_cmd_header header;

	
	u8 addr[ETH_ALEN];

	
	u16 key_action;

	u16 reserved_1;

	
	u8 key_size;

	
	u8 key_type;
	u8 ssid_profile;

	u8 id;
	u8 reserved_2[6];
	u8 key[MAX_KEY_SIZE];
	u16 ac_seq_num16[NUM_ACCESS_CATEGORIES_COPY];
	u32 ac_seq_num32[NUM_ACCESS_CATEGORIES_COPY];
} __packed;


#endif 
