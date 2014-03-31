/*
 * Central processing for nfsd.
 *
 * Authors:	Olaf Kirch (okir@monad.swb.de)
 *
 * Copyright (C) 1995, 1996, 1997 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/module.h>
#include <linux/fs_struct.h>
#include <linux/swap.h>

#include <linux/sunrpc/stats.h>
#include <linux/sunrpc/svcsock.h>
#include <linux/lockd/bind.h>
#include <linux/nfsacl.h>
#include <linux/seq_file.h>
#include <net/net_namespace.h>
#include "nfsd.h"
#include "cache.h"
#include "vfs.h"

#define NFSDDBG_FACILITY	NFSDDBG_SVC

extern struct svc_program	nfsd_program;
static int			nfsd(void *vrqstp);
struct timeval			nfssvc_boot;

DEFINE_MUTEX(nfsd_mutex);
struct svc_serv 		*nfsd_serv;

spinlock_t	nfsd_drc_lock;
unsigned int	nfsd_drc_max_mem;
unsigned int	nfsd_drc_mem_used;

#if defined(CONFIG_NFSD_V2_ACL) || defined(CONFIG_NFSD_V3_ACL)
static struct svc_stat	nfsd_acl_svcstats;
static struct svc_version *	nfsd_acl_version[] = {
	[2] = &nfsd_acl_version2,
	[3] = &nfsd_acl_version3,
};

#define NFSD_ACL_MINVERS            2
#define NFSD_ACL_NRVERS		ARRAY_SIZE(nfsd_acl_version)
static struct svc_version *nfsd_acl_versions[NFSD_ACL_NRVERS];

static struct svc_program	nfsd_acl_program = {
	.pg_prog		= NFS_ACL_PROGRAM,
	.pg_nvers		= NFSD_ACL_NRVERS,
	.pg_vers		= nfsd_acl_versions,
	.pg_name		= "nfsacl",
	.pg_class		= "nfsd",
	.pg_stats		= &nfsd_acl_svcstats,
	.pg_authenticate	= &svc_set_client,
};

static struct svc_stat	nfsd_acl_svcstats = {
	.program	= &nfsd_acl_program,
};
#endif 

static struct svc_version *	nfsd_version[] = {
	[2] = &nfsd_version2,
#if defined(CONFIG_NFSD_V3)
	[3] = &nfsd_version3,
#endif
#if defined(CONFIG_NFSD_V4)
	[4] = &nfsd_version4,
#endif
};

#define NFSD_MINVERS    	2
#define NFSD_NRVERS		ARRAY_SIZE(nfsd_version)
static struct svc_version *nfsd_versions[NFSD_NRVERS];

struct svc_program		nfsd_program = {
#if defined(CONFIG_NFSD_V2_ACL) || defined(CONFIG_NFSD_V3_ACL)
	.pg_next		= &nfsd_acl_program,
#endif
	.pg_prog		= NFS_PROGRAM,		
	.pg_nvers		= NFSD_NRVERS,		
	.pg_vers		= nfsd_versions,	
	.pg_name		= "nfsd",		
	.pg_class		= "nfsd",		
	.pg_stats		= &nfsd_svcstats,	
	.pg_authenticate	= &svc_set_client,	

};

u32 nfsd_supported_minorversion;

int nfsd_vers(int vers, enum vers_op change)
{
	if (vers < NFSD_MINVERS || vers >= NFSD_NRVERS)
		return 0;
	switch(change) {
	case NFSD_SET:
		nfsd_versions[vers] = nfsd_version[vers];
#if defined(CONFIG_NFSD_V2_ACL) || defined(CONFIG_NFSD_V3_ACL)
		if (vers < NFSD_ACL_NRVERS)
			nfsd_acl_versions[vers] = nfsd_acl_version[vers];
#endif
		break;
	case NFSD_CLEAR:
		nfsd_versions[vers] = NULL;
#if defined(CONFIG_NFSD_V2_ACL) || defined(CONFIG_NFSD_V3_ACL)
		if (vers < NFSD_ACL_NRVERS)
			nfsd_acl_versions[vers] = NULL;
#endif
		break;
	case NFSD_TEST:
		return nfsd_versions[vers] != NULL;
	case NFSD_AVAIL:
		return nfsd_version[vers] != NULL;
	}
	return 0;
}

int nfsd_minorversion(u32 minorversion, enum vers_op change)
{
	if (minorversion > NFSD_SUPPORTED_MINOR_VERSION)
		return -1;
	switch(change) {
	case NFSD_SET:
		nfsd_supported_minorversion = minorversion;
		break;
	case NFSD_CLEAR:
		if (minorversion == 0)
			return -1;
		nfsd_supported_minorversion = minorversion - 1;
		break;
	case NFSD_TEST:
		return minorversion <= nfsd_supported_minorversion;
	case NFSD_AVAIL:
		return minorversion <= NFSD_SUPPORTED_MINOR_VERSION;
	}
	return 0;
}

#define	NFSD_MAXSERVS		8192

int nfsd_nrthreads(void)
{
	int rv = 0;
	mutex_lock(&nfsd_mutex);
	if (nfsd_serv)
		rv = nfsd_serv->sv_nrthreads;
	mutex_unlock(&nfsd_mutex);
	return rv;
}

static int nfsd_init_socks(int port)
{
	int error;
	if (!list_empty(&nfsd_serv->sv_permsocks))
		return 0;

	error = svc_create_xprt(nfsd_serv, "udp", &init_net, PF_INET, port,
					SVC_SOCK_DEFAULTS);
	if (error < 0)
		return error;

	error = svc_create_xprt(nfsd_serv, "tcp", &init_net, PF_INET, port,
					SVC_SOCK_DEFAULTS);
	if (error < 0)
		return error;

	return 0;
}

static bool nfsd_up = false;

static int nfsd_startup(unsigned short port, int nrservs)
{
	int ret;

	if (nfsd_up)
		return 0;
	ret = nfsd_racache_init(2*nrservs);
	if (ret)
		return ret;
	ret = nfsd_init_socks(port);
	if (ret)
		goto out_racache;
	ret = lockd_up();
	if (ret)
		goto out_racache;
	ret = nfs4_state_start();
	if (ret)
		goto out_lockd;
	nfsd_up = true;
	return 0;
out_lockd:
	lockd_down();
out_racache:
	nfsd_racache_shutdown();
	return ret;
}

static void nfsd_shutdown(void)
{
	if (!nfsd_up)
		return;
	nfs4_state_shutdown();
	lockd_down();
	nfsd_racache_shutdown();
	nfsd_up = false;
}

static void nfsd_last_thread(struct svc_serv *serv, struct net *net)
{
	
	nfsd_serv = NULL;
	nfsd_shutdown();

	svc_rpcb_cleanup(serv, net);

	printk(KERN_WARNING "nfsd: last server has exited, flushing export "
			    "cache\n");
	nfsd_export_flush();
}

void nfsd_reset_versions(void)
{
	int found_one = 0;
	int i;

	for (i = NFSD_MINVERS; i < NFSD_NRVERS; i++) {
		if (nfsd_program.pg_vers[i])
			found_one = 1;
	}

	if (!found_one) {
		for (i = NFSD_MINVERS; i < NFSD_NRVERS; i++)
			nfsd_program.pg_vers[i] = nfsd_version[i];
#if defined(CONFIG_NFSD_V2_ACL) || defined(CONFIG_NFSD_V3_ACL)
		for (i = NFSD_ACL_MINVERS; i < NFSD_ACL_NRVERS; i++)
			nfsd_acl_program.pg_vers[i] =
				nfsd_acl_version[i];
#endif
	}
}

static void set_max_drc(void)
{
	#define NFSD_DRC_SIZE_SHIFT	10
	nfsd_drc_max_mem = (nr_free_buffer_pages()
					>> NFSD_DRC_SIZE_SHIFT) * PAGE_SIZE;
	nfsd_drc_mem_used = 0;
	spin_lock_init(&nfsd_drc_lock);
	dprintk("%s nfsd_drc_max_mem %u \n", __func__, nfsd_drc_max_mem);
}

static int nfsd_get_default_max_blksize(void)
{
	struct sysinfo i;
	unsigned long long target;
	unsigned long ret;

	si_meminfo(&i);
	target = (i.totalram - i.totalhigh) << PAGE_SHIFT;
	target >>= 12;

	ret = NFSSVC_MAXBLKSIZE;
	while (ret > target && ret >= 8*1024*2)
		ret /= 2;
	return ret;
}

int nfsd_create_serv(void)
{
	WARN_ON(!mutex_is_locked(&nfsd_mutex));
	if (nfsd_serv) {
		svc_get(nfsd_serv);
		return 0;
	}
	if (nfsd_max_blksize == 0)
		nfsd_max_blksize = nfsd_get_default_max_blksize();
	nfsd_reset_versions();
	nfsd_serv = svc_create_pooled(&nfsd_program, nfsd_max_blksize,
				      nfsd_last_thread, nfsd, THIS_MODULE);
	if (nfsd_serv == NULL)
		return -ENOMEM;

	set_max_drc();
	do_gettimeofday(&nfssvc_boot);		
	return 0;
}

int nfsd_nrpools(void)
{
	if (nfsd_serv == NULL)
		return 0;
	else
		return nfsd_serv->sv_nrpools;
}

int nfsd_get_nrthreads(int n, int *nthreads)
{
	int i = 0;

	if (nfsd_serv != NULL) {
		for (i = 0; i < nfsd_serv->sv_nrpools && i < n; i++)
			nthreads[i] = nfsd_serv->sv_pools[i].sp_nrthreads;
	}

	return 0;
}

int nfsd_set_nrthreads(int n, int *nthreads)
{
	int i = 0;
	int tot = 0;
	int err = 0;

	WARN_ON(!mutex_is_locked(&nfsd_mutex));

	if (nfsd_serv == NULL || n <= 0)
		return 0;

	if (n > nfsd_serv->sv_nrpools)
		n = nfsd_serv->sv_nrpools;

	
	tot = 0;
	for (i = 0; i < n; i++) {
		if (nthreads[i] > NFSD_MAXSERVS)
			nthreads[i] = NFSD_MAXSERVS;
		tot += nthreads[i];
	}
	if (tot > NFSD_MAXSERVS) {
		
		for (i = 0; i < n && tot > 0; i++) {
		    	int new = nthreads[i] * NFSD_MAXSERVS / tot;
			tot -= (nthreads[i] - new);
			nthreads[i] = new;
		}
		for (i = 0; i < n && tot > 0; i++) {
			nthreads[i]--;
			tot--;
		}
	}

	if (nthreads[0] == 0)
		nthreads[0] = 1;

	
	svc_get(nfsd_serv);
	for (i = 0; i < n; i++) {
		err = svc_set_num_threads(nfsd_serv, &nfsd_serv->sv_pools[i],
				    	  nthreads[i]);
		if (err)
			break;
	}
	svc_destroy(nfsd_serv);

	return err;
}

int
nfsd_svc(unsigned short port, int nrservs)
{
	int	error;
	bool	nfsd_up_before;

	mutex_lock(&nfsd_mutex);
	dprintk("nfsd: creating service\n");
	if (nrservs <= 0)
		nrservs = 0;
	if (nrservs > NFSD_MAXSERVS)
		nrservs = NFSD_MAXSERVS;
	error = 0;
	if (nrservs == 0 && nfsd_serv == NULL)
		goto out;

	error = nfsd_create_serv();
	if (error)
		goto out;

	nfsd_up_before = nfsd_up;

	error = nfsd_startup(port, nrservs);
	if (error)
		goto out_destroy;
	error = svc_set_num_threads(nfsd_serv, NULL, nrservs);
	if (error)
		goto out_shutdown;
	error = nfsd_serv->sv_nrthreads - 1;
out_shutdown:
	if (error < 0 && !nfsd_up_before)
		nfsd_shutdown();
out_destroy:
	svc_destroy(nfsd_serv);		
out:
	mutex_unlock(&nfsd_mutex);
	return error;
}


static int
nfsd(void *vrqstp)
{
	struct svc_rqst *rqstp = (struct svc_rqst *) vrqstp;
	int err, preverr = 0;

	
	mutex_lock(&nfsd_mutex);

	if (unshare_fs_struct() < 0) {
		printk("Unable to start nfsd thread: out of memory\n");
		goto out;
	}

	current->fs->umask = 0;

	allow_signal(SIGKILL);
	allow_signal(SIGHUP);
	allow_signal(SIGINT);
	allow_signal(SIGQUIT);

	nfsdstats.th_cnt++;
	mutex_unlock(&nfsd_mutex);

	current->flags |= PF_LESS_THROTTLE;
	set_freezable();

	for (;;) {
		while ((err = svc_recv(rqstp, 60*60*HZ)) == -EAGAIN)
			;
		if (err == -EINTR)
			break;
		else if (err < 0) {
			if (err != preverr) {
				printk(KERN_WARNING "%s: unexpected error "
					"from svc_recv (%d)\n", __func__, -err);
				preverr = err;
			}
			schedule_timeout_uninterruptible(HZ);
			continue;
		}

		validate_process_creds();
		svc_process(rqstp);
		validate_process_creds();
	}

	
	flush_signals(current);

	mutex_lock(&nfsd_mutex);
	nfsdstats.th_cnt --;

out:
	
	svc_exit_thread(rqstp);

	
	mutex_unlock(&nfsd_mutex);
	module_put_and_exit(0);
	return 0;
}

static __be32 map_new_errors(u32 vers, __be32 nfserr)
{
	if (nfserr == nfserr_jukebox && vers == 2)
		return nfserr_dropit;
	if (nfserr == nfserr_wrongsec && vers < 4)
		return nfserr_acces;
	return nfserr;
}

int
nfsd_dispatch(struct svc_rqst *rqstp, __be32 *statp)
{
	struct svc_procedure	*proc;
	kxdrproc_t		xdr;
	__be32			nfserr;
	__be32			*nfserrp;

	dprintk("nfsd_dispatch: vers %d proc %d\n",
				rqstp->rq_vers, rqstp->rq_proc);
	proc = rqstp->rq_procinfo;

	rqstp->rq_cachetype = proc->pc_cachetype;
	
	xdr = proc->pc_decode;
	if (xdr && !xdr(rqstp, (__be32*)rqstp->rq_arg.head[0].iov_base,
			rqstp->rq_argp)) {
		dprintk("nfsd: failed to decode arguments!\n");
		*statp = rpc_garbage_args;
		return 1;
	}

	
	switch (nfsd_cache_lookup(rqstp)) {
	case RC_INTR:
	case RC_DROPIT:
		return 0;
	case RC_REPLY:
		return 1;
	case RC_DOIT:;
		
	}

	nfserrp = rqstp->rq_res.head[0].iov_base
		+ rqstp->rq_res.head[0].iov_len;
	rqstp->rq_res.head[0].iov_len += sizeof(__be32);

	
	nfserr = proc->pc_func(rqstp, rqstp->rq_argp, rqstp->rq_resp);
	nfserr = map_new_errors(rqstp->rq_vers, nfserr);
	if (nfserr == nfserr_dropit || rqstp->rq_dropme) {
		dprintk("nfsd: Dropping request; may be revisited later\n");
		nfsd_cache_update(rqstp, RC_NOCACHE, NULL);
		return 0;
	}

	if (rqstp->rq_proc != 0)
		*nfserrp++ = nfserr;

	if (!(nfserr && rqstp->rq_vers == 2)) {
		xdr = proc->pc_encode;
		if (xdr && !xdr(rqstp, nfserrp,
				rqstp->rq_resp)) {
			
			dprintk("nfsd: failed to encode result!\n");
			nfsd_cache_update(rqstp, RC_NOCACHE, NULL);
			*statp = rpc_system_err;
			return 1;
		}
	}

	
	nfsd_cache_update(rqstp, proc->pc_cachetype, statp + 1);
	return 1;
}

int nfsd_pool_stats_open(struct inode *inode, struct file *file)
{
	int ret;
	mutex_lock(&nfsd_mutex);
	if (nfsd_serv == NULL) {
		mutex_unlock(&nfsd_mutex);
		return -ENODEV;
	}
	
	svc_get(nfsd_serv);
	ret = svc_pool_stats_open(nfsd_serv, file);
	mutex_unlock(&nfsd_mutex);
	return ret;
}

int nfsd_pool_stats_release(struct inode *inode, struct file *file)
{
	int ret = seq_release(inode, file);
	mutex_lock(&nfsd_mutex);
	
	svc_destroy(nfsd_serv);
	mutex_unlock(&nfsd_mutex);
	return ret;
}
