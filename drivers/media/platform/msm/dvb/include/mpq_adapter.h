/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef _MPQ_ADAPTER_H
#define _MPQ_ADAPTER_H

#include "dvbdev.h"
#include "dvb_demux.h"
#include "mpq_stream_buffer.h"



enum mpq_adapter_stream_if {
	
	MPQ_ADAPTER_VIDEO0_STREAM_IF = 0,

	
	MPQ_ADAPTER_VIDEO1_STREAM_IF = 1,

	
	MPQ_ADAPTER_VIDEO2_STREAM_IF = 2,

	
	MPQ_ADAPTER_VIDEO3_STREAM_IF = 3,

	
	MPQ_ADAPTER_MAX_NUM_OF_INTERFACES,
};

enum dmx_packet_type {
	DMX_PES_PACKET,
	DMX_FRAMING_INFO_PACKET,
	DMX_EOS_PACKET,
	DMX_MARKER_PACKET
};

struct dmx_pts_dts_info {
	
	int pts_exist;

	
	int dts_exist;

	
	u64 pts;

	
	u64 dts;
};

struct dmx_framing_packet_info {
	
	u64 pattern_type;

	
	struct dmx_pts_dts_info pts_dts_info;

	
	u64 stc;

	__u32 transport_error_indicator_counter;

	
	__u32 continuity_error_counter;

	__u32 ts_dropped_bytes;

	
	__u32 ts_packets_num;
};

struct dmx_pes_packet_info {
	
	struct dmx_pts_dts_info pts_dts_info;

	
	u64 stc;
};

struct dmx_marker_info {
	
	u64 id;
};

struct mpq_adapter_video_meta_data {
	
	enum dmx_packet_type packet_type;

	
	union {
		struct dmx_framing_packet_info framing;
		struct dmx_pes_packet_info pes;
		struct dmx_marker_info marker;
	} info;
} __packed;


typedef void (*mpq_adapter_stream_if_callback)(
				enum mpq_adapter_stream_if interface_id,
				void *user_param);


struct dvb_adapter *mpq_adapter_get(void);


int mpq_adapter_register_stream_if(
		enum mpq_adapter_stream_if interface_id,
		struct mpq_streambuffer *stream_buffer);


int mpq_adapter_unregister_stream_if(
		enum mpq_adapter_stream_if interface_id);


int mpq_adapter_get_stream_if(
		enum mpq_adapter_stream_if interface_id,
		struct mpq_streambuffer **stream_buffer);


int mpq_adapter_notify_stream_if(
		enum mpq_adapter_stream_if interface_id,
		mpq_adapter_stream_if_callback callback,
		void *user_param);

#endif 

