/*
 * Host Side support for RNDIS Networking Links
 * Copyright (C) 2005 by David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef	__LINUX_USB_RNDIS_HOST_H
#define	__LINUX_USB_RNDIS_HOST_H

struct rndis_msg_hdr {
	__le32	msg_type;			
	__le32	msg_len;
	
	__le32	request_id;
	__le32	status;
	
} __attribute__ ((packed));

#define	CONTROL_BUFFER_SIZE		1025

#define	RNDIS_CONTROL_TIMEOUT_MS	(5 * 1000)

#define RNDIS_MSG_COMPLETION	cpu_to_le32(0x80000000)

#define RNDIS_MSG_PACKET	cpu_to_le32(0x00000001)	
#define RNDIS_MSG_INIT		cpu_to_le32(0x00000002)
#define RNDIS_MSG_INIT_C	(RNDIS_MSG_INIT|RNDIS_MSG_COMPLETION)
#define RNDIS_MSG_HALT		cpu_to_le32(0x00000003)
#define RNDIS_MSG_QUERY		cpu_to_le32(0x00000004)
#define RNDIS_MSG_QUERY_C	(RNDIS_MSG_QUERY|RNDIS_MSG_COMPLETION)
#define RNDIS_MSG_SET		cpu_to_le32(0x00000005)
#define RNDIS_MSG_SET_C		(RNDIS_MSG_SET|RNDIS_MSG_COMPLETION)
#define RNDIS_MSG_RESET		cpu_to_le32(0x00000006)
#define RNDIS_MSG_RESET_C	(RNDIS_MSG_RESET|RNDIS_MSG_COMPLETION)
#define RNDIS_MSG_INDICATE	cpu_to_le32(0x00000007)
#define RNDIS_MSG_KEEPALIVE	cpu_to_le32(0x00000008)
#define RNDIS_MSG_KEEPALIVE_C	(RNDIS_MSG_KEEPALIVE|RNDIS_MSG_COMPLETION)

#define	RNDIS_STATUS_SUCCESS			cpu_to_le32(0x00000000)
#define	RNDIS_STATUS_FAILURE			cpu_to_le32(0xc0000001)
#define	RNDIS_STATUS_INVALID_DATA		cpu_to_le32(0xc0010015)
#define	RNDIS_STATUS_NOT_SUPPORTED		cpu_to_le32(0xc00000bb)
#define	RNDIS_STATUS_MEDIA_CONNECT		cpu_to_le32(0x4001000b)
#define	RNDIS_STATUS_MEDIA_DISCONNECT		cpu_to_le32(0x4001000c)
#define	RNDIS_STATUS_MEDIA_SPECIFIC_INDICATION	cpu_to_le32(0x40010012)

#define	RNDIS_PHYSICAL_MEDIUM_UNSPECIFIED	cpu_to_le32(0x00000000)
#define	RNDIS_PHYSICAL_MEDIUM_WIRELESS_LAN	cpu_to_le32(0x00000001)
#define	RNDIS_PHYSICAL_MEDIUM_CABLE_MODEM	cpu_to_le32(0x00000002)
#define	RNDIS_PHYSICAL_MEDIUM_PHONE_LINE	cpu_to_le32(0x00000003)
#define	RNDIS_PHYSICAL_MEDIUM_POWER_LINE	cpu_to_le32(0x00000004)
#define	RNDIS_PHYSICAL_MEDIUM_DSL		cpu_to_le32(0x00000005)
#define	RNDIS_PHYSICAL_MEDIUM_FIBRE_CHANNEL	cpu_to_le32(0x00000006)
#define	RNDIS_PHYSICAL_MEDIUM_1394		cpu_to_le32(0x00000007)
#define	RNDIS_PHYSICAL_MEDIUM_WIRELESS_WAN	cpu_to_le32(0x00000008)
#define	RNDIS_PHYSICAL_MEDIUM_MAX		cpu_to_le32(0x00000009)

struct rndis_data_hdr {
	__le32	msg_type;		
	__le32	msg_len;		
	__le32	data_offset;		
	__le32	data_len;		

	__le32	oob_data_offset;	
	__le32	oob_data_len;		
	__le32	num_oob;		
	__le32	packet_data_offset;	

	__le32	packet_data_len;	
	__le32	vc_handle;		
	__le32	reserved;		
} __attribute__ ((packed));

struct rndis_init {		
	
	__le32	msg_type;			
	__le32	msg_len;			
	__le32	request_id;
	__le32	major_version;			
	__le32	minor_version;
	__le32	max_transfer_size;
} __attribute__ ((packed));

struct rndis_init_c {		
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	request_id;
	__le32	status;
	__le32	major_version;			
	__le32	minor_version;
	__le32	device_flags;
	__le32	medium;				
	__le32	max_packets_per_message;
	__le32	max_transfer_size;
	__le32	packet_alignment;		
	__le32	af_list_offset;			
	__le32	af_list_size;			
} __attribute__ ((packed));

struct rndis_halt {		
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	request_id;
} __attribute__ ((packed));

struct rndis_query {		
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	request_id;
	__le32	oid;
	__le32	len;
	__le32	offset;
	__le32	handle;				
} __attribute__ ((packed));

struct rndis_query_c {		
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	request_id;
	__le32	status;
	__le32	len;
	__le32	offset;
} __attribute__ ((packed));

struct rndis_set {		
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	request_id;
	__le32	oid;
	__le32	len;
	__le32	offset;
	__le32	handle;				
} __attribute__ ((packed));

struct rndis_set_c {		
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	request_id;
	__le32	status;
} __attribute__ ((packed));

struct rndis_reset {		
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	reserved;
} __attribute__ ((packed));

struct rndis_reset_c {		
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	status;
	__le32	addressing_lost;
} __attribute__ ((packed));

struct rndis_indicate {		
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	status;
	__le32	length;
	__le32	offset;
	__le32	diag_status;
	__le32	error_offset;
	__le32	message;
} __attribute__ ((packed));

struct rndis_keepalive {	
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	request_id;
} __attribute__ ((packed));

struct rndis_keepalive_c {	
	
	__le32	msg_type;			
	__le32	msg_len;
	__le32	request_id;
	__le32	status;
} __attribute__ ((packed));

#define OID_802_3_PERMANENT_ADDRESS	cpu_to_le32(0x01010101)
#define OID_GEN_MAXIMUM_FRAME_SIZE	cpu_to_le32(0x00010106)
#define OID_GEN_CURRENT_PACKET_FILTER	cpu_to_le32(0x0001010e)
#define OID_GEN_PHYSICAL_MEDIUM		cpu_to_le32(0x00010202)

#define RNDIS_PACKET_TYPE_DIRECTED		cpu_to_le32(0x00000001)
#define RNDIS_PACKET_TYPE_MULTICAST		cpu_to_le32(0x00000002)
#define RNDIS_PACKET_TYPE_ALL_MULTICAST		cpu_to_le32(0x00000004)
#define RNDIS_PACKET_TYPE_BROADCAST		cpu_to_le32(0x00000008)
#define RNDIS_PACKET_TYPE_SOURCE_ROUTING	cpu_to_le32(0x00000010)
#define RNDIS_PACKET_TYPE_PROMISCUOUS		cpu_to_le32(0x00000020)
#define RNDIS_PACKET_TYPE_SMT			cpu_to_le32(0x00000040)
#define RNDIS_PACKET_TYPE_ALL_LOCAL		cpu_to_le32(0x00000080)
#define RNDIS_PACKET_TYPE_GROUP			cpu_to_le32(0x00001000)
#define RNDIS_PACKET_TYPE_ALL_FUNCTIONAL	cpu_to_le32(0x00002000)
#define RNDIS_PACKET_TYPE_FUNCTIONAL		cpu_to_le32(0x00004000)
#define RNDIS_PACKET_TYPE_MAC_FRAME		cpu_to_le32(0x00008000)

#define RNDIS_DEFAULT_FILTER ( \
	RNDIS_PACKET_TYPE_DIRECTED | \
	RNDIS_PACKET_TYPE_BROADCAST | \
	RNDIS_PACKET_TYPE_ALL_MULTICAST | \
	RNDIS_PACKET_TYPE_PROMISCUOUS)

#define FLAG_RNDIS_PHYM_NOT_WIRELESS	0x0001
#define FLAG_RNDIS_PHYM_WIRELESS	0x0002

#define RNDIS_DRIVER_DATA_POLL_STATUS	1	

extern void rndis_status(struct usbnet *dev, struct urb *urb);
extern int
rndis_command(struct usbnet *dev, struct rndis_msg_hdr *buf, int buflen);
extern int
generic_rndis_bind(struct usbnet *dev, struct usb_interface *intf, int flags);
extern void rndis_unbind(struct usbnet *dev, struct usb_interface *intf);
extern int rndis_rx_fixup(struct usbnet *dev, struct sk_buff *skb);
extern struct sk_buff *
rndis_tx_fixup(struct usbnet *dev, struct sk_buff *skb, gfp_t flags);

#endif	
