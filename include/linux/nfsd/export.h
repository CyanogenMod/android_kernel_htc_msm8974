/*
 * include/linux/nfsd/export.h
 * 
 * Public declarations for NFS exports. The definitions for the
 * syscall interface are in nfsctl.h
 *
 * Copyright (C) 1995-1997 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef NFSD_EXPORT_H
#define NFSD_EXPORT_H

# include <linux/types.h>
#ifdef __KERNEL__
# include <linux/nfsd/nfsfh.h>
#endif

#define NFSCLNT_IDMAX		1024
#define NFSCLNT_ADDRMAX		16
#define NFSCLNT_KEYMAX		32

#define NFSEXP_READONLY		0x0001
#define NFSEXP_INSECURE_PORT	0x0002
#define NFSEXP_ROOTSQUASH	0x0004
#define NFSEXP_ALLSQUASH	0x0008
#define NFSEXP_ASYNC		0x0010
#define NFSEXP_GATHERED_WRITES	0x0020
#define NFSEXP_NOHIDE		0x0200
#define NFSEXP_NOSUBTREECHECK	0x0400
#define	NFSEXP_NOAUTHNLM	0x0800		
#define NFSEXP_MSNFS		0x1000	
#define NFSEXP_FSID		0x2000
#define	NFSEXP_CROSSMOUNT	0x4000
#define	NFSEXP_NOACL		0x8000	
#define	NFSEXP_V4ROOT		0x10000
#define NFSEXP_ALLFLAGS		0x17E3F

#define NFSEXP_SECINFO_FLAGS	(NFSEXP_READONLY | NFSEXP_ROOTSQUASH \
					| NFSEXP_ALLSQUASH \
					| NFSEXP_INSECURE_PORT)

#ifdef __KERNEL__


#define MAX_FS_LOCATIONS	128

struct nfsd4_fs_location {
	char *hosts; 
	char *path;  
};

struct nfsd4_fs_locations {
	uint32_t locations_count;
	struct nfsd4_fs_location *locations;
	int migrated;
};

#define MAX_SECINFO_LIST	8

struct exp_flavor_info {
	u32	pseudoflavor;
	u32	flags;
};

struct svc_export {
	struct cache_head	h;
	struct auth_domain *	ex_client;
	int			ex_flags;
	struct path		ex_path;
	uid_t			ex_anon_uid;
	gid_t			ex_anon_gid;
	int			ex_fsid;
	unsigned char *		ex_uuid; 
	struct nfsd4_fs_locations ex_fslocs;
	int			ex_nflavors;
	struct exp_flavor_info	ex_flavors[MAX_SECINFO_LIST];
};

struct svc_expkey {
	struct cache_head	h;

	struct auth_domain *	ek_client;
	int			ek_fsidtype;
	u32			ek_fsid[6];

	struct path		ek_path;
};

#define EX_ISSYNC(exp)		(!((exp)->ex_flags & NFSEXP_ASYNC))
#define EX_NOHIDE(exp)		((exp)->ex_flags & NFSEXP_NOHIDE)
#define EX_WGATHER(exp)		((exp)->ex_flags & NFSEXP_GATHERED_WRITES)

int nfsexp_flags(struct svc_rqst *rqstp, struct svc_export *exp);
__be32 check_nfsd_access(struct svc_export *exp, struct svc_rqst *rqstp);

int			nfsd_export_init(void);
void			nfsd_export_shutdown(void);
void			nfsd_export_flush(void);
struct svc_export *	rqst_exp_get_by_name(struct svc_rqst *,
					     struct path *);
struct svc_export *	rqst_exp_parent(struct svc_rqst *,
					struct path *);
struct svc_export *	rqst_find_fsidzero_export(struct svc_rqst *);
int			exp_rootfh(struct auth_domain *, 
					char *path, struct knfsd_fh *, int maxsize);
__be32			exp_pseudoroot(struct svc_rqst *, struct svc_fh *);
__be32			nfserrno(int errno);

extern struct cache_detail svc_export_cache;

static inline void exp_put(struct svc_export *exp)
{
	cache_put(&exp->h, &svc_export_cache);
}

static inline void exp_get(struct svc_export *exp)
{
	cache_get(&exp->h);
}
struct svc_export * rqst_exp_find(struct svc_rqst *, int, u32 *);

#endif 

#endif 

