/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём)
 *          Adrian Hunter
 */


#ifndef __UBIFS_KEY_H__
#define __UBIFS_KEY_H__

static inline uint32_t key_mask_hash(uint32_t hash)
{
	hash &= UBIFS_S_KEY_HASH_MASK;
	if (unlikely(hash <= 2))
		hash += 3;
	return hash;
}

static inline uint32_t key_r5_hash(const char *s, int len)
{
	uint32_t a = 0;
	const signed char *str = (const signed char *)s;

	while (*str) {
		a += *str << 4;
		a += *str >> 4;
		a *= 11;
		str++;
	}

	return key_mask_hash(a);
}

static inline uint32_t key_test_hash(const char *str, int len)
{
	uint32_t a = 0;

	len = min_t(uint32_t, len, 4);
	memcpy(&a, str, len);
	return key_mask_hash(a);
}

static inline void ino_key_init(const struct ubifs_info *c,
				union ubifs_key *key, ino_t inum)
{
	key->u32[0] = inum;
	key->u32[1] = UBIFS_INO_KEY << UBIFS_S_KEY_BLOCK_BITS;
}

static inline void ino_key_init_flash(const struct ubifs_info *c, void *k,
				      ino_t inum)
{
	union ubifs_key *key = k;

	key->j32[0] = cpu_to_le32(inum);
	key->j32[1] = cpu_to_le32(UBIFS_INO_KEY << UBIFS_S_KEY_BLOCK_BITS);
	memset(k + 8, 0, UBIFS_MAX_KEY_LEN - 8);
}

static inline void lowest_ino_key(const struct ubifs_info *c,
				union ubifs_key *key, ino_t inum)
{
	key->u32[0] = inum;
	key->u32[1] = 0;
}

static inline void highest_ino_key(const struct ubifs_info *c,
				union ubifs_key *key, ino_t inum)
{
	key->u32[0] = inum;
	key->u32[1] = 0xffffffff;
}

static inline void dent_key_init(const struct ubifs_info *c,
				 union ubifs_key *key, ino_t inum,
				 const struct qstr *nm)
{
	uint32_t hash = c->key_hash(nm->name, nm->len);

	ubifs_assert(!(hash & ~UBIFS_S_KEY_HASH_MASK));
	key->u32[0] = inum;
	key->u32[1] = hash | (UBIFS_DENT_KEY << UBIFS_S_KEY_HASH_BITS);
}

static inline void dent_key_init_hash(const struct ubifs_info *c,
				      union ubifs_key *key, ino_t inum,
				      uint32_t hash)
{
	ubifs_assert(!(hash & ~UBIFS_S_KEY_HASH_MASK));
	key->u32[0] = inum;
	key->u32[1] = hash | (UBIFS_DENT_KEY << UBIFS_S_KEY_HASH_BITS);
}

static inline void dent_key_init_flash(const struct ubifs_info *c, void *k,
				       ino_t inum, const struct qstr *nm)
{
	union ubifs_key *key = k;
	uint32_t hash = c->key_hash(nm->name, nm->len);

	ubifs_assert(!(hash & ~UBIFS_S_KEY_HASH_MASK));
	key->j32[0] = cpu_to_le32(inum);
	key->j32[1] = cpu_to_le32(hash |
				  (UBIFS_DENT_KEY << UBIFS_S_KEY_HASH_BITS));
	memset(k + 8, 0, UBIFS_MAX_KEY_LEN - 8);
}

static inline void lowest_dent_key(const struct ubifs_info *c,
				   union ubifs_key *key, ino_t inum)
{
	key->u32[0] = inum;
	key->u32[1] = UBIFS_DENT_KEY << UBIFS_S_KEY_HASH_BITS;
}

static inline void xent_key_init(const struct ubifs_info *c,
				 union ubifs_key *key, ino_t inum,
				 const struct qstr *nm)
{
	uint32_t hash = c->key_hash(nm->name, nm->len);

	ubifs_assert(!(hash & ~UBIFS_S_KEY_HASH_MASK));
	key->u32[0] = inum;
	key->u32[1] = hash | (UBIFS_XENT_KEY << UBIFS_S_KEY_HASH_BITS);
}

static inline void xent_key_init_flash(const struct ubifs_info *c, void *k,
				       ino_t inum, const struct qstr *nm)
{
	union ubifs_key *key = k;
	uint32_t hash = c->key_hash(nm->name, nm->len);

	ubifs_assert(!(hash & ~UBIFS_S_KEY_HASH_MASK));
	key->j32[0] = cpu_to_le32(inum);
	key->j32[1] = cpu_to_le32(hash |
				  (UBIFS_XENT_KEY << UBIFS_S_KEY_HASH_BITS));
	memset(k + 8, 0, UBIFS_MAX_KEY_LEN - 8);
}

static inline void lowest_xent_key(const struct ubifs_info *c,
				   union ubifs_key *key, ino_t inum)
{
	key->u32[0] = inum;
	key->u32[1] = UBIFS_XENT_KEY << UBIFS_S_KEY_HASH_BITS;
}

static inline void data_key_init(const struct ubifs_info *c,
				 union ubifs_key *key, ino_t inum,
				 unsigned int block)
{
	ubifs_assert(!(block & ~UBIFS_S_KEY_BLOCK_MASK));
	key->u32[0] = inum;
	key->u32[1] = block | (UBIFS_DATA_KEY << UBIFS_S_KEY_BLOCK_BITS);
}

static inline void highest_data_key(const struct ubifs_info *c,
				   union ubifs_key *key, ino_t inum)
{
	data_key_init(c, key, inum, UBIFS_S_KEY_BLOCK_MASK);
}

static inline void trun_key_init(const struct ubifs_info *c,
				 union ubifs_key *key, ino_t inum)
{
	key->u32[0] = inum;
	key->u32[1] = UBIFS_TRUN_KEY << UBIFS_S_KEY_BLOCK_BITS;
}

static inline void invalid_key_init(const struct ubifs_info *c,
				    union ubifs_key *key)
{
	key->u32[0] = 0xDEADBEAF;
	key->u32[1] = UBIFS_INVALID_KEY;
}

static inline int key_type(const struct ubifs_info *c,
			   const union ubifs_key *key)
{
	return key->u32[1] >> UBIFS_S_KEY_BLOCK_BITS;
}

static inline int key_type_flash(const struct ubifs_info *c, const void *k)
{
	const union ubifs_key *key = k;

	return le32_to_cpu(key->j32[1]) >> UBIFS_S_KEY_BLOCK_BITS;
}

static inline ino_t key_inum(const struct ubifs_info *c, const void *k)
{
	const union ubifs_key *key = k;

	return key->u32[0];
}

static inline ino_t key_inum_flash(const struct ubifs_info *c, const void *k)
{
	const union ubifs_key *key = k;

	return le32_to_cpu(key->j32[0]);
}

static inline uint32_t key_hash(const struct ubifs_info *c,
				const union ubifs_key *key)
{
	return key->u32[1] & UBIFS_S_KEY_HASH_MASK;
}

static inline uint32_t key_hash_flash(const struct ubifs_info *c, const void *k)
{
	const union ubifs_key *key = k;

	return le32_to_cpu(key->j32[1]) & UBIFS_S_KEY_HASH_MASK;
}

static inline unsigned int key_block(const struct ubifs_info *c,
				     const union ubifs_key *key)
{
	return key->u32[1] & UBIFS_S_KEY_BLOCK_MASK;
}

static inline unsigned int key_block_flash(const struct ubifs_info *c,
					   const void *k)
{
	const union ubifs_key *key = k;

	return le32_to_cpu(key->j32[1]) & UBIFS_S_KEY_BLOCK_MASK;
}

static inline void key_read(const struct ubifs_info *c, const void *from,
			    union ubifs_key *to)
{
	const union ubifs_key *f = from;

	to->u32[0] = le32_to_cpu(f->j32[0]);
	to->u32[1] = le32_to_cpu(f->j32[1]);
}

static inline void key_write(const struct ubifs_info *c,
			     const union ubifs_key *from, void *to)
{
	union ubifs_key *t = to;

	t->j32[0] = cpu_to_le32(from->u32[0]);
	t->j32[1] = cpu_to_le32(from->u32[1]);
	memset(to + 8, 0, UBIFS_MAX_KEY_LEN - 8);
}

static inline void key_write_idx(const struct ubifs_info *c,
				 const union ubifs_key *from, void *to)
{
	union ubifs_key *t = to;

	t->j32[0] = cpu_to_le32(from->u32[0]);
	t->j32[1] = cpu_to_le32(from->u32[1]);
}

static inline void key_copy(const struct ubifs_info *c,
			    const union ubifs_key *from, union ubifs_key *to)
{
	to->u64[0] = from->u64[0];
}

static inline int keys_cmp(const struct ubifs_info *c,
			   const union ubifs_key *key1,
			   const union ubifs_key *key2)
{
	if (key1->u32[0] < key2->u32[0])
		return -1;
	if (key1->u32[0] > key2->u32[0])
		return 1;
	if (key1->u32[1] < key2->u32[1])
		return -1;
	if (key1->u32[1] > key2->u32[1])
		return 1;

	return 0;
}

static inline int keys_eq(const struct ubifs_info *c,
			  const union ubifs_key *key1,
			  const union ubifs_key *key2)
{
	if (key1->u32[0] != key2->u32[0])
		return 0;
	if (key1->u32[1] != key2->u32[1])
		return 0;
	return 1;
}

static inline int is_hash_key(const struct ubifs_info *c,
			      const union ubifs_key *key)
{
	int type = key_type(c, key);

	return type == UBIFS_DENT_KEY || type == UBIFS_XENT_KEY;
}

static inline unsigned long long key_max_inode_size(const struct ubifs_info *c)
{
	switch (c->key_fmt) {
	case UBIFS_SIMPLE_KEY_FMT:
		return (1ULL << UBIFS_S_KEY_BLOCK_BITS) * UBIFS_BLOCK_SIZE;
	default:
		return 0;
	}
}

#endif 
