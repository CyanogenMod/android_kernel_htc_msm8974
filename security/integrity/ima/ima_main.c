/*
 * Copyright (C) 2005,2006,2007,2008 IBM Corporation
 *
 * Authors:
 * Reiner Sailer <sailer@watson.ibm.com>
 * Serge Hallyn <serue@us.ibm.com>
 * Kylene Hall <kylene@us.ibm.com>
 * Mimi Zohar <zohar@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima_main.c
 *	implements the IMA hooks: ima_bprm_check, ima_file_mmap,
 *	and ima_file_check.
 */
#include <linux/module.h>
#include <linux/file.h>
#include <linux/binfmts.h>
#include <linux/mount.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/ima.h>

#include "ima.h"

int ima_initialized;

char *ima_hash = "sha1";
static int __init hash_setup(char *str)
{
	if (strncmp(str, "md5", 3) == 0)
		ima_hash = "md5";
	return 1;
}
__setup("ima_hash=", hash_setup);

static void ima_rdwr_violation_check(struct file *file)
{
	struct dentry *dentry = file->f_path.dentry;
	struct inode *inode = dentry->d_inode;
	fmode_t mode = file->f_mode;
	int rc;
	bool send_tomtou = false, send_writers = false;

	if (!S_ISREG(inode->i_mode) || !ima_initialized)
		return;

	mutex_lock(&inode->i_mutex);	

	if (mode & FMODE_WRITE) {
		if (atomic_read(&inode->i_readcount) && IS_IMA(inode))
			send_tomtou = true;
		goto out;
	}

	rc = ima_must_measure(inode, MAY_READ, FILE_CHECK);
	if (rc < 0)
		goto out;

	if (atomic_read(&inode->i_writecount) > 0)
		send_writers = true;
out:
	mutex_unlock(&inode->i_mutex);

	if (send_tomtou)
		ima_add_violation(inode, dentry->d_name.name, "invalid_pcr",
				  "ToMToU");
	if (send_writers)
		ima_add_violation(inode, dentry->d_name.name, "invalid_pcr",
				  "open_writers");
}

static void ima_check_last_writer(struct integrity_iint_cache *iint,
				  struct inode *inode,
				  struct file *file)
{
	fmode_t mode = file->f_mode;

	mutex_lock(&iint->mutex);
	if (mode & FMODE_WRITE &&
	    atomic_read(&inode->i_writecount) == 1 &&
	    iint->version != inode->i_version)
		iint->flags &= ~IMA_MEASURED;
	mutex_unlock(&iint->mutex);
}

void ima_file_free(struct file *file)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct integrity_iint_cache *iint;

	if (!iint_initialized || !S_ISREG(inode->i_mode))
		return;

	iint = integrity_iint_find(inode);
	if (!iint)
		return;

	ima_check_last_writer(iint, inode, file);
}

static int process_measurement(struct file *file, const unsigned char *filename,
			       int mask, int function)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct integrity_iint_cache *iint;
	int rc = 0;

	if (!ima_initialized || !S_ISREG(inode->i_mode))
		return 0;

	rc = ima_must_measure(inode, mask, function);
	if (rc != 0)
		return rc;
retry:
	iint = integrity_iint_find(inode);
	if (!iint) {
		rc = integrity_inode_alloc(inode);
		if (!rc || rc == -EEXIST)
			goto retry;
		return rc;
	}

	mutex_lock(&iint->mutex);

	rc = iint->flags & IMA_MEASURED ? 1 : 0;
	if (rc != 0)
		goto out;

	rc = ima_collect_measurement(iint, file);
	if (!rc)
		ima_store_measurement(iint, file, filename);
out:
	mutex_unlock(&iint->mutex);
	return rc;
}

int ima_file_mmap(struct file *file, unsigned long prot)
{
	int rc;

	if (!file)
		return 0;
	if (prot & PROT_EXEC)
		rc = process_measurement(file, file->f_dentry->d_name.name,
					 MAY_EXEC, FILE_MMAP);
	return 0;
}

int ima_bprm_check(struct linux_binprm *bprm)
{
	int rc;

	rc = process_measurement(bprm->file, bprm->filename,
				 MAY_EXEC, BPRM_CHECK);
	return 0;
}

int ima_file_check(struct file *file, int mask)
{
	int rc;

	ima_rdwr_violation_check(file);
	rc = process_measurement(file, file->f_dentry->d_name.name,
				 mask & (MAY_READ | MAY_WRITE | MAY_EXEC),
				 FILE_CHECK);
	return 0;
}
EXPORT_SYMBOL_GPL(ima_file_check);

static int __init init_ima(void)
{
	int error;

	error = ima_init();
	ima_initialized = 1;
	return error;
}

static void __exit cleanup_ima(void)
{
	ima_cleanup();
}

late_initcall(init_ima);	

MODULE_DESCRIPTION("Integrity Measurement Architecture");
MODULE_LICENSE("GPL");
