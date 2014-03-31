/*
 * linux/include/linux/nfsd/stats.h
 *
 * Statistics for NFS server.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef LINUX_NFSD_STATS_H
#define LINUX_NFSD_STATS_H

#include <linux/nfs4.h>

#define	NFSD_USAGE_WRAP	(HZ*1000000)

#ifdef __KERNEL__

struct nfsd_stats {
	unsigned int	rchits;		
	unsigned int	rcmisses;	
	unsigned int	rcnocache;	
	unsigned int	fh_stale;	
	unsigned int	fh_lookup;	
	unsigned int	fh_anon;	
	unsigned int	fh_nocache_dir;	
	unsigned int	fh_nocache_nondir;	
	unsigned int	io_read;	
	unsigned int	io_write;	
	unsigned int	th_cnt;		
	unsigned int	th_usage[10];	
	unsigned int	th_fullcnt;	
	unsigned int	ra_size;	
	unsigned int	ra_depth[11];	
#ifdef CONFIG_NFSD_V4
	unsigned int	nfs4_opcount[LAST_NFS4_OP + 1];	
#endif

};


extern struct nfsd_stats	nfsdstats;
extern struct svc_stat		nfsd_svcstats;

void	nfsd_stat_init(void);
void	nfsd_stat_shutdown(void);

#endif 
#endif 
