/******************************************************************************
 *
 * Copyright(c) 2005 - 2011 Intel Corporation. All rights reserved.
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

#include "common.h"
#include "4965.h"

#define IL4965_RS_NAME "iwl-4965-rs"

#define NUM_TRY_BEFORE_ANT_TOGGLE 1
#define IL_NUMBER_TRY      1
#define IL_HT_NUMBER_TRY   3

#define RATE_MAX_WINDOW		62	
#define RATE_MIN_FAILURE_TH		6	
#define RATE_MIN_SUCCESS_TH		8	

#define IL_MISSED_RATE_MAX		15
#define RATE_SCALE_FLUSH_INTVL   (3*HZ)

static u8 rs_ht_to_legacy[] = {
	RATE_6M_IDX, RATE_6M_IDX,
	RATE_6M_IDX, RATE_6M_IDX,
	RATE_6M_IDX,
	RATE_6M_IDX, RATE_9M_IDX,
	RATE_12M_IDX, RATE_18M_IDX,
	RATE_24M_IDX, RATE_36M_IDX,
	RATE_48M_IDX, RATE_54M_IDX
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

#define IL_DECLARE_RATE_INFO(r, s, ip, in, rp, rn, pp, np)    \
	[RATE_##r##M_IDX] = { RATE_##r##M_PLCP,      \
				    RATE_SISO_##s##M_PLCP, \
				    RATE_MIMO2_##s##M_PLCP,\
				    RATE_##r##M_IEEE,      \
				    RATE_##ip##M_IDX,    \
				    RATE_##in##M_IDX,    \
				    RATE_##rp##M_IDX,    \
				    RATE_##rn##M_IDX,    \
				    RATE_##pp##M_IDX,    \
				    RATE_##np##M_IDX }

const struct il_rate_info il_rates[RATE_COUNT] = {
	IL_DECLARE_RATE_INFO(1, INV, INV, 2, INV, 2, INV, 2),	
	IL_DECLARE_RATE_INFO(2, INV, 1, 5, 1, 5, 1, 5),		
	IL_DECLARE_RATE_INFO(5, INV, 2, 6, 2, 11, 2, 11),	
	IL_DECLARE_RATE_INFO(11, INV, 9, 12, 9, 12, 5, 18),	
	IL_DECLARE_RATE_INFO(6, 6, 5, 9, 5, 11, 5, 11),		
	IL_DECLARE_RATE_INFO(9, 6, 6, 11, 6, 11, 5, 11),	
	IL_DECLARE_RATE_INFO(12, 12, 11, 18, 11, 18, 11, 18),	
	IL_DECLARE_RATE_INFO(18, 18, 12, 24, 12, 24, 11, 24),	
	IL_DECLARE_RATE_INFO(24, 24, 18, 36, 18, 36, 18, 36),	
	IL_DECLARE_RATE_INFO(36, 36, 24, 48, 24, 48, 24, 48),	
	IL_DECLARE_RATE_INFO(48, 48, 36, 54, 36, 54, 36, 54),	
	IL_DECLARE_RATE_INFO(54, 54, 48, INV, 48, INV, 48, INV),
	IL_DECLARE_RATE_INFO(60, 60, 48, INV, 48, INV, 48, INV),
};

static int
il4965_hwrate_to_plcp_idx(u32 rate_n_flags)
{
	int idx = 0;

	
	if (rate_n_flags & RATE_MCS_HT_MSK) {
		idx = (rate_n_flags & 0xff);

		if (idx >= RATE_MIMO2_6M_PLCP)
			idx = idx - RATE_MIMO2_6M_PLCP;

		idx += IL_FIRST_OFDM_RATE;
		
		if (idx >= RATE_9M_IDX)
			idx += 1;
		if (idx >= IL_FIRST_OFDM_RATE && idx <= IL_LAST_OFDM_RATE)
			return idx;

		
	} else {
		for (idx = 0; idx < ARRAY_SIZE(il_rates); idx++)
			if (il_rates[idx].plcp == (rate_n_flags & 0xFF))
				return idx;
	}

	return -1;
}

static void il4965_rs_rate_scale_perform(struct il_priv *il,
					 struct sk_buff *skb,
					 struct ieee80211_sta *sta,
					 struct il_lq_sta *lq_sta);
static void il4965_rs_fill_link_cmd(struct il_priv *il,
				    struct il_lq_sta *lq_sta, u32 rate_n_flags);
static void il4965_rs_stay_in_table(struct il_lq_sta *lq_sta,
				    bool force_search);

#ifdef CONFIG_MAC80211_DEBUGFS
static void il4965_rs_dbgfs_set_mcs(struct il_lq_sta *lq_sta,
				    u32 *rate_n_flags, int idx);
#else
static void
il4965_rs_dbgfs_set_mcs(struct il_lq_sta *lq_sta, u32 * rate_n_flags, int idx)
{
}
#endif


static s32 expected_tpt_legacy[RATE_COUNT] = {
	7, 13, 35, 58, 40, 57, 72, 98, 121, 154, 177, 186, 0
};

static s32 expected_tpt_siso20MHz[4][RATE_COUNT] = {
	{0, 0, 0, 0, 42, 0, 76, 102, 124, 158, 183, 193, 202},	
	{0, 0, 0, 0, 46, 0, 82, 110, 132, 167, 192, 202, 210},	
	{0, 0, 0, 0, 48, 0, 93, 135, 176, 251, 319, 351, 381},	
	{0, 0, 0, 0, 53, 0, 102, 149, 193, 275, 348, 381, 413},	
};

static s32 expected_tpt_siso40MHz[4][RATE_COUNT] = {
	{0, 0, 0, 0, 77, 0, 127, 160, 184, 220, 242, 250, 257},	
	{0, 0, 0, 0, 83, 0, 135, 169, 193, 229, 250, 257, 264},	
	{0, 0, 0, 0, 96, 0, 182, 259, 328, 451, 553, 598, 640},	
	{0, 0, 0, 0, 106, 0, 199, 282, 357, 487, 593, 640, 683},	
};

static s32 expected_tpt_mimo2_20MHz[4][RATE_COUNT] = {
	{0, 0, 0, 0, 74, 0, 123, 155, 179, 213, 235, 243, 250},	
	{0, 0, 0, 0, 81, 0, 131, 164, 187, 221, 242, 250, 256},	
	{0, 0, 0, 0, 92, 0, 175, 250, 317, 436, 534, 578, 619},	
	{0, 0, 0, 0, 102, 0, 192, 273, 344, 470, 573, 619, 660},	
};

static s32 expected_tpt_mimo2_40MHz[4][RATE_COUNT] = {
	{0, 0, 0, 0, 123, 0, 182, 214, 235, 264, 279, 285, 289},	
	{0, 0, 0, 0, 131, 0, 191, 222, 242, 270, 284, 289, 293},	
	{0, 0, 0, 0, 180, 0, 327, 446, 545, 708, 828, 878, 922},	
	{0, 0, 0, 0, 197, 0, 355, 481, 584, 752, 872, 922, 966},	
};

static const struct il_rate_mcs_info il_rate_mcs[RATE_COUNT] = {
	{"1", "BPSK DSSS"},
	{"2", "QPSK DSSS"},
	{"5.5", "BPSK CCK"},
	{"11", "QPSK CCK"},
	{"6", "BPSK 1/2"},
	{"9", "BPSK 1/2"},
	{"12", "QPSK 1/2"},
	{"18", "QPSK 3/4"},
	{"24", "16QAM 1/2"},
	{"36", "16QAM 3/4"},
	{"48", "64QAM 2/3"},
	{"54", "64QAM 3/4"},
	{"60", "64QAM 5/6"},
};

#define MCS_IDX_PER_STREAM	(8)

static inline u8
il4965_rs_extract_rate(u32 rate_n_flags)
{
	return (u8) (rate_n_flags & 0xFF);
}

static void
il4965_rs_rate_scale_clear_win(struct il_rate_scale_data *win)
{
	win->data = 0;
	win->success_counter = 0;
	win->success_ratio = IL_INVALID_VALUE;
	win->counter = 0;
	win->average_tpt = IL_INVALID_VALUE;
	win->stamp = 0;
}

static inline u8
il4965_rs_is_valid_ant(u8 valid_antenna, u8 ant_type)
{
	return (ant_type & valid_antenna) == ant_type;
}

static void
il4965_rs_tl_rm_old_stats(struct il_traffic_load *tl, u32 curr_time)
{
	
	u32 oldest_time = curr_time - TID_MAX_TIME_DIFF;

	while (tl->queue_count && tl->time_stamp < oldest_time) {
		tl->total -= tl->packet_count[tl->head];
		tl->packet_count[tl->head] = 0;
		tl->time_stamp += TID_QUEUE_CELL_SPACING;
		tl->queue_count--;
		tl->head++;
		if (tl->head >= TID_QUEUE_MAX_SIZE)
			tl->head = 0;
	}
}

static u8
il4965_rs_tl_add_packet(struct il_lq_sta *lq_data, struct ieee80211_hdr *hdr)
{
	u32 curr_time = jiffies_to_msecs(jiffies);
	u32 time_diff;
	s32 idx;
	struct il_traffic_load *tl = NULL;
	u8 tid;

	if (ieee80211_is_data_qos(hdr->frame_control)) {
		u8 *qc = ieee80211_get_qos_ctl(hdr);
		tid = qc[0] & 0xf;
	} else
		return MAX_TID_COUNT;

	if (unlikely(tid >= TID_MAX_LOAD_COUNT))
		return MAX_TID_COUNT;

	tl = &lq_data->load[tid];

	curr_time -= curr_time % TID_ROUND_VALUE;

	
	if (!(tl->queue_count)) {
		tl->total = 1;
		tl->time_stamp = curr_time;
		tl->queue_count = 1;
		tl->head = 0;
		tl->packet_count[0] = 1;
		return MAX_TID_COUNT;
	}

	time_diff = TIME_WRAP_AROUND(tl->time_stamp, curr_time);
	idx = time_diff / TID_QUEUE_CELL_SPACING;

	
	
	if (idx >= TID_QUEUE_MAX_SIZE)
		il4965_rs_tl_rm_old_stats(tl, curr_time);

	idx = (tl->head + idx) % TID_QUEUE_MAX_SIZE;
	tl->packet_count[idx] = tl->packet_count[idx] + 1;
	tl->total = tl->total + 1;

	if ((idx + 1) > tl->queue_count)
		tl->queue_count = idx + 1;

	return tid;
}

static u32
il4965_rs_tl_get_load(struct il_lq_sta *lq_data, u8 tid)
{
	u32 curr_time = jiffies_to_msecs(jiffies);
	u32 time_diff;
	s32 idx;
	struct il_traffic_load *tl = NULL;

	if (tid >= TID_MAX_LOAD_COUNT)
		return 0;

	tl = &(lq_data->load[tid]);

	curr_time -= curr_time % TID_ROUND_VALUE;

	if (!(tl->queue_count))
		return 0;

	time_diff = TIME_WRAP_AROUND(tl->time_stamp, curr_time);
	idx = time_diff / TID_QUEUE_CELL_SPACING;

	
	
	if (idx >= TID_QUEUE_MAX_SIZE)
		il4965_rs_tl_rm_old_stats(tl, curr_time);

	return tl->total;
}

static int
il4965_rs_tl_turn_on_agg_for_tid(struct il_priv *il, struct il_lq_sta *lq_data,
				 u8 tid, struct ieee80211_sta *sta)
{
	int ret = -EAGAIN;
	u32 load;

	load = il4965_rs_tl_get_load(lq_data, tid);

	if (load > IL_AGG_LOAD_THRESHOLD) {
		D_HT("Starting Tx agg: STA: %pM tid: %d\n", sta->addr, tid);
		ret = ieee80211_start_tx_ba_session(sta, tid, 5000);
		if (ret == -EAGAIN) {
			IL_ERR("Fail start Tx agg on tid: %d\n", tid);
			ieee80211_stop_tx_ba_session(sta, tid);
		}
	} else
		D_HT("Aggregation not enabled for tid %d because load = %u\n",
		     tid, load);

	return ret;
}

static void
il4965_rs_tl_turn_on_agg(struct il_priv *il, u8 tid, struct il_lq_sta *lq_data,
			 struct ieee80211_sta *sta)
{
	if (tid < TID_MAX_LOAD_COUNT)
		il4965_rs_tl_turn_on_agg_for_tid(il, lq_data, tid, sta);
	else
		IL_ERR("tid exceeds max load count: %d/%d\n", tid,
		       TID_MAX_LOAD_COUNT);
}

static inline int
il4965_get_il4965_num_of_ant_from_rate(u32 rate_n_flags)
{
	return !!(rate_n_flags & RATE_MCS_ANT_A_MSK) +
	    !!(rate_n_flags & RATE_MCS_ANT_B_MSK) +
	    !!(rate_n_flags & RATE_MCS_ANT_C_MSK);
}

static s32
il4965_get_expected_tpt(struct il_scale_tbl_info *tbl, int rs_idx)
{
	if (tbl->expected_tpt)
		return tbl->expected_tpt[rs_idx];
	return 0;
}

static int
il4965_rs_collect_tx_data(struct il_scale_tbl_info *tbl, int scale_idx,
			  int attempts, int successes)
{
	struct il_rate_scale_data *win = NULL;
	static const u64 mask = (((u64) 1) << (RATE_MAX_WINDOW - 1));
	s32 fail_count, tpt;

	if (scale_idx < 0 || scale_idx >= RATE_COUNT)
		return -EINVAL;

	
	win = &(tbl->win[scale_idx]);

	
	tpt = il4965_get_expected_tpt(tbl, scale_idx);

	while (attempts > 0) {
		if (win->counter >= RATE_MAX_WINDOW) {

			
			win->counter = RATE_MAX_WINDOW - 1;

			if (win->data & mask) {
				win->data &= ~mask;
				win->success_counter--;
			}
		}

		
		win->counter++;

		
		win->data <<= 1;

		
		if (successes > 0) {
			win->success_counter++;
			win->data |= 0x1;
			successes--;
		}

		attempts--;
	}

	
	if (win->counter > 0)
		win->success_ratio =
		    128 * (100 * win->success_counter) / win->counter;
	else
		win->success_ratio = IL_INVALID_VALUE;

	fail_count = win->counter - win->success_counter;

	
	if (fail_count >= RATE_MIN_FAILURE_TH ||
	    win->success_counter >= RATE_MIN_SUCCESS_TH)
		win->average_tpt = (win->success_ratio * tpt + 64) / 128;
	else
		win->average_tpt = IL_INVALID_VALUE;

	
	win->stamp = jiffies;

	return 0;
}

static u32
il4965_rate_n_flags_from_tbl(struct il_priv *il, struct il_scale_tbl_info *tbl,
			     int idx, u8 use_green)
{
	u32 rate_n_flags = 0;

	if (is_legacy(tbl->lq_type)) {
		rate_n_flags = il_rates[idx].plcp;
		if (idx >= IL_FIRST_CCK_RATE && idx <= IL_LAST_CCK_RATE)
			rate_n_flags |= RATE_MCS_CCK_MSK;

	} else if (is_Ht(tbl->lq_type)) {
		if (idx > IL_LAST_OFDM_RATE) {
			IL_ERR("Invalid HT rate idx %d\n", idx);
			idx = IL_LAST_OFDM_RATE;
		}
		rate_n_flags = RATE_MCS_HT_MSK;

		if (is_siso(tbl->lq_type))
			rate_n_flags |= il_rates[idx].plcp_siso;
		else
			rate_n_flags |= il_rates[idx].plcp_mimo2;
	} else {
		IL_ERR("Invalid tbl->lq_type %d\n", tbl->lq_type);
	}

	rate_n_flags |=
	    ((tbl->ant_type << RATE_MCS_ANT_POS) & RATE_MCS_ANT_ABC_MSK);

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
				IL_ERR("GF was set with SGI:SISO\n");
			}
		}
	}
	return rate_n_flags;
}

static int
il4965_rs_get_tbl_info_from_mcs(const u32 rate_n_flags,
				enum ieee80211_band band,
				struct il_scale_tbl_info *tbl, int *rate_idx)
{
	u32 ant_msk = (rate_n_flags & RATE_MCS_ANT_ABC_MSK);
	u8 il4965_num_of_ant =
	    il4965_get_il4965_num_of_ant_from_rate(rate_n_flags);
	u8 mcs;

	memset(tbl, 0, sizeof(struct il_scale_tbl_info));
	*rate_idx = il4965_hwrate_to_plcp_idx(rate_n_flags);

	if (*rate_idx == RATE_INVALID) {
		*rate_idx = -1;
		return -EINVAL;
	}
	tbl->is_SGI = 0;	
	tbl->is_ht40 = 0;
	tbl->is_dup = 0;
	tbl->ant_type = (ant_msk >> RATE_MCS_ANT_POS);
	tbl->lq_type = LQ_NONE;
	tbl->max_search = IL_MAX_SEARCH;

	
	if (!(rate_n_flags & RATE_MCS_HT_MSK)) {
		if (il4965_num_of_ant == 1) {
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

		mcs = il4965_rs_extract_rate(rate_n_flags);

		
		if (mcs <= RATE_SISO_60M_PLCP) {
			if (il4965_num_of_ant == 1)
				tbl->lq_type = LQ_SISO;	
			
		} else {
			if (il4965_num_of_ant == 2)
				tbl->lq_type = LQ_MIMO2;
		}
	}
	return 0;
}

static int
il4965_rs_toggle_antenna(u32 valid_ant, u32 *rate_n_flags,
			 struct il_scale_tbl_info *tbl)
{
	u8 new_ant_type;

	if (!tbl->ant_type || tbl->ant_type > ANT_ABC)
		return 0;

	if (!il4965_rs_is_valid_ant(valid_ant, tbl->ant_type))
		return 0;

	new_ant_type = ant_toggle_lookup[tbl->ant_type];

	while (new_ant_type != tbl->ant_type &&
	       !il4965_rs_is_valid_ant(valid_ant, new_ant_type))
		new_ant_type = ant_toggle_lookup[new_ant_type];

	if (new_ant_type == tbl->ant_type)
		return 0;

	tbl->ant_type = new_ant_type;
	*rate_n_flags &= ~RATE_MCS_ANT_ABC_MSK;
	*rate_n_flags |= new_ant_type << RATE_MCS_ANT_POS;
	return 1;
}

static bool
il4965_rs_use_green(struct il_priv *il, struct ieee80211_sta *sta)
{
	return (sta->ht_cap.cap & IEEE80211_HT_CAP_GRN_FLD) &&
	       !il->ht.non_gf_sta_present;
}

static u16
il4965_rs_get_supported_rates(struct il_lq_sta *lq_sta,
			      struct ieee80211_hdr *hdr,
			      enum il_table_type rate_type)
{
	if (is_legacy(rate_type)) {
		return lq_sta->active_legacy_rate;
	} else {
		if (is_siso(rate_type))
			return lq_sta->active_siso_rate;
		else
			return lq_sta->active_mimo2_rate;
	}
}

static u16
il4965_rs_get_adjacent_rate(struct il_priv *il, u8 idx, u16 rate_mask,
			    int rate_type)
{
	u8 high = RATE_INVALID;
	u8 low = RATE_INVALID;

	if (is_a_band(rate_type) || !is_legacy(rate_type)) {
		int i;
		u32 mask;

		
		i = idx - 1;
		for (mask = (1 << i); i >= 0; i--, mask >>= 1) {
			if (rate_mask & mask) {
				low = i;
				break;
			}
		}

		
		i = idx + 1;
		for (mask = (1 << i); i < RATE_COUNT; i++, mask <<= 1) {
			if (rate_mask & mask) {
				high = i;
				break;
			}
		}

		return (high << 8) | low;
	}

	low = idx;
	while (low != RATE_INVALID) {
		low = il_rates[low].prev_rs;
		if (low == RATE_INVALID)
			break;
		if (rate_mask & (1 << low))
			break;
		D_RATE("Skipping masked lower rate: %d\n", low);
	}

	high = idx;
	while (high != RATE_INVALID) {
		high = il_rates[high].next_rs;
		if (high == RATE_INVALID)
			break;
		if (rate_mask & (1 << high))
			break;
		D_RATE("Skipping masked higher rate: %d\n", high);
	}

	return (high << 8) | low;
}

static u32
il4965_rs_get_lower_rate(struct il_lq_sta *lq_sta,
			 struct il_scale_tbl_info *tbl, u8 scale_idx,
			 u8 ht_possible)
{
	s32 low;
	u16 rate_mask;
	u16 high_low;
	u8 switch_to_legacy = 0;
	u8 is_green = lq_sta->is_green;
	struct il_priv *il = lq_sta->drv;

	if (!is_legacy(tbl->lq_type) && (!ht_possible || !scale_idx)) {
		switch_to_legacy = 1;
		scale_idx = rs_ht_to_legacy[scale_idx];
		if (lq_sta->band == IEEE80211_BAND_5GHZ)
			tbl->lq_type = LQ_A;
		else
			tbl->lq_type = LQ_G;

		if (il4965_num_of_ant(tbl->ant_type) > 1)
			tbl->ant_type =
			    il4965_first_antenna(il->hw_params.valid_tx_ant);

		tbl->is_ht40 = 0;
		tbl->is_SGI = 0;
		tbl->max_search = IL_MAX_SEARCH;
	}

	rate_mask = il4965_rs_get_supported_rates(lq_sta, NULL, tbl->lq_type);

	
	if (is_legacy(tbl->lq_type)) {
		
		if (lq_sta->band == IEEE80211_BAND_5GHZ)
			rate_mask =
			    (u16) (rate_mask &
				   (lq_sta->supp_rates << IL_FIRST_OFDM_RATE));
		else
			rate_mask = (u16) (rate_mask & lq_sta->supp_rates);
	}

	
	if (switch_to_legacy && (rate_mask & (1 << scale_idx))) {
		low = scale_idx;
		goto out;
	}

	high_low =
	    il4965_rs_get_adjacent_rate(lq_sta->drv, scale_idx, rate_mask,
					tbl->lq_type);
	low = high_low & 0xff;

	if (low == RATE_INVALID)
		low = scale_idx;

out:
	return il4965_rate_n_flags_from_tbl(lq_sta->drv, tbl, low, is_green);
}

static bool
il4965_table_type_matches(struct il_scale_tbl_info *a,
			  struct il_scale_tbl_info *b)
{
	return (a->lq_type == b->lq_type && a->ant_type == b->ant_type &&
		a->is_SGI == b->is_SGI);
}

static void
il4965_rs_tx_status(void *il_r, struct ieee80211_supported_band *sband,
		    struct ieee80211_sta *sta, void *il_sta,
		    struct sk_buff *skb)
{
	int legacy_success;
	int retries;
	int rs_idx, mac_idx, i;
	struct il_lq_sta *lq_sta = il_sta;
	struct il_link_quality_cmd *table;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct il_priv *il = (struct il_priv *)il_r;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	enum mac80211_rate_control_flags mac_flags;
	u32 tx_rate;
	struct il_scale_tbl_info tbl_type;
	struct il_scale_tbl_info *curr_tbl, *other_tbl, *tmp_tbl;

	D_RATE("get frame ack response, update rate scale win\n");

	
	if (!lq_sta) {
		D_RATE("Station rate scaling not created yet.\n");
		return;
	} else if (!lq_sta->drv) {
		D_RATE("Rate scaling not initialized yet.\n");
		return;
	}

	if (!ieee80211_is_data(hdr->frame_control) ||
	    (info->flags & IEEE80211_TX_CTL_NO_ACK))
		return;

	
	if ((info->flags & IEEE80211_TX_CTL_AMPDU) &&
	    !(info->flags & IEEE80211_TX_STAT_AMPDU))
		return;

	table = &lq_sta->lq;
	tx_rate = le32_to_cpu(table->rs_table[0].rate_n_flags);
	il4965_rs_get_tbl_info_from_mcs(tx_rate, il->band, &tbl_type, &rs_idx);
	if (il->band == IEEE80211_BAND_5GHZ)
		rs_idx -= IL_FIRST_OFDM_RATE;
	mac_flags = info->status.rates[0].flags;
	mac_idx = info->status.rates[0].idx;
	
	if (mac_flags & IEEE80211_TX_RC_MCS) {
		mac_idx &= RATE_MCS_CODE_MSK;	
		if (mac_idx >= (RATE_9M_IDX - IL_FIRST_OFDM_RATE))
			mac_idx++;
		if (il->band == IEEE80211_BAND_2GHZ)
			mac_idx += IL_FIRST_OFDM_RATE;
	}
	
	if (mac_idx < 0 ||
	    tbl_type.is_SGI != !!(mac_flags & IEEE80211_TX_RC_SHORT_GI) ||
	    tbl_type.is_ht40 != !!(mac_flags & IEEE80211_TX_RC_40_MHZ_WIDTH) ||
	    tbl_type.is_dup != !!(mac_flags & IEEE80211_TX_RC_DUP_DATA) ||
	    tbl_type.ant_type != info->antenna_sel_tx ||
	    !!(tx_rate & RATE_MCS_HT_MSK) != !!(mac_flags & IEEE80211_TX_RC_MCS)
	    || !!(tx_rate & RATE_MCS_GF_MSK) !=
	    !!(mac_flags & IEEE80211_TX_RC_GREEN_FIELD) || rs_idx != mac_idx) {
		D_RATE("initial rate %d does not match %d (0x%x)\n", mac_idx,
		       rs_idx, tx_rate);
		lq_sta->missed_rate_counter++;
		if (lq_sta->missed_rate_counter > IL_MISSED_RATE_MAX) {
			lq_sta->missed_rate_counter = 0;
			il_send_lq_cmd(il, &lq_sta->lq, CMD_ASYNC, false);
		}
		
		return;
	} else
		
		lq_sta->missed_rate_counter = 0;

	
	if (il4965_table_type_matches
	    (&tbl_type, &(lq_sta->lq_info[lq_sta->active_tbl]))) {
		curr_tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
		other_tbl = &(lq_sta->lq_info[1 - lq_sta->active_tbl]);
	} else
	    if (il4965_table_type_matches
		(&tbl_type, &lq_sta->lq_info[1 - lq_sta->active_tbl])) {
		curr_tbl = &(lq_sta->lq_info[1 - lq_sta->active_tbl]);
		other_tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
	} else {
		D_RATE("Neither active nor search matches tx rate\n");
		tmp_tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
		D_RATE("active- lq:%x, ant:%x, SGI:%d\n", tmp_tbl->lq_type,
		       tmp_tbl->ant_type, tmp_tbl->is_SGI);
		tmp_tbl = &(lq_sta->lq_info[1 - lq_sta->active_tbl]);
		D_RATE("search- lq:%x, ant:%x, SGI:%d\n", tmp_tbl->lq_type,
		       tmp_tbl->ant_type, tmp_tbl->is_SGI);
		D_RATE("actual- lq:%x, ant:%x, SGI:%d\n", tbl_type.lq_type,
		       tbl_type.ant_type, tbl_type.is_SGI);
		il4965_rs_stay_in_table(lq_sta, true);
		goto done;
	}

	if (info->flags & IEEE80211_TX_STAT_AMPDU) {
		tx_rate = le32_to_cpu(table->rs_table[0].rate_n_flags);
		il4965_rs_get_tbl_info_from_mcs(tx_rate, il->band, &tbl_type,
						&rs_idx);
		il4965_rs_collect_tx_data(curr_tbl, rs_idx,
					  info->status.ampdu_len,
					  info->status.ampdu_ack_len);

		
		if (lq_sta->stay_in_tbl) {
			lq_sta->total_success += info->status.ampdu_ack_len;
			lq_sta->total_failed +=
			    (info->status.ampdu_len -
			     info->status.ampdu_ack_len);
		}
	} else {
		retries = info->status.rates[0].count - 1;
		
		retries = min(retries, 15);

		
		legacy_success = !!(info->flags & IEEE80211_TX_STAT_ACK);
		
		for (i = 0; i <= retries; ++i) {
			tx_rate = le32_to_cpu(table->rs_table[i].rate_n_flags);
			il4965_rs_get_tbl_info_from_mcs(tx_rate, il->band,
							&tbl_type, &rs_idx);
			if (il4965_table_type_matches(&tbl_type, curr_tbl))
				tmp_tbl = curr_tbl;
			else if (il4965_table_type_matches
				 (&tbl_type, other_tbl))
				tmp_tbl = other_tbl;
			else
				continue;
			il4965_rs_collect_tx_data(tmp_tbl, rs_idx, 1,
						  i <
						  retries ? 0 : legacy_success);
		}

		
		if (lq_sta->stay_in_tbl) {
			lq_sta->total_success += legacy_success;
			lq_sta->total_failed += retries + (1 - legacy_success);
		}
	}
	
	lq_sta->last_rate_n_flags = tx_rate;
done:
	
	if (sta->supp_rates[sband->band])
		il4965_rs_rate_scale_perform(il, skb, sta, lq_sta);
}

static void
il4965_rs_set_stay_in_table(struct il_priv *il, u8 is_legacy,
			    struct il_lq_sta *lq_sta)
{
	D_RATE("we are staying in the same table\n");
	lq_sta->stay_in_tbl = 1;	
	if (is_legacy) {
		lq_sta->table_count_limit = IL_LEGACY_TBL_COUNT;
		lq_sta->max_failure_limit = IL_LEGACY_FAILURE_LIMIT;
		lq_sta->max_success_limit = IL_LEGACY_SUCCESS_LIMIT;
	} else {
		lq_sta->table_count_limit = IL_NONE_LEGACY_TBL_COUNT;
		lq_sta->max_failure_limit = IL_NONE_LEGACY_FAILURE_LIMIT;
		lq_sta->max_success_limit = IL_NONE_LEGACY_SUCCESS_LIMIT;
	}
	lq_sta->table_count = 0;
	lq_sta->total_failed = 0;
	lq_sta->total_success = 0;
	lq_sta->flush_timer = jiffies;
	lq_sta->action_counter = 0;
}

static void
il4965_rs_set_expected_tpt_table(struct il_lq_sta *lq_sta,
				 struct il_scale_tbl_info *tbl)
{
	
	s32(*ht_tbl_pointer)[RATE_COUNT];

	
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
	else			
		ht_tbl_pointer = expected_tpt_mimo2_40MHz;

	if (!tbl->is_SGI && !lq_sta->is_agg)	
		tbl->expected_tpt = ht_tbl_pointer[0];
	else if (tbl->is_SGI && !lq_sta->is_agg)	
		tbl->expected_tpt = ht_tbl_pointer[1];
	else if (!tbl->is_SGI && lq_sta->is_agg)	
		tbl->expected_tpt = ht_tbl_pointer[2];
	else			
		tbl->expected_tpt = ht_tbl_pointer[3];
}

static s32
il4965_rs_get_best_rate(struct il_priv *il, struct il_lq_sta *lq_sta,
			struct il_scale_tbl_info *tbl,	
			u16 rate_mask, s8 idx)
{
	
	struct il_scale_tbl_info *active_tbl =
	    &(lq_sta->lq_info[lq_sta->active_tbl]);
	s32 active_sr = active_tbl->win[idx].success_ratio;
	s32 active_tpt = active_tbl->expected_tpt[idx];

	
	s32 *tpt_tbl = tbl->expected_tpt;

	s32 new_rate, high, low, start_hi;
	u16 high_low;
	s8 rate = idx;

	new_rate = high = low = start_hi = RATE_INVALID;

	for (;;) {
		high_low =
		    il4965_rs_get_adjacent_rate(il, rate, rate_mask,
						tbl->lq_type);

		low = high_low & 0xff;
		high = (high_low >> 8) & 0xff;

		if ((100 * tpt_tbl[rate] > lq_sta->last_tpt &&
		     (active_sr > RATE_DECREASE_TH && active_sr <= RATE_HIGH_TH
		      && tpt_tbl[rate] <= active_tpt)) ||
		    (active_sr >= RATE_SCALE_SWITCH &&
		     tpt_tbl[rate] > active_tpt)) {

			if (start_hi != RATE_INVALID) {
				new_rate = start_hi;
				break;
			}

			new_rate = rate;

			
			if (low != RATE_INVALID)
				rate = low;

			
			else
				break;

			
		} else {
			if (new_rate != RATE_INVALID)
				break;

			
			else if (high != RATE_INVALID) {
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

static int
il4965_rs_switch_to_mimo2(struct il_priv *il, struct il_lq_sta *lq_sta,
			  struct ieee80211_conf *conf,
			  struct ieee80211_sta *sta,
			  struct il_scale_tbl_info *tbl, int idx)
{
	u16 rate_mask;
	s32 rate;
	s8 is_green = lq_sta->is_green;

	if (!conf_is_ht(conf) || !sta->ht_cap.ht_supported)
		return -1;

	if (((sta->ht_cap.cap & IEEE80211_HT_CAP_SM_PS) >> 2) ==
	    WLAN_HT_CAP_SM_PS_STATIC)
		return -1;

	
	if (il->hw_params.tx_chains_num < 2)
		return -1;

	D_RATE("LQ: try to switch to MIMO2\n");

	tbl->lq_type = LQ_MIMO2;
	tbl->is_dup = lq_sta->is_dup;
	tbl->action = 0;
	tbl->max_search = IL_MAX_SEARCH;
	rate_mask = lq_sta->active_mimo2_rate;

	if (il_is_ht40_tx_allowed(il, &sta->ht_cap))
		tbl->is_ht40 = 1;
	else
		tbl->is_ht40 = 0;

	il4965_rs_set_expected_tpt_table(lq_sta, tbl);

	rate = il4965_rs_get_best_rate(il, lq_sta, tbl, rate_mask, idx);

	D_RATE("LQ: MIMO2 best rate %d mask %X\n", rate, rate_mask);
	if (rate == RATE_INVALID || !((1 << rate) & rate_mask)) {
		D_RATE("Can't switch with idx %d rate mask %x\n", rate,
		       rate_mask);
		return -1;
	}
	tbl->current_rate =
	    il4965_rate_n_flags_from_tbl(il, tbl, rate, is_green);

	D_RATE("LQ: Switch to new mcs %X idx is green %X\n", tbl->current_rate,
	       is_green);
	return 0;
}

static int
il4965_rs_switch_to_siso(struct il_priv *il, struct il_lq_sta *lq_sta,
			 struct ieee80211_conf *conf, struct ieee80211_sta *sta,
			 struct il_scale_tbl_info *tbl, int idx)
{
	u16 rate_mask;
	u8 is_green = lq_sta->is_green;
	s32 rate;

	if (!conf_is_ht(conf) || !sta->ht_cap.ht_supported)
		return -1;

	D_RATE("LQ: try to switch to SISO\n");

	tbl->is_dup = lq_sta->is_dup;
	tbl->lq_type = LQ_SISO;
	tbl->action = 0;
	tbl->max_search = IL_MAX_SEARCH;
	rate_mask = lq_sta->active_siso_rate;

	if (il_is_ht40_tx_allowed(il, &sta->ht_cap))
		tbl->is_ht40 = 1;
	else
		tbl->is_ht40 = 0;

	if (is_green)
		tbl->is_SGI = 0;	

	il4965_rs_set_expected_tpt_table(lq_sta, tbl);
	rate = il4965_rs_get_best_rate(il, lq_sta, tbl, rate_mask, idx);

	D_RATE("LQ: get best rate %d mask %X\n", rate, rate_mask);
	if (rate == RATE_INVALID || !((1 << rate) & rate_mask)) {
		D_RATE("can not switch with idx %d rate mask %x\n", rate,
		       rate_mask);
		return -1;
	}
	tbl->current_rate =
	    il4965_rate_n_flags_from_tbl(il, tbl, rate, is_green);
	D_RATE("LQ: Switch to new mcs %X idx is green %X\n", tbl->current_rate,
	       is_green);
	return 0;
}

static int
il4965_rs_move_legacy_other(struct il_priv *il, struct il_lq_sta *lq_sta,
			    struct ieee80211_conf *conf,
			    struct ieee80211_sta *sta, int idx)
{
	struct il_scale_tbl_info *tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
	struct il_scale_tbl_info *search_tbl =
	    &(lq_sta->lq_info[(1 - lq_sta->active_tbl)]);
	struct il_rate_scale_data *win = &(tbl->win[idx]);
	u32 sz =
	    (sizeof(struct il_scale_tbl_info) -
	     (sizeof(struct il_rate_scale_data) * RATE_COUNT));
	u8 start_action;
	u8 valid_tx_ant = il->hw_params.valid_tx_ant;
	u8 tx_chains_num = il->hw_params.tx_chains_num;
	int ret = 0;
	u8 update_search_tbl_counter = 0;

	tbl->action = IL_LEGACY_SWITCH_SISO;

	start_action = tbl->action;
	for (;;) {
		lq_sta->action_counter++;
		switch (tbl->action) {
		case IL_LEGACY_SWITCH_ANTENNA1:
		case IL_LEGACY_SWITCH_ANTENNA2:
			D_RATE("LQ: Legacy toggle Antenna\n");

			if ((tbl->action == IL_LEGACY_SWITCH_ANTENNA1 &&
			     tx_chains_num <= 1) ||
			    (tbl->action == IL_LEGACY_SWITCH_ANTENNA2 &&
			     tx_chains_num <= 2))
				break;

			
			if (win->success_ratio >= IL_RS_GOOD_RATIO)
				break;

			
			memcpy(search_tbl, tbl, sz);

			if (il4965_rs_toggle_antenna
			    (valid_tx_ant, &search_tbl->current_rate,
			     search_tbl)) {
				update_search_tbl_counter = 1;
				il4965_rs_set_expected_tpt_table(lq_sta,
								 search_tbl);
				goto out;
			}
			break;
		case IL_LEGACY_SWITCH_SISO:
			D_RATE("LQ: Legacy switch to SISO\n");

			
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;
			ret =
			    il4965_rs_switch_to_siso(il, lq_sta, conf, sta,
						     search_tbl, idx);
			if (!ret) {
				lq_sta->action_counter = 0;
				goto out;
			}

			break;
		case IL_LEGACY_SWITCH_MIMO2_AB:
		case IL_LEGACY_SWITCH_MIMO2_AC:
		case IL_LEGACY_SWITCH_MIMO2_BC:
			D_RATE("LQ: Legacy switch to MIMO2\n");

			
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;

			if (tbl->action == IL_LEGACY_SWITCH_MIMO2_AB)
				search_tbl->ant_type = ANT_AB;
			else if (tbl->action == IL_LEGACY_SWITCH_MIMO2_AC)
				search_tbl->ant_type = ANT_AC;
			else
				search_tbl->ant_type = ANT_BC;

			if (!il4965_rs_is_valid_ant
			    (valid_tx_ant, search_tbl->ant_type))
				break;

			ret =
			    il4965_rs_switch_to_mimo2(il, lq_sta, conf, sta,
						      search_tbl, idx);
			if (!ret) {
				lq_sta->action_counter = 0;
				goto out;
			}
			break;
		}
		tbl->action++;
		if (tbl->action > IL_LEGACY_SWITCH_MIMO2_BC)
			tbl->action = IL_LEGACY_SWITCH_ANTENNA1;

		if (tbl->action == start_action)
			break;

	}
	search_tbl->lq_type = LQ_NONE;
	return 0;

out:
	lq_sta->search_better_tbl = 1;
	tbl->action++;
	if (tbl->action > IL_LEGACY_SWITCH_MIMO2_BC)
		tbl->action = IL_LEGACY_SWITCH_ANTENNA1;
	if (update_search_tbl_counter)
		search_tbl->action = tbl->action;
	return 0;

}

static int
il4965_rs_move_siso_to_other(struct il_priv *il, struct il_lq_sta *lq_sta,
			     struct ieee80211_conf *conf,
			     struct ieee80211_sta *sta, int idx)
{
	u8 is_green = lq_sta->is_green;
	struct il_scale_tbl_info *tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
	struct il_scale_tbl_info *search_tbl =
	    &(lq_sta->lq_info[(1 - lq_sta->active_tbl)]);
	struct il_rate_scale_data *win = &(tbl->win[idx]);
	struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
	u32 sz =
	    (sizeof(struct il_scale_tbl_info) -
	     (sizeof(struct il_rate_scale_data) * RATE_COUNT));
	u8 start_action;
	u8 valid_tx_ant = il->hw_params.valid_tx_ant;
	u8 tx_chains_num = il->hw_params.tx_chains_num;
	u8 update_search_tbl_counter = 0;
	int ret;

	start_action = tbl->action;

	for (;;) {
		lq_sta->action_counter++;
		switch (tbl->action) {
		case IL_SISO_SWITCH_ANTENNA1:
		case IL_SISO_SWITCH_ANTENNA2:
			D_RATE("LQ: SISO toggle Antenna\n");
			if ((tbl->action == IL_SISO_SWITCH_ANTENNA1 &&
			     tx_chains_num <= 1) ||
			    (tbl->action == IL_SISO_SWITCH_ANTENNA2 &&
			     tx_chains_num <= 2))
				break;

			if (win->success_ratio >= IL_RS_GOOD_RATIO)
				break;

			memcpy(search_tbl, tbl, sz);
			if (il4965_rs_toggle_antenna
			    (valid_tx_ant, &search_tbl->current_rate,
			     search_tbl)) {
				update_search_tbl_counter = 1;
				goto out;
			}
			break;
		case IL_SISO_SWITCH_MIMO2_AB:
		case IL_SISO_SWITCH_MIMO2_AC:
		case IL_SISO_SWITCH_MIMO2_BC:
			D_RATE("LQ: SISO switch to MIMO2\n");
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = 0;

			if (tbl->action == IL_SISO_SWITCH_MIMO2_AB)
				search_tbl->ant_type = ANT_AB;
			else if (tbl->action == IL_SISO_SWITCH_MIMO2_AC)
				search_tbl->ant_type = ANT_AC;
			else
				search_tbl->ant_type = ANT_BC;

			if (!il4965_rs_is_valid_ant
			    (valid_tx_ant, search_tbl->ant_type))
				break;

			ret =
			    il4965_rs_switch_to_mimo2(il, lq_sta, conf, sta,
						      search_tbl, idx);
			if (!ret)
				goto out;
			break;
		case IL_SISO_SWITCH_GI:
			if (!tbl->is_ht40 &&
			    !(ht_cap->cap & IEEE80211_HT_CAP_SGI_20))
				break;
			if (tbl->is_ht40 &&
			    !(ht_cap->cap & IEEE80211_HT_CAP_SGI_40))
				break;

			D_RATE("LQ: SISO toggle SGI/NGI\n");

			memcpy(search_tbl, tbl, sz);
			if (is_green) {
				if (!tbl->is_SGI)
					break;
				else
					IL_ERR("SGI was set in GF+SISO\n");
			}
			search_tbl->is_SGI = !tbl->is_SGI;
			il4965_rs_set_expected_tpt_table(lq_sta, search_tbl);
			if (tbl->is_SGI) {
				s32 tpt = lq_sta->last_tpt / 100;
				if (tpt >= search_tbl->expected_tpt[idx])
					break;
			}
			search_tbl->current_rate =
			    il4965_rate_n_flags_from_tbl(il, search_tbl, idx,
							 is_green);
			update_search_tbl_counter = 1;
			goto out;
		}
		tbl->action++;
		if (tbl->action > IL_SISO_SWITCH_GI)
			tbl->action = IL_SISO_SWITCH_ANTENNA1;

		if (tbl->action == start_action)
			break;
	}
	search_tbl->lq_type = LQ_NONE;
	return 0;

out:
	lq_sta->search_better_tbl = 1;
	tbl->action++;
	if (tbl->action > IL_SISO_SWITCH_GI)
		tbl->action = IL_SISO_SWITCH_ANTENNA1;
	if (update_search_tbl_counter)
		search_tbl->action = tbl->action;

	return 0;
}

static int
il4965_rs_move_mimo2_to_other(struct il_priv *il, struct il_lq_sta *lq_sta,
			      struct ieee80211_conf *conf,
			      struct ieee80211_sta *sta, int idx)
{
	s8 is_green = lq_sta->is_green;
	struct il_scale_tbl_info *tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
	struct il_scale_tbl_info *search_tbl =
	    &(lq_sta->lq_info[(1 - lq_sta->active_tbl)]);
	struct il_rate_scale_data *win = &(tbl->win[idx]);
	struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
	u32 sz =
	    (sizeof(struct il_scale_tbl_info) -
	     (sizeof(struct il_rate_scale_data) * RATE_COUNT));
	u8 start_action;
	u8 valid_tx_ant = il->hw_params.valid_tx_ant;
	u8 tx_chains_num = il->hw_params.tx_chains_num;
	u8 update_search_tbl_counter = 0;
	int ret;

	start_action = tbl->action;
	for (;;) {
		lq_sta->action_counter++;
		switch (tbl->action) {
		case IL_MIMO2_SWITCH_ANTENNA1:
		case IL_MIMO2_SWITCH_ANTENNA2:
			D_RATE("LQ: MIMO2 toggle Antennas\n");

			if (tx_chains_num <= 2)
				break;

			if (win->success_ratio >= IL_RS_GOOD_RATIO)
				break;

			memcpy(search_tbl, tbl, sz);
			if (il4965_rs_toggle_antenna
			    (valid_tx_ant, &search_tbl->current_rate,
			     search_tbl)) {
				update_search_tbl_counter = 1;
				goto out;
			}
			break;
		case IL_MIMO2_SWITCH_SISO_A:
		case IL_MIMO2_SWITCH_SISO_B:
		case IL_MIMO2_SWITCH_SISO_C:
			D_RATE("LQ: MIMO2 switch to SISO\n");

			
			memcpy(search_tbl, tbl, sz);

			if (tbl->action == IL_MIMO2_SWITCH_SISO_A)
				search_tbl->ant_type = ANT_A;
			else if (tbl->action == IL_MIMO2_SWITCH_SISO_B)
				search_tbl->ant_type = ANT_B;
			else
				search_tbl->ant_type = ANT_C;

			if (!il4965_rs_is_valid_ant
			    (valid_tx_ant, search_tbl->ant_type))
				break;

			ret =
			    il4965_rs_switch_to_siso(il, lq_sta, conf, sta,
						     search_tbl, idx);
			if (!ret)
				goto out;

			break;

		case IL_MIMO2_SWITCH_GI:
			if (!tbl->is_ht40 &&
			    !(ht_cap->cap & IEEE80211_HT_CAP_SGI_20))
				break;
			if (tbl->is_ht40 &&
			    !(ht_cap->cap & IEEE80211_HT_CAP_SGI_40))
				break;

			D_RATE("LQ: MIMO2 toggle SGI/NGI\n");

			
			memcpy(search_tbl, tbl, sz);
			search_tbl->is_SGI = !tbl->is_SGI;
			il4965_rs_set_expected_tpt_table(lq_sta, search_tbl);
			if (tbl->is_SGI) {
				s32 tpt = lq_sta->last_tpt / 100;
				if (tpt >= search_tbl->expected_tpt[idx])
					break;
			}
			search_tbl->current_rate =
			    il4965_rate_n_flags_from_tbl(il, search_tbl, idx,
							 is_green);
			update_search_tbl_counter = 1;
			goto out;

		}
		tbl->action++;
		if (tbl->action > IL_MIMO2_SWITCH_GI)
			tbl->action = IL_MIMO2_SWITCH_ANTENNA1;

		if (tbl->action == start_action)
			break;
	}
	search_tbl->lq_type = LQ_NONE;
	return 0;
out:
	lq_sta->search_better_tbl = 1;
	tbl->action++;
	if (tbl->action > IL_MIMO2_SWITCH_GI)
		tbl->action = IL_MIMO2_SWITCH_ANTENNA1;
	if (update_search_tbl_counter)
		search_tbl->action = tbl->action;

	return 0;

}

static void
il4965_rs_stay_in_table(struct il_lq_sta *lq_sta, bool force_search)
{
	struct il_scale_tbl_info *tbl;
	int i;
	int active_tbl;
	int flush_interval_passed = 0;
	struct il_priv *il;

	il = lq_sta->drv;
	active_tbl = lq_sta->active_tbl;

	tbl = &(lq_sta->lq_info[active_tbl]);

	
	if (lq_sta->stay_in_tbl) {

		
		if (lq_sta->flush_timer)
			flush_interval_passed =
			    time_after(jiffies,
				       (unsigned long)(lq_sta->flush_timer +
						       RATE_SCALE_FLUSH_INTVL));

		if (force_search ||
		    lq_sta->total_failed > lq_sta->max_failure_limit ||
		    lq_sta->total_success > lq_sta->max_success_limit ||
		    (!lq_sta->search_better_tbl && lq_sta->flush_timer &&
		     flush_interval_passed)) {
			D_RATE("LQ: stay is expired %d %d %d\n:",
			       lq_sta->total_failed, lq_sta->total_success,
			       flush_interval_passed);

			
			lq_sta->stay_in_tbl = 0;	
			lq_sta->total_failed = 0;
			lq_sta->total_success = 0;
			lq_sta->flush_timer = 0;

		} else {
			lq_sta->table_count++;
			if (lq_sta->table_count >= lq_sta->table_count_limit) {
				lq_sta->table_count = 0;

				D_RATE("LQ: stay in table clear win\n");
				for (i = 0; i < RATE_COUNT; i++)
					il4965_rs_rate_scale_clear_win(&
								       (tbl->
									win
									[i]));
			}
		}

		if (!lq_sta->stay_in_tbl) {
			for (i = 0; i < RATE_COUNT; i++)
				il4965_rs_rate_scale_clear_win(&(tbl->win[i]));
		}
	}
}

static void
il4965_rs_update_rate_tbl(struct il_priv *il, struct il_lq_sta *lq_sta,
			  struct il_scale_tbl_info *tbl, int idx, u8 is_green)
{
	u32 rate;

	
	rate = il4965_rate_n_flags_from_tbl(il, tbl, idx, is_green);
	il4965_rs_fill_link_cmd(il, lq_sta, rate);
	il_send_lq_cmd(il, &lq_sta->lq, CMD_ASYNC, false);
}

static void
il4965_rs_rate_scale_perform(struct il_priv *il, struct sk_buff *skb,
			     struct ieee80211_sta *sta,
			     struct il_lq_sta *lq_sta)
{
	struct ieee80211_hw *hw = il->hw;
	struct ieee80211_conf *conf = &hw->conf;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	int low = RATE_INVALID;
	int high = RATE_INVALID;
	int idx;
	int i;
	struct il_rate_scale_data *win = NULL;
	int current_tpt = IL_INVALID_VALUE;
	int low_tpt = IL_INVALID_VALUE;
	int high_tpt = IL_INVALID_VALUE;
	u32 fail_count;
	s8 scale_action = 0;
	u16 rate_mask;
	u8 update_lq = 0;
	struct il_scale_tbl_info *tbl, *tbl1;
	u16 rate_scale_idx_msk = 0;
	u8 is_green = 0;
	u8 active_tbl = 0;
	u8 done_search = 0;
	u16 high_low;
	s32 sr;
	u8 tid = MAX_TID_COUNT;
	struct il_tid_data *tid_data;

	D_RATE("rate scale calculate new rate for skb\n");

	
	
	if (!ieee80211_is_data(hdr->frame_control) ||
	    (info->flags & IEEE80211_TX_CTL_NO_ACK))
		return;

	lq_sta->supp_rates = sta->supp_rates[lq_sta->band];

	tid = il4965_rs_tl_add_packet(lq_sta, hdr);
	if (tid != MAX_TID_COUNT && (lq_sta->tx_agg_tid_en & (1 << tid))) {
		tid_data = &il->stations[lq_sta->lq.sta_id].tid[tid];
		if (tid_data->agg.state == IL_AGG_OFF)
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
		lq_sta->is_green = il4965_rs_use_green(il, sta);
	is_green = lq_sta->is_green;

	
	idx = lq_sta->last_txrate_idx;

	D_RATE("Rate scale idx %d for type %d\n", idx, tbl->lq_type);

	
	rate_mask = il4965_rs_get_supported_rates(lq_sta, hdr, tbl->lq_type);

	D_RATE("mask 0x%04X\n", rate_mask);

	
	if (is_legacy(tbl->lq_type)) {
		if (lq_sta->band == IEEE80211_BAND_5GHZ)
			
			rate_scale_idx_msk =
			    (u16) (rate_mask &
				   (lq_sta->supp_rates << IL_FIRST_OFDM_RATE));
		else
			rate_scale_idx_msk =
			    (u16) (rate_mask & lq_sta->supp_rates);

	} else
		rate_scale_idx_msk = rate_mask;

	if (!rate_scale_idx_msk)
		rate_scale_idx_msk = rate_mask;

	if (!((1 << idx) & rate_scale_idx_msk)) {
		IL_ERR("Current Rate is not valid\n");
		if (lq_sta->search_better_tbl) {
			
			tbl->lq_type = LQ_NONE;
			lq_sta->search_better_tbl = 0;
			tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);
			
			idx = il4965_hwrate_to_plcp_idx(tbl->current_rate);
			il4965_rs_update_rate_tbl(il, lq_sta, tbl, idx,
						      is_green);
		}
		return;
	}

	
	if (!tbl->expected_tpt) {
		IL_ERR("tbl->expected_tpt is NULL\n");
		return;
	}

	
	if (lq_sta->max_rate_idx != -1 && lq_sta->max_rate_idx < idx) {
		idx = lq_sta->max_rate_idx;
		update_lq = 1;
		win = &(tbl->win[idx]);
		goto lq_update;
	}

	win = &(tbl->win[idx]);

	fail_count = win->counter - win->success_counter;
	if (fail_count < RATE_MIN_FAILURE_TH &&
	    win->success_counter < RATE_MIN_SUCCESS_TH) {
		D_RATE("LQ: still below TH. succ=%d total=%d " "for idx %d\n",
		       win->success_counter, win->counter, idx);

		
		win->average_tpt = IL_INVALID_VALUE;

		il4965_rs_stay_in_table(lq_sta, false);

		goto out;
	}
	if (win->average_tpt !=
	    ((win->success_ratio * tbl->expected_tpt[idx] + 64) / 128)) {
		IL_ERR("expected_tpt should have been calculated by now\n");
		win->average_tpt =
		    ((win->success_ratio * tbl->expected_tpt[idx] + 64) / 128);
	}

	
	if (lq_sta->search_better_tbl) {
		if (win->average_tpt > lq_sta->last_tpt) {

			D_RATE("LQ: SWITCHING TO NEW TBL "
			       "suc=%d cur-tpt=%d old-tpt=%d\n",
			       win->success_ratio, win->average_tpt,
			       lq_sta->last_tpt);

			if (!is_legacy(tbl->lq_type))
				lq_sta->enable_counter = 1;

			
			lq_sta->active_tbl = active_tbl;
			current_tpt = win->average_tpt;

			
		} else {

			D_RATE("LQ: GOING BACK TO THE OLD TBL "
			       "suc=%d cur-tpt=%d old-tpt=%d\n",
			       win->success_ratio, win->average_tpt,
			       lq_sta->last_tpt);

			
			tbl->lq_type = LQ_NONE;

			
			active_tbl = lq_sta->active_tbl;
			tbl = &(lq_sta->lq_info[active_tbl]);

			
			idx = il4965_hwrate_to_plcp_idx(tbl->current_rate);
			current_tpt = lq_sta->last_tpt;

			
			update_lq = 1;
		}

		lq_sta->search_better_tbl = 0;
		done_search = 1;	
		goto lq_update;
	}

	high_low =
	    il4965_rs_get_adjacent_rate(il, idx, rate_scale_idx_msk,
					tbl->lq_type);
	low = high_low & 0xff;
	high = (high_low >> 8) & 0xff;

	
	if (lq_sta->max_rate_idx != -1 && lq_sta->max_rate_idx < high)
		high = RATE_INVALID;

	sr = win->success_ratio;

	
	current_tpt = win->average_tpt;
	if (low != RATE_INVALID)
		low_tpt = tbl->win[low].average_tpt;
	if (high != RATE_INVALID)
		high_tpt = tbl->win[high].average_tpt;

	scale_action = 0;

	
	if (sr <= RATE_DECREASE_TH || current_tpt == 0) {
		D_RATE("decrease rate because of low success_ratio\n");
		scale_action = -1;

		
	} else if (low_tpt == IL_INVALID_VALUE && high_tpt == IL_INVALID_VALUE) {

		if (high != RATE_INVALID && sr >= RATE_INCREASE_TH)
			scale_action = 1;
		else if (low != RATE_INVALID)
			scale_action = 0;
	}

	else if (low_tpt != IL_INVALID_VALUE && high_tpt != IL_INVALID_VALUE &&
		 low_tpt < current_tpt && high_tpt < current_tpt)
		scale_action = 0;

	else {
		
		if (high_tpt != IL_INVALID_VALUE) {
			
			if (high_tpt > current_tpt && sr >= RATE_INCREASE_TH)
				scale_action = 1;
			else
				scale_action = 0;

			
		} else if (low_tpt != IL_INVALID_VALUE) {
			
			if (low_tpt > current_tpt) {
				D_RATE("decrease rate because of low tpt\n");
				scale_action = -1;
			} else if (sr >= RATE_INCREASE_TH) {
				scale_action = 1;
			}
		}
	}

	if (scale_action == -1 && low != RATE_INVALID &&
	    (sr > RATE_HIGH_TH || current_tpt > 100 * tbl->expected_tpt[low]))
		scale_action = 0;

	switch (scale_action) {
	case -1:
		
		if (low != RATE_INVALID) {
			update_lq = 1;
			idx = low;
		}

		break;
	case 1:
		
		if (high != RATE_INVALID) {
			update_lq = 1;
			idx = high;
		}

		break;
	case 0:
		
	default:
		break;
	}

	D_RATE("choose rate scale idx %d action %d low %d " "high %d type %d\n",
	       idx, scale_action, low, high, tbl->lq_type);

lq_update:
	
	if (update_lq)
		il4965_rs_update_rate_tbl(il, lq_sta, tbl, idx, is_green);

	il4965_rs_stay_in_table(lq_sta, false);

	if (!update_lq && !done_search && !lq_sta->stay_in_tbl && win->counter) {
		
		lq_sta->last_tpt = current_tpt;

		if (is_legacy(tbl->lq_type))
			il4965_rs_move_legacy_other(il, lq_sta, conf, sta, idx);
		else if (is_siso(tbl->lq_type))
			il4965_rs_move_siso_to_other(il, lq_sta, conf, sta,
						     idx);
		else		
			il4965_rs_move_mimo2_to_other(il, lq_sta, conf, sta,
						      idx);

		
		if (lq_sta->search_better_tbl) {
			
			tbl = &(lq_sta->lq_info[(1 - lq_sta->active_tbl)]);
			for (i = 0; i < RATE_COUNT; i++)
				il4965_rs_rate_scale_clear_win(&(tbl->win[i]));

			
			idx = il4965_hwrate_to_plcp_idx(tbl->current_rate);

			D_RATE("Switch current  mcs: %X idx: %d\n",
			       tbl->current_rate, idx);
			il4965_rs_fill_link_cmd(il, lq_sta, tbl->current_rate);
			il_send_lq_cmd(il, &lq_sta->lq, CMD_ASYNC, false);
		} else
			done_search = 1;
	}

	if (done_search && !lq_sta->stay_in_tbl) {
		tbl1 = &(lq_sta->lq_info[lq_sta->active_tbl]);
		if (is_legacy(tbl1->lq_type) && !conf_is_ht(conf) &&
		    lq_sta->action_counter > tbl1->max_search) {
			D_RATE("LQ: STAY in legacy table\n");
			il4965_rs_set_stay_in_table(il, 1, lq_sta);
		}

		if (lq_sta->enable_counter &&
		    lq_sta->action_counter >= tbl1->max_search) {
			if (lq_sta->last_tpt > IL_AGG_TPT_THREHOLD &&
			    (lq_sta->tx_agg_tid_en & (1 << tid)) &&
			    tid != MAX_TID_COUNT) {
				tid_data =
				    &il->stations[lq_sta->lq.sta_id].tid[tid];
				if (tid_data->agg.state == IL_AGG_OFF) {
					D_RATE("try to aggregate tid %d\n",
					       tid);
					il4965_rs_tl_turn_on_agg(il, tid,
								 lq_sta, sta);
				}
			}
			il4965_rs_set_stay_in_table(il, 0, lq_sta);
		}
	}

out:
	tbl->current_rate =
	    il4965_rate_n_flags_from_tbl(il, tbl, idx, is_green);
	i = idx;
	lq_sta->last_txrate_idx = i;
}

static void
il4965_rs_initialize_lq(struct il_priv *il, struct ieee80211_conf *conf,
			struct ieee80211_sta *sta, struct il_lq_sta *lq_sta)
{
	struct il_scale_tbl_info *tbl;
	int rate_idx;
	int i;
	u32 rate;
	u8 use_green = il4965_rs_use_green(il, sta);
	u8 active_tbl = 0;
	u8 valid_tx_ant;
	struct il_station_priv *sta_priv;

	if (!sta || !lq_sta)
		return;

	sta_priv = (void *)sta->drv_priv;

	i = lq_sta->last_txrate_idx;

	valid_tx_ant = il->hw_params.valid_tx_ant;

	if (!lq_sta->search_better_tbl)
		active_tbl = lq_sta->active_tbl;
	else
		active_tbl = 1 - lq_sta->active_tbl;

	tbl = &(lq_sta->lq_info[active_tbl]);

	if (i < 0 || i >= RATE_COUNT)
		i = 0;

	rate = il_rates[i].plcp;
	tbl->ant_type = il4965_first_antenna(valid_tx_ant);
	rate |= tbl->ant_type << RATE_MCS_ANT_POS;

	if (i >= IL_FIRST_CCK_RATE && i <= IL_LAST_CCK_RATE)
		rate |= RATE_MCS_CCK_MSK;

	il4965_rs_get_tbl_info_from_mcs(rate, il->band, tbl, &rate_idx);
	if (!il4965_rs_is_valid_ant(valid_tx_ant, tbl->ant_type))
		il4965_rs_toggle_antenna(valid_tx_ant, &rate, tbl);

	rate = il4965_rate_n_flags_from_tbl(il, tbl, rate_idx, use_green);
	tbl->current_rate = rate;
	il4965_rs_set_expected_tpt_table(lq_sta, tbl);
	il4965_rs_fill_link_cmd(NULL, lq_sta, rate);
	il->stations[lq_sta->lq.sta_id].lq = &lq_sta->lq;
	il_send_lq_cmd(il, &lq_sta->lq, CMD_SYNC, true);
}

static void
il4965_rs_get_rate(void *il_r, struct ieee80211_sta *sta, void *il_sta,
		   struct ieee80211_tx_rate_control *txrc)
{

	struct sk_buff *skb = txrc->skb;
	struct ieee80211_supported_band *sband = txrc->sband;
	struct il_priv *il __maybe_unused = (struct il_priv *)il_r;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct il_lq_sta *lq_sta = il_sta;
	int rate_idx;

	D_RATE("rate scale calculate new rate for skb\n");

	
	if (lq_sta) {
		lq_sta->max_rate_idx = txrc->max_rate_idx;
		if (sband->band == IEEE80211_BAND_5GHZ &&
		    lq_sta->max_rate_idx != -1)
			lq_sta->max_rate_idx += IL_FIRST_OFDM_RATE;
		if (lq_sta->max_rate_idx < 0 ||
		    lq_sta->max_rate_idx >= RATE_COUNT)
			lq_sta->max_rate_idx = -1;
	}

	
	if (lq_sta && !lq_sta->drv) {
		D_RATE("Rate scaling not initialized yet.\n");
		il_sta = NULL;
	}

	
	if (rate_control_send_low(sta, il_sta, txrc))
		return;

	if (!lq_sta)
		return;

	rate_idx = lq_sta->last_txrate_idx;

	if (lq_sta->last_rate_n_flags & RATE_MCS_HT_MSK) {
		rate_idx -= IL_FIRST_OFDM_RATE;
		
		rate_idx = (rate_idx > 0) ? (rate_idx - 1) : 0;
		if (il4965_rs_extract_rate(lq_sta->last_rate_n_flags) >=
		    RATE_MIMO2_6M_PLCP)
			rate_idx = rate_idx + MCS_IDX_PER_STREAM;
		info->control.rates[0].flags = IEEE80211_TX_RC_MCS;
		if (lq_sta->last_rate_n_flags & RATE_MCS_SGI_MSK)
			info->control.rates[0].flags |=
			    IEEE80211_TX_RC_SHORT_GI;
		if (lq_sta->last_rate_n_flags & RATE_MCS_DUP_MSK)
			info->control.rates[0].flags |=
			    IEEE80211_TX_RC_DUP_DATA;
		if (lq_sta->last_rate_n_flags & RATE_MCS_HT40_MSK)
			info->control.rates[0].flags |=
			    IEEE80211_TX_RC_40_MHZ_WIDTH;
		if (lq_sta->last_rate_n_flags & RATE_MCS_GF_MSK)
			info->control.rates[0].flags |=
			    IEEE80211_TX_RC_GREEN_FIELD;
	} else {
		
		if (rate_idx < 0 || rate_idx >= RATE_COUNT_LEGACY ||
		    (sband->band == IEEE80211_BAND_5GHZ &&
		     rate_idx < IL_FIRST_OFDM_RATE))
			rate_idx = rate_lowest_index(sband, sta);
		
		else if (sband->band == IEEE80211_BAND_5GHZ)
			rate_idx -= IL_FIRST_OFDM_RATE;
		info->control.rates[0].flags = 0;
	}
	info->control.rates[0].idx = rate_idx;

}

static void *
il4965_rs_alloc_sta(void *il_rate, struct ieee80211_sta *sta, gfp_t gfp)
{
	struct il_station_priv *sta_priv =
	    (struct il_station_priv *)sta->drv_priv;
	struct il_priv *il;

	il = (struct il_priv *)il_rate;
	D_RATE("create station rate scale win\n");

	return &sta_priv->lq_sta;
}

void
il4965_rs_rate_init(struct il_priv *il, struct ieee80211_sta *sta, u8 sta_id)
{
	int i, j;
	struct ieee80211_hw *hw = il->hw;
	struct ieee80211_conf *conf = &il->hw->conf;
	struct ieee80211_sta_ht_cap *ht_cap = &sta->ht_cap;
	struct il_station_priv *sta_priv;
	struct il_lq_sta *lq_sta;
	struct ieee80211_supported_band *sband;

	sta_priv = (struct il_station_priv *)sta->drv_priv;
	lq_sta = &sta_priv->lq_sta;
	sband = hw->wiphy->bands[conf->channel->band];

	lq_sta->lq.sta_id = sta_id;

	for (j = 0; j < LQ_SIZE; j++)
		for (i = 0; i < RATE_COUNT; i++)
			il4965_rs_rate_scale_clear_win(&lq_sta->lq_info[j].
						       win[i]);

	lq_sta->flush_timer = 0;
	lq_sta->supp_rates = sta->supp_rates[sband->band];
	for (j = 0; j < LQ_SIZE; j++)
		for (i = 0; i < RATE_COUNT; i++)
			il4965_rs_rate_scale_clear_win(&lq_sta->lq_info[j].
						       win[i]);

	D_RATE("LQ:" "*** rate scale station global init for station %d ***\n",
	       sta_id);

	lq_sta->is_dup = 0;
	lq_sta->max_rate_idx = -1;
	lq_sta->missed_rate_counter = IL_MISSED_RATE_MAX;
	lq_sta->is_green = il4965_rs_use_green(il, sta);
	lq_sta->active_legacy_rate = il->active_rate & ~(0x1000);
	lq_sta->band = il->band;
	lq_sta->active_siso_rate = ht_cap->mcs.rx_mask[0] << 1;
	lq_sta->active_siso_rate |= ht_cap->mcs.rx_mask[0] & 0x1;
	lq_sta->active_siso_rate &= ~((u16) 0x2);
	lq_sta->active_siso_rate <<= IL_FIRST_OFDM_RATE;

	
	lq_sta->active_mimo2_rate = ht_cap->mcs.rx_mask[1] << 1;
	lq_sta->active_mimo2_rate |= ht_cap->mcs.rx_mask[1] & 0x1;
	lq_sta->active_mimo2_rate &= ~((u16) 0x2);
	lq_sta->active_mimo2_rate <<= IL_FIRST_OFDM_RATE;

	
	lq_sta->lq.general_params.single_stream_ant_msk =
	    il4965_first_antenna(il->hw_params.valid_tx_ant);
	lq_sta->lq.general_params.dual_stream_ant_msk =
	    il->hw_params.valid_tx_ant & ~il4965_first_antenna(il->hw_params.
							       valid_tx_ant);
	if (!lq_sta->lq.general_params.dual_stream_ant_msk) {
		lq_sta->lq.general_params.dual_stream_ant_msk = ANT_AB;
	} else if (il4965_num_of_ant(il->hw_params.valid_tx_ant) == 2) {
		lq_sta->lq.general_params.dual_stream_ant_msk =
		    il->hw_params.valid_tx_ant;
	}

	
	lq_sta->tx_agg_tid_en = IL_AGG_ALL_TID;
	lq_sta->drv = il;

	
	lq_sta->last_txrate_idx = rate_lowest_index(sband, sta);
	if (sband->band == IEEE80211_BAND_5GHZ)
		lq_sta->last_txrate_idx += IL_FIRST_OFDM_RATE;
	lq_sta->is_agg = 0;

#ifdef CONFIG_MAC80211_DEBUGFS
	lq_sta->dbg_fixed_rate = 0;
#endif

	il4965_rs_initialize_lq(il, conf, sta, lq_sta);
}

static void
il4965_rs_fill_link_cmd(struct il_priv *il, struct il_lq_sta *lq_sta,
			u32 new_rate)
{
	struct il_scale_tbl_info tbl_type;
	int idx = 0;
	int rate_idx;
	int repeat_rate = 0;
	u8 ant_toggle_cnt = 0;
	u8 use_ht_possible = 1;
	u8 valid_tx_ant = 0;
	struct il_link_quality_cmd *lq_cmd = &lq_sta->lq;

	
	il4965_rs_dbgfs_set_mcs(lq_sta, &new_rate, idx);

	
	il4965_rs_get_tbl_info_from_mcs(new_rate, lq_sta->band, &tbl_type,
					&rate_idx);

	
	if (is_legacy(tbl_type.lq_type)) {
		ant_toggle_cnt = 1;
		repeat_rate = IL_NUMBER_TRY;
	} else {
		repeat_rate = IL_HT_NUMBER_TRY;
	}

	lq_cmd->general_params.mimo_delimiter =
	    is_mimo(tbl_type.lq_type) ? 1 : 0;

	
	lq_cmd->rs_table[idx].rate_n_flags = cpu_to_le32(new_rate);

	if (il4965_num_of_ant(tbl_type.ant_type) == 1) {
		lq_cmd->general_params.single_stream_ant_msk =
		    tbl_type.ant_type;
	} else if (il4965_num_of_ant(tbl_type.ant_type) == 2) {
		lq_cmd->general_params.dual_stream_ant_msk = tbl_type.ant_type;
	}
	
	idx++;
	repeat_rate--;
	if (il)
		valid_tx_ant = il->hw_params.valid_tx_ant;

	
	while (idx < LINK_QUAL_MAX_RETRY_NUM) {
		while (repeat_rate > 0 && idx < LINK_QUAL_MAX_RETRY_NUM) {
			if (is_legacy(tbl_type.lq_type)) {
				if (ant_toggle_cnt < NUM_TRY_BEFORE_ANT_TOGGLE)
					ant_toggle_cnt++;
				else if (il &&
					 il4965_rs_toggle_antenna(valid_tx_ant,
								  &new_rate,
								  &tbl_type))
					ant_toggle_cnt = 1;
			}

			
			il4965_rs_dbgfs_set_mcs(lq_sta, &new_rate, idx);

			
			lq_cmd->rs_table[idx].rate_n_flags =
			    cpu_to_le32(new_rate);
			repeat_rate--;
			idx++;
		}

		il4965_rs_get_tbl_info_from_mcs(new_rate, lq_sta->band,
						&tbl_type, &rate_idx);

		if (is_mimo(tbl_type.lq_type))
			lq_cmd->general_params.mimo_delimiter = idx;

		
		new_rate =
		    il4965_rs_get_lower_rate(lq_sta, &tbl_type, rate_idx,
					     use_ht_possible);

		
		if (is_legacy(tbl_type.lq_type)) {
			if (ant_toggle_cnt < NUM_TRY_BEFORE_ANT_TOGGLE)
				ant_toggle_cnt++;
			else if (il &&
				 il4965_rs_toggle_antenna(valid_tx_ant,
							  &new_rate, &tbl_type))
				ant_toggle_cnt = 1;

			repeat_rate = IL_NUMBER_TRY;
		} else {
			repeat_rate = IL_HT_NUMBER_TRY;
		}

		use_ht_possible = 0;

		
		il4965_rs_dbgfs_set_mcs(lq_sta, &new_rate, idx);

		
		lq_cmd->rs_table[idx].rate_n_flags = cpu_to_le32(new_rate);

		idx++;
		repeat_rate--;
	}

	lq_cmd->agg_params.agg_frame_cnt_limit = LINK_QUAL_AGG_FRAME_LIMIT_DEF;
	lq_cmd->agg_params.agg_dis_start_th = LINK_QUAL_AGG_DISABLE_START_DEF;

	lq_cmd->agg_params.agg_time_limit =
	    cpu_to_le16(LINK_QUAL_AGG_TIME_LIMIT_DEF);
}

static void *
il4965_rs_alloc(struct ieee80211_hw *hw, struct dentry *debugfsdir)
{
	return hw->priv;
}

static void
il4965_rs_free(void *il_rate)
{
	return;
}

static void
il4965_rs_free_sta(void *il_r, struct ieee80211_sta *sta, void *il_sta)
{
	struct il_priv *il __maybe_unused = il_r;

	D_RATE("enter\n");
	D_RATE("leave\n");
}

#ifdef CONFIG_MAC80211_DEBUGFS

static void
il4965_rs_dbgfs_set_mcs(struct il_lq_sta *lq_sta, u32 * rate_n_flags, int idx)
{
	struct il_priv *il;
	u8 valid_tx_ant;
	u8 ant_sel_tx;

	il = lq_sta->drv;
	valid_tx_ant = il->hw_params.valid_tx_ant;
	if (lq_sta->dbg_fixed_rate) {
		ant_sel_tx =
		    ((lq_sta->
		      dbg_fixed_rate & RATE_MCS_ANT_ABC_MSK) >>
		     RATE_MCS_ANT_POS);
		if ((valid_tx_ant & ant_sel_tx) == ant_sel_tx) {
			*rate_n_flags = lq_sta->dbg_fixed_rate;
			D_RATE("Fixed rate ON\n");
		} else {
			lq_sta->dbg_fixed_rate = 0;
			IL_ERR
			    ("Invalid antenna selection 0x%X, Valid is 0x%X\n",
			     ant_sel_tx, valid_tx_ant);
			D_RATE("Fixed rate OFF\n");
		}
	} else {
		D_RATE("Fixed rate OFF\n");
	}
}

static ssize_t
il4965_rs_sta_dbgfs_scale_table_write(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct il_lq_sta *lq_sta = file->private_data;
	struct il_priv *il;
	char buf[64];
	size_t buf_size;
	u32 parsed_rate;

	il = lq_sta->drv;
	memset(buf, 0, sizeof(buf));
	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (sscanf(buf, "%x", &parsed_rate) == 1)
		lq_sta->dbg_fixed_rate = parsed_rate;
	else
		lq_sta->dbg_fixed_rate = 0;

	lq_sta->active_legacy_rate = 0x0FFF;	
	lq_sta->active_siso_rate = 0x1FD0;	
	lq_sta->active_mimo2_rate = 0x1FD0;	

	D_RATE("sta_id %d rate 0x%X\n", lq_sta->lq.sta_id,
	       lq_sta->dbg_fixed_rate);

	if (lq_sta->dbg_fixed_rate) {
		il4965_rs_fill_link_cmd(NULL, lq_sta, lq_sta->dbg_fixed_rate);
		il_send_lq_cmd(lq_sta->drv, &lq_sta->lq, CMD_ASYNC, false);
	}

	return count;
}

static ssize_t
il4965_rs_sta_dbgfs_scale_table_read(struct file *file, char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	char *buff;
	int desc = 0;
	int i = 0;
	int idx = 0;
	ssize_t ret;

	struct il_lq_sta *lq_sta = file->private_data;
	struct il_priv *il;
	struct il_scale_tbl_info *tbl = &(lq_sta->lq_info[lq_sta->active_tbl]);

	il = lq_sta->drv;
	buff = kmalloc(1024, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	desc += sprintf(buff + desc, "sta_id %d\n", lq_sta->lq.sta_id);
	desc +=
	    sprintf(buff + desc, "failed=%d success=%d rate=0%X\n",
		    lq_sta->total_failed, lq_sta->total_success,
		    lq_sta->active_legacy_rate);
	desc +=
	    sprintf(buff + desc, "fixed rate 0x%X\n", lq_sta->dbg_fixed_rate);
	desc +=
	    sprintf(buff + desc, "valid_tx_ant %s%s%s\n",
		    (il->hw_params.valid_tx_ant & ANT_A) ? "ANT_A," : "",
		    (il->hw_params.valid_tx_ant & ANT_B) ? "ANT_B," : "",
		    (il->hw_params.valid_tx_ant & ANT_C) ? "ANT_C" : "");
	desc +=
	    sprintf(buff + desc, "lq type %s\n",
		    (is_legacy(tbl->lq_type)) ? "legacy" : "HT");
	if (is_Ht(tbl->lq_type)) {
		desc +=
		    sprintf(buff + desc, " %s",
			    (is_siso(tbl->lq_type)) ? "SISO" : "MIMO2");
		desc +=
		    sprintf(buff + desc, " %s",
			    (tbl->is_ht40) ? "40MHz" : "20MHz");
		desc +=
		    sprintf(buff + desc, " %s %s %s\n",
			    (tbl->is_SGI) ? "SGI" : "",
			    (lq_sta->is_green) ? "GF enabled" : "",
			    (lq_sta->is_agg) ? "AGG on" : "");
	}
	desc +=
	    sprintf(buff + desc, "last tx rate=0x%X\n",
		    lq_sta->last_rate_n_flags);
	desc +=
	    sprintf(buff + desc,
		    "general:" "flags=0x%X mimo-d=%d s-ant0x%x d-ant=0x%x\n",
		    lq_sta->lq.general_params.flags,
		    lq_sta->lq.general_params.mimo_delimiter,
		    lq_sta->lq.general_params.single_stream_ant_msk,
		    lq_sta->lq.general_params.dual_stream_ant_msk);

	desc +=
	    sprintf(buff + desc,
		    "agg:"
		    "time_limit=%d dist_start_th=%d frame_cnt_limit=%d\n",
		    le16_to_cpu(lq_sta->lq.agg_params.agg_time_limit),
		    lq_sta->lq.agg_params.agg_dis_start_th,
		    lq_sta->lq.agg_params.agg_frame_cnt_limit);

	desc +=
	    sprintf(buff + desc,
		    "Start idx [0]=0x%x [1]=0x%x [2]=0x%x [3]=0x%x\n",
		    lq_sta->lq.general_params.start_rate_idx[0],
		    lq_sta->lq.general_params.start_rate_idx[1],
		    lq_sta->lq.general_params.start_rate_idx[2],
		    lq_sta->lq.general_params.start_rate_idx[3]);

	for (i = 0; i < LINK_QUAL_MAX_RETRY_NUM; i++) {
		idx =
		    il4965_hwrate_to_plcp_idx(le32_to_cpu
					      (lq_sta->lq.rs_table[i].
					       rate_n_flags));
		if (is_legacy(tbl->lq_type)) {
			desc +=
			    sprintf(buff + desc, " rate[%d] 0x%X %smbps\n", i,
				    le32_to_cpu(lq_sta->lq.rs_table[i].
						rate_n_flags),
				    il_rate_mcs[idx].mbps);
		} else {
			desc +=
			    sprintf(buff + desc, " rate[%d] 0x%X %smbps (%s)\n",
				    i,
				    le32_to_cpu(lq_sta->lq.rs_table[i].
						rate_n_flags),
				    il_rate_mcs[idx].mbps,
				    il_rate_mcs[idx].mcs);
		}
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buff, desc);
	kfree(buff);
	return ret;
}

static const struct file_operations rs_sta_dbgfs_scale_table_ops = {
	.write = il4965_rs_sta_dbgfs_scale_table_write,
	.read = il4965_rs_sta_dbgfs_scale_table_read,
	.open = simple_open,
	.llseek = default_llseek,
};

static ssize_t
il4965_rs_sta_dbgfs_stats_table_read(struct file *file, char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	char *buff;
	int desc = 0;
	int i, j;
	ssize_t ret;

	struct il_lq_sta *lq_sta = file->private_data;

	buff = kmalloc(1024, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	for (i = 0; i < LQ_SIZE; i++) {
		desc +=
		    sprintf(buff + desc,
			    "%s type=%d SGI=%d HT40=%d DUP=%d GF=%d\n"
			    "rate=0x%X\n", lq_sta->active_tbl == i ? "*" : "x",
			    lq_sta->lq_info[i].lq_type,
			    lq_sta->lq_info[i].is_SGI,
			    lq_sta->lq_info[i].is_ht40,
			    lq_sta->lq_info[i].is_dup, lq_sta->is_green,
			    lq_sta->lq_info[i].current_rate);
		for (j = 0; j < RATE_COUNT; j++) {
			desc +=
			    sprintf(buff + desc,
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
	.read = il4965_rs_sta_dbgfs_stats_table_read,
	.open = simple_open,
	.llseek = default_llseek,
};

static ssize_t
il4965_rs_sta_dbgfs_rate_scale_data_read(struct file *file,
					 char __user *user_buf, size_t count,
					 loff_t *ppos)
{
	char buff[120];
	int desc = 0;
	struct il_lq_sta *lq_sta = file->private_data;
	struct il_scale_tbl_info *tbl = &lq_sta->lq_info[lq_sta->active_tbl];

	if (is_Ht(tbl->lq_type))
		desc +=
		    sprintf(buff + desc, "Bit Rate= %d Mb/s\n",
			    tbl->expected_tpt[lq_sta->last_txrate_idx]);
	else
		desc +=
		    sprintf(buff + desc, "Bit Rate= %d Mb/s\n",
			    il_rates[lq_sta->last_txrate_idx].ieee >> 1);

	return simple_read_from_buffer(user_buf, count, ppos, buff, desc);
}

static const struct file_operations rs_sta_dbgfs_rate_scale_data_ops = {
	.read = il4965_rs_sta_dbgfs_rate_scale_data_read,
	.open = simple_open,
	.llseek = default_llseek,
};

static void
il4965_rs_add_debugfs(void *il, void *il_sta, struct dentry *dir)
{
	struct il_lq_sta *lq_sta = il_sta;
	lq_sta->rs_sta_dbgfs_scale_table_file =
	    debugfs_create_file("rate_scale_table", S_IRUSR | S_IWUSR, dir,
				lq_sta, &rs_sta_dbgfs_scale_table_ops);
	lq_sta->rs_sta_dbgfs_stats_table_file =
	    debugfs_create_file("rate_stats_table", S_IRUSR, dir, lq_sta,
				&rs_sta_dbgfs_stats_table_ops);
	lq_sta->rs_sta_dbgfs_rate_scale_data_file =
	    debugfs_create_file("rate_scale_data", S_IRUSR, dir, lq_sta,
				&rs_sta_dbgfs_rate_scale_data_ops);
	lq_sta->rs_sta_dbgfs_tx_agg_tid_en_file =
	    debugfs_create_u8("tx_agg_tid_enable", S_IRUSR | S_IWUSR, dir,
			      &lq_sta->tx_agg_tid_en);

}

static void
il4965_rs_remove_debugfs(void *il, void *il_sta)
{
	struct il_lq_sta *lq_sta = il_sta;
	debugfs_remove(lq_sta->rs_sta_dbgfs_scale_table_file);
	debugfs_remove(lq_sta->rs_sta_dbgfs_stats_table_file);
	debugfs_remove(lq_sta->rs_sta_dbgfs_rate_scale_data_file);
	debugfs_remove(lq_sta->rs_sta_dbgfs_tx_agg_tid_en_file);
}
#endif

static void
il4965_rs_rate_init_stub(void *il_r, struct ieee80211_supported_band *sband,
			 struct ieee80211_sta *sta, void *il_sta)
{
}

static struct rate_control_ops rs_4965_ops = {
	.module = NULL,
	.name = IL4965_RS_NAME,
	.tx_status = il4965_rs_tx_status,
	.get_rate = il4965_rs_get_rate,
	.rate_init = il4965_rs_rate_init_stub,
	.alloc = il4965_rs_alloc,
	.free = il4965_rs_free,
	.alloc_sta = il4965_rs_alloc_sta,
	.free_sta = il4965_rs_free_sta,
#ifdef CONFIG_MAC80211_DEBUGFS
	.add_sta_debugfs = il4965_rs_add_debugfs,
	.remove_sta_debugfs = il4965_rs_remove_debugfs,
#endif
};

int
il4965_rate_control_register(void)
{
	return ieee80211_rate_control_register(&rs_4965_ops);
}

void
il4965_rate_control_unregister(void)
{
	ieee80211_rate_control_unregister(&rs_4965_ops);
}
