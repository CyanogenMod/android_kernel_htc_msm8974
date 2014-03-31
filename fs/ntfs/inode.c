/**
 * inode.c - NTFS kernel inode handling. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2007 Anton Altaparmakov
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
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/mutex.h>
#include <linux/pagemap.h>
#include <linux/quotaops.h>
#include <linux/slab.h>
#include <linux/log2.h>

#include "aops.h"
#include "attrib.h"
#include "bitmap.h"
#include "dir.h"
#include "debug.h"
#include "inode.h"
#include "lcnalloc.h"
#include "malloc.h"
#include "mft.h"
#include "time.h"
#include "ntfs.h"

int ntfs_test_inode(struct inode *vi, ntfs_attr *na)
{
	ntfs_inode *ni;

	if (vi->i_ino != na->mft_no)
		return 0;
	ni = NTFS_I(vi);
	
	if (likely(!NInoAttr(ni))) {
		
		if (unlikely(na->type != AT_UNUSED))
			return 0;
	} else {
		
		if (ni->type != na->type)
			return 0;
		if (ni->name_len != na->name_len)
			return 0;
		if (na->name_len && memcmp(ni->name, na->name,
				na->name_len * sizeof(ntfschar)))
			return 0;
	}
	
	return 1;
}

static int ntfs_init_locked_inode(struct inode *vi, ntfs_attr *na)
{
	ntfs_inode *ni = NTFS_I(vi);

	vi->i_ino = na->mft_no;

	ni->type = na->type;
	if (na->type == AT_INDEX_ALLOCATION)
		NInoSetMstProtected(ni);

	ni->name = na->name;
	ni->name_len = na->name_len;

	
	if (likely(na->type == AT_UNUSED)) {
		BUG_ON(na->name);
		BUG_ON(na->name_len);
		return 0;
	}

	
	NInoSetAttr(ni);

	if (na->name_len && na->name != I30) {
		unsigned int i;

		BUG_ON(!na->name);
		i = na->name_len * sizeof(ntfschar);
		ni->name = kmalloc(i + sizeof(ntfschar), GFP_ATOMIC);
		if (!ni->name)
			return -ENOMEM;
		memcpy(ni->name, na->name, i);
		ni->name[na->name_len] = 0;
	}
	return 0;
}

typedef int (*set_t)(struct inode *, void *);
static int ntfs_read_locked_inode(struct inode *vi);
static int ntfs_read_locked_attr_inode(struct inode *base_vi, struct inode *vi);
static int ntfs_read_locked_index_inode(struct inode *base_vi,
		struct inode *vi);

struct inode *ntfs_iget(struct super_block *sb, unsigned long mft_no)
{
	struct inode *vi;
	int err;
	ntfs_attr na;

	na.mft_no = mft_no;
	na.type = AT_UNUSED;
	na.name = NULL;
	na.name_len = 0;

	vi = iget5_locked(sb, mft_no, (test_t)ntfs_test_inode,
			(set_t)ntfs_init_locked_inode, &na);
	if (unlikely(!vi))
		return ERR_PTR(-ENOMEM);

	err = 0;

	
	if (vi->i_state & I_NEW) {
		err = ntfs_read_locked_inode(vi);
		unlock_new_inode(vi);
	}
	if (unlikely(err == -ENOMEM)) {
		iput(vi);
		vi = ERR_PTR(err);
	}
	return vi;
}

struct inode *ntfs_attr_iget(struct inode *base_vi, ATTR_TYPE type,
		ntfschar *name, u32 name_len)
{
	struct inode *vi;
	int err;
	ntfs_attr na;

	
	BUG_ON(type == AT_INDEX_ALLOCATION);

	na.mft_no = base_vi->i_ino;
	na.type = type;
	na.name = name;
	na.name_len = name_len;

	vi = iget5_locked(base_vi->i_sb, na.mft_no, (test_t)ntfs_test_inode,
			(set_t)ntfs_init_locked_inode, &na);
	if (unlikely(!vi))
		return ERR_PTR(-ENOMEM);

	err = 0;

	
	if (vi->i_state & I_NEW) {
		err = ntfs_read_locked_attr_inode(base_vi, vi);
		unlock_new_inode(vi);
	}
	if (unlikely(err)) {
		iput(vi);
		vi = ERR_PTR(err);
	}
	return vi;
}

struct inode *ntfs_index_iget(struct inode *base_vi, ntfschar *name,
		u32 name_len)
{
	struct inode *vi;
	int err;
	ntfs_attr na;

	na.mft_no = base_vi->i_ino;
	na.type = AT_INDEX_ALLOCATION;
	na.name = name;
	na.name_len = name_len;

	vi = iget5_locked(base_vi->i_sb, na.mft_no, (test_t)ntfs_test_inode,
			(set_t)ntfs_init_locked_inode, &na);
	if (unlikely(!vi))
		return ERR_PTR(-ENOMEM);

	err = 0;

	
	if (vi->i_state & I_NEW) {
		err = ntfs_read_locked_index_inode(base_vi, vi);
		unlock_new_inode(vi);
	}
	if (unlikely(err)) {
		iput(vi);
		vi = ERR_PTR(err);
	}
	return vi;
}

struct inode *ntfs_alloc_big_inode(struct super_block *sb)
{
	ntfs_inode *ni;

	ntfs_debug("Entering.");
	ni = kmem_cache_alloc(ntfs_big_inode_cache, GFP_NOFS);
	if (likely(ni != NULL)) {
		ni->state = 0;
		return VFS_I(ni);
	}
	ntfs_error(sb, "Allocation of NTFS big inode structure failed.");
	return NULL;
}

static void ntfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(ntfs_big_inode_cache, NTFS_I(inode));
}

void ntfs_destroy_big_inode(struct inode *inode)
{
	ntfs_inode *ni = NTFS_I(inode);

	ntfs_debug("Entering.");
	BUG_ON(ni->page);
	if (!atomic_dec_and_test(&ni->count))
		BUG();
	call_rcu(&inode->i_rcu, ntfs_i_callback);
}

static inline ntfs_inode *ntfs_alloc_extent_inode(void)
{
	ntfs_inode *ni;

	ntfs_debug("Entering.");
	ni = kmem_cache_alloc(ntfs_inode_cache, GFP_NOFS);
	if (likely(ni != NULL)) {
		ni->state = 0;
		return ni;
	}
	ntfs_error(NULL, "Allocation of NTFS inode structure failed.");
	return NULL;
}

static void ntfs_destroy_extent_inode(ntfs_inode *ni)
{
	ntfs_debug("Entering.");
	BUG_ON(ni->page);
	if (!atomic_dec_and_test(&ni->count))
		BUG();
	kmem_cache_free(ntfs_inode_cache, ni);
}

static struct lock_class_key attr_list_rl_lock_class;

void __ntfs_init_inode(struct super_block *sb, ntfs_inode *ni)
{
	ntfs_debug("Entering.");
	rwlock_init(&ni->size_lock);
	ni->initialized_size = ni->allocated_size = 0;
	ni->seq_no = 0;
	atomic_set(&ni->count, 1);
	ni->vol = NTFS_SB(sb);
	ntfs_init_runlist(&ni->runlist);
	mutex_init(&ni->mrec_lock);
	ni->page = NULL;
	ni->page_ofs = 0;
	ni->attr_list_size = 0;
	ni->attr_list = NULL;
	ntfs_init_runlist(&ni->attr_list_rl);
	lockdep_set_class(&ni->attr_list_rl.lock,
				&attr_list_rl_lock_class);
	ni->itype.index.block_size = 0;
	ni->itype.index.vcn_size = 0;
	ni->itype.index.collation_rule = 0;
	ni->itype.index.block_size_bits = 0;
	ni->itype.index.vcn_size_bits = 0;
	mutex_init(&ni->extent_lock);
	ni->nr_extents = 0;
	ni->ext.base_ntfs_ino = NULL;
}

static struct lock_class_key extent_inode_mrec_lock_key;

inline ntfs_inode *ntfs_new_extent_inode(struct super_block *sb,
		unsigned long mft_no)
{
	ntfs_inode *ni = ntfs_alloc_extent_inode();

	ntfs_debug("Entering.");
	if (likely(ni != NULL)) {
		__ntfs_init_inode(sb, ni);
		lockdep_set_class(&ni->mrec_lock, &extent_inode_mrec_lock_key);
		ni->mft_no = mft_no;
		ni->type = AT_UNUSED;
		ni->name = NULL;
		ni->name_len = 0;
	}
	return ni;
}

static int ntfs_is_extended_system_file(ntfs_attr_search_ctx *ctx)
{
	int nr_links, err;

	
	ntfs_attr_reinit_search_ctx(ctx);

	
	nr_links = le16_to_cpu(ctx->mrec->link_count);

	
	while (!(err = ntfs_attr_lookup(AT_FILE_NAME, NULL, 0, 0, 0, NULL, 0,
			ctx))) {
		FILE_NAME_ATTR *file_name_attr;
		ATTR_RECORD *attr = ctx->attr;
		u8 *p, *p2;

		nr_links--;
		p = (u8*)attr + le32_to_cpu(attr->length);
		if (p < (u8*)ctx->mrec || (u8*)p > (u8*)ctx->mrec +
				le32_to_cpu(ctx->mrec->bytes_in_use)) {
err_corrupt_attr:
			ntfs_error(ctx->ntfs_ino->vol->sb, "Corrupt file name "
					"attribute. You should run chkdsk.");
			return -EIO;
		}
		if (attr->non_resident) {
			ntfs_error(ctx->ntfs_ino->vol->sb, "Non-resident file "
					"name. You should run chkdsk.");
			return -EIO;
		}
		if (attr->flags) {
			ntfs_error(ctx->ntfs_ino->vol->sb, "File name with "
					"invalid flags. You should run "
					"chkdsk.");
			return -EIO;
		}
		if (!(attr->data.resident.flags & RESIDENT_ATTR_IS_INDEXED)) {
			ntfs_error(ctx->ntfs_ino->vol->sb, "Unindexed file "
					"name. You should run chkdsk.");
			return -EIO;
		}
		file_name_attr = (FILE_NAME_ATTR*)((u8*)attr +
				le16_to_cpu(attr->data.resident.value_offset));
		p2 = (u8*)attr + le32_to_cpu(attr->data.resident.value_length);
		if (p2 < (u8*)attr || p2 > p)
			goto err_corrupt_attr;
		
		if (MREF_LE(file_name_attr->parent_directory) == FILE_Extend)
			return 1;	
	}
	if (unlikely(err != -ENOENT))
		return err;
	if (unlikely(nr_links)) {
		ntfs_error(ctx->ntfs_ino->vol->sb, "Inode hard link count "
				"doesn't match number of name attributes. You "
				"should run chkdsk.");
		return -EIO;
	}
	return 0;	
}

static int ntfs_read_locked_inode(struct inode *vi)
{
	ntfs_volume *vol = NTFS_SB(vi->i_sb);
	ntfs_inode *ni;
	struct inode *bvi;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	STANDARD_INFORMATION *si;
	ntfs_attr_search_ctx *ctx;
	int err = 0;

	ntfs_debug("Entering for i_ino 0x%lx.", vi->i_ino);

	

	vi->i_version = 1;

	vi->i_uid = vol->uid;
	vi->i_gid = vol->gid;
	vi->i_mode = 0;

	if (vi->i_ino != FILE_MFT)
		ntfs_init_big_inode(vi);
	ni = NTFS_I(vi);

	m = map_mft_record(ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		goto err_out;
	}
	ctx = ntfs_attr_get_search_ctx(ni, m);
	if (!ctx) {
		err = -ENOMEM;
		goto unm_err_out;
	}

	if (!(m->flags & MFT_RECORD_IN_USE)) {
		ntfs_error(vi->i_sb, "Inode is not in use!");
		goto unm_err_out;
	}
	if (m->base_mft_record) {
		ntfs_error(vi->i_sb, "Inode is an extent inode!");
		goto unm_err_out;
	}

	
	vi->i_generation = ni->seq_no = le16_to_cpu(m->sequence_number);

	set_nlink(vi, le16_to_cpu(m->link_count));
	
	vi->i_mode |= S_IRWXUGO;
	
	if (IS_RDONLY(vi))
		vi->i_mode &= ~S_IWUGO;
	if (m->flags & MFT_RECORD_IS_DIRECTORY) {
		vi->i_mode |= S_IFDIR;
		vi->i_mode &= ~vol->dmask;
		
		if (vi->i_nlink > 1)
			set_nlink(vi, 1);
	} else {
		vi->i_mode |= S_IFREG;
		
		vi->i_mode &= ~vol->fmask;
	}
	err = ntfs_attr_lookup(AT_STANDARD_INFORMATION, NULL, 0, 0, 0, NULL, 0,
			ctx);
	if (unlikely(err)) {
		if (err == -ENOENT) {
			ntfs_error(vi->i_sb, "$STANDARD_INFORMATION attribute "
					"is missing.");
		}
		goto unm_err_out;
	}
	a = ctx->attr;
	
	si = (STANDARD_INFORMATION*)((u8*)a +
			le16_to_cpu(a->data.resident.value_offset));

	
	vi->i_mtime = ntfs2utc(si->last_data_change_time);
	vi->i_ctime = ntfs2utc(si->last_mft_change_time);
	/*
	 * Last access to the data within the file. Not changed during a rename
	 * for example but changed whenever the file is written to.
	 */
	vi->i_atime = ntfs2utc(si->last_access_time);

	
	ntfs_attr_reinit_search_ctx(ctx);
	err = ntfs_attr_lookup(AT_ATTRIBUTE_LIST, NULL, 0, 0, 0, NULL, 0, ctx);
	if (err) {
		if (unlikely(err != -ENOENT)) {
			ntfs_error(vi->i_sb, "Failed to lookup attribute list "
					"attribute.");
			goto unm_err_out;
		}
	} else  {
		if (vi->i_ino == FILE_MFT)
			goto skip_attr_list_load;
		ntfs_debug("Attribute list found in inode 0x%lx.", vi->i_ino);
		NInoSetAttrList(ni);
		a = ctx->attr;
		if (a->flags & ATTR_COMPRESSION_MASK) {
			ntfs_error(vi->i_sb, "Attribute list attribute is "
					"compressed.");
			goto unm_err_out;
		}
		if (a->flags & ATTR_IS_ENCRYPTED ||
				a->flags & ATTR_IS_SPARSE) {
			if (a->non_resident) {
				ntfs_error(vi->i_sb, "Non-resident attribute "
						"list attribute is encrypted/"
						"sparse.");
				goto unm_err_out;
			}
			ntfs_warning(vi->i_sb, "Resident attribute list "
					"attribute in inode 0x%lx is marked "
					"encrypted/sparse which is not true.  "
					"However, Windows allows this and "
					"chkdsk does not detect or correct it "
					"so we will just ignore the invalid "
					"flags and pretend they are not set.",
					vi->i_ino);
		}
		
		ni->attr_list_size = (u32)ntfs_attr_size(a);
		ni->attr_list = ntfs_malloc_nofs(ni->attr_list_size);
		if (!ni->attr_list) {
			ntfs_error(vi->i_sb, "Not enough memory to allocate "
					"buffer for attribute list.");
			err = -ENOMEM;
			goto unm_err_out;
		}
		if (a->non_resident) {
			NInoSetAttrListNonResident(ni);
			if (a->data.non_resident.lowest_vcn) {
				ntfs_error(vi->i_sb, "Attribute list has non "
						"zero lowest_vcn.");
				goto unm_err_out;
			}
			ni->attr_list_rl.rl = ntfs_mapping_pairs_decompress(vol,
					a, NULL);
			if (IS_ERR(ni->attr_list_rl.rl)) {
				err = PTR_ERR(ni->attr_list_rl.rl);
				ni->attr_list_rl.rl = NULL;
				ntfs_error(vi->i_sb, "Mapping pairs "
						"decompression failed.");
				goto unm_err_out;
			}
			
			if ((err = load_attribute_list(vol, &ni->attr_list_rl,
					ni->attr_list, ni->attr_list_size,
					sle64_to_cpu(a->data.non_resident.
					initialized_size)))) {
				ntfs_error(vi->i_sb, "Failed to load "
						"attribute list attribute.");
				goto unm_err_out;
			}
		} else  {
			if ((u8*)a + le16_to_cpu(a->data.resident.value_offset)
					+ le32_to_cpu(
					a->data.resident.value_length) >
					(u8*)ctx->mrec + vol->mft_record_size) {
				ntfs_error(vi->i_sb, "Corrupt attribute list "
						"in inode.");
				goto unm_err_out;
			}
			
			memcpy(ni->attr_list, (u8*)a + le16_to_cpu(
					a->data.resident.value_offset),
					le32_to_cpu(
					a->data.resident.value_length));
		}
	}
skip_attr_list_load:
	if (S_ISDIR(vi->i_mode)) {
		loff_t bvi_size;
		ntfs_inode *bni;
		INDEX_ROOT *ir;
		u8 *ir_end, *index_end;

		
		ntfs_attr_reinit_search_ctx(ctx);
		err = ntfs_attr_lookup(AT_INDEX_ROOT, I30, 4, CASE_SENSITIVE,
				0, NULL, 0, ctx);
		if (unlikely(err)) {
			if (err == -ENOENT) {
				
				
				
				ntfs_error(vi->i_sb, "$INDEX_ROOT attribute "
						"is missing.");
			}
			goto unm_err_out;
		}
		a = ctx->attr;
		
		if (unlikely(a->non_resident)) {
			ntfs_error(vol->sb, "$INDEX_ROOT attribute is not "
					"resident.");
			goto unm_err_out;
		}
		
		if (unlikely(a->name_length && (le16_to_cpu(a->name_offset) >=
				le16_to_cpu(a->data.resident.value_offset)))) {
			ntfs_error(vol->sb, "$INDEX_ROOT attribute name is "
					"placed after the attribute value.");
			goto unm_err_out;
		}
		if (a->flags & ATTR_COMPRESSION_MASK)
			NInoSetCompressed(ni);
		if (a->flags & ATTR_IS_ENCRYPTED) {
			if (a->flags & ATTR_COMPRESSION_MASK) {
				ntfs_error(vi->i_sb, "Found encrypted and "
						"compressed attribute.");
				goto unm_err_out;
			}
			NInoSetEncrypted(ni);
		}
		if (a->flags & ATTR_IS_SPARSE)
			NInoSetSparse(ni);
		ir = (INDEX_ROOT*)((u8*)a +
				le16_to_cpu(a->data.resident.value_offset));
		ir_end = (u8*)ir + le32_to_cpu(a->data.resident.value_length);
		if (ir_end > (u8*)ctx->mrec + vol->mft_record_size) {
			ntfs_error(vi->i_sb, "$INDEX_ROOT attribute is "
					"corrupt.");
			goto unm_err_out;
		}
		index_end = (u8*)&ir->index +
				le32_to_cpu(ir->index.index_length);
		if (index_end > ir_end) {
			ntfs_error(vi->i_sb, "Directory index is corrupt.");
			goto unm_err_out;
		}
		if (ir->type != AT_FILE_NAME) {
			ntfs_error(vi->i_sb, "Indexed attribute is not "
					"$FILE_NAME.");
			goto unm_err_out;
		}
		if (ir->collation_rule != COLLATION_FILE_NAME) {
			ntfs_error(vi->i_sb, "Index collation rule is not "
					"COLLATION_FILE_NAME.");
			goto unm_err_out;
		}
		ni->itype.index.collation_rule = ir->collation_rule;
		ni->itype.index.block_size = le32_to_cpu(ir->index_block_size);
		if (ni->itype.index.block_size &
				(ni->itype.index.block_size - 1)) {
			ntfs_error(vi->i_sb, "Index block size (%u) is not a "
					"power of two.",
					ni->itype.index.block_size);
			goto unm_err_out;
		}
		if (ni->itype.index.block_size > PAGE_CACHE_SIZE) {
			ntfs_error(vi->i_sb, "Index block size (%u) > "
					"PAGE_CACHE_SIZE (%ld) is not "
					"supported.  Sorry.",
					ni->itype.index.block_size,
					PAGE_CACHE_SIZE);
			err = -EOPNOTSUPP;
			goto unm_err_out;
		}
		if (ni->itype.index.block_size < NTFS_BLOCK_SIZE) {
			ntfs_error(vi->i_sb, "Index block size (%u) < "
					"NTFS_BLOCK_SIZE (%i) is not "
					"supported.  Sorry.",
					ni->itype.index.block_size,
					NTFS_BLOCK_SIZE);
			err = -EOPNOTSUPP;
			goto unm_err_out;
		}
		ni->itype.index.block_size_bits =
				ffs(ni->itype.index.block_size) - 1;
		
		if (vol->cluster_size <= ni->itype.index.block_size) {
			ni->itype.index.vcn_size = vol->cluster_size;
			ni->itype.index.vcn_size_bits = vol->cluster_size_bits;
		} else {
			ni->itype.index.vcn_size = vol->sector_size;
			ni->itype.index.vcn_size_bits = vol->sector_size_bits;
		}

		
		NInoSetMstProtected(ni);
		ni->type = AT_INDEX_ALLOCATION;
		ni->name = I30;
		ni->name_len = 4;

		if (!(ir->index.flags & LARGE_INDEX)) {
			
			vi->i_size = ni->initialized_size =
					ni->allocated_size = 0;
			
			ntfs_attr_put_search_ctx(ctx);
			unmap_mft_record(ni);
			m = NULL;
			ctx = NULL;
			goto skip_large_dir_stuff;
		} 
		NInoSetIndexAllocPresent(ni);
		
		ntfs_attr_reinit_search_ctx(ctx);
		err = ntfs_attr_lookup(AT_INDEX_ALLOCATION, I30, 4,
				CASE_SENSITIVE, 0, NULL, 0, ctx);
		if (unlikely(err)) {
			if (err == -ENOENT)
				ntfs_error(vi->i_sb, "$INDEX_ALLOCATION "
						"attribute is not present but "
						"$INDEX_ROOT indicated it is.");
			else
				ntfs_error(vi->i_sb, "Failed to lookup "
						"$INDEX_ALLOCATION "
						"attribute.");
			goto unm_err_out;
		}
		a = ctx->attr;
		if (!a->non_resident) {
			ntfs_error(vi->i_sb, "$INDEX_ALLOCATION attribute "
					"is resident.");
			goto unm_err_out;
		}
		if (unlikely(a->name_length && (le16_to_cpu(a->name_offset) >=
				le16_to_cpu(
				a->data.non_resident.mapping_pairs_offset)))) {
			ntfs_error(vol->sb, "$INDEX_ALLOCATION attribute name "
					"is placed after the mapping pairs "
					"array.");
			goto unm_err_out;
		}
		if (a->flags & ATTR_IS_ENCRYPTED) {
			ntfs_error(vi->i_sb, "$INDEX_ALLOCATION attribute "
					"is encrypted.");
			goto unm_err_out;
		}
		if (a->flags & ATTR_IS_SPARSE) {
			ntfs_error(vi->i_sb, "$INDEX_ALLOCATION attribute "
					"is sparse.");
			goto unm_err_out;
		}
		if (a->flags & ATTR_COMPRESSION_MASK) {
			ntfs_error(vi->i_sb, "$INDEX_ALLOCATION attribute "
					"is compressed.");
			goto unm_err_out;
		}
		if (a->data.non_resident.lowest_vcn) {
			ntfs_error(vi->i_sb, "First extent of "
					"$INDEX_ALLOCATION attribute has non "
					"zero lowest_vcn.");
			goto unm_err_out;
		}
		vi->i_size = sle64_to_cpu(a->data.non_resident.data_size);
		ni->initialized_size = sle64_to_cpu(
				a->data.non_resident.initialized_size);
		ni->allocated_size = sle64_to_cpu(
				a->data.non_resident.allocated_size);
		ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(ni);
		m = NULL;
		ctx = NULL;
		
		bvi = ntfs_attr_iget(vi, AT_BITMAP, I30, 4);
		if (IS_ERR(bvi)) {
			ntfs_error(vi->i_sb, "Failed to get bitmap attribute.");
			err = PTR_ERR(bvi);
			goto unm_err_out;
		}
		bni = NTFS_I(bvi);
		if (NInoCompressed(bni) || NInoEncrypted(bni) ||
				NInoSparse(bni)) {
			ntfs_error(vi->i_sb, "$BITMAP attribute is compressed "
					"and/or encrypted and/or sparse.");
			goto iput_unm_err_out;
		}
		
		bvi_size = i_size_read(bvi);
		if ((bvi_size << 3) < (vi->i_size >>
				ni->itype.index.block_size_bits)) {
			ntfs_error(vi->i_sb, "Index bitmap too small (0x%llx) "
					"for index allocation (0x%llx).",
					bvi_size << 3, vi->i_size);
			goto iput_unm_err_out;
		}
		
		iput(bvi);
skip_large_dir_stuff:
		
		vi->i_op = &ntfs_dir_inode_ops;
		vi->i_fop = &ntfs_dir_ops;
	} else {
		
		ntfs_attr_reinit_search_ctx(ctx);

		
		ni->type = AT_DATA;
		ni->name = NULL;
		ni->name_len = 0;

		
		err = ntfs_attr_lookup(AT_DATA, NULL, 0, 0, 0, NULL, 0, ctx);
		if (unlikely(err)) {
			vi->i_size = ni->initialized_size =
					ni->allocated_size = 0;
			if (err != -ENOENT) {
				ntfs_error(vi->i_sb, "Failed to lookup $DATA "
						"attribute.");
				goto unm_err_out;
			}
			if (vi->i_ino == FILE_Secure)
				goto no_data_attr_special_case;
			if (ntfs_is_extended_system_file(ctx) > 0)
				goto no_data_attr_special_case;
			
			
			ntfs_error(vi->i_sb, "$DATA attribute is missing.");
			goto unm_err_out;
		}
		a = ctx->attr;
		
		if (a->flags & (ATTR_COMPRESSION_MASK | ATTR_IS_SPARSE)) {
			if (a->flags & ATTR_COMPRESSION_MASK) {
				NInoSetCompressed(ni);
				if (vol->cluster_size > 4096) {
					ntfs_error(vi->i_sb, "Found "
							"compressed data but "
							"compression is "
							"disabled due to "
							"cluster size (%i) > "
							"4kiB.",
							vol->cluster_size);
					goto unm_err_out;
				}
				if ((a->flags & ATTR_COMPRESSION_MASK)
						!= ATTR_IS_COMPRESSED) {
					ntfs_error(vi->i_sb, "Found unknown "
							"compression method "
							"or corrupt file.");
					goto unm_err_out;
				}
			}
			if (a->flags & ATTR_IS_SPARSE)
				NInoSetSparse(ni);
		}
		if (a->flags & ATTR_IS_ENCRYPTED) {
			if (NInoCompressed(ni)) {
				ntfs_error(vi->i_sb, "Found encrypted and "
						"compressed data.");
				goto unm_err_out;
			}
			NInoSetEncrypted(ni);
		}
		if (a->non_resident) {
			NInoSetNonResident(ni);
			if (NInoCompressed(ni) || NInoSparse(ni)) {
				if (NInoCompressed(ni) && a->data.non_resident.
						compression_unit != 4) {
					ntfs_error(vi->i_sb, "Found "
							"non-standard "
							"compression unit (%u "
							"instead of 4).  "
							"Cannot handle this.",
							a->data.non_resident.
							compression_unit);
					err = -EOPNOTSUPP;
					goto unm_err_out;
				}
				if (a->data.non_resident.compression_unit) {
					ni->itype.compressed.block_size = 1U <<
							(a->data.non_resident.
							compression_unit +
							vol->cluster_size_bits);
					ni->itype.compressed.block_size_bits =
							ffs(ni->itype.
							compressed.
							block_size) - 1;
					ni->itype.compressed.block_clusters =
							1U << a->data.
							non_resident.
							compression_unit;
				} else {
					ni->itype.compressed.block_size = 0;
					ni->itype.compressed.block_size_bits =
							0;
					ni->itype.compressed.block_clusters =
							0;
				}
				ni->itype.compressed.size = sle64_to_cpu(
						a->data.non_resident.
						compressed_size);
			}
			if (a->data.non_resident.lowest_vcn) {
				ntfs_error(vi->i_sb, "First extent of $DATA "
						"attribute has non zero "
						"lowest_vcn.");
				goto unm_err_out;
			}
			vi->i_size = sle64_to_cpu(
					a->data.non_resident.data_size);
			ni->initialized_size = sle64_to_cpu(
					a->data.non_resident.initialized_size);
			ni->allocated_size = sle64_to_cpu(
					a->data.non_resident.allocated_size);
		} else { 
			vi->i_size = ni->initialized_size = le32_to_cpu(
					a->data.resident.value_length);
			ni->allocated_size = le32_to_cpu(a->length) -
					le16_to_cpu(
					a->data.resident.value_offset);
			if (vi->i_size > ni->allocated_size) {
				ntfs_error(vi->i_sb, "Resident data attribute "
						"is corrupt (size exceeds "
						"allocation).");
				goto unm_err_out;
			}
		}
no_data_attr_special_case:
		
		ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(ni);
		m = NULL;
		ctx = NULL;
		
		vi->i_op = &ntfs_file_inode_ops;
		vi->i_fop = &ntfs_file_ops;
	}
	if (NInoMstProtected(ni))
		vi->i_mapping->a_ops = &ntfs_mst_aops;
	else
		vi->i_mapping->a_ops = &ntfs_aops;
	if (S_ISREG(vi->i_mode) && (NInoCompressed(ni) || NInoSparse(ni)))
		vi->i_blocks = ni->itype.compressed.size >> 9;
	else
		vi->i_blocks = ni->allocated_size >> 9;
	ntfs_debug("Done.");
	return 0;
iput_unm_err_out:
	iput(bvi);
unm_err_out:
	if (!err)
		err = -EIO;
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(ni);
err_out:
	ntfs_error(vol->sb, "Failed with error code %i.  Marking corrupt "
			"inode 0x%lx as bad.  Run chkdsk.", err, vi->i_ino);
	make_bad_inode(vi);
	if (err != -EOPNOTSUPP && err != -ENOMEM)
		NVolSetErrors(vol);
	return err;
}

static int ntfs_read_locked_attr_inode(struct inode *base_vi, struct inode *vi)
{
	ntfs_volume *vol = NTFS_SB(vi->i_sb);
	ntfs_inode *ni, *base_ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *ctx;
	int err = 0;

	ntfs_debug("Entering for i_ino 0x%lx.", vi->i_ino);

	ntfs_init_big_inode(vi);

	ni	= NTFS_I(vi);
	base_ni = NTFS_I(base_vi);

	
	vi->i_version	= base_vi->i_version;
	vi->i_uid	= base_vi->i_uid;
	vi->i_gid	= base_vi->i_gid;
	set_nlink(vi, base_vi->i_nlink);
	vi->i_mtime	= base_vi->i_mtime;
	vi->i_ctime	= base_vi->i_ctime;
	vi->i_atime	= base_vi->i_atime;
	vi->i_generation = ni->seq_no = base_ni->seq_no;

	
	vi->i_mode	= base_vi->i_mode & ~S_IFMT;

	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		goto err_out;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (!ctx) {
		err = -ENOMEM;
		goto unm_err_out;
	}
	
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err))
		goto unm_err_out;
	a = ctx->attr;
	if (a->flags & (ATTR_COMPRESSION_MASK | ATTR_IS_SPARSE)) {
		if (a->flags & ATTR_COMPRESSION_MASK) {
			NInoSetCompressed(ni);
			if ((ni->type != AT_DATA) || (ni->type == AT_DATA &&
					ni->name_len)) {
				ntfs_error(vi->i_sb, "Found compressed "
						"non-data or named data "
						"attribute.  Please report "
						"you saw this message to "
						"linux-ntfs-dev@lists."
						"sourceforge.net");
				goto unm_err_out;
			}
			if (vol->cluster_size > 4096) {
				ntfs_error(vi->i_sb, "Found compressed "
						"attribute but compression is "
						"disabled due to cluster size "
						"(%i) > 4kiB.",
						vol->cluster_size);
				goto unm_err_out;
			}
			if ((a->flags & ATTR_COMPRESSION_MASK) !=
					ATTR_IS_COMPRESSED) {
				ntfs_error(vi->i_sb, "Found unknown "
						"compression method.");
				goto unm_err_out;
			}
		}
		if (NInoMstProtected(ni) && ni->type != AT_INDEX_ROOT) {
			ntfs_error(vi->i_sb, "Found mst protected attribute "
					"but the attribute is %s.  Please "
					"report you saw this message to "
					"linux-ntfs-dev@lists.sourceforge.net",
					NInoCompressed(ni) ? "compressed" :
					"sparse");
			goto unm_err_out;
		}
		if (a->flags & ATTR_IS_SPARSE)
			NInoSetSparse(ni);
	}
	if (a->flags & ATTR_IS_ENCRYPTED) {
		if (NInoCompressed(ni)) {
			ntfs_error(vi->i_sb, "Found encrypted and compressed "
					"data.");
			goto unm_err_out;
		}
		if (NInoMstProtected(ni) && ni->type != AT_INDEX_ROOT) {
			ntfs_error(vi->i_sb, "Found mst protected attribute "
					"but the attribute is encrypted.  "
					"Please report you saw this message "
					"to linux-ntfs-dev@lists.sourceforge."
					"net");
			goto unm_err_out;
		}
		if (ni->type != AT_DATA) {
			ntfs_error(vi->i_sb, "Found encrypted non-data "
					"attribute.");
			goto unm_err_out;
		}
		NInoSetEncrypted(ni);
	}
	if (!a->non_resident) {
		
		if (unlikely(a->name_length && (le16_to_cpu(a->name_offset) >=
				le16_to_cpu(a->data.resident.value_offset)))) {
			ntfs_error(vol->sb, "Attribute name is placed after "
					"the attribute value.");
			goto unm_err_out;
		}
		if (NInoMstProtected(ni)) {
			ntfs_error(vi->i_sb, "Found mst protected attribute "
					"but the attribute is resident.  "
					"Please report you saw this message to "
					"linux-ntfs-dev@lists.sourceforge.net");
			goto unm_err_out;
		}
		vi->i_size = ni->initialized_size = le32_to_cpu(
				a->data.resident.value_length);
		ni->allocated_size = le32_to_cpu(a->length) -
				le16_to_cpu(a->data.resident.value_offset);
		if (vi->i_size > ni->allocated_size) {
			ntfs_error(vi->i_sb, "Resident attribute is corrupt "
					"(size exceeds allocation).");
			goto unm_err_out;
		}
	} else {
		NInoSetNonResident(ni);
		if (unlikely(a->name_length && (le16_to_cpu(a->name_offset) >=
				le16_to_cpu(
				a->data.non_resident.mapping_pairs_offset)))) {
			ntfs_error(vol->sb, "Attribute name is placed after "
					"the mapping pairs array.");
			goto unm_err_out;
		}
		if (NInoCompressed(ni) || NInoSparse(ni)) {
			if (NInoCompressed(ni) && a->data.non_resident.
					compression_unit != 4) {
				ntfs_error(vi->i_sb, "Found non-standard "
						"compression unit (%u instead "
						"of 4).  Cannot handle this.",
						a->data.non_resident.
						compression_unit);
				err = -EOPNOTSUPP;
				goto unm_err_out;
			}
			if (a->data.non_resident.compression_unit) {
				ni->itype.compressed.block_size = 1U <<
						(a->data.non_resident.
						compression_unit +
						vol->cluster_size_bits);
				ni->itype.compressed.block_size_bits =
						ffs(ni->itype.compressed.
						block_size) - 1;
				ni->itype.compressed.block_clusters = 1U <<
						a->data.non_resident.
						compression_unit;
			} else {
				ni->itype.compressed.block_size = 0;
				ni->itype.compressed.block_size_bits = 0;
				ni->itype.compressed.block_clusters = 0;
			}
			ni->itype.compressed.size = sle64_to_cpu(
					a->data.non_resident.compressed_size);
		}
		if (a->data.non_resident.lowest_vcn) {
			ntfs_error(vi->i_sb, "First extent of attribute has "
					"non-zero lowest_vcn.");
			goto unm_err_out;
		}
		vi->i_size = sle64_to_cpu(a->data.non_resident.data_size);
		ni->initialized_size = sle64_to_cpu(
				a->data.non_resident.initialized_size);
		ni->allocated_size = sle64_to_cpu(
				a->data.non_resident.allocated_size);
	}
	if (NInoMstProtected(ni))
		vi->i_mapping->a_ops = &ntfs_mst_aops;
	else
		vi->i_mapping->a_ops = &ntfs_aops;
	if ((NInoCompressed(ni) || NInoSparse(ni)) && ni->type != AT_INDEX_ROOT)
		vi->i_blocks = ni->itype.compressed.size >> 9;
	else
		vi->i_blocks = ni->allocated_size >> 9;
	igrab(base_vi);
	ni->ext.base_ntfs_ino = base_ni;
	ni->nr_extents = -1;

	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);

	ntfs_debug("Done.");
	return 0;

unm_err_out:
	if (!err)
		err = -EIO;
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
err_out:
	ntfs_error(vol->sb, "Failed with error code %i while reading attribute "
			"inode (mft_no 0x%lx, type 0x%x, name_len %i).  "
			"Marking corrupt inode and base inode 0x%lx as bad.  "
			"Run chkdsk.", err, vi->i_ino, ni->type, ni->name_len,
			base_vi->i_ino);
	make_bad_inode(vi);
	if (err != -ENOMEM)
		NVolSetErrors(vol);
	return err;
}

static int ntfs_read_locked_index_inode(struct inode *base_vi, struct inode *vi)
{
	loff_t bvi_size;
	ntfs_volume *vol = NTFS_SB(vi->i_sb);
	ntfs_inode *ni, *base_ni, *bni;
	struct inode *bvi;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *ctx;
	INDEX_ROOT *ir;
	u8 *ir_end, *index_end;
	int err = 0;

	ntfs_debug("Entering for i_ino 0x%lx.", vi->i_ino);
	ntfs_init_big_inode(vi);
	ni	= NTFS_I(vi);
	base_ni = NTFS_I(base_vi);
	
	vi->i_version	= base_vi->i_version;
	vi->i_uid	= base_vi->i_uid;
	vi->i_gid	= base_vi->i_gid;
	set_nlink(vi, base_vi->i_nlink);
	vi->i_mtime	= base_vi->i_mtime;
	vi->i_ctime	= base_vi->i_ctime;
	vi->i_atime	= base_vi->i_atime;
	vi->i_generation = ni->seq_no = base_ni->seq_no;
	
	vi->i_mode	= base_vi->i_mode & ~S_IFMT;
	
	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		goto err_out;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (!ctx) {
		err = -ENOMEM;
		goto unm_err_out;
	}
	
	err = ntfs_attr_lookup(AT_INDEX_ROOT, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT)
			ntfs_error(vi->i_sb, "$INDEX_ROOT attribute is "
					"missing.");
		goto unm_err_out;
	}
	a = ctx->attr;
	
	if (unlikely(a->non_resident)) {
		ntfs_error(vol->sb, "$INDEX_ROOT attribute is not resident.");
		goto unm_err_out;
	}
	
	if (unlikely(a->name_length && (le16_to_cpu(a->name_offset) >=
			le16_to_cpu(a->data.resident.value_offset)))) {
		ntfs_error(vol->sb, "$INDEX_ROOT attribute name is placed "
				"after the attribute value.");
		goto unm_err_out;
	}
	if (a->flags & (ATTR_COMPRESSION_MASK | ATTR_IS_ENCRYPTED |
			ATTR_IS_SPARSE)) {
		ntfs_error(vi->i_sb, "Found compressed/encrypted/sparse index "
				"root attribute.");
		goto unm_err_out;
	}
	ir = (INDEX_ROOT*)((u8*)a + le16_to_cpu(a->data.resident.value_offset));
	ir_end = (u8*)ir + le32_to_cpu(a->data.resident.value_length);
	if (ir_end > (u8*)ctx->mrec + vol->mft_record_size) {
		ntfs_error(vi->i_sb, "$INDEX_ROOT attribute is corrupt.");
		goto unm_err_out;
	}
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	if (index_end > ir_end) {
		ntfs_error(vi->i_sb, "Index is corrupt.");
		goto unm_err_out;
	}
	if (ir->type) {
		ntfs_error(vi->i_sb, "Index type is not 0 (type is 0x%x).",
				le32_to_cpu(ir->type));
		goto unm_err_out;
	}
	ni->itype.index.collation_rule = ir->collation_rule;
	ntfs_debug("Index collation rule is 0x%x.",
			le32_to_cpu(ir->collation_rule));
	ni->itype.index.block_size = le32_to_cpu(ir->index_block_size);
	if (!is_power_of_2(ni->itype.index.block_size)) {
		ntfs_error(vi->i_sb, "Index block size (%u) is not a power of "
				"two.", ni->itype.index.block_size);
		goto unm_err_out;
	}
	if (ni->itype.index.block_size > PAGE_CACHE_SIZE) {
		ntfs_error(vi->i_sb, "Index block size (%u) > PAGE_CACHE_SIZE "
				"(%ld) is not supported.  Sorry.",
				ni->itype.index.block_size, PAGE_CACHE_SIZE);
		err = -EOPNOTSUPP;
		goto unm_err_out;
	}
	if (ni->itype.index.block_size < NTFS_BLOCK_SIZE) {
		ntfs_error(vi->i_sb, "Index block size (%u) < NTFS_BLOCK_SIZE "
				"(%i) is not supported.  Sorry.",
				ni->itype.index.block_size, NTFS_BLOCK_SIZE);
		err = -EOPNOTSUPP;
		goto unm_err_out;
	}
	ni->itype.index.block_size_bits = ffs(ni->itype.index.block_size) - 1;
	
	if (vol->cluster_size <= ni->itype.index.block_size) {
		ni->itype.index.vcn_size = vol->cluster_size;
		ni->itype.index.vcn_size_bits = vol->cluster_size_bits;
	} else {
		ni->itype.index.vcn_size = vol->sector_size;
		ni->itype.index.vcn_size_bits = vol->sector_size_bits;
	}
	
	if (!(ir->index.flags & LARGE_INDEX)) {
		
		vi->i_size = ni->initialized_size = ni->allocated_size = 0;
		
		ntfs_attr_put_search_ctx(ctx);
		unmap_mft_record(base_ni);
		m = NULL;
		ctx = NULL;
		goto skip_large_index_stuff;
	} 
	NInoSetIndexAllocPresent(ni);
	
	ntfs_attr_reinit_search_ctx(ctx);
	err = ntfs_attr_lookup(AT_INDEX_ALLOCATION, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT)
			ntfs_error(vi->i_sb, "$INDEX_ALLOCATION attribute is "
					"not present but $INDEX_ROOT "
					"indicated it is.");
		else
			ntfs_error(vi->i_sb, "Failed to lookup "
					"$INDEX_ALLOCATION attribute.");
		goto unm_err_out;
	}
	a = ctx->attr;
	if (!a->non_resident) {
		ntfs_error(vi->i_sb, "$INDEX_ALLOCATION attribute is "
				"resident.");
		goto unm_err_out;
	}
	if (unlikely(a->name_length && (le16_to_cpu(a->name_offset) >=
			le16_to_cpu(
			a->data.non_resident.mapping_pairs_offset)))) {
		ntfs_error(vol->sb, "$INDEX_ALLOCATION attribute name is "
				"placed after the mapping pairs array.");
		goto unm_err_out;
	}
	if (a->flags & ATTR_IS_ENCRYPTED) {
		ntfs_error(vi->i_sb, "$INDEX_ALLOCATION attribute is "
				"encrypted.");
		goto unm_err_out;
	}
	if (a->flags & ATTR_IS_SPARSE) {
		ntfs_error(vi->i_sb, "$INDEX_ALLOCATION attribute is sparse.");
		goto unm_err_out;
	}
	if (a->flags & ATTR_COMPRESSION_MASK) {
		ntfs_error(vi->i_sb, "$INDEX_ALLOCATION attribute is "
				"compressed.");
		goto unm_err_out;
	}
	if (a->data.non_resident.lowest_vcn) {
		ntfs_error(vi->i_sb, "First extent of $INDEX_ALLOCATION "
				"attribute has non zero lowest_vcn.");
		goto unm_err_out;
	}
	vi->i_size = sle64_to_cpu(a->data.non_resident.data_size);
	ni->initialized_size = sle64_to_cpu(
			a->data.non_resident.initialized_size);
	ni->allocated_size = sle64_to_cpu(a->data.non_resident.allocated_size);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
	m = NULL;
	ctx = NULL;
	
	bvi = ntfs_attr_iget(base_vi, AT_BITMAP, ni->name, ni->name_len);
	if (IS_ERR(bvi)) {
		ntfs_error(vi->i_sb, "Failed to get bitmap attribute.");
		err = PTR_ERR(bvi);
		goto unm_err_out;
	}
	bni = NTFS_I(bvi);
	if (NInoCompressed(bni) || NInoEncrypted(bni) ||
			NInoSparse(bni)) {
		ntfs_error(vi->i_sb, "$BITMAP attribute is compressed and/or "
				"encrypted and/or sparse.");
		goto iput_unm_err_out;
	}
	
	bvi_size = i_size_read(bvi);
	if ((bvi_size << 3) < (vi->i_size >> ni->itype.index.block_size_bits)) {
		ntfs_error(vi->i_sb, "Index bitmap too small (0x%llx) for "
				"index allocation (0x%llx).", bvi_size << 3,
				vi->i_size);
		goto iput_unm_err_out;
	}
	iput(bvi);
skip_large_index_stuff:
	
	vi->i_op = NULL;
	vi->i_fop = NULL;
	vi->i_mapping->a_ops = &ntfs_mst_aops;
	vi->i_blocks = ni->allocated_size >> 9;
	igrab(base_vi);
	ni->ext.base_ntfs_ino = base_ni;
	ni->nr_extents = -1;

	ntfs_debug("Done.");
	return 0;
iput_unm_err_out:
	iput(bvi);
unm_err_out:
	if (!err)
		err = -EIO;
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(base_ni);
err_out:
	ntfs_error(vi->i_sb, "Failed with error code %i while reading index "
			"inode (mft_no 0x%lx, name_len %i.", err, vi->i_ino,
			ni->name_len);
	make_bad_inode(vi);
	if (err != -EOPNOTSUPP && err != -ENOMEM)
		NVolSetErrors(vol);
	return err;
}

static struct lock_class_key mft_ni_runlist_lock_key, mft_ni_mrec_lock_key;

int ntfs_read_inode_mount(struct inode *vi)
{
	VCN next_vcn, last_vcn, highest_vcn;
	s64 block;
	struct super_block *sb = vi->i_sb;
	ntfs_volume *vol = NTFS_SB(sb);
	struct buffer_head *bh;
	ntfs_inode *ni;
	MFT_RECORD *m = NULL;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *ctx;
	unsigned int i, nr_blocks;
	int err;

	ntfs_debug("Entering.");

	
	ntfs_init_big_inode(vi);

	ni = NTFS_I(vi);

	
	NInoSetNonResident(ni);
	NInoSetMstProtected(ni);
	NInoSetSparseDisabled(ni);
	ni->type = AT_DATA;
	ni->name = NULL;
	ni->name_len = 0;
	ni->itype.index.block_size = vol->mft_record_size;
	ni->itype.index.block_size_bits = vol->mft_record_size_bits;

	
	vol->mft_ino = vi;

	
	if (vol->mft_record_size > 64 * 1024) {
		ntfs_error(sb, "Unsupported mft record size %i (max 64kiB).",
				vol->mft_record_size);
		goto err_out;
	}
	i = vol->mft_record_size;
	if (i < sb->s_blocksize)
		i = sb->s_blocksize;
	m = (MFT_RECORD*)ntfs_malloc_nofs(i);
	if (!m) {
		ntfs_error(sb, "Failed to allocate buffer for $MFT record 0.");
		goto err_out;
	}

	
	block = vol->mft_lcn << vol->cluster_size_bits >>
			sb->s_blocksize_bits;
	nr_blocks = vol->mft_record_size >> sb->s_blocksize_bits;
	if (!nr_blocks)
		nr_blocks = 1;

	
	for (i = 0; i < nr_blocks; i++) {
		bh = sb_bread(sb, block++);
		if (!bh) {
			ntfs_error(sb, "Device read failed.");
			goto err_out;
		}
		memcpy((char*)m + (i << sb->s_blocksize_bits), bh->b_data,
				sb->s_blocksize);
		brelse(bh);
	}

	
	if (post_read_mst_fixup((NTFS_RECORD*)m, vol->mft_record_size)) {
		
		ntfs_error(sb, "MST fixup failed. $MFT is corrupt.");
		goto err_out;
	}

	
	vi->i_generation = ni->seq_no = le16_to_cpu(m->sequence_number);

	
	vi->i_mapping->a_ops = &ntfs_mst_aops;

	ctx = ntfs_attr_get_search_ctx(ni, m);
	if (!ctx) {
		err = -ENOMEM;
		goto err_out;
	}

	
	err = ntfs_attr_lookup(AT_ATTRIBUTE_LIST, NULL, 0, 0, 0, NULL, 0, ctx);
	if (err) {
		if (unlikely(err != -ENOENT)) {
			ntfs_error(sb, "Failed to lookup attribute list "
					"attribute. You should run chkdsk.");
			goto put_err_out;
		}
	} else  {
		ATTR_LIST_ENTRY *al_entry, *next_al_entry;
		u8 *al_end;
		static const char *es = "  Not allowed.  $MFT is corrupt.  "
				"You should run chkdsk.";

		ntfs_debug("Attribute list attribute found in $MFT.");
		NInoSetAttrList(ni);
		a = ctx->attr;
		if (a->flags & ATTR_COMPRESSION_MASK) {
			ntfs_error(sb, "Attribute list attribute is "
					"compressed.%s", es);
			goto put_err_out;
		}
		if (a->flags & ATTR_IS_ENCRYPTED ||
				a->flags & ATTR_IS_SPARSE) {
			if (a->non_resident) {
				ntfs_error(sb, "Non-resident attribute list "
						"attribute is encrypted/"
						"sparse.%s", es);
				goto put_err_out;
			}
			ntfs_warning(sb, "Resident attribute list attribute "
					"in $MFT system file is marked "
					"encrypted/sparse which is not true.  "
					"However, Windows allows this and "
					"chkdsk does not detect or correct it "
					"so we will just ignore the invalid "
					"flags and pretend they are not set.");
		}
		
		ni->attr_list_size = (u32)ntfs_attr_size(a);
		ni->attr_list = ntfs_malloc_nofs(ni->attr_list_size);
		if (!ni->attr_list) {
			ntfs_error(sb, "Not enough memory to allocate buffer "
					"for attribute list.");
			goto put_err_out;
		}
		if (a->non_resident) {
			NInoSetAttrListNonResident(ni);
			if (a->data.non_resident.lowest_vcn) {
				ntfs_error(sb, "Attribute list has non zero "
						"lowest_vcn. $MFT is corrupt. "
						"You should run chkdsk.");
				goto put_err_out;
			}
			
			ni->attr_list_rl.rl = ntfs_mapping_pairs_decompress(vol,
					a, NULL);
			if (IS_ERR(ni->attr_list_rl.rl)) {
				err = PTR_ERR(ni->attr_list_rl.rl);
				ni->attr_list_rl.rl = NULL;
				ntfs_error(sb, "Mapping pairs decompression "
						"failed with error code %i.",
						-err);
				goto put_err_out;
			}
			
			if ((err = load_attribute_list(vol, &ni->attr_list_rl,
					ni->attr_list, ni->attr_list_size,
					sle64_to_cpu(a->data.
					non_resident.initialized_size)))) {
				ntfs_error(sb, "Failed to load attribute list "
						"attribute with error code %i.",
						-err);
				goto put_err_out;
			}
		} else  {
			if ((u8*)a + le16_to_cpu(
					a->data.resident.value_offset) +
					le32_to_cpu(
					a->data.resident.value_length) >
					(u8*)ctx->mrec + vol->mft_record_size) {
				ntfs_error(sb, "Corrupt attribute list "
						"attribute.");
				goto put_err_out;
			}
			
			memcpy(ni->attr_list, (u8*)a + le16_to_cpu(
					a->data.resident.value_offset),
					le32_to_cpu(
					a->data.resident.value_length));
		}
		
		al_entry = (ATTR_LIST_ENTRY*)ni->attr_list;
		al_end = (u8*)al_entry + ni->attr_list_size;
		for (;; al_entry = next_al_entry) {
			
			if ((u8*)al_entry < ni->attr_list ||
					(u8*)al_entry > al_end)
				goto em_put_err_out;
			
			if ((u8*)al_entry == al_end)
				goto em_put_err_out;
			if (!al_entry->length)
				goto em_put_err_out;
			if ((u8*)al_entry + 6 > al_end || (u8*)al_entry +
					le16_to_cpu(al_entry->length) > al_end)
				goto em_put_err_out;
			next_al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
					le16_to_cpu(al_entry->length));
			if (le32_to_cpu(al_entry->type) > le32_to_cpu(AT_DATA))
				goto em_put_err_out;
			if (AT_DATA != al_entry->type)
				continue;
			
			if (al_entry->name_length)
				goto em_put_err_out;
			
			if (al_entry->lowest_vcn)
				goto em_put_err_out;
			
			if (MREF_LE(al_entry->mft_reference) != vi->i_ino) {
				
				ntfs_error(sb, "BUG: The first $DATA extent "
						"of $MFT is not in the base "
						"mft record. Please report "
						"you saw this message to "
						"linux-ntfs-dev@lists."
						"sourceforge.net");
				goto put_err_out;
			} else {
				
				if (MSEQNO_LE(al_entry->mft_reference) !=
						ni->seq_no)
					goto em_put_err_out;
				
				break;
			}
		}
	}

	ntfs_attr_reinit_search_ctx(ctx);

	
	a = NULL;
	next_vcn = last_vcn = highest_vcn = 0;
	while (!(err = ntfs_attr_lookup(AT_DATA, NULL, 0, 0, next_vcn, NULL, 0,
			ctx))) {
		runlist_element *nrl;

		
		a = ctx->attr;
		
		if (!a->non_resident) {
			ntfs_error(sb, "$MFT must be non-resident but a "
					"resident extent was found. $MFT is "
					"corrupt. Run chkdsk.");
			goto put_err_out;
		}
		
		if (a->flags & ATTR_COMPRESSION_MASK ||
				a->flags & ATTR_IS_ENCRYPTED ||
				a->flags & ATTR_IS_SPARSE) {
			ntfs_error(sb, "$MFT must be uncompressed, "
					"non-sparse, and unencrypted but a "
					"compressed/sparse/encrypted extent "
					"was found. $MFT is corrupt. Run "
					"chkdsk.");
			goto put_err_out;
		}
		nrl = ntfs_mapping_pairs_decompress(vol, a, ni->runlist.rl);
		if (IS_ERR(nrl)) {
			ntfs_error(sb, "ntfs_mapping_pairs_decompress() "
					"failed with error code %ld.  $MFT is "
					"corrupt.", PTR_ERR(nrl));
			goto put_err_out;
		}
		ni->runlist.rl = nrl;

		
		if (!next_vcn) {
			if (a->data.non_resident.lowest_vcn) {
				ntfs_error(sb, "First extent of $DATA "
						"attribute has non zero "
						"lowest_vcn. $MFT is corrupt. "
						"You should run chkdsk.");
				goto put_err_out;
			}
			
			last_vcn = sle64_to_cpu(
					a->data.non_resident.allocated_size)
					>> vol->cluster_size_bits;
			
			vi->i_size = sle64_to_cpu(
					a->data.non_resident.data_size);
			ni->initialized_size = sle64_to_cpu(
					a->data.non_resident.initialized_size);
			ni->allocated_size = sle64_to_cpu(
					a->data.non_resident.allocated_size);
			if ((vi->i_size >> vol->mft_record_size_bits) >=
					(1ULL << 32)) {
				ntfs_error(sb, "$MFT is too big! Aborting.");
				goto put_err_out;
			}
			ntfs_read_locked_inode(vi);
			if (is_bad_inode(vi)) {
				ntfs_error(sb, "ntfs_read_inode() of $MFT "
						"failed. BUG or corrupt $MFT. "
						"Run chkdsk and if no errors "
						"are found, please report you "
						"saw this message to "
						"linux-ntfs-dev@lists."
						"sourceforge.net");
				ntfs_attr_put_search_ctx(ctx);
				
				ntfs_free(m);
				return -1;
			}
			
			vi->i_uid = vi->i_gid = 0;
			
			vi->i_mode = S_IFREG;
			
			vi->i_op = &ntfs_empty_inode_ops;
			vi->i_fop = &ntfs_empty_file_ops;
		}

		
		highest_vcn = sle64_to_cpu(a->data.non_resident.highest_vcn);
		next_vcn = highest_vcn + 1;

		
		if (next_vcn <= 0)
			break;

		
		if (next_vcn < sle64_to_cpu(
				a->data.non_resident.lowest_vcn)) {
			ntfs_error(sb, "$MFT has corrupt attribute list "
					"attribute. Run chkdsk.");
			goto put_err_out;
		}
	}
	if (err != -ENOENT) {
		ntfs_error(sb, "Failed to lookup $MFT/$DATA attribute extent. "
				"$MFT is corrupt. Run chkdsk.");
		goto put_err_out;
	}
	if (!a) {
		ntfs_error(sb, "$MFT/$DATA attribute not found. $MFT is "
				"corrupt. Run chkdsk.");
		goto put_err_out;
	}
	if (highest_vcn && highest_vcn != last_vcn - 1) {
		ntfs_error(sb, "Failed to load the complete runlist for "
				"$MFT/$DATA. Driver bug or corrupt $MFT. "
				"Run chkdsk.");
		ntfs_debug("highest_vcn = 0x%llx, last_vcn - 1 = 0x%llx",
				(unsigned long long)highest_vcn,
				(unsigned long long)last_vcn - 1);
		goto put_err_out;
	}
	ntfs_attr_put_search_ctx(ctx);
	ntfs_debug("Done.");
	ntfs_free(m);

	lockdep_set_class(&ni->runlist.lock, &mft_ni_runlist_lock_key);
	lockdep_set_class(&ni->mrec_lock, &mft_ni_mrec_lock_key);

	return 0;

em_put_err_out:
	ntfs_error(sb, "Couldn't find first extent of $DATA attribute in "
			"attribute list. $MFT is corrupt. Run chkdsk.");
put_err_out:
	ntfs_attr_put_search_ctx(ctx);
err_out:
	ntfs_error(sb, "Failed. Marking inode as bad.");
	make_bad_inode(vi);
	ntfs_free(m);
	return -1;
}

static void __ntfs_clear_inode(ntfs_inode *ni)
{
	
	down_write(&ni->runlist.lock);
	if (ni->runlist.rl) {
		ntfs_free(ni->runlist.rl);
		ni->runlist.rl = NULL;
	}
	up_write(&ni->runlist.lock);

	if (ni->attr_list) {
		ntfs_free(ni->attr_list);
		ni->attr_list = NULL;
	}

	down_write(&ni->attr_list_rl.lock);
	if (ni->attr_list_rl.rl) {
		ntfs_free(ni->attr_list_rl.rl);
		ni->attr_list_rl.rl = NULL;
	}
	up_write(&ni->attr_list_rl.lock);

	if (ni->name_len && ni->name != I30) {
		
		BUG_ON(!ni->name);
		kfree(ni->name);
	}
}

void ntfs_clear_extent_inode(ntfs_inode *ni)
{
	ntfs_debug("Entering for inode 0x%lx.", ni->mft_no);

	BUG_ON(NInoAttr(ni));
	BUG_ON(ni->nr_extents != -1);

#ifdef NTFS_RW
	if (NInoDirty(ni)) {
		if (!is_bad_inode(VFS_I(ni->ext.base_ntfs_ino)))
			ntfs_error(ni->vol->sb, "Clearing dirty extent inode!  "
					"Losing data!  This is a BUG!!!");
		
	}
#endif 

	__ntfs_clear_inode(ni);

	
	ntfs_destroy_extent_inode(ni);
}

void ntfs_evict_big_inode(struct inode *vi)
{
	ntfs_inode *ni = NTFS_I(vi);

	truncate_inode_pages(&vi->i_data, 0);
	end_writeback(vi);

#ifdef NTFS_RW
	if (NInoDirty(ni)) {
		bool was_bad = (is_bad_inode(vi));

		
		ntfs_commit_inode(vi);

		if (!was_bad && (is_bad_inode(vi) || NInoDirty(ni))) {
			ntfs_error(vi->i_sb, "Failed to commit dirty inode "
					"0x%lx.  Losing data!", vi->i_ino);
			
		}
	}
#endif 

	
	if (ni->nr_extents > 0) {
		int i;

		for (i = 0; i < ni->nr_extents; i++)
			ntfs_clear_extent_inode(ni->ext.extent_ntfs_inos[i]);
		kfree(ni->ext.extent_ntfs_inos);
	}

	__ntfs_clear_inode(ni);

	if (NInoAttr(ni)) {
		
		if (ni->nr_extents == -1) {
			iput(VFS_I(ni->ext.base_ntfs_ino));
			ni->nr_extents = 0;
			ni->ext.base_ntfs_ino = NULL;
		}
	}
	return;
}

/**
 * ntfs_show_options - show mount options in /proc/mounts
 * @sf:		seq_file in which to write our mount options
 * @root:	root of the mounted tree whose mount options to display
 *
 * Called by the VFS once for each mounted ntfs volume when someone reads
 * /proc/mounts in order to display the NTFS specific mount options of each
 * mount. The mount options of fs specified by @root are written to the seq file
 * @sf and success is returned.
 */
int ntfs_show_options(struct seq_file *sf, struct dentry *root)
{
	ntfs_volume *vol = NTFS_SB(root->d_sb);
	int i;

	seq_printf(sf, ",uid=%i", vol->uid);
	seq_printf(sf, ",gid=%i", vol->gid);
	if (vol->fmask == vol->dmask)
		seq_printf(sf, ",umask=0%o", vol->fmask);
	else {
		seq_printf(sf, ",fmask=0%o", vol->fmask);
		seq_printf(sf, ",dmask=0%o", vol->dmask);
	}
	seq_printf(sf, ",nls=%s", vol->nls_map->charset);
	if (NVolCaseSensitive(vol))
		seq_printf(sf, ",case_sensitive");
	if (NVolShowSystemFiles(vol))
		seq_printf(sf, ",show_sys_files");
	if (!NVolSparseEnabled(vol))
		seq_printf(sf, ",disable_sparse");
	for (i = 0; on_errors_arr[i].val; i++) {
		if (on_errors_arr[i].val & vol->on_errors)
			seq_printf(sf, ",errors=%s", on_errors_arr[i].str);
	}
	seq_printf(sf, ",mft_zone_multiplier=%i", vol->mft_zone_multiplier);
	return 0;
}

#ifdef NTFS_RW

static const char *es = "  Leaving inconsistent metadata.  Unmount and run "
		"chkdsk.";

int ntfs_truncate(struct inode *vi)
{
	s64 new_size, old_size, nr_freed, new_alloc_size, old_alloc_size;
	VCN highest_vcn;
	unsigned long flags;
	ntfs_inode *base_ni, *ni = NTFS_I(vi);
	ntfs_volume *vol = ni->vol;
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	const char *te = "  Leaving file length out of sync with i_size.";
	int err, mp_size, size_change, alloc_change;
	u32 attr_len;

	ntfs_debug("Entering for inode 0x%lx.", vi->i_ino);
	BUG_ON(NInoAttr(ni));
	BUG_ON(S_ISDIR(vi->i_mode));
	BUG_ON(NInoMstProtected(ni));
	BUG_ON(ni->nr_extents < 0);
retry_truncate:
	down_write(&ni->runlist.lock);
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->ext.base_ntfs_ino;
	m = map_mft_record(base_ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		ntfs_error(vi->i_sb, "Failed to map mft record for inode 0x%lx "
				"(error code %d).%s", vi->i_ino, err, te);
		ctx = NULL;
		m = NULL;
		goto old_bad_out;
	}
	ctx = ntfs_attr_get_search_ctx(base_ni, m);
	if (unlikely(!ctx)) {
		ntfs_error(vi->i_sb, "Failed to allocate a search context for "
				"inode 0x%lx (not enough memory).%s",
				vi->i_ino, te);
		err = -ENOMEM;
		goto old_bad_out;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT) {
			ntfs_error(vi->i_sb, "Open attribute is missing from "
					"mft record.  Inode 0x%lx is corrupt.  "
					"Run chkdsk.%s", vi->i_ino, te);
			err = -EIO;
		} else
			ntfs_error(vi->i_sb, "Failed to lookup attribute in "
					"inode 0x%lx (error code %d).%s",
					vi->i_ino, err, te);
		goto old_bad_out;
	}
	m = ctx->mrec;
	a = ctx->attr;
	new_size = i_size_read(vi);
	
	old_size = ntfs_attr_size(a);
	
	if (NInoNonResident(ni))
		new_alloc_size = (new_size + vol->cluster_size - 1) &
				~(s64)vol->cluster_size_mask;
	else
		new_alloc_size = (new_size + 7) & ~7;
	
	read_lock_irqsave(&ni->size_lock, flags);
	old_alloc_size = ni->allocated_size;
	read_unlock_irqrestore(&ni->size_lock, flags);
	size_change = -1;
	if (new_size - old_size >= 0) {
		size_change = 1;
		if (new_size == old_size)
			size_change = 0;
	}
	
	alloc_change = -1;
	if (new_alloc_size - old_alloc_size >= 0) {
		alloc_change = 1;
		if (new_alloc_size == old_alloc_size)
			alloc_change = 0;
	}
	if (!size_change && !alloc_change)
		goto unm_done;
	
	if (size_change) {
		err = ntfs_attr_size_bounds_check(vol, ni->type, new_size);
		if (unlikely(err)) {
			if (err == -ERANGE) {
				ntfs_error(vol->sb, "Truncate would cause the "
						"inode 0x%lx to %simum size "
						"for its attribute type "
						"(0x%x).  Aborting truncate.",
						vi->i_ino,
						new_size > old_size ? "exceed "
						"the max" : "go under the min",
						le32_to_cpu(ni->type));
				err = -EFBIG;
			} else {
				ntfs_error(vol->sb, "Inode 0x%lx has unknown "
						"attribute type 0x%x.  "
						"Aborting truncate.",
						vi->i_ino,
						le32_to_cpu(ni->type));
				err = -EIO;
			}
			
			i_size_write(vi, old_size);
			goto err_out;
		}
	}
	if (NInoCompressed(ni) || NInoEncrypted(ni)) {
		ntfs_warning(vi->i_sb, "Changes in inode size are not "
				"supported yet for %s files, ignoring.",
				NInoCompressed(ni) ? "compressed" :
				"encrypted");
		err = -EOPNOTSUPP;
		goto bad_out;
	}
	if (a->non_resident)
		goto do_non_resident_truncate;
	BUG_ON(NInoNonResident(ni));
	
	if (new_size < vol->mft_record_size &&
			!ntfs_resident_attr_value_resize(m, a, new_size)) {
		
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		mark_mft_record_dirty(ctx->ntfs_ino);
		write_lock_irqsave(&ni->size_lock, flags);
		
		ni->allocated_size = le32_to_cpu(a->length) -
				le16_to_cpu(a->data.resident.value_offset);
		/*
		 * Note ntfs_resident_attr_value_resize() has already done any
		 * necessary data clearing in the attribute record.  When the
		 * file is being shrunk vmtruncate() will already have cleared
		 * the top part of the last partial page, i.e. since this is
		 * the resident case this is the page with index 0.  However,
		 * when the file is being expanded, the page cache page data
		 * between the old data_size, i.e. old_size, and the new_size
		 * has not been zeroed.  Fortunately, we do not need to zero it
		 * either since on one hand it will either already be zero due
		 * to both readpage and writepage clearing partial page data
		 * beyond i_size in which case there is nothing to do or in the
		 * case of the file being mmap()ped at the same time, POSIX
		 * specifies that the behaviour is unspecified thus we do not
		 * have to do anything.  This means that in our implementation
		 * in the rare case that the file is mmap()ped and a write
		 * occurred into the mmap()ped region just beyond the file size
		 * and writepage has not yet been called to write out the page
		 * (which would clear the area beyond the file size) and we now
		 * extend the file size to incorporate this dirty region
		 * outside the file size, a write of the page would result in
		 * this data being written to disk instead of being cleared.
		 * Given both POSIX and the Linux mmap(2) man page specify that
		 * this corner case is undefined, we choose to leave it like
		 * that as this is much simpler for us as we cannot lock the
		 * relevant page now since we are holding too many ntfs locks
		 * which would result in a lock reversal deadlock.
		 */
		ni->initialized_size = new_size;
		write_unlock_irqrestore(&ni->size_lock, flags);
		goto unm_done;
	}
	
	BUG_ON(size_change < 0);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
	up_write(&ni->runlist.lock);
	err = ntfs_attr_make_non_resident(ni, old_size);
	if (likely(!err))
		goto retry_truncate;
	if (unlikely(err != -EPERM && err != -ENOSPC)) {
		ntfs_error(vol->sb, "Cannot truncate inode 0x%lx, attribute "
				"type 0x%x, because the conversion from "
				"resident to non-resident attribute failed "
				"with error code %i.", vi->i_ino,
				(unsigned)le32_to_cpu(ni->type), err);
		if (err != -ENOMEM)
			err = -EIO;
		goto conv_err_out;
	}
	
	if (err == -ENOSPC)
		ntfs_error(vol->sb, "Not enough space in the mft record/on "
				"disk for the non-resident attribute value.  "
				"This case is not implemented yet.");
	else 
		ntfs_error(vol->sb, "This attribute type may not be "
				"non-resident.  This case is not implemented "
				"yet.");
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
do_non_resident_truncate:
	BUG_ON(!NInoNonResident(ni));
	if (alloc_change < 0) {
		highest_vcn = sle64_to_cpu(a->data.non_resident.highest_vcn);
		if (highest_vcn > 0 &&
				old_alloc_size >> vol->cluster_size_bits >
				highest_vcn + 1) {
			ntfs_error(vol->sb, "Cannot truncate inode 0x%lx, "
					"attribute type 0x%x, because the "
					"attribute is highly fragmented (it "
					"consists of multiple extents) and "
					"this case is not implemented yet.",
					vi->i_ino,
					(unsigned)le32_to_cpu(ni->type));
			err = -EOPNOTSUPP;
			goto bad_out;
		}
	}
	if (size_change < 0) {
		write_lock_irqsave(&ni->size_lock, flags);
		if (new_size < ni->initialized_size) {
			ni->initialized_size = new_size;
			a->data.non_resident.initialized_size =
					cpu_to_sle64(new_size);
		}
		a->data.non_resident.data_size = cpu_to_sle64(new_size);
		write_unlock_irqrestore(&ni->size_lock, flags);
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		mark_mft_record_dirty(ctx->ntfs_ino);
		
		if (!alloc_change)
			goto unm_done;
		BUG_ON(alloc_change > 0);
	} else  {
		if (alloc_change > 0) {
			ntfs_attr_put_search_ctx(ctx);
			unmap_mft_record(base_ni);
			up_write(&ni->runlist.lock);
			err = ntfs_attr_extend_allocation(ni, new_size,
					size_change > 0 ? new_size : -1, -1);
			goto done;
		}
		if (!alloc_change)
			goto alloc_done;
	}
	
	
	nr_freed = ntfs_cluster_free(ni, new_alloc_size >>
			vol->cluster_size_bits, -1, ctx);
	m = ctx->mrec;
	a = ctx->attr;
	if (unlikely(nr_freed < 0)) {
		ntfs_error(vol->sb, "Failed to release cluster(s) (error code "
				"%lli).  Unmount and run chkdsk to recover "
				"the lost cluster(s).", (long long)nr_freed);
		NVolSetErrors(vol);
		nr_freed = 0;
	}
	
	err = ntfs_rl_truncate_nolock(vol, &ni->runlist,
			new_alloc_size >> vol->cluster_size_bits);
	if (unlikely(err || IS_ERR(m))) {
		ntfs_error(vol->sb, "Failed to %s (error code %li).%s",
				IS_ERR(m) ?
				"restore attribute search context" :
				"truncate attribute runlist",
				IS_ERR(m) ? PTR_ERR(m) : err, es);
		err = -EIO;
		goto bad_out;
	}
	
	mp_size = ntfs_get_size_for_mapping_pairs(vol, ni->runlist.rl, 0, -1);
	if (unlikely(mp_size <= 0)) {
		ntfs_error(vol->sb, "Cannot shrink allocation of inode 0x%lx, "
				"attribute type 0x%x, because determining the "
				"size for the mapping pairs failed with error "
				"code %i.%s", vi->i_ino,
				(unsigned)le32_to_cpu(ni->type), mp_size, es);
		err = -EIO;
		goto bad_out;
	}
	attr_len = le32_to_cpu(a->length);
	err = ntfs_attr_record_resize(m, a, mp_size +
			le16_to_cpu(a->data.non_resident.mapping_pairs_offset));
	BUG_ON(err);
	err = ntfs_mapping_pairs_build(vol, (u8*)a +
			le16_to_cpu(a->data.non_resident.mapping_pairs_offset),
			mp_size, ni->runlist.rl, 0, -1, NULL);
	if (unlikely(err)) {
		ntfs_error(vol->sb, "Cannot shrink allocation of inode 0x%lx, "
				"attribute type 0x%x, because building the "
				"mapping pairs failed with error code %i.%s",
				vi->i_ino, (unsigned)le32_to_cpu(ni->type),
				err, es);
		err = -EIO;
		goto bad_out;
	}
	
	a->data.non_resident.highest_vcn = cpu_to_sle64((new_alloc_size >>
			vol->cluster_size_bits) - 1);
	write_lock_irqsave(&ni->size_lock, flags);
	ni->allocated_size = new_alloc_size;
	a->data.non_resident.allocated_size = cpu_to_sle64(new_alloc_size);
	if (NInoSparse(ni) || NInoCompressed(ni)) {
		if (nr_freed) {
			ni->itype.compressed.size -= nr_freed <<
					vol->cluster_size_bits;
			BUG_ON(ni->itype.compressed.size < 0);
			a->data.non_resident.compressed_size = cpu_to_sle64(
					ni->itype.compressed.size);
			vi->i_blocks = ni->itype.compressed.size >> 9;
		}
	} else
		vi->i_blocks = new_alloc_size >> 9;
	write_unlock_irqrestore(&ni->size_lock, flags);
alloc_done:
	if (size_change > 0)
		a->data.non_resident.data_size = cpu_to_sle64(new_size);
	/* Ensure the modified mft record is written out. */
	flush_dcache_mft_record_page(ctx->ntfs_ino);
	mark_mft_record_dirty(ctx->ntfs_ino);
unm_done:
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(base_ni);
	up_write(&ni->runlist.lock);
done:
	
	if (!IS_NOCMTIME(VFS_I(base_ni)) && !IS_RDONLY(VFS_I(base_ni))) {
		struct timespec now = current_fs_time(VFS_I(base_ni)->i_sb);
		int sync_it = 0;

		if (!timespec_equal(&VFS_I(base_ni)->i_mtime, &now) ||
		    !timespec_equal(&VFS_I(base_ni)->i_ctime, &now))
			sync_it = 1;
		VFS_I(base_ni)->i_mtime = now;
		VFS_I(base_ni)->i_ctime = now;

		if (sync_it)
			mark_inode_dirty_sync(VFS_I(base_ni));
	}

	if (likely(!err)) {
		NInoClearTruncateFailed(ni);
		ntfs_debug("Done.");
	}
	return err;
old_bad_out:
	old_size = -1;
bad_out:
	if (err != -ENOMEM && err != -EOPNOTSUPP)
		NVolSetErrors(vol);
	if (err != -EOPNOTSUPP)
		NInoSetTruncateFailed(ni);
	else if (old_size >= 0)
		i_size_write(vi, old_size);
err_out:
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(base_ni);
	up_write(&ni->runlist.lock);
out:
	ntfs_debug("Failed.  Returning error code %i.", err);
	return err;
conv_err_out:
	if (err != -ENOMEM && err != -EOPNOTSUPP)
		NVolSetErrors(vol);
	if (err != -EOPNOTSUPP)
		NInoSetTruncateFailed(ni);
	else
		i_size_write(vi, old_size);
	goto out;
}

void ntfs_truncate_vfs(struct inode *vi) {
	ntfs_truncate(vi);
}

int ntfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *vi = dentry->d_inode;
	int err;
	unsigned int ia_valid = attr->ia_valid;

	err = inode_change_ok(vi, attr);
	if (err)
		goto out;
	
	if (ia_valid & (ATTR_UID | ATTR_GID | ATTR_MODE)) {
		ntfs_warning(vi->i_sb, "Changes in user/group/mode are not "
				"supported yet, ignoring.");
		err = -EOPNOTSUPP;
		goto out;
	}
	if (ia_valid & ATTR_SIZE) {
		if (attr->ia_size != i_size_read(vi)) {
			ntfs_inode *ni = NTFS_I(vi);
			if (NInoCompressed(ni) || NInoEncrypted(ni)) {
				ntfs_warning(vi->i_sb, "Changes in inode size "
						"are not supported yet for "
						"%s files, ignoring.",
						NInoCompressed(ni) ?
						"compressed" : "encrypted");
				err = -EOPNOTSUPP;
			} else
				err = vmtruncate(vi, attr->ia_size);
			if (err || ia_valid == ATTR_SIZE)
				goto out;
		} else {
			ia_valid |= ATTR_MTIME | ATTR_CTIME;
		}
	}
	if (ia_valid & ATTR_ATIME)
		vi->i_atime = timespec_trunc(attr->ia_atime,
				vi->i_sb->s_time_gran);
	if (ia_valid & ATTR_MTIME)
		vi->i_mtime = timespec_trunc(attr->ia_mtime,
				vi->i_sb->s_time_gran);
	if (ia_valid & ATTR_CTIME)
		vi->i_ctime = timespec_trunc(attr->ia_ctime,
				vi->i_sb->s_time_gran);
	mark_inode_dirty(vi);
out:
	return err;
}

int __ntfs_write_inode(struct inode *vi, int sync)
{
	sle64 nt;
	ntfs_inode *ni = NTFS_I(vi);
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *m;
	STANDARD_INFORMATION *si;
	int err = 0;
	bool modified = false;

	ntfs_debug("Entering for %sinode 0x%lx.", NInoAttr(ni) ? "attr " : "",
			vi->i_ino);
	/*
	 * Dirty attribute inodes are written via their real inodes so just
	 * clean them here.  Access time updates are taken care off when the
	 * real inode is written.
	 */
	if (NInoAttr(ni)) {
		NInoClearDirty(ni);
		ntfs_debug("Done.");
		return 0;
	}
	
	m = map_mft_record(ni);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		goto err_out;
	}
	
	ctx = ntfs_attr_get_search_ctx(ni, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto unm_err_out;
	}
	err = ntfs_attr_lookup(AT_STANDARD_INFORMATION, NULL, 0,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (unlikely(err)) {
		ntfs_attr_put_search_ctx(ctx);
		goto unm_err_out;
	}
	si = (STANDARD_INFORMATION*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->data.resident.value_offset));
	
	nt = utc2ntfs(vi->i_mtime);
	if (si->last_data_change_time != nt) {
		ntfs_debug("Updating mtime for inode 0x%lx: old = 0x%llx, "
				"new = 0x%llx", vi->i_ino, (long long)
				sle64_to_cpu(si->last_data_change_time),
				(long long)sle64_to_cpu(nt));
		si->last_data_change_time = nt;
		modified = true;
	}
	nt = utc2ntfs(vi->i_ctime);
	if (si->last_mft_change_time != nt) {
		ntfs_debug("Updating ctime for inode 0x%lx: old = 0x%llx, "
				"new = 0x%llx", vi->i_ino, (long long)
				sle64_to_cpu(si->last_mft_change_time),
				(long long)sle64_to_cpu(nt));
		si->last_mft_change_time = nt;
		modified = true;
	}
	nt = utc2ntfs(vi->i_atime);
	if (si->last_access_time != nt) {
		ntfs_debug("Updating atime for inode 0x%lx: old = 0x%llx, "
				"new = 0x%llx", vi->i_ino,
				(long long)sle64_to_cpu(si->last_access_time),
				(long long)sle64_to_cpu(nt));
		si->last_access_time = nt;
		modified = true;
	}
	/*
	 * If we just modified the standard information attribute we need to
	 * mark the mft record it is in dirty.  We do this manually so that
	 * mark_inode_dirty() is not called which would redirty the inode and
	 * hence result in an infinite loop of trying to write the inode.
	 * There is no need to mark the base inode nor the base mft record
	 * dirty, since we are going to write this mft record below in any case
	 * and the base mft record may actually not have been modified so it
	 * might not need to be written out.
	 * NOTE: It is not a problem when the inode for $MFT itself is being
	 * written out as mark_ntfs_record_dirty() will only set I_DIRTY_PAGES
	 * on the $MFT inode and hence ntfs_write_inode() will not be
	 * re-invoked because of it which in turn is ok since the dirtied mft
	 * record will be cleaned and written out to disk below, i.e. before
	 * this function returns.
	 */
	if (modified) {
		flush_dcache_mft_record_page(ctx->ntfs_ino);
		if (!NInoTestSetDirty(ctx->ntfs_ino))
			mark_ntfs_record_dirty(ctx->ntfs_ino->page,
					ctx->ntfs_ino->page_ofs);
	}
	ntfs_attr_put_search_ctx(ctx);
	
	if (NInoDirty(ni))
		err = write_mft_record(ni, m, sync);
	
	mutex_lock(&ni->extent_lock);
	if (ni->nr_extents > 0) {
		ntfs_inode **extent_nis = ni->ext.extent_ntfs_inos;
		int i;

		ntfs_debug("Writing %i extent inodes.", ni->nr_extents);
		for (i = 0; i < ni->nr_extents; i++) {
			ntfs_inode *tni = extent_nis[i];

			if (NInoDirty(tni)) {
				MFT_RECORD *tm = map_mft_record(tni);
				int ret;

				if (IS_ERR(tm)) {
					if (!err || err == -ENOMEM)
						err = PTR_ERR(tm);
					continue;
				}
				ret = write_mft_record(tni, tm, sync);
				unmap_mft_record(tni);
				if (unlikely(ret)) {
					if (!err || err == -ENOMEM)
						err = ret;
				}
			}
		}
	}
	mutex_unlock(&ni->extent_lock);
	unmap_mft_record(ni);
	if (unlikely(err))
		goto err_out;
	ntfs_debug("Done.");
	return 0;
unm_err_out:
	unmap_mft_record(ni);
err_out:
	if (err == -ENOMEM) {
		ntfs_warning(vi->i_sb, "Not enough memory to write inode.  "
				"Marking the inode dirty again, so the VFS "
				"retries later.");
		mark_inode_dirty(vi);
	} else {
		ntfs_error(vi->i_sb, "Failed (error %i):  Run chkdsk.", -err);
		NVolSetErrors(ni->vol);
	}
	return err;
}

#endif 
