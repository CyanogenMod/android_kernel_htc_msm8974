/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2005 - 2011 Intel Corporation. All rights reserved.
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
 * Copyright(c) 2005 - 2011 Intel Corporation. All rights reserved.
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

#ifndef __il_commands_h__
#define __il_commands_h__

#include <linux/ieee80211.h>

struct il_priv;

#define IL_UCODE_MAJOR(ver)	(((ver) & 0xFF000000) >> 24)
#define IL_UCODE_MINOR(ver)	(((ver) & 0x00FF0000) >> 16)
#define IL_UCODE_API(ver)	(((ver) & 0x0000FF00) >> 8)
#define IL_UCODE_SERIAL(ver)	((ver) & 0x000000FF)

#define IL_CCK_RATES	4
#define IL_OFDM_RATES	8
#define IL_MAX_RATES	(IL_CCK_RATES + IL_OFDM_RATES)

enum {
	N_ALIVE = 0x1,
	N_ERROR = 0x2,

	
	C_RXON = 0x10,
	C_RXON_ASSOC = 0x11,
	C_QOS_PARAM = 0x13,
	C_RXON_TIMING = 0x14,

	
	C_ADD_STA = 0x18,
	C_REM_STA = 0x19,

	
	C_WEPKEY = 0x20,

	
	N_3945_RX = 0x1b,	
	C_TX = 0x1c,
	C_RATE_SCALE = 0x47,	
	C_LEDS = 0x48,
	C_TX_LINK_QUALITY_CMD = 0x4e,	

	
	C_CHANNEL_SWITCH = 0x72,
	N_CHANNEL_SWITCH = 0x73,
	C_SPECTRUM_MEASUREMENT = 0x74,
	N_SPECTRUM_MEASUREMENT = 0x75,

	
	C_POWER_TBL = 0x77,
	N_PM_SLEEP = 0x7A,
	N_PM_DEBUG_STATS = 0x7B,

	
	C_SCAN = 0x80,
	C_SCAN_ABORT = 0x81,
	N_SCAN_START = 0x82,
	N_SCAN_RESULTS = 0x83,
	N_SCAN_COMPLETE = 0x84,

	
	N_BEACON = 0x90,
	C_TX_BEACON = 0x91,

	
	C_TX_PWR_TBL = 0x97,

	
	C_BT_CONFIG = 0x9b,

	
	C_STATS = 0x9c,
	N_STATS = 0x9d,

	
	N_CARD_STATE = 0xa1,

	
	N_MISSED_BEACONS = 0xa2,

	C_CT_KILL_CONFIG = 0xa4,
	C_SENSITIVITY = 0xa8,
	C_PHY_CALIBRATION = 0xb0,
	N_RX_PHY = 0xc0,
	N_RX_MPDU = 0xc1,
	N_RX = 0xc3,
	N_COMPRESSED_BA = 0xc5,

	IL_CN_MAX = 0xff
};


#define IL_CMD_FAILED_MSK 0x40

#define SEQ_TO_QUEUE(s)	(((s) >> 8) & 0x1f)
#define QUEUE_TO_SEQ(q)	(((q) & 0x1f) << 8)
#define SEQ_TO_IDX(s)	((s) & 0xff)
#define IDX_TO_SEQ(i)	((i) & 0xff)
#define SEQ_HUGE_FRAME	cpu_to_le16(0x4000)
#define SEQ_RX_FRAME	cpu_to_le16(0x8000)

struct il_cmd_header {
	u8 cmd;			
	u8 flags;		
	__le16 sequence;

	
	u8 data[0];
} __packed;

struct il3945_tx_power {
	u8 tx_gain;		
	u8 dsp_atten;		
} __packed;

struct il3945_power_per_rate {
	u8 rate;		
	struct il3945_tx_power tpc;
	u8 reserved;
} __packed;

#define RATE_MCS_CODE_MSK 0x7
#define RATE_MCS_SPATIAL_POS 3
#define RATE_MCS_SPATIAL_MSK 0x18
#define RATE_MCS_HT_DUP_POS 5
#define RATE_MCS_HT_DUP_MSK 0x20

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

#define POWER_TBL_NUM_ENTRIES			33
#define POWER_TBL_NUM_HT_OFDM_ENTRIES		32
#define POWER_TBL_CCK_ENTRY			32

#define IL_PWR_NUM_HT_OFDM_ENTRIES		24
#define IL_PWR_CCK_ENTRIES			2

union il4965_tx_power_dual_stream {
	struct {
		u8 radio_tx_gain[2];
		u8 dsp_predis_atten[2];
	} s;
	u32 dw;
};

struct tx_power_dual_stream {
	__le32 dw;
} __packed;

struct il4965_tx_power_db {
	struct tx_power_dual_stream power_tbl[POWER_TBL_NUM_ENTRIES];
} __packed;


#define UCODE_VALID_OK	cpu_to_le32(0x1)
#define INITIALIZE_SUBTYPE    (9)

struct il_init_alive_resp {
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

	
	__le32 voltage;		
	__le32 therm_r1[2];	
	__le32 therm_r2[2];	
	__le32 therm_r3[2];	
	__le32 therm_r4[2];	
	__le32 tx_atten[5][2];	
} __packed;

struct il_alive_resp {
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

struct il_error_resp {
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
#define RXON_FLG_CHANNEL_MODE_LEGACY			\
	cpu_to_le32(CHANNEL_MODE_LEGACY << RXON_FLG_CHANNEL_MODE_POS)
#define RXON_FLG_CHANNEL_MODE_PURE_40			\
	cpu_to_le32(CHANNEL_MODE_PURE_40 << RXON_FLG_CHANNEL_MODE_POS)
#define RXON_FLG_CHANNEL_MODE_MIXED			\
	cpu_to_le32(CHANNEL_MODE_MIXED << RXON_FLG_CHANNEL_MODE_POS)

#define RXON_FLG_SELF_CTS_EN			cpu_to_le32(0x1<<30)

#define RXON_FILTER_PROMISC_MSK         cpu_to_le32(1 << 0)
#define RXON_FILTER_CTL2HOST_MSK        cpu_to_le32(1 << 1)
#define RXON_FILTER_ACCEPT_GRP_MSK      cpu_to_le32(1 << 2)
#define RXON_FILTER_DIS_DECRYPT_MSK     cpu_to_le32(1 << 3)
#define RXON_FILTER_DIS_GRP_DECRYPT_MSK cpu_to_le32(1 << 4)
#define RXON_FILTER_ASSOC_MSK           cpu_to_le32(1 << 5)
#define RXON_FILTER_BCON_AWARE_MSK      cpu_to_le32(1 << 6)


struct il3945_rxon_cmd {
	u8 node_addr[6];
	__le16 reserved1;
	u8 bssid_addr[6];
	__le16 reserved2;
	u8 wlap_bssid_addr[6];
	__le16 reserved3;
	u8 dev_type;
	u8 air_propagation;
	__le16 reserved4;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	__le16 assoc_id;
	__le32 flags;
	__le32 filter_flags;
	__le16 channel;
	__le16 reserved5;
} __packed;

struct il4965_rxon_cmd {
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
} __packed;

struct il_rxon_cmd {
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
	u8 reserved4;
	u8 reserved5;
} __packed;

struct il3945_rxon_assoc_cmd {
	__le32 flags;
	__le32 filter_flags;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	__le16 reserved;
} __packed;

struct il4965_rxon_assoc_cmd {
	__le32 flags;
	__le32 filter_flags;
	u8 ofdm_basic_rates;
	u8 cck_basic_rates;
	u8 ofdm_ht_single_stream_basic_rates;
	u8 ofdm_ht_dual_stream_basic_rates;
	__le16 rx_chain_select_flags;
	__le16 reserved;
} __packed;

#define IL_CONN_MAX_LISTEN_INTERVAL	10
#define IL_MAX_UCODE_BEACON_INTERVAL	4	
#define IL39_MAX_UCODE_BEACON_INTERVAL	1	

struct il_rxon_time_cmd {
	__le64 timestamp;
	__le16 beacon_interval;
	__le16 atim_win;
	__le32 beacon_init_val;
	__le16 listen_interval;
	u8 dtim_period;
	u8 delta_cp_bss_tbtts;
} __packed;

struct il3945_channel_switch_cmd {
	u8 band;
	u8 expect_beacon;
	__le16 channel;
	__le32 rxon_flags;
	__le32 rxon_filter_flags;
	__le32 switch_time;
	struct il3945_power_per_rate power[IL_MAX_RATES];
} __packed;

struct il4965_channel_switch_cmd {
	u8 band;
	u8 expect_beacon;
	__le16 channel;
	__le32 rxon_flags;
	__le32 rxon_filter_flags;
	__le32 switch_time;
	struct il4965_tx_power_db tx_power;
} __packed;

struct il_csa_notification {
	__le16 band;
	__le16 channel;
	__le32 status;		
} __packed;


struct il_ac_qos {
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

struct il_qosparam_cmd {
	__le32 qos_flags;
	struct il_ac_qos ac[AC_NUM];
} __packed;


#define	IL_AP_ID		0
#define	IL_STA_ID		2
#define	IL3945_BROADCAST_ID	24
#define IL3945_STATION_COUNT	25
#define IL4965_BROADCAST_ID	31
#define	IL4965_STATION_COUNT	32

#define	IL_STATION_COUNT	32	
#define	IL_INVALID_STATION	255

#define STA_FLG_TX_RATE_MSK		cpu_to_le32(1 << 2)
#define STA_FLG_PWR_SAVE_MSK		cpu_to_le32(1 << 8)
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
#define STA_KEY_FLG_INVALID	cpu_to_le16(0x0800)
#define STA_KEY_FLG_MAP_KEY_MSK	cpu_to_le16(0x0008)

#define STA_KEY_FLG_KEY_SIZE_MSK	cpu_to_le16(0x1000)
#define STA_KEY_MULTICAST_MSK		cpu_to_le16(0x4000)
#define STA_KEY_MAX_NUM		8

#define	STA_MODIFY_KEY_MASK		0x01
#define	STA_MODIFY_TID_DISABLE_TX	0x02
#define	STA_MODIFY_TX_RATE_MSK		0x04
#define STA_MODIFY_ADDBA_TID_MSK	0x08
#define STA_MODIFY_DELBA_TID_MSK	0x10
#define STA_MODIFY_SLEEP_TX_COUNT_MSK	0x20

#define BUILD_RAxTID(sta_id, tid)	(((sta_id) << 4) + (tid))

struct il4965_keyinfo {
	__le16 key_flags;
	u8 tkip_rx_tsc_byte2;	
	u8 reserved1;
	__le16 tkip_rx_ttak[5];	
	u8 key_offset;
	u8 reserved2;
	u8 key[16];		
} __packed;

struct sta_id_modify {
	u8 addr[ETH_ALEN];
	__le16 reserved1;
	u8 sta_id;
	u8 modify_mask;
	__le16 reserved2;
} __packed;


struct il3945_addsta_cmd {
	u8 mode;		
	u8 reserved[3];
	struct sta_id_modify sta;
	struct il4965_keyinfo key;
	__le32 station_flags;	
	__le32 station_flags_msk;	

	__le16 tid_disable_tx;

	__le16 rate_n_flags;

	u8 add_immediate_ba_tid;

	u8 remove_immediate_ba_tid;

	__le16 add_immediate_ba_ssn;
} __packed;

struct il4965_addsta_cmd {
	u8 mode;		
	u8 reserved[3];
	struct sta_id_modify sta;
	struct il4965_keyinfo key;
	__le32 station_flags;	
	__le32 station_flags_msk;	

	__le16 tid_disable_tx;

	__le16 reserved1;

	u8 add_immediate_ba_tid;

	u8 remove_immediate_ba_tid;

	__le16 add_immediate_ba_ssn;

	__le16 sleep_tx_count;

	__le16 reserved2;
} __packed;

struct il_addsta_cmd {
	u8 mode;		
	u8 reserved[3];
	struct sta_id_modify sta;
	struct il4965_keyinfo key;
	__le32 station_flags;	
	__le32 station_flags_msk;	

	__le16 tid_disable_tx;

	__le16 rate_n_flags;	

	u8 add_immediate_ba_tid;

	u8 remove_immediate_ba_tid;

	__le16 add_immediate_ba_ssn;

	__le16 sleep_tx_count;

	__le16 reserved2;
} __packed;

#define ADD_STA_SUCCESS_MSK		0x1
#define ADD_STA_NO_ROOM_IN_TBL	0x2
#define ADD_STA_NO_BLOCK_ACK_RESOURCE	0x4
#define ADD_STA_MODIFY_NON_EXIST_STA	0x8
struct il_add_sta_resp {
	u8 status;		
} __packed;

#define REM_STA_SUCCESS_MSK              0x1
struct il_rem_sta_resp {
	u8 status;
} __packed;

struct il_rem_sta_cmd {
	u8 num_sta;		
	u8 reserved[3];
	u8 addr[ETH_ALEN];	
	u8 reserved2[2];
} __packed;

#define IL_TX_FIFO_BK_MSK		cpu_to_le32(BIT(0))
#define IL_TX_FIFO_BE_MSK		cpu_to_le32(BIT(1))
#define IL_TX_FIFO_VI_MSK		cpu_to_le32(BIT(2))
#define IL_TX_FIFO_VO_MSK		cpu_to_le32(BIT(3))
#define IL_AGG_TX_QUEUE_MSK		cpu_to_le32(0xffc00)

#define IL_DROP_SINGLE		0
#define IL_DROP_SELECTED	1
#define IL_DROP_ALL		2

struct il_wep_key {
	u8 key_idx;
	u8 key_offset;
	u8 reserved1[2];
	u8 key_size;
	u8 reserved2[3];
	u8 key[16];
} __packed;

struct il_wep_cmd {
	u8 num_keys;
	u8 global_key_type;
	u8 flags;
	u8 reserved;
	struct il_wep_key key[0];
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

struct il3945_rx_frame_stats {
	u8 phy_count;
	u8 id;
	u8 rssi;
	u8 agc;
	__le16 sig_avg;
	__le16 noise_diff;
	u8 payload[0];
} __packed;

struct il3945_rx_frame_hdr {
	__le16 channel;
	__le16 phy_flags;
	u8 reserved1;
	u8 rate;
	__le16 len;
	u8 payload[0];
} __packed;

struct il3945_rx_frame_end {
	__le32 status;
	__le64 timestamp;
	__le32 beacon_timestamp;
} __packed;

struct il3945_rx_frame {
	struct il3945_rx_frame_stats stats;
	struct il3945_rx_frame_hdr hdr;
	struct il3945_rx_frame_end end;
} __packed;

#define IL39_RX_FRAME_SIZE	(4 + sizeof(struct il3945_rx_frame))


#define IL49_RX_RES_PHY_CNT 14
#define IL49_RX_PHY_FLAGS_ANTENNAE_OFFSET	(4)
#define IL49_RX_PHY_FLAGS_ANTENNAE_MASK	(0x70)
#define IL49_AGC_DB_MASK			(0x3f80)	
#define IL49_AGC_DB_POS			(7)
struct il4965_rx_non_cfg_phy {
	__le16 ant_selection;	
	__le16 agc_info;	
	u8 rssi_info[6];	
	u8 pad[0];
} __packed;

struct il_rx_phy_res {
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

struct il_rx_mpdu_res_start {
	__le16 byte_count;
	__le16 reserved;
} __packed;



#define TX_CMD_FLG_RTS_MSK cpu_to_le32(1 << 1)

#define TX_CMD_FLG_CTS_MSK cpu_to_le32(1 << 2)

#define TX_CMD_FLG_ACK_MSK cpu_to_le32(1 << 3)

#define TX_CMD_FLG_STA_RATE_MSK cpu_to_le32(1 << 4)

#define TX_CMD_FLG_IMM_BA_RSP_MASK  cpu_to_le32(1 << 6)

#define TX_CMD_FLG_FULL_TXOP_PROT_MSK cpu_to_le32(1 << 7)

#define TX_CMD_FLG_ANT_SEL_MSK cpu_to_le32(0xf00)
#define TX_CMD_FLG_ANT_A_MSK cpu_to_le32(1 << 8)
#define TX_CMD_FLG_ANT_B_MSK cpu_to_le32(1 << 9)

#define TX_CMD_FLG_SEQ_CTL_MSK cpu_to_le32(1 << 13)

#define TX_CMD_FLG_MORE_FRAG_MSK cpu_to_le32(1 << 14)

#define TX_CMD_FLG_TSF_MSK cpu_to_le32(1 << 16)

#define TX_CMD_FLG_MH_PAD_MSK cpu_to_le32(1 << 20)

#define TX_CMD_FLG_AGG_CCMP_MSK cpu_to_le32(1 << 22)

#define TX_CMD_FLG_DUR_MSK cpu_to_le32(1 << 25)

#define TX_CMD_SEC_WEP		0x01
#define TX_CMD_SEC_CCM		0x02
#define TX_CMD_SEC_TKIP		0x03
#define TX_CMD_SEC_MSK		0x03
#define TX_CMD_SEC_SHIFT	6
#define TX_CMD_SEC_KEY128	0x08

#define WEP_IV_LEN 4
#define WEP_ICV_LEN 4
#define CCMP_MIC_LEN 8
#define TKIP_ICV_LEN 4


struct il3945_tx_cmd {
	__le16 len;

	__le16 next_frame_len;

	__le32 tx_flags;	

	u8 rate;

	
	u8 sta_id;
	u8 tid_tspec;
	u8 sec_ctl;
	u8 key[16];
	union {
		u8 byte[8];
		__le16 word[4];
		__le32 dw[2];
	} tkip_mic;
	__le32 next_frame_info;
	union {
		__le32 life_time;
		__le32 attempt;
	} stop_time;
	u8 supp_rates[2];
	u8 rts_retry_limit;	
	u8 data_retry_limit;	
	union {
		__le16 pm_frame_timeout;
		__le16 attempt_duration;
	} timeout;

	__le16 driver_txop;

	u8 payload[0];
	struct ieee80211_hdr hdr[0];
} __packed;

struct il3945_tx_resp {
	u8 failure_rts;
	u8 failure_frame;
	u8 bt_kill_count;
	u8 rate;
	__le32 wireless_media_time;
	__le32 status;		
} __packed;

struct il_dram_scratch {
	u8 try_cnt;		
	u8 bt_kill_cnt;		
	__le16 reserved;
} __packed;

struct il_tx_cmd {
	__le16 len;

	__le16 next_frame_len;

	__le32 tx_flags;	

	struct il_dram_scratch scratch;

	
	__le32 rate_n_flags;	

	
	u8 sta_id;

	
	u8 sec_ctl;		

	u8 initial_rate_idx;
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
	TX_3945_STATUS_SUCCESS = 0x01,
	TX_3945_STATUS_DIRECT_DONE = 0x02,
	TX_3945_STATUS_FAIL_SHORT_LIMIT = 0x82,
	TX_3945_STATUS_FAIL_LONG_LIMIT = 0x83,
	TX_3945_STATUS_FAIL_FIFO_UNDERRUN = 0x84,
	TX_3945_STATUS_FAIL_MGMNT_ABORT = 0x85,
	TX_3945_STATUS_FAIL_NEXT_FRAG = 0x86,
	TX_3945_STATUS_FAIL_LIFE_EXPIRE = 0x87,
	TX_3945_STATUS_FAIL_DEST_PS = 0x88,
	TX_3945_STATUS_FAIL_ABORTED = 0x89,
	TX_3945_STATUS_FAIL_BT_RETRY = 0x8a,
	TX_3945_STATUS_FAIL_STA_INVALID = 0x8b,
	TX_3945_STATUS_FAIL_FRAG_DROPPED = 0x8c,
	TX_3945_STATUS_FAIL_TID_DISABLE = 0x8d,
	TX_3945_STATUS_FAIL_FRAME_FLUSHED = 0x8e,
	TX_3945_STATUS_FAIL_INSUFFICIENT_CF_POLL = 0x8f,
	TX_3945_STATUS_FAIL_TX_LOCKED = 0x90,
	TX_3945_STATUS_FAIL_NO_BEACON_ON_RADAR = 0x91,
};

enum {
	TX_STATUS_SUCCESS = 0x01,
	TX_STATUS_DIRECT_DONE = 0x02,
	
	TX_STATUS_POSTPONE_DELAY = 0x40,
	TX_STATUS_POSTPONE_FEW_BYTES = 0x41,
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
	AGG_TX_STATE_FEW_BYTES_MSK = 0x04,
	AGG_TX_STATE_ABORT_MSK = 0x08,
	AGG_TX_STATE_LAST_SENT_TTL_MSK = 0x10,
	AGG_TX_STATE_LAST_SENT_TRY_CNT_MSK = 0x20,
	AGG_TX_STATE_SCD_QUERY_MSK = 0x80,
	AGG_TX_STATE_TEST_BAD_CRC32_MSK = 0x100,
	AGG_TX_STATE_RESPONSE_MSK = 0x1ff,
	AGG_TX_STATE_DUMP_TX_MSK = 0x200,
	AGG_TX_STATE_DELAY_TX_MSK = 0x400
};

#define AGG_TX_STATUS_MSK	0x00000fff	
#define AGG_TX_TRY_MSK		0x0000f000	

#define AGG_TX_STATE_LAST_SENT_MSK  (AGG_TX_STATE_LAST_SENT_TTL_MSK | \
				     AGG_TX_STATE_LAST_SENT_TRY_CNT_MSK)

#define AGG_TX_STATE_TRY_CNT_POS 12
#define AGG_TX_STATE_TRY_CNT_MSK 0xf000

#define AGG_TX_STATE_SEQ_NUM_POS 16
#define AGG_TX_STATE_SEQ_NUM_MSK 0xffff0000

struct agg_tx_status {
	__le16 status;
	__le16 sequence;
} __packed;

struct il4965_tx_resp {
	u8 frame_count;		
	u8 bt_kill_count;	
	u8 failure_rts;		
	u8 failure_frame;	

	__le32 rate_n_flags;	

	__le16 wireless_media_time;	

	__le16 reserved;
	__le32 pa_power1;	
	__le32 pa_power2;

	union {
		__le32 status;
		struct agg_tx_status agg_status[0];	
	} u;
} __packed;

struct il_compressed_ba_resp {
	__le32 sta_addr_lo32;
	__le16 sta_addr_hi16;
	__le16 reserved;

	
	u8 sta_id;
	u8 tid;
	__le16 seq_ctl;
	__le64 bitmap;
	__le16 scd_flow;
	__le16 scd_ssn;
} __packed;


struct il3945_txpowertable_cmd {
	u8 band;		
	u8 reserved;
	__le16 channel;
	struct il3945_power_per_rate power[IL_MAX_RATES];
} __packed;

struct il4965_txpowertable_cmd {
	u8 band;		
	u8 reserved;
	__le16 channel;
	struct il4965_tx_power_db tx_power;
} __packed;

struct il3945_rate_scaling_info {
	__le16 rate_n_flags;
	u8 try_cnt;
	u8 next_rate_idx;
} __packed;

struct il3945_rate_scaling_cmd {
	u8 table_id;
	u8 reserved[3];
	struct il3945_rate_scaling_info table[IL_MAX_RATES];
} __packed;

#define  LINK_QUAL_FLAGS_SET_STA_TLC_RTS_MSK	(1 << 0)

#define  LINK_QUAL_AC_NUM AC_NUM

#define  LINK_QUAL_MAX_RETRY_NUM 16

#define  LINK_QUAL_ANT_A_MSK (1 << 0)
#define  LINK_QUAL_ANT_B_MSK (1 << 1)
#define  LINK_QUAL_ANT_MSK   (LINK_QUAL_ANT_A_MSK|LINK_QUAL_ANT_B_MSK)

struct il_link_qual_general_params {
	u8 flags;

	
	u8 mimo_delimiter;

	
	u8 single_stream_ant_msk;	

	
	u8 dual_stream_ant_msk;	

	u8 start_rate_idx[LINK_QUAL_AC_NUM];
} __packed;

#define LINK_QUAL_AGG_TIME_LIMIT_DEF	(4000)	
#define LINK_QUAL_AGG_TIME_LIMIT_MAX	(8000)
#define LINK_QUAL_AGG_TIME_LIMIT_MIN	(100)

#define LINK_QUAL_AGG_DISABLE_START_DEF	(3)
#define LINK_QUAL_AGG_DISABLE_START_MAX	(255)
#define LINK_QUAL_AGG_DISABLE_START_MIN	(0)

#define LINK_QUAL_AGG_FRAME_LIMIT_DEF	(31)
#define LINK_QUAL_AGG_FRAME_LIMIT_MAX	(63)
#define LINK_QUAL_AGG_FRAME_LIMIT_MIN	(0)

struct il_link_qual_agg_params {

	__le16 agg_time_limit;

	u8 agg_dis_start_th;

	u8 agg_frame_cnt_limit;

	__le32 reserved;
} __packed;

struct il_link_quality_cmd {

	
	u8 sta_id;
	u8 reserved1;
	__le16 control;		
	struct il_link_qual_general_params general_params;
	struct il_link_qual_agg_params agg_params;

	struct {
		__le32 rate_n_flags;	
	} rs_table[LINK_QUAL_MAX_RETRY_NUM];
	__le32 reserved2;
} __packed;

#define BT_COEX_DISABLE (0x0)
#define BT_ENABLE_CHANNEL_ANNOUNCE BIT(0)
#define BT_ENABLE_PRIORITY	   BIT(1)

#define BT_COEX_ENABLE  (BT_ENABLE_CHANNEL_ANNOUNCE | BT_ENABLE_PRIORITY)

#define BT_LEAD_TIME_DEF (0x1E)

#define BT_MAX_KILL_DEF (0x5)

struct il_bt_cmd {
	u8 flags;
	u8 lead_time;
	u8 max_kill;
	u8 reserved;
	__le32 kill_ack_mask;
	__le32 kill_cts_mask;
} __packed;


#define MEASUREMENT_FILTER_FLAG (RXON_FILTER_PROMISC_MSK         | \
				 RXON_FILTER_CTL2HOST_MSK        | \
				 RXON_FILTER_ACCEPT_GRP_MSK      | \
				 RXON_FILTER_DIS_DECRYPT_MSK     | \
				 RXON_FILTER_DIS_GRP_DECRYPT_MSK | \
				 RXON_FILTER_ASSOC_MSK           | \
				 RXON_FILTER_BCON_AWARE_MSK)

struct il_measure_channel {
	__le32 duration;	
	u8 channel;		
	u8 type;		
	__le16 reserved;
} __packed;

struct il_spectrum_cmd {
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
	struct il_measure_channel channels[10];
} __packed;

struct il_spectrum_resp {
	u8 token;
	u8 id;			
	__le16 status;		
} __packed;

enum il_measurement_state {
	IL_MEASUREMENT_START = 0,
	IL_MEASUREMENT_STOP = 1,
};

enum il_measurement_status {
	IL_MEASUREMENT_OK = 0,
	IL_MEASUREMENT_CONCURRENT = 1,
	IL_MEASUREMENT_CSA_CONFLICT = 2,
	IL_MEASUREMENT_TGH_CONFLICT = 3,
	
	IL_MEASUREMENT_STOPPED = 6,
	IL_MEASUREMENT_TIMEOUT = 7,
	IL_MEASUREMENT_PERIODIC_FAILED = 8,
};

#define NUM_ELEMENTS_IN_HISTOGRAM 8

struct il_measurement_histogram {
	__le32 ofdm[NUM_ELEMENTS_IN_HISTOGRAM];	
	__le32 cck[NUM_ELEMENTS_IN_HISTOGRAM];	
} __packed;

struct il_measurement_cca_counters {
	__le32 ofdm;
	__le32 cck;
} __packed;

enum il_measure_type {
	IL_MEASURE_BASIC = (1 << 0),
	IL_MEASURE_CHANNEL_LOAD = (1 << 1),
	IL_MEASURE_HISTOGRAM_RPI = (1 << 2),
	IL_MEASURE_HISTOGRAM_NOISE = (1 << 3),
	IL_MEASURE_FRAME = (1 << 4),
	
	IL_MEASURE_IDLE = (1 << 7),
};

struct il_spectrum_notification {
	u8 id;			
	u8 token;
	u8 channel_idx;		
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
	struct il_measurement_histogram histogram;
	__le32 stop_time;	
	__le32 status;		
} __packed;


#define IL_POWER_VEC_SIZE 5

#define IL_POWER_DRIVER_ALLOW_SLEEP_MSK	cpu_to_le16(BIT(0))
#define IL_POWER_PCI_PM_MSK			cpu_to_le16(BIT(3))

struct il3945_powertable_cmd {
	__le16 flags;
	u8 reserved[2];
	__le32 rx_data_timeout;
	__le32 tx_data_timeout;
	__le32 sleep_interval[IL_POWER_VEC_SIZE];
} __packed;

struct il_powertable_cmd {
	__le16 flags;
	u8 keep_alive_seconds;	
	u8 debug_flags;		
	__le32 rx_data_timeout;
	__le32 tx_data_timeout;
	__le32 sleep_interval[IL_POWER_VEC_SIZE];
	__le32 keep_alive_beacons;
} __packed;

struct il_sleep_notification {
	u8 pm_sleep_mode;
	u8 pm_wakeup_src;
	__le16 reserved;
	__le32 sleep_time;
	__le32 tsf_low;
	__le32 bcon_timer;
} __packed;

enum {
	IL_PM_NO_SLEEP = 0,
	IL_PM_SLP_MAC = 1,
	IL_PM_SLP_FULL_MAC_UNASSOCIATE = 2,
	IL_PM_SLP_FULL_MAC_CARD_STATE = 3,
	IL_PM_SLP_PHY = 4,
	IL_PM_SLP_REPENT = 5,
	IL_PM_WAKEUP_BY_TIMER = 6,
	IL_PM_WAKEUP_BY_DRIVER = 7,
	IL_PM_WAKEUP_BY_RFKILL = 8,
	
	IL_PM_NUM_OF_MODES = 12,
};

struct il_card_state_notif {
	__le32 flags;
} __packed;

#define HW_CARD_DISABLED   0x01
#define SW_CARD_DISABLED   0x02
#define CT_CARD_DISABLED   0x04
#define RXON_CARD_DISABLED 0x10

struct il_ct_kill_config {
	__le32 reserved;
	__le32 critical_temperature_M;
	__le32 critical_temperature_R;
} __packed;


#define SCAN_CHANNEL_TYPE_PASSIVE cpu_to_le32(0)
#define SCAN_CHANNEL_TYPE_ACTIVE  cpu_to_le32(1)

struct il3945_scan_channel {
	u8 type;
	u8 channel;		
	struct il3945_tx_power tpc;
	__le16 active_dwell;	
	__le16 passive_dwell;	
} __packed;

#define IL39_SCAN_PROBE_MASK(n) ((BIT(n) | (BIT(n) - BIT(1))))

struct il_scan_channel {
	__le32 type;
	__le16 channel;		
	u8 tx_gain;		
	u8 dsp_atten;		
	__le16 active_dwell;	
	__le16 passive_dwell;	
} __packed;

#define IL_SCAN_PROBE_MASK(n)	cpu_to_le32((BIT(n) | (BIT(n) - BIT(1))))

struct il_ssid_ie {
	u8 id;
	u8 len;
	u8 ssid[32];
} __packed;

#define PROBE_OPTION_MAX_3945		4
#define PROBE_OPTION_MAX		20
#define TX_CMD_LIFE_TIME_INFINITE	cpu_to_le32(0xFFFFFFFF)
#define IL_GOOD_CRC_TH_DISABLED	0
#define IL_GOOD_CRC_TH_DEFAULT		cpu_to_le16(1)
#define IL_GOOD_CRC_TH_NEVER		cpu_to_le16(0xffff)
#define IL_MAX_SCAN_SIZE 1024
#define IL_MAX_CMD_SIZE 4096


struct il3945_scan_cmd {
	__le16 len;
	u8 reserved0;
	u8 channel_count;	
	__le16 quiet_time;	
	__le16 quiet_plcp_th;	
	__le16 good_CRC_th;	
	__le16 reserved1;
	__le32 max_out_time;	
	__le32 suspend_time;	
	__le32 flags;		
	__le32 filter_flags;	

	struct il3945_tx_cmd tx_cmd;

	
	struct il_ssid_ie direct_scan[PROBE_OPTION_MAX_3945];

	u8 data[0];
} __packed;

struct il_scan_cmd {
	__le16 len;
	u8 reserved0;
	u8 channel_count;	
	__le16 quiet_time;	
	__le16 quiet_plcp_th;	
	__le16 good_CRC_th;	
	__le16 rx_chain;	
	__le32 max_out_time;	
	__le32 suspend_time;	
	__le32 flags;		
	__le32 filter_flags;	

	struct il_tx_cmd tx_cmd;

	
	struct il_ssid_ie direct_scan[PROBE_OPTION_MAX];

	u8 data[0];
} __packed;

#define CAN_ABORT_STATUS	cpu_to_le32(0x1)
#define ABORT_STATUS            0x2

struct il_scanreq_notification {
	__le32 status;		
} __packed;

struct il_scanstart_notification {
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

#define IL_PROBE_STATUS_OK		0
#define IL_PROBE_STATUS_TX_FAILED	BIT(0)
#define IL_PROBE_STATUS_FAIL_TTL	BIT(1)
#define IL_PROBE_STATUS_FAIL_BT	BIT(2)

#define NUMBER_OF_STATS 1	
struct il_scanresults_notification {
	u8 channel;
	u8 band;
	u8 probe_status;
	u8 num_probe_not_sent;	
	__le32 tsf_low;
	__le32 tsf_high;
	__le32 stats[NUMBER_OF_STATS];
} __packed;

struct il_scancomplete_notification {
	u8 scanned_channels;
	u8 status;
	u8 last_channel;
	__le32 tsf_low;
	__le32 tsf_high;
} __packed;


enum il_ibss_manager {
	IL_NOT_IBSS_MANAGER = 0,
	IL_IBSS_MANAGER = 1,
};


struct il3945_beacon_notif {
	struct il3945_tx_resp beacon_notify_hdr;
	__le32 low_tsf;
	__le32 high_tsf;
	__le32 ibss_mgr_status;
} __packed;

struct il4965_beacon_notif {
	struct il4965_tx_resp beacon_notify_hdr;
	__le32 low_tsf;
	__le32 high_tsf;
	__le32 ibss_mgr_status;
} __packed;


struct il3945_tx_beacon_cmd {
	struct il3945_tx_cmd tx;
	__le16 tim_idx;
	u8 tim_size;
	u8 reserved1;
	struct ieee80211_hdr frame[0];	
} __packed;

struct il_tx_beacon_cmd {
	struct il_tx_cmd tx;
	__le16 tim_idx;
	u8 tim_size;
	u8 reserved1;
	struct ieee80211_hdr frame[0];	
} __packed;


#define IL_TEMP_CONVERT 260

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


struct iwl39_stats_rx_phy {
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
} __packed;

struct iwl39_stats_rx_non_phy {
	__le32 bogus_cts;	
	__le32 bogus_ack;	
	__le32 non_bssid_frames;	
	__le32 filtered_frames;	
	__le32 non_channel_beacons;	
} __packed;

struct iwl39_stats_rx {
	struct iwl39_stats_rx_phy ofdm;
	struct iwl39_stats_rx_phy cck;
	struct iwl39_stats_rx_non_phy general;
} __packed;

struct iwl39_stats_tx {
	__le32 preamble_cnt;
	__le32 rx_detected_cnt;
	__le32 bt_prio_defer_cnt;
	__le32 bt_prio_kill_cnt;
	__le32 few_bytes_cnt;
	__le32 cts_timeout;
	__le32 ack_timeout;
	__le32 expected_ack_cnt;
	__le32 actual_ack_cnt;
} __packed;

struct stats_dbg {
	__le32 burst_check;
	__le32 burst_count;
	__le32 wait_for_silence_timeout_cnt;
	__le32 reserved[3];
} __packed;

struct iwl39_stats_div {
	__le32 tx_on_a;
	__le32 tx_on_b;
	__le32 exec_time;
	__le32 probe_time;
} __packed;

struct iwl39_stats_general {
	__le32 temperature;
	struct stats_dbg dbg;
	__le32 sleep_time;
	__le32 slots_out;
	__le32 slots_idle;
	__le32 ttl_timestamp;
	struct iwl39_stats_div div;
} __packed;

struct stats_rx_phy {
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

struct stats_rx_ht_phy {
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

struct stats_rx_non_phy {
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

struct stats_rx {
	struct stats_rx_phy ofdm;
	struct stats_rx_phy cck;
	struct stats_rx_non_phy general;
	struct stats_rx_ht_phy ofdm_ht;
} __packed;

struct stats_tx_power {
	u8 ant_a;
	u8 ant_b;
	u8 ant_c;
	u8 reserved;
} __packed;

struct stats_tx_non_phy_agg {
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

struct stats_tx {
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
	struct stats_tx_non_phy_agg agg;

	__le32 reserved1;
} __packed;

struct stats_div {
	__le32 tx_on_a;
	__le32 tx_on_b;
	__le32 exec_time;
	__le32 probe_time;
	__le32 reserved1;
	__le32 reserved2;
} __packed;

struct stats_general_common {
	__le32 temperature;	
	struct stats_dbg dbg;
	__le32 sleep_time;
	__le32 slots_out;
	__le32 slots_idle;
	__le32 ttl_timestamp;
	struct stats_div div;
	__le32 rx_enable_counter;
	__le32 num_of_sos_states;
} __packed;

struct stats_general {
	struct stats_general_common common;
	__le32 reserved2;
	__le32 reserved3;
} __packed;

#define UCODE_STATS_CLEAR_MSK		(0x1 << 0)
#define UCODE_STATS_FREQUENCY_MSK		(0x1 << 1)
#define UCODE_STATS_NARROW_BAND_MSK	(0x1 << 2)

#define IL_STATS_CONF_CLEAR_STATS cpu_to_le32(0x1)	
#define IL_STATS_CONF_DISABLE_NOTIF cpu_to_le32(0x2)	
struct il_stats_cmd {
	__le32 configuration_flags;	
} __packed;

#define STATS_REPLY_FLG_BAND_24G_MSK         cpu_to_le32(0x2)
#define STATS_REPLY_FLG_HT40_MODE_MSK        cpu_to_le32(0x8)

struct il3945_notif_stats {
	__le32 flag;
	struct iwl39_stats_rx rx;
	struct iwl39_stats_tx tx;
	struct iwl39_stats_general general;
} __packed;

struct il_notif_stats {
	__le32 flag;
	struct stats_rx rx;
	struct stats_tx tx;
	struct stats_general general;
} __packed;


#define IL_MISSED_BEACON_THRESHOLD_MIN	(1)
#define IL_MISSED_BEACON_THRESHOLD_DEF	(5)
#define IL_MISSED_BEACON_THRESHOLD_MAX	IL_MISSED_BEACON_THRESHOLD_DEF

struct il_missed_beacon_notif {
	__le32 consecutive_missed_beacons;
	__le32 total_missed_becons;
	__le32 num_expected_beacons;
	__le32 num_recvd_beacons;
} __packed;



#define HD_TBL_SIZE  (11)	
#define HD_MIN_ENERGY_CCK_DET_IDX                 (0)	
#define HD_MIN_ENERGY_OFDM_DET_IDX                (1)
#define HD_AUTO_CORR32_X1_TH_ADD_MIN_IDX          (2)
#define HD_AUTO_CORR32_X1_TH_ADD_MIN_MRC_IDX      (3)
#define HD_AUTO_CORR40_X4_TH_ADD_MIN_MRC_IDX      (4)
#define HD_AUTO_CORR32_X4_TH_ADD_MIN_IDX          (5)
#define HD_AUTO_CORR32_X4_TH_ADD_MIN_MRC_IDX      (6)
#define HD_BARKER_CORR_TH_ADD_MIN_IDX             (7)
#define HD_BARKER_CORR_TH_ADD_MIN_MRC_IDX         (8)
#define HD_AUTO_CORR40_X4_TH_ADD_MIN_IDX          (9)
#define HD_OFDM_ENERGY_TH_IN_IDX                  (10)

#define C_SENSITIVITY_CONTROL_DEFAULT_TBL	cpu_to_le16(0)
#define C_SENSITIVITY_CONTROL_WORK_TBL	cpu_to_le16(1)

struct il_sensitivity_cmd {
	__le16 control;		
	__le16 table[HD_TBL_SIZE];	
} __packed;


#define IL_DEFAULT_STANDARD_PHY_CALIBRATE_TBL_SIZE	18
enum {
	IL_PHY_CALIBRATE_DIFF_GAIN_CMD = 7,
	IL_MAX_STANDARD_PHY_CALIBRATE_TBL_SIZE = 19,
};

#define IL_MAX_PHY_CALIBRATE_TBL_SIZE		(253)

struct il_calib_hdr {
	u8 op_code;
	u8 first_group;
	u8 groups_num;
	u8 data_valid;
} __packed;

struct il_calib_diff_gain_cmd {
	struct il_calib_hdr hdr;
	s8 diff_gain_a;		
	s8 diff_gain_b;
	s8 diff_gain_c;
	u8 reserved1;
} __packed;


struct il_led_cmd {
	__le32 interval;	
	u8 id;			
	u8 off;			
	u8 on;			
	u8 reserved;
} __packed;


#define IL_RX_FRAME_SIZE_MSK	0x00003fff

struct il_rx_pkt {
	__le32 len_n_flags;
	struct il_cmd_header hdr;
	union {
		struct il3945_rx_frame rx_frame;
		struct il3945_tx_resp tx_resp;
		struct il3945_beacon_notif beacon_status;

		struct il_alive_resp alive_frame;
		struct il_spectrum_notification spectrum_notif;
		struct il_csa_notification csa_notif;
		struct il_error_resp err_resp;
		struct il_card_state_notif card_state_notif;
		struct il_add_sta_resp add_sta;
		struct il_rem_sta_resp rem_sta;
		struct il_sleep_notification sleep_notif;
		struct il_spectrum_resp spectrum;
		struct il_notif_stats stats;
		struct il_compressed_ba_resp compressed_ba;
		struct il_missed_beacon_notif missed_beacon;
		__le32 status;
		u8 raw[0];
	} u;
} __packed;

#endif 
