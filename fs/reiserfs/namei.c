/*
 * Copyright 2000 by Hans Reiser, licensing governed by reiserfs/README
 *
 * Trivial changes by Alan Cox to remove EHASHCOLLISION for compatibility
 *
 * Trivial Changes:
 * Rights granted to Hans Reiser to redistribute under other terms providing
 * he accepts all liability including but not limited to patent, fitness
 * for purpose, and direct or indirect claims arising from failure to perform.
 *
 * NO WARRANTY
 */

#include <linux/time.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include "reiserfs.h"
#include "acl.h"
#include "xattr.h"
#include <linux/quotaops.h>

#define INC_DIR_INODE_NLINK(i) if (i->i_nlink != 1) { inc_nlink(i); if (i->i_nlink >= REISERFS_LINK_MAX) set_nlink(i, 1); }
#define DEC_DIR_INODE_NLINK(i) if (i->i_nlink != 1) drop_nlink(i);

static int bin_search_in_dir_item(struct reiserfs_dir_entry *de, loff_t off)
{
	struct item_head *ih = de->de_ih;
	struct reiserfs_de_head *deh = de->de_deh;
	int rbound, lbound, j;

	lbound = 0;
	rbound = I_ENTRY_COUNT(ih) - 1;

	for (j = (rbound + lbound) / 2; lbound <= rbound;
	     j = (rbound + lbound) / 2) {
		if (off < deh_offset(deh + j)) {
			rbound = j - 1;
			continue;
		}
		if (off > deh_offset(deh + j)) {
			lbound = j + 1;
			continue;
		}
		
		de->de_entry_num = j;
		return NAME_FOUND;
	}

	de->de_entry_num = lbound;
	return NAME_NOT_FOUND;
}

static inline void set_de_item_location(struct reiserfs_dir_entry *de,
					struct treepath *path)
{
	de->de_bh = get_last_bh(path);
	de->de_ih = get_ih(path);
	de->de_deh = B_I_DEH(de->de_bh, de->de_ih);
	de->de_item_num = PATH_LAST_POSITION(path);
}

inline void set_de_name_and_namelen(struct reiserfs_dir_entry *de)
{
	struct reiserfs_de_head *deh = de->de_deh + de->de_entry_num;

	BUG_ON(de->de_entry_num >= ih_entry_count(de->de_ih));

	de->de_entrylen = entry_length(de->de_bh, de->de_ih, de->de_entry_num);
	de->de_namelen = de->de_entrylen - (de_with_sd(deh) ? SD_SIZE : 0);
	de->de_name = B_I_PITEM(de->de_bh, de->de_ih) + deh_location(deh);
	if (de->de_name[de->de_namelen - 1] == 0)
		de->de_namelen = strlen(de->de_name);
}

static inline void set_de_object_key(struct reiserfs_dir_entry *de)
{
	BUG_ON(de->de_entry_num >= ih_entry_count(de->de_ih));
	de->de_dir_id = deh_dir_id(&(de->de_deh[de->de_entry_num]));
	de->de_objectid = deh_objectid(&(de->de_deh[de->de_entry_num]));
}

static inline void store_de_entry_key(struct reiserfs_dir_entry *de)
{
	struct reiserfs_de_head *deh = de->de_deh + de->de_entry_num;

	BUG_ON(de->de_entry_num >= ih_entry_count(de->de_ih));

	
	de->de_entry_key.version = KEY_FORMAT_3_5;
	de->de_entry_key.on_disk_key.k_dir_id =
	    le32_to_cpu(de->de_ih->ih_key.k_dir_id);
	de->de_entry_key.on_disk_key.k_objectid =
	    le32_to_cpu(de->de_ih->ih_key.k_objectid);
	set_cpu_key_k_offset(&(de->de_entry_key), deh_offset(deh));
	set_cpu_key_k_type(&(de->de_entry_key), TYPE_DIRENTRY);
}


int search_by_entry_key(struct super_block *sb, const struct cpu_key *key,
			struct treepath *path, struct reiserfs_dir_entry *de)
{
	int retval;

	retval = search_item(sb, key, path);
	switch (retval) {
	case ITEM_NOT_FOUND:
		if (!PATH_LAST_POSITION(path)) {
			reiserfs_error(sb, "vs-7000", "search_by_key "
				       "returned item position == 0");
			pathrelse(path);
			return IO_ERROR;
		}
		PATH_LAST_POSITION(path)--;

	case ITEM_FOUND:
		break;

	case IO_ERROR:
		return retval;

	default:
		pathrelse(path);
		reiserfs_error(sb, "vs-7002", "no path to here");
		return IO_ERROR;
	}

	set_de_item_location(de, path);

#ifdef CONFIG_REISERFS_CHECK
	if (!is_direntry_le_ih(de->de_ih) ||
	    COMP_SHORT_KEYS(&(de->de_ih->ih_key), key)) {
		print_block(de->de_bh, 0, -1, -1);
		reiserfs_panic(sb, "vs-7005", "found item %h is not directory "
			       "item or does not belong to the same directory "
			       "as key %K", de->de_ih, key);
	}
#endif				

	retval = bin_search_in_dir_item(de, cpu_key_k_offset(key));
	path->pos_in_item = de->de_entry_num;
	if (retval != NAME_NOT_FOUND) {
		
		set_de_name_and_namelen(de);
		set_de_object_key(de);
	}
	return retval;
}



static __u32 get_third_component(struct super_block *s,
				 const char *name, int len)
{
	__u32 res;

	if (!len || (len == 1 && name[0] == '.'))
		return DOT_OFFSET;
	if (len == 2 && name[0] == '.' && name[1] == '.')
		return DOT_DOT_OFFSET;

	res = REISERFS_SB(s)->s_hash_function(name, len);

	
	res = GET_HASH_VALUE(res);
	if (res == 0)
		
		
		res = 128;
	return res + MAX_GENERATION_NUMBER;
}

static int reiserfs_match(struct reiserfs_dir_entry *de,
			  const char *name, int namelen)
{
	int retval = NAME_NOT_FOUND;

	if ((namelen == de->de_namelen) &&
	    !memcmp(de->de_name, name, de->de_namelen))
		retval =
		    (de_visible(de->de_deh + de->de_entry_num) ? NAME_FOUND :
		     NAME_FOUND_INVISIBLE);

	return retval;
}


				

static int linear_search_in_dir_item(struct cpu_key *key,
				     struct reiserfs_dir_entry *de,
				     const char *name, int namelen)
{
	struct reiserfs_de_head *deh = de->de_deh;
	int retval;
	int i;

	i = de->de_entry_num;

	if (i == I_ENTRY_COUNT(de->de_ih) ||
	    GET_HASH_VALUE(deh_offset(deh + i)) !=
	    GET_HASH_VALUE(cpu_key_k_offset(key))) {
		i--;
	}

	RFALSE(de->de_deh != B_I_DEH(de->de_bh, de->de_ih),
	       "vs-7010: array of entry headers not found");

	deh += i;

	for (; i >= 0; i--, deh--) {
		if (GET_HASH_VALUE(deh_offset(deh)) !=
		    GET_HASH_VALUE(cpu_key_k_offset(key))) {
			
			return NAME_NOT_FOUND;
		}

		
		if (de->de_gen_number_bit_string)
			set_bit(GET_GENERATION_NUMBER(deh_offset(deh)),
				de->de_gen_number_bit_string);

		
		de->de_entry_num = i;
		set_de_name_and_namelen(de);

		if ((retval =
		     reiserfs_match(de, name, namelen)) != NAME_NOT_FOUND) {
			

			
			set_de_object_key(de);

			store_de_entry_key(de);

			
			return retval;
		}
	}

	if (GET_GENERATION_NUMBER(le_ih_k_offset(de->de_ih)) == 0)
		
		
		
		return NAME_NOT_FOUND;

	RFALSE(de->de_item_num,
	       "vs-7015: two diritems of the same directory in one node?");

	return GOTO_PREVIOUS_ITEM;
}

static int reiserfs_find_entry(struct inode *dir, const char *name, int namelen,
			       struct treepath *path_to_entry,
			       struct reiserfs_dir_entry *de)
{
	struct cpu_key key_to_search;
	int retval;

	if (namelen > REISERFS_MAX_NAME(dir->i_sb->s_blocksize))
		return NAME_NOT_FOUND;

	
	make_cpu_key(&key_to_search, dir,
		     get_third_component(dir->i_sb, name, namelen),
		     TYPE_DIRENTRY, 3);

	while (1) {
		retval =
		    search_by_entry_key(dir->i_sb, &key_to_search,
					path_to_entry, de);
		if (retval == IO_ERROR) {
			reiserfs_error(dir->i_sb, "zam-7001", "io error");
			return IO_ERROR;
		}

		
		retval =
		    linear_search_in_dir_item(&key_to_search, de, name,
					      namelen);
		if (retval != GOTO_PREVIOUS_ITEM) {
			
			path_to_entry->pos_in_item = de->de_entry_num;
			return retval;
		}

		
		set_cpu_key_k_offset(&key_to_search,
				     le_ih_k_offset(de->de_ih) - 1);
		pathrelse(path_to_entry);

	}			
}

static struct dentry *reiserfs_lookup(struct inode *dir, struct dentry *dentry,
				      struct nameidata *nd)
{
	int retval;
	int lock_depth;
	struct inode *inode = NULL;
	struct reiserfs_dir_entry de;
	INITIALIZE_PATH(path_to_entry);

	if (REISERFS_MAX_NAME(dir->i_sb->s_blocksize) < dentry->d_name.len)
		return ERR_PTR(-ENAMETOOLONG);

	lock_depth = reiserfs_write_lock_once(dir->i_sb);

	de.de_gen_number_bit_string = NULL;
	retval =
	    reiserfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len,
				&path_to_entry, &de);
	pathrelse(&path_to_entry);
	if (retval == NAME_FOUND) {
		inode = reiserfs_iget(dir->i_sb,
				      (struct cpu_key *)&(de.de_dir_id));
		if (!inode || IS_ERR(inode)) {
			reiserfs_write_unlock_once(dir->i_sb, lock_depth);
			return ERR_PTR(-EACCES);
		}

		if (IS_PRIVATE(dir))
			inode->i_flags |= S_PRIVATE;
	}
	reiserfs_write_unlock_once(dir->i_sb, lock_depth);
	if (retval == IO_ERROR) {
		return ERR_PTR(-EIO);
	}

	return d_splice_alias(inode, dentry);
}

struct dentry *reiserfs_get_parent(struct dentry *child)
{
	int retval;
	struct inode *inode = NULL;
	struct reiserfs_dir_entry de;
	INITIALIZE_PATH(path_to_entry);
	struct inode *dir = child->d_inode;

	if (dir->i_nlink == 0) {
		return ERR_PTR(-ENOENT);
	}
	de.de_gen_number_bit_string = NULL;

	reiserfs_write_lock(dir->i_sb);
	retval = reiserfs_find_entry(dir, "..", 2, &path_to_entry, &de);
	pathrelse(&path_to_entry);
	if (retval != NAME_FOUND) {
		reiserfs_write_unlock(dir->i_sb);
		return ERR_PTR(-ENOENT);
	}
	inode = reiserfs_iget(dir->i_sb, (struct cpu_key *)&(de.de_dir_id));
	reiserfs_write_unlock(dir->i_sb);

	return d_obtain_alias(inode);
}


static int reiserfs_add_entry(struct reiserfs_transaction_handle *th,
			      struct inode *dir, const char *name, int namelen,
			      struct inode *inode, int visible)
{
	struct cpu_key entry_key;
	struct reiserfs_de_head *deh;
	INITIALIZE_PATH(path);
	struct reiserfs_dir_entry de;
	DECLARE_BITMAP(bit_string, MAX_GENERATION_NUMBER + 1);
	int gen_number;
	char small_buf[32 + DEH_SIZE];	
	char *buffer;
	int buflen, paste_size;
	int retval;

	BUG_ON(!th->t_trans_id);

	
	if (!namelen)
		return -EINVAL;

	if (namelen > REISERFS_MAX_NAME(dir->i_sb->s_blocksize))
		return -ENAMETOOLONG;

	
	make_cpu_key(&entry_key, dir,
		     get_third_component(dir->i_sb, name, namelen),
		     TYPE_DIRENTRY, 3);

	
	buflen = DEH_SIZE + ROUND_UP(namelen);
	if (buflen > sizeof(small_buf)) {
		buffer = kmalloc(buflen, GFP_NOFS);
		if (!buffer)
			return -ENOMEM;
	} else
		buffer = small_buf;

	paste_size =
	    (get_inode_sd_version(dir) ==
	     STAT_DATA_V1) ? (DEH_SIZE + namelen) : buflen;

	
	deh = (struct reiserfs_de_head *)buffer;
	deh->deh_location = 0;	
	put_deh_offset(deh, cpu_key_k_offset(&entry_key));
	deh->deh_state = 0;	
	
	deh->deh_dir_id = INODE_PKEY(inode)->k_dir_id;	
	deh->deh_objectid = INODE_PKEY(inode)->k_objectid;	

	
	memcpy((char *)(deh + 1), name, namelen);
	
	padd_item((char *)(deh + 1), ROUND_UP(namelen), namelen);

	
	mark_de_without_sd(deh);
	visible ? mark_de_visible(deh) : mark_de_hidden(deh);

	
	memset(bit_string, 0, sizeof(bit_string));
	de.de_gen_number_bit_string = bit_string;
	retval = reiserfs_find_entry(dir, name, namelen, &path, &de);
	if (retval != NAME_NOT_FOUND) {
		if (buffer != small_buf)
			kfree(buffer);
		pathrelse(&path);

		if (retval == IO_ERROR) {
			return -EIO;
		}

		if (retval != NAME_FOUND) {
			reiserfs_error(dir->i_sb, "zam-7002",
				       "reiserfs_find_entry() returned "
				       "unexpected value (%d)", retval);
		}

		return -EEXIST;
	}

	gen_number =
	    find_first_zero_bit(bit_string,
				MAX_GENERATION_NUMBER + 1);
	if (gen_number > MAX_GENERATION_NUMBER) {
		
		reiserfs_warning(dir->i_sb, "reiserfs-7010",
				 "Congratulations! we have got hash function "
				 "screwed up");
		if (buffer != small_buf)
			kfree(buffer);
		pathrelse(&path);
		return -EBUSY;
	}
	
	put_deh_offset(deh, SET_GENERATION_NUMBER(deh_offset(deh), gen_number));
	set_cpu_key_k_offset(&entry_key, deh_offset(deh));

	
	PROC_INFO_MAX(th->t_super, max_hash_collisions, gen_number);

	if (gen_number != 0) {	
		if (search_by_entry_key(dir->i_sb, &entry_key, &path, &de) !=
		    NAME_NOT_FOUND) {
			reiserfs_warning(dir->i_sb, "vs-7032",
					 "entry with this key (%K) already "
					 "exists", &entry_key);

			if (buffer != small_buf)
				kfree(buffer);
			pathrelse(&path);
			return -EBUSY;
		}
	}

	
	retval =
	    reiserfs_paste_into_item(th, &path, &entry_key, dir, buffer,
				     paste_size);
	if (buffer != small_buf)
		kfree(buffer);
	if (retval) {
		reiserfs_check_path(&path);
		return retval;
	}

	dir->i_size += paste_size;
	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;
	if (!S_ISDIR(inode->i_mode) && visible)
		
		reiserfs_update_sd(th, dir);

	reiserfs_check_path(&path);
	return 0;
}

static int drop_new_inode(struct inode *inode)
{
	dquot_drop(inode);
	make_bad_inode(inode);
	inode->i_flags |= S_NOQUOTA;
	iput(inode);
	return 0;
}

static int new_inode_init(struct inode *inode, struct inode *dir, umode_t mode)
{
	INODE_PKEY(inode)->k_objectid = 0;
	inode_init_owner(inode, dir, mode);
	dquot_initialize(inode);
	return 0;
}

static int reiserfs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
			   struct nameidata *nd)
{
	int retval;
	struct inode *inode;
	
	int jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 2 +
	    2 * (REISERFS_QUOTA_INIT_BLOCKS(dir->i_sb) +
		 REISERFS_QUOTA_TRANS_BLOCKS(dir->i_sb));
	struct reiserfs_transaction_handle th;
	struct reiserfs_security_handle security;

	dquot_initialize(dir);

	if (!(inode = new_inode(dir->i_sb))) {
		return -ENOMEM;
	}
	new_inode_init(inode, dir, mode);

	jbegin_count += reiserfs_cache_default_acl(dir);
	retval = reiserfs_security_init(dir, inode, &dentry->d_name, &security);
	if (retval < 0) {
		drop_new_inode(inode);
		return retval;
	}
	jbegin_count += retval;
	reiserfs_write_lock(dir->i_sb);

	retval = journal_begin(&th, dir->i_sb, jbegin_count);
	if (retval) {
		drop_new_inode(inode);
		goto out_failed;
	}

	retval =
	    reiserfs_new_inode(&th, dir, mode, NULL, 0  , dentry,
			       inode, &security);
	if (retval)
		goto out_failed;

	inode->i_op = &reiserfs_file_inode_operations;
	inode->i_fop = &reiserfs_file_operations;
	inode->i_mapping->a_ops = &reiserfs_address_space_operations;

	retval =
	    reiserfs_add_entry(&th, dir, dentry->d_name.name,
			       dentry->d_name.len, inode, 1  );
	if (retval) {
		int err;
		drop_nlink(inode);
		reiserfs_update_sd(&th, inode);
		err = journal_end(&th, dir->i_sb, jbegin_count);
		if (err)
			retval = err;
		unlock_new_inode(inode);
		iput(inode);
		goto out_failed;
	}
	reiserfs_update_inode_transaction(inode);
	reiserfs_update_inode_transaction(dir);

	d_instantiate(dentry, inode);
	unlock_new_inode(inode);
	retval = journal_end(&th, dir->i_sb, jbegin_count);

      out_failed:
	reiserfs_write_unlock(dir->i_sb);
	return retval;
}

static int reiserfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
			  dev_t rdev)
{
	int retval;
	struct inode *inode;
	struct reiserfs_transaction_handle th;
	struct reiserfs_security_handle security;
	
	int jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 3 +
	    2 * (REISERFS_QUOTA_INIT_BLOCKS(dir->i_sb) +
		 REISERFS_QUOTA_TRANS_BLOCKS(dir->i_sb));

	if (!new_valid_dev(rdev))
		return -EINVAL;

	dquot_initialize(dir);

	if (!(inode = new_inode(dir->i_sb))) {
		return -ENOMEM;
	}
	new_inode_init(inode, dir, mode);

	jbegin_count += reiserfs_cache_default_acl(dir);
	retval = reiserfs_security_init(dir, inode, &dentry->d_name, &security);
	if (retval < 0) {
		drop_new_inode(inode);
		return retval;
	}
	jbegin_count += retval;
	reiserfs_write_lock(dir->i_sb);

	retval = journal_begin(&th, dir->i_sb, jbegin_count);
	if (retval) {
		drop_new_inode(inode);
		goto out_failed;
	}

	retval =
	    reiserfs_new_inode(&th, dir, mode, NULL, 0  , dentry,
			       inode, &security);
	if (retval) {
		goto out_failed;
	}

	inode->i_op = &reiserfs_special_inode_operations;
	init_special_inode(inode, inode->i_mode, rdev);

	
	reiserfs_update_sd(&th, inode);

	reiserfs_update_inode_transaction(inode);
	reiserfs_update_inode_transaction(dir);

	retval =
	    reiserfs_add_entry(&th, dir, dentry->d_name.name,
			       dentry->d_name.len, inode, 1  );
	if (retval) {
		int err;
		drop_nlink(inode);
		reiserfs_update_sd(&th, inode);
		err = journal_end(&th, dir->i_sb, jbegin_count);
		if (err)
			retval = err;
		unlock_new_inode(inode);
		iput(inode);
		goto out_failed;
	}

	d_instantiate(dentry, inode);
	unlock_new_inode(inode);
	retval = journal_end(&th, dir->i_sb, jbegin_count);

      out_failed:
	reiserfs_write_unlock(dir->i_sb);
	return retval;
}

static int reiserfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int retval;
	struct inode *inode;
	struct reiserfs_transaction_handle th;
	struct reiserfs_security_handle security;
	int lock_depth;
	
	int jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 3 +
	    2 * (REISERFS_QUOTA_INIT_BLOCKS(dir->i_sb) +
		 REISERFS_QUOTA_TRANS_BLOCKS(dir->i_sb));

	dquot_initialize(dir);

#ifdef DISPLACE_NEW_PACKING_LOCALITIES
	
	REISERFS_I(dir)->new_packing_locality = 1;
#endif
	mode = S_IFDIR | mode;
	if (!(inode = new_inode(dir->i_sb))) {
		return -ENOMEM;
	}
	new_inode_init(inode, dir, mode);

	jbegin_count += reiserfs_cache_default_acl(dir);
	retval = reiserfs_security_init(dir, inode, &dentry->d_name, &security);
	if (retval < 0) {
		drop_new_inode(inode);
		return retval;
	}
	jbegin_count += retval;
	lock_depth = reiserfs_write_lock_once(dir->i_sb);

	retval = journal_begin(&th, dir->i_sb, jbegin_count);
	if (retval) {
		drop_new_inode(inode);
		goto out_failed;
	}

	INC_DIR_INODE_NLINK(dir)

	    retval = reiserfs_new_inode(&th, dir, mode, NULL  ,
					old_format_only(dir->i_sb) ?
					EMPTY_DIR_SIZE_V1 : EMPTY_DIR_SIZE,
					dentry, inode, &security);
	if (retval) {
		DEC_DIR_INODE_NLINK(dir)
		goto out_failed;
	}

	reiserfs_update_inode_transaction(inode);
	reiserfs_update_inode_transaction(dir);

	inode->i_op = &reiserfs_dir_inode_operations;
	inode->i_fop = &reiserfs_dir_operations;

	
	retval =
	    reiserfs_add_entry(&th, dir, dentry->d_name.name,
			       dentry->d_name.len, inode, 1  );
	if (retval) {
		int err;
		clear_nlink(inode);
		DEC_DIR_INODE_NLINK(dir);
		reiserfs_update_sd(&th, inode);
		err = journal_end(&th, dir->i_sb, jbegin_count);
		if (err)
			retval = err;
		unlock_new_inode(inode);
		iput(inode);
		goto out_failed;
	}
	
	reiserfs_update_sd(&th, dir);

	d_instantiate(dentry, inode);
	unlock_new_inode(inode);
	retval = journal_end(&th, dir->i_sb, jbegin_count);
out_failed:
	reiserfs_write_unlock_once(dir->i_sb, lock_depth);
	return retval;
}

static inline int reiserfs_empty_dir(struct inode *inode)
{
	if (inode->i_size != EMPTY_DIR_SIZE &&
	    inode->i_size != EMPTY_DIR_SIZE_V1) {
		return 0;
	}
	return 1;
}

static int reiserfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int retval, err;
	struct inode *inode;
	struct reiserfs_transaction_handle th;
	int jbegin_count;
	INITIALIZE_PATH(path);
	struct reiserfs_dir_entry de;

	jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 2 + 2 +
	    4 * REISERFS_QUOTA_TRANS_BLOCKS(dir->i_sb);

	dquot_initialize(dir);

	reiserfs_write_lock(dir->i_sb);
	retval = journal_begin(&th, dir->i_sb, jbegin_count);
	if (retval)
		goto out_rmdir;

	de.de_gen_number_bit_string = NULL;
	if ((retval =
	     reiserfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len,
				 &path, &de)) == NAME_NOT_FOUND) {
		retval = -ENOENT;
		goto end_rmdir;
	} else if (retval == IO_ERROR) {
		retval = -EIO;
		goto end_rmdir;
	}

	inode = dentry->d_inode;

	reiserfs_update_inode_transaction(inode);
	reiserfs_update_inode_transaction(dir);

	if (de.de_objectid != inode->i_ino) {
		
		
		retval = -EIO;
		goto end_rmdir;
	}
	if (!reiserfs_empty_dir(inode)) {
		retval = -ENOTEMPTY;
		goto end_rmdir;
	}

	
	retval = reiserfs_cut_from_item(&th, &path, &(de.de_entry_key), dir, NULL,	
					0  );
	if (retval < 0)
		goto end_rmdir;

	if (inode->i_nlink != 2 && inode->i_nlink != 1)
		reiserfs_error(inode->i_sb, "reiserfs-7040",
			       "empty directory has nlink != 2 (%d)",
			       inode->i_nlink);

	clear_nlink(inode);
	inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME_SEC;
	reiserfs_update_sd(&th, inode);

	DEC_DIR_INODE_NLINK(dir)
	    dir->i_size -= (DEH_SIZE + de.de_entrylen);
	reiserfs_update_sd(&th, dir);

	
	add_save_link(&th, inode, 0  );

	retval = journal_end(&th, dir->i_sb, jbegin_count);
	reiserfs_check_path(&path);
      out_rmdir:
	reiserfs_write_unlock(dir->i_sb);
	return retval;

      end_rmdir:
	pathrelse(&path);
	err = journal_end(&th, dir->i_sb, jbegin_count);
	reiserfs_write_unlock(dir->i_sb);
	return err ? err : retval;
}

static int reiserfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int retval, err;
	struct inode *inode;
	struct reiserfs_dir_entry de;
	INITIALIZE_PATH(path);
	struct reiserfs_transaction_handle th;
	int jbegin_count;
	unsigned long savelink;
	int depth;

	dquot_initialize(dir);

	inode = dentry->d_inode;

	jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 2 + 2 +
	    4 * REISERFS_QUOTA_TRANS_BLOCKS(dir->i_sb);

	depth = reiserfs_write_lock_once(dir->i_sb);
	retval = journal_begin(&th, dir->i_sb, jbegin_count);
	if (retval)
		goto out_unlink;

	de.de_gen_number_bit_string = NULL;
	if ((retval =
	     reiserfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len,
				 &path, &de)) == NAME_NOT_FOUND) {
		retval = -ENOENT;
		goto end_unlink;
	} else if (retval == IO_ERROR) {
		retval = -EIO;
		goto end_unlink;
	}

	reiserfs_update_inode_transaction(inode);
	reiserfs_update_inode_transaction(dir);

	if (de.de_objectid != inode->i_ino) {
		
		
		retval = -EIO;
		goto end_unlink;
	}

	if (!inode->i_nlink) {
		reiserfs_warning(inode->i_sb, "reiserfs-7042",
				 "deleting nonexistent file (%lu), %d",
				 inode->i_ino, inode->i_nlink);
		set_nlink(inode, 1);
	}

	drop_nlink(inode);

	savelink = inode->i_nlink;

	retval =
	    reiserfs_cut_from_item(&th, &path, &(de.de_entry_key), dir, NULL,
				   0);
	if (retval < 0) {
		inc_nlink(inode);
		goto end_unlink;
	}
	inode->i_ctime = CURRENT_TIME_SEC;
	reiserfs_update_sd(&th, inode);

	dir->i_size -= (de.de_entrylen + DEH_SIZE);
	dir->i_ctime = dir->i_mtime = CURRENT_TIME_SEC;
	reiserfs_update_sd(&th, dir);

	if (!savelink)
		
		add_save_link(&th, inode, 0  );

	retval = journal_end(&th, dir->i_sb, jbegin_count);
	reiserfs_check_path(&path);
	reiserfs_write_unlock_once(dir->i_sb, depth);
	return retval;

      end_unlink:
	pathrelse(&path);
	err = journal_end(&th, dir->i_sb, jbegin_count);
	reiserfs_check_path(&path);
	if (err)
		retval = err;
      out_unlink:
	reiserfs_write_unlock_once(dir->i_sb, depth);
	return retval;
}

static int reiserfs_symlink(struct inode *parent_dir,
			    struct dentry *dentry, const char *symname)
{
	int retval;
	struct inode *inode;
	char *name;
	int item_len;
	struct reiserfs_transaction_handle th;
	struct reiserfs_security_handle security;
	int mode = S_IFLNK | S_IRWXUGO;
	
	int jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 3 +
	    2 * (REISERFS_QUOTA_INIT_BLOCKS(parent_dir->i_sb) +
		 REISERFS_QUOTA_TRANS_BLOCKS(parent_dir->i_sb));

	dquot_initialize(parent_dir);

	if (!(inode = new_inode(parent_dir->i_sb))) {
		return -ENOMEM;
	}
	new_inode_init(inode, parent_dir, mode);

	retval = reiserfs_security_init(parent_dir, inode, &dentry->d_name,
					&security);
	if (retval < 0) {
		drop_new_inode(inode);
		return retval;
	}
	jbegin_count += retval;

	reiserfs_write_lock(parent_dir->i_sb);
	item_len = ROUND_UP(strlen(symname));
	if (item_len > MAX_DIRECT_ITEM_LEN(parent_dir->i_sb->s_blocksize)) {
		retval = -ENAMETOOLONG;
		drop_new_inode(inode);
		goto out_failed;
	}

	name = kmalloc(item_len, GFP_NOFS);
	if (!name) {
		drop_new_inode(inode);
		retval = -ENOMEM;
		goto out_failed;
	}
	memcpy(name, symname, strlen(symname));
	padd_item(name, item_len, strlen(symname));

	retval = journal_begin(&th, parent_dir->i_sb, jbegin_count);
	if (retval) {
		drop_new_inode(inode);
		kfree(name);
		goto out_failed;
	}

	retval =
	    reiserfs_new_inode(&th, parent_dir, mode, name, strlen(symname),
			       dentry, inode, &security);
	kfree(name);
	if (retval) {		
		goto out_failed;
	}

	reiserfs_update_inode_transaction(inode);
	reiserfs_update_inode_transaction(parent_dir);

	inode->i_op = &reiserfs_symlink_inode_operations;
	inode->i_mapping->a_ops = &reiserfs_address_space_operations;

	// must be sure this inode is written with this transaction
	
	

	retval = reiserfs_add_entry(&th, parent_dir, dentry->d_name.name,
				    dentry->d_name.len, inode, 1  );
	if (retval) {
		int err;
		drop_nlink(inode);
		reiserfs_update_sd(&th, inode);
		err = journal_end(&th, parent_dir->i_sb, jbegin_count);
		if (err)
			retval = err;
		unlock_new_inode(inode);
		iput(inode);
		goto out_failed;
	}

	d_instantiate(dentry, inode);
	unlock_new_inode(inode);
	retval = journal_end(&th, parent_dir->i_sb, jbegin_count);
      out_failed:
	reiserfs_write_unlock(parent_dir->i_sb);
	return retval;
}

static int reiserfs_link(struct dentry *old_dentry, struct inode *dir,
			 struct dentry *dentry)
{
	int retval;
	struct inode *inode = old_dentry->d_inode;
	struct reiserfs_transaction_handle th;
	
	int jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 3 +
	    2 * REISERFS_QUOTA_TRANS_BLOCKS(dir->i_sb);

	dquot_initialize(dir);

	reiserfs_write_lock(dir->i_sb);
	if (inode->i_nlink >= REISERFS_LINK_MAX) {
		
		reiserfs_write_unlock(dir->i_sb);
		return -EMLINK;
	}

	
	inc_nlink(inode);

	retval = journal_begin(&th, dir->i_sb, jbegin_count);
	if (retval) {
		drop_nlink(inode);
		reiserfs_write_unlock(dir->i_sb);
		return retval;
	}

	
	retval =
	    reiserfs_add_entry(&th, dir, dentry->d_name.name,
			       dentry->d_name.len, inode, 1  );

	reiserfs_update_inode_transaction(inode);
	reiserfs_update_inode_transaction(dir);

	if (retval) {
		int err;
		drop_nlink(inode);
		err = journal_end(&th, dir->i_sb, jbegin_count);
		reiserfs_write_unlock(dir->i_sb);
		return err ? err : retval;
	}

	inode->i_ctime = CURRENT_TIME_SEC;
	reiserfs_update_sd(&th, inode);

	ihold(inode);
	d_instantiate(dentry, inode);
	retval = journal_end(&th, dir->i_sb, jbegin_count);
	reiserfs_write_unlock(dir->i_sb);
	return retval;
}

static int de_still_valid(const char *name, int len,
			  struct reiserfs_dir_entry *de)
{
	struct reiserfs_dir_entry tmp = *de;

	
	set_de_name_and_namelen(&tmp);
	
	if (tmp.de_namelen != len || memcmp(name, de->de_name, len))
		return 0;
	return 1;
}

static int entry_points_to_object(const char *name, int len,
				  struct reiserfs_dir_entry *de,
				  struct inode *inode)
{
	if (!de_still_valid(name, len, de))
		return 0;

	if (inode) {
		if (!de_visible(de->de_deh + de->de_entry_num))
			reiserfs_panic(inode->i_sb, "vs-7042",
				       "entry must be visible");
		return (de->de_objectid == inode->i_ino) ? 1 : 0;
	}

	
	if (de_visible(de->de_deh + de->de_entry_num))
		reiserfs_panic(NULL, "vs-7043", "entry must be visible");

	return 1;
}

static void set_ino_in_dir_entry(struct reiserfs_dir_entry *de,
				 struct reiserfs_key *key)
{
	
	de->de_deh[de->de_entry_num].deh_dir_id = key->k_dir_id;
	de->de_deh[de->de_entry_num].deh_objectid = key->k_objectid;
}

static int reiserfs_rename(struct inode *old_dir, struct dentry *old_dentry,
			   struct inode *new_dir, struct dentry *new_dentry)
{
	int retval;
	INITIALIZE_PATH(old_entry_path);
	INITIALIZE_PATH(new_entry_path);
	INITIALIZE_PATH(dot_dot_entry_path);
	struct item_head new_entry_ih, old_entry_ih, dot_dot_ih;
	struct reiserfs_dir_entry old_de, new_de, dot_dot_de;
	struct inode *old_inode, *new_dentry_inode;
	struct reiserfs_transaction_handle th;
	int jbegin_count;
	umode_t old_inode_mode;
	unsigned long savelink = 1;
	struct timespec ctime;

	jbegin_count =
	    JOURNAL_PER_BALANCE_CNT * 3 + 5 +
	    4 * REISERFS_QUOTA_TRANS_BLOCKS(old_dir->i_sb);

	dquot_initialize(old_dir);
	dquot_initialize(new_dir);

	old_inode = old_dentry->d_inode;
	new_dentry_inode = new_dentry->d_inode;

	
	
	old_de.de_gen_number_bit_string = NULL;
	reiserfs_write_lock(old_dir->i_sb);
	retval =
	    reiserfs_find_entry(old_dir, old_dentry->d_name.name,
				old_dentry->d_name.len, &old_entry_path,
				&old_de);
	pathrelse(&old_entry_path);
	if (retval == IO_ERROR) {
		reiserfs_write_unlock(old_dir->i_sb);
		return -EIO;
	}

	if (retval != NAME_FOUND || old_de.de_objectid != old_inode->i_ino) {
		reiserfs_write_unlock(old_dir->i_sb);
		return -ENOENT;
	}

	old_inode_mode = old_inode->i_mode;
	if (S_ISDIR(old_inode_mode)) {
		
		
		

		if (new_dentry_inode) {
			if (!reiserfs_empty_dir(new_dentry_inode)) {
				reiserfs_write_unlock(old_dir->i_sb);
				return -ENOTEMPTY;
			}
		}

		dot_dot_de.de_gen_number_bit_string = NULL;
		retval =
		    reiserfs_find_entry(old_inode, "..", 2, &dot_dot_entry_path,
					&dot_dot_de);
		pathrelse(&dot_dot_entry_path);
		if (retval != NAME_FOUND) {
			reiserfs_write_unlock(old_dir->i_sb);
			return -EIO;
		}

		
		if (dot_dot_de.de_objectid != old_dir->i_ino) {
			reiserfs_write_unlock(old_dir->i_sb);
			return -EIO;
		}
	}

	retval = journal_begin(&th, old_dir->i_sb, jbegin_count);
	if (retval) {
		reiserfs_write_unlock(old_dir->i_sb);
		return retval;
	}

	
	retval =
	    reiserfs_add_entry(&th, new_dir, new_dentry->d_name.name,
			       new_dentry->d_name.len, old_inode, 0);
	if (retval == -EEXIST) {
		if (!new_dentry_inode) {
			reiserfs_panic(old_dir->i_sb, "vs-7050",
				       "new entry is found, new inode == 0");
		}
	} else if (retval) {
		int err = journal_end(&th, old_dir->i_sb, jbegin_count);
		reiserfs_write_unlock(old_dir->i_sb);
		return err ? err : retval;
	}

	reiserfs_update_inode_transaction(old_dir);
	reiserfs_update_inode_transaction(new_dir);

	reiserfs_update_inode_transaction(old_inode);

	if (new_dentry_inode)
		reiserfs_update_inode_transaction(new_dentry_inode);

	while (1) {
		
		if ((retval =
		     search_by_entry_key(new_dir->i_sb, &old_de.de_entry_key,
					 &old_entry_path,
					 &old_de)) != NAME_FOUND) {
			pathrelse(&old_entry_path);
			journal_end(&th, old_dir->i_sb, jbegin_count);
			reiserfs_write_unlock(old_dir->i_sb);
			return -EIO;
		}

		copy_item_head(&old_entry_ih, get_ih(&old_entry_path));

		reiserfs_prepare_for_journal(old_inode->i_sb, old_de.de_bh, 1);

		
		new_de.de_gen_number_bit_string = NULL;
		retval =
		    reiserfs_find_entry(new_dir, new_dentry->d_name.name,
					new_dentry->d_name.len, &new_entry_path,
					&new_de);
		
		
		if (retval != NAME_FOUND_INVISIBLE && retval != NAME_FOUND) {
			pathrelse(&new_entry_path);
			pathrelse(&old_entry_path);
			journal_end(&th, old_dir->i_sb, jbegin_count);
			reiserfs_write_unlock(old_dir->i_sb);
			return -EIO;
		}

		copy_item_head(&new_entry_ih, get_ih(&new_entry_path));

		reiserfs_prepare_for_journal(old_inode->i_sb, new_de.de_bh, 1);

		if (S_ISDIR(old_inode->i_mode)) {
			if ((retval =
			     search_by_entry_key(new_dir->i_sb,
						 &dot_dot_de.de_entry_key,
						 &dot_dot_entry_path,
						 &dot_dot_de)) != NAME_FOUND) {
				pathrelse(&dot_dot_entry_path);
				pathrelse(&new_entry_path);
				pathrelse(&old_entry_path);
				journal_end(&th, old_dir->i_sb, jbegin_count);
				reiserfs_write_unlock(old_dir->i_sb);
				return -EIO;
			}
			copy_item_head(&dot_dot_ih,
				       get_ih(&dot_dot_entry_path));
			
			reiserfs_prepare_for_journal(old_inode->i_sb,
						     dot_dot_de.de_bh, 1);
		}
		/* probably.  our rename needs to hold more
		 ** than one path at once.  The seals would
		 ** have to be written to deal with multi-path
		 ** issues -chris
		 */
		if (item_moved(&new_entry_ih, &new_entry_path) ||
		    !entry_points_to_object(new_dentry->d_name.name,
					    new_dentry->d_name.len,
					    &new_de, new_dentry_inode) ||
		    item_moved(&old_entry_ih, &old_entry_path) ||
		    !entry_points_to_object(old_dentry->d_name.name,
					    old_dentry->d_name.len,
					    &old_de, old_inode)) {
			reiserfs_restore_prepared_buffer(old_inode->i_sb,
							 new_de.de_bh);
			reiserfs_restore_prepared_buffer(old_inode->i_sb,
							 old_de.de_bh);
			if (S_ISDIR(old_inode_mode))
				reiserfs_restore_prepared_buffer(old_inode->
								 i_sb,
								 dot_dot_de.
								 de_bh);
			continue;
		}
		if (S_ISDIR(old_inode_mode)) {
			if (item_moved(&dot_dot_ih, &dot_dot_entry_path) ||
			    !entry_points_to_object("..", 2, &dot_dot_de,
						    old_dir)) {
				reiserfs_restore_prepared_buffer(old_inode->
								 i_sb,
								 old_de.de_bh);
				reiserfs_restore_prepared_buffer(old_inode->
								 i_sb,
								 new_de.de_bh);
				reiserfs_restore_prepared_buffer(old_inode->
								 i_sb,
								 dot_dot_de.
								 de_bh);
				continue;
			}
		}

		RFALSE(S_ISDIR(old_inode_mode) &&
		       !buffer_journal_prepared(dot_dot_de.de_bh), "");

		break;
	}


	mark_de_visible(new_de.de_deh + new_de.de_entry_num);
	set_ino_in_dir_entry(&new_de, INODE_PKEY(old_inode));
	journal_mark_dirty(&th, old_dir->i_sb, new_de.de_bh);

	mark_de_hidden(old_de.de_deh + old_de.de_entry_num);
	journal_mark_dirty(&th, old_dir->i_sb, old_de.de_bh);
	ctime = CURRENT_TIME_SEC;
	old_dir->i_ctime = old_dir->i_mtime = ctime;
	new_dir->i_ctime = new_dir->i_mtime = ctime;
	old_inode->i_ctime = ctime;

	if (new_dentry_inode) {
		
		if (S_ISDIR(new_dentry_inode->i_mode)) {
			clear_nlink(new_dentry_inode);
		} else {
			drop_nlink(new_dentry_inode);
		}
		new_dentry_inode->i_ctime = ctime;
		savelink = new_dentry_inode->i_nlink;
	}

	if (S_ISDIR(old_inode_mode)) {
		
		set_ino_in_dir_entry(&dot_dot_de, INODE_PKEY(new_dir));
		journal_mark_dirty(&th, new_dir->i_sb, dot_dot_de.de_bh);

		if (!new_dentry_inode)
			INC_DIR_INODE_NLINK(new_dir);

		
		DEC_DIR_INODE_NLINK(old_dir);
	}
	
	pathrelse(&new_entry_path);
	pathrelse(&dot_dot_entry_path);

	
	
	
	if (reiserfs_cut_from_item
	    (&th, &old_entry_path, &(old_de.de_entry_key), old_dir, NULL,
	     0) < 0)
		reiserfs_error(old_dir->i_sb, "vs-7060",
			       "couldn't not cut old name. Fsck later?");

	old_dir->i_size -= DEH_SIZE + old_de.de_entrylen;

	reiserfs_update_sd(&th, old_dir);
	reiserfs_update_sd(&th, new_dir);
	reiserfs_update_sd(&th, old_inode);

	if (new_dentry_inode) {
		if (savelink == 0)
			add_save_link(&th, new_dentry_inode,
				      0  );
		reiserfs_update_sd(&th, new_dentry_inode);
	}

	retval = journal_end(&th, old_dir->i_sb, jbegin_count);
	reiserfs_write_unlock(old_dir->i_sb);
	return retval;
}

const struct inode_operations reiserfs_dir_inode_operations = {
	
	.create = reiserfs_create,
	.lookup = reiserfs_lookup,
	.link = reiserfs_link,
	.unlink = reiserfs_unlink,
	.symlink = reiserfs_symlink,
	.mkdir = reiserfs_mkdir,
	.rmdir = reiserfs_rmdir,
	.mknod = reiserfs_mknod,
	.rename = reiserfs_rename,
	.setattr = reiserfs_setattr,
	.setxattr = reiserfs_setxattr,
	.getxattr = reiserfs_getxattr,
	.listxattr = reiserfs_listxattr,
	.removexattr = reiserfs_removexattr,
	.permission = reiserfs_permission,
	.get_acl = reiserfs_get_acl,
};

const struct inode_operations reiserfs_symlink_inode_operations = {
	.readlink = generic_readlink,
	.follow_link = page_follow_link_light,
	.put_link = page_put_link,
	.setattr = reiserfs_setattr,
	.setxattr = reiserfs_setxattr,
	.getxattr = reiserfs_getxattr,
	.listxattr = reiserfs_listxattr,
	.removexattr = reiserfs_removexattr,
	.permission = reiserfs_permission,
	.get_acl = reiserfs_get_acl,

};

const struct inode_operations reiserfs_special_inode_operations = {
	.setattr = reiserfs_setattr,
	.setxattr = reiserfs_setxattr,
	.getxattr = reiserfs_getxattr,
	.listxattr = reiserfs_listxattr,
	.removexattr = reiserfs_removexattr,
	.permission = reiserfs_permission,
	.get_acl = reiserfs_get_acl,
};
