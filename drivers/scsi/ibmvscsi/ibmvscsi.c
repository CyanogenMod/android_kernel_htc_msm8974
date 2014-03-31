/* ------------------------------------------------------------
 * ibmvscsi.c
 * (C) Copyright IBM Corporation 1994, 2004
 * Authors: Colin DeVilbiss (devilbis@us.ibm.com)
 *          Santiago Leon (santil@us.ibm.com)
 *          Dave Boutcher (sleddog@us.ibm.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 * ------------------------------------------------------------
 * Emulation of a SCSI host adapter for Virtual I/O devices
 *
 * This driver supports the SCSI adapter implemented by the IBM
 * Power5 firmware.  That SCSI adapter is not a physical adapter,
 * but allows Linux SCSI peripheral drivers to directly
 * access devices in another logical partition on the physical system.
 *
 * The virtual adapter(s) are present in the open firmware device
 * tree just like real adapters.
 *
 * One of the capabilities provided on these systems is the ability
 * to DMA between partitions.  The architecture states that for VSCSI,
 * the server side is allowed to DMA to and from the client.  The client
 * is never trusted to DMA to or from the server directly.
 *
 * Messages are sent between partitions on a "Command/Response Queue" 
 * (CRQ), which is just a buffer of 16 byte entries in the receiver's 
 * Senders cannot access the buffer directly, but send messages by
 * making a hypervisor call and passing in the 16 bytes.  The hypervisor
 * puts the message in the next 16 byte space in round-robin fashion,
 * turns on the high order bit of the message (the valid bit), and 
 * generates an interrupt to the receiver (if interrupts are turned on.) 
 * The receiver just turns off the valid bit when they have copied out
 * the message.
 *
 * The VSCSI client builds a SCSI Remote Protocol (SRP) Information Unit
 * (IU) (as defined in the T10 standard available at www.t10.org), gets 
 * a DMA address for the message, and sends it to the server as the
 * payload of a CRQ message.  The server DMAs the SRP IU and processes it,
 * including doing any additional data transfers.  When it is done, it
 * DMAs the SRP response back to the same address as the request came from,
 * and sends a CRQ message back to inform the client that the request has
 * completed.
 *
 * TODO: This is currently pretty tied to the IBM pSeries hypervisor
 * interfaces.  It would be really nice to abstract this above an RDMA
 * layer.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/kthread.h>
#include <asm/firmware.h>
#include <asm/vio.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_transport_srp.h>
#include "ibmvscsi.h"

static int max_id = 64;
static int max_channel = 3;
static int init_timeout = 300;
static int login_timeout = 60;
static int info_timeout = 30;
static int abort_timeout = 60;
static int reset_timeout = 60;
static int max_requests = IBMVSCSI_MAX_REQUESTS_DEFAULT;
static int max_events = IBMVSCSI_MAX_REQUESTS_DEFAULT + 2;
static int fast_fail = 1;
static int client_reserve = 1;

static struct scsi_transport_template *ibmvscsi_transport_template;

#define IBMVSCSI_VERSION "1.5.9"

static struct ibmvscsi_ops *ibmvscsi_ops;

MODULE_DESCRIPTION("IBM Virtual SCSI");
MODULE_AUTHOR("Dave Boutcher");
MODULE_LICENSE("GPL");
MODULE_VERSION(IBMVSCSI_VERSION);

module_param_named(max_id, max_id, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_id, "Largest ID value for each channel");
module_param_named(max_channel, max_channel, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_channel, "Largest channel value");
module_param_named(init_timeout, init_timeout, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(init_timeout, "Initialization timeout in seconds");
module_param_named(max_requests, max_requests, int, S_IRUGO);
MODULE_PARM_DESC(max_requests, "Maximum requests for this adapter");
module_param_named(fast_fail, fast_fail, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fast_fail, "Enable fast fail. [Default=1]");
module_param_named(client_reserve, client_reserve, int, S_IRUGO );
MODULE_PARM_DESC(client_reserve, "Attempt client managed reserve/release");

static int initialize_event_pool(struct event_pool *pool,
				 int size, struct ibmvscsi_host_data *hostdata)
{
	int i;

	pool->size = size;
	pool->next = 0;
	pool->events = kcalloc(pool->size, sizeof(*pool->events), GFP_KERNEL);
	if (!pool->events)
		return -ENOMEM;

	pool->iu_storage =
	    dma_alloc_coherent(hostdata->dev,
			       pool->size * sizeof(*pool->iu_storage),
			       &pool->iu_token, 0);
	if (!pool->iu_storage) {
		kfree(pool->events);
		return -ENOMEM;
	}

	for (i = 0; i < pool->size; ++i) {
		struct srp_event_struct *evt = &pool->events[i];
		memset(&evt->crq, 0x00, sizeof(evt->crq));
		atomic_set(&evt->free, 1);
		evt->crq.valid = 0x80;
		evt->crq.IU_length = sizeof(*evt->xfer_iu);
		evt->crq.IU_data_ptr = pool->iu_token + 
			sizeof(*evt->xfer_iu) * i;
		evt->xfer_iu = pool->iu_storage + i;
		evt->hostdata = hostdata;
		evt->ext_list = NULL;
		evt->ext_list_token = 0;
	}

	return 0;
}

static void release_event_pool(struct event_pool *pool,
			       struct ibmvscsi_host_data *hostdata)
{
	int i, in_use = 0;
	for (i = 0; i < pool->size; ++i) {
		if (atomic_read(&pool->events[i].free) != 1)
			++in_use;
		if (pool->events[i].ext_list) {
			dma_free_coherent(hostdata->dev,
				  SG_ALL * sizeof(struct srp_direct_buf),
				  pool->events[i].ext_list,
				  pool->events[i].ext_list_token);
		}
	}
	if (in_use)
		dev_warn(hostdata->dev, "releasing event pool with %d "
			 "events still in use?\n", in_use);
	kfree(pool->events);
	dma_free_coherent(hostdata->dev,
			  pool->size * sizeof(*pool->iu_storage),
			  pool->iu_storage, pool->iu_token);
}

static int valid_event_struct(struct event_pool *pool,
				struct srp_event_struct *evt)
{
	int index = evt - pool->events;
	if (index < 0 || index >= pool->size)	
		return 0;
	if (evt != pool->events + index)	
		return 0;
	return 1;
}

static void free_event_struct(struct event_pool *pool,
				       struct srp_event_struct *evt)
{
	if (!valid_event_struct(pool, evt)) {
		dev_err(evt->hostdata->dev, "Freeing invalid event_struct %p "
			"(not in pool %p)\n", evt, pool->events);
		return;
	}
	if (atomic_inc_return(&evt->free) != 1) {
		dev_err(evt->hostdata->dev, "Freeing event_struct %p "
			"which is not in use!\n", evt);
		return;
	}
}

static struct srp_event_struct *get_event_struct(struct event_pool *pool)
{
	int i;
	int poolsize = pool->size;
	int offset = pool->next;

	for (i = 0; i < poolsize; i++) {
		offset = (offset + 1) % poolsize;
		if (!atomic_dec_if_positive(&pool->events[offset].free)) {
			pool->next = offset;
			return &pool->events[offset];
		}
	}

	printk(KERN_ERR "ibmvscsi: found no event struct in pool!\n");
	return NULL;
}

static void init_event_struct(struct srp_event_struct *evt_struct,
			      void (*done) (struct srp_event_struct *),
			      u8 format,
			      int timeout)
{
	evt_struct->cmnd = NULL;
	evt_struct->cmnd_done = NULL;
	evt_struct->sync_srp = NULL;
	evt_struct->crq.format = format;
	evt_struct->crq.timeout = timeout;
	evt_struct->done = done;
}


static void set_srp_direction(struct scsi_cmnd *cmd,
			      struct srp_cmd *srp_cmd, 
			      int numbuf)
{
	u8 fmt;

	if (numbuf == 0)
		return;
	
	if (numbuf == 1)
		fmt = SRP_DATA_DESC_DIRECT;
	else {
		fmt = SRP_DATA_DESC_INDIRECT;
		numbuf = min(numbuf, MAX_INDIRECT_BUFS);

		if (cmd->sc_data_direction == DMA_TO_DEVICE)
			srp_cmd->data_out_desc_cnt = numbuf;
		else
			srp_cmd->data_in_desc_cnt = numbuf;
	}

	if (cmd->sc_data_direction == DMA_TO_DEVICE)
		srp_cmd->buf_fmt = fmt << 4;
	else
		srp_cmd->buf_fmt = fmt;
}

static void unmap_cmd_data(struct srp_cmd *cmd,
			   struct srp_event_struct *evt_struct,
			   struct device *dev)
{
	u8 out_fmt, in_fmt;

	out_fmt = cmd->buf_fmt >> 4;
	in_fmt = cmd->buf_fmt & ((1U << 4) - 1);

	if (out_fmt == SRP_NO_DATA_DESC && in_fmt == SRP_NO_DATA_DESC)
		return;

	if (evt_struct->cmnd)
		scsi_dma_unmap(evt_struct->cmnd);
}

static int map_sg_list(struct scsi_cmnd *cmd, int nseg,
		       struct srp_direct_buf *md)
{
	int i;
	struct scatterlist *sg;
	u64 total_length = 0;

	scsi_for_each_sg(cmd, sg, nseg, i) {
		struct srp_direct_buf *descr = md + i;
		descr->va = sg_dma_address(sg);
		descr->len = sg_dma_len(sg);
		descr->key = 0;
		total_length += sg_dma_len(sg);
 	}
	return total_length;
}

static int map_sg_data(struct scsi_cmnd *cmd,
		       struct srp_event_struct *evt_struct,
		       struct srp_cmd *srp_cmd, struct device *dev)
{

	int sg_mapped;
	u64 total_length = 0;
	struct srp_direct_buf *data =
		(struct srp_direct_buf *) srp_cmd->add_data;
	struct srp_indirect_buf *indirect =
		(struct srp_indirect_buf *) data;

	sg_mapped = scsi_dma_map(cmd);
	if (!sg_mapped)
		return 1;
	else if (sg_mapped < 0)
		return 0;

	set_srp_direction(cmd, srp_cmd, sg_mapped);

	
	if (sg_mapped == 1) {
		map_sg_list(cmd, sg_mapped, data);
		return 1;
	}

	indirect->table_desc.va = 0;
	indirect->table_desc.len = sg_mapped * sizeof(struct srp_direct_buf);
	indirect->table_desc.key = 0;

	if (sg_mapped <= MAX_INDIRECT_BUFS) {
		total_length = map_sg_list(cmd, sg_mapped,
					   &indirect->desc_list[0]);
		indirect->len = total_length;
		return 1;
	}

	
	if (!evt_struct->ext_list) {
		evt_struct->ext_list = (struct srp_direct_buf *)
			dma_alloc_coherent(dev,
					   SG_ALL * sizeof(struct srp_direct_buf),
					   &evt_struct->ext_list_token, 0);
		if (!evt_struct->ext_list) {
			if (!firmware_has_feature(FW_FEATURE_CMO))
				sdev_printk(KERN_ERR, cmd->device,
				            "Can't allocate memory "
				            "for indirect table\n");
			scsi_dma_unmap(cmd);
			return 0;
		}
	}

	total_length = map_sg_list(cmd, sg_mapped, evt_struct->ext_list);

	indirect->len = total_length;
	indirect->table_desc.va = evt_struct->ext_list_token;
	indirect->table_desc.len = sg_mapped * sizeof(indirect->desc_list[0]);
	memcpy(indirect->desc_list, evt_struct->ext_list,
	       MAX_INDIRECT_BUFS * sizeof(struct srp_direct_buf));
 	return 1;
}

static int map_data_for_srp_cmd(struct scsi_cmnd *cmd,
				struct srp_event_struct *evt_struct,
				struct srp_cmd *srp_cmd, struct device *dev)
{
	switch (cmd->sc_data_direction) {
	case DMA_FROM_DEVICE:
	case DMA_TO_DEVICE:
		break;
	case DMA_NONE:
		return 1;
	case DMA_BIDIRECTIONAL:
		sdev_printk(KERN_ERR, cmd->device,
			    "Can't map DMA_BIDIRECTIONAL to read/write\n");
		return 0;
	default:
		sdev_printk(KERN_ERR, cmd->device,
			    "Unknown data direction 0x%02x; can't map!\n",
			    cmd->sc_data_direction);
		return 0;
	}

	return map_sg_data(cmd, evt_struct, srp_cmd, dev);
}

static void purge_requests(struct ibmvscsi_host_data *hostdata, int error_code)
{
	struct srp_event_struct *evt;
	unsigned long flags;

	spin_lock_irqsave(hostdata->host->host_lock, flags);
	while (!list_empty(&hostdata->sent)) {
		evt = list_first_entry(&hostdata->sent, struct srp_event_struct, list);
		list_del(&evt->list);
		del_timer(&evt->timer);

		spin_unlock_irqrestore(hostdata->host->host_lock, flags);
		if (evt->cmnd) {
			evt->cmnd->result = (error_code << 16);
			unmap_cmd_data(&evt->iu.srp.cmd, evt,
				       evt->hostdata->dev);
			if (evt->cmnd_done)
				evt->cmnd_done(evt->cmnd);
		} else if (evt->done)
			evt->done(evt);
		free_event_struct(&evt->hostdata->pool, evt);
		spin_lock_irqsave(hostdata->host->host_lock, flags);
	}
	spin_unlock_irqrestore(hostdata->host->host_lock, flags);
}

static void ibmvscsi_reset_host(struct ibmvscsi_host_data *hostdata)
{
	scsi_block_requests(hostdata->host);
	atomic_set(&hostdata->request_limit, 0);

	purge_requests(hostdata, DID_ERROR);
	hostdata->reset_crq = 1;
	wake_up(&hostdata->work_wait_q);
}

static void ibmvscsi_timeout(struct srp_event_struct *evt_struct)
{
	struct ibmvscsi_host_data *hostdata = evt_struct->hostdata;

	dev_err(hostdata->dev, "Command timed out (%x). Resetting connection\n",
		evt_struct->iu.srp.cmd.opcode);

	ibmvscsi_reset_host(hostdata);
}


static int ibmvscsi_send_srp_event(struct srp_event_struct *evt_struct,
				   struct ibmvscsi_host_data *hostdata,
				   unsigned long timeout)
{
	u64 *crq_as_u64 = (u64 *) &evt_struct->crq;
	int request_status = 0;
	int rc;
	int srp_req = 0;

	if (evt_struct->crq.format == VIOSRP_SRP_FORMAT) {
		srp_req = 1;
		request_status =
			atomic_dec_if_positive(&hostdata->request_limit);
		if (request_status < -1)
			goto send_error;
		
		else if (request_status == -1 &&
		         evt_struct->iu.srp.login_req.opcode != SRP_LOGIN_REQ)
			goto send_busy;
		else if (request_status < 2 &&
		         evt_struct->iu.srp.cmd.opcode != SRP_TSK_MGMT) {
			int server_limit = request_status;
			struct srp_event_struct *tmp_evt;

			list_for_each_entry(tmp_evt, &hostdata->sent, list) {
				server_limit++;
			}

			if (server_limit > 2)
				goto send_busy;
		}
	}

	
	*evt_struct->xfer_iu = evt_struct->iu;
	evt_struct->xfer_iu->srp.rsp.tag = (u64)evt_struct;

	list_add_tail(&evt_struct->list, &hostdata->sent);

	init_timer(&evt_struct->timer);
	if (timeout) {
		evt_struct->timer.data = (unsigned long) evt_struct;
		evt_struct->timer.expires = jiffies + (timeout * HZ);
		evt_struct->timer.function = (void (*)(unsigned long))ibmvscsi_timeout;
		add_timer(&evt_struct->timer);
	}

	if ((rc =
	     ibmvscsi_ops->send_crq(hostdata, crq_as_u64[0], crq_as_u64[1])) != 0) {
		list_del(&evt_struct->list);
		del_timer(&evt_struct->timer);

		if (rc == H_CLOSED) {
			dev_warn(hostdata->dev, "send warning. "
			         "Receive queue closed, will retry.\n");
			goto send_busy;
		}
		dev_err(hostdata->dev, "send error %d\n", rc);
		if (srp_req)
			atomic_inc(&hostdata->request_limit);
		goto send_error;
	}

	return 0;

 send_busy:
	unmap_cmd_data(&evt_struct->iu.srp.cmd, evt_struct, hostdata->dev);

	free_event_struct(&hostdata->pool, evt_struct);
	if (srp_req && request_status != -1)
		atomic_inc(&hostdata->request_limit);
	return SCSI_MLQUEUE_HOST_BUSY;

 send_error:
	unmap_cmd_data(&evt_struct->iu.srp.cmd, evt_struct, hostdata->dev);

	if (evt_struct->cmnd != NULL) {
		evt_struct->cmnd->result = DID_ERROR << 16;
		evt_struct->cmnd_done(evt_struct->cmnd);
	} else if (evt_struct->done)
		evt_struct->done(evt_struct);

	free_event_struct(&hostdata->pool, evt_struct);
	return 0;
}

static void handle_cmd_rsp(struct srp_event_struct *evt_struct)
{
	struct srp_rsp *rsp = &evt_struct->xfer_iu->srp.rsp;
	struct scsi_cmnd *cmnd = evt_struct->cmnd;

	if (unlikely(rsp->opcode != SRP_RSP)) {
		if (printk_ratelimit())
			dev_warn(evt_struct->hostdata->dev,
				 "bad SRP RSP type %d\n", rsp->opcode);
	}
	
	if (cmnd) {
		cmnd->result |= rsp->status;
		if (((cmnd->result >> 1) & 0x1f) == CHECK_CONDITION)
			memcpy(cmnd->sense_buffer,
			       rsp->data,
			       rsp->sense_data_len);
		unmap_cmd_data(&evt_struct->iu.srp.cmd, 
			       evt_struct, 
			       evt_struct->hostdata->dev);

		if (rsp->flags & SRP_RSP_FLAG_DOOVER)
			scsi_set_resid(cmnd, rsp->data_out_res_cnt);
		else if (rsp->flags & SRP_RSP_FLAG_DIOVER)
			scsi_set_resid(cmnd, rsp->data_in_res_cnt);
	}

	if (evt_struct->cmnd_done)
		evt_struct->cmnd_done(cmnd);
}

static inline u16 lun_from_dev(struct scsi_device *dev)
{
	return (0x2 << 14) | (dev->id << 8) | (dev->channel << 5) | dev->lun;
}

static int ibmvscsi_queuecommand_lck(struct scsi_cmnd *cmnd,
				 void (*done) (struct scsi_cmnd *))
{
	struct srp_cmd *srp_cmd;
	struct srp_event_struct *evt_struct;
	struct srp_indirect_buf *indirect;
	struct ibmvscsi_host_data *hostdata = shost_priv(cmnd->device->host);
	u16 lun = lun_from_dev(cmnd->device);
	u8 out_fmt, in_fmt;

	cmnd->result = (DID_OK << 16);
	evt_struct = get_event_struct(&hostdata->pool);
	if (!evt_struct)
		return SCSI_MLQUEUE_HOST_BUSY;

	
	srp_cmd = &evt_struct->iu.srp.cmd;
	memset(srp_cmd, 0x00, SRP_MAX_IU_LEN);
	srp_cmd->opcode = SRP_CMD;
	memcpy(srp_cmd->cdb, cmnd->cmnd, sizeof(srp_cmd->cdb));
	srp_cmd->lun = ((u64) lun) << 48;

	if (!map_data_for_srp_cmd(cmnd, evt_struct, srp_cmd, hostdata->dev)) {
		if (!firmware_has_feature(FW_FEATURE_CMO))
			sdev_printk(KERN_ERR, cmnd->device,
			            "couldn't convert cmd to srp_cmd\n");
		free_event_struct(&hostdata->pool, evt_struct);
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	init_event_struct(evt_struct,
			  handle_cmd_rsp,
			  VIOSRP_SRP_FORMAT,
			  cmnd->request->timeout/HZ);

	evt_struct->cmnd = cmnd;
	evt_struct->cmnd_done = done;

	
	indirect = (struct srp_indirect_buf *) srp_cmd->add_data;
	out_fmt = srp_cmd->buf_fmt >> 4;
	in_fmt = srp_cmd->buf_fmt & ((1U << 4) - 1);
	if ((in_fmt == SRP_DATA_DESC_INDIRECT ||
	     out_fmt == SRP_DATA_DESC_INDIRECT) &&
	    indirect->table_desc.va == 0) {
		indirect->table_desc.va = evt_struct->crq.IU_data_ptr +
			offsetof(struct srp_cmd, add_data) +
			offsetof(struct srp_indirect_buf, desc_list);
	}

	return ibmvscsi_send_srp_event(evt_struct, hostdata, 0);
}

static DEF_SCSI_QCMD(ibmvscsi_queuecommand)


static int map_persist_bufs(struct ibmvscsi_host_data *hostdata)
{

	hostdata->caps_addr = dma_map_single(hostdata->dev, &hostdata->caps,
					     sizeof(hostdata->caps), DMA_BIDIRECTIONAL);

	if (dma_mapping_error(hostdata->dev, hostdata->caps_addr)) {
		dev_err(hostdata->dev, "Unable to map capabilities buffer!\n");
		return 1;
	}

	hostdata->adapter_info_addr = dma_map_single(hostdata->dev,
						     &hostdata->madapter_info,
						     sizeof(hostdata->madapter_info),
						     DMA_BIDIRECTIONAL);
	if (dma_mapping_error(hostdata->dev, hostdata->adapter_info_addr)) {
		dev_err(hostdata->dev, "Unable to map adapter info buffer!\n");
		dma_unmap_single(hostdata->dev, hostdata->caps_addr,
				 sizeof(hostdata->caps), DMA_BIDIRECTIONAL);
		return 1;
	}

	return 0;
}

static void unmap_persist_bufs(struct ibmvscsi_host_data *hostdata)
{
	dma_unmap_single(hostdata->dev, hostdata->caps_addr,
			 sizeof(hostdata->caps), DMA_BIDIRECTIONAL);

	dma_unmap_single(hostdata->dev, hostdata->adapter_info_addr,
			 sizeof(hostdata->madapter_info), DMA_BIDIRECTIONAL);
}

static void login_rsp(struct srp_event_struct *evt_struct)
{
	struct ibmvscsi_host_data *hostdata = evt_struct->hostdata;
	switch (evt_struct->xfer_iu->srp.login_rsp.opcode) {
	case SRP_LOGIN_RSP:	
		break;
	case SRP_LOGIN_REJ:	
		dev_info(hostdata->dev, "SRP_LOGIN_REJ reason %u\n",
			 evt_struct->xfer_iu->srp.login_rej.reason);
		
		atomic_set(&hostdata->request_limit, -1);
		return;
	default:
		dev_err(hostdata->dev, "Invalid login response typecode 0x%02x!\n",
			evt_struct->xfer_iu->srp.login_rsp.opcode);
		
		atomic_set(&hostdata->request_limit, -1);
		return;
	}

	dev_info(hostdata->dev, "SRP_LOGIN succeeded\n");
	hostdata->client_migrated = 0;

	atomic_set(&hostdata->request_limit,
		   evt_struct->xfer_iu->srp.login_rsp.req_lim_delta);

	
	scsi_unblock_requests(hostdata->host);
}

static int send_srp_login(struct ibmvscsi_host_data *hostdata)
{
	int rc;
	unsigned long flags;
	struct srp_login_req *login;
	struct srp_event_struct *evt_struct = get_event_struct(&hostdata->pool);

	BUG_ON(!evt_struct);
	init_event_struct(evt_struct, login_rsp,
			  VIOSRP_SRP_FORMAT, login_timeout);

	login = &evt_struct->iu.srp.login_req;
	memset(login, 0, sizeof(*login));
	login->opcode = SRP_LOGIN_REQ;
	login->req_it_iu_len = sizeof(union srp_iu);
	login->req_buf_fmt = SRP_BUF_FORMAT_DIRECT | SRP_BUF_FORMAT_INDIRECT;

	spin_lock_irqsave(hostdata->host->host_lock, flags);
	atomic_set(&hostdata->request_limit, 0);

	rc = ibmvscsi_send_srp_event(evt_struct, hostdata, login_timeout * 2);
	spin_unlock_irqrestore(hostdata->host->host_lock, flags);
	dev_info(hostdata->dev, "sent SRP login\n");
	return rc;
};

static void capabilities_rsp(struct srp_event_struct *evt_struct)
{
	struct ibmvscsi_host_data *hostdata = evt_struct->hostdata;

	if (evt_struct->xfer_iu->mad.capabilities.common.status) {
		dev_err(hostdata->dev, "error 0x%X getting capabilities info\n",
			evt_struct->xfer_iu->mad.capabilities.common.status);
	} else {
		if (hostdata->caps.migration.common.server_support != SERVER_SUPPORTS_CAP)
			dev_info(hostdata->dev, "Partition migration not supported\n");

		if (client_reserve) {
			if (hostdata->caps.reserve.common.server_support ==
			    SERVER_SUPPORTS_CAP)
				dev_info(hostdata->dev, "Client reserve enabled\n");
			else
				dev_info(hostdata->dev, "Client reserve not supported\n");
		}
	}

	send_srp_login(hostdata);
}

static void send_mad_capabilities(struct ibmvscsi_host_data *hostdata)
{
	struct viosrp_capabilities *req;
	struct srp_event_struct *evt_struct;
	unsigned long flags;
	struct device_node *of_node = hostdata->dev->of_node;
	const char *location;

	evt_struct = get_event_struct(&hostdata->pool);
	BUG_ON(!evt_struct);

	init_event_struct(evt_struct, capabilities_rsp,
			  VIOSRP_MAD_FORMAT, info_timeout);

	req = &evt_struct->iu.mad.capabilities;
	memset(req, 0, sizeof(*req));

	hostdata->caps.flags = CAP_LIST_SUPPORTED;
	if (hostdata->client_migrated)
		hostdata->caps.flags |= CLIENT_MIGRATED;

	strncpy(hostdata->caps.name, dev_name(&hostdata->host->shost_gendev),
		sizeof(hostdata->caps.name));
	hostdata->caps.name[sizeof(hostdata->caps.name) - 1] = '\0';

	location = of_get_property(of_node, "ibm,loc-code", NULL);
	location = location ? location : dev_name(hostdata->dev);
	strncpy(hostdata->caps.loc, location, sizeof(hostdata->caps.loc));
	hostdata->caps.loc[sizeof(hostdata->caps.loc) - 1] = '\0';

	req->common.type = VIOSRP_CAPABILITIES_TYPE;
	req->buffer = hostdata->caps_addr;

	hostdata->caps.migration.common.cap_type = MIGRATION_CAPABILITIES;
	hostdata->caps.migration.common.length = sizeof(hostdata->caps.migration);
	hostdata->caps.migration.common.server_support = SERVER_SUPPORTS_CAP;
	hostdata->caps.migration.ecl = 1;

	if (client_reserve) {
		hostdata->caps.reserve.common.cap_type = RESERVATION_CAPABILITIES;
		hostdata->caps.reserve.common.length = sizeof(hostdata->caps.reserve);
		hostdata->caps.reserve.common.server_support = SERVER_SUPPORTS_CAP;
		hostdata->caps.reserve.type = CLIENT_RESERVE_SCSI_2;
		req->common.length = sizeof(hostdata->caps);
	} else
		req->common.length = sizeof(hostdata->caps) - sizeof(hostdata->caps.reserve);

	spin_lock_irqsave(hostdata->host->host_lock, flags);
	if (ibmvscsi_send_srp_event(evt_struct, hostdata, info_timeout * 2))
		dev_err(hostdata->dev, "couldn't send CAPABILITIES_REQ!\n");
	spin_unlock_irqrestore(hostdata->host->host_lock, flags);
};

static void fast_fail_rsp(struct srp_event_struct *evt_struct)
{
	struct ibmvscsi_host_data *hostdata = evt_struct->hostdata;
	u8 status = evt_struct->xfer_iu->mad.fast_fail.common.status;

	if (status == VIOSRP_MAD_NOT_SUPPORTED)
		dev_err(hostdata->dev, "fast_fail not supported in server\n");
	else if (status == VIOSRP_MAD_FAILED)
		dev_err(hostdata->dev, "fast_fail request failed\n");
	else if (status != VIOSRP_MAD_SUCCESS)
		dev_err(hostdata->dev, "error 0x%X enabling fast_fail\n", status);

	send_mad_capabilities(hostdata);
}

static int enable_fast_fail(struct ibmvscsi_host_data *hostdata)
{
	int rc;
	unsigned long flags;
	struct viosrp_fast_fail *fast_fail_mad;
	struct srp_event_struct *evt_struct;

	if (!fast_fail) {
		send_mad_capabilities(hostdata);
		return 0;
	}

	evt_struct = get_event_struct(&hostdata->pool);
	BUG_ON(!evt_struct);

	init_event_struct(evt_struct, fast_fail_rsp, VIOSRP_MAD_FORMAT, info_timeout);

	fast_fail_mad = &evt_struct->iu.mad.fast_fail;
	memset(fast_fail_mad, 0, sizeof(*fast_fail_mad));
	fast_fail_mad->common.type = VIOSRP_ENABLE_FAST_FAIL;
	fast_fail_mad->common.length = sizeof(*fast_fail_mad);

	spin_lock_irqsave(hostdata->host->host_lock, flags);
	rc = ibmvscsi_send_srp_event(evt_struct, hostdata, info_timeout * 2);
	spin_unlock_irqrestore(hostdata->host->host_lock, flags);
	return rc;
}

static void adapter_info_rsp(struct srp_event_struct *evt_struct)
{
	struct ibmvscsi_host_data *hostdata = evt_struct->hostdata;

	if (evt_struct->xfer_iu->mad.adapter_info.common.status) {
		dev_err(hostdata->dev, "error %d getting adapter info\n",
			evt_struct->xfer_iu->mad.adapter_info.common.status);
	} else {
		dev_info(hostdata->dev, "host srp version: %s, "
			 "host partition %s (%d), OS %d, max io %u\n",
			 hostdata->madapter_info.srp_version,
			 hostdata->madapter_info.partition_name,
			 hostdata->madapter_info.partition_number,
			 hostdata->madapter_info.os_type,
			 hostdata->madapter_info.port_max_txu[0]);
		
		if (hostdata->madapter_info.port_max_txu[0]) 
			hostdata->host->max_sectors = 
				hostdata->madapter_info.port_max_txu[0] >> 9;
		
		if (hostdata->madapter_info.os_type == 3 &&
		    strcmp(hostdata->madapter_info.srp_version, "1.6a") <= 0) {
			dev_err(hostdata->dev, "host (Ver. %s) doesn't support large transfers\n",
				hostdata->madapter_info.srp_version);
			dev_err(hostdata->dev, "limiting scatterlists to %d\n",
				MAX_INDIRECT_BUFS);
			hostdata->host->sg_tablesize = MAX_INDIRECT_BUFS;
		}

		if (hostdata->madapter_info.os_type == 3) {
			enable_fast_fail(hostdata);
			return;
		}
	}

	send_srp_login(hostdata);
}

static void send_mad_adapter_info(struct ibmvscsi_host_data *hostdata)
{
	struct viosrp_adapter_info *req;
	struct srp_event_struct *evt_struct;
	unsigned long flags;

	evt_struct = get_event_struct(&hostdata->pool);
	BUG_ON(!evt_struct);

	init_event_struct(evt_struct,
			  adapter_info_rsp,
			  VIOSRP_MAD_FORMAT,
			  info_timeout);
	
	req = &evt_struct->iu.mad.adapter_info;
	memset(req, 0x00, sizeof(*req));
	
	req->common.type = VIOSRP_ADAPTER_INFO_TYPE;
	req->common.length = sizeof(hostdata->madapter_info);
	req->buffer = hostdata->adapter_info_addr;

	spin_lock_irqsave(hostdata->host->host_lock, flags);
	if (ibmvscsi_send_srp_event(evt_struct, hostdata, info_timeout * 2))
		dev_err(hostdata->dev, "couldn't send ADAPTER_INFO_REQ!\n");
	spin_unlock_irqrestore(hostdata->host->host_lock, flags);
};

static void init_adapter(struct ibmvscsi_host_data *hostdata)
{
	send_mad_adapter_info(hostdata);
}

static void sync_completion(struct srp_event_struct *evt_struct)
{
	
	if (evt_struct->sync_srp)
		*evt_struct->sync_srp = *evt_struct->xfer_iu;
	
	complete(&evt_struct->comp);
}

static int ibmvscsi_eh_abort_handler(struct scsi_cmnd *cmd)
{
	struct ibmvscsi_host_data *hostdata = shost_priv(cmd->device->host);
	struct srp_tsk_mgmt *tsk_mgmt;
	struct srp_event_struct *evt;
	struct srp_event_struct *tmp_evt, *found_evt;
	union viosrp_iu srp_rsp;
	int rsp_rc;
	unsigned long flags;
	u16 lun = lun_from_dev(cmd->device);
	unsigned long wait_switch = 0;

	spin_lock_irqsave(hostdata->host->host_lock, flags);
	wait_switch = jiffies + (init_timeout * HZ);
	do {
		found_evt = NULL;
		list_for_each_entry(tmp_evt, &hostdata->sent, list) {
			if (tmp_evt->cmnd == cmd) {
				found_evt = tmp_evt;
				break;
			}
		}

		if (!found_evt) {
			spin_unlock_irqrestore(hostdata->host->host_lock, flags);
			return SUCCESS;
		}

		evt = get_event_struct(&hostdata->pool);
		if (evt == NULL) {
			spin_unlock_irqrestore(hostdata->host->host_lock, flags);
			sdev_printk(KERN_ERR, cmd->device,
				"failed to allocate abort event\n");
			return FAILED;
		}
	
		init_event_struct(evt,
				  sync_completion,
				  VIOSRP_SRP_FORMAT,
				  abort_timeout);

		tsk_mgmt = &evt->iu.srp.tsk_mgmt;
	
		
		memset(tsk_mgmt, 0x00, sizeof(*tsk_mgmt));
		tsk_mgmt->opcode = SRP_TSK_MGMT;
		tsk_mgmt->lun = ((u64) lun) << 48;
		tsk_mgmt->tsk_mgmt_func = SRP_TSK_ABORT_TASK;
		tsk_mgmt->task_tag = (u64) found_evt;

		evt->sync_srp = &srp_rsp;

		init_completion(&evt->comp);
		rsp_rc = ibmvscsi_send_srp_event(evt, hostdata, abort_timeout * 2);

		if (rsp_rc != SCSI_MLQUEUE_HOST_BUSY)
			break;

		spin_unlock_irqrestore(hostdata->host->host_lock, flags);
		msleep(10);
		spin_lock_irqsave(hostdata->host->host_lock, flags);
	} while (time_before(jiffies, wait_switch));

	spin_unlock_irqrestore(hostdata->host->host_lock, flags);

	if (rsp_rc != 0) {
		sdev_printk(KERN_ERR, cmd->device,
			    "failed to send abort() event. rc=%d\n", rsp_rc);
		return FAILED;
	}

	sdev_printk(KERN_INFO, cmd->device,
                    "aborting command. lun 0x%llx, tag 0x%llx\n",
		    (((u64) lun) << 48), (u64) found_evt);

	wait_for_completion(&evt->comp);

	
	if (unlikely(srp_rsp.srp.rsp.opcode != SRP_RSP)) {
		if (printk_ratelimit())
			sdev_printk(KERN_WARNING, cmd->device, "abort bad SRP RSP type %d\n",
				    srp_rsp.srp.rsp.opcode);
		return FAILED;
	}

	if (srp_rsp.srp.rsp.flags & SRP_RSP_FLAG_RSPVALID)
		rsp_rc = *((int *)srp_rsp.srp.rsp.data);
	else
		rsp_rc = srp_rsp.srp.rsp.status;

	if (rsp_rc) {
		if (printk_ratelimit())
			sdev_printk(KERN_WARNING, cmd->device,
				    "abort code %d for task tag 0x%llx\n",
				    rsp_rc, tsk_mgmt->task_tag);
		return FAILED;
	}

	spin_lock_irqsave(hostdata->host->host_lock, flags);
	found_evt = NULL;
	list_for_each_entry(tmp_evt, &hostdata->sent, list) {
		if (tmp_evt->cmnd == cmd) {
			found_evt = tmp_evt;
			break;
		}
	}

	if (found_evt == NULL) {
		spin_unlock_irqrestore(hostdata->host->host_lock, flags);
		sdev_printk(KERN_INFO, cmd->device, "aborted task tag 0x%llx completed\n",
			    tsk_mgmt->task_tag);
		return SUCCESS;
	}

	sdev_printk(KERN_INFO, cmd->device, "successfully aborted task tag 0x%llx\n",
		    tsk_mgmt->task_tag);

	cmd->result = (DID_ABORT << 16);
	list_del(&found_evt->list);
	unmap_cmd_data(&found_evt->iu.srp.cmd, found_evt,
		       found_evt->hostdata->dev);
	free_event_struct(&found_evt->hostdata->pool, found_evt);
	spin_unlock_irqrestore(hostdata->host->host_lock, flags);
	atomic_inc(&hostdata->request_limit);
	return SUCCESS;
}

static int ibmvscsi_eh_device_reset_handler(struct scsi_cmnd *cmd)
{
	struct ibmvscsi_host_data *hostdata = shost_priv(cmd->device->host);
	struct srp_tsk_mgmt *tsk_mgmt;
	struct srp_event_struct *evt;
	struct srp_event_struct *tmp_evt, *pos;
	union viosrp_iu srp_rsp;
	int rsp_rc;
	unsigned long flags;
	u16 lun = lun_from_dev(cmd->device);
	unsigned long wait_switch = 0;

	spin_lock_irqsave(hostdata->host->host_lock, flags);
	wait_switch = jiffies + (init_timeout * HZ);
	do {
		evt = get_event_struct(&hostdata->pool);
		if (evt == NULL) {
			spin_unlock_irqrestore(hostdata->host->host_lock, flags);
			sdev_printk(KERN_ERR, cmd->device,
				"failed to allocate reset event\n");
			return FAILED;
		}
	
		init_event_struct(evt,
				  sync_completion,
				  VIOSRP_SRP_FORMAT,
				  reset_timeout);

		tsk_mgmt = &evt->iu.srp.tsk_mgmt;

		
		memset(tsk_mgmt, 0x00, sizeof(*tsk_mgmt));
		tsk_mgmt->opcode = SRP_TSK_MGMT;
		tsk_mgmt->lun = ((u64) lun) << 48;
		tsk_mgmt->tsk_mgmt_func = SRP_TSK_LUN_RESET;

		evt->sync_srp = &srp_rsp;

		init_completion(&evt->comp);
		rsp_rc = ibmvscsi_send_srp_event(evt, hostdata, reset_timeout * 2);

		if (rsp_rc != SCSI_MLQUEUE_HOST_BUSY)
			break;

		spin_unlock_irqrestore(hostdata->host->host_lock, flags);
		msleep(10);
		spin_lock_irqsave(hostdata->host->host_lock, flags);
	} while (time_before(jiffies, wait_switch));

	spin_unlock_irqrestore(hostdata->host->host_lock, flags);

	if (rsp_rc != 0) {
		sdev_printk(KERN_ERR, cmd->device,
			    "failed to send reset event. rc=%d\n", rsp_rc);
		return FAILED;
	}

	sdev_printk(KERN_INFO, cmd->device, "resetting device. lun 0x%llx\n",
		    (((u64) lun) << 48));

	wait_for_completion(&evt->comp);

	
	if (unlikely(srp_rsp.srp.rsp.opcode != SRP_RSP)) {
		if (printk_ratelimit())
			sdev_printk(KERN_WARNING, cmd->device, "reset bad SRP RSP type %d\n",
				    srp_rsp.srp.rsp.opcode);
		return FAILED;
	}

	if (srp_rsp.srp.rsp.flags & SRP_RSP_FLAG_RSPVALID)
		rsp_rc = *((int *)srp_rsp.srp.rsp.data);
	else
		rsp_rc = srp_rsp.srp.rsp.status;

	if (rsp_rc) {
		if (printk_ratelimit())
			sdev_printk(KERN_WARNING, cmd->device,
				    "reset code %d for task tag 0x%llx\n",
				    rsp_rc, tsk_mgmt->task_tag);
		return FAILED;
	}

	spin_lock_irqsave(hostdata->host->host_lock, flags);
	list_for_each_entry_safe(tmp_evt, pos, &hostdata->sent, list) {
		if ((tmp_evt->cmnd) && (tmp_evt->cmnd->device == cmd->device)) {
			if (tmp_evt->cmnd)
				tmp_evt->cmnd->result = (DID_RESET << 16);
			list_del(&tmp_evt->list);
			unmap_cmd_data(&tmp_evt->iu.srp.cmd, tmp_evt,
				       tmp_evt->hostdata->dev);
			free_event_struct(&tmp_evt->hostdata->pool,
						   tmp_evt);
			atomic_inc(&hostdata->request_limit);
			if (tmp_evt->cmnd_done)
				tmp_evt->cmnd_done(tmp_evt->cmnd);
			else if (tmp_evt->done)
				tmp_evt->done(tmp_evt);
		}
	}
	spin_unlock_irqrestore(hostdata->host->host_lock, flags);
	return SUCCESS;
}

static int ibmvscsi_eh_host_reset_handler(struct scsi_cmnd *cmd)
{
	unsigned long wait_switch = 0;
	struct ibmvscsi_host_data *hostdata = shost_priv(cmd->device->host);

	dev_err(hostdata->dev, "Resetting connection due to error recovery\n");

	ibmvscsi_reset_host(hostdata);

	for (wait_switch = jiffies + (init_timeout * HZ);
	     time_before(jiffies, wait_switch) &&
		     atomic_read(&hostdata->request_limit) < 2;) {

		msleep(10);
	}

	if (atomic_read(&hostdata->request_limit) <= 0)
		return FAILED;

	return SUCCESS;
}

void ibmvscsi_handle_crq(struct viosrp_crq *crq,
			 struct ibmvscsi_host_data *hostdata)
{
	long rc;
	unsigned long flags;
	struct srp_event_struct *evt_struct =
	    (struct srp_event_struct *)crq->IU_data_ptr;
	switch (crq->valid) {
	case 0xC0:		
		switch (crq->format) {
		case 0x01:	
			dev_info(hostdata->dev, "partner initialized\n");
			
			if ((rc = ibmvscsi_ops->send_crq(hostdata,
							 0xC002000000000000LL, 0)) == 0) {
				
				init_adapter(hostdata);
			} else {
				dev_err(hostdata->dev, "Unable to send init rsp. rc=%ld\n", rc);
			}

			break;
		case 0x02:	
			dev_info(hostdata->dev, "partner initialization complete\n");

			
			init_adapter(hostdata);
			break;
		default:
			dev_err(hostdata->dev, "unknown crq message type: %d\n", crq->format);
		}
		return;
	case 0xFF:	
		scsi_block_requests(hostdata->host);
		atomic_set(&hostdata->request_limit, 0);
		if (crq->format == 0x06) {
			
			dev_info(hostdata->dev, "Re-enabling adapter!\n");
			hostdata->client_migrated = 1;
			hostdata->reenable_crq = 1;
			purge_requests(hostdata, DID_REQUEUE);
			wake_up(&hostdata->work_wait_q);
		} else {
			dev_err(hostdata->dev, "Virtual adapter failed rc %d!\n",
				crq->format);
			ibmvscsi_reset_host(hostdata);
		}
		return;
	case 0x80:		
		break;
	default:
		dev_err(hostdata->dev, "got an invalid message type 0x%02x\n",
			crq->valid);
		return;
	}

	if (!valid_event_struct(&hostdata->pool, evt_struct)) {
		dev_err(hostdata->dev, "returned correlation_token 0x%p is invalid!\n",
		       (void *)crq->IU_data_ptr);
		return;
	}

	if (atomic_read(&evt_struct->free)) {
		dev_err(hostdata->dev, "received duplicate correlation_token 0x%p!\n",
			(void *)crq->IU_data_ptr);
		return;
	}

	if (crq->format == VIOSRP_SRP_FORMAT)
		atomic_add(evt_struct->xfer_iu->srp.rsp.req_lim_delta,
			   &hostdata->request_limit);

	del_timer(&evt_struct->timer);

	if ((crq->status != VIOSRP_OK && crq->status != VIOSRP_OK2) && evt_struct->cmnd)
		evt_struct->cmnd->result = DID_ERROR << 16;
	if (evt_struct->done)
		evt_struct->done(evt_struct);
	else
		dev_err(hostdata->dev, "returned done() is NULL; not running it!\n");

	spin_lock_irqsave(evt_struct->hostdata->host->host_lock, flags);
	list_del(&evt_struct->list);
	free_event_struct(&evt_struct->hostdata->pool, evt_struct);
	spin_unlock_irqrestore(evt_struct->hostdata->host->host_lock, flags);
}

static int ibmvscsi_do_host_config(struct ibmvscsi_host_data *hostdata,
				   unsigned char *buffer, int length)
{
	struct viosrp_host_config *host_config;
	struct srp_event_struct *evt_struct;
	unsigned long flags;
	dma_addr_t addr;
	int rc;

	evt_struct = get_event_struct(&hostdata->pool);
	if (!evt_struct) {
		dev_err(hostdata->dev, "couldn't allocate event for HOST_CONFIG!\n");
		return -1;
	}

	init_event_struct(evt_struct,
			  sync_completion,
			  VIOSRP_MAD_FORMAT,
			  info_timeout);

	host_config = &evt_struct->iu.mad.host_config;

	
	memset(host_config, 0x00, sizeof(*host_config));
	host_config->common.type = VIOSRP_HOST_CONFIG_TYPE;
	host_config->common.length = length;
	host_config->buffer = addr = dma_map_single(hostdata->dev, buffer,
						    length,
						    DMA_BIDIRECTIONAL);

	if (dma_mapping_error(hostdata->dev, host_config->buffer)) {
		if (!firmware_has_feature(FW_FEATURE_CMO))
			dev_err(hostdata->dev,
			        "dma_mapping error getting host config\n");
		free_event_struct(&hostdata->pool, evt_struct);
		return -1;
	}

	init_completion(&evt_struct->comp);
	spin_lock_irqsave(hostdata->host->host_lock, flags);
	rc = ibmvscsi_send_srp_event(evt_struct, hostdata, info_timeout * 2);
	spin_unlock_irqrestore(hostdata->host->host_lock, flags);
	if (rc == 0)
		wait_for_completion(&evt_struct->comp);
	dma_unmap_single(hostdata->dev, addr, length, DMA_BIDIRECTIONAL);

	return rc;
}

static int ibmvscsi_slave_configure(struct scsi_device *sdev)
{
	struct Scsi_Host *shost = sdev->host;
	unsigned long lock_flags = 0;

	spin_lock_irqsave(shost->host_lock, lock_flags);
	if (sdev->type == TYPE_DISK) {
		sdev->allow_restart = 1;
		blk_queue_rq_timeout(sdev->request_queue, 120 * HZ);
	}
	scsi_adjust_queue_depth(sdev, 0, shost->cmd_per_lun);
	spin_unlock_irqrestore(shost->host_lock, lock_flags);
	return 0;
}

static int ibmvscsi_change_queue_depth(struct scsi_device *sdev, int qdepth,
				       int reason)
{
	if (reason != SCSI_QDEPTH_DEFAULT)
		return -EOPNOTSUPP;

	if (qdepth > IBMVSCSI_MAX_CMDS_PER_LUN)
		qdepth = IBMVSCSI_MAX_CMDS_PER_LUN;

	scsi_adjust_queue_depth(sdev, 0, qdepth);
	return sdev->queue_depth;
}

static ssize_t show_host_vhost_loc(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ibmvscsi_host_data *hostdata = shost_priv(shost);
	int len;

	len = snprintf(buf, sizeof(hostdata->caps.loc), "%s\n",
		       hostdata->caps.loc);
	return len;
}

static struct device_attribute ibmvscsi_host_vhost_loc = {
	.attr = {
		 .name = "vhost_loc",
		 .mode = S_IRUGO,
		 },
	.show = show_host_vhost_loc,
};

static ssize_t show_host_vhost_name(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ibmvscsi_host_data *hostdata = shost_priv(shost);
	int len;

	len = snprintf(buf, sizeof(hostdata->caps.name), "%s\n",
		       hostdata->caps.name);
	return len;
}

static struct device_attribute ibmvscsi_host_vhost_name = {
	.attr = {
		 .name = "vhost_name",
		 .mode = S_IRUGO,
		 },
	.show = show_host_vhost_name,
};

static ssize_t show_host_srp_version(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ibmvscsi_host_data *hostdata = shost_priv(shost);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%s\n",
		       hostdata->madapter_info.srp_version);
	return len;
}

static struct device_attribute ibmvscsi_host_srp_version = {
	.attr = {
		 .name = "srp_version",
		 .mode = S_IRUGO,
		 },
	.show = show_host_srp_version,
};

static ssize_t show_host_partition_name(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ibmvscsi_host_data *hostdata = shost_priv(shost);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%s\n",
		       hostdata->madapter_info.partition_name);
	return len;
}

static struct device_attribute ibmvscsi_host_partition_name = {
	.attr = {
		 .name = "partition_name",
		 .mode = S_IRUGO,
		 },
	.show = show_host_partition_name,
};

static ssize_t show_host_partition_number(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ibmvscsi_host_data *hostdata = shost_priv(shost);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
		       hostdata->madapter_info.partition_number);
	return len;
}

static struct device_attribute ibmvscsi_host_partition_number = {
	.attr = {
		 .name = "partition_number",
		 .mode = S_IRUGO,
		 },
	.show = show_host_partition_number,
};

static ssize_t show_host_mad_version(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ibmvscsi_host_data *hostdata = shost_priv(shost);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
		       hostdata->madapter_info.mad_version);
	return len;
}

static struct device_attribute ibmvscsi_host_mad_version = {
	.attr = {
		 .name = "mad_version",
		 .mode = S_IRUGO,
		 },
	.show = show_host_mad_version,
};

static ssize_t show_host_os_type(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ibmvscsi_host_data *hostdata = shost_priv(shost);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n", hostdata->madapter_info.os_type);
	return len;
}

static struct device_attribute ibmvscsi_host_os_type = {
	.attr = {
		 .name = "os_type",
		 .mode = S_IRUGO,
		 },
	.show = show_host_os_type,
};

static ssize_t show_host_config(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ibmvscsi_host_data *hostdata = shost_priv(shost);

	
	if (ibmvscsi_do_host_config(hostdata, buf, PAGE_SIZE) == 0)
		return strlen(buf);
	else
		return 0;
}

static struct device_attribute ibmvscsi_host_config = {
	.attr = {
		 .name = "config",
		 .mode = S_IRUGO,
		 },
	.show = show_host_config,
};

static struct device_attribute *ibmvscsi_attrs[] = {
	&ibmvscsi_host_vhost_loc,
	&ibmvscsi_host_vhost_name,
	&ibmvscsi_host_srp_version,
	&ibmvscsi_host_partition_name,
	&ibmvscsi_host_partition_number,
	&ibmvscsi_host_mad_version,
	&ibmvscsi_host_os_type,
	&ibmvscsi_host_config,
	NULL
};

static struct scsi_host_template driver_template = {
	.module = THIS_MODULE,
	.name = "IBM POWER Virtual SCSI Adapter " IBMVSCSI_VERSION,
	.proc_name = "ibmvscsi",
	.queuecommand = ibmvscsi_queuecommand,
	.eh_abort_handler = ibmvscsi_eh_abort_handler,
	.eh_device_reset_handler = ibmvscsi_eh_device_reset_handler,
	.eh_host_reset_handler = ibmvscsi_eh_host_reset_handler,
	.slave_configure = ibmvscsi_slave_configure,
	.change_queue_depth = ibmvscsi_change_queue_depth,
	.cmd_per_lun = IBMVSCSI_CMDS_PER_LUN_DEFAULT,
	.can_queue = IBMVSCSI_MAX_REQUESTS_DEFAULT,
	.this_id = -1,
	.sg_tablesize = SG_ALL,
	.use_clustering = ENABLE_CLUSTERING,
	.shost_attrs = ibmvscsi_attrs,
};

static unsigned long ibmvscsi_get_desired_dma(struct vio_dev *vdev)
{
	
	unsigned long desired_io = max_events * sizeof(union viosrp_iu);

	
	desired_io += (IBMVSCSI_MAX_SECTORS_DEFAULT * 512 *
	                     IBMVSCSI_CMDS_PER_LUN_DEFAULT);

	return desired_io;
}

static void ibmvscsi_do_work(struct ibmvscsi_host_data *hostdata)
{
	int rc;
	char *action = "reset";

	if (hostdata->reset_crq) {
		smp_rmb();
		hostdata->reset_crq = 0;

		rc = ibmvscsi_ops->reset_crq_queue(&hostdata->queue, hostdata);
		if (!rc)
			rc = ibmvscsi_ops->send_crq(hostdata, 0xC001000000000000LL, 0);
		vio_enable_interrupts(to_vio_dev(hostdata->dev));
	} else if (hostdata->reenable_crq) {
		smp_rmb();
		action = "enable";
		rc = ibmvscsi_ops->reenable_crq_queue(&hostdata->queue, hostdata);
		hostdata->reenable_crq = 0;
		if (!rc)
			rc = ibmvscsi_ops->send_crq(hostdata, 0xC001000000000000LL, 0);
	} else
		return;

	if (rc) {
		atomic_set(&hostdata->request_limit, -1);
		dev_err(hostdata->dev, "error after %s\n", action);
	}

	scsi_unblock_requests(hostdata->host);
}

static int ibmvscsi_work_to_do(struct ibmvscsi_host_data *hostdata)
{
	if (kthread_should_stop())
		return 1;
	else if (hostdata->reset_crq) {
		smp_rmb();
		return 1;
	} else if (hostdata->reenable_crq) {
		smp_rmb();
		return 1;
	}

	return 0;
}

static int ibmvscsi_work(void *data)
{
	struct ibmvscsi_host_data *hostdata = data;
	int rc;

	set_user_nice(current, -20);

	while (1) {
		rc = wait_event_interruptible(hostdata->work_wait_q,
					      ibmvscsi_work_to_do(hostdata));

		BUG_ON(rc);

		if (kthread_should_stop())
			break;

		ibmvscsi_do_work(hostdata);
	}

	return 0;
}

static int ibmvscsi_probe(struct vio_dev *vdev, const struct vio_device_id *id)
{
	struct ibmvscsi_host_data *hostdata;
	struct Scsi_Host *host;
	struct device *dev = &vdev->dev;
	struct srp_rport_identifiers ids;
	struct srp_rport *rport;
	unsigned long wait_switch = 0;
	int rc;

	dev_set_drvdata(&vdev->dev, NULL);

	host = scsi_host_alloc(&driver_template, sizeof(*hostdata));
	if (!host) {
		dev_err(&vdev->dev, "couldn't allocate host data\n");
		goto scsi_host_alloc_failed;
	}

	host->transportt = ibmvscsi_transport_template;
	hostdata = shost_priv(host);
	memset(hostdata, 0x00, sizeof(*hostdata));
	INIT_LIST_HEAD(&hostdata->sent);
	init_waitqueue_head(&hostdata->work_wait_q);
	hostdata->host = host;
	hostdata->dev = dev;
	atomic_set(&hostdata->request_limit, -1);
	hostdata->host->max_sectors = IBMVSCSI_MAX_SECTORS_DEFAULT;

	if (map_persist_bufs(hostdata)) {
		dev_err(&vdev->dev, "couldn't map persistent buffers\n");
		goto persist_bufs_failed;
	}

	hostdata->work_thread = kthread_run(ibmvscsi_work, hostdata, "%s_%d",
					    "ibmvscsi", host->host_no);

	if (IS_ERR(hostdata->work_thread)) {
		dev_err(&vdev->dev, "couldn't initialize kthread. rc=%ld\n",
			PTR_ERR(hostdata->work_thread));
		goto init_crq_failed;
	}

	rc = ibmvscsi_ops->init_crq_queue(&hostdata->queue, hostdata, max_events);
	if (rc != 0 && rc != H_RESOURCE) {
		dev_err(&vdev->dev, "couldn't initialize crq. rc=%d\n", rc);
		goto kill_kthread;
	}
	if (initialize_event_pool(&hostdata->pool, max_events, hostdata) != 0) {
		dev_err(&vdev->dev, "couldn't initialize event pool\n");
		goto init_pool_failed;
	}

	host->max_lun = 8;
	host->max_id = max_id;
	host->max_channel = max_channel;
	host->max_cmd_len = 16;

	if (scsi_add_host(hostdata->host, hostdata->dev))
		goto add_host_failed;

	
	memcpy(ids.port_id, hostdata->madapter_info.partition_name,
	       sizeof(ids.port_id));
	ids.roles = SRP_RPORT_ROLE_TARGET;
	rport = srp_rport_add(host, &ids);
	if (IS_ERR(rport))
		goto add_srp_port_failed;

	if (ibmvscsi_ops->send_crq(hostdata, 0xC001000000000000LL, 0) == 0
	    || rc == H_RESOURCE) {
		for (wait_switch = jiffies + (init_timeout * HZ);
		     time_before(jiffies, wait_switch) &&
		     atomic_read(&hostdata->request_limit) < 2;) {

			msleep(10);
		}

		
		if (atomic_read(&hostdata->request_limit) > 0)
			scsi_scan_host(host);
	}

	dev_set_drvdata(&vdev->dev, hostdata);
	return 0;

      add_srp_port_failed:
	scsi_remove_host(hostdata->host);
      add_host_failed:
	release_event_pool(&hostdata->pool, hostdata);
      init_pool_failed:
	ibmvscsi_ops->release_crq_queue(&hostdata->queue, hostdata, max_events);
      kill_kthread:
      kthread_stop(hostdata->work_thread);
      init_crq_failed:
	unmap_persist_bufs(hostdata);
      persist_bufs_failed:
	scsi_host_put(host);
      scsi_host_alloc_failed:
	return -1;
}

static int ibmvscsi_remove(struct vio_dev *vdev)
{
	struct ibmvscsi_host_data *hostdata = dev_get_drvdata(&vdev->dev);
	unmap_persist_bufs(hostdata);
	release_event_pool(&hostdata->pool, hostdata);
	ibmvscsi_ops->release_crq_queue(&hostdata->queue, hostdata,
					max_events);

	kthread_stop(hostdata->work_thread);
	srp_remove_host(hostdata->host);
	scsi_remove_host(hostdata->host);
	scsi_host_put(hostdata->host);

	return 0;
}

static int ibmvscsi_resume(struct device *dev)
{
	struct ibmvscsi_host_data *hostdata = dev_get_drvdata(dev);
	return ibmvscsi_ops->resume(hostdata);
}

static struct vio_device_id ibmvscsi_device_table[] __devinitdata = {
	{"vscsi", "IBM,v-scsi"},
	{ "", "" }
};
MODULE_DEVICE_TABLE(vio, ibmvscsi_device_table);

static struct dev_pm_ops ibmvscsi_pm_ops = {
	.resume = ibmvscsi_resume
};

static struct vio_driver ibmvscsi_driver = {
	.id_table = ibmvscsi_device_table,
	.probe = ibmvscsi_probe,
	.remove = ibmvscsi_remove,
	.get_desired_dma = ibmvscsi_get_desired_dma,
	.name = "ibmvscsi",
	.pm = &ibmvscsi_pm_ops,
};

static struct srp_function_template ibmvscsi_transport_functions = {
};

int __init ibmvscsi_module_init(void)
{
	int ret;

	
	driver_template.can_queue = max_requests;
	max_events = max_requests + 2;

	if (firmware_has_feature(FW_FEATURE_VIO))
		ibmvscsi_ops = &rpavscsi_ops;
	else
		return -ENODEV;

	ibmvscsi_transport_template =
		srp_attach_transport(&ibmvscsi_transport_functions);
	if (!ibmvscsi_transport_template)
		return -ENOMEM;

	ret = vio_register_driver(&ibmvscsi_driver);
	if (ret)
		srp_release_transport(ibmvscsi_transport_template);
	return ret;
}

void __exit ibmvscsi_module_exit(void)
{
	vio_unregister_driver(&ibmvscsi_driver);
	srp_release_transport(ibmvscsi_transport_template);
}

module_init(ibmvscsi_module_init);
module_exit(ibmvscsi_module_exit);
