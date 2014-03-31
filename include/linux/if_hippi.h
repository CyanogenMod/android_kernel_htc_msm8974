/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Global definitions for the HIPPI interface.
 *
 * Version:	@(#)if_hippi.h	1.0.0	05/26/97
 *
 * Author:	Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Donald Becker, <becker@super.org>
 *		Alan Cox, <alan@lxorguk.ukuu.org.uk>
 *		Steve Whitehouse, <gw7rrm@eeshack3.swan.ac.uk>
 *		Jes Sorensen, <Jes.Sorensen@cern.ch>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
 
#ifndef _LINUX_IF_HIPPI_H
#define _LINUX_IF_HIPPI_H

#include <linux/types.h>
#include <asm/byteorder.h>


#define HIPPI_ALEN	6		
#define HIPPI_HLEN	sizeof(struct hippi_hdr)
#define HIPPI_ZLEN	0		
#define HIPPI_DATA_LEN	65280		
#define HIPPI_FRAME_LEN	(HIPPI_DATA_LEN + HIPPI_HLEN)
					

#define HIPPI_EXTENDED_SAP	0xAA
#define HIPPI_UI_CMD		0x03



 
struct hipnet_statistics {
	int	rx_packets;		
	int	tx_packets;		
	int	rx_errors;		
	int	tx_errors;		
	int	rx_dropped;		
	int	tx_dropped;		

	
	int	rx_length_errors;
	int	rx_over_errors;		
	int	rx_crc_errors;		
	int	rx_frame_errors;	
	int	rx_fifo_errors;		
	int	rx_missed_errors;	

	
	int	tx_aborted_errors;
	int	tx_carrier_errors;
	int	tx_fifo_errors;
	int	tx_heartbeat_errors;
	int	tx_window_errors;
};


struct hippi_fp_hdr {
#if 0
	__u8		ulp;				
#if defined (__BIG_ENDIAN_BITFIELD)
	__u8		d1_data_present:1;		
	__u8		start_d2_burst_boundary:1;	
	__u8		reserved:6;			
#if 0
	__u16		reserved1:5;
	__u16		d1_area_size:8;			
	__u16		d2_offset:3;			
#endif
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		reserved:6;			
	__u8	 	start_d2_burst_boundary:1;	
	__u8		d1_data_present:1;		
#if 0
	__u16		d2_offset:3;			
	__u16		d1_area_size:8;			
	__u16		reserved1:5;			
#endif
#else
#error	"Please fix <asm/byteorder.h>"
#endif
#else
	__be32		fixed;
#endif
	__be32		d2_size;
} __attribute__((packed));

struct hippi_le_hdr {
#if defined (__BIG_ENDIAN_BITFIELD)
	__u8		fc:3;
	__u8		double_wide:1;
	__u8		message_type:4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		message_type:4;
	__u8		double_wide:1;
	__u8		fc:3;
#endif
	__u8		dest_switch_addr[3];
#if defined (__BIG_ENDIAN_BITFIELD)
	__u8		dest_addr_type:4,
			src_addr_type:4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		src_addr_type:4,
			dest_addr_type:4;
#endif
	__u8		src_switch_addr[3];
	__u16		reserved;
	__u8		daddr[HIPPI_ALEN];
	__u16		locally_administered;
	__u8		saddr[HIPPI_ALEN];
} __attribute__((packed));

#define HIPPI_OUI_LEN	3
struct hippi_snap_hdr {
	__u8	dsap;			
	__u8	ssap;			
	__u8	ctrl;			
	__u8	oui[HIPPI_OUI_LEN];	
	__be16	ethertype;		
} __attribute__((packed));

struct hippi_hdr {
	struct hippi_fp_hdr	fp;
	struct hippi_le_hdr	le;
	struct hippi_snap_hdr	snap;
} __attribute__((packed));

#endif	
