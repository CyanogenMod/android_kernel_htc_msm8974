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


#include "bfad_drv.h"
#include "bfad_im.h"
#include "bfa_fcs.h"
#include "bfa_fcbuild.h"

BFA_TRC_FILE(FCS, FCS);

struct bfa_fcs_mod_s {
	void		(*attach) (struct bfa_fcs_s *fcs);
	void		(*modinit) (struct bfa_fcs_s *fcs);
	void		(*modexit) (struct bfa_fcs_s *fcs);
};

#define BFA_FCS_MODULE(_mod) { _mod ## _modinit, _mod ## _modexit }

static struct bfa_fcs_mod_s fcs_modules[] = {
	{ bfa_fcs_port_attach, NULL, NULL },
	{ bfa_fcs_uf_attach, NULL, NULL },
	{ bfa_fcs_fabric_attach, bfa_fcs_fabric_modinit,
	  bfa_fcs_fabric_modexit },
};


static void
bfa_fcs_exit_comp(void *fcs_cbarg)
{
	struct bfa_fcs_s      *fcs = fcs_cbarg;
	struct bfad_s         *bfad = fcs->bfad;

	complete(&bfad->comp);
}




void
bfa_fcs_attach(struct bfa_fcs_s *fcs, struct bfa_s *bfa, struct bfad_s *bfad,
	       bfa_boolean_t min_cfg)
{
	int		i;
	struct bfa_fcs_mod_s  *mod;

	fcs->bfa = bfa;
	fcs->bfad = bfad;
	fcs->min_cfg = min_cfg;

	bfa->fcs = BFA_TRUE;
	fcbuild_init();

	for (i = 0; i < sizeof(fcs_modules) / sizeof(fcs_modules[0]); i++) {
		mod = &fcs_modules[i];
		if (mod->attach)
			mod->attach(fcs);
	}
}

void
bfa_fcs_init(struct bfa_fcs_s *fcs)
{
	int	i;
	struct bfa_fcs_mod_s  *mod;

	for (i = 0; i < sizeof(fcs_modules) / sizeof(fcs_modules[0]); i++) {
		mod = &fcs_modules[i];
		if (mod->modinit)
			mod->modinit(fcs);
	}
}

void
bfa_fcs_update_cfg(struct bfa_fcs_s *fcs)
{
	struct bfa_fcs_fabric_s *fabric = &fcs->fabric;
	struct bfa_lport_cfg_s *port_cfg = &fabric->bport.port_cfg;
	struct bfa_ioc_s *ioc = &fabric->fcs->bfa->ioc;

	port_cfg->nwwn = ioc->attr->nwwn;
	port_cfg->pwwn = ioc->attr->pwwn;
}

void
bfa_fcs_pbc_vport_init(struct bfa_fcs_s *fcs)
{
	int i, npbc_vports;
	struct bfi_pbc_vport_s pbc_vports[BFI_PBC_MAX_VPORTS];

	
	if (!fcs->min_cfg) {
		npbc_vports =
			bfa_iocfc_get_pbc_vports(fcs->bfa, pbc_vports);
		for (i = 0; i < npbc_vports; i++)
			bfa_fcb_pbc_vport_create(fcs->bfa->bfad, pbc_vports[i]);
	}
}

void
bfa_fcs_driver_info_init(struct bfa_fcs_s *fcs,
			struct bfa_fcs_driver_info_s *driver_info)
{

	fcs->driver_info = *driver_info;

	bfa_fcs_fabric_psymb_init(&fcs->fabric);
}

void
bfa_fcs_exit(struct bfa_fcs_s *fcs)
{
	struct bfa_fcs_mod_s  *mod;
	int		nmods, i;

	bfa_wc_init(&fcs->wc, bfa_fcs_exit_comp, fcs);

	nmods = sizeof(fcs_modules) / sizeof(fcs_modules[0]);

	for (i = 0; i < nmods; i++) {

		mod = &fcs_modules[i];
		if (mod->modexit) {
			bfa_wc_up(&fcs->wc);
			mod->modexit(fcs);
		}
	}

	bfa_wc_wait(&fcs->wc);
}



#define BFA_FCS_FABRIC_RETRY_DELAY	(2000)	
#define BFA_FCS_FABRIC_CLEANUP_DELAY	(10000)	

#define bfa_fcs_fabric_set_opertype(__fabric) do {			\
	if (bfa_fcport_get_topology((__fabric)->fcs->bfa)		\
				== BFA_PORT_TOPOLOGY_P2P) {		\
		if (fabric->fab_type == BFA_FCS_FABRIC_SWITCHED)	\
			(__fabric)->oper_type = BFA_PORT_TYPE_NPORT;	\
		else							\
			(__fabric)->oper_type = BFA_PORT_TYPE_P2P;	\
	} else								\
		(__fabric)->oper_type = BFA_PORT_TYPE_NLPORT;		\
} while (0)

static void bfa_fcs_fabric_init(struct bfa_fcs_fabric_s *fabric);
static void bfa_fcs_fabric_login(struct bfa_fcs_fabric_s *fabric);
static void bfa_fcs_fabric_notify_online(struct bfa_fcs_fabric_s *fabric);
static void bfa_fcs_fabric_notify_offline(struct bfa_fcs_fabric_s *fabric);
static void bfa_fcs_fabric_delay(void *cbarg);
static void bfa_fcs_fabric_delete(struct bfa_fcs_fabric_s *fabric);
static void bfa_fcs_fabric_delete_comp(void *cbarg);
static void bfa_fcs_fabric_process_uf(struct bfa_fcs_fabric_s *fabric,
				      struct fchs_s *fchs, u16 len);
static void bfa_fcs_fabric_process_flogi(struct bfa_fcs_fabric_s *fabric,
					 struct fchs_s *fchs, u16 len);
static void bfa_fcs_fabric_send_flogi_acc(struct bfa_fcs_fabric_s *fabric);
static void bfa_fcs_fabric_flogiacc_comp(void *fcsarg,
					 struct bfa_fcxp_s *fcxp, void *cbarg,
					 bfa_status_t status,
					 u32 rsp_len,
					 u32 resid_len,
					 struct fchs_s *rspfchs);
static u8 bfa_fcs_fabric_oper_bbscn(struct bfa_fcs_fabric_s *fabric);
static bfa_boolean_t bfa_fcs_fabric_is_bbscn_enabled(
				struct bfa_fcs_fabric_s *fabric);

static void	bfa_fcs_fabric_sm_uninit(struct bfa_fcs_fabric_s *fabric,
					 enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_created(struct bfa_fcs_fabric_s *fabric,
					  enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_linkdown(struct bfa_fcs_fabric_s *fabric,
					   enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_flogi(struct bfa_fcs_fabric_s *fabric,
					enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_flogi_retry(struct bfa_fcs_fabric_s *fabric,
					      enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_auth(struct bfa_fcs_fabric_s *fabric,
				       enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_nofabric(struct bfa_fcs_fabric_s *fabric,
					   enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_evfp(struct bfa_fcs_fabric_s *fabric,
				       enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_evfp_done(struct bfa_fcs_fabric_s *fabric,
					    enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_isolated(struct bfa_fcs_fabric_s *fabric,
					   enum bfa_fcs_fabric_event event);
static void	bfa_fcs_fabric_sm_deleting(struct bfa_fcs_fabric_s *fabric,
					   enum bfa_fcs_fabric_event event);
static void
bfa_fcs_fabric_sm_uninit(struct bfa_fcs_fabric_s *fabric,
			 enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_CREATE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_created);
		bfa_fcs_fabric_init(fabric);
		bfa_fcs_lport_init(&fabric->bport, &fabric->bport.port_cfg);
		break;

	case BFA_FCS_FABRIC_SM_LINK_UP:
	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

static void
bfa_fcs_fabric_sm_created(struct bfa_fcs_fabric_s *fabric,
			  enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_START:
		if (bfa_fcport_is_linkup(fabric->fcs->bfa)) {
			bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_flogi);
			bfa_fcs_fabric_login(fabric);
		} else
			bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_linkdown);
		break;

	case BFA_FCS_FABRIC_SM_LINK_UP:
	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		break;

	case BFA_FCS_FABRIC_SM_DELETE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_deleting);
		bfa_fcs_fabric_delete(fabric);
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

static void
bfa_fcs_fabric_sm_linkdown(struct bfa_fcs_fabric_s *fabric,
			   enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_LINK_UP:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_flogi);
		bfa_fcs_fabric_login(fabric);
		break;

	case BFA_FCS_FABRIC_SM_RETRY_OP:
		break;

	case BFA_FCS_FABRIC_SM_DELETE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_deleting);
		bfa_fcs_fabric_delete(fabric);
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

static void
bfa_fcs_fabric_sm_flogi(struct bfa_fcs_fabric_s *fabric,
			enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_CONT_OP:

		bfa_fcport_set_tx_bbcredit(fabric->fcs->bfa,
					   fabric->bb_credit,
					   bfa_fcs_fabric_oper_bbscn(fabric));
		fabric->fab_type = BFA_FCS_FABRIC_SWITCHED;

		if (fabric->auth_reqd && fabric->is_auth) {
			bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_auth);
			bfa_trc(fabric->fcs, event);
		} else {
			bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_online);
			bfa_fcs_fabric_notify_online(fabric);
		}
		break;

	case BFA_FCS_FABRIC_SM_RETRY_OP:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_flogi_retry);
		bfa_timer_start(fabric->fcs->bfa, &fabric->delay_timer,
				bfa_fcs_fabric_delay, fabric,
				BFA_FCS_FABRIC_RETRY_DELAY);
		break;

	case BFA_FCS_FABRIC_SM_LOOPBACK:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_loopback);
		bfa_sm_send_event(fabric->lps, BFA_LPS_SM_OFFLINE);
		bfa_fcs_fabric_set_opertype(fabric);
		break;

	case BFA_FCS_FABRIC_SM_NO_FABRIC:
		fabric->fab_type = BFA_FCS_FABRIC_N2N;
		bfa_fcport_set_tx_bbcredit(fabric->fcs->bfa,
					   fabric->bb_credit,
					   bfa_fcs_fabric_oper_bbscn(fabric));
		bfa_fcs_fabric_notify_online(fabric);
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_nofabric);
		break;

	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_linkdown);
		bfa_sm_send_event(fabric->lps, BFA_LPS_SM_OFFLINE);
		break;

	case BFA_FCS_FABRIC_SM_DELETE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_deleting);
		bfa_sm_send_event(fabric->lps, BFA_LPS_SM_OFFLINE);
		bfa_fcs_fabric_delete(fabric);
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}


static void
bfa_fcs_fabric_sm_flogi_retry(struct bfa_fcs_fabric_s *fabric,
			      enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_DELAYED:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_flogi);
		bfa_fcs_fabric_login(fabric);
		break;

	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_linkdown);
		bfa_timer_stop(&fabric->delay_timer);
		break;

	case BFA_FCS_FABRIC_SM_DELETE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_deleting);
		bfa_timer_stop(&fabric->delay_timer);
		bfa_fcs_fabric_delete(fabric);
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

static void
bfa_fcs_fabric_sm_auth(struct bfa_fcs_fabric_s *fabric,
		       enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_AUTH_FAILED:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_auth_failed);
		bfa_sm_send_event(fabric->lps, BFA_LPS_SM_OFFLINE);
		break;

	case BFA_FCS_FABRIC_SM_AUTH_SUCCESS:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_online);
		bfa_fcs_fabric_notify_online(fabric);
		break;

	case BFA_FCS_FABRIC_SM_PERF_EVFP:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_evfp);
		break;

	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_linkdown);
		bfa_sm_send_event(fabric->lps, BFA_LPS_SM_OFFLINE);
		break;

	case BFA_FCS_FABRIC_SM_DELETE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_deleting);
		bfa_fcs_fabric_delete(fabric);
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

void
bfa_fcs_fabric_sm_auth_failed(struct bfa_fcs_fabric_s *fabric,
			      enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_linkdown);
		bfa_fcs_fabric_notify_offline(fabric);
		break;

	case BFA_FCS_FABRIC_SM_DELETE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_deleting);
		bfa_fcs_fabric_delete(fabric);
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

void
bfa_fcs_fabric_sm_loopback(struct bfa_fcs_fabric_s *fabric,
			   enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_linkdown);
		bfa_fcs_fabric_notify_offline(fabric);
		break;

	case BFA_FCS_FABRIC_SM_DELETE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_deleting);
		bfa_fcs_fabric_delete(fabric);
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

static void
bfa_fcs_fabric_sm_nofabric(struct bfa_fcs_fabric_s *fabric,
			   enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_linkdown);
		bfa_sm_send_event(fabric->lps, BFA_LPS_SM_OFFLINE);
		bfa_fcs_fabric_notify_offline(fabric);
		break;

	case BFA_FCS_FABRIC_SM_DELETE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_deleting);
		bfa_fcs_fabric_delete(fabric);
		break;

	case BFA_FCS_FABRIC_SM_NO_FABRIC:
		bfa_trc(fabric->fcs, fabric->bb_credit);
		bfa_fcport_set_tx_bbcredit(fabric->fcs->bfa,
					   fabric->bb_credit,
					   bfa_fcs_fabric_oper_bbscn(fabric));
		break;

	case BFA_FCS_FABRIC_SM_RETRY_OP:
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

void
bfa_fcs_fabric_sm_online(struct bfa_fcs_fabric_s *fabric,
			 enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_linkdown);
		bfa_sm_send_event(fabric->lps, BFA_LPS_SM_OFFLINE);
		bfa_fcs_fabric_notify_offline(fabric);
		break;

	case BFA_FCS_FABRIC_SM_DELETE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_deleting);
		bfa_fcs_fabric_delete(fabric);
		break;

	case BFA_FCS_FABRIC_SM_AUTH_FAILED:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_auth_failed);
		bfa_sm_send_event(fabric->lps, BFA_LPS_SM_OFFLINE);
		break;

	case BFA_FCS_FABRIC_SM_AUTH_SUCCESS:
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

static void
bfa_fcs_fabric_sm_evfp(struct bfa_fcs_fabric_s *fabric,
		       enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_CONT_OP:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_evfp_done);
		break;

	case BFA_FCS_FABRIC_SM_ISOLATE:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_isolated);
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}

static void
bfa_fcs_fabric_sm_evfp_done(struct bfa_fcs_fabric_s *fabric,
			    enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);
}

static void
bfa_fcs_fabric_sm_isolated(struct bfa_fcs_fabric_s *fabric,
			   enum bfa_fcs_fabric_event event)
{
	struct bfad_s *bfad = (struct bfad_s *)fabric->fcs->bfad;
	char	pwwn_ptr[BFA_STRING_32];

	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);
	wwn2str(pwwn_ptr, fabric->bport.port_cfg.pwwn);

	BFA_LOG(KERN_INFO, bfad, bfa_log_level,
		"Port is isolated due to VF_ID mismatch. "
		"PWWN: %s Port VF_ID: %04x switch port VF_ID: %04x.",
		pwwn_ptr, fabric->fcs->port_vfid,
		fabric->event_arg.swp_vfid);
}

static void
bfa_fcs_fabric_sm_deleting(struct bfa_fcs_fabric_s *fabric,
			   enum bfa_fcs_fabric_event event)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, event);

	switch (event) {
	case BFA_FCS_FABRIC_SM_DELCOMP:
		bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_uninit);
		bfa_wc_down(&fabric->fcs->wc);
		break;

	case BFA_FCS_FABRIC_SM_LINK_UP:
		break;

	case BFA_FCS_FABRIC_SM_LINK_DOWN:
		bfa_fcs_fabric_notify_offline(fabric);
		break;

	default:
		bfa_sm_fault(fabric->fcs, event);
	}
}




static void
bfa_fcs_fabric_init(struct bfa_fcs_fabric_s *fabric)
{
	struct bfa_lport_cfg_s *port_cfg = &fabric->bport.port_cfg;

	port_cfg->roles = BFA_LPORT_ROLE_FCP_IM;
	port_cfg->nwwn = fabric->fcs->bfa->ioc.attr->nwwn;
	port_cfg->pwwn = fabric->fcs->bfa->ioc.attr->pwwn;
}

void
bfa_fcs_fabric_psymb_init(struct bfa_fcs_fabric_s *fabric)
{
	struct bfa_lport_cfg_s *port_cfg = &fabric->bport.port_cfg;
	char model[BFA_ADAPTER_MODEL_NAME_LEN] = {0};
	struct bfa_fcs_driver_info_s *driver_info = &fabric->fcs->driver_info;

	bfa_ioc_get_adapter_model(&fabric->fcs->bfa->ioc, model);

	
	strncpy((char *)&port_cfg->sym_name, model,
		BFA_FCS_PORT_SYMBNAME_MODEL_SZ);
	strncat((char *)&port_cfg->sym_name, BFA_FCS_PORT_SYMBNAME_SEPARATOR,
		sizeof(BFA_FCS_PORT_SYMBNAME_SEPARATOR));

	
	strncat((char *)&port_cfg->sym_name, (char *)driver_info->version,
		BFA_FCS_PORT_SYMBNAME_VERSION_SZ);
	strncat((char *)&port_cfg->sym_name, BFA_FCS_PORT_SYMBNAME_SEPARATOR,
		sizeof(BFA_FCS_PORT_SYMBNAME_SEPARATOR));

	
	strncat((char *)&port_cfg->sym_name,
		(char *)driver_info->host_machine_name,
		BFA_FCS_PORT_SYMBNAME_MACHINENAME_SZ);
	strncat((char *)&port_cfg->sym_name, BFA_FCS_PORT_SYMBNAME_SEPARATOR,
		sizeof(BFA_FCS_PORT_SYMBNAME_SEPARATOR));

	if (driver_info->host_os_patch[0] == '\0') {
		strncat((char *)&port_cfg->sym_name,
			(char *)driver_info->host_os_name,
			BFA_FCS_OS_STR_LEN);
		strncat((char *)&port_cfg->sym_name,
			BFA_FCS_PORT_SYMBNAME_SEPARATOR,
			sizeof(BFA_FCS_PORT_SYMBNAME_SEPARATOR));
	} else {
		strncat((char *)&port_cfg->sym_name,
			(char *)driver_info->host_os_name,
			BFA_FCS_PORT_SYMBNAME_OSINFO_SZ);
		strncat((char *)&port_cfg->sym_name,
			BFA_FCS_PORT_SYMBNAME_SEPARATOR,
			sizeof(BFA_FCS_PORT_SYMBNAME_SEPARATOR));

		
		strncat((char *)&port_cfg->sym_name,
			(char *)driver_info->host_os_patch,
			BFA_FCS_PORT_SYMBNAME_OSPATCH_SZ);
	}

	
	port_cfg->sym_name.symname[BFA_SYMNAME_MAXLEN - 1] = 0;
}

void
bfa_cb_lps_flogi_comp(void *bfad, void *uarg, bfa_status_t status)
{
	struct bfa_fcs_fabric_s *fabric = uarg;

	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_trc(fabric->fcs, status);

	switch (status) {
	case BFA_STATUS_OK:
		fabric->stats.flogi_accepts++;
		break;

	case BFA_STATUS_INVALID_MAC:
		
		fabric->stats.flogi_acc_err++;
		bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_RETRY_OP);

		return;

	case BFA_STATUS_EPROTOCOL:
		switch (fabric->lps->ext_status) {
		case BFA_EPROTO_BAD_ACCEPT:
			fabric->stats.flogi_acc_err++;
			break;

		case BFA_EPROTO_UNKNOWN_RSP:
			fabric->stats.flogi_unknown_rsp++;
			break;

		default:
			break;
		}
		bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_RETRY_OP);

		return;

	case BFA_STATUS_FABRIC_RJT:
		fabric->stats.flogi_rejects++;
		if (fabric->lps->lsrjt_rsn == FC_LS_RJT_RSN_LOGICAL_ERROR &&
		    fabric->lps->lsrjt_expl == FC_LS_RJT_EXP_NO_ADDL_INFO)
			fabric->fcs->bbscn_flogi_rjt = BFA_TRUE;

		bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_RETRY_OP);
		return;

	default:
		fabric->stats.flogi_rsp_err++;
		bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_RETRY_OP);
		return;
	}

	fabric->bb_credit = fabric->lps->pr_bbcred;
	bfa_trc(fabric->fcs, fabric->bb_credit);

	if (!(fabric->lps->brcd_switch))
		fabric->fabric_name =  fabric->lps->pr_nwwn;

	if (fabric->lps->fport) {
		fabric->bport.pid = fabric->lps->lp_pid;
		fabric->is_npiv = fabric->lps->npiv_en;
		fabric->is_auth = fabric->lps->auth_req;
		bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_CONT_OP);
	} else {
		fabric->bport.port_topo.pn2n.rem_port_wwn =
			fabric->lps->pr_pwwn;
		fabric->fab_type = BFA_FCS_FABRIC_N2N;
		bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_NO_FABRIC);
	}

	bfa_trc(fabric->fcs, fabric->bport.pid);
	bfa_trc(fabric->fcs, fabric->is_npiv);
	bfa_trc(fabric->fcs, fabric->is_auth);
}
static void
bfa_fcs_fabric_login(struct bfa_fcs_fabric_s *fabric)
{
	struct bfa_s		*bfa = fabric->fcs->bfa;
	struct bfa_lport_cfg_s	*pcfg = &fabric->bport.port_cfg;
	u8			alpa = 0, bb_scn = 0;

	if (bfa_fcport_get_topology(bfa) == BFA_PORT_TOPOLOGY_LOOP)
		alpa = bfa_fcport_get_myalpa(bfa);

	if (bfa_fcs_fabric_is_bbscn_enabled(fabric) &&
	    (!fabric->fcs->bbscn_flogi_rjt))
		bb_scn = BFA_FCS_PORT_DEF_BB_SCN;

	bfa_lps_flogi(fabric->lps, fabric, alpa, bfa_fcport_get_maxfrsize(bfa),
		      pcfg->pwwn, pcfg->nwwn, fabric->auth_reqd, bb_scn);

	fabric->stats.flogi_sent++;
}

static void
bfa_fcs_fabric_notify_online(struct bfa_fcs_fabric_s *fabric)
{
	struct bfa_fcs_vport_s *vport;
	struct list_head	      *qe, *qen;

	bfa_trc(fabric->fcs, fabric->fabric_name);

	bfa_fcs_fabric_set_opertype(fabric);
	fabric->stats.fabric_onlines++;

	bfa_fcs_lport_online(&fabric->bport);

	list_for_each_safe(qe, qen, &fabric->vport_q) {
		vport = (struct bfa_fcs_vport_s *) qe;
		bfa_fcs_vport_online(vport);
	}
}

static void
bfa_fcs_fabric_notify_offline(struct bfa_fcs_fabric_s *fabric)
{
	struct bfa_fcs_vport_s *vport;
	struct list_head	      *qe, *qen;

	bfa_trc(fabric->fcs, fabric->fabric_name);
	fabric->stats.fabric_offlines++;

	list_for_each_safe(qe, qen, &fabric->vport_q) {
		vport = (struct bfa_fcs_vport_s *) qe;
		bfa_fcs_vport_offline(vport);
	}

	bfa_fcs_lport_offline(&fabric->bport);

	fabric->fabric_name = 0;
	fabric->fabric_ip_addr[0] = 0;
}

static void
bfa_fcs_fabric_delay(void *cbarg)
{
	struct bfa_fcs_fabric_s *fabric = cbarg;

	bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_DELAYED);
}

static u8
bfa_fcs_fabric_oper_bbscn(struct bfa_fcs_fabric_s *fabric)
{
	u8	pr_bbscn = fabric->lps->pr_bbscn;
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(fabric->fcs->bfa);

	if (!(fcport->cfg.bb_scn_state && pr_bbscn))
		return 0;

	
	return ((pr_bbscn > BFA_FCS_PORT_DEF_BB_SCN) ?
		pr_bbscn : BFA_FCS_PORT_DEF_BB_SCN);
}

static bfa_boolean_t
bfa_fcs_fabric_is_bbscn_enabled(struct bfa_fcs_fabric_s *fabric)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(fabric->fcs->bfa);

	if (bfa_ioc_get_fcmode(&fabric->fcs->bfa->ioc) &&
			fcport->cfg.bb_scn_state &&
			!bfa_fcport_is_qos_enabled(fabric->fcs->bfa) &&
			!bfa_fcport_is_trunk_enabled(fabric->fcs->bfa))
		return BFA_TRUE;
	else
		return BFA_FALSE;
}

static void
bfa_fcs_fabric_delete(struct bfa_fcs_fabric_s *fabric)
{
	struct bfa_fcs_vport_s *vport;
	struct list_head	      *qe, *qen;

	list_for_each_safe(qe, qen, &fabric->vport_q) {
		vport = (struct bfa_fcs_vport_s *) qe;
		bfa_fcs_vport_fcs_delete(vport);
	}

	bfa_fcs_lport_delete(&fabric->bport);
	bfa_wc_wait(&fabric->wc);
}

static void
bfa_fcs_fabric_delete_comp(void *cbarg)
{
	struct bfa_fcs_fabric_s *fabric = cbarg;

	bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_DELCOMP);
}


void
bfa_fcs_fabric_attach(struct bfa_fcs_s *fcs)
{
	struct bfa_fcs_fabric_s *fabric;

	fabric = &fcs->fabric;
	memset(fabric, 0, sizeof(struct bfa_fcs_fabric_s));

	fabric->fcs = fcs;
	INIT_LIST_HEAD(&fabric->vport_q);
	INIT_LIST_HEAD(&fabric->vf_q);
	fabric->lps = bfa_lps_alloc(fcs->bfa);
	WARN_ON(!fabric->lps);

	bfa_wc_init(&fabric->wc, bfa_fcs_fabric_delete_comp, fabric);
	bfa_wc_up(&fabric->wc); 

	bfa_sm_set_state(fabric, bfa_fcs_fabric_sm_uninit);
	bfa_fcs_lport_attach(&fabric->bport, fabric->fcs, FC_VF_ID_NULL, NULL);
}

void
bfa_fcs_fabric_modinit(struct bfa_fcs_s *fcs)
{
	bfa_sm_send_event(&fcs->fabric, BFA_FCS_FABRIC_SM_CREATE);
	bfa_trc(fcs, 0);
}

void
bfa_fcs_fabric_modexit(struct bfa_fcs_s *fcs)
{
	struct bfa_fcs_fabric_s *fabric;

	bfa_trc(fcs, 0);

	fabric = &fcs->fabric;
	bfa_lps_delete(fabric->lps);
	bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_DELETE);
}

void
bfa_fcs_fabric_modstart(struct bfa_fcs_s *fcs)
{
	struct bfa_fcs_fabric_s *fabric;

	bfa_trc(fcs, 0);
	fabric = &fcs->fabric;
	bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_START);
}


void
bfa_fcs_fabric_link_up(struct bfa_fcs_fabric_s *fabric)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_LINK_UP);
}

void
bfa_fcs_fabric_link_down(struct bfa_fcs_fabric_s *fabric)
{
	bfa_trc(fabric->fcs, fabric->bport.port_cfg.pwwn);
	fabric->fcs->bbscn_flogi_rjt = BFA_FALSE;
	bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_LINK_DOWN);
}

void
bfa_fcs_fabric_addvport(struct bfa_fcs_fabric_s *fabric,
			struct bfa_fcs_vport_s *vport)
{
	bfa_trc(fabric->fcs, fabric->vf_id);

	list_add_tail(&vport->qe, &fabric->vport_q);
	fabric->num_vports++;
	bfa_wc_up(&fabric->wc);
}

void
bfa_fcs_fabric_delvport(struct bfa_fcs_fabric_s *fabric,
			struct bfa_fcs_vport_s *vport)
{
	list_del(&vport->qe);
	fabric->num_vports--;
	bfa_wc_down(&fabric->wc);
}


struct bfa_fcs_vport_s *
bfa_fcs_fabric_vport_lookup(struct bfa_fcs_fabric_s *fabric, wwn_t pwwn)
{
	struct bfa_fcs_vport_s *vport;
	struct list_head	      *qe;

	list_for_each(qe, &fabric->vport_q) {
		vport = (struct bfa_fcs_vport_s *) qe;
		if (bfa_fcs_lport_get_pwwn(&vport->lport) == pwwn)
			return vport;
	}

	return NULL;
}



u16
bfa_fcs_fabric_get_switch_oui(struct bfa_fcs_fabric_s *fabric)
{
	wwn_t fab_nwwn;
	u8 *tmp;
	u16 oui;

	fab_nwwn = fabric->lps->pr_nwwn;

	tmp = (u8 *)&fab_nwwn;
	oui = (tmp[3] << 8) | tmp[4];

	return oui;
}
void
bfa_fcs_fabric_uf_recv(struct bfa_fcs_fabric_s *fabric, struct fchs_s *fchs,
		       u16 len)
{
	u32	pid = fchs->d_id;
	struct bfa_fcs_vport_s *vport;
	struct list_head	      *qe;
	struct fc_els_cmd_s *els_cmd = (struct fc_els_cmd_s *) (fchs + 1);
	struct fc_logi_s *flogi = (struct fc_logi_s *) els_cmd;

	bfa_trc(fabric->fcs, len);
	bfa_trc(fabric->fcs, pid);

	if ((pid == bfa_ntoh3b(FC_FABRIC_PORT)) &&
	    (els_cmd->els_code == FC_ELS_FLOGI) &&
	    (flogi->port_name == bfa_fcs_lport_get_pwwn(&fabric->bport))) {
		bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_LOOPBACK);
		return;
	}

	if (fchs->d_id == bfa_hton3b(FC_FABRIC_PORT)) {
		bfa_trc(fabric->fcs, pid);
		bfa_fcs_fabric_process_uf(fabric, fchs, len);
		return;
	}

	if (fabric->bport.pid == pid) {
		bfa_trc(fabric->fcs, els_cmd->els_code);
		if (els_cmd->els_code == FC_ELS_AUTH) {
			bfa_trc(fabric->fcs, els_cmd->els_code);
			return;
		}

		bfa_trc(fabric->fcs, *(u8 *) ((u8 *) fchs));
		bfa_fcs_lport_uf_recv(&fabric->bport, fchs, len);
		return;
	}

	list_for_each(qe, &fabric->vport_q) {
		vport = (struct bfa_fcs_vport_s *) qe;
		if (vport->lport.pid == pid) {
			bfa_fcs_lport_uf_recv(&vport->lport, fchs, len);
			return;
		}
	}
	bfa_trc(fabric->fcs, els_cmd->els_code);
	bfa_fcs_lport_uf_recv(&fabric->bport, fchs, len);
}

static void
bfa_fcs_fabric_process_uf(struct bfa_fcs_fabric_s *fabric, struct fchs_s *fchs,
			  u16 len)
{
	struct fc_els_cmd_s *els_cmd = (struct fc_els_cmd_s *) (fchs + 1);

	bfa_trc(fabric->fcs, els_cmd->els_code);

	switch (els_cmd->els_code) {
	case FC_ELS_FLOGI:
		bfa_fcs_fabric_process_flogi(fabric, fchs, len);
		break;

	default:
		break;
	}
}

static void
bfa_fcs_fabric_process_flogi(struct bfa_fcs_fabric_s *fabric,
			struct fchs_s *fchs, u16 len)
{
	struct fc_logi_s *flogi = (struct fc_logi_s *) (fchs + 1);
	struct bfa_fcs_lport_s *bport = &fabric->bport;

	bfa_trc(fabric->fcs, fchs->s_id);

	fabric->stats.flogi_rcvd++;
	if (flogi->csp.port_type) {
		bfa_trc(fabric->fcs, flogi->port_name);
		fabric->stats.flogi_rejected++;
		return;
	}

	fabric->bb_credit = be16_to_cpu(flogi->csp.bbcred);
	fabric->lps->pr_bbscn = (be16_to_cpu(flogi->csp.rxsz) >> 12);
	bport->port_topo.pn2n.rem_port_wwn = flogi->port_name;
	bport->port_topo.pn2n.reply_oxid = fchs->ox_id;

	bfa_fcs_fabric_send_flogi_acc(fabric);
	bfa_sm_send_event(fabric, BFA_FCS_FABRIC_SM_NO_FABRIC);
}

static void
bfa_fcs_fabric_send_flogi_acc(struct bfa_fcs_fabric_s *fabric)
{
	struct bfa_lport_cfg_s *pcfg = &fabric->bport.port_cfg;
	struct bfa_fcs_lport_n2n_s *n2n_port = &fabric->bport.port_topo.pn2n;
	struct bfa_s	  *bfa = fabric->fcs->bfa;
	struct bfa_fcxp_s *fcxp;
	u16	reqlen;
	struct fchs_s	fchs;

	fcxp = bfa_fcs_fcxp_alloc(fabric->fcs);
	if (!fcxp)
		return;

	reqlen = fc_flogi_acc_build(&fchs, bfa_fcxp_get_reqbuf(fcxp),
				    bfa_hton3b(FC_FABRIC_PORT),
				    n2n_port->reply_oxid, pcfg->pwwn,
				    pcfg->nwwn,
				    bfa_fcport_get_maxfrsize(bfa),
				    bfa_fcport_get_rx_bbcredit(bfa),
				    bfa_fcs_fabric_oper_bbscn(fabric));

	bfa_fcxp_send(fcxp, NULL, fabric->vf_id, fabric->lps->bfa_tag,
		      BFA_FALSE, FC_CLASS_3,
		      reqlen, &fchs, bfa_fcs_fabric_flogiacc_comp, fabric,
		      FC_MAX_PDUSZ, 0);
}

static void
bfa_fcs_fabric_flogiacc_comp(void *fcsarg, struct bfa_fcxp_s *fcxp, void *cbarg,
			     bfa_status_t status, u32 rsp_len,
			     u32 resid_len, struct fchs_s *rspfchs)
{
	struct bfa_fcs_fabric_s *fabric = cbarg;

	bfa_trc(fabric->fcs, status);
}


static void
bfa_fcs_fabric_aen_post(struct bfa_fcs_lport_s *port,
			enum bfa_port_aen_event event)
{
	struct bfad_s *bfad = (struct bfad_s *)port->fabric->fcs->bfad;
	struct bfa_aen_entry_s  *aen_entry;

	bfad_get_aen_entry(bfad, aen_entry);
	if (!aen_entry)
		return;

	aen_entry->aen_data.port.pwwn = bfa_fcs_lport_get_pwwn(port);
	aen_entry->aen_data.port.fwwn = bfa_fcs_lport_get_fabric_name(port);

	
	bfad_im_post_vendor_event(aen_entry, bfad, ++port->fcs->fcs_aen_seq,
				  BFA_AEN_CAT_PORT, event);
}

void
bfa_fcs_fabric_set_fabric_name(struct bfa_fcs_fabric_s *fabric,
			       wwn_t fabric_name)
{
	struct bfad_s *bfad = (struct bfad_s *)fabric->fcs->bfad;
	char	pwwn_ptr[BFA_STRING_32];
	char	fwwn_ptr[BFA_STRING_32];

	bfa_trc(fabric->fcs, fabric_name);

	if (fabric->fabric_name == 0) {
		fabric->fabric_name = fabric_name;
	} else {
		fabric->fabric_name = fabric_name;
		wwn2str(pwwn_ptr, bfa_fcs_lport_get_pwwn(&fabric->bport));
		wwn2str(fwwn_ptr,
			bfa_fcs_lport_get_fabric_name(&fabric->bport));
		BFA_LOG(KERN_WARNING, bfad, bfa_log_level,
			"Base port WWN = %s Fabric WWN = %s\n",
			pwwn_ptr, fwwn_ptr);
		bfa_fcs_fabric_aen_post(&fabric->bport,
				BFA_PORT_AEN_FABRIC_NAME_CHANGE);
	}
}

bfa_fcs_vf_t   *
bfa_fcs_vf_lookup(struct bfa_fcs_s *fcs, u16 vf_id)
{
	bfa_trc(fcs, vf_id);
	if (vf_id == FC_VF_ID_NULL)
		return &fcs->fabric;

	return NULL;
}

void
bfa_fcs_vf_get_ports(bfa_fcs_vf_t *vf, wwn_t lpwwn[], int *nlports)
{
	struct list_head *qe;
	struct bfa_fcs_vport_s *vport;
	int	i = 0;
	struct bfa_fcs_s	*fcs;

	if (vf == NULL || lpwwn == NULL || *nlports == 0)
		return;

	fcs = vf->fcs;

	bfa_trc(fcs, vf->vf_id);
	bfa_trc(fcs, (uint32_t) *nlports);

	lpwwn[i++] = vf->bport.port_cfg.pwwn;

	list_for_each(qe, &vf->vport_q) {
		if (i >= *nlports)
			break;

		vport = (struct bfa_fcs_vport_s *) qe;
		lpwwn[i++] = vport->lport.port_cfg.pwwn;
	}

	bfa_trc(fcs, i);
	*nlports = i;
}

static void
bfa_fcs_port_event_handler(void *cbarg, enum bfa_port_linkstate event)
{
	struct bfa_fcs_s      *fcs = cbarg;

	bfa_trc(fcs, event);

	switch (event) {
	case BFA_PORT_LINKUP:
		bfa_fcs_fabric_link_up(&fcs->fabric);
		break;

	case BFA_PORT_LINKDOWN:
		bfa_fcs_fabric_link_down(&fcs->fabric);
		break;

	default:
		WARN_ON(1);
	}
}

void
bfa_fcs_port_attach(struct bfa_fcs_s *fcs)
{
	bfa_fcport_event_register(fcs->bfa, bfa_fcs_port_event_handler, fcs);
}


static void
bfa_fcs_uf_recv(void *cbarg, struct bfa_uf_s *uf)
{
	struct bfa_fcs_s	*fcs = (struct bfa_fcs_s *) cbarg;
	struct fchs_s	*fchs = bfa_uf_get_frmbuf(uf);
	u16	len = bfa_uf_get_frmlen(uf);
	struct fc_vft_s *vft;
	struct bfa_fcs_fabric_s *fabric;

	if (fchs->routing == FC_RTG_EXT_HDR &&
	    fchs->cat_info == FC_CAT_VFT_HDR) {
		bfa_stats(fcs, uf.tagged);
		vft = bfa_uf_get_frmbuf(uf);
		if (fcs->port_vfid == vft->vf_id)
			fabric = &fcs->fabric;
		else
			fabric = bfa_fcs_vf_lookup(fcs, (u16) vft->vf_id);

		if (!fabric) {
			WARN_ON(1);
			bfa_stats(fcs, uf.vfid_unknown);
			bfa_uf_free(uf);
			return;
		}

		fchs = (struct fchs_s *) (vft + 1);
		len -= sizeof(struct fc_vft_s);

		bfa_trc(fcs, vft->vf_id);
	} else {
		bfa_stats(fcs, uf.untagged);
		fabric = &fcs->fabric;
	}

	bfa_trc(fcs, ((u32 *) fchs)[0]);
	bfa_trc(fcs, ((u32 *) fchs)[1]);
	bfa_trc(fcs, ((u32 *) fchs)[2]);
	bfa_trc(fcs, ((u32 *) fchs)[3]);
	bfa_trc(fcs, ((u32 *) fchs)[4]);
	bfa_trc(fcs, ((u32 *) fchs)[5]);
	bfa_trc(fcs, len);

	bfa_fcs_fabric_uf_recv(fabric, fchs, len);
	bfa_uf_free(uf);
}

void
bfa_fcs_uf_attach(struct bfa_fcs_s *fcs)
{
	bfa_uf_recv_register(fcs->bfa, bfa_fcs_uf_recv, fcs);
}
