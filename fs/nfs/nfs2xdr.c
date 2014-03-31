/*
 * linux/fs/nfs/nfs2xdr.c
 *
 * XDR functions to encode/decode NFS RPC arguments and results.
 *
 * Copyright (C) 1992, 1993, 1994  Rick Sladkey
 * Copyright (C) 1996 Olaf Kirch
 * 04 Aug 1998  Ion Badulescu <ionut@cs.columbia.edu>
 * 		FIFO's need special handling in NFSv2
 */

#include <linux/param.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/pagemap.h>
#include <linux/proc_fs.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs.h>
#include <linux/nfs2.h>
#include <linux/nfs_fs.h>
#include "internal.h"

#define NFSDBG_FACILITY		NFSDBG_XDR

#define errno_NFSERR_IO		EIO

#define NFS_fhandle_sz		(8)
#define NFS_sattr_sz		(8)
#define NFS_filename_sz		(1+(NFS2_MAXNAMLEN>>2))
#define NFS_path_sz		(1+(NFS2_MAXPATHLEN>>2))
#define NFS_fattr_sz		(17)
#define NFS_info_sz		(5)
#define NFS_entry_sz		(NFS_filename_sz+3)

#define NFS_diropargs_sz	(NFS_fhandle_sz+NFS_filename_sz)
#define NFS_removeargs_sz	(NFS_fhandle_sz+NFS_filename_sz)
#define NFS_sattrargs_sz	(NFS_fhandle_sz+NFS_sattr_sz)
#define NFS_readlinkargs_sz	(NFS_fhandle_sz)
#define NFS_readargs_sz		(NFS_fhandle_sz+3)
#define NFS_writeargs_sz	(NFS_fhandle_sz+4)
#define NFS_createargs_sz	(NFS_diropargs_sz+NFS_sattr_sz)
#define NFS_renameargs_sz	(NFS_diropargs_sz+NFS_diropargs_sz)
#define NFS_linkargs_sz		(NFS_fhandle_sz+NFS_diropargs_sz)
#define NFS_symlinkargs_sz	(NFS_diropargs_sz+1+NFS_sattr_sz)
#define NFS_readdirargs_sz	(NFS_fhandle_sz+2)

#define NFS_attrstat_sz		(1+NFS_fattr_sz)
#define NFS_diropres_sz		(1+NFS_fhandle_sz+NFS_fattr_sz)
#define NFS_readlinkres_sz	(2)
#define NFS_readres_sz		(1+NFS_fattr_sz+1)
#define NFS_writeres_sz         (NFS_attrstat_sz)
#define NFS_stat_sz		(1)
#define NFS_readdirres_sz	(1)
#define NFS_statfsres_sz	(1+NFS_info_sz)


static void prepare_reply_buffer(struct rpc_rqst *req, struct page **pages,
				 unsigned int base, unsigned int len,
				 unsigned int bufsize)
{
	struct rpc_auth	*auth = req->rq_cred->cr_auth;
	unsigned int replen;

	replen = RPC_REPHDRSIZE + auth->au_rslack + bufsize;
	xdr_inline_pages(&req->rq_rcv_buf, replen << 2, pages, base, len);
}

static void print_overflow_msg(const char *func, const struct xdr_stream *xdr)
{
	dprintk("NFS: %s prematurely hit the end of our receive buffer. "
		"Remaining buffer length is %tu words.\n",
		func, xdr->end - xdr->p);
}



static int decode_nfsdata(struct xdr_stream *xdr, struct nfs_readres *result)
{
	u32 recvd, count;
	size_t hdrlen;
	__be32 *p;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	count = be32_to_cpup(p);
	hdrlen = (u8 *)xdr->p - (u8 *)xdr->iov->iov_base;
	recvd = xdr->buf->len - hdrlen;
	if (unlikely(count > recvd))
		goto out_cheating;
out:
	xdr_read_pages(xdr, count);
	result->eof = 0;	
	result->count = count;
	return count;
out_cheating:
	dprintk("NFS: server cheating in read result: "
		"count %u > recvd %u\n", count, recvd);
	count = recvd;
	goto out;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_stat(struct xdr_stream *xdr, enum nfs_stat *status)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	*status = be32_to_cpup(p);
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static __be32 *xdr_decode_ftype(__be32 *p, u32 *type)
{
	*type = be32_to_cpup(p++);
	if (unlikely(*type > NF2FIFO))
		*type = NFBAD;
	return p;
}

static void encode_fhandle(struct xdr_stream *xdr, const struct nfs_fh *fh)
{
	__be32 *p;

	BUG_ON(fh->size != NFS2_FHSIZE);
	p = xdr_reserve_space(xdr, NFS2_FHSIZE);
	memcpy(p, fh->data, NFS2_FHSIZE);
}

static int decode_fhandle(struct xdr_stream *xdr, struct nfs_fh *fh)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, NFS2_FHSIZE);
	if (unlikely(p == NULL))
		goto out_overflow;
	fh->size = NFS2_FHSIZE;
	memcpy(fh->data, p, NFS2_FHSIZE);
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static __be32 *xdr_encode_time(__be32 *p, const struct timespec *timep)
{
	*p++ = cpu_to_be32(timep->tv_sec);
	if (timep->tv_nsec != 0)
		*p++ = cpu_to_be32(timep->tv_nsec / NSEC_PER_USEC);
	else
		*p++ = cpu_to_be32(0);
	return p;
}

static __be32 *xdr_encode_current_server_time(__be32 *p,
					      const struct timespec *timep)
{
	*p++ = cpu_to_be32(timep->tv_sec);
	*p++ = cpu_to_be32(1000000);
	return p;
}

static __be32 *xdr_decode_time(__be32 *p, struct timespec *timep)
{
	timep->tv_sec = be32_to_cpup(p++);
	timep->tv_nsec = be32_to_cpup(p++) * NSEC_PER_USEC;
	return p;
}

static int decode_fattr(struct xdr_stream *xdr, struct nfs_fattr *fattr)
{
	u32 rdev, type;
	__be32 *p;

	p = xdr_inline_decode(xdr, NFS_fattr_sz << 2);
	if (unlikely(p == NULL))
		goto out_overflow;

	fattr->valid |= NFS_ATTR_FATTR_V2;

	p = xdr_decode_ftype(p, &type);

	fattr->mode = be32_to_cpup(p++);
	fattr->nlink = be32_to_cpup(p++);
	fattr->uid = be32_to_cpup(p++);
	fattr->gid = be32_to_cpup(p++);
	fattr->size = be32_to_cpup(p++);
	fattr->du.nfs2.blocksize = be32_to_cpup(p++);

	rdev = be32_to_cpup(p++);
	fattr->rdev = new_decode_dev(rdev);
	if (type == (u32)NFCHR && rdev == (u32)NFS2_FIFO_DEV) {
		fattr->mode = (fattr->mode & ~S_IFMT) | S_IFIFO;
		fattr->rdev = 0;
	}

	fattr->du.nfs2.blocks = be32_to_cpup(p++);
	fattr->fsid.major = be32_to_cpup(p++);
	fattr->fsid.minor = 0;
	fattr->fileid = be32_to_cpup(p++);

	p = xdr_decode_time(p, &fattr->atime);
	p = xdr_decode_time(p, &fattr->mtime);
	xdr_decode_time(p, &fattr->ctime);
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}


#define NFS2_SATTR_NOT_SET	(0xffffffff)

static __be32 *xdr_time_not_set(__be32 *p)
{
	*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	return p;
}

static void encode_sattr(struct xdr_stream *xdr, const struct iattr *attr)
{
	__be32 *p;

	p = xdr_reserve_space(xdr, NFS_sattr_sz << 2);

	if (attr->ia_valid & ATTR_MODE)
		*p++ = cpu_to_be32(attr->ia_mode);
	else
		*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	if (attr->ia_valid & ATTR_UID)
		*p++ = cpu_to_be32(attr->ia_uid);
	else
		*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	if (attr->ia_valid & ATTR_GID)
		*p++ = cpu_to_be32(attr->ia_gid);
	else
		*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);
	if (attr->ia_valid & ATTR_SIZE)
		*p++ = cpu_to_be32((u32)attr->ia_size);
	else
		*p++ = cpu_to_be32(NFS2_SATTR_NOT_SET);

	if (attr->ia_valid & ATTR_ATIME_SET)
		p = xdr_encode_time(p, &attr->ia_atime);
	else if (attr->ia_valid & ATTR_ATIME)
		p = xdr_encode_current_server_time(p, &attr->ia_atime);
	else
		p = xdr_time_not_set(p);
	if (attr->ia_valid & ATTR_MTIME_SET)
		xdr_encode_time(p, &attr->ia_mtime);
	else if (attr->ia_valid & ATTR_MTIME)
		xdr_encode_current_server_time(p, &attr->ia_mtime);
	else
		xdr_time_not_set(p);
}

static void encode_filename(struct xdr_stream *xdr,
			    const char *name, u32 length)
{
	__be32 *p;

	BUG_ON(length > NFS2_MAXNAMLEN);
	p = xdr_reserve_space(xdr, 4 + length);
	xdr_encode_opaque(p, name, length);
}

static int decode_filename_inline(struct xdr_stream *xdr,
				  const char **name, u32 *length)
{
	__be32 *p;
	u32 count;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	count = be32_to_cpup(p);
	if (count > NFS3_MAXNAMLEN)
		goto out_nametoolong;
	p = xdr_inline_decode(xdr, count);
	if (unlikely(p == NULL))
		goto out_overflow;
	*name = (const char *)p;
	*length = count;
	return 0;
out_nametoolong:
	dprintk("NFS: returned filename too long: %u\n", count);
	return -ENAMETOOLONG;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static void encode_path(struct xdr_stream *xdr, struct page **pages, u32 length)
{
	__be32 *p;

	BUG_ON(length > NFS2_MAXPATHLEN);
	p = xdr_reserve_space(xdr, 4);
	*p = cpu_to_be32(length);
	xdr_write_pages(xdr, pages, 0, length);
}

static int decode_path(struct xdr_stream *xdr)
{
	u32 length, recvd;
	size_t hdrlen;
	__be32 *p;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	length = be32_to_cpup(p);
	if (unlikely(length >= xdr->buf->page_len || length > NFS_MAXPATHLEN))
		goto out_size;
	hdrlen = (u8 *)xdr->p - (u8 *)xdr->iov->iov_base;
	recvd = xdr->buf->len - hdrlen;
	if (unlikely(length > recvd))
		goto out_cheating;

	xdr_read_pages(xdr, length);
	xdr_terminate_string(xdr->buf, length);
	return 0;
out_size:
	dprintk("NFS: returned pathname too long: %u\n", length);
	return -ENAMETOOLONG;
out_cheating:
	dprintk("NFS: server cheating in pathname result: "
		"length %u > received %u\n", length, recvd);
	return -EIO;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attrstat(struct xdr_stream *xdr, struct nfs_fattr *result)
{
	enum nfs_stat status;
	int error;

	error = decode_stat(xdr, &status);
	if (unlikely(error))
		goto out;
	if (status != NFS_OK)
		goto out_default;
	error = decode_fattr(xdr, result);
out:
	return error;
out_default:
	return nfs_stat_to_errno(status);
}

static void encode_diropargs(struct xdr_stream *xdr, const struct nfs_fh *fh,
			     const char *name, u32 length)
{
	encode_fhandle(xdr, fh);
	encode_filename(xdr, name, length);
}

static int decode_diropok(struct xdr_stream *xdr, struct nfs_diropok *result)
{
	int error;

	error = decode_fhandle(xdr, result->fh);
	if (unlikely(error))
		goto out;
	error = decode_fattr(xdr, result->fattr);
out:
	return error;
}

static int decode_diropres(struct xdr_stream *xdr, struct nfs_diropok *result)
{
	enum nfs_stat status;
	int error;

	error = decode_stat(xdr, &status);
	if (unlikely(error))
		goto out;
	if (status != NFS_OK)
		goto out_default;
	error = decode_diropok(xdr, result);
out:
	return error;
out_default:
	return nfs_stat_to_errno(status);
}



static void nfs2_xdr_enc_fhandle(struct rpc_rqst *req,
				 struct xdr_stream *xdr,
				 const struct nfs_fh *fh)
{
	encode_fhandle(xdr, fh);
}

static void nfs2_xdr_enc_sattrargs(struct rpc_rqst *req,
				   struct xdr_stream *xdr,
				   const struct nfs_sattrargs *args)
{
	encode_fhandle(xdr, args->fh);
	encode_sattr(xdr, args->sattr);
}

static void nfs2_xdr_enc_diropargs(struct rpc_rqst *req,
				   struct xdr_stream *xdr,
				   const struct nfs_diropargs *args)
{
	encode_diropargs(xdr, args->fh, args->name, args->len);
}

static void nfs2_xdr_enc_readlinkargs(struct rpc_rqst *req,
				      struct xdr_stream *xdr,
				      const struct nfs_readlinkargs *args)
{
	encode_fhandle(xdr, args->fh);
	prepare_reply_buffer(req, args->pages, args->pgbase,
					args->pglen, NFS_readlinkres_sz);
}

static void encode_readargs(struct xdr_stream *xdr,
			    const struct nfs_readargs *args)
{
	u32 offset = args->offset;
	u32 count = args->count;
	__be32 *p;

	encode_fhandle(xdr, args->fh);

	p = xdr_reserve_space(xdr, 4 + 4 + 4);
	*p++ = cpu_to_be32(offset);
	*p++ = cpu_to_be32(count);
	*p = cpu_to_be32(count);
}

static void nfs2_xdr_enc_readargs(struct rpc_rqst *req,
				  struct xdr_stream *xdr,
				  const struct nfs_readargs *args)
{
	encode_readargs(xdr, args);
	prepare_reply_buffer(req, args->pages, args->pgbase,
					args->count, NFS_readres_sz);
	req->rq_rcv_buf.flags |= XDRBUF_READ;
}

static void encode_writeargs(struct xdr_stream *xdr,
			     const struct nfs_writeargs *args)
{
	u32 offset = args->offset;
	u32 count = args->count;
	__be32 *p;

	encode_fhandle(xdr, args->fh);

	p = xdr_reserve_space(xdr, 4 + 4 + 4 + 4);
	*p++ = cpu_to_be32(offset);
	*p++ = cpu_to_be32(offset);
	*p++ = cpu_to_be32(count);

	
	*p = cpu_to_be32(count);
	xdr_write_pages(xdr, args->pages, args->pgbase, count);
}

static void nfs2_xdr_enc_writeargs(struct rpc_rqst *req,
				   struct xdr_stream *xdr,
				   const struct nfs_writeargs *args)
{
	encode_writeargs(xdr, args);
	xdr->buf->flags |= XDRBUF_WRITE;
}

static void nfs2_xdr_enc_createargs(struct rpc_rqst *req,
				    struct xdr_stream *xdr,
				    const struct nfs_createargs *args)
{
	encode_diropargs(xdr, args->fh, args->name, args->len);
	encode_sattr(xdr, args->sattr);
}

static void nfs2_xdr_enc_removeargs(struct rpc_rqst *req,
				    struct xdr_stream *xdr,
				    const struct nfs_removeargs *args)
{
	encode_diropargs(xdr, args->fh, args->name.name, args->name.len);
}

static void nfs2_xdr_enc_renameargs(struct rpc_rqst *req,
				    struct xdr_stream *xdr,
				    const struct nfs_renameargs *args)
{
	const struct qstr *old = args->old_name;
	const struct qstr *new = args->new_name;

	encode_diropargs(xdr, args->old_dir, old->name, old->len);
	encode_diropargs(xdr, args->new_dir, new->name, new->len);
}

static void nfs2_xdr_enc_linkargs(struct rpc_rqst *req,
				  struct xdr_stream *xdr,
				  const struct nfs_linkargs *args)
{
	encode_fhandle(xdr, args->fromfh);
	encode_diropargs(xdr, args->tofh, args->toname, args->tolen);
}

static void nfs2_xdr_enc_symlinkargs(struct rpc_rqst *req,
				     struct xdr_stream *xdr,
				     const struct nfs_symlinkargs *args)
{
	encode_diropargs(xdr, args->fromfh, args->fromname, args->fromlen);
	encode_path(xdr, args->pages, args->pathlen);
	encode_sattr(xdr, args->sattr);
}

static void encode_readdirargs(struct xdr_stream *xdr,
			       const struct nfs_readdirargs *args)
{
	__be32 *p;

	encode_fhandle(xdr, args->fh);

	p = xdr_reserve_space(xdr, 4 + 4);
	*p++ = cpu_to_be32(args->cookie);
	*p = cpu_to_be32(args->count);
}

static void nfs2_xdr_enc_readdirargs(struct rpc_rqst *req,
				     struct xdr_stream *xdr,
				     const struct nfs_readdirargs *args)
{
	encode_readdirargs(xdr, args);
	prepare_reply_buffer(req, args->pages, 0,
					args->count, NFS_readdirres_sz);
}


static int nfs2_xdr_dec_stat(struct rpc_rqst *req, struct xdr_stream *xdr,
			     void *__unused)
{
	enum nfs_stat status;
	int error;

	error = decode_stat(xdr, &status);
	if (unlikely(error))
		goto out;
	if (status != NFS_OK)
		goto out_default;
out:
	return error;
out_default:
	return nfs_stat_to_errno(status);
}

static int nfs2_xdr_dec_attrstat(struct rpc_rqst *req, struct xdr_stream *xdr,
				 struct nfs_fattr *result)
{
	return decode_attrstat(xdr, result);
}

static int nfs2_xdr_dec_diropres(struct rpc_rqst *req, struct xdr_stream *xdr,
				 struct nfs_diropok *result)
{
	return decode_diropres(xdr, result);
}

static int nfs2_xdr_dec_readlinkres(struct rpc_rqst *req,
				    struct xdr_stream *xdr, void *__unused)
{
	enum nfs_stat status;
	int error;

	error = decode_stat(xdr, &status);
	if (unlikely(error))
		goto out;
	if (status != NFS_OK)
		goto out_default;
	error = decode_path(xdr);
out:
	return error;
out_default:
	return nfs_stat_to_errno(status);
}

static int nfs2_xdr_dec_readres(struct rpc_rqst *req, struct xdr_stream *xdr,
				struct nfs_readres *result)
{
	enum nfs_stat status;
	int error;

	error = decode_stat(xdr, &status);
	if (unlikely(error))
		goto out;
	if (status != NFS_OK)
		goto out_default;
	error = decode_fattr(xdr, result->fattr);
	if (unlikely(error))
		goto out;
	error = decode_nfsdata(xdr, result);
out:
	return error;
out_default:
	return nfs_stat_to_errno(status);
}

static int nfs2_xdr_dec_writeres(struct rpc_rqst *req, struct xdr_stream *xdr,
				 struct nfs_writeres *result)
{
	
	result->verf->committed = NFS_FILE_SYNC;
	return decode_attrstat(xdr, result->fattr);
}

int nfs2_decode_dirent(struct xdr_stream *xdr, struct nfs_entry *entry,
		       int plus)
{
	__be32 *p;
	int error;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	if (*p++ == xdr_zero) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(p == NULL))
			goto out_overflow;
		if (*p++ == xdr_zero)
			return -EAGAIN;
		entry->eof = 1;
		return -EBADCOOKIE;
	}

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	entry->ino = be32_to_cpup(p);

	error = decode_filename_inline(xdr, &entry->name, &entry->len);
	if (unlikely(error))
		return error;

	entry->prev_cookie = entry->cookie;
	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	entry->cookie = be32_to_cpup(p);

	entry->d_type = DT_UNKNOWN;

	return 0;

out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EAGAIN;
}

static int decode_readdirok(struct xdr_stream *xdr)
{
	u32 recvd, pglen;
	size_t hdrlen;

	pglen = xdr->buf->page_len;
	hdrlen = (u8 *)xdr->p - (u8 *)xdr->iov->iov_base;
	recvd = xdr->buf->len - hdrlen;
	if (unlikely(pglen > recvd))
		goto out_cheating;
out:
	xdr_read_pages(xdr, pglen);
	return pglen;
out_cheating:
	dprintk("NFS: server cheating in readdir result: "
		"pglen %u > recvd %u\n", pglen, recvd);
	pglen = recvd;
	goto out;
}

static int nfs2_xdr_dec_readdirres(struct rpc_rqst *req,
				   struct xdr_stream *xdr, void *__unused)
{
	enum nfs_stat status;
	int error;

	error = decode_stat(xdr, &status);
	if (unlikely(error))
		goto out;
	if (status != NFS_OK)
		goto out_default;
	error = decode_readdirok(xdr);
out:
	return error;
out_default:
	return nfs_stat_to_errno(status);
}

static int decode_info(struct xdr_stream *xdr, struct nfs2_fsstat *result)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, NFS_info_sz << 2);
	if (unlikely(p == NULL))
		goto out_overflow;
	result->tsize  = be32_to_cpup(p++);
	result->bsize  = be32_to_cpup(p++);
	result->blocks = be32_to_cpup(p++);
	result->bfree  = be32_to_cpup(p++);
	result->bavail = be32_to_cpup(p);
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int nfs2_xdr_dec_statfsres(struct rpc_rqst *req, struct xdr_stream *xdr,
				  struct nfs2_fsstat *result)
{
	enum nfs_stat status;
	int error;

	error = decode_stat(xdr, &status);
	if (unlikely(error))
		goto out;
	if (status != NFS_OK)
		goto out_default;
	error = decode_info(xdr, result);
out:
	return error;
out_default:
	return nfs_stat_to_errno(status);
}


static const struct {
	int stat;
	int errno;
} nfs_errtbl[] = {
	{ NFS_OK,		0		},
	{ NFSERR_PERM,		-EPERM		},
	{ NFSERR_NOENT,		-ENOENT		},
	{ NFSERR_IO,		-errno_NFSERR_IO},
	{ NFSERR_NXIO,		-ENXIO		},
	{ NFSERR_ACCES,		-EACCES		},
	{ NFSERR_EXIST,		-EEXIST		},
	{ NFSERR_XDEV,		-EXDEV		},
	{ NFSERR_NODEV,		-ENODEV		},
	{ NFSERR_NOTDIR,	-ENOTDIR	},
	{ NFSERR_ISDIR,		-EISDIR		},
	{ NFSERR_INVAL,		-EINVAL		},
	{ NFSERR_FBIG,		-EFBIG		},
	{ NFSERR_NOSPC,		-ENOSPC		},
	{ NFSERR_ROFS,		-EROFS		},
	{ NFSERR_MLINK,		-EMLINK		},
	{ NFSERR_NAMETOOLONG,	-ENAMETOOLONG	},
	{ NFSERR_NOTEMPTY,	-ENOTEMPTY	},
	{ NFSERR_DQUOT,		-EDQUOT		},
	{ NFSERR_STALE,		-ESTALE		},
	{ NFSERR_REMOTE,	-EREMOTE	},
#ifdef EWFLUSH
	{ NFSERR_WFLUSH,	-EWFLUSH	},
#endif
	{ NFSERR_BADHANDLE,	-EBADHANDLE	},
	{ NFSERR_NOT_SYNC,	-ENOTSYNC	},
	{ NFSERR_BAD_COOKIE,	-EBADCOOKIE	},
	{ NFSERR_NOTSUPP,	-ENOTSUPP	},
	{ NFSERR_TOOSMALL,	-ETOOSMALL	},
	{ NFSERR_SERVERFAULT,	-EREMOTEIO	},
	{ NFSERR_BADTYPE,	-EBADTYPE	},
	{ NFSERR_JUKEBOX,	-EJUKEBOX	},
	{ -1,			-EIO		}
};

int nfs_stat_to_errno(enum nfs_stat status)
{
	int i;

	for (i = 0; nfs_errtbl[i].stat != -1; i++) {
		if (nfs_errtbl[i].stat == (int)status)
			return nfs_errtbl[i].errno;
	}
	dprintk("NFS: Unrecognized nfs status value: %u\n", status);
	return nfs_errtbl[i].errno;
}

#define PROC(proc, argtype, restype, timer)				\
[NFSPROC_##proc] = {							\
	.p_proc	    =  NFSPROC_##proc,					\
	.p_encode   =  (kxdreproc_t)nfs2_xdr_enc_##argtype,		\
	.p_decode   =  (kxdrdproc_t)nfs2_xdr_dec_##restype,		\
	.p_arglen   =  NFS_##argtype##_sz,				\
	.p_replen   =  NFS_##restype##_sz,				\
	.p_timer    =  timer,						\
	.p_statidx  =  NFSPROC_##proc,					\
	.p_name     =  #proc,						\
	}
struct rpc_procinfo	nfs_procedures[] = {
	PROC(GETATTR,	fhandle,	attrstat,	1),
	PROC(SETATTR,	sattrargs,	attrstat,	0),
	PROC(LOOKUP,	diropargs,	diropres,	2),
	PROC(READLINK,	readlinkargs,	readlinkres,	3),
	PROC(READ,	readargs,	readres,	3),
	PROC(WRITE,	writeargs,	writeres,	4),
	PROC(CREATE,	createargs,	diropres,	0),
	PROC(REMOVE,	removeargs,	stat,		0),
	PROC(RENAME,	renameargs,	stat,		0),
	PROC(LINK,	linkargs,	stat,		0),
	PROC(SYMLINK,	symlinkargs,	stat,		0),
	PROC(MKDIR,	createargs,	diropres,	0),
	PROC(RMDIR,	diropargs,	stat,		0),
	PROC(READDIR,	readdirargs,	readdirres,	3),
	PROC(STATFS,	fhandle,	statfsres,	0),
};

const struct rpc_version nfs_version2 = {
	.number			= 2,
	.nrprocs		= ARRAY_SIZE(nfs_procedures),
	.procs			= nfs_procedures
};
