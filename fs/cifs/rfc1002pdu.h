/*
 *   fs/cifs/rfc1002pdu.h
 *
 *   Protocol Data Unit definitions for RFC 1001/1002 support
 *
 *   Copyright (c) International Business Machines  Corp., 2004
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


	
#define RFC1002_SESSION_MESSAGE 0x00
#define RFC1002_SESSION_REQUEST  0x81
#define RFC1002_POSITIVE_SESSION_RESPONSE 0x82
#define RFC1002_NEGATIVE_SESSION_RESPONSE 0x83
#define RFC1002_RETARGET_SESSION_RESPONSE 0x84
#define RFC1002_SESSION_KEEP_ALIVE 0x85

	
#define RFC1002_LENGTH_EXTEND 0x80 

struct rfc1002_session_packet {
	__u8	type;
	__u8	flags;
	__u16	length;
	union {
		struct {
			__u8 called_len;
			__u8 called_name[32];
			__u8 scope1; 
			__u8 calling_len;
			__u8 calling_name[32];
			__u8 scope2; 
		} __attribute__((packed)) session_req;
		struct {
			__u32 retarget_ip_addr;
			__u16 port;
		} __attribute__((packed)) retarget_resp;
		__u8 neg_ses_resp_error_code;
	} __attribute__((packed)) trailer;
} __attribute__((packed));

#define RFC1002_NOT_LISTENING_CALLED  0x80 
#define RFC1002_NOT_LISTENING_CALLING 0x81 
#define RFC1002_NOT_PRESENT           0x82 
#define RFC1002_INSUFFICIENT_RESOURCE 0x83
#define RFC1002_UNSPECIFIED_ERROR     0x8F


#define DEFAULT_CIFS_CALLED_NAME  "*SMBSERVER      "
