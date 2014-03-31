/*
 * linux/include/linux/sunrpc/svc.h
 *
 * RPC server declarations.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */


#ifndef SUNRPC_SVC_H
#define SUNRPC_SVC_H

#include <linux/in.h>
#include <linux/in6.h>
#include <linux/sunrpc/types.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/auth.h>
#include <linux/sunrpc/svcauth.h>
#include <linux/wait.h>
#include <linux/mm.h>

typedef int		(*svc_thread_fn)(void *);

struct svc_pool_stats {
	unsigned long	packets;
	unsigned long	sockets_queued;
	unsigned long	threads_woken;
	unsigned long	threads_timedout;
};

struct svc_pool {
	unsigned int		sp_id;	    	
	spinlock_t		sp_lock;	
	struct list_head	sp_threads;	
	struct list_head	sp_sockets;	
	unsigned int		sp_nrthreads;	
	struct list_head	sp_all_threads;	
	struct svc_pool_stats	sp_stats;	
} ____cacheline_aligned_in_smp;

struct svc_serv {
	struct svc_program *	sv_program;	
	struct svc_stat *	sv_stats;	
	spinlock_t		sv_lock;
	unsigned int		sv_nrthreads;	
	unsigned int		sv_maxconn;	

	unsigned int		sv_max_payload;	
	unsigned int		sv_max_mesg;	
	unsigned int		sv_xdrsize;	
	struct list_head	sv_permsocks;	
	struct list_head	sv_tempsocks;	
	int			sv_tmpcnt;	
	struct timer_list	sv_temptimer;	

	char *			sv_name;	

	unsigned int		sv_nrpools;	
	struct svc_pool *	sv_pools;	

	void			(*sv_shutdown)(struct svc_serv *serv,
					       struct net *net);

	struct module *		sv_module;	
	svc_thread_fn		sv_function;	
#if defined(CONFIG_SUNRPC_BACKCHANNEL)
	struct list_head	sv_cb_list;	
	spinlock_t		sv_cb_lock;	
	wait_queue_head_t	sv_cb_waitq;	
	struct svc_xprt		*sv_bc_xprt;	
#endif 
};

static inline void svc_get(struct svc_serv *serv)
{
	serv->sv_nrthreads++;
}

#define RPCSVC_MAXPAYLOAD	(1*1024*1024u)
#define RPCSVC_MAXPAYLOAD_TCP	RPCSVC_MAXPAYLOAD
#define RPCSVC_MAXPAYLOAD_UDP	(32*1024u)

extern u32 svc_max_payload(const struct svc_rqst *rqstp);

#define RPCSVC_MAXPAGES		((RPCSVC_MAXPAYLOAD+PAGE_SIZE-1)/PAGE_SIZE \
				+ 2 + 1)

static inline u32 svc_getnl(struct kvec *iov)
{
	__be32 val, *vp;
	vp = iov->iov_base;
	val = *vp++;
	iov->iov_base = (void*)vp;
	iov->iov_len -= sizeof(__be32);
	return ntohl(val);
}

static inline void svc_putnl(struct kvec *iov, u32 val)
{
	__be32 *vp = iov->iov_base + iov->iov_len;
	*vp = htonl(val);
	iov->iov_len += sizeof(__be32);
}

static inline __be32 svc_getu32(struct kvec *iov)
{
	__be32 val, *vp;
	vp = iov->iov_base;
	val = *vp++;
	iov->iov_base = (void*)vp;
	iov->iov_len -= sizeof(__be32);
	return val;
}

static inline void svc_ungetu32(struct kvec *iov)
{
	__be32 *vp = (__be32 *)iov->iov_base;
	iov->iov_base = (void *)(vp - 1);
	iov->iov_len += sizeof(*vp);
}

static inline void svc_putu32(struct kvec *iov, __be32 val)
{
	__be32 *vp = iov->iov_base + iov->iov_len;
	*vp = val;
	iov->iov_len += sizeof(__be32);
}

struct svc_rqst {
	struct list_head	rq_list;	
	struct list_head	rq_all;		
	struct svc_xprt *	rq_xprt;	

	struct sockaddr_storage	rq_addr;	
	size_t			rq_addrlen;
	struct sockaddr_storage	rq_daddr;	
	size_t			rq_daddrlen;

	struct svc_serv *	rq_server;	
	struct svc_pool *	rq_pool;	
	struct svc_procedure *	rq_procinfo;	
	struct auth_ops *	rq_authop;	
	u32			rq_flavor;	
	struct svc_cred		rq_cred;	
	void *			rq_xprt_ctxt;	
	struct svc_deferred_req*rq_deferred;	
	int			rq_usedeferral;	

	size_t			rq_xprt_hlen;	
	struct xdr_buf		rq_arg;
	struct xdr_buf		rq_res;
	struct page *		rq_pages[RPCSVC_MAXPAGES];
	struct page *		*rq_respages;	
	int			rq_resused;	

	struct kvec		rq_vec[RPCSVC_MAXPAGES]; 

	__be32			rq_xid;		
	u32			rq_prog;	
	u32			rq_vers;	
	u32			rq_proc;	
	u32			rq_prot;	
	unsigned short
				rq_secure  : 1;	

	void *			rq_argp;	
	void *			rq_resp;	
	void *			rq_auth_data;	

	int			rq_reserved;	

	struct cache_req	rq_chandle;	
	bool			rq_dropme;
	
	struct auth_domain *	rq_client;	
	struct auth_domain *	rq_gssclient;	
	int			rq_cachetype;
	struct svc_cacherep *	rq_cacherep;	
	int			rq_splice_ok;   
	wait_queue_head_t	rq_wait;	
	struct task_struct	*rq_task;	
};

static inline struct sockaddr_in *svc_addr_in(const struct svc_rqst *rqst)
{
	return (struct sockaddr_in *) &rqst->rq_addr;
}

static inline struct sockaddr_in6 *svc_addr_in6(const struct svc_rqst *rqst)
{
	return (struct sockaddr_in6 *) &rqst->rq_addr;
}

static inline struct sockaddr *svc_addr(const struct svc_rqst *rqst)
{
	return (struct sockaddr *) &rqst->rq_addr;
}

static inline struct sockaddr_in *svc_daddr_in(const struct svc_rqst *rqst)
{
	return (struct sockaddr_in *) &rqst->rq_daddr;
}

static inline struct sockaddr_in6 *svc_daddr_in6(const struct svc_rqst *rqst)
{
	return (struct sockaddr_in6 *) &rqst->rq_daddr;
}

static inline struct sockaddr *svc_daddr(const struct svc_rqst *rqst)
{
	return (struct sockaddr *) &rqst->rq_daddr;
}

static inline int
xdr_argsize_check(struct svc_rqst *rqstp, __be32 *p)
{
	char *cp = (char *)p;
	struct kvec *vec = &rqstp->rq_arg.head[0];
	return cp >= (char*)vec->iov_base
		&& cp <= (char*)vec->iov_base + vec->iov_len;
}

static inline int
xdr_ressize_check(struct svc_rqst *rqstp, __be32 *p)
{
	struct kvec *vec = &rqstp->rq_res.head[0];
	char *cp = (char*)p;

	vec->iov_len = cp - (char*)vec->iov_base;

	return vec->iov_len <= PAGE_SIZE;
}

static inline void svc_free_res_pages(struct svc_rqst *rqstp)
{
	while (rqstp->rq_resused) {
		struct page **pp = (rqstp->rq_respages +
				    --rqstp->rq_resused);
		if (*pp) {
			put_page(*pp);
			*pp = NULL;
		}
	}
}

struct svc_deferred_req {
	u32			prot;	
	struct svc_xprt		*xprt;
	struct sockaddr_storage	addr;	
	size_t			addrlen;
	struct sockaddr_storage	daddr;	
	size_t			daddrlen;
	struct cache_deferred_req handle;
	size_t			xprt_hlen;
	int			argslen;
	__be32			args[0];
};

struct svc_program {
	struct svc_program *	pg_next;	
	u32			pg_prog;	
	unsigned int		pg_lovers;	
	unsigned int		pg_hivers;	
	unsigned int		pg_nvers;	
	struct svc_version **	pg_vers;	
	char *			pg_name;	
	char *			pg_class;	
	struct svc_stat *	pg_stats;	
	int			(*pg_authenticate)(struct svc_rqst *);
};

struct svc_version {
	u32			vs_vers;	
	u32			vs_nproc;	
	struct svc_procedure *	vs_proc;	
	u32			vs_xdrsize;	

	unsigned int		vs_hidden : 1;	

	int			(*vs_dispatch)(struct svc_rqst *, __be32 *);
};

typedef __be32	(*svc_procfunc)(struct svc_rqst *, void *argp, void *resp);
struct svc_procedure {
	svc_procfunc		pc_func;	
	kxdrproc_t		pc_decode;	
	kxdrproc_t		pc_encode;	
	kxdrproc_t		pc_release;	
	unsigned int		pc_argsize;	
	unsigned int		pc_ressize;	
	unsigned int		pc_count;	
	unsigned int		pc_cachetype;	
	unsigned int		pc_xdrressize;	
};

int svc_rpcb_setup(struct svc_serv *serv, struct net *net);
void svc_rpcb_cleanup(struct svc_serv *serv, struct net *net);
struct svc_serv *svc_create(struct svc_program *, unsigned int,
			    void (*shutdown)(struct svc_serv *, struct net *net));
struct svc_rqst *svc_prepare_thread(struct svc_serv *serv,
					struct svc_pool *pool, int node);
void		   svc_exit_thread(struct svc_rqst *);
struct svc_serv *  svc_create_pooled(struct svc_program *, unsigned int,
			void (*shutdown)(struct svc_serv *, struct net *net),
			svc_thread_fn, struct module *);
int		   svc_set_num_threads(struct svc_serv *, struct svc_pool *, int);
int		   svc_pool_stats_open(struct svc_serv *serv, struct file *file);
void		   svc_destroy(struct svc_serv *);
void		   svc_shutdown_net(struct svc_serv *, struct net *);
int		   svc_process(struct svc_rqst *);
int		   bc_svc_process(struct svc_serv *, struct rpc_rqst *,
			struct svc_rqst *);
int		   svc_register(const struct svc_serv *, struct net *, const int,
				const unsigned short, const unsigned short);

void		   svc_wake_up(struct svc_serv *);
void		   svc_reserve(struct svc_rqst *rqstp, int space);
struct svc_pool *  svc_pool_for_cpu(struct svc_serv *serv, int cpu);
char *		   svc_print_addr(struct svc_rqst *, char *, size_t);

#define	RPC_MAX_ADDRBUFLEN	(63U)

static inline void svc_reserve_auth(struct svc_rqst *rqstp, int space)
{
	int added_space = 0;

	if (rqstp->rq_authop->flavour)
		added_space = RPC_MAX_AUTH_SIZE;
	svc_reserve(rqstp, space + added_space);
}

#endif 
