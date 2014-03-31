/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2005 - 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2012 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#ifndef __iwl_commands_h__
#define __iwl_commands_h__

#include <linux/ieee80211.h>
#include <linux/types.h>


enum {
	REPLY_ALIVE = 0x1,
	REPLY_ERROR = 0x2,
	REPLY_ECHO = 0x3,		

	
	REPLY_RXON = 0x10,
	REPLY_RXON_ASSOC = 0x11,
	REPLY_QOS_PARAM = 0x13,
	REPLY_RXON_TIMING = 0x14,

	
	REPLY_ADD_STA = 0x18,
	REPLY_REMOVE_STA = 0x19,
	REPLY_REMOVE_ALL_STA = 0x1a,	
	REPLY_TXFIFO_FLUSH = 0x1e,

	
	REPLY_WEPKEY = 0x20,

	
	REPLY_TX = 0x1c,
	REPLY_LEDS_CMD = 0x48,
	REPLY_TX_LINK_QUALITY_CMD = 0x4e,

	
	COEX_PRIORITY_TABLE_CMD = 0x5a,
	COEX_MEDIUM_NOTIFICATION = 0x5b,
	COEX_EVENT_CMD = 0x5c,

	
	TEMPERATURE_NOTIFICATION = 0x62,
	CALIBRATION_CFG_CMD = 0x65,
	CALIBRATION_RES_NOTIFICATION = 0x66,
	CALIBRATION_COMPLETE_NOTIFICATION = 0x67,

	
	REPLY_QUIET_CMD = 0x71,		
	REPLY_CHANNEL_SWITCH = 0x72,
	CHANNEL_SWITCH_NOTIFICATION = 0x73,
	REPLY_SPECTRUM_MEASUREMENT_CMD = 0x74,
	SPECTRUM_MEASURE_NOTIFICATION = 0x75,

	
	POWER_TABLE_CMD = 0x77,
	PM_SLEEP_NOTIFICATION = 0x7A,
	PM_DEBUG_STATISTIC_NOTIFIC = 0x7B,

	
	REPLY_SCAN_CMD = 0x80,
	REPLY_SCAN_ABORT_CMD = 0x81,
	SCAN_START_NOTIFICATION = 0x82,
	SCAN_RESULTS_NOTIFICATION = 0x83,
	SCAN_COMPLETE_NOTIFICATION = 0x84,

	
	BEACON_NOTIFICATION = 0x90,
	REPLY_TX_BEACON = 0x91,
	WHO_IS_AWAKE_NOTIFICATION = 0x94,	

	
	REPLY_TX_POWER_DBM_CMD = 0x95,
	QUIET_NOTIFICATION = 0x96,		
	REPLY_TX_PWR_TABLE_CMD = 0x97,
	REPLY_TX_POWER_DBM_CMD_V1 = 0x98,	
	TX_ANT_CONFIGURATION_CMD = 0x98,
	MEASURE_ABORT_NOTIFICATION = 0x99,	

	
	REPLY_BT_CONFIG = 0x9b,

	
	REPLY_STATISTICS_CMD = 0x9c,
	STATISTICS_NOTIFICATION = 0x9d,

	
	REPLY_CARD_STATE_CMD = 0xa0,
	CARD_STATE_NOTIFICATION = 0xa1,

	
	MISSED_BEACONS_NOTIFICATION = 0xa2,

	REPLY_CT_KILL_CONFIG_CMD = 0xa4,
	SENSITIVITY_CMD = 0xa8,
	REPLY_PHY_CALIBRATION_CMD = 0xb0,
	REPLY_RX_PHY_CMD = 0xc0,
	REPLY_RX_MPDU_CMD = 0xc1,
	REPLY_RX = 0xc3,
	REPLY_COMPRESSED_BA = 0xc5,

	
	REPLY_BT_COEX_PRIO_TABLE = 0xcc,
	REPLY_BT_COEX_PROT_ENV = 0xcd,
	REPLY_BT_COEX_PROFILE_NOTIF = 0xce,

	
	REPLY_WIPAN_PARAMS = 0xb2,
	REPLY_WIPAN_RXON = 0xb3,	
	REPLY_WIPAN_RXON_TIMING = 0xb4,	
	REPLY_WIPAN_RXON_ASSOC = 0xb6,	
	REPLY_WIPAN_QOS_PARAM = 0xb7,	
	REPLY_WIPAN_WEPKEY = 0xb8,	
	REPLY_WIPAN_P2P_CHANNEL_SWITCH = 0xb9,
	REPLY_WIPAN_NOA_NOTIFICATION = 0xbc,
	REPLY_WIPAN_DEACTIVATION_COMPLETE = 0xbd,

	REPLY_WOWLAN_PATTERNS = 0xe0,
	REPLY_WOWLAN_WAKEUP_FILTER = 0xe1,
	REPLY_WOWLAN_TSC_RSC_PARAMS = 0xe2,
	REPLY_WOWLAN_TKIP_PARAMS = 0xe3,
	REPLY_WOWLAN_KEK_KCK_MATERIAL = 0xe4,
	REPLY_WOWLAN_GET_STATUS = 0xe5,
	REPLY_D3_CONFIG = 0xd3,

	REPLY_MAX = 0xff
};


#define IWL_CMD_FAILED_MSK 0x40

#define RATE_MCS_CODE_MSK 0x7
#define RATE_MCS_SPATIAL_POS 3
#define RATE_MCS_SPATIAL_MSK 0x18
#define RATE_MCS_HT_DUP_POS 5
#define RATE_MCS_HT_DUP_MSK 0x20
#define RATE_MCS_RATE_MSK 0xff

#define RATE_MCS_FLAGS_POS 8
#define RATE_MCS_HT_POS 8
#define RATE_MCS_HT_MSK 0x100

#define RATE_MCS_CCK_POS 9
#define RATE_MCS_CCK_MSK 0x200

#define RATE_MCS_GF_POS 10
#define RATE_MCS_GF_MSK 0x400

#define RATE_MCS_HT40_POS 11
#define RATE_MCS_HT40_MSK 0x800

#define RATE_MCS_DUP_POS 12
#define RATE_MCS_DUP_MSK 0x1000

#define RATE_MCS_SGI_POS 13
#define RATE_MCS_SGI_MSK 0x2000

#define RATE_MCS_ANT_POS	14
#define RATE_MCS_ANT_A_MSK	0x04000
#define RATE_MCS_ANT_B_MSK	0x08000
#define RATE_MCS_ANT_C_MSK	0x10000
#define RATE_MCS_ANT_AB_MSK	(RATE_MCS_ANT_A_MSK | RATE_MCS_ANT_B_MSK)
#define RATE_MCS_ANT_ABC_MSK	(RATE_MCS_ANT_AB_MSK | RATE_MCS_ANT_C_MSK)
#define RATE_ANT_NUM 3

#define POWER_TABLE_NUM_ENTRIES			33
#define POWER_TABLE_NUM_HT_OFDM_ENTRIES		32
#define POWER_TABLE_CCK_ENTRY			32

#define IWL_PWR_NUM_HT_OFDM_ENTRIES		24
#define IWL_PWR_CCK_ENTRIES			2

struct tx_power_dual_stream {
	__le32 dw;
} __packed;

#define IWLAGN_TX_POWER_AUTO 0x7f
#define IWLAGN_TX_POWER_NO_CLOSED (0x1 << 6)

struct iwlagn_tx_power_dbm_cmd {
	s8 global_lmt; 
	u8 flags;
	s8 srv_chan_lmt; 
	u8 reserved;
} __packed;

struct iwl_tx_ant_config_cmd {
	__le32 valid;
} __packed;


#define UCODE_VALID_OK	cpu_to_le32(0x1)


struct iwl_error_event_table {
	u32 valid;		
	u32 error_id;		
	u32 pc;			
	u32 blink1;		
	u32 blink2;		
	u32 ilink1;		
	u32 ilink2;		
	u32 data1;		
	u32 data2;		
	u32 line;		
	u32 bcon_time;		
	u32 tsf_low;		
	u32 tsf_hi;		
	u32 gp1;		
	u32 gp2;		
	u32 gp3;		
	u32 ucode_ver;		
	u32 hw_ver;		
	u32 brd_ver;		
	u32 log_pc;		
	u32 frame_ptr;		
	u32 stack_ptr;		
	u32 hcmd;		
	u32 isr0;		
	u32 isr1;		
	u32 isr2;		
	u32 isr3;		
	u32 isr4;		
	u32 isr_pref;		
	u32 wait_event;		
	u32 l2p_control;	
	u32 l2p_duration;	
	u32 l2p_mhvalid;	
	u32 l2p_addr_match;	
	u32 lmpm_pmg_sel;	
	u32 u_timestamp;	
	u32 flow_handler;	
} __packed;

struct iwl_alive_resp {
	u8 ucode_minor;
	u8 ucode_major;
	__le16 reserved1;
	u8 sw_rev[8];
	u8 ver_type;
	u8 ver_subtype;			
	__le16 reserved2;
	__le32 log_event_table_ptr;	
	__le32 error_event_table_ptr;	
	__le32 timestamp;
	__le32 is_valid;
} __packed;

struct iwl_error_resp {
	__le32 error_type;
	u8 cmd_id;
	u8 reserved1;
	__le16 bad_cmd_seq_num;
	__le32 error_info;
	__le64 timestamp;
} __packed;


enum {
	RXON_DEV_TYPE_AP = 1,
	RXON_DEV_TYPE_ESS = 3,
	RXON_DEV_TYPE_IBSS = 4,
	RXON_DEV_TYPE_SNIFFER = 6,
	RXON_DEV_TYPE_CP = 7,
	RXON_DEV_TYPE_2STA = 8,
	RXON_DEV_TYPE_P2P = 9,
};


#define RXON_RX_CHAIN_DRIVER_FORCE_MSK		cpu_to_le16(0x1 << 0)
#define RXON_RX_CHAIN_DRIVER_FORCE_POS		(0)
#define RXON_RX_CHAIN_VALID_MSK			cpu_to_le16(0x7 << 1)
#define RXON_RX_CHAIN_VALID_POS			(1)
#define RXON_RX_CHAIN_FORCE_SEL_MSK		cpu_to_le16(0x7 << 4)
#define RXON_RX_CHAIN_FORCE_SEL_POS		(4)
#define RXON_RX_CHAIN_FORCE_MIMO_SEL_MSK	cpu_to_le16(0x7 << 7)
#define RXON_RX_CHAIN_FORCE_MIMO_SEL_POS	(7)
#define RXON_RX_CHAIN_CNT_MSK			cpu_to_le16(0x3 << 10)
#define RXON_RX_CHAIN_CNT_POS			(10)
#define RXON_RX_CHAIN_MIMO_CNT_MSK		cpu_to_le16(0x3 << 12)
#define RXON_RX_CHAIN_MIMO_CNT_POS		(12)
#define RXON_RX_CHAIN_MIMO_FORCE_MSK		cpu_to_le16(0x1 << 14)
#define RXON_RX_CHAIN_MIMO_FORCE_POS		(14)

#define RXON_FLG_BAND_24G_MSK           cpu_to_le32(1 << 0)
#define RXON_FLG_CCK_MSK                cpu_to_le32(1 << 1)
#define RXON_FLG_AUTO_DETECT_MSK        cpu_to_le32(1 << 2)
#define RXON_FLG_TGG_PROTECT_MSK        cpu_to_le32(1 << 3)
#define RXON_FLG_SHORT_SLOT_MSK          cpu_to_le32(1 << 4)
#define RXON_FLG_SHORT_PREAMBLE_MSK     cpu_to_le32(1 << 5)
#define RXON_FLG_DIS_DIV_MSK            cpu_to_le32(1 << 7)
#define RXON_FLG_ANT_SEL_MSK            cpu_to_le32(0x0f00)
#define RXON_FLG_ANT_A_MSK              cpu_to_le32(1 << 8)
#define RXON_FLG_ANT_B_MSK              cpu_to_le32(1 << 9)
#define RXON_FLG_RADAR_DETECT_MSK       cpu_to_le32(1 << 12)
#define RXON_FLG_TGJ_NARROW_BAND_MSK    cpu_to_le32(1 << 13)
#define RXON_FLG_TSF2HOST_MSK           cpu_to_le32(1 << 15)


#define RXON_FLG_CTRL_CHANNEL_LOC_POS		(22)
#define RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK	cpu_to_le32(0x1 << 22)

#define RXON_FLG_HT_OPERATING_MODE_POS		(23)

#define RXON_FLG_HT_PROT_MSK			cpu_to_le32(0x1 << 23)
#define RXON_FLG_HT40_PROT_MSK			cpu_to_le32(0x2 << 23)

#define RXON_FLG_CHANNEL_MODE_POS		(25)
#define RXON_FLG_CHANNEL_MODE_MSK		cpu_to_le32(0x3 << 25)

enum {
	CHANNEL_MODE_LEGACY = 0,
	CHANNEL_MODE_PURE_40 = 1,
	CHANNEL_MODE_MIXED = 2,
	CHANNEL_MODE_RESERVED = 3,
};
#define RXON_FLG_CHANNEL_MODE_LEGACY	cpu_to_le32(CHANNEL_MODE_LEGACY << RXON_FLG_CHANNEL_MODE_POS)
#define RXON_FLG_CHANNEL_MODE_PURE_40	cpu_to_le32(CHANNEL_MODE_PURE_40 << RXON_FLG_CHANNEL_MODE_POS)
#define RXON_FLG_CHANNEL_MODE_MIXED	cpu_to_le32(CHANNEL_MODE_MIXED << RXON_FLG_CHANNEL_MODE_POS)

#define RXON_FLG_SELF_CTS_EN			cpu_to_le32(0x1<<30)

#define RXON_FILTER_PROMISC_MSK         cpu_to_le32(1 << 0)
#define RXON_FILTER_CTL2HOST_MSK        cpu_to_le32(1 << 1)
#define RXON_FILTER_ACCEPT_GRP_MSK      cpu_to_le32(1 << 2)
#define RXON_FILTER_DIS_DECRYPT_MSK     cpu_to_le32(1 << 3)
#define RXON_FILTER_DIS_GRP_DECRYPT_MSK cpu_to_le32(1 << 4)
#define RXON_FILTER_ASSOC_MSK           cpu_to_le32(1 << 5)
#define RXON_FILTER_BCON_AWARE_MSK      cpu_to_le32(1 << 6)


struct iwl_rxon_cmd {
	u8 node_addr[6];
	__le16 reserved1;
	u8 bssid_addr[6];
	__le16 reserved2;
	u8 wlap_bssid_addr[6];
	__le16 reserved3;
	u8 dev_type;
	u8 air_propagation;
	__le16 rx_chain;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	__le16 assoc_id;
	__le32 flags;
	__le32 filter_flags;
	__le16 channel;
	u8 ofdm_ht_single_stream_basic_rates;
	u8 ofdm_ht_dual_stream_basic_rates;
	u8 ofdm_ht_triple_stream_basic_rates;
	u8 reserved5;
	__le16 acquisition_data;
	__le16 reserved6;
} __packed;

struct iwl_rxon_assoc_cmd {
	__le32 flags;
	__le32 filter_flags;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	__le16 reserved1;
	u8 ofdm_ht_single_stream_basic_rates;
	u8 ofdm_ht_dual_stream_basic_rates;
	u8 ofdm_ht_triple_stream_basic_rates;
	u8 reserved2;
	__le16 rx_chain_select_flags;
	__le16 acquisition_data;
	__le32 reserved3;
} __packed;

#define IWL_CONN_MAX_LISTEN_INTERVAL	10
#define IWL_MAX_UCODE_BEACON_INTERVAL	4 

struct iwl_rxon_time_cmd {
	__le64 timestamp;
	__le16 beacon_interval;
	__le16 atim_window;
	__le32 beacon_init_val;
	__le16 listen_interval;
	u8 dtim_period;
	u8 delta_cp_bss_tbtts;
} __packed;

struct iwl5000_channel_switch_cmd {
	u8 band;
	u8 expect_beacon;
	__le16 channel;
	__le32 rxon_flags;
	__le32 rxon_filter_flags;
	__le32 switch_time;
	__le32 reserved[2][IWL_PWR_NUM_HT_OFDM_ENTRIES + IWL_PWR_CCK_ENTRIES];
} __packed;

struct iwl6000_channel_switch_cmd {
	u8 band;
	u8 expect_beacon;
	__le16 channel;
	__le32 rxon_flags;
	__le32 rxon_filter_flags;
	__le32 switch_time;
	__le32 reserved[3][IWL_PWR_NUM_HT_OFDM_ENTRIES + IWL_PWR_CCK_ENTRIES];
} __packed;

struct iwl_csa_notification {
	__le16 band;
	__le16 channel;
	__le32 status;		
} __packed;


struct iwl_ac_qos {
	__le16 cw_min;
	__le16 cw_max;
	u8 aifsn;
	u8 reserved1;
	__le16 edca_txop;
} __packed;

#define QOS_PARAM_FLG_UPDATE_EDCA_MSK	cpu_to_le32(0x01)
#define QOS_PARAM_FLG_TGN_MSK		cpu_to_le32(0x02)
#define QOS_PARAM_FLG_TXOP_TYPE_MSK	cpu_to_le32(0x10)

#define AC_NUM                4

struct iwl_qosparam_cmd {
	__le32 qos_flags;
	struct iwl_ac_qos ac[AC_NUM];
} __packed;


#define	IWL_AP_ID		0
#define	IWL_AP_ID_PAN		1
#define	IWL_STA_ID		2
#define IWLAGN_PAN_BCAST_ID	14
#define IWLAGN_BROADCAST_ID	15
#define	IWLAGN_STATION_COUNT	16

#define	IWL_INVALID_STATION 	255
#define IWL_MAX_TID_COUNT	8
#define IWL_TID_NON_QOS IWL_MAX_TID_COUNT

#define STA_FLG_TX_RATE_MSK		cpu_to_le32(1 << 2)
#define STA_FLG_PWR_SAVE_MSK		cpu_to_le32(1 << 8)
#define STA_FLG_PAN_STATION		cpu_to_le32(1 << 13)
#define STA_FLG_RTS_MIMO_PROT_MSK	cpu_to_le32(1 << 17)
#define STA_FLG_AGG_MPDU_8US_MSK	cpu_to_le32(1 << 18)
#define STA_FLG_MAX_AGG_SIZE_POS	(19)
#define STA_FLG_MAX_AGG_SIZE_MSK	cpu_to_le32(3 << 19)
#define STA_FLG_HT40_EN_MSK		cpu_to_le32(1 << 21)
#define STA_FLG_MIMO_DIS_MSK		cpu_to_le32(1 << 22)
#define STA_FLG_AGG_MPDU_DENSITY_POS	(23)
#define STA_FLG_AGG_MPDU_DENSITY_MSK	cpu_to_le32(7 << 23)

#define STA_CONTROL_MODIFY_MSK		0x01

#define STA_KEY_FLG_ENCRYPT_MSK	cpu_to_le16(0x0007)
#define STA_KEY_FLG_NO_ENC	cpu_to_le16(0x0000)
#define STA_KEY_FLG_WEP		cpu_to_le16(0x0001)
#define STA_KEY_FLG_CCMP	cpu_to_le16(0x0002)
#define STA_KEY_FLG_TKIP	cpu_to_le16(0x0003)

#define STA_KEY_FLG_KEYID_POS	8
#define STA_KEY_FLG_INVALID 	cpu_to_le16(0x0800)
#define STA_KEY_FLG_MAP_KEY_MSK	cpu_to_le16(0x0008)

#define STA_KEY_FLG_KEY_SIZE_MSK     cpu_to_le16(0x1000)
#define STA_KEY_MULTICAST_MSK        cpu_to_le16(0x4000)
#define STA_KEY_MAX_NUM		8
#define STA_KEY_MAX_NUM_PAN	16
#define IWLAGN_HW_KEY_DEFAULT	0xfe

#define	STA_MODIFY_KEY_MASK		0x01
#define	STA_MODIFY_TID_DISABLE_TX	0x02
#define	STA_MODIFY_TX_RATE_MSK		0x04
#define STA_MODIFY_ADDBA_TID_MSK	0x08
#define STA_MODIFY_DELBA_TID_MSK	0x10
#define STA_MODIFY_SLEEP_TX_COUNT_MSK	0x20

#define BUILD_RAxTID(sta_id, tid)	(((sta_id) << 4) + (tid))

struct iwl_keyinfo {
	__le16 key_flags;
	u8 tkip_rx_tsc_byte2;	
	u8 reserved1;
	__le16 tkip_rx_ttak[5];	
	u8 key_offset;
	u8 reserved2;
	u8 key[16];		
	__le64 tx_secur_seq_cnt;
	__le64 hw_tkip_mic_rx_key;
	__le64 hw_tkip_mic_tx_key;
} __packed;

struct sta_id_modify {
	u8 addr[ETH_ALEN];
	__le16 reserved1;
	u8 sta_id;
	u8 modify_mask;
	__le16 reserved2;
} __packed;


struct iwl_addsta_cmd {
	u8 mode;		
	u8 reserved[3];
	struct sta_id_modify sta;
	struct iwl_keyinfo key;
	__le32 station_flags;		
	__le32 station_flags_msk;	

	__le16 tid_disable_tx;
	__le16 legacy_reserved;

	u8 add_immediate_ba_tid;

	u8 remove_immediate_ba_tid;

	__le16 add_immediate_ba_ssn;

	__le16 sleep_tx_count;

	__le16 reserved2;
} __packed;


#define ADD_STA_SUCCESS_MSK		0x1
#define ADD_STA_NO_ROOM_IN_TABLE	0x2
#define ADD_STA_NO_BLOCK_ACK_RESOURCE	0x4
#define ADD_STA_MODIFY_NON_EXIST_STA	0x8
struct iwl_add_sta_resp {
	u8 status;	
} __packed;

#define REM_STA_SUCCESS_MSK              0x1
struct iwl_rem_sta_resp {
	u8 status;
} __packed;

struct iwl_rem_sta_cmd {
	u8 num_sta;     
	u8 reserved[3];
	u8 addr[ETH_ALEN]; 
	u8 reserved2[2];
} __packed;


#define IWL_SCD_BK_MSK			cpu_to_le32(BIT(0))
#define IWL_SCD_BE_MSK			cpu_to_le32(BIT(1))
#define IWL_SCD_VI_MSK			cpu_to_le32(BIT(2))
#define IWL_SCD_VO_MSK			cpu_to_le32(BIT(3))
#define IWL_SCD_MGMT_MSK		cpu_to_le32(BIT(3))

#define IWL_PAN_SCD_BK_MSK		cpu_to_le32(BIT(4))
#define IWL_PAN_SCD_BE_MSK		cpu_to_le32(BIT(5))
#define IWL_PAN_SCD_VI_MSK		cpu_to_le32(BIT(6))
#define IWL_PAN_SCD_VO_MSK		cpu_to_le32(BIT(7))
#define IWL_PAN_SCD_MGMT_MSK		cpu_to_le32(BIT(7))
#define IWL_PAN_SCD_MULTICAST_MSK	cpu_to_le32(BIT(8))

#define IWL_AGG_TX_QUEUE_MSK		cpu_to_le32(0xffc00)

#define IWL_DROP_SINGLE		0
#define IWL_DROP_ALL		(BIT(IWL_RXON_CTX_BSS) | BIT(IWL_RXON_CTX_PAN))

struct iwl_txfifo_flush_cmd {
	__le32 fifo_control;
	__le16 flush_control;
	__le16 reserved;
} __packed;

struct iwl_wep_key {
	u8 key_index;
	u8 key_offset;
	u8 reserved1[2];
	u8 key_size;
	u8 reserved2[3];
	u8 key[16];
} __packed;

struct iwl_wep_cmd {
	u8 num_keys;
	u8 global_key_type;
	u8 flags;
	u8 reserved;
	struct iwl_wep_key key[0];
} __packed;

#define WEP_KEY_WEP_TYPE 1
#define WEP_KEYS_MAX 4
#define WEP_INVALID_OFFSET 0xff
#define WEP_KEY_LEN_64 5
#define WEP_KEY_LEN_128 13


#define RX_RES_STATUS_NO_CRC32_ERROR	cpu_to_le32(1 << 0)
#define RX_RES_STATUS_NO_RXE_OVERFLOW	cpu_to_le32(1 << 1)

#define RX_RES_PHY_FLAGS_BAND_24_MSK	cpu_to_le16(1 << 0)
#define RX_RES_PHY_FLAGS_MOD_CCK_MSK		cpu_to_le16(1 << 1)
#define RX_RES_PHY_FLAGS_SHORT_PREAMBLE_MSK	cpu_to_le16(1 << 2)
#define RX_RES_PHY_FLAGS_NARROW_BAND_MSK	cpu_to_le16(1 << 3)
#define RX_RES_PHY_FLAGS_ANTENNA_MSK		0xf0
#define RX_RES_PHY_FLAGS_ANTENNA_POS		4

#define RX_RES_STATUS_SEC_TYPE_MSK	(0x7 << 8)
#define RX_RES_STATUS_SEC_TYPE_NONE	(0x0 << 8)
#define RX_RES_STATUS_SEC_TYPE_WEP	(0x1 << 8)
#define RX_RES_STATUS_SEC_TYPE_CCMP	(0x2 << 8)
#define RX_RES_STATUS_SEC_TYPE_TKIP	(0x3 << 8)
#define	RX_RES_STATUS_SEC_TYPE_ERR	(0x7 << 8)

#define RX_RES_STATUS_STATION_FOUND	(1<<6)
#define RX_RES_STATUS_NO_STATION_INFO_MISMATCH	(1<<7)

#define RX_RES_STATUS_DECRYPT_TYPE_MSK	(0x3 << 11)
#define RX_RES_STATUS_NOT_DECRYPT	(0x0 << 11)
#define RX_RES_STATUS_DECRYPT_OK	(0x3 << 11)
#define RX_RES_STATUS_BAD_ICV_MIC	(0x1 << 11)
#define RX_RES_STATUS_BAD_KEY_TTAK	(0x2 << 11)

#define RX_MPDU_RES_STATUS_ICV_OK	(0x20)
#define RX_MPDU_RES_STATUS_MIC_OK	(0x40)
#define RX_MPDU_RES_STATUS_TTAK_OK	(1 << 7)
#define RX_MPDU_RES_STATUS_DEC_DONE_MSK	(0x800)


#define IWLAGN_RX_RES_PHY_CNT 8
#define IWLAGN_RX_RES_AGC_IDX     1
#define IWLAGN_RX_RES_RSSI_AB_IDX 2
#define IWLAGN_RX_RES_RSSI_C_IDX  3
#define IWLAGN_OFDM_AGC_MSK 0xfe00
#define IWLAGN_OFDM_AGC_BIT_POS 9
#define IWLAGN_OFDM_RSSI_INBAND_A_BITMSK 0x00ff
#define IWLAGN_OFDM_RSSI_ALLBAND_A_BITMSK 0xff00
#define IWLAGN_OFDM_RSSI_A_BIT_POS 0
#define IWLAGN_OFDM_RSSI_INBAND_B_BITMSK 0xff0000
#define IWLAGN_OFDM_RSSI_ALLBAND_B_BITMSK 0xff000000
#define IWLAGN_OFDM_RSSI_B_BIT_POS 16
#define IWLAGN_OFDM_RSSI_INBAND_C_BITMSK 0x00ff
#define IWLAGN_OFDM_RSSI_ALLBAND_C_BITMSK 0xff00
#define IWLAGN_OFDM_RSSI_C_BIT_POS 0

struct iwlagn_non_cfg_phy {
	__le32 non_cfg_phy[IWLAGN_RX_RES_PHY_CNT];  
} __packed;


struct iwl_rx_phy_res {
	u8 non_cfg_phy_cnt;     
	u8 cfg_phy_cnt;		
	u8 stat_id;		
	u8 reserved1;
	__le64 timestamp;	
	__le32 beacon_time_stamp; 
	__le16 phy_flags;	
	__le16 channel;		
	u8 non_cfg_phy_buf[32]; 
	__le32 rate_n_flags;	
	__le16 byte_count;	
	__le16 frame_time;	
} __packed;

struct iwl_rx_mpdu_res_start {
	__le16 byte_count;
	__le16 reserved;
} __packed;




#define TX_CMD_FLG_PROT_REQUIRE_MSK cpu_to_le32(1 << 0)

#define TX_CMD_FLG_ACK_MSK cpu_to_le32(1 << 3)

#define TX_CMD_FLG_STA_RATE_MSK cpu_to_le32(1 << 4)

#define TX_CMD_FLG_IMM_BA_RSP_MASK  cpu_to_le32(1 << 6)

#define TX_CMD_FLG_ANT_SEL_MSK cpu_to_le32(0xf00)

#define TX_CMD_FLG_IGNORE_BT cpu_to_le32(1 << 12)

#define TX_CMD_FLG_SEQ_CTL_MSK cpu_to_le32(1 << 13)

#define TX_CMD_FLG_MORE_FRAG_MSK cpu_to_le32(1 << 14)

#define TX_CMD_FLG_TSF_MSK cpu_to_le32(1 << 16)

#define TX_CMD_FLG_MH_PAD_MSK cpu_to_le32(1 << 20)

#define TX_CMD_FLG_AGG_CCMP_MSK cpu_to_le32(1 << 22)

#define TX_CMD_FLG_DUR_MSK cpu_to_le32(1 << 25)


#define TX_CMD_SEC_WEP  	0x01
#define TX_CMD_SEC_CCM  	0x02
#define TX_CMD_SEC_TKIP		0x03
#define TX_CMD_SEC_MSK		0x03
#define TX_CMD_SEC_SHIFT	6
#define TX_CMD_SEC_KEY128	0x08

#define WEP_IV_LEN 4
#define WEP_ICV_LEN 4
#define CCMP_MIC_LEN 8
#define TKIP_ICV_LEN 4


struct iwl_dram_scratch {
	u8 try_cnt;		
	u8 bt_kill_cnt;		
	__le16 reserved;
} __packed;

struct iwl_tx_cmd {
	__le16 len;

	__le16 next_frame_len;

	__le32 tx_flags;	

	struct iwl_dram_scratch scratch;

	
	__le32 rate_n_flags;	

	
	u8 sta_id;

	
	u8 sec_ctl;		

	u8 initial_rate_index;
	u8 reserved;
	u8 key[16];
	__le16 next_frame_flags;
	__le16 reserved2;
	union {
		__le32 life_time;
		__le32 attempt;
	} stop_time;

	__le32 dram_lsb_ptr;
	u8 dram_msb_ptr;

	u8 rts_retry_limit;	
	u8 data_retry_limit;	
	u8 tid_tspec;
	union {
		__le16 pm_frame_timeout;
		__le16 attempt_duration;
	} timeout;

	__le16 driver_txop;

	u8 payload[0];
	struct ieee80211_hdr hdr[0];
} __packed;

enum {
	TX_STATUS_SUCCESS = 0x01,
	TX_STATUS_DIRECT_DONE = 0x02,
	
	TX_STATUS_POSTPONE_DELAY = 0x40,
	TX_STATUS_POSTPONE_FEW_BYTES = 0x41,
	TX_STATUS_POSTPONE_BT_PRIO = 0x42,
	TX_STATUS_POSTPONE_QUIET_PERIOD = 0x43,
	TX_STATUS_POSTPONE_CALC_TTAK = 0x44,
	
	TX_STATUS_FAIL_INTERNAL_CROSSED_RETRY = 0x81,
	TX_STATUS_FAIL_SHORT_LIMIT = 0x82,
	TX_STATUS_FAIL_LONG_LIMIT = 0x83,
	TX_STATUS_FAIL_FIFO_UNDERRUN = 0x84,
	TX_STATUS_FAIL_DRAIN_FLOW = 0x85,
	TX_STATUS_FAIL_RFKILL_FLUSH = 0x86,
	TX_STATUS_FAIL_LIFE_EXPIRE = 0x87,
	TX_STATUS_FAIL_DEST_PS = 0x88,
	TX_STATUS_FAIL_HOST_ABORTED = 0x89,
	TX_STATUS_FAIL_BT_RETRY = 0x8a,
	TX_STATUS_FAIL_STA_INVALID = 0x8b,
	TX_STATUS_FAIL_FRAG_DROPPED = 0x8c,
	TX_STATUS_FAIL_TID_DISABLE = 0x8d,
	TX_STATUS_FAIL_FIFO_FLUSHED = 0x8e,
	TX_STATUS_FAIL_INSUFFICIENT_CF_POLL = 0x8f,
	TX_STATUS_FAIL_PASSIVE_NO_RX = 0x90,
	TX_STATUS_FAIL_NO_BEACON_ON_RADAR = 0x91,
};

#define	TX_PACKET_MODE_REGULAR		0x0000
#define	TX_PACKET_MODE_BURST_SEQ	0x0100
#define	TX_PACKET_MODE_BURST_FIRST	0x0200

enum {
	TX_POWER_PA_NOT_ACTIVE = 0x0,
};

enum {
	TX_STATUS_MSK = 0x000000ff,		
	TX_STATUS_DELAY_MSK = 0x00000040,
	TX_STATUS_ABORT_MSK = 0x00000080,
	TX_PACKET_MODE_MSK = 0x0000ff00,	
	TX_FIFO_NUMBER_MSK = 0x00070000,	
	TX_RESERVED = 0x00780000,		
	TX_POWER_PA_DETECT_MSK = 0x7f800000,	
	TX_ABORT_REQUIRED_MSK = 0x80000000,	
};


enum {
	AGG_TX_STATE_TRANSMITTED = 0x00,
	AGG_TX_STATE_UNDERRUN_MSK = 0x01,
	AGG_TX_STATE_BT_PRIO_MSK = 0x02,
	AGG_TX_STATE_FEW_BYTES_MSK = 0x04,
	AGG_TX_STATE_ABORT_MSK = 0x08,
	AGG_TX_STATE_LAST_SENT_TTL_MSK = 0x10,
	AGG_TX_STATE_LAST_SENT_TRY_CNT_MSK = 0x20,
	AGG_TX_STATE_LAST_SENT_BT_KILL_MSK = 0x40,
	AGG_TX_STATE_SCD_QUERY_MSK = 0x80,
	AGG_TX_STATE_TEST_BAD_CRC32_MSK = 0x100,
	AGG_TX_STATE_RESPONSE_MSK = 0x1ff,
	AGG_TX_STATE_DUMP_TX_MSK = 0x200,
	AGG_TX_STATE_DELAY_TX_MSK = 0x400
};

#define AGG_TX_STATUS_MSK	0x00000fff	
#define AGG_TX_TRY_MSK		0x0000f000	

#define AGG_TX_STATE_LAST_SENT_MSK  (AGG_TX_STATE_LAST_SENT_TTL_MSK | \
				     AGG_TX_STATE_LAST_SENT_TRY_CNT_MSK | \
				     AGG_TX_STATE_LAST_SENT_BT_KILL_MSK)

#define AGG_TX_STATE_TRY_CNT_POS 12
#define AGG_TX_STATE_TRY_CNT_MSK 0xf000

#define AGG_TX_STATE_SEQ_NUM_POS 16
#define AGG_TX_STATE_SEQ_NUM_MSK 0xffff0000

struct agg_tx_status {
	__le16 status;
	__le16 sequence;
} __packed;


#define IWL50_TX_RES_INIT_RATE_INDEX_POS	0
#define IWL50_TX_RES_INIT_RATE_INDEX_MSK	0x0f
#define IWL50_TX_RES_RATE_TABLE_COLOR_POS	4
#define IWL50_TX_RES_RATE_TABLE_COLOR_MSK	0x70
#define IWL50_TX_RES_INV_RATE_INDEX_MSK	0x80

#define IWLAGN_TX_RES_TID_POS	0
#define IWLAGN_TX_RES_TID_MSK	0x0f
#define IWLAGN_TX_RES_RA_POS	4
#define IWLAGN_TX_RES_RA_MSK	0xf0

struct iwlagn_tx_resp {
	u8 frame_count;		
	u8 bt_kill_count;	
	u8 failure_rts;		
	u8 failure_frame;	

	__le32 rate_n_flags;	

	__le16 wireless_media_time;	

	u8 pa_status;		
	u8 pa_integ_res_a[3];
	u8 pa_integ_res_b[3];
	u8 pa_integ_res_C[3];

	__le32 tfd_info;
	__le16 seq_ctl;
	__le16 byte_cnt;
	u8 tlc_info;
	u8 ra_tid;		
	__le16 frame_ctrl;
	struct agg_tx_status status;	
} __packed;
struct iwl_compressed_ba_resp {
	__le32 sta_addr_lo32;
	__le16 sta_addr_hi16;
	__le16 reserved;

	
	u8 sta_id;
	u8 tid;
	__le16 seq_ctl;
	__le64 bitmap;
	__le16 scd_flow;
	__le16 scd_ssn;
	u8 txed;	
	u8 txed_2_done; 
} __packed;


#define  LINK_QUAL_FLAGS_SET_STA_TLC_RTS_MSK	(1 << 0)

#define  LINK_QUAL_AC_NUM AC_NUM

#define  LINK_QUAL_MAX_RETRY_NUM 16

#define  LINK_QUAL_ANT_A_MSK (1 << 0)
#define  LINK_QUAL_ANT_B_MSK (1 << 1)
#define  LINK_QUAL_ANT_MSK   (LINK_QUAL_ANT_A_MSK|LINK_QUAL_ANT_B_MSK)


struct iwl_link_qual_general_params {
	u8 flags;

	
	u8 mimo_delimiter;

	
	u8 single_stream_ant_msk;	

	
	u8 dual_stream_ant_msk;		

	u8 start_rate_index[LINK_QUAL_AC_NUM];
} __packed;

#define LINK_QUAL_AGG_TIME_LIMIT_DEF	(4000) 
#define LINK_QUAL_AGG_TIME_LIMIT_MAX	(8000)
#define LINK_QUAL_AGG_TIME_LIMIT_MIN	(100)

#define LINK_QUAL_AGG_DISABLE_START_DEF	(3)
#define LINK_QUAL_AGG_DISABLE_START_MAX	(255)
#define LINK_QUAL_AGG_DISABLE_START_MIN	(0)

#define LINK_QUAL_AGG_FRAME_LIMIT_DEF	(63)
#define LINK_QUAL_AGG_FRAME_LIMIT_MAX	(63)
#define LINK_QUAL_AGG_FRAME_LIMIT_MIN	(0)

struct iwl_link_qual_agg_params {

	__le16 agg_time_limit;

	u8 agg_dis_start_th;

	u8 agg_frame_cnt_limit;

	__le32 reserved;
} __packed;

struct iwl_link_quality_cmd {

	
	u8 sta_id;
	u8 reserved1;
	__le16 control;		
	struct iwl_link_qual_general_params general_params;
	struct iwl_link_qual_agg_params agg_params;

	struct {
		__le32 rate_n_flags;	
	} rs_table[LINK_QUAL_MAX_RETRY_NUM];
	__le32 reserved2;
} __packed;

#define BT_COEX_DISABLE (0x0)
#define BT_ENABLE_CHANNEL_ANNOUNCE BIT(0)
#define BT_ENABLE_PRIORITY	   BIT(1)
#define BT_ENABLE_2_WIRE	   BIT(2)

#define BT_COEX_DISABLE (0x0)
#define BT_COEX_ENABLE  (BT_ENABLE_CHANNEL_ANNOUNCE | BT_ENABLE_PRIORITY)

#define BT_LEAD_TIME_MIN (0x0)
#define BT_LEAD_TIME_DEF (0x1E)
#define BT_LEAD_TIME_MAX (0xFF)

#define BT_MAX_KILL_MIN (0x1)
#define BT_MAX_KILL_DEF (0x5)
#define BT_MAX_KILL_MAX (0xFF)

#define BT_DURATION_LIMIT_DEF	625
#define BT_DURATION_LIMIT_MAX	1250
#define BT_DURATION_LIMIT_MIN	625

#define BT_ON_THRESHOLD_DEF	4
#define BT_ON_THRESHOLD_MAX	1000
#define BT_ON_THRESHOLD_MIN	1

#define BT_FRAG_THRESHOLD_DEF	0
#define BT_FRAG_THRESHOLD_MAX	0
#define BT_FRAG_THRESHOLD_MIN	0

#define BT_AGG_THRESHOLD_DEF	1200
#define BT_AGG_THRESHOLD_MAX	8000
#define BT_AGG_THRESHOLD_MIN	400

struct iwl_bt_cmd {
	u8 flags;
	u8 lead_time;
	u8 max_kill;
	u8 reserved;
	__le32 kill_ack_mask;
	__le32 kill_cts_mask;
} __packed;

#define IWLAGN_BT_FLAG_CHANNEL_INHIBITION	BIT(0)

#define IWLAGN_BT_FLAG_COEX_MODE_MASK		(BIT(3)|BIT(4)|BIT(5))
#define IWLAGN_BT_FLAG_COEX_MODE_SHIFT		3
#define IWLAGN_BT_FLAG_COEX_MODE_DISABLED	0
#define IWLAGN_BT_FLAG_COEX_MODE_LEGACY_2W	1
#define IWLAGN_BT_FLAG_COEX_MODE_3W		2
#define IWLAGN_BT_FLAG_COEX_MODE_4W		3

#define IWLAGN_BT_FLAG_UCODE_DEFAULT		BIT(6)
#define IWLAGN_BT_FLAG_SYNC_2_BT_DISABLE	BIT(7)

#define IWLAGN_BT_PSP_MIN_RSSI_THRESHOLD	-75 
#define IWLAGN_BT_PSP_MAX_RSSI_THRESHOLD	-65 

#define IWLAGN_BT_PRIO_BOOST_MAX	0xFF
#define IWLAGN_BT_PRIO_BOOST_MIN	0x00
#define IWLAGN_BT_PRIO_BOOST_DEFAULT	0xF0

#define IWLAGN_BT_MAX_KILL_DEFAULT	5

#define IWLAGN_BT3_T7_DEFAULT		1

#define IWLAGN_BT_KILL_ACK_MASK_DEFAULT	cpu_to_le32(0xffff0000)
#define IWLAGN_BT_KILL_CTS_MASK_DEFAULT	cpu_to_le32(0xffff0000)
#define IWLAGN_BT_KILL_ACK_CTS_MASK_SCO	cpu_to_le32(0xffffffff)

#define IWLAGN_BT3_PRIO_SAMPLE_DEFAULT	2

#define IWLAGN_BT3_T2_DEFAULT		0xc

#define IWLAGN_BT_VALID_ENABLE_FLAGS	cpu_to_le16(BIT(0))
#define IWLAGN_BT_VALID_BOOST		cpu_to_le16(BIT(1))
#define IWLAGN_BT_VALID_MAX_KILL	cpu_to_le16(BIT(2))
#define IWLAGN_BT_VALID_3W_TIMERS	cpu_to_le16(BIT(3))
#define IWLAGN_BT_VALID_KILL_ACK_MASK	cpu_to_le16(BIT(4))
#define IWLAGN_BT_VALID_KILL_CTS_MASK	cpu_to_le16(BIT(5))
#define IWLAGN_BT_VALID_BT4_TIMES	cpu_to_le16(BIT(6))
#define IWLAGN_BT_VALID_3W_LUT		cpu_to_le16(BIT(7))

#define IWLAGN_BT_ALL_VALID_MSK		(IWLAGN_BT_VALID_ENABLE_FLAGS | \
					IWLAGN_BT_VALID_BOOST | \
					IWLAGN_BT_VALID_MAX_KILL | \
					IWLAGN_BT_VALID_3W_TIMERS | \
					IWLAGN_BT_VALID_KILL_ACK_MASK | \
					IWLAGN_BT_VALID_KILL_CTS_MASK | \
					IWLAGN_BT_VALID_BT4_TIMES | \
					IWLAGN_BT_VALID_3W_LUT)

struct iwl_basic_bt_cmd {
	u8 flags;
	u8 ledtime; 
	u8 max_kill;
	u8 bt3_timer_t7_value;
	__le32 kill_ack_mask;
	__le32 kill_cts_mask;
	u8 bt3_prio_sample_time;
	u8 bt3_timer_t2_value;
	__le16 bt4_reaction_time; 
	__le32 bt3_lookup_table[12];
	__le16 bt4_decision_time; 
	__le16 valid;
};

struct iwl6000_bt_cmd {
	struct iwl_basic_bt_cmd basic;
	u8 prio_boost;
	u8 tx_prio_boost;	
	__le16 rx_prio_boost;	
};

struct iwl2000_bt_cmd {
	struct iwl_basic_bt_cmd basic;
	__le32 prio_boost;
	u8 reserved;
	u8 tx_prio_boost;	
	__le16 rx_prio_boost;	
};

#define IWLAGN_BT_SCO_ACTIVE	cpu_to_le32(BIT(0))

struct iwlagn_bt_sco_cmd {
	__le32 flags;
};


#define MEASUREMENT_FILTER_FLAG (RXON_FILTER_PROMISC_MSK         | \
				 RXON_FILTER_CTL2HOST_MSK        | \
				 RXON_FILTER_ACCEPT_GRP_MSK      | \
				 RXON_FILTER_DIS_DECRYPT_MSK     | \
				 RXON_FILTER_DIS_GRP_DECRYPT_MSK | \
				 RXON_FILTER_ASSOC_MSK           | \
				 RXON_FILTER_BCON_AWARE_MSK)

struct iwl_measure_channel {
	__le32 duration;	
	u8 channel;		
	u8 type;		
	__le16 reserved;
} __packed;

struct iwl_spectrum_cmd {
	__le16 len;		
	u8 token;		
	u8 id;			
	u8 origin;		
	u8 periodic;		
	__le16 path_loss_timeout;
	__le32 start_time;	
	__le32 reserved2;
	__le32 flags;		
	__le32 filter_flags;	
	__le16 channel_count;	
	__le16 reserved3;
	struct iwl_measure_channel channels[10];
} __packed;

struct iwl_spectrum_resp {
	u8 token;
	u8 id;			
	__le16 status;		
} __packed;

enum iwl_measurement_state {
	IWL_MEASUREMENT_START = 0,
	IWL_MEASUREMENT_STOP = 1,
};

enum iwl_measurement_status {
	IWL_MEASUREMENT_OK = 0,
	IWL_MEASUREMENT_CONCURRENT = 1,
	IWL_MEASUREMENT_CSA_CONFLICT = 2,
	IWL_MEASUREMENT_TGH_CONFLICT = 3,
	
	IWL_MEASUREMENT_STOPPED = 6,
	IWL_MEASUREMENT_TIMEOUT = 7,
	IWL_MEASUREMENT_PERIODIC_FAILED = 8,
};

#define NUM_ELEMENTS_IN_HISTOGRAM 8

struct iwl_measurement_histogram {
	__le32 ofdm[NUM_ELEMENTS_IN_HISTOGRAM];	
	__le32 cck[NUM_ELEMENTS_IN_HISTOGRAM];	
} __packed;

struct iwl_measurement_cca_counters {
	__le32 ofdm;
	__le32 cck;
} __packed;

enum iwl_measure_type {
	IWL_MEASURE_BASIC = (1 << 0),
	IWL_MEASURE_CHANNEL_LOAD = (1 << 1),
	IWL_MEASURE_HISTOGRAM_RPI = (1 << 2),
	IWL_MEASURE_HISTOGRAM_NOISE = (1 << 3),
	IWL_MEASURE_FRAME = (1 << 4),
	
	IWL_MEASURE_IDLE = (1 << 7),
};

struct iwl_spectrum_notification {
	u8 id;			
	u8 token;
	u8 channel_index;	
	u8 state;		
	__le32 start_time;	
	u8 band;		
	u8 channel;
	u8 type;		
	u8 reserved1;
	__le32 cca_ofdm;	
	__le32 cca_cck;		
	__le32 cca_time;	
	u8 basic_type;		
	u8 reserved2[3];
	struct iwl_measurement_histogram histogram;
	__le32 stop_time;	
	__le32 status;		
} __packed;


#define IWL_POWER_VEC_SIZE 5

#define IWL_POWER_DRIVER_ALLOW_SLEEP_MSK	cpu_to_le16(BIT(0))
#define IWL_POWER_POWER_SAVE_ENA_MSK		cpu_to_le16(BIT(0))
#define IWL_POWER_POWER_MANAGEMENT_ENA_MSK	cpu_to_le16(BIT(1))
#define IWL_POWER_SLEEP_OVER_DTIM_MSK		cpu_to_le16(BIT(2))
#define IWL_POWER_PCI_PM_MSK			cpu_to_le16(BIT(3))
#define IWL_POWER_FAST_PD			cpu_to_le16(BIT(4))
#define IWL_POWER_BEACON_FILTERING		cpu_to_le16(BIT(5))
#define IWL_POWER_SHADOW_REG_ENA		cpu_to_le16(BIT(6))
#define IWL_POWER_CT_KILL_SET			cpu_to_le16(BIT(7))
#define IWL_POWER_BT_SCO_ENA			cpu_to_le16(BIT(8))
#define IWL_POWER_ADVANCE_PM_ENA_MSK		cpu_to_le16(BIT(9))

struct iwl_powertable_cmd {
	__le16 flags;
	u8 keep_alive_seconds;
	u8 debug_flags;
	__le32 rx_data_timeout;
	__le32 tx_data_timeout;
	__le32 sleep_interval[IWL_POWER_VEC_SIZE];
	__le32 keep_alive_beacons;
} __packed;

struct iwl_sleep_notification {
	u8 pm_sleep_mode;
	u8 pm_wakeup_src;
	__le16 reserved;
	__le32 sleep_time;
	__le32 tsf_low;
	__le32 bcon_timer;
} __packed;

enum {
	IWL_PM_NO_SLEEP = 0,
	IWL_PM_SLP_MAC = 1,
	IWL_PM_SLP_FULL_MAC_UNASSOCIATE = 2,
	IWL_PM_SLP_FULL_MAC_CARD_STATE = 3,
	IWL_PM_SLP_PHY = 4,
	IWL_PM_SLP_REPENT = 5,
	IWL_PM_WAKEUP_BY_TIMER = 6,
	IWL_PM_WAKEUP_BY_DRIVER = 7,
	IWL_PM_WAKEUP_BY_RFKILL = 8,
	
	IWL_PM_NUM_OF_MODES = 12,
};

#define CARD_STATE_CMD_DISABLE 0x00	
#define CARD_STATE_CMD_ENABLE  0x01	
#define CARD_STATE_CMD_HALT    0x02	
struct iwl_card_state_cmd {
	__le32 status;		
} __packed;

struct iwl_card_state_notif {
	__le32 flags;
} __packed;

#define HW_CARD_DISABLED   0x01
#define SW_CARD_DISABLED   0x02
#define CT_CARD_DISABLED   0x04
#define RXON_CARD_DISABLED 0x10

struct iwl_ct_kill_config {
	__le32   reserved;
	__le32   critical_temperature_M;
	__le32   critical_temperature_R;
}  __packed;

struct iwl_ct_kill_throttling_config {
	__le32   critical_temperature_exit;
	__le32   reserved;
	__le32   critical_temperature_enter;
}  __packed;


#define SCAN_CHANNEL_TYPE_PASSIVE cpu_to_le32(0)
#define SCAN_CHANNEL_TYPE_ACTIVE  cpu_to_le32(1)


struct iwl_scan_channel {
	__le32 type;
	__le16 channel;	
	u8 tx_gain;		
	u8 dsp_atten;		
	__le16 active_dwell;	
	__le16 passive_dwell;	
} __packed;

#define IWL_SCAN_PROBE_MASK(n) 	cpu_to_le32((BIT(n) | (BIT(n) - BIT(1))))

struct iwl_ssid_ie {
	u8 id;
	u8 len;
	u8 ssid[32];
} __packed;

#define PROBE_OPTION_MAX		20
#define TX_CMD_LIFE_TIME_INFINITE	cpu_to_le32(0xFFFFFFFF)
#define IWL_GOOD_CRC_TH_DISABLED	0
#define IWL_GOOD_CRC_TH_DEFAULT		cpu_to_le16(1)
#define IWL_GOOD_CRC_TH_NEVER		cpu_to_le16(0xffff)
#define IWL_MAX_SCAN_SIZE 1024
#define IWL_MAX_CMD_SIZE 4096


enum iwl_scan_flags {
	
	IWL_SCAN_FLAGS_ACTION_FRAME_TX	= BIT(1),
	
};

struct iwl_scan_cmd {
	__le16 len;
	u8 scan_flags;		
	u8 channel_count;	
	__le16 quiet_time;	
	__le16 quiet_plcp_th;	
	__le16 good_CRC_th;	
	__le16 rx_chain;	
	__le32 max_out_time;	
	__le32 suspend_time;	
	__le32 flags;		
	__le32 filter_flags;	

	struct iwl_tx_cmd tx_cmd;

	
	struct iwl_ssid_ie direct_scan[PROBE_OPTION_MAX];

	u8 data[0];
} __packed;

#define CAN_ABORT_STATUS	cpu_to_le32(0x1)
#define ABORT_STATUS            0x2

struct iwl_scanreq_notification {
	__le32 status;		
} __packed;

struct iwl_scanstart_notification {
	__le32 tsf_low;
	__le32 tsf_high;
	__le32 beacon_timer;
	u8 channel;
	u8 band;
	u8 reserved[2];
	__le32 status;
} __packed;

#define  SCAN_OWNER_STATUS 0x1
#define  MEASURE_OWNER_STATUS 0x2

#define IWL_PROBE_STATUS_OK		0
#define IWL_PROBE_STATUS_TX_FAILED	BIT(0)
#define IWL_PROBE_STATUS_FAIL_TTL	BIT(1)
#define IWL_PROBE_STATUS_FAIL_BT	BIT(2)

#define NUMBER_OF_STATISTICS 1	
struct iwl_scanresults_notification {
	u8 channel;
	u8 band;
	u8 probe_status;
	u8 num_probe_not_sent; 
	__le32 tsf_low;
	__le32 tsf_high;
	__le32 statistics[NUMBER_OF_STATISTICS];
} __packed;

struct iwl_scancomplete_notification {
	u8 scanned_channels;
	u8 status;
	u8 bt_status;	
	u8 last_channel;
	__le32 tsf_low;
	__le32 tsf_high;
} __packed;



enum iwl_ibss_manager {
	IWL_NOT_IBSS_MANAGER = 0,
	IWL_IBSS_MANAGER = 1,
};


struct iwlagn_beacon_notif {
	struct iwlagn_tx_resp beacon_notify_hdr;
	__le32 low_tsf;
	__le32 high_tsf;
	__le32 ibss_mgr_status;
} __packed;


struct iwl_tx_beacon_cmd {
	struct iwl_tx_cmd tx;
	__le16 tim_idx;
	u8 tim_size;
	u8 reserved1;
	struct ieee80211_hdr frame[0];	
} __packed;


#define IWL_TEMP_CONVERT 260

#define SUP_RATE_11A_MAX_NUM_CHANNELS  8
#define SUP_RATE_11B_MAX_NUM_CHANNELS  4
#define SUP_RATE_11G_MAX_NUM_CHANNELS  12

struct rate_histogram {
	union {
		__le32 a[SUP_RATE_11A_MAX_NUM_CHANNELS];
		__le32 b[SUP_RATE_11B_MAX_NUM_CHANNELS];
		__le32 g[SUP_RATE_11G_MAX_NUM_CHANNELS];
	} success;
	union {
		__le32 a[SUP_RATE_11A_MAX_NUM_CHANNELS];
		__le32 b[SUP_RATE_11B_MAX_NUM_CHANNELS];
		__le32 g[SUP_RATE_11G_MAX_NUM_CHANNELS];
	} failed;
} __packed;


struct statistics_dbg {
	__le32 burst_check;
	__le32 burst_count;
	__le32 wait_for_silence_timeout_cnt;
	__le32 reserved[3];
} __packed;

struct statistics_rx_phy {
	__le32 ina_cnt;
	__le32 fina_cnt;
	__le32 plcp_err;
	__le32 crc32_err;
	__le32 overrun_err;
	__le32 early_overrun_err;
	__le32 crc32_good;
	__le32 false_alarm_cnt;
	__le32 fina_sync_err_cnt;
	__le32 sfd_timeout;
	__le32 fina_timeout;
	__le32 unresponded_rts;
	__le32 rxe_frame_limit_overrun;
	__le32 sent_ack_cnt;
	__le32 sent_cts_cnt;
	__le32 sent_ba_rsp_cnt;
	__le32 dsp_self_kill;
	__le32 mh_format_err;
	__le32 re_acq_main_rssi_sum;
	__le32 reserved3;
} __packed;

struct statistics_rx_ht_phy {
	__le32 plcp_err;
	__le32 overrun_err;
	__le32 early_overrun_err;
	__le32 crc32_good;
	__le32 crc32_err;
	__le32 mh_format_err;
	__le32 agg_crc32_good;
	__le32 agg_mpdu_cnt;
	__le32 agg_cnt;
	__le32 unsupport_mcs;
} __packed;

#define INTERFERENCE_DATA_AVAILABLE      cpu_to_le32(1)

struct statistics_rx_non_phy {
	__le32 bogus_cts;	
	__le32 bogus_ack;	
	__le32 non_bssid_frames;	
	__le32 filtered_frames;	
	__le32 non_channel_beacons;	
	__le32 channel_beacons;	
	__le32 num_missed_bcon;	
	__le32 adc_rx_saturation_time;	
	__le32 ina_detection_search_time;
	__le32 beacon_silence_rssi_a;	
	__le32 beacon_silence_rssi_b;	
	__le32 beacon_silence_rssi_c;	
	__le32 interference_data_flag;	
	__le32 channel_load;		
	__le32 dsp_false_alarms;	
	__le32 beacon_rssi_a;
	__le32 beacon_rssi_b;
	__le32 beacon_rssi_c;
	__le32 beacon_energy_a;
	__le32 beacon_energy_b;
	__le32 beacon_energy_c;
} __packed;

struct statistics_rx_non_phy_bt {
	struct statistics_rx_non_phy common;
	
	__le32 num_bt_kills;
	__le32 reserved[2];
} __packed;

struct statistics_rx {
	struct statistics_rx_phy ofdm;
	struct statistics_rx_phy cck;
	struct statistics_rx_non_phy general;
	struct statistics_rx_ht_phy ofdm_ht;
} __packed;

struct statistics_rx_bt {
	struct statistics_rx_phy ofdm;
	struct statistics_rx_phy cck;
	struct statistics_rx_non_phy_bt general;
	struct statistics_rx_ht_phy ofdm_ht;
} __packed;

struct statistics_tx_power {
	u8 ant_a;
	u8 ant_b;
	u8 ant_c;
	u8 reserved;
} __packed;

struct statistics_tx_non_phy_agg {
	__le32 ba_timeout;
	__le32 ba_reschedule_frames;
	__le32 scd_query_agg_frame_cnt;
	__le32 scd_query_no_agg;
	__le32 scd_query_agg;
	__le32 scd_query_mismatch;
	__le32 frame_not_ready;
	__le32 underrun;
	__le32 bt_prio_kill;
	__le32 rx_ba_rsp_cnt;
} __packed;

struct statistics_tx {
	__le32 preamble_cnt;
	__le32 rx_detected_cnt;
	__le32 bt_prio_defer_cnt;
	__le32 bt_prio_kill_cnt;
	__le32 few_bytes_cnt;
	__le32 cts_timeout;
	__le32 ack_timeout;
	__le32 expected_ack_cnt;
	__le32 actual_ack_cnt;
	__le32 dump_msdu_cnt;
	__le32 burst_abort_next_frame_mismatch_cnt;
	__le32 burst_abort_missing_next_frame_cnt;
	__le32 cts_timeout_collision;
	__le32 ack_or_ba_timeout_collision;
	struct statistics_tx_non_phy_agg agg;
	struct statistics_tx_power tx_power;
	__le32 reserved1;
} __packed;


struct statistics_div {
	__le32 tx_on_a;
	__le32 tx_on_b;
	__le32 exec_time;
	__le32 probe_time;
	__le32 reserved1;
	__le32 reserved2;
} __packed;

struct statistics_general_common {
	__le32 temperature;   
	__le32 temperature_m; 
	struct statistics_dbg dbg;
	__le32 sleep_time;
	__le32 slots_out;
	__le32 slots_idle;
	__le32 ttl_timestamp;
	struct statistics_div div;
	__le32 rx_enable_counter;
	__le32 num_of_sos_states;
} __packed;

struct statistics_bt_activity {
	
	__le32 hi_priority_tx_req_cnt;
	__le32 hi_priority_tx_denied_cnt;
	__le32 lo_priority_tx_req_cnt;
	__le32 lo_priority_tx_denied_cnt;
	
	__le32 hi_priority_rx_req_cnt;
	__le32 hi_priority_rx_denied_cnt;
	__le32 lo_priority_rx_req_cnt;
	__le32 lo_priority_rx_denied_cnt;
} __packed;

struct statistics_general {
	struct statistics_general_common common;
	__le32 reserved2;
	__le32 reserved3;
} __packed;

struct statistics_general_bt {
	struct statistics_general_common common;
	struct statistics_bt_activity activity;
	__le32 reserved2;
	__le32 reserved3;
} __packed;

#define UCODE_STATISTICS_CLEAR_MSK		(0x1 << 0)
#define UCODE_STATISTICS_FREQUENCY_MSK		(0x1 << 1)
#define UCODE_STATISTICS_NARROW_BAND_MSK	(0x1 << 2)

#define IWL_STATS_CONF_CLEAR_STATS cpu_to_le32(0x1)	
#define IWL_STATS_CONF_DISABLE_NOTIF cpu_to_le32(0x2)
struct iwl_statistics_cmd {
	__le32 configuration_flags;	
} __packed;

#define STATISTICS_REPLY_FLG_BAND_24G_MSK         cpu_to_le32(0x2)
#define STATISTICS_REPLY_FLG_HT40_MODE_MSK        cpu_to_le32(0x8)

struct iwl_notif_statistics {
	__le32 flag;
	struct statistics_rx rx;
	struct statistics_tx tx;
	struct statistics_general general;
} __packed;

struct iwl_bt_notif_statistics {
	__le32 flag;
	struct statistics_rx_bt rx;
	struct statistics_tx tx;
	struct statistics_general_bt general;
} __packed;


#define IWL_MISSED_BEACON_THRESHOLD_MIN	(1)
#define IWL_MISSED_BEACON_THRESHOLD_DEF	(5)
#define IWL_MISSED_BEACON_THRESHOLD_MAX	IWL_MISSED_BEACON_THRESHOLD_DEF

struct iwl_missed_beacon_notif {
	__le32 consecutive_missed_beacons;
	__le32 total_missed_becons;
	__le32 num_expected_beacons;
	__le32 num_recvd_beacons;
} __packed;




#define HD_TABLE_SIZE  (11)	
#define HD_MIN_ENERGY_CCK_DET_INDEX                 (0)	
#define HD_MIN_ENERGY_OFDM_DET_INDEX                (1)
#define HD_AUTO_CORR32_X1_TH_ADD_MIN_INDEX          (2)
#define HD_AUTO_CORR32_X1_TH_ADD_MIN_MRC_INDEX      (3)
#define HD_AUTO_CORR40_X4_TH_ADD_MIN_MRC_INDEX      (4)
#define HD_AUTO_CORR32_X4_TH_ADD_MIN_INDEX          (5)
#define HD_AUTO_CORR32_X4_TH_ADD_MIN_MRC_INDEX      (6)
#define HD_BARKER_CORR_TH_ADD_MIN_INDEX             (7)
#define HD_BARKER_CORR_TH_ADD_MIN_MRC_INDEX         (8)
#define HD_AUTO_CORR40_X4_TH_ADD_MIN_INDEX          (9)
#define HD_OFDM_ENERGY_TH_IN_INDEX                  (10)

#define HD_INA_NON_SQUARE_DET_OFDM_INDEX		(11)
#define HD_INA_NON_SQUARE_DET_CCK_INDEX			(12)
#define HD_CORR_11_INSTEAD_OF_CORR_9_EN_INDEX		(13)
#define HD_OFDM_NON_SQUARE_DET_SLOPE_MRC_INDEX		(14)
#define HD_OFDM_NON_SQUARE_DET_INTERCEPT_MRC_INDEX	(15)
#define HD_OFDM_NON_SQUARE_DET_SLOPE_INDEX		(16)
#define HD_OFDM_NON_SQUARE_DET_INTERCEPT_INDEX		(17)
#define HD_CCK_NON_SQUARE_DET_SLOPE_MRC_INDEX		(18)
#define HD_CCK_NON_SQUARE_DET_INTERCEPT_MRC_INDEX	(19)
#define HD_CCK_NON_SQUARE_DET_SLOPE_INDEX		(20)
#define HD_CCK_NON_SQUARE_DET_INTERCEPT_INDEX		(21)
#define HD_RESERVED					(22)

#define ENHANCE_HD_TABLE_SIZE  (23)

#define ENHANCE_HD_TABLE_ENTRIES  (ENHANCE_HD_TABLE_SIZE - HD_TABLE_SIZE)

#define HD_INA_NON_SQUARE_DET_OFDM_DATA_V1		cpu_to_le16(0)
#define HD_INA_NON_SQUARE_DET_CCK_DATA_V1		cpu_to_le16(0)
#define HD_CORR_11_INSTEAD_OF_CORR_9_EN_DATA_V1		cpu_to_le16(0)
#define HD_OFDM_NON_SQUARE_DET_SLOPE_MRC_DATA_V1	cpu_to_le16(668)
#define HD_OFDM_NON_SQUARE_DET_INTERCEPT_MRC_DATA_V1	cpu_to_le16(4)
#define HD_OFDM_NON_SQUARE_DET_SLOPE_DATA_V1		cpu_to_le16(486)
#define HD_OFDM_NON_SQUARE_DET_INTERCEPT_DATA_V1	cpu_to_le16(37)
#define HD_CCK_NON_SQUARE_DET_SLOPE_MRC_DATA_V1		cpu_to_le16(853)
#define HD_CCK_NON_SQUARE_DET_INTERCEPT_MRC_DATA_V1	cpu_to_le16(4)
#define HD_CCK_NON_SQUARE_DET_SLOPE_DATA_V1		cpu_to_le16(476)
#define HD_CCK_NON_SQUARE_DET_INTERCEPT_DATA_V1		cpu_to_le16(99)

#define HD_INA_NON_SQUARE_DET_OFDM_DATA_V2		cpu_to_le16(1)
#define HD_INA_NON_SQUARE_DET_CCK_DATA_V2		cpu_to_le16(1)
#define HD_CORR_11_INSTEAD_OF_CORR_9_EN_DATA_V2		cpu_to_le16(1)
#define HD_OFDM_NON_SQUARE_DET_SLOPE_MRC_DATA_V2	cpu_to_le16(600)
#define HD_OFDM_NON_SQUARE_DET_INTERCEPT_MRC_DATA_V2	cpu_to_le16(40)
#define HD_OFDM_NON_SQUARE_DET_SLOPE_DATA_V2		cpu_to_le16(486)
#define HD_OFDM_NON_SQUARE_DET_INTERCEPT_DATA_V2	cpu_to_le16(45)
#define HD_CCK_NON_SQUARE_DET_SLOPE_MRC_DATA_V2		cpu_to_le16(853)
#define HD_CCK_NON_SQUARE_DET_INTERCEPT_MRC_DATA_V2	cpu_to_le16(60)
#define HD_CCK_NON_SQUARE_DET_SLOPE_DATA_V2		cpu_to_le16(476)
#define HD_CCK_NON_SQUARE_DET_INTERCEPT_DATA_V2		cpu_to_le16(99)


#define SENSITIVITY_CMD_CONTROL_DEFAULT_TABLE	cpu_to_le16(0)
#define SENSITIVITY_CMD_CONTROL_WORK_TABLE	cpu_to_le16(1)

struct iwl_sensitivity_cmd {
	__le16 control;			
	__le16 table[HD_TABLE_SIZE];	
} __packed;

struct iwl_enhance_sensitivity_cmd {
	__le16 control;			
	__le16 enhance_table[ENHANCE_HD_TABLE_SIZE];	
} __packed;



enum {
	IWL_PHY_CALIBRATE_DC_CMD		= 8,
	IWL_PHY_CALIBRATE_LO_CMD		= 9,
	IWL_PHY_CALIBRATE_TX_IQ_CMD		= 11,
	IWL_PHY_CALIBRATE_CRYSTAL_FRQ_CMD	= 15,
	IWL_PHY_CALIBRATE_BASE_BAND_CMD		= 16,
	IWL_PHY_CALIBRATE_TX_IQ_PERD_CMD	= 17,
	IWL_PHY_CALIBRATE_TEMP_OFFSET_CMD	= 18,
};

enum iwl_ucode_calib_cfg {
	IWL_CALIB_CFG_RX_BB_IDX			= BIT(0),
	IWL_CALIB_CFG_DC_IDX			= BIT(1),
	IWL_CALIB_CFG_LO_IDX			= BIT(2),
	IWL_CALIB_CFG_TX_IQ_IDX			= BIT(3),
	IWL_CALIB_CFG_RX_IQ_IDX			= BIT(4),
	IWL_CALIB_CFG_NOISE_IDX			= BIT(5),
	IWL_CALIB_CFG_CRYSTAL_IDX		= BIT(6),
	IWL_CALIB_CFG_TEMPERATURE_IDX		= BIT(7),
	IWL_CALIB_CFG_PAPD_IDX			= BIT(8),
	IWL_CALIB_CFG_SENSITIVITY_IDX		= BIT(9),
	IWL_CALIB_CFG_TX_PWR_IDX		= BIT(10),
};

#define IWL_CALIB_INIT_CFG_ALL	cpu_to_le32(IWL_CALIB_CFG_RX_BB_IDX |	\
					IWL_CALIB_CFG_DC_IDX |		\
					IWL_CALIB_CFG_LO_IDX |		\
					IWL_CALIB_CFG_TX_IQ_IDX |	\
					IWL_CALIB_CFG_RX_IQ_IDX |	\
					IWL_CALIB_CFG_CRYSTAL_IDX)

#define IWL_CALIB_RT_CFG_ALL	cpu_to_le32(IWL_CALIB_CFG_RX_BB_IDX |	\
					IWL_CALIB_CFG_DC_IDX |		\
					IWL_CALIB_CFG_LO_IDX |		\
					IWL_CALIB_CFG_TX_IQ_IDX |	\
					IWL_CALIB_CFG_RX_IQ_IDX |	\
					IWL_CALIB_CFG_TEMPERATURE_IDX |	\
					IWL_CALIB_CFG_PAPD_IDX |	\
					IWL_CALIB_CFG_TX_PWR_IDX |	\
					IWL_CALIB_CFG_CRYSTAL_IDX)

#define IWL_CALIB_CFG_FLAG_SEND_COMPLETE_NTFY_MSK	cpu_to_le32(BIT(0))

struct iwl_calib_cfg_elmnt_s {
	__le32 is_enable;
	__le32 start;
	__le32 send_res;
	__le32 apply_res;
	__le32 reserved;
} __packed;

struct iwl_calib_cfg_status_s {
	struct iwl_calib_cfg_elmnt_s once;
	struct iwl_calib_cfg_elmnt_s perd;
	__le32 flags;
} __packed;

struct iwl_calib_cfg_cmd {
	struct iwl_calib_cfg_status_s ucd_calib_cfg;
	struct iwl_calib_cfg_status_s drv_calib_cfg;
	__le32 reserved1;
} __packed;

struct iwl_calib_hdr {
	u8 op_code;
	u8 first_group;
	u8 groups_num;
	u8 data_valid;
} __packed;

struct iwl_calib_cmd {
	struct iwl_calib_hdr hdr;
	u8 data[0];
} __packed;

struct iwl_calib_xtal_freq_cmd {
	struct iwl_calib_hdr hdr;
	u8 cap_pin1;
	u8 cap_pin2;
	u8 pad[2];
} __packed;

#define DEFAULT_RADIO_SENSOR_OFFSET    cpu_to_le16(2700)
struct iwl_calib_temperature_offset_cmd {
	struct iwl_calib_hdr hdr;
	__le16 radio_sensor_offset;
	__le16 reserved;
} __packed;

struct iwl_calib_temperature_offset_v2_cmd {
	struct iwl_calib_hdr hdr;
	__le16 radio_sensor_offset_high;
	__le16 radio_sensor_offset_low;
	__le16 burntVoltageRef;
	__le16 reserved;
} __packed;

struct iwl_calib_chain_noise_reset_cmd {
	struct iwl_calib_hdr hdr;
	u8 data[0];
};

struct iwl_calib_chain_noise_gain_cmd {
	struct iwl_calib_hdr hdr;
	u8 delta_gain_1;
	u8 delta_gain_2;
	u8 pad[2];
} __packed;


struct iwl_led_cmd {
	__le32 interval;	
	u8 id;			
	u8 off;			
	u8 on;			
	u8 reserved;
} __packed;


#define COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG        (0x1)
#define COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG        (0x2)
#define COEX_EVT_FLAG_DELAY_MEDIUM_FREE_NTFY_FLG  (0x4)

#define COEX_CU_UNASSOC_IDLE_RP               4
#define COEX_CU_UNASSOC_MANUAL_SCAN_RP        4
#define COEX_CU_UNASSOC_AUTO_SCAN_RP          4
#define COEX_CU_CALIBRATION_RP                4
#define COEX_CU_PERIODIC_CALIBRATION_RP       4
#define COEX_CU_CONNECTION_ESTAB_RP           4
#define COEX_CU_ASSOCIATED_IDLE_RP            4
#define COEX_CU_ASSOC_MANUAL_SCAN_RP          4
#define COEX_CU_ASSOC_AUTO_SCAN_RP            4
#define COEX_CU_ASSOC_ACTIVE_LEVEL_RP         4
#define COEX_CU_RF_ON_RP                      6
#define COEX_CU_RF_OFF_RP                     4
#define COEX_CU_STAND_ALONE_DEBUG_RP          6
#define COEX_CU_IPAN_ASSOC_LEVEL_RP           4
#define COEX_CU_RSRVD1_RP                     4
#define COEX_CU_RSRVD2_RP                     4

#define COEX_CU_UNASSOC_IDLE_WP               3
#define COEX_CU_UNASSOC_MANUAL_SCAN_WP        3
#define COEX_CU_UNASSOC_AUTO_SCAN_WP          3
#define COEX_CU_CALIBRATION_WP                3
#define COEX_CU_PERIODIC_CALIBRATION_WP       3
#define COEX_CU_CONNECTION_ESTAB_WP           3
#define COEX_CU_ASSOCIATED_IDLE_WP            3
#define COEX_CU_ASSOC_MANUAL_SCAN_WP          3
#define COEX_CU_ASSOC_AUTO_SCAN_WP            3
#define COEX_CU_ASSOC_ACTIVE_LEVEL_WP         3
#define COEX_CU_RF_ON_WP                      3
#define COEX_CU_RF_OFF_WP                     3
#define COEX_CU_STAND_ALONE_DEBUG_WP          6
#define COEX_CU_IPAN_ASSOC_LEVEL_WP           3
#define COEX_CU_RSRVD1_WP                     3
#define COEX_CU_RSRVD2_WP                     3

#define COEX_UNASSOC_IDLE_FLAGS                     0
#define COEX_UNASSOC_MANUAL_SCAN_FLAGS		\
	(COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG |	\
	COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG)
#define COEX_UNASSOC_AUTO_SCAN_FLAGS		\
	(COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG |	\
	COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG)
#define COEX_CALIBRATION_FLAGS			\
	(COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG |	\
	COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG)
#define COEX_PERIODIC_CALIBRATION_FLAGS             0
#define COEX_CONNECTION_ESTAB_FLAGS		\
	(COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG |	\
	COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG |	\
	COEX_EVT_FLAG_DELAY_MEDIUM_FREE_NTFY_FLG)
#define COEX_ASSOCIATED_IDLE_FLAGS                  0
#define COEX_ASSOC_MANUAL_SCAN_FLAGS		\
	(COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG |	\
	COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG)
#define COEX_ASSOC_AUTO_SCAN_FLAGS		\
	(COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG |	\
	 COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG)
#define COEX_ASSOC_ACTIVE_LEVEL_FLAGS               0
#define COEX_RF_ON_FLAGS                            0
#define COEX_RF_OFF_FLAGS                           0
#define COEX_STAND_ALONE_DEBUG_FLAGS		\
	(COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG |	\
	 COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG)
#define COEX_IPAN_ASSOC_LEVEL_FLAGS		\
	(COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG |	\
	 COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG |	\
	 COEX_EVT_FLAG_DELAY_MEDIUM_FREE_NTFY_FLG)
#define COEX_RSRVD1_FLAGS                           0
#define COEX_RSRVD2_FLAGS                           0
#define COEX_CU_RF_ON_FLAGS			\
	(COEX_EVT_FLAG_MEDIUM_FREE_NTFY_FLG |	\
	 COEX_EVT_FLAG_MEDIUM_ACTV_NTFY_FLG |	\
	 COEX_EVT_FLAG_DELAY_MEDIUM_FREE_NTFY_FLG)


enum {
	
	COEX_UNASSOC_IDLE		= 0,
	COEX_UNASSOC_MANUAL_SCAN	= 1,
	COEX_UNASSOC_AUTO_SCAN		= 2,
	
	COEX_CALIBRATION		= 3,
	COEX_PERIODIC_CALIBRATION	= 4,
	
	COEX_CONNECTION_ESTAB		= 5,
	
	COEX_ASSOCIATED_IDLE		= 6,
	COEX_ASSOC_MANUAL_SCAN		= 7,
	COEX_ASSOC_AUTO_SCAN		= 8,
	COEX_ASSOC_ACTIVE_LEVEL		= 9,
	
	COEX_RF_ON			= 10,
	COEX_RF_OFF			= 11,
	COEX_STAND_ALONE_DEBUG		= 12,
	
	COEX_IPAN_ASSOC_LEVEL		= 13,
	
	COEX_RSRVD1			= 14,
	COEX_RSRVD2			= 15,
	COEX_NUM_OF_EVENTS		= 16
};

struct iwl_wimax_coex_event_entry {
	u8 request_prio;
	u8 win_medium_prio;
	u8 reserved;
	u8 flags;
} __packed;


#define COEX_FLAGS_STA_TABLE_VALID_MSK      (0x1)
#define COEX_FLAGS_UNASSOC_WA_UNMASK_MSK    (0x4)
#define COEX_FLAGS_ASSOC_WA_UNMASK_MSK      (0x8)
#define COEX_FLAGS_COEX_ENABLE_MSK          (0x80)

struct iwl_wimax_coex_cmd {
	u8 flags;
	u8 reserved[3];
	struct iwl_wimax_coex_event_entry sta_prio[COEX_NUM_OF_EVENTS];
} __packed;

#define COEX_MEDIUM_BUSY	(0x0) 
#define COEX_MEDIUM_ACTIVE	(0x1) 
#define COEX_MEDIUM_PRE_RELEASE	(0x2) 
#define COEX_MEDIUM_MSK		(0x7)

#define COEX_MEDIUM_CHANGED	(0x8)
#define COEX_MEDIUM_CHANGED_MSK	(0x8)
#define COEX_MEDIUM_SHIFT	(3)

struct iwl_coex_medium_notification {
	__le32 status;
	__le32 events;
} __packed;

#define COEX_EVENT_REQUEST_MSK	(0x1)

struct iwl_coex_event_cmd {
	u8 flags;
	u8 event;
	__le16 reserved;
} __packed;

struct iwl_coex_event_resp {
	__le32 status;
} __packed;



enum iwl_bt_coex_profile_traffic_load {
	IWL_BT_COEX_TRAFFIC_LOAD_NONE = 	0,
	IWL_BT_COEX_TRAFFIC_LOAD_LOW =		1,
	IWL_BT_COEX_TRAFFIC_LOAD_HIGH = 	2,
	IWL_BT_COEX_TRAFFIC_LOAD_CONTINUOUS =	3,
};

#define BT_SESSION_ACTIVITY_1_UART_MSG		0x1
#define BT_SESSION_ACTIVITY_2_UART_MSG		0x2

#define BT_UART_MSG_FRAME1MSGTYPE_POS		(0)
#define BT_UART_MSG_FRAME1MSGTYPE_MSK		\
		(0x7 << BT_UART_MSG_FRAME1MSGTYPE_POS)
#define BT_UART_MSG_FRAME1SSN_POS		(3)
#define BT_UART_MSG_FRAME1SSN_MSK		\
		(0x3 << BT_UART_MSG_FRAME1SSN_POS)
#define BT_UART_MSG_FRAME1UPDATEREQ_POS		(5)
#define BT_UART_MSG_FRAME1UPDATEREQ_MSK		\
		(0x1 << BT_UART_MSG_FRAME1UPDATEREQ_POS)
#define BT_UART_MSG_FRAME1RESERVED_POS		(6)
#define BT_UART_MSG_FRAME1RESERVED_MSK		\
		(0x3 << BT_UART_MSG_FRAME1RESERVED_POS)

#define BT_UART_MSG_FRAME2OPENCONNECTIONS_POS	(0)
#define BT_UART_MSG_FRAME2OPENCONNECTIONS_MSK	\
		(0x3 << BT_UART_MSG_FRAME2OPENCONNECTIONS_POS)
#define BT_UART_MSG_FRAME2TRAFFICLOAD_POS	(2)
#define BT_UART_MSG_FRAME2TRAFFICLOAD_MSK	\
		(0x3 << BT_UART_MSG_FRAME2TRAFFICLOAD_POS)
#define BT_UART_MSG_FRAME2CHLSEQN_POS		(4)
#define BT_UART_MSG_FRAME2CHLSEQN_MSK		\
		(0x1 << BT_UART_MSG_FRAME2CHLSEQN_POS)
#define BT_UART_MSG_FRAME2INBAND_POS		(5)
#define BT_UART_MSG_FRAME2INBAND_MSK		\
		(0x1 << BT_UART_MSG_FRAME2INBAND_POS)
#define BT_UART_MSG_FRAME2RESERVED_POS		(6)
#define BT_UART_MSG_FRAME2RESERVED_MSK		\
		(0x3 << BT_UART_MSG_FRAME2RESERVED_POS)

#define BT_UART_MSG_FRAME3SCOESCO_POS		(0)
#define BT_UART_MSG_FRAME3SCOESCO_MSK		\
		(0x1 << BT_UART_MSG_FRAME3SCOESCO_POS)
#define BT_UART_MSG_FRAME3SNIFF_POS		(1)
#define BT_UART_MSG_FRAME3SNIFF_MSK		\
		(0x1 << BT_UART_MSG_FRAME3SNIFF_POS)
#define BT_UART_MSG_FRAME3A2DP_POS		(2)
#define BT_UART_MSG_FRAME3A2DP_MSK		\
		(0x1 << BT_UART_MSG_FRAME3A2DP_POS)
#define BT_UART_MSG_FRAME3ACL_POS		(3)
#define BT_UART_MSG_FRAME3ACL_MSK		\
		(0x1 << BT_UART_MSG_FRAME3ACL_POS)
#define BT_UART_MSG_FRAME3MASTER_POS		(4)
#define BT_UART_MSG_FRAME3MASTER_MSK		\
		(0x1 << BT_UART_MSG_FRAME3MASTER_POS)
#define BT_UART_MSG_FRAME3OBEX_POS		(5)
#define BT_UART_MSG_FRAME3OBEX_MSK		\
		(0x1 << BT_UART_MSG_FRAME3OBEX_POS)
#define BT_UART_MSG_FRAME3RESERVED_POS		(6)
#define BT_UART_MSG_FRAME3RESERVED_MSK		\
		(0x3 << BT_UART_MSG_FRAME3RESERVED_POS)

#define BT_UART_MSG_FRAME4IDLEDURATION_POS	(0)
#define BT_UART_MSG_FRAME4IDLEDURATION_MSK	\
		(0x3F << BT_UART_MSG_FRAME4IDLEDURATION_POS)
#define BT_UART_MSG_FRAME4RESERVED_POS		(6)
#define BT_UART_MSG_FRAME4RESERVED_MSK		\
		(0x3 << BT_UART_MSG_FRAME4RESERVED_POS)

#define BT_UART_MSG_FRAME5TXACTIVITY_POS	(0)
#define BT_UART_MSG_FRAME5TXACTIVITY_MSK	\
		(0x3 << BT_UART_MSG_FRAME5TXACTIVITY_POS)
#define BT_UART_MSG_FRAME5RXACTIVITY_POS	(2)
#define BT_UART_MSG_FRAME5RXACTIVITY_MSK	\
		(0x3 << BT_UART_MSG_FRAME5RXACTIVITY_POS)
#define BT_UART_MSG_FRAME5ESCORETRANSMIT_POS	(4)
#define BT_UART_MSG_FRAME5ESCORETRANSMIT_MSK	\
		(0x3 << BT_UART_MSG_FRAME5ESCORETRANSMIT_POS)
#define BT_UART_MSG_FRAME5RESERVED_POS		(6)
#define BT_UART_MSG_FRAME5RESERVED_MSK		\
		(0x3 << BT_UART_MSG_FRAME5RESERVED_POS)

#define BT_UART_MSG_FRAME6SNIFFINTERVAL_POS	(0)
#define BT_UART_MSG_FRAME6SNIFFINTERVAL_MSK	\
		(0x1F << BT_UART_MSG_FRAME6SNIFFINTERVAL_POS)
#define BT_UART_MSG_FRAME6DISCOVERABLE_POS	(5)
#define BT_UART_MSG_FRAME6DISCOVERABLE_MSK	\
		(0x1 << BT_UART_MSG_FRAME6DISCOVERABLE_POS)
#define BT_UART_MSG_FRAME6RESERVED_POS		(6)
#define BT_UART_MSG_FRAME6RESERVED_MSK		\
		(0x3 << BT_UART_MSG_FRAME6RESERVED_POS)

#define BT_UART_MSG_FRAME7SNIFFACTIVITY_POS	(0)
#define BT_UART_MSG_FRAME7SNIFFACTIVITY_MSK	\
		(0x7 << BT_UART_MSG_FRAME7SNIFFACTIVITY_POS)
#define BT_UART_MSG_FRAME7PAGE_POS		(3)
#define BT_UART_MSG_FRAME7PAGE_MSK		\
		(0x1 << BT_UART_MSG_FRAME7PAGE_POS)
#define BT_UART_MSG_FRAME7INQUIRY_POS		(4)
#define BT_UART_MSG_FRAME7INQUIRY_MSK		\
		(0x1 << BT_UART_MSG_FRAME7INQUIRY_POS)
#define BT_UART_MSG_FRAME7CONNECTABLE_POS	(5)
#define BT_UART_MSG_FRAME7CONNECTABLE_MSK	\
		(0x1 << BT_UART_MSG_FRAME7CONNECTABLE_POS)
#define BT_UART_MSG_FRAME7RESERVED_POS		(6)
#define BT_UART_MSG_FRAME7RESERVED_MSK		\
		(0x3 << BT_UART_MSG_FRAME7RESERVED_POS)

#define BT_UART_MSG_2_FRAME1RESERVED1_POS	(5)
#define BT_UART_MSG_2_FRAME1RESERVED1_MSK	\
		(0x1<<BT_UART_MSG_2_FRAME1RESERVED1_POS)
#define BT_UART_MSG_2_FRAME1RESERVED2_POS	(6)
#define BT_UART_MSG_2_FRAME1RESERVED2_MSK	\
		(0x3<<BT_UART_MSG_2_FRAME1RESERVED2_POS)

#define BT_UART_MSG_2_FRAME2AGGTRAFFICLOAD_POS	(0)
#define BT_UART_MSG_2_FRAME2AGGTRAFFICLOAD_MSK	\
		(0x3F<<BT_UART_MSG_2_FRAME2AGGTRAFFICLOAD_POS)
#define BT_UART_MSG_2_FRAME2RESERVED_POS	(6)
#define BT_UART_MSG_2_FRAME2RESERVED_MSK	\
		(0x3<<BT_UART_MSG_2_FRAME2RESERVED_POS)

#define BT_UART_MSG_2_FRAME3BRLASTTXPOWER_POS	(0)
#define BT_UART_MSG_2_FRAME3BRLASTTXPOWER_MSK	\
		(0xF<<BT_UART_MSG_2_FRAME3BRLASTTXPOWER_POS)
#define BT_UART_MSG_2_FRAME3INQPAGESRMODE_POS	(4)
#define BT_UART_MSG_2_FRAME3INQPAGESRMODE_MSK	\
		(0x1<<BT_UART_MSG_2_FRAME3INQPAGESRMODE_POS)
#define BT_UART_MSG_2_FRAME3LEMASTER_POS	(5)
#define BT_UART_MSG_2_FRAME3LEMASTER_MSK	\
		(0x1<<BT_UART_MSG_2_FRAME3LEMASTER_POS)
#define BT_UART_MSG_2_FRAME3RESERVED_POS	(6)
#define BT_UART_MSG_2_FRAME3RESERVED_MSK	\
		(0x3<<BT_UART_MSG_2_FRAME3RESERVED_POS)

#define BT_UART_MSG_2_FRAME4LELASTTXPOWER_POS	(0)
#define BT_UART_MSG_2_FRAME4LELASTTXPOWER_MSK	\
		(0xF<<BT_UART_MSG_2_FRAME4LELASTTXPOWER_POS)
#define BT_UART_MSG_2_FRAME4NUMLECONN_POS	(4)
#define BT_UART_MSG_2_FRAME4NUMLECONN_MSK	\
		(0x3<<BT_UART_MSG_2_FRAME4NUMLECONN_POS)
#define BT_UART_MSG_2_FRAME4RESERVED_POS	(6)
#define BT_UART_MSG_2_FRAME4RESERVED_MSK	\
		(0x3<<BT_UART_MSG_2_FRAME4RESERVED_POS)

#define BT_UART_MSG_2_FRAME5BTMINRSSI_POS	(0)
#define BT_UART_MSG_2_FRAME5BTMINRSSI_MSK	\
		(0xF<<BT_UART_MSG_2_FRAME5BTMINRSSI_POS)
#define BT_UART_MSG_2_FRAME5LESCANINITMODE_POS	(4)
#define BT_UART_MSG_2_FRAME5LESCANINITMODE_MSK	\
		(0x1<<BT_UART_MSG_2_FRAME5LESCANINITMODE_POS)
#define BT_UART_MSG_2_FRAME5LEADVERMODE_POS	(5)
#define BT_UART_MSG_2_FRAME5LEADVERMODE_MSK	\
		(0x1<<BT_UART_MSG_2_FRAME5LEADVERMODE_POS)
#define BT_UART_MSG_2_FRAME5RESERVED_POS	(6)
#define BT_UART_MSG_2_FRAME5RESERVED_MSK	\
		(0x3<<BT_UART_MSG_2_FRAME5RESERVED_POS)

#define BT_UART_MSG_2_FRAME6LECONNINTERVAL_POS	(0)
#define BT_UART_MSG_2_FRAME6LECONNINTERVAL_MSK	\
		(0x1F<<BT_UART_MSG_2_FRAME6LECONNINTERVAL_POS)
#define BT_UART_MSG_2_FRAME6RFU_POS		(5)
#define BT_UART_MSG_2_FRAME6RFU_MSK		\
		(0x1<<BT_UART_MSG_2_FRAME6RFU_POS)
#define BT_UART_MSG_2_FRAME6RESERVED_POS	(6)
#define BT_UART_MSG_2_FRAME6RESERVED_MSK	\
		(0x3<<BT_UART_MSG_2_FRAME6RESERVED_POS)

#define BT_UART_MSG_2_FRAME7LECONNSLAVELAT_POS	(0)
#define BT_UART_MSG_2_FRAME7LECONNSLAVELAT_MSK	\
		(0x7<<BT_UART_MSG_2_FRAME7LECONNSLAVELAT_POS)
#define BT_UART_MSG_2_FRAME7LEPROFILE1_POS	(3)
#define BT_UART_MSG_2_FRAME7LEPROFILE1_MSK	\
		(0x1<<BT_UART_MSG_2_FRAME7LEPROFILE1_POS)
#define BT_UART_MSG_2_FRAME7LEPROFILE2_POS	(4)
#define BT_UART_MSG_2_FRAME7LEPROFILE2_MSK	\
		(0x1<<BT_UART_MSG_2_FRAME7LEPROFILE2_POS)
#define BT_UART_MSG_2_FRAME7LEPROFILEOTHER_POS	(5)
#define BT_UART_MSG_2_FRAME7LEPROFILEOTHER_MSK	\
		(0x1<<BT_UART_MSG_2_FRAME7LEPROFILEOTHER_POS)
#define BT_UART_MSG_2_FRAME7RESERVED_POS	(6)
#define BT_UART_MSG_2_FRAME7RESERVED_MSK	\
		(0x3<<BT_UART_MSG_2_FRAME7RESERVED_POS)


struct iwl_bt_uart_msg {
	u8 header;
	u8 frame1;
	u8 frame2;
	u8 frame3;
	u8 frame4;
	u8 frame5;
	u8 frame6;
	u8 frame7;
} __attribute__((packed));

struct iwl_bt_coex_profile_notif {
	struct iwl_bt_uart_msg last_bt_uart_msg;
	u8 bt_status; 
	u8 bt_traffic_load; 
	u8 bt_ci_compliance; 
	u8 reserved;
} __attribute__((packed));

#define IWL_BT_COEX_PRIO_TBL_SHARED_ANTENNA_POS	0
#define IWL_BT_COEX_PRIO_TBL_SHARED_ANTENNA_MSK	0x1
#define IWL_BT_COEX_PRIO_TBL_PRIO_POS		1
#define IWL_BT_COEX_PRIO_TBL_PRIO_MASK		0x0e
#define IWL_BT_COEX_PRIO_TBL_RESERVED_POS	4
#define IWL_BT_COEX_PRIO_TBL_RESERVED_MASK	0xf0
#define IWL_BT_COEX_PRIO_TBL_PRIO_SHIFT		1

enum bt_coex_prio_table_events {
	BT_COEX_PRIO_TBL_EVT_INIT_CALIB1 = 0,
	BT_COEX_PRIO_TBL_EVT_INIT_CALIB2 = 1,
	BT_COEX_PRIO_TBL_EVT_PERIODIC_CALIB_LOW1 = 2,
	BT_COEX_PRIO_TBL_EVT_PERIODIC_CALIB_LOW2 = 3, 
	BT_COEX_PRIO_TBL_EVT_PERIODIC_CALIB_HIGH1 = 4,
	BT_COEX_PRIO_TBL_EVT_PERIODIC_CALIB_HIGH2 = 5,
	BT_COEX_PRIO_TBL_EVT_DTIM = 6,
	BT_COEX_PRIO_TBL_EVT_SCAN52 = 7,
	BT_COEX_PRIO_TBL_EVT_SCAN24 = 8,
	BT_COEX_PRIO_TBL_EVT_RESERVED0 = 9,
	BT_COEX_PRIO_TBL_EVT_RESERVED1 = 10,
	BT_COEX_PRIO_TBL_EVT_RESERVED2 = 11,
	BT_COEX_PRIO_TBL_EVT_RESERVED3 = 12,
	BT_COEX_PRIO_TBL_EVT_RESERVED4 = 13,
	BT_COEX_PRIO_TBL_EVT_RESERVED5 = 14,
	BT_COEX_PRIO_TBL_EVT_RESERVED6 = 15,
	
	BT_COEX_PRIO_TBL_EVT_MAX,
};

enum bt_coex_prio_table_priorities {
	BT_COEX_PRIO_TBL_DISABLED = 0,
	BT_COEX_PRIO_TBL_PRIO_LOW = 1,
	BT_COEX_PRIO_TBL_PRIO_HIGH = 2,
	BT_COEX_PRIO_TBL_PRIO_BYPASS = 3,
	BT_COEX_PRIO_TBL_PRIO_COEX_OFF = 4,
	BT_COEX_PRIO_TBL_PRIO_COEX_ON = 5,
	BT_COEX_PRIO_TBL_PRIO_RSRVD1 = 6,
	BT_COEX_PRIO_TBL_PRIO_RSRVD2 = 7,
	BT_COEX_PRIO_TBL_MAX,
};

struct iwl_bt_coex_prio_table_cmd {
	u8 prio_tbl[BT_COEX_PRIO_TBL_EVT_MAX];
} __attribute__((packed));

#define IWL_BT_COEX_ENV_CLOSE	0
#define IWL_BT_COEX_ENV_OPEN	1
struct iwl_bt_coex_prot_env_cmd {
	u8 action; 
	u8 type; 
	u8 reserved[2];
} __attribute__((packed));

enum iwlagn_d3_wakeup_filters {
	IWLAGN_D3_WAKEUP_RFKILL		= BIT(0),
	IWLAGN_D3_WAKEUP_SYSASSERT	= BIT(1),
};

struct iwlagn_d3_config_cmd {
	__le32 min_sleep_time;
	__le32 wakeup_flags;
} __packed;

#define IWLAGN_WOWLAN_MIN_PATTERN_LEN	16
#define IWLAGN_WOWLAN_MAX_PATTERN_LEN	128

struct iwlagn_wowlan_pattern {
	u8 mask[IWLAGN_WOWLAN_MAX_PATTERN_LEN / 8];
	u8 pattern[IWLAGN_WOWLAN_MAX_PATTERN_LEN];
	u8 mask_size;
	u8 pattern_size;
	__le16 reserved;
} __packed;

#define IWLAGN_WOWLAN_MAX_PATTERNS	20

struct iwlagn_wowlan_patterns_cmd {
	__le32 n_patterns;
	struct iwlagn_wowlan_pattern patterns[];
} __packed;

enum iwlagn_wowlan_wakeup_filters {
	IWLAGN_WOWLAN_WAKEUP_MAGIC_PACKET	= BIT(0),
	IWLAGN_WOWLAN_WAKEUP_PATTERN_MATCH	= BIT(1),
	IWLAGN_WOWLAN_WAKEUP_BEACON_MISS	= BIT(2),
	IWLAGN_WOWLAN_WAKEUP_LINK_CHANGE	= BIT(3),
	IWLAGN_WOWLAN_WAKEUP_GTK_REKEY_FAIL	= BIT(4),
	IWLAGN_WOWLAN_WAKEUP_EAP_IDENT_REQ	= BIT(5),
	IWLAGN_WOWLAN_WAKEUP_4WAY_HANDSHAKE	= BIT(6),
	IWLAGN_WOWLAN_WAKEUP_ALWAYS		= BIT(7),
	IWLAGN_WOWLAN_WAKEUP_ENABLE_NET_DETECT	= BIT(8),
};

struct iwlagn_wowlan_wakeup_filter_cmd {
	__le32 enabled;
	__le16 non_qos_seq;
	__le16 reserved;
	__le16 qos_seq[8];
};

#define IWLAGN_NUM_RSC	16

struct tkip_sc {
	__le16 iv16;
	__le16 pad;
	__le32 iv32;
} __packed;

struct iwlagn_tkip_rsc_tsc {
	struct tkip_sc unicast_rsc[IWLAGN_NUM_RSC];
	struct tkip_sc multicast_rsc[IWLAGN_NUM_RSC];
	struct tkip_sc tsc;
} __packed;

struct aes_sc {
	__le64 pn;
} __packed;

struct iwlagn_aes_rsc_tsc {
	struct aes_sc unicast_rsc[IWLAGN_NUM_RSC];
	struct aes_sc multicast_rsc[IWLAGN_NUM_RSC];
	struct aes_sc tsc;
} __packed;

union iwlagn_all_tsc_rsc {
	struct iwlagn_tkip_rsc_tsc tkip;
	struct iwlagn_aes_rsc_tsc aes;
};

struct iwlagn_wowlan_rsc_tsc_params_cmd {
	union iwlagn_all_tsc_rsc all_tsc_rsc;
} __packed;

#define IWLAGN_MIC_KEY_SIZE	8
#define IWLAGN_P1K_SIZE		5
struct iwlagn_mic_keys {
	u8 tx[IWLAGN_MIC_KEY_SIZE];
	u8 rx_unicast[IWLAGN_MIC_KEY_SIZE];
	u8 rx_mcast[IWLAGN_MIC_KEY_SIZE];
} __packed;

struct iwlagn_p1k_cache {
	__le16 p1k[IWLAGN_P1K_SIZE];
} __packed;

#define IWLAGN_NUM_RX_P1K_CACHE	2

struct iwlagn_wowlan_tkip_params_cmd {
	struct iwlagn_mic_keys mic_keys;
	struct iwlagn_p1k_cache tx;
	struct iwlagn_p1k_cache rx_uni[IWLAGN_NUM_RX_P1K_CACHE];
	struct iwlagn_p1k_cache rx_multi[IWLAGN_NUM_RX_P1K_CACHE];
} __packed;


#define IWLAGN_KCK_MAX_SIZE	32
#define IWLAGN_KEK_MAX_SIZE	32

struct iwlagn_wowlan_kek_kck_material_cmd {
	u8	kck[IWLAGN_KCK_MAX_SIZE];
	u8	kek[IWLAGN_KEK_MAX_SIZE];
	__le16	kck_len;
	__le16	kek_len;
	__le64	replay_ctr;
} __packed;


#define IWL_MIN_SLOT_TIME	20

struct iwl_wipan_slot {
	__le16 width;
	u8 type;
	u8 reserved;
} __packed;

#define IWL_WIPAN_PARAMS_FLG_LEAVE_CHANNEL_CTS		BIT(1)	
#define IWL_WIPAN_PARAMS_FLG_LEAVE_CHANNEL_QUIET	BIT(2)	
#define IWL_WIPAN_PARAMS_FLG_SLOTTED_MODE		BIT(3)	
#define IWL_WIPAN_PARAMS_FLG_FILTER_BEACON_NOTIF	BIT(4)
#define IWL_WIPAN_PARAMS_FLG_FULL_SLOTTED_MODE		BIT(5)

struct iwl_wipan_params_cmd {
	__le16 flags;
	u8 reserved;
	u8 num_slots;
	struct iwl_wipan_slot slots[10];
} __packed;


struct iwl_wipan_p2p_channel_switch_cmd {
	__le16 channel;
	__le16 reserved;
};


struct iwl_wipan_noa_descriptor {
	u8 count;
	__le32 duration;
	__le32 interval;
	__le32 starttime;
} __packed;

struct iwl_wipan_noa_attribute {
	u8 id;
	__le16 length;
	u8 index;
	u8 ct_window;
	struct iwl_wipan_noa_descriptor descr0, descr1;
	u8 reserved;
} __packed;

struct iwl_wipan_noa_notification {
	u32 noa_active;
	struct iwl_wipan_noa_attribute noa_attribute;
} __packed;

#endif				
