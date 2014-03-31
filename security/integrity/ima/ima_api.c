/*
 * Copyright (C) 2008 IBM Corporation
 *
 * Author: Mimi Zohar <zohar@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima_api.c
 *	Implements must_measure, collect_measurement, store_measurement,
 *	and store_template.
 */
#include <linux/module.h>
#include <linux/slab.h>

#include "ima.h"
static const char *IMA_TEMPLATE_NAME = "ima";

int ima_store_template(struct ima_template_entry *entry,
		       int violation, struct inode *inode)
{
	const char *op = "add_template_measure";
	const char *audit_cause = "hashing_error";
	int result;

	memset(entry->digest, 0, sizeof(entry->digest));
	entry->template_name = IMA_TEMPLATE_NAME;
	entry->template_len = sizeof(entry->template);

	if (!violation) {
		result = ima_calc_template_hash(entry->template_len,
						&entry->template,
						entry->digest);
		if (result < 0) {
			integrity_audit_msg(AUDIT_INTEGRITY_PCR, inode,
					    entry->template_name, op,
					    audit_cause, result, 0);
			return result;
		}
	}
	result = ima_add_template_entry(entry, violation, op, inode);
	return result;
}

void ima_add_violation(struct inode *inode, const unsigned char *filename,
		       const char *op, const char *cause)
{
	struct ima_template_entry *entry;
	int violation = 1;
	int result;

	
	atomic_long_inc(&ima_htable.violations);

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		result = -ENOMEM;
		goto err_out;
	}
	memset(&entry->template, 0, sizeof(entry->template));
	strncpy(entry->template.file_name, filename, IMA_EVENT_NAME_LEN_MAX);
	result = ima_store_template(entry, violation, inode);
	if (result < 0)
		kfree(entry);
err_out:
	integrity_audit_msg(AUDIT_INTEGRITY_PCR, inode, filename,
			    op, cause, result, 0);
}

int ima_must_measure(struct inode *inode, int mask, int function)
{
	int must_measure;

	must_measure = ima_match_policy(inode, function, mask);
	return must_measure ? 0 : -EACCES;
}

int ima_collect_measurement(struct integrity_iint_cache *iint,
			    struct file *file)
{
	int result = -EEXIST;

	if (!(iint->flags & IMA_MEASURED)) {
		u64 i_version = file->f_dentry->d_inode->i_version;

		memset(iint->digest, 0, IMA_DIGEST_SIZE);
		result = ima_calc_hash(file, iint->digest);
		if (!result)
			iint->version = i_version;
	}
	return result;
}

void ima_store_measurement(struct integrity_iint_cache *iint,
			   struct file *file, const unsigned char *filename)
{
	const char *op = "add_template_measure";
	const char *audit_cause = "ENOMEM";
	int result = -ENOMEM;
	struct inode *inode = file->f_dentry->d_inode;
	struct ima_template_entry *entry;
	int violation = 0;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		integrity_audit_msg(AUDIT_INTEGRITY_PCR, inode, filename,
				    op, audit_cause, result, 0);
		return;
	}
	memset(&entry->template, 0, sizeof(entry->template));
	memcpy(entry->template.digest, iint->digest, IMA_DIGEST_SIZE);
	strncpy(entry->template.file_name, filename, IMA_EVENT_NAME_LEN_MAX);

	result = ima_store_template(entry, violation, inode);
	if (!result || result == -EEXIST)
		iint->flags |= IMA_MEASURED;
	if (result < 0)
		kfree(entry);
}
