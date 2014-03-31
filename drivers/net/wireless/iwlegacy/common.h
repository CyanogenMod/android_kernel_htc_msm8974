/******************************************************************************
 *
 * Copyright(c) 2003 - 2011 Intel Corporation. All rights reserved.
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
#ifndef __il_core_h__
#define __il_core_h__

#include <linux/interrupt.h>
#include <linux/pci.h>		
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <net/mac80211.h>
#include <net/ieee80211_radiotap.h>

#include "commands.h"
#include "csr.h"
#include "prph.h"

struct il_host_cmd;
struct il_cmd;
struct il_tx_queue;

#define IL_ERR(f, a...) dev_err(&il->pci_dev->dev, f, ## a)
#define IL_WARN(f, a...) dev_warn(&il->pci_dev->dev, f, ## a)
#define IL_INFO(f, a...) dev_info(&il->pci_dev->dev, f, ## a)

#define RX_QUEUE_SIZE                         256
#define RX_QUEUE_MASK                         255
#define RX_QUEUE_SIZE_LOG                     8

#define RX_FREE_BUFFERS 64
#define RX_LOW_WATERMARK 8

#define U32_PAD(n)		((4-(n))&0x3)

#define CT_KILL_THRESHOLD_LEGACY   110	

#define IL_NOISE_MEAS_NOT_AVAILABLE (-127)

#define DEFAULT_RTS_THRESHOLD     2347U
#define MIN_RTS_THRESHOLD         0U
#define MAX_RTS_THRESHOLD         2347U
#define MAX_MSDU_SIZE		  2304U
#define MAX_MPDU_SIZE		  2346U
#define DEFAULT_BEACON_INTERVAL   100U
#define	DEFAULT_SHORT_RETRY_LIMIT 7U
#define	DEFAULT_LONG_RETRY_LIMIT  4U

struct il_rx_buf {
	dma_addr_t page_dma;
	struct page *page;
	struct list_head list;
};

#define rxb_addr(r) page_address(r->page)

struct il_device_cmd;

struct il_cmd_meta {
	
	struct il_host_cmd *source;
	void (*callback) (struct il_priv *il, struct il_device_cmd *cmd,
			  struct il_rx_pkt *pkt);

	u32 flags;

	 DEFINE_DMA_UNMAP_ADDR(mapping);
	 DEFINE_DMA_UNMAP_LEN(len);
};

struct il_queue {
	int n_bd;		
	int write_ptr;		
	int read_ptr;		
	
	dma_addr_t dma_addr;	
	int n_win;		
	u32 id;
	int low_mark;		
	int high_mark;		
};

#define TFD_TX_CMD_SLOTS 256
#define TFD_CMD_SLOTS 32

struct il_tx_queue {
	struct il_queue q;
	void *tfds;
	struct il_device_cmd **cmd;
	struct il_cmd_meta *meta;
	struct sk_buff **skbs;
	unsigned long time_stamp;
	u8 need_update;
	u8 sched_retry;
	u8 active;
	u8 swq_id;
};

#define IL_EEPROM_ACCESS_TIMEOUT	5000	

#define IL_EEPROM_SEM_TIMEOUT		10	
#define IL_EEPROM_SEM_RETRY_LIMIT	1000	

#define IL_NUM_TX_CALIB_GROUPS 5
enum {
	EEPROM_CHANNEL_VALID = (1 << 0),	
	EEPROM_CHANNEL_IBSS = (1 << 1),	
	
	EEPROM_CHANNEL_ACTIVE = (1 << 3),	
	EEPROM_CHANNEL_RADAR = (1 << 4),	
	EEPROM_CHANNEL_WIDE = (1 << 5),	
	
	EEPROM_CHANNEL_DFS = (1 << 7),	
};

#define EEPROM_SKU_CAP_SW_RF_KILL_ENABLE                (1 << 0)
#define EEPROM_SKU_CAP_HW_RF_KILL_ENABLE                (1 << 1)

struct il_eeprom_channel {
	u8 flags;		
	s8 max_power_avg;	
} __packed;

#define EEPROM_3945_EEPROM_VERSION	(0x2f)

#define EEPROM_TX_POWER_TX_CHAINS      (2)

#define EEPROM_TX_POWER_BANDS          (8)

#define EEPROM_TX_POWER_MEASUREMENTS   (3)

#define EEPROM_4965_TX_POWER_VERSION    (5)
#define EEPROM_4965_EEPROM_VERSION	(0x2f)
#define EEPROM_4965_CALIB_VERSION_OFFSET       (2*0xB6)	
#define EEPROM_4965_CALIB_TXPOWER_OFFSET       (2*0xE8)	
#define EEPROM_4965_BOARD_REVISION             (2*0x4F)	
#define EEPROM_4965_BOARD_PBA                  (2*0x56+1)	

extern const u8 il_eeprom_band_1[14];

struct il_eeprom_calib_measure {
	u8 temperature;		
	u8 gain_idx;		
	u8 actual_pow;		
	s8 pa_det;		
} __packed;

struct il_eeprom_calib_ch_info {
	u8 ch_num;
	struct il_eeprom_calib_measure
	    measurements[EEPROM_TX_POWER_TX_CHAINS]
	    [EEPROM_TX_POWER_MEASUREMENTS];
} __packed;

struct il_eeprom_calib_subband_info {
	u8 ch_from;		
	u8 ch_to;		
	struct il_eeprom_calib_ch_info ch1;
	struct il_eeprom_calib_ch_info ch2;
} __packed;

struct il_eeprom_calib_info {
	u8 saturation_power24;	
	u8 saturation_power52;	
	__le16 voltage;		
	struct il_eeprom_calib_subband_info band_info[EEPROM_TX_POWER_BANDS];
} __packed;

#define EEPROM_DEVICE_ID                    (2*0x08)	
#define EEPROM_MAC_ADDRESS                  (2*0x15)	
#define EEPROM_BOARD_REVISION               (2*0x35)	
#define EEPROM_BOARD_PBA_NUMBER             (2*0x3B+1)	
#define EEPROM_VERSION                      (2*0x44)	
#define EEPROM_SKU_CAP                      (2*0x45)	
#define EEPROM_OEM_MODE                     (2*0x46)	
#define EEPROM_WOWLAN_MODE                  (2*0x47)	
#define EEPROM_RADIO_CONFIG                 (2*0x48)	
#define EEPROM_NUM_MAC_ADDRESS              (2*0x4C)	

#define EEPROM_RF_CFG_TYPE_MSK(x)   (x & 0x3)	
#define EEPROM_RF_CFG_STEP_MSK(x)   ((x >> 2)  & 0x3)	
#define EEPROM_RF_CFG_DASH_MSK(x)   ((x >> 4)  & 0x3)	
#define EEPROM_RF_CFG_PNUM_MSK(x)   ((x >> 6)  & 0x3)	
#define EEPROM_RF_CFG_TX_ANT_MSK(x) ((x >> 8)  & 0xF)	
#define EEPROM_RF_CFG_RX_ANT_MSK(x) ((x >> 12) & 0xF)	

#define EEPROM_3945_RF_CFG_TYPE_MAX  0x0
#define EEPROM_4965_RF_CFG_TYPE_MAX  0x1

#define EEPROM_REGULATORY_SKU_ID            (2*0x60)	
#define EEPROM_REGULATORY_BAND_1            (2*0x62)	
#define EEPROM_REGULATORY_BAND_1_CHANNELS   (2*0x63)	

#define EEPROM_REGULATORY_BAND_2            (2*0x71)	
#define EEPROM_REGULATORY_BAND_2_CHANNELS   (2*0x72)	

#define EEPROM_REGULATORY_BAND_3            (2*0x7F)	
#define EEPROM_REGULATORY_BAND_3_CHANNELS   (2*0x80)	

#define EEPROM_REGULATORY_BAND_4            (2*0x8C)	
#define EEPROM_REGULATORY_BAND_4_CHANNELS   (2*0x8D)	

#define EEPROM_REGULATORY_BAND_5            (2*0x98)	
#define EEPROM_REGULATORY_BAND_5_CHANNELS   (2*0x99)	

#define EEPROM_4965_REGULATORY_BAND_24_HT40_CHANNELS (2*0xA0)	

#define EEPROM_4965_REGULATORY_BAND_52_HT40_CHANNELS (2*0xA8)	

#define EEPROM_REGULATORY_BAND_NO_HT40			(0)

int il_eeprom_init(struct il_priv *il);
void il_eeprom_free(struct il_priv *il);
const u8 *il_eeprom_query_addr(const struct il_priv *il, size_t offset);
u16 il_eeprom_query16(const struct il_priv *il, size_t offset);
int il_init_channel_map(struct il_priv *il);
void il_free_channel_map(struct il_priv *il);
const struct il_channel_info *il_get_channel_info(const struct il_priv *il,
						  enum ieee80211_band band,
						  u16 channel);

#define IL_NUM_SCAN_RATES         (2)

struct il4965_channel_tgd_info {
	u8 type;
	s8 max_power;
};

struct il4965_channel_tgh_info {
	s64 last_radar_time;
};

#define IL4965_MAX_RATE (33)

struct il3945_clip_group {
	const s8 clip_powers[IL_MAX_RATES];
};

struct il3945_channel_power_info {
	struct il3945_tx_power tpc;	
	s8 power_table_idx;	
	s8 base_power_idx;	
	s8 requested_power;	
};

struct il3945_scan_power_info {
	struct il3945_tx_power tpc;	
	s8 power_table_idx;	
	s8 requested_power;	
};

struct il_channel_info {
	struct il4965_channel_tgd_info tgd;
	struct il4965_channel_tgh_info tgh;
	struct il_eeprom_channel eeprom;	
	struct il_eeprom_channel ht40_eeprom;	

	u8 channel;		
	u8 flags;		
	s8 max_power_avg;	
	s8 curr_txpow;		
	s8 min_power;		
	s8 scan_power;		

	u8 group_idx;		
	u8 band_idx;		
	enum ieee80211_band band;

	
	s8 ht40_max_power_avg;	
	u8 ht40_flags;		
	u8 ht40_extension_channel;	

	struct il3945_channel_power_info power_info[IL4965_MAX_RATE];

	
	struct il3945_scan_power_info scan_pwr_info[IL_NUM_SCAN_RATES];
};

#define IL_TX_FIFO_BK		0	
#define IL_TX_FIFO_BE		1
#define IL_TX_FIFO_VI		2	
#define IL_TX_FIFO_VO		3
#define IL_TX_FIFO_UNUSED	-1

#define IL_MIN_NUM_QUEUES	10

#define IL_DEFAULT_CMD_QUEUE_NUM	4

#define IEEE80211_DATA_LEN              2304
#define IEEE80211_4ADDR_LEN             30
#define IEEE80211_HLEN                  (IEEE80211_4ADDR_LEN)
#define IEEE80211_FRAME_LEN             (IEEE80211_DATA_LEN + IEEE80211_HLEN)

struct il_frame {
	union {
		struct ieee80211_hdr frame;
		struct il_tx_beacon_cmd beacon;
		u8 raw[IEEE80211_FRAME_LEN];
		u8 cmd[360];
	} u;
	struct list_head list;
};

#define SEQ_TO_SN(seq) (((seq) & IEEE80211_SCTL_SEQ) >> 4)
#define SN_TO_SEQ(ssn) (((ssn) << 4) & IEEE80211_SCTL_SEQ)
#define MAX_SN ((IEEE80211_SCTL_SEQ) >> 4)

enum {
	CMD_SYNC = 0,
	CMD_SIZE_NORMAL = 0,
	CMD_NO_SKB = 0,
	CMD_SIZE_HUGE = (1 << 0),
	CMD_ASYNC = (1 << 1),
	CMD_WANT_SKB = (1 << 2),
	CMD_MAPPED = (1 << 3),
};

#define DEF_CMD_PAYLOAD_SIZE 320

struct il_device_cmd {
	struct il_cmd_header hdr;	
	union {
		u32 flags;
		u8 val8;
		u16 val16;
		u32 val32;
		struct il_tx_cmd tx;
		u8 payload[DEF_CMD_PAYLOAD_SIZE];
	} __packed cmd;
} __packed;

#define TFD_MAX_PAYLOAD_SIZE (sizeof(struct il_device_cmd))

struct il_host_cmd {
	const void *data;
	unsigned long reply_page;
	void (*callback) (struct il_priv *il, struct il_device_cmd *cmd,
			  struct il_rx_pkt *pkt);
	u32 flags;
	u16 len;
	u8 id;
};

#define SUP_RATE_11A_MAX_NUM_CHANNELS  8
#define SUP_RATE_11B_MAX_NUM_CHANNELS  4
#define SUP_RATE_11G_MAX_NUM_CHANNELS  12

/**
 * struct il_rx_queue - Rx queue
 * @bd: driver's pointer to buffer of receive buffer descriptors (rbd)
 * @bd_dma: bus address of buffer of receive buffer descriptors (rbd)
 * @read: Shared idx to newest available Rx buffer
 * @write: Shared idx to oldest written Rx packet
 * @free_count: Number of pre-allocated buffers in rx_free
 * @rx_free: list of free SKBs for use
 * @rx_used: List of Rx buffers with no SKB
 * @need_update: flag to indicate we need to update read/write idx
 * @rb_stts: driver's pointer to receive buffer status
 * @rb_stts_dma: bus address of receive buffer status
 *
 * NOTE:  rx_free and rx_used are used as a FIFO for il_rx_bufs
 */
struct il_rx_queue {
	__le32 *bd;
	dma_addr_t bd_dma;
	struct il_rx_buf pool[RX_QUEUE_SIZE + RX_FREE_BUFFERS];
	struct il_rx_buf *queue[RX_QUEUE_SIZE];
	u32 read;
	u32 write;
	u32 free_count;
	u32 write_actual;
	struct list_head rx_free;
	struct list_head rx_used;
	int need_update;
	struct il_rb_status *rb_stts;
	dma_addr_t rb_stts_dma;
	spinlock_t lock;
};

#define IL_SUPPORTED_RATES_IE_LEN         8

#define MAX_TID_COUNT        9

#define IL_INVALID_RATE     0xFF
#define IL_INVALID_VALUE    -1

struct il_ht_agg {
	u16 txq_id;
	u16 frame_count;
	u16 wait_for_ba;
	u16 start_idx;
	u64 bitmap;
	u32 rate_n_flags;
#define IL_AGG_OFF 0
#define IL_AGG_ON 1
#define IL_EMPTYING_HW_QUEUE_ADDBA 2
#define IL_EMPTYING_HW_QUEUE_DELBA 3
	u8 state;
};

struct il_tid_data {
	u16 seq_number;		
	u16 tfds_in_queue;
	struct il_ht_agg agg;
};

struct il_hw_key {
	u32 cipher;
	int keylen;
	u8 keyidx;
	u8 key[32];
};

union il_ht_rate_supp {
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

struct il_ht_config {
	bool single_chain_sufficient;
	enum ieee80211_smps_mode smps;	
};

struct il_qos_info {
	int qos_active;
	struct il_qosparam_cmd def_qos_parm;
};

struct il_station_entry {
	struct il_addsta_cmd sta;
	struct il_tid_data tid[MAX_TID_COUNT];
	u8 used;
	struct il_hw_key keyinfo;
	struct il_link_quality_cmd *lq;
};

struct il_station_priv_common {
	u8 sta_id;
};

struct il_vif_priv {
	u8 ibss_bssid_sta_id;
};

struct fw_desc {
	void *v_addr;		
	dma_addr_t p_addr;	
	u32 len;		
};

struct il_ucode_header {
	__le32 ver;		
	struct {
		__le32 inst_size;	
		__le32 data_size;	
		__le32 init_size;	
		__le32 init_data_size;	
		__le32 boot_size;	
		u8 data[0];	
	} v1;
};

struct il4965_ibss_seq {
	u8 mac[ETH_ALEN];
	u16 seq_num;
	u16 frag_num;
	unsigned long packet_time;
	struct list_head list;
};

struct il_sensitivity_ranges {
	u16 min_nrg_cck;
	u16 max_nrg_cck;

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

struct il_hw_params {
	u8 bcast_id;
	u8 max_txq_num;
	u8 dma_chnl_num;
	u16 scd_bc_tbls_size;
	u32 tfd_size;
	u8 tx_chains_num;
	u8 rx_chains_num;
	u8 valid_tx_ant;
	u8 valid_rx_ant;
	u16 max_rxq_size;
	u16 max_rxq_log;
	u32 rx_page_order;
	u32 rx_wrt_ptr_reg;
	u8 max_stations;
	u8 ht40_channel;
	u8 max_beacon_itrvl;	
	u32 max_inst_size;
	u32 max_data_size;
	u32 max_bsm_size;
	u32 ct_kill_threshold;	
	u16 beacon_time_tsf_bits;
	const struct il_sensitivity_ranges *sens;
};

extern void il4965_update_chain_flags(struct il_priv *il);
extern const u8 il_bcast_addr[ETH_ALEN];
extern int il_queue_space(const struct il_queue *q);
static inline int
il_queue_used(const struct il_queue *q, int i)
{
	return q->write_ptr >= q->read_ptr ? (i >= q->read_ptr &&
					      i < q->write_ptr) : !(i <
								    q->read_ptr
								    && i >=
								    q->
								    write_ptr);
}

static inline u8
il_get_cmd_idx(struct il_queue *q, u32 idx, int is_huge)
{
	if (is_huge)
		return q->n_win;	

	
	return idx & (q->n_win - 1);
}

struct il_dma_ptr {
	dma_addr_t dma;
	void *addr;
	size_t size;
};

#define IL_OPERATION_MODE_AUTO     0
#define IL_OPERATION_MODE_HT_ONLY  1
#define IL_OPERATION_MODE_MIXED    2
#define IL_OPERATION_MODE_20MHZ    3

#define IL_TX_CRC_SIZE 4
#define IL_TX_DELIMITER_SIZE 4

#define TX_POWER_IL_ILLEGAL_VOLTAGE -10000

#define INITIALIZATION_VALUE		0xFFFF
#define IL4965_CAL_NUM_BEACONS		20
#define IL_CAL_NUM_BEACONS		16
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

enum il4965_false_alarm_state {
	IL_FA_TOO_MANY = 0,
	IL_FA_TOO_FEW = 1,
	IL_FA_GOOD_RANGE = 2,
};

enum il4965_chain_noise_state {
	IL_CHAIN_NOISE_ALIVE = 0,	
	IL_CHAIN_NOISE_ACCUMULATE,
	IL_CHAIN_NOISE_CALIBRATED,
	IL_CHAIN_NOISE_DONE,
};

enum ucode_type {
	UCODE_NONE = 0,
	UCODE_INIT,
	UCODE_RT
};

struct il_sensitivity_data {
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
	u8 nrg_silence_rssi[NRG_NUM_PREV_STAT_L];
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

struct il_chain_noise_data {
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

#define	EEPROM_SEM_TIMEOUT 10	
#define EEPROM_SEM_RETRY_LIMIT 1000	

#define IL_TRAFFIC_ENTRIES	(256)
#define IL_TRAFFIC_ENTRY_SIZE  (64)

enum {
	MEASUREMENT_READY = (1 << 0),
	MEASUREMENT_ACTIVE = (1 << 1),
};

struct isr_stats {
	u32 hw;
	u32 sw;
	u32 err_code;
	u32 sch;
	u32 alive;
	u32 rfkill;
	u32 ctkill;
	u32 wakeup;
	u32 rx;
	u32 handlers[IL_CN_MAX];
	u32 tx;
	u32 unhandled;
};

enum il_mgmt_stats {
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
enum il_ctrl_stats {
	CONTROL_BACK_REQ = 0,
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
#ifdef CONFIG_IWLEGACY_DEBUGFS
	u32 mgmt[MANAGEMENT_MAX];
	u32 ctrl[CONTROL_MAX];
	u32 data_cnt;
	u64 data_bytes;
#endif
};

#define IL_HOST_INT_TIMEOUT_MAX	(0xFF)
#define IL_HOST_INT_TIMEOUT_DEF	(0x40)
#define IL_HOST_INT_TIMEOUT_MIN	(0x0)
#define IL_HOST_INT_CALIB_TIMEOUT_MAX	(0xFF)
#define IL_HOST_INT_CALIB_TIMEOUT_DEF	(0x10)
#define IL_HOST_INT_CALIB_TIMEOUT_MIN	(0x0)

#define IL_DELAY_NEXT_FORCE_FW_RELOAD (HZ*5)

#define IL_DEF_WD_TIMEOUT	(2000)
#define IL_LONG_WD_TIMEOUT	(10000)
#define IL_MAX_WD_TIMEOUT	(120000)

struct il_force_reset {
	int reset_request_count;
	int reset_success_count;
	int reset_reject_count;
	unsigned long reset_duration;
	unsigned long last_force_reset_jiffies;
};

#define IL3945_EXT_BEACON_TIME_POS	24
#define IL4965_EXT_BEACON_TIME_POS	22

struct il_rxon_context {
	struct ieee80211_vif *vif;
};

struct il_power_mgr {
	struct il_powertable_cmd sleep_cmd;
	struct il_powertable_cmd sleep_cmd_next;
	int debug_sleep_level_override;
	bool pci_pm;
};

struct il_priv {
	struct ieee80211_hw *hw;
	struct ieee80211_channel *ieee_channels;
	struct ieee80211_rate *ieee_rates;

	struct il_cfg *cfg;
	const struct il_ops *ops;
#ifdef CONFIG_IWLEGACY_DEBUGFS
	const struct il_debugfs_ops *debugfs_ops;
#endif

	
	struct list_head free_frames;
	int frames_count;

	enum ieee80211_band band;
	int alloc_rxb_page;

	void (*handlers[IL_CN_MAX]) (struct il_priv *il,
				     struct il_rx_buf *rxb);

	struct ieee80211_supported_band bands[IEEE80211_NUM_BANDS];

	
	struct il_spectrum_notification measure_report;
	u8 measurement_status;

	
	u32 ucode_beacon_time;
	int missed_beacon_threshold;

	
	u32 ibss_manager;

	
	struct il_force_reset force_reset;

	struct il_channel_info *channel_info;	
	u8 channel_count;	

	
	s32 temperature;	
	s32 last_temperature;

	
	unsigned long scan_start;
	unsigned long scan_start_tsf;
	void *scan_cmd;
	enum ieee80211_band scan_band;
	struct cfg80211_scan_request *scan_request;
	struct ieee80211_vif *scan_vif;
	u8 scan_tx_ant[IEEE80211_NUM_BANDS];
	u8 mgmt_tx_ant;

	
	spinlock_t lock;	
	spinlock_t hcmd_lock;	
	spinlock_t reg_lock;	
	struct mutex mutex;

	
	struct pci_dev *pci_dev;

	
	void __iomem *hw_base;
	u32 hw_rev;
	u32 hw_wa_rev;
	u8 rev_id;

	
	u8 cmd_queue;

	
	u8 sta_key_max_num;

	
	struct mac_address addresses[1];

	
	int fw_idx;		
	u32 ucode_ver;		
	struct fw_desc ucode_code;	
	struct fw_desc ucode_data;	
	struct fw_desc ucode_data_backup;	
	struct fw_desc ucode_init;	
	struct fw_desc ucode_init_data;	
	struct fw_desc ucode_boot;	
	enum ucode_type ucode_type;
	u8 ucode_write_complete;	
	char firmware_name[25];

	struct ieee80211_vif *vif;

	struct il_qos_info qos_data;

	struct {
		bool enabled;
		bool is_40mhz;
		bool non_gf_sta_present;
		u8 protection;
		u8 extension_chan_offset;
	} ht;

	const struct il_rxon_cmd active;
	struct il_rxon_cmd staging;

	struct il_rxon_time_cmd timing;

	__le16 switch_channel;

	struct il_init_alive_resp card_alive_init;
	struct il_alive_resp card_alive;

	u16 active_rate;

	u8 start_calib;
	struct il_sensitivity_data sensitivity_data;
	struct il_chain_noise_data chain_noise_data;
	__le16 sensitivity_tbl[HD_TBL_SIZE];

	struct il_ht_config current_ht_config;

	
	u8 retry_rate;

	wait_queue_head_t wait_command_queue;

	int activity_timer_active;

	
	struct il_rx_queue rxq;
	struct il_tx_queue *txq;
	unsigned long txq_ctx_active_msk;
	struct il_dma_ptr kw;	
	struct il_dma_ptr scd_bc_tbls;

	u32 scd_base_addr;	

	unsigned long status;

	
	struct traffic_stats tx_stats;
	struct traffic_stats rx_stats;

	
	struct isr_stats isr_stats;

	struct il_power_mgr power_data;

	
	u8 bssid[ETH_ALEN];	

	

	
	spinlock_t sta_lock;
	int num_stations;
	struct il_station_entry stations[IL_STATION_COUNT];
	unsigned long ucode_key_table;

	
#define IL_MAX_HW_QUEUES	32
	unsigned long queue_stopped[BITS_TO_LONGS(IL_MAX_HW_QUEUES)];
	
	atomic_t queue_stop_count[4];

	
	u8 is_open;

	u8 mac80211_registered;

	
	u8 *eeprom;
	struct il_eeprom_calib_info *calib_info;

	enum nl80211_iftype iw_mode;

	
	u64 timestamp;

	union {
#if defined(CONFIG_IWL3945) || defined(CONFIG_IWL3945_MODULE)
		struct {
			void *shared_virt;
			dma_addr_t shared_phys;

			struct delayed_work thermal_periodic;
			struct delayed_work rfkill_poll;

			struct il3945_notif_stats stats;
#ifdef CONFIG_IWLEGACY_DEBUGFS
			struct il3945_notif_stats accum_stats;
			struct il3945_notif_stats delta_stats;
			struct il3945_notif_stats max_delta;
#endif

			u32 sta_supp_rates;
			int last_rx_rssi;	

			
			u32 last_beacon_time;
			u64 last_tsf;

			const struct il3945_clip_group clip_groups[5];

		} _3945;
#endif
#if defined(CONFIG_IWL4965) || defined(CONFIG_IWL4965_MODULE)
		struct {
			struct il_rx_phy_res last_phy_res;
			bool last_phy_res_valid;

			struct completion firmware_loading_complete;

			u8 phy_calib_chain_noise_reset_cmd;
			u8 phy_calib_chain_noise_gain_cmd;

			u8 key_mapping_keys;
			struct il_wep_key wep_keys[WEP_KEYS_MAX];

			struct il_notif_stats stats;
#ifdef CONFIG_IWLEGACY_DEBUGFS
			struct il_notif_stats accum_stats;
			struct il_notif_stats delta_stats;
			struct il_notif_stats max_delta;
#endif

		} _4965;
#endif
	};

	struct il_hw_params hw_params;

	u32 inta_mask;

	struct workqueue_struct *workqueue;

	struct work_struct restart;
	struct work_struct scan_completed;
	struct work_struct rx_replenish;
	struct work_struct abort_scan;

	bool beacon_enabled;
	struct sk_buff *beacon_skb;

	struct work_struct tx_flush;

	struct tasklet_struct irq_tasklet;

	struct delayed_work init_alive_start;
	struct delayed_work alive_start;
	struct delayed_work scan_check;

	
	s8 tx_power_user_lmt;
	s8 tx_power_device_lmt;
	s8 tx_power_next;

#ifdef CONFIG_IWLEGACY_DEBUG
	
	u32 debug_level;	
#endif				
#ifdef CONFIG_IWLEGACY_DEBUGFS
	
	u16 tx_traffic_idx;
	u16 rx_traffic_idx;
	u8 *tx_traffic;
	u8 *rx_traffic;
	struct dentry *debugfs_dir;
	u32 dbgfs_sram_offset, dbgfs_sram_len;
	bool disable_ht40;
#endif				

	struct work_struct txpower_work;
	u32 disable_sens_cal;
	u32 disable_chain_noise_cal;
	u32 disable_tx_power_cal;
	struct work_struct run_time_calib_work;
	struct timer_list stats_periodic;
	struct timer_list watchdog;
	bool hw_ready;

	struct led_classdev led;
	unsigned long blink_on, blink_off;
	bool led_registered;
};				

static inline void
il_txq_ctx_activate(struct il_priv *il, int txq_id)
{
	set_bit(txq_id, &il->txq_ctx_active_msk);
}

static inline void
il_txq_ctx_deactivate(struct il_priv *il, int txq_id)
{
	clear_bit(txq_id, &il->txq_ctx_active_msk);
}

static inline int
il_is_associated(struct il_priv *il)
{
	return (il->active.filter_flags & RXON_FILTER_ASSOC_MSK) ? 1 : 0;
}

static inline int
il_is_any_associated(struct il_priv *il)
{
	return il_is_associated(il);
}

static inline int
il_is_channel_valid(const struct il_channel_info *ch_info)
{
	if (ch_info == NULL)
		return 0;
	return (ch_info->flags & EEPROM_CHANNEL_VALID) ? 1 : 0;
}

static inline int
il_is_channel_radar(const struct il_channel_info *ch_info)
{
	return (ch_info->flags & EEPROM_CHANNEL_RADAR) ? 1 : 0;
}

static inline u8
il_is_channel_a_band(const struct il_channel_info *ch_info)
{
	return ch_info->band == IEEE80211_BAND_5GHZ;
}

static inline int
il_is_channel_passive(const struct il_channel_info *ch)
{
	return (!(ch->flags & EEPROM_CHANNEL_ACTIVE)) ? 1 : 0;
}

static inline int
il_is_channel_ibss(const struct il_channel_info *ch)
{
	return (ch->flags & EEPROM_CHANNEL_IBSS) ? 1 : 0;
}

static inline void
__il_free_pages(struct il_priv *il, struct page *page)
{
	__free_pages(page, il->hw_params.rx_page_order);
	il->alloc_rxb_page--;
}

static inline void
il_free_pages(struct il_priv *il, unsigned long page)
{
	free_pages(page, il->hw_params.rx_page_order);
	il->alloc_rxb_page--;
}

#define IWLWIFI_VERSION "in-tree:"
#define DRV_COPYRIGHT	"Copyright(c) 2003-2011 Intel Corporation"
#define DRV_AUTHOR     "<ilw@linux.intel.com>"

#define IL_PCI_DEVICE(dev, subdev, cfg) \
	.vendor = PCI_VENDOR_ID_INTEL,  .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = (subdev), \
	.driver_data = (kernel_ulong_t)&(cfg)

#define TIME_UNIT		1024

#define IL_SKU_G       0x1
#define IL_SKU_A       0x2
#define IL_SKU_N       0x8

#define IL_CMD(x) case x: return #x

#define IL_RX_BUF_SIZE_3K (3 * 1000)	
#define IL_RX_BUF_SIZE_4K (4 * 1024)
#define IL_RX_BUF_SIZE_8K (8 * 1024)

#ifdef CONFIG_IWLEGACY_DEBUGFS
struct il_debugfs_ops {
	ssize_t(*rx_stats_read) (struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos);
	ssize_t(*tx_stats_read) (struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos);
	ssize_t(*general_stats_read) (struct file *file,
				      char __user *user_buf, size_t count,
				      loff_t *ppos);
};
#endif

struct il_ops {
	
	void (*txq_update_byte_cnt_tbl) (struct il_priv *il,
					 struct il_tx_queue *txq,
					 u16 byte_cnt);
	int (*txq_attach_buf_to_tfd) (struct il_priv *il,
				      struct il_tx_queue *txq, dma_addr_t addr,
				      u16 len, u8 reset, u8 pad);
	void (*txq_free_tfd) (struct il_priv *il, struct il_tx_queue *txq);
	int (*txq_init) (struct il_priv *il, struct il_tx_queue *txq);
	
	void (*init_alive_start) (struct il_priv *il);
	
	int (*is_valid_rtc_data_addr) (u32 addr);
	
	int (*load_ucode) (struct il_priv *il);

	void (*dump_nic_error_log) (struct il_priv *il);
	int (*dump_fh) (struct il_priv *il, char **buf, bool display);
	int (*set_channel_switch) (struct il_priv *il,
				   struct ieee80211_channel_switch *ch_switch);
	
	int (*apm_init) (struct il_priv *il);

	
	int (*send_tx_power) (struct il_priv *il);
	void (*update_chain_flags) (struct il_priv *il);

	
	int (*eeprom_acquire_semaphore) (struct il_priv *il);
	void (*eeprom_release_semaphore) (struct il_priv *il);

	int (*rxon_assoc) (struct il_priv *il);
	int (*commit_rxon) (struct il_priv *il);
	void (*set_rxon_chain) (struct il_priv *il);

	u16(*get_hcmd_size) (u8 cmd_id, u16 len);
	u16(*build_addsta_hcmd) (const struct il_addsta_cmd *cmd, u8 *data);

	int (*request_scan) (struct il_priv *il, struct ieee80211_vif *vif);
	void (*post_scan) (struct il_priv *il);
	void (*post_associate) (struct il_priv *il);
	void (*config_ap) (struct il_priv *il);
	
	int (*update_bcast_stations) (struct il_priv *il);
	int (*manage_ibss_station) (struct il_priv *il,
				    struct ieee80211_vif *vif, bool add);

	int (*send_led_cmd) (struct il_priv *il, struct il_led_cmd *led_cmd);
};

struct il_mod_params {
	int sw_crypto;		
	int disable_hw_scan;	
	int num_of_queues;	
	int disable_11n;	
	int amsdu_size_8K;	
	int antenna;		
	int restart_fw;		
};

#define IL_LED_SOLID 11
#define IL_DEF_LED_INTRVL cpu_to_le32(1000)

#define IL_LED_ACTIVITY       (0<<1)
#define IL_LED_LINK           (1<<1)

enum il_led_mode {
	IL_LED_DEFAULT,
	IL_LED_RF_STATE,
	IL_LED_BLINK,
};

void il_leds_init(struct il_priv *il);
void il_leds_exit(struct il_priv *il);

struct il_cfg {
	
	const char *name;
	const char *fw_name_pre;
	const unsigned int ucode_api_max;
	const unsigned int ucode_api_min;
	u8 valid_tx_ant;
	u8 valid_rx_ant;
	unsigned int sku;
	u16 eeprom_ver;
	u16 eeprom_calib_ver;
	
	const struct il_mod_params *mod_params;
	
	struct il_base_params *base_params;
	
	u8 scan_rx_antennas[IEEE80211_NUM_BANDS];
	enum il_led_mode led_mode;

	int eeprom_size;
	int num_of_queues;		
	int num_of_ampdu_queues;	
	
	u32 pll_cfg_val;
	bool set_l0s;
	bool use_bsm;

	u16 led_compensation;
	int chain_noise_num_beacons;
	unsigned int wd_timeout;
	bool temperature_kelvin;
	const bool ucode_tracing;
	const bool sensitivity_calib_by_driver;
	const bool chain_noise_calib_by_driver;

	const u32 regulatory_bands[7];
};


int il_mac_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		   u16 queue, const struct ieee80211_tx_queue_params *params);
int il_mac_tx_last_beacon(struct ieee80211_hw *hw);

void il_set_rxon_hwcrypto(struct il_priv *il, int hw_decrypt);
int il_check_rxon_cmd(struct il_priv *il);
int il_full_rxon_required(struct il_priv *il);
int il_set_rxon_channel(struct il_priv *il, struct ieee80211_channel *ch);
void il_set_flags_for_band(struct il_priv *il, enum ieee80211_band band,
			   struct ieee80211_vif *vif);
u8 il_get_single_channel_number(struct il_priv *il, enum ieee80211_band band);
void il_set_rxon_ht(struct il_priv *il, struct il_ht_config *ht_conf);
bool il_is_ht40_tx_allowed(struct il_priv *il,
			   struct ieee80211_sta_ht_cap *ht_cap);
void il_connection_init_rx_config(struct il_priv *il);
void il_set_rate(struct il_priv *il);
int il_set_decrypted_flag(struct il_priv *il, struct ieee80211_hdr *hdr,
			  u32 decrypt_res, struct ieee80211_rx_status *stats);
void il_irq_handle_error(struct il_priv *il);
int il_mac_add_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
void il_mac_remove_interface(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif);
int il_mac_change_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			    enum nl80211_iftype newtype, bool newp2p);
int il_alloc_txq_mem(struct il_priv *il);
void il_free_txq_mem(struct il_priv *il);

#ifdef CONFIG_IWLEGACY_DEBUGFS
extern void il_update_stats(struct il_priv *il, bool is_tx, __le16 fc, u16 len);
#else
static inline void
il_update_stats(struct il_priv *il, bool is_tx, __le16 fc, u16 len)
{
}
#endif

void il_hdl_pm_sleep(struct il_priv *il, struct il_rx_buf *rxb);
void il_hdl_pm_debug_stats(struct il_priv *il, struct il_rx_buf *rxb);
void il_hdl_error(struct il_priv *il, struct il_rx_buf *rxb);
void il_hdl_csa(struct il_priv *il, struct il_rx_buf *rxb);

void il_cmd_queue_unmap(struct il_priv *il);
void il_cmd_queue_free(struct il_priv *il);
int il_rx_queue_alloc(struct il_priv *il);
void il_rx_queue_update_write_ptr(struct il_priv *il, struct il_rx_queue *q);
int il_rx_queue_space(const struct il_rx_queue *q);
void il_tx_cmd_complete(struct il_priv *il, struct il_rx_buf *rxb);

void il_hdl_spectrum_measurement(struct il_priv *il, struct il_rx_buf *rxb);
void il_recover_from_stats(struct il_priv *il, struct il_rx_pkt *pkt);
void il_chswitch_done(struct il_priv *il, bool is_success);

extern void il_txq_update_write_ptr(struct il_priv *il, struct il_tx_queue *txq);
extern int il_tx_queue_init(struct il_priv *il, u32 txq_id);
extern void il_tx_queue_reset(struct il_priv *il, u32 txq_id);
extern void il_tx_queue_unmap(struct il_priv *il, int txq_id);
extern void il_tx_queue_free(struct il_priv *il, int txq_id);
extern void il_setup_watchdog(struct il_priv *il);
int il_set_tx_power(struct il_priv *il, s8 tx_power, bool force);


u8 il_get_lowest_plcp(struct il_priv *il);

void il_init_scan_params(struct il_priv *il);
int il_scan_cancel(struct il_priv *il);
int il_scan_cancel_timeout(struct il_priv *il, unsigned long ms);
void il_force_scan_end(struct il_priv *il);
int il_mac_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		   struct cfg80211_scan_request *req);
void il_internal_short_hw_scan(struct il_priv *il);
int il_force_reset(struct il_priv *il, bool external);
u16 il_fill_probe_req(struct il_priv *il, struct ieee80211_mgmt *frame,
		      const u8 *ta, const u8 *ie, int ie_len, int left);
void il_setup_rx_scan_handlers(struct il_priv *il);
u16 il_get_active_dwell_time(struct il_priv *il, enum ieee80211_band band,
			     u8 n_probes);
u16 il_get_passive_dwell_time(struct il_priv *il, enum ieee80211_band band,
			      struct ieee80211_vif *vif);
void il_setup_scan_deferred_work(struct il_priv *il);
void il_cancel_scan_deferred_work(struct il_priv *il);

#define IL_ACTIVE_QUIET_TIME       cpu_to_le16(10)	
#define IL_PLCP_QUIET_THRESH       cpu_to_le16(1)	

#define IL_SCAN_CHECK_WATCHDOG		(HZ * 7)


const char *il_get_cmd_string(u8 cmd);
int __must_check il_send_cmd_sync(struct il_priv *il, struct il_host_cmd *cmd);
int il_send_cmd(struct il_priv *il, struct il_host_cmd *cmd);
int __must_check il_send_cmd_pdu(struct il_priv *il, u8 id, u16 len,
				 const void *data);
int il_send_cmd_pdu_async(struct il_priv *il, u8 id, u16 len, const void *data,
			  void (*callback) (struct il_priv *il,
					    struct il_device_cmd *cmd,
					    struct il_rx_pkt *pkt));

int il_enqueue_hcmd(struct il_priv *il, struct il_host_cmd *cmd);


static inline u16
il_pcie_link_ctl(struct il_priv *il)
{
	int pos;
	u16 pci_lnk_ctl;
	pos = pci_pcie_cap(il->pci_dev);
	pci_read_config_word(il->pci_dev, pos + PCI_EXP_LNKCTL, &pci_lnk_ctl);
	return pci_lnk_ctl;
}

void il_bg_watchdog(unsigned long data);
u32 il_usecs_to_beacons(struct il_priv *il, u32 usec, u32 beacon_interval);
__le32 il_add_beacon_time(struct il_priv *il, u32 base, u32 addon,
			  u32 beacon_interval);

#ifdef CONFIG_PM
int il_pci_suspend(struct device *device);
int il_pci_resume(struct device *device);
extern const struct dev_pm_ops il_pm_ops;

#define IL_LEGACY_PM_OPS	(&il_pm_ops)

#else 

#define IL_LEGACY_PM_OPS	NULL

#endif 

void il4965_dump_nic_error_log(struct il_priv *il);
#ifdef CONFIG_IWLEGACY_DEBUG
void il_print_rx_config_cmd(struct il_priv *il);
#else
static inline void
il_print_rx_config_cmd(struct il_priv *il)
{
}
#endif

void il_clear_isr_stats(struct il_priv *il);

int il_init_geos(struct il_priv *il);
void il_free_geos(struct il_priv *il);


#define S_HCMD_ACTIVE	0	
#define S_INT_ENABLED	2
#define S_RFKILL	3
#define S_CT_KILL		4
#define S_INIT		5
#define S_ALIVE		6
#define S_READY		7
#define S_TEMPERATURE	8
#define S_GEO_CONFIGURED	9
#define S_EXIT_PENDING	10
#define S_STATS		12
#define S_SCANNING		13
#define S_SCAN_ABORTING	14
#define S_SCAN_HW		15
#define S_POWER_PMI	16
#define S_FW_ERROR		17
#define S_CHANNEL_SWITCH_PENDING 18

static inline int
il_is_ready(struct il_priv *il)
{
	return test_bit(S_READY, &il->status) &&
	    test_bit(S_GEO_CONFIGURED, &il->status) &&
	    !test_bit(S_EXIT_PENDING, &il->status);
}

static inline int
il_is_alive(struct il_priv *il)
{
	return test_bit(S_ALIVE, &il->status);
}

static inline int
il_is_init(struct il_priv *il)
{
	return test_bit(S_INIT, &il->status);
}

static inline int
il_is_rfkill(struct il_priv *il)
{
	return test_bit(S_RFKILL, &il->status);
}

static inline int
il_is_ctkill(struct il_priv *il)
{
	return test_bit(S_CT_KILL, &il->status);
}

static inline int
il_is_ready_rf(struct il_priv *il)
{

	if (il_is_rfkill(il))
		return 0;

	return il_is_ready(il);
}

extern void il_send_bt_config(struct il_priv *il);
extern int il_send_stats_request(struct il_priv *il, u8 flags, bool clear);
extern void il_apm_stop(struct il_priv *il);
extern void _il_apm_stop(struct il_priv *il);

int il_apm_init(struct il_priv *il);

int il_send_rxon_timing(struct il_priv *il);

static inline int
il_send_rxon_assoc(struct il_priv *il)
{
	return il->ops->rxon_assoc(il);
}

static inline int
il_commit_rxon(struct il_priv *il)
{
	return il->ops->commit_rxon(il);
}

static inline const struct ieee80211_supported_band *
il_get_hw_mode(struct il_priv *il, enum ieee80211_band band)
{
	return il->hw->wiphy->bands[band];
}

int il_mac_config(struct ieee80211_hw *hw, u32 changed);
void il_mac_reset_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
void il_mac_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			     struct ieee80211_bss_conf *bss_conf, u32 changes);
void il_tx_cmd_protection(struct il_priv *il, struct ieee80211_tx_info *info,
			  __le16 fc, __le32 *tx_flags);

irqreturn_t il_isr(int irq, void *data);

extern void il_set_bit(struct il_priv *p, u32 r, u32 m);
extern void il_clear_bit(struct il_priv *p, u32 r, u32 m);
extern bool _il_grab_nic_access(struct il_priv *il);
extern int _il_poll_bit(struct il_priv *il, u32 addr, u32 bits, u32 mask, int timeout);
extern int il_poll_bit(struct il_priv *il, u32 addr, u32 mask, int timeout);
extern u32 il_rd_prph(struct il_priv *il, u32 reg);
extern void il_wr_prph(struct il_priv *il, u32 addr, u32 val);
extern u32 il_read_targ_mem(struct il_priv *il, u32 addr);
extern void il_write_targ_mem(struct il_priv *il, u32 addr, u32 val);

static inline void
_il_write8(struct il_priv *il, u32 ofs, u8 val)
{
	writeb(val, il->hw_base + ofs);
}
#define il_write8(il, ofs, val) _il_write8(il, ofs, val)

static inline void
_il_wr(struct il_priv *il, u32 ofs, u32 val)
{
	writel(val, il->hw_base + ofs);
}

static inline u32
_il_rd(struct il_priv *il, u32 ofs)
{
	return readl(il->hw_base + ofs);
}

static inline void
_il_clear_bit(struct il_priv *il, u32 reg, u32 mask)
{
	_il_wr(il, reg, _il_rd(il, reg) & ~mask);
}

static inline void
_il_set_bit(struct il_priv *il, u32 reg, u32 mask)
{
	_il_wr(il, reg, _il_rd(il, reg) | mask);
}

static inline void
_il_release_nic_access(struct il_priv *il)
{
	_il_clear_bit(il, CSR_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
	mmiowb();
}

static inline u32
il_rd(struct il_priv *il, u32 reg)
{
	u32 value;
	unsigned long reg_flags;

	spin_lock_irqsave(&il->reg_lock, reg_flags);
	_il_grab_nic_access(il);
	value = _il_rd(il, reg);
	_il_release_nic_access(il);
	spin_unlock_irqrestore(&il->reg_lock, reg_flags);
	return value;
}

static inline void
il_wr(struct il_priv *il, u32 reg, u32 value)
{
	unsigned long reg_flags;

	spin_lock_irqsave(&il->reg_lock, reg_flags);
	if (likely(_il_grab_nic_access(il))) {
		_il_wr(il, reg, value);
		_il_release_nic_access(il);
	}
	spin_unlock_irqrestore(&il->reg_lock, reg_flags);
}

static inline u32
_il_rd_prph(struct il_priv *il, u32 reg)
{
	_il_wr(il, HBUS_TARG_PRPH_RADDR, reg | (3 << 24));
	return _il_rd(il, HBUS_TARG_PRPH_RDAT);
}

static inline void
_il_wr_prph(struct il_priv *il, u32 addr, u32 val)
{
	_il_wr(il, HBUS_TARG_PRPH_WADDR, ((addr & 0x0000FFFF) | (3 << 24)));
	_il_wr(il, HBUS_TARG_PRPH_WDAT, val);
}

static inline void
il_set_bits_prph(struct il_priv *il, u32 reg, u32 mask)
{
	unsigned long reg_flags;

	spin_lock_irqsave(&il->reg_lock, reg_flags);
	if (likely(_il_grab_nic_access(il))) {
		_il_wr_prph(il, reg, (_il_rd_prph(il, reg) | mask));
		_il_release_nic_access(il);
	}
	spin_unlock_irqrestore(&il->reg_lock, reg_flags);
}

static inline void
il_set_bits_mask_prph(struct il_priv *il, u32 reg, u32 bits, u32 mask)
{
	unsigned long reg_flags;

	spin_lock_irqsave(&il->reg_lock, reg_flags);
	if (likely(_il_grab_nic_access(il))) {
		_il_wr_prph(il, reg, ((_il_rd_prph(il, reg) & mask) | bits));
		_il_release_nic_access(il);
	}
	spin_unlock_irqrestore(&il->reg_lock, reg_flags);
}

static inline void
il_clear_bits_prph(struct il_priv *il, u32 reg, u32 mask)
{
	unsigned long reg_flags;
	u32 val;

	spin_lock_irqsave(&il->reg_lock, reg_flags);
	if (likely(_il_grab_nic_access(il))) {
		val = _il_rd_prph(il, reg);
		_il_wr_prph(il, reg, (val & ~mask));
		_il_release_nic_access(il);
	}
	spin_unlock_irqrestore(&il->reg_lock, reg_flags);
}

#define HW_KEY_DYNAMIC 0
#define HW_KEY_DEFAULT 1

#define IL_STA_DRIVER_ACTIVE BIT(0)	
#define IL_STA_UCODE_ACTIVE  BIT(1)	
#define IL_STA_UCODE_INPROGRESS  BIT(2)	
#define IL_STA_LOCAL BIT(3)	
#define IL_STA_BCAST BIT(4)	

void il_restore_stations(struct il_priv *il);
void il_clear_ucode_stations(struct il_priv *il);
void il_dealloc_bcast_stations(struct il_priv *il);
int il_get_free_ucode_key_idx(struct il_priv *il);
int il_send_add_sta(struct il_priv *il, struct il_addsta_cmd *sta, u8 flags);
int il_add_station_common(struct il_priv *il, const u8 *addr, bool is_ap,
			  struct ieee80211_sta *sta, u8 *sta_id_r);
int il_remove_station(struct il_priv *il, const u8 sta_id, const u8 * addr);
int il_mac_sta_remove(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		      struct ieee80211_sta *sta);

u8 il_prep_station(struct il_priv *il, const u8 *addr, bool is_ap,
		   struct ieee80211_sta *sta);

int il_send_lq_cmd(struct il_priv *il, struct il_link_quality_cmd *lq,
		   u8 flags, bool init);

static inline void
il_clear_driver_stations(struct il_priv *il)
{
	unsigned long flags;

	spin_lock_irqsave(&il->sta_lock, flags);
	memset(il->stations, 0, sizeof(il->stations));
	il->num_stations = 0;
	il->ucode_key_table = 0;
	spin_unlock_irqrestore(&il->sta_lock, flags);
}

static inline int
il_sta_id(struct ieee80211_sta *sta)
{
	if (WARN_ON(!sta))
		return IL_INVALID_STATION;

	return ((struct il_station_priv_common *)sta->drv_priv)->sta_id;
}

static inline int
il_sta_id_or_broadcast(struct il_priv *il, struct ieee80211_sta *sta)
{
	int sta_id;

	if (!sta)
		return il->hw_params.bcast_id;

	sta_id = il_sta_id(sta);

	WARN_ON(sta_id == IL_INVALID_STATION);

	return sta_id;
}

static inline int
il_queue_inc_wrap(int idx, int n_bd)
{
	return ++idx & (n_bd - 1);
}

static inline int
il_queue_dec_wrap(int idx, int n_bd)
{
	return --idx & (n_bd - 1);
}

static inline void
il_free_fw_desc(struct pci_dev *pci_dev, struct fw_desc *desc)
{
	if (desc->v_addr)
		dma_free_coherent(&pci_dev->dev, desc->len, desc->v_addr,
				  desc->p_addr);
	desc->v_addr = NULL;
	desc->len = 0;
}

static inline int
il_alloc_fw_desc(struct pci_dev *pci_dev, struct fw_desc *desc)
{
	if (!desc->len) {
		desc->v_addr = NULL;
		return -EINVAL;
	}

	desc->v_addr =
	    dma_alloc_coherent(&pci_dev->dev, desc->len, &desc->p_addr,
			       GFP_KERNEL);
	return (desc->v_addr != NULL) ? 0 : -ENOMEM;
}

static inline void
il_set_swq_id(struct il_tx_queue *txq, u8 ac, u8 hwq)
{
	BUG_ON(ac > 3);		
	BUG_ON(hwq > 31);	

	txq->swq_id = (hwq << 2) | ac;
}

static inline void
il_wake_queue(struct il_priv *il, struct il_tx_queue *txq)
{
	u8 queue = txq->swq_id;
	u8 ac = queue & 3;
	u8 hwq = (queue >> 2) & 0x1f;

	if (test_and_clear_bit(hwq, il->queue_stopped))
		if (atomic_dec_return(&il->queue_stop_count[ac]) <= 0)
			ieee80211_wake_queue(il->hw, ac);
}

static inline void
il_stop_queue(struct il_priv *il, struct il_tx_queue *txq)
{
	u8 queue = txq->swq_id;
	u8 ac = queue & 3;
	u8 hwq = (queue >> 2) & 0x1f;

	if (!test_and_set_bit(hwq, il->queue_stopped))
		if (atomic_inc_return(&il->queue_stop_count[ac]) > 0)
			ieee80211_stop_queue(il->hw, ac);
}

#ifdef ieee80211_stop_queue
#undef ieee80211_stop_queue
#endif

#define ieee80211_stop_queue DO_NOT_USE_ieee80211_stop_queue

#ifdef ieee80211_wake_queue
#undef ieee80211_wake_queue
#endif

#define ieee80211_wake_queue DO_NOT_USE_ieee80211_wake_queue

static inline void
il_disable_interrupts(struct il_priv *il)
{
	clear_bit(S_INT_ENABLED, &il->status);

	
	_il_wr(il, CSR_INT_MASK, 0x00000000);

	_il_wr(il, CSR_INT, 0xffffffff);
	_il_wr(il, CSR_FH_INT_STATUS, 0xffffffff);
}

static inline void
il_enable_rfkill_int(struct il_priv *il)
{
	_il_wr(il, CSR_INT_MASK, CSR_INT_BIT_RF_KILL);
}

static inline void
il_enable_interrupts(struct il_priv *il)
{
	set_bit(S_INT_ENABLED, &il->status);
	_il_wr(il, CSR_INT_MASK, il->inta_mask);
}

static inline u32
il_beacon_time_mask_low(struct il_priv *il, u16 tsf_bits)
{
	return (1 << tsf_bits) - 1;
}

static inline u32
il_beacon_time_mask_high(struct il_priv *il, u16 tsf_bits)
{
	return ((1 << (32 - tsf_bits)) - 1) << tsf_bits;
}

/**
 * struct il_rb_status - reseve buffer status host memory mapped FH registers
 *
 * @closed_rb_num [0:11] - Indicates the idx of the RB which was closed
 * @closed_fr_num [0:11] - Indicates the idx of the RX Frame which was closed
 * @finished_rb_num [0:11] - Indicates the idx of the current RB
 *			     in which the last frame was written to
 * @finished_fr_num [0:11] - Indicates the idx of the RX Frame
 *			     which was transferred
 */
struct il_rb_status {
	__le16 closed_rb_num;
	__le16 closed_fr_num;
	__le16 finished_rb_num;
	__le16 finished_fr_nam;
	__le32 __unused;	
} __packed;

#define TFD_QUEUE_SIZE_MAX      256
#define TFD_QUEUE_SIZE_BC_DUP	64
#define TFD_QUEUE_BC_SIZE	(TFD_QUEUE_SIZE_MAX + TFD_QUEUE_SIZE_BC_DUP)
#define IL_TX_DMA_MASK		DMA_BIT_MASK(36)
#define IL_NUM_OF_TBS		20

static inline u8
il_get_dma_hi_addr(dma_addr_t addr)
{
	return (sizeof(addr) > sizeof(u32) ? (addr >> 16) >> 16 : 0) & 0xF;
}

struct il_tfd_tb {
	__le32 lo;
	__le16 hi_n_len;
} __packed;

struct il_tfd {
	u8 __reserved1[3];
	u8 num_tbs;
	struct il_tfd_tb tbs[IL_NUM_OF_TBS];
	__le32 __pad;
} __packed;
#define PCI_CFG_RETRY_TIMEOUT	0x041

#define PCI_CFG_LINK_CTRL_VAL_L0S_EN	0x01
#define PCI_CFG_LINK_CTRL_VAL_L1_EN	0x02

struct il_rate_info {
	u8 plcp;		
	u8 plcp_siso;		
	u8 plcp_mimo2;		
	u8 ieee;		
	u8 prev_ieee;		
	u8 next_ieee;		
	u8 prev_rs;		
	u8 next_rs;		
	u8 prev_rs_tgg;		
	u8 next_rs_tgg;		
};

struct il3945_rate_info {
	u8 plcp;		
	u8 ieee;		
	u8 prev_ieee;		
	u8 next_ieee;		
	u8 prev_rs;		
	u8 next_rs;		
	u8 prev_rs_tgg;		
	u8 next_rs_tgg;		
	u8 table_rs_idx;	
	u8 prev_table_rs;	
};

enum {
	RATE_1M_IDX = 0,
	RATE_2M_IDX,
	RATE_5M_IDX,
	RATE_11M_IDX,
	RATE_6M_IDX,
	RATE_9M_IDX,
	RATE_12M_IDX,
	RATE_18M_IDX,
	RATE_24M_IDX,
	RATE_36M_IDX,
	RATE_48M_IDX,
	RATE_54M_IDX,
	RATE_60M_IDX,
	RATE_COUNT,
	RATE_COUNT_LEGACY = RATE_COUNT - 1,	
	RATE_COUNT_3945 = RATE_COUNT - 1,
	RATE_INVM_IDX = RATE_COUNT,
	RATE_INVALID = RATE_COUNT,
};

enum {
	RATE_6M_IDX_TBL = 0,
	RATE_9M_IDX_TBL,
	RATE_12M_IDX_TBL,
	RATE_18M_IDX_TBL,
	RATE_24M_IDX_TBL,
	RATE_36M_IDX_TBL,
	RATE_48M_IDX_TBL,
	RATE_54M_IDX_TBL,
	RATE_1M_IDX_TBL,
	RATE_2M_IDX_TBL,
	RATE_5M_IDX_TBL,
	RATE_11M_IDX_TBL,
	RATE_INVM_IDX_TBL = RATE_INVM_IDX - 1,
};

enum {
	IL_FIRST_OFDM_RATE = RATE_6M_IDX,
	IL39_LAST_OFDM_RATE = RATE_54M_IDX,
	IL_LAST_OFDM_RATE = RATE_60M_IDX,
	IL_FIRST_CCK_RATE = RATE_1M_IDX,
	IL_LAST_CCK_RATE = RATE_11M_IDX,
};

#define	RATE_6M_MASK   (1 << RATE_6M_IDX)
#define	RATE_9M_MASK   (1 << RATE_9M_IDX)
#define	RATE_12M_MASK  (1 << RATE_12M_IDX)
#define	RATE_18M_MASK  (1 << RATE_18M_IDX)
#define	RATE_24M_MASK  (1 << RATE_24M_IDX)
#define	RATE_36M_MASK  (1 << RATE_36M_IDX)
#define	RATE_48M_MASK  (1 << RATE_48M_IDX)
#define	RATE_54M_MASK  (1 << RATE_54M_IDX)
#define RATE_60M_MASK  (1 << RATE_60M_IDX)
#define	RATE_1M_MASK   (1 << RATE_1M_IDX)
#define	RATE_2M_MASK   (1 << RATE_2M_IDX)
#define	RATE_5M_MASK   (1 << RATE_5M_IDX)
#define	RATE_11M_MASK  (1 << RATE_11M_IDX)

enum {
	RATE_6M_PLCP = 13,
	RATE_9M_PLCP = 15,
	RATE_12M_PLCP = 5,
	RATE_18M_PLCP = 7,
	RATE_24M_PLCP = 9,
	RATE_36M_PLCP = 11,
	RATE_48M_PLCP = 1,
	RATE_54M_PLCP = 3,
	RATE_60M_PLCP = 3,	
	RATE_1M_PLCP = 10,
	RATE_2M_PLCP = 20,
	RATE_5M_PLCP = 55,
	RATE_11M_PLCP = 110,
	
};

enum {
	RATE_SISO_6M_PLCP = 0,
	RATE_SISO_12M_PLCP = 1,
	RATE_SISO_18M_PLCP = 2,
	RATE_SISO_24M_PLCP = 3,
	RATE_SISO_36M_PLCP = 4,
	RATE_SISO_48M_PLCP = 5,
	RATE_SISO_54M_PLCP = 6,
	RATE_SISO_60M_PLCP = 7,
	RATE_MIMO2_6M_PLCP = 0x8,
	RATE_MIMO2_12M_PLCP = 0x9,
	RATE_MIMO2_18M_PLCP = 0xa,
	RATE_MIMO2_24M_PLCP = 0xb,
	RATE_MIMO2_36M_PLCP = 0xc,
	RATE_MIMO2_48M_PLCP = 0xd,
	RATE_MIMO2_54M_PLCP = 0xe,
	RATE_MIMO2_60M_PLCP = 0xf,
	RATE_SISO_INVM_PLCP,
	RATE_MIMO2_INVM_PLCP = RATE_SISO_INVM_PLCP,
};

enum {
	RATE_6M_IEEE = 12,
	RATE_9M_IEEE = 18,
	RATE_12M_IEEE = 24,
	RATE_18M_IEEE = 36,
	RATE_24M_IEEE = 48,
	RATE_36M_IEEE = 72,
	RATE_48M_IEEE = 96,
	RATE_54M_IEEE = 108,
	RATE_60M_IEEE = 120,
	RATE_1M_IEEE = 2,
	RATE_2M_IEEE = 4,
	RATE_5M_IEEE = 11,
	RATE_11M_IEEE = 22,
};

#define IL_CCK_BASIC_RATES_MASK    \
	(RATE_1M_MASK          | \
	RATE_2M_MASK)

#define IL_CCK_RATES_MASK          \
	(IL_CCK_BASIC_RATES_MASK  | \
	RATE_5M_MASK          | \
	RATE_11M_MASK)

#define IL_OFDM_BASIC_RATES_MASK   \
	(RATE_6M_MASK         | \
	RATE_12M_MASK         | \
	RATE_24M_MASK)

#define IL_OFDM_RATES_MASK         \
	(IL_OFDM_BASIC_RATES_MASK | \
	RATE_9M_MASK          | \
	RATE_18M_MASK         | \
	RATE_36M_MASK         | \
	RATE_48M_MASK         | \
	RATE_54M_MASK)

#define IL_BASIC_RATES_MASK         \
	(IL_OFDM_BASIC_RATES_MASK | \
	 IL_CCK_BASIC_RATES_MASK)

#define RATES_MASK ((1 << RATE_COUNT) - 1)
#define RATES_MASK_3945 ((1 << RATE_COUNT_3945) - 1)

#define IL_INVALID_VALUE    -1

#define IL_MIN_RSSI_VAL                 -100
#define IL_MAX_RSSI_VAL                    0

#define IL_LEGACY_FAILURE_LIMIT	160
#define IL_LEGACY_SUCCESS_LIMIT	480
#define IL_LEGACY_TBL_COUNT		160

#define IL_NONE_LEGACY_FAILURE_LIMIT	400
#define IL_NONE_LEGACY_SUCCESS_LIMIT	4500
#define IL_NONE_LEGACY_TBL_COUNT	1500

#define IL_RS_GOOD_RATIO		12800	
#define RATE_SCALE_SWITCH		10880	
#define RATE_HIGH_TH		10880	
#define RATE_INCREASE_TH		6400	
#define RATE_DECREASE_TH		1920	

#define IL_LEGACY_SWITCH_ANTENNA1      0
#define IL_LEGACY_SWITCH_ANTENNA2      1
#define IL_LEGACY_SWITCH_SISO          2
#define IL_LEGACY_SWITCH_MIMO2_AB      3
#define IL_LEGACY_SWITCH_MIMO2_AC      4
#define IL_LEGACY_SWITCH_MIMO2_BC      5

#define IL_SISO_SWITCH_ANTENNA1        0
#define IL_SISO_SWITCH_ANTENNA2        1
#define IL_SISO_SWITCH_MIMO2_AB        2
#define IL_SISO_SWITCH_MIMO2_AC        3
#define IL_SISO_SWITCH_MIMO2_BC        4
#define IL_SISO_SWITCH_GI              5

#define IL_MIMO2_SWITCH_ANTENNA1       0
#define IL_MIMO2_SWITCH_ANTENNA2       1
#define IL_MIMO2_SWITCH_SISO_A         2
#define IL_MIMO2_SWITCH_SISO_B         3
#define IL_MIMO2_SWITCH_SISO_C         4
#define IL_MIMO2_SWITCH_GI             5

#define IL_MAX_SEARCH IL_MIMO2_SWITCH_GI

#define IL_ACTION_LIMIT		3	

#define LQ_SIZE		2	

#define IL_AGG_TPT_THREHOLD	0
#define IL_AGG_LOAD_THRESHOLD	10
#define IL_AGG_ALL_TID		0xff
#define TID_QUEUE_CELL_SPACING	50	
#define TID_QUEUE_MAX_SIZE	20
#define TID_ROUND_VALUE		5	
#define TID_MAX_LOAD_COUNT	8

#define TID_MAX_TIME_DIFF ((TID_QUEUE_MAX_SIZE - 1) * TID_QUEUE_CELL_SPACING)
#define TIME_WRAP_AROUND(x, y) (((y) > (x)) ? (y) - (x) : (0-(x)) + (y))

extern const struct il_rate_info il_rates[RATE_COUNT];

enum il_table_type {
	LQ_NONE,
	LQ_G,			
	LQ_A,
	LQ_SISO,		
	LQ_MIMO2,
	LQ_MAX,
};

#define is_legacy(tbl) ((tbl) == LQ_G || (tbl) == LQ_A)
#define is_siso(tbl) ((tbl) == LQ_SISO)
#define is_mimo2(tbl) ((tbl) == LQ_MIMO2)
#define is_mimo(tbl) (is_mimo2(tbl))
#define is_Ht(tbl) (is_siso(tbl) || is_mimo(tbl))
#define is_a_band(tbl) ((tbl) == LQ_A)
#define is_g_and(tbl) ((tbl) == LQ_G)

#define	ANT_NONE	0x0
#define	ANT_A		BIT(0)
#define	ANT_B		BIT(1)
#define	ANT_AB		(ANT_A | ANT_B)
#define ANT_C		BIT(2)
#define	ANT_AC		(ANT_A | ANT_C)
#define ANT_BC		(ANT_B | ANT_C)
#define ANT_ABC		(ANT_AB | ANT_C)

#define IL_MAX_MCS_DISPLAY_SIZE	12

struct il_rate_mcs_info {
	char mbps[IL_MAX_MCS_DISPLAY_SIZE];
	char mcs[IL_MAX_MCS_DISPLAY_SIZE];
};

struct il_rate_scale_data {
	u64 data;		
	s32 success_counter;	
	s32 success_ratio;	
	s32 counter;		
	s32 average_tpt;	
	unsigned long stamp;
};

struct il_scale_tbl_info {
	enum il_table_type lq_type;
	u8 ant_type;
	u8 is_SGI;		
	u8 is_ht40;		
	u8 is_dup;		
	u8 action;		
	u8 max_search;		
	s32 *expected_tpt;	
	u32 current_rate;	
	struct il_rate_scale_data win[RATE_COUNT];	
};

struct il_traffic_load {
	unsigned long time_stamp;	
	u32 packet_count[TID_QUEUE_MAX_SIZE];	
	u32 total;		
	u8 queue_count;		
	u8 head;		
};

struct il_lq_sta {
	u8 active_tbl;		
	u8 enable_counter;	
	u8 stay_in_tbl;		
	u8 search_better_tbl;	
	s32 last_tpt;

	
	u32 table_count_limit;
	u32 max_failure_limit;	
	u32 max_success_limit;	
	u32 table_count;
	u32 total_failed;	
	u32 total_success;	
	u64 flush_timer;	

	u8 action_counter;	
	u8 is_green;
	u8 is_dup;
	enum ieee80211_band band;

	
	u32 supp_rates;
	u16 active_legacy_rate;
	u16 active_siso_rate;
	u16 active_mimo2_rate;
	s8 max_rate_idx;	
	u8 missed_rate_counter;

	struct il_link_quality_cmd lq;
	struct il_scale_tbl_info lq_info[LQ_SIZE];	
	struct il_traffic_load load[TID_MAX_LOAD_COUNT];
	u8 tx_agg_tid_en;
#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *rs_sta_dbgfs_scale_table_file;
	struct dentry *rs_sta_dbgfs_stats_table_file;
	struct dentry *rs_sta_dbgfs_rate_scale_data_file;
	struct dentry *rs_sta_dbgfs_tx_agg_tid_en_file;
	u32 dbg_fixed_rate;
#endif
	struct il_priv *drv;

	
	int last_txrate_idx;
	
	u32 last_rate_n_flags;
	
	u8 is_agg;
};

struct il_station_priv {
	struct il_station_priv_common common;
	struct il_lq_sta lq_sta;
	atomic_t pending_frames;
	bool client;
	bool asleep;
};

static inline u8
il4965_num_of_ant(u8 m)
{
	return !!(m & ANT_A) + !!(m & ANT_B) + !!(m & ANT_C);
}

static inline u8
il4965_first_antenna(u8 mask)
{
	if (mask & ANT_A)
		return ANT_A;
	if (mask & ANT_B)
		return ANT_B;
	return ANT_C;
}

extern void il3945_rate_scale_init(struct ieee80211_hw *hw, s32 sta_id);

extern void il4965_rs_rate_init(struct il_priv *il, struct ieee80211_sta *sta,
				u8 sta_id);
extern void il3945_rs_rate_init(struct il_priv *il, struct ieee80211_sta *sta,
				u8 sta_id);

extern int il4965_rate_control_register(void);
extern int il3945_rate_control_register(void);

extern void il4965_rate_control_unregister(void);
extern void il3945_rate_control_unregister(void);

extern int il_power_update_mode(struct il_priv *il, bool force);
extern void il_power_initialize(struct il_priv *il);

extern u32 il_debug_level;

#ifdef CONFIG_IWLEGACY_DEBUG
static inline u32
il_get_debug_level(struct il_priv *il)
{
	if (il->debug_level)
		return il->debug_level;
	else
		return il_debug_level;
}
#else
static inline u32
il_get_debug_level(struct il_priv *il)
{
	return il_debug_level;
}
#endif

#define il_print_hex_error(il, p, len)					\
do {									\
	print_hex_dump(KERN_ERR, "iwl data: ",				\
		       DUMP_PREFIX_OFFSET, 16, 1, p, len, 1);		\
} while (0)

#ifdef CONFIG_IWLEGACY_DEBUG
#define IL_DBG(level, fmt, args...)					\
do {									\
	if (il_get_debug_level(il) & level)				\
		dev_printk(KERN_ERR, &il->hw->wiphy->dev,		\
			 "%c %s " fmt, in_interrupt() ? 'I' : 'U',	\
			__func__ , ## args);				\
} while (0)

#define il_print_hex_dump(il, level, p, len)				\
do {									\
	if (il_get_debug_level(il) & level)				\
		print_hex_dump(KERN_DEBUG, "iwl data: ",		\
			       DUMP_PREFIX_OFFSET, 16, 1, p, len, 1);	\
} while (0)

#else
#define IL_DBG(level, fmt, args...)
static inline void
il_print_hex_dump(struct il_priv *il, int level, const void *p, u32 len)
{
}
#endif 

#ifdef CONFIG_IWLEGACY_DEBUGFS
int il_dbgfs_register(struct il_priv *il, const char *name);
void il_dbgfs_unregister(struct il_priv *il);
#else
static inline int
il_dbgfs_register(struct il_priv *il, const char *name)
{
	return 0;
}

static inline void
il_dbgfs_unregister(struct il_priv *il)
{
}
#endif 


#define IL_DL_INFO		(1 << 0)
#define IL_DL_MAC80211		(1 << 1)
#define IL_DL_HCMD		(1 << 2)
#define IL_DL_STATE		(1 << 3)
#define IL_DL_MACDUMP		(1 << 4)
#define IL_DL_HCMD_DUMP		(1 << 5)
#define IL_DL_EEPROM		(1 << 6)
#define IL_DL_RADIO		(1 << 7)
#define IL_DL_POWER		(1 << 8)
#define IL_DL_TEMP		(1 << 9)
#define IL_DL_NOTIF		(1 << 10)
#define IL_DL_SCAN		(1 << 11)
#define IL_DL_ASSOC		(1 << 12)
#define IL_DL_DROP		(1 << 13)
#define IL_DL_TXPOWER		(1 << 14)
#define IL_DL_AP		(1 << 15)
#define IL_DL_FW		(1 << 16)
#define IL_DL_RF_KILL		(1 << 17)
#define IL_DL_FW_ERRORS		(1 << 18)
#define IL_DL_LED		(1 << 19)
#define IL_DL_RATE		(1 << 20)
#define IL_DL_CALIB		(1 << 21)
#define IL_DL_WEP		(1 << 22)
#define IL_DL_TX		(1 << 23)
#define IL_DL_RX		(1 << 24)
#define IL_DL_ISR		(1 << 25)
#define IL_DL_HT		(1 << 26)
#define IL_DL_11H		(1 << 28)
#define IL_DL_STATS		(1 << 29)
#define IL_DL_TX_REPLY		(1 << 30)
#define IL_DL_QOS		(1 << 31)

#define D_INFO(f, a...)		IL_DBG(IL_DL_INFO, f, ## a)
#define D_MAC80211(f, a...)	IL_DBG(IL_DL_MAC80211, f, ## a)
#define D_MACDUMP(f, a...)	IL_DBG(IL_DL_MACDUMP, f, ## a)
#define D_TEMP(f, a...)		IL_DBG(IL_DL_TEMP, f, ## a)
#define D_SCAN(f, a...)		IL_DBG(IL_DL_SCAN, f, ## a)
#define D_RX(f, a...)		IL_DBG(IL_DL_RX, f, ## a)
#define D_TX(f, a...)		IL_DBG(IL_DL_TX, f, ## a)
#define D_ISR(f, a...)		IL_DBG(IL_DL_ISR, f, ## a)
#define D_LED(f, a...)		IL_DBG(IL_DL_LED, f, ## a)
#define D_WEP(f, a...)		IL_DBG(IL_DL_WEP, f, ## a)
#define D_HC(f, a...)		IL_DBG(IL_DL_HCMD, f, ## a)
#define D_HC_DUMP(f, a...)	IL_DBG(IL_DL_HCMD_DUMP, f, ## a)
#define D_EEPROM(f, a...)	IL_DBG(IL_DL_EEPROM, f, ## a)
#define D_CALIB(f, a...)	IL_DBG(IL_DL_CALIB, f, ## a)
#define D_FW(f, a...)		IL_DBG(IL_DL_FW, f, ## a)
#define D_RF_KILL(f, a...)	IL_DBG(IL_DL_RF_KILL, f, ## a)
#define D_DROP(f, a...)		IL_DBG(IL_DL_DROP, f, ## a)
#define D_AP(f, a...)		IL_DBG(IL_DL_AP, f, ## a)
#define D_TXPOWER(f, a...)	IL_DBG(IL_DL_TXPOWER, f, ## a)
#define D_RATE(f, a...)		IL_DBG(IL_DL_RATE, f, ## a)
#define D_NOTIF(f, a...)	IL_DBG(IL_DL_NOTIF, f, ## a)
#define D_ASSOC(f, a...)	IL_DBG(IL_DL_ASSOC, f, ## a)
#define D_HT(f, a...)		IL_DBG(IL_DL_HT, f, ## a)
#define D_STATS(f, a...)	IL_DBG(IL_DL_STATS, f, ## a)
#define D_TX_REPLY(f, a...)	IL_DBG(IL_DL_TX_REPLY, f, ## a)
#define D_QOS(f, a...)		IL_DBG(IL_DL_QOS, f, ## a)
#define D_RADIO(f, a...)	IL_DBG(IL_DL_RADIO, f, ## a)
#define D_POWER(f, a...)	IL_DBG(IL_DL_POWER, f, ## a)
#define D_11H(f, a...)		IL_DBG(IL_DL_11H, f, ## a)

#endif 
