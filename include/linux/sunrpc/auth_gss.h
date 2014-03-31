/*
 * linux/include/linux/sunrpc/auth_gss.h
 *
 * Declarations for RPCSEC_GSS
 *
 * Dug Song <dugsong@monkey.org>
 * Andy Adamson <andros@umich.edu>
 * Bruce Fields <bfields@umich.edu>
 * Copyright (c) 2000 The Regents of the University of Michigan
 */

#ifndef _LINUX_SUNRPC_AUTH_GSS_H
#define _LINUX_SUNRPC_AUTH_GSS_H

#ifdef __KERNEL__
#include <linux/sunrpc/auth.h>
#include <linux/sunrpc/svc.h>
#include <linux/sunrpc/gss_api.h>

#define RPC_GSS_VERSION		1

#define MAXSEQ 0x80000000 

enum rpc_gss_proc {
	RPC_GSS_PROC_DATA = 0,
	RPC_GSS_PROC_INIT = 1,
	RPC_GSS_PROC_CONTINUE_INIT = 2,
	RPC_GSS_PROC_DESTROY = 3
};

enum rpc_gss_svc {
	RPC_GSS_SVC_NONE = 1,
	RPC_GSS_SVC_INTEGRITY = 2,
	RPC_GSS_SVC_PRIVACY = 3
};

struct rpc_gss_wire_cred {
	u32			gc_v;		
	u32			gc_proc;	
	u32			gc_seq;		
	u32			gc_svc;		
	struct xdr_netobj	gc_ctx;		
};

struct rpc_gss_wire_verf {
	u32			gv_flavor;
	struct xdr_netobj	gv_verf;
};

struct rpc_gss_init_res {
	struct xdr_netobj	gr_ctx;		
	u32			gr_major;	
	u32			gr_minor;	
	u32			gr_win;		
	struct xdr_netobj	gr_token;	
};


struct gss_cl_ctx {
	atomic_t		count;
	enum rpc_gss_proc	gc_proc;
	u32			gc_seq;
	spinlock_t		gc_seq_lock;
	struct gss_ctx __rcu	*gc_gss_ctx;
	struct xdr_netobj	gc_wire_ctx;
	u32			gc_win;
	unsigned long		gc_expiry;
	struct rcu_head		gc_rcu;
};

struct gss_upcall_msg;
struct gss_cred {
	struct rpc_cred		gc_base;
	enum rpc_gss_svc	gc_service;
	struct gss_cl_ctx __rcu	*gc_ctx;
	struct gss_upcall_msg	*gc_upcall;
	const char		*gc_principal;
	unsigned long		gc_upcall_timestamp;
};

#endif 
#endif 

