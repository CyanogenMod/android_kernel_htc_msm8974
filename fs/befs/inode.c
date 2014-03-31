/*
 * inode.c
 * 
 * Copyright (C) 2001 Will Dyson <will_dyson@pobox.com>
 */

#include <linux/fs.h>

#include "befs.h"
#include "inode.h"

int
befs_check_inode(struct super_block *sb, befs_inode * raw_inode,
		 befs_blocknr_t inode)
{
	u32 magic1 = fs32_to_cpu(sb, raw_inode->magic1);
	befs_inode_addr ino_num = fsrun_to_cpu(sb, raw_inode->inode_num);
	u32 flags = fs32_to_cpu(sb, raw_inode->flags);

	
	if (magic1 != BEFS_INODE_MAGIC1) {
		befs_error(sb,
			   "Inode has a bad magic header - inode = %lu", inode);
		return BEFS_BAD_INODE;
	}

	if (inode != iaddr2blockno(sb, &ino_num)) {
		befs_error(sb, "inode blocknr field disagrees with vfs "
			   "VFS: %lu, Inode %lu",
			   inode, iaddr2blockno(sb, &ino_num));
		return BEFS_BAD_INODE;
	}


	if (!(flags & BEFS_INODE_IN_USE)) {
		befs_error(sb, "inode is not used - inode = %lu", inode);
		return BEFS_BAD_INODE;
	}

	return BEFS_OK;
}
