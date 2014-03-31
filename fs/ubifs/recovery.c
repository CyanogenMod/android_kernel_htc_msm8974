/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation
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

static int is_empty(void *buf, int len)
{
	uint8_t *p = buf;
	int i;

	for (i = 0; i < len; i++)
		if (*p++ != 0xff)
			return 0;
	return 1;
}

static int first_non_ff(void *buf, int len)
{
	uint8_t *p = buf;
	int i;

	for (i = 0; i < len; i++)
		if (*p++ != 0xff)
			return i;
	return -1;
}

/**
 * get_master_node - get the last valid master node allowing for corruption.
 * @c: UBIFS file-system description object
 * @lnum: LEB number
 * @pbuf: buffer containing the LEB read, is returned here
 * @mst: master node, if found, is returned here
 * @cor: corruption, if found, is returned here
 *
 * This function allocates a buffer, reads the LEB into it, and finds and
 * returns the last valid master node allowing for one area of corruption.
 * The corrupt area, if there is one, must be consistent with the assumption
 * that it is the result of an unclean unmount while the master node was being
 * written. Under those circumstances, it is valid to use the previously written
 * master node.
 *
 * This function returns %0 on success and a negative error code on failure.
 */
static int get_master_node(const struct ubifs_info *c, int lnum, void **pbuf,
			   struct ubifs_mst_node **mst, void **cor)
{
	const int sz = c->mst_node_alsz;
	int err, offs, len;
	void *sbuf, *buf;

	sbuf = vmalloc(c->leb_size);
	if (!sbuf)
		return -ENOMEM;

	err = ubifs_leb_read(c, lnum, sbuf, 0, c->leb_size, 0);
	if (err && err != -EBADMSG)
		goto out_free;

	
	offs = 0;
	buf = sbuf;
	len = c->leb_size;
	while (offs + UBIFS_MST_NODE_SZ <= c->leb_size) {
		struct ubifs_ch *ch = buf;

		if (le32_to_cpu(ch->magic) != UBIFS_NODE_MAGIC)
			break;
		offs += sz;
		buf  += sz;
		len  -= sz;
	}
	
	if (offs) {
		int ret;

		offs -= sz;
		buf  -= sz;
		len  += sz;
		ret = ubifs_scan_a_node(c, buf, len, lnum, offs, 1);
		if (ret != SCANNED_A_NODE && offs) {
			
			offs -= sz;
			buf  -= sz;
			len  += sz;
			ret = ubifs_scan_a_node(c, buf, len, lnum, offs, 1);
			if (ret != SCANNED_A_NODE)
				goto out_err;
		}
		if (ret == SCANNED_A_NODE) {
			struct ubifs_ch *ch = buf;

			if (ch->node_type != UBIFS_MST_NODE)
				goto out_err;
			dbg_rcvry("found a master node at %d:%d", lnum, offs);
			*mst = buf;
			offs += sz;
			buf  += sz;
			len  -= sz;
		}
	}
	
	if (offs < c->leb_size) {
		if (!is_empty(buf, min_t(int, len, sz))) {
			*cor = buf;
			dbg_rcvry("found corruption at %d:%d", lnum, offs);
		}
		offs += sz;
		buf  += sz;
		len  -= sz;
	}
	
	if (offs < c->leb_size)
		if (!is_empty(buf, len))
			goto out_err;
	*pbuf = sbuf;
	return 0;

out_err:
	err = -EINVAL;
out_free:
	vfree(sbuf);
	*mst = NULL;
	*cor = NULL;
	return err;
}

static int write_rcvrd_mst_node(struct ubifs_info *c,
				struct ubifs_mst_node *mst)
{
	int err = 0, lnum = UBIFS_MST_LNUM, sz = c->mst_node_alsz;
	__le32 save_flags;

	dbg_rcvry("recovery");

	save_flags = mst->flags;
	mst->flags |= cpu_to_le32(UBIFS_MST_RCVRY);

	ubifs_prepare_node(c, mst, UBIFS_MST_NODE_SZ, 1);
	err = ubifs_leb_change(c, lnum, mst, sz, UBI_SHORTTERM);
	if (err)
		goto out;
	err = ubifs_leb_change(c, lnum + 1, mst, sz, UBI_SHORTTERM);
	if (err)
		goto out;
out:
	mst->flags = save_flags;
	return err;
}

int ubifs_recover_master_node(struct ubifs_info *c)
{
	void *buf1 = NULL, *buf2 = NULL, *cor1 = NULL, *cor2 = NULL;
	struct ubifs_mst_node *mst1 = NULL, *mst2 = NULL, *mst;
	const int sz = c->mst_node_alsz;
	int err, offs1, offs2;

	dbg_rcvry("recovery");

	err = get_master_node(c, UBIFS_MST_LNUM, &buf1, &mst1, &cor1);
	if (err)
		goto out_free;

	err = get_master_node(c, UBIFS_MST_LNUM + 1, &buf2, &mst2, &cor2);
	if (err)
		goto out_free;

	if (mst1) {
		offs1 = (void *)mst1 - buf1;
		if ((le32_to_cpu(mst1->flags) & UBIFS_MST_RCVRY) &&
		    (offs1 == 0 && !cor1)) {
			/*
			 * mst1 was written by recovery at offset 0 with no
			 * corruption.
			 */
			dbg_rcvry("recovery recovery");
			mst = mst1;
		} else if (mst2) {
			offs2 = (void *)mst2 - buf2;
			if (offs1 == offs2) {
				
				if (memcmp((void *)mst1 + UBIFS_CH_SZ,
					   (void *)mst2 + UBIFS_CH_SZ,
					   UBIFS_MST_NODE_SZ - UBIFS_CH_SZ))
					goto out_err;
				mst = mst1;
			} else if (offs2 + sz == offs1) {
				/* 1st LEB was written, 2nd was not */
				if (cor1)
					goto out_err;
				mst = mst1;
			} else if (offs1 == 0 &&
				   c->leb_size - offs2 - sz < sz) {
				/* 1st LEB was unmapped and written, 2nd not */
				if (cor1)
					goto out_err;
				mst = mst1;
			} else
				goto out_err;
		} else {
			/*
			 * 2nd LEB was unmapped and about to be written, so
			 * there must be only one master node in the first LEB
			 * and no corruption.
			 */
			if (offs1 != 0 || cor1)
				goto out_err;
			mst = mst1;
		}
	} else {
		if (!mst2)
			goto out_err;
		/*
		 * 1st LEB was unmapped and about to be written, so there must
		 * be no room left in 2nd LEB.
		 */
		offs2 = (void *)mst2 - buf2;
		if (offs2 + sz + sz <= c->leb_size)
			goto out_err;
		mst = mst2;
	}

	ubifs_msg("recovered master node from LEB %d",
		  (mst == mst1 ? UBIFS_MST_LNUM : UBIFS_MST_LNUM + 1));

	memcpy(c->mst_node, mst, UBIFS_MST_NODE_SZ);

	if (c->ro_mount) {
		
		c->rcvrd_mst_node = kmalloc(sz, GFP_KERNEL);
		if (!c->rcvrd_mst_node) {
			err = -ENOMEM;
			goto out_free;
		}
		memcpy(c->rcvrd_mst_node, c->mst_node, UBIFS_MST_NODE_SZ);

		c->mst_node->flags |= cpu_to_le32(UBIFS_MST_DIRTY);
	} else {
		
		c->max_sqnum = le64_to_cpu(mst->ch.sqnum) - 1;
		err = write_rcvrd_mst_node(c, c->mst_node);
		if (err)
			goto out_free;
	}

	vfree(buf2);
	vfree(buf1);

	return 0;

out_err:
	err = -EINVAL;
out_free:
	ubifs_err("failed to recover master node");
	if (mst1) {
		dbg_err("dumping first master node");
		dbg_dump_node(c, mst1);
	}
	if (mst2) {
		dbg_err("dumping second master node");
		dbg_dump_node(c, mst2);
	}
	vfree(buf2);
	vfree(buf1);
	return err;
}

/**
 * ubifs_write_rcvrd_mst_node - write the recovered master node.
 * @c: UBIFS file-system description object
 *
 * This function writes the master node that was recovered during mounting in
 * read-only mode and must now be written because we are remounting rw.
 *
 * This function returns %0 on success and a negative error code on failure.
 */
int ubifs_write_rcvrd_mst_node(struct ubifs_info *c)
{
	int err;

	if (!c->rcvrd_mst_node)
		return 0;
	c->rcvrd_mst_node->flags |= cpu_to_le32(UBIFS_MST_DIRTY);
	c->mst_node->flags |= cpu_to_le32(UBIFS_MST_DIRTY);
	err = write_rcvrd_mst_node(c, c->rcvrd_mst_node);
	if (err)
		return err;
	kfree(c->rcvrd_mst_node);
	c->rcvrd_mst_node = NULL;
	return 0;
}

static int is_last_write(const struct ubifs_info *c, void *buf, int offs)
{
	int empty_offs, check_len;
	uint8_t *p;

	/*
	 * Round up to the next @c->max_write_size boundary i.e. @offs is in
	 * the last wbuf written. After that should be empty space.
	 */
	empty_offs = ALIGN(offs + 1, c->max_write_size);
	check_len = c->leb_size - empty_offs;
	p = buf + empty_offs - offs;
	return is_empty(p, check_len);
}

static void clean_buf(const struct ubifs_info *c, void **buf, int lnum,
		      int *offs, int *len)
{
	int empty_offs, pad_len;

	lnum = lnum;
	dbg_rcvry("cleaning corruption at %d:%d", lnum, *offs);

	ubifs_assert(!(*offs & 7));
	empty_offs = ALIGN(*offs, c->min_io_size);
	pad_len = empty_offs - *offs;
	ubifs_pad(c, *buf, pad_len);
	*offs += pad_len;
	*buf += pad_len;
	*len -= pad_len;
	memset(*buf, 0xff, c->leb_size - empty_offs);
}

/**
 * no_more_nodes - determine if there are no more nodes in a buffer.
 * @c: UBIFS file-system description object
 * @buf: buffer to check
 * @len: length of buffer
 * @lnum: LEB number of the LEB from which @buf was read
 * @offs: offset from which @buf was read
 *
 * This function ensures that the corrupted node at @offs is the last thing
 * written to a LEB. This function returns %1 if more data is not found and
 * %0 if more data is found.
 */
static int no_more_nodes(const struct ubifs_info *c, void *buf, int len,
			int lnum, int offs)
{
	struct ubifs_ch *ch = buf;
	int skip, dlen = le32_to_cpu(ch->len);

	
	skip = ALIGN(offs + UBIFS_CH_SZ, c->max_write_size) - offs;
	if (is_empty(buf + skip, len - skip))
		return 1;
	if (ubifs_check_node(c, buf, lnum, offs, 1, 0) != -EUCLEAN) {
		dbg_rcvry("unexpected bad common header at %d:%d", lnum, offs);
		return 0;
	}
	
	skip = ALIGN(offs + dlen, c->max_write_size) - offs;
	
	if (is_empty(buf + skip, len - skip))
		return 1;
	dbg_rcvry("unexpected data at %d:%d", lnum, offs + skip);
	return 0;
}

static int fix_unclean_leb(struct ubifs_info *c, struct ubifs_scan_leb *sleb,
			   int start)
{
	int lnum = sleb->lnum, endpt = start;

	
	if (!list_empty(&sleb->nodes)) {
		struct ubifs_scan_node *snod;

		snod = list_entry(sleb->nodes.prev,
				  struct ubifs_scan_node, list);
		endpt = snod->offs + snod->len;
	}

	if (c->ro_mount && !c->remounting_rw) {
		
		struct ubifs_unclean_leb *ucleb;

		dbg_rcvry("need to fix LEB %d start %d endpt %d",
			  lnum, start, sleb->endpt);
		ucleb = kzalloc(sizeof(struct ubifs_unclean_leb), GFP_NOFS);
		if (!ucleb)
			return -ENOMEM;
		ucleb->lnum = lnum;
		ucleb->endpt = endpt;
		list_add_tail(&ucleb->list, &c->unclean_leb_list);
	} else {
		
		int err;

		dbg_rcvry("fixing LEB %d start %d endpt %d",
			  lnum, start, sleb->endpt);
		if (endpt == 0) {
			err = ubifs_leb_unmap(c, lnum);
			if (err)
				return err;
		} else {
			int len = ALIGN(endpt, c->min_io_size);

			if (start) {
				err = ubifs_leb_read(c, lnum, sleb->buf, 0,
						     start, 1);
				if (err)
					return err;
			}
			
			if (len > endpt) {
				int pad_len = len - ALIGN(endpt, 8);

				if (pad_len > 0) {
					void *buf = sleb->buf + len - pad_len;

					ubifs_pad(c, buf, pad_len);
				}
			}
			err = ubifs_leb_change(c, lnum, sleb->buf, len,
					       UBI_UNKNOWN);
			if (err)
				return err;
		}
	}
	return 0;
}

static void drop_last_group(struct ubifs_scan_leb *sleb, int *offs)
{
	while (!list_empty(&sleb->nodes)) {
		struct ubifs_scan_node *snod;
		struct ubifs_ch *ch;

		snod = list_entry(sleb->nodes.prev, struct ubifs_scan_node,
				  list);
		ch = snod->node;
		if (ch->group_type != UBIFS_IN_NODE_GROUP)
			break;

		dbg_rcvry("dropping grouped node at %d:%d",
			  sleb->lnum, snod->offs);
		*offs = snod->offs;
		list_del(&snod->list);
		kfree(snod);
		sleb->nodes_cnt -= 1;
	}
}

static void drop_last_node(struct ubifs_scan_leb *sleb, int *offs)
{
	struct ubifs_scan_node *snod;

	if (!list_empty(&sleb->nodes)) {
		snod = list_entry(sleb->nodes.prev, struct ubifs_scan_node,
				  list);

		dbg_rcvry("dropping last node at %d:%d", sleb->lnum, snod->offs);
		*offs = snod->offs;
		list_del(&snod->list);
		kfree(snod);
		sleb->nodes_cnt -= 1;
	}
}

struct ubifs_scan_leb *ubifs_recover_leb(struct ubifs_info *c, int lnum,
					 int offs, void *sbuf, int jhead)
{
	int ret = 0, err, len = c->leb_size - offs, start = offs, min_io_unit;
	int grouped = jhead == -1 ? 0 : c->jheads[jhead].grouped;
	struct ubifs_scan_leb *sleb;
	void *buf = sbuf + offs;

	dbg_rcvry("%d:%d, jhead %d, grouped %d", lnum, offs, jhead, grouped);

	sleb = ubifs_start_scan(c, lnum, offs, sbuf);
	if (IS_ERR(sleb))
		return sleb;

	ubifs_assert(len >= 8);
	while (len >= 8) {
		dbg_scan("look at LEB %d:%d (%d bytes left)",
			 lnum, offs, len);

		cond_resched();

		ret = ubifs_scan_a_node(c, buf, len, lnum, offs, 1);
		if (ret == SCANNED_A_NODE) {
			
			struct ubifs_ch *ch = buf;
			int node_len;

			err = ubifs_add_snod(c, sleb, buf, offs);
			if (err)
				goto error;
			node_len = ALIGN(le32_to_cpu(ch->len), 8);
			offs += node_len;
			buf += node_len;
			len -= node_len;
		} else if (ret > 0) {
			
			offs += ret;
			buf += ret;
			len -= ret;
		} else if (ret == SCANNED_EMPTY_SPACE ||
			   ret == SCANNED_GARBAGE     ||
			   ret == SCANNED_A_BAD_PAD_NODE ||
			   ret == SCANNED_A_CORRUPT_NODE) {
			dbg_rcvry("found corruption (%d) at %d:%d",
				  ret, lnum, offs);
			break;
		} else {
			dbg_err("unexpected return value %d", ret);
			err = -EINVAL;
			goto error;
		}
	}

	if (ret == SCANNED_GARBAGE || ret == SCANNED_A_BAD_PAD_NODE) {
		if (!is_last_write(c, buf, offs))
			goto corrupted_rescan;
	} else if (ret == SCANNED_A_CORRUPT_NODE) {
		if (!no_more_nodes(c, buf, len, lnum, offs))
			goto corrupted_rescan;
	} else if (!is_empty(buf, len)) {
		if (!is_last_write(c, buf, offs)) {
			int corruption = first_non_ff(buf, len);

			ubifs_err("corrupt empty space LEB %d:%d, corruption "
				  "starts at %d", lnum, offs, corruption);
			
			offs += corruption;
			buf += corruption;
			goto corrupted;
		}
	}

	min_io_unit = round_down(offs, c->min_io_size);
	if (grouped)
		drop_last_group(sleb, &offs);

	if (jhead == GCHD) {
		/*
		 * If this LEB belongs to the GC head then while we are in the
		 * middle of the same min. I/O unit keep dropping nodes. So
		 * basically, what we want is to make sure that the last min.
		 * I/O unit where we saw the corruption is dropped completely
		 * with all the uncorrupted nodes which may possibly sit there.
		 *
		 * In other words, let's name the min. I/O unit where the
		 * corruption starts B, and the previous min. I/O unit A. The
		 * below code tries to deal with a situation when half of B
		 * contains valid nodes or the end of a valid node, and the
		 * second half of B contains corrupted data or garbage. This
		 * means that UBIFS had been writing to B just before the power
		 * cut happened. I do not know how realistic is this scenario
		 * that half of the min. I/O unit had been written successfully
		 * and the other half not, but this is possible in our 'failure
		 * mode emulation' infrastructure at least.
		 *
		 * So what is the problem, why we need to drop those nodes? Why
		 * can't we just clean-up the second half of B by putting a
		 * padding node there? We can, and this works fine with one
		 * exception which was reproduced with power cut emulation
		 * testing and happens extremely rarely.
		 *
		 * Imagine the file-system is full, we run GC which starts
		 * moving valid nodes from LEB X to LEB Y (obviously, LEB Y is
		 * the current GC head LEB). The @c->gc_lnum is -1, which means
		 * that GC will retain LEB X and will try to continue. Imagine
		 * that LEB X is currently the dirtiest LEB, and the amount of
		 * used space in LEB Y is exactly the same as amount of free
		 * space in LEB X.
		 *
		 * And a power cut happens when nodes are moved from LEB X to
		 * LEB Y. We are here trying to recover LEB Y which is the GC
		 * head LEB. We find the min. I/O unit B as described above.
		 * Then we clean-up LEB Y by padding min. I/O unit. And later
		 * 'ubifs_rcvry_gc_commit()' function fails, because it cannot
		 * find a dirty LEB which could be GC'd into LEB Y! Even LEB X
		 * does not match because the amount of valid nodes there does
		 * not fit the free space in LEB Y any more! And this is
		 * because of the padding node which we added to LEB Y. The
		 * user-visible effect of this which I once observed and
		 * analysed is that we cannot mount the file-system with
		 * -ENOSPC error.
		 *
		 * So obviously, to make sure that situation does not happen we
		 * should free min. I/O unit B in LEB Y completely and the last
		 * used min. I/O unit in LEB Y should be A. This is basically
		 * what the below code tries to do.
		 */
		while (offs > min_io_unit)
			drop_last_node(sleb, &offs);
	}

	buf = sbuf + offs;
	len = c->leb_size - offs;

	clean_buf(c, &buf, lnum, &offs, &len);
	ubifs_end_scan(c, sleb, lnum, offs);

	err = fix_unclean_leb(c, sleb, start);
	if (err)
		goto error;

	return sleb;

corrupted_rescan:
	
	dbg_err("corruptio %d", ret);
	ubifs_scan_a_node(c, buf, len, lnum, offs, 1);
corrupted:
	ubifs_scanned_corruption(c, lnum, offs, buf);
	err = -EUCLEAN;
error:
	ubifs_err("LEB %d scanning failed", lnum);
	ubifs_scan_destroy(sleb);
	return ERR_PTR(err);
}

static int get_cs_sqnum(struct ubifs_info *c, int lnum, int offs,
			unsigned long long *cs_sqnum)
{
	struct ubifs_cs_node *cs_node = NULL;
	int err, ret;

	dbg_rcvry("at %d:%d", lnum, offs);
	cs_node = kmalloc(UBIFS_CS_NODE_SZ, GFP_KERNEL);
	if (!cs_node)
		return -ENOMEM;
	if (c->leb_size - offs < UBIFS_CS_NODE_SZ)
		goto out_err;
	err = ubifs_leb_read(c, lnum, (void *)cs_node, offs,
			     UBIFS_CS_NODE_SZ, 0);
	if (err && err != -EBADMSG)
		goto out_free;
	ret = ubifs_scan_a_node(c, cs_node, UBIFS_CS_NODE_SZ, lnum, offs, 0);
	if (ret != SCANNED_A_NODE) {
		dbg_err("Not a valid node");
		goto out_err;
	}
	if (cs_node->ch.node_type != UBIFS_CS_NODE) {
		dbg_err("Node a CS node, type is %d", cs_node->ch.node_type);
		goto out_err;
	}
	if (le64_to_cpu(cs_node->cmt_no) != c->cmt_no) {
		dbg_err("CS node cmt_no %llu != current cmt_no %llu",
			(unsigned long long)le64_to_cpu(cs_node->cmt_no),
			c->cmt_no);
		goto out_err;
	}
	*cs_sqnum = le64_to_cpu(cs_node->ch.sqnum);
	dbg_rcvry("commit start sqnum %llu", *cs_sqnum);
	kfree(cs_node);
	return 0;

out_err:
	err = -EINVAL;
out_free:
	ubifs_err("failed to get CS sqnum");
	kfree(cs_node);
	return err;
}

struct ubifs_scan_leb *ubifs_recover_log_leb(struct ubifs_info *c, int lnum,
					     int offs, void *sbuf)
{
	struct ubifs_scan_leb *sleb;
	int next_lnum;

	dbg_rcvry("LEB %d", lnum);
	next_lnum = lnum + 1;
	if (next_lnum >= UBIFS_LOG_LNUM + c->log_lebs)
		next_lnum = UBIFS_LOG_LNUM;
	if (next_lnum != c->ltail_lnum) {
		sleb = ubifs_scan(c, next_lnum, 0, sbuf, 0);
		if (IS_ERR(sleb))
			return sleb;
		if (sleb->nodes_cnt) {
			struct ubifs_scan_node *snod;
			unsigned long long cs_sqnum = c->cs_sqnum;

			snod = list_entry(sleb->nodes.next,
					  struct ubifs_scan_node, list);
			if (cs_sqnum == 0) {
				int err;

				err = get_cs_sqnum(c, lnum, offs, &cs_sqnum);
				if (err) {
					ubifs_scan_destroy(sleb);
					return ERR_PTR(err);
				}
			}
			if (snod->sqnum > cs_sqnum) {
				ubifs_err("unrecoverable log corruption "
					  "in LEB %d", lnum);
				ubifs_scan_destroy(sleb);
				return ERR_PTR(-EUCLEAN);
			}
		}
		ubifs_scan_destroy(sleb);
	}
	return ubifs_recover_leb(c, lnum, offs, sbuf, -1);
}

static int recover_head(struct ubifs_info *c, int lnum, int offs, void *sbuf)
{
	int len = c->max_write_size, err;

	if (offs + len > c->leb_size)
		len = c->leb_size - offs;

	if (!len)
		return 0;

	
	err = ubifs_leb_read(c, lnum, sbuf, offs, len, 1);
	if (err || !is_empty(sbuf, len)) {
		dbg_rcvry("cleaning head at %d:%d", lnum, offs);
		if (offs == 0)
			return ubifs_leb_unmap(c, lnum);
		err = ubifs_leb_read(c, lnum, sbuf, 0, offs, 1);
		if (err)
			return err;
		return ubifs_leb_change(c, lnum, sbuf, offs, UBI_UNKNOWN);
	}

	return 0;
}

int ubifs_recover_inl_heads(struct ubifs_info *c, void *sbuf)
{
	int err;

	ubifs_assert(!c->ro_mount || c->remounting_rw);

	dbg_rcvry("checking index head at %d:%d", c->ihead_lnum, c->ihead_offs);
	err = recover_head(c, c->ihead_lnum, c->ihead_offs, sbuf);
	if (err)
		return err;

	dbg_rcvry("checking LPT head at %d:%d", c->nhead_lnum, c->nhead_offs);
	err = recover_head(c, c->nhead_lnum, c->nhead_offs, sbuf);
	if (err)
		return err;

	return 0;
}

static int clean_an_unclean_leb(struct ubifs_info *c,
				struct ubifs_unclean_leb *ucleb, void *sbuf)
{
	int err, lnum = ucleb->lnum, offs = 0, len = ucleb->endpt, quiet = 1;
	void *buf = sbuf;

	dbg_rcvry("LEB %d len %d", lnum, len);

	if (len == 0) {
		
		err = ubifs_leb_unmap(c, lnum);
		if (err)
			return err;
		return 0;
	}

	err = ubifs_leb_read(c, lnum, buf, offs, len, 0);
	if (err && err != -EBADMSG)
		return err;

	while (len >= 8) {
		int ret;

		cond_resched();

		
		ret = ubifs_scan_a_node(c, buf, len, lnum, offs, quiet);

		if (ret == SCANNED_A_NODE) {
			
			struct ubifs_ch *ch = buf;
			int node_len;

			node_len = ALIGN(le32_to_cpu(ch->len), 8);
			offs += node_len;
			buf += node_len;
			len -= node_len;
			continue;
		}

		if (ret > 0) {
			
			offs += ret;
			buf += ret;
			len -= ret;
			continue;
		}

		if (ret == SCANNED_EMPTY_SPACE) {
			ubifs_err("unexpected empty space at %d:%d",
				  lnum, offs);
			return -EUCLEAN;
		}

		if (quiet) {
			
			quiet = 0;
			continue;
		}

		ubifs_scanned_corruption(c, lnum, offs, buf);
		return -EUCLEAN;
	}

	
	len = ALIGN(ucleb->endpt, c->min_io_size);
	if (len > ucleb->endpt) {
		int pad_len = len - ALIGN(ucleb->endpt, 8);

		if (pad_len > 0) {
			buf = c->sbuf + len - pad_len;
			ubifs_pad(c, buf, pad_len);
		}
	}

	
	err = ubifs_leb_change(c, lnum, sbuf, len, UBI_UNKNOWN);
	if (err)
		return err;

	dbg_rcvry("cleaned LEB %d", lnum);

	return 0;
}

/**
 * ubifs_clean_lebs - clean LEBs recovered during read-only mount.
 * @c: UBIFS file-system description object
 * @sbuf: LEB-sized buffer to use
 *
 * This function cleans a LEB identified during recovery that needs to be
 * written but was not because UBIFS was mounted read-only. This happens when
 * remounting to read-write mode.
 *
 * This function returns %0 on success and a negative error code on failure.
 */
int ubifs_clean_lebs(struct ubifs_info *c, void *sbuf)
{
	dbg_rcvry("recovery");
	while (!list_empty(&c->unclean_leb_list)) {
		struct ubifs_unclean_leb *ucleb;
		int err;

		ucleb = list_entry(c->unclean_leb_list.next,
				   struct ubifs_unclean_leb, list);
		err = clean_an_unclean_leb(c, ucleb, sbuf);
		if (err)
			return err;
		list_del(&ucleb->list);
		kfree(ucleb);
	}
	return 0;
}

static int grab_empty_leb(struct ubifs_info *c)
{
	int lnum, err;

	lnum = ubifs_find_free_leb_for_idx(c);
	if (lnum < 0) {
		dbg_err("could not find an empty LEB");
		dbg_dump_lprops(c);
		dbg_dump_budg(c, &c->bi);
		return lnum;
	}

	
	err = ubifs_change_one_lp(c, lnum, LPROPS_NC, LPROPS_NC, 0,
				  LPROPS_INDEX, 0);
	if (err)
		return err;

	c->gc_lnum = lnum;
	dbg_rcvry("found empty LEB %d, run commit", lnum);

	return ubifs_run_commit(c);
}

/**
 * ubifs_rcvry_gc_commit - recover the GC LEB number and run the commit.
 * @c: UBIFS file-system description object
 *
 * Out-of-place garbage collection requires always one empty LEB with which to
 * start garbage collection. The LEB number is recorded in c->gc_lnum and is
 * written to the master node on unmounting. In the case of an unclean unmount
 * the value of gc_lnum recorded in the master node is out of date and cannot
 * be used. Instead, recovery must allocate an empty LEB for this purpose.
 * However, there may not be enough empty space, in which case it must be
 * possible to GC the dirtiest LEB into the GC head LEB.
 *
 * This function also runs the commit which causes the TNC updates from
 * size-recovery and orphans to be written to the flash. That is important to
 * ensure correct replay order for subsequent mounts.
 *
 * This function returns %0 on success and a negative error code on failure.
 */
int ubifs_rcvry_gc_commit(struct ubifs_info *c)
{
	struct ubifs_wbuf *wbuf = &c->jheads[GCHD].wbuf;
	struct ubifs_lprops lp;
	int err;

	dbg_rcvry("GC head LEB %d, offs %d", wbuf->lnum, wbuf->offs);

	c->gc_lnum = -1;
	if (wbuf->lnum == -1 || wbuf->offs == c->leb_size)
		return grab_empty_leb(c);

	err = ubifs_find_dirty_leb(c, &lp, wbuf->offs, 2);
	if (err) {
		if (err != -ENOSPC)
			return err;

		dbg_rcvry("could not find a dirty LEB");
		return grab_empty_leb(c);
	}

	ubifs_assert(!(lp.flags & LPROPS_INDEX));
	ubifs_assert(lp.free + lp.dirty >= wbuf->offs);

	dbg_rcvry("committing");
	err = ubifs_run_commit(c);
	if (err)
		return err;

	dbg_rcvry("GC'ing LEB %d", lp.lnum);
	mutex_lock_nested(&wbuf->io_mutex, wbuf->jhead);
	err = ubifs_garbage_collect_leb(c, &lp);
	if (err >= 0) {
		int err2 = ubifs_wbuf_sync_nolock(wbuf);

		if (err2)
			err = err2;
	}
	mutex_unlock(&wbuf->io_mutex);
	if (err < 0) {
		dbg_err("GC failed, error %d", err);
		if (err == -EAGAIN)
			err = -EINVAL;
		return err;
	}

	ubifs_assert(err == LEB_RETAINED);
	if (err != LEB_RETAINED)
		return -EINVAL;

	err = ubifs_leb_unmap(c, c->gc_lnum);
	if (err)
		return err;

	dbg_rcvry("allocated LEB %d for GC", lp.lnum);
	return 0;
}

struct size_entry {
	struct rb_node rb;
	ino_t inum;
	loff_t i_size;
	loff_t d_size;
	int exists;
	struct inode *inode;
};

static int add_ino(struct ubifs_info *c, ino_t inum, loff_t i_size,
		   loff_t d_size, int exists)
{
	struct rb_node **p = &c->size_tree.rb_node, *parent = NULL;
	struct size_entry *e;

	while (*p) {
		parent = *p;
		e = rb_entry(parent, struct size_entry, rb);
		if (inum < e->inum)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	e = kzalloc(sizeof(struct size_entry), GFP_KERNEL);
	if (!e)
		return -ENOMEM;

	e->inum = inum;
	e->i_size = i_size;
	e->d_size = d_size;
	e->exists = exists;

	rb_link_node(&e->rb, parent, p);
	rb_insert_color(&e->rb, &c->size_tree);

	return 0;
}

static struct size_entry *find_ino(struct ubifs_info *c, ino_t inum)
{
	struct rb_node *p = c->size_tree.rb_node;
	struct size_entry *e;

	while (p) {
		e = rb_entry(p, struct size_entry, rb);
		if (inum < e->inum)
			p = p->rb_left;
		else if (inum > e->inum)
			p = p->rb_right;
		else
			return e;
	}
	return NULL;
}

static void remove_ino(struct ubifs_info *c, ino_t inum)
{
	struct size_entry *e = find_ino(c, inum);

	if (!e)
		return;
	rb_erase(&e->rb, &c->size_tree);
	kfree(e);
}

void ubifs_destroy_size_tree(struct ubifs_info *c)
{
	struct rb_node *this = c->size_tree.rb_node;
	struct size_entry *e;

	while (this) {
		if (this->rb_left) {
			this = this->rb_left;
			continue;
		} else if (this->rb_right) {
			this = this->rb_right;
			continue;
		}
		e = rb_entry(this, struct size_entry, rb);
		if (e->inode)
			iput(e->inode);
		this = rb_parent(this);
		if (this) {
			if (this->rb_left == &e->rb)
				this->rb_left = NULL;
			else
				this->rb_right = NULL;
		}
		kfree(e);
	}
	c->size_tree = RB_ROOT;
}

int ubifs_recover_size_accum(struct ubifs_info *c, union ubifs_key *key,
			     int deletion, loff_t new_size)
{
	ino_t inum = key_inum(c, key);
	struct size_entry *e;
	int err;

	switch (key_type(c, key)) {
	case UBIFS_INO_KEY:
		if (deletion)
			remove_ino(c, inum);
		else {
			e = find_ino(c, inum);
			if (e) {
				e->i_size = new_size;
				e->exists = 1;
			} else {
				err = add_ino(c, inum, new_size, 0, 1);
				if (err)
					return err;
			}
		}
		break;
	case UBIFS_DATA_KEY:
		e = find_ino(c, inum);
		if (e) {
			if (new_size > e->d_size)
				e->d_size = new_size;
		} else {
			err = add_ino(c, inum, 0, new_size, 0);
			if (err)
				return err;
		}
		break;
	case UBIFS_TRUN_KEY:
		e = find_ino(c, inum);
		if (e)
			e->d_size = new_size;
		break;
	}
	return 0;
}

static int fix_size_in_place(struct ubifs_info *c, struct size_entry *e)
{
	struct ubifs_ino_node *ino = c->sbuf;
	unsigned char *p;
	union ubifs_key key;
	int err, lnum, offs, len;
	loff_t i_size;
	uint32_t crc;

	
	ino_key_init(c, &key, e->inum);
	err = ubifs_tnc_locate(c, &key, ino, &lnum, &offs);
	if (err)
		goto out;
	i_size = le64_to_cpu(ino->size);
	if (i_size >= e->d_size)
		return 0;
	
	err = ubifs_leb_read(c, lnum, c->sbuf, 0, c->leb_size, 1);
	if (err)
		goto out;
	
	ino = c->sbuf + offs;
	ino->size = cpu_to_le64(e->d_size);
	len = le32_to_cpu(ino->ch.len);
	crc = crc32(UBIFS_CRC32_INIT, (void *)ino + 8, len - 8);
	ino->ch.crc = cpu_to_le32(crc);
	
	p = c->sbuf;
	len = c->leb_size - 1;
	while (p[len] == 0xff)
		len -= 1;
	len = ALIGN(len + 1, c->min_io_size);
	
	err = ubifs_leb_change(c, lnum, c->sbuf, len, UBI_UNKNOWN);
	if (err)
		goto out;
	dbg_rcvry("inode %lu at %d:%d size %lld -> %lld",
		  (unsigned long)e->inum, lnum, offs, i_size, e->d_size);
	return 0;

out:
	ubifs_warn("inode %lu failed to fix size %lld -> %lld error %d",
		   (unsigned long)e->inum, e->i_size, e->d_size, err);
	return err;
}

int ubifs_recover_size(struct ubifs_info *c)
{
	struct rb_node *this = rb_first(&c->size_tree);

	while (this) {
		struct size_entry *e;
		int err;

		e = rb_entry(this, struct size_entry, rb);
		if (!e->exists) {
			union ubifs_key key;

			ino_key_init(c, &key, e->inum);
			err = ubifs_tnc_lookup(c, &key, c->sbuf);
			if (err && err != -ENOENT)
				return err;
			if (err == -ENOENT) {
				
				dbg_rcvry("removing ino %lu",
					  (unsigned long)e->inum);
				err = ubifs_tnc_remove_ino(c, e->inum);
				if (err)
					return err;
			} else {
				struct ubifs_ino_node *ino = c->sbuf;

				e->exists = 1;
				e->i_size = le64_to_cpu(ino->size);
			}
		}

		if (e->exists && e->i_size < e->d_size) {
			if (c->ro_mount) {
				
				struct inode *inode;
				struct ubifs_inode *ui;

				ubifs_assert(!e->inode);

				inode = ubifs_iget(c->vfs_sb, e->inum);
				if (IS_ERR(inode))
					return PTR_ERR(inode);

				ui = ubifs_inode(inode);
				if (inode->i_size < e->d_size) {
					dbg_rcvry("ino %lu size %lld -> %lld",
						  (unsigned long)e->inum,
						  inode->i_size, e->d_size);
					inode->i_size = e->d_size;
					ui->ui_size = e->d_size;
					ui->synced_i_size = e->d_size;
					e->inode = inode;
					this = rb_next(this);
					continue;
				}
				iput(inode);
			} else {
				
				err = fix_size_in_place(c, e);
				if (err)
					return err;
				if (e->inode)
					iput(e->inode);
			}
		}

		this = rb_next(this);
		rb_erase(&e->rb, &c->size_tree);
		kfree(e);
	}

	return 0;
}
