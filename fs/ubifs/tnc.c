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


#include <linux/crc32.h>
#include <linux/slab.h>
#include "ubifs.h"

enum {
	NAME_LESS    = 0,
	NAME_MATCHES = 1,
	NAME_GREATER = 2,
	NOT_ON_MEDIA = 3,
};

static int insert_old_idx(struct ubifs_info *c, int lnum, int offs)
{
	struct ubifs_old_idx *old_idx, *o;
	struct rb_node **p, *parent = NULL;

	old_idx = kmalloc(sizeof(struct ubifs_old_idx), GFP_NOFS);
	if (unlikely(!old_idx))
		return -ENOMEM;
	old_idx->lnum = lnum;
	old_idx->offs = offs;

	p = &c->old_idx.rb_node;
	while (*p) {
		parent = *p;
		o = rb_entry(parent, struct ubifs_old_idx, rb);
		if (lnum < o->lnum)
			p = &(*p)->rb_left;
		else if (lnum > o->lnum)
			p = &(*p)->rb_right;
		else if (offs < o->offs)
			p = &(*p)->rb_left;
		else if (offs > o->offs)
			p = &(*p)->rb_right;
		else {
			ubifs_err("old idx added twice!");
			kfree(old_idx);
			return 0;
		}
	}
	rb_link_node(&old_idx->rb, parent, p);
	rb_insert_color(&old_idx->rb, &c->old_idx);
	return 0;
}

int insert_old_idx_znode(struct ubifs_info *c, struct ubifs_znode *znode)
{
	if (znode->parent) {
		struct ubifs_zbranch *zbr;

		zbr = &znode->parent->zbranch[znode->iip];
		if (zbr->len)
			return insert_old_idx(c, zbr->lnum, zbr->offs);
	} else
		if (c->zroot.len)
			return insert_old_idx(c, c->zroot.lnum,
					      c->zroot.offs);
	return 0;
}

static int ins_clr_old_idx_znode(struct ubifs_info *c,
				 struct ubifs_znode *znode)
{
	int err;

	if (znode->parent) {
		struct ubifs_zbranch *zbr;

		zbr = &znode->parent->zbranch[znode->iip];
		if (zbr->len) {
			err = insert_old_idx(c, zbr->lnum, zbr->offs);
			if (err)
				return err;
			zbr->lnum = 0;
			zbr->offs = 0;
			zbr->len = 0;
		}
	} else
		if (c->zroot.len) {
			err = insert_old_idx(c, c->zroot.lnum, c->zroot.offs);
			if (err)
				return err;
			c->zroot.lnum = 0;
			c->zroot.offs = 0;
			c->zroot.len = 0;
		}
	return 0;
}

/**
 * destroy_old_idx - destroy the old_idx RB-tree.
 * @c: UBIFS file-system description object
 *
 * During start commit, the old_idx RB-tree is used to avoid overwriting index
 * nodes that were in the index last commit but have since been deleted.  This
 * is necessary for recovery i.e. the old index must be kept intact until the
 * new index is successfully written.  The old-idx RB-tree is used for the
 * in-the-gaps method of writing index nodes and is destroyed every commit.
 */
void destroy_old_idx(struct ubifs_info *c)
{
	struct rb_node *this = c->old_idx.rb_node;
	struct ubifs_old_idx *old_idx;

	while (this) {
		if (this->rb_left) {
			this = this->rb_left;
			continue;
		} else if (this->rb_right) {
			this = this->rb_right;
			continue;
		}
		old_idx = rb_entry(this, struct ubifs_old_idx, rb);
		this = rb_parent(this);
		if (this) {
			if (this->rb_left == &old_idx->rb)
				this->rb_left = NULL;
			else
				this->rb_right = NULL;
		}
		kfree(old_idx);
	}
	c->old_idx = RB_ROOT;
}

static struct ubifs_znode *copy_znode(struct ubifs_info *c,
				      struct ubifs_znode *znode)
{
	struct ubifs_znode *zn;

	zn = kmalloc(c->max_znode_sz, GFP_NOFS);
	if (unlikely(!zn))
		return ERR_PTR(-ENOMEM);

	memcpy(zn, znode, c->max_znode_sz);
	zn->cnext = NULL;
	__set_bit(DIRTY_ZNODE, &zn->flags);
	__clear_bit(COW_ZNODE, &zn->flags);

	ubifs_assert(!ubifs_zn_obsolete(znode));
	__set_bit(OBSOLETE_ZNODE, &znode->flags);

	if (znode->level != 0) {
		int i;
		const int n = zn->child_cnt;

		
		for (i = 0; i < n; i++) {
			struct ubifs_zbranch *zbr = &zn->zbranch[i];

			if (zbr->znode)
				zbr->znode->parent = zn;
		}
	}

	atomic_long_inc(&c->dirty_zn_cnt);
	return zn;
}

static int add_idx_dirt(struct ubifs_info *c, int lnum, int dirt)
{
	c->calc_idx_sz -= ALIGN(dirt, 8);
	return ubifs_add_dirt(c, lnum, dirt);
}

static struct ubifs_znode *dirty_cow_znode(struct ubifs_info *c,
					   struct ubifs_zbranch *zbr)
{
	struct ubifs_znode *znode = zbr->znode;
	struct ubifs_znode *zn;
	int err;

	if (!ubifs_zn_cow(znode)) {
		
		if (!test_and_set_bit(DIRTY_ZNODE, &znode->flags)) {
			atomic_long_inc(&c->dirty_zn_cnt);
			atomic_long_dec(&c->clean_zn_cnt);
			atomic_long_dec(&ubifs_clean_zn_cnt);
			err = add_idx_dirt(c, zbr->lnum, zbr->len);
			if (unlikely(err))
				return ERR_PTR(err);
		}
		return znode;
	}

	zn = copy_znode(c, znode);
	if (IS_ERR(zn))
		return zn;

	if (zbr->len) {
		err = insert_old_idx(c, zbr->lnum, zbr->offs);
		if (unlikely(err))
			return ERR_PTR(err);
		err = add_idx_dirt(c, zbr->lnum, zbr->len);
	} else
		err = 0;

	zbr->znode = zn;
	zbr->lnum = 0;
	zbr->offs = 0;
	zbr->len = 0;

	if (unlikely(err))
		return ERR_PTR(err);
	return zn;
}

static int lnc_add(struct ubifs_info *c, struct ubifs_zbranch *zbr,
		   const void *node)
{
	int err;
	void *lnc_node;
	const struct ubifs_dent_node *dent = node;

	ubifs_assert(!zbr->leaf);
	ubifs_assert(zbr->len != 0);
	ubifs_assert(is_hash_key(c, &zbr->key));

	err = ubifs_validate_entry(c, dent);
	if (err) {
		dbg_dump_stack();
		dbg_dump_node(c, dent);
		return err;
	}

	lnc_node = kmemdup(node, zbr->len, GFP_NOFS);
	if (!lnc_node)
		
		return 0;

	zbr->leaf = lnc_node;
	return 0;
}

static int lnc_add_directly(struct ubifs_info *c, struct ubifs_zbranch *zbr,
			    void *node)
{
	int err;

	ubifs_assert(!zbr->leaf);
	ubifs_assert(zbr->len != 0);

	err = ubifs_validate_entry(c, node);
	if (err) {
		dbg_dump_stack();
		dbg_dump_node(c, node);
		return err;
	}

	zbr->leaf = node;
	return 0;
}

static void lnc_free(struct ubifs_zbranch *zbr)
{
	if (!zbr->leaf)
		return;
	kfree(zbr->leaf);
	zbr->leaf = NULL;
}

static int tnc_read_node_nm(struct ubifs_info *c, struct ubifs_zbranch *zbr,
			    void *node)
{
	int err;

	ubifs_assert(is_hash_key(c, &zbr->key));

	if (zbr->leaf) {
		
		ubifs_assert(zbr->len != 0);
		memcpy(node, zbr->leaf, zbr->len);
		return 0;
	}

	err = ubifs_tnc_read_node(c, zbr, node);
	if (err)
		return err;

	
	err = lnc_add(c, zbr, node);
	return err;
}

static int try_read_node(const struct ubifs_info *c, void *buf, int type,
			 int len, int lnum, int offs)
{
	int err, node_len;
	struct ubifs_ch *ch = buf;
	uint32_t crc, node_crc;

	dbg_io("LEB %d:%d, %s, length %d", lnum, offs, dbg_ntype(type), len);

	err = ubifs_leb_read(c, lnum, buf, offs, len, 1);
	if (err) {
		ubifs_err("cannot read node type %d from LEB %d:%d, error %d",
			  type, lnum, offs, err);
		return err;
	}

	if (le32_to_cpu(ch->magic) != UBIFS_NODE_MAGIC)
		return 0;

	if (ch->node_type != type)
		return 0;

	node_len = le32_to_cpu(ch->len);
	if (node_len != len)
		return 0;

	if (type == UBIFS_DATA_NODE && c->no_chk_data_crc && !c->mounting &&
	    !c->remounting_rw)
		return 1;

	crc = crc32(UBIFS_CRC32_INIT, buf + 8, node_len - 8);
	node_crc = le32_to_cpu(ch->crc);
	if (crc != node_crc)
		return 0;

	return 1;
}

static int fallible_read_node(struct ubifs_info *c, const union ubifs_key *key,
			      struct ubifs_zbranch *zbr, void *node)
{
	int ret;

	dbg_tnck(key, "LEB %d:%d, key ", zbr->lnum, zbr->offs);

	ret = try_read_node(c, node, key_type(c, key), zbr->len, zbr->lnum,
			    zbr->offs);
	if (ret == 1) {
		union ubifs_key node_key;
		struct ubifs_dent_node *dent = node;

		
		key_read(c, &dent->key, &node_key);
		if (keys_cmp(c, key, &node_key) != 0)
			ret = 0;
	}
	if (ret == 0 && c->replaying)
		dbg_mntk(key, "dangling branch LEB %d:%d len %d, key ",
			zbr->lnum, zbr->offs, zbr->len);
	return ret;
}

static int matches_name(struct ubifs_info *c, struct ubifs_zbranch *zbr,
			const struct qstr *nm)
{
	struct ubifs_dent_node *dent;
	int nlen, err;

	
	if (!zbr->leaf) {
		dent = kmalloc(zbr->len, GFP_NOFS);
		if (!dent)
			return -ENOMEM;

		err = ubifs_tnc_read_node(c, zbr, dent);
		if (err)
			goto out_free;

		
		err = lnc_add_directly(c, zbr, dent);
		if (err)
			goto out_free;
	} else
		dent = zbr->leaf;

	nlen = le16_to_cpu(dent->nlen);
	err = memcmp(dent->name, nm->name, min_t(int, nlen, nm->len));
	if (err == 0) {
		if (nlen == nm->len)
			return NAME_MATCHES;
		else if (nlen < nm->len)
			return NAME_LESS;
		else
			return NAME_GREATER;
	} else if (err < 0)
		return NAME_LESS;
	else
		return NAME_GREATER;

out_free:
	kfree(dent);
	return err;
}

static struct ubifs_znode *get_znode(struct ubifs_info *c,
				     struct ubifs_znode *znode, int n)
{
	struct ubifs_zbranch *zbr;

	zbr = &znode->zbranch[n];
	if (zbr->znode)
		znode = zbr->znode;
	else
		znode = ubifs_load_znode(c, zbr, znode, n);
	return znode;
}

static int tnc_next(struct ubifs_info *c, struct ubifs_znode **zn, int *n)
{
	struct ubifs_znode *znode = *zn;
	int nn = *n;

	nn += 1;
	if (nn < znode->child_cnt) {
		*n = nn;
		return 0;
	}
	while (1) {
		struct ubifs_znode *zp;

		zp = znode->parent;
		if (!zp)
			return -ENOENT;
		nn = znode->iip + 1;
		znode = zp;
		if (nn < znode->child_cnt) {
			znode = get_znode(c, znode, nn);
			if (IS_ERR(znode))
				return PTR_ERR(znode);
			while (znode->level != 0) {
				znode = get_znode(c, znode, 0);
				if (IS_ERR(znode))
					return PTR_ERR(znode);
			}
			nn = 0;
			break;
		}
	}
	*zn = znode;
	*n = nn;
	return 0;
}

static int tnc_prev(struct ubifs_info *c, struct ubifs_znode **zn, int *n)
{
	struct ubifs_znode *znode = *zn;
	int nn = *n;

	if (nn > 0) {
		*n = nn - 1;
		return 0;
	}
	while (1) {
		struct ubifs_znode *zp;

		zp = znode->parent;
		if (!zp)
			return -ENOENT;
		nn = znode->iip - 1;
		znode = zp;
		if (nn >= 0) {
			znode = get_znode(c, znode, nn);
			if (IS_ERR(znode))
				return PTR_ERR(znode);
			while (znode->level != 0) {
				nn = znode->child_cnt - 1;
				znode = get_znode(c, znode, nn);
				if (IS_ERR(znode))
					return PTR_ERR(znode);
			}
			nn = znode->child_cnt - 1;
			break;
		}
	}
	*zn = znode;
	*n = nn;
	return 0;
}

static int resolve_collision(struct ubifs_info *c, const union ubifs_key *key,
			     struct ubifs_znode **zn, int *n,
			     const struct qstr *nm)
{
	int err;

	err = matches_name(c, &(*zn)->zbranch[*n], nm);
	if (unlikely(err < 0))
		return err;
	if (err == NAME_MATCHES)
		return 1;

	if (err == NAME_GREATER) {
		
		while (1) {
			err = tnc_prev(c, zn, n);
			if (err == -ENOENT) {
				ubifs_assert(*n == 0);
				*n = -1;
				return 0;
			}
			if (err < 0)
				return err;
			if (keys_cmp(c, &(*zn)->zbranch[*n].key, key)) {
				if (*n == (*zn)->child_cnt - 1) {
					err = tnc_next(c, zn, n);
					if (err) {
						
						ubifs_assert(0);
						if (err == -ENOENT)
							err = -EINVAL;
						return err;
					}
					ubifs_assert(*n == 0);
					*n = -1;
				}
				return 0;
			}
			err = matches_name(c, &(*zn)->zbranch[*n], nm);
			if (err < 0)
				return err;
			if (err == NAME_LESS)
				return 0;
			if (err == NAME_MATCHES)
				return 1;
			ubifs_assert(err == NAME_GREATER);
		}
	} else {
		int nn = *n;
		struct ubifs_znode *znode = *zn;

		
		while (1) {
			err = tnc_next(c, &znode, &nn);
			if (err == -ENOENT)
				return 0;
			if (err < 0)
				return err;
			if (keys_cmp(c, &znode->zbranch[nn].key, key))
				return 0;
			err = matches_name(c, &znode->zbranch[nn], nm);
			if (err < 0)
				return err;
			if (err == NAME_GREATER)
				return 0;
			*zn = znode;
			*n = nn;
			if (err == NAME_MATCHES)
				return 1;
			ubifs_assert(err == NAME_LESS);
		}
	}
}

static int fallible_matches_name(struct ubifs_info *c,
				 struct ubifs_zbranch *zbr,
				 const struct qstr *nm)
{
	struct ubifs_dent_node *dent;
	int nlen, err;

	
	if (!zbr->leaf) {
		dent = kmalloc(zbr->len, GFP_NOFS);
		if (!dent)
			return -ENOMEM;

		err = fallible_read_node(c, &zbr->key, zbr, dent);
		if (err < 0)
			goto out_free;
		if (err == 0) {
			
			err = NOT_ON_MEDIA;
			goto out_free;
		}
		ubifs_assert(err == 1);

		err = lnc_add_directly(c, zbr, dent);
		if (err)
			goto out_free;
	} else
		dent = zbr->leaf;

	nlen = le16_to_cpu(dent->nlen);
	err = memcmp(dent->name, nm->name, min_t(int, nlen, nm->len));
	if (err == 0) {
		if (nlen == nm->len)
			return NAME_MATCHES;
		else if (nlen < nm->len)
			return NAME_LESS;
		else
			return NAME_GREATER;
	} else if (err < 0)
		return NAME_LESS;
	else
		return NAME_GREATER;

out_free:
	kfree(dent);
	return err;
}

static int fallible_resolve_collision(struct ubifs_info *c,
				      const union ubifs_key *key,
				      struct ubifs_znode **zn, int *n,
				      const struct qstr *nm, int adding)
{
	struct ubifs_znode *o_znode = NULL, *znode = *zn;
	int uninitialized_var(o_n), err, cmp, unsure = 0, nn = *n;

	cmp = fallible_matches_name(c, &znode->zbranch[nn], nm);
	if (unlikely(cmp < 0))
		return cmp;
	if (cmp == NAME_MATCHES)
		return 1;
	if (cmp == NOT_ON_MEDIA) {
		o_znode = znode;
		o_n = nn;
		unsure = 1;
	} else if (!adding)
		unsure = 1; 

	if (cmp == NAME_GREATER || unsure) {
		
		while (1) {
			err = tnc_prev(c, zn, n);
			if (err == -ENOENT) {
				ubifs_assert(*n == 0);
				*n = -1;
				break;
			}
			if (err < 0)
				return err;
			if (keys_cmp(c, &(*zn)->zbranch[*n].key, key)) {
				
				if (*n == (*zn)->child_cnt - 1) {
					err = tnc_next(c, zn, n);
					if (err) {
						
						ubifs_assert(0);
						if (err == -ENOENT)
							err = -EINVAL;
						return err;
					}
					ubifs_assert(*n == 0);
					*n = -1;
				}
				break;
			}
			err = fallible_matches_name(c, &(*zn)->zbranch[*n], nm);
			if (err < 0)
				return err;
			if (err == NAME_MATCHES)
				return 1;
			if (err == NOT_ON_MEDIA) {
				o_znode = *zn;
				o_n = *n;
				continue;
			}
			if (!adding)
				continue;
			if (err == NAME_LESS)
				break;
			else
				unsure = 0;
		}
	}

	if (cmp == NAME_LESS || unsure) {
		
		*zn = znode;
		*n = nn;
		while (1) {
			err = tnc_next(c, &znode, &nn);
			if (err == -ENOENT)
				break;
			if (err < 0)
				return err;
			if (keys_cmp(c, &znode->zbranch[nn].key, key))
				break;
			err = fallible_matches_name(c, &znode->zbranch[nn], nm);
			if (err < 0)
				return err;
			if (err == NAME_GREATER)
				break;
			*zn = znode;
			*n = nn;
			if (err == NAME_MATCHES)
				return 1;
			if (err == NOT_ON_MEDIA) {
				o_znode = znode;
				o_n = nn;
			}
		}
	}

	
	if (adding || !o_znode)
		return 0;

	dbg_mntk(key, "dangling match LEB %d:%d len %d key ",
		o_znode->zbranch[o_n].lnum, o_znode->zbranch[o_n].offs,
		o_znode->zbranch[o_n].len);
	*zn = o_znode;
	*n = o_n;
	return 1;
}

static int matches_position(struct ubifs_zbranch *zbr, int lnum, int offs)
{
	if (zbr->lnum == lnum && zbr->offs == offs)
		return 1;
	else
		return 0;
}

static int resolve_collision_directly(struct ubifs_info *c,
				      const union ubifs_key *key,
				      struct ubifs_znode **zn, int *n,
				      int lnum, int offs)
{
	struct ubifs_znode *znode;
	int nn, err;

	znode = *zn;
	nn = *n;
	if (matches_position(&znode->zbranch[nn], lnum, offs))
		return 1;

	
	while (1) {
		err = tnc_prev(c, &znode, &nn);
		if (err == -ENOENT)
			break;
		if (err < 0)
			return err;
		if (keys_cmp(c, &znode->zbranch[nn].key, key))
			break;
		if (matches_position(&znode->zbranch[nn], lnum, offs)) {
			*zn = znode;
			*n = nn;
			return 1;
		}
	}

	
	znode = *zn;
	nn = *n;
	while (1) {
		err = tnc_next(c, &znode, &nn);
		if (err == -ENOENT)
			return 0;
		if (err < 0)
			return err;
		if (keys_cmp(c, &znode->zbranch[nn].key, key))
			return 0;
		*zn = znode;
		*n = nn;
		if (matches_position(&znode->zbranch[nn], lnum, offs))
			return 1;
	}
}

static struct ubifs_znode *dirty_cow_bottom_up(struct ubifs_info *c,
					       struct ubifs_znode *znode)
{
	struct ubifs_znode *zp;
	int *path = c->bottom_up_buf, p = 0;

	ubifs_assert(c->zroot.znode);
	ubifs_assert(znode);
	if (c->zroot.znode->level > BOTTOM_UP_HEIGHT) {
		kfree(c->bottom_up_buf);
		c->bottom_up_buf = kmalloc(c->zroot.znode->level * sizeof(int),
					   GFP_NOFS);
		if (!c->bottom_up_buf)
			return ERR_PTR(-ENOMEM);
		path = c->bottom_up_buf;
	}
	if (c->zroot.znode->level) {
		
		while (1) {
			int n;

			zp = znode->parent;
			if (!zp)
				break;
			n = znode->iip;
			ubifs_assert(p < c->zroot.znode->level);
			path[p++] = n;
			if (!zp->cnext && ubifs_zn_dirty(znode))
				break;
			znode = zp;
		}
	}

	
	while (1) {
		struct ubifs_zbranch *zbr;

		zp = znode->parent;
		if (zp) {
			ubifs_assert(path[p - 1] >= 0);
			ubifs_assert(path[p - 1] < zp->child_cnt);
			zbr = &zp->zbranch[path[--p]];
			znode = dirty_cow_znode(c, zbr);
		} else {
			ubifs_assert(znode == c->zroot.znode);
			znode = dirty_cow_znode(c, &c->zroot);
		}
		if (IS_ERR(znode) || !p)
			break;
		ubifs_assert(path[p - 1] >= 0);
		ubifs_assert(path[p - 1] < znode->child_cnt);
		znode = znode->zbranch[path[p - 1]].znode;
	}

	return znode;
}

int ubifs_lookup_level0(struct ubifs_info *c, const union ubifs_key *key,
			struct ubifs_znode **zn, int *n)
{
	int err, exact;
	struct ubifs_znode *znode;
	unsigned long time = get_seconds();

	dbg_tnck(key, "search key ");
	ubifs_assert(key_type(c, key) < UBIFS_INVALID_KEY);

	znode = c->zroot.znode;
	if (unlikely(!znode)) {
		znode = ubifs_load_znode(c, &c->zroot, NULL, 0);
		if (IS_ERR(znode))
			return PTR_ERR(znode);
	}

	znode->time = time;

	while (1) {
		struct ubifs_zbranch *zbr;

		exact = ubifs_search_zbranch(c, znode, key, n);

		if (znode->level == 0)
			break;

		if (*n < 0)
			*n = 0;
		zbr = &znode->zbranch[*n];

		if (zbr->znode) {
			znode->time = time;
			znode = zbr->znode;
			continue;
		}

		
		znode = ubifs_load_znode(c, zbr, znode, *n);
		if (IS_ERR(znode))
			return PTR_ERR(znode);
	}

	*zn = znode;
	if (exact || !is_hash_key(c, key) || *n != -1) {
		dbg_tnc("found %d, lvl %d, n %d", exact, znode->level, *n);
		return exact;
	}

	err = tnc_prev(c, &znode, n);
	if (err == -ENOENT) {
		dbg_tnc("found 0, lvl %d, n -1", znode->level);
		*n = -1;
		return 0;
	}
	if (unlikely(err < 0))
		return err;
	if (keys_cmp(c, key, &znode->zbranch[*n].key)) {
		dbg_tnc("found 0, lvl %d, n -1", znode->level);
		*n = -1;
		return 0;
	}

	dbg_tnc("found 1, lvl %d, n %d", znode->level, *n);
	*zn = znode;
	return 1;
}

static int lookup_level0_dirty(struct ubifs_info *c, const union ubifs_key *key,
			       struct ubifs_znode **zn, int *n)
{
	int err, exact;
	struct ubifs_znode *znode;
	unsigned long time = get_seconds();

	dbg_tnck(key, "search and dirty key ");

	znode = c->zroot.znode;
	if (unlikely(!znode)) {
		znode = ubifs_load_znode(c, &c->zroot, NULL, 0);
		if (IS_ERR(znode))
			return PTR_ERR(znode);
	}

	znode = dirty_cow_znode(c, &c->zroot);
	if (IS_ERR(znode))
		return PTR_ERR(znode);

	znode->time = time;

	while (1) {
		struct ubifs_zbranch *zbr;

		exact = ubifs_search_zbranch(c, znode, key, n);

		if (znode->level == 0)
			break;

		if (*n < 0)
			*n = 0;
		zbr = &znode->zbranch[*n];

		if (zbr->znode) {
			znode->time = time;
			znode = dirty_cow_znode(c, zbr);
			if (IS_ERR(znode))
				return PTR_ERR(znode);
			continue;
		}

		
		znode = ubifs_load_znode(c, zbr, znode, *n);
		if (IS_ERR(znode))
			return PTR_ERR(znode);
		znode = dirty_cow_znode(c, zbr);
		if (IS_ERR(znode))
			return PTR_ERR(znode);
	}

	*zn = znode;
	if (exact || !is_hash_key(c, key) || *n != -1) {
		dbg_tnc("found %d, lvl %d, n %d", exact, znode->level, *n);
		return exact;
	}

	err = tnc_prev(c, &znode, n);
	if (err == -ENOENT) {
		*n = -1;
		dbg_tnc("found 0, lvl %d, n -1", znode->level);
		return 0;
	}
	if (unlikely(err < 0))
		return err;
	if (keys_cmp(c, key, &znode->zbranch[*n].key)) {
		*n = -1;
		dbg_tnc("found 0, lvl %d, n -1", znode->level);
		return 0;
	}

	if (znode->cnext || !ubifs_zn_dirty(znode)) {
		znode = dirty_cow_bottom_up(c, znode);
		if (IS_ERR(znode))
			return PTR_ERR(znode);
	}

	dbg_tnc("found 1, lvl %d, n %d", znode->level, *n);
	*zn = znode;
	return 1;
}

static int maybe_leb_gced(struct ubifs_info *c, int lnum, int gc_seq1)
{
	int gc_seq2, gced_lnum;

	gced_lnum = c->gced_lnum;
	smp_rmb();
	gc_seq2 = c->gc_seq;
	
	if (gc_seq1 == gc_seq2)
		return 0;
	
	if (gc_seq1 + 1 != gc_seq2)
		return 1;
	smp_rmb();
	if (gced_lnum != c->gced_lnum)
		return 1;
	
	if (gced_lnum == lnum)
		return 1;
	return 0;
}

int ubifs_tnc_locate(struct ubifs_info *c, const union ubifs_key *key,
		     void *node, int *lnum, int *offs)
{
	int found, n, err, safely = 0, gc_seq1;
	struct ubifs_znode *znode;
	struct ubifs_zbranch zbr, *zt;

again:
	mutex_lock(&c->tnc_mutex);
	found = ubifs_lookup_level0(c, key, &znode, &n);
	if (!found) {
		err = -ENOENT;
		goto out;
	} else if (found < 0) {
		err = found;
		goto out;
	}
	zt = &znode->zbranch[n];
	if (lnum) {
		*lnum = zt->lnum;
		*offs = zt->offs;
	}
	if (is_hash_key(c, key)) {
		err = tnc_read_node_nm(c, zt, node);
		goto out;
	}
	if (safely) {
		err = ubifs_tnc_read_node(c, zt, node);
		goto out;
	}
	
	zbr = znode->zbranch[n];
	gc_seq1 = c->gc_seq;
	mutex_unlock(&c->tnc_mutex);

	if (ubifs_get_wbuf(c, zbr.lnum)) {
		
		err = ubifs_tnc_read_node(c, &zbr, node);
		return err;
	}

	err = fallible_read_node(c, key, &zbr, node);
	if (err <= 0 || maybe_leb_gced(c, zbr.lnum, gc_seq1)) {
		safely = 1;
		goto again;
	}
	return 0;

out:
	mutex_unlock(&c->tnc_mutex);
	return err;
}

int ubifs_tnc_get_bu_keys(struct ubifs_info *c, struct bu_info *bu)
{
	int n, err = 0, lnum = -1, uninitialized_var(offs);
	int uninitialized_var(len);
	unsigned int block = key_block(c, &bu->key);
	struct ubifs_znode *znode;

	bu->cnt = 0;
	bu->blk_cnt = 0;
	bu->eof = 0;

	mutex_lock(&c->tnc_mutex);
	
	err = ubifs_lookup_level0(c, &bu->key, &znode, &n);
	if (err < 0)
		goto out;
	if (err) {
		
		len = znode->zbranch[n].len;
		
		if (len > bu->buf_len) {
			err = -EINVAL;
			goto out;
		}
		
		bu->zbranch[bu->cnt++] = znode->zbranch[n];
		bu->blk_cnt += 1;
		lnum = znode->zbranch[n].lnum;
		offs = ALIGN(znode->zbranch[n].offs + len, 8);
	}
	while (1) {
		struct ubifs_zbranch *zbr;
		union ubifs_key *key;
		unsigned int next_block;

		
		err = tnc_next(c, &znode, &n);
		if (err)
			goto out;
		zbr = &znode->zbranch[n];
		key = &zbr->key;
		
		if (key_inum(c, key) != key_inum(c, &bu->key) ||
		    key_type(c, key) != UBIFS_DATA_KEY) {
			err = -ENOENT;
			goto out;
		}
		if (lnum < 0) {
			
			lnum = zbr->lnum;
			offs = ALIGN(zbr->offs + zbr->len, 8);
			len = zbr->len;
			if (len > bu->buf_len) {
				err = -EINVAL;
				goto out;
			}
		} else {
			if (zbr->lnum != lnum || zbr->offs != offs)
				goto out;
			offs += ALIGN(zbr->len, 8);
			len = ALIGN(len, 8) + zbr->len;
			
			if (len > bu->buf_len)
				goto out;
		}
		
		next_block = key_block(c, key);
		bu->blk_cnt += (next_block - block - 1);
		if (bu->blk_cnt >= UBIFS_MAX_BULK_READ)
			goto out;
		block = next_block;
		
		bu->zbranch[bu->cnt++] = *zbr;
		bu->blk_cnt += 1;
		
		if (bu->cnt >= UBIFS_MAX_BULK_READ)
			goto out;
		if (bu->blk_cnt >= UBIFS_MAX_BULK_READ)
			goto out;
	}
out:
	if (err == -ENOENT) {
		bu->eof = 1;
		err = 0;
	}
	bu->gc_seq = c->gc_seq;
	mutex_unlock(&c->tnc_mutex);
	if (err)
		return err;
	if (bu->blk_cnt > UBIFS_MAX_BULK_READ)
		bu->blk_cnt = UBIFS_MAX_BULK_READ;
	if (UBIFS_BLOCKS_PER_PAGE == 1 ||
	    !(bu->blk_cnt & (UBIFS_BLOCKS_PER_PAGE - 1)))
		return 0;
	if (bu->eof) {
		
		bu->blk_cnt += UBIFS_BLOCKS_PER_PAGE - 1;
		return 0;
	}
	
	block = key_block(c, &bu->key) + bu->blk_cnt;
	block &= ~(UBIFS_BLOCKS_PER_PAGE - 1);
	while (bu->cnt) {
		if (key_block(c, &bu->zbranch[bu->cnt - 1].key) < block)
			break;
		bu->cnt -= 1;
	}
	return 0;
}

static int read_wbuf(struct ubifs_wbuf *wbuf, void *buf, int len, int lnum,
		     int offs)
{
	const struct ubifs_info *c = wbuf->c;
	int rlen, overlap;

	dbg_io("LEB %d:%d, length %d", lnum, offs, len);
	ubifs_assert(wbuf && lnum >= 0 && lnum < c->leb_cnt && offs >= 0);
	ubifs_assert(!(offs & 7) && offs < c->leb_size);
	ubifs_assert(offs + len <= c->leb_size);

	spin_lock(&wbuf->lock);
	overlap = (lnum == wbuf->lnum && offs + len > wbuf->offs);
	if (!overlap) {
		
		spin_unlock(&wbuf->lock);
		return ubifs_leb_read(c, lnum, buf, offs, len, 0);
	}

	
	rlen = wbuf->offs - offs;
	if (rlen < 0)
		rlen = 0;

	
	memcpy(buf + rlen, wbuf->buf + offs + rlen - wbuf->offs, len - rlen);
	spin_unlock(&wbuf->lock);

	if (rlen > 0)
		
		return ubifs_leb_read(c, lnum, buf, offs, rlen, 0);

	return 0;
}

static int validate_data_node(struct ubifs_info *c, void *buf,
			      struct ubifs_zbranch *zbr)
{
	union ubifs_key key1;
	struct ubifs_ch *ch = buf;
	int err, len;

	if (ch->node_type != UBIFS_DATA_NODE) {
		ubifs_err("bad node type (%d but expected %d)",
			  ch->node_type, UBIFS_DATA_NODE);
		goto out_err;
	}

	err = ubifs_check_node(c, buf, zbr->lnum, zbr->offs, 0, 0);
	if (err) {
		ubifs_err("expected node type %d", UBIFS_DATA_NODE);
		goto out;
	}

	len = le32_to_cpu(ch->len);
	if (len != zbr->len) {
		ubifs_err("bad node length %d, expected %d", len, zbr->len);
		goto out_err;
	}

	
	key_read(c, buf + UBIFS_KEY_OFFSET, &key1);
	if (!keys_eq(c, &zbr->key, &key1)) {
		ubifs_err("bad key in node at LEB %d:%d",
			  zbr->lnum, zbr->offs);
		dbg_tnck(&zbr->key, "looked for key ");
		dbg_tnck(&key1, "found node's key ");
		goto out_err;
	}

	return 0;

out_err:
	err = -EINVAL;
out:
	ubifs_err("bad node at LEB %d:%d", zbr->lnum, zbr->offs);
	dbg_dump_node(c, buf);
	dbg_dump_stack();
	return err;
}

int ubifs_tnc_bulk_read(struct ubifs_info *c, struct bu_info *bu)
{
	int lnum = bu->zbranch[0].lnum, offs = bu->zbranch[0].offs, len, err, i;
	struct ubifs_wbuf *wbuf;
	void *buf;

	len = bu->zbranch[bu->cnt - 1].offs;
	len += bu->zbranch[bu->cnt - 1].len - offs;
	if (len > bu->buf_len) {
		ubifs_err("buffer too small %d vs %d", bu->buf_len, len);
		return -EINVAL;
	}

	
	wbuf = ubifs_get_wbuf(c, lnum);
	if (wbuf)
		err = read_wbuf(wbuf, bu->buf, len, lnum, offs);
	else
		err = ubifs_leb_read(c, lnum, bu->buf, offs, len, 0);

	
	if (maybe_leb_gced(c, lnum, bu->gc_seq))
		return -EAGAIN;

	if (err && err != -EBADMSG) {
		ubifs_err("failed to read from LEB %d:%d, error %d",
			  lnum, offs, err);
		dbg_dump_stack();
		dbg_tnck(&bu->key, "key ");
		return err;
	}

	
	buf = bu->buf;
	for (i = 0; i < bu->cnt; i++) {
		err = validate_data_node(c, buf, &bu->zbranch[i]);
		if (err)
			return err;
		buf = buf + ALIGN(bu->zbranch[i].len, 8);
	}

	return 0;
}

static int do_lookup_nm(struct ubifs_info *c, const union ubifs_key *key,
			void *node, const struct qstr *nm)
{
	int found, n, err;
	struct ubifs_znode *znode;

	dbg_tnck(key, "name '%.*s' key ", nm->len, nm->name);
	mutex_lock(&c->tnc_mutex);
	found = ubifs_lookup_level0(c, key, &znode, &n);
	if (!found) {
		err = -ENOENT;
		goto out_unlock;
	} else if (found < 0) {
		err = found;
		goto out_unlock;
	}

	ubifs_assert(n >= 0);

	err = resolve_collision(c, key, &znode, &n, nm);
	dbg_tnc("rc returned %d, znode %p, n %d", err, znode, n);
	if (unlikely(err < 0))
		goto out_unlock;
	if (err == 0) {
		err = -ENOENT;
		goto out_unlock;
	}

	err = tnc_read_node_nm(c, &znode->zbranch[n], node);

out_unlock:
	mutex_unlock(&c->tnc_mutex);
	return err;
}

int ubifs_tnc_lookup_nm(struct ubifs_info *c, const union ubifs_key *key,
			void *node, const struct qstr *nm)
{
	int err, len;
	const struct ubifs_dent_node *dent = node;

	err = ubifs_tnc_lookup(c, key, node);
	if (err)
		return err;

	len = le16_to_cpu(dent->nlen);
	if (nm->len == len && !memcmp(dent->name, nm->name, len))
		return 0;

	return do_lookup_nm(c, key, node, nm);
}

static void correct_parent_keys(const struct ubifs_info *c,
				struct ubifs_znode *znode)
{
	union ubifs_key *key, *key1;

	ubifs_assert(znode->parent);
	ubifs_assert(znode->iip == 0);

	key = &znode->zbranch[0].key;
	key1 = &znode->parent->zbranch[0].key;

	while (keys_cmp(c, key, key1) < 0) {
		key_copy(c, key, key1);
		znode = znode->parent;
		znode->alt = 1;
		if (!znode->parent || znode->iip)
			break;
		key1 = &znode->parent->zbranch[0].key;
	}
}

static void insert_zbranch(struct ubifs_znode *znode,
			   const struct ubifs_zbranch *zbr, int n)
{
	int i;

	ubifs_assert(ubifs_zn_dirty(znode));

	if (znode->level) {
		for (i = znode->child_cnt; i > n; i--) {
			znode->zbranch[i] = znode->zbranch[i - 1];
			if (znode->zbranch[i].znode)
				znode->zbranch[i].znode->iip = i;
		}
		if (zbr->znode)
			zbr->znode->iip = n;
	} else
		for (i = znode->child_cnt; i > n; i--)
			znode->zbranch[i] = znode->zbranch[i - 1];

	znode->zbranch[n] = *zbr;
	znode->child_cnt += 1;

	if (n == 0)
		znode->alt = 1;
}

static int tnc_insert(struct ubifs_info *c, struct ubifs_znode *znode,
		      struct ubifs_zbranch *zbr, int n)
{
	struct ubifs_znode *zn, *zi, *zp;
	int i, keep, move, appending = 0;
	union ubifs_key *key = &zbr->key, *key1;

	ubifs_assert(n >= 0 && n <= c->fanout);

	
again:
	zp = znode->parent;
	if (znode->child_cnt < c->fanout) {
		ubifs_assert(n != c->fanout);
		dbg_tnck(key, "inserted at %d level %d, key ", n, znode->level);

		insert_zbranch(znode, zbr, n);

		
		if (n == 0 && zp && znode->iip == 0)
			correct_parent_keys(c, znode);

		return 0;
	}

	dbg_tnck(key, "splitting level %d, key ", znode->level);

	if (znode->alt)
		ins_clr_old_idx_znode(c, znode);

	zn = kzalloc(c->max_znode_sz, GFP_NOFS);
	if (!zn)
		return -ENOMEM;
	zn->parent = zp;
	zn->level = znode->level;

	
	if (znode->level == 0 && key_type(c, key) == UBIFS_DATA_KEY) {
		
		if (n == c->fanout) {
			key1 = &znode->zbranch[n - 1].key;
			if (key_inum(c, key1) == key_inum(c, key) &&
			    key_type(c, key1) == UBIFS_DATA_KEY)
				appending = 1;
		} else
			goto check_split;
	} else if (appending && n != c->fanout) {
		
		appending = 0;
check_split:
		if (n >= (c->fanout + 1) / 2) {
			key1 = &znode->zbranch[0].key;
			if (key_inum(c, key1) == key_inum(c, key) &&
			    key_type(c, key1) == UBIFS_DATA_KEY) {
				key1 = &znode->zbranch[n].key;
				if (key_inum(c, key1) != key_inum(c, key) ||
				    key_type(c, key1) != UBIFS_DATA_KEY) {
					keep = n;
					move = c->fanout - keep;
					zi = znode;
					goto do_split;
				}
			}
		}
	}

	if (appending) {
		keep = c->fanout;
		move = 0;
	} else {
		keep = (c->fanout + 1) / 2;
		move = c->fanout - keep;
	}


	if (n < keep) {
		
		zi = znode;
		move += 1;
		keep -= 1;
	} else {
		
		zi = zn;
		n -= keep;
		
		if (zn->level != 0)
			zbr->znode->parent = zn;
	}

do_split:

	__set_bit(DIRTY_ZNODE, &zn->flags);
	atomic_long_inc(&c->dirty_zn_cnt);

	zn->child_cnt = move;
	znode->child_cnt = keep;

	dbg_tnc("moving %d, keeping %d", move, keep);

	
	for (i = 0; i < move; i++) {
		zn->zbranch[i] = znode->zbranch[keep + i];
		
		if (zn->level != 0)
			if (zn->zbranch[i].znode) {
				zn->zbranch[i].znode->parent = zn;
				zn->zbranch[i].znode->iip = i;
			}
	}

	
	dbg_tnck(key, "inserting at %d level %d, key ", n, zn->level);

	insert_zbranch(zi, zbr, n);

	
	if (zp) {
		if (n == 0 && zi == znode && znode->iip == 0)
			correct_parent_keys(c, znode);

		
		n = znode->iip + 1;

		
		zbr->key = zn->zbranch[0].key;
		zbr->znode = zn;
		zbr->lnum = 0;
		zbr->offs = 0;
		zbr->len = 0;
		znode = zp;

		goto again;
	}

	
	dbg_tnc("creating new zroot at level %d", znode->level + 1);

	zi = kzalloc(c->max_znode_sz, GFP_NOFS);
	if (!zi)
		return -ENOMEM;

	zi->child_cnt = 2;
	zi->level = znode->level + 1;

	__set_bit(DIRTY_ZNODE, &zi->flags);
	atomic_long_inc(&c->dirty_zn_cnt);

	zi->zbranch[0].key = znode->zbranch[0].key;
	zi->zbranch[0].znode = znode;
	zi->zbranch[0].lnum = c->zroot.lnum;
	zi->zbranch[0].offs = c->zroot.offs;
	zi->zbranch[0].len = c->zroot.len;
	zi->zbranch[1].key = zn->zbranch[0].key;
	zi->zbranch[1].znode = zn;

	c->zroot.lnum = 0;
	c->zroot.offs = 0;
	c->zroot.len = 0;
	c->zroot.znode = zi;

	zn->parent = zi;
	zn->iip = 1;
	znode->parent = zi;
	znode->iip = 0;

	return 0;
}

int ubifs_tnc_add(struct ubifs_info *c, const union ubifs_key *key, int lnum,
		  int offs, int len)
{
	int found, n, err = 0;
	struct ubifs_znode *znode;

	mutex_lock(&c->tnc_mutex);
	dbg_tnck(key, "%d:%d, len %d, key ", lnum, offs, len);
	found = lookup_level0_dirty(c, key, &znode, &n);
	if (!found) {
		struct ubifs_zbranch zbr;

		zbr.znode = NULL;
		zbr.lnum = lnum;
		zbr.offs = offs;
		zbr.len = len;
		key_copy(c, key, &zbr.key);
		err = tnc_insert(c, znode, &zbr, n + 1);
	} else if (found == 1) {
		struct ubifs_zbranch *zbr = &znode->zbranch[n];

		lnc_free(zbr);
		err = ubifs_add_dirt(c, zbr->lnum, zbr->len);
		zbr->lnum = lnum;
		zbr->offs = offs;
		zbr->len = len;
	} else
		err = found;
	if (!err)
		err = dbg_check_tnc(c, 0);
	mutex_unlock(&c->tnc_mutex);

	return err;
}

int ubifs_tnc_replace(struct ubifs_info *c, const union ubifs_key *key,
		      int old_lnum, int old_offs, int lnum, int offs, int len)
{
	int found, n, err = 0;
	struct ubifs_znode *znode;

	mutex_lock(&c->tnc_mutex);
	dbg_tnck(key, "old LEB %d:%d, new LEB %d:%d, len %d, key ", old_lnum,
		 old_offs, lnum, offs, len);
	found = lookup_level0_dirty(c, key, &znode, &n);
	if (found < 0) {
		err = found;
		goto out_unlock;
	}

	if (found == 1) {
		struct ubifs_zbranch *zbr = &znode->zbranch[n];

		found = 0;
		if (zbr->lnum == old_lnum && zbr->offs == old_offs) {
			lnc_free(zbr);
			err = ubifs_add_dirt(c, zbr->lnum, zbr->len);
			if (err)
				goto out_unlock;
			zbr->lnum = lnum;
			zbr->offs = offs;
			zbr->len = len;
			found = 1;
		} else if (is_hash_key(c, key)) {
			found = resolve_collision_directly(c, key, &znode, &n,
							   old_lnum, old_offs);
			dbg_tnc("rc returned %d, znode %p, n %d, LEB %d:%d",
				found, znode, n, old_lnum, old_offs);
			if (found < 0) {
				err = found;
				goto out_unlock;
			}

			if (found) {
				
				if (znode->cnext || !ubifs_zn_dirty(znode)) {
					znode = dirty_cow_bottom_up(c, znode);
					if (IS_ERR(znode)) {
						err = PTR_ERR(znode);
						goto out_unlock;
					}
				}
				zbr = &znode->zbranch[n];
				lnc_free(zbr);
				err = ubifs_add_dirt(c, zbr->lnum,
						     zbr->len);
				if (err)
					goto out_unlock;
				zbr->lnum = lnum;
				zbr->offs = offs;
				zbr->len = len;
			}
		}
	}

	if (!found)
		err = ubifs_add_dirt(c, lnum, len);

	if (!err)
		err = dbg_check_tnc(c, 0);

out_unlock:
	mutex_unlock(&c->tnc_mutex);
	return err;
}

int ubifs_tnc_add_nm(struct ubifs_info *c, const union ubifs_key *key,
		     int lnum, int offs, int len, const struct qstr *nm)
{
	int found, n, err = 0;
	struct ubifs_znode *znode;

	mutex_lock(&c->tnc_mutex);
	dbg_tnck(key, "LEB %d:%d, name '%.*s', key ",
		 lnum, offs, nm->len, nm->name);
	found = lookup_level0_dirty(c, key, &znode, &n);
	if (found < 0) {
		err = found;
		goto out_unlock;
	}

	if (found == 1) {
		if (c->replaying)
			found = fallible_resolve_collision(c, key, &znode, &n,
							   nm, 1);
		else
			found = resolve_collision(c, key, &znode, &n, nm);
		dbg_tnc("rc returned %d, znode %p, n %d", found, znode, n);
		if (found < 0) {
			err = found;
			goto out_unlock;
		}

		
		if (znode->cnext || !ubifs_zn_dirty(znode)) {
			znode = dirty_cow_bottom_up(c, znode);
			if (IS_ERR(znode)) {
				err = PTR_ERR(znode);
				goto out_unlock;
			}
		}

		if (found == 1) {
			struct ubifs_zbranch *zbr = &znode->zbranch[n];

			lnc_free(zbr);
			err = ubifs_add_dirt(c, zbr->lnum, zbr->len);
			zbr->lnum = lnum;
			zbr->offs = offs;
			zbr->len = len;
			goto out_unlock;
		}
	}

	if (!found) {
		struct ubifs_zbranch zbr;

		zbr.znode = NULL;
		zbr.lnum = lnum;
		zbr.offs = offs;
		zbr.len = len;
		key_copy(c, key, &zbr.key);
		err = tnc_insert(c, znode, &zbr, n + 1);
		if (err)
			goto out_unlock;
		if (c->replaying) {
			struct qstr noname = { .len = 0, .name = "" };

			err = dbg_check_tnc(c, 0);
			mutex_unlock(&c->tnc_mutex);
			if (err)
				return err;
			return ubifs_tnc_remove_nm(c, key, &noname);
		}
	}

out_unlock:
	if (!err)
		err = dbg_check_tnc(c, 0);
	mutex_unlock(&c->tnc_mutex);
	return err;
}

static int tnc_delete(struct ubifs_info *c, struct ubifs_znode *znode, int n)
{
	struct ubifs_zbranch *zbr;
	struct ubifs_znode *zp;
	int i, err;

	
	ubifs_assert(znode->level == 0);
	ubifs_assert(n >= 0 && n < c->fanout);
	dbg_tnck(&znode->zbranch[n].key, "deleting key ");

	zbr = &znode->zbranch[n];
	lnc_free(zbr);

	err = ubifs_add_dirt(c, zbr->lnum, zbr->len);
	if (err) {
		dbg_dump_znode(c, znode);
		return err;
	}

	
	for (i = n; i < znode->child_cnt - 1; i++)
		znode->zbranch[i] = znode->zbranch[i + 1];
	znode->child_cnt -= 1;

	if (znode->child_cnt > 0)
		return 0;


	do {
		ubifs_assert(!ubifs_zn_obsolete(znode));
		ubifs_assert(ubifs_zn_dirty(znode));

		zp = znode->parent;
		n = znode->iip;

		atomic_long_dec(&c->dirty_zn_cnt);

		err = insert_old_idx_znode(c, znode);
		if (err)
			return err;

		if (znode->cnext) {
			__set_bit(OBSOLETE_ZNODE, &znode->flags);
			atomic_long_inc(&c->clean_zn_cnt);
			atomic_long_inc(&ubifs_clean_zn_cnt);
		} else
			kfree(znode);
		znode = zp;
	} while (znode->child_cnt == 1); 

	
	znode->child_cnt -= 1;
	ubifs_assert(znode->level != 0);
	for (i = n; i < znode->child_cnt; i++) {
		znode->zbranch[i] = znode->zbranch[i + 1];
		if (znode->zbranch[i].znode)
			znode->zbranch[i].znode->iip = i;
	}

	if (!znode->parent) {
		while (znode->child_cnt == 1 && znode->level != 0) {
			zp = znode;
			zbr = &znode->zbranch[0];
			znode = get_znode(c, znode, 0);
			if (IS_ERR(znode))
				return PTR_ERR(znode);
			znode = dirty_cow_znode(c, zbr);
			if (IS_ERR(znode))
				return PTR_ERR(znode);
			znode->parent = NULL;
			znode->iip = 0;
			if (c->zroot.len) {
				err = insert_old_idx(c, c->zroot.lnum,
						     c->zroot.offs);
				if (err)
					return err;
			}
			c->zroot.lnum = zbr->lnum;
			c->zroot.offs = zbr->offs;
			c->zroot.len = zbr->len;
			c->zroot.znode = znode;
			ubifs_assert(!ubifs_zn_obsolete(zp));
			ubifs_assert(ubifs_zn_dirty(zp));
			atomic_long_dec(&c->dirty_zn_cnt);

			if (zp->cnext) {
				__set_bit(OBSOLETE_ZNODE, &zp->flags);
				atomic_long_inc(&c->clean_zn_cnt);
				atomic_long_inc(&ubifs_clean_zn_cnt);
			} else
				kfree(zp);
		}
	}

	return 0;
}

int ubifs_tnc_remove(struct ubifs_info *c, const union ubifs_key *key)
{
	int found, n, err = 0;
	struct ubifs_znode *znode;

	mutex_lock(&c->tnc_mutex);
	dbg_tnck(key, "key ");
	found = lookup_level0_dirty(c, key, &znode, &n);
	if (found < 0) {
		err = found;
		goto out_unlock;
	}
	if (found == 1)
		err = tnc_delete(c, znode, n);
	if (!err)
		err = dbg_check_tnc(c, 0);

out_unlock:
	mutex_unlock(&c->tnc_mutex);
	return err;
}

int ubifs_tnc_remove_nm(struct ubifs_info *c, const union ubifs_key *key,
			const struct qstr *nm)
{
	int n, err;
	struct ubifs_znode *znode;

	mutex_lock(&c->tnc_mutex);
	dbg_tnck(key, "%.*s, key ", nm->len, nm->name);
	err = lookup_level0_dirty(c, key, &znode, &n);
	if (err < 0)
		goto out_unlock;

	if (err) {
		if (c->replaying)
			err = fallible_resolve_collision(c, key, &znode, &n,
							 nm, 0);
		else
			err = resolve_collision(c, key, &znode, &n, nm);
		dbg_tnc("rc returned %d, znode %p, n %d", err, znode, n);
		if (err < 0)
			goto out_unlock;
		if (err) {
			
			if (znode->cnext || !ubifs_zn_dirty(znode)) {
				znode = dirty_cow_bottom_up(c, znode);
				if (IS_ERR(znode)) {
					err = PTR_ERR(znode);
					goto out_unlock;
				}
			}
			err = tnc_delete(c, znode, n);
		}
	}

out_unlock:
	if (!err)
		err = dbg_check_tnc(c, 0);
	mutex_unlock(&c->tnc_mutex);
	return err;
}

static int key_in_range(struct ubifs_info *c, union ubifs_key *key,
			union ubifs_key *from_key, union ubifs_key *to_key)
{
	if (keys_cmp(c, key, from_key) < 0)
		return 0;
	if (keys_cmp(c, key, to_key) > 0)
		return 0;
	return 1;
}

int ubifs_tnc_remove_range(struct ubifs_info *c, union ubifs_key *from_key,
			   union ubifs_key *to_key)
{
	int i, n, k, err = 0;
	struct ubifs_znode *znode;
	union ubifs_key *key;

	mutex_lock(&c->tnc_mutex);
	while (1) {
		
		err = ubifs_lookup_level0(c, from_key, &znode, &n);
		if (err < 0)
			goto out_unlock;

		if (err)
			key = from_key;
		else {
			err = tnc_next(c, &znode, &n);
			if (err == -ENOENT) {
				err = 0;
				goto out_unlock;
			}
			if (err < 0)
				goto out_unlock;
			key = &znode->zbranch[n].key;
			if (!key_in_range(c, key, from_key, to_key)) {
				err = 0;
				goto out_unlock;
			}
		}

		
		if (znode->cnext || !ubifs_zn_dirty(znode)) {
			znode = dirty_cow_bottom_up(c, znode);
			if (IS_ERR(znode)) {
				err = PTR_ERR(znode);
				goto out_unlock;
			}
		}

		
		for (i = n + 1, k = 0; i < znode->child_cnt; i++, k++) {
			key = &znode->zbranch[i].key;
			if (!key_in_range(c, key, from_key, to_key))
				break;
			lnc_free(&znode->zbranch[i]);
			err = ubifs_add_dirt(c, znode->zbranch[i].lnum,
					     znode->zbranch[i].len);
			if (err) {
				dbg_dump_znode(c, znode);
				goto out_unlock;
			}
			dbg_tnck(key, "removing key ");
		}
		if (k) {
			for (i = n + 1 + k; i < znode->child_cnt; i++)
				znode->zbranch[i - k] = znode->zbranch[i];
			znode->child_cnt -= k;
		}

		
		err = tnc_delete(c, znode, n);
		if (err)
			goto out_unlock;
	}

out_unlock:
	if (!err)
		err = dbg_check_tnc(c, 0);
	mutex_unlock(&c->tnc_mutex);
	return err;
}

int ubifs_tnc_remove_ino(struct ubifs_info *c, ino_t inum)
{
	union ubifs_key key1, key2;
	struct ubifs_dent_node *xent, *pxent = NULL;
	struct qstr nm = { .name = NULL };

	dbg_tnc("ino %lu", (unsigned long)inum);

	lowest_xent_key(c, &key1, inum);
	while (1) {
		ino_t xattr_inum;
		int err;

		xent = ubifs_tnc_next_ent(c, &key1, &nm);
		if (IS_ERR(xent)) {
			err = PTR_ERR(xent);
			if (err == -ENOENT)
				break;
			return err;
		}

		xattr_inum = le64_to_cpu(xent->inum);
		dbg_tnc("xent '%s', ino %lu", xent->name,
			(unsigned long)xattr_inum);

		nm.name = xent->name;
		nm.len = le16_to_cpu(xent->nlen);
		err = ubifs_tnc_remove_nm(c, &key1, &nm);
		if (err) {
			kfree(xent);
			return err;
		}

		lowest_ino_key(c, &key1, xattr_inum);
		highest_ino_key(c, &key2, xattr_inum);
		err = ubifs_tnc_remove_range(c, &key1, &key2);
		if (err) {
			kfree(xent);
			return err;
		}

		kfree(pxent);
		pxent = xent;
		key_read(c, &xent->key, &key1);
	}

	kfree(pxent);
	lowest_ino_key(c, &key1, inum);
	highest_ino_key(c, &key2, inum);

	return ubifs_tnc_remove_range(c, &key1, &key2);
}

struct ubifs_dent_node *ubifs_tnc_next_ent(struct ubifs_info *c,
					   union ubifs_key *key,
					   const struct qstr *nm)
{
	int n, err, type = key_type(c, key);
	struct ubifs_znode *znode;
	struct ubifs_dent_node *dent;
	struct ubifs_zbranch *zbr;
	union ubifs_key *dkey;

	dbg_tnck(key, "%s ", nm->name ? (char *)nm->name : "(lowest)");
	ubifs_assert(is_hash_key(c, key));

	mutex_lock(&c->tnc_mutex);
	err = ubifs_lookup_level0(c, key, &znode, &n);
	if (unlikely(err < 0))
		goto out_unlock;

	if (nm->name) {
		if (err) {
			
			err = resolve_collision(c, key, &znode, &n, nm);
			dbg_tnc("rc returned %d, znode %p, n %d",
				err, znode, n);
			if (unlikely(err < 0))
				goto out_unlock;
		}

		
		err = tnc_next(c, &znode, &n);
		if (unlikely(err))
			goto out_unlock;
	} else {
		if (!err) {
			err = tnc_next(c, &znode, &n);
			if (err)
				goto out_unlock;
		}
	}

	zbr = &znode->zbranch[n];
	dent = kmalloc(zbr->len, GFP_NOFS);
	if (unlikely(!dent)) {
		err = -ENOMEM;
		goto out_unlock;
	}

	dkey = &zbr->key;
	if (key_inum(c, dkey) != key_inum(c, key) ||
	    key_type(c, dkey) != type) {
		err = -ENOENT;
		goto out_free;
	}

	err = tnc_read_node_nm(c, zbr, dent);
	if (unlikely(err))
		goto out_free;

	mutex_unlock(&c->tnc_mutex);
	return dent;

out_free:
	kfree(dent);
out_unlock:
	mutex_unlock(&c->tnc_mutex);
	return ERR_PTR(err);
}

static void tnc_destroy_cnext(struct ubifs_info *c)
{
	struct ubifs_znode *cnext;

	if (!c->cnext)
		return;
	ubifs_assert(c->cmt_state == COMMIT_BROKEN);
	cnext = c->cnext;
	do {
		struct ubifs_znode *znode = cnext;

		cnext = cnext->cnext;
		if (ubifs_zn_obsolete(znode))
			kfree(znode);
	} while (cnext && cnext != c->cnext);
}

void ubifs_tnc_close(struct ubifs_info *c)
{
	tnc_destroy_cnext(c);
	if (c->zroot.znode) {
		long n;

		ubifs_destroy_tnc_subtree(c->zroot.znode);
		n = atomic_long_read(&c->clean_zn_cnt);
		atomic_long_sub(n, &ubifs_clean_zn_cnt);
	}
	kfree(c->gap_lebs);
	kfree(c->ilebs);
	destroy_old_idx(c);
}

static struct ubifs_znode *left_znode(struct ubifs_info *c,
				      struct ubifs_znode *znode)
{
	int level = znode->level;

	while (1) {
		int n = znode->iip - 1;

		
		znode = znode->parent;
		if (!znode)
			return NULL;
		if (n >= 0) {
			
			znode = get_znode(c, znode, n);
			if (IS_ERR(znode))
				return znode;
			while (znode->level != level) {
				n = znode->child_cnt - 1;
				znode = get_znode(c, znode, n);
				if (IS_ERR(znode))
					return znode;
			}
			break;
		}
	}
	return znode;
}

static struct ubifs_znode *right_znode(struct ubifs_info *c,
				       struct ubifs_znode *znode)
{
	int level = znode->level;

	while (1) {
		int n = znode->iip + 1;

		
		znode = znode->parent;
		if (!znode)
			return NULL;
		if (n < znode->child_cnt) {
			
			znode = get_znode(c, znode, n);
			if (IS_ERR(znode))
				return znode;
			while (znode->level != level) {
				znode = get_znode(c, znode, 0);
				if (IS_ERR(znode))
					return znode;
			}
			break;
		}
	}
	return znode;
}

static struct ubifs_znode *lookup_znode(struct ubifs_info *c,
					union ubifs_key *key, int level,
					int lnum, int offs)
{
	struct ubifs_znode *znode, *zn;
	int n, nn;

	ubifs_assert(key_type(c, key) < UBIFS_INVALID_KEY);

	if (level < 0)
		return ERR_PTR(-EINVAL);

	
	znode = c->zroot.znode;
	if (!znode) {
		znode = ubifs_load_znode(c, &c->zroot, NULL, 0);
		if (IS_ERR(znode))
			return znode;
	}
	
	if (c->zroot.lnum == lnum && c->zroot.offs == offs)
		return znode;
	
	if (level >= znode->level)
		return NULL;
	while (1) {
		ubifs_search_zbranch(c, znode, key, &n);
		if (n < 0) {
			znode = left_znode(c, znode);
			if (!znode)
				return NULL;
			if (IS_ERR(znode))
				return znode;
			ubifs_search_zbranch(c, znode, key, &n);
			ubifs_assert(n >= 0);
		}
		if (znode->level == level + 1)
			break;
		znode = get_znode(c, znode, n);
		if (IS_ERR(znode))
			return znode;
	}
	
	if (znode->zbranch[n].lnum == lnum && znode->zbranch[n].offs == offs)
		return get_znode(c, znode, n);
	
	if (!is_hash_key(c, key))
		return NULL;
	zn = znode;
	nn = n;
	
	while (1) {
		
		if (n)
			n -= 1;
		else {
			znode = left_znode(c, znode);
			if (!znode)
				break;
			if (IS_ERR(znode))
				return znode;
			n = znode->child_cnt - 1;
		}
		
		if (znode->zbranch[n].lnum == lnum &&
		    znode->zbranch[n].offs == offs)
			return get_znode(c, znode, n);
		
		if (keys_cmp(c, &znode->zbranch[n].key, key) < 0)
			break;
	}
	
	znode = zn;
	n = nn;
	
	while (1) {
		
		if (++n >= znode->child_cnt) {
			znode = right_znode(c, znode);
			if (!znode)
				break;
			if (IS_ERR(znode))
				return znode;
			n = 0;
		}
		
		if (znode->zbranch[n].lnum == lnum &&
		    znode->zbranch[n].offs == offs)
			return get_znode(c, znode, n);
		
		if (keys_cmp(c, &znode->zbranch[n].key, key) > 0)
			break;
	}
	return NULL;
}

int is_idx_node_in_tnc(struct ubifs_info *c, union ubifs_key *key, int level,
		       int lnum, int offs)
{
	struct ubifs_znode *znode;

	znode = lookup_znode(c, key, level, lnum, offs);
	if (!znode)
		return 0;
	if (IS_ERR(znode))
		return PTR_ERR(znode);

	return ubifs_zn_dirty(znode) ? 1 : 2;
}

static int is_leaf_node_in_tnc(struct ubifs_info *c, union ubifs_key *key,
			       int lnum, int offs)
{
	struct ubifs_zbranch *zbr;
	struct ubifs_znode *znode, *zn;
	int n, found, err, nn;
	const int unique = !is_hash_key(c, key);

	found = ubifs_lookup_level0(c, key, &znode, &n);
	if (found < 0)
		return found; 
	if (!found)
		return 0;
	zbr = &znode->zbranch[n];
	if (lnum == zbr->lnum && offs == zbr->offs)
		return 1; 
	if (unique)
		return 0;
	zn = znode;
	nn = n;
	
	while (1) {
		err = tnc_prev(c, &znode, &n);
		if (err == -ENOENT)
			break;
		if (err)
			return err;
		if (keys_cmp(c, key, &znode->zbranch[n].key))
			break;
		zbr = &znode->zbranch[n];
		if (lnum == zbr->lnum && offs == zbr->offs)
			return 1; 
	}
	
	znode = zn;
	n = nn;
	while (1) {
		err = tnc_next(c, &znode, &n);
		if (err) {
			if (err == -ENOENT)
				return 0;
			return err;
		}
		if (keys_cmp(c, key, &znode->zbranch[n].key))
			break;
		zbr = &znode->zbranch[n];
		if (lnum == zbr->lnum && offs == zbr->offs)
			return 1; 
	}
	return 0;
}

int ubifs_tnc_has_node(struct ubifs_info *c, union ubifs_key *key, int level,
		       int lnum, int offs, int is_idx)
{
	int err;

	mutex_lock(&c->tnc_mutex);
	if (is_idx) {
		err = is_idx_node_in_tnc(c, key, level, lnum, offs);
		if (err < 0)
			goto out_unlock;
		if (err == 1)
			
			err = 0;
		else if (err == 2)
			
			err = 1;
		else
			BUG_ON(err != 0);
	} else
		err = is_leaf_node_in_tnc(c, key, lnum, offs);

out_unlock:
	mutex_unlock(&c->tnc_mutex);
	return err;
}

int ubifs_dirty_idx_node(struct ubifs_info *c, union ubifs_key *key, int level,
			 int lnum, int offs)
{
	struct ubifs_znode *znode;
	int err = 0;

	mutex_lock(&c->tnc_mutex);
	znode = lookup_znode(c, key, level, lnum, offs);
	if (!znode)
		goto out_unlock;
	if (IS_ERR(znode)) {
		err = PTR_ERR(znode);
		goto out_unlock;
	}
	znode = dirty_cow_bottom_up(c, znode);
	if (IS_ERR(znode)) {
		err = PTR_ERR(znode);
		goto out_unlock;
	}

out_unlock:
	mutex_unlock(&c->tnc_mutex);
	return err;
}

#ifdef CONFIG_UBIFS_FS_DEBUG

int dbg_check_inode_size(struct ubifs_info *c, const struct inode *inode,
			 loff_t size)
{
	int err, n;
	union ubifs_key from_key, to_key, *key;
	struct ubifs_znode *znode;
	unsigned int block;

	if (!S_ISREG(inode->i_mode))
		return 0;
	if (!dbg_is_chk_gen(c))
		return 0;

	block = (size + UBIFS_BLOCK_SIZE - 1) >> UBIFS_BLOCK_SHIFT;
	data_key_init(c, &from_key, inode->i_ino, block);
	highest_data_key(c, &to_key, inode->i_ino);

	mutex_lock(&c->tnc_mutex);
	err = ubifs_lookup_level0(c, &from_key, &znode, &n);
	if (err < 0)
		goto out_unlock;

	if (err) {
		err = -EINVAL;
		key = &from_key;
		goto out_dump;
	}

	err = tnc_next(c, &znode, &n);
	if (err == -ENOENT) {
		err = 0;
		goto out_unlock;
	}
	if (err < 0)
		goto out_unlock;

	ubifs_assert(err == 0);
	key = &znode->zbranch[n].key;
	if (!key_in_range(c, key, &from_key, &to_key))
		goto out_unlock;

out_dump:
	block = key_block(c, key);
	ubifs_err("inode %lu has size %lld, but there are data at offset %lld",
		  (unsigned long)inode->i_ino, size,
		  ((loff_t)block) << UBIFS_BLOCK_SHIFT);
	mutex_unlock(&c->tnc_mutex);
	dbg_dump_inode(c, inode);
	dbg_dump_stack();
	return -EINVAL;

out_unlock:
	mutex_unlock(&c->tnc_mutex);
	return err;
}

#endif 
