/******************************************************************************
 *
 * Copyright(c) 2003 - 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/

#ifndef __iwl_dev_h__
#define __iwl_dev_h__

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include "iwl-eeprom.h"
#include "iwl-csr.h"
#include "iwl-debug.h"
#include "iwl-agn-hw.h"
#include "iwl-led.h"
#include "iwl-power.h"
#include "iwl-agn-rs.h"
#include "iwl-agn-tt.h"
#include "iwl-trans.h"
#include "iwl-shared.h"
#include "iwl-op-mode.h"
#include "iwl-notif-wait.h"

struct iwl_tx_queue;

#define CT_KILL_THRESHOLD_LEGACY   110 
#define CT_KILL_THRESHOLD	   114 
#define CT_KILL_EXIT_THRESHOLD     95  

#define IWL_NOISE_MEAS_NOT_AVAILABLE (-127)

#define DEFAULT_RTS_THRESHOLD     2347U
#define MIN_RTS_THRESHOLD         0U
#define MAX_RTS_THRESHOLD         2347U
#define MAX_MSDU_SIZE		  2304U
#define MAX_MPDU_SIZE		  2346U
#define DEFAULT_BEACON_INTERVAL   200U
#define	DEFAULT_SHORT_RETRY_LIMIT 7U
#define	DEFAULT_LONG_RETRY_LIMIT  4U

#define IWL_NUM_SCAN_RATES         (2)

struct iwl_channel_info {
	struct iwl_eeprom_channel eeprom;	
	struct iwl_eeprom_channel ht40_eeprom;	

	u8 channel;	  
	u8 flags;	  
	s8 max_power_avg; 
	s8 curr_txpow;	  
	s8 min_power;	  
	s8 scan_power;	  

	u8 group_index;	  
	u8 band_index;	  
	enum ieee80211_band band;

	
	s8 ht40_max_power_avg;	
	u8 ht40_flags;		
	u8 ht40_extension_channel; 
};

#define IWL_MIN_NUM_QUEUES	11

#define IWL_DEFAULT_CMD_QUEUE_NUM	4
#define IWL_IPAN_CMD_QUEUE_NUM		9

#define IEEE80211_DATA_LEN              2304
#define IEEE80211_4ADDR_LEN             30
#define IEEE80211_HLEN                  (IEEE80211_4ADDR_LEN)
#define IEEE80211_FRAME_LEN             (IEEE80211_DATA_LEN + IEEE80211_HLEN)

#define SUP_RATE_11A_MAX_NUM_CHANNELS  8
#define SUP_RATE_11B_MAX_NUM_CHANNELS  4
#define SUP_RATE_11G_MAX_NUM_CHANNELS  12

#define IWL_SUPPORTED_RATES_IE_LEN         8

#define IWL_INVALID_RATE     0xFF
#define IWL_INVALID_VALUE    -1

union iwl_ht_rate_supp {
	u16 rates;
	struct {
		u8 siso_rate;
		u8 mimo_rate;
	};
};

#define CFG_HT_RX_AMPDU_FACTOR_8K   (0x0)
#define CFG_HT_RX_AMPDU_FACTOR_16K  (0x1)
#define CFG_HT_RX_AMPDU_FACTOR_32K  (0x2)
#define CFG_HT_RX_AMPDU_FACTOR_64K  (0x3)
#define CFG_HT_RX_AMPDU_FACTOR_DEF  CFG_HT_RX_AMPDU_FACTOR_64K
#define CFG_HT_RX_AMPDU_FACTOR_MAX  CFG_HT_RX_AMPDU_FACTOR_64K
#define CFG_HT_RX_AMPDU_FACTOR_MIN  CFG_HT_RX_AMPDU_FACTOR_8K

#define CFG_HT_MPDU_DENSITY_2USEC   (0x4)
#define CFG_HT_MPDU_DENSITY_4USEC   (0x5)
#define CFG_HT_MPDU_DENSITY_8USEC   (0x6)
#define CFG_HT_MPDU_DENSITY_16USEC  (0x7)
#define CFG_HT_MPDU_DENSITY_DEF CFG_HT_MPDU_DENSITY_4USEC
#define CFG_HT_MPDU_DENSITY_MAX CFG_HT_MPDU_DENSITY_16USEC
#define CFG_HT_MPDU_DENSITY_MIN     (0x1)

struct iwl_ht_config {
	bool single_chain_sufficient;
	enum ieee80211_smps_mode smps; 
};

struct iwl_qos_info {
	int qos_active;
	struct iwl_qosparam_cmd def_qos_parm;
};

enum iwl_agg_state {
	IWL_AGG_OFF = 0,
	IWL_AGG_ON,
	IWL_EMPTYING_HW_QUEUE_ADDBA,
	IWL_EMPTYING_HW_QUEUE_DELBA,
};

struct iwl_ht_agg {
	u32 rate_n_flags;
	enum iwl_agg_state state;
	u16 txq_id;
	u16 ssn;
	bool wait_for_ba;
};

struct iwl_tid_data {
	u16 seq_number;
	u16 next_reclaimed;
	struct iwl_ht_agg agg;
};

struct iwl_station_entry {
	struct iwl_addsta_cmd sta;
	u8 used, ctxid;
	struct iwl_link_quality_cmd *lq;
};

struct iwl_station_priv {
	struct iwl_rxon_context *ctx;
	struct iwl_lq_sta lq_sta;
	atomic_t pending_frames;
	bool client;
	bool asleep;
	u8 max_agg_bufsize;
	u8 sta_id;
};

struct iwl_vif_priv {
	struct iwl_rxon_context *ctx;
	u8 ibss_bssid_sta_id;
};

struct iwl_sensitivity_ranges {
	u16 min_nrg_cck;

	u16 nrg_th_cck;
	u16 nrg_th_ofdm;

	u16 auto_corr_min_ofdm;
	u16 auto_corr_min_ofdm_mrc;
	u16 auto_corr_min_ofdm_x1;
	u16 auto_corr_min_ofdm_mrc_x1;

	u16 auto_corr_max_ofdm;
	u16 auto_corr_max_ofdm_mrc;
	u16 auto_corr_max_ofdm_x1;
	u16 auto_corr_max_ofdm_mrc_x1;

	u16 auto_corr_max_cck;
	u16 auto_corr_max_cck_mrc;
	u16 auto_corr_min_cck;
	u16 auto_corr_min_cck_mrc;

	u16 barker_corr_th_min;
	u16 barker_corr_th_min_mrc;
	u16 nrg_th_cca;
};


#define KELVIN_TO_CELSIUS(x) ((x)-273)
#define CELSIUS_TO_KELVIN(x) ((x)+273)


extern void iwl_update_chain_flags(struct iwl_priv *priv);
extern const u8 iwl_bcast_addr[ETH_ALEN];

#define IWL_OPERATION_MODE_AUTO     0
#define IWL_OPERATION_MODE_HT_ONLY  1
#define IWL_OPERATION_MODE_MIXED    2
#define IWL_OPERATION_MODE_20MHZ    3

#define TX_POWER_IWL_ILLEGAL_VOLTAGE -10000

#define INITIALIZATION_VALUE		0xFFFF
#define IWL_CAL_NUM_BEACONS		16
#define MAXIMUM_ALLOWED_PATHLOSS	15

#define CHAIN_NOISE_MAX_DELTA_GAIN_CODE 3

#define MAX_FA_OFDM  50
#define MIN_FA_OFDM  5
#define MAX_FA_CCK   50
#define MIN_FA_CCK   5

#define AUTO_CORR_STEP_OFDM       1

#define AUTO_CORR_STEP_CCK     3
#define AUTO_CORR_MAX_TH_CCK   160

#define NRG_DIFF               2
#define NRG_STEP_CCK           2
#define NRG_MARGIN             8
#define MAX_NUMBER_CCK_NO_FA 100

#define AUTO_CORR_CCK_MIN_VAL_DEF    (125)

#define CHAIN_A             0
#define CHAIN_B             1
#define CHAIN_C             2
#define CHAIN_NOISE_DELTA_GAIN_INIT_VAL 4
#define ALL_BAND_FILTER			0xFF00
#define IN_BAND_FILTER			0xFF
#define MIN_AVERAGE_NOISE_MAX_VALUE	0xFFFFFFFF

#define NRG_NUM_PREV_STAT_L     20
#define NUM_RX_CHAINS           3

enum iwlagn_false_alarm_state {
	IWL_FA_TOO_MANY = 0,
	IWL_FA_TOO_FEW = 1,
	IWL_FA_GOOD_RANGE = 2,
};

enum iwlagn_chain_noise_state {
	IWL_CHAIN_NOISE_ALIVE = 0,  
	IWL_CHAIN_NOISE_ACCUMULATE,
	IWL_CHAIN_NOISE_CALIBRATED,
	IWL_CHAIN_NOISE_DONE,
};

struct iwl_sensitivity_data {
	u32 auto_corr_ofdm;
	u32 auto_corr_ofdm_mrc;
	u32 auto_corr_ofdm_x1;
	u32 auto_corr_ofdm_mrc_x1;
	u32 auto_corr_cck;
	u32 auto_corr_cck_mrc;

	u32 last_bad_plcp_cnt_ofdm;
	u32 last_fa_cnt_ofdm;
	u32 last_bad_plcp_cnt_cck;
	u32 last_fa_cnt_cck;

	u32 nrg_curr_state;
	u32 nrg_prev_state;
	u32 nrg_value[10];
	u8  nrg_silence_rssi[NRG_NUM_PREV_STAT_L];
	u32 nrg_silence_ref;
	u32 nrg_energy_idx;
	u32 nrg_silence_idx;
	u32 nrg_th_cck;
	s32 nrg_auto_corr_silence_diff;
	u32 num_in_cck_no_fa;
	u32 nrg_th_ofdm;

	u16 barker_corr_th_min;
	u16 barker_corr_th_min_mrc;
	u16 nrg_th_cca;
};

struct iwl_chain_noise_data {
	u32 active_chains;
	u32 chain_noise_a;
	u32 chain_noise_b;
	u32 chain_noise_c;
	u32 chain_signal_a;
	u32 chain_signal_b;
	u32 chain_signal_c;
	u16 beacon_count;
	u8 disconn_array[NUM_RX_CHAINS];
	u8 delta_gain_code[NUM_RX_CHAINS];
	u8 radio_write;
	u8 state;
};

enum {
	MEASUREMENT_READY = (1 << 0),
	MEASUREMENT_ACTIVE = (1 << 1),
};

enum iwl_nvm_type {
	NVM_DEVICE_TYPE_EEPROM = 0,
	NVM_DEVICE_TYPE_OTP,
};

enum iwl_access_mode {
	IWL_OTP_ACCESS_ABSOLUTE,
	IWL_OTP_ACCESS_RELATIVE,
};

struct reply_tx_error_statistics {
	u32 pp_delay;
	u32 pp_few_bytes;
	u32 pp_bt_prio;
	u32 pp_quiet_period;
	u32 pp_calc_ttak;
	u32 int_crossed_retry;
	u32 short_limit;
	u32 long_limit;
	u32 fifo_underrun;
	u32 drain_flow;
	u32 rfkill_flush;
	u32 life_expire;
	u32 dest_ps;
	u32 host_abort;
	u32 bt_retry;
	u32 sta_invalid;
	u32 frag_drop;
	u32 tid_disable;
	u32 fifo_flush;
	u32 insuff_cf_poll;
	u32 fail_hw_drop;
	u32 sta_color_mismatch;
	u32 unknown;
};

struct reply_agg_tx_error_statistics {
	u32 underrun;
	u32 bt_prio;
	u32 few_bytes;
	u32 abort;
	u32 last_sent_ttl;
	u32 last_sent_try;
	u32 last_sent_bt_kill;
	u32 scd_query;
	u32 bad_crc32;
	u32 response;
	u32 dump_tx;
	u32 delay_tx;
	u32 unknown;
};

enum iwl_mgmt_stats {
	MANAGEMENT_ASSOC_REQ = 0,
	MANAGEMENT_ASSOC_RESP,
	MANAGEMENT_REASSOC_REQ,
	MANAGEMENT_REASSOC_RESP,
	MANAGEMENT_PROBE_REQ,
	MANAGEMENT_PROBE_RESP,
	MANAGEMENT_BEACON,
	MANAGEMENT_ATIM,
	MANAGEMENT_DISASSOC,
	MANAGEMENT_AUTH,
	MANAGEMENT_DEAUTH,
	MANAGEMENT_ACTION,
	MANAGEMENT_MAX,
};
enum iwl_ctrl_stats {
	CONTROL_BACK_REQ =  0,
	CONTROL_BACK,
	CONTROL_PSPOLL,
	CONTROL_RTS,
	CONTROL_CTS,
	CONTROL_ACK,
	CONTROL_CFEND,
	CONTROL_CFENDACK,
	CONTROL_MAX,
};

struct traffic_stats {
#ifdef CONFIG_IWLWIFI_DEBUGFS
	u32 mgmt[MANAGEMENT_MAX];
	u32 ctrl[CONTROL_MAX];
	u32 data_cnt;
	u64 data_bytes;
#endif
};

#define UCODE_TRACE_PERIOD (10)

struct iwl_event_log {
	bool ucode_trace;
	u32 num_wraps;
	u32 next_entry;
	int non_wraps_count;
	int wraps_once_count;
	int wraps_more_count;
};

#define IWL_MAX_PLCP_ERR_THRESHOLD_MIN	(1)
#define IWL_MAX_PLCP_ERR_THRESHOLD_DEF	(50)
#define IWL_MAX_PLCP_ERR_LONG_THRESHOLD_DEF	(100)
#define IWL_MAX_PLCP_ERR_EXT_LONG_THRESHOLD_DEF	(200)
#define IWL_MAX_PLCP_ERR_THRESHOLD_MAX	(255)
#define IWL_MAX_PLCP_ERR_THRESHOLD_DISABLE	(0)

#define IWL_DELAY_NEXT_FORCE_RF_RESET  (HZ*3)
#define IWL_DELAY_NEXT_FORCE_FW_RELOAD (HZ*5)

#define IWL_DEF_WD_TIMEOUT	(2000)
#define IWL_LONG_WD_TIMEOUT	(10000)
#define IWL_MAX_WD_TIMEOUT	(120000)

#define IWL_BT_ANTENNA_COUPLING_THRESHOLD	(35)

#define IWL_MIN_RELOAD_DURATION		1000 
#define IWL_MAX_CONTINUE_RELOAD_CNT	4


enum iwl_reset {
	IWL_RF_RESET = 0,
	IWL_FW_RESET,
	IWL_MAX_FORCE_RESET,
};

struct iwl_force_reset {
	int reset_request_count;
	int reset_success_count;
	int reset_reject_count;
	unsigned long reset_duration;
	unsigned long last_force_reset_jiffies;
};

#define IWLAGN_EXT_BEACON_TIME_POS	22

struct iwl_rxon_context {
	struct ieee80211_vif *vif;

	bool always_active, is_active;

	bool ht_need_multiple_chains;

	enum iwl_rxon_context_id ctxid;

	u32 interface_modes, exclusive_interface_modes;
	u8 unused_devtype, ap_devtype, ibss_devtype, station_devtype;

	const struct iwl_rxon_cmd active;
	struct iwl_rxon_cmd staging;

	struct iwl_rxon_time_cmd timing;

	struct iwl_qos_info qos_data;

	u8 bcast_sta_id, ap_sta_id;

	u8 rxon_cmd, rxon_assoc_cmd, rxon_timing_cmd;
	u8 qos_cmd;
	u8 wep_key_cmd;

	struct iwl_wep_key wep_keys[WEP_KEYS_MAX];
	u8 key_mapping_keys;

	__le32 station_flags;

	int beacon_int;

	struct {
		bool non_gf_sta_present;
		u8 protection;
		bool enabled, is_40mhz;
		u8 extension_chan_offset;
	} ht;
};

enum iwl_scan_type {
	IWL_SCAN_NORMAL,
	IWL_SCAN_RADIO_RESET,
	IWL_SCAN_ROC,
};

#ifdef CONFIG_IWLWIFI_DEVICE_TESTMODE
struct iwl_testmode_trace {
	u32 buff_size;
	u32 total_size;
	u32 num_chunks;
	u8 *cpu_addr;
	u8 *trace_addr;
	dma_addr_t dma_addr;
	bool trace_enabled;
};
struct iwl_testmode_mem {
	u32 buff_size;
	u32 num_chunks;
	u8 *buff_addr;
	bool read_in_progress;
};
#endif

struct iwl_wipan_noa_data {
	struct rcu_head rcu_head;
	u32 length;
	u8 data[];
};

#define IWL_OP_MODE_GET_DVM(_iwl_op_mode) \
	((struct iwl_priv *) ((_iwl_op_mode)->op_mode_specific))

#define IWL_MAC80211_GET_DVM(_hw) \
	((struct iwl_priv *) ((struct iwl_op_mode *) \
	(_hw)->priv)->op_mode_specific)

struct iwl_priv {

	
	struct iwl_shared *shrd;
	const struct iwl_fw *fw;
	unsigned long status;

	spinlock_t sta_lock;
	struct mutex mutex;

	unsigned long transport_queue_stop;
	bool passive_no_rx;

	
	struct ieee80211_hw *hw;
	struct ieee80211_channel *ieee_channels;
	struct ieee80211_rate *ieee_rates;

	struct list_head calib_results;

	struct workqueue_struct *workqueue;

	enum ieee80211_band band;

	void (*pre_rx_handler)(struct iwl_priv *priv,
			       struct iwl_rx_cmd_buffer *rxb);
	int (*rx_handlers[REPLY_MAX])(struct iwl_priv *priv,
				       struct iwl_rx_cmd_buffer *rxb,
				       struct iwl_device_cmd *cmd);

	struct iwl_notif_wait_data notif_wait;

	struct ieee80211_supported_band bands[IEEE80211_NUM_BANDS];

	
	struct iwl_spectrum_notification measure_report;
	u8 measurement_status;

#define IWL_OWNERSHIP_DRIVER	0
#define IWL_OWNERSHIP_TM	1
	u8 ucode_owner;

	
	u32 ucode_beacon_time;
	int missed_beacon_threshold;

	
	u32 ibss_manager;

	
	unsigned long rx_statistics_jiffies;

	
	u32 rx_handlers_stats[REPLY_MAX];

	
	struct iwl_force_reset force_reset[IWL_MAX_FORCE_RESET];

	
	unsigned long reload_jiffies;
	int reload_count;
	bool ucode_loaded;
	bool init_ucode_run;		

	struct iwl_channel_info *channel_info;	
	u8 channel_count;	

	u8 plcp_delta_threshold;

	
	s32 temperature;	
	s32 last_temperature;

	struct iwl_wipan_noa_data __rcu *noa_data;

	
	unsigned long scan_start;
	unsigned long scan_start_tsf;
	void *scan_cmd;
	enum ieee80211_band scan_band;
	struct cfg80211_scan_request *scan_request;
	struct ieee80211_vif *scan_vif;
	enum iwl_scan_type scan_type;
	u8 scan_tx_ant[IEEE80211_NUM_BANDS];
	u8 mgmt_tx_ant;

	
	u8 sta_key_max_num;

	bool new_scan_threshold_behaviour;

	bool wowlan;

	
	struct mac_address addresses[2];

	struct iwl_rxon_context contexts[NUM_IWL_RXON_CTX];

	__le16 switch_channel;

	u16 active_rate;

	u8 start_calib;
	struct iwl_sensitivity_data sensitivity_data;
	struct iwl_chain_noise_data chain_noise_data;
	__le16 sensitivity_tbl[HD_TABLE_SIZE];
	__le16 enhance_sensitivity_tbl[ENHANCE_HD_TABLE_ENTRIES];

	struct iwl_ht_config current_ht_config;

	
	u8 retry_rate;

	int activity_timer_active;

	
	struct traffic_stats tx_stats;
	struct traffic_stats rx_stats;

	struct iwl_power_mgr power_data;
	struct iwl_tt_mgmt thermal_throttle;

	
	int num_stations;
	struct iwl_station_entry stations[IWLAGN_STATION_COUNT];
	unsigned long ucode_key_table;
	struct iwl_tid_data tid_data[IWLAGN_STATION_COUNT][IWL_MAX_TID_COUNT];

	u8 mac80211_registered;

	
	u8 is_open;

	enum nl80211_iftype iw_mode;

	
	u64 timestamp;

	struct {
		__le32 flag;
		struct statistics_general_common common;
		struct statistics_rx_non_phy rx_non_phy;
		struct statistics_rx_phy rx_ofdm;
		struct statistics_rx_ht_phy rx_ofdm_ht;
		struct statistics_rx_phy rx_cck;
		struct statistics_tx tx;
#ifdef CONFIG_IWLWIFI_DEBUGFS
		struct statistics_bt_activity bt_activity;
		__le32 num_bt_kills, accum_num_bt_kills;
#endif
		spinlock_t lock;
	} statistics;
#ifdef CONFIG_IWLWIFI_DEBUGFS
	struct {
		struct statistics_general_common common;
		struct statistics_rx_non_phy rx_non_phy;
		struct statistics_rx_phy rx_ofdm;
		struct statistics_rx_ht_phy rx_ofdm_ht;
		struct statistics_rx_phy rx_cck;
		struct statistics_tx tx;
		struct statistics_bt_activity bt_activity;
	} accum_stats, delta_stats, max_delta_stats;
#endif

	u8 agg_tids_count;

	struct iwl_rx_phy_res last_phy_res;
	bool last_phy_res_valid;

	u8 phy_calib_chain_noise_reset_cmd;
	u8 phy_calib_chain_noise_gain_cmd;

	
	struct reply_tx_error_statistics reply_tx_stats;
	struct reply_agg_tx_error_statistics reply_agg_tx_stats;

	
	struct ieee80211_channel *hw_roc_channel;
	struct delayed_work hw_roc_disable_work;
	enum nl80211_channel_type hw_roc_chantype;
	int hw_roc_duration;
	bool hw_roc_setup, hw_roc_start_notified;

	
	u8 bt_enable_flag;
	u8 bt_status;
	u8 bt_traffic_load, last_bt_traffic_load;
	bool bt_ch_announce;
	bool bt_full_concurrent;
	bool bt_ant_couple_ok;
	__le32 kill_ack_mask;
	__le32 kill_cts_mask;
	__le16 bt_valid;
	u16 bt_on_thresh;
	u16 bt_duration;
	u16 dynamic_frag_thresh;
	u8 bt_ci_compliance;
	struct work_struct bt_traffic_change_work;
	bool bt_enable_pspoll;
	struct iwl_rxon_context *cur_rssi_ctx;
	bool bt_is_sco;

	struct work_struct restart;
	struct work_struct scan_completed;
	struct work_struct abort_scan;

	struct work_struct beacon_update;
	struct iwl_rxon_context *beacon_ctx;
	struct sk_buff *beacon_skb;
	void *beacon_cmd;

	struct work_struct tt_work;
	struct work_struct ct_enter;
	struct work_struct ct_exit;
	struct work_struct start_internal_scan;
	struct work_struct tx_flush;
	struct work_struct bt_full_concurrency;
	struct work_struct bt_runtime_config;

	struct delayed_work scan_check;

	
	s8 tx_power_user_lmt;
	s8 tx_power_device_lmt;
	s8 tx_power_lmt_in_half_dbm; 
	s8 tx_power_next;

#ifdef CONFIG_IWLWIFI_DEBUGFS
	
	u16 tx_traffic_idx;
	u16 rx_traffic_idx;
	u8 *tx_traffic;
	u8 *rx_traffic;
	struct dentry *debugfs_dir;
	u32 dbgfs_sram_offset, dbgfs_sram_len;
	bool disable_ht40;
	void *wowlan_sram;
#endif 

	struct work_struct txpower_work;
	u32 disable_sens_cal;
	u32 disable_chain_noise_cal;
	struct work_struct run_time_calib_work;
	struct timer_list statistics_periodic;
	struct timer_list ucode_trace;
	struct timer_list watchdog;

	struct iwl_event_log event_log;

	struct led_classdev led;
	unsigned long blink_on, blink_off;
	bool led_registered;
#ifdef CONFIG_IWLWIFI_DEVICE_TESTMODE
	struct iwl_testmode_trace testmode_trace;
	struct iwl_testmode_mem testmode_mem;
	u32 tm_fixed_rate;
#endif

	
	u8 kck[NL80211_KCK_LEN], kek[NL80211_KEK_LEN];
	__le64 replay_ctr;
	__le16 last_seq_ctl;
	bool have_rekey_data;
}; 

extern struct kmem_cache *iwl_tx_cmd_pool;
extern struct iwl_mod_params iwlagn_mod_params;

static inline struct iwl_rxon_context *
iwl_rxon_ctx_from_vif(struct ieee80211_vif *vif)
{
	struct iwl_vif_priv *vif_priv = (void *)vif->drv_priv;

	return vif_priv->ctx;
}

#define for_each_context(priv, ctx)				\
	for (ctx = &priv->contexts[IWL_RXON_CTX_BSS];		\
	     ctx < &priv->contexts[NUM_IWL_RXON_CTX]; ctx++)	\
		if (priv->shrd->valid_contexts & BIT(ctx->ctxid))

static inline int iwl_is_associated_ctx(struct iwl_rxon_context *ctx)
{
	return (ctx->active.filter_flags & RXON_FILTER_ASSOC_MSK) ? 1 : 0;
}

static inline int iwl_is_associated(struct iwl_priv *priv,
				    enum iwl_rxon_context_id ctxid)
{
	return iwl_is_associated_ctx(&priv->contexts[ctxid]);
}

static inline int iwl_is_any_associated(struct iwl_priv *priv)
{
	struct iwl_rxon_context *ctx;
	for_each_context(priv, ctx)
		if (iwl_is_associated_ctx(ctx))
			return true;
	return false;
}

static inline int is_channel_valid(const struct iwl_channel_info *ch_info)
{
	if (ch_info == NULL)
		return 0;
	return (ch_info->flags & EEPROM_CHANNEL_VALID) ? 1 : 0;
}

static inline int is_channel_radar(const struct iwl_channel_info *ch_info)
{
	return (ch_info->flags & EEPROM_CHANNEL_RADAR) ? 1 : 0;
}

static inline u8 is_channel_a_band(const struct iwl_channel_info *ch_info)
{
	return ch_info->band == IEEE80211_BAND_5GHZ;
}

static inline u8 is_channel_bg_band(const struct iwl_channel_info *ch_info)
{
	return ch_info->band == IEEE80211_BAND_2GHZ;
}

static inline int is_channel_passive(const struct iwl_channel_info *ch)
{
	return (!(ch->flags & EEPROM_CHANNEL_ACTIVE)) ? 1 : 0;
}

static inline int is_channel_ibss(const struct iwl_channel_info *ch)
{
	return ((ch->flags & EEPROM_CHANNEL_IBSS)) ? 1 : 0;
}

#endif				
