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

#ifndef __BFA_DEFS_FCS_H__
#define __BFA_DEFS_FCS_H__

#include "bfa_fc.h"
#include "bfa_defs_svc.h"

enum bfa_vf_state {
	BFA_VF_UNINIT    = 0,	
	BFA_VF_LINK_DOWN = 1,	
	BFA_VF_FLOGI     = 2,	
	BFA_VF_AUTH      = 3,	
	BFA_VF_NOFABRIC  = 4,	
	BFA_VF_ONLINE    = 5,	
	BFA_VF_EVFP      = 6,	
	BFA_VF_ISOLATED  = 7,	
};

struct bfa_vf_stats_s {
	u32	flogi_sent;	
	u32	flogi_rsp_err;	
	u32	flogi_acc_err;	
	u32	flogi_accepts;	
	u32	flogi_rejects;	
	u32	flogi_unknown_rsp; 
	u32	flogi_alloc_wait; 
	u32	flogi_rcvd;	
	u32	flogi_rejected;	
	u32	fabric_onlines;	
	u32	fabric_offlines; 
	u32	resvd; 
};

struct bfa_vf_attr_s {
	enum bfa_vf_state  state;		
	u32        rsvd;
	wwn_t           fabric_name;	
};

#define BFA_FCS_MAX_LPORTS 256
#define BFA_FCS_FABRIC_IPADDR_SZ  16

#define BFA_SYMNAME_MAXLEN	128	
struct bfa_lport_symname_s {
	char	    symname[BFA_SYMNAME_MAXLEN];
};

enum bfa_lport_role {
	BFA_LPORT_ROLE_FCP_IM	= 0x01,	
	BFA_LPORT_ROLE_FCP_MAX	= BFA_LPORT_ROLE_FCP_IM,
};

struct bfa_lport_cfg_s {
	wwn_t	       pwwn;       
	wwn_t	       nwwn;       
	struct bfa_lport_symname_s  sym_name;   
	enum bfa_lport_role roles;      
	u32     rsvd;
	bfa_boolean_t   preboot_vp;  
	u8	tag[16];        
	u8	padding[4];
};

enum bfa_lport_state {
	BFA_LPORT_UNINIT  = 0,	
	BFA_LPORT_FDISC   = 1,	
	BFA_LPORT_ONLINE  = 2,	
	BFA_LPORT_OFFLINE = 3,	
};

enum bfa_lport_type {
	BFA_LPORT_TYPE_PHYSICAL = 0,
	BFA_LPORT_TYPE_VIRTUAL,
};

enum bfa_lport_offline_reason {
	BFA_LPORT_OFFLINE_UNKNOWN = 0,
	BFA_LPORT_OFFLINE_LINKDOWN,
	BFA_LPORT_OFFLINE_FAB_UNSUPPORTED,	
	BFA_LPORT_OFFLINE_FAB_NORESOURCES,
	BFA_LPORT_OFFLINE_FAB_LOGOUT,
};

struct bfa_lport_info_s {
	u8	 port_type;	
	u8	 port_state;	
	u8	 offline_reason;	
	wwn_t	   port_wwn;
	wwn_t	   node_wwn;

	u32	max_vports_supp;	
	u32	num_vports_inuse;	
	u32	max_rports_supp;	
	u32	num_rports_inuse;	

};

struct bfa_lport_stats_s {
	u32	ns_plogi_sent;
	u32	ns_plogi_rsp_err;
	u32	ns_plogi_acc_err;
	u32	ns_plogi_accepts;
	u32	ns_rejects;	
	u32	ns_plogi_unknown_rsp;
	u32	ns_plogi_alloc_wait;

	u32	ns_retries;	
	u32	ns_timeouts;	

	u32	ns_rspnid_sent;
	u32	ns_rspnid_accepts;
	u32	ns_rspnid_rsp_err;
	u32	ns_rspnid_rejects;
	u32	ns_rspnid_alloc_wait;

	u32	ns_rftid_sent;
	u32	ns_rftid_accepts;
	u32	ns_rftid_rsp_err;
	u32	ns_rftid_rejects;
	u32	ns_rftid_alloc_wait;

	u32	ns_rffid_sent;
	u32	ns_rffid_accepts;
	u32	ns_rffid_rsp_err;
	u32	ns_rffid_rejects;
	u32	ns_rffid_alloc_wait;

	u32	ns_gidft_sent;
	u32	ns_gidft_accepts;
	u32	ns_gidft_rsp_err;
	u32	ns_gidft_rejects;
	u32	ns_gidft_unknown_rsp;
	u32	ns_gidft_alloc_wait;

	u32	ms_retries;	
	u32	ms_timeouts;	
	u32	ms_plogi_sent;
	u32	ms_plogi_rsp_err;
	u32	ms_plogi_acc_err;
	u32	ms_plogi_accepts;
	u32	ms_rejects;	
	u32	ms_plogi_unknown_rsp;
	u32	ms_plogi_alloc_wait;

	u32	num_rscn;	
	u32	num_portid_rscn;

	u32	uf_recvs;	
	u32	uf_recv_drops;	

	u32	plogi_rcvd;	
	u32	prli_rcvd;	
	u32	adisc_rcvd;	
	u32	prlo_rcvd;	
	u32	logo_rcvd;	
	u32	rpsc_rcvd;	
	u32	un_handled_els_rcvd;	
	u32	rport_plogi_timeouts; 
	u32	rport_del_max_plogi_retry; 
};

struct bfa_lport_attr_s {
	enum bfa_lport_state state;	
	u32	 pid;	
	struct bfa_lport_cfg_s   port_cfg;	
	enum bfa_port_type port_type;	
	u32	 loopback;	
	wwn_t	fabric_name; 
	u8	fabric_ip_addr[BFA_FCS_FABRIC_IPADDR_SZ]; 
	mac_t	   fpma_mac;	
	u16	authfail;	
};


enum bfa_vport_state {
	BFA_FCS_VPORT_UNINIT		= 0,
	BFA_FCS_VPORT_CREATED		= 1,
	BFA_FCS_VPORT_OFFLINE		= 1,
	BFA_FCS_VPORT_FDISC_SEND	= 2,
	BFA_FCS_VPORT_FDISC		= 3,
	BFA_FCS_VPORT_FDISC_RETRY	= 4,
	BFA_FCS_VPORT_FDISC_RSP_WAIT	= 5,
	BFA_FCS_VPORT_ONLINE		= 6,
	BFA_FCS_VPORT_DELETING		= 7,
	BFA_FCS_VPORT_CLEANUP		= 8,
	BFA_FCS_VPORT_LOGO_SEND		= 9,
	BFA_FCS_VPORT_LOGO		= 10,
	BFA_FCS_VPORT_ERROR		= 11,
	BFA_FCS_VPORT_MAX_STATE,
};

struct bfa_vport_stats_s {
	struct bfa_lport_stats_s port_stats;	

	u32        fdisc_sent;	
	u32        fdisc_accepts;	
	u32        fdisc_retries;	
	u32        fdisc_timeouts;	
	u32        fdisc_rsp_err;	
	u32        fdisc_acc_bad;	
	u32        fdisc_rejects;	
	u32        fdisc_unknown_rsp;
	u32        fdisc_alloc_wait;

	u32        logo_alloc_wait;
	u32        logo_sent;	
	u32        logo_accepts;	
	u32        logo_rejects;	
	u32        logo_rsp_err;	
	u32        logo_unknown_rsp;
			

	u32        fab_no_npiv;	

	u32        fab_offline;	
	u32        fab_online;	
	u32        fab_cleanup;	
	u32        rsvd;
};

struct bfa_vport_attr_s {
	struct bfa_lport_attr_s   port_attr; 
	enum bfa_vport_state vport_state; 
	u32          rsvd;
};

enum bfa_rport_state {
	BFA_RPORT_UNINIT	= 0,	
	BFA_RPORT_OFFLINE	= 1,	
	BFA_RPORT_PLOGI		= 2,	
	BFA_RPORT_ONLINE	= 3,	
	BFA_RPORT_PLOGI_RETRY	= 4,	
	BFA_RPORT_NSQUERY	= 5,	
	BFA_RPORT_ADISC		= 6,	
	BFA_RPORT_LOGO		= 7,	
	BFA_RPORT_LOGORCV	= 8,	
	BFA_RPORT_NSDISC	= 9,	
};

enum bfa_rport_function {
	BFA_RPORT_INITIATOR	= 0x01,	
	BFA_RPORT_TARGET	= 0x02,	
};

#define BFA_RPORT_SYMNAME_MAXLEN	255
struct bfa_rport_symname_s {
	char            symname[BFA_RPORT_SYMNAME_MAXLEN];
};

struct bfa_rport_stats_s {
	u32        offlines;           
	u32        onlines;            
	u32        rscns;              
	u32        plogis;		    
	u32        plogi_accs;	    
	u32        plogi_timeouts;	    
	u32        plogi_rejects;	    
	u32        plogi_failed;	    
	u32        plogi_rcvd;	    
	u32        prli_rcvd;          
	u32        adisc_rcvd;         
	u32        adisc_rejects;      
	u32        adisc_sent;         
	u32        adisc_accs;         
	u32        adisc_failed;       
	u32        adisc_rejected;     
	u32        logos;              
	u32        logo_accs;          
	u32        logo_failed;        
	u32        logo_rejected;      
	u32        logo_rcvd;          

	u32        rpsc_rcvd;         
	u32        rpsc_rejects;      
	u32        rpsc_sent;         
	u32        rpsc_accs;         
	u32        rpsc_failed;       
	u32        rpsc_rejected;     

	u32	rjt_insuff_res;	
	struct bfa_rport_hal_stats_s	hal_stats;  
};

struct bfa_rport_attr_s {
	wwn_t		nwwn;	
	wwn_t		pwwn;	
	enum fc_cos cos_supported;	
	u32		pid;	
	u32		df_sz;	
	enum bfa_rport_state	state;	
	enum fc_cos	fc_cos;	
	bfa_boolean_t	cisc;	
	struct bfa_rport_symname_s symname; 
	enum bfa_rport_function	scsi_function; 
	struct bfa_rport_qos_attr_s qos_attr; 
	enum bfa_port_speed curr_speed;   
	bfa_boolean_t	trl_enforced;	
	enum bfa_port_speed	assigned_speed;	
};

struct bfa_rport_remote_link_stats_s {
	u32 lfc; 
	u32 lsyc; 
	u32 lsic; 
	u32 pspec; 
	u32 itwc; 
	u32 icc; 
};


#define BFA_MAX_IO_INDEX 7
#define BFA_NO_IO_INDEX 9

enum bfa_itnim_state {
	BFA_ITNIM_OFFLINE	= 0,	
	BFA_ITNIM_PRLI_SEND	= 1,	
	BFA_ITNIM_PRLI_SENT	= 2,	
	BFA_ITNIM_PRLI_RETRY	= 3,	
	BFA_ITNIM_HCB_ONLINE	= 4,	
	BFA_ITNIM_ONLINE	= 5,	
	BFA_ITNIM_HCB_OFFLINE	= 6,	
	BFA_ITNIM_INITIATIOR	= 7,	
};

struct bfa_itnim_stats_s {
	u32        onlines;	
	u32        offlines;	
	u32        prli_sent;	
	u32        fcxp_alloc_wait;
	u32        prli_rsp_err;	
	u32        prli_rsp_acc;	
	u32        initiator;	
	u32        prli_rsp_parse_err;	
	u32        prli_rsp_rjt;	
	u32        timeout;	
	u32        sler;		
	u32	rsvd;		
};

struct bfa_itnim_attr_s {
	enum bfa_itnim_state state; 
	u8 retry;		
	u8	task_retry_id;  
	u8 rec_support;    
	u8 conf_comp;      
};

#endif 
