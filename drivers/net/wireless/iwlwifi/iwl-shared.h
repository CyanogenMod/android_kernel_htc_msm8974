/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2012 Intel Corporation. All rights reserved.
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
#ifndef __iwl_shared_h__
#define __iwl_shared_h__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/gfp.h>
#include <net/mac80211.h>

#include "iwl-commands.h"
#include "iwl-fw.h"


struct iwl_priv;
struct iwl_trans;
struct iwl_sensitivity_ranges;
struct iwl_trans_ops;

#define DRV_NAME        "iwlwifi"
#define IWLWIFI_VERSION "in-tree:"
#define DRV_COPYRIGHT	"Copyright(c) 2003-2012 Intel Corporation"
#define DRV_AUTHOR     "<ilw@linux.intel.com>"

extern struct iwl_mod_params iwlagn_mod_params;

#define IWL_DISABLE_HT_ALL	BIT(0)
#define IWL_DISABLE_HT_TXAGG	BIT(1)
#define IWL_DISABLE_HT_RXAGG	BIT(2)

struct iwl_mod_params {
	int sw_crypto;
	unsigned int disable_11n;
	int amsdu_size_8K;
	int antenna;
	int restart_fw;
	bool plcp_check;
	bool ack_check;
	int  wd_disable;
	bool bt_coex_active;
	int led_mode;
	bool no_sleep_autoadjust;
	bool power_save;
	int power_level;
	u32 debug_level;
	int ant_coupling;
	bool bt_ch_announce;
	int wanted_ucode_alternative;
	bool auto_agg;
};

struct iwl_hw_params {
	u8  num_ampdu_queues;
	u8  tx_chains_num;
	u8  rx_chains_num;
	u8  valid_tx_ant;
	u8  valid_rx_ant;
	u8  ht40_channel;
	bool use_rts_for_aggregation;
	u16 sku;
	u32 rx_page_order;
	u32 ct_kill_threshold;
	u32 ct_kill_exit_threshold;
	unsigned int wd_timeout;

	const struct iwl_sensitivity_ranges *sens;
};

enum iwl_led_mode {
	IWL_LED_DEFAULT,
	IWL_LED_RF_STATE,
	IWL_LED_BLINK,
	IWL_LED_DISABLE,
};

struct iwl_base_params {
	int eeprom_size;
	int num_of_queues;	
	int num_of_ampdu_queues;
	
	u32 pll_cfg_val;

	const u16 max_ll_items;
	const bool shadow_ram_support;
	u16 led_compensation;
	bool adv_thermal_throttle;
	bool support_ct_kill_exit;
	const bool support_wimax_coexist;
	u8 plcp_delta_threshold;
	s32 chain_noise_scale;
	unsigned int wd_timeout;
	u32 max_event_log_size;
	const bool shadow_reg_enable;
	const bool hd_v2;
	const bool no_idle_support;
	const bool wd_disable;
};

struct iwl_bt_params {
	bool advanced_bt_coexist;
	u8 bt_init_traffic_load;
	u8 bt_prio_boost;
	u16 agg_time_limit;
	bool bt_sco_disable;
	bool bt_session_2;
};
struct iwl_ht_params {
	const bool ht_greenfield_support; 
	bool use_rts_for_aggregation;
	enum ieee80211_smps_mode smps_mode;
};

struct iwl_cfg {
	
	const char *name;
	const char *fw_name_pre;
	const unsigned int ucode_api_max;
	const unsigned int ucode_api_ok;
	const unsigned int ucode_api_min;
	const u32 max_data_size;
	const u32 max_inst_size;
	u8   valid_tx_ant;
	u8   valid_rx_ant;
	u16  eeprom_ver;
	u16  eeprom_calib_ver;
	const struct iwl_lib_ops *lib;
	void (*additional_nic_config)(struct iwl_priv *priv);
	
	const struct iwl_base_params *base_params;
	
	const struct iwl_ht_params *ht_params;
	const struct iwl_bt_params *bt_params;
	const bool need_temp_offset_calib; 
	const bool no_xtal_calib;
	u8 scan_rx_antennas[IEEE80211_NUM_BANDS];
	enum iwl_led_mode led_mode;
	const bool adv_pm;
	const bool rx_with_siso_diversity;
	const bool internal_wimax_coex;
	const bool iq_invert;
	const bool temp_offset_v2;
};

struct iwl_shared {
	unsigned long status;
	u8 valid_contexts;

	const struct iwl_cfg *cfg;
	struct iwl_trans *trans;
	void *drv;
	struct iwl_hw_params hw_params;
	const struct iwl_fw *fw;

	
	u8 *eeprom;

	
	enum iwl_ucode_type ucode_type;

	struct {
		u32 error_event_table;
		u32 log_event_table;
	} device_pointers;

};

#define cfg(_m)		((_m)->shrd->cfg)
#define trans(_m)	((_m)->shrd->trans)
#define hw_params(_m)	((_m)->shrd->hw_params)

static inline bool iwl_have_debug_level(u32 level)
{
	return iwlagn_mod_params.debug_level & level;
}

enum iwl_rxon_context_id {
	IWL_RXON_CTX_BSS,
	IWL_RXON_CTX_PAN,

	NUM_IWL_RXON_CTX
};

int iwlagn_hw_valid_rtc_data_addr(u32 addr);
const char *get_cmd_string(u8 cmd);

#define IWL_CMD(x) case x: return #x

#define STATUS_HCMD_ACTIVE	0	
#define STATUS_INT_ENABLED	2
#define STATUS_RF_KILL_HW	3
#define STATUS_CT_KILL		4
#define STATUS_INIT		5
#define STATUS_ALIVE		6
#define STATUS_READY		7
#define STATUS_TEMPERATURE	8
#define STATUS_GEO_CONFIGURED	9
#define STATUS_EXIT_PENDING	10
#define STATUS_STATISTICS	12
#define STATUS_SCANNING		13
#define STATUS_SCAN_ABORTING	14
#define STATUS_SCAN_HW		15
#define STATUS_POWER_PMI	16
#define STATUS_FW_ERROR		17
#define STATUS_DEVICE_ENABLED	18
#define STATUS_CHANNEL_SWITCH_PENDING 19
#define STATUS_SCAN_COMPLETE	20

#endif 
