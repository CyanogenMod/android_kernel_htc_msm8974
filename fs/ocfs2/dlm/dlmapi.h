/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * dlmapi.h
 *
 * externally exported dlm interfaces
 *
 * Copyright (C) 2004 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 *
 */

#ifndef DLMAPI_H
#define DLMAPI_H

struct dlm_lock;
struct dlm_ctxt;

enum dlm_status {
	DLM_NORMAL = 0,           
	DLM_GRANTED,              
	DLM_DENIED,               
	DLM_DENIED_NOLOCKS,       
	DLM_WORKING,              
	DLM_BLOCKED,              
	DLM_BLOCKED_ORPHAN,       
	DLM_DENIED_GRACE_PERIOD,  
	DLM_SYSERR,               
	DLM_NOSUPPORT,            
	DLM_CANCELGRANT,          
	DLM_IVLOCKID,             
	DLM_SYNC,                 
	DLM_BADTYPE,              
	DLM_BADRESOURCE,          
	DLM_MAXHANDLES,           
	DLM_NOCLINFO,             
	DLM_NOLOCKMGR,            
	DLM_NOPURGED,             
	DLM_BADARGS,              
	DLM_VOID,                 
	DLM_NOTQUEUED,            
	DLM_IVBUFLEN,             
	DLM_CVTUNGRANT,           
	DLM_BADPARAM,             
	DLM_VALNOTVALID,          
	DLM_REJECTED,             
	DLM_ABORT,                
	DLM_CANCEL,               
	DLM_IVRESHANDLE,          
	DLM_DEADLOCK,             
	DLM_DENIED_NOASTS,        
	DLM_FORWARD,              
	DLM_TIMEOUT,              
	DLM_IVGROUPID,            
	DLM_VERS_CONFLICT,        
	DLM_BAD_DEVICE_PATH,      
	DLM_NO_DEVICE_PERMISSION, 
	DLM_NO_CONTROL_DEVICE,    

	DLM_RECOVERING,           
	DLM_MIGRATING,            
	DLM_MAXSTATS,             
};

const char *dlm_errmsg(enum dlm_status err);
const char *dlm_errname(enum dlm_status err);

#define dlm_error(st) do {						\
	if ((st) != DLM_RECOVERING &&					\
	    (st) != DLM_MIGRATING &&					\
	    (st) != DLM_FORWARD)					\
		mlog(ML_ERROR, "dlm status = %s\n", dlm_errname((st)));	\
} while (0)

#define DLM_LKSB_UNUSED1           0x01
#define DLM_LKSB_PUT_LVB           0x02
#define DLM_LKSB_GET_LVB           0x04
#define DLM_LKSB_UNUSED2           0x08
#define DLM_LKSB_UNUSED3           0x10
#define DLM_LKSB_UNUSED4           0x20
#define DLM_LKSB_UNUSED5           0x40
#define DLM_LKSB_UNUSED6           0x80

#define DLM_LVB_LEN  64

struct dlm_lockstatus {
	enum dlm_status status;
	u32 flags;
	struct dlm_lock *lockid;
	char lvb[DLM_LVB_LEN];
};

#define LKM_IVMODE      (-1)            
#define LKM_NLMODE      0               
#define LKM_CRMODE      1               
#define LKM_CWMODE      2               
#define LKM_PRMODE      3               
#define LKM_PWMODE      4               
#define LKM_EXMODE      5               
#define LKM_MAXMODE     5
#define LKM_MODEMASK    0xff

#define LKM_ORPHAN       0x00000010  
#define LKM_PARENTABLE   0x00000020  
#define LKM_BLOCK        0x00000040  
#define LKM_LOCAL        0x00000080  
#define LKM_VALBLK       0x00000100  
#define LKM_NOQUEUE      0x00000200  
#define LKM_CONVERT      0x00000400  
#define LKM_NODLCKWT     0x00000800  
#define LKM_UNLOCK       0x00001000  
#define LKM_CANCEL       0x00002000  
#define LKM_DEQALL       0x00004000  
#define LKM_INVVALBLK    0x00008000  
#define LKM_SYNCSTS      0x00010000  
#define LKM_TIMEOUT      0x00020000  
#define LKM_SNGLDLCK     0x00040000  
#define LKM_FINDLOCAL    0x00080000  
#define LKM_PROC_OWNED   0x00100000  
#define LKM_XID          0x00200000  
#define LKM_XID_CONFLICT 0x00400000  
#define LKM_FORCE        0x00800000  
#define LKM_REVVALBLK    0x01000000  
#define LKM_UNUSED1      0x00000001  
#define LKM_UNUSED2      0x00000002  
#define LKM_UNUSED3      0x00000004  
#define LKM_UNUSED4      0x00000008  
#define LKM_UNUSED5      0x02000000  
#define LKM_UNUSED6      0x04000000  
#define LKM_UNUSED7      0x08000000  

#define LKM_MIGRATION    0x10000000  
#define LKM_PUT_LVB      0x20000000  
#define LKM_GET_LVB      0x40000000  
#define LKM_RECOVERY     0x80000000  


typedef void (dlm_astlockfunc_t)(void *);
typedef void (dlm_bastlockfunc_t)(void *, int);
typedef void (dlm_astunlockfunc_t)(void *, enum dlm_status);

enum dlm_status dlmlock(struct dlm_ctxt *dlm,
			int mode,
			struct dlm_lockstatus *lksb,
			int flags,
			const char *name,
			int namelen,
			dlm_astlockfunc_t *ast,
			void *data,
			dlm_bastlockfunc_t *bast);

enum dlm_status dlmunlock(struct dlm_ctxt *dlm,
			  struct dlm_lockstatus *lksb,
			  int flags,
			  dlm_astunlockfunc_t *unlockast,
			  void *data);

struct dlm_protocol_version {
	u8 pv_major;
	u8 pv_minor;
};
struct dlm_ctxt * dlm_register_domain(const char *domain, u32 key,
				      struct dlm_protocol_version *fs_proto);

void dlm_unregister_domain(struct dlm_ctxt *dlm);

void dlm_print_one_lock(struct dlm_lock *lockid);

typedef void (dlm_eviction_func)(int, void *);
struct dlm_eviction_cb {
	struct list_head        ec_item;
	dlm_eviction_func       *ec_func;
	void                    *ec_data;
};
void dlm_setup_eviction_cb(struct dlm_eviction_cb *cb,
			   dlm_eviction_func *f,
			   void *data);
void dlm_register_eviction_cb(struct dlm_ctxt *dlm,
			      struct dlm_eviction_cb *cb);
void dlm_unregister_eviction_cb(struct dlm_eviction_cb *cb);

#endif 
