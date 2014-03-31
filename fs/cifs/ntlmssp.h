/*
 *   fs/cifs/ntlmssp.h
 *
 *   Copyright (c) International Business Machines  Corp., 2002,2007
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

#define NTLMSSP_SIGNATURE "NTLMSSP"
#define NtLmNegotiate     cpu_to_le32(1)
#define NtLmChallenge     cpu_to_le32(2)
#define NtLmAuthenticate  cpu_to_le32(3)
#define UnknownMessage    cpu_to_le32(8)

#define NTLMSSP_NEGOTIATE_UNICODE         0x01 
#define NTLMSSP_NEGOTIATE_OEM             0x02 
#define NTLMSSP_REQUEST_TARGET            0x04 
#define NTLMSSP_NEGOTIATE_SIGN          0x0010 
#define NTLMSSP_NEGOTIATE_SEAL          0x0020 
#define NTLMSSP_NEGOTIATE_DGRAM         0x0040
#define NTLMSSP_NEGOTIATE_LM_KEY        0x0080 
#define NTLMSSP_NEGOTIATE_NTLM          0x0200 
#define NTLMSSP_NEGOTIATE_NT_ONLY       0x0400 
#define NTLMSSP_ANONYMOUS               0x0800
#define NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED 0x1000 
#define NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED 0x2000
#define NTLMSSP_NEGOTIATE_LOCAL_CALL    0x4000 
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN   0x8000 
#define NTLMSSP_TARGET_TYPE_DOMAIN     0x10000
#define NTLMSSP_TARGET_TYPE_SERVER     0x20000
#define NTLMSSP_TARGET_TYPE_SHARE      0x40000
#define NTLMSSP_NEGOTIATE_EXTENDED_SEC 0x80000 
#define NTLMSSP_NEGOTIATE_IDENTIFY    0x100000
#define NTLMSSP_REQUEST_ACCEPT_RESP   0x200000 
#define NTLMSSP_REQUEST_NON_NT_KEY    0x400000
#define NTLMSSP_NEGOTIATE_TARGET_INFO 0x800000
#define NTLMSSP_NEGOTIATE_VERSION    0x2000000 
#define NTLMSSP_NEGOTIATE_128       0x20000000
#define NTLMSSP_NEGOTIATE_KEY_XCH   0x40000000
#define NTLMSSP_NEGOTIATE_56        0x80000000

enum av_field_type {
	NTLMSSP_AV_EOL = 0,
	NTLMSSP_AV_NB_COMPUTER_NAME,
	NTLMSSP_AV_NB_DOMAIN_NAME,
	NTLMSSP_AV_DNS_COMPUTER_NAME,
	NTLMSSP_AV_DNS_DOMAIN_NAME,
	NTLMSSP_AV_DNS_TREE_NAME,
	NTLMSSP_AV_FLAGS,
	NTLMSSP_AV_TIMESTAMP,
	NTLMSSP_AV_RESTRICTION,
	NTLMSSP_AV_TARGET_NAME,
	NTLMSSP_AV_CHANNEL_BINDINGS
};


typedef struct _SECURITY_BUFFER {
	__le16 Length;
	__le16 MaximumLength;
	__le32 BufferOffset;	
} __attribute__((packed)) SECURITY_BUFFER;

typedef struct _NEGOTIATE_MESSAGE {
	__u8 Signature[sizeof(NTLMSSP_SIGNATURE)];
	__le32 MessageType;     
	__le32 NegotiateFlags;
	SECURITY_BUFFER DomainName;	
	SECURITY_BUFFER WorkstationName;	
	char DomainString[0];
	
} __attribute__((packed)) NEGOTIATE_MESSAGE, *PNEGOTIATE_MESSAGE;

typedef struct _CHALLENGE_MESSAGE {
	__u8 Signature[sizeof(NTLMSSP_SIGNATURE)];
	__le32 MessageType;   
	SECURITY_BUFFER TargetName;
	__le32 NegotiateFlags;
	__u8 Challenge[CIFS_CRYPTO_KEY_SIZE];
	__u8 Reserved[8];
	SECURITY_BUFFER TargetInfoArray;
} __attribute__((packed)) CHALLENGE_MESSAGE, *PCHALLENGE_MESSAGE;

typedef struct _AUTHENTICATE_MESSAGE {
	__u8 Signature[sizeof(NTLMSSP_SIGNATURE)];
	__le32 MessageType;  
	SECURITY_BUFFER LmChallengeResponse;
	SECURITY_BUFFER NtChallengeResponse;
	SECURITY_BUFFER DomainName;
	SECURITY_BUFFER UserName;
	SECURITY_BUFFER WorkstationName;
	SECURITY_BUFFER SessionKey;
	__le32 NegotiateFlags;
	char UserString[0];
} __attribute__((packed)) AUTHENTICATE_MESSAGE, *PAUTHENTICATE_MESSAGE;
