/******************************************************************************
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
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
 *****************************************************************************/

#ifndef __il_4965_h__
#define __il_4965_h__

struct il_rx_queue;
struct il_rx_buf;
struct il_rx_pkt;
struct il_tx_queue;
struct il_rxon_context;

extern struct il_cfg il4965_cfg;
extern const struct il_ops il4965_ops;

extern struct il_mod_params il4965_mod_params;

void il4965_free_tfds_in_queue(struct il_priv *il, int sta_id, int tid,
			       int freed);

void il4965_set_rxon_chain(struct il_priv *il);

int il4965_verify_ucode(struct il_priv *il);

void il4965_check_abort_status(struct il_priv *il, u8 frame_count, u32 status);

void il4965_rx_queue_reset(struct il_priv *il, struct il_rx_queue *rxq);
int il4965_rx_init(struct il_priv *il, struct il_rx_queue *rxq);
int il4965_hw_nic_init(struct il_priv *il);
int il4965_dump_fh(struct il_priv *il, char **buf, bool display);

void il4965_nic_config(struct il_priv *il);

void il4965_rx_queue_restock(struct il_priv *il);
void il4965_rx_replenish(struct il_priv *il);
void il4965_rx_replenish_now(struct il_priv *il);
void il4965_rx_queue_free(struct il_priv *il, struct il_rx_queue *rxq);
int il4965_rxq_stop(struct il_priv *il);
int il4965_hwrate_to_mac80211_idx(u32 rate_n_flags, enum ieee80211_band band);
void il4965_rx_handle(struct il_priv *il);

void il4965_hw_txq_free_tfd(struct il_priv *il, struct il_tx_queue *txq);
int il4965_hw_txq_attach_buf_to_tfd(struct il_priv *il, struct il_tx_queue *txq,
				    dma_addr_t addr, u16 len, u8 reset, u8 pad);
int il4965_hw_tx_queue_init(struct il_priv *il, struct il_tx_queue *txq);
void il4965_hwrate_to_tx_control(struct il_priv *il, u32 rate_n_flags,
				 struct ieee80211_tx_info *info);
int il4965_tx_skb(struct il_priv *il, struct sk_buff *skb);
int il4965_tx_agg_start(struct il_priv *il, struct ieee80211_vif *vif,
			struct ieee80211_sta *sta, u16 tid, u16 * ssn);
int il4965_tx_agg_stop(struct il_priv *il, struct ieee80211_vif *vif,
		       struct ieee80211_sta *sta, u16 tid);
int il4965_txq_check_empty(struct il_priv *il, int sta_id, u8 tid, int txq_id);
int il4965_tx_queue_reclaim(struct il_priv *il, int txq_id, int idx);
void il4965_hw_txq_ctx_free(struct il_priv *il);
int il4965_txq_ctx_alloc(struct il_priv *il);
void il4965_txq_ctx_reset(struct il_priv *il);
void il4965_txq_ctx_stop(struct il_priv *il);
void il4965_txq_set_sched(struct il_priv *il, u32 mask);

void il4965_set_wr_ptrs(struct il_priv *il, int txq_id, u32 idx);
void il4965_tx_queue_set_status(struct il_priv *il, struct il_tx_queue *txq,
				int tx_fifo_id, int scd_retry);

int il4965_request_scan(struct il_priv *il, struct ieee80211_vif *vif);

int il4965_manage_ibss_station(struct il_priv *il, struct ieee80211_vif *vif,
			       bool add);

int il4965_send_beacon_cmd(struct il_priv *il);

#ifdef CONFIG_IWLEGACY_DEBUG
const char *il4965_get_tx_fail_reason(u32 status);
#else
static inline const char *
il4965_get_tx_fail_reason(u32 status)
{
	return "";
}
#endif

int il4965_alloc_bcast_station(struct il_priv *il);
int il4965_add_bssid_station(struct il_priv *il, const u8 *addr, u8 *sta_id_r);
int il4965_remove_default_wep_key(struct il_priv *il,
				  struct ieee80211_key_conf *key);
int il4965_set_default_wep_key(struct il_priv *il,
			       struct ieee80211_key_conf *key);
int il4965_restore_default_wep_keys(struct il_priv *il);
int il4965_set_dynamic_key(struct il_priv *il,
			   struct ieee80211_key_conf *key, u8 sta_id);
int il4965_remove_dynamic_key(struct il_priv *il,
			      struct ieee80211_key_conf *key, u8 sta_id);
void il4965_update_tkip_key(struct il_priv *il,
			    struct ieee80211_key_conf *keyconf,
			    struct ieee80211_sta *sta, u32 iv32,
			    u16 *phase1key);
int il4965_sta_tx_modify_enable_tid(struct il_priv *il, int sta_id, int tid);
int il4965_sta_rx_agg_start(struct il_priv *il, struct ieee80211_sta *sta,
			    int tid, u16 ssn);
int il4965_sta_rx_agg_stop(struct il_priv *il, struct ieee80211_sta *sta,
			   int tid);
void il4965_sta_modify_sleep_tx_count(struct il_priv *il, int sta_id, int cnt);
int il4965_update_bcast_stations(struct il_priv *il);

static inline u8
il4965_hw_get_rate(__le32 rate_n_flags)
{
	return le32_to_cpu(rate_n_flags) & 0xFF;
}

void il4965_eeprom_get_mac(const struct il_priv *il, u8 * mac);
int il4965_eeprom_acquire_semaphore(struct il_priv *il);
void il4965_eeprom_release_semaphore(struct il_priv *il);
int il4965_eeprom_check_version(struct il_priv *il);

void il4965_mac_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
int il4965_mac_start(struct ieee80211_hw *hw);
void il4965_mac_stop(struct ieee80211_hw *hw);
void il4965_configure_filter(struct ieee80211_hw *hw,
			     unsigned int changed_flags,
			     unsigned int *total_flags, u64 multicast);
int il4965_mac_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
		       struct ieee80211_vif *vif, struct ieee80211_sta *sta,
		       struct ieee80211_key_conf *key);
void il4965_mac_update_tkip_key(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct ieee80211_key_conf *keyconf,
				struct ieee80211_sta *sta, u32 iv32,
				u16 *phase1key);
int il4965_mac_ampdu_action(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			    enum ieee80211_ampdu_mlme_action action,
			    struct ieee80211_sta *sta, u16 tid, u16 * ssn,
			    u8 buf_size);
int il4965_mac_sta_add(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		       struct ieee80211_sta *sta);
void il4965_mac_channel_switch(struct ieee80211_hw *hw,
			       struct ieee80211_channel_switch *ch_switch);

void il4965_led_enable(struct il_priv *il);

#define IL4965_EEPROM_IMG_SIZE			1024

#define IL49_FIRST_AMPDU_QUEUE	7

#define IL49_RTC_INST_LOWER_BOUND		(0x000000)
#define IL49_RTC_INST_UPPER_BOUND		(0x018000)

#define IL49_RTC_DATA_LOWER_BOUND		(0x800000)
#define IL49_RTC_DATA_UPPER_BOUND		(0x80A000)

#define IL49_RTC_INST_SIZE  (IL49_RTC_INST_UPPER_BOUND - \
				IL49_RTC_INST_LOWER_BOUND)
#define IL49_RTC_DATA_SIZE  (IL49_RTC_DATA_UPPER_BOUND - \
				IL49_RTC_DATA_LOWER_BOUND)

#define IL49_MAX_INST_SIZE IL49_RTC_INST_SIZE
#define IL49_MAX_DATA_SIZE IL49_RTC_DATA_SIZE

#define IL49_MAX_BSM_SIZE BSM_SRAM_SIZE

static inline int
il4965_hw_valid_rtc_data_addr(u32 addr)
{
	return (addr >= IL49_RTC_DATA_LOWER_BOUND &&
		addr < IL49_RTC_DATA_UPPER_BOUND);
}


#define TEMPERATURE_CALIB_KELVIN_OFFSET 8
#define TEMPERATURE_CALIB_A_VAL 259

#define IL_TX_POWER_TEMPERATURE_MIN  (263)
#define IL_TX_POWER_TEMPERATURE_MAX  (410)

#define IL_TX_POWER_TEMPERATURE_OUT_OF_RANGE(t) \
	((t) < IL_TX_POWER_TEMPERATURE_MIN || \
	 (t) > IL_TX_POWER_TEMPERATURE_MAX)

extern void il4965_temperature_calib(struct il_priv *il);




#define IL_TX_POWER_MIMO_REGULATORY_COMPENSATION (6)

#define IL_TX_POWER_CCK_COMPENSATION_B_STEP (9)
#define IL_TX_POWER_CCK_COMPENSATION_C_STEP (5)

#define TX_POWER_IL_VOLTAGE_CODES_PER_03V   (7)

#define MIN_TX_GAIN_IDX		(0)	
#define MIN_TX_GAIN_IDX_52GHZ_EXT	(-9)	



#define IL_TX_POWER_DEFAULT_REGULATORY_24   (34)
#define IL_TX_POWER_DEFAULT_REGULATORY_52   (34)
#define IL_TX_POWER_REGULATORY_MIN          (0)
#define IL_TX_POWER_REGULATORY_MAX          (34)

#define IL_TX_POWER_DEFAULT_SATURATION_24   (38)
#define IL_TX_POWER_DEFAULT_SATURATION_52   (38)
#define IL_TX_POWER_SATURATION_MIN          (20)
#define IL_TX_POWER_SATURATION_MAX          (50)

#define CALIB_IL_TX_ATTEN_GR1_FCH 34
#define CALIB_IL_TX_ATTEN_GR1_LCH 43

#define CALIB_IL_TX_ATTEN_GR2_FCH 44
#define CALIB_IL_TX_ATTEN_GR2_LCH 70

#define CALIB_IL_TX_ATTEN_GR3_FCH 71
#define CALIB_IL_TX_ATTEN_GR3_LCH 124

#define CALIB_IL_TX_ATTEN_GR4_FCH 125
#define CALIB_IL_TX_ATTEN_GR4_LCH 200

#define CALIB_IL_TX_ATTEN_GR5_FCH 1
#define CALIB_IL_TX_ATTEN_GR5_LCH 20

enum {
	CALIB_CH_GROUP_1 = 0,
	CALIB_CH_GROUP_2 = 1,
	CALIB_CH_GROUP_3 = 2,
	CALIB_CH_GROUP_4 = 3,
	CALIB_CH_GROUP_5 = 4,
	CALIB_CH_GROUP_MAX
};


/**
 * Tx/Rx Queues
 *
 * Most communication between driver and 4965 is via queues of data buffers.
 * For example, all commands that the driver issues to device's embedded
 * controller (uCode) are via the command queue (one of the Tx queues).  All
 * uCode command responses/replies/notifications, including Rx frames, are
 * conveyed from uCode to driver via the Rx queue.
 *
 * Most support for these queues, including handshake support, resides in
 * structures in host DRAM, shared between the driver and the device.  When
 * allocating this memory, the driver must make sure that data written by
 * the host CPU updates DRAM immediately (and does not get "stuck" in CPU's
 * cache memory), so DRAM and cache are consistent, and the device can
 * immediately see changes made by the driver.
 *
 * 4965 supports up to 16 DRAM-based Tx queues, and services these queues via
 * up to 7 DMA channels (FIFOs).  Each Tx queue is supported by a circular array
 * in DRAM containing 256 Transmit Frame Descriptors (TFDs).
 */
#define IL49_NUM_FIFOS	7
#define IL49_CMD_FIFO_NUM	4
#define IL49_NUM_QUEUES	16
#define IL49_NUM_AMPDU_QUEUES	8

struct il4965_scd_bc_tbl {
	__le16 tfd_offset[TFD_QUEUE_BC_SIZE];
	u8 pad[1024 - (TFD_QUEUE_BC_SIZE) * sizeof(__le16)];
} __packed;

#define IL4965_RTC_INST_LOWER_BOUND		(0x000000)

#define IL4965_RSSI_OFFSET	44

#define PCI_CFG_RETRY_TIMEOUT	0x041

#define PCI_CFG_LINK_CTRL_VAL_L0S_EN	0x01
#define PCI_CFG_LINK_CTRL_VAL_L1_EN	0x02

#define IL4965_DEFAULT_TX_RETRY  15

#define IL4965_FIRST_AMPDU_QUEUE	10

void il4965_chain_noise_calibration(struct il_priv *il, void *stat_resp);
void il4965_sensitivity_calibration(struct il_priv *il, void *resp);
void il4965_init_sensitivity(struct il_priv *il);
void il4965_reset_run_time_calib(struct il_priv *il);

#ifdef CONFIG_IWLEGACY_DEBUGFS
extern const struct il_debugfs_ops il4965_debugfs_ops;
#endif


#define FH49_MEM_LOWER_BOUND                   (0x1000)
#define FH49_MEM_UPPER_BOUND                   (0x2000)

#define FH49_KW_MEM_ADDR_REG		     (FH49_MEM_LOWER_BOUND + 0x97C)

#define FH49_MEM_CBBC_LOWER_BOUND          (FH49_MEM_LOWER_BOUND + 0x9D0)
#define FH49_MEM_CBBC_UPPER_BOUND          (FH49_MEM_LOWER_BOUND + 0xA10)

#define FH49_MEM_CBBC_QUEUE(x)  (FH49_MEM_CBBC_LOWER_BOUND + (x) * 0x4)

#define FH49_MEM_RSCSR_LOWER_BOUND	(FH49_MEM_LOWER_BOUND + 0xBC0)
#define FH49_MEM_RSCSR_UPPER_BOUND	(FH49_MEM_LOWER_BOUND + 0xC00)
#define FH49_MEM_RSCSR_CHNL0		(FH49_MEM_RSCSR_LOWER_BOUND)

#define FH49_RSCSR_CHNL0_STTS_WPTR_REG	(FH49_MEM_RSCSR_CHNL0)

#define FH49_RSCSR_CHNL0_RBDCB_BASE_REG	(FH49_MEM_RSCSR_CHNL0 + 0x004)

#define FH49_RSCSR_CHNL0_RBDCB_WPTR_REG	(FH49_MEM_RSCSR_CHNL0 + 0x008)
#define FH49_RSCSR_CHNL0_WPTR        (FH49_RSCSR_CHNL0_RBDCB_WPTR_REG)

#define FH49_MEM_RCSR_LOWER_BOUND      (FH49_MEM_LOWER_BOUND + 0xC00)
#define FH49_MEM_RCSR_UPPER_BOUND      (FH49_MEM_LOWER_BOUND + 0xCC0)
#define FH49_MEM_RCSR_CHNL0            (FH49_MEM_RCSR_LOWER_BOUND)

#define FH49_MEM_RCSR_CHNL0_CONFIG_REG	(FH49_MEM_RCSR_CHNL0)

#define FH49_RCSR_CHNL0_RX_CONFIG_RB_TIMEOUT_MSK (0x00000FF0)	
#define FH49_RCSR_CHNL0_RX_CONFIG_IRQ_DEST_MSK   (0x00001000)	
#define FH49_RCSR_CHNL0_RX_CONFIG_SINGLE_FRAME_MSK (0x00008000)	
#define FH49_RCSR_CHNL0_RX_CONFIG_RB_SIZE_MSK   (0x00030000)	
#define FH49_RCSR_CHNL0_RX_CONFIG_RBDBC_SIZE_MSK (0x00F00000)	
#define FH49_RCSR_CHNL0_RX_CONFIG_DMA_CHNL_EN_MSK (0xC0000000)	

#define FH49_RCSR_RX_CONFIG_RBDCB_SIZE_POS	(20)
#define FH49_RCSR_RX_CONFIG_REG_IRQ_RBTH_POS	(4)
#define RX_RB_TIMEOUT	(0x10)

#define FH49_RCSR_RX_CONFIG_CHNL_EN_PAUSE_VAL         (0x00000000)
#define FH49_RCSR_RX_CONFIG_CHNL_EN_PAUSE_EOF_VAL     (0x40000000)
#define FH49_RCSR_RX_CONFIG_CHNL_EN_ENABLE_VAL        (0x80000000)

#define FH49_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_4K    (0x00000000)
#define FH49_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_8K    (0x00010000)
#define FH49_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_12K   (0x00020000)
#define FH49_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_16K   (0x00030000)

#define FH49_RCSR_CHNL0_RX_IGNORE_RXF_EMPTY              (0x00000004)
#define FH49_RCSR_CHNL0_RX_CONFIG_IRQ_DEST_NO_INT_VAL    (0x00000000)
#define FH49_RCSR_CHNL0_RX_CONFIG_IRQ_DEST_INT_HOST_VAL  (0x00001000)

#define FH49_MEM_RSSR_LOWER_BOUND           (FH49_MEM_LOWER_BOUND + 0xC40)
#define FH49_MEM_RSSR_UPPER_BOUND           (FH49_MEM_LOWER_BOUND + 0xD00)

#define FH49_MEM_RSSR_SHARED_CTRL_REG       (FH49_MEM_RSSR_LOWER_BOUND)
#define FH49_MEM_RSSR_RX_STATUS_REG	(FH49_MEM_RSSR_LOWER_BOUND + 0x004)
#define FH49_MEM_RSSR_RX_ENABLE_ERR_IRQ2DRV\
					(FH49_MEM_RSSR_LOWER_BOUND + 0x008)

#define FH49_RSSR_CHNL0_RX_STATUS_CHNL_IDLE	(0x01000000)

#define FH49_MEM_TFDIB_REG1_ADDR_BITSHIFT	28

#define FH49_MEM_TFDIB_DRAM_ADDR_LSB_MSK      (0xFFFFFFFF)
#define FH49_TFDIB_LOWER_BOUND       (FH49_MEM_LOWER_BOUND + 0x900)
#define FH49_TFDIB_UPPER_BOUND       (FH49_MEM_LOWER_BOUND + 0x958)
#define FH49_TFDIB_CTRL0_REG(_chnl)  (FH49_TFDIB_LOWER_BOUND + 0x8 * (_chnl))
#define FH49_TFDIB_CTRL1_REG(_chnl)  (FH49_TFDIB_LOWER_BOUND + 0x8 * (_chnl) + 0x4)

#define FH49_TCSR_LOWER_BOUND  (FH49_MEM_LOWER_BOUND + 0xD00)
#define FH49_TCSR_UPPER_BOUND  (FH49_MEM_LOWER_BOUND + 0xE60)

#define FH49_TCSR_CHNL_NUM                            (7)
#define FH50_TCSR_CHNL_NUM                            (8)

#define FH49_TCSR_CHNL_TX_CONFIG_REG(_chnl)	\
		(FH49_TCSR_LOWER_BOUND + 0x20 * (_chnl))
#define FH49_TCSR_CHNL_TX_CREDIT_REG(_chnl)	\
		(FH49_TCSR_LOWER_BOUND + 0x20 * (_chnl) + 0x4)
#define FH49_TCSR_CHNL_TX_BUF_STS_REG(_chnl)	\
		(FH49_TCSR_LOWER_BOUND + 0x20 * (_chnl) + 0x8)

#define FH49_TCSR_TX_CONFIG_REG_VAL_MSG_MODE_TXF		(0x00000000)
#define FH49_TCSR_TX_CONFIG_REG_VAL_MSG_MODE_DRV		(0x00000001)

#define FH49_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_DISABLE	(0x00000000)
#define FH49_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_ENABLE	(0x00000008)

#define FH49_TCSR_TX_CONFIG_REG_VAL_CIRQ_HOST_NOINT	(0x00000000)
#define FH49_TCSR_TX_CONFIG_REG_VAL_CIRQ_HOST_ENDTFD	(0x00100000)
#define FH49_TCSR_TX_CONFIG_REG_VAL_CIRQ_HOST_IFTFD	(0x00200000)

#define FH49_TCSR_TX_CONFIG_REG_VAL_CIRQ_RTC_NOINT	(0x00000000)
#define FH49_TCSR_TX_CONFIG_REG_VAL_CIRQ_RTC_ENDTFD	(0x00400000)
#define FH49_TCSR_TX_CONFIG_REG_VAL_CIRQ_RTC_IFTFD	(0x00800000)

#define FH49_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_PAUSE	(0x00000000)
#define FH49_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_PAUSE_EOF	(0x40000000)
#define FH49_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE	(0x80000000)

#define FH49_TCSR_CHNL_TX_BUF_STS_REG_VAL_TFDB_EMPTY	(0x00000000)
#define FH49_TCSR_CHNL_TX_BUF_STS_REG_VAL_TFDB_WAIT	(0x00002000)
#define FH49_TCSR_CHNL_TX_BUF_STS_REG_VAL_TFDB_VALID	(0x00000003)

#define FH49_TCSR_CHNL_TX_BUF_STS_REG_POS_TB_NUM		(20)
#define FH49_TCSR_CHNL_TX_BUF_STS_REG_POS_TB_IDX		(12)

#define FH49_TSSR_LOWER_BOUND		(FH49_MEM_LOWER_BOUND + 0xEA0)
#define FH49_TSSR_UPPER_BOUND		(FH49_MEM_LOWER_BOUND + 0xEC0)

#define FH49_TSSR_TX_STATUS_REG		(FH49_TSSR_LOWER_BOUND + 0x010)

#define FH49_TSSR_TX_ERROR_REG		(FH49_TSSR_LOWER_BOUND + 0x018)

#define FH49_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(_chnl) ((1 << (_chnl)) << 16)

#define FH49_SRVC_CHNL		(9)
#define FH49_SRVC_LOWER_BOUND	(FH49_MEM_LOWER_BOUND + 0x9C8)
#define FH49_SRVC_UPPER_BOUND	(FH49_MEM_LOWER_BOUND + 0x9D0)
#define FH49_SRVC_CHNL_SRAM_ADDR_REG(_chnl) \
		(FH49_SRVC_LOWER_BOUND + ((_chnl) - 9) * 0x4)

#define FH49_TX_CHICKEN_BITS_REG	(FH49_MEM_LOWER_BOUND + 0xE98)
#define FH49_TX_CHICKEN_BITS_SCD_AUTO_RETRY_EN	(0x00000002)

#define IL_KW_SIZE 0x1000	

#endif 
