/* AF_RXRPC parameters
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_RXRPC_H
#define _LINUX_RXRPC_H

#include <linux/in.h>
#include <linux/in6.h>

struct sockaddr_rxrpc {
	sa_family_t	srx_family;	
	u16		srx_service;	
	u16		transport_type;	
	u16		transport_len;	
	union {
		sa_family_t family;		
		struct sockaddr_in sin;		
		struct sockaddr_in6 sin6;	
	} transport;
};

#define RXRPC_SECURITY_KEY		1	
#define RXRPC_SECURITY_KEYRING		2	
#define RXRPC_EXCLUSIVE_CONNECTION	3	
#define RXRPC_MIN_SECURITY_LEVEL	4	

#define RXRPC_USER_CALL_ID	1	
#define RXRPC_ABORT		2	
#define RXRPC_ACK		3	
#define RXRPC_NET_ERROR		5	
#define RXRPC_BUSY		6	
#define RXRPC_LOCAL_ERROR	7	
#define RXRPC_NEW_CALL		8	
#define RXRPC_ACCEPT		9	

#define RXRPC_SECURITY_PLAIN	0	
#define RXRPC_SECURITY_AUTH	1	
#define RXRPC_SECURITY_ENCRYPT	2	

#define RXRPC_SECURITY_NONE	0	
#define RXRPC_SECURITY_RXKAD	2	
#define RXRPC_SECURITY_RXGK	4	
#define RXRPC_SECURITY_RXK5	5	

#endif 
