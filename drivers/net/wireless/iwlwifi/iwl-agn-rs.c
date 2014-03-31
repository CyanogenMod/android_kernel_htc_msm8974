/******************************************************************************
 *
 * Copyright(c) 2005 - 2012 Intel Corporation. All rights reserved.
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
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <net/mac80211.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>

#include <linux/workqueue.h>

#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-agn.h"
#include "iwl-op-mode.h"

#define RS_NAME "iwl-agn-rs"

#define NUM_TRY_BEFORE_ANT_TOGGLE 1
#define IWL_NUMBER_TRY      1
#define IWL_HT_NUMBER_TRY   3

#define IWL_RATE_MAX_WINDOW		62	
#define IWL_RATE_MIN_FAILURE_TH		6	
#define IWL_RATE_MIN_SUCCESS_TH		8	

#define IWL_MISSED_RATE_MAX		15
#define IWL_RATE_SCALE_FLUSH_INTVL   (3*HZ)

static u8 rs_ht_to_legacy[] = {
	IWL_RATE_6M_INDEX, IWL_RATE_6M_INDEX,
	IWL_RATE_6M_INDEX, IWL_RATE_6M_INDEX,
	IWL_RATE_6M_INDEX,
	IWL_RATE_6M_INDEX, IWL_RATE_9M_INDEX,
	IWL_RATE_12M_INDEX, IWL_RATE_18M_INDEX,
	IWL_RATE_24M_INDEX, IWL_RATE_36M_INDEX,
	IWL_RATE_48M_INDEX, IWL_RATE_54M_INDEX
};

static const u8 ant_toggle_lookup[] = {
	 ANT_NONE,
	 ANT_B,
	 ANT_C,
	 ANT_BC,
	 ANT_A,
	 ANT_AB,
	 ANT_AC,
	 ANT_ABC,
};

#define IWL_DECLARE_RATE_INFO(r, s, ip, in, rp, rn, pp, np)    \
	[IWL_RATE_##r##M_INDEX] = { IWL_RATE_##r##M_PLCP,      \
				    IWL_RATE_SISO_##s##M_PLCP, \
				    IWL_RATE_MIMO2_##s##M_PLCP,\
				    IWL_RATE_MIMO3_##s##M_PLCP,\
				    IWL_RATE_##r##M_IEEE,      \
				    IWL_RATE_##ip##M_INDEX,    \
				    IWL_RATE_##in##M_INDEX,    \
				    IWL_RATE_##rp##M_INDEX,    \
				    IWL_RATE_##rn##M_INDEX,    \
				    IWL_RATE_##pp##M_INDEX,    \
				    IWL_RATE_##np##M_INDEX }

const struct iwl_rate_info iwl_rates[IWL_RATE_COUNT] = {
	IWL_DECLARE_RATE_INFO(1, INV, INV, 2, INV, 2, INV, 2),    
	IWL_DECLARE_RATE_INFO(2, INV, 1, 5, 1, 5, 1, 5),          
	IWL_DECLARE_RATE_INFO(5, INV, 2, 6, 2, 11, 2, 11),        
	IWL_DECLARE_RATE_INFO(11, INV, 9, 12, 9, 12, 5, 18),      
	IWL_DECLARE_RATE_INFO(6, 6, 5, 9, 5, 11, 5, 11),        
	IWL_DECLARE_RATE_INFO(9, 6, 6, 11, 6, 11, 5, 11),       
	IWL_DECLARE_RATE_INFO(12, 12, 11, 18, 11, 18, 11, 18),   
	IWL_DECLARE_RATE_INFO(18, 18, 12, 24, 12, 24, 11, 24),   
	IWL_DECLARE_RATE_INFO(24, 24, 18, 36, 18, 36, 18, 36),   
	IWL_DECLARE_RATE_INFO(36, 36, 24, 48, 24, 48, 24, 48),   
	IWL_DECLARE_RATE_INFO(48, 48, 36, 54, 36, 54, 36, 54),   
	IWL_DECLARE_RATE_INFO(54, 54, 48, INV, 48, INV, 48, INV),
	IWL_DECLARE_RATE_INFO(60, 60, 48, INV, 48, INV, 48, INV),
	
};

static inline u8 rs_extract_rate(u32 rate_n_flags)
{
	return (u8)(rate_n_flags & RATE_MCS_RATE_MSK);
}

static int iwl_hwrate_to_plcp_idx(u32 rate_n_flags)
{
	int idx = 0;

	
	if (rate_n_flags & RATE_MCS_HT_MSK) {
		idx = rs_extract_rate(rate_n_flags);

		if (idx >= IWL_RATE_MIMO3_6M_PLCP)
			idx = idx - IWL_RATE_MIMO3_6M_PLCP;
		else if (idx >= IWL_RATE_MIMO2_6M_PLCP)
			idx = idx - IWL_RATE_MIMO2_6M_PLCP;

		idx += IWL_FIRST_OFDM_RATE;
		
		if (idx >= IWL_RATE_9M_INDEX)
			idx += 1;
		if ((idx >= IWL_FIRST_OFDM_RATE) && (idx <= IWL_LAST_OFDM_RATE))
			return idx;

	
	} else {
		for (idx = 0; idx < ARRAY_SIZE(iwl_rates); idx++)
			if (iwl_rates[idx].plcp ==
					rs_extract_rate(rate_n_flags))
				return idx;
	}

	return -1;
}

static void rs_rate_scale_perform(struct iwl_priv *priv,
				   struct sk_buff *skb,
				   struct ieee80211_sta *sta,
				   struct iwl_lq_sta *lq_sta);
static void rs_fill_link_cmd(struct iwl_priv *priv,
			     struct iwl_lq_sta *lq_sta, u32 rate_n_flags);
static void rs_stay_in_table(struct iwl_lq_sta *lq_sta, bool force_search);


#ifdef CONFIG_MAC80211_DEBUGFS
static void rs_dbgfs_set_mcs(struct iwl_lq_sta *lq_sta,
			     u32 *rate_n_flags, int index);
#else
static void rs_dbgfs_set_mcs(struct iwl_lq_sta *lq_sta,
			     u32 *rate_n_flags, int index)
{}
#endif


static s32 expected_tpt_legacy[IWL_RATE_COUNT] = {
	7, 13, 35, 58, 40, 57, 72, 98, 121, 154, 177, 186, 0
};

static s32 expected_tpt_siso20MHz[4][IWL_RATE_COUNT] = {
	{0, 0, 0, 0, 42, 0,  76, 102, 124, 159, 183, 193, 202}, 
	{0, 0, 0, 0, 46, 0,  82, 110, 132, 168, 192, 202, 210}, 
	{0, 0, 0, 0, 47, 0,  91, 133, 171, 242, 305, 334, 362}, 
	{0, 0, 0, 0, 52, 0, 101, 145, 187, 264, 330, 361, 390}, 
};

static s32 expected_tpt_siso40MHz[4][IWL_RATE_COUNT] = {
	{0, 0, 0, 0,  77, 0, 127, 160, 184, 220, 242, 250, 257}, 
	{0, 0, 0, 0,  83, 0, 135, 169, 193, 229, 250, 257, 264}, 
	{0, 0, 0, 0,  94, 0, 177, 249, 313, 423, 512, 550, 586}, 
	{0, 0, 0, 0, 104, 0, 193, 270, 338, 454, 545, 584, 620}, 
};

static s32 expected_tpt_mimo2_20MHz[4][IWL_RATE_COUNT] = {
	{0, 0, 0, 0,  74, 0, 123, 155, 179, 214, 236, 244, 251}, 
	{0, 0, 0, 0,  81, 0, 131, 164, 188, 223, 243, 251, 257}, 
	{0, 0, 0, 0,  89, 0, 167, 235, 296, 402, 488, 526, 560}, 
	{0, 0, 0, 0,  97, 0, 182, 255, 320, 431, 520, 558, 593}, 
};

static s32 expected_tpt_mimo2_40MHz[4][IWL_RATE_COUNT] = {
	{0, 0, 0, 0, 123, 0, 182, 214, 235, 264, 279, 285, 289}, 
	{0, 0, 0, 0, 131, 0, 191, 222, 242, 270, 284, 289, 293}, 
	{0, 0, 0, 0, 171, 0, 305, 410, 496, 634, 731, 771, 805}, 
	{0, 0, 0, 0, 186, 0, 329, 439, 527, 667, 764, 803, 838}, 
};

static s32 expected_tpt_mimo3_20MHz[4][IWL_RATE_COUNT] = {
	{0, 0, 0, 0,  99, 0, 153, 186, 208, 239, 256, 263, 268}, 
	{0, 0, 0, 0, 106, 0, 162, 194, 215, 246, 262, 268, 273}, 
	{0, 0, 0, 0, 134, 0, 249, 346, 431, 574, 685, 732, 775}, 
	{0, 0, 0, 0, 148, 0, 272, 376, 465, 614, 727, 775, 818}, 
};

static s32 expected_tpt_mimo3_40MHz[4][IWL_RATE_COUNT] = {
	{0, 0, 0, 0, 152, 0, 211, 239, 255, 279,  290,  294,  297}, 
	{0, 0, 0, 0, 160, 0, 219, 245, 261, 284,  294,  297,  300}, 
	{0, 0, 0, 0, 254, 0, 443, 584, 695, 868,  984, 1030, 1070}, 
	{0, 0, 0, 0, 277, 0, 478, 624, 737, 911, 1026, 1070, 1109}, 
};

static const struct iwl_rate_mcs_info iwl_rate_mcs[IWL_RATE_COUNT] = {
	{  "1", "BPSK DSSS"},
	{  "2", "QPSK DSSS"},
	{"5.5", "BPSK CCK"},
	{ "11", "QPSK CCK"},
	{  "6", "BPSK 1/2"},
	{  "9", "BPSK 1/2"},
	{ "12", "QPSK 1/2"},
	{ "18", "QPSK 3/4"},
	{ "24", "16QAM 1/2"},
	{ "36", "16QAM 3/4"},
	{ "48", "64QAM 2/3"},
	{ "54", "64QAM 3/4"},
	{ "60", "64QAM 5/6"},
};

#define MCS_INDEX_PER_STREAM	(8)

static void rs_rate_scale_clear_window(struct iwl_rate_scale_data *window)
{
	window->data = 0;
	window->success_counter = 0;
	window->success_ratio = IWL_INVALID_VALUE;
	window->counter = 0;
	window->average_tpt = IWL_INVALID_VALUE;
	window->stamp = 0;
}

static inline u8 rs_is_valid_ant(u8 valid_antenna, u8 ant_type)
{
	return (ant_type & valid_antenna) == ant_type;
}

static void rs_tl_rm_old_stats(struct iwl_traffic_load *tl, u32 curr_time)
{
	
	u32 oldest_time = curr_time - TID_MAX_TIME_DIFF;

	while (tl->queue_count &&
	       (tl->time_stamp < oldest_time)) {
		tl->total -= tl->packet_count[tl->head];
		tl->packet_count[tl->head] = 0;
		tl->time_stamp += TID_QUEUE_CELL_SPACING;
		tl->queue_count--;
		tl->head++;
		if (tl->head >= TID_QUEUE_MAX_SIZE)
			tl->head = 0;
	}
}

static u8 rs_tl_add_packet(struct iwl_lq_sta *lq_data,
			   struct ieee80211_hdr *hdr)
{
	u32 curr_time = jiffies_to_msecs(jiffies);
	u32 time_diff;
	s32 index;
	struct iwl_traffic_load *tl = NULL;
	u8 tid;

	if (ieee80211_is_data_qos(hdr->frame_control)) {
		u8 *qc = ieee80211_get_qos_ctl(hdr);
		tid = qc[0] & 0xf;
	} else
		return IWL_MAX_TID_COUNT;

	if (unlikely(tid >= IWL_MAX_TID_COUNT))
		return IWL_MAX_TID_COUNT;

	tl = &lq_data->load[tid];

	curr_time -= curr_time % TID_ROUND_VALUE;

	
	if (!(tl->queue_count)) {
		tl->total = 1;
		tl->time_stamp = curr_time;
		tl->queue_count = 1;
		tl->head = 0;
		tl->packet_count[0] = 1;
		return IWL_MAX_TID_COUNT;
	}

	time_diff = TIME_WRAP_AROUND(tl->time_stamp, curr_time);
	index = time_diff / TID_QUEUE_CELL_SPACING;

	
	
	if (index >= TID_QUEUE_MAX_SIZE)
		rs_tl_rm_old_stats(tl, curr_time);

	index = (tl->head + index) % TID_QUEUE_MAX_SIZE;
	tl->packet_count[index] = tl->packet_count[index] + 1;
	tl->total = tl->total + 1;

	if ((index + 1) > tl->queue_count)
		tl->queue_count = index + 1;

	return tid;
}

#ifdef CONFIG_MAC80211_DEBUGFS
static void rs_program_fix_rate(struct iwl_priv *priv,
				struct iwl_lq_sta *lq_sta)
{
	struct iwl_station_priv *sta_priv =
		container_of(lq_sta, struct iwl_station_priv, lq_sta);
	struct iwl_rxon_context *ctx = sta_priv->ctx;

	lq_sta->active_legacy_rate = 0x0FFF;	
	lq_sta->active_siso_rate   = 0x1FD0;	
	lq_sta->active_mimo2_rate  = 0x1FD0;	
	lq_sta->active_mimo3_rate  = 0x1FD0;	

#ifdef CONFIG_IWLWIFI_DEVICE_TESTMODE
	
	if (priv->tm_fixed_rate)
		lq_sta->dbg_fixed_rate = priv->tm_fixed_rate;
#endif

	IWL_DEBUG_RATE(priv, "sta_id %d rate 0x%X\n",
		lq_sta->lq.sta_id, lq_sta->dbg_fixed_rate);

	if (lq_sta->dbg_fixed_rate) {
		rs_fill_link_cmd(NULL, lq_sta, lq_sta->dbg_fixed_rate);
		iwl_send_lq_cmd(lq_sta->drv, ctx, &lq_sta->lq, CMD_ASYNC,
				false);
	}
}
#endif

static u32 rs_tl_get_load(struct iwl_lq_sta *lq_data, u8 tid)
{
	u32 curr_time = jiffies_to_msecs(jiffies);
	u32 time_diff;
	s32 index;
	struct iwl_traffic_load *tl = NULL;

	if (tid >= IWL_MAX_TID_COUNT)
		return 0;

	tl = &(lq_data->load[tid]);

	curr_time -= curr_time % TID_ROUND_VALUE;

	if (!(tl->queue_count))
		return 0;

	time_diff = TIME_WRAP_AROUND(tl->time_stamp, curr_time);
	index = time_diff / TID_QUEUE_CELL_SPACING;

	
	
	if (index >= TID_QUEUE_MAX_SIZE)
		rs_tl_rm_old_stats(tl, curr_time);

	return tl->total;
}

static int rs_tl_turn_on_agg_for_tid(struct iwl_priv *priv,
				      struct iwl_lq_sta *lq_data, u8 tid,
				      struct ieee80211_sta *sta)
{
	int ret = -EAGAIN;
	u32 load;

	if (priv->bt_traffic_load >= IWL_BT_COEX_TRAFFIC_LOAD_HIGH) {
		IWL_ERR(priv, "BT traffic (%d), no aggregation allowed\n",
			priv->bt_traffic_load);
		return ret;
	}

	load = rs_tl_get_load(lq_data, tid);

	if ((iwlagn_mod_params.auto_agg) || (load > IWL_AGG_LOAD_THRESHOLD)) {
		IWL_DEBUG_HT(priv, "Starting Tx agg: STA: %pM tid: %d\n",
				sta->addr, tid);
		ret = ieee80211_start_tx_ba_session(sta, tid, 5000);
		if (ret == -EAGAIN) {
			IWL_ERR(priv, "Fail start Tx agg on tid: %d\n",
				tid);
			ieee80211_stop_tx_ba_session(sta, tid);
		}
	} else {
		IWL_DEBUG_HT(priv, "Aggregation not enabled for tid %d "
			"because load = %u\n", tid, load);
	}
	return ret;
}

static void rs_tl_turn_on_agg(struct iwl_priv *priv, u8 tid,
			      struct iwl_lq_sta *lq_data,
			      struct ieee80211_sta *sta)
{
	if (tid < IWL_MAX_TID_COUNT)
		rs_tl_turn_on_agg_for_tid(priv, lq_data, tid, sta);
	else
		IWL_ERR(priv, "tid exceeds max TID count: %d/%d\n",
			tid, IWL_MAX_TID_COUNT);
}

static inline int get_num_of_ant_from_rate(u32 rate_n_flags)
{
	return !!(rate_n_flags & RATE_MCS_ANT_A_MSK) +
	       !!(rate_n_flags & RATE_MCS_ANT_B_MSK) +
	       !!(rate_n_flags & RATE_MCS_ANT_C_MSK);
}

static s32 get_expected_tpt(struct iwl_scale_tbl_info *tbl, int rs_index)
{
	if (tbl->expected_tpt)
		return tbl->expected_tpt[rs_index];
	return 0;
}

static int rs_collect_tx_data(struct iwl_scale_tbl_info *tbl,
			      int scale_index, int attempts, int successes)
{
	struct iwl_rate_scale_data *window = NULL;
	static const u64 mask = (((u64)1) << (IWL_RATE_MAX_WINDOW - 1));
	s32 fail_count, tpt;

	if (scale_index < 0 || scale_index >= IWL_RATE_COUNT)
		return -EINVAL;

	
	window = &(tbl->win[scale_index]);

	
	tpt = get_expected_tpt(tbl, scale_index);

	while (attempts > 0) {
		if (window->counter >= IWL_RATE_MAX_WINDOW) {

			
			window->counter = IWL_RATE_MAX_WINDOW - 1;

			if (window->data & mask) {
				window->data &= ~mask;
				window->success_counter--;
			}
		}

		
		window->counter++;

		
		window->data <<= 1;

		
		if (successes > 0) {
			window->success_counter++;
			window->data |= 0x1;
			successes--;
		}

		attempts--;
	}

	
	if (window->counter > 0)
		window->success_ratio = 128 * (100 * window->success_counter)
					/ window->counter;
	else
		window->success_ratio = IWL_INVALID_VALUE;

	fail_count = window->counter - window->success_counter;

	
	if ((fail_count >= IWL_RATE_MIN_FAILURE_TH) ||
	    (window->success_counter >= IWL_RATE_MIN_SUCCESS_TH))
		window->average_tpt = (window->success_ratio * tpt + 64) / 128;
	else
		window->average_tpt = IWL_INVALID_VALUE;

	
	window->stamp = jiffies;

	return 0;
}

static u32 rate_n_flags_from_tbl(struct iwl_priv *priv,
				 struct iwl_scale_tbl_info *tbl,
				 int index, u8 use_green)
{
	u32 rate_n_flags = 0;

	if (is_legacy(tbl->lq_type)) {
		rate_n_flags = iwl_rates[index].plcp;
		if (index >= IWL_FIRST_CCK_RATE && index <= IWL_LAST_CCK_RATE)
			rate_n_flags |= RATE_MCS_CCK_MSK;

	} else if (is_Ht(tbl->lq_type)) {
		if (index > IWL_LAST_OFDM_RATE) {
			IWL_ERR(priv, "Invalid HT rate index %d\n", index);
			index = IWL_LAST_OFDM_RATE;
		}
		rate_n_flags = RATE_MCS_HT_MSK;

		if (is_siso(tbl->lq_type))
			rate_n_flags |=	iwl_rates[index].plcp_siso;
		else if (is_mimo2(tbl->lq_type))
			rate_n_flags |=	iwl_rates[index].plcp_mimo2;
		else
			rate_n_flags |=	iwl_rates[index].plcp_mimo3;
	} else {
		IWL_ERR(priv, "Invalid tbl->lq_type %d\n", tbl->lq_type);
	}

	rate_n_flags |= ((tbl->ant_type << RATE_MCS_ANT_POS) &
						     RATE_MCS_ANT_ABC_MSK);

	if (is_Ht(tbl->lq_type)) {
		if (tbl->is_ht40) {
			if (tbl->is_dup)
				rate_n_flags |= RATE_MCS_DUP_MSK;
			else
				rate_n_flags |= RATE_MCS_HT40_MSK;
		}
		if (tbl->is_SGI)
			rate_n_flags |= RATE_MCS_SGI_MSK;

		if (use_green) {
			rate_n_flags |= RATE_MCS_GF_MSK;
			if (is_siso(tbl->lq_type) && tbl->is_SGI) {
				rate_n_flags &= ~RATE_MCS_SGI_MSK;
				IWL_ERR(priv, "GF was set with SGI:SISO\n");
			}
		}
	}
	return rate_n_flags;
}

static int rs_get_tbl_info_from_mcs(const u32 rate_n_flags,
				    enum ieee80211_band band,
				    struct iwl_scale_tbl_info *tbl,
				    int *rate_idx)
{
	u32 ant_msk = (rate_n_flags & RATE_MCS_ANT_ABC_MSK);
	u8 num_of_ant = get_num_of_ant_from_rate(rate_n_flags);
	u8 mcs;

	memset(tbl, 0, sizeof(struct iwl_scale_tbl_info));
	*rate_idx = iwl_hwrate_to_plcp_idx(rate_n_flags);

	if (*rate_idx  == IWL_RATE_INVALID) {
		*rate_idx = -1;
		return -EINVAL;
	}
	tbl->is_SGI = 0;	
	tbl->is_ht40 = 0;
	tbl->is_dup = 0;
	tbl->ant_type = (ant_msk >> RATE_MCS_ANT_POS);
	tbl->lq_type = LQ_NONE;
	tbl->max_search = IWL_MAX_SEARCH;

	
	if (!(rate_n_flags & RATE_MCS_HT_MSK)) {
		if (num_of_ant == 1) {
			if (band == IEEE80211_BAND_5GHZ)
				tbl->lq_type = LQ_A;
			else
				tbl->lq_type = LQ_G;
		}
	
	} else {
		if (rate_n_flags & RATE_MCS_SGI_MSK)
			tbl->is_SGI = 1;

		if ((rate_n_flags & RATE_MCS_HT40_MSK) ||
		    (rate_n_flags & RATE_MCS_DUP_MSK))
			tbl->is_ht40 = 1;

		if (rate_n_flags & RATE_MCS_DUP_MSK)
			tbl->is_dup = 1;

		mcs = rs_extract_rate(rate_n_flags);

		
		if (mcs <= IWL_RATE_SISO_60M_PLCP) {
			if (num_of_ant == 1)
				tbl->lq_type = LQ_SISO; 
		
		} else if (mcs <= IWL_RATE_MIMO2_60M_PLCP) {
			if (num_of_ant == 2)
				tbl->lq_type = LQ_MIMO2;
		
		} else {
			if (num_of_ant == 3) {
				tbl->max_search = IWL_MAX_11N_MIMO3_SEARCH;
				tbl->lq_type = LQ_MIMO3;
			}
		}
	}
	return 0;
}

static int rs_toggle_antenna(u32 valid_ant, u32 *rate_n_flags,
			     struct iwl_scale_tbl_info *tbl)
{
	u8 new_ant_type;

	if (!tbl->ant_type || tbl->ant_type > ANT_ABC)
		return 0;

	if (!rs_is_valid_ant(valid_ant, tbl->ant_type))
		return 0;

	new_ant_type = ant_toggle_lookup[tbl->ant_type];

	while ((new_ant_type != tbl->ant_type) &&
	       !rs_is_valid_ant(valid_ant, new_ant_type))
		new_ant_type = ant_toggle_lookup[new_ant_type];

	if (new_ant_type == tbl->ant_type)
		return 0;

	tbl->ant_type = new_ant_type;
	*rate_n_flags &= ~RATE_MCS_ANT_ABC_MSK;
	*rate_n_flags |= new_ant_type << RATE_MCS_ANT_POS;
	return 1;
}

static bool rs_use_green(struct ieee80211_sta *sta)
{
	struct iwl_station_priv *sta_priv = (void *)sta->drv_priv;
	struct iwl_rxon_context *ctx = sta_priv->ctx;

	return (sta->ht_cap.cap & IEEE80211_HT_CAP_GRN_FLD) &&
		!(ctx->ht.non_gf_sta_present);
}

static u16 rs_get_supported_rates(struct iwl_lq_sta *lq_sta,
				  struct ieee80211_hdr *hdr,
				  enum iwl_table_type rate_type)
{
	if (is_legacy(rate_type)) {
		return lq_sta->active_legacy_rate;
	} else {
		if (is_siso(rate_type))
			return lq_sta->active_siso_rate;
		else if (is_mimo2(rate_type))
			return lq_sta->active_mimo2_rate;
		else
			return lq_sta->active_mimo3_rate;
	}
}

static u16 rs_get_adjacent_rate(struct iwl_priv *priv, u8 index, u16 rate_mask,
				int rate_type)
{
	u8 high = IWL_RATE_INVALID;
	u8 low = IWL_RATE_INVALID;

	if (is_a_band(rate_type) || !is_legacy(rate_type)) {
		int i;
		u32 mask;

		
		i = index - 1;
		for (mask = (1 << i); i >= 0; i--, mask >>= 1) {
			if (rate_mask & mask) {
				low = i;
				break;
			}
		}

		
		i = index + 1;
		for (mask = (1 << i); i < IWL_RATE_COUNT; i++, mask <<= 1) {
			if (rate_mask & mask) {
				high = i;
				break;
			}
		}

		return (high << 8) | low;
	}

	low = index;
	while (low != IWL_RATE_INVALID) {
		low = iwl_rates[low].prev_rs;
		if (low == IWL_RATE_INVALID)
			break;
		if (rate_mask & (1 << low))
			break;
		IWL_DEBUG_RATE(priv, "Skipping masked lower rate: %d\n", low);
	}

	high = index;
	while (high != IWL_RATE_INVALID) {
		high = iwl_rates[high].next_rs;
		if (high == IWL_RATE_INVALID)
			break;
		if (rate_mask & (1 << high))
			break;
		IWL_DEBUG_RATE(priv, "Skipping masked higher rate: %d\n", high);
	}

	return (high << 8) | low;
}

static u32 rs_get_lower_rate(struct iwl_lq_sta *lq_sta,
			     struct iwl_scale_tbl_info *tbl,
			     u8 scale_index, u8 ht_possible)
{
	s32 low;
	u16 rate_mask;
	u16 high_low;
	u8 switch_to_legacy = 0;
	u8 is_green = lq_sta->is_green;
	struct iwl_priv *priv = lq_sta->drv;

	if (!is_legacy(tbl->lq_type) && (!ht_possible || !scale_index)) {
		switch_to_legacy = 1;
		scale_index = rs_ht_to_legacy[scale_index];
		if (lq_sta->band == IEEE80211_BAND_5GHZ)
			tbl->lq_type = LQ_A;
		else
			tbl->lq_type = LQ_G;

		if (num_of_ant(tbl->ant_type) > 1)
			tbl->ant_type =
			    first_antenna(hw_params(priv).valid_tx_ant);

		tbl->is_ht40 = 0;
		tbl->is_SGI = 0;
		tbl->max_search = IWL_MAX_SEARCH;
	}

	rate_mask = rs_get_supported_rates(lq_sta, NULL, tbl->lq_type);

	
	if (is_legacy(tbl->lq_type)) {
		
		if (lq_sta->band == IEEE80211_BAND_5GHZ)
			rate_mask  = (u16)(rate_mask &
			   (lq_sta->supp_rates << IWL_FIRST_OFDM_RATE));
		else
			rate_mask = (u16)(rate_mask & lq_sta->supp_rates);
	}

	
	if (switch_to_legacy && (rate_mask & (1 << scale_index))) {
		low = scale_index;
		goto out;
	}

	high_low = rs_get_adjacent_rate(lq_sta->drv, scale_index, rate_mask,
					tbl->lq_type);
	low = high_low & 0xff;

	if (low == IWL_RATE_INVALID)
		low = scale_index;

out:
	return rate_n_flags_from_tbl(lq_sta->drv, tbl, low, is_green);
}

static bool table_type_matches(struct iwl_scale_tbl_info *a,
			       struct iwl_scale_tbl_info *b)
{
	return (a->lq_type == b->lq_type) && (a->ant_type == b->ant_type) &&
		(a->is_SGI == b->is_SGI);
}

static void rs_bt_update_lq(struct iwl_priv *priv, struct iwl_rxon_context *ctx,
			    struct iwl_lq_sta *lq_sta)
{
	struct iwl_scale_tbl_info *tbl;
	bool full_concurrent = priv->bt_full_concurrent;

	if (priv->bt_ant_couple_ok) {
		if (priv->bt_ci_compliance && priv->bt_ant_couple_ok)
			full_concurrent = true;
		else
			full_concurrent = false;
	}
	if ((priv->bt_traffic_load != priv->last_bt_traffic_load) ||
	    (priv->bt_full_concurrent != full_concurrent)) {
		priv->bt_full_concurrent = full_concurrent;

		
		tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
		rs_fill_link_cmd(priv, lq_sta, tbl->current_rate);
		iwl_send_lq_cmd(priv, ctx, &lq_sta->lq, CMD_ASYNC, false);

		queue_work(priv->workqueue, &priv->bt_full_concurrency);
	}
}

static void rs_tx_status(void *priv_r, struct ieee80211_supported_band *sband,
			 struct ieee80211_sta *sta, void *priv_sta,
			 struct sk_buff *skb)
{
	int legacy_success;
	int retries;
	int rs_index, mac_index, i;
	struct iwl_lq_sta *lq_sta = priv_sta;
	struct iwl_link_quality_cmd *table;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct iwl_op_mode *op_mode = (struct iwl_op_mode *)priv_r;
	struct iwl_priv *priv = IWL_OP_MODE_GET_DVM(op_mode);
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	enum mac80211_rate_control_flags mac_flags;
	u32 tx_rate;
	struct iwl_scale_tbl_info tbl_type;
	struct iwl_scale_tbl_info *curr_tbl, *other_tbl, *tmp_tbl;
	struct iwl_station_priv *sta_priv = (void *)sta->drv_priv;
	struct iwl_rxon_context *ctx = sta_priv->ctx;

	IWL_DEBUG_RATE_LIMIT(priv, "get frame ack response, update rate scale window\n");

	
	if (!lq_sta) {
		IWL_DEBUG_RATE(priv, "Station rate scaling not created yet.\n");
		return;
	} else if (!lq_sta->drv) {
		IWL_DEBUG_RATE(priv, "Rate scaling not initialized yet.\n");
		return;
	}

	if (!ieee80211_is_data(hdr->frame_control) ||
	    info->flags & IEEE80211_TX_CTL_NO_ACK)
		return;

	
	if ((info->flags & IEEE80211_TX_CTL_AMPDU) &&
	    !(info->flags & IEEE80211_TX_STAT_AMPDU))
		return;

	table = &lq_sta->lq;
	tx_rate = le32_to_cpu(table->rs_table[0].rate_n_flags);
	rs_get_tbl_info_from_mcs(tx_rate, priv->band, &tbl_type, &rs_index);
	if (priv->band == IEEE80211_BAND_5GHZ)
		rs_index -= IWL_FIRST_OFDM_RATE;
	mac_flags = info->status.rates[0].flags;
	mac_index = info->status.rates[0].idx;
	
	if (mac_flags & IEEE80211_TX_RC_MCS) {
		mac_index &= RATE_MCS_CODE_MSK;	
		if (mac_index >= (IWL_RATE_9M_INDEX - IWL_FIRST_OFDM_RATE))
			mac_index++;
		if (priv->band == IEEE80211_BAND_2GHZ)
			mac_index += IWL_FIRST_OFDM_RATE;
	}
	
	if ((mac_index < 0) ||
	    (tbl_type.is_SGI != !!(mac_flags & IEEE80211_TX_RC_SHORT_GI)) ||
	    (tbl_type.is_ht40 != !!(mac_flags & IEEE80211_TX_RC_40_MHZ_WIDTH)) ||
	    (tbl_type.is_dup != !!(mac_flags & IEEE80211_TX_RC_DUP_DATA)) ||
	    (tbl_type.ant_type != info->antenna_sel_tx) ||
	    (!!(tx_rate & RATE_MCS_HT_MSK) != !!(mac_flags & IEEE80211_TX_RC_MCS)) ||
	    (!!(tx_rate & RATE_MCS_GF_MSK) != !!(mac_flags & IEEE80211_TX_RC_GREEN_FIELD)) ||
	    (rs_index != mac_index)) {
		IWL_DEBUG_RATE(priv, "initial rate %d does not match %d (0x%x)\n", mac_index, rs_index, tx_rate);
		lq_sta->missed_rate_counter++;
		if (lq_sta->missed_rate_counter > IWL_MISSED_RATE_MAX) {
			lq_sta->missed_rate_counter = 0;
			iwl_send_lq_cmd(priv, ctx, &lq_sta->lq, CMD_ASYNC, false);
		}
		
		return;
	} else
		
		lq_sta->missed_rate_counter = 0;

	
	if (table_type_matches(&tbl_type,
				&(lq_sta->lq_info[lq_sta->active_tbl]))) {
		curr_tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
		other_tbl = &(lq_sta->lq_info[1 - lq_sta->active_tbl]);
	} else if (table_type_matches(&tbl_type,
				&lq_sta->lq_info[1 - lq_sta->active_tbl])) {
		curr_tbl = &(lq_sta->lq_info[1 - lq_sta->active_tbl]);
		other_tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
	} else {
		IWL_DEBUG_RATE(priv, "Neither active nor search matches tx rate\n");
		tmp_tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
		IWL_DEBUG_RATE(priv, "active- lq:%x, ant:%x, SGI:%d\n",
			tmp_tbl->lq_type, tmp_tbl->ant_type, tmp_tbl->is_SGI);
		tmp_tbl = &(lq_sta->lq_info[1 - lq_sta->active_tbl]);
		IWL_DEBUG_RATE(priv, "search- lq:%x, ant:%x, SGI:%d\n",
			tmp_tbl->lq_type, tmp_tbl->ant_type, tmp_tbl->is_SGI);
		IWL_DEBUG_RATE(priv, "actual- lq:%x, ant:%x, SGI:%d\n",
			tbl_type.lq_type, tbl_type.ant_type, tbl_type.is_SGI);
		rs_stay_in_table(lq_sta, true);
		goto done;
	}

	if (info->flags & IEEE80211_TX_STAT_AMPDU) {
		tx_rate = le32_to_cpu(table->rs_table[0].rate_n_flags);
		rs_get_tbl_info_from_mcs(tx_rate, priv->band, &tbl_type,
				&rs_index);
		rs_collect_tx_data(curr_tbl, rs_index,
				   info->status.ampdu_len,
				   info->status.ampdu_ack_len);

		
		if (lq_sta->stay_in_tbl) {
			lq_sta->total_success += info->status.ampdu_ack_len;
			lq_sta->total_failed += (info->status.ampdu_len -
					info->status.ampdu_ack_len);
		}
	} else {
		retries = info->status.rates[0].count - 1;
		
		retries = min(retries, 15);

		
		legacy_success = !!(info->flags & IEEE80211_TX_STAT_ACK);
		
		for (i = 0; i <= retries; ++i) {
			tx_rate = le32_to_cpu(table->rs_table[i].rate_n_flags);
			rs_get_tbl_info_from_mcs(tx_rate, priv->band,
					&tbl_type, &rs_index);
			if (table_type_matches(&tbl_type, curr_tbl))
				tmp_tbl = curr_tbl;
			else if (table_type_matches(&tbl_type, other_tbl))
				tmp_tbl = other_tbl;
			else
				continue;
			rs_collect_tx_data(tmp_tbl, rs_index, 1,
					   i < retries ? 0 : legacy_success);
		}

		
		if (lq_sta->stay_in_tbl) {
			lq_sta->total_success += legacy_success;
			lq_sta->total_failed += retries + (1 - legacy_success);
		}
	}
	
	lq_sta->last_rate_n_flags = tx_rate;
done:
	
	if (sta && sta->supp_rates[sband->band])
		rs_rate_scale_perform(priv, skb, sta, lq_sta);

#if defined(CONFIG_MAC80211_DEBUGFS) && defined(CONFIG_IWLWIFI_DEVICE_TESTMODE)
	if ((priv->tm_fixed_rate) &&
	    (priv->tm_fixed_rate != lq_sta->dbg_fixed_rate))
		rs_program_fix_rate(priv, lq_sta);
#endif
	if (cfg(priv)->bt_params && cfg(priv)->bt_params->advanced_bt_coexist)
		rs_bt_update_lq(priv, ctx, lq_sta);
}

static void rs_set_stay_in_table(struct iwl_priv *priv, u8 is_legacy,
				 struct iwl_lq_sta *lq_sta)
{
	IWL_DEBUG_RATE(priv, "we are staying in the same table\n");
	lq_sta->stay_in_tbl = 1;	
	if (is_legacy) {
		lq_sta->table_count_limit = IWL_LEGACY_TABLE_COUNT;
		lq_sta->max_failure_limit = IWL_LEGACY_FAILURE_LIMIT;
		lq_sta->max_success_limit = IWL_LEGACY_SUCCESS_LIMIT;
	} else {
		lq_sta->table_count_limit = IWL_NONE_LEGACY_TABLE_COUNT;
		lq_sta->max_failure_limit = IWL_NONE_LEGACY_FAILURE_LIMIT;
		lq_sta->max_success_limit = IWL_NONE_LEGACY_SUCCESS_LIMIT;
	}
	lq_sta->table_count = 0;
	lq_sta->total_failed = 0;
	lq_sta->total_success = 0;
	lq_sta->flush_timer = jiffies;
	lq_sta->action_counter = 0;
}

static void rs_set_expected_tpt_table(struct iwl_lq_sta *lq_sta,
				      struct iwl_scale_tbl_info *tbl)
{
	
	s32 (*ht_tbl_pointer)[IWL_RATE_COUNT];

	
	if (WARN_ON_ONCE(!is_legacy(tbl->lq_type) && !is_Ht(tbl->lq_type))) {
		tbl->expected_tpt = expected_tpt_legacy;
		return;
	}

	
	if (is_legacy(tbl->lq_type)) {
		tbl->expected_tpt = expected_tpt_legacy;
		return;
	}

	if (is_siso(tbl->lq_type) && (!tbl->is_ht40 || lq_sta->is_dup))
		ht_tbl_pointer = expected_tpt_siso20MHz;
	else if (is_siso(tbl->lq_type))
		ht_tbl_pointer = expected_tpt_siso40MHz;
	else if (is_mimo2(tbl->lq_type) && (!tbl->is_ht40 || lq_sta->is_dup))
		ht_tbl_pointer = expected_tpt_mimo2_20MHz;
	else if (is_mimo2(tbl->lq_type))
		ht_tbl_pointer = expected_tpt_mimo2_40MHz;
	else if (is_mimo3(tbl->lq_type) && (!tbl->is_ht40 || lq_sta->is_dup))
		ht_tbl_pointer = expected_tpt_mimo3_20MHz;
	else 
		ht_tbl_pointer = expected_tpt_mimo3_40MHz;

	if (!tbl->is_SGI && !lq_sta->is_agg)		
		tbl->expected_tpt = ht_tbl_pointer[0];
	else if (tbl->is_SGI && !lq_sta->is_agg)	
		tbl->expected_tpt = ht_tbl_pointer[1];
	else if (!tbl->is_SGI && lq_sta->is_agg)	
		tbl->expected_tpt = ht_tbl_pointer[2];
	else						
		tbl->expected_tpt = ht_tbl_pointer[3];
}

static s32 rs_get_best_rate(struct iwl_priv *priv,
			    struct iwl_lq_sta *lq_sta,
			    struct iwl_scale_tbl_info *tbl,	
			    u16 rate_mask, s8 index)
{
	
	struct iwl_scale_tbl_info *active_tbl =
	    &(lq_sta->lq_info[lq_sta->active_tbl]);
	s32 active_sr = active_tbl->win[index].success_ratio;
	s32 active_tpt = active_tbl->expected_tpt[index];

	
	s32 *tpt_tbl = tbl->expected_tpt;

	s32 new_rate, high, low, start_hi;
	u16 high_low;
	s8 rate = index;

	new_rate = high = low = start_hi = IWL_RATE_INVALID;

	for (; ;) {
		high_low = rs_get_adjacent_rate(priv, rate, rate_mask,
						tbl->lq_type);

		low = high_low & 0xff;
		high = (high_low >> 8) & 0xff;

		if ((((100 * tpt_tbl[rate]) > lq_sta->last_tpt) &&
		     ((active_sr > IWL_RATE_DECREASE_TH) &&
		      (active_sr <= IWL_RATE_HIGH_TH) &&
		      (tpt_tbl[rate] <= active_tpt))) ||
		    ((active_sr >= IWL_RATE_SCALE_SWITCH) &&
		     (tpt_tbl[rate] > active_tpt))) {

			if (start_hi != IWL_RATE_INVALID) {
				new_rate = start_hi;
				break;
			}

			new_rate = rate;

			
			if (low != IWL_RATE_INVALID)
				rate = low;

			
			else
				break;

		
		} else {
			if (new_rate != IWL_RATE_INVALID)
				break;

			
			else if (high != IWL_RATE_INVALID) {
				start_hi = high;
				rate = high;

			
			} else {
				new_rate = rate;
				break;
			}
		}
	}

	return new_rate;
}

static int rs_switch_to_mimo2(struct iwl_priv *priv,
			     struct iwl_lq_sta *lq_sta,
			     struct ieee80211_conf *conf,
			     struct ieee80211_sta *sta,
			     struct iwl_scale_tbl_info *tbl, int index)
{
	u16 rate_mask;
	s32 rate;
	s8 is_green = lq_sta->is_green;
	struct iwl_station_priv *sta_priv = (void *)sta->drv_priv;
	struct iwl_rxon_context *ctx = sta_priv->ctx;

	if (!conf_is_ht(conf) || !sta->ht_cap.ht_supported)
		return -1;

	if (((sta->ht_cap.cap & IEEE80211_HT_CAP_SM_PS) >> 2)
						== WLAN_HT_CAP_SM_PS_STATIC)
		return -1;

	
	if (hw_params(priv).tx_chains_num < 2)
		return -1;

	IWL_DEBUG_RATE(priv, "LQ: try to switch to MIMO2\n");

	tbl->lq_type = LQ_MIMO2;
	tbl->is_dup = lq_sta->is_dup;
	tbl->action = 0;
	tbl->max_search = IWL_MAX_SEARCH;
	rate_mask = lq_sta->active_mimo2_rate;

	if (iwl_is_ht40_tx_allowed(priv, ctx, &sta->ht_cap))
		tbl->is_ht40 = 1;
	else
		tbl->is_ht40 = 0;

	rs_set_expected_tpt_table(lq_sta, tbl);

	rate = rs_get_best_rate(priv, lq_sta, tbl, rate_mask, index);

	IWL_DEBUG_RATE(priv, "LQ: MIMO2 best rate %d mask %X\n", rate, rate_mask);
	if ((rate == IWL_RATE_INVALID) || !((1 << rate) & rate_mask)) {
		IWL_DEBUG_RATE(priv, "Can't switch with index %d rate mask %x\n",
						rate, rate_mask);
		return -1;
	}
	tbl->current_rate = rate_n_flags_from_tbl(priv, tbl, rate, is_green);

	IWL_DEBUG_RATE(priv, "LQ: Switch to new mcs %X index is green %X\n",
		     tbl->current_rate, is_green);
	return 0;
}

static int rs_switch_to_mimo3(struct iwl_priv *priv,
			     struct iwl_lq_sta *lq_sta,
			     struct ieee80211_conf *conf,
			     struct ieee80211_sta *sta,
			     struct iwl_scale_tbl_info *tbl, int index)
{
	u16 rate_mask;
	s32 rate;
	s8 is_green = lq_sta->is_green;
	struct iwl_station_priv *sta_priv = (void *)sta->drv_priv;
	struct iwl_rxon_context *ctx = sta_priv->ctx;

	if (!conf_is_ht(conf) || !sta->ht_cap.ht_supported)
		return -1;

	if (((sta->ht_cap.cap & IEEE80211_HT_CAP_SM_PS) >> 2)
						== WLAN_HT_CAP_SM_PS_STATIC)
		return -1;

	
	if (hw_params(priv).tx_chains_num < 3)
		return -1;

	IWL_DEBUG_RATE(priv, "LQ: try to switch to MIMO3\n");

	tbl->lq_type = LQ_MIMO3;
	tbl->is_dup = lq_sta->is_dup;
	tbl->action = 0;
	tbl->max_search = IWL_MAX_11N_MIMO3_SEARCH;
	rate_mask = lq_sta->active_mimo3_rate;

	if (iwl_is_ht40_tx_allowed(priv, ctx, &sta->ht_cap))
		tbl->is_ht40 = 1;
	else
		tbl->is_ht40 = 0;

	rs_set_expected_tpt_table(lq_sta, tbl);

	rate = rs_get_best_rate(priv, lq_sta, tbl, rate_mask, index);

	IWL_DEBUG_RATE(priv, "LQ: MIMO3 best rate %d mask %X\n",
		rate, rate_mask);
	if ((rate == IWL_RATE_INVALID) || !((1 << rate) & rate_mask)) {
		IWL_DEBUG_RATE(priv, "Can't switch with index %d rate mask %x\n",
						rate, rate_mask);
		return -1;
	}
	tbl->current_rate = rate_n_flags_from_tbl(priv, tbl, rate, is_green);

	IWL_DEBUG_RATE(priv, "LQ: Switch to new mcs %X index is green %X\n",
		     tbl->current_rate, is_green);
	return 0;
}

static int rs_switch_to_siso(struct iwl_priv *priv,
			     struct iwl_lq_sta *lq_sta,
			     struct ieee80211_conf *conf,
			     struct ieee80211_sta *sta,
			     struct iwl_scale_tbl_info *tbl, int index)
{
	u16 rate_mask;
	u8 is_green = lq_sta->is_green;
	s32 rate;
	struct iwl_station_priv *sta_priv = (void *)sta->drv_priv;
	struct iwl_rxon_context *ctx = sta_priv->ctx;

	if (!conf_is_ht(conf) || !sta->ht_cap.ht_supported)
		return -1;

	IWL_DEBUG_RATE(priv, "LQ: try to switch to SISO\n");

	tbl->is_dup = lq_sta->is_dup;
	tbl->lq_type = LQ_SISO;
	tbl->action = 0;
	tbl->max_search = IWL_MAX_SEARCH;
	rate_mask = lq_sta->active_siso_rate;

	if (iwl_is_ht40_tx_allowed(priv, ctx, &sta->ht_cap))
		tbl->is_ht40 = 1;
	else
		tbl->is_ht40 = 0;

	if (is_green)
		tbl->is_SGI = 0; 

	rs_set_expected_tpt_table(lq_sta, tbl);
	rate = rs_get_best_rate(priv, lq_sta, tbl, rate_mask, index);

	IWL_DEBUG_RATE(priv, "LQ: get best rate %d mask %X\n", rate, rate_mask);
	if ((rate == IWL_RATE_INVALID) || !((1 << rate) & rate_mask)) {
		IWL_DEBUG_RATE(priv, "can not switch with index %d rate mask %x\n",
			     rate, rate_mask);
		return -1;
	}
	tbl->current_rate = rate_n_flags_from_tbl(priv, tbl, rate, is_green);
	IWL_DEBUG_RATE(priv, "LQ: Switch to new mcs %X index is green %X\n",
		     tbl->current_rate, is_green);
	return 0;
}

static int rs_move_legacy_other(struct iwl_priv *priv,
				struct iwl_lq_sta *lq_sta,
				struct ieee80211_conf *conf,
				struct ieee80211_sta *sta,
				int index)
{
	struct iwl_scale_tbl_info *tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
	struct iwl_scale_tbl_info *search_tbl =
				&(lq_sta->lq_info[(1 - lq_sta->active_tbl)]);
	struct iwl_rate_scale_data *window = &(tbl->win[index]);
	u32 sz = (sizeof(struct iwl_scale_tbl_info) -
		  (sizeof(struct iwl_rate_scale_data) * IWL_RATE_COUNT));
	u8 start_action;
	u8 valid_tx_ant = hw_params(priv).valid_tx_ant;
	u8 tx_chains_num = hw_params(priv).tx_chains_num;
	int ret = 0;
	u8 update_search_tbl_counter = 0;

	switch (priv->bt_traffic_load) {
	case IWL_BT_COEX_TRAFFIC_LOAD_NONE:
		
		break;
	case IWL_BT_COEX_TRAFFIC_LOAD_LOW:
		
		if (tbl->action == IWL_LEGACY_SWITCH_ANTENNA2)
			tbl->action = IWL_LEGACY_SWITCH_SISO;
		break;
	case IWL_BT_COEX_TRAFFIC_LOAD_HIGH:
	case IWL_BT_COEX_TRAFFIC_LOAD_CONTINUOUS:
		
		valid_tx_ant =
			first_antenna(hw_params(priv).valid_tx_ant);
		if (tbl->action >= IWL_LEGACY_SWITCH_ANTENNA2 &&
		    tbl->action != IWL_LEGACY_SWITCH_SISO)
			tbl->action = IWL_LEGACY_SWITCH_SISO;
		break;
	default:
		IWL_ERR(priv, "Invalid BT load %d", priv->bt_traffic_load);
		break;
	}

	if (!iwl_ht_enabled(priv))
		
		tbl->action = IWL_LEGACY_SWITCH_ANTENNA1;
	else if (iwl_tx_ant_restriction(priv) == IWL_ANT_OK_SINGLE &&
		   tbl->action > IWL_LEGACY_SWITCH_SISO)
		tbl->action = IWL_LEGACY_SWITCH_SISO;

	
	if (priv->bt_full_concurrent) {
		if (!iwl_ht_enabled(priv))
			tbl->action = IWL_LEGACY_SWITCH_ANTENNA1;
		else if (tbl->action >= IWL_LEGACY_SWITCH_ANTENNA2)
			tbl->action = IWL_LEGACY_SWITCH_SISO;
		valid_tx_ant =
			first_antenna(hw_params(priv).valid_tx_ant);
	}

	start_action = tbl->action;
	for (; ;) {
		lq_sta->action_counter++;
		switch (tbl->action) {
		case IWL_LEGACY_SWITCH_ANTENNA1:
		case IWL_LEGACY_SWITCH_ANTENNA2:
			IWL_DEBUG_RATE(priv, "LQ: Legacy toggle Antenna\n");

			if ((tbl->action == IWL_LEGACY_SWITCH_ANTENNA1 &&
							tx_chains_num <= 1) ||
			    (tbl->action == IWL_LEGACY_SWITCH_ANTENNA2 &&
							tx_chains_num <= 2))
				break;

			
			if (window->success_ratio >= IWL_RS_GOOD_RATIO &&
			    !priv->bt_full_concurrent &&
			    priv->bt_traffic_load ==
					IWL_BT_COEX_TRAFFIC_LOAD_NONE)
				break;

			
			memcpy(search_tbl, tbl, sz);

			if (rs_toggle_antenna(valid_tx_ant,
				&search_tbl->current_rate, search_tbl)) {
				update_search_tbl_counter = 1;
				rs_set_expected_tpt_table(lq_sta, search_tbl);
				goto out;
			}
			break;
		case IWL_LEGACY_SWITCH_SISO:
			IWL_DEBUG_RATE(priv, "LQ: Legacy switch to SISO\n");

			
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;
			ret = rs_switch_to_siso(priv, lq_sta, conf, sta,
						 search_tbl, index);
			if (!ret) {
				lq_sta->action_counter = 0;
				goto out;
			}

			break;
		case IWL_LEGACY_SWITCH_MIMO2_AB:
		case IWL_LEGACY_SWITCH_MIMO2_AC:
		case IWL_LEGACY_SWITCH_MIMO2_BC:
			IWL_DEBUG_RATE(priv, "LQ: Legacy switch to MIMO2\n");

			
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;

			if (tbl->action == IWL_LEGACY_SWITCH_MIMO2_AB)
				search_tbl->ant_type = ANT_AB;
			else if (tbl->action == IWL_LEGACY_SWITCH_MIMO2_AC)
				search_tbl->ant_type = ANT_AC;
			else
				search_tbl->ant_type = ANT_BC;

			if (!rs_is_valid_ant(valid_tx_ant, search_tbl->ant_type))
				break;

			ret = rs_switch_to_mimo2(priv, lq_sta, conf, sta,
						 search_tbl, index);
			if (!ret) {
				lq_sta->action_counter = 0;
				goto out;
			}
			break;

		case IWL_LEGACY_SWITCH_MIMO3_ABC:
			IWL_DEBUG_RATE(priv, "LQ: Legacy switch to MIMO3\n");

			
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;

			search_tbl->ant_type = ANT_ABC;

			if (!rs_is_valid_ant(valid_tx_ant, search_tbl->ant_type))
				break;

			ret = rs_switch_to_mimo3(priv, lq_sta, conf, sta,
						 search_tbl, index);
			if (!ret) {
				lq_sta->action_counter = 0;
				goto out;
			}
			break;
		}
		tbl->action++;
		if (tbl->action > IWL_LEGACY_SWITCH_MIMO3_ABC)
			tbl->action = IWL_LEGACY_SWITCH_ANTENNA1;

		if (tbl->action == start_action)
			break;

	}
	search_tbl->lq_type = LQ_NONE;
	return 0;

out:
	lq_sta->search_better_tbl = 1;
	tbl->action++;
	if (tbl->action > IWL_LEGACY_SWITCH_MIMO3_ABC)
		tbl->action = IWL_LEGACY_SWITCH_ANTENNA1;
	if (update_search_tbl_counter)
		search_tbl->action = tbl->action;
	return 0;

}

static int rs_move_siso_to_other(struct iwl_priv *priv,
				 struct iwl_lq_sta *lq_sta,
				 struct ieee80211_conf *conf,
				 struct ieee80211_sta *sta, int index)
{
	u8 is_green = lq_sta->is_green;
	struct iwl_scale_tbl_info *tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
	struct iwl_scale_tbl_info *search_tbl =
				&(lq_sta->lq_info[(1 - lq_sta->active_tbl)]);
	struct iwl_rate_scale_data *window = &(tbl->win[index]);
	struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
	u32 sz = (sizeof(struct iwl_scale_tbl_info) -
		  (sizeof(struct iwl_rate_scale_data) * IWL_RATE_COUNT));
	u8 start_action;
	u8 valid_tx_ant = hw_params(priv).valid_tx_ant;
	u8 tx_chains_num = hw_params(priv).tx_chains_num;
	u8 update_search_tbl_counter = 0;
	int ret;

	switch (priv->bt_traffic_load) {
	case IWL_BT_COEX_TRAFFIC_LOAD_NONE:
		
		break;
	case IWL_BT_COEX_TRAFFIC_LOAD_LOW:
		
		if (tbl->action == IWL_SISO_SWITCH_ANTENNA2)
			tbl->action = IWL_SISO_SWITCH_MIMO2_AB;
		break;
	case IWL_BT_COEX_TRAFFIC_LOAD_HIGH:
	case IWL_BT_COEX_TRAFFIC_LOAD_CONTINUOUS:
		
		valid_tx_ant =
			first_antenna(hw_params(priv).valid_tx_ant);
		if (tbl->action != IWL_SISO_SWITCH_ANTENNA1)
			tbl->action = IWL_SISO_SWITCH_ANTENNA1;
		break;
	default:
		IWL_ERR(priv, "Invalid BT load %d", priv->bt_traffic_load);
		break;
	}

	if (iwl_tx_ant_restriction(priv) == IWL_ANT_OK_SINGLE &&
	    tbl->action > IWL_SISO_SWITCH_ANTENNA2) {
		
		tbl->action = IWL_SISO_SWITCH_ANTENNA1;
	}

	
	if (priv->bt_full_concurrent) {
		valid_tx_ant =
			first_antenna(hw_params(priv).valid_tx_ant);
		if (tbl->action >= IWL_LEGACY_SWITCH_ANTENNA2)
			tbl->action = IWL_SISO_SWITCH_ANTENNA1;
	}

	start_action = tbl->action;
	for (;;) {
		lq_sta->action_counter++;
		switch (tbl->action) {
		case IWL_SISO_SWITCH_ANTENNA1:
		case IWL_SISO_SWITCH_ANTENNA2:
			IWL_DEBUG_RATE(priv, "LQ: SISO toggle Antenna\n");
			if ((tbl->action == IWL_SISO_SWITCH_ANTENNA1 &&
						tx_chains_num <= 1) ||
			    (tbl->action == IWL_SISO_SWITCH_ANTENNA2 &&
						tx_chains_num <= 2))
				break;

			if (window->success_ratio >= IWL_RS_GOOD_RATIO &&
			    !priv->bt_full_concurrent &&
			    priv->bt_traffic_load ==
					IWL_BT_COEX_TRAFFIC_LOAD_NONE)
				break;

			memcpy(search_tbl, tbl, sz);
			if (rs_toggle_antenna(valid_tx_ant,
				       &search_tbl->current_rate, search_tbl)) {
				update_search_tbl_counter = 1;
				goto out;
			}
			break;
		case IWL_SISO_SWITCH_MIMO2_AB:
		case IWL_SISO_SWITCH_MIMO2_AC:
		case IWL_SISO_SWITCH_MIMO2_BC:
			IWL_DEBUG_RATE(priv, "LQ: SISO switch to MIMO2\n");
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;

			if (tbl->action == IWL_SISO_SWITCH_MIMO2_AB)
				search_tbl->ant_type = ANT_AB;
			else if (tbl->action == IWL_SISO_SWITCH_MIMO2_AC)
				search_tbl->ant_type = ANT_AC;
			else
				search_tbl->ant_type = ANT_BC;

			if (!rs_is_valid_ant(valid_tx_ant, search_tbl->ant_type))
				break;

			ret = rs_switch_to_mimo2(priv, lq_sta, conf, sta,
						 search_tbl, index);
			if (!ret)
				goto out;
			break;
		case IWL_SISO_SWITCH_GI:
			if (!tbl->is_ht40 && !(ht_cap->cap &
						IEEE80211_HT_CAP_SGI_20))
				break;
			if (tbl->is_ht40 && !(ht_cap->cap &
						IEEE80211_HT_CAP_SGI_40))
				break;

			IWL_DEBUG_RATE(priv, "LQ: SISO toggle SGI/NGI\n");

			memcpy(search_tbl, tbl, sz);
			if (is_green) {
				if (!tbl->is_SGI)
					break;
				else
					IWL_ERR(priv,
						"SGI was set in GF+SISO\n");
			}
			search_tbl->is_SGI = !tbl->is_SGI;
			rs_set_expected_tpt_table(lq_sta, search_tbl);
			if (tbl->is_SGI) {
				s32 tpt = lq_sta->last_tpt / 100;
				if (tpt >= search_tbl->expected_tpt[index])
					break;
			}
			search_tbl->current_rate =
				rate_n_flags_from_tbl(priv, search_tbl,
						      index, is_green);
			update_search_tbl_counter = 1;
			goto out;
		case IWL_SISO_SWITCH_MIMO3_ABC:
			IWL_DEBUG_RATE(priv, "LQ: SISO switch to MIMO3\n");
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;
			search_tbl->ant_type = ANT_ABC;

			if (!rs_is_valid_ant(valid_tx_ant, search_tbl->ant_type))
				break;

			ret = rs_switch_to_mimo3(priv, lq_sta, conf, sta,
						 search_tbl, index);
			if (!ret)
				goto out;
			break;
		}
		tbl->action++;
		if (tbl->action > IWL_LEGACY_SWITCH_MIMO3_ABC)
			tbl->action = IWL_SISO_SWITCH_ANTENNA1;

		if (tbl->action == start_action)
			break;
	}
	search_tbl->lq_type = LQ_NONE;
	return 0;

 out:
	lq_sta->search_better_tbl = 1;
	tbl->action++;
	if (tbl->action > IWL_SISO_SWITCH_MIMO3_ABC)
		tbl->action = IWL_SISO_SWITCH_ANTENNA1;
	if (update_search_tbl_counter)
		search_tbl->action = tbl->action;

	return 0;
}

static int rs_move_mimo2_to_other(struct iwl_priv *priv,
				 struct iwl_lq_sta *lq_sta,
				 struct ieee80211_conf *conf,
				 struct ieee80211_sta *sta, int index)
{
	s8 is_green = lq_sta->is_green;
	struct iwl_scale_tbl_info *tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
	struct iwl_scale_tbl_info *search_tbl =
				&(lq_sta->lq_info[(1 - lq_sta->active_tbl)]);
	struct iwl_rate_scale_data *window = &(tbl->win[index]);
	struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
	u32 sz = (sizeof(struct iwl_scale_tbl_info) -
		  (sizeof(struct iwl_rate_scale_data) * IWL_RATE_COUNT));
	u8 start_action;
	u8 valid_tx_ant = hw_params(priv).valid_tx_ant;
	u8 tx_chains_num = hw_params(priv).tx_chains_num;
	u8 update_search_tbl_counter = 0;
	int ret;

	switch (priv->bt_traffic_load) {
	case IWL_BT_COEX_TRAFFIC_LOAD_NONE:
		
		break;
	case IWL_BT_COEX_TRAFFIC_LOAD_HIGH:
	case IWL_BT_COEX_TRAFFIC_LOAD_CONTINUOUS:
		
		if (tbl->action != IWL_MIMO2_SWITCH_SISO_A)
			tbl->action = IWL_MIMO2_SWITCH_SISO_A;
		break;
	case IWL_BT_COEX_TRAFFIC_LOAD_LOW:
		
		if (tbl->action == IWL_MIMO2_SWITCH_SISO_B ||
		    tbl->action == IWL_MIMO2_SWITCH_SISO_C)
			tbl->action = IWL_MIMO2_SWITCH_SISO_A;
		break;
	default:
		IWL_ERR(priv, "Invalid BT load %d", priv->bt_traffic_load);
		break;
	}

	if ((iwl_tx_ant_restriction(priv) == IWL_ANT_OK_SINGLE) &&
	    (tbl->action < IWL_MIMO2_SWITCH_SISO_A ||
	     tbl->action > IWL_MIMO2_SWITCH_SISO_C)) {
		
		tbl->action = IWL_MIMO2_SWITCH_SISO_A;
	}

	
	if (priv->bt_full_concurrent &&
	    (tbl->action < IWL_MIMO2_SWITCH_SISO_A ||
	     tbl->action > IWL_MIMO2_SWITCH_SISO_C))
		tbl->action = IWL_MIMO2_SWITCH_SISO_A;

	start_action = tbl->action;
	for (;;) {
		lq_sta->action_counter++;
		switch (tbl->action) {
		case IWL_MIMO2_SWITCH_ANTENNA1:
		case IWL_MIMO2_SWITCH_ANTENNA2:
			IWL_DEBUG_RATE(priv, "LQ: MIMO2 toggle Antennas\n");

			if (tx_chains_num <= 2)
				break;

			if (window->success_ratio >= IWL_RS_GOOD_RATIO)
				break;

			memcpy(search_tbl, tbl, sz);
			if (rs_toggle_antenna(valid_tx_ant,
				       &search_tbl->current_rate, search_tbl)) {
				update_search_tbl_counter = 1;
				goto out;
			}
			break;
		case IWL_MIMO2_SWITCH_SISO_A:
		case IWL_MIMO2_SWITCH_SISO_B:
		case IWL_MIMO2_SWITCH_SISO_C:
			IWL_DEBUG_RATE(priv, "LQ: MIMO2 switch to SISO\n");

			
			memcpy(search_tbl, tbl, sz);

			if (tbl->action == IWL_MIMO2_SWITCH_SISO_A)
				search_tbl->ant_type = ANT_A;
			else if (tbl->action == IWL_MIMO2_SWITCH_SISO_B)
				search_tbl->ant_type = ANT_B;
			else
				search_tbl->ant_type = ANT_C;

			if (!rs_is_valid_ant(valid_tx_ant, search_tbl->ant_type))
				break;

			ret = rs_switch_to_siso(priv, lq_sta, conf, sta,
						 search_tbl, index);
			if (!ret)
				goto out;

			break;

		case IWL_MIMO2_SWITCH_GI:
			if (!tbl->is_ht40 && !(ht_cap->cap &
						IEEE80211_HT_CAP_SGI_20))
				break;
			if (tbl->is_ht40 && !(ht_cap->cap &
						IEEE80211_HT_CAP_SGI_40))
				break;

			IWL_DEBUG_RATE(priv, "LQ: MIMO2 toggle SGI/NGI\n");

			
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = !tbl->is_SGI;
			rs_set_expected_tpt_table(lq_sta, search_tbl);
			if (tbl->is_SGI) {
				s32 tpt = lq_sta->last_tpt / 100;
				if (tpt >= search_tbl->expected_tpt[index])
					break;
			}
			search_tbl->current_rate =
				rate_n_flags_from_tbl(priv, search_tbl,
						      index, is_green);
			update_search_tbl_counter = 1;
			goto out;

		case IWL_MIMO2_SWITCH_MIMO3_ABC:
			IWL_DEBUG_RATE(priv, "LQ: MIMO2 switch to MIMO3\n");
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;
			search_tbl->ant_type = ANT_ABC;

			if (!rs_is_valid_ant(valid_tx_ant, search_tbl->ant_type))
				break;

			ret = rs_switch_to_mimo3(priv, lq_sta, conf, sta,
						 search_tbl, index);
			if (!ret)
				goto out;

			break;
		}
		tbl->action++;
		if (tbl->action > IWL_MIMO2_SWITCH_MIMO3_ABC)
			tbl->action = IWL_MIMO2_SWITCH_ANTENNA1;

		if (tbl->action == start_action)
			break;
	}
	search_tbl->lq_type = LQ_NONE;
	return 0;
 out:
	lq_sta->search_better_tbl = 1;
	tbl->action++;
	if (tbl->action > IWL_MIMO2_SWITCH_MIMO3_ABC)
		tbl->action = IWL_MIMO2_SWITCH_ANTENNA1;
	if (update_search_tbl_counter)
		search_tbl->action = tbl->action;

	return 0;

}

static int rs_move_mimo3_to_other(struct iwl_priv *priv,
				 struct iwl_lq_sta *lq_sta,
				 struct ieee80211_conf *conf,
				 struct ieee80211_sta *sta, int index)
{
	s8 is_green = lq_sta->is_green;
	struct iwl_scale_tbl_info *tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
	struct iwl_scale_tbl_info *search_tbl =
				&(lq_sta->lq_info[(1 - lq_sta->active_tbl)]);
	struct iwl_rate_scale_data *window = &(tbl->win[index]);
	struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
	u32 sz = (sizeof(struct iwl_scale_tbl_info) -
		  (sizeof(struct iwl_rate_scale_data) * IWL_RATE_COUNT));
	u8 start_action;
	u8 valid_tx_ant = hw_params(priv).valid_tx_ant;
	u8 tx_chains_num = hw_params(priv).tx_chains_num;
	int ret;
	u8 update_search_tbl_counter = 0;

	switch (priv->bt_traffic_load) {
	case IWL_BT_COEX_TRAFFIC_LOAD_NONE:
		
		break;
	case IWL_BT_COEX_TRAFFIC_LOAD_HIGH:
	case IWL_BT_COEX_TRAFFIC_LOAD_CONTINUOUS:
		
		if (tbl->action != IWL_MIMO3_SWITCH_SISO_A)
			tbl->action = IWL_MIMO3_SWITCH_SISO_A;
		break;
	case IWL_BT_COEX_TRAFFIC_LOAD_LOW:
		
		if (tbl->action == IWL_MIMO3_SWITCH_SISO_B ||
		    tbl->action == IWL_MIMO3_SWITCH_SISO_C)
			tbl->action = IWL_MIMO3_SWITCH_SISO_A;
		break;
	default:
		IWL_ERR(priv, "Invalid BT load %d", priv->bt_traffic_load);
		break;
	}

	if ((iwl_tx_ant_restriction(priv) == IWL_ANT_OK_SINGLE) &&
	    (tbl->action < IWL_MIMO3_SWITCH_SISO_A ||
	     tbl->action > IWL_MIMO3_SWITCH_SISO_C)) {
		
		tbl->action = IWL_MIMO3_SWITCH_SISO_A;
	}

	
	if (priv->bt_full_concurrent &&
	    (tbl->action < IWL_MIMO3_SWITCH_SISO_A ||
	     tbl->action > IWL_MIMO3_SWITCH_SISO_C))
		tbl->action = IWL_MIMO3_SWITCH_SISO_A;

	start_action = tbl->action;
	for (;;) {
		lq_sta->action_counter++;
		switch (tbl->action) {
		case IWL_MIMO3_SWITCH_ANTENNA1:
		case IWL_MIMO3_SWITCH_ANTENNA2:
			IWL_DEBUG_RATE(priv, "LQ: MIMO3 toggle Antennas\n");

			if (tx_chains_num <= 3)
				break;

			if (window->success_ratio >= IWL_RS_GOOD_RATIO)
				break;

			memcpy(search_tbl, tbl, sz);
			if (rs_toggle_antenna(valid_tx_ant,
				       &search_tbl->current_rate, search_tbl))
				goto out;
			break;
		case IWL_MIMO3_SWITCH_SISO_A:
		case IWL_MIMO3_SWITCH_SISO_B:
		case IWL_MIMO3_SWITCH_SISO_C:
			IWL_DEBUG_RATE(priv, "LQ: MIMO3 switch to SISO\n");

			
			memcpy(search_tbl, tbl, sz);

			if (tbl->action == IWL_MIMO3_SWITCH_SISO_A)
				search_tbl->ant_type = ANT_A;
			else if (tbl->action == IWL_MIMO3_SWITCH_SISO_B)
				search_tbl->ant_type = ANT_B;
			else
				search_tbl->ant_type = ANT_C;

			if (!rs_is_valid_ant(valid_tx_ant, search_tbl->ant_type))
				break;

			ret = rs_switch_to_siso(priv, lq_sta, conf, sta,
						 search_tbl, index);
			if (!ret)
				goto out;

			break;

		case IWL_MIMO3_SWITCH_MIMO2_AB:
		case IWL_MIMO3_SWITCH_MIMO2_AC:
		case IWL_MIMO3_SWITCH_MIMO2_BC:
			IWL_DEBUG_RATE(priv, "LQ: MIMO3 switch to MIMO2\n");

			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;
			if (tbl->action == IWL_MIMO3_SWITCH_MIMO2_AB)
				search_tbl->ant_type = ANT_AB;
			else if (tbl->action == IWL_MIMO3_SWITCH_MIMO2_AC)
				search_tbl->ant_type = ANT_AC;
			else
				search_tbl->ant_type = ANT_BC;

			if (!rs_is_valid_ant(valid_tx_ant, search_tbl->ant_type))
				break;

			ret = rs_switch_to_mimo2(priv, lq_sta, conf, sta,
						 search_tbl, index);
			if (!ret)
				goto out;

			break;

		case IWL_MIMO3_SWITCH_GI:
			if (!tbl->is_ht40 && !(ht_cap->cap &
						IEEE80211_HT_CAP_SGI_20))
				break;
			if (tbl->is_ht40 && !(ht_cap->cap &
						IEEE80211_HT_CAP_SGI_40))
				break;

			IWL_DEBUG_RATE(priv, "LQ: MIMO3 toggle SGI/NGI\n");

			
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = !tbl->is_SGI;
			rs_set_expected_tpt_table(lq_sta, search_tbl);
			if (tbl->is_SGI) {
				s32 tpt = lq_sta->last_tpt / 100;
				if (tpt >= search_tbl->expected_tpt[index])
					break;
			}
			search_tbl->current_rate =
				rate_n_flags_from_tbl(priv, search_tbl,
						      index, is_green);
			update_search_tbl_counter = 1;
			goto out;
		}
		tbl->action++;
		if (tbl->action > IWL_MIMO3_SWITCH_GI)
			tbl->action = IWL_MIMO3_SWITCH_ANTENNA1;

		if (tbl->action == start_action)
			break;
	}
	search_tbl->lq_type = LQ_NONE;
	return 0;
 out:
	lq_sta->search_better_tbl = 1;
	tbl->action++;
	if (tbl->action > IWL_MIMO3_SWITCH_GI)
		tbl->action = IWL_MIMO3_SWITCH_ANTENNA1;
	if (update_search_tbl_counter)
		search_tbl->action = tbl->action;

	return 0;

}

static void rs_stay_in_table(struct iwl_lq_sta *lq_sta, bool force_search)
{
	struct iwl_scale_tbl_info *tbl;
	int i;
	int active_tbl;
	int flush_interval_passed = 0;
	struct iwl_priv *priv;

	priv = lq_sta->drv;
	active_tbl = lq_sta->active_tbl;

	tbl = &(lq_sta->lq_info[active_tbl]);

	
	if (lq_sta->stay_in_tbl) {

		
		if (lq_sta->flush_timer)
			flush_interval_passed =
			time_after(jiffies,
					(unsigned long)(lq_sta->flush_timer +
					IWL_RATE_SCALE_FLUSH_INTVL));

		if (force_search ||
		    (lq_sta->total_failed > lq_sta->max_failure_limit) ||
		    (lq_sta->total_success > lq_sta->max_success_limit) ||
		    ((!lq_sta->search_better_tbl) && (lq_sta->flush_timer)
		     && (flush_interval_passed))) {
			IWL_DEBUG_RATE(priv, "LQ: stay is expired %d %d %d\n:",
				     lq_sta->total_failed,
				     lq_sta->total_success,
				     flush_interval_passed);

			
			lq_sta->stay_in_tbl = 0;	
			lq_sta->total_failed = 0;
			lq_sta->total_success = 0;
			lq_sta->flush_timer = 0;

		} else {
			lq_sta->table_count++;
			if (lq_sta->table_count >=
			    lq_sta->table_count_limit) {
				lq_sta->table_count = 0;

				IWL_DEBUG_RATE(priv, "LQ: stay in table clear win\n");
				for (i = 0; i < IWL_RATE_COUNT; i++)
					rs_rate_scale_clear_window(
						&(tbl->win[i]));
			}
		}

		if (!lq_sta->stay_in_tbl) {
			for (i = 0; i < IWL_RATE_COUNT; i++)
				rs_rate_scale_clear_window(&(tbl->win[i]));
		}
	}
}

static void rs_update_rate_tbl(struct iwl_priv *priv,
			       struct iwl_rxon_context *ctx,
			       struct iwl_lq_sta *lq_sta,
			       struct iwl_scale_tbl_info *tbl,
			       int index, u8 is_green)
{
	u32 rate;

	
	rate = rate_n_flags_from_tbl(priv, tbl, index, is_green);
	rs_fill_link_cmd(priv, lq_sta, rate);
	iwl_send_lq_cmd(priv, ctx, &lq_sta->lq, CMD_ASYNC, false);
}

static void rs_rate_scale_perform(struct iwl_priv *priv,
				  struct sk_buff *skb,
				  struct ieee80211_sta *sta,
				  struct iwl_lq_sta *lq_sta)
{
	struct ieee80211_hw *hw = priv->hw;
	struct ieee80211_conf *conf = &hw->conf;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	int low = IWL_RATE_INVALID;
	int high = IWL_RATE_INVALID;
	int index;
	int i;
	struct iwl_rate_scale_data *window = NULL;
	int current_tpt = IWL_INVALID_VALUE;
	int low_tpt = IWL_INVALID_VALUE;
	int high_tpt = IWL_INVALID_VALUE;
	u32 fail_count;
	s8 scale_action = 0;
	u16 rate_mask;
	u8 update_lq = 0;
	struct iwl_scale_tbl_info *tbl, *tbl1;
	u16 rate_scale_index_msk = 0;
	u8 is_green = 0;
	u8 active_tbl = 0;
	u8 done_search = 0;
	u16 high_low;
	s32 sr;
	u8 tid = IWL_MAX_TID_COUNT;
	struct iwl_tid_data *tid_data;
	struct iwl_station_priv *sta_priv = (void *)sta->drv_priv;
	struct iwl_rxon_context *ctx = sta_priv->ctx;

	IWL_DEBUG_RATE(priv, "rate scale calculate new rate for skb\n");

	
	
	if (!ieee80211_is_data(hdr->frame_control) ||
	    info->flags & IEEE80211_TX_CTL_NO_ACK)
		return;

	lq_sta->supp_rates = sta->supp_rates[lq_sta->band];

	tid = rs_tl_add_packet(lq_sta, hdr);
	if ((tid != IWL_MAX_TID_COUNT) &&
	    (lq_sta->tx_agg_tid_en & (1 << tid))) {
		tid_data = &priv->tid_data[lq_sta->lq.sta_id][tid];
		if (tid_data->agg.state == IWL_AGG_OFF)
			lq_sta->is_agg = 0;
		else
			lq_sta->is_agg = 1;
	} else
		lq_sta->is_agg = 0;

	if (!lq_sta->search_better_tbl)
		active_tbl = lq_sta->active_tbl;
	else
		active_tbl = 1 - lq_sta->active_tbl;

	tbl = &(lq_sta->lq_info[active_tbl]);
	if (is_legacy(tbl->lq_type))
		lq_sta->is_green = 0;
	else
		lq_sta->is_green = rs_use_green(sta);
	is_green = lq_sta->is_green;

	
	index = lq_sta->last_txrate_idx;

	IWL_DEBUG_RATE(priv, "Rate scale index %d for type %d\n", index,
		       tbl->lq_type);

	
	rate_mask = rs_get_supported_rates(lq_sta, hdr, tbl->lq_type);

	IWL_DEBUG_RATE(priv, "mask 0x%04X\n", rate_mask);

	
	if (is_legacy(tbl->lq_type)) {
		if (lq_sta->band == IEEE80211_BAND_5GHZ)
			
			rate_scale_index_msk = (u16) (rate_mask &
				(lq_sta->supp_rates << IWL_FIRST_OFDM_RATE));
		else
			rate_scale_index_msk = (u16) (rate_mask &
						      lq_sta->supp_rates);

	} else
		rate_scale_index_msk = rate_mask;

	if (!rate_scale_index_msk)
		rate_scale_index_msk = rate_mask;

	if (!((1 << index) & rate_scale_index_msk)) {
		IWL_ERR(priv, "Current Rate is not valid\n");
		if (lq_sta->search_better_tbl) {
			
			tbl->lq_type = LQ_NONE;
			lq_sta->search_better_tbl = 0;
			tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
			
			index = iwl_hwrate_to_plcp_idx(tbl->current_rate);
			rs_update_rate_tbl(priv, ctx, lq_sta, tbl,
					   index, is_green);
		}
		return;
	}

	
	if (!tbl->expected_tpt) {
		IWL_ERR(priv, "tbl->expected_tpt is NULL\n");
		return;
	}

	
	if ((lq_sta->max_rate_idx != -1) &&
	    (lq_sta->max_rate_idx < index)) {
		index = lq_sta->max_rate_idx;
		update_lq = 1;
		window = &(tbl->win[index]);
		goto lq_update;
	}

	window = &(tbl->win[index]);

	fail_count = window->counter - window->success_counter;
	if ((fail_count < IWL_RATE_MIN_FAILURE_TH) &&
			(window->success_counter < IWL_RATE_MIN_SUCCESS_TH)) {
		IWL_DEBUG_RATE(priv, "LQ: still below TH. succ=%d total=%d "
			       "for index %d\n",
			       window->success_counter, window->counter, index);

		
		window->average_tpt = IWL_INVALID_VALUE;

		rs_stay_in_table(lq_sta, false);

		goto out;
	}
	if (window->average_tpt != ((window->success_ratio *
			tbl->expected_tpt[index] + 64) / 128)) {
		IWL_ERR(priv, "expected_tpt should have been calculated by now\n");
		window->average_tpt = ((window->success_ratio *
					tbl->expected_tpt[index] + 64) / 128);
	}

	
	if (lq_sta->search_better_tbl &&
	    (iwl_tx_ant_restriction(priv) == IWL_ANT_OK_MULTI)) {
		if (window->average_tpt > lq_sta->last_tpt) {

			IWL_DEBUG_RATE(priv, "LQ: SWITCHING TO NEW TABLE "
					"suc=%d cur-tpt=%d old-tpt=%d\n",
					window->success_ratio,
					window->average_tpt,
					lq_sta->last_tpt);

			if (!is_legacy(tbl->lq_type))
				lq_sta->enable_counter = 1;

			
			lq_sta->active_tbl = active_tbl;
			current_tpt = window->average_tpt;

		
		} else {

			IWL_DEBUG_RATE(priv, "LQ: GOING BACK TO THE OLD TABLE "
					"suc=%d cur-tpt=%d old-tpt=%d\n",
					window->success_ratio,
					window->average_tpt,
					lq_sta->last_tpt);

			
			tbl->lq_type = LQ_NONE;

			
			active_tbl = lq_sta->active_tbl;
			tbl = &(lq_sta->lq_info[active_tbl]);

			
			index = iwl_hwrate_to_plcp_idx(tbl->current_rate);
			current_tpt = lq_sta->last_tpt;

			
			update_lq = 1;
		}

		lq_sta->search_better_tbl = 0;
		done_search = 1;	
		goto lq_update;
	}

	high_low = rs_get_adjacent_rate(priv, index, rate_scale_index_msk,
					tbl->lq_type);
	low = high_low & 0xff;
	high = (high_low >> 8) & 0xff;

	
	if ((lq_sta->max_rate_idx != -1) &&
	    (lq_sta->max_rate_idx < high))
		high = IWL_RATE_INVALID;

	sr = window->success_ratio;

	
	current_tpt = window->average_tpt;
	if (low != IWL_RATE_INVALID)
		low_tpt = tbl->win[low].average_tpt;
	if (high != IWL_RATE_INVALID)
		high_tpt = tbl->win[high].average_tpt;

	scale_action = 0;

	
	if ((sr <= IWL_RATE_DECREASE_TH) || (current_tpt == 0)) {
		IWL_DEBUG_RATE(priv, "decrease rate because of low success_ratio\n");
		scale_action = -1;

	
	} else if ((low_tpt == IWL_INVALID_VALUE) &&
		   (high_tpt == IWL_INVALID_VALUE)) {

		if (high != IWL_RATE_INVALID && sr >= IWL_RATE_INCREASE_TH)
			scale_action = 1;
		else if (low != IWL_RATE_INVALID)
			scale_action = 0;
	}

	else if ((low_tpt != IWL_INVALID_VALUE) &&
		 (high_tpt != IWL_INVALID_VALUE) &&
		 (low_tpt < current_tpt) &&
		 (high_tpt < current_tpt))
		scale_action = 0;

	else {
		
		if (high_tpt != IWL_INVALID_VALUE) {
			
			if (high_tpt > current_tpt &&
					sr >= IWL_RATE_INCREASE_TH) {
				scale_action = 1;
			} else {
				scale_action = 0;
			}

		
		} else if (low_tpt != IWL_INVALID_VALUE) {
			
			if (low_tpt > current_tpt) {
				IWL_DEBUG_RATE(priv,
				    "decrease rate because of low tpt\n");
				scale_action = -1;
			} else if (sr >= IWL_RATE_INCREASE_TH) {
				scale_action = 1;
			}
		}
	}

	if ((scale_action == -1) && (low != IWL_RATE_INVALID) &&
		    ((sr > IWL_RATE_HIGH_TH) ||
		     (current_tpt > (100 * tbl->expected_tpt[low]))))
		scale_action = 0;
	if (!iwl_ht_enabled(priv) && !is_legacy(tbl->lq_type))
		scale_action = -1;
	if (iwl_tx_ant_restriction(priv) != IWL_ANT_OK_MULTI &&
		(is_mimo2(tbl->lq_type) || is_mimo3(tbl->lq_type)))
		scale_action = -1;

	if ((priv->bt_traffic_load >= IWL_BT_COEX_TRAFFIC_LOAD_HIGH) &&
	     (is_mimo2(tbl->lq_type) || is_mimo3(tbl->lq_type))) {
		if (lq_sta->last_bt_traffic > priv->bt_traffic_load) {
		} else if (lq_sta->last_bt_traffic <= priv->bt_traffic_load) {
			scale_action = -1;
		}
	}
	lq_sta->last_bt_traffic = priv->bt_traffic_load;

	if ((priv->bt_traffic_load >= IWL_BT_COEX_TRAFFIC_LOAD_HIGH) &&
	     (is_mimo2(tbl->lq_type) || is_mimo3(tbl->lq_type))) {
		
		rs_stay_in_table(lq_sta, true);
		goto lq_update;
	}

	switch (scale_action) {
	case -1:
		
		if (low != IWL_RATE_INVALID) {
			update_lq = 1;
			index = low;
		}

		break;
	case 1:
		
		if (high != IWL_RATE_INVALID) {
			update_lq = 1;
			index = high;
		}

		break;
	case 0:
		
	default:
		break;
	}

	IWL_DEBUG_RATE(priv, "choose rate scale index %d action %d low %d "
		    "high %d type %d\n",
		     index, scale_action, low, high, tbl->lq_type);

lq_update:
	
	if (update_lq)
		rs_update_rate_tbl(priv, ctx, lq_sta, tbl, index, is_green);

	if (iwl_tx_ant_restriction(priv) == IWL_ANT_OK_MULTI) {
	  rs_stay_in_table(lq_sta, false);
	}
	if (!update_lq && !done_search && !lq_sta->stay_in_tbl && window->counter) {
		
		lq_sta->last_tpt = current_tpt;

		if (is_legacy(tbl->lq_type))
			rs_move_legacy_other(priv, lq_sta, conf, sta, index);
		else if (is_siso(tbl->lq_type))
			rs_move_siso_to_other(priv, lq_sta, conf, sta, index);
		else if (is_mimo2(tbl->lq_type))
			rs_move_mimo2_to_other(priv, lq_sta, conf, sta, index);
		else
			rs_move_mimo3_to_other(priv, lq_sta, conf, sta, index);

		
		if (lq_sta->search_better_tbl) {
			
			tbl = &(lq_sta->lq_info[(1 - lq_sta->active_tbl)]);
			for (i = 0; i < IWL_RATE_COUNT; i++)
				rs_rate_scale_clear_window(&(tbl->win[i]));

			
			index = iwl_hwrate_to_plcp_idx(tbl->current_rate);

			IWL_DEBUG_RATE(priv, "Switch current  mcs: %X index: %d\n",
				     tbl->current_rate, index);
			rs_fill_link_cmd(priv, lq_sta, tbl->current_rate);
			iwl_send_lq_cmd(priv, ctx, &lq_sta->lq, CMD_ASYNC, false);
		} else
			done_search = 1;
	}

	if (done_search && !lq_sta->stay_in_tbl) {
		tbl1 = &(lq_sta->lq_info[lq_sta->active_tbl]);
		if (is_legacy(tbl1->lq_type) && !conf_is_ht(conf) &&
		    lq_sta->action_counter > tbl1->max_search) {
			IWL_DEBUG_RATE(priv, "LQ: STAY in legacy table\n");
			rs_set_stay_in_table(priv, 1, lq_sta);
		}

		if (lq_sta->enable_counter &&
		    (lq_sta->action_counter >= tbl1->max_search) &&
		    iwl_ht_enabled(priv)) {
			if ((lq_sta->last_tpt > IWL_AGG_TPT_THREHOLD) &&
			    (lq_sta->tx_agg_tid_en & (1 << tid)) &&
			    (tid != IWL_MAX_TID_COUNT)) {
				u8 sta_id = lq_sta->lq.sta_id;
				tid_data = &priv->tid_data[sta_id][tid];
				if (tid_data->agg.state == IWL_AGG_OFF) {
					IWL_DEBUG_RATE(priv,
						       "try to aggregate tid %d\n",
						       tid);
					rs_tl_turn_on_agg(priv, tid,
							  lq_sta, sta);
				}
			}
			rs_set_stay_in_table(priv, 0, lq_sta);
		}
	}

out:
	tbl->current_rate = rate_n_flags_from_tbl(priv, tbl, index, is_green);
	lq_sta->last_txrate_idx = index;
}

static void rs_initialize_lq(struct iwl_priv *priv,
			     struct ieee80211_sta *sta,
			     struct iwl_lq_sta *lq_sta)
{
	struct iwl_scale_tbl_info *tbl;
	int rate_idx;
	int i;
	u32 rate;
	u8 use_green = rs_use_green(sta);
	u8 active_tbl = 0;
	u8 valid_tx_ant;
	struct iwl_station_priv *sta_priv;
	struct iwl_rxon_context *ctx;

	if (!sta || !lq_sta)
		return;

	sta_priv = (void *)sta->drv_priv;
	ctx = sta_priv->ctx;

	i = lq_sta->last_txrate_idx;

	valid_tx_ant = hw_params(priv).valid_tx_ant;

	if (!lq_sta->search_better_tbl)
		active_tbl = lq_sta->active_tbl;
	else
		active_tbl = 1 - lq_sta->active_tbl;

	tbl = &(lq_sta->lq_info[active_tbl]);

	if ((i < 0) || (i >= IWL_RATE_COUNT))
		i = 0;

	rate = iwl_rates[i].plcp;
	tbl->ant_type = first_antenna(valid_tx_ant);
	rate |= tbl->ant_type << RATE_MCS_ANT_POS;

	if (i >= IWL_FIRST_CCK_RATE && i <= IWL_LAST_CCK_RATE)
		rate |= RATE_MCS_CCK_MSK;

	rs_get_tbl_info_from_mcs(rate, priv->band, tbl, &rate_idx);
	if (!rs_is_valid_ant(valid_tx_ant, tbl->ant_type))
	    rs_toggle_antenna(valid_tx_ant, &rate, tbl);

	rate = rate_n_flags_from_tbl(priv, tbl, rate_idx, use_green);
	tbl->current_rate = rate;
	rs_set_expected_tpt_table(lq_sta, tbl);
	rs_fill_link_cmd(NULL, lq_sta, rate);
	priv->stations[lq_sta->lq.sta_id].lq = &lq_sta->lq;
	iwl_send_lq_cmd(priv, ctx, &lq_sta->lq, CMD_SYNC, true);
}

static void rs_get_rate(void *priv_r, struct ieee80211_sta *sta, void *priv_sta,
			struct ieee80211_tx_rate_control *txrc)
{

	struct sk_buff *skb = txrc->skb;
	struct ieee80211_supported_band *sband = txrc->sband;
	struct iwl_op_mode *op_mode __maybe_unused =
			(struct iwl_op_mode *)priv_r;
	struct iwl_priv *priv __maybe_unused = IWL_OP_MODE_GET_DVM(op_mode);
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct iwl_lq_sta *lq_sta = priv_sta;
	int rate_idx;

	IWL_DEBUG_RATE_LIMIT(priv, "rate scale calculate new rate for skb\n");

	
	if (lq_sta) {
		lq_sta->max_rate_idx = txrc->max_rate_idx;
		if ((sband->band == IEEE80211_BAND_5GHZ) &&
		    (lq_sta->max_rate_idx != -1))
			lq_sta->max_rate_idx += IWL_FIRST_OFDM_RATE;
		if ((lq_sta->max_rate_idx < 0) ||
		    (lq_sta->max_rate_idx >= IWL_RATE_COUNT))
			lq_sta->max_rate_idx = -1;
	}

	
	if (lq_sta && !lq_sta->drv) {
		IWL_DEBUG_RATE(priv, "Rate scaling not initialized yet.\n");
		priv_sta = NULL;
	}

	
	if (rate_control_send_low(sta, priv_sta, txrc))
		return;

	rate_idx  = lq_sta->last_txrate_idx;

	if (lq_sta->last_rate_n_flags & RATE_MCS_HT_MSK) {
		rate_idx -= IWL_FIRST_OFDM_RATE;
		
		rate_idx = (rate_idx > 0) ? (rate_idx - 1) : 0;
		if (rs_extract_rate(lq_sta->last_rate_n_flags) >=
		    IWL_RATE_MIMO3_6M_PLCP)
			rate_idx = rate_idx + (2 * MCS_INDEX_PER_STREAM);
		else if (rs_extract_rate(lq_sta->last_rate_n_flags) >=
			 IWL_RATE_MIMO2_6M_PLCP)
			rate_idx = rate_idx + MCS_INDEX_PER_STREAM;
		info->control.rates[0].flags = IEEE80211_TX_RC_MCS;
		if (lq_sta->last_rate_n_flags & RATE_MCS_SGI_MSK)
			info->control.rates[0].flags |= IEEE80211_TX_RC_SHORT_GI;
		if (lq_sta->last_rate_n_flags & RATE_MCS_DUP_MSK)
			info->control.rates[0].flags |= IEEE80211_TX_RC_DUP_DATA;
		if (lq_sta->last_rate_n_flags & RATE_MCS_HT40_MSK)
			info->control.rates[0].flags |= IEEE80211_TX_RC_40_MHZ_WIDTH;
		if (lq_sta->last_rate_n_flags & RATE_MCS_GF_MSK)
			info->control.rates[0].flags |= IEEE80211_TX_RC_GREEN_FIELD;
	} else {
		
		if ((rate_idx < 0) || (rate_idx >= IWL_RATE_COUNT_LEGACY) ||
				((sband->band == IEEE80211_BAND_5GHZ) &&
				 (rate_idx < IWL_FIRST_OFDM_RATE)))
			rate_idx = rate_lowest_index(sband, sta);
		
		else if (sband->band == IEEE80211_BAND_5GHZ)
			rate_idx -= IWL_FIRST_OFDM_RATE;
		info->control.rates[0].flags = 0;
	}
	info->control.rates[0].idx = rate_idx;

}

static void *rs_alloc_sta(void *priv_rate, struct ieee80211_sta *sta,
			  gfp_t gfp)
{
	struct iwl_station_priv *sta_priv = (struct iwl_station_priv *) sta->drv_priv;
	struct iwl_op_mode *op_mode __maybe_unused =
			(struct iwl_op_mode *)priv_rate;
	struct iwl_priv *priv __maybe_unused = IWL_OP_MODE_GET_DVM(op_mode);

	IWL_DEBUG_RATE(priv, "create station rate scale window\n");

	return &sta_priv->lq_sta;
}

void iwl_rs_rate_init(struct iwl_priv *priv, struct ieee80211_sta *sta, u8 sta_id)
{
	int i, j;
	struct ieee80211_hw *hw = priv->hw;
	struct ieee80211_conf *conf = &priv->hw->conf;
	struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
	struct iwl_station_priv *sta_priv;
	struct iwl_lq_sta *lq_sta;
	struct ieee80211_supported_band *sband;

	sta_priv = (struct iwl_station_priv *) sta->drv_priv;
	lq_sta = &sta_priv->lq_sta;
	sband = hw->wiphy->bands[conf->channel->band];


	lq_sta->lq.sta_id = sta_id;

	for (j = 0; j < LQ_SIZE; j++)
		for (i = 0; i < IWL_RATE_COUNT; i++)
			rs_rate_scale_clear_window(&lq_sta->lq_info[j].win[i]);

	lq_sta->flush_timer = 0;
	lq_sta->supp_rates = sta->supp_rates[sband->band];
	for (j = 0; j < LQ_SIZE; j++)
		for (i = 0; i < IWL_RATE_COUNT; i++)
			rs_rate_scale_clear_window(&lq_sta->lq_info[j].win[i]);

	IWL_DEBUG_RATE(priv, "LQ: *** rate scale station global init for station %d ***\n",
		       sta_id);

	lq_sta->is_dup = 0;
	lq_sta->max_rate_idx = -1;
	lq_sta->missed_rate_counter = IWL_MISSED_RATE_MAX;
	lq_sta->is_green = rs_use_green(sta);
	lq_sta->active_legacy_rate = priv->active_rate & ~(0x1000);
	lq_sta->band = priv->band;
	lq_sta->active_siso_rate = ht_cap->mcs.rx_mask[0] << 1;
	lq_sta->active_siso_rate |= ht_cap->mcs.rx_mask[0] & 0x1;
	lq_sta->active_siso_rate &= ~((u16)0x2);
	lq_sta->active_siso_rate <<= IWL_FIRST_OFDM_RATE;

	
	lq_sta->active_mimo2_rate = ht_cap->mcs.rx_mask[1] << 1;
	lq_sta->active_mimo2_rate |= ht_cap->mcs.rx_mask[1] & 0x1;
	lq_sta->active_mimo2_rate &= ~((u16)0x2);
	lq_sta->active_mimo2_rate <<= IWL_FIRST_OFDM_RATE;

	lq_sta->active_mimo3_rate = ht_cap->mcs.rx_mask[2] << 1;
	lq_sta->active_mimo3_rate |= ht_cap->mcs.rx_mask[2] & 0x1;
	lq_sta->active_mimo3_rate &= ~((u16)0x2);
	lq_sta->active_mimo3_rate <<= IWL_FIRST_OFDM_RATE;

	IWL_DEBUG_RATE(priv, "SISO-RATE=%X MIMO2-RATE=%X MIMO3-RATE=%X\n",
		     lq_sta->active_siso_rate,
		     lq_sta->active_mimo2_rate,
		     lq_sta->active_mimo3_rate);

	
	lq_sta->lq.general_params.single_stream_ant_msk =
		first_antenna(hw_params(priv).valid_tx_ant);
	lq_sta->lq.general_params.dual_stream_ant_msk =
		hw_params(priv).valid_tx_ant &
		~first_antenna(hw_params(priv).valid_tx_ant);
	if (!lq_sta->lq.general_params.dual_stream_ant_msk) {
		lq_sta->lq.general_params.dual_stream_ant_msk = ANT_AB;
	} else if (num_of_ant(hw_params(priv).valid_tx_ant) == 2) {
		lq_sta->lq.general_params.dual_stream_ant_msk =
			hw_params(priv).valid_tx_ant;
	}

	
	lq_sta->tx_agg_tid_en = IWL_AGG_ALL_TID;
	lq_sta->drv = priv;

	
	lq_sta->last_txrate_idx = rate_lowest_index(sband, sta);
	if (sband->band == IEEE80211_BAND_5GHZ)
		lq_sta->last_txrate_idx += IWL_FIRST_OFDM_RATE;
	lq_sta->is_agg = 0;
#ifdef CONFIG_IWLWIFI_DEVICE_TESTMODE
	priv->tm_fixed_rate = 0;
#endif
#ifdef CONFIG_MAC80211_DEBUGFS
	lq_sta->dbg_fixed_rate = 0;
#endif

	rs_initialize_lq(priv, sta, lq_sta);
}

static void rs_fill_link_cmd(struct iwl_priv *priv,
			     struct iwl_lq_sta *lq_sta, u32 new_rate)
{
	struct iwl_scale_tbl_info tbl_type;
	int index = 0;
	int rate_idx;
	int repeat_rate = 0;
	u8 ant_toggle_cnt = 0;
	u8 use_ht_possible = 1;
	u8 valid_tx_ant = 0;
	struct iwl_station_priv *sta_priv =
		container_of(lq_sta, struct iwl_station_priv, lq_sta);
	struct iwl_link_quality_cmd *lq_cmd = &lq_sta->lq;

	
	rs_dbgfs_set_mcs(lq_sta, &new_rate, index);

	
	rs_get_tbl_info_from_mcs(new_rate, lq_sta->band,
				  &tbl_type, &rate_idx);

	if (priv && priv->bt_full_concurrent) {
		
		tbl_type.ant_type =
			first_antenna(hw_params(priv).valid_tx_ant);
	}

	
	if (is_legacy(tbl_type.lq_type)) {
		ant_toggle_cnt = 1;
		repeat_rate = IWL_NUMBER_TRY;
	} else {
		repeat_rate = min(IWL_HT_NUMBER_TRY,
				  LINK_QUAL_AGG_DISABLE_START_DEF - 1);
	}

	lq_cmd->general_params.mimo_delimiter =
			is_mimo(tbl_type.lq_type) ? 1 : 0;

	
	lq_cmd->rs_table[index].rate_n_flags = cpu_to_le32(new_rate);

	if (num_of_ant(tbl_type.ant_type) == 1) {
		lq_cmd->general_params.single_stream_ant_msk =
						tbl_type.ant_type;
	} else if (num_of_ant(tbl_type.ant_type) == 2) {
		lq_cmd->general_params.dual_stream_ant_msk =
						tbl_type.ant_type;
	} 

	index++;
	repeat_rate--;
	if (priv) {
		if (priv->bt_full_concurrent)
			valid_tx_ant = ANT_A;
		else
			valid_tx_ant = hw_params(priv).valid_tx_ant;
	}

	
	while (index < LINK_QUAL_MAX_RETRY_NUM) {
		while (repeat_rate > 0 && (index < LINK_QUAL_MAX_RETRY_NUM)) {
			if (is_legacy(tbl_type.lq_type)) {
				if (ant_toggle_cnt < NUM_TRY_BEFORE_ANT_TOGGLE)
					ant_toggle_cnt++;
				else if (priv &&
					 rs_toggle_antenna(valid_tx_ant,
							&new_rate, &tbl_type))
					ant_toggle_cnt = 1;
			}

			
			rs_dbgfs_set_mcs(lq_sta, &new_rate, index);

			
			lq_cmd->rs_table[index].rate_n_flags =
					cpu_to_le32(new_rate);
			repeat_rate--;
			index++;
		}

		rs_get_tbl_info_from_mcs(new_rate, lq_sta->band, &tbl_type,
						&rate_idx);

		if (priv && priv->bt_full_concurrent) {
			
			tbl_type.ant_type =
			    first_antenna(hw_params(priv).valid_tx_ant);
		}

		if (is_mimo(tbl_type.lq_type))
			lq_cmd->general_params.mimo_delimiter = index;

		
		new_rate = rs_get_lower_rate(lq_sta, &tbl_type, rate_idx,
					     use_ht_possible);

		
		if (is_legacy(tbl_type.lq_type)) {
			if (ant_toggle_cnt < NUM_TRY_BEFORE_ANT_TOGGLE)
				ant_toggle_cnt++;
			else if (priv &&
				 rs_toggle_antenna(valid_tx_ant,
						   &new_rate, &tbl_type))
				ant_toggle_cnt = 1;

			repeat_rate = IWL_NUMBER_TRY;
		} else {
			repeat_rate = IWL_HT_NUMBER_TRY;
		}

		use_ht_possible = 0;

		
		rs_dbgfs_set_mcs(lq_sta, &new_rate, index);

		
		lq_cmd->rs_table[index].rate_n_flags = cpu_to_le32(new_rate);

		index++;
		repeat_rate--;
	}

	lq_cmd->agg_params.agg_frame_cnt_limit =
		sta_priv->max_agg_bufsize ?: LINK_QUAL_AGG_FRAME_LIMIT_DEF;
	lq_cmd->agg_params.agg_dis_start_th = LINK_QUAL_AGG_DISABLE_START_DEF;

	lq_cmd->agg_params.agg_time_limit =
		cpu_to_le16(LINK_QUAL_AGG_TIME_LIMIT_DEF);
	if (priv && cfg(priv)->bt_params &&
	    cfg(priv)->bt_params->agg_time_limit &&
	    priv->bt_traffic_load >= IWL_BT_COEX_TRAFFIC_LOAD_HIGH)
		lq_cmd->agg_params.agg_time_limit =
			cpu_to_le16(cfg(priv)->bt_params->agg_time_limit);
}

static void *rs_alloc(struct ieee80211_hw *hw, struct dentry *debugfsdir)
{
	return hw->priv;
}
static void rs_free(void *priv_rate)
{
	return;
}

static void rs_free_sta(void *priv_r, struct ieee80211_sta *sta,
			void *priv_sta)
{
	struct iwl_op_mode *op_mode __maybe_unused = priv_r;
	struct iwl_priv *priv __maybe_unused = IWL_OP_MODE_GET_DVM(op_mode);

	IWL_DEBUG_RATE(priv, "enter\n");
	IWL_DEBUG_RATE(priv, "leave\n");
}

#ifdef CONFIG_MAC80211_DEBUGFS
static void rs_dbgfs_set_mcs(struct iwl_lq_sta *lq_sta,
			     u32 *rate_n_flags, int index)
{
	struct iwl_priv *priv;
	u8 valid_tx_ant;
	u8 ant_sel_tx;

	priv = lq_sta->drv;
	valid_tx_ant = hw_params(priv).valid_tx_ant;
	if (lq_sta->dbg_fixed_rate) {
		ant_sel_tx =
		  ((lq_sta->dbg_fixed_rate & RATE_MCS_ANT_ABC_MSK)
		  >> RATE_MCS_ANT_POS);
		if ((valid_tx_ant & ant_sel_tx) == ant_sel_tx) {
			*rate_n_flags = lq_sta->dbg_fixed_rate;
			IWL_DEBUG_RATE(priv, "Fixed rate ON\n");
		} else {
			lq_sta->dbg_fixed_rate = 0;
			IWL_ERR(priv,
			    "Invalid antenna selection 0x%X, Valid is 0x%X\n",
			    ant_sel_tx, valid_tx_ant);
			IWL_DEBUG_RATE(priv, "Fixed rate OFF\n");
		}
	} else {
		IWL_DEBUG_RATE(priv, "Fixed rate OFF\n");
	}
}

static ssize_t rs_sta_dbgfs_scale_table_write(struct file *file,
			const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct iwl_lq_sta *lq_sta = file->private_data;
	struct iwl_priv *priv;
	char buf[64];
	size_t buf_size;
	u32 parsed_rate;


	priv = lq_sta->drv;
	memset(buf, 0, sizeof(buf));
	buf_size = min(count, sizeof(buf) -  1);
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (sscanf(buf, "%x", &parsed_rate) == 1)
		lq_sta->dbg_fixed_rate = parsed_rate;
	else
		lq_sta->dbg_fixed_rate = 0;

	rs_program_fix_rate(priv, lq_sta);

	return count;
}

static ssize_t rs_sta_dbgfs_scale_table_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char *buff;
	int desc = 0;
	int i = 0;
	int index = 0;
	ssize_t ret;

	struct iwl_lq_sta *lq_sta = file->private_data;
	struct iwl_priv *priv;
	struct iwl_scale_tbl_info *tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);

	priv = lq_sta->drv;
	buff = kmalloc(1024, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	desc += sprintf(buff+desc, "sta_id %d\n", lq_sta->lq.sta_id);
	desc += sprintf(buff+desc, "failed=%d success=%d rate=0%X\n",
			lq_sta->total_failed, lq_sta->total_success,
			lq_sta->active_legacy_rate);
	desc += sprintf(buff+desc, "fixed rate 0x%X\n",
			lq_sta->dbg_fixed_rate);
	desc += sprintf(buff+desc, "valid_tx_ant %s%s%s\n",
	    (hw_params(priv).valid_tx_ant & ANT_A) ? "ANT_A," : "",
	    (hw_params(priv).valid_tx_ant & ANT_B) ? "ANT_B," : "",
	    (hw_params(priv).valid_tx_ant & ANT_C) ? "ANT_C" : "");
	desc += sprintf(buff+desc, "lq type %s\n",
	   (is_legacy(tbl->lq_type)) ? "legacy" : "HT");
	if (is_Ht(tbl->lq_type)) {
		desc += sprintf(buff+desc, " %s",
		   (is_siso(tbl->lq_type)) ? "SISO" :
		   ((is_mimo2(tbl->lq_type)) ? "MIMO2" : "MIMO3"));
		   desc += sprintf(buff+desc, " %s",
		   (tbl->is_ht40) ? "40MHz" : "20MHz");
		   desc += sprintf(buff+desc, " %s %s %s\n", (tbl->is_SGI) ? "SGI" : "",
		   (lq_sta->is_green) ? "GF enabled" : "",
		   (lq_sta->is_agg) ? "AGG on" : "");
	}
	desc += sprintf(buff+desc, "last tx rate=0x%X\n",
		lq_sta->last_rate_n_flags);
	desc += sprintf(buff+desc, "general:"
		"flags=0x%X mimo-d=%d s-ant0x%x d-ant=0x%x\n",
		lq_sta->lq.general_params.flags,
		lq_sta->lq.general_params.mimo_delimiter,
		lq_sta->lq.general_params.single_stream_ant_msk,
		lq_sta->lq.general_params.dual_stream_ant_msk);

	desc += sprintf(buff+desc, "agg:"
			"time_limit=%d dist_start_th=%d frame_cnt_limit=%d\n",
			le16_to_cpu(lq_sta->lq.agg_params.agg_time_limit),
			lq_sta->lq.agg_params.agg_dis_start_th,
			lq_sta->lq.agg_params.agg_frame_cnt_limit);

	desc += sprintf(buff+desc,
			"Start idx [0]=0x%x [1]=0x%x [2]=0x%x [3]=0x%x\n",
			lq_sta->lq.general_params.start_rate_index[0],
			lq_sta->lq.general_params.start_rate_index[1],
			lq_sta->lq.general_params.start_rate_index[2],
			lq_sta->lq.general_params.start_rate_index[3]);

	for (i = 0; i < LINK_QUAL_MAX_RETRY_NUM; i++) {
		index = iwl_hwrate_to_plcp_idx(
			le32_to_cpu(lq_sta->lq.rs_table[i].rate_n_flags));
		if (is_legacy(tbl->lq_type)) {
			desc += sprintf(buff+desc, " rate[%d] 0x%X %smbps\n",
				i, le32_to_cpu(lq_sta->lq.rs_table[i].rate_n_flags),
				iwl_rate_mcs[index].mbps);
		} else {
			desc += sprintf(buff+desc, " rate[%d] 0x%X %smbps (%s)\n",
				i, le32_to_cpu(lq_sta->lq.rs_table[i].rate_n_flags),
				iwl_rate_mcs[index].mbps, iwl_rate_mcs[index].mcs);
		}
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buff, desc);
	kfree(buff);
	return ret;
}

static const struct file_operations rs_sta_dbgfs_scale_table_ops = {
	.write = rs_sta_dbgfs_scale_table_write,
	.read = rs_sta_dbgfs_scale_table_read,
	.open = simple_open,
	.llseek = default_llseek,
};
static ssize_t rs_sta_dbgfs_stats_table_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char *buff;
	int desc = 0;
	int i, j;
	ssize_t ret;

	struct iwl_lq_sta *lq_sta = file->private_data;

	buff = kmalloc(1024, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	for (i = 0; i < LQ_SIZE; i++) {
		desc += sprintf(buff+desc,
				"%s type=%d SGI=%d HT40=%d DUP=%d GF=%d\n"
				"rate=0x%X\n",
				lq_sta->active_tbl == i ? "*" : "x",
				lq_sta->lq_info[i].lq_type,
				lq_sta->lq_info[i].is_SGI,
				lq_sta->lq_info[i].is_ht40,
				lq_sta->lq_info[i].is_dup,
				lq_sta->is_green,
				lq_sta->lq_info[i].current_rate);
		for (j = 0; j < IWL_RATE_COUNT; j++) {
			desc += sprintf(buff+desc,
				"counter=%d success=%d %%=%d\n",
				lq_sta->lq_info[i].win[j].counter,
				lq_sta->lq_info[i].win[j].success_counter,
				lq_sta->lq_info[i].win[j].success_ratio);
		}
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, buff, desc);
	kfree(buff);
	return ret;
}

static const struct file_operations rs_sta_dbgfs_stats_table_ops = {
	.read = rs_sta_dbgfs_stats_table_read,
	.open = simple_open,
	.llseek = default_llseek,
};

static ssize_t rs_sta_dbgfs_rate_scale_data_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct iwl_lq_sta *lq_sta = file->private_data;
	struct iwl_scale_tbl_info *tbl = &lq_sta->lq_info[lq_sta->active_tbl];
	char buff[120];
	int desc = 0;

	if (is_Ht(tbl->lq_type))
		desc += sprintf(buff+desc,
				"Bit Rate= %d Mb/s\n",
				tbl->expected_tpt[lq_sta->last_txrate_idx]);
	else
		desc += sprintf(buff+desc,
				"Bit Rate= %d Mb/s\n",
				iwl_rates[lq_sta->last_txrate_idx].ieee >> 1);

	return simple_read_from_buffer(user_buf, count, ppos, buff, desc);
}

static const struct file_operations rs_sta_dbgfs_rate_scale_data_ops = {
	.read = rs_sta_dbgfs_rate_scale_data_read,
	.open = simple_open,
	.llseek = default_llseek,
};

static void rs_add_debugfs(void *priv, void *priv_sta,
					struct dentry *dir)
{
	struct iwl_lq_sta *lq_sta = priv_sta;
	lq_sta->rs_sta_dbgfs_scale_table_file =
		debugfs_create_file("rate_scale_table", S_IRUSR | S_IWUSR, dir,
				lq_sta, &rs_sta_dbgfs_scale_table_ops);
	lq_sta->rs_sta_dbgfs_stats_table_file =
		debugfs_create_file("rate_stats_table", S_IRUSR, dir,
			lq_sta, &rs_sta_dbgfs_stats_table_ops);
	lq_sta->rs_sta_dbgfs_rate_scale_data_file =
		debugfs_create_file("rate_scale_data", S_IRUSR, dir,
			lq_sta, &rs_sta_dbgfs_rate_scale_data_ops);
	lq_sta->rs_sta_dbgfs_tx_agg_tid_en_file =
		debugfs_create_u8("tx_agg_tid_enable", S_IRUSR | S_IWUSR, dir,
		&lq_sta->tx_agg_tid_en);

}

static void rs_remove_debugfs(void *priv, void *priv_sta)
{
	struct iwl_lq_sta *lq_sta = priv_sta;
	debugfs_remove(lq_sta->rs_sta_dbgfs_scale_table_file);
	debugfs_remove(lq_sta->rs_sta_dbgfs_stats_table_file);
	debugfs_remove(lq_sta->rs_sta_dbgfs_rate_scale_data_file);
	debugfs_remove(lq_sta->rs_sta_dbgfs_tx_agg_tid_en_file);
}
#endif

static void rs_rate_init_stub(void *priv_r, struct ieee80211_supported_band *sband,
			 struct ieee80211_sta *sta, void *priv_sta)
{
}
static struct rate_control_ops rs_ops = {
	.module = NULL,
	.name = RS_NAME,
	.tx_status = rs_tx_status,
	.get_rate = rs_get_rate,
	.rate_init = rs_rate_init_stub,
	.alloc = rs_alloc,
	.free = rs_free,
	.alloc_sta = rs_alloc_sta,
	.free_sta = rs_free_sta,
#ifdef CONFIG_MAC80211_DEBUGFS
	.add_sta_debugfs = rs_add_debugfs,
	.remove_sta_debugfs = rs_remove_debugfs,
#endif
};

int iwlagn_rate_control_register(void)
{
	return ieee80211_rate_control_register(&rs_ops);
}

void iwlagn_rate_control_unregister(void)
{
	ieee80211_rate_control_unregister(&rs_ops);
}

