/*
 * include/linux/tipc_config.h: Include file for TIPC configuration interface
 *
 * Copyright (c) 2003-2006, Ericsson AB
 * Copyright (c) 2005-2007, 2010-2011, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_TIPC_CONFIG_H_
#define _LINUX_TIPC_CONFIG_H_

#include <linux/types.h>
#include <linux/string.h>
#include <asm/byteorder.h>

#ifndef __KERNEL__
#include <arpa/inet.h> 
#endif



#define  TIPC_CMD_NOOP              0x0000    
#define  TIPC_CMD_GET_NODES         0x0001    
#define  TIPC_CMD_GET_MEDIA_NAMES   0x0002    
#define  TIPC_CMD_GET_BEARER_NAMES  0x0003    
#define  TIPC_CMD_GET_LINKS         0x0004    
#define  TIPC_CMD_SHOW_NAME_TABLE   0x0005    
#define  TIPC_CMD_SHOW_PORTS        0x0006    
#define  TIPC_CMD_SHOW_LINK_STATS   0x000B    
#define  TIPC_CMD_SHOW_STATS        0x000F    


#define  TIPC_CMD_GET_REMOTE_MNG    0x4003    
#define  TIPC_CMD_GET_MAX_PORTS     0x4004    
#define  TIPC_CMD_GET_MAX_PUBL      0x4005    
#define  TIPC_CMD_GET_MAX_SUBSCR    0x4006    
#define  TIPC_CMD_GET_MAX_ZONES     0x4007    
#define  TIPC_CMD_GET_MAX_CLUSTERS  0x4008    
#define  TIPC_CMD_GET_MAX_NODES     0x4009    
#define  TIPC_CMD_GET_MAX_SLAVES    0x400A    
#define  TIPC_CMD_GET_NETID         0x400B    

#define  TIPC_CMD_ENABLE_BEARER     0x4101    
#define  TIPC_CMD_DISABLE_BEARER    0x4102    
#define  TIPC_CMD_SET_LINK_TOL      0x4107    
#define  TIPC_CMD_SET_LINK_PRI      0x4108    
#define  TIPC_CMD_SET_LINK_WINDOW   0x4109    
#define  TIPC_CMD_SET_LOG_SIZE      0x410A    
#define  TIPC_CMD_DUMP_LOG          0x410B    
#define  TIPC_CMD_RESET_LINK_STATS  0x410C    


#define  TIPC_CMD_SET_NODE_ADDR     0x8001    
#define  TIPC_CMD_SET_REMOTE_MNG    0x8003    
#define  TIPC_CMD_SET_MAX_PORTS     0x8004    
#define  TIPC_CMD_SET_MAX_PUBL      0x8005    
#define  TIPC_CMD_SET_MAX_SUBSCR    0x8006    
#define  TIPC_CMD_SET_MAX_ZONES     0x8007    
#define  TIPC_CMD_SET_MAX_CLUSTERS  0x8008    
#define  TIPC_CMD_SET_MAX_NODES     0x8009    
#define  TIPC_CMD_SET_MAX_SLAVES    0x800A    
#define  TIPC_CMD_SET_NETID         0x800B    


#define  TIPC_CMD_NOT_NET_ADMIN     0xC001    


#define TIPC_TLV_NONE		0	
#define TIPC_TLV_VOID		1	
#define TIPC_TLV_UNSIGNED	2	
#define TIPC_TLV_STRING		3	
#define TIPC_TLV_LARGE_STRING	4	
#define TIPC_TLV_ULTRA_STRING	5	

#define TIPC_TLV_ERROR_STRING	16	
#define TIPC_TLV_NET_ADDR	17	
#define TIPC_TLV_MEDIA_NAME	18	
#define TIPC_TLV_BEARER_NAME	19	
#define TIPC_TLV_LINK_NAME	20	
#define TIPC_TLV_NODE_INFO	21	
#define TIPC_TLV_LINK_INFO	22	
#define TIPC_TLV_BEARER_CONFIG	23	
#define TIPC_TLV_LINK_CONFIG	24	
#define TIPC_TLV_NAME_TBL_QUERY	25	
#define TIPC_TLV_PORT_REF	26	


#define TIPC_MAX_MEDIA_NAME	16	
#define TIPC_MAX_IF_NAME	16	
#define TIPC_MAX_BEARER_NAME	32	
#define TIPC_MAX_LINK_NAME	60	


#define TIPC_MIN_LINK_PRI	0
#define TIPC_DEF_LINK_PRI	10
#define TIPC_MAX_LINK_PRI	31
#define TIPC_MEDIA_LINK_PRI	(TIPC_MAX_LINK_PRI + 1)


#define TIPC_MIN_LINK_TOL 50
#define TIPC_DEF_LINK_TOL 1500
#define TIPC_MAX_LINK_TOL 30000

#if (TIPC_MIN_LINK_TOL < 16)
#error "TIPC_MIN_LINK_TOL is too small (abort limit may be NaN)"
#endif


#define TIPC_MIN_LINK_WIN 16
#define TIPC_DEF_LINK_WIN 50
#define TIPC_MAX_LINK_WIN 150


struct tipc_node_info {
	__be32 addr;			
	__be32 up;			
};

struct tipc_link_info {
	__be32 dest;			
	__be32 up;			
	char str[TIPC_MAX_LINK_NAME];	
};

struct tipc_bearer_config {
	__be32 priority;		
	__be32 disc_domain;		
	char name[TIPC_MAX_BEARER_NAME];
};

struct tipc_link_config {
	__be32 value;
	char name[TIPC_MAX_LINK_NAME];
};

#define TIPC_NTQ_ALLTYPES 0x80000000

struct tipc_name_table_query {
	__be32 depth;	
	__be32 type;	
	__be32 lowbound; 
	__be32 upbound;
};


#define TIPC_CFG_TLV_ERROR      "\x80"  
#define TIPC_CFG_NOT_NET_ADMIN  "\x81"	
#define TIPC_CFG_NOT_ZONE_MSTR	"\x82"	
#define TIPC_CFG_NO_REMOTE	"\x83"	
#define TIPC_CFG_NOT_SUPPORTED  "\x84"	
#define TIPC_CFG_INVALID_VALUE  "\x85"  


struct tlv_desc {
	__be16 tlv_len;		
	__be16 tlv_type;		
};

#define TLV_ALIGNTO 4

#define TLV_ALIGN(datalen) (((datalen)+(TLV_ALIGNTO-1)) & ~(TLV_ALIGNTO-1))
#define TLV_LENGTH(datalen) (sizeof(struct tlv_desc) + (datalen))
#define TLV_SPACE(datalen) (TLV_ALIGN(TLV_LENGTH(datalen)))
#define TLV_DATA(tlv) ((void *)((char *)(tlv) + TLV_LENGTH(0)))

static inline int TLV_OK(const void *tlv, __u16 space)
{

	return (space >= TLV_SPACE(0)) &&
		(ntohs(((struct tlv_desc *)tlv)->tlv_len) <= space);
}

static inline int TLV_CHECK(const void *tlv, __u16 space, __u16 exp_type)
{
	return TLV_OK(tlv, space) &&
		(ntohs(((struct tlv_desc *)tlv)->tlv_type) == exp_type);
}

static inline int TLV_SET(void *tlv, __u16 type, void *data, __u16 len)
{
	struct tlv_desc *tlv_ptr;
	int tlv_len;

	tlv_len = TLV_LENGTH(len);
	tlv_ptr = (struct tlv_desc *)tlv;
	tlv_ptr->tlv_type = htons(type);
	tlv_ptr->tlv_len  = htons(tlv_len);
	if (len && data)
		memcpy(TLV_DATA(tlv_ptr), data, tlv_len);
	return TLV_SPACE(len);
}


struct tlv_list_desc {
	struct tlv_desc *tlv_ptr;	
	__u32 tlv_space;		
};

static inline void TLV_LIST_INIT(struct tlv_list_desc *list,
				 void *data, __u32 space)
{
	list->tlv_ptr = (struct tlv_desc *)data;
	list->tlv_space = space;
}

static inline int TLV_LIST_EMPTY(struct tlv_list_desc *list)
{
	return (list->tlv_space == 0);
}

static inline int TLV_LIST_CHECK(struct tlv_list_desc *list, __u16 exp_type)
{
	return TLV_CHECK(list->tlv_ptr, list->tlv_space, exp_type);
}

static inline void *TLV_LIST_DATA(struct tlv_list_desc *list)
{
	return TLV_DATA(list->tlv_ptr);
}

static inline void TLV_LIST_STEP(struct tlv_list_desc *list)
{
	__u16 tlv_space = TLV_ALIGN(ntohs(list->tlv_ptr->tlv_len));

	list->tlv_ptr = (struct tlv_desc *)((char *)list->tlv_ptr + tlv_space);
	list->tlv_space -= tlv_space;
}

#define TIPC_GENL_NAME		"TIPC"
#define TIPC_GENL_VERSION	0x1
#define TIPC_GENL_CMD		0x1

struct tipc_genlmsghdr {
	__u32 dest;		
	__u16 cmd;		
	__u16 reserved;		
};

#define TIPC_GENL_HDRLEN	NLMSG_ALIGN(sizeof(struct tipc_genlmsghdr))


struct tipc_cfg_msg_hdr {
	__be32 tcm_len;		
	__be16 tcm_type;	
	__be16 tcm_flags;	
	char  tcm_reserved[8];	
};

#define TCM_F_REQUEST	0x1	
#define TCM_F_MORE	0x2	

#define TCM_ALIGN(datalen)  (((datalen)+3) & ~3)
#define TCM_LENGTH(datalen) (sizeof(struct tipc_cfg_msg_hdr) + datalen)
#define TCM_SPACE(datalen)  (TCM_ALIGN(TCM_LENGTH(datalen)))
#define TCM_DATA(tcm_hdr)   ((void *)((char *)(tcm_hdr) + TCM_LENGTH(0)))

static inline int TCM_SET(void *msg, __u16 cmd, __u16 flags,
			  void *data, __u16 data_len)
{
	struct tipc_cfg_msg_hdr *tcm_hdr;
	int msg_len;

	msg_len = TCM_LENGTH(data_len);
	tcm_hdr = (struct tipc_cfg_msg_hdr *)msg;
	tcm_hdr->tcm_len   = htonl(msg_len);
	tcm_hdr->tcm_type  = htons(cmd);
	tcm_hdr->tcm_flags = htons(flags);
	if (data_len && data)
		memcpy(TCM_DATA(msg), data, data_len);
	return TCM_SPACE(data_len);
}

#endif
