/*
 * Copyright (c) 2004-2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006-2008 Nick Kossifidis <mickflemm@gmail.com>
 * Copyright (c) 2007-2008 Matthew W. S. Bell  <mentor@madwifi.org>
 * Copyright (c) 2007-2008 Luis Rodriguez <mcgrof@winlab.rutgers.edu>
 * Copyright (c) 2007-2008 Pavel Roskin <proski@gnu.org>
 * Copyright (c) 2007-2008 Jiri Slaby <jirislaby@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


#include <asm/unaligned.h>

#include "ath5k.h"
#include "reg.h"
#include "debug.h"


static const unsigned int ack_rates_high[] =
	{ 0,
	1,
	1,
	1,
	4,
	4,
	6,
	6,
	8,
	8,
	8,
	8 };


int
ath5k_hw_get_frame_duration(struct ath5k_hw *ah,
		int len, struct ieee80211_rate *rate, bool shortpre)
{
	int sifs, preamble, plcp_bits, sym_time;
	int bitrate, bits, symbols, symbol_bits;
	int dur;

	
	if (!ah->ah_bwmode) {
		__le16 raw_dur = ieee80211_generic_frame_duration(ah->hw,
					NULL, len, rate);

		
		dur = le16_to_cpu(raw_dur);
		if (shortpre)
			dur -= 96;

		return dur;
	}

	bitrate = rate->bitrate;
	preamble = AR5K_INIT_OFDM_PREAMPLE_TIME;
	plcp_bits = AR5K_INIT_OFDM_PLCP_BITS;
	sym_time = AR5K_INIT_OFDM_SYMBOL_TIME;

	switch (ah->ah_bwmode) {
	case AR5K_BWMODE_40MHZ:
		sifs = AR5K_INIT_SIFS_TURBO;
		preamble = AR5K_INIT_OFDM_PREAMBLE_TIME_MIN;
		break;
	case AR5K_BWMODE_10MHZ:
		sifs = AR5K_INIT_SIFS_HALF_RATE;
		preamble *= 2;
		sym_time *= 2;
		break;
	case AR5K_BWMODE_5MHZ:
		sifs = AR5K_INIT_SIFS_QUARTER_RATE;
		preamble *= 4;
		sym_time *= 4;
		break;
	default:
		sifs = AR5K_INIT_SIFS_DEFAULT_BG;
		break;
	}

	bits = plcp_bits + (len << 3);
	
	symbol_bits = bitrate * sym_time;
	symbols = DIV_ROUND_UP(bits * 10, symbol_bits);

	dur = sifs + preamble + (sym_time * symbols);

	return dur;
}

unsigned int
ath5k_hw_get_default_slottime(struct ath5k_hw *ah)
{
	struct ieee80211_channel *channel = ah->ah_current_channel;
	unsigned int slot_time;

	switch (ah->ah_bwmode) {
	case AR5K_BWMODE_40MHZ:
		slot_time = AR5K_INIT_SLOT_TIME_TURBO;
		break;
	case AR5K_BWMODE_10MHZ:
		slot_time = AR5K_INIT_SLOT_TIME_HALF_RATE;
		break;
	case AR5K_BWMODE_5MHZ:
		slot_time = AR5K_INIT_SLOT_TIME_QUARTER_RATE;
		break;
	case AR5K_BWMODE_DEFAULT:
	default:
		slot_time = AR5K_INIT_SLOT_TIME_DEFAULT;
		if ((channel->hw_value == AR5K_MODE_11B) && !ah->ah_short_slot)
			slot_time = AR5K_INIT_SLOT_TIME_B;
		break;
	}

	return slot_time;
}

unsigned int
ath5k_hw_get_default_sifs(struct ath5k_hw *ah)
{
	struct ieee80211_channel *channel = ah->ah_current_channel;
	unsigned int sifs;

	switch (ah->ah_bwmode) {
	case AR5K_BWMODE_40MHZ:
		sifs = AR5K_INIT_SIFS_TURBO;
		break;
	case AR5K_BWMODE_10MHZ:
		sifs = AR5K_INIT_SIFS_HALF_RATE;
		break;
	case AR5K_BWMODE_5MHZ:
		sifs = AR5K_INIT_SIFS_QUARTER_RATE;
		break;
	case AR5K_BWMODE_DEFAULT:
		sifs = AR5K_INIT_SIFS_DEFAULT_BG;
	default:
		if (channel->band == IEEE80211_BAND_5GHZ)
			sifs = AR5K_INIT_SIFS_DEFAULT_A;
		break;
	}

	return sifs;
}

void
ath5k_hw_update_mib_counters(struct ath5k_hw *ah)
{
	struct ath5k_statistics *stats = &ah->stats;

	
	stats->ack_fail += ath5k_hw_reg_read(ah, AR5K_ACK_FAIL);
	stats->rts_fail += ath5k_hw_reg_read(ah, AR5K_RTS_FAIL);
	stats->rts_ok += ath5k_hw_reg_read(ah, AR5K_RTS_OK);
	stats->fcs_error += ath5k_hw_reg_read(ah, AR5K_FCS_FAIL);
	stats->beacons += ath5k_hw_reg_read(ah, AR5K_BEACON_CNT);
}



static inline void
ath5k_hw_write_rate_duration(struct ath5k_hw *ah)
{
	struct ieee80211_rate *rate;
	unsigned int i;
	
	u8 band = IEEE80211_BAND_2GHZ;

	
	for (i = 0; i < ah->sbands[band].n_bitrates; i++) {
		u32 reg;
		u16 tx_time;

		if (ah->ah_ack_bitrate_high)
			rate = &ah->sbands[band].bitrates[ack_rates_high[i]];
		
		else if (i < 4)
			rate = &ah->sbands[band].bitrates[0];
		
		else
			rate = &ah->sbands[band].bitrates[4];

		
		reg = AR5K_RATE_DUR(rate->hw_value);

		tx_time = ath5k_hw_get_frame_duration(ah, 10, rate, false);

		ath5k_hw_reg_write(ah, tx_time, reg);

		if (!(rate->flags & IEEE80211_RATE_SHORT_PREAMBLE))
			continue;

		tx_time = ath5k_hw_get_frame_duration(ah, 10, rate, true);
		ath5k_hw_reg_write(ah, tx_time,
			reg + (AR5K_SET_SHORT_PREAMBLE << 2));
	}
}

static int
ath5k_hw_set_ack_timeout(struct ath5k_hw *ah, unsigned int timeout)
{
	if (ath5k_hw_clocktoh(ah, AR5K_REG_MS(0xffffffff, AR5K_TIME_OUT_ACK))
			<= timeout)
		return -EINVAL;

	AR5K_REG_WRITE_BITS(ah, AR5K_TIME_OUT, AR5K_TIME_OUT_ACK,
		ath5k_hw_htoclock(ah, timeout));

	return 0;
}

static int
ath5k_hw_set_cts_timeout(struct ath5k_hw *ah, unsigned int timeout)
{
	if (ath5k_hw_clocktoh(ah, AR5K_REG_MS(0xffffffff, AR5K_TIME_OUT_CTS))
			<= timeout)
		return -EINVAL;

	AR5K_REG_WRITE_BITS(ah, AR5K_TIME_OUT, AR5K_TIME_OUT_CTS,
			ath5k_hw_htoclock(ah, timeout));

	return 0;
}



int
ath5k_hw_set_lladdr(struct ath5k_hw *ah, const u8 *mac)
{
	struct ath_common *common = ath5k_hw_common(ah);
	u32 low_id, high_id;
	u32 pcu_reg;

	
	memcpy(common->macaddr, mac, ETH_ALEN);

	pcu_reg = ath5k_hw_reg_read(ah, AR5K_STA_ID1) & 0xffff0000;

	low_id = get_unaligned_le32(mac);
	high_id = get_unaligned_le16(mac + 4);

	ath5k_hw_reg_write(ah, low_id, AR5K_STA_ID0);
	ath5k_hw_reg_write(ah, pcu_reg | high_id, AR5K_STA_ID1);

	return 0;
}

void
ath5k_hw_set_bssid(struct ath5k_hw *ah)
{
	struct ath_common *common = ath5k_hw_common(ah);
	u16 tim_offset = 0;

	if (ah->ah_version == AR5K_AR5212)
		ath_hw_setbssidmask(common);

	ath5k_hw_reg_write(ah,
			   get_unaligned_le32(common->curbssid),
			   AR5K_BSS_ID0);
	ath5k_hw_reg_write(ah,
			   get_unaligned_le16(common->curbssid + 4) |
			   ((common->curaid & 0x3fff) << AR5K_BSS_ID1_AID_S),
			   AR5K_BSS_ID1);

	if (common->curaid == 0) {
		ath5k_hw_disable_pspoll(ah);
		return;
	}

	AR5K_REG_WRITE_BITS(ah, AR5K_BEACON, AR5K_BEACON_TIM,
			    tim_offset ? tim_offset + 4 : 0);

	ath5k_hw_enable_pspoll(ah, NULL, 0);
}

void
ath5k_hw_set_bssid_mask(struct ath5k_hw *ah, const u8 *mask)
{
	struct ath_common *common = ath5k_hw_common(ah);

	memcpy(common->bssidmask, mask, ETH_ALEN);
	if (ah->ah_version == AR5K_AR5212)
		ath_hw_setbssidmask(common);
}

void
ath5k_hw_set_mcast_filter(struct ath5k_hw *ah, u32 filter0, u32 filter1)
{
	ath5k_hw_reg_write(ah, filter0, AR5K_MCAST_FILTER0);
	ath5k_hw_reg_write(ah, filter1, AR5K_MCAST_FILTER1);
}

u32
ath5k_hw_get_rx_filter(struct ath5k_hw *ah)
{
	u32 data, filter = 0;

	filter = ath5k_hw_reg_read(ah, AR5K_RX_FILTER);

	
	if (ah->ah_version == AR5K_AR5212) {
		data = ath5k_hw_reg_read(ah, AR5K_PHY_ERR_FIL);

		if (data & AR5K_PHY_ERR_FIL_RADAR)
			filter |= AR5K_RX_FILTER_RADARERR;
		if (data & (AR5K_PHY_ERR_FIL_OFDM | AR5K_PHY_ERR_FIL_CCK))
			filter |= AR5K_RX_FILTER_PHYERR;
	}

	return filter;
}

void
ath5k_hw_set_rx_filter(struct ath5k_hw *ah, u32 filter)
{
	u32 data = 0;

	
	if (ah->ah_version == AR5K_AR5212) {
		if (filter & AR5K_RX_FILTER_RADARERR)
			data |= AR5K_PHY_ERR_FIL_RADAR;
		if (filter & AR5K_RX_FILTER_PHYERR)
			data |= AR5K_PHY_ERR_FIL_OFDM | AR5K_PHY_ERR_FIL_CCK;
	}

	if (ah->ah_version == AR5K_AR5210 &&
			(filter & AR5K_RX_FILTER_RADARERR)) {
		filter &= ~AR5K_RX_FILTER_RADARERR;
		filter |= AR5K_RX_FILTER_PROM;
	}

	
	if (data)
		AR5K_REG_ENABLE_BITS(ah, AR5K_RXCFG, AR5K_RXCFG_ZLFDMA);
	else
		AR5K_REG_DISABLE_BITS(ah, AR5K_RXCFG, AR5K_RXCFG_ZLFDMA);

	
	ath5k_hw_reg_write(ah, filter & 0xff, AR5K_RX_FILTER);

	
	if (ah->ah_version == AR5K_AR5212)
		ath5k_hw_reg_write(ah, data, AR5K_PHY_ERR_FIL);

}



#define ATH5K_MAX_TSF_READ 10

u64
ath5k_hw_get_tsf64(struct ath5k_hw *ah)
{
	u32 tsf_lower, tsf_upper1, tsf_upper2;
	int i;
	unsigned long flags;

	
	local_irq_save(flags);


	tsf_upper1 = ath5k_hw_reg_read(ah, AR5K_TSF_U32);
	for (i = 0; i < ATH5K_MAX_TSF_READ; i++) {
		tsf_lower = ath5k_hw_reg_read(ah, AR5K_TSF_L32);
		tsf_upper2 = ath5k_hw_reg_read(ah, AR5K_TSF_U32);
		if (tsf_upper2 == tsf_upper1)
			break;
		tsf_upper1 = tsf_upper2;
	}

	local_irq_restore(flags);

	WARN_ON(i == ATH5K_MAX_TSF_READ);

	return ((u64)tsf_upper1 << 32) | tsf_lower;
}

#undef ATH5K_MAX_TSF_READ

void
ath5k_hw_set_tsf64(struct ath5k_hw *ah, u64 tsf64)
{
	ath5k_hw_reg_write(ah, tsf64 & 0xffffffff, AR5K_TSF_L32);
	ath5k_hw_reg_write(ah, (tsf64 >> 32) & 0xffffffff, AR5K_TSF_U32);
}

void
ath5k_hw_reset_tsf(struct ath5k_hw *ah)
{
	u32 val;

	val = ath5k_hw_reg_read(ah, AR5K_BEACON) | AR5K_BEACON_RESET_TSF;

	ath5k_hw_reg_write(ah, val, AR5K_BEACON);
	ath5k_hw_reg_write(ah, val, AR5K_BEACON);
}

void
ath5k_hw_init_beacon_timers(struct ath5k_hw *ah, u32 next_beacon, u32 interval)
{
	u32 timer1, timer2, timer3;

	switch (ah->opmode) {
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_STATION:
		
		if (ah->ah_version == AR5K_AR5210) {
			timer1 = 0xffffffff;
			timer2 = 0xffffffff;
		} else {
			timer1 = 0x0000ffff;
			timer2 = 0x0007ffff;
		}
		
		AR5K_REG_DISABLE_BITS(ah, AR5K_STA_ID1, AR5K_STA_ID1_PCF);
		break;
	case NL80211_IFTYPE_ADHOC:
		AR5K_REG_ENABLE_BITS(ah, AR5K_TXCFG, AR5K_TXCFG_ADHOC_BCN_ATIM);
	default:
		timer1 = (next_beacon - AR5K_TUNE_DMA_BEACON_RESP) << 3;
		timer2 = (next_beacon - AR5K_TUNE_SW_BEACON_RESP) << 3;
		break;
	}

	timer3 = next_beacon + 1;

	
	if (ah->opmode == NL80211_IFTYPE_AP ||
	    ah->opmode == NL80211_IFTYPE_MESH_POINT)
		ath5k_hw_reg_write(ah, 0, AR5K_TIMER0);

	ath5k_hw_reg_write(ah, next_beacon, AR5K_TIMER0);
	ath5k_hw_reg_write(ah, timer1, AR5K_TIMER1);
	ath5k_hw_reg_write(ah, timer2, AR5K_TIMER2);
	ath5k_hw_reg_write(ah, timer3, AR5K_TIMER3);

	
	if (interval & AR5K_BEACON_RESET_TSF)
		ath5k_hw_reset_tsf(ah);

	ath5k_hw_reg_write(ah, interval & (AR5K_BEACON_PERIOD |
					AR5K_BEACON_ENABLE),
						AR5K_BEACON);

	if (ah->ah_version == AR5K_AR5210)
		ath5k_hw_reg_write(ah, AR5K_ISR_BMISS, AR5K_ISR);
	else
		ath5k_hw_reg_write(ah, AR5K_ISR_BMISS, AR5K_PISR);

	AR5K_REG_DISABLE_BITS(ah, AR5K_STA_ID1, AR5K_STA_ID1_PWR_SV);

}

static inline bool
ath5k_check_timer_win(int a, int b, int window, int intval)
{
	if ((b - a == window) ||				
	    (a - b == intval - window) ||			
	    ((a | 0x10000) - b == intval - window) ||		
	    ((b | 0x10000) - a == window))			
		return true; 
	return false;
}

bool
ath5k_hw_check_beacon_timers(struct ath5k_hw *ah, int intval)
{
	unsigned int nbtt, atim, dma;

	nbtt = ath5k_hw_reg_read(ah, AR5K_TIMER0);
	atim = ath5k_hw_reg_read(ah, AR5K_TIMER3);
	dma = ath5k_hw_reg_read(ah, AR5K_TIMER1) >> 3;


	if (ath5k_check_timer_win(nbtt, atim, 1, intval) &&
	    ath5k_check_timer_win(dma, nbtt, AR5K_TUNE_DMA_BEACON_RESP,
				  intval))
		return true; 
	return false;
}

void
ath5k_hw_set_coverage_class(struct ath5k_hw *ah, u8 coverage_class)
{
	
	int slot_time = ath5k_hw_get_default_slottime(ah) + 3 * coverage_class;
	int ack_timeout = ath5k_hw_get_default_sifs(ah) + slot_time;
	int cts_timeout = ack_timeout;

	ath5k_hw_set_ifs_intervals(ah, slot_time);
	ath5k_hw_set_ack_timeout(ah, ack_timeout);
	ath5k_hw_set_cts_timeout(ah, cts_timeout);

	ah->ah_coverage_class = coverage_class;
}


void
ath5k_hw_start_rx_pcu(struct ath5k_hw *ah)
{
	AR5K_REG_DISABLE_BITS(ah, AR5K_DIAG_SW, AR5K_DIAG_SW_DIS_RX);
}

void
ath5k_hw_stop_rx_pcu(struct ath5k_hw *ah)
{
	AR5K_REG_ENABLE_BITS(ah, AR5K_DIAG_SW, AR5K_DIAG_SW_DIS_RX);
}

int
ath5k_hw_set_opmode(struct ath5k_hw *ah, enum nl80211_iftype op_mode)
{
	struct ath_common *common = ath5k_hw_common(ah);
	u32 pcu_reg, beacon_reg, low_id, high_id;

	ATH5K_DBG(ah, ATH5K_DEBUG_MODE, "mode %d\n", op_mode);

	
	pcu_reg = ath5k_hw_reg_read(ah, AR5K_STA_ID1) & 0xffff0000;
	pcu_reg &= ~(AR5K_STA_ID1_ADHOC | AR5K_STA_ID1_AP
			| AR5K_STA_ID1_KEYSRCH_MODE
			| (ah->ah_version == AR5K_AR5210 ?
			(AR5K_STA_ID1_PWR_SV | AR5K_STA_ID1_NO_PSPOLL) : 0));

	beacon_reg = 0;

	switch (op_mode) {
	case NL80211_IFTYPE_ADHOC:
		pcu_reg |= AR5K_STA_ID1_ADHOC | AR5K_STA_ID1_KEYSRCH_MODE;
		beacon_reg |= AR5K_BCR_ADHOC;
		if (ah->ah_version == AR5K_AR5210)
			pcu_reg |= AR5K_STA_ID1_NO_PSPOLL;
		else
			AR5K_REG_ENABLE_BITS(ah, AR5K_CFG, AR5K_CFG_IBSS);
		break;

	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_MESH_POINT:
		pcu_reg |= AR5K_STA_ID1_AP | AR5K_STA_ID1_KEYSRCH_MODE;
		beacon_reg |= AR5K_BCR_AP;
		if (ah->ah_version == AR5K_AR5210)
			pcu_reg |= AR5K_STA_ID1_NO_PSPOLL;
		else
			AR5K_REG_DISABLE_BITS(ah, AR5K_CFG, AR5K_CFG_IBSS);
		break;

	case NL80211_IFTYPE_STATION:
		pcu_reg |= AR5K_STA_ID1_KEYSRCH_MODE
			| (ah->ah_version == AR5K_AR5210 ?
				AR5K_STA_ID1_PWR_SV : 0);
	case NL80211_IFTYPE_MONITOR:
		pcu_reg |= AR5K_STA_ID1_KEYSRCH_MODE
			| (ah->ah_version == AR5K_AR5210 ?
				AR5K_STA_ID1_NO_PSPOLL : 0);
		break;

	default:
		return -EINVAL;
	}

	low_id = get_unaligned_le32(common->macaddr);
	high_id = get_unaligned_le16(common->macaddr + 4);
	ath5k_hw_reg_write(ah, low_id, AR5K_STA_ID0);
	ath5k_hw_reg_write(ah, pcu_reg | high_id, AR5K_STA_ID1);

	if (ah->ah_version == AR5K_AR5210)
		ath5k_hw_reg_write(ah, beacon_reg, AR5K_BCR);

	return 0;
}

void
ath5k_hw_pcu_init(struct ath5k_hw *ah, enum nl80211_iftype op_mode)
{
	
	ath5k_hw_set_bssid(ah);

	
	ath5k_hw_set_opmode(ah, op_mode);

	if (ah->ah_version == AR5K_AR5212 &&
		ah->nvifs)
		ath5k_hw_write_rate_duration(ah);

	ath5k_hw_reg_write(ah, (AR5K_TUNE_RSSI_THRES |
				AR5K_TUNE_BMISS_THRES <<
				AR5K_RSSI_THR_BMISS_S),
				AR5K_RSSI_THR);

	
	if (ah->ah_mac_srev >= AR5K_SREV_AR2413) {
		ath5k_hw_reg_write(ah, 0x000100aa, AR5K_MIC_QOS_CTL);
		ath5k_hw_reg_write(ah, 0x00003210, AR5K_MIC_QOS_SEL);
	}

	
	if (ah->ah_version == AR5K_AR5212) {
		ath5k_hw_reg_write(ah,
			AR5K_REG_SM(2, AR5K_QOS_NOACK_2BIT_VALUES) |
			AR5K_REG_SM(5, AR5K_QOS_NOACK_BIT_OFFSET)  |
			AR5K_REG_SM(0, AR5K_QOS_NOACK_BYTE_OFFSET),
			AR5K_QOS_NOACK);
	}

	
	if (ah->ah_coverage_class > 0)
		ath5k_hw_set_coverage_class(ah, ah->ah_coverage_class);

	
	if (ah->ah_version == AR5K_AR5212) {
		u32 val = AR5K_STA_ID1_BASE_RATE_11B | AR5K_STA_ID1_ACKCTS_6MB;
		if (ah->ah_ack_bitrate_high)
			AR5K_REG_DISABLE_BITS(ah, AR5K_STA_ID1, val);
		else
			AR5K_REG_ENABLE_BITS(ah, AR5K_STA_ID1, val);
	}
	return;
}
