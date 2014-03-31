/*
 * Copyright (C) 2005,2006,2007,2008 IBM Corporation
 *
 * Authors:
 * Reiner Sailer      <sailer@watson.ibm.com>
 * Leendert van Doorn <leendert@watson.ibm.com>
 * Mimi Zohar         <zohar@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * File: ima_init.c
 *             initialization and cleanup functions
 */
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/err.h>
#include "ima.h"

static const char *boot_aggregate_name = "boot_aggregate";
int ima_used_chip;

static void __init ima_add_boot_aggregate(void)
{
	struct ima_template_entry *entry;
	const char *op = "add_boot_aggregate";
	const char *audit_cause = "ENOMEM";
	int result = -ENOMEM;
	int violation = 1;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		goto err_out;

	memset(&entry->template, 0, sizeof(entry->template));
	strncpy(entry->template.file_name, boot_aggregate_name,
		IMA_EVENT_NAME_LEN_MAX);
	if (ima_used_chip) {
		violation = 0;
		result = ima_calc_boot_aggregate(entry->template.digest);
		if (result < 0) {
			audit_cause = "hashing_error";
			kfree(entry);
			goto err_out;
		}
	}
	result = ima_store_template(entry, violation, NULL);
	if (result < 0)
		kfree(entry);
	return;
err_out:
	integrity_audit_msg(AUDIT_INTEGRITY_PCR, NULL, boot_aggregate_name, op,
			    audit_cause, result, 0);
}

int __init ima_init(void)
{
	u8 pcr_i[IMA_DIGEST_SIZE];
	int rc;

	ima_used_chip = 0;
	rc = tpm_pcr_read(TPM_ANY_NUM, 0, pcr_i);
	if (rc == 0)
		ima_used_chip = 1;

	if (!ima_used_chip)
		pr_info("IMA: No TPM chip found, activating TPM-bypass!\n");

	ima_add_boot_aggregate();	
	ima_init_policy();

	return ima_fs_init();
}

void __exit ima_cleanup(void)
{
	ima_fs_cleanup();
}
