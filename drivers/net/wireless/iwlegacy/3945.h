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

#ifndef __il_3945_h__
#define __il_3945_h__

#include <linux/pci.h>		
#include <linux/kernel.h>
#include <net/ieee80211_radiotap.h>

extern const struct pci_device_id il3945_hw_card_ids[];

#include "common.h"

extern const struct il_ops il3945_ops;

#define IL3945_UCODE_API_MAX 2

#define IL3945_UCODE_API_MIN 1

#define IL3945_FW_PRE	"iwlwifi-3945-"
#define _IL3945_MODULE_FIRMWARE(api) IL3945_FW_PRE #api ".ucode"
#define IL3945_MODULE_FIRMWARE(api) _IL3945_MODULE_FIRMWARE(api)

#define IL_NOISE_MEAS_NOT_AVAILABLE (-127)

extern struct il_mod_params il3945_mod_params;

struct il3945_rate_scale_data {
	u64 data;
	s32 success_counter;
	s32 success_ratio;
	s32 counter;
	s32 average_tpt;
	unsigned long stamp;
};

struct il3945_rs_sta {
	spinlock_t lock;
	struct il_priv *il;
	s32 *expected_tpt;
	unsigned long last_partial_flush;
	unsigned long last_flush;
	u32 flush_time;
	u32 last_tx_packets;
	u32 tx_packets;
	u8 tgg;
	u8 flush_pending;
	u8 start_rate;
	struct timer_list rate_scale_flush;
	struct il3945_rate_scale_data win[RATE_COUNT_3945];
#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *rs_sta_dbgfs_stats_table_file;
#endif

	
	int last_txrate_idx;
};

struct il3945_sta_priv {
	struct il_station_priv_common common;
	struct il3945_rs_sta rs_sta;
};

enum il3945_antenna {
	IL_ANTENNA_DIVERSITY,
	IL_ANTENNA_MAIN,
	IL_ANTENNA_AUX
};

#define DEFAULT_RTS_THRESHOLD     2347U
#define MIN_RTS_THRESHOLD         0U
#define MAX_RTS_THRESHOLD         2347U
#define MAX_MSDU_SIZE		  2304U
#define MAX_MPDU_SIZE		  2346U
#define DEFAULT_BEACON_INTERVAL   100U
#define	DEFAULT_SHORT_RETRY_LIMIT 7U
#define	DEFAULT_LONG_RETRY_LIMIT  4U

#define IL_TX_FIFO_AC0	0
#define IL_TX_FIFO_AC1	1
#define IL_TX_FIFO_AC2	2
#define IL_TX_FIFO_AC3	3
#define IL_TX_FIFO_HCCA_1	5
#define IL_TX_FIFO_HCCA_2	6
#define IL_TX_FIFO_NONE	7

#define IEEE80211_DATA_LEN              2304
#define IEEE80211_4ADDR_LEN             30
#define IEEE80211_HLEN                  (IEEE80211_4ADDR_LEN)
#define IEEE80211_FRAME_LEN             (IEEE80211_DATA_LEN + IEEE80211_HLEN)

struct il3945_frame {
	union {
		struct ieee80211_hdr frame;
		struct il3945_tx_beacon_cmd beacon;
		u8 raw[IEEE80211_FRAME_LEN];
		u8 cmd[360];
	} u;
	struct list_head list;
};

#define SEQ_TO_SN(seq) (((seq) & IEEE80211_SCTL_SEQ) >> 4)
#define SN_TO_SEQ(ssn) (((ssn) << 4) & IEEE80211_SCTL_SEQ)
#define MAX_SN ((IEEE80211_SCTL_SEQ) >> 4)

#define SUP_RATE_11A_MAX_NUM_CHANNELS  8
#define SUP_RATE_11B_MAX_NUM_CHANNELS  4
#define SUP_RATE_11G_MAX_NUM_CHANNELS  12

#define IL_SUPPORTED_RATES_IE_LEN         8

#define SCAN_INTERVAL 100

#define MAX_TID_COUNT        9

#define IL_INVALID_RATE     0xFF
#define IL_INVALID_VALUE    -1

#define STA_PS_STATUS_WAKE             0
#define STA_PS_STATUS_SLEEP            1

struct il3945_ibss_seq {
	u8 mac[ETH_ALEN];
	u16 seq_num;
	u16 frag_num;
	unsigned long packet_time;
	struct list_head list;
};

#define IL_RX_HDR(x) ((struct il3945_rx_frame_hdr *)(\
		       x->u.rx_frame.stats.payload + \
		       x->u.rx_frame.stats.phy_count))
#define IL_RX_END(x) ((struct il3945_rx_frame_end *)(\
		       IL_RX_HDR(x)->payload + \
		       le16_to_cpu(IL_RX_HDR(x)->len)))
#define IL_RX_STATS(x) (&x->u.rx_frame.stats)
#define IL_RX_DATA(x) (IL_RX_HDR(x)->payload)

extern int il3945_calc_db_from_ratio(int sig_ratio);
extern void il3945_rx_replenish(void *data);
extern void il3945_rx_queue_reset(struct il_priv *il, struct il_rx_queue *rxq);
extern unsigned int il3945_fill_beacon_frame(struct il_priv *il,
					     struct ieee80211_hdr *hdr,
					     int left);
extern int il3945_dump_nic_event_log(struct il_priv *il, bool full_log,
				     char **buf, bool display);
extern void il3945_dump_nic_error_log(struct il_priv *il);

extern void il3945_hw_handler_setup(struct il_priv *il);
extern void il3945_hw_setup_deferred_work(struct il_priv *il);
extern void il3945_hw_cancel_deferred_work(struct il_priv *il);
extern int il3945_hw_rxq_stop(struct il_priv *il);
extern int il3945_hw_set_hw_params(struct il_priv *il);
extern int il3945_hw_nic_init(struct il_priv *il);
extern int il3945_hw_nic_stop_master(struct il_priv *il);
extern void il3945_hw_txq_ctx_free(struct il_priv *il);
extern void il3945_hw_txq_ctx_stop(struct il_priv *il);
extern int il3945_hw_nic_reset(struct il_priv *il);
extern int il3945_hw_txq_attach_buf_to_tfd(struct il_priv *il,
					   struct il_tx_queue *txq,
					   dma_addr_t addr, u16 len, u8 reset,
					   u8 pad);
extern void il3945_hw_txq_free_tfd(struct il_priv *il, struct il_tx_queue *txq);
extern int il3945_hw_get_temperature(struct il_priv *il);
extern int il3945_hw_tx_queue_init(struct il_priv *il, struct il_tx_queue *txq);
extern unsigned int il3945_hw_get_beacon_cmd(struct il_priv *il,
					     struct il3945_frame *frame,
					     u8 rate);
void il3945_hw_build_tx_cmd_rate(struct il_priv *il, struct il_device_cmd *cmd,
				 struct ieee80211_tx_info *info,
				 struct ieee80211_hdr *hdr, int sta_id);
extern int il3945_hw_reg_send_txpower(struct il_priv *il);
extern int il3945_hw_reg_set_txpower(struct il_priv *il, s8 power);
extern void il3945_hdl_stats(struct il_priv *il, struct il_rx_buf *rxb);
void il3945_hdl_c_stats(struct il_priv *il, struct il_rx_buf *rxb);
extern void il3945_disable_events(struct il_priv *il);
extern int il4965_get_temperature(const struct il_priv *il);
extern void il3945_post_associate(struct il_priv *il);
extern void il3945_config_ap(struct il_priv *il);

extern int il3945_commit_rxon(struct il_priv *il);

extern u8 il3945_hw_find_station(struct il_priv *il, const u8 * bssid);

extern __le32 il3945_get_antenna_flags(const struct il_priv *il);
extern int il3945_init_hw_rate_table(struct il_priv *il);
extern void il3945_reg_txpower_periodic(struct il_priv *il);
extern int il3945_txpower_set_from_eeprom(struct il_priv *il);

extern int il3945_rs_next_rate(struct il_priv *il, int rate);

int il3945_request_scan(struct il_priv *il, struct ieee80211_vif *vif);
void il3945_post_scan(struct il_priv *il);

extern const struct il3945_rate_info il3945_rates[RATE_COUNT_3945];

#define IL39_RSSI_OFFSET	95

#define EEPROM_SKU_CAP_OP_MODE_MRC                      (1 << 7)

struct il3945_eeprom_txpower_sample {
	u8 gain_idx;		
	s8 power;		
	u16 v_det;		
} __packed;

struct il3945_eeprom_txpower_group {
	struct il3945_eeprom_txpower_sample samples[5];	
	s32 a, b, c, d, e;	
	s32 Fa, Fb, Fc, Fd, Fe;	
	s8 saturation_power;	
	u8 group_channel;	
	s16 temperature;	
} __packed;

struct il3945_eeprom_temperature_corr {
	u32 Ta;
	u32 Tb;
	u32 Tc;
	u32 Td;
	u32 Te;
} __packed;

struct il3945_eeprom {
	u8 reserved0[16];
	u16 device_id;		
	u8 reserved1[2];
	u16 pmc;		
	u8 reserved2[20];
	u8 mac_address[6];	
	u8 reserved3[58];
	u16 board_revision;	
	u8 reserved4[11];
	u8 board_pba_number[9];	
	u8 reserved5[8];
	u16 version;		
	u8 sku_cap;		
	u8 leds_mode;		
	u16 oem_mode;
	u16 wowlan_mode;	
	u16 leds_time_interval;	
	u8 leds_off_time;	
	u8 leds_on_time;	
	u8 almgor_m_version;	
	u8 antenna_switch_type;	
	u8 reserved6[42];
	u8 sku_id[4];		

	u16 band_1_count;	
	struct il_eeprom_channel band_1_channels[14];	

	u16 band_2_count;	
	struct il_eeprom_channel band_2_channels[13];	

	u16 band_3_count;	
	struct il_eeprom_channel band_3_channels[12];	

	u16 band_4_count;	
	struct il_eeprom_channel band_4_channels[11];	

	u16 band_5_count;	
	struct il_eeprom_channel band_5_channels[6];	

	u8 reserved9[194];

#define IL_NUM_TX_CALIB_GROUPS 5
	struct il3945_eeprom_txpower_group groups[IL_NUM_TX_CALIB_GROUPS];
	struct il3945_eeprom_temperature_corr corrections;	
	u8 reserved16[172];	
} __packed;

#define IL3945_EEPROM_IMG_SIZE 1024


#define PCI_CFG_REV_ID_BIT_BASIC_SKU                (0x40)	
#define PCI_CFG_REV_ID_BIT_RTP                      (0x80)	

#define IL39_NUM_QUEUES        5
#define IL39_CMD_QUEUE_NUM	4

#define IL_DEFAULT_TX_RETRY  15


#define RFD_SIZE                              4
#define NUM_TFD_CHUNKS                        4

#define TFD_CTL_COUNT_SET(n)       (n << 24)
#define TFD_CTL_COUNT_GET(ctl)     ((ctl >> 24) & 7)
#define TFD_CTL_PAD_SET(n)         (n << 28)
#define TFD_CTL_PAD_GET(ctl)       (ctl >> 28)

#define IL39_RTC_INST_LOWER_BOUND		(0x000000)
#define IL39_RTC_INST_UPPER_BOUND		(0x014000)

#define IL39_RTC_DATA_LOWER_BOUND		(0x800000)
#define IL39_RTC_DATA_UPPER_BOUND		(0x808000)

#define IL39_RTC_INST_SIZE (IL39_RTC_INST_UPPER_BOUND - \
				IL39_RTC_INST_LOWER_BOUND)
#define IL39_RTC_DATA_SIZE (IL39_RTC_DATA_UPPER_BOUND - \
				IL39_RTC_DATA_LOWER_BOUND)

#define IL39_MAX_INST_SIZE IL39_RTC_INST_SIZE
#define IL39_MAX_DATA_SIZE IL39_RTC_DATA_SIZE

#define IL39_MAX_BSM_SIZE IL39_RTC_INST_SIZE

static inline int
il3945_hw_valid_rtc_data_addr(u32 addr)
{
	return (addr >= IL39_RTC_DATA_LOWER_BOUND &&
		addr < IL39_RTC_DATA_UPPER_BOUND);
}

struct il3945_shared {
	__le32 tx_base_ptr[8];
} __packed;


#define FH39_MEM_LOWER_BOUND                   (0x0800)
#define FH39_MEM_UPPER_BOUND                   (0x1000)

#define FH39_CBCC_TBL		(FH39_MEM_LOWER_BOUND + 0x140)
#define FH39_TFDB_TBL		(FH39_MEM_LOWER_BOUND + 0x180)
#define FH39_RCSR_TBL		(FH39_MEM_LOWER_BOUND + 0x400)
#define FH39_RSSR_TBL		(FH39_MEM_LOWER_BOUND + 0x4c0)
#define FH39_TCSR_TBL		(FH39_MEM_LOWER_BOUND + 0x500)
#define FH39_TSSR_TBL		(FH39_MEM_LOWER_BOUND + 0x680)

#define FH39_TFDB(_ch, buf)			(FH39_TFDB_TBL + \
						 ((_ch) * 2 + (buf)) * 0x28)
#define FH39_TFDB_CHNL_BUF_CTRL_REG(_ch)	(FH39_TFDB_TBL + 0x50 * (_ch))

#define FH39_CBCC(_ch)		(FH39_CBCC_TBL + (_ch) * 0x8)
#define FH39_CBCC_CTRL(_ch)	(FH39_CBCC(_ch) + 0x00)
#define FH39_CBCC_BASE(_ch)	(FH39_CBCC(_ch) + 0x04)

#define FH39_RCSR(_ch)			(FH39_RCSR_TBL + (_ch) * 0x40)
#define FH39_RCSR_CONFIG(_ch)		(FH39_RCSR(_ch) + 0x00)
#define FH39_RCSR_RBD_BASE(_ch)		(FH39_RCSR(_ch) + 0x04)
#define FH39_RCSR_WPTR(_ch)		(FH39_RCSR(_ch) + 0x20)
#define FH39_RCSR_RPTR_ADDR(_ch)	(FH39_RCSR(_ch) + 0x24)

#define FH39_RSCSR_CHNL0_WPTR		(FH39_RCSR_WPTR(0))

#define FH39_RSSR_CTRL			(FH39_RSSR_TBL + 0x000)
#define FH39_RSSR_STATUS		(FH39_RSSR_TBL + 0x004)

#define FH39_TCSR(_ch)			(FH39_TCSR_TBL + (_ch) * 0x20)
#define FH39_TCSR_CONFIG(_ch)		(FH39_TCSR(_ch) + 0x00)
#define FH39_TCSR_CREDIT(_ch)		(FH39_TCSR(_ch) + 0x04)
#define FH39_TCSR_BUFF_STTS(_ch)	(FH39_TCSR(_ch) + 0x08)

#define FH39_TSSR_CBB_BASE        (FH39_TSSR_TBL + 0x000)
#define FH39_TSSR_MSG_CONFIG      (FH39_TSSR_TBL + 0x008)
#define FH39_TSSR_TX_STATUS       (FH39_TSSR_TBL + 0x010)


#define FH39_SRVC_CHNL                            (6)

#define FH39_RCSR_RX_CONFIG_REG_POS_RBDC_SIZE     (20)
#define FH39_RCSR_RX_CONFIG_REG_POS_IRQ_RBTH      (4)

#define FH39_RCSR_RX_CONFIG_REG_BIT_WR_STTS_EN    (0x08000000)

#define FH39_RCSR_RX_CONFIG_REG_VAL_DMA_CHNL_EN_ENABLE        (0x80000000)

#define FH39_RCSR_RX_CONFIG_REG_VAL_RDRBD_EN_ENABLE           (0x20000000)

#define FH39_RCSR_RX_CONFIG_REG_VAL_MAX_FRAG_SIZE_128		(0x01000000)

#define FH39_RCSR_RX_CONFIG_REG_VAL_IRQ_DEST_INT_HOST		(0x00001000)

#define FH39_RCSR_RX_CONFIG_REG_VAL_MSG_MODE_FH			(0x00000000)

#define FH39_TCSR_TX_CONFIG_REG_VAL_MSG_MODE_TXF		(0x00000000)
#define FH39_TCSR_TX_CONFIG_REG_VAL_MSG_MODE_DRIVER		(0x00000001)

#define FH39_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_DISABLE_VAL	(0x00000000)
#define FH39_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_ENABLE_VAL	(0x00000008)

#define FH39_TCSR_TX_CONFIG_REG_VAL_CIRQ_HOST_IFTFD		(0x00200000)

#define FH39_TCSR_TX_CONFIG_REG_VAL_CIRQ_RTC_NOINT		(0x00000000)

#define FH39_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_PAUSE		(0x00000000)
#define FH39_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE		(0x80000000)

#define FH39_TCSR_CHNL_TX_BUF_STS_REG_VAL_TFDB_VALID		(0x00004000)

#define FH39_TCSR_CHNL_TX_BUF_STS_REG_BIT_TFDB_WPTR		(0x00000001)

#define FH39_TSSR_TX_MSG_CONFIG_REG_VAL_SNOOP_RD_TXPD_ON	(0xFF000000)
#define FH39_TSSR_TX_MSG_CONFIG_REG_VAL_ORDER_RD_TXPD_ON	(0x00FF0000)

#define FH39_TSSR_TX_MSG_CONFIG_REG_VAL_MAX_FRAG_SIZE_128B	(0x00000400)

#define FH39_TSSR_TX_MSG_CONFIG_REG_VAL_SNOOP_RD_TFD_ON		(0x00000100)
#define FH39_TSSR_TX_MSG_CONFIG_REG_VAL_ORDER_RD_CBB_ON		(0x00000080)

#define FH39_TSSR_TX_MSG_CONFIG_REG_VAL_ORDER_RSP_WAIT_TH	(0x00000020)
#define FH39_TSSR_TX_MSG_CONFIG_REG_VAL_RSP_WAIT_TH		(0x00000005)

#define FH39_TSSR_TX_STATUS_REG_BIT_BUFS_EMPTY(_ch)	(BIT(_ch) << 24)
#define FH39_TSSR_TX_STATUS_REG_BIT_NO_PEND_REQ(_ch)	(BIT(_ch) << 16)

#define FH39_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(_ch) \
	(FH39_TSSR_TX_STATUS_REG_BIT_BUFS_EMPTY(_ch) | \
	 FH39_TSSR_TX_STATUS_REG_BIT_NO_PEND_REQ(_ch))

#define FH39_RSSR_CHNL0_RX_STATUS_CHNL_IDLE			(0x01000000)

struct il3945_tfd_tb {
	__le32 addr;
	__le32 len;
} __packed;

struct il3945_tfd {
	__le32 control_flags;
	struct il3945_tfd_tb tbs[4];
	u8 __pad[28];
} __packed;

#ifdef CONFIG_IWLEGACY_DEBUGFS
extern const struct il_debugfs_ops il3945_debugfs_ops;
#endif

#endif
