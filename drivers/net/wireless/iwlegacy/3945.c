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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/firmware.h>
#include <linux/etherdevice.h>
#include <asm/unaligned.h>
#include <net/mac80211.h>

#include "common.h"
#include "3945.h"

static int
il3945_send_led_cmd(struct il_priv *il, struct il_led_cmd *led_cmd)
{
	struct il_host_cmd cmd = {
		.id = C_LEDS,
		.len = sizeof(struct il_led_cmd),
		.data = led_cmd,
		.flags = CMD_ASYNC,
		.callback = NULL,
	};

	return il_send_cmd(il, &cmd);
}

#define IL_DECLARE_RATE_INFO(r, ip, in, rp, rn, pp, np)    \
	[RATE_##r##M_IDX] = { RATE_##r##M_PLCP,   \
				    RATE_##r##M_IEEE,   \
				    RATE_##ip##M_IDX, \
				    RATE_##in##M_IDX, \
				    RATE_##rp##M_IDX, \
				    RATE_##rn##M_IDX, \
				    RATE_##pp##M_IDX, \
				    RATE_##np##M_IDX, \
				    RATE_##r##M_IDX_TBL, \
				    RATE_##ip##M_IDX_TBL }

const struct il3945_rate_info il3945_rates[RATE_COUNT_3945] = {
	IL_DECLARE_RATE_INFO(1, INV, 2, INV, 2, INV, 2),	
	IL_DECLARE_RATE_INFO(2, 1, 5, 1, 5, 1, 5),	
	IL_DECLARE_RATE_INFO(5, 2, 6, 2, 11, 2, 11),	
	IL_DECLARE_RATE_INFO(11, 9, 12, 5, 12, 5, 18),	
	IL_DECLARE_RATE_INFO(6, 5, 9, 5, 11, 5, 11),	
	IL_DECLARE_RATE_INFO(9, 6, 11, 5, 11, 5, 11),	
	IL_DECLARE_RATE_INFO(12, 11, 18, 11, 18, 11, 18),	
	IL_DECLARE_RATE_INFO(18, 12, 24, 12, 24, 11, 24),	
	IL_DECLARE_RATE_INFO(24, 18, 36, 18, 36, 18, 36),	
	IL_DECLARE_RATE_INFO(36, 24, 48, 24, 48, 24, 48),	
	IL_DECLARE_RATE_INFO(48, 36, 54, 36, 54, 36, 54),	
	IL_DECLARE_RATE_INFO(54, 48, INV, 48, INV, 48, INV),	
};

static inline u8
il3945_get_prev_ieee_rate(u8 rate_idx)
{
	u8 rate = il3945_rates[rate_idx].prev_ieee;

	if (rate == RATE_INVALID)
		rate = rate_idx;
	return rate;
}

#define IL_EVT_DISABLE (0)
#define IL_EVT_DISABLE_SIZE (1532/32)

void
il3945_disable_events(struct il_priv *il)
{
	int i;
	u32 base;		
	u32 disable_ptr;	
	u32 array_size;		
	static const u32 evt_disable[IL_EVT_DISABLE_SIZE] = {
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
		0x00000000,	
	};

	base = le32_to_cpu(il->card_alive.log_event_table_ptr);
	if (!il3945_hw_valid_rtc_data_addr(base)) {
		IL_ERR("Invalid event log pointer 0x%08X\n", base);
		return;
	}

	disable_ptr = il_read_targ_mem(il, base + (4 * sizeof(u32)));
	array_size = il_read_targ_mem(il, base + (5 * sizeof(u32)));

	if (IL_EVT_DISABLE && array_size == IL_EVT_DISABLE_SIZE) {
		D_INFO("Disabling selected uCode log events at 0x%x\n",
		       disable_ptr);
		for (i = 0; i < IL_EVT_DISABLE_SIZE; i++)
			il_write_targ_mem(il, disable_ptr + (i * sizeof(u32)),
					  evt_disable[i]);

	} else {
		D_INFO("Selected uCode log events may be disabled\n");
		D_INFO("  by writing \"1\"s into disable bitmap\n");
		D_INFO("  in SRAM at 0x%x, size %d u32s\n", disable_ptr,
		       array_size);
	}

}

static int
il3945_hwrate_to_plcp_idx(u8 plcp)
{
	int idx;

	for (idx = 0; idx < RATE_COUNT_3945; idx++)
		if (il3945_rates[idx].plcp == plcp)
			return idx;
	return -1;
}

#ifdef CONFIG_IWLEGACY_DEBUG
#define TX_STATUS_ENTRY(x) case TX_3945_STATUS_FAIL_ ## x: return #x

static const char *
il3945_get_tx_fail_reason(u32 status)
{
	switch (status & TX_STATUS_MSK) {
	case TX_3945_STATUS_SUCCESS:
		return "SUCCESS";
		TX_STATUS_ENTRY(SHORT_LIMIT);
		TX_STATUS_ENTRY(LONG_LIMIT);
		TX_STATUS_ENTRY(FIFO_UNDERRUN);
		TX_STATUS_ENTRY(MGMNT_ABORT);
		TX_STATUS_ENTRY(NEXT_FRAG);
		TX_STATUS_ENTRY(LIFE_EXPIRE);
		TX_STATUS_ENTRY(DEST_PS);
		TX_STATUS_ENTRY(ABORTED);
		TX_STATUS_ENTRY(BT_RETRY);
		TX_STATUS_ENTRY(STA_INVALID);
		TX_STATUS_ENTRY(FRAG_DROPPED);
		TX_STATUS_ENTRY(TID_DISABLE);
		TX_STATUS_ENTRY(FRAME_FLUSHED);
		TX_STATUS_ENTRY(INSUFFICIENT_CF_POLL);
		TX_STATUS_ENTRY(TX_LOCKED);
		TX_STATUS_ENTRY(NO_BEACON_ON_RADAR);
	}

	return "UNKNOWN";
}
#else
static inline const char *
il3945_get_tx_fail_reason(u32 status)
{
	return "";
}
#endif

int
il3945_rs_next_rate(struct il_priv *il, int rate)
{
	int next_rate = il3945_get_prev_ieee_rate(rate);

	switch (il->band) {
	case IEEE80211_BAND_5GHZ:
		if (rate == RATE_12M_IDX)
			next_rate = RATE_9M_IDX;
		else if (rate == RATE_6M_IDX)
			next_rate = RATE_6M_IDX;
		break;
	case IEEE80211_BAND_2GHZ:
		if (!(il->_3945.sta_supp_rates & IL_OFDM_RATES_MASK) &&
		    il_is_associated(il)) {
			if (rate == RATE_11M_IDX)
				next_rate = RATE_5M_IDX;
		}
		break;

	default:
		break;
	}

	return next_rate;
}

static void
il3945_tx_queue_reclaim(struct il_priv *il, int txq_id, int idx)
{
	struct il_tx_queue *txq = &il->txq[txq_id];
	struct il_queue *q = &txq->q;
	struct sk_buff *skb;

	BUG_ON(txq_id == IL39_CMD_QUEUE_NUM);

	for (idx = il_queue_inc_wrap(idx, q->n_bd); q->read_ptr != idx;
	     q->read_ptr = il_queue_inc_wrap(q->read_ptr, q->n_bd)) {

		skb = txq->skbs[txq->q.read_ptr];
		ieee80211_tx_status_irqsafe(il->hw, skb);
		txq->skbs[txq->q.read_ptr] = NULL;
		il->ops->txq_free_tfd(il, txq);
	}

	if (il_queue_space(q) > q->low_mark && txq_id >= 0 &&
	    txq_id != IL39_CMD_QUEUE_NUM && il->mac80211_registered)
		il_wake_queue(il, txq);
}

static void
il3945_hdl_tx(struct il_priv *il, struct il_rx_buf *rxb)
{
	struct il_rx_pkt *pkt = rxb_addr(rxb);
	u16 sequence = le16_to_cpu(pkt->hdr.sequence);
	int txq_id = SEQ_TO_QUEUE(sequence);
	int idx = SEQ_TO_IDX(sequence);
	struct il_tx_queue *txq = &il->txq[txq_id];
	struct ieee80211_tx_info *info;
	struct il3945_tx_resp *tx_resp = (void *)&pkt->u.raw[0];
	u32 status = le32_to_cpu(tx_resp->status);
	int rate_idx;
	int fail;

	if (idx >= txq->q.n_bd || il_queue_used(&txq->q, idx) == 0) {
		IL_ERR("Read idx for DMA queue txq_id (%d) idx %d "
		       "is out of range [0-%d] %d %d\n", txq_id, idx,
		       txq->q.n_bd, txq->q.write_ptr, txq->q.read_ptr);
		return;
	}

	txq->time_stamp = jiffies;
	info = IEEE80211_SKB_CB(txq->skbs[txq->q.read_ptr]);
	ieee80211_tx_info_clear_status(info);

	
	rate_idx = il3945_hwrate_to_plcp_idx(tx_resp->rate);
	if (info->band == IEEE80211_BAND_5GHZ)
		rate_idx -= IL_FIRST_OFDM_RATE;

	fail = tx_resp->failure_frame;

	info->status.rates[0].idx = rate_idx;
	info->status.rates[0].count = fail + 1;	

	
	info->flags |=
	    ((status & TX_STATUS_MSK) ==
	     TX_STATUS_SUCCESS) ? IEEE80211_TX_STAT_ACK : 0;

	D_TX("Tx queue %d Status %s (0x%08x) plcp rate %d retries %d\n", txq_id,
	     il3945_get_tx_fail_reason(status), status, tx_resp->rate,
	     tx_resp->failure_frame);

	D_TX_REPLY("Tx queue reclaim %d\n", idx);
	il3945_tx_queue_reclaim(il, txq_id, idx);

	if (status & TX_ABORT_REQUIRED_MSK)
		IL_ERR("TODO:  Implement Tx ABORT REQUIRED!!!\n");
}

#ifdef CONFIG_IWLEGACY_DEBUGFS
static void
il3945_accumulative_stats(struct il_priv *il, __le32 * stats)
{
	int i;
	__le32 *prev_stats;
	u32 *accum_stats;
	u32 *delta, *max_delta;

	prev_stats = (__le32 *) &il->_3945.stats;
	accum_stats = (u32 *) &il->_3945.accum_stats;
	delta = (u32 *) &il->_3945.delta_stats;
	max_delta = (u32 *) &il->_3945.max_delta;

	for (i = sizeof(__le32); i < sizeof(struct il3945_notif_stats);
	     i +=
	     sizeof(__le32), stats++, prev_stats++, delta++, max_delta++,
	     accum_stats++) {
		if (le32_to_cpu(*stats) > le32_to_cpu(*prev_stats)) {
			*delta =
			    (le32_to_cpu(*stats) - le32_to_cpu(*prev_stats));
			*accum_stats += *delta;
			if (*delta > *max_delta)
				*max_delta = *delta;
		}
	}

	
	il->_3945.accum_stats.general.temperature =
	    il->_3945.stats.general.temperature;
	il->_3945.accum_stats.general.ttl_timestamp =
	    il->_3945.stats.general.ttl_timestamp;
}
#endif

void
il3945_hdl_stats(struct il_priv *il, struct il_rx_buf *rxb)
{
	struct il_rx_pkt *pkt = rxb_addr(rxb);

	D_RX("Statistics notification received (%d vs %d).\n",
	     (int)sizeof(struct il3945_notif_stats),
	     le32_to_cpu(pkt->len_n_flags) & IL_RX_FRAME_SIZE_MSK);
#ifdef CONFIG_IWLEGACY_DEBUGFS
	il3945_accumulative_stats(il, (__le32 *) &pkt->u.raw);
#endif

	memcpy(&il->_3945.stats, pkt->u.raw, sizeof(il->_3945.stats));
}

void
il3945_hdl_c_stats(struct il_priv *il, struct il_rx_buf *rxb)
{
	struct il_rx_pkt *pkt = rxb_addr(rxb);
	__le32 *flag = (__le32 *) &pkt->u.raw;

	if (le32_to_cpu(*flag) & UCODE_STATS_CLEAR_MSK) {
#ifdef CONFIG_IWLEGACY_DEBUGFS
		memset(&il->_3945.accum_stats, 0,
		       sizeof(struct il3945_notif_stats));
		memset(&il->_3945.delta_stats, 0,
		       sizeof(struct il3945_notif_stats));
		memset(&il->_3945.max_delta, 0,
		       sizeof(struct il3945_notif_stats));
#endif
		D_RX("Statistics have been cleared\n");
	}
	il3945_hdl_stats(il, rxb);
}


static int
il3945_is_network_packet(struct il_priv *il, struct ieee80211_hdr *header)
{
	switch (il->iw_mode) {
	case NL80211_IFTYPE_ADHOC:	
		
		return !compare_ether_addr(header->addr3, il->bssid);
	case NL80211_IFTYPE_STATION:	
		
		return !compare_ether_addr(header->addr2, il->bssid);
	default:
		return 1;
	}
}

static void
il3945_pass_packet_to_mac80211(struct il_priv *il, struct il_rx_buf *rxb,
			       struct ieee80211_rx_status *stats)
{
	struct il_rx_pkt *pkt = rxb_addr(rxb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)IL_RX_DATA(pkt);
	struct il3945_rx_frame_hdr *rx_hdr = IL_RX_HDR(pkt);
	struct il3945_rx_frame_end *rx_end = IL_RX_END(pkt);
	u16 len = le16_to_cpu(rx_hdr->len);
	struct sk_buff *skb;
	__le16 fc = hdr->frame_control;

	
	if (unlikely
	    (len + IL39_RX_FRAME_SIZE >
	     PAGE_SIZE << il->hw_params.rx_page_order)) {
		D_DROP("Corruption detected!\n");
		return;
	}

	
	if (unlikely(!il->is_open)) {
		D_DROP("Dropping packet while interface is not open.\n");
		return;
	}

	skb = dev_alloc_skb(128);
	if (!skb) {
		IL_ERR("dev_alloc_skb failed\n");
		return;
	}

	if (!il3945_mod_params.sw_crypto)
		il_set_decrypted_flag(il, (struct ieee80211_hdr *)rxb_addr(rxb),
				      le32_to_cpu(rx_end->status), stats);

	skb_add_rx_frag(skb, 0, rxb->page,
			(void *)rx_hdr->payload - (void *)pkt, len,
			len);

	il_update_stats(il, false, fc, len);
	memcpy(IEEE80211_SKB_RXCB(skb), stats, sizeof(*stats));

	ieee80211_rx(il->hw, skb);
	il->alloc_rxb_page--;
	rxb->page = NULL;
}

#define IL_DELAY_NEXT_SCAN_AFTER_ASSOC (HZ*6)

static void
il3945_hdl_rx(struct il_priv *il, struct il_rx_buf *rxb)
{
	struct ieee80211_hdr *header;
	struct ieee80211_rx_status rx_status;
	struct il_rx_pkt *pkt = rxb_addr(rxb);
	struct il3945_rx_frame_stats *rx_stats = IL_RX_STATS(pkt);
	struct il3945_rx_frame_hdr *rx_hdr = IL_RX_HDR(pkt);
	struct il3945_rx_frame_end *rx_end = IL_RX_END(pkt);
	u16 rx_stats_sig_avg __maybe_unused = le16_to_cpu(rx_stats->sig_avg);
	u16 rx_stats_noise_diff __maybe_unused =
	    le16_to_cpu(rx_stats->noise_diff);
	u8 network_packet;

	rx_status.flag = 0;
	rx_status.mactime = le64_to_cpu(rx_end->timestamp);
	rx_status.band =
	    (rx_hdr->
	     phy_flags & RX_RES_PHY_FLAGS_BAND_24_MSK) ? IEEE80211_BAND_2GHZ :
	    IEEE80211_BAND_5GHZ;
	rx_status.freq =
	    ieee80211_channel_to_frequency(le16_to_cpu(rx_hdr->channel),
					   rx_status.band);

	rx_status.rate_idx = il3945_hwrate_to_plcp_idx(rx_hdr->rate);
	if (rx_status.band == IEEE80211_BAND_5GHZ)
		rx_status.rate_idx -= IL_FIRST_OFDM_RATE;

	rx_status.antenna =
	    (le16_to_cpu(rx_hdr->phy_flags) & RX_RES_PHY_FLAGS_ANTENNA_MSK) >>
	    4;

	
	if (rx_hdr->phy_flags & RX_RES_PHY_FLAGS_SHORT_PREAMBLE_MSK)
		rx_status.flag |= RX_FLAG_SHORTPRE;

	if ((unlikely(rx_stats->phy_count > 20))) {
		D_DROP("dsp size out of range [0,20]: %d/n",
		       rx_stats->phy_count);
		return;
	}

	if (!(rx_end->status & RX_RES_STATUS_NO_CRC32_ERROR) ||
	    !(rx_end->status & RX_RES_STATUS_NO_RXE_OVERFLOW)) {
		D_RX("Bad CRC or FIFO: 0x%08X.\n", rx_end->status);
		return;
	}

	
	rx_status.signal = rx_stats->rssi - IL39_RSSI_OFFSET;

	D_STATS("Rssi %d sig_avg %d noise_diff %d\n", rx_status.signal,
		rx_stats_sig_avg, rx_stats_noise_diff);

	header = (struct ieee80211_hdr *)IL_RX_DATA(pkt);

	network_packet = il3945_is_network_packet(il, header);

	D_STATS("[%c] %d RSSI:%d Signal:%u, Rate:%u\n",
		network_packet ? '*' : ' ', le16_to_cpu(rx_hdr->channel),
		rx_status.signal, rx_status.signal, rx_status.rate_idx);

	if (network_packet) {
		il->_3945.last_beacon_time =
		    le32_to_cpu(rx_end->beacon_timestamp);
		il->_3945.last_tsf = le64_to_cpu(rx_end->timestamp);
		il->_3945.last_rx_rssi = rx_status.signal;
	}

	il3945_pass_packet_to_mac80211(il, rxb, &rx_status);
}

int
il3945_hw_txq_attach_buf_to_tfd(struct il_priv *il, struct il_tx_queue *txq,
				dma_addr_t addr, u16 len, u8 reset, u8 pad)
{
	int count;
	struct il_queue *q;
	struct il3945_tfd *tfd, *tfd_tmp;

	q = &txq->q;
	tfd_tmp = (struct il3945_tfd *)txq->tfds;
	tfd = &tfd_tmp[q->write_ptr];

	if (reset)
		memset(tfd, 0, sizeof(*tfd));

	count = TFD_CTL_COUNT_GET(le32_to_cpu(tfd->control_flags));

	if (count >= NUM_TFD_CHUNKS || count < 0) {
		IL_ERR("Error can not send more than %d chunks\n",
		       NUM_TFD_CHUNKS);
		return -EINVAL;
	}

	tfd->tbs[count].addr = cpu_to_le32(addr);
	tfd->tbs[count].len = cpu_to_le32(len);

	count++;

	tfd->control_flags =
	    cpu_to_le32(TFD_CTL_COUNT_SET(count) | TFD_CTL_PAD_SET(pad));

	return 0;
}

void
il3945_hw_txq_free_tfd(struct il_priv *il, struct il_tx_queue *txq)
{
	struct il3945_tfd *tfd_tmp = (struct il3945_tfd *)txq->tfds;
	int idx = txq->q.read_ptr;
	struct il3945_tfd *tfd = &tfd_tmp[idx];
	struct pci_dev *dev = il->pci_dev;
	int i;
	int counter;

	
	counter = TFD_CTL_COUNT_GET(le32_to_cpu(tfd->control_flags));
	if (counter > NUM_TFD_CHUNKS) {
		IL_ERR("Too many chunks: %i\n", counter);
		
		return;
	}

	
	if (counter)
		pci_unmap_single(dev, dma_unmap_addr(&txq->meta[idx], mapping),
				 dma_unmap_len(&txq->meta[idx], len),
				 PCI_DMA_TODEVICE);

	

	for (i = 1; i < counter; i++)
		pci_unmap_single(dev, le32_to_cpu(tfd->tbs[i].addr),
				 le32_to_cpu(tfd->tbs[i].len),
				 PCI_DMA_TODEVICE);

	
	if (txq->skbs) {
		struct sk_buff *skb = txq->skbs[txq->q.read_ptr];

		
		if (skb) {
			dev_kfree_skb_any(skb);
			txq->skbs[txq->q.read_ptr] = NULL;
		}
	}
}

void
il3945_hw_build_tx_cmd_rate(struct il_priv *il, struct il_device_cmd *cmd,
			    struct ieee80211_tx_info *info,
			    struct ieee80211_hdr *hdr, int sta_id)
{
	u16 hw_value = ieee80211_get_tx_rate(il->hw, info)->hw_value;
	u16 rate_idx = min(hw_value & 0xffff, RATE_COUNT_3945 - 1);
	u16 rate_mask;
	int rate;
	const u8 rts_retry_limit = 7;
	u8 data_retry_limit;
	__le32 tx_flags;
	__le16 fc = hdr->frame_control;
	struct il3945_tx_cmd *tx_cmd = (struct il3945_tx_cmd *)cmd->cmd.payload;

	rate = il3945_rates[rate_idx].plcp;
	tx_flags = tx_cmd->tx_flags;

	rate_mask = RATES_MASK_3945;

	
	if (ieee80211_is_probe_resp(fc))
		data_retry_limit = 3;
	else
		data_retry_limit = IL_DEFAULT_TX_RETRY;
	tx_cmd->data_retry_limit = data_retry_limit;
	
	tx_cmd->rts_retry_limit = min(data_retry_limit, rts_retry_limit);

	tx_cmd->rate = rate;
	tx_cmd->tx_flags = tx_flags;

	
	tx_cmd->supp_rates[0] =
	    ((rate_mask & IL_OFDM_RATES_MASK) >> IL_FIRST_OFDM_RATE) & 0xFF;

	
	tx_cmd->supp_rates[1] = (rate_mask & 0xF);

	D_RATE("Tx sta id: %d, rate: %d (plcp), flags: 0x%4X "
	       "cck/ofdm mask: 0x%x/0x%x\n", sta_id, tx_cmd->rate,
	       le32_to_cpu(tx_cmd->tx_flags), tx_cmd->supp_rates[1],
	       tx_cmd->supp_rates[0]);
}

static u8
il3945_sync_sta(struct il_priv *il, int sta_id, u16 tx_rate)
{
	unsigned long flags_spin;
	struct il_station_entry *station;

	if (sta_id == IL_INVALID_STATION)
		return IL_INVALID_STATION;

	spin_lock_irqsave(&il->sta_lock, flags_spin);
	station = &il->stations[sta_id];

	station->sta.sta.modify_mask = STA_MODIFY_TX_RATE_MSK;
	station->sta.rate_n_flags = cpu_to_le16(tx_rate);
	station->sta.mode = STA_CONTROL_MODIFY_MSK;
	il_send_add_sta(il, &station->sta, CMD_ASYNC);
	spin_unlock_irqrestore(&il->sta_lock, flags_spin);

	D_RATE("SCALE sync station %d to rate %d\n", sta_id, tx_rate);
	return sta_id;
}

static void
il3945_set_pwr_vmain(struct il_priv *il)
{

	il_set_bits_mask_prph(il, APMG_PS_CTRL_REG,
			      APMG_PS_CTRL_VAL_PWR_SRC_VMAIN,
			      ~APMG_PS_CTRL_MSK_PWR_SRC);

	_il_poll_bit(il, CSR_GPIO_IN, CSR_GPIO_IN_VAL_VMAIN_PWR_SRC,
		     CSR_GPIO_IN_BIT_AUX_POWER, 5000);
}

static int
il3945_rx_init(struct il_priv *il, struct il_rx_queue *rxq)
{
	il_wr(il, FH39_RCSR_RBD_BASE(0), rxq->bd_dma);
	il_wr(il, FH39_RCSR_RPTR_ADDR(0), rxq->rb_stts_dma);
	il_wr(il, FH39_RCSR_WPTR(0), 0);
	il_wr(il, FH39_RCSR_CONFIG(0),
	      FH39_RCSR_RX_CONFIG_REG_VAL_DMA_CHNL_EN_ENABLE |
	      FH39_RCSR_RX_CONFIG_REG_VAL_RDRBD_EN_ENABLE |
	      FH39_RCSR_RX_CONFIG_REG_BIT_WR_STTS_EN |
	      FH39_RCSR_RX_CONFIG_REG_VAL_MAX_FRAG_SIZE_128 | (RX_QUEUE_SIZE_LOG
							       <<
							       FH39_RCSR_RX_CONFIG_REG_POS_RBDC_SIZE)
	      | FH39_RCSR_RX_CONFIG_REG_VAL_IRQ_DEST_INT_HOST | (1 <<
								 FH39_RCSR_RX_CONFIG_REG_POS_IRQ_RBTH)
	      | FH39_RCSR_RX_CONFIG_REG_VAL_MSG_MODE_FH);

	
	il_rd(il, FH39_RSSR_CTRL);

	return 0;
}

static int
il3945_tx_reset(struct il_priv *il)
{
	
	il_wr_prph(il, ALM_SCD_MODE_REG, 0x2);

	
	il_wr_prph(il, ALM_SCD_ARASTAT_REG, 0x01);

	
	il_wr_prph(il, ALM_SCD_TXFACT_REG, 0x3f);

	il_wr_prph(il, ALM_SCD_SBYP_MODE_1_REG, 0x010000);
	il_wr_prph(il, ALM_SCD_SBYP_MODE_2_REG, 0x030002);
	il_wr_prph(il, ALM_SCD_TXF4MF_REG, 0x000004);
	il_wr_prph(il, ALM_SCD_TXF5MF_REG, 0x000005);

	il_wr(il, FH39_TSSR_CBB_BASE, il->_3945.shared_phys);

	il_wr(il, FH39_TSSR_MSG_CONFIG,
	      FH39_TSSR_TX_MSG_CONFIG_REG_VAL_SNOOP_RD_TXPD_ON |
	      FH39_TSSR_TX_MSG_CONFIG_REG_VAL_ORDER_RD_TXPD_ON |
	      FH39_TSSR_TX_MSG_CONFIG_REG_VAL_MAX_FRAG_SIZE_128B |
	      FH39_TSSR_TX_MSG_CONFIG_REG_VAL_SNOOP_RD_TFD_ON |
	      FH39_TSSR_TX_MSG_CONFIG_REG_VAL_ORDER_RD_CBB_ON |
	      FH39_TSSR_TX_MSG_CONFIG_REG_VAL_ORDER_RSP_WAIT_TH |
	      FH39_TSSR_TX_MSG_CONFIG_REG_VAL_RSP_WAIT_TH);

	return 0;
}

static int
il3945_txq_ctx_reset(struct il_priv *il)
{
	int rc, txq_id;

	il3945_hw_txq_ctx_free(il);

	
	rc = il_alloc_txq_mem(il);
	if (rc)
		return rc;

	
	rc = il3945_tx_reset(il);
	if (rc)
		goto error;

	
	for (txq_id = 0; txq_id < il->hw_params.max_txq_num; txq_id++) {
		rc = il_tx_queue_init(il, txq_id);
		if (rc) {
			IL_ERR("Tx %d queue init failed\n", txq_id);
			goto error;
		}
	}

	return rc;

error:
	il3945_hw_txq_ctx_free(il);
	return rc;
}

static int
il3945_apm_init(struct il_priv *il)
{
	int ret = il_apm_init(il);

	
	il_wr_prph(il, APMG_RTC_INT_MSK_REG, 0x0);
	il_wr_prph(il, APMG_RTC_INT_STT_REG, 0xFFFFFFFF);

	
	il_set_bits_prph(il, APMG_PS_CTRL_REG, APMG_PS_CTRL_VAL_RESET_REQ);
	udelay(5);
	il_clear_bits_prph(il, APMG_PS_CTRL_REG, APMG_PS_CTRL_VAL_RESET_REQ);

	return ret;
}

static void
il3945_nic_config(struct il_priv *il)
{
	struct il3945_eeprom *eeprom = (struct il3945_eeprom *)il->eeprom;
	unsigned long flags;
	u8 rev_id = il->pci_dev->revision;

	spin_lock_irqsave(&il->lock, flags);

	
	D_INFO("HW Revision ID = 0x%X\n", rev_id);

	if (rev_id & PCI_CFG_REV_ID_BIT_RTP)
		D_INFO("RTP type\n");
	else if (rev_id & PCI_CFG_REV_ID_BIT_BASIC_SKU) {
		D_INFO("3945 RADIO-MB type\n");
		il_set_bit(il, CSR_HW_IF_CONFIG_REG,
			   CSR39_HW_IF_CONFIG_REG_BIT_3945_MB);
	} else {
		D_INFO("3945 RADIO-MM type\n");
		il_set_bit(il, CSR_HW_IF_CONFIG_REG,
			   CSR39_HW_IF_CONFIG_REG_BIT_3945_MM);
	}

	if (EEPROM_SKU_CAP_OP_MODE_MRC == eeprom->sku_cap) {
		D_INFO("SKU OP mode is mrc\n");
		il_set_bit(il, CSR_HW_IF_CONFIG_REG,
			   CSR39_HW_IF_CONFIG_REG_BIT_SKU_MRC);
	} else
		D_INFO("SKU OP mode is basic\n");

	if ((eeprom->board_revision & 0xF0) == 0xD0) {
		D_INFO("3945ABG revision is 0x%X\n", eeprom->board_revision);
		il_set_bit(il, CSR_HW_IF_CONFIG_REG,
			   CSR39_HW_IF_CONFIG_REG_BIT_BOARD_TYPE);
	} else {
		D_INFO("3945ABG revision is 0x%X\n", eeprom->board_revision);
		il_clear_bit(il, CSR_HW_IF_CONFIG_REG,
			     CSR39_HW_IF_CONFIG_REG_BIT_BOARD_TYPE);
	}

	if (eeprom->almgor_m_version <= 1) {
		il_set_bit(il, CSR_HW_IF_CONFIG_REG,
			   CSR39_HW_IF_CONFIG_REG_BITS_SILICON_TYPE_A);
		D_INFO("Card M type A version is 0x%X\n",
		       eeprom->almgor_m_version);
	} else {
		D_INFO("Card M type B version is 0x%X\n",
		       eeprom->almgor_m_version);
		il_set_bit(il, CSR_HW_IF_CONFIG_REG,
			   CSR39_HW_IF_CONFIG_REG_BITS_SILICON_TYPE_B);
	}
	spin_unlock_irqrestore(&il->lock, flags);

	if (eeprom->sku_cap & EEPROM_SKU_CAP_SW_RF_KILL_ENABLE)
		D_RF_KILL("SW RF KILL supported in EEPROM.\n");

	if (eeprom->sku_cap & EEPROM_SKU_CAP_HW_RF_KILL_ENABLE)
		D_RF_KILL("HW RF KILL supported in EEPROM.\n");
}

int
il3945_hw_nic_init(struct il_priv *il)
{
	int rc;
	unsigned long flags;
	struct il_rx_queue *rxq = &il->rxq;

	spin_lock_irqsave(&il->lock, flags);
	il3945_apm_init(il);
	spin_unlock_irqrestore(&il->lock, flags);

	il3945_set_pwr_vmain(il);
	il3945_nic_config(il);

	
	if (!rxq->bd) {
		rc = il_rx_queue_alloc(il);
		if (rc) {
			IL_ERR("Unable to initialize Rx queue\n");
			return -ENOMEM;
		}
	} else
		il3945_rx_queue_reset(il, rxq);

	il3945_rx_replenish(il);

	il3945_rx_init(il, rxq);


	il_wr(il, FH39_RCSR_WPTR(0), rxq->write & ~7);

	rc = il3945_txq_ctx_reset(il);
	if (rc)
		return rc;

	set_bit(S_INIT, &il->status);

	return 0;
}

void
il3945_hw_txq_ctx_free(struct il_priv *il)
{
	int txq_id;

	
	if (il->txq)
		for (txq_id = 0; txq_id < il->hw_params.max_txq_num; txq_id++)
			if (txq_id == IL39_CMD_QUEUE_NUM)
				il_cmd_queue_free(il);
			else
				il_tx_queue_free(il, txq_id);

	
	il_free_txq_mem(il);
}

void
il3945_hw_txq_ctx_stop(struct il_priv *il)
{
	int txq_id;

	
	_il_wr_prph(il, ALM_SCD_MODE_REG, 0);
	_il_wr_prph(il, ALM_SCD_TXFACT_REG, 0);

	
	for (txq_id = 0; txq_id < il->hw_params.max_txq_num; txq_id++) {
		_il_wr(il, FH39_TCSR_CONFIG(txq_id), 0x0);
		_il_poll_bit(il, FH39_TSSR_TX_STATUS,
			     FH39_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(txq_id),
			     FH39_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(txq_id),
			     1000);
	}
}

static int
il3945_hw_reg_adjust_power_by_temp(int new_reading, int old_reading)
{
	return (new_reading - old_reading) * (-11) / 100;
}

static inline int
il3945_hw_reg_temp_out_of_range(int temperature)
{
	return (temperature < -260 || temperature > 25) ? 1 : 0;
}

int
il3945_hw_get_temperature(struct il_priv *il)
{
	return _il_rd(il, CSR_UCODE_DRV_GP2);
}

static int
il3945_hw_reg_txpower_get_temperature(struct il_priv *il)
{
	struct il3945_eeprom *eeprom = (struct il3945_eeprom *)il->eeprom;
	int temperature;

	temperature = il3945_hw_get_temperature(il);

	D_INFO("Temperature: %d\n", temperature + IL_TEMP_CONVERT);

	
	if (il3945_hw_reg_temp_out_of_range(temperature)) {
		IL_ERR("Error bad temperature value  %d\n", temperature);

		if (il->last_temperature > 100)
			temperature = eeprom->groups[2].temperature;
		else		
			temperature = il->last_temperature;
	}

	return temperature;	
}

#define IL_TEMPERATURE_LIMIT_TIMER   6

static int
il3945_is_temp_calib_needed(struct il_priv *il)
{
	int temp_diff;

	il->temperature = il3945_hw_reg_txpower_get_temperature(il);
	temp_diff = il->temperature - il->last_temperature;

	
	if (temp_diff < 0) {
		D_POWER("Getting cooler, delta %d,\n", temp_diff);
		temp_diff = -temp_diff;
	} else if (temp_diff == 0)
		D_POWER("Same temp,\n");
	else
		D_POWER("Getting warmer, delta %d,\n", temp_diff);

	
	if (temp_diff < IL_TEMPERATURE_LIMIT_TIMER) {
		D_POWER("Timed thermal calib not needed\n");
		return 0;
	}

	D_POWER("Timed thermal calib needed\n");

	il->last_temperature = il->temperature;
	return 1;
}

#define IL_MAX_GAIN_ENTRIES 78
#define IL_CCK_FROM_OFDM_POWER_DIFF  -5
#define IL_CCK_FROM_OFDM_IDX_DIFF (10)

static struct il3945_tx_power power_gain_table[2][IL_MAX_GAIN_ENTRIES] = {
	{
	 {251, 127},		
	 {251, 127},
	 {251, 127},
	 {251, 127},
	 {251, 125},
	 {251, 110},
	 {251, 105},
	 {251, 98},
	 {187, 125},
	 {187, 115},
	 {187, 108},
	 {187, 99},
	 {243, 119},
	 {243, 111},
	 {243, 105},
	 {243, 97},
	 {243, 92},
	 {211, 106},
	 {211, 100},
	 {179, 120},
	 {179, 113},
	 {179, 107},
	 {147, 125},
	 {147, 119},
	 {147, 112},
	 {147, 106},
	 {147, 101},
	 {147, 97},
	 {147, 91},
	 {115, 107},
	 {235, 121},
	 {235, 115},
	 {235, 109},
	 {203, 127},
	 {203, 121},
	 {203, 115},
	 {203, 108},
	 {203, 102},
	 {203, 96},
	 {203, 92},
	 {171, 110},
	 {171, 104},
	 {171, 98},
	 {139, 116},
	 {227, 125},
	 {227, 119},
	 {227, 113},
	 {227, 107},
	 {227, 101},
	 {227, 96},
	 {195, 113},
	 {195, 106},
	 {195, 102},
	 {195, 95},
	 {163, 113},
	 {163, 106},
	 {163, 102},
	 {163, 95},
	 {131, 113},
	 {131, 106},
	 {131, 102},
	 {131, 95},
	 {99, 113},
	 {99, 106},
	 {99, 102},
	 {99, 95},
	 {67, 113},
	 {67, 106},
	 {67, 102},
	 {67, 95},
	 {35, 113},
	 {35, 106},
	 {35, 102},
	 {35, 95},
	 {3, 113},
	 {3, 106},
	 {3, 102},
	 {3, 95}		
	},
	{
	 {251, 127},		
	 {251, 120},
	 {251, 114},
	 {219, 119},
	 {219, 101},
	 {187, 113},
	 {187, 102},
	 {155, 114},
	 {155, 103},
	 {123, 117},
	 {123, 107},
	 {123, 99},
	 {123, 92},
	 {91, 108},
	 {59, 125},
	 {59, 118},
	 {59, 109},
	 {59, 102},
	 {59, 96},
	 {59, 90},
	 {27, 104},
	 {27, 98},
	 {27, 92},
	 {115, 118},
	 {115, 111},
	 {115, 104},
	 {83, 126},
	 {83, 121},
	 {83, 113},
	 {83, 105},
	 {83, 99},
	 {51, 118},
	 {51, 111},
	 {51, 104},
	 {51, 98},
	 {19, 116},
	 {19, 109},
	 {19, 102},
	 {19, 98},
	 {19, 93},
	 {171, 113},
	 {171, 107},
	 {171, 99},
	 {139, 120},
	 {139, 113},
	 {139, 107},
	 {139, 99},
	 {107, 120},
	 {107, 113},
	 {107, 107},
	 {107, 99},
	 {75, 120},
	 {75, 113},
	 {75, 107},
	 {75, 99},
	 {43, 120},
	 {43, 113},
	 {43, 107},
	 {43, 99},
	 {11, 120},
	 {11, 113},
	 {11, 107},
	 {11, 99},
	 {131, 107},
	 {131, 99},
	 {99, 120},
	 {99, 113},
	 {99, 107},
	 {99, 99},
	 {67, 120},
	 {67, 113},
	 {67, 107},
	 {67, 99},
	 {35, 120},
	 {35, 113},
	 {35, 107},
	 {35, 99},
	 {3, 120}		
	}
};

static inline u8
il3945_hw_reg_fix_power_idx(int idx)
{
	if (idx < 0)
		return 0;
	if (idx >= IL_MAX_GAIN_ENTRIES)
		return IL_MAX_GAIN_ENTRIES - 1;
	return (u8) idx;
}

#define REG_RECALIB_PERIOD (60)

static void
il3945_hw_reg_set_scan_power(struct il_priv *il, u32 scan_tbl_idx, s32 rate_idx,
			     const s8 *clip_pwrs,
			     struct il_channel_info *ch_info, int band_idx)
{
	struct il3945_scan_power_info *scan_power_info;
	s8 power;
	u8 power_idx;

	scan_power_info = &ch_info->scan_pwr_info[scan_tbl_idx];

	power = min(ch_info->scan_power, clip_pwrs[RATE_6M_IDX_TBL]);

	power = min(power, il->tx_power_user_lmt);
	scan_power_info->requested_power = power;

	power_idx =
	    ch_info->power_info[rate_idx].power_table_idx - (power -
							     ch_info->
							     power_info
							     [RATE_6M_IDX_TBL].
							     requested_power) *
	    2;


	
	power_idx = il3945_hw_reg_fix_power_idx(power_idx);

	scan_power_info->power_table_idx = power_idx;
	scan_power_info->tpc.tx_gain =
	    power_gain_table[band_idx][power_idx].tx_gain;
	scan_power_info->tpc.dsp_atten =
	    power_gain_table[band_idx][power_idx].dsp_atten;
}

static int
il3945_send_tx_power(struct il_priv *il)
{
	int rate_idx, i;
	const struct il_channel_info *ch_info = NULL;
	struct il3945_txpowertable_cmd txpower = {
		.channel = il->active.channel,
	};
	u16 chan;

	if (WARN_ONCE
	    (test_bit(S_SCAN_HW, &il->status),
	     "TX Power requested while scanning!\n"))
		return -EAGAIN;

	chan = le16_to_cpu(il->active.channel);

	txpower.band = (il->band == IEEE80211_BAND_5GHZ) ? 0 : 1;
	ch_info = il_get_channel_info(il, il->band, chan);
	if (!ch_info) {
		IL_ERR("Failed to get channel info for channel %d [%d]\n", chan,
		       il->band);
		return -EINVAL;
	}

	if (!il_is_channel_valid(ch_info)) {
		D_POWER("Not calling TX_PWR_TBL_CMD on " "non-Tx channel.\n");
		return 0;
	}

	
	
	for (rate_idx = IL_FIRST_OFDM_RATE, i = 0;
	     rate_idx <= IL39_LAST_OFDM_RATE; rate_idx++, i++) {

		txpower.power[i].tpc = ch_info->power_info[i].tpc;
		txpower.power[i].rate = il3945_rates[rate_idx].plcp;

		D_POWER("ch %d:%d rf %d dsp %3d rate code 0x%02x\n",
			le16_to_cpu(txpower.channel), txpower.band,
			txpower.power[i].tpc.tx_gain,
			txpower.power[i].tpc.dsp_atten, txpower.power[i].rate);
	}
	
	for (rate_idx = IL_FIRST_CCK_RATE; rate_idx <= IL_LAST_CCK_RATE;
	     rate_idx++, i++) {
		txpower.power[i].tpc = ch_info->power_info[i].tpc;
		txpower.power[i].rate = il3945_rates[rate_idx].plcp;

		D_POWER("ch %d:%d rf %d dsp %3d rate code 0x%02x\n",
			le16_to_cpu(txpower.channel), txpower.band,
			txpower.power[i].tpc.tx_gain,
			txpower.power[i].tpc.dsp_atten, txpower.power[i].rate);
	}

	return il_send_cmd_pdu(il, C_TX_PWR_TBL,
			       sizeof(struct il3945_txpowertable_cmd),
			       &txpower);

}

static int
il3945_hw_reg_set_new_power(struct il_priv *il, struct il_channel_info *ch_info)
{
	struct il3945_channel_power_info *power_info;
	int power_changed = 0;
	int i;
	const s8 *clip_pwrs;
	int power;

	
	clip_pwrs = il->_3945.clip_groups[ch_info->group_idx].clip_powers;

	
	power_info = ch_info->power_info;

	
	for (i = RATE_6M_IDX_TBL; i <= RATE_54M_IDX_TBL; i++, ++power_info) {
		int delta_idx;

		
		power = min(ch_info->curr_txpow, clip_pwrs[i]);
		if (power == power_info->requested_power)
			continue;

		delta_idx = (power - power_info->requested_power) * 2;
		power_info->base_power_idx -= delta_idx;

		
		power_info->requested_power = power;

		power_changed = 1;
	}

	if (power_changed) {
		power =
		    ch_info->power_info[RATE_12M_IDX_TBL].requested_power +
		    IL_CCK_FROM_OFDM_POWER_DIFF;

		
		for (i = RATE_1M_IDX_TBL; i <= RATE_11M_IDX_TBL; i++) {
			power_info->requested_power = power;
			power_info->base_power_idx =
			    ch_info->power_info[RATE_12M_IDX_TBL].
			    base_power_idx + IL_CCK_FROM_OFDM_IDX_DIFF;
			++power_info;
		}
	}

	return 0;
}

static int
il3945_hw_reg_get_ch_txpower_limit(struct il_channel_info *ch_info)
{
	s8 max_power;

#if 0
	
	if (ch_info->tgd_data.max_power != 0)
		max_power =
		    min(ch_info->tgd_data.max_power,
			ch_info->eeprom.max_power_avg);

	
	else
#endif
		max_power = ch_info->eeprom.max_power_avg;

	return min(max_power, ch_info->max_power_avg);
}

static int
il3945_hw_reg_comp_txpower_temp(struct il_priv *il)
{
	struct il_channel_info *ch_info = NULL;
	struct il3945_eeprom *eeprom = (struct il3945_eeprom *)il->eeprom;
	int delta_idx;
	const s8 *clip_pwrs;	
	u8 a_band;
	u8 rate_idx;
	u8 scan_tbl_idx;
	u8 i;
	int ref_temp;
	int temperature = il->temperature;

	if (il->disable_tx_power_cal || test_bit(S_SCANNING, &il->status)) {
		
		return 0;
	}
	
	for (i = 0; i < il->channel_count; i++) {
		ch_info = &il->channel_info[i];
		a_band = il_is_channel_a_band(ch_info);

		
		ref_temp = (s16) eeprom->groups[ch_info->group_idx].temperature;

		delta_idx =
		    il3945_hw_reg_adjust_power_by_temp(temperature, ref_temp);

		
		for (rate_idx = 0; rate_idx < RATE_COUNT_3945; rate_idx++) {
			int power_idx =
			    ch_info->power_info[rate_idx].base_power_idx;

			
			power_idx += delta_idx;

			
			power_idx = il3945_hw_reg_fix_power_idx(power_idx);
			ch_info->power_info[rate_idx].power_table_idx =
			    (u8) power_idx;
			ch_info->power_info[rate_idx].tpc =
			    power_gain_table[a_band][power_idx];
		}

		
		clip_pwrs =
		    il->_3945.clip_groups[ch_info->group_idx].clip_powers;

		
		for (scan_tbl_idx = 0; scan_tbl_idx < IL_NUM_SCAN_RATES;
		     scan_tbl_idx++) {
			s32 actual_idx =
			    (scan_tbl_idx ==
			     0) ? RATE_1M_IDX_TBL : RATE_6M_IDX_TBL;
			il3945_hw_reg_set_scan_power(il, scan_tbl_idx,
						     actual_idx, clip_pwrs,
						     ch_info, a_band);
		}
	}

	
	return il->ops->send_tx_power(il);
}

int
il3945_hw_reg_set_txpower(struct il_priv *il, s8 power)
{
	struct il_channel_info *ch_info;
	s8 max_power;
	u8 a_band;
	u8 i;

	if (il->tx_power_user_lmt == power) {
		D_POWER("Requested Tx power same as current " "limit: %ddBm.\n",
			power);
		return 0;
	}

	D_POWER("Setting upper limit clamp to %ddBm.\n", power);
	il->tx_power_user_lmt = power;

	

	for (i = 0; i < il->channel_count; i++) {
		ch_info = &il->channel_info[i];
		a_band = il_is_channel_a_band(ch_info);

		max_power = il3945_hw_reg_get_ch_txpower_limit(ch_info);
		max_power = min(power, max_power);
		if (max_power != ch_info->curr_txpow) {
			ch_info->curr_txpow = max_power;

			
			il3945_hw_reg_set_new_power(il, ch_info);
		}
	}

	il3945_is_temp_calib_needed(il);
	il3945_hw_reg_comp_txpower_temp(il);

	return 0;
}

static int
il3945_send_rxon_assoc(struct il_priv *il)
{
	int rc = 0;
	struct il_rx_pkt *pkt;
	struct il3945_rxon_assoc_cmd rxon_assoc;
	struct il_host_cmd cmd = {
		.id = C_RXON_ASSOC,
		.len = sizeof(rxon_assoc),
		.flags = CMD_WANT_SKB,
		.data = &rxon_assoc,
	};
	const struct il_rxon_cmd *rxon1 = &il->staging;
	const struct il_rxon_cmd *rxon2 = &il->active;

	if (rxon1->flags == rxon2->flags &&
	    rxon1->filter_flags == rxon2->filter_flags &&
	    rxon1->cck_basic_rates == rxon2->cck_basic_rates &&
	    rxon1->ofdm_basic_rates == rxon2->ofdm_basic_rates) {
		D_INFO("Using current RXON_ASSOC.  Not resending.\n");
		return 0;
	}

	rxon_assoc.flags = il->staging.flags;
	rxon_assoc.filter_flags = il->staging.filter_flags;
	rxon_assoc.ofdm_basic_rates = il->staging.ofdm_basic_rates;
	rxon_assoc.cck_basic_rates = il->staging.cck_basic_rates;
	rxon_assoc.reserved = 0;

	rc = il_send_cmd_sync(il, &cmd);
	if (rc)
		return rc;

	pkt = (struct il_rx_pkt *)cmd.reply_page;
	if (pkt->hdr.flags & IL_CMD_FAILED_MSK) {
		IL_ERR("Bad return from C_RXON_ASSOC command\n");
		rc = -EIO;
	}

	il_free_pages(il, cmd.reply_page);

	return rc;
}

int
il3945_commit_rxon(struct il_priv *il)
{
	
	struct il3945_rxon_cmd *active_rxon = (void *)&il->active;
	struct il3945_rxon_cmd *staging_rxon = (void *)&il->staging;
	int rc = 0;
	bool new_assoc = !!(staging_rxon->filter_flags & RXON_FILTER_ASSOC_MSK);

	if (test_bit(S_EXIT_PENDING, &il->status))
		return -EINVAL;

	if (!il_is_alive(il))
		return -1;

	
	staging_rxon->flags |= RXON_FLG_TSF2HOST_MSK;

	
	staging_rxon->flags &= ~(RXON_FLG_DIS_DIV_MSK | RXON_FLG_ANT_SEL_MSK);
	staging_rxon->flags |= il3945_get_antenna_flags(il);

	rc = il_check_rxon_cmd(il);
	if (rc) {
		IL_ERR("Invalid RXON configuration.  Not committing.\n");
		return -EINVAL;
	}

	if (!il_full_rxon_required(il)) {
		rc = il_send_rxon_assoc(il);
		if (rc) {
			IL_ERR("Error setting RXON_ASSOC "
			       "configuration (%d).\n", rc);
			return rc;
		}

		memcpy(active_rxon, staging_rxon, sizeof(*active_rxon));
		il_set_tx_power(il, il->tx_power_next, false);
		return 0;
	}

	if (il_is_associated(il) && new_assoc) {
		D_INFO("Toggling associated bit on current RXON\n");
		active_rxon->filter_flags &= ~RXON_FILTER_ASSOC_MSK;

		active_rxon->reserved4 = 0;
		active_rxon->reserved5 = 0;
		rc = il_send_cmd_pdu(il, C_RXON, sizeof(struct il3945_rxon_cmd),
				     &il->active);

		if (rc) {
			active_rxon->filter_flags |= RXON_FILTER_ASSOC_MSK;
			IL_ERR("Error clearing ASSOC_MSK on current "
			       "configuration (%d).\n", rc);
			return rc;
		}
		il_clear_ucode_stations(il);
		il_restore_stations(il);
	}

	D_INFO("Sending RXON\n" "* with%s RXON_FILTER_ASSOC_MSK\n"
	       "* channel = %d\n" "* bssid = %pM\n", (new_assoc ? "" : "out"),
	       le16_to_cpu(staging_rxon->channel), staging_rxon->bssid_addr);

	staging_rxon->reserved4 = 0;
	staging_rxon->reserved5 = 0;

	il_set_rxon_hwcrypto(il, !il3945_mod_params.sw_crypto);

	
	rc = il_send_cmd_pdu(il, C_RXON, sizeof(struct il3945_rxon_cmd),
			     staging_rxon);
	if (rc) {
		IL_ERR("Error setting new configuration (%d).\n", rc);
		return rc;
	}

	memcpy(active_rxon, staging_rxon, sizeof(*active_rxon));

	if (!new_assoc) {
		il_clear_ucode_stations(il);
		il_restore_stations(il);
	}

	rc = il_set_tx_power(il, il->tx_power_next, true);
	if (rc) {
		IL_ERR("Error setting Tx power (%d).\n", rc);
		return rc;
	}

	
	rc = il3945_init_hw_rate_table(il);
	if (rc) {
		IL_ERR("Error setting HW rate table: %02X\n", rc);
		return -EIO;
	}

	return 0;
}

void
il3945_reg_txpower_periodic(struct il_priv *il)
{
	if (!il3945_is_temp_calib_needed(il))
		goto reschedule;

	il3945_hw_reg_comp_txpower_temp(il);

reschedule:
	queue_delayed_work(il->workqueue, &il->_3945.thermal_periodic,
			   REG_RECALIB_PERIOD * HZ);
}

static void
il3945_bg_reg_txpower_periodic(struct work_struct *work)
{
	struct il_priv *il = container_of(work, struct il_priv,
					  _3945.thermal_periodic.work);

	mutex_lock(&il->mutex);
	if (test_bit(S_EXIT_PENDING, &il->status) || il->txq == NULL)
		goto out;

	il3945_reg_txpower_periodic(il);
out:
	mutex_unlock(&il->mutex);
}

static u16
il3945_hw_reg_get_ch_grp_idx(struct il_priv *il,
			     const struct il_channel_info *ch_info)
{
	struct il3945_eeprom *eeprom = (struct il3945_eeprom *)il->eeprom;
	struct il3945_eeprom_txpower_group *ch_grp = &eeprom->groups[0];
	u8 group;
	u16 group_idx = 0;	
	u8 grp_channel;

	
	if (il_is_channel_a_band(ch_info)) {
		for (group = 1; group < 5; group++) {
			grp_channel = ch_grp[group].group_channel;
			if (ch_info->channel <= grp_channel) {
				group_idx = group;
				break;
			}
		}
		
		if (group == 5)
			group_idx = 4;
	} else
		group_idx = 0;	

	D_POWER("Chnl %d mapped to grp %d\n", ch_info->channel, group_idx);
	return group_idx;
}

static int
il3945_hw_reg_get_matched_power_idx(struct il_priv *il, s8 requested_power,
				    s32 setting_idx, s32 *new_idx)
{
	const struct il3945_eeprom_txpower_group *chnl_grp = NULL;
	struct il3945_eeprom *eeprom = (struct il3945_eeprom *)il->eeprom;
	s32 idx0, idx1;
	s32 power = 2 * requested_power;
	s32 i;
	const struct il3945_eeprom_txpower_sample *samples;
	s32 gains0, gains1;
	s32 res;
	s32 denominator;

	chnl_grp = &eeprom->groups[setting_idx];
	samples = chnl_grp->samples;
	for (i = 0; i < 5; i++) {
		if (power == samples[i].power) {
			*new_idx = samples[i].gain_idx;
			return 0;
		}
	}

	if (power > samples[1].power) {
		idx0 = 0;
		idx1 = 1;
	} else if (power > samples[2].power) {
		idx0 = 1;
		idx1 = 2;
	} else if (power > samples[3].power) {
		idx0 = 2;
		idx1 = 3;
	} else {
		idx0 = 3;
		idx1 = 4;
	}

	denominator = (s32) samples[idx1].power - (s32) samples[idx0].power;
	if (denominator == 0)
		return -EINVAL;
	gains0 = (s32) samples[idx0].gain_idx * (1 << 19);
	gains1 = (s32) samples[idx1].gain_idx * (1 << 19);
	res =
	    gains0 + (gains1 - gains0) * ((s32) power -
					  (s32) samples[idx0].power) /
	    denominator + (1 << 18);
	*new_idx = res >> 19;
	return 0;
}

static void
il3945_hw_reg_init_channel_groups(struct il_priv *il)
{
	u32 i;
	s32 rate_idx;
	struct il3945_eeprom *eeprom = (struct il3945_eeprom *)il->eeprom;
	const struct il3945_eeprom_txpower_group *group;

	D_POWER("Initializing factory calib info from EEPROM\n");

	for (i = 0; i < IL_NUM_TX_CALIB_GROUPS; i++) {
		s8 *clip_pwrs;	
		s8 satur_pwr;	
		group = &eeprom->groups[i];

		
		if (group->saturation_power < 40) {
			IL_WARN("Error: saturation power is %d, "
				"less than minimum expected 40\n",
				group->saturation_power);
			return;
		}

		
		clip_pwrs = (s8 *) il->_3945.clip_groups[i].clip_powers;

		
		satur_pwr = (s8) (group->saturation_power >> 1);

		
		for (rate_idx = 0; rate_idx < RATE_COUNT_3945;
		     rate_idx++, clip_pwrs++) {
			switch (rate_idx) {
			case RATE_36M_IDX_TBL:
				if (i == 0)	
					*clip_pwrs = satur_pwr;
				else	
					*clip_pwrs = satur_pwr - 5;
				break;
			case RATE_48M_IDX_TBL:
				if (i == 0)
					*clip_pwrs = satur_pwr - 7;
				else
					*clip_pwrs = satur_pwr - 10;
				break;
			case RATE_54M_IDX_TBL:
				if (i == 0)
					*clip_pwrs = satur_pwr - 9;
				else
					*clip_pwrs = satur_pwr - 12;
				break;
			default:
				*clip_pwrs = satur_pwr;
				break;
			}
		}
	}
}

int
il3945_txpower_set_from_eeprom(struct il_priv *il)
{
	struct il_channel_info *ch_info = NULL;
	struct il3945_channel_power_info *pwr_info;
	struct il3945_eeprom *eeprom = (struct il3945_eeprom *)il->eeprom;
	int delta_idx;
	u8 rate_idx;
	u8 scan_tbl_idx;
	const s8 *clip_pwrs;	
	u8 gain, dsp_atten;
	s8 power;
	u8 pwr_idx, base_pwr_idx, a_band;
	u8 i;
	int temperature;

	temperature = il3945_hw_reg_txpower_get_temperature(il);
	il->last_temperature = temperature;

	il3945_hw_reg_init_channel_groups(il);

	
	for (i = 0, ch_info = il->channel_info; i < il->channel_count;
	     i++, ch_info++) {
		a_band = il_is_channel_a_band(ch_info);
		if (!il_is_channel_valid(ch_info))
			continue;

		
		ch_info->group_idx = il3945_hw_reg_get_ch_grp_idx(il, ch_info);

		
		clip_pwrs =
		    il->_3945.clip_groups[ch_info->group_idx].clip_powers;

		delta_idx =
		    il3945_hw_reg_adjust_power_by_temp(temperature,
						       eeprom->groups[ch_info->
								      group_idx].
						       temperature);

		D_POWER("Delta idx for channel %d: %d [%d]\n", ch_info->channel,
			delta_idx, temperature + IL_TEMP_CONVERT);

		
		for (rate_idx = 0; rate_idx < IL_OFDM_RATES; rate_idx++) {
			s32 uninitialized_var(power_idx);
			int rc;

			s8 pwr = min(ch_info->max_power_avg,
				     clip_pwrs[rate_idx]);

			pwr_info = &ch_info->power_info[rate_idx];

			rc = il3945_hw_reg_get_matched_power_idx(il, pwr,
								 ch_info->
								 group_idx,
								 &power_idx);
			if (rc) {
				IL_ERR("Invalid power idx\n");
				return rc;
			}
			pwr_info->base_power_idx = (u8) power_idx;

			
			power_idx += delta_idx;

			
			power_idx = il3945_hw_reg_fix_power_idx(power_idx);

			
			pwr_info->requested_power = pwr;
			pwr_info->power_table_idx = (u8) power_idx;
			pwr_info->tpc.tx_gain =
			    power_gain_table[a_band][power_idx].tx_gain;
			pwr_info->tpc.dsp_atten =
			    power_gain_table[a_band][power_idx].dsp_atten;
		}

		
		pwr_info = &ch_info->power_info[RATE_12M_IDX_TBL];
		power = pwr_info->requested_power + IL_CCK_FROM_OFDM_POWER_DIFF;
		pwr_idx = pwr_info->power_table_idx + IL_CCK_FROM_OFDM_IDX_DIFF;
		base_pwr_idx =
		    pwr_info->base_power_idx + IL_CCK_FROM_OFDM_IDX_DIFF;

		
		pwr_idx = il3945_hw_reg_fix_power_idx(pwr_idx);
		gain = power_gain_table[a_band][pwr_idx].tx_gain;
		dsp_atten = power_gain_table[a_band][pwr_idx].dsp_atten;

		for (rate_idx = 0; rate_idx < IL_CCK_RATES; rate_idx++) {
			pwr_info =
			    &ch_info->power_info[rate_idx + IL_OFDM_RATES];
			pwr_info->requested_power = power;
			pwr_info->power_table_idx = pwr_idx;
			pwr_info->base_power_idx = base_pwr_idx;
			pwr_info->tpc.tx_gain = gain;
			pwr_info->tpc.dsp_atten = dsp_atten;
		}

		
		for (scan_tbl_idx = 0; scan_tbl_idx < IL_NUM_SCAN_RATES;
		     scan_tbl_idx++) {
			s32 actual_idx =
			    (scan_tbl_idx ==
			     0) ? RATE_1M_IDX_TBL : RATE_6M_IDX_TBL;
			il3945_hw_reg_set_scan_power(il, scan_tbl_idx,
						     actual_idx, clip_pwrs,
						     ch_info, a_band);
		}
	}

	return 0;
}

int
il3945_hw_rxq_stop(struct il_priv *il)
{
	int ret;

	_il_wr(il, FH39_RCSR_CONFIG(0), 0);
	ret = _il_poll_bit(il, FH39_RSSR_STATUS,
			   FH39_RSSR_CHNL0_RX_STATUS_CHNL_IDLE,
			   FH39_RSSR_CHNL0_RX_STATUS_CHNL_IDLE,
			   1000);
	if (ret < 0)
		IL_ERR("Can't stop Rx DMA.\n");

	return 0;
}

int
il3945_hw_tx_queue_init(struct il_priv *il, struct il_tx_queue *txq)
{
	int txq_id = txq->q.id;

	struct il3945_shared *shared_data = il->_3945.shared_virt;

	shared_data->tx_base_ptr[txq_id] = cpu_to_le32((u32) txq->q.dma_addr);

	il_wr(il, FH39_CBCC_CTRL(txq_id), 0);
	il_wr(il, FH39_CBCC_BASE(txq_id), 0);

	il_wr(il, FH39_TCSR_CONFIG(txq_id),
	      FH39_TCSR_TX_CONFIG_REG_VAL_CIRQ_RTC_NOINT |
	      FH39_TCSR_TX_CONFIG_REG_VAL_MSG_MODE_TXF |
	      FH39_TCSR_TX_CONFIG_REG_VAL_CIRQ_HOST_IFTFD |
	      FH39_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_ENABLE_VAL |
	      FH39_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE);

	
	_il_rd(il, FH39_TSSR_CBB_BASE);

	return 0;
}

static u16
il3945_get_hcmd_size(u8 cmd_id, u16 len)
{
	switch (cmd_id) {
	case C_RXON:
		return sizeof(struct il3945_rxon_cmd);
	case C_POWER_TBL:
		return sizeof(struct il3945_powertable_cmd);
	default:
		return len;
	}
}

static u16
il3945_build_addsta_hcmd(const struct il_addsta_cmd *cmd, u8 * data)
{
	struct il3945_addsta_cmd *addsta = (struct il3945_addsta_cmd *)data;
	addsta->mode = cmd->mode;
	memcpy(&addsta->sta, &cmd->sta, sizeof(struct sta_id_modify));
	memcpy(&addsta->key, &cmd->key, sizeof(struct il4965_keyinfo));
	addsta->station_flags = cmd->station_flags;
	addsta->station_flags_msk = cmd->station_flags_msk;
	addsta->tid_disable_tx = cpu_to_le16(0);
	addsta->rate_n_flags = cmd->rate_n_flags;
	addsta->add_immediate_ba_tid = cmd->add_immediate_ba_tid;
	addsta->remove_immediate_ba_tid = cmd->remove_immediate_ba_tid;
	addsta->add_immediate_ba_ssn = cmd->add_immediate_ba_ssn;

	return (u16) sizeof(struct il3945_addsta_cmd);
}

static int
il3945_add_bssid_station(struct il_priv *il, const u8 * addr, u8 * sta_id_r)
{
	int ret;
	u8 sta_id;
	unsigned long flags;

	if (sta_id_r)
		*sta_id_r = IL_INVALID_STATION;

	ret = il_add_station_common(il, addr, 0, NULL, &sta_id);
	if (ret) {
		IL_ERR("Unable to add station %pM\n", addr);
		return ret;
	}

	if (sta_id_r)
		*sta_id_r = sta_id;

	spin_lock_irqsave(&il->sta_lock, flags);
	il->stations[sta_id].used |= IL_STA_LOCAL;
	spin_unlock_irqrestore(&il->sta_lock, flags);

	return 0;
}

static int
il3945_manage_ibss_station(struct il_priv *il, struct ieee80211_vif *vif,
			   bool add)
{
	struct il_vif_priv *vif_priv = (void *)vif->drv_priv;
	int ret;

	if (add) {
		ret =
		    il3945_add_bssid_station(il, vif->bss_conf.bssid,
					     &vif_priv->ibss_bssid_sta_id);
		if (ret)
			return ret;

		il3945_sync_sta(il, vif_priv->ibss_bssid_sta_id,
				(il->band ==
				 IEEE80211_BAND_5GHZ) ? RATE_6M_PLCP :
				RATE_1M_PLCP);
		il3945_rate_scale_init(il->hw, vif_priv->ibss_bssid_sta_id);

		return 0;
	}

	return il_remove_station(il, vif_priv->ibss_bssid_sta_id,
				 vif->bss_conf.bssid);
}

int
il3945_init_hw_rate_table(struct il_priv *il)
{
	int rc, i, idx, prev_idx;
	struct il3945_rate_scaling_cmd rate_cmd = {
		.reserved = {0, 0, 0},
	};
	struct il3945_rate_scaling_info *table = rate_cmd.table;

	for (i = 0; i < ARRAY_SIZE(il3945_rates); i++) {
		idx = il3945_rates[i].table_rs_idx;

		table[idx].rate_n_flags = cpu_to_le16(il3945_rates[i].plcp);
		table[idx].try_cnt = il->retry_rate;
		prev_idx = il3945_get_prev_ieee_rate(i);
		table[idx].next_rate_idx = il3945_rates[prev_idx].table_rs_idx;
	}

	switch (il->band) {
	case IEEE80211_BAND_5GHZ:
		D_RATE("Select A mode rate scale\n");
		for (i = RATE_1M_IDX_TBL; i <= RATE_11M_IDX_TBL; i++)
			table[i].next_rate_idx =
			    il3945_rates[IL_FIRST_OFDM_RATE].table_rs_idx;

		
		table[RATE_12M_IDX_TBL].next_rate_idx = RATE_9M_IDX_TBL;

		
		table[RATE_6M_IDX_TBL].next_rate_idx =
		    il3945_rates[IL_FIRST_OFDM_RATE].table_rs_idx;
		break;

	case IEEE80211_BAND_2GHZ:
		D_RATE("Select B/G mode rate scale\n");

		if (!(il->_3945.sta_supp_rates & IL_OFDM_RATES_MASK) &&
		    il_is_associated(il)) {

			idx = IL_FIRST_CCK_RATE;
			for (i = RATE_6M_IDX_TBL; i <= RATE_54M_IDX_TBL; i++)
				table[i].next_rate_idx =
				    il3945_rates[idx].table_rs_idx;

			idx = RATE_11M_IDX_TBL;
			
			table[idx].next_rate_idx = RATE_5M_IDX_TBL;
		}
		break;

	default:
		WARN_ON(1);
		break;
	}

	
	rate_cmd.table_id = 0;
	rc = il_send_cmd_pdu(il, C_RATE_SCALE, sizeof(rate_cmd), &rate_cmd);
	if (rc)
		return rc;

	
	rate_cmd.table_id = 1;
	return il_send_cmd_pdu(il, C_RATE_SCALE, sizeof(rate_cmd), &rate_cmd);
}

int
il3945_hw_set_hw_params(struct il_priv *il)
{
	memset((void *)&il->hw_params, 0, sizeof(struct il_hw_params));

	il->_3945.shared_virt =
	    dma_alloc_coherent(&il->pci_dev->dev, sizeof(struct il3945_shared),
			       &il->_3945.shared_phys, GFP_KERNEL);
	if (!il->_3945.shared_virt) {
		IL_ERR("failed to allocate pci memory\n");
		return -ENOMEM;
	}

	il->hw_params.bcast_id = IL3945_BROADCAST_ID;

	
	il->hw_params.max_txq_num = il->cfg->num_of_queues;

	il->hw_params.tfd_size = sizeof(struct il3945_tfd);
	il->hw_params.rx_page_order = get_order(IL_RX_BUF_SIZE_3K);
	il->hw_params.max_rxq_size = RX_QUEUE_SIZE;
	il->hw_params.max_rxq_log = RX_QUEUE_SIZE_LOG;
	il->hw_params.max_stations = IL3945_STATION_COUNT;

	il->sta_key_max_num = STA_KEY_MAX_NUM;

	il->hw_params.rx_wrt_ptr_reg = FH39_RSCSR_CHNL0_WPTR;
	il->hw_params.max_beacon_itrvl = IL39_MAX_UCODE_BEACON_INTERVAL;
	il->hw_params.beacon_time_tsf_bits = IL3945_EXT_BEACON_TIME_POS;

	return 0;
}

unsigned int
il3945_hw_get_beacon_cmd(struct il_priv *il, struct il3945_frame *frame,
			 u8 rate)
{
	struct il3945_tx_beacon_cmd *tx_beacon_cmd;
	unsigned int frame_size;

	tx_beacon_cmd = (struct il3945_tx_beacon_cmd *)&frame->u;
	memset(tx_beacon_cmd, 0, sizeof(*tx_beacon_cmd));

	tx_beacon_cmd->tx.sta_id = il->hw_params.bcast_id;
	tx_beacon_cmd->tx.stop_time.life_time = TX_CMD_LIFE_TIME_INFINITE;

	frame_size =
	    il3945_fill_beacon_frame(il, tx_beacon_cmd->frame,
				     sizeof(frame->u) - sizeof(*tx_beacon_cmd));

	BUG_ON(frame_size > MAX_MPDU_SIZE);
	tx_beacon_cmd->tx.len = cpu_to_le16((u16) frame_size);

	tx_beacon_cmd->tx.rate = rate;
	tx_beacon_cmd->tx.tx_flags =
	    (TX_CMD_FLG_SEQ_CTL_MSK | TX_CMD_FLG_TSF_MSK);

	
	tx_beacon_cmd->tx.supp_rates[0] =
	    (IL_OFDM_BASIC_RATES_MASK >> IL_FIRST_OFDM_RATE) & 0xFF;

	tx_beacon_cmd->tx.supp_rates[1] = (IL_CCK_BASIC_RATES_MASK & 0xF);

	return sizeof(struct il3945_tx_beacon_cmd) + frame_size;
}

void
il3945_hw_handler_setup(struct il_priv *il)
{
	il->handlers[C_TX] = il3945_hdl_tx;
	il->handlers[N_3945_RX] = il3945_hdl_rx;
}

void
il3945_hw_setup_deferred_work(struct il_priv *il)
{
	INIT_DELAYED_WORK(&il->_3945.thermal_periodic,
			  il3945_bg_reg_txpower_periodic);
}

void
il3945_hw_cancel_deferred_work(struct il_priv *il)
{
	cancel_delayed_work(&il->_3945.thermal_periodic);
}

static int
il3945_verify_bsm(struct il_priv *il)
{
	__le32 *image = il->ucode_boot.v_addr;
	u32 len = il->ucode_boot.len;
	u32 reg;
	u32 val;

	D_INFO("Begin verify bsm\n");

	
	val = il_rd_prph(il, BSM_WR_DWCOUNT_REG);
	for (reg = BSM_SRAM_LOWER_BOUND; reg < BSM_SRAM_LOWER_BOUND + len;
	     reg += sizeof(u32), image++) {
		val = il_rd_prph(il, reg);
		if (val != le32_to_cpu(*image)) {
			IL_ERR("BSM uCode verification failed at "
			       "addr 0x%08X+%u (of %u), is 0x%x, s/b 0x%x\n",
			       BSM_SRAM_LOWER_BOUND, reg - BSM_SRAM_LOWER_BOUND,
			       len, val, le32_to_cpu(*image));
			return -EIO;
		}
	}

	D_INFO("BSM bootstrap uCode image OK\n");

	return 0;
}


static int
il3945_eeprom_acquire_semaphore(struct il_priv *il)
{
	_il_clear_bit(il, CSR_EEPROM_GP, CSR_EEPROM_GP_IF_OWNER_MSK);
	return 0;
}

static void
il3945_eeprom_release_semaphore(struct il_priv *il)
{
	return;
}

static int
il3945_load_bsm(struct il_priv *il)
{
	__le32 *image = il->ucode_boot.v_addr;
	u32 len = il->ucode_boot.len;
	dma_addr_t pinst;
	dma_addr_t pdata;
	u32 inst_len;
	u32 data_len;
	int rc;
	int i;
	u32 done;
	u32 reg_offset;

	D_INFO("Begin load bsm\n");

	
	if (len > IL39_MAX_BSM_SIZE)
		return -EINVAL;

	pinst = il->ucode_init.p_addr;
	pdata = il->ucode_init_data.p_addr;
	inst_len = il->ucode_init.len;
	data_len = il->ucode_init_data.len;

	il_wr_prph(il, BSM_DRAM_INST_PTR_REG, pinst);
	il_wr_prph(il, BSM_DRAM_DATA_PTR_REG, pdata);
	il_wr_prph(il, BSM_DRAM_INST_BYTECOUNT_REG, inst_len);
	il_wr_prph(il, BSM_DRAM_DATA_BYTECOUNT_REG, data_len);

	
	for (reg_offset = BSM_SRAM_LOWER_BOUND;
	     reg_offset < BSM_SRAM_LOWER_BOUND + len;
	     reg_offset += sizeof(u32), image++)
		_il_wr_prph(il, reg_offset, le32_to_cpu(*image));

	rc = il3945_verify_bsm(il);
	if (rc)
		return rc;

	
	il_wr_prph(il, BSM_WR_MEM_SRC_REG, 0x0);
	il_wr_prph(il, BSM_WR_MEM_DST_REG, IL39_RTC_INST_LOWER_BOUND);
	il_wr_prph(il, BSM_WR_DWCOUNT_REG, len / sizeof(u32));

	il_wr_prph(il, BSM_WR_CTRL_REG, BSM_WR_CTRL_REG_BIT_START);

	
	for (i = 0; i < 100; i++) {
		done = il_rd_prph(il, BSM_WR_CTRL_REG);
		if (!(done & BSM_WR_CTRL_REG_BIT_START))
			break;
		udelay(10);
	}
	if (i < 100)
		D_INFO("BSM write complete, poll %d iterations\n", i);
	else {
		IL_ERR("BSM write did not complete!\n");
		return -EIO;
	}

	il_wr_prph(il, BSM_WR_CTRL_REG, BSM_WR_CTRL_REG_BIT_START_EN);

	return 0;
}

const struct il_ops il3945_ops = {
	.txq_attach_buf_to_tfd = il3945_hw_txq_attach_buf_to_tfd,
	.txq_free_tfd = il3945_hw_txq_free_tfd,
	.txq_init = il3945_hw_tx_queue_init,
	.load_ucode = il3945_load_bsm,
	.dump_nic_error_log = il3945_dump_nic_error_log,
	.apm_init = il3945_apm_init,
	.send_tx_power = il3945_send_tx_power,
	.is_valid_rtc_data_addr = il3945_hw_valid_rtc_data_addr,
	.eeprom_acquire_semaphore = il3945_eeprom_acquire_semaphore,
	.eeprom_release_semaphore = il3945_eeprom_release_semaphore,

	.rxon_assoc = il3945_send_rxon_assoc,
	.commit_rxon = il3945_commit_rxon,

	.get_hcmd_size = il3945_get_hcmd_size,
	.build_addsta_hcmd = il3945_build_addsta_hcmd,
	.request_scan = il3945_request_scan,
	.post_scan = il3945_post_scan,

	.post_associate = il3945_post_associate,
	.config_ap = il3945_config_ap,
	.manage_ibss_station = il3945_manage_ibss_station,

	.send_led_cmd = il3945_send_led_cmd,
};

static struct il_cfg il3945_bg_cfg = {
	.name = "3945BG",
	.fw_name_pre = IL3945_FW_PRE,
	.ucode_api_max = IL3945_UCODE_API_MAX,
	.ucode_api_min = IL3945_UCODE_API_MIN,
	.sku = IL_SKU_G,
	.eeprom_ver = EEPROM_3945_EEPROM_VERSION,
	.mod_params = &il3945_mod_params,
	.led_mode = IL_LED_BLINK,

	.eeprom_size = IL3945_EEPROM_IMG_SIZE,
	.num_of_queues = IL39_NUM_QUEUES,
	.pll_cfg_val = CSR39_ANA_PLL_CFG_VAL,
	.set_l0s = false,
	.use_bsm = true,
	.led_compensation = 64,
	.wd_timeout = IL_DEF_WD_TIMEOUT,

	.regulatory_bands = {
		EEPROM_REGULATORY_BAND_1_CHANNELS,
		EEPROM_REGULATORY_BAND_2_CHANNELS,
		EEPROM_REGULATORY_BAND_3_CHANNELS,
		EEPROM_REGULATORY_BAND_4_CHANNELS,
		EEPROM_REGULATORY_BAND_5_CHANNELS,
		EEPROM_REGULATORY_BAND_NO_HT40,
		EEPROM_REGULATORY_BAND_NO_HT40,
	},
};

static struct il_cfg il3945_abg_cfg = {
	.name = "3945ABG",
	.fw_name_pre = IL3945_FW_PRE,
	.ucode_api_max = IL3945_UCODE_API_MAX,
	.ucode_api_min = IL3945_UCODE_API_MIN,
	.sku = IL_SKU_A | IL_SKU_G,
	.eeprom_ver = EEPROM_3945_EEPROM_VERSION,
	.mod_params = &il3945_mod_params,
	.led_mode = IL_LED_BLINK,

	.eeprom_size = IL3945_EEPROM_IMG_SIZE,
	.num_of_queues = IL39_NUM_QUEUES,
	.pll_cfg_val = CSR39_ANA_PLL_CFG_VAL,
	.set_l0s = false,
	.use_bsm = true,
	.led_compensation = 64,
	.wd_timeout = IL_DEF_WD_TIMEOUT,

	.regulatory_bands = {
		EEPROM_REGULATORY_BAND_1_CHANNELS,
		EEPROM_REGULATORY_BAND_2_CHANNELS,
		EEPROM_REGULATORY_BAND_3_CHANNELS,
		EEPROM_REGULATORY_BAND_4_CHANNELS,
		EEPROM_REGULATORY_BAND_5_CHANNELS,
		EEPROM_REGULATORY_BAND_NO_HT40,
		EEPROM_REGULATORY_BAND_NO_HT40,
	},
};

DEFINE_PCI_DEVICE_TABLE(il3945_hw_card_ids) = {
	{IL_PCI_DEVICE(0x4222, 0x1005, il3945_bg_cfg)},
	{IL_PCI_DEVICE(0x4222, 0x1034, il3945_bg_cfg)},
	{IL_PCI_DEVICE(0x4222, 0x1044, il3945_bg_cfg)},
	{IL_PCI_DEVICE(0x4227, 0x1014, il3945_bg_cfg)},
	{IL_PCI_DEVICE(0x4222, PCI_ANY_ID, il3945_abg_cfg)},
	{IL_PCI_DEVICE(0x4227, PCI_ANY_ID, il3945_abg_cfg)},
	{0}
};

MODULE_DEVICE_TABLE(pci, il3945_hw_card_ids);
