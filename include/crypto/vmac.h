/*
 * Modified to interface to the Linux kernel
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef __CRYPTO_VMAC_H
#define __CRYPTO_VMAC_H


#define VMAC_TAG_LEN	64
#define VMAC_KEY_SIZE	128
#define VMAC_KEY_LEN	(VMAC_KEY_SIZE/8)
#define VMAC_NHBYTES	128

struct vmac_ctx {
	u64 nhkey[(VMAC_NHBYTES/8)+2*(VMAC_TAG_LEN/64-1)];
	u64 polykey[2*VMAC_TAG_LEN/64];
	u64 l3key[2*VMAC_TAG_LEN/64];
	u64 polytmp[2*VMAC_TAG_LEN/64];
	u64 cached_nonce[2];
	u64 cached_aes[2];
	int first_block_processed;
};

typedef u64 vmac_t;

struct vmac_ctx_t {
	struct crypto_cipher *child;
	struct vmac_ctx __vmac_ctx;
};

#endif 
