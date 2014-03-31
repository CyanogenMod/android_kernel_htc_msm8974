/**
 * TSIF driver
 *
 * Kernel API
 *
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#ifndef _TSIF_API_H_
#define _TSIF_API_H_
/**
 * Theory of operation
 *
 * TSIF driver maintains internal cyclic data buffer where
 * received TSIF packets are stored. Size of buffer, in packets,
 * and its address, may be obtained by tsif_get_info().
 *
 * TSIF stream delivered to the client that should register with
 * TSIF driver using tsif_attach()
 *
 * Producer-consumer pattern used. TSIF driver act as producer,
 * writing data to the buffer; clientis consumer.
 * 2 indexes maintained by the TSIF driver:
 * - wi (write index) points to the next item to be written by
 *   TSIF
 * - ri (read index) points to the next item available for read
 *   by the client.
 * Write index advanced by the TSIF driver when new data
 * received;
 * Read index advanced only when client tell so to the TSIF
 * driver by tsif_reclaim_packets()
 *
 * Consumer may directly access data TSIF buffer between ri and
 * wi. When ri==wi, buffer is empty.
 *
 * TSIF driver notifies client about any change by calling
 * notify function. Client should use tsif_get_state() to query
 * new state.
 */

#define TSIF_PKT_SIZE             (192)

static inline u32 tsif_pkt_status(void *pkt)
{
	u32 *x = pkt;
	return x[TSIF_PKT_SIZE / sizeof(u32) - 1];
}

#define TSIF_STATUS_TTS(x)   ((x) & 0xffffff)
#define TSIF_STATUS_VALID(x) ((x) & (1<<24))
#define TSIF_STATUS_FIRST(x) ((x) & (1<<25))
#define TSIF_STATUS_OVFLW(x) ((x) & (1<<26))
#define TSIF_STATUS_ERROR(x) ((x) & (1<<27))
#define TSIF_STATUS_NULL(x)  ((x) & (1<<28))
#define TSIF_STATUS_TIMEO(x) ((x) & (1<<30))

enum tsif_state {
	tsif_state_stopped  = 0,
	tsif_state_running  = 1,
	tsif_state_flushing = 2,
	tsif_state_error    = 3,
};

int tsif_get_active(void);

void *tsif_attach(int id, void (*notify)(void *client_data), void *client_data);

void tsif_detach(void *cookie);

void tsif_get_info(void *cookie, void **pdata, int *psize);

int tsif_set_mode(void *cookie, int mode);

int tsif_set_time_limit(void *cookie, u32 value);

int tsif_set_buf_config(void *cookie, u32 pkts_in_chunk, u32 chunks_in_buf);

void tsif_get_state(void *cookie, int *ri, int *wi, enum tsif_state *state);

int tsif_set_clk_inverse(void *cookie, int inverse);

int tsif_set_data_inverse(void *cookie, int inverse);

int tsif_set_sync_inverse(void *cookie, int inverse);

int tsif_set_enable_inverse(void *cookie, int inverse);

int tsif_start(void *cookie);

void tsif_stop(void *cookie);

int tsif_get_ref_clk_counter(void *cookie, u32 *tcr_counter);

void tsif_reclaim_packets(void *cookie, int ri);

#endif 

