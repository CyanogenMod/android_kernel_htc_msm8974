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

#ifndef __BFA_FCS_H__
#define __BFA_FCS_H__

#include "bfa_cs.h"
#include "bfa_defs.h"
#include "bfa_defs_fcs.h"
#include "bfa_modules.h"
#include "bfa_fc.h"

#define BFA_FCS_OS_STR_LEN		64


enum bfa_lps_event {
	BFA_LPS_SM_LOGIN	= 1,	
	BFA_LPS_SM_LOGOUT	= 2,	
	BFA_LPS_SM_FWRSP	= 3,	
	BFA_LPS_SM_RESUME	= 4,	
	BFA_LPS_SM_DELETE	= 5,	
	BFA_LPS_SM_OFFLINE	= 6,	
	BFA_LPS_SM_RX_CVL	= 7,	
	BFA_LPS_SM_SET_N2N_PID  = 8,	
};


enum {
	BFA_TRC_FCS_FCS		= 1,
	BFA_TRC_FCS_PORT	= 2,
	BFA_TRC_FCS_RPORT	= 3,
	BFA_TRC_FCS_FCPIM	= 4,
};


struct bfa_fcs_s;

#define __fcs_min_cfg(__fcs)       ((__fcs)->min_cfg)

#define BFA_FCS_BRCD_SWITCH_OUI  0x051e
#define N2N_LOCAL_PID	    0x010000
#define N2N_REMOTE_PID		0x020000
#define	BFA_FCS_RETRY_TIMEOUT 2000
#define BFA_FCS_PID_IS_WKA(pid)  ((bfa_ntoh3b(pid) > 0xFFF000) ?  1 : 0)



struct bfa_fcs_lport_ns_s {
	bfa_sm_t        sm;		
	struct bfa_timer_s timer;
	struct bfa_fcs_lport_s *port;	
	struct bfa_fcxp_s *fcxp;
	struct bfa_fcxp_wqe_s fcxp_wqe;
};


struct bfa_fcs_lport_scn_s {
	bfa_sm_t        sm;		
	struct bfa_timer_s timer;
	struct bfa_fcs_lport_s *port;	
	struct bfa_fcxp_s *fcxp;
	struct bfa_fcxp_wqe_s fcxp_wqe;
};


struct bfa_fcs_lport_fdmi_s {
	bfa_sm_t        sm;		
	struct bfa_timer_s timer;
	struct bfa_fcs_lport_ms_s *ms;	
	struct bfa_fcxp_s *fcxp;
	struct bfa_fcxp_wqe_s fcxp_wqe;
	u8	retry_cnt;	
	u8	rsvd[3];
};


struct bfa_fcs_lport_ms_s {
	bfa_sm_t        sm;		
	struct bfa_timer_s timer;
	struct bfa_fcs_lport_s *port;	
	struct bfa_fcxp_s *fcxp;
	struct bfa_fcxp_wqe_s fcxp_wqe;
	struct bfa_fcs_lport_fdmi_s fdmi;	
	u8         retry_cnt;	
	u8	rsvd[3];
};


struct bfa_fcs_lport_fab_s {
	struct bfa_fcs_lport_ns_s ns;	
	struct bfa_fcs_lport_scn_s scn;	
	struct bfa_fcs_lport_ms_s ms;	
};

#define	MAX_ALPA_COUNT	127

struct bfa_fcs_lport_loop_s {
	u8         num_alpa;	
	u8         alpa_pos_map[MAX_ALPA_COUNT];	
	struct bfa_fcs_lport_s *port;	
};

struct bfa_fcs_lport_n2n_s {
	u32        rsvd;
	__be16     reply_oxid;	
	wwn_t           rem_port_wwn;	
};


union bfa_fcs_lport_topo_u {
	struct bfa_fcs_lport_fab_s pfab;
	struct bfa_fcs_lport_loop_s ploop;
	struct bfa_fcs_lport_n2n_s pn2n;
};


struct bfa_fcs_lport_s {
	struct list_head         qe;	
	bfa_sm_t               sm;	
	struct bfa_fcs_fabric_s *fabric;	
	struct bfa_lport_cfg_s  port_cfg;	
	struct bfa_timer_s link_timer;	
	u32        pid:24;	
	u8         lp_tag;		
	u16        num_rports;	
	struct list_head         rport_q; 
	struct bfa_fcs_s *fcs;	
	union bfa_fcs_lport_topo_u port_topo;	
	struct bfad_port_s *bfad_port;	
	struct bfa_fcs_vport_s *vport;	
	struct bfa_fcxp_s *fcxp;
	struct bfa_fcxp_wqe_s fcxp_wqe;
	struct bfa_lport_stats_s stats;
	struct bfa_wc_s        wc;	
};
#define BFA_FCS_GET_HAL_FROM_PORT(port)  (port->fcs->bfa)
#define BFA_FCS_GET_NS_FROM_PORT(port)  (&port->port_topo.pfab.ns)
#define BFA_FCS_GET_SCN_FROM_PORT(port)  (&port->port_topo.pfab.scn)
#define BFA_FCS_GET_MS_FROM_PORT(port)  (&port->port_topo.pfab.ms)
#define BFA_FCS_GET_FDMI_FROM_PORT(port)  (&port->port_topo.pfab.ms.fdmi)
#define	BFA_FCS_VPORT_IS_INITIATOR_MODE(port) \
		(port->port_cfg.roles & BFA_LPORT_ROLE_FCP_IM)

struct bfad_vf_s;

enum bfa_fcs_fabric_type {
	BFA_FCS_FABRIC_UNKNOWN = 0,
	BFA_FCS_FABRIC_SWITCHED = 1,
	BFA_FCS_FABRIC_N2N = 2,
};


struct bfa_fcs_fabric_s {
	struct list_head   qe;		
	bfa_sm_t	 sm;		
	struct bfa_fcs_s *fcs;		
	struct bfa_fcs_lport_s  bport;	
	enum bfa_fcs_fabric_type fab_type; 
	enum bfa_port_type oper_type;	
	u8         is_vf;		
	u8         is_npiv;	
	u8         is_auth;	
	u16        bb_credit;	
	u16        vf_id;		
	u16        num_vports;	
	u16        rsvd;
	struct list_head         vport_q;	
	struct list_head         vf_q;	
	struct bfad_vf_s      *vf_drv;	
	struct bfa_timer_s link_timer;	
	wwn_t           fabric_name;	
	bfa_boolean_t   auth_reqd;	
	struct bfa_timer_s delay_timer;	
	union {
		u16        swp_vfid;
	} event_arg;
	struct bfa_wc_s        wc;	
	struct bfa_vf_stats_s	stats;	
	struct bfa_lps_s	*lps;	
	u8	fabric_ip_addr[BFA_FCS_FABRIC_IPADDR_SZ];
					
};

#define bfa_fcs_fabric_npiv_capable(__f)    ((__f)->is_npiv)
#define bfa_fcs_fabric_is_switched(__f)			\
	((__f)->fab_type == BFA_FCS_FABRIC_SWITCHED)

#define bfa_fcs_vf_t struct bfa_fcs_fabric_s

struct bfa_vf_event_s {
	u32        undefined;
};

struct bfa_fcs_s;
struct bfa_fcs_fabric_s;

#define BFA_FCS_MAX_RPORTS_SUPP  256	

#define bfa_fcs_lport_t struct bfa_fcs_lport_s

#define BFA_FCS_PORT_SYMBNAME_SEPARATOR			" | "

#define BFA_FCS_PORT_SYMBNAME_MODEL_SZ			12
#define BFA_FCS_PORT_SYMBNAME_VERSION_SZ		10
#define BFA_FCS_PORT_SYMBNAME_MACHINENAME_SZ		30
#define BFA_FCS_PORT_SYMBNAME_OSINFO_SZ			48
#define BFA_FCS_PORT_SYMBNAME_OSPATCH_SZ		16

#define BFA_FCS_PORT_DEF_BB_SCN				3

#define bfa_fcs_lport_get_fcid(_lport)	((_lport)->pid)
#define bfa_fcs_lport_get_pwwn(_lport)	((_lport)->port_cfg.pwwn)
#define bfa_fcs_lport_get_nwwn(_lport)	((_lport)->port_cfg.nwwn)
#define bfa_fcs_lport_get_psym_name(_lport)	((_lport)->port_cfg.sym_name)
#define bfa_fcs_lport_is_initiator(_lport)			\
	((_lport)->port_cfg.roles & BFA_LPORT_ROLE_FCP_IM)
#define bfa_fcs_lport_get_nrports(_lport)	\
	((_lport) ? (_lport)->num_rports : 0)

static inline struct bfad_port_s *
bfa_fcs_lport_get_drvport(struct bfa_fcs_lport_s *port)
{
	return port->bfad_port;
}

#define bfa_fcs_lport_get_opertype(_lport)	((_lport)->fabric->oper_type)
#define bfa_fcs_lport_get_fabric_name(_lport)	((_lport)->fabric->fabric_name)
#define bfa_fcs_lport_get_fabric_ipaddr(_lport)		\
		((_lport)->fabric->fabric_ip_addr)


bfa_boolean_t   bfa_fcs_lport_is_online(struct bfa_fcs_lport_s *port);
struct bfa_fcs_lport_s *bfa_fcs_get_base_port(struct bfa_fcs_s *fcs);
void bfa_fcs_lport_get_rports(struct bfa_fcs_lport_s *port,
			      wwn_t rport_wwns[], int *nrports);

wwn_t bfa_fcs_lport_get_rport(struct bfa_fcs_lport_s *port, wwn_t wwn,
			      int index, int nrports, bfa_boolean_t bwwn);

struct bfa_fcs_lport_s *bfa_fcs_lookup_port(struct bfa_fcs_s *fcs,
					    u16 vf_id, wwn_t lpwwn);

void bfa_fcs_lport_get_info(struct bfa_fcs_lport_s *port,
			    struct bfa_lport_info_s *port_info);
void bfa_fcs_lport_get_attr(struct bfa_fcs_lport_s *port,
			    struct bfa_lport_attr_s *port_attr);
void bfa_fcs_lport_get_stats(struct bfa_fcs_lport_s *fcs_port,
			     struct bfa_lport_stats_s *port_stats);
void bfa_fcs_lport_clear_stats(struct bfa_fcs_lport_s *fcs_port);
enum bfa_port_speed bfa_fcs_lport_get_rport_max_speed(
			struct bfa_fcs_lport_s *port);

void bfa_fcs_lport_ms_init(struct bfa_fcs_lport_s *port);
void bfa_fcs_lport_ms_offline(struct bfa_fcs_lport_s *port);
void bfa_fcs_lport_ms_online(struct bfa_fcs_lport_s *port);
void bfa_fcs_lport_ms_fabric_rscn(struct bfa_fcs_lport_s *port);

void bfa_fcs_lport_fdmi_init(struct bfa_fcs_lport_ms_s *ms);
void bfa_fcs_lport_fdmi_offline(struct bfa_fcs_lport_ms_s *ms);
void bfa_fcs_lport_fdmi_online(struct bfa_fcs_lport_ms_s *ms);
void bfa_fcs_lport_uf_recv(struct bfa_fcs_lport_s *lport, struct fchs_s *fchs,
				     u16 len);
void bfa_fcs_lport_attach(struct bfa_fcs_lport_s *lport, struct bfa_fcs_s *fcs,
			u16 vf_id, struct bfa_fcs_vport_s *vport);
void bfa_fcs_lport_init(struct bfa_fcs_lport_s *lport,
				struct bfa_lport_cfg_s *port_cfg);
void            bfa_fcs_lport_online(struct bfa_fcs_lport_s *port);
void            bfa_fcs_lport_offline(struct bfa_fcs_lport_s *port);
void            bfa_fcs_lport_delete(struct bfa_fcs_lport_s *port);
struct bfa_fcs_rport_s *bfa_fcs_lport_get_rport_by_pid(
		struct bfa_fcs_lport_s *port, u32 pid);
struct bfa_fcs_rport_s *bfa_fcs_lport_get_rport_by_pwwn(
		struct bfa_fcs_lport_s *port, wwn_t pwwn);
struct bfa_fcs_rport_s *bfa_fcs_lport_get_rport_by_nwwn(
		struct bfa_fcs_lport_s *port, wwn_t nwwn);
void            bfa_fcs_lport_add_rport(struct bfa_fcs_lport_s *port,
				       struct bfa_fcs_rport_s *rport);
void            bfa_fcs_lport_del_rport(struct bfa_fcs_lport_s *port,
				       struct bfa_fcs_rport_s *rport);
void            bfa_fcs_lport_ns_init(struct bfa_fcs_lport_s *vport);
void            bfa_fcs_lport_ns_offline(struct bfa_fcs_lport_s *vport);
void            bfa_fcs_lport_ns_online(struct bfa_fcs_lport_s *vport);
void            bfa_fcs_lport_ns_query(struct bfa_fcs_lport_s *port);
void            bfa_fcs_lport_scn_init(struct bfa_fcs_lport_s *vport);
void            bfa_fcs_lport_scn_offline(struct bfa_fcs_lport_s *vport);
void            bfa_fcs_lport_scn_online(struct bfa_fcs_lport_s *vport);
void            bfa_fcs_lport_scn_process_rscn(struct bfa_fcs_lport_s *port,
					      struct fchs_s *rx_frame, u32 len);

struct bfa_fcs_vport_s {
	struct list_head		qe;		
	bfa_sm_t		sm;		
	bfa_fcs_lport_t		lport;		
	struct bfa_timer_s	timer;
	struct bfad_vport_s	*vport_drv;	
	struct bfa_vport_stats_s vport_stats;	
	struct bfa_lps_s	*lps;		
	int			fdisc_retries;
};

#define bfa_fcs_vport_get_port(vport)			\
	((struct bfa_fcs_lport_s  *)(&vport->port))

bfa_status_t bfa_fcs_vport_create(struct bfa_fcs_vport_s *vport,
				  struct bfa_fcs_s *fcs, u16 vf_id,
				  struct bfa_lport_cfg_s *port_cfg,
				  struct bfad_vport_s *vport_drv);
bfa_status_t bfa_fcs_pbc_vport_create(struct bfa_fcs_vport_s *vport,
				      struct bfa_fcs_s *fcs, u16 vf_id,
				      struct bfa_lport_cfg_s *port_cfg,
				      struct bfad_vport_s *vport_drv);
bfa_boolean_t bfa_fcs_is_pbc_vport(struct bfa_fcs_vport_s *vport);
bfa_status_t bfa_fcs_vport_delete(struct bfa_fcs_vport_s *vport);
bfa_status_t bfa_fcs_vport_start(struct bfa_fcs_vport_s *vport);
bfa_status_t bfa_fcs_vport_stop(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_get_attr(struct bfa_fcs_vport_s *vport,
			    struct bfa_vport_attr_s *vport_attr);
struct bfa_fcs_vport_s *bfa_fcs_vport_lookup(struct bfa_fcs_s *fcs,
					     u16 vf_id, wwn_t vpwwn);
void bfa_fcs_vport_cleanup(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_online(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_offline(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_delete_comp(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_fcs_delete(struct bfa_fcs_vport_s *vport);
void bfa_fcs_vport_stop_comp(struct bfa_fcs_vport_s *vport);

#define BFA_FCS_RPORT_DEF_DEL_TIMEOUT	90	
#define BFA_FCS_RPORT_MAX_RETRIES	(5)

struct bfad_rport_s;

struct bfa_fcs_itnim_s;
struct bfa_fcs_tin_s;
struct bfa_fcs_iprp_s;

struct bfa_fcs_rpf_s {
	bfa_sm_t	sm;	
	struct bfa_fcs_rport_s *rport;	
	struct bfa_timer_s	timer;	
	struct bfa_fcxp_s	*fcxp;	
	struct bfa_fcxp_wqe_s	fcxp_wqe; 
	int	rpsc_retries;	
	enum bfa_port_speed	rpsc_speed;
	
	enum bfa_port_speed	assigned_speed;
};

struct bfa_fcs_rport_s {
	struct list_head	qe;	
	struct bfa_fcs_lport_s *port;	
	struct bfa_fcs_s	*fcs;	
	struct bfad_rport_s	*rp_drv;	
	u32	pid;	
	u16	maxfrsize;	
	__be16	reply_oxid;	
	enum fc_cos	fc_cos;	
	bfa_boolean_t	cisc;	
	bfa_boolean_t	prlo;	
	bfa_boolean_t	plogi_pending;	
	wwn_t	pwwn;	
	wwn_t	nwwn;	
	struct bfa_rport_symname_s psym_name; 
	bfa_sm_t	sm;		
	struct bfa_timer_s timer;	
	struct bfa_fcs_itnim_s *itnim;	
	struct bfa_fcs_tin_s *tin;	
	struct bfa_fcs_iprp_s *iprp;	
	struct bfa_rport_s *bfa_rport;	
	struct bfa_fcxp_s *fcxp;	
	int	plogi_retries;	
	int	ns_retries;	
	struct bfa_fcxp_wqe_s	fcxp_wqe; 
	struct bfa_rport_stats_s stats;	
	enum bfa_rport_function	scsi_function;  
	struct bfa_fcs_rpf_s rpf;	
};

static inline struct bfa_rport_s *
bfa_fcs_rport_get_halrport(struct bfa_fcs_rport_s *rport)
{
	return rport->bfa_rport;
}

void bfa_fcs_rport_get_attr(struct bfa_fcs_rport_s *rport,
			struct bfa_rport_attr_s *attr);
struct bfa_fcs_rport_s *bfa_fcs_rport_lookup(struct bfa_fcs_lport_s *port,
					     wwn_t rpwwn);
struct bfa_fcs_rport_s *bfa_fcs_rport_lookup_by_nwwn(
	struct bfa_fcs_lport_s *port, wwn_t rnwwn);
void bfa_fcs_rport_set_del_timeout(u8 rport_tmo);

void bfa_fcs_rport_uf_recv(struct bfa_fcs_rport_s *rport,
	 struct fchs_s *fchs, u16 len);
void bfa_fcs_rport_scn(struct bfa_fcs_rport_s *rport);

struct bfa_fcs_rport_s *bfa_fcs_rport_create(struct bfa_fcs_lport_s *port,
	 u32 pid);
void bfa_fcs_rport_start(struct bfa_fcs_lport_s *port, struct fchs_s *rx_fchs,
			 struct fc_logi_s *plogi_rsp);
void bfa_fcs_rport_plogi_create(struct bfa_fcs_lport_s *port,
				struct fchs_s *rx_fchs,
				struct fc_logi_s *plogi);
void bfa_fcs_rport_plogi(struct bfa_fcs_rport_s *rport, struct fchs_s *fchs,
			 struct fc_logi_s *plogi);
void bfa_fcs_rport_prlo(struct bfa_fcs_rport_s *rport, __be16 ox_id);

void bfa_fcs_rport_itntm_ack(struct bfa_fcs_rport_s *rport);
void bfa_fcs_rport_fcptm_offline_done(struct bfa_fcs_rport_s *rport);
int  bfa_fcs_rport_get_state(struct bfa_fcs_rport_s *rport);
struct bfa_fcs_rport_s *bfa_fcs_rport_create_by_wwn(
			struct bfa_fcs_lport_s *port, wwn_t wwn);
void  bfa_fcs_rpf_init(struct bfa_fcs_rport_s *rport);
void  bfa_fcs_rpf_rport_online(struct bfa_fcs_rport_s *rport);
void  bfa_fcs_rpf_rport_offline(struct bfa_fcs_rport_s *rport);

struct bfad_itnim_s;

struct bfa_fcs_itnim_s {
	bfa_sm_t		sm;		
	struct bfa_fcs_rport_s	*rport;		
	struct bfad_itnim_s	*itnim_drv;	
	struct bfa_fcs_s	*fcs;		
	struct bfa_timer_s	timer;		
	struct bfa_itnim_s	*bfa_itnim;	
	u32		prli_retries;	
	bfa_boolean_t		seq_rec;	
	bfa_boolean_t		rec_support;	
	bfa_boolean_t		conf_comp;	
	bfa_boolean_t		task_retry_id;	
	struct bfa_fcxp_wqe_s	fcxp_wqe;	
	struct bfa_fcxp_s	*fcxp;		
	struct bfa_itnim_stats_s	stats;	
};
#define bfa_fcs_fcxp_alloc(__fcs)	\
	bfa_fcxp_alloc(NULL, (__fcs)->bfa, 0, 0, NULL, NULL, NULL, NULL)

#define bfa_fcs_fcxp_alloc_wait(__bfa, __wqe, __alloc_cbfn, __alloc_cbarg) \
	bfa_fcxp_alloc_wait(__bfa, __wqe, __alloc_cbfn, __alloc_cbarg, \
					NULL, 0, 0, NULL, NULL, NULL, NULL)

static inline struct bfad_port_s *
bfa_fcs_itnim_get_drvport(struct bfa_fcs_itnim_s *itnim)
{
	return itnim->rport->port->bfad_port;
}


static inline struct bfa_fcs_lport_s *
bfa_fcs_itnim_get_port(struct bfa_fcs_itnim_s *itnim)
{
	return itnim->rport->port;
}


static inline wwn_t
bfa_fcs_itnim_get_nwwn(struct bfa_fcs_itnim_s *itnim)
{
	return itnim->rport->nwwn;
}


static inline wwn_t
bfa_fcs_itnim_get_pwwn(struct bfa_fcs_itnim_s *itnim)
{
	return itnim->rport->pwwn;
}


static inline u32
bfa_fcs_itnim_get_fcid(struct bfa_fcs_itnim_s *itnim)
{
	return itnim->rport->pid;
}


static inline	u32
bfa_fcs_itnim_get_maxfrsize(struct bfa_fcs_itnim_s *itnim)
{
	return itnim->rport->maxfrsize;
}


static inline	enum fc_cos
bfa_fcs_itnim_get_cos(struct bfa_fcs_itnim_s *itnim)
{
	return itnim->rport->fc_cos;
}


static inline struct bfad_itnim_s *
bfa_fcs_itnim_get_drvitn(struct bfa_fcs_itnim_s *itnim)
{
	return itnim->itnim_drv;
}


static inline struct bfa_itnim_s *
bfa_fcs_itnim_get_halitn(struct bfa_fcs_itnim_s *itnim)
{
	return itnim->bfa_itnim;
}

void bfa_fcs_itnim_get_attr(struct bfa_fcs_itnim_s *itnim,
			    struct bfa_itnim_attr_s *attr);
void bfa_fcs_itnim_get_stats(struct bfa_fcs_itnim_s *itnim,
			     struct bfa_itnim_stats_s *stats);
struct bfa_fcs_itnim_s *bfa_fcs_itnim_lookup(struct bfa_fcs_lport_s *port,
					     wwn_t rpwwn);
bfa_status_t bfa_fcs_itnim_attr_get(struct bfa_fcs_lport_s *port, wwn_t rpwwn,
				    struct bfa_itnim_attr_s *attr);
bfa_status_t bfa_fcs_itnim_stats_get(struct bfa_fcs_lport_s *port, wwn_t rpwwn,
				     struct bfa_itnim_stats_s *stats);
bfa_status_t bfa_fcs_itnim_stats_clear(struct bfa_fcs_lport_s *port,
				       wwn_t rpwwn);
struct bfa_fcs_itnim_s *bfa_fcs_itnim_create(struct bfa_fcs_rport_s *rport);
void bfa_fcs_itnim_delete(struct bfa_fcs_itnim_s *itnim);
void bfa_fcs_itnim_rport_offline(struct bfa_fcs_itnim_s *itnim);
void bfa_fcs_itnim_rport_online(struct bfa_fcs_itnim_s *itnim);
bfa_status_t bfa_fcs_itnim_get_online_state(struct bfa_fcs_itnim_s *itnim);
void bfa_fcs_itnim_is_initiator(struct bfa_fcs_itnim_s *itnim);
void bfa_fcs_fcpim_uf_recv(struct bfa_fcs_itnim_s *itnim,
			struct fchs_s *fchs, u16 len);

#define BFA_FCS_FDMI_SUPP_SPEEDS_4G	(FDMI_TRANS_SPEED_1G  |	\
				FDMI_TRANS_SPEED_2G |		\
				FDMI_TRANS_SPEED_4G)

#define BFA_FCS_FDMI_SUPP_SPEEDS_8G	(FDMI_TRANS_SPEED_1G  |	\
				FDMI_TRANS_SPEED_2G |		\
				FDMI_TRANS_SPEED_4G |		\
				FDMI_TRANS_SPEED_8G)

#define BFA_FCS_FDMI_SUPP_SPEEDS_16G	(FDMI_TRANS_SPEED_2G  |	\
				FDMI_TRANS_SPEED_4G |		\
				FDMI_TRANS_SPEED_8G |		\
				FDMI_TRANS_SPEED_16G)

#define BFA_FCS_FDMI_SUPP_SPEEDS_10G	FDMI_TRANS_SPEED_10G

struct bfa_fcs_fdmi_hba_attr_s {
	wwn_t           node_name;
	u8         manufacturer[64];
	u8         serial_num[64];
	u8         model[16];
	u8         model_desc[256];
	u8         hw_version[8];
	u8         driver_version[8];
	u8         option_rom_ver[BFA_VERSION_LEN];
	u8         fw_version[8];
	u8         os_name[256];
	__be32        max_ct_pyld;
};

struct bfa_fcs_fdmi_port_attr_s {
	u8         supp_fc4_types[32];	
	__be32        supp_speed;	
	__be32        curr_speed;	
	__be32        max_frm_size;	
	u8         os_device_name[256];	
	u8         host_name[256];	
};

struct bfa_fcs_stats_s {
	struct {
		u32	untagged; 
		u32	tagged;	
		u32	vfid_unknown;	
	} uf;
};

struct bfa_fcs_driver_info_s {
	u8	 version[BFA_VERSION_LEN];		
	u8	 host_machine_name[BFA_FCS_OS_STR_LEN];
	u8	 host_os_name[BFA_FCS_OS_STR_LEN]; 
	u8	 host_os_patch[BFA_FCS_OS_STR_LEN]; 
	u8	 os_device_name[BFA_FCS_OS_STR_LEN]; 
};

struct bfa_fcs_s {
	struct bfa_s	  *bfa;	
	struct bfad_s	      *bfad; 
	struct bfa_trc_mod_s  *trcmod;	
	bfa_boolean_t	vf_enabled;	
	bfa_boolean_t	fdmi_enabled;	
	bfa_boolean_t	bbscn_enabled;	
	bfa_boolean_t	bbscn_flogi_rjt;
	bfa_boolean_t min_cfg;		
	u16	port_vfid;	
	struct bfa_fcs_driver_info_s driver_info;
	struct bfa_fcs_fabric_s fabric; 
	struct bfa_fcs_stats_s	stats;	
	struct bfa_wc_s		wc;	
	int			fcs_aen_seq;
};


enum bfa_fcs_fabric_event {
	BFA_FCS_FABRIC_SM_CREATE        = 1,    
	BFA_FCS_FABRIC_SM_DELETE        = 2,    
	BFA_FCS_FABRIC_SM_LINK_DOWN     = 3,    
	BFA_FCS_FABRIC_SM_LINK_UP       = 4,    
	BFA_FCS_FABRIC_SM_CONT_OP       = 5,    
	BFA_FCS_FABRIC_SM_RETRY_OP      = 6,    
	BFA_FCS_FABRIC_SM_NO_FABRIC     = 7,    
	BFA_FCS_FABRIC_SM_PERF_EVFP     = 8,    
	BFA_FCS_FABRIC_SM_ISOLATE       = 9,    
	BFA_FCS_FABRIC_SM_NO_TAGGING    = 10,   
	BFA_FCS_FABRIC_SM_DELAYED       = 11,   
	BFA_FCS_FABRIC_SM_AUTH_FAILED   = 12,   
	BFA_FCS_FABRIC_SM_AUTH_SUCCESS  = 13,   
	BFA_FCS_FABRIC_SM_DELCOMP       = 14,   
	BFA_FCS_FABRIC_SM_LOOPBACK      = 15,   
	BFA_FCS_FABRIC_SM_START         = 16,   
};


enum rport_event {
	RPSM_EVENT_PLOGI_SEND   = 1,    
	RPSM_EVENT_PLOGI_RCVD   = 2,    
	RPSM_EVENT_PLOGI_COMP   = 3,    
	RPSM_EVENT_LOGO_RCVD    = 4,    
	RPSM_EVENT_LOGO_IMP     = 5,    
	RPSM_EVENT_FCXP_SENT    = 6,    
	RPSM_EVENT_DELETE       = 7,    
	RPSM_EVENT_SCN          = 8,    
	RPSM_EVENT_ACCEPTED     = 9,    
	RPSM_EVENT_FAILED       = 10,   
	RPSM_EVENT_TIMEOUT      = 11,   
	RPSM_EVENT_HCB_ONLINE  = 12,    
	RPSM_EVENT_HCB_OFFLINE = 13,    
	RPSM_EVENT_FC4_OFFLINE = 14,    
	RPSM_EVENT_ADDRESS_CHANGE = 15, 
	RPSM_EVENT_ADDRESS_DISC = 16,   
	RPSM_EVENT_PRLO_RCVD   = 17,    
	RPSM_EVENT_PLOGI_RETRY = 18,    
};

void bfa_fcs_attach(struct bfa_fcs_s *fcs, struct bfa_s *bfa,
		    struct bfad_s *bfad,
		    bfa_boolean_t min_cfg);
void bfa_fcs_init(struct bfa_fcs_s *fcs);
void bfa_fcs_pbc_vport_init(struct bfa_fcs_s *fcs);
void bfa_fcs_update_cfg(struct bfa_fcs_s *fcs);
void bfa_fcs_driver_info_init(struct bfa_fcs_s *fcs,
			      struct bfa_fcs_driver_info_s *driver_info);
void bfa_fcs_exit(struct bfa_fcs_s *fcs);

bfa_fcs_vf_t *bfa_fcs_vf_lookup(struct bfa_fcs_s *fcs, u16 vf_id);
void bfa_fcs_vf_get_ports(bfa_fcs_vf_t *vf, wwn_t vpwwn[], int *nports);

void bfa_fcs_fabric_attach(struct bfa_fcs_s *fcs);
void bfa_fcs_fabric_modinit(struct bfa_fcs_s *fcs);
void bfa_fcs_fabric_modexit(struct bfa_fcs_s *fcs);
void bfa_fcs_fabric_link_up(struct bfa_fcs_fabric_s *fabric);
void bfa_fcs_fabric_link_down(struct bfa_fcs_fabric_s *fabric);
void bfa_fcs_fabric_addvport(struct bfa_fcs_fabric_s *fabric,
	struct bfa_fcs_vport_s *vport);
void bfa_fcs_fabric_delvport(struct bfa_fcs_fabric_s *fabric,
	struct bfa_fcs_vport_s *vport);
struct bfa_fcs_vport_s *bfa_fcs_fabric_vport_lookup(
		struct bfa_fcs_fabric_s *fabric, wwn_t pwwn);
void bfa_fcs_fabric_modstart(struct bfa_fcs_s *fcs);
void bfa_fcs_fabric_uf_recv(struct bfa_fcs_fabric_s *fabric,
		struct fchs_s *fchs, u16 len);
void	bfa_fcs_fabric_psymb_init(struct bfa_fcs_fabric_s *fabric);
void bfa_fcs_fabric_set_fabric_name(struct bfa_fcs_fabric_s *fabric,
	       wwn_t fabric_name);
u16 bfa_fcs_fabric_get_switch_oui(struct bfa_fcs_fabric_s *fabric);
void bfa_fcs_uf_attach(struct bfa_fcs_s *fcs);
void bfa_fcs_port_attach(struct bfa_fcs_s *fcs);
void bfa_fcs_fabric_sm_online(struct bfa_fcs_fabric_s *fabric,
			enum bfa_fcs_fabric_event event);
void bfa_fcs_fabric_sm_loopback(struct bfa_fcs_fabric_s *fabric,
			enum bfa_fcs_fabric_event event);
void bfa_fcs_fabric_sm_auth_failed(struct bfa_fcs_fabric_s *fabric,
			enum bfa_fcs_fabric_event event);



struct bfad_port_s;
struct bfad_vf_s;
struct bfad_vport_s;
struct bfad_rport_s;

struct bfad_port_s *bfa_fcb_lport_new(struct bfad_s *bfad,
				      struct bfa_fcs_lport_s *port,
				      enum bfa_lport_role roles,
				      struct bfad_vf_s *vf_drv,
				      struct bfad_vport_s *vp_drv);
void bfa_fcb_lport_delete(struct bfad_s *bfad, enum bfa_lport_role roles,
			  struct bfad_vf_s *vf_drv,
			  struct bfad_vport_s *vp_drv);

void bfa_fcb_pbc_vport_create(struct bfad_s *bfad, struct bfi_pbc_vport_s);

bfa_status_t bfa_fcb_rport_alloc(struct bfad_s *bfad,
				 struct bfa_fcs_rport_s **rport,
				 struct bfad_rport_s **rport_drv);

void bfa_fcb_itnim_alloc(struct bfad_s *bfad, struct bfa_fcs_itnim_s **itnim,
			 struct bfad_itnim_s **itnim_drv);
void bfa_fcb_itnim_free(struct bfad_s *bfad,
			struct bfad_itnim_s *itnim_drv);
void bfa_fcb_itnim_online(struct bfad_itnim_s *itnim_drv);
void bfa_fcb_itnim_offline(struct bfad_itnim_s *itnim_drv);

#endif 
