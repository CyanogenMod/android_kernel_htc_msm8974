/*
 * Copyright(c) 2007 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */



#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include <scsi/fc/fc_gs.h>

#include <scsi/libfc.h>
#include <scsi/fc_encode.h>
#include <linux/scatterlist.h>

#include "fc_libfc.h"

#define FC_LOCAL_PTP_FID_LO   0x010101
#define FC_LOCAL_PTP_FID_HI   0x010102

#define	DNS_DELAY	      3 

static void fc_lport_error(struct fc_lport *, struct fc_frame *);

static void fc_lport_enter_reset(struct fc_lport *);
static void fc_lport_enter_flogi(struct fc_lport *);
static void fc_lport_enter_dns(struct fc_lport *);
static void fc_lport_enter_ns(struct fc_lport *, enum fc_lport_state);
static void fc_lport_enter_scr(struct fc_lport *);
static void fc_lport_enter_ready(struct fc_lport *);
static void fc_lport_enter_logo(struct fc_lport *);
static void fc_lport_enter_fdmi(struct fc_lport *lport);
static void fc_lport_enter_ms(struct fc_lport *, enum fc_lport_state);

static const char *fc_lport_state_names[] = {
	[LPORT_ST_DISABLED] = "disabled",
	[LPORT_ST_FLOGI] =    "FLOGI",
	[LPORT_ST_DNS] =      "dNS",
	[LPORT_ST_RNN_ID] =   "RNN_ID",
	[LPORT_ST_RSNN_NN] =  "RSNN_NN",
	[LPORT_ST_RSPN_ID] =  "RSPN_ID",
	[LPORT_ST_RFT_ID] =   "RFT_ID",
	[LPORT_ST_RFF_ID] =   "RFF_ID",
	[LPORT_ST_FDMI] =     "FDMI",
	[LPORT_ST_RHBA] =     "RHBA",
	[LPORT_ST_RPA] =      "RPA",
	[LPORT_ST_DHBA] =     "DHBA",
	[LPORT_ST_DPRT] =     "DPRT",
	[LPORT_ST_SCR] =      "SCR",
	[LPORT_ST_READY] =    "Ready",
	[LPORT_ST_LOGO] =     "LOGO",
	[LPORT_ST_RESET] =    "reset",
};

struct fc_bsg_info {
	struct fc_bsg_job *job;
	struct fc_lport *lport;
	u16 rsp_code;
	struct scatterlist *sg;
	u32 nents;
	size_t offset;
};

static int fc_frame_drop(struct fc_lport *lport, struct fc_frame *fp)
{
	fc_frame_free(fp);
	return 0;
}

static void fc_lport_rport_callback(struct fc_lport *lport,
				    struct fc_rport_priv *rdata,
				    enum fc_rport_event event)
{
	FC_LPORT_DBG(lport, "Received a %d event for port (%6.6x)\n", event,
		     rdata->ids.port_id);

	mutex_lock(&lport->lp_mutex);
	switch (event) {
	case RPORT_EV_READY:
		if (lport->state == LPORT_ST_DNS) {
			lport->dns_rdata = rdata;
			fc_lport_enter_ns(lport, LPORT_ST_RNN_ID);
		} else if (lport->state == LPORT_ST_FDMI) {
			lport->ms_rdata = rdata;
			fc_lport_enter_ms(lport, LPORT_ST_DHBA);
		} else {
			FC_LPORT_DBG(lport, "Received an READY event "
				     "on port (%6.6x) for the directory "
				     "server, but the lport is not "
				     "in the DNS or FDMI state, it's in the "
				     "%d state", rdata->ids.port_id,
				     lport->state);
			lport->tt.rport_logoff(rdata);
		}
		break;
	case RPORT_EV_LOGO:
	case RPORT_EV_FAILED:
	case RPORT_EV_STOP:
		if (rdata->ids.port_id == FC_FID_DIR_SERV)
			lport->dns_rdata = NULL;
		else if (rdata->ids.port_id == FC_FID_MGMT_SERV)
			lport->ms_rdata = NULL;
		break;
	case RPORT_EV_NONE:
		break;
	}
	mutex_unlock(&lport->lp_mutex);
}

static const char *fc_lport_state(struct fc_lport *lport)
{
	const char *cp;

	cp = fc_lport_state_names[lport->state];
	if (!cp)
		cp = "unknown";
	return cp;
}

static void fc_lport_ptp_setup(struct fc_lport *lport,
			       u32 remote_fid, u64 remote_wwpn,
			       u64 remote_wwnn)
{
	mutex_lock(&lport->disc.disc_mutex);
	if (lport->ptp_rdata) {
		lport->tt.rport_logoff(lport->ptp_rdata);
		kref_put(&lport->ptp_rdata->kref, lport->tt.rport_destroy);
	}
	lport->ptp_rdata = lport->tt.rport_create(lport, remote_fid);
	kref_get(&lport->ptp_rdata->kref);
	lport->ptp_rdata->ids.port_name = remote_wwpn;
	lport->ptp_rdata->ids.node_name = remote_wwnn;
	mutex_unlock(&lport->disc.disc_mutex);

	lport->tt.rport_login(lport->ptp_rdata);

	fc_lport_enter_ready(lport);
}

void fc_get_host_port_state(struct Scsi_Host *shost)
{
	struct fc_lport *lport = shost_priv(shost);

	mutex_lock(&lport->lp_mutex);
	if (!lport->link_up)
		fc_host_port_state(shost) = FC_PORTSTATE_LINKDOWN;
	else
		switch (lport->state) {
		case LPORT_ST_READY:
			fc_host_port_state(shost) = FC_PORTSTATE_ONLINE;
			break;
		default:
			fc_host_port_state(shost) = FC_PORTSTATE_OFFLINE;
		}
	mutex_unlock(&lport->lp_mutex);
}
EXPORT_SYMBOL(fc_get_host_port_state);

void fc_get_host_speed(struct Scsi_Host *shost)
{
	struct fc_lport *lport = shost_priv(shost);

	fc_host_speed(shost) = lport->link_speed;
}
EXPORT_SYMBOL(fc_get_host_speed);

struct fc_host_statistics *fc_get_host_stats(struct Scsi_Host *shost)
{
	struct fc_host_statistics *fcoe_stats;
	struct fc_lport *lport = shost_priv(shost);
	struct timespec v0, v1;
	unsigned int cpu;
	u64 fcp_in_bytes = 0;
	u64 fcp_out_bytes = 0;

	fcoe_stats = &lport->host_stats;
	memset(fcoe_stats, 0, sizeof(struct fc_host_statistics));

	jiffies_to_timespec(jiffies, &v0);
	jiffies_to_timespec(lport->boot_time, &v1);
	fcoe_stats->seconds_since_last_reset = (v0.tv_sec - v1.tv_sec);

	for_each_possible_cpu(cpu) {
		struct fcoe_dev_stats *stats;

		stats = per_cpu_ptr(lport->dev_stats, cpu);

		fcoe_stats->tx_frames += stats->TxFrames;
		fcoe_stats->tx_words += stats->TxWords;
		fcoe_stats->rx_frames += stats->RxFrames;
		fcoe_stats->rx_words += stats->RxWords;
		fcoe_stats->error_frames += stats->ErrorFrames;
		fcoe_stats->invalid_crc_count += stats->InvalidCRCCount;
		fcoe_stats->fcp_input_requests += stats->InputRequests;
		fcoe_stats->fcp_output_requests += stats->OutputRequests;
		fcoe_stats->fcp_control_requests += stats->ControlRequests;
		fcp_in_bytes += stats->InputBytes;
		fcp_out_bytes += stats->OutputBytes;
		fcoe_stats->link_failure_count += stats->LinkFailureCount;
	}
	fcoe_stats->fcp_input_megabytes = div_u64(fcp_in_bytes, 1000000);
	fcoe_stats->fcp_output_megabytes = div_u64(fcp_out_bytes, 1000000);
	fcoe_stats->lip_count = -1;
	fcoe_stats->nos_count = -1;
	fcoe_stats->loss_of_sync_count = -1;
	fcoe_stats->loss_of_signal_count = -1;
	fcoe_stats->prim_seq_protocol_err_count = -1;
	fcoe_stats->dumped_frames = -1;
	return fcoe_stats;
}
EXPORT_SYMBOL(fc_get_host_stats);

static void fc_lport_flogi_fill(struct fc_lport *lport,
				struct fc_els_flogi *flogi,
				unsigned int op)
{
	struct fc_els_csp *sp;
	struct fc_els_cssp *cp;

	memset(flogi, 0, sizeof(*flogi));
	flogi->fl_cmd = (u8) op;
	put_unaligned_be64(lport->wwpn, &flogi->fl_wwpn);
	put_unaligned_be64(lport->wwnn, &flogi->fl_wwnn);
	sp = &flogi->fl_csp;
	sp->sp_hi_ver = 0x20;
	sp->sp_lo_ver = 0x20;
	sp->sp_bb_cred = htons(10);	
	sp->sp_bb_data = htons((u16) lport->mfs);
	cp = &flogi->fl_cssp[3 - 1];	
	cp->cp_class = htons(FC_CPC_VALID | FC_CPC_SEQ);
	if (op != ELS_FLOGI) {
		sp->sp_features = htons(FC_SP_FT_CIRO);
		sp->sp_tot_seq = htons(255);	
		sp->sp_rel_off = htons(0x1f);
		sp->sp_e_d_tov = htonl(lport->e_d_tov);

		cp->cp_rdfs = htons((u16) lport->mfs);
		cp->cp_con_seq = htons(255);
		cp->cp_open_seq = 1;
	}
}

static void fc_lport_add_fc4_type(struct fc_lport *lport, enum fc_fh_type type)
{
	__be32 *mp;

	mp = &lport->fcts.ff_type_map[type / FC_NS_BPW];
	*mp = htonl(ntohl(*mp) | 1UL << (type % FC_NS_BPW));
}

static void fc_lport_recv_rlir_req(struct fc_lport *lport, struct fc_frame *fp)
{
	FC_LPORT_DBG(lport, "Received RLIR request while in state %s\n",
		     fc_lport_state(lport));

	lport->tt.seq_els_rsp_send(fp, ELS_LS_ACC, NULL);
	fc_frame_free(fp);
}

static void fc_lport_recv_echo_req(struct fc_lport *lport,
				   struct fc_frame *in_fp)
{
	struct fc_frame *fp;
	unsigned int len;
	void *pp;
	void *dp;

	FC_LPORT_DBG(lport, "Received ECHO request while in state %s\n",
		     fc_lport_state(lport));

	len = fr_len(in_fp) - sizeof(struct fc_frame_header);
	pp = fc_frame_payload_get(in_fp, len);

	if (len < sizeof(__be32))
		len = sizeof(__be32);

	fp = fc_frame_alloc(lport, len);
	if (fp) {
		dp = fc_frame_payload_get(fp, len);
		memcpy(dp, pp, len);
		*((__be32 *)dp) = htonl(ELS_LS_ACC << 24);
		fc_fill_reply_hdr(fp, in_fp, FC_RCTL_ELS_REP, 0);
		lport->tt.frame_send(lport, fp);
	}
	fc_frame_free(in_fp);
}

static void fc_lport_recv_rnid_req(struct fc_lport *lport,
				   struct fc_frame *in_fp)
{
	struct fc_frame *fp;
	struct fc_els_rnid *req;
	struct {
		struct fc_els_rnid_resp rnid;
		struct fc_els_rnid_cid cid;
		struct fc_els_rnid_gen gen;
	} *rp;
	struct fc_seq_els_data rjt_data;
	u8 fmt;
	size_t len;

	FC_LPORT_DBG(lport, "Received RNID request while in state %s\n",
		     fc_lport_state(lport));

	req = fc_frame_payload_get(in_fp, sizeof(*req));
	if (!req) {
		rjt_data.reason = ELS_RJT_LOGIC;
		rjt_data.explan = ELS_EXPL_NONE;
		lport->tt.seq_els_rsp_send(in_fp, ELS_LS_RJT, &rjt_data);
	} else {
		fmt = req->rnid_fmt;
		len = sizeof(*rp);
		if (fmt != ELS_RNIDF_GEN ||
		    ntohl(lport->rnid_gen.rnid_atype) == 0) {
			fmt = ELS_RNIDF_NONE;	
			len -= sizeof(rp->gen);
		}
		fp = fc_frame_alloc(lport, len);
		if (fp) {
			rp = fc_frame_payload_get(fp, len);
			memset(rp, 0, len);
			rp->rnid.rnid_cmd = ELS_LS_ACC;
			rp->rnid.rnid_fmt = fmt;
			rp->rnid.rnid_cid_len = sizeof(rp->cid);
			rp->cid.rnid_wwpn = htonll(lport->wwpn);
			rp->cid.rnid_wwnn = htonll(lport->wwnn);
			if (fmt == ELS_RNIDF_GEN) {
				rp->rnid.rnid_sid_len = sizeof(rp->gen);
				memcpy(&rp->gen, &lport->rnid_gen,
				       sizeof(rp->gen));
			}
			fc_fill_reply_hdr(fp, in_fp, FC_RCTL_ELS_REP, 0);
			lport->tt.frame_send(lport, fp);
		}
	}
	fc_frame_free(in_fp);
}

static void fc_lport_recv_logo_req(struct fc_lport *lport, struct fc_frame *fp)
{
	lport->tt.seq_els_rsp_send(fp, ELS_LS_ACC, NULL);
	fc_lport_enter_reset(lport);
	fc_frame_free(fp);
}

int fc_fabric_login(struct fc_lport *lport)
{
	int rc = -1;

	mutex_lock(&lport->lp_mutex);
	if (lport->state == LPORT_ST_DISABLED ||
	    lport->state == LPORT_ST_LOGO) {
		fc_lport_state_enter(lport, LPORT_ST_RESET);
		fc_lport_enter_reset(lport);
		rc = 0;
	}
	mutex_unlock(&lport->lp_mutex);

	return rc;
}
EXPORT_SYMBOL(fc_fabric_login);

void __fc_linkup(struct fc_lport *lport)
{
	if (!lport->link_up) {
		lport->link_up = 1;

		if (lport->state == LPORT_ST_RESET)
			fc_lport_enter_flogi(lport);
	}
}

void fc_linkup(struct fc_lport *lport)
{
	printk(KERN_INFO "host%d: libfc: Link up on port (%6.6x)\n",
	       lport->host->host_no, lport->port_id);

	mutex_lock(&lport->lp_mutex);
	__fc_linkup(lport);
	mutex_unlock(&lport->lp_mutex);
}
EXPORT_SYMBOL(fc_linkup);

void __fc_linkdown(struct fc_lport *lport)
{
	if (lport->link_up) {
		lport->link_up = 0;
		fc_lport_enter_reset(lport);
		lport->tt.fcp_cleanup(lport);
	}
}

void fc_linkdown(struct fc_lport *lport)
{
	printk(KERN_INFO "host%d: libfc: Link down on port (%6.6x)\n",
	       lport->host->host_no, lport->port_id);

	mutex_lock(&lport->lp_mutex);
	__fc_linkdown(lport);
	mutex_unlock(&lport->lp_mutex);
}
EXPORT_SYMBOL(fc_linkdown);

int fc_fabric_logoff(struct fc_lport *lport)
{
	lport->tt.disc_stop_final(lport);
	mutex_lock(&lport->lp_mutex);
	if (lport->dns_rdata)
		lport->tt.rport_logoff(lport->dns_rdata);
	mutex_unlock(&lport->lp_mutex);
	lport->tt.rport_flush_queue();
	mutex_lock(&lport->lp_mutex);
	fc_lport_enter_logo(lport);
	mutex_unlock(&lport->lp_mutex);
	cancel_delayed_work_sync(&lport->retry_work);
	return 0;
}
EXPORT_SYMBOL(fc_fabric_logoff);

int fc_lport_destroy(struct fc_lport *lport)
{
	mutex_lock(&lport->lp_mutex);
	lport->state = LPORT_ST_DISABLED;
	lport->link_up = 0;
	lport->tt.frame_send = fc_frame_drop;
	mutex_unlock(&lport->lp_mutex);

	lport->tt.fcp_abort_io(lport);
	lport->tt.disc_stop_final(lport);
	lport->tt.exch_mgr_reset(lport, 0, 0);
	fc_fc4_del_lport(lport);
	return 0;
}
EXPORT_SYMBOL(fc_lport_destroy);

int fc_set_mfs(struct fc_lport *lport, u32 mfs)
{
	unsigned int old_mfs;
	int rc = -EINVAL;

	mutex_lock(&lport->lp_mutex);

	old_mfs = lport->mfs;

	if (mfs >= FC_MIN_MAX_FRAME) {
		mfs &= ~3;
		if (mfs > FC_MAX_FRAME)
			mfs = FC_MAX_FRAME;
		mfs -= sizeof(struct fc_frame_header);
		lport->mfs = mfs;
		rc = 0;
	}

	if (!rc && mfs < old_mfs)
		fc_lport_enter_reset(lport);

	mutex_unlock(&lport->lp_mutex);

	return rc;
}
EXPORT_SYMBOL(fc_set_mfs);

static void fc_lport_disc_callback(struct fc_lport *lport,
				   enum fc_disc_event event)
{
	switch (event) {
	case DISC_EV_SUCCESS:
		FC_LPORT_DBG(lport, "Discovery succeeded\n");
		break;
	case DISC_EV_FAILED:
		printk(KERN_ERR "host%d: libfc: "
		       "Discovery failed for port (%6.6x)\n",
		       lport->host->host_no, lport->port_id);
		mutex_lock(&lport->lp_mutex);
		fc_lport_enter_reset(lport);
		mutex_unlock(&lport->lp_mutex);
		break;
	case DISC_EV_NONE:
		WARN_ON(1);
		break;
	}
}

static void fc_lport_enter_ready(struct fc_lport *lport)
{
	FC_LPORT_DBG(lport, "Entered READY from state %s\n",
		     fc_lport_state(lport));

	fc_lport_state_enter(lport, LPORT_ST_READY);
	if (lport->vport)
		fc_vport_set_state(lport->vport, FC_VPORT_ACTIVE);
	fc_vports_linkchange(lport);

	if (!lport->ptp_rdata)
		lport->tt.disc_start(fc_lport_disc_callback, lport);
}

static void fc_lport_set_port_id(struct fc_lport *lport, u32 port_id,
				 struct fc_frame *fp)
{
	if (port_id)
		printk(KERN_INFO "host%d: Assigned Port ID %6.6x\n",
		       lport->host->host_no, port_id);

	lport->port_id = port_id;

	
	fc_host_port_id(lport->host) = port_id;

	if (lport->tt.lport_set_port_id)
		lport->tt.lport_set_port_id(lport, port_id, fp);
}

void fc_lport_set_local_id(struct fc_lport *lport, u32 port_id)
{
	mutex_lock(&lport->lp_mutex);

	fc_lport_set_port_id(lport, port_id, NULL);

	switch (lport->state) {
	case LPORT_ST_RESET:
	case LPORT_ST_FLOGI:
		if (port_id)
			fc_lport_enter_ready(lport);
		break;
	default:
		break;
	}
	mutex_unlock(&lport->lp_mutex);
}
EXPORT_SYMBOL(fc_lport_set_local_id);

static void fc_lport_recv_flogi_req(struct fc_lport *lport,
				    struct fc_frame *rx_fp)
{
	struct fc_frame *fp;
	struct fc_frame_header *fh;
	struct fc_els_flogi *flp;
	struct fc_els_flogi *new_flp;
	u64 remote_wwpn;
	u32 remote_fid;
	u32 local_fid;

	FC_LPORT_DBG(lport, "Received FLOGI request while in state %s\n",
		     fc_lport_state(lport));

	remote_fid = fc_frame_sid(rx_fp);
	flp = fc_frame_payload_get(rx_fp, sizeof(*flp));
	if (!flp)
		goto out;
	remote_wwpn = get_unaligned_be64(&flp->fl_wwpn);
	if (remote_wwpn == lport->wwpn) {
		printk(KERN_WARNING "host%d: libfc: Received FLOGI from port "
		       "with same WWPN %16.16llx\n",
		       lport->host->host_no, remote_wwpn);
		goto out;
	}
	FC_LPORT_DBG(lport, "FLOGI from port WWPN %16.16llx\n", remote_wwpn);

	local_fid = FC_LOCAL_PTP_FID_LO;
	if (remote_wwpn < lport->wwpn) {
		local_fid = FC_LOCAL_PTP_FID_HI;
		if (!remote_fid || remote_fid == local_fid)
			remote_fid = FC_LOCAL_PTP_FID_LO;
	} else if (!remote_fid) {
		remote_fid = FC_LOCAL_PTP_FID_HI;
	}

	fc_lport_set_port_id(lport, local_fid, rx_fp);

	fp = fc_frame_alloc(lport, sizeof(*flp));
	if (fp) {
		new_flp = fc_frame_payload_get(fp, sizeof(*flp));
		fc_lport_flogi_fill(lport, new_flp, ELS_FLOGI);
		new_flp->fl_cmd = (u8) ELS_LS_ACC;

		fc_fill_reply_hdr(fp, rx_fp, FC_RCTL_ELS_REP, 0);
		fh = fc_frame_header_get(fp);
		hton24(fh->fh_s_id, local_fid);
		hton24(fh->fh_d_id, remote_fid);
		lport->tt.frame_send(lport, fp);

	} else {
		fc_lport_error(lport, fp);
	}
	fc_lport_ptp_setup(lport, remote_fid, remote_wwpn,
			   get_unaligned_be64(&flp->fl_wwnn));
out:
	fc_frame_free(rx_fp);
}

static void fc_lport_recv_els_req(struct fc_lport *lport,
				  struct fc_frame *fp)
{
	void (*recv)(struct fc_lport *, struct fc_frame *);

	mutex_lock(&lport->lp_mutex);

	if (!lport->link_up)
		fc_frame_free(fp);
	else {
		recv = lport->tt.rport_recv_req;
		switch (fc_frame_payload_op(fp)) {
		case ELS_FLOGI:
			if (!lport->point_to_multipoint)
				recv = fc_lport_recv_flogi_req;
			break;
		case ELS_LOGO:
			if (fc_frame_sid(fp) == FC_FID_FLOGI)
				recv = fc_lport_recv_logo_req;
			break;
		case ELS_RSCN:
			recv = lport->tt.disc_recv_req;
			break;
		case ELS_ECHO:
			recv = fc_lport_recv_echo_req;
			break;
		case ELS_RLIR:
			recv = fc_lport_recv_rlir_req;
			break;
		case ELS_RNID:
			recv = fc_lport_recv_rnid_req;
			break;
		}

		recv(lport, fp);
	}
	mutex_unlock(&lport->lp_mutex);
}

static int fc_lport_els_prli(struct fc_rport_priv *rdata, u32 spp_len,
			     const struct fc_els_spp *spp_in,
			     struct fc_els_spp *spp_out)
{
	return FC_SPP_RESP_INVL;
}

struct fc4_prov fc_lport_els_prov = {
	.prli = fc_lport_els_prli,
	.recv = fc_lport_recv_els_req,
};

static void fc_lport_recv_req(struct fc_lport *lport,
			      struct fc_frame *fp)
{
	struct fc_frame_header *fh = fc_frame_header_get(fp);
	struct fc_seq *sp = fr_seq(fp);
	struct fc4_prov *prov;


	rcu_read_lock();
	if (fh->fh_type >= FC_FC4_PROV_SIZE)
		goto drop;
	prov = rcu_dereference(fc_passive_prov[fh->fh_type]);
	if (!prov || !try_module_get(prov->module))
		goto drop;
	rcu_read_unlock();
	prov->recv(lport, fp);
	module_put(prov->module);
	return;
drop:
	rcu_read_unlock();
	FC_LPORT_DBG(lport, "dropping unexpected frame type %x\n", fh->fh_type);
	fc_frame_free(fp);
	lport->tt.exch_done(sp);
}

int fc_lport_reset(struct fc_lport *lport)
{
	cancel_delayed_work_sync(&lport->retry_work);
	mutex_lock(&lport->lp_mutex);
	fc_lport_enter_reset(lport);
	mutex_unlock(&lport->lp_mutex);
	return 0;
}
EXPORT_SYMBOL(fc_lport_reset);

static void fc_lport_reset_locked(struct fc_lport *lport)
{
	if (lport->dns_rdata)
		lport->tt.rport_logoff(lport->dns_rdata);

	if (lport->ptp_rdata) {
		lport->tt.rport_logoff(lport->ptp_rdata);
		kref_put(&lport->ptp_rdata->kref, lport->tt.rport_destroy);
		lport->ptp_rdata = NULL;
	}

	lport->tt.disc_stop(lport);

	lport->tt.exch_mgr_reset(lport, 0, 0);
	fc_host_fabric_name(lport->host) = 0;

	if (lport->port_id && (!lport->point_to_multipoint || !lport->link_up))
		fc_lport_set_port_id(lport, 0, NULL);
}

static void fc_lport_enter_reset(struct fc_lport *lport)
{
	FC_LPORT_DBG(lport, "Entered RESET state from %s state\n",
		     fc_lport_state(lport));

	if (lport->state == LPORT_ST_DISABLED || lport->state == LPORT_ST_LOGO)
		return;

	if (lport->vport) {
		if (lport->link_up)
			fc_vport_set_state(lport->vport, FC_VPORT_INITIALIZING);
		else
			fc_vport_set_state(lport->vport, FC_VPORT_LINKDOWN);
	}
	fc_lport_state_enter(lport, LPORT_ST_RESET);
	fc_host_post_event(lport->host, fc_get_event_number(),
			   FCH_EVT_LIPRESET, 0);
	fc_vports_linkchange(lport);
	fc_lport_reset_locked(lport);
	if (lport->link_up)
		fc_lport_enter_flogi(lport);
}

static void fc_lport_enter_disabled(struct fc_lport *lport)
{
	FC_LPORT_DBG(lport, "Entered disabled state from %s state\n",
		     fc_lport_state(lport));

	fc_lport_state_enter(lport, LPORT_ST_DISABLED);
	fc_vports_linkchange(lport);
	fc_lport_reset_locked(lport);
}

static void fc_lport_error(struct fc_lport *lport, struct fc_frame *fp)
{
	unsigned long delay = 0;
	FC_LPORT_DBG(lport, "Error %ld in state %s, retries %d\n",
		     PTR_ERR(fp), fc_lport_state(lport),
		     lport->retry_count);

	if (PTR_ERR(fp) == -FC_EX_CLOSED)
		return;

	if (lport->retry_count < lport->max_retry_count) {
		lport->retry_count++;
		if (!fp)
			delay = msecs_to_jiffies(500);
		else
			delay =	msecs_to_jiffies(lport->e_d_tov);

		schedule_delayed_work(&lport->retry_work, delay);
	} else
		fc_lport_enter_reset(lport);
}

static void fc_lport_ns_resp(struct fc_seq *sp, struct fc_frame *fp,
			     void *lp_arg)
{
	struct fc_lport *lport = lp_arg;
	struct fc_frame_header *fh;
	struct fc_ct_hdr *ct;

	FC_LPORT_DBG(lport, "Received a ns %s\n", fc_els_resp_type(fp));

	if (fp == ERR_PTR(-FC_EX_CLOSED))
		return;

	mutex_lock(&lport->lp_mutex);

	if (lport->state < LPORT_ST_RNN_ID || lport->state > LPORT_ST_RFF_ID) {
		FC_LPORT_DBG(lport, "Received a name server response, "
			     "but in state %s\n", fc_lport_state(lport));
		if (IS_ERR(fp))
			goto err;
		goto out;
	}

	if (IS_ERR(fp)) {
		fc_lport_error(lport, fp);
		goto err;
	}

	fh = fc_frame_header_get(fp);
	ct = fc_frame_payload_get(fp, sizeof(*ct));

	if (fh && ct && fh->fh_type == FC_TYPE_CT &&
	    ct->ct_fs_type == FC_FST_DIR &&
	    ct->ct_fs_subtype == FC_NS_SUBTYPE &&
	    ntohs(ct->ct_cmd) == FC_FS_ACC)
		switch (lport->state) {
		case LPORT_ST_RNN_ID:
			fc_lport_enter_ns(lport, LPORT_ST_RSNN_NN);
			break;
		case LPORT_ST_RSNN_NN:
			fc_lport_enter_ns(lport, LPORT_ST_RSPN_ID);
			break;
		case LPORT_ST_RSPN_ID:
			fc_lport_enter_ns(lport, LPORT_ST_RFT_ID);
			break;
		case LPORT_ST_RFT_ID:
			fc_lport_enter_ns(lport, LPORT_ST_RFF_ID);
			break;
		case LPORT_ST_RFF_ID:
			if (lport->fdmi_enabled)
				fc_lport_enter_fdmi(lport);
			else
				fc_lport_enter_scr(lport);
			break;
		default:
			
			break;
		}
	else
		fc_lport_error(lport, fp);
out:
	fc_frame_free(fp);
err:
	mutex_unlock(&lport->lp_mutex);
}

static void fc_lport_ms_resp(struct fc_seq *sp, struct fc_frame *fp,
			     void *lp_arg)
{
	struct fc_lport *lport = lp_arg;
	struct fc_frame_header *fh;
	struct fc_ct_hdr *ct;

	FC_LPORT_DBG(lport, "Received a ms %s\n", fc_els_resp_type(fp));

	if (fp == ERR_PTR(-FC_EX_CLOSED))
		return;

	mutex_lock(&lport->lp_mutex);

	if (lport->state < LPORT_ST_RHBA || lport->state > LPORT_ST_DPRT) {
		FC_LPORT_DBG(lport, "Received a management server response, "
			     "but in state %s\n", fc_lport_state(lport));
		if (IS_ERR(fp))
			goto err;
		goto out;
	}

	if (IS_ERR(fp)) {
		fc_lport_error(lport, fp);
		goto err;
	}

	fh = fc_frame_header_get(fp);
	ct = fc_frame_payload_get(fp, sizeof(*ct));

	if (fh && ct && fh->fh_type == FC_TYPE_CT &&
	    ct->ct_fs_type == FC_FST_MGMT &&
	    ct->ct_fs_subtype == FC_FDMI_SUBTYPE) {
		FC_LPORT_DBG(lport, "Received a management server response, "
				    "reason=%d explain=%d\n",
				    ct->ct_reason,
				    ct->ct_explan);

		switch (lport->state) {
		case LPORT_ST_RHBA:
			if (ntohs(ct->ct_cmd) == FC_FS_ACC)
				fc_lport_enter_ms(lport, LPORT_ST_RPA);
			else 
				fc_lport_enter_scr(lport);
			break;
		case LPORT_ST_RPA:
			fc_lport_enter_scr(lport);
			break;
		case LPORT_ST_DPRT:
			fc_lport_enter_ms(lport, LPORT_ST_RHBA);
			break;
		case LPORT_ST_DHBA:
			fc_lport_enter_ms(lport, LPORT_ST_DPRT);
			break;
		default:
			
			break;
		}
	} else {
		
		fc_lport_error(lport, fp);
	}
out:
	fc_frame_free(fp);
err:
	mutex_unlock(&lport->lp_mutex);
}

static void fc_lport_scr_resp(struct fc_seq *sp, struct fc_frame *fp,
			      void *lp_arg)
{
	struct fc_lport *lport = lp_arg;
	u8 op;

	FC_LPORT_DBG(lport, "Received a SCR %s\n", fc_els_resp_type(fp));

	if (fp == ERR_PTR(-FC_EX_CLOSED))
		return;

	mutex_lock(&lport->lp_mutex);

	if (lport->state != LPORT_ST_SCR) {
		FC_LPORT_DBG(lport, "Received a SCR response, but in state "
			     "%s\n", fc_lport_state(lport));
		if (IS_ERR(fp))
			goto err;
		goto out;
	}

	if (IS_ERR(fp)) {
		fc_lport_error(lport, fp);
		goto err;
	}

	op = fc_frame_payload_op(fp);
	if (op == ELS_LS_ACC)
		fc_lport_enter_ready(lport);
	else
		fc_lport_error(lport, fp);

out:
	fc_frame_free(fp);
err:
	mutex_unlock(&lport->lp_mutex);
}

static void fc_lport_enter_scr(struct fc_lport *lport)
{
	struct fc_frame *fp;

	FC_LPORT_DBG(lport, "Entered SCR state from %s state\n",
		     fc_lport_state(lport));

	fc_lport_state_enter(lport, LPORT_ST_SCR);

	fp = fc_frame_alloc(lport, sizeof(struct fc_els_scr));
	if (!fp) {
		fc_lport_error(lport, fp);
		return;
	}

	if (!lport->tt.elsct_send(lport, FC_FID_FCTRL, fp, ELS_SCR,
				  fc_lport_scr_resp, lport,
				  2 * lport->r_a_tov))
		fc_lport_error(lport, NULL);
}

static void fc_lport_enter_ns(struct fc_lport *lport, enum fc_lport_state state)
{
	struct fc_frame *fp;
	enum fc_ns_req cmd;
	int size = sizeof(struct fc_ct_hdr);
	size_t len;

	FC_LPORT_DBG(lport, "Entered %s state from %s state\n",
		     fc_lport_state_names[state],
		     fc_lport_state(lport));

	fc_lport_state_enter(lport, state);

	switch (state) {
	case LPORT_ST_RNN_ID:
		cmd = FC_NS_RNN_ID;
		size += sizeof(struct fc_ns_rn_id);
		break;
	case LPORT_ST_RSNN_NN:
		len = strnlen(fc_host_symbolic_name(lport->host), 255);
		
		if (!len)
			return fc_lport_enter_ns(lport, LPORT_ST_RFT_ID);
		cmd = FC_NS_RSNN_NN;
		size += sizeof(struct fc_ns_rsnn) + len;
		break;
	case LPORT_ST_RSPN_ID:
		len = strnlen(fc_host_symbolic_name(lport->host), 255);
		
		if (!len)
			return fc_lport_enter_ns(lport, LPORT_ST_RFT_ID);
		cmd = FC_NS_RSPN_ID;
		size += sizeof(struct fc_ns_rspn) + len;
		break;
	case LPORT_ST_RFT_ID:
		cmd = FC_NS_RFT_ID;
		size += sizeof(struct fc_ns_rft);
		break;
	case LPORT_ST_RFF_ID:
		cmd = FC_NS_RFF_ID;
		size += sizeof(struct fc_ns_rff_id);
		break;
	default:
		fc_lport_error(lport, NULL);
		return;
	}

	fp = fc_frame_alloc(lport, size);
	if (!fp) {
		fc_lport_error(lport, fp);
		return;
	}

	if (!lport->tt.elsct_send(lport, FC_FID_DIR_SERV, fp, cmd,
				  fc_lport_ns_resp,
				  lport, 3 * lport->r_a_tov))
		fc_lport_error(lport, fp);
}

static struct fc_rport_operations fc_lport_rport_ops = {
	.event_callback = fc_lport_rport_callback,
};

static void fc_lport_enter_dns(struct fc_lport *lport)
{
	struct fc_rport_priv *rdata;

	FC_LPORT_DBG(lport, "Entered DNS state from %s state\n",
		     fc_lport_state(lport));

	fc_lport_state_enter(lport, LPORT_ST_DNS);

	mutex_lock(&lport->disc.disc_mutex);
	rdata = lport->tt.rport_create(lport, FC_FID_DIR_SERV);
	mutex_unlock(&lport->disc.disc_mutex);
	if (!rdata)
		goto err;

	rdata->ops = &fc_lport_rport_ops;
	lport->tt.rport_login(rdata);
	return;

err:
	fc_lport_error(lport, NULL);
}

static void fc_lport_enter_ms(struct fc_lport *lport, enum fc_lport_state state)
{
	struct fc_frame *fp;
	enum fc_fdmi_req cmd;
	int size = sizeof(struct fc_ct_hdr);
	size_t len;
	int numattrs;

	FC_LPORT_DBG(lport, "Entered %s state from %s state\n",
		     fc_lport_state_names[state],
		     fc_lport_state(lport));

	fc_lport_state_enter(lport, state);

	switch (state) {
	case LPORT_ST_RHBA:
		cmd = FC_FDMI_RHBA;
		
		numattrs = 10;
		len = sizeof(struct fc_fdmi_rhba);
		len -= sizeof(struct fc_fdmi_attr_entry);
		len += (numattrs * FC_FDMI_ATTR_ENTRY_HEADER_LEN);
		len += FC_FDMI_HBA_ATTR_NODENAME_LEN;
		len += FC_FDMI_HBA_ATTR_MANUFACTURER_LEN;
		len += FC_FDMI_HBA_ATTR_SERIALNUMBER_LEN;
		len += FC_FDMI_HBA_ATTR_MODEL_LEN;
		len += FC_FDMI_HBA_ATTR_MODELDESCR_LEN;
		len += FC_FDMI_HBA_ATTR_HARDWAREVERSION_LEN;
		len += FC_FDMI_HBA_ATTR_DRIVERVERSION_LEN;
		len += FC_FDMI_HBA_ATTR_OPTIONROMVERSION_LEN;
		len += FC_FDMI_HBA_ATTR_FIRMWAREVERSION_LEN;
		len += FC_FDMI_HBA_ATTR_OSNAMEVERSION_LEN;

		size += len;
		break;
	case LPORT_ST_RPA:
		cmd = FC_FDMI_RPA;
		
		numattrs = 6;
		len = sizeof(struct fc_fdmi_rpa);
		len -= sizeof(struct fc_fdmi_attr_entry);
		len += (numattrs * FC_FDMI_ATTR_ENTRY_HEADER_LEN);
		len += FC_FDMI_PORT_ATTR_FC4TYPES_LEN;
		len += FC_FDMI_PORT_ATTR_SUPPORTEDSPEED_LEN;
		len += FC_FDMI_PORT_ATTR_CURRENTPORTSPEED_LEN;
		len += FC_FDMI_PORT_ATTR_MAXFRAMESIZE_LEN;
		len += FC_FDMI_PORT_ATTR_OSDEVICENAME_LEN;
		len += FC_FDMI_PORT_ATTR_HOSTNAME_LEN;

		size += len;
		break;
	case LPORT_ST_DPRT:
		cmd = FC_FDMI_DPRT;
		len = sizeof(struct fc_fdmi_dprt);
		size += len;
		break;
	case LPORT_ST_DHBA:
		cmd = FC_FDMI_DHBA;
		len = sizeof(struct fc_fdmi_dhba);
		size += len;
		break;
	default:
		fc_lport_error(lport, NULL);
		return;
	}

	FC_LPORT_DBG(lport, "Cmd=0x%x Len %d size %d\n",
			     cmd, (int)len, size);
	fp = fc_frame_alloc(lport, size);
	if (!fp) {
		fc_lport_error(lport, fp);
		return;
	}

	if (!lport->tt.elsct_send(lport, FC_FID_MGMT_SERV, fp, cmd,
				  fc_lport_ms_resp,
				  lport, 3 * lport->r_a_tov))
		fc_lport_error(lport, fp);
}

static void fc_lport_enter_fdmi(struct fc_lport *lport)
{
	struct fc_rport_priv *rdata;

	FC_LPORT_DBG(lport, "Entered FDMI state from %s state\n",
		     fc_lport_state(lport));

	fc_lport_state_enter(lport, LPORT_ST_FDMI);

	mutex_lock(&lport->disc.disc_mutex);
	rdata = lport->tt.rport_create(lport, FC_FID_MGMT_SERV);
	mutex_unlock(&lport->disc.disc_mutex);
	if (!rdata)
		goto err;

	rdata->ops = &fc_lport_rport_ops;
	lport->tt.rport_login(rdata);
	return;

err:
	fc_lport_error(lport, NULL);
}

static void fc_lport_timeout(struct work_struct *work)
{
	struct fc_lport *lport =
		container_of(work, struct fc_lport,
			     retry_work.work);

	mutex_lock(&lport->lp_mutex);

	switch (lport->state) {
	case LPORT_ST_DISABLED:
		WARN_ON(1);
		break;
	case LPORT_ST_READY:
		break;
	case LPORT_ST_RESET:
		break;
	case LPORT_ST_FLOGI:
		fc_lport_enter_flogi(lport);
		break;
	case LPORT_ST_DNS:
		fc_lport_enter_dns(lport);
		break;
	case LPORT_ST_RNN_ID:
	case LPORT_ST_RSNN_NN:
	case LPORT_ST_RSPN_ID:
	case LPORT_ST_RFT_ID:
	case LPORT_ST_RFF_ID:
		fc_lport_enter_ns(lport, lport->state);
		break;
	case LPORT_ST_FDMI:
		fc_lport_enter_fdmi(lport);
		break;
	case LPORT_ST_RHBA:
	case LPORT_ST_RPA:
	case LPORT_ST_DHBA:
	case LPORT_ST_DPRT:
		fc_lport_enter_ms(lport, lport->state);
		break;
	case LPORT_ST_SCR:
		fc_lport_enter_scr(lport);
		break;
	case LPORT_ST_LOGO:
		fc_lport_enter_logo(lport);
		break;
	}

	mutex_unlock(&lport->lp_mutex);
}

void fc_lport_logo_resp(struct fc_seq *sp, struct fc_frame *fp,
			void *lp_arg)
{
	struct fc_lport *lport = lp_arg;
	u8 op;

	FC_LPORT_DBG(lport, "Received a LOGO %s\n", fc_els_resp_type(fp));

	if (fp == ERR_PTR(-FC_EX_CLOSED))
		return;

	mutex_lock(&lport->lp_mutex);

	if (lport->state != LPORT_ST_LOGO) {
		FC_LPORT_DBG(lport, "Received a LOGO response, but in state "
			     "%s\n", fc_lport_state(lport));
		if (IS_ERR(fp))
			goto err;
		goto out;
	}

	if (IS_ERR(fp)) {
		fc_lport_error(lport, fp);
		goto err;
	}

	op = fc_frame_payload_op(fp);
	if (op == ELS_LS_ACC)
		fc_lport_enter_disabled(lport);
	else
		fc_lport_error(lport, fp);

out:
	fc_frame_free(fp);
err:
	mutex_unlock(&lport->lp_mutex);
}
EXPORT_SYMBOL(fc_lport_logo_resp);

static void fc_lport_enter_logo(struct fc_lport *lport)
{
	struct fc_frame *fp;
	struct fc_els_logo *logo;

	FC_LPORT_DBG(lport, "Entered LOGO state from %s state\n",
		     fc_lport_state(lport));

	fc_lport_state_enter(lport, LPORT_ST_LOGO);
	fc_vports_linkchange(lport);

	fp = fc_frame_alloc(lport, sizeof(*logo));
	if (!fp) {
		fc_lport_error(lport, fp);
		return;
	}

	if (!lport->tt.elsct_send(lport, FC_FID_FLOGI, fp, ELS_LOGO,
				  fc_lport_logo_resp, lport,
				  2 * lport->r_a_tov))
		fc_lport_error(lport, NULL);
}

void fc_lport_flogi_resp(struct fc_seq *sp, struct fc_frame *fp,
			 void *lp_arg)
{
	struct fc_lport *lport = lp_arg;
	struct fc_frame_header *fh;
	struct fc_els_flogi *flp;
	u32 did;
	u16 csp_flags;
	unsigned int r_a_tov;
	unsigned int e_d_tov;
	u16 mfs;

	FC_LPORT_DBG(lport, "Received a FLOGI %s\n", fc_els_resp_type(fp));

	if (fp == ERR_PTR(-FC_EX_CLOSED))
		return;

	mutex_lock(&lport->lp_mutex);

	if (lport->state != LPORT_ST_FLOGI) {
		FC_LPORT_DBG(lport, "Received a FLOGI response, but in state "
			     "%s\n", fc_lport_state(lport));
		if (IS_ERR(fp))
			goto err;
		goto out;
	}

	if (IS_ERR(fp)) {
		fc_lport_error(lport, fp);
		goto err;
	}

	fh = fc_frame_header_get(fp);
	did = fc_frame_did(fp);
	if (fh->fh_r_ctl != FC_RCTL_ELS_REP || did == 0 ||
	    fc_frame_payload_op(fp) != ELS_LS_ACC) {
		FC_LPORT_DBG(lport, "FLOGI not accepted or bad response\n");
		fc_lport_error(lport, fp);
		goto err;
	}

	flp = fc_frame_payload_get(fp, sizeof(*flp));
	if (!flp) {
		FC_LPORT_DBG(lport, "FLOGI bad response\n");
		fc_lport_error(lport, fp);
		goto err;
	}

	mfs = ntohs(flp->fl_csp.sp_bb_data) &
		FC_SP_BB_DATA_MASK;

	if (mfs < FC_SP_MIN_MAX_PAYLOAD || mfs > FC_SP_MAX_MAX_PAYLOAD) {
		FC_LPORT_DBG(lport, "FLOGI bad mfs:%hu response, "
			     "lport->mfs:%hu\n", mfs, lport->mfs);
		fc_lport_error(lport, fp);
		goto err;
	}

	if (mfs <= lport->mfs) {
		lport->mfs = mfs;
		fc_host_maxframe_size(lport->host) = mfs;
	}

	csp_flags = ntohs(flp->fl_csp.sp_features);
	r_a_tov = ntohl(flp->fl_csp.sp_r_a_tov);
	e_d_tov = ntohl(flp->fl_csp.sp_e_d_tov);
	if (csp_flags & FC_SP_FT_EDTR)
		e_d_tov /= 1000000;

	lport->npiv_enabled = !!(csp_flags & FC_SP_FT_NPIV_ACC);

	if ((csp_flags & FC_SP_FT_FPORT) == 0) {
		if (e_d_tov > lport->e_d_tov)
			lport->e_d_tov = e_d_tov;
		lport->r_a_tov = 2 * e_d_tov;
		fc_lport_set_port_id(lport, did, fp);
		printk(KERN_INFO "host%d: libfc: "
		       "Port (%6.6x) entered "
		       "point-to-point mode\n",
		       lport->host->host_no, did);
		fc_lport_ptp_setup(lport, fc_frame_sid(fp),
				   get_unaligned_be64(
					   &flp->fl_wwpn),
				   get_unaligned_be64(
					   &flp->fl_wwnn));
	} else {
		lport->e_d_tov = e_d_tov;
		lport->r_a_tov = r_a_tov;
		fc_host_fabric_name(lport->host) =
			get_unaligned_be64(&flp->fl_wwnn);
		fc_lport_set_port_id(lport, did, fp);
		fc_lport_enter_dns(lport);
	}

out:
	fc_frame_free(fp);
err:
	mutex_unlock(&lport->lp_mutex);
}
EXPORT_SYMBOL(fc_lport_flogi_resp);

static void fc_lport_enter_flogi(struct fc_lport *lport)
{
	struct fc_frame *fp;

	FC_LPORT_DBG(lport, "Entered FLOGI state from %s state\n",
		     fc_lport_state(lport));

	fc_lport_state_enter(lport, LPORT_ST_FLOGI);

	if (lport->point_to_multipoint) {
		if (lport->port_id)
			fc_lport_enter_ready(lport);
		return;
	}

	fp = fc_frame_alloc(lport, sizeof(struct fc_els_flogi));
	if (!fp)
		return fc_lport_error(lport, fp);

	if (!lport->tt.elsct_send(lport, FC_FID_FLOGI, fp,
				  lport->vport ? ELS_FDISC : ELS_FLOGI,
				  fc_lport_flogi_resp, lport,
				  lport->vport ? 2 * lport->r_a_tov :
				  lport->e_d_tov))
		fc_lport_error(lport, NULL);
}

int fc_lport_config(struct fc_lport *lport)
{
	INIT_DELAYED_WORK(&lport->retry_work, fc_lport_timeout);
	mutex_init(&lport->lp_mutex);

	fc_lport_state_enter(lport, LPORT_ST_DISABLED);

	fc_lport_add_fc4_type(lport, FC_TYPE_FCP);
	fc_lport_add_fc4_type(lport, FC_TYPE_CT);
	fc_fc4_conf_lport_params(lport, FC_TYPE_FCP);

	return 0;
}
EXPORT_SYMBOL(fc_lport_config);

int fc_lport_init(struct fc_lport *lport)
{
	if (!lport->tt.lport_recv)
		lport->tt.lport_recv = fc_lport_recv_req;

	if (!lport->tt.lport_reset)
		lport->tt.lport_reset = fc_lport_reset;

	fc_host_port_type(lport->host) = FC_PORTTYPE_NPORT;
	fc_host_node_name(lport->host) = lport->wwnn;
	fc_host_port_name(lport->host) = lport->wwpn;
	fc_host_supported_classes(lport->host) = FC_COS_CLASS3;
	memset(fc_host_supported_fc4s(lport->host), 0,
	       sizeof(fc_host_supported_fc4s(lport->host)));
	fc_host_supported_fc4s(lport->host)[2] = 1;
	fc_host_supported_fc4s(lport->host)[7] = 1;

	
	memset(fc_host_active_fc4s(lport->host), 0,
	       sizeof(fc_host_active_fc4s(lport->host)));
	fc_host_active_fc4s(lport->host)[2] = 1;
	fc_host_active_fc4s(lport->host)[7] = 1;
	fc_host_maxframe_size(lport->host) = lport->mfs;
	fc_host_supported_speeds(lport->host) = 0;
	if (lport->link_supported_speeds & FC_PORTSPEED_1GBIT)
		fc_host_supported_speeds(lport->host) |= FC_PORTSPEED_1GBIT;
	if (lport->link_supported_speeds & FC_PORTSPEED_10GBIT)
		fc_host_supported_speeds(lport->host) |= FC_PORTSPEED_10GBIT;
	fc_fc4_add_lport(lport);

	return 0;
}
EXPORT_SYMBOL(fc_lport_init);

static void fc_lport_bsg_resp(struct fc_seq *sp, struct fc_frame *fp,
			      void *info_arg)
{
	struct fc_bsg_info *info = info_arg;
	struct fc_bsg_job *job = info->job;
	struct fc_lport *lport = info->lport;
	struct fc_frame_header *fh;
	size_t len;
	void *buf;

	if (IS_ERR(fp)) {
		job->reply->result = (PTR_ERR(fp) == -FC_EX_CLOSED) ?
			-ECONNABORTED : -ETIMEDOUT;
		job->reply_len = sizeof(uint32_t);
		job->state_flags |= FC_RQST_STATE_DONE;
		job->job_done(job);
		kfree(info);
		return;
	}

	mutex_lock(&lport->lp_mutex);
	fh = fc_frame_header_get(fp);
	len = fr_len(fp) - sizeof(*fh);
	buf = fc_frame_payload_get(fp, 0);

	if (fr_sof(fp) == FC_SOF_I3 && !ntohs(fh->fh_seq_cnt)) {
		
		unsigned short cmd = (info->rsp_code == FC_FS_ACC) ?
			ntohs(((struct fc_ct_hdr *)buf)->ct_cmd) :
			(unsigned short)fc_frame_payload_op(fp);

		
		job->reply->reply_data.ctels_reply.status =
			(cmd == info->rsp_code) ?
			FC_CTELS_STATUS_OK : FC_CTELS_STATUS_REJECT;
	}

	job->reply->reply_payload_rcv_len +=
		fc_copy_buffer_to_sglist(buf, len, info->sg, &info->nents,
					 &info->offset, NULL);

	if (fr_eof(fp) == FC_EOF_T &&
	    (ntoh24(fh->fh_f_ctl) & (FC_FC_LAST_SEQ | FC_FC_END_SEQ)) ==
	    (FC_FC_LAST_SEQ | FC_FC_END_SEQ)) {
		if (job->reply->reply_payload_rcv_len >
		    job->reply_payload.payload_len)
			job->reply->reply_payload_rcv_len =
				job->reply_payload.payload_len;
		job->reply->result = 0;
		job->state_flags |= FC_RQST_STATE_DONE;
		job->job_done(job);
		kfree(info);
	}
	fc_frame_free(fp);
	mutex_unlock(&lport->lp_mutex);
}

static int fc_lport_els_request(struct fc_bsg_job *job,
				struct fc_lport *lport,
				u32 did, u32 tov)
{
	struct fc_bsg_info *info;
	struct fc_frame *fp;
	struct fc_frame_header *fh;
	char *pp;
	int len;

	fp = fc_frame_alloc(lport, job->request_payload.payload_len);
	if (!fp)
		return -ENOMEM;

	len = job->request_payload.payload_len;
	pp = fc_frame_payload_get(fp, len);

	sg_copy_to_buffer(job->request_payload.sg_list,
			  job->request_payload.sg_cnt,
			  pp, len);

	fh = fc_frame_header_get(fp);
	fh->fh_r_ctl = FC_RCTL_ELS_REQ;
	hton24(fh->fh_d_id, did);
	hton24(fh->fh_s_id, lport->port_id);
	fh->fh_type = FC_TYPE_ELS;
	hton24(fh->fh_f_ctl, FC_FCTL_REQ);
	fh->fh_cs_ctl = 0;
	fh->fh_df_ctl = 0;
	fh->fh_parm_offset = 0;

	info = kzalloc(sizeof(struct fc_bsg_info), GFP_KERNEL);
	if (!info) {
		fc_frame_free(fp);
		return -ENOMEM;
	}

	info->job = job;
	info->lport = lport;
	info->rsp_code = ELS_LS_ACC;
	info->nents = job->reply_payload.sg_cnt;
	info->sg = job->reply_payload.sg_list;

	if (!lport->tt.exch_seq_send(lport, fp, fc_lport_bsg_resp,
				     NULL, info, tov)) {
		kfree(info);
		return -ECOMM;
	}
	return 0;
}

static int fc_lport_ct_request(struct fc_bsg_job *job,
			       struct fc_lport *lport, u32 did, u32 tov)
{
	struct fc_bsg_info *info;
	struct fc_frame *fp;
	struct fc_frame_header *fh;
	struct fc_ct_req *ct;
	size_t len;

	fp = fc_frame_alloc(lport, sizeof(struct fc_ct_hdr) +
			    job->request_payload.payload_len);
	if (!fp)
		return -ENOMEM;

	len = job->request_payload.payload_len;
	ct = fc_frame_payload_get(fp, len);

	sg_copy_to_buffer(job->request_payload.sg_list,
			  job->request_payload.sg_cnt,
			  ct, len);

	fh = fc_frame_header_get(fp);
	fh->fh_r_ctl = FC_RCTL_DD_UNSOL_CTL;
	hton24(fh->fh_d_id, did);
	hton24(fh->fh_s_id, lport->port_id);
	fh->fh_type = FC_TYPE_CT;
	hton24(fh->fh_f_ctl, FC_FCTL_REQ);
	fh->fh_cs_ctl = 0;
	fh->fh_df_ctl = 0;
	fh->fh_parm_offset = 0;

	info = kzalloc(sizeof(struct fc_bsg_info), GFP_KERNEL);
	if (!info) {
		fc_frame_free(fp);
		return -ENOMEM;
	}

	info->job = job;
	info->lport = lport;
	info->rsp_code = FC_FS_ACC;
	info->nents = job->reply_payload.sg_cnt;
	info->sg = job->reply_payload.sg_list;

	if (!lport->tt.exch_seq_send(lport, fp, fc_lport_bsg_resp,
				     NULL, info, tov)) {
		kfree(info);
		return -ECOMM;
	}
	return 0;
}

int fc_lport_bsg_request(struct fc_bsg_job *job)
{
	struct request *rsp = job->req->next_rq;
	struct Scsi_Host *shost = job->shost;
	struct fc_lport *lport = shost_priv(shost);
	struct fc_rport *rport;
	struct fc_rport_priv *rdata;
	int rc = -EINVAL;
	u32 did;

	job->reply->reply_payload_rcv_len = 0;
	if (rsp)
		rsp->resid_len = job->reply_payload.payload_len;

	mutex_lock(&lport->lp_mutex);

	switch (job->request->msgcode) {
	case FC_BSG_RPT_ELS:
		rport = job->rport;
		if (!rport)
			break;

		rdata = rport->dd_data;
		rc = fc_lport_els_request(job, lport, rport->port_id,
					  rdata->e_d_tov);
		break;

	case FC_BSG_RPT_CT:
		rport = job->rport;
		if (!rport)
			break;

		rdata = rport->dd_data;
		rc = fc_lport_ct_request(job, lport, rport->port_id,
					 rdata->e_d_tov);
		break;

	case FC_BSG_HST_CT:
		did = ntoh24(job->request->rqst_data.h_ct.port_id);
		if (did == FC_FID_DIR_SERV)
			rdata = lport->dns_rdata;
		else
			rdata = lport->tt.rport_lookup(lport, did);

		if (!rdata)
			break;

		rc = fc_lport_ct_request(job, lport, did, rdata->e_d_tov);
		break;

	case FC_BSG_HST_ELS_NOLOGIN:
		did = ntoh24(job->request->rqst_data.h_els.port_id);
		rc = fc_lport_els_request(job, lport, did, lport->e_d_tov);
		break;
	}

	mutex_unlock(&lport->lp_mutex);
	return rc;
}
EXPORT_SYMBOL(fc_lport_bsg_request);
