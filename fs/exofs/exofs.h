/*
 * Copyright (C) 2005, 2006
 * Avishay Traeger (avishay@gmail.com)
 * Copyright (C) 2008, 2009
 * Boaz Harrosh <bharrosh@panasas.com>
 *
 * Copyrights for code taken from ext2:
 *     Copyright (C) 1992, 1993, 1994, 1995
 *     Remy Card (card@masi.ibp.fr)
 *     Laboratoire MASI - Institut Blaise Pascal
 *     Universite Pierre et Marie Curie (Paris VI)
 *     from
 *     linux/fs/minix/inode.c
 *     Copyright (C) 1991, 1992  Linus Torvalds
 *
 * This file is part of exofs.
 *
 * exofs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.  Since it is based on ext2, and the only
 * valid version of GPL for the Linux kernel is version 2, the only valid
 * version of GPL for exofs is version 2.
 *
 * exofs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with exofs; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __EXOFS_H__
#define __EXOFS_H__

#include <linux/fs.h>
#include <linux/time.h>
#include <linux/backing-dev.h>
#include <scsi/osd_ore.h>

#include "common.h"

#define EXOFS_ERR(fmt, a...) printk(KERN_ERR "exofs: " fmt, ##a)

#ifdef CONFIG_EXOFS_DEBUG
#define EXOFS_DBGMSG(fmt, a...) \
	printk(KERN_NOTICE "exofs @%s:%d: " fmt, __func__, __LINE__, ##a)
#else
#define EXOFS_DBGMSG(fmt, a...) \
	do { if (0) printk(fmt, ##a); } while (0)
#endif

#define _LLU(x) (unsigned long long)(x)

struct exofs_dev {
	struct ore_dev ored;
	unsigned did;
};
struct exofs_sb_info {
	struct backing_dev_info bdi;		
	struct exofs_sb_stats s_ess;		/* Written often, pre-allocate*/
	int		s_timeout;		
	uint64_t	s_nextid;		
	uint32_t	s_numfiles;		
	spinlock_t	s_next_gen_lock;	
	u32		s_next_generation;	
	atomic_t	s_curr_pending;		

	struct ore_layout	layout;		
	struct ore_comp one_comp;		
	struct ore_components oc;		
};

struct exofs_i_info {
	struct inode   vfs_inode;          
	wait_queue_head_t i_wq;            
	unsigned long  i_flags;            
	uint32_t       i_data[EXOFS_IDATA];
	uint32_t       i_dir_start_lookup; 
	uint64_t       i_commit_size;      /* the object's written length     */
	struct ore_comp one_comp;	   
	struct ore_components oc;	   
};

static inline osd_id exofs_oi_objno(struct exofs_i_info *oi)
{
	return oi->vfs_inode.i_ino + EXOFS_OBJ_OFF;
}

#define OBJ_2BCREATED	0	
#define OBJ_CREATED	1	

static inline int obj_2bcreated(struct exofs_i_info *oi)
{
	return test_bit(OBJ_2BCREATED, &oi->i_flags);
}

static inline void set_obj_2bcreated(struct exofs_i_info *oi)
{
	set_bit(OBJ_2BCREATED, &oi->i_flags);
}

static inline int obj_created(struct exofs_i_info *oi)
{
	return test_bit(OBJ_CREATED, &oi->i_flags);
}

static inline void set_obj_created(struct exofs_i_info *oi)
{
	set_bit(OBJ_CREATED, &oi->i_flags);
}

int __exofs_wait_obj_created(struct exofs_i_info *oi);
static inline int wait_obj_created(struct exofs_i_info *oi)
{
	if (likely(obj_created(oi)))
		return 0;

	return __exofs_wait_obj_created(oi);
}

static inline struct exofs_i_info *exofs_i(struct inode *inode)
{
	return container_of(inode, struct exofs_i_info, vfs_inode);
}

#define EXOFS_LINK_MAX           32000


unsigned exofs_max_io_pages(struct ore_layout *layout,
			    unsigned expected_pages);
int exofs_setattr(struct dentry *, struct iattr *);
int exofs_write_begin(struct file *file, struct address_space *mapping,
		loff_t pos, unsigned len, unsigned flags,
		struct page **pagep, void **fsdata);
extern struct inode *exofs_iget(struct super_block *, unsigned long);
struct inode *exofs_new_inode(struct inode *, umode_t);
extern int exofs_write_inode(struct inode *, struct writeback_control *wbc);
extern void exofs_evict_inode(struct inode *);

int exofs_add_link(struct dentry *, struct inode *);
ino_t exofs_inode_by_name(struct inode *, struct dentry *);
int exofs_delete_entry(struct exofs_dir_entry *, struct page *);
int exofs_make_empty(struct inode *, struct inode *);
struct exofs_dir_entry *exofs_find_entry(struct inode *, struct dentry *,
					 struct page **);
int exofs_empty_dir(struct inode *);
struct exofs_dir_entry *exofs_dotdot(struct inode *, struct page **);
ino_t exofs_parent_ino(struct dentry *child);
int exofs_set_link(struct inode *, struct exofs_dir_entry *, struct page *,
		    struct inode *);

void exofs_make_credential(u8 cred_a[OSD_CAP_LEN],
			   const struct osd_obj_id *obj);
int exofs_sbi_write_stats(struct exofs_sb_info *sbi);

extern const struct file_operations exofs_dir_operations;

extern const struct inode_operations exofs_file_inode_operations;
extern const struct file_operations exofs_file_operations;

extern const struct address_space_operations exofs_aops;

extern const struct inode_operations exofs_dir_inode_operations;
extern const struct inode_operations exofs_special_inode_operations;

extern const struct inode_operations exofs_symlink_inode_operations;
extern const struct inode_operations exofs_fast_symlink_inode_operations;

static inline void exofs_init_comps(struct ore_components *oc,
				    struct ore_comp *one_comp,
				    struct exofs_sb_info *sbi, osd_id oid)
{
	unsigned dev_mod = (unsigned)oid, first_dev;

	one_comp->obj.partition = sbi->one_comp.obj.partition;
	one_comp->obj.id = oid;
	exofs_make_credential(one_comp->cred, &one_comp->obj);

	oc->first_dev = 0;
	oc->numdevs = sbi->layout.group_width * sbi->layout.mirrors_p1 *
							sbi->layout.group_count;
	oc->single_comp = EC_SINGLE_COMP;
	oc->comps = one_comp;

	
	first_dev = (dev_mod * sbi->layout.mirrors_p1) % sbi->oc.numdevs;
	oc->ods = &sbi->oc.ods[first_dev];
}

#endif
