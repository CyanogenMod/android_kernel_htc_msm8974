/*
 * Copyright(c) 2007 - 2008 Intel Corporation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/export.h>
#include <asm/unaligned.h>

#include <scsi/fc/fc_gs.h>

#include <scsi/libfc.h>

#include "fc_libfc.h"

#define FC_DISC_RETRY_LIMIT	3	
#define FC_DISC_RETRY_DELAY	500UL	

static void fc_disc_gpn_ft_req(struct fc_disc *);
static void fc_disc_gpn_ft_resp(struct fc_seq *, struct fc_frame *, void *);
static void fc_disc_done(struct fc_disc *, enum fc_disc_event);
static void fc_disc_timeout(struct work_struct *);
static int fc_disc_single(struct fc_lport *, struct fc_disc_port *);
static void fc_disc_restart(struct fc_disc *);

static void fc_disc_stop_rports(struct fc_disc *disc)
{
	struct fc_lport *lport;
	struct fc_rport_priv *rdata;

	lport = fc_disc_lport(disc);

	mutex_lock(&disc->disc_mutex);
	list_for_each_entry_rcu(rdata, &disc->rports, peers)
		lport->tt.rport_logoff(rdata);
	mutex_unlock(&disc->disc_mutex);
}

static void fc_disc_recv_rscn_req(struct fc_disc *disc, struct fc_frame *fp)
{
	struct fc_lport *lport;
	struct fc_els_rscn *rp;
	struct fc_els_rscn_page *pp;
	struct fc_seq_els_data rjt_data;
	unsigned int len;
	int redisc = 0;
	enum fc_els_rscn_ev_qual ev_qual;
	enum fc_els_rscn_addr_fmt fmt;
	LIST_HEAD(disc_ports);
	struct fc_disc_port *dp, *next;

	lport = fc_disc_lport(disc);

	FC_DISC_DBG(disc, "Received an RSCN event\n");

	
	rp = fc_frame_payload_get(fp, sizeof(*rp));
	if (!rp)
		goto reject;
	
	if (rp->rscn_page_len != sizeof(*pp))
		goto reject;
	
	len = ntohs(rp->rscn_plen);
	if (len < sizeof(*rp))
		goto reject;
	
	rp = fc_frame_payload_get(fp, len);
	if (!rp)
		goto reject;
	
	len -= sizeof(*rp);
	if (len % sizeof(*pp))
		goto reject;

	for (pp = (void *)(rp + 1); len > 0; len -= sizeof(*pp), pp++) {
		ev_qual = pp->rscn_page_flags >> ELS_RSCN_EV_QUAL_BIT;
		ev_qual &= ELS_RSCN_EV_QUAL_MASK;
		fmt = pp->rscn_page_flags >> ELS_RSCN_ADDR_FMT_BIT;
		fmt &= ELS_RSCN_ADDR_FMT_MASK;
		switch (fmt) {
		case ELS_ADDR_FMT_PORT:
			FC_DISC_DBG(disc, "Port address format for port "
				    "(%6.6x)\n", ntoh24(pp->rscn_fid));
			dp = kzalloc(sizeof(*dp), GFP_KERNEL);
			if (!dp) {
				redisc = 1;
				break;
			}
			dp->lp = lport;
			dp->port_id = ntoh24(pp->rscn_fid);
			list_add_tail(&dp->peers, &disc_ports);
			break;
		case ELS_ADDR_FMT_AREA:
		case ELS_ADDR_FMT_DOM:
		case ELS_ADDR_FMT_FAB:
		default:
			FC_DISC_DBG(disc, "Address format is (%d)\n", fmt);
			redisc = 1;
			break;
		}
	}
	lport->tt.seq_els_rsp_send(fp, ELS_LS_ACC, NULL);

	list_for_each_entry_safe(dp, next, &disc_ports, peers) {
		list_del(&dp->peers);
		if (!redisc)
			redisc = fc_disc_single(lport, dp);
		kfree(dp);
	}
	if (redisc) {
		FC_DISC_DBG(disc, "RSCN received: rediscovering\n");
		fc_disc_restart(disc);
	} else {
		FC_DISC_DBG(disc, "RSCN received: not rediscovering. "
			    "redisc %d state %d in_prog %d\n",
			    redisc, lport->state, disc->pending);
	}
	fc_frame_free(fp);
	return;
reject:
	FC_DISC_DBG(disc, "Received a bad RSCN frame\n");
	rjt_data.reason = ELS_RJT_LOGIC;
	rjt_data.explan = ELS_EXPL_NONE;
	lport->tt.seq_els_rsp_send(fp, ELS_LS_RJT, &rjt_data);
	fc_frame_free(fp);
}

static void fc_disc_recv_req(struct fc_lport *lport, struct fc_frame *fp)
{
	u8 op;
	struct fc_disc *disc = &lport->disc;

	op = fc_frame_payload_op(fp);
	switch (op) {
	case ELS_RSCN:
		mutex_lock(&disc->disc_mutex);
		fc_disc_recv_rscn_req(disc, fp);
		mutex_unlock(&disc->disc_mutex);
		break;
	default:
		FC_DISC_DBG(disc, "Received an unsupported request, "
			    "the opcode is (%x)\n", op);
		fc_frame_free(fp);
		break;
	}
}

static void fc_disc_restart(struct fc_disc *disc)
{
	if (!disc->disc_callback)
		return;

	FC_DISC_DBG(disc, "Restarting discovery\n");

	disc->requested = 1;
	if (disc->pending)
		return;

	disc->disc_id = (disc->disc_id + 2) | 1;
	disc->retry_count = 0;
	fc_disc_gpn_ft_req(disc);
}

static void fc_disc_start(void (*disc_callback)(struct fc_lport *,
						enum fc_disc_event),
			  struct fc_lport *lport)
{
	struct fc_disc *disc = &lport->disc;

	mutex_lock(&disc->disc_mutex);
	disc->disc_callback = disc_callback;
	fc_disc_restart(disc);
	mutex_unlock(&disc->disc_mutex);
}

static void fc_disc_done(struct fc_disc *disc, enum fc_disc_event event)
{
	struct fc_lport *lport = fc_disc_lport(disc);
	struct fc_rport_priv *rdata;

	FC_DISC_DBG(disc, "Discovery complete\n");

	disc->pending = 0;
	if (disc->requested) {
		fc_disc_restart(disc);
		return;
	}

	list_for_each_entry_rcu(rdata, &disc->rports, peers) {
		if (!rdata->disc_id)
			continue;
		if (rdata->disc_id == disc->disc_id)
			lport->tt.rport_login(rdata);
		else
			lport->tt.rport_logoff(rdata);
	}

	mutex_unlock(&disc->disc_mutex);
	disc->disc_callback(lport, event);
	mutex_lock(&disc->disc_mutex);
}

static void fc_disc_error(struct fc_disc *disc, struct fc_frame *fp)
{
	struct fc_lport *lport = fc_disc_lport(disc);
	unsigned long delay = 0;

	FC_DISC_DBG(disc, "Error %ld, retries %d/%d\n",
		    PTR_ERR(fp), disc->retry_count,
		    FC_DISC_RETRY_LIMIT);

	if (!fp || PTR_ERR(fp) == -FC_EX_TIMEOUT) {
		if (disc->retry_count < FC_DISC_RETRY_LIMIT) {
			
			if (!fp)
				delay = msecs_to_jiffies(FC_DISC_RETRY_DELAY);
			else {
				delay = msecs_to_jiffies(lport->e_d_tov);

				
				if (!disc->retry_count)
					delay /= 4;
			}
			disc->retry_count++;
			schedule_delayed_work(&disc->disc_work, delay);
		} else
			fc_disc_done(disc, DISC_EV_FAILED);
	} else if (PTR_ERR(fp) == -FC_EX_CLOSED) {
		disc->pending = 0;
	}
}

static void fc_disc_gpn_ft_req(struct fc_disc *disc)
{
	struct fc_frame *fp;
	struct fc_lport *lport = fc_disc_lport(disc);

	WARN_ON(!fc_lport_test_ready(lport));

	disc->pending = 1;
	disc->requested = 0;

	disc->buf_len = 0;
	disc->seq_count = 0;
	fp = fc_frame_alloc(lport,
			    sizeof(struct fc_ct_hdr) +
			    sizeof(struct fc_ns_gid_ft));
	if (!fp)
		goto err;

	if (lport->tt.elsct_send(lport, 0, fp,
				 FC_NS_GPN_FT,
				 fc_disc_gpn_ft_resp,
				 disc, 3 * lport->r_a_tov))
		return;
err:
	fc_disc_error(disc, NULL);
}

static int fc_disc_gpn_ft_parse(struct fc_disc *disc, void *buf, size_t len)
{
	struct fc_lport *lport;
	struct fc_gpn_ft_resp *np;
	char *bp;
	size_t plen;
	size_t tlen;
	int error = 0;
	struct fc_rport_identifiers ids;
	struct fc_rport_priv *rdata;

	lport = fc_disc_lport(disc);
	disc->seq_count++;

	bp = buf;
	plen = len;
	np = (struct fc_gpn_ft_resp *)bp;
	tlen = disc->buf_len;
	disc->buf_len = 0;
	if (tlen) {
		WARN_ON(tlen >= sizeof(*np));
		plen = sizeof(*np) - tlen;
		WARN_ON(plen <= 0);
		WARN_ON(plen >= sizeof(*np));
		if (plen > len)
			plen = len;
		np = &disc->partial_buf;
		memcpy((char *)np + tlen, bp, plen);

		bp -= tlen;
		len += tlen;
		plen += tlen;
		disc->buf_len = (unsigned char) plen;
		if (plen == sizeof(*np))
			disc->buf_len = 0;
	}

	while (plen >= sizeof(*np)) {
		ids.port_id = ntoh24(np->fp_fid);
		ids.port_name = ntohll(np->fp_wwpn);

		if (ids.port_id != lport->port_id &&
		    ids.port_name != lport->wwpn) {
			rdata = lport->tt.rport_create(lport, ids.port_id);
			if (rdata) {
				rdata->ids.port_name = ids.port_name;
				rdata->disc_id = disc->disc_id;
			} else {
				printk(KERN_WARNING "libfc: Failed to allocate "
				       "memory for the newly discovered port "
				       "(%6.6x)\n", ids.port_id);
				error = -ENOMEM;
			}
		}

		if (np->fp_flags & FC_NS_FID_LAST) {
			fc_disc_done(disc, DISC_EV_SUCCESS);
			len = 0;
			break;
		}
		len -= sizeof(*np);
		bp += sizeof(*np);
		np = (struct fc_gpn_ft_resp *)bp;
		plen = len;
	}

	if (error == 0 && len > 0 && len < sizeof(*np)) {
		if (np != &disc->partial_buf) {
			FC_DISC_DBG(disc, "Partial buffer remains "
				    "for discovery\n");
			memcpy(&disc->partial_buf, np, len);
		}
		disc->buf_len = (unsigned char) len;
	}
	return error;
}

static void fc_disc_timeout(struct work_struct *work)
{
	struct fc_disc *disc = container_of(work,
					    struct fc_disc,
					    disc_work.work);
	mutex_lock(&disc->disc_mutex);
	fc_disc_gpn_ft_req(disc);
	mutex_unlock(&disc->disc_mutex);
}

static void fc_disc_gpn_ft_resp(struct fc_seq *sp, struct fc_frame *fp,
				void *disc_arg)
{
	struct fc_disc *disc = disc_arg;
	struct fc_ct_hdr *cp;
	struct fc_frame_header *fh;
	enum fc_disc_event event = DISC_EV_NONE;
	unsigned int seq_cnt;
	unsigned int len;
	int error = 0;

	mutex_lock(&disc->disc_mutex);
	FC_DISC_DBG(disc, "Received a GPN_FT response\n");

	if (IS_ERR(fp)) {
		fc_disc_error(disc, fp);
		mutex_unlock(&disc->disc_mutex);
		return;
	}

	WARN_ON(!fc_frame_is_linear(fp));	
	fh = fc_frame_header_get(fp);
	len = fr_len(fp) - sizeof(*fh);
	seq_cnt = ntohs(fh->fh_seq_cnt);
	if (fr_sof(fp) == FC_SOF_I3 && seq_cnt == 0 && disc->seq_count == 0) {
		cp = fc_frame_payload_get(fp, sizeof(*cp));
		if (!cp) {
			FC_DISC_DBG(disc, "GPN_FT response too short, len %d\n",
				    fr_len(fp));
			event = DISC_EV_FAILED;
		} else if (ntohs(cp->ct_cmd) == FC_FS_ACC) {

			
			len -= sizeof(*cp);
			error = fc_disc_gpn_ft_parse(disc, cp + 1, len);
		} else if (ntohs(cp->ct_cmd) == FC_FS_RJT) {
			FC_DISC_DBG(disc, "GPN_FT rejected reason %x exp %x "
				    "(check zoning)\n", cp->ct_reason,
				    cp->ct_explan);
			event = DISC_EV_FAILED;
			if (cp->ct_reason == FC_FS_RJT_UNABL &&
			    cp->ct_explan == FC_FS_EXP_FTNR)
				event = DISC_EV_SUCCESS;
		} else {
			FC_DISC_DBG(disc, "GPN_FT unexpected response code "
				    "%x\n", ntohs(cp->ct_cmd));
			event = DISC_EV_FAILED;
		}
	} else if (fr_sof(fp) == FC_SOF_N3 && seq_cnt == disc->seq_count) {
		error = fc_disc_gpn_ft_parse(disc, fh + 1, len);
	} else {
		FC_DISC_DBG(disc, "GPN_FT unexpected frame - out of sequence? "
			    "seq_cnt %x expected %x sof %x eof %x\n",
			    seq_cnt, disc->seq_count, fr_sof(fp), fr_eof(fp));
		event = DISC_EV_FAILED;
	}
	if (error)
		fc_disc_error(disc, fp);
	else if (event != DISC_EV_NONE)
		fc_disc_done(disc, event);
	fc_frame_free(fp);
	mutex_unlock(&disc->disc_mutex);
}

static void fc_disc_gpn_id_resp(struct fc_seq *sp, struct fc_frame *fp,
				void *rdata_arg)
{
	struct fc_rport_priv *rdata = rdata_arg;
	struct fc_rport_priv *new_rdata;
	struct fc_lport *lport;
	struct fc_disc *disc;
	struct fc_ct_hdr *cp;
	struct fc_ns_gid_pn *pn;
	u64 port_name;

	lport = rdata->local_port;
	disc = &lport->disc;

	mutex_lock(&disc->disc_mutex);
	if (PTR_ERR(fp) == -FC_EX_CLOSED)
		goto out;
	if (IS_ERR(fp))
		goto redisc;

	cp = fc_frame_payload_get(fp, sizeof(*cp));
	if (!cp)
		goto redisc;
	if (ntohs(cp->ct_cmd) == FC_FS_ACC) {
		if (fr_len(fp) < sizeof(struct fc_frame_header) +
		    sizeof(*cp) + sizeof(*pn))
			goto redisc;
		pn = (struct fc_ns_gid_pn *)(cp + 1);
		port_name = get_unaligned_be64(&pn->fn_wwpn);
		if (rdata->ids.port_name == -1)
			rdata->ids.port_name = port_name;
		else if (rdata->ids.port_name != port_name) {
			FC_DISC_DBG(disc, "GPN_ID accepted.  WWPN changed. "
				    "Port-id %6.6x wwpn %16.16llx\n",
				    rdata->ids.port_id, port_name);
			lport->tt.rport_logoff(rdata);

			new_rdata = lport->tt.rport_create(lport,
							   rdata->ids.port_id);
			if (new_rdata) {
				new_rdata->disc_id = disc->disc_id;
				lport->tt.rport_login(new_rdata);
			}
			goto out;
		}
		rdata->disc_id = disc->disc_id;
		lport->tt.rport_login(rdata);
	} else if (ntohs(cp->ct_cmd) == FC_FS_RJT) {
		FC_DISC_DBG(disc, "GPN_ID rejected reason %x exp %x\n",
			    cp->ct_reason, cp->ct_explan);
		lport->tt.rport_logoff(rdata);
	} else {
		FC_DISC_DBG(disc, "GPN_ID unexpected response code %x\n",
			    ntohs(cp->ct_cmd));
redisc:
		fc_disc_restart(disc);
	}
out:
	mutex_unlock(&disc->disc_mutex);
	kref_put(&rdata->kref, lport->tt.rport_destroy);
}

static int fc_disc_gpn_id_req(struct fc_lport *lport,
			      struct fc_rport_priv *rdata)
{
	struct fc_frame *fp;

	fp = fc_frame_alloc(lport, sizeof(struct fc_ct_hdr) +
			    sizeof(struct fc_ns_fid));
	if (!fp)
		return -ENOMEM;
	if (!lport->tt.elsct_send(lport, rdata->ids.port_id, fp, FC_NS_GPN_ID,
				  fc_disc_gpn_id_resp, rdata,
				  3 * lport->r_a_tov))
		return -ENOMEM;
	kref_get(&rdata->kref);
	return 0;
}

static int fc_disc_single(struct fc_lport *lport, struct fc_disc_port *dp)
{
	struct fc_rport_priv *rdata;

	rdata = lport->tt.rport_create(lport, dp->port_id);
	if (!rdata)
		return -ENOMEM;
	rdata->disc_id = 0;
	return fc_disc_gpn_id_req(lport, rdata);
}

static void fc_disc_stop(struct fc_lport *lport)
{
	struct fc_disc *disc = &lport->disc;

	if (disc->pending)
		cancel_delayed_work_sync(&disc->disc_work);
	fc_disc_stop_rports(disc);
}

static void fc_disc_stop_final(struct fc_lport *lport)
{
	fc_disc_stop(lport);
	lport->tt.rport_flush_queue();
}

int fc_disc_init(struct fc_lport *lport)
{
	struct fc_disc *disc;

	if (!lport->tt.disc_start)
		lport->tt.disc_start = fc_disc_start;

	if (!lport->tt.disc_stop)
		lport->tt.disc_stop = fc_disc_stop;

	if (!lport->tt.disc_stop_final)
		lport->tt.disc_stop_final = fc_disc_stop_final;

	if (!lport->tt.disc_recv_req)
		lport->tt.disc_recv_req = fc_disc_recv_req;

	disc = &lport->disc;
	INIT_DELAYED_WORK(&disc->disc_work, fc_disc_timeout);
	mutex_init(&disc->disc_mutex);
	INIT_LIST_HEAD(&disc->rports);

	disc->priv = lport;

	return 0;
}
EXPORT_SYMBOL(fc_disc_init);
