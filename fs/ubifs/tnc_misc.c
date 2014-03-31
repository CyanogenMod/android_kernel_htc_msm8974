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
 * Authors: Adrian Hunter
 *          Artem Bityutskiy (Битюцкий Артём)
 */


#include "ubifs.h"

struct ubifs_znode *ubifs_tnc_levelorder_next(struct ubifs_znode *zr,
					      struct ubifs_znode *znode)
{
	int level, iip, level_search = 0;
	struct ubifs_znode *zn;

	ubifs_assert(zr);

	if (unlikely(!znode))
		return zr;

	if (unlikely(znode == zr)) {
		if (znode->level == 0)
			return NULL;
		return ubifs_tnc_find_child(zr, 0);
	}

	level = znode->level;

	iip = znode->iip;
	while (1) {
		ubifs_assert(znode->level <= zr->level);

		while (znode->parent != zr && iip >= znode->parent->child_cnt) {
			znode = znode->parent;
			iip = znode->iip;
		}

		if (unlikely(znode->parent == zr &&
			     iip >= znode->parent->child_cnt)) {
			
			level -= 1;
			if (level_search || level < 0)
				return NULL;

			level_search = 1;
			iip = -1;
			znode = ubifs_tnc_find_child(zr, 0);
			ubifs_assert(znode);
		}

		
		zn = ubifs_tnc_find_child(znode->parent, iip + 1);
		if (!zn) {
			
			iip = znode->parent->child_cnt;
			continue;
		}

		
		while (zn->level != level) {
			znode = zn;
			zn = ubifs_tnc_find_child(zn, 0);
			if (!zn) {
				iip = znode->iip;
				break;
			}
		}

		if (zn) {
			ubifs_assert(zn->level >= 0);
			return zn;
		}
	}
}

int ubifs_search_zbranch(const struct ubifs_info *c,
			 const struct ubifs_znode *znode,
			 const union ubifs_key *key, int *n)
{
	int beg = 0, end = znode->child_cnt, uninitialized_var(mid);
	int uninitialized_var(cmp);
	const struct ubifs_zbranch *zbr = &znode->zbranch[0];

	ubifs_assert(end > beg);

	while (end > beg) {
		mid = (beg + end) >> 1;
		cmp = keys_cmp(c, key, &zbr[mid].key);
		if (cmp > 0)
			beg = mid + 1;
		else if (cmp < 0)
			end = mid;
		else {
			*n = mid;
			return 1;
		}
	}

	*n = end - 1;

	
	ubifs_assert(*n >= -1 && *n < znode->child_cnt);
	if (*n == -1)
		ubifs_assert(keys_cmp(c, key, &zbr[0].key) < 0);
	else
		ubifs_assert(keys_cmp(c, key, &zbr[*n].key) > 0);
	if (*n + 1 < znode->child_cnt)
		ubifs_assert(keys_cmp(c, key, &zbr[*n + 1].key) < 0);

	return 0;
}

struct ubifs_znode *ubifs_tnc_postorder_first(struct ubifs_znode *znode)
{
	if (unlikely(!znode))
		return NULL;

	while (znode->level > 0) {
		struct ubifs_znode *child;

		child = ubifs_tnc_find_child(znode, 0);
		if (!child)
			return znode;
		znode = child;
	}

	return znode;
}

struct ubifs_znode *ubifs_tnc_postorder_next(struct ubifs_znode *znode)
{
	struct ubifs_znode *zn;

	ubifs_assert(znode);
	if (unlikely(!znode->parent))
		return NULL;

	
	zn = ubifs_tnc_find_child(znode->parent, znode->iip + 1);
	if (!zn)
		
		return znode->parent;

	
	return ubifs_tnc_postorder_first(zn);
}

long ubifs_destroy_tnc_subtree(struct ubifs_znode *znode)
{
	struct ubifs_znode *zn = ubifs_tnc_postorder_first(znode);
	long clean_freed = 0;
	int n;

	ubifs_assert(zn);
	while (1) {
		for (n = 0; n < zn->child_cnt; n++) {
			if (!zn->zbranch[n].znode)
				continue;

			if (zn->level > 0 &&
			    !ubifs_zn_dirty(zn->zbranch[n].znode))
				clean_freed += 1;

			cond_resched();
			kfree(zn->zbranch[n].znode);
		}

		if (zn == znode) {
			if (!ubifs_zn_dirty(zn))
				clean_freed += 1;
			kfree(zn);
			return clean_freed;
		}

		zn = ubifs_tnc_postorder_next(zn);
	}
}

static int read_znode(struct ubifs_info *c, int lnum, int offs, int len,
		      struct ubifs_znode *znode)
{
	int i, err, type, cmp;
	struct ubifs_idx_node *idx;

	idx = kmalloc(c->max_idx_node_sz, GFP_NOFS);
	if (!idx)
		return -ENOMEM;

	err = ubifs_read_node(c, idx, UBIFS_IDX_NODE, len, lnum, offs);
	if (err < 0) {
		kfree(idx);
		return err;
	}

	znode->child_cnt = le16_to_cpu(idx->child_cnt);
	znode->level = le16_to_cpu(idx->level);

	dbg_tnc("LEB %d:%d, level %d, %d branch",
		lnum, offs, znode->level, znode->child_cnt);

	if (znode->child_cnt > c->fanout || znode->level > UBIFS_MAX_LEVELS) {
		dbg_err("current fanout %d, branch count %d",
			c->fanout, znode->child_cnt);
		dbg_err("max levels %d, znode level %d",
			UBIFS_MAX_LEVELS, znode->level);
		err = 1;
		goto out_dump;
	}

	for (i = 0; i < znode->child_cnt; i++) {
		const struct ubifs_branch *br = ubifs_idx_branch(c, idx, i);
		struct ubifs_zbranch *zbr = &znode->zbranch[i];

		key_read(c, &br->key, &zbr->key);
		zbr->lnum = le32_to_cpu(br->lnum);
		zbr->offs = le32_to_cpu(br->offs);
		zbr->len  = le32_to_cpu(br->len);
		zbr->znode = NULL;

		

		if (zbr->lnum < c->main_first ||
		    zbr->lnum >= c->leb_cnt || zbr->offs < 0 ||
		    zbr->offs + zbr->len > c->leb_size || zbr->offs & 7) {
			dbg_err("bad branch %d", i);
			err = 2;
			goto out_dump;
		}

		switch (key_type(c, &zbr->key)) {
		case UBIFS_INO_KEY:
		case UBIFS_DATA_KEY:
		case UBIFS_DENT_KEY:
		case UBIFS_XENT_KEY:
			break;
		default:
			dbg_msg("bad key type at slot %d: %d",
				i, key_type(c, &zbr->key));
			err = 3;
			goto out_dump;
		}

		if (znode->level)
			continue;

		type = key_type(c, &zbr->key);
		if (c->ranges[type].max_len == 0) {
			if (zbr->len != c->ranges[type].len) {
				dbg_err("bad target node (type %d) length (%d)",
					type, zbr->len);
				dbg_err("have to be %d", c->ranges[type].len);
				err = 4;
				goto out_dump;
			}
		} else if (zbr->len < c->ranges[type].min_len ||
			   zbr->len > c->ranges[type].max_len) {
			dbg_err("bad target node (type %d) length (%d)",
				type, zbr->len);
			dbg_err("have to be in range of %d-%d",
				c->ranges[type].min_len,
				c->ranges[type].max_len);
			err = 5;
			goto out_dump;
		}
	}

	for (i = 0; i < znode->child_cnt - 1; i++) {
		const union ubifs_key *key1, *key2;

		key1 = &znode->zbranch[i].key;
		key2 = &znode->zbranch[i + 1].key;

		cmp = keys_cmp(c, key1, key2);
		if (cmp > 0) {
			dbg_err("bad key order (keys %d and %d)", i, i + 1);
			err = 6;
			goto out_dump;
		} else if (cmp == 0 && !is_hash_key(c, key1)) {
			
			dbg_err("keys %d and %d are not hashed but equivalent",
				i, i + 1);
			err = 7;
			goto out_dump;
		}
	}

	kfree(idx);
	return 0;

out_dump:
	ubifs_err("bad indexing node at LEB %d:%d, error %d", lnum, offs, err);
	dbg_dump_node(c, idx);
	kfree(idx);
	return -EINVAL;
}

struct ubifs_znode *ubifs_load_znode(struct ubifs_info *c,
				     struct ubifs_zbranch *zbr,
				     struct ubifs_znode *parent, int iip)
{
	int err;
	struct ubifs_znode *znode;

	ubifs_assert(!zbr->znode);
	znode = kzalloc(c->max_znode_sz, GFP_NOFS);
	if (!znode)
		return ERR_PTR(-ENOMEM);

	err = read_znode(c, zbr->lnum, zbr->offs, zbr->len, znode);
	if (err)
		goto out;

	atomic_long_inc(&c->clean_zn_cnt);

	atomic_long_inc(&ubifs_clean_zn_cnt);

	zbr->znode = znode;
	znode->parent = parent;
	znode->time = get_seconds();
	znode->iip = iip;

	return znode;

out:
	kfree(znode);
	return ERR_PTR(err);
}

int ubifs_tnc_read_node(struct ubifs_info *c, struct ubifs_zbranch *zbr,
			void *node)
{
	union ubifs_key key1, *key = &zbr->key;
	int err, type = key_type(c, key);
	struct ubifs_wbuf *wbuf;

	wbuf = ubifs_get_wbuf(c, zbr->lnum);
	if (wbuf)
		err = ubifs_read_node_wbuf(wbuf, node, type, zbr->len,
					   zbr->lnum, zbr->offs);
	else
		err = ubifs_read_node(c, node, type, zbr->len, zbr->lnum,
				      zbr->offs);

	if (err) {
		dbg_tnck(key, "key ");
		return err;
	}

	
	key_read(c, node + UBIFS_KEY_OFFSET, &key1);
	if (!keys_eq(c, key, &key1)) {
		ubifs_err("bad key in node at LEB %d:%d",
			  zbr->lnum, zbr->offs);
		dbg_tnck(key, "looked for key ");
		dbg_tnck(&key1, "but found node's key ");
		dbg_dump_node(c, node);
		return -EINVAL;
	}

	return 0;
}
