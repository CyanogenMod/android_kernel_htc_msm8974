/*
 * Copyright (C) 2007 Casey Schaufler <casey@schaufler-ca.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, version 2.
 *
 * Author:
 *      Casey Schaufler <casey@schaufler-ca.com>
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include "smack.h"

struct smack_known smack_known_huh = {
	.smk_known	= "?",
	.smk_secid	= 2,
	.smk_cipso	= NULL,
};

struct smack_known smack_known_hat = {
	.smk_known	= "^",
	.smk_secid	= 3,
	.smk_cipso	= NULL,
};

struct smack_known smack_known_star = {
	.smk_known	= "*",
	.smk_secid	= 4,
	.smk_cipso	= NULL,
};

struct smack_known smack_known_floor = {
	.smk_known	= "_",
	.smk_secid	= 5,
	.smk_cipso	= NULL,
};

struct smack_known smack_known_invalid = {
	.smk_known	= "",
	.smk_secid	= 6,
	.smk_cipso	= NULL,
};

struct smack_known smack_known_web = {
	.smk_known	= "@",
	.smk_secid	= 7,
	.smk_cipso	= NULL,
};

LIST_HEAD(smack_known_list);

static u32 smack_next_secid = 10;

/*
 * what events do we log
 * can be overwritten at run-time by /smack/logging
 */
int log_policy = SMACK_AUDIT_DENIED;

int smk_access_entry(char *subject_label, char *object_label,
			struct list_head *rule_list)
{
	int may = -ENOENT;
	struct smack_rule *srp;

	list_for_each_entry_rcu(srp, rule_list, list) {
		if (srp->smk_object == object_label &&
		    srp->smk_subject == subject_label) {
			may = srp->smk_access;
			break;
		}
	}

	return may;
}

int smk_access(char *subject_label, char *object_label, int request,
	       struct smk_audit_info *a)
{
	struct smack_known *skp;
	int may = MAY_NOT;
	int rc = 0;

	if (subject_label == smack_known_star.smk_known) {
		rc = -EACCES;
		goto out_audit;
	}
	if (object_label == smack_known_web.smk_known ||
	    subject_label == smack_known_web.smk_known)
		goto out_audit;
	if (object_label == smack_known_star.smk_known)
		goto out_audit;
	if (subject_label == object_label)
		goto out_audit;
	if ((request & MAY_ANYREAD) == request) {
		if (object_label == smack_known_floor.smk_known)
			goto out_audit;
		if (subject_label == smack_known_hat.smk_known)
			goto out_audit;
	}
	skp = smk_find_entry(subject_label);
	rcu_read_lock();
	may = smk_access_entry(subject_label, object_label, &skp->smk_rules);
	rcu_read_unlock();

	if (may > 0 && (request & may) == request)
		goto out_audit;

	rc = -EACCES;
out_audit:
#ifdef CONFIG_AUDIT
	if (a)
		smack_log(subject_label, object_label, request, rc, a);
#endif
	return rc;
}

int smk_curacc(char *obj_label, u32 mode, struct smk_audit_info *a)
{
	struct task_smack *tsp = current_security();
	char *sp = smk_of_task(tsp);
	int may;
	int rc;

	rc = smk_access(sp, obj_label, mode, NULL);
	if (rc == 0) {
		may = smk_access_entry(sp, obj_label, &tsp->smk_rules);
		if (may < 0)
			goto out_audit;
		if ((mode & may) == mode)
			goto out_audit;
		rc = -EACCES;
	}

	if (smack_onlycap != NULL && smack_onlycap != sp)
		goto out_audit;

	if (capable(CAP_MAC_OVERRIDE))
		rc = 0;

out_audit:
#ifdef CONFIG_AUDIT
	if (a)
		smack_log(sp, obj_label, mode, rc, a);
#endif
	return rc;
}

#ifdef CONFIG_AUDIT
static inline void smack_str_from_perm(char *string, int access)
{
	int i = 0;
	if (access & MAY_READ)
		string[i++] = 'r';
	if (access & MAY_WRITE)
		string[i++] = 'w';
	if (access & MAY_EXEC)
		string[i++] = 'x';
	if (access & MAY_APPEND)
		string[i++] = 'a';
	string[i] = '\0';
}
static void smack_log_callback(struct audit_buffer *ab, void *a)
{
	struct common_audit_data *ad = a;
	struct smack_audit_data *sad = ad->smack_audit_data;
	audit_log_format(ab, "lsm=SMACK fn=%s action=%s",
			 ad->smack_audit_data->function,
			 sad->result ? "denied" : "granted");
	audit_log_format(ab, " subject=");
	audit_log_untrustedstring(ab, sad->subject);
	audit_log_format(ab, " object=");
	audit_log_untrustedstring(ab, sad->object);
	audit_log_format(ab, " requested=%s", sad->request);
}

void smack_log(char *subject_label, char *object_label, int request,
	       int result, struct smk_audit_info *ad)
{
	char request_buffer[SMK_NUM_ACCESS_TYPE + 1];
	struct smack_audit_data *sad;
	struct common_audit_data *a = &ad->a;

	
	if (result != 0 && (log_policy & SMACK_AUDIT_DENIED) == 0)
		return;
	if (result == 0 && (log_policy & SMACK_AUDIT_ACCEPT) == 0)
		return;

	sad = a->smack_audit_data;

	if (sad->function == NULL)
		sad->function = "unknown";

	
	smack_str_from_perm(request_buffer, request);
	sad->subject = subject_label;
	sad->object  = object_label;
	sad->request = request_buffer;
	sad->result  = result;

	common_lsm_audit(a, smack_log_callback, NULL);
}
#else 
void smack_log(char *subject_label, char *object_label, int request,
               int result, struct smk_audit_info *ad)
{
}
#endif

static DEFINE_MUTEX(smack_known_lock);

struct smack_known *smk_find_entry(const char *string)
{
	struct smack_known *skp;

	list_for_each_entry_rcu(skp, &smack_known_list, list) {
		if (strncmp(skp->smk_known, string, SMK_MAXLEN) == 0)
			return skp;
	}

	return NULL;
}

void smk_parse_smack(const char *string, int len, char *smack)
{
	int found;
	int i;

	if (len <= 0 || len > SMK_MAXLEN)
		len = SMK_MAXLEN;

	for (i = 0, found = 0; i < SMK_LABELLEN; i++) {
		if (found)
			smack[i] = '\0';
		else if (i >= len || string[i] > '~' || string[i] <= ' ' ||
			 string[i] == '/' || string[i] == '"' ||
			 string[i] == '\\' || string[i] == '\'') {
			smack[i] = '\0';
			found = 1;
		} else
			smack[i] = string[i];
	}
}

struct smack_known *smk_import_entry(const char *string, int len)
{
	struct smack_known *skp;
	char smack[SMK_LABELLEN];

	smk_parse_smack(string, len, smack);
	if (smack[0] == '\0')
		return NULL;

	mutex_lock(&smack_known_lock);

	skp = smk_find_entry(smack);

	if (skp == NULL) {
		skp = kzalloc(sizeof(struct smack_known), GFP_KERNEL);
		if (skp != NULL) {
			strncpy(skp->smk_known, smack, SMK_MAXLEN);
			skp->smk_secid = smack_next_secid++;
			skp->smk_cipso = NULL;
			INIT_LIST_HEAD(&skp->smk_rules);
			spin_lock_init(&skp->smk_cipsolock);
			mutex_init(&skp->smk_rules_lock);
			list_add_rcu(&skp->list, &smack_known_list);
		}
	}

	mutex_unlock(&smack_known_lock);

	return skp;
}

char *smk_import(const char *string, int len)
{
	struct smack_known *skp;

	
	if (string[0] == '-')
		return NULL;
	skp = smk_import_entry(string, len);
	if (skp == NULL)
		return NULL;
	return skp->smk_known;
}

char *smack_from_secid(const u32 secid)
{
	struct smack_known *skp;

	rcu_read_lock();
	list_for_each_entry_rcu(skp, &smack_known_list, list) {
		if (skp->smk_secid == secid) {
			rcu_read_unlock();
			return skp->smk_known;
		}
	}

	rcu_read_unlock();
	return smack_known_invalid.smk_known;
}

u32 smack_to_secid(const char *smack)
{
	struct smack_known *skp;

	rcu_read_lock();
	list_for_each_entry_rcu(skp, &smack_known_list, list) {
		if (strncmp(skp->smk_known, smack, SMK_MAXLEN) == 0) {
			rcu_read_unlock();
			return skp->smk_secid;
		}
	}
	rcu_read_unlock();
	return 0;
}

char *smack_from_cipso(u32 level, char *cp)
{
	struct smack_known *kp;
	char *final = NULL;

	rcu_read_lock();
	list_for_each_entry(kp, &smack_known_list, list) {
		if (kp->smk_cipso == NULL)
			continue;

		spin_lock_bh(&kp->smk_cipsolock);

		if (kp->smk_cipso->smk_level == level &&
		    memcmp(kp->smk_cipso->smk_catset, cp, SMK_LABELLEN) == 0)
			final = kp->smk_known;

		spin_unlock_bh(&kp->smk_cipsolock);

		if (final != NULL)
			break;
	}
	rcu_read_unlock();

	return final;
}

int smack_to_cipso(const char *smack, struct smack_cipso *cp)
{
	struct smack_known *kp;
	int found = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(kp, &smack_known_list, list) {
		if (kp->smk_known == smack ||
		    strcmp(kp->smk_known, smack) == 0) {
			found = 1;
			break;
		}
	}
	rcu_read_unlock();

	if (found == 0 || kp->smk_cipso == NULL)
		return -ENOENT;

	memcpy(cp, kp->smk_cipso, sizeof(struct smack_cipso));
	return 0;
}
