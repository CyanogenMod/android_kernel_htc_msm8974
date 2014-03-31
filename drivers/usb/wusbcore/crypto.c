/*
 * Ultra Wide Band
 * AES-128 CCM Encryption
 *
 * Copyright (C) 2007 Intel Corporation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * We don't do any encryption here; we use the Linux Kernel's AES-128
 * crypto modules to construct keys and payload blocks in a way
 * defined by WUSB1.0[6]. Check the erratas, as typos are are patched
 * there.
 *
 * Thanks a zillion to John Keys for his help and clarifications over
 * the designed-by-a-committee text.
 *
 * So the idea is that there is this basic Pseudo-Random-Function
 * defined in WUSB1.0[6.5] which is the core of everything. It works
 * by tweaking some blocks, AES crypting them and then xoring
 * something else with them (this seems to be called CBC(AES) -- can
 * you tell I know jack about crypto?). So we just funnel it into the
 * Linux Crypto API.
 *
 * We leave a crypto test module so we can verify that vectors match,
 * every now and then.
 *
 * Block size: 16 bytes -- AES seems to do things in 'block sizes'. I
 *             am learning a lot...
 *
 *             Conveniently, some data structures that need to be
 *             funneled through AES are...16 bytes in size!
 */

#include <linux/crypto.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/uwb.h>
#include <linux/slab.h>
#include <linux/usb/wusb.h>
#include <linux/scatterlist.h>

static int debug_crypto_verify = 0;

module_param(debug_crypto_verify, int, 0);
MODULE_PARM_DESC(debug_crypto_verify, "verify the key generation algorithms");

static void wusb_key_dump(const void *buf, size_t len)
{
	print_hex_dump(KERN_ERR, "  ", DUMP_PREFIX_OFFSET, 16, 1,
		       buf, len, 0);
}

struct aes_ccm_block {
	u8 data[16];
} __attribute__((packed));


struct aes_ccm_b0 {
	u8 flags;	
	struct aes_ccm_nonce ccm_nonce;
	__be16 lm;
} __attribute__((packed));

struct aes_ccm_b1 {
	__be16 la;
	u8 mac_header[10];
	__le16 eo;
	u8 security_reserved;	
	u8 padding;		
} __attribute__((packed));

struct aes_ccm_a {
	u8 flags;	
	struct aes_ccm_nonce ccm_nonce;
	__be16 counter;	
} __attribute__((packed));

static void bytewise_xor(void *_bo, const void *_bi1, const void *_bi2,
			 size_t size)
{
	u8 *bo = _bo;
	const u8 *bi1 = _bi1, *bi2 = _bi2;
	size_t itr;
	for (itr = 0; itr < size; itr++)
		bo[itr] = bi1[itr] ^ bi2[itr];
}

static int wusb_ccm_mac(struct crypto_blkcipher *tfm_cbc,
			struct crypto_cipher *tfm_aes, void *mic,
			const struct aes_ccm_nonce *n,
			const struct aes_ccm_label *a, const void *b,
			size_t blen)
{
	int result = 0;
	struct blkcipher_desc desc;
	struct aes_ccm_b0 b0;
	struct aes_ccm_b1 b1;
	struct aes_ccm_a ax;
	struct scatterlist sg[4], sg_dst;
	void *iv, *dst_buf;
	size_t ivsize, dst_size;
	const u8 bzero[16] = { 0 };
	size_t zero_padding;

	WARN_ON(sizeof(*a) != sizeof(b1) - sizeof(b1.la));
	WARN_ON(sizeof(b0) != sizeof(struct aes_ccm_block));
	WARN_ON(sizeof(b1) != sizeof(struct aes_ccm_block));
	WARN_ON(sizeof(ax) != sizeof(struct aes_ccm_block));

	result = -ENOMEM;
	zero_padding = sizeof(struct aes_ccm_block)
		- blen % sizeof(struct aes_ccm_block);
	zero_padding = blen % sizeof(struct aes_ccm_block);
	if (zero_padding)
		zero_padding = sizeof(struct aes_ccm_block) - zero_padding;
	dst_size = blen + sizeof(b0) + sizeof(b1) + zero_padding;
	dst_buf = kzalloc(dst_size, GFP_KERNEL);
	if (dst_buf == NULL) {
		printk(KERN_ERR "E: can't alloc destination buffer\n");
		goto error_dst_buf;
	}

	iv = crypto_blkcipher_crt(tfm_cbc)->iv;
	ivsize = crypto_blkcipher_ivsize(tfm_cbc);
	memset(iv, 0, ivsize);

	
	b0.flags = 0x59;	
	b0.ccm_nonce = *n;
	b0.lm = cpu_to_be16(0);	

	b1.la = cpu_to_be16(blen + 14);
	memcpy(&b1.mac_header, a, sizeof(*a));

	sg_init_table(sg, ARRAY_SIZE(sg));
	sg_set_buf(&sg[0], &b0, sizeof(b0));
	sg_set_buf(&sg[1], &b1, sizeof(b1));
	sg_set_buf(&sg[2], b, blen);
	
	sg_set_buf(&sg[3], bzero, zero_padding);
	sg_init_one(&sg_dst, dst_buf, dst_size);

	desc.tfm = tfm_cbc;
	desc.flags = 0;
	result = crypto_blkcipher_encrypt(&desc, &sg_dst, sg, dst_size);
	if (result < 0) {
		printk(KERN_ERR "E: can't compute CBC-MAC tag (MIC): %d\n",
		       result);
		goto error_cbc_crypt;
	}

	ax.flags = 0x01;		
	ax.ccm_nonce = *n;
	ax.counter = 0;
	crypto_cipher_encrypt_one(tfm_aes, (void *)&ax, (void *)&ax);
	bytewise_xor(mic, &ax, iv, 8);
	result = 8;
error_cbc_crypt:
	kfree(dst_buf);
error_dst_buf:
	return result;
}

ssize_t wusb_prf(void *out, size_t out_size,
		 const u8 key[16], const struct aes_ccm_nonce *_n,
		 const struct aes_ccm_label *a,
		 const void *b, size_t blen, size_t len)
{
	ssize_t result, bytes = 0, bitr;
	struct aes_ccm_nonce n = *_n;
	struct crypto_blkcipher *tfm_cbc;
	struct crypto_cipher *tfm_aes;
	u64 sfn = 0;
	__le64 sfn_le;

	tfm_cbc = crypto_alloc_blkcipher("cbc(aes)", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm_cbc)) {
		result = PTR_ERR(tfm_cbc);
		printk(KERN_ERR "E: can't load CBC(AES): %d\n", (int)result);
		goto error_alloc_cbc;
	}
	result = crypto_blkcipher_setkey(tfm_cbc, key, 16);
	if (result < 0) {
		printk(KERN_ERR "E: can't set CBC key: %d\n", (int)result);
		goto error_setkey_cbc;
	}

	tfm_aes = crypto_alloc_cipher("aes", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm_aes)) {
		result = PTR_ERR(tfm_aes);
		printk(KERN_ERR "E: can't load AES: %d\n", (int)result);
		goto error_alloc_aes;
	}
	result = crypto_cipher_setkey(tfm_aes, key, 16);
	if (result < 0) {
		printk(KERN_ERR "E: can't set AES key: %d\n", (int)result);
		goto error_setkey_aes;
	}

	for (bitr = 0; bitr < (len + 63) / 64; bitr++) {
		sfn_le = cpu_to_le64(sfn++);
		memcpy(&n.sfn, &sfn_le, sizeof(n.sfn));	
		result = wusb_ccm_mac(tfm_cbc, tfm_aes, out + bytes,
				      &n, a, b, blen);
		if (result < 0)
			goto error_ccm_mac;
		bytes += result;
	}
	result = bytes;
error_ccm_mac:
error_setkey_aes:
	crypto_free_cipher(tfm_aes);
error_alloc_aes:
error_setkey_cbc:
	crypto_free_blkcipher(tfm_cbc);
error_alloc_cbc:
	return result;
}

static const u8 stv_hsmic_key[16] = {
	0x4b, 0x79, 0xa3, 0xcf, 0xe5, 0x53, 0x23, 0x9d,
	0xd7, 0xc1, 0x6d, 0x1c, 0x2d, 0xab, 0x6d, 0x3f
};

static const struct aes_ccm_nonce stv_hsmic_n = {
	.sfn = { 0 },
	.tkid = { 0x76, 0x98, 0x01,  },
	.dest_addr = { .data = { 0xbe, 0x00 } },
		.src_addr = { .data = { 0x76, 0x98 } },
};

static int wusb_oob_mic_verify(void)
{
	int result;
	u8 mic[8];
	struct usb_handshake stv_hsmic_hs = {
		.bMessageNumber = 2,
		.bStatus 	= 00,
		.tTKID 		= { 0x76, 0x98, 0x01 },
		.bReserved 	= 00,
		.CDID 		= { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
				    0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
				    0x3c, 0x3d, 0x3e, 0x3f },
		.nonce	 	= { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
				    0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
				    0x2c, 0x2d, 0x2e, 0x2f },
		.MIC	 	= { 0x75, 0x6a, 0x97, 0x51, 0x0c, 0x8c,
				    0x14, 0x7b } ,
	};
	size_t hs_size;

	result = wusb_oob_mic(mic, stv_hsmic_key, &stv_hsmic_n, &stv_hsmic_hs);
	if (result < 0)
		printk(KERN_ERR "E: WUSB OOB MIC test: failed: %d\n", result);
	else if (memcmp(stv_hsmic_hs.MIC, mic, sizeof(mic))) {
		printk(KERN_ERR "E: OOB MIC test: "
		       "mismatch between MIC result and WUSB1.0[A2]\n");
		hs_size = sizeof(stv_hsmic_hs) - sizeof(stv_hsmic_hs.MIC);
		printk(KERN_ERR "E: Handshake2 in: (%zu bytes)\n", hs_size);
		wusb_key_dump(&stv_hsmic_hs, hs_size);
		printk(KERN_ERR "E: CCM Nonce in: (%zu bytes)\n",
		       sizeof(stv_hsmic_n));
		wusb_key_dump(&stv_hsmic_n, sizeof(stv_hsmic_n));
		printk(KERN_ERR "E: MIC out:\n");
		wusb_key_dump(mic, sizeof(mic));
		printk(KERN_ERR "E: MIC out (from WUSB1.0[A.2]):\n");
		wusb_key_dump(stv_hsmic_hs.MIC, sizeof(stv_hsmic_hs.MIC));
		result = -EINVAL;
	} else
		result = 0;
	return result;
}

static const u8 stv_key_a1[16] __attribute__ ((__aligned__(4))) = {
	0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87,
	0x78, 0x69, 0x5a, 0x4b, 0x3c, 0x2d, 0x1e, 0x0f
};

static const struct aes_ccm_nonce stv_keydvt_n_a1 = {
	.sfn = { 0 },
	.tkid = { 0x76, 0x98, 0x01,  },
	.dest_addr = { .data = { 0xbe, 0x00 } },
	.src_addr = { .data = { 0x76, 0x98 } },
};

static const struct wusb_keydvt_out stv_keydvt_out_a1 = {
	.kck = {
		0x4b, 0x79, 0xa3, 0xcf, 0xe5, 0x53, 0x23, 0x9d,
		0xd7, 0xc1, 0x6d, 0x1c, 0x2d, 0xab, 0x6d, 0x3f
	},
	.ptk = {
		0xc8, 0x70, 0x62, 0x82, 0xb6, 0x7c, 0xe9, 0x06,
		0x7b, 0xc5, 0x25, 0x69, 0xf2, 0x36, 0x61, 0x2d
	}
};

static int wusb_key_derive_verify(void)
{
	int result = 0;
	struct wusb_keydvt_out keydvt_out;
	struct wusb_keydvt_in stv_keydvt_in_a1 = {
		.hnonce = {
			0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
			0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
		},
		.dnonce = {
			0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
			0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f
		}
	};

	result = wusb_key_derive(&keydvt_out, stv_key_a1, &stv_keydvt_n_a1,
				 &stv_keydvt_in_a1);
	if (result < 0)
		printk(KERN_ERR "E: WUSB key derivation test: "
		       "derivation failed: %d\n", result);
	if (memcmp(&stv_keydvt_out_a1, &keydvt_out, sizeof(keydvt_out))) {
		printk(KERN_ERR "E: WUSB key derivation test: "
		       "mismatch between key derivation result "
		       "and WUSB1.0[A1] Errata 2006/12\n");
		printk(KERN_ERR "E: keydvt in: key\n");
		wusb_key_dump(stv_key_a1, sizeof(stv_key_a1));
		printk(KERN_ERR "E: keydvt in: nonce\n");
		wusb_key_dump( &stv_keydvt_n_a1, sizeof(stv_keydvt_n_a1));
		printk(KERN_ERR "E: keydvt in: hnonce & dnonce\n");
		wusb_key_dump(&stv_keydvt_in_a1, sizeof(stv_keydvt_in_a1));
		printk(KERN_ERR "E: keydvt out: KCK\n");
		wusb_key_dump(&keydvt_out.kck, sizeof(keydvt_out.kck));
		printk(KERN_ERR "E: keydvt out: PTK\n");
		wusb_key_dump(&keydvt_out.ptk, sizeof(keydvt_out.ptk));
		result = -EINVAL;
	} else
		result = 0;
	return result;
}

int wusb_crypto_init(void)
{
	int result;

	if (debug_crypto_verify) {
		result = wusb_key_derive_verify();
		if (result < 0)
			return result;
		return wusb_oob_mic_verify();
	}
	return 0;
}

void wusb_crypto_exit(void)
{
	
}
