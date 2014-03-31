/******************************************************************************
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2012 Intel Corporation. All rights reserved.
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
 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <net/mac80211.h>

#include "iwl-eeprom.h"
#include "iwl-debug.h"
#include "iwl-core.h"
#include "iwl-io.h"
#include "iwl-power.h"
#include "iwl-shared.h"
#include "iwl-agn.h"
#include "iwl-trans.h"

const u8 iwl_bcast_addr[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define MAX_BIT_RATE_40_MHZ 150 
#define MAX_BIT_RATE_20_MHZ 72 
static void iwl_init_ht_hw_capab(const struct iwl_priv *priv,
			      struct ieee80211_sta_ht_cap *ht_info,
			      enum ieee80211_band band)
{
	u16 max_bit_rate = 0;
	u8 rx_chains_num = hw_params(priv).rx_chains_num;
	u8 tx_chains_num = hw_params(priv).tx_chains_num;

	ht_info->cap = 0;
	memset(&ht_info->mcs, 0, sizeof(ht_info->mcs));

	ht_info->ht_supported = true;

	if (cfg(priv)->ht_params &&
	    cfg(priv)->ht_params->ht_greenfield_support)
		ht_info->cap |= IEEE80211_HT_CAP_GRN_FLD;
	ht_info->cap |= IEEE80211_HT_CAP_SGI_20;
	max_bit_rate = MAX_BIT_RATE_20_MHZ;
	if (hw_params(priv).ht40_channel & BIT(band)) {
		ht_info->cap |= IEEE80211_HT_CAP_SUP_WIDTH_20_40;
		ht_info->cap |= IEEE80211_HT_CAP_SGI_40;
		ht_info->mcs.rx_mask[4] = 0x01;
		max_bit_rate = MAX_BIT_RATE_40_MHZ;
	}

	if (iwlagn_mod_params.amsdu_size_8K)
		ht_info->cap |= IEEE80211_HT_CAP_MAX_AMSDU;

	ht_info->ampdu_factor = CFG_HT_RX_AMPDU_FACTOR_DEF;
	ht_info->ampdu_density = CFG_HT_MPDU_DENSITY_DEF;

	ht_info->mcs.rx_mask[0] = 0xFF;
	if (rx_chains_num >= 2)
		ht_info->mcs.rx_mask[1] = 0xFF;
	if (rx_chains_num >= 3)
		ht_info->mcs.rx_mask[2] = 0xFF;

	
	max_bit_rate *= rx_chains_num;
	WARN_ON(max_bit_rate & ~IEEE80211_HT_MCS_RX_HIGHEST_MASK);
	ht_info->mcs.rx_highest = cpu_to_le16(max_bit_rate);

	
	ht_info->mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
	if (tx_chains_num != rx_chains_num) {
		ht_info->mcs.tx_params |= IEEE80211_HT_MCS_TX_RX_DIFF;
		ht_info->mcs.tx_params |= ((tx_chains_num - 1) <<
				IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT);
	}
}

int iwl_init_geos(struct iwl_priv *priv)
{
	struct iwl_channel_info *ch;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *channels;
	struct ieee80211_channel *geo_ch;
	struct ieee80211_rate *rates;
	int i = 0;
	s8 max_tx_power = IWLAGN_TX_POWER_TARGET_POWER_MIN;

	if (priv->bands[IEEE80211_BAND_2GHZ].n_bitrates ||
	    priv->bands[IEEE80211_BAND_5GHZ].n_bitrates) {
		IWL_DEBUG_INFO(priv, "Geography modes already initialized.\n");
		set_bit(STATUS_GEO_CONFIGURED, &priv->status);
		return 0;
	}

	channels = kcalloc(priv->channel_count,
			   sizeof(struct ieee80211_channel), GFP_KERNEL);
	if (!channels)
		return -ENOMEM;

	rates = kcalloc(IWL_RATE_COUNT_LEGACY, sizeof(struct ieee80211_rate),
			GFP_KERNEL);
	if (!rates) {
		kfree(channels);
		return -ENOMEM;
	}

	
	sband = &priv->bands[IEEE80211_BAND_5GHZ];
	sband->channels = &channels[ARRAY_SIZE(iwl_eeprom_band_1)];
	
	sband->bitrates = &rates[IWL_FIRST_OFDM_RATE];
	sband->n_bitrates = IWL_RATE_COUNT_LEGACY - IWL_FIRST_OFDM_RATE;

	if (hw_params(priv).sku & EEPROM_SKU_CAP_11N_ENABLE)
		iwl_init_ht_hw_capab(priv, &sband->ht_cap,
					 IEEE80211_BAND_5GHZ);

	sband = &priv->bands[IEEE80211_BAND_2GHZ];
	sband->channels = channels;
	
	sband->bitrates = rates;
	sband->n_bitrates = IWL_RATE_COUNT_LEGACY;

	if (hw_params(priv).sku & EEPROM_SKU_CAP_11N_ENABLE)
		iwl_init_ht_hw_capab(priv, &sband->ht_cap,
					 IEEE80211_BAND_2GHZ);

	priv->ieee_channels = channels;
	priv->ieee_rates = rates;

	for (i = 0;  i < priv->channel_count; i++) {
		ch = &priv->channel_info[i];

		
		if (!is_channel_valid(ch))
			continue;

		sband =  &priv->bands[ch->band];

		geo_ch = &sband->channels[sband->n_channels++];

		geo_ch->center_freq =
			ieee80211_channel_to_frequency(ch->channel, ch->band);
		geo_ch->max_power = ch->max_power_avg;
		geo_ch->max_antenna_gain = 0xff;
		geo_ch->hw_value = ch->channel;

		if (is_channel_valid(ch)) {
			if (!(ch->flags & EEPROM_CHANNEL_IBSS))
				geo_ch->flags |= IEEE80211_CHAN_NO_IBSS;

			if (!(ch->flags & EEPROM_CHANNEL_ACTIVE))
				geo_ch->flags |= IEEE80211_CHAN_PASSIVE_SCAN;

			if (ch->flags & EEPROM_CHANNEL_RADAR)
				geo_ch->flags |= IEEE80211_CHAN_RADAR;

			geo_ch->flags |= ch->ht40_extension_channel;

			if (ch->max_power_avg > max_tx_power)
				max_tx_power = ch->max_power_avg;
		} else {
			geo_ch->flags |= IEEE80211_CHAN_DISABLED;
		}

		IWL_DEBUG_INFO(priv, "Channel %d Freq=%d[%sGHz] %s flag=0x%X\n",
				ch->channel, geo_ch->center_freq,
				is_channel_a_band(ch) ?  "5.2" : "2.4",
				geo_ch->flags & IEEE80211_CHAN_DISABLED ?
				"restricted" : "valid",
				 geo_ch->flags);
	}

	priv->tx_power_device_lmt = max_tx_power;
	priv->tx_power_user_lmt = max_tx_power;
	priv->tx_power_next = max_tx_power;

	if ((priv->bands[IEEE80211_BAND_5GHZ].n_channels == 0) &&
	     hw_params(priv).sku & EEPROM_SKU_CAP_BAND_52GHZ) {
		IWL_INFO(priv, "Incorrectly detected BG card as ABG. "
			"Please send your %s to maintainer.\n",
			trans(priv)->hw_id_str);
		hw_params(priv).sku &= ~EEPROM_SKU_CAP_BAND_52GHZ;
	}

	IWL_INFO(priv, "Tunable channels: %d 802.11bg, %d 802.11a channels\n",
		   priv->bands[IEEE80211_BAND_2GHZ].n_channels,
		   priv->bands[IEEE80211_BAND_5GHZ].n_channels);

	set_bit(STATUS_GEO_CONFIGURED, &priv->status);

	return 0;
}

void iwl_free_geos(struct iwl_priv *priv)
{
	kfree(priv->ieee_channels);
	kfree(priv->ieee_rates);
	clear_bit(STATUS_GEO_CONFIGURED, &priv->status);
}

static bool iwl_is_channel_extension(struct iwl_priv *priv,
				     enum ieee80211_band band,
				     u16 channel, u8 extension_chan_offset)
{
	const struct iwl_channel_info *ch_info;

	ch_info = iwl_get_channel_info(priv, band, channel);
	if (!is_channel_valid(ch_info))
		return false;

	if (extension_chan_offset == IEEE80211_HT_PARAM_CHA_SEC_ABOVE)
		return !(ch_info->ht40_extension_channel &
					IEEE80211_CHAN_NO_HT40PLUS);
	else if (extension_chan_offset == IEEE80211_HT_PARAM_CHA_SEC_BELOW)
		return !(ch_info->ht40_extension_channel &
					IEEE80211_CHAN_NO_HT40MINUS);

	return false;
}

bool iwl_is_ht40_tx_allowed(struct iwl_priv *priv,
			    struct iwl_rxon_context *ctx,
			    struct ieee80211_sta_ht_cap *ht_cap)
{
	if (!ctx->ht.enabled || !ctx->ht.is_40mhz)
		return false;

	if (ht_cap && !ht_cap->ht_supported)
		return false;

#ifdef CONFIG_IWLWIFI_DEBUGFS
	if (priv->disable_ht40)
		return false;
#endif

	return iwl_is_channel_extension(priv, priv->band,
			le16_to_cpu(ctx->staging.channel),
			ctx->ht.extension_chan_offset);
}

static u16 iwl_adjust_beacon_interval(u16 beacon_val, u16 max_beacon_val)
{
	u16 new_val;
	u16 beacon_factor;

	if (!beacon_val)
		return DEFAULT_BEACON_INTERVAL;


	beacon_factor = (beacon_val + max_beacon_val) / max_beacon_val;
	new_val = beacon_val / beacon_factor;

	if (!new_val)
		new_val = max_beacon_val;

	return new_val;
}

int iwl_send_rxon_timing(struct iwl_priv *priv, struct iwl_rxon_context *ctx)
{
	u64 tsf;
	s32 interval_tm, rem;
	struct ieee80211_conf *conf = NULL;
	u16 beacon_int;
	struct ieee80211_vif *vif = ctx->vif;

	conf = &priv->hw->conf;

	lockdep_assert_held(&priv->mutex);

	memset(&ctx->timing, 0, sizeof(struct iwl_rxon_time_cmd));

	ctx->timing.timestamp = cpu_to_le64(priv->timestamp);
	ctx->timing.listen_interval = cpu_to_le16(conf->listen_interval);

	beacon_int = vif ? vif->bss_conf.beacon_int : 0;

	ctx->timing.atim_window = 0;

	if (ctx->ctxid == IWL_RXON_CTX_PAN &&
	    (!ctx->vif || ctx->vif->type != NL80211_IFTYPE_STATION) &&
	    iwl_is_associated(priv, IWL_RXON_CTX_BSS) &&
	    priv->contexts[IWL_RXON_CTX_BSS].vif &&
	    priv->contexts[IWL_RXON_CTX_BSS].vif->bss_conf.beacon_int) {
		ctx->timing.beacon_interval =
			priv->contexts[IWL_RXON_CTX_BSS].timing.beacon_interval;
		beacon_int = le16_to_cpu(ctx->timing.beacon_interval);
	} else if (ctx->ctxid == IWL_RXON_CTX_BSS &&
		   iwl_is_associated(priv, IWL_RXON_CTX_PAN) &&
		   priv->contexts[IWL_RXON_CTX_PAN].vif &&
		   priv->contexts[IWL_RXON_CTX_PAN].vif->bss_conf.beacon_int &&
		   (!iwl_is_associated_ctx(ctx) || !ctx->vif ||
		    !ctx->vif->bss_conf.beacon_int)) {
		ctx->timing.beacon_interval =
			priv->contexts[IWL_RXON_CTX_PAN].timing.beacon_interval;
		beacon_int = le16_to_cpu(ctx->timing.beacon_interval);
	} else {
		beacon_int = iwl_adjust_beacon_interval(beacon_int,
			IWL_MAX_UCODE_BEACON_INTERVAL * TIME_UNIT);
		ctx->timing.beacon_interval = cpu_to_le16(beacon_int);
	}

	ctx->beacon_int = beacon_int;

	tsf = priv->timestamp; 
	interval_tm = beacon_int * TIME_UNIT;
	rem = do_div(tsf, interval_tm);
	ctx->timing.beacon_init_val = cpu_to_le32(interval_tm - rem);

	ctx->timing.dtim_period = vif ? (vif->bss_conf.dtim_period ?: 1) : 1;

	IWL_DEBUG_ASSOC(priv,
			"beacon interval %d beacon timer %d beacon tim %d\n",
			le16_to_cpu(ctx->timing.beacon_interval),
			le32_to_cpu(ctx->timing.beacon_init_val),
			le16_to_cpu(ctx->timing.atim_window));

	return iwl_dvm_send_cmd_pdu(priv, ctx->rxon_timing_cmd,
				CMD_SYNC, sizeof(ctx->timing), &ctx->timing);
}

void iwl_set_rxon_hwcrypto(struct iwl_priv *priv, struct iwl_rxon_context *ctx,
			   int hw_decrypt)
{
	struct iwl_rxon_cmd *rxon = &ctx->staging;

	if (hw_decrypt)
		rxon->filter_flags &= ~RXON_FILTER_DIS_DECRYPT_MSK;
	else
		rxon->filter_flags |= RXON_FILTER_DIS_DECRYPT_MSK;

}

int iwl_check_rxon_cmd(struct iwl_priv *priv, struct iwl_rxon_context *ctx)
{
	struct iwl_rxon_cmd *rxon = &ctx->staging;
	u32 errors = 0;

	if (rxon->flags & RXON_FLG_BAND_24G_MSK) {
		if (rxon->flags & RXON_FLG_TGJ_NARROW_BAND_MSK) {
			IWL_WARN(priv, "check 2.4G: wrong narrow\n");
			errors |= BIT(0);
		}
		if (rxon->flags & RXON_FLG_RADAR_DETECT_MSK) {
			IWL_WARN(priv, "check 2.4G: wrong radar\n");
			errors |= BIT(1);
		}
	} else {
		if (!(rxon->flags & RXON_FLG_SHORT_SLOT_MSK)) {
			IWL_WARN(priv, "check 5.2G: not short slot!\n");
			errors |= BIT(2);
		}
		if (rxon->flags & RXON_FLG_CCK_MSK) {
			IWL_WARN(priv, "check 5.2G: CCK!\n");
			errors |= BIT(3);
		}
	}
	if ((rxon->node_addr[0] | rxon->bssid_addr[0]) & 0x1) {
		IWL_WARN(priv, "mac/bssid mcast!\n");
		errors |= BIT(4);
	}

	
	if ((rxon->ofdm_basic_rates & IWL_RATE_6M_MASK) == 0 &&
	    (rxon->cck_basic_rates & IWL_RATE_1M_MASK) == 0) {
		IWL_WARN(priv, "neither 1 nor 6 are basic\n");
		errors |= BIT(5);
	}

	if (le16_to_cpu(rxon->assoc_id) > 2007) {
		IWL_WARN(priv, "aid > 2007\n");
		errors |= BIT(6);
	}

	if ((rxon->flags & (RXON_FLG_CCK_MSK | RXON_FLG_SHORT_SLOT_MSK))
			== (RXON_FLG_CCK_MSK | RXON_FLG_SHORT_SLOT_MSK)) {
		IWL_WARN(priv, "CCK and short slot\n");
		errors |= BIT(7);
	}

	if ((rxon->flags & (RXON_FLG_CCK_MSK | RXON_FLG_AUTO_DETECT_MSK))
			== (RXON_FLG_CCK_MSK | RXON_FLG_AUTO_DETECT_MSK)) {
		IWL_WARN(priv, "CCK and auto detect");
		errors |= BIT(8);
	}

	if ((rxon->flags & (RXON_FLG_AUTO_DETECT_MSK |
			    RXON_FLG_TGG_PROTECT_MSK)) ==
			    RXON_FLG_TGG_PROTECT_MSK) {
		IWL_WARN(priv, "TGg but no auto-detect\n");
		errors |= BIT(9);
	}

	if (rxon->channel == 0) {
		IWL_WARN(priv, "zero channel is invalid\n");
		errors |= BIT(10);
	}

	WARN(errors, "Invalid RXON (%#x), channel %d",
	     errors, le16_to_cpu(rxon->channel));

	return errors ? -EINVAL : 0;
}

int iwl_full_rxon_required(struct iwl_priv *priv,
			   struct iwl_rxon_context *ctx)
{
	const struct iwl_rxon_cmd *staging = &ctx->staging;
	const struct iwl_rxon_cmd *active = &ctx->active;

#define CHK(cond)							\
	if ((cond)) {							\
		IWL_DEBUG_INFO(priv, "need full RXON - " #cond "\n");	\
		return 1;						\
	}

#define CHK_NEQ(c1, c2)						\
	if ((c1) != (c2)) {					\
		IWL_DEBUG_INFO(priv, "need full RXON - "	\
			       #c1 " != " #c2 " - %d != %d\n",	\
			       (c1), (c2));			\
		return 1;					\
	}

	
	CHK(!iwl_is_associated_ctx(ctx));
	CHK(compare_ether_addr(staging->bssid_addr, active->bssid_addr));
	CHK(compare_ether_addr(staging->node_addr, active->node_addr));
	CHK(compare_ether_addr(staging->wlap_bssid_addr,
				active->wlap_bssid_addr));
	CHK_NEQ(staging->dev_type, active->dev_type);
	CHK_NEQ(staging->channel, active->channel);
	CHK_NEQ(staging->air_propagation, active->air_propagation);
	CHK_NEQ(staging->ofdm_ht_single_stream_basic_rates,
		active->ofdm_ht_single_stream_basic_rates);
	CHK_NEQ(staging->ofdm_ht_dual_stream_basic_rates,
		active->ofdm_ht_dual_stream_basic_rates);
	CHK_NEQ(staging->ofdm_ht_triple_stream_basic_rates,
		active->ofdm_ht_triple_stream_basic_rates);
	CHK_NEQ(staging->assoc_id, active->assoc_id);


	
	CHK_NEQ(staging->flags & RXON_FLG_BAND_24G_MSK,
		active->flags & RXON_FLG_BAND_24G_MSK);

	
	CHK_NEQ(staging->filter_flags & RXON_FILTER_ASSOC_MSK,
		active->filter_flags & RXON_FILTER_ASSOC_MSK);

#undef CHK
#undef CHK_NEQ

	return 0;
}

static void _iwl_set_rxon_ht(struct iwl_priv *priv,
			     struct iwl_ht_config *ht_conf,
			     struct iwl_rxon_context *ctx)
{
	struct iwl_rxon_cmd *rxon = &ctx->staging;

	if (!ctx->ht.enabled) {
		rxon->flags &= ~(RXON_FLG_CHANNEL_MODE_MSK |
			RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK |
			RXON_FLG_HT40_PROT_MSK |
			RXON_FLG_HT_PROT_MSK);
		return;
	}

	rxon->flags |= cpu_to_le32(ctx->ht.protection << RXON_FLG_HT_OPERATING_MODE_POS);

	
	rxon->flags &= ~(RXON_FLG_CHANNEL_MODE_MSK |
			 RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK);
	if (iwl_is_ht40_tx_allowed(priv, ctx, NULL)) {
		
		if (ctx->ht.protection == IEEE80211_HT_OP_MODE_PROTECTION_20MHZ) {
			rxon->flags |= RXON_FLG_CHANNEL_MODE_PURE_40;
			
			switch (ctx->ht.extension_chan_offset) {
			case IEEE80211_HT_PARAM_CHA_SEC_ABOVE:
				rxon->flags &= ~RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK;
				break;
			case IEEE80211_HT_PARAM_CHA_SEC_BELOW:
				rxon->flags |= RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK;
				break;
			}
		} else {
			
			switch (ctx->ht.extension_chan_offset) {
			case IEEE80211_HT_PARAM_CHA_SEC_ABOVE:
				rxon->flags &= ~(RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK);
				rxon->flags |= RXON_FLG_CHANNEL_MODE_MIXED;
				break;
			case IEEE80211_HT_PARAM_CHA_SEC_BELOW:
				rxon->flags |= RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK;
				rxon->flags |= RXON_FLG_CHANNEL_MODE_MIXED;
				break;
			case IEEE80211_HT_PARAM_CHA_SEC_NONE:
			default:
				
				IWL_ERR(priv, "invalid extension channel offset\n");
				break;
			}
		}
	} else {
		rxon->flags |= RXON_FLG_CHANNEL_MODE_LEGACY;
	}

	iwlagn_set_rxon_chain(priv, ctx);

	IWL_DEBUG_ASSOC(priv, "rxon flags 0x%X operation mode :0x%X "
			"extension channel offset 0x%x\n",
			le32_to_cpu(rxon->flags), ctx->ht.protection,
			ctx->ht.extension_chan_offset);
}

void iwl_set_rxon_ht(struct iwl_priv *priv, struct iwl_ht_config *ht_conf)
{
	struct iwl_rxon_context *ctx;

	for_each_context(priv, ctx)
		_iwl_set_rxon_ht(priv, ht_conf, ctx);
}

u8 iwl_get_single_channel_number(struct iwl_priv *priv,
				 enum ieee80211_band band)
{
	const struct iwl_channel_info *ch_info;
	int i;
	u8 channel = 0;
	u8 min, max;
	struct iwl_rxon_context *ctx;

	if (band == IEEE80211_BAND_5GHZ) {
		min = 14;
		max = priv->channel_count;
	} else {
		min = 0;
		max = 14;
	}

	for (i = min; i < max; i++) {
		bool busy = false;

		for_each_context(priv, ctx) {
			busy = priv->channel_info[i].channel ==
				le16_to_cpu(ctx->staging.channel);
			if (busy)
				break;
		}

		if (busy)
			continue;

		channel = priv->channel_info[i].channel;
		ch_info = iwl_get_channel_info(priv, band, channel);
		if (is_channel_valid(ch_info))
			break;
	}

	return channel;
}

void iwl_set_rxon_channel(struct iwl_priv *priv, struct ieee80211_channel *ch,
			 struct iwl_rxon_context *ctx)
{
	enum ieee80211_band band = ch->band;
	u16 channel = ch->hw_value;

	if ((le16_to_cpu(ctx->staging.channel) == channel) &&
	    (priv->band == band))
		return;

	ctx->staging.channel = cpu_to_le16(channel);
	if (band == IEEE80211_BAND_5GHZ)
		ctx->staging.flags &= ~RXON_FLG_BAND_24G_MSK;
	else
		ctx->staging.flags |= RXON_FLG_BAND_24G_MSK;

	priv->band = band;

	IWL_DEBUG_INFO(priv, "Staging channel set to %d [%d]\n", channel, band);

}

void iwl_set_flags_for_band(struct iwl_priv *priv,
			    struct iwl_rxon_context *ctx,
			    enum ieee80211_band band,
			    struct ieee80211_vif *vif)
{
	if (band == IEEE80211_BAND_5GHZ) {
		ctx->staging.flags &=
		    ~(RXON_FLG_BAND_24G_MSK | RXON_FLG_AUTO_DETECT_MSK
		      | RXON_FLG_CCK_MSK);
		ctx->staging.flags |= RXON_FLG_SHORT_SLOT_MSK;
	} else {
		
		if (vif && vif->bss_conf.use_short_slot)
			ctx->staging.flags |= RXON_FLG_SHORT_SLOT_MSK;
		else
			ctx->staging.flags &= ~RXON_FLG_SHORT_SLOT_MSK;

		ctx->staging.flags |= RXON_FLG_BAND_24G_MSK;
		ctx->staging.flags |= RXON_FLG_AUTO_DETECT_MSK;
		ctx->staging.flags &= ~RXON_FLG_CCK_MSK;
	}
}

void iwl_connection_init_rx_config(struct iwl_priv *priv,
				   struct iwl_rxon_context *ctx)
{
	const struct iwl_channel_info *ch_info;

	memset(&ctx->staging, 0, sizeof(ctx->staging));

	if (!ctx->vif) {
		ctx->staging.dev_type = ctx->unused_devtype;
	} else switch (ctx->vif->type) {
	case NL80211_IFTYPE_AP:
		ctx->staging.dev_type = ctx->ap_devtype;
		break;

	case NL80211_IFTYPE_STATION:
		ctx->staging.dev_type = ctx->station_devtype;
		ctx->staging.filter_flags = RXON_FILTER_ACCEPT_GRP_MSK;
		break;

	case NL80211_IFTYPE_ADHOC:
		ctx->staging.dev_type = ctx->ibss_devtype;
		ctx->staging.flags = RXON_FLG_SHORT_PREAMBLE_MSK;
		ctx->staging.filter_flags = RXON_FILTER_BCON_AWARE_MSK |
						  RXON_FILTER_ACCEPT_GRP_MSK;
		break;

	default:
		IWL_ERR(priv, "Unsupported interface type %d\n",
			ctx->vif->type);
		break;
	}

#if 0
	if (!hw_to_local(priv->hw)->short_preamble)
		ctx->staging.flags &= ~RXON_FLG_SHORT_PREAMBLE_MSK;
	else
		ctx->staging.flags |= RXON_FLG_SHORT_PREAMBLE_MSK;
#endif

	ch_info = iwl_get_channel_info(priv, priv->band,
				       le16_to_cpu(ctx->active.channel));

	if (!ch_info)
		ch_info = &priv->channel_info[0];

	ctx->staging.channel = cpu_to_le16(ch_info->channel);
	priv->band = ch_info->band;

	iwl_set_flags_for_band(priv, ctx, priv->band, ctx->vif);

	ctx->staging.ofdm_basic_rates =
	    (IWL_OFDM_RATES_MASK >> IWL_FIRST_OFDM_RATE) & 0xFF;
	ctx->staging.cck_basic_rates =
	    (IWL_CCK_RATES_MASK >> IWL_FIRST_CCK_RATE) & 0xF;

	
	ctx->staging.flags &= ~(RXON_FLG_CHANNEL_MODE_MIXED |
					RXON_FLG_CHANNEL_MODE_PURE_40);
	if (ctx->vif)
		memcpy(ctx->staging.node_addr, ctx->vif->addr, ETH_ALEN);

	ctx->staging.ofdm_ht_single_stream_basic_rates = 0xff;
	ctx->staging.ofdm_ht_dual_stream_basic_rates = 0xff;
	ctx->staging.ofdm_ht_triple_stream_basic_rates = 0xff;
}

void iwl_set_rate(struct iwl_priv *priv)
{
	const struct ieee80211_supported_band *hw = NULL;
	struct ieee80211_rate *rate;
	struct iwl_rxon_context *ctx;
	int i;

	hw = iwl_get_hw_mode(priv, priv->band);
	if (!hw) {
		IWL_ERR(priv, "Failed to set rate: unable to get hw mode\n");
		return;
	}

	priv->active_rate = 0;

	for (i = 0; i < hw->n_bitrates; i++) {
		rate = &(hw->bitrates[i]);
		if (rate->hw_value < IWL_RATE_COUNT_LEGACY)
			priv->active_rate |= (1 << rate->hw_value);
	}

	IWL_DEBUG_RATE(priv, "Set active_rate = %0x\n", priv->active_rate);

	for_each_context(priv, ctx) {
		ctx->staging.cck_basic_rates =
		    (IWL_CCK_BASIC_RATES_MASK >> IWL_FIRST_CCK_RATE) & 0xF;

		ctx->staging.ofdm_basic_rates =
		   (IWL_OFDM_BASIC_RATES_MASK >> IWL_FIRST_OFDM_RATE) & 0xFF;
	}
}

void iwl_chswitch_done(struct iwl_priv *priv, bool is_success)
{
	struct iwl_rxon_context *ctx = &priv->contexts[IWL_RXON_CTX_BSS];

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	if (test_and_clear_bit(STATUS_CHANNEL_SWITCH_PENDING, &priv->status))
		ieee80211_chswitch_done(ctx->vif, is_success);
}

#ifdef CONFIG_IWLWIFI_DEBUG
void iwl_print_rx_config_cmd(struct iwl_priv *priv,
			     enum iwl_rxon_context_id ctxid)
{
	struct iwl_rxon_context *ctx = &priv->contexts[ctxid];
	struct iwl_rxon_cmd *rxon = &ctx->staging;

	IWL_DEBUG_RADIO(priv, "RX CONFIG:\n");
	iwl_print_hex_dump(priv, IWL_DL_RADIO, (u8 *) rxon, sizeof(*rxon));
	IWL_DEBUG_RADIO(priv, "u16 channel: 0x%x\n", le16_to_cpu(rxon->channel));
	IWL_DEBUG_RADIO(priv, "u32 flags: 0x%08X\n", le32_to_cpu(rxon->flags));
	IWL_DEBUG_RADIO(priv, "u32 filter_flags: 0x%08x\n",
			le32_to_cpu(rxon->filter_flags));
	IWL_DEBUG_RADIO(priv, "u8 dev_type: 0x%x\n", rxon->dev_type);
	IWL_DEBUG_RADIO(priv, "u8 ofdm_basic_rates: 0x%02x\n",
			rxon->ofdm_basic_rates);
	IWL_DEBUG_RADIO(priv, "u8 cck_basic_rates: 0x%02x\n", rxon->cck_basic_rates);
	IWL_DEBUG_RADIO(priv, "u8[6] node_addr: %pM\n", rxon->node_addr);
	IWL_DEBUG_RADIO(priv, "u8[6] bssid_addr: %pM\n", rxon->bssid_addr);
	IWL_DEBUG_RADIO(priv, "u16 assoc_id: 0x%x\n", le16_to_cpu(rxon->assoc_id));
}
#endif

static void iwlagn_fw_error(struct iwl_priv *priv, bool ondemand)
{
	unsigned int reload_msec;
	unsigned long reload_jiffies;

#ifdef CONFIG_IWLWIFI_DEBUG
	if (iwl_have_debug_level(IWL_DL_FW_ERRORS))
		iwl_print_rx_config_cmd(priv, IWL_RXON_CTX_BSS);
#endif

	
	priv->ucode_loaded = false;

	
	set_bit(STATUS_FW_ERROR, &priv->shrd->status);

	
	clear_bit(STATUS_HCMD_ACTIVE, &priv->shrd->status);

	iwl_abort_notification_waits(&priv->notif_wait);

	clear_bit(STATUS_READY, &priv->status);

	wake_up(&trans(priv)->wait_command_queue);

	if (!ondemand) {
		reload_jiffies = jiffies;
		reload_msec = jiffies_to_msecs((long) reload_jiffies -
					(long) priv->reload_jiffies);
		priv->reload_jiffies = reload_jiffies;
		if (reload_msec <= IWL_MIN_RELOAD_DURATION) {
			priv->reload_count++;
			if (priv->reload_count >= IWL_MAX_CONTINUE_RELOAD_CNT) {
				IWL_ERR(priv, "BUG_ON, Stop restarting\n");
				return;
			}
		} else
			priv->reload_count = 0;
	}

	if (!test_bit(STATUS_EXIT_PENDING, &priv->status)) {
		if (iwlagn_mod_params.restart_fw) {
			IWL_DEBUG_FW_ERRORS(priv,
				  "Restarting adapter due to uCode error.\n");
			queue_work(priv->workqueue, &priv->restart);
		} else
			IWL_DEBUG_FW_ERRORS(priv,
				  "Detected FW error, but not restarting\n");
	}
}

int iwl_set_tx_power(struct iwl_priv *priv, s8 tx_power, bool force)
{
	int ret;
	s8 prev_tx_power;
	bool defer;
	struct iwl_rxon_context *ctx = &priv->contexts[IWL_RXON_CTX_BSS];

	lockdep_assert_held(&priv->mutex);

	if (priv->tx_power_user_lmt == tx_power && !force)
		return 0;

	if (tx_power < IWLAGN_TX_POWER_TARGET_POWER_MIN) {
		IWL_WARN(priv,
			 "Requested user TXPOWER %d below lower limit %d.\n",
			 tx_power,
			 IWLAGN_TX_POWER_TARGET_POWER_MIN);
		return -EINVAL;
	}

	if (tx_power > priv->tx_power_device_lmt) {
		IWL_WARN(priv,
			"Requested user TXPOWER %d above upper limit %d.\n",
			 tx_power, priv->tx_power_device_lmt);
		return -EINVAL;
	}

	if (!iwl_is_ready_rf(priv))
		return -EIO;

	priv->tx_power_next = tx_power;

	
	defer = test_bit(STATUS_SCANNING, &priv->status) ||
		memcmp(&ctx->active, &ctx->staging, sizeof(ctx->staging));
	if (defer && !force) {
		IWL_DEBUG_INFO(priv, "Deferring tx power set\n");
		return 0;
	}

	prev_tx_power = priv->tx_power_user_lmt;
	priv->tx_power_user_lmt = tx_power;

	ret = iwlagn_send_tx_power(priv);

	
	if (ret) {
		priv->tx_power_user_lmt = prev_tx_power;
		priv->tx_power_next = prev_tx_power;
	}
	return ret;
}

void iwl_send_bt_config(struct iwl_priv *priv)
{
	struct iwl_bt_cmd bt_cmd = {
		.lead_time = BT_LEAD_TIME_DEF,
		.max_kill = BT_MAX_KILL_DEF,
		.kill_ack_mask = 0,
		.kill_cts_mask = 0,
	};

	if (!iwlagn_mod_params.bt_coex_active)
		bt_cmd.flags = BT_COEX_DISABLE;
	else
		bt_cmd.flags = BT_COEX_ENABLE;

	priv->bt_enable_flag = bt_cmd.flags;
	IWL_DEBUG_INFO(priv, "BT coex %s\n",
		(bt_cmd.flags == BT_COEX_DISABLE) ? "disable" : "active");

	if (iwl_dvm_send_cmd_pdu(priv, REPLY_BT_CONFIG,
			     CMD_SYNC, sizeof(struct iwl_bt_cmd), &bt_cmd))
		IWL_ERR(priv, "failed to send BT Coex Config\n");
}

int iwl_send_statistics_request(struct iwl_priv *priv, u8 flags, bool clear)
{
	struct iwl_statistics_cmd statistics_cmd = {
		.configuration_flags =
			clear ? IWL_STATS_CONF_CLEAR_STATS : 0,
	};

	if (flags & CMD_ASYNC)
		return iwl_dvm_send_cmd_pdu(priv, REPLY_STATISTICS_CMD,
					      CMD_ASYNC,
					       sizeof(struct iwl_statistics_cmd),
					       &statistics_cmd);
	else
		return iwl_dvm_send_cmd_pdu(priv, REPLY_STATISTICS_CMD,
					CMD_SYNC,
					sizeof(struct iwl_statistics_cmd),
					&statistics_cmd);
}




#ifdef CONFIG_IWLWIFI_DEBUGFS

#define IWL_TRAFFIC_DUMP_SIZE	(IWL_TRAFFIC_ENTRY_SIZE * IWL_TRAFFIC_ENTRIES)

void iwl_reset_traffic_log(struct iwl_priv *priv)
{
	priv->tx_traffic_idx = 0;
	priv->rx_traffic_idx = 0;
	if (priv->tx_traffic)
		memset(priv->tx_traffic, 0, IWL_TRAFFIC_DUMP_SIZE);
	if (priv->rx_traffic)
		memset(priv->rx_traffic, 0, IWL_TRAFFIC_DUMP_SIZE);
}

int iwl_alloc_traffic_mem(struct iwl_priv *priv)
{
	u32 traffic_size = IWL_TRAFFIC_DUMP_SIZE;

	if (iwl_have_debug_level(IWL_DL_TX)) {
		if (!priv->tx_traffic) {
			priv->tx_traffic =
				kzalloc(traffic_size, GFP_KERNEL);
			if (!priv->tx_traffic)
				return -ENOMEM;
		}
	}
	if (iwl_have_debug_level(IWL_DL_RX)) {
		if (!priv->rx_traffic) {
			priv->rx_traffic =
				kzalloc(traffic_size, GFP_KERNEL);
			if (!priv->rx_traffic)
				return -ENOMEM;
		}
	}
	iwl_reset_traffic_log(priv);
	return 0;
}

void iwl_free_traffic_mem(struct iwl_priv *priv)
{
	kfree(priv->tx_traffic);
	priv->tx_traffic = NULL;

	kfree(priv->rx_traffic);
	priv->rx_traffic = NULL;
}

void iwl_dbg_log_tx_data_frame(struct iwl_priv *priv,
		      u16 length, struct ieee80211_hdr *header)
{
	__le16 fc;
	u16 len;

	if (likely(!iwl_have_debug_level(IWL_DL_TX)))
		return;

	if (!priv->tx_traffic)
		return;

	fc = header->frame_control;
	if (ieee80211_is_data(fc)) {
		len = (length > IWL_TRAFFIC_ENTRY_SIZE)
		       ? IWL_TRAFFIC_ENTRY_SIZE : length;
		memcpy((priv->tx_traffic +
		       (priv->tx_traffic_idx * IWL_TRAFFIC_ENTRY_SIZE)),
		       header, len);
		priv->tx_traffic_idx =
			(priv->tx_traffic_idx + 1) % IWL_TRAFFIC_ENTRIES;
	}
}

void iwl_dbg_log_rx_data_frame(struct iwl_priv *priv,
		      u16 length, struct ieee80211_hdr *header)
{
	__le16 fc;
	u16 len;

	if (likely(!iwl_have_debug_level(IWL_DL_RX)))
		return;

	if (!priv->rx_traffic)
		return;

	fc = header->frame_control;
	if (ieee80211_is_data(fc)) {
		len = (length > IWL_TRAFFIC_ENTRY_SIZE)
		       ? IWL_TRAFFIC_ENTRY_SIZE : length;
		memcpy((priv->rx_traffic +
		       (priv->rx_traffic_idx * IWL_TRAFFIC_ENTRY_SIZE)),
		       header, len);
		priv->rx_traffic_idx =
			(priv->rx_traffic_idx + 1) % IWL_TRAFFIC_ENTRIES;
	}
}

const char *get_mgmt_string(int cmd)
{
	switch (cmd) {
		IWL_CMD(MANAGEMENT_ASSOC_REQ);
		IWL_CMD(MANAGEMENT_ASSOC_RESP);
		IWL_CMD(MANAGEMENT_REASSOC_REQ);
		IWL_CMD(MANAGEMENT_REASSOC_RESP);
		IWL_CMD(MANAGEMENT_PROBE_REQ);
		IWL_CMD(MANAGEMENT_PROBE_RESP);
		IWL_CMD(MANAGEMENT_BEACON);
		IWL_CMD(MANAGEMENT_ATIM);
		IWL_CMD(MANAGEMENT_DISASSOC);
		IWL_CMD(MANAGEMENT_AUTH);
		IWL_CMD(MANAGEMENT_DEAUTH);
		IWL_CMD(MANAGEMENT_ACTION);
	default:
		return "UNKNOWN";

	}
}

const char *get_ctrl_string(int cmd)
{
	switch (cmd) {
		IWL_CMD(CONTROL_BACK_REQ);
		IWL_CMD(CONTROL_BACK);
		IWL_CMD(CONTROL_PSPOLL);
		IWL_CMD(CONTROL_RTS);
		IWL_CMD(CONTROL_CTS);
		IWL_CMD(CONTROL_ACK);
		IWL_CMD(CONTROL_CFEND);
		IWL_CMD(CONTROL_CFENDACK);
	default:
		return "UNKNOWN";

	}
}

void iwl_clear_traffic_stats(struct iwl_priv *priv)
{
	memset(&priv->tx_stats, 0, sizeof(struct traffic_stats));
	memset(&priv->rx_stats, 0, sizeof(struct traffic_stats));
}

void iwl_update_stats(struct iwl_priv *priv, bool is_tx, __le16 fc, u16 len)
{
	struct traffic_stats	*stats;

	if (is_tx)
		stats = &priv->tx_stats;
	else
		stats = &priv->rx_stats;

	if (ieee80211_is_mgmt(fc)) {
		switch (fc & cpu_to_le16(IEEE80211_FCTL_STYPE)) {
		case cpu_to_le16(IEEE80211_STYPE_ASSOC_REQ):
			stats->mgmt[MANAGEMENT_ASSOC_REQ]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_ASSOC_RESP):
			stats->mgmt[MANAGEMENT_ASSOC_RESP]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_REASSOC_REQ):
			stats->mgmt[MANAGEMENT_REASSOC_REQ]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_REASSOC_RESP):
			stats->mgmt[MANAGEMENT_REASSOC_RESP]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_PROBE_REQ):
			stats->mgmt[MANAGEMENT_PROBE_REQ]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_PROBE_RESP):
			stats->mgmt[MANAGEMENT_PROBE_RESP]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_BEACON):
			stats->mgmt[MANAGEMENT_BEACON]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_ATIM):
			stats->mgmt[MANAGEMENT_ATIM]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_DISASSOC):
			stats->mgmt[MANAGEMENT_DISASSOC]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_AUTH):
			stats->mgmt[MANAGEMENT_AUTH]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_DEAUTH):
			stats->mgmt[MANAGEMENT_DEAUTH]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_ACTION):
			stats->mgmt[MANAGEMENT_ACTION]++;
			break;
		}
	} else if (ieee80211_is_ctl(fc)) {
		switch (fc & cpu_to_le16(IEEE80211_FCTL_STYPE)) {
		case cpu_to_le16(IEEE80211_STYPE_BACK_REQ):
			stats->ctrl[CONTROL_BACK_REQ]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_BACK):
			stats->ctrl[CONTROL_BACK]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_PSPOLL):
			stats->ctrl[CONTROL_PSPOLL]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_RTS):
			stats->ctrl[CONTROL_RTS]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_CTS):
			stats->ctrl[CONTROL_CTS]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_ACK):
			stats->ctrl[CONTROL_ACK]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_CFEND):
			stats->ctrl[CONTROL_CFEND]++;
			break;
		case cpu_to_le16(IEEE80211_STYPE_CFENDACK):
			stats->ctrl[CONTROL_CFENDACK]++;
			break;
		}
	} else {
		
		stats->data_cnt++;
		stats->data_bytes += len;
	}
}
#endif

static void iwl_force_rf_reset(struct iwl_priv *priv)
{
	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	if (!iwl_is_any_associated(priv)) {
		IWL_DEBUG_SCAN(priv, "force reset rejected: not associated\n");
		return;
	}
	IWL_DEBUG_INFO(priv, "perform radio reset.\n");
	iwl_internal_short_hw_scan(priv);
}


int iwl_force_reset(struct iwl_priv *priv, int mode, bool external)
{
	struct iwl_force_reset *force_reset;

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return -EINVAL;

	if (mode >= IWL_MAX_FORCE_RESET) {
		IWL_DEBUG_INFO(priv, "invalid reset request.\n");
		return -EINVAL;
	}
	force_reset = &priv->force_reset[mode];
	force_reset->reset_request_count++;
	if (!external) {
		if (force_reset->last_force_reset_jiffies &&
		    time_after(force_reset->last_force_reset_jiffies +
		    force_reset->reset_duration, jiffies)) {
			IWL_DEBUG_INFO(priv, "force reset rejected\n");
			force_reset->reset_reject_count++;
			return -EAGAIN;
		}
	}
	force_reset->reset_success_count++;
	force_reset->last_force_reset_jiffies = jiffies;
	IWL_DEBUG_INFO(priv, "perform force reset (%d)\n", mode);
	switch (mode) {
	case IWL_RF_RESET:
		iwl_force_rf_reset(priv);
		break;
	case IWL_FW_RESET:
		if (!external && !iwlagn_mod_params.restart_fw) {
			IWL_DEBUG_INFO(priv, "Cancel firmware reload based on "
				       "module parameter setting\n");
			break;
		}
		IWL_ERR(priv, "On demand firmware reload\n");
		iwlagn_fw_error(priv, true);
		break;
	}
	return 0;
}


int iwl_cmd_echo_test(struct iwl_priv *priv)
{
	int ret;
	struct iwl_host_cmd cmd = {
		.id = REPLY_ECHO,
		.len = { 0 },
		.flags = CMD_SYNC,
	};

	ret = iwl_dvm_send_cmd(priv, &cmd);
	if (ret)
		IWL_ERR(priv, "echo testing fail: 0X%x\n", ret);
	else
		IWL_DEBUG_INFO(priv, "echo testing pass\n");
	return ret;
}

static inline int iwl_check_stuck_queue(struct iwl_priv *priv, int txq)
{
	if (iwl_trans_check_stuck_queue(trans(priv), txq)) {
		int ret;
		ret = iwl_force_reset(priv, IWL_FW_RESET, false);
		return (ret == -EAGAIN) ? 0 : 1;
	}
	return 0;
}

#define IWL_WD_TICK(timeout) ((timeout) / 4)

void iwl_bg_watchdog(unsigned long data)
{
	struct iwl_priv *priv = (struct iwl_priv *)data;
	int cnt;
	unsigned long timeout;

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	if (iwl_is_rfkill(priv))
		return;

	timeout = hw_params(priv).wd_timeout;
	if (timeout == 0)
		return;

	
	for (cnt = 0; cnt < cfg(priv)->base_params->num_of_queues; cnt++)
		if (iwl_check_stuck_queue(priv, cnt))
			return;

	mod_timer(&priv->watchdog, jiffies +
		  msecs_to_jiffies(IWL_WD_TICK(timeout)));
}

void iwl_setup_watchdog(struct iwl_priv *priv)
{
	unsigned int timeout = hw_params(priv).wd_timeout;

	if (!iwlagn_mod_params.wd_disable) {
		
		if (timeout && !cfg(priv)->base_params->wd_disable)
			mod_timer(&priv->watchdog,
				jiffies +
				msecs_to_jiffies(IWL_WD_TICK(timeout)));
		else
			del_timer(&priv->watchdog);
	} else {
		
		if (timeout && iwlagn_mod_params.wd_disable == 2)
			mod_timer(&priv->watchdog,
				jiffies +
				msecs_to_jiffies(IWL_WD_TICK(timeout)));
		else
			del_timer(&priv->watchdog);
	}
}

static inline u32 iwl_beacon_time_mask_low(struct iwl_priv *priv,
					   u16 tsf_bits)
{
	return (1 << tsf_bits) - 1;
}

static inline u32 iwl_beacon_time_mask_high(struct iwl_priv *priv,
					    u16 tsf_bits)
{
	return ((1 << (32 - tsf_bits)) - 1) << tsf_bits;
}

u32 iwl_usecs_to_beacons(struct iwl_priv *priv, u32 usec, u32 beacon_interval)
{
	u32 quot;
	u32 rem;
	u32 interval = beacon_interval * TIME_UNIT;

	if (!interval || !usec)
		return 0;

	quot = (usec / interval) &
		(iwl_beacon_time_mask_high(priv, IWLAGN_EXT_BEACON_TIME_POS) >>
		IWLAGN_EXT_BEACON_TIME_POS);
	rem = (usec % interval) & iwl_beacon_time_mask_low(priv,
				   IWLAGN_EXT_BEACON_TIME_POS);

	return (quot << IWLAGN_EXT_BEACON_TIME_POS) + rem;
}

__le32 iwl_add_beacon_time(struct iwl_priv *priv, u32 base,
			   u32 addon, u32 beacon_interval)
{
	u32 base_low = base & iwl_beacon_time_mask_low(priv,
				IWLAGN_EXT_BEACON_TIME_POS);
	u32 addon_low = addon & iwl_beacon_time_mask_low(priv,
				IWLAGN_EXT_BEACON_TIME_POS);
	u32 interval = beacon_interval * TIME_UNIT;
	u32 res = (base & iwl_beacon_time_mask_high(priv,
				IWLAGN_EXT_BEACON_TIME_POS)) +
				(addon & iwl_beacon_time_mask_high(priv,
				IWLAGN_EXT_BEACON_TIME_POS));

	if (base_low > addon_low)
		res += base_low - addon_low;
	else if (base_low < addon_low) {
		res += interval + base_low - addon_low;
		res += (1 << IWLAGN_EXT_BEACON_TIME_POS);
	} else
		res += (1 << IWLAGN_EXT_BEACON_TIME_POS);

	return cpu_to_le32(res);
}

void iwl_nic_error(struct iwl_op_mode *op_mode)
{
	struct iwl_priv *priv = IWL_OP_MODE_GET_DVM(op_mode);

	iwlagn_fw_error(priv, false);
}

void iwl_set_hw_rfkill_state(struct iwl_op_mode *op_mode, bool state)
{
	struct iwl_priv *priv = IWL_OP_MODE_GET_DVM(op_mode);

	if (state)
		set_bit(STATUS_RF_KILL_HW, &priv->status);
	else
		clear_bit(STATUS_RF_KILL_HW, &priv->status);

	wiphy_rfkill_set_hw_state(priv->hw->wiphy, state);
}

void iwl_free_skb(struct iwl_op_mode *op_mode, struct sk_buff *skb)
{
	struct ieee80211_tx_info *info;

	info = IEEE80211_SKB_CB(skb);
	kmem_cache_free(iwl_tx_cmd_pool, (info->driver_data[1]));
	dev_kfree_skb_any(skb);
}
