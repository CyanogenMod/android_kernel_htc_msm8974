/**
 * attrib.c - NTFS attribute operations.  Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2012 Anton Altaparmakov and Tuxera Inc.
 * Copyright (c) 2002 Richard Russon
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/buffer_head.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/writeback.h>

#include "attrib.h"
#include "debug.h"
#include "layout.h"
#include "lcnalloc.h"
#include "malloc.h"
#include "mft.h"
#include "ntfs.h"
#include "types.h"

int ntfs_map_runlist_nolock(ntfs_inode *ni, VCN vcn, ntfs_attr_search_ctx *ctx)
{
	VCN end_vcn;
	unsigned long flags;
	ntfs_inode *base_ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	runlist_element *rl;
	struct page *put_this_page = NULL;
	int err = 0;
	bool ctx_is_temporary, ctx_needs_reset;
	ntfs_attr_search_ctx old_ctx = { NULL, };

	ntfs_debug("Mapping runlist part containing vcn 0x%llx.",
			(unsigned long long)vcn);
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	if (!ctx) {
		ctx_is_temporary = ctx_needs_reset = true;
		m = map_mft_record(base_ni);
		if (IS_ERR(m))
			return PTR_ERR(m);
		ctx = ntfs_attr_get_search_ctx(base_ni, m);
		if (unlikely(!ctx)) {
			err = -ENOMEM;
			goto err_out;
		}
	} else {
		VCN allocated_size_vcn;

		BUG_ON(IS_ERR(ctx->mrec));
		a = ctx->attr;
		BUG_ON(!a->non_resident);
		ctx_is_temporary = false;
		end_vcn = sle64_to_cpu(a->data.non_resident.highest_vcn);
		read_lock_irqsave(&ni->size_lock, flags);
		allocated_size_vcn = ni->allocated_size >>
				ni->vol->cluster_size_bits;
		read_unlock_irqrestore(&ni->size_lock, flags);
		if (!a->data.non_resident.lowest_vcn && end_vcn <= 0)
			end_vcn = allocated_size_vcn - 1;
		if (vcn >= allocated_size_vcn || (a->type == ni->type &&
				a->name_length == ni->name_len &&
				!memcmp((u8*)a + le16_to_cpu(a->name_offset),
				ni->name, ni->name_len) &&
				sle64_to_cpu(a->data.non_resident.lowest_vcn)
				<= vcn && end_vcn >= vcn))
			ctx_needs_reset = false;
		else {
			
			old_ctx = *ctx;
			if (old_ctx.base_ntfs_ino && old_ctx.ntfs_ino !=
					old_ctx.base_ntfs_ino) {
				put_this_page = old_ctx.ntfs_ino->page;
				page_cache_get(put_this_page);
			}
			ntfs_attr_reinit_search_ctx(ctx);
			ctx_needs_reset = true;
		}
	}
	if (ctx_needs_reset) {
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
				CASE_SENSITIVE, vcn, NULL, 0, ctx);
		if (unlikely(err)) {
			if (err == -ENOENT)
				err = -EIO;
			goto err_out;
		}
		BUG_ON(!ctx->attr->non_resident);
	}
	a = ctx->attr;
	end_vcn = sle64_to_cpu(a->data.non_resident.highest_vcn) + 1;
	if (unlikely(vcn && vcn >= end_vcn)) {
		err = -ENOENT;
		goto err_out;
	}
	rl = ntfs_mapping_pairs_decompress(ni->vol, a, ni->runlist.rl);
	if (IS_ERR(rl))
		err = PTR_ERR(rl);
	else
		ni->runlist.rl = rl;
err_out:
	if (ctx_is_temporary) {
		if (likely(ctx))
			ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(base_ni);
	} else if (ctx_needs_reset) {
		if (NInoAttrList(base_ni)) {
			if (ctx->ntfs_ino != old_ctx.ntfs_ino) {
				if (ctx->base_ntfs_ino && ctx->ntfs_ino !=
						ctx->base_ntfs_ino) {
					unmap_extent_mft_record(ctx->ntfs_ino);
					ctx->mrec = ctx->base_mrec;
					BUG_ON(!ctx->mrec);
				}
				if (old_ctx.base_ntfs_ino &&
						old_ctx.ntfs_ino !=
						old_ctx.base_ntfs_ino) {
retry_map:
					ctx->mrec = map_mft_record(
							old_ctx.ntfs_ino);
					if (IS_ERR(ctx->mrec)) {
						if (PTR_ERR(ctx->mrec) ==
								-ENOMEM) {
							schedule();
							goto retry_map;
						} else
							old_ctx.ntfs_ino =
								old_ctx.
								base_ntfs_ino;
					}
				}
			}
			
			if (ctx->mrec != old_ctx.mrec) {
				if (!IS_ERR(ctx->mrec))
					old_ctx.attr = (ATTR_RECORD*)(
							(u8*)ctx->mrec +
							((u8*)old_ctx.attr -
							(u8*)old_ctx.mrec));
				old_ctx.mrec = ctx->mrec;
			}
		}
		
		*ctx = old_ctx;
		/*
		 * We drop the reference on the page we took earlier.  In the
		 * case that IS_ERR(ctx->mrec) is true this means we might lose
		 * some changes to the mft record that had been made between
		 * the last time it was marked dirty/written out and now.  This
		 * at this stage is not a problem as the mapping error is fatal
		 * enough that the mft record cannot be written out anyway and
		 * the caller is very likely to shutdown the whole inode
		 * immediately and mark the volume dirty for chkdsk to pick up
		 * the pieces anyway.
		 */
		if (put_this_page)
			page_cache_release(put_this_page);
	}
	return err;
}

int ntfs_map_runlist(ntfs_inode *ni, VCN vcn)
{
	int err = 0;

	down_write(&ni->runlist.lock);
	
	if (likely(ntfs_rl_vcn_to_lcn(ni->runlist.rl, vcn) <=
			LCN_RL_NOT_MAPPED))
		err = ntfs_map_runlist_nolock(ni, vcn, NULL);
	up_write(&ni->runlist.lock);
	return err;
}

LCN ntfs_attr_vcn_to_lcn_nolock(ntfs_inode *ni, const VCN vcn,
		const bool write_locked)
{
	LCN lcn;
	unsigned long flags;
	bool is_retry = false;

	BUG_ON(!ni);
	ntfs_debug("Entering for i_ino 0x%lx, vcn 0x%llx, %s_locked.",
			ni->mft_no, (unsigned long long)vcn,
			write_locked ? "write" : "read");
	BUG_ON(!NInoNonResident(ni));
	BUG_ON(vcn < 0);
	if (!ni->runlist.rl) {
		read_lock_irqsave(&ni->size_lock, flags);
		if (!ni->allocated_size) {
			read_unlock_irqrestore(&ni->size_lock, flags);
			return LCN_ENOENT;
		}
		read_unlock_irqrestore(&ni->size_lock, flags);
	}
retry_remap:
	
	lcn = ntfs_rl_vcn_to_lcn(ni->runlist.rl, vcn);
	if (likely(lcn >= LCN_HOLE)) {
		ntfs_debug("Done, lcn 0x%llx.", (long long)lcn);
		return lcn;
	}
	if (lcn != LCN_RL_NOT_MAPPED) {
		if (lcn != LCN_ENOENT)
			lcn = LCN_EIO;
	} else if (!is_retry) {
		int err;

		if (!write_locked) {
			up_read(&ni->runlist.lock);
			down_write(&ni->runlist.lock);
			if (unlikely(ntfs_rl_vcn_to_lcn(ni->runlist.rl, vcn) !=
					LCN_RL_NOT_MAPPED)) {
				up_write(&ni->runlist.lock);
				down_read(&ni->runlist.lock);
				goto retry_remap;
			}
		}
		err = ntfs_map_runlist_nolock(ni, vcn, NULL);
		if (!write_locked) {
			up_write(&ni->runlist.lock);
			down_read(&ni->runlist.lock);
		}
		if (likely(!err)) {
			is_retry = true;
			goto retry_remap;
		}
		if (err == -ENOENT)
			lcn = LCN_ENOENT;
		else if (err == -ENOMEM)
			lcn = LCN_ENOMEM;
		else
			lcn = LCN_EIO;
	}
	if (lcn != LCN_ENOENT)
		ntfs_error(ni->vol->sb, "Failed with error code %lli.",
				(long long)lcn);
	return lcn;
}

runlist_element *ntfs_attr_find_vcn_nolock(ntfs_inode *ni, const VCN vcn,
		ntfs_attr_search_ctx *ctx)
{
	unsigned long flags;
	runlist_element *rl;
	int err = 0;
	bool is_retry = false;

	BUG_ON(!ni);
	ntfs_debug("Entering for i_ino 0x%lx, vcn 0x%llx, with%s ctx.",
			ni->mft_no, (unsigned long long)vcn, ctx ? "" : "out");
	BUG_ON(!NInoNonResident(ni));
	BUG_ON(vcn < 0);
	if (!ni->runlist.rl) {
		read_lock_irqsave(&ni->size_lock, flags);
		if (!ni->allocated_size) {
			read_unlock_irqrestore(&ni->size_lock, flags);
			return ERR_PTR(-ENOENT);
		}
		read_unlock_irqrestore(&ni->size_lock, flags);
	}
retry_remap:
	rl = ni->runlist.rl;
	if (likely(rl && vcn >= rl[0].vcn)) {
		while (likely(rl->length)) {
			if (unlikely(vcn < rl[1].vcn)) {
				if (likely(rl->lcn >= LCN_HOLE)) {
					ntfs_debug("Done.");
					return rl;
				}
				break;
			}
			rl++;
		}
		if (likely(rl->lcn != LCN_RL_NOT_MAPPED)) {
			if (likely(rl->lcn == LCN_ENOENT))
				err = -ENOENT;
			else
				err = -EIO;
		}
	}
	if (!err && !is_retry) {
		if (IS_ERR(ctx->mrec))
			err = PTR_ERR(ctx->mrec);
		else {
			err = ntfs_map_runlist_nolock(ni, vcn, ctx);
			if (likely(!err)) {
				is_retry = true;
				goto retry_remap;
			}
		}
		if (err == -EINVAL)
			err = -EIO;
	} else if (!err)
		err = -EIO;
	if (err != -ENOENT)
		ntfs_error(ni->vol->sb, "Failed with error code %i.", err);
	return ERR_PTR(err);
}

static int ntfs_attr_find(const ATTR_TYPE type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
	ATTR_RECORD *a;
	ntfs_volume *vol = ctx->ntfs_ino->vol;
	ntfschar *upcase = vol->upcase;
	u32 upcase_len = vol->upcase_len;

	if (ctx->is_first) {
		a = ctx->attr;
		ctx->is_first = false;
	} else
		a = (ATTR_RECORD*)((u8*)ctx->attr +
				le32_to_cpu(ctx->attr->length));
	for (;;	a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length))) {
		if ((u8*)a < (u8*)ctx->mrec || (u8*)a > (u8*)ctx->mrec +
				le32_to_cpu(ctx->mrec->bytes_allocated))
			break;
		ctx->attr = a;
		if (unlikely(le32_to_cpu(a->type) > le32_to_cpu(type) ||
				a->type == AT_END))
			return -ENOENT;
		if (unlikely(!a->length))
			break;
		if (a->type != type)
			continue;
		if (!name) {
			
			if (a->name_length)
				return -ENOENT;
		} else if (!ntfs_are_names_equal(name, name_len,
			    (ntfschar*)((u8*)a + le16_to_cpu(a->name_offset)),
			    a->name_length, ic, upcase, upcase_len)) {
			register int rc;

			rc = ntfs_collate_names(name, name_len,
					(ntfschar*)((u8*)a +
					le16_to_cpu(a->name_offset)),
					a->name_length, 1, IGNORE_CASE,
					upcase, upcase_len);
			if (rc == -1)
				return -ENOENT;
			
			if (rc)
				continue;
			rc = ntfs_collate_names(name, name_len,
					(ntfschar*)((u8*)a +
					le16_to_cpu(a->name_offset)),
					a->name_length, 1, CASE_SENSITIVE,
					upcase, upcase_len);
			if (rc == -1)
				return -ENOENT;
			if (rc)
				continue;
		}
		if (!val)
			return 0;
		
		else {
			register int rc;

			rc = memcmp(val, (u8*)a + le16_to_cpu(
					a->data.resident.value_offset),
					min_t(u32, val_len, le32_to_cpu(
					a->data.resident.value_length)));
			if (!rc) {
				register u32 avl;

				avl = le32_to_cpu(
						a->data.resident.value_length);
				if (val_len == avl)
					return 0;
				if (val_len < avl)
					return -ENOENT;
			} else if (rc < 0)
				return -ENOENT;
		}
	}
	ntfs_error(vol->sb, "Inode is corrupt.  Run chkdsk.");
	NVolSetErrors(vol);
	return -EIO;
}

int load_attribute_list(ntfs_volume *vol, runlist *runlist, u8 *al_start,
		const s64 size, const s64 initialized_size)
{
	LCN lcn;
	u8 *al = al_start;
	u8 *al_end = al + initialized_size;
	runlist_element *rl;
	struct buffer_head *bh;
	struct super_block *sb;
	unsigned long block_size;
	unsigned long block, max_block;
	int err = 0;
	unsigned char block_size_bits;

	ntfs_debug("Entering.");
	if (!vol || !runlist || !al || size <= 0 || initialized_size < 0 ||
			initialized_size > size)
		return -EINVAL;
	if (!initialized_size) {
		memset(al, 0, size);
		return 0;
	}
	sb = vol->sb;
	block_size = sb->s_blocksize;
	block_size_bits = sb->s_blocksize_bits;
	down_read(&runlist->lock);
	rl = runlist->rl;
	if (!rl) {
		ntfs_error(sb, "Cannot read attribute list since runlist is "
				"missing.");
		goto err_out;	
	}
	
	while (rl->length) {
		lcn = ntfs_rl_vcn_to_lcn(rl, rl->vcn);
		ntfs_debug("Reading vcn = 0x%llx, lcn = 0x%llx.",
				(unsigned long long)rl->vcn,
				(unsigned long long)lcn);
		
		if (lcn < 0) {
			ntfs_error(sb, "ntfs_rl_vcn_to_lcn() failed.  Cannot "
					"read attribute list.");
			goto err_out;
		}
		block = lcn << vol->cluster_size_bits >> block_size_bits;
		
		max_block = block + (rl->length << vol->cluster_size_bits >>
				block_size_bits);
		ntfs_debug("max_block = 0x%lx.", max_block);
		do {
			ntfs_debug("Reading block = 0x%lx.", block);
			bh = sb_bread(sb, block);
			if (!bh) {
				ntfs_error(sb, "sb_bread() failed. Cannot "
						"read attribute list.");
				goto err_out;
			}
			if (al + block_size >= al_end)
				goto do_final;
			memcpy(al, bh->b_data, block_size);
			brelse(bh);
			al += block_size;
		} while (++block < max_block);
		rl++;
	}
	if (initialized_size < size) {
initialize:
		memset(al_start + initialized_size, 0, size - initialized_size);
	}
done:
	up_read(&runlist->lock);
	return err;
do_final:
	if (al < al_end) {
		memcpy(al, bh->b_data, al_end - al);
		brelse(bh);
		if (initialized_size < size)
			goto initialize;
		goto done;
	}
	brelse(bh);
	
	ntfs_error(sb, "Attribute list buffer overflow. Read attribute list "
			"is truncated.");
err_out:
	err = -EIO;
	goto done;
}

static int ntfs_external_attr_find(const ATTR_TYPE type,
		const ntfschar *name, const u32 name_len,
		const IGNORE_CASE_BOOL ic, const VCN lowest_vcn,
		const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *base_ni, *ni;
	ntfs_volume *vol;
	ATTR_LIST_ENTRY *al_entry, *next_al_entry;
	u8 *al_start, *al_end;
	ATTR_RECORD *a;
	ntfschar *al_name;
	u32 al_name_len;
	int err = 0;
	static const char *es = " Unmount and run chkdsk.";

	ni = ctx->ntfs_ino;
	base_ni = ctx->base_ntfs_ino;
	ntfs_debug("Entering for inode 0x%lx, type 0x%x.", ni->mft_no, type);
	if (!base_ni) {
		
		base_ni = ctx->base_ntfs_ino = ctx->ntfs_ino;
		ctx->base_mrec = ctx->mrec;
	}
	if (ni == base_ni)
		ctx->base_attr = ctx->attr;
	if (type == AT_END)
		goto not_found;
	vol = base_ni->vol;
	al_start = base_ni->attr_list;
	al_end = al_start + base_ni->attr_list_size;
	if (!ctx->al_entry)
		ctx->al_entry = (ATTR_LIST_ENTRY*)al_start;
	if (ctx->is_first) {
		al_entry = ctx->al_entry;
		ctx->is_first = false;
	} else
		al_entry = (ATTR_LIST_ENTRY*)((u8*)ctx->al_entry +
				le16_to_cpu(ctx->al_entry->length));
	for (;; al_entry = next_al_entry) {
		
		if ((u8*)al_entry < base_ni->attr_list ||
				(u8*)al_entry > al_end)
			break;	
		ctx->al_entry = al_entry;
		
		if ((u8*)al_entry == al_end)
			goto not_found;
		if (!al_entry->length)
			break;
		if ((u8*)al_entry + 6 > al_end || (u8*)al_entry +
				le16_to_cpu(al_entry->length) > al_end)
			break;
		next_al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
				le16_to_cpu(al_entry->length));
		if (le32_to_cpu(al_entry->type) > le32_to_cpu(type))
			goto not_found;
		if (type != al_entry->type)
			continue;
		al_name_len = al_entry->name_length;
		al_name = (ntfschar*)((u8*)al_entry + al_entry->name_offset);
		if (!name) {
			if (al_name_len)
				goto not_found;
		} else if (!ntfs_are_names_equal(al_name, al_name_len, name,
				name_len, ic, vol->upcase, vol->upcase_len)) {
			register int rc;

			rc = ntfs_collate_names(name, name_len, al_name,
					al_name_len, 1, IGNORE_CASE,
					vol->upcase, vol->upcase_len);
			if (rc == -1)
				goto not_found;
			
			if (rc)
				continue;
			rc = ntfs_collate_names(name, name_len, al_name,
					al_name_len, 1, CASE_SENSITIVE,
					vol->upcase, vol->upcase_len);
			if (rc == -1)
				goto not_found;
			if (rc)
				continue;
		}
		if (lowest_vcn && (u8*)next_al_entry >= al_start	    &&
				(u8*)next_al_entry + 6 < al_end		    &&
				(u8*)next_al_entry + le16_to_cpu(
					next_al_entry->length) <= al_end    &&
				sle64_to_cpu(next_al_entry->lowest_vcn) <=
					lowest_vcn			    &&
				next_al_entry->type == al_entry->type	    &&
				next_al_entry->name_length == al_name_len   &&
				ntfs_are_names_equal((ntfschar*)((u8*)
					next_al_entry +
					next_al_entry->name_offset),
					next_al_entry->name_length,
					al_name, al_name_len, CASE_SENSITIVE,
					vol->upcase, vol->upcase_len))
			continue;
		if (MREF_LE(al_entry->mft_reference) == ni->mft_no) {
			if (MSEQNO_LE(al_entry->mft_reference) != ni->seq_no) {
				ntfs_error(vol->sb, "Found stale mft "
						"reference in attribute list "
						"of base inode 0x%lx.%s",
						base_ni->mft_no, es);
				err = -EIO;
				break;
			}
		} else { 
			
			if (ni != base_ni)
				unmap_extent_mft_record(ni);
			
			if (MREF_LE(al_entry->mft_reference) ==
					base_ni->mft_no) {
				ni = ctx->ntfs_ino = base_ni;
				ctx->mrec = ctx->base_mrec;
			} else {
				
				ctx->mrec = map_extent_mft_record(base_ni,
						le64_to_cpu(
						al_entry->mft_reference), &ni);
				if (IS_ERR(ctx->mrec)) {
					ntfs_error(vol->sb, "Failed to map "
							"extent mft record "
							"0x%lx of base inode "
							"0x%lx.%s",
							MREF_LE(al_entry->
							mft_reference),
							base_ni->mft_no, es);
					err = PTR_ERR(ctx->mrec);
					if (err == -ENOENT)
						err = -EIO;
					
					ni = NULL;
					break;
				}
				ctx->ntfs_ino = ni;
			}
			ctx->attr = (ATTR_RECORD*)((u8*)ctx->mrec +
					le16_to_cpu(ctx->mrec->attrs_offset));
		}
		a = ctx->attr;
do_next_attr_loop:
		if ((u8*)a < (u8*)ctx->mrec || (u8*)a > (u8*)ctx->mrec +
				le32_to_cpu(ctx->mrec->bytes_allocated))
			break;
		if (a->type == AT_END)
			break;
		if (!a->length)
			break;
		if (al_entry->instance != a->instance)
			goto do_next_attr;
		if (al_entry->type != a->type)
			break;
		if (!ntfs_are_names_equal((ntfschar*)((u8*)a +
				le16_to_cpu(a->name_offset)), a->name_length,
				al_name, al_name_len, CASE_SENSITIVE,
				vol->upcase, vol->upcase_len))
			break;
		ctx->attr = a;
		if (!val || (!a->non_resident && le32_to_cpu(
				a->data.resident.value_length) == val_len &&
				!memcmp((u8*)a +
				le16_to_cpu(a->data.resident.value_offset),
				val, val_len))) {
			ntfs_debug("Done, found.");
			return 0;
		}
do_next_attr:
		
		a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length));
		goto do_next_attr_loop;
	}
	if (!err) {
		ntfs_error(vol->sb, "Base inode 0x%lx contains corrupt "
				"attribute list attribute.%s", base_ni->mft_no,
				es);
		err = -EIO;
	}
	if (ni != base_ni) {
		if (ni)
			unmap_extent_mft_record(ni);
		ctx->ntfs_ino = base_ni;
		ctx->mrec = ctx->base_mrec;
		ctx->attr = ctx->base_attr;
	}
	if (err != -ENOMEM)
		NVolSetErrors(vol);
	return err;
not_found:
	if (type == AT_END) {
		ntfs_attr_reinit_search_ctx(ctx);
		return ntfs_attr_find(AT_END, name, name_len, ic, val, val_len,
				ctx);
	}
	if (ni != base_ni)
		unmap_extent_mft_record(ni);
	ctx->mrec = ctx->base_mrec;
	ctx->attr = (ATTR_RECORD*)((u8*)ctx->mrec +
			le16_to_cpu(ctx->mrec->attrs_offset));
	ctx->is_first = true;
	ctx->ntfs_ino = base_ni;
	ctx->base_ntfs_ino = NULL;
	ctx->base_mrec = NULL;
	ctx->base_attr = NULL;
	do {
		err = ntfs_attr_find(type, name, name_len, ic, val, val_len,
				ctx);
	} while (!err);
	ntfs_debug("Done, not found.");
	return err;
}

int ntfs_attr_lookup(const ATTR_TYPE type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const VCN lowest_vcn, const u8 *val, const u32 val_len,
		ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *base_ni;

	ntfs_debug("Entering.");
	BUG_ON(IS_ERR(ctx->mrec));
	if (ctx->base_ntfs_ino)
		base_ni = ctx->base_ntfs_ino;
	else
		base_ni = ctx->ntfs_ino;
	
	BUG_ON(!base_ni);
	if (!NInoAttrList(base_ni) || type == AT_ATTRIBUTE_LIST)
		return ntfs_attr_find(type, name, name_len, ic, val, val_len,
				ctx);
	return ntfs_external_attr_find(type, name, name_len, ic, lowest_vcn,
			val, val_len, ctx);
}

static inline void ntfs_attr_init_search_ctx(ntfs_attr_search_ctx *ctx,
		ntfs_inode *ni, MFT_RECORD *mrec)
{
	*ctx = (ntfs_attr_search_ctx) {
		.mrec = mrec,
		
		.attr = (ATTR_RECORD*)((u8*)mrec +
				le16_to_cpu(mrec->attrs_offset)),
		.is_first = true,
		.ntfs_ino = ni,
	};
}

void ntfs_attr_reinit_search_ctx(ntfs_attr_search_ctx *ctx)
{
	if (likely(!ctx->base_ntfs_ino)) {
		
		ctx->is_first = true;
		
		ctx->attr = (ATTR_RECORD*)((u8*)ctx->mrec +
				le16_to_cpu(ctx->mrec->attrs_offset));
		ctx->al_entry = NULL;
		return;
	} 
	if (ctx->ntfs_ino != ctx->base_ntfs_ino)
		unmap_extent_mft_record(ctx->ntfs_ino);
	ntfs_attr_init_search_ctx(ctx, ctx->base_ntfs_ino, ctx->base_mrec);
	return;
}

ntfs_attr_search_ctx *ntfs_attr_get_search_ctx(ntfs_inode *ni, MFT_RECORD *mrec)
{
	ntfs_attr_search_ctx *ctx;

	ctx = kmem_cache_alloc(ntfs_attr_ctx_cache, GFP_NOFS);
	if (ctx)
		ntfs_attr_init_search_ctx(ctx, ni, mrec);
	return ctx;
}

void ntfs_attr_put_search_ctx(ntfs_attr_search_ctx *ctx)
{
	if (ctx->base_ntfs_ino && ctx->ntfs_ino != ctx->base_ntfs_ino)
		unmap_extent_mft_record(ctx->ntfs_ino);
	kmem_cache_free(ntfs_attr_ctx_cache, ctx);
	return;
}

#ifdef NTFS_RW

static ATTR_DEF *ntfs_attr_find_in_attrdef(const ntfs_volume *vol,
		const ATTR_TYPE type)
{
	ATTR_DEF *ad;

	BUG_ON(!vol->attrdef);
	BUG_ON(!type);
	for (ad = vol->attrdef; (u8*)ad - (u8*)vol->attrdef <
			vol->attrdef_size && ad->type; ++ad) {
		
		if (likely(le32_to_cpu(ad->type) < le32_to_cpu(type)))
			continue;
		
		if (likely(ad->type == type))
			return ad;
		
		break;
	}
	
	ntfs_debug("Attribute type 0x%x not found in $AttrDef.",
			le32_to_cpu(type));
	return NULL;
}

int ntfs_attr_size_bounds_check(const ntfs_volume *vol, const ATTR_TYPE type,
		const s64 size)
{
	ATTR_DEF *ad;

	BUG_ON(size < 0);
	if (unlikely(type == AT_ATTRIBUTE_LIST && size > 256 * 1024))
		return -ERANGE;
	
	ad = ntfs_attr_find_in_attrdef(vol, type);
	if (unlikely(!ad))
		return -ENOENT;
	
	if (((sle64_to_cpu(ad->min_size) > 0) &&
			size < sle64_to_cpu(ad->min_size)) ||
			((sle64_to_cpu(ad->max_size) > 0) && size >
			sle64_to_cpu(ad->max_size)))
		return -ERANGE;
	return 0;
}

int ntfs_attr_can_be_non_resident(const ntfs_volume *vol, const ATTR_TYPE type)
{
	ATTR_DEF *ad;

	
	ad = ntfs_attr_find_in_attrdef(vol, type);
	if (unlikely(!ad))
		return -ENOENT;
	
	if (ad->flags & ATTR_DEF_RESIDENT)
		return -EPERM;
	return 0;
}

int ntfs_attr_can_be_resident(const ntfs_volume *vol, const ATTR_TYPE type)
{
	if (type == AT_INDEX_ALLOCATION)
		return -EPERM;
	return 0;
}

/**
 * ntfs_attr_record_resize - resize an attribute record
 * @m:		mft record containing attribute record
 * @a:		attribute record to resize
 * @new_size:	new size in bytes to which to resize the attribute record @a
 *
 * Resize the attribute record @a, i.e. the resident part of the attribute, in
 * the mft record @m to @new_size bytes.
 *
 * Return 0 on success and -errno on error.  The following error codes are
 * defined:
 *	-ENOSPC	- Not enough space in the mft record @m to perform the resize.
 *
 * Note: On error, no modifications have been performed whatsoever.
 *
 * Warning: If you make a record smaller without having copied all the data you
 *	    are interested in the data may be overwritten.
 */
int ntfs_attr_record_resize(MFT_RECORD *m, ATTR_RECORD *a, u32 new_size)
{
	ntfs_debug("Entering for new_size %u.", new_size);
	
	if (new_size & 7)
		new_size = (new_size + 7) & ~7;
	
	if (new_size != le32_to_cpu(a->length)) {
		u32 new_muse = le32_to_cpu(m->bytes_in_use) -
				le32_to_cpu(a->length) + new_size;
		
		if (new_muse > le32_to_cpu(m->bytes_allocated))
			return -ENOSPC;
		
		memmove((u8*)a + new_size, (u8*)a + le32_to_cpu(a->length),
				le32_to_cpu(m->bytes_in_use) - ((u8*)a -
				(u8*)m) - le32_to_cpu(a->length));
		
		m->bytes_in_use = cpu_to_le32(new_muse);
		
		if (new_size >= offsetof(ATTR_REC, length) + sizeof(a->length))
			a->length = cpu_to_le32(new_size);
	}
	return 0;
}

/**
 * ntfs_resident_attr_value_resize - resize the value of a resident attribute
 * @m:		mft record containing attribute record
 * @a:		attribute record whose value to resize
 * @new_size:	new size in bytes to which to resize the attribute value of @a
 *
 * Resize the value of the attribute @a in the mft record @m to @new_size bytes.
 * If the value is made bigger, the newly allocated space is cleared.
 *
 * Return 0 on success and -errno on error.  The following error codes are
 * defined:
 *	-ENOSPC	- Not enough space in the mft record @m to perform the resize.
 *
 * Note: On error, no modifications have been performed whatsoever.
 *
 * Warning: If you make a record smaller without having copied all the data you
 *	    are interested in the data may be overwritten.
 */
int ntfs_resident_attr_value_resize(MFT_RECORD *m, ATTR_RECORD *a,
		const u32 new_size)
{
	u32 old_size;

	
	if (ntfs_attr_record_resize(m, a,
			le16_to_cpu(a->data.resident.value_offset) + new_size))
		return -ENOSPC;
	old_size = le32_to_cpu(a->data.resident.value_length);
	if (new_size > old_size)
		memset((u8*)a + le16_to_cpu(a->data.resident.value_offset) +
				old_size, 0, new_size - old_size);
	
	a->data.resident.value_length = cpu_to_le32(new_size);
	return 0;
}

int ntfs_attr_make_non_resident(ntfs_inode *ni, const u32 data_size)
{
	s64 new_size;
	struct inode *vi = VFS_I(ni);
	ntfs_volume *vol = ni->vol;
	ntfs_inode *base_ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *ctx;
	struct page *page;
	runlist_element *rl;
	u8 *kaddr;
	unsigned long flags;
	int mp_size, mp_ofs, name_ofs, arec_size, err, err2;
	u32 attr_size;
	u8 old_res_attr_flags;

	
	err = ntfs_attr_can_be_non_resident(vol, ni->type);
	if (unlikely(err)) {
		if (err == -EPERM)
			ntfs_debug("Attribute is not allowed to be "
					"non-resident.");
		else
			ntfs_debug("Attribute not defined on the NTFS "
					"volume!");
		return err;
	}
	BUG_ON(NInoCompressed(ni));
	BUG_ON(NInoEncrypted(ni));
	new_size = (data_size + vol->cluster_size - 1) &
			~(vol->cluster_size - 1);
	if (new_size > 0) {
		page = find_or_create_page(vi->i_mapping, 0,
				mapping_gfp_mask(vi->i_mapping));
		if (unlikely(!page))
			return -ENOMEM;
		
		rl = ntfs_cluster_alloc(vol, 0, new_size >>
				vol->cluster_size_bits, -1, DATA_ZONE, true);
		if (IS_ERR(rl)) {
			err = PTR_ERR(rl);
			ntfs_debug("Failed to allocate cluster%s, error code "
					"%i.", (new_size >>
					vol->cluster_size_bits) > 1 ? "s" : "",
					err);
			goto page_err_out;
		}
	} else {
		rl = NULL;
		page = NULL;
	}
	
	mp_size = ntfs_get_size_for_mapping_pairs(vol, rl, 0, -1);
	if (unlikely(mp_size < 0)) {
		err = mp_size;
		ntfs_debug("Failed to get size for mapping pairs array, error "
				"code %i.", err);
		goto rl_err_out;
	}
	down_write(&ni->runlist.lock);
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		m = NULL;
		ctx = NULL;
		goto err_out;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto err_out;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT)
			err = -EIO;
		goto err_out;
	}
	m = ctx->mrec;
	a = ctx->attr;
	BUG_ON(NInoNonResident(ni));
	BUG_ON(a->non_resident);
	if (NInoSparse(ni) || NInoCompressed(ni))
		name_ofs = (offsetof(ATTR_REC,
				data.non_resident.compressed_size) +
				sizeof(a->data.non_resident.compressed_size) +
				7) & ~7;
	else
		name_ofs = (offsetof(ATTR_REC,
				data.non_resident.compressed_size) + 7) & ~7;
	mp_ofs = (name_ofs + a->name_length * sizeof(ntfschar) + 7) & ~7;
	arec_size = (mp_ofs + mp_size + 7) & ~7;
	attr_size = le32_to_cpu(a->data.resident.value_length);
	BUG_ON(attr_size != data_size);
	if (page && !PageUptodate(page)) {
		kaddr = kmap_atomic(page);
		memcpy(kaddr, (u8*)a +
				le16_to_cpu(a->data.resident.value_offset),
				attr_size);
		memset(kaddr + attr_size, 0, PAGE_CACHE_SIZE - attr_size);
		kunmap_atomic(kaddr);
		flush_dcache_page(page);
		SetPageUptodate(page);
	}
	
	old_res_attr_flags = a->data.resident.flags;
	
	err = ntfs_attr_record_resize(m, a, arec_size);
	if (unlikely(err))
		goto err_out;
	a->non_resident = 1;
	
	if (a->name_length)
		memmove((u8*)a + name_ofs, (u8*)a + le16_to_cpu(a->name_offset),
				a->name_length * sizeof(ntfschar));
	a->name_offset = cpu_to_le16(name_ofs);
	
	a->data.non_resident.lowest_vcn = 0;
	a->data.non_resident.highest_vcn = cpu_to_sle64((new_size - 1) >>
			vol->cluster_size_bits);
	a->data.non_resident.mapping_pairs_offset = cpu_to_le16(mp_ofs);
	memset(&a->data.non_resident.reserved, 0,
			sizeof(a->data.non_resident.reserved));
	a->data.non_resident.allocated_size = cpu_to_sle64(new_size);
	a->data.non_resident.data_size =
			a->data.non_resident.initialized_size =
			cpu_to_sle64(attr_size);
	if (NInoSparse(ni) || NInoCompressed(ni)) {
		a->data.non_resident.compression_unit = 0;
		if (NInoCompressed(ni) || vol->major_ver < 3)
			a->data.non_resident.compression_unit = 4;
		a->data.non_resident.compressed_size =
				a->data.non_resident.allocated_size;
	} else
		a->data.non_resident.compression_unit = 0;
	
	err = ntfs_mapping_pairs_build(vol, (u8*)a + mp_ofs,
			arec_size - mp_ofs, rl, 0, -1, NULL);
	if (unlikely(err)) {
		ntfs_debug("Failed to build mapping pairs, error code %i.",
				err);
		goto undo_err_out;
	}
	
	ni->runlist.rl = rl;
	write_lock_irqsave(&ni->size_lock, flags);
	ni->allocated_size = new_size;
	if (NInoSparse(ni) || NInoCompressed(ni)) {
		ni->itype.compressed.size = ni->allocated_size;
		if (a->data.non_resident.compression_unit) {
			ni->itype.compressed.block_size = 1U << (a->data.
					non_resident.compression_unit +
					vol->cluster_size_bits);
			ni->itype.compressed.block_size_bits =
					ffs(ni->itype.compressed.block_size) -
					1;
			ni->itype.compressed.block_clusters = 1U <<
					a->data.non_resident.compression_unit;
		} else {
			ni->itype.compressed.block_size = 0;
			ni->itype.compressed.block_size_bits = 0;
			ni->itype.compressed.block_clusters = 0;
		}
		vi->i_blocks = ni->itype.compressed.size >> 9;
	} else
		vi->i_blocks = ni->allocated_size >> 9;
	write_unlock_irqrestore(&ni->size_lock, flags);
	NInoSetNonResident(ni);
	/* Mark the mft record dirty, so it gets written back. */
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
	up_write(&ni->runlist.lock);
	if (page) {
		set_page_dirty(page);
		unlock_page(page);
		mark_page_accessed(page);
		page_cache_release(page);
	}
	ntfs_debug("Done.");
	return 0;
undo_err_out:
	
	a->non_resident = 0;
	
	name_ofs = (offsetof(ATTR_RECORD, data.resident.reserved) +
			sizeof(a->data.resident.reserved) + 7) & ~7;
	if (a->name_length)
		memmove((u8*)a + name_ofs, (u8*)a + le16_to_cpu(a->name_offset),
				a->name_length * sizeof(ntfschar));
	mp_ofs = (name_ofs + a->name_length * sizeof(ntfschar) + 7) & ~7;
	a->name_offset = cpu_to_le16(name_ofs);
	arec_size = (mp_ofs + attr_size + 7) & ~7;
	
	err2 = ntfs_attr_record_resize(m, a, arec_size);
	if (unlikely(err2)) {
		arec_size = le32_to_cpu(a->length);
		if ((mp_ofs + attr_size) > arec_size) {
			err2 = attr_size;
			attr_size = arec_size - mp_ofs;
			ntfs_error(vol->sb, "Failed to undo partial resident "
					"to non-resident attribute "
					"conversion.  Truncating inode 0x%lx, "
					"attribute type 0x%x from %i bytes to "
					"%i bytes to maintain metadata "
					"consistency.  THIS MEANS YOU ARE "
					"LOSING %i BYTES DATA FROM THIS %s.",
					vi->i_ino,
					(unsigned)le32_to_cpu(ni->type),
					err2, attr_size, err2 - attr_size,
					((ni->type == AT_DATA) &&
					!ni->name_len) ? "FILE": "ATTRIBUTE");
			write_lock_irqsave(&ni->size_lock, flags);
			ni->initialized_size = attr_size;
			i_size_write(vi, attr_size);
			write_unlock_irqrestore(&ni->size_lock, flags);
		}
	}
	
	a->data.resident.value_length = cpu_to_le32(attr_size);
	a->data.resident.value_offset = cpu_to_le16(mp_ofs);
	a->data.resident.flags = old_res_attr_flags;
	memset(&a->data.resident.reserved, 0,
			sizeof(a->data.resident.reserved));
	
	if (page) {
		kaddr = kmap_atomic(page);
		memcpy((u8*)a + mp_ofs, kaddr, attr_size);
		kunmap_atomic(kaddr);
	}
	
	write_lock_irqsave(&ni->size_lock, flags);
	ni->allocated_size = arec_size - mp_ofs;
	write_unlock_irqrestore(&ni->size_lock, flags);
	/* Mark the mft record dirty, so it gets written back. */
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(base_ni);
	ni->runlist.rl = NULL;
	up_write(&ni->runlist.lock);
rl_err_out:
	if (rl) {
		if (ntfs_cluster_free_from_rl(vol, rl) < 0) {
			ntfs_error(vol->sb, "Failed to release allocated "
					"cluster(s) in error code path.  Run "
					"chkdsk to recover the lost "
					"cluster(s).");
			NVolSetErrors(vol);
		}
		ntfs_free(rl);
page_err_out:
		unlock_page(page);
		page_cache_release(page);
	}
	if (err == -EINVAL)
		err = -EIO;
	return err;
}

s64 ntfs_attr_extend_allocation(ntfs_inode *ni, s64 new_alloc_size,
		const s64 new_data_size, const s64 data_start)
{
	VCN vcn;
	s64 ll, allocated_size, start = data_start;
	struct inode *vi = VFS_I(ni);
	ntfs_volume *vol = ni->vol;
	ntfs_inode *base_ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *ctx;
	runlist_element *rl, *rl2;
	unsigned long flags;
	int err, mp_size;
	u32 attr_len = 0; 
	bool mp_rebuilt;

#ifdef DEBUG
	read_lock_irqsave(&ni->size_lock, flags);
	allocated_size = ni->allocated_size;
	read_unlock_irqrestore(&ni->size_lock, flags);
	ntfs_debug("Entering for i_ino 0x%lx, attribute type 0x%x, "
			"old_allocated_size 0x%llx, "
			"new_allocated_size 0x%llx, new_data_size 0x%llx, "
			"data_start 0x%llx.", vi->i_ino,
			(unsigned)le32_to_cpu(ni->type),
			(unsigned long long)allocated_size,
			(unsigned long long)new_alloc_size,
			(unsigned long long)new_data_size,
			(unsigned long long)start);
#endif
retry_extend:
	if (NInoNonResident(ni)) {
		if (start > 0)
			start &= ~(s64)vol->cluster_size_mask;
		new_alloc_size = (new_alloc_size + vol->cluster_size - 1) &
				~(s64)vol->cluster_size_mask;
	}
	BUG_ON(new_data_size >= 0 && new_data_size > new_alloc_size);
	
	err = ntfs_attr_size_bounds_check(vol, ni->type, new_alloc_size);
	if (unlikely(err)) {
		
		read_lock_irqsave(&ni->size_lock, flags);
		allocated_size = ni->allocated_size;
		read_unlock_irqrestore(&ni->size_lock, flags);
		if (start < 0 || start >= allocated_size) {
			if (err == -ERANGE) {
				ntfs_error(vol->sb, "Cannot extend allocation "
						"of inode 0x%lx, attribute "
						"type 0x%x, because the new "
						"allocation would exceed the "
						"maximum allowed size for "
						"this attribute type.",
						vi->i_ino, (unsigned)
						le32_to_cpu(ni->type));
			} else {
				ntfs_error(vol->sb, "Cannot extend allocation "
						"of inode 0x%lx, attribute "
						"type 0x%x, because this "
						"attribute type is not "
						"defined on the NTFS volume.  "
						"Possible corruption!  You "
						"should run chkdsk!",
						vi->i_ino, (unsigned)
						le32_to_cpu(ni->type));
			}
		}
		
		if (err == -ERANGE)
			err = -EFBIG;
		else
			err = -EIO;
		return err;
	}
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	down_write(&ni->runlist.lock);
	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		m = NULL;
		ctx = NULL;
		goto err_out;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto err_out;
	}
	read_lock_irqsave(&ni->size_lock, flags);
	allocated_size = ni->allocated_size;
	read_unlock_irqrestore(&ni->size_lock, flags);
	vcn = NInoNonResident(ni) ? allocated_size >> vol->cluster_size_bits :
			0;
	if (unlikely(new_alloc_size <= allocated_size)) {
		ntfs_debug("Allocated size already exceeds requested size.");
		new_alloc_size = allocated_size;
		if (new_data_size < 0)
			goto done;
		vcn = 0;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, vcn, NULL, 0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT)
			err = -EIO;
		goto err_out;
	}
	m = ctx->mrec;
	a = ctx->attr;
	
	if (a->non_resident)
		goto do_non_resident_extend;
	BUG_ON(NInoNonResident(ni));
	
	attr_len = le32_to_cpu(a->data.resident.value_length);
	if (new_alloc_size < vol->mft_record_size &&
			!ntfs_attr_record_resize(m, a,
			le16_to_cpu(a->data.resident.value_offset) +
			new_alloc_size)) {
		
		write_lock_irqsave(&ni->size_lock, flags);
		ni->allocated_size = le32_to_cpu(a->length) -
				le16_to_cpu(a->data.resident.value_offset);
		write_unlock_irqrestore(&ni->size_lock, flags);
		if (new_data_size >= 0) {
			BUG_ON(new_data_size < attr_len);
			a->data.resident.value_length =
					cpu_to_le32((u32)new_data_size);
		}
		goto flush_done;
	}
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
	up_write(&ni->runlist.lock);
	err = ntfs_attr_make_non_resident(ni, attr_len);
	if (likely(!err))
		goto retry_extend;
	if (unlikely(err != -EPERM && err != -ENOSPC)) {
		
		read_lock_irqsave(&ni->size_lock, flags);
		allocated_size = ni->allocated_size;
		read_unlock_irqrestore(&ni->size_lock, flags);
		if (start < 0 || start >= allocated_size)
			ntfs_error(vol->sb, "Cannot extend allocation of "
					"inode 0x%lx, attribute type 0x%x, "
					"because the conversion from resident "
					"to non-resident attribute failed "
					"with error code %i.", vi->i_ino,
					(unsigned)le32_to_cpu(ni->type), err);
		if (err != -ENOMEM)
			err = -EIO;
		goto conv_err_out;
	}
	
	read_lock_irqsave(&ni->size_lock, flags);
	allocated_size = ni->allocated_size;
	read_unlock_irqrestore(&ni->size_lock, flags);
	if (start < 0 || start >= allocated_size) {
		if (err == -ENOSPC)
			ntfs_error(vol->sb, "Not enough space in the mft "
					"record/on disk for the non-resident "
					"attribute value.  This case is not "
					"implemented yet.");
		else 
			ntfs_error(vol->sb, "This attribute type may not be "
					"non-resident.  This case is not "
					"implemented yet.");
	}
	err = -EOPNOTSUPP;
	goto conv_err_out;
#if 0
	
	if (!err)
		goto do_resident_extend;
	if (ni->type == AT_ATTRIBUTE_LIST ||
			ni->type == AT_STANDARD_INFORMATION) {
		
		
		err = -EOPNOTSUPP;
		if (!err)
			goto do_resident_extend;
		goto err_out;
	}
	
	
	
	err = -EOPNOTSUPP;
	if (!err)
		goto do_resident_extend;
	
	goto err_out;
#endif
do_non_resident_extend:
	BUG_ON(!NInoNonResident(ni));
	if (new_alloc_size == allocated_size) {
		BUG_ON(vcn);
		goto alloc_done;
	}
	if ((start >= 0 && start <= allocated_size) || ni->type != AT_DATA ||
			!NVolSparseEnabled(vol) || NInoSparseDisabled(ni))
		goto skip_sparse;
	
	
	ntfs_debug("Inserting holes is not-implemented yet.  Falling back to "
			"allocating real clusters instead.");
skip_sparse:
	rl = ni->runlist.rl;
	if (likely(rl)) {
		
		while (rl->length)
			rl++;
	}
	
	if (unlikely(!rl || rl->lcn == LCN_RL_NOT_MAPPED ||
			(rl->lcn == LCN_ENOENT && rl > ni->runlist.rl &&
			(rl-1)->lcn == LCN_RL_NOT_MAPPED))) {
		if (!rl && !allocated_size)
			goto first_alloc;
		rl = ntfs_mapping_pairs_decompress(vol, a, ni->runlist.rl);
		if (IS_ERR(rl)) {
			err = PTR_ERR(rl);
			if (start < 0 || start >= allocated_size)
				ntfs_error(vol->sb, "Cannot extend allocation "
						"of inode 0x%lx, attribute "
						"type 0x%x, because the "
						"mapping of a runlist "
						"fragment failed with error "
						"code %i.", vi->i_ino,
						(unsigned)le32_to_cpu(ni->type),
						err);
			if (err != -ENOMEM)
				err = -EIO;
			goto err_out;
		}
		ni->runlist.rl = rl;
		
		while (rl->length)
			rl++;
	}
	
	while (rl->lcn < 0 && rl > ni->runlist.rl)
		rl--;
first_alloc:
	
	
	
	rl2 = ntfs_cluster_alloc(vol, allocated_size >> vol->cluster_size_bits,
			(new_alloc_size - allocated_size) >>
			vol->cluster_size_bits, (rl && (rl->lcn >= 0)) ?
			rl->lcn + rl->length : -1, DATA_ZONE, true);
	if (IS_ERR(rl2)) {
		err = PTR_ERR(rl2);
		if (start < 0 || start >= allocated_size)
			ntfs_error(vol->sb, "Cannot extend allocation of "
					"inode 0x%lx, attribute type 0x%x, "
					"because the allocation of clusters "
					"failed with error code %i.", vi->i_ino,
					(unsigned)le32_to_cpu(ni->type), err);
		if (err != -ENOMEM && err != -ENOSPC)
			err = -EIO;
		goto err_out;
	}
	rl = ntfs_runlists_merge(ni->runlist.rl, rl2);
	if (IS_ERR(rl)) {
		err = PTR_ERR(rl);
		if (start < 0 || start >= allocated_size)
			ntfs_error(vol->sb, "Cannot extend allocation of "
					"inode 0x%lx, attribute type 0x%x, "
					"because the runlist merge failed "
					"with error code %i.", vi->i_ino,
					(unsigned)le32_to_cpu(ni->type), err);
		if (err != -ENOMEM)
			err = -EIO;
		if (ntfs_cluster_free_from_rl(vol, rl2)) {
			ntfs_error(vol->sb, "Failed to release allocated "
					"cluster(s) in error code path.  Run "
					"chkdsk to recover the lost "
					"cluster(s).");
			NVolSetErrors(vol);
		}
		ntfs_free(rl2);
		goto err_out;
	}
	ni->runlist.rl = rl;
	ntfs_debug("Allocated 0x%llx clusters.", (long long)(new_alloc_size -
			allocated_size) >> vol->cluster_size_bits);
	
	ll = sle64_to_cpu(a->data.non_resident.lowest_vcn);
	rl2 = ntfs_rl_find_vcn_nolock(rl, ll);
	BUG_ON(!rl2);
	BUG_ON(!rl2->length);
	BUG_ON(rl2->lcn < LCN_HOLE);
	mp_rebuilt = false;
	
	mp_size = ntfs_get_size_for_mapping_pairs(vol, rl2, ll, -1);
	if (unlikely(mp_size <= 0)) {
		err = mp_size;
		if (start < 0 || start >= allocated_size)
			ntfs_error(vol->sb, "Cannot extend allocation of "
					"inode 0x%lx, attribute type 0x%x, "
					"because determining the size for the "
					"mapping pairs failed with error code "
					"%i.", vi->i_ino,
					(unsigned)le32_to_cpu(ni->type), err);
		err = -EIO;
		goto undo_alloc;
	}
	
	attr_len = le32_to_cpu(a->length);
	err = ntfs_attr_record_resize(m, a, mp_size +
			le16_to_cpu(a->data.non_resident.mapping_pairs_offset));
	if (unlikely(err)) {
		BUG_ON(err != -ENOSPC);
		
		
		
		
		
		
		if (start < 0 || start >= allocated_size)
			ntfs_error(vol->sb, "Not enough space in the mft "
					"record for the extended attribute "
					"record.  This case is not "
					"implemented yet.");
		err = -EOPNOTSUPP;
		goto undo_alloc;
	}
	mp_rebuilt = true;
	
	err = ntfs_mapping_pairs_build(vol, (u8*)a +
			le16_to_cpu(a->data.non_resident.mapping_pairs_offset),
			mp_size, rl2, ll, -1, NULL);
	if (unlikely(err)) {
		if (start < 0 || start >= allocated_size)
			ntfs_error(vol->sb, "Cannot extend allocation of "
					"inode 0x%lx, attribute type 0x%x, "
					"because building the mapping pairs "
					"failed with error code %i.", vi->i_ino,
					(unsigned)le32_to_cpu(ni->type), err);
		err = -EIO;
		goto undo_alloc;
	}
	
	a->data.non_resident.highest_vcn = cpu_to_sle64((new_alloc_size >>
			vol->cluster_size_bits) - 1);
	if (a->data.non_resident.lowest_vcn) {
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		mark_mft_record_dirty(ctx->ntfs_ino);
		ntfs_attr_reinit_search_ctx(ctx);
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
				CASE_SENSITIVE, 0, NULL, 0, ctx);
		if (unlikely(err))
			goto restore_undo_alloc;
		
		a = ctx->attr;
	}
	write_lock_irqsave(&ni->size_lock, flags);
	ni->allocated_size = new_alloc_size;
	a->data.non_resident.allocated_size = cpu_to_sle64(new_alloc_size);
	if (NInoSparse(ni) || NInoCompressed(ni)) {
		ni->itype.compressed.size += new_alloc_size - allocated_size;
		a->data.non_resident.compressed_size =
				cpu_to_sle64(ni->itype.compressed.size);
		vi->i_blocks = ni->itype.compressed.size >> 9;
	} else
		vi->i_blocks = new_alloc_size >> 9;
	write_unlock_irqrestore(&ni->size_lock, flags);
alloc_done:
	if (new_data_size >= 0) {
		BUG_ON(new_data_size <
				sle64_to_cpu(a->data.non_resident.data_size));
		a->data.non_resident.data_size = cpu_to_sle64(new_data_size);
	}
flush_done:
	
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
done:
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
	up_write(&ni->runlist.lock);
	ntfs_debug("Done, new_allocated_size 0x%llx.",
			(unsigned long long)new_alloc_size);
	return new_alloc_size;
restore_undo_alloc:
	if (start < 0 || start >= allocated_size)
		ntfs_error(vol->sb, "Cannot complete extension of allocation "
				"of inode 0x%lx, attribute type 0x%x, because "
				"lookup of first attribute extent failed with "
				"error code %i.", vi->i_ino,
				(unsigned)le32_to_cpu(ni->type), err);
	if (err == -ENOENT)
		err = -EIO;
	ntfs_attr_reinit_search_ctx(ctx);
	if (ntfs_attr_lookup(ni->type, ni->name, ni->name_len, CASE_SENSITIVE,
			allocated_size >> vol->cluster_size_bits, NULL, 0,
			ctx)) {
		ntfs_error(vol->sb, "Failed to find last attribute extent of "
				"attribute in error code path.  Run chkdsk to "
				"recover.");
		write_lock_irqsave(&ni->size_lock, flags);
		ni->allocated_size = new_alloc_size;
		if (NInoSparse(ni) || NInoCompressed(ni)) {
			ni->itype.compressed.size += new_alloc_size -
					allocated_size;
			vi->i_blocks = ni->itype.compressed.size >> 9;
		} else
			vi->i_blocks = new_alloc_size >> 9;
		write_unlock_irqrestore(&ni->size_lock, flags);
		ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(base_ni);
		up_write(&ni->runlist.lock);
		NVolSetErrors(vol);
		return err;
	}
	ctx->attr->data.non_resident.highest_vcn = cpu_to_sle64(
			(allocated_size >> vol->cluster_size_bits) - 1);
undo_alloc:
	ll = allocated_size >> vol->cluster_size_bits;
	if (ntfs_cluster_free(ni, ll, -1, ctx) < 0) {
		ntfs_error(vol->sb, "Failed to release allocated cluster(s) "
				"in error code path.  Run chkdsk to recover "
				"the lost cluster(s).");
		NVolSetErrors(vol);
	}
	m = ctx->mrec;
	a = ctx->attr;
	if (ntfs_rl_truncate_nolock(vol, &ni->runlist, ll) || IS_ERR(m)) {
		ntfs_error(vol->sb, "Failed to %s in error code path.  Run "
				"chkdsk to recover.", IS_ERR(m) ?
				"restore attribute search context" :
				"truncate attribute runlist");
		NVolSetErrors(vol);
	} else if (mp_rebuilt) {
		if (ntfs_attr_record_resize(m, a, attr_len)) {
			ntfs_error(vol->sb, "Failed to restore attribute "
					"record in error code path.  Run "
					"chkdsk to recover.");
			NVolSetErrors(vol);
		} else  {
			if (ntfs_mapping_pairs_build(vol, (u8*)a + le16_to_cpu(
					a->data.non_resident.
					mapping_pairs_offset), attr_len -
					le16_to_cpu(a->data.non_resident.
					mapping_pairs_offset), rl2, ll, -1,
					NULL)) {
				ntfs_error(vol->sb, "Failed to restore "
						"mapping pairs array in error "
						"code path.  Run chkdsk to "
						"recover.");
				NVolSetErrors(vol);
			}
			flush_dcache_mft_record_page(ctx->ntfs_ino);
			mark_mft_record_dirty(ctx->ntfs_ino);
		}
	}
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(base_ni);
	up_write(&ni->runlist.lock);
conv_err_out:
	ntfs_debug("Failed.  Returning error code %i.", err);
	return err;
}

/**
 * ntfs_attr_set - fill (a part of) an attribute with a byte
 * @ni:		ntfs inode describing the attribute to fill
 * @ofs:	offset inside the attribute at which to start to fill
 * @cnt:	number of bytes to fill
 * @val:	the unsigned 8-bit value with which to fill the attribute
 *
 * Fill @cnt bytes of the attribute described by the ntfs inode @ni starting at
 * byte offset @ofs inside the attribute with the constant byte @val.
 *
 * This function is effectively like memset() applied to an ntfs attribute.
 * Note thie function actually only operates on the page cache pages belonging
 * to the ntfs attribute and it marks them dirty after doing the memset().
 * Thus it relies on the vm dirty page write code paths to cause the modified
 * pages to be written to the mft record/disk.
 *
 * Return 0 on success and -errno on error.  An error code of -ESPIPE means
 * that @ofs + @cnt were outside the end of the attribute and no write was
 * performed.
 */
int ntfs_attr_set(ntfs_inode *ni, const s64 ofs, const s64 cnt, const u8 val)
{
	ntfs_volume *vol = ni->vol;
	struct address_space *mapping;
	struct page *page;
	u8 *kaddr;
	pgoff_t idx, end;
	unsigned start_ofs, end_ofs, size;

	ntfs_debug("Entering for ofs 0x%llx, cnt 0x%llx, val 0x%hx.",
			(long long)ofs, (long long)cnt, val);
	BUG_ON(ofs < 0);
	BUG_ON(cnt < 0);
	if (!cnt)
		goto done;
	BUG_ON(NInoCompressed(ni));
	BUG_ON(NInoEncrypted(ni));
	mapping = VFS_I(ni)->i_mapping;
	
	idx = ofs >> PAGE_CACHE_SHIFT;
	start_ofs = ofs & ~PAGE_CACHE_MASK;
	
	end = ofs + cnt;
	end_ofs = end & ~PAGE_CACHE_MASK;
	
	if (unlikely(end > i_size_read(VFS_I(ni)))) {
		ntfs_error(vol->sb, "Request exceeds end of attribute.");
		return -ESPIPE;
	}
	end >>= PAGE_CACHE_SHIFT;
	
	if (start_ofs) {
		page = read_mapping_page(mapping, idx, NULL);
		if (IS_ERR(page)) {
			ntfs_error(vol->sb, "Failed to read first partial "
					"page (error, index 0x%lx).", idx);
			return PTR_ERR(page);
		}
		size = PAGE_CACHE_SIZE;
		if (idx == end)
			size = end_ofs;
		kaddr = kmap_atomic(page);
		memset(kaddr + start_ofs, val, size - start_ofs);
		flush_dcache_page(page);
		kunmap_atomic(kaddr);
		set_page_dirty(page);
		page_cache_release(page);
		balance_dirty_pages_ratelimited(mapping);
		cond_resched();
		if (idx == end)
			goto done;
		idx++;
	}
	
	for (; idx < end; idx++) {
		
		page = grab_cache_page(mapping, idx);
		if (unlikely(!page)) {
			ntfs_error(vol->sb, "Insufficient memory to grab "
					"page (index 0x%lx).", idx);
			return -ENOMEM;
		}
		kaddr = kmap_atomic(page);
		memset(kaddr, val, PAGE_CACHE_SIZE);
		flush_dcache_page(page);
		kunmap_atomic(kaddr);
		if (page_has_buffers(page)) {
			struct buffer_head *bh, *head;

			bh = head = page_buffers(page);
			do {
				set_buffer_uptodate(bh);
			} while ((bh = bh->b_this_page) != head);
		}
		
		SetPageUptodate(page);
		set_page_dirty(page);
		
		unlock_page(page);
		page_cache_release(page);
		balance_dirty_pages_ratelimited(mapping);
		cond_resched();
	}
	
	if (end_ofs) {
		page = read_mapping_page(mapping, idx, NULL);
		if (IS_ERR(page)) {
			ntfs_error(vol->sb, "Failed to read last partial page "
					"(error, index 0x%lx).", idx);
			return PTR_ERR(page);
		}
		kaddr = kmap_atomic(page);
		memset(kaddr, val, end_ofs);
		flush_dcache_page(page);
		kunmap_atomic(kaddr);
		set_page_dirty(page);
		page_cache_release(page);
		balance_dirty_pages_ratelimited(mapping);
		cond_resched();
	}
done:
	ntfs_debug("Done.");
	return 0;
}

#endif 
