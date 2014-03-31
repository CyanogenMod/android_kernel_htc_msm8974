/*
	Copyright (C) 2010 Willow Garage <http://www.willowgarage.com>
	Copyright (C) 2004 - 2010 Ivo van Doorn <IvDoorn@gmail.com>
	Copyright (C) 2004 - 2009 Gertjan van Wingerde <gwingerde@gmail.com>
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef RT2X00_H
#define RT2X00_H

#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <linux/leds.h>
#include <linux/mutex.h>
#include <linux/etherdevice.h>
#include <linux/input-polldev.h>
#include <linux/kfifo.h>
#include <linux/hrtimer.h>

#include <net/mac80211.h>

#include "rt2x00debug.h"
#include "rt2x00dump.h"
#include "rt2x00leds.h"
#include "rt2x00reg.h"
#include "rt2x00queue.h"

#define DRV_VERSION	"2.3.0"
#define DRV_PROJECT	"http://rt2x00.serialmonkey.com"

#define DEBUG_PRINTK_MSG(__dev, __kernlvl, __lvl, __msg, __args...)	\
	printk(__kernlvl "%s -> %s: %s - " __msg,			\
	       wiphy_name((__dev)->hw->wiphy), __func__, __lvl, ##__args)

#define DEBUG_PRINTK_PROBE(__kernlvl, __lvl, __msg, __args...)	\
	printk(__kernlvl "%s -> %s: %s - " __msg,		\
	       KBUILD_MODNAME, __func__, __lvl, ##__args)

#ifdef CONFIG_RT2X00_DEBUG
#define DEBUG_PRINTK(__dev, __kernlvl, __lvl, __msg, __args...)	\
	DEBUG_PRINTK_MSG(__dev, __kernlvl, __lvl, __msg, ##__args)
#else
#define DEBUG_PRINTK(__dev, __kernlvl, __lvl, __msg, __args...)	\
	do { } while (0)
#endif 

#define PANIC(__dev, __msg, __args...) \
	DEBUG_PRINTK_MSG(__dev, KERN_CRIT, "Panic", __msg, ##__args)
#define ERROR(__dev, __msg, __args...)	\
	DEBUG_PRINTK_MSG(__dev, KERN_ERR, "Error", __msg, ##__args)
#define ERROR_PROBE(__msg, __args...) \
	DEBUG_PRINTK_PROBE(KERN_ERR, "Error", __msg, ##__args)
#define WARNING(__dev, __msg, __args...) \
	DEBUG_PRINTK(__dev, KERN_WARNING, "Warning", __msg, ##__args)
#define NOTICE(__dev, __msg, __args...) \
	DEBUG_PRINTK(__dev, KERN_NOTICE, "Notice", __msg, ##__args)
#define INFO(__dev, __msg, __args...) \
	DEBUG_PRINTK(__dev, KERN_INFO, "Info", __msg, ##__args)
#define DEBUG(__dev, __msg, __args...) \
	DEBUG_PRINTK(__dev, KERN_DEBUG, "Debug", __msg, ##__args)
#define EEPROM(__dev, __msg, __args...) \
	DEBUG_PRINTK(__dev, KERN_DEBUG, "EEPROM recovery", __msg, ##__args)

#define GET_DURATION(__size, __rate)	(((__size) * 8 * 10) / (__rate))
#define GET_DURATION_RES(__size, __rate)(((__size) * 8 * 10) % (__rate))

#define L2PAD_SIZE(__hdrlen)	(-(__hdrlen) & 3)

#define ALIGN_SIZE(__skb, __header) \
	(  ((unsigned long)((__skb)->data + (__header))) & 3 )

#define RT2X00_ALIGN_SIZE	4 
#define RT2X00_L2PAD_SIZE	8 

#define ACK_SIZE		14
#define IEEE80211_HEADER	24
#define PLCP			48
#define BEACON			100
#define PREAMBLE		144
#define SHORT_PREAMBLE		72
#define SLOT_TIME		20
#define SHORT_SLOT_TIME		9
#define SIFS			10
#define PIFS			( SIFS + SLOT_TIME )
#define SHORT_PIFS		( SIFS + SHORT_SLOT_TIME )
#define DIFS			( PIFS + SLOT_TIME )
#define SHORT_DIFS		( SHORT_PIFS + SHORT_SLOT_TIME )
#define EIFS			( SIFS + DIFS + \
				  GET_DURATION(IEEE80211_HEADER + ACK_SIZE, 10) )
#define SHORT_EIFS		( SIFS + SHORT_DIFS + \
				  GET_DURATION(IEEE80211_HEADER + ACK_SIZE, 10) )

struct avg_val {
	int avg;
	int avg_weight;
};

enum rt2x00_chip_intf {
	RT2X00_CHIP_INTF_PCI,
	RT2X00_CHIP_INTF_PCIE,
	RT2X00_CHIP_INTF_USB,
	RT2X00_CHIP_INTF_SOC,
};

struct rt2x00_chip {
	u16 rt;
#define RT2460		0x2460
#define RT2560		0x2560
#define RT2570		0x2570
#define RT2661		0x2661
#define RT2573		0x2573
#define RT2860		0x2860	
#define RT2872		0x2872	
#define RT2883		0x2883	
#define RT3070		0x3070
#define RT3071		0x3071
#define RT3090		0x3090	
#define RT3390		0x3390
#define RT3572		0x3572
#define RT3593		0x3593
#define RT3883		0x3883	
#define RT5390		0x5390  
#define RT5392		0x5392  

	u16 rf;
	u16 rev;

	enum rt2x00_chip_intf intf;
};

struct rf_channel {
	int channel;
	u32 rf1;
	u32 rf2;
	u32 rf3;
	u32 rf4;
};

struct channel_info {
	unsigned int flags;
#define GEOGRAPHY_ALLOWED	0x00000001

	short max_power;
	short default_power1;
	short default_power2;
};

struct antenna_setup {
	enum antenna rx;
	enum antenna tx;
	u8 rx_chain_num;
	u8 tx_chain_num;
};

struct link_qual {
	int rssi;
	int false_cca;

	u8 vgc_level;
	u8 vgc_level_reg;

	int rx_success;
	int rx_failed;
	int tx_success;
	int tx_failed;
};

struct link_ant {
	unsigned int flags;
#define ANTENNA_RX_DIVERSITY	0x00000001
#define ANTENNA_TX_DIVERSITY	0x00000002
#define ANTENNA_MODE_SAMPLE	0x00000004

	struct antenna_setup active;

	int rssi_history;

	struct avg_val rssi_ant;
};

struct link {
	u32 count;

	struct link_qual qual;

	struct link_ant ant;

	struct avg_val avg_rssi;

	struct delayed_work work;

	struct delayed_work watchdog_work;

	struct delayed_work agc_work;

	struct delayed_work vco_work;
};

enum rt2x00_delayed_flags {
	DELAYED_UPDATE_BEACON,
};

struct rt2x00_intf {
	struct mutex beacon_skb_mutex;

	struct queue_entry *beacon;
	bool enable_beacon;

	unsigned long delayed_flags;

	spinlock_t seqlock;
	u16 seqno;
};

static inline struct rt2x00_intf* vif_to_intf(struct ieee80211_vif *vif)
{
	return (struct rt2x00_intf *)vif->drv_priv;
}

struct hw_mode_spec {
	unsigned int supported_bands;
#define SUPPORT_BAND_2GHZ	0x00000001
#define SUPPORT_BAND_5GHZ	0x00000002

	unsigned int supported_rates;
#define SUPPORT_RATE_CCK	0x00000001
#define SUPPORT_RATE_OFDM	0x00000002

	unsigned int num_channels;
	const struct rf_channel *channels;
	const struct channel_info *channels_info;

	struct ieee80211_sta_ht_cap ht;
};

struct rt2x00lib_conf {
	struct ieee80211_conf *conf;

	struct rf_channel rf;
	struct channel_info channel;
};

struct rt2x00lib_erp {
	int short_preamble;
	int cts_protection;

	u32 basic_rates;

	int slot_time;

	short sifs;
	short pifs;
	short difs;
	short eifs;

	u16 beacon_int;
	u16 ht_opmode;
};

struct rt2x00lib_crypto {
	enum cipher cipher;

	enum set_key_cmd cmd;
	const u8 *address;

	u32 bssidx;

	u8 key[16];
	u8 tx_mic[8];
	u8 rx_mic[8];

	int wcid;
};

struct rt2x00intf_conf {
	enum nl80211_iftype type;

	enum tsf_sync sync;

	__le32 mac[2];
	__le32 bssid[2];
};

struct rt2x00_sta {
	int wcid;
};

static inline struct rt2x00_sta* sta_to_rt2x00_sta(struct ieee80211_sta *sta)
{
	return (struct rt2x00_sta *)sta->drv_priv;
}

struct rt2x00lib_ops {
	irq_handler_t irq_handler;

	void (*txstatus_tasklet) (unsigned long data);
	void (*pretbtt_tasklet) (unsigned long data);
	void (*tbtt_tasklet) (unsigned long data);
	void (*rxdone_tasklet) (unsigned long data);
	void (*autowake_tasklet) (unsigned long data);

	int (*probe_hw) (struct rt2x00_dev *rt2x00dev);
	char *(*get_firmware_name) (struct rt2x00_dev *rt2x00dev);
	int (*check_firmware) (struct rt2x00_dev *rt2x00dev,
			       const u8 *data, const size_t len);
	int (*load_firmware) (struct rt2x00_dev *rt2x00dev,
			      const u8 *data, const size_t len);

	int (*initialize) (struct rt2x00_dev *rt2x00dev);
	void (*uninitialize) (struct rt2x00_dev *rt2x00dev);

	bool (*get_entry_state) (struct queue_entry *entry);
	void (*clear_entry) (struct queue_entry *entry);

	int (*set_device_state) (struct rt2x00_dev *rt2x00dev,
				 enum dev_state state);
	int (*rfkill_poll) (struct rt2x00_dev *rt2x00dev);
	void (*link_stats) (struct rt2x00_dev *rt2x00dev,
			    struct link_qual *qual);
	void (*reset_tuner) (struct rt2x00_dev *rt2x00dev,
			     struct link_qual *qual);
	void (*link_tuner) (struct rt2x00_dev *rt2x00dev,
			    struct link_qual *qual, const u32 count);
	void (*gain_calibration) (struct rt2x00_dev *rt2x00dev);
	void (*vco_calibration) (struct rt2x00_dev *rt2x00dev);

	void (*watchdog) (struct rt2x00_dev *rt2x00dev);
	void (*start_queue) (struct data_queue *queue);
	void (*kick_queue) (struct data_queue *queue);
	void (*stop_queue) (struct data_queue *queue);
	void (*flush_queue) (struct data_queue *queue, bool drop);
	void (*tx_dma_done) (struct queue_entry *entry);

	void (*write_tx_desc) (struct queue_entry *entry,
			       struct txentry_desc *txdesc);
	void (*write_tx_data) (struct queue_entry *entry,
			       struct txentry_desc *txdesc);
	void (*write_beacon) (struct queue_entry *entry,
			      struct txentry_desc *txdesc);
	void (*clear_beacon) (struct queue_entry *entry);
	int (*get_tx_data_len) (struct queue_entry *entry);

	void (*fill_rxdone) (struct queue_entry *entry,
			     struct rxdone_entry_desc *rxdesc);

	int (*config_shared_key) (struct rt2x00_dev *rt2x00dev,
				  struct rt2x00lib_crypto *crypto,
				  struct ieee80211_key_conf *key);
	int (*config_pairwise_key) (struct rt2x00_dev *rt2x00dev,
				    struct rt2x00lib_crypto *crypto,
				    struct ieee80211_key_conf *key);
	void (*config_filter) (struct rt2x00_dev *rt2x00dev,
			       const unsigned int filter_flags);
	void (*config_intf) (struct rt2x00_dev *rt2x00dev,
			     struct rt2x00_intf *intf,
			     struct rt2x00intf_conf *conf,
			     const unsigned int flags);
#define CONFIG_UPDATE_TYPE		( 1 << 1 )
#define CONFIG_UPDATE_MAC		( 1 << 2 )
#define CONFIG_UPDATE_BSSID		( 1 << 3 )

	void (*config_erp) (struct rt2x00_dev *rt2x00dev,
			    struct rt2x00lib_erp *erp,
			    u32 changed);
	void (*config_ant) (struct rt2x00_dev *rt2x00dev,
			    struct antenna_setup *ant);
	void (*config) (struct rt2x00_dev *rt2x00dev,
			struct rt2x00lib_conf *libconf,
			const unsigned int changed_flags);
	int (*sta_add) (struct rt2x00_dev *rt2x00dev,
			struct ieee80211_vif *vif,
			struct ieee80211_sta *sta);
	int (*sta_remove) (struct rt2x00_dev *rt2x00dev,
			   int wcid);
};

struct rt2x00_ops {
	const char *name;
	const unsigned int drv_data_size;
	const unsigned int max_sta_intf;
	const unsigned int max_ap_intf;
	const unsigned int eeprom_size;
	const unsigned int rf_size;
	const unsigned int tx_queues;
	const unsigned int extra_tx_headroom;
	const struct data_queue_desc *rx;
	const struct data_queue_desc *tx;
	const struct data_queue_desc *bcn;
	const struct data_queue_desc *atim;
	const struct rt2x00lib_ops *lib;
	const void *drv;
	const struct ieee80211_ops *hw;
#ifdef CONFIG_RT2X00_LIB_DEBUGFS
	const struct rt2x00debug *debugfs;
#endif 
};

enum rt2x00_state_flags {
	DEVICE_STATE_PRESENT,
	DEVICE_STATE_REGISTERED_HW,
	DEVICE_STATE_INITIALIZED,
	DEVICE_STATE_STARTED,
	DEVICE_STATE_ENABLED_RADIO,
	DEVICE_STATE_SCANNING,

	CONFIG_CHANNEL_HT40,
	CONFIG_POWERSAVING,

	TX_STATUS_READING,
};

enum rt2x00_capability_flags {
	REQUIRE_FIRMWARE,
	REQUIRE_BEACON_GUARD,
	REQUIRE_ATIM_QUEUE,
	REQUIRE_DMA,
	REQUIRE_COPY_IV,
	REQUIRE_L2PAD,
	REQUIRE_TXSTATUS_FIFO,
	REQUIRE_TASKLET_CONTEXT,
	REQUIRE_SW_SEQNO,
	REQUIRE_HT_TX_DESC,
	REQUIRE_PS_AUTOWAKE,

	CAPABILITY_HW_BUTTON,
	CAPABILITY_HW_CRYPTO,
	CAPABILITY_POWER_LIMIT,
	CAPABILITY_CONTROL_FILTERS,
	CAPABILITY_CONTROL_FILTER_PSPOLL,
	CAPABILITY_PRE_TBTT_INTERRUPT,
	CAPABILITY_LINK_TUNING,
	CAPABILITY_FRAME_TYPE,
	CAPABILITY_RF_SEQUENCE,
	CAPABILITY_EXTERNAL_LNA_A,
	CAPABILITY_EXTERNAL_LNA_BG,
	CAPABILITY_DOUBLE_ANTENNA,
	CAPABILITY_BT_COEXIST,
	CAPABILITY_VCO_RECALIBRATION,
};

struct rt2x00_dev {
	struct device *dev;

	const struct rt2x00_ops *ops;

	void *drv_data;

	struct ieee80211_hw *hw;
	struct ieee80211_supported_band bands[IEEE80211_NUM_BANDS];
	enum ieee80211_band curr_band;
	int curr_freq;

#ifdef CONFIG_RT2X00_LIB_DEBUGFS
	struct rt2x00debug_intf *debugfs_intf;
#endif 

#ifdef CONFIG_RT2X00_LIB_LEDS
	struct rt2x00_led led_radio;
	struct rt2x00_led led_assoc;
	struct rt2x00_led led_qual;
	u16 led_mcu_reg;
#endif 

	unsigned long flags;

	unsigned long cap_flags;

	int irq;
	const char *name;

	struct rt2x00_chip chip;

	struct hw_mode_spec spec;

	struct antenna_setup default_ant;

	union csr {
		void __iomem *base;
		void *cache;
	} csr;

	struct mutex csr_mutex;

	unsigned int packet_filter;

	unsigned int intf_ap_count;
	unsigned int intf_sta_count;
	unsigned int intf_associated;
	unsigned int intf_beaconing;

	struct link link;

	__le16 *eeprom;

	u32 *rf;

	short lna_gain;

	u16 tx_power;

	u8 short_retry;
	u8 long_retry;

	u8 rssi_offset;

	u8 freq_offset;

	u16 aid;

	u16 beacon_int;

	unsigned long last_beacon;

	struct ieee80211_low_level_stats low_level_stats;

	struct workqueue_struct *workqueue;

	struct work_struct intf_work;

	struct work_struct rxdone_work;
	struct work_struct txdone_work;

	struct delayed_work autowakeup_work;
	struct work_struct sleep_work;

	unsigned int data_queues;
	struct data_queue *rx;
	struct data_queue *tx;
	struct data_queue *bcn;
	struct data_queue *atim;

	const struct firmware *fw;

	DECLARE_KFIFO_PTR(txstatus_fifo, u32);

	struct hrtimer txstatus_timer;

	struct tasklet_struct txstatus_tasklet;
	struct tasklet_struct pretbtt_tasklet;
	struct tasklet_struct tbtt_tasklet;
	struct tasklet_struct rxdone_tasklet;
	struct tasklet_struct autowake_tasklet;

	int rf_channel;

	spinlock_t irqmask_lock;
};

#define REGISTER_BUSY_COUNT	100
#define REGISTER_BUSY_DELAY	100

static inline void rt2x00_rf_read(struct rt2x00_dev *rt2x00dev,
				  const unsigned int word, u32 *data)
{
	BUG_ON(word < 1 || word > rt2x00dev->ops->rf_size / sizeof(u32));
	*data = rt2x00dev->rf[word - 1];
}

static inline void rt2x00_rf_write(struct rt2x00_dev *rt2x00dev,
				   const unsigned int word, u32 data)
{
	BUG_ON(word < 1 || word > rt2x00dev->ops->rf_size / sizeof(u32));
	rt2x00dev->rf[word - 1] = data;
}

static inline void *rt2x00_eeprom_addr(struct rt2x00_dev *rt2x00dev,
				       const unsigned int word)
{
	return (void *)&rt2x00dev->eeprom[word];
}

static inline void rt2x00_eeprom_read(struct rt2x00_dev *rt2x00dev,
				      const unsigned int word, u16 *data)
{
	*data = le16_to_cpu(rt2x00dev->eeprom[word]);
}

static inline void rt2x00_eeprom_write(struct rt2x00_dev *rt2x00dev,
				       const unsigned int word, u16 data)
{
	rt2x00dev->eeprom[word] = cpu_to_le16(data);
}

static inline void rt2x00_set_chip(struct rt2x00_dev *rt2x00dev,
				   const u16 rt, const u16 rf, const u16 rev)
{
	rt2x00dev->chip.rt = rt;
	rt2x00dev->chip.rf = rf;
	rt2x00dev->chip.rev = rev;

	INFO(rt2x00dev,
	     "Chipset detected - rt: %04x, rf: %04x, rev: %04x.\n",
	     rt2x00dev->chip.rt, rt2x00dev->chip.rf, rt2x00dev->chip.rev);
}

static inline bool rt2x00_rt(struct rt2x00_dev *rt2x00dev, const u16 rt)
{
	return (rt2x00dev->chip.rt == rt);
}

static inline bool rt2x00_rf(struct rt2x00_dev *rt2x00dev, const u16 rf)
{
	return (rt2x00dev->chip.rf == rf);
}

static inline u16 rt2x00_rev(struct rt2x00_dev *rt2x00dev)
{
	return rt2x00dev->chip.rev;
}

static inline bool rt2x00_rt_rev(struct rt2x00_dev *rt2x00dev,
				 const u16 rt, const u16 rev)
{
	return (rt2x00_rt(rt2x00dev, rt) && rt2x00_rev(rt2x00dev) == rev);
}

static inline bool rt2x00_rt_rev_lt(struct rt2x00_dev *rt2x00dev,
				    const u16 rt, const u16 rev)
{
	return (rt2x00_rt(rt2x00dev, rt) && rt2x00_rev(rt2x00dev) < rev);
}

static inline bool rt2x00_rt_rev_gte(struct rt2x00_dev *rt2x00dev,
				     const u16 rt, const u16 rev)
{
	return (rt2x00_rt(rt2x00dev, rt) && rt2x00_rev(rt2x00dev) >= rev);
}

static inline void rt2x00_set_chip_intf(struct rt2x00_dev *rt2x00dev,
					enum rt2x00_chip_intf intf)
{
	rt2x00dev->chip.intf = intf;
}

static inline bool rt2x00_intf(struct rt2x00_dev *rt2x00dev,
			       enum rt2x00_chip_intf intf)
{
	return (rt2x00dev->chip.intf == intf);
}

static inline bool rt2x00_is_pci(struct rt2x00_dev *rt2x00dev)
{
	return rt2x00_intf(rt2x00dev, RT2X00_CHIP_INTF_PCI) ||
	       rt2x00_intf(rt2x00dev, RT2X00_CHIP_INTF_PCIE);
}

static inline bool rt2x00_is_pcie(struct rt2x00_dev *rt2x00dev)
{
	return rt2x00_intf(rt2x00dev, RT2X00_CHIP_INTF_PCIE);
}

static inline bool rt2x00_is_usb(struct rt2x00_dev *rt2x00dev)
{
	return rt2x00_intf(rt2x00dev, RT2X00_CHIP_INTF_USB);
}

static inline bool rt2x00_is_soc(struct rt2x00_dev *rt2x00dev)
{
	return rt2x00_intf(rt2x00dev, RT2X00_CHIP_INTF_SOC);
}

void rt2x00queue_map_txskb(struct queue_entry *entry);

void rt2x00queue_unmap_skb(struct queue_entry *entry);

static inline struct data_queue *
rt2x00queue_get_tx_queue(struct rt2x00_dev *rt2x00dev,
			 const enum data_queue_qid queue)
{
	if (queue < rt2x00dev->ops->tx_queues && rt2x00dev->tx)
		return &rt2x00dev->tx[queue];

	if (queue == QID_ATIM)
		return rt2x00dev->atim;

	return NULL;
}

struct queue_entry *rt2x00queue_get_entry(struct data_queue *queue,
					  enum queue_index index);

void rt2x00queue_pause_queue(struct data_queue *queue);

void rt2x00queue_unpause_queue(struct data_queue *queue);

void rt2x00queue_start_queue(struct data_queue *queue);

void rt2x00queue_stop_queue(struct data_queue *queue);

void rt2x00queue_flush_queue(struct data_queue *queue, bool drop);

void rt2x00queue_start_queues(struct rt2x00_dev *rt2x00dev);

void rt2x00queue_stop_queues(struct rt2x00_dev *rt2x00dev);

void rt2x00queue_flush_queues(struct rt2x00_dev *rt2x00dev, bool drop);

#ifdef CONFIG_RT2X00_LIB_DEBUGFS
void rt2x00debug_dump_frame(struct rt2x00_dev *rt2x00dev,
			    enum rt2x00_dump_type type, struct sk_buff *skb);
#else
static inline void rt2x00debug_dump_frame(struct rt2x00_dev *rt2x00dev,
					  enum rt2x00_dump_type type,
					  struct sk_buff *skb)
{
}
#endif 

u32 rt2x00lib_get_bssidx(struct rt2x00_dev *rt2x00dev,
			 struct ieee80211_vif *vif);

void rt2x00lib_beacondone(struct rt2x00_dev *rt2x00dev);
void rt2x00lib_pretbtt(struct rt2x00_dev *rt2x00dev);
void rt2x00lib_dmastart(struct queue_entry *entry);
void rt2x00lib_dmadone(struct queue_entry *entry);
void rt2x00lib_txdone(struct queue_entry *entry,
		      struct txdone_entry_desc *txdesc);
void rt2x00lib_txdone_noinfo(struct queue_entry *entry, u32 status);
void rt2x00lib_rxdone(struct queue_entry *entry);

void rt2x00mac_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
int rt2x00mac_start(struct ieee80211_hw *hw);
void rt2x00mac_stop(struct ieee80211_hw *hw);
int rt2x00mac_add_interface(struct ieee80211_hw *hw,
			    struct ieee80211_vif *vif);
void rt2x00mac_remove_interface(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif);
int rt2x00mac_config(struct ieee80211_hw *hw, u32 changed);
void rt2x00mac_configure_filter(struct ieee80211_hw *hw,
				unsigned int changed_flags,
				unsigned int *total_flags,
				u64 multicast);
int rt2x00mac_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
		      bool set);
#ifdef CONFIG_RT2X00_LIB_CRYPTO
int rt2x00mac_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
		      struct ieee80211_vif *vif, struct ieee80211_sta *sta,
		      struct ieee80211_key_conf *key);
#else
#define rt2x00mac_set_key	NULL
#endif 
int rt2x00mac_sta_add(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		      struct ieee80211_sta *sta);
int rt2x00mac_sta_remove(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			 struct ieee80211_sta *sta);
void rt2x00mac_sw_scan_start(struct ieee80211_hw *hw);
void rt2x00mac_sw_scan_complete(struct ieee80211_hw *hw);
int rt2x00mac_get_stats(struct ieee80211_hw *hw,
			struct ieee80211_low_level_stats *stats);
void rt2x00mac_bss_info_changed(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct ieee80211_bss_conf *bss_conf,
				u32 changes);
int rt2x00mac_conf_tx(struct ieee80211_hw *hw,
		      struct ieee80211_vif *vif, u16 queue,
		      const struct ieee80211_tx_queue_params *params);
void rt2x00mac_rfkill_poll(struct ieee80211_hw *hw);
void rt2x00mac_flush(struct ieee80211_hw *hw, bool drop);
int rt2x00mac_set_antenna(struct ieee80211_hw *hw, u32 tx_ant, u32 rx_ant);
int rt2x00mac_get_antenna(struct ieee80211_hw *hw, u32 *tx_ant, u32 *rx_ant);
void rt2x00mac_get_ringparam(struct ieee80211_hw *hw,
			     u32 *tx, u32 *tx_max, u32 *rx, u32 *rx_max);
bool rt2x00mac_tx_frames_pending(struct ieee80211_hw *hw);

int rt2x00lib_probe_dev(struct rt2x00_dev *rt2x00dev);
void rt2x00lib_remove_dev(struct rt2x00_dev *rt2x00dev);
#ifdef CONFIG_PM
int rt2x00lib_suspend(struct rt2x00_dev *rt2x00dev, pm_message_t state);
int rt2x00lib_resume(struct rt2x00_dev *rt2x00dev);
#endif 

#endif 
