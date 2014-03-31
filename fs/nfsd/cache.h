/*
 * Request reply cache. This was heavily inspired by the
 * implementation in 4.3BSD/4.4BSD.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef NFSCACHE_H
#define NFSCACHE_H

#include <linux/sunrpc/svc.h>

struct svc_cacherep {
	struct hlist_node	c_hash;
	struct list_head	c_lru;

	unsigned char		c_state,	
				c_type,		
				c_secure : 1;	
	struct sockaddr_in	c_addr;
	__be32			c_xid;
	u32			c_prot;
	u32			c_proc;
	u32			c_vers;
	unsigned long		c_timestamp;
	union {
		struct kvec	u_vec;
		__be32		u_status;
	}			c_u;
};

#define c_replvec		c_u.u_vec
#define c_replstat		c_u.u_status

enum {
	RC_UNUSED,
	RC_INPROG,
	RC_DONE
};

enum {
	RC_DROPIT,
	RC_REPLY,
	RC_DOIT,
	RC_INTR
};

enum {
	RC_NOCACHE,
	RC_REPLSTAT,
	RC_REPLBUFF,
};

#define RC_DELAY		(HZ/5)

int	nfsd_reply_cache_init(void);
void	nfsd_reply_cache_shutdown(void);
int	nfsd_cache_lookup(struct svc_rqst *);
void	nfsd_cache_update(struct svc_rqst *, int, __be32 *);

#ifdef CONFIG_NFSD_V4
void	nfsd4_set_statp(struct svc_rqst *rqstp, __be32 *statp);
#else  
static inline void nfsd4_set_statp(struct svc_rqst *rqstp, __be32 *statp)
{
}
#endif 

#endif 
