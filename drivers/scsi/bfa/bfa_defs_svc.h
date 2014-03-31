/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 *
 * Linux driver for Brocade Fibre Channel Host Bus Adapter.
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

#ifndef __BFA_DEFS_SVC_H__
#define __BFA_DEFS_SVC_H__

#include "bfa_defs.h"
#include "bfa_fc.h"
#include "bfi.h"

#define BFA_IOCFC_INTR_DELAY	1125
#define BFA_IOCFC_INTR_LATENCY	225
#define BFA_IOCFCOE_INTR_DELAY	25
#define BFA_IOCFCOE_INTR_LATENCY 5

#pragma pack(1)
struct bfa_iocfc_intr_attr_s {
	u8		coalesce;	
	u8		rsvd[3];
	__be16		latency;	
	__be16		delay;		
};

struct bfa_iocfc_fwcfg_s {
	u16		num_fabrics;	
	u16		num_lports;	
	u16		num_rports;	
	u16		num_ioim_reqs;	
	u16		num_tskim_reqs;	
	u16		num_fwtio_reqs;	
	u16		num_fcxp_reqs;	
	u16		num_uf_bufs;	
	u8		num_cqs;
	u8		fw_tick_res;	
	u8		rsvd[6];
};
#pragma pack()

struct bfa_iocfc_drvcfg_s {
	u16		num_reqq_elems;	
	u16		num_rspq_elems;	
	u16		num_sgpgs;	
	u16		num_sboot_tgts;	
	u16		num_sboot_luns;	
	u16		ioc_recover;	
	u16		min_cfg;	
	u16		path_tov;	
	u16		num_tio_reqs;	
	u8		port_mode;
	u8		rsvd_a;
	bfa_boolean_t	delay_comp;	
	u16		num_ttsk_reqs;	 
	u32		rsvd;
};

struct bfa_iocfc_cfg_s {
	struct bfa_iocfc_fwcfg_s	fwcfg;	
	struct bfa_iocfc_drvcfg_s	drvcfg;	
};

struct bfa_fw_ioim_stats_s {
	u32	host_abort;		
	u32	host_cleanup;		

	u32	fw_io_timeout;		
	u32	fw_frm_parse;		
	u32	fw_frm_data;		
	u32	fw_frm_rsp;		
	u32	fw_frm_xfer_rdy;	
	u32	fw_frm_bls_acc;		
	u32	fw_frm_tgt_abort;	
	u32	fw_frm_unknown;		
	u32	fw_data_dma;		
	u32	fw_frm_drop;		

	u32	rec_timeout;		
	u32	error_rec;		
	u32	wait_for_si;		
	u32	rec_rsp_inval;		
	u32	seqr_io_abort;		
	u32	seqr_io_retry;		

	u32	itn_cisc_upd_rsp;	
	u32	itn_cisc_upd_data;	
	u32	itn_cisc_upd_xfer_rdy;	

	u32	fcp_data_lost;		

	u32	ro_set_in_xfer_rdy;	
	u32	xfer_rdy_ooo_err;	
	u32	xfer_rdy_unknown_err;	

	u32	io_abort_timeout;	
	u32	sler_initiated;		

	u32	unexp_fcp_rsp;		

	u32	fcp_rsp_under_run;	
	u32     fcp_rsp_under_run_wr;   
	u32	fcp_rsp_under_run_err;	
	u32     fcp_rsp_resid_inval;    
	u32	fcp_rsp_over_run;	
	u32	fcp_rsp_over_run_err;	
	u32	fcp_rsp_proto_err;	
	u32	fcp_rsp_sense_err;	
	u32	fcp_conf_req;		

	u32	tgt_aborted_io;		

	u32	ioh_edtov_timeout_event;
	u32	ioh_fcp_rsp_excp_event;	
	u32	ioh_fcp_conf_event;	
	u32	ioh_mult_frm_rsp_event;	
	u32	ioh_hit_class2_event;	
	u32	ioh_miss_other_event;	
	u32	ioh_seq_cnt_err_event;	
	u32	ioh_len_err_event;	
	u32	ioh_seq_len_err_event;	
	u32	ioh_data_oor_event;	
	u32	ioh_ro_ooo_event;	
	u32	ioh_cpu_owned_event;	
	u32	ioh_unexp_frame_event;	
	u32	ioh_err_int;		
};

struct bfa_fw_tio_stats_s {
	u32	tio_conf_proc;	    
	u32	tio_conf_drop;      
	u32	tio_cleanup_req;    
	u32	tio_cleanup_comp;   
	u32	tio_abort_rsp;      
	u32	tio_abort_rsp_comp; 
	u32	tio_abts_req;       
	u32	tio_abts_ack;       
	u32	tio_abts_ack_nocomp;
	u32	tio_abts_tmo;       
	u32	tio_snsdata_dma;    
	u32	tio_rxwchan_wait;   
	u32	tio_rxwchan_avail;  
	u32	tio_hit_bls;        
	u32	tio_uf_recv;        
	u32	tio_rd_invalid_sm;  
	u32	tio_wr_invalid_sm;  

	u32	ds_rxwchan_wait;    
	u32	ds_rxwchan_avail;   
	u32	ds_unaligned_rd;    
	u32	ds_rdcomp_invalid_sm; 
	u32	ds_wrcomp_invalid_sm; 
	u32	ds_flush_req;       
	u32	ds_flush_comp;      
	u32	ds_xfrdy_exp;       
	u32	ds_seq_cnt_err;     
	u32	ds_seq_len_err;     
	u32	ds_data_oor;        
	u32	ds_hit_bls;	    
	u32	ds_edtov_timer_exp; 
	u32	ds_cpu_owned;       
	u32	ds_hit_class2;      
	u32	ds_length_err;      
	u32	ds_ro_ooo_err;      
	u32	ds_rectov_timer_exp;
	u32	ds_unexp_fr_err;    
};

struct bfa_fw_io_stats_s {
	struct bfa_fw_ioim_stats_s	ioim_stats;
	struct bfa_fw_tio_stats_s	tio_stats;
};


struct bfa_fw_port_fpg_stats_s {
	u32    intr_evt;
	u32    intr;
	u32    intr_excess;
	u32    intr_cause0;
	u32    intr_other;
	u32    intr_other_ign;
	u32    sig_lost;
	u32    sig_regained;
	u32    sync_lost;
	u32    sync_to;
	u32    sync_regained;
	u32    div2_overflow;
	u32    div2_underflow;
	u32    efifo_overflow;
	u32    efifo_underflow;
	u32    idle_rx;
	u32    lrr_rx;
	u32    lr_rx;
	u32    ols_rx;
	u32    nos_rx;
	u32    lip_rx;
	u32    arbf0_rx;
	u32    arb_rx;
	u32    mrk_rx;
	u32    const_mrk_rx;
	u32    prim_unknown;
};


struct bfa_fw_port_lksm_stats_s {
	u32    hwsm_success;       
	u32    hwsm_fails;         
	u32    hwsm_wdtov;         
	u32    swsm_success;       
	u32    swsm_fails;         
	u32    swsm_wdtov;         
	u32    busybufs;           
	u32    buf_waits;          
	u32    link_fails;         
	u32    psp_errors;         
	u32    lr_unexp;           
	u32    lrr_unexp;          
	u32    lr_tx;              
	u32    lrr_tx;             
	u32    ols_tx;             
	u32    nos_tx;             
	u32    hwsm_lrr_rx;        
	u32    hwsm_lr_rx;         
	u32    bbsc_lr;		   
};

struct bfa_fw_port_snsm_stats_s {
	u32    hwsm_success;       
	u32    hwsm_fails;         
	u32    hwsm_wdtov;         
	u32    swsm_success;       
	u32    swsm_wdtov;         
	u32    error_resets;       
	u32    sync_lost;          
	u32    sig_lost;           
	u32    asn8g_attempts;	   
};

struct bfa_fw_port_physm_stats_s {
	u32    module_inserts;     
	u32    module_xtracts;     
	u32    module_invalids;    
	u32    module_read_ign;    
	u32    laser_faults;       
	u32    rsvd;
};

struct bfa_fw_fip_stats_s {
	u32    vlan_req;           
	u32    vlan_notify;        
	u32    vlan_err;           
	u32    vlan_timeouts;      
	u32    vlan_invalids;      
	u32    disc_req;           
	u32    disc_rsp;           
	u32    disc_err;           
	u32    disc_unsol;         
	u32    disc_timeouts;      
	u32    disc_fcf_unavail;   
	u32    linksvc_unsupp;     
	u32    linksvc_err;        
	u32    logo_req;           
	u32    clrvlink_req;       
	u32    op_unsupp;          
	u32    untagged;           
	u32    invalid_version;    
};

struct bfa_fw_lps_stats_s {
	u32    mac_invalids;       
	u32    rsvd;
};

struct bfa_fw_fcoe_stats_s {
	u32    cee_linkups;        
	u32    cee_linkdns;        
	u32    fip_linkups;        
	u32    fip_linkdns;        
	u32    fip_fails;          
	u32    mac_invalids;       
};

struct bfa_fw_fcoe_port_stats_s {
	struct bfa_fw_fcoe_stats_s		fcoe_stats;
	struct bfa_fw_fip_stats_s		fip_stats;
};

struct bfa_fw_fc_uport_stats_s {
	struct bfa_fw_port_snsm_stats_s		snsm_stats;
	struct bfa_fw_port_lksm_stats_s		lksm_stats;
};

union bfa_fw_fc_port_stats_s {
	struct bfa_fw_fc_uport_stats_s		fc_stats;
	struct bfa_fw_fcoe_port_stats_s		fcoe_stats;
};

struct bfa_fw_port_stats_s {
	struct bfa_fw_port_fpg_stats_s		fpg_stats;
	struct bfa_fw_port_physm_stats_s	physm_stats;
	union  bfa_fw_fc_port_stats_s		fc_port;
};

struct bfa_fw_fcxchg_stats_s {
	u32	ua_tag_inv;
	u32	ua_state_inv;
};

struct bfa_fw_lpsm_stats_s {
	u32	cls_rx;
	u32	cls_tx;
};

struct bfa_fw_trunk_stats_s {
	u32 emt_recvd;		
	u32 emt_accepted;	
	u32 emt_rejected;	
	u32 etp_recvd;		
	u32 etp_accepted;	
	u32 etp_rejected;	
	u32 lr_recvd;		
	u32 rsvd;		
};

struct bfa_fw_advsm_stats_s {
	u32 flogi_sent;		
	u32 flogi_acc_recvd;	
	u32 flogi_rjt_recvd;	
	u32 flogi_retries;	

	u32 elp_recvd;		
	u32 elp_accepted;	
	u32 elp_rejected;	
	u32 elp_dropped;	
};

struct bfa_fw_iocfc_stats_s {
	u32	cfg_reqs;	
	u32	updq_reqs;	
	u32	ic_reqs;	
	u32	unknown_reqs;
	u32	set_intr_reqs;	
};

struct bfa_iocfc_attr_s {
	struct bfa_iocfc_cfg_s		config;		
	struct bfa_iocfc_intr_attr_s	intr_attr;	
};

struct bfa_fw_eth_sndrcv_stats_s {
	u32	crc_err;
	u32	rsvd;		
};

struct bfa_fw_mac_mod_stats_s {
	u32	mac_on;		
	u32	link_up;	
	u32	signal_off;	
	u32	dfe_on;		
	u32	mac_reset;	
	u32	pcs_reset;	
	u32	loopback;	
	u32	lb_mac_reset;
			
	u32	lb_pcs_reset;
			
	u32	rsvd;		
};

struct bfa_fw_ct_mod_stats_s {
	u32	rxa_rds_undrun;	
	u32	rad_bpc_ovfl;	
	u32	rad_rlb_bpc_ovfl; 
	u32	bpc_fcs_err;	
	u32	txa_tso_hdr;	
	u32	rsvd;		
};

struct bfa_fw_stats_s {
	struct bfa_fw_ioc_stats_s	ioc_stats;
	struct bfa_fw_iocfc_stats_s	iocfc_stats;
	struct bfa_fw_io_stats_s	io_stats;
	struct bfa_fw_port_stats_s	port_stats;
	struct bfa_fw_fcxchg_stats_s	fcxchg_stats;
	struct bfa_fw_lpsm_stats_s	lpsm_stats;
	struct bfa_fw_lps_stats_s	lps_stats;
	struct bfa_fw_trunk_stats_s	trunk_stats;
	struct bfa_fw_advsm_stats_s	advsm_stats;
	struct bfa_fw_mac_mod_stats_s	macmod_stats;
	struct bfa_fw_ct_mod_stats_s	ctmod_stats;
	struct bfa_fw_eth_sndrcv_stats_s	ethsndrcv_stats;
};

#define BFA_IOCFC_PATHTOV_MAX	60
#define BFA_IOCFC_QDEPTH_MAX	2000

enum bfa_qos_state {
	BFA_QOS_DISABLED = 0,		
	BFA_QOS_ONLINE = 1,		
	BFA_QOS_OFFLINE = 2,		
};

enum bfa_qos_priority {
	BFA_QOS_UNKNOWN = 0,
	BFA_QOS_HIGH  = 1,	
	BFA_QOS_MED  =  2,	
	BFA_QOS_LOW  =  3,	
};

enum bfa_qos_bw_alloc {
	BFA_QOS_BW_HIGH  = 60,	
	BFA_QOS_BW_MED  =  30,	
	BFA_QOS_BW_LOW  =  10,	
};
#pragma pack(1)
struct bfa_qos_attr_s {
	u8		state;		
	u8		rsvd[3];
	u32  total_bb_cr;		
};

#define  BFA_QOS_MAX_VC  16

struct bfa_qos_vc_info_s {
	u8 vc_credit;
	u8 borrow_credit;
	u8 priority;
	u8 resvd;
};

struct bfa_qos_vc_attr_s {
	u16  total_vc_count;                    
	u16  shared_credit;
	u32  elp_opmode_flags;
	struct bfa_qos_vc_info_s vc_info[BFA_QOS_MAX_VC];  
};

struct bfa_qos_stats_s {
	u32	flogi_sent;		
	u32	flogi_acc_recvd;	
	u32	flogi_rjt_recvd;	
	u32	flogi_retries;		

	u32	elp_recvd;		
	u32	elp_accepted;		
	u32	elp_rejected;		
	u32	elp_dropped;		

	u32	qos_rscn_recvd;		
	u32	rsvd;			
};

struct bfa_fcoe_stats_s {
	u64	secs_reset;	
	u64	cee_linkups;	
	u64	cee_linkdns;	
	u64	fip_linkups;	
	u64	fip_linkdns;	
	u64	fip_fails;	
	u64	mac_invalids;	
	u64	vlan_req;	
	u64	vlan_notify;	
	u64	vlan_err;	
	u64	vlan_timeouts;	
	u64	vlan_invalids;	
	u64	disc_req;	
	u64	disc_rsp;	
	u64	disc_err;	
	u64	disc_unsol;	
	u64	disc_timeouts;	
	u64	disc_fcf_unavail; 
	u64	linksvc_unsupp;	
	u64	linksvc_err;	
	u64	logo_req;	
	u64	clrvlink_req;	
	u64	op_unsupp;	
	u64	untagged;	
	u64	txf_ucast;	
	u64	txf_ucast_vlan;	
	u64	txf_ucast_octets; 
	u64	txf_mcast;	
	u64	txf_mcast_vlan;	
	u64	txf_mcast_octets; 
	u64	txf_bcast;	
	u64	txf_bcast_vlan;	
	u64	txf_bcast_octets; 
	u64	txf_timeout;	  
	u64	txf_parity_errors; 
	u64	txf_fid_parity_errors; 
	u64	rxf_ucast_octets; 
	u64	rxf_ucast;	
	u64	rxf_ucast_vlan;	
	u64	rxf_mcast_octets; 
	u64	rxf_mcast;	
	u64	rxf_mcast_vlan;	
	u64	rxf_bcast_octets; 
	u64	rxf_bcast;	
	u64	rxf_bcast_vlan;	
};

union bfa_fcport_stats_u {
	struct bfa_qos_stats_s	fcqos;
	struct bfa_fcoe_stats_s	fcoe;
};
#pragma pack()

struct bfa_fcpim_del_itn_stats_s {
	u32	del_itn_iocomp_aborted;	   
	u32	del_itn_iocomp_timedout;   
	u32	del_itn_iocom_sqer_needed; 
	u32	del_itn_iocom_res_free;    
	u32	del_itn_iocom_hostabrts;   
	u32	del_itn_total_ios;	   
	u32	del_io_iocdowns;	   
	u32	del_tm_iocdowns;	   
};

struct bfa_itnim_iostats_s {

	u32	total_ios;		
	u32	input_reqs;		
	u32	output_reqs;		
	u32	io_comps;		
	u32	wr_throughput;		
	u32	rd_throughput;		

	u32	iocomp_ok;		
	u32	iocomp_underrun;	
	u32	iocomp_overrun;		
	u32	qwait;			
	u32	qresumes;		
	u32	no_iotags;		
	u32	iocomp_timedout;	
	u32	iocom_nexus_abort;	
	u32	iocom_proto_err;	
	u32	iocom_dif_err;		

	u32	iocom_sqer_needed;	
	u32	iocom_res_free;		


	u32	io_aborts;		
	u32	iocom_hostabrts;	
	u32	io_cleanups;		
	u32	path_tov_expired;	
	u32	iocomp_aborted;		
	u32	io_iocdowns;		
	u32	iocom_utags;		

	u32	io_tmaborts;		
	u32	tm_io_comps;		

	u32	creates;		
	u32	fw_create;		
	u32	create_comps;		
	u32	onlines;		
	u32	offlines;		
	u32	fw_delete;		
	u32	delete_comps;		
	u32	deletes;		
	u32	sler_events;		
	u32	ioc_disabled;		
	u32	cleanup_comps;		

	u32	tm_cmnds;		
	u32	tm_fw_rsps;		
	u32	tm_success;		
	u32	tm_failures;		
	u32	no_tskims;		
	u32	tm_qwait;		
	u32	tm_qresumes;		

	u32	tm_iocdowns;		
	u32	tm_cleanups;		
	u32	tm_cleanup_comps;	
	u32	rsvd[6];
};

enum bfa_port_states {
	BFA_PORT_ST_UNINIT		= 1,
	BFA_PORT_ST_ENABLING_QWAIT	= 2,
	BFA_PORT_ST_ENABLING		= 3,
	BFA_PORT_ST_LINKDOWN		= 4,
	BFA_PORT_ST_LINKUP		= 5,
	BFA_PORT_ST_DISABLING_QWAIT	= 6,
	BFA_PORT_ST_DISABLING		= 7,
	BFA_PORT_ST_DISABLED		= 8,
	BFA_PORT_ST_STOPPED		= 9,
	BFA_PORT_ST_IOCDOWN		= 10,
	BFA_PORT_ST_IOCDIS		= 11,
	BFA_PORT_ST_FWMISMATCH		= 12,
	BFA_PORT_ST_PREBOOT_DISABLED	= 13,
	BFA_PORT_ST_TOGGLING_QWAIT	= 14,
	BFA_PORT_ST_ACQ_ADDR		= 15,
	BFA_PORT_ST_MAX_STATE,
};

enum bfa_port_type {
	BFA_PORT_TYPE_UNKNOWN	= 1,	
	BFA_PORT_TYPE_NPORT	= 5,	
	BFA_PORT_TYPE_NLPORT	= 6,	
	BFA_PORT_TYPE_LPORT	= 20,	
	BFA_PORT_TYPE_P2P	= 21,	
	BFA_PORT_TYPE_VPORT	= 22,	
};

enum bfa_port_topology {
	BFA_PORT_TOPOLOGY_NONE = 0,	
	BFA_PORT_TOPOLOGY_P2P  = 1,	
	BFA_PORT_TOPOLOGY_LOOP = 2,	
	BFA_PORT_TOPOLOGY_AUTO = 3,	
};

enum bfa_port_opmode {
	BFA_PORT_OPMODE_NORMAL   = 0x00, 
	BFA_PORT_OPMODE_LB_INT   = 0x01, 
	BFA_PORT_OPMODE_LB_SLW   = 0x02, 
	BFA_PORT_OPMODE_LB_EXT   = 0x04, 
	BFA_PORT_OPMODE_LB_CBL   = 0x08, 
	BFA_PORT_OPMODE_LB_NLINT = 0x20, 
};

#define BFA_PORT_OPMODE_LB_HARD(_mode)			\
	((_mode == BFA_PORT_OPMODE_LB_INT) ||		\
	(_mode == BFA_PORT_OPMODE_LB_SLW) ||		\
	(_mode == BFA_PORT_OPMODE_LB_EXT))

enum bfa_port_linkstate {
	BFA_PORT_LINKUP		= 1,	
	BFA_PORT_LINKDOWN	= 2,	
};

enum bfa_port_linkstate_rsn {
	BFA_PORT_LINKSTATE_RSN_NONE		= 0,
	BFA_PORT_LINKSTATE_RSN_DISABLED		= 1,
	BFA_PORT_LINKSTATE_RSN_RX_NOS		= 2,
	BFA_PORT_LINKSTATE_RSN_RX_OLS		= 3,
	BFA_PORT_LINKSTATE_RSN_RX_LIP		= 4,
	BFA_PORT_LINKSTATE_RSN_RX_LIPF7		= 5,
	BFA_PORT_LINKSTATE_RSN_SFP_REMOVED	= 6,
	BFA_PORT_LINKSTATE_RSN_PORT_FAULT	= 7,
	BFA_PORT_LINKSTATE_RSN_RX_LOS		= 8,
	BFA_PORT_LINKSTATE_RSN_LOCAL_FAULT	= 9,
	BFA_PORT_LINKSTATE_RSN_REMOTE_FAULT	= 10,
	BFA_PORT_LINKSTATE_RSN_TIMEOUT		= 11,



	
	CEE_LLDP_INFO_AGED_OUT			= 20,
	CEE_LLDP_SHUTDOWN_TLV_RCVD		= 21,
	CEE_PEER_NOT_ADVERTISE_DCBX		= 22,
	CEE_PEER_NOT_ADVERTISE_PG		= 23,
	CEE_PEER_NOT_ADVERTISE_PFC		= 24,
	CEE_PEER_NOT_ADVERTISE_FCOE		= 25,
	CEE_PG_NOT_COMPATIBLE			= 26,
	CEE_PFC_NOT_COMPATIBLE			= 27,
	CEE_FCOE_NOT_COMPATIBLE			= 28,
	CEE_BAD_PG_RCVD				= 29,
	CEE_BAD_BW_RCVD				= 30,
	CEE_BAD_PFC_RCVD			= 31,
	CEE_BAD_APP_PRI_RCVD			= 32,
	CEE_FCOE_PRI_PFC_OFF			= 33,
	CEE_DUP_CONTROL_TLV_RCVD		= 34,
	CEE_DUP_FEAT_TLV_RCVD			= 35,
	CEE_APPLY_NEW_CFG			= 36, 
	CEE_PROTOCOL_INIT			= 37, 
	CEE_PHY_LINK_DOWN			= 38,
	CEE_LLS_FCOE_ABSENT			= 39,
	CEE_LLS_FCOE_DOWN			= 40,
	CEE_ISCSI_NOT_COMPATIBLE		= 41,
	CEE_ISCSI_PRI_PFC_OFF			= 42,
	CEE_ISCSI_PRI_OVERLAP_FCOE_PRI		= 43
};

#define MAX_LUN_MASK_CFG 16

enum bfa_ioim_lun_mask_state_s {
	BFA_IOIM_LUN_MASK_INACTIVE = 0,
	BFA_IOIM_LUN_MASK_ACTIVE = 1,
	BFA_IOIM_LUN_MASK_FETCHED = 2,
};

enum bfa_lunmask_state_s {
	BFA_LUNMASK_DISABLED = 0x00,
	BFA_LUNMASK_ENABLED = 0x01,
	BFA_LUNMASK_MINCFG = 0x02,
	BFA_LUNMASK_UNINITIALIZED = 0xff,
};

#pragma pack(1)
struct bfa_lun_mask_s {
	wwn_t		lp_wwn;
	wwn_t		rp_wwn;
	struct scsi_lun	lun;
	u8		ua;
	u8		rsvd[3];
	u16		rp_tag;
	u8		lp_tag;
	u8		state;
};

#define MAX_LUN_MASK_CFG 16
struct bfa_lunmask_cfg_s {
	u32	status;
	u32	rsvd;
	struct bfa_lun_mask_s	lun_list[MAX_LUN_MASK_CFG];
};

struct bfa_port_cfg_s {
	u8	 topology;	
	u8	 speed;		
	u8	 trunked;	
	u8	 qos_enabled;	
	u8	 cfg_hardalpa;	
	u8	 hardalpa;	
	__be16	 maxfrsize;	
	u8	 rx_bbcredit;	
	u8	 tx_bbcredit;	
	u8	 ratelimit;	
	u8	 trl_def_speed;	
	u8	 bb_scn;	
	u8	 bb_scn_state;	
	u8	 faa_state;	
	u8	 rsvd[1];
	u16	 path_tov;	
	u16	 q_depth;	
};
#pragma pack()

struct bfa_port_attr_s {
	wwn_t			nwwn;		
	wwn_t			pwwn;		
	wwn_t			factorynwwn;	
	wwn_t			factorypwwn;	
	enum fc_cos		cos_supported;	
	u32			rsvd;
	struct fc_symname_s	port_symname;	
	enum bfa_port_speed	speed_supported; 
	bfa_boolean_t		pbind_enabled;

	struct bfa_port_cfg_s	pport_cfg;	

	enum bfa_port_states	port_state;	
	enum bfa_port_speed	speed;		
	enum bfa_port_topology	topology;	
	bfa_boolean_t		beacon;		
	bfa_boolean_t		link_e2e_beacon; 
	bfa_boolean_t		bbsc_op_status;	

	u32			pid;		
	enum bfa_port_type	port_type;	
	u32			loopback;	
	u32			authfail;	

	
	u16			fcoe_vlan;
	u8			rsvd1[2];
};

struct bfa_port_fcpmap_s {
	char	osdevname[256];
	u32	bus;
	u32	target;
	u32	oslun;
	u32	fcid;
	wwn_t	nwwn;
	wwn_t	pwwn;
	u64	fcplun;
	char	luid[256];
};

struct bfa_port_rnid_s {
	wwn_t	  wwn;
	u32	  unittype;
	u32	  portid;
	u32	  attached_nodes_num;
	u16	  ip_version;
	u16	  udp_port;
	u8	  ipaddr[16];
	u16	  rsvd;
	u16	  topologydiscoveryflags;
};

#pragma pack(1)
struct bfa_fcport_fcf_s {
	wwn_t	name;		
	wwn_t	fabric_name;    
	u8	fipenabled;	
	u8	fipfailed;	
	u8	resv[2];
	u8	pri;		
	u8	version;	
	u8	available;      
	u8	fka_disabled;   
	u8	maxsz_verified; 
	u8	fc_map[3];      
	__be16	vlan;		
	u32	fka_adv_per;    
	mac_t	mac;		
};

enum bfa_trunk_state {
	BFA_TRUNK_DISABLED	= 0,	
	BFA_TRUNK_ONLINE	= 1,	
	BFA_TRUNK_OFFLINE	= 2,	
};

struct bfa_trunk_vc_attr_s {
	u32 bb_credit;
	u32 elp_opmode_flags;
	u32 req_credit;
	u16 vc_credits[8];
};

struct bfa_port_link_s {
	u8	 linkstate;	
	u8	 linkstate_rsn;	
	u8	 topology;	
	u8	 speed;		
	u32	 linkstate_opt; 
	u8	 trunked;	
	u8	 resvd[3];
	struct bfa_qos_attr_s  qos_attr;   
	union {
		struct bfa_qos_vc_attr_s qos_vc_attr;  
		struct bfa_trunk_vc_attr_s trunk_vc_attr;
		struct bfa_fcport_fcf_s fcf; 
	} vc_fcf;
};
#pragma pack()

enum bfa_trunk_link_fctl {
	BFA_TRUNK_LINK_FCTL_NORMAL,
	BFA_TRUNK_LINK_FCTL_VC,
	BFA_TRUNK_LINK_FCTL_VC_QOS,
};

enum bfa_trunk_link_state {
	BFA_TRUNK_LINK_STATE_UP = 1,		
	BFA_TRUNK_LINK_STATE_DN_LINKDN = 2,	
	BFA_TRUNK_LINK_STATE_DN_GRP_MIS = 3,	
	BFA_TRUNK_LINK_STATE_DN_SPD_MIS = 4,	
	BFA_TRUNK_LINK_STATE_DN_MODE_MIS = 5,	
};

#define BFA_TRUNK_MAX_PORTS	2
struct bfa_trunk_link_attr_s {
	wwn_t    trunk_wwn;
	enum bfa_trunk_link_fctl fctl;
	enum bfa_trunk_link_state link_state;
	enum bfa_port_speed	speed;
	u32 deskew;
};

struct bfa_trunk_attr_s {
	enum bfa_trunk_state	state;
	enum bfa_port_speed	speed;
	u32		port_id;
	u32		rsvd;
	struct bfa_trunk_link_attr_s link_attr[BFA_TRUNK_MAX_PORTS];
};

struct bfa_rport_hal_stats_s {
	u32        sm_un_cr;	    
	u32        sm_un_unexp;	    
	u32        sm_cr_on;	    
	u32        sm_cr_del;	    
	u32        sm_cr_hwf;	    
	u32        sm_cr_unexp;	    
	u32        sm_fwc_rsp;	    
	u32        sm_fwc_del;	    
	u32        sm_fwc_off;	    
	u32        sm_fwc_hwf;	    
	u32        sm_fwc_unexp;    
	u32        sm_on_off;	    
	u32        sm_on_del;	    
	u32        sm_on_hwf;	    
	u32        sm_on_unexp;	    
	u32        sm_fwd_rsp;	    
	u32        sm_fwd_del;	    
	u32        sm_fwd_hwf;	    
	u32        sm_fwd_unexp;    
	u32        sm_off_del;	    
	u32        sm_off_on;	    
	u32        sm_off_hwf;	    
	u32        sm_off_unexp;    
	u32        sm_del_fwrsp;    
	u32        sm_del_hwf;	    
	u32        sm_del_unexp;    
	u32        sm_delp_fwrsp;   
	u32        sm_delp_hwf;	    
	u32        sm_delp_unexp;   
	u32        sm_offp_fwrsp;   
	u32        sm_offp_del;	    
	u32        sm_offp_hwf;	    
	u32        sm_offp_unexp;   
	u32        sm_iocd_off;	    
	u32        sm_iocd_del;	    
	u32        sm_iocd_on;	    
	u32        sm_iocd_unexp;   
	u32        rsvd;
};
#pragma pack(1)
struct bfa_rport_qos_attr_s {
	u8		qos_priority;	
	u8		rsvd[3];
	u32		qos_flow_id;	
};
#pragma pack()

#define BFA_IOBUCKET_MAX 14

struct bfa_itnim_latency_s {
	u32 min[BFA_IOBUCKET_MAX];
	u32 max[BFA_IOBUCKET_MAX];
	u32 count[BFA_IOBUCKET_MAX];
	u32 avg[BFA_IOBUCKET_MAX];
};

struct bfa_itnim_ioprofile_s {
	u32 clock_res_mul;
	u32 clock_res_div;
	u32 index;
	u32 io_profile_start_time;	
	u32 iocomps[BFA_IOBUCKET_MAX];	
	struct bfa_itnim_latency_s io_latency;
};

struct bfa_vhba_attr_s {
	wwn_t	nwwn;       
	wwn_t	pwwn;       
	u32	pid;        
	bfa_boolean_t       io_profile; 
	bfa_boolean_t       plog_enabled;   
	u16	path_tov;
	u8	rsvd[2];
};

struct bfa_port_fc_stats_s {
	u64     secs_reset;     
	u64     tx_frames;      
	u64     tx_words;       
	u64     tx_lip;         
	u64     tx_nos;         
	u64     tx_ols;         
	u64     tx_lr;          
	u64     tx_lrr;         
	u64     rx_frames;      
	u64     rx_words;       
	u64     lip_count;      
	u64     nos_count;      
	u64     ols_count;      
	u64     lr_count;       
	u64     lrr_count;      
	u64     invalid_crcs;   
	u64     invalid_crc_gd_eof; 
	u64     undersized_frm; 
	u64     oversized_frm;  
	u64     bad_eof_frm;    
	u64     error_frames;   
	u64     dropped_frames; 
	u64     link_failures;  
	u64     loss_of_syncs;  
	u64     loss_of_signals; 
	u64     primseq_errs;   
	u64     bad_os_count;   
	u64     err_enc_out;    
	u64     err_enc;        
	u64	bbsc_frames_lost; 
	u64	bbsc_credits_lost; 
	u64	bbsc_link_resets; 
};

struct bfa_port_eth_stats_s {
	u64     secs_reset;     
	u64     frame_64;       
	u64     frame_65_127;   
	u64     frame_128_255;  
	u64     frame_256_511;  
	u64     frame_512_1023; 
	u64     frame_1024_1518; 
	u64     frame_1519_1522; 
	u64     tx_bytes;       
	u64     tx_packets;      
	u64     tx_mcast_packets; 
	u64     tx_bcast_packets; 
	u64     tx_control_frame; 
	u64     tx_drop;        
	u64     tx_jabber;      
	u64     tx_fcs_error;   
	u64     tx_fragments;   
	u64     rx_bytes;       
	u64     rx_packets;     
	u64     rx_mcast_packets; 
	u64     rx_bcast_packets; 
	u64     rx_control_frames; 
	u64     rx_unknown_opcode; 
	u64     rx_drop;        
	u64     rx_jabber;      
	u64     rx_fcs_error;   
	u64     rx_alignment_error; 
	u64     rx_frame_length_error; 
	u64     rx_code_error;  
	u64     rx_fragments;   
	u64     rx_pause;       
	u64     rx_zero_pause;  
	u64     tx_pause;       
	u64     tx_zero_pause;  
	u64     rx_fcoe_pause;  
	u64     rx_fcoe_zero_pause; 
	u64     tx_fcoe_pause;  
	u64     tx_fcoe_zero_pause; 
	u64     rx_iscsi_pause; 
	u64     rx_iscsi_zero_pause; 
	u64     tx_iscsi_pause; 
	u64     tx_iscsi_zero_pause; 
};

union bfa_port_stats_u {
	struct bfa_port_fc_stats_s      fc;
	struct bfa_port_eth_stats_s     eth;
};

struct bfa_port_cfg_mode_s {
	u16		max_pf;
	u16		max_vf;
	enum bfa_mode_s	mode;
};

#pragma pack(1)

#define BFA_CEE_LLDP_MAX_STRING_LEN	(128)
#define BFA_CEE_DCBX_MAX_PRIORITY	(8)
#define BFA_CEE_DCBX_MAX_PGID		(8)

struct bfa_cee_lldp_str_s {
	u8	sub_type;
	u8	len;
	u8	rsvd[2];
	u8	value[BFA_CEE_LLDP_MAX_STRING_LEN];
};

struct bfa_cee_lldp_cfg_s {
	struct bfa_cee_lldp_str_s chassis_id;
	struct bfa_cee_lldp_str_s port_id;
	struct bfa_cee_lldp_str_s port_desc;
	struct bfa_cee_lldp_str_s sys_name;
	struct bfa_cee_lldp_str_s sys_desc;
	struct bfa_cee_lldp_str_s mgmt_addr;
	u16	time_to_live;
	u16	enabled_system_cap;
};

struct bfa_cee_dcbx_cfg_s {
	u8	pgid[BFA_CEE_DCBX_MAX_PRIORITY];
	u8	pg_percentage[BFA_CEE_DCBX_MAX_PGID];
	u8	pfc_primap; 
	u8	fcoe_primap; 
	u8	iscsi_primap; 
	u8	dcbx_version; 
	u8	lls_fcoe; 
	u8	lls_lan; 
	u8	rsvd[2];
};

struct bfa_cee_attr_s {
	u8	cee_status;
	u8	error_reason;
	struct bfa_cee_lldp_cfg_s lldp_remote;
	struct bfa_cee_dcbx_cfg_s dcbx_remote;
	mac_t src_mac;
	u8	link_speed;
	u8	nw_priority;
	u8	filler[2];
};

struct bfa_cee_stats_s {
	u32		lldp_tx_frames;		
	u32		lldp_rx_frames;		
	u32		lldp_rx_frames_invalid; 
	u32		lldp_rx_frames_new;     
	u32		lldp_tlvs_unrecognized; 
	u32		lldp_rx_shutdown_tlvs;  
	u32		lldp_info_aged_out;     
	u32		dcbx_phylink_ups;       
	u32		dcbx_phylink_downs;     
	u32		dcbx_rx_tlvs;           
	u32		dcbx_rx_tlvs_invalid;   
	u32		dcbx_control_tlv_error; 
	u32		dcbx_feature_tlv_error; 
	u32		dcbx_cee_cfg_new;       
	u32		cee_status_down;        
	u32		cee_status_up;          
	u32		cee_hw_cfg_changed;     
	u32		cee_rx_invalid_cfg;     
};

#pragma pack()

#define BFAD_NL_VENDOR_ID (((u64)0x01 << SCSI_NL_VID_TYPE_SHIFT) \
			   | BFA_PCI_VENDOR_ID_BROCADE)

enum bfa_rport_aen_event {
	BFA_RPORT_AEN_ONLINE     = 1,   
	BFA_RPORT_AEN_OFFLINE    = 2,   
	BFA_RPORT_AEN_DISCONNECT = 3,   
	BFA_RPORT_AEN_QOS_PRIO   = 4,   
	BFA_RPORT_AEN_QOS_FLOWID = 5,   
};

struct bfa_rport_aen_data_s {
	u16             vf_id;  
	u16             rsvd[3];
	wwn_t           ppwwn;  
	wwn_t           lpwwn;  
	wwn_t           rpwwn;  
	union {
		struct bfa_rport_qos_attr_s qos;
	} priv;
};

union bfa_aen_data_u {
	struct bfa_adapter_aen_data_s	adapter;
	struct bfa_port_aen_data_s	port;
	struct bfa_lport_aen_data_s	lport;
	struct bfa_rport_aen_data_s	rport;
	struct bfa_itnim_aen_data_s	itnim;
	struct bfa_audit_aen_data_s	audit;
	struct bfa_ioc_aen_data_s	ioc;
};

#define BFA_AEN_MAX_ENTRY	512

struct bfa_aen_entry_s {
	struct list_head	qe;
	enum bfa_aen_category   aen_category;
	u32                     aen_type;
	union bfa_aen_data_u    aen_data;
	struct timeval          aen_tv;
	u32                     seq_num;
	u32                     bfad_num;
};

#endif 
