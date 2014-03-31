/*
 * Linux network driver for Brocade Converged Network Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
/*
 * Copyright (c) 2005-2011 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 */


/**
 * Skipping statistics collection to avoid clutter.
 * Command is no longer needed:
 *	MTU
 *	TxQ Stop
 *	RxQ Stop
 *	RxF Enable/Disable
 *
 * HDS-off request is dynamic
 * keep structures as multiple of 32-bit fields for alignment.
 * All values must be written in big-endian.
 */
#ifndef __BFI_ENET_H__
#define __BFI_ENET_H__

#include "bfa_defs.h"
#include "bfi.h"

#pragma pack(1)

#define BFI_ENET_CFG_MAX		32	

#define BFI_ENET_TXQ_PRIO_MAX		8
#define BFI_ENET_RX_QSET_MAX		16
#define BFI_ENET_TXQ_WI_VECT_MAX	4

#define BFI_ENET_VLAN_ID_MAX		4096
#define BFI_ENET_VLAN_BLOCK_SIZE	512	
#define BFI_ENET_VLAN_BLOCKS_MAX					\
	(BFI_ENET_VLAN_ID_MAX / BFI_ENET_VLAN_BLOCK_SIZE)
#define BFI_ENET_VLAN_WORD_SIZE		32	
#define BFI_ENET_VLAN_WORDS_MAX						\
	(BFI_ENET_VLAN_BLOCK_SIZE / BFI_ENET_VLAN_WORD_SIZE)

#define BFI_ENET_RSS_RIT_MAX		64	
#define BFI_ENET_RSS_KEY_LEN		10	

union bfi_addr_be_u {
	struct {
		u32	addr_hi;	
		u32	addr_lo;	
	} a32;
};

#define BFI_ENET_TXQ_WI_SEND		(0x402)	
#define BFI_ENET_TXQ_WI_SEND_LSO	(0x403)	
#define BFI_ENET_TXQ_WI_EXTENSION	(0x104)	

#define BFI_ENET_TXQ_WI_CF_FCOE_CRC	(1 << 8)
#define BFI_ENET_TXQ_WI_CF_IPID_MODE	(1 << 5)
#define BFI_ENET_TXQ_WI_CF_INS_PRIO	(1 << 4)
#define BFI_ENET_TXQ_WI_CF_INS_VLAN	(1 << 3)
#define BFI_ENET_TXQ_WI_CF_UDP_CKSUM	(1 << 2)
#define BFI_ENET_TXQ_WI_CF_TCP_CKSUM	(1 << 1)
#define BFI_ENET_TXQ_WI_CF_IP_CKSUM	(1 << 0)

struct bfi_enet_txq_wi_base {
	u8			reserved;
	u8			num_vectors;	
	u16			opcode;
			
	u16			flags;		
	u16			l4_hdr_size_n_offset;
	u16			vlan_tag;
	u16			lso_mss;	
	u32			frame_length;	
};

struct bfi_enet_txq_wi_ext {
	u16			reserved;
	u16			opcode;		
	u32			reserved2[3];
};

struct bfi_enet_txq_wi_vector {			
	u16			reserved;
	u16			length;		
	union bfi_addr_be_u	addr;
};

struct bfi_enet_txq_entry {
	union {
		struct bfi_enet_txq_wi_base	base;
		struct bfi_enet_txq_wi_ext	ext;
	} wi;
	struct bfi_enet_txq_wi_vector vector[BFI_ENET_TXQ_WI_VECT_MAX];
};

#define wi_hdr		wi.base
#define wi_ext_hdr	wi.ext

#define BFI_ENET_TXQ_WI_L4_HDR_N_OFFSET(_hdr_size, _offset) \
		(((_hdr_size) << 10) | ((_offset) & 0x3FF))

struct bfi_enet_rxq_entry {
	union bfi_addr_be_u  rx_buffer;
};

#define	BFI_ENET_CQ_EF_MAC_ERROR	(1 <<  0)
#define	BFI_ENET_CQ_EF_FCS_ERROR	(1 <<  1)
#define	BFI_ENET_CQ_EF_TOO_LONG		(1 <<  2)
#define	BFI_ENET_CQ_EF_FC_CRC_OK	(1 <<  3)

#define	BFI_ENET_CQ_EF_RSVD1		(1 <<  4)
#define	BFI_ENET_CQ_EF_L4_CKSUM_OK	(1 <<  5)
#define	BFI_ENET_CQ_EF_L3_CKSUM_OK	(1 <<  6)
#define	BFI_ENET_CQ_EF_HDS_HEADER	(1 <<  7)

#define	BFI_ENET_CQ_EF_UDP		(1 <<  8)
#define	BFI_ENET_CQ_EF_TCP		(1 <<  9)
#define	BFI_ENET_CQ_EF_IP_OPTIONS	(1 << 10)
#define	BFI_ENET_CQ_EF_IPV6		(1 << 11)

#define	BFI_ENET_CQ_EF_IPV4		(1 << 12)
#define	BFI_ENET_CQ_EF_VLAN		(1 << 13)
#define	BFI_ENET_CQ_EF_RSS		(1 << 14)
#define	BFI_ENET_CQ_EF_RSVD2		(1 << 15)

#define	BFI_ENET_CQ_EF_MCAST_MATCH	(1 << 16)
#define	BFI_ENET_CQ_EF_MCAST		(1 << 17)
#define BFI_ENET_CQ_EF_BCAST		(1 << 18)
#define	BFI_ENET_CQ_EF_REMOTE		(1 << 19)

#define	BFI_ENET_CQ_EF_LOCAL		(1 << 20)

struct bfi_enet_cq_entry {
	u32 flags;
	u16	vlan_tag;
	u16	length;
	u32	rss_hash;
	u8	valid;
	u8	reserved1;
	u8	reserved2;
	u8	rxq_id;
};

struct bfi_enet_q {
	union bfi_addr_u	pg_tbl;
	union bfi_addr_u	first_entry;
	u16		pages;	
	u16		page_sz;
};

struct bfi_enet_txq {
	struct bfi_enet_q	q;
	u8			priority;
	u8			rsvd[3];
};

struct bfi_enet_rxq {
	struct bfi_enet_q	q;
	u16		rx_buffer_size;
	u16		rsvd;
};

struct bfi_enet_cq {
	struct bfi_enet_q	q;
};

struct bfi_enet_ib_cfg {
	u8		int_pkt_dma;
	u8		int_enabled;
	u8		int_pkt_enabled;
	u8		continuous_coalescing;
	u8		msix;
	u8		rsvd[3];
	u32	coalescing_timeout;
	u32	inter_pkt_timeout;
	u8		inter_pkt_count;
	u8		rsvd1[3];
};

struct bfi_enet_ib {
	union bfi_addr_u	index_addr;
	union {
		u16	msix_index;
		u16	intx_bitmask;
	} intr;
	u16		rsvd;
};

enum bfi_enet_h2i_msgs {
	
	BFI_ENET_H2I_RX_CFG_SET_REQ = 1,
	BFI_ENET_H2I_RX_CFG_CLR_REQ = 2,

	BFI_ENET_H2I_RIT_CFG_REQ = 3,
	BFI_ENET_H2I_RSS_CFG_REQ = 4,
	BFI_ENET_H2I_RSS_ENABLE_REQ = 5,
	BFI_ENET_H2I_RX_PROMISCUOUS_REQ = 6,
	BFI_ENET_H2I_RX_DEFAULT_REQ = 7,

	BFI_ENET_H2I_MAC_UCAST_SET_REQ = 8,
	BFI_ENET_H2I_MAC_UCAST_CLR_REQ = 9,
	BFI_ENET_H2I_MAC_UCAST_ADD_REQ = 10,
	BFI_ENET_H2I_MAC_UCAST_DEL_REQ = 11,

	BFI_ENET_H2I_MAC_MCAST_ADD_REQ = 12,
	BFI_ENET_H2I_MAC_MCAST_DEL_REQ = 13,
	BFI_ENET_H2I_MAC_MCAST_FILTER_REQ = 14,

	BFI_ENET_H2I_RX_VLAN_SET_REQ = 15,
	BFI_ENET_H2I_RX_VLAN_STRIP_ENABLE_REQ = 16,

	
	BFI_ENET_H2I_TX_CFG_SET_REQ = 17,
	BFI_ENET_H2I_TX_CFG_CLR_REQ = 18,

	
	BFI_ENET_H2I_PORT_ADMIN_UP_REQ = 19,
	BFI_ENET_H2I_SET_PAUSE_REQ = 20,
	BFI_ENET_H2I_DIAG_LOOPBACK_REQ = 21,

	
	BFI_ENET_H2I_GET_ATTR_REQ = 22,

	
	BFI_ENET_H2I_STATS_GET_REQ = 23,
	BFI_ENET_H2I_STATS_CLR_REQ = 24,

	BFI_ENET_H2I_WOL_MAGIC_REQ = 25,
	BFI_ENET_H2I_WOL_FRAME_REQ = 26,

	BFI_ENET_H2I_MAX = 27,
};

enum bfi_enet_i2h_msgs {
	
	BFI_ENET_I2H_RX_CFG_SET_RSP =
		BFA_I2HM(BFI_ENET_H2I_RX_CFG_SET_REQ),
	BFI_ENET_I2H_RX_CFG_CLR_RSP =
		BFA_I2HM(BFI_ENET_H2I_RX_CFG_CLR_REQ),

	BFI_ENET_I2H_RIT_CFG_RSP =
		BFA_I2HM(BFI_ENET_H2I_RIT_CFG_REQ),
	BFI_ENET_I2H_RSS_CFG_RSP =
		BFA_I2HM(BFI_ENET_H2I_RSS_CFG_REQ),
	BFI_ENET_I2H_RSS_ENABLE_RSP =
		BFA_I2HM(BFI_ENET_H2I_RSS_ENABLE_REQ),
	BFI_ENET_I2H_RX_PROMISCUOUS_RSP =
		BFA_I2HM(BFI_ENET_H2I_RX_PROMISCUOUS_REQ),
	BFI_ENET_I2H_RX_DEFAULT_RSP =
		BFA_I2HM(BFI_ENET_H2I_RX_DEFAULT_REQ),

	BFI_ENET_I2H_MAC_UCAST_SET_RSP =
		BFA_I2HM(BFI_ENET_H2I_MAC_UCAST_SET_REQ),
	BFI_ENET_I2H_MAC_UCAST_CLR_RSP =
		BFA_I2HM(BFI_ENET_H2I_MAC_UCAST_CLR_REQ),
	BFI_ENET_I2H_MAC_UCAST_ADD_RSP =
		BFA_I2HM(BFI_ENET_H2I_MAC_UCAST_ADD_REQ),
	BFI_ENET_I2H_MAC_UCAST_DEL_RSP =
		BFA_I2HM(BFI_ENET_H2I_MAC_UCAST_DEL_REQ),

	BFI_ENET_I2H_MAC_MCAST_ADD_RSP =
		BFA_I2HM(BFI_ENET_H2I_MAC_MCAST_ADD_REQ),
	BFI_ENET_I2H_MAC_MCAST_DEL_RSP =
		BFA_I2HM(BFI_ENET_H2I_MAC_MCAST_DEL_REQ),
	BFI_ENET_I2H_MAC_MCAST_FILTER_RSP =
		BFA_I2HM(BFI_ENET_H2I_MAC_MCAST_FILTER_REQ),

	BFI_ENET_I2H_RX_VLAN_SET_RSP =
		BFA_I2HM(BFI_ENET_H2I_RX_VLAN_SET_REQ),

	BFI_ENET_I2H_RX_VLAN_STRIP_ENABLE_RSP =
		BFA_I2HM(BFI_ENET_H2I_RX_VLAN_STRIP_ENABLE_REQ),

	
	BFI_ENET_I2H_TX_CFG_SET_RSP =
		BFA_I2HM(BFI_ENET_H2I_TX_CFG_SET_REQ),
	BFI_ENET_I2H_TX_CFG_CLR_RSP =
		BFA_I2HM(BFI_ENET_H2I_TX_CFG_CLR_REQ),

	
	BFI_ENET_I2H_PORT_ADMIN_RSP =
		BFA_I2HM(BFI_ENET_H2I_PORT_ADMIN_UP_REQ),

	BFI_ENET_I2H_SET_PAUSE_RSP =
		BFA_I2HM(BFI_ENET_H2I_SET_PAUSE_REQ),
	BFI_ENET_I2H_DIAG_LOOPBACK_RSP =
		BFA_I2HM(BFI_ENET_H2I_DIAG_LOOPBACK_REQ),

	
	BFI_ENET_I2H_GET_ATTR_RSP =
		BFA_I2HM(BFI_ENET_H2I_GET_ATTR_REQ),

	
	BFI_ENET_I2H_STATS_GET_RSP =
		BFA_I2HM(BFI_ENET_H2I_STATS_GET_REQ),
	BFI_ENET_I2H_STATS_CLR_RSP =
		BFA_I2HM(BFI_ENET_H2I_STATS_CLR_REQ),

	BFI_ENET_I2H_WOL_MAGIC_RSP =
		BFA_I2HM(BFI_ENET_H2I_WOL_MAGIC_REQ),
	BFI_ENET_I2H_WOL_FRAME_RSP =
		BFA_I2HM(BFI_ENET_H2I_WOL_FRAME_REQ),

	
	BFI_ENET_I2H_LINK_DOWN_AEN = BFA_I2HM(BFI_ENET_H2I_MAX),
	BFI_ENET_I2H_LINK_UP_AEN = BFA_I2HM(BFI_ENET_H2I_MAX + 1),

	BFI_ENET_I2H_PORT_ENABLE_AEN = BFA_I2HM(BFI_ENET_H2I_MAX + 2),
	BFI_ENET_I2H_PORT_DISABLE_AEN = BFA_I2HM(BFI_ENET_H2I_MAX + 3),

	BFI_ENET_I2H_BW_UPDATE_AEN = BFA_I2HM(BFI_ENET_H2I_MAX + 4),
};

enum bfi_enet_err {
	BFI_ENET_CMD_OK		= 0,
	BFI_ENET_CMD_FAIL	= 1,
	BFI_ENET_CMD_DUP_ENTRY	= 2,	
	BFI_ENET_CMD_CAM_FULL	= 3,	
	BFI_ENET_CMD_NOT_OWNER	= 4,	
	BFI_ENET_CMD_NOT_EXEC	= 5,	
	BFI_ENET_CMD_WAITING	= 6,	
	BFI_ENET_CMD_PORT_DISABLED = 7,	
};

struct bfi_enet_req {
	struct bfi_msgq_mhdr mh;
};

struct bfi_enet_enable_req {
	struct		bfi_msgq_mhdr mh;
	u8		enable;		
	u8		rsvd[3];
};

struct bfi_enet_rsp {
	struct bfi_msgq_mhdr mh;
	u8		error;		
	u8		rsvd;
	u16		cmd_offset;	
};


struct bfi_enet_attr_req {
	struct bfi_msgq_mhdr	mh;
};

struct bfi_enet_attr_rsp {
	struct bfi_msgq_mhdr mh;
	u8		error;		
	u8		rsvd;
	u16		cmd_offset;	
	u32		max_cfg;
	u32		max_ucmac;
	u32		rit_size;
};

enum bfi_enet_tx_vlan_mode {
	BFI_ENET_TX_VLAN_NOP	= 0,
	BFI_ENET_TX_VLAN_INS	= 1,
	BFI_ENET_TX_VLAN_WI	= 2,
};

struct bfi_enet_tx_cfg {
	u8		vlan_mode;	
	u8		rsvd;
	u16		vlan_id;
	u8		admit_tagged_frame;
	u8		apply_vlan_filter;
	u8		add_to_vswitch;
	u8		rsvd1[1];
};

struct bfi_enet_tx_cfg_req {
	struct bfi_msgq_mhdr mh;
	u8			num_queues;	
	u8			rsvd[3];

	struct {
		struct bfi_enet_txq	q;
		struct bfi_enet_ib	ib;
	} q_cfg[BFI_ENET_TXQ_PRIO_MAX];

	struct bfi_enet_ib_cfg	ib_cfg;

	struct bfi_enet_tx_cfg	tx_cfg;
};

struct bfi_enet_tx_cfg_rsp {
	struct		bfi_msgq_mhdr mh;
	u8		error;
	u8		hw_id;		
	u8		rsvd[2];
	struct {
		u32	q_dbell;	
		u32	i_dbell;	
		u8	hw_qid;		
		u8	rsvd[3];
	} q_handles[BFI_ENET_TXQ_PRIO_MAX];
};

enum bfi_enet_rxq_type {
	BFI_ENET_RXQ_SINGLE		= 1,
	BFI_ENET_RXQ_LARGE_SMALL	= 2,
	BFI_ENET_RXQ_HDS		= 3,
	BFI_ENET_RXQ_HDS_OPT_BASED	= 4,
};

enum bfi_enet_hds_type {
	BFI_ENET_HDS_FORCED	= 0x01,
	BFI_ENET_HDS_IPV6_UDP	= 0x02,
	BFI_ENET_HDS_IPV6_TCP	= 0x04,
	BFI_ENET_HDS_IPV4_TCP	= 0x08,
	BFI_ENET_HDS_IPV4_UDP	= 0x10,
};

struct bfi_enet_rx_cfg {
	u8		rxq_type;
	u8		rsvd[3];

	struct {
		u8			max_header_size;
		u8			force_offset;
		u8			type;
		u8			rsvd1;
	} hds;

	u8		multi_buffer;
	u8		strip_vlan;
	u8		drop_untagged;
	u8		rsvd2;
};

struct bfi_enet_rx_cfg_req {
	struct bfi_msgq_mhdr mh;
	u8			num_queue_sets;	
	u8			rsvd[3];

	struct {
		struct bfi_enet_rxq	ql;	
		struct bfi_enet_rxq	qs;	
		struct bfi_enet_cq	cq;
		struct bfi_enet_ib	ib;
	} q_cfg[BFI_ENET_RX_QSET_MAX];

	struct bfi_enet_ib_cfg	ib_cfg;

	struct bfi_enet_rx_cfg	rx_cfg;
};

struct bfi_enet_rx_cfg_rsp {
	struct bfi_msgq_mhdr mh;
	u8		error;
	u8		hw_id;	 
	u8		rsvd[2];
	struct {
		u32	ql_dbell; 
		u32	qs_dbell; 
		u32	i_dbell;  
		u8		hw_lqid;  
		u8		hw_sqid;  
		u8		hw_cqid;  
		u8		rsvd;
	} q_handles[BFI_ENET_RX_QSET_MAX];
};

struct bfi_enet_rit_req {
	struct	bfi_msgq_mhdr mh;
	u16	size;			
	u8	rsvd[2];
	u8	table[BFI_ENET_RSS_RIT_MAX];
};

enum bfi_enet_rss_type {
	BFI_ENET_RSS_IPV6	= 0x01,
	BFI_ENET_RSS_IPV6_TCP	= 0x02,
	BFI_ENET_RSS_IPV4	= 0x04,
	BFI_ENET_RSS_IPV4_TCP	= 0x08
};

struct bfi_enet_rss_cfg {
	u8	type;
	u8	mask;
	u8	rsvd[2];
	u32	key[BFI_ENET_RSS_KEY_LEN];
};

struct bfi_enet_rss_cfg_req {
	struct bfi_msgq_mhdr	mh;
	struct bfi_enet_rss_cfg	cfg;
};

struct bfi_enet_ucast_req {
	struct bfi_msgq_mhdr	mh;
	mac_t			mac_addr;
	u8			rsvd[2];
};

struct bfi_enet_mac_n_vlan_req {
	struct bfi_msgq_mhdr	mh;
	u16			vlan_id;
	mac_t			mac_addr;
};

struct bfi_enet_mcast_add_req {
	struct bfi_msgq_mhdr	mh;
	mac_t			mac_addr;
	u8			rsvd[2];
};

struct bfi_enet_mcast_add_rsp {
	struct bfi_msgq_mhdr	mh;
	u8			error;
	u8			rsvd;
	u16			cmd_offset;
	u16			handle;
	u8			rsvd1[2];
};

struct bfi_enet_mcast_del_req {
	struct bfi_msgq_mhdr	mh;
	u16			handle;
	u8			rsvd[2];
};

struct bfi_enet_rx_vlan_req {
	struct bfi_msgq_mhdr	mh;
	u8			block_idx;
	u8			rsvd[3];
	u32			bit_mask[BFI_ENET_VLAN_WORDS_MAX];
};

struct bfi_enet_set_pause_req {
	struct bfi_msgq_mhdr	mh;
	u8			rsvd[2];
	u8			tx_pause;	
	u8			rx_pause;	
};

struct bfi_enet_diag_lb_req {
	struct bfi_msgq_mhdr	mh;
	u8			rsvd[2];
	u8			mode;		
	u8			enable;		
};

enum {
	BFI_ENET_DIAG_LB_OPMODE_EXT = 0,
	BFI_ENET_DIAG_LB_OPMODE_CBL = 1,
};

struct bfi_enet_stats_req {
	struct bfi_msgq_mhdr	mh;
	u16			stats_mask;
	u8			rsvd[2];
	u32			rx_enet_mask;
	u32			tx_enet_mask;
	union bfi_addr_u	host_buffer;
};

#define BFI_ENET_STATS_MAC    (1 << 0)    
#define BFI_ENET_STATS_BPC    (1 << 1)    
#define BFI_ENET_STATS_RAD    (1 << 2)    
#define BFI_ENET_STATS_RX_FC  (1 << 3)    
#define BFI_ENET_STATS_TX_FC  (1 << 4)    

#define BFI_ENET_STATS_ALL    0x1f

struct bfi_enet_stats_txf {
	u64 ucast_octets;
	u64 ucast;
	u64 ucast_vlan;

	u64 mcast_octets;
	u64 mcast;
	u64 mcast_vlan;

	u64 bcast_octets;
	u64 bcast;
	u64 bcast_vlan;

	u64 errors;
	u64 filter_vlan;      
	u64 filter_mac_sa;    
};

struct bfi_enet_stats_rxf {
	u64 ucast_octets;
	u64 ucast;
	u64 ucast_vlan;

	u64 mcast_octets;
	u64 mcast;
	u64 mcast_vlan;

	u64 bcast_octets;
	u64 bcast;
	u64 bcast_vlan;
	u64 frame_drops;
};

struct bfi_enet_stats_fc_tx {
	u64 txf_ucast_octets;
	u64 txf_ucast;
	u64 txf_ucast_vlan;

	u64 txf_mcast_octets;
	u64 txf_mcast;
	u64 txf_mcast_vlan;

	u64 txf_bcast_octets;
	u64 txf_bcast;
	u64 txf_bcast_vlan;

	u64 txf_parity_errors;
	u64 txf_timeout;
	u64 txf_fid_parity_errors;
};

struct bfi_enet_stats_fc_rx {
	u64 rxf_ucast_octets;
	u64 rxf_ucast;
	u64 rxf_ucast_vlan;

	u64 rxf_mcast_octets;
	u64 rxf_mcast;
	u64 rxf_mcast_vlan;

	u64 rxf_bcast_octets;
	u64 rxf_bcast;
	u64 rxf_bcast_vlan;
};

struct bfi_enet_stats_rad {
	u64 rx_frames;
	u64 rx_octets;
	u64 rx_vlan_frames;

	u64 rx_ucast;
	u64 rx_ucast_octets;
	u64 rx_ucast_vlan;

	u64 rx_mcast;
	u64 rx_mcast_octets;
	u64 rx_mcast_vlan;

	u64 rx_bcast;
	u64 rx_bcast_octets;
	u64 rx_bcast_vlan;

	u64 rx_drops;
};

struct bfi_enet_stats_bpc {
	
	u64 tx_pause[8];
	u64 tx_zero_pause[8];	
	
	u64 tx_first_pause[8];

	
	u64 rx_pause[8];
	u64 rx_zero_pause[8];	
	
	u64 rx_first_pause[8];
};

struct bfi_enet_stats_mac {
	u64 frame_64;		
	u64 frame_65_127;		
	u64 frame_128_255;		
	u64 frame_256_511;		
	u64 frame_512_1023;	
	u64 frame_1024_1518;	
	u64 frame_1519_1522;	

	
	u64 rx_bytes;
	u64 rx_packets;
	u64 rx_fcs_error;
	u64 rx_multicast;
	u64 rx_broadcast;
	u64 rx_control_frames;
	u64 rx_pause;
	u64 rx_unknown_opcode;
	u64 rx_alignment_error;
	u64 rx_frame_length_error;
	u64 rx_code_error;
	u64 rx_carrier_sense_error;
	u64 rx_undersize;
	u64 rx_oversize;
	u64 rx_fragments;
	u64 rx_jabber;
	u64 rx_drop;

	
	u64 tx_bytes;
	u64 tx_packets;
	u64 tx_multicast;
	u64 tx_broadcast;
	u64 tx_pause;
	u64 tx_deferral;
	u64 tx_excessive_deferral;
	u64 tx_single_collision;
	u64 tx_muliple_collision;
	u64 tx_late_collision;
	u64 tx_excessive_collision;
	u64 tx_total_collision;
	u64 tx_pause_honored;
	u64 tx_drop;
	u64 tx_jabber;
	u64 tx_fcs_error;
	u64 tx_control_frame;
	u64 tx_oversize;
	u64 tx_undersize;
	u64 tx_fragments;
};

struct bfi_enet_stats {
	struct bfi_enet_stats_mac	mac_stats;
	struct bfi_enet_stats_bpc	bpc_stats;
	struct bfi_enet_stats_rad	rad_stats;
	struct bfi_enet_stats_rad	rlb_stats;
	struct bfi_enet_stats_fc_rx	fc_rx_stats;
	struct bfi_enet_stats_fc_tx	fc_tx_stats;
	struct bfi_enet_stats_rxf	rxf_stats[BFI_ENET_CFG_MAX];
	struct bfi_enet_stats_txf	txf_stats[BFI_ENET_CFG_MAX];
};

#pragma pack()

#endif  
