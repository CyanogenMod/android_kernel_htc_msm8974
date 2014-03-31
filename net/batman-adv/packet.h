/*
 * Copyright (C) 2007-2012 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner, Simon Wunderlich
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#ifndef _NET_BATMAN_ADV_PACKET_H_
#define _NET_BATMAN_ADV_PACKET_H_

#define ETH_P_BATMAN  0x4305	

enum bat_packettype {
	BAT_OGM		 = 0x01,
	BAT_ICMP	 = 0x02,
	BAT_UNICAST	 = 0x03,
	BAT_BCAST	 = 0x04,
	BAT_VIS		 = 0x05,
	BAT_UNICAST_FRAG = 0x06,
	BAT_TT_QUERY	 = 0x07,
	BAT_ROAM_ADV	 = 0x08
};

#define COMPAT_VERSION 14

enum batman_flags {
	PRIMARIES_FIRST_HOP = 1 << 4,
	VIS_SERVER	    = 1 << 5,
	DIRECTLINK	    = 1 << 6
};

enum icmp_packettype {
	ECHO_REPLY		= 0,
	DESTINATION_UNREACHABLE = 3,
	ECHO_REQUEST		= 8,
	TTL_EXCEEDED		= 11,
	PARAMETER_PROBLEM	= 12
};

enum vis_packettype {
	VIS_TYPE_SERVER_SYNC   = 0,
	VIS_TYPE_CLIENT_UPDATE = 1
};

enum unicast_frag_flags {
	UNI_FRAG_HEAD	   = 1 << 0,
	UNI_FRAG_LARGETAIL = 1 << 1
};

#define TT_QUERY_TYPE_MASK 0x3

enum tt_query_packettype {
	TT_REQUEST    = 0,
	TT_RESPONSE   = 1
};

enum tt_query_flags {
	TT_FULL_TABLE = 1 << 2
};

enum tt_client_flags {
	TT_CLIENT_DEL     = 1 << 0,
	TT_CLIENT_ROAM    = 1 << 1,
	TT_CLIENT_WIFI    = 1 << 2,
	TT_CLIENT_NOPURGE = 1 << 8,
	TT_CLIENT_NEW     = 1 << 9,
	TT_CLIENT_PENDING = 1 << 10
};

struct batman_header {
	uint8_t  packet_type;
	uint8_t  version;  
	uint8_t  ttl;
} __packed;

struct batman_ogm_packet {
	struct batman_header header;
	uint8_t  flags;    
	uint32_t seqno;
	uint8_t  orig[6];
	uint8_t  prev_sender[6];
	uint8_t  gw_flags;  
	uint8_t  tq;
	uint8_t  tt_num_changes;
	uint8_t  ttvn; 
	uint16_t tt_crc;
} __packed;

#define BATMAN_OGM_LEN sizeof(struct batman_ogm_packet)

struct icmp_packet {
	struct batman_header header;
	uint8_t  msg_type; 
	uint8_t  dst[6];
	uint8_t  orig[6];
	uint16_t seqno;
	uint8_t  uid;
	uint8_t  reserved;
} __packed;

#define BAT_RR_LEN 16

struct icmp_packet_rr {
	struct batman_header header;
	uint8_t  msg_type; 
	uint8_t  dst[6];
	uint8_t  orig[6];
	uint16_t seqno;
	uint8_t  uid;
	uint8_t  rr_cur;
	uint8_t  rr[BAT_RR_LEN][ETH_ALEN];
} __packed;

struct unicast_packet {
	struct batman_header header;
	uint8_t  ttvn; 
	uint8_t  dest[6];
} __packed;

struct unicast_frag_packet {
	struct batman_header header;
	uint8_t  ttvn; 
	uint8_t  dest[6];
	uint8_t  flags;
	uint8_t  align;
	uint8_t  orig[6];
	uint16_t seqno;
} __packed;

struct bcast_packet {
	struct batman_header header;
	uint8_t  reserved;
	uint32_t seqno;
	uint8_t  orig[6];
} __packed;

struct vis_packet {
	struct batman_header header;
	uint8_t  vis_type;	 
	uint32_t seqno;		 
	uint8_t  entries;	 
	uint8_t  reserved;
	uint8_t  vis_orig[6];	 
	uint8_t  target_orig[6]; 
	uint8_t  sender_orig[6]; 
} __packed;

struct tt_query_packet {
	struct batman_header header;
	uint8_t  flags;
	uint8_t  dst[ETH_ALEN];
	uint8_t  src[ETH_ALEN];
	uint8_t  ttvn;
	uint16_t tt_data;
} __packed;

struct roam_adv_packet {
	struct batman_header header;
	uint8_t  reserved;
	uint8_t  dst[ETH_ALEN];
	uint8_t  src[ETH_ALEN];
	uint8_t  client[ETH_ALEN];
} __packed;

struct tt_change {
	uint8_t flags;
	uint8_t addr[ETH_ALEN];
} __packed;

#endif 
