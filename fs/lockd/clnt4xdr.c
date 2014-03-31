/*
 * linux/fs/lockd/clnt4xdr.c
 *
 * XDR functions to encode/decode NLM version 4 RPC arguments and results.
 *
 * NLM client-side only.
 *
 * Copyright (C) 2010, Oracle.  All rights reserved.
 */

#include <linux/types.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/stats.h>
#include <linux/lockd/lockd.h>

#define NLMDBG_FACILITY		NLMDBG_XDR

#if (NLMCLNT_OHSIZE > XDR_MAX_NETOBJ)
#  error "NLM host name cannot be larger than XDR_MAX_NETOBJ!"
#endif

#if (NLMCLNT_OHSIZE > NLM_MAXSTRLEN)
#  error "NLM host name cannot be larger than NLM's maximum string length!"
#endif

#define NLM4_void_sz		(0)
#define NLM4_cookie_sz		(1+(NLM_MAXCOOKIELEN>>2))
#define NLM4_caller_sz		(1+(NLMCLNT_OHSIZE>>2))
#define NLM4_owner_sz		(1+(NLMCLNT_OHSIZE>>2))
#define NLM4_fhandle_sz		(1+(NFS3_FHSIZE>>2))
#define NLM4_lock_sz		(5+NLM4_caller_sz+NLM4_owner_sz+NLM4_fhandle_sz)
#define NLM4_holder_sz		(6+NLM4_owner_sz)

#define NLM4_testargs_sz	(NLM4_cookie_sz+1+NLM4_lock_sz)
#define NLM4_lockargs_sz	(NLM4_cookie_sz+4+NLM4_lock_sz)
#define NLM4_cancargs_sz	(NLM4_cookie_sz+2+NLM4_lock_sz)
#define NLM4_unlockargs_sz	(NLM4_cookie_sz+NLM4_lock_sz)

#define NLM4_testres_sz		(NLM4_cookie_sz+1+NLM4_holder_sz)
#define NLM4_res_sz		(NLM4_cookie_sz+1)
#define NLM4_norep_sz		(0)


static s64 loff_t_to_s64(loff_t offset)
{
	s64 res;

	if (offset >= NLM4_OFFSET_MAX)
		res = NLM4_OFFSET_MAX;
	else if (offset <= -NLM4_OFFSET_MAX)
		res = -NLM4_OFFSET_MAX;
	else
		res = offset;
	return res;
}

static void nlm4_compute_offsets(const struct nlm_lock *lock,
				 u64 *l_offset, u64 *l_len)
{
	const struct file_lock *fl = &lock->fl;

	BUG_ON(fl->fl_start > NLM4_OFFSET_MAX);
	BUG_ON(fl->fl_end > NLM4_OFFSET_MAX &&
				fl->fl_end != OFFSET_MAX);

	*l_offset = loff_t_to_s64(fl->fl_start);
	if (fl->fl_end == OFFSET_MAX)
		*l_len = 0;
	else
		*l_len = loff_t_to_s64(fl->fl_end - fl->fl_start + 1);
}

static void print_overflow_msg(const char *func, const struct xdr_stream *xdr)
{
	dprintk("lockd: %s prematurely hit the end of our receive buffer. "
		"Remaining buffer length is %tu words.\n",
		func, xdr->end - xdr->p);
}



static void encode_bool(struct xdr_stream *xdr, const int value)
{
	__be32 *p;

	p = xdr_reserve_space(xdr, 4);
	*p = value ? xdr_one : xdr_zero;
}

static void encode_int32(struct xdr_stream *xdr, const s32 value)
{
	__be32 *p;

	p = xdr_reserve_space(xdr, 4);
	*p = cpu_to_be32(value);
}

static void encode_netobj(struct xdr_stream *xdr,
			  const u8 *data, const unsigned int length)
{
	__be32 *p;

	BUG_ON(length > XDR_MAX_NETOBJ);
	p = xdr_reserve_space(xdr, 4 + length);
	xdr_encode_opaque(p, data, length);
}

static int decode_netobj(struct xdr_stream *xdr,
			 struct xdr_netobj *obj)
{
	u32 length;
	__be32 *p;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	length = be32_to_cpup(p++);
	if (unlikely(length > XDR_MAX_NETOBJ))
		goto out_size;
	obj->len = length;
	obj->data = (u8 *)p;
	return 0;
out_size:
	dprintk("NFS: returned netobj was too long: %u\n", length);
	return -EIO;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static void encode_cookie(struct xdr_stream *xdr,
			  const struct nlm_cookie *cookie)
{
	BUG_ON(cookie->len > NLM_MAXCOOKIELEN);
	encode_netobj(xdr, (u8 *)&cookie->data, cookie->len);
}

static int decode_cookie(struct xdr_stream *xdr,
			     struct nlm_cookie *cookie)
{
	u32 length;
	__be32 *p;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	length = be32_to_cpup(p++);
	
	if (length == 0)
		goto out_hpux;
	if (length > NLM_MAXCOOKIELEN)
		goto out_size;
	p = xdr_inline_decode(xdr, length);
	if (unlikely(p == NULL))
		goto out_overflow;
	cookie->len = length;
	memcpy(cookie->data, p, length);
	return 0;
out_hpux:
	cookie->len = 4;
	memset(cookie->data, 0, 4);
	return 0;
out_size:
	dprintk("NFS: returned cookie was too long: %u\n", length);
	return -EIO;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static void encode_fh(struct xdr_stream *xdr, const struct nfs_fh *fh)
{
	BUG_ON(fh->size > NFS3_FHSIZE);
	encode_netobj(xdr, (u8 *)&fh->data, fh->size);
}

static void encode_nlm4_stat(struct xdr_stream *xdr,
			     const __be32 stat)
{
	__be32 *p;

	BUG_ON(be32_to_cpu(stat) > NLM_FAILED);
	p = xdr_reserve_space(xdr, 4);
	*p = stat;
}

static int decode_nlm4_stat(struct xdr_stream *xdr, __be32 *stat)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	if (unlikely(ntohl(*p) > ntohl(nlm4_failed)))
		goto out_bad_xdr;
	*stat = *p;
	return 0;
out_bad_xdr:
	dprintk("%s: server returned invalid nlm4_stats value: %u\n",
			__func__, be32_to_cpup(p));
	return -EIO;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static void encode_nlm4_holder(struct xdr_stream *xdr,
			       const struct nlm_res *result)
{
	const struct nlm_lock *lock = &result->lock;
	u64 l_offset, l_len;
	__be32 *p;

	encode_bool(xdr, lock->fl.fl_type == F_RDLCK);
	encode_int32(xdr, lock->svid);
	encode_netobj(xdr, lock->oh.data, lock->oh.len);

	p = xdr_reserve_space(xdr, 4 + 4);
	nlm4_compute_offsets(lock, &l_offset, &l_len);
	p = xdr_encode_hyper(p, l_offset);
	xdr_encode_hyper(p, l_len);
}

static int decode_nlm4_holder(struct xdr_stream *xdr, struct nlm_res *result)
{
	struct nlm_lock *lock = &result->lock;
	struct file_lock *fl = &lock->fl;
	u64 l_offset, l_len;
	u32 exclusive;
	int error;
	__be32 *p;
	s32 end;

	memset(lock, 0, sizeof(*lock));
	locks_init_lock(fl);

	p = xdr_inline_decode(xdr, 4 + 4);
	if (unlikely(p == NULL))
		goto out_overflow;
	exclusive = be32_to_cpup(p++);
	lock->svid = be32_to_cpup(p);
	fl->fl_pid = (pid_t)lock->svid;

	error = decode_netobj(xdr, &lock->oh);
	if (unlikely(error))
		goto out;

	p = xdr_inline_decode(xdr, 8 + 8);
	if (unlikely(p == NULL))
		goto out_overflow;

	fl->fl_flags = FL_POSIX;
	fl->fl_type  = exclusive != 0 ? F_WRLCK : F_RDLCK;
	p = xdr_decode_hyper(p, &l_offset);
	xdr_decode_hyper(p, &l_len);
	end = l_offset + l_len - 1;

	fl->fl_start = (loff_t)l_offset;
	if (l_len == 0 || end < 0)
		fl->fl_end = OFFSET_MAX;
	else
		fl->fl_end = (loff_t)end;
	error = 0;
out:
	return error;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static void encode_caller_name(struct xdr_stream *xdr, const char *name)
{
	
	u32 length = strlen(name);
	__be32 *p;

	BUG_ON(length > NLM_MAXSTRLEN);
	p = xdr_reserve_space(xdr, 4 + length);
	xdr_encode_opaque(p, name, length);
}

static void encode_nlm4_lock(struct xdr_stream *xdr,
			     const struct nlm_lock *lock)
{
	u64 l_offset, l_len;
	__be32 *p;

	encode_caller_name(xdr, lock->caller);
	encode_fh(xdr, &lock->fh);
	encode_netobj(xdr, lock->oh.data, lock->oh.len);

	p = xdr_reserve_space(xdr, 4 + 8 + 8);
	*p++ = cpu_to_be32(lock->svid);

	nlm4_compute_offsets(lock, &l_offset, &l_len);
	p = xdr_encode_hyper(p, l_offset);
	xdr_encode_hyper(p, l_len);
}



static void nlm4_xdr_enc_testargs(struct rpc_rqst *req,
				  struct xdr_stream *xdr,
				  const struct nlm_args *args)
{
	const struct nlm_lock *lock = &args->lock;

	encode_cookie(xdr, &args->cookie);
	encode_bool(xdr, lock->fl.fl_type == F_WRLCK);
	encode_nlm4_lock(xdr, lock);
}

static void nlm4_xdr_enc_lockargs(struct rpc_rqst *req,
				  struct xdr_stream *xdr,
				  const struct nlm_args *args)
{
	const struct nlm_lock *lock = &args->lock;

	encode_cookie(xdr, &args->cookie);
	encode_bool(xdr, args->block);
	encode_bool(xdr, lock->fl.fl_type == F_WRLCK);
	encode_nlm4_lock(xdr, lock);
	encode_bool(xdr, args->reclaim);
	encode_int32(xdr, args->state);
}

static void nlm4_xdr_enc_cancargs(struct rpc_rqst *req,
				  struct xdr_stream *xdr,
				  const struct nlm_args *args)
{
	const struct nlm_lock *lock = &args->lock;

	encode_cookie(xdr, &args->cookie);
	encode_bool(xdr, args->block);
	encode_bool(xdr, lock->fl.fl_type == F_WRLCK);
	encode_nlm4_lock(xdr, lock);
}

static void nlm4_xdr_enc_unlockargs(struct rpc_rqst *req,
				    struct xdr_stream *xdr,
				    const struct nlm_args *args)
{
	const struct nlm_lock *lock = &args->lock;

	encode_cookie(xdr, &args->cookie);
	encode_nlm4_lock(xdr, lock);
}

static void nlm4_xdr_enc_res(struct rpc_rqst *req,
			     struct xdr_stream *xdr,
			     const struct nlm_res *result)
{
	encode_cookie(xdr, &result->cookie);
	encode_nlm4_stat(xdr, result->status);
}

static void nlm4_xdr_enc_testres(struct rpc_rqst *req,
				 struct xdr_stream *xdr,
				 const struct nlm_res *result)
{
	encode_cookie(xdr, &result->cookie);
	encode_nlm4_stat(xdr, result->status);
	if (result->status == nlm_lck_denied)
		encode_nlm4_holder(xdr, result);
}



static int decode_nlm4_testrply(struct xdr_stream *xdr,
				struct nlm_res *result)
{
	int error;

	error = decode_nlm4_stat(xdr, &result->status);
	if (unlikely(error))
		goto out;
	if (result->status == nlm_lck_denied)
		error = decode_nlm4_holder(xdr, result);
out:
	return error;
}

static int nlm4_xdr_dec_testres(struct rpc_rqst *req,
				struct xdr_stream *xdr,
				struct nlm_res *result)
{
	int error;

	error = decode_cookie(xdr, &result->cookie);
	if (unlikely(error))
		goto out;
	error = decode_nlm4_testrply(xdr, result);
out:
	return error;
}

static int nlm4_xdr_dec_res(struct rpc_rqst *req,
			    struct xdr_stream *xdr,
			    struct nlm_res *result)
{
	int error;

	error = decode_cookie(xdr, &result->cookie);
	if (unlikely(error))
		goto out;
	error = decode_nlm4_stat(xdr, &result->status);
out:
	return error;
}


#define nlm4_xdr_dec_norep	NULL

#define PROC(proc, argtype, restype)					\
[NLMPROC_##proc] = {							\
	.p_proc      = NLMPROC_##proc,					\
	.p_encode    = (kxdreproc_t)nlm4_xdr_enc_##argtype,		\
	.p_decode    = (kxdrdproc_t)nlm4_xdr_dec_##restype,		\
	.p_arglen    = NLM4_##argtype##_sz,				\
	.p_replen    = NLM4_##restype##_sz,				\
	.p_statidx   = NLMPROC_##proc,					\
	.p_name      = #proc,						\
	}

static struct rpc_procinfo	nlm4_procedures[] = {
	PROC(TEST,		testargs,	testres),
	PROC(LOCK,		lockargs,	res),
	PROC(CANCEL,		cancargs,	res),
	PROC(UNLOCK,		unlockargs,	res),
	PROC(GRANTED,		testargs,	res),
	PROC(TEST_MSG,		testargs,	norep),
	PROC(LOCK_MSG,		lockargs,	norep),
	PROC(CANCEL_MSG,	cancargs,	norep),
	PROC(UNLOCK_MSG,	unlockargs,	norep),
	PROC(GRANTED_MSG,	testargs,	norep),
	PROC(TEST_RES,		testres,	norep),
	PROC(LOCK_RES,		res,		norep),
	PROC(CANCEL_RES,	res,		norep),
	PROC(UNLOCK_RES,	res,		norep),
	PROC(GRANTED_RES,	res,		norep),
};

const struct rpc_version nlm_version4 = {
	.number		= 4,
	.nrprocs	= ARRAY_SIZE(nlm4_procedures),
	.procs		= nlm4_procedures,
};
