/*-
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2004-2005 Atheros Communications, Inc.
 * Copyright (c) 2006 Devicescape Software, Inc.
 * Copyright (c) 2007 Jiri Slaby <jirislaby@gmail.com>
 * Copyright (c) 2007 Luis R. Rodriguez <mcgrof@winlab.rutgers.edu>
 * Copyright (c) 2010 Bruno Randolf <br1@einfach.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 */

#include <net/mac80211.h>
#include <asm/unaligned.h>

#include "ath5k.h"
#include "base.h"
#include "reg.h"


static void
ath5k_tx(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct ath5k_hw *ah = hw->priv;
	u16 qnum = skb_get_queue_mapping(skb);

	if (WARN_ON(qnum >= ah->ah_capabilities.cap_queues.q_tx_num)) {
		dev_kfree_skb_any(skb);
		return;
	}

	ath5k_tx_queue(hw, skb, &ah->txqs[qnum]);
}


static int
ath5k_add_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct ath5k_hw *ah = hw->priv;
	int ret;
	struct ath5k_vif *avf = (void *)vif->drv_priv;

	mutex_lock(&ah->lock);

	if ((vif->type == NL80211_IFTYPE_AP ||
	     vif->type == NL80211_IFTYPE_ADHOC)
	    && (ah->num_ap_vifs + ah->num_adhoc_vifs) >= ATH_BCBUF) {
		ret = -ELNRNG;
		goto end;
	}

	if (ah->num_adhoc_vifs ||
	    (ah->nvifs && vif->type == NL80211_IFTYPE_ADHOC)) {
		ATH5K_ERR(ah, "Only one single ad-hoc interface is allowed.\n");
		ret = -ELNRNG;
		goto end;
	}

	switch (vif->type) {
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_ADHOC:
	case NL80211_IFTYPE_MESH_POINT:
		avf->opmode = vif->type;
		break;
	default:
		ret = -EOPNOTSUPP;
		goto end;
	}

	ah->nvifs++;
	ATH5K_DBG(ah, ATH5K_DEBUG_MODE, "add interface mode %d\n", avf->opmode);

	
	if ((avf->opmode == NL80211_IFTYPE_AP) ||
	    (avf->opmode == NL80211_IFTYPE_ADHOC) ||
	    (avf->opmode == NL80211_IFTYPE_MESH_POINT)) {
		int slot;

		WARN_ON(list_empty(&ah->bcbuf));
		avf->bbuf = list_first_entry(&ah->bcbuf, struct ath5k_buf,
					     list);
		list_del(&avf->bbuf->list);

		avf->bslot = 0;
		for (slot = 0; slot < ATH_BCBUF; slot++) {
			if (!ah->bslot[slot]) {
				avf->bslot = slot;
				break;
			}
		}
		BUG_ON(ah->bslot[avf->bslot] != NULL);
		ah->bslot[avf->bslot] = vif;
		if (avf->opmode == NL80211_IFTYPE_AP)
			ah->num_ap_vifs++;
		else if (avf->opmode == NL80211_IFTYPE_ADHOC)
			ah->num_adhoc_vifs++;
		else if (avf->opmode == NL80211_IFTYPE_MESH_POINT)
			ah->num_mesh_vifs++;
	}

	ath5k_hw_set_lladdr(ah, vif->addr);

	ath5k_update_bssid_mask_and_opmode(ah, vif);
	ret = 0;
end:
	mutex_unlock(&ah->lock);
	return ret;
}


static void
ath5k_remove_interface(struct ieee80211_hw *hw,
		       struct ieee80211_vif *vif)
{
	struct ath5k_hw *ah = hw->priv;
	struct ath5k_vif *avf = (void *)vif->drv_priv;
	unsigned int i;

	mutex_lock(&ah->lock);
	ah->nvifs--;

	if (avf->bbuf) {
		ath5k_txbuf_free_skb(ah, avf->bbuf);
		list_add_tail(&avf->bbuf->list, &ah->bcbuf);
		for (i = 0; i < ATH_BCBUF; i++) {
			if (ah->bslot[i] == vif) {
				ah->bslot[i] = NULL;
				break;
			}
		}
		avf->bbuf = NULL;
	}
	if (avf->opmode == NL80211_IFTYPE_AP)
		ah->num_ap_vifs--;
	else if (avf->opmode == NL80211_IFTYPE_ADHOC)
		ah->num_adhoc_vifs--;
	else if (avf->opmode == NL80211_IFTYPE_MESH_POINT)
		ah->num_mesh_vifs--;

	ath5k_update_bssid_mask_and_opmode(ah, NULL);
	mutex_unlock(&ah->lock);
}


static int
ath5k_config(struct ieee80211_hw *hw, u32 changed)
{
	struct ath5k_hw *ah = hw->priv;
	struct ieee80211_conf *conf = &hw->conf;
	int ret = 0;
	int i;

	mutex_lock(&ah->lock);

	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		ret = ath5k_chan_set(ah, conf->channel);
		if (ret < 0)
			goto unlock;
	}

	if ((changed & IEEE80211_CONF_CHANGE_POWER) &&
	(ah->power_level != conf->power_level)) {
		ah->power_level = conf->power_level;

		
		ath5k_hw_set_txpower_limit(ah, (conf->power_level * 2));
	}

	if (changed & IEEE80211_CONF_CHANGE_RETRY_LIMITS) {
		ah->ah_retry_long = conf->long_frame_max_tx_count;
		ah->ah_retry_short = conf->short_frame_max_tx_count;

		for (i = 0; i < ah->ah_capabilities.cap_queues.q_tx_num; i++)
			ath5k_hw_set_tx_retry_limits(ah, i);
	}

	ath5k_hw_set_antenna_mode(ah, ah->ah_ant_mode);

unlock:
	mutex_unlock(&ah->lock);
	return ret;
}


static void
ath5k_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		       struct ieee80211_bss_conf *bss_conf, u32 changes)
{
	struct ath5k_vif *avf = (void *)vif->drv_priv;
	struct ath5k_hw *ah = hw->priv;
	struct ath_common *common = ath5k_hw_common(ah);
	unsigned long flags;

	mutex_lock(&ah->lock);

	if (changes & BSS_CHANGED_BSSID) {
		
		memcpy(common->curbssid, bss_conf->bssid, ETH_ALEN);
		common->curaid = 0;
		ath5k_hw_set_bssid(ah);
		mmiowb();
	}

	if (changes & BSS_CHANGED_BEACON_INT)
		ah->bintval = bss_conf->beacon_int;

	if (changes & BSS_CHANGED_ERP_SLOT) {
		int slot_time;

		ah->ah_short_slot = bss_conf->use_short_slot;
		slot_time = ath5k_hw_get_default_slottime(ah) +
			    3 * ah->ah_coverage_class;
		ath5k_hw_set_ifs_intervals(ah, slot_time);
	}

	if (changes & BSS_CHANGED_ASSOC) {
		avf->assoc = bss_conf->assoc;
		if (bss_conf->assoc)
			ah->assoc = bss_conf->assoc;
		else
			ah->assoc = ath5k_any_vif_assoc(ah);

		if (ah->opmode == NL80211_IFTYPE_STATION)
			ath5k_set_beacon_filter(hw, ah->assoc);
		ath5k_hw_set_ledstate(ah, ah->assoc ?
			AR5K_LED_ASSOC : AR5K_LED_INIT);
		if (bss_conf->assoc) {
			ATH5K_DBG(ah, ATH5K_DEBUG_ANY,
				  "Bss Info ASSOC %d, bssid: %pM\n",
				  bss_conf->aid, common->curbssid);
			common->curaid = bss_conf->aid;
			ath5k_hw_set_bssid(ah);
			
		}
	}

	if (changes & BSS_CHANGED_BEACON) {
		spin_lock_irqsave(&ah->block, flags);
		ath5k_beacon_update(hw, vif);
		spin_unlock_irqrestore(&ah->block, flags);
	}

	if (changes & BSS_CHANGED_BEACON_ENABLED)
		ah->enable_beacon = bss_conf->enable_beacon;

	if (changes & (BSS_CHANGED_BEACON | BSS_CHANGED_BEACON_ENABLED |
		       BSS_CHANGED_BEACON_INT))
		ath5k_beacon_config(ah);

	mutex_unlock(&ah->lock);
}


static u64
ath5k_prepare_multicast(struct ieee80211_hw *hw,
			struct netdev_hw_addr_list *mc_list)
{
	u32 mfilt[2], val;
	u8 pos;
	struct netdev_hw_addr *ha;

	mfilt[0] = 0;
	mfilt[1] = 1;

	netdev_hw_addr_list_for_each(ha, mc_list) {
		
		val = get_unaligned_le32(ha->addr + 0);
		pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
		val = get_unaligned_le32(ha->addr + 3);
		pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
		pos &= 0x3f;
		mfilt[pos / 32] |= (1 << (pos % 32));
	}

	return ((u64)(mfilt[1]) << 32) | mfilt[0];
}


static void
ath5k_configure_filter(struct ieee80211_hw *hw, unsigned int changed_flags,
		       unsigned int *new_flags, u64 multicast)
{
#define SUPPORTED_FIF_FLAGS \
	(FIF_PROMISC_IN_BSS |  FIF_ALLMULTI | FIF_FCSFAIL | \
	FIF_PLCPFAIL | FIF_CONTROL | FIF_OTHER_BSS | \
	FIF_BCN_PRBRESP_PROMISC)

	struct ath5k_hw *ah = hw->priv;
	u32 mfilt[2], rfilt;
	struct ath5k_vif_iter_data iter_data; 

	mutex_lock(&ah->lock);

	mfilt[0] = multicast;
	mfilt[1] = multicast >> 32;

	
	changed_flags &= SUPPORTED_FIF_FLAGS;
	*new_flags &= SUPPORTED_FIF_FLAGS;

	rfilt = (ath5k_hw_get_rx_filter(ah) & (AR5K_RX_FILTER_PHYERR)) |
		(AR5K_RX_FILTER_UCAST | AR5K_RX_FILTER_BCAST |
		AR5K_RX_FILTER_MCAST);

	if (changed_flags & (FIF_PROMISC_IN_BSS | FIF_OTHER_BSS)) {
		if (*new_flags & FIF_PROMISC_IN_BSS)
			__set_bit(ATH_STAT_PROMISC, ah->status);
		else
			__clear_bit(ATH_STAT_PROMISC, ah->status);
	}

	if (test_bit(ATH_STAT_PROMISC, ah->status))
		rfilt |= AR5K_RX_FILTER_PROM;

	
	if (*new_flags & FIF_ALLMULTI) {
		mfilt[0] =  ~0;
		mfilt[1] =  ~0;
	}

	
	if (*new_flags & (FIF_FCSFAIL | FIF_PLCPFAIL))
		rfilt |= AR5K_RX_FILTER_PHYERR;

	if ((*new_flags & FIF_BCN_PRBRESP_PROMISC) || (ah->nvifs > 1))
		rfilt |= AR5K_RX_FILTER_BEACON;

	if (*new_flags & FIF_CONTROL)
		rfilt |= AR5K_RX_FILTER_CONTROL;

	

	

	switch (ah->opmode) {
	case NL80211_IFTYPE_MESH_POINT:
		rfilt |= AR5K_RX_FILTER_CONTROL |
			 AR5K_RX_FILTER_BEACON |
			 AR5K_RX_FILTER_PROBEREQ |
			 AR5K_RX_FILTER_PROM;
		break;
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_ADHOC:
		rfilt |= AR5K_RX_FILTER_PROBEREQ |
			 AR5K_RX_FILTER_BEACON;
		break;
	case NL80211_IFTYPE_STATION:
		if (ah->assoc)
			rfilt |= AR5K_RX_FILTER_BEACON;
	default:
		break;
	}

	iter_data.hw_macaddr = NULL;
	iter_data.n_stas = 0;
	iter_data.need_set_hw_addr = false;
	ieee80211_iterate_active_interfaces_atomic(ah->hw, ath5k_vif_iter,
						   &iter_data);

	
	if (iter_data.n_stas > 1) {
		rfilt |= AR5K_RX_FILTER_PROM;
	}

	
	ath5k_hw_set_rx_filter(ah, rfilt);

	
	ath5k_hw_set_mcast_filter(ah, mfilt[0], mfilt[1]);
	ah->filter_flags = rfilt;

	mutex_unlock(&ah->lock);
}


static int
ath5k_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
	      struct ieee80211_vif *vif, struct ieee80211_sta *sta,
	      struct ieee80211_key_conf *key)
{
	struct ath5k_hw *ah = hw->priv;
	struct ath_common *common = ath5k_hw_common(ah);
	int ret = 0;

	if (ath5k_modparam_nohwcrypt)
		return -EOPNOTSUPP;

	if (vif->type == NL80211_IFTYPE_ADHOC &&
	    (key->cipher == WLAN_CIPHER_SUITE_TKIP ||
	     key->cipher == WLAN_CIPHER_SUITE_CCMP) &&
	    !(key->flags & IEEE80211_KEY_FLAG_PAIRWISE)) {
		
		return -EOPNOTSUPP;
	}

	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
	case WLAN_CIPHER_SUITE_TKIP:
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		if (common->crypt_caps & ATH_CRYPT_CAP_CIPHER_AESCCM)
			break;
		return -EOPNOTSUPP;
	default:
		WARN_ON(1);
		return -EINVAL;
	}

	mutex_lock(&ah->lock);

	switch (cmd) {
	case SET_KEY:
		ret = ath_key_config(common, vif, sta, key);
		if (ret >= 0) {
			key->hw_key_idx = ret;
			
			key->flags |= IEEE80211_KEY_FLAG_GENERATE_IV;
			if (key->cipher == WLAN_CIPHER_SUITE_TKIP)
				key->flags |= IEEE80211_KEY_FLAG_GENERATE_MMIC;
			if (key->cipher == WLAN_CIPHER_SUITE_CCMP)
				key->flags |= IEEE80211_KEY_FLAG_SW_MGMT;
			ret = 0;
		}
		break;
	case DISABLE_KEY:
		ath_key_delete(common, key);
		break;
	default:
		ret = -EINVAL;
	}

	mmiowb();
	mutex_unlock(&ah->lock);
	return ret;
}


static void
ath5k_sw_scan_start(struct ieee80211_hw *hw)
{
	struct ath5k_hw *ah = hw->priv;
	if (!ah->assoc)
		ath5k_hw_set_ledstate(ah, AR5K_LED_SCAN);
}


static void
ath5k_sw_scan_complete(struct ieee80211_hw *hw)
{
	struct ath5k_hw *ah = hw->priv;
	ath5k_hw_set_ledstate(ah, ah->assoc ?
		AR5K_LED_ASSOC : AR5K_LED_INIT);
}


static int
ath5k_get_stats(struct ieee80211_hw *hw,
		struct ieee80211_low_level_stats *stats)
{
	struct ath5k_hw *ah = hw->priv;

	
	ath5k_hw_update_mib_counters(ah);

	stats->dot11ACKFailureCount = ah->stats.ack_fail;
	stats->dot11RTSFailureCount = ah->stats.rts_fail;
	stats->dot11RTSSuccessCount = ah->stats.rts_ok;
	stats->dot11FCSErrorCount = ah->stats.fcs_error;

	return 0;
}


static int
ath5k_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u16 queue,
	      const struct ieee80211_tx_queue_params *params)
{
	struct ath5k_hw *ah = hw->priv;
	struct ath5k_txq_info qi;
	int ret = 0;

	if (queue >= ah->ah_capabilities.cap_queues.q_tx_num)
		return 0;

	mutex_lock(&ah->lock);

	ath5k_hw_get_tx_queueprops(ah, queue, &qi);

	qi.tqi_aifs = params->aifs;
	qi.tqi_cw_min = params->cw_min;
	qi.tqi_cw_max = params->cw_max;
	qi.tqi_burst_time = params->txop;

	ATH5K_DBG(ah, ATH5K_DEBUG_ANY,
		  "Configure tx [queue %d],  "
		  "aifs: %d, cw_min: %d, cw_max: %d, txop: %d\n",
		  queue, params->aifs, params->cw_min,
		  params->cw_max, params->txop);

	if (ath5k_hw_set_tx_queueprops(ah, queue, &qi)) {
		ATH5K_ERR(ah,
			  "Unable to update hardware queue %u!\n", queue);
		ret = -EIO;
	} else
		ath5k_hw_reset_tx_queue(ah, queue);

	mutex_unlock(&ah->lock);

	return ret;
}


static u64
ath5k_get_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct ath5k_hw *ah = hw->priv;

	return ath5k_hw_get_tsf64(ah);
}


static void
ath5k_set_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u64 tsf)
{
	struct ath5k_hw *ah = hw->priv;

	ath5k_hw_set_tsf64(ah, tsf);
}


static void
ath5k_reset_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct ath5k_hw *ah = hw->priv;

	if (ah->opmode == NL80211_IFTYPE_ADHOC)
		ath5k_beacon_update_timers(ah, 0);
	else
		ath5k_hw_reset_tsf(ah);
}


static int
ath5k_get_survey(struct ieee80211_hw *hw, int idx, struct survey_info *survey)
{
	struct ath5k_hw *ah = hw->priv;
	struct ieee80211_conf *conf = &hw->conf;
	struct ath_common *common = ath5k_hw_common(ah);
	struct ath_cycle_counters *cc = &common->cc_survey;
	unsigned int div = common->clockrate * 1000;

	if (idx != 0)
		return -ENOENT;

	spin_lock_bh(&common->cc_lock);
	ath_hw_cycle_counters_update(common);
	if (cc->cycles > 0) {
		ah->survey.channel_time += cc->cycles / div;
		ah->survey.channel_time_busy += cc->rx_busy / div;
		ah->survey.channel_time_rx += cc->rx_frame / div;
		ah->survey.channel_time_tx += cc->tx_frame / div;
	}
	memset(cc, 0, sizeof(*cc));
	spin_unlock_bh(&common->cc_lock);

	memcpy(survey, &ah->survey, sizeof(*survey));

	survey->channel = conf->channel;
	survey->noise = ah->ah_noise_floor;
	survey->filled = SURVEY_INFO_NOISE_DBM |
			SURVEY_INFO_CHANNEL_TIME |
			SURVEY_INFO_CHANNEL_TIME_BUSY |
			SURVEY_INFO_CHANNEL_TIME_RX |
			SURVEY_INFO_CHANNEL_TIME_TX;

	return 0;
}


static void
ath5k_set_coverage_class(struct ieee80211_hw *hw, u8 coverage_class)
{
	struct ath5k_hw *ah = hw->priv;

	mutex_lock(&ah->lock);
	ath5k_hw_set_coverage_class(ah, coverage_class);
	mutex_unlock(&ah->lock);
}


static int
ath5k_set_antenna(struct ieee80211_hw *hw, u32 tx_ant, u32 rx_ant)
{
	struct ath5k_hw *ah = hw->priv;

	if (tx_ant == 1 && rx_ant == 1)
		ath5k_hw_set_antenna_mode(ah, AR5K_ANTMODE_FIXED_A);
	else if (tx_ant == 2 && rx_ant == 2)
		ath5k_hw_set_antenna_mode(ah, AR5K_ANTMODE_FIXED_B);
	else if ((tx_ant & 3) == 3 && (rx_ant & 3) == 3)
		ath5k_hw_set_antenna_mode(ah, AR5K_ANTMODE_DEFAULT);
	else
		return -EINVAL;
	return 0;
}


static int
ath5k_get_antenna(struct ieee80211_hw *hw, u32 *tx_ant, u32 *rx_ant)
{
	struct ath5k_hw *ah = hw->priv;

	switch (ah->ah_ant_mode) {
	case AR5K_ANTMODE_FIXED_A:
		*tx_ant = 1; *rx_ant = 1; break;
	case AR5K_ANTMODE_FIXED_B:
		*tx_ant = 2; *rx_ant = 2; break;
	case AR5K_ANTMODE_DEFAULT:
		*tx_ant = 3; *rx_ant = 3; break;
	}
	return 0;
}


static void ath5k_get_ringparam(struct ieee80211_hw *hw,
				u32 *tx, u32 *tx_max, u32 *rx, u32 *rx_max)
{
	struct ath5k_hw *ah = hw->priv;

	*tx = ah->txqs[AR5K_TX_QUEUE_ID_DATA_MIN].txq_max;

	*tx_max = ATH5K_TXQ_LEN_MAX;
	*rx = *rx_max = ATH_RXBUF;
}


static int ath5k_set_ringparam(struct ieee80211_hw *hw, u32 tx, u32 rx)
{
	struct ath5k_hw *ah = hw->priv;
	u16 qnum;

	
	if (rx != ATH_RXBUF)
		return -EINVAL;

	
	if (!tx || tx > ATH5K_TXQ_LEN_MAX)
		return -EINVAL;

	for (qnum = 0; qnum < ARRAY_SIZE(ah->txqs); qnum++) {
		if (!ah->txqs[qnum].setup)
			continue;
		if (ah->txqs[qnum].qnum < AR5K_TX_QUEUE_ID_DATA_MIN ||
		    ah->txqs[qnum].qnum > AR5K_TX_QUEUE_ID_DATA_MAX)
			continue;

		ah->txqs[qnum].txq_max = tx;
		if (ah->txqs[qnum].txq_len >= ah->txqs[qnum].txq_max)
			ieee80211_stop_queue(hw, ah->txqs[qnum].qnum);
	}

	return 0;
}


const struct ieee80211_ops ath5k_hw_ops = {
	.tx			= ath5k_tx,
	.start			= ath5k_start,
	.stop			= ath5k_stop,
	.add_interface		= ath5k_add_interface,
	
	.remove_interface	= ath5k_remove_interface,
	.config			= ath5k_config,
	.bss_info_changed	= ath5k_bss_info_changed,
	.prepare_multicast	= ath5k_prepare_multicast,
	.configure_filter	= ath5k_configure_filter,
	
	.set_key		= ath5k_set_key,
	
	
	.sw_scan_start		= ath5k_sw_scan_start,
	.sw_scan_complete	= ath5k_sw_scan_complete,
	.get_stats		= ath5k_get_stats,
	
	
	
	
	
	
	.conf_tx		= ath5k_conf_tx,
	.get_tsf		= ath5k_get_tsf,
	.set_tsf		= ath5k_set_tsf,
	.reset_tsf		= ath5k_reset_tsf,
	
	
	.get_survey		= ath5k_get_survey,
	.set_coverage_class	= ath5k_set_coverage_class,
	
	
	
	
	.set_antenna		= ath5k_set_antenna,
	.get_antenna		= ath5k_get_antenna,
	.set_ringparam		= ath5k_set_ringparam,
	.get_ringparam		= ath5k_get_ringparam,
};
