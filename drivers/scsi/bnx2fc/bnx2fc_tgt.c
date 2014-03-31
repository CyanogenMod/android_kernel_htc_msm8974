/* bnx2fc_tgt.c: Broadcom NetXtreme II Linux FCoE offload driver.
 * Handles operations such as session offload/upload etc, and manages
 * session resources such as connection id and qp resources.
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
static void bnx2fc_upld_timer(unsigned long data);
static void bnx2fc_ofld_timer(unsigned long data);
static int bnx2fc_init_tgt(struct bnx2fc_rport *tgt,
			   struct fcoe_port *port,
			   struct fc_rport_priv *rdata);
static u32 bnx2fc_alloc_conn_id(struct bnx2fc_hba *hba,
				struct bnx2fc_rport *tgt);
static int bnx2fc_alloc_session_resc(struct bnx2fc_hba *hba,
			      struct bnx2fc_rport *tgt);
static void bnx2fc_free_session_resc(struct bnx2fc_hba *hba,
			      struct bnx2fc_rport *tgt);
static void bnx2fc_free_conn_id(struct bnx2fc_hba *hba, u32 conn_id);

static void bnx2fc_upld_timer(unsigned long data)
{

	struct bnx2fc_rport *tgt = (struct bnx2fc_rport *)data;

	BNX2FC_TGT_DBG(tgt, "upld_timer - Upload compl not received!!\n");
	
	clear_bit(BNX2FC_FLAG_OFFLOADED, &tgt->flags);
	set_bit(BNX2FC_FLAG_UPLD_REQ_COMPL, &tgt->flags);
	wake_up_interruptible(&tgt->upld_wait);
}

static void bnx2fc_ofld_timer(unsigned long data)
{

	struct bnx2fc_rport *tgt = (struct bnx2fc_rport *)data;

	BNX2FC_TGT_DBG(tgt, "entered bnx2fc_ofld_timer\n");
	clear_bit(BNX2FC_FLAG_OFFLOADED, &tgt->flags);
	set_bit(BNX2FC_FLAG_OFLD_REQ_CMPL, &tgt->flags);
	wake_up_interruptible(&tgt->ofld_wait);
}

static void bnx2fc_offload_session(struct fcoe_port *port,
					struct bnx2fc_rport *tgt,
					struct fc_rport_priv *rdata)
{
	struct fc_lport *lport = rdata->local_port;
	struct fc_rport *rport = rdata->rport;
	struct bnx2fc_interface *interface = port->priv;
	struct bnx2fc_hba *hba = interface->hba;
	int rval;
	int i = 0;

	
	
	rval = bnx2fc_init_tgt(tgt, port, rdata);
	if (rval) {
		printk(KERN_ERR PFX "Failed to allocate conn id for "
			"port_id (%6x)\n", rport->port_id);
		goto tgt_init_err;
	}

	
	rval = bnx2fc_alloc_session_resc(hba, tgt);
	if (rval) {
		printk(KERN_ERR PFX "Failed to allocate resources\n");
		goto ofld_err;
	}

retry_ofld:
	clear_bit(BNX2FC_FLAG_OFLD_REQ_CMPL, &tgt->flags);
	rval = bnx2fc_send_session_ofld_req(port, tgt);
	if (rval) {
		printk(KERN_ERR PFX "ofld_req failed\n");
		goto ofld_err;
	}

	setup_timer(&tgt->ofld_timer, bnx2fc_ofld_timer, (unsigned long)tgt);
	mod_timer(&tgt->ofld_timer, jiffies + BNX2FC_FW_TIMEOUT);

	wait_event_interruptible(tgt->ofld_wait,
				 (test_bit(
				  BNX2FC_FLAG_OFLD_REQ_CMPL,
				  &tgt->flags)));
	if (signal_pending(current))
		flush_signals(current);

	del_timer_sync(&tgt->ofld_timer);

	if (!(test_bit(BNX2FC_FLAG_OFFLOADED, &tgt->flags))) {
		if (test_and_clear_bit(BNX2FC_FLAG_CTX_ALLOC_FAILURE,
				       &tgt->flags)) {
			BNX2FC_TGT_DBG(tgt, "ctx_alloc_failure, "
				"retry ofld..%d\n", i++);
			msleep_interruptible(1000);
			if (i > 3) {
				i = 0;
				goto ofld_err;
			}
			goto retry_ofld;
		}
		goto ofld_err;
	}
	if (bnx2fc_map_doorbell(tgt)) {
		printk(KERN_ERR PFX "map doorbell failed - no mem\n");
		
		lport->tt.rport_logoff(rdata);
	}
	return;

ofld_err:
	
	BNX2FC_TGT_DBG(tgt, "bnx2fc_offload_session - offload error\n");
	
	bnx2fc_free_session_resc(hba, tgt);
tgt_init_err:
	if (tgt->fcoe_conn_id != -1)
		bnx2fc_free_conn_id(hba, tgt->fcoe_conn_id);
	lport->tt.rport_logoff(rdata);
}

void bnx2fc_flush_active_ios(struct bnx2fc_rport *tgt)
{
	struct bnx2fc_cmd *io_req;
	struct list_head *list;
	struct list_head *tmp;
	int rc;
	int i = 0;
	BNX2FC_TGT_DBG(tgt, "Entered flush_active_ios - %d\n",
		       tgt->num_active_ios.counter);

	spin_lock_bh(&tgt->tgt_lock);
	tgt->flush_in_prog = 1;

	list_for_each_safe(list, tmp, &tgt->active_cmd_queue) {
		i++;
		io_req = (struct bnx2fc_cmd *)list;
		list_del_init(&io_req->link);
		io_req->on_active_queue = 0;
		BNX2FC_IO_DBG(io_req, "cmd_queue cleanup\n");

		if (cancel_delayed_work(&io_req->timeout_work)) {
			if (test_and_clear_bit(BNX2FC_FLAG_EH_ABORT,
						&io_req->req_flags)) {
				
				BNX2FC_IO_DBG(io_req, "eh_abort for IO "
					      "cleaned up\n");
				complete(&io_req->tm_done);
			}
			kref_put(&io_req->refcount,
				 bnx2fc_cmd_release); 
		}

		set_bit(BNX2FC_FLAG_IO_COMPL, &io_req->req_flags);
		set_bit(BNX2FC_FLAG_IO_CLEANUP, &io_req->req_flags);
		rc = bnx2fc_initiate_cleanup(io_req);
		BUG_ON(rc);
	}

	list_for_each_safe(list, tmp, &tgt->els_queue) {
		i++;
		io_req = (struct bnx2fc_cmd *)list;
		list_del_init(&io_req->link);
		io_req->on_active_queue = 0;

		BNX2FC_IO_DBG(io_req, "els_queue cleanup\n");

		if (cancel_delayed_work(&io_req->timeout_work))
			kref_put(&io_req->refcount,
				 bnx2fc_cmd_release); 

		if ((io_req->cb_func) && (io_req->cb_arg)) {
			io_req->cb_func(io_req->cb_arg);
			io_req->cb_arg = NULL;
		}

		rc = bnx2fc_initiate_cleanup(io_req);
		BUG_ON(rc);
	}

	list_for_each_safe(list, tmp, &tgt->io_retire_queue) {
		i++;
		io_req = (struct bnx2fc_cmd *)list;
		list_del_init(&io_req->link);

		BNX2FC_IO_DBG(io_req, "retire_queue flush\n");

		if (cancel_delayed_work(&io_req->timeout_work))
			kref_put(&io_req->refcount, bnx2fc_cmd_release);

		clear_bit(BNX2FC_FLAG_ISSUE_RRQ, &io_req->req_flags);
	}

	BNX2FC_TGT_DBG(tgt, "IOs flushed = %d\n", i);
	i = 0;
	spin_unlock_bh(&tgt->tgt_lock);
	
	while ((tgt->num_active_ios.counter != 0) && (i++ < BNX2FC_WAIT_CNT))
		msleep(25);
	if (tgt->num_active_ios.counter != 0)
		printk(KERN_ERR PFX "CLEANUP on port 0x%x:"
				    " active_ios = %d\n",
			tgt->rdata->ids.port_id, tgt->num_active_ios.counter);
	spin_lock_bh(&tgt->tgt_lock);
	tgt->flush_in_prog = 0;
	spin_unlock_bh(&tgt->tgt_lock);
}

static void bnx2fc_upload_session(struct fcoe_port *port,
					struct bnx2fc_rport *tgt)
{
	struct bnx2fc_interface *interface = port->priv;
	struct bnx2fc_hba *hba = interface->hba;

	BNX2FC_TGT_DBG(tgt, "upload_session: active_ios = %d\n",
		tgt->num_active_ios.counter);

	clear_bit(BNX2FC_FLAG_UPLD_REQ_COMPL, &tgt->flags);
	bnx2fc_send_session_disable_req(port, tgt);

	setup_timer(&tgt->upld_timer, bnx2fc_upld_timer, (unsigned long)tgt);
	mod_timer(&tgt->upld_timer, jiffies + BNX2FC_FW_TIMEOUT);

	BNX2FC_TGT_DBG(tgt, "waiting for disable compl\n");
	wait_event_interruptible(tgt->upld_wait,
				 (test_bit(
				  BNX2FC_FLAG_UPLD_REQ_COMPL,
				  &tgt->flags)));

	if (signal_pending(current))
		flush_signals(current);

	del_timer_sync(&tgt->upld_timer);

	BNX2FC_TGT_DBG(tgt, "flush/upload - disable wait flags = 0x%lx\n",
		       tgt->flags);
	bnx2fc_flush_active_ios(tgt);

	
	if (test_bit(BNX2FC_FLAG_DISABLED, &tgt->flags)) {
		BNX2FC_TGT_DBG(tgt, "send destroy req\n");
		clear_bit(BNX2FC_FLAG_UPLD_REQ_COMPL, &tgt->flags);
		bnx2fc_send_session_destroy_req(hba, tgt);

		
		setup_timer(&tgt->upld_timer,
			    bnx2fc_upld_timer, (unsigned long)tgt);
		mod_timer(&tgt->upld_timer, jiffies + BNX2FC_FW_TIMEOUT);

		wait_event_interruptible(tgt->upld_wait,
					 (test_bit(
					  BNX2FC_FLAG_UPLD_REQ_COMPL,
					  &tgt->flags)));

		if (!(test_bit(BNX2FC_FLAG_DESTROYED, &tgt->flags)))
			printk(KERN_ERR PFX "ERROR!! destroy timed out\n");

		BNX2FC_TGT_DBG(tgt, "destroy wait complete flags = 0x%lx\n",
			tgt->flags);
		if (signal_pending(current))
			flush_signals(current);

		del_timer_sync(&tgt->upld_timer);

	} else
		printk(KERN_ERR PFX "ERROR!! DISABLE req timed out, destroy"
				" not sent to FW\n");

	
	bnx2fc_free_session_resc(hba, tgt);
	bnx2fc_free_conn_id(hba, tgt->fcoe_conn_id);
}

static int bnx2fc_init_tgt(struct bnx2fc_rport *tgt,
			   struct fcoe_port *port,
			   struct fc_rport_priv *rdata)
{

	struct fc_rport *rport = rdata->rport;
	struct bnx2fc_interface *interface = port->priv;
	struct bnx2fc_hba *hba = interface->hba;
	struct b577xx_doorbell_set_prod *sq_db = &tgt->sq_db;
	struct b577xx_fcoe_rx_doorbell *rx_db = &tgt->rx_db;

	tgt->rport = rport;
	tgt->rdata = rdata;
	tgt->port = port;

	if (hba->num_ofld_sess >= BNX2FC_NUM_MAX_SESS) {
		BNX2FC_TGT_DBG(tgt, "exceeded max sessions. logoff this tgt\n");
		tgt->fcoe_conn_id = -1;
		return -1;
	}

	tgt->fcoe_conn_id = bnx2fc_alloc_conn_id(hba, tgt);
	if (tgt->fcoe_conn_id == -1)
		return -1;

	BNX2FC_TGT_DBG(tgt, "init_tgt - conn_id = 0x%x\n", tgt->fcoe_conn_id);

	tgt->max_sqes = BNX2FC_SQ_WQES_MAX;
	tgt->max_rqes = BNX2FC_RQ_WQES_MAX;
	tgt->max_cqes = BNX2FC_CQ_WQES_MAX;
	atomic_set(&tgt->free_sqes, BNX2FC_SQ_WQES_MAX);

	
	tgt->sq_curr_toggle_bit = 1;
	tgt->cq_curr_toggle_bit = 1;
	tgt->sq_prod_idx = 0;
	tgt->cq_cons_idx = 0;
	tgt->rq_prod_idx = 0x8000;
	tgt->rq_cons_idx = 0;
	atomic_set(&tgt->num_active_ios, 0);

	if (rdata->flags & FC_RP_FLAGS_RETRY) {
		tgt->dev_type = TYPE_TAPE;
		tgt->io_timeout = 0; 
	} else {
		tgt->dev_type = TYPE_DISK;
		tgt->io_timeout = BNX2FC_IO_TIMEOUT;
	}

	
	sq_db->header.header = B577XX_DOORBELL_HDR_DB_TYPE;
	sq_db->header.header |= B577XX_FCOE_CONNECTION_TYPE <<
					B577XX_DOORBELL_HDR_CONN_TYPE_SHIFT;
	
	rx_db->hdr.header = ((0x1 << B577XX_DOORBELL_HDR_RX_SHIFT) |
			  (0x1 << B577XX_DOORBELL_HDR_DB_TYPE_SHIFT) |
			  (B577XX_FCOE_CONNECTION_TYPE <<
				B577XX_DOORBELL_HDR_CONN_TYPE_SHIFT));
	rx_db->params = (0x2 << B577XX_FCOE_RX_DOORBELL_NEGATIVE_ARM_SHIFT) |
		     (0x3 << B577XX_FCOE_RX_DOORBELL_OPCODE_SHIFT);

	spin_lock_init(&tgt->tgt_lock);
	spin_lock_init(&tgt->cq_lock);

	
	INIT_LIST_HEAD(&tgt->active_cmd_queue);

	
	INIT_LIST_HEAD(&tgt->io_retire_queue);

	INIT_LIST_HEAD(&tgt->els_queue);

	
	INIT_LIST_HEAD(&tgt->active_tm_queue);

	init_waitqueue_head(&tgt->ofld_wait);
	init_waitqueue_head(&tgt->upld_wait);

	return 0;
}

void bnx2fc_rport_event_handler(struct fc_lport *lport,
				struct fc_rport_priv *rdata,
				enum fc_rport_event event)
{
	struct fcoe_port *port = lport_priv(lport);
	struct bnx2fc_interface *interface = port->priv;
	struct bnx2fc_hba *hba = interface->hba;
	struct fc_rport *rport = rdata->rport;
	struct fc_rport_libfc_priv *rp;
	struct bnx2fc_rport *tgt;
	u32 port_id;

	BNX2FC_HBA_DBG(lport, "rport_event_hdlr: event = %d, port_id = 0x%x\n",
		event, rdata->ids.port_id);
	switch (event) {
	case RPORT_EV_READY:
		if (!rport) {
			printk(KERN_ERR PFX "rport is NULL: ERROR!\n");
			break;
		}

		rp = rport->dd_data;
		if (rport->port_id == FC_FID_DIR_SERV) {
			printk(KERN_ERR PFX "%x - rport_event_handler ERROR\n",
				rdata->ids.port_id);
			break;
		}

		if (rdata->spp_type != FC_TYPE_FCP) {
			BNX2FC_HBA_DBG(lport, "not FCP type target."
				   " not offloading\n");
			break;
		}
		if (!(rdata->ids.roles & FC_RPORT_ROLE_FCP_TARGET)) {
			BNX2FC_HBA_DBG(lport, "not FCP_TARGET"
				   " not offloading\n");
			break;
		}

		mutex_lock(&hba->hba_mutex);
		tgt = (struct bnx2fc_rport *)&rp[1];

		
		if (test_bit(BNX2FC_FLAG_OFFLOADED, &tgt->flags)) {
			BNX2FC_TGT_DBG(tgt, "already offloaded\n");
			mutex_unlock(&hba->hba_mutex);
			return;
		}

		bnx2fc_offload_session(port, tgt, rdata);

		BNX2FC_TGT_DBG(tgt, "OFFLOAD num_ofld_sess = %d\n",
			hba->num_ofld_sess);

		if (test_bit(BNX2FC_FLAG_OFFLOADED, &tgt->flags)) {
			BNX2FC_TGT_DBG(tgt, "sess offloaded\n");
			
			hba->num_ofld_sess++;

			set_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags);
		} else {
			BNX2FC_TGT_DBG(tgt, "Port is being logged off as "
				   "offloaded flag not set\n");
		}
		mutex_unlock(&hba->hba_mutex);
		break;
	case RPORT_EV_LOGO:
	case RPORT_EV_FAILED:
	case RPORT_EV_STOP:
		port_id = rdata->ids.port_id;
		if (port_id == FC_FID_DIR_SERV)
			break;

		if (!rport) {
			printk(KERN_INFO PFX "%x - rport not created Yet!!\n",
				port_id);
			break;
		}
		rp = rport->dd_data;
		mutex_lock(&hba->hba_mutex);
		tgt = (struct bnx2fc_rport *)&rp[1];

		if (!(test_bit(BNX2FC_FLAG_OFFLOADED, &tgt->flags))) {
			mutex_unlock(&hba->hba_mutex);
			break;
		}
		clear_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags);

		bnx2fc_upload_session(port, tgt);
		hba->num_ofld_sess--;
		BNX2FC_TGT_DBG(tgt, "UPLOAD num_ofld_sess = %d\n",
			hba->num_ofld_sess);
		if ((hba->wait_for_link_down) &&
		    (hba->num_ofld_sess == 0)) {
			wake_up_interruptible(&hba->shutdown_wait);
		}
		if (test_bit(BNX2FC_FLAG_EXPL_LOGO, &tgt->flags)) {
			printk(KERN_ERR PFX "Relogin to the tgt\n");
			mutex_lock(&lport->disc.disc_mutex);
			lport->tt.rport_login(rdata);
			mutex_unlock(&lport->disc.disc_mutex);
		}
		mutex_unlock(&hba->hba_mutex);

		break;

	case RPORT_EV_NONE:
		break;
	}
}

struct bnx2fc_rport *bnx2fc_tgt_lookup(struct fcoe_port *port,
					     u32 port_id)
{
	struct bnx2fc_interface *interface = port->priv;
	struct bnx2fc_hba *hba = interface->hba;
	struct bnx2fc_rport *tgt;
	struct fc_rport_priv *rdata;
	int i;

	for (i = 0; i < BNX2FC_NUM_MAX_SESS; i++) {
		tgt = hba->tgt_ofld_list[i];
		if ((tgt) && (tgt->port == port)) {
			rdata = tgt->rdata;
			if (rdata->ids.port_id == port_id) {
				if (rdata->rp_state != RPORT_ST_DELETE) {
					BNX2FC_TGT_DBG(tgt, "rport "
						"obtained\n");
					return tgt;
				} else {
					BNX2FC_TGT_DBG(tgt, "rport 0x%x "
						"is in DELETED state\n",
						rdata->ids.port_id);
					return NULL;
				}
			}
		}
	}
	return NULL;
}


static u32 bnx2fc_alloc_conn_id(struct bnx2fc_hba *hba,
				struct bnx2fc_rport *tgt)
{
	u32 conn_id, next;

	


	spin_lock_bh(&hba->hba_lock);
	next = hba->next_conn_id;
	conn_id = hba->next_conn_id++;
	if (hba->next_conn_id == BNX2FC_NUM_MAX_SESS)
		hba->next_conn_id = 0;

	while (hba->tgt_ofld_list[conn_id] != NULL) {
		conn_id++;
		if (conn_id == BNX2FC_NUM_MAX_SESS)
			conn_id = 0;

		if (conn_id == next) {
			
			spin_unlock_bh(&hba->hba_lock);
			return -1;
		}
	}
	hba->tgt_ofld_list[conn_id] = tgt;
	tgt->fcoe_conn_id = conn_id;
	spin_unlock_bh(&hba->hba_lock);
	return conn_id;
}

static void bnx2fc_free_conn_id(struct bnx2fc_hba *hba, u32 conn_id)
{
	
	spin_lock_bh(&hba->hba_lock);
	hba->tgt_ofld_list[conn_id] = NULL;
	spin_unlock_bh(&hba->hba_lock);
}

static int bnx2fc_alloc_session_resc(struct bnx2fc_hba *hba,
					struct bnx2fc_rport *tgt)
{
	dma_addr_t page;
	int num_pages;
	u32 *pbl;

	
	tgt->sq_mem_size = tgt->max_sqes * BNX2FC_SQ_WQE_SIZE;
	tgt->sq_mem_size = (tgt->sq_mem_size + (PAGE_SIZE - 1)) & PAGE_MASK;

	tgt->sq = dma_alloc_coherent(&hba->pcidev->dev, tgt->sq_mem_size,
				     &tgt->sq_dma, GFP_KERNEL);
	if (!tgt->sq) {
		printk(KERN_ERR PFX "unable to allocate SQ memory %d\n",
			tgt->sq_mem_size);
		goto mem_alloc_failure;
	}
	memset(tgt->sq, 0, tgt->sq_mem_size);

	
	tgt->cq_mem_size = tgt->max_cqes * BNX2FC_CQ_WQE_SIZE;
	tgt->cq_mem_size = (tgt->cq_mem_size + (PAGE_SIZE - 1)) & PAGE_MASK;

	tgt->cq = dma_alloc_coherent(&hba->pcidev->dev, tgt->cq_mem_size,
				     &tgt->cq_dma, GFP_KERNEL);
	if (!tgt->cq) {
		printk(KERN_ERR PFX "unable to allocate CQ memory %d\n",
			tgt->cq_mem_size);
		goto mem_alloc_failure;
	}
	memset(tgt->cq, 0, tgt->cq_mem_size);

	
	tgt->rq_mem_size = tgt->max_rqes * BNX2FC_RQ_WQE_SIZE;
	tgt->rq_mem_size = (tgt->rq_mem_size + (PAGE_SIZE - 1)) & PAGE_MASK;

	tgt->rq = dma_alloc_coherent(&hba->pcidev->dev, tgt->rq_mem_size,
					&tgt->rq_dma, GFP_KERNEL);
	if (!tgt->rq) {
		printk(KERN_ERR PFX "unable to allocate RQ memory %d\n",
			tgt->rq_mem_size);
		goto mem_alloc_failure;
	}
	memset(tgt->rq, 0, tgt->rq_mem_size);

	tgt->rq_pbl_size = (tgt->rq_mem_size / PAGE_SIZE) * sizeof(void *);
	tgt->rq_pbl_size = (tgt->rq_pbl_size + (PAGE_SIZE - 1)) & PAGE_MASK;

	tgt->rq_pbl = dma_alloc_coherent(&hba->pcidev->dev, tgt->rq_pbl_size,
					 &tgt->rq_pbl_dma, GFP_KERNEL);
	if (!tgt->rq_pbl) {
		printk(KERN_ERR PFX "unable to allocate RQ PBL %d\n",
			tgt->rq_pbl_size);
		goto mem_alloc_failure;
	}

	memset(tgt->rq_pbl, 0, tgt->rq_pbl_size);
	num_pages = tgt->rq_mem_size / PAGE_SIZE;
	page = tgt->rq_dma;
	pbl = (u32 *)tgt->rq_pbl;

	while (num_pages--) {
		*pbl = (u32)page;
		pbl++;
		*pbl = (u32)((u64)page >> 32);
		pbl++;
		page += PAGE_SIZE;
	}

	
	tgt->xferq_mem_size = tgt->max_sqes * BNX2FC_XFERQ_WQE_SIZE;
	tgt->xferq_mem_size = (tgt->xferq_mem_size + (PAGE_SIZE - 1)) &
			       PAGE_MASK;

	tgt->xferq = dma_alloc_coherent(&hba->pcidev->dev, tgt->xferq_mem_size,
					&tgt->xferq_dma, GFP_KERNEL);
	if (!tgt->xferq) {
		printk(KERN_ERR PFX "unable to allocate XFERQ %d\n",
			tgt->xferq_mem_size);
		goto mem_alloc_failure;
	}
	memset(tgt->xferq, 0, tgt->xferq_mem_size);

	
	tgt->confq_mem_size = tgt->max_sqes * BNX2FC_CONFQ_WQE_SIZE;
	tgt->confq_mem_size = (tgt->confq_mem_size + (PAGE_SIZE - 1)) &
			       PAGE_MASK;

	tgt->confq = dma_alloc_coherent(&hba->pcidev->dev, tgt->confq_mem_size,
					&tgt->confq_dma, GFP_KERNEL);
	if (!tgt->confq) {
		printk(KERN_ERR PFX "unable to allocate CONFQ %d\n",
			tgt->confq_mem_size);
		goto mem_alloc_failure;
	}
	memset(tgt->confq, 0, tgt->confq_mem_size);

	tgt->confq_pbl_size =
		(tgt->confq_mem_size / PAGE_SIZE) * sizeof(void *);
	tgt->confq_pbl_size =
		(tgt->confq_pbl_size + (PAGE_SIZE - 1)) & PAGE_MASK;

	tgt->confq_pbl = dma_alloc_coherent(&hba->pcidev->dev,
					    tgt->confq_pbl_size,
					    &tgt->confq_pbl_dma, GFP_KERNEL);
	if (!tgt->confq_pbl) {
		printk(KERN_ERR PFX "unable to allocate CONFQ PBL %d\n",
			tgt->confq_pbl_size);
		goto mem_alloc_failure;
	}

	memset(tgt->confq_pbl, 0, tgt->confq_pbl_size);
	num_pages = tgt->confq_mem_size / PAGE_SIZE;
	page = tgt->confq_dma;
	pbl = (u32 *)tgt->confq_pbl;

	while (num_pages--) {
		*pbl = (u32)page;
		pbl++;
		*pbl = (u32)((u64)page >> 32);
		pbl++;
		page += PAGE_SIZE;
	}

	
	tgt->conn_db_mem_size = sizeof(struct fcoe_conn_db);

	tgt->conn_db = dma_alloc_coherent(&hba->pcidev->dev,
					  tgt->conn_db_mem_size,
					  &tgt->conn_db_dma, GFP_KERNEL);
	if (!tgt->conn_db) {
		printk(KERN_ERR PFX "unable to allocate conn_db %d\n",
						tgt->conn_db_mem_size);
		goto mem_alloc_failure;
	}
	memset(tgt->conn_db, 0, tgt->conn_db_mem_size);


	
	tgt->lcq_mem_size = (tgt->max_sqes + 8) * BNX2FC_SQ_WQE_SIZE;
	tgt->lcq_mem_size = (tgt->lcq_mem_size + (PAGE_SIZE - 1)) &
			     PAGE_MASK;

	tgt->lcq = dma_alloc_coherent(&hba->pcidev->dev, tgt->lcq_mem_size,
				      &tgt->lcq_dma, GFP_KERNEL);

	if (!tgt->lcq) {
		printk(KERN_ERR PFX "unable to allocate lcq %d\n",
		       tgt->lcq_mem_size);
		goto mem_alloc_failure;
	}
	memset(tgt->lcq, 0, tgt->lcq_mem_size);

	tgt->conn_db->rq_prod = 0x8000;

	return 0;

mem_alloc_failure:
	return -ENOMEM;
}

static void bnx2fc_free_session_resc(struct bnx2fc_hba *hba,
						struct bnx2fc_rport *tgt)
{
	void __iomem *ctx_base_ptr;

	BNX2FC_TGT_DBG(tgt, "Freeing up session resources\n");

	spin_lock_bh(&tgt->cq_lock);
	ctx_base_ptr = tgt->ctx_base;
	tgt->ctx_base = NULL;

	
	if (tgt->lcq) {
		dma_free_coherent(&hba->pcidev->dev, tgt->lcq_mem_size,
				    tgt->lcq, tgt->lcq_dma);
		tgt->lcq = NULL;
	}
	
	if (tgt->conn_db) {
		dma_free_coherent(&hba->pcidev->dev, tgt->conn_db_mem_size,
				    tgt->conn_db, tgt->conn_db_dma);
		tgt->conn_db = NULL;
	}
	
	if (tgt->confq_pbl) {
		dma_free_coherent(&hba->pcidev->dev, tgt->confq_pbl_size,
				    tgt->confq_pbl, tgt->confq_pbl_dma);
		tgt->confq_pbl = NULL;
	}
	if (tgt->confq) {
		dma_free_coherent(&hba->pcidev->dev, tgt->confq_mem_size,
				    tgt->confq, tgt->confq_dma);
		tgt->confq = NULL;
	}
	
	if (tgt->xferq) {
		dma_free_coherent(&hba->pcidev->dev, tgt->xferq_mem_size,
				    tgt->xferq, tgt->xferq_dma);
		tgt->xferq = NULL;
	}
	
	if (tgt->rq_pbl) {
		dma_free_coherent(&hba->pcidev->dev, tgt->rq_pbl_size,
				    tgt->rq_pbl, tgt->rq_pbl_dma);
		tgt->rq_pbl = NULL;
	}
	if (tgt->rq) {
		dma_free_coherent(&hba->pcidev->dev, tgt->rq_mem_size,
				    tgt->rq, tgt->rq_dma);
		tgt->rq = NULL;
	}
	
	if (tgt->cq) {
		dma_free_coherent(&hba->pcidev->dev, tgt->cq_mem_size,
				    tgt->cq, tgt->cq_dma);
		tgt->cq = NULL;
	}
	
	if (tgt->sq) {
		dma_free_coherent(&hba->pcidev->dev, tgt->sq_mem_size,
				    tgt->sq, tgt->sq_dma);
		tgt->sq = NULL;
	}
	spin_unlock_bh(&tgt->cq_lock);

	if (ctx_base_ptr)
		iounmap(ctx_base_ptr);
}
