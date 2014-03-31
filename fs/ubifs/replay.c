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
#include <linux/list_sort.h>

struct replay_entry {
	int lnum;
	int offs;
	int len;
	unsigned int deletion:1;
	unsigned long long sqnum;
	struct list_head list;
	union ubifs_key key;
	union {
		struct qstr nm;
		struct {
			loff_t old_size;
			loff_t new_size;
		};
	};
};

struct bud_entry {
	struct list_head list;
	struct ubifs_bud *bud;
	unsigned long long sqnum;
	int free;
	int dirty;
};

static int set_bud_lprops(struct ubifs_info *c, struct bud_entry *b)
{
	const struct ubifs_lprops *lp;
	int err = 0, dirty;

	ubifs_get_lprops(c);

	lp = ubifs_lpt_lookup_dirty(c, b->bud->lnum);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	dirty = lp->dirty;
	if (b->bud->start == 0 && (lp->free != c->leb_size || lp->dirty != 0)) {
		dbg_mnt("bud LEB %d was GC'd (%d free, %d dirty)", b->bud->lnum,
			lp->free, lp->dirty);
		dbg_gc("bud LEB %d was GC'd (%d free, %d dirty)", b->bud->lnum,
			lp->free, lp->dirty);
		dirty -= c->leb_size - lp->free;
		if (dirty != 0)
			dbg_msg("LEB %d lp: %d free %d dirty "
				"replay: %d free %d dirty", b->bud->lnum,
				lp->free, lp->dirty, b->free, b->dirty);
	}
	lp = ubifs_change_lp(c, lp, b->free, dirty + b->dirty,
			     lp->flags | LPROPS_TAKEN, 0);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	
	err = ubifs_wbuf_seek_nolock(&c->jheads[b->bud->jhead].wbuf,
				     b->bud->lnum, c->leb_size - b->free,
				     UBI_SHORTTERM);

out:
	ubifs_release_lprops(c);
	return err;
}

static int set_buds_lprops(struct ubifs_info *c)
{
	struct bud_entry *b;
	int err;

	list_for_each_entry(b, &c->replay_buds, list) {
		err = set_bud_lprops(c, b);
		if (err)
			return err;
	}

	return 0;
}

static int trun_remove_range(struct ubifs_info *c, struct replay_entry *r)
{
	unsigned min_blk, max_blk;
	union ubifs_key min_key, max_key;
	ino_t ino;

	min_blk = r->new_size / UBIFS_BLOCK_SIZE;
	if (r->new_size & (UBIFS_BLOCK_SIZE - 1))
		min_blk += 1;

	max_blk = r->old_size / UBIFS_BLOCK_SIZE;
	if ((r->old_size & (UBIFS_BLOCK_SIZE - 1)) == 0)
		max_blk -= 1;

	ino = key_inum(c, &r->key);

	data_key_init(c, &min_key, ino, min_blk);
	data_key_init(c, &max_key, ino, max_blk);

	return ubifs_tnc_remove_range(c, &min_key, &max_key);
}

static int apply_replay_entry(struct ubifs_info *c, struct replay_entry *r)
{
	int err;

	dbg_mntk(&r->key, "LEB %d:%d len %d deletion %d sqnum %llu key ",
		 r->lnum, r->offs, r->len, r->deletion, r->sqnum);

	
	c->replay_sqnum = r->sqnum;

	if (is_hash_key(c, &r->key)) {
		if (r->deletion)
			err = ubifs_tnc_remove_nm(c, &r->key, &r->nm);
		else
			err = ubifs_tnc_add_nm(c, &r->key, r->lnum, r->offs,
					       r->len, &r->nm);
	} else {
		if (r->deletion)
			switch (key_type(c, &r->key)) {
			case UBIFS_INO_KEY:
			{
				ino_t inum = key_inum(c, &r->key);

				err = ubifs_tnc_remove_ino(c, inum);
				break;
			}
			case UBIFS_TRUN_KEY:
				err = trun_remove_range(c, r);
				break;
			default:
				err = ubifs_tnc_remove(c, &r->key);
				break;
			}
		else
			err = ubifs_tnc_add(c, &r->key, r->lnum, r->offs,
					    r->len);
		if (err)
			return err;

		if (c->need_recovery)
			err = ubifs_recover_size_accum(c, &r->key, r->deletion,
						       r->new_size);
	}

	return err;
}

static int replay_entries_cmp(void *priv, struct list_head *a,
			      struct list_head *b)
{
	struct replay_entry *ra, *rb;

	cond_resched();
	if (a == b)
		return 0;

	ra = list_entry(a, struct replay_entry, list);
	rb = list_entry(b, struct replay_entry, list);
	ubifs_assert(ra->sqnum != rb->sqnum);
	if (ra->sqnum > rb->sqnum)
		return 1;
	return -1;
}

static int apply_replay_list(struct ubifs_info *c)
{
	struct replay_entry *r;
	int err;

	list_sort(c, &c->replay_list, &replay_entries_cmp);

	list_for_each_entry(r, &c->replay_list, list) {
		cond_resched();

		err = apply_replay_entry(c, r);
		if (err)
			return err;
	}

	return 0;
}

static void destroy_replay_list(struct ubifs_info *c)
{
	struct replay_entry *r, *tmp;

	list_for_each_entry_safe(r, tmp, &c->replay_list, list) {
		if (is_hash_key(c, &r->key))
			kfree(r->nm.name);
		list_del(&r->list);
		kfree(r);
	}
}

static int insert_node(struct ubifs_info *c, int lnum, int offs, int len,
		       union ubifs_key *key, unsigned long long sqnum,
		       int deletion, int *used, loff_t old_size,
		       loff_t new_size)
{
	struct replay_entry *r;

	dbg_mntk(key, "add LEB %d:%d, key ", lnum, offs);

	if (key_inum(c, key) >= c->highest_inum)
		c->highest_inum = key_inum(c, key);

	r = kzalloc(sizeof(struct replay_entry), GFP_KERNEL);
	if (!r)
		return -ENOMEM;

	if (!deletion)
		*used += ALIGN(len, 8);
	r->lnum = lnum;
	r->offs = offs;
	r->len = len;
	r->deletion = !!deletion;
	r->sqnum = sqnum;
	key_copy(c, key, &r->key);
	r->old_size = old_size;
	r->new_size = new_size;

	list_add_tail(&r->list, &c->replay_list);
	return 0;
}

static int insert_dent(struct ubifs_info *c, int lnum, int offs, int len,
		       union ubifs_key *key, const char *name, int nlen,
		       unsigned long long sqnum, int deletion, int *used)
{
	struct replay_entry *r;
	char *nbuf;

	dbg_mntk(key, "add LEB %d:%d, key ", lnum, offs);
	if (key_inum(c, key) >= c->highest_inum)
		c->highest_inum = key_inum(c, key);

	r = kzalloc(sizeof(struct replay_entry), GFP_KERNEL);
	if (!r)
		return -ENOMEM;

	nbuf = kmalloc(nlen + 1, GFP_KERNEL);
	if (!nbuf) {
		kfree(r);
		return -ENOMEM;
	}

	if (!deletion)
		*used += ALIGN(len, 8);
	r->lnum = lnum;
	r->offs = offs;
	r->len = len;
	r->deletion = !!deletion;
	r->sqnum = sqnum;
	key_copy(c, key, &r->key);
	r->nm.len = nlen;
	memcpy(nbuf, name, nlen);
	nbuf[nlen] = '\0';
	r->nm.name = nbuf;

	list_add_tail(&r->list, &c->replay_list);
	return 0;
}

int ubifs_validate_entry(struct ubifs_info *c,
			 const struct ubifs_dent_node *dent)
{
	int key_type = key_type_flash(c, dent->key);
	int nlen = le16_to_cpu(dent->nlen);

	if (le32_to_cpu(dent->ch.len) != nlen + UBIFS_DENT_NODE_SZ + 1 ||
	    dent->type >= UBIFS_ITYPES_CNT ||
	    nlen > UBIFS_MAX_NLEN || dent->name[nlen] != 0 ||
	    strnlen(dent->name, nlen) != nlen ||
	    le64_to_cpu(dent->inum) > MAX_INUM) {
		ubifs_err("bad %s node", key_type == UBIFS_DENT_KEY ?
			  "directory entry" : "extended attribute entry");
		return -EINVAL;
	}

	if (key_type != UBIFS_DENT_KEY && key_type != UBIFS_XENT_KEY) {
		ubifs_err("bad key type %d", key_type);
		return -EINVAL;
	}

	return 0;
}

static int is_last_bud(struct ubifs_info *c, struct ubifs_bud *bud)
{
	struct ubifs_jhead *jh = &c->jheads[bud->jhead];
	struct ubifs_bud *next;
	uint32_t data;
	int err;

	if (list_is_last(&bud->list, &jh->buds_list))
		return 1;

	next = list_entry(bud->list.next, struct ubifs_bud, list);
	if (!list_is_last(&next->list, &jh->buds_list))
		return 0;

	err = ubifs_leb_read(c, next->lnum, (char *)&data, next->start, 4, 1);
	if (err)
		return 0;

	return data == 0xFFFFFFFF;
}

static int replay_bud(struct ubifs_info *c, struct bud_entry *b)
{
	int is_last = is_last_bud(c, b->bud);
	int err = 0, used = 0, lnum = b->bud->lnum, offs = b->bud->start;
	struct ubifs_scan_leb *sleb;
	struct ubifs_scan_node *snod;

	dbg_mnt("replay bud LEB %d, head %d, offs %d, is_last %d",
		lnum, b->bud->jhead, offs, is_last);

	if (c->need_recovery && is_last)
		/*
		 * Recover only last LEBs in the journal heads, because power
		 * cuts may cause corruptions only in these LEBs, because only
		 * these LEBs could possibly be written to at the power cut
		 * time.
		 */
		sleb = ubifs_recover_leb(c, lnum, offs, c->sbuf, b->bud->jhead);
	else
		sleb = ubifs_scan(c, lnum, offs, c->sbuf, 0);
	if (IS_ERR(sleb))
		return PTR_ERR(sleb);


	list_for_each_entry(snod, &sleb->nodes, list) {
		int deletion = 0;

		cond_resched();

		if (snod->sqnum >= SQNUM_WATERMARK) {
			ubifs_err("file system's life ended");
			goto out_dump;
		}

		if (snod->sqnum > c->max_sqnum)
			c->max_sqnum = snod->sqnum;

		switch (snod->type) {
		case UBIFS_INO_NODE:
		{
			struct ubifs_ino_node *ino = snod->node;
			loff_t new_size = le64_to_cpu(ino->size);

			if (le32_to_cpu(ino->nlink) == 0)
				deletion = 1;
			err = insert_node(c, lnum, snod->offs, snod->len,
					  &snod->key, snod->sqnum, deletion,
					  &used, 0, new_size);
			break;
		}
		case UBIFS_DATA_NODE:
		{
			struct ubifs_data_node *dn = snod->node;
			loff_t new_size = le32_to_cpu(dn->size) +
					  key_block(c, &snod->key) *
					  UBIFS_BLOCK_SIZE;

			err = insert_node(c, lnum, snod->offs, snod->len,
					  &snod->key, snod->sqnum, deletion,
					  &used, 0, new_size);
			break;
		}
		case UBIFS_DENT_NODE:
		case UBIFS_XENT_NODE:
		{
			struct ubifs_dent_node *dent = snod->node;

			err = ubifs_validate_entry(c, dent);
			if (err)
				goto out_dump;

			err = insert_dent(c, lnum, snod->offs, snod->len,
					  &snod->key, dent->name,
					  le16_to_cpu(dent->nlen), snod->sqnum,
					  !le64_to_cpu(dent->inum), &used);
			break;
		}
		case UBIFS_TRUN_NODE:
		{
			struct ubifs_trun_node *trun = snod->node;
			loff_t old_size = le64_to_cpu(trun->old_size);
			loff_t new_size = le64_to_cpu(trun->new_size);
			union ubifs_key key;

			
			if (old_size < 0 || old_size > c->max_inode_sz ||
			    new_size < 0 || new_size > c->max_inode_sz ||
			    old_size <= new_size) {
				ubifs_err("bad truncation node");
				goto out_dump;
			}

			trun_key_init(c, &key, le32_to_cpu(trun->inum));
			err = insert_node(c, lnum, snod->offs, snod->len,
					  &key, snod->sqnum, 1, &used,
					  old_size, new_size);
			break;
		}
		default:
			ubifs_err("unexpected node type %d in bud LEB %d:%d",
				  snod->type, lnum, snod->offs);
			err = -EINVAL;
			goto out_dump;
		}
		if (err)
			goto out;
	}

	ubifs_assert(ubifs_search_bud(c, lnum));
	ubifs_assert(sleb->endpt - offs >= used);
	ubifs_assert(sleb->endpt % c->min_io_size == 0);

	b->dirty = sleb->endpt - offs - used;
	b->free = c->leb_size - sleb->endpt;
	dbg_mnt("bud LEB %d replied: dirty %d, free %d", lnum, b->dirty, b->free);

out:
	ubifs_scan_destroy(sleb);
	return err;

out_dump:
	ubifs_err("bad node is at LEB %d:%d", lnum, snod->offs);
	dbg_dump_node(c, snod->node);
	ubifs_scan_destroy(sleb);
	return -EINVAL;
}

static int replay_buds(struct ubifs_info *c)
{
	struct bud_entry *b;
	int err;
	unsigned long long prev_sqnum = 0;

	list_for_each_entry(b, &c->replay_buds, list) {
		err = replay_bud(c, b);
		if (err)
			return err;

		ubifs_assert(b->sqnum > prev_sqnum);
		prev_sqnum = b->sqnum;
	}

	return 0;
}

static void destroy_bud_list(struct ubifs_info *c)
{
	struct bud_entry *b;

	while (!list_empty(&c->replay_buds)) {
		b = list_entry(c->replay_buds.next, struct bud_entry, list);
		list_del(&b->list);
		kfree(b);
	}
}

static int add_replay_bud(struct ubifs_info *c, int lnum, int offs, int jhead,
			  unsigned long long sqnum)
{
	struct ubifs_bud *bud;
	struct bud_entry *b;

	dbg_mnt("add replay bud LEB %d:%d, head %d", lnum, offs, jhead);

	bud = kmalloc(sizeof(struct ubifs_bud), GFP_KERNEL);
	if (!bud)
		return -ENOMEM;

	b = kmalloc(sizeof(struct bud_entry), GFP_KERNEL);
	if (!b) {
		kfree(bud);
		return -ENOMEM;
	}

	bud->lnum = lnum;
	bud->start = offs;
	bud->jhead = jhead;
	ubifs_add_bud(c, bud);

	b->bud = bud;
	b->sqnum = sqnum;
	list_add_tail(&b->list, &c->replay_buds);

	return 0;
}

static int validate_ref(struct ubifs_info *c, const struct ubifs_ref_node *ref)
{
	struct ubifs_bud *bud;
	int lnum = le32_to_cpu(ref->lnum);
	unsigned int offs = le32_to_cpu(ref->offs);
	unsigned int jhead = le32_to_cpu(ref->jhead);

	if (jhead >= c->jhead_cnt || lnum >= c->leb_cnt ||
	    lnum < c->main_first || offs > c->leb_size ||
	    offs & (c->min_io_size - 1))
		return -EINVAL;

	
	bud = ubifs_search_bud(c, lnum);
	if (bud) {
		if (bud->jhead == jhead && bud->start <= offs)
			return 1;
		ubifs_err("bud at LEB %d:%d was already referred", lnum, offs);
		return -EINVAL;
	}

	return 0;
}

static int replay_log_leb(struct ubifs_info *c, int lnum, int offs, void *sbuf)
{
	int err;
	struct ubifs_scan_leb *sleb;
	struct ubifs_scan_node *snod;
	const struct ubifs_cs_node *node;

	dbg_mnt("replay log LEB %d:%d", lnum, offs);
	sleb = ubifs_scan(c, lnum, offs, sbuf, c->need_recovery);
	if (IS_ERR(sleb)) {
		if (PTR_ERR(sleb) != -EUCLEAN || !c->need_recovery)
			return PTR_ERR(sleb);
		sleb = ubifs_recover_log_leb(c, lnum, offs, sbuf);
		if (IS_ERR(sleb))
			return PTR_ERR(sleb);
	}

	if (sleb->nodes_cnt == 0) {
		err = 1;
		goto out;
	}

	node = sleb->buf;
	snod = list_entry(sleb->nodes.next, struct ubifs_scan_node, list);
	if (c->cs_sqnum == 0) {
		if (snod->type != UBIFS_CS_NODE) {
			dbg_err("first log node at LEB %d:%d is not CS node",
				lnum, offs);
			goto out_dump;
		}
		if (le64_to_cpu(node->cmt_no) != c->cmt_no) {
			dbg_err("first CS node at LEB %d:%d has wrong "
				"commit number %llu expected %llu",
				lnum, offs,
				(unsigned long long)le64_to_cpu(node->cmt_no),
				c->cmt_no);
			goto out_dump;
		}

		c->cs_sqnum = le64_to_cpu(node->ch.sqnum);
		dbg_mnt("commit start sqnum %llu", c->cs_sqnum);
	}

	if (snod->sqnum < c->cs_sqnum) {
		err = 1;
		goto out;
	}

	
	if (snod->offs != 0) {
		dbg_err("first node is not at zero offset");
		goto out_dump;
	}

	list_for_each_entry(snod, &sleb->nodes, list) {
		cond_resched();

		if (snod->sqnum >= SQNUM_WATERMARK) {
			ubifs_err("file system's life ended");
			goto out_dump;
		}

		if (snod->sqnum < c->cs_sqnum) {
			dbg_err("bad sqnum %llu, commit sqnum %llu",
				snod->sqnum, c->cs_sqnum);
			goto out_dump;
		}

		if (snod->sqnum > c->max_sqnum)
			c->max_sqnum = snod->sqnum;

		switch (snod->type) {
		case UBIFS_REF_NODE: {
			const struct ubifs_ref_node *ref = snod->node;

			err = validate_ref(c, ref);
			if (err == 1)
				break; 
			if (err)
				goto out_dump;

			err = add_replay_bud(c, le32_to_cpu(ref->lnum),
					     le32_to_cpu(ref->offs),
					     le32_to_cpu(ref->jhead),
					     snod->sqnum);
			if (err)
				goto out;

			break;
		}
		case UBIFS_CS_NODE:
			
			if (snod->offs != 0) {
				ubifs_err("unexpected node in log");
				goto out_dump;
			}
			break;
		default:
			ubifs_err("unexpected node in log");
			goto out_dump;
		}
	}

	if (sleb->endpt || c->lhead_offs >= c->leb_size) {
		c->lhead_lnum = lnum;
		c->lhead_offs = sleb->endpt;
	}

	err = !sleb->endpt;
out:
	ubifs_scan_destroy(sleb);
	return err;

out_dump:
	ubifs_err("log error detected while replaying the log at LEB %d:%d",
		  lnum, offs + snod->offs);
	dbg_dump_node(c, snod->node);
	ubifs_scan_destroy(sleb);
	return -EINVAL;
}

static int take_ihead(struct ubifs_info *c)
{
	const struct ubifs_lprops *lp;
	int err, free;

	ubifs_get_lprops(c);

	lp = ubifs_lpt_lookup_dirty(c, c->ihead_lnum);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	free = lp->free;

	lp = ubifs_change_lp(c, lp, LPROPS_NC, LPROPS_NC,
			     lp->flags | LPROPS_TAKEN, 0);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	err = free;
out:
	ubifs_release_lprops(c);
	return err;
}

int ubifs_replay_journal(struct ubifs_info *c)
{
	int err, i, lnum, offs, free;

	BUILD_BUG_ON(UBIFS_TRUN_KEY > 5);

	
	free = take_ihead(c);
	if (free < 0)
		return free; 

	if (c->ihead_offs != c->leb_size - free) {
		ubifs_err("bad index head LEB %d:%d", c->ihead_lnum,
			  c->ihead_offs);
		return -EINVAL;
	}

	dbg_mnt("start replaying the journal");
	c->replaying = 1;
	lnum = c->ltail_lnum = c->lhead_lnum;
	offs = c->lhead_offs;

	for (i = 0; i < c->log_lebs; i++, lnum++) {
		if (lnum >= UBIFS_LOG_LNUM + c->log_lebs) {
			lnum = UBIFS_LOG_LNUM;
			offs = 0;
		}
		err = replay_log_leb(c, lnum, offs, c->sbuf);
		if (err == 1)
			
			break;
		if (err)
			goto out;
		offs = 0;
	}

	err = replay_buds(c);
	if (err)
		goto out;

	err = apply_replay_list(c);
	if (err)
		goto out;

	err = set_buds_lprops(c);
	if (err)
		goto out;

	c->bi.uncommitted_idx = atomic_long_read(&c->dirty_zn_cnt);
	c->bi.uncommitted_idx *= c->max_idx_node_sz;

	ubifs_assert(c->bud_bytes <= c->max_bud_bytes || c->need_recovery);
	dbg_mnt("finished, log head LEB %d:%d, max_sqnum %llu, "
		"highest_inum %lu", c->lhead_lnum, c->lhead_offs, c->max_sqnum,
		(unsigned long)c->highest_inum);
out:
	destroy_replay_list(c);
	destroy_bud_list(c);
	c->replaying = 0;
	return err;
}
