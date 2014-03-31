/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright © 2001-2007 Red Hat, Inc.
 * Copyright © 2004-2010 David Woodhouse <dwmw2@infradead.org>
 *
 * Created by David Woodhouse <dwmw2@infradead.org>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 */

#ifndef _JFFS2_FS_I
#define _JFFS2_FS_I

#include <linux/rbtree.h>
#include <linux/posix_acl.h>
#include <linux/mutex.h>

struct jffs2_inode_info {
	struct mutex sem;

	
	uint32_t highest_version;

	
	struct rb_root fragtree;

	struct jffs2_full_dnode *metadata;

	
	struct jffs2_full_dirent *dents;

	
	unsigned char *target;

	
	struct jffs2_inode_cache *inocache;

	uint16_t flags;
	uint8_t usercompr;
	struct inode vfs_inode;
};

#endif 
