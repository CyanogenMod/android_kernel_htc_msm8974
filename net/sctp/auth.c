/* SCTP kernel implementation
 * (C) Copyright 2007 Hewlett-Packard Development Company, L.P.
 *
 * This file is part of the SCTP kernel implementation
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
 *   Vlad Yasevich     <vladislav.yasevich@hp.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <net/sctp/sctp.h>
#include <net/sctp/auth.h>

static struct sctp_hmac sctp_hmac_list[SCTP_AUTH_NUM_HMACS] = {
	{
		
		.hmac_id = SCTP_AUTH_HMAC_ID_RESERVED_0,
	},
	{
		.hmac_id = SCTP_AUTH_HMAC_ID_SHA1,
		.hmac_name="hmac(sha1)",
		.hmac_len = SCTP_SHA1_SIG_SIZE,
	},
	{
		
		.hmac_id = SCTP_AUTH_HMAC_ID_RESERVED_2,
	},
#if defined (CONFIG_CRYPTO_SHA256) || defined (CONFIG_CRYPTO_SHA256_MODULE)
	{
		.hmac_id = SCTP_AUTH_HMAC_ID_SHA256,
		.hmac_name="hmac(sha256)",
		.hmac_len = SCTP_SHA256_SIG_SIZE,
	}
#endif
};


void sctp_auth_key_put(struct sctp_auth_bytes *key)
{
	if (!key)
		return;

	if (atomic_dec_and_test(&key->refcnt)) {
		kfree(key);
		SCTP_DBG_OBJCNT_DEC(keys);
	}
}

static struct sctp_auth_bytes *sctp_auth_create_key(__u32 key_len, gfp_t gfp)
{
	struct sctp_auth_bytes *key;

	
	if (key_len > (INT_MAX - sizeof(struct sctp_auth_bytes)))
		return NULL;

	
	key = kmalloc(sizeof(struct sctp_auth_bytes) + key_len, gfp);
	if (!key)
		return NULL;

	key->len = key_len;
	atomic_set(&key->refcnt, 1);
	SCTP_DBG_OBJCNT_INC(keys);

	return key;
}

struct sctp_shared_key *sctp_auth_shkey_create(__u16 key_id, gfp_t gfp)
{
	struct sctp_shared_key *new;

	
	new = kzalloc(sizeof(struct sctp_shared_key), gfp);
	if (!new)
		return NULL;

	INIT_LIST_HEAD(&new->key_list);
	new->key_id = key_id;

	return new;
}

static void sctp_auth_shkey_free(struct sctp_shared_key *sh_key)
{
	BUG_ON(!list_empty(&sh_key->key_list));
	sctp_auth_key_put(sh_key->key);
	sh_key->key = NULL;
	kfree(sh_key);
}

void sctp_auth_destroy_keys(struct list_head *keys)
{
	struct sctp_shared_key *ep_key;
	struct sctp_shared_key *tmp;

	if (list_empty(keys))
		return;

	key_for_each_safe(ep_key, tmp, keys) {
		list_del_init(&ep_key->key_list);
		sctp_auth_shkey_free(ep_key);
	}
}

static int sctp_auth_compare_vectors(struct sctp_auth_bytes *vector1,
			      struct sctp_auth_bytes *vector2)
{
	int diff;
	int i;
	const __u8 *longer;

	diff = vector1->len - vector2->len;
	if (diff) {
		longer = (diff > 0) ? vector1->data : vector2->data;

		for (i = 0; i < abs(diff); i++ ) {
			if (longer[i] != 0)
				return diff;
		}
	}

	
	return memcmp(vector1->data, vector2->data, vector1->len);
}

static struct sctp_auth_bytes *sctp_auth_make_key_vector(
			sctp_random_param_t *random,
			sctp_chunks_param_t *chunks,
			sctp_hmac_algo_param_t *hmacs,
			gfp_t gfp)
{
	struct sctp_auth_bytes *new;
	__u32	len;
	__u32	offset = 0;

	len = ntohs(random->param_hdr.length) + ntohs(hmacs->param_hdr.length);
        if (chunks)
		len += ntohs(chunks->param_hdr.length);

	new = kmalloc(sizeof(struct sctp_auth_bytes) + len, gfp);
	if (!new)
		return NULL;

	new->len = len;

	memcpy(new->data, random, ntohs(random->param_hdr.length));
	offset += ntohs(random->param_hdr.length);

	if (chunks) {
		memcpy(new->data + offset, chunks,
			ntohs(chunks->param_hdr.length));
		offset += ntohs(chunks->param_hdr.length);
	}

	memcpy(new->data + offset, hmacs, ntohs(hmacs->param_hdr.length));

	return new;
}


static struct sctp_auth_bytes *sctp_auth_make_local_vector(
				    const struct sctp_association *asoc,
				    gfp_t gfp)
{
	return sctp_auth_make_key_vector(
				    (sctp_random_param_t*)asoc->c.auth_random,
				    (sctp_chunks_param_t*)asoc->c.auth_chunks,
				    (sctp_hmac_algo_param_t*)asoc->c.auth_hmacs,
				    gfp);
}

static struct sctp_auth_bytes *sctp_auth_make_peer_vector(
				    const struct sctp_association *asoc,
				    gfp_t gfp)
{
	return sctp_auth_make_key_vector(asoc->peer.peer_random,
					 asoc->peer.peer_chunks,
					 asoc->peer.peer_hmacs,
					 gfp);
}


static struct sctp_auth_bytes *sctp_auth_asoc_set_secret(
			struct sctp_shared_key *ep_key,
			struct sctp_auth_bytes *first_vector,
			struct sctp_auth_bytes *last_vector,
			gfp_t gfp)
{
	struct sctp_auth_bytes *secret;
	__u32 offset = 0;
	__u32 auth_len;

	auth_len = first_vector->len + last_vector->len;
	if (ep_key->key)
		auth_len += ep_key->key->len;

	secret = sctp_auth_create_key(auth_len, gfp);
	if (!secret)
		return NULL;

	if (ep_key->key) {
		memcpy(secret->data, ep_key->key->data, ep_key->key->len);
		offset += ep_key->key->len;
	}

	memcpy(secret->data + offset, first_vector->data, first_vector->len);
	offset += first_vector->len;

	memcpy(secret->data + offset, last_vector->data, last_vector->len);

	return secret;
}

static struct sctp_auth_bytes *sctp_auth_asoc_create_secret(
				 const struct sctp_association *asoc,
				 struct sctp_shared_key *ep_key,
				 gfp_t gfp)
{
	struct sctp_auth_bytes *local_key_vector;
	struct sctp_auth_bytes *peer_key_vector;
	struct sctp_auth_bytes	*first_vector,
				*last_vector;
	struct sctp_auth_bytes	*secret = NULL;
	int	cmp;



	local_key_vector = sctp_auth_make_local_vector(asoc, gfp);
	peer_key_vector = sctp_auth_make_peer_vector(asoc, gfp);

	if (!peer_key_vector || !local_key_vector)
		goto out;

	cmp = sctp_auth_compare_vectors(local_key_vector,
					peer_key_vector);
	if (cmp < 0) {
		first_vector = local_key_vector;
		last_vector = peer_key_vector;
	} else {
		first_vector = peer_key_vector;
		last_vector = local_key_vector;
	}

	secret = sctp_auth_asoc_set_secret(ep_key, first_vector, last_vector,
					    gfp);
out:
	kfree(local_key_vector);
	kfree(peer_key_vector);

	return secret;
}

int sctp_auth_asoc_copy_shkeys(const struct sctp_endpoint *ep,
				struct sctp_association *asoc,
				gfp_t gfp)
{
	struct sctp_shared_key *sh_key;
	struct sctp_shared_key *new;

	BUG_ON(!list_empty(&asoc->endpoint_shared_keys));

	key_for_each(sh_key, &ep->endpoint_shared_keys) {
		new = sctp_auth_shkey_create(sh_key->key_id, gfp);
		if (!new)
			goto nomem;

		new->key = sh_key->key;
		sctp_auth_key_hold(new->key);
		list_add(&new->key_list, &asoc->endpoint_shared_keys);
	}

	return 0;

nomem:
	sctp_auth_destroy_keys(&asoc->endpoint_shared_keys);
	return -ENOMEM;
}


int sctp_auth_asoc_init_active_key(struct sctp_association *asoc, gfp_t gfp)
{
	struct sctp_auth_bytes	*secret;
	struct sctp_shared_key *ep_key;

	if (!sctp_auth_enable || !asoc->peer.auth_capable)
		return 0;

	ep_key = sctp_auth_get_shkey(asoc, asoc->active_key_id);
	BUG_ON(!ep_key);

	secret = sctp_auth_asoc_create_secret(asoc, ep_key, gfp);
	if (!secret)
		return -ENOMEM;

	sctp_auth_key_put(asoc->asoc_shared_key);
	asoc->asoc_shared_key = secret;

	return 0;
}


struct sctp_shared_key *sctp_auth_get_shkey(
				const struct sctp_association *asoc,
				__u16 key_id)
{
	struct sctp_shared_key *key;

	
	key_for_each(key, &asoc->endpoint_shared_keys) {
		if (key->key_id == key_id)
			return key;
	}

	return NULL;
}

int sctp_auth_init_hmacs(struct sctp_endpoint *ep, gfp_t gfp)
{
	struct crypto_hash *tfm = NULL;
	__u16   id;

	
	if (!sctp_auth_enable) {
		ep->auth_hmacs = NULL;
		return 0;
	}

	if (ep->auth_hmacs)
		return 0;

	
	ep->auth_hmacs = kzalloc(
			    sizeof(struct crypto_hash *) * SCTP_AUTH_NUM_HMACS,
			    gfp);
	if (!ep->auth_hmacs)
		return -ENOMEM;

	for (id = 0; id < SCTP_AUTH_NUM_HMACS; id++) {

		if (!sctp_hmac_list[id].hmac_name)
			continue;

		
		if (ep->auth_hmacs[id])
			continue;

		
		tfm = crypto_alloc_hash(sctp_hmac_list[id].hmac_name, 0,
					CRYPTO_ALG_ASYNC);
		if (IS_ERR(tfm))
			goto out_err;

		ep->auth_hmacs[id] = tfm;
	}

	return 0;

out_err:
	
	sctp_auth_destroy_hmacs(ep->auth_hmacs);
	return -ENOMEM;
}

void sctp_auth_destroy_hmacs(struct crypto_hash *auth_hmacs[])
{
	int i;

	if (!auth_hmacs)
		return;

	for (i = 0; i < SCTP_AUTH_NUM_HMACS; i++)
	{
		if (auth_hmacs[i])
			crypto_free_hash(auth_hmacs[i]);
	}
	kfree(auth_hmacs);
}


struct sctp_hmac *sctp_auth_get_hmac(__u16 hmac_id)
{
	return &sctp_hmac_list[hmac_id];
}

struct sctp_hmac *sctp_auth_asoc_get_hmac(const struct sctp_association *asoc)
{
	struct sctp_hmac_algo_param *hmacs;
	__u16 n_elt;
	__u16 id = 0;
	int i;

	
	if (asoc->default_hmac_id)
		return &sctp_hmac_list[asoc->default_hmac_id];

	hmacs = asoc->peer.peer_hmacs;
	if (!hmacs)
		return NULL;

	n_elt = (ntohs(hmacs->param_hdr.length) - sizeof(sctp_paramhdr_t)) >> 1;
	for (i = 0; i < n_elt; i++) {
		id = ntohs(hmacs->hmac_ids[i]);

		
		if (id > SCTP_AUTH_HMAC_ID_MAX) {
			id = 0;
			continue;
		}

		if (!sctp_hmac_list[id].hmac_name) {
			id = 0;
			continue;
		}

		break;
	}

	if (id == 0)
		return NULL;

	return &sctp_hmac_list[id];
}

static int __sctp_auth_find_hmacid(__be16 *hmacs, int n_elts, __be16 hmac_id)
{
	int  found = 0;
	int  i;

	for (i = 0; i < n_elts; i++) {
		if (hmac_id == hmacs[i]) {
			found = 1;
			break;
		}
	}

	return found;
}

int sctp_auth_asoc_verify_hmac_id(const struct sctp_association *asoc,
				    __be16 hmac_id)
{
	struct sctp_hmac_algo_param *hmacs;
	__u16 n_elt;

	if (!asoc)
		return 0;

	hmacs = (struct sctp_hmac_algo_param *)asoc->c.auth_hmacs;
	n_elt = (ntohs(hmacs->param_hdr.length) - sizeof(sctp_paramhdr_t)) >> 1;

	return __sctp_auth_find_hmacid(hmacs->hmac_ids, n_elt, hmac_id);
}


void sctp_auth_asoc_set_default_hmac(struct sctp_association *asoc,
				     struct sctp_hmac_algo_param *hmacs)
{
	struct sctp_endpoint *ep;
	__u16   id;
	int	i;
	int	n_params;

	
	if (asoc->default_hmac_id)
		return;

	n_params = (ntohs(hmacs->param_hdr.length)
				- sizeof(sctp_paramhdr_t)) >> 1;
	ep = asoc->ep;
	for (i = 0; i < n_params; i++) {
		id = ntohs(hmacs->hmac_ids[i]);

		
		if (id > SCTP_AUTH_HMAC_ID_MAX)
			continue;

		
		if (ep->auth_hmacs[id]) {
			asoc->default_hmac_id = id;
			break;
		}
	}
}


static int __sctp_auth_cid(sctp_cid_t chunk, struct sctp_chunks_param *param)
{
	unsigned short len;
	int found = 0;
	int i;

	if (!param || param->param_hdr.length == 0)
		return 0;

	len = ntohs(param->param_hdr.length) - sizeof(sctp_paramhdr_t);

	for (i = 0; !found && i < len; i++) {
		switch (param->chunks[i]) {
		    case SCTP_CID_INIT:
		    case SCTP_CID_INIT_ACK:
		    case SCTP_CID_SHUTDOWN_COMPLETE:
		    case SCTP_CID_AUTH:
			break;

		    default:
			if (param->chunks[i] == chunk)
			    found = 1;
			break;
		}
	}

	return found;
}

int sctp_auth_send_cid(sctp_cid_t chunk, const struct sctp_association *asoc)
{
	if (!sctp_auth_enable || !asoc || !asoc->peer.auth_capable)
		return 0;

	return __sctp_auth_cid(chunk, asoc->peer.peer_chunks);
}

int sctp_auth_recv_cid(sctp_cid_t chunk, const struct sctp_association *asoc)
{
	if (!sctp_auth_enable || !asoc)
		return 0;

	return __sctp_auth_cid(chunk,
			      (struct sctp_chunks_param *)asoc->c.auth_chunks);
}

void sctp_auth_calculate_hmac(const struct sctp_association *asoc,
			      struct sk_buff *skb,
			      struct sctp_auth_chunk *auth,
			      gfp_t gfp)
{
	struct scatterlist sg;
	struct hash_desc desc;
	struct sctp_auth_bytes *asoc_key;
	__u16 key_id, hmac_id;
	__u8 *digest;
	unsigned char *end;
	int free_key = 0;

	key_id = ntohs(auth->auth_hdr.shkey_id);
	hmac_id = ntohs(auth->auth_hdr.hmac_id);

	if (key_id == asoc->active_key_id)
		asoc_key = asoc->asoc_shared_key;
	else {
		struct sctp_shared_key *ep_key;

		ep_key = sctp_auth_get_shkey(asoc, key_id);
		if (!ep_key)
			return;

		asoc_key = sctp_auth_asoc_create_secret(asoc, ep_key, gfp);
		if (!asoc_key)
			return;

		free_key = 1;
	}

	
	end = skb_tail_pointer(skb);
	sg_init_one(&sg, auth, end - (unsigned char *)auth);

	desc.tfm = asoc->ep->auth_hmacs[hmac_id];
	desc.flags = 0;

	digest = auth->auth_hdr.hmac;
	if (crypto_hash_setkey(desc.tfm, &asoc_key->data[0], asoc_key->len))
		goto free;

	crypto_hash_digest(&desc, &sg, sg.length, digest);

free:
	if (free_key)
		sctp_auth_key_put(asoc_key);
}


int sctp_auth_ep_add_chunkid(struct sctp_endpoint *ep, __u8 chunk_id)
{
	struct sctp_chunks_param *p = ep->auth_chunk_list;
	__u16 nchunks;
	__u16 param_len;

	
	if (__sctp_auth_cid(chunk_id, p))
		return 0;

	
	param_len = ntohs(p->param_hdr.length);
	nchunks = param_len - sizeof(sctp_paramhdr_t);
	if (nchunks == SCTP_NUM_CHUNK_TYPES)
		return -EINVAL;

	p->chunks[nchunks] = chunk_id;
	p->param_hdr.length = htons(param_len + 1);
	return 0;
}

int sctp_auth_ep_set_hmacs(struct sctp_endpoint *ep,
			   struct sctp_hmacalgo *hmacs)
{
	int has_sha1 = 0;
	__u16 id;
	int i;

	for (i = 0; i < hmacs->shmac_num_idents; i++) {
		id = hmacs->shmac_idents[i];

		if (id > SCTP_AUTH_HMAC_ID_MAX)
			return -EOPNOTSUPP;

		if (SCTP_AUTH_HMAC_ID_SHA1 == id)
			has_sha1 = 1;

		if (!sctp_hmac_list[id].hmac_name)
			return -EOPNOTSUPP;
	}

	if (!has_sha1)
		return -EINVAL;

	memcpy(ep->auth_hmacs_list->hmac_ids, &hmacs->shmac_idents[0],
		hmacs->shmac_num_idents * sizeof(__u16));
	ep->auth_hmacs_list->param_hdr.length = htons(sizeof(sctp_paramhdr_t) +
				hmacs->shmac_num_idents * sizeof(__u16));
	return 0;
}

int sctp_auth_set_key(struct sctp_endpoint *ep,
		      struct sctp_association *asoc,
		      struct sctp_authkey *auth_key)
{
	struct sctp_shared_key *cur_key = NULL;
	struct sctp_auth_bytes *key;
	struct list_head *sh_keys;
	int replace = 0;

	if (asoc)
		sh_keys = &asoc->endpoint_shared_keys;
	else
		sh_keys = &ep->endpoint_shared_keys;

	key_for_each(cur_key, sh_keys) {
		if (cur_key->key_id == auth_key->sca_keynumber) {
			replace = 1;
			break;
		}
	}

	if (!replace) {
		cur_key = sctp_auth_shkey_create(auth_key->sca_keynumber,
						 GFP_KERNEL);
		if (!cur_key)
			return -ENOMEM;
	}

	
	key = sctp_auth_create_key(auth_key->sca_keylength, GFP_KERNEL);
	if (!key)
		goto nomem;

	memcpy(key->data, &auth_key->sca_key[0], auth_key->sca_keylength);

	if (replace)
		sctp_auth_key_put(cur_key->key);
	else
		list_add(&cur_key->key_list, sh_keys);

	cur_key->key = key;
	sctp_auth_key_hold(key);

	return 0;
nomem:
	if (!replace)
		sctp_auth_shkey_free(cur_key);

	return -ENOMEM;
}

int sctp_auth_set_active_key(struct sctp_endpoint *ep,
			     struct sctp_association *asoc,
			     __u16  key_id)
{
	struct sctp_shared_key *key;
	struct list_head *sh_keys;
	int found = 0;

	
	if (asoc)
		sh_keys = &asoc->endpoint_shared_keys;
	else
		sh_keys = &ep->endpoint_shared_keys;

	key_for_each(key, sh_keys) {
		if (key->key_id == key_id) {
			found = 1;
			break;
		}
	}

	if (!found)
		return -EINVAL;

	if (asoc) {
		asoc->active_key_id = key_id;
		sctp_auth_asoc_init_active_key(asoc, GFP_KERNEL);
	} else
		ep->active_key_id = key_id;

	return 0;
}

int sctp_auth_del_key_id(struct sctp_endpoint *ep,
			 struct sctp_association *asoc,
			 __u16  key_id)
{
	struct sctp_shared_key *key;
	struct list_head *sh_keys;
	int found = 0;

	if (asoc) {
		if (asoc->active_key_id == key_id)
			return -EINVAL;

		sh_keys = &asoc->endpoint_shared_keys;
	} else {
		if (ep->active_key_id == key_id)
			return -EINVAL;

		sh_keys = &ep->endpoint_shared_keys;
	}

	key_for_each(key, sh_keys) {
		if (key->key_id == key_id) {
			found = 1;
			break;
		}
	}

	if (!found)
		return -EINVAL;

	
	list_del_init(&key->key_list);
	sctp_auth_shkey_free(key);

	return 0;
}
