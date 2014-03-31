/* bnx2fc_io.c: Broadcom NetXtreme II Linux FCoE offload driver.
 * IO manager and SCSI IO processing.
 *
 * Copyright (c) 2008 - 2011 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Bhanu Prakash Gollapudi (bprakash@broadcom.com)
 */

#include "bnx2fc.h"

#define RESERVE_FREE_LIST_INDEX num_possible_cpus()

static int bnx2fc_split_bd(struct bnx2fc_cmd *io_req, u64 addr, int sg_len,
			   int bd_index);
static int bnx2fc_map_sg(struct bnx2fc_cmd *io_req);
static int bnx2fc_build_bd_list_from_sg(struct bnx2fc_cmd *io_req);
static void bnx2fc_unmap_sg_list(struct bnx2fc_cmd *io_req);
static void bnx2fc_free_mp_resc(struct bnx2fc_cmd *io_req);
static void bnx2fc_parse_fcp_rsp(struct bnx2fc_cmd *io_req,
				 struct fcoe_fcp_rsp_payload *fcp_rsp,
				 u8 num_rq);

void bnx2fc_cmd_timer_set(struct bnx2fc_cmd *io_req,
			  unsigned int timer_msec)
{
	struct bnx2fc_interface *interface = io_req->port->priv;

	if (queue_delayed_work(interface->timer_work_queue,
			       &io_req->timeout_work,
			       msecs_to_jiffies(timer_msec)))
		kref_get(&io_req->refcount);
}

static void bnx2fc_cmd_timeout(struct work_struct *work)
{
	struct bnx2fc_cmd *io_req = container_of(work, struct bnx2fc_cmd,
						 timeout_work.work);
	struct fc_lport *lport;
	struct fc_rport_priv *rdata;
	u8 cmd_type = io_req->cmd_type;
	struct bnx2fc_rport *tgt = io_req->tgt;
	int logo_issued;
	int rc;

	BNX2FC_IO_DBG(io_req, "cmd_timeout, cmd_type = %d,"
		      "req_flags = %lx\n", cmd_type, io_req->req_flags);

	spin_lock_bh(&tgt->tgt_lock);
	if (test_and_clear_bit(BNX2FC_FLAG_ISSUE_RRQ, &io_req->req_flags)) {
		clear_bit(BNX2FC_FLAG_RETIRE_OXID, &io_req->req_flags);
		spin_unlock_bh(&tgt->tgt_lock);
		bnx2fc_send_rrq(io_req);
		return;
	}
	if (test_and_clear_bit(BNX2FC_FLAG_RETIRE_OXID, &io_req->req_flags)) {
		BNX2FC_IO_DBG(io_req, "IO ready for reuse now\n");
		goto done;
	}

	switch (cmd_type) {
	case BNX2FC_SCSI_CMD:
		if (test_and_clear_bit(BNX2FC_FLAG_EH_ABORT,
							&io_req->req_flags)) {
			
			BNX2FC_IO_DBG(io_req, "eh_abort timed out\n");
			complete(&io_req->tm_done);
		} else if (test_bit(BNX2FC_FLAG_ISSUE_ABTS,
				    &io_req->req_flags)) {
			
			BNX2FC_IO_DBG(io_req, "ABTS timed out refcnt = %d\n",
					io_req->refcount.refcount.counter);
			if (!(test_and_set_bit(BNX2FC_FLAG_ABTS_DONE,
					       &io_req->req_flags))) {

				lport = io_req->port->lport;
				rdata = io_req->tgt->rdata;
				logo_issued = test_and_set_bit(
						BNX2FC_FLAG_EXPL_LOGO,
						&tgt->flags);
				kref_put(&io_req->refcount, bnx2fc_cmd_release);
				spin_unlock_bh(&tgt->tgt_lock);

				
				if (!logo_issued) {
					BNX2FC_IO_DBG(io_req, "Explicit "
						   "logo - tgt flags = 0x%lx\n",
						   tgt->flags);

					mutex_lock(&lport->disc.disc_mutex);
					lport->tt.rport_logoff(rdata);
					mutex_unlock(&lport->disc.disc_mutex);
				}
				return;
			}
		} else {
			
			BNX2FC_IO_DBG(io_req, "IO timed out. issue ABTS\n");
			if (test_and_set_bit(BNX2FC_FLAG_IO_COMPL,
					     &io_req->req_flags)) {
				BNX2FC_IO_DBG(io_req, "IO completed before "
							   " timer expiry\n");
				goto done;
			}

			if (!test_and_set_bit(BNX2FC_FLAG_ISSUE_ABTS,
					      &io_req->req_flags)) {
				rc = bnx2fc_initiate_abts(io_req);
				if (rc == SUCCESS)
					goto done;
				lport = io_req->port->lport;
				rdata = io_req->tgt->rdata;
				logo_issued = test_and_set_bit(
						BNX2FC_FLAG_EXPL_LOGO,
						&tgt->flags);
				kref_put(&io_req->refcount, bnx2fc_cmd_release);
				spin_unlock_bh(&tgt->tgt_lock);

				if (!logo_issued) {
					BNX2FC_IO_DBG(io_req, "Explicit "
						   "logo - tgt flags = 0x%lx\n",
						   tgt->flags);


					mutex_lock(&lport->disc.disc_mutex);
					lport->tt.rport_logoff(rdata);
					mutex_unlock(&lport->disc.disc_mutex);
				}
				return;
			} else {
				BNX2FC_IO_DBG(io_req, "IO already in "
						      "ABTS processing\n");
			}
		}
		break;
	case BNX2FC_ELS:

		if (test_bit(BNX2FC_FLAG_ISSUE_ABTS, &io_req->req_flags)) {
			BNX2FC_IO_DBG(io_req, "ABTS for ELS timed out\n");

			if (!test_and_set_bit(BNX2FC_FLAG_ABTS_DONE,
					      &io_req->req_flags)) {
				lport = io_req->port->lport;
				rdata = io_req->tgt->rdata;
				logo_issued = test_and_set_bit(
						BNX2FC_FLAG_EXPL_LOGO,
						&tgt->flags);
				kref_put(&io_req->refcount, bnx2fc_cmd_release);
				spin_unlock_bh(&tgt->tgt_lock);

				
				if (!logo_issued) {
					BNX2FC_IO_DBG(io_req, "Explicitly logo"
						   "(els)\n");
					mutex_lock(&lport->disc.disc_mutex);
					lport->tt.rport_logoff(rdata);
					mutex_unlock(&lport->disc.disc_mutex);
				}
				return;
			}
		} else {
			BNX2FC_IO_DBG(io_req, "ELS timed out\n");
			if (test_and_set_bit(BNX2FC_FLAG_ELS_DONE,
					       &io_req->req_flags))
				goto done;

			
			set_bit(BNX2FC_FLAG_ELS_TIMEOUT, &io_req->req_flags);

			if ((io_req->cb_func) && (io_req->cb_arg)) {
				io_req->cb_func(io_req->cb_arg);
				io_req->cb_arg = NULL;
			}
		}
		break;
	default:
		printk(KERN_ERR PFX "cmd_timeout: invalid cmd_type %d\n",
			cmd_type);
		break;
	}

done:
	
	kref_put(&io_req->refcount, bnx2fc_cmd_release);
	spin_unlock_bh(&tgt->tgt_lock);
}

static void bnx2fc_scsi_done(struct bnx2fc_cmd *io_req, int err_code)
{
	
	struct scsi_cmnd *sc_cmd = io_req->sc_cmd;

	if (io_req->cmd_type != BNX2FC_SCSI_CMD)
		return;

	BNX2FC_IO_DBG(io_req, "scsi_done. err_code = 0x%x\n", err_code);
	if (test_bit(BNX2FC_FLAG_CMD_LOST, &io_req->req_flags)) {
		
		return;
	}

	bnx2fc_unmap_sg_list(io_req);
	io_req->sc_cmd = NULL;
	if (!sc_cmd) {
		printk(KERN_ERR PFX "scsi_done - sc_cmd NULL. "
				    "IO(0x%x) already cleaned up\n",
		       io_req->xid);
		return;
	}
	sc_cmd->result = err_code << 16;

	BNX2FC_IO_DBG(io_req, "sc=%p, result=0x%x, retries=%d, allowed=%d\n",
		sc_cmd, host_byte(sc_cmd->result), sc_cmd->retries,
		sc_cmd->allowed);
	scsi_set_resid(sc_cmd, scsi_bufflen(sc_cmd));
	sc_cmd->SCp.ptr = NULL;
	sc_cmd->scsi_done(sc_cmd);
}

struct bnx2fc_cmd_mgr *bnx2fc_cmd_mgr_alloc(struct bnx2fc_hba *hba,
						u16 min_xid, u16 max_xid)
{
	struct bnx2fc_cmd_mgr *cmgr;
	struct io_bdt *bdt_info;
	struct bnx2fc_cmd *io_req;
	size_t len;
	u32 mem_size;
	u16 xid;
	int i;
	int num_ios, num_pri_ios;
	size_t bd_tbl_sz;
	int arr_sz = num_possible_cpus() + 1;

	if (max_xid <= min_xid || max_xid == FC_XID_UNKNOWN) {
		printk(KERN_ERR PFX "cmd_mgr_alloc: Invalid min_xid 0x%x \
					and max_xid 0x%x\n", min_xid, max_xid);
		return NULL;
	}
	BNX2FC_MISC_DBG("min xid 0x%x, max xid 0x%x\n", min_xid, max_xid);

	num_ios = max_xid - min_xid + 1;
	len = (num_ios * (sizeof(struct bnx2fc_cmd *)));
	len += sizeof(struct bnx2fc_cmd_mgr);

	cmgr = kzalloc(len, GFP_KERNEL);
	if (!cmgr) {
		printk(KERN_ERR PFX "failed to alloc cmgr\n");
		return NULL;
	}

	cmgr->free_list = kzalloc(sizeof(*cmgr->free_list) *
				  arr_sz, GFP_KERNEL);
	if (!cmgr->free_list) {
		printk(KERN_ERR PFX "failed to alloc free_list\n");
		goto mem_err;
	}

	cmgr->free_list_lock = kzalloc(sizeof(*cmgr->free_list_lock) *
				       arr_sz, GFP_KERNEL);
	if (!cmgr->free_list_lock) {
		printk(KERN_ERR PFX "failed to alloc free_list_lock\n");
		goto mem_err;
	}

	cmgr->hba = hba;
	cmgr->cmds = (struct bnx2fc_cmd **)(cmgr + 1);

	for (i = 0; i < arr_sz; i++)  {
		INIT_LIST_HEAD(&cmgr->free_list[i]);
		spin_lock_init(&cmgr->free_list_lock[i]);
	}

	xid = BNX2FC_MIN_XID;
	num_pri_ios = num_ios - BNX2FC_ELSTM_XIDS;
	for (i = 0; i < num_ios; i++) {
		io_req = kzalloc(sizeof(*io_req), GFP_KERNEL);

		if (!io_req) {
			printk(KERN_ERR PFX "failed to alloc io_req\n");
			goto mem_err;
		}

		INIT_LIST_HEAD(&io_req->link);
		INIT_DELAYED_WORK(&io_req->timeout_work, bnx2fc_cmd_timeout);

		io_req->xid = xid++;
		if (i < num_pri_ios)
			list_add_tail(&io_req->link,
				&cmgr->free_list[io_req->xid %
						 num_possible_cpus()]);
		else
			list_add_tail(&io_req->link,
				&cmgr->free_list[num_possible_cpus()]);
		io_req++;
	}

	
	mem_size = num_ios * sizeof(struct io_bdt *);
	cmgr->io_bdt_pool = kmalloc(mem_size, GFP_KERNEL);
	if (!cmgr->io_bdt_pool) {
		printk(KERN_ERR PFX "failed to alloc io_bdt_pool\n");
		goto mem_err;
	}

	mem_size = sizeof(struct io_bdt);
	for (i = 0; i < num_ios; i++) {
		cmgr->io_bdt_pool[i] = kmalloc(mem_size, GFP_KERNEL);
		if (!cmgr->io_bdt_pool[i]) {
			printk(KERN_ERR PFX "failed to alloc "
				"io_bdt_pool[%d]\n", i);
			goto mem_err;
		}
	}

	
	bd_tbl_sz = BNX2FC_MAX_BDS_PER_CMD * sizeof(struct fcoe_bd_ctx);
	for (i = 0; i < num_ios; i++) {
		bdt_info = cmgr->io_bdt_pool[i];
		bdt_info->bd_tbl = dma_alloc_coherent(&hba->pcidev->dev,
						      bd_tbl_sz,
						      &bdt_info->bd_tbl_dma,
						      GFP_KERNEL);
		if (!bdt_info->bd_tbl) {
			printk(KERN_ERR PFX "failed to alloc "
				"bdt_tbl[%d]\n", i);
			goto mem_err;
		}
	}

	return cmgr;

mem_err:
	bnx2fc_cmd_mgr_free(cmgr);
	return NULL;
}

void bnx2fc_cmd_mgr_free(struct bnx2fc_cmd_mgr *cmgr)
{
	struct io_bdt *bdt_info;
	struct bnx2fc_hba *hba = cmgr->hba;
	size_t bd_tbl_sz;
	u16 min_xid = BNX2FC_MIN_XID;
	u16 max_xid = BNX2FC_MAX_XID;
	int num_ios;
	int i;

	num_ios = max_xid - min_xid + 1;

	
	if (!cmgr->io_bdt_pool)
		goto free_cmd_pool;

	bd_tbl_sz = BNX2FC_MAX_BDS_PER_CMD * sizeof(struct fcoe_bd_ctx);
	for (i = 0; i < num_ios; i++) {
		bdt_info = cmgr->io_bdt_pool[i];
		if (bdt_info->bd_tbl) {
			dma_free_coherent(&hba->pcidev->dev, bd_tbl_sz,
					    bdt_info->bd_tbl,
					    bdt_info->bd_tbl_dma);
			bdt_info->bd_tbl = NULL;
		}
	}

	
	for (i = 0; i < num_ios; i++) {
		kfree(cmgr->io_bdt_pool[i]);
		cmgr->io_bdt_pool[i] = NULL;
	}

	kfree(cmgr->io_bdt_pool);
	cmgr->io_bdt_pool = NULL;

free_cmd_pool:
	kfree(cmgr->free_list_lock);

	
	if (!cmgr->free_list)
		goto free_cmgr;

	for (i = 0; i < num_possible_cpus() + 1; i++)  {
		struct list_head *list;
		struct list_head *tmp;

		list_for_each_safe(list, tmp, &cmgr->free_list[i]) {
			struct bnx2fc_cmd *io_req = (struct bnx2fc_cmd *)list;
			list_del(&io_req->link);
			kfree(io_req);
		}
	}
	kfree(cmgr->free_list);
free_cmgr:
	
	kfree(cmgr);
}

struct bnx2fc_cmd *bnx2fc_elstm_alloc(struct bnx2fc_rport *tgt, int type)
{
	struct fcoe_port *port = tgt->port;
	struct bnx2fc_interface *interface = port->priv;
	struct bnx2fc_cmd_mgr *cmd_mgr = interface->hba->cmd_mgr;
	struct bnx2fc_cmd *io_req;
	struct list_head *listp;
	struct io_bdt *bd_tbl;
	int index = RESERVE_FREE_LIST_INDEX;
	u32 free_sqes;
	u32 max_sqes;
	u16 xid;

	max_sqes = tgt->max_sqes;
	switch (type) {
	case BNX2FC_TASK_MGMT_CMD:
		max_sqes = BNX2FC_TM_MAX_SQES;
		break;
	case BNX2FC_ELS:
		max_sqes = BNX2FC_ELS_MAX_SQES;
		break;
	default:
		break;
	}

	spin_lock_bh(&cmd_mgr->free_list_lock[index]);
	free_sqes = atomic_read(&tgt->free_sqes);
	if ((list_empty(&(cmd_mgr->free_list[index]))) ||
	    (tgt->num_active_ios.counter  >= max_sqes) ||
	    (free_sqes + max_sqes <= BNX2FC_SQ_WQES_MAX)) {
		BNX2FC_TGT_DBG(tgt, "No free els_tm cmds available "
			"ios(%d):sqes(%d)\n",
			tgt->num_active_ios.counter, tgt->max_sqes);
		if (list_empty(&(cmd_mgr->free_list[index])))
			printk(KERN_ERR PFX "elstm_alloc: list_empty\n");
		spin_unlock_bh(&cmd_mgr->free_list_lock[index]);
		return NULL;
	}

	listp = (struct list_head *)
			cmd_mgr->free_list[index].next;
	list_del_init(listp);
	io_req = (struct bnx2fc_cmd *) listp;
	xid = io_req->xid;
	cmd_mgr->cmds[xid] = io_req;
	atomic_inc(&tgt->num_active_ios);
	atomic_dec(&tgt->free_sqes);
	spin_unlock_bh(&cmd_mgr->free_list_lock[index]);

	INIT_LIST_HEAD(&io_req->link);

	io_req->port = port;
	io_req->cmd_mgr = cmd_mgr;
	io_req->req_flags = 0;
	io_req->cmd_type = type;

	
	
	bd_tbl = io_req->bd_tbl = cmd_mgr->io_bdt_pool[xid];
	bd_tbl->io_req = io_req;

	
	kref_init(&io_req->refcount);
	return io_req;
}

struct bnx2fc_cmd *bnx2fc_cmd_alloc(struct bnx2fc_rport *tgt)
{
	struct fcoe_port *port = tgt->port;
	struct bnx2fc_interface *interface = port->priv;
	struct bnx2fc_cmd_mgr *cmd_mgr = interface->hba->cmd_mgr;
	struct bnx2fc_cmd *io_req;
	struct list_head *listp;
	struct io_bdt *bd_tbl;
	u32 free_sqes;
	u32 max_sqes;
	u16 xid;
	int index = get_cpu();

	max_sqes = BNX2FC_SCSI_MAX_SQES;
	spin_lock_bh(&cmd_mgr->free_list_lock[index]);
	free_sqes = atomic_read(&tgt->free_sqes);
	if ((list_empty(&cmd_mgr->free_list[index])) ||
	    (tgt->num_active_ios.counter  >= max_sqes) ||
	    (free_sqes + max_sqes <= BNX2FC_SQ_WQES_MAX)) {
		spin_unlock_bh(&cmd_mgr->free_list_lock[index]);
		put_cpu();
		return NULL;
	}

	listp = (struct list_head *)
		cmd_mgr->free_list[index].next;
	list_del_init(listp);
	io_req = (struct bnx2fc_cmd *) listp;
	xid = io_req->xid;
	cmd_mgr->cmds[xid] = io_req;
	atomic_inc(&tgt->num_active_ios);
	atomic_dec(&tgt->free_sqes);
	spin_unlock_bh(&cmd_mgr->free_list_lock[index]);
	put_cpu();

	INIT_LIST_HEAD(&io_req->link);

	io_req->port = port;
	io_req->cmd_mgr = cmd_mgr;
	io_req->req_flags = 0;

	
	
	bd_tbl = io_req->bd_tbl = cmd_mgr->io_bdt_pool[xid];
	bd_tbl->io_req = io_req;

	
	kref_init(&io_req->refcount);
	return io_req;
}

void bnx2fc_cmd_release(struct kref *ref)
{
	struct bnx2fc_cmd *io_req = container_of(ref,
						struct bnx2fc_cmd, refcount);
	struct bnx2fc_cmd_mgr *cmd_mgr = io_req->cmd_mgr;
	int index;

	if (io_req->cmd_type == BNX2FC_SCSI_CMD)
		index = io_req->xid % num_possible_cpus();
	else
		index = RESERVE_FREE_LIST_INDEX;


	spin_lock_bh(&cmd_mgr->free_list_lock[index]);
	if (io_req->cmd_type != BNX2FC_SCSI_CMD)
		bnx2fc_free_mp_resc(io_req);
	cmd_mgr->cmds[io_req->xid] = NULL;
	
	list_del_init(&io_req->link);
	
	list_add(&io_req->link,
			&cmd_mgr->free_list[index]);
	atomic_dec(&io_req->tgt->num_active_ios);
	spin_unlock_bh(&cmd_mgr->free_list_lock[index]);

}

static void bnx2fc_free_mp_resc(struct bnx2fc_cmd *io_req)
{
	struct bnx2fc_mp_req *mp_req = &(io_req->mp_req);
	struct bnx2fc_interface *interface = io_req->port->priv;
	struct bnx2fc_hba *hba = interface->hba;
	size_t sz = sizeof(struct fcoe_bd_ctx);

	
	mp_req->tm_flags = 0;
	if (mp_req->mp_req_bd) {
		dma_free_coherent(&hba->pcidev->dev, sz,
				     mp_req->mp_req_bd,
				     mp_req->mp_req_bd_dma);
		mp_req->mp_req_bd = NULL;
	}
	if (mp_req->mp_resp_bd) {
		dma_free_coherent(&hba->pcidev->dev, sz,
				     mp_req->mp_resp_bd,
				     mp_req->mp_resp_bd_dma);
		mp_req->mp_resp_bd = NULL;
	}
	if (mp_req->req_buf) {
		dma_free_coherent(&hba->pcidev->dev, PAGE_SIZE,
				     mp_req->req_buf,
				     mp_req->req_buf_dma);
		mp_req->req_buf = NULL;
	}
	if (mp_req->resp_buf) {
		dma_free_coherent(&hba->pcidev->dev, PAGE_SIZE,
				     mp_req->resp_buf,
				     mp_req->resp_buf_dma);
		mp_req->resp_buf = NULL;
	}
}

int bnx2fc_init_mp_req(struct bnx2fc_cmd *io_req)
{
	struct bnx2fc_mp_req *mp_req;
	struct fcoe_bd_ctx *mp_req_bd;
	struct fcoe_bd_ctx *mp_resp_bd;
	struct bnx2fc_interface *interface = io_req->port->priv;
	struct bnx2fc_hba *hba = interface->hba;
	dma_addr_t addr;
	size_t sz;

	mp_req = (struct bnx2fc_mp_req *)&(io_req->mp_req);
	memset(mp_req, 0, sizeof(struct bnx2fc_mp_req));

	mp_req->req_len = sizeof(struct fcp_cmnd);
	io_req->data_xfer_len = mp_req->req_len;
	mp_req->req_buf = dma_alloc_coherent(&hba->pcidev->dev, PAGE_SIZE,
					     &mp_req->req_buf_dma,
					     GFP_ATOMIC);
	if (!mp_req->req_buf) {
		printk(KERN_ERR PFX "unable to alloc MP req buffer\n");
		bnx2fc_free_mp_resc(io_req);
		return FAILED;
	}

	mp_req->resp_buf = dma_alloc_coherent(&hba->pcidev->dev, PAGE_SIZE,
					      &mp_req->resp_buf_dma,
					      GFP_ATOMIC);
	if (!mp_req->resp_buf) {
		printk(KERN_ERR PFX "unable to alloc TM resp buffer\n");
		bnx2fc_free_mp_resc(io_req);
		return FAILED;
	}
	memset(mp_req->req_buf, 0, PAGE_SIZE);
	memset(mp_req->resp_buf, 0, PAGE_SIZE);

	
	sz = sizeof(struct fcoe_bd_ctx);
	mp_req->mp_req_bd = dma_alloc_coherent(&hba->pcidev->dev, sz,
						 &mp_req->mp_req_bd_dma,
						 GFP_ATOMIC);
	if (!mp_req->mp_req_bd) {
		printk(KERN_ERR PFX "unable to alloc MP req bd\n");
		bnx2fc_free_mp_resc(io_req);
		return FAILED;
	}
	mp_req->mp_resp_bd = dma_alloc_coherent(&hba->pcidev->dev, sz,
						 &mp_req->mp_resp_bd_dma,
						 GFP_ATOMIC);
	if (!mp_req->mp_req_bd) {
		printk(KERN_ERR PFX "unable to alloc MP resp bd\n");
		bnx2fc_free_mp_resc(io_req);
		return FAILED;
	}
	
	addr = mp_req->req_buf_dma;
	mp_req_bd = mp_req->mp_req_bd;
	mp_req_bd->buf_addr_lo = (u32)addr & 0xffffffff;
	mp_req_bd->buf_addr_hi = (u32)((u64)addr >> 32);
	mp_req_bd->buf_len = PAGE_SIZE;
	mp_req_bd->flags = 0;

	mp_resp_bd = mp_req->mp_resp_bd;
	addr = mp_req->resp_buf_dma;
	mp_resp_bd->buf_addr_lo = (u32)addr & 0xffffffff;
	mp_resp_bd->buf_addr_hi = (u32)((u64)addr >> 32);
	mp_resp_bd->buf_len = PAGE_SIZE;
	mp_resp_bd->flags = 0;

	return SUCCESS;
}

static int bnx2fc_initiate_tmf(struct scsi_cmnd *sc_cmd, u8 tm_flags)
{
	struct fc_lport *lport;
	struct fc_rport *rport = starget_to_rport(scsi_target(sc_cmd->device));
	struct fc_rport_libfc_priv *rp = rport->dd_data;
	struct fcoe_port *port;
	struct bnx2fc_interface *interface;
	struct bnx2fc_rport *tgt;
	struct bnx2fc_cmd *io_req;
	struct bnx2fc_mp_req *tm_req;
	struct fcoe_task_ctx_entry *task;
	struct fcoe_task_ctx_entry *task_page;
	struct Scsi_Host *host = sc_cmd->device->host;
	struct fc_frame_header *fc_hdr;
	struct fcp_cmnd *fcp_cmnd;
	int task_idx, index;
	int rc = SUCCESS;
	u16 xid;
	u32 sid, did;
	unsigned long start = jiffies;

	lport = shost_priv(host);
	port = lport_priv(lport);
	interface = port->priv;

	if (rport == NULL) {
		printk(KERN_ERR PFX "device_reset: rport is NULL\n");
		rc = FAILED;
		goto tmf_err;
	}

	rc = fc_block_scsi_eh(sc_cmd);
	if (rc)
		return rc;

	if (lport->state != LPORT_ST_READY || !(lport->link_up)) {
		printk(KERN_ERR PFX "device_reset: link is not ready\n");
		rc = FAILED;
		goto tmf_err;
	}
	
	tgt = (struct bnx2fc_rport *)&rp[1];

	if (!(test_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags))) {
		printk(KERN_ERR PFX "device_reset: tgt not offloaded\n");
		rc = FAILED;
		goto tmf_err;
	}
retry_tmf:
	io_req = bnx2fc_elstm_alloc(tgt, BNX2FC_TASK_MGMT_CMD);
	if (!io_req) {
		if (time_after(jiffies, start + HZ)) {
			printk(KERN_ERR PFX "tmf: Failed TMF");
			rc = FAILED;
			goto tmf_err;
		}
		msleep(20);
		goto retry_tmf;
	}
	
	io_req->sc_cmd = sc_cmd;
	io_req->port = port;
	io_req->tgt = tgt;

	tm_req = (struct bnx2fc_mp_req *)&(io_req->mp_req);

	rc = bnx2fc_init_mp_req(io_req);
	if (rc == FAILED) {
		printk(KERN_ERR PFX "Task mgmt MP request init failed\n");
		spin_lock_bh(&tgt->tgt_lock);
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		goto tmf_err;
	}

	
	io_req->io_req_flags = 0;
	tm_req->tm_flags = tm_flags;

	
	bnx2fc_build_fcp_cmnd(io_req, (struct fcp_cmnd *)tm_req->req_buf);
	fcp_cmnd = (struct fcp_cmnd *)tm_req->req_buf;
	memset(fcp_cmnd->fc_cdb, 0,  sc_cmd->cmd_len);
	fcp_cmnd->fc_dl = 0;

	
	fc_hdr = &(tm_req->req_fc_hdr);
	sid = tgt->sid;
	did = rport->port_id;
	__fc_fill_fc_hdr(fc_hdr, FC_RCTL_DD_UNSOL_CMD, did, sid,
			   FC_TYPE_FCP, FC_FC_FIRST_SEQ | FC_FC_END_SEQ |
			   FC_FC_SEQ_INIT, 0);
	
	xid = io_req->xid;

	BNX2FC_TGT_DBG(tgt, "Initiate TMF - xid = 0x%x\n", xid);
	task_idx = xid/BNX2FC_TASKS_PER_PAGE;
	index = xid % BNX2FC_TASKS_PER_PAGE;

	
	task_page = (struct fcoe_task_ctx_entry *)
			interface->hba->task_ctx[task_idx];
	task = &(task_page[index]);
	bnx2fc_init_mp_task(io_req, task);

	sc_cmd->SCp.ptr = (char *)io_req;

	
	spin_lock_bh(&tgt->tgt_lock);
	bnx2fc_add_2_sq(tgt, xid);

	
	io_req->on_tmf_queue = 1;
	list_add_tail(&io_req->link, &tgt->active_tm_queue);

	init_completion(&io_req->tm_done);
	io_req->wait_for_comp = 1;

	
	bnx2fc_ring_doorbell(tgt);
	spin_unlock_bh(&tgt->tgt_lock);

	rc = wait_for_completion_timeout(&io_req->tm_done,
					 BNX2FC_TM_TIMEOUT * HZ);
	spin_lock_bh(&tgt->tgt_lock);

	io_req->wait_for_comp = 0;
	if (!(test_bit(BNX2FC_FLAG_TM_COMPL, &io_req->req_flags)))
		set_bit(BNX2FC_FLAG_TM_TIMEOUT, &io_req->req_flags);

	spin_unlock_bh(&tgt->tgt_lock);

	if (!rc) {
		BNX2FC_TGT_DBG(tgt, "task mgmt command failed...\n");
		rc = FAILED;
	} else {
		BNX2FC_TGT_DBG(tgt, "task mgmt command success...\n");
		rc = SUCCESS;
	}
tmf_err:
	return rc;
}

int bnx2fc_initiate_abts(struct bnx2fc_cmd *io_req)
{
	struct fc_lport *lport;
	struct bnx2fc_rport *tgt = io_req->tgt;
	struct fc_rport *rport = tgt->rport;
	struct fc_rport_priv *rdata = tgt->rdata;
	struct bnx2fc_interface *interface;
	struct fcoe_port *port;
	struct bnx2fc_cmd *abts_io_req;
	struct fcoe_task_ctx_entry *task;
	struct fcoe_task_ctx_entry *task_page;
	struct fc_frame_header *fc_hdr;
	struct bnx2fc_mp_req *abts_req;
	int task_idx, index;
	u32 sid, did;
	u16 xid;
	int rc = SUCCESS;
	u32 r_a_tov = rdata->r_a_tov;

	
	BNX2FC_IO_DBG(io_req, "Entered bnx2fc_initiate_abts\n");

	port = io_req->port;
	interface = port->priv;
	lport = port->lport;

	if (!test_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags)) {
		printk(KERN_ERR PFX "initiate_abts: tgt not offloaded\n");
		rc = FAILED;
		goto abts_err;
	}

	if (rport == NULL) {
		printk(KERN_ERR PFX "initiate_abts: rport is NULL\n");
		rc = FAILED;
		goto abts_err;
	}

	if (lport->state != LPORT_ST_READY || !(lport->link_up)) {
		printk(KERN_ERR PFX "initiate_abts: link is not ready\n");
		rc = FAILED;
		goto abts_err;
	}

	abts_io_req = bnx2fc_elstm_alloc(tgt, BNX2FC_ABTS);
	if (!abts_io_req) {
		printk(KERN_ERR PFX "abts: couldnt allocate cmd\n");
		rc = FAILED;
		goto abts_err;
	}

	
	abts_io_req->sc_cmd = NULL;
	abts_io_req->port = port;
	abts_io_req->tgt = tgt;
	abts_io_req->data_xfer_len = 0; 

	abts_req = (struct bnx2fc_mp_req *)&(abts_io_req->mp_req);
	memset(abts_req, 0, sizeof(struct bnx2fc_mp_req));

	
	fc_hdr = &(abts_req->req_fc_hdr);

	
	fc_hdr->fh_ox_id = htons(io_req->xid);
	fc_hdr->fh_rx_id = htons(io_req->task->rxwr_txrd.var_ctx.rx_id);

	sid = tgt->sid;
	did = rport->port_id;

	__fc_fill_fc_hdr(fc_hdr, FC_RCTL_BA_ABTS, did, sid,
			   FC_TYPE_BLS, FC_FC_FIRST_SEQ | FC_FC_END_SEQ |
			   FC_FC_SEQ_INIT, 0);

	xid = abts_io_req->xid;
	BNX2FC_IO_DBG(abts_io_req, "ABTS io_req\n");
	task_idx = xid/BNX2FC_TASKS_PER_PAGE;
	index = xid % BNX2FC_TASKS_PER_PAGE;

	
	task_page = (struct fcoe_task_ctx_entry *)
			interface->hba->task_ctx[task_idx];
	task = &(task_page[index]);
	bnx2fc_init_mp_task(abts_io_req, task);


	
	bnx2fc_cmd_timer_set(io_req, 2 * r_a_tov);

	
	bnx2fc_add_2_sq(tgt, xid);

	
	bnx2fc_ring_doorbell(tgt);

abts_err:
	return rc;
}

int bnx2fc_initiate_seq_cleanup(struct bnx2fc_cmd *orig_io_req, u32 offset,
				enum fc_rctl r_ctl)
{
	struct fc_lport *lport;
	struct bnx2fc_rport *tgt = orig_io_req->tgt;
	struct bnx2fc_interface *interface;
	struct fcoe_port *port;
	struct bnx2fc_cmd *seq_clnp_req;
	struct fcoe_task_ctx_entry *task;
	struct fcoe_task_ctx_entry *task_page;
	struct bnx2fc_els_cb_arg *cb_arg = NULL;
	int task_idx, index;
	u16 xid;
	int rc = 0;

	BNX2FC_IO_DBG(orig_io_req, "bnx2fc_initiate_seq_cleanup xid = 0x%x\n",
		   orig_io_req->xid);
	kref_get(&orig_io_req->refcount);

	port = orig_io_req->port;
	interface = port->priv;
	lport = port->lport;

	cb_arg = kzalloc(sizeof(struct bnx2fc_els_cb_arg), GFP_ATOMIC);
	if (!cb_arg) {
		printk(KERN_ERR PFX "Unable to alloc cb_arg for seq clnup\n");
		rc = -ENOMEM;
		goto cleanup_err;
	}

	seq_clnp_req = bnx2fc_elstm_alloc(tgt, BNX2FC_SEQ_CLEANUP);
	if (!seq_clnp_req) {
		printk(KERN_ERR PFX "cleanup: couldnt allocate cmd\n");
		rc = -ENOMEM;
		kfree(cb_arg);
		goto cleanup_err;
	}
	
	seq_clnp_req->sc_cmd = NULL;
	seq_clnp_req->port = port;
	seq_clnp_req->tgt = tgt;
	seq_clnp_req->data_xfer_len = 0; 

	xid = seq_clnp_req->xid;

	task_idx = xid/BNX2FC_TASKS_PER_PAGE;
	index = xid % BNX2FC_TASKS_PER_PAGE;

	
	task_page = (struct fcoe_task_ctx_entry *)
		     interface->hba->task_ctx[task_idx];
	task = &(task_page[index]);
	cb_arg->aborted_io_req = orig_io_req;
	cb_arg->io_req = seq_clnp_req;
	cb_arg->r_ctl = r_ctl;
	cb_arg->offset = offset;
	seq_clnp_req->cb_arg = cb_arg;

	printk(KERN_ERR PFX "call init_seq_cleanup_task\n");
	bnx2fc_init_seq_cleanup_task(seq_clnp_req, task, orig_io_req, offset);

	
	bnx2fc_add_2_sq(tgt, xid);

	
	bnx2fc_ring_doorbell(tgt);
cleanup_err:
	return rc;
}

int bnx2fc_initiate_cleanup(struct bnx2fc_cmd *io_req)
{
	struct fc_lport *lport;
	struct bnx2fc_rport *tgt = io_req->tgt;
	struct bnx2fc_interface *interface;
	struct fcoe_port *port;
	struct bnx2fc_cmd *cleanup_io_req;
	struct fcoe_task_ctx_entry *task;
	struct fcoe_task_ctx_entry *task_page;
	int task_idx, index;
	u16 xid, orig_xid;
	int rc = 0;

	
	BNX2FC_IO_DBG(io_req, "Entered bnx2fc_initiate_cleanup\n");

	port = io_req->port;
	interface = port->priv;
	lport = port->lport;

	cleanup_io_req = bnx2fc_elstm_alloc(tgt, BNX2FC_CLEANUP);
	if (!cleanup_io_req) {
		printk(KERN_ERR PFX "cleanup: couldnt allocate cmd\n");
		rc = -1;
		goto cleanup_err;
	}

	
	cleanup_io_req->sc_cmd = NULL;
	cleanup_io_req->port = port;
	cleanup_io_req->tgt = tgt;
	cleanup_io_req->data_xfer_len = 0; 

	xid = cleanup_io_req->xid;

	task_idx = xid/BNX2FC_TASKS_PER_PAGE;
	index = xid % BNX2FC_TASKS_PER_PAGE;

	
	task_page = (struct fcoe_task_ctx_entry *)
			interface->hba->task_ctx[task_idx];
	task = &(task_page[index]);
	orig_xid = io_req->xid;

	BNX2FC_IO_DBG(io_req, "CLEANUP io_req xid = 0x%x\n", xid);

	bnx2fc_init_cleanup_task(cleanup_io_req, task, orig_xid);

	
	bnx2fc_add_2_sq(tgt, xid);

	
	bnx2fc_ring_doorbell(tgt);

cleanup_err:
	return rc;
}

int bnx2fc_eh_target_reset(struct scsi_cmnd *sc_cmd)
{
	return bnx2fc_initiate_tmf(sc_cmd, FCP_TMF_TGT_RESET);
}

int bnx2fc_eh_device_reset(struct scsi_cmnd *sc_cmd)
{
	return bnx2fc_initiate_tmf(sc_cmd, FCP_TMF_LUN_RESET);
}

int bnx2fc_eh_abort(struct scsi_cmnd *sc_cmd)
{
	struct fc_rport *rport = starget_to_rport(scsi_target(sc_cmd->device));
	struct fc_rport_libfc_priv *rp = rport->dd_data;
	struct bnx2fc_cmd *io_req;
	struct fc_lport *lport;
	struct fc_rport_priv *rdata;
	struct bnx2fc_rport *tgt;
	int logo_issued;
	int wait_cnt = 0;
	int rc = FAILED;


	rc = fc_block_scsi_eh(sc_cmd);
	if (rc)
		return rc;

	lport = shost_priv(sc_cmd->device->host);
	if ((lport->state != LPORT_ST_READY) || !(lport->link_up)) {
		printk(KERN_ERR PFX "eh_abort: link not ready\n");
		return rc;
	}

	tgt = (struct bnx2fc_rport *)&rp[1];

	BNX2FC_TGT_DBG(tgt, "Entered bnx2fc_eh_abort\n");

	spin_lock_bh(&tgt->tgt_lock);
	io_req = (struct bnx2fc_cmd *)sc_cmd->SCp.ptr;
	if (!io_req) {
		
		printk(KERN_ERR PFX "eh_abort: io_req is NULL\n");
		spin_unlock_bh(&tgt->tgt_lock);
		return SUCCESS;
	}
	BNX2FC_IO_DBG(io_req, "eh_abort - refcnt = %d\n",
		      io_req->refcount.refcount.counter);

	
	kref_get(&io_req->refcount);

	BUG_ON(tgt != io_req->tgt);

	
	if (tgt->flush_in_prog) {
		printk(KERN_ERR PFX "eh_abort: io_req (xid = 0x%x) "
			"flush in progress\n", io_req->xid);
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		return SUCCESS;
	}

	if (io_req->on_active_queue == 0) {
		printk(KERN_ERR PFX "eh_abort: io_req (xid = 0x%x) "
				"not on active_q\n", io_req->xid);
		bnx2fc_scsi_done(io_req, DID_ABORT);

		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		return SUCCESS;
	}

	list_del_init(&io_req->link);
	io_req->on_active_queue = 0;
	
	list_add_tail(&io_req->link, &tgt->io_retire_queue);

	init_completion(&io_req->tm_done);
	io_req->wait_for_comp = 1;

	if (!test_and_set_bit(BNX2FC_FLAG_ISSUE_ABTS, &io_req->req_flags)) {
		
		if (cancel_delayed_work(&io_req->timeout_work))
			kref_put(&io_req->refcount,
				 bnx2fc_cmd_release); 
		set_bit(BNX2FC_FLAG_EH_ABORT, &io_req->req_flags);
		rc = bnx2fc_initiate_abts(io_req);
	} else {
		printk(KERN_ERR PFX "eh_abort: io_req (xid = 0x%x) "
				"already in abts processing\n", io_req->xid);
		if (cancel_delayed_work(&io_req->timeout_work))
			kref_put(&io_req->refcount,
				 bnx2fc_cmd_release); 
		bnx2fc_initiate_cleanup(io_req);

		spin_unlock_bh(&tgt->tgt_lock);

		wait_for_completion(&io_req->tm_done);

		spin_lock_bh(&tgt->tgt_lock);
		io_req->wait_for_comp = 0;
		rdata = io_req->tgt->rdata;
		logo_issued = test_and_set_bit(BNX2FC_FLAG_EXPL_LOGO,
					       &tgt->flags);
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);

		if (!logo_issued) {
			BNX2FC_IO_DBG(io_req, "Expl logo - tgt flags = 0x%lx\n",
				      tgt->flags);
			mutex_lock(&lport->disc.disc_mutex);
			lport->tt.rport_logoff(rdata);
			mutex_unlock(&lport->disc.disc_mutex);
			do {
				msleep(BNX2FC_RELOGIN_WAIT_TIME);
				if (wait_cnt++ > BNX2FC_RELOGIN_WAIT_CNT)
					return FAILED;
			} while (!test_bit(BNX2FC_FLAG_SESSION_READY,
					   &tgt->flags));
		}
		return SUCCESS;
	}
	if (rc == FAILED) {
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		return rc;
	}
	spin_unlock_bh(&tgt->tgt_lock);

	wait_for_completion(&io_req->tm_done);

	spin_lock_bh(&tgt->tgt_lock);
	io_req->wait_for_comp = 0;
	if (!(test_and_set_bit(BNX2FC_FLAG_ABTS_DONE,
				    &io_req->req_flags))) {
		
		printk(KERN_ERR PFX "abort failed, xid = 0x%x\n",
		       io_req->xid);
		rc = FAILED;
	} else {
		BNX2FC_IO_DBG(io_req, "abort succeeded\n");
		rc = SUCCESS;
		bnx2fc_scsi_done(io_req, DID_ABORT);
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
	}

	
	kref_put(&io_req->refcount, bnx2fc_cmd_release);
	spin_unlock_bh(&tgt->tgt_lock);
	return rc;
}

void bnx2fc_process_seq_cleanup_compl(struct bnx2fc_cmd *seq_clnp_req,
				      struct fcoe_task_ctx_entry *task,
				      u8 rx_state)
{
	struct bnx2fc_els_cb_arg *cb_arg = seq_clnp_req->cb_arg;
	struct bnx2fc_cmd *orig_io_req = cb_arg->aborted_io_req;
	u32 offset = cb_arg->offset;
	enum fc_rctl r_ctl = cb_arg->r_ctl;
	int rc = 0;
	struct bnx2fc_rport *tgt = orig_io_req->tgt;

	BNX2FC_IO_DBG(orig_io_req, "Entered process_cleanup_compl xid = 0x%x"
			      "cmd_type = %d\n",
		   seq_clnp_req->xid, seq_clnp_req->cmd_type);

	if (rx_state == FCOE_TASK_RX_STATE_IGNORED_SEQUENCE_CLEANUP) {
		printk(KERN_ERR PFX "seq cleanup ignored - xid = 0x%x\n",
			seq_clnp_req->xid);
		goto free_cb_arg;
	}

	spin_unlock_bh(&tgt->tgt_lock);
	rc = bnx2fc_send_srr(orig_io_req, offset, r_ctl);
	spin_lock_bh(&tgt->tgt_lock);

	if (rc)
		printk(KERN_ERR PFX "clnup_compl: Unable to send SRR"
			" IO will abort\n");
	seq_clnp_req->cb_arg = NULL;
	kref_put(&orig_io_req->refcount, bnx2fc_cmd_release);
free_cb_arg:
	kfree(cb_arg);
	return;
}

void bnx2fc_process_cleanup_compl(struct bnx2fc_cmd *io_req,
				  struct fcoe_task_ctx_entry *task,
				  u8 num_rq)
{
	BNX2FC_IO_DBG(io_req, "Entered process_cleanup_compl "
			      "refcnt = %d, cmd_type = %d\n",
		   io_req->refcount.refcount.counter, io_req->cmd_type);
	bnx2fc_scsi_done(io_req, DID_ERROR);
	kref_put(&io_req->refcount, bnx2fc_cmd_release);
	if (io_req->wait_for_comp)
		complete(&io_req->tm_done);
}

void bnx2fc_process_abts_compl(struct bnx2fc_cmd *io_req,
			       struct fcoe_task_ctx_entry *task,
			       u8 num_rq)
{
	u32 r_ctl;
	u32 r_a_tov = FC_DEF_R_A_TOV;
	u8 issue_rrq = 0;
	struct bnx2fc_rport *tgt = io_req->tgt;

	BNX2FC_IO_DBG(io_req, "Entered process_abts_compl xid = 0x%x"
			      "refcnt = %d, cmd_type = %d\n",
		   io_req->xid,
		   io_req->refcount.refcount.counter, io_req->cmd_type);

	if (test_and_set_bit(BNX2FC_FLAG_ABTS_DONE,
				       &io_req->req_flags)) {
		BNX2FC_IO_DBG(io_req, "Timer context finished processing"
				" this io\n");
		return;
	}

	
	if (test_and_set_bit(BNX2FC_FLAG_IO_CLEANUP,
				&io_req->req_flags))
		goto io_compl;

	if (test_bit(BNX2FC_FLAG_ISSUE_ABTS, &io_req->req_flags))
		if (cancel_delayed_work(&io_req->timeout_work))
			kref_put(&io_req->refcount,
				 bnx2fc_cmd_release); 

	r_ctl = (u8)task->rxwr_only.union_ctx.comp_info.abts_rsp.r_ctl;

	switch (r_ctl) {
	case FC_RCTL_BA_ACC:
		BNX2FC_IO_DBG(io_req, "ABTS response - ACC Send RRQ\n");
		issue_rrq = 1;
		break;

	case FC_RCTL_BA_RJT:
		BNX2FC_IO_DBG(io_req, "ABTS response - RJT\n");
		break;
	default:
		printk(KERN_ERR PFX "Unknown ABTS response\n");
		break;
	}

	if (issue_rrq) {
		BNX2FC_IO_DBG(io_req, "Issue RRQ after R_A_TOV\n");
		set_bit(BNX2FC_FLAG_ISSUE_RRQ, &io_req->req_flags);
	}
	set_bit(BNX2FC_FLAG_RETIRE_OXID, &io_req->req_flags);
	bnx2fc_cmd_timer_set(io_req, r_a_tov);

io_compl:
	if (io_req->wait_for_comp) {
		if (test_and_clear_bit(BNX2FC_FLAG_EH_ABORT,
				       &io_req->req_flags))
			complete(&io_req->tm_done);
	} else {

		if (io_req->on_active_queue) {
			list_del_init(&io_req->link);
			io_req->on_active_queue = 0;
			
			list_add_tail(&io_req->link, &tgt->io_retire_queue);
		}
		bnx2fc_scsi_done(io_req, DID_ERROR);
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
	}
}

static void bnx2fc_lun_reset_cmpl(struct bnx2fc_cmd *io_req)
{
	struct scsi_cmnd *sc_cmd = io_req->sc_cmd;
	struct bnx2fc_rport *tgt = io_req->tgt;
	struct list_head *list;
	struct list_head *tmp;
	struct bnx2fc_cmd *cmd;
	int tm_lun = sc_cmd->device->lun;
	int rc = 0;
	int lun;

	
	BNX2FC_IO_DBG(io_req, "Entered bnx2fc_lun_reset_cmpl\n");
	list_for_each_safe(list, tmp, &tgt->active_cmd_queue) {
		BNX2FC_TGT_DBG(tgt, "LUN RST cmpl: scan for pending IOs\n");
		cmd = (struct bnx2fc_cmd *)list;
		lun = cmd->sc_cmd->device->lun;
		if (lun == tm_lun) {
			
			if (!test_and_set_bit(BNX2FC_FLAG_ISSUE_ABTS,
					      &cmd->req_flags)) {
				
				if (cancel_delayed_work(&io_req->timeout_work))
					kref_put(&io_req->refcount,
						 bnx2fc_cmd_release);
							
				rc = bnx2fc_initiate_abts(cmd);
				
				WARN_ON(rc != SUCCESS);
			} else
				printk(KERN_ERR PFX "lun_rst: abts already in"
					" progress for this IO 0x%x\n",
					cmd->xid);
		}
	}
}

static void bnx2fc_tgt_reset_cmpl(struct bnx2fc_cmd *io_req)
{
	struct bnx2fc_rport *tgt = io_req->tgt;
	struct list_head *list;
	struct list_head *tmp;
	struct bnx2fc_cmd *cmd;
	int rc = 0;

	
	BNX2FC_IO_DBG(io_req, "Entered bnx2fc_tgt_reset_cmpl\n");
	list_for_each_safe(list, tmp, &tgt->active_cmd_queue) {
		BNX2FC_TGT_DBG(tgt, "TGT RST cmpl: scan for pending IOs\n");
		cmd = (struct bnx2fc_cmd *)list;
		
		if (!test_and_set_bit(BNX2FC_FLAG_ISSUE_ABTS,
							&cmd->req_flags)) {
			
			if (cancel_delayed_work(&io_req->timeout_work))
				kref_put(&io_req->refcount,
					 bnx2fc_cmd_release); 
			rc = bnx2fc_initiate_abts(cmd);
			
			WARN_ON(rc != SUCCESS);

		} else
			printk(KERN_ERR PFX "tgt_rst: abts already in progress"
				" for this IO 0x%x\n", cmd->xid);
	}
}

void bnx2fc_process_tm_compl(struct bnx2fc_cmd *io_req,
			     struct fcoe_task_ctx_entry *task, u8 num_rq)
{
	struct bnx2fc_mp_req *tm_req;
	struct fc_frame_header *fc_hdr;
	struct scsi_cmnd *sc_cmd = io_req->sc_cmd;
	u64 *hdr;
	u64 *temp_hdr;
	void *rsp_buf;

	
	BNX2FC_IO_DBG(io_req, "Entered process_tm_compl\n");

	if (!(test_bit(BNX2FC_FLAG_TM_TIMEOUT, &io_req->req_flags)))
		set_bit(BNX2FC_FLAG_TM_COMPL, &io_req->req_flags);
	else {
		return;
	}

	tm_req = &(io_req->mp_req);
	fc_hdr = &(tm_req->resp_fc_hdr);
	hdr = (u64 *)fc_hdr;
	temp_hdr = (u64 *)
		&task->rxwr_only.union_ctx.comp_info.mp_rsp.fc_hdr;
	hdr[0] = cpu_to_be64(temp_hdr[0]);
	hdr[1] = cpu_to_be64(temp_hdr[1]);
	hdr[2] = cpu_to_be64(temp_hdr[2]);

	tm_req->resp_len =
		task->rxwr_only.union_ctx.comp_info.mp_rsp.mp_payload_len;

	rsp_buf = tm_req->resp_buf;

	if (fc_hdr->fh_r_ctl == FC_RCTL_DD_CMD_STATUS) {
		bnx2fc_parse_fcp_rsp(io_req,
				     (struct fcoe_fcp_rsp_payload *)
				     rsp_buf, num_rq);
		if (io_req->fcp_rsp_code == 0) {
			
			if (tm_req->tm_flags & FCP_TMF_LUN_RESET)
				bnx2fc_lun_reset_cmpl(io_req);
			else if (tm_req->tm_flags & FCP_TMF_TGT_RESET)
				bnx2fc_tgt_reset_cmpl(io_req);
		}
	} else {
		printk(KERN_ERR PFX "tmf's fc_hdr r_ctl = 0x%x\n",
			fc_hdr->fh_r_ctl);
	}
	if (!sc_cmd->SCp.ptr) {
		printk(KERN_ERR PFX "tm_compl: SCp.ptr is NULL\n");
		return;
	}
	switch (io_req->fcp_status) {
	case FC_GOOD:
		if (io_req->cdb_status == 0) {
			
			sc_cmd->result = DID_OK << 16;
		} else {
			
			sc_cmd->result = (DID_OK << 16) | io_req->cdb_status;
		}
		if (io_req->fcp_resid)
			scsi_set_resid(sc_cmd, io_req->fcp_resid);
		break;

	default:
		BNX2FC_IO_DBG(io_req, "process_tm_compl: fcp_status = %d\n",
			   io_req->fcp_status);
		break;
	}

	sc_cmd = io_req->sc_cmd;
	io_req->sc_cmd = NULL;

	
	if (io_req->on_tmf_queue) {

		list_del_init(&io_req->link);
		io_req->on_tmf_queue = 0;
	} else {

		printk(KERN_ERR PFX "Command not on active_cmd_queue!\n");
		return;
	}

	sc_cmd->SCp.ptr = NULL;
	sc_cmd->scsi_done(sc_cmd);

	kref_put(&io_req->refcount, bnx2fc_cmd_release);
	if (io_req->wait_for_comp) {
		BNX2FC_IO_DBG(io_req, "tm_compl - wake up the waiter\n");
		complete(&io_req->tm_done);
	}
}

static int bnx2fc_split_bd(struct bnx2fc_cmd *io_req, u64 addr, int sg_len,
			   int bd_index)
{
	struct fcoe_bd_ctx *bd = io_req->bd_tbl->bd_tbl;
	int frag_size, sg_frags;

	sg_frags = 0;
	while (sg_len) {
		if (sg_len >= BNX2FC_BD_SPLIT_SZ)
			frag_size = BNX2FC_BD_SPLIT_SZ;
		else
			frag_size = sg_len;
		bd[bd_index + sg_frags].buf_addr_lo = addr & 0xffffffff;
		bd[bd_index + sg_frags].buf_addr_hi  = addr >> 32;
		bd[bd_index + sg_frags].buf_len = (u16)frag_size;
		bd[bd_index + sg_frags].flags = 0;

		addr += (u64) frag_size;
		sg_frags++;
		sg_len -= frag_size;
	}
	return sg_frags;

}

static int bnx2fc_map_sg(struct bnx2fc_cmd *io_req)
{
	struct bnx2fc_interface *interface = io_req->port->priv;
	struct bnx2fc_hba *hba = interface->hba;
	struct scsi_cmnd *sc = io_req->sc_cmd;
	struct fcoe_bd_ctx *bd = io_req->bd_tbl->bd_tbl;
	struct scatterlist *sg;
	int byte_count = 0;
	int sg_count = 0;
	int bd_count = 0;
	int sg_frags;
	unsigned int sg_len;
	u64 addr;
	int i;

	sg_count = dma_map_sg(&hba->pcidev->dev, scsi_sglist(sc),
			      scsi_sg_count(sc), sc->sc_data_direction);
	scsi_for_each_sg(sc, sg, sg_count, i) {
		sg_len = sg_dma_len(sg);
		addr = sg_dma_address(sg);
		if (sg_len > BNX2FC_MAX_BD_LEN) {
			sg_frags = bnx2fc_split_bd(io_req, addr, sg_len,
						   bd_count);
		} else {

			sg_frags = 1;
			bd[bd_count].buf_addr_lo = addr & 0xffffffff;
			bd[bd_count].buf_addr_hi  = addr >> 32;
			bd[bd_count].buf_len = (u16)sg_len;
			bd[bd_count].flags = 0;
		}
		bd_count += sg_frags;
		byte_count += sg_len;
	}
	if (byte_count != scsi_bufflen(sc))
		printk(KERN_ERR PFX "byte_count = %d != scsi_bufflen = %d, "
			"task_id = 0x%x\n", byte_count, scsi_bufflen(sc),
			io_req->xid);
	return bd_count;
}

static int bnx2fc_build_bd_list_from_sg(struct bnx2fc_cmd *io_req)
{
	struct scsi_cmnd *sc = io_req->sc_cmd;
	struct fcoe_bd_ctx *bd = io_req->bd_tbl->bd_tbl;
	int bd_count;

	if (scsi_sg_count(sc)) {
		bd_count = bnx2fc_map_sg(io_req);
		if (bd_count == 0)
			return -ENOMEM;
	} else {
		bd_count = 0;
		bd[0].buf_addr_lo = bd[0].buf_addr_hi = 0;
		bd[0].buf_len = bd[0].flags = 0;
	}
	io_req->bd_tbl->bd_valid = bd_count;

	return 0;
}

static void bnx2fc_unmap_sg_list(struct bnx2fc_cmd *io_req)
{
	struct scsi_cmnd *sc = io_req->sc_cmd;

	if (io_req->bd_tbl->bd_valid && sc) {
		scsi_dma_unmap(sc);
		io_req->bd_tbl->bd_valid = 0;
	}
}

void bnx2fc_build_fcp_cmnd(struct bnx2fc_cmd *io_req,
				  struct fcp_cmnd *fcp_cmnd)
{
	struct scsi_cmnd *sc_cmd = io_req->sc_cmd;
	char tag[2];

	memset(fcp_cmnd, 0, sizeof(struct fcp_cmnd));

	int_to_scsilun(sc_cmd->device->lun, &fcp_cmnd->fc_lun);

	fcp_cmnd->fc_dl = htonl(io_req->data_xfer_len);
	memcpy(fcp_cmnd->fc_cdb, sc_cmd->cmnd, sc_cmd->cmd_len);

	fcp_cmnd->fc_cmdref = 0;
	fcp_cmnd->fc_pri_ta = 0;
	fcp_cmnd->fc_tm_flags = io_req->mp_req.tm_flags;
	fcp_cmnd->fc_flags = io_req->io_req_flags;

	if (scsi_populate_tag_msg(sc_cmd, tag)) {
		switch (tag[0]) {
		case HEAD_OF_QUEUE_TAG:
			fcp_cmnd->fc_pri_ta = FCP_PTA_HEADQ;
			break;
		case ORDERED_QUEUE_TAG:
			fcp_cmnd->fc_pri_ta = FCP_PTA_ORDERED;
			break;
		default:
			fcp_cmnd->fc_pri_ta = FCP_PTA_SIMPLE;
			break;
		}
	} else {
		fcp_cmnd->fc_pri_ta = 0;
	}
}

static void bnx2fc_parse_fcp_rsp(struct bnx2fc_cmd *io_req,
				 struct fcoe_fcp_rsp_payload *fcp_rsp,
				 u8 num_rq)
{
	struct scsi_cmnd *sc_cmd = io_req->sc_cmd;
	struct bnx2fc_rport *tgt = io_req->tgt;
	u8 rsp_flags = fcp_rsp->fcp_flags.flags;
	u32 rq_buff_len = 0;
	int i;
	unsigned char *rq_data;
	unsigned char *dummy;
	int fcp_sns_len = 0;
	int fcp_rsp_len = 0;

	io_req->fcp_status = FC_GOOD;
	io_req->fcp_resid = fcp_rsp->fcp_resid;

	io_req->scsi_comp_flags = rsp_flags;
	CMD_SCSI_STATUS(sc_cmd) = io_req->cdb_status =
				fcp_rsp->scsi_status_code;

	
	if (num_rq) {


		if (rsp_flags &
		    FCOE_FCP_RSP_FLAGS_FCP_RSP_LEN_VALID) {
			fcp_rsp_len = rq_buff_len
					= fcp_rsp->fcp_rsp_len;
		}

		if (rsp_flags &
		    FCOE_FCP_RSP_FLAGS_FCP_SNS_LEN_VALID) {
			fcp_sns_len = fcp_rsp->fcp_sns_len;
			rq_buff_len += fcp_rsp->fcp_sns_len;
		}

		io_req->fcp_rsp_len = fcp_rsp_len;
		io_req->fcp_sns_len = fcp_sns_len;

		if (rq_buff_len > num_rq * BNX2FC_RQ_BUF_SZ) {
			
			printk(KERN_ERR PFX "invalid sns length %d\n",
				rq_buff_len);
			
			rq_buff_len =  num_rq * BNX2FC_RQ_BUF_SZ;
		}

		rq_data = bnx2fc_get_next_rqe(tgt, 1);

		if (num_rq > 1) {
			
			for (i = 1; i < num_rq; i++)
				dummy = bnx2fc_get_next_rqe(tgt, 1);
		}

		
		if ((fcp_rsp_len == 4) || (fcp_rsp_len == 8)) {
			
			io_req->fcp_rsp_code = rq_data[3];
			printk(KERN_ERR PFX "fcp_rsp_code = %d\n",
				io_req->fcp_rsp_code);
		}

		
		rq_data += fcp_rsp_len;

		if (fcp_sns_len > SCSI_SENSE_BUFFERSIZE) {
			printk(KERN_ERR PFX "Truncating sense buffer\n");
			fcp_sns_len = SCSI_SENSE_BUFFERSIZE;
		}

		memset(sc_cmd->sense_buffer, 0, sizeof(sc_cmd->sense_buffer));
		if (fcp_sns_len)
			memcpy(sc_cmd->sense_buffer, rq_data, fcp_sns_len);

		
		for (i = 0; i < num_rq; i++)
			bnx2fc_return_rqe(tgt, 1);
	}
}

int bnx2fc_queuecommand(struct Scsi_Host *host,
			struct scsi_cmnd *sc_cmd)
{
	struct fc_lport *lport = shost_priv(host);
	struct fc_rport *rport = starget_to_rport(scsi_target(sc_cmd->device));
	struct fc_rport_libfc_priv *rp = rport->dd_data;
	struct bnx2fc_rport *tgt;
	struct bnx2fc_cmd *io_req;
	int rc = 0;
	int rval;

	rval = fc_remote_port_chkready(rport);
	if (rval) {
		sc_cmd->result = rval;
		sc_cmd->scsi_done(sc_cmd);
		return 0;
	}

	if ((lport->state != LPORT_ST_READY) || !(lport->link_up)) {
		rc = SCSI_MLQUEUE_HOST_BUSY;
		goto exit_qcmd;
	}

	
	tgt = (struct bnx2fc_rport *)&rp[1];

	if (!test_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags)) {
		rc = SCSI_MLQUEUE_TARGET_BUSY;
		goto exit_qcmd;
	}

	io_req = bnx2fc_cmd_alloc(tgt);
	if (!io_req) {
		rc = SCSI_MLQUEUE_HOST_BUSY;
		goto exit_qcmd;
	}
	io_req->sc_cmd = sc_cmd;

	if (bnx2fc_post_io_req(tgt, io_req)) {
		printk(KERN_ERR PFX "Unable to post io_req\n");
		rc = SCSI_MLQUEUE_HOST_BUSY;
		goto exit_qcmd;
	}
exit_qcmd:
	return rc;
}

void bnx2fc_process_scsi_cmd_compl(struct bnx2fc_cmd *io_req,
				   struct fcoe_task_ctx_entry *task,
				   u8 num_rq)
{
	struct fcoe_fcp_rsp_payload *fcp_rsp;
	struct bnx2fc_rport *tgt = io_req->tgt;
	struct scsi_cmnd *sc_cmd;
	struct Scsi_Host *host;


	

	if (test_and_set_bit(BNX2FC_FLAG_IO_COMPL, &io_req->req_flags)) {
		
		BNX2FC_IO_DBG(io_req, "Timer context finished processing "
			   "this scsi cmd\n");
	}

	
	if (cancel_delayed_work(&io_req->timeout_work))
		kref_put(&io_req->refcount,
			 bnx2fc_cmd_release); 

	sc_cmd = io_req->sc_cmd;
	if (sc_cmd == NULL) {
		printk(KERN_ERR PFX "scsi_cmd_compl - sc_cmd is NULL\n");
		return;
	}

	
	fcp_rsp = (struct fcoe_fcp_rsp_payload *)
		   &(task->rxwr_only.union_ctx.comp_info.fcp_rsp.payload);

	
	bnx2fc_parse_fcp_rsp(io_req, fcp_rsp, num_rq);

	host = sc_cmd->device->host;
	if (!sc_cmd->SCp.ptr) {
		printk(KERN_ERR PFX "SCp.ptr is NULL\n");
		return;
	}

	if (io_req->on_active_queue) {
		list_del_init(&io_req->link);
		io_req->on_active_queue = 0;
		
		list_add_tail(&io_req->link, &tgt->io_retire_queue);
	} else {
		BNX2FC_IO_DBG(io_req, "xid not on active_cmd_queue\n");
		if (io_req->wait_for_comp)
			if (test_and_clear_bit(BNX2FC_FLAG_EH_ABORT,
					       &io_req->req_flags))
				complete(&io_req->tm_done);
	}

	bnx2fc_unmap_sg_list(io_req);
	io_req->sc_cmd = NULL;

	switch (io_req->fcp_status) {
	case FC_GOOD:
		if (io_req->cdb_status == 0) {
			
			sc_cmd->result = DID_OK << 16;
		} else {
			
			BNX2FC_IO_DBG(io_req, "scsi_cmpl: cdb_status = %d"
				 " fcp_resid = 0x%x\n",
				io_req->cdb_status, io_req->fcp_resid);
			sc_cmd->result = (DID_OK << 16) | io_req->cdb_status;
		}
		if (io_req->fcp_resid)
			scsi_set_resid(sc_cmd, io_req->fcp_resid);
		break;
	default:
		printk(KERN_ERR PFX "scsi_cmd_compl: fcp_status = %d\n",
			io_req->fcp_status);
		break;
	}
	sc_cmd->SCp.ptr = NULL;
	sc_cmd->scsi_done(sc_cmd);
	kref_put(&io_req->refcount, bnx2fc_cmd_release);
}

int bnx2fc_post_io_req(struct bnx2fc_rport *tgt,
			       struct bnx2fc_cmd *io_req)
{
	struct fcoe_task_ctx_entry *task;
	struct fcoe_task_ctx_entry *task_page;
	struct scsi_cmnd *sc_cmd = io_req->sc_cmd;
	struct fcoe_port *port = tgt->port;
	struct bnx2fc_interface *interface = port->priv;
	struct bnx2fc_hba *hba = interface->hba;
	struct fc_lport *lport = port->lport;
	struct fcoe_dev_stats *stats;
	int task_idx, index;
	u16 xid;

	
	io_req->cmd_type = BNX2FC_SCSI_CMD;
	io_req->port = port;
	io_req->tgt = tgt;
	io_req->data_xfer_len = scsi_bufflen(sc_cmd);
	sc_cmd->SCp.ptr = (char *)io_req;

	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
	if (sc_cmd->sc_data_direction == DMA_FROM_DEVICE) {
		io_req->io_req_flags = BNX2FC_READ;
		stats->InputRequests++;
		stats->InputBytes += io_req->data_xfer_len;
	} else if (sc_cmd->sc_data_direction == DMA_TO_DEVICE) {
		io_req->io_req_flags = BNX2FC_WRITE;
		stats->OutputRequests++;
		stats->OutputBytes += io_req->data_xfer_len;
	} else {
		io_req->io_req_flags = 0;
		stats->ControlRequests++;
	}
	put_cpu();

	xid = io_req->xid;

	
	if (bnx2fc_build_bd_list_from_sg(io_req)) {
		printk(KERN_ERR PFX "BD list creation failed\n");
		spin_lock_bh(&tgt->tgt_lock);
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		return -EAGAIN;
	}

	task_idx = xid / BNX2FC_TASKS_PER_PAGE;
	index = xid % BNX2FC_TASKS_PER_PAGE;

	
	task_page = (struct fcoe_task_ctx_entry *) hba->task_ctx[task_idx];
	task = &(task_page[index]);
	bnx2fc_init_task(io_req, task);

	spin_lock_bh(&tgt->tgt_lock);

	if (tgt->flush_in_prog) {
		printk(KERN_ERR PFX "Flush in progress..Host Busy\n");
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		return -EAGAIN;
	}

	if (!test_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags)) {
		printk(KERN_ERR PFX "Session not ready...post_io\n");
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		return -EAGAIN;
	}

	
	if (tgt->io_timeout)
		bnx2fc_cmd_timer_set(io_req, BNX2FC_IO_TIMEOUT);
	
	bnx2fc_add_2_sq(tgt, xid);

	

	io_req->on_active_queue = 1;
	
	list_add_tail(&io_req->link, &tgt->active_cmd_queue);

	
	bnx2fc_ring_doorbell(tgt);
	spin_unlock_bh(&tgt->tgt_lock);
	return 0;
}
