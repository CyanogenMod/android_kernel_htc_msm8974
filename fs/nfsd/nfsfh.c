/*
 * NFS server file handle treatment.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 * Portions Copyright (C) 1999 G. Allen Morris III <gam3@acm.org>
 * Extensive rewrite by Neil Brown <neilb@cse.unsw.edu.au> Southern-Spring 1999
 * ... and again Southern-Winter 2001 to support export_operations
 */

#include <linux/exportfs.h>

#include <linux/sunrpc/svcauth_gss.h>
#include "nfsd.h"
#include "vfs.h"
#include "auth.h"

#define NFSDDBG_FACILITY		NFSDDBG_FH


static int nfsd_acceptable(void *expv, struct dentry *dentry)
{
	struct svc_export *exp = expv;
	int rv;
	struct dentry *tdentry;
	struct dentry *parent;

	if (exp->ex_flags & NFSEXP_NOSUBTREECHECK)
		return 1;

	tdentry = dget(dentry);
	while (tdentry != exp->ex_path.dentry && !IS_ROOT(tdentry)) {
		
		int err;
		parent = dget_parent(tdentry);
		err = inode_permission(parent->d_inode, MAY_EXEC);
		if (err < 0) {
			dput(parent);
			break;
		}
		dput(tdentry);
		tdentry = parent;
	}
	if (tdentry != exp->ex_path.dentry)
		dprintk("nfsd_acceptable failed at %p %s\n", tdentry, tdentry->d_name.name);
	rv = (tdentry == exp->ex_path.dentry);
	dput(tdentry);
	return rv;
}

static inline __be32
nfsd_mode_check(struct svc_rqst *rqstp, umode_t mode, umode_t requested)
{
	mode &= S_IFMT;

	if (requested == 0) 
		return nfs_ok;
	if (mode == requested)
		return nfs_ok;
	if (rqstp->rq_vers == 4 && mode == S_IFLNK)
		return nfserr_symlink;
	if (requested == S_IFDIR)
		return nfserr_notdir;
	if (mode == S_IFDIR)
		return nfserr_isdir;
	return nfserr_inval;
}

static __be32 nfsd_setuser_and_check_port(struct svc_rqst *rqstp,
					  struct svc_export *exp)
{
	int flags = nfsexp_flags(rqstp, exp);

	
	if (!rqstp->rq_secure && !(flags & NFSEXP_INSECURE_PORT)) {
		RPC_IFDEBUG(char buf[RPC_MAX_ADDRBUFLEN]);
		dprintk(KERN_WARNING
		       "nfsd: request from insecure port %s!\n",
		       svc_print_addr(rqstp, buf, sizeof(buf)));
		return nfserr_perm;
	}

	
	return nfserrno(nfsd_setuser(rqstp, exp));
}

static inline __be32 check_pseudo_root(struct svc_rqst *rqstp,
	struct dentry *dentry, struct svc_export *exp)
{
	if (!(exp->ex_flags & NFSEXP_V4ROOT))
		return nfs_ok;
	if (!nfsd_v4client(rqstp))
		return nfserr_stale;
	if (unlikely(!S_ISDIR(dentry->d_inode->i_mode) &&
		     !S_ISLNK(dentry->d_inode->i_mode)))
		return nfserr_stale;
	if (unlikely(dentry != exp->ex_path.dentry))
		return nfserr_stale;
	return nfs_ok;
}

static __be32 nfsd_set_fh_dentry(struct svc_rqst *rqstp, struct svc_fh *fhp)
{
	struct knfsd_fh	*fh = &fhp->fh_handle;
	struct fid *fid = NULL, sfid;
	struct svc_export *exp;
	struct dentry *dentry;
	int fileid_type;
	int data_left = fh->fh_size/4;
	__be32 error;

	error = nfserr_stale;
	if (rqstp->rq_vers > 2)
		error = nfserr_badhandle;
	if (rqstp->rq_vers == 4 && fh->fh_size == 0)
		return nfserr_nofilehandle;

	if (fh->fh_version == 1) {
		int len;

		if (--data_left < 0)
			return error;
		if (fh->fh_auth_type != 0)
			return error;
		len = key_len(fh->fh_fsid_type) / 4;
		if (len == 0)
			return error;
		if  (fh->fh_fsid_type == FSID_MAJOR_MINOR) {
			
			len = key_len(FSID_ENCODE_DEV)/4;
			fh->fh_fsid_type = FSID_ENCODE_DEV;
			fh->fh_fsid[0] = new_encode_dev(MKDEV(ntohl(fh->fh_fsid[0]), ntohl(fh->fh_fsid[1])));
			fh->fh_fsid[1] = fh->fh_fsid[2];
		}
		data_left -= len;
		if (data_left < 0)
			return error;
		exp = rqst_exp_find(rqstp, fh->fh_fsid_type, fh->fh_auth);
		fid = (struct fid *)(fh->fh_auth + len);
	} else {
		__u32 tfh[2];
		dev_t xdev;
		ino_t xino;

		if (fh->fh_size != NFS_FHSIZE)
			return error;
		
		xdev = old_decode_dev(fh->ofh_xdev);
		xino = u32_to_ino_t(fh->ofh_xino);
		mk_fsid(FSID_DEV, tfh, xdev, xino, 0, NULL);
		exp = rqst_exp_find(rqstp, FSID_DEV, tfh);
	}

	error = nfserr_stale;
	if (PTR_ERR(exp) == -ENOENT)
		return error;

	if (IS_ERR(exp))
		return nfserrno(PTR_ERR(exp));

	if (exp->ex_flags & NFSEXP_NOSUBTREECHECK) {
		struct cred *new = prepare_creds();
		if (!new)
			return nfserrno(-ENOMEM);
		new->cap_effective =
			cap_raise_nfsd_set(new->cap_effective,
					   new->cap_permitted);
		put_cred(override_creds(new));
		put_cred(new);
	} else {
		error = nfsd_setuser_and_check_port(rqstp, exp);
		if (error)
			goto out;
	}

	error = nfserr_stale;
	if (rqstp->rq_vers > 2)
		error = nfserr_badhandle;

	if (fh->fh_version != 1) {
		sfid.i32.ino = fh->ofh_ino;
		sfid.i32.gen = fh->ofh_generation;
		sfid.i32.parent_ino = fh->ofh_dirino;
		fid = &sfid;
		data_left = 3;
		if (fh->ofh_dirino == 0)
			fileid_type = FILEID_INO32_GEN;
		else
			fileid_type = FILEID_INO32_GEN_PARENT;
	} else
		fileid_type = fh->fh_fileid_type;

	if (fileid_type == FILEID_ROOT)
		dentry = dget(exp->ex_path.dentry);
	else {
		dentry = exportfs_decode_fh(exp->ex_path.mnt, fid,
				data_left, fileid_type,
				nfsd_acceptable, exp);
	}
	if (dentry == NULL)
		goto out;
	if (IS_ERR(dentry)) {
		if (PTR_ERR(dentry) != -EINVAL)
			error = nfserrno(PTR_ERR(dentry));
		goto out;
	}

	if (S_ISDIR(dentry->d_inode->i_mode) &&
			(dentry->d_flags & DCACHE_DISCONNECTED)) {
		printk("nfsd: find_fh_dentry returned a DISCONNECTED directory: %s/%s\n",
				dentry->d_parent->d_name.name, dentry->d_name.name);
	}

	fhp->fh_dentry = dentry;
	fhp->fh_export = exp;
	return 0;
out:
	exp_put(exp);
	return error;
}

__be32
fh_verify(struct svc_rqst *rqstp, struct svc_fh *fhp, umode_t type, int access)
{
	struct svc_export *exp;
	struct dentry	*dentry;
	__be32		error;

	dprintk("nfsd: fh_verify(%s)\n", SVCFH_fmt(fhp));

	if (!fhp->fh_dentry) {
		error = nfsd_set_fh_dentry(rqstp, fhp);
		if (error)
			goto out;
	}
	dentry = fhp->fh_dentry;
	exp = fhp->fh_export;
	error = check_pseudo_root(rqstp, dentry, exp);
	if (error)
		goto out;

	error = nfsd_setuser_and_check_port(rqstp, exp);
	if (error)
		goto out;

	error = nfsd_mode_check(rqstp, dentry->d_inode->i_mode, type);
	if (error)
		goto out;

	if (access & NFSD_MAY_LOCK || access & NFSD_MAY_BYPASS_GSS)
		goto skip_pseudoflavor_check;
	if (access & NFSD_MAY_BYPASS_GSS_ON_ROOT
			&& exp->ex_path.dentry == dentry)
		goto skip_pseudoflavor_check;

	error = check_nfsd_access(exp, rqstp);
	if (error)
		goto out;

skip_pseudoflavor_check:
	
	error = nfsd_permission(rqstp, exp, dentry, access);

	if (error) {
		dprintk("fh_verify: %s/%s permission failure, "
			"acc=%x, error=%d\n",
			dentry->d_parent->d_name.name,
			dentry->d_name.name,
			access, ntohl(error));
	}
out:
	if (error == nfserr_stale)
		nfsdstats.fh_stale++;
	return error;
}


static void _fh_update(struct svc_fh *fhp, struct svc_export *exp,
		struct dentry *dentry)
{
	if (dentry != exp->ex_path.dentry) {
		struct fid *fid = (struct fid *)
			(fhp->fh_handle.fh_auth + fhp->fh_handle.fh_size/4 - 1);
		int maxsize = (fhp->fh_maxsize - fhp->fh_handle.fh_size)/4;
		int subtreecheck = !(exp->ex_flags & NFSEXP_NOSUBTREECHECK);

		fhp->fh_handle.fh_fileid_type =
			exportfs_encode_fh(dentry, fid, &maxsize, subtreecheck);
		fhp->fh_handle.fh_size += maxsize * 4;
	} else {
		fhp->fh_handle.fh_fileid_type = FILEID_ROOT;
	}
}

static inline void _fh_update_old(struct dentry *dentry,
				  struct svc_export *exp,
				  struct knfsd_fh *fh)
{
	fh->ofh_ino = ino_t_to_u32(dentry->d_inode->i_ino);
	fh->ofh_generation = dentry->d_inode->i_generation;
	if (S_ISDIR(dentry->d_inode->i_mode) ||
	    (exp->ex_flags & NFSEXP_NOSUBTREECHECK))
		fh->ofh_dirino = 0;
}

static bool is_root_export(struct svc_export *exp)
{
	return exp->ex_path.dentry == exp->ex_path.dentry->d_sb->s_root;
}

static struct super_block *exp_sb(struct svc_export *exp)
{
	return exp->ex_path.dentry->d_inode->i_sb;
}

static bool fsid_type_ok_for_exp(u8 fsid_type, struct svc_export *exp)
{
	switch (fsid_type) {
	case FSID_DEV:
		if (!old_valid_dev(exp_sb(exp)->s_dev))
			return 0;
		
	case FSID_MAJOR_MINOR:
	case FSID_ENCODE_DEV:
		return exp_sb(exp)->s_type->fs_flags & FS_REQUIRES_DEV;
	case FSID_NUM:
		return exp->ex_flags & NFSEXP_FSID;
	case FSID_UUID8:
	case FSID_UUID16:
		if (!is_root_export(exp))
			return 0;
		
	case FSID_UUID4_INUM:
	case FSID_UUID16_INUM:
		return exp->ex_uuid != NULL;
	}
	return 1;
}


static void set_version_and_fsid_type(struct svc_fh *fhp, struct svc_export *exp, struct svc_fh *ref_fh)
{
	u8 version;
	u8 fsid_type;
retry:
	version = 1;
	if (ref_fh && ref_fh->fh_export == exp) {
		version = ref_fh->fh_handle.fh_version;
		fsid_type = ref_fh->fh_handle.fh_fsid_type;

		ref_fh = NULL;

		switch (version) {
		case 0xca:
			fsid_type = FSID_DEV;
			break;
		case 1:
			break;
		default:
			goto retry;
		}

		if (!fsid_type_ok_for_exp(fsid_type, exp))
			goto retry;
	} else if (exp->ex_flags & NFSEXP_FSID) {
		fsid_type = FSID_NUM;
	} else if (exp->ex_uuid) {
		if (fhp->fh_maxsize >= 64) {
			if (is_root_export(exp))
				fsid_type = FSID_UUID16;
			else
				fsid_type = FSID_UUID16_INUM;
		} else {
			if (is_root_export(exp))
				fsid_type = FSID_UUID8;
			else
				fsid_type = FSID_UUID4_INUM;
		}
	} else if (!old_valid_dev(exp_sb(exp)->s_dev))
		
		fsid_type = FSID_ENCODE_DEV;
	else
		fsid_type = FSID_DEV;
	fhp->fh_handle.fh_version = version;
	if (version)
		fhp->fh_handle.fh_fsid_type = fsid_type;
}

__be32
fh_compose(struct svc_fh *fhp, struct svc_export *exp, struct dentry *dentry,
	   struct svc_fh *ref_fh)
{

	struct inode * inode = dentry->d_inode;
	struct dentry *parent = dentry->d_parent;
	__u32 *datap;
	dev_t ex_dev = exp_sb(exp)->s_dev;

	dprintk("nfsd: fh_compose(exp %02x:%02x/%ld %s/%s, ino=%ld)\n",
		MAJOR(ex_dev), MINOR(ex_dev),
		(long) exp->ex_path.dentry->d_inode->i_ino,
		parent->d_name.name, dentry->d_name.name,
		(inode ? inode->i_ino : 0));

	 set_version_and_fsid_type(fhp, exp, ref_fh);

	if (ref_fh == fhp)
		fh_put(ref_fh);

	if (fhp->fh_locked || fhp->fh_dentry) {
		printk(KERN_ERR "fh_compose: fh %s/%s not initialized!\n",
		       parent->d_name.name, dentry->d_name.name);
	}
	if (fhp->fh_maxsize < NFS_FHSIZE)
		printk(KERN_ERR "fh_compose: called with maxsize %d! %s/%s\n",
		       fhp->fh_maxsize,
		       parent->d_name.name, dentry->d_name.name);

	fhp->fh_dentry = dget(dentry); 
	fhp->fh_export = exp;
	cache_get(&exp->h);

	if (fhp->fh_handle.fh_version == 0xca) {
		
		memset(&fhp->fh_handle.fh_base, 0, NFS_FHSIZE);
		fhp->fh_handle.fh_size = NFS_FHSIZE;
		fhp->fh_handle.ofh_dcookie = 0xfeebbaca;
		fhp->fh_handle.ofh_dev =  old_encode_dev(ex_dev);
		fhp->fh_handle.ofh_xdev = fhp->fh_handle.ofh_dev;
		fhp->fh_handle.ofh_xino =
			ino_t_to_u32(exp->ex_path.dentry->d_inode->i_ino);
		fhp->fh_handle.ofh_dirino = ino_t_to_u32(parent_ino(dentry));
		if (inode)
			_fh_update_old(dentry, exp, &fhp->fh_handle);
	} else {
		int len;
		fhp->fh_handle.fh_auth_type = 0;
		datap = fhp->fh_handle.fh_auth+0;
		mk_fsid(fhp->fh_handle.fh_fsid_type, datap, ex_dev,
			exp->ex_path.dentry->d_inode->i_ino,
			exp->ex_fsid, exp->ex_uuid);

		len = key_len(fhp->fh_handle.fh_fsid_type);
		datap += len/4;
		fhp->fh_handle.fh_size = 4 + len;

		if (inode)
			_fh_update(fhp, exp, dentry);
		if (fhp->fh_handle.fh_fileid_type == 255) {
			fh_put(fhp);
			return nfserr_opnotsupp;
		}
	}

	return 0;
}

__be32
fh_update(struct svc_fh *fhp)
{
	struct dentry *dentry;

	if (!fhp->fh_dentry)
		goto out_bad;

	dentry = fhp->fh_dentry;
	if (!dentry->d_inode)
		goto out_negative;
	if (fhp->fh_handle.fh_version != 1) {
		_fh_update_old(dentry, fhp->fh_export, &fhp->fh_handle);
	} else {
		if (fhp->fh_handle.fh_fileid_type != FILEID_ROOT)
			goto out;

		_fh_update(fhp, fhp->fh_export, dentry);
		if (fhp->fh_handle.fh_fileid_type == 255)
			return nfserr_opnotsupp;
	}
out:
	return 0;

out_bad:
	printk(KERN_ERR "fh_update: fh not verified!\n");
	goto out;
out_negative:
	printk(KERN_ERR "fh_update: %s/%s still negative!\n",
		dentry->d_parent->d_name.name, dentry->d_name.name);
	goto out;
}

void
fh_put(struct svc_fh *fhp)
{
	struct dentry * dentry = fhp->fh_dentry;
	struct svc_export * exp = fhp->fh_export;
	if (dentry) {
		fh_unlock(fhp);
		fhp->fh_dentry = NULL;
		dput(dentry);
#ifdef CONFIG_NFSD_V3
		fhp->fh_pre_saved = 0;
		fhp->fh_post_saved = 0;
#endif
	}
	if (exp) {
		cache_put(&exp->h, &svc_export_cache);
		fhp->fh_export = NULL;
	}
	return;
}

char * SVCFH_fmt(struct svc_fh *fhp)
{
	struct knfsd_fh *fh = &fhp->fh_handle;

	static char buf[80];
	sprintf(buf, "%d: %08x %08x %08x %08x %08x %08x",
		fh->fh_size,
		fh->fh_base.fh_pad[0],
		fh->fh_base.fh_pad[1],
		fh->fh_base.fh_pad[2],
		fh->fh_base.fh_pad[3],
		fh->fh_base.fh_pad[4],
		fh->fh_base.fh_pad[5]);
	return buf;
}

enum fsid_source fsid_source(struct svc_fh *fhp)
{
	if (fhp->fh_handle.fh_version != 1)
		return FSIDSOURCE_DEV;
	switch(fhp->fh_handle.fh_fsid_type) {
	case FSID_DEV:
	case FSID_ENCODE_DEV:
	case FSID_MAJOR_MINOR:
		if (exp_sb(fhp->fh_export)->s_type->fs_flags & FS_REQUIRES_DEV)
			return FSIDSOURCE_DEV;
		break;
	case FSID_NUM:
		if (fhp->fh_export->ex_flags & NFSEXP_FSID)
			return FSIDSOURCE_FSID;
		break;
	default:
		break;
	}
	if (fhp->fh_export->ex_flags & NFSEXP_FSID)
		return FSIDSOURCE_FSID;
	if (fhp->fh_export->ex_uuid)
		return FSIDSOURCE_UUID;
	return FSIDSOURCE_DEV;
}
