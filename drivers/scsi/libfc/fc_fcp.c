/*
 * Copyright(c) 2007 Intel Corporation. All rights reserved.
 * Copyright(c) 2008 Red Hat, Inc.  All rights reserved.
 * Copyright(c) 2008 Mike Christie
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include <linux/crc32.h>
#include <linux/slab.h>

#include <scsi/scsi_tcq.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>

#include <scsi/fc/fc_fc2.h>

#include <scsi/libfc.h>
#include <scsi/fc_encode.h>

#include "fc_libfc.h"

static struct kmem_cache *scsi_pkt_cachep;

#define FC_SRB_FREE		0		
#define FC_SRB_CMD_SENT		(1 << 0)	
#define FC_SRB_RCV_STATUS	(1 << 1)	
#define FC_SRB_ABORT_PENDING	(1 << 2)	
#define FC_SRB_ABORTED		(1 << 3)	
#define FC_SRB_DISCONTIG	(1 << 4)	
#define FC_SRB_COMPL		(1 << 5)	
#define FC_SRB_FCP_PROCESSING_TMO (1 << 6)	

#define FC_SRB_READ		(1 << 1)
#define FC_SRB_WRITE		(1 << 0)

#define CMD_SP(Cmnd)		    ((struct fc_fcp_pkt *)(Cmnd)->SCp.ptr)
#define CMD_ENTRY_STATUS(Cmnd)	    ((Cmnd)->SCp.have_data_in)
#define CMD_COMPL_STATUS(Cmnd)	    ((Cmnd)->SCp.this_residual)
#define CMD_SCSI_STATUS(Cmnd)	    ((Cmnd)->SCp.Status)
#define CMD_RESID_LEN(Cmnd)	    ((Cmnd)->SCp.buffers_residual)

struct fc_fcp_internal {
	mempool_t		*scsi_pkt_pool;
	spinlock_t		scsi_queue_lock;
	struct list_head	scsi_pkt_queue;
	unsigned long		last_can_queue_ramp_down_time;
	unsigned long		last_can_queue_ramp_up_time;
	int			max_can_queue;
};

#define fc_get_scsi_internal(x)	((struct fc_fcp_internal *)(x)->scsi_priv)

static void fc_fcp_recv_data(struct fc_fcp_pkt *, struct fc_frame *);
static void fc_fcp_recv(struct fc_seq *, struct fc_frame *, void *);
static void fc_fcp_resp(struct fc_fcp_pkt *, struct fc_frame *);
static void fc_fcp_complete_locked(struct fc_fcp_pkt *);
static void fc_tm_done(struct fc_seq *, struct fc_frame *, void *);
static void fc_fcp_error(struct fc_fcp_pkt *, struct fc_frame *);
static void fc_fcp_recovery(struct fc_fcp_pkt *, u8 code);
static void fc_fcp_timeout(unsigned long);
static void fc_fcp_rec(struct fc_fcp_pkt *);
static void fc_fcp_rec_error(struct fc_fcp_pkt *, struct fc_frame *);
static void fc_fcp_rec_resp(struct fc_seq *, struct fc_frame *, void *);
static void fc_io_compl(struct fc_fcp_pkt *);

static void fc_fcp_srr(struct fc_fcp_pkt *, enum fc_rctl, u32);
static void fc_fcp_srr_resp(struct fc_seq *, struct fc_frame *, void *);
static void fc_fcp_srr_error(struct fc_fcp_pkt *, struct fc_frame *);

#define FC_COMPLETE		0
#define FC_CMD_ABORTED		1
#define FC_CMD_RESET		2
#define FC_CMD_PLOGO		3
#define FC_SNS_RCV		4
#define FC_TRANS_ERR		5
#define FC_DATA_OVRRUN		6
#define FC_DATA_UNDRUN		7
#define FC_ERROR		8
#define FC_HRD_ERROR		9
#define FC_CRC_ERROR		10
#define FC_TIMED_OUT		11

#define FC_SCSI_TM_TOV		(10 * HZ)
#define FC_HOST_RESET_TIMEOUT	(30 * HZ)
#define FC_CAN_QUEUE_PERIOD	(60 * HZ)

#define FC_MAX_ERROR_CNT	5
#define FC_MAX_RECOV_RETRY	3

#define FC_FCP_DFLT_QUEUE_DEPTH 32

static struct fc_fcp_pkt *fc_fcp_pkt_alloc(struct fc_lport *lport, gfp_t gfp)
{
	struct fc_fcp_internal *si = fc_get_scsi_internal(lport);
	struct fc_fcp_pkt *fsp;

	fsp = mempool_alloc(si->scsi_pkt_pool, gfp);
	if (fsp) {
		memset(fsp, 0, sizeof(*fsp));
		fsp->lp = lport;
		fsp->xfer_ddp = FC_XID_UNKNOWN;
		atomic_set(&fsp->ref_cnt, 1);
		init_timer(&fsp->timer);
		fsp->timer.data = (unsigned long)fsp;
		INIT_LIST_HEAD(&fsp->list);
		spin_lock_init(&fsp->scsi_pkt_lock);
	}
	return fsp;
}

static void fc_fcp_pkt_release(struct fc_fcp_pkt *fsp)
{
	if (atomic_dec_and_test(&fsp->ref_cnt)) {
		struct fc_fcp_internal *si = fc_get_scsi_internal(fsp->lp);

		mempool_free(fsp, si->scsi_pkt_pool);
	}
}

static void fc_fcp_pkt_hold(struct fc_fcp_pkt *fsp)
{
	atomic_inc(&fsp->ref_cnt);
}

static void fc_fcp_pkt_destroy(struct fc_seq *seq, void *fsp)
{
	fc_fcp_pkt_release(fsp);
}

static inline int fc_fcp_lock_pkt(struct fc_fcp_pkt *fsp)
{
	spin_lock_bh(&fsp->scsi_pkt_lock);
	if (fsp->state & FC_SRB_COMPL) {
		spin_unlock_bh(&fsp->scsi_pkt_lock);
		return -EPERM;
	}

	fc_fcp_pkt_hold(fsp);
	return 0;
}

static inline void fc_fcp_unlock_pkt(struct fc_fcp_pkt *fsp)
{
	spin_unlock_bh(&fsp->scsi_pkt_lock);
	fc_fcp_pkt_release(fsp);
}

static void fc_fcp_timer_set(struct fc_fcp_pkt *fsp, unsigned long delay)
{
	if (!(fsp->state & FC_SRB_COMPL))
		mod_timer(&fsp->timer, jiffies + delay);
}

static int fc_fcp_send_abort(struct fc_fcp_pkt *fsp)
{
	if (!fsp->seq_ptr)
		return -EINVAL;

	fsp->state |= FC_SRB_ABORT_PENDING;
	return fsp->lp->tt.seq_exch_abort(fsp->seq_ptr, 0);
}

static void fc_fcp_retry_cmd(struct fc_fcp_pkt *fsp)
{
	if (fsp->seq_ptr) {
		fsp->lp->tt.exch_done(fsp->seq_ptr);
		fsp->seq_ptr = NULL;
	}

	fsp->state &= ~FC_SRB_ABORT_PENDING;
	fsp->io_status = 0;
	fsp->status_code = FC_ERROR;
	fc_fcp_complete_locked(fsp);
}

void fc_fcp_ddp_setup(struct fc_fcp_pkt *fsp, u16 xid)
{
	struct fc_lport *lport;

	lport = fsp->lp;
	if ((fsp->req_flags & FC_SRB_READ) &&
	    (lport->lro_enabled) && (lport->tt.ddp_setup)) {
		if (lport->tt.ddp_setup(lport, xid, scsi_sglist(fsp->cmd),
					scsi_sg_count(fsp->cmd)))
			fsp->xfer_ddp = xid;
	}
}

void fc_fcp_ddp_done(struct fc_fcp_pkt *fsp)
{
	struct fc_lport *lport;

	if (!fsp)
		return;

	if (fsp->xfer_ddp == FC_XID_UNKNOWN)
		return;

	lport = fsp->lp;
	if (lport->tt.ddp_done) {
		fsp->xfer_len = lport->tt.ddp_done(lport, fsp->xfer_ddp);
		fsp->xfer_ddp = FC_XID_UNKNOWN;
	}
}

static void fc_fcp_can_queue_ramp_up(struct fc_lport *lport)
{
	struct fc_fcp_internal *si = fc_get_scsi_internal(lport);
	unsigned long flags;
	int can_queue;

	spin_lock_irqsave(lport->host->host_lock, flags);

	if (si->last_can_queue_ramp_up_time &&
	    (time_before(jiffies, si->last_can_queue_ramp_up_time +
			 FC_CAN_QUEUE_PERIOD)))
		goto unlock;

	if (time_before(jiffies, si->last_can_queue_ramp_down_time +
			FC_CAN_QUEUE_PERIOD))
		goto unlock;

	si->last_can_queue_ramp_up_time = jiffies;

	can_queue = lport->host->can_queue << 1;
	if (can_queue >= si->max_can_queue) {
		can_queue = si->max_can_queue;
		si->last_can_queue_ramp_down_time = 0;
	}
	lport->host->can_queue = can_queue;
	shost_printk(KERN_ERR, lport->host, "libfc: increased "
		     "can_queue to %d.\n", can_queue);

unlock:
	spin_unlock_irqrestore(lport->host->host_lock, flags);
}

static void fc_fcp_can_queue_ramp_down(struct fc_lport *lport)
{
	struct fc_fcp_internal *si = fc_get_scsi_internal(lport);
	unsigned long flags;
	int can_queue;

	spin_lock_irqsave(lport->host->host_lock, flags);

	if (si->last_can_queue_ramp_down_time &&
	    (time_before(jiffies, si->last_can_queue_ramp_down_time +
			 FC_CAN_QUEUE_PERIOD)))
		goto unlock;

	si->last_can_queue_ramp_down_time = jiffies;

	can_queue = lport->host->can_queue;
	can_queue >>= 1;
	if (!can_queue)
		can_queue = 1;
	lport->host->can_queue = can_queue;
	shost_printk(KERN_ERR, lport->host, "libfc: Could not allocate frame.\n"
		     "Reducing can_queue to %d.\n", can_queue);

unlock:
	spin_unlock_irqrestore(lport->host->host_lock, flags);
}

static inline struct fc_frame *fc_fcp_frame_alloc(struct fc_lport *lport,
						  size_t len)
{
	struct fc_frame *fp;

	fp = fc_frame_alloc(lport, len);
	if (likely(fp))
		return fp;

	
	fc_fcp_can_queue_ramp_down(lport);
	return NULL;
}

static void fc_fcp_recv_data(struct fc_fcp_pkt *fsp, struct fc_frame *fp)
{
	struct scsi_cmnd *sc = fsp->cmd;
	struct fc_lport *lport = fsp->lp;
	struct fcoe_dev_stats *stats;
	struct fc_frame_header *fh;
	size_t start_offset;
	size_t offset;
	u32 crc;
	u32 copy_len = 0;
	size_t len;
	void *buf;
	struct scatterlist *sg;
	u32 nents;
	u8 host_bcode = FC_COMPLETE;

	fh = fc_frame_header_get(fp);
	offset = ntohl(fh->fh_parm_offset);
	start_offset = offset;
	len = fr_len(fp) - sizeof(*fh);
	buf = fc_frame_payload_get(fp, 0);

	if (fsp->xfer_ddp != FC_XID_UNKNOWN) {
		fc_fcp_ddp_done(fsp);
		FC_FCP_DBG(fsp, "DDP I/O in fc_fcp_recv_data set ERROR\n");
		host_bcode = FC_ERROR;
		goto err;
	}
	if (offset + len > fsp->data_len) {
		
		if ((fr_flags(fp) & FCPHF_CRC_UNCHECKED) &&
		    fc_frame_crc_check(fp))
			goto crc_err;
		FC_FCP_DBG(fsp, "data received past end. len %zx offset %zx "
			   "data_len %x\n", len, offset, fsp->data_len);

		
		host_bcode = FC_DATA_OVRRUN;
		goto err;
	}
	if (offset != fsp->xfer_len)
		fsp->state |= FC_SRB_DISCONTIG;

	sg = scsi_sglist(sc);
	nents = scsi_sg_count(sc);

	if (!(fr_flags(fp) & FCPHF_CRC_UNCHECKED)) {
		copy_len = fc_copy_buffer_to_sglist(buf, len, sg, &nents,
						    &offset, NULL);
	} else {
		crc = crc32(~0, (u8 *) fh, sizeof(*fh));
		copy_len = fc_copy_buffer_to_sglist(buf, len, sg, &nents,
						    &offset, &crc);
		buf = fc_frame_payload_get(fp, 0);
		if (len % 4)
			crc = crc32(crc, buf + len, 4 - (len % 4));

		if (~crc != le32_to_cpu(fr_crc(fp))) {
crc_err:
			stats = per_cpu_ptr(lport->dev_stats, get_cpu());
			stats->ErrorFrames++;
			
			if (stats->InvalidCRCCount++ < FC_MAX_ERROR_CNT)
				printk(KERN_WARNING "libfc: CRC error on data "
				       "frame for port (%6.6x)\n",
				       lport->port_id);
			put_cpu();
			if (fsp->state & FC_SRB_DISCONTIG) {
				host_bcode = FC_CRC_ERROR;
				goto err;
			}
			return;
		}
	}

	if (fsp->xfer_contig_end == start_offset)
		fsp->xfer_contig_end += copy_len;
	fsp->xfer_len += copy_len;

	if (unlikely(fsp->state & FC_SRB_RCV_STATUS) &&
	    fsp->xfer_len == fsp->data_len - fsp->scsi_resid)
		fc_fcp_complete_locked(fsp);
	return;
err:
	fc_fcp_recovery(fsp, host_bcode);
}

static int fc_fcp_send_data(struct fc_fcp_pkt *fsp, struct fc_seq *seq,
			    size_t offset, size_t seq_blen)
{
	struct fc_exch *ep;
	struct scsi_cmnd *sc;
	struct scatterlist *sg;
	struct fc_frame *fp = NULL;
	struct fc_lport *lport = fsp->lp;
	struct page *page;
	size_t remaining;
	size_t t_blen;
	size_t tlen;
	size_t sg_bytes;
	size_t frame_offset, fh_parm_offset;
	size_t off;
	int error;
	void *data = NULL;
	void *page_addr;
	int using_sg = lport->sg_supp;
	u32 f_ctl;

	WARN_ON(seq_blen <= 0);
	if (unlikely(offset + seq_blen > fsp->data_len)) {
		
		FC_FCP_DBG(fsp, "xfer-ready past end. seq_blen %zx "
			   "offset %zx\n", seq_blen, offset);
		fc_fcp_send_abort(fsp);
		return 0;
	} else if (offset != fsp->xfer_len) {
		
		FC_FCP_DBG(fsp, "xfer-ready non-contiguous. "
			   "seq_blen %zx offset %zx\n", seq_blen, offset);
	}

	t_blen = fsp->max_payload;
	if (lport->seq_offload) {
		t_blen = min(seq_blen, (size_t)lport->lso_max);
		FC_FCP_DBG(fsp, "fsp=%p:lso:blen=%zx lso_max=0x%x t_blen=%zx\n",
			   fsp, seq_blen, lport->lso_max, t_blen);
	}

	if (t_blen > 512)
		t_blen &= ~(512 - 1);	
	sc = fsp->cmd;

	remaining = seq_blen;
	fh_parm_offset = frame_offset = offset;
	tlen = 0;
	seq = lport->tt.seq_start_next(seq);
	f_ctl = FC_FC_REL_OFF;
	WARN_ON(!seq);

	sg = scsi_sglist(sc);

	while (remaining > 0 && sg) {
		if (offset >= sg->length) {
			offset -= sg->length;
			sg = sg_next(sg);
			continue;
		}
		if (!fp) {
			tlen = min(t_blen, remaining);

			if (tlen % 4)
				using_sg = 0;
			fp = fc_frame_alloc(lport, using_sg ? 0 : tlen);
			if (!fp)
				return -ENOMEM;

			data = fc_frame_header_get(fp) + 1;
			fh_parm_offset = frame_offset;
			fr_max_payload(fp) = fsp->max_payload;
		}

		off = offset + sg->offset;
		sg_bytes = min(tlen, sg->length - offset);
		sg_bytes = min(sg_bytes,
			       (size_t) (PAGE_SIZE - (off & ~PAGE_MASK)));
		page = sg_page(sg) + (off >> PAGE_SHIFT);
		if (using_sg) {
			get_page(page);
			skb_fill_page_desc(fp_skb(fp),
					   skb_shinfo(fp_skb(fp))->nr_frags,
					   page, off & ~PAGE_MASK, sg_bytes);
			fp_skb(fp)->data_len += sg_bytes;
			fr_len(fp) += sg_bytes;
			fp_skb(fp)->truesize += PAGE_SIZE;
		} else {
			page_addr = kmap_atomic(page);
			memcpy(data, (char *)page_addr + (off & ~PAGE_MASK),
			       sg_bytes);
			kunmap_atomic(page_addr);
			data += sg_bytes;
		}
		offset += sg_bytes;
		frame_offset += sg_bytes;
		tlen -= sg_bytes;
		remaining -= sg_bytes;

		if ((skb_shinfo(fp_skb(fp))->nr_frags < FC_FRAME_SG_LEN) &&
		    (tlen))
			continue;

		if (remaining == 0)
			f_ctl |= FC_FC_SEQ_INIT | FC_FC_END_SEQ;

		ep = fc_seq_exch(seq);
		fc_fill_fc_hdr(fp, FC_RCTL_DD_SOL_DATA, ep->did, ep->sid,
			       FC_TYPE_FCP, f_ctl, fh_parm_offset);

		error = lport->tt.seq_send(lport, seq, fp);
		if (error) {
			WARN_ON(1);		
			return error;
		}
		fp = NULL;
	}
	fsp->xfer_len += seq_blen;	
	return 0;
}

static void fc_fcp_abts_resp(struct fc_fcp_pkt *fsp, struct fc_frame *fp)
{
	int ba_done = 1;
	struct fc_ba_rjt *brp;
	struct fc_frame_header *fh;

	fh = fc_frame_header_get(fp);
	switch (fh->fh_r_ctl) {
	case FC_RCTL_BA_ACC:
		break;
	case FC_RCTL_BA_RJT:
		brp = fc_frame_payload_get(fp, sizeof(*brp));
		if (brp && brp->br_reason == FC_BA_RJT_LOG_ERR)
			break;
		
	default:
		ba_done = 0;
	}

	if (ba_done) {
		fsp->state |= FC_SRB_ABORTED;
		fsp->state &= ~FC_SRB_ABORT_PENDING;

		if (fsp->wait_for_comp)
			complete(&fsp->tm_done);
		else
			fc_fcp_complete_locked(fsp);
	}
}

static void fc_fcp_recv(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fc_fcp_pkt *fsp = (struct fc_fcp_pkt *)arg;
	struct fc_lport *lport = fsp->lp;
	struct fc_frame_header *fh;
	struct fcp_txrdy *dd;
	u8 r_ctl;
	int rc = 0;

	if (IS_ERR(fp)) {
		fc_fcp_error(fsp, fp);
		return;
	}

	fh = fc_frame_header_get(fp);
	r_ctl = fh->fh_r_ctl;

	if (lport->state != LPORT_ST_READY)
		goto out;
	if (fc_fcp_lock_pkt(fsp))
		goto out;

	if (fh->fh_type == FC_TYPE_BLS) {
		fc_fcp_abts_resp(fsp, fp);
		goto unlock;
	}

	if (fsp->state & (FC_SRB_ABORTED | FC_SRB_ABORT_PENDING))
		goto unlock;

	if (r_ctl == FC_RCTL_DD_DATA_DESC) {
		WARN_ON(fr_flags(fp) & FCPHF_CRC_UNCHECKED);
		dd = fc_frame_payload_get(fp, sizeof(*dd));
		WARN_ON(!dd);

		rc = fc_fcp_send_data(fsp, seq,
				      (size_t) ntohl(dd->ft_data_ro),
				      (size_t) ntohl(dd->ft_burst_len));
		if (!rc)
			seq->rec_data = fsp->xfer_len;
	} else if (r_ctl == FC_RCTL_DD_SOL_DATA) {
		WARN_ON(fr_len(fp) < sizeof(*fh));	
		fc_fcp_recv_data(fsp, fp);
		seq->rec_data = fsp->xfer_contig_end;
	} else if (r_ctl == FC_RCTL_DD_CMD_STATUS) {
		WARN_ON(fr_flags(fp) & FCPHF_CRC_UNCHECKED);

		fc_fcp_resp(fsp, fp);
	} else {
		FC_FCP_DBG(fsp, "unexpected frame.  r_ctl %x\n", r_ctl);
	}
unlock:
	fc_fcp_unlock_pkt(fsp);
out:
	fc_frame_free(fp);
}

static void fc_fcp_resp(struct fc_fcp_pkt *fsp, struct fc_frame *fp)
{
	struct fc_frame_header *fh;
	struct fcp_resp *fc_rp;
	struct fcp_resp_ext *rp_ex;
	struct fcp_resp_rsp_info *fc_rp_info;
	u32 plen;
	u32 expected_len;
	u32 respl = 0;
	u32 snsl = 0;
	u8 flags = 0;

	plen = fr_len(fp);
	fh = (struct fc_frame_header *)fr_hdr(fp);
	if (unlikely(plen < sizeof(*fh) + sizeof(*fc_rp)))
		goto len_err;
	plen -= sizeof(*fh);
	fc_rp = (struct fcp_resp *)(fh + 1);
	fsp->cdb_status = fc_rp->fr_status;
	flags = fc_rp->fr_flags;
	fsp->scsi_comp_flags = flags;
	expected_len = fsp->data_len;

	
	fc_fcp_ddp_done(fsp);

	if (unlikely((flags & ~FCP_CONF_REQ) || fc_rp->fr_status)) {
		rp_ex = (void *)(fc_rp + 1);
		if (flags & (FCP_RSP_LEN_VAL | FCP_SNS_LEN_VAL)) {
			if (plen < sizeof(*fc_rp) + sizeof(*rp_ex))
				goto len_err;
			fc_rp_info = (struct fcp_resp_rsp_info *)(rp_ex + 1);
			if (flags & FCP_RSP_LEN_VAL) {
				respl = ntohl(rp_ex->fr_rsp_len);
				if (respl != sizeof(*fc_rp_info))
					goto len_err;
				if (fsp->wait_for_comp) {
					
					fsp->cdb_status = fc_rp_info->rsp_code;
					complete(&fsp->tm_done);
					return;
				}
			}
			if (flags & FCP_SNS_LEN_VAL) {
				snsl = ntohl(rp_ex->fr_sns_len);
				if (snsl > SCSI_SENSE_BUFFERSIZE)
					snsl = SCSI_SENSE_BUFFERSIZE;
				memcpy(fsp->cmd->sense_buffer,
				       (char *)fc_rp_info + respl, snsl);
			}
		}
		if (flags & (FCP_RESID_UNDER | FCP_RESID_OVER)) {
			if (plen < sizeof(*fc_rp) + sizeof(rp_ex->fr_resid))
				goto len_err;
			if (flags & FCP_RESID_UNDER) {
				fsp->scsi_resid = ntohl(rp_ex->fr_resid);
				if (!(flags & FCP_SNS_LEN_VAL) &&
				    (fc_rp->fr_status == 0) &&
				    (scsi_bufflen(fsp->cmd) -
				     fsp->scsi_resid) < fsp->cmd->underflow)
					goto err;
				expected_len -= fsp->scsi_resid;
			} else {
				fsp->status_code = FC_ERROR;
			}
		}
	}
	fsp->state |= FC_SRB_RCV_STATUS;

	if (unlikely(fsp->xfer_len != expected_len)) {
		if (fsp->xfer_len < expected_len) {
			fc_fcp_timer_set(fsp, 2);
			return;
		}
		fsp->status_code = FC_DATA_OVRRUN;
		FC_FCP_DBG(fsp, "tgt %6.6x xfer len %zx greater than expected, "
			   "len %x, data len %x\n",
			   fsp->rport->port_id,
			   fsp->xfer_len, expected_len, fsp->data_len);
	}
	fc_fcp_complete_locked(fsp);
	return;

len_err:
	FC_FCP_DBG(fsp, "short FCP response. flags 0x%x len %u respl %u "
		   "snsl %u\n", flags, fr_len(fp), respl, snsl);
err:
	fsp->status_code = FC_ERROR;
	fc_fcp_complete_locked(fsp);
}

static void fc_fcp_complete_locked(struct fc_fcp_pkt *fsp)
{
	struct fc_lport *lport = fsp->lp;
	struct fc_seq *seq;
	struct fc_exch *ep;
	u32 f_ctl;

	if (fsp->state & FC_SRB_ABORT_PENDING)
		return;

	if (fsp->state & FC_SRB_ABORTED) {
		if (!fsp->status_code)
			fsp->status_code = FC_CMD_ABORTED;
	} else {
		if (fsp->xfer_len < fsp->data_len && !fsp->io_status &&
		    (!(fsp->scsi_comp_flags & FCP_RESID_UNDER) ||
		     fsp->xfer_len < fsp->data_len - fsp->scsi_resid)) {
			fsp->status_code = FC_DATA_UNDRUN;
			fsp->io_status = 0;
		}
	}

	seq = fsp->seq_ptr;
	if (seq) {
		fsp->seq_ptr = NULL;
		if (unlikely(fsp->scsi_comp_flags & FCP_CONF_REQ)) {
			struct fc_frame *conf_frame;
			struct fc_seq *csp;

			csp = lport->tt.seq_start_next(seq);
			conf_frame = fc_fcp_frame_alloc(fsp->lp, 0);
			if (conf_frame) {
				f_ctl = FC_FC_SEQ_INIT;
				f_ctl |= FC_FC_LAST_SEQ | FC_FC_END_SEQ;
				ep = fc_seq_exch(seq);
				fc_fill_fc_hdr(conf_frame, FC_RCTL_DD_SOL_CTL,
					       ep->did, ep->sid,
					       FC_TYPE_FCP, f_ctl, 0);
				lport->tt.seq_send(lport, csp, conf_frame);
			}
		}
		lport->tt.exch_done(seq);
	}
	if (fsp->cmd)
		fc_io_compl(fsp);
}

static void fc_fcp_cleanup_cmd(struct fc_fcp_pkt *fsp, int error)
{
	struct fc_lport *lport = fsp->lp;

	if (fsp->seq_ptr) {
		lport->tt.exch_done(fsp->seq_ptr);
		fsp->seq_ptr = NULL;
	}
	fsp->status_code = error;
}

static void fc_fcp_cleanup_each_cmd(struct fc_lport *lport, unsigned int id,
				    unsigned int lun, int error)
{
	struct fc_fcp_internal *si = fc_get_scsi_internal(lport);
	struct fc_fcp_pkt *fsp;
	struct scsi_cmnd *sc_cmd;
	unsigned long flags;

	spin_lock_irqsave(&si->scsi_queue_lock, flags);
restart:
	list_for_each_entry(fsp, &si->scsi_pkt_queue, list) {
		sc_cmd = fsp->cmd;
		if (id != -1 && scmd_id(sc_cmd) != id)
			continue;

		if (lun != -1 && sc_cmd->device->lun != lun)
			continue;

		fc_fcp_pkt_hold(fsp);
		spin_unlock_irqrestore(&si->scsi_queue_lock, flags);

		if (!fc_fcp_lock_pkt(fsp)) {
			fc_fcp_cleanup_cmd(fsp, error);
			fc_io_compl(fsp);
			fc_fcp_unlock_pkt(fsp);
		}

		fc_fcp_pkt_release(fsp);
		spin_lock_irqsave(&si->scsi_queue_lock, flags);
		goto restart;
	}
	spin_unlock_irqrestore(&si->scsi_queue_lock, flags);
}

static void fc_fcp_abort_io(struct fc_lport *lport)
{
	fc_fcp_cleanup_each_cmd(lport, -1, -1, FC_HRD_ERROR);
}

static int fc_fcp_pkt_send(struct fc_lport *lport, struct fc_fcp_pkt *fsp)
{
	struct fc_fcp_internal *si = fc_get_scsi_internal(lport);
	unsigned long flags;
	int rc;

	fsp->cmd->SCp.ptr = (char *)fsp;
	fsp->cdb_cmd.fc_dl = htonl(fsp->data_len);
	fsp->cdb_cmd.fc_flags = fsp->req_flags & ~FCP_CFL_LEN_MASK;

	int_to_scsilun(fsp->cmd->device->lun, &fsp->cdb_cmd.fc_lun);
	memcpy(fsp->cdb_cmd.fc_cdb, fsp->cmd->cmnd, fsp->cmd->cmd_len);

	spin_lock_irqsave(&si->scsi_queue_lock, flags);
	list_add_tail(&fsp->list, &si->scsi_pkt_queue);
	spin_unlock_irqrestore(&si->scsi_queue_lock, flags);
	rc = lport->tt.fcp_cmd_send(lport, fsp, fc_fcp_recv);
	if (unlikely(rc)) {
		spin_lock_irqsave(&si->scsi_queue_lock, flags);
		fsp->cmd->SCp.ptr = NULL;
		list_del(&fsp->list);
		spin_unlock_irqrestore(&si->scsi_queue_lock, flags);
	}

	return rc;
}

static inline unsigned int get_fsp_rec_tov(struct fc_fcp_pkt *fsp)
{
	struct fc_rport_libfc_priv *rpriv = fsp->rport->dd_data;

	return msecs_to_jiffies(rpriv->e_d_tov) + HZ;
}

static int fc_fcp_cmd_send(struct fc_lport *lport, struct fc_fcp_pkt *fsp,
			   void (*resp)(struct fc_seq *,
					struct fc_frame *fp,
					void *arg))
{
	struct fc_frame *fp;
	struct fc_seq *seq;
	struct fc_rport *rport;
	struct fc_rport_libfc_priv *rpriv;
	const size_t len = sizeof(fsp->cdb_cmd);
	int rc = 0;

	if (fc_fcp_lock_pkt(fsp))
		return 0;

	fp = fc_fcp_frame_alloc(lport, sizeof(fsp->cdb_cmd));
	if (!fp) {
		rc = -1;
		goto unlock;
	}

	memcpy(fc_frame_payload_get(fp, len), &fsp->cdb_cmd, len);
	fr_fsp(fp) = fsp;
	rport = fsp->rport;
	fsp->max_payload = rport->maxframe_size;
	rpriv = rport->dd_data;

	fc_fill_fc_hdr(fp, FC_RCTL_DD_UNSOL_CMD, rport->port_id,
		       rpriv->local_port->port_id, FC_TYPE_FCP,
		       FC_FCTL_REQ, 0);

	seq = lport->tt.exch_seq_send(lport, fp, resp, fc_fcp_pkt_destroy,
				      fsp, 0);
	if (!seq) {
		rc = -1;
		goto unlock;
	}
	fsp->seq_ptr = seq;
	fc_fcp_pkt_hold(fsp);	

	setup_timer(&fsp->timer, fc_fcp_timeout, (unsigned long)fsp);
	if (rpriv->flags & FC_RP_FLAGS_REC_SUPPORTED)
		fc_fcp_timer_set(fsp, get_fsp_rec_tov(fsp));

unlock:
	fc_fcp_unlock_pkt(fsp);
	return rc;
}

static void fc_fcp_error(struct fc_fcp_pkt *fsp, struct fc_frame *fp)
{
	int error = PTR_ERR(fp);

	if (fc_fcp_lock_pkt(fsp))
		return;

	if (error == -FC_EX_CLOSED) {
		fc_fcp_retry_cmd(fsp);
		goto unlock;
	}

	fsp->state &= ~FC_SRB_ABORT_PENDING;
	fsp->status_code = FC_CMD_PLOGO;
	fc_fcp_complete_locked(fsp);
unlock:
	fc_fcp_unlock_pkt(fsp);
}

static int fc_fcp_pkt_abort(struct fc_fcp_pkt *fsp)
{
	int rc = FAILED;
	unsigned long ticks_left;

	if (fc_fcp_send_abort(fsp))
		return FAILED;

	init_completion(&fsp->tm_done);
	fsp->wait_for_comp = 1;

	spin_unlock_bh(&fsp->scsi_pkt_lock);
	ticks_left = wait_for_completion_timeout(&fsp->tm_done,
							FC_SCSI_TM_TOV);
	spin_lock_bh(&fsp->scsi_pkt_lock);
	fsp->wait_for_comp = 0;

	if (!ticks_left) {
		FC_FCP_DBG(fsp, "target abort cmd  failed\n");
	} else if (fsp->state & FC_SRB_ABORTED) {
		FC_FCP_DBG(fsp, "target abort cmd  passed\n");
		rc = SUCCESS;
		fc_fcp_complete_locked(fsp);
	}

	return rc;
}

static void fc_lun_reset_send(unsigned long data)
{
	struct fc_fcp_pkt *fsp = (struct fc_fcp_pkt *)data;
	struct fc_lport *lport = fsp->lp;

	if (lport->tt.fcp_cmd_send(lport, fsp, fc_tm_done)) {
		if (fsp->recov_retry++ >= FC_MAX_RECOV_RETRY)
			return;
		if (fc_fcp_lock_pkt(fsp))
			return;
		setup_timer(&fsp->timer, fc_lun_reset_send, (unsigned long)fsp);
		fc_fcp_timer_set(fsp, get_fsp_rec_tov(fsp));
		fc_fcp_unlock_pkt(fsp);
	}
}

static int fc_lun_reset(struct fc_lport *lport, struct fc_fcp_pkt *fsp,
			unsigned int id, unsigned int lun)
{
	int rc;

	fsp->cdb_cmd.fc_dl = htonl(fsp->data_len);
	fsp->cdb_cmd.fc_tm_flags = FCP_TMF_LUN_RESET;
	int_to_scsilun(lun, &fsp->cdb_cmd.fc_lun);

	fsp->wait_for_comp = 1;
	init_completion(&fsp->tm_done);

	fc_lun_reset_send((unsigned long)fsp);

	rc = wait_for_completion_timeout(&fsp->tm_done, FC_SCSI_TM_TOV);

	spin_lock_bh(&fsp->scsi_pkt_lock);
	fsp->state |= FC_SRB_COMPL;
	spin_unlock_bh(&fsp->scsi_pkt_lock);

	del_timer_sync(&fsp->timer);

	spin_lock_bh(&fsp->scsi_pkt_lock);
	if (fsp->seq_ptr) {
		lport->tt.exch_done(fsp->seq_ptr);
		fsp->seq_ptr = NULL;
	}
	fsp->wait_for_comp = 0;
	spin_unlock_bh(&fsp->scsi_pkt_lock);

	if (!rc) {
		FC_SCSI_DBG(lport, "lun reset failed\n");
		return FAILED;
	}

	
	if (fsp->cdb_status != FCP_TMF_CMPL)
		return FAILED;

	FC_SCSI_DBG(lport, "lun reset to lun %u completed\n", lun);
	fc_fcp_cleanup_each_cmd(lport, id, lun, FC_CMD_ABORTED);
	return SUCCESS;
}

static void fc_tm_done(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fc_fcp_pkt *fsp = arg;
	struct fc_frame_header *fh;

	if (IS_ERR(fp)) {
		return;
	}

	if (fc_fcp_lock_pkt(fsp))
		goto out;

	if (!fsp->seq_ptr || !fsp->wait_for_comp)
		goto out_unlock;

	fh = fc_frame_header_get(fp);
	if (fh->fh_type != FC_TYPE_BLS)
		fc_fcp_resp(fsp, fp);
	fsp->seq_ptr = NULL;
	fsp->lp->tt.exch_done(seq);
out_unlock:
	fc_fcp_unlock_pkt(fsp);
out:
	fc_frame_free(fp);
}

static void fc_fcp_cleanup(struct fc_lport *lport)
{
	fc_fcp_cleanup_each_cmd(lport, -1, -1, FC_ERROR);
}

static void fc_fcp_timeout(unsigned long data)
{
	struct fc_fcp_pkt *fsp = (struct fc_fcp_pkt *)data;
	struct fc_rport *rport = fsp->rport;
	struct fc_rport_libfc_priv *rpriv = rport->dd_data;

	if (fc_fcp_lock_pkt(fsp))
		return;

	if (fsp->cdb_cmd.fc_tm_flags)
		goto unlock;

	fsp->state |= FC_SRB_FCP_PROCESSING_TMO;

	if (rpriv->flags & FC_RP_FLAGS_REC_SUPPORTED)
		fc_fcp_rec(fsp);
	else if (fsp->state & FC_SRB_RCV_STATUS)
		fc_fcp_complete_locked(fsp);
	else
		fc_fcp_recovery(fsp, FC_TIMED_OUT);
	fsp->state &= ~FC_SRB_FCP_PROCESSING_TMO;
unlock:
	fc_fcp_unlock_pkt(fsp);
}

static void fc_fcp_rec(struct fc_fcp_pkt *fsp)
{
	struct fc_lport *lport;
	struct fc_frame *fp;
	struct fc_rport *rport;
	struct fc_rport_libfc_priv *rpriv;

	lport = fsp->lp;
	rport = fsp->rport;
	rpriv = rport->dd_data;
	if (!fsp->seq_ptr || rpriv->rp_state != RPORT_ST_READY) {
		fsp->status_code = FC_HRD_ERROR;
		fsp->io_status = 0;
		fc_fcp_complete_locked(fsp);
		return;
	}

	fp = fc_fcp_frame_alloc(lport, sizeof(struct fc_els_rec));
	if (!fp)
		goto retry;

	fr_seq(fp) = fsp->seq_ptr;
	fc_fill_fc_hdr(fp, FC_RCTL_ELS_REQ, rport->port_id,
		       rpriv->local_port->port_id, FC_TYPE_ELS,
		       FC_FCTL_REQ, 0);
	if (lport->tt.elsct_send(lport, rport->port_id, fp, ELS_REC,
				 fc_fcp_rec_resp, fsp,
				 2 * lport->r_a_tov)) {
		fc_fcp_pkt_hold(fsp);		
		return;
	}
retry:
	if (fsp->recov_retry++ < FC_MAX_RECOV_RETRY)
		fc_fcp_timer_set(fsp, get_fsp_rec_tov(fsp));
	else
		fc_fcp_recovery(fsp, FC_TIMED_OUT);
}

static void fc_fcp_rec_resp(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fc_fcp_pkt *fsp = (struct fc_fcp_pkt *)arg;
	struct fc_els_rec_acc *recp;
	struct fc_els_ls_rjt *rjt;
	u32 e_stat;
	u8 opcode;
	u32 offset;
	enum dma_data_direction data_dir;
	enum fc_rctl r_ctl;
	struct fc_rport_libfc_priv *rpriv;

	if (IS_ERR(fp)) {
		fc_fcp_rec_error(fsp, fp);
		return;
	}

	if (fc_fcp_lock_pkt(fsp))
		goto out;

	fsp->recov_retry = 0;
	opcode = fc_frame_payload_op(fp);
	if (opcode == ELS_LS_RJT) {
		rjt = fc_frame_payload_get(fp, sizeof(*rjt));
		switch (rjt->er_reason) {
		default:
			FC_FCP_DBG(fsp, "device %x unexpected REC reject "
				   "reason %d expl %d\n",
				   fsp->rport->port_id, rjt->er_reason,
				   rjt->er_explan);
			
		case ELS_RJT_UNSUP:
			FC_FCP_DBG(fsp, "device does not support REC\n");
			rpriv = fsp->rport->dd_data;
			rpriv->flags &= ~FC_RP_FLAGS_REC_SUPPORTED;
			break;
		case ELS_RJT_LOGIC:
		case ELS_RJT_UNAB:
			if (rjt->er_explan == ELS_EXPL_OXID_RXID &&
			    fsp->xfer_len == 0) {
				fc_fcp_retry_cmd(fsp);
				break;
			}
			fc_fcp_recovery(fsp, FC_ERROR);
			break;
		}
	} else if (opcode == ELS_LS_ACC) {
		if (fsp->state & FC_SRB_ABORTED)
			goto unlock_out;

		data_dir = fsp->cmd->sc_data_direction;
		recp = fc_frame_payload_get(fp, sizeof(*recp));
		offset = ntohl(recp->reca_fc4value);
		e_stat = ntohl(recp->reca_e_stat);

		if (e_stat & ESB_ST_COMPLETE) {

			if (data_dir == DMA_TO_DEVICE) {
				r_ctl = FC_RCTL_DD_CMD_STATUS;
			} else if (fsp->xfer_contig_end == offset) {
				r_ctl = FC_RCTL_DD_CMD_STATUS;
			} else {
				offset = fsp->xfer_contig_end;
				r_ctl = FC_RCTL_DD_SOL_DATA;
			}
			fc_fcp_srr(fsp, r_ctl, offset);
		} else if (e_stat & ESB_ST_SEQ_INIT) {
			fc_fcp_timer_set(fsp,  get_fsp_rec_tov(fsp));
		} else {

			r_ctl = FC_RCTL_DD_SOL_DATA;
			if (data_dir == DMA_TO_DEVICE) {
				r_ctl = FC_RCTL_DD_CMD_STATUS;
				if (offset < fsp->data_len)
					r_ctl = FC_RCTL_DD_DATA_DESC;
			} else if (offset == fsp->xfer_contig_end) {
				r_ctl = FC_RCTL_DD_CMD_STATUS;
			} else if (fsp->xfer_contig_end < offset) {
				offset = fsp->xfer_contig_end;
			}
			fc_fcp_srr(fsp, r_ctl, offset);
		}
	}
unlock_out:
	fc_fcp_unlock_pkt(fsp);
out:
	fc_fcp_pkt_release(fsp);	
	fc_frame_free(fp);
}

static void fc_fcp_rec_error(struct fc_fcp_pkt *fsp, struct fc_frame *fp)
{
	int error = PTR_ERR(fp);

	if (fc_fcp_lock_pkt(fsp))
		goto out;

	switch (error) {
	case -FC_EX_CLOSED:
		fc_fcp_retry_cmd(fsp);
		break;

	default:
		FC_FCP_DBG(fsp, "REC %p fid %6.6x error unexpected error %d\n",
			   fsp, fsp->rport->port_id, error);
		fsp->status_code = FC_CMD_PLOGO;
		

	case -FC_EX_TIMEOUT:
		FC_FCP_DBG(fsp, "REC fid %6.6x error error %d retry %d/%d\n",
			   fsp->rport->port_id, error, fsp->recov_retry,
			   FC_MAX_RECOV_RETRY);
		if (fsp->recov_retry++ < FC_MAX_RECOV_RETRY)
			fc_fcp_rec(fsp);
		else
			fc_fcp_recovery(fsp, FC_ERROR);
		break;
	}
	fc_fcp_unlock_pkt(fsp);
out:
	fc_fcp_pkt_release(fsp);	
}

static void fc_fcp_recovery(struct fc_fcp_pkt *fsp, u8 code)
{
	fsp->status_code = code;
	fsp->cdb_status = 0;
	fsp->io_status = 0;
	fc_fcp_send_abort(fsp);
}

static void fc_fcp_srr(struct fc_fcp_pkt *fsp, enum fc_rctl r_ctl, u32 offset)
{
	struct fc_lport *lport = fsp->lp;
	struct fc_rport *rport;
	struct fc_rport_libfc_priv *rpriv;
	struct fc_exch *ep = fc_seq_exch(fsp->seq_ptr);
	struct fc_seq *seq;
	struct fcp_srr *srr;
	struct fc_frame *fp;
	unsigned int rec_tov;

	rport = fsp->rport;
	rpriv = rport->dd_data;

	if (!(rpriv->flags & FC_RP_FLAGS_RETRY) ||
	    rpriv->rp_state != RPORT_ST_READY)
		goto retry;			
	fp = fc_fcp_frame_alloc(lport, sizeof(*srr));
	if (!fp)
		goto retry;

	srr = fc_frame_payload_get(fp, sizeof(*srr));
	memset(srr, 0, sizeof(*srr));
	srr->srr_op = ELS_SRR;
	srr->srr_ox_id = htons(ep->oxid);
	srr->srr_rx_id = htons(ep->rxid);
	srr->srr_r_ctl = r_ctl;
	srr->srr_rel_off = htonl(offset);

	fc_fill_fc_hdr(fp, FC_RCTL_ELS4_REQ, rport->port_id,
		       rpriv->local_port->port_id, FC_TYPE_FCP,
		       FC_FCTL_REQ, 0);

	rec_tov = get_fsp_rec_tov(fsp);
	seq = lport->tt.exch_seq_send(lport, fp, fc_fcp_srr_resp,
				      fc_fcp_pkt_destroy,
				      fsp, jiffies_to_msecs(rec_tov));
	if (!seq)
		goto retry;

	fsp->recov_seq = seq;
	fsp->xfer_len = offset;
	fsp->xfer_contig_end = offset;
	fsp->state &= ~FC_SRB_RCV_STATUS;
	fc_fcp_pkt_hold(fsp);		
	return;
retry:
	fc_fcp_retry_cmd(fsp);
}

static void fc_fcp_srr_resp(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fc_fcp_pkt *fsp = arg;
	struct fc_frame_header *fh;

	if (IS_ERR(fp)) {
		fc_fcp_srr_error(fsp, fp);
		return;
	}

	if (fc_fcp_lock_pkt(fsp))
		goto out;

	fh = fc_frame_header_get(fp);
	if (fh->fh_type == FC_TYPE_BLS) {
		fc_fcp_unlock_pkt(fsp);
		return;
	}

	switch (fc_frame_payload_op(fp)) {
	case ELS_LS_ACC:
		fsp->recov_retry = 0;
		fc_fcp_timer_set(fsp, get_fsp_rec_tov(fsp));
		break;
	case ELS_LS_RJT:
	default:
		fc_fcp_recovery(fsp, FC_ERROR);
		break;
	}
	fc_fcp_unlock_pkt(fsp);
out:
	fsp->lp->tt.exch_done(seq);
	fc_frame_free(fp);
}

static void fc_fcp_srr_error(struct fc_fcp_pkt *fsp, struct fc_frame *fp)
{
	if (fc_fcp_lock_pkt(fsp))
		goto out;
	switch (PTR_ERR(fp)) {
	case -FC_EX_TIMEOUT:
		if (fsp->recov_retry++ < FC_MAX_RECOV_RETRY)
			fc_fcp_rec(fsp);
		else
			fc_fcp_recovery(fsp, FC_TIMED_OUT);
		break;
	case -FC_EX_CLOSED:			
		
	default:
		fc_fcp_retry_cmd(fsp);
		break;
	}
	fc_fcp_unlock_pkt(fsp);
out:
	fsp->lp->tt.exch_done(fsp->recov_seq);
}

static inline int fc_fcp_lport_queue_ready(struct fc_lport *lport)
{
	
	return (lport->state == LPORT_ST_READY) &&
		lport->link_up && !lport->qfull;
}

int fc_queuecommand(struct Scsi_Host *shost, struct scsi_cmnd *sc_cmd)
{
	struct fc_lport *lport = shost_priv(shost);
	struct fc_rport *rport = starget_to_rport(scsi_target(sc_cmd->device));
	struct fc_fcp_pkt *fsp;
	struct fc_rport_libfc_priv *rpriv;
	int rval;
	int rc = 0;
	struct fcoe_dev_stats *stats;

	rval = fc_remote_port_chkready(rport);
	if (rval) {
		sc_cmd->result = rval;
		sc_cmd->scsi_done(sc_cmd);
		return 0;
	}

	if (!*(struct fc_remote_port **)rport->dd_data) {
		sc_cmd->result = DID_IMM_RETRY << 16;
		sc_cmd->scsi_done(sc_cmd);
		goto out;
	}

	rpriv = rport->dd_data;

	if (!fc_fcp_lport_queue_ready(lport)) {
		if (lport->qfull)
			fc_fcp_can_queue_ramp_down(lport);
		rc = SCSI_MLQUEUE_HOST_BUSY;
		goto out;
	}

	fsp = fc_fcp_pkt_alloc(lport, GFP_ATOMIC);
	if (fsp == NULL) {
		rc = SCSI_MLQUEUE_HOST_BUSY;
		goto out;
	}

	fsp->cmd = sc_cmd;	
	fsp->rport = rport;	

	fsp->data_len = scsi_bufflen(sc_cmd);
	fsp->xfer_len = 0;

	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
	if (sc_cmd->sc_data_direction == DMA_FROM_DEVICE) {
		fsp->req_flags = FC_SRB_READ;
		stats->InputRequests++;
		stats->InputBytes += fsp->data_len;
	} else if (sc_cmd->sc_data_direction == DMA_TO_DEVICE) {
		fsp->req_flags = FC_SRB_WRITE;
		stats->OutputRequests++;
		stats->OutputBytes += fsp->data_len;
	} else {
		fsp->req_flags = 0;
		stats->ControlRequests++;
	}
	put_cpu();

	rval = fc_fcp_pkt_send(lport, fsp);
	if (rval != 0) {
		fsp->state = FC_SRB_FREE;
		fc_fcp_pkt_release(fsp);
		rc = SCSI_MLQUEUE_HOST_BUSY;
	}
out:
	return rc;
}
EXPORT_SYMBOL(fc_queuecommand);

static void fc_io_compl(struct fc_fcp_pkt *fsp)
{
	struct fc_fcp_internal *si;
	struct scsi_cmnd *sc_cmd;
	struct fc_lport *lport;
	unsigned long flags;

	
	fc_fcp_ddp_done(fsp);

	fsp->state |= FC_SRB_COMPL;
	if (!(fsp->state & FC_SRB_FCP_PROCESSING_TMO)) {
		spin_unlock_bh(&fsp->scsi_pkt_lock);
		del_timer_sync(&fsp->timer);
		spin_lock_bh(&fsp->scsi_pkt_lock);
	}

	lport = fsp->lp;
	si = fc_get_scsi_internal(lport);

	if (si->last_can_queue_ramp_down_time)
		fc_fcp_can_queue_ramp_up(lport);

	sc_cmd = fsp->cmd;
	CMD_SCSI_STATUS(sc_cmd) = fsp->cdb_status;
	switch (fsp->status_code) {
	case FC_COMPLETE:
		if (fsp->cdb_status == 0) {
			sc_cmd->result = DID_OK << 16;
			if (fsp->scsi_resid)
				CMD_RESID_LEN(sc_cmd) = fsp->scsi_resid;
		} else {
			sc_cmd->result = (DID_OK << 16) | fsp->cdb_status;
		}
		break;
	case FC_ERROR:
		FC_FCP_DBG(fsp, "Returning DID_ERROR to scsi-ml "
			   "due to FC_ERROR\n");
		sc_cmd->result = DID_ERROR << 16;
		break;
	case FC_DATA_UNDRUN:
		if ((fsp->cdb_status == 0) && !(fsp->req_flags & FC_SRB_READ)) {
			if (fsp->state & FC_SRB_RCV_STATUS) {
				sc_cmd->result = DID_OK << 16;
			} else {
				FC_FCP_DBG(fsp, "Returning DID_ERROR to scsi-ml"
					   " due to FC_DATA_UNDRUN (trans)\n");
				sc_cmd->result = DID_ERROR << 16;
			}
		} else {
			FC_FCP_DBG(fsp, "Returning DID_ERROR to scsi-ml "
				   "due to FC_DATA_UNDRUN (scsi)\n");
			CMD_RESID_LEN(sc_cmd) = fsp->scsi_resid;
			sc_cmd->result = (DID_ERROR << 16) | fsp->cdb_status;
		}
		break;
	case FC_DATA_OVRRUN:
		FC_FCP_DBG(fsp, "Returning DID_ERROR to scsi-ml "
			   "due to FC_DATA_OVRRUN\n");
		sc_cmd->result = (DID_ERROR << 16) | fsp->cdb_status;
		break;
	case FC_CMD_ABORTED:
		FC_FCP_DBG(fsp, "Returning DID_ERROR to scsi-ml "
			  "due to FC_CMD_ABORTED\n");
		sc_cmd->result = (DID_ERROR << 16) | fsp->io_status;
		break;
	case FC_CMD_RESET:
		FC_FCP_DBG(fsp, "Returning DID_RESET to scsi-ml "
			   "due to FC_CMD_RESET\n");
		sc_cmd->result = (DID_RESET << 16);
		break;
	case FC_HRD_ERROR:
		FC_FCP_DBG(fsp, "Returning DID_NO_CONNECT to scsi-ml "
			   "due to FC_HRD_ERROR\n");
		sc_cmd->result = (DID_NO_CONNECT << 16);
		break;
	case FC_CRC_ERROR:
		FC_FCP_DBG(fsp, "Returning DID_PARITY to scsi-ml "
			   "due to FC_CRC_ERROR\n");
		sc_cmd->result = (DID_PARITY << 16);
		break;
	case FC_TIMED_OUT:
		FC_FCP_DBG(fsp, "Returning DID_BUS_BUSY to scsi-ml "
			   "due to FC_TIMED_OUT\n");
		sc_cmd->result = (DID_BUS_BUSY << 16) | fsp->io_status;
		break;
	default:
		FC_FCP_DBG(fsp, "Returning DID_ERROR to scsi-ml "
			   "due to unknown error\n");
		sc_cmd->result = (DID_ERROR << 16);
		break;
	}

	if (lport->state != LPORT_ST_READY && fsp->status_code != FC_COMPLETE)
		sc_cmd->result = (DID_TRANSPORT_DISRUPTED << 16);

	spin_lock_irqsave(&si->scsi_queue_lock, flags);
	list_del(&fsp->list);
	sc_cmd->SCp.ptr = NULL;
	spin_unlock_irqrestore(&si->scsi_queue_lock, flags);
	sc_cmd->scsi_done(sc_cmd);

	
	fc_fcp_pkt_release(fsp);
}

int fc_eh_abort(struct scsi_cmnd *sc_cmd)
{
	struct fc_fcp_pkt *fsp;
	struct fc_lport *lport;
	struct fc_fcp_internal *si;
	int rc = FAILED;
	unsigned long flags;
	int rval;

	rval = fc_block_scsi_eh(sc_cmd);
	if (rval)
		return rval;

	lport = shost_priv(sc_cmd->device->host);
	if (lport->state != LPORT_ST_READY)
		return rc;
	else if (!lport->link_up)
		return rc;

	si = fc_get_scsi_internal(lport);
	spin_lock_irqsave(&si->scsi_queue_lock, flags);
	fsp = CMD_SP(sc_cmd);
	if (!fsp) {
		
		spin_unlock_irqrestore(&si->scsi_queue_lock, flags);
		return SUCCESS;
	}
	
	fc_fcp_pkt_hold(fsp);
	spin_unlock_irqrestore(&si->scsi_queue_lock, flags);

	if (fc_fcp_lock_pkt(fsp)) {
		
		rc = SUCCESS;
		goto release_pkt;
	}

	rc = fc_fcp_pkt_abort(fsp);
	fc_fcp_unlock_pkt(fsp);

release_pkt:
	fc_fcp_pkt_release(fsp);
	return rc;
}
EXPORT_SYMBOL(fc_eh_abort);

int fc_eh_device_reset(struct scsi_cmnd *sc_cmd)
{
	struct fc_lport *lport;
	struct fc_fcp_pkt *fsp;
	struct fc_rport *rport = starget_to_rport(scsi_target(sc_cmd->device));
	int rc = FAILED;
	int rval;

	rval = fc_block_scsi_eh(sc_cmd);
	if (rval)
		return rval;

	lport = shost_priv(sc_cmd->device->host);

	if (lport->state != LPORT_ST_READY)
		return rc;

	FC_SCSI_DBG(lport, "Resetting rport (%6.6x)\n", rport->port_id);

	fsp = fc_fcp_pkt_alloc(lport, GFP_NOIO);
	if (fsp == NULL) {
		printk(KERN_WARNING "libfc: could not allocate scsi_pkt\n");
		goto out;
	}

	fsp->rport = rport;	

	rc = fc_lun_reset(lport, fsp, scmd_id(sc_cmd), sc_cmd->device->lun);
	fsp->state = FC_SRB_FREE;
	fc_fcp_pkt_release(fsp);

out:
	return rc;
}
EXPORT_SYMBOL(fc_eh_device_reset);

int fc_eh_host_reset(struct scsi_cmnd *sc_cmd)
{
	struct Scsi_Host *shost = sc_cmd->device->host;
	struct fc_lport *lport = shost_priv(shost);
	unsigned long wait_tmo;

	FC_SCSI_DBG(lport, "Resetting host\n");

	fc_block_scsi_eh(sc_cmd);

	lport->tt.lport_reset(lport);
	wait_tmo = jiffies + FC_HOST_RESET_TIMEOUT;
	while (!fc_fcp_lport_queue_ready(lport) && time_before(jiffies,
							       wait_tmo))
		msleep(1000);

	if (fc_fcp_lport_queue_ready(lport)) {
		shost_printk(KERN_INFO, shost, "libfc: Host reset succeeded "
			     "on port (%6.6x)\n", lport->port_id);
		return SUCCESS;
	} else {
		shost_printk(KERN_INFO, shost, "libfc: Host reset failed, "
			     "port (%6.6x) is not ready.\n",
			     lport->port_id);
		return FAILED;
	}
}
EXPORT_SYMBOL(fc_eh_host_reset);

int fc_slave_alloc(struct scsi_device *sdev)
{
	struct fc_rport *rport = starget_to_rport(scsi_target(sdev));

	if (!rport || fc_remote_port_chkready(rport))
		return -ENXIO;

	if (sdev->tagged_supported)
		scsi_activate_tcq(sdev, FC_FCP_DFLT_QUEUE_DEPTH);
	else
		scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev),
					FC_FCP_DFLT_QUEUE_DEPTH);

	return 0;
}
EXPORT_SYMBOL(fc_slave_alloc);

int fc_change_queue_depth(struct scsi_device *sdev, int qdepth, int reason)
{
	switch (reason) {
	case SCSI_QDEPTH_DEFAULT:
		scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
		break;
	case SCSI_QDEPTH_QFULL:
		scsi_track_queue_full(sdev, qdepth);
		break;
	case SCSI_QDEPTH_RAMP_UP:
		scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return sdev->queue_depth;
}
EXPORT_SYMBOL(fc_change_queue_depth);

int fc_change_queue_type(struct scsi_device *sdev, int tag_type)
{
	if (sdev->tagged_supported) {
		scsi_set_tag_type(sdev, tag_type);
		if (tag_type)
			scsi_activate_tcq(sdev, sdev->queue_depth);
		else
			scsi_deactivate_tcq(sdev, sdev->queue_depth);
	} else
		tag_type = 0;

	return tag_type;
}
EXPORT_SYMBOL(fc_change_queue_type);

void fc_fcp_destroy(struct fc_lport *lport)
{
	struct fc_fcp_internal *si = fc_get_scsi_internal(lport);

	if (!list_empty(&si->scsi_pkt_queue))
		printk(KERN_ERR "libfc: Leaked SCSI packets when destroying "
		       "port (%6.6x)\n", lport->port_id);

	mempool_destroy(si->scsi_pkt_pool);
	kfree(si);
	lport->scsi_priv = NULL;
}
EXPORT_SYMBOL(fc_fcp_destroy);

int fc_setup_fcp(void)
{
	int rc = 0;

	scsi_pkt_cachep = kmem_cache_create("libfc_fcp_pkt",
					    sizeof(struct fc_fcp_pkt),
					    0, SLAB_HWCACHE_ALIGN, NULL);
	if (!scsi_pkt_cachep) {
		printk(KERN_ERR "libfc: Unable to allocate SRB cache, "
		       "module load failed!");
		rc = -ENOMEM;
	}

	return rc;
}

void fc_destroy_fcp(void)
{
	if (scsi_pkt_cachep)
		kmem_cache_destroy(scsi_pkt_cachep);
}

int fc_fcp_init(struct fc_lport *lport)
{
	int rc;
	struct fc_fcp_internal *si;

	if (!lport->tt.fcp_cmd_send)
		lport->tt.fcp_cmd_send = fc_fcp_cmd_send;

	if (!lport->tt.fcp_cleanup)
		lport->tt.fcp_cleanup = fc_fcp_cleanup;

	if (!lport->tt.fcp_abort_io)
		lport->tt.fcp_abort_io = fc_fcp_abort_io;

	si = kzalloc(sizeof(struct fc_fcp_internal), GFP_KERNEL);
	if (!si)
		return -ENOMEM;
	lport->scsi_priv = si;
	si->max_can_queue = lport->host->can_queue;
	INIT_LIST_HEAD(&si->scsi_pkt_queue);
	spin_lock_init(&si->scsi_queue_lock);

	si->scsi_pkt_pool = mempool_create_slab_pool(2, scsi_pkt_cachep);
	if (!si->scsi_pkt_pool) {
		rc = -ENOMEM;
		goto free_internal;
	}
	return 0;

free_internal:
	kfree(si);
	return rc;
}
EXPORT_SYMBOL(fc_fcp_init);
