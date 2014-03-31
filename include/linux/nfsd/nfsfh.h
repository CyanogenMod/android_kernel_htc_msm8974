/*
 * include/linux/nfsd/nfsfh.h
 *
 * This file describes the layout of the file handles as passed
 * over the wire.
 *
 * Earlier versions of knfsd used to sign file handles using keyed MD5
 * or SHA. I've removed this code, because it doesn't give you more
 * security than blocking external access to port 2049 on your firewall.
 *
 * Copyright (C) 1995, 1996, 1997 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef _LINUX_NFSD_FH_H
#define _LINUX_NFSD_FH_H

#include <linux/types.h>
#include <linux/nfs.h>
#include <linux/nfs2.h>
#include <linux/nfs3.h>
#include <linux/nfs4.h>
#ifdef __KERNEL__
# include <linux/sunrpc/svc.h>
#endif

struct nfs_fhbase_old {
	__u32		fb_dcookie;	
	__u32		fb_ino;		
	__u32		fb_dirino;	
	__u32		fb_dev;		
	__u32		fb_xdev;
	__u32		fb_xino;
	__u32		fb_generation;
};

struct nfs_fhbase_new {
	__u8		fb_version;	
	__u8		fb_auth_type;
	__u8		fb_fsid_type;
	__u8		fb_fileid_type;
	__u32		fb_auth[1];
};

struct knfsd_fh {
	unsigned int	fh_size;	
	union {
		struct nfs_fhbase_old	fh_old;
		__u32			fh_pad[NFS4_FHSIZE/4];
		struct nfs_fhbase_new	fh_new;
	} fh_base;
};

#define ofh_dcookie		fh_base.fh_old.fb_dcookie
#define ofh_ino			fh_base.fh_old.fb_ino
#define ofh_dirino		fh_base.fh_old.fb_dirino
#define ofh_dev			fh_base.fh_old.fb_dev
#define ofh_xdev		fh_base.fh_old.fb_xdev
#define ofh_xino		fh_base.fh_old.fb_xino
#define ofh_generation		fh_base.fh_old.fb_generation

#define	fh_version		fh_base.fh_new.fb_version
#define	fh_fsid_type		fh_base.fh_new.fb_fsid_type
#define	fh_auth_type		fh_base.fh_new.fb_auth_type
#define	fh_fileid_type		fh_base.fh_new.fb_fileid_type
#define	fh_auth			fh_base.fh_new.fb_auth
#define	fh_fsid			fh_base.fh_new.fb_auth

#ifdef __KERNEL__

static inline __u32 ino_t_to_u32(ino_t ino)
{
	return (__u32) ino;
}

static inline ino_t u32_to_ino_t(__u32 uino)
{
	return (ino_t) uino;
}

typedef struct svc_fh {
	struct knfsd_fh		fh_handle;	
	struct dentry *		fh_dentry;	
	struct svc_export *	fh_export;	
	int			fh_maxsize;	

	unsigned char		fh_locked;	

#ifdef CONFIG_NFSD_V3
	unsigned char		fh_post_saved;	
	unsigned char		fh_pre_saved;	

	
	__u64			fh_pre_size;	
	struct timespec		fh_pre_mtime;	
	struct timespec		fh_pre_ctime;	
	u64			fh_pre_change;

	
	struct kstat		fh_post_attr;	
	u64			fh_post_change; 
#endif 

} svc_fh;

#endif 


#endif 
