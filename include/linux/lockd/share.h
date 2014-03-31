/*
 * linux/include/linux/lockd/share.h
 *
 * DOS share management for lockd.
 *
 * Copyright (C) 1996, Olaf Kirch <okir@monad.swb.de>
 */

#ifndef LINUX_LOCKD_SHARE_H
#define LINUX_LOCKD_SHARE_H

struct nlm_share {
	struct nlm_share *	s_next;		
	struct nlm_host *	s_host;		
	struct nlm_file *	s_file;		
	struct xdr_netobj	s_owner;	
	u32			s_access;	
	u32			s_mode;		
};

__be32	nlmsvc_share_file(struct nlm_host *, struct nlm_file *,
					       struct nlm_args *);
__be32	nlmsvc_unshare_file(struct nlm_host *, struct nlm_file *,
					       struct nlm_args *);
void	nlmsvc_traverse_shares(struct nlm_host *, struct nlm_file *,
					       nlm_host_match_fn_t);

#endif 
