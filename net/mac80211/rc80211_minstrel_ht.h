/*
 * Copyright (C) 2010 Felix Fietkau <nbd@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RC_MINSTREL_HT_H
#define __RC_MINSTREL_HT_H

#define MINSTREL_MAX_STREAMS	3
#define MINSTREL_STREAM_GROUPS	4

#define MINSTREL_SCALE	16
#define MINSTREL_FRAC(val, div) (((val) << MINSTREL_SCALE) / div)
#define MINSTREL_TRUNC(val) ((val) >> MINSTREL_SCALE)

#define MCS_GROUP_RATES	8

struct mcs_group {
	u32 flags;
	unsigned int streams;
	unsigned int duration[MCS_GROUP_RATES];
};

extern const struct mcs_group minstrel_mcs_groups[];

struct minstrel_rate_stats {
	
	unsigned int attempts, last_attempts;
	unsigned int success, last_success;

	
	u64 att_hist, succ_hist;

	
	unsigned int cur_tp;

	
	unsigned int cur_prob, probability;

	
	unsigned int retry_count;
	unsigned int retry_count_rtscts;

	bool retry_updated;
	u8 sample_skipped;
};

struct minstrel_mcs_group_data {
	u8 index;
	u8 column;

	
	u8 supported;

	
	unsigned int max_tp_rate;
	unsigned int max_tp_rate2;
	unsigned int max_prob_rate;

	
	struct minstrel_rate_stats rates[MCS_GROUP_RATES];
};

struct minstrel_ht_sta {
	
	unsigned int ampdu_len;
	unsigned int ampdu_packets;

	
	unsigned int avg_ampdu_len;

	
	unsigned int max_tp_rate;

	
	unsigned int max_tp_rate2;

	
	unsigned int max_prob_rate;

	
	unsigned long stats_update;

	
	unsigned int overhead;
	unsigned int overhead_rtscts;

	unsigned int total_packets;
	unsigned int sample_packets;

	
	u32 tx_flags;

	u8 sample_wait;
	u8 sample_tries;
	u8 sample_count;
	u8 sample_slow;

	
	u8 sample_group;

	
	struct minstrel_mcs_group_data groups[MINSTREL_MAX_STREAMS * MINSTREL_STREAM_GROUPS];
};

struct minstrel_ht_sta_priv {
	union {
		struct minstrel_ht_sta ht;
		struct minstrel_sta_info legacy;
	};
#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *dbg_stats;
#endif
	void *ratelist;
	void *sample_table;
	bool is_ht;
};

void minstrel_ht_add_sta_debugfs(void *priv, void *priv_sta, struct dentry *dir);
void minstrel_ht_remove_sta_debugfs(void *priv, void *priv_sta);

#endif
