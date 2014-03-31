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
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 */
#ifndef __BFA_DEFS_CNA_H__
#define __BFA_DEFS_CNA_H__

#include "bfa_defs.h"

struct bfa_port_fc_stats {
	u64	secs_reset;	
	u64	tx_frames;	
	u64	tx_words;	
	u64	tx_lip;		
	u64	tx_nos;		
	u64	tx_ols;		
	u64	tx_lr;		
	u64	tx_lrr;		
	u64	rx_frames;	
	u64	rx_words;	
	u64	lip_count;	
	u64	nos_count;	
	u64	ols_count;	
	u64	lr_count;	
	u64	lrr_count;	
	u64	invalid_crcs;	
	u64	invalid_crc_gd_eof; 
	u64	undersized_frm; 
	u64	oversized_frm;	
	u64	bad_eof_frm;	
	u64	error_frames;	
	u64	dropped_frames;	
	u64	link_failures;	
	u64	loss_of_syncs;	
	u64	loss_of_signals; 
	u64	primseq_errs;	
	u64	bad_os_count;	
	u64	err_enc_out;	
	u64	err_enc;	
	u64	bbsc_frames_lost; 
	u64	bbsc_credits_lost; 
	u64	bbsc_link_resets; 
};

struct bfa_port_eth_stats {
	u64	secs_reset;	
	u64	frame_64;	
	u64	frame_65_127;	
	u64	frame_128_255;	
	u64	frame_256_511;	
	u64	frame_512_1023;	
	u64	frame_1024_1518; 
	u64	frame_1519_1522; 
	u64	tx_bytes;	
	u64	tx_packets;	 
	u64	tx_mcast_packets; 
	u64	tx_bcast_packets; 
	u64	tx_control_frame; 
	u64	tx_drop;	
	u64	tx_jabber;	
	u64	tx_fcs_error;	
	u64	tx_fragments;	
	u64	rx_bytes;	
	u64	rx_packets;	
	u64	rx_mcast_packets; 
	u64	rx_bcast_packets; 
	u64	rx_control_frames; 
	u64	rx_unknown_opcode; 
	u64	rx_drop;	
	u64	rx_jabber;	
	u64	rx_fcs_error;	
	u64	rx_alignment_error; 
	u64	rx_frame_length_error; 
	u64	rx_code_error;	
	u64	rx_fragments;	
	u64	rx_pause;	
	u64	rx_zero_pause;	
	u64	tx_pause;	
	u64	tx_zero_pause;	
	u64	rx_fcoe_pause;	
	u64	rx_fcoe_zero_pause; 
	u64	tx_fcoe_pause;	
	u64	tx_fcoe_zero_pause; 
	u64	rx_iscsi_pause;	
	u64	rx_iscsi_zero_pause; 
	u64	tx_iscsi_pause;	
	u64	tx_iscsi_zero_pause; 
};

union bfa_port_stats_u {
	struct bfa_port_fc_stats fc;
	struct bfa_port_eth_stats eth;
};

#pragma pack(1)

#define BFA_CEE_LLDP_MAX_STRING_LEN (128)
#define BFA_CEE_DCBX_MAX_PRIORITY	(8)
#define BFA_CEE_DCBX_MAX_PGID		(8)

#define BFA_CEE_LLDP_SYS_CAP_OTHER	0x0001
#define BFA_CEE_LLDP_SYS_CAP_REPEATER	0x0002
#define BFA_CEE_LLDP_SYS_CAP_MAC_BRIDGE	0x0004
#define BFA_CEE_LLDP_SYS_CAP_WLAN_AP	0x0008
#define BFA_CEE_LLDP_SYS_CAP_ROUTER	0x0010
#define BFA_CEE_LLDP_SYS_CAP_TELEPHONE	0x0020
#define BFA_CEE_LLDP_SYS_CAP_DOCSIS_CD	0x0040
#define BFA_CEE_LLDP_SYS_CAP_STATION	0x0080
#define BFA_CEE_LLDP_SYS_CAP_CVLAN	0x0100
#define BFA_CEE_LLDP_SYS_CAP_SVLAN	0x0200
#define BFA_CEE_LLDP_SYS_CAP_TPMR	0x0400

struct bfa_cee_lldp_str {
	u8 sub_type;
	u8 len;
	u8 rsvd[2];
	u8 value[BFA_CEE_LLDP_MAX_STRING_LEN];
};

struct bfa_cee_lldp_cfg {
	struct bfa_cee_lldp_str chassis_id;
	struct bfa_cee_lldp_str port_id;
	struct bfa_cee_lldp_str port_desc;
	struct bfa_cee_lldp_str sys_name;
	struct bfa_cee_lldp_str sys_desc;
	struct bfa_cee_lldp_str mgmt_addr;
	u16 time_to_live;
	u16 enabled_system_cap;
};

enum bfa_cee_dcbx_version {
	DCBX_PROTOCOL_PRECEE	= 1,
	DCBX_PROTOCOL_CEE	= 2,
};

enum bfa_cee_lls {
	
	CEE_LLS_DOWN_NO_TLV = 0,
	
	CEE_LLS_DOWN	= 1,
	CEE_LLS_UP	= 2,
};

struct bfa_cee_dcbx_cfg {
	u8 pgid[BFA_CEE_DCBX_MAX_PRIORITY];
	u8 pg_percentage[BFA_CEE_DCBX_MAX_PGID];
	u8 pfc_primap; 
	u8 fcoe_primap; 
	u8 iscsi_primap; 
	u8 dcbx_version; 
	u8 lls_fcoe; 
	u8 lls_lan; 
	u8 rsvd[2];
};

enum bfa_cee_status {
	CEE_UP = 0,
	CEE_PHY_UP = 1,
	CEE_LOOPBACK = 2,
	CEE_PHY_DOWN = 3,
};

struct bfa_cee_attr {
	u8	cee_status;
	u8 error_reason;
	struct bfa_cee_lldp_cfg lldp_remote;
	struct bfa_cee_dcbx_cfg dcbx_remote;
	mac_t src_mac;
	u8 link_speed;
	u8 nw_priority;
	u8 filler[2];
};

struct bfa_cee_stats {
	u32	lldp_tx_frames;		
	u32	lldp_rx_frames;		
	u32	lldp_rx_frames_invalid;	
	u32	lldp_rx_frames_new;	
	u32	lldp_tlvs_unrecognized;	
	u32	lldp_rx_shutdown_tlvs;	
	u32	lldp_info_aged_out;	
	u32	dcbx_phylink_ups;	
	u32	dcbx_phylink_downs;	
	u32	dcbx_rx_tlvs;		
	u32	dcbx_rx_tlvs_invalid;	
	u32	dcbx_control_tlv_error;	
	u32	dcbx_feature_tlv_error;	
	u32	dcbx_cee_cfg_new;	
	u32	cee_status_down;	
	u32	cee_status_up;		
	u32	cee_hw_cfg_changed;	
	u32	cee_rx_invalid_cfg;	
};

#pragma pack()

#endif	
