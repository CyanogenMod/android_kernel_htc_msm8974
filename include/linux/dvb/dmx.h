/*
 * dmx.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
 *                    for convergence integrated media GmbH
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

#ifndef _DVBDMX_H_
#define _DVBDMX_H_

#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/time.h>
#else
#include <time.h>
#endif


#define DMX_FILTER_SIZE 16

#define DMX_REC_BUFF_CHUNK_MIN_SIZE		(100*188)

#define DMX_MAX_DECODER_BUFFER_NUM		(32)

typedef enum
{
	DMX_OUT_DECODER, 
	DMX_OUT_TAP,     
			 
	DMX_OUT_TS_TAP,  
			 
			 
	DMX_OUT_TSDEMUX_TAP 
} dmx_output_t;


typedef enum
{
	DMX_IN_FRONTEND, 
	DMX_IN_DVR       
} dmx_input_t;


typedef enum
{
	DMX_PES_AUDIO0,
	DMX_PES_VIDEO0,
	DMX_PES_TELETEXT0,
	DMX_PES_SUBTITLE0,
	DMX_PES_PCR0,

	DMX_PES_AUDIO1,
	DMX_PES_VIDEO1,
	DMX_PES_TELETEXT1,
	DMX_PES_SUBTITLE1,
	DMX_PES_PCR1,

	DMX_PES_AUDIO2,
	DMX_PES_VIDEO2,
	DMX_PES_TELETEXT2,
	DMX_PES_SUBTITLE2,
	DMX_PES_PCR2,

	DMX_PES_AUDIO3,
	DMX_PES_VIDEO3,
	DMX_PES_TELETEXT3,
	DMX_PES_SUBTITLE3,
	DMX_PES_PCR3,

	DMX_PES_OTHER
} dmx_pes_type_t;

#define DMX_PES_AUDIO    DMX_PES_AUDIO0
#define DMX_PES_VIDEO    DMX_PES_VIDEO0
#define DMX_PES_TELETEXT DMX_PES_TELETEXT0
#define DMX_PES_SUBTITLE DMX_PES_SUBTITLE0
#define DMX_PES_PCR      DMX_PES_PCR0


typedef struct dmx_filter
{
	__u8  filter[DMX_FILTER_SIZE];
	__u8  mask[DMX_FILTER_SIZE];
	__u8  mode[DMX_FILTER_SIZE];
} dmx_filter_t;


#define DMX_CHECK_CRC		0x01
#define DMX_ONESHOT		0x02
#define DMX_IMMEDIATE_START	0x04
#define DMX_KERNEL_CLIENT	0x8000

struct dmx_sct_filter_params
{
	__u16          pid;
	dmx_filter_t   filter;
	__u32          timeout;
	__u32          flags;
};


enum dmx_video_codec {
	DMX_VIDEO_CODEC_MPEG2,
	DMX_VIDEO_CODEC_H264,
	DMX_VIDEO_CODEC_VC1
};

#define DMX_IDX_RAI                         0x00000001
#define DMX_IDX_PUSI                        0x00000002
#define DMX_IDX_MPEG_SEQ_HEADER             0x00000004
#define DMX_IDX_MPEG_GOP                    0x00000008
#define DMX_IDX_MPEG_FIRST_SEQ_FRAME_START  0x00000010
#define DMX_IDX_MPEG_FIRST_SEQ_FRAME_END    0x00000020
#define DMX_IDX_MPEG_I_FRAME_START          0x00000040
#define DMX_IDX_MPEG_I_FRAME_END            0x00000080
#define DMX_IDX_MPEG_P_FRAME_START          0x00000100
#define DMX_IDX_MPEG_P_FRAME_END            0x00000200
#define DMX_IDX_MPEG_B_FRAME_START          0x00000400
#define DMX_IDX_MPEG_B_FRAME_END            0x00000800
#define DMX_IDX_H264_SPS                    0x00001000
#define DMX_IDX_H264_PPS                    0x00002000
#define DMX_IDX_H264_FIRST_SPS_FRAME_START  0x00004000
#define DMX_IDX_H264_FIRST_SPS_FRAME_END    0x00008000
#define DMX_IDX_H264_IDR_START              0x00010000
#define DMX_IDX_H264_IDR_END                0x00020000
#define DMX_IDX_H264_NON_IDR_START          0x00040000
#define DMX_IDX_H264_NON_IDR_END            0x00080000
#define DMX_IDX_VC1_SEQ_HEADER              0x00100000
#define DMX_IDX_VC1_ENTRY_POINT             0x00200000
#define DMX_IDX_VC1_FIRST_SEQ_FRAME_START   0x00400000
#define DMX_IDX_VC1_FIRST_SEQ_FRAME_END     0x00800000
#define DMX_IDX_VC1_FRAME_START             0x01000000
#define DMX_IDX_VC1_FRAME_END               0x02000000
#define DMX_IDX_H264_ACCESS_UNIT_DEL        0x04000000
#define DMX_IDX_H264_SEI                    0x08000000

struct dmx_pes_filter_params
{
	__u16          pid;
	dmx_input_t    input;
	dmx_output_t   output;
	dmx_pes_type_t pes_type;
	__u32          flags;

	__u32          rec_chunk_size;

	enum dmx_video_codec video_codec;
};

struct dmx_buffer_status {
	
	unsigned int size;

	
	unsigned int fullness;

	unsigned int free_bytes;

	
	unsigned int read_offset;

	
	unsigned int write_offset;

	
	int error;
};

enum dmx_event {
	
	DMX_EVENT_NEW_PES = 0x00000001,

	
	DMX_EVENT_NEW_SECTION = 0x00000002,

	
	DMX_EVENT_NEW_REC_CHUNK = 0x00000004,

	
	DMX_EVENT_NEW_PCR = 0x00000008,

	
	DMX_EVENT_BUFFER_OVERFLOW = 0x00000010,

	
	DMX_EVENT_SECTION_CRC_ERROR = 0x00000020,

	
	DMX_EVENT_EOS = 0x00000040,

	
	DMX_EVENT_NEW_ES_DATA = 0x00000080,

	
	DMX_EVENT_MARKER = 0x00000100,

	
	DMX_EVENT_NEW_INDEX_ENTRY = 0x00000200,

	DMX_EVENT_SECTION_TIMEOUT = 0x00000400,

	
	DMX_EVENT_SCRAMBLING_STATUS_CHANGE = 0x00000800
};

enum dmx_oob_cmd {
	
	DMX_OOB_CMD_EOS,

	
	DMX_OOB_CMD_MARKER,
};


#define DMX_FILTER_CC_ERROR			0x01

#define DMX_FILTER_DISCONTINUITY_INDICATOR	0x02

#define DMX_FILTER_PES_LENGTH_ERROR		0x04


struct dmx_pes_event_info {
	
	__u32 base_offset;

	__u32 start_offset;

	
	__u32 total_length;

	
	__u32 actual_length;

	
	__u64 stc;

	
	__u32 flags;

	__u32 transport_error_indicator_counter;

	
	__u32 continuity_error_counter;

	
	__u32 ts_packets_num;
};

struct dmx_section_event_info {
	
	__u32 base_offset;

	__u32 start_offset;

	
	__u32 total_length;

	
	__u32 actual_length;

	
	__u32 flags;
};

struct dmx_rec_chunk_event_info {
	
	__u32 offset;

	
	__u32 size;
};

struct dmx_pcr_event_info {
	__u64 stc;

	
	__u64 pcr;

	
	__u32 flags;
};

struct dmx_es_data_event_info {
	
	int buf_handle;

	int cookie;

	
	__u32 offset;

	
	__u32 data_len;

	
	int pts_valid;

	
	__u64 pts;

	
	int dts_valid;

	
	__u64 dts;

	
	__u64 stc;

	__u32 transport_error_indicator_counter;

	
	__u32 continuity_error_counter;

	
	__u32 ts_packets_num;

	__u32 ts_dropped_bytes;
};

struct dmx_marker_event_info {
	
	__u64 id;
};

struct dmx_index_event_info {
	
	__u64 type;

	__u16 pid;

	__u64 match_tsp_num;

	__u64 last_pusi_tsp_num;

	
	__u64 stc;
};

struct dmx_scrambling_status_event_info {
	__u16 pid;

	
	__u8 old_value;

	
	__u8 new_value;
};

struct dmx_filter_event {
	enum dmx_event type;

	union {
		struct dmx_pes_event_info pes;
		struct dmx_section_event_info section;
		struct dmx_rec_chunk_event_info recording_chunk;
		struct dmx_pcr_event_info pcr;
		struct dmx_es_data_event_info es_data;
		struct dmx_marker_event_info marker;
		struct dmx_index_event_info index;
		struct dmx_scrambling_status_event_info scrambling_status;
	} params;
};

struct dmx_buffer_requirement {
	
	__u32 size_alignment;

	/* Maximum buffer size allowed */
	__u32 max_size;

	
	__u32 max_buffer_num;

	
	__u32 flags;

#define DMX_BUFFER_CONTIGUOUS_MEM		0x1

#define DMX_BUFFER_SECURED_IF_DECRYPTED		0x2

#define DMX_BUFFER_EXTERNAL_SUPPORT		0x4

#define DMX_BUFFER_INTERNAL_SUPPORT		0x8

#define DMX_BUFFER_LINEAR_GROUP_SUPPORT		0x10
};

struct dmx_oob_command {
	enum dmx_oob_cmd type;

	union {
		struct dmx_marker_event_info marker;
	} params;
};

typedef struct dmx_caps {
	__u32 caps;

#define DMX_CAP_PULL_MODE				0x01

#define DMX_CAP_VIDEO_INDEXING			0x02

#define DMX_CAP_VIDEO_DECODER_DATA		0x04

#define DMX_CAP_AUDIO_DECODER_DATA		0x08

#define DMX_CAP_SUBTITLE_DECODER_DATA	0x10

#define DMX_CAP_TS_INSERTION	0x20

#define DMX_CAP_SECURED_INPUT_PLAYBACK	0x40

	
	int num_decoders;

	
	int num_demux_devices;

	
	int num_pid_filters;

	
	int num_section_filters;

	int num_section_filters_per_pid;

	int section_filter_length;

	
	int num_demod_inputs;

	
	int num_memory_inputs;

	
	int max_bitrate;

	
	int demod_input_max_bitrate;

	
	int memory_input_max_bitrate;

	
	int num_cipher_ops;

	
	__u64 max_stc;

	struct dmx_buffer_requirement section;

	
	struct dmx_buffer_requirement pes;

	
	struct dmx_buffer_requirement decoder;

	
	struct dmx_buffer_requirement recording_188_tsp;

	
	struct dmx_buffer_requirement recording_192_tsp;

	
	struct dmx_buffer_requirement playback_188_tsp;

	
	struct dmx_buffer_requirement playback_192_tsp;
} dmx_caps_t;

typedef enum {
	DMX_SOURCE_FRONT0 = 0,
	DMX_SOURCE_FRONT1,
	DMX_SOURCE_FRONT2,
	DMX_SOURCE_FRONT3,
	DMX_SOURCE_DVR0   = 16,
	DMX_SOURCE_DVR1,
	DMX_SOURCE_DVR2,
	DMX_SOURCE_DVR3
} dmx_source_t;

enum dmx_tsp_format_t {
	DMX_TSP_FORMAT_188 = 0,
	DMX_TSP_FORMAT_192_TAIL,
	DMX_TSP_FORMAT_192_HEAD,
	DMX_TSP_FORMAT_204,
};

enum dmx_playback_mode_t {
	DMX_PB_MODE_PUSH = 0,

	DMX_PB_MODE_PULL,
};

struct dmx_stc {
	unsigned int num; 
	unsigned int base; 
	__u64 stc; 
};

enum dmx_buffer_mode {
	DMX_BUFFER_MODE_INTERNAL,

	DMX_BUFFER_MODE_EXTERNAL,
};

struct dmx_buffer {
	unsigned int size;
	int handle;

	int is_protected;
};

struct dmx_decoder_buffers {
	int is_linear;

	__u32 buffers_num;

	
	__u32 buffers_size;

	
	int handles[DMX_MAX_DECODER_BUFFER_NUM];
};

struct dmx_secure_mode {
	int is_secured;
};

struct dmx_cipher_operation {
	
	int encrypt;

	
	__u32 key_ladder_id;
};

#define DMX_MAX_CIPHER_OPERATIONS_COUNT	5
struct dmx_cipher_operations {
	__u16 pid;

	
	__u8 operations_count;

	struct dmx_cipher_operation operations[DMX_MAX_CIPHER_OPERATIONS_COUNT];
};

struct dmx_events_mask {
	__u32 disable_mask;

	__u32 no_wakeup_mask;

	__u32 wakeup_threshold;
};

struct dmx_indexing_params {
	__u16 pid;

	
	int enable;

	
	__u64 types;
};

struct dmx_set_ts_insertion {
	__u32 identifier;

	__u32 repetition_time;

	const __u8 *ts_packets;

	size_t size;
};

struct dmx_abort_ts_insertion {
	__u32 identifier;
};

struct dmx_scrambling_bits {
	__u16 pid;

	
	__u8 value;
};

#define DMX_START                _IO('o', 41)
#define DMX_STOP                 _IO('o', 42)
#define DMX_SET_FILTER           _IOW('o', 43, struct dmx_sct_filter_params)
#define DMX_SET_PES_FILTER       _IOW('o', 44, struct dmx_pes_filter_params)
#define DMX_SET_BUFFER_SIZE      _IO('o', 45)
#define DMX_GET_PES_PIDS         _IOR('o', 47, __u16[5])
#define DMX_GET_CAPS             _IOR('o', 48, dmx_caps_t)
#define DMX_SET_SOURCE           _IOW('o', 49, dmx_source_t)
#define DMX_GET_STC              _IOWR('o', 50, struct dmx_stc)
#define DMX_ADD_PID              _IOW('o', 51, __u16)
#define DMX_REMOVE_PID           _IOW('o', 52, __u16)
#define DMX_SET_TS_PACKET_FORMAT _IOW('o', 53, enum dmx_tsp_format_t)
#define DMX_SET_TS_OUT_FORMAT	 _IOW('o', 54, enum dmx_tsp_format_t)
#define DMX_SET_DECODER_BUFFER_SIZE	_IO('o', 55)
#define DMX_GET_BUFFER_STATUS	 _IOR('o', 56, struct dmx_buffer_status)
#define DMX_RELEASE_DATA		 _IO('o', 57)
#define DMX_FEED_DATA			 _IO('o', 58)
#define DMX_SET_PLAYBACK_MODE	 _IOW('o', 59, enum dmx_playback_mode_t)
#define DMX_GET_EVENT		 _IOR('o', 60, struct dmx_filter_event)
#define DMX_SET_BUFFER_MODE	 _IOW('o', 61, enum dmx_buffer_mode)
#define DMX_SET_BUFFER		 _IOW('o', 62, struct dmx_buffer)
#define DMX_SET_DECODER_BUFFER	 _IOW('o', 63, struct dmx_decoder_buffers)
#define DMX_REUSE_DECODER_BUFFER _IO('o', 64)
#define DMX_SET_SECURE_MODE	_IOW('o', 65, struct dmx_secure_mode)
#define DMX_SET_EVENTS_MASK	_IOW('o', 66, struct dmx_events_mask)
#define DMX_GET_EVENTS_MASK	_IOR('o', 67, struct dmx_events_mask)
#define DMX_PUSH_OOB_COMMAND	_IOW('o', 68, struct dmx_oob_command)
#define DMX_SET_INDEXING_PARAMS _IOW('o', 69, struct dmx_indexing_params)
#define DMX_SET_TS_INSERTION _IOW('o', 70, struct dmx_set_ts_insertion)
#define DMX_ABORT_TS_INSERTION _IOW('o', 71, struct dmx_abort_ts_insertion)
#define DMX_GET_SCRAMBLING_BITS _IOWR('o', 72, struct dmx_scrambling_bits)
#define DMX_SET_CIPHER _IOW('o', 73, struct dmx_cipher_operations)


#endif 
