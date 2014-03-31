/*
 * Copyright (C) 2009-2010 IBM Corporation
 *
 * Authors:
 * Mimi Zohar <zohar@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 */

#include <linux/types.h>
#include <linux/integrity.h>
#include <crypto/sha.h>

#define IMA_MEASURED		0x01

enum evm_ima_xattr_type {
	IMA_XATTR_DIGEST = 0x01,
	EVM_XATTR_HMAC,
	EVM_IMA_XATTR_DIGSIG,
};

struct evm_ima_xattr_data {
	u8 type;
	u8 digest[SHA1_DIGEST_SIZE];
}  __attribute__((packed));

struct integrity_iint_cache {
	struct rb_node rb_node; 
	struct inode *inode;	
	u64 version;		
	unsigned char flags;
	u8 digest[SHA1_DIGEST_SIZE];
	struct mutex mutex;	
	enum integrity_status evm_status;
};

struct integrity_iint_cache *integrity_iint_insert(struct inode *inode);
struct integrity_iint_cache *integrity_iint_find(struct inode *inode);

#define INTEGRITY_KEYRING_EVM		0
#define INTEGRITY_KEYRING_MODULE	1
#define INTEGRITY_KEYRING_IMA		2
#define INTEGRITY_KEYRING_MAX		3

#ifdef CONFIG_INTEGRITY_SIGNATURE

int integrity_digsig_verify(const unsigned int id, const char *sig, int siglen,
					const char *digest, int digestlen);

#else

static inline int integrity_digsig_verify(const unsigned int id,
					  const char *sig, int siglen,
					  const char *digest, int digestlen)
{
	return -EOPNOTSUPP;
}

#endif 

extern int iint_initialized;
