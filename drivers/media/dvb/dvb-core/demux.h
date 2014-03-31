/*
 * demux.h
 *
 * Copyright (c) 2002 Convergence GmbH
 *
 * based on code:
 * Copyright (c) 2000 Nokia Research Center
 *                    Tampere, FINLAND
 *
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef __DEMUX_H
#define __DEMUX_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/dvb/dmx.h>



#ifndef DMX_MAX_FILTER_SIZE
#define DMX_MAX_FILTER_SIZE 18
#endif


#ifndef DMX_MAX_SECTION_SIZE
#define DMX_MAX_SECTION_SIZE 4096
#endif
#ifndef DMX_MAX_SECFEED_SIZE
#define DMX_MAX_SECFEED_SIZE (DMX_MAX_SECTION_SIZE + 188)
#endif



enum dmx_success {
	DMX_OK = 0, 
	DMX_OK_PES_END, 
	DMX_OK_PCR, 
	DMX_OK_EOS, 
	DMX_OK_MARKER, 
	DMX_LENGTH_ERROR, 
	DMX_OVERRUN_ERROR, 
	DMX_CRC_ERROR, 
	DMX_FRAME_ERROR, 
	DMX_FIFO_ERROR, 
	DMX_MISSED_ERROR, 
	DMX_OK_DECODER_BUF, 
	DMX_OK_IDX, 
	DMX_OK_SCRAMBLING_STATUS, 
} ;


/*
 * struct dmx_data_ready: Parameters for event notification callback.
 * Event notification notifies demux device that data is written
 * and available in the device's output buffer or provides
 * notification on errors and other events. In the latter case
 * data_length is zero.
 */
struct dmx_data_ready {
	enum dmx_success status;

	int data_length;

	union {
		struct {
			int start_gap;
			int actual_length;
			int disc_indicator_set;
			int pes_length_mismatch;
			u64 stc;
			u32 tei_counter;
			u32 cont_err_counter;
			u32 ts_packets_num;
		} pes_end;

		struct {
			u64 pcr;
			u64 stc;
			int disc_indicator_set;
		} pcr;

		struct {
			int handle;
			int cookie;
			u32 offset;
			u32 len;
			int pts_exists;
			u64 pts;
			int dts_exists;
			u64 dts;
			u32 tei_counter;
			u32 cont_err_counter;
			u32 ts_packets_num;
			u32 ts_dropped_bytes;
			u64 stc;
		} buf;

		struct {
			u64 id;
		} marker;

		struct dmx_index_event_info idx_event;
		struct dmx_scrambling_status_event_info scrambling_bits;
	};
};

struct data_buffer {
	
	const struct dvb_ringbuffer *ringbuff;


	void *priv_handle;
};



#define TS_PACKET       1   
#define	TS_PAYLOAD_ONLY 2   
#define TS_DECODER      4   
#define TS_DEMUX        8   


enum dmx_ts_pes
{  
	DMX_TS_PES_AUDIO0,
	DMX_TS_PES_VIDEO0,
	DMX_TS_PES_TELETEXT0,
	DMX_TS_PES_SUBTITLE0,
	DMX_TS_PES_PCR0,

	DMX_TS_PES_AUDIO1,
	DMX_TS_PES_VIDEO1,
	DMX_TS_PES_TELETEXT1,
	DMX_TS_PES_SUBTITLE1,
	DMX_TS_PES_PCR1,

	DMX_TS_PES_AUDIO2,
	DMX_TS_PES_VIDEO2,
	DMX_TS_PES_TELETEXT2,
	DMX_TS_PES_SUBTITLE2,
	DMX_TS_PES_PCR2,

	DMX_TS_PES_AUDIO3,
	DMX_TS_PES_VIDEO3,
	DMX_TS_PES_TELETEXT3,
	DMX_TS_PES_SUBTITLE3,
	DMX_TS_PES_PCR3,

	DMX_TS_PES_OTHER
};

#define DMX_TS_PES_AUDIO    DMX_TS_PES_AUDIO0
#define DMX_TS_PES_VIDEO    DMX_TS_PES_VIDEO0
#define DMX_TS_PES_TELETEXT DMX_TS_PES_TELETEXT0
#define DMX_TS_PES_SUBTITLE DMX_TS_PES_SUBTITLE0
#define DMX_TS_PES_PCR      DMX_TS_PES_PCR0

struct dmx_ts_feed;
typedef int (*dmx_ts_data_ready_cb)(
		struct dmx_ts_feed *source,
		struct dmx_data_ready *dmx_data_ready);

struct dmx_ts_feed {
	int is_filtering; 
	struct dmx_demux *parent; 
	struct data_buffer buffer;
	void *priv; 
	struct dmx_decoder_buffers *decoder_buffers;
	int (*set) (struct dmx_ts_feed *feed,
		    u16 pid,
		    int type,
		    enum dmx_ts_pes pes_type,
		    size_t circular_buffer_size,
		    struct timespec timeout);
	int (*start_filtering) (struct dmx_ts_feed* feed);
	int (*stop_filtering) (struct dmx_ts_feed* feed);
	int (*set_video_codec) (struct dmx_ts_feed *feed,
				enum dmx_video_codec video_codec);
	int (*set_idx_params) (struct dmx_ts_feed *feed,
				struct dmx_indexing_params *idx_params);
	int (*get_decoder_buff_status)(
			struct dmx_ts_feed *feed,
			struct dmx_buffer_status *dmx_buffer_status);
	int (*reuse_decoder_buffer)(
			struct dmx_ts_feed *feed,
			int cookie);
	int (*data_ready_cb)(struct dmx_ts_feed *feed,
			dmx_ts_data_ready_cb callback);
	int (*notify_data_read)(struct dmx_ts_feed *feed,
			u32 bytes_num);
	int (*set_tsp_out_format)(struct dmx_ts_feed *feed,
				enum dmx_tsp_format_t tsp_format);
	int (*set_secure_mode)(struct dmx_ts_feed *feed,
				struct dmx_secure_mode *sec_mode);
	int (*set_cipher_ops)(struct dmx_ts_feed *feed,
				struct dmx_cipher_operations *cipher_ops);
	int (*oob_command) (struct dmx_ts_feed *feed,
			struct dmx_oob_command *cmd);
	int (*ts_insertion_init)(struct dmx_ts_feed *feed);
	int (*ts_insertion_terminate)(struct dmx_ts_feed *feed);
	int (*ts_insertion_insert_buffer)(struct dmx_ts_feed *feed,
			char *data, size_t size);
	int (*get_scrambling_bits)(struct dmx_ts_feed *feed, u8 *value);
};


struct dmx_section_filter {
	u8 filter_value [DMX_MAX_FILTER_SIZE];
	u8 filter_mask [DMX_MAX_FILTER_SIZE];
	u8 filter_mode [DMX_MAX_FILTER_SIZE];
	struct dmx_section_feed* parent; 
	struct data_buffer buffer;
	void* priv; 
};

struct dmx_section_feed;
typedef int (*dmx_section_data_ready_cb)(
		struct dmx_section_filter *source,
		struct dmx_data_ready *dmx_data_ready);

struct dmx_section_feed {
	int is_filtering; 
	struct dmx_demux* parent; 
	void* priv; 

	int check_crc;
	u32 crc_val;

	u8 *secbuf;
	u8 secbuf_base[DMX_MAX_SECFEED_SIZE];
	u16 secbufp, seclen, tsfeedp;

	int (*set) (struct dmx_section_feed* feed,
		    u16 pid,
		    size_t circular_buffer_size,
		    int check_crc);
	int (*allocate_filter) (struct dmx_section_feed* feed,
				struct dmx_section_filter** filter);
	int (*release_filter) (struct dmx_section_feed* feed,
			       struct dmx_section_filter* filter);
	int (*start_filtering) (struct dmx_section_feed* feed);
	int (*stop_filtering) (struct dmx_section_feed* feed);
	int (*data_ready_cb)(struct dmx_section_feed *feed,
			dmx_section_data_ready_cb callback);
	int (*notify_data_read)(struct dmx_section_filter *filter,
			u32 bytes_num);
	int (*set_secure_mode)(struct dmx_section_feed *feed,
				struct dmx_secure_mode *sec_mode);
	int (*set_cipher_ops)(struct dmx_section_feed *feed,
				struct dmx_cipher_operations *cipher_ops);
	int (*oob_command) (struct dmx_section_feed *feed,
				struct dmx_oob_command *cmd);
	int (*get_scrambling_bits)(struct dmx_section_feed *feed, u8 *value);
};


typedef int (*dmx_ts_cb) ( const u8 * buffer1,
			   size_t buffer1_length,
			   const u8 * buffer2,
			   size_t buffer2_length,
			   struct dmx_ts_feed* source,
			   enum dmx_success success);

typedef int (*dmx_section_cb) (	const u8 * buffer1,
				size_t buffer1_len,
				const u8 * buffer2,
				size_t buffer2_len,
				struct dmx_section_filter * source,
				enum dmx_success success);

typedef int (*dmx_ts_fullness) (
				struct dmx_ts_feed *source,
				int required_space);

typedef int (*dmx_section_fullness) (
				struct dmx_section_filter *source,
				int required_space);


enum dmx_frontend_source {
	DMX_MEMORY_FE,
	DMX_FRONTEND_0,
	DMX_FRONTEND_1,
	DMX_FRONTEND_2,
	DMX_FRONTEND_3,
	DMX_STREAM_0,    
	DMX_STREAM_1,
	DMX_STREAM_2,
	DMX_STREAM_3
};

struct dmx_frontend {
	struct list_head connectivity_list; 
	enum dmx_frontend_source source;
};



#define DMX_TS_FILTERING                        1
#define DMX_PES_FILTERING                       2
#define DMX_SECTION_FILTERING                   4
#define DMX_MEMORY_BASED_FILTERING              8    
#define DMX_CRC_CHECKING                        16
#define DMX_TS_DESCRAMBLING                     32



#define DMX_FE_ENTRY(list) list_entry(list, struct dmx_frontend, connectivity_list)

struct dmx_demux {
	u32 capabilities;            
	struct dmx_frontend* frontend;    
	void* priv;                  
	struct data_buffer dvr_input; 
	int dvr_input_protected;
	struct dentry *debugfs_demux_dir; 

	int (*open) (struct dmx_demux* demux);
	int (*close) (struct dmx_demux* demux);
	int (*write) (struct dmx_demux *demux, const char *buf, size_t count);
	int (*allocate_ts_feed) (struct dmx_demux* demux,
				 struct dmx_ts_feed** feed,
				 dmx_ts_cb callback);
	int (*release_ts_feed) (struct dmx_demux* demux,
				struct dmx_ts_feed* feed);
	int (*allocate_section_feed) (struct dmx_demux* demux,
				      struct dmx_section_feed** feed,
				      dmx_section_cb callback);
	int (*release_section_feed) (struct dmx_demux* demux,
				     struct dmx_section_feed* feed);
	int (*add_frontend) (struct dmx_demux* demux,
			     struct dmx_frontend* frontend);
	int (*remove_frontend) (struct dmx_demux* demux,
				struct dmx_frontend* frontend);
	struct list_head* (*get_frontends) (struct dmx_demux* demux);
	int (*connect_frontend) (struct dmx_demux* demux,
				 struct dmx_frontend* frontend);
	int (*disconnect_frontend) (struct dmx_demux* demux);

	int (*get_pes_pids) (struct dmx_demux* demux, u16 *pids);

	int (*get_caps) (struct dmx_demux* demux, struct dmx_caps *caps);

	int (*set_source) (struct dmx_demux *demux, const dmx_source_t *src);

	int (*set_tsp_format) (struct dmx_demux *demux,
				enum dmx_tsp_format_t tsp_format);

	int (*set_playback_mode) (struct dmx_demux *demux,
				 enum dmx_playback_mode_t mode,
				 dmx_ts_fullness ts_fullness_callback,
				 dmx_section_fullness sec_fullness_callback);

	int (*write_cancel) (struct dmx_demux *demux);

	int (*get_stc) (struct dmx_demux* demux, unsigned int num,
			u64 *stc, unsigned int *base);

	int (*map_buffer) (struct dmx_demux *demux,
			struct dmx_buffer *dmx_buffer,
			void **priv_handle, void **mem);

	int (*unmap_buffer) (struct dmx_demux *demux,
			void *priv_handle);

	int (*get_tsp_size) (struct dmx_demux *demux);
};

#endif 
