/* packet.h: Rx packet layout and definitions
 *
 * Copyright (C) 2002, 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_RXRPC_PACKET_H
#define _LINUX_RXRPC_PACKET_H

typedef u32	rxrpc_seq_t;	
typedef u32	rxrpc_serial_t;	
typedef __be32	rxrpc_seq_net_t; 
typedef __be32	rxrpc_serial_net_t; 

struct rxrpc_header {
	__be32		epoch;		

	__be32		cid;		
#define RXRPC_MAXCALLS		4			
#define RXRPC_CHANNELMASK	(RXRPC_MAXCALLS-1)	
#define RXRPC_CIDMASK		(~RXRPC_CHANNELMASK)	
#define RXRPC_CIDSHIFT		ilog2(RXRPC_MAXCALLS)	
#define RXRPC_CID_INC		(1 << RXRPC_CIDSHIFT)	

	__be32		callNumber;	
#define RXRPC_PROCESS_MAXCALLS	(1<<2)	

	__be32		seq;		
	__be32		serial;		

	uint8_t		type;		
#define RXRPC_PACKET_TYPE_DATA		1	
#define RXRPC_PACKET_TYPE_ACK		2	
#define RXRPC_PACKET_TYPE_BUSY		3	
#define RXRPC_PACKET_TYPE_ABORT		4	
#define RXRPC_PACKET_TYPE_ACKALL	5	
#define RXRPC_PACKET_TYPE_CHALLENGE	6	
#define RXRPC_PACKET_TYPE_RESPONSE	7	
#define RXRPC_PACKET_TYPE_DEBUG		8	
#define RXRPC_N_PACKET_TYPES		9	

	uint8_t		flags;		
#define RXRPC_CLIENT_INITIATED	0x01		
#define RXRPC_REQUEST_ACK	0x02		
#define RXRPC_LAST_PACKET	0x04		
#define RXRPC_MORE_PACKETS	0x08		
#define RXRPC_JUMBO_PACKET	0x20		
#define RXRPC_SLOW_START_OK	0x20		

	uint8_t		userStatus;	
	uint8_t		securityIndex;	
	union {
		__be16	_rsvd;		
		__be16	cksum;		
	};
	__be16		serviceId;	

} __packed;

#define __rxrpc_header_off(X) offsetof(struct rxrpc_header,X)

extern const char *rxrpc_pkts[];

struct rxrpc_jumbo_header {
	uint8_t		flags;		
	uint8_t		pad;
	__be16		_rsvd;		
};

#define RXRPC_JUMBO_DATALEN	1412	

struct rxrpc_ackpacket {
	__be16		bufferSpace;	
	__be16		maxSkew;	
	__be32		firstPacket;	
	__be32		previousPacket;	
	__be32		serial;		

	uint8_t		reason;		
#define RXRPC_ACK_REQUESTED		1	
#define RXRPC_ACK_DUPLICATE		2	
#define RXRPC_ACK_OUT_OF_SEQUENCE	3	
#define RXRPC_ACK_EXCEEDS_WINDOW	4	
#define RXRPC_ACK_NOSPACE		5	
#define RXRPC_ACK_PING			6	
#define RXRPC_ACK_PING_RESPONSE		7	
#define RXRPC_ACK_DELAY			8	
#define RXRPC_ACK_IDLE			9	

	uint8_t		nAcks;		
#define RXRPC_MAXACKS	255

	uint8_t		acks[0];	
#define RXRPC_ACK_TYPE_NACK		0
#define RXRPC_ACK_TYPE_ACK		1

} __packed;

struct rxrpc_ackinfo {
	__be32		rxMTU;		
	__be32		maxMTU;		
	__be32		rwind;		
	__be32		jumbo_max;	
};

struct rxkad_challenge {
	__be32		version;	
	__be32		nonce;		
	__be32		min_level;	
	__be32		__padding;	
} __packed;

struct rxkad_response {
	__be32		version;	
	__be32		__pad;

	
	struct {
		__be32		epoch;		
		__be32		cid;		
		__be32		checksum;	
		__be32		securityIndex;	
		__be32		call_id[4];	
		__be32		inc_nonce;	
		__be32		level;		
	} encrypted;

	__be32		kvno;		
	__be32		ticket_len;	
} __packed;

#define RX_CALL_DEAD		-1	
#define RX_INVALID_OPERATION	-2	
#define RX_CALL_TIMEOUT		-3	
#define RX_EOF			-4	
#define RX_PROTOCOL_ERROR	-5	
#define RX_USER_ABORT		-6	
#define RX_ADDRINUSE		-7	
#define RX_DEBUGI_BADTYPE	-8	

#define	RXGEN_CC_MARSHAL    -450
#define	RXGEN_CC_UNMARSHAL  -451
#define	RXGEN_SS_MARSHAL    -452
#define	RXGEN_SS_UNMARSHAL  -453
#define	RXGEN_DECODE	    -454
#define	RXGEN_OPCODE	    -455
#define	RXGEN_SS_XDRFREE    -456
#define	RXGEN_CC_XDRFREE    -457

#define RXKADINCONSISTENCY	19270400	
#define RXKADPACKETSHORT	19270401	
#define RXKADLEVELFAIL		19270402	
#define RXKADTICKETLEN		19270403	
#define RXKADOUTOFSEQUENCE	19270404	
#define RXKADNOAUTH		19270405	
#define RXKADBADKEY		19270406	
#define RXKADBADTICKET		19270407	
#define RXKADUNKNOWNKEY		19270408	
#define RXKADEXPIRED		19270409	
#define RXKADSEALEDINCON	19270410	
#define RXKADDATALEN		19270411	
#define RXKADILLEGALLEVEL	19270412	

#endif 
