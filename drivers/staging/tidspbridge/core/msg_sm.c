/*
 * msg_sm.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Implements upper edge functions for Bridge message module.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <linux/types.h>

#include <dspbridge/dbdefs.h>

#include <dspbridge/sync.h>

#include <dspbridge/dev.h>

#include <dspbridge/io_sm.h>

#include <_msg_sm.h>
#include <dspbridge/dspmsg.h>

static int add_new_msg(struct list_head *msg_list);
static void delete_msg_mgr(struct msg_mgr *hmsg_mgr);
static void delete_msg_queue(struct msg_queue *msg_queue_obj, u32 num_to_dsp);
static void free_msg_list(struct list_head *msg_list);

int bridge_msg_create(struct msg_mgr **msg_man,
			     struct dev_object *hdev_obj,
			     msg_onexit msg_callback)
{
	struct msg_mgr *msg_mgr_obj;
	struct io_mgr *hio_mgr;
	int status = 0;

	if (!msg_man || !msg_callback || !hdev_obj)
		return -EFAULT;

	dev_get_io_mgr(hdev_obj, &hio_mgr);
	if (!hio_mgr)
		return -EFAULT;

	*msg_man = NULL;
	
	msg_mgr_obj = kzalloc(sizeof(struct msg_mgr), GFP_KERNEL);
	if (!msg_mgr_obj)
		return -ENOMEM;

	msg_mgr_obj->on_exit = msg_callback;
	msg_mgr_obj->iomgr = hio_mgr;
	
	INIT_LIST_HEAD(&msg_mgr_obj->queue_list);
	INIT_LIST_HEAD(&msg_mgr_obj->msg_free_list);
	INIT_LIST_HEAD(&msg_mgr_obj->msg_used_list);
	spin_lock_init(&msg_mgr_obj->msg_mgr_lock);

	msg_mgr_obj->sync_event =
		kzalloc(sizeof(struct sync_object), GFP_KERNEL);
	if (!msg_mgr_obj->sync_event) {
		kfree(msg_mgr_obj);
		return -ENOMEM;
	}
	sync_init_event(msg_mgr_obj->sync_event);

	*msg_man = msg_mgr_obj;

	return status;
}

int bridge_msg_create_queue(struct msg_mgr *hmsg_mgr, struct msg_queue **msgq,
				u32 msgq_id, u32 max_msgs, void *arg)
{
	u32 i;
	u32 num_allocated = 0;
	struct msg_queue *msg_q;
	int status = 0;

	if (!hmsg_mgr || msgq == NULL)
		return -EFAULT;

	*msgq = NULL;
	
	msg_q = kzalloc(sizeof(struct msg_queue), GFP_KERNEL);
	if (!msg_q)
		return -ENOMEM;

	msg_q->max_msgs = max_msgs;
	msg_q->msg_mgr = hmsg_mgr;
	msg_q->arg = arg;	
	msg_q->msgq_id = msgq_id;	
	
	INIT_LIST_HEAD(&msg_q->msg_free_list);
	INIT_LIST_HEAD(&msg_q->msg_used_list);

	msg_q->sync_event = kzalloc(sizeof(struct sync_object), GFP_KERNEL);
	if (!msg_q->sync_event) {
		status = -ENOMEM;
		goto out_err;

	}
	sync_init_event(msg_q->sync_event);

	
	msg_q->ntfy_obj = kmalloc(sizeof(struct ntfy_object), GFP_KERNEL);
	if (!msg_q->ntfy_obj) {
		status = -ENOMEM;
		goto out_err;
	}
	ntfy_init(msg_q->ntfy_obj);

	msg_q->sync_done = kzalloc(sizeof(struct sync_object), GFP_KERNEL);
	if (!msg_q->sync_done) {
		status = -ENOMEM;
		goto out_err;
	}
	sync_init_event(msg_q->sync_done);

	msg_q->sync_done_ack = kzalloc(sizeof(struct sync_object), GFP_KERNEL);
	if (!msg_q->sync_done_ack) {
		status = -ENOMEM;
		goto out_err;
	}
	sync_init_event(msg_q->sync_done_ack);

	
	spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
	
	for (i = 0; i < max_msgs && !status; i++) {
		status = add_new_msg(&hmsg_mgr->msg_free_list);
		if (!status) {
			num_allocated++;
			status = add_new_msg(&msg_q->msg_free_list);
		}
	}
	if (status) {
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		goto out_err;
	}

	list_add_tail(&msg_q->list_elem, &hmsg_mgr->queue_list);
	*msgq = msg_q;
	
	if (!list_empty(&hmsg_mgr->msg_free_list))
		sync_set_event(hmsg_mgr->sync_event);

	
	spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);

	return 0;
out_err:
	delete_msg_queue(msg_q, num_allocated);
	return status;
}

void bridge_msg_delete(struct msg_mgr *hmsg_mgr)
{
	if (hmsg_mgr)
		delete_msg_mgr(hmsg_mgr);
}

void bridge_msg_delete_queue(struct msg_queue *msg_queue_obj)
{
	struct msg_mgr *hmsg_mgr;
	u32 io_msg_pend;

	if (!msg_queue_obj || !msg_queue_obj->msg_mgr)
		return;

	hmsg_mgr = msg_queue_obj->msg_mgr;
	msg_queue_obj->done = true;
	
	io_msg_pend = msg_queue_obj->io_msg_pend;
	while (io_msg_pend) {
		
		sync_set_event(msg_queue_obj->sync_done);
		
		sync_wait_on_event(msg_queue_obj->sync_done_ack, SYNC_INFINITE);
		io_msg_pend = msg_queue_obj->io_msg_pend;
	}
	
	spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
	list_del(&msg_queue_obj->list_elem);
	
	delete_msg_queue(msg_queue_obj, msg_queue_obj->max_msgs);
	if (list_empty(&hmsg_mgr->msg_free_list))
		sync_reset_event(hmsg_mgr->sync_event);
	spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
}

int bridge_msg_get(struct msg_queue *msg_queue_obj,
			  struct dsp_msg *pmsg, u32 utimeout)
{
	struct msg_frame *msg_frame_obj;
	struct msg_mgr *hmsg_mgr;
	struct sync_object *syncs[2];
	u32 index;
	int status = 0;

	if (!msg_queue_obj || pmsg == NULL)
		return -ENOMEM;

	hmsg_mgr = msg_queue_obj->msg_mgr;

	spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
	
	if (!list_empty(&msg_queue_obj->msg_used_list)) {
		msg_frame_obj = list_first_entry(&msg_queue_obj->msg_used_list,
				struct msg_frame, list_elem);
		list_del(&msg_frame_obj->list_elem);
		*pmsg = msg_frame_obj->msg_data.msg;
		list_add_tail(&msg_frame_obj->list_elem,
				&msg_queue_obj->msg_free_list);
		if (list_empty(&msg_queue_obj->msg_used_list))
			sync_reset_event(msg_queue_obj->sync_event);
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		return 0;
	}

	if (msg_queue_obj->done) {
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		return -EPERM;
	}
	msg_queue_obj->io_msg_pend++;
	spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);

	syncs[0] = msg_queue_obj->sync_event;
	syncs[1] = msg_queue_obj->sync_done;
	status = sync_wait_on_multiple_events(syncs, 2, utimeout, &index);

	spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
	if (msg_queue_obj->done) {
		msg_queue_obj->io_msg_pend--;
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		sync_set_event(msg_queue_obj->sync_done_ack);
		return -EPERM;
	}
	if (!status && !list_empty(&msg_queue_obj->msg_used_list)) {
		
		msg_frame_obj = list_first_entry(&msg_queue_obj->msg_used_list,
				struct msg_frame, list_elem);
		list_del(&msg_frame_obj->list_elem);
		
		*pmsg = msg_frame_obj->msg_data.msg;
		list_add_tail(&msg_frame_obj->list_elem,
				&msg_queue_obj->msg_free_list);
	}
	msg_queue_obj->io_msg_pend--;
	
	if (!list_empty(&msg_queue_obj->msg_used_list))
		sync_set_event(msg_queue_obj->sync_event);

	spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);

	return status;
}

int bridge_msg_put(struct msg_queue *msg_queue_obj,
			  const struct dsp_msg *pmsg, u32 utimeout)
{
	struct msg_frame *msg_frame_obj;
	struct msg_mgr *hmsg_mgr;
	struct sync_object *syncs[2];
	u32 index;
	int status;

	if (!msg_queue_obj || !pmsg || !msg_queue_obj->msg_mgr)
		return -EFAULT;

	hmsg_mgr = msg_queue_obj->msg_mgr;

	spin_lock_bh(&hmsg_mgr->msg_mgr_lock);

	
	if (!list_empty(&hmsg_mgr->msg_free_list)) {
		msg_frame_obj = list_first_entry(&hmsg_mgr->msg_free_list,
				struct msg_frame, list_elem);
		list_del(&msg_frame_obj->list_elem);
		msg_frame_obj->msg_data.msg = *pmsg;
		msg_frame_obj->msg_data.msgq_id =
			msg_queue_obj->msgq_id;
		list_add_tail(&msg_frame_obj->list_elem,
				&hmsg_mgr->msg_used_list);
		hmsg_mgr->msgs_pending++;

		if (list_empty(&hmsg_mgr->msg_free_list))
			sync_reset_event(hmsg_mgr->sync_event);

		
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		
		iosm_schedule(hmsg_mgr->iomgr);
		return 0;
	}

	if (msg_queue_obj->done) {
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		return -EPERM;
	}
	msg_queue_obj->io_msg_pend++;

	spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);

	
	syncs[0] = hmsg_mgr->sync_event;
	syncs[1] = msg_queue_obj->sync_done;
	status = sync_wait_on_multiple_events(syncs, 2, utimeout, &index);
	if (status)
		return status;

	
	spin_lock_bh(&hmsg_mgr->msg_mgr_lock);
	if (msg_queue_obj->done) {
		msg_queue_obj->io_msg_pend--;
		
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		sync_set_event(msg_queue_obj->sync_done_ack);
		return -EPERM;
	}

	if (list_empty(&hmsg_mgr->msg_free_list)) {
		spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);
		return -EFAULT;
	}

	
	msg_frame_obj = list_first_entry(&hmsg_mgr->msg_free_list,
			struct msg_frame, list_elem);
	list_del(&msg_frame_obj->list_elem);
	msg_frame_obj->msg_data.msg = *pmsg;
	msg_frame_obj->msg_data.msgq_id = msg_queue_obj->msgq_id;
	list_add_tail(&msg_frame_obj->list_elem, &hmsg_mgr->msg_used_list);
	hmsg_mgr->msgs_pending++;
	iosm_schedule(hmsg_mgr->iomgr);

	msg_queue_obj->io_msg_pend--;
	
	if (!list_empty(&hmsg_mgr->msg_free_list))
		sync_set_event(hmsg_mgr->sync_event);

	
	spin_unlock_bh(&hmsg_mgr->msg_mgr_lock);

	return 0;
}

int bridge_msg_register_notify(struct msg_queue *msg_queue_obj,
				   u32 event_mask, u32 notify_type,
				   struct dsp_notification *hnotification)
{
	int status = 0;

	if (!msg_queue_obj || !hnotification) {
		status = -ENOMEM;
		goto func_end;
	}

	if (!(event_mask == DSP_NODEMESSAGEREADY || event_mask == 0)) {
		status = -EPERM;
		goto func_end;
	}

	if (notify_type != DSP_SIGNALEVENT) {
		status = -EBADR;
		goto func_end;
	}

	if (event_mask)
		status = ntfy_register(msg_queue_obj->ntfy_obj, hnotification,
						event_mask, notify_type);
	else
		status = ntfy_unregister(msg_queue_obj->ntfy_obj,
							hnotification);

	if (status == -EINVAL) {
		status = 0;
	}
func_end:
	return status;
}

void bridge_msg_set_queue_id(struct msg_queue *msg_queue_obj, u32 msgq_id)
{
	if (msg_queue_obj)
		msg_queue_obj->msgq_id = msgq_id;
}

static int add_new_msg(struct list_head *msg_list)
{
	struct msg_frame *pmsg;

	pmsg = kzalloc(sizeof(struct msg_frame), GFP_ATOMIC);
	if (!pmsg)
		return -ENOMEM;

	list_add_tail(&pmsg->list_elem, msg_list);

	return 0;
}

static void delete_msg_mgr(struct msg_mgr *hmsg_mgr)
{
	if (!hmsg_mgr)
		return;

	
	free_msg_list(&hmsg_mgr->msg_free_list);
	free_msg_list(&hmsg_mgr->msg_used_list);
	kfree(hmsg_mgr->sync_event);
	kfree(hmsg_mgr);
}

static void delete_msg_queue(struct msg_queue *msg_queue_obj, u32 num_to_dsp)
{
	struct msg_mgr *hmsg_mgr;
	struct msg_frame *pmsg, *tmp;
	u32 i;

	if (!msg_queue_obj || !msg_queue_obj->msg_mgr)
		return;

	hmsg_mgr = msg_queue_obj->msg_mgr;

	
	i = 0;
	list_for_each_entry_safe(pmsg, tmp, &hmsg_mgr->msg_free_list,
			list_elem) {
		list_del(&pmsg->list_elem);
		kfree(pmsg);
		if (i++ >= num_to_dsp)
			break;
	}

	free_msg_list(&msg_queue_obj->msg_free_list);
	free_msg_list(&msg_queue_obj->msg_used_list);

	if (msg_queue_obj->ntfy_obj) {
		ntfy_delete(msg_queue_obj->ntfy_obj);
		kfree(msg_queue_obj->ntfy_obj);
	}

	kfree(msg_queue_obj->sync_event);
	kfree(msg_queue_obj->sync_done);
	kfree(msg_queue_obj->sync_done_ack);

	kfree(msg_queue_obj);
}

static void free_msg_list(struct list_head *msg_list)
{
	struct msg_frame *pmsg, *tmp;

	if (!msg_list)
		return;

	list_for_each_entry_safe(pmsg, tmp, msg_list, list_elem) {
		list_del(&pmsg->list_elem);
		kfree(pmsg);
	}
}
