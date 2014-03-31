/*
 * linux/fs/ext2/xattr.c
 *
 * Copyright (C) 2001-2003 Andreas Gruenbacher <agruen@suse.de>
 *
 * Fix by Harrison Xing <harrison@mountainviewdata.com>.
 * Extended attributes for symlinks and special files added per
 *  suggestion of Luka Renko <luka.renko@hermes.si>.
 * xattr consolidation Copyright (c) 2004 James Morris <jmorris@redhat.com>,
 *  Red Hat Inc.
 *
 */


#include <linux/buffer_head.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mbcache.h>
#include <linux/quotaops.h>
#include <linux/rwsem.h>
#include <linux/security.h>
#include "ext2.h"
#include "xattr.h"
#include "acl.h"

#define HDR(bh) ((struct ext2_xattr_header *)((bh)->b_data))
#define ENTRY(ptr) ((struct ext2_xattr_entry *)(ptr))
#define FIRST_ENTRY(bh) ENTRY(HDR(bh)+1)
#define IS_LAST_ENTRY(entry) (*(__u32 *)(entry) == 0)

#ifdef EXT2_XATTR_DEBUG
# define ea_idebug(inode, f...) do { \
		printk(KERN_DEBUG "inode %s:%ld: ", \
			inode->i_sb->s_id, inode->i_ino); \
		printk(f); \
		printk("\n"); \
	} while (0)
# define ea_bdebug(bh, f...) do { \
		char b[BDEVNAME_SIZE]; \
		printk(KERN_DEBUG "block %s:%lu: ", \
			bdevname(bh->b_bdev, b), \
			(unsigned long) bh->b_blocknr); \
		printk(f); \
		printk("\n"); \
	} while (0)
#else
# define ea_idebug(f...)
# define ea_bdebug(f...)
#endif

static int ext2_xattr_set2(struct inode *, struct buffer_head *,
			   struct ext2_xattr_header *);

static int ext2_xattr_cache_insert(struct buffer_head *);
static struct buffer_head *ext2_xattr_cache_find(struct inode *,
						 struct ext2_xattr_header *);
static void ext2_xattr_rehash(struct ext2_xattr_header *,
			      struct ext2_xattr_entry *);

static struct mb_cache *ext2_xattr_cache;

static const struct xattr_handler *ext2_xattr_handler_map[] = {
	[EXT2_XATTR_INDEX_USER]		     = &ext2_xattr_user_handler,
#ifdef CONFIG_EXT2_FS_POSIX_ACL
	[EXT2_XATTR_INDEX_POSIX_ACL_ACCESS]  = &ext2_xattr_acl_access_handler,
	[EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT] = &ext2_xattr_acl_default_handler,
#endif
	[EXT2_XATTR_INDEX_TRUSTED]	     = &ext2_xattr_trusted_handler,
#ifdef CONFIG_EXT2_FS_SECURITY
	[EXT2_XATTR_INDEX_SECURITY]	     = &ext2_xattr_security_handler,
#endif
};

const struct xattr_handler *ext2_xattr_handlers[] = {
	&ext2_xattr_user_handler,
	&ext2_xattr_trusted_handler,
#ifdef CONFIG_EXT2_FS_POSIX_ACL
	&ext2_xattr_acl_access_handler,
	&ext2_xattr_acl_default_handler,
#endif
#ifdef CONFIG_EXT2_FS_SECURITY
	&ext2_xattr_security_handler,
#endif
	NULL
};

static inline const struct xattr_handler *
ext2_xattr_handler(int name_index)
{
	const struct xattr_handler *handler = NULL;

	if (name_index > 0 && name_index < ARRAY_SIZE(ext2_xattr_handler_map))
		handler = ext2_xattr_handler_map[name_index];
	return handler;
}

int
ext2_xattr_get(struct inode *inode, int name_index, const char *name,
	       void *buffer, size_t buffer_size)
{
	struct buffer_head *bh = NULL;
	struct ext2_xattr_entry *entry;
	size_t name_len, size;
	char *end;
	int error;

	ea_idebug(inode, "name=%d.%s, buffer=%p, buffer_size=%ld",
		  name_index, name, buffer, (long)buffer_size);

	if (name == NULL)
		return -EINVAL;
	name_len = strlen(name);
	if (name_len > 255)
		return -ERANGE;

	down_read(&EXT2_I(inode)->xattr_sem);
	error = -ENODATA;
	if (!EXT2_I(inode)->i_file_acl)
		goto cleanup;
	ea_idebug(inode, "reading block %d", EXT2_I(inode)->i_file_acl);
	bh = sb_bread(inode->i_sb, EXT2_I(inode)->i_file_acl);
	error = -EIO;
	if (!bh)
		goto cleanup;
	ea_bdebug(bh, "b_count=%d, refcount=%d",
		atomic_read(&(bh->b_count)), le32_to_cpu(HDR(bh)->h_refcount));
	end = bh->b_data + bh->b_size;
	if (HDR(bh)->h_magic != cpu_to_le32(EXT2_XATTR_MAGIC) ||
	    HDR(bh)->h_blocks != cpu_to_le32(1)) {
bad_block:	ext2_error(inode->i_sb, "ext2_xattr_get",
			"inode %ld: bad block %d", inode->i_ino,
			EXT2_I(inode)->i_file_acl);
		error = -EIO;
		goto cleanup;
	}

	
	entry = FIRST_ENTRY(bh);
	while (!IS_LAST_ENTRY(entry)) {
		struct ext2_xattr_entry *next =
			EXT2_XATTR_NEXT(entry);
		if ((char *)next >= end)
			goto bad_block;
		if (name_index == entry->e_name_index &&
		    name_len == entry->e_name_len &&
		    memcmp(name, entry->e_name, name_len) == 0)
			goto found;
		entry = next;
	}
	if (ext2_xattr_cache_insert(bh))
		ea_idebug(inode, "cache insert failed");
	error = -ENODATA;
	goto cleanup;
found:
	
	if (entry->e_value_block != 0)
		goto bad_block;
	size = le32_to_cpu(entry->e_value_size);
	if (size > inode->i_sb->s_blocksize ||
	    le16_to_cpu(entry->e_value_offs) + size > inode->i_sb->s_blocksize)
		goto bad_block;

	if (ext2_xattr_cache_insert(bh))
		ea_idebug(inode, "cache insert failed");
	if (buffer) {
		error = -ERANGE;
		if (size > buffer_size)
			goto cleanup;
		
		memcpy(buffer, bh->b_data + le16_to_cpu(entry->e_value_offs),
			size);
	}
	error = size;

cleanup:
	brelse(bh);
	up_read(&EXT2_I(inode)->xattr_sem);

	return error;
}

static int
ext2_xattr_list(struct dentry *dentry, char *buffer, size_t buffer_size)
{
	struct inode *inode = dentry->d_inode;
	struct buffer_head *bh = NULL;
	struct ext2_xattr_entry *entry;
	char *end;
	size_t rest = buffer_size;
	int error;

	ea_idebug(inode, "buffer=%p, buffer_size=%ld",
		  buffer, (long)buffer_size);

	down_read(&EXT2_I(inode)->xattr_sem);
	error = 0;
	if (!EXT2_I(inode)->i_file_acl)
		goto cleanup;
	ea_idebug(inode, "reading block %d", EXT2_I(inode)->i_file_acl);
	bh = sb_bread(inode->i_sb, EXT2_I(inode)->i_file_acl);
	error = -EIO;
	if (!bh)
		goto cleanup;
	ea_bdebug(bh, "b_count=%d, refcount=%d",
		atomic_read(&(bh->b_count)), le32_to_cpu(HDR(bh)->h_refcount));
	end = bh->b_data + bh->b_size;
	if (HDR(bh)->h_magic != cpu_to_le32(EXT2_XATTR_MAGIC) ||
	    HDR(bh)->h_blocks != cpu_to_le32(1)) {
bad_block:	ext2_error(inode->i_sb, "ext2_xattr_list",
			"inode %ld: bad block %d", inode->i_ino,
			EXT2_I(inode)->i_file_acl);
		error = -EIO;
		goto cleanup;
	}

	
	entry = FIRST_ENTRY(bh);
	while (!IS_LAST_ENTRY(entry)) {
		struct ext2_xattr_entry *next = EXT2_XATTR_NEXT(entry);

		if ((char *)next >= end)
			goto bad_block;
		entry = next;
	}
	if (ext2_xattr_cache_insert(bh))
		ea_idebug(inode, "cache insert failed");

	
	for (entry = FIRST_ENTRY(bh); !IS_LAST_ENTRY(entry);
	     entry = EXT2_XATTR_NEXT(entry)) {
		const struct xattr_handler *handler =
			ext2_xattr_handler(entry->e_name_index);

		if (handler) {
			size_t size = handler->list(dentry, buffer, rest,
						    entry->e_name,
						    entry->e_name_len,
						    handler->flags);
			if (buffer) {
				if (size > rest) {
					error = -ERANGE;
					goto cleanup;
				}
				buffer += size;
			}
			rest -= size;
		}
	}
	error = buffer_size - rest;  

cleanup:
	brelse(bh);
	up_read(&EXT2_I(inode)->xattr_sem);

	return error;
}

ssize_t
ext2_listxattr(struct dentry *dentry, char *buffer, size_t size)
{
	return ext2_xattr_list(dentry, buffer, size);
}

static void ext2_xattr_update_super_block(struct super_block *sb)
{
	if (EXT2_HAS_COMPAT_FEATURE(sb, EXT2_FEATURE_COMPAT_EXT_ATTR))
		return;

	spin_lock(&EXT2_SB(sb)->s_lock);
	EXT2_SET_COMPAT_FEATURE(sb, EXT2_FEATURE_COMPAT_EXT_ATTR);
	spin_unlock(&EXT2_SB(sb)->s_lock);
	sb->s_dirt = 1;
	mark_buffer_dirty(EXT2_SB(sb)->s_sbh);
}

int
ext2_xattr_set(struct inode *inode, int name_index, const char *name,
	       const void *value, size_t value_len, int flags)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh = NULL;
	struct ext2_xattr_header *header = NULL;
	struct ext2_xattr_entry *here, *last;
	size_t name_len, free, min_offs = sb->s_blocksize;
	int not_found = 1, error;
	char *end;
	
	
	ea_idebug(inode, "name=%d.%s, value=%p, value_len=%ld",
		  name_index, name, value, (long)value_len);

	if (value == NULL)
		value_len = 0;
	if (name == NULL)
		return -EINVAL;
	name_len = strlen(name);
	if (name_len > 255 || value_len > sb->s_blocksize)
		return -ERANGE;
	down_write(&EXT2_I(inode)->xattr_sem);
	if (EXT2_I(inode)->i_file_acl) {
		
		bh = sb_bread(sb, EXT2_I(inode)->i_file_acl);
		error = -EIO;
		if (!bh)
			goto cleanup;
		ea_bdebug(bh, "b_count=%d, refcount=%d",
			atomic_read(&(bh->b_count)),
			le32_to_cpu(HDR(bh)->h_refcount));
		header = HDR(bh);
		end = bh->b_data + bh->b_size;
		if (header->h_magic != cpu_to_le32(EXT2_XATTR_MAGIC) ||
		    header->h_blocks != cpu_to_le32(1)) {
bad_block:		ext2_error(sb, "ext2_xattr_set",
				"inode %ld: bad block %d", inode->i_ino, 
				   EXT2_I(inode)->i_file_acl);
			error = -EIO;
			goto cleanup;
		}
		
		here = FIRST_ENTRY(bh);
		while (!IS_LAST_ENTRY(here)) {
			struct ext2_xattr_entry *next = EXT2_XATTR_NEXT(here);
			if ((char *)next >= end)
				goto bad_block;
			if (!here->e_value_block && here->e_value_size) {
				size_t offs = le16_to_cpu(here->e_value_offs);
				if (offs < min_offs)
					min_offs = offs;
			}
			not_found = name_index - here->e_name_index;
			if (!not_found)
				not_found = name_len - here->e_name_len;
			if (!not_found)
				not_found = memcmp(name, here->e_name,name_len);
			if (not_found <= 0)
				break;
			here = next;
		}
		last = here;
		
		while (!IS_LAST_ENTRY(last)) {
			struct ext2_xattr_entry *next = EXT2_XATTR_NEXT(last);
			if ((char *)next >= end)
				goto bad_block;
			if (!last->e_value_block && last->e_value_size) {
				size_t offs = le16_to_cpu(last->e_value_offs);
				if (offs < min_offs)
					min_offs = offs;
			}
			last = next;
		}

		
		free = min_offs - ((char*)last - (char*)header) - sizeof(__u32);
	} else {
		
		free = sb->s_blocksize -
			sizeof(struct ext2_xattr_header) - sizeof(__u32);
		here = last = NULL;  
	}

	if (not_found) {
		
		error = -ENODATA;
		if (flags & XATTR_REPLACE)
			goto cleanup;
		error = 0;
		if (value == NULL)
			goto cleanup;
	} else {
		
		error = -EEXIST;
		if (flags & XATTR_CREATE)
			goto cleanup;
		if (!here->e_value_block && here->e_value_size) {
			size_t size = le32_to_cpu(here->e_value_size);

			if (le16_to_cpu(here->e_value_offs) + size > 
			    sb->s_blocksize || size > sb->s_blocksize)
				goto bad_block;
			free += EXT2_XATTR_SIZE(size);
		}
		free += EXT2_XATTR_LEN(name_len);
	}
	error = -ENOSPC;
	if (free < EXT2_XATTR_LEN(name_len) + EXT2_XATTR_SIZE(value_len))
		goto cleanup;

	

	if (header) {
		struct mb_cache_entry *ce;

		
		ce = mb_cache_entry_get(ext2_xattr_cache, bh->b_bdev,
					bh->b_blocknr);
		lock_buffer(bh);
		if (header->h_refcount == cpu_to_le32(1)) {
			ea_bdebug(bh, "modifying in-place");
			if (ce)
				mb_cache_entry_free(ce);
			
		} else {
			int offset;

			if (ce)
				mb_cache_entry_release(ce);
			unlock_buffer(bh);
			ea_bdebug(bh, "cloning");
			header = kmalloc(bh->b_size, GFP_KERNEL);
			error = -ENOMEM;
			if (header == NULL)
				goto cleanup;
			memcpy(header, HDR(bh), bh->b_size);
			header->h_refcount = cpu_to_le32(1);

			offset = (char *)here - bh->b_data;
			here = ENTRY((char *)header + offset);
			offset = (char *)last - bh->b_data;
			last = ENTRY((char *)header + offset);
		}
	} else {
		
		header = kzalloc(sb->s_blocksize, GFP_KERNEL);
		error = -ENOMEM;
		if (header == NULL)
			goto cleanup;
		end = (char *)header + sb->s_blocksize;
		header->h_magic = cpu_to_le32(EXT2_XATTR_MAGIC);
		header->h_blocks = header->h_refcount = cpu_to_le32(1);
		last = here = ENTRY(header+1);
	}

	

	if (not_found) {
		
		size_t size = EXT2_XATTR_LEN(name_len);
		size_t rest = (char *)last - (char *)here;
		memmove((char *)here + size, here, rest);
		memset(here, 0, size);
		here->e_name_index = name_index;
		here->e_name_len = name_len;
		memcpy(here->e_name, name, name_len);
	} else {
		if (!here->e_value_block && here->e_value_size) {
			char *first_val = (char *)header + min_offs;
			size_t offs = le16_to_cpu(here->e_value_offs);
			char *val = (char *)header + offs;
			size_t size = EXT2_XATTR_SIZE(
				le32_to_cpu(here->e_value_size));

			if (size == EXT2_XATTR_SIZE(value_len)) {
				here->e_value_size = cpu_to_le32(value_len);
				memset(val + size - EXT2_XATTR_PAD, 0,
				       EXT2_XATTR_PAD); 
				memcpy(val, value, value_len);
				goto skip_replace;
			}

			
			memmove(first_val + size, first_val, val - first_val);
			memset(first_val, 0, size);
			here->e_value_offs = 0;
			min_offs += size;

			
			last = ENTRY(header+1);
			while (!IS_LAST_ENTRY(last)) {
				size_t o = le16_to_cpu(last->e_value_offs);
				if (!last->e_value_block && o < offs)
					last->e_value_offs =
						cpu_to_le16(o + size);
				last = EXT2_XATTR_NEXT(last);
			}
		}
		if (value == NULL) {
			
			size_t size = EXT2_XATTR_LEN(name_len);
			last = ENTRY((char *)last - size);
			memmove(here, (char*)here + size,
				(char*)last - (char*)here);
			memset(last, 0, size);
		}
	}

	if (value != NULL) {
		
		here->e_value_size = cpu_to_le32(value_len);
		if (value_len) {
			size_t size = EXT2_XATTR_SIZE(value_len);
			char *val = (char *)header + min_offs - size;
			here->e_value_offs =
				cpu_to_le16((char *)val - (char *)header);
			memset(val + size - EXT2_XATTR_PAD, 0,
			       EXT2_XATTR_PAD); 
			memcpy(val, value, value_len);
		}
	}

skip_replace:
	if (IS_LAST_ENTRY(ENTRY(header+1))) {
		
		if (bh && header == HDR(bh))
			unlock_buffer(bh);  
		error = ext2_xattr_set2(inode, bh, NULL);
	} else {
		ext2_xattr_rehash(header, here);
		if (bh && header == HDR(bh))
			unlock_buffer(bh);  
		error = ext2_xattr_set2(inode, bh, header);
	}

cleanup:
	brelse(bh);
	if (!(bh && header == HDR(bh)))
		kfree(header);
	up_write(&EXT2_I(inode)->xattr_sem);

	return error;
}

static int
ext2_xattr_set2(struct inode *inode, struct buffer_head *old_bh,
		struct ext2_xattr_header *header)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *new_bh = NULL;
	int error;

	if (header) {
		new_bh = ext2_xattr_cache_find(inode, header);
		if (new_bh) {
			
			if (new_bh == old_bh) {
				ea_bdebug(new_bh, "keeping this block");
			} else {
				ea_bdebug(new_bh, "reusing block");

				error = dquot_alloc_block(inode, 1);
				if (error) {
					unlock_buffer(new_bh);
					goto cleanup;
				}
				le32_add_cpu(&HDR(new_bh)->h_refcount, 1);
				ea_bdebug(new_bh, "refcount now=%d",
					le32_to_cpu(HDR(new_bh)->h_refcount));
			}
			unlock_buffer(new_bh);
		} else if (old_bh && header == HDR(old_bh)) {
			new_bh = old_bh;
			get_bh(new_bh);
			ext2_xattr_cache_insert(new_bh);
		} else {
			
			ext2_fsblk_t goal = ext2_group_first_block_no(sb,
						EXT2_I(inode)->i_block_group);
			int block = ext2_new_block(inode, goal, &error);
			if (error)
				goto cleanup;
			ea_idebug(inode, "creating block %d", block);

			new_bh = sb_getblk(sb, block);
			if (!new_bh) {
				ext2_free_blocks(inode, block, 1);
				mark_inode_dirty(inode);
				error = -EIO;
				goto cleanup;
			}
			lock_buffer(new_bh);
			memcpy(new_bh->b_data, header, new_bh->b_size);
			set_buffer_uptodate(new_bh);
			unlock_buffer(new_bh);
			ext2_xattr_cache_insert(new_bh);
			
			ext2_xattr_update_super_block(sb);
		}
		mark_buffer_dirty(new_bh);
		if (IS_SYNC(inode)) {
			sync_dirty_buffer(new_bh);
			error = -EIO;
			if (buffer_req(new_bh) && !buffer_uptodate(new_bh))
				goto cleanup;
		}
	}

	
	EXT2_I(inode)->i_file_acl = new_bh ? new_bh->b_blocknr : 0;
	inode->i_ctime = CURRENT_TIME_SEC;
	if (IS_SYNC(inode)) {
		error = sync_inode_metadata(inode, 1);
		/* In case sync failed due to ENOSPC the inode was actually
		 * written (only some dirty data were not) so we just proceed
		 * as if nothing happened and cleanup the unused block */
		if (error && error != -ENOSPC) {
			if (new_bh && new_bh != old_bh) {
				dquot_free_block_nodirty(inode, 1);
				mark_inode_dirty(inode);
			}
			goto cleanup;
		}
	} else
		mark_inode_dirty(inode);

	error = 0;
	if (old_bh && old_bh != new_bh) {
		struct mb_cache_entry *ce;

		ce = mb_cache_entry_get(ext2_xattr_cache, old_bh->b_bdev,
					old_bh->b_blocknr);
		lock_buffer(old_bh);
		if (HDR(old_bh)->h_refcount == cpu_to_le32(1)) {
			
			if (ce)
				mb_cache_entry_free(ce);
			ea_bdebug(old_bh, "freeing");
			ext2_free_blocks(inode, old_bh->b_blocknr, 1);
			mark_inode_dirty(inode);
			get_bh(old_bh);
			bforget(old_bh);
		} else {
			
			le32_add_cpu(&HDR(old_bh)->h_refcount, -1);
			if (ce)
				mb_cache_entry_release(ce);
			dquot_free_block_nodirty(inode, 1);
			mark_inode_dirty(inode);
			mark_buffer_dirty(old_bh);
			ea_bdebug(old_bh, "refcount now=%d",
				le32_to_cpu(HDR(old_bh)->h_refcount));
		}
		unlock_buffer(old_bh);
	}

cleanup:
	brelse(new_bh);

	return error;
}

void
ext2_xattr_delete_inode(struct inode *inode)
{
	struct buffer_head *bh = NULL;
	struct mb_cache_entry *ce;

	down_write(&EXT2_I(inode)->xattr_sem);
	if (!EXT2_I(inode)->i_file_acl)
		goto cleanup;
	bh = sb_bread(inode->i_sb, EXT2_I(inode)->i_file_acl);
	if (!bh) {
		ext2_error(inode->i_sb, "ext2_xattr_delete_inode",
			"inode %ld: block %d read error", inode->i_ino,
			EXT2_I(inode)->i_file_acl);
		goto cleanup;
	}
	ea_bdebug(bh, "b_count=%d", atomic_read(&(bh->b_count)));
	if (HDR(bh)->h_magic != cpu_to_le32(EXT2_XATTR_MAGIC) ||
	    HDR(bh)->h_blocks != cpu_to_le32(1)) {
		ext2_error(inode->i_sb, "ext2_xattr_delete_inode",
			"inode %ld: bad block %d", inode->i_ino,
			EXT2_I(inode)->i_file_acl);
		goto cleanup;
	}
	ce = mb_cache_entry_get(ext2_xattr_cache, bh->b_bdev, bh->b_blocknr);
	lock_buffer(bh);
	if (HDR(bh)->h_refcount == cpu_to_le32(1)) {
		if (ce)
			mb_cache_entry_free(ce);
		ext2_free_blocks(inode, EXT2_I(inode)->i_file_acl, 1);
		get_bh(bh);
		bforget(bh);
		unlock_buffer(bh);
	} else {
		le32_add_cpu(&HDR(bh)->h_refcount, -1);
		if (ce)
			mb_cache_entry_release(ce);
		ea_bdebug(bh, "refcount now=%d",
			le32_to_cpu(HDR(bh)->h_refcount));
		unlock_buffer(bh);
		mark_buffer_dirty(bh);
		if (IS_SYNC(inode))
			sync_dirty_buffer(bh);
		dquot_free_block_nodirty(inode, 1);
	}
	EXT2_I(inode)->i_file_acl = 0;

cleanup:
	brelse(bh);
	up_write(&EXT2_I(inode)->xattr_sem);
}

void
ext2_xattr_put_super(struct super_block *sb)
{
	mb_cache_shrink(sb->s_bdev);
}


static int
ext2_xattr_cache_insert(struct buffer_head *bh)
{
	__u32 hash = le32_to_cpu(HDR(bh)->h_hash);
	struct mb_cache_entry *ce;
	int error;

	ce = mb_cache_entry_alloc(ext2_xattr_cache, GFP_NOFS);
	if (!ce)
		return -ENOMEM;
	error = mb_cache_entry_insert(ce, bh->b_bdev, bh->b_blocknr, hash);
	if (error) {
		mb_cache_entry_free(ce);
		if (error == -EBUSY) {
			ea_bdebug(bh, "already in cache (%d cache entries)",
				atomic_read(&ext2_xattr_cache->c_entry_count));
			error = 0;
		}
	} else {
		ea_bdebug(bh, "inserting [%x] (%d cache entries)", (int)hash,
			  atomic_read(&ext2_xattr_cache->c_entry_count));
		mb_cache_entry_release(ce);
	}
	return error;
}

static int
ext2_xattr_cmp(struct ext2_xattr_header *header1,
	       struct ext2_xattr_header *header2)
{
	struct ext2_xattr_entry *entry1, *entry2;

	entry1 = ENTRY(header1+1);
	entry2 = ENTRY(header2+1);
	while (!IS_LAST_ENTRY(entry1)) {
		if (IS_LAST_ENTRY(entry2))
			return 1;
		if (entry1->e_hash != entry2->e_hash ||
		    entry1->e_name_index != entry2->e_name_index ||
		    entry1->e_name_len != entry2->e_name_len ||
		    entry1->e_value_size != entry2->e_value_size ||
		    memcmp(entry1->e_name, entry2->e_name, entry1->e_name_len))
			return 1;
		if (entry1->e_value_block != 0 || entry2->e_value_block != 0)
			return -EIO;
		if (memcmp((char *)header1 + le16_to_cpu(entry1->e_value_offs),
			   (char *)header2 + le16_to_cpu(entry2->e_value_offs),
			   le32_to_cpu(entry1->e_value_size)))
			return 1;

		entry1 = EXT2_XATTR_NEXT(entry1);
		entry2 = EXT2_XATTR_NEXT(entry2);
	}
	if (!IS_LAST_ENTRY(entry2))
		return 1;
	return 0;
}

static struct buffer_head *
ext2_xattr_cache_find(struct inode *inode, struct ext2_xattr_header *header)
{
	__u32 hash = le32_to_cpu(header->h_hash);
	struct mb_cache_entry *ce;

	if (!header->h_hash)
		return NULL;  
	ea_idebug(inode, "looking for cached blocks [%x]", (int)hash);
again:
	ce = mb_cache_entry_find_first(ext2_xattr_cache, inode->i_sb->s_bdev,
				       hash);
	while (ce) {
		struct buffer_head *bh;

		if (IS_ERR(ce)) {
			if (PTR_ERR(ce) == -EAGAIN)
				goto again;
			break;
		}

		bh = sb_bread(inode->i_sb, ce->e_block);
		if (!bh) {
			ext2_error(inode->i_sb, "ext2_xattr_cache_find",
				"inode %ld: block %ld read error",
				inode->i_ino, (unsigned long) ce->e_block);
		} else {
			lock_buffer(bh);
			if (le32_to_cpu(HDR(bh)->h_refcount) >
				   EXT2_XATTR_REFCOUNT_MAX) {
				ea_idebug(inode, "block %ld refcount %d>%d",
					  (unsigned long) ce->e_block,
					  le32_to_cpu(HDR(bh)->h_refcount),
					  EXT2_XATTR_REFCOUNT_MAX);
			} else if (!ext2_xattr_cmp(header, HDR(bh))) {
				ea_bdebug(bh, "b_count=%d",
					  atomic_read(&(bh->b_count)));
				mb_cache_entry_release(ce);
				return bh;
			}
			unlock_buffer(bh);
			brelse(bh);
		}
		ce = mb_cache_entry_find_next(ce, inode->i_sb->s_bdev, hash);
	}
	return NULL;
}

#define NAME_HASH_SHIFT 5
#define VALUE_HASH_SHIFT 16

static inline void ext2_xattr_hash_entry(struct ext2_xattr_header *header,
					 struct ext2_xattr_entry *entry)
{
	__u32 hash = 0;
	char *name = entry->e_name;
	int n;

	for (n=0; n < entry->e_name_len; n++) {
		hash = (hash << NAME_HASH_SHIFT) ^
		       (hash >> (8*sizeof(hash) - NAME_HASH_SHIFT)) ^
		       *name++;
	}

	if (entry->e_value_block == 0 && entry->e_value_size != 0) {
		__le32 *value = (__le32 *)((char *)header +
			le16_to_cpu(entry->e_value_offs));
		for (n = (le32_to_cpu(entry->e_value_size) +
		     EXT2_XATTR_ROUND) >> EXT2_XATTR_PAD_BITS; n; n--) {
			hash = (hash << VALUE_HASH_SHIFT) ^
			       (hash >> (8*sizeof(hash) - VALUE_HASH_SHIFT)) ^
			       le32_to_cpu(*value++);
		}
	}
	entry->e_hash = cpu_to_le32(hash);
}

#undef NAME_HASH_SHIFT
#undef VALUE_HASH_SHIFT

#define BLOCK_HASH_SHIFT 16

static void ext2_xattr_rehash(struct ext2_xattr_header *header,
			      struct ext2_xattr_entry *entry)
{
	struct ext2_xattr_entry *here;
	__u32 hash = 0;
	
	ext2_xattr_hash_entry(header, entry);
	here = ENTRY(header+1);
	while (!IS_LAST_ENTRY(here)) {
		if (!here->e_hash) {
			
			hash = 0;
			break;
		}
		hash = (hash << BLOCK_HASH_SHIFT) ^
		       (hash >> (8*sizeof(hash) - BLOCK_HASH_SHIFT)) ^
		       le32_to_cpu(here->e_hash);
		here = EXT2_XATTR_NEXT(here);
	}
	header->h_hash = cpu_to_le32(hash);
}

#undef BLOCK_HASH_SHIFT

int __init
init_ext2_xattr(void)
{
	ext2_xattr_cache = mb_cache_create("ext2_xattr", 6);
	if (!ext2_xattr_cache)
		return -ENOMEM;
	return 0;
}

void
exit_ext2_xattr(void)
{
	mb_cache_destroy(ext2_xattr_cache);
}
