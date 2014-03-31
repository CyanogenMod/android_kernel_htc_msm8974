/*
 * Copyright 2007, Mattias Nissler <mattias.nissler@gmx.de>
 * Copyright 2007, Stefano Brivio <stefano.brivio@polimi.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef RC80211_PID_H
#define RC80211_PID_H

#define RC_PID_INTERVAL			125

#define RC_PID_SMOOTHING_SHIFT		3
#define RC_PID_SMOOTHING		(1 << RC_PID_SMOOTHING_SHIFT)

#define RC_PID_SHARPENING_FACTOR	0
#define RC_PID_SHARPENING_DURATION	0

#define RC_PID_ARITH_SHIFT		8

#define RC_PID_COEFF_P			15
#define RC_PID_COEFF_I			9
#define RC_PID_COEFF_D			15

#define RC_PID_TARGET_PF		14

#define RC_PID_NORM_OFFSET		3

#define RC_PID_FAST_START		0

#define RC_PID_DO_ARITH_RIGHT_SHIFT(x, y) \
	((x) < 0 ? -((-(x)) >> (y)) : (x) >> (y))

enum rc_pid_event_type {
	RC_PID_EVENT_TYPE_TX_STATUS,
	RC_PID_EVENT_TYPE_RATE_CHANGE,
	RC_PID_EVENT_TYPE_TX_RATE,
	RC_PID_EVENT_TYPE_PF_SAMPLE,
};

union rc_pid_event_data {
	
	struct {
		u32 flags;
		struct ieee80211_tx_info tx_status;
	};
	
	
	struct {
		int index;
		int rate;
	};
	
	struct {
		s32 pf_sample;
		s32 prop_err;
		s32 int_err;
		s32 der_err;
	};
};

struct rc_pid_event {
	
	unsigned long timestamp;

	
	unsigned int id;

	
	enum rc_pid_event_type type;

	
	union rc_pid_event_data data;
};

#define RC_PID_EVENT_RING_SIZE 32

struct rc_pid_event_buffer {
	
	unsigned int ev_count;

	
	struct rc_pid_event ring[RC_PID_EVENT_RING_SIZE];

	
	unsigned int next_entry;

	
	spinlock_t lock;

	
	wait_queue_head_t waitqueue;
};

struct rc_pid_events_file_info {
	
	struct rc_pid_event_buffer *events;

	
	unsigned int next_entry;
};

struct rc_pid_debugfs_entries {
	struct dentry *target;
	struct dentry *sampling_period;
	struct dentry *coeff_p;
	struct dentry *coeff_i;
	struct dentry *coeff_d;
	struct dentry *smoothing_shift;
	struct dentry *sharpen_factor;
	struct dentry *sharpen_duration;
	struct dentry *norm_offset;
};

void rate_control_pid_event_tx_status(struct rc_pid_event_buffer *buf,
				      struct ieee80211_tx_info *stat);

void rate_control_pid_event_rate_change(struct rc_pid_event_buffer *buf,
					       int index, int rate);

void rate_control_pid_event_tx_rate(struct rc_pid_event_buffer *buf,
					   int index, int rate);

void rate_control_pid_event_pf_sample(struct rc_pid_event_buffer *buf,
					     s32 pf_sample, s32 prop_err,
					     s32 int_err, s32 der_err);

void rate_control_pid_add_sta_debugfs(void *priv, void *priv_sta,
					     struct dentry *dir);

void rate_control_pid_remove_sta_debugfs(void *priv, void *priv_sta);

struct rc_pid_sta_info {
	unsigned long last_change;
	unsigned long last_sample;

	u32 tx_num_failed;
	u32 tx_num_xmit;

	int txrate_idx;

	s32 err_avg_sc;

	
	u32 last_pf;

	
	u8 sharp_cnt;

#ifdef CONFIG_MAC80211_DEBUGFS
	
	struct rc_pid_event_buffer events;

	
	struct dentry *events_entry;
#endif
};

struct rc_pid_rateinfo {

	
	int index;

	
	int rev_index;

	
	bool valid;

	
	int diff;
};

struct rc_pid_info {

	
	unsigned int target;

	
	unsigned int sampling_period;

	
	int coeff_p;
	int coeff_i;
	int coeff_d;

	
	unsigned int smoothing_shift;

	
	unsigned int sharpen_factor;
	unsigned int sharpen_duration;

	
	unsigned int norm_offset;

	
	struct rc_pid_rateinfo *rinfo;

	
	int oldrate;

#ifdef CONFIG_MAC80211_DEBUGFS
	
	struct rc_pid_debugfs_entries dentries;
#endif
};

#endif 
