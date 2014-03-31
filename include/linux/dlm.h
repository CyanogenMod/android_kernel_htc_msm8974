/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004-2011 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#ifndef __DLM_DOT_H__
#define __DLM_DOT_H__


#include <linux/dlmconstants.h>
#include <linux/types.h>

typedef void dlm_lockspace_t;


#define DLM_SBF_DEMOTED		0x01
#define DLM_SBF_VALNOTVALID	0x02
#define DLM_SBF_ALTMODE		0x04

struct dlm_lksb {
	int 	 sb_status;
	__u32	 sb_lkid;
	char 	 sb_flags;
	char *	 sb_lvbptr;
};


#define DLM_LSFL_NODIR		0x00000001
#define DLM_LSFL_TIMEWARN	0x00000002
#define DLM_LSFL_FS     	0x00000004
#define DLM_LSFL_NEWEXCL     	0x00000008

#ifdef __KERNEL__

struct dlm_slot {
	int nodeid; 
	int slot;   
};


struct dlm_lockspace_ops {
	void (*recover_prep) (void *ops_arg);
	void (*recover_slot) (void *ops_arg, struct dlm_slot *slot);
	void (*recover_done) (void *ops_arg, struct dlm_slot *slots,
			      int num_slots, int our_slot, uint32_t generation);
};


int dlm_new_lockspace(const char *name, const char *cluster,
		      uint32_t flags, int lvblen,
		      const struct dlm_lockspace_ops *ops, void *ops_arg,
		      int *ops_result, dlm_lockspace_t **lockspace);


int dlm_release_lockspace(dlm_lockspace_t *lockspace, int force);


int dlm_lock(dlm_lockspace_t *lockspace,
	     int mode,
	     struct dlm_lksb *lksb,
	     uint32_t flags,
	     void *name,
	     unsigned int namelen,
	     uint32_t parent_lkid,
	     void (*lockast) (void *astarg),
	     void *astarg,
	     void (*bast) (void *astarg, int mode));


int dlm_unlock(dlm_lockspace_t *lockspace,
	       uint32_t lkid,
	       uint32_t flags,
	       struct dlm_lksb *lksb,
	       void *astarg);

#endif				

#endif				

