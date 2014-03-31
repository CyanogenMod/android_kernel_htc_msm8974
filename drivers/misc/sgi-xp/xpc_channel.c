/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2004-2009 Silicon Graphics, Inc.  All Rights Reserved.
 */


#include <linux/device.h>
#include "xpc.h"

static void
xpc_process_connect(struct xpc_channel *ch, unsigned long *irq_flags)
{
	enum xp_retval ret;

	DBUG_ON(!spin_is_locked(&ch->lock));

	if (!(ch->flags & XPC_C_OPENREQUEST) ||
	    !(ch->flags & XPC_C_ROPENREQUEST)) {
		
		return;
	}
	DBUG_ON(!(ch->flags & XPC_C_CONNECTING));

	if (!(ch->flags & XPC_C_SETUP)) {
		spin_unlock_irqrestore(&ch->lock, *irq_flags);
		ret = xpc_arch_ops.setup_msg_structures(ch);
		spin_lock_irqsave(&ch->lock, *irq_flags);

		if (ret != xpSuccess)
			XPC_DISCONNECT_CHANNEL(ch, ret, irq_flags);
		else
			ch->flags |= XPC_C_SETUP;

		if (ch->flags & XPC_C_DISCONNECTING)
			return;
	}

	if (!(ch->flags & XPC_C_OPENREPLY)) {
		ch->flags |= XPC_C_OPENREPLY;
		xpc_arch_ops.send_chctl_openreply(ch, irq_flags);
	}

	if (!(ch->flags & XPC_C_ROPENREPLY))
		return;

	if (!(ch->flags & XPC_C_OPENCOMPLETE)) {
		ch->flags |= (XPC_C_OPENCOMPLETE | XPC_C_CONNECTED);
		xpc_arch_ops.send_chctl_opencomplete(ch, irq_flags);
	}

	if (!(ch->flags & XPC_C_ROPENCOMPLETE))
		return;

	dev_info(xpc_chan, "channel %d to partition %d connected\n",
		 ch->number, ch->partid);

	ch->flags = (XPC_C_CONNECTED | XPC_C_SETUP);	
}

static void
xpc_process_disconnect(struct xpc_channel *ch, unsigned long *irq_flags)
{
	struct xpc_partition *part = &xpc_partitions[ch->partid];
	u32 channel_was_connected = (ch->flags & XPC_C_WASCONNECTED);

	DBUG_ON(!spin_is_locked(&ch->lock));

	if (!(ch->flags & XPC_C_DISCONNECTING))
		return;

	DBUG_ON(!(ch->flags & XPC_C_CLOSEREQUEST));

	

	if (atomic_read(&ch->kthreads_assigned) > 0 ||
	    atomic_read(&ch->references) > 0) {
		return;
	}
	DBUG_ON((ch->flags & XPC_C_CONNECTEDCALLOUT_MADE) &&
		!(ch->flags & XPC_C_DISCONNECTINGCALLOUT_MADE));

	if (part->act_state == XPC_P_AS_DEACTIVATING) {
		
		if (xpc_arch_ops.partition_engaged(ch->partid))
			return;

	} else {

		

		if (!(ch->flags & XPC_C_RCLOSEREQUEST))
			return;

		if (!(ch->flags & XPC_C_CLOSEREPLY)) {
			ch->flags |= XPC_C_CLOSEREPLY;
			xpc_arch_ops.send_chctl_closereply(ch, irq_flags);
		}

		if (!(ch->flags & XPC_C_RCLOSEREPLY))
			return;
	}

	
	if (atomic_read(&ch->n_to_notify) > 0) {
		
		xpc_arch_ops.notify_senders_of_disconnect(ch);
	}

	

	if (ch->flags & XPC_C_DISCONNECTINGCALLOUT_MADE) {
		spin_unlock_irqrestore(&ch->lock, *irq_flags);
		xpc_disconnect_callout(ch, xpDisconnected);
		spin_lock_irqsave(&ch->lock, *irq_flags);
	}

	DBUG_ON(atomic_read(&ch->n_to_notify) != 0);

	
	xpc_arch_ops.teardown_msg_structures(ch);

	ch->func = NULL;
	ch->key = NULL;
	ch->entry_size = 0;
	ch->local_nentries = 0;
	ch->remote_nentries = 0;
	ch->kthreads_assigned_limit = 0;
	ch->kthreads_idle_limit = 0;

	ch->flags = (XPC_C_DISCONNECTED | (ch->flags & XPC_C_WDISCONNECT));

	atomic_dec(&part->nchannels_active);

	if (channel_was_connected) {
		dev_info(xpc_chan, "channel %d to partition %d disconnected, "
			 "reason=%d\n", ch->number, ch->partid, ch->reason);
	}

	if (ch->flags & XPC_C_WDISCONNECT) {
		
		complete(&ch->wdisconnect_wait);
	} else if (ch->delayed_chctl_flags) {
		if (part->act_state != XPC_P_AS_DEACTIVATING) {
			
			spin_lock(&part->chctl_lock);
			part->chctl.flags[ch->number] |=
			    ch->delayed_chctl_flags;
			spin_unlock(&part->chctl_lock);
		}
		ch->delayed_chctl_flags = 0;
	}
}

static void
xpc_process_openclose_chctl_flags(struct xpc_partition *part, int ch_number,
				  u8 chctl_flags)
{
	unsigned long irq_flags;
	struct xpc_openclose_args *args =
	    &part->remote_openclose_args[ch_number];
	struct xpc_channel *ch = &part->channels[ch_number];
	enum xp_retval reason;
	enum xp_retval ret;
	int create_kthread = 0;

	spin_lock_irqsave(&ch->lock, irq_flags);

again:

	if ((ch->flags & XPC_C_DISCONNECTED) &&
	    (ch->flags & XPC_C_WDISCONNECT)) {
		ch->delayed_chctl_flags |= chctl_flags;
		goto out;
	}

	if (chctl_flags & XPC_CHCTL_CLOSEREQUEST) {

		dev_dbg(xpc_chan, "XPC_CHCTL_CLOSEREQUEST (reason=%d) received "
			"from partid=%d, channel=%d\n", args->reason,
			ch->partid, ch->number);


		if (ch->flags & XPC_C_RCLOSEREQUEST) {
			DBUG_ON(!(ch->flags & XPC_C_DISCONNECTING));
			DBUG_ON(!(ch->flags & XPC_C_CLOSEREQUEST));
			DBUG_ON(!(ch->flags & XPC_C_CLOSEREPLY));
			DBUG_ON(ch->flags & XPC_C_RCLOSEREPLY);

			DBUG_ON(!(chctl_flags & XPC_CHCTL_CLOSEREPLY));
			chctl_flags &= ~XPC_CHCTL_CLOSEREPLY;
			ch->flags |= XPC_C_RCLOSEREPLY;

			
			xpc_process_disconnect(ch, &irq_flags);
			DBUG_ON(!(ch->flags & XPC_C_DISCONNECTED));
			goto again;
		}

		if (ch->flags & XPC_C_DISCONNECTED) {
			if (!(chctl_flags & XPC_CHCTL_OPENREQUEST)) {
				if (part->chctl.flags[ch_number] &
				    XPC_CHCTL_OPENREQUEST) {

					DBUG_ON(ch->delayed_chctl_flags != 0);
					spin_lock(&part->chctl_lock);
					part->chctl.flags[ch_number] |=
					    XPC_CHCTL_CLOSEREQUEST;
					spin_unlock(&part->chctl_lock);
				}
				goto out;
			}

			XPC_SET_REASON(ch, 0, 0);
			ch->flags &= ~XPC_C_DISCONNECTED;

			atomic_inc(&part->nchannels_active);
			ch->flags |= (XPC_C_CONNECTING | XPC_C_ROPENREQUEST);
		}

		chctl_flags &= ~(XPC_CHCTL_OPENREQUEST | XPC_CHCTL_OPENREPLY |
		    XPC_CHCTL_OPENCOMPLETE);


		ch->flags |= XPC_C_RCLOSEREQUEST;

		if (!(ch->flags & XPC_C_DISCONNECTING)) {
			reason = args->reason;
			if (reason <= xpSuccess || reason > xpUnknownReason)
				reason = xpUnknownReason;
			else if (reason == xpUnregistering)
				reason = xpOtherUnregistering;

			XPC_DISCONNECT_CHANNEL(ch, reason, &irq_flags);

			DBUG_ON(chctl_flags & XPC_CHCTL_CLOSEREPLY);
			goto out;
		}

		xpc_process_disconnect(ch, &irq_flags);
	}

	if (chctl_flags & XPC_CHCTL_CLOSEREPLY) {

		dev_dbg(xpc_chan, "XPC_CHCTL_CLOSEREPLY received from partid="
			"%d, channel=%d\n", ch->partid, ch->number);

		if (ch->flags & XPC_C_DISCONNECTED) {
			DBUG_ON(part->act_state != XPC_P_AS_DEACTIVATING);
			goto out;
		}

		DBUG_ON(!(ch->flags & XPC_C_CLOSEREQUEST));

		if (!(ch->flags & XPC_C_RCLOSEREQUEST)) {
			if (part->chctl.flags[ch_number] &
			    XPC_CHCTL_CLOSEREQUEST) {

				DBUG_ON(ch->delayed_chctl_flags != 0);
				spin_lock(&part->chctl_lock);
				part->chctl.flags[ch_number] |=
				    XPC_CHCTL_CLOSEREPLY;
				spin_unlock(&part->chctl_lock);
			}
			goto out;
		}

		ch->flags |= XPC_C_RCLOSEREPLY;

		if (ch->flags & XPC_C_CLOSEREPLY) {
			
			xpc_process_disconnect(ch, &irq_flags);
		}
	}

	if (chctl_flags & XPC_CHCTL_OPENREQUEST) {

		dev_dbg(xpc_chan, "XPC_CHCTL_OPENREQUEST (entry_size=%d, "
			"local_nentries=%d) received from partid=%d, "
			"channel=%d\n", args->entry_size, args->local_nentries,
			ch->partid, ch->number);

		if (part->act_state == XPC_P_AS_DEACTIVATING ||
		    (ch->flags & XPC_C_ROPENREQUEST)) {
			goto out;
		}

		if (ch->flags & (XPC_C_DISCONNECTING | XPC_C_WDISCONNECT)) {
			ch->delayed_chctl_flags |= XPC_CHCTL_OPENREQUEST;
			goto out;
		}
		DBUG_ON(!(ch->flags & (XPC_C_DISCONNECTED |
				       XPC_C_OPENREQUEST)));
		DBUG_ON(ch->flags & (XPC_C_ROPENREQUEST | XPC_C_ROPENREPLY |
				     XPC_C_OPENREPLY | XPC_C_CONNECTED));

		if (args->entry_size == 0 || args->local_nentries == 0) {
			
			goto out;
		}

		ch->flags |= (XPC_C_ROPENREQUEST | XPC_C_CONNECTING);
		ch->remote_nentries = args->local_nentries;

		if (ch->flags & XPC_C_OPENREQUEST) {
			if (args->entry_size != ch->entry_size) {
				XPC_DISCONNECT_CHANNEL(ch, xpUnequalMsgSizes,
						       &irq_flags);
				goto out;
			}
		} else {
			ch->entry_size = args->entry_size;

			XPC_SET_REASON(ch, 0, 0);
			ch->flags &= ~XPC_C_DISCONNECTED;

			atomic_inc(&part->nchannels_active);
		}

		xpc_process_connect(ch, &irq_flags);
	}

	if (chctl_flags & XPC_CHCTL_OPENREPLY) {

		dev_dbg(xpc_chan, "XPC_CHCTL_OPENREPLY (local_msgqueue_pa="
			"0x%lx, local_nentries=%d, remote_nentries=%d) "
			"received from partid=%d, channel=%d\n",
			args->local_msgqueue_pa, args->local_nentries,
			args->remote_nentries, ch->partid, ch->number);

		if (ch->flags & (XPC_C_DISCONNECTING | XPC_C_DISCONNECTED))
			goto out;

		if (!(ch->flags & XPC_C_OPENREQUEST)) {
			XPC_DISCONNECT_CHANNEL(ch, xpOpenCloseError,
					       &irq_flags);
			goto out;
		}

		DBUG_ON(!(ch->flags & XPC_C_ROPENREQUEST));
		DBUG_ON(ch->flags & XPC_C_CONNECTED);

		DBUG_ON(args->local_msgqueue_pa == 0);
		DBUG_ON(args->local_nentries == 0);
		DBUG_ON(args->remote_nentries == 0);

		ret = xpc_arch_ops.save_remote_msgqueue_pa(ch,
						      args->local_msgqueue_pa);
		if (ret != xpSuccess) {
			XPC_DISCONNECT_CHANNEL(ch, ret, &irq_flags);
			goto out;
		}
		ch->flags |= XPC_C_ROPENREPLY;

		if (args->local_nentries < ch->remote_nentries) {
			dev_dbg(xpc_chan, "XPC_CHCTL_OPENREPLY: new "
				"remote_nentries=%d, old remote_nentries=%d, "
				"partid=%d, channel=%d\n",
				args->local_nentries, ch->remote_nentries,
				ch->partid, ch->number);

			ch->remote_nentries = args->local_nentries;
		}
		if (args->remote_nentries < ch->local_nentries) {
			dev_dbg(xpc_chan, "XPC_CHCTL_OPENREPLY: new "
				"local_nentries=%d, old local_nentries=%d, "
				"partid=%d, channel=%d\n",
				args->remote_nentries, ch->local_nentries,
				ch->partid, ch->number);

			ch->local_nentries = args->remote_nentries;
		}

		xpc_process_connect(ch, &irq_flags);
	}

	if (chctl_flags & XPC_CHCTL_OPENCOMPLETE) {

		dev_dbg(xpc_chan, "XPC_CHCTL_OPENCOMPLETE received from "
			"partid=%d, channel=%d\n", ch->partid, ch->number);

		if (ch->flags & (XPC_C_DISCONNECTING | XPC_C_DISCONNECTED))
			goto out;

		if (!(ch->flags & XPC_C_OPENREQUEST) ||
		    !(ch->flags & XPC_C_OPENREPLY)) {
			XPC_DISCONNECT_CHANNEL(ch, xpOpenCloseError,
					       &irq_flags);
			goto out;
		}

		DBUG_ON(!(ch->flags & XPC_C_ROPENREQUEST));
		DBUG_ON(!(ch->flags & XPC_C_ROPENREPLY));
		DBUG_ON(!(ch->flags & XPC_C_CONNECTED));

		ch->flags |= XPC_C_ROPENCOMPLETE;

		xpc_process_connect(ch, &irq_flags);
		create_kthread = 1;
	}

out:
	spin_unlock_irqrestore(&ch->lock, irq_flags);

	if (create_kthread)
		xpc_create_kthreads(ch, 1, 0);
}

static enum xp_retval
xpc_connect_channel(struct xpc_channel *ch)
{
	unsigned long irq_flags;
	struct xpc_registration *registration = &xpc_registrations[ch->number];

	if (mutex_trylock(&registration->mutex) == 0)
		return xpRetry;

	if (!XPC_CHANNEL_REGISTERED(ch->number)) {
		mutex_unlock(&registration->mutex);
		return xpUnregistered;
	}

	spin_lock_irqsave(&ch->lock, irq_flags);

	DBUG_ON(ch->flags & XPC_C_CONNECTED);
	DBUG_ON(ch->flags & XPC_C_OPENREQUEST);

	if (ch->flags & XPC_C_DISCONNECTING) {
		spin_unlock_irqrestore(&ch->lock, irq_flags);
		mutex_unlock(&registration->mutex);
		return ch->reason;
	}

	

	ch->kthreads_assigned_limit = registration->assigned_limit;
	ch->kthreads_idle_limit = registration->idle_limit;
	DBUG_ON(atomic_read(&ch->kthreads_assigned) != 0);
	DBUG_ON(atomic_read(&ch->kthreads_idle) != 0);
	DBUG_ON(atomic_read(&ch->kthreads_active) != 0);

	ch->func = registration->func;
	DBUG_ON(registration->func == NULL);
	ch->key = registration->key;

	ch->local_nentries = registration->nentries;

	if (ch->flags & XPC_C_ROPENREQUEST) {
		if (registration->entry_size != ch->entry_size) {
			

			mutex_unlock(&registration->mutex);
			XPC_DISCONNECT_CHANNEL(ch, xpUnequalMsgSizes,
					       &irq_flags);
			spin_unlock_irqrestore(&ch->lock, irq_flags);
			return xpUnequalMsgSizes;
		}
	} else {
		ch->entry_size = registration->entry_size;

		XPC_SET_REASON(ch, 0, 0);
		ch->flags &= ~XPC_C_DISCONNECTED;

		atomic_inc(&xpc_partitions[ch->partid].nchannels_active);
	}

	mutex_unlock(&registration->mutex);

	

	ch->flags |= (XPC_C_OPENREQUEST | XPC_C_CONNECTING);
	xpc_arch_ops.send_chctl_openrequest(ch, &irq_flags);

	xpc_process_connect(ch, &irq_flags);

	spin_unlock_irqrestore(&ch->lock, irq_flags);

	return xpSuccess;
}

void
xpc_process_sent_chctl_flags(struct xpc_partition *part)
{
	unsigned long irq_flags;
	union xpc_channel_ctl_flags chctl;
	struct xpc_channel *ch;
	int ch_number;
	u32 ch_flags;

	chctl.all_flags = xpc_arch_ops.get_chctl_all_flags(part);


	for (ch_number = 0; ch_number < part->nchannels; ch_number++) {
		ch = &part->channels[ch_number];


		if (chctl.flags[ch_number] & XPC_OPENCLOSE_CHCTL_FLAGS) {
			xpc_process_openclose_chctl_flags(part, ch_number,
							chctl.flags[ch_number]);
		}

		ch_flags = ch->flags;	

		if (ch_flags & XPC_C_DISCONNECTING) {
			spin_lock_irqsave(&ch->lock, irq_flags);
			xpc_process_disconnect(ch, &irq_flags);
			spin_unlock_irqrestore(&ch->lock, irq_flags);
			continue;
		}

		if (part->act_state == XPC_P_AS_DEACTIVATING)
			continue;

		if (!(ch_flags & XPC_C_CONNECTED)) {
			if (!(ch_flags & XPC_C_OPENREQUEST)) {
				DBUG_ON(ch_flags & XPC_C_SETUP);
				(void)xpc_connect_channel(ch);
			}
			continue;
		}


		if (chctl.flags[ch_number] & XPC_MSG_CHCTL_FLAGS)
			xpc_arch_ops.process_msg_chctl_flags(part, ch_number);
	}
}

void
xpc_partition_going_down(struct xpc_partition *part, enum xp_retval reason)
{
	unsigned long irq_flags;
	int ch_number;
	struct xpc_channel *ch;

	dev_dbg(xpc_chan, "deactivating partition %d, reason=%d\n",
		XPC_PARTID(part), reason);

	if (!xpc_part_ref(part)) {
		
		return;
	}

	

	for (ch_number = 0; ch_number < part->nchannels; ch_number++) {
		ch = &part->channels[ch_number];

		xpc_msgqueue_ref(ch);
		spin_lock_irqsave(&ch->lock, irq_flags);

		XPC_DISCONNECT_CHANNEL(ch, reason, &irq_flags);

		spin_unlock_irqrestore(&ch->lock, irq_flags);
		xpc_msgqueue_deref(ch);
	}

	xpc_wakeup_channel_mgr(part);

	xpc_part_deref(part);
}

void
xpc_initiate_connect(int ch_number)
{
	short partid;
	struct xpc_partition *part;
	struct xpc_channel *ch;

	DBUG_ON(ch_number < 0 || ch_number >= XPC_MAX_NCHANNELS);

	for (partid = 0; partid < xp_max_npartitions; partid++) {
		part = &xpc_partitions[partid];

		if (xpc_part_ref(part)) {
			ch = &part->channels[ch_number];

			xpc_wakeup_channel_mgr(part);
			xpc_part_deref(part);
		}
	}
}

void
xpc_connected_callout(struct xpc_channel *ch)
{
	

	if (ch->func != NULL) {
		dev_dbg(xpc_chan, "ch->func() called, reason=xpConnected, "
			"partid=%d, channel=%d\n", ch->partid, ch->number);

		ch->func(xpConnected, ch->partid, ch->number,
			 (void *)(u64)ch->local_nentries, ch->key);

		dev_dbg(xpc_chan, "ch->func() returned, reason=xpConnected, "
			"partid=%d, channel=%d\n", ch->partid, ch->number);
	}
}

void
xpc_initiate_disconnect(int ch_number)
{
	unsigned long irq_flags;
	short partid;
	struct xpc_partition *part;
	struct xpc_channel *ch;

	DBUG_ON(ch_number < 0 || ch_number >= XPC_MAX_NCHANNELS);

	
	for (partid = 0; partid < xp_max_npartitions; partid++) {
		part = &xpc_partitions[partid];

		if (xpc_part_ref(part)) {
			ch = &part->channels[ch_number];
			xpc_msgqueue_ref(ch);

			spin_lock_irqsave(&ch->lock, irq_flags);

			if (!(ch->flags & XPC_C_DISCONNECTED)) {
				ch->flags |= XPC_C_WDISCONNECT;

				XPC_DISCONNECT_CHANNEL(ch, xpUnregistering,
						       &irq_flags);
			}

			spin_unlock_irqrestore(&ch->lock, irq_flags);

			xpc_msgqueue_deref(ch);
			xpc_part_deref(part);
		}
	}

	xpc_disconnect_wait(ch_number);
}

void
xpc_disconnect_channel(const int line, struct xpc_channel *ch,
		       enum xp_retval reason, unsigned long *irq_flags)
{
	u32 channel_was_connected = (ch->flags & XPC_C_CONNECTED);

	DBUG_ON(!spin_is_locked(&ch->lock));

	if (ch->flags & (XPC_C_DISCONNECTING | XPC_C_DISCONNECTED))
		return;

	DBUG_ON(!(ch->flags & (XPC_C_CONNECTING | XPC_C_CONNECTED)));

	dev_dbg(xpc_chan, "reason=%d, line=%d, partid=%d, channel=%d\n",
		reason, line, ch->partid, ch->number);

	XPC_SET_REASON(ch, reason, line);

	ch->flags |= (XPC_C_CLOSEREQUEST | XPC_C_DISCONNECTING);
	
	ch->flags &= ~(XPC_C_OPENREQUEST | XPC_C_OPENREPLY |
		       XPC_C_ROPENREQUEST | XPC_C_ROPENREPLY |
		       XPC_C_CONNECTING | XPC_C_CONNECTED);

	xpc_arch_ops.send_chctl_closerequest(ch, irq_flags);

	if (channel_was_connected)
		ch->flags |= XPC_C_WASCONNECTED;

	spin_unlock_irqrestore(&ch->lock, *irq_flags);

	
	if (atomic_read(&ch->kthreads_idle) > 0) {
		wake_up_all(&ch->idle_wq);

	} else if ((ch->flags & XPC_C_CONNECTEDCALLOUT_MADE) &&
		   !(ch->flags & XPC_C_DISCONNECTINGCALLOUT)) {
		
		xpc_create_kthreads(ch, 1, 1);
	}

	
	if (atomic_read(&ch->n_on_msg_allocate_wq) > 0)
		wake_up(&ch->msg_allocate_wq);

	spin_lock_irqsave(&ch->lock, *irq_flags);
}

void
xpc_disconnect_callout(struct xpc_channel *ch, enum xp_retval reason)
{

	if (ch->func != NULL) {
		dev_dbg(xpc_chan, "ch->func() called, reason=%d, partid=%d, "
			"channel=%d\n", reason, ch->partid, ch->number);

		ch->func(reason, ch->partid, ch->number, NULL, ch->key);

		dev_dbg(xpc_chan, "ch->func() returned, reason=%d, partid=%d, "
			"channel=%d\n", reason, ch->partid, ch->number);
	}
}

enum xp_retval
xpc_allocate_msg_wait(struct xpc_channel *ch)
{
	enum xp_retval ret;

	if (ch->flags & XPC_C_DISCONNECTING) {
		DBUG_ON(ch->reason == xpInterrupted);
		return ch->reason;
	}

	atomic_inc(&ch->n_on_msg_allocate_wq);
	ret = interruptible_sleep_on_timeout(&ch->msg_allocate_wq, 1);
	atomic_dec(&ch->n_on_msg_allocate_wq);

	if (ch->flags & XPC_C_DISCONNECTING) {
		ret = ch->reason;
		DBUG_ON(ch->reason == xpInterrupted);
	} else if (ret == 0) {
		ret = xpTimeout;
	} else {
		ret = xpInterrupted;
	}

	return ret;
}

enum xp_retval
xpc_initiate_send(short partid, int ch_number, u32 flags, void *payload,
		  u16 payload_size)
{
	struct xpc_partition *part = &xpc_partitions[partid];
	enum xp_retval ret = xpUnknownReason;

	dev_dbg(xpc_chan, "payload=0x%p, partid=%d, channel=%d\n", payload,
		partid, ch_number);

	DBUG_ON(partid < 0 || partid >= xp_max_npartitions);
	DBUG_ON(ch_number < 0 || ch_number >= part->nchannels);
	DBUG_ON(payload == NULL);

	if (xpc_part_ref(part)) {
		ret = xpc_arch_ops.send_payload(&part->channels[ch_number],
				  flags, payload, payload_size, 0, NULL, NULL);
		xpc_part_deref(part);
	}

	return ret;
}

enum xp_retval
xpc_initiate_send_notify(short partid, int ch_number, u32 flags, void *payload,
			 u16 payload_size, xpc_notify_func func, void *key)
{
	struct xpc_partition *part = &xpc_partitions[partid];
	enum xp_retval ret = xpUnknownReason;

	dev_dbg(xpc_chan, "payload=0x%p, partid=%d, channel=%d\n", payload,
		partid, ch_number);

	DBUG_ON(partid < 0 || partid >= xp_max_npartitions);
	DBUG_ON(ch_number < 0 || ch_number >= part->nchannels);
	DBUG_ON(payload == NULL);
	DBUG_ON(func == NULL);

	if (xpc_part_ref(part)) {
		ret = xpc_arch_ops.send_payload(&part->channels[ch_number],
			  flags, payload, payload_size, XPC_N_CALL, func, key);
		xpc_part_deref(part);
	}
	return ret;
}

void
xpc_deliver_payload(struct xpc_channel *ch)
{
	void *payload;

	payload = xpc_arch_ops.get_deliverable_payload(ch);
	if (payload != NULL) {

		xpc_msgqueue_ref(ch);

		atomic_inc(&ch->kthreads_active);

		if (ch->func != NULL) {
			dev_dbg(xpc_chan, "ch->func() called, payload=0x%p "
				"partid=%d channel=%d\n", payload, ch->partid,
				ch->number);

			
			ch->func(xpMsgReceived, ch->partid, ch->number, payload,
				 ch->key);

			dev_dbg(xpc_chan, "ch->func() returned, payload=0x%p "
				"partid=%d channel=%d\n", payload, ch->partid,
				ch->number);
		}

		atomic_dec(&ch->kthreads_active);
	}
}

void
xpc_initiate_received(short partid, int ch_number, void *payload)
{
	struct xpc_partition *part = &xpc_partitions[partid];
	struct xpc_channel *ch;

	DBUG_ON(partid < 0 || partid >= xp_max_npartitions);
	DBUG_ON(ch_number < 0 || ch_number >= part->nchannels);

	ch = &part->channels[ch_number];
	xpc_arch_ops.received_payload(ch, payload);

	
	xpc_msgqueue_deref(ch);
}
