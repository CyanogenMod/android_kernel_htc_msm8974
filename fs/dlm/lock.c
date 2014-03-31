/******************************************************************************
*******************************************************************************
**
**  Copyright (C) 2005-2010 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#include <linux/types.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include "dlm_internal.h"
#include <linux/dlm_device.h>
#include "memory.h"
#include "lowcomms.h"
#include "requestqueue.h"
#include "util.h"
#include "dir.h"
#include "member.h"
#include "lockspace.h"
#include "ast.h"
#include "lock.h"
#include "rcom.h"
#include "recover.h"
#include "lvb_table.h"
#include "user.h"
#include "config.h"

static int send_request(struct dlm_rsb *r, struct dlm_lkb *lkb);
static int send_convert(struct dlm_rsb *r, struct dlm_lkb *lkb);
static int send_unlock(struct dlm_rsb *r, struct dlm_lkb *lkb);
static int send_cancel(struct dlm_rsb *r, struct dlm_lkb *lkb);
static int send_grant(struct dlm_rsb *r, struct dlm_lkb *lkb);
static int send_bast(struct dlm_rsb *r, struct dlm_lkb *lkb, int mode);
static int send_lookup(struct dlm_rsb *r, struct dlm_lkb *lkb);
static int send_remove(struct dlm_rsb *r);
static int _request_lock(struct dlm_rsb *r, struct dlm_lkb *lkb);
static int _cancel_lock(struct dlm_rsb *r, struct dlm_lkb *lkb);
static void __receive_convert_reply(struct dlm_rsb *r, struct dlm_lkb *lkb,
				    struct dlm_message *ms);
static int receive_extralen(struct dlm_message *ms);
static void do_purge(struct dlm_ls *ls, int nodeid, int pid);
static void del_timeout(struct dlm_lkb *lkb);


static const int __dlm_compat_matrix[8][8] = {
      
        {1, 1, 1, 1, 1, 1, 1, 0},       
        {1, 1, 1, 1, 1, 1, 1, 0},       
        {1, 1, 1, 1, 1, 1, 0, 0},       
        {1, 1, 1, 1, 0, 0, 0, 0},       
        {1, 1, 1, 0, 1, 0, 0, 0},       
        {1, 1, 1, 0, 0, 0, 0, 0},       
        {1, 1, 0, 0, 0, 0, 0, 0},       
        {0, 0, 0, 0, 0, 0, 0, 0}        
};

/*
 * This defines the direction of transfer of LVB data.
 * Granted mode is the row; requested mode is the column.
 * Usage: matrix[grmode+1][rqmode+1]
 * 1 = LVB is returned to the caller
 * 0 = LVB is written to the resource
 * -1 = nothing happens to the LVB
 */

const int dlm_lvb_operations[8][8] = {
        
        {  -1,  1,  1,  1,  1,  1,  1, -1 }, 
        {  -1,  1,  1,  1,  1,  1,  1,  0 }, 
        {  -1, -1,  1,  1,  1,  1,  1,  0 }, 
        {  -1, -1, -1,  1,  1,  1,  1,  0 }, 
        {  -1, -1, -1, -1,  1,  1,  1,  0 }, 
        {  -1,  0,  0,  0,  0,  0,  1,  0 }, 
        {  -1,  0,  0,  0,  0,  0,  0,  0 }, 
        {  -1,  0,  0,  0,  0,  0,  0,  0 }  
};

#define modes_compat(gr, rq) \
	__dlm_compat_matrix[(gr)->lkb_grmode + 1][(rq)->lkb_rqmode + 1]

int dlm_modes_compat(int mode1, int mode2)
{
	return __dlm_compat_matrix[mode1 + 1][mode2 + 1];
}


static const int __quecvt_compat_matrix[8][8] = {
      
        {0, 0, 0, 0, 0, 0, 0, 0},       
        {0, 0, 1, 1, 1, 1, 1, 0},       
        {0, 0, 0, 1, 1, 1, 1, 0},       
        {0, 0, 0, 0, 1, 1, 1, 0},       
        {0, 0, 0, 1, 0, 1, 1, 0},       
        {0, 0, 0, 0, 0, 0, 1, 0},       
        {0, 0, 0, 0, 0, 0, 0, 0},       
        {0, 0, 0, 0, 0, 0, 0, 0}        
};

void dlm_print_lkb(struct dlm_lkb *lkb)
{
	printk(KERN_ERR "lkb: nodeid %d id %x remid %x exflags %x flags %x\n"
	       "     status %d rqmode %d grmode %d wait_type %d\n",
	       lkb->lkb_nodeid, lkb->lkb_id, lkb->lkb_remid, lkb->lkb_exflags,
	       lkb->lkb_flags, lkb->lkb_status, lkb->lkb_rqmode,
	       lkb->lkb_grmode, lkb->lkb_wait_type);
}

static void dlm_print_rsb(struct dlm_rsb *r)
{
	printk(KERN_ERR "rsb: nodeid %d flags %lx first %x rlc %d name %s\n",
	       r->res_nodeid, r->res_flags, r->res_first_lkid,
	       r->res_recover_locks_count, r->res_name);
}

void dlm_dump_rsb(struct dlm_rsb *r)
{
	struct dlm_lkb *lkb;

	dlm_print_rsb(r);

	printk(KERN_ERR "rsb: root_list empty %d recover_list empty %d\n",
	       list_empty(&r->res_root_list), list_empty(&r->res_recover_list));
	printk(KERN_ERR "rsb lookup list\n");
	list_for_each_entry(lkb, &r->res_lookup, lkb_rsb_lookup)
		dlm_print_lkb(lkb);
	printk(KERN_ERR "rsb grant queue:\n");
	list_for_each_entry(lkb, &r->res_grantqueue, lkb_statequeue)
		dlm_print_lkb(lkb);
	printk(KERN_ERR "rsb convert queue:\n");
	list_for_each_entry(lkb, &r->res_convertqueue, lkb_statequeue)
		dlm_print_lkb(lkb);
	printk(KERN_ERR "rsb wait queue:\n");
	list_for_each_entry(lkb, &r->res_waitqueue, lkb_statequeue)
		dlm_print_lkb(lkb);
}


static inline void dlm_lock_recovery(struct dlm_ls *ls)
{
	down_read(&ls->ls_in_recovery);
}

void dlm_unlock_recovery(struct dlm_ls *ls)
{
	up_read(&ls->ls_in_recovery);
}

int dlm_lock_recovery_try(struct dlm_ls *ls)
{
	return down_read_trylock(&ls->ls_in_recovery);
}

static inline int can_be_queued(struct dlm_lkb *lkb)
{
	return !(lkb->lkb_exflags & DLM_LKF_NOQUEUE);
}

static inline int force_blocking_asts(struct dlm_lkb *lkb)
{
	return (lkb->lkb_exflags & DLM_LKF_NOQUEUEBAST);
}

static inline int is_demoted(struct dlm_lkb *lkb)
{
	return (lkb->lkb_sbflags & DLM_SBF_DEMOTED);
}

static inline int is_altmode(struct dlm_lkb *lkb)
{
	return (lkb->lkb_sbflags & DLM_SBF_ALTMODE);
}

static inline int is_granted(struct dlm_lkb *lkb)
{
	return (lkb->lkb_status == DLM_LKSTS_GRANTED);
}

static inline int is_remote(struct dlm_rsb *r)
{
	DLM_ASSERT(r->res_nodeid >= 0, dlm_print_rsb(r););
	return !!r->res_nodeid;
}

static inline int is_process_copy(struct dlm_lkb *lkb)
{
	return (lkb->lkb_nodeid && !(lkb->lkb_flags & DLM_IFL_MSTCPY));
}

static inline int is_master_copy(struct dlm_lkb *lkb)
{
	if (lkb->lkb_flags & DLM_IFL_MSTCPY)
		DLM_ASSERT(lkb->lkb_nodeid, dlm_print_lkb(lkb););
	return (lkb->lkb_flags & DLM_IFL_MSTCPY) ? 1 : 0;
}

static inline int middle_conversion(struct dlm_lkb *lkb)
{
	if ((lkb->lkb_grmode==DLM_LOCK_PR && lkb->lkb_rqmode==DLM_LOCK_CW) ||
	    (lkb->lkb_rqmode==DLM_LOCK_PR && lkb->lkb_grmode==DLM_LOCK_CW))
		return 1;
	return 0;
}

static inline int down_conversion(struct dlm_lkb *lkb)
{
	return (!middle_conversion(lkb) && lkb->lkb_rqmode < lkb->lkb_grmode);
}

static inline int is_overlap_unlock(struct dlm_lkb *lkb)
{
	return lkb->lkb_flags & DLM_IFL_OVERLAP_UNLOCK;
}

static inline int is_overlap_cancel(struct dlm_lkb *lkb)
{
	return lkb->lkb_flags & DLM_IFL_OVERLAP_CANCEL;
}

static inline int is_overlap(struct dlm_lkb *lkb)
{
	return (lkb->lkb_flags & (DLM_IFL_OVERLAP_UNLOCK |
				  DLM_IFL_OVERLAP_CANCEL));
}

static void queue_cast(struct dlm_rsb *r, struct dlm_lkb *lkb, int rv)
{
	if (is_master_copy(lkb))
		return;

	del_timeout(lkb);

	DLM_ASSERT(lkb->lkb_lksb, dlm_print_lkb(lkb););

	if (rv == -DLM_ECANCEL && (lkb->lkb_flags & DLM_IFL_TIMEOUT_CANCEL)) {
		lkb->lkb_flags &= ~DLM_IFL_TIMEOUT_CANCEL;
		rv = -ETIMEDOUT;
	}

	if (rv == -DLM_ECANCEL && (lkb->lkb_flags & DLM_IFL_DEADLOCK_CANCEL)) {
		lkb->lkb_flags &= ~DLM_IFL_DEADLOCK_CANCEL;
		rv = -EDEADLK;
	}

	dlm_add_cb(lkb, DLM_CB_CAST, lkb->lkb_grmode, rv, lkb->lkb_sbflags);
}

static inline void queue_cast_overlap(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	queue_cast(r, lkb,
		   is_overlap_unlock(lkb) ? -DLM_EUNLOCK : -DLM_ECANCEL);
}

static void queue_bast(struct dlm_rsb *r, struct dlm_lkb *lkb, int rqmode)
{
	if (is_master_copy(lkb)) {
		send_bast(r, lkb, rqmode);
	} else {
		dlm_add_cb(lkb, DLM_CB_BAST, rqmode, 0, 0);
	}
}


static int pre_rsb_struct(struct dlm_ls *ls)
{
	struct dlm_rsb *r1, *r2;
	int count = 0;

	spin_lock(&ls->ls_new_rsb_spin);
	if (ls->ls_new_rsb_count > dlm_config.ci_new_rsb_count / 2) {
		spin_unlock(&ls->ls_new_rsb_spin);
		return 0;
	}
	spin_unlock(&ls->ls_new_rsb_spin);

	r1 = dlm_allocate_rsb(ls);
	r2 = dlm_allocate_rsb(ls);

	spin_lock(&ls->ls_new_rsb_spin);
	if (r1) {
		list_add(&r1->res_hashchain, &ls->ls_new_rsb);
		ls->ls_new_rsb_count++;
	}
	if (r2) {
		list_add(&r2->res_hashchain, &ls->ls_new_rsb);
		ls->ls_new_rsb_count++;
	}
	count = ls->ls_new_rsb_count;
	spin_unlock(&ls->ls_new_rsb_spin);

	if (!count)
		return -ENOMEM;
	return 0;
}


static int get_rsb_struct(struct dlm_ls *ls, char *name, int len,
			  struct dlm_rsb **r_ret)
{
	struct dlm_rsb *r;
	int count;

	spin_lock(&ls->ls_new_rsb_spin);
	if (list_empty(&ls->ls_new_rsb)) {
		count = ls->ls_new_rsb_count;
		spin_unlock(&ls->ls_new_rsb_spin);
		log_debug(ls, "find_rsb retry %d %d %s",
			  count, dlm_config.ci_new_rsb_count, name);
		return -EAGAIN;
	}

	r = list_first_entry(&ls->ls_new_rsb, struct dlm_rsb, res_hashchain);
	list_del(&r->res_hashchain);
	
	memset(&r->res_hashnode, 0, sizeof(struct rb_node));
	ls->ls_new_rsb_count--;
	spin_unlock(&ls->ls_new_rsb_spin);

	r->res_ls = ls;
	r->res_length = len;
	memcpy(r->res_name, name, len);
	mutex_init(&r->res_mutex);

	INIT_LIST_HEAD(&r->res_lookup);
	INIT_LIST_HEAD(&r->res_grantqueue);
	INIT_LIST_HEAD(&r->res_convertqueue);
	INIT_LIST_HEAD(&r->res_waitqueue);
	INIT_LIST_HEAD(&r->res_root_list);
	INIT_LIST_HEAD(&r->res_recover_list);

	*r_ret = r;
	return 0;
}

static int rsb_cmp(struct dlm_rsb *r, const char *name, int nlen)
{
	char maxname[DLM_RESNAME_MAXLEN];

	memset(maxname, 0, DLM_RESNAME_MAXLEN);
	memcpy(maxname, name, nlen);
	return memcmp(r->res_name, maxname, DLM_RESNAME_MAXLEN);
}

int dlm_search_rsb_tree(struct rb_root *tree, char *name, int len,
			unsigned int flags, struct dlm_rsb **r_ret)
{
	struct rb_node *node = tree->rb_node;
	struct dlm_rsb *r;
	int error = 0;
	int rc;

	while (node) {
		r = rb_entry(node, struct dlm_rsb, res_hashnode);
		rc = rsb_cmp(r, name, len);
		if (rc < 0)
			node = node->rb_left;
		else if (rc > 0)
			node = node->rb_right;
		else
			goto found;
	}
	*r_ret = NULL;
	return -EBADR;

 found:
	if (r->res_nodeid && (flags & R_MASTER))
		error = -ENOTBLK;
	*r_ret = r;
	return error;
}

static int rsb_insert(struct dlm_rsb *rsb, struct rb_root *tree)
{
	struct rb_node **newn = &tree->rb_node;
	struct rb_node *parent = NULL;
	int rc;

	while (*newn) {
		struct dlm_rsb *cur = rb_entry(*newn, struct dlm_rsb,
					       res_hashnode);

		parent = *newn;
		rc = rsb_cmp(cur, rsb->res_name, rsb->res_length);
		if (rc < 0)
			newn = &parent->rb_left;
		else if (rc > 0)
			newn = &parent->rb_right;
		else {
			log_print("rsb_insert match");
			dlm_dump_rsb(rsb);
			dlm_dump_rsb(cur);
			return -EEXIST;
		}
	}

	rb_link_node(&rsb->res_hashnode, parent, newn);
	rb_insert_color(&rsb->res_hashnode, tree);
	return 0;
}

static int _search_rsb(struct dlm_ls *ls, char *name, int len, int b,
		       unsigned int flags, struct dlm_rsb **r_ret)
{
	struct dlm_rsb *r;
	int error;

	error = dlm_search_rsb_tree(&ls->ls_rsbtbl[b].keep, name, len, flags, &r);
	if (!error) {
		kref_get(&r->res_ref);
		goto out;
	}
	error = dlm_search_rsb_tree(&ls->ls_rsbtbl[b].toss, name, len, flags, &r);
	if (error)
		goto out;

	rb_erase(&r->res_hashnode, &ls->ls_rsbtbl[b].toss);
	error = rsb_insert(r, &ls->ls_rsbtbl[b].keep);
	if (error)
		return error;

	if (dlm_no_directory(ls))
		goto out;

	if (r->res_nodeid == -1) {
		rsb_clear_flag(r, RSB_MASTER_UNCERTAIN);
		r->res_first_lkid = 0;
	} else if (r->res_nodeid > 0) {
		rsb_set_flag(r, RSB_MASTER_UNCERTAIN);
		r->res_first_lkid = 0;
	} else {
		DLM_ASSERT(r->res_nodeid == 0, dlm_print_rsb(r););
		DLM_ASSERT(!rsb_flag(r, RSB_MASTER_UNCERTAIN),);
	}
 out:
	*r_ret = r;
	return error;
}


static int find_rsb(struct dlm_ls *ls, char *name, int namelen,
		    unsigned int flags, struct dlm_rsb **r_ret)
{
	struct dlm_rsb *r = NULL;
	uint32_t hash, bucket;
	int error;

	if (namelen > DLM_RESNAME_MAXLEN) {
		error = -EINVAL;
		goto out;
	}

	if (dlm_no_directory(ls))
		flags |= R_CREATE;

	hash = jhash(name, namelen, 0);
	bucket = hash & (ls->ls_rsbtbl_size - 1);

 retry:
	if (flags & R_CREATE) {
		error = pre_rsb_struct(ls);
		if (error < 0)
			goto out;
	}

	spin_lock(&ls->ls_rsbtbl[bucket].lock);

	error = _search_rsb(ls, name, namelen, bucket, flags, &r);
	if (!error)
		goto out_unlock;

	if (error == -EBADR && !(flags & R_CREATE))
		goto out_unlock;

	
	if (error == -ENOTBLK)
		goto out_unlock;

	error = get_rsb_struct(ls, name, namelen, &r);
	if (error == -EAGAIN) {
		spin_unlock(&ls->ls_rsbtbl[bucket].lock);
		goto retry;
	}
	if (error)
		goto out_unlock;

	r->res_hash = hash;
	r->res_bucket = bucket;
	r->res_nodeid = -1;
	kref_init(&r->res_ref);

	
	if (dlm_no_directory(ls)) {
		int nodeid = dlm_dir_nodeid(r);
		if (nodeid == dlm_our_nodeid())
			nodeid = 0;
		r->res_nodeid = nodeid;
	}
	error = rsb_insert(r, &ls->ls_rsbtbl[bucket].keep);
 out_unlock:
	spin_unlock(&ls->ls_rsbtbl[bucket].lock);
 out:
	*r_ret = r;
	return error;
}


static inline void hold_rsb(struct dlm_rsb *r)
{
	kref_get(&r->res_ref);
}

void dlm_hold_rsb(struct dlm_rsb *r)
{
	hold_rsb(r);
}

static void toss_rsb(struct kref *kref)
{
	struct dlm_rsb *r = container_of(kref, struct dlm_rsb, res_ref);
	struct dlm_ls *ls = r->res_ls;

	DLM_ASSERT(list_empty(&r->res_root_list), dlm_print_rsb(r););
	kref_init(&r->res_ref);
	rb_erase(&r->res_hashnode, &ls->ls_rsbtbl[r->res_bucket].keep);
	rsb_insert(r, &ls->ls_rsbtbl[r->res_bucket].toss);
	r->res_toss_time = jiffies;
	if (r->res_lvbptr) {
		dlm_free_lvb(r->res_lvbptr);
		r->res_lvbptr = NULL;
	}
}


static void put_rsb(struct dlm_rsb *r)
{
	struct dlm_ls *ls = r->res_ls;
	uint32_t bucket = r->res_bucket;

	spin_lock(&ls->ls_rsbtbl[bucket].lock);
	kref_put(&r->res_ref, toss_rsb);
	spin_unlock(&ls->ls_rsbtbl[bucket].lock);
}

void dlm_put_rsb(struct dlm_rsb *r)
{
	put_rsb(r);
}


static void unhold_rsb(struct dlm_rsb *r)
{
	int rv;
	rv = kref_put(&r->res_ref, toss_rsb);
	DLM_ASSERT(!rv, dlm_dump_rsb(r););
}

static void kill_rsb(struct kref *kref)
{
	struct dlm_rsb *r = container_of(kref, struct dlm_rsb, res_ref);


	DLM_ASSERT(list_empty(&r->res_lookup), dlm_dump_rsb(r););
	DLM_ASSERT(list_empty(&r->res_grantqueue), dlm_dump_rsb(r););
	DLM_ASSERT(list_empty(&r->res_convertqueue), dlm_dump_rsb(r););
	DLM_ASSERT(list_empty(&r->res_waitqueue), dlm_dump_rsb(r););
	DLM_ASSERT(list_empty(&r->res_root_list), dlm_dump_rsb(r););
	DLM_ASSERT(list_empty(&r->res_recover_list), dlm_dump_rsb(r););
}


static void attach_lkb(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	hold_rsb(r);
	lkb->lkb_resource = r;
}

static void detach_lkb(struct dlm_lkb *lkb)
{
	if (lkb->lkb_resource) {
		put_rsb(lkb->lkb_resource);
		lkb->lkb_resource = NULL;
	}
}

static int create_lkb(struct dlm_ls *ls, struct dlm_lkb **lkb_ret)
{
	struct dlm_lkb *lkb;
	int rv, id;

	lkb = dlm_allocate_lkb(ls);
	if (!lkb)
		return -ENOMEM;

	lkb->lkb_nodeid = -1;
	lkb->lkb_grmode = DLM_LOCK_IV;
	kref_init(&lkb->lkb_ref);
	INIT_LIST_HEAD(&lkb->lkb_ownqueue);
	INIT_LIST_HEAD(&lkb->lkb_rsb_lookup);
	INIT_LIST_HEAD(&lkb->lkb_time_list);
	INIT_LIST_HEAD(&lkb->lkb_cb_list);
	mutex_init(&lkb->lkb_cb_mutex);
	INIT_WORK(&lkb->lkb_cb_work, dlm_callback_work);

 retry:
	rv = idr_pre_get(&ls->ls_lkbidr, GFP_NOFS);
	if (!rv)
		return -ENOMEM;

	spin_lock(&ls->ls_lkbidr_spin);
	rv = idr_get_new_above(&ls->ls_lkbidr, lkb, 1, &id);
	if (!rv)
		lkb->lkb_id = id;
	spin_unlock(&ls->ls_lkbidr_spin);

	if (rv == -EAGAIN)
		goto retry;

	if (rv < 0) {
		log_error(ls, "create_lkb idr error %d", rv);
		return rv;
	}

	*lkb_ret = lkb;
	return 0;
}

static int find_lkb(struct dlm_ls *ls, uint32_t lkid, struct dlm_lkb **lkb_ret)
{
	struct dlm_lkb *lkb;

	spin_lock(&ls->ls_lkbidr_spin);
	lkb = idr_find(&ls->ls_lkbidr, lkid);
	if (lkb)
		kref_get(&lkb->lkb_ref);
	spin_unlock(&ls->ls_lkbidr_spin);

	*lkb_ret = lkb;
	return lkb ? 0 : -ENOENT;
}

static void kill_lkb(struct kref *kref)
{
	struct dlm_lkb *lkb = container_of(kref, struct dlm_lkb, lkb_ref);


	DLM_ASSERT(!lkb->lkb_status, dlm_print_lkb(lkb););
}


static int __put_lkb(struct dlm_ls *ls, struct dlm_lkb *lkb)
{
	uint32_t lkid = lkb->lkb_id;

	spin_lock(&ls->ls_lkbidr_spin);
	if (kref_put(&lkb->lkb_ref, kill_lkb)) {
		idr_remove(&ls->ls_lkbidr, lkid);
		spin_unlock(&ls->ls_lkbidr_spin);

		detach_lkb(lkb);

		
		if (lkb->lkb_lvbptr && is_master_copy(lkb))
			dlm_free_lvb(lkb->lkb_lvbptr);
		dlm_free_lkb(lkb);
		return 1;
	} else {
		spin_unlock(&ls->ls_lkbidr_spin);
		return 0;
	}
}

int dlm_put_lkb(struct dlm_lkb *lkb)
{
	struct dlm_ls *ls;

	DLM_ASSERT(lkb->lkb_resource, dlm_print_lkb(lkb););
	DLM_ASSERT(lkb->lkb_resource->res_ls, dlm_print_lkb(lkb););

	ls = lkb->lkb_resource->res_ls;
	return __put_lkb(ls, lkb);
}


static inline void hold_lkb(struct dlm_lkb *lkb)
{
	kref_get(&lkb->lkb_ref);
}


static inline void unhold_lkb(struct dlm_lkb *lkb)
{
	int rv;
	rv = kref_put(&lkb->lkb_ref, kill_lkb);
	DLM_ASSERT(!rv, dlm_print_lkb(lkb););
}

static void lkb_add_ordered(struct list_head *new, struct list_head *head,
			    int mode)
{
	struct dlm_lkb *lkb = NULL;

	list_for_each_entry(lkb, head, lkb_statequeue)
		if (lkb->lkb_rqmode < mode)
			break;

	__list_add(new, lkb->lkb_statequeue.prev, &lkb->lkb_statequeue);
}


static void add_lkb(struct dlm_rsb *r, struct dlm_lkb *lkb, int status)
{
	kref_get(&lkb->lkb_ref);

	DLM_ASSERT(!lkb->lkb_status, dlm_print_lkb(lkb););

	lkb->lkb_timestamp = ktime_get();

	lkb->lkb_status = status;

	switch (status) {
	case DLM_LKSTS_WAITING:
		if (lkb->lkb_exflags & DLM_LKF_HEADQUE)
			list_add(&lkb->lkb_statequeue, &r->res_waitqueue);
		else
			list_add_tail(&lkb->lkb_statequeue, &r->res_waitqueue);
		break;
	case DLM_LKSTS_GRANTED:
		
		lkb_add_ordered(&lkb->lkb_statequeue, &r->res_grantqueue,
				lkb->lkb_grmode);
		break;
	case DLM_LKSTS_CONVERT:
		if (lkb->lkb_exflags & DLM_LKF_HEADQUE)
			list_add(&lkb->lkb_statequeue, &r->res_convertqueue);
		else
			list_add_tail(&lkb->lkb_statequeue,
				      &r->res_convertqueue);
		break;
	default:
		DLM_ASSERT(0, dlm_print_lkb(lkb); printk("sts=%d\n", status););
	}
}

static void del_lkb(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	lkb->lkb_status = 0;
	list_del(&lkb->lkb_statequeue);
	unhold_lkb(lkb);
}

static void move_lkb(struct dlm_rsb *r, struct dlm_lkb *lkb, int sts)
{
	hold_lkb(lkb);
	del_lkb(r, lkb);
	add_lkb(r, lkb, sts);
	unhold_lkb(lkb);
}

static int msg_reply_type(int mstype)
{
	switch (mstype) {
	case DLM_MSG_REQUEST:
		return DLM_MSG_REQUEST_REPLY;
	case DLM_MSG_CONVERT:
		return DLM_MSG_CONVERT_REPLY;
	case DLM_MSG_UNLOCK:
		return DLM_MSG_UNLOCK_REPLY;
	case DLM_MSG_CANCEL:
		return DLM_MSG_CANCEL_REPLY;
	case DLM_MSG_LOOKUP:
		return DLM_MSG_LOOKUP_REPLY;
	}
	return -1;
}

static int nodeid_warned(int nodeid, int num_nodes, int *warned)
{
	int i;

	for (i = 0; i < num_nodes; i++) {
		if (!warned[i]) {
			warned[i] = nodeid;
			return 0;
		}
		if (warned[i] == nodeid)
			return 1;
	}
	return 0;
}

void dlm_scan_waiters(struct dlm_ls *ls)
{
	struct dlm_lkb *lkb;
	ktime_t zero = ktime_set(0, 0);
	s64 us;
	s64 debug_maxus = 0;
	u32 debug_scanned = 0;
	u32 debug_expired = 0;
	int num_nodes = 0;
	int *warned = NULL;

	if (!dlm_config.ci_waitwarn_us)
		return;

	mutex_lock(&ls->ls_waiters_mutex);

	list_for_each_entry(lkb, &ls->ls_waiters, lkb_wait_reply) {
		if (ktime_equal(lkb->lkb_wait_time, zero))
			continue;

		debug_scanned++;

		us = ktime_to_us(ktime_sub(ktime_get(), lkb->lkb_wait_time));

		if (us < dlm_config.ci_waitwarn_us)
			continue;

		lkb->lkb_wait_time = zero;

		debug_expired++;
		if (us > debug_maxus)
			debug_maxus = us;

		if (!num_nodes) {
			num_nodes = ls->ls_num_nodes;
			warned = kzalloc(num_nodes * sizeof(int), GFP_KERNEL);
		}
		if (!warned)
			continue;
		if (nodeid_warned(lkb->lkb_wait_nodeid, num_nodes, warned))
			continue;

		log_error(ls, "waitwarn %x %lld %d us check connection to "
			  "node %d", lkb->lkb_id, (long long)us,
			  dlm_config.ci_waitwarn_us, lkb->lkb_wait_nodeid);
	}
	mutex_unlock(&ls->ls_waiters_mutex);
	kfree(warned);

	if (debug_expired)
		log_debug(ls, "scan_waiters %u warn %u over %d us max %lld us",
			  debug_scanned, debug_expired,
			  dlm_config.ci_waitwarn_us, (long long)debug_maxus);
}


static int add_to_waiters(struct dlm_lkb *lkb, int mstype, int to_nodeid)
{
	struct dlm_ls *ls = lkb->lkb_resource->res_ls;
	int error = 0;

	mutex_lock(&ls->ls_waiters_mutex);

	if (is_overlap_unlock(lkb) ||
	    (is_overlap_cancel(lkb) && (mstype == DLM_MSG_CANCEL))) {
		error = -EINVAL;
		goto out;
	}

	if (lkb->lkb_wait_type || is_overlap_cancel(lkb)) {
		switch (mstype) {
		case DLM_MSG_UNLOCK:
			lkb->lkb_flags |= DLM_IFL_OVERLAP_UNLOCK;
			break;
		case DLM_MSG_CANCEL:
			lkb->lkb_flags |= DLM_IFL_OVERLAP_CANCEL;
			break;
		default:
			error = -EBUSY;
			goto out;
		}
		lkb->lkb_wait_count++;
		hold_lkb(lkb);

		log_debug(ls, "addwait %x cur %d overlap %d count %d f %x",
			  lkb->lkb_id, lkb->lkb_wait_type, mstype,
			  lkb->lkb_wait_count, lkb->lkb_flags);
		goto out;
	}

	DLM_ASSERT(!lkb->lkb_wait_count,
		   dlm_print_lkb(lkb);
		   printk("wait_count %d\n", lkb->lkb_wait_count););

	lkb->lkb_wait_count++;
	lkb->lkb_wait_type = mstype;
	lkb->lkb_wait_time = ktime_get();
	lkb->lkb_wait_nodeid = to_nodeid; 
	hold_lkb(lkb);
	list_add(&lkb->lkb_wait_reply, &ls->ls_waiters);
 out:
	if (error)
		log_error(ls, "addwait error %x %d flags %x %d %d %s",
			  lkb->lkb_id, error, lkb->lkb_flags, mstype,
			  lkb->lkb_wait_type, lkb->lkb_resource->res_name);
	mutex_unlock(&ls->ls_waiters_mutex);
	return error;
}


static int _remove_from_waiters(struct dlm_lkb *lkb, int mstype,
				struct dlm_message *ms)
{
	struct dlm_ls *ls = lkb->lkb_resource->res_ls;
	int overlap_done = 0;

	if (is_overlap_unlock(lkb) && (mstype == DLM_MSG_UNLOCK_REPLY)) {
		log_debug(ls, "remwait %x unlock_reply overlap", lkb->lkb_id);
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_UNLOCK;
		overlap_done = 1;
		goto out_del;
	}

	if (is_overlap_cancel(lkb) && (mstype == DLM_MSG_CANCEL_REPLY)) {
		log_debug(ls, "remwait %x cancel_reply overlap", lkb->lkb_id);
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_CANCEL;
		overlap_done = 1;
		goto out_del;
	}


	if ((mstype == DLM_MSG_CANCEL_REPLY) &&
	    (lkb->lkb_wait_type != DLM_MSG_CANCEL)) {
		log_debug(ls, "remwait %x cancel_reply wait_type %d",
			  lkb->lkb_id, lkb->lkb_wait_type);
		return -1;
	}


	if ((mstype == DLM_MSG_CONVERT_REPLY) &&
	    (lkb->lkb_wait_type == DLM_MSG_CONVERT) &&
	    is_overlap_cancel(lkb) && ms && !ms->m_result) {
		log_debug(ls, "remwait %x convert_reply zap overlap_cancel",
			  lkb->lkb_id);
		lkb->lkb_wait_type = 0;
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_CANCEL;
		lkb->lkb_wait_count--;
		goto out_del;
	}


	if (lkb->lkb_wait_type) {
		lkb->lkb_wait_type = 0;
		goto out_del;
	}

	log_error(ls, "remwait error %x reply %d flags %x no wait_type",
		  lkb->lkb_id, mstype, lkb->lkb_flags);
	return -1;

 out_del:

	if (overlap_done && lkb->lkb_wait_type) {
		log_error(ls, "remwait error %x reply %d wait_type %d overlap",
			  lkb->lkb_id, mstype, lkb->lkb_wait_type);
		lkb->lkb_wait_count--;
		lkb->lkb_wait_type = 0;
	}

	DLM_ASSERT(lkb->lkb_wait_count, dlm_print_lkb(lkb););

	lkb->lkb_flags &= ~DLM_IFL_RESEND;
	lkb->lkb_wait_count--;
	if (!lkb->lkb_wait_count)
		list_del_init(&lkb->lkb_wait_reply);
	unhold_lkb(lkb);
	return 0;
}

static int remove_from_waiters(struct dlm_lkb *lkb, int mstype)
{
	struct dlm_ls *ls = lkb->lkb_resource->res_ls;
	int error;

	mutex_lock(&ls->ls_waiters_mutex);
	error = _remove_from_waiters(lkb, mstype, NULL);
	mutex_unlock(&ls->ls_waiters_mutex);
	return error;
}


static int remove_from_waiters_ms(struct dlm_lkb *lkb, struct dlm_message *ms)
{
	struct dlm_ls *ls = lkb->lkb_resource->res_ls;
	int error;

	if (ms->m_flags != DLM_IFL_STUB_MS)
		mutex_lock(&ls->ls_waiters_mutex);
	error = _remove_from_waiters(lkb, ms->m_type, ms);
	if (ms->m_flags != DLM_IFL_STUB_MS)
		mutex_unlock(&ls->ls_waiters_mutex);
	return error;
}

static void dir_remove(struct dlm_rsb *r)
{
	int to_nodeid;

	if (dlm_no_directory(r->res_ls))
		return;

	to_nodeid = dlm_dir_nodeid(r);
	if (to_nodeid != dlm_our_nodeid())
		send_remove(r);
	else
		dlm_dir_remove_entry(r->res_ls, to_nodeid,
				     r->res_name, r->res_length);
}


static int shrink_bucket(struct dlm_ls *ls, int b)
{
	struct rb_node *n;
	struct dlm_rsb *r;
	int count = 0, found;

	for (;;) {
		found = 0;
		spin_lock(&ls->ls_rsbtbl[b].lock);
		for (n = rb_first(&ls->ls_rsbtbl[b].toss); n; n = rb_next(n)) {
			r = rb_entry(n, struct dlm_rsb, res_hashnode);
			if (!time_after_eq(jiffies, r->res_toss_time +
					   dlm_config.ci_toss_secs * HZ))
				continue;
			found = 1;
			break;
		}

		if (!found) {
			spin_unlock(&ls->ls_rsbtbl[b].lock);
			break;
		}

		if (kref_put(&r->res_ref, kill_rsb)) {
			rb_erase(&r->res_hashnode, &ls->ls_rsbtbl[b].toss);
			spin_unlock(&ls->ls_rsbtbl[b].lock);

			if (is_master(r))
				dir_remove(r);
			dlm_free_rsb(r);
			count++;
		} else {
			spin_unlock(&ls->ls_rsbtbl[b].lock);
			log_error(ls, "tossed rsb in use %s", r->res_name);
		}
	}

	return count;
}

void dlm_scan_rsbs(struct dlm_ls *ls)
{
	int i;

	for (i = 0; i < ls->ls_rsbtbl_size; i++) {
		shrink_bucket(ls, i);
		if (dlm_locking_stopped(ls))
			break;
		cond_resched();
	}
}

static void add_timeout(struct dlm_lkb *lkb)
{
	struct dlm_ls *ls = lkb->lkb_resource->res_ls;

	if (is_master_copy(lkb))
		return;

	if (test_bit(LSFL_TIMEWARN, &ls->ls_flags) &&
	    !(lkb->lkb_exflags & DLM_LKF_NODLCKWT)) {
		lkb->lkb_flags |= DLM_IFL_WATCH_TIMEWARN;
		goto add_it;
	}
	if (lkb->lkb_exflags & DLM_LKF_TIMEOUT)
		goto add_it;
	return;

 add_it:
	DLM_ASSERT(list_empty(&lkb->lkb_time_list), dlm_print_lkb(lkb););
	mutex_lock(&ls->ls_timeout_mutex);
	hold_lkb(lkb);
	list_add_tail(&lkb->lkb_time_list, &ls->ls_timeout);
	mutex_unlock(&ls->ls_timeout_mutex);
}

static void del_timeout(struct dlm_lkb *lkb)
{
	struct dlm_ls *ls = lkb->lkb_resource->res_ls;

	mutex_lock(&ls->ls_timeout_mutex);
	if (!list_empty(&lkb->lkb_time_list)) {
		list_del_init(&lkb->lkb_time_list);
		unhold_lkb(lkb);
	}
	mutex_unlock(&ls->ls_timeout_mutex);
}


void dlm_scan_timeout(struct dlm_ls *ls)
{
	struct dlm_rsb *r;
	struct dlm_lkb *lkb;
	int do_cancel, do_warn;
	s64 wait_us;

	for (;;) {
		if (dlm_locking_stopped(ls))
			break;

		do_cancel = 0;
		do_warn = 0;
		mutex_lock(&ls->ls_timeout_mutex);
		list_for_each_entry(lkb, &ls->ls_timeout, lkb_time_list) {

			wait_us = ktime_to_us(ktime_sub(ktime_get(),
					      		lkb->lkb_timestamp));

			if ((lkb->lkb_exflags & DLM_LKF_TIMEOUT) &&
			    wait_us >= (lkb->lkb_timeout_cs * 10000))
				do_cancel = 1;

			if ((lkb->lkb_flags & DLM_IFL_WATCH_TIMEWARN) &&
			    wait_us >= dlm_config.ci_timewarn_cs * 10000)
				do_warn = 1;

			if (!do_cancel && !do_warn)
				continue;
			hold_lkb(lkb);
			break;
		}
		mutex_unlock(&ls->ls_timeout_mutex);

		if (!do_cancel && !do_warn)
			break;

		r = lkb->lkb_resource;
		hold_rsb(r);
		lock_rsb(r);

		if (do_warn) {
			
			lkb->lkb_flags &= ~DLM_IFL_WATCH_TIMEWARN;
			if (!(lkb->lkb_exflags & DLM_LKF_TIMEOUT))
				del_timeout(lkb);
			dlm_timeout_warn(lkb);
		}

		if (do_cancel) {
			log_debug(ls, "timeout cancel %x node %d %s",
				  lkb->lkb_id, lkb->lkb_nodeid, r->res_name);
			lkb->lkb_flags &= ~DLM_IFL_WATCH_TIMEWARN;
			lkb->lkb_flags |= DLM_IFL_TIMEOUT_CANCEL;
			del_timeout(lkb);
			_cancel_lock(r, lkb);
		}

		unlock_rsb(r);
		unhold_rsb(r);
		dlm_put_lkb(lkb);
	}
}


void dlm_adjust_timeouts(struct dlm_ls *ls)
{
	struct dlm_lkb *lkb;
	u64 adj_us = jiffies_to_usecs(jiffies - ls->ls_recover_begin);

	ls->ls_recover_begin = 0;
	mutex_lock(&ls->ls_timeout_mutex);
	list_for_each_entry(lkb, &ls->ls_timeout, lkb_time_list)
		lkb->lkb_timestamp = ktime_add_us(lkb->lkb_timestamp, adj_us);
	mutex_unlock(&ls->ls_timeout_mutex);

	if (!dlm_config.ci_waitwarn_us)
		return;

	mutex_lock(&ls->ls_waiters_mutex);
	list_for_each_entry(lkb, &ls->ls_waiters, lkb_wait_reply) {
		if (ktime_to_us(lkb->lkb_wait_time))
			lkb->lkb_wait_time = ktime_get();
	}
	mutex_unlock(&ls->ls_waiters_mutex);
}


static void set_lvb_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int b, len = r->res_ls->ls_lvblen;

	/* b=1 lvb returned to caller
	   b=0 lvb written to rsb or invalidated
	   b=-1 do nothing */

	b =  dlm_lvb_operations[lkb->lkb_grmode + 1][lkb->lkb_rqmode + 1];

	if (b == 1) {
		if (!lkb->lkb_lvbptr)
			return;

		if (!(lkb->lkb_exflags & DLM_LKF_VALBLK))
			return;

		if (!r->res_lvbptr)
			return;

		memcpy(lkb->lkb_lvbptr, r->res_lvbptr, len);
		lkb->lkb_lvbseq = r->res_lvbseq;

	} else if (b == 0) {
		if (lkb->lkb_exflags & DLM_LKF_IVVALBLK) {
			rsb_set_flag(r, RSB_VALNOTVALID);
			return;
		}

		if (!lkb->lkb_lvbptr)
			return;

		if (!(lkb->lkb_exflags & DLM_LKF_VALBLK))
			return;

		if (!r->res_lvbptr)
			r->res_lvbptr = dlm_allocate_lvb(r->res_ls);

		if (!r->res_lvbptr)
			return;

		memcpy(r->res_lvbptr, lkb->lkb_lvbptr, len);
		r->res_lvbseq++;
		lkb->lkb_lvbseq = r->res_lvbseq;
		rsb_clear_flag(r, RSB_VALNOTVALID);
	}

	if (rsb_flag(r, RSB_VALNOTVALID))
		lkb->lkb_sbflags |= DLM_SBF_VALNOTVALID;
}

static void set_lvb_unlock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	if (lkb->lkb_grmode < DLM_LOCK_PW)
		return;

	if (lkb->lkb_exflags & DLM_LKF_IVVALBLK) {
		rsb_set_flag(r, RSB_VALNOTVALID);
		return;
	}

	if (!lkb->lkb_lvbptr)
		return;

	if (!(lkb->lkb_exflags & DLM_LKF_VALBLK))
		return;

	if (!r->res_lvbptr)
		r->res_lvbptr = dlm_allocate_lvb(r->res_ls);

	if (!r->res_lvbptr)
		return;

	memcpy(r->res_lvbptr, lkb->lkb_lvbptr, r->res_ls->ls_lvblen);
	r->res_lvbseq++;
	rsb_clear_flag(r, RSB_VALNOTVALID);
}


static void set_lvb_lock_pc(struct dlm_rsb *r, struct dlm_lkb *lkb,
			    struct dlm_message *ms)
{
	int b;

	if (!lkb->lkb_lvbptr)
		return;

	if (!(lkb->lkb_exflags & DLM_LKF_VALBLK))
		return;

	b = dlm_lvb_operations[lkb->lkb_grmode + 1][lkb->lkb_rqmode + 1];
	if (b == 1) {
		int len = receive_extralen(ms);
		if (len > DLM_RESNAME_MAXLEN)
			len = DLM_RESNAME_MAXLEN;
		memcpy(lkb->lkb_lvbptr, ms->m_extra, len);
		lkb->lkb_lvbseq = ms->m_lvbseq;
	}
}


static void _remove_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	del_lkb(r, lkb);
	lkb->lkb_grmode = DLM_LOCK_IV;
	unhold_lkb(lkb);
}

static void remove_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	set_lvb_unlock(r, lkb);
	_remove_lock(r, lkb);
}

static void remove_lock_pc(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	_remove_lock(r, lkb);
}


static int revert_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int rv = 0;

	lkb->lkb_rqmode = DLM_LOCK_IV;

	switch (lkb->lkb_status) {
	case DLM_LKSTS_GRANTED:
		break;
	case DLM_LKSTS_CONVERT:
		move_lkb(r, lkb, DLM_LKSTS_GRANTED);
		rv = 1;
		break;
	case DLM_LKSTS_WAITING:
		del_lkb(r, lkb);
		lkb->lkb_grmode = DLM_LOCK_IV;
		unhold_lkb(lkb);
		rv = -1;
		break;
	default:
		log_print("invalid status for revert %d", lkb->lkb_status);
	}
	return rv;
}

static int revert_lock_pc(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	return revert_lock(r, lkb);
}

static void _grant_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	if (lkb->lkb_grmode != lkb->lkb_rqmode) {
		lkb->lkb_grmode = lkb->lkb_rqmode;
		if (lkb->lkb_status)
			move_lkb(r, lkb, DLM_LKSTS_GRANTED);
		else
			add_lkb(r, lkb, DLM_LKSTS_GRANTED);
	}

	lkb->lkb_rqmode = DLM_LOCK_IV;
}

static void grant_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	set_lvb_lock(r, lkb);
	_grant_lock(r, lkb);
	lkb->lkb_highbast = 0;
}

static void grant_lock_pc(struct dlm_rsb *r, struct dlm_lkb *lkb,
			  struct dlm_message *ms)
{
	set_lvb_lock_pc(r, lkb, ms);
	_grant_lock(r, lkb);
}


static void grant_lock_pending(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	grant_lock(r, lkb);
	if (is_master_copy(lkb))
		send_grant(r, lkb);
	else
		queue_cast(r, lkb, 0);
}


static void munge_demoted(struct dlm_lkb *lkb)
{
	if (lkb->lkb_rqmode == DLM_LOCK_IV || lkb->lkb_grmode == DLM_LOCK_IV) {
		log_print("munge_demoted %x invalid modes gr %d rq %d",
			  lkb->lkb_id, lkb->lkb_grmode, lkb->lkb_rqmode);
		return;
	}

	lkb->lkb_grmode = DLM_LOCK_NL;
}

static void munge_altmode(struct dlm_lkb *lkb, struct dlm_message *ms)
{
	if (ms->m_type != DLM_MSG_REQUEST_REPLY &&
	    ms->m_type != DLM_MSG_GRANT) {
		log_print("munge_altmode %x invalid reply type %d",
			  lkb->lkb_id, ms->m_type);
		return;
	}

	if (lkb->lkb_exflags & DLM_LKF_ALTPR)
		lkb->lkb_rqmode = DLM_LOCK_PR;
	else if (lkb->lkb_exflags & DLM_LKF_ALTCW)
		lkb->lkb_rqmode = DLM_LOCK_CW;
	else {
		log_print("munge_altmode invalid exflags %x", lkb->lkb_exflags);
		dlm_print_lkb(lkb);
	}
}

static inline int first_in_list(struct dlm_lkb *lkb, struct list_head *head)
{
	struct dlm_lkb *first = list_entry(head->next, struct dlm_lkb,
					   lkb_statequeue);
	if (lkb->lkb_id == first->lkb_id)
		return 1;

	return 0;
}


static int queue_conflict(struct list_head *head, struct dlm_lkb *lkb)
{
	struct dlm_lkb *this;

	list_for_each_entry(this, head, lkb_statequeue) {
		if (this == lkb)
			continue;
		if (!modes_compat(this, lkb))
			return 1;
	}
	return 0;
}


static int conversion_deadlock_detect(struct dlm_rsb *r, struct dlm_lkb *lkb2)
{
	struct dlm_lkb *lkb1;
	int lkb_is_ahead = 0;

	list_for_each_entry(lkb1, &r->res_convertqueue, lkb_statequeue) {
		if (lkb1 == lkb2) {
			lkb_is_ahead = 1;
			continue;
		}

		if (!lkb_is_ahead) {
			if (!modes_compat(lkb2, lkb1))
				return 1;
		} else {
			if (!modes_compat(lkb2, lkb1) &&
			    !modes_compat(lkb1, lkb2))
				return 1;
		}
	}
	return 0;
}


static int _can_be_granted(struct dlm_rsb *r, struct dlm_lkb *lkb, int now)
{
	int8_t conv = (lkb->lkb_grmode != DLM_LOCK_IV);


	if (lkb->lkb_exflags & DLM_LKF_EXPEDITE)
		return 1;


	if (queue_conflict(&r->res_grantqueue, lkb))
		goto out;


	if (queue_conflict(&r->res_convertqueue, lkb))
		goto out;


	if (now && conv && !(lkb->lkb_exflags & DLM_LKF_QUECVT))
		return 1;


	if (now && conv && (lkb->lkb_exflags & DLM_LKF_QUECVT)) {
		if (list_empty(&r->res_convertqueue))
			return 1;
		else
			goto out;
	}


	if (lkb->lkb_exflags & DLM_LKF_NOORDER)
		return 1;


	if (!now && conv && first_in_list(lkb, &r->res_convertqueue))
		return 1;


	if (now && !conv && list_empty(&r->res_convertqueue) &&
	    list_empty(&r->res_waitqueue))
		return 1;


	if (!now && !conv && list_empty(&r->res_convertqueue) &&
	    first_in_list(lkb, &r->res_waitqueue))
		return 1;
 out:
	return 0;
}

static int can_be_granted(struct dlm_rsb *r, struct dlm_lkb *lkb, int now,
			  int *err)
{
	int rv;
	int8_t alt = 0, rqmode = lkb->lkb_rqmode;
	int8_t is_convert = (lkb->lkb_grmode != DLM_LOCK_IV);

	if (err)
		*err = 0;

	rv = _can_be_granted(r, lkb, now);
	if (rv)
		goto out;


	if (is_convert && can_be_queued(lkb) &&
	    conversion_deadlock_detect(r, lkb)) {
		if (lkb->lkb_exflags & DLM_LKF_CONVDEADLK) {
			lkb->lkb_grmode = DLM_LOCK_NL;
			lkb->lkb_sbflags |= DLM_SBF_DEMOTED;
		} else if (!(lkb->lkb_exflags & DLM_LKF_NODLCKWT)) {
			if (err)
				*err = -EDEADLK;
			else {
				log_print("can_be_granted deadlock %x now %d",
					  lkb->lkb_id, now);
				dlm_dump_rsb(r);
			}
		}
		goto out;
	}


	if (rqmode != DLM_LOCK_PR && (lkb->lkb_exflags & DLM_LKF_ALTPR))
		alt = DLM_LOCK_PR;
	else if (rqmode != DLM_LOCK_CW && (lkb->lkb_exflags & DLM_LKF_ALTCW))
		alt = DLM_LOCK_CW;

	if (alt) {
		lkb->lkb_rqmode = alt;
		rv = _can_be_granted(r, lkb, now);
		if (rv)
			lkb->lkb_sbflags |= DLM_SBF_ALTMODE;
		else
			lkb->lkb_rqmode = rqmode;
	}
 out:
	return rv;
}



static int grant_pending_convert(struct dlm_rsb *r, int high, int *cw)
{
	struct dlm_lkb *lkb, *s;
	int hi, demoted, quit, grant_restart, demote_restart;
	int deadlk;

	quit = 0;
 restart:
	grant_restart = 0;
	demote_restart = 0;
	hi = DLM_LOCK_IV;

	list_for_each_entry_safe(lkb, s, &r->res_convertqueue, lkb_statequeue) {
		demoted = is_demoted(lkb);
		deadlk = 0;

		if (can_be_granted(r, lkb, 0, &deadlk)) {
			grant_lock_pending(r, lkb);
			grant_restart = 1;
			continue;
		}

		if (!demoted && is_demoted(lkb)) {
			log_print("WARN: pending demoted %x node %d %s",
				  lkb->lkb_id, lkb->lkb_nodeid, r->res_name);
			demote_restart = 1;
			continue;
		}

		if (deadlk) {
			log_print("WARN: pending deadlock %x node %d %s",
				  lkb->lkb_id, lkb->lkb_nodeid, r->res_name);
			dlm_dump_rsb(r);
			continue;
		}

		hi = max_t(int, lkb->lkb_rqmode, hi);

		if (cw && lkb->lkb_rqmode == DLM_LOCK_CW)
			*cw = 1;
	}

	if (grant_restart)
		goto restart;
	if (demote_restart && !quit) {
		quit = 1;
		goto restart;
	}

	return max_t(int, high, hi);
}

static int grant_pending_wait(struct dlm_rsb *r, int high, int *cw)
{
	struct dlm_lkb *lkb, *s;

	list_for_each_entry_safe(lkb, s, &r->res_waitqueue, lkb_statequeue) {
		if (can_be_granted(r, lkb, 0, NULL))
			grant_lock_pending(r, lkb);
                else {
			high = max_t(int, lkb->lkb_rqmode, high);
			if (lkb->lkb_rqmode == DLM_LOCK_CW)
				*cw = 1;
		}
	}

	return high;
}


static int lock_requires_bast(struct dlm_lkb *gr, int high, int cw)
{
	if (gr->lkb_grmode == DLM_LOCK_PR && cw) {
		if (gr->lkb_highbast < DLM_LOCK_EX)
			return 1;
		return 0;
	}

	if (gr->lkb_highbast < high &&
	    !__dlm_compat_matrix[gr->lkb_grmode+1][high+1])
		return 1;
	return 0;
}

static void grant_pending_locks(struct dlm_rsb *r)
{
	struct dlm_lkb *lkb, *s;
	int high = DLM_LOCK_IV;
	int cw = 0;

	DLM_ASSERT(is_master(r), dlm_dump_rsb(r););

	high = grant_pending_convert(r, high, &cw);
	high = grant_pending_wait(r, high, &cw);

	if (high == DLM_LOCK_IV)
		return;


	list_for_each_entry_safe(lkb, s, &r->res_grantqueue, lkb_statequeue) {
		if (lkb->lkb_bastfn && lock_requires_bast(lkb, high, cw)) {
			if (cw && high == DLM_LOCK_PR &&
			    lkb->lkb_grmode == DLM_LOCK_PR)
				queue_bast(r, lkb, DLM_LOCK_CW);
			else
				queue_bast(r, lkb, high);
			lkb->lkb_highbast = high;
		}
	}
}

static int modes_require_bast(struct dlm_lkb *gr, struct dlm_lkb *rq)
{
	if ((gr->lkb_grmode == DLM_LOCK_PR && rq->lkb_rqmode == DLM_LOCK_CW) ||
	    (gr->lkb_grmode == DLM_LOCK_CW && rq->lkb_rqmode == DLM_LOCK_PR)) {
		if (gr->lkb_highbast < DLM_LOCK_EX)
			return 1;
		return 0;
	}

	if (gr->lkb_highbast < rq->lkb_rqmode && !modes_compat(gr, rq))
		return 1;
	return 0;
}

static void send_bast_queue(struct dlm_rsb *r, struct list_head *head,
			    struct dlm_lkb *lkb)
{
	struct dlm_lkb *gr;

	list_for_each_entry(gr, head, lkb_statequeue) {
		
		if (gr == lkb)
			continue;
		if (gr->lkb_bastfn && modes_require_bast(gr, lkb)) {
			queue_bast(r, gr, lkb->lkb_rqmode);
			gr->lkb_highbast = lkb->lkb_rqmode;
		}
	}
}

static void send_blocking_asts(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	send_bast_queue(r, &r->res_grantqueue, lkb);
}

static void send_blocking_asts_all(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	send_bast_queue(r, &r->res_grantqueue, lkb);
	send_bast_queue(r, &r->res_convertqueue, lkb);
}


static int set_master(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	struct dlm_ls *ls = r->res_ls;
	int i, error, dir_nodeid, ret_nodeid, our_nodeid = dlm_our_nodeid();

	if (rsb_flag(r, RSB_MASTER_UNCERTAIN)) {
		rsb_clear_flag(r, RSB_MASTER_UNCERTAIN);
		r->res_first_lkid = lkb->lkb_id;
		lkb->lkb_nodeid = r->res_nodeid;
		return 0;
	}

	if (r->res_first_lkid && r->res_first_lkid != lkb->lkb_id) {
		list_add_tail(&lkb->lkb_rsb_lookup, &r->res_lookup);
		return 1;
	}

	if (r->res_nodeid == 0) {
		lkb->lkb_nodeid = 0;
		return 0;
	}

	if (r->res_nodeid > 0) {
		lkb->lkb_nodeid = r->res_nodeid;
		return 0;
	}

	DLM_ASSERT(r->res_nodeid == -1, dlm_dump_rsb(r););

	dir_nodeid = dlm_dir_nodeid(r);

	if (dir_nodeid != our_nodeid) {
		r->res_first_lkid = lkb->lkb_id;
		send_lookup(r, lkb);
		return 1;
	}

	for (i = 0; i < 2; i++) {

		error = dlm_dir_lookup(ls, our_nodeid, r->res_name,
				       r->res_length, &ret_nodeid);
		if (!error)
			break;
		log_debug(ls, "dir_lookup error %d %s", error, r->res_name);
		schedule();
	}
	if (error && error != -EEXIST)
		return error;

	if (ret_nodeid == our_nodeid) {
		r->res_first_lkid = 0;
		r->res_nodeid = 0;
		lkb->lkb_nodeid = 0;
	} else {
		r->res_first_lkid = lkb->lkb_id;
		r->res_nodeid = ret_nodeid;
		lkb->lkb_nodeid = ret_nodeid;
	}
	return 0;
}

static void process_lookup_list(struct dlm_rsb *r)
{
	struct dlm_lkb *lkb, *safe;

	list_for_each_entry_safe(lkb, safe, &r->res_lookup, lkb_rsb_lookup) {
		list_del_init(&lkb->lkb_rsb_lookup);
		_request_lock(r, lkb);
		schedule();
	}
}


static void confirm_master(struct dlm_rsb *r, int error)
{
	struct dlm_lkb *lkb;

	if (!r->res_first_lkid)
		return;

	switch (error) {
	case 0:
	case -EINPROGRESS:
		r->res_first_lkid = 0;
		process_lookup_list(r);
		break;

	case -EAGAIN:
	case -EBADR:
	case -ENOTBLK:

		r->res_first_lkid = 0;

		if (!list_empty(&r->res_lookup)) {
			lkb = list_entry(r->res_lookup.next, struct dlm_lkb,
					 lkb_rsb_lookup);
			list_del_init(&lkb->lkb_rsb_lookup);
			r->res_first_lkid = lkb->lkb_id;
			_request_lock(r, lkb);
		}
		break;

	default:
		log_error(r->res_ls, "confirm_master unknown error %d", error);
	}
}

static int set_lock_args(int mode, struct dlm_lksb *lksb, uint32_t flags,
			 int namelen, unsigned long timeout_cs,
			 void (*ast) (void *astparam),
			 void *astparam,
			 void (*bast) (void *astparam, int mode),
			 struct dlm_args *args)
{
	int rv = -EINVAL;

	

	if (mode < 0 || mode > DLM_LOCK_EX)
		goto out;

	if (!(flags & DLM_LKF_CONVERT) && (namelen > DLM_RESNAME_MAXLEN))
		goto out;

	if (flags & DLM_LKF_CANCEL)
		goto out;

	if (flags & DLM_LKF_QUECVT && !(flags & DLM_LKF_CONVERT))
		goto out;

	if (flags & DLM_LKF_CONVDEADLK && !(flags & DLM_LKF_CONVERT))
		goto out;

	if (flags & DLM_LKF_CONVDEADLK && flags & DLM_LKF_NOQUEUE)
		goto out;

	if (flags & DLM_LKF_EXPEDITE && flags & DLM_LKF_CONVERT)
		goto out;

	if (flags & DLM_LKF_EXPEDITE && flags & DLM_LKF_QUECVT)
		goto out;

	if (flags & DLM_LKF_EXPEDITE && flags & DLM_LKF_NOQUEUE)
		goto out;

	if (flags & DLM_LKF_EXPEDITE && mode != DLM_LOCK_NL)
		goto out;

	if (!ast || !lksb)
		goto out;

	if (flags & DLM_LKF_VALBLK && !lksb->sb_lvbptr)
		goto out;

	if (flags & DLM_LKF_CONVERT && !lksb->sb_lkid)
		goto out;


	args->flags = flags;
	args->astfn = ast;
	args->astparam = astparam;
	args->bastfn = bast;
	args->timeout = timeout_cs;
	args->mode = mode;
	args->lksb = lksb;
	rv = 0;
 out:
	return rv;
}

static int set_unlock_args(uint32_t flags, void *astarg, struct dlm_args *args)
{
	if (flags & ~(DLM_LKF_CANCEL | DLM_LKF_VALBLK | DLM_LKF_IVVALBLK |
 		      DLM_LKF_FORCEUNLOCK))
		return -EINVAL;

	if (flags & DLM_LKF_CANCEL && flags & DLM_LKF_FORCEUNLOCK)
		return -EINVAL;

	args->flags = flags;
	args->astparam = astarg;
	return 0;
}

static int validate_lock_args(struct dlm_ls *ls, struct dlm_lkb *lkb,
			      struct dlm_args *args)
{
	int rv = -EINVAL;

	if (args->flags & DLM_LKF_CONVERT) {
		if (lkb->lkb_flags & DLM_IFL_MSTCPY)
			goto out;

		if (args->flags & DLM_LKF_QUECVT &&
		    !__quecvt_compat_matrix[lkb->lkb_grmode+1][args->mode+1])
			goto out;

		rv = -EBUSY;
		if (lkb->lkb_status != DLM_LKSTS_GRANTED)
			goto out;

		if (lkb->lkb_wait_type)
			goto out;

		if (is_overlap(lkb))
			goto out;
	}

	lkb->lkb_exflags = args->flags;
	lkb->lkb_sbflags = 0;
	lkb->lkb_astfn = args->astfn;
	lkb->lkb_astparam = args->astparam;
	lkb->lkb_bastfn = args->bastfn;
	lkb->lkb_rqmode = args->mode;
	lkb->lkb_lksb = args->lksb;
	lkb->lkb_lvbptr = args->lksb->sb_lvbptr;
	lkb->lkb_ownpid = (int) current->pid;
	lkb->lkb_timeout_cs = args->timeout;
	rv = 0;
 out:
	if (rv)
		log_debug(ls, "validate_lock_args %d %x %x %x %d %d %s",
			  rv, lkb->lkb_id, lkb->lkb_flags, args->flags,
			  lkb->lkb_status, lkb->lkb_wait_type,
			  lkb->lkb_resource->res_name);
	return rv;
}



static int validate_unlock_args(struct dlm_lkb *lkb, struct dlm_args *args)
{
	struct dlm_ls *ls = lkb->lkb_resource->res_ls;
	int rv = -EINVAL;

	if (lkb->lkb_flags & DLM_IFL_MSTCPY) {
		log_error(ls, "unlock on MSTCPY %x", lkb->lkb_id);
		dlm_print_lkb(lkb);
		goto out;
	}


	if (lkb->lkb_flags & DLM_IFL_ENDOFLIFE) {
		log_debug(ls, "unlock on ENDOFLIFE %x", lkb->lkb_id);
		rv = -ENOENT;
		goto out;
	}


	if (!list_empty(&lkb->lkb_rsb_lookup)) {
		if (args->flags & (DLM_LKF_CANCEL | DLM_LKF_FORCEUNLOCK)) {
			log_debug(ls, "unlock on rsb_lookup %x", lkb->lkb_id);
			list_del_init(&lkb->lkb_rsb_lookup);
			queue_cast(lkb->lkb_resource, lkb,
				   args->flags & DLM_LKF_CANCEL ?
				   -DLM_ECANCEL : -DLM_EUNLOCK);
			unhold_lkb(lkb); 
		}
		
		rv = -EBUSY;
		goto out;
	}

	

	if (args->flags & DLM_LKF_CANCEL) {
		if (lkb->lkb_exflags & DLM_LKF_CANCEL)
			goto out;

		if (is_overlap(lkb))
			goto out;

		
		del_timeout(lkb);

		if (lkb->lkb_flags & DLM_IFL_RESEND) {
			lkb->lkb_flags |= DLM_IFL_OVERLAP_CANCEL;
			rv = -EBUSY;
			goto out;
		}

		
		if (lkb->lkb_status == DLM_LKSTS_GRANTED &&
		    !lkb->lkb_wait_type) {
			rv = -EBUSY;
			goto out;
		}

		switch (lkb->lkb_wait_type) {
		case DLM_MSG_LOOKUP:
		case DLM_MSG_REQUEST:
			lkb->lkb_flags |= DLM_IFL_OVERLAP_CANCEL;
			rv = -EBUSY;
			goto out;
		case DLM_MSG_UNLOCK:
		case DLM_MSG_CANCEL:
			goto out;
		}
		
		goto out_ok;
	}


	if (args->flags & DLM_LKF_FORCEUNLOCK) {
		if (lkb->lkb_exflags & DLM_LKF_FORCEUNLOCK)
			goto out;

		if (is_overlap_unlock(lkb))
			goto out;

		
		del_timeout(lkb);

		if (lkb->lkb_flags & DLM_IFL_RESEND) {
			lkb->lkb_flags |= DLM_IFL_OVERLAP_UNLOCK;
			rv = -EBUSY;
			goto out;
		}

		switch (lkb->lkb_wait_type) {
		case DLM_MSG_LOOKUP:
		case DLM_MSG_REQUEST:
			lkb->lkb_flags |= DLM_IFL_OVERLAP_UNLOCK;
			rv = -EBUSY;
			goto out;
		case DLM_MSG_UNLOCK:
			goto out;
		}
		
		goto out_ok;
	}

	
	rv = -EBUSY;
	if (lkb->lkb_wait_type || lkb->lkb_wait_count)
		goto out;

 out_ok:
	
	lkb->lkb_exflags |= args->flags;
	lkb->lkb_sbflags = 0;
	lkb->lkb_astparam = args->astparam;
	rv = 0;
 out:
	if (rv)
		log_debug(ls, "validate_unlock_args %d %x %x %x %x %d %s", rv,
			  lkb->lkb_id, lkb->lkb_flags, lkb->lkb_exflags,
			  args->flags, lkb->lkb_wait_type,
			  lkb->lkb_resource->res_name);
	return rv;
}


static int do_request(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int error = 0;

	if (can_be_granted(r, lkb, 1, NULL)) {
		grant_lock(r, lkb);
		queue_cast(r, lkb, 0);
		goto out;
	}

	if (can_be_queued(lkb)) {
		error = -EINPROGRESS;
		add_lkb(r, lkb, DLM_LKSTS_WAITING);
		add_timeout(lkb);
		goto out;
	}

	error = -EAGAIN;
	queue_cast(r, lkb, -EAGAIN);
 out:
	return error;
}

static void do_request_effects(struct dlm_rsb *r, struct dlm_lkb *lkb,
			       int error)
{
	switch (error) {
	case -EAGAIN:
		if (force_blocking_asts(lkb))
			send_blocking_asts_all(r, lkb);
		break;
	case -EINPROGRESS:
		send_blocking_asts(r, lkb);
		break;
	}
}

static int do_convert(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int error = 0;
	int deadlk = 0;

	

	if (can_be_granted(r, lkb, 1, &deadlk)) {
		grant_lock(r, lkb);
		queue_cast(r, lkb, 0);
		goto out;
	}


	if (deadlk) {
		
		revert_lock(r, lkb);
		queue_cast(r, lkb, -EDEADLK);
		error = -EDEADLK;
		goto out;
	}


	if (is_demoted(lkb)) {
		grant_pending_convert(r, DLM_LOCK_IV, NULL);
		if (_can_be_granted(r, lkb, 1)) {
			grant_lock(r, lkb);
			queue_cast(r, lkb, 0);
			goto out;
		}
		
	}

	if (can_be_queued(lkb)) {
		error = -EINPROGRESS;
		del_lkb(r, lkb);
		add_lkb(r, lkb, DLM_LKSTS_CONVERT);
		add_timeout(lkb);
		goto out;
	}

	error = -EAGAIN;
	queue_cast(r, lkb, -EAGAIN);
 out:
	return error;
}

static void do_convert_effects(struct dlm_rsb *r, struct dlm_lkb *lkb,
			       int error)
{
	switch (error) {
	case 0:
		grant_pending_locks(r);
		
		break;
	case -EAGAIN:
		if (force_blocking_asts(lkb))
			send_blocking_asts_all(r, lkb);
		break;
	case -EINPROGRESS:
		send_blocking_asts(r, lkb);
		break;
	}
}

static int do_unlock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	remove_lock(r, lkb);
	queue_cast(r, lkb, -DLM_EUNLOCK);
	return -DLM_EUNLOCK;
}

static void do_unlock_effects(struct dlm_rsb *r, struct dlm_lkb *lkb,
			      int error)
{
	grant_pending_locks(r);
}

 
static int do_cancel(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int error;

	error = revert_lock(r, lkb);
	if (error) {
		queue_cast(r, lkb, -DLM_ECANCEL);
		return -DLM_ECANCEL;
	}
	return 0;
}

static void do_cancel_effects(struct dlm_rsb *r, struct dlm_lkb *lkb,
			      int error)
{
	if (error)
		grant_pending_locks(r);
}



static int _request_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int error;

	

	error = set_master(r, lkb);
	if (error < 0)
		goto out;
	if (error) {
		error = 0;
		goto out;
	}

	if (is_remote(r)) {
		
		error = send_request(r, lkb);
	} else {
		error = do_request(r, lkb);
		do_request_effects(r, lkb, error);
	}
 out:
	return error;
}


static int _convert_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int error;

	if (is_remote(r)) {
		
		error = send_convert(r, lkb);
	} else {
		error = do_convert(r, lkb);
		do_convert_effects(r, lkb, error);
	}

	return error;
}


static int _unlock_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int error;

	if (is_remote(r)) {
		
		error = send_unlock(r, lkb);
	} else {
		error = do_unlock(r, lkb);
		do_unlock_effects(r, lkb, error);
	}

	return error;
}


static int _cancel_lock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int error;

	if (is_remote(r)) {
		
		error = send_cancel(r, lkb);
	} else {
		error = do_cancel(r, lkb);
		do_cancel_effects(r, lkb, error);
	}

	return error;
}


static int request_lock(struct dlm_ls *ls, struct dlm_lkb *lkb, char *name,
			int len, struct dlm_args *args)
{
	struct dlm_rsb *r;
	int error;

	error = validate_lock_args(ls, lkb, args);
	if (error)
		goto out;

	error = find_rsb(ls, name, len, R_CREATE, &r);
	if (error)
		goto out;

	lock_rsb(r);

	attach_lkb(r, lkb);
	lkb->lkb_lksb->sb_lkid = lkb->lkb_id;

	error = _request_lock(r, lkb);

	unlock_rsb(r);
	put_rsb(r);

 out:
	return error;
}

static int convert_lock(struct dlm_ls *ls, struct dlm_lkb *lkb,
			struct dlm_args *args)
{
	struct dlm_rsb *r;
	int error;

	r = lkb->lkb_resource;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_lock_args(ls, lkb, args);
	if (error)
		goto out;

	error = _convert_lock(r, lkb);
 out:
	unlock_rsb(r);
	put_rsb(r);
	return error;
}

static int unlock_lock(struct dlm_ls *ls, struct dlm_lkb *lkb,
		       struct dlm_args *args)
{
	struct dlm_rsb *r;
	int error;

	r = lkb->lkb_resource;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_unlock_args(lkb, args);
	if (error)
		goto out;

	error = _unlock_lock(r, lkb);
 out:
	unlock_rsb(r);
	put_rsb(r);
	return error;
}

static int cancel_lock(struct dlm_ls *ls, struct dlm_lkb *lkb,
		       struct dlm_args *args)
{
	struct dlm_rsb *r;
	int error;

	r = lkb->lkb_resource;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_unlock_args(lkb, args);
	if (error)
		goto out;

	error = _cancel_lock(r, lkb);
 out:
	unlock_rsb(r);
	put_rsb(r);
	return error;
}


int dlm_lock(dlm_lockspace_t *lockspace,
	     int mode,
	     struct dlm_lksb *lksb,
	     uint32_t flags,
	     void *name,
	     unsigned int namelen,
	     uint32_t parent_lkid,
	     void (*ast) (void *astarg),
	     void *astarg,
	     void (*bast) (void *astarg, int mode))
{
	struct dlm_ls *ls;
	struct dlm_lkb *lkb;
	struct dlm_args args;
	int error, convert = flags & DLM_LKF_CONVERT;

	ls = dlm_find_lockspace_local(lockspace);
	if (!ls)
		return -EINVAL;

	dlm_lock_recovery(ls);

	if (convert)
		error = find_lkb(ls, lksb->sb_lkid, &lkb);
	else
		error = create_lkb(ls, &lkb);

	if (error)
		goto out;

	error = set_lock_args(mode, lksb, flags, namelen, 0, ast,
			      astarg, bast, &args);
	if (error)
		goto out_put;

	if (convert)
		error = convert_lock(ls, lkb, &args);
	else
		error = request_lock(ls, lkb, name, namelen, &args);

	if (error == -EINPROGRESS)
		error = 0;
 out_put:
	if (convert || error)
		__put_lkb(ls, lkb);
	if (error == -EAGAIN || error == -EDEADLK)
		error = 0;
 out:
	dlm_unlock_recovery(ls);
	dlm_put_lockspace(ls);
	return error;
}

int dlm_unlock(dlm_lockspace_t *lockspace,
	       uint32_t lkid,
	       uint32_t flags,
	       struct dlm_lksb *lksb,
	       void *astarg)
{
	struct dlm_ls *ls;
	struct dlm_lkb *lkb;
	struct dlm_args args;
	int error;

	ls = dlm_find_lockspace_local(lockspace);
	if (!ls)
		return -EINVAL;

	dlm_lock_recovery(ls);

	error = find_lkb(ls, lkid, &lkb);
	if (error)
		goto out;

	error = set_unlock_args(flags, astarg, &args);
	if (error)
		goto out_put;

	if (flags & DLM_LKF_CANCEL)
		error = cancel_lock(ls, lkb, &args);
	else
		error = unlock_lock(ls, lkb, &args);

	if (error == -DLM_EUNLOCK || error == -DLM_ECANCEL)
		error = 0;
	if (error == -EBUSY && (flags & (DLM_LKF_CANCEL | DLM_LKF_FORCEUNLOCK)))
		error = 0;
 out_put:
	dlm_put_lkb(lkb);
 out:
	dlm_unlock_recovery(ls);
	dlm_put_lockspace(ls);
	return error;
}


static int _create_message(struct dlm_ls *ls, int mb_len,
			   int to_nodeid, int mstype,
			   struct dlm_message **ms_ret,
			   struct dlm_mhandle **mh_ret)
{
	struct dlm_message *ms;
	struct dlm_mhandle *mh;
	char *mb;


	mh = dlm_lowcomms_get_buffer(to_nodeid, mb_len, GFP_NOFS, &mb);
	if (!mh)
		return -ENOBUFS;

	memset(mb, 0, mb_len);

	ms = (struct dlm_message *) mb;

	ms->m_header.h_version = (DLM_HEADER_MAJOR | DLM_HEADER_MINOR);
	ms->m_header.h_lockspace = ls->ls_global_id;
	ms->m_header.h_nodeid = dlm_our_nodeid();
	ms->m_header.h_length = mb_len;
	ms->m_header.h_cmd = DLM_MSG;

	ms->m_type = mstype;

	*mh_ret = mh;
	*ms_ret = ms;
	return 0;
}

static int create_message(struct dlm_rsb *r, struct dlm_lkb *lkb,
			  int to_nodeid, int mstype,
			  struct dlm_message **ms_ret,
			  struct dlm_mhandle **mh_ret)
{
	int mb_len = sizeof(struct dlm_message);

	switch (mstype) {
	case DLM_MSG_REQUEST:
	case DLM_MSG_LOOKUP:
	case DLM_MSG_REMOVE:
		mb_len += r->res_length;
		break;
	case DLM_MSG_CONVERT:
	case DLM_MSG_UNLOCK:
	case DLM_MSG_REQUEST_REPLY:
	case DLM_MSG_CONVERT_REPLY:
	case DLM_MSG_GRANT:
		if (lkb && lkb->lkb_lvbptr)
			mb_len += r->res_ls->ls_lvblen;
		break;
	}

	return _create_message(r->res_ls, mb_len, to_nodeid, mstype,
			       ms_ret, mh_ret);
}


static int send_message(struct dlm_mhandle *mh, struct dlm_message *ms)
{
	dlm_message_out(ms);
	dlm_lowcomms_commit_buffer(mh);
	return 0;
}

static void send_args(struct dlm_rsb *r, struct dlm_lkb *lkb,
		      struct dlm_message *ms)
{
	ms->m_nodeid   = lkb->lkb_nodeid;
	ms->m_pid      = lkb->lkb_ownpid;
	ms->m_lkid     = lkb->lkb_id;
	ms->m_remid    = lkb->lkb_remid;
	ms->m_exflags  = lkb->lkb_exflags;
	ms->m_sbflags  = lkb->lkb_sbflags;
	ms->m_flags    = lkb->lkb_flags;
	ms->m_lvbseq   = lkb->lkb_lvbseq;
	ms->m_status   = lkb->lkb_status;
	ms->m_grmode   = lkb->lkb_grmode;
	ms->m_rqmode   = lkb->lkb_rqmode;
	ms->m_hash     = r->res_hash;


	if (lkb->lkb_bastfn)
		ms->m_asts |= DLM_CB_BAST;
	if (lkb->lkb_astfn)
		ms->m_asts |= DLM_CB_CAST;


	switch (ms->m_type) {
	case DLM_MSG_REQUEST:
	case DLM_MSG_LOOKUP:
		memcpy(ms->m_extra, r->res_name, r->res_length);
		break;
	case DLM_MSG_CONVERT:
	case DLM_MSG_UNLOCK:
	case DLM_MSG_REQUEST_REPLY:
	case DLM_MSG_CONVERT_REPLY:
	case DLM_MSG_GRANT:
		if (!lkb->lkb_lvbptr)
			break;
		memcpy(ms->m_extra, lkb->lkb_lvbptr, r->res_ls->ls_lvblen);
		break;
	}
}

static int send_common(struct dlm_rsb *r, struct dlm_lkb *lkb, int mstype)
{
	struct dlm_message *ms;
	struct dlm_mhandle *mh;
	int to_nodeid, error;

	to_nodeid = r->res_nodeid;

	error = add_to_waiters(lkb, mstype, to_nodeid);
	if (error)
		return error;

	error = create_message(r, lkb, to_nodeid, mstype, &ms, &mh);
	if (error)
		goto fail;

	send_args(r, lkb, ms);

	error = send_message(mh, ms);
	if (error)
		goto fail;
	return 0;

 fail:
	remove_from_waiters(lkb, msg_reply_type(mstype));
	return error;
}

static int send_request(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	return send_common(r, lkb, DLM_MSG_REQUEST);
}

static int send_convert(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	int error;

	error = send_common(r, lkb, DLM_MSG_CONVERT);

	
	if (!error && down_conversion(lkb)) {
		remove_from_waiters(lkb, DLM_MSG_CONVERT_REPLY);
		r->res_ls->ls_stub_ms.m_flags = DLM_IFL_STUB_MS;
		r->res_ls->ls_stub_ms.m_type = DLM_MSG_CONVERT_REPLY;
		r->res_ls->ls_stub_ms.m_result = 0;
		__receive_convert_reply(r, lkb, &r->res_ls->ls_stub_ms);
	}

	return error;
}


static int send_unlock(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	return send_common(r, lkb, DLM_MSG_UNLOCK);
}

static int send_cancel(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	return send_common(r, lkb, DLM_MSG_CANCEL);
}

static int send_grant(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	struct dlm_message *ms;
	struct dlm_mhandle *mh;
	int to_nodeid, error;

	to_nodeid = lkb->lkb_nodeid;

	error = create_message(r, lkb, to_nodeid, DLM_MSG_GRANT, &ms, &mh);
	if (error)
		goto out;

	send_args(r, lkb, ms);

	ms->m_result = 0;

	error = send_message(mh, ms);
 out:
	return error;
}

static int send_bast(struct dlm_rsb *r, struct dlm_lkb *lkb, int mode)
{
	struct dlm_message *ms;
	struct dlm_mhandle *mh;
	int to_nodeid, error;

	to_nodeid = lkb->lkb_nodeid;

	error = create_message(r, NULL, to_nodeid, DLM_MSG_BAST, &ms, &mh);
	if (error)
		goto out;

	send_args(r, lkb, ms);

	ms->m_bastmode = mode;

	error = send_message(mh, ms);
 out:
	return error;
}

static int send_lookup(struct dlm_rsb *r, struct dlm_lkb *lkb)
{
	struct dlm_message *ms;
	struct dlm_mhandle *mh;
	int to_nodeid, error;

	to_nodeid = dlm_dir_nodeid(r);

	error = add_to_waiters(lkb, DLM_MSG_LOOKUP, to_nodeid);
	if (error)
		return error;

	error = create_message(r, NULL, to_nodeid, DLM_MSG_LOOKUP, &ms, &mh);
	if (error)
		goto fail;

	send_args(r, lkb, ms);

	error = send_message(mh, ms);
	if (error)
		goto fail;
	return 0;

 fail:
	remove_from_waiters(lkb, DLM_MSG_LOOKUP_REPLY);
	return error;
}

static int send_remove(struct dlm_rsb *r)
{
	struct dlm_message *ms;
	struct dlm_mhandle *mh;
	int to_nodeid, error;

	to_nodeid = dlm_dir_nodeid(r);

	error = create_message(r, NULL, to_nodeid, DLM_MSG_REMOVE, &ms, &mh);
	if (error)
		goto out;

	memcpy(ms->m_extra, r->res_name, r->res_length);
	ms->m_hash = r->res_hash;

	error = send_message(mh, ms);
 out:
	return error;
}

static int send_common_reply(struct dlm_rsb *r, struct dlm_lkb *lkb,
			     int mstype, int rv)
{
	struct dlm_message *ms;
	struct dlm_mhandle *mh;
	int to_nodeid, error;

	to_nodeid = lkb->lkb_nodeid;

	error = create_message(r, lkb, to_nodeid, mstype, &ms, &mh);
	if (error)
		goto out;

	send_args(r, lkb, ms);

	ms->m_result = rv;

	error = send_message(mh, ms);
 out:
	return error;
}

static int send_request_reply(struct dlm_rsb *r, struct dlm_lkb *lkb, int rv)
{
	return send_common_reply(r, lkb, DLM_MSG_REQUEST_REPLY, rv);
}

static int send_convert_reply(struct dlm_rsb *r, struct dlm_lkb *lkb, int rv)
{
	return send_common_reply(r, lkb, DLM_MSG_CONVERT_REPLY, rv);
}

static int send_unlock_reply(struct dlm_rsb *r, struct dlm_lkb *lkb, int rv)
{
	return send_common_reply(r, lkb, DLM_MSG_UNLOCK_REPLY, rv);
}

static int send_cancel_reply(struct dlm_rsb *r, struct dlm_lkb *lkb, int rv)
{
	return send_common_reply(r, lkb, DLM_MSG_CANCEL_REPLY, rv);
}

static int send_lookup_reply(struct dlm_ls *ls, struct dlm_message *ms_in,
			     int ret_nodeid, int rv)
{
	struct dlm_rsb *r = &ls->ls_stub_rsb;
	struct dlm_message *ms;
	struct dlm_mhandle *mh;
	int error, nodeid = ms_in->m_header.h_nodeid;

	error = create_message(r, NULL, nodeid, DLM_MSG_LOOKUP_REPLY, &ms, &mh);
	if (error)
		goto out;

	ms->m_lkid = ms_in->m_lkid;
	ms->m_result = rv;
	ms->m_nodeid = ret_nodeid;

	error = send_message(mh, ms);
 out:
	return error;
}


static void receive_flags(struct dlm_lkb *lkb, struct dlm_message *ms)
{
	lkb->lkb_exflags = ms->m_exflags;
	lkb->lkb_sbflags = ms->m_sbflags;
	lkb->lkb_flags = (lkb->lkb_flags & 0xFFFF0000) |
		         (ms->m_flags & 0x0000FFFF);
}

static void receive_flags_reply(struct dlm_lkb *lkb, struct dlm_message *ms)
{
	if (ms->m_flags == DLM_IFL_STUB_MS)
		return;

	lkb->lkb_sbflags = ms->m_sbflags;
	lkb->lkb_flags = (lkb->lkb_flags & 0xFFFF0000) |
		         (ms->m_flags & 0x0000FFFF);
}

static int receive_extralen(struct dlm_message *ms)
{
	return (ms->m_header.h_length - sizeof(struct dlm_message));
}

static int receive_lvb(struct dlm_ls *ls, struct dlm_lkb *lkb,
		       struct dlm_message *ms)
{
	int len;

	if (lkb->lkb_exflags & DLM_LKF_VALBLK) {
		if (!lkb->lkb_lvbptr)
			lkb->lkb_lvbptr = dlm_allocate_lvb(ls);
		if (!lkb->lkb_lvbptr)
			return -ENOMEM;
		len = receive_extralen(ms);
		if (len > DLM_RESNAME_MAXLEN)
			len = DLM_RESNAME_MAXLEN;
		memcpy(lkb->lkb_lvbptr, ms->m_extra, len);
	}
	return 0;
}

static void fake_bastfn(void *astparam, int mode)
{
	log_print("fake_bastfn should not be called");
}

static void fake_astfn(void *astparam)
{
	log_print("fake_astfn should not be called");
}

static int receive_request_args(struct dlm_ls *ls, struct dlm_lkb *lkb,
				struct dlm_message *ms)
{
	lkb->lkb_nodeid = ms->m_header.h_nodeid;
	lkb->lkb_ownpid = ms->m_pid;
	lkb->lkb_remid = ms->m_lkid;
	lkb->lkb_grmode = DLM_LOCK_IV;
	lkb->lkb_rqmode = ms->m_rqmode;

	lkb->lkb_bastfn = (ms->m_asts & DLM_CB_BAST) ? &fake_bastfn : NULL;
	lkb->lkb_astfn = (ms->m_asts & DLM_CB_CAST) ? &fake_astfn : NULL;

	if (lkb->lkb_exflags & DLM_LKF_VALBLK) {
		
		lkb->lkb_lvbptr = dlm_allocate_lvb(ls);
		if (!lkb->lkb_lvbptr)
			return -ENOMEM;
	}

	return 0;
}

static int receive_convert_args(struct dlm_ls *ls, struct dlm_lkb *lkb,
				struct dlm_message *ms)
{
	if (lkb->lkb_status != DLM_LKSTS_GRANTED)
		return -EBUSY;

	if (receive_lvb(ls, lkb, ms))
		return -ENOMEM;

	lkb->lkb_rqmode = ms->m_rqmode;
	lkb->lkb_lvbseq = ms->m_lvbseq;

	return 0;
}

static int receive_unlock_args(struct dlm_ls *ls, struct dlm_lkb *lkb,
			       struct dlm_message *ms)
{
	if (receive_lvb(ls, lkb, ms))
		return -ENOMEM;
	return 0;
}


static void setup_stub_lkb(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb = &ls->ls_stub_lkb;
	lkb->lkb_nodeid = ms->m_header.h_nodeid;
	lkb->lkb_remid = ms->m_lkid;
}


static int validate_message(struct dlm_lkb *lkb, struct dlm_message *ms)
{
	int from = ms->m_header.h_nodeid;
	int error = 0;

	switch (ms->m_type) {
	case DLM_MSG_CONVERT:
	case DLM_MSG_UNLOCK:
	case DLM_MSG_CANCEL:
		if (!is_master_copy(lkb) || lkb->lkb_nodeid != from)
			error = -EINVAL;
		break;

	case DLM_MSG_CONVERT_REPLY:
	case DLM_MSG_UNLOCK_REPLY:
	case DLM_MSG_CANCEL_REPLY:
	case DLM_MSG_GRANT:
	case DLM_MSG_BAST:
		if (!is_process_copy(lkb) || lkb->lkb_nodeid != from)
			error = -EINVAL;
		break;

	case DLM_MSG_REQUEST_REPLY:
		if (!is_process_copy(lkb))
			error = -EINVAL;
		else if (lkb->lkb_nodeid != -1 && lkb->lkb_nodeid != from)
			error = -EINVAL;
		break;

	default:
		error = -EINVAL;
	}

	if (error)
		log_error(lkb->lkb_resource->res_ls,
			  "ignore invalid message %d from %d %x %x %x %d",
			  ms->m_type, from, lkb->lkb_id, lkb->lkb_remid,
			  lkb->lkb_flags, lkb->lkb_nodeid);
	return error;
}

static void receive_request(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	struct dlm_rsb *r;
	int error, namelen;

	error = create_lkb(ls, &lkb);
	if (error)
		goto fail;

	receive_flags(lkb, ms);
	lkb->lkb_flags |= DLM_IFL_MSTCPY;
	error = receive_request_args(ls, lkb, ms);
	if (error) {
		__put_lkb(ls, lkb);
		goto fail;
	}

	namelen = receive_extralen(ms);

	error = find_rsb(ls, ms->m_extra, namelen, R_MASTER, &r);
	if (error) {
		__put_lkb(ls, lkb);
		goto fail;
	}

	lock_rsb(r);

	attach_lkb(r, lkb);
	error = do_request(r, lkb);
	send_request_reply(r, lkb, error);
	do_request_effects(r, lkb, error);

	unlock_rsb(r);
	put_rsb(r);

	if (error == -EINPROGRESS)
		error = 0;
	if (error)
		dlm_put_lkb(lkb);
	return;

 fail:
	setup_stub_lkb(ls, ms);
	send_request_reply(&ls->ls_stub_rsb, &ls->ls_stub_lkb, error);
}

static void receive_convert(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	struct dlm_rsb *r;
	int error, reply = 1;

	error = find_lkb(ls, ms->m_remid, &lkb);
	if (error)
		goto fail;

	r = lkb->lkb_resource;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_message(lkb, ms);
	if (error)
		goto out;

	receive_flags(lkb, ms);

	error = receive_convert_args(ls, lkb, ms);
	if (error) {
		send_convert_reply(r, lkb, error);
		goto out;
	}

	reply = !down_conversion(lkb);

	error = do_convert(r, lkb);
	if (reply)
		send_convert_reply(r, lkb, error);
	do_convert_effects(r, lkb, error);
 out:
	unlock_rsb(r);
	put_rsb(r);
	dlm_put_lkb(lkb);
	return;

 fail:
	setup_stub_lkb(ls, ms);
	send_convert_reply(&ls->ls_stub_rsb, &ls->ls_stub_lkb, error);
}

static void receive_unlock(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	struct dlm_rsb *r;
	int error;

	error = find_lkb(ls, ms->m_remid, &lkb);
	if (error)
		goto fail;

	r = lkb->lkb_resource;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_message(lkb, ms);
	if (error)
		goto out;

	receive_flags(lkb, ms);

	error = receive_unlock_args(ls, lkb, ms);
	if (error) {
		send_unlock_reply(r, lkb, error);
		goto out;
	}

	error = do_unlock(r, lkb);
	send_unlock_reply(r, lkb, error);
	do_unlock_effects(r, lkb, error);
 out:
	unlock_rsb(r);
	put_rsb(r);
	dlm_put_lkb(lkb);
	return;

 fail:
	setup_stub_lkb(ls, ms);
	send_unlock_reply(&ls->ls_stub_rsb, &ls->ls_stub_lkb, error);
}

static void receive_cancel(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	struct dlm_rsb *r;
	int error;

	error = find_lkb(ls, ms->m_remid, &lkb);
	if (error)
		goto fail;

	receive_flags(lkb, ms);

	r = lkb->lkb_resource;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_message(lkb, ms);
	if (error)
		goto out;

	error = do_cancel(r, lkb);
	send_cancel_reply(r, lkb, error);
	do_cancel_effects(r, lkb, error);
 out:
	unlock_rsb(r);
	put_rsb(r);
	dlm_put_lkb(lkb);
	return;

 fail:
	setup_stub_lkb(ls, ms);
	send_cancel_reply(&ls->ls_stub_rsb, &ls->ls_stub_lkb, error);
}

static void receive_grant(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	struct dlm_rsb *r;
	int error;

	error = find_lkb(ls, ms->m_remid, &lkb);
	if (error) {
		log_debug(ls, "receive_grant from %d no lkb %x",
			  ms->m_header.h_nodeid, ms->m_remid);
		return;
	}

	r = lkb->lkb_resource;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_message(lkb, ms);
	if (error)
		goto out;

	receive_flags_reply(lkb, ms);
	if (is_altmode(lkb))
		munge_altmode(lkb, ms);
	grant_lock_pc(r, lkb, ms);
	queue_cast(r, lkb, 0);
 out:
	unlock_rsb(r);
	put_rsb(r);
	dlm_put_lkb(lkb);
}

static void receive_bast(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	struct dlm_rsb *r;
	int error;

	error = find_lkb(ls, ms->m_remid, &lkb);
	if (error) {
		log_debug(ls, "receive_bast from %d no lkb %x",
			  ms->m_header.h_nodeid, ms->m_remid);
		return;
	}

	r = lkb->lkb_resource;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_message(lkb, ms);
	if (error)
		goto out;

	queue_bast(r, lkb, ms->m_bastmode);
 out:
	unlock_rsb(r);
	put_rsb(r);
	dlm_put_lkb(lkb);
}

static void receive_lookup(struct dlm_ls *ls, struct dlm_message *ms)
{
	int len, error, ret_nodeid, dir_nodeid, from_nodeid, our_nodeid;

	from_nodeid = ms->m_header.h_nodeid;
	our_nodeid = dlm_our_nodeid();

	len = receive_extralen(ms);

	dir_nodeid = dlm_hash2nodeid(ls, ms->m_hash);
	if (dir_nodeid != our_nodeid) {
		log_error(ls, "lookup dir_nodeid %d from %d",
			  dir_nodeid, from_nodeid);
		error = -EINVAL;
		ret_nodeid = -1;
		goto out;
	}

	error = dlm_dir_lookup(ls, from_nodeid, ms->m_extra, len, &ret_nodeid);

	
	if (!error && ret_nodeid == our_nodeid) {
		receive_request(ls, ms);
		return;
	}
 out:
	send_lookup_reply(ls, ms, ret_nodeid, error);
}

static void receive_remove(struct dlm_ls *ls, struct dlm_message *ms)
{
	int len, dir_nodeid, from_nodeid;

	from_nodeid = ms->m_header.h_nodeid;

	len = receive_extralen(ms);

	dir_nodeid = dlm_hash2nodeid(ls, ms->m_hash);
	if (dir_nodeid != dlm_our_nodeid()) {
		log_error(ls, "remove dir entry dir_nodeid %d from %d",
			  dir_nodeid, from_nodeid);
		return;
	}

	dlm_dir_remove_entry(ls, from_nodeid, ms->m_extra, len);
}

static void receive_purge(struct dlm_ls *ls, struct dlm_message *ms)
{
	do_purge(ls, ms->m_nodeid, ms->m_pid);
}

static void receive_request_reply(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	struct dlm_rsb *r;
	int error, mstype, result;

	error = find_lkb(ls, ms->m_remid, &lkb);
	if (error) {
		log_debug(ls, "receive_request_reply from %d no lkb %x",
			  ms->m_header.h_nodeid, ms->m_remid);
		return;
	}

	r = lkb->lkb_resource;
	hold_rsb(r);
	lock_rsb(r);

	error = validate_message(lkb, ms);
	if (error)
		goto out;

	mstype = lkb->lkb_wait_type;
	error = remove_from_waiters(lkb, DLM_MSG_REQUEST_REPLY);
	if (error)
		goto out;

	if (mstype == DLM_MSG_LOOKUP) {
		r->res_nodeid = ms->m_header.h_nodeid;
		lkb->lkb_nodeid = r->res_nodeid;
	}

	
	result = ms->m_result;

	switch (result) {
	case -EAGAIN:
		
		queue_cast(r, lkb, -EAGAIN);
		confirm_master(r, -EAGAIN);
		unhold_lkb(lkb); 
		break;

	case -EINPROGRESS:
	case 0:
		
		receive_flags_reply(lkb, ms);
		lkb->lkb_remid = ms->m_lkid;
		if (is_altmode(lkb))
			munge_altmode(lkb, ms);
		if (result) {
			add_lkb(r, lkb, DLM_LKSTS_WAITING);
			add_timeout(lkb);
		} else {
			grant_lock_pc(r, lkb, ms);
			queue_cast(r, lkb, 0);
		}
		confirm_master(r, result);
		break;

	case -EBADR:
	case -ENOTBLK:
		
		log_debug(ls, "receive_request_reply %x %x master diff %d %d",
			  lkb->lkb_id, lkb->lkb_flags, r->res_nodeid, result);
		r->res_nodeid = -1;
		lkb->lkb_nodeid = -1;

		if (is_overlap(lkb)) {
			
			queue_cast_overlap(r, lkb);
			confirm_master(r, result);
			unhold_lkb(lkb); 
		} else
			_request_lock(r, lkb);
		break;

	default:
		log_error(ls, "receive_request_reply %x error %d",
			  lkb->lkb_id, result);
	}

	if (is_overlap_unlock(lkb) && (result == 0 || result == -EINPROGRESS)) {
		log_debug(ls, "receive_request_reply %x result %d unlock",
			  lkb->lkb_id, result);
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_UNLOCK;
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_CANCEL;
		send_unlock(r, lkb);
	} else if (is_overlap_cancel(lkb) && (result == -EINPROGRESS)) {
		log_debug(ls, "receive_request_reply %x cancel", lkb->lkb_id);
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_UNLOCK;
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_CANCEL;
		send_cancel(r, lkb);
	} else {
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_CANCEL;
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_UNLOCK;
	}
 out:
	unlock_rsb(r);
	put_rsb(r);
	dlm_put_lkb(lkb);
}

static void __receive_convert_reply(struct dlm_rsb *r, struct dlm_lkb *lkb,
				    struct dlm_message *ms)
{
	
	switch (ms->m_result) {
	case -EAGAIN:
		
		queue_cast(r, lkb, -EAGAIN);
		break;

	case -EDEADLK:
		receive_flags_reply(lkb, ms);
		revert_lock_pc(r, lkb);
		queue_cast(r, lkb, -EDEADLK);
		break;

	case -EINPROGRESS:
		
		receive_flags_reply(lkb, ms);
		if (is_demoted(lkb))
			munge_demoted(lkb);
		del_lkb(r, lkb);
		add_lkb(r, lkb, DLM_LKSTS_CONVERT);
		add_timeout(lkb);
		break;

	case 0:
		
		receive_flags_reply(lkb, ms);
		if (is_demoted(lkb))
			munge_demoted(lkb);
		grant_lock_pc(r, lkb, ms);
		queue_cast(r, lkb, 0);
		break;

	default:
		log_error(r->res_ls, "receive_convert_reply %x error %d",
			  lkb->lkb_id, ms->m_result);
	}
}

static void _receive_convert_reply(struct dlm_lkb *lkb, struct dlm_message *ms)
{
	struct dlm_rsb *r = lkb->lkb_resource;
	int error;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_message(lkb, ms);
	if (error)
		goto out;

	
	error = remove_from_waiters_ms(lkb, ms);
	if (error)
		goto out;

	__receive_convert_reply(r, lkb, ms);
 out:
	unlock_rsb(r);
	put_rsb(r);
}

static void receive_convert_reply(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	int error;

	error = find_lkb(ls, ms->m_remid, &lkb);
	if (error) {
		log_debug(ls, "receive_convert_reply from %d no lkb %x",
			  ms->m_header.h_nodeid, ms->m_remid);
		return;
	}

	_receive_convert_reply(lkb, ms);
	dlm_put_lkb(lkb);
}

static void _receive_unlock_reply(struct dlm_lkb *lkb, struct dlm_message *ms)
{
	struct dlm_rsb *r = lkb->lkb_resource;
	int error;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_message(lkb, ms);
	if (error)
		goto out;

	
	error = remove_from_waiters_ms(lkb, ms);
	if (error)
		goto out;

	

	switch (ms->m_result) {
	case -DLM_EUNLOCK:
		receive_flags_reply(lkb, ms);
		remove_lock_pc(r, lkb);
		queue_cast(r, lkb, -DLM_EUNLOCK);
		break;
	case -ENOENT:
		break;
	default:
		log_error(r->res_ls, "receive_unlock_reply %x error %d",
			  lkb->lkb_id, ms->m_result);
	}
 out:
	unlock_rsb(r);
	put_rsb(r);
}

static void receive_unlock_reply(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	int error;

	error = find_lkb(ls, ms->m_remid, &lkb);
	if (error) {
		log_debug(ls, "receive_unlock_reply from %d no lkb %x",
			  ms->m_header.h_nodeid, ms->m_remid);
		return;
	}

	_receive_unlock_reply(lkb, ms);
	dlm_put_lkb(lkb);
}

static void _receive_cancel_reply(struct dlm_lkb *lkb, struct dlm_message *ms)
{
	struct dlm_rsb *r = lkb->lkb_resource;
	int error;

	hold_rsb(r);
	lock_rsb(r);

	error = validate_message(lkb, ms);
	if (error)
		goto out;

	
	error = remove_from_waiters_ms(lkb, ms);
	if (error)
		goto out;

	

	switch (ms->m_result) {
	case -DLM_ECANCEL:
		receive_flags_reply(lkb, ms);
		revert_lock_pc(r, lkb);
		queue_cast(r, lkb, -DLM_ECANCEL);
		break;
	case 0:
		break;
	default:
		log_error(r->res_ls, "receive_cancel_reply %x error %d",
			  lkb->lkb_id, ms->m_result);
	}
 out:
	unlock_rsb(r);
	put_rsb(r);
}

static void receive_cancel_reply(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	int error;

	error = find_lkb(ls, ms->m_remid, &lkb);
	if (error) {
		log_debug(ls, "receive_cancel_reply from %d no lkb %x",
			  ms->m_header.h_nodeid, ms->m_remid);
		return;
	}

	_receive_cancel_reply(lkb, ms);
	dlm_put_lkb(lkb);
}

static void receive_lookup_reply(struct dlm_ls *ls, struct dlm_message *ms)
{
	struct dlm_lkb *lkb;
	struct dlm_rsb *r;
	int error, ret_nodeid;

	error = find_lkb(ls, ms->m_lkid, &lkb);
	if (error) {
		log_error(ls, "receive_lookup_reply no lkb");
		return;
	}


	r = lkb->lkb_resource;
	hold_rsb(r);
	lock_rsb(r);

	error = remove_from_waiters(lkb, DLM_MSG_LOOKUP_REPLY);
	if (error)
		goto out;

	ret_nodeid = ms->m_nodeid;
	if (ret_nodeid == dlm_our_nodeid()) {
		r->res_nodeid = 0;
		ret_nodeid = 0;
		r->res_first_lkid = 0;
	} else {
		
		r->res_nodeid = ret_nodeid;
	}

	if (is_overlap(lkb)) {
		log_debug(ls, "receive_lookup_reply %x unlock %x",
			  lkb->lkb_id, lkb->lkb_flags);
		queue_cast_overlap(r, lkb);
		unhold_lkb(lkb); 
		goto out_list;
	}

	_request_lock(r, lkb);

 out_list:
	if (!ret_nodeid)
		process_lookup_list(r);
 out:
	unlock_rsb(r);
	put_rsb(r);
	dlm_put_lkb(lkb);
}

static void _receive_message(struct dlm_ls *ls, struct dlm_message *ms)
{
	if (!dlm_is_member(ls, ms->m_header.h_nodeid)) {
		log_debug(ls, "ignore non-member message %d from %d %x %x %d",
			  ms->m_type, ms->m_header.h_nodeid, ms->m_lkid,
			  ms->m_remid, ms->m_result);
		return;
	}

	switch (ms->m_type) {

	

	case DLM_MSG_REQUEST:
		receive_request(ls, ms);
		break;

	case DLM_MSG_CONVERT:
		receive_convert(ls, ms);
		break;

	case DLM_MSG_UNLOCK:
		receive_unlock(ls, ms);
		break;

	case DLM_MSG_CANCEL:
		receive_cancel(ls, ms);
		break;

	

	case DLM_MSG_REQUEST_REPLY:
		receive_request_reply(ls, ms);
		break;

	case DLM_MSG_CONVERT_REPLY:
		receive_convert_reply(ls, ms);
		break;

	case DLM_MSG_UNLOCK_REPLY:
		receive_unlock_reply(ls, ms);
		break;

	case DLM_MSG_CANCEL_REPLY:
		receive_cancel_reply(ls, ms);
		break;

	

	case DLM_MSG_GRANT:
		receive_grant(ls, ms);
		break;

	case DLM_MSG_BAST:
		receive_bast(ls, ms);
		break;

	

	case DLM_MSG_LOOKUP:
		receive_lookup(ls, ms);
		break;

	case DLM_MSG_REMOVE:
		receive_remove(ls, ms);
		break;

	

	case DLM_MSG_LOOKUP_REPLY:
		receive_lookup_reply(ls, ms);
		break;

	

	case DLM_MSG_PURGE:
		receive_purge(ls, ms);
		break;

	default:
		log_error(ls, "unknown message type %d", ms->m_type);
	}
}


static void dlm_receive_message(struct dlm_ls *ls, struct dlm_message *ms,
				int nodeid)
{
	if (dlm_locking_stopped(ls)) {
		dlm_add_requestqueue(ls, nodeid, ms);
	} else {
		dlm_wait_requestqueue(ls);
		_receive_message(ls, ms);
	}
}


void dlm_receive_message_saved(struct dlm_ls *ls, struct dlm_message *ms)
{
	_receive_message(ls, ms);
}


void dlm_receive_buffer(union dlm_packet *p, int nodeid)
{
	struct dlm_header *hd = &p->header;
	struct dlm_ls *ls;
	int type = 0;

	switch (hd->h_cmd) {
	case DLM_MSG:
		dlm_message_in(&p->message);
		type = p->message.m_type;
		break;
	case DLM_RCOM:
		dlm_rcom_in(&p->rcom);
		type = p->rcom.rc_type;
		break;
	default:
		log_print("invalid h_cmd %d from %u", hd->h_cmd, nodeid);
		return;
	}

	if (hd->h_nodeid != nodeid) {
		log_print("invalid h_nodeid %d from %d lockspace %x",
			  hd->h_nodeid, nodeid, hd->h_lockspace);
		return;
	}

	ls = dlm_find_lockspace_global(hd->h_lockspace);
	if (!ls) {
		if (dlm_config.ci_log_debug)
			log_print("invalid lockspace %x from %d cmd %d type %d",
				  hd->h_lockspace, nodeid, hd->h_cmd, type);

		if (hd->h_cmd == DLM_RCOM && type == DLM_RCOM_STATUS)
			dlm_send_ls_not_ready(nodeid, &p->rcom);
		return;
	}


	down_read(&ls->ls_recv_active);
	if (hd->h_cmd == DLM_MSG)
		dlm_receive_message(ls, &p->message, nodeid);
	else
		dlm_receive_rcom(ls, &p->rcom, nodeid);
	up_read(&ls->ls_recv_active);

	dlm_put_lockspace(ls);
}

static void recover_convert_waiter(struct dlm_ls *ls, struct dlm_lkb *lkb,
				   struct dlm_message *ms_stub)
{
	if (middle_conversion(lkb)) {
		hold_lkb(lkb);
		memset(ms_stub, 0, sizeof(struct dlm_message));
		ms_stub->m_flags = DLM_IFL_STUB_MS;
		ms_stub->m_type = DLM_MSG_CONVERT_REPLY;
		ms_stub->m_result = -EINPROGRESS;
		ms_stub->m_header.h_nodeid = lkb->lkb_nodeid;
		_receive_convert_reply(lkb, ms_stub);

		
		lkb->lkb_grmode = DLM_LOCK_IV;
		rsb_set_flag(lkb->lkb_resource, RSB_RECOVER_CONVERT);
		unhold_lkb(lkb);

	} else if (lkb->lkb_rqmode >= lkb->lkb_grmode) {
		lkb->lkb_flags |= DLM_IFL_RESEND;
	}

}


static int waiter_needs_recovery(struct dlm_ls *ls, struct dlm_lkb *lkb)
{
	if (dlm_is_removed(ls, lkb->lkb_nodeid))
		return 1;

	if (!dlm_no_directory(ls))
		return 0;

	if (dlm_dir_nodeid(lkb->lkb_resource) != lkb->lkb_nodeid)
		return 1;

	return 0;
}


void dlm_recover_waiters_pre(struct dlm_ls *ls)
{
	struct dlm_lkb *lkb, *safe;
	struct dlm_message *ms_stub;
	int wait_type, stub_unlock_result, stub_cancel_result;

	ms_stub = kmalloc(sizeof(struct dlm_message), GFP_KERNEL);
	if (!ms_stub) {
		log_error(ls, "dlm_recover_waiters_pre no mem");
		return;
	}

	mutex_lock(&ls->ls_waiters_mutex);

	list_for_each_entry_safe(lkb, safe, &ls->ls_waiters, lkb_wait_reply) {


		if (lkb->lkb_wait_type != DLM_MSG_UNLOCK) {
			log_debug(ls, "recover_waiter %x nodeid %d "
				  "msg %d to %d", lkb->lkb_id, lkb->lkb_nodeid,
				  lkb->lkb_wait_type, lkb->lkb_wait_nodeid);
		}


		if (lkb->lkb_wait_type == DLM_MSG_LOOKUP) {
			lkb->lkb_flags |= DLM_IFL_RESEND;
			continue;
		}

		if (!waiter_needs_recovery(ls, lkb))
			continue;

		wait_type = lkb->lkb_wait_type;
		stub_unlock_result = -DLM_EUNLOCK;
		stub_cancel_result = -DLM_ECANCEL;


		if (!wait_type) {
			if (is_overlap_cancel(lkb)) {
				wait_type = DLM_MSG_CANCEL;
				if (lkb->lkb_grmode == DLM_LOCK_IV)
					stub_cancel_result = 0;
			}
			if (is_overlap_unlock(lkb)) {
				wait_type = DLM_MSG_UNLOCK;
				if (lkb->lkb_grmode == DLM_LOCK_IV)
					stub_unlock_result = -ENOENT;
			}

			log_debug(ls, "rwpre overlap %x %x %d %d %d",
				  lkb->lkb_id, lkb->lkb_flags, wait_type,
				  stub_cancel_result, stub_unlock_result);
		}

		switch (wait_type) {

		case DLM_MSG_REQUEST:
			lkb->lkb_flags |= DLM_IFL_RESEND;
			break;

		case DLM_MSG_CONVERT:
			recover_convert_waiter(ls, lkb, ms_stub);
			break;

		case DLM_MSG_UNLOCK:
			hold_lkb(lkb);
			memset(ms_stub, 0, sizeof(struct dlm_message));
			ms_stub->m_flags = DLM_IFL_STUB_MS;
			ms_stub->m_type = DLM_MSG_UNLOCK_REPLY;
			ms_stub->m_result = stub_unlock_result;
			ms_stub->m_header.h_nodeid = lkb->lkb_nodeid;
			_receive_unlock_reply(lkb, ms_stub);
			dlm_put_lkb(lkb);
			break;

		case DLM_MSG_CANCEL:
			hold_lkb(lkb);
			memset(ms_stub, 0, sizeof(struct dlm_message));
			ms_stub->m_flags = DLM_IFL_STUB_MS;
			ms_stub->m_type = DLM_MSG_CANCEL_REPLY;
			ms_stub->m_result = stub_cancel_result;
			ms_stub->m_header.h_nodeid = lkb->lkb_nodeid;
			_receive_cancel_reply(lkb, ms_stub);
			dlm_put_lkb(lkb);
			break;

		default:
			log_error(ls, "invalid lkb wait_type %d %d",
				  lkb->lkb_wait_type, wait_type);
		}
		schedule();
	}
	mutex_unlock(&ls->ls_waiters_mutex);
	kfree(ms_stub);
}

static struct dlm_lkb *find_resend_waiter(struct dlm_ls *ls)
{
	struct dlm_lkb *lkb;
	int found = 0;

	mutex_lock(&ls->ls_waiters_mutex);
	list_for_each_entry(lkb, &ls->ls_waiters, lkb_wait_reply) {
		if (lkb->lkb_flags & DLM_IFL_RESEND) {
			hold_lkb(lkb);
			found = 1;
			break;
		}
	}
	mutex_unlock(&ls->ls_waiters_mutex);

	if (!found)
		lkb = NULL;
	return lkb;
}




int dlm_recover_waiters_post(struct dlm_ls *ls)
{
	struct dlm_lkb *lkb;
	struct dlm_rsb *r;
	int error = 0, mstype, err, oc, ou;

	while (1) {
		if (dlm_locking_stopped(ls)) {
			log_debug(ls, "recover_waiters_post aborted");
			error = -EINTR;
			break;
		}

		lkb = find_resend_waiter(ls);
		if (!lkb)
			break;

		r = lkb->lkb_resource;
		hold_rsb(r);
		lock_rsb(r);

		mstype = lkb->lkb_wait_type;
		oc = is_overlap_cancel(lkb);
		ou = is_overlap_unlock(lkb);
		err = 0;

		log_debug(ls, "recover_waiter %x nodeid %d msg %d r_nodeid %d",
			  lkb->lkb_id, lkb->lkb_nodeid, mstype, r->res_nodeid);


		lkb->lkb_flags &= ~DLM_IFL_RESEND;
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_UNLOCK;
		lkb->lkb_flags &= ~DLM_IFL_OVERLAP_CANCEL;
		lkb->lkb_wait_type = 0;
		lkb->lkb_wait_count = 0;
		mutex_lock(&ls->ls_waiters_mutex);
		list_del_init(&lkb->lkb_wait_reply);
		mutex_unlock(&ls->ls_waiters_mutex);
		unhold_lkb(lkb); 

		if (oc || ou) {
			
			switch (mstype) {
			case DLM_MSG_LOOKUP:
			case DLM_MSG_REQUEST:
				queue_cast(r, lkb, ou ? -DLM_EUNLOCK :
							-DLM_ECANCEL);
				unhold_lkb(lkb); 
				break;
			case DLM_MSG_CONVERT:
				if (oc) {
					queue_cast(r, lkb, -DLM_ECANCEL);
				} else {
					lkb->lkb_exflags |= DLM_LKF_FORCEUNLOCK;
					_unlock_lock(r, lkb);
				}
				break;
			default:
				err = 1;
			}
		} else {
			switch (mstype) {
			case DLM_MSG_LOOKUP:
			case DLM_MSG_REQUEST:
				_request_lock(r, lkb);
				if (is_master(r))
					confirm_master(r, 0);
				break;
			case DLM_MSG_CONVERT:
				_convert_lock(r, lkb);
				break;
			default:
				err = 1;
			}
		}

		if (err)
			log_error(ls, "recover_waiters_post %x %d %x %d %d",
			  	  lkb->lkb_id, mstype, lkb->lkb_flags, oc, ou);
		unlock_rsb(r);
		put_rsb(r);
		dlm_put_lkb(lkb);
	}

	return error;
}

static void purge_queue(struct dlm_rsb *r, struct list_head *queue,
			int (*test)(struct dlm_ls *ls, struct dlm_lkb *lkb))
{
	struct dlm_ls *ls = r->res_ls;
	struct dlm_lkb *lkb, *safe;

	list_for_each_entry_safe(lkb, safe, queue, lkb_statequeue) {
		if (test(ls, lkb)) {
			rsb_set_flag(r, RSB_LOCKS_PURGED);
			del_lkb(r, lkb);
			
			if (!dlm_put_lkb(lkb))
				log_error(ls, "purged lkb not released");
		}
	}
}

static int purge_dead_test(struct dlm_ls *ls, struct dlm_lkb *lkb)
{
	return (is_master_copy(lkb) && dlm_is_removed(ls, lkb->lkb_nodeid));
}

static int purge_mstcpy_test(struct dlm_ls *ls, struct dlm_lkb *lkb)
{
	return is_master_copy(lkb);
}

static void purge_dead_locks(struct dlm_rsb *r)
{
	purge_queue(r, &r->res_grantqueue, &purge_dead_test);
	purge_queue(r, &r->res_convertqueue, &purge_dead_test);
	purge_queue(r, &r->res_waitqueue, &purge_dead_test);
}

void dlm_purge_mstcpy_locks(struct dlm_rsb *r)
{
	purge_queue(r, &r->res_grantqueue, &purge_mstcpy_test);
	purge_queue(r, &r->res_convertqueue, &purge_mstcpy_test);
	purge_queue(r, &r->res_waitqueue, &purge_mstcpy_test);
}


int dlm_purge_locks(struct dlm_ls *ls)
{
	struct dlm_rsb *r;

	log_debug(ls, "dlm_purge_locks");

	down_write(&ls->ls_root_sem);
	list_for_each_entry(r, &ls->ls_root_list, res_root_list) {
		hold_rsb(r);
		lock_rsb(r);
		if (is_master(r))
			purge_dead_locks(r);
		unlock_rsb(r);
		unhold_rsb(r);

		schedule();
	}
	up_write(&ls->ls_root_sem);

	return 0;
}

static struct dlm_rsb *find_purged_rsb(struct dlm_ls *ls, int bucket)
{
	struct rb_node *n;
	struct dlm_rsb *r, *r_ret = NULL;

	spin_lock(&ls->ls_rsbtbl[bucket].lock);
	for (n = rb_first(&ls->ls_rsbtbl[bucket].keep); n; n = rb_next(n)) {
		r = rb_entry(n, struct dlm_rsb, res_hashnode);
		if (!rsb_flag(r, RSB_LOCKS_PURGED))
			continue;
		hold_rsb(r);
		rsb_clear_flag(r, RSB_LOCKS_PURGED);
		r_ret = r;
		break;
	}
	spin_unlock(&ls->ls_rsbtbl[bucket].lock);
	return r_ret;
}

void dlm_grant_after_purge(struct dlm_ls *ls)
{
	struct dlm_rsb *r;
	int bucket = 0;

	while (1) {
		r = find_purged_rsb(ls, bucket);
		if (!r) {
			if (bucket == ls->ls_rsbtbl_size - 1)
				break;
			bucket++;
			continue;
		}
		lock_rsb(r);
		if (is_master(r)) {
			grant_pending_locks(r);
			confirm_master(r, 0);
		}
		unlock_rsb(r);
		put_rsb(r);
		schedule();
	}
}

static struct dlm_lkb *search_remid_list(struct list_head *head, int nodeid,
					 uint32_t remid)
{
	struct dlm_lkb *lkb;

	list_for_each_entry(lkb, head, lkb_statequeue) {
		if (lkb->lkb_nodeid == nodeid && lkb->lkb_remid == remid)
			return lkb;
	}
	return NULL;
}

static struct dlm_lkb *search_remid(struct dlm_rsb *r, int nodeid,
				    uint32_t remid)
{
	struct dlm_lkb *lkb;

	lkb = search_remid_list(&r->res_grantqueue, nodeid, remid);
	if (lkb)
		return lkb;
	lkb = search_remid_list(&r->res_convertqueue, nodeid, remid);
	if (lkb)
		return lkb;
	lkb = search_remid_list(&r->res_waitqueue, nodeid, remid);
	if (lkb)
		return lkb;
	return NULL;
}

static int receive_rcom_lock_args(struct dlm_ls *ls, struct dlm_lkb *lkb,
				  struct dlm_rsb *r, struct dlm_rcom *rc)
{
	struct rcom_lock *rl = (struct rcom_lock *) rc->rc_buf;

	lkb->lkb_nodeid = rc->rc_header.h_nodeid;
	lkb->lkb_ownpid = le32_to_cpu(rl->rl_ownpid);
	lkb->lkb_remid = le32_to_cpu(rl->rl_lkid);
	lkb->lkb_exflags = le32_to_cpu(rl->rl_exflags);
	lkb->lkb_flags = le32_to_cpu(rl->rl_flags) & 0x0000FFFF;
	lkb->lkb_flags |= DLM_IFL_MSTCPY;
	lkb->lkb_lvbseq = le32_to_cpu(rl->rl_lvbseq);
	lkb->lkb_rqmode = rl->rl_rqmode;
	lkb->lkb_grmode = rl->rl_grmode;
	

	lkb->lkb_bastfn = (rl->rl_asts & DLM_CB_BAST) ? &fake_bastfn : NULL;
	lkb->lkb_astfn = (rl->rl_asts & DLM_CB_CAST) ? &fake_astfn : NULL;

	if (lkb->lkb_exflags & DLM_LKF_VALBLK) {
		int lvblen = rc->rc_header.h_length - sizeof(struct dlm_rcom) -
			 sizeof(struct rcom_lock);
		if (lvblen > ls->ls_lvblen)
			return -EINVAL;
		lkb->lkb_lvbptr = dlm_allocate_lvb(ls);
		if (!lkb->lkb_lvbptr)
			return -ENOMEM;
		memcpy(lkb->lkb_lvbptr, rl->rl_lvb, lvblen);
	}


	if (rl->rl_wait_type == cpu_to_le16(DLM_MSG_CONVERT) &&
	    middle_conversion(lkb)) {
		rl->rl_status = DLM_LKSTS_CONVERT;
		lkb->lkb_grmode = DLM_LOCK_IV;
		rsb_set_flag(r, RSB_RECOVER_CONVERT);
	}

	return 0;
}


int dlm_recover_master_copy(struct dlm_ls *ls, struct dlm_rcom *rc)
{
	struct rcom_lock *rl = (struct rcom_lock *) rc->rc_buf;
	struct dlm_rsb *r;
	struct dlm_lkb *lkb;
	int error;

	if (rl->rl_parent_lkid) {
		error = -EOPNOTSUPP;
		goto out;
	}

	error = find_rsb(ls, rl->rl_name, le16_to_cpu(rl->rl_namelen),
			 R_MASTER, &r);
	if (error)
		goto out;

	lock_rsb(r);

	lkb = search_remid(r, rc->rc_header.h_nodeid, le32_to_cpu(rl->rl_lkid));
	if (lkb) {
		error = -EEXIST;
		goto out_remid;
	}

	error = create_lkb(ls, &lkb);
	if (error)
		goto out_unlock;

	error = receive_rcom_lock_args(ls, lkb, r, rc);
	if (error) {
		__put_lkb(ls, lkb);
		goto out_unlock;
	}

	attach_lkb(r, lkb);
	add_lkb(r, lkb, rl->rl_status);
	error = 0;

 out_remid:
	rl->rl_remid = cpu_to_le32(lkb->lkb_id);

 out_unlock:
	unlock_rsb(r);
	put_rsb(r);
 out:
	if (error)
		log_debug(ls, "recover_master_copy %d %x", error,
			  le32_to_cpu(rl->rl_lkid));
	rl->rl_result = cpu_to_le32(error);
	return error;
}

int dlm_recover_process_copy(struct dlm_ls *ls, struct dlm_rcom *rc)
{
	struct rcom_lock *rl = (struct rcom_lock *) rc->rc_buf;
	struct dlm_rsb *r;
	struct dlm_lkb *lkb;
	int error;

	error = find_lkb(ls, le32_to_cpu(rl->rl_lkid), &lkb);
	if (error) {
		log_error(ls, "recover_process_copy no lkid %x",
				le32_to_cpu(rl->rl_lkid));
		return error;
	}

	DLM_ASSERT(is_process_copy(lkb), dlm_print_lkb(lkb););

	error = le32_to_cpu(rl->rl_result);

	r = lkb->lkb_resource;
	hold_rsb(r);
	lock_rsb(r);

	switch (error) {
	case -EBADR:
		log_debug(ls, "master copy not ready %x r %lx %s", lkb->lkb_id,
			  (unsigned long)r, r->res_name);
		dlm_send_rcom_lock(r, lkb);
		goto out;
	case -EEXIST:
		log_debug(ls, "master copy exists %x", lkb->lkb_id);
		
	case 0:
		lkb->lkb_remid = le32_to_cpu(rl->rl_remid);
		break;
	default:
		log_error(ls, "dlm_recover_process_copy unknown error %d %x",
			  error, lkb->lkb_id);
	}

	dlm_recovered_lock(r);
 out:
	unlock_rsb(r);
	put_rsb(r);
	dlm_put_lkb(lkb);

	return 0;
}

int dlm_user_request(struct dlm_ls *ls, struct dlm_user_args *ua,
		     int mode, uint32_t flags, void *name, unsigned int namelen,
		     unsigned long timeout_cs)
{
	struct dlm_lkb *lkb;
	struct dlm_args args;
	int error;

	dlm_lock_recovery(ls);

	error = create_lkb(ls, &lkb);
	if (error) {
		kfree(ua);
		goto out;
	}

	if (flags & DLM_LKF_VALBLK) {
		ua->lksb.sb_lvbptr = kzalloc(DLM_USER_LVB_LEN, GFP_NOFS);
		if (!ua->lksb.sb_lvbptr) {
			kfree(ua);
			__put_lkb(ls, lkb);
			error = -ENOMEM;
			goto out;
		}
	}


	error = set_lock_args(mode, &ua->lksb, flags, namelen, timeout_cs,
			      fake_astfn, ua, fake_bastfn, &args);
	lkb->lkb_flags |= DLM_IFL_USER;

	if (error) {
		__put_lkb(ls, lkb);
		goto out;
	}

	error = request_lock(ls, lkb, name, namelen, &args);

	switch (error) {
	case 0:
		break;
	case -EINPROGRESS:
		error = 0;
		break;
	case -EAGAIN:
		error = 0;
		
	default:
		__put_lkb(ls, lkb);
		goto out;
	}

	
	spin_lock(&ua->proc->locks_spin);
	hold_lkb(lkb);
	list_add_tail(&lkb->lkb_ownqueue, &ua->proc->locks);
	spin_unlock(&ua->proc->locks_spin);
 out:
	dlm_unlock_recovery(ls);
	return error;
}

int dlm_user_convert(struct dlm_ls *ls, struct dlm_user_args *ua_tmp,
		     int mode, uint32_t flags, uint32_t lkid, char *lvb_in,
		     unsigned long timeout_cs)
{
	struct dlm_lkb *lkb;
	struct dlm_args args;
	struct dlm_user_args *ua;
	int error;

	dlm_lock_recovery(ls);

	error = find_lkb(ls, lkid, &lkb);
	if (error)
		goto out;


	ua = lkb->lkb_ua;

	if (flags & DLM_LKF_VALBLK && !ua->lksb.sb_lvbptr) {
		ua->lksb.sb_lvbptr = kzalloc(DLM_USER_LVB_LEN, GFP_NOFS);
		if (!ua->lksb.sb_lvbptr) {
			error = -ENOMEM;
			goto out_put;
		}
	}
	if (lvb_in && ua->lksb.sb_lvbptr)
		memcpy(ua->lksb.sb_lvbptr, lvb_in, DLM_USER_LVB_LEN);

	ua->xid = ua_tmp->xid;
	ua->castparam = ua_tmp->castparam;
	ua->castaddr = ua_tmp->castaddr;
	ua->bastparam = ua_tmp->bastparam;
	ua->bastaddr = ua_tmp->bastaddr;
	ua->user_lksb = ua_tmp->user_lksb;

	error = set_lock_args(mode, &ua->lksb, flags, 0, timeout_cs,
			      fake_astfn, ua, fake_bastfn, &args);
	if (error)
		goto out_put;

	error = convert_lock(ls, lkb, &args);

	if (error == -EINPROGRESS || error == -EAGAIN || error == -EDEADLK)
		error = 0;
 out_put:
	dlm_put_lkb(lkb);
 out:
	dlm_unlock_recovery(ls);
	kfree(ua_tmp);
	return error;
}

int dlm_user_unlock(struct dlm_ls *ls, struct dlm_user_args *ua_tmp,
		    uint32_t flags, uint32_t lkid, char *lvb_in)
{
	struct dlm_lkb *lkb;
	struct dlm_args args;
	struct dlm_user_args *ua;
	int error;

	dlm_lock_recovery(ls);

	error = find_lkb(ls, lkid, &lkb);
	if (error)
		goto out;

	ua = lkb->lkb_ua;

	if (lvb_in && ua->lksb.sb_lvbptr)
		memcpy(ua->lksb.sb_lvbptr, lvb_in, DLM_USER_LVB_LEN);
	if (ua_tmp->castparam)
		ua->castparam = ua_tmp->castparam;
	ua->user_lksb = ua_tmp->user_lksb;

	error = set_unlock_args(flags, ua, &args);
	if (error)
		goto out_put;

	error = unlock_lock(ls, lkb, &args);

	if (error == -DLM_EUNLOCK)
		error = 0;
	
	if (error == -EBUSY && (flags & DLM_LKF_FORCEUNLOCK))
		error = 0;
	if (error)
		goto out_put;

	spin_lock(&ua->proc->locks_spin);
	
	if (!list_empty(&lkb->lkb_ownqueue))
		list_move(&lkb->lkb_ownqueue, &ua->proc->unlocking);
	spin_unlock(&ua->proc->locks_spin);
 out_put:
	dlm_put_lkb(lkb);
 out:
	dlm_unlock_recovery(ls);
	kfree(ua_tmp);
	return error;
}

int dlm_user_cancel(struct dlm_ls *ls, struct dlm_user_args *ua_tmp,
		    uint32_t flags, uint32_t lkid)
{
	struct dlm_lkb *lkb;
	struct dlm_args args;
	struct dlm_user_args *ua;
	int error;

	dlm_lock_recovery(ls);

	error = find_lkb(ls, lkid, &lkb);
	if (error)
		goto out;

	ua = lkb->lkb_ua;
	if (ua_tmp->castparam)
		ua->castparam = ua_tmp->castparam;
	ua->user_lksb = ua_tmp->user_lksb;

	error = set_unlock_args(flags, ua, &args);
	if (error)
		goto out_put;

	error = cancel_lock(ls, lkb, &args);

	if (error == -DLM_ECANCEL)
		error = 0;
	
	if (error == -EBUSY)
		error = 0;
 out_put:
	dlm_put_lkb(lkb);
 out:
	dlm_unlock_recovery(ls);
	kfree(ua_tmp);
	return error;
}

int dlm_user_deadlock(struct dlm_ls *ls, uint32_t flags, uint32_t lkid)
{
	struct dlm_lkb *lkb;
	struct dlm_args args;
	struct dlm_user_args *ua;
	struct dlm_rsb *r;
	int error;

	dlm_lock_recovery(ls);

	error = find_lkb(ls, lkid, &lkb);
	if (error)
		goto out;

	ua = lkb->lkb_ua;

	error = set_unlock_args(flags, ua, &args);
	if (error)
		goto out_put;

	

	r = lkb->lkb_resource;
	hold_rsb(r);
	lock_rsb(r);

	error = validate_unlock_args(lkb, &args);
	if (error)
		goto out_r;
	lkb->lkb_flags |= DLM_IFL_DEADLOCK_CANCEL;

	error = _cancel_lock(r, lkb);
 out_r:
	unlock_rsb(r);
	put_rsb(r);

	if (error == -DLM_ECANCEL)
		error = 0;
	
	if (error == -EBUSY)
		error = 0;
 out_put:
	dlm_put_lkb(lkb);
 out:
	dlm_unlock_recovery(ls);
	return error;
}


static int orphan_proc_lock(struct dlm_ls *ls, struct dlm_lkb *lkb)
{
	struct dlm_args args;
	int error;

	hold_lkb(lkb);
	mutex_lock(&ls->ls_orphans_mutex);
	list_add_tail(&lkb->lkb_ownqueue, &ls->ls_orphans);
	mutex_unlock(&ls->ls_orphans_mutex);

	set_unlock_args(0, lkb->lkb_ua, &args);

	error = cancel_lock(ls, lkb, &args);
	if (error == -DLM_ECANCEL)
		error = 0;
	return error;
}


static int unlock_proc_lock(struct dlm_ls *ls, struct dlm_lkb *lkb)
{
	struct dlm_args args;
	int error;

	set_unlock_args(DLM_LKF_FORCEUNLOCK, lkb->lkb_ua, &args);

	error = unlock_lock(ls, lkb, &args);
	if (error == -DLM_EUNLOCK)
		error = 0;
	return error;
}


static struct dlm_lkb *del_proc_lock(struct dlm_ls *ls,
				     struct dlm_user_proc *proc)
{
	struct dlm_lkb *lkb = NULL;

	mutex_lock(&ls->ls_clear_proc_locks);
	if (list_empty(&proc->locks))
		goto out;

	lkb = list_entry(proc->locks.next, struct dlm_lkb, lkb_ownqueue);
	list_del_init(&lkb->lkb_ownqueue);

	if (lkb->lkb_exflags & DLM_LKF_PERSISTENT)
		lkb->lkb_flags |= DLM_IFL_ORPHAN;
	else
		lkb->lkb_flags |= DLM_IFL_DEAD;
 out:
	mutex_unlock(&ls->ls_clear_proc_locks);
	return lkb;
}



void dlm_clear_proc_locks(struct dlm_ls *ls, struct dlm_user_proc *proc)
{
	struct dlm_lkb *lkb, *safe;

	dlm_lock_recovery(ls);

	while (1) {
		lkb = del_proc_lock(ls, proc);
		if (!lkb)
			break;
		del_timeout(lkb);
		if (lkb->lkb_exflags & DLM_LKF_PERSISTENT)
			orphan_proc_lock(ls, lkb);
		else
			unlock_proc_lock(ls, lkb);


		dlm_put_lkb(lkb);
	}

	mutex_lock(&ls->ls_clear_proc_locks);

	
	list_for_each_entry_safe(lkb, safe, &proc->unlocking, lkb_ownqueue) {
		list_del_init(&lkb->lkb_ownqueue);
		lkb->lkb_flags |= DLM_IFL_DEAD;
		dlm_put_lkb(lkb);
	}

	list_for_each_entry_safe(lkb, safe, &proc->asts, lkb_cb_list) {
		memset(&lkb->lkb_callbacks, 0,
		       sizeof(struct dlm_callback) * DLM_CALLBACKS_SIZE);
		list_del_init(&lkb->lkb_cb_list);
		dlm_put_lkb(lkb);
	}

	mutex_unlock(&ls->ls_clear_proc_locks);
	dlm_unlock_recovery(ls);
}

static void purge_proc_locks(struct dlm_ls *ls, struct dlm_user_proc *proc)
{
	struct dlm_lkb *lkb, *safe;

	while (1) {
		lkb = NULL;
		spin_lock(&proc->locks_spin);
		if (!list_empty(&proc->locks)) {
			lkb = list_entry(proc->locks.next, struct dlm_lkb,
					 lkb_ownqueue);
			list_del_init(&lkb->lkb_ownqueue);
		}
		spin_unlock(&proc->locks_spin);

		if (!lkb)
			break;

		lkb->lkb_flags |= DLM_IFL_DEAD;
		unlock_proc_lock(ls, lkb);
		dlm_put_lkb(lkb); 
	}

	spin_lock(&proc->locks_spin);
	list_for_each_entry_safe(lkb, safe, &proc->unlocking, lkb_ownqueue) {
		list_del_init(&lkb->lkb_ownqueue);
		lkb->lkb_flags |= DLM_IFL_DEAD;
		dlm_put_lkb(lkb);
	}
	spin_unlock(&proc->locks_spin);

	spin_lock(&proc->asts_spin);
	list_for_each_entry_safe(lkb, safe, &proc->asts, lkb_cb_list) {
		memset(&lkb->lkb_callbacks, 0,
		       sizeof(struct dlm_callback) * DLM_CALLBACKS_SIZE);
		list_del_init(&lkb->lkb_cb_list);
		dlm_put_lkb(lkb);
	}
	spin_unlock(&proc->asts_spin);
}


static void do_purge(struct dlm_ls *ls, int nodeid, int pid)
{
	struct dlm_lkb *lkb, *safe;

	mutex_lock(&ls->ls_orphans_mutex);
	list_for_each_entry_safe(lkb, safe, &ls->ls_orphans, lkb_ownqueue) {
		if (pid && lkb->lkb_ownpid != pid)
			continue;
		unlock_proc_lock(ls, lkb);
		list_del_init(&lkb->lkb_ownqueue);
		dlm_put_lkb(lkb);
	}
	mutex_unlock(&ls->ls_orphans_mutex);
}

static int send_purge(struct dlm_ls *ls, int nodeid, int pid)
{
	struct dlm_message *ms;
	struct dlm_mhandle *mh;
	int error;

	error = _create_message(ls, sizeof(struct dlm_message), nodeid,
				DLM_MSG_PURGE, &ms, &mh);
	if (error)
		return error;
	ms->m_nodeid = nodeid;
	ms->m_pid = pid;

	return send_message(mh, ms);
}

int dlm_user_purge(struct dlm_ls *ls, struct dlm_user_proc *proc,
		   int nodeid, int pid)
{
	int error = 0;

	if (nodeid != dlm_our_nodeid()) {
		error = send_purge(ls, nodeid, pid);
	} else {
		dlm_lock_recovery(ls);
		if (pid == current->pid)
			purge_proc_locks(ls, proc);
		else
			do_purge(ls, nodeid, pid);
		dlm_unlock_recovery(ls);
	}
	return error;
}

