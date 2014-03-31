/*
 * security/tomoyo/domain.c
 *
 * Copyright (C) 2005-2011  NTT DATA CORPORATION
 */

#include "common.h"
#include <linux/binfmts.h>
#include <linux/slab.h>


struct tomoyo_domain_info tomoyo_kernel_domain;

int tomoyo_update_policy(struct tomoyo_acl_head *new_entry, const int size,
			 struct tomoyo_acl_param *param,
			 bool (*check_duplicate) (const struct tomoyo_acl_head
						  *,
						  const struct tomoyo_acl_head
						  *))
{
	int error = param->is_delete ? -ENOENT : -ENOMEM;
	struct tomoyo_acl_head *entry;
	struct list_head *list = param->list;

	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		return -ENOMEM;
	list_for_each_entry_rcu(entry, list, list) {
		if (entry->is_deleted == TOMOYO_GC_IN_PROGRESS)
			continue;
		if (!check_duplicate(entry, new_entry))
			continue;
		entry->is_deleted = param->is_delete;
		error = 0;
		break;
	}
	if (error && !param->is_delete) {
		entry = tomoyo_commit_ok(new_entry, size);
		if (entry) {
			list_add_tail_rcu(&entry->list, list);
			error = 0;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
	return error;
}

static inline bool tomoyo_same_acl_head(const struct tomoyo_acl_info *a,
					const struct tomoyo_acl_info *b)
{
	return a->type == b->type && a->cond == b->cond;
}

int tomoyo_update_domain(struct tomoyo_acl_info *new_entry, const int size,
			 struct tomoyo_acl_param *param,
			 bool (*check_duplicate) (const struct tomoyo_acl_info
						  *,
						  const struct tomoyo_acl_info
						  *),
			 bool (*merge_duplicate) (struct tomoyo_acl_info *,
						  struct tomoyo_acl_info *,
						  const bool))
{
	const bool is_delete = param->is_delete;
	int error = is_delete ? -ENOENT : -ENOMEM;
	struct tomoyo_acl_info *entry;
	struct list_head * const list = param->list;

	if (param->data[0]) {
		new_entry->cond = tomoyo_get_condition(param);
		if (!new_entry->cond)
			return -EINVAL;
		if (new_entry->cond->transit &&
		    !(new_entry->type == TOMOYO_TYPE_PATH_ACL &&
		      container_of(new_entry, struct tomoyo_path_acl, head)
		      ->perm == 1 << TOMOYO_TYPE_EXECUTE))
			goto out;
	}
	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		goto out;
	list_for_each_entry_rcu(entry, list, list) {
		if (entry->is_deleted == TOMOYO_GC_IN_PROGRESS)
			continue;
		if (!tomoyo_same_acl_head(entry, new_entry) ||
		    !check_duplicate(entry, new_entry))
			continue;
		if (merge_duplicate)
			entry->is_deleted = merge_duplicate(entry, new_entry,
							    is_delete);
		else
			entry->is_deleted = is_delete;
		error = 0;
		break;
	}
	if (error && !is_delete) {
		entry = tomoyo_commit_ok(new_entry, size);
		if (entry) {
			list_add_tail_rcu(&entry->list, list);
			error = 0;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
out:
	tomoyo_put_condition(new_entry->cond);
	return error;
}

void tomoyo_check_acl(struct tomoyo_request_info *r,
		      bool (*check_entry) (struct tomoyo_request_info *,
					   const struct tomoyo_acl_info *))
{
	const struct tomoyo_domain_info *domain = r->domain;
	struct tomoyo_acl_info *ptr;
	bool retried = false;
	const struct list_head *list = &domain->acl_info_list;

retry:
	list_for_each_entry_rcu(ptr, list, list) {
		if (ptr->is_deleted || ptr->type != r->param_type)
			continue;
		if (!check_entry(r, ptr))
			continue;
		if (!tomoyo_condition(r, ptr->cond))
			continue;
		r->matched_acl = ptr;
		r->granted = true;
		return;
	}
	if (!retried) {
		retried = true;
		list = &domain->ns->acl_group[domain->group];
		goto retry;
	}
	r->granted = false;
}

LIST_HEAD(tomoyo_domain_list);

static const char *tomoyo_last_word(const char *name)
{
	const char *cp = strrchr(name, ' ');
	if (cp)
		return cp + 1;
	return name;
}

static bool tomoyo_same_transition_control(const struct tomoyo_acl_head *a,
					   const struct tomoyo_acl_head *b)
{
	const struct tomoyo_transition_control *p1 = container_of(a,
								  typeof(*p1),
								  head);
	const struct tomoyo_transition_control *p2 = container_of(b,
								  typeof(*p2),
								  head);
	return p1->type == p2->type && p1->is_last_name == p2->is_last_name
		&& p1->domainname == p2->domainname
		&& p1->program == p2->program;
}

int tomoyo_write_transition_control(struct tomoyo_acl_param *param,
				    const u8 type)
{
	struct tomoyo_transition_control e = { .type = type };
	int error = param->is_delete ? -ENOENT : -ENOMEM;
	char *program = param->data;
	char *domainname = strstr(program, " from ");
	if (domainname) {
		*domainname = '\0';
		domainname += 6;
	} else if (type == TOMOYO_TRANSITION_CONTROL_NO_KEEP ||
		   type == TOMOYO_TRANSITION_CONTROL_KEEP) {
		domainname = program;
		program = NULL;
	}
	if (program && strcmp(program, "any")) {
		if (!tomoyo_correct_path(program))
			return -EINVAL;
		e.program = tomoyo_get_name(program);
		if (!e.program)
			goto out;
	}
	if (domainname && strcmp(domainname, "any")) {
		if (!tomoyo_correct_domain(domainname)) {
			if (!tomoyo_correct_path(domainname))
				goto out;
			e.is_last_name = true;
		}
		e.domainname = tomoyo_get_name(domainname);
		if (!e.domainname)
			goto out;
	}
	param->list = &param->ns->policy_list[TOMOYO_ID_TRANSITION_CONTROL];
	error = tomoyo_update_policy(&e.head, sizeof(e), param,
				     tomoyo_same_transition_control);
out:
	tomoyo_put_name(e.domainname);
	tomoyo_put_name(e.program);
	return error;
}

static inline bool tomoyo_scan_transition
(const struct list_head *list, const struct tomoyo_path_info *domainname,
 const struct tomoyo_path_info *program, const char *last_name,
 const enum tomoyo_transition_type type)
{
	const struct tomoyo_transition_control *ptr;
	list_for_each_entry_rcu(ptr, list, head.list) {
		if (ptr->head.is_deleted || ptr->type != type)
			continue;
		if (ptr->domainname) {
			if (!ptr->is_last_name) {
				if (ptr->domainname != domainname)
					continue;
			} else {
				if (strcmp(ptr->domainname->name, last_name))
					continue;
			}
		}
		if (ptr->program && tomoyo_pathcmp(ptr->program, program))
			continue;
		return true;
	}
	return false;
}

static enum tomoyo_transition_type tomoyo_transition_type
(const struct tomoyo_policy_namespace *ns,
 const struct tomoyo_path_info *domainname,
 const struct tomoyo_path_info *program)
{
	const char *last_name = tomoyo_last_word(domainname->name);
	enum tomoyo_transition_type type = TOMOYO_TRANSITION_CONTROL_NO_RESET;
	while (type < TOMOYO_MAX_TRANSITION_TYPE) {
		const struct list_head * const list =
			&ns->policy_list[TOMOYO_ID_TRANSITION_CONTROL];
		if (!tomoyo_scan_transition(list, domainname, program,
					    last_name, type)) {
			type++;
			continue;
		}
		if (type != TOMOYO_TRANSITION_CONTROL_NO_RESET &&
		    type != TOMOYO_TRANSITION_CONTROL_NO_INITIALIZE)
			break;
		type++;
		type++;
	}
	return type;
}

static bool tomoyo_same_aggregator(const struct tomoyo_acl_head *a,
				   const struct tomoyo_acl_head *b)
{
	const struct tomoyo_aggregator *p1 = container_of(a, typeof(*p1),
							  head);
	const struct tomoyo_aggregator *p2 = container_of(b, typeof(*p2),
							  head);
	return p1->original_name == p2->original_name &&
		p1->aggregated_name == p2->aggregated_name;
}

int tomoyo_write_aggregator(struct tomoyo_acl_param *param)
{
	struct tomoyo_aggregator e = { };
	int error = param->is_delete ? -ENOENT : -ENOMEM;
	const char *original_name = tomoyo_read_token(param);
	const char *aggregated_name = tomoyo_read_token(param);
	if (!tomoyo_correct_word(original_name) ||
	    !tomoyo_correct_path(aggregated_name))
		return -EINVAL;
	e.original_name = tomoyo_get_name(original_name);
	e.aggregated_name = tomoyo_get_name(aggregated_name);
	if (!e.original_name || !e.aggregated_name ||
	    e.aggregated_name->is_patterned) 
		goto out;
	param->list = &param->ns->policy_list[TOMOYO_ID_AGGREGATOR];
	error = tomoyo_update_policy(&e.head, sizeof(e), param,
				     tomoyo_same_aggregator);
out:
	tomoyo_put_name(e.original_name);
	tomoyo_put_name(e.aggregated_name);
	return error;
}

static struct tomoyo_policy_namespace *tomoyo_find_namespace
(const char *name, const unsigned int len)
{
	struct tomoyo_policy_namespace *ns;
	list_for_each_entry(ns, &tomoyo_namespace_list, namespace_list) {
		if (strncmp(name, ns->name, len) ||
		    (name[len] && name[len] != ' '))
			continue;
		return ns;
	}
	return NULL;
}

struct tomoyo_policy_namespace *tomoyo_assign_namespace(const char *domainname)
{
	struct tomoyo_policy_namespace *ptr;
	struct tomoyo_policy_namespace *entry;
	const char *cp = domainname;
	unsigned int len = 0;
	while (*cp && *cp++ != ' ')
		len++;
	ptr = tomoyo_find_namespace(domainname, len);
	if (ptr)
		return ptr;
	if (len >= TOMOYO_EXEC_TMPSIZE - 10 || !tomoyo_domain_def(domainname))
		return NULL;
	entry = kzalloc(sizeof(*entry) + len + 1, GFP_NOFS);
	if (!entry)
		return NULL;
	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		goto out;
	ptr = tomoyo_find_namespace(domainname, len);
	if (!ptr && tomoyo_memory_ok(entry)) {
		char *name = (char *) (entry + 1);
		ptr = entry;
		memmove(name, domainname, len);
		name[len] = '\0';
		entry->name = name;
		tomoyo_init_policy_namespace(entry);
		entry = NULL;
	}
	mutex_unlock(&tomoyo_policy_lock);
out:
	kfree(entry);
	return ptr;
}

static bool tomoyo_namespace_jump(const char *domainname)
{
	const char *namespace = tomoyo_current_namespace()->name;
	const int len = strlen(namespace);
	return strncmp(domainname, namespace, len) ||
		(domainname[len] && domainname[len] != ' ');
}

struct tomoyo_domain_info *tomoyo_assign_domain(const char *domainname,
						const bool transit)
{
	struct tomoyo_domain_info e = { };
	struct tomoyo_domain_info *entry = tomoyo_find_domain(domainname);
	bool created = false;
	if (entry) {
		if (transit) {
			if (tomoyo_policy_loaded &&
			    !entry->ns->profile_ptr[entry->profile])
				return NULL;
		}
		return entry;
	}
	
	
	if (strlen(domainname) >= TOMOYO_EXEC_TMPSIZE - 10 ||
	    !tomoyo_correct_domain(domainname))
		return NULL;
	if (transit && tomoyo_namespace_jump(domainname))
		return NULL;
	e.ns = tomoyo_assign_namespace(domainname);
	if (!e.ns)
		return NULL;
	if (transit) {
		const struct tomoyo_domain_info *domain = tomoyo_domain();
		e.profile = domain->profile;
		e.group = domain->group;
	}
	e.domainname = tomoyo_get_name(domainname);
	if (!e.domainname)
		return NULL;
	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		goto out;
	entry = tomoyo_find_domain(domainname);
	if (!entry) {
		entry = tomoyo_commit_ok(&e, sizeof(e));
		if (entry) {
			INIT_LIST_HEAD(&entry->acl_info_list);
			list_add_tail_rcu(&entry->list, &tomoyo_domain_list);
			created = true;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
out:
	tomoyo_put_name(e.domainname);
	if (entry && transit) {
		if (created) {
			struct tomoyo_request_info r;
			tomoyo_init_request_info(&r, entry,
						 TOMOYO_MAC_FILE_EXECUTE);
			r.granted = false;
			tomoyo_write_log(&r, "use_profile %u\n",
					 entry->profile);
			tomoyo_write_log(&r, "use_group %u\n", entry->group);
			tomoyo_update_stat(TOMOYO_STAT_POLICY_UPDATES);
		}
	}
	return entry;
}

static int tomoyo_environ(struct tomoyo_execve *ee)
{
	struct tomoyo_request_info *r = &ee->r;
	struct linux_binprm *bprm = ee->bprm;
	
	struct tomoyo_page_dump env_page = { };
	char *arg_ptr; 
	int arg_len = 0;
	unsigned long pos = bprm->p;
	int offset = pos % PAGE_SIZE;
	int argv_count = bprm->argc;
	int envp_count = bprm->envc;
	int error = -ENOMEM;

	ee->r.type = TOMOYO_MAC_ENVIRON;
	ee->r.profile = r->domain->profile;
	ee->r.mode = tomoyo_get_mode(r->domain->ns, ee->r.profile,
				     TOMOYO_MAC_ENVIRON);
	if (!r->mode || !envp_count)
		return 0;
	arg_ptr = kzalloc(TOMOYO_EXEC_TMPSIZE, GFP_NOFS);
	if (!arg_ptr)
		goto out;
	while (error == -ENOMEM) {
		if (!tomoyo_dump_page(bprm, pos, &env_page))
			goto out;
		pos += PAGE_SIZE - offset;
		
		while (argv_count && offset < PAGE_SIZE) {
			if (!env_page.data[offset++])
				argv_count--;
		}
		if (argv_count) {
			offset = 0;
			continue;
		}
		while (offset < PAGE_SIZE) {
			const unsigned char c = env_page.data[offset++];

			if (c && arg_len < TOMOYO_EXEC_TMPSIZE - 10) {
				if (c == '=') {
					arg_ptr[arg_len++] = '\0';
				} else if (c == '\\') {
					arg_ptr[arg_len++] = '\\';
					arg_ptr[arg_len++] = '\\';
				} else if (c > ' ' && c < 127) {
					arg_ptr[arg_len++] = c;
				} else {
					arg_ptr[arg_len++] = '\\';
					arg_ptr[arg_len++] = (c >> 6) + '0';
					arg_ptr[arg_len++]
						= ((c >> 3) & 7) + '0';
					arg_ptr[arg_len++] = (c & 7) + '0';
				}
			} else {
				arg_ptr[arg_len] = '\0';
			}
			if (c)
				continue;
			if (tomoyo_env_perm(r, arg_ptr)) {
				error = -EPERM;
				break;
			}
			if (!--envp_count) {
				error = 0;
				break;
			}
			arg_len = 0;
		}
		offset = 0;
	}
out:
	if (r->mode != TOMOYO_CONFIG_ENFORCING)
		error = 0;
	kfree(env_page.data);
	kfree(arg_ptr);
	return error;
}

int tomoyo_find_next_domain(struct linux_binprm *bprm)
{
	struct tomoyo_domain_info *old_domain = tomoyo_domain();
	struct tomoyo_domain_info *domain = NULL;
	const char *original_name = bprm->filename;
	int retval = -ENOMEM;
	bool reject_on_transition_failure = false;
	const struct tomoyo_path_info *candidate;
	struct tomoyo_path_info exename;
	struct tomoyo_execve *ee = kzalloc(sizeof(*ee), GFP_NOFS);

	if (!ee)
		return -ENOMEM;
	ee->tmp = kzalloc(TOMOYO_EXEC_TMPSIZE, GFP_NOFS);
	if (!ee->tmp) {
		kfree(ee);
		return -ENOMEM;
	}
	
	tomoyo_init_request_info(&ee->r, NULL, TOMOYO_MAC_FILE_EXECUTE);
	ee->r.ee = ee;
	ee->bprm = bprm;
	ee->r.obj = &ee->obj;
	ee->obj.path1 = bprm->file->f_path;
	
	retval = -ENOENT;
	exename.name = tomoyo_realpath_nofollow(original_name);
	if (!exename.name)
		goto out;
	tomoyo_fill_path_info(&exename);
retry:
	
	{
		struct tomoyo_aggregator *ptr;
		struct list_head *list =
			&old_domain->ns->policy_list[TOMOYO_ID_AGGREGATOR];
		
		candidate = &exename;
		list_for_each_entry_rcu(ptr, list, head.list) {
			if (ptr->head.is_deleted ||
			    !tomoyo_path_matches_pattern(&exename,
							 ptr->original_name))
				continue;
			candidate = ptr->aggregated_name;
			break;
		}
	}

	
	retval = tomoyo_execute_permission(&ee->r, candidate);
	if (retval == TOMOYO_RETRY_REQUEST)
		goto retry;
	if (retval < 0)
		goto out;
	if (ee->r.param.path.matched_path)
		candidate = ee->r.param.path.matched_path;

	if (ee->transition) {
		const char *domainname = ee->transition->name;
		reject_on_transition_failure = true;
		if (!strcmp(domainname, "keep"))
			goto force_keep_domain;
		if (!strcmp(domainname, "child"))
			goto force_child_domain;
		if (!strcmp(domainname, "reset"))
			goto force_reset_domain;
		if (!strcmp(domainname, "initialize"))
			goto force_initialize_domain;
		if (!strcmp(domainname, "parent")) {
			char *cp;
			strncpy(ee->tmp, old_domain->domainname->name,
				TOMOYO_EXEC_TMPSIZE - 1);
			cp = strrchr(ee->tmp, ' ');
			if (cp)
				*cp = '\0';
		} else if (*domainname == '<')
			strncpy(ee->tmp, domainname, TOMOYO_EXEC_TMPSIZE - 1);
		else
			snprintf(ee->tmp, TOMOYO_EXEC_TMPSIZE - 1, "%s %s",
				 old_domain->domainname->name, domainname);
		goto force_jump_domain;
	}
	switch (tomoyo_transition_type(old_domain->ns, old_domain->domainname,
				       candidate)) {
	case TOMOYO_TRANSITION_CONTROL_RESET:
force_reset_domain:
		
		snprintf(ee->tmp, TOMOYO_EXEC_TMPSIZE - 1, "<%s>",
			 candidate->name);
		reject_on_transition_failure = true;
		break;
	case TOMOYO_TRANSITION_CONTROL_INITIALIZE:
force_initialize_domain:
		
		snprintf(ee->tmp, TOMOYO_EXEC_TMPSIZE - 1, "%s %s",
			 old_domain->ns->name, candidate->name);
		break;
	case TOMOYO_TRANSITION_CONTROL_KEEP:
force_keep_domain:
		
		domain = old_domain;
		break;
	default:
		if (old_domain == &tomoyo_kernel_domain &&
		    !tomoyo_policy_loaded) {
			domain = old_domain;
			break;
		}
force_child_domain:
		
		snprintf(ee->tmp, TOMOYO_EXEC_TMPSIZE - 1, "%s %s",
			 old_domain->domainname->name, candidate->name);
		break;
	}
force_jump_domain:
	if (!domain)
		domain = tomoyo_assign_domain(ee->tmp, true);
	if (domain)
		retval = 0;
	else if (reject_on_transition_failure) {
		printk(KERN_WARNING "ERROR: Domain '%s' not ready.\n",
		       ee->tmp);
		retval = -ENOMEM;
	} else if (ee->r.mode == TOMOYO_CONFIG_ENFORCING)
		retval = -ENOMEM;
	else {
		retval = 0;
		if (!old_domain->flags[TOMOYO_DIF_TRANSITION_FAILED]) {
			old_domain->flags[TOMOYO_DIF_TRANSITION_FAILED] = true;
			ee->r.granted = false;
			tomoyo_write_log(&ee->r, "%s", tomoyo_dif
					 [TOMOYO_DIF_TRANSITION_FAILED]);
			printk(KERN_WARNING
			       "ERROR: Domain '%s' not defined.\n", ee->tmp);
		}
	}
 out:
	if (!domain)
		domain = old_domain;
	
	atomic_inc(&domain->users);
	bprm->cred->security = domain;
	kfree(exename.name);
	if (!retval) {
		ee->r.domain = domain;
		retval = tomoyo_environ(ee);
	}
	kfree(ee->tmp);
	kfree(ee->dump.data);
	kfree(ee);
	return retval;
}

bool tomoyo_dump_page(struct linux_binprm *bprm, unsigned long pos,
		      struct tomoyo_page_dump *dump)
{
	struct page *page;

	
	if (!dump->data) {
		dump->data = kzalloc(PAGE_SIZE, GFP_NOFS);
		if (!dump->data)
			return false;
	}
	
#ifdef CONFIG_MMU
	if (get_user_pages(current, bprm->mm, pos, 1, 0, 1, &page, NULL) <= 0)
		return false;
#else
	page = bprm->page[pos / PAGE_SIZE];
#endif
	if (page != dump->page) {
		const unsigned int offset = pos % PAGE_SIZE;
		char *kaddr = kmap_atomic(page);

		dump->page = page;
		memcpy(dump->data + offset, kaddr + offset,
		       PAGE_SIZE - offset);
		kunmap_atomic(kaddr);
	}
	
#ifdef CONFIG_MMU
	put_page(page);
#endif
	return true;
}
