/*
 * Copyright (C) 2005,2006,2007,2008 IBM Corporation
 *
 * Authors:
 * Kylene Hall <kjhall@us.ibm.com>
 * Reiner Sailer <sailer@us.ibm.com>
 * Mimi Zohar <zohar@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima_fs.c
 *	implemenents security file system for reporting
 *	current measurement list and IMA statistics
 */
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/parser.h>

#include "ima.h"

static int valid_policy = 1;
#define TMPBUFLEN 12
static ssize_t ima_show_htable_value(char __user *buf, size_t count,
				     loff_t *ppos, atomic_long_t *val)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t len;

	len = scnprintf(tmpbuf, TMPBUFLEN, "%li\n", atomic_long_read(val));
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, len);
}

static ssize_t ima_show_htable_violations(struct file *filp,
					  char __user *buf,
					  size_t count, loff_t *ppos)
{
	return ima_show_htable_value(buf, count, ppos, &ima_htable.violations);
}

static const struct file_operations ima_htable_violations_ops = {
	.read = ima_show_htable_violations,
	.llseek = generic_file_llseek,
};

static ssize_t ima_show_measurements_count(struct file *filp,
					   char __user *buf,
					   size_t count, loff_t *ppos)
{
	return ima_show_htable_value(buf, count, ppos, &ima_htable.len);

}

static const struct file_operations ima_measurements_count_ops = {
	.read = ima_show_measurements_count,
	.llseek = generic_file_llseek,
};

static void *ima_measurements_start(struct seq_file *m, loff_t *pos)
{
	loff_t l = *pos;
	struct ima_queue_entry *qe;

	
	rcu_read_lock();
	list_for_each_entry_rcu(qe, &ima_measurements, later) {
		if (!l--) {
			rcu_read_unlock();
			return qe;
		}
	}
	rcu_read_unlock();
	return NULL;
}

static void *ima_measurements_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct ima_queue_entry *qe = v;

	rcu_read_lock();
	qe = list_entry_rcu(qe->later.next,
			    struct ima_queue_entry, later);
	rcu_read_unlock();
	(*pos)++;

	return (&qe->later == &ima_measurements) ? NULL : qe;
}

static void ima_measurements_stop(struct seq_file *m, void *v)
{
}

static void ima_putc(struct seq_file *m, void *data, int datalen)
{
	while (datalen--)
		seq_putc(m, *(char *)data++);
}

static int ima_measurements_show(struct seq_file *m, void *v)
{
	
	struct ima_queue_entry *qe = v;
	struct ima_template_entry *e;
	int namelen;
	u32 pcr = CONFIG_IMA_MEASURE_PCR_IDX;

	
	e = qe->entry;
	if (e == NULL)
		return -1;

	ima_putc(m, &pcr, sizeof pcr);

	
	ima_putc(m, e->digest, IMA_DIGEST_SIZE);

	
	namelen = strlen(e->template_name);
	ima_putc(m, &namelen, sizeof namelen);

	
	ima_putc(m, (void *)e->template_name, namelen);

	
	ima_template_show(m, (struct ima_template_data *)&e->template,
			  IMA_SHOW_BINARY);
	return 0;
}

static const struct seq_operations ima_measurments_seqops = {
	.start = ima_measurements_start,
	.next = ima_measurements_next,
	.stop = ima_measurements_stop,
	.show = ima_measurements_show
};

static int ima_measurements_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ima_measurments_seqops);
}

static const struct file_operations ima_measurements_ops = {
	.open = ima_measurements_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static void ima_print_digest(struct seq_file *m, u8 *digest)
{
	int i;

	for (i = 0; i < IMA_DIGEST_SIZE; i++)
		seq_printf(m, "%02x", *(digest + i));
}

void ima_template_show(struct seq_file *m, void *e, enum ima_show_type show)
{
	struct ima_template_data *entry = e;
	int namelen;

	switch (show) {
	case IMA_SHOW_ASCII:
		ima_print_digest(m, entry->digest);
		seq_printf(m, " %s\n", entry->file_name);
		break;
	case IMA_SHOW_BINARY:
		ima_putc(m, entry->digest, IMA_DIGEST_SIZE);

		namelen = strlen(entry->file_name);
		ima_putc(m, &namelen, sizeof namelen);
		ima_putc(m, entry->file_name, namelen);
	default:
		break;
	}
}

static int ima_ascii_measurements_show(struct seq_file *m, void *v)
{
	
	struct ima_queue_entry *qe = v;
	struct ima_template_entry *e;

	
	e = qe->entry;
	if (e == NULL)
		return -1;

	
	seq_printf(m, "%2d ", CONFIG_IMA_MEASURE_PCR_IDX);

	
	ima_print_digest(m, e->digest);

	
	seq_printf(m, " %s ", e->template_name);

	
	ima_template_show(m, (struct ima_template_data *)&e->template,
			  IMA_SHOW_ASCII);
	return 0;
}

static const struct seq_operations ima_ascii_measurements_seqops = {
	.start = ima_measurements_start,
	.next = ima_measurements_next,
	.stop = ima_measurements_stop,
	.show = ima_ascii_measurements_show
};

static int ima_ascii_measurements_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ima_ascii_measurements_seqops);
}

static const struct file_operations ima_ascii_measurements_ops = {
	.open = ima_ascii_measurements_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static ssize_t ima_write_policy(struct file *file, const char __user *buf,
				size_t datalen, loff_t *ppos)
{
	char *data = NULL;
	ssize_t result;

	if (datalen >= PAGE_SIZE)
		datalen = PAGE_SIZE - 1;

	
	result = -EINVAL;
	if (*ppos != 0)
		goto out;

	result = -ENOMEM;
	data = kmalloc(datalen + 1, GFP_KERNEL);
	if (!data)
		goto out;

	*(data + datalen) = '\0';

	result = -EFAULT;
	if (copy_from_user(data, buf, datalen))
		goto out;

	result = ima_parse_add_rule(data);
out:
	if (result < 0)
		valid_policy = 0;
	kfree(data);
	return result;
}

static struct dentry *ima_dir;
static struct dentry *binary_runtime_measurements;
static struct dentry *ascii_runtime_measurements;
static struct dentry *runtime_measurements_count;
static struct dentry *violations;
static struct dentry *ima_policy;

static atomic_t policy_opencount = ATOMIC_INIT(1);
static int ima_open_policy(struct inode * inode, struct file * filp)
{
	
	if (!(filp->f_flags & O_WRONLY))
		return -EACCES;
	if (atomic_dec_and_test(&policy_opencount))
		return 0;
	return -EBUSY;
}

static int ima_release_policy(struct inode *inode, struct file *file)
{
	if (!valid_policy) {
		ima_delete_rules();
		valid_policy = 1;
		atomic_set(&policy_opencount, 1);
		return 0;
	}
	ima_update_policy();
	securityfs_remove(ima_policy);
	ima_policy = NULL;
	return 0;
}

static const struct file_operations ima_measure_policy_ops = {
	.open = ima_open_policy,
	.write = ima_write_policy,
	.release = ima_release_policy,
	.llseek = generic_file_llseek,
};

int __init ima_fs_init(void)
{
	ima_dir = securityfs_create_dir("ima", NULL);
	if (IS_ERR(ima_dir))
		return -1;

	binary_runtime_measurements =
	    securityfs_create_file("binary_runtime_measurements",
				   S_IRUSR | S_IRGRP, ima_dir, NULL,
				   &ima_measurements_ops);
	if (IS_ERR(binary_runtime_measurements))
		goto out;

	ascii_runtime_measurements =
	    securityfs_create_file("ascii_runtime_measurements",
				   S_IRUSR | S_IRGRP, ima_dir, NULL,
				   &ima_ascii_measurements_ops);
	if (IS_ERR(ascii_runtime_measurements))
		goto out;

	runtime_measurements_count =
	    securityfs_create_file("runtime_measurements_count",
				   S_IRUSR | S_IRGRP, ima_dir, NULL,
				   &ima_measurements_count_ops);
	if (IS_ERR(runtime_measurements_count))
		goto out;

	violations =
	    securityfs_create_file("violations", S_IRUSR | S_IRGRP,
				   ima_dir, NULL, &ima_htable_violations_ops);
	if (IS_ERR(violations))
		goto out;

	ima_policy = securityfs_create_file("policy",
					    S_IWUSR,
					    ima_dir, NULL,
					    &ima_measure_policy_ops);
	if (IS_ERR(ima_policy))
		goto out;

	return 0;
out:
	securityfs_remove(runtime_measurements_count);
	securityfs_remove(ascii_runtime_measurements);
	securityfs_remove(binary_runtime_measurements);
	securityfs_remove(ima_dir);
	securityfs_remove(ima_policy);
	return -1;
}

void __exit ima_fs_cleanup(void)
{
	securityfs_remove(violations);
	securityfs_remove(runtime_measurements_count);
	securityfs_remove(ascii_runtime_measurements);
	securityfs_remove(binary_runtime_measurements);
	securityfs_remove(ima_dir);
	securityfs_remove(ima_policy);
}
