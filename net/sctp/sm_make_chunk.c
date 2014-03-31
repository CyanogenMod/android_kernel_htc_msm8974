/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001-2002 Intel Corp.
 *
 * This file is part of the SCTP kernel implementation
 *
 * These functions work with the state functions in sctp_sm_statefuns.c
 * to implement the state operations.  These functions implement the
 * steps which require modifying existing data structures.
 *
 * This SCTP implementation is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by:
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Karl Knutson          <karl@athena.chicago.il.us>
 *    C. Robin              <chris@hundredacre.ac.uk>
 *    Jon Grimm             <jgrimm@us.ibm.com>
 *    Xingang Guo           <xingang.guo@intel.com>
 *    Dajiang Zhang	    <dajiang.zhang@nokia.com>
 *    Sridhar Samudrala	    <sri@us.ibm.com>
 *    Daisy Chang	    <daisyc@us.ibm.com>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Kevin Gao             <kevin.gao@intel.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/net.h>
#include <linux/inet.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
#include <linux/slab.h>
#include <net/sock.h>

#include <linux/skbuff.h>
#include <linux/random.h>	
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

SCTP_STATIC
struct sctp_chunk *sctp_make_chunk(const struct sctp_association *asoc,
				   __u8 type, __u8 flags, int paylen);
static sctp_cookie_param_t *sctp_pack_cookie(const struct sctp_endpoint *ep,
					const struct sctp_association *asoc,
					const struct sctp_chunk *init_chunk,
					int *cookie_len,
					const __u8 *raw_addrs, int addrs_len);
static int sctp_process_param(struct sctp_association *asoc,
			      union sctp_params param,
			      const union sctp_addr *peer_addr,
			      gfp_t gfp);
static void *sctp_addto_param(struct sctp_chunk *chunk, int len,
			      const void *data);

int sctp_chunk_iif(const struct sctp_chunk *chunk)
{
	struct sctp_af *af;
	int iif = 0;

	af = sctp_get_af_specific(ipver2af(ip_hdr(chunk->skb)->version));
	if (af)
		iif = af->skb_iif(chunk->skb);

	return iif;
}

static const struct sctp_paramhdr ecap_param = {
	SCTP_PARAM_ECN_CAPABLE,
	cpu_to_be16(sizeof(struct sctp_paramhdr)),
};
static const struct sctp_paramhdr prsctp_param = {
	SCTP_PARAM_FWD_TSN_SUPPORT,
	cpu_to_be16(sizeof(struct sctp_paramhdr)),
};

void  sctp_init_cause(struct sctp_chunk *chunk, __be16 cause_code,
		      size_t paylen)
{
	sctp_errhdr_t err;
	__u16 len;

	
	err.cause = cause_code;
	len = sizeof(sctp_errhdr_t) + paylen;
	err.length  = htons(len);
	chunk->subh.err_hdr = sctp_addto_chunk(chunk, sizeof(sctp_errhdr_t), &err);
}

int sctp_init_cause_fixed(struct sctp_chunk *chunk, __be16 cause_code,
		      size_t paylen)
{
	sctp_errhdr_t err;
	__u16 len;

	
	err.cause = cause_code;
	len = sizeof(sctp_errhdr_t) + paylen;
	err.length  = htons(len);

	if (skb_tailroom(chunk->skb) < len)
		return -ENOSPC;
	chunk->subh.err_hdr = sctp_addto_chunk_fixed(chunk,
						     sizeof(sctp_errhdr_t),
						     &err);
	return 0;
}
struct sctp_chunk *sctp_make_init(const struct sctp_association *asoc,
			     const struct sctp_bind_addr *bp,
			     gfp_t gfp, int vparam_len)
{
	sctp_inithdr_t init;
	union sctp_params addrs;
	size_t chunksize;
	struct sctp_chunk *retval = NULL;
	int num_types, addrs_len = 0;
	struct sctp_sock *sp;
	sctp_supported_addrs_param_t sat;
	__be16 types[2];
	sctp_adaptation_ind_param_t aiparam;
	sctp_supported_ext_param_t ext_param;
	int num_ext = 0;
	__u8 extensions[3];
	sctp_paramhdr_t *auth_chunks = NULL,
			*auth_hmacs = NULL;

	retval = NULL;

	
	addrs = sctp_bind_addrs_to_raw(bp, &addrs_len, gfp);

	init.init_tag		   = htonl(asoc->c.my_vtag);
	init.a_rwnd		   = htonl(asoc->rwnd);
	init.num_outbound_streams  = htons(asoc->c.sinit_num_ostreams);
	init.num_inbound_streams   = htons(asoc->c.sinit_max_instreams);
	init.initial_tsn	   = htonl(asoc->c.initial_tsn);

	
	sp = sctp_sk(asoc->base.sk);
	num_types = sp->pf->supported_addrs(sp, types);

	chunksize = sizeof(init) + addrs_len;
	chunksize += WORD_ROUND(SCTP_SAT_LEN(num_types));
	chunksize += sizeof(ecap_param);

	if (sctp_prsctp_enable)
		chunksize += sizeof(prsctp_param);

	if (sctp_addip_enable) {
		extensions[num_ext] = SCTP_CID_ASCONF;
		extensions[num_ext+1] = SCTP_CID_ASCONF_ACK;
		num_ext += 2;
	}

	if (sp->adaptation_ind)
		chunksize += sizeof(aiparam);

	chunksize += vparam_len;

	
	if (sctp_auth_enable) {
		
		chunksize += sizeof(asoc->c.auth_random);

		
		auth_hmacs = (sctp_paramhdr_t *)asoc->c.auth_hmacs;
		if (auth_hmacs->length)
			chunksize += WORD_ROUND(ntohs(auth_hmacs->length));
		else
			auth_hmacs = NULL;

		
		auth_chunks = (sctp_paramhdr_t *)asoc->c.auth_chunks;
		if (auth_chunks->length)
			chunksize += WORD_ROUND(ntohs(auth_chunks->length));
		else
			auth_chunks = NULL;

		extensions[num_ext] = SCTP_CID_AUTH;
		num_ext += 1;
	}

	
	if (num_ext)
		chunksize += WORD_ROUND(sizeof(sctp_supported_ext_param_t) +
					num_ext);


	retval = sctp_make_chunk(asoc, SCTP_CID_INIT, 0, chunksize);
	if (!retval)
		goto nodata;

	retval->subh.init_hdr =
		sctp_addto_chunk(retval, sizeof(init), &init);
	retval->param_hdr.v =
		sctp_addto_chunk(retval, addrs_len, addrs.v);

	sat.param_hdr.type = SCTP_PARAM_SUPPORTED_ADDRESS_TYPES;
	sat.param_hdr.length = htons(SCTP_SAT_LEN(num_types));
	sctp_addto_chunk(retval, sizeof(sat), &sat);
	sctp_addto_chunk(retval, num_types * sizeof(__u16), &types);

	sctp_addto_chunk(retval, sizeof(ecap_param), &ecap_param);

	if (num_ext) {
		ext_param.param_hdr.type = SCTP_PARAM_SUPPORTED_EXT;
		ext_param.param_hdr.length =
			    htons(sizeof(sctp_supported_ext_param_t) + num_ext);
		sctp_addto_chunk(retval, sizeof(sctp_supported_ext_param_t),
				&ext_param);
		sctp_addto_param(retval, num_ext, extensions);
	}

	if (sctp_prsctp_enable)
		sctp_addto_chunk(retval, sizeof(prsctp_param), &prsctp_param);

	if (sp->adaptation_ind) {
		aiparam.param_hdr.type = SCTP_PARAM_ADAPTATION_LAYER_IND;
		aiparam.param_hdr.length = htons(sizeof(aiparam));
		aiparam.adaptation_ind = htonl(sp->adaptation_ind);
		sctp_addto_chunk(retval, sizeof(aiparam), &aiparam);
	}

	
	if (sctp_auth_enable) {
		sctp_addto_chunk(retval, sizeof(asoc->c.auth_random),
				 asoc->c.auth_random);
		if (auth_hmacs)
			sctp_addto_chunk(retval, ntohs(auth_hmacs->length),
					auth_hmacs);
		if (auth_chunks)
			sctp_addto_chunk(retval, ntohs(auth_chunks->length),
					auth_chunks);
	}
nodata:
	kfree(addrs.v);
	return retval;
}

struct sctp_chunk *sctp_make_init_ack(const struct sctp_association *asoc,
				 const struct sctp_chunk *chunk,
				 gfp_t gfp, int unkparam_len)
{
	sctp_inithdr_t initack;
	struct sctp_chunk *retval;
	union sctp_params addrs;
	struct sctp_sock *sp;
	int addrs_len;
	sctp_cookie_param_t *cookie;
	int cookie_len;
	size_t chunksize;
	sctp_adaptation_ind_param_t aiparam;
	sctp_supported_ext_param_t ext_param;
	int num_ext = 0;
	__u8 extensions[3];
	sctp_paramhdr_t *auth_chunks = NULL,
			*auth_hmacs = NULL,
			*auth_random = NULL;

	retval = NULL;

	
	addrs = sctp_bind_addrs_to_raw(&asoc->base.bind_addr, &addrs_len, gfp);

	initack.init_tag	        = htonl(asoc->c.my_vtag);
	initack.a_rwnd			= htonl(asoc->rwnd);
	initack.num_outbound_streams	= htons(asoc->c.sinit_num_ostreams);
	initack.num_inbound_streams	= htons(asoc->c.sinit_max_instreams);
	initack.initial_tsn		= htonl(asoc->c.initial_tsn);

	cookie = sctp_pack_cookie(asoc->ep, asoc, chunk, &cookie_len,
				  addrs.v, addrs_len);
	if (!cookie)
		goto nomem_cookie;

	sp = sctp_sk(asoc->base.sk);
	chunksize = sizeof(initack) + addrs_len + cookie_len + unkparam_len;

	
	if (asoc->peer.ecn_capable)
		chunksize += sizeof(ecap_param);

	if (asoc->peer.prsctp_capable)
		chunksize += sizeof(prsctp_param);

	if (asoc->peer.asconf_capable) {
		extensions[num_ext] = SCTP_CID_ASCONF;
		extensions[num_ext+1] = SCTP_CID_ASCONF_ACK;
		num_ext += 2;
	}

	if (sp->adaptation_ind)
		chunksize += sizeof(aiparam);

	if (asoc->peer.auth_capable) {
		auth_random = (sctp_paramhdr_t *)asoc->c.auth_random;
		chunksize += ntohs(auth_random->length);

		auth_hmacs = (sctp_paramhdr_t *)asoc->c.auth_hmacs;
		if (auth_hmacs->length)
			chunksize += WORD_ROUND(ntohs(auth_hmacs->length));
		else
			auth_hmacs = NULL;

		auth_chunks = (sctp_paramhdr_t *)asoc->c.auth_chunks;
		if (auth_chunks->length)
			chunksize += WORD_ROUND(ntohs(auth_chunks->length));
		else
			auth_chunks = NULL;

		extensions[num_ext] = SCTP_CID_AUTH;
		num_ext += 1;
	}

	if (num_ext)
		chunksize += WORD_ROUND(sizeof(sctp_supported_ext_param_t) +
					num_ext);

	
	retval = sctp_make_chunk(asoc, SCTP_CID_INIT_ACK, 0, chunksize);
	if (!retval)
		goto nomem_chunk;

	retval->transport = chunk->transport;

	retval->subh.init_hdr =
		sctp_addto_chunk(retval, sizeof(initack), &initack);
	retval->param_hdr.v = sctp_addto_chunk(retval, addrs_len, addrs.v);
	sctp_addto_chunk(retval, cookie_len, cookie);
	if (asoc->peer.ecn_capable)
		sctp_addto_chunk(retval, sizeof(ecap_param), &ecap_param);
	if (num_ext) {
		ext_param.param_hdr.type = SCTP_PARAM_SUPPORTED_EXT;
		ext_param.param_hdr.length =
			    htons(sizeof(sctp_supported_ext_param_t) + num_ext);
		sctp_addto_chunk(retval, sizeof(sctp_supported_ext_param_t),
				 &ext_param);
		sctp_addto_param(retval, num_ext, extensions);
	}
	if (asoc->peer.prsctp_capable)
		sctp_addto_chunk(retval, sizeof(prsctp_param), &prsctp_param);

	if (sp->adaptation_ind) {
		aiparam.param_hdr.type = SCTP_PARAM_ADAPTATION_LAYER_IND;
		aiparam.param_hdr.length = htons(sizeof(aiparam));
		aiparam.adaptation_ind = htonl(sp->adaptation_ind);
		sctp_addto_chunk(retval, sizeof(aiparam), &aiparam);
	}

	if (asoc->peer.auth_capable) {
		sctp_addto_chunk(retval, ntohs(auth_random->length),
				 auth_random);
		if (auth_hmacs)
			sctp_addto_chunk(retval, ntohs(auth_hmacs->length),
					auth_hmacs);
		if (auth_chunks)
			sctp_addto_chunk(retval, ntohs(auth_chunks->length),
					auth_chunks);
	}

	
	retval->asoc = (struct sctp_association *) asoc;

nomem_chunk:
	kfree(cookie);
nomem_cookie:
	kfree(addrs.v);
	return retval;
}

struct sctp_chunk *sctp_make_cookie_echo(const struct sctp_association *asoc,
				    const struct sctp_chunk *chunk)
{
	struct sctp_chunk *retval;
	void *cookie;
	int cookie_len;

	cookie = asoc->peer.cookie;
	cookie_len = asoc->peer.cookie_len;

	
	retval = sctp_make_chunk(asoc, SCTP_CID_COOKIE_ECHO, 0, cookie_len);
	if (!retval)
		goto nodata;
	retval->subh.cookie_hdr =
		sctp_addto_chunk(retval, cookie_len, cookie);

	if (chunk)
		retval->transport = chunk->transport;

nodata:
	return retval;
}

struct sctp_chunk *sctp_make_cookie_ack(const struct sctp_association *asoc,
				   const struct sctp_chunk *chunk)
{
	struct sctp_chunk *retval;

	retval = sctp_make_chunk(asoc, SCTP_CID_COOKIE_ACK, 0, 0);

	if (retval && chunk)
		retval->transport = chunk->transport;

	return retval;
}

struct sctp_chunk *sctp_make_cwr(const struct sctp_association *asoc,
			    const __u32 lowest_tsn,
			    const struct sctp_chunk *chunk)
{
	struct sctp_chunk *retval;
	sctp_cwrhdr_t cwr;

	cwr.lowest_tsn = htonl(lowest_tsn);
	retval = sctp_make_chunk(asoc, SCTP_CID_ECN_CWR, 0,
				 sizeof(sctp_cwrhdr_t));

	if (!retval)
		goto nodata;

	retval->subh.ecn_cwr_hdr =
		sctp_addto_chunk(retval, sizeof(cwr), &cwr);

	if (chunk)
		retval->transport = chunk->transport;

nodata:
	return retval;
}

struct sctp_chunk *sctp_make_ecne(const struct sctp_association *asoc,
			     const __u32 lowest_tsn)
{
	struct sctp_chunk *retval;
	sctp_ecnehdr_t ecne;

	ecne.lowest_tsn = htonl(lowest_tsn);
	retval = sctp_make_chunk(asoc, SCTP_CID_ECN_ECNE, 0,
				 sizeof(sctp_ecnehdr_t));
	if (!retval)
		goto nodata;
	retval->subh.ecne_hdr =
		sctp_addto_chunk(retval, sizeof(ecne), &ecne);

nodata:
	return retval;
}

struct sctp_chunk *sctp_make_datafrag_empty(struct sctp_association *asoc,
				       const struct sctp_sndrcvinfo *sinfo,
				       int data_len, __u8 flags, __u16 ssn)
{
	struct sctp_chunk *retval;
	struct sctp_datahdr dp;
	int chunk_len;

	dp.tsn = 0;
	dp.stream = htons(sinfo->sinfo_stream);
	dp.ppid   = sinfo->sinfo_ppid;

	
	if (sinfo->sinfo_flags & SCTP_UNORDERED) {
		flags |= SCTP_DATA_UNORDERED;
		dp.ssn = 0;
	} else
		dp.ssn = htons(ssn);

	chunk_len = sizeof(dp) + data_len;
	retval = sctp_make_chunk(asoc, SCTP_CID_DATA, flags, chunk_len);
	if (!retval)
		goto nodata;

	retval->subh.data_hdr = sctp_addto_chunk(retval, sizeof(dp), &dp);
	memcpy(&retval->sinfo, sinfo, sizeof(struct sctp_sndrcvinfo));

nodata:
	return retval;
}

struct sctp_chunk *sctp_make_sack(const struct sctp_association *asoc)
{
	struct sctp_chunk *retval;
	struct sctp_sackhdr sack;
	int len;
	__u32 ctsn;
	__u16 num_gabs, num_dup_tsns;
	struct sctp_tsnmap *map = (struct sctp_tsnmap *)&asoc->peer.tsn_map;
	struct sctp_gap_ack_block gabs[SCTP_MAX_GABS];

	memset(gabs, 0, sizeof(gabs));
	ctsn = sctp_tsnmap_get_ctsn(map);
	SCTP_DEBUG_PRINTK("sackCTSNAck sent:  0x%x.\n", ctsn);

	
	num_gabs = sctp_tsnmap_num_gabs(map, gabs);
	num_dup_tsns = sctp_tsnmap_num_dups(map);

	
	sack.cum_tsn_ack	    = htonl(ctsn);
	sack.a_rwnd 		    = htonl(asoc->a_rwnd);
	sack.num_gap_ack_blocks     = htons(num_gabs);
	sack.num_dup_tsns           = htons(num_dup_tsns);

	len = sizeof(sack)
		+ sizeof(struct sctp_gap_ack_block) * num_gabs
		+ sizeof(__u32) * num_dup_tsns;

	
	retval = sctp_make_chunk(asoc, SCTP_CID_SACK, 0, len);
	if (!retval)
		goto nodata;

	retval->transport = asoc->peer.last_data_from;

	retval->subh.sack_hdr =
		sctp_addto_chunk(retval, sizeof(sack), &sack);

	
	if (num_gabs)
		sctp_addto_chunk(retval, sizeof(__u32) * num_gabs,
				 gabs);

	
	if (num_dup_tsns)
		sctp_addto_chunk(retval, sizeof(__u32) * num_dup_tsns,
				 sctp_tsnmap_get_dups(map));

nodata:
	return retval;
}

struct sctp_chunk *sctp_make_shutdown(const struct sctp_association *asoc,
				      const struct sctp_chunk *chunk)
{
	struct sctp_chunk *retval;
	sctp_shutdownhdr_t shut;
	__u32 ctsn;

	ctsn = sctp_tsnmap_get_ctsn(&asoc->peer.tsn_map);
	shut.cum_tsn_ack = htonl(ctsn);

	retval = sctp_make_chunk(asoc, SCTP_CID_SHUTDOWN, 0,
				 sizeof(sctp_shutdownhdr_t));
	if (!retval)
		goto nodata;

	retval->subh.shutdown_hdr =
		sctp_addto_chunk(retval, sizeof(shut), &shut);

	if (chunk)
		retval->transport = chunk->transport;
nodata:
	return retval;
}

struct sctp_chunk *sctp_make_shutdown_ack(const struct sctp_association *asoc,
				     const struct sctp_chunk *chunk)
{
	struct sctp_chunk *retval;

	retval = sctp_make_chunk(asoc, SCTP_CID_SHUTDOWN_ACK, 0, 0);

	if (retval && chunk)
		retval->transport = chunk->transport;

	return retval;
}

struct sctp_chunk *sctp_make_shutdown_complete(
	const struct sctp_association *asoc,
	const struct sctp_chunk *chunk)
{
	struct sctp_chunk *retval;
	__u8 flags = 0;

	flags |= asoc ? 0 : SCTP_CHUNK_FLAG_T;

	retval = sctp_make_chunk(asoc, SCTP_CID_SHUTDOWN_COMPLETE, flags, 0);

	if (retval && chunk)
		retval->transport = chunk->transport;

	return retval;
}

struct sctp_chunk *sctp_make_abort(const struct sctp_association *asoc,
			      const struct sctp_chunk *chunk,
			      const size_t hint)
{
	struct sctp_chunk *retval;
	__u8 flags = 0;

	if (!asoc) {
		if (chunk && chunk->chunk_hdr &&
		    chunk->chunk_hdr->type == SCTP_CID_INIT)
			flags = 0;
		else
			flags = SCTP_CHUNK_FLAG_T;
	}

	retval = sctp_make_chunk(asoc, SCTP_CID_ABORT, flags, hint);

	if (retval && chunk)
		retval->transport = chunk->transport;

	return retval;
}

struct sctp_chunk *sctp_make_abort_no_data(
	const struct sctp_association *asoc,
	const struct sctp_chunk *chunk, __u32 tsn)
{
	struct sctp_chunk *retval;
	__be32 payload;

	retval = sctp_make_abort(asoc, chunk, sizeof(sctp_errhdr_t)
				 + sizeof(tsn));

	if (!retval)
		goto no_mem;

	
	payload = htonl(tsn);
	sctp_init_cause(retval, SCTP_ERROR_NO_DATA, sizeof(payload));
	sctp_addto_chunk(retval, sizeof(payload), (const void *)&payload);

	if (chunk)
		retval->transport = chunk->transport;

no_mem:
	return retval;
}

struct sctp_chunk *sctp_make_abort_user(const struct sctp_association *asoc,
					const struct msghdr *msg,
					size_t paylen)
{
	struct sctp_chunk *retval;
	void *payload = NULL;
	int err;

	retval = sctp_make_abort(asoc, NULL, sizeof(sctp_errhdr_t) + paylen);
	if (!retval)
		goto err_chunk;

	if (paylen) {
		
		payload = kmalloc(paylen, GFP_KERNEL);
		if (!payload)
			goto err_payload;

		err = memcpy_fromiovec(payload, msg->msg_iov, paylen);
		if (err < 0)
			goto err_copy;
	}

	sctp_init_cause(retval, SCTP_ERROR_USER_ABORT, paylen);
	sctp_addto_chunk(retval, paylen, payload);

	if (paylen)
		kfree(payload);

	return retval;

err_copy:
	kfree(payload);
err_payload:
	sctp_chunk_free(retval);
	retval = NULL;
err_chunk:
	return retval;
}

static void *sctp_addto_param(struct sctp_chunk *chunk, int len,
			      const void *data)
{
	void *target;
	int chunklen = ntohs(chunk->chunk_hdr->length);

	target = skb_put(chunk->skb, len);

	if (data)
		memcpy(target, data, len);
	else
		memset(target, 0, len);

	
	chunk->chunk_hdr->length = htons(chunklen + len);
	chunk->chunk_end = skb_tail_pointer(chunk->skb);

	return target;
}

struct sctp_chunk *sctp_make_abort_violation(
	const struct sctp_association *asoc,
	const struct sctp_chunk *chunk,
	const __u8   *payload,
	const size_t paylen)
{
	struct sctp_chunk  *retval;
	struct sctp_paramhdr phdr;

	retval = sctp_make_abort(asoc, chunk, sizeof(sctp_errhdr_t) + paylen
					+ sizeof(sctp_paramhdr_t));
	if (!retval)
		goto end;

	sctp_init_cause(retval, SCTP_ERROR_PROTO_VIOLATION, paylen
					+ sizeof(sctp_paramhdr_t));

	phdr.type = htons(chunk->chunk_hdr->type);
	phdr.length = chunk->chunk_hdr->length;
	sctp_addto_chunk(retval, paylen, payload);
	sctp_addto_param(retval, sizeof(sctp_paramhdr_t), &phdr);

end:
	return retval;
}

struct sctp_chunk *sctp_make_violation_paramlen(
	const struct sctp_association *asoc,
	const struct sctp_chunk *chunk,
	struct sctp_paramhdr *param)
{
	struct sctp_chunk *retval;
	static const char error[] = "The following parameter had invalid length:";
	size_t payload_len = sizeof(error) + sizeof(sctp_errhdr_t) +
				sizeof(sctp_paramhdr_t);

	retval = sctp_make_abort(asoc, chunk, payload_len);
	if (!retval)
		goto nodata;

	sctp_init_cause(retval, SCTP_ERROR_PROTO_VIOLATION,
			sizeof(error) + sizeof(sctp_paramhdr_t));
	sctp_addto_chunk(retval, sizeof(error), error);
	sctp_addto_param(retval, sizeof(sctp_paramhdr_t), param);

nodata:
	return retval;
}

struct sctp_chunk *sctp_make_heartbeat(const struct sctp_association *asoc,
				  const struct sctp_transport *transport)
{
	struct sctp_chunk *retval;
	sctp_sender_hb_info_t hbinfo;

	retval = sctp_make_chunk(asoc, SCTP_CID_HEARTBEAT, 0, sizeof(hbinfo));

	if (!retval)
		goto nodata;

	hbinfo.param_hdr.type = SCTP_PARAM_HEARTBEAT_INFO;
	hbinfo.param_hdr.length = htons(sizeof(sctp_sender_hb_info_t));
	hbinfo.daddr = transport->ipaddr;
	hbinfo.sent_at = jiffies;
	hbinfo.hb_nonce = transport->hb_nonce;

	retval->transport = (struct sctp_transport *) transport;
	retval->subh.hbs_hdr = sctp_addto_chunk(retval, sizeof(hbinfo),
						&hbinfo);

nodata:
	return retval;
}

struct sctp_chunk *sctp_make_heartbeat_ack(const struct sctp_association *asoc,
				      const struct sctp_chunk *chunk,
				      const void *payload, const size_t paylen)
{
	struct sctp_chunk *retval;

	retval  = sctp_make_chunk(asoc, SCTP_CID_HEARTBEAT_ACK, 0, paylen);
	if (!retval)
		goto nodata;

	retval->subh.hbs_hdr = sctp_addto_chunk(retval, paylen, payload);

	if (chunk)
		retval->transport = chunk->transport;

nodata:
	return retval;
}

static struct sctp_chunk *sctp_make_op_error_space(
	const struct sctp_association *asoc,
	const struct sctp_chunk *chunk,
	size_t size)
{
	struct sctp_chunk *retval;

	retval = sctp_make_chunk(asoc, SCTP_CID_ERROR, 0,
				 sizeof(sctp_errhdr_t) + size);
	if (!retval)
		goto nodata;

	if (chunk)
		retval->transport = chunk->transport;

nodata:
	return retval;
}

static inline struct sctp_chunk *sctp_make_op_error_fixed(
	const struct sctp_association *asoc,
	const struct sctp_chunk *chunk)
{
	size_t size = asoc ? asoc->pathmtu : 0;

	if (!size)
		size = SCTP_DEFAULT_MAXSEGMENT;

	return sctp_make_op_error_space(asoc, chunk, size);
}

struct sctp_chunk *sctp_make_op_error(const struct sctp_association *asoc,
				 const struct sctp_chunk *chunk,
				 __be16 cause_code, const void *payload,
				 size_t paylen, size_t reserve_tail)
{
	struct sctp_chunk *retval;

	retval = sctp_make_op_error_space(asoc, chunk, paylen + reserve_tail);
	if (!retval)
		goto nodata;

	sctp_init_cause(retval, cause_code, paylen + reserve_tail);
	sctp_addto_chunk(retval, paylen, payload);
	if (reserve_tail)
		sctp_addto_param(retval, reserve_tail, NULL);

nodata:
	return retval;
}

struct sctp_chunk *sctp_make_auth(const struct sctp_association *asoc)
{
	struct sctp_chunk *retval;
	struct sctp_hmac *hmac_desc;
	struct sctp_authhdr auth_hdr;
	__u8 *hmac;

	
	hmac_desc = sctp_auth_asoc_get_hmac(asoc);
	if (unlikely(!hmac_desc))
		return NULL;

	retval = sctp_make_chunk(asoc, SCTP_CID_AUTH, 0,
			hmac_desc->hmac_len + sizeof(sctp_authhdr_t));
	if (!retval)
		return NULL;

	auth_hdr.hmac_id = htons(hmac_desc->hmac_id);
	auth_hdr.shkey_id = htons(asoc->active_key_id);

	retval->subh.auth_hdr = sctp_addto_chunk(retval, sizeof(sctp_authhdr_t),
						&auth_hdr);

	hmac = skb_put(retval->skb, hmac_desc->hmac_len);
	memset(hmac, 0, hmac_desc->hmac_len);

	
	retval->chunk_hdr->length =
		htons(ntohs(retval->chunk_hdr->length) + hmac_desc->hmac_len);
	retval->chunk_end = skb_tail_pointer(retval->skb);

	return retval;
}



struct sctp_chunk *sctp_chunkify(struct sk_buff *skb,
			    const struct sctp_association *asoc,
			    struct sock *sk)
{
	struct sctp_chunk *retval;

	retval = kmem_cache_zalloc(sctp_chunk_cachep, GFP_ATOMIC);

	if (!retval)
		goto nodata;

	if (!sk) {
		SCTP_DEBUG_PRINTK("chunkifying skb %p w/o an sk\n", skb);
	}

	INIT_LIST_HEAD(&retval->list);
	retval->skb		= skb;
	retval->asoc		= (struct sctp_association *)asoc;
	retval->has_tsn		= 0;
	retval->has_ssn         = 0;
	retval->rtt_in_progress	= 0;
	retval->sent_at		= 0;
	retval->singleton	= 1;
	retval->end_of_packet	= 0;
	retval->ecn_ce_done	= 0;
	retval->pdiscard	= 0;

	retval->tsn_missing_report = 0;
	retval->tsn_gap_acked = 0;
	retval->fast_retransmit = SCTP_CAN_FRTX;

	retval->msg = NULL;

	
	INIT_LIST_HEAD(&retval->transmitted_list);
	INIT_LIST_HEAD(&retval->frag_list);
	SCTP_DBG_OBJCNT_INC(chunk);
	atomic_set(&retval->refcnt, 1);

nodata:
	return retval;
}

void sctp_init_addrs(struct sctp_chunk *chunk, union sctp_addr *src,
		     union sctp_addr *dest)
{
	memcpy(&chunk->source, src, sizeof(union sctp_addr));
	memcpy(&chunk->dest, dest, sizeof(union sctp_addr));
}

const union sctp_addr *sctp_source(const struct sctp_chunk *chunk)
{
	
	if (chunk->transport) {
		return &chunk->transport->ipaddr;
	} else {
		
		return &chunk->source;
	}
}

SCTP_STATIC
struct sctp_chunk *sctp_make_chunk(const struct sctp_association *asoc,
				   __u8 type, __u8 flags, int paylen)
{
	struct sctp_chunk *retval;
	sctp_chunkhdr_t *chunk_hdr;
	struct sk_buff *skb;
	struct sock *sk;

	
	skb = alloc_skb(WORD_ROUND(sizeof(sctp_chunkhdr_t) + paylen),
			GFP_ATOMIC);
	if (!skb)
		goto nodata;

	
	chunk_hdr = (sctp_chunkhdr_t *)skb_put(skb, sizeof(sctp_chunkhdr_t));
	chunk_hdr->type	  = type;
	chunk_hdr->flags  = flags;
	chunk_hdr->length = htons(sizeof(sctp_chunkhdr_t));

	sk = asoc ? asoc->base.sk : NULL;
	retval = sctp_chunkify(skb, asoc, sk);
	if (!retval) {
		kfree_skb(skb);
		goto nodata;
	}

	retval->chunk_hdr = chunk_hdr;
	retval->chunk_end = ((__u8 *)chunk_hdr) + sizeof(struct sctp_chunkhdr);

	
	if (sctp_auth_send_cid(type, asoc))
		retval->auth = 1;

	
	skb->sk = sk;

	return retval;
nodata:
	return NULL;
}


static void sctp_chunk_destroy(struct sctp_chunk *chunk)
{
	BUG_ON(!list_empty(&chunk->list));
	list_del_init(&chunk->transmitted_list);

	
	dev_kfree_skb(chunk->skb);

	SCTP_DBG_OBJCNT_DEC(chunk);
	kmem_cache_free(sctp_chunk_cachep, chunk);
}

void sctp_chunk_free(struct sctp_chunk *chunk)
{
	
	if (chunk->msg)
		sctp_datamsg_put(chunk->msg);

	sctp_chunk_put(chunk);
}

void sctp_chunk_hold(struct sctp_chunk *ch)
{
	atomic_inc(&ch->refcnt);
}

void sctp_chunk_put(struct sctp_chunk *ch)
{
	if (atomic_dec_and_test(&ch->refcnt))
		sctp_chunk_destroy(ch);
}

void *sctp_addto_chunk(struct sctp_chunk *chunk, int len, const void *data)
{
	void *target;
	void *padding;
	int chunklen = ntohs(chunk->chunk_hdr->length);
	int padlen = WORD_ROUND(chunklen) - chunklen;

	padding = skb_put(chunk->skb, padlen);
	target = skb_put(chunk->skb, len);

	memset(padding, 0, padlen);
	memcpy(target, data, len);

	
	chunk->chunk_hdr->length = htons(chunklen + padlen + len);
	chunk->chunk_end = skb_tail_pointer(chunk->skb);

	return target;
}

void *sctp_addto_chunk_fixed(struct sctp_chunk *chunk,
			     int len, const void *data)
{
	if (skb_tailroom(chunk->skb) >= len)
		return sctp_addto_chunk(chunk, len, data);
	else
		return NULL;
}

int sctp_user_addto_chunk(struct sctp_chunk *chunk, int off, int len,
			  struct iovec *data)
{
	__u8 *target;
	int err = 0;

	
	target = skb_put(chunk->skb, len);

	
	if ((err = memcpy_fromiovecend(target, data, off, len)))
		goto out;

	
	chunk->chunk_hdr->length =
		htons(ntohs(chunk->chunk_hdr->length) + len);
	chunk->chunk_end = skb_tail_pointer(chunk->skb);

out:
	return err;
}

void sctp_chunk_assign_ssn(struct sctp_chunk *chunk)
{
	struct sctp_datamsg *msg;
	struct sctp_chunk *lchunk;
	struct sctp_stream *stream;
	__u16 ssn;
	__u16 sid;

	if (chunk->has_ssn)
		return;

	
	sid = ntohs(chunk->subh.data_hdr->stream);
	stream = &chunk->asoc->ssnmap->out;

	msg = chunk->msg;
	list_for_each_entry(lchunk, &msg->chunks, frag_list) {
		if (lchunk->chunk_hdr->flags & SCTP_DATA_UNORDERED) {
			ssn = 0;
		} else {
			if (lchunk->chunk_hdr->flags & SCTP_DATA_LAST_FRAG)
				ssn = sctp_ssn_next(stream, sid);
			else
				ssn = sctp_ssn_peek(stream, sid);
		}

		lchunk->subh.data_hdr->ssn = htons(ssn);
		lchunk->has_ssn = 1;
	}
}

void sctp_chunk_assign_tsn(struct sctp_chunk *chunk)
{
	if (!chunk->has_tsn) {
		chunk->subh.data_hdr->tsn =
			htonl(sctp_association_get_next_tsn(chunk->asoc));
		chunk->has_tsn = 1;
	}
}

struct sctp_association *sctp_make_temp_asoc(const struct sctp_endpoint *ep,
					struct sctp_chunk *chunk,
					gfp_t gfp)
{
	struct sctp_association *asoc;
	struct sk_buff *skb;
	sctp_scope_t scope;
	struct sctp_af *af;

	
	scope = sctp_scope(sctp_source(chunk));
	asoc = sctp_association_new(ep, ep->base.sk, scope, gfp);
	if (!asoc)
		goto nodata;
	asoc->temp = 1;
	skb = chunk->skb;
	
	af = sctp_get_af_specific(ipver2af(ip_hdr(skb)->version));
	if (unlikely(!af))
		goto fail;
	af->from_skb(&asoc->c.peer_addr, skb, 1);
nodata:
	return asoc;

fail:
	sctp_association_free(asoc);
	return NULL;
}

static sctp_cookie_param_t *sctp_pack_cookie(const struct sctp_endpoint *ep,
				      const struct sctp_association *asoc,
				      const struct sctp_chunk *init_chunk,
				      int *cookie_len,
				      const __u8 *raw_addrs, int addrs_len)
{
	sctp_cookie_param_t *retval;
	struct sctp_signed_cookie *cookie;
	struct scatterlist sg;
	int headersize, bodysize;
	unsigned int keylen;
	char *key;

	headersize = sizeof(sctp_paramhdr_t) +
		     (sizeof(struct sctp_signed_cookie) -
		      sizeof(struct sctp_cookie));
	bodysize = sizeof(struct sctp_cookie)
		+ ntohs(init_chunk->chunk_hdr->length) + addrs_len;

	if (bodysize % SCTP_COOKIE_MULTIPLE)
		bodysize += SCTP_COOKIE_MULTIPLE
			- (bodysize % SCTP_COOKIE_MULTIPLE);
	*cookie_len = headersize + bodysize;

	retval = kzalloc(*cookie_len, GFP_ATOMIC);
	if (!retval)
		goto nodata;

	cookie = (struct sctp_signed_cookie *) retval->body;

	
	retval->p.type = SCTP_PARAM_STATE_COOKIE;
	retval->p.length = htons(*cookie_len);

	
	cookie->c = asoc->c;
	
	cookie->c.raw_addr_list_len = addrs_len;

	
	cookie->c.prsctp_capable = asoc->peer.prsctp_capable;

	
	cookie->c.adaptation_ind = asoc->peer.adaptation_ind;

	
	do_gettimeofday(&cookie->c.expiration);
	TIMEVAL_ADD(asoc->cookie_life, cookie->c.expiration);

	
	memcpy(&cookie->c.peer_init[0], init_chunk->chunk_hdr,
	       ntohs(init_chunk->chunk_hdr->length));

	
	memcpy((__u8 *)&cookie->c.peer_init[0] +
	       ntohs(init_chunk->chunk_hdr->length), raw_addrs, addrs_len);

	if (sctp_sk(ep->base.sk)->hmac) {
		struct hash_desc desc;

		
		sg_init_one(&sg, &cookie->c, bodysize);
		keylen = SCTP_SECRET_SIZE;
		key = (char *)ep->secret_key[ep->current_key];
		desc.tfm = sctp_sk(ep->base.sk)->hmac;
		desc.flags = 0;

		if (crypto_hash_setkey(desc.tfm, key, keylen) ||
		    crypto_hash_digest(&desc, &sg, bodysize, cookie->signature))
			goto free_cookie;
	}

	return retval;

free_cookie:
	kfree(retval);
nodata:
	*cookie_len = 0;
	return NULL;
}

struct sctp_association *sctp_unpack_cookie(
	const struct sctp_endpoint *ep,
	const struct sctp_association *asoc,
	struct sctp_chunk *chunk, gfp_t gfp,
	int *error, struct sctp_chunk **errp)
{
	struct sctp_association *retval = NULL;
	struct sctp_signed_cookie *cookie;
	struct sctp_cookie *bear_cookie;
	int headersize, bodysize, fixed_size;
	__u8 *digest = ep->digest;
	struct scatterlist sg;
	unsigned int keylen, len;
	char *key;
	sctp_scope_t scope;
	struct sk_buff *skb = chunk->skb;
	struct timeval tv;
	struct hash_desc desc;

	headersize = sizeof(sctp_chunkhdr_t) +
		     (sizeof(struct sctp_signed_cookie) -
		      sizeof(struct sctp_cookie));
	bodysize = ntohs(chunk->chunk_hdr->length) - headersize;
	fixed_size = headersize + sizeof(struct sctp_cookie);

	len = ntohs(chunk->chunk_hdr->length);
	if (len < fixed_size + sizeof(struct sctp_chunkhdr))
		goto malformed;

	
	if (bodysize % SCTP_COOKIE_MULTIPLE)
		goto malformed;

	
	cookie = chunk->subh.cookie_hdr;
	bear_cookie = &cookie->c;

	if (!sctp_sk(ep->base.sk)->hmac)
		goto no_hmac;

	
	keylen = SCTP_SECRET_SIZE;
	sg_init_one(&sg, bear_cookie, bodysize);
	key = (char *)ep->secret_key[ep->current_key];
	desc.tfm = sctp_sk(ep->base.sk)->hmac;
	desc.flags = 0;

	memset(digest, 0x00, SCTP_SIGNATURE_SIZE);
	if (crypto_hash_setkey(desc.tfm, key, keylen) ||
	    crypto_hash_digest(&desc, &sg, bodysize, digest)) {
		*error = -SCTP_IERROR_NOMEM;
		goto fail;
	}

	if (memcmp(digest, cookie->signature, SCTP_SIGNATURE_SIZE)) {
		
		key = (char *)ep->secret_key[ep->last_key];
		memset(digest, 0x00, SCTP_SIGNATURE_SIZE);
		if (crypto_hash_setkey(desc.tfm, key, keylen) ||
		    crypto_hash_digest(&desc, &sg, bodysize, digest)) {
			*error = -SCTP_IERROR_NOMEM;
			goto fail;
		}

		if (memcmp(digest, cookie->signature, SCTP_SIGNATURE_SIZE)) {
			
			*error = -SCTP_IERROR_BAD_SIG;
			goto fail;
		}
	}

no_hmac:
	if (ntohl(chunk->sctp_hdr->vtag) != bear_cookie->my_vtag) {
		*error = -SCTP_IERROR_BAD_TAG;
		goto fail;
	}

	if (chunk->sctp_hdr->source != bear_cookie->peer_addr.v4.sin_port ||
	    ntohs(chunk->sctp_hdr->dest) != bear_cookie->my_port) {
		*error = -SCTP_IERROR_BAD_PORTS;
		goto fail;
	}

	if (sock_flag(ep->base.sk, SOCK_TIMESTAMP))
		skb_get_timestamp(skb, &tv);
	else
		do_gettimeofday(&tv);

	if (!asoc && tv_lt(bear_cookie->expiration, tv)) {
		len = ntohs(chunk->chunk_hdr->length);
		*errp = sctp_make_op_error_space(asoc, chunk, len);
		if (*errp) {
			suseconds_t usecs = (tv.tv_sec -
				bear_cookie->expiration.tv_sec) * 1000000L +
				tv.tv_usec - bear_cookie->expiration.tv_usec;
			__be32 n = htonl(usecs);

			sctp_init_cause(*errp, SCTP_ERROR_STALE_COOKIE,
					sizeof(n));
			sctp_addto_chunk(*errp, sizeof(n), &n);
			*error = -SCTP_IERROR_STALE_COOKIE;
		} else
			*error = -SCTP_IERROR_NOMEM;

		goto fail;
	}

	
	scope = sctp_scope(sctp_source(chunk));
	retval = sctp_association_new(ep, ep->base.sk, scope, gfp);
	if (!retval) {
		*error = -SCTP_IERROR_NOMEM;
		goto fail;
	}

	
	retval->peer.port = ntohs(chunk->sctp_hdr->source);

	
	memcpy(&retval->c, bear_cookie, sizeof(*bear_cookie));

	if (sctp_assoc_set_bind_addr_from_cookie(retval, bear_cookie,
						 GFP_ATOMIC) < 0) {
		*error = -SCTP_IERROR_NOMEM;
		goto fail;
	}

	
	if (list_empty(&retval->base.bind_addr.address_list)) {
		sctp_add_bind_addr(&retval->base.bind_addr, &chunk->dest,
				SCTP_ADDR_SRC, GFP_ATOMIC);
	}

	retval->next_tsn = retval->c.initial_tsn;
	retval->ctsn_ack_point = retval->next_tsn - 1;
	retval->addip_serial = retval->c.initial_tsn;
	retval->adv_peer_ack_point = retval->ctsn_ack_point;
	retval->peer.prsctp_capable = retval->c.prsctp_capable;
	retval->peer.adaptation_ind = retval->c.adaptation_ind;

	
	return retval;

fail:
	if (retval)
		sctp_association_free(retval);

	return NULL;

malformed:
	*error = -SCTP_IERROR_MALFORMED;
	goto fail;
}


struct __sctp_missing {
	__be32 num_missing;
	__be16 type;
}  __packed;

static int sctp_process_missing_param(const struct sctp_association *asoc,
				      sctp_param_t paramtype,
				      struct sctp_chunk *chunk,
				      struct sctp_chunk **errp)
{
	struct __sctp_missing report;
	__u16 len;

	len = WORD_ROUND(sizeof(report));

	if (!*errp)
		*errp = sctp_make_op_error_space(asoc, chunk, len);

	if (*errp) {
		report.num_missing = htonl(1);
		report.type = paramtype;
		sctp_init_cause(*errp, SCTP_ERROR_MISS_PARAM,
				sizeof(report));
		sctp_addto_chunk(*errp, sizeof(report), &report);
	}

	
	return 0;
}

static int sctp_process_inv_mandatory(const struct sctp_association *asoc,
				      struct sctp_chunk *chunk,
				      struct sctp_chunk **errp)
{
	

	if (!*errp)
		*errp = sctp_make_op_error_space(asoc, chunk, 0);

	if (*errp)
		sctp_init_cause(*errp, SCTP_ERROR_INV_PARAM, 0);

	
	return 0;
}

static int sctp_process_inv_paramlength(const struct sctp_association *asoc,
					struct sctp_paramhdr *param,
					const struct sctp_chunk *chunk,
					struct sctp_chunk **errp)
{
	if (*errp)
		sctp_chunk_free(*errp);

	
	*errp = sctp_make_violation_paramlen(asoc, chunk, param);

	return 0;
}


static int sctp_process_hn_param(const struct sctp_association *asoc,
				 union sctp_params param,
				 struct sctp_chunk *chunk,
				 struct sctp_chunk **errp)
{
	__u16 len = ntohs(param.p->length);

	if (*errp)
		sctp_chunk_free(*errp);

	*errp = sctp_make_op_error_space(asoc, chunk, len);

	if (*errp) {
		sctp_init_cause(*errp, SCTP_ERROR_DNS_FAILED, len);
		sctp_addto_chunk(*errp, len, param.v);
	}

	
	return 0;
}

static int sctp_verify_ext_param(union sctp_params param)
{
	__u16 num_ext = ntohs(param.p->length) - sizeof(sctp_paramhdr_t);
	int have_auth = 0;
	int have_asconf = 0;
	int i;

	for (i = 0; i < num_ext; i++) {
		switch (param.ext->chunks[i]) {
		    case SCTP_CID_AUTH:
			    have_auth = 1;
			    break;
		    case SCTP_CID_ASCONF:
		    case SCTP_CID_ASCONF_ACK:
			    have_asconf = 1;
			    break;
		}
	}

	if (sctp_addip_noauth)
		return 1;

	if (sctp_addip_enable && !have_auth && have_asconf)
		return 0;

	return 1;
}

static void sctp_process_ext_param(struct sctp_association *asoc,
				    union sctp_params param)
{
	__u16 num_ext = ntohs(param.p->length) - sizeof(sctp_paramhdr_t);
	int i;

	for (i = 0; i < num_ext; i++) {
		switch (param.ext->chunks[i]) {
		    case SCTP_CID_FWD_TSN:
			    if (sctp_prsctp_enable &&
				!asoc->peer.prsctp_capable)
				    asoc->peer.prsctp_capable = 1;
			    break;
		    case SCTP_CID_AUTH:
			    if (sctp_auth_enable)
				    asoc->peer.auth_capable = 1;
			    break;
		    case SCTP_CID_ASCONF:
		    case SCTP_CID_ASCONF_ACK:
			    if (sctp_addip_enable)
				    asoc->peer.asconf_capable = 1;
			    break;
		    default:
			    break;
		}
	}
}

static sctp_ierror_t sctp_process_unk_param(const struct sctp_association *asoc,
					    union sctp_params param,
					    struct sctp_chunk *chunk,
					    struct sctp_chunk **errp)
{
	int retval = SCTP_IERROR_NO_ERROR;

	switch (param.p->type & SCTP_PARAM_ACTION_MASK) {
	case SCTP_PARAM_ACTION_DISCARD:
		retval =  SCTP_IERROR_ERROR;
		break;
	case SCTP_PARAM_ACTION_SKIP:
		break;
	case SCTP_PARAM_ACTION_DISCARD_ERR:
		retval =  SCTP_IERROR_ERROR;
		
	case SCTP_PARAM_ACTION_SKIP_ERR:
		if (NULL == *errp)
			*errp = sctp_make_op_error_fixed(asoc, chunk);

		if (*errp) {
			if (!sctp_init_cause_fixed(*errp, SCTP_ERROR_UNKNOWN_PARAM,
					WORD_ROUND(ntohs(param.p->length))))
				sctp_addto_chunk_fixed(*errp,
						WORD_ROUND(ntohs(param.p->length)),
						param.v);
		} else {
			retval = SCTP_IERROR_NOMEM;
		}
		break;
	default:
		break;
	}

	return retval;
}

static sctp_ierror_t sctp_verify_param(const struct sctp_association *asoc,
					union sctp_params param,
					sctp_cid_t cid,
					struct sctp_chunk *chunk,
					struct sctp_chunk **err_chunk)
{
	struct sctp_hmac_algo_param *hmacs;
	int retval = SCTP_IERROR_NO_ERROR;
	__u16 n_elt, id = 0;
	int i;


	switch (param.p->type) {
	case SCTP_PARAM_IPV4_ADDRESS:
	case SCTP_PARAM_IPV6_ADDRESS:
	case SCTP_PARAM_COOKIE_PRESERVATIVE:
	case SCTP_PARAM_SUPPORTED_ADDRESS_TYPES:
	case SCTP_PARAM_STATE_COOKIE:
	case SCTP_PARAM_HEARTBEAT_INFO:
	case SCTP_PARAM_UNRECOGNIZED_PARAMETERS:
	case SCTP_PARAM_ECN_CAPABLE:
	case SCTP_PARAM_ADAPTATION_LAYER_IND:
		break;

	case SCTP_PARAM_SUPPORTED_EXT:
		if (!sctp_verify_ext_param(param))
			return SCTP_IERROR_ABORT;
		break;

	case SCTP_PARAM_SET_PRIMARY:
		if (sctp_addip_enable)
			break;
		goto fallthrough;

	case SCTP_PARAM_HOST_NAME_ADDRESS:
		
		sctp_process_hn_param(asoc, param, chunk, err_chunk);
		retval = SCTP_IERROR_ABORT;
		break;

	case SCTP_PARAM_FWD_TSN_SUPPORT:
		if (sctp_prsctp_enable)
			break;
		goto fallthrough;

	case SCTP_PARAM_RANDOM:
		if (!sctp_auth_enable)
			goto fallthrough;

		if (SCTP_AUTH_RANDOM_LENGTH !=
			ntohs(param.p->length) - sizeof(sctp_paramhdr_t)) {
			sctp_process_inv_paramlength(asoc, param.p,
							chunk, err_chunk);
			retval = SCTP_IERROR_ABORT;
		}
		break;

	case SCTP_PARAM_CHUNKS:
		if (!sctp_auth_enable)
			goto fallthrough;

		if (260 < ntohs(param.p->length)) {
			sctp_process_inv_paramlength(asoc, param.p,
						     chunk, err_chunk);
			retval = SCTP_IERROR_ABORT;
		}
		break;

	case SCTP_PARAM_HMAC_ALGO:
		if (!sctp_auth_enable)
			goto fallthrough;

		hmacs = (struct sctp_hmac_algo_param *)param.p;
		n_elt = (ntohs(param.p->length) - sizeof(sctp_paramhdr_t)) >> 1;

		for (i = 0; i < n_elt; i++) {
			id = ntohs(hmacs->hmac_ids[i]);

			if (id == SCTP_AUTH_HMAC_ID_SHA1)
				break;
		}

		if (id != SCTP_AUTH_HMAC_ID_SHA1) {
			sctp_process_inv_paramlength(asoc, param.p, chunk,
						     err_chunk);
			retval = SCTP_IERROR_ABORT;
		}
		break;
fallthrough:
	default:
		SCTP_DEBUG_PRINTK("Unrecognized param: %d for chunk %d.\n",
				ntohs(param.p->type), cid);
		retval = sctp_process_unk_param(asoc, param, chunk, err_chunk);
		break;
	}
	return retval;
}

int sctp_verify_init(const struct sctp_association *asoc,
		     sctp_cid_t cid,
		     sctp_init_chunk_t *peer_init,
		     struct sctp_chunk *chunk,
		     struct sctp_chunk **errp)
{
	union sctp_params param;
	int has_cookie = 0;
	int result;

	
	if ((0 == peer_init->init_hdr.num_outbound_streams) ||
	    (0 == peer_init->init_hdr.num_inbound_streams) ||
	    (0 == peer_init->init_hdr.init_tag) ||
	    (SCTP_DEFAULT_MINWINDOW > ntohl(peer_init->init_hdr.a_rwnd))) {

		return sctp_process_inv_mandatory(asoc, chunk, errp);
	}

	
	sctp_walk_params(param, peer_init, init_hdr.params) {

		if (SCTP_PARAM_STATE_COOKIE == param.p->type)
			has_cookie = 1;

	} 

	if (param.v != (void*)chunk->chunk_end)
		return sctp_process_inv_paramlength(asoc, param.p, chunk, errp);

	if ((SCTP_CID_INIT_ACK == cid) && !has_cookie)
		return sctp_process_missing_param(asoc, SCTP_PARAM_STATE_COOKIE,
						  chunk, errp);

	
	sctp_walk_params(param, peer_init, init_hdr.params) {

		result = sctp_verify_param(asoc, param, cid, chunk, errp);
		switch (result) {
		    case SCTP_IERROR_ABORT:
		    case SCTP_IERROR_NOMEM:
				return 0;
		    case SCTP_IERROR_ERROR:
				return 1;
		    case SCTP_IERROR_NO_ERROR:
		    default:
				break;
		}

	} 

	return 1;
}

int sctp_process_init(struct sctp_association *asoc, struct sctp_chunk *chunk,
		      const union sctp_addr *peer_addr,
		      sctp_init_chunk_t *peer_init, gfp_t gfp)
{
	union sctp_params param;
	struct sctp_transport *transport;
	struct list_head *pos, *temp;
	struct sctp_af *af;
	union sctp_addr addr;
	char *cookie;
	int src_match = 0;


	if(!sctp_assoc_add_peer(asoc, peer_addr, gfp, SCTP_ACTIVE))
		goto nomem;

	if (sctp_cmp_addr_exact(sctp_source(chunk), peer_addr))
		src_match = 1;

	
	sctp_walk_params(param, peer_init, init_hdr.params) {
		if (!src_match && (param.p->type == SCTP_PARAM_IPV4_ADDRESS ||
		    param.p->type == SCTP_PARAM_IPV6_ADDRESS)) {
			af = sctp_get_af_specific(param_type2af(param.p->type));
			af->from_addr_param(&addr, param.addr,
					    chunk->sctp_hdr->source, 0);
			if (sctp_cmp_addr_exact(sctp_source(chunk), &addr))
				src_match = 1;
		}

		if (!sctp_process_param(asoc, param, peer_addr, gfp))
			goto clean_up;
	}

	
	if (!src_match)
		goto clean_up;

	if (asoc->peer.auth_capable && (!asoc->peer.peer_random ||
					!asoc->peer.peer_hmacs))
		asoc->peer.auth_capable = 0;

	if (!sctp_addip_noauth &&
	     (asoc->peer.asconf_capable && !asoc->peer.auth_capable)) {
		asoc->peer.addip_disabled_mask |= (SCTP_PARAM_ADD_IP |
						  SCTP_PARAM_DEL_IP |
						  SCTP_PARAM_SET_PRIMARY);
		asoc->peer.asconf_capable = 0;
		goto clean_up;
	}

	
	list_for_each_safe(pos, temp, &asoc->peer.transport_addr_list) {
		transport = list_entry(pos, struct sctp_transport, transports);
		if (transport->state == SCTP_UNKNOWN) {
			sctp_assoc_rm_peer(asoc, transport);
		}
	}

	asoc->peer.i.init_tag =
		ntohl(peer_init->init_hdr.init_tag);
	asoc->peer.i.a_rwnd =
		ntohl(peer_init->init_hdr.a_rwnd);
	asoc->peer.i.num_outbound_streams =
		ntohs(peer_init->init_hdr.num_outbound_streams);
	asoc->peer.i.num_inbound_streams =
		ntohs(peer_init->init_hdr.num_inbound_streams);
	asoc->peer.i.initial_tsn =
		ntohl(peer_init->init_hdr.initial_tsn);

	if (asoc->c.sinit_num_ostreams  >
	    ntohs(peer_init->init_hdr.num_inbound_streams)) {
		asoc->c.sinit_num_ostreams =
			ntohs(peer_init->init_hdr.num_inbound_streams);
	}

	if (asoc->c.sinit_max_instreams >
	    ntohs(peer_init->init_hdr.num_outbound_streams)) {
		asoc->c.sinit_max_instreams =
			ntohs(peer_init->init_hdr.num_outbound_streams);
	}

	
	asoc->c.peer_vtag = asoc->peer.i.init_tag;

	
	asoc->peer.rwnd = asoc->peer.i.a_rwnd;

	
	cookie = asoc->peer.cookie;
	if (cookie) {
		asoc->peer.cookie = kmemdup(cookie, asoc->peer.cookie_len, gfp);
		if (!asoc->peer.cookie)
			goto clean_up;
	}

	list_for_each_entry(transport, &asoc->peer.transport_addr_list,
			transports) {
		transport->ssthresh = asoc->peer.i.a_rwnd;
	}

	
	if (!sctp_tsnmap_init(&asoc->peer.tsn_map, SCTP_TSN_MAP_INITIAL,
				asoc->peer.i.initial_tsn, gfp))
		goto clean_up;


	if (!asoc->temp) {
		int error;

		asoc->ssnmap = sctp_ssnmap_new(asoc->c.sinit_max_instreams,
					       asoc->c.sinit_num_ostreams, gfp);
		if (!asoc->ssnmap)
			goto clean_up;

		error = sctp_assoc_set_id(asoc, gfp);
		if (error)
			goto clean_up;
	}

	asoc->peer.addip_serial = asoc->peer.i.initial_tsn - 1;
	return 1;

clean_up:
	
	list_for_each_safe(pos, temp, &asoc->peer.transport_addr_list) {
		transport = list_entry(pos, struct sctp_transport, transports);
		if (transport->state != SCTP_ACTIVE)
			sctp_assoc_rm_peer(asoc, transport);
	}

nomem:
	return 0;
}


static int sctp_process_param(struct sctp_association *asoc,
			      union sctp_params param,
			      const union sctp_addr *peer_addr,
			      gfp_t gfp)
{
	union sctp_addr addr;
	int i;
	__u16 sat;
	int retval = 1;
	sctp_scope_t scope;
	time_t stale;
	struct sctp_af *af;
	union sctp_addr_param *addr_param;
	struct sctp_transport *t;

	switch (param.p->type) {
	case SCTP_PARAM_IPV6_ADDRESS:
		if (PF_INET6 != asoc->base.sk->sk_family)
			break;
		goto do_addr_param;

	case SCTP_PARAM_IPV4_ADDRESS:
		
		if (ipv6_only_sock(asoc->base.sk))
			break;
do_addr_param:
		af = sctp_get_af_specific(param_type2af(param.p->type));
		af->from_addr_param(&addr, param.addr, htons(asoc->peer.port), 0);
		scope = sctp_scope(peer_addr);
		if (sctp_in_scope(&addr, scope))
			if (!sctp_assoc_add_peer(asoc, &addr, gfp, SCTP_UNCONFIRMED))
				return 0;
		break;

	case SCTP_PARAM_COOKIE_PRESERVATIVE:
		if (!sctp_cookie_preserve_enable)
			break;

		stale = ntohl(param.life->lifespan_increment);

		asoc->cookie_life.tv_sec += stale / 1000;
		asoc->cookie_life.tv_usec += (stale % 1000) * 1000;
		break;

	case SCTP_PARAM_HOST_NAME_ADDRESS:
		SCTP_DEBUG_PRINTK("unimplemented SCTP_HOST_NAME_ADDRESS\n");
		break;

	case SCTP_PARAM_SUPPORTED_ADDRESS_TYPES:
		asoc->peer.ipv4_address = 0;
		asoc->peer.ipv6_address = 0;

		if (peer_addr->sa.sa_family == AF_INET6)
			asoc->peer.ipv6_address = 1;
		else if (peer_addr->sa.sa_family == AF_INET)
			asoc->peer.ipv4_address = 1;

		
		sat = ntohs(param.p->length) - sizeof(sctp_paramhdr_t);
		if (sat)
			sat /= sizeof(__u16);

		for (i = 0; i < sat; ++i) {
			switch (param.sat->types[i]) {
			case SCTP_PARAM_IPV4_ADDRESS:
				asoc->peer.ipv4_address = 1;
				break;

			case SCTP_PARAM_IPV6_ADDRESS:
				if (PF_INET6 == asoc->base.sk->sk_family)
					asoc->peer.ipv6_address = 1;
				break;

			case SCTP_PARAM_HOST_NAME_ADDRESS:
				asoc->peer.hostname_address = 1;
				break;

			default: 
				break;
			}
		}
		break;

	case SCTP_PARAM_STATE_COOKIE:
		asoc->peer.cookie_len =
			ntohs(param.p->length) - sizeof(sctp_paramhdr_t);
		asoc->peer.cookie = param.cookie->body;
		break;

	case SCTP_PARAM_HEARTBEAT_INFO:
		
		break;

	case SCTP_PARAM_UNRECOGNIZED_PARAMETERS:
		
		break;

	case SCTP_PARAM_ECN_CAPABLE:
		asoc->peer.ecn_capable = 1;
		break;

	case SCTP_PARAM_ADAPTATION_LAYER_IND:
		asoc->peer.adaptation_ind = ntohl(param.aind->adaptation_ind);
		break;

	case SCTP_PARAM_SET_PRIMARY:
		if (!sctp_addip_enable)
			goto fall_through;

		addr_param = param.v + sizeof(sctp_addip_param_t);

		af = sctp_get_af_specific(param_type2af(param.p->type));
		af->from_addr_param(&addr, addr_param,
				    htons(asoc->peer.port), 0);

		if (!af->addr_valid(&addr, NULL, NULL))
			break;

		t = sctp_assoc_lookup_paddr(asoc, &addr);
		if (!t)
			break;

		sctp_assoc_set_primary(asoc, t);
		break;

	case SCTP_PARAM_SUPPORTED_EXT:
		sctp_process_ext_param(asoc, param);
		break;

	case SCTP_PARAM_FWD_TSN_SUPPORT:
		if (sctp_prsctp_enable) {
			asoc->peer.prsctp_capable = 1;
			break;
		}
		
		goto fall_through;

	case SCTP_PARAM_RANDOM:
		if (!sctp_auth_enable)
			goto fall_through;

		
		asoc->peer.peer_random = kmemdup(param.p,
					    ntohs(param.p->length), gfp);
		if (!asoc->peer.peer_random) {
			retval = 0;
			break;
		}
		break;

	case SCTP_PARAM_HMAC_ALGO:
		if (!sctp_auth_enable)
			goto fall_through;

		
		asoc->peer.peer_hmacs = kmemdup(param.p,
					    ntohs(param.p->length), gfp);
		if (!asoc->peer.peer_hmacs) {
			retval = 0;
			break;
		}

		
		sctp_auth_asoc_set_default_hmac(asoc, param.hmac_algo);
		break;

	case SCTP_PARAM_CHUNKS:
		if (!sctp_auth_enable)
			goto fall_through;

		asoc->peer.peer_chunks = kmemdup(param.p,
					    ntohs(param.p->length), gfp);
		if (!asoc->peer.peer_chunks)
			retval = 0;
		break;
fall_through:
	default:
		SCTP_DEBUG_PRINTK("Ignoring param: %d for association %p.\n",
				  ntohs(param.p->type), asoc);
		break;
	}

	return retval;
}

__u32 sctp_generate_tag(const struct sctp_endpoint *ep)
{
	__u32 x;

	do {
		get_random_bytes(&x, sizeof(__u32));
	} while (x == 0);

	return x;
}

__u32 sctp_generate_tsn(const struct sctp_endpoint *ep)
{
	__u32 retval;

	get_random_bytes(&retval, sizeof(__u32));
	return retval;
}

static struct sctp_chunk *sctp_make_asconf(struct sctp_association *asoc,
					   union sctp_addr *addr,
					   int vparam_len)
{
	sctp_addiphdr_t asconf;
	struct sctp_chunk *retval;
	int length = sizeof(asconf) + vparam_len;
	union sctp_addr_param addrparam;
	int addrlen;
	struct sctp_af *af = sctp_get_af_specific(addr->v4.sin_family);

	addrlen = af->to_addr_param(addr, &addrparam);
	if (!addrlen)
		return NULL;
	length += addrlen;

	
	retval = sctp_make_chunk(asoc, SCTP_CID_ASCONF, 0, length);
	if (!retval)
		return NULL;

	asconf.serial = htonl(asoc->addip_serial++);

	retval->subh.addip_hdr =
		sctp_addto_chunk(retval, sizeof(asconf), &asconf);
	retval->param_hdr.v =
		sctp_addto_chunk(retval, addrlen, &addrparam);

	return retval;
}

struct sctp_chunk *sctp_make_asconf_update_ip(struct sctp_association *asoc,
					      union sctp_addr	      *laddr,
					      struct sockaddr	      *addrs,
					      int		      addrcnt,
					      __be16		      flags)
{
	sctp_addip_param_t	param;
	struct sctp_chunk	*retval;
	union sctp_addr_param	addr_param;
	union sctp_addr		*addr;
	void			*addr_buf;
	struct sctp_af		*af;
	int			paramlen = sizeof(param);
	int			addr_param_len = 0;
	int 			totallen = 0;
	int 			i;
	int			del_pickup = 0;

	
	addr_buf = addrs;
	for (i = 0; i < addrcnt; i++) {
		addr = addr_buf;
		af = sctp_get_af_specific(addr->v4.sin_family);
		addr_param_len = af->to_addr_param(addr, &addr_param);

		totallen += paramlen;
		totallen += addr_param_len;

		addr_buf += af->sockaddr_len;
		if (asoc->asconf_addr_del_pending && !del_pickup) {
			
			totallen += paramlen;
			totallen += addr_param_len;
			del_pickup = 1;
			SCTP_DEBUG_PRINTK("mkasconf_update_ip: picked same-scope del_pending addr, totallen for all addresses is %d\n", totallen);
		}
	}

	
	retval = sctp_make_asconf(asoc, laddr, totallen);
	if (!retval)
		return NULL;

	
	addr_buf = addrs;
	for (i = 0; i < addrcnt; i++) {
		addr = addr_buf;
		af = sctp_get_af_specific(addr->v4.sin_family);
		addr_param_len = af->to_addr_param(addr, &addr_param);
		param.param_hdr.type = flags;
		param.param_hdr.length = htons(paramlen + addr_param_len);
		param.crr_id = i;

		sctp_addto_chunk(retval, paramlen, &param);
		sctp_addto_chunk(retval, addr_param_len, &addr_param);

		addr_buf += af->sockaddr_len;
	}
	if (flags == SCTP_PARAM_ADD_IP && del_pickup) {
		addr = asoc->asconf_addr_del_pending;
		af = sctp_get_af_specific(addr->v4.sin_family);
		addr_param_len = af->to_addr_param(addr, &addr_param);
		param.param_hdr.type = SCTP_PARAM_DEL_IP;
		param.param_hdr.length = htons(paramlen + addr_param_len);
		param.crr_id = i;

		sctp_addto_chunk(retval, paramlen, &param);
		sctp_addto_chunk(retval, addr_param_len, &addr_param);
	}
	return retval;
}

struct sctp_chunk *sctp_make_asconf_set_prim(struct sctp_association *asoc,
					     union sctp_addr *addr)
{
	sctp_addip_param_t	param;
	struct sctp_chunk 	*retval;
	int 			len = sizeof(param);
	union sctp_addr_param	addrparam;
	int			addrlen;
	struct sctp_af		*af = sctp_get_af_specific(addr->v4.sin_family);

	addrlen = af->to_addr_param(addr, &addrparam);
	if (!addrlen)
		return NULL;
	len += addrlen;

	
	retval = sctp_make_asconf(asoc, addr, len);
	if (!retval)
		return NULL;

	param.param_hdr.type = SCTP_PARAM_SET_PRIMARY;
	param.param_hdr.length = htons(len);
	param.crr_id = 0;

	sctp_addto_chunk(retval, sizeof(param), &param);
	sctp_addto_chunk(retval, addrlen, &addrparam);

	return retval;
}

static struct sctp_chunk *sctp_make_asconf_ack(const struct sctp_association *asoc,
					       __u32 serial, int vparam_len)
{
	sctp_addiphdr_t		asconf;
	struct sctp_chunk	*retval;
	int			length = sizeof(asconf) + vparam_len;

	
	retval = sctp_make_chunk(asoc, SCTP_CID_ASCONF_ACK, 0, length);
	if (!retval)
		return NULL;

	asconf.serial = htonl(serial);

	retval->subh.addip_hdr =
		sctp_addto_chunk(retval, sizeof(asconf), &asconf);

	return retval;
}

static void sctp_add_asconf_response(struct sctp_chunk *chunk, __be32 crr_id,
			      __be16 err_code, sctp_addip_param_t *asconf_param)
{
	sctp_addip_param_t 	ack_param;
	sctp_errhdr_t		err_param;
	int			asconf_param_len = 0;
	int			err_param_len = 0;
	__be16			response_type;

	if (SCTP_ERROR_NO_ERROR == err_code) {
		response_type = SCTP_PARAM_SUCCESS_REPORT;
	} else {
		response_type = SCTP_PARAM_ERR_CAUSE;
		err_param_len = sizeof(err_param);
		if (asconf_param)
			asconf_param_len =
				 ntohs(asconf_param->param_hdr.length);
	}

	
	ack_param.param_hdr.type = response_type;
	ack_param.param_hdr.length = htons(sizeof(ack_param) +
					   err_param_len +
					   asconf_param_len);
	ack_param.crr_id = crr_id;
	sctp_addto_chunk(chunk, sizeof(ack_param), &ack_param);

	if (SCTP_ERROR_NO_ERROR == err_code)
		return;

	
	err_param.cause = err_code;
	err_param.length = htons(err_param_len + asconf_param_len);
	sctp_addto_chunk(chunk, err_param_len, &err_param);

	
	if (asconf_param)
		sctp_addto_chunk(chunk, asconf_param_len, asconf_param);
}

static __be16 sctp_process_asconf_param(struct sctp_association *asoc,
				       struct sctp_chunk *asconf,
				       sctp_addip_param_t *asconf_param)
{
	struct sctp_transport *peer;
	struct sctp_af *af;
	union sctp_addr	addr;
	union sctp_addr_param *addr_param;

	addr_param = (void *)asconf_param + sizeof(sctp_addip_param_t);

	if (asconf_param->param_hdr.type != SCTP_PARAM_ADD_IP &&
	    asconf_param->param_hdr.type != SCTP_PARAM_DEL_IP &&
	    asconf_param->param_hdr.type != SCTP_PARAM_SET_PRIMARY)
		return SCTP_ERROR_UNKNOWN_PARAM;

	switch (addr_param->p.type) {
	case SCTP_PARAM_IPV6_ADDRESS:
		if (!asoc->peer.ipv6_address)
			return SCTP_ERROR_DNS_FAILED;
		break;
	case SCTP_PARAM_IPV4_ADDRESS:
		if (!asoc->peer.ipv4_address)
			return SCTP_ERROR_DNS_FAILED;
		break;
	default:
		return SCTP_ERROR_DNS_FAILED;
	}

	af = sctp_get_af_specific(param_type2af(addr_param->p.type));
	if (unlikely(!af))
		return SCTP_ERROR_DNS_FAILED;

	af->from_addr_param(&addr, addr_param, htons(asoc->peer.port), 0);

	if (!af->is_any(&addr) && !af->addr_valid(&addr, NULL, asconf->skb))
		return SCTP_ERROR_DNS_FAILED;

	switch (asconf_param->param_hdr.type) {
	case SCTP_PARAM_ADD_IP:
		if (af->is_any(&addr))
			memcpy(&addr, &asconf->source, sizeof(addr));


		peer = sctp_assoc_add_peer(asoc, &addr, GFP_ATOMIC, SCTP_UNCONFIRMED);
		if (!peer)
			return SCTP_ERROR_RSRC_LOW;

		
		if (!mod_timer(&peer->hb_timer, sctp_transport_timeout(peer)))
			sctp_transport_hold(peer);
		asoc->new_transport = peer;
		break;
	case SCTP_PARAM_DEL_IP:
		if (asoc->peer.transport_count == 1)
			return SCTP_ERROR_DEL_LAST_IP;

		if (sctp_cmp_addr_exact(&asconf->source, &addr))
			return SCTP_ERROR_DEL_SRC_IP;

		if (af->is_any(&addr)) {
			sctp_assoc_set_primary(asoc, asconf->transport);
			sctp_assoc_del_nonprimary_peers(asoc,
							asconf->transport);
		} else
			sctp_assoc_del_peer(asoc, &addr);
		break;
	case SCTP_PARAM_SET_PRIMARY:
		if (af->is_any(&addr))
			memcpy(&addr.v4, sctp_source(asconf), sizeof(addr));

		peer = sctp_assoc_lookup_paddr(asoc, &addr);
		if (!peer)
			return SCTP_ERROR_DNS_FAILED;

		sctp_assoc_set_primary(asoc, peer);
		break;
	}

	return SCTP_ERROR_NO_ERROR;
}

int sctp_verify_asconf(const struct sctp_association *asoc,
		       struct sctp_paramhdr *param_hdr, void *chunk_end,
		       struct sctp_paramhdr **errp) {
	sctp_addip_param_t *asconf_param;
	union sctp_params param;
	int length, plen;

	param.v = (sctp_paramhdr_t *) param_hdr;
	while (param.v <= chunk_end - sizeof(sctp_paramhdr_t)) {
		length = ntohs(param.p->length);
		*errp = param.p;

		if (param.v > chunk_end - length ||
		    length < sizeof(sctp_paramhdr_t))
			return 0;

		switch (param.p->type) {
		case SCTP_PARAM_ADD_IP:
		case SCTP_PARAM_DEL_IP:
		case SCTP_PARAM_SET_PRIMARY:
			asconf_param = (sctp_addip_param_t *)param.v;
			plen = ntohs(asconf_param->param_hdr.length);
			if (plen < sizeof(sctp_addip_param_t) +
			    sizeof(sctp_paramhdr_t))
				return 0;
			break;
		case SCTP_PARAM_SUCCESS_REPORT:
		case SCTP_PARAM_ADAPTATION_LAYER_IND:
			if (length != sizeof(sctp_addip_param_t))
				return 0;

			break;
		default:
			break;
		}

		param.v += WORD_ROUND(length);
	}

	if (param.v != chunk_end)
		return 0;

	return 1;
}

struct sctp_chunk *sctp_process_asconf(struct sctp_association *asoc,
				       struct sctp_chunk *asconf)
{
	sctp_addiphdr_t		*hdr;
	union sctp_addr_param	*addr_param;
	sctp_addip_param_t	*asconf_param;
	struct sctp_chunk	*asconf_ack;

	__be16	err_code;
	int	length = 0;
	int	chunk_len;
	__u32	serial;
	int	all_param_pass = 1;

	chunk_len = ntohs(asconf->chunk_hdr->length) - sizeof(sctp_chunkhdr_t);
	hdr = (sctp_addiphdr_t *)asconf->skb->data;
	serial = ntohl(hdr->serial);

	
	length = sizeof(sctp_addiphdr_t);
	addr_param = (union sctp_addr_param *)(asconf->skb->data + length);
	chunk_len -= length;

	length = ntohs(addr_param->p.length);
	asconf_param = (void *)addr_param + length;
	chunk_len -= length;

	asconf_ack = sctp_make_asconf_ack(asoc, serial, chunk_len * 4);
	if (!asconf_ack)
		goto done;

	
	while (chunk_len > 0) {
		err_code = sctp_process_asconf_param(asoc, asconf,
						     asconf_param);
		if (SCTP_ERROR_NO_ERROR != err_code)
			all_param_pass = 0;

		if (!all_param_pass)
			sctp_add_asconf_response(asconf_ack,
						 asconf_param->crr_id, err_code,
						 asconf_param);

		if (SCTP_ERROR_RSRC_LOW == err_code)
			goto done;

		
		length = ntohs(asconf_param->param_hdr.length);
		asconf_param = (void *)asconf_param + length;
		chunk_len -= length;
	}

done:
	asoc->peer.addip_serial++;

	if (asconf_ack) {
		sctp_chunk_hold(asconf_ack);
		list_add_tail(&asconf_ack->transmitted_list,
			      &asoc->asconf_ack_list);
	}

	return asconf_ack;
}

static void sctp_asconf_param_success(struct sctp_association *asoc,
				     sctp_addip_param_t *asconf_param)
{
	struct sctp_af *af;
	union sctp_addr	addr;
	struct sctp_bind_addr *bp = &asoc->base.bind_addr;
	union sctp_addr_param *addr_param;
	struct sctp_transport *transport;
	struct sctp_sockaddr_entry *saddr;

	addr_param = (void *)asconf_param + sizeof(sctp_addip_param_t);

	
	af = sctp_get_af_specific(param_type2af(addr_param->p.type));
	af->from_addr_param(&addr, addr_param, htons(bp->port), 0);

	switch (asconf_param->param_hdr.type) {
	case SCTP_PARAM_ADD_IP:
		local_bh_disable();
		list_for_each_entry(saddr, &bp->address_list, list) {
			if (sctp_cmp_addr_exact(&saddr->a, &addr))
				saddr->state = SCTP_ADDR_SRC;
		}
		local_bh_enable();
		list_for_each_entry(transport, &asoc->peer.transport_addr_list,
				transports) {
			dst_release(transport->dst);
			transport->dst = NULL;
		}
		break;
	case SCTP_PARAM_DEL_IP:
		local_bh_disable();
		sctp_del_bind_addr(bp, &addr);
		if (asoc->asconf_addr_del_pending != NULL &&
		    sctp_cmp_addr_exact(asoc->asconf_addr_del_pending, &addr)) {
			kfree(asoc->asconf_addr_del_pending);
			asoc->asconf_addr_del_pending = NULL;
		}
		local_bh_enable();
		list_for_each_entry(transport, &asoc->peer.transport_addr_list,
				transports) {
			dst_release(transport->dst);
			transport->dst = NULL;
		}
		break;
	default:
		break;
	}
}

static __be16 sctp_get_asconf_response(struct sctp_chunk *asconf_ack,
				      sctp_addip_param_t *asconf_param,
				      int no_err)
{
	sctp_addip_param_t	*asconf_ack_param;
	sctp_errhdr_t		*err_param;
	int			length;
	int			asconf_ack_len;
	__be16			err_code;

	if (no_err)
		err_code = SCTP_ERROR_NO_ERROR;
	else
		err_code = SCTP_ERROR_REQ_REFUSED;

	asconf_ack_len = ntohs(asconf_ack->chunk_hdr->length) -
			     sizeof(sctp_chunkhdr_t);

	length = sizeof(sctp_addiphdr_t);
	asconf_ack_param = (sctp_addip_param_t *)(asconf_ack->skb->data +
						  length);
	asconf_ack_len -= length;

	while (asconf_ack_len > 0) {
		if (asconf_ack_param->crr_id == asconf_param->crr_id) {
			switch(asconf_ack_param->param_hdr.type) {
			case SCTP_PARAM_SUCCESS_REPORT:
				return SCTP_ERROR_NO_ERROR;
			case SCTP_PARAM_ERR_CAUSE:
				length = sizeof(sctp_addip_param_t);
				err_param = (void *)asconf_ack_param + length;
				asconf_ack_len -= length;
				if (asconf_ack_len > 0)
					return err_param->cause;
				else
					return SCTP_ERROR_INV_PARAM;
				break;
			default:
				return SCTP_ERROR_INV_PARAM;
			}
		}

		length = ntohs(asconf_ack_param->param_hdr.length);
		asconf_ack_param = (void *)asconf_ack_param + length;
		asconf_ack_len -= length;
	}

	return err_code;
}

int sctp_process_asconf_ack(struct sctp_association *asoc,
			    struct sctp_chunk *asconf_ack)
{
	struct sctp_chunk	*asconf = asoc->addip_last_asconf;
	union sctp_addr_param	*addr_param;
	sctp_addip_param_t	*asconf_param;
	int	length = 0;
	int	asconf_len = asconf->skb->len;
	int	all_param_pass = 0;
	int	no_err = 1;
	int	retval = 0;
	__be16	err_code = SCTP_ERROR_NO_ERROR;

	length = sizeof(sctp_addip_chunk_t);
	addr_param = (union sctp_addr_param *)(asconf->skb->data + length);
	asconf_len -= length;

	length = ntohs(addr_param->p.length);
	asconf_param = (void *)addr_param + length;
	asconf_len -= length;

	if (asconf_ack->skb->len == sizeof(sctp_addiphdr_t))
		all_param_pass = 1;

	
	while (asconf_len > 0) {
		if (all_param_pass)
			err_code = SCTP_ERROR_NO_ERROR;
		else {
			err_code = sctp_get_asconf_response(asconf_ack,
							    asconf_param,
							    no_err);
			if (no_err && (SCTP_ERROR_NO_ERROR != err_code))
				no_err = 0;
		}

		switch (err_code) {
		case SCTP_ERROR_NO_ERROR:
			sctp_asconf_param_success(asoc, asconf_param);
			break;

		case SCTP_ERROR_RSRC_LOW:
			retval = 1;
			break;

		case SCTP_ERROR_UNKNOWN_PARAM:
			asoc->peer.addip_disabled_mask |=
				asconf_param->param_hdr.type;
			break;

		case SCTP_ERROR_REQ_REFUSED:
		case SCTP_ERROR_DEL_LAST_IP:
		case SCTP_ERROR_DEL_SRC_IP:
		default:
			 break;
		}

		length = ntohs(asconf_param->param_hdr.length);
		asconf_param = (void *)asconf_param + length;
		asconf_len -= length;
	}

	if (no_err && asoc->src_out_of_asoc_ok) {
		asoc->src_out_of_asoc_ok = 0;
		sctp_transport_immediate_rtx(asoc->peer.primary_path);
	}

	
	list_del_init(&asconf->transmitted_list);
	sctp_chunk_free(asconf);
	asoc->addip_last_asconf = NULL;

	return retval;
}

struct sctp_chunk *sctp_make_fwdtsn(const struct sctp_association *asoc,
				    __u32 new_cum_tsn, size_t nstreams,
				    struct sctp_fwdtsn_skip *skiplist)
{
	struct sctp_chunk *retval = NULL;
	struct sctp_fwdtsn_hdr ftsn_hdr;
	struct sctp_fwdtsn_skip skip;
	size_t hint;
	int i;

	hint = (nstreams + 1) * sizeof(__u32);

	retval = sctp_make_chunk(asoc, SCTP_CID_FWD_TSN, 0, hint);

	if (!retval)
		return NULL;

	ftsn_hdr.new_cum_tsn = htonl(new_cum_tsn);
	retval->subh.fwdtsn_hdr =
		sctp_addto_chunk(retval, sizeof(ftsn_hdr), &ftsn_hdr);

	for (i = 0; i < nstreams; i++) {
		skip.stream = skiplist[i].stream;
		skip.ssn = skiplist[i].ssn;
		sctp_addto_chunk(retval, sizeof(skip), &skip);
	}

	return retval;
}
