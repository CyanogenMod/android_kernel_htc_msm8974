/*
 *   fs/cifs/cifs_fs_sb.h
 *
 *   Copyright (c) International Business Machines  Corp., 2002,2004
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 */
#include <linux/rbtree.h>

#ifndef _CIFS_FS_SB_H
#define _CIFS_FS_SB_H

#include <linux/backing-dev.h>

#define CIFS_MOUNT_NO_PERM      1 
#define CIFS_MOUNT_SET_UID      2 
#define CIFS_MOUNT_SERVER_INUM  4 
#define CIFS_MOUNT_DIRECT_IO    8 
#define CIFS_MOUNT_NO_XATTR     0x10  
#define CIFS_MOUNT_MAP_SPECIAL_CHR 0x20 
#define CIFS_MOUNT_POSIX_PATHS  0x40  
#define CIFS_MOUNT_UNX_EMUL     0x80  
#define CIFS_MOUNT_NO_BRL       0x100 
#define CIFS_MOUNT_CIFS_ACL     0x200 
#define CIFS_MOUNT_OVERR_UID    0x400 
#define CIFS_MOUNT_OVERR_GID    0x800 
#define CIFS_MOUNT_DYNPERM      0x1000 
#define CIFS_MOUNT_NOPOSIXBRL   0x2000 
#define CIFS_MOUNT_NOSSYNC      0x4000 
#define CIFS_MOUNT_FSCACHE	0x8000 
#define CIFS_MOUNT_MF_SYMLINKS	0x10000 
#define CIFS_MOUNT_MULTIUSER	0x20000 
#define CIFS_MOUNT_STRICT_IO	0x40000 
#define CIFS_MOUNT_RWPIDFORWARD	0x80000 
#define CIFS_MOUNT_POSIXACL	0x100000 
#define CIFS_MOUNT_CIFS_BACKUPUID 0x200000 
#define CIFS_MOUNT_CIFS_BACKUPGID 0x400000 

struct cifs_sb_info {
	struct rb_root tlink_tree;
	spinlock_t tlink_tree_lock;
	struct tcon_link *master_tlink;
	struct nls_table *local_nls;
	unsigned int rsize;
	unsigned int wsize;
	unsigned long actimeo; 
	atomic_t active;
	uid_t	mnt_uid;
	gid_t	mnt_gid;
	uid_t	mnt_backupuid;
	gid_t	mnt_backupgid;
	umode_t	mnt_file_mode;
	umode_t	mnt_dir_mode;
	unsigned int mnt_cifs_flags;
	char   *mountdata; 
	struct backing_dev_info bdi;
	struct delayed_work prune_tlinks;
};
#endif				
