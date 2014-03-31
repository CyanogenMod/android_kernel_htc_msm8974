/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation, version 2 of the
 *  License.
 */

#include <linux/export.h>
#include <linux/nsproxy.h>
#include <linux/slab.h>
#include <linux/user_namespace.h>
#include <linux/highuid.h>
#include <linux/cred.h>

static struct kmem_cache *user_ns_cachep __read_mostly;

int create_user_ns(struct cred *new)
{
	struct user_namespace *ns;
	struct user_struct *root_user;
	int n;

	ns = kmem_cache_alloc(user_ns_cachep, GFP_KERNEL);
	if (!ns)
		return -ENOMEM;

	kref_init(&ns->kref);

	for (n = 0; n < UIDHASH_SZ; ++n)
		INIT_HLIST_HEAD(ns->uidhash_table + n);

	
	root_user = alloc_uid(ns, 0);
	if (!root_user) {
		kmem_cache_free(user_ns_cachep, ns);
		return -ENOMEM;
	}

	
	ns->creator = new->user;
	new->user = root_user;
	new->uid = new->euid = new->suid = new->fsuid = 0;
	new->gid = new->egid = new->sgid = new->fsgid = 0;
	put_group_info(new->group_info);
	new->group_info = get_group_info(&init_groups);
#ifdef CONFIG_KEYS
	key_put(new->request_key_auth);
	new->request_key_auth = NULL;
#endif
	

	
	put_user_ns(ns);

	return 0;
}

static void free_user_ns_work(struct work_struct *work)
{
	struct user_namespace *ns =
		container_of(work, struct user_namespace, destroyer);
	free_uid(ns->creator);
	kmem_cache_free(user_ns_cachep, ns);
}

void free_user_ns(struct kref *kref)
{
	struct user_namespace *ns =
		container_of(kref, struct user_namespace, kref);

	INIT_WORK(&ns->destroyer, free_user_ns_work);
	schedule_work(&ns->destroyer);
}
EXPORT_SYMBOL(free_user_ns);

uid_t user_ns_map_uid(struct user_namespace *to, const struct cred *cred, uid_t uid)
{
	struct user_namespace *tmp;

	if (likely(to == cred->user->user_ns))
		return uid;


	for ( tmp = to; tmp != &init_user_ns;
	      tmp = tmp->creator->user_ns ) {
		if (cred->user == tmp->creator) {
			return (uid_t)0;
		}
	}

	
	return overflowuid;
}

gid_t user_ns_map_gid(struct user_namespace *to, const struct cred *cred, gid_t gid)
{
	struct user_namespace *tmp;

	if (likely(to == cred->user->user_ns))
		return gid;

	for ( tmp = to; tmp != &init_user_ns;
	      tmp = tmp->creator->user_ns ) {
		if (cred->user == tmp->creator) {
			return (gid_t)0;
		}
	}

	
	return overflowgid;
}

static __init int user_namespaces_init(void)
{
	user_ns_cachep = KMEM_CACHE(user_namespace, SLAB_PANIC);
	return 0;
}
module_init(user_namespaces_init);
