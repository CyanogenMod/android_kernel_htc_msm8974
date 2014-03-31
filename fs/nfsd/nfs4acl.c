/*
 *  Common NFSv4 ACL handling code.
 *
 *  Copyright (c) 2002, 2003 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Marius Aamodt Eriksen <marius@umich.edu>
 *  Jeff Sedlak <jsedlak@umich.edu>
 *  J. Bruce Fields <bfields@umich.edu>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the University nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/slab.h>
#include <linux/nfs_fs.h>
#include <linux/export.h>
#include "acl.h"


#define NFS4_READ_MODE (NFS4_ACE_READ_DATA)
#define NFS4_WRITE_MODE (NFS4_ACE_WRITE_DATA | NFS4_ACE_APPEND_DATA)
#define NFS4_EXECUTE_MODE NFS4_ACE_EXECUTE
#define NFS4_ANYONE_MODE (NFS4_ACE_READ_ATTRIBUTES | NFS4_ACE_READ_ACL | NFS4_ACE_SYNCHRONIZE)
#define NFS4_OWNER_MODE (NFS4_ACE_WRITE_ATTRIBUTES | NFS4_ACE_WRITE_ACL)

#define NFS4_MASK_UNSUPP (NFS4_ACE_DELETE | NFS4_ACE_WRITE_OWNER \
		| NFS4_ACE_READ_NAMED_ATTRS | NFS4_ACE_WRITE_NAMED_ATTRS)

#define NFS4_INHERITANCE_FLAGS (NFS4_ACE_FILE_INHERIT_ACE \
		| NFS4_ACE_DIRECTORY_INHERIT_ACE)

#define NFS4_SUPPORTED_FLAGS (NFS4_INHERITANCE_FLAGS \
		| NFS4_ACE_INHERIT_ONLY_ACE \
		| NFS4_ACE_IDENTIFIER_GROUP)

#define MASK_EQUAL(mask1, mask2) \
	( ((mask1) & NFS4_ACE_MASK_ALL) == ((mask2) & NFS4_ACE_MASK_ALL) )

static u32
mask_from_posix(unsigned short perm, unsigned int flags)
{
	int mask = NFS4_ANYONE_MODE;

	if (flags & NFS4_ACL_OWNER)
		mask |= NFS4_OWNER_MODE;
	if (perm & ACL_READ)
		mask |= NFS4_READ_MODE;
	if (perm & ACL_WRITE)
		mask |= NFS4_WRITE_MODE;
	if ((perm & ACL_WRITE) && (flags & NFS4_ACL_DIR))
		mask |= NFS4_ACE_DELETE_CHILD;
	if (perm & ACL_EXECUTE)
		mask |= NFS4_EXECUTE_MODE;
	return mask;
}

static u32
deny_mask_from_posix(unsigned short perm, u32 flags)
{
	u32 mask = 0;

	if (perm & ACL_READ)
		mask |= NFS4_READ_MODE;
	if (perm & ACL_WRITE)
		mask |= NFS4_WRITE_MODE;
	if ((perm & ACL_WRITE) && (flags & NFS4_ACL_DIR))
		mask |= NFS4_ACE_DELETE_CHILD;
	if (perm & ACL_EXECUTE)
		mask |= NFS4_EXECUTE_MODE;
	return mask;
}



static void
low_mode_from_nfs4(u32 perm, unsigned short *mode, unsigned int flags)
{
	u32 write_mode = NFS4_WRITE_MODE;

	if (flags & NFS4_ACL_DIR)
		write_mode |= NFS4_ACE_DELETE_CHILD;
	*mode = 0;
	if ((perm & NFS4_READ_MODE) == NFS4_READ_MODE)
		*mode |= ACL_READ;
	if ((perm & write_mode) == write_mode)
		*mode |= ACL_WRITE;
	if ((perm & NFS4_EXECUTE_MODE) == NFS4_EXECUTE_MODE)
		*mode |= ACL_EXECUTE;
}

struct ace_container {
	struct nfs4_ace  *ace;
	struct list_head  ace_l;
};

static short ace2type(struct nfs4_ace *);
static void _posix_to_nfsv4_one(struct posix_acl *, struct nfs4_acl *,
				unsigned int);

struct nfs4_acl *
nfs4_acl_posix_to_nfsv4(struct posix_acl *pacl, struct posix_acl *dpacl,
			unsigned int flags)
{
	struct nfs4_acl *acl;
	int size = 0;

	if (pacl) {
		if (posix_acl_valid(pacl) < 0)
			return ERR_PTR(-EINVAL);
		size += 2*pacl->a_count;
	}
	if (dpacl) {
		if (posix_acl_valid(dpacl) < 0)
			return ERR_PTR(-EINVAL);
		size += 2*dpacl->a_count;
	}

	
	acl = nfs4_acl_new(size);
	if (acl == NULL)
		return ERR_PTR(-ENOMEM);

	if (pacl)
		_posix_to_nfsv4_one(pacl, acl, flags & ~NFS4_ACL_TYPE_DEFAULT);

	if (dpacl)
		_posix_to_nfsv4_one(dpacl, acl, flags | NFS4_ACL_TYPE_DEFAULT);

	return acl;
}

struct posix_acl_summary {
	unsigned short owner;
	unsigned short users;
	unsigned short group;
	unsigned short groups;
	unsigned short other;
	unsigned short mask;
};

static void
summarize_posix_acl(struct posix_acl *acl, struct posix_acl_summary *pas)
{
	struct posix_acl_entry *pa, *pe;

	memset(pas, 0, sizeof(*pas));
	pas->mask = 07;

	pe = acl->a_entries + acl->a_count;

	FOREACH_ACL_ENTRY(pa, acl, pe) {
		switch (pa->e_tag) {
			case ACL_USER_OBJ:
				pas->owner = pa->e_perm;
				break;
			case ACL_GROUP_OBJ:
				pas->group = pa->e_perm;
				break;
			case ACL_USER:
				pas->users |= pa->e_perm;
				break;
			case ACL_GROUP:
				pas->groups |= pa->e_perm;
				break;
			case ACL_OTHER:
				pas->other = pa->e_perm;
				break;
			case ACL_MASK:
				pas->mask = pa->e_perm;
				break;
		}
	}
	
	pas->users &= pas->mask;
	pas->group &= pas->mask;
	pas->groups &= pas->mask;
}

static void
_posix_to_nfsv4_one(struct posix_acl *pacl, struct nfs4_acl *acl,
						unsigned int flags)
{
	struct posix_acl_entry *pa, *group_owner_entry;
	struct nfs4_ace *ace;
	struct posix_acl_summary pas;
	unsigned short deny;
	int eflag = ((flags & NFS4_ACL_TYPE_DEFAULT) ?
		NFS4_INHERITANCE_FLAGS | NFS4_ACE_INHERIT_ONLY_ACE : 0);

	BUG_ON(pacl->a_count < 3);
	summarize_posix_acl(pacl, &pas);

	pa = pacl->a_entries;
	ace = acl->aces + acl->naces;

	
	deny = ~pas.owner;
	deny &= pas.users | pas.group | pas.groups | pas.other;
	if (deny) {
		ace->type = NFS4_ACE_ACCESS_DENIED_ACE_TYPE;
		ace->flag = eflag;
		ace->access_mask = deny_mask_from_posix(deny, flags);
		ace->whotype = NFS4_ACL_WHO_OWNER;
		ace++;
		acl->naces++;
	}

	ace->type = NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE;
	ace->flag = eflag;
	ace->access_mask = mask_from_posix(pa->e_perm, flags | NFS4_ACL_OWNER);
	ace->whotype = NFS4_ACL_WHO_OWNER;
	ace++;
	acl->naces++;
	pa++;

	while (pa->e_tag == ACL_USER) {
		deny = ~(pa->e_perm & pas.mask);
		deny &= pas.groups | pas.group | pas.other;
		if (deny) {
			ace->type = NFS4_ACE_ACCESS_DENIED_ACE_TYPE;
			ace->flag = eflag;
			ace->access_mask = deny_mask_from_posix(deny, flags);
			ace->whotype = NFS4_ACL_WHO_NAMED;
			ace->who = pa->e_id;
			ace++;
			acl->naces++;
		}
		ace->type = NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE;
		ace->flag = eflag;
		ace->access_mask = mask_from_posix(pa->e_perm & pas.mask,
						   flags);
		ace->whotype = NFS4_ACL_WHO_NAMED;
		ace->who = pa->e_id;
		ace++;
		acl->naces++;
		pa++;
	}


	

	group_owner_entry = pa;

	ace->type = NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE;
	ace->flag = eflag;
	ace->access_mask = mask_from_posix(pas.group, flags);
	ace->whotype = NFS4_ACL_WHO_GROUP;
	ace++;
	acl->naces++;
	pa++;

	while (pa->e_tag == ACL_GROUP) {
		ace->type = NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE;
		ace->flag = eflag | NFS4_ACE_IDENTIFIER_GROUP;
		ace->access_mask = mask_from_posix(pa->e_perm & pas.mask,
						   flags);
		ace->whotype = NFS4_ACL_WHO_NAMED;
		ace->who = pa->e_id;
		ace++;
		acl->naces++;
		pa++;
	}

	

	pa = group_owner_entry;

	deny = ~pas.group & pas.other;
	if (deny) {
		ace->type = NFS4_ACE_ACCESS_DENIED_ACE_TYPE;
		ace->flag = eflag;
		ace->access_mask = deny_mask_from_posix(deny, flags);
		ace->whotype = NFS4_ACL_WHO_GROUP;
		ace++;
		acl->naces++;
	}
	pa++;

	while (pa->e_tag == ACL_GROUP) {
		deny = ~(pa->e_perm & pas.mask);
		deny &= pas.other;
		if (deny) {
			ace->type = NFS4_ACE_ACCESS_DENIED_ACE_TYPE;
			ace->flag = eflag | NFS4_ACE_IDENTIFIER_GROUP;
			ace->access_mask = deny_mask_from_posix(deny, flags);
			ace->whotype = NFS4_ACL_WHO_NAMED;
			ace->who = pa->e_id;
			ace++;
			acl->naces++;
		}
		pa++;
	}

	if (pa->e_tag == ACL_MASK)
		pa++;
	ace->type = NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE;
	ace->flag = eflag;
	ace->access_mask = mask_from_posix(pa->e_perm, flags);
	ace->whotype = NFS4_ACL_WHO_EVERYONE;
	acl->naces++;
}

static void
sort_pacl_range(struct posix_acl *pacl, int start, int end) {
	int sorted = 0, i;
	struct posix_acl_entry tmp;

	while (!sorted) {
		sorted = 1;
		for (i = start; i < end; i++) {
			if (pacl->a_entries[i].e_id
					> pacl->a_entries[i+1].e_id) {
				sorted = 0;
				tmp = pacl->a_entries[i];
				pacl->a_entries[i] = pacl->a_entries[i+1];
				pacl->a_entries[i+1] = tmp;
			}
		}
	}
}

static void
sort_pacl(struct posix_acl *pacl)
{
	int i, j;

	if (pacl->a_count <= 4)
		return; 
	i = 1;
	while (pacl->a_entries[i].e_tag == ACL_USER)
		i++;
	sort_pacl_range(pacl, 1, i-1);

	BUG_ON(pacl->a_entries[i].e_tag != ACL_GROUP_OBJ);
	j = ++i;
	while (pacl->a_entries[j].e_tag == ACL_GROUP)
		j++;
	sort_pacl_range(pacl, i, j-1);
	return;
}

struct posix_ace_state {
	u32 allow;
	u32 deny;
};

struct posix_user_ace_state {
	uid_t uid;
	struct posix_ace_state perms;
};

struct posix_ace_state_array {
	int n;
	struct posix_user_ace_state aces[];
};


struct posix_acl_state {
	int empty;
	struct posix_ace_state owner;
	struct posix_ace_state group;
	struct posix_ace_state other;
	struct posix_ace_state everyone;
	struct posix_ace_state mask; 
	struct posix_ace_state_array *users;
	struct posix_ace_state_array *groups;
};

static int
init_state(struct posix_acl_state *state, int cnt)
{
	int alloc;

	memset(state, 0, sizeof(struct posix_acl_state));
	state->empty = 1;
	alloc = sizeof(struct posix_ace_state_array)
		+ cnt*sizeof(struct posix_user_ace_state);
	state->users = kzalloc(alloc, GFP_KERNEL);
	if (!state->users)
		return -ENOMEM;
	state->groups = kzalloc(alloc, GFP_KERNEL);
	if (!state->groups) {
		kfree(state->users);
		return -ENOMEM;
	}
	return 0;
}

static void
free_state(struct posix_acl_state *state) {
	kfree(state->users);
	kfree(state->groups);
}

static inline void add_to_mask(struct posix_acl_state *state, struct posix_ace_state *astate)
{
	state->mask.allow |= astate->allow;
}


static inline int check_deny(u32 mask, int isowner)
{
	if (mask & (NFS4_ACE_READ_ATTRIBUTES | NFS4_ACE_READ_ACL))
		return -EINVAL;
	if (!isowner)
		return 0;
	if (mask & (NFS4_ACE_WRITE_ATTRIBUTES | NFS4_ACE_WRITE_ACL))
		return -EINVAL;
	return 0;
}

static struct posix_acl *
posix_state_to_acl(struct posix_acl_state *state, unsigned int flags)
{
	struct posix_acl_entry *pace;
	struct posix_acl *pacl;
	int nace;
	int i, error = 0;

	if (state->empty && (flags & NFS4_ACL_TYPE_DEFAULT)) {
		pacl = posix_acl_alloc(0, GFP_KERNEL);
		return pacl ? pacl : ERR_PTR(-ENOMEM);
	}
	nace = 4 + state->users->n + state->groups->n;
	pacl = posix_acl_alloc(nace, GFP_KERNEL);
	if (!pacl)
		return ERR_PTR(-ENOMEM);

	pace = pacl->a_entries;
	pace->e_tag = ACL_USER_OBJ;
	error = check_deny(state->owner.deny, 1);
	if (error)
		goto out_err;
	low_mode_from_nfs4(state->owner.allow, &pace->e_perm, flags);
	pace->e_id = ACL_UNDEFINED_ID;

	for (i=0; i < state->users->n; i++) {
		pace++;
		pace->e_tag = ACL_USER;
		error = check_deny(state->users->aces[i].perms.deny, 0);
		if (error)
			goto out_err;
		low_mode_from_nfs4(state->users->aces[i].perms.allow,
					&pace->e_perm, flags);
		pace->e_id = state->users->aces[i].uid;
		add_to_mask(state, &state->users->aces[i].perms);
	}

	pace++;
	pace->e_tag = ACL_GROUP_OBJ;
	error = check_deny(state->group.deny, 0);
	if (error)
		goto out_err;
	low_mode_from_nfs4(state->group.allow, &pace->e_perm, flags);
	pace->e_id = ACL_UNDEFINED_ID;
	add_to_mask(state, &state->group);

	for (i=0; i < state->groups->n; i++) {
		pace++;
		pace->e_tag = ACL_GROUP;
		error = check_deny(state->groups->aces[i].perms.deny, 0);
		if (error)
			goto out_err;
		low_mode_from_nfs4(state->groups->aces[i].perms.allow,
					&pace->e_perm, flags);
		pace->e_id = state->groups->aces[i].uid;
		add_to_mask(state, &state->groups->aces[i].perms);
	}

	pace++;
	pace->e_tag = ACL_MASK;
	low_mode_from_nfs4(state->mask.allow, &pace->e_perm, flags);
	pace->e_id = ACL_UNDEFINED_ID;

	pace++;
	pace->e_tag = ACL_OTHER;
	error = check_deny(state->other.deny, 0);
	if (error)
		goto out_err;
	low_mode_from_nfs4(state->other.allow, &pace->e_perm, flags);
	pace->e_id = ACL_UNDEFINED_ID;

	return pacl;
out_err:
	posix_acl_release(pacl);
	return ERR_PTR(error);
}

static inline void allow_bits(struct posix_ace_state *astate, u32 mask)
{
	
	astate->allow |= mask & ~astate->deny;
}

static inline void deny_bits(struct posix_ace_state *astate, u32 mask)
{
	
	astate->deny |= mask & ~astate->allow;
}

static int find_uid(struct posix_acl_state *state, struct posix_ace_state_array *a, uid_t uid)
{
	int i;

	for (i = 0; i < a->n; i++)
		if (a->aces[i].uid == uid)
			return i;
	
	a->n++;
	a->aces[i].uid = uid;
	a->aces[i].perms.allow = state->everyone.allow;
	a->aces[i].perms.deny  = state->everyone.deny;

	return i;
}

static void deny_bits_array(struct posix_ace_state_array *a, u32 mask)
{
	int i;

	for (i=0; i < a->n; i++)
		deny_bits(&a->aces[i].perms, mask);
}

static void allow_bits_array(struct posix_ace_state_array *a, u32 mask)
{
	int i;

	for (i=0; i < a->n; i++)
		allow_bits(&a->aces[i].perms, mask);
}

static void process_one_v4_ace(struct posix_acl_state *state,
				struct nfs4_ace *ace)
{
	u32 mask = ace->access_mask;
	int i;

	state->empty = 0;

	switch (ace2type(ace)) {
	case ACL_USER_OBJ:
		if (ace->type == NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE) {
			allow_bits(&state->owner, mask);
		} else {
			deny_bits(&state->owner, mask);
		}
		break;
	case ACL_USER:
		i = find_uid(state, state->users, ace->who);
		if (ace->type == NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE) {
			allow_bits(&state->users->aces[i].perms, mask);
		} else {
			deny_bits(&state->users->aces[i].perms, mask);
			mask = state->users->aces[i].perms.deny;
			deny_bits(&state->owner, mask);
		}
		break;
	case ACL_GROUP_OBJ:
		if (ace->type == NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE) {
			allow_bits(&state->group, mask);
		} else {
			deny_bits(&state->group, mask);
			mask = state->group.deny;
			deny_bits(&state->owner, mask);
			deny_bits(&state->everyone, mask);
			deny_bits_array(state->users, mask);
			deny_bits_array(state->groups, mask);
		}
		break;
	case ACL_GROUP:
		i = find_uid(state, state->groups, ace->who);
		if (ace->type == NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE) {
			allow_bits(&state->groups->aces[i].perms, mask);
		} else {
			deny_bits(&state->groups->aces[i].perms, mask);
			mask = state->groups->aces[i].perms.deny;
			deny_bits(&state->owner, mask);
			deny_bits(&state->group, mask);
			deny_bits(&state->everyone, mask);
			deny_bits_array(state->users, mask);
			deny_bits_array(state->groups, mask);
		}
		break;
	case ACL_OTHER:
		if (ace->type == NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE) {
			allow_bits(&state->owner, mask);
			allow_bits(&state->group, mask);
			allow_bits(&state->other, mask);
			allow_bits(&state->everyone, mask);
			allow_bits_array(state->users, mask);
			allow_bits_array(state->groups, mask);
		} else {
			deny_bits(&state->owner, mask);
			deny_bits(&state->group, mask);
			deny_bits(&state->other, mask);
			deny_bits(&state->everyone, mask);
			deny_bits_array(state->users, mask);
			deny_bits_array(state->groups, mask);
		}
	}
}

int nfs4_acl_nfsv4_to_posix(struct nfs4_acl *acl, struct posix_acl **pacl,
			    struct posix_acl **dpacl, unsigned int flags)
{
	struct posix_acl_state effective_acl_state, default_acl_state;
	struct nfs4_ace *ace;
	int ret;

	ret = init_state(&effective_acl_state, acl->naces);
	if (ret)
		return ret;
	ret = init_state(&default_acl_state, acl->naces);
	if (ret)
		goto out_estate;
	ret = -EINVAL;
	for (ace = acl->aces; ace < acl->aces + acl->naces; ace++) {
		if (ace->type != NFS4_ACE_ACCESS_ALLOWED_ACE_TYPE &&
		    ace->type != NFS4_ACE_ACCESS_DENIED_ACE_TYPE)
			goto out_dstate;
		if (ace->flag & ~NFS4_SUPPORTED_FLAGS)
			goto out_dstate;
		if ((ace->flag & NFS4_INHERITANCE_FLAGS) == 0) {
			process_one_v4_ace(&effective_acl_state, ace);
			continue;
		}
		if (!(flags & NFS4_ACL_DIR))
			goto out_dstate;
		process_one_v4_ace(&default_acl_state, ace);

		if (!(ace->flag & NFS4_ACE_INHERIT_ONLY_ACE))
			process_one_v4_ace(&effective_acl_state, ace);
	}
	*pacl = posix_state_to_acl(&effective_acl_state, flags);
	if (IS_ERR(*pacl)) {
		ret = PTR_ERR(*pacl);
		*pacl = NULL;
		goto out_dstate;
	}
	*dpacl = posix_state_to_acl(&default_acl_state,
						flags | NFS4_ACL_TYPE_DEFAULT);
	if (IS_ERR(*dpacl)) {
		ret = PTR_ERR(*dpacl);
		*dpacl = NULL;
		posix_acl_release(*pacl);
		*pacl = NULL;
		goto out_dstate;
	}
	sort_pacl(*pacl);
	sort_pacl(*dpacl);
	ret = 0;
out_dstate:
	free_state(&default_acl_state);
out_estate:
	free_state(&effective_acl_state);
	return ret;
}

static short
ace2type(struct nfs4_ace *ace)
{
	switch (ace->whotype) {
		case NFS4_ACL_WHO_NAMED:
			return (ace->flag & NFS4_ACE_IDENTIFIER_GROUP ?
					ACL_GROUP : ACL_USER);
		case NFS4_ACL_WHO_OWNER:
			return ACL_USER_OBJ;
		case NFS4_ACL_WHO_GROUP:
			return ACL_GROUP_OBJ;
		case NFS4_ACL_WHO_EVERYONE:
			return ACL_OTHER;
	}
	BUG();
	return -1;
}

EXPORT_SYMBOL(nfs4_acl_posix_to_nfsv4);
EXPORT_SYMBOL(nfs4_acl_nfsv4_to_posix);

struct nfs4_acl *
nfs4_acl_new(int n)
{
	struct nfs4_acl *acl;

	acl = kmalloc(sizeof(*acl) + n*sizeof(struct nfs4_ace), GFP_KERNEL);
	if (acl == NULL)
		return NULL;
	acl->naces = 0;
	return acl;
}

static struct {
	char *string;
	int   stringlen;
	int type;
} s2t_map[] = {
	{
		.string    = "OWNER@",
		.stringlen = sizeof("OWNER@") - 1,
		.type      = NFS4_ACL_WHO_OWNER,
	},
	{
		.string    = "GROUP@",
		.stringlen = sizeof("GROUP@") - 1,
		.type      = NFS4_ACL_WHO_GROUP,
	},
	{
		.string    = "EVERYONE@",
		.stringlen = sizeof("EVERYONE@") - 1,
		.type      = NFS4_ACL_WHO_EVERYONE,
	},
};

int
nfs4_acl_get_whotype(char *p, u32 len)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(s2t_map); i++) {
		if (s2t_map[i].stringlen == len &&
				0 == memcmp(s2t_map[i].string, p, len))
			return s2t_map[i].type;
	}
	return NFS4_ACL_WHO_NAMED;
}

int
nfs4_acl_write_who(int who, char *p)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(s2t_map); i++) {
		if (s2t_map[i].type == who) {
			memcpy(p, s2t_map[i].string, s2t_map[i].stringlen);
			return s2t_map[i].stringlen;
		}
	}
	BUG();
	return -1;
}

EXPORT_SYMBOL(nfs4_acl_new);
EXPORT_SYMBOL(nfs4_acl_get_whotype);
EXPORT_SYMBOL(nfs4_acl_write_who);
