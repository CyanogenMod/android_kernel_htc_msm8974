/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __SDIO_AL_PRIVATE__
#define __SDIO_AL_PRIVATE__

#include <linux/mmc/card.h>
#include <linux/platform_device.h>
#include <mach/sdio_al.h>

#define DRV_VERSION "1.30"
#define MODULE_NAME "sdio_al"
#define SDIOC_CHAN_TO_FUNC_NUM(x)	((x)+2)
#define REAL_FUNC_TO_FUNC_IN_ARRAY(x)	((x)-1)
#define SDIO_PREFIX "SDIO_"
#define PEER_CHANNEL_NAME_SIZE		4
#define CHANNEL_NAME_SIZE (sizeof(SDIO_PREFIX) + PEER_CHANNEL_NAME_SIZE)
#define SDIO_TEST_POSTFIX_SIZE 5
#define MAX_NUM_OF_SDIO_DEVICES	2
#define TEST_CH_NAME_SIZE (CHANNEL_NAME_SIZE + SDIO_TEST_POSTFIX_SIZE)

struct sdio_al_device; 

enum sdio_channel_state {
	SDIO_CHANNEL_STATE_INVALID,	 
	SDIO_CHANNEL_STATE_IDLE,         
	SDIO_CHANNEL_STATE_CLOSED,       
	SDIO_CHANNEL_STATE_OPEN,	 
	SDIO_CHANNEL_STATE_CLOSING,      
};
struct peer_sdioc_channel_config {
	u32 is_ready;
	u32 max_rx_threshold; 
	u32 max_tx_threshold; 
	u32 tx_buf_size;
	u32 max_packet_size;
	u32 is_host_ok_to_sleep;
	u32 is_packet_mode;
	u32 peer_operation;
	u32 is_low_latency_ch;
	u32 reserved[23];
};


struct sdio_channel_statistics {
	int last_any_read_avail;
	int last_read_avail;
	int last_old_read_avail;
	int total_notifs;
	int total_read_times;
};

struct sdio_channel {
	
	char name[CHANNEL_NAME_SIZE];
	char ch_test_name[TEST_CH_NAME_SIZE];
	int read_threshold;
	int write_threshold;
	int def_read_threshold;
	int threshold_change_cnt;
	int min_write_avail;
	int poll_delay_msec;
	int is_packet_mode;
	int is_low_latency_ch;

	struct peer_sdioc_channel_config ch_config;

	
	int num;

	void (*notify)(void *priv, unsigned channel_event);
	void *priv;

	int state;

	struct sdio_func *func;

	int rx_pipe_index;
	int tx_pipe_index;

	struct mutex ch_lock;

	u32 read_avail;
	u32 write_avail;

	u32 peer_tx_buf_size;

	u16 rx_pending_bytes;

	struct list_head rx_size_list_head;

	struct platform_device *pdev;

	u32 total_rx_bytes;
	u32 total_tx_bytes;

	u32 signature;

	struct sdio_al_device *sdio_al_dev;

	struct sdio_channel_statistics statistics;
};

int sdio_downloader_setup(struct mmc_card *card,
			  unsigned int num_of_devices,
			  int func_number,
			  int(*func)(void));

int test_channel_init(char *name);

void sdio_al_register_lpm_cb(void *device_handle,
				       int(*lpm_callback)(void *, int));

void sdio_al_unregister_lpm_cb(void *device_handle);

#endif 
