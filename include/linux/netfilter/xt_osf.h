/*
 * Copyright (c) 2003+ Evgeniy Polyakov <johnpol@2ka.mxt.ru>
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _XT_OSF_H
#define _XT_OSF_H

#include <linux/types.h>

#define MAXGENRELEN		32

#define XT_OSF_GENRE		(1<<0)
#define	XT_OSF_TTL		(1<<1)
#define XT_OSF_LOG		(1<<2)
#define XT_OSF_INVERT		(1<<3)

#define XT_OSF_LOGLEVEL_ALL	0	
#define XT_OSF_LOGLEVEL_FIRST	1	
#define XT_OSF_LOGLEVEL_ALL_KNOWN	2 

#define XT_OSF_TTL_TRUE		0	
#define XT_OSF_TTL_LESS		1	
#define XT_OSF_TTL_NOCHECK	2	

struct xt_osf_info {
	char			genre[MAXGENRELEN];
	__u32			len;
	__u32			flags;
	__u32			loglevel;
	__u32			ttl;
};

struct xt_osf_wc {
	__u32			wc;
	__u32			val;
};

struct xt_osf_opt {
	__u16			kind, length;
	struct xt_osf_wc	wc;
};

struct xt_osf_user_finger {
	struct xt_osf_wc	wss;

	__u8			ttl, df;
	__u16			ss, mss;
	__u16			opt_num;

	char			genre[MAXGENRELEN];
	char			version[MAXGENRELEN];
	char			subtype[MAXGENRELEN];

	
	struct xt_osf_opt	opt[MAX_IPOPTLEN];
};

struct xt_osf_nlmsg {
	struct xt_osf_user_finger	f;
	struct iphdr		ip;
	struct tcphdr		tcp;
};


enum iana_options {
	OSFOPT_EOL = 0,		
	OSFOPT_NOP, 		
	OSFOPT_MSS, 		
	OSFOPT_WSO, 		
	OSFOPT_SACKP,		
	OSFOPT_SACK,		
	OSFOPT_ECHO,
	OSFOPT_ECHOREPLY,
	OSFOPT_TS,		
	OSFOPT_POCP,		
	OSFOPT_POSP,		

	
	OSFOPT_EMPTY = 255,
};

enum xt_osf_window_size_options {
	OSF_WSS_PLAIN	= 0,
	OSF_WSS_MSS,
	OSF_WSS_MTU,
	OSF_WSS_MODULO,
	OSF_WSS_MAX,
};

enum xt_osf_msg_types {
	OSF_MSG_ADD,
	OSF_MSG_REMOVE,
	OSF_MSG_MAX,
};

enum xt_osf_attr_type {
	OSF_ATTR_UNSPEC,
	OSF_ATTR_FINGER,
	OSF_ATTR_MAX,
};

#endif				
