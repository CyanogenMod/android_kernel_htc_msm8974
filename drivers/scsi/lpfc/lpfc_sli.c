/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2012 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 * Portions Copyright (C) 2004-2005 Christoph Hellwig              *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/fc/fc_fs.h>
#include <linux/aer.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vport.h"

typedef enum _lpfc_iocb_type {
	LPFC_UNKNOWN_IOCB,
	LPFC_UNSOL_IOCB,
	LPFC_SOL_IOCB,
	LPFC_ABORT_IOCB
} lpfc_iocb_type;


static int lpfc_sli_issue_mbox_s4(struct lpfc_hba *, LPFC_MBOXQ_t *,
				  uint32_t);
static int lpfc_sli4_read_rev(struct lpfc_hba *, LPFC_MBOXQ_t *,
			      uint8_t *, uint32_t *);
static struct lpfc_iocbq *lpfc_sli4_els_wcqe_to_rspiocbq(struct lpfc_hba *,
							 struct lpfc_iocbq *);
static void lpfc_sli4_send_seq_to_ulp(struct lpfc_vport *,
				      struct hbq_dmabuf *);
static int lpfc_sli4_fp_handle_wcqe(struct lpfc_hba *, struct lpfc_queue *,
				    struct lpfc_cqe *);

static IOCB_t *
lpfc_get_iocb_from_iocbq(struct lpfc_iocbq *iocbq)
{
	return &iocbq->iocb;
}

static uint32_t
lpfc_sli4_wq_put(struct lpfc_queue *q, union lpfc_wqe *wqe)
{
	union lpfc_wqe *temp_wqe;
	struct lpfc_register doorbell;
	uint32_t host_index;

	
	if (unlikely(!q))
		return -ENOMEM;
	temp_wqe = q->qe[q->host_index].wqe;

	
	if (((q->host_index + 1) % q->entry_count) == q->hba_index)
		return -ENOMEM;
	
	if (!((q->host_index + 1) % q->entry_repost))
		bf_set(wqe_wqec, &wqe->generic.wqe_com, 1);
	if (q->phba->sli3_options & LPFC_SLI4_PHWQ_ENABLED)
		bf_set(wqe_wqid, &wqe->generic.wqe_com, q->queue_id);
	lpfc_sli_pcimem_bcopy(wqe, temp_wqe, q->entry_size);

	
	host_index = q->host_index;
	q->host_index = ((q->host_index + 1) % q->entry_count);

	
	doorbell.word0 = 0;
	bf_set(lpfc_wq_doorbell_num_posted, &doorbell, 1);
	bf_set(lpfc_wq_doorbell_index, &doorbell, host_index);
	bf_set(lpfc_wq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.WQDBregaddr);
	readl(q->phba->sli4_hba.WQDBregaddr); 

	return 0;
}

static uint32_t
lpfc_sli4_wq_release(struct lpfc_queue *q, uint32_t index)
{
	uint32_t released = 0;

	
	if (unlikely(!q))
		return 0;

	if (q->hba_index == index)
		return 0;
	do {
		q->hba_index = ((q->hba_index + 1) % q->entry_count);
		released++;
	} while (q->hba_index != index);
	return released;
}

static uint32_t
lpfc_sli4_mq_put(struct lpfc_queue *q, struct lpfc_mqe *mqe)
{
	struct lpfc_mqe *temp_mqe;
	struct lpfc_register doorbell;
	uint32_t host_index;

	
	if (unlikely(!q))
		return -ENOMEM;
	temp_mqe = q->qe[q->host_index].mqe;

	
	if (((q->host_index + 1) % q->entry_count) == q->hba_index)
		return -ENOMEM;
	lpfc_sli_pcimem_bcopy(mqe, temp_mqe, q->entry_size);
	
	q->phba->mbox = (MAILBOX_t *)temp_mqe;

	
	host_index = q->host_index;
	q->host_index = ((q->host_index + 1) % q->entry_count);

	
	doorbell.word0 = 0;
	bf_set(lpfc_mq_doorbell_num_posted, &doorbell, 1);
	bf_set(lpfc_mq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.MQDBregaddr);
	readl(q->phba->sli4_hba.MQDBregaddr); 
	return 0;
}

static uint32_t
lpfc_sli4_mq_release(struct lpfc_queue *q)
{
	
	if (unlikely(!q))
		return 0;

	
	q->phba->mbox = NULL;
	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return 1;
}

static struct lpfc_eqe *
lpfc_sli4_eq_get(struct lpfc_queue *q)
{
	struct lpfc_eqe *eqe;

	
	if (unlikely(!q))
		return NULL;
	eqe = q->qe[q->hba_index].eqe;

	
	if (!bf_get_le32(lpfc_eqe_valid, eqe))
		return NULL;
	
	if (((q->hba_index + 1) % q->entry_count) == q->host_index)
		return NULL;

	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return eqe;
}

uint32_t
lpfc_sli4_eq_release(struct lpfc_queue *q, bool arm)
{
	uint32_t released = 0;
	struct lpfc_eqe *temp_eqe;
	struct lpfc_register doorbell;

	
	if (unlikely(!q))
		return 0;

	
	while (q->hba_index != q->host_index) {
		temp_eqe = q->qe[q->host_index].eqe;
		bf_set_le32(lpfc_eqe_valid, temp_eqe, 0);
		released++;
		q->host_index = ((q->host_index + 1) % q->entry_count);
	}
	if (unlikely(released == 0 && !arm))
		return 0;

	
	doorbell.word0 = 0;
	if (arm) {
		bf_set(lpfc_eqcq_doorbell_arm, &doorbell, 1);
		bf_set(lpfc_eqcq_doorbell_eqci, &doorbell, 1);
	}
	bf_set(lpfc_eqcq_doorbell_num_released, &doorbell, released);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_EVENT);
	bf_set(lpfc_eqcq_doorbell_eqid_hi, &doorbell,
			(q->queue_id >> LPFC_EQID_HI_FIELD_SHIFT));
	bf_set(lpfc_eqcq_doorbell_eqid_lo, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQCQDBregaddr);
	
	if ((q->phba->intr_type == INTx) && (arm == LPFC_QUEUE_REARM))
		readl(q->phba->sli4_hba.EQCQDBregaddr);
	return released;
}

static struct lpfc_cqe *
lpfc_sli4_cq_get(struct lpfc_queue *q)
{
	struct lpfc_cqe *cqe;

	
	if (unlikely(!q))
		return NULL;

	
	if (!bf_get_le32(lpfc_cqe_valid, q->qe[q->hba_index].cqe))
		return NULL;
	
	if (((q->hba_index + 1) % q->entry_count) == q->host_index)
		return NULL;

	cqe = q->qe[q->hba_index].cqe;
	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return cqe;
}

uint32_t
lpfc_sli4_cq_release(struct lpfc_queue *q, bool arm)
{
	uint32_t released = 0;
	struct lpfc_cqe *temp_qe;
	struct lpfc_register doorbell;

	
	if (unlikely(!q))
		return 0;
	
	while (q->hba_index != q->host_index) {
		temp_qe = q->qe[q->host_index].cqe;
		bf_set_le32(lpfc_cqe_valid, temp_qe, 0);
		released++;
		q->host_index = ((q->host_index + 1) % q->entry_count);
	}
	if (unlikely(released == 0 && !arm))
		return 0;

	
	doorbell.word0 = 0;
	if (arm)
		bf_set(lpfc_eqcq_doorbell_arm, &doorbell, 1);
	bf_set(lpfc_eqcq_doorbell_num_released, &doorbell, released);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_COMPLETION);
	bf_set(lpfc_eqcq_doorbell_cqid_hi, &doorbell,
			(q->queue_id >> LPFC_CQID_HI_FIELD_SHIFT));
	bf_set(lpfc_eqcq_doorbell_cqid_lo, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQCQDBregaddr);
	return released;
}

static int
lpfc_sli4_rq_put(struct lpfc_queue *hq, struct lpfc_queue *dq,
		 struct lpfc_rqe *hrqe, struct lpfc_rqe *drqe)
{
	struct lpfc_rqe *temp_hrqe;
	struct lpfc_rqe *temp_drqe;
	struct lpfc_register doorbell;
	int put_index = hq->host_index;

	
	if (unlikely(!hq) || unlikely(!dq))
		return -ENOMEM;
	temp_hrqe = hq->qe[hq->host_index].rqe;
	temp_drqe = dq->qe[dq->host_index].rqe;

	if (hq->type != LPFC_HRQ || dq->type != LPFC_DRQ)
		return -EINVAL;
	if (hq->host_index != dq->host_index)
		return -EINVAL;
	
	if (((hq->host_index + 1) % hq->entry_count) == hq->hba_index)
		return -EBUSY;
	lpfc_sli_pcimem_bcopy(hrqe, temp_hrqe, hq->entry_size);
	lpfc_sli_pcimem_bcopy(drqe, temp_drqe, dq->entry_size);

	
	hq->host_index = ((hq->host_index + 1) % hq->entry_count);
	dq->host_index = ((dq->host_index + 1) % dq->entry_count);

	
	if (!(hq->host_index % hq->entry_repost)) {
		doorbell.word0 = 0;
		bf_set(lpfc_rq_doorbell_num_posted, &doorbell,
		       hq->entry_repost);
		bf_set(lpfc_rq_doorbell_id, &doorbell, hq->queue_id);
		writel(doorbell.word0, hq->phba->sli4_hba.RQDBregaddr);
	}
	return put_index;
}

static uint32_t
lpfc_sli4_rq_release(struct lpfc_queue *hq, struct lpfc_queue *dq)
{
	
	if (unlikely(!hq) || unlikely(!dq))
		return 0;

	if ((hq->type != LPFC_HRQ) || (dq->type != LPFC_DRQ))
		return 0;
	hq->hba_index = ((hq->hba_index + 1) % hq->entry_count);
	dq->hba_index = ((dq->hba_index + 1) % dq->entry_count);
	return 1;
}

static inline IOCB_t *
lpfc_cmd_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->cmdringaddr) +
			   pring->cmdidx * phba->iocb_cmd_size);
}

static inline IOCB_t *
lpfc_resp_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->rspringaddr) +
			   pring->rspidx * phba->iocb_rsp_size);
}

static struct lpfc_iocbq *
__lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct list_head *lpfc_iocb_list = &phba->lpfc_iocb_list;
	struct lpfc_iocbq * iocbq = NULL;

	list_remove_head(lpfc_iocb_list, iocbq, struct lpfc_iocbq, list);
	if (iocbq)
		phba->iocb_cnt++;
	if (phba->iocb_cnt > phba->iocb_max)
		phba->iocb_max = phba->iocb_cnt;
	return iocbq;
}

static struct lpfc_sglq *
__lpfc_clear_active_sglq(struct lpfc_hba *phba, uint16_t xritag)
{
	struct lpfc_sglq *sglq;

	sglq = phba->sli4_hba.lpfc_sglq_active_list[xritag];
	phba->sli4_hba.lpfc_sglq_active_list[xritag] = NULL;
	return sglq;
}

struct lpfc_sglq *
__lpfc_get_active_sglq(struct lpfc_hba *phba, uint16_t xritag)
{
	struct lpfc_sglq *sglq;

	sglq =  phba->sli4_hba.lpfc_sglq_active_list[xritag];
	return sglq;
}

void
lpfc_clr_rrq_active(struct lpfc_hba *phba,
		    uint16_t xritag,
		    struct lpfc_node_rrq *rrq)
{
	struct lpfc_nodelist *ndlp = NULL;

	if ((rrq->vport) && NLP_CHK_NODE_ACT(rrq->ndlp))
		ndlp = lpfc_findnode_did(rrq->vport, rrq->nlp_DID);

	if ((!ndlp) && rrq->ndlp)
		ndlp = rrq->ndlp;

	if (!ndlp)
		goto out;

	if (test_and_clear_bit(xritag, ndlp->active_rrqs.xri_bitmap)) {
		rrq->send_rrq = 0;
		rrq->xritag = 0;
		rrq->rrq_stop_time = 0;
	}
out:
	mempool_free(rrq, phba->rrq_pool);
}

void
lpfc_handle_rrq_active(struct lpfc_hba *phba)
{
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long next_time;
	unsigned long iflags;
	LIST_HEAD(send_rrq);

	spin_lock_irqsave(&phba->hbalock, iflags);
	phba->hba_flag &= ~HBA_RRQ_ACTIVE;
	next_time = jiffies + HZ * (phba->fc_ratov + 1);
	list_for_each_entry_safe(rrq, nextrrq,
				 &phba->active_rrq_list, list) {
		if (time_after(jiffies, rrq->rrq_stop_time))
			list_move(&rrq->list, &send_rrq);
		else if (time_before(rrq->rrq_stop_time, next_time))
			next_time = rrq->rrq_stop_time;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if (!list_empty(&phba->active_rrq_list))
		mod_timer(&phba->rrq_tmr, next_time);
	list_for_each_entry_safe(rrq, nextrrq, &send_rrq, list) {
		list_del(&rrq->list);
		if (!rrq->send_rrq)
			
		lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
		else if (lpfc_send_rrq(phba, rrq)) {
			lpfc_clr_rrq_active(phba, rrq->xritag,
					    rrq);
		}
	}
}

struct lpfc_node_rrq *
lpfc_get_active_rrq(struct lpfc_vport *vport, uint16_t xri, uint32_t did)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long iflags;

	if (phba->sli_rev != LPFC_SLI_REV4)
		return NULL;
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_for_each_entry_safe(rrq, nextrrq, &phba->active_rrq_list, list) {
		if (rrq->vport == vport && rrq->xritag == xri &&
				rrq->nlp_DID == did){
			list_del(&rrq->list);
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			return rrq;
		}
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return NULL;
}

void
lpfc_cleanup_vports_rrqs(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)

{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long iflags;
	LIST_HEAD(rrq_list);

	if (phba->sli_rev != LPFC_SLI_REV4)
		return;
	if (!ndlp) {
		lpfc_sli4_vport_delete_els_xri_aborted(vport);
		lpfc_sli4_vport_delete_fcp_xri_aborted(vport);
	}
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_for_each_entry_safe(rrq, nextrrq, &phba->active_rrq_list, list)
		if ((rrq->vport == vport) && (!ndlp  || rrq->ndlp == ndlp))
			list_move(&rrq->list, &rrq_list);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	list_for_each_entry_safe(rrq, nextrrq, &rrq_list, list) {
		list_del(&rrq->list);
		lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
	}
}

void
lpfc_cleanup_wt_rrqs(struct lpfc_hba *phba)
{
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long next_time;
	unsigned long iflags;
	LIST_HEAD(rrq_list);

	if (phba->sli_rev != LPFC_SLI_REV4)
		return;
	spin_lock_irqsave(&phba->hbalock, iflags);
	phba->hba_flag &= ~HBA_RRQ_ACTIVE;
	next_time = jiffies + HZ * (phba->fc_ratov * 2);
	list_splice_init(&phba->active_rrq_list, &rrq_list);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	list_for_each_entry_safe(rrq, nextrrq, &rrq_list, list) {
		list_del(&rrq->list);
		lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
	}
	if (!list_empty(&phba->active_rrq_list))
		mod_timer(&phba->rrq_tmr, next_time);
}


int
lpfc_test_rrq_active(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp,
			uint16_t  xritag)
{
	if (!ndlp)
		return 0;
	if (test_bit(xritag, ndlp->active_rrqs.xri_bitmap))
			return 1;
	else
		return 0;
}

int
lpfc_set_rrq_active(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp,
		    uint16_t xritag, uint16_t rxid, uint16_t send_rrq)
{
	unsigned long iflags;
	struct lpfc_node_rrq *rrq;
	int empty;

	if (!ndlp)
		return -EINVAL;

	if (!phba->cfg_enable_rrq)
		return -EINVAL;

	spin_lock_irqsave(&phba->hbalock, iflags);
	if (phba->pport->load_flag & FC_UNLOADING) {
		phba->hba_flag &= ~HBA_RRQ_ACTIVE;
		goto out;
	}

	if (NLP_CHK_FREE_REQ(ndlp))
		goto out;

	if (ndlp->vport && (ndlp->vport->load_flag & FC_UNLOADING))
		goto out;

	if (test_and_set_bit(xritag, ndlp->active_rrqs.xri_bitmap))
		goto out;

	spin_unlock_irqrestore(&phba->hbalock, iflags);
	rrq = mempool_alloc(phba->rrq_pool, GFP_KERNEL);
	if (!rrq) {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3155 Unable to allocate RRQ xri:0x%x rxid:0x%x"
				" DID:0x%x Send:%d\n",
				xritag, rxid, ndlp->nlp_DID, send_rrq);
		return -EINVAL;
	}
	rrq->send_rrq = send_rrq;
	rrq->xritag = xritag;
	rrq->rrq_stop_time = jiffies + HZ * (phba->fc_ratov + 1);
	rrq->ndlp = ndlp;
	rrq->nlp_DID = ndlp->nlp_DID;
	rrq->vport = ndlp->vport;
	rrq->rxid = rxid;
	rrq->send_rrq = send_rrq;
	spin_lock_irqsave(&phba->hbalock, iflags);
	empty = list_empty(&phba->active_rrq_list);
	list_add_tail(&rrq->list, &phba->active_rrq_list);
	phba->hba_flag |= HBA_RRQ_ACTIVE;
	if (empty)
		lpfc_worker_wake_up(phba);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return 0;
out:
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2921 Can't set rrq active xri:0x%x rxid:0x%x"
			" DID:0x%x Send:%d\n",
			xritag, rxid, ndlp->nlp_DID, send_rrq);
	return -EINVAL;
}

static struct lpfc_sglq *
__lpfc_sli_get_sglq(struct lpfc_hba *phba, struct lpfc_iocbq *piocbq)
{
	struct list_head *lpfc_sgl_list = &phba->sli4_hba.lpfc_sgl_list;
	struct lpfc_sglq *sglq = NULL;
	struct lpfc_sglq *start_sglq = NULL;
	struct lpfc_scsi_buf *lpfc_cmd;
	struct lpfc_nodelist *ndlp;
	int found = 0;

	if (piocbq->iocb_flag &  LPFC_IO_FCP) {
		lpfc_cmd = (struct lpfc_scsi_buf *) piocbq->context1;
		ndlp = lpfc_cmd->rdata->pnode;
	} else  if ((piocbq->iocb.ulpCommand == CMD_GEN_REQUEST64_CR) &&
			!(piocbq->iocb_flag & LPFC_IO_LIBDFC))
		ndlp = piocbq->context_un.ndlp;
	else
		ndlp = piocbq->context1;

	list_remove_head(lpfc_sgl_list, sglq, struct lpfc_sglq, list);
	start_sglq = sglq;
	while (!found) {
		if (!sglq)
			return NULL;
		if (lpfc_test_rrq_active(phba, ndlp, sglq->sli4_xritag)) {
			list_add_tail(&sglq->list, lpfc_sgl_list);
			sglq = NULL;
			list_remove_head(lpfc_sgl_list, sglq,
						struct lpfc_sglq, list);
			if (sglq == start_sglq) {
				sglq = NULL;
				break;
			} else
				continue;
		}
		sglq->ndlp = ndlp;
		found = 1;
		phba->sli4_hba.lpfc_sglq_active_list[sglq->sli4_lxritag] = sglq;
		sglq->state = SGL_ALLOCATED;
	}
	return sglq;
}

struct lpfc_iocbq *
lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct lpfc_iocbq * iocbq = NULL;
	unsigned long iflags;

	spin_lock_irqsave(&phba->hbalock, iflags);
	iocbq = __lpfc_sli_get_iocbq(phba);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return iocbq;
}

static void
__lpfc_sli_release_iocbq_s4(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_sglq *sglq;
	size_t start_clean = offsetof(struct lpfc_iocbq, iocb);
	unsigned long iflag = 0;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];

	if (iocbq->sli4_xritag == NO_XRI)
		sglq = NULL;
	else
		sglq = __lpfc_clear_active_sglq(phba, iocbq->sli4_lxritag);

	if (sglq)  {
		if ((iocbq->iocb_flag & LPFC_EXCHANGE_BUSY) &&
			(sglq->state != SGL_XRI_ABORTED)) {
			spin_lock_irqsave(&phba->sli4_hba.abts_sgl_list_lock,
					iflag);
			list_add(&sglq->list,
				&phba->sli4_hba.lpfc_abts_els_sgl_list);
			spin_unlock_irqrestore(
				&phba->sli4_hba.abts_sgl_list_lock, iflag);
		} else {
			sglq->state = SGL_FREED;
			sglq->ndlp = NULL;
			list_add_tail(&sglq->list,
				&phba->sli4_hba.lpfc_sgl_list);

			
			if (pring->txq_cnt)
				lpfc_worker_wake_up(phba);
		}
	}


	memset((char *)iocbq + start_clean, 0, sizeof(*iocbq) - start_clean);
	iocbq->sli4_lxritag = NO_XRI;
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(&iocbq->list, &phba->lpfc_iocb_list);
}


static void
__lpfc_sli_release_iocbq_s3(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	size_t start_clean = offsetof(struct lpfc_iocbq, iocb);

	memset((char*)iocbq + start_clean, 0, sizeof(*iocbq) - start_clean);
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(&iocbq->list, &phba->lpfc_iocb_list);
}

static void
__lpfc_sli_release_iocbq(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	phba->__lpfc_sli_release_iocbq(phba, iocbq);
	phba->iocb_cnt--;
}

void
lpfc_sli_release_iocbq(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	unsigned long iflags;

	spin_lock_irqsave(&phba->hbalock, iflags);
	__lpfc_sli_release_iocbq(phba, iocbq);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
}

void
lpfc_sli_cancel_iocbs(struct lpfc_hba *phba, struct list_head *iocblist,
		      uint32_t ulpstatus, uint32_t ulpWord4)
{
	struct lpfc_iocbq *piocb;

	while (!list_empty(iocblist)) {
		list_remove_head(iocblist, piocb, struct lpfc_iocbq, list);

		if (!piocb->iocb_cmpl)
			lpfc_sli_release_iocbq(phba, piocb);
		else {
			piocb->iocb.ulpStatus = ulpstatus;
			piocb->iocb.un.ulpWord[4] = ulpWord4;
			(piocb->iocb_cmpl) (phba, piocb, piocb);
		}
	}
	return;
}

static lpfc_iocb_type
lpfc_sli_iocb_cmd_type(uint8_t iocb_cmnd)
{
	lpfc_iocb_type type = LPFC_UNKNOWN_IOCB;

	if (iocb_cmnd > CMD_MAX_IOCB_CMD)
		return 0;

	switch (iocb_cmnd) {
	case CMD_XMIT_SEQUENCE_CR:
	case CMD_XMIT_SEQUENCE_CX:
	case CMD_XMIT_BCAST_CN:
	case CMD_XMIT_BCAST_CX:
	case CMD_ELS_REQUEST_CR:
	case CMD_ELS_REQUEST_CX:
	case CMD_CREATE_XRI_CR:
	case CMD_CREATE_XRI_CX:
	case CMD_GET_RPI_CN:
	case CMD_XMIT_ELS_RSP_CX:
	case CMD_GET_RPI_CR:
	case CMD_FCP_IWRITE_CR:
	case CMD_FCP_IWRITE_CX:
	case CMD_FCP_IREAD_CR:
	case CMD_FCP_IREAD_CX:
	case CMD_FCP_ICMND_CR:
	case CMD_FCP_ICMND_CX:
	case CMD_FCP_TSEND_CX:
	case CMD_FCP_TRSP_CX:
	case CMD_FCP_TRECEIVE_CX:
	case CMD_FCP_AUTO_TRSP_CX:
	case CMD_ADAPTER_MSG:
	case CMD_ADAPTER_DUMP:
	case CMD_XMIT_SEQUENCE64_CR:
	case CMD_XMIT_SEQUENCE64_CX:
	case CMD_XMIT_BCAST64_CN:
	case CMD_XMIT_BCAST64_CX:
	case CMD_ELS_REQUEST64_CR:
	case CMD_ELS_REQUEST64_CX:
	case CMD_FCP_IWRITE64_CR:
	case CMD_FCP_IWRITE64_CX:
	case CMD_FCP_IREAD64_CR:
	case CMD_FCP_IREAD64_CX:
	case CMD_FCP_ICMND64_CR:
	case CMD_FCP_ICMND64_CX:
	case CMD_FCP_TSEND64_CX:
	case CMD_FCP_TRSP64_CX:
	case CMD_FCP_TRECEIVE64_CX:
	case CMD_GEN_REQUEST64_CR:
	case CMD_GEN_REQUEST64_CX:
	case CMD_XMIT_ELS_RSP64_CX:
	case DSSCMD_IWRITE64_CR:
	case DSSCMD_IWRITE64_CX:
	case DSSCMD_IREAD64_CR:
	case DSSCMD_IREAD64_CX:
		type = LPFC_SOL_IOCB;
		break;
	case CMD_ABORT_XRI_CN:
	case CMD_ABORT_XRI_CX:
	case CMD_CLOSE_XRI_CN:
	case CMD_CLOSE_XRI_CX:
	case CMD_XRI_ABORTED_CX:
	case CMD_ABORT_MXRI64_CN:
	case CMD_XMIT_BLS_RSP64_CX:
		type = LPFC_ABORT_IOCB;
		break;
	case CMD_RCV_SEQUENCE_CX:
	case CMD_RCV_ELS_REQ_CX:
	case CMD_RCV_SEQUENCE64_CX:
	case CMD_RCV_ELS_REQ64_CX:
	case CMD_ASYNC_STATUS:
	case CMD_IOCB_RCV_SEQ64_CX:
	case CMD_IOCB_RCV_ELS64_CX:
	case CMD_IOCB_RCV_CONT64_CX:
	case CMD_IOCB_RET_XRI64_CX:
		type = LPFC_UNSOL_IOCB;
		break;
	case CMD_IOCB_XMIT_MSEQ64_CR:
	case CMD_IOCB_XMIT_MSEQ64_CX:
	case CMD_IOCB_RCV_SEQ_LIST64_CX:
	case CMD_IOCB_RCV_ELS_LIST64_CX:
	case CMD_IOCB_CLOSE_EXTENDED_CN:
	case CMD_IOCB_ABORT_EXTENDED_CN:
	case CMD_IOCB_RET_HBQE64_CN:
	case CMD_IOCB_FCP_IBIDIR64_CR:
	case CMD_IOCB_FCP_IBIDIR64_CX:
	case CMD_IOCB_FCP_ITASKMGT64_CX:
	case CMD_IOCB_LOGENTRY_CN:
	case CMD_IOCB_LOGENTRY_ASYNC_CN:
		printk("%s - Unhandled SLI-3 Command x%x\n",
				__func__, iocb_cmnd);
		type = LPFC_UNKNOWN_IOCB;
		break;
	default:
		type = LPFC_UNKNOWN_IOCB;
		break;
	}

	return type;
}

static int
lpfc_sli_ring_map(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *pmbox;
	int i, rc, ret = 0;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb)
		return -ENOMEM;
	pmbox = &pmb->u.mb;
	phba->link_state = LPFC_INIT_MBX_CMDS;
	for (i = 0; i < psli->num_rings; i++) {
		lpfc_config_ring(phba, i, pmb);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0446 Adapter failed to init (%d), "
					"mbxCmd x%x CFG_RING, mbxStatus x%x, "
					"ring %d\n",
					rc, pmbox->mbxCommand,
					pmbox->mbxStatus, i);
			phba->link_state = LPFC_HBA_ERROR;
			ret = -ENXIO;
			break;
		}
	}
	mempool_free(pmb, phba->mbox_mem_pool);
	return ret;
}

static int
lpfc_sli_ringtxcmpl_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			struct lpfc_iocbq *piocb)
{
	list_add_tail(&piocb->list, &pring->txcmplq);
	piocb->iocb_flag |= LPFC_IO_ON_Q;
	pring->txcmplq_cnt++;
	if (pring->txcmplq_cnt > pring->txcmplq_max)
		pring->txcmplq_max = pring->txcmplq_cnt;

	if ((unlikely(pring->ringno == LPFC_ELS_RING)) &&
	   (piocb->iocb.ulpCommand != CMD_ABORT_XRI_CN) &&
	   (piocb->iocb.ulpCommand != CMD_CLOSE_XRI_CN)) {
		if (!piocb->vport)
			BUG();
		else
			mod_timer(&piocb->vport->els_tmofunc,
				  jiffies + HZ * (phba->fc_ratov << 1));
	}


	return 0;
}

struct lpfc_iocbq *
lpfc_sli_ringtx_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_iocbq *cmd_iocb;

	list_remove_head((&pring->txq), cmd_iocb, struct lpfc_iocbq, list);
	if (cmd_iocb != NULL)
		pring->txq_cnt--;
	return cmd_iocb;
}

static IOCB_t *
lpfc_sli_next_iocb_slot (struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	uint32_t  max_cmd_idx = pring->numCiocb;
	if ((pring->next_cmdidx == pring->cmdidx) &&
	   (++pring->next_cmdidx >= max_cmd_idx))
		pring->next_cmdidx = 0;

	if (unlikely(pring->local_getidx == pring->next_cmdidx)) {

		pring->local_getidx = le32_to_cpu(pgp->cmdGetInx);

		if (unlikely(pring->local_getidx >= max_cmd_idx)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0315 Ring %d issue: portCmdGet %d "
					"is bigger than cmd ring %d\n",
					pring->ringno,
					pring->local_getidx, max_cmd_idx);

			phba->link_state = LPFC_HBA_ERROR;
			phba->work_ha |= HA_ERATT;
			phba->work_hs = HS_FFER3;

			lpfc_worker_wake_up(phba);

			return NULL;
		}

		if (pring->local_getidx == pring->next_cmdidx)
			return NULL;
	}

	return lpfc_cmd_iocb(phba, pring);
}

uint16_t
lpfc_sli_next_iotag(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_iocbq **new_arr;
	struct lpfc_iocbq **old_arr;
	size_t new_len;
	struct lpfc_sli *psli = &phba->sli;
	uint16_t iotag;

	spin_lock_irq(&phba->hbalock);
	iotag = psli->last_iotag;
	if(++iotag < psli->iocbq_lookup_len) {
		psli->last_iotag = iotag;
		psli->iocbq_lookup[iotag] = iocbq;
		spin_unlock_irq(&phba->hbalock);
		iocbq->iotag = iotag;
		return iotag;
	} else if (psli->iocbq_lookup_len < (0xffff
					   - LPFC_IOCBQ_LOOKUP_INCREMENT)) {
		new_len = psli->iocbq_lookup_len + LPFC_IOCBQ_LOOKUP_INCREMENT;
		spin_unlock_irq(&phba->hbalock);
		new_arr = kzalloc(new_len * sizeof (struct lpfc_iocbq *),
				  GFP_KERNEL);
		if (new_arr) {
			spin_lock_irq(&phba->hbalock);
			old_arr = psli->iocbq_lookup;
			if (new_len <= psli->iocbq_lookup_len) {
				
				kfree(new_arr);
				iotag = psli->last_iotag;
				if(++iotag < psli->iocbq_lookup_len) {
					psli->last_iotag = iotag;
					psli->iocbq_lookup[iotag] = iocbq;
					spin_unlock_irq(&phba->hbalock);
					iocbq->iotag = iotag;
					return iotag;
				}
				spin_unlock_irq(&phba->hbalock);
				return 0;
			}
			if (psli->iocbq_lookup)
				memcpy(new_arr, old_arr,
				       ((psli->last_iotag  + 1) *
					sizeof (struct lpfc_iocbq *)));
			psli->iocbq_lookup = new_arr;
			psli->iocbq_lookup_len = new_len;
			psli->last_iotag = iotag;
			psli->iocbq_lookup[iotag] = iocbq;
			spin_unlock_irq(&phba->hbalock);
			iocbq->iotag = iotag;
			kfree(old_arr);
			return iotag;
		}
	} else
		spin_unlock_irq(&phba->hbalock);

	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"0318 Failed to allocate IOTAG.last IOTAG is %d\n",
			psli->last_iotag);

	return 0;
}

static void
lpfc_sli_submit_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		IOCB_t *iocb, struct lpfc_iocbq *nextiocb)
{
	nextiocb->iocb.ulpIoTag = (nextiocb->iocb_cmpl) ? nextiocb->iotag : 0;


	if (pring->ringno == LPFC_ELS_RING) {
		lpfc_debugfs_slow_ring_trc(phba,
			"IOCB cmd ring:   wd4:x%08x wd6:x%08x wd7:x%08x",
			*(((uint32_t *) &nextiocb->iocb) + 4),
			*(((uint32_t *) &nextiocb->iocb) + 6),
			*(((uint32_t *) &nextiocb->iocb) + 7));
	}

	lpfc_sli_pcimem_bcopy(&nextiocb->iocb, iocb, phba->iocb_cmd_size);
	wmb();
	pring->stats.iocb_cmd++;

	if (nextiocb->iocb_cmpl)
		lpfc_sli_ringtxcmpl_put(phba, pring, nextiocb);
	else
		__lpfc_sli_release_iocbq(phba, nextiocb);

	pring->cmdidx = pring->next_cmdidx;
	writel(pring->cmdidx, &phba->host_gp[pring->ringno].cmdPutInx);
}

static void
lpfc_sli_update_full_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	pring->flag |= LPFC_CALL_RING_AVAILABLE;

	wmb();

	writel((CA_R0ATT|CA_R0CE_REQ) << (ringno*4), phba->CAregaddr);
	readl(phba->CAregaddr); 

	pring->stats.iocb_cmd_full++;
}

static void
lpfc_sli_update_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	if (!(phba->sli3_options & LPFC_SLI3_CRP_ENABLED)) {
		wmb();
		writel(CA_R0ATT << (ringno * 4), phba->CAregaddr);
		readl(phba->CAregaddr); 
	}
}

static void
lpfc_sli_resume_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	IOCB_t *iocb;
	struct lpfc_iocbq *nextiocb;

	if (pring->txq_cnt &&
	    lpfc_is_link_up(phba) &&
	    (pring->ringno != phba->sli.fcp_ring ||
	     phba->sli.sli_flag & LPFC_PROCESS_LA)) {

		while ((iocb = lpfc_sli_next_iocb_slot(phba, pring)) &&
		       (nextiocb = lpfc_sli_ringtx_get(phba, pring)))
			lpfc_sli_submit_iocb(phba, pring, iocb, nextiocb);

		if (iocb)
			lpfc_sli_update_ring(phba, pring);
		else
			lpfc_sli_update_full_ring(phba, pring);
	}

	return;
}

static struct lpfc_hbq_entry *
lpfc_sli_next_hbq_slot(struct lpfc_hba *phba, uint32_t hbqno)
{
	struct hbq_s *hbqp = &phba->hbqs[hbqno];

	if (hbqp->next_hbqPutIdx == hbqp->hbqPutIdx &&
	    ++hbqp->next_hbqPutIdx >= hbqp->entry_count)
		hbqp->next_hbqPutIdx = 0;

	if (unlikely(hbqp->local_hbqGetIdx == hbqp->next_hbqPutIdx)) {
		uint32_t raw_index = phba->hbq_get[hbqno];
		uint32_t getidx = le32_to_cpu(raw_index);

		hbqp->local_hbqGetIdx = getidx;

		if (unlikely(hbqp->local_hbqGetIdx >= hbqp->entry_count)) {
			lpfc_printf_log(phba, KERN_ERR,
					LOG_SLI | LOG_VPORT,
					"1802 HBQ %d: local_hbqGetIdx "
					"%u is > than hbqp->entry_count %u\n",
					hbqno, hbqp->local_hbqGetIdx,
					hbqp->entry_count);

			phba->link_state = LPFC_HBA_ERROR;
			return NULL;
		}

		if (hbqp->local_hbqGetIdx == hbqp->next_hbqPutIdx)
			return NULL;
	}

	return (struct lpfc_hbq_entry *) phba->hbqs[hbqno].hbq_virt +
			hbqp->hbqPutIdx;
}

void
lpfc_sli_hbqbuf_free_all(struct lpfc_hba *phba)
{
	struct lpfc_dmabuf *dmabuf, *next_dmabuf;
	struct hbq_dmabuf *hbq_buf;
	unsigned long flags;
	int i, hbq_count;
	uint32_t hbqno;

	hbq_count = lpfc_sli_hbq_count();
	
	spin_lock_irqsave(&phba->hbalock, flags);
	for (i = 0; i < hbq_count; ++i) {
		list_for_each_entry_safe(dmabuf, next_dmabuf,
				&phba->hbqs[i].hbq_buffer_list, list) {
			hbq_buf = container_of(dmabuf, struct hbq_dmabuf, dbuf);
			list_del(&hbq_buf->dbuf.list);
			(phba->hbqs[i].hbq_free_buffer)(phba, hbq_buf);
		}
		phba->hbqs[i].buffer_count = 0;
	}
	
	list_for_each_entry_safe(dmabuf, next_dmabuf, &phba->rb_pend_list,
				 list) {
		hbq_buf = container_of(dmabuf, struct hbq_dmabuf, dbuf);
		list_del(&hbq_buf->dbuf.list);
		if (hbq_buf->tag == -1) {
			(phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer)
				(phba, hbq_buf);
		} else {
			hbqno = hbq_buf->tag >> 16;
			if (hbqno >= LPFC_MAX_HBQS)
				(phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer)
					(phba, hbq_buf);
			else
				(phba->hbqs[hbqno].hbq_free_buffer)(phba,
					hbq_buf);
		}
	}

	
	phba->hbq_in_use = 0;
	spin_unlock_irqrestore(&phba->hbalock, flags);
}

static int
lpfc_sli_hbq_to_firmware(struct lpfc_hba *phba, uint32_t hbqno,
			 struct hbq_dmabuf *hbq_buf)
{
	return phba->lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buf);
}

static int
lpfc_sli_hbq_to_firmware_s3(struct lpfc_hba *phba, uint32_t hbqno,
			    struct hbq_dmabuf *hbq_buf)
{
	struct lpfc_hbq_entry *hbqe;
	dma_addr_t physaddr = hbq_buf->dbuf.phys;

	
	hbqe = lpfc_sli_next_hbq_slot(phba, hbqno);
	if (hbqe) {
		struct hbq_s *hbqp = &phba->hbqs[hbqno];

		hbqe->bde.addrHigh = le32_to_cpu(putPaddrHigh(physaddr));
		hbqe->bde.addrLow  = le32_to_cpu(putPaddrLow(physaddr));
		hbqe->bde.tus.f.bdeSize = hbq_buf->size;
		hbqe->bde.tus.f.bdeFlags = 0;
		hbqe->bde.tus.w = le32_to_cpu(hbqe->bde.tus.w);
		hbqe->buffer_tag = le32_to_cpu(hbq_buf->tag);
				
		hbqp->hbqPutIdx = hbqp->next_hbqPutIdx;
		writel(hbqp->hbqPutIdx, phba->hbq_put + hbqno);
				
		readl(phba->hbq_put + hbqno);
		list_add_tail(&hbq_buf->dbuf.list, &hbqp->hbq_buffer_list);
		return 0;
	} else
		return -ENOMEM;
}

static int
lpfc_sli_hbq_to_firmware_s4(struct lpfc_hba *phba, uint32_t hbqno,
			    struct hbq_dmabuf *hbq_buf)
{
	int rc;
	struct lpfc_rqe hrqe;
	struct lpfc_rqe drqe;

	hrqe.address_lo = putPaddrLow(hbq_buf->hbuf.phys);
	hrqe.address_hi = putPaddrHigh(hbq_buf->hbuf.phys);
	drqe.address_lo = putPaddrLow(hbq_buf->dbuf.phys);
	drqe.address_hi = putPaddrHigh(hbq_buf->dbuf.phys);
	rc = lpfc_sli4_rq_put(phba->sli4_hba.hdr_rq, phba->sli4_hba.dat_rq,
			      &hrqe, &drqe);
	if (rc < 0)
		return rc;
	hbq_buf->tag = rc;
	list_add_tail(&hbq_buf->dbuf.list, &phba->hbqs[hbqno].hbq_buffer_list);
	return 0;
}

static struct lpfc_hbq_init lpfc_els_hbq = {
	.rn = 1,
	.entry_count = 256,
	.mask_count = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_ELS_RING),
	.buffer_count = 0,
	.init_count = 40,
	.add_count = 40,
};

static struct lpfc_hbq_init lpfc_extra_hbq = {
	.rn = 1,
	.entry_count = 200,
	.mask_count = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_EXTRA_RING),
	.buffer_count = 0,
	.init_count = 0,
	.add_count = 5,
};

struct lpfc_hbq_init *lpfc_hbq_defs[] = {
	&lpfc_els_hbq,
	&lpfc_extra_hbq,
};

static int
lpfc_sli_hbqbuf_fill_hbqs(struct lpfc_hba *phba, uint32_t hbqno, uint32_t count)
{
	uint32_t i, posted = 0;
	unsigned long flags;
	struct hbq_dmabuf *hbq_buffer;
	LIST_HEAD(hbq_buf_list);
	if (!phba->hbqs[hbqno].hbq_alloc_buffer)
		return 0;

	if ((phba->hbqs[hbqno].buffer_count + count) >
	    lpfc_hbq_defs[hbqno]->entry_count)
		count = lpfc_hbq_defs[hbqno]->entry_count -
					phba->hbqs[hbqno].buffer_count;
	if (!count)
		return 0;
	
	for (i = 0; i < count; i++) {
		hbq_buffer = (phba->hbqs[hbqno].hbq_alloc_buffer)(phba);
		if (!hbq_buffer)
			break;
		list_add_tail(&hbq_buffer->dbuf.list, &hbq_buf_list);
	}
	
	spin_lock_irqsave(&phba->hbalock, flags);
	if (!phba->hbq_in_use)
		goto err;
	while (!list_empty(&hbq_buf_list)) {
		list_remove_head(&hbq_buf_list, hbq_buffer, struct hbq_dmabuf,
				 dbuf.list);
		hbq_buffer->tag = (phba->hbqs[hbqno].buffer_count |
				      (hbqno << 16));
		if (!lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buffer)) {
			phba->hbqs[hbqno].buffer_count++;
			posted++;
		} else
			(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return posted;
err:
	spin_unlock_irqrestore(&phba->hbalock, flags);
	while (!list_empty(&hbq_buf_list)) {
		list_remove_head(&hbq_buf_list, hbq_buffer, struct hbq_dmabuf,
				 dbuf.list);
		(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
	return 0;
}

int
lpfc_sli_hbqbuf_add_hbqs(struct lpfc_hba *phba, uint32_t qno)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return 0;
	else
		return lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					 lpfc_hbq_defs[qno]->add_count);
}

static int
lpfc_sli_hbqbuf_init_hbqs(struct lpfc_hba *phba, uint32_t qno)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					lpfc_hbq_defs[qno]->entry_count);
	else
		return lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					 lpfc_hbq_defs[qno]->init_count);
}

static struct hbq_dmabuf *
lpfc_sli_hbqbuf_get(struct list_head *rb_list)
{
	struct lpfc_dmabuf *d_buf;

	list_remove_head(rb_list, d_buf, struct lpfc_dmabuf, list);
	if (!d_buf)
		return NULL;
	return container_of(d_buf, struct hbq_dmabuf, dbuf);
}

static struct hbq_dmabuf *
lpfc_sli_hbqbuf_find(struct lpfc_hba *phba, uint32_t tag)
{
	struct lpfc_dmabuf *d_buf;
	struct hbq_dmabuf *hbq_buf;
	uint32_t hbqno;

	hbqno = tag >> 16;
	if (hbqno >= LPFC_MAX_HBQS)
		return NULL;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(d_buf, &phba->hbqs[hbqno].hbq_buffer_list, list) {
		hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		if (hbq_buf->tag == tag) {
			spin_unlock_irq(&phba->hbalock);
			return hbq_buf;
		}
	}
	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI | LOG_VPORT,
			"1803 Bad hbq tag. Data: x%x x%x\n",
			tag, phba->hbqs[tag >> 16].buffer_count);
	return NULL;
}

void
lpfc_sli_free_hbq(struct lpfc_hba *phba, struct hbq_dmabuf *hbq_buffer)
{
	uint32_t hbqno;

	if (hbq_buffer) {
		hbqno = hbq_buffer->tag >> 16;
		if (lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buffer))
			(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
}

static int
lpfc_sli_chk_mbx_command(uint8_t mbxCommand)
{
	uint8_t ret;

	switch (mbxCommand) {
	case MBX_LOAD_SM:
	case MBX_READ_NV:
	case MBX_WRITE_NV:
	case MBX_WRITE_VPARMS:
	case MBX_RUN_BIU_DIAG:
	case MBX_INIT_LINK:
	case MBX_DOWN_LINK:
	case MBX_CONFIG_LINK:
	case MBX_CONFIG_RING:
	case MBX_RESET_RING:
	case MBX_READ_CONFIG:
	case MBX_READ_RCONFIG:
	case MBX_READ_SPARM:
	case MBX_READ_STATUS:
	case MBX_READ_RPI:
	case MBX_READ_XRI:
	case MBX_READ_REV:
	case MBX_READ_LNK_STAT:
	case MBX_REG_LOGIN:
	case MBX_UNREG_LOGIN:
	case MBX_CLEAR_LA:
	case MBX_DUMP_MEMORY:
	case MBX_DUMP_CONTEXT:
	case MBX_RUN_DIAGS:
	case MBX_RESTART:
	case MBX_UPDATE_CFG:
	case MBX_DOWN_LOAD:
	case MBX_DEL_LD_ENTRY:
	case MBX_RUN_PROGRAM:
	case MBX_SET_MASK:
	case MBX_SET_VARIABLE:
	case MBX_UNREG_D_ID:
	case MBX_KILL_BOARD:
	case MBX_CONFIG_FARP:
	case MBX_BEACON:
	case MBX_LOAD_AREA:
	case MBX_RUN_BIU_DIAG64:
	case MBX_CONFIG_PORT:
	case MBX_READ_SPARM64:
	case MBX_READ_RPI64:
	case MBX_REG_LOGIN64:
	case MBX_READ_TOPOLOGY:
	case MBX_WRITE_WWN:
	case MBX_SET_DEBUG:
	case MBX_LOAD_EXP_ROM:
	case MBX_ASYNCEVT_ENABLE:
	case MBX_REG_VPI:
	case MBX_UNREG_VPI:
	case MBX_HEARTBEAT:
	case MBX_PORT_CAPABILITIES:
	case MBX_PORT_IOV_CONTROL:
	case MBX_SLI4_CONFIG:
	case MBX_SLI4_REQ_FTRS:
	case MBX_REG_FCFI:
	case MBX_UNREG_FCFI:
	case MBX_REG_VFI:
	case MBX_UNREG_VFI:
	case MBX_INIT_VPI:
	case MBX_INIT_VFI:
	case MBX_RESUME_RPI:
	case MBX_READ_EVENT_LOG_STATUS:
	case MBX_READ_EVENT_LOG:
	case MBX_SECURITY_MGMT:
	case MBX_AUTH_PORT:
		ret = mbxCommand;
		break;
	default:
		ret = MBX_SHUTDOWN;
		break;
	}
	return ret;
}

void
lpfc_sli_wake_mbox_wait(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq)
{
	wait_queue_head_t *pdone_q;
	unsigned long drvr_flag;

	pmboxq->mbox_flag |= LPFC_MBX_WAKE;
	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	pdone_q = (wait_queue_head_t *) pmboxq->context1;
	if (pdone_q)
		wake_up_interruptible(pdone_q);
	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
	return;
}


void
lpfc_sli_def_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport  *vport = pmb->vport;
	struct lpfc_dmabuf *mp;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost;
	uint16_t rpi, vpi;
	int rc;

	mp = (struct lpfc_dmabuf *) (pmb->context1);

	if (mp) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
	}

	if (!(phba->pport->load_flag & FC_UNLOADING) &&
	    pmb->u.mb.mbxCommand == MBX_REG_LOGIN64 &&
	    !pmb->u.mb.mbxStatus) {
		rpi = pmb->u.mb.un.varWords[0];
		vpi = pmb->u.mb.un.varRegLogin.vpi;
		lpfc_unreg_login(phba, vpi, rpi, pmb);
		pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if (rc != MBX_NOT_FINISHED)
			return;
	}

	if ((pmb->u.mb.mbxCommand == MBX_REG_VPI) &&
		!(phba->pport->load_flag & FC_UNLOADING) &&
		!pmb->u.mb.mbxStatus) {
		shost = lpfc_shost_from_vport(vport);
		spin_lock_irq(shost->host_lock);
		vport->vpi_state |= LPFC_VPI_REGISTERED;
		vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
		spin_unlock_irq(shost->host_lock);
	}

	if (pmb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
		ndlp = (struct lpfc_nodelist *)pmb->context2;
		lpfc_nlp_put(ndlp);
		pmb->context2 = NULL;
	}

	
	if ((pmb->u.mb.mbxCommand == MBX_INIT_LINK) &&
	    (pmb->u.mb.mbxStatus == MBXERR_SEC_NO_PERMISSION))
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"2860 SLI authentication is required "
				"for INIT_LINK but has not done yet\n");

	if (bf_get(lpfc_mqe_command, &pmb->u.mqe) == MBX_SLI4_CONFIG)
		lpfc_sli4_mbox_cmd_free(phba, pmb);
	else
		mempool_free(pmb, phba->mbox_mem_pool);
}

int
lpfc_sli_handle_mb_event(struct lpfc_hba *phba)
{
	MAILBOX_t *pmbox;
	LPFC_MBOXQ_t *pmb;
	int rc;
	LIST_HEAD(cmplq);

	phba->sli.slistat.mbox_event++;

	
	spin_lock_irq(&phba->hbalock);
	list_splice_init(&phba->sli.mboxq_cmpl, &cmplq);
	spin_unlock_irq(&phba->hbalock);

	
	do {
		list_remove_head(&cmplq, pmb, LPFC_MBOXQ_t, list);
		if (pmb == NULL)
			break;

		pmbox = &pmb->u.mb;

		if (pmbox->mbxCommand != MBX_HEARTBEAT) {
			if (pmb->vport) {
				lpfc_debugfs_disc_trc(pmb->vport,
					LPFC_DISC_TRC_MBOX_VPORT,
					"MBOX cmpl vport: cmd:x%x mb:x%x x%x",
					(uint32_t)pmbox->mbxCommand,
					pmbox->un.varWords[0],
					pmbox->un.varWords[1]);
			}
			else {
				lpfc_debugfs_disc_trc(phba->pport,
					LPFC_DISC_TRC_MBOX,
					"MBOX cmpl:       cmd:x%x mb:x%x x%x",
					(uint32_t)pmbox->mbxCommand,
					pmbox->un.varWords[0],
					pmbox->un.varWords[1]);
			}
		}

		if (lpfc_sli_chk_mbx_command(pmbox->mbxCommand) ==
		    MBX_SHUTDOWN) {
			
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"(%d):0323 Unknown Mailbox command "
					"x%x (x%x/x%x) Cmpl\n",
					pmb->vport ? pmb->vport->vpi : 0,
					pmbox->mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									pmb),
					lpfc_sli_config_mbox_opcode_get(phba,
									pmb));
			phba->link_state = LPFC_HBA_ERROR;
			phba->work_hs = HS_FFER3;
			lpfc_handle_eratt(phba);
			continue;
		}

		if (pmbox->mbxStatus) {
			phba->sli.slistat.mbox_stat_err++;
			if (pmbox->mbxStatus == MBXERR_NO_RESOURCES) {
				
				lpfc_printf_log(phba, KERN_INFO,
					LOG_MBOX | LOG_SLI,
					"(%d):0305 Mbox cmd cmpl "
					"error - RETRYing Data: x%x "
					"(x%x/x%x) x%x x%x x%x\n",
					pmb->vport ? pmb->vport->vpi : 0,
					pmbox->mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									pmb),
					lpfc_sli_config_mbox_opcode_get(phba,
									pmb),
					pmbox->mbxStatus,
					pmbox->un.varWords[0],
					pmb->vport->port_state);
				pmbox->mbxStatus = 0;
				pmbox->mbxOwner = OWN_HOST;
				rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
				if (rc != MBX_NOT_FINISHED)
					continue;
			}
		}

		
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"(%d):0307 Mailbox cmd x%x (x%x/x%x) Cmpl x%p "
				"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x\n",
				pmb->vport ? pmb->vport->vpi : 0,
				pmbox->mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, pmb),
				lpfc_sli_config_mbox_opcode_get(phba, pmb),
				pmb->mbox_cmpl,
				*((uint32_t *) pmbox),
				pmbox->un.varWords[0],
				pmbox->un.varWords[1],
				pmbox->un.varWords[2],
				pmbox->un.varWords[3],
				pmbox->un.varWords[4],
				pmbox->un.varWords[5],
				pmbox->un.varWords[6],
				pmbox->un.varWords[7]);

		if (pmb->mbox_cmpl)
			pmb->mbox_cmpl(phba,pmb);
	} while (1);
	return 0;
}

static struct lpfc_dmabuf *
lpfc_sli_get_buff(struct lpfc_hba *phba,
		  struct lpfc_sli_ring *pring,
		  uint32_t tag)
{
	struct hbq_dmabuf *hbq_entry;

	if (tag & QUE_BUFTAG_BIT)
		return lpfc_sli_ring_taggedbuf_get(phba, pring, tag);
	hbq_entry = lpfc_sli_hbqbuf_find(phba, tag);
	if (!hbq_entry)
		return NULL;
	return &hbq_entry->dbuf;
}

static int
lpfc_complete_unsol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 struct lpfc_iocbq *saveq, uint32_t fch_r_ctl,
			 uint32_t fch_type)
{
	int i;

	
	if (pring->prt[0].profile) {
		if (pring->prt[0].lpfc_sli_rcv_unsol_event)
			(pring->prt[0].lpfc_sli_rcv_unsol_event) (phba, pring,
									saveq);
		return 1;
	}
	for (i = 0; i < pring->num_mask; i++) {
		if ((pring->prt[i].rctl == fch_r_ctl) &&
		    (pring->prt[i].type == fch_type)) {
			if (pring->prt[i].lpfc_sli_rcv_unsol_event)
				(pring->prt[i].lpfc_sli_rcv_unsol_event)
						(phba, pring, saveq);
			return 1;
		}
	}
	return 0;
}

static int
lpfc_sli_process_unsol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			    struct lpfc_iocbq *saveq)
{
	IOCB_t           * irsp;
	WORD5            * w5p;
	uint32_t           Rctl, Type;
	uint32_t           match;
	struct lpfc_iocbq *iocbq;
	struct lpfc_dmabuf *dmzbuf;

	match = 0;
	irsp = &(saveq->iocb);

	if (irsp->ulpCommand == CMD_ASYNC_STATUS) {
		if (pring->lpfc_sli_rcv_async_status)
			pring->lpfc_sli_rcv_async_status(phba, pring, saveq);
		else
			lpfc_printf_log(phba,
					KERN_WARNING,
					LOG_SLI,
					"0316 Ring %d handler: unexpected "
					"ASYNC_STATUS iocb received evt_code "
					"0x%x\n",
					pring->ringno,
					irsp->un.asyncstat.evt_code);
		return 1;
	}

	if ((irsp->ulpCommand == CMD_IOCB_RET_XRI64_CX) &&
		(phba->sli3_options & LPFC_SLI3_HBQ_ENABLED)) {
		if (irsp->ulpBdeCount > 0) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
					irsp->un.ulpWord[3]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		if (irsp->ulpBdeCount > 1) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
					irsp->unsli3.sli3Words[3]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		if (irsp->ulpBdeCount > 2) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
				irsp->unsli3.sli3Words[7]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		return 1;
	}

	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		if (irsp->ulpBdeCount != 0) {
			saveq->context2 = lpfc_sli_get_buff(phba, pring,
						irsp->un.ulpWord[3]);
			if (!saveq->context2)
				lpfc_printf_log(phba,
					KERN_ERR,
					LOG_SLI,
					"0341 Ring %d Cannot find buffer for "
					"an unsolicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->un.ulpWord[3]);
		}
		if (irsp->ulpBdeCount == 2) {
			saveq->context3 = lpfc_sli_get_buff(phba, pring,
						irsp->unsli3.sli3Words[7]);
			if (!saveq->context3)
				lpfc_printf_log(phba,
					KERN_ERR,
					LOG_SLI,
					"0342 Ring %d Cannot find buffer for an"
					" unsolicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->unsli3.sli3Words[7]);
		}
		list_for_each_entry(iocbq, &saveq->list, list) {
			irsp = &(iocbq->iocb);
			if (irsp->ulpBdeCount != 0) {
				iocbq->context2 = lpfc_sli_get_buff(phba, pring,
							irsp->un.ulpWord[3]);
				if (!iocbq->context2)
					lpfc_printf_log(phba,
						KERN_ERR,
						LOG_SLI,
						"0343 Ring %d Cannot find "
						"buffer for an unsolicited iocb"
						". tag 0x%x\n", pring->ringno,
						irsp->un.ulpWord[3]);
			}
			if (irsp->ulpBdeCount == 2) {
				iocbq->context3 = lpfc_sli_get_buff(phba, pring,
						irsp->unsli3.sli3Words[7]);
				if (!iocbq->context3)
					lpfc_printf_log(phba,
						KERN_ERR,
						LOG_SLI,
						"0344 Ring %d Cannot find "
						"buffer for an unsolicited "
						"iocb. tag 0x%x\n",
						pring->ringno,
						irsp->unsli3.sli3Words[7]);
			}
		}
	}
	if (irsp->ulpBdeCount != 0 &&
	    (irsp->ulpCommand == CMD_IOCB_RCV_CONT64_CX ||
	     irsp->ulpStatus == IOSTAT_INTERMED_RSP)) {
		int found = 0;

		
		list_for_each_entry(iocbq, &pring->iocb_continue_saveq, clist) {
			if (iocbq->iocb.unsli3.rcvsli3.ox_id ==
				saveq->iocb.unsli3.rcvsli3.ox_id) {
				list_add_tail(&saveq->list, &iocbq->list);
				found = 1;
				break;
			}
		}
		if (!found)
			list_add_tail(&saveq->clist,
				      &pring->iocb_continue_saveq);
		if (saveq->iocb.ulpStatus != IOSTAT_INTERMED_RSP) {
			list_del_init(&iocbq->clist);
			saveq = iocbq;
			irsp = &(saveq->iocb);
		} else
			return 0;
	}
	if ((irsp->ulpCommand == CMD_RCV_ELS_REQ64_CX) ||
	    (irsp->ulpCommand == CMD_RCV_ELS_REQ_CX) ||
	    (irsp->ulpCommand == CMD_IOCB_RCV_ELS64_CX)) {
		Rctl = FC_RCTL_ELS_REQ;
		Type = FC_TYPE_ELS;
	} else {
		w5p = (WORD5 *)&(saveq->iocb.un.ulpWord[5]);
		Rctl = w5p->hcsw.Rctl;
		Type = w5p->hcsw.Type;

		
		if ((Rctl == 0) && (pring->ringno == LPFC_ELS_RING) &&
			(irsp->ulpCommand == CMD_RCV_SEQUENCE64_CX ||
			 irsp->ulpCommand == CMD_IOCB_RCV_SEQ64_CX)) {
			Rctl = FC_RCTL_ELS_REQ;
			Type = FC_TYPE_ELS;
			w5p->hcsw.Rctl = Rctl;
			w5p->hcsw.Type = Type;
		}
	}

	if (!lpfc_complete_unsol_iocb(phba, pring, saveq, Rctl, Type))
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0313 Ring %d handler: unexpected Rctl x%x "
				"Type x%x received\n",
				pring->ringno, Rctl, Type);

	return 1;
}

static struct lpfc_iocbq *
lpfc_sli_iocbq_lookup(struct lpfc_hba *phba,
		      struct lpfc_sli_ring *pring,
		      struct lpfc_iocbq *prspiocb)
{
	struct lpfc_iocbq *cmd_iocb = NULL;
	uint16_t iotag;

	iotag = prspiocb->iocb.ulpIoTag;

	if (iotag != 0 && iotag <= phba->sli.last_iotag) {
		cmd_iocb = phba->sli.iocbq_lookup[iotag];
		list_del_init(&cmd_iocb->list);
		if (cmd_iocb->iocb_flag & LPFC_IO_ON_Q) {
			pring->txcmplq_cnt--;
			cmd_iocb->iocb_flag &= ~LPFC_IO_ON_Q;
		}
		return cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0317 iotag x%x is out off "
			"range: max iotag x%x wd0 x%x\n",
			iotag, phba->sli.last_iotag,
			*(((uint32_t *) &prspiocb->iocb) + 7));
	return NULL;
}

static struct lpfc_iocbq *
lpfc_sli_iocbq_lookup_by_tag(struct lpfc_hba *phba,
			     struct lpfc_sli_ring *pring, uint16_t iotag)
{
	struct lpfc_iocbq *cmd_iocb;

	if (iotag != 0 && iotag <= phba->sli.last_iotag) {
		cmd_iocb = phba->sli.iocbq_lookup[iotag];
		list_del_init(&cmd_iocb->list);
		if (cmd_iocb->iocb_flag & LPFC_IO_ON_Q) {
			cmd_iocb->iocb_flag &= ~LPFC_IO_ON_Q;
			pring->txcmplq_cnt--;
		}
		return cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0372 iotag x%x is out off range: max iotag (x%x)\n",
			iotag, phba->sli.last_iotag);
	return NULL;
}

static int
lpfc_sli_process_sol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			  struct lpfc_iocbq *saveq)
{
	struct lpfc_iocbq *cmdiocbp;
	int rc = 1;
	unsigned long iflag;

	
	spin_lock_irqsave(&phba->hbalock, iflag);
	cmdiocbp = lpfc_sli_iocbq_lookup(phba, pring, saveq);
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	if (cmdiocbp) {
		if (cmdiocbp->iocb_cmpl) {
			if (saveq->iocb.ulpStatus &&
			     (pring->ringno == LPFC_ELS_RING) &&
			     (cmdiocbp->iocb.ulpCommand ==
				CMD_ELS_REQUEST64_CR))
				lpfc_send_els_failure_event(phba,
					cmdiocbp, saveq);

			if (pring->ringno == LPFC_ELS_RING) {
				if ((phba->sli_rev < LPFC_SLI_REV4) &&
				    (cmdiocbp->iocb_flag &
							LPFC_DRIVER_ABORTED)) {
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
					cmdiocbp->iocb_flag &=
						~LPFC_DRIVER_ABORTED;
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
					saveq->iocb.ulpStatus =
						IOSTAT_LOCAL_REJECT;
					saveq->iocb.un.ulpWord[4] =
						IOERR_SLI_ABORTED;

					spin_lock_irqsave(&phba->hbalock,
							  iflag);
					saveq->iocb_flag |= LPFC_DELAY_MEM_FREE;
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
				}
				if (phba->sli_rev == LPFC_SLI_REV4) {
					if (saveq->iocb_flag &
					    LPFC_EXCHANGE_BUSY) {
						spin_lock_irqsave(
							&phba->hbalock, iflag);
						cmdiocbp->iocb_flag |=
							LPFC_EXCHANGE_BUSY;
						spin_unlock_irqrestore(
							&phba->hbalock, iflag);
					}
					if (cmdiocbp->iocb_flag &
					    LPFC_DRIVER_ABORTED) {
						spin_lock_irqsave(
							&phba->hbalock, iflag);
						cmdiocbp->iocb_flag &=
							~LPFC_DRIVER_ABORTED;
						spin_unlock_irqrestore(
							&phba->hbalock, iflag);
						cmdiocbp->iocb.ulpStatus =
							IOSTAT_LOCAL_REJECT;
						cmdiocbp->iocb.un.ulpWord[4] =
							IOERR_ABORT_REQUESTED;
						saveq->iocb.ulpStatus =
							IOSTAT_LOCAL_REJECT;
						saveq->iocb.un.ulpWord[4] =
							IOERR_SLI_ABORTED;
						spin_lock_irqsave(
							&phba->hbalock, iflag);
						saveq->iocb_flag |=
							LPFC_DELAY_MEM_FREE;
						spin_unlock_irqrestore(
							&phba->hbalock, iflag);
					}
				}
			}
			(cmdiocbp->iocb_cmpl) (phba, cmdiocbp, saveq);
		} else
			lpfc_sli_release_iocbq(phba, cmdiocbp);
	} else {
		if (pring->ringno != LPFC_ELS_RING) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					 "0322 Ring %d handler: "
					 "unexpected completion IoTag x%x "
					 "Data: x%x x%x x%x x%x\n",
					 pring->ringno,
					 saveq->iocb.ulpIoTag,
					 saveq->iocb.ulpStatus,
					 saveq->iocb.un.ulpWord[4],
					 saveq->iocb.ulpCommand,
					 saveq->iocb.ulpContext);
		}
	}

	return rc;
}

static void
lpfc_sli_rsp_pointers_error(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0312 Ring %d handler: portRspPut %d "
			"is bigger than rsp ring %d\n",
			pring->ringno, le32_to_cpu(pgp->rspPutInx),
			pring->numRiocb);

	phba->link_state = LPFC_HBA_ERROR;

	phba->work_ha |= HA_ERATT;
	phba->work_hs = HS_FFER3;

	lpfc_worker_wake_up(phba);

	return;
}

void lpfc_poll_eratt(unsigned long ptr)
{
	struct lpfc_hba *phba;
	uint32_t eratt = 0;

	phba = (struct lpfc_hba *)ptr;

	
	eratt = lpfc_sli_check_eratt(phba);

	if (eratt)
		
		lpfc_worker_wake_up(phba);
	else
		
		mod_timer(&phba->eratt_poll, jiffies +
					HZ * LPFC_ERATT_POLL_INTERVAL);
	return;
}


int
lpfc_sli_handle_fast_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	IOCB_t *irsp = NULL;
	IOCB_t *entry = NULL;
	struct lpfc_iocbq *cmdiocbq = NULL;
	struct lpfc_iocbq rspiocbq;
	uint32_t status;
	uint32_t portRspPut, portRspMax;
	int rc = 1;
	lpfc_iocb_type type;
	unsigned long iflag;
	uint32_t rsp_cmpl = 0;

	spin_lock_irqsave(&phba->hbalock, iflag);
	pring->stats.iocb_event++;

	portRspMax = pring->numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (unlikely(portRspPut >= portRspMax)) {
		lpfc_sli_rsp_pointers_error(phba, pring);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return 1;
	}
	if (phba->fcp_ring_in_use) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return 1;
	} else
		phba->fcp_ring_in_use = 1;

	rmb();
	while (pring->rspidx != portRspPut) {
		entry = lpfc_resp_iocb(phba, pring);
		phba->last_completion_time = jiffies;

		if (++pring->rspidx >= portRspMax)
			pring->rspidx = 0;

		lpfc_sli_pcimem_bcopy((uint32_t *) entry,
				      (uint32_t *) &rspiocbq.iocb,
				      phba->iocb_rsp_size);
		INIT_LIST_HEAD(&(rspiocbq.list));
		irsp = &rspiocbq.iocb;

		type = lpfc_sli_iocb_cmd_type(irsp->ulpCommand & CMD_IOCB_MASK);
		pring->stats.iocb_rsp++;
		rsp_cmpl++;

		if (unlikely(irsp->ulpStatus)) {
			if ((irsp->ulpStatus == IOSTAT_LOCAL_REJECT) &&
				(irsp->un.ulpWord[4] == IOERR_NO_RESOURCES)) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				phba->lpfc_rampdown_queue_depth(phba);
				spin_lock_irqsave(&phba->hbalock, iflag);
			}

			
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0336 Rsp Ring %d error: IOCB Data: "
					"x%x x%x x%x x%x x%x x%x x%x x%x\n",
					pring->ringno,
					irsp->un.ulpWord[0],
					irsp->un.ulpWord[1],
					irsp->un.ulpWord[2],
					irsp->un.ulpWord[3],
					irsp->un.ulpWord[4],
					irsp->un.ulpWord[5],
					*(uint32_t *)&irsp->un1,
					*((uint32_t *)&irsp->un1 + 1));
		}

		switch (type) {
		case LPFC_ABORT_IOCB:
		case LPFC_SOL_IOCB:
			if (unlikely(irsp->ulpCommand == CMD_XRI_ABORTED_CX)) {
				lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
						"0333 IOCB cmd 0x%x"
						" processed. Skipping"
						" completion\n",
						irsp->ulpCommand);
				break;
			}

			cmdiocbq = lpfc_sli_iocbq_lookup(phba, pring,
							 &rspiocbq);
			if (unlikely(!cmdiocbq))
				break;
			if (cmdiocbq->iocb_flag & LPFC_DRIVER_ABORTED)
				cmdiocbq->iocb_flag &= ~LPFC_DRIVER_ABORTED;
			if (cmdiocbq->iocb_cmpl) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				(cmdiocbq->iocb_cmpl)(phba, cmdiocbq,
						      &rspiocbq);
				spin_lock_irqsave(&phba->hbalock, iflag);
			}
			break;
		case LPFC_UNSOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			lpfc_sli_process_unsol_iocb(phba, pring, &rspiocbq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			break;
		default:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adaptermsg[LPFC_MAX_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
				memcpy(&adaptermsg[0], (uint8_t *) irsp,
				       MAX_MSG_DATA);
				dev_warn(&((phba->pcidev)->dev),
					 "lpfc%d: %s\n",
					 phba->brd_no, adaptermsg);
			} else {
				
				lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
						"0334 Unknown IOCB command "
						"Data: x%x, x%x x%x x%x x%x\n",
						type, irsp->ulpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		writel(pring->rspidx, &phba->host_gp[pring->ringno].rspGetInx);

		if (pring->rspidx == portRspPut)
			portRspPut = le32_to_cpu(pgp->rspPutInx);
	}

	if ((rsp_cmpl > 0) && (mask & HA_R0RE_REQ)) {
		pring->stats.iocb_rsp_full++;
		status = ((CA_R0ATT | CA_R0RE_RSP) << (pring->ringno * 4));
		writel(status, phba->CAregaddr);
		readl(phba->CAregaddr);
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & LPFC_CALL_RING_AVAILABLE)) {
		pring->flag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		
		pring->local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sli_resume_iocb(phba, pring);

		if ((pring->lpfc_sli_cmd_available))
			(pring->lpfc_sli_cmd_available) (phba, pring);

	}

	phba->fcp_ring_in_use = 0;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rc;
}

static struct lpfc_iocbq *
lpfc_sli_sp_handle_rspiocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			struct lpfc_iocbq *rspiocbp)
{
	struct lpfc_iocbq *saveq;
	struct lpfc_iocbq *cmdiocbp;
	struct lpfc_iocbq *next_iocb;
	IOCB_t *irsp = NULL;
	uint32_t free_saveq;
	uint8_t iocb_cmd_type;
	lpfc_iocb_type type;
	unsigned long iflag;
	int rc;

	spin_lock_irqsave(&phba->hbalock, iflag);
	
	list_add_tail(&rspiocbp->list, &(pring->iocb_continueq));
	pring->iocb_continueq_cnt++;

	
	irsp = &rspiocbp->iocb;
	if (irsp->ulpLe) {
		free_saveq = 1;
		saveq = list_get_first(&pring->iocb_continueq,
				       struct lpfc_iocbq, list);
		irsp = &(saveq->iocb);
		list_del_init(&pring->iocb_continueq);
		pring->iocb_continueq_cnt = 0;

		pring->stats.iocb_rsp++;

		if ((irsp->ulpStatus == IOSTAT_LOCAL_REJECT) &&
		    (irsp->un.ulpWord[4] == IOERR_NO_RESOURCES)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			phba->lpfc_rampdown_queue_depth(phba);
			spin_lock_irqsave(&phba->hbalock, iflag);
		}

		if (irsp->ulpStatus) {
			
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0328 Rsp Ring %d error: "
					"IOCB Data: "
					"x%x x%x x%x x%x "
					"x%x x%x x%x x%x "
					"x%x x%x x%x x%x "
					"x%x x%x x%x x%x\n",
					pring->ringno,
					irsp->un.ulpWord[0],
					irsp->un.ulpWord[1],
					irsp->un.ulpWord[2],
					irsp->un.ulpWord[3],
					irsp->un.ulpWord[4],
					irsp->un.ulpWord[5],
					*(((uint32_t *) irsp) + 6),
					*(((uint32_t *) irsp) + 7),
					*(((uint32_t *) irsp) + 8),
					*(((uint32_t *) irsp) + 9),
					*(((uint32_t *) irsp) + 10),
					*(((uint32_t *) irsp) + 11),
					*(((uint32_t *) irsp) + 12),
					*(((uint32_t *) irsp) + 13),
					*(((uint32_t *) irsp) + 14),
					*(((uint32_t *) irsp) + 15));
		}

		iocb_cmd_type = irsp->ulpCommand & CMD_IOCB_MASK;
		type = lpfc_sli_iocb_cmd_type(iocb_cmd_type);
		switch (type) {
		case LPFC_SOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_sol_iocb(phba, pring, saveq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			break;

		case LPFC_UNSOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_unsol_iocb(phba, pring, saveq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			if (!rc)
				free_saveq = 0;
			break;

		case LPFC_ABORT_IOCB:
			cmdiocbp = NULL;
			if (irsp->ulpCommand != CMD_XRI_ABORTED_CX)
				cmdiocbp = lpfc_sli_iocbq_lookup(phba, pring,
								 saveq);
			if (cmdiocbp) {
				
				if (cmdiocbp->iocb_cmpl) {
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
					(cmdiocbp->iocb_cmpl)(phba, cmdiocbp,
							      saveq);
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
				} else
					__lpfc_sli_release_iocbq(phba,
								 cmdiocbp);
			}
			break;

		case LPFC_UNKNOWN_IOCB:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adaptermsg[LPFC_MAX_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
				memcpy(&adaptermsg[0], (uint8_t *)irsp,
				       MAX_MSG_DATA);
				dev_warn(&((phba->pcidev)->dev),
					 "lpfc%d: %s\n",
					 phba->brd_no, adaptermsg);
			} else {
				
				lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
						"0335 Unknown IOCB "
						"command Data: x%x "
						"x%x x%x x%x\n",
						irsp->ulpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		if (free_saveq) {
			list_for_each_entry_safe(rspiocbp, next_iocb,
						 &saveq->list, list) {
				list_del(&rspiocbp->list);
				__lpfc_sli_release_iocbq(phba, rspiocbp);
			}
			__lpfc_sli_release_iocbq(phba, saveq);
		}
		rspiocbp = NULL;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rspiocbp;
}

void
lpfc_sli_handle_slow_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t mask)
{
	phba->lpfc_sli_handle_slow_ring_event(phba, pring, mask);
}

static void
lpfc_sli_handle_slow_ring_event_s3(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_pgp *pgp;
	IOCB_t *entry;
	IOCB_t *irsp = NULL;
	struct lpfc_iocbq *rspiocbp = NULL;
	uint32_t portRspPut, portRspMax;
	unsigned long iflag;
	uint32_t status;

	pgp = &phba->port_gp[pring->ringno];
	spin_lock_irqsave(&phba->hbalock, iflag);
	pring->stats.iocb_event++;

	portRspMax = pring->numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (portRspPut >= portRspMax) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0303 Ring %d handler: portRspPut %d "
				"is bigger than rsp ring %d\n",
				pring->ringno, portRspPut, portRspMax);

		phba->link_state = LPFC_HBA_ERROR;
		spin_unlock_irqrestore(&phba->hbalock, iflag);

		phba->work_hs = HS_FFER3;
		lpfc_handle_eratt(phba);

		return;
	}

	rmb();
	while (pring->rspidx != portRspPut) {
		entry = lpfc_resp_iocb(phba, pring);

		phba->last_completion_time = jiffies;
		rspiocbp = __lpfc_sli_get_iocbq(phba);
		if (rspiocbp == NULL) {
			printk(KERN_ERR "%s: out of buffers! Failing "
			       "completion.\n", __func__);
			break;
		}

		lpfc_sli_pcimem_bcopy(entry, &rspiocbp->iocb,
				      phba->iocb_rsp_size);
		irsp = &rspiocbp->iocb;

		if (++pring->rspidx >= portRspMax)
			pring->rspidx = 0;

		if (pring->ringno == LPFC_ELS_RING) {
			lpfc_debugfs_slow_ring_trc(phba,
			"IOCB rsp ring:   wd4:x%08x wd6:x%08x wd7:x%08x",
				*(((uint32_t *) irsp) + 4),
				*(((uint32_t *) irsp) + 6),
				*(((uint32_t *) irsp) + 7));
		}

		writel(pring->rspidx, &phba->host_gp[pring->ringno].rspGetInx);

		spin_unlock_irqrestore(&phba->hbalock, iflag);
		
		rspiocbp = lpfc_sli_sp_handle_rspiocb(phba, pring, rspiocbp);
		spin_lock_irqsave(&phba->hbalock, iflag);

		if (pring->rspidx == portRspPut) {
			portRspPut = le32_to_cpu(pgp->rspPutInx);
		}
	} 

	if ((rspiocbp != NULL) && (mask & HA_R0RE_REQ)) {
		
		pring->stats.iocb_rsp_full++;
		
		status = ((CA_R0ATT | CA_R0RE_RSP) << (pring->ringno * 4));
		writel(status, phba->CAregaddr);
		readl(phba->CAregaddr); 
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & LPFC_CALL_RING_AVAILABLE)) {
		pring->flag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		
		pring->local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sli_resume_iocb(phba, pring);

		if ((pring->lpfc_sli_cmd_available))
			(pring->lpfc_sli_cmd_available) (phba, pring);

	}

	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return;
}

static void
lpfc_sli_handle_slow_ring_event_s4(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_iocbq *irspiocbq;
	struct hbq_dmabuf *dmabuf;
	struct lpfc_cq_event *cq_event;
	unsigned long iflag;

	spin_lock_irqsave(&phba->hbalock, iflag);
	phba->hba_flag &= ~HBA_SP_QUEUE_EVT;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	while (!list_empty(&phba->sli4_hba.sp_queue_event)) {
		
		spin_lock_irqsave(&phba->hbalock, iflag);
		list_remove_head(&phba->sli4_hba.sp_queue_event,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irqrestore(&phba->hbalock, iflag);

		switch (bf_get(lpfc_wcqe_c_code, &cq_event->cqe.wcqe_cmpl)) {
		case CQE_CODE_COMPL_WQE:
			irspiocbq = container_of(cq_event, struct lpfc_iocbq,
						 cq_event);
			
			irspiocbq = lpfc_sli4_els_wcqe_to_rspiocbq(phba,
								   irspiocbq);
			if (irspiocbq)
				lpfc_sli_sp_handle_rspiocb(phba, pring,
							   irspiocbq);
			break;
		case CQE_CODE_RECEIVE:
		case CQE_CODE_RECEIVE_V1:
			dmabuf = container_of(cq_event, struct hbq_dmabuf,
					      cq_event);
			lpfc_sli4_handle_received_buffer(phba, dmabuf);
			break;
		default:
			break;
		}
	}
}

void
lpfc_sli_abort_iocb_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	LIST_HEAD(completions);
	struct lpfc_iocbq *iocb, *next_iocb;

	if (pring->ringno == LPFC_ELS_RING) {
		lpfc_fabric_abort_hba(phba);
	}

	spin_lock_irq(&phba->hbalock);
	list_splice_init(&pring->txq, &completions);
	pring->txq_cnt = 0;

	
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list)
		lpfc_sli_issue_abort_iotag(phba, pring, iocb);

	spin_unlock_irq(&phba->hbalock);

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

void
lpfc_sli_flush_fcp_rings(struct lpfc_hba *phba)
{
	LIST_HEAD(txq);
	LIST_HEAD(txcmplq);
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring  *pring;

	
	pring = &psli->ring[psli->fcp_ring];

	spin_lock_irq(&phba->hbalock);
	
	list_splice_init(&pring->txq, &txq);
	pring->txq_cnt = 0;

	
	list_splice_init(&pring->txcmplq, &txcmplq);
	pring->txcmplq_cnt = 0;
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_sli_cancel_iocbs(phba, &txq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);

	
	lpfc_sli_cancel_iocbs(phba, &txcmplq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);
}

static int
lpfc_sli_brdready_s3(struct lpfc_hba *phba, uint32_t mask)
{
	uint32_t status;
	int i = 0;
	int retval = 0;

	
	if (lpfc_readl(phba->HSregaddr, &status))
		return 1;

	while (((status & mask) != mask) &&
	       !(status & HS_FFERM) &&
	       i++ < 20) {

		if (i <= 5)
			msleep(10);
		else if (i <= 10)
			msleep(500);
		else
			msleep(2500);

		if (i == 15) {
				
			phba->pport->port_state = LPFC_VPORT_UNKNOWN;
			lpfc_sli_brdrestart(phba);
		}
		
		if (lpfc_readl(phba->HSregaddr, &status)) {
			retval = 1;
			break;
		}
	}

	
	if ((status & HS_FFERM) || (i >= 20)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2751 Adapter failed to restart, "
				"status reg x%x, FW Data: A8 x%x AC x%x\n",
				status,
				readl(phba->MBslimaddr + 0xa8),
				readl(phba->MBslimaddr + 0xac));
		phba->link_state = LPFC_HBA_ERROR;
		retval = 1;
	}

	return retval;
}

static int
lpfc_sli_brdready_s4(struct lpfc_hba *phba, uint32_t mask)
{
	uint32_t status;
	int retval = 0;

	
	status = lpfc_sli4_post_status_check(phba);

	if (status) {
		phba->pport->port_state = LPFC_VPORT_UNKNOWN;
		lpfc_sli_brdrestart(phba);
		status = lpfc_sli4_post_status_check(phba);
	}

	
	if (status) {
		phba->link_state = LPFC_HBA_ERROR;
		retval = 1;
	} else
		phba->sli4_hba.intr_enable = 0;

	return retval;
}

int
lpfc_sli_brdready(struct lpfc_hba *phba, uint32_t mask)
{
	return phba->lpfc_sli_brdready(phba, mask);
}

#define BARRIER_TEST_PATTERN (0xdeadbeef)

void lpfc_reset_barrier(struct lpfc_hba *phba)
{
	uint32_t __iomem *resp_buf;
	uint32_t __iomem *mbox_buf;
	volatile uint32_t mbox;
	uint32_t hc_copy, ha_copy, resp_data;
	int  i;
	uint8_t hdrtype;

	pci_read_config_byte(phba->pcidev, PCI_HEADER_TYPE, &hdrtype);
	if (hdrtype != 0x80 ||
	    (FC_JEDEC_ID(phba->vpd.rev.biuRev) != HELIOS_JEDEC_ID &&
	     FC_JEDEC_ID(phba->vpd.rev.biuRev) != THOR_JEDEC_ID))
		return;

	resp_buf = phba->MBslimaddr;

	
	if (lpfc_readl(phba->HCregaddr, &hc_copy))
		return;
	writel((hc_copy & ~HC_ERINT_ENA), phba->HCregaddr);
	readl(phba->HCregaddr); 
	phba->link_flag |= LS_IGNORE_ERATT;

	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		return;
	if (ha_copy & HA_ERATT) {
		
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}

	mbox = 0;
	((MAILBOX_t *)&mbox)->mbxCommand = MBX_KILL_BOARD;
	((MAILBOX_t *)&mbox)->mbxOwner = OWN_CHIP;

	writel(BARRIER_TEST_PATTERN, (resp_buf + 1));
	mbox_buf = phba->MBslimaddr;
	writel(mbox, mbox_buf);

	for (i = 0; i < 50; i++) {
		if (lpfc_readl((resp_buf + 1), &resp_data))
			return;
		if (resp_data != ~(BARRIER_TEST_PATTERN))
			mdelay(1);
		else
			break;
	}
	resp_data = 0;
	if (lpfc_readl((resp_buf + 1), &resp_data))
		return;
	if (resp_data  != ~(BARRIER_TEST_PATTERN)) {
		if (phba->sli.sli_flag & LPFC_SLI_ACTIVE ||
		    phba->pport->stopped)
			goto restore_hc;
		else
			goto clear_errat;
	}

	((MAILBOX_t *)&mbox)->mbxOwner = OWN_HOST;
	resp_data = 0;
	for (i = 0; i < 500; i++) {
		if (lpfc_readl(resp_buf, &resp_data))
			return;
		if (resp_data != mbox)
			mdelay(1);
		else
			break;
	}

clear_errat:

	while (++i < 500) {
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			return;
		if (!(ha_copy & HA_ERATT))
			mdelay(1);
		else
			break;
	}

	if (readl(phba->HAregaddr) & HA_ERATT) {
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}

restore_hc:
	phba->link_flag &= ~LS_IGNORE_ERATT;
	writel(hc_copy, phba->HCregaddr);
	readl(phba->HCregaddr); 
}

int
lpfc_sli_brdkill(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	LPFC_MBOXQ_t *pmb;
	uint32_t status;
	uint32_t ha_copy;
	int retval;
	int i = 0;

	psli = &phba->sli;

	
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0329 Kill HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb)
		return 1;

	
	spin_lock_irq(&phba->hbalock);
	if (lpfc_readl(phba->HCregaddr, &status)) {
		spin_unlock_irq(&phba->hbalock);
		mempool_free(pmb, phba->mbox_mem_pool);
		return 1;
	}
	status &= ~HC_ERINT_ENA;
	writel(status, phba->HCregaddr);
	readl(phba->HCregaddr); 
	phba->link_flag |= LS_IGNORE_ERATT;
	spin_unlock_irq(&phba->hbalock);

	lpfc_kill_board(phba, pmb);
	pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	retval = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if (retval != MBX_SUCCESS) {
		if (retval != MBX_BUSY)
			mempool_free(pmb, phba->mbox_mem_pool);
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2752 KILL_BOARD command failed retval %d\n",
				retval);
		spin_lock_irq(&phba->hbalock);
		phba->link_flag &= ~LS_IGNORE_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return 1;
	}

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	mempool_free(pmb, phba->mbox_mem_pool);

	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		return 1;
	while ((i++ < 30) && !(ha_copy & HA_ERATT)) {
		mdelay(100);
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			return 1;
	}

	del_timer_sync(&psli->mbox_tmo);
	if (ha_copy & HA_ERATT) {
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	psli->mbox_active = NULL;
	phba->link_flag &= ~LS_IGNORE_ERATT;
	spin_unlock_irq(&phba->hbalock);

	lpfc_hba_down_post(phba);
	phba->link_state = LPFC_HBA_ERROR;

	return ha_copy & HA_ERATT ? 0 : 1;
}

int
lpfc_sli_brdreset(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;
	uint16_t cfg_value;
	int i;

	psli = &phba->sli;

	
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0325 Reset HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	
	phba->fc_eventTag = 0;
	phba->link_events = 0;
	phba->pport->fc_myDID = 0;
	phba->pport->fc_prevDID = 0;

	
	pci_read_config_word(phba->pcidev, PCI_COMMAND, &cfg_value);
	pci_write_config_word(phba->pcidev, PCI_COMMAND,
			      (cfg_value &
			       ~(PCI_COMMAND_PARITY | PCI_COMMAND_SERR)));

	psli->sli_flag &= ~(LPFC_SLI_ACTIVE | LPFC_PROCESS_LA);

	
	writel(HC_INITFF, phba->HCregaddr);
	mdelay(1);
	readl(phba->HCregaddr); 
	writel(0, phba->HCregaddr);
	readl(phba->HCregaddr); 

	
	pci_write_config_word(phba->pcidev, PCI_COMMAND, cfg_value);

	
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		pring->flag = 0;
		pring->rspidx = 0;
		pring->next_cmdidx  = 0;
		pring->local_getidx = 0;
		pring->cmdidx = 0;
		pring->missbufcnt = 0;
	}

	phba->link_state = LPFC_WARM_START;
	return 0;
}

int
lpfc_sli4_brdreset(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	uint16_t cfg_value;

	
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0295 Reset HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	
	phba->fc_eventTag = 0;
	phba->link_events = 0;
	phba->pport->fc_myDID = 0;
	phba->pport->fc_prevDID = 0;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~(LPFC_PROCESS_LA);
	phba->fcf.fcf_flag = 0;
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0389 Performing PCI function reset!\n");

	
	pci_read_config_word(phba->pcidev, PCI_COMMAND, &cfg_value);
	pci_write_config_word(phba->pcidev, PCI_COMMAND, (cfg_value &
			      ~(PCI_COMMAND_PARITY | PCI_COMMAND_SERR)));

	
	lpfc_sli4_queue_destroy(phba);
	lpfc_pci_function_reset(phba);

	
	pci_write_config_word(phba->pcidev, PCI_COMMAND, cfg_value);

	return 0;
}

static int
lpfc_sli_brdrestart_s3(struct lpfc_hba *phba)
{
	MAILBOX_t *mb;
	struct lpfc_sli *psli;
	volatile uint32_t word0;
	void __iomem *to_slim;
	uint32_t hba_aer_enabled;

	spin_lock_irq(&phba->hbalock);

	
	hba_aer_enabled = phba->hba_flag & HBA_AER_ENABLED;

	psli = &phba->sli;

	
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0337 Restart HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	word0 = 0;
	mb = (MAILBOX_t *) &word0;
	mb->mbxCommand = MBX_RESTART;
	mb->mbxHc = 1;

	lpfc_reset_barrier(phba);

	to_slim = phba->MBslimaddr;
	writel(*(uint32_t *) mb, to_slim);
	readl(to_slim); 

	
	if (phba->pport->port_state)
		word0 = 1;	
	else
		word0 = 0;	
	to_slim = phba->MBslimaddr + sizeof (uint32_t);
	writel(*(uint32_t *) mb, to_slim);
	readl(to_slim); 

	lpfc_sli_brdreset(phba);
	phba->pport->stopped = 0;
	phba->link_state = LPFC_INIT_START;
	phba->hba_flag = 0;
	spin_unlock_irq(&phba->hbalock);

	memset(&psli->lnk_stat_offsets, 0, sizeof(psli->lnk_stat_offsets));
	psli->stats_start = get_seconds();

	
	mdelay(100);

	
	if (hba_aer_enabled)
		pci_disable_pcie_error_reporting(phba->pcidev);

	lpfc_hba_down_post(phba);

	return 0;
}

static int
lpfc_sli_brdrestart_s4(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	uint32_t hba_aer_enabled;

	
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0296 Restart HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	
	hba_aer_enabled = phba->hba_flag & HBA_AER_ENABLED;

	lpfc_sli4_brdreset(phba);

	spin_lock_irq(&phba->hbalock);
	phba->pport->stopped = 0;
	phba->link_state = LPFC_INIT_START;
	phba->hba_flag = 0;
	spin_unlock_irq(&phba->hbalock);

	memset(&psli->lnk_stat_offsets, 0, sizeof(psli->lnk_stat_offsets));
	psli->stats_start = get_seconds();

	
	if (hba_aer_enabled)
		pci_disable_pcie_error_reporting(phba->pcidev);

	lpfc_hba_down_post(phba);

	return 0;
}

int
lpfc_sli_brdrestart(struct lpfc_hba *phba)
{
	return phba->lpfc_sli_brdrestart(phba);
}

static int
lpfc_sli_chipset_init(struct lpfc_hba *phba)
{
	uint32_t status, i = 0;

	
	if (lpfc_readl(phba->HSregaddr, &status))
		return -EIO;

	
	i = 0;
	while ((status & (HS_FFRDY | HS_MBRDY)) != (HS_FFRDY | HS_MBRDY)) {

		if (i++ >= 200) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0436 Adapter failed to init, "
					"timeout, status reg x%x, "
					"FW Data: A8 x%x AC x%x\n", status,
					readl(phba->MBslimaddr + 0xa8),
					readl(phba->MBslimaddr + 0xac));
			phba->link_state = LPFC_HBA_ERROR;
			return -ETIMEDOUT;
		}

		
		if (status & HS_FFERM) {
			
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0437 Adapter failed to init, "
					"chipset, status reg x%x, "
					"FW Data: A8 x%x AC x%x\n", status,
					readl(phba->MBslimaddr + 0xa8),
					readl(phba->MBslimaddr + 0xac));
			phba->link_state = LPFC_HBA_ERROR;
			return -EIO;
		}

		if (i <= 10)
			msleep(10);
		else if (i <= 100)
			msleep(100);
		else
			msleep(1000);

		if (i == 150) {
			
			phba->pport->port_state = LPFC_VPORT_UNKNOWN;
			lpfc_sli_brdrestart(phba);
		}
		
		if (lpfc_readl(phba->HSregaddr, &status))
			return -EIO;
	}

	
	if (status & HS_FFERM) {
		
		
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0438 Adapter failed to init, chipset, "
				"status reg x%x, "
				"FW Data: A8 x%x AC x%x\n", status,
				readl(phba->MBslimaddr + 0xa8),
				readl(phba->MBslimaddr + 0xac));
		phba->link_state = LPFC_HBA_ERROR;
		return -EIO;
	}

	
	writel(0, phba->HCregaddr);
	readl(phba->HCregaddr); 

	
	writel(0xffffffff, phba->HAregaddr);
	readl(phba->HAregaddr); 
	return 0;
}

int
lpfc_sli_hbq_count(void)
{
	return ARRAY_SIZE(lpfc_hbq_defs);
}

static int
lpfc_sli_hbq_entry_count(void)
{
	int  hbq_count = lpfc_sli_hbq_count();
	int  count = 0;
	int  i;

	for (i = 0; i < hbq_count; ++i)
		count += lpfc_hbq_defs[i]->entry_count;
	return count;
}

int
lpfc_sli_hbq_size(void)
{
	return lpfc_sli_hbq_entry_count() * sizeof(struct lpfc_hbq_entry);
}

static int
lpfc_sli_hbq_setup(struct lpfc_hba *phba)
{
	int  hbq_count = lpfc_sli_hbq_count();
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *pmbox;
	uint32_t hbqno;
	uint32_t hbq_entry_index;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);

	if (!pmb)
		return -ENOMEM;

	pmbox = &pmb->u.mb;

	
	phba->link_state = LPFC_INIT_MBX_CMDS;
	phba->hbq_in_use = 1;

	hbq_entry_index = 0;
	for (hbqno = 0; hbqno < hbq_count; ++hbqno) {
		phba->hbqs[hbqno].next_hbqPutIdx = 0;
		phba->hbqs[hbqno].hbqPutIdx      = 0;
		phba->hbqs[hbqno].local_hbqGetIdx   = 0;
		phba->hbqs[hbqno].entry_count =
			lpfc_hbq_defs[hbqno]->entry_count;
		lpfc_config_hbq(phba, hbqno, lpfc_hbq_defs[hbqno],
			hbq_entry_index, pmb);
		hbq_entry_index += phba->hbqs[hbqno].entry_count;

		if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL) != MBX_SUCCESS) {

			lpfc_printf_log(phba, KERN_ERR,
					LOG_SLI | LOG_VPORT,
					"1805 Adapter failed to init. "
					"Data: x%x x%x x%x\n",
					pmbox->mbxCommand,
					pmbox->mbxStatus, hbqno);

			phba->link_state = LPFC_HBA_ERROR;
			mempool_free(pmb, phba->mbox_mem_pool);
			return -ENXIO;
		}
	}
	phba->hbq_count = hbq_count;

	mempool_free(pmb, phba->mbox_mem_pool);

	
	for (hbqno = 0; hbqno < hbq_count; ++hbqno)
		lpfc_sli_hbqbuf_init_hbqs(phba, hbqno);
	return 0;
}

static int
lpfc_sli4_rb_setup(struct lpfc_hba *phba)
{
	phba->hbq_in_use = 1;
	phba->hbqs[0].entry_count = lpfc_hbq_defs[0]->entry_count;
	phba->hbq_count = 1;
	
	lpfc_sli_hbqbuf_init_hbqs(phba, 0);
	return 0;
}

int
lpfc_sli_config_port(struct lpfc_hba *phba, int sli_mode)
{
	LPFC_MBOXQ_t *pmb;
	uint32_t resetcount = 0, rc = 0, done = 0;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	phba->sli_rev = sli_mode;
	while (resetcount < 2 && !done) {
		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag |= LPFC_SLI_MBOX_ACTIVE;
		spin_unlock_irq(&phba->hbalock);
		phba->pport->port_state = LPFC_VPORT_UNKNOWN;
		lpfc_sli_brdrestart(phba);
		rc = lpfc_sli_chipset_init(phba);
		if (rc)
			break;

		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		spin_unlock_irq(&phba->hbalock);
		resetcount++;

		rc = lpfc_config_port_prep(phba);
		if (rc == -ERESTART) {
			phba->link_state = LPFC_LINK_UNKNOWN;
			continue;
		} else if (rc)
			break;

		phba->link_state = LPFC_INIT_MBX_CMDS;
		lpfc_config_port(phba, pmb);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		phba->sli3_options &= ~(LPFC_SLI3_NPIV_ENABLED |
					LPFC_SLI3_HBQ_ENABLED |
					LPFC_SLI3_CRP_ENABLED |
					LPFC_SLI3_BG_ENABLED |
					LPFC_SLI3_DSS_ENABLED);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0442 Adapter failed to init, mbxCmd x%x "
				"CONFIG_PORT, mbxStatus x%x Data: x%x\n",
				pmb->u.mb.mbxCommand, pmb->u.mb.mbxStatus, 0);
			spin_lock_irq(&phba->hbalock);
			phba->sli.sli_flag &= ~LPFC_SLI_ACTIVE;
			spin_unlock_irq(&phba->hbalock);
			rc = -ENXIO;
		} else {
			
			spin_lock_irq(&phba->hbalock);
			phba->sli.sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
			spin_unlock_irq(&phba->hbalock);
			done = 1;

			if ((pmb->u.mb.un.varCfgPort.casabt == 1) &&
			    (pmb->u.mb.un.varCfgPort.gasabt == 0))
				lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"3110 Port did not grant ASABT\n");
		}
	}
	if (!done) {
		rc = -EINVAL;
		goto do_prep_failed;
	}
	if (pmb->u.mb.un.varCfgPort.sli_mode == 3) {
		if (!pmb->u.mb.un.varCfgPort.cMA) {
			rc = -ENXIO;
			goto do_prep_failed;
		}
		if (phba->max_vpi && pmb->u.mb.un.varCfgPort.gmv) {
			phba->sli3_options |= LPFC_SLI3_NPIV_ENABLED;
			phba->max_vpi = pmb->u.mb.un.varCfgPort.max_vpi;
			phba->max_vports = (phba->max_vpi > phba->max_vports) ?
				phba->max_vpi : phba->max_vports;

		} else
			phba->max_vpi = 0;
		phba->fips_level = 0;
		phba->fips_spec_rev = 0;
		if (pmb->u.mb.un.varCfgPort.gdss) {
			phba->sli3_options |= LPFC_SLI3_DSS_ENABLED;
			phba->fips_level = pmb->u.mb.un.varCfgPort.fips_level;
			phba->fips_spec_rev = pmb->u.mb.un.varCfgPort.fips_rev;
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2850 Security Crypto Active. FIPS x%d "
					"(Spec Rev: x%d)",
					phba->fips_level, phba->fips_spec_rev);
		}
		if (pmb->u.mb.un.varCfgPort.sec_err) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2856 Config Port Security Crypto "
					"Error: x%x ",
					pmb->u.mb.un.varCfgPort.sec_err);
		}
		if (pmb->u.mb.un.varCfgPort.gerbm)
			phba->sli3_options |= LPFC_SLI3_HBQ_ENABLED;
		if (pmb->u.mb.un.varCfgPort.gcrp)
			phba->sli3_options |= LPFC_SLI3_CRP_ENABLED;

		phba->hbq_get = phba->mbox->us.s3_pgp.hbq_get;
		phba->port_gp = phba->mbox->us.s3_pgp.port;

		if (phba->cfg_enable_bg) {
			if (pmb->u.mb.un.varCfgPort.gbg)
				phba->sli3_options |= LPFC_SLI3_BG_ENABLED;
			else
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
						"0443 Adapter did not grant "
						"BlockGuard\n");
		}
	} else {
		phba->hbq_get = NULL;
		phba->port_gp = phba->mbox->us.s2.port;
		phba->max_vpi = 0;
	}
do_prep_failed:
	mempool_free(pmb, phba->mbox_mem_pool);
	return rc;
}


int
lpfc_sli_hba_setup(struct lpfc_hba *phba)
{
	uint32_t rc;
	int  mode = 3, i;
	int longs;

	switch (lpfc_sli_mode) {
	case 2:
		if (phba->cfg_enable_npiv) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_VPORT,
				"1824 NPIV enabled: Override lpfc_sli_mode "
				"parameter (%d) to auto (0).\n",
				lpfc_sli_mode);
			break;
		}
		mode = 2;
		break;
	case 0:
	case 3:
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_VPORT,
				"1819 Unrecognized lpfc_sli_mode "
				"parameter: %d.\n", lpfc_sli_mode);

		break;
	}

	rc = lpfc_sli_config_port(phba, mode);

	if (rc && lpfc_sli_mode == 3)
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_VPORT,
				"1820 Unable to select SLI-3.  "
				"Not supported by adapter.\n");
	if (rc && mode != 2)
		rc = lpfc_sli_config_port(phba, 2);
	if (rc)
		goto lpfc_sli_hba_setup_error;

	
	if (phba->cfg_aer_support == 1 && !(phba->hba_flag & HBA_AER_ENABLED)) {
		rc = pci_enable_pcie_error_reporting(phba->pcidev);
		if (!rc) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2709 This device supports "
					"Advanced Error Reporting (AER)\n");
			spin_lock_irq(&phba->hbalock);
			phba->hba_flag |= HBA_AER_ENABLED;
			spin_unlock_irq(&phba->hbalock);
		} else {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2708 This device does not support "
					"Advanced Error Reporting (AER)\n");
			phba->cfg_aer_support = 0;
		}
	}

	if (phba->sli_rev == 3) {
		phba->iocb_cmd_size = SLI3_IOCB_CMD_SIZE;
		phba->iocb_rsp_size = SLI3_IOCB_RSP_SIZE;
	} else {
		phba->iocb_cmd_size = SLI2_IOCB_CMD_SIZE;
		phba->iocb_rsp_size = SLI2_IOCB_RSP_SIZE;
		phba->sli3_options = 0;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0444 Firmware in SLI %x mode. Max_vpi %d\n",
			phba->sli_rev, phba->max_vpi);
	rc = lpfc_sli_ring_map(phba);

	if (rc)
		goto lpfc_sli_hba_setup_error;

	
	if (phba->sli_rev == LPFC_SLI_REV3) {
		if ((phba->vpi_bmask == NULL) && (phba->vpi_ids == NULL)) {
			longs = (phba->max_vpi + BITS_PER_LONG) / BITS_PER_LONG;
			phba->vpi_bmask = kzalloc(longs * sizeof(unsigned long),
						  GFP_KERNEL);
			if (!phba->vpi_bmask) {
				rc = -ENOMEM;
				goto lpfc_sli_hba_setup_error;
			}

			phba->vpi_ids = kzalloc(
					(phba->max_vpi+1) * sizeof(uint16_t),
					GFP_KERNEL);
			if (!phba->vpi_ids) {
				kfree(phba->vpi_bmask);
				rc = -ENOMEM;
				goto lpfc_sli_hba_setup_error;
			}
			for (i = 0; i < phba->max_vpi; i++)
				phba->vpi_ids[i] = i;
		}
	}

	
	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		rc = lpfc_sli_hbq_setup(phba);
		if (rc)
			goto lpfc_sli_hba_setup_error;
	}
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag |= LPFC_PROCESS_LA;
	spin_unlock_irq(&phba->hbalock);

	rc = lpfc_config_port_post(phba);
	if (rc)
		goto lpfc_sli_hba_setup_error;

	return rc;

lpfc_sli_hba_setup_error:
	phba->link_state = LPFC_HBA_ERROR;
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0445 Firmware initialization failed\n");
	return rc;
}

static int
lpfc_sli4_read_fcoe_params(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_dmabuf *mp;
	struct lpfc_mqe *mqe;
	uint32_t data_length;
	int rc;

	
	phba->valid_vlan = 0;
	phba->fc_map[0] = LPFC_FCOE_FCF_MAP0;
	phba->fc_map[1] = LPFC_FCOE_FCF_MAP1;
	phba->fc_map[2] = LPFC_FCOE_FCF_MAP2;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	mqe = &mboxq->u.mqe;
	if (lpfc_sli4_dump_cfg_rg23(phba, mboxq)) {
		rc = -ENOMEM;
		goto out_free_mboxq;
	}

	mp = (struct lpfc_dmabuf *) mboxq->context1;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):2571 Mailbox cmd x%x Status x%x "
			"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x "
			"x%x x%x x%x x%x x%x x%x x%x x%x x%x "
			"CQ: x%x x%x x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0,
			bf_get(lpfc_mqe_command, mqe),
			bf_get(lpfc_mqe_status, mqe),
			mqe->un.mb_words[0], mqe->un.mb_words[1],
			mqe->un.mb_words[2], mqe->un.mb_words[3],
			mqe->un.mb_words[4], mqe->un.mb_words[5],
			mqe->un.mb_words[6], mqe->un.mb_words[7],
			mqe->un.mb_words[8], mqe->un.mb_words[9],
			mqe->un.mb_words[10], mqe->un.mb_words[11],
			mqe->un.mb_words[12], mqe->un.mb_words[13],
			mqe->un.mb_words[14], mqe->un.mb_words[15],
			mqe->un.mb_words[16], mqe->un.mb_words[50],
			mboxq->mcqe.word0,
			mboxq->mcqe.mcqe_tag0, 	mboxq->mcqe.mcqe_tag1,
			mboxq->mcqe.trailer);

	if (rc) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
		rc = -EIO;
		goto out_free_mboxq;
	}
	data_length = mqe->un.mb_words[5];
	if (data_length > DMP_RGN23_SIZE) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
		rc = -EIO;
		goto out_free_mboxq;
	}

	lpfc_parse_fcoe_conf(phba, mp->virt, data_length);
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	rc = 0;

out_free_mboxq:
	mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
}

static int
lpfc_sli4_read_rev(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq,
		    uint8_t *vpd, uint32_t *vpd_size)
{
	int rc = 0;
	uint32_t dma_size;
	struct lpfc_dmabuf *dmabuf;
	struct lpfc_mqe *mqe;

	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf)
		return -ENOMEM;

	dma_size = *vpd_size;
	dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
					  dma_size,
					  &dmabuf->phys,
					  GFP_KERNEL);
	if (!dmabuf->virt) {
		kfree(dmabuf);
		return -ENOMEM;
	}
	memset(dmabuf->virt, 0, dma_size);

	lpfc_read_rev(phba, mboxq);
	mqe = &mboxq->u.mqe;
	mqe->un.read_rev.vpd_paddr_high = putPaddrHigh(dmabuf->phys);
	mqe->un.read_rev.vpd_paddr_low = putPaddrLow(dmabuf->phys);
	mqe->un.read_rev.word1 &= 0x0000FFFF;
	bf_set(lpfc_mbx_rd_rev_vpd, &mqe->un.read_rev, 1);
	bf_set(lpfc_mbx_rd_rev_avail_len, &mqe->un.read_rev, dma_size);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (rc) {
		dma_free_coherent(&phba->pcidev->dev, dma_size,
				  dmabuf->virt, dmabuf->phys);
		kfree(dmabuf);
		return -EIO;
	}

	if (mqe->un.read_rev.avail_vpd_len < *vpd_size)
		*vpd_size = mqe->un.read_rev.avail_vpd_len;

	memcpy(vpd, dmabuf->virt, *vpd_size);

	dma_free_coherent(&phba->pcidev->dev, dma_size,
			  dmabuf->virt, dmabuf->phys);
	kfree(dmabuf);
	return 0;
}

static int
lpfc_sli4_retrieve_pport_name(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mbx_get_cntl_attributes *mbx_cntl_attr;
	struct lpfc_controller_attribute *cntl_attr;
	struct lpfc_mbx_get_port_name *get_port_name;
	void *virtaddr = NULL;
	uint32_t alloclen, reqlen;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	char cport_name = 0;
	int rc;

	
	phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_INVAL;
	phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_NON;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;
	
	phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_INVAL;
	lpfc_sli4_read_config(phba);
	if (phba->sli4_hba.lnk_info.lnk_dv == LPFC_LNK_DAT_VAL)
		goto retrieve_ppname;

	
	reqlen = sizeof(struct lpfc_mbx_get_cntl_attributes);
	alloclen = lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			LPFC_MBOX_OPCODE_GET_CNTL_ATTRIBUTES, reqlen,
			LPFC_SLI4_MBX_NEMBED);
	if (alloclen < reqlen) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"3084 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory size "
				"(%d)\n", alloclen, reqlen);
		rc = -ENOMEM;
		goto out_free_mboxq;
	}
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	virtaddr = mboxq->sge_array->addr[0];
	mbx_cntl_attr = (struct lpfc_mbx_get_cntl_attributes *)virtaddr;
	shdr = &mbx_cntl_attr->cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3085 Mailbox x%x (x%x/x%x) failed, "
				"rc:x%x, status:x%x, add_status:x%x\n",
				bf_get(lpfc_mqe_command, &mboxq->u.mqe),
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				rc, shdr_status, shdr_add_status);
		rc = -ENXIO;
		goto out_free_mboxq;
	}
	cntl_attr = &mbx_cntl_attr->cntl_attr;
	phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_VAL;
	phba->sli4_hba.lnk_info.lnk_tp =
		bf_get(lpfc_cntl_attr_lnk_type, cntl_attr);
	phba->sli4_hba.lnk_info.lnk_no =
		bf_get(lpfc_cntl_attr_lnk_numb, cntl_attr);
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3086 lnk_type:%d, lnk_numb:%d\n",
			phba->sli4_hba.lnk_info.lnk_tp,
			phba->sli4_hba.lnk_info.lnk_no);

retrieve_ppname:
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
		LPFC_MBOX_OPCODE_GET_PORT_NAME,
		sizeof(struct lpfc_mbx_get_port_name) -
		sizeof(struct lpfc_sli4_cfg_mhdr),
		LPFC_SLI4_MBX_EMBED);
	get_port_name = &mboxq->u.mqe.un.get_port_name;
	shdr = (union lpfc_sli4_cfg_shdr *)&get_port_name->header.cfg_shdr;
	bf_set(lpfc_mbox_hdr_version, &shdr->request, LPFC_OPCODE_VERSION_1);
	bf_set(lpfc_mbx_get_port_name_lnk_type, &get_port_name->u.request,
		phba->sli4_hba.lnk_info.lnk_tp);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3087 Mailbox x%x (x%x/x%x) failed: "
				"rc:x%x, status:x%x, add_status:x%x\n",
				bf_get(lpfc_mqe_command, &mboxq->u.mqe),
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				rc, shdr_status, shdr_add_status);
		rc = -ENXIO;
		goto out_free_mboxq;
	}
	switch (phba->sli4_hba.lnk_info.lnk_no) {
	case LPFC_LINK_NUMBER_0:
		cport_name = bf_get(lpfc_mbx_get_port_name_name0,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	case LPFC_LINK_NUMBER_1:
		cport_name = bf_get(lpfc_mbx_get_port_name_name1,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	case LPFC_LINK_NUMBER_2:
		cport_name = bf_get(lpfc_mbx_get_port_name_name2,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	case LPFC_LINK_NUMBER_3:
		cport_name = bf_get(lpfc_mbx_get_port_name_name3,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	default:
		break;
	}

	if (phba->sli4_hba.pport_name_sta == LPFC_SLI4_PPNAME_GET) {
		phba->Port[0] = cport_name;
		phba->Port[1] = '\0';
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3091 SLI get port name: %s\n", phba->Port);
	}

out_free_mboxq:
	if (rc != MBX_TIMEOUT) {
		if (bf_get(lpfc_mqe_command, &mboxq->u.mqe) == MBX_SLI4_CONFIG)
			lpfc_sli4_mbox_cmd_free(phba, mboxq);
		else
			mempool_free(mboxq, phba->mbox_mem_pool);
	}
	return rc;
}

static void
lpfc_sli4_arm_cqeq_intr(struct lpfc_hba *phba)
{
	uint8_t fcp_eqidx;

	lpfc_sli4_cq_release(phba->sli4_hba.mbx_cq, LPFC_QUEUE_REARM);
	lpfc_sli4_cq_release(phba->sli4_hba.els_cq, LPFC_QUEUE_REARM);
	fcp_eqidx = 0;
	if (phba->sli4_hba.fcp_cq) {
		do
			lpfc_sli4_cq_release(phba->sli4_hba.fcp_cq[fcp_eqidx],
					     LPFC_QUEUE_REARM);
		while (++fcp_eqidx < phba->cfg_fcp_eq_count);
	}
	lpfc_sli4_eq_release(phba->sli4_hba.sp_eq, LPFC_QUEUE_REARM);
	if (phba->sli4_hba.fp_eq) {
		for (fcp_eqidx = 0; fcp_eqidx < phba->cfg_fcp_eq_count;
		     fcp_eqidx++)
			lpfc_sli4_eq_release(phba->sli4_hba.fp_eq[fcp_eqidx],
					     LPFC_QUEUE_REARM);
	}
}

int
lpfc_sli4_get_avail_extnt_rsrc(struct lpfc_hba *phba, uint16_t type,
			       uint16_t *extnt_count, uint16_t *extnt_size)
{
	int rc = 0;
	uint32_t length;
	uint32_t mbox_tmo;
	struct lpfc_mbx_get_rsrc_extent_info *rsrc_info;
	LPFC_MBOXQ_t *mbox;

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	
	length = (sizeof(struct lpfc_mbx_get_rsrc_extent_info) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_GET_RSRC_EXTENT_INFO,
			 length, LPFC_SLI4_MBX_EMBED);

	
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, 0, type,
					LPFC_SLI4_MBX_EMBED);
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	rsrc_info = &mbox->u.mqe.un.rsrc_extent_info;
	if (bf_get(lpfc_mbox_hdr_status,
		   &rsrc_info->header.cfg_shdr.response)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_INIT,
				"2930 Failed to get resource extents "
				"Status 0x%x Add'l Status 0x%x\n",
				bf_get(lpfc_mbox_hdr_status,
				       &rsrc_info->header.cfg_shdr.response),
				bf_get(lpfc_mbox_hdr_add_status,
				       &rsrc_info->header.cfg_shdr.response));
		rc = -EIO;
		goto err_exit;
	}

	*extnt_count = bf_get(lpfc_mbx_get_rsrc_extent_info_cnt,
			      &rsrc_info->u.rsp);
	*extnt_size = bf_get(lpfc_mbx_get_rsrc_extent_info_size,
			     &rsrc_info->u.rsp);
 err_exit:
	mempool_free(mbox, phba->mbox_mem_pool);
	return rc;
}

static int
lpfc_sli4_chk_avail_extnt_rsrc(struct lpfc_hba *phba, uint16_t type)
{
	uint16_t curr_ext_cnt, rsrc_ext_cnt;
	uint16_t size_diff, rsrc_ext_size;
	int rc = 0;
	struct lpfc_rsrc_blks *rsrc_entry;
	struct list_head *rsrc_blk_list = NULL;

	size_diff = 0;
	curr_ext_cnt = 0;
	rc = lpfc_sli4_get_avail_extnt_rsrc(phba, type,
					    &rsrc_ext_cnt,
					    &rsrc_ext_size);
	if (unlikely(rc))
		return -EIO;

	switch (type) {
	case LPFC_RSC_TYPE_FCOE_RPI:
		rsrc_blk_list = &phba->sli4_hba.lpfc_rpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VPI:
		rsrc_blk_list = &phba->lpfc_vpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		rsrc_blk_list = &phba->sli4_hba.lpfc_xri_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		rsrc_blk_list = &phba->sli4_hba.lpfc_vfi_blk_list;
		break;
	default:
		break;
	}

	list_for_each_entry(rsrc_entry, rsrc_blk_list, list) {
		curr_ext_cnt++;
		if (rsrc_entry->rsrc_size != rsrc_ext_size)
			size_diff++;
	}

	if (curr_ext_cnt != rsrc_ext_cnt || size_diff != 0)
		rc = 1;

	return rc;
}

static int
lpfc_sli4_cfg_post_extnts(struct lpfc_hba *phba, uint16_t *extnt_cnt,
			  uint16_t type, bool *emb, LPFC_MBOXQ_t *mbox)
{
	int rc = 0;
	uint32_t req_len;
	uint32_t emb_len;
	uint32_t alloc_len, mbox_tmo;

	
	req_len = *extnt_cnt * sizeof(uint16_t);

	emb_len = sizeof(MAILBOX_t) - sizeof(struct mbox_header) -
		sizeof(uint32_t);

	*emb = LPFC_SLI4_MBX_EMBED;
	if (req_len > emb_len) {
		req_len = *extnt_cnt * sizeof(uint16_t) +
			sizeof(union lpfc_sli4_cfg_shdr) +
			sizeof(uint32_t);
		*emb = LPFC_SLI4_MBX_NEMBED;
	}

	alloc_len = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
				     LPFC_MBOX_OPCODE_ALLOC_RSRC_EXTENT,
				     req_len, *emb);
	if (alloc_len < req_len) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2982 Allocated DMA memory size (x%x) is "
			"less than the requested DMA memory "
			"size (x%x)\n", alloc_len, req_len);
		return -ENOMEM;
	}
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, *extnt_cnt, type, *emb);
	if (unlikely(rc))
		return -EIO;

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}

	if (unlikely(rc))
		rc = -EIO;
	return rc;
}

static int
lpfc_sli4_alloc_extent(struct lpfc_hba *phba, uint16_t type)
{
	bool emb = false;
	uint16_t rsrc_id_cnt, rsrc_cnt, rsrc_size;
	uint16_t rsrc_id, rsrc_start, j, k;
	uint16_t *ids;
	int i, rc;
	unsigned long longs;
	unsigned long *bmask;
	struct lpfc_rsrc_blks *rsrc_blks;
	LPFC_MBOXQ_t *mbox;
	uint32_t length;
	struct lpfc_id_range *id_array = NULL;
	void *virtaddr = NULL;
	struct lpfc_mbx_nembed_rsrc_extent *n_rsrc;
	struct lpfc_mbx_alloc_rsrc_extents *rsrc_ext;
	struct list_head *ext_blk_list;

	rc = lpfc_sli4_get_avail_extnt_rsrc(phba, type,
					    &rsrc_cnt,
					    &rsrc_size);
	if (unlikely(rc))
		return -EIO;

	if ((rsrc_cnt == 0) || (rsrc_size == 0)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_INIT,
			"3009 No available Resource Extents "
			"for resource type 0x%x: Count: 0x%x, "
			"Size 0x%x\n", type, rsrc_cnt,
			rsrc_size);
		return -ENOMEM;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_INIT,
			"2903 Available Resource Extents "
			"for resource type 0x%x: Count: 0x%x, "
			"Size 0x%x\n", type, rsrc_cnt,
			rsrc_size);

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	rc = lpfc_sli4_cfg_post_extnts(phba, &rsrc_cnt, type, &emb, mbox);
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	if (emb == LPFC_SLI4_MBX_EMBED) {
		rsrc_ext = &mbox->u.mqe.un.alloc_rsrc_extents;
		id_array = &rsrc_ext->u.rsp.id[0];
		rsrc_cnt = bf_get(lpfc_mbx_rsrc_cnt, &rsrc_ext->u.rsp);
	} else {
		virtaddr = mbox->sge_array->addr[0];
		n_rsrc = (struct lpfc_mbx_nembed_rsrc_extent *) virtaddr;
		rsrc_cnt = bf_get(lpfc_mbx_rsrc_cnt, n_rsrc);
		id_array = &n_rsrc->id;
	}

	longs = ((rsrc_cnt * rsrc_size) + BITS_PER_LONG - 1) / BITS_PER_LONG;
	rsrc_id_cnt = rsrc_cnt * rsrc_size;

	length = sizeof(struct lpfc_rsrc_blks);
	switch (type) {
	case LPFC_RSC_TYPE_FCOE_RPI:
		phba->sli4_hba.rpi_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.rpi_ids = kzalloc(rsrc_id_cnt *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_ids)) {
			kfree(phba->sli4_hba.rpi_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		phba->sli4_hba.next_rpi = rsrc_id_cnt;

		
		bmask = phba->sli4_hba.rpi_bmask;
		ids = phba->sli4_hba.rpi_ids;
		ext_blk_list = &phba->sli4_hba.lpfc_rpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VPI:
		phba->vpi_bmask = kzalloc(longs *
					  sizeof(unsigned long),
					  GFP_KERNEL);
		if (unlikely(!phba->vpi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->vpi_ids = kzalloc(rsrc_id_cnt *
					 sizeof(uint16_t),
					 GFP_KERNEL);
		if (unlikely(!phba->vpi_ids)) {
			kfree(phba->vpi_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		
		bmask = phba->vpi_bmask;
		ids = phba->vpi_ids;
		ext_blk_list = &phba->lpfc_vpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		phba->sli4_hba.xri_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.xri_ids = kzalloc(rsrc_id_cnt *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_ids)) {
			kfree(phba->sli4_hba.xri_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		
		bmask = phba->sli4_hba.xri_bmask;
		ids = phba->sli4_hba.xri_ids;
		ext_blk_list = &phba->sli4_hba.lpfc_xri_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		phba->sli4_hba.vfi_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.vfi_ids = kzalloc(rsrc_id_cnt *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_ids)) {
			kfree(phba->sli4_hba.vfi_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		
		bmask = phba->sli4_hba.vfi_bmask;
		ids = phba->sli4_hba.vfi_ids;
		ext_blk_list = &phba->sli4_hba.lpfc_vfi_blk_list;
		break;
	default:
		
		id_array = NULL;
		bmask = NULL;
		ids = NULL;
		ext_blk_list = NULL;
		goto err_exit;
	}

	for (i = 0, j = 0, k = 0; i < rsrc_cnt; i++) {
		if ((i % 2) == 0)
			rsrc_id = bf_get(lpfc_mbx_rsrc_id_word4_0,
					 &id_array[k]);
		else
			rsrc_id = bf_get(lpfc_mbx_rsrc_id_word4_1,
					 &id_array[k]);

		rsrc_blks = kzalloc(length, GFP_KERNEL);
		if (unlikely(!rsrc_blks)) {
			rc = -ENOMEM;
			kfree(bmask);
			kfree(ids);
			goto err_exit;
		}
		rsrc_blks->rsrc_start = rsrc_id;
		rsrc_blks->rsrc_size = rsrc_size;
		list_add_tail(&rsrc_blks->list, ext_blk_list);
		rsrc_start = rsrc_id;
		if ((type == LPFC_RSC_TYPE_FCOE_XRI) && (j == 0))
			phba->sli4_hba.scsi_xri_start = rsrc_start +
				lpfc_sli4_get_els_iocb_cnt(phba);

		while (rsrc_id < (rsrc_start + rsrc_size)) {
			ids[j] = rsrc_id;
			rsrc_id++;
			j++;
		}
		
		if ((i % 2) == 1)
			k++;
	}
 err_exit:
	lpfc_sli4_mbox_cmd_free(phba, mbox);
	return rc;
}

static int
lpfc_sli4_dealloc_extent(struct lpfc_hba *phba, uint16_t type)
{
	int rc;
	uint32_t length, mbox_tmo = 0;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_mbx_dealloc_rsrc_extents *dealloc_rsrc;
	struct lpfc_rsrc_blks *rsrc_blk, *rsrc_blk_next;

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	length = (sizeof(struct lpfc_mbx_dealloc_rsrc_extents) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_DEALLOC_RSRC_EXTENT,
			 length, LPFC_SLI4_MBX_EMBED);

	
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, 0, type,
					LPFC_SLI4_MBX_EMBED);
	if (unlikely(rc)) {
		rc = -EIO;
		goto out_free_mbox;
	}
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	if (unlikely(rc)) {
		rc = -EIO;
		goto out_free_mbox;
	}

	dealloc_rsrc = &mbox->u.mqe.un.dealloc_rsrc_extents;
	if (bf_get(lpfc_mbox_hdr_status,
		   &dealloc_rsrc->header.cfg_shdr.response)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_INIT,
				"2919 Failed to release resource extents "
				"for type %d - Status 0x%x Add'l Status 0x%x. "
				"Resource memory not released.\n",
				type,
				bf_get(lpfc_mbox_hdr_status,
				    &dealloc_rsrc->header.cfg_shdr.response),
				bf_get(lpfc_mbox_hdr_add_status,
				    &dealloc_rsrc->header.cfg_shdr.response));
		rc = -EIO;
		goto out_free_mbox;
	}

	
	switch (type) {
	case LPFC_RSC_TYPE_FCOE_VPI:
		kfree(phba->vpi_bmask);
		kfree(phba->vpi_ids);
		bf_set(lpfc_vpi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->lpfc_vpi_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		kfree(phba->sli4_hba.xri_bmask);
		kfree(phba->sli4_hba.xri_ids);
		bf_set(lpfc_xri_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->sli4_hba.lpfc_xri_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		kfree(phba->sli4_hba.vfi_bmask);
		kfree(phba->sli4_hba.vfi_ids);
		bf_set(lpfc_vfi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->sli4_hba.lpfc_vfi_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	case LPFC_RSC_TYPE_FCOE_RPI:
		
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->sli4_hba.lpfc_rpi_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	default:
		break;
	}

	bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);

 out_free_mbox:
	mempool_free(mbox, phba->mbox_mem_pool);
	return rc;
}

int
lpfc_sli4_alloc_resource_identifiers(struct lpfc_hba *phba)
{
	int i, rc, error = 0;
	uint16_t count, base;
	unsigned long longs;

	if (!phba->sli4_hba.rpi_hdrs_in_use)
		phba->sli4_hba.next_rpi = phba->sli4_hba.max_cfg_param.max_rpi;
	if (phba->sli4_hba.extents_in_use) {
		if (bf_get(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags) ==
		    LPFC_IDX_RSRC_RDY) {
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_VFI);
			if (rc != 0)
				error++;
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_VPI);
			if (rc != 0)
				error++;
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_XRI);
			if (rc != 0)
				error++;
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_RPI);
			if (rc != 0)
				error++;

			if (error) {
				lpfc_printf_log(phba, KERN_INFO,
						LOG_MBOX | LOG_INIT,
						"2931 Detected extent resource "
						"change.  Reallocating all "
						"extents.\n");
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_VFI);
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_VPI);
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_XRI);
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_RPI);
			} else
				return 0;
		}

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_VFI);
		if (unlikely(rc))
			goto err_exit;

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_VPI);
		if (unlikely(rc))
			goto err_exit;

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_RPI);
		if (unlikely(rc))
			goto err_exit;

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_XRI);
		if (unlikely(rc))
			goto err_exit;
		bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags,
		       LPFC_IDX_RSRC_RDY);
		return rc;
	} else {
		if (bf_get(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags) ==
		    LPFC_IDX_RSRC_RDY) {
			lpfc_sli4_dealloc_resource_identifiers(phba);
			lpfc_sli4_remove_rpis(phba);
		}
		
		count = phba->sli4_hba.max_cfg_param.max_rpi;
		base = phba->sli4_hba.max_cfg_param.rpi_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->sli4_hba.rpi_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.rpi_ids = kzalloc(count *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_ids)) {
			rc = -ENOMEM;
			goto free_rpi_bmask;
		}

		for (i = 0; i < count; i++)
			phba->sli4_hba.rpi_ids[i] = base + i;

		
		count = phba->sli4_hba.max_cfg_param.max_vpi;
		base = phba->sli4_hba.max_cfg_param.vpi_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->vpi_bmask = kzalloc(longs *
					  sizeof(unsigned long),
					  GFP_KERNEL);
		if (unlikely(!phba->vpi_bmask)) {
			rc = -ENOMEM;
			goto free_rpi_ids;
		}
		phba->vpi_ids = kzalloc(count *
					sizeof(uint16_t),
					GFP_KERNEL);
		if (unlikely(!phba->vpi_ids)) {
			rc = -ENOMEM;
			goto free_vpi_bmask;
		}

		for (i = 0; i < count; i++)
			phba->vpi_ids[i] = base + i;

		
		count = phba->sli4_hba.max_cfg_param.max_xri;
		base = phba->sli4_hba.max_cfg_param.xri_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->sli4_hba.xri_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_bmask)) {
			rc = -ENOMEM;
			goto free_vpi_ids;
		}
		phba->sli4_hba.max_cfg_param.xri_used = 0;
		phba->sli4_hba.xri_count = 0;
		phba->sli4_hba.xri_ids = kzalloc(count *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_ids)) {
			rc = -ENOMEM;
			goto free_xri_bmask;
		}

		for (i = 0; i < count; i++)
			phba->sli4_hba.xri_ids[i] = base + i;

		
		count = phba->sli4_hba.max_cfg_param.max_vfi;
		base = phba->sli4_hba.max_cfg_param.vfi_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->sli4_hba.vfi_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_bmask)) {
			rc = -ENOMEM;
			goto free_xri_ids;
		}
		phba->sli4_hba.vfi_ids = kzalloc(count *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_ids)) {
			rc = -ENOMEM;
			goto free_vfi_bmask;
		}

		for (i = 0; i < count; i++)
			phba->sli4_hba.vfi_ids[i] = base + i;

		bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags,
		       LPFC_IDX_RSRC_RDY);
		return 0;
	}

 free_vfi_bmask:
	kfree(phba->sli4_hba.vfi_bmask);
 free_xri_ids:
	kfree(phba->sli4_hba.xri_ids);
 free_xri_bmask:
	kfree(phba->sli4_hba.xri_bmask);
 free_vpi_ids:
	kfree(phba->vpi_ids);
 free_vpi_bmask:
	kfree(phba->vpi_bmask);
 free_rpi_ids:
	kfree(phba->sli4_hba.rpi_ids);
 free_rpi_bmask:
	kfree(phba->sli4_hba.rpi_bmask);
 err_exit:
	return rc;
}

int
lpfc_sli4_dealloc_resource_identifiers(struct lpfc_hba *phba)
{
	if (phba->sli4_hba.extents_in_use) {
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_VPI);
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_RPI);
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_XRI);
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_VFI);
	} else {
		kfree(phba->vpi_bmask);
		kfree(phba->vpi_ids);
		bf_set(lpfc_vpi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		kfree(phba->sli4_hba.xri_bmask);
		kfree(phba->sli4_hba.xri_ids);
		bf_set(lpfc_xri_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		kfree(phba->sli4_hba.vfi_bmask);
		kfree(phba->sli4_hba.vfi_ids);
		bf_set(lpfc_vfi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
	}

	return 0;
}

int
lpfc_sli4_get_allocated_extnts(struct lpfc_hba *phba, uint16_t type,
			       uint16_t *extnt_cnt, uint16_t *extnt_size)
{
	bool emb;
	int rc = 0;
	uint16_t curr_blks = 0;
	uint32_t req_len, emb_len;
	uint32_t alloc_len, mbox_tmo;
	struct list_head *blk_list_head;
	struct lpfc_rsrc_blks *rsrc_blk;
	LPFC_MBOXQ_t *mbox;
	void *virtaddr = NULL;
	struct lpfc_mbx_nembed_rsrc_extent *n_rsrc;
	struct lpfc_mbx_alloc_rsrc_extents *rsrc_ext;
	union  lpfc_sli4_cfg_shdr *shdr;

	switch (type) {
	case LPFC_RSC_TYPE_FCOE_VPI:
		blk_list_head = &phba->lpfc_vpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		blk_list_head = &phba->sli4_hba.lpfc_xri_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		blk_list_head = &phba->sli4_hba.lpfc_vfi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_RPI:
		blk_list_head = &phba->sli4_hba.lpfc_rpi_blk_list;
		break;
	default:
		return -EIO;
	}

	
	list_for_each_entry(rsrc_blk, blk_list_head, list) {
		if (curr_blks == 0) {
			*extnt_size = rsrc_blk->rsrc_size;
		}
		curr_blks++;
	}

	
	req_len = curr_blks * sizeof(uint16_t);

	emb_len = sizeof(MAILBOX_t) - sizeof(struct mbox_header) -
		sizeof(uint32_t);

	emb = LPFC_SLI4_MBX_EMBED;
	req_len = emb_len;
	if (req_len > emb_len) {
		req_len = curr_blks * sizeof(uint16_t) +
			sizeof(union lpfc_sli4_cfg_shdr) +
			sizeof(uint32_t);
		emb = LPFC_SLI4_MBX_NEMBED;
	}

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	memset(mbox, 0, sizeof(LPFC_MBOXQ_t));

	alloc_len = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
				     LPFC_MBOX_OPCODE_GET_ALLOC_RSRC_EXTENT,
				     req_len, emb);
	if (alloc_len < req_len) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2983 Allocated DMA memory size (x%x) is "
			"less than the requested DMA memory "
			"size (x%x)\n", alloc_len, req_len);
		rc = -ENOMEM;
		goto err_exit;
	}
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, curr_blks, type, emb);
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}

	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	if (emb == LPFC_SLI4_MBX_EMBED) {
		rsrc_ext = &mbox->u.mqe.un.alloc_rsrc_extents;
		shdr = &rsrc_ext->header.cfg_shdr;
		*extnt_cnt = bf_get(lpfc_mbx_rsrc_cnt, &rsrc_ext->u.rsp);
	} else {
		virtaddr = mbox->sge_array->addr[0];
		n_rsrc = (struct lpfc_mbx_nembed_rsrc_extent *) virtaddr;
		shdr = &n_rsrc->cfg_shdr;
		*extnt_cnt = bf_get(lpfc_mbx_rsrc_cnt, n_rsrc);
	}

	if (bf_get(lpfc_mbox_hdr_status, &shdr->response)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_INIT,
			"2984 Failed to read allocated resources "
			"for type %d - Status 0x%x Add'l Status 0x%x.\n",
			type,
			bf_get(lpfc_mbox_hdr_status, &shdr->response),
			bf_get(lpfc_mbox_hdr_add_status, &shdr->response));
		rc = -EIO;
		goto err_exit;
	}
 err_exit:
	lpfc_sli4_mbox_cmd_free(phba, mbox);
	return rc;
}

int
lpfc_sli4_hba_setup(struct lpfc_hba *phba)
{
	int rc;
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mqe *mqe;
	uint8_t *vpd;
	uint32_t vpd_size;
	uint32_t ftr_rsp = 0;
	struct Scsi_Host *shost = lpfc_shost_from_vport(phba->pport);
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_dmabuf *mp;

	
	rc = lpfc_pci_function_reset(phba);
	if (unlikely(rc))
		return -ENODEV;

	
	rc = lpfc_sli4_post_status_check(phba);
	if (unlikely(rc))
		return -ENODEV;
	else {
		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag |= LPFC_SLI_ACTIVE;
		spin_unlock_irq(&phba->hbalock);
	}

	mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	
	vpd_size = SLI4_PAGE_SIZE;
	vpd = kzalloc(vpd_size, GFP_KERNEL);
	if (!vpd) {
		rc = -ENOMEM;
		goto out_free_mbox;
	}

	rc = lpfc_sli4_read_rev(phba, mboxq, vpd, &vpd_size);
	if (unlikely(rc)) {
		kfree(vpd);
		goto out_free_mbox;
	}
	mqe = &mboxq->u.mqe;
	phba->sli_rev = bf_get(lpfc_mbx_rd_rev_sli_lvl, &mqe->un.read_rev);
	if (bf_get(lpfc_mbx_rd_rev_fcoe, &mqe->un.read_rev))
		phba->hba_flag |= HBA_FCOE_MODE;
	else
		phba->hba_flag &= ~HBA_FCOE_MODE;

	if (bf_get(lpfc_mbx_rd_rev_cee_ver, &mqe->un.read_rev) ==
		LPFC_DCBX_CEE_MODE)
		phba->hba_flag |= HBA_FIP_SUPPORT;
	else
		phba->hba_flag &= ~HBA_FIP_SUPPORT;

	if (phba->sli_rev != LPFC_SLI_REV4) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
			"0376 READ_REV Error. SLI Level %d "
			"FCoE enabled %d\n",
			phba->sli_rev, phba->hba_flag & HBA_FCOE_MODE);
		rc = -EIO;
		kfree(vpd);
		goto out_free_mbox;
	}

	if (phba->hba_flag & HBA_FCOE_MODE &&
	    lpfc_sli4_read_fcoe_params(phba))
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_INIT,
			"2570 Failed to read FCoE parameters\n");

	rc = lpfc_sli4_retrieve_pport_name(phba);
	if (!rc)
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"3080 Successful retrieving SLI4 device "
				"physical port name: %s.\n", phba->Port);

	rc = lpfc_parse_vpd(phba, vpd, vpd_size);
	if (unlikely(!rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0377 Error %d parsing vpd. "
				"Using defaults.\n", rc);
		rc = 0;
	}
	kfree(vpd);

	
	phba->vpd.rev.biuRev = mqe->un.read_rev.first_hw_rev;
	phba->vpd.rev.smRev = mqe->un.read_rev.second_hw_rev;
	phba->vpd.rev.endecRev = mqe->un.read_rev.third_hw_rev;
	phba->vpd.rev.fcphHigh = bf_get(lpfc_mbx_rd_rev_fcph_high,
					 &mqe->un.read_rev);
	phba->vpd.rev.fcphLow = bf_get(lpfc_mbx_rd_rev_fcph_low,
				       &mqe->un.read_rev);
	phba->vpd.rev.feaLevelHigh = bf_get(lpfc_mbx_rd_rev_ftr_lvl_high,
					    &mqe->un.read_rev);
	phba->vpd.rev.feaLevelLow = bf_get(lpfc_mbx_rd_rev_ftr_lvl_low,
					   &mqe->un.read_rev);
	phba->vpd.rev.sli1FwRev = mqe->un.read_rev.fw_id_rev;
	memcpy(phba->vpd.rev.sli1FwName, mqe->un.read_rev.fw_name, 16);
	phba->vpd.rev.sli2FwRev = mqe->un.read_rev.ulp_fw_id_rev;
	memcpy(phba->vpd.rev.sli2FwName, mqe->un.read_rev.ulp_fw_name, 16);
	phba->vpd.rev.opFwRev = mqe->un.read_rev.fw_id_rev;
	memcpy(phba->vpd.rev.opFwName, mqe->un.read_rev.fw_name, 16);
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0380 READ_REV Status x%x "
			"fw_rev:%s fcphHi:%x fcphLo:%x flHi:%x flLo:%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0,
			bf_get(lpfc_mqe_status, mqe),
			phba->vpd.rev.opFwName,
			phba->vpd.rev.fcphHigh, phba->vpd.rev.fcphLow,
			phba->vpd.rev.feaLevelHigh, phba->vpd.rev.feaLevelLow);

	lpfc_request_features(phba, mboxq);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (unlikely(rc)) {
		rc = -EIO;
		goto out_free_mbox;
	}

	if (!(bf_get(lpfc_mbx_rq_ftr_rsp_fcpi, &mqe->un.req_ftrs))) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"0378 No support for fcpi mode.\n");
		ftr_rsp++;
	}
	if (bf_get(lpfc_mbx_rq_ftr_rsp_perfh, &mqe->un.req_ftrs))
		phba->sli3_options |= LPFC_SLI4_PERFH_ENABLED;
	else
		phba->sli3_options &= ~LPFC_SLI4_PERFH_ENABLED;
	phba->sli3_options &= ~LPFC_SLI3_BG_ENABLED;
	if (phba->cfg_enable_bg) {
		if (bf_get(lpfc_mbx_rq_ftr_rsp_dif, &mqe->un.req_ftrs))
			phba->sli3_options |= LPFC_SLI3_BG_ENABLED;
		else
			ftr_rsp++;
	}

	if (phba->max_vpi && phba->cfg_enable_npiv &&
	    !(bf_get(lpfc_mbx_rq_ftr_rsp_npiv, &mqe->un.req_ftrs)))
		ftr_rsp++;

	if (ftr_rsp) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"0379 Feature Mismatch Data: x%08x %08x "
				"x%x x%x x%x\n", mqe->un.req_ftrs.word2,
				mqe->un.req_ftrs.word3, phba->cfg_enable_bg,
				phba->cfg_enable_npiv, phba->max_vpi);
		if (!(bf_get(lpfc_mbx_rq_ftr_rsp_dif, &mqe->un.req_ftrs)))
			phba->cfg_enable_bg = 0;
		if (!(bf_get(lpfc_mbx_rq_ftr_rsp_npiv, &mqe->un.req_ftrs)))
			phba->cfg_enable_npiv = 0;
	}

	
	spin_lock_irq(&phba->hbalock);
	phba->sli3_options |= (LPFC_SLI3_NPIV_ENABLED | LPFC_SLI3_HBQ_ENABLED);
	spin_unlock_irq(&phba->hbalock);

	rc = lpfc_sli4_alloc_resource_identifiers(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"2920 Failed to alloc Resource IDs "
				"rc = x%x\n", rc);
		goto out_free_mbox;
	}
	
	lpfc_scsi_buf_update(phba);

	
	rc = lpfc_read_sparam(phba, mboxq, vport->vpi);
	if (rc) {
		phba->link_state = LPFC_HBA_ERROR;
		rc = -ENOMEM;
		goto out_free_mbox;
	}

	mboxq->vport = vport;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	mp = (struct lpfc_dmabuf *) mboxq->context1;
	if (rc == MBX_SUCCESS) {
		memcpy(&vport->fc_sparam, mp->virt, sizeof(struct serv_parm));
		rc = 0;
	}

	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mboxq->context1 = NULL;
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0382 READ_SPARAM command failed "
				"status %d, mbxStatus x%x\n",
				rc, bf_get(lpfc_mqe_status, mqe));
		phba->link_state = LPFC_HBA_ERROR;
		rc = -EIO;
		goto out_free_mbox;
	}

	lpfc_update_vport_wwn(vport);

	
	fc_host_node_name(shost) = wwn_to_u64(vport->fc_nodename.u.wwn);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);

	
	if (!phba->sli4_hba.extents_in_use) {
		rc = lpfc_sli4_post_els_sgl_list(phba);
		if (unlikely(rc)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"0582 Error %d during els sgl post "
					"operation\n", rc);
			rc = -ENODEV;
			goto out_free_mbox;
		}
	} else {
		rc = lpfc_sli4_post_els_sgl_list_ext(phba);
		if (unlikely(rc)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"2560 Error %d during els sgl post "
					"operation\n", rc);
			rc = -ENODEV;
			goto out_free_mbox;
		}
	}

	
	rc = lpfc_sli4_repost_scsi_sgl_list(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0383 Error %d during scsi sgl post "
				"operation\n", rc);
		
		
		rc = -ENODEV;
		goto out_free_mbox;
	}

	
	rc = lpfc_sli4_post_all_rpi_hdrs(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0393 Error %d during rpi post operation\n",
				rc);
		rc = -ENODEV;
		goto out_free_mbox;
	}
	lpfc_sli4_node_prep(phba);

	
	rc = lpfc_sli4_queue_create(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3089 Failed to allocate queues\n");
		rc = -ENODEV;
		goto out_stop_timers;
	}
	
	rc = lpfc_sli4_queue_setup(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0381 Error %d during queue setup.\n ", rc);
		goto out_destroy_queue;
	}

	
	lpfc_sli4_arm_cqeq_intr(phba);

	
	phba->sli4_hba.intr_enable = 1;

	
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_sli4_rb_setup(phba);

	
	phba->fcf.fcf_flag = 0;
	phba->fcf.current_rec.flag = 0;

	
	mod_timer(&vport->els_tmofunc,
		  jiffies + HZ * (phba->fc_ratov * 2));

	
	mod_timer(&phba->hb_tmofunc,
		  jiffies + HZ * LPFC_HB_MBOX_INTERVAL);
	phba->hb_outstanding = 0;
	phba->last_completion_time = jiffies;

	
	mod_timer(&phba->eratt_poll, jiffies + HZ * LPFC_ERATT_POLL_INTERVAL);

	
	if (phba->cfg_aer_support == 1 && !(phba->hba_flag & HBA_AER_ENABLED)) {
		rc = pci_enable_pcie_error_reporting(phba->pcidev);
		if (!rc) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2829 This device supports "
					"Advanced Error Reporting (AER)\n");
			spin_lock_irq(&phba->hbalock);
			phba->hba_flag |= HBA_AER_ENABLED;
			spin_unlock_irq(&phba->hbalock);
		} else {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2830 This device does not support "
					"Advanced Error Reporting (AER)\n");
			phba->cfg_aer_support = 0;
		}
		rc = 0;
	}

	if (!(phba->hba_flag & HBA_FCOE_MODE)) {
		lpfc_reg_fcfi(phba, mboxq);
		mboxq->vport = phba->pport;
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc != MBX_SUCCESS)
			goto out_unset_queue;
		rc = 0;
		phba->fcf.fcfi = bf_get(lpfc_reg_fcfi_fcfi,
					&mboxq->u.mqe.un.reg_fcfi);

		
		lpfc_sli_read_link_ste(phba);
	}

	spin_lock_irq(&phba->hbalock);
	phba->link_state = LPFC_LINK_DOWN;
	spin_unlock_irq(&phba->hbalock);
	if (!(phba->hba_flag & HBA_FCOE_MODE) &&
	    (phba->hba_flag & LINK_DISABLED)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_SLI,
				"3103 Adapter Link is disabled.\n");
		lpfc_down_link(phba, mboxq);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_SLI,
					"3104 Adapter failed to issue "
					"DOWN_LINK mbox cmd, rc:x%x\n", rc);
			goto out_unset_queue;
		}
	} else if (phba->cfg_suppress_link_up == LPFC_INITIALIZE_LINK) {
		
		if (!(phba->link_flag & LS_LOOPBACK_MODE)) {
			rc = phba->lpfc_hba_init_link(phba, MBX_NOWAIT);
			if (rc)
				goto out_unset_queue;
		}
	}
	mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
out_unset_queue:
	
	lpfc_sli4_queue_unset(phba);
out_destroy_queue:
	lpfc_sli4_queue_destroy(phba);
out_stop_timers:
	lpfc_stop_hba_timers(phba);
out_free_mbox:
	mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
}

void
lpfc_mbox_timeout(unsigned long ptr)
{
	struct lpfc_hba  *phba = (struct lpfc_hba *) ptr;
	unsigned long iflag;
	uint32_t tmo_posted;

	spin_lock_irqsave(&phba->pport->work_port_lock, iflag);
	tmo_posted = phba->pport->work_port_events & WORKER_MBOX_TMO;
	if (!tmo_posted)
		phba->pport->work_port_events |= WORKER_MBOX_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflag);

	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}


void
lpfc_mbox_timeout_handler(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmbox = phba->sli.mbox_active;
	MAILBOX_t *mb = &pmbox->u.mb;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;

	spin_lock_irq(&phba->hbalock);
	if (pmbox == NULL) {
		lpfc_printf_log(phba, KERN_WARNING,
				LOG_MBOX | LOG_SLI,
				"0353 Active Mailbox cleared - mailbox timeout "
				"exiting\n");
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	
	lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
			"0310 Mailbox command x%x timeout Data: x%x x%x x%p\n",
			mb->mbxCommand,
			phba->pport->port_state,
			phba->sli.sli_flag,
			phba->sli.mbox_active);
	spin_unlock_irq(&phba->hbalock);

	spin_lock_irq(&phba->pport->work_port_lock);
	phba->pport->work_port_events &= ~WORKER_MBOX_TMO;
	spin_unlock_irq(&phba->pport->work_port_lock);
	spin_lock_irq(&phba->hbalock);
	phba->link_state = LPFC_LINK_UNKNOWN;
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	pring = &psli->ring[psli->fcp_ring];
	lpfc_sli_abort_iocb_ring(phba, pring);

	lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
			"0345 Resetting board due to mailbox timeout\n");

	
	lpfc_reset_hba(phba);
}

static int
lpfc_sli_issue_mbox_s3(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmbox,
		       uint32_t flag)
{
	MAILBOX_t *mb;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t status, evtctr;
	uint32_t ha_copy, hc_copy;
	int i;
	unsigned long timeout;
	unsigned long drvr_flag = 0;
	uint32_t word0, ldata;
	void __iomem *to_slim;
	int processing_queue = 0;

	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	if (!pmbox) {
		phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		
		if (unlikely(psli->sli_flag & LPFC_SLI_ASYNC_MBX_BLK)) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			return MBX_SUCCESS;
		}
		processing_queue = 1;
		pmbox = lpfc_mbox_get(phba);
		if (!pmbox) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			return MBX_SUCCESS;
		}
	}

	if (pmbox->mbox_cmpl && pmbox->mbox_cmpl != lpfc_sli_def_mbox_cmpl &&
		pmbox->mbox_cmpl != lpfc_sli_wake_mbox_wait) {
		if(!pmbox->vport) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			lpfc_printf_log(phba, KERN_ERR,
					LOG_MBOX | LOG_VPORT,
					"1806 Mbox x%x failed. No vport\n",
					pmbox->u.mb.mbxCommand);
			dump_stack();
			goto out_not_finished;
		}
	}

	
	if (unlikely(pci_channel_offline(phba->pcidev))) {
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
		goto out_not_finished;
	}

	
	if (unlikely(phba->hba_flag & DEFER_ERATT)) {
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
		goto out_not_finished;
	}

	psli = &phba->sli;

	mb = &pmbox->u.mb;
	status = MBX_SUCCESS;

	if (phba->link_state == LPFC_HBA_ERROR) {
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

		
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):0311 Mailbox command x%x cannot "
				"issue Data: x%x x%x\n",
				pmbox->vport ? pmbox->vport->vpi : 0,
				pmbox->u.mb.mbxCommand, psli->sli_flag, flag);
		goto out_not_finished;
	}

	if (mb->mbxCommand != MBX_KILL_BOARD && flag & MBX_NOWAIT) {
		if (lpfc_readl(phba->HCregaddr, &hc_copy) ||
			!(hc_copy & HC_MBINT_ENA)) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2528 Mailbox command x%x cannot "
				"issue Data: x%x x%x\n",
				pmbox->vport ? pmbox->vport->vpi : 0,
				pmbox->u.mb.mbxCommand, psli->sli_flag, flag);
			goto out_not_finished;
		}
	}

	if (psli->sli_flag & LPFC_SLI_MBOX_ACTIVE) {

		if (flag & MBX_POLL) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

			
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"(%d):2529 Mailbox command x%x "
					"cannot issue Data: x%x x%x\n",
					pmbox->vport ? pmbox->vport->vpi : 0,
					pmbox->u.mb.mbxCommand,
					psli->sli_flag, flag);
			goto out_not_finished;
		}

		if (!(psli->sli_flag & LPFC_SLI_ACTIVE)) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"(%d):2530 Mailbox command x%x "
					"cannot issue Data: x%x x%x\n",
					pmbox->vport ? pmbox->vport->vpi : 0,
					pmbox->u.mb.mbxCommand,
					psli->sli_flag, flag);
			goto out_not_finished;
		}

		lpfc_mbox_put(phba, pmbox);

		
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"(%d):0308 Mbox cmd issue - BUSY Data: "
				"x%x x%x x%x x%x\n",
				pmbox->vport ? pmbox->vport->vpi : 0xffffff,
				mb->mbxCommand, phba->pport->port_state,
				psli->sli_flag, flag);

		psli->slistat.mbox_busy++;
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

		if (pmbox->vport) {
			lpfc_debugfs_disc_trc(pmbox->vport,
				LPFC_DISC_TRC_MBOX_VPORT,
				"MBOX Bsy vport:  cmd:x%x mb:x%x x%x",
				(uint32_t)mb->mbxCommand,
				mb->un.varWords[0], mb->un.varWords[1]);
		}
		else {
			lpfc_debugfs_disc_trc(phba->pport,
				LPFC_DISC_TRC_MBOX,
				"MBOX Bsy:        cmd:x%x mb:x%x x%x",
				(uint32_t)mb->mbxCommand,
				mb->un.varWords[0], mb->un.varWords[1]);
		}

		return MBX_BUSY;
	}

	psli->sli_flag |= LPFC_SLI_MBOX_ACTIVE;

	
	if (flag != MBX_POLL) {
		if (!(psli->sli_flag & LPFC_SLI_ACTIVE) &&
		    (mb->mbxCommand != MBX_KILL_BOARD)) {
			psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"(%d):2531 Mailbox command x%x "
					"cannot issue Data: x%x x%x\n",
					pmbox->vport ? pmbox->vport->vpi : 0,
					pmbox->u.mb.mbxCommand,
					psli->sli_flag, flag);
			goto out_not_finished;
		}
		
		mod_timer(&psli->mbox_tmo, (jiffies +
			       (HZ * lpfc_mbox_tmo_val(phba, pmbox))));
	}

	
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0309 Mailbox cmd x%x issue Data: x%x x%x "
			"x%x\n",
			pmbox->vport ? pmbox->vport->vpi : 0,
			mb->mbxCommand, phba->pport->port_state,
			psli->sli_flag, flag);

	if (mb->mbxCommand != MBX_HEARTBEAT) {
		if (pmbox->vport) {
			lpfc_debugfs_disc_trc(pmbox->vport,
				LPFC_DISC_TRC_MBOX_VPORT,
				"MBOX Send vport: cmd:x%x mb:x%x x%x",
				(uint32_t)mb->mbxCommand,
				mb->un.varWords[0], mb->un.varWords[1]);
		}
		else {
			lpfc_debugfs_disc_trc(phba->pport,
				LPFC_DISC_TRC_MBOX,
				"MBOX Send:       cmd:x%x mb:x%x x%x",
				(uint32_t)mb->mbxCommand,
				mb->un.varWords[0], mb->un.varWords[1]);
		}
	}

	psli->slistat.mbox_cmd++;
	evtctr = psli->slistat.mbox_event;

	
	mb->mbxOwner = OWN_CHIP;

	if (psli->sli_flag & LPFC_SLI_ACTIVE) {
		
		if (pmbox->in_ext_byte_len || pmbox->out_ext_byte_len) {
			*(((uint32_t *)mb) + pmbox->mbox_offset_word)
				= (uint8_t *)phba->mbox_ext
				  - (uint8_t *)phba->mbox;
		}

		
		if (pmbox->in_ext_byte_len && pmbox->context2) {
			lpfc_sli_pcimem_bcopy(pmbox->context2,
				(uint8_t *)phba->mbox_ext,
				pmbox->in_ext_byte_len);
		}
		
		lpfc_sli_pcimem_bcopy(mb, phba->mbox, MAILBOX_CMD_SIZE);
	} else {
		
		if (pmbox->in_ext_byte_len || pmbox->out_ext_byte_len)
			*(((uint32_t *)mb) + pmbox->mbox_offset_word)
				= MAILBOX_HBA_EXT_OFFSET;

		
		if (pmbox->in_ext_byte_len && pmbox->context2) {
			lpfc_memcpy_to_slim(phba->MBslimaddr +
				MAILBOX_HBA_EXT_OFFSET,
				pmbox->context2, pmbox->in_ext_byte_len);

		}
		if (mb->mbxCommand == MBX_CONFIG_PORT) {
			
			lpfc_sli_pcimem_bcopy(mb, phba->mbox, MAILBOX_CMD_SIZE);
		}

		to_slim = phba->MBslimaddr + sizeof (uint32_t);
		lpfc_memcpy_to_slim(to_slim, &mb->un.varWords[0],
			    MAILBOX_CMD_SIZE - sizeof (uint32_t));

		
		ldata = *((uint32_t *)mb);
		to_slim = phba->MBslimaddr;
		writel(ldata, to_slim);
		readl(to_slim); 

		if (mb->mbxCommand == MBX_CONFIG_PORT) {
			
			psli->sli_flag |= LPFC_SLI_ACTIVE;
		}
	}

	wmb();

	switch (flag) {
	case MBX_NOWAIT:
		
		psli->mbox_active = pmbox;
		
		writel(CA_MBATT, phba->CAregaddr);
		readl(phba->CAregaddr); 
		
		break;

	case MBX_POLL:
		
		psli->mbox_active = NULL;
		
		writel(CA_MBATT, phba->CAregaddr);
		readl(phba->CAregaddr); 

		if (psli->sli_flag & LPFC_SLI_ACTIVE) {
			
			word0 = *((uint32_t *)phba->mbox);
			word0 = le32_to_cpu(word0);
		} else {
			
			if (lpfc_readl(phba->MBslimaddr, &word0)) {
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				goto out_not_finished;
			}
		}

		
		if (lpfc_readl(phba->HAregaddr, &ha_copy)) {
			spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
			goto out_not_finished;
		}
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba, pmbox) *
							1000) + jiffies;
		i = 0;
		
		while (((word0 & OWN_CHIP) == OWN_CHIP) ||
		       (!(ha_copy & HA_MBATT) &&
			(phba->link_state > LPFC_WARM_START))) {
			if (time_after(jiffies, timeout)) {
				psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				goto out_not_finished;
			}

			if (((word0 & OWN_CHIP) != OWN_CHIP)
			    && (evtctr != psli->slistat.mbox_event))
				break;

			if (i++ > 10) {
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				msleep(1);
				spin_lock_irqsave(&phba->hbalock, drvr_flag);
			}

			if (psli->sli_flag & LPFC_SLI_ACTIVE) {
				
				word0 = *((uint32_t *)phba->mbox);
				word0 = le32_to_cpu(word0);
				if (mb->mbxCommand == MBX_CONFIG_PORT) {
					MAILBOX_t *slimmb;
					uint32_t slimword0;
					
					slimword0 = readl(phba->MBslimaddr);
					slimmb = (MAILBOX_t *) & slimword0;
					if (((slimword0 & OWN_CHIP) != OWN_CHIP)
					    && slimmb->mbxStatus) {
						psli->sli_flag &=
						    ~LPFC_SLI_ACTIVE;
						word0 = slimword0;
					}
				}
			} else {
				
				word0 = readl(phba->MBslimaddr);
			}
			
			if (lpfc_readl(phba->HAregaddr, &ha_copy)) {
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				goto out_not_finished;
			}
		}

		if (psli->sli_flag & LPFC_SLI_ACTIVE) {
			
			lpfc_sli_pcimem_bcopy(phba->mbox, mb, MAILBOX_CMD_SIZE);
			
			if (pmbox->out_ext_byte_len && pmbox->context2) {
				lpfc_sli_pcimem_bcopy(phba->mbox_ext,
						      pmbox->context2,
						      pmbox->out_ext_byte_len);
			}
		} else {
			
			lpfc_memcpy_from_slim(mb, phba->MBslimaddr,
							MAILBOX_CMD_SIZE);
			
			if (pmbox->out_ext_byte_len && pmbox->context2) {
				lpfc_memcpy_from_slim(pmbox->context2,
					phba->MBslimaddr +
					MAILBOX_HBA_EXT_OFFSET,
					pmbox->out_ext_byte_len);
			}
		}

		writel(HA_MBATT, phba->HAregaddr);
		readl(phba->HAregaddr); 

		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		status = mb->mbxStatus;
	}

	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
	return status;

out_not_finished:
	if (processing_queue) {
		pmbox->u.mb.mbxStatus = MBX_NOT_FINISHED;
		lpfc_mbox_cmpl_put(phba, pmbox);
	}
	return MBX_NOT_FINISHED;
}

static int
lpfc_sli4_async_mbox_block(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	int rc = 0;
	unsigned long timeout = 0;

	
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_SLI_ASYNC_MBX_BLK;
	if (phba->sli.mbox_active)
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba,
						phba->sli.mbox_active) *
						1000) + jiffies;
	spin_unlock_irq(&phba->hbalock);

	
	while (phba->sli.mbox_active) {
		
		msleep(2);
		if (time_after(jiffies, timeout)) {
			
			rc = 1;
			break;
		}
	}

	
	if (rc) {
		spin_lock_irq(&phba->hbalock);
		psli->sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
		spin_unlock_irq(&phba->hbalock);
	}
	return rc;
}

static void
lpfc_sli4_async_mbox_unblock(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;

	spin_lock_irq(&phba->hbalock);
	if (!(psli->sli_flag & LPFC_SLI_ASYNC_MBX_BLK)) {
		
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	psli->sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
	spin_unlock_irq(&phba->hbalock);

	
	lpfc_worker_wake_up(phba);
}

static int
lpfc_sli4_post_sync_mbox(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	int rc = MBX_SUCCESS;
	unsigned long iflag;
	uint32_t db_ready;
	uint32_t mcqe_status;
	uint32_t mbx_cmnd;
	unsigned long timeout;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_mqe *mb = &mboxq->u.mqe;
	struct lpfc_bmbx_create *mbox_rgn;
	struct dma_address *dma_address;
	struct lpfc_register bmbx_reg;

	spin_lock_irqsave(&phba->hbalock, iflag);
	if (psli->sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2532 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, MBX_POLL);
		return MBXERR_ERROR;
	}
	
	psli->sli_flag |= LPFC_SLI_MBOX_ACTIVE;
	phba->sli.mbox_active = mboxq;
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	mbx_cmnd = bf_get(lpfc_mqe_command, mb);
	memset(phba->sli4_hba.bmbx.avirt, 0, sizeof(struct lpfc_bmbx_create));
	lpfc_sli_pcimem_bcopy(mb, phba->sli4_hba.bmbx.avirt,
			      sizeof(struct lpfc_mqe));

	
	dma_address = &phba->sli4_hba.bmbx.dma_address;
	writel(dma_address->addr_hi, phba->sli4_hba.BMBXregaddr);

	timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba, mboxq)
				   * 1000) + jiffies;
	do {
		bmbx_reg.word0 = readl(phba->sli4_hba.BMBXregaddr);
		db_ready = bf_get(lpfc_bmbx_rdy, &bmbx_reg);
		if (!db_ready)
			msleep(2);

		if (time_after(jiffies, timeout)) {
			rc = MBXERR_ERROR;
			goto exit;
		}
	} while (!db_ready);

	
	writel(dma_address->addr_lo, phba->sli4_hba.BMBXregaddr);
	timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba, mboxq)
				   * 1000) + jiffies;
	do {
		bmbx_reg.word0 = readl(phba->sli4_hba.BMBXregaddr);
		db_ready = bf_get(lpfc_bmbx_rdy, &bmbx_reg);
		if (!db_ready)
			msleep(2);

		if (time_after(jiffies, timeout)) {
			rc = MBXERR_ERROR;
			goto exit;
		}
	} while (!db_ready);

	lpfc_sli_pcimem_bcopy(phba->sli4_hba.bmbx.avirt, mb,
			      sizeof(struct lpfc_mqe));
	mbox_rgn = (struct lpfc_bmbx_create *) phba->sli4_hba.bmbx.avirt;
	lpfc_sli_pcimem_bcopy(&mbox_rgn->mcqe, &mboxq->mcqe,
			      sizeof(struct lpfc_mcqe));
	mcqe_status = bf_get(lpfc_mcqe_status, &mbox_rgn->mcqe);
	if (mcqe_status != MB_CQE_STATUS_SUCCESS) {
		if (bf_get(lpfc_mqe_status, mb) == MBX_SUCCESS)
			bf_set(lpfc_mqe_status, mb,
			       (LPFC_MBX_ERROR_RANGE | mcqe_status));
		rc = MBXERR_ERROR;
	} else
		lpfc_sli4_swap_str(phba, mboxq);

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0356 Mailbox cmd x%x (x%x/x%x) Status x%x "
			"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x x%x x%x"
			" x%x x%x CQ: x%x x%x x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0, mbx_cmnd,
			lpfc_sli_config_mbox_subsys_get(phba, mboxq),
			lpfc_sli_config_mbox_opcode_get(phba, mboxq),
			bf_get(lpfc_mqe_status, mb),
			mb->un.mb_words[0], mb->un.mb_words[1],
			mb->un.mb_words[2], mb->un.mb_words[3],
			mb->un.mb_words[4], mb->un.mb_words[5],
			mb->un.mb_words[6], mb->un.mb_words[7],
			mb->un.mb_words[8], mb->un.mb_words[9],
			mb->un.mb_words[10], mb->un.mb_words[11],
			mb->un.mb_words[12], mboxq->mcqe.word0,
			mboxq->mcqe.mcqe_tag0, 	mboxq->mcqe.mcqe_tag1,
			mboxq->mcqe.trailer);
exit:
	
	spin_lock_irqsave(&phba->hbalock, iflag);
	psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	phba->sli.mbox_active = NULL;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rc;
}

static int
lpfc_sli_issue_mbox_s4(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq,
		       uint32_t flag)
{
	struct lpfc_sli *psli = &phba->sli;
	unsigned long iflags;
	int rc;

	
	lpfc_idiag_mbxacc_dump_issue_mbox(phba, &mboxq->u.mb);

	rc = lpfc_mbox_dev_check(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2544 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, flag);
		goto out_not_finished;
	}

	
	if (!phba->sli4_hba.intr_enable) {
		if (flag == MBX_POLL)
			rc = lpfc_sli4_post_sync_mbox(phba, mboxq);
		else
			rc = -EIO;
		if (rc != MBX_SUCCESS)
			lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
					"(%d):2541 Mailbox command x%x "
					"(x%x/x%x) cannot issue Data: "
					"x%x x%x\n",
					mboxq->vport ? mboxq->vport->vpi : 0,
					mboxq->u.mb.mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									mboxq),
					lpfc_sli_config_mbox_opcode_get(phba,
									mboxq),
					psli->sli_flag, flag);
		return rc;
	} else if (flag == MBX_POLL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"(%d):2542 Try to issue mailbox command "
				"x%x (x%x/x%x) synchronously ahead of async"
				"mailbox command queue: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, flag);
		
		rc = lpfc_sli4_async_mbox_block(phba);
		if (!rc) {
			
			rc = lpfc_sli4_post_sync_mbox(phba, mboxq);
			if (rc != MBX_SUCCESS)
				lpfc_printf_log(phba, KERN_ERR,
					LOG_MBOX | LOG_SLI,
					"(%d):2597 Mailbox command "
					"x%x (x%x/x%x) cannot issue "
					"Data: x%x x%x\n",
					mboxq->vport ?
					mboxq->vport->vpi : 0,
					mboxq->u.mb.mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									mboxq),
					lpfc_sli_config_mbox_opcode_get(phba,
									mboxq),
					psli->sli_flag, flag);
			
			lpfc_sli4_async_mbox_unblock(phba);
		}
		return rc;
	}

	
	rc = lpfc_mbox_cmd_check(phba, mboxq);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2543 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, flag);
		goto out_not_finished;
	}

	
	psli->slistat.mbox_busy++;
	spin_lock_irqsave(&phba->hbalock, iflags);
	lpfc_mbox_put(phba, mboxq);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0354 Mbox cmd issue - Enqueue Data: "
			"x%x (x%x/x%x) x%x x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0xffffff,
			bf_get(lpfc_mqe_command, &mboxq->u.mqe),
			lpfc_sli_config_mbox_subsys_get(phba, mboxq),
			lpfc_sli_config_mbox_opcode_get(phba, mboxq),
			phba->pport->port_state,
			psli->sli_flag, MBX_NOWAIT);
	
	lpfc_worker_wake_up(phba);

	return MBX_BUSY;

out_not_finished:
	return MBX_NOT_FINISHED;
}

int
lpfc_sli4_post_async_mbox(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *mboxq;
	int rc = MBX_SUCCESS;
	unsigned long iflags;
	struct lpfc_mqe *mqe;
	uint32_t mbx_cmnd;

	
	if (unlikely(!phba->sli4_hba.intr_enable))
		return MBX_NOT_FINISHED;

	
	spin_lock_irqsave(&phba->hbalock, iflags);
	if (unlikely(psli->sli_flag & LPFC_SLI_ASYNC_MBX_BLK)) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return MBX_NOT_FINISHED;
	}
	if (psli->sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return MBX_NOT_FINISHED;
	}
	if (unlikely(phba->sli.mbox_active)) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0384 There is pending active mailbox cmd\n");
		return MBX_NOT_FINISHED;
	}
	
	psli->sli_flag |= LPFC_SLI_MBOX_ACTIVE;

	
	mboxq = lpfc_mbox_get(phba);

	
	if (!mboxq) {
		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return MBX_SUCCESS;
	}
	phba->sli.mbox_active = mboxq;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	
	rc = lpfc_mbox_dev_check(phba);
	if (unlikely(rc))
		
		goto out_not_finished;

	
	mqe = &mboxq->u.mqe;
	mbx_cmnd = bf_get(lpfc_mqe_command, mqe);

	
	mod_timer(&psli->mbox_tmo, (jiffies +
		  (HZ * lpfc_mbox_tmo_val(phba, mboxq))));

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0355 Mailbox cmd x%x (x%x/x%x) issue Data: "
			"x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0, mbx_cmnd,
			lpfc_sli_config_mbox_subsys_get(phba, mboxq),
			lpfc_sli_config_mbox_opcode_get(phba, mboxq),
			phba->pport->port_state, psli->sli_flag);

	if (mbx_cmnd != MBX_HEARTBEAT) {
		if (mboxq->vport) {
			lpfc_debugfs_disc_trc(mboxq->vport,
				LPFC_DISC_TRC_MBOX_VPORT,
				"MBOX Send vport: cmd:x%x mb:x%x x%x",
				mbx_cmnd, mqe->un.mb_words[0],
				mqe->un.mb_words[1]);
		} else {
			lpfc_debugfs_disc_trc(phba->pport,
				LPFC_DISC_TRC_MBOX,
				"MBOX Send: cmd:x%x mb:x%x x%x",
				mbx_cmnd, mqe->un.mb_words[0],
				mqe->un.mb_words[1]);
		}
	}
	psli->slistat.mbox_cmd++;

	
	rc = lpfc_sli4_mq_put(phba->sli4_hba.mbx_wq, mqe);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2533 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, MBX_NOWAIT);
		goto out_not_finished;
	}

	return rc;

out_not_finished:
	spin_lock_irqsave(&phba->hbalock, iflags);
	if (phba->sli.mbox_active) {
		mboxq->u.mb.mbxStatus = MBX_NOT_FINISHED;
		__lpfc_mbox_cmpl_put(phba, mboxq);
		
		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		phba->sli.mbox_active = NULL;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return MBX_NOT_FINISHED;
}

int
lpfc_sli_issue_mbox(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmbox, uint32_t flag)
{
	return phba->lpfc_sli_issue_mbox(phba, pmbox, flag);
}

int
lpfc_mbox_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{

	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->lpfc_sli_issue_mbox = lpfc_sli_issue_mbox_s3;
		phba->lpfc_sli_handle_slow_ring_event =
				lpfc_sli_handle_slow_ring_event_s3;
		phba->lpfc_sli_hbq_to_firmware = lpfc_sli_hbq_to_firmware_s3;
		phba->lpfc_sli_brdrestart = lpfc_sli_brdrestart_s3;
		phba->lpfc_sli_brdready = lpfc_sli_brdready_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->lpfc_sli_issue_mbox = lpfc_sli_issue_mbox_s4;
		phba->lpfc_sli_handle_slow_ring_event =
				lpfc_sli_handle_slow_ring_event_s4;
		phba->lpfc_sli_hbq_to_firmware = lpfc_sli_hbq_to_firmware_s4;
		phba->lpfc_sli_brdrestart = lpfc_sli_brdrestart_s4;
		phba->lpfc_sli_brdready = lpfc_sli_brdready_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1420 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
		break;
	}
	return 0;
}

void
__lpfc_sli_ringtx_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		    struct lpfc_iocbq *piocb)
{
	
	list_add_tail(&piocb->list, &pring->txq);
	pring->txq_cnt++;
}

static struct lpfc_iocbq *
lpfc_sli_next_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		   struct lpfc_iocbq **piocb)
{
	struct lpfc_iocbq * nextiocb;

	nextiocb = lpfc_sli_ringtx_get(phba, pring);
	if (!nextiocb) {
		nextiocb = *piocb;
		*piocb = NULL;
	}

	return nextiocb;
}

static int
__lpfc_sli_issue_iocb_s3(struct lpfc_hba *phba, uint32_t ring_number,
		    struct lpfc_iocbq *piocb, uint32_t flag)
{
	struct lpfc_iocbq *nextiocb;
	IOCB_t *iocb;
	struct lpfc_sli_ring *pring = &phba->sli.ring[ring_number];

	if (piocb->iocb_cmpl && (!piocb->vport) &&
	   (piocb->iocb.ulpCommand != CMD_ABORT_XRI_CN) &&
	   (piocb->iocb.ulpCommand != CMD_CLOSE_XRI_CN)) {
		lpfc_printf_log(phba, KERN_ERR,
				LOG_SLI | LOG_VPORT,
				"1807 IOCB x%x failed. No vport\n",
				piocb->iocb.ulpCommand);
		dump_stack();
		return IOCB_ERROR;
	}


	
	if (unlikely(pci_channel_offline(phba->pcidev)))
		return IOCB_ERROR;

	
	if (unlikely(phba->hba_flag & DEFER_ERATT))
		return IOCB_ERROR;

	if (unlikely(phba->link_state < LPFC_LINK_DOWN))
		return IOCB_ERROR;

	if (unlikely(pring->flag & LPFC_STOP_IOCB_EVENT))
		goto iocb_busy;

	if (unlikely(phba->link_state == LPFC_LINK_DOWN)) {
		switch (piocb->iocb.ulpCommand) {
		case CMD_GEN_REQUEST64_CR:
		case CMD_GEN_REQUEST64_CX:
			if (!(phba->sli.sli_flag & LPFC_MENLO_MAINT) ||
				(piocb->iocb.un.genreq64.w5.hcsw.Rctl !=
					FC_RCTL_DD_UNSOL_CMD) ||
				(piocb->iocb.un.genreq64.w5.hcsw.Type !=
					MENLO_TRANSPORT_TYPE))

				goto iocb_busy;
			break;
		case CMD_QUE_RING_BUF_CN:
		case CMD_QUE_RING_BUF64_CN:
			if (piocb->iocb_cmpl)
				piocb->iocb_cmpl = NULL;
			
		case CMD_CREATE_XRI_CR:
		case CMD_CLOSE_XRI_CN:
		case CMD_CLOSE_XRI_CX:
			break;
		default:
			goto iocb_busy;
		}

	} else if (unlikely(pring->ringno == phba->sli.fcp_ring &&
			    !(phba->sli.sli_flag & LPFC_PROCESS_LA))) {
		goto iocb_busy;
	}

	while ((iocb = lpfc_sli_next_iocb_slot(phba, pring)) &&
	       (nextiocb = lpfc_sli_next_iocb(phba, pring, &piocb)))
		lpfc_sli_submit_iocb(phba, pring, iocb, nextiocb);

	if (iocb)
		lpfc_sli_update_ring(phba, pring);
	else
		lpfc_sli_update_full_ring(phba, pring);

	if (!piocb)
		return IOCB_SUCCESS;

	goto out_busy;

 iocb_busy:
	pring->stats.iocb_cmd_delay++;

 out_busy:

	if (!(flag & SLI_IOCB_RET_IOCB)) {
		__lpfc_sli_ringtx_put(phba, pring, piocb);
		return IOCB_SUCCESS;
	}

	return IOCB_BUSY;
}

static uint16_t
lpfc_sli4_bpl2sgl(struct lpfc_hba *phba, struct lpfc_iocbq *piocbq,
		struct lpfc_sglq *sglq)
{
	uint16_t xritag = NO_XRI;
	struct ulp_bde64 *bpl = NULL;
	struct ulp_bde64 bde;
	struct sli4_sge *sgl  = NULL;
	struct lpfc_dmabuf *dmabuf;
	IOCB_t *icmd;
	int numBdes = 0;
	int i = 0;
	uint32_t offset = 0; 
	int inbound = 0; 

	if (!piocbq || !sglq)
		return xritag;

	sgl  = (struct sli4_sge *)sglq->sgl;
	icmd = &piocbq->iocb;
	if (icmd->ulpCommand == CMD_XMIT_BLS_RSP64_CX)
		return sglq->sli4_xritag;
	if (icmd->un.genreq64.bdl.bdeFlags == BUFF_TYPE_BLP_64) {
		numBdes = icmd->un.genreq64.bdl.bdeSize /
				sizeof(struct ulp_bde64);
		if (piocbq->context3)
			dmabuf = (struct lpfc_dmabuf *)piocbq->context3;
		else
			return xritag;

		bpl  = (struct ulp_bde64 *)dmabuf->virt;
		if (!bpl)
			return xritag;

		for (i = 0; i < numBdes; i++) {
			
			sgl->addr_hi = bpl->addrHigh;
			sgl->addr_lo = bpl->addrLow;

			sgl->word2 = le32_to_cpu(sgl->word2);
			if ((i+1) == numBdes)
				bf_set(lpfc_sli4_sge_last, sgl, 1);
			else
				bf_set(lpfc_sli4_sge_last, sgl, 0);
			bde.tus.w = le32_to_cpu(bpl->tus.w);
			sgl->sge_len = cpu_to_le32(bde.tus.f.bdeSize);
			if (piocbq->iocb.ulpCommand == CMD_GEN_REQUEST64_CR) {
				
				if (bpl->tus.f.bdeFlags == BUFF_TYPE_BDE_64I)
					inbound++;
				
				if (inbound == 1)
					offset = 0;
				bf_set(lpfc_sli4_sge_offset, sgl, offset);
				bf_set(lpfc_sli4_sge_type, sgl,
					LPFC_SGE_TYPE_DATA);
				offset += bde.tus.f.bdeSize;
			}
			sgl->word2 = cpu_to_le32(sgl->word2);
			bpl++;
			sgl++;
		}
	} else if (icmd->un.genreq64.bdl.bdeFlags == BUFF_TYPE_BDE_64) {
			sgl->addr_hi =
				cpu_to_le32(icmd->un.genreq64.bdl.addrHigh);
			sgl->addr_lo =
				cpu_to_le32(icmd->un.genreq64.bdl.addrLow);
			sgl->word2 = le32_to_cpu(sgl->word2);
			bf_set(lpfc_sli4_sge_last, sgl, 1);
			sgl->word2 = cpu_to_le32(sgl->word2);
			sgl->sge_len =
				cpu_to_le32(icmd->un.genreq64.bdl.bdeSize);
	}
	return sglq->sli4_xritag;
}

static uint32_t
lpfc_sli4_scmd_to_wqidx_distr(struct lpfc_hba *phba)
{
	++phba->fcp_qidx;
	if (phba->fcp_qidx >= phba->cfg_fcp_wq_count)
		phba->fcp_qidx = 0;

	return phba->fcp_qidx;
}

static int
lpfc_sli4_iocb2wqe(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq,
		union lpfc_wqe *wqe)
{
	uint32_t xmit_len = 0, total_len = 0;
	uint8_t ct = 0;
	uint32_t fip;
	uint32_t abort_tag;
	uint8_t command_type = ELS_COMMAND_NON_FIP;
	uint8_t cmnd;
	uint16_t xritag;
	uint16_t abrt_iotag;
	struct lpfc_iocbq *abrtiocbq;
	struct ulp_bde64 *bpl = NULL;
	uint32_t els_id = LPFC_ELS_ID_DEFAULT;
	int numBdes, i;
	struct ulp_bde64 bde;
	struct lpfc_nodelist *ndlp;
	uint32_t *pcmd;
	uint32_t if_type;

	fip = phba->hba_flag & HBA_FIP_SUPPORT;
	
	if (iocbq->iocb_flag &  LPFC_IO_FCP)
		command_type = FCP_COMMAND;
	else if (fip && (iocbq->iocb_flag & LPFC_FIP_ELS_ID_MASK))
		command_type = ELS_COMMAND_FIP;
	else
		command_type = ELS_COMMAND_NON_FIP;

	
	memcpy(wqe, &iocbq->iocb, sizeof(union lpfc_wqe));
	abort_tag = (uint32_t) iocbq->iotag;
	xritag = iocbq->sli4_xritag;
	wqe->generic.wqe_com.word7 = 0; 
	
	if (iocbq->iocb.un.genreq64.bdl.bdeFlags == BUFF_TYPE_BLP_64) {
		numBdes = iocbq->iocb.un.genreq64.bdl.bdeSize /
				sizeof(struct ulp_bde64);
		bpl  = (struct ulp_bde64 *)
			((struct lpfc_dmabuf *)iocbq->context3)->virt;
		if (!bpl)
			return IOCB_ERROR;

		
		wqe->generic.bde.addrHigh =  le32_to_cpu(bpl->addrHigh);
		wqe->generic.bde.addrLow =  le32_to_cpu(bpl->addrLow);
		wqe->generic.bde.tus.w  = le32_to_cpu(bpl->tus.w);
		xmit_len = wqe->generic.bde.tus.f.bdeSize;
		total_len = 0;
		for (i = 0; i < numBdes; i++) {
			bde.tus.w  = le32_to_cpu(bpl[i].tus.w);
			total_len += bde.tus.f.bdeSize;
		}
	} else
		xmit_len = iocbq->iocb.un.fcpi64.bdl.bdeSize;

	iocbq->iocb.ulpIoTag = iocbq->iotag;
	cmnd = iocbq->iocb.ulpCommand;

	switch (iocbq->iocb.ulpCommand) {
	case CMD_ELS_REQUEST64_CR:
		ndlp = (struct lpfc_nodelist *)iocbq->context1;
		if (!iocbq->iocb.ulpLe) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2007 Only Limited Edition cmd Format"
				" supported 0x%x\n",
				iocbq->iocb.ulpCommand);
			return IOCB_ERROR;
		}

		wqe->els_req.payload_len = xmit_len;
		
		bf_set(wqe_tmo, &wqe->els_req.wqe_com,
			iocbq->iocb.ulpTimeout);
		
		bf_set(els_req64_vf, &wqe->els_req, 0);
		
		bf_set(els_req64_vfid, &wqe->els_req, 0);
		ct = ((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l);
		bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
		       iocbq->iocb.ulpContext);
		bf_set(wqe_ct, &wqe->els_req.wqe_com, ct);
		bf_set(wqe_pu, &wqe->els_req.wqe_com, 0);
		
		if (command_type == ELS_COMMAND_FIP)
			els_id = ((iocbq->iocb_flag & LPFC_FIP_ELS_ID_MASK)
					>> LPFC_FIP_ELS_ID_SHIFT);
		pcmd = (uint32_t *) (((struct lpfc_dmabuf *)
					iocbq->context2)->virt);
		if_type = bf_get(lpfc_sli_intf_if_type,
					&phba->sli4_hba.sli_intf);
		if (if_type == LPFC_SLI_INTF_IF_TYPE_2) {
			if (pcmd && (*pcmd == ELS_CMD_FLOGI ||
				*pcmd == ELS_CMD_SCR ||
				*pcmd == ELS_CMD_FDISC ||
				*pcmd == ELS_CMD_LOGO ||
				*pcmd == ELS_CMD_PLOGI)) {
				bf_set(els_req64_sp, &wqe->els_req, 1);
				bf_set(els_req64_sid, &wqe->els_req,
					iocbq->vport->fc_myDID);
				bf_set(wqe_ct, &wqe->els_req.wqe_com, 1);
				bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
					phba->vpi_ids[phba->pport->vpi]);
			} else if (pcmd && iocbq->context1) {
				bf_set(wqe_ct, &wqe->els_req.wqe_com, 0);
				bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
					phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);
			}
		}
		bf_set(wqe_temp_rpi, &wqe->els_req.wqe_com,
		       phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);
		bf_set(wqe_els_id, &wqe->els_req.wqe_com, els_id);
		bf_set(wqe_dbde, &wqe->els_req.wqe_com, 1);
		bf_set(wqe_iod, &wqe->els_req.wqe_com, LPFC_WQE_IOD_READ);
		bf_set(wqe_qosd, &wqe->els_req.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->els_req.wqe_com, LPFC_WQE_LENLOC_NONE);
		bf_set(wqe_ebde_cnt, &wqe->els_req.wqe_com, 0);
		break;
	case CMD_XMIT_SEQUENCE64_CX:
		bf_set(wqe_ctxt_tag, &wqe->xmit_sequence.wqe_com,
		       iocbq->iocb.un.ulpWord[3]);
		bf_set(wqe_rcvoxid, &wqe->xmit_sequence.wqe_com,
		       iocbq->iocb.unsli3.rcvsli3.ox_id);
		
		xmit_len = total_len;
		cmnd = CMD_XMIT_SEQUENCE64_CR;
		if (phba->link_flag & LS_LOOPBACK_MODE)
			bf_set(wqe_xo, &wqe->xmit_sequence.wge_ctl, 1);
	case CMD_XMIT_SEQUENCE64_CR:
		
		wqe->xmit_sequence.rsvd3 = 0;
		
		
		bf_set(wqe_pu, &wqe->xmit_sequence.wqe_com, 0);
		bf_set(wqe_dbde, &wqe->xmit_sequence.wqe_com, 1);
		bf_set(wqe_iod, &wqe->xmit_sequence.wqe_com,
		       LPFC_WQE_IOD_WRITE);
		bf_set(wqe_lenloc, &wqe->xmit_sequence.wqe_com,
		       LPFC_WQE_LENLOC_WORD12);
		bf_set(wqe_ebde_cnt, &wqe->xmit_sequence.wqe_com, 0);
		wqe->xmit_sequence.xmit_len = xmit_len;
		command_type = OTHER_COMMAND;
		break;
	case CMD_XMIT_BCAST64_CN:
		
		wqe->xmit_bcast64.seq_payload_len = xmit_len;
		
		
		
		bf_set(wqe_ct, &wqe->xmit_bcast64.wqe_com,
			((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l));
		bf_set(wqe_dbde, &wqe->xmit_bcast64.wqe_com, 1);
		bf_set(wqe_iod, &wqe->xmit_bcast64.wqe_com, LPFC_WQE_IOD_WRITE);
		bf_set(wqe_lenloc, &wqe->xmit_bcast64.wqe_com,
		       LPFC_WQE_LENLOC_WORD3);
		bf_set(wqe_ebde_cnt, &wqe->xmit_bcast64.wqe_com, 0);
		break;
	case CMD_FCP_IWRITE64_CR:
		command_type = FCP_COMMAND_DATA_OUT;
		
		
		wqe->fcp_iwrite.payload_offset_len =
			xmit_len + sizeof(struct fcp_rsp);
		
		
		bf_set(wqe_erp, &wqe->fcp_iwrite.wqe_com,
		       iocbq->iocb.ulpFCP2Rcvy);
		bf_set(wqe_lnk, &wqe->fcp_iwrite.wqe_com, iocbq->iocb.ulpXS);
		
		bf_set(wqe_xc, &wqe->fcp_iwrite.wqe_com, 0);
		bf_set(wqe_iod, &wqe->fcp_iwrite.wqe_com, LPFC_WQE_IOD_WRITE);
		bf_set(wqe_lenloc, &wqe->fcp_iwrite.wqe_com,
		       LPFC_WQE_LENLOC_WORD4);
		bf_set(wqe_ebde_cnt, &wqe->fcp_iwrite.wqe_com, 0);
		bf_set(wqe_pu, &wqe->fcp_iwrite.wqe_com, iocbq->iocb.ulpPU);
		if (iocbq->iocb_flag & LPFC_IO_DIF) {
			iocbq->iocb_flag &= ~LPFC_IO_DIF;
			bf_set(wqe_dif, &wqe->generic.wqe_com, 1);
		}
		bf_set(wqe_dbde, &wqe->fcp_iwrite.wqe_com, 1);
		break;
	case CMD_FCP_IREAD64_CR:
		
		
		wqe->fcp_iread.payload_offset_len =
			xmit_len + sizeof(struct fcp_rsp);
		
		
		bf_set(wqe_erp, &wqe->fcp_iread.wqe_com,
		       iocbq->iocb.ulpFCP2Rcvy);
		bf_set(wqe_lnk, &wqe->fcp_iread.wqe_com, iocbq->iocb.ulpXS);
		
		bf_set(wqe_xc, &wqe->fcp_iread.wqe_com, 0);
		bf_set(wqe_iod, &wqe->fcp_iread.wqe_com, LPFC_WQE_IOD_READ);
		bf_set(wqe_lenloc, &wqe->fcp_iread.wqe_com,
		       LPFC_WQE_LENLOC_WORD4);
		bf_set(wqe_ebde_cnt, &wqe->fcp_iread.wqe_com, 0);
		bf_set(wqe_pu, &wqe->fcp_iread.wqe_com, iocbq->iocb.ulpPU);
		if (iocbq->iocb_flag & LPFC_IO_DIF) {
			iocbq->iocb_flag &= ~LPFC_IO_DIF;
			bf_set(wqe_dif, &wqe->generic.wqe_com, 1);
		}
		bf_set(wqe_dbde, &wqe->fcp_iread.wqe_com, 1);
		break;
	case CMD_FCP_ICMND64_CR:
		
		wqe->fcp_icmd.rsrvd3 = 0;
		bf_set(wqe_pu, &wqe->fcp_icmd.wqe_com, 0);
		
		bf_set(wqe_xc, &wqe->fcp_icmd.wqe_com, 0);
		bf_set(wqe_dbde, &wqe->fcp_icmd.wqe_com, 1);
		bf_set(wqe_iod, &wqe->fcp_icmd.wqe_com, LPFC_WQE_IOD_WRITE);
		bf_set(wqe_qosd, &wqe->fcp_icmd.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->fcp_icmd.wqe_com,
		       LPFC_WQE_LENLOC_NONE);
		bf_set(wqe_ebde_cnt, &wqe->fcp_icmd.wqe_com, 0);
		break;
	case CMD_GEN_REQUEST64_CR:
		xmit_len = 0;
		numBdes = iocbq->iocb.un.genreq64.bdl.bdeSize /
			sizeof(struct ulp_bde64);
		for (i = 0; i < numBdes; i++) {
			bde.tus.w = le32_to_cpu(bpl[i].tus.w);
			if (bde.tus.f.bdeFlags != BUFF_TYPE_BDE_64)
				break;
			xmit_len += bde.tus.f.bdeSize;
		}
		
		wqe->gen_req.request_payload_len = xmit_len;
		
		
		
		if (iocbq->iocb.ulpCt_h  || iocbq->iocb.ulpCt_l) {
			ct = ((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l);
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2015 Invalid CT %x command 0x%x\n",
				ct, iocbq->iocb.ulpCommand);
			return IOCB_ERROR;
		}
		bf_set(wqe_ct, &wqe->gen_req.wqe_com, 0);
		bf_set(wqe_tmo, &wqe->gen_req.wqe_com, iocbq->iocb.ulpTimeout);
		bf_set(wqe_pu, &wqe->gen_req.wqe_com, iocbq->iocb.ulpPU);
		bf_set(wqe_dbde, &wqe->gen_req.wqe_com, 1);
		bf_set(wqe_iod, &wqe->gen_req.wqe_com, LPFC_WQE_IOD_READ);
		bf_set(wqe_qosd, &wqe->gen_req.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->gen_req.wqe_com, LPFC_WQE_LENLOC_NONE);
		bf_set(wqe_ebde_cnt, &wqe->gen_req.wqe_com, 0);
		command_type = OTHER_COMMAND;
		break;
	case CMD_XMIT_ELS_RSP64_CX:
		ndlp = (struct lpfc_nodelist *)iocbq->context1;
		
		
		wqe->xmit_els_rsp.response_payload_len = xmit_len;
		
		wqe->xmit_els_rsp.rsvd4 = 0;
		
		bf_set(wqe_els_did, &wqe->xmit_els_rsp.wqe_dest,
			 iocbq->iocb.un.elsreq64.remoteID);
		bf_set(wqe_ct, &wqe->xmit_els_rsp.wqe_com,
		       ((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l));
		bf_set(wqe_pu, &wqe->xmit_els_rsp.wqe_com, iocbq->iocb.ulpPU);
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       iocbq->iocb.unsli3.rcvsli3.ox_id);
		if (!iocbq->iocb.ulpCt_h && iocbq->iocb.ulpCt_l)
			bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com,
			       phba->vpi_ids[iocbq->vport->vpi]);
		bf_set(wqe_dbde, &wqe->xmit_els_rsp.wqe_com, 1);
		bf_set(wqe_iod, &wqe->xmit_els_rsp.wqe_com, LPFC_WQE_IOD_WRITE);
		bf_set(wqe_qosd, &wqe->xmit_els_rsp.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->xmit_els_rsp.wqe_com,
		       LPFC_WQE_LENLOC_WORD3);
		bf_set(wqe_ebde_cnt, &wqe->xmit_els_rsp.wqe_com, 0);
		bf_set(wqe_rsp_temp_rpi, &wqe->xmit_els_rsp,
		       phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);
		pcmd = (uint32_t *) (((struct lpfc_dmabuf *)
					iocbq->context2)->virt);
		if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
				bf_set(els_req64_sp, &wqe->els_req, 1);
				bf_set(els_req64_sid, &wqe->els_req,
					iocbq->vport->fc_myDID);
				bf_set(wqe_ct, &wqe->els_req.wqe_com, 1);
				bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
					phba->vpi_ids[phba->pport->vpi]);
		}
		command_type = OTHER_COMMAND;
		break;
	case CMD_CLOSE_XRI_CN:
	case CMD_ABORT_XRI_CN:
	case CMD_ABORT_XRI_CX:
		
		
		abrt_iotag = iocbq->iocb.un.acxri.abortContextTag;
		if (abrt_iotag != 0 && abrt_iotag <= phba->sli.last_iotag) {
			abrtiocbq = phba->sli.iocbq_lookup[abrt_iotag];
			fip = abrtiocbq->iocb_flag & LPFC_FIP_ELS_ID_MASK;
		} else
			fip = 0;

		if ((iocbq->iocb.ulpCommand == CMD_CLOSE_XRI_CN) || fip)
			bf_set(abort_cmd_ia, &wqe->abort_cmd, 1);
		else
			bf_set(abort_cmd_ia, &wqe->abort_cmd, 0);
		bf_set(abort_cmd_criteria, &wqe->abort_cmd, T_XRI_TAG);
		
		wqe->abort_cmd.rsrvd5 = 0;
		bf_set(wqe_ct, &wqe->abort_cmd.wqe_com,
			((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l));
		abort_tag = iocbq->iocb.un.acxri.abortIoTag;
		bf_set(wqe_cmnd, &wqe->abort_cmd.wqe_com, CMD_ABORT_XRI_CX);
		bf_set(wqe_qosd, &wqe->abort_cmd.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->abort_cmd.wqe_com,
		       LPFC_WQE_LENLOC_NONE);
		cmnd = CMD_ABORT_XRI_CX;
		command_type = OTHER_COMMAND;
		xritag = 0;
		break;
	case CMD_XMIT_BLS_RSP64_CX:
		ndlp = (struct lpfc_nodelist *)iocbq->context1;
		memset(wqe, 0, sizeof(union lpfc_wqe));
		
		bf_set(xmit_bls_rsp64_oxid, &wqe->xmit_bls_rsp,
		       bf_get(lpfc_abts_oxid, &iocbq->iocb.un.bls_rsp));
		if (bf_get(lpfc_abts_orig, &iocbq->iocb.un.bls_rsp) ==
		    LPFC_ABTS_UNSOL_INT) {
			bf_set(xmit_bls_rsp64_rxid, &wqe->xmit_bls_rsp,
			       iocbq->sli4_xritag);
		} else {
			bf_set(xmit_bls_rsp64_rxid, &wqe->xmit_bls_rsp,
			       bf_get(lpfc_abts_rxid, &iocbq->iocb.un.bls_rsp));
		}
		bf_set(xmit_bls_rsp64_seqcnthi, &wqe->xmit_bls_rsp, 0xffff);
		bf_set(wqe_xmit_bls_pt, &wqe->xmit_bls_rsp.wqe_dest, 0x1);

		
		bf_set(wqe_els_did, &wqe->xmit_bls_rsp.wqe_dest,
			ndlp->nlp_DID);
		bf_set(xmit_bls_rsp64_temprpi, &wqe->xmit_bls_rsp,
			iocbq->iocb.ulpContext);
		bf_set(wqe_ct, &wqe->xmit_bls_rsp.wqe_com, 1);
		bf_set(wqe_ctxt_tag, &wqe->xmit_bls_rsp.wqe_com,
			phba->vpi_ids[phba->pport->vpi]);
		bf_set(wqe_qosd, &wqe->xmit_bls_rsp.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->xmit_bls_rsp.wqe_com,
		       LPFC_WQE_LENLOC_NONE);
		
		command_type = OTHER_COMMAND;
		if (iocbq->iocb.un.xseq64.w5.hcsw.Rctl == FC_RCTL_BA_RJT) {
			bf_set(xmit_bls_rsp64_rjt_vspec, &wqe->xmit_bls_rsp,
			       bf_get(lpfc_vndr_code, &iocbq->iocb.un.bls_rsp));
			bf_set(xmit_bls_rsp64_rjt_expc, &wqe->xmit_bls_rsp,
			       bf_get(lpfc_rsn_expln, &iocbq->iocb.un.bls_rsp));
			bf_set(xmit_bls_rsp64_rjt_rsnc, &wqe->xmit_bls_rsp,
			       bf_get(lpfc_rsn_code, &iocbq->iocb.un.bls_rsp));
		}

		break;
	case CMD_XRI_ABORTED_CX:
	case CMD_CREATE_XRI_CR: 
	case CMD_IOCB_FCP_IBIDIR64_CR: 
	case CMD_FCP_TSEND64_CX: 
	case CMD_FCP_TRSP64_CX: 
	case CMD_FCP_AUTO_TRSP_CX: 
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2014 Invalid command 0x%x\n",
				iocbq->iocb.ulpCommand);
		return IOCB_ERROR;
		break;
	}

	bf_set(wqe_xri_tag, &wqe->generic.wqe_com, xritag);
	bf_set(wqe_reqtag, &wqe->generic.wqe_com, iocbq->iotag);
	wqe->generic.wqe_com.abort_tag = abort_tag;
	bf_set(wqe_cmd_type, &wqe->generic.wqe_com, command_type);
	bf_set(wqe_cmnd, &wqe->generic.wqe_com, cmnd);
	bf_set(wqe_class, &wqe->generic.wqe_com, iocbq->iocb.ulpClass);
	bf_set(wqe_cqid, &wqe->generic.wqe_com, LPFC_WQE_CQ_ID_DEFAULT);
	return 0;
}

static int
__lpfc_sli_issue_iocb_s4(struct lpfc_hba *phba, uint32_t ring_number,
			 struct lpfc_iocbq *piocb, uint32_t flag)
{
	struct lpfc_sglq *sglq;
	union lpfc_wqe wqe;
	struct lpfc_sli_ring *pring = &phba->sli.ring[ring_number];

	if (piocb->sli4_xritag == NO_XRI) {
		if (piocb->iocb.ulpCommand == CMD_ABORT_XRI_CN ||
		    piocb->iocb.ulpCommand == CMD_CLOSE_XRI_CN)
			sglq = NULL;
		else {
			if (pring->txq_cnt) {
				if (!(flag & SLI_IOCB_RET_IOCB)) {
					__lpfc_sli_ringtx_put(phba,
						pring, piocb);
					return IOCB_SUCCESS;
				} else {
					return IOCB_BUSY;
				}
			} else {
				sglq = __lpfc_sli_get_sglq(phba, piocb);
				if (!sglq) {
					if (!(flag & SLI_IOCB_RET_IOCB)) {
						__lpfc_sli_ringtx_put(phba,
								pring,
								piocb);
						return IOCB_SUCCESS;
					} else
						return IOCB_BUSY;
				}
			}
		}
	} else if (piocb->iocb_flag &  LPFC_IO_FCP) {
		
		sglq = NULL;
	} else {
		sglq = __lpfc_get_active_sglq(phba, piocb->sli4_xritag);
		if (!sglq)
			return IOCB_ERROR;
	}

	if (sglq) {
		piocb->sli4_lxritag = sglq->sli4_lxritag;
		piocb->sli4_xritag = sglq->sli4_xritag;
		if (NO_XRI == lpfc_sli4_bpl2sgl(phba, piocb, sglq))
			return IOCB_ERROR;
	}

	if (lpfc_sli4_iocb2wqe(phba, piocb, &wqe))
		return IOCB_ERROR;

	if ((piocb->iocb_flag & LPFC_IO_FCP) ||
		(piocb->iocb_flag & LPFC_USE_FCPWQIDX)) {
		if (piocb->iocb_flag & LPFC_IO_FCP)
			piocb->fcp_wqidx = lpfc_sli4_scmd_to_wqidx_distr(phba);
		if (unlikely(!phba->sli4_hba.fcp_wq))
			return IOCB_ERROR;
		if (lpfc_sli4_wq_put(phba->sli4_hba.fcp_wq[piocb->fcp_wqidx],
				     &wqe))
			return IOCB_ERROR;
	} else {
		if (lpfc_sli4_wq_put(phba->sli4_hba.els_wq, &wqe))
			return IOCB_ERROR;
	}
	lpfc_sli_ringtxcmpl_put(phba, pring, piocb);

	return 0;
}

int
__lpfc_sli_issue_iocb(struct lpfc_hba *phba, uint32_t ring_number,
		struct lpfc_iocbq *piocb, uint32_t flag)
{
	return phba->__lpfc_sli_issue_iocb(phba, ring_number, piocb, flag);
}

int
lpfc_sli_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{

	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->__lpfc_sli_issue_iocb = __lpfc_sli_issue_iocb_s3;
		phba->__lpfc_sli_release_iocbq = __lpfc_sli_release_iocbq_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->__lpfc_sli_issue_iocb = __lpfc_sli_issue_iocb_s4;
		phba->__lpfc_sli_release_iocbq = __lpfc_sli_release_iocbq_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1419 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
		break;
	}
	phba->lpfc_get_iocb_from_iocbq = lpfc_get_iocb_from_iocbq;
	return 0;
}

int
lpfc_sli_issue_iocb(struct lpfc_hba *phba, uint32_t ring_number,
		    struct lpfc_iocbq *piocb, uint32_t flag)
{
	unsigned long iflags;
	int rc;

	spin_lock_irqsave(&phba->hbalock, iflags);
	rc = __lpfc_sli_issue_iocb(phba, ring_number, piocb, flag);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return rc;
}

static int
lpfc_extra_ring_setup( struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;

	psli = &phba->sli;

	

	
	pring = &psli->ring[psli->fcp_ring];
	pring->numCiocb -= SLI2_IOCB_CMD_R1XTRA_ENTRIES;
	pring->numRiocb -= SLI2_IOCB_RSP_R1XTRA_ENTRIES;
	pring->numCiocb -= SLI2_IOCB_CMD_R3XTRA_ENTRIES;
	pring->numRiocb -= SLI2_IOCB_RSP_R3XTRA_ENTRIES;

	
	pring = &psli->ring[psli->extra_ring];

	pring->numCiocb += SLI2_IOCB_CMD_R1XTRA_ENTRIES;
	pring->numRiocb += SLI2_IOCB_RSP_R1XTRA_ENTRIES;
	pring->numCiocb += SLI2_IOCB_CMD_R3XTRA_ENTRIES;
	pring->numRiocb += SLI2_IOCB_RSP_R3XTRA_ENTRIES;

	
	pring->iotag_max = 4096;
	pring->num_mask = 1;
	pring->prt[0].profile = 0;      
	pring->prt[0].rctl = phba->cfg_multi_ring_rctl;
	pring->prt[0].type = phba->cfg_multi_ring_type;
	pring->prt[0].lpfc_sli_rcv_unsol_event = NULL;
	return 0;
}

static void
lpfc_sli_abts_recover_port(struct lpfc_vport *vport,
			   struct lpfc_nodelist *ndlp)
{
	struct Scsi_Host *shost;
	struct lpfc_hba *phba;
	unsigned long flags = 0;

	shost = lpfc_shost_from_vport(vport);
	phba = vport->phba;
	if (ndlp->nlp_state != NLP_STE_MAPPED_NODE) {
		lpfc_printf_log(phba, KERN_INFO,
			LOG_SLI, "3093 No rport recovery needed. "
			"rport in state 0x%x\n",
			ndlp->nlp_state);
		return;
	}
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"3094 Start rport recovery on shost id 0x%x "
			"fc_id 0x%06x vpi 0x%x rpi 0x%x state 0x%x "
			"flags 0x%x\n",
			shost->host_no, ndlp->nlp_DID,
			vport->vpi, ndlp->nlp_rpi, ndlp->nlp_state,
			ndlp->nlp_flag);
	spin_lock_irqsave(shost->host_lock, flags);
	ndlp->nlp_fcp_info &= ~NLP_FCP_2_DEVICE;
	spin_unlock_irqrestore(shost->host_lock, flags);
	lpfc_disc_state_machine(vport, ndlp, NULL,
				NLP_EVT_DEVICE_RECOVERY);
	lpfc_cancel_retry_delay_tmo(vport, ndlp);
	spin_lock_irqsave(shost->host_lock, flags);
	ndlp->nlp_flag |= NLP_NPR_2B_DISC;
	spin_unlock_irqrestore(shost->host_lock, flags);
	lpfc_disc_start(vport);
}

static void
lpfc_sli_abts_err_handler(struct lpfc_hba *phba,
			  struct lpfc_iocbq *iocbq)
{
	struct lpfc_nodelist *ndlp = NULL;
	uint16_t rpi = 0, vpi = 0;
	struct lpfc_vport *vport = NULL;

	
	vpi = iocbq->iocb.un.asyncstat.sub_ctxt_tag;
	rpi = iocbq->iocb.ulpContext;

	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"3092 Port generated ABTS async event "
			"on vpi %d rpi %d status 0x%x\n",
			vpi, rpi, iocbq->iocb.ulpStatus);

	vport = lpfc_find_vport_by_vpid(phba, vpi);
	if (!vport)
		goto err_exit;
	ndlp = lpfc_findnode_rpi(vport, rpi);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		goto err_exit;

	if (iocbq->iocb.ulpStatus == IOSTAT_LOCAL_REJECT)
		lpfc_sli_abts_recover_port(vport, ndlp);
	return;

 err_exit:
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3095 Event Context not found, no "
			"action on vpi %d rpi %d status 0x%x, reason 0x%x\n",
			iocbq->iocb.ulpContext, iocbq->iocb.ulpStatus,
			vpi, rpi);
}

void
lpfc_sli4_abts_err_handler(struct lpfc_hba *phba,
			   struct lpfc_nodelist *ndlp,
			   struct sli4_wcqe_xri_aborted *axri)
{
	struct lpfc_vport *vport;
	uint32_t ext_status = 0;

	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3115 Node Context not found, driver "
				"ignoring abts err event\n");
		return;
	}

	vport = ndlp->vport;
	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"3116 Port generated FCP XRI ABORT event on "
			"vpi %d rpi %d xri x%x status 0x%x parameter x%x\n",
			ndlp->vport->vpi, ndlp->nlp_rpi,
			bf_get(lpfc_wcqe_xa_xri, axri),
			bf_get(lpfc_wcqe_xa_status, axri),
			axri->parameter);

	ext_status = axri->parameter & WCQE_PARAM_MASK;
	if ((bf_get(lpfc_wcqe_xa_status, axri) == IOSTAT_LOCAL_REJECT) &&
	    ((ext_status == IOERR_SEQUENCE_TIMEOUT) || (ext_status == 0)))
		lpfc_sli_abts_recover_port(vport, ndlp);
}

static void
lpfc_sli_async_event_handler(struct lpfc_hba * phba,
	struct lpfc_sli_ring * pring, struct lpfc_iocbq * iocbq)
{
	IOCB_t *icmd;
	uint16_t evt_code;
	struct temp_event temp_event_data;
	struct Scsi_Host *shost;
	uint32_t *iocb_w;

	icmd = &iocbq->iocb;
	evt_code = icmd->un.asyncstat.evt_code;

	switch (evt_code) {
	case ASYNC_TEMP_WARN:
	case ASYNC_TEMP_SAFE:
		temp_event_data.data = (uint32_t) icmd->ulpContext;
		temp_event_data.event_type = FC_REG_TEMPERATURE_EVENT;
		if (evt_code == ASYNC_TEMP_WARN) {
			temp_event_data.event_code = LPFC_THRESHOLD_TEMP;
			lpfc_printf_log(phba, KERN_ERR, LOG_TEMP,
				"0347 Adapter is very hot, please take "
				"corrective action. temperature : %d Celsius\n",
				(uint32_t) icmd->ulpContext);
		} else {
			temp_event_data.event_code = LPFC_NORMAL_TEMP;
			lpfc_printf_log(phba, KERN_ERR, LOG_TEMP,
				"0340 Adapter temperature is OK now. "
				"temperature : %d Celsius\n",
				(uint32_t) icmd->ulpContext);
		}

		
		shost = lpfc_shost_from_vport(phba->pport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
			sizeof(temp_event_data), (char *) &temp_event_data,
			LPFC_NL_VENDOR_ID);
		break;
	case ASYNC_STATUS_CN:
		lpfc_sli_abts_err_handler(phba, iocbq);
		break;
	default:
		iocb_w = (uint32_t *) icmd;
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0346 Ring %d handler: unexpected ASYNC_STATUS"
			" evt_code 0x%x\n"
			"W0  0x%08x W1  0x%08x W2  0x%08x W3  0x%08x\n"
			"W4  0x%08x W5  0x%08x W6  0x%08x W7  0x%08x\n"
			"W8  0x%08x W9  0x%08x W10 0x%08x W11 0x%08x\n"
			"W12 0x%08x W13 0x%08x W14 0x%08x W15 0x%08x\n",
			pring->ringno, icmd->un.asyncstat.evt_code,
			iocb_w[0], iocb_w[1], iocb_w[2], iocb_w[3],
			iocb_w[4], iocb_w[5], iocb_w[6], iocb_w[7],
			iocb_w[8], iocb_w[9], iocb_w[10], iocb_w[11],
			iocb_w[12], iocb_w[13], iocb_w[14], iocb_w[15]);

		break;
	}
}


int
lpfc_sli_setup(struct lpfc_hba *phba)
{
	int i, totiocbsize = 0;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;

	psli->num_rings = MAX_CONFIGURED_RINGS;
	psli->sli_flag = 0;
	psli->fcp_ring = LPFC_FCP_RING;
	psli->next_ring = LPFC_FCP_NEXT_RING;
	psli->extra_ring = LPFC_EXTRA_RING;

	psli->iocbq_lookup = NULL;
	psli->iocbq_lookup_len = 0;
	psli->last_iotag = 0;

	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		switch (i) {
		case LPFC_FCP_RING:	
			
			pring->numCiocb = SLI2_IOCB_CMD_R0_ENTRIES;
			pring->numRiocb = SLI2_IOCB_RSP_R0_ENTRIES;
			pring->numCiocb += SLI2_IOCB_CMD_R1XTRA_ENTRIES;
			pring->numRiocb += SLI2_IOCB_RSP_R1XTRA_ENTRIES;
			pring->numCiocb += SLI2_IOCB_CMD_R3XTRA_ENTRIES;
			pring->numRiocb += SLI2_IOCB_RSP_R3XTRA_ENTRIES;
			pring->sizeCiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_CMD_SIZE :
							SLI2_IOCB_CMD_SIZE;
			pring->sizeRiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_RSP_SIZE :
							SLI2_IOCB_RSP_SIZE;
			pring->iotag_ctr = 0;
			pring->iotag_max =
			    (phba->cfg_hba_queue_depth * 2);
			pring->fast_iotag = pring->iotag_max;
			pring->num_mask = 0;
			break;
		case LPFC_EXTRA_RING:	
			
			pring->numCiocb = SLI2_IOCB_CMD_R1_ENTRIES;
			pring->numRiocb = SLI2_IOCB_RSP_R1_ENTRIES;
			pring->sizeCiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_CMD_SIZE :
							SLI2_IOCB_CMD_SIZE;
			pring->sizeRiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_RSP_SIZE :
							SLI2_IOCB_RSP_SIZE;
			pring->iotag_max = phba->cfg_hba_queue_depth;
			pring->num_mask = 0;
			break;
		case LPFC_ELS_RING:	
			
			pring->numCiocb = SLI2_IOCB_CMD_R2_ENTRIES;
			pring->numRiocb = SLI2_IOCB_RSP_R2_ENTRIES;
			pring->sizeCiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_CMD_SIZE :
							SLI2_IOCB_CMD_SIZE;
			pring->sizeRiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_RSP_SIZE :
							SLI2_IOCB_RSP_SIZE;
			pring->fast_iotag = 0;
			pring->iotag_ctr = 0;
			pring->iotag_max = 4096;
			pring->lpfc_sli_rcv_async_status =
				lpfc_sli_async_event_handler;
			pring->num_mask = LPFC_MAX_RING_MASK;
			pring->prt[0].profile = 0;	
			pring->prt[0].rctl = FC_RCTL_ELS_REQ;
			pring->prt[0].type = FC_TYPE_ELS;
			pring->prt[0].lpfc_sli_rcv_unsol_event =
			    lpfc_els_unsol_event;
			pring->prt[1].profile = 0;	
			pring->prt[1].rctl = FC_RCTL_ELS_REP;
			pring->prt[1].type = FC_TYPE_ELS;
			pring->prt[1].lpfc_sli_rcv_unsol_event =
			    lpfc_els_unsol_event;
			pring->prt[2].profile = 0;	
			
			pring->prt[2].rctl = FC_RCTL_DD_UNSOL_CTL;
			
			pring->prt[2].type = FC_TYPE_CT;
			pring->prt[2].lpfc_sli_rcv_unsol_event =
			    lpfc_ct_unsol_event;
			pring->prt[3].profile = 0;	
			
			pring->prt[3].rctl = FC_RCTL_DD_SOL_CTL;
			
			pring->prt[3].type = FC_TYPE_CT;
			pring->prt[3].lpfc_sli_rcv_unsol_event =
			    lpfc_ct_unsol_event;
			
			pring->prt[4].profile = 0;	
			pring->prt[4].rctl = FC_RCTL_BA_ABTS;
			pring->prt[4].type = FC_TYPE_BLS;
			pring->prt[4].lpfc_sli_rcv_unsol_event =
			    lpfc_sli4_ct_abort_unsol_event;
			break;
		}
		totiocbsize += (pring->numCiocb * pring->sizeCiocb) +
				(pring->numRiocb * pring->sizeRiocb);
	}
	if (totiocbsize > MAX_SLIM_IOCB_SIZE) {
		
		printk(KERN_ERR "%d:0462 Too many cmd / rsp ring entries in "
		       "SLI2 SLIM Data: x%x x%lx\n",
		       phba->brd_no, totiocbsize,
		       (unsigned long) MAX_SLIM_IOCB_SIZE);
	}
	if (phba->cfg_multi_ring_support == 2)
		lpfc_extra_ring_setup(phba);

	return 0;
}

int
lpfc_sli_queue_setup(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;
	int i;

	psli = &phba->sli;
	spin_lock_irq(&phba->hbalock);
	INIT_LIST_HEAD(&psli->mboxq);
	INIT_LIST_HEAD(&psli->mboxq_cmpl);
	
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		pring->ringno = i;
		pring->next_cmdidx  = 0;
		pring->local_getidx = 0;
		pring->cmdidx = 0;
		INIT_LIST_HEAD(&pring->txq);
		INIT_LIST_HEAD(&pring->txcmplq);
		INIT_LIST_HEAD(&pring->iocb_continueq);
		INIT_LIST_HEAD(&pring->iocb_continue_saveq);
		INIT_LIST_HEAD(&pring->postbufq);
	}
	spin_unlock_irq(&phba->hbalock);
	return 1;
}

static void
lpfc_sli_mbox_sys_flush(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	unsigned long iflag;

	
	spin_lock_irqsave(&phba->hbalock, iflag);
	
	list_splice_init(&phba->sli.mboxq, &completions);
	
	if (psli->mbox_active) {
		list_add_tail(&psli->mbox_active->list, &completions);
		psli->mbox_active = NULL;
		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	}
	
	list_splice_init(&phba->sli.mboxq_cmpl, &completions);
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	
	while (!list_empty(&completions)) {
		list_remove_head(&completions, pmb, LPFC_MBOXQ_t, list);
		pmb->u.mb.mbxStatus = MBX_NOT_FINISHED;
		if (pmb->mbox_cmpl)
			pmb->mbox_cmpl(phba, pmb);
	}
}

int
lpfc_sli_host_down(struct lpfc_vport *vport)
{
	LIST_HEAD(completions);
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *iocb, *next_iocb;
	int i;
	unsigned long flags = 0;
	uint16_t prev_pring_flag;

	lpfc_cleanup_discovery_resources(vport);

	spin_lock_irqsave(&phba->hbalock, flags);
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		prev_pring_flag = pring->flag;
		
		if (pring->ringno == LPFC_ELS_RING) {
			pring->flag |= LPFC_DEFERRED_RING_EVENT;
			
			set_bit(LPFC_DATA_READY, &phba->data_flags);
		}
		list_for_each_entry_safe(iocb, next_iocb, &pring->txq, list) {
			if (iocb->vport != vport)
				continue;
			list_move_tail(&iocb->list, &completions);
			pring->txq_cnt--;
		}

		
		list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq,
									list) {
			if (iocb->vport != vport)
				continue;
			lpfc_sli_issue_abort_iotag(phba, pring, iocb);
		}

		pring->flag = prev_pring_flag;
	}

	spin_unlock_irqrestore(&phba->hbalock, flags);

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);
	return 1;
}

int
lpfc_sli_hba_down(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	struct lpfc_dmabuf *buf_ptr;
	unsigned long flags = 0;
	int i;

	
	lpfc_sli_mbox_sys_shutdown(phba);

	lpfc_hba_down_prep(phba);

	lpfc_fabric_abort_hba(phba);

	spin_lock_irqsave(&phba->hbalock, flags);
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		
		if (pring->ringno == LPFC_ELS_RING) {
			pring->flag |= LPFC_DEFERRED_RING_EVENT;
			
			set_bit(LPFC_DATA_READY, &phba->data_flags);
		}

		list_splice_init(&pring->txq, &completions);
		pring->txq_cnt = 0;

	}
	spin_unlock_irqrestore(&phba->hbalock, flags);

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);

	spin_lock_irqsave(&phba->hbalock, flags);
	list_splice_init(&phba->elsbuf, &completions);
	phba->elsbuf_cnt = 0;
	phba->elsbuf_prev_cnt = 0;
	spin_unlock_irqrestore(&phba->hbalock, flags);

	while (!list_empty(&completions)) {
		list_remove_head(&completions, buf_ptr,
			struct lpfc_dmabuf, list);
		lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
		kfree(buf_ptr);
	}

	
	del_timer_sync(&psli->mbox_tmo);

	spin_lock_irqsave(&phba->pport->work_port_lock, flags);
	phba->pport->work_port_events &= ~WORKER_MBOX_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, flags);

	return 1;
}

void
lpfc_sli_pcimem_bcopy(void *srcp, void *destp, uint32_t cnt)
{
	uint32_t *src = srcp;
	uint32_t *dest = destp;
	uint32_t ldata;
	int i;

	for (i = 0; i < (int)cnt; i += sizeof (uint32_t)) {
		ldata = *src;
		ldata = le32_to_cpu(ldata);
		*dest = ldata;
		src++;
		dest++;
	}
}


void
lpfc_sli_bemem_bcopy(void *srcp, void *destp, uint32_t cnt)
{
	uint32_t *src = srcp;
	uint32_t *dest = destp;
	uint32_t ldata;
	int i;

	for (i = 0; i < (int)cnt; i += sizeof(uint32_t)) {
		ldata = *src;
		ldata = be32_to_cpu(ldata);
		*dest = ldata;
		src++;
		dest++;
	}
}

int
lpfc_sli_ringpostbuf_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 struct lpfc_dmabuf *mp)
{
	spin_lock_irq(&phba->hbalock);
	list_add_tail(&mp->list, &pring->postbufq);
	pring->postbufq_cnt++;
	spin_unlock_irq(&phba->hbalock);
	return 0;
}

uint32_t
lpfc_sli_get_buffer_tag(struct lpfc_hba *phba)
{
	spin_lock_irq(&phba->hbalock);
	phba->buffer_tag_count++;
	phba->buffer_tag_count |= QUE_BUFTAG_BIT;
	spin_unlock_irq(&phba->hbalock);
	return phba->buffer_tag_count;
}

struct lpfc_dmabuf *
lpfc_sli_ring_taggedbuf_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			uint32_t tag)
{
	struct lpfc_dmabuf *mp, *next_mp;
	struct list_head *slp = &pring->postbufq;

	
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mp, next_mp, &pring->postbufq, list) {
		if (mp->buffer_tag == tag) {
			list_del_init(&mp->list);
			pring->postbufq_cnt--;
			spin_unlock_irq(&phba->hbalock);
			return mp;
		}
	}

	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0402 Cannot find virtual addr for buffer tag on "
			"ring %d Data x%lx x%p x%p x%x\n",
			pring->ringno, (unsigned long) tag,
			slp->next, slp->prev, pring->postbufq_cnt);

	return NULL;
}

struct lpfc_dmabuf *
lpfc_sli_ringpostbuf_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 dma_addr_t phys)
{
	struct lpfc_dmabuf *mp, *next_mp;
	struct list_head *slp = &pring->postbufq;

	
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mp, next_mp, &pring->postbufq, list) {
		if (mp->phys == phys) {
			list_del_init(&mp->list);
			pring->postbufq_cnt--;
			spin_unlock_irq(&phba->hbalock);
			return mp;
		}
	}

	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0410 Cannot find virtual addr for mapped buf on "
			"ring %d Data x%llx x%p x%p x%x\n",
			pring->ringno, (unsigned long long)phys,
			slp->next, slp->prev, pring->postbufq_cnt);
	return NULL;
}

static void
lpfc_sli_abort_els_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	IOCB_t *irsp = &rspiocb->iocb;
	uint16_t abort_iotag, abort_context;
	struct lpfc_iocbq *abort_iocb = NULL;

	if (irsp->ulpStatus) {

		abort_context = cmdiocb->iocb.un.acxri.abortContextTag;
		abort_iotag = cmdiocb->iocb.un.acxri.abortIoTag;

		spin_lock_irq(&phba->hbalock);
		if (phba->sli_rev < LPFC_SLI_REV4) {
			if (abort_iotag != 0 &&
				abort_iotag <= phba->sli.last_iotag)
				abort_iocb =
					phba->sli.iocbq_lookup[abort_iotag];
		} else
			abort_iocb = phba->sli.iocbq_lookup[abort_context];

		lpfc_printf_log(phba, KERN_WARNING, LOG_ELS | LOG_SLI,
				"0327 Cannot abort els iocb %p "
				"with tag %x context %x, abort status %x, "
				"abort code %x\n",
				abort_iocb, abort_iotag, abort_context,
				irsp->ulpStatus, irsp->un.ulpWord[4]);

		spin_unlock_irq(&phba->hbalock);
	}
	lpfc_sli_release_iocbq(phba, cmdiocb);
	return;
}

static void
lpfc_ignore_els_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		     struct lpfc_iocbq *rspiocb)
{
	IOCB_t *irsp = &rspiocb->iocb;

	
	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"0139 Ignoring ELS cmd tag x%x completion Data: "
			"x%x x%x x%x\n",
			irsp->ulpIoTag, irsp->ulpStatus,
			irsp->un.ulpWord[4], irsp->ulpTimeout);
	if (cmdiocb->iocb.ulpCommand == CMD_GEN_REQUEST64_CR)
		lpfc_ct_free_iocb(phba, cmdiocb);
	else
		lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

static int
lpfc_sli_abort_iotag_issue(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			   struct lpfc_iocbq *cmdiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct lpfc_iocbq *abtsiocbp;
	IOCB_t *icmd = NULL;
	IOCB_t *iabt = NULL;
	int retval;

	icmd = &cmdiocb->iocb;
	if (icmd->ulpCommand == CMD_ABORT_XRI_CN ||
	    icmd->ulpCommand == CMD_CLOSE_XRI_CN ||
	    (cmdiocb->iocb_flag & LPFC_DRIVER_ABORTED) != 0)
		return 0;

	
	abtsiocbp = __lpfc_sli_get_iocbq(phba);
	if (abtsiocbp == NULL)
		return 0;

	cmdiocb->iocb_flag |= LPFC_DRIVER_ABORTED;

	iabt = &abtsiocbp->iocb;
	iabt->un.acxri.abortType = ABORT_TYPE_ABTS;
	iabt->un.acxri.abortContextTag = icmd->ulpContext;
	if (phba->sli_rev == LPFC_SLI_REV4) {
		iabt->un.acxri.abortIoTag = cmdiocb->sli4_xritag;
		iabt->un.acxri.abortContextTag = cmdiocb->iotag;
	}
	else
		iabt->un.acxri.abortIoTag = icmd->ulpIoTag;
	iabt->ulpLe = 1;
	iabt->ulpClass = icmd->ulpClass;

	
	abtsiocbp->fcp_wqidx = cmdiocb->fcp_wqidx;
	if (cmdiocb->iocb_flag & LPFC_IO_FCP)
		abtsiocbp->iocb_flag |= LPFC_USE_FCPWQIDX;

	if (phba->link_state >= LPFC_LINK_UP)
		iabt->ulpCommand = CMD_ABORT_XRI_CN;
	else
		iabt->ulpCommand = CMD_CLOSE_XRI_CN;

	abtsiocbp->iocb_cmpl = lpfc_sli_abort_els_cmpl;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_SLI,
			 "0339 Abort xri x%x, original iotag x%x, "
			 "abort cmd iotag x%x\n",
			 iabt->un.acxri.abortIoTag,
			 iabt->un.acxri.abortContextTag,
			 abtsiocbp->iotag);
	retval = __lpfc_sli_issue_iocb(phba, pring->ringno, abtsiocbp, 0);

	if (retval)
		__lpfc_sli_release_iocbq(phba, abtsiocbp);

	return retval;
}

int
lpfc_sli_issue_abort_iotag(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			   struct lpfc_iocbq *cmdiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	int retval = IOCB_ERROR;
	IOCB_t *icmd = NULL;

	icmd = &cmdiocb->iocb;
	if (icmd->ulpCommand == CMD_ABORT_XRI_CN ||
	    icmd->ulpCommand == CMD_CLOSE_XRI_CN ||
	    (cmdiocb->iocb_flag & LPFC_DRIVER_ABORTED) != 0)
		return 0;

	if ((vport->load_flag & FC_UNLOADING) &&
	    (pring->ringno == LPFC_ELS_RING)) {
		if (cmdiocb->iocb_flag & LPFC_IO_FABRIC)
			cmdiocb->fabric_iocb_cmpl = lpfc_ignore_els_cmpl;
		else
			cmdiocb->iocb_cmpl = lpfc_ignore_els_cmpl;
		goto abort_iotag_exit;
	}

	
	retval = lpfc_sli_abort_iotag_issue(phba, pring, cmdiocb);

abort_iotag_exit:
	return retval;
}

static void
lpfc_sli_iocb_ring_abort(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	LIST_HEAD(completions);
	struct lpfc_iocbq *iocb, *next_iocb;

	if (pring->ringno == LPFC_ELS_RING)
		lpfc_fabric_abort_hba(phba);

	spin_lock_irq(&phba->hbalock);

	
	list_splice_init(&pring->txq, &completions);
	pring->txq_cnt = 0;

	
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list)
		lpfc_sli_abort_iotag_issue(phba, pring, iocb);

	spin_unlock_irq(&phba->hbalock);

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

void
lpfc_sli_hba_iocb_abort(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	int i;

	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		lpfc_sli_iocb_ring_abort(phba, pring);
	}
}

static int
lpfc_sli_validate_fcp_iocb(struct lpfc_iocbq *iocbq, struct lpfc_vport *vport,
			   uint16_t tgt_id, uint64_t lun_id,
			   lpfc_ctx_cmd ctx_cmd)
{
	struct lpfc_scsi_buf *lpfc_cmd;
	int rc = 1;

	if (!(iocbq->iocb_flag &  LPFC_IO_FCP))
		return rc;

	if (iocbq->vport != vport)
		return rc;

	lpfc_cmd = container_of(iocbq, struct lpfc_scsi_buf, cur_iocbq);

	if (lpfc_cmd->pCmd == NULL)
		return rc;

	switch (ctx_cmd) {
	case LPFC_CTX_LUN:
		if ((lpfc_cmd->rdata->pnode) &&
		    (lpfc_cmd->rdata->pnode->nlp_sid == tgt_id) &&
		    (scsilun_to_int(&lpfc_cmd->fcp_cmnd->fcp_lun) == lun_id))
			rc = 0;
		break;
	case LPFC_CTX_TGT:
		if ((lpfc_cmd->rdata->pnode) &&
		    (lpfc_cmd->rdata->pnode->nlp_sid == tgt_id))
			rc = 0;
		break;
	case LPFC_CTX_HOST:
		rc = 0;
		break;
	default:
		printk(KERN_ERR "%s: Unknown context cmd type, value %d\n",
			__func__, ctx_cmd);
		break;
	}

	return rc;
}

int
lpfc_sli_sum_iocb(struct lpfc_vport *vport, uint16_t tgt_id, uint64_t lun_id,
		  lpfc_ctx_cmd ctx_cmd)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *iocbq;
	int sum, i;

	for (i = 1, sum = 0; i <= phba->sli.last_iotag; i++) {
		iocbq = phba->sli.iocbq_lookup[i];

		if (lpfc_sli_validate_fcp_iocb (iocbq, vport, tgt_id, lun_id,
						ctx_cmd) == 0)
			sum++;
	}

	return sum;
}

void
lpfc_sli_abort_fcp_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3096 ABORT_XRI_CN completing on xri x%x "
			"original iotag x%x, abort cmd iotag x%x "
			"status 0x%x, reason 0x%x\n",
			cmdiocb->iocb.un.acxri.abortContextTag,
			cmdiocb->iocb.un.acxri.abortIoTag,
			cmdiocb->iotag, rspiocb->iocb.ulpStatus,
			rspiocb->iocb.un.ulpWord[4]);
	lpfc_sli_release_iocbq(phba, cmdiocb);
	return;
}

int
lpfc_sli_abort_iocb(struct lpfc_vport *vport, struct lpfc_sli_ring *pring,
		    uint16_t tgt_id, uint64_t lun_id, lpfc_ctx_cmd abort_cmd)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *iocbq;
	struct lpfc_iocbq *abtsiocb;
	IOCB_t *cmd = NULL;
	int errcnt = 0, ret_val = 0;
	int i;

	for (i = 1; i <= phba->sli.last_iotag; i++) {
		iocbq = phba->sli.iocbq_lookup[i];

		if (lpfc_sli_validate_fcp_iocb(iocbq, vport, tgt_id, lun_id,
					       abort_cmd) != 0)
			continue;

		
		abtsiocb = lpfc_sli_get_iocbq(phba);
		if (abtsiocb == NULL) {
			errcnt++;
			continue;
		}

		cmd = &iocbq->iocb;
		abtsiocb->iocb.un.acxri.abortType = ABORT_TYPE_ABTS;
		abtsiocb->iocb.un.acxri.abortContextTag = cmd->ulpContext;
		if (phba->sli_rev == LPFC_SLI_REV4)
			abtsiocb->iocb.un.acxri.abortIoTag = iocbq->sli4_xritag;
		else
			abtsiocb->iocb.un.acxri.abortIoTag = cmd->ulpIoTag;
		abtsiocb->iocb.ulpLe = 1;
		abtsiocb->iocb.ulpClass = cmd->ulpClass;
		abtsiocb->vport = phba->pport;

		
		abtsiocb->fcp_wqidx = iocbq->fcp_wqidx;
		if (iocbq->iocb_flag & LPFC_IO_FCP)
			abtsiocb->iocb_flag |= LPFC_USE_FCPWQIDX;

		if (lpfc_is_link_up(phba))
			abtsiocb->iocb.ulpCommand = CMD_ABORT_XRI_CN;
		else
			abtsiocb->iocb.ulpCommand = CMD_CLOSE_XRI_CN;

		
		abtsiocb->iocb_cmpl = lpfc_sli_abort_fcp_cmpl;
		ret_val = lpfc_sli_issue_iocb(phba, pring->ringno,
					      abtsiocb, 0);
		if (ret_val == IOCB_ERROR) {
			lpfc_sli_release_iocbq(phba, abtsiocb);
			errcnt++;
			continue;
		}
	}

	return errcnt;
}

static void
lpfc_sli_wake_iocb_wait(struct lpfc_hba *phba,
			struct lpfc_iocbq *cmdiocbq,
			struct lpfc_iocbq *rspiocbq)
{
	wait_queue_head_t *pdone_q;
	unsigned long iflags;
	struct lpfc_scsi_buf *lpfc_cmd;

	spin_lock_irqsave(&phba->hbalock, iflags);
	cmdiocbq->iocb_flag |= LPFC_IO_WAKE;
	if (cmdiocbq->context2 && rspiocbq)
		memcpy(&((struct lpfc_iocbq *)cmdiocbq->context2)->iocb,
		       &rspiocbq->iocb, sizeof(IOCB_t));

	
	if ((cmdiocbq->iocb_flag & LPFC_IO_FCP) &&
		!(cmdiocbq->iocb_flag & LPFC_IO_LIBDFC)) {
		lpfc_cmd = container_of(cmdiocbq, struct lpfc_scsi_buf,
			cur_iocbq);
		lpfc_cmd->exch_busy = rspiocbq->iocb_flag & LPFC_EXCHANGE_BUSY;
	}

	pdone_q = cmdiocbq->context_un.wait_queue;
	if (pdone_q)
		wake_up(pdone_q);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return;
}

static int
lpfc_chk_iocb_flg(struct lpfc_hba *phba,
		 struct lpfc_iocbq *piocbq, uint32_t flag)
{
	unsigned long iflags;
	int ret;

	spin_lock_irqsave(&phba->hbalock, iflags);
	ret = piocbq->iocb_flag & flag;
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return ret;

}

int
lpfc_sli_issue_iocb_wait(struct lpfc_hba *phba,
			 uint32_t ring_number,
			 struct lpfc_iocbq *piocb,
			 struct lpfc_iocbq *prspiocbq,
			 uint32_t timeout)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(done_q);
	long timeleft, timeout_req = 0;
	int retval = IOCB_SUCCESS;
	uint32_t creg_val;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	if (prspiocbq) {
		if (piocb->context2)
			return IOCB_ERROR;
		piocb->context2 = prspiocbq;
	}

	piocb->iocb_cmpl = lpfc_sli_wake_iocb_wait;
	piocb->context_un.wait_queue = &done_q;
	piocb->iocb_flag &= ~LPFC_IO_WAKE;

	if (phba->cfg_poll & DISABLE_FCP_RING_INT) {
		if (lpfc_readl(phba->HCregaddr, &creg_val))
			return IOCB_ERROR;
		creg_val |= (HC_R0INT_ENA << LPFC_FCP_RING);
		writel(creg_val, phba->HCregaddr);
		readl(phba->HCregaddr); 
	}

	retval = lpfc_sli_issue_iocb(phba, ring_number, piocb,
				     SLI_IOCB_RET_IOCB);
	if (retval == IOCB_SUCCESS) {
		timeout_req = timeout * HZ;
		timeleft = wait_event_timeout(done_q,
				lpfc_chk_iocb_flg(phba, piocb, LPFC_IO_WAKE),
				timeout_req);

		if (piocb->iocb_flag & LPFC_IO_WAKE) {
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
					"0331 IOCB wake signaled\n");
		} else if (timeleft == 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0338 IOCB wait timeout error - no "
					"wake response Data x%x\n", timeout);
			retval = IOCB_TIMEDOUT;
		} else {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0330 IOCB wake NOT set, "
					"Data x%x x%lx\n",
					timeout, (timeleft / jiffies));
			retval = IOCB_TIMEDOUT;
		}
	} else if (retval == IOCB_BUSY) {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2818 Max IOCBs %d txq cnt %d txcmplq cnt %d\n",
			phba->iocb_cnt, pring->txq_cnt, pring->txcmplq_cnt);
		return retval;
	} else {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"0332 IOCB wait issue failed, Data x%x\n",
				retval);
		retval = IOCB_ERROR;
	}

	if (phba->cfg_poll & DISABLE_FCP_RING_INT) {
		if (lpfc_readl(phba->HCregaddr, &creg_val))
			return IOCB_ERROR;
		creg_val &= ~(HC_R0INT_ENA << LPFC_FCP_RING);
		writel(creg_val, phba->HCregaddr);
		readl(phba->HCregaddr); 
	}

	if (prspiocbq)
		piocb->context2 = NULL;

	piocb->context_un.wait_queue = NULL;
	piocb->iocb_cmpl = NULL;
	return retval;
}

int
lpfc_sli_issue_mbox_wait(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq,
			 uint32_t timeout)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(done_q);
	int retval;
	unsigned long flag;

	
	if (pmboxq->context1)
		return MBX_NOT_FINISHED;

	pmboxq->mbox_flag &= ~LPFC_MBX_WAKE;
	
	pmboxq->mbox_cmpl = lpfc_sli_wake_mbox_wait;
	
	pmboxq->context1 = &done_q;

	
	retval = lpfc_sli_issue_mbox(phba, pmboxq, MBX_NOWAIT);
	if (retval == MBX_BUSY || retval == MBX_SUCCESS) {
		wait_event_interruptible_timeout(done_q,
				pmboxq->mbox_flag & LPFC_MBX_WAKE,
				timeout * HZ);

		spin_lock_irqsave(&phba->hbalock, flag);
		pmboxq->context1 = NULL;
		if (pmboxq->mbox_flag & LPFC_MBX_WAKE) {
			retval = MBX_SUCCESS;
			lpfc_sli4_swap_str(phba, pmboxq);
		} else {
			retval = MBX_TIMEOUT;
			pmboxq->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		}
		spin_unlock_irqrestore(&phba->hbalock, flag);
	}

	return retval;
}

void
lpfc_sli_mbox_sys_shutdown(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	unsigned long timeout;

	timeout = msecs_to_jiffies(LPFC_MBOX_TMO * 1000) + jiffies;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_SLI_ASYNC_MBX_BLK;

	if (psli->sli_flag & LPFC_SLI_ACTIVE) {
		if (phba->sli.mbox_active)
			timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba,
						phba->sli.mbox_active) *
						1000) + jiffies;
		spin_unlock_irq(&phba->hbalock);

		while (phba->sli.mbox_active) {
			
			msleep(2);
			if (time_after(jiffies, timeout))
				break;
		}
	} else
		spin_unlock_irq(&phba->hbalock);

	lpfc_sli_mbox_sys_flush(phba);
}

static int
lpfc_sli_eratt_read(struct lpfc_hba *phba)
{
	uint32_t ha_copy;

	
	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		goto unplug_err;

	if (ha_copy & HA_ERATT) {
		
		if (lpfc_sli_read_hs(phba))
			goto unplug_err;

		
		if ((HS_FFER1 & phba->work_hs) &&
		    ((HS_FFER2 | HS_FFER3 | HS_FFER4 | HS_FFER5 |
		      HS_FFER6 | HS_FFER7 | HS_FFER8) & phba->work_hs)) {
			phba->hba_flag |= DEFER_ERATT;
			
			writel(0, phba->HCregaddr);
			readl(phba->HCregaddr);
		}

		
		phba->work_ha |= HA_ERATT;
		
		phba->hba_flag |= HBA_ERATT_HANDLED;
		return 1;
	}
	return 0;

unplug_err:
	
	phba->work_hs |= UNPLUG_ERR;
	
	phba->work_ha |= HA_ERATT;
	
	phba->hba_flag |= HBA_ERATT_HANDLED;
	return 1;
}

static int
lpfc_sli4_eratt_read(struct lpfc_hba *phba)
{
	uint32_t uerr_sta_hi, uerr_sta_lo;
	uint32_t if_type, portsmphr;
	struct lpfc_register portstat_reg;

	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		if (lpfc_readl(phba->sli4_hba.u.if_type0.UERRLOregaddr,
			&uerr_sta_lo) ||
			lpfc_readl(phba->sli4_hba.u.if_type0.UERRHIregaddr,
			&uerr_sta_hi)) {
			phba->work_hs |= UNPLUG_ERR;
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		if ((~phba->sli4_hba.ue_mask_lo & uerr_sta_lo) ||
		    (~phba->sli4_hba.ue_mask_hi & uerr_sta_hi)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"1423 HBA Unrecoverable error: "
					"uerr_lo_reg=0x%x, uerr_hi_reg=0x%x, "
					"ue_mask_lo_reg=0x%x, "
					"ue_mask_hi_reg=0x%x\n",
					uerr_sta_lo, uerr_sta_hi,
					phba->sli4_hba.ue_mask_lo,
					phba->sli4_hba.ue_mask_hi);
			phba->work_status[0] = uerr_sta_lo;
			phba->work_status[1] = uerr_sta_hi;
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
		if (lpfc_readl(phba->sli4_hba.u.if_type2.STATUSregaddr,
			&portstat_reg.word0) ||
			lpfc_readl(phba->sli4_hba.PSMPHRregaddr,
			&portsmphr)){
			phba->work_hs |= UNPLUG_ERR;
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		if (bf_get(lpfc_sliport_status_err, &portstat_reg)) {
			phba->work_status[0] =
				readl(phba->sli4_hba.u.if_type2.ERR1regaddr);
			phba->work_status[1] =
				readl(phba->sli4_hba.u.if_type2.ERR2regaddr);
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2885 Port Status Event: "
					"port status reg 0x%x, "
					"port smphr reg 0x%x, "
					"error 1=0x%x, error 2=0x%x\n",
					portstat_reg.word0,
					portsmphr,
					phba->work_status[0],
					phba->work_status[1]);
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2886 HBA Error Attention on unsupported "
				"if type %d.", if_type);
		return 1;
	}

	return 0;
}

int
lpfc_sli_check_eratt(struct lpfc_hba *phba)
{
	uint32_t ha_copy;

	if (phba->link_flag & LS_IGNORE_ERATT)
		return 0;

	
	spin_lock_irq(&phba->hbalock);
	if (phba->hba_flag & HBA_ERATT_HANDLED) {
		
		spin_unlock_irq(&phba->hbalock);
		return 0;
	}

	if (unlikely(phba->hba_flag & DEFER_ERATT)) {
		spin_unlock_irq(&phba->hbalock);
		return 0;
	}

	
	if (unlikely(pci_channel_offline(phba->pcidev))) {
		spin_unlock_irq(&phba->hbalock);
		return 0;
	}

	switch (phba->sli_rev) {
	case LPFC_SLI_REV2:
	case LPFC_SLI_REV3:
		
		ha_copy = lpfc_sli_eratt_read(phba);
		break;
	case LPFC_SLI_REV4:
		
		ha_copy = lpfc_sli4_eratt_read(phba);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0299 Invalid SLI revision (%d)\n",
				phba->sli_rev);
		ha_copy = 0;
		break;
	}
	spin_unlock_irq(&phba->hbalock);

	return ha_copy;
}

static inline int
lpfc_intr_state_check(struct lpfc_hba *phba)
{
	
	if (unlikely(pci_channel_offline(phba->pcidev)))
		return -EIO;

	
	phba->sli.slistat.sli_intr++;

	
	if (unlikely(phba->link_state < LPFC_LINK_DOWN))
		return -EIO;

	return 0;
}

irqreturn_t
lpfc_sli_sp_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	uint32_t ha_copy, hc_copy;
	uint32_t work_ha_copy;
	unsigned long status;
	unsigned long iflag;
	uint32_t control;

	MAILBOX_t *mbox, *pmbox;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *mp;
	LPFC_MBOXQ_t *pmb;
	int rc;

	phba = (struct lpfc_hba *)dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	if (phba->intr_type == MSIX) {
		
		if (lpfc_intr_state_check(phba))
			return IRQ_NONE;
		
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			goto unplug_error;
		if (phba->link_flag & LS_IGNORE_ERATT)
			ha_copy &= ~HA_ERATT;
		
		if (ha_copy & HA_ERATT) {
			if (phba->hba_flag & HBA_ERATT_HANDLED)
				
				ha_copy &= ~HA_ERATT;
			else
				
				phba->hba_flag |= HBA_ERATT_HANDLED;
		}

		if (unlikely(phba->hba_flag & DEFER_ERATT)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			return IRQ_NONE;
		}

		
		if (lpfc_readl(phba->HCregaddr, &hc_copy))
			goto unplug_error;

		writel(hc_copy & ~(HC_MBINT_ENA | HC_R2INT_ENA |
			HC_LAINT_ENA | HC_ERINT_ENA),
			phba->HCregaddr);
		writel((ha_copy & (HA_MBATT | HA_R2_CLR_MSK)),
			phba->HAregaddr);
		writel(hc_copy, phba->HCregaddr);
		readl(phba->HAregaddr); 
		spin_unlock_irqrestore(&phba->hbalock, iflag);
	} else
		ha_copy = phba->ha_copy;

	work_ha_copy = ha_copy & phba->work_ha_mask;

	if (work_ha_copy) {
		if (work_ha_copy & HA_LATT) {
			if (phba->sli.sli_flag & LPFC_PROCESS_LA) {
				spin_lock_irqsave(&phba->hbalock, iflag);
				phba->sli.sli_flag &= ~LPFC_PROCESS_LA;
				if (lpfc_readl(phba->HCregaddr, &control))
					goto unplug_error;
				control &= ~HC_LAINT_ENA;
				writel(control, phba->HCregaddr);
				readl(phba->HCregaddr); 
				spin_unlock_irqrestore(&phba->hbalock, iflag);
			}
			else
				work_ha_copy &= ~HA_LATT;
		}

		if (work_ha_copy & ~(HA_ERATT | HA_MBATT | HA_LATT)) {
			status = (work_ha_copy &
				(HA_RXMASK  << (4*LPFC_ELS_RING)));
			status >>= (4*LPFC_ELS_RING);
			if (status & HA_RXMASK) {
				spin_lock_irqsave(&phba->hbalock, iflag);
				if (lpfc_readl(phba->HCregaddr, &control))
					goto unplug_error;

				lpfc_debugfs_slow_ring_trc(phba,
				"ISR slow ring:   ctl:x%x stat:x%x isrcnt:x%x",
				control, status,
				(uint32_t)phba->sli.slistat.sli_intr);

				if (control & (HC_R0INT_ENA << LPFC_ELS_RING)) {
					lpfc_debugfs_slow_ring_trc(phba,
						"ISR Disable ring:"
						"pwork:x%x hawork:x%x wait:x%x",
						phba->work_ha, work_ha_copy,
						(uint32_t)((unsigned long)
						&phba->work_waitq));

					control &=
					    ~(HC_R0INT_ENA << LPFC_ELS_RING);
					writel(control, phba->HCregaddr);
					readl(phba->HCregaddr); 
				}
				else {
					lpfc_debugfs_slow_ring_trc(phba,
						"ISR slow ring:   pwork:"
						"x%x hawork:x%x wait:x%x",
						phba->work_ha, work_ha_copy,
						(uint32_t)((unsigned long)
						&phba->work_waitq));
				}
				spin_unlock_irqrestore(&phba->hbalock, iflag);
			}
		}
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (work_ha_copy & HA_ERATT) {
			if (lpfc_sli_read_hs(phba))
				goto unplug_error;
			if ((HS_FFER1 & phba->work_hs) &&
				((HS_FFER2 | HS_FFER3 | HS_FFER4 | HS_FFER5 |
				  HS_FFER6 | HS_FFER7 | HS_FFER8) &
				  phba->work_hs)) {
				phba->hba_flag |= DEFER_ERATT;
				
				writel(0, phba->HCregaddr);
				readl(phba->HCregaddr);
			}
		}

		if ((work_ha_copy & HA_MBATT) && (phba->sli.mbox_active)) {
			pmb = phba->sli.mbox_active;
			pmbox = &pmb->u.mb;
			mbox = phba->mbox;
			vport = pmb->vport;

			
			lpfc_sli_pcimem_bcopy(mbox, pmbox, sizeof(uint32_t));
			if (pmbox->mbxOwner != OWN_HOST) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				lpfc_printf_log(phba, KERN_ERR, LOG_MBOX |
						LOG_SLI,
						"(%d):0304 Stray Mailbox "
						"Interrupt mbxCommand x%x "
						"mbxStatus x%x\n",
						(vport ? vport->vpi : 0),
						pmbox->mbxCommand,
						pmbox->mbxStatus);
				
				work_ha_copy &= ~HA_MBATT;
			} else {
				phba->sli.mbox_active = NULL;
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				phba->last_completion_time = jiffies;
				del_timer(&phba->sli.mbox_tmo);
				if (pmb->mbox_cmpl) {
					lpfc_sli_pcimem_bcopy(mbox, pmbox,
							MAILBOX_CMD_SIZE);
					if (pmb->out_ext_byte_len &&
						pmb->context2)
						lpfc_sli_pcimem_bcopy(
						phba->mbox_ext,
						pmb->context2,
						pmb->out_ext_byte_len);
				}
				if (pmb->mbox_flag & LPFC_MBX_IMED_UNREG) {
					pmb->mbox_flag &= ~LPFC_MBX_IMED_UNREG;

					lpfc_debugfs_disc_trc(vport,
						LPFC_DISC_TRC_MBOX_VPORT,
						"MBOX dflt rpi: : "
						"status:x%x rpi:x%x",
						(uint32_t)pmbox->mbxStatus,
						pmbox->un.varWords[0], 0);

					if (!pmbox->mbxStatus) {
						mp = (struct lpfc_dmabuf *)
							(pmb->context1);
						ndlp = (struct lpfc_nodelist *)
							pmb->context2;

						lpfc_unreg_login(phba,
							vport->vpi,
							pmbox->un.varWords[0],
							pmb);
						pmb->mbox_cmpl =
							lpfc_mbx_cmpl_dflt_rpi;
						pmb->context1 = mp;
						pmb->context2 = ndlp;
						pmb->vport = vport;
						rc = lpfc_sli_issue_mbox(phba,
								pmb,
								MBX_NOWAIT);
						if (rc != MBX_BUSY)
							lpfc_printf_log(phba,
							KERN_ERR,
							LOG_MBOX | LOG_SLI,
							"0350 rc should have"
							"been MBX_BUSY\n");
						if (rc != MBX_NOT_FINISHED)
							goto send_current_mbox;
					}
				}
				spin_lock_irqsave(
						&phba->pport->work_port_lock,
						iflag);
				phba->pport->work_port_events &=
					~WORKER_MBOX_TMO;
				spin_unlock_irqrestore(
						&phba->pport->work_port_lock,
						iflag);
				lpfc_mbox_cmpl_put(phba, pmb);
			}
		} else
			spin_unlock_irqrestore(&phba->hbalock, iflag);

		if ((work_ha_copy & HA_MBATT) &&
		    (phba->sli.mbox_active == NULL)) {
send_current_mbox:
			
			do {
				rc = lpfc_sli_issue_mbox(phba, NULL,
							 MBX_NOWAIT);
			} while (rc == MBX_NOT_FINISHED);
			if (rc != MBX_SUCCESS)
				lpfc_printf_log(phba, KERN_ERR, LOG_MBOX |
						LOG_SLI, "0349 rc should be "
						"MBX_SUCCESS\n");
		}

		spin_lock_irqsave(&phba->hbalock, iflag);
		phba->work_ha |= work_ha_copy;
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		lpfc_worker_wake_up(phba);
	}
	return IRQ_HANDLED;
unplug_error:
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return IRQ_HANDLED;

} 

irqreturn_t
lpfc_sli_fp_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	uint32_t ha_copy;
	unsigned long status;
	unsigned long iflag;

	phba = (struct lpfc_hba *) dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	if (phba->intr_type == MSIX) {
		
		if (lpfc_intr_state_check(phba))
			return IRQ_NONE;
		
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			return IRQ_HANDLED;
		
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (unlikely(phba->hba_flag & DEFER_ERATT)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			return IRQ_NONE;
		}
		writel((ha_copy & (HA_R0_CLR_MSK | HA_R1_CLR_MSK)),
			phba->HAregaddr);
		readl(phba->HAregaddr); 
		spin_unlock_irqrestore(&phba->hbalock, iflag);
	} else
		ha_copy = phba->ha_copy;

	ha_copy &= ~(phba->work_ha_mask);

	status = (ha_copy & (HA_RXMASK << (4*LPFC_FCP_RING)));
	status >>= (4*LPFC_FCP_RING);
	if (status & HA_RXMASK)
		lpfc_sli_handle_fast_ring_event(phba,
						&phba->sli.ring[LPFC_FCP_RING],
						status);

	if (phba->cfg_multi_ring_support == 2) {
		status = (ha_copy & (HA_RXMASK << (4*LPFC_EXTRA_RING)));
		status >>= (4*LPFC_EXTRA_RING);
		if (status & HA_RXMASK) {
			lpfc_sli_handle_fast_ring_event(phba,
					&phba->sli.ring[LPFC_EXTRA_RING],
					status);
		}
	}
	return IRQ_HANDLED;
}  

irqreturn_t
lpfc_sli_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	irqreturn_t sp_irq_rc, fp_irq_rc;
	unsigned long status1, status2;
	uint32_t hc_copy;

	phba = (struct lpfc_hba *) dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	
	if (lpfc_intr_state_check(phba))
		return IRQ_NONE;

	spin_lock(&phba->hbalock);
	if (lpfc_readl(phba->HAregaddr, &phba->ha_copy)) {
		spin_unlock(&phba->hbalock);
		return IRQ_HANDLED;
	}

	if (unlikely(!phba->ha_copy)) {
		spin_unlock(&phba->hbalock);
		return IRQ_NONE;
	} else if (phba->ha_copy & HA_ERATT) {
		if (phba->hba_flag & HBA_ERATT_HANDLED)
			
			phba->ha_copy &= ~HA_ERATT;
		else
			
			phba->hba_flag |= HBA_ERATT_HANDLED;
	}

	if (unlikely(phba->hba_flag & DEFER_ERATT)) {
		spin_unlock(&phba->hbalock);
		return IRQ_NONE;
	}

	
	if (lpfc_readl(phba->HCregaddr, &hc_copy)) {
		spin_unlock(&phba->hbalock);
		return IRQ_HANDLED;
	}
	writel(hc_copy & ~(HC_MBINT_ENA | HC_R0INT_ENA | HC_R1INT_ENA
		| HC_R2INT_ENA | HC_LAINT_ENA | HC_ERINT_ENA),
		phba->HCregaddr);
	writel((phba->ha_copy & ~(HA_LATT | HA_ERATT)), phba->HAregaddr);
	writel(hc_copy, phba->HCregaddr);
	readl(phba->HAregaddr); 
	spin_unlock(&phba->hbalock);


	
	status1 = phba->ha_copy & (HA_MBATT | HA_LATT | HA_ERATT);

	
	status2 = (phba->ha_copy & (HA_RXMASK  << (4*LPFC_ELS_RING)));
	status2 >>= (4*LPFC_ELS_RING);

	if (status1 || (status2 & HA_RXMASK))
		sp_irq_rc = lpfc_sli_sp_intr_handler(irq, dev_id);
	else
		sp_irq_rc = IRQ_NONE;


	
	status1 = (phba->ha_copy & (HA_RXMASK << (4*LPFC_FCP_RING)));
	status1 >>= (4*LPFC_FCP_RING);

	
	if (phba->cfg_multi_ring_support == 2) {
		status2 = (phba->ha_copy & (HA_RXMASK << (4*LPFC_EXTRA_RING)));
		status2 >>= (4*LPFC_EXTRA_RING);
	} else
		status2 = 0;

	if ((status1 & HA_RXMASK) || (status2 & HA_RXMASK))
		fp_irq_rc = lpfc_sli_fp_intr_handler(irq, dev_id);
	else
		fp_irq_rc = IRQ_NONE;

	
	return (sp_irq_rc == IRQ_HANDLED) ? sp_irq_rc : fp_irq_rc;
}  

void lpfc_sli4_fcp_xri_abort_event_proc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;

	
	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~FCP_XRI_ABORT_EVENT;
	spin_unlock_irq(&phba->hbalock);
	
	while (!list_empty(&phba->sli4_hba.sp_fcp_xri_aborted_work_queue)) {
		
		spin_lock_irq(&phba->hbalock);
		list_remove_head(&phba->sli4_hba.sp_fcp_xri_aborted_work_queue,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irq(&phba->hbalock);
		
		lpfc_sli4_fcp_xri_aborted(phba, &cq_event->cqe.wcqe_axri);
		
		lpfc_sli4_cq_event_release(phba, cq_event);
	}
}

void lpfc_sli4_els_xri_abort_event_proc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;

	
	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~ELS_XRI_ABORT_EVENT;
	spin_unlock_irq(&phba->hbalock);
	
	while (!list_empty(&phba->sli4_hba.sp_els_xri_aborted_work_queue)) {
		
		spin_lock_irq(&phba->hbalock);
		list_remove_head(&phba->sli4_hba.sp_els_xri_aborted_work_queue,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irq(&phba->hbalock);
		
		lpfc_sli4_els_xri_aborted(phba, &cq_event->cqe.wcqe_axri);
		
		lpfc_sli4_cq_event_release(phba, cq_event);
	}
}

static void
lpfc_sli4_iocb_param_transfer(struct lpfc_hba *phba,
			      struct lpfc_iocbq *pIocbIn,
			      struct lpfc_iocbq *pIocbOut,
			      struct lpfc_wcqe_complete *wcqe)
{
	unsigned long iflags;
	uint32_t status;
	size_t offset = offsetof(struct lpfc_iocbq, iocb);

	memcpy((char *)pIocbIn + offset, (char *)pIocbOut + offset,
	       sizeof(struct lpfc_iocbq) - offset);
	
	status = bf_get(lpfc_wcqe_c_status, wcqe);
	pIocbIn->iocb.ulpStatus = (status & LPFC_IOCB_STATUS_MASK);
	if (pIocbOut->iocb_flag & LPFC_IO_FCP)
		if (pIocbIn->iocb.ulpStatus == IOSTAT_FCP_RSP_ERROR)
			pIocbIn->iocb.un.fcpi.fcpi_parm =
					pIocbOut->iocb.un.fcpi.fcpi_parm -
					wcqe->total_data_placed;
		else
			pIocbIn->iocb.un.ulpWord[4] = wcqe->parameter;
	else {
		pIocbIn->iocb.un.ulpWord[4] = wcqe->parameter;
		pIocbIn->iocb.un.genreq64.bdl.bdeSize = wcqe->total_data_placed;
	}

	
	if (status == CQE_STATUS_DI_ERROR) {
		pIocbIn->iocb.ulpStatus = IOSTAT_LOCAL_REJECT;

		if (bf_get(lpfc_wcqe_c_bg_edir, wcqe))
			pIocbIn->iocb.un.ulpWord[4] = IOERR_RX_DMA_FAILED;
		else
			pIocbIn->iocb.un.ulpWord[4] = IOERR_TX_DMA_FAILED;

		pIocbIn->iocb.unsli3.sli3_bg.bgstat = 0;
		if (bf_get(lpfc_wcqe_c_bg_ge, wcqe)) 
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				BGS_GUARD_ERR_MASK;
		if (bf_get(lpfc_wcqe_c_bg_ae, wcqe)) 
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				BGS_APPTAG_ERR_MASK;
		if (bf_get(lpfc_wcqe_c_bg_re, wcqe)) 
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				BGS_REFTAG_ERR_MASK;

		
		if (bf_get(lpfc_wcqe_c_bg_tdpv, wcqe)) {
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				BGS_HI_WATER_MARK_PRESENT_MASK;
			pIocbIn->iocb.unsli3.sli3_bg.bghm =
				wcqe->total_data_placed;
		}

		if (!pIocbIn->iocb.unsli3.sli3_bg.bgstat)
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				(BGS_REFTAG_ERR_MASK | BGS_APPTAG_ERR_MASK |
				BGS_GUARD_ERR_MASK);
	}

	
	if (bf_get(lpfc_wcqe_c_xb, wcqe)) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		pIocbIn->iocb_flag |= LPFC_EXCHANGE_BUSY;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}
}

static struct lpfc_iocbq *
lpfc_sli4_els_wcqe_to_rspiocbq(struct lpfc_hba *phba,
			       struct lpfc_iocbq *irspiocbq)
{
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	struct lpfc_iocbq *cmdiocbq;
	struct lpfc_wcqe_complete *wcqe;
	unsigned long iflags;

	wcqe = &irspiocbq->cq_event.cqe.wcqe_cmpl;
	spin_lock_irqsave(&phba->hbalock, iflags);
	pring->stats.iocb_event++;
	
	cmdiocbq = lpfc_sli_iocbq_lookup_by_tag(phba, pring,
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	if (unlikely(!cmdiocbq)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0386 ELS complete with no corresponding "
				"cmdiocb: iotag (%d)\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
		lpfc_sli_release_iocbq(phba, irspiocbq);
		return NULL;
	}

	
	lpfc_sli4_iocb_param_transfer(phba, irspiocbq, cmdiocbq, wcqe);

	return irspiocbq;
}

static bool
lpfc_sli4_sp_handle_async_event(struct lpfc_hba *phba, struct lpfc_mcqe *mcqe)
{
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0392 Async Event: word0:x%x, word1:x%x, "
			"word2:x%x, word3:x%x\n", mcqe->word0,
			mcqe->mcqe_tag0, mcqe->mcqe_tag1, mcqe->trailer);

	
	cq_event = lpfc_sli4_cq_event_alloc(phba);
	if (!cq_event) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0394 Failed to allocate CQ_EVENT entry\n");
		return false;
	}

	
	memcpy(&cq_event->cqe, mcqe, sizeof(struct lpfc_mcqe));
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_add_tail(&cq_event->list, &phba->sli4_hba.sp_asynce_work_queue);
	
	phba->hba_flag |= ASYNC_EVENT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return true;
}

static bool
lpfc_sli4_sp_handle_mbox_event(struct lpfc_hba *phba, struct lpfc_mcqe *mcqe)
{
	uint32_t mcqe_status;
	MAILBOX_t *mbox, *pmbox;
	struct lpfc_mqe *mqe;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *mp;
	unsigned long iflags;
	LPFC_MBOXQ_t *pmb;
	bool workposted = false;
	int rc;

	
	if (!bf_get(lpfc_trailer_completed, mcqe))
		goto out_no_mqe_complete;

	
	spin_lock_irqsave(&phba->hbalock, iflags);
	pmb = phba->sli.mbox_active;
	if (unlikely(!pmb)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
				"1832 No pending MBOX command to handle\n");
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		goto out_no_mqe_complete;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	mqe = &pmb->u.mqe;
	pmbox = (MAILBOX_t *)&pmb->u.mqe;
	mbox = phba->mbox;
	vport = pmb->vport;

	
	phba->last_completion_time = jiffies;
	del_timer(&phba->sli.mbox_tmo);

	
	if (pmb->mbox_cmpl && mbox)
		lpfc_sli_pcimem_bcopy(mbox, mqe, sizeof(struct lpfc_mqe));

	mcqe_status = bf_get(lpfc_mcqe_status, mcqe);
	if (mcqe_status != MB_CQE_STATUS_SUCCESS) {
		if (bf_get(lpfc_mqe_status, mqe) == MBX_SUCCESS)
			bf_set(lpfc_mqe_status, mqe,
			       (LPFC_MBX_ERROR_RANGE | mcqe_status));
	}
	if (pmb->mbox_flag & LPFC_MBX_IMED_UNREG) {
		pmb->mbox_flag &= ~LPFC_MBX_IMED_UNREG;
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_MBOX_VPORT,
				      "MBOX dflt rpi: status:x%x rpi:x%x",
				      mcqe_status,
				      pmbox->un.varWords[0], 0);
		if (mcqe_status == MB_CQE_STATUS_SUCCESS) {
			mp = (struct lpfc_dmabuf *)(pmb->context1);
			ndlp = (struct lpfc_nodelist *)pmb->context2;
			lpfc_unreg_login(phba, vport->vpi,
					 pmbox->un.varWords[0], pmb);
			pmb->mbox_cmpl = lpfc_mbx_cmpl_dflt_rpi;
			pmb->context1 = mp;
			pmb->context2 = ndlp;
			pmb->vport = vport;
			rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
			if (rc != MBX_BUSY)
				lpfc_printf_log(phba, KERN_ERR, LOG_MBOX |
						LOG_SLI, "0385 rc should "
						"have been MBX_BUSY\n");
			if (rc != MBX_NOT_FINISHED)
				goto send_current_mbox;
		}
	}
	spin_lock_irqsave(&phba->pport->work_port_lock, iflags);
	phba->pport->work_port_events &= ~WORKER_MBOX_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflags);

	
	spin_lock_irqsave(&phba->hbalock, iflags);
	__lpfc_mbox_cmpl_put(phba, pmb);
	phba->work_ha |= HA_MBATT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	workposted = true;

send_current_mbox:
	spin_lock_irqsave(&phba->hbalock, iflags);
	
	phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	
	phba->sli.mbox_active = NULL;
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	
	lpfc_worker_wake_up(phba);
out_no_mqe_complete:
	if (bf_get(lpfc_trailer_consumed, mcqe))
		lpfc_sli4_mq_release(phba->sli4_hba.mbx_wq);
	return workposted;
}

static bool
lpfc_sli4_sp_handle_mcqe(struct lpfc_hba *phba, struct lpfc_cqe *cqe)
{
	struct lpfc_mcqe mcqe;
	bool workposted;

	
	lpfc_sli_pcimem_bcopy(cqe, &mcqe, sizeof(struct lpfc_mcqe));

	
	if (!bf_get(lpfc_trailer_async, &mcqe))
		workposted = lpfc_sli4_sp_handle_mbox_event(phba, &mcqe);
	else
		workposted = lpfc_sli4_sp_handle_async_event(phba, &mcqe);
	return workposted;
}

static bool
lpfc_sli4_sp_handle_els_wcqe(struct lpfc_hba *phba,
			     struct lpfc_wcqe_complete *wcqe)
{
	struct lpfc_iocbq *irspiocbq;
	unsigned long iflags;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_FCP_RING];

	
	irspiocbq = lpfc_sli_get_iocbq(phba);
	if (!irspiocbq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0387 NO IOCBQ data: txq_cnt=%d iocb_cnt=%d "
			"fcp_txcmplq_cnt=%d, els_txcmplq_cnt=%d\n",
			pring->txq_cnt, phba->iocb_cnt,
			phba->sli.ring[LPFC_FCP_RING].txcmplq_cnt,
			phba->sli.ring[LPFC_ELS_RING].txcmplq_cnt);
		return false;
	}

	
	memcpy(&irspiocbq->cq_event.cqe.wcqe_cmpl, wcqe, sizeof(*wcqe));
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_add_tail(&irspiocbq->cq_event.list,
		      &phba->sli4_hba.sp_queue_event);
	phba->hba_flag |= HBA_SP_QUEUE_EVT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return true;
}

static void
lpfc_sli4_sp_handle_rel_wcqe(struct lpfc_hba *phba,
			     struct lpfc_wcqe_release *wcqe)
{
	
	if (unlikely(!phba->sli4_hba.els_wq))
		return;
	
	if (bf_get(lpfc_wcqe_r_wq_id, wcqe) == phba->sli4_hba.els_wq->queue_id)
		lpfc_sli4_wq_release(phba->sli4_hba.els_wq,
				     bf_get(lpfc_wcqe_r_wqe_index, wcqe));
	else
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2579 Slow-path wqe consume event carries "
				"miss-matched qid: wcqe-qid=x%x, sp-qid=x%x\n",
				bf_get(lpfc_wcqe_r_wqe_index, wcqe),
				phba->sli4_hba.els_wq->queue_id);
}

static bool
lpfc_sli4_sp_handle_abort_xri_wcqe(struct lpfc_hba *phba,
				   struct lpfc_queue *cq,
				   struct sli4_wcqe_xri_aborted *wcqe)
{
	bool workposted = false;
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	
	cq_event = lpfc_sli4_cq_event_alloc(phba);
	if (!cq_event) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0602 Failed to allocate CQ_EVENT entry\n");
		return false;
	}

	
	memcpy(&cq_event->cqe, wcqe, sizeof(struct sli4_wcqe_xri_aborted));
	switch (cq->subtype) {
	case LPFC_FCP:
		spin_lock_irqsave(&phba->hbalock, iflags);
		list_add_tail(&cq_event->list,
			      &phba->sli4_hba.sp_fcp_xri_aborted_work_queue);
		
		phba->hba_flag |= FCP_XRI_ABORT_EVENT;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	case LPFC_ELS:
		spin_lock_irqsave(&phba->hbalock, iflags);
		list_add_tail(&cq_event->list,
			      &phba->sli4_hba.sp_els_xri_aborted_work_queue);
		
		phba->hba_flag |= ELS_XRI_ABORT_EVENT;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0603 Invalid work queue CQE subtype (x%x)\n",
				cq->subtype);
		workposted = false;
		break;
	}
	return workposted;
}

static bool
lpfc_sli4_sp_handle_rcqe(struct lpfc_hba *phba, struct lpfc_rcqe *rcqe)
{
	bool workposted = false;
	struct lpfc_queue *hrq = phba->sli4_hba.hdr_rq;
	struct lpfc_queue *drq = phba->sli4_hba.dat_rq;
	struct hbq_dmabuf *dma_buf;
	uint32_t status, rq_id;
	unsigned long iflags;

	
	if (unlikely(!hrq) || unlikely(!drq))
		return workposted;

	if (bf_get(lpfc_cqe_code, rcqe) == CQE_CODE_RECEIVE_V1)
		rq_id = bf_get(lpfc_rcqe_rq_id_v1, rcqe);
	else
		rq_id = bf_get(lpfc_rcqe_rq_id, rcqe);
	if (rq_id != hrq->queue_id)
		goto out;

	status = bf_get(lpfc_rcqe_status, rcqe);
	switch (status) {
	case FC_STATUS_RQ_BUF_LEN_EXCEEDED:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2537 Receive Frame Truncated!!\n");
	case FC_STATUS_RQ_SUCCESS:
		lpfc_sli4_rq_release(hrq, drq);
		spin_lock_irqsave(&phba->hbalock, iflags);
		dma_buf = lpfc_sli_hbqbuf_get(&phba->hbqs[0].hbq_buffer_list);
		if (!dma_buf) {
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			goto out;
		}
		memcpy(&dma_buf->cq_event.cqe.rcqe_cmpl, rcqe, sizeof(*rcqe));
		
		list_add_tail(&dma_buf->cq_event.list,
			      &phba->sli4_hba.sp_queue_event);
		
		phba->hba_flag |= HBA_SP_QUEUE_EVT;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	case FC_STATUS_INSUFF_BUF_NEED_BUF:
	case FC_STATUS_INSUFF_BUF_FRM_DISC:
		
		spin_lock_irqsave(&phba->hbalock, iflags);
		phba->hba_flag |= HBA_POST_RECEIVE_BUFFER;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	}
out:
	return workposted;
}

static bool
lpfc_sli4_sp_handle_cqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			 struct lpfc_cqe *cqe)
{
	struct lpfc_cqe cqevt;
	bool workposted = false;

	
	lpfc_sli_pcimem_bcopy(cqe, &cqevt, sizeof(struct lpfc_cqe));

	
	switch (bf_get(lpfc_cqe_code, &cqevt)) {
	case CQE_CODE_COMPL_WQE:
		
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_els_wcqe(phba,
				(struct lpfc_wcqe_complete *)&cqevt);
		break;
	case CQE_CODE_RELEASE_WQE:
		
		lpfc_sli4_sp_handle_rel_wcqe(phba,
				(struct lpfc_wcqe_release *)&cqevt);
		break;
	case CQE_CODE_XRI_ABORTED:
		
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_abort_xri_wcqe(phba, cq,
				(struct sli4_wcqe_xri_aborted *)&cqevt);
		break;
	case CQE_CODE_RECEIVE:
	case CQE_CODE_RECEIVE_V1:
		
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_rcqe(phba,
				(struct lpfc_rcqe *)&cqevt);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0388 Not a valid WCQE code: x%x\n",
				bf_get(lpfc_cqe_code, &cqevt));
		break;
	}
	return workposted;
}

static void
lpfc_sli4_sp_handle_eqe(struct lpfc_hba *phba, struct lpfc_eqe *eqe)
{
	struct lpfc_queue *cq = NULL, *childq, *speq;
	struct lpfc_cqe *cqe;
	bool workposted = false;
	int ecount = 0;
	uint16_t cqid;

	if (bf_get_le32(lpfc_eqe_major_code, eqe) != 0) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0359 Not a valid slow-path completion "
				"event: majorcode=x%x, minorcode=x%x\n",
				bf_get_le32(lpfc_eqe_major_code, eqe),
				bf_get_le32(lpfc_eqe_minor_code, eqe));
		return;
	}

	
	cqid = bf_get_le32(lpfc_eqe_resource_id, eqe);

	
	speq = phba->sli4_hba.sp_eq;
	
	if (unlikely(!speq))
		return;
	list_for_each_entry(childq, &speq->child_list, list) {
		if (childq->queue_id == cqid) {
			cq = childq;
			break;
		}
	}
	if (unlikely(!cq)) {
		if (phba->sli.sli_flag & LPFC_SLI_ACTIVE)
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0365 Slow-path CQ identifier "
					"(%d) does not exist\n", cqid);
		return;
	}

	
	switch (cq->type) {
	case LPFC_MCQ:
		while ((cqe = lpfc_sli4_cq_get(cq))) {
			workposted |= lpfc_sli4_sp_handle_mcqe(phba, cqe);
			if (!(++ecount % cq->entry_repost))
				lpfc_sli4_cq_release(cq, LPFC_QUEUE_NOARM);
		}
		break;
	case LPFC_WCQ:
		while ((cqe = lpfc_sli4_cq_get(cq))) {
			if (cq->subtype == LPFC_FCP)
				workposted |= lpfc_sli4_fp_handle_wcqe(phba, cq,
								       cqe);
			else
				workposted |= lpfc_sli4_sp_handle_cqe(phba, cq,
								      cqe);
			if (!(++ecount % cq->entry_repost))
				lpfc_sli4_cq_release(cq, LPFC_QUEUE_NOARM);
		}
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0370 Invalid completion queue type (%d)\n",
				cq->type);
		return;
	}

	
	if (unlikely(ecount == 0))
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0371 No entry from the CQ: identifier "
				"(x%x), type (%d)\n", cq->queue_id, cq->type);

	
	lpfc_sli4_cq_release(cq, LPFC_QUEUE_REARM);

	
	if (workposted)
		lpfc_worker_wake_up(phba);
}

static void
lpfc_sli4_fp_handle_fcp_wcqe(struct lpfc_hba *phba,
			     struct lpfc_wcqe_complete *wcqe)
{
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_FCP_RING];
	struct lpfc_iocbq *cmdiocbq;
	struct lpfc_iocbq irspiocbq;
	unsigned long iflags;

	spin_lock_irqsave(&phba->hbalock, iflags);
	pring->stats.iocb_event++;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	
	if (unlikely(bf_get(lpfc_wcqe_c_status, wcqe))) {
		if ((bf_get(lpfc_wcqe_c_status, wcqe) ==
		     IOSTAT_LOCAL_REJECT) &&
		    (wcqe->parameter == IOERR_NO_RESOURCES)) {
			phba->lpfc_rampdown_queue_depth(phba);
		}
		
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0373 FCP complete error: status=x%x, "
				"hw_status=x%x, total_data_specified=%d, "
				"parameter=x%x, word3=x%x\n",
				bf_get(lpfc_wcqe_c_status, wcqe),
				bf_get(lpfc_wcqe_c_hw_status, wcqe),
				wcqe->total_data_placed, wcqe->parameter,
				wcqe->word3);
	}

	
	spin_lock_irqsave(&phba->hbalock, iflags);
	cmdiocbq = lpfc_sli_iocbq_lookup_by_tag(phba, pring,
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if (unlikely(!cmdiocbq)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0374 FCP complete with no corresponding "
				"cmdiocb: iotag (%d)\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
		return;
	}
	if (unlikely(!cmdiocbq->iocb_cmpl)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0375 FCP cmdiocb not callback function "
				"iotag: (%d)\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
		return;
	}

	
	lpfc_sli4_iocb_param_transfer(phba, &irspiocbq, cmdiocbq, wcqe);

	if (cmdiocbq->iocb_flag & LPFC_DRIVER_ABORTED) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		cmdiocbq->iocb_flag &= ~LPFC_DRIVER_ABORTED;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}

	
	(cmdiocbq->iocb_cmpl)(phba, cmdiocbq, &irspiocbq);
}

static void
lpfc_sli4_fp_handle_rel_wcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			     struct lpfc_wcqe_release *wcqe)
{
	struct lpfc_queue *childwq;
	bool wqid_matched = false;
	uint16_t fcp_wqid;

	
	fcp_wqid = bf_get(lpfc_wcqe_r_wq_id, wcqe);
	list_for_each_entry(childwq, &cq->child_list, list) {
		if (childwq->queue_id == fcp_wqid) {
			lpfc_sli4_wq_release(childwq,
					bf_get(lpfc_wcqe_r_wqe_index, wcqe));
			wqid_matched = true;
			break;
		}
	}
	
	if (wqid_matched != true)
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2580 Fast-path wqe consume event carries "
				"miss-matched qid: wcqe-qid=x%x\n", fcp_wqid);
}

static int
lpfc_sli4_fp_handle_wcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			 struct lpfc_cqe *cqe)
{
	struct lpfc_wcqe_release wcqe;
	bool workposted = false;

	
	lpfc_sli_pcimem_bcopy(cqe, &wcqe, sizeof(struct lpfc_cqe));

	
	switch (bf_get(lpfc_wcqe_c_code, &wcqe)) {
	case CQE_CODE_COMPL_WQE:
		
		phba->last_completion_time = jiffies;
		lpfc_sli4_fp_handle_fcp_wcqe(phba,
				(struct lpfc_wcqe_complete *)&wcqe);
		break;
	case CQE_CODE_RELEASE_WQE:
		
		lpfc_sli4_fp_handle_rel_wcqe(phba, cq,
				(struct lpfc_wcqe_release *)&wcqe);
		break;
	case CQE_CODE_XRI_ABORTED:
		
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_abort_xri_wcqe(phba, cq,
				(struct sli4_wcqe_xri_aborted *)&wcqe);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0144 Not a valid WCQE code: x%x\n",
				bf_get(lpfc_wcqe_c_code, &wcqe));
		break;
	}
	return workposted;
}

static void
lpfc_sli4_fp_handle_eqe(struct lpfc_hba *phba, struct lpfc_eqe *eqe,
			uint32_t fcp_cqidx)
{
	struct lpfc_queue *cq;
	struct lpfc_cqe *cqe;
	bool workposted = false;
	uint16_t cqid;
	int ecount = 0;

	if (unlikely(bf_get_le32(lpfc_eqe_major_code, eqe) != 0)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0366 Not a valid fast-path completion "
				"event: majorcode=x%x, minorcode=x%x\n",
				bf_get_le32(lpfc_eqe_major_code, eqe),
				bf_get_le32(lpfc_eqe_minor_code, eqe));
		return;
	}

	if (unlikely(!phba->sli4_hba.fcp_cq)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3146 Fast-path completion queues "
				"does not exist\n");
		return;
	}
	cq = phba->sli4_hba.fcp_cq[fcp_cqidx];
	if (unlikely(!cq)) {
		if (phba->sli.sli_flag & LPFC_SLI_ACTIVE)
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0367 Fast-path completion queue "
					"(%d) does not exist\n", fcp_cqidx);
		return;
	}

	
	cqid = bf_get_le32(lpfc_eqe_resource_id, eqe);
	if (unlikely(cqid != cq->queue_id)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0368 Miss-matched fast-path completion "
				"queue identifier: eqcqid=%d, fcpcqid=%d\n",
				cqid, cq->queue_id);
		return;
	}

	
	while ((cqe = lpfc_sli4_cq_get(cq))) {
		workposted |= lpfc_sli4_fp_handle_wcqe(phba, cq, cqe);
		if (!(++ecount % cq->entry_repost))
			lpfc_sli4_cq_release(cq, LPFC_QUEUE_NOARM);
	}

	
	if (unlikely(ecount == 0))
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0369 No entry from fast-path completion "
				"queue fcpcqid=%d\n", cq->queue_id);

	
	lpfc_sli4_cq_release(cq, LPFC_QUEUE_REARM);

	
	if (workposted)
		lpfc_worker_wake_up(phba);
}

static void
lpfc_sli4_eq_flush(struct lpfc_hba *phba, struct lpfc_queue *eq)
{
	struct lpfc_eqe *eqe;

	
	while ((eqe = lpfc_sli4_eq_get(eq)))
		;

	
	lpfc_sli4_eq_release(eq, LPFC_QUEUE_REARM);
}

irqreturn_t
lpfc_sli4_sp_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba *phba;
	struct lpfc_queue *speq;
	struct lpfc_eqe *eqe;
	unsigned long iflag;
	int ecount = 0;

	phba = (struct lpfc_hba *)dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	
	speq = phba->sli4_hba.sp_eq;
	if (unlikely(!speq))
		return IRQ_NONE;

	
	if (unlikely(lpfc_intr_state_check(phba))) {
		
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (phba->link_state < LPFC_LINK_DOWN)
			
			lpfc_sli4_eq_flush(phba, speq);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return IRQ_NONE;
	}

	while ((eqe = lpfc_sli4_eq_get(speq))) {
		lpfc_sli4_sp_handle_eqe(phba, eqe);
		if (!(++ecount % speq->entry_repost))
			lpfc_sli4_eq_release(speq, LPFC_QUEUE_NOARM);
	}

	
	lpfc_sli4_eq_release(speq, LPFC_QUEUE_REARM);

	
	if (unlikely(ecount == 0)) {
		if (phba->intr_type == MSIX)
			
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0357 MSI-X interrupt with no EQE\n");
		else
			
			return IRQ_NONE;
	}

	return IRQ_HANDLED;
} 

irqreturn_t
lpfc_sli4_fp_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba *phba;
	struct lpfc_fcp_eq_hdl *fcp_eq_hdl;
	struct lpfc_queue *fpeq;
	struct lpfc_eqe *eqe;
	unsigned long iflag;
	int ecount = 0;
	uint32_t fcp_eqidx;

	
	fcp_eq_hdl = (struct lpfc_fcp_eq_hdl *)dev_id;
	phba = fcp_eq_hdl->phba;
	fcp_eqidx = fcp_eq_hdl->idx;

	if (unlikely(!phba))
		return IRQ_NONE;
	if (unlikely(!phba->sli4_hba.fp_eq))
		return IRQ_NONE;

	
	fpeq = phba->sli4_hba.fp_eq[fcp_eqidx];
	if (unlikely(!fpeq))
		return IRQ_NONE;

	
	if (unlikely(lpfc_intr_state_check(phba))) {
		
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (phba->link_state < LPFC_LINK_DOWN)
			
			lpfc_sli4_eq_flush(phba, fpeq);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return IRQ_NONE;
	}

	while ((eqe = lpfc_sli4_eq_get(fpeq))) {
		lpfc_sli4_fp_handle_eqe(phba, eqe, fcp_eqidx);
		if (!(++ecount % fpeq->entry_repost))
			lpfc_sli4_eq_release(fpeq, LPFC_QUEUE_NOARM);
	}

	
	lpfc_sli4_eq_release(fpeq, LPFC_QUEUE_REARM);

	if (unlikely(ecount == 0)) {
		if (phba->intr_type == MSIX)
			
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0358 MSI-X interrupt with no EQE\n");
		else
			
			return IRQ_NONE;
	}

	return IRQ_HANDLED;
} 

irqreturn_t
lpfc_sli4_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	irqreturn_t sp_irq_rc, fp_irq_rc;
	bool fp_handled = false;
	uint32_t fcp_eqidx;

	
	phba = (struct lpfc_hba *)dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	sp_irq_rc = lpfc_sli4_sp_intr_handler(irq, dev_id);

	for (fcp_eqidx = 0; fcp_eqidx < phba->cfg_fcp_eq_count; fcp_eqidx++) {
		fp_irq_rc = lpfc_sli4_fp_intr_handler(irq,
					&phba->sli4_hba.fcp_eq_hdl[fcp_eqidx]);
		if (fp_irq_rc == IRQ_HANDLED)
			fp_handled |= true;
	}

	return (fp_handled == true) ? IRQ_HANDLED : sp_irq_rc;
} 

void
lpfc_sli4_queue_free(struct lpfc_queue *queue)
{
	struct lpfc_dmabuf *dmabuf;

	if (!queue)
		return;

	while (!list_empty(&queue->page_list)) {
		list_remove_head(&queue->page_list, dmabuf, struct lpfc_dmabuf,
				 list);
		dma_free_coherent(&queue->phba->pcidev->dev, SLI4_PAGE_SIZE,
				  dmabuf->virt, dmabuf->phys);
		kfree(dmabuf);
	}
	kfree(queue);
	return;
}

struct lpfc_queue *
lpfc_sli4_queue_alloc(struct lpfc_hba *phba, uint32_t entry_size,
		      uint32_t entry_count)
{
	struct lpfc_queue *queue;
	struct lpfc_dmabuf *dmabuf;
	int x, total_qe_count;
	void *dma_pointer;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	queue = kzalloc(sizeof(struct lpfc_queue) +
			(sizeof(union sli4_qe) * entry_count), GFP_KERNEL);
	if (!queue)
		return NULL;
	queue->page_count = (ALIGN(entry_size * entry_count,
			hw_page_size))/hw_page_size;
	INIT_LIST_HEAD(&queue->list);
	INIT_LIST_HEAD(&queue->page_list);
	INIT_LIST_HEAD(&queue->child_list);
	for (x = 0, total_qe_count = 0; x < queue->page_count; x++) {
		dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
		if (!dmabuf)
			goto out_fail;
		dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
						  hw_page_size, &dmabuf->phys,
						  GFP_KERNEL);
		if (!dmabuf->virt) {
			kfree(dmabuf);
			goto out_fail;
		}
		memset(dmabuf->virt, 0, hw_page_size);
		dmabuf->buffer_tag = x;
		list_add_tail(&dmabuf->list, &queue->page_list);
		
		dma_pointer = dmabuf->virt;
		for (; total_qe_count < entry_count &&
		     dma_pointer < (hw_page_size + dmabuf->virt);
		     total_qe_count++, dma_pointer += entry_size) {
			queue->qe[total_qe_count].address = dma_pointer;
		}
	}
	queue->entry_size = entry_size;
	queue->entry_count = entry_count;

	queue->entry_repost = (entry_count >> 3);
	if (queue->entry_repost < LPFC_QUEUE_MIN_REPOST)
		queue->entry_repost = LPFC_QUEUE_MIN_REPOST;
	queue->phba = phba;

	return queue;
out_fail:
	lpfc_sli4_queue_free(queue);
	return NULL;
}

uint32_t
lpfc_eq_create(struct lpfc_hba *phba, struct lpfc_queue *eq, uint16_t imax)
{
	struct lpfc_mbx_eq_create *eq_create;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	struct lpfc_dmabuf *dmabuf;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint16_t dmult;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	
	if (!eq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_eq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_EQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	eq_create = &mbox->u.mqe.un.eq_create;
	bf_set(lpfc_mbx_eq_create_num_pages, &eq_create->u.request,
	       eq->page_count);
	bf_set(lpfc_eq_context_size, &eq_create->u.request.context,
	       LPFC_EQE_SIZE);
	bf_set(lpfc_eq_context_valid, &eq_create->u.request.context, 1);
	
	dmult = LPFC_DMULT_CONST/imax - 1;
	bf_set(lpfc_eq_context_delay_multi, &eq_create->u.request.context,
	       dmult);
	switch (eq->entry_count) {
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0360 Unsupported EQ count. (%d)\n",
				eq->entry_count);
		if (eq->entry_count < 256)
			return -EINVAL;
		
	case 256:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_256);
		break;
	case 512:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_512);
		break;
	case 1024:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_1024);
		break;
	case 2048:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_2048);
		break;
	case 4096:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_4096);
		break;
	}
	list_for_each_entry(dmabuf, &eq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		eq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		eq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->context1 = NULL;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *) &eq_create->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2500 EQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	eq->type = LPFC_EQ;
	eq->subtype = LPFC_NONE;
	eq->queue_id = bf_get(lpfc_mbx_eq_create_q_id, &eq_create->u.response);
	if (eq->queue_id == 0xFFFF)
		status = -ENXIO;
	eq->host_index = 0;
	eq->hba_index = 0;

	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

uint32_t
lpfc_cq_create(struct lpfc_hba *phba, struct lpfc_queue *cq,
	       struct lpfc_queue *eq, uint32_t type, uint32_t subtype)
{
	struct lpfc_mbx_cq_create *cq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	
	if (!cq || !eq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_cq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_CQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	cq_create = &mbox->u.mqe.un.cq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &cq_create->header.cfg_shdr;
	bf_set(lpfc_mbx_cq_create_num_pages, &cq_create->u.request,
		    cq->page_count);
	bf_set(lpfc_cq_context_event, &cq_create->u.request.context, 1);
	bf_set(lpfc_cq_context_valid, &cq_create->u.request.context, 1);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.cqv);
	if (phba->sli4_hba.pc_sli4_params.cqv == LPFC_Q_CREATE_VERSION_2) {
		
		bf_set(lpfc_mbx_cq_create_page_size, &cq_create->u.request, 1);
		bf_set(lpfc_cq_eq_id_2, &cq_create->u.request.context,
		       eq->queue_id);
	} else {
		bf_set(lpfc_cq_eq_id, &cq_create->u.request.context,
		       eq->queue_id);
	}
	switch (cq->entry_count) {
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0361 Unsupported CQ count. (%d)\n",
				cq->entry_count);
		if (cq->entry_count < 256)
			return -EINVAL;
		
	case 256:
		bf_set(lpfc_cq_context_count, &cq_create->u.request.context,
		       LPFC_CQ_CNT_256);
		break;
	case 512:
		bf_set(lpfc_cq_context_count, &cq_create->u.request.context,
		       LPFC_CQ_CNT_512);
		break;
	case 1024:
		bf_set(lpfc_cq_context_count, &cq_create->u.request.context,
		       LPFC_CQ_CNT_1024);
		break;
	}
	list_for_each_entry(dmabuf, &cq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		cq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		cq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);

	
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2501 CQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	cq->queue_id = bf_get(lpfc_mbx_cq_create_q_id, &cq_create->u.response);
	if (cq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	
	list_add_tail(&cq->list, &eq->child_list);
	
	cq->type = type;
	cq->subtype = subtype;
	cq->queue_id = bf_get(lpfc_mbx_cq_create_q_id, &cq_create->u.response);
	cq->assoc_qid = eq->queue_id;
	cq->host_index = 0;
	cq->hba_index = 0;

out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

static void
lpfc_mq_create_fb_init(struct lpfc_hba *phba, struct lpfc_queue *mq,
		       LPFC_MBOXQ_t *mbox, struct lpfc_queue *cq)
{
	struct lpfc_mbx_mq_create *mq_create;
	struct lpfc_dmabuf *dmabuf;
	int length;

	length = (sizeof(struct lpfc_mbx_mq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	mq_create = &mbox->u.mqe.un.mq_create;
	bf_set(lpfc_mbx_mq_create_num_pages, &mq_create->u.request,
	       mq->page_count);
	bf_set(lpfc_mq_context_cq_id, &mq_create->u.request.context,
	       cq->queue_id);
	bf_set(lpfc_mq_context_valid, &mq_create->u.request.context, 1);
	switch (mq->entry_count) {
	case 16:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_16);
		break;
	case 32:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_32);
		break;
	case 64:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_64);
		break;
	case 128:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_128);
		break;
	}
	list_for_each_entry(dmabuf, &mq->page_list, list) {
		mq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
			putPaddrLow(dmabuf->phys);
		mq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
			putPaddrHigh(dmabuf->phys);
	}
}

int32_t
lpfc_mq_create(struct lpfc_hba *phba, struct lpfc_queue *mq,
	       struct lpfc_queue *cq, uint32_t subtype)
{
	struct lpfc_mbx_mq_create *mq_create;
	struct lpfc_mbx_mq_create_ext *mq_create_ext;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	
	if (!mq || !cq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_mq_create_ext) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MQ_CREATE_EXT,
			 length, LPFC_SLI4_MBX_EMBED);

	mq_create_ext = &mbox->u.mqe.un.mq_create_ext;
	shdr = (union lpfc_sli4_cfg_shdr *) &mq_create_ext->header.cfg_shdr;
	bf_set(lpfc_mbx_mq_create_ext_num_pages,
	       &mq_create_ext->u.request, mq->page_count);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_link,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_fip,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_group5,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_fc,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_sli,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mq_context_valid, &mq_create_ext->u.request.context, 1);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.mqv);
	if (phba->sli4_hba.pc_sli4_params.mqv == LPFC_Q_CREATE_VERSION_1)
		bf_set(lpfc_mbx_mq_create_ext_cq_id, &mq_create_ext->u.request,
		       cq->queue_id);
	else
		bf_set(lpfc_mq_context_cq_id, &mq_create_ext->u.request.context,
		       cq->queue_id);
	switch (mq->entry_count) {
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0362 Unsupported MQ count. (%d)\n",
				mq->entry_count);
		if (mq->entry_count < 16)
			return -EINVAL;
		
	case 16:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_16);
		break;
	case 32:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_32);
		break;
	case 64:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_64);
		break;
	case 128:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_128);
		break;
	}
	list_for_each_entry(dmabuf, &mq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		mq_create_ext->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		mq_create_ext->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	mq->queue_id = bf_get(lpfc_mbx_mq_create_q_id,
			      &mq_create_ext->u.response);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"2795 MQ_CREATE_EXT failed with "
				"status x%x. Failback to MQ_CREATE.\n",
				rc);
		lpfc_mq_create_fb_init(phba, mq, mbox, cq);
		mq_create = &mbox->u.mqe.un.mq_create;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
		shdr = (union lpfc_sli4_cfg_shdr *) &mq_create->header.cfg_shdr;
		mq->queue_id = bf_get(lpfc_mbx_mq_create_q_id,
				      &mq_create->u.response);
	}

	
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2502 MQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	if (mq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	mq->type = LPFC_MQ;
	mq->assoc_qid = cq->queue_id;
	mq->subtype = subtype;
	mq->host_index = 0;
	mq->hba_index = 0;

	
	list_add_tail(&mq->list, &cq->child_list);
out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

uint32_t
lpfc_wq_create(struct lpfc_hba *phba, struct lpfc_queue *wq,
	       struct lpfc_queue *cq, uint32_t subtype)
{
	struct lpfc_mbx_wq_create *wq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;
	struct dma_address *page;

	
	if (!wq || !cq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_wq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_WQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	wq_create = &mbox->u.mqe.un.wq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &wq_create->header.cfg_shdr;
	bf_set(lpfc_mbx_wq_create_num_pages, &wq_create->u.request,
		    wq->page_count);
	bf_set(lpfc_mbx_wq_create_cq_id, &wq_create->u.request,
		    cq->queue_id);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.wqv);
	if (phba->sli4_hba.pc_sli4_params.wqv == LPFC_Q_CREATE_VERSION_1) {
		bf_set(lpfc_mbx_wq_create_wqe_count, &wq_create->u.request_1,
		       wq->entry_count);
		switch (wq->entry_size) {
		default:
		case 64:
			bf_set(lpfc_mbx_wq_create_wqe_size,
			       &wq_create->u.request_1,
			       LPFC_WQ_WQE_SIZE_64);
			break;
		case 128:
			bf_set(lpfc_mbx_wq_create_wqe_size,
			       &wq_create->u.request_1,
			       LPFC_WQ_WQE_SIZE_128);
			break;
		}
		bf_set(lpfc_mbx_wq_create_page_size, &wq_create->u.request_1,
		       (PAGE_SIZE/SLI4_PAGE_SIZE));
		page = wq_create->u.request_1.page;
	} else {
		page = wq_create->u.request.page;
	}
	list_for_each_entry(dmabuf, &wq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		page[dmabuf->buffer_tag].addr_lo = putPaddrLow(dmabuf->phys);
		page[dmabuf->buffer_tag].addr_hi = putPaddrHigh(dmabuf->phys);
	}
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2503 WQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	wq->queue_id = bf_get(lpfc_mbx_wq_create_q_id, &wq_create->u.response);
	if (wq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	wq->type = LPFC_WQ;
	wq->assoc_qid = cq->queue_id;
	wq->subtype = subtype;
	wq->host_index = 0;
	wq->hba_index = 0;
	wq->entry_repost = LPFC_RELEASE_NOTIFICATION_INTERVAL;

	
	list_add_tail(&wq->list, &cq->child_list);
out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

void
lpfc_rq_adjust_repost(struct lpfc_hba *phba, struct lpfc_queue *rq, int qno)
{
	uint32_t cnt;

	
	if (!rq)
		return;
	cnt = lpfc_hbq_defs[qno]->entry_count;

	
	cnt = (cnt >> 3);
	if (cnt < LPFC_QUEUE_MIN_REPOST)
		cnt = LPFC_QUEUE_MIN_REPOST;

	rq->entry_repost = cnt;
}

uint32_t
lpfc_rq_create(struct lpfc_hba *phba, struct lpfc_queue *hrq,
	       struct lpfc_queue *drq, struct lpfc_queue *cq, uint32_t subtype)
{
	struct lpfc_mbx_rq_create *rq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	
	if (!hrq || !drq || !cq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	if (hrq->entry_count != drq->entry_count)
		return -EINVAL;
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_rq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_RQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	rq_create = &mbox->u.mqe.un.rq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &rq_create->header.cfg_shdr;
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.rqv);
	if (phba->sli4_hba.pc_sli4_params.rqv == LPFC_Q_CREATE_VERSION_1) {
		bf_set(lpfc_rq_context_rqe_count_1,
		       &rq_create->u.request.context,
		       hrq->entry_count);
		rq_create->u.request.context.buffer_size = LPFC_HDR_BUF_SIZE;
		bf_set(lpfc_rq_context_rqe_size,
		       &rq_create->u.request.context,
		       LPFC_RQE_SIZE_8);
		bf_set(lpfc_rq_context_page_size,
		       &rq_create->u.request.context,
		       (PAGE_SIZE/SLI4_PAGE_SIZE));
	} else {
		switch (hrq->entry_count) {
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2535 Unsupported RQ count. (%d)\n",
					hrq->entry_count);
			if (hrq->entry_count < 512)
				return -EINVAL;
			
		case 512:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_512);
			break;
		case 1024:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_1024);
			break;
		case 2048:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_2048);
			break;
		case 4096:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_4096);
			break;
		}
		bf_set(lpfc_rq_context_buf_size, &rq_create->u.request.context,
		       LPFC_HDR_BUF_SIZE);
	}
	bf_set(lpfc_rq_context_cq_id, &rq_create->u.request.context,
	       cq->queue_id);
	bf_set(lpfc_mbx_rq_create_num_pages, &rq_create->u.request,
	       hrq->page_count);
	list_for_each_entry(dmabuf, &hrq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		rq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		rq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2504 RQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	hrq->queue_id = bf_get(lpfc_mbx_rq_create_q_id, &rq_create->u.response);
	if (hrq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	hrq->type = LPFC_HRQ;
	hrq->assoc_qid = cq->queue_id;
	hrq->subtype = subtype;
	hrq->host_index = 0;
	hrq->hba_index = 0;

	
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_RQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.rqv);
	if (phba->sli4_hba.pc_sli4_params.rqv == LPFC_Q_CREATE_VERSION_1) {
		bf_set(lpfc_rq_context_rqe_count_1,
		       &rq_create->u.request.context, hrq->entry_count);
		rq_create->u.request.context.buffer_size = LPFC_DATA_BUF_SIZE;
		bf_set(lpfc_rq_context_rqe_size, &rq_create->u.request.context,
		       LPFC_RQE_SIZE_8);
		bf_set(lpfc_rq_context_page_size, &rq_create->u.request.context,
		       (PAGE_SIZE/SLI4_PAGE_SIZE));
	} else {
		switch (drq->entry_count) {
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2536 Unsupported RQ count. (%d)\n",
					drq->entry_count);
			if (drq->entry_count < 512)
				return -EINVAL;
			
		case 512:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_512);
			break;
		case 1024:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_1024);
			break;
		case 2048:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_2048);
			break;
		case 4096:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_4096);
			break;
		}
		bf_set(lpfc_rq_context_buf_size, &rq_create->u.request.context,
		       LPFC_DATA_BUF_SIZE);
	}
	bf_set(lpfc_rq_context_cq_id, &rq_create->u.request.context,
	       cq->queue_id);
	bf_set(lpfc_mbx_rq_create_num_pages, &rq_create->u.request,
	       drq->page_count);
	list_for_each_entry(dmabuf, &drq->page_list, list) {
		rq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		rq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	
	shdr = (union lpfc_sli4_cfg_shdr *) &rq_create->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		status = -ENXIO;
		goto out;
	}
	drq->queue_id = bf_get(lpfc_mbx_rq_create_q_id, &rq_create->u.response);
	if (drq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	drq->type = LPFC_DRQ;
	drq->assoc_qid = cq->queue_id;
	drq->subtype = subtype;
	drq->host_index = 0;
	drq->hba_index = 0;

	
	list_add_tail(&hrq->list, &cq->child_list);
	list_add_tail(&drq->list, &cq->child_list);

out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

uint32_t
lpfc_eq_destroy(struct lpfc_hba *phba, struct lpfc_queue *eq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	
	if (!eq)
		return -ENODEV;
	mbox = mempool_alloc(eq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_eq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_EQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_eq_destroy_q_id, &mbox->u.mqe.un.eq_destroy.u.request,
	       eq->queue_id);
	mbox->vport = eq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;

	rc = lpfc_sli_issue_mbox(eq->phba, mbox, MBX_POLL);
	
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.eq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2505 EQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}

	
	list_del_init(&eq->list);
	mempool_free(mbox, eq->phba->mbox_mem_pool);
	return status;
}

uint32_t
lpfc_cq_destroy(struct lpfc_hba *phba, struct lpfc_queue *cq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	
	if (!cq)
		return -ENODEV;
	mbox = mempool_alloc(cq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_cq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_CQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_cq_destroy_q_id, &mbox->u.mqe.un.cq_destroy.u.request,
	       cq->queue_id);
	mbox->vport = cq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(cq->phba, mbox, MBX_POLL);
	
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.wq_create.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2506 CQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	
	list_del_init(&cq->list);
	mempool_free(mbox, cq->phba->mbox_mem_pool);
	return status;
}

uint32_t
lpfc_mq_destroy(struct lpfc_hba *phba, struct lpfc_queue *mq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	
	if (!mq)
		return -ENODEV;
	mbox = mempool_alloc(mq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_mq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_mq_destroy_q_id, &mbox->u.mqe.un.mq_destroy.u.request,
	       mq->queue_id);
	mbox->vport = mq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(mq->phba, mbox, MBX_POLL);
	
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.mq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2507 MQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	
	list_del_init(&mq->list);
	mempool_free(mbox, mq->phba->mbox_mem_pool);
	return status;
}

uint32_t
lpfc_wq_destroy(struct lpfc_hba *phba, struct lpfc_queue *wq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	
	if (!wq)
		return -ENODEV;
	mbox = mempool_alloc(wq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_wq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_WQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_wq_destroy_q_id, &mbox->u.mqe.un.wq_destroy.u.request,
	       wq->queue_id);
	mbox->vport = wq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(wq->phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.wq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2508 WQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	
	list_del_init(&wq->list);
	mempool_free(mbox, wq->phba->mbox_mem_pool);
	return status;
}

uint32_t
lpfc_rq_destroy(struct lpfc_hba *phba, struct lpfc_queue *hrq,
		struct lpfc_queue *drq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	
	if (!hrq || !drq)
		return -ENODEV;
	mbox = mempool_alloc(hrq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_rq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_RQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_rq_destroy_q_id, &mbox->u.mqe.un.rq_destroy.u.request,
	       hrq->queue_id);
	mbox->vport = hrq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(hrq->phba, mbox, MBX_POLL);
	
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.rq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2509 RQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		if (rc != MBX_TIMEOUT)
			mempool_free(mbox, hrq->phba->mbox_mem_pool);
		return -ENXIO;
	}
	bf_set(lpfc_mbx_rq_destroy_q_id, &mbox->u.mqe.un.rq_destroy.u.request,
	       drq->queue_id);
	rc = lpfc_sli_issue_mbox(drq->phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.rq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2510 RQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	list_del_init(&hrq->list);
	list_del_init(&drq->list);
	mempool_free(mbox, hrq->phba->mbox_mem_pool);
	return status;
}

int
lpfc_sli4_post_sgl(struct lpfc_hba *phba,
		dma_addr_t pdma_phys_addr0,
		dma_addr_t pdma_phys_addr1,
		uint16_t xritag)
{
	struct lpfc_mbx_post_sgl_pages *post_sgl_pages;
	LPFC_MBOXQ_t *mbox;
	int rc;
	uint32_t shdr_status, shdr_add_status;
	uint32_t mbox_tmo;
	union lpfc_sli4_cfg_shdr *shdr;

	if (xritag == NO_XRI) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0364 Invalid param:\n");
		return -EINVAL;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES,
			sizeof(struct lpfc_mbx_post_sgl_pages) -
			sizeof(struct lpfc_sli4_cfg_mhdr), LPFC_SLI4_MBX_EMBED);

	post_sgl_pages = (struct lpfc_mbx_post_sgl_pages *)
				&mbox->u.mqe.un.post_sgl_pages;
	bf_set(lpfc_post_sgl_pages_xri, post_sgl_pages, xritag);
	bf_set(lpfc_post_sgl_pages_xricnt, post_sgl_pages, 1);

	post_sgl_pages->sgl_pg_pairs[0].sgl_pg0_addr_lo	=
				cpu_to_le32(putPaddrLow(pdma_phys_addr0));
	post_sgl_pages->sgl_pg_pairs[0].sgl_pg0_addr_hi =
				cpu_to_le32(putPaddrHigh(pdma_phys_addr0));

	post_sgl_pages->sgl_pg_pairs[0].sgl_pg1_addr_lo	=
				cpu_to_le32(putPaddrLow(pdma_phys_addr1));
	post_sgl_pages->sgl_pg_pairs[0].sgl_pg1_addr_hi =
				cpu_to_le32(putPaddrHigh(pdma_phys_addr1));
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	
	shdr = (union lpfc_sli4_cfg_shdr *) &post_sgl_pages->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		mempool_free(mbox, phba->mbox_mem_pool);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2511 POST_SGL mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}
	return 0;
}

uint16_t
lpfc_sli4_alloc_xri(struct lpfc_hba *phba)
{
	unsigned long xri;

	spin_lock_irq(&phba->hbalock);
	xri = find_next_zero_bit(phba->sli4_hba.xri_bmask,
				 phba->sli4_hba.max_cfg_param.max_xri, 0);
	if (xri >= phba->sli4_hba.max_cfg_param.max_xri) {
		spin_unlock_irq(&phba->hbalock);
		return NO_XRI;
	} else {
		set_bit(xri, phba->sli4_hba.xri_bmask);
		phba->sli4_hba.max_cfg_param.xri_used++;
		phba->sli4_hba.xri_count++;
	}

	spin_unlock_irq(&phba->hbalock);
	return xri;
}

void
__lpfc_sli4_free_xri(struct lpfc_hba *phba, int xri)
{
	if (test_and_clear_bit(xri, phba->sli4_hba.xri_bmask)) {
		phba->sli4_hba.xri_count--;
		phba->sli4_hba.max_cfg_param.xri_used--;
	}
}

void
lpfc_sli4_free_xri(struct lpfc_hba *phba, int xri)
{
	spin_lock_irq(&phba->hbalock);
	__lpfc_sli4_free_xri(phba, xri);
	spin_unlock_irq(&phba->hbalock);
}

uint16_t
lpfc_sli4_next_xritag(struct lpfc_hba *phba)
{
	uint16_t xri_index;

	xri_index = lpfc_sli4_alloc_xri(phba);
	if (xri_index != NO_XRI)
		return xri_index;

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"2004 Failed to allocate XRI.last XRITAG is %d"
			" Max XRI is %d, Used XRI is %d\n",
			xri_index,
			phba->sli4_hba.max_cfg_param.max_xri,
			phba->sli4_hba.max_cfg_param.xri_used);
	return NO_XRI;
}

int
lpfc_sli4_post_els_sgl_list(struct lpfc_hba *phba)
{
	struct lpfc_sglq *sglq_entry;
	struct lpfc_mbx_post_uembed_sgl_page1 *sgl;
	struct sgl_page_pairs *sgl_pg_pairs;
	void *viraddr;
	LPFC_MBOXQ_t *mbox;
	uint32_t reqlen, alloclen, pg_pairs;
	uint32_t mbox_tmo;
	uint16_t xritag_start = 0, lxri = 0;
	int els_xri_cnt, rc = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	
	els_xri_cnt = lpfc_sli4_get_els_iocb_cnt(phba);

	reqlen = els_xri_cnt * sizeof(struct sgl_page_pairs) +
		 sizeof(union lpfc_sli4_cfg_shdr) + sizeof(uint32_t);
	if (reqlen > SLI4_PAGE_SIZE) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2559 Block sgl registration required DMA "
				"size (%d) great than a page\n", reqlen);
		return -ENOMEM;
	}
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	
	alloclen = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES, reqlen,
			 LPFC_SLI4_MBX_NEMBED);

	if (alloclen < reqlen) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0285 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory "
				"size (%d)\n", alloclen, reqlen);
		lpfc_sli4_mbox_cmd_free(phba, mbox);
		return -ENOMEM;
	}
	
	viraddr = mbox->sge_array->addr[0];
	sgl = (struct lpfc_mbx_post_uembed_sgl_page1 *)viraddr;
	sgl_pg_pairs = &sgl->sgl_pg_pairs;

	for (pg_pairs = 0; pg_pairs < els_xri_cnt; pg_pairs++) {
		sglq_entry = phba->sli4_hba.lpfc_els_sgl_array[pg_pairs];

		if (bf_get(lpfc_xri_rsrc_rdy, &phba->sli4_hba.sli4_flags) !=
		    LPFC_XRI_RSRC_RDY) {
			lxri = lpfc_sli4_next_xritag(phba);
			if (lxri == NO_XRI) {
				lpfc_sli4_mbox_cmd_free(phba, mbox);
				return -ENOMEM;
			}
			sglq_entry->sli4_lxritag = lxri;
			sglq_entry->sli4_xritag = phba->sli4_hba.xri_ids[lxri];
		}

		
		sgl_pg_pairs->sgl_pg0_addr_lo =
				cpu_to_le32(putPaddrLow(sglq_entry->phys));
		sgl_pg_pairs->sgl_pg0_addr_hi =
				cpu_to_le32(putPaddrHigh(sglq_entry->phys));
		sgl_pg_pairs->sgl_pg1_addr_lo =
				cpu_to_le32(putPaddrLow(0));
		sgl_pg_pairs->sgl_pg1_addr_hi =
				cpu_to_le32(putPaddrHigh(0));

		
		if (pg_pairs == 0)
			xritag_start = sglq_entry->sli4_xritag;
		sgl_pg_pairs++;
	}

	
	bf_set(lpfc_post_sgl_pages_xri, sgl, xritag_start);
	bf_set(lpfc_post_sgl_pages_xricnt, sgl, els_xri_cnt);
	sgl->word0 = cpu_to_le32(sgl->word0);
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	shdr = (union lpfc_sli4_cfg_shdr *) &sgl->cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		lpfc_sli4_mbox_cmd_free(phba, mbox);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2513 POST_SGL_BLOCK mailbox command failed "
				"status x%x add_status x%x mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}

	if (rc == 0)
		bf_set(lpfc_xri_rsrc_rdy, &phba->sli4_hba.sli4_flags,
		       LPFC_XRI_RSRC_RDY);
	return rc;
}

int
lpfc_sli4_post_els_sgl_list_ext(struct lpfc_hba *phba)
{
	struct lpfc_sglq *sglq_entry;
	struct lpfc_mbx_post_uembed_sgl_page1 *sgl;
	struct sgl_page_pairs *sgl_pg_pairs;
	void *viraddr;
	LPFC_MBOXQ_t *mbox;
	uint32_t reqlen, alloclen, index;
	uint32_t mbox_tmo;
	uint16_t rsrc_start, rsrc_size, els_xri_cnt, post_els_xri_cnt;
	uint16_t xritag_start = 0, lxri = 0;
	struct lpfc_rsrc_blks *rsrc_blk;
	int cnt, ttl_cnt, rc = 0;
	int loop_cnt;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	
	els_xri_cnt = lpfc_sli4_get_els_iocb_cnt(phba);

	reqlen = els_xri_cnt * sizeof(struct sgl_page_pairs) +
		 sizeof(union lpfc_sli4_cfg_shdr) + sizeof(uint32_t);
	if (reqlen > SLI4_PAGE_SIZE) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2989 Block sgl registration required DMA "
				"size (%d) great than a page\n", reqlen);
		return -ENOMEM;
	}

	cnt = 0;
	ttl_cnt = 0;
	post_els_xri_cnt = els_xri_cnt;
	list_for_each_entry(rsrc_blk, &phba->sli4_hba.lpfc_xri_blk_list,
			    list) {
		rsrc_start = rsrc_blk->rsrc_start;
		rsrc_size = rsrc_blk->rsrc_size;

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3014 Working ELS Extent start %d, cnt %d\n",
				rsrc_start, rsrc_size);

		loop_cnt = min(post_els_xri_cnt, rsrc_size);
		if (loop_cnt < post_els_xri_cnt) {
			post_els_xri_cnt -= loop_cnt;
			ttl_cnt += loop_cnt;
		} else
			ttl_cnt += post_els_xri_cnt;

		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!mbox)
			return -ENOMEM;
		alloclen = lpfc_sli4_config(phba, mbox,
					LPFC_MBOX_SUBSYSTEM_FCOE,
					LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES,
					reqlen, LPFC_SLI4_MBX_NEMBED);
		if (alloclen < reqlen) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2987 Allocated DMA memory size (%d) "
					"is less than the requested DMA memory "
					"size (%d)\n", alloclen, reqlen);
			lpfc_sli4_mbox_cmd_free(phba, mbox);
			return -ENOMEM;
		}

		
		viraddr = mbox->sge_array->addr[0];
		sgl = (struct lpfc_mbx_post_uembed_sgl_page1 *)viraddr;
		sgl_pg_pairs = &sgl->sgl_pg_pairs;

		for (index = rsrc_start;
		     index < rsrc_start + loop_cnt;
		     index++) {
			sglq_entry = phba->sli4_hba.lpfc_els_sgl_array[cnt];

			if (bf_get(lpfc_xri_rsrc_rdy,
				   &phba->sli4_hba.sli4_flags) !=
				   LPFC_XRI_RSRC_RDY) {
				lxri = lpfc_sli4_next_xritag(phba);
				if (lxri == NO_XRI) {
					lpfc_sli4_mbox_cmd_free(phba, mbox);
					rc = -ENOMEM;
					goto err_exit;
				}
				sglq_entry->sli4_lxritag = lxri;
				sglq_entry->sli4_xritag =
						phba->sli4_hba.xri_ids[lxri];
			}

			
			sgl_pg_pairs->sgl_pg0_addr_lo =
				cpu_to_le32(putPaddrLow(sglq_entry->phys));
			sgl_pg_pairs->sgl_pg0_addr_hi =
				cpu_to_le32(putPaddrHigh(sglq_entry->phys));
			sgl_pg_pairs->sgl_pg1_addr_lo =
				cpu_to_le32(putPaddrLow(0));
			sgl_pg_pairs->sgl_pg1_addr_hi =
				cpu_to_le32(putPaddrHigh(0));

			
			if (index == rsrc_start)
				xritag_start = sglq_entry->sli4_xritag;
			sgl_pg_pairs++;
			cnt++;
		}

		
		rsrc_blk->rsrc_used += loop_cnt;
		bf_set(lpfc_post_sgl_pages_xri, sgl, xritag_start);
		bf_set(lpfc_post_sgl_pages_xricnt, sgl, loop_cnt);
		sgl->word0 = cpu_to_le32(sgl->word0);

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3015 Post ELS Extent SGL, start %d, "
				"cnt %d, used %d\n",
				xritag_start, loop_cnt, rsrc_blk->rsrc_used);
		if (!phba->sli4_hba.intr_enable)
			rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
		else {
			mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
			rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
		}
		shdr = (union lpfc_sli4_cfg_shdr *) &sgl->cfg_shdr;
		shdr_status = bf_get(lpfc_mbox_hdr_status,
				     &shdr->response);
		shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
					 &shdr->response);
		if (rc != MBX_TIMEOUT)
			lpfc_sli4_mbox_cmd_free(phba, mbox);
		if (shdr_status || shdr_add_status || rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2988 POST_SGL_BLOCK mailbox "
					"command failed status x%x "
					"add_status x%x mbx status x%x\n",
					shdr_status, shdr_add_status, rc);
			rc = -ENXIO;
			goto err_exit;
		}
		if (ttl_cnt >= els_xri_cnt)
			break;
	}

 err_exit:
	if (rc == 0)
		bf_set(lpfc_xri_rsrc_rdy, &phba->sli4_hba.sli4_flags,
		       LPFC_XRI_RSRC_RDY);
	return rc;
}

int
lpfc_sli4_post_scsi_sgl_block(struct lpfc_hba *phba, struct list_head *sblist,
			      int cnt)
{
	struct lpfc_scsi_buf *psb;
	struct lpfc_mbx_post_uembed_sgl_page1 *sgl;
	struct sgl_page_pairs *sgl_pg_pairs;
	void *viraddr;
	LPFC_MBOXQ_t *mbox;
	uint32_t reqlen, alloclen, pg_pairs;
	uint32_t mbox_tmo;
	uint16_t xritag_start = 0;
	int rc = 0;
	uint32_t shdr_status, shdr_add_status;
	dma_addr_t pdma_phys_bpl1;
	union lpfc_sli4_cfg_shdr *shdr;

	
	reqlen = cnt * sizeof(struct sgl_page_pairs) +
		 sizeof(union lpfc_sli4_cfg_shdr) + sizeof(uint32_t);
	if (reqlen > SLI4_PAGE_SIZE) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0217 Block sgl registration required DMA "
				"size (%d) great than a page\n", reqlen);
		return -ENOMEM;
	}
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0283 Failed to allocate mbox cmd memory\n");
		return -ENOMEM;
	}

	
	alloclen = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
				LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES, reqlen,
				LPFC_SLI4_MBX_NEMBED);

	if (alloclen < reqlen) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2561 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory "
				"size (%d)\n", alloclen, reqlen);
		lpfc_sli4_mbox_cmd_free(phba, mbox);
		return -ENOMEM;
	}

	
	viraddr = mbox->sge_array->addr[0];

	
	sgl = (struct lpfc_mbx_post_uembed_sgl_page1 *)viraddr;
	sgl_pg_pairs = &sgl->sgl_pg_pairs;

	pg_pairs = 0;
	list_for_each_entry(psb, sblist, list) {
		
		sgl_pg_pairs->sgl_pg0_addr_lo =
			cpu_to_le32(putPaddrLow(psb->dma_phys_bpl));
		sgl_pg_pairs->sgl_pg0_addr_hi =
			cpu_to_le32(putPaddrHigh(psb->dma_phys_bpl));
		if (phba->cfg_sg_dma_buf_size > SGL_PAGE_SIZE)
			pdma_phys_bpl1 = psb->dma_phys_bpl + SGL_PAGE_SIZE;
		else
			pdma_phys_bpl1 = 0;
		sgl_pg_pairs->sgl_pg1_addr_lo =
			cpu_to_le32(putPaddrLow(pdma_phys_bpl1));
		sgl_pg_pairs->sgl_pg1_addr_hi =
			cpu_to_le32(putPaddrHigh(pdma_phys_bpl1));
		
		if (pg_pairs == 0)
			xritag_start = psb->cur_iocbq.sli4_xritag;
		sgl_pg_pairs++;
		pg_pairs++;
	}
	bf_set(lpfc_post_sgl_pages_xri, sgl, xritag_start);
	bf_set(lpfc_post_sgl_pages_xricnt, sgl, pg_pairs);
	
	sgl->word0 = cpu_to_le32(sgl->word0);

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	shdr = (union lpfc_sli4_cfg_shdr *) &sgl->cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		lpfc_sli4_mbox_cmd_free(phba, mbox);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2564 POST_SGL_BLOCK mailbox command failed "
				"status x%x add_status x%x mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}
	return rc;
}

int
lpfc_sli4_post_scsi_sgl_blk_ext(struct lpfc_hba *phba, struct list_head *sblist,
				int cnt)
{
	struct lpfc_scsi_buf *psb = NULL;
	struct lpfc_mbx_post_uembed_sgl_page1 *sgl;
	struct sgl_page_pairs *sgl_pg_pairs;
	void *viraddr;
	LPFC_MBOXQ_t *mbox;
	uint32_t reqlen, alloclen, pg_pairs;
	uint32_t mbox_tmo;
	uint16_t xri_start = 0, scsi_xri_start;
	uint16_t rsrc_range;
	int rc = 0, avail_cnt;
	uint32_t shdr_status, shdr_add_status;
	dma_addr_t pdma_phys_bpl1;
	union lpfc_sli4_cfg_shdr *shdr;
	struct lpfc_rsrc_blks *rsrc_blk;
	uint32_t xri_cnt = 0;

	
	reqlen = cnt * sizeof(struct sgl_page_pairs) +
		 sizeof(union lpfc_sli4_cfg_shdr) + sizeof(uint32_t);
	if (reqlen > SLI4_PAGE_SIZE) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2932 Block sgl registration required DMA "
				"size (%d) great than a page\n", reqlen);
		return -ENOMEM;
	}

	psb = list_prepare_entry(psb, sblist, list);
	scsi_xri_start = phba->sli4_hba.scsi_xri_start;
	list_for_each_entry(rsrc_blk, &phba->sli4_hba.lpfc_xri_blk_list,
			    list) {
		rsrc_range = rsrc_blk->rsrc_start + rsrc_blk->rsrc_size;
		if (rsrc_range < scsi_xri_start)
			continue;
		else if (rsrc_blk->rsrc_used >= rsrc_blk->rsrc_size)
			continue;
		else
			avail_cnt = rsrc_blk->rsrc_size - rsrc_blk->rsrc_used;

		reqlen = (avail_cnt * sizeof(struct sgl_page_pairs)) +
			sizeof(union lpfc_sli4_cfg_shdr) + sizeof(uint32_t);
		mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (!mbox) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2933 Failed to allocate mbox cmd "
					"memory\n");
			return -ENOMEM;
		}
		alloclen = lpfc_sli4_config(phba, mbox,
					LPFC_MBOX_SUBSYSTEM_FCOE,
					LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES,
					reqlen,
					LPFC_SLI4_MBX_NEMBED);
		if (alloclen < reqlen) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2934 Allocated DMA memory size (%d) "
					"is less than the requested DMA memory "
					"size (%d)\n", alloclen, reqlen);
			lpfc_sli4_mbox_cmd_free(phba, mbox);
			return -ENOMEM;
		}

		
		viraddr = mbox->sge_array->addr[0];

		
		sgl = (struct lpfc_mbx_post_uembed_sgl_page1 *)viraddr;
		sgl_pg_pairs = &sgl->sgl_pg_pairs;

		
		pg_pairs = 0;
		list_for_each_entry_continue(psb, sblist, list) {
			
			sgl_pg_pairs->sgl_pg0_addr_lo =
				cpu_to_le32(putPaddrLow(psb->dma_phys_bpl));
			sgl_pg_pairs->sgl_pg0_addr_hi =
				cpu_to_le32(putPaddrHigh(psb->dma_phys_bpl));
			if (phba->cfg_sg_dma_buf_size > SGL_PAGE_SIZE)
				pdma_phys_bpl1 = psb->dma_phys_bpl +
					SGL_PAGE_SIZE;
			else
				pdma_phys_bpl1 = 0;
			sgl_pg_pairs->sgl_pg1_addr_lo =
				cpu_to_le32(putPaddrLow(pdma_phys_bpl1));
			sgl_pg_pairs->sgl_pg1_addr_hi =
				cpu_to_le32(putPaddrHigh(pdma_phys_bpl1));
			
			if (pg_pairs == 0)
				xri_start = psb->cur_iocbq.sli4_xritag;
			sgl_pg_pairs++;
			pg_pairs++;
			xri_cnt++;

			if ((xri_cnt == cnt) || (pg_pairs >= avail_cnt))
				break;
		}
		rsrc_blk->rsrc_used += pg_pairs;
		bf_set(lpfc_post_sgl_pages_xri, sgl, xri_start);
		bf_set(lpfc_post_sgl_pages_xricnt, sgl, pg_pairs);

		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3016 Post SCSI Extent SGL, start %d, cnt %d "
				"blk use %d\n",
				xri_start, pg_pairs, rsrc_blk->rsrc_used);
		
		sgl->word0 = cpu_to_le32(sgl->word0);
		if (!phba->sli4_hba.intr_enable)
			rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
		else {
			mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
			rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
		}
		shdr = (union lpfc_sli4_cfg_shdr *) &sgl->cfg_shdr;
		shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
		shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
					 &shdr->response);
		if (rc != MBX_TIMEOUT)
			lpfc_sli4_mbox_cmd_free(phba, mbox);
		if (shdr_status || shdr_add_status || rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2935 POST_SGL_BLOCK mailbox command "
					"failed status x%x add_status x%x "
					"mbx status x%x\n",
					shdr_status, shdr_add_status, rc);
			return -ENXIO;
		}

		
		if (xri_cnt >= cnt)
			break;
	}
	return rc;
}

static int
lpfc_fc_frame_check(struct lpfc_hba *phba, struct fc_frame_header *fc_hdr)
{
	
	static char *rctl_names[] = FC_RCTL_NAMES_INIT;
	char *type_names[] = FC_TYPE_NAMES_INIT;
	struct fc_vft_header *fc_vft_hdr;
	uint32_t *header = (uint32_t *) fc_hdr;

	switch (fc_hdr->fh_r_ctl) {
	case FC_RCTL_DD_UNCAT:		
	case FC_RCTL_DD_SOL_DATA:	
	case FC_RCTL_DD_UNSOL_CTL:	
	case FC_RCTL_DD_SOL_CTL:	
	case FC_RCTL_DD_UNSOL_DATA:	
	case FC_RCTL_DD_DATA_DESC:	
	case FC_RCTL_DD_UNSOL_CMD:	
	case FC_RCTL_DD_CMD_STATUS:	
	case FC_RCTL_ELS_REQ:	
	case FC_RCTL_ELS_REP:	
	case FC_RCTL_ELS4_REQ:	
	case FC_RCTL_ELS4_REP:	
	case FC_RCTL_BA_NOP:  	
	case FC_RCTL_BA_ABTS: 	
	case FC_RCTL_BA_RMC: 	
	case FC_RCTL_BA_ACC:	
	case FC_RCTL_BA_RJT:	
	case FC_RCTL_BA_PRMT:
	case FC_RCTL_ACK_1:	
	case FC_RCTL_ACK_0:	
	case FC_RCTL_P_RJT:	
	case FC_RCTL_F_RJT:	
	case FC_RCTL_P_BSY:	
	case FC_RCTL_F_BSY:	
	case FC_RCTL_F_BSYL:	
	case FC_RCTL_LCR:	
	case FC_RCTL_END:	
		break;
	case FC_RCTL_VFTH:	
		fc_vft_hdr = (struct fc_vft_header *)fc_hdr;
		fc_hdr = &((struct fc_frame_header *)fc_vft_hdr)[1];
		return lpfc_fc_frame_check(phba, fc_hdr);
	default:
		goto drop;
	}
	switch (fc_hdr->fh_type) {
	case FC_TYPE_BLS:
	case FC_TYPE_ELS:
	case FC_TYPE_FCP:
	case FC_TYPE_CT:
		break;
	case FC_TYPE_IP:
	case FC_TYPE_ILS:
	default:
		goto drop;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"2538 Received frame rctl:%s type:%s "
			"Frame Data:%08x %08x %08x %08x %08x %08x\n",
			rctl_names[fc_hdr->fh_r_ctl],
			type_names[fc_hdr->fh_type],
			be32_to_cpu(header[0]), be32_to_cpu(header[1]),
			be32_to_cpu(header[2]), be32_to_cpu(header[3]),
			be32_to_cpu(header[4]), be32_to_cpu(header[5]));
	return 0;
drop:
	lpfc_printf_log(phba, KERN_WARNING, LOG_ELS,
			"2539 Dropped frame rctl:%s type:%s\n",
			rctl_names[fc_hdr->fh_r_ctl],
			type_names[fc_hdr->fh_type]);
	return 1;
}

static uint32_t
lpfc_fc_hdr_get_vfi(struct fc_frame_header *fc_hdr)
{
	struct fc_vft_header *fc_vft_hdr = (struct fc_vft_header *)fc_hdr;

	if (fc_hdr->fh_r_ctl != FC_RCTL_VFTH)
		return 0;
	return bf_get(fc_vft_hdr_vf_id, fc_vft_hdr);
}

static struct lpfc_vport *
lpfc_fc_frame_to_vport(struct lpfc_hba *phba, struct fc_frame_header *fc_hdr,
		       uint16_t fcfi)
{
	struct lpfc_vport **vports;
	struct lpfc_vport *vport = NULL;
	int i;
	uint32_t did = (fc_hdr->fh_d_id[0] << 16 |
			fc_hdr->fh_d_id[1] << 8 |
			fc_hdr->fh_d_id[2]);
	if (did == Fabric_DID)
		return phba->pport;
	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
			if (phba->fcf.fcfi == fcfi &&
			    vports[i]->vfi == lpfc_fc_hdr_get_vfi(fc_hdr) &&
			    vports[i]->fc_myDID == did) {
				vport = vports[i];
				break;
			}
		}
	lpfc_destroy_vport_work_array(phba, vports);
	return vport;
}

void
lpfc_update_rcv_time_stamp(struct lpfc_vport *vport)
{
	struct lpfc_dmabuf *h_buf;
	struct hbq_dmabuf *dmabuf = NULL;

	
	h_buf = list_get_first(&vport->rcv_buffer_list,
			       struct lpfc_dmabuf, list);
	if (!h_buf)
		return;
	dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
	vport->rcv_buffer_time_stamp = dmabuf->time_stamp;
}

void
lpfc_cleanup_rcv_buffers(struct lpfc_vport *vport)
{
	struct lpfc_dmabuf *h_buf, *hnext;
	struct lpfc_dmabuf *d_buf, *dnext;
	struct hbq_dmabuf *dmabuf = NULL;

	
	list_for_each_entry_safe(h_buf, hnext, &vport->rcv_buffer_list, list) {
		dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		list_del_init(&dmabuf->hbuf.list);
		list_for_each_entry_safe(d_buf, dnext,
					 &dmabuf->dbuf.list, list) {
			list_del_init(&d_buf->list);
			lpfc_in_buf_free(vport->phba, d_buf);
		}
		lpfc_in_buf_free(vport->phba, &dmabuf->dbuf);
	}
}

void
lpfc_rcv_seq_check_edtov(struct lpfc_vport *vport)
{
	struct lpfc_dmabuf *h_buf, *hnext;
	struct lpfc_dmabuf *d_buf, *dnext;
	struct hbq_dmabuf *dmabuf = NULL;
	unsigned long timeout;
	int abort_count = 0;

	timeout = (msecs_to_jiffies(vport->phba->fc_edtov) +
		   vport->rcv_buffer_time_stamp);
	if (list_empty(&vport->rcv_buffer_list) ||
	    time_before(jiffies, timeout))
		return;
	
	list_for_each_entry_safe(h_buf, hnext, &vport->rcv_buffer_list, list) {
		dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		timeout = (msecs_to_jiffies(vport->phba->fc_edtov) +
			   dmabuf->time_stamp);
		if (time_before(jiffies, timeout))
			break;
		abort_count++;
		list_del_init(&dmabuf->hbuf.list);
		list_for_each_entry_safe(d_buf, dnext,
					 &dmabuf->dbuf.list, list) {
			list_del_init(&d_buf->list);
			lpfc_in_buf_free(vport->phba, d_buf);
		}
		lpfc_in_buf_free(vport->phba, &dmabuf->dbuf);
	}
	if (abort_count)
		lpfc_update_rcv_time_stamp(vport);
}

static struct hbq_dmabuf *
lpfc_fc_frame_add(struct lpfc_vport *vport, struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *new_hdr;
	struct fc_frame_header *temp_hdr;
	struct lpfc_dmabuf *d_buf;
	struct lpfc_dmabuf *h_buf;
	struct hbq_dmabuf *seq_dmabuf = NULL;
	struct hbq_dmabuf *temp_dmabuf = NULL;

	INIT_LIST_HEAD(&dmabuf->dbuf.list);
	dmabuf->time_stamp = jiffies;
	new_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	
	list_for_each_entry(h_buf, &vport->rcv_buffer_list, list) {
		temp_hdr = (struct fc_frame_header *)h_buf->virt;
		if ((temp_hdr->fh_seq_id != new_hdr->fh_seq_id) ||
		    (temp_hdr->fh_ox_id != new_hdr->fh_ox_id) ||
		    (memcmp(&temp_hdr->fh_s_id, &new_hdr->fh_s_id, 3)))
			continue;
		
		seq_dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		break;
	}
	if (!seq_dmabuf) {
		list_add_tail(&dmabuf->hbuf.list, &vport->rcv_buffer_list);
		lpfc_update_rcv_time_stamp(vport);
		return dmabuf;
	}
	temp_hdr = seq_dmabuf->hbuf.virt;
	if (be16_to_cpu(new_hdr->fh_seq_cnt) <
		be16_to_cpu(temp_hdr->fh_seq_cnt)) {
		list_del_init(&seq_dmabuf->hbuf.list);
		list_add_tail(&dmabuf->hbuf.list, &vport->rcv_buffer_list);
		list_add_tail(&dmabuf->dbuf.list, &seq_dmabuf->dbuf.list);
		lpfc_update_rcv_time_stamp(vport);
		return dmabuf;
	}
	
	list_move_tail(&seq_dmabuf->hbuf.list, &vport->rcv_buffer_list);
	seq_dmabuf->time_stamp = jiffies;
	lpfc_update_rcv_time_stamp(vport);
	if (list_empty(&seq_dmabuf->dbuf.list)) {
		temp_hdr = dmabuf->hbuf.virt;
		list_add_tail(&dmabuf->dbuf.list, &seq_dmabuf->dbuf.list);
		return seq_dmabuf;
	}
	
	list_for_each_entry_reverse(d_buf, &seq_dmabuf->dbuf.list, list) {
		temp_dmabuf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		temp_hdr = (struct fc_frame_header *)temp_dmabuf->hbuf.virt;
		if (be16_to_cpu(new_hdr->fh_seq_cnt) >
			be16_to_cpu(temp_hdr->fh_seq_cnt)) {
			list_add(&dmabuf->dbuf.list, &temp_dmabuf->dbuf.list);
			return seq_dmabuf;
		}
	}
	return NULL;
}

static bool
lpfc_sli4_abort_partial_seq(struct lpfc_vport *vport,
			    struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *new_hdr;
	struct fc_frame_header *temp_hdr;
	struct lpfc_dmabuf *d_buf, *n_buf, *h_buf;
	struct hbq_dmabuf *seq_dmabuf = NULL;

	
	INIT_LIST_HEAD(&dmabuf->dbuf.list);
	INIT_LIST_HEAD(&dmabuf->hbuf.list);
	new_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	list_for_each_entry(h_buf, &vport->rcv_buffer_list, list) {
		temp_hdr = (struct fc_frame_header *)h_buf->virt;
		if ((temp_hdr->fh_seq_id != new_hdr->fh_seq_id) ||
		    (temp_hdr->fh_ox_id != new_hdr->fh_ox_id) ||
		    (memcmp(&temp_hdr->fh_s_id, &new_hdr->fh_s_id, 3)))
			continue;
		
		seq_dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		break;
	}

	
	if (seq_dmabuf) {
		list_for_each_entry_safe(d_buf, n_buf,
					 &seq_dmabuf->dbuf.list, list) {
			list_del_init(&d_buf->list);
			lpfc_in_buf_free(vport->phba, d_buf);
		}
		return true;
	}
	return false;
}

static void
lpfc_sli4_seq_abort_rsp_cmpl(struct lpfc_hba *phba,
			     struct lpfc_iocbq *cmd_iocbq,
			     struct lpfc_iocbq *rsp_iocbq)
{
	if (cmd_iocbq)
		lpfc_sli_release_iocbq(phba, cmd_iocbq);

	
	if (rsp_iocbq && rsp_iocbq->iocb.ulpStatus)
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"3154 BLS ABORT RSP failed, data:  x%x/x%x\n",
			rsp_iocbq->iocb.ulpStatus,
			rsp_iocbq->iocb.un.ulpWord[4]);
}

uint16_t
lpfc_sli4_xri_inrange(struct lpfc_hba *phba,
		      uint16_t xri)
{
	int i;

	for (i = 0; i < phba->sli4_hba.max_cfg_param.max_xri; i++) {
		if (xri == phba->sli4_hba.xri_ids[i])
			return i;
	}
	return NO_XRI;
}


static void
lpfc_sli4_seq_abort_rsp(struct lpfc_hba *phba,
			struct fc_frame_header *fc_hdr)
{
	struct lpfc_iocbq *ctiocb = NULL;
	struct lpfc_nodelist *ndlp;
	uint16_t oxid, rxid;
	uint32_t sid, fctl;
	IOCB_t *icmd;
	int rc;

	if (!lpfc_is_link_up(phba))
		return;

	sid = sli4_sid_from_fc_hdr(fc_hdr);
	oxid = be16_to_cpu(fc_hdr->fh_ox_id);
	rxid = be16_to_cpu(fc_hdr->fh_rx_id);

	ndlp = lpfc_findnode_did(phba->pport, sid);
	if (!ndlp) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_ELS,
				"1268 Find ndlp returned NULL for oxid:x%x "
				"SID:x%x\n", oxid, sid);
		return;
	}
	if (lpfc_sli4_xri_inrange(phba, rxid))
		lpfc_set_rrq_active(phba, ndlp, rxid, oxid, 0);

	
	ctiocb = lpfc_sli_get_iocbq(phba);
	if (!ctiocb)
		return;

	
	fctl = sli4_fctl_from_fc_hdr(fc_hdr);

	icmd = &ctiocb->iocb;
	icmd->un.xseq64.bdl.bdeSize = 0;
	icmd->un.xseq64.bdl.ulpIoTag32 = 0;
	icmd->un.xseq64.w5.hcsw.Dfctl = 0;
	icmd->un.xseq64.w5.hcsw.Rctl = FC_RCTL_BA_ACC;
	icmd->un.xseq64.w5.hcsw.Type = FC_TYPE_BLS;

	
	icmd->ulpCommand = CMD_XMIT_BLS_RSP64_CX;
	icmd->ulpBdeCount = 0;
	icmd->ulpLe = 1;
	icmd->ulpClass = CLASS3;
	icmd->ulpContext = phba->sli4_hba.rpi_ids[ndlp->nlp_rpi];
	ctiocb->context1 = ndlp;

	ctiocb->iocb_cmpl = NULL;
	ctiocb->vport = phba->pport;
	ctiocb->iocb_cmpl = lpfc_sli4_seq_abort_rsp_cmpl;
	ctiocb->sli4_lxritag = NO_XRI;
	ctiocb->sli4_xritag = NO_XRI;

	if (oxid > (phba->sli4_hba.max_cfg_param.max_xri +
		    phba->sli4_hba.max_cfg_param.xri_base) ||
	    oxid > (lpfc_sli4_get_els_iocb_cnt(phba) +
		    phba->sli4_hba.max_cfg_param.xri_base)) {
		icmd->un.xseq64.w5.hcsw.Rctl = FC_RCTL_BA_RJT;
		bf_set(lpfc_vndr_code, &icmd->un.bls_rsp, 0);
		bf_set(lpfc_rsn_expln, &icmd->un.bls_rsp, FC_BA_RJT_INV_XID);
		bf_set(lpfc_rsn_code, &icmd->un.bls_rsp, FC_BA_RJT_UNABLE);
	}

	if (fctl & FC_FC_EX_CTX) {
		bf_set(lpfc_abts_orig, &icmd->un.bls_rsp, LPFC_ABTS_UNSOL_RSP);
	} else {
		bf_set(lpfc_abts_orig, &icmd->un.bls_rsp, LPFC_ABTS_UNSOL_INT);
	}
	bf_set(lpfc_abts_rxid, &icmd->un.bls_rsp, rxid);
	bf_set(lpfc_abts_oxid, &icmd->un.bls_rsp, oxid);

	
	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"1200 Send BLS cmd x%x on oxid x%x Data: x%x\n",
			icmd->un.xseq64.w5.hcsw.Rctl, oxid, phba->link_state);

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, ctiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_printf_log(phba, KERN_ERR, LOG_ELS,
				"2925 Failed to issue CT ABTS RSP x%x on "
				"xri x%x, Data x%x\n",
				icmd->un.xseq64.w5.hcsw.Rctl, oxid,
				phba->link_state);
		lpfc_sli_release_iocbq(phba, ctiocb);
	}
}

void
lpfc_sli4_handle_unsol_abort(struct lpfc_vport *vport,
			     struct hbq_dmabuf *dmabuf)
{
	struct lpfc_hba *phba = vport->phba;
	struct fc_frame_header fc_hdr;
	uint32_t fctl;
	bool abts_par;

	
	memcpy(&fc_hdr, dmabuf->hbuf.virt, sizeof(struct fc_frame_header));
	fctl = sli4_fctl_from_fc_hdr(&fc_hdr);

	if (fctl & FC_FC_EX_CTX) {
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
	} else {
		
		abts_par = lpfc_sli4_abort_partial_seq(vport, dmabuf);

		
		if (abts_par == false)
			lpfc_sli4_send_seq_to_ulp(vport, dmabuf);
		else
			lpfc_in_buf_free(phba, &dmabuf->dbuf);
	}
	
	lpfc_sli4_seq_abort_rsp(phba, &fc_hdr);
}

static int
lpfc_seq_complete(struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *hdr;
	struct lpfc_dmabuf *d_buf;
	struct hbq_dmabuf *seq_dmabuf;
	uint32_t fctl;
	int seq_count = 0;

	hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	
	if (hdr->fh_seq_cnt != seq_count)
		return 0;
	fctl = (hdr->fh_f_ctl[0] << 16 |
		hdr->fh_f_ctl[1] << 8 |
		hdr->fh_f_ctl[2]);
	
	if (fctl & FC_FC_END_SEQ)
		return 1;
	list_for_each_entry(d_buf, &dmabuf->dbuf.list, list) {
		seq_dmabuf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		hdr = (struct fc_frame_header *)seq_dmabuf->hbuf.virt;
		
		if (++seq_count != be16_to_cpu(hdr->fh_seq_cnt))
			return 0;
		fctl = (hdr->fh_f_ctl[0] << 16 |
			hdr->fh_f_ctl[1] << 8 |
			hdr->fh_f_ctl[2]);
		
		if (fctl & FC_FC_END_SEQ)
			return 1;
	}
	return 0;
}

static struct lpfc_iocbq *
lpfc_prep_seq(struct lpfc_vport *vport, struct hbq_dmabuf *seq_dmabuf)
{
	struct hbq_dmabuf *hbq_buf;
	struct lpfc_dmabuf *d_buf, *n_buf;
	struct lpfc_iocbq *first_iocbq, *iocbq;
	struct fc_frame_header *fc_hdr;
	uint32_t sid;
	uint32_t len, tot_len;
	struct ulp_bde64 *pbde;

	fc_hdr = (struct fc_frame_header *)seq_dmabuf->hbuf.virt;
	
	list_del_init(&seq_dmabuf->hbuf.list);
	lpfc_update_rcv_time_stamp(vport);
	
	sid = sli4_sid_from_fc_hdr(fc_hdr);
	tot_len = 0;
	
	first_iocbq = lpfc_sli_get_iocbq(vport->phba);
	if (first_iocbq) {
		
		first_iocbq->iocb.unsli3.rcvsli3.acc_len = 0;
		first_iocbq->iocb.ulpStatus = IOSTAT_SUCCESS;
		first_iocbq->iocb.ulpCommand = CMD_IOCB_RCV_SEQ64_CX;
		first_iocbq->iocb.ulpContext = NO_XRI;
		first_iocbq->iocb.unsli3.rcvsli3.ox_id =
			be16_to_cpu(fc_hdr->fh_ox_id);
		
		first_iocbq->iocb.unsli3.rcvsli3.vpi =
			vport->phba->vpi_ids[vport->vpi];
		
		first_iocbq->context2 = &seq_dmabuf->dbuf;
		first_iocbq->context3 = NULL;
		first_iocbq->iocb.ulpBdeCount = 1;
		first_iocbq->iocb.un.cont64[0].tus.f.bdeSize =
							LPFC_DATA_BUF_SIZE;
		first_iocbq->iocb.un.rcvels.remoteID = sid;
		tot_len = bf_get(lpfc_rcqe_length,
				       &seq_dmabuf->cq_event.cqe.rcqe_cmpl);
		first_iocbq->iocb.unsli3.rcvsli3.acc_len = tot_len;
	}
	iocbq = first_iocbq;
	list_for_each_entry_safe(d_buf, n_buf, &seq_dmabuf->dbuf.list, list) {
		if (!iocbq) {
			lpfc_in_buf_free(vport->phba, d_buf);
			continue;
		}
		if (!iocbq->context3) {
			iocbq->context3 = d_buf;
			iocbq->iocb.ulpBdeCount++;
			pbde = (struct ulp_bde64 *)
					&iocbq->iocb.unsli3.sli3Words[4];
			pbde->tus.f.bdeSize = LPFC_DATA_BUF_SIZE;

			
			hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
			len = bf_get(lpfc_rcqe_length,
				       &hbq_buf->cq_event.cqe.rcqe_cmpl);
			iocbq->iocb.unsli3.rcvsli3.acc_len += len;
			tot_len += len;
		} else {
			iocbq = lpfc_sli_get_iocbq(vport->phba);
			if (!iocbq) {
				if (first_iocbq) {
					first_iocbq->iocb.ulpStatus =
							IOSTAT_FCP_RSP_ERROR;
					first_iocbq->iocb.un.ulpWord[4] =
							IOERR_NO_RESOURCES;
				}
				lpfc_in_buf_free(vport->phba, d_buf);
				continue;
			}
			iocbq->context2 = d_buf;
			iocbq->context3 = NULL;
			iocbq->iocb.ulpBdeCount = 1;
			iocbq->iocb.un.cont64[0].tus.f.bdeSize =
							LPFC_DATA_BUF_SIZE;

			
			hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
			len = bf_get(lpfc_rcqe_length,
				       &hbq_buf->cq_event.cqe.rcqe_cmpl);
			tot_len += len;
			iocbq->iocb.unsli3.rcvsli3.acc_len = tot_len;

			iocbq->iocb.un.rcvels.remoteID = sid;
			list_add_tail(&iocbq->list, &first_iocbq->list);
		}
	}
	return first_iocbq;
}

static void
lpfc_sli4_send_seq_to_ulp(struct lpfc_vport *vport,
			  struct hbq_dmabuf *seq_dmabuf)
{
	struct fc_frame_header *fc_hdr;
	struct lpfc_iocbq *iocbq, *curr_iocb, *next_iocb;
	struct lpfc_hba *phba = vport->phba;

	fc_hdr = (struct fc_frame_header *)seq_dmabuf->hbuf.virt;
	iocbq = lpfc_prep_seq(vport, seq_dmabuf);
	if (!iocbq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2707 Ring %d handler: Failed to allocate "
				"iocb Rctl x%x Type x%x received\n",
				LPFC_ELS_RING,
				fc_hdr->fh_r_ctl, fc_hdr->fh_type);
		return;
	}
	if (!lpfc_complete_unsol_iocb(phba,
				      &phba->sli.ring[LPFC_ELS_RING],
				      iocbq, fc_hdr->fh_r_ctl,
				      fc_hdr->fh_type))
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2540 Ring %d handler: unexpected Rctl "
				"x%x Type x%x received\n",
				LPFC_ELS_RING,
				fc_hdr->fh_r_ctl, fc_hdr->fh_type);

	
	list_for_each_entry_safe(curr_iocb, next_iocb,
		&iocbq->list, list) {
		list_del_init(&curr_iocb->list);
		lpfc_sli_release_iocbq(phba, curr_iocb);
	}
	lpfc_sli_release_iocbq(phba, iocbq);
}

void
lpfc_sli4_handle_received_buffer(struct lpfc_hba *phba,
				 struct hbq_dmabuf *dmabuf)
{
	struct hbq_dmabuf *seq_dmabuf;
	struct fc_frame_header *fc_hdr;
	struct lpfc_vport *vport;
	uint32_t fcfi;

	
	fc_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	
	if (lpfc_fc_frame_check(phba, fc_hdr)) {
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}
	if ((bf_get(lpfc_cqe_code,
		    &dmabuf->cq_event.cqe.rcqe_cmpl) == CQE_CODE_RECEIVE_V1))
		fcfi = bf_get(lpfc_rcqe_fcf_id_v1,
			      &dmabuf->cq_event.cqe.rcqe_cmpl);
	else
		fcfi = bf_get(lpfc_rcqe_fcf_id,
			      &dmabuf->cq_event.cqe.rcqe_cmpl);
	vport = lpfc_fc_frame_to_vport(phba, fc_hdr, fcfi);
	if (!vport || !(vport->vpi_state & LPFC_VPI_REGISTERED)) {
		
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}
	
	if (fc_hdr->fh_r_ctl == FC_RCTL_BA_ABTS) {
		lpfc_sli4_handle_unsol_abort(vport, dmabuf);
		return;
	}

	
	seq_dmabuf = lpfc_fc_frame_add(vport, dmabuf);
	if (!seq_dmabuf) {
		
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}
	
	if (!lpfc_seq_complete(seq_dmabuf))
		return;

	
	lpfc_sli4_send_seq_to_ulp(vport, seq_dmabuf);
}

int
lpfc_sli4_post_all_rpi_hdrs(struct lpfc_hba *phba)
{
	struct lpfc_rpi_hdr *rpi_page;
	uint32_t rc = 0;
	uint16_t lrpi = 0;

	
	if (!phba->sli4_hba.rpi_hdrs_in_use)
		goto exit;
	if (phba->sli4_hba.extents_in_use)
		return -EIO;

	list_for_each_entry(rpi_page, &phba->sli4_hba.lpfc_rpi_hdr_list, list) {
		if (bf_get(lpfc_rpi_rsrc_rdy, &phba->sli4_hba.sli4_flags) !=
		    LPFC_RPI_RSRC_RDY)
			rpi_page->start_rpi = phba->sli4_hba.rpi_ids[lrpi];

		rc = lpfc_sli4_post_rpi_hdr(phba, rpi_page);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2008 Error %d posting all rpi "
					"headers\n", rc);
			rc = -EIO;
			break;
		}
	}

 exit:
	bf_set(lpfc_rpi_rsrc_rdy, &phba->sli4_hba.sli4_flags,
	       LPFC_RPI_RSRC_RDY);
	return rc;
}

int
lpfc_sli4_post_rpi_hdr(struct lpfc_hba *phba, struct lpfc_rpi_hdr *rpi_page)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mbx_post_hdr_tmpl *hdr_tmpl;
	uint32_t rc = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	
	if (!phba->sli4_hba.rpi_hdrs_in_use)
		return rc;
	if (phba->sli4_hba.extents_in_use)
		return -EIO;

	
	mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2001 Unable to allocate memory for issuing "
				"SLI_CONFIG_SPECIAL mailbox command\n");
		return -ENOMEM;
	}

	
	hdr_tmpl = &mboxq->u.mqe.un.hdr_tmpl;
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_POST_HDR_TEMPLATE,
			 sizeof(struct lpfc_mbx_post_hdr_tmpl) -
			 sizeof(struct lpfc_sli4_cfg_mhdr),
			 LPFC_SLI4_MBX_EMBED);


	
	bf_set(lpfc_mbx_post_hdr_tmpl_rpi_offset, hdr_tmpl,
	       rpi_page->start_rpi);
	bf_set(lpfc_mbx_post_hdr_tmpl_page_cnt,
	       hdr_tmpl, rpi_page->page_count);

	hdr_tmpl->rpi_paddr_lo = putPaddrLow(rpi_page->dmabuf->phys);
	hdr_tmpl->rpi_paddr_hi = putPaddrHigh(rpi_page->dmabuf->phys);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *) &hdr_tmpl->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		mempool_free(mboxq, phba->mbox_mem_pool);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2514 POST_RPI_HDR mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}
	return rc;
}

int
lpfc_sli4_alloc_rpi(struct lpfc_hba *phba)
{
	unsigned long rpi;
	uint16_t max_rpi, rpi_limit;
	uint16_t rpi_remaining, lrpi = 0;
	struct lpfc_rpi_hdr *rpi_hdr;

	max_rpi = phba->sli4_hba.max_cfg_param.max_rpi;
	rpi_limit = phba->sli4_hba.next_rpi;

	spin_lock_irq(&phba->hbalock);
	rpi = find_next_zero_bit(phba->sli4_hba.rpi_bmask, rpi_limit, 0);
	if (rpi >= rpi_limit)
		rpi = LPFC_RPI_ALLOC_ERROR;
	else {
		set_bit(rpi, phba->sli4_hba.rpi_bmask);
		phba->sli4_hba.max_cfg_param.rpi_used++;
		phba->sli4_hba.rpi_count++;
	}

	if ((rpi == LPFC_RPI_ALLOC_ERROR) &&
	    (phba->sli4_hba.rpi_count >= max_rpi)) {
		spin_unlock_irq(&phba->hbalock);
		return rpi;
	}

	if (!phba->sli4_hba.rpi_hdrs_in_use) {
		spin_unlock_irq(&phba->hbalock);
		return rpi;
	}

	rpi_remaining = phba->sli4_hba.next_rpi - phba->sli4_hba.rpi_count;
	spin_unlock_irq(&phba->hbalock);
	if (rpi_remaining < LPFC_RPI_LOW_WATER_MARK) {
		rpi_hdr = lpfc_sli4_create_rpi_hdr(phba);
		if (!rpi_hdr) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2002 Error Could not grow rpi "
					"count\n");
		} else {
			lrpi = rpi_hdr->start_rpi;
			rpi_hdr->start_rpi = phba->sli4_hba.rpi_ids[lrpi];
			lpfc_sli4_post_rpi_hdr(phba, rpi_hdr);
		}
	}

	return rpi;
}

void
__lpfc_sli4_free_rpi(struct lpfc_hba *phba, int rpi)
{
	if (test_and_clear_bit(rpi, phba->sli4_hba.rpi_bmask)) {
		phba->sli4_hba.rpi_count--;
		phba->sli4_hba.max_cfg_param.rpi_used--;
	}
}

void
lpfc_sli4_free_rpi(struct lpfc_hba *phba, int rpi)
{
	spin_lock_irq(&phba->hbalock);
	__lpfc_sli4_free_rpi(phba, rpi);
	spin_unlock_irq(&phba->hbalock);
}

void
lpfc_sli4_remove_rpis(struct lpfc_hba *phba)
{
	kfree(phba->sli4_hba.rpi_bmask);
	kfree(phba->sli4_hba.rpi_ids);
	bf_set(lpfc_rpi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
}

int
lpfc_sli4_resume_rpi(struct lpfc_nodelist *ndlp,
	void (*cmpl)(struct lpfc_hba *, LPFC_MBOXQ_t *), void *arg)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_hba *phba = ndlp->phba;
	int rc;

	
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	
	lpfc_resume_rpi(mboxq, ndlp);
	if (cmpl) {
		mboxq->mbox_cmpl = cmpl;
		mboxq->context1 = arg;
		mboxq->context2 = ndlp;
	} else
		mboxq->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mboxq->vport = ndlp->vport;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2010 Resume RPI Mailbox failed "
				"status %d, mbxStatus x%x\n", rc,
				bf_get(lpfc_mqe_status, &mboxq->u.mqe));
		mempool_free(mboxq, phba->mbox_mem_pool);
		return -EIO;
	}
	return 0;
}

int
lpfc_sli4_init_vpi(struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *mboxq;
	int rc = 0;
	int retval = MBX_SUCCESS;
	uint32_t mbox_tmo;
	struct lpfc_hba *phba = vport->phba;
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;
	lpfc_init_vpi(phba, mboxq, vport->vpi);
	mbox_tmo = lpfc_mbox_tmo_val(phba, mboxq);
	rc = lpfc_sli_issue_mbox_wait(phba, mboxq, mbox_tmo);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_SLI,
				"2022 INIT VPI Mailbox failed "
				"status %d, mbxStatus x%x\n", rc,
				bf_get(lpfc_mqe_status, &mboxq->u.mqe));
		retval = -EIO;
	}
	if (rc != MBX_TIMEOUT)
		mempool_free(mboxq, vport->phba->mbox_mem_pool);

	return retval;
}

static void
lpfc_mbx_cmpl_add_fcf_record(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	void *virt_addr;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t shdr_status, shdr_add_status;

	virt_addr = mboxq->sge_array->addr[0];
	
	shdr = (union lpfc_sli4_cfg_shdr *) virt_addr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);

	if ((shdr_status || shdr_add_status) &&
		(shdr_status != STATUS_FCF_IN_USE))
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2558 ADD_FCF_RECORD mailbox failed with "
			"status x%x add_status x%x\n",
			shdr_status, shdr_add_status);

	lpfc_sli4_mbox_cmd_free(phba, mboxq);
}

int
lpfc_sli4_add_fcf_record(struct lpfc_hba *phba, struct fcf_record *fcf_record)
{
	int rc = 0;
	LPFC_MBOXQ_t *mboxq;
	uint8_t *bytep;
	void *virt_addr;
	dma_addr_t phys_addr;
	struct lpfc_mbx_sge sge;
	uint32_t alloc_len, req_len;
	uint32_t fcfindex;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2009 Failed to allocate mbox for ADD_FCF cmd\n");
		return -ENOMEM;
	}

	req_len = sizeof(struct fcf_record) + sizeof(union lpfc_sli4_cfg_shdr) +
		  sizeof(uint32_t);

	
	alloc_len = lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_FCOE,
				     LPFC_MBOX_OPCODE_FCOE_ADD_FCF,
				     req_len, LPFC_SLI4_MBX_NEMBED);
	if (alloc_len < req_len) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2523 Allocated DMA memory size (x%x) is "
			"less than the requested DMA memory "
			"size (x%x)\n", alloc_len, req_len);
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
		return -ENOMEM;
	}

	lpfc_sli4_mbx_sge_get(mboxq, 0, &sge);
	phys_addr = getPaddr(sge.pa_hi, sge.pa_lo);
	virt_addr = mboxq->sge_array->addr[0];
	fcfindex = bf_get(lpfc_fcf_record_fcf_index, fcf_record);
	bytep = virt_addr + sizeof(union lpfc_sli4_cfg_shdr);
	lpfc_sli_pcimem_bcopy(&fcfindex, bytep, sizeof(uint32_t));

	bytep += sizeof(uint32_t);
	lpfc_sli_pcimem_bcopy(fcf_record, bytep, sizeof(struct fcf_record));
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_add_fcf_record;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2515 ADD_FCF_RECORD mailbox failed with "
			"status 0x%x\n", rc);
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
		rc = -EIO;
	} else
		rc = 0;

	return rc;
}

void
lpfc_sli4_build_dflt_fcf_record(struct lpfc_hba *phba,
				struct fcf_record *fcf_record,
				uint16_t fcf_index)
{
	memset(fcf_record, 0, sizeof(struct fcf_record));
	fcf_record->max_rcv_size = LPFC_FCOE_MAX_RCV_SIZE;
	fcf_record->fka_adv_period = LPFC_FCOE_FKA_ADV_PER;
	fcf_record->fip_priority = LPFC_FCOE_FIP_PRIORITY;
	bf_set(lpfc_fcf_record_mac_0, fcf_record, phba->fc_map[0]);
	bf_set(lpfc_fcf_record_mac_1, fcf_record, phba->fc_map[1]);
	bf_set(lpfc_fcf_record_mac_2, fcf_record, phba->fc_map[2]);
	bf_set(lpfc_fcf_record_mac_3, fcf_record, LPFC_FCOE_FCF_MAC3);
	bf_set(lpfc_fcf_record_mac_4, fcf_record, LPFC_FCOE_FCF_MAC4);
	bf_set(lpfc_fcf_record_mac_5, fcf_record, LPFC_FCOE_FCF_MAC5);
	bf_set(lpfc_fcf_record_fc_map_0, fcf_record, phba->fc_map[0]);
	bf_set(lpfc_fcf_record_fc_map_1, fcf_record, phba->fc_map[1]);
	bf_set(lpfc_fcf_record_fc_map_2, fcf_record, phba->fc_map[2]);
	bf_set(lpfc_fcf_record_fcf_valid, fcf_record, 1);
	bf_set(lpfc_fcf_record_fcf_avail, fcf_record, 1);
	bf_set(lpfc_fcf_record_fcf_index, fcf_record, fcf_index);
	bf_set(lpfc_fcf_record_mac_addr_prov, fcf_record,
		LPFC_FCF_FPMA | LPFC_FCF_SPMA);
	
	if (phba->valid_vlan) {
		fcf_record->vlan_bitmap[phba->vlan_id / 8]
			= 1 << (phba->vlan_id % 8);
	}
}

int
lpfc_sli4_fcf_scan_read_fcf_rec(struct lpfc_hba *phba, uint16_t fcf_index)
{
	int rc = 0, error;
	LPFC_MBOXQ_t *mboxq;

	phba->fcoe_eventtag_at_fcf_scan = phba->fcoe_eventtag;
	phba->fcoe_cvl_eventtag_attn = phba->fcoe_cvl_eventtag;
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2000 Failed to allocate mbox for "
				"READ_FCF cmd\n");
		error = -ENOMEM;
		goto fail_fcf_scan;
	}
	
	rc = lpfc_sli4_mbx_read_fcf_rec(phba, mboxq, fcf_index);
	if (rc) {
		error = -EINVAL;
		goto fail_fcf_scan;
	}
	
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_fcf_scan_read_fcf_rec;

	spin_lock_irq(&phba->hbalock);
	phba->hba_flag |= FCF_TS_INPROG;
	spin_unlock_irq(&phba->hbalock);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		error = -EIO;
	else {
		
		if (fcf_index == LPFC_FCOE_FCF_GET_FIRST)
			phba->fcf.eligible_fcf_cnt = 0;
		error = 0;
	}
fail_fcf_scan:
	if (error) {
		if (mboxq)
			lpfc_sli4_mbox_cmd_free(phba, mboxq);
		
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~FCF_TS_INPROG;
		spin_unlock_irq(&phba->hbalock);
	}
	return error;
}

int
lpfc_sli4_fcf_rr_read_fcf_rec(struct lpfc_hba *phba, uint16_t fcf_index)
{
	int rc = 0, error;
	LPFC_MBOXQ_t *mboxq;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_INIT,
				"2763 Failed to allocate mbox for "
				"READ_FCF cmd\n");
		error = -ENOMEM;
		goto fail_fcf_read;
	}
	
	rc = lpfc_sli4_mbx_read_fcf_rec(phba, mboxq, fcf_index);
	if (rc) {
		error = -EINVAL;
		goto fail_fcf_read;
	}
	
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_fcf_rr_read_fcf_rec;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		error = -EIO;
	else
		error = 0;

fail_fcf_read:
	if (error && mboxq)
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
	return error;
}

int
lpfc_sli4_read_fcf_rec(struct lpfc_hba *phba, uint16_t fcf_index)
{
	int rc = 0, error;
	LPFC_MBOXQ_t *mboxq;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_INIT,
				"2758 Failed to allocate mbox for "
				"READ_FCF cmd\n");
				error = -ENOMEM;
				goto fail_fcf_read;
	}
	
	rc = lpfc_sli4_mbx_read_fcf_rec(phba, mboxq, fcf_index);
	if (rc) {
		error = -EINVAL;
		goto fail_fcf_read;
	}
	
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_read_fcf_rec;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		error = -EIO;
	else
		error = 0;

fail_fcf_read:
	if (error && mboxq)
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
	return error;
}

int
lpfc_check_next_fcf_pri_level(struct lpfc_hba *phba)
{
	uint16_t next_fcf_pri;
	uint16_t last_index;
	struct lpfc_fcf_pri *fcf_pri;
	int rc;
	int ret = 0;

	last_index = find_first_bit(phba->fcf.fcf_rr_bmask,
			LPFC_SLI4_FCF_TBL_INDX_MAX);
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"3060 Last IDX %d\n", last_index);
	if (list_empty(&phba->fcf.fcf_pri_list)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
			"3061 Last IDX %d\n", last_index);
		return 0; 
	}
	next_fcf_pri = 0;
	memset(phba->fcf.fcf_rr_bmask, 0,
			sizeof(*phba->fcf.fcf_rr_bmask));
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(fcf_pri, &phba->fcf.fcf_pri_list, list) {
		if (fcf_pri->fcf_rec.flag & LPFC_FCF_FLOGI_FAILED)
			continue;
		if (!next_fcf_pri)
			next_fcf_pri = fcf_pri->fcf_rec.priority;
		spin_unlock_irq(&phba->hbalock);
		if (fcf_pri->fcf_rec.priority == next_fcf_pri) {
			rc = lpfc_sli4_fcf_rr_index_set(phba,
						fcf_pri->fcf_rec.fcf_index);
			if (rc)
				return 0;
		}
		spin_lock_irq(&phba->hbalock);
	}
	if (!next_fcf_pri && !list_empty(&phba->fcf.fcf_pri_list)) {
		list_for_each_entry(fcf_pri, &phba->fcf.fcf_pri_list, list) {
			fcf_pri->fcf_rec.flag &= ~LPFC_FCF_FLOGI_FAILED;
			if (!next_fcf_pri)
				next_fcf_pri = fcf_pri->fcf_rec.priority;
			spin_unlock_irq(&phba->hbalock);
			if (fcf_pri->fcf_rec.priority == next_fcf_pri) {
				rc = lpfc_sli4_fcf_rr_index_set(phba,
						fcf_pri->fcf_rec.fcf_index);
				if (rc)
					return 0;
			}
			spin_lock_irq(&phba->hbalock);
		}
	} else
		ret = 1;
	spin_unlock_irq(&phba->hbalock);

	return ret;
}
uint16_t
lpfc_sli4_fcf_rr_next_index_get(struct lpfc_hba *phba)
{
	uint16_t next_fcf_index;

	
next_priority:
	next_fcf_index = (phba->fcf.current_rec.fcf_indx + 1) %
					LPFC_SLI4_FCF_TBL_INDX_MAX;
	next_fcf_index = find_next_bit(phba->fcf.fcf_rr_bmask,
				       LPFC_SLI4_FCF_TBL_INDX_MAX,
				       next_fcf_index);

	
	if (next_fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		next_fcf_index = find_next_bit(phba->fcf.fcf_rr_bmask,
					       LPFC_SLI4_FCF_TBL_INDX_MAX, 0);
	}


	
	if (next_fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX ||
		next_fcf_index == phba->fcf.current_rec.fcf_indx) {
		if (lpfc_check_next_fcf_pri_level(phba))
			goto next_priority;
		lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
				"2844 No roundrobin failover FCF available\n");
		if (next_fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX)
			return LPFC_FCOE_FCF_NEXT_NONE;
		else {
			lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
				"3063 Only FCF available idx %d, flag %x\n",
				next_fcf_index,
			phba->fcf.fcf_pri[next_fcf_index].fcf_rec.flag);
			return next_fcf_index;
		}
	}

	if (next_fcf_index < LPFC_SLI4_FCF_TBL_INDX_MAX &&
		phba->fcf.fcf_pri[next_fcf_index].fcf_rec.flag &
		LPFC_FCF_FLOGI_FAILED)
		goto next_priority;

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2845 Get next roundrobin failover FCF (x%x)\n",
			next_fcf_index);

	return next_fcf_index;
}

int
lpfc_sli4_fcf_rr_index_set(struct lpfc_hba *phba, uint16_t fcf_index)
{
	if (fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
				"2610 FCF (x%x) reached driver's book "
				"keeping dimension:x%x\n",
				fcf_index, LPFC_SLI4_FCF_TBL_INDX_MAX);
		return -EINVAL;
	}
	
	set_bit(fcf_index, phba->fcf.fcf_rr_bmask);

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2790 Set FCF (x%x) to roundrobin FCF failover "
			"bmask\n", fcf_index);

	return 0;
}

void
lpfc_sli4_fcf_rr_index_clear(struct lpfc_hba *phba, uint16_t fcf_index)
{
	struct lpfc_fcf_pri *fcf_pri;
	if (fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
				"2762 FCF (x%x) reached driver's book "
				"keeping dimension:x%x\n",
				fcf_index, LPFC_SLI4_FCF_TBL_INDX_MAX);
		return;
	}
	
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(fcf_pri, &phba->fcf.fcf_pri_list, list) {
		if (fcf_pri->fcf_rec.fcf_index == fcf_index) {
			list_del_init(&fcf_pri->list);
			break;
		}
	}
	spin_unlock_irq(&phba->hbalock);
	clear_bit(fcf_index, phba->fcf.fcf_rr_bmask);

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2791 Clear FCF (x%x) from roundrobin failover "
			"bmask\n", fcf_index);
}

void
lpfc_mbx_cmpl_redisc_fcf_table(struct lpfc_hba *phba, LPFC_MBOXQ_t *mbox)
{
	struct lpfc_mbx_redisc_fcf_tbl *redisc_fcf;
	uint32_t shdr_status, shdr_add_status;

	redisc_fcf = &mbox->u.mqe.un.redisc_fcf_tbl;

	shdr_status = bf_get(lpfc_mbox_hdr_status,
			     &redisc_fcf->header.cfg_shdr.response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
			     &redisc_fcf->header.cfg_shdr.response);
	if (shdr_status || shdr_add_status) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
				"2746 Requesting for FCF rediscovery failed "
				"status x%x add_status x%x\n",
				shdr_status, shdr_add_status);
		if (phba->fcf.fcf_flag & FCF_ACVL_DISC) {
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_ACVL_DISC;
			spin_unlock_irq(&phba->hbalock);
			lpfc_retry_pport_discovery(phba);
		} else {
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DEAD_DISC;
			spin_unlock_irq(&phba->hbalock);
			lpfc_sli4_fcf_dead_failthrough(phba);
		}
	} else {
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
				"2775 Start FCF rediscover quiescent timer\n");
		lpfc_fcf_redisc_wait_start_timer(phba);
	}

	mempool_free(mbox, phba->mbox_mem_pool);
}

int
lpfc_sli4_redisc_fcf_table(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mbox;
	struct lpfc_mbx_redisc_fcf_tbl *redisc_fcf;
	int rc, length;

	
	lpfc_cancel_all_vport_retry_delay_timer(phba);

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2745 Failed to allocate mbox for "
				"requesting FCF rediscover.\n");
		return -ENOMEM;
	}

	length = (sizeof(struct lpfc_mbx_redisc_fcf_tbl) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_REDISCOVER_FCF,
			 length, LPFC_SLI4_MBX_EMBED);

	redisc_fcf = &mbox->u.mqe.un.redisc_fcf_tbl;
	
	bf_set(lpfc_mbx_redisc_fcf_count, redisc_fcf, 0);

	
	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_mbx_cmpl_redisc_fcf_table;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);

	if (rc == MBX_NOT_FINISHED) {
		mempool_free(mbox, phba->mbox_mem_pool);
		return -EIO;
	}
	return 0;
}

void
lpfc_sli4_fcf_dead_failthrough(struct lpfc_hba *phba)
{
	uint32_t link_state;

	link_state = phba->link_state;
	lpfc_linkdown(phba);
	phba->link_state = link_state;

	
	lpfc_unregister_unused_fcf(phba);
}

static uint32_t
lpfc_sli_get_config_region23(struct lpfc_hba *phba, char *rgn23_data)
{
	LPFC_MBOXQ_t *pmb = NULL;
	MAILBOX_t *mb;
	uint32_t offset = 0;
	int rc;

	if (!rgn23_data)
		return 0;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2600 failed to allocate mailbox memory\n");
		return 0;
	}
	mb = &pmb->u.mb;

	do {
		lpfc_dump_mem(phba, pmb, offset, DMP_REGION_23);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2601 failed to read config "
					"region 23, rc 0x%x Status 0x%x\n",
					rc, mb->mbxStatus);
			mb->un.varDmp.word_cnt = 0;
		}
		if (mb->un.varDmp.word_cnt == 0)
			break;
		if (mb->un.varDmp.word_cnt > DMP_RGN23_SIZE - offset)
			mb->un.varDmp.word_cnt = DMP_RGN23_SIZE - offset;

		lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				       rgn23_data + offset,
				       mb->un.varDmp.word_cnt);
		offset += mb->un.varDmp.word_cnt;
	} while (mb->un.varDmp.word_cnt && offset < DMP_RGN23_SIZE);

	mempool_free(pmb, phba->mbox_mem_pool);
	return offset;
}

static uint32_t
lpfc_sli4_get_config_region23(struct lpfc_hba *phba, char *rgn23_data)
{
	LPFC_MBOXQ_t *mboxq = NULL;
	struct lpfc_dmabuf *mp = NULL;
	struct lpfc_mqe *mqe;
	uint32_t data_length = 0;
	int rc;

	if (!rgn23_data)
		return 0;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3105 failed to allocate mailbox memory\n");
		return 0;
	}

	if (lpfc_sli4_dump_cfg_rg23(phba, mboxq))
		goto out;
	mqe = &mboxq->u.mqe;
	mp = (struct lpfc_dmabuf *) mboxq->context1;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (rc)
		goto out;
	data_length = mqe->un.mb_words[5];
	if (data_length == 0)
		goto out;
	if (data_length > DMP_RGN23_SIZE) {
		data_length = 0;
		goto out;
	}
	lpfc_sli_pcimem_bcopy((char *)mp->virt, rgn23_data, data_length);
out:
	mempool_free(mboxq, phba->mbox_mem_pool);
	if (mp) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
	}
	return data_length;
}

void
lpfc_sli_read_link_ste(struct lpfc_hba *phba)
{
	uint8_t *rgn23_data = NULL;
	uint32_t if_type, data_size, sub_tlv_len, tlv_offset;
	uint32_t offset = 0;

	
	rgn23_data = kzalloc(DMP_RGN23_SIZE, GFP_KERNEL);
	if (!rgn23_data)
		goto out;

	if (phba->sli_rev < LPFC_SLI_REV4)
		data_size = lpfc_sli_get_config_region23(phba, rgn23_data);
	else {
		if_type = bf_get(lpfc_sli_intf_if_type,
				 &phba->sli4_hba.sli_intf);
		if (if_type == LPFC_SLI_INTF_IF_TYPE_0)
			goto out;
		data_size = lpfc_sli4_get_config_region23(phba, rgn23_data);
	}

	if (!data_size)
		goto out;

	
	if (memcmp(&rgn23_data[offset], LPFC_REGION23_SIGNATURE, 4)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2619 Config region 23 has bad signature\n");
			goto out;
	}
	offset += 4;

	
	if (rgn23_data[offset] != LPFC_REGION23_VERSION) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2620 Config region 23 has bad version\n");
		goto out;
	}
	offset += 4;

	
	while (offset < data_size) {
		if (rgn23_data[offset] == LPFC_REGION23_LAST_REC)
			break;
		if ((rgn23_data[offset] != DRIVER_SPECIFIC_TYPE) ||
		    (rgn23_data[offset + 2] != LINUX_DRIVER_ID) ||
		    (rgn23_data[offset + 3] != 0)) {
			offset += rgn23_data[offset + 1] * 4 + 4;
			continue;
		}

		
		sub_tlv_len = rgn23_data[offset + 1] * 4;
		offset += 4;
		tlv_offset = 0;

		while ((offset < data_size) &&
			(tlv_offset < sub_tlv_len)) {
			if (rgn23_data[offset] == LPFC_REGION23_LAST_REC) {
				offset += 4;
				tlv_offset += 4;
				break;
			}
			if (rgn23_data[offset] != PORT_STE_TYPE) {
				offset += rgn23_data[offset + 1] * 4 + 4;
				tlv_offset += rgn23_data[offset + 1] * 4 + 4;
				continue;
			}

			
			if (!rgn23_data[offset + 2])
				phba->hba_flag |= LINK_DISABLED;

			goto out;
		}
	}

out:
	kfree(rgn23_data);
	return;
}

int
lpfc_wr_object(struct lpfc_hba *phba, struct list_head *dmabuf_list,
	       uint32_t size, uint32_t *offset)
{
	struct lpfc_mbx_wr_object *wr_object;
	LPFC_MBOXQ_t *mbox;
	int rc = 0, i = 0;
	uint32_t shdr_status, shdr_add_status;
	uint32_t mbox_tmo;
	union lpfc_sli4_cfg_shdr *shdr;
	struct lpfc_dmabuf *dmabuf;
	uint32_t written = 0;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			LPFC_MBOX_OPCODE_WRITE_OBJECT,
			sizeof(struct lpfc_mbx_wr_object) -
			sizeof(struct lpfc_sli4_cfg_mhdr), LPFC_SLI4_MBX_EMBED);

	wr_object = (struct lpfc_mbx_wr_object *)&mbox->u.mqe.un.wr_object;
	wr_object->u.request.write_offset = *offset;
	sprintf((uint8_t *)wr_object->u.request.object_name, "/");
	wr_object->u.request.object_name[0] =
		cpu_to_le32(wr_object->u.request.object_name[0]);
	bf_set(lpfc_wr_object_eof, &wr_object->u.request, 0);
	list_for_each_entry(dmabuf, dmabuf_list, list) {
		if (i >= LPFC_MBX_WR_CONFIG_MAX_BDE || written >= size)
			break;
		wr_object->u.request.bde[i].addrLow = putPaddrLow(dmabuf->phys);
		wr_object->u.request.bde[i].addrHigh =
			putPaddrHigh(dmabuf->phys);
		if (written + SLI4_PAGE_SIZE >= size) {
			wr_object->u.request.bde[i].tus.f.bdeSize =
				(size - written);
			written += (size - written);
			bf_set(lpfc_wr_object_eof, &wr_object->u.request, 1);
		} else {
			wr_object->u.request.bde[i].tus.f.bdeSize =
				SLI4_PAGE_SIZE;
			written += SLI4_PAGE_SIZE;
		}
		i++;
	}
	wr_object->u.request.bde_count = i;
	bf_set(lpfc_wr_object_write_length, &wr_object->u.request, written);
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	
	shdr = (union lpfc_sli4_cfg_shdr *) &wr_object->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		mempool_free(mbox, phba->mbox_mem_pool);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3025 Write Object mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	} else
		*offset += wr_object->u.response.actual_write_length;
	return rc;
}

void
lpfc_cleanup_pending_mbox(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	LPFC_MBOXQ_t *mb, *nextmb;
	struct lpfc_dmabuf *mp;
	struct lpfc_nodelist *ndlp;
	struct lpfc_nodelist *act_mbx_ndlp = NULL;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	LIST_HEAD(mbox_cmd_list);
	uint8_t restart_loop;

	
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mb, nextmb, &phba->sli.mboxq, list) {
		if (mb->vport != vport)
			continue;

		if ((mb->u.mb.mbxCommand != MBX_REG_LOGIN64) &&
			(mb->u.mb.mbxCommand != MBX_REG_VPI))
			continue;

		list_del(&mb->list);
		list_add_tail(&mb->list, &mbox_cmd_list);
	}
	
	mb = phba->sli.mbox_active;
	if (mb && (mb->vport == vport)) {
		if ((mb->u.mb.mbxCommand == MBX_REG_LOGIN64) ||
			(mb->u.mb.mbxCommand == MBX_REG_VPI))
			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		if (mb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
			act_mbx_ndlp = (struct lpfc_nodelist *)mb->context2;
			
			act_mbx_ndlp = lpfc_nlp_get(act_mbx_ndlp);
			
			mb->mbox_flag |= LPFC_MBX_IMED_UNREG;
		}
	}
	
	do {
		restart_loop = 0;
		list_for_each_entry(mb, &phba->sli.mboxq_cmpl, list) {
			if ((mb->vport != vport) ||
				(mb->mbox_flag & LPFC_MBX_IMED_UNREG))
				continue;

			if ((mb->u.mb.mbxCommand != MBX_REG_LOGIN64) &&
				(mb->u.mb.mbxCommand != MBX_REG_VPI))
				continue;

			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
			if (mb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
				ndlp = (struct lpfc_nodelist *)mb->context2;
				
				mb->mbox_flag |= LPFC_MBX_IMED_UNREG;
				restart_loop = 1;
				spin_unlock_irq(&phba->hbalock);
				spin_lock(shost->host_lock);
				ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
				spin_unlock(shost->host_lock);
				spin_lock_irq(&phba->hbalock);
				break;
			}
		}
	} while (restart_loop);

	spin_unlock_irq(&phba->hbalock);

	
	while (!list_empty(&mbox_cmd_list)) {
		list_remove_head(&mbox_cmd_list, mb, LPFC_MBOXQ_t, list);
		if (mb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
			mp = (struct lpfc_dmabuf *) (mb->context1);
			if (mp) {
				__lpfc_mbuf_free(phba, mp->virt, mp->phys);
				kfree(mp);
			}
			ndlp = (struct lpfc_nodelist *) mb->context2;
			mb->context2 = NULL;
			if (ndlp) {
				spin_lock(shost->host_lock);
				ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
				spin_unlock(shost->host_lock);
				lpfc_nlp_put(ndlp);
			}
		}
		mempool_free(mb, phba->mbox_mem_pool);
	}

	
	if (act_mbx_ndlp) {
		spin_lock(shost->host_lock);
		act_mbx_ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
		spin_unlock(shost->host_lock);
		lpfc_nlp_put(act_mbx_ndlp);
	}
}


uint32_t
lpfc_drain_txq(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	struct lpfc_iocbq *piocbq = 0;
	unsigned long iflags = 0;
	char *fail_msg = NULL;
	struct lpfc_sglq *sglq;
	union lpfc_wqe wqe;

	spin_lock_irqsave(&phba->hbalock, iflags);
	if (pring->txq_cnt > pring->txq_max)
		pring->txq_max = pring->txq_cnt;

	spin_unlock_irqrestore(&phba->hbalock, iflags);

	while (pring->txq_cnt) {
		spin_lock_irqsave(&phba->hbalock, iflags);

		piocbq = lpfc_sli_ringtx_get(phba, pring);
		sglq = __lpfc_sli_get_sglq(phba, piocbq);
		if (!sglq) {
			__lpfc_sli_ringtx_put(phba, pring, piocbq);
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			break;
		} else {
			if (!piocbq) {
				sglq = __lpfc_clear_active_sglq(phba,
						 sglq->sli4_lxritag);
				spin_unlock_irqrestore(&phba->hbalock, iflags);
				lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2823 txq empty and txq_cnt is %d\n ",
					pring->txq_cnt);
				break;
			}
		}

		piocbq->sli4_lxritag = sglq->sli4_lxritag;
		piocbq->sli4_xritag = sglq->sli4_xritag;
		if (NO_XRI == lpfc_sli4_bpl2sgl(phba, piocbq, sglq))
			fail_msg = "to convert bpl to sgl";
		else if (lpfc_sli4_iocb2wqe(phba, piocbq, &wqe))
			fail_msg = "to convert iocb to wqe";
		else if (lpfc_sli4_wq_put(phba->sli4_hba.els_wq, &wqe))
			fail_msg = " - Wq is full";
		else
			lpfc_sli_ringtxcmpl_put(phba, pring, piocbq);

		if (fail_msg) {
			
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2822 IOCB failed %s iotag 0x%x "
					"xri 0x%x\n",
					fail_msg,
					piocbq->iotag, piocbq->sli4_xritag);
			list_add_tail(&piocbq->list, &completions);
		}
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}

	
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
				IOERR_SLI_ABORTED);

	return pring->txq_cnt;
}
