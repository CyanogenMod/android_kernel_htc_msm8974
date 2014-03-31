/*
 *  linux/drivers/s390/crypto/zcrypt_cca_key.h
 *
 *  zcrypt 2.1.0
 *
 *  Copyright (C)  2001, 2006 IBM Corporation
 *  Author(s): Robert Burroughs
 *	       Eric Rossman (edrossma@us.ibm.com)
 *
 *  Hotplug & misc device support: Jochen Roehrig (roehrig@de.ibm.com)
 *  Major cleanup & driver split: Martin Schwidefsky <schwidefsky@de.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _ZCRYPT_CCA_KEY_H_
#define _ZCRYPT_CCA_KEY_H_

struct T6_keyBlock_hdr {
	unsigned short blen;
	unsigned short ulen;
	unsigned short flags;
};

struct cca_token_hdr {
	unsigned char  token_identifier;
	unsigned char  version;
	unsigned short token_length;
	unsigned char  reserved[4];
} __attribute__((packed));

#define CCA_TKN_HDR_ID_EXT 0x1E

struct cca_private_ext_ME_sec {
	unsigned char  section_identifier;
	unsigned char  version;
	unsigned short section_length;
	unsigned char  private_key_hash[20];
	unsigned char  reserved1[4];
	unsigned char  key_format;
	unsigned char  reserved2;
	unsigned char  key_name_hash[20];
	unsigned char  key_use_flags[4];
	unsigned char  reserved3[6];
	unsigned char  reserved4[24];
	unsigned char  confounder[24];
	unsigned char  exponent[128];
	unsigned char  modulus[128];
} __attribute__((packed));

#define CCA_PVT_USAGE_ALL 0x80

struct cca_public_sec {
	unsigned char  section_identifier;
	unsigned char  version;
	unsigned short section_length;
	unsigned char  reserved[2];
	unsigned short exponent_len;
	unsigned short modulus_bit_len;
	unsigned short modulus_byte_len;    
} __attribute__((packed));

struct cca_pvt_ext_CRT_sec {
	unsigned char  section_identifier;
	unsigned char  version;
	unsigned short section_length;
	unsigned char  private_key_hash[20];
	unsigned char  reserved1[4];
	unsigned char  key_format;
	unsigned char  reserved2;
	unsigned char  key_name_hash[20];
	unsigned char  key_use_flags[4];
	unsigned short p_len;
	unsigned short q_len;
	unsigned short dp_len;
	unsigned short dq_len;
	unsigned short u_len;
	unsigned short mod_len;
	unsigned char  reserved3[4];
	unsigned short pad_len;
	unsigned char  reserved4[52];
	unsigned char  confounder[8];
} __attribute__((packed));

#define CCA_PVT_EXT_CRT_SEC_ID_PVT 0x08
#define CCA_PVT_EXT_CRT_SEC_FMT_CL 0x40

static inline int zcrypt_type6_mex_key_de(struct ica_rsa_modexpo *mex,
					  void *p, int big_endian)
{
	static struct cca_token_hdr static_pvt_me_hdr = {
		.token_identifier	=  0x1E,
		.token_length		=  0x0183,
	};
	static struct cca_private_ext_ME_sec static_pvt_me_sec = {
		.section_identifier	=  0x02,
		.section_length		=  0x016C,
		.key_use_flags		= {0x80,0x00,0x00,0x00},
	};
	static struct cca_public_sec static_pub_me_sec = {
		.section_identifier	=  0x04,
		.section_length		=  0x000F,
		.exponent_len		=  0x0003,
	};
	static char pk_exponent[3] = { 0x01, 0x00, 0x01 };
	struct {
		struct T6_keyBlock_hdr t6_hdr;
		struct cca_token_hdr pvtMeHdr;
		struct cca_private_ext_ME_sec pvtMeSec;
		struct cca_public_sec pubMeSec;
		char exponent[3];
	} __attribute__((packed)) *key = p;
	unsigned char *temp;

	memset(key, 0, sizeof(*key));

	if (big_endian) {
		key->t6_hdr.blen = cpu_to_be16(0x189);
		key->t6_hdr.ulen = cpu_to_be16(0x189 - 2);
	} else {
		key->t6_hdr.blen = cpu_to_le16(0x189);
		key->t6_hdr.ulen = cpu_to_le16(0x189 - 2);
	}
	key->pvtMeHdr = static_pvt_me_hdr;
	key->pvtMeSec = static_pvt_me_sec;
	key->pubMeSec = static_pub_me_sec;
	memcpy(key->exponent, pk_exponent, 3);

	
	temp = key->pvtMeSec.exponent +
		sizeof(key->pvtMeSec.exponent) - mex->inputdatalength;
	if (copy_from_user(temp, mex->b_key, mex->inputdatalength))
		return -EFAULT;

	
	temp = key->pvtMeSec.modulus +
		sizeof(key->pvtMeSec.modulus) - mex->inputdatalength;
	if (copy_from_user(temp, mex->n_modulus, mex->inputdatalength))
		return -EFAULT;
	key->pubMeSec.modulus_bit_len = 8 * mex->inputdatalength;
	return sizeof(*key);
}

static inline int zcrypt_type6_mex_key_en(struct ica_rsa_modexpo *mex,
					  void *p, int big_endian)
{
	static struct cca_token_hdr static_pub_hdr = {
		.token_identifier	=  0x1E,
	};
	static struct cca_public_sec static_pub_sec = {
		.section_identifier	=  0x04,
	};
	struct {
		struct T6_keyBlock_hdr t6_hdr;
		struct cca_token_hdr pubHdr;
		struct cca_public_sec pubSec;
		char exponent[0];
	} __attribute__((packed)) *key = p;
	unsigned char *temp;
	int i;

	memset(key, 0, sizeof(*key));

	key->pubHdr = static_pub_hdr;
	key->pubSec = static_pub_sec;

	
	temp = key->exponent;
	if (copy_from_user(temp, mex->b_key, mex->inputdatalength))
		return -EFAULT;
	
	for (i = 0; i < mex->inputdatalength; i++)
		if (temp[i])
			break;
	if (i >= mex->inputdatalength)
		return -EINVAL;
	memmove(temp, temp + i, mex->inputdatalength - i);
	temp += mex->inputdatalength - i;
	
	if (copy_from_user(temp, mex->n_modulus, mex->inputdatalength))
		return -EFAULT;

	key->pubSec.modulus_bit_len = 8 * mex->inputdatalength;
	key->pubSec.modulus_byte_len = mex->inputdatalength;
	key->pubSec.exponent_len = mex->inputdatalength - i;
	key->pubSec.section_length = sizeof(key->pubSec) +
					2*mex->inputdatalength - i;
	key->pubHdr.token_length =
		key->pubSec.section_length + sizeof(key->pubHdr);
	if (big_endian) {
		key->t6_hdr.ulen = cpu_to_be16(key->pubHdr.token_length + 4);
		key->t6_hdr.blen = cpu_to_be16(key->pubHdr.token_length + 6);
	} else {
		key->t6_hdr.ulen = cpu_to_le16(key->pubHdr.token_length + 4);
		key->t6_hdr.blen = cpu_to_le16(key->pubHdr.token_length + 6);
	}
	return sizeof(*key) + 2*mex->inputdatalength - i;
}

static inline int zcrypt_type6_crt_key(struct ica_rsa_modexpo_crt *crt,
				       void *p, int big_endian)
{
	static struct cca_public_sec static_cca_pub_sec = {
		.section_identifier = 4,
		.section_length = 0x000f,
		.exponent_len = 0x0003,
	};
	static char pk_exponent[3] = { 0x01, 0x00, 0x01 };
	struct {
		struct T6_keyBlock_hdr t6_hdr;
		struct cca_token_hdr token;
		struct cca_pvt_ext_CRT_sec pvt;
		char key_parts[0];
	} __attribute__((packed)) *key = p;
	struct cca_public_sec *pub;
	int short_len, long_len, pad_len, key_len, size;

	memset(key, 0, sizeof(*key));

	short_len = crt->inputdatalength / 2;
	long_len = short_len + 8;
	pad_len = -(3*long_len + 2*short_len) & 7;
	key_len = 3*long_len + 2*short_len + pad_len + crt->inputdatalength;
	size = sizeof(*key) + key_len + sizeof(*pub) + 3;

	
	if (big_endian) {
		key->t6_hdr.blen = cpu_to_be16(size);
		key->t6_hdr.ulen = cpu_to_be16(size - 2);
	} else {
		key->t6_hdr.blen = cpu_to_le16(size);
		key->t6_hdr.ulen = cpu_to_le16(size - 2);
	}

	
	key->token.token_identifier = CCA_TKN_HDR_ID_EXT;
	key->token.token_length = size - 6;

	
	key->pvt.section_identifier = CCA_PVT_EXT_CRT_SEC_ID_PVT;
	key->pvt.section_length = sizeof(key->pvt) + key_len;
	key->pvt.key_format = CCA_PVT_EXT_CRT_SEC_FMT_CL;
	key->pvt.key_use_flags[0] = CCA_PVT_USAGE_ALL;
	key->pvt.p_len = key->pvt.dp_len = key->pvt.u_len = long_len;
	key->pvt.q_len = key->pvt.dq_len = short_len;
	key->pvt.mod_len = crt->inputdatalength;
	key->pvt.pad_len = pad_len;

	
	if (copy_from_user(key->key_parts, crt->np_prime, long_len) ||
	    copy_from_user(key->key_parts + long_len,
					crt->nq_prime, short_len) ||
	    copy_from_user(key->key_parts + long_len + short_len,
					crt->bp_key, long_len) ||
	    copy_from_user(key->key_parts + 2*long_len + short_len,
					crt->bq_key, short_len) ||
	    copy_from_user(key->key_parts + 2*long_len + 2*short_len,
					crt->u_mult_inv, long_len))
		return -EFAULT;
	memset(key->key_parts + 3*long_len + 2*short_len + pad_len,
	       0xff, crt->inputdatalength);
	pub = (struct cca_public_sec *)(key->key_parts + key_len);
	*pub = static_cca_pub_sec;
	pub->modulus_bit_len = 8 * crt->inputdatalength;
	memcpy((char *) (pub + 1), pk_exponent, 3);
	return size;
}

#endif 
