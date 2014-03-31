/******************************************************************************
*******************************************************************************
**
**  Copyright (C) 2005-2007 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#include "dlm_internal.h"
#include "member.h"
#include "lock.h"
#include "dir.h"
#include "config.h"
#include "requestqueue.h"

struct rq_entry {
	struct list_head list;
	int nodeid;
	struct dlm_message request;
};


void dlm_add_requestqueue(struct dlm_ls *ls, int nodeid, struct dlm_message *ms)
{
	struct rq_entry *e;
	int length = ms->m_header.h_length - sizeof(struct dlm_message);

	e = kmalloc(sizeof(struct rq_entry) + length, GFP_NOFS);
	if (!e) {
		log_print("dlm_add_requestqueue: out of memory len %d", length);
		return;
	}

	e->nodeid = nodeid;
	memcpy(&e->request, ms, ms->m_header.h_length);

	mutex_lock(&ls->ls_requestqueue_mutex);
	list_add_tail(&e->list, &ls->ls_requestqueue);
	mutex_unlock(&ls->ls_requestqueue_mutex);
}


int dlm_process_requestqueue(struct dlm_ls *ls)
{
	struct rq_entry *e;
	int error = 0;

	mutex_lock(&ls->ls_requestqueue_mutex);

	for (;;) {
		if (list_empty(&ls->ls_requestqueue)) {
			mutex_unlock(&ls->ls_requestqueue_mutex);
			error = 0;
			break;
		}
		e = list_entry(ls->ls_requestqueue.next, struct rq_entry, list);
		mutex_unlock(&ls->ls_requestqueue_mutex);

		dlm_receive_message_saved(ls, &e->request);

		mutex_lock(&ls->ls_requestqueue_mutex);
		list_del(&e->list);
		kfree(e);

		if (dlm_locking_stopped(ls)) {
			log_debug(ls, "process_requestqueue abort running");
			mutex_unlock(&ls->ls_requestqueue_mutex);
			error = -EINTR;
			break;
		}
		schedule();
	}

	return error;
}


void dlm_wait_requestqueue(struct dlm_ls *ls)
{
	for (;;) {
		mutex_lock(&ls->ls_requestqueue_mutex);
		if (list_empty(&ls->ls_requestqueue))
			break;
		mutex_unlock(&ls->ls_requestqueue_mutex);
		schedule();
	}
	mutex_unlock(&ls->ls_requestqueue_mutex);
}

static int purge_request(struct dlm_ls *ls, struct dlm_message *ms, int nodeid)
{
	uint32_t type = ms->m_type;

	
	if (!ls->ls_count)
		return 1;

	if (dlm_is_removed(ls, nodeid))
		return 1;


	if (type == DLM_MSG_REMOVE ||
	    type == DLM_MSG_LOOKUP ||
	    type == DLM_MSG_LOOKUP_REPLY)
		return 1;

	if (!dlm_no_directory(ls))
		return 0;


	switch (type) {
	case DLM_MSG_REQUEST:
	case DLM_MSG_CONVERT:
	case DLM_MSG_UNLOCK:
	case DLM_MSG_CANCEL:

		if (dlm_hash2nodeid(ls, ms->m_hash) != dlm_our_nodeid())
			return 1;
		break;

	case DLM_MSG_REQUEST_REPLY:
	case DLM_MSG_CONVERT_REPLY:
	case DLM_MSG_UNLOCK_REPLY:
	case DLM_MSG_CANCEL_REPLY:
	case DLM_MSG_GRANT:

		if (dlm_hash2nodeid(ls, ms->m_hash) != nodeid)
			return 1;
		break;
	}

	return 0;
}

void dlm_purge_requestqueue(struct dlm_ls *ls)
{
	struct dlm_message *ms;
	struct rq_entry *e, *safe;

	mutex_lock(&ls->ls_requestqueue_mutex);
	list_for_each_entry_safe(e, safe, &ls->ls_requestqueue, list) {
		ms =  &e->request;

		if (purge_request(ls, ms, e->nodeid)) {
			list_del(&e->list);
			kfree(e);
		}
	}
	mutex_unlock(&ls->ls_requestqueue_mutex);
}

