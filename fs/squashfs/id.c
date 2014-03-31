/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * id.c
 */


#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/slab.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs.h"

int squashfs_get_id(struct super_block *sb, unsigned int index,
					unsigned int *id)
{
	struct squashfs_sb_info *msblk = sb->s_fs_info;
	int block = SQUASHFS_ID_BLOCK(index);
	int offset = SQUASHFS_ID_BLOCK_OFFSET(index);
	u64 start_block = le64_to_cpu(msblk->id_table[block]);
	__le32 disk_id;
	int err;

	err = squashfs_read_metadata(sb, &disk_id, &start_block, &offset,
							sizeof(disk_id));
	if (err < 0)
		return err;

	*id = le32_to_cpu(disk_id);
	return 0;
}


__le64 *squashfs_read_id_index_table(struct super_block *sb,
		u64 id_table_start, u64 next_table, unsigned short no_ids)
{
	unsigned int length = SQUASHFS_ID_BLOCK_BYTES(no_ids);
	__le64 *table;

	TRACE("In read_id_index_table, length %d\n", length);

	

	
	if (no_ids == 0)
		return ERR_PTR(-EINVAL);

	if (id_table_start + length > next_table)
		return ERR_PTR(-EINVAL);

	table = squashfs_read_table(sb, id_table_start, length);

	if (!IS_ERR(table) && le64_to_cpu(table[0]) >= id_table_start) {
		kfree(table);
		return ERR_PTR(-EINVAL);
	}

	return table;
}
