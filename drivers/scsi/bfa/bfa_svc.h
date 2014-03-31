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

#ifndef __BFA_SVC_H__
#define __BFA_SVC_H__

#include "bfa_cs.h"
#include "bfi_ms.h"


#define BFA_SGPG_MIN	(16)
#define BFA_SGPG_MAX	(8192)

#define BFA_SGPG_ROUNDUP(_l) (((_l) + (sizeof(struct bfi_sgpg_s) - 1))	\
			      & ~(sizeof(struct bfi_sgpg_s) - 1))

struct bfa_sgpg_wqe_s {
	struct list_head qe;	
	int	nsgpg;		
	int	nsgpg_total;	
	void	(*cbfn) (void *cbarg);	
	void	*cbarg;		
	struct list_head sgpg_q;	
};

struct bfa_sgpg_s {
	struct list_head  qe;	
	struct bfi_sgpg_s *sgpg;	
	union bfi_addr_u sgpg_pa;	
};

#define BFA_SGPG_NPAGE(_nsges)  (((_nsges) / BFI_SGPG_DATA_SGES) + 1)

#define BFA_SGPG_DMA_SEGS	\
	BFI_MEM_DMA_NSEGS(BFA_SGPG_MAX, (uint32_t)sizeof(struct bfi_sgpg_s))

struct bfa_sgpg_mod_s {
	struct bfa_s *bfa;
	int		num_sgpgs;	
	int		free_sgpgs;	
	struct list_head	sgpg_q;		
	struct list_head	sgpg_wait_q;	
	struct bfa_mem_dma_s	dma_seg[BFA_SGPG_DMA_SEGS];
	struct bfa_mem_kva_s	kva_seg;
};
#define BFA_SGPG_MOD(__bfa)	(&(__bfa)->modules.sgpg_mod)
#define BFA_MEM_SGPG_KVA(__bfa) (&(BFA_SGPG_MOD(__bfa)->kva_seg))

bfa_status_t bfa_sgpg_malloc(struct bfa_s *bfa, struct list_head *sgpg_q,
			     int nsgpgs);
void bfa_sgpg_mfree(struct bfa_s *bfa, struct list_head *sgpg_q, int nsgpgs);
void bfa_sgpg_winit(struct bfa_sgpg_wqe_s *wqe,
		    void (*cbfn) (void *cbarg), void *cbarg);
void bfa_sgpg_wait(struct bfa_s *bfa, struct bfa_sgpg_wqe_s *wqe, int nsgpgs);
void bfa_sgpg_wcancel(struct bfa_s *bfa, struct bfa_sgpg_wqe_s *wqe);


#define BFA_FCXP_MIN		(1)
#define BFA_FCXP_MAX		(256)
#define BFA_FCXP_MAX_IBUF_SZ	(2 * 1024 + 256)
#define BFA_FCXP_MAX_LBUF_SZ	(4 * 1024 + 256)

#define BFA_FCXP_DMA_SEGS						\
	BFI_MEM_DMA_NSEGS(BFA_FCXP_MAX,					\
		(u32)BFA_FCXP_MAX_IBUF_SZ + BFA_FCXP_MAX_LBUF_SZ)

struct bfa_fcxp_mod_s {
	struct bfa_s      *bfa;		
	struct bfa_fcxp_s *fcxp_list;	
	u16	num_fcxps;	
	struct list_head  fcxp_free_q;	
	struct list_head  fcxp_active_q;	
	struct list_head  wait_q;		
	struct list_head fcxp_unused_q; 
	u32	req_pld_sz;
	u32	rsp_pld_sz;
	struct bfa_mem_dma_s dma_seg[BFA_FCXP_DMA_SEGS];
	struct bfa_mem_kva_s kva_seg;
};

#define BFA_FCXP_MOD(__bfa)		(&(__bfa)->modules.fcxp_mod)
#define BFA_FCXP_FROM_TAG(__mod, __tag)	(&(__mod)->fcxp_list[__tag])
#define BFA_MEM_FCXP_KVA(__bfa) (&(BFA_FCXP_MOD(__bfa)->kva_seg))

typedef void    (*fcxp_send_cb_t) (struct bfa_s *ioc, struct bfa_fcxp_s *fcxp,
				   void *cb_arg, bfa_status_t req_status,
				   u32 rsp_len, u32 resid_len,
				   struct fchs_s *rsp_fchs);

typedef u64 (*bfa_fcxp_get_sgaddr_t) (void *bfad_fcxp, int sgeid);
typedef u32 (*bfa_fcxp_get_sglen_t) (void *bfad_fcxp, int sgeid);
typedef void (*bfa_cb_fcxp_send_t) (void *bfad_fcxp, struct bfa_fcxp_s *fcxp,
				    void *cbarg, enum bfa_status req_status,
				    u32 rsp_len, u32 resid_len,
				    struct fchs_s *rsp_fchs);
typedef void (*bfa_fcxp_alloc_cbfn_t) (void *cbarg, struct bfa_fcxp_s *fcxp);



struct bfa_fcxp_req_info_s {
	struct bfa_rport_s *bfa_rport;
	struct fchs_s	fchs;	
	u8		cts;	
	u8		class;	
	u16	max_frmsz;	
	u16	vf_id;	
	u8		lp_tag;	
	u32	req_tot_len;	
};

struct bfa_fcxp_rsp_info_s {
	struct fchs_s	rsp_fchs;
	u8		rsp_timeout;
				
	u8		rsvd2[3];
	u32	rsp_maxlen;	
};

struct bfa_fcxp_s {
	struct list_head	qe;		
	bfa_sm_t	sm;		
	void		*caller;	
	struct bfa_fcxp_mod_s *fcxp_mod;
	
	u16	fcxp_tag;	
	struct bfa_fcxp_req_info_s req_info;
	
	struct bfa_fcxp_rsp_info_s rsp_info;
	
	u8	use_ireqbuf;	
	u8		use_irspbuf;	
	u32	nreq_sgles;	
	u32	nrsp_sgles;	
	struct list_head req_sgpg_q;	
	struct list_head req_sgpg_wqe;	
	struct list_head rsp_sgpg_q;	
	struct list_head rsp_sgpg_wqe;	

	bfa_fcxp_get_sgaddr_t req_sga_cbfn;
	
	bfa_fcxp_get_sglen_t req_sglen_cbfn;
	
	bfa_fcxp_get_sgaddr_t rsp_sga_cbfn;
	
	bfa_fcxp_get_sglen_t rsp_sglen_cbfn;
	
	bfa_cb_fcxp_send_t send_cbfn;   
	void		*send_cbarg;	
	struct bfa_sge_s   req_sge[BFA_FCXP_MAX_SGES];
	
	struct bfa_sge_s   rsp_sge[BFA_FCXP_MAX_SGES];
	
	u8		rsp_status;	
	u32	rsp_len;	
	u32	residue_len;	
	struct fchs_s	rsp_fchs;	
	struct bfa_cb_qe_s    hcb_qe;	
	struct bfa_reqq_wait_s	reqq_wqe;
	bfa_boolean_t	reqq_waiting;
};

struct bfa_fcxp_wqe_s {
	struct list_head		qe;
	bfa_fcxp_alloc_cbfn_t	alloc_cbfn;
	void		*alloc_cbarg;
	void		*caller;
	struct bfa_s	*bfa;
	int		nreq_sgles;
	int		nrsp_sgles;
	bfa_fcxp_get_sgaddr_t	req_sga_cbfn;
	bfa_fcxp_get_sglen_t	req_sglen_cbfn;
	bfa_fcxp_get_sgaddr_t	rsp_sga_cbfn;
	bfa_fcxp_get_sglen_t	rsp_sglen_cbfn;
};

#define BFA_FCXP_REQ_PLD(_fcxp)		(bfa_fcxp_get_reqbuf(_fcxp))
#define BFA_FCXP_RSP_FCHS(_fcxp)	(&((_fcxp)->rsp_info.fchs))
#define BFA_FCXP_RSP_PLD(_fcxp)		(bfa_fcxp_get_rspbuf(_fcxp))

#define BFA_FCXP_REQ_PLD_PA(_fcxp)					      \
	bfa_mem_get_dmabuf_pa((_fcxp)->fcxp_mod, (_fcxp)->fcxp_tag,	      \
		(_fcxp)->fcxp_mod->req_pld_sz + (_fcxp)->fcxp_mod->rsp_pld_sz)

#define BFA_FCXP_RSP_PLD_PA(_fcxp)					       \
	(bfa_mem_get_dmabuf_pa((_fcxp)->fcxp_mod, (_fcxp)->fcxp_tag,	       \
	      (_fcxp)->fcxp_mod->req_pld_sz + (_fcxp)->fcxp_mod->rsp_pld_sz) + \
	      (_fcxp)->fcxp_mod->req_pld_sz)

void	bfa_fcxp_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);


enum bfa_rport_event {
	BFA_RPORT_SM_CREATE	= 1,	
	BFA_RPORT_SM_DELETE	= 2,	
	BFA_RPORT_SM_ONLINE	= 3,	
	BFA_RPORT_SM_OFFLINE	= 4,	
	BFA_RPORT_SM_FWRSP	= 5,	
	BFA_RPORT_SM_HWFAIL	= 6,	
	BFA_RPORT_SM_QOS_SCN	= 7,	
	BFA_RPORT_SM_SET_SPEED	= 8,	
	BFA_RPORT_SM_QRESUME	= 9,	
};

#define BFA_RPORT_MIN	4

struct bfa_rport_mod_s {
	struct bfa_rport_s *rps_list;	
	struct list_head	rp_free_q;	
	struct list_head	rp_active_q;	
	struct list_head	rp_unused_q;	
	u16	num_rports;	
	struct bfa_mem_kva_s	kva_seg;
};

#define BFA_RPORT_MOD(__bfa)	(&(__bfa)->modules.rport_mod)
#define BFA_MEM_RPORT_KVA(__bfa) (&(BFA_RPORT_MOD(__bfa)->kva_seg))

#define BFA_RPORT_FROM_TAG(__bfa, _tag)				\
	(BFA_RPORT_MOD(__bfa)->rps_list +			\
	 ((_tag) & (BFA_RPORT_MOD(__bfa)->num_rports - 1)))

void	bfa_rport_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);
void	bfa_rport_res_recfg(struct bfa_s *bfa, u16 num_rport_fw);

struct bfa_rport_info_s {
	u16	max_frmsz;	
	u32	pid:24,	
		lp_tag:8;	
	u32	local_pid:24,	
		cisc:8;	
	u8	fc_class;	
	u8	vf_en;		
	u16	vf_id;		
	enum bfa_port_speed speed;	
};

struct bfa_rport_s {
	struct list_head	qe;	
	bfa_sm_t	sm;		
	struct bfa_s	*bfa;		
	void		*rport_drv;	
	u16	fw_handle;	
	u16	rport_tag;	
	u8	lun_mask;	
	struct bfa_rport_info_s rport_info; 
	struct bfa_reqq_wait_s reqq_wait; 
	struct bfa_cb_qe_s hcb_qe;	
	struct bfa_rport_hal_stats_s stats; 
	struct bfa_rport_qos_attr_s qos_attr;
	union a {
		bfa_status_t	status;	
		void		*fw_msg; 
	} event_arg;
};
#define BFA_RPORT_FC_COS(_rport)	((_rport)->rport_info.fc_class)



#define BFA_UF_MIN	(4)
#define BFA_UF_MAX	(256)

struct bfa_uf_s {
	struct list_head	qe;	
	struct bfa_s		*bfa;	
	u16	uf_tag;		
	u16	vf_id;
	u16	src_rport_handle;
	u16	rsvd;
	u8		*data_ptr;
	u16	data_len;	
	u16	pb_len;		
	void		*buf_kva;	
	u64	buf_pa;		
	struct bfa_cb_qe_s hcb_qe;	
	struct bfa_sge_s sges[BFI_SGE_INLINE_MAX];
};

typedef void (*bfa_cb_uf_recv_t) (void *cbarg, struct bfa_uf_s *uf);

#define BFA_UF_BUFSZ	(2 * 1024 + 256)

struct bfa_uf_buf_s {
	u8	d[BFA_UF_BUFSZ];
};

#define BFA_PER_UF_DMA_SZ	\
	(u32)BFA_ROUNDUP(sizeof(struct bfa_uf_buf_s), BFA_DMA_ALIGN_SZ)

#define BFA_UF_DMA_SEGS BFI_MEM_DMA_NSEGS(BFA_UF_MAX, BFA_PER_UF_DMA_SZ)

struct bfa_uf_mod_s {
	struct bfa_s *bfa;		
	struct bfa_uf_s *uf_list;	
	u16	num_ufs;	
	struct list_head	uf_free_q;	
	struct list_head	uf_posted_q;	
	struct list_head	uf_unused_q;	
	struct bfi_uf_buf_post_s *uf_buf_posts;
	
	bfa_cb_uf_recv_t ufrecv;	
	void		*cbarg;		
	struct bfa_mem_dma_s	dma_seg[BFA_UF_DMA_SEGS];
	struct bfa_mem_kva_s	kva_seg;
};

#define BFA_UF_MOD(__bfa)	(&(__bfa)->modules.uf_mod)
#define BFA_MEM_UF_KVA(__bfa)	(&(BFA_UF_MOD(__bfa)->kva_seg))

#define ufm_pbs_pa(_ufmod, _uftag)					\
	bfa_mem_get_dmabuf_pa(_ufmod, _uftag, BFA_PER_UF_DMA_SZ)

void	bfa_uf_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);
void	bfa_uf_res_recfg(struct bfa_s *bfa, u16 num_uf_fw);

struct bfa_lps_s {
	struct list_head	qe;	
	struct bfa_s	*bfa;		
	bfa_sm_t	sm;		
	u8		bfa_tag;	
	u8		fw_tag;		
	u8		reqq;		
	u8		alpa;		
	u32	lp_pid;		
	bfa_boolean_t	fdisc;		
	bfa_boolean_t	auth_en;	
	bfa_boolean_t	auth_req;	
	bfa_boolean_t	npiv_en;	
	bfa_boolean_t	fport;		
	bfa_boolean_t	brcd_switch;	
	bfa_status_t	status;		
	u16		pdusz;		
	u16		pr_bbcred;	
	u8		pr_bbscn;	
	u8		bb_scn;		
	u8		lsrjt_rsn;	
	u8		lsrjt_expl;	
	u8		lun_mask;	
	wwn_t		pwwn;		
	wwn_t		nwwn;		
	wwn_t		pr_pwwn;	
	wwn_t		pr_nwwn;	
	mac_t		lp_mac;		
	mac_t		fcf_mac;	
	struct bfa_reqq_wait_s	wqe;	
	void		*uarg;		
	struct bfa_cb_qe_s hcb_qe;	
	struct bfi_lps_login_rsp_s *loginrsp;
	bfa_eproto_status_t ext_status;
};

struct bfa_lps_mod_s {
	struct list_head		lps_free_q;
	struct list_head		lps_active_q;
	struct list_head		lps_login_q;
	struct bfa_lps_s	*lps_arr;
	int			num_lps;
	struct bfa_mem_kva_s	kva_seg;
};

#define BFA_LPS_MOD(__bfa)		(&(__bfa)->modules.lps_mod)
#define BFA_LPS_FROM_TAG(__mod, __tag)	(&(__mod)->lps_arr[__tag])
#define BFA_MEM_LPS_KVA(__bfa)	(&(BFA_LPS_MOD(__bfa)->kva_seg))

void	bfa_lps_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);



#define BFA_FCPORT(_bfa)	(&((_bfa)->modules.port))

struct bfa_fcport_ln_s {
	struct bfa_fcport_s	*fcport;
	bfa_sm_t		sm;
	struct bfa_cb_qe_s	ln_qe;	
	enum bfa_port_linkstate ln_event; 
};

struct bfa_fcport_trunk_s {
	struct bfa_trunk_attr_s	attr;
};

struct bfa_fcport_s {
	struct bfa_s		*bfa;	
	bfa_sm_t		sm;	
	wwn_t			nwwn;	
	wwn_t			pwwn;	
	enum bfa_port_speed speed_sup;
	
	enum bfa_port_speed speed;	
	enum bfa_port_topology topology;	
	u8			myalpa;	
	u8			rsvd[3];
	struct bfa_port_cfg_s	cfg;	
	bfa_boolean_t		use_flash_cfg; 
	struct bfa_qos_attr_s  qos_attr;   
	struct bfa_qos_vc_attr_s qos_vc_attr;  
	struct bfa_reqq_wait_s	reqq_wait;
	
	struct bfa_reqq_wait_s	svcreq_wait;
	
	struct bfa_reqq_wait_s	stats_reqq_wait;
	
	void			*event_cbarg;
	void			(*event_cbfn) (void *cbarg,
					       enum bfa_port_linkstate event);
	union {
		union bfi_fcport_i2h_msg_u i2hmsg;
	} event_arg;
	void			*bfad;	
	struct bfa_fcport_ln_s	ln; 
	struct bfa_cb_qe_s	hcb_qe;	
	struct bfa_timer_s	timer;	
	u32		msgtag;	
	u8			*stats_kva;
	u64		stats_pa;
	union bfa_fcport_stats_u *stats;
	bfa_status_t		stats_status; 
	struct list_head	stats_pending_q;
	struct list_head	statsclr_pending_q;
	bfa_boolean_t		stats_qfull;
	u32		stats_reset_time; 
	bfa_boolean_t		diag_busy; 
	bfa_boolean_t		beacon; 
	bfa_boolean_t		link_e2e_beacon; 
	bfa_boolean_t		bbsc_op_state;	
	struct bfa_fcport_trunk_s trunk;
	u16		fcoe_vlan;
	struct bfa_mem_dma_s	fcport_dma;
};

#define BFA_FCPORT_MOD(__bfa)	(&(__bfa)->modules.fcport)
#define BFA_MEM_FCPORT_DMA(__bfa) (&(BFA_FCPORT_MOD(__bfa)->fcport_dma))

void bfa_fcport_init(struct bfa_s *bfa);
void bfa_fcport_isr(struct bfa_s *bfa, struct bfi_msg_s *msg);

bfa_status_t bfa_fcport_enable(struct bfa_s *bfa);
bfa_status_t bfa_fcport_disable(struct bfa_s *bfa);
bfa_status_t bfa_fcport_cfg_speed(struct bfa_s *bfa,
				  enum bfa_port_speed speed);
enum bfa_port_speed bfa_fcport_get_speed(struct bfa_s *bfa);
bfa_status_t bfa_fcport_cfg_topology(struct bfa_s *bfa,
				     enum bfa_port_topology topo);
enum bfa_port_topology bfa_fcport_get_topology(struct bfa_s *bfa);
bfa_status_t bfa_fcport_cfg_hardalpa(struct bfa_s *bfa, u8 alpa);
bfa_boolean_t bfa_fcport_get_hardalpa(struct bfa_s *bfa, u8 *alpa);
u8 bfa_fcport_get_myalpa(struct bfa_s *bfa);
bfa_status_t bfa_fcport_clr_hardalpa(struct bfa_s *bfa);
bfa_status_t bfa_fcport_cfg_maxfrsize(struct bfa_s *bfa, u16 maxsize);
u16 bfa_fcport_get_maxfrsize(struct bfa_s *bfa);
u8 bfa_fcport_get_rx_bbcredit(struct bfa_s *bfa);
void bfa_fcport_get_attr(struct bfa_s *bfa, struct bfa_port_attr_s *attr);
wwn_t bfa_fcport_get_wwn(struct bfa_s *bfa, bfa_boolean_t node);
void bfa_fcport_event_register(struct bfa_s *bfa,
			void (*event_cbfn) (void *cbarg,
			enum bfa_port_linkstate event), void *event_cbarg);
bfa_boolean_t bfa_fcport_is_disabled(struct bfa_s *bfa);
enum bfa_port_speed bfa_fcport_get_ratelim_speed(struct bfa_s *bfa);

void bfa_fcport_set_tx_bbcredit(struct bfa_s *bfa, u16 tx_bbcredit, u8 bb_scn);
bfa_boolean_t     bfa_fcport_is_ratelim(struct bfa_s *bfa);
void bfa_fcport_beacon(void *dev, bfa_boolean_t beacon,
			bfa_boolean_t link_e2e_beacon);
bfa_boolean_t	bfa_fcport_is_linkup(struct bfa_s *bfa);
bfa_status_t bfa_fcport_get_stats(struct bfa_s *bfa,
			struct bfa_cb_pending_q_s *cb);
bfa_status_t bfa_fcport_clear_stats(struct bfa_s *bfa,
			struct bfa_cb_pending_q_s *cb);
bfa_boolean_t bfa_fcport_is_qos_enabled(struct bfa_s *bfa);
bfa_boolean_t bfa_fcport_is_trunk_enabled(struct bfa_s *bfa);
bfa_status_t bfa_fcport_is_pbcdisabled(struct bfa_s *bfa);
void bfa_fcport_cfg_faa(struct bfa_s *bfa, u8 state);

struct bfa_rport_s *bfa_rport_create(struct bfa_s *bfa, void *rport_drv);
void bfa_rport_online(struct bfa_rport_s *rport,
		      struct bfa_rport_info_s *rport_info);
void bfa_rport_speed(struct bfa_rport_s *rport, enum bfa_port_speed speed);
void bfa_cb_rport_online(void *rport);
void bfa_cb_rport_offline(void *rport);
void bfa_cb_rport_qos_scn_flowid(void *rport,
				 struct bfa_rport_qos_attr_s old_qos_attr,
				 struct bfa_rport_qos_attr_s new_qos_attr);
void bfa_cb_rport_qos_scn_prio(void *rport,
			       struct bfa_rport_qos_attr_s old_qos_attr,
			       struct bfa_rport_qos_attr_s new_qos_attr);

#define BFA_RPORT_TAG_INVALID	0xffff
#define BFA_LP_TAG_INVALID	0xff
void	bfa_rport_set_lunmask(struct bfa_s *bfa, struct bfa_rport_s *rp);
void	bfa_rport_unset_lunmask(struct bfa_s *bfa, struct bfa_rport_s *rp);

struct bfa_fcxp_s *bfa_fcxp_alloc(void *bfad_fcxp, struct bfa_s *bfa,
				  int nreq_sgles, int nrsp_sgles,
				  bfa_fcxp_get_sgaddr_t get_req_sga,
				  bfa_fcxp_get_sglen_t get_req_sglen,
				  bfa_fcxp_get_sgaddr_t get_rsp_sga,
				  bfa_fcxp_get_sglen_t get_rsp_sglen);
void bfa_fcxp_alloc_wait(struct bfa_s *bfa, struct bfa_fcxp_wqe_s *wqe,
				bfa_fcxp_alloc_cbfn_t alloc_cbfn,
				void *cbarg, void *bfad_fcxp,
				int nreq_sgles, int nrsp_sgles,
				bfa_fcxp_get_sgaddr_t get_req_sga,
				bfa_fcxp_get_sglen_t get_req_sglen,
				bfa_fcxp_get_sgaddr_t get_rsp_sga,
				bfa_fcxp_get_sglen_t get_rsp_sglen);
void bfa_fcxp_walloc_cancel(struct bfa_s *bfa,
			    struct bfa_fcxp_wqe_s *wqe);
void bfa_fcxp_discard(struct bfa_fcxp_s *fcxp);

void *bfa_fcxp_get_reqbuf(struct bfa_fcxp_s *fcxp);
void *bfa_fcxp_get_rspbuf(struct bfa_fcxp_s *fcxp);

void bfa_fcxp_free(struct bfa_fcxp_s *fcxp);

void bfa_fcxp_send(struct bfa_fcxp_s *fcxp, struct bfa_rport_s *rport,
		   u16 vf_id, u8 lp_tag,
		   bfa_boolean_t cts, enum fc_cos cos,
		   u32 reqlen, struct fchs_s *fchs,
		   bfa_cb_fcxp_send_t cbfn,
		   void *cbarg,
		   u32 rsp_maxlen, u8 rsp_timeout);
bfa_status_t bfa_fcxp_abort(struct bfa_fcxp_s *fcxp);
u32 bfa_fcxp_get_reqbufsz(struct bfa_fcxp_s *fcxp);
u32 bfa_fcxp_get_maxrsp(struct bfa_s *bfa);
void bfa_fcxp_res_recfg(struct bfa_s *bfa, u16 num_fcxp_fw);

static inline void *
bfa_uf_get_frmbuf(struct bfa_uf_s *uf)
{
	return uf->data_ptr;
}

static inline   u16
bfa_uf_get_frmlen(struct bfa_uf_s *uf)
{
	return uf->data_len;
}

void bfa_uf_recv_register(struct bfa_s *bfa, bfa_cb_uf_recv_t ufrecv,
			  void *cbarg);
void bfa_uf_free(struct bfa_uf_s *uf);


u32 bfa_lps_get_max_vport(struct bfa_s *bfa);
struct bfa_lps_s *bfa_lps_alloc(struct bfa_s *bfa);
void bfa_lps_delete(struct bfa_lps_s *lps);
void bfa_lps_flogi(struct bfa_lps_s *lps, void *uarg, u8 alpa,
		   u16 pdusz, wwn_t pwwn, wwn_t nwwn,
		   bfa_boolean_t auth_en, u8 bb_scn);
void bfa_lps_fdisc(struct bfa_lps_s *lps, void *uarg, u16 pdusz,
		   wwn_t pwwn, wwn_t nwwn);
void bfa_lps_fdisclogo(struct bfa_lps_s *lps);
void bfa_lps_set_n2n_pid(struct bfa_lps_s *lps, u32 n2n_pid);
u8 bfa_lps_get_fwtag(struct bfa_s *bfa, u8 lp_tag);
u32 bfa_lps_get_base_pid(struct bfa_s *bfa);
u8 bfa_lps_get_tag_from_pid(struct bfa_s *bfa, u32 pid);
void bfa_cb_lps_flogi_comp(void *bfad, void *uarg, bfa_status_t status);
void bfa_cb_lps_fdisc_comp(void *bfad, void *uarg, bfa_status_t status);
void bfa_cb_lps_fdisclogo_comp(void *bfad, void *uarg);
void bfa_cb_lps_cvl_event(void *bfad, void *uarg);

bfa_status_t bfa_faa_query(struct bfa_s *bfa, struct bfa_faa_attr_s *attr,
			bfa_cb_iocfc_t cbfn, void *cbarg);

struct bfa_fcdiag_qtest_s {
	struct bfa_diag_qtest_result_s *result;
	bfa_cb_diag_t	cbfn;
	void		*cbarg;
	struct bfa_timer_s	timer;
	u32	status;
	u32	count;
	u8	lock;
	u8	queue;
	u8	all;
	u8	timer_active;
};

struct bfa_fcdiag_lb_s {
	bfa_cb_diag_t   cbfn;
	void            *cbarg;
	void            *result;
	bfa_boolean_t   lock;
	u32        status;
};

struct bfa_fcdiag_s {
	struct bfa_s    *bfa;           
	struct bfa_trc_mod_s   *trcmod;
	struct bfa_fcdiag_lb_s lb;
	struct bfa_fcdiag_qtest_s qtest;
};

#define BFA_FCDIAG_MOD(__bfa)	(&(__bfa)->modules.fcdiag)

void	bfa_fcdiag_intr(struct bfa_s *bfa, struct bfi_msg_s *msg);

bfa_status_t	bfa_fcdiag_loopback(struct bfa_s *bfa,
				enum bfa_port_opmode opmode,
				enum bfa_port_speed speed, u32 lpcnt, u32 pat,
				struct bfa_diag_loopback_result_s *result,
				bfa_cb_diag_t cbfn, void *cbarg);
bfa_status_t	bfa_fcdiag_queuetest(struct bfa_s *bfa, u32 ignore,
			u32 queue, struct bfa_diag_qtest_result_s *result,
			bfa_cb_diag_t cbfn, void *cbarg);
bfa_status_t	bfa_fcdiag_lb_is_running(struct bfa_s *bfa);

#endif 
