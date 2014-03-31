/*
 * AppArmor security module
 *
 * This file contains AppArmor functions for unpacking policy loaded from
 * userspace.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2010 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * AppArmor uses a serialized binary format for loading policy. To find
 * policy format documentation look in Documentation/security/apparmor.txt
 * All policy is validated before it is used.
 */

#include <asm/unaligned.h>
#include <linux/ctype.h>
#include <linux/errno.h>

#include "include/apparmor.h"
#include "include/audit.h"
#include "include/context.h"
#include "include/match.h"
#include "include/policy.h"
#include "include/policy_unpack.h"
#include "include/sid.h"


enum aa_code {
	AA_U8,
	AA_U16,
	AA_U32,
	AA_U64,
	AA_NAME,		
	AA_STRING,
	AA_BLOB,
	AA_STRUCT,
	AA_STRUCTEND,
	AA_LIST,
	AA_LISTEND,
	AA_ARRAY,
	AA_ARRAYEND,
};

struct aa_ext {
	void *start;
	void *end;
	void *pos;		
	u32 version;
};

static void audit_cb(struct audit_buffer *ab, void *va)
{
	struct common_audit_data *sa = va;
	if (sa->aad->iface.target) {
		struct aa_profile *name = sa->aad->iface.target;
		audit_log_format(ab, " name=");
		audit_log_untrustedstring(ab, name->base.hname);
	}
	if (sa->aad->iface.pos)
		audit_log_format(ab, " offset=%ld", sa->aad->iface.pos);
}

static int audit_iface(struct aa_profile *new, const char *name,
		       const char *info, struct aa_ext *e, int error)
{
	struct aa_profile *profile = __aa_current_profile();
	struct common_audit_data sa;
	struct apparmor_audit_data aad = {0,};
	COMMON_AUDIT_DATA_INIT(&sa, NONE);
	sa.aad = &aad;
	if (e)
		aad.iface.pos = e->pos - e->start;
	aad.iface.target = new;
	aad.name = name;
	aad.info = info;
	aad.error = error;

	return aa_audit(AUDIT_APPARMOR_STATUS, profile, GFP_KERNEL, &sa,
			audit_cb);
}

static bool inbounds(struct aa_ext *e, size_t size)
{
	return (size <= e->end - e->pos);
}

static size_t unpack_u16_chunk(struct aa_ext *e, char **chunk)
{
	size_t size = 0;

	if (!inbounds(e, sizeof(u16)))
		return 0;
	size = le16_to_cpu(get_unaligned((u16 *) e->pos));
	e->pos += sizeof(u16);
	if (!inbounds(e, size))
		return 0;
	*chunk = e->pos;
	e->pos += size;
	return size;
}

static bool unpack_X(struct aa_ext *e, enum aa_code code)
{
	if (!inbounds(e, 1))
		return 0;
	if (*(u8 *) e->pos != code)
		return 0;
	e->pos++;
	return 1;
}

static bool unpack_nameX(struct aa_ext *e, enum aa_code code, const char *name)
{
	void *pos = e->pos;
	if (unpack_X(e, AA_NAME)) {
		char *tag = NULL;
		size_t size = unpack_u16_chunk(e, &tag);
		
		if (name && (!size || strcmp(name, tag)))
			goto fail;
	} else if (name) {
		
		goto fail;
	}

	
	if (unpack_X(e, code))
		return 1;

fail:
	e->pos = pos;
	return 0;
}

static bool unpack_u32(struct aa_ext *e, u32 *data, const char *name)
{
	if (unpack_nameX(e, AA_U32, name)) {
		if (!inbounds(e, sizeof(u32)))
			return 0;
		if (data)
			*data = le32_to_cpu(get_unaligned((u32 *) e->pos));
		e->pos += sizeof(u32);
		return 1;
	}
	return 0;
}

static bool unpack_u64(struct aa_ext *e, u64 *data, const char *name)
{
	if (unpack_nameX(e, AA_U64, name)) {
		if (!inbounds(e, sizeof(u64)))
			return 0;
		if (data)
			*data = le64_to_cpu(get_unaligned((u64 *) e->pos));
		e->pos += sizeof(u64);
		return 1;
	}
	return 0;
}

static size_t unpack_array(struct aa_ext *e, const char *name)
{
	if (unpack_nameX(e, AA_ARRAY, name)) {
		int size;
		if (!inbounds(e, sizeof(u16)))
			return 0;
		size = (int)le16_to_cpu(get_unaligned((u16 *) e->pos));
		e->pos += sizeof(u16);
		return size;
	}
	return 0;
}

static size_t unpack_blob(struct aa_ext *e, char **blob, const char *name)
{
	if (unpack_nameX(e, AA_BLOB, name)) {
		u32 size;
		if (!inbounds(e, sizeof(u32)))
			return 0;
		size = le32_to_cpu(get_unaligned((u32 *) e->pos));
		e->pos += sizeof(u32);
		if (inbounds(e, (size_t) size)) {
			*blob = e->pos;
			e->pos += size;
			return size;
		}
	}
	return 0;
}

static int unpack_str(struct aa_ext *e, const char **string, const char *name)
{
	char *src_str;
	size_t size = 0;
	void *pos = e->pos;
	*string = NULL;
	if (unpack_nameX(e, AA_STRING, name)) {
		size = unpack_u16_chunk(e, &src_str);
		if (size) {
			
			if (src_str[size - 1] != 0)
				goto fail;
			*string = src_str;
		}
	}
	return size;

fail:
	e->pos = pos;
	return 0;
}

static int unpack_strdup(struct aa_ext *e, char **string, const char *name)
{
	const char *tmp;
	void *pos = e->pos;
	int res = unpack_str(e, &tmp, name);
	*string = NULL;

	if (!res)
		return 0;

	*string = kmemdup(tmp, res, GFP_KERNEL);
	if (!*string) {
		e->pos = pos;
		return 0;
	}

	return res;
}

static bool verify_accept(struct aa_dfa *dfa, int flags)
{
	int i;

	
	for (i = 0; i < dfa->tables[YYTD_ID_ACCEPT]->td_lolen; i++) {
		int mode = ACCEPT_TABLE(dfa)[i];

		if (mode & ~DFA_VALID_PERM_MASK)
			return 0;

		if (ACCEPT_TABLE2(dfa)[i] & ~DFA_VALID_PERM2_MASK)
			return 0;
	}
	return 1;
}

static struct aa_dfa *unpack_dfa(struct aa_ext *e)
{
	char *blob = NULL;
	size_t size;
	struct aa_dfa *dfa = NULL;

	size = unpack_blob(e, &blob, "aadfa");
	if (size) {
		size_t sz = blob - (char *)e->start;
		size_t pad = ALIGN(sz, 8) - sz;
		int flags = TO_ACCEPT1_FLAG(YYTD_DATA32) |
			TO_ACCEPT2_FLAG(YYTD_DATA32);


		if (aa_g_paranoid_load)
			flags |= DFA_FLAG_VERIFY_STATES;

		dfa = aa_dfa_unpack(blob + pad, size - pad, flags);

		if (IS_ERR(dfa))
			return dfa;

		if (!verify_accept(dfa, flags))
			goto fail;
	}

	return dfa;

fail:
	aa_put_dfa(dfa);
	return ERR_PTR(-EPROTO);
}

static bool unpack_trans_table(struct aa_ext *e, struct aa_profile *profile)
{
	void *pos = e->pos;

	
	if (unpack_nameX(e, AA_STRUCT, "xtable")) {
		int i, size;

		size = unpack_array(e, NULL);
		
		if (size > 16 - 4)
			goto fail;
		profile->file.trans.table = kzalloc(sizeof(char *) * size,
						    GFP_KERNEL);
		if (!profile->file.trans.table)
			goto fail;

		profile->file.trans.size = size;
		for (i = 0; i < size; i++) {
			char *str;
			int c, j, size2 = unpack_strdup(e, &str, NULL);
			if (!size2)
				goto fail;
			profile->file.trans.table[i] = str;
			
			if (isspace(*str))
				goto fail;

			
			for (c = j = 0; j < size2 - 2; j++) {
				if (!str[j])
					c++;
			}
			if (*str == ':') {
				if (c != 1)
					goto fail;
				
				if (!str[1])
					goto fail;
			} else if (c)
				
				goto fail;
		}
		if (!unpack_nameX(e, AA_ARRAYEND, NULL))
			goto fail;
		if (!unpack_nameX(e, AA_STRUCTEND, NULL))
			goto fail;
	}
	return 1;

fail:
	aa_free_domain_entries(&profile->file.trans);
	e->pos = pos;
	return 0;
}

static bool unpack_rlimits(struct aa_ext *e, struct aa_profile *profile)
{
	void *pos = e->pos;

	
	if (unpack_nameX(e, AA_STRUCT, "rlimits")) {
		int i, size;
		u32 tmp = 0;
		if (!unpack_u32(e, &tmp, NULL))
			goto fail;
		profile->rlimits.mask = tmp;

		size = unpack_array(e, NULL);
		if (size > RLIM_NLIMITS)
			goto fail;
		for (i = 0; i < size; i++) {
			u64 tmp2 = 0;
			int a = aa_map_resource(i);
			if (!unpack_u64(e, &tmp2, NULL))
				goto fail;
			profile->rlimits.limits[a].rlim_max = tmp2;
		}
		if (!unpack_nameX(e, AA_ARRAYEND, NULL))
			goto fail;
		if (!unpack_nameX(e, AA_STRUCTEND, NULL))
			goto fail;
	}
	return 1;

fail:
	e->pos = pos;
	return 0;
}

static struct aa_profile *unpack_profile(struct aa_ext *e)
{
	struct aa_profile *profile = NULL;
	const char *name = NULL;
	int i, error = -EPROTO;
	kernel_cap_t tmpcap;
	u32 tmp;

	
	if (!unpack_nameX(e, AA_STRUCT, "profile"))
		goto fail;
	if (!unpack_str(e, &name, NULL))
		goto fail;

	profile = aa_alloc_profile(name);
	if (!profile)
		return ERR_PTR(-ENOMEM);

	
	(void) unpack_str(e, &profile->rename, "rename");

	
	profile->xmatch = unpack_dfa(e);
	if (IS_ERR(profile->xmatch)) {
		error = PTR_ERR(profile->xmatch);
		profile->xmatch = NULL;
		goto fail;
	}
	
	if (profile->xmatch) {
		if (!unpack_u32(e, &tmp, NULL))
			goto fail;
		profile->xmatch_len = tmp;
	}

	
	if (!unpack_nameX(e, AA_STRUCT, "flags"))
		goto fail;
	if (!unpack_u32(e, &tmp, NULL))
		goto fail;
	if (tmp)
		profile->flags |= PFLAG_HAT;
	if (!unpack_u32(e, &tmp, NULL))
		goto fail;
	if (tmp)
		profile->mode = APPARMOR_COMPLAIN;
	if (!unpack_u32(e, &tmp, NULL))
		goto fail;
	if (tmp)
		profile->audit = AUDIT_ALL;

	if (!unpack_nameX(e, AA_STRUCTEND, NULL))
		goto fail;

	
	if (unpack_u32(e, &profile->path_flags, "path_flags"))
		profile->path_flags |= profile->flags & PFLAG_MEDIATE_DELETED;
	else
		
		profile->path_flags = PFLAG_MEDIATE_DELETED;

	if (!unpack_u32(e, &(profile->caps.allow.cap[0]), NULL))
		goto fail;
	if (!unpack_u32(e, &(profile->caps.audit.cap[0]), NULL))
		goto fail;
	if (!unpack_u32(e, &(profile->caps.quiet.cap[0]), NULL))
		goto fail;
	if (!unpack_u32(e, &tmpcap.cap[0], NULL))
		goto fail;

	if (unpack_nameX(e, AA_STRUCT, "caps64")) {
		
		if (!unpack_u32(e, &(profile->caps.allow.cap[1]), NULL))
			goto fail;
		if (!unpack_u32(e, &(profile->caps.audit.cap[1]), NULL))
			goto fail;
		if (!unpack_u32(e, &(profile->caps.quiet.cap[1]), NULL))
			goto fail;
		if (!unpack_u32(e, &(tmpcap.cap[1]), NULL))
			goto fail;
		if (!unpack_nameX(e, AA_STRUCTEND, NULL))
			goto fail;
	}

	if (unpack_nameX(e, AA_STRUCT, "capsx")) {
		
		if (!unpack_u32(e, &(profile->caps.extended.cap[0]), NULL))
			goto fail;
		if (!unpack_u32(e, &(profile->caps.extended.cap[1]), NULL))
			goto fail;
		if (!unpack_nameX(e, AA_STRUCTEND, NULL))
			goto fail;
	}

	if (!unpack_rlimits(e, profile))
		goto fail;

	if (unpack_nameX(e, AA_STRUCT, "policydb")) {
		
		profile->policy.dfa = unpack_dfa(e);
		if (IS_ERR(profile->policy.dfa)) {
			error = PTR_ERR(profile->policy.dfa);
			profile->policy.dfa = NULL;
			goto fail;
		}
		if (!unpack_u32(e, &profile->policy.start[0], "start"))
			
			profile->policy.start[0] = DFA_START;
		
		for (i = AA_CLASS_FILE; i <= AA_CLASS_LAST; i++) {
			profile->policy.start[i] =
				aa_dfa_next(profile->policy.dfa,
					    profile->policy.start[0],
					    i);
		}
		if (!unpack_nameX(e, AA_STRUCTEND, NULL))
			goto fail;
	}

	
	profile->file.dfa = unpack_dfa(e);
	if (IS_ERR(profile->file.dfa)) {
		error = PTR_ERR(profile->file.dfa);
		profile->file.dfa = NULL;
		goto fail;
	}

	if (!unpack_u32(e, &profile->file.start, "dfa_start"))
		
		profile->file.start = DFA_START;

	if (!unpack_trans_table(e, profile))
		goto fail;

	if (!unpack_nameX(e, AA_STRUCTEND, NULL))
		goto fail;

	return profile;

fail:
	if (profile)
		name = NULL;
	else if (!name)
		name = "unknown";
	audit_iface(profile, name, "failed to unpack profile", e, error);
	aa_put_profile(profile);

	return ERR_PTR(error);
}

static int verify_header(struct aa_ext *e, const char **ns)
{
	int error = -EPROTONOSUPPORT;
	
	if (!unpack_u32(e, &e->version, "version")) {
		audit_iface(NULL, NULL, "invalid profile format", e, error);
		return error;
	}

	
	if (e->version != 5) {
		audit_iface(NULL, NULL, "unsupported interface version", e,
			    error);
		return error;
	}

	
	if (!unpack_str(e, ns, "namespace"))
		*ns = NULL;

	return 0;
}

static bool verify_xindex(int xindex, int table_size)
{
	int index, xtype;
	xtype = xindex & AA_X_TYPE_MASK;
	index = xindex & AA_X_INDEX_MASK;
	if (xtype == AA_X_TABLE && index > table_size)
		return 0;
	return 1;
}

static bool verify_dfa_xindex(struct aa_dfa *dfa, int table_size)
{
	int i;
	for (i = 0; i < dfa->tables[YYTD_ID_ACCEPT]->td_lolen; i++) {
		if (!verify_xindex(dfa_user_xindex(dfa, i), table_size))
			return 0;
		if (!verify_xindex(dfa_other_xindex(dfa, i), table_size))
			return 0;
	}
	return 1;
}

static int verify_profile(struct aa_profile *profile)
{
	if (aa_g_paranoid_load) {
		if (profile->file.dfa &&
		    !verify_dfa_xindex(profile->file.dfa,
				       profile->file.trans.size)) {
			audit_iface(profile, NULL, "Invalid named transition",
				    NULL, -EPROTO);
			return -EPROTO;
		}
	}

	return 0;
}

struct aa_profile *aa_unpack(void *udata, size_t size, const char **ns)
{
	struct aa_profile *profile = NULL;
	int error;
	struct aa_ext e = {
		.start = udata,
		.end = udata + size,
		.pos = udata,
	};

	error = verify_header(&e, ns);
	if (error)
		return ERR_PTR(error);

	profile = unpack_profile(&e);
	if (IS_ERR(profile))
		return profile;

	error = verify_profile(profile);
	if (error) {
		aa_put_profile(profile);
		profile = ERR_PTR(error);
	}

	
	return profile;
}
