/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef AUDIO_LPA_H
#define AUDIO_LPA_H

#include <linux/earlysuspend.h>
#include <linux/wakelock.h>

#define ADRV_STATUS_OBUF_GIVEN 0x00000001
#define ADRV_STATUS_IBUF_GIVEN 0x00000002
#define ADRV_STATUS_FSYNC 0x00000004
#define ADRV_STATUS_PAUSE 0x00000008

struct buffer {
	void *data;
	unsigned size;
	unsigned used;		
	unsigned addr;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
struct audlpa_suspend_ctl {
	struct early_suspend node;
	struct audio *audio;
};
#endif

struct codec_operations {
	long (*ioctl)(struct file *, unsigned int, unsigned long);
	int (*set_params)(void *);
};

struct audio {
	spinlock_t dsp_lock;

	uint8_t out_needed; 
	struct list_head out_queue; 

	struct mutex lock;
	struct mutex write_lock;
	wait_queue_head_t write_wait;

	struct audio_client *ac;

	
	uint32_t out_sample_rate;
	uint32_t out_channel_mode;
	uint32_t out_bits; 

	int32_t phys; 

	uint32_t drv_status;
	int wflush; 
	int opened;
	int out_enabled;
	int out_prefill;
	int running;
	int stopped; 
	int buf_refresh;
	int teos; 

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct audlpa_suspend_ctl suspend_ctl;
#endif

	struct wake_lock wakelock;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif

	wait_queue_head_t wait;
	struct list_head free_event_queue;
	struct list_head event_queue;
	wait_queue_head_t event_wait;
	spinlock_t event_queue_lock;
	struct mutex get_event_lock;
	int event_abort;

	uint32_t device_events;

	struct list_head ion_region_queue; 
	struct ion_client *client;

	int eq_enable;
	int eq_needs_commit;
	uint32_t volume;

	unsigned int minor_no;
	struct codec_operations codec_ops;
	uint32_t buffer_size;
	uint32_t buffer_count;
	uint32_t bytes_consumed;
};

#endif 
