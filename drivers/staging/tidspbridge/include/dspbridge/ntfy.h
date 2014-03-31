/*
 * ntfy.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Manage lists of notification events.
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

#ifndef NTFY_
#define NTFY_

#include <dspbridge/host_os.h>
#include <dspbridge/dbdefs.h>
#include <dspbridge/sync.h>

struct ntfy_object {
	struct raw_notifier_head head;
	spinlock_t ntfy_lock;	
};

struct ntfy_event {
	struct notifier_block noti_block;
	u32 event;	
	u32 type;	
	struct sync_object sync_obj;
};


int dsp_notifier_event(struct notifier_block *this, unsigned long event,
			   void *data);


static inline void ntfy_init(struct ntfy_object *no)
{
	spin_lock_init(&no->ntfy_lock);
	RAW_INIT_NOTIFIER_HEAD(&no->head);
}

static inline void ntfy_delete(struct ntfy_object *ntfy_obj)
{
	struct ntfy_event *ne;
	struct notifier_block *nb;

	spin_lock_bh(&ntfy_obj->ntfy_lock);
	nb = ntfy_obj->head.head;
	while (nb) {
		ne = container_of(nb, struct ntfy_event, noti_block);
		nb = nb->next;
		kfree(ne);
	}
	spin_unlock_bh(&ntfy_obj->ntfy_lock);
}

static inline void ntfy_notify(struct ntfy_object *ntfy_obj, u32 event)
{
	spin_lock_bh(&ntfy_obj->ntfy_lock);
	raw_notifier_call_chain(&ntfy_obj->head, event, NULL);
	spin_unlock_bh(&ntfy_obj->ntfy_lock);
}




static inline struct ntfy_event *ntfy_event_create(u32 event, u32 type)
{
	struct ntfy_event *ne;
	ne = kmalloc(sizeof(struct ntfy_event), GFP_KERNEL);
	if (ne) {
		sync_init_event(&ne->sync_obj);
		ne->noti_block.notifier_call = dsp_notifier_event;
		ne->event = event;
		ne->type = type;
	}
	return ne;
}

static  inline int ntfy_register(struct ntfy_object *ntfy_obj,
			 struct dsp_notification *noti,
			 u32 event, u32 type)
{
	struct ntfy_event *ne;
	int status = 0;

	if (!noti || !ntfy_obj) {
		status = -EFAULT;
		goto func_end;
	}
	if (!event) {
		status = -EINVAL;
		goto func_end;
	}
	ne = ntfy_event_create(event, type);
	if (!ne) {
		status = -ENOMEM;
		goto func_end;
	}
	noti->handle = &ne->sync_obj;

	spin_lock_bh(&ntfy_obj->ntfy_lock);
	raw_notifier_chain_register(&ntfy_obj->head, &ne->noti_block);
	spin_unlock_bh(&ntfy_obj->ntfy_lock);
func_end:
	return status;
}

static  inline int ntfy_unregister(struct ntfy_object *ntfy_obj,
			 struct dsp_notification *noti)
{
	int status = 0;
	struct ntfy_event *ne;

	if (!noti || !ntfy_obj) {
		status = -EFAULT;
		goto func_end;
	}

	ne = container_of((struct sync_object *)noti, struct ntfy_event,
								sync_obj);
	spin_lock_bh(&ntfy_obj->ntfy_lock);
	raw_notifier_chain_unregister(&ntfy_obj->head,
						&ne->noti_block);
	kfree(ne);
	spin_unlock_bh(&ntfy_obj->ntfy_lock);
func_end:
	return status;
}

#endif				
