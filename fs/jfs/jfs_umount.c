/*
 *   Copyright (C) International Business Machines Corp., 2000-2004
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#include <linux/fs.h>
#include "jfs_incore.h"
#include "jfs_filsys.h"
#include "jfs_superblock.h"
#include "jfs_dmap.h"
#include "jfs_imap.h"
#include "jfs_metapage.h"
#include "jfs_debug.h"

int jfs_umount(struct super_block *sb)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct inode *ipbmap = sbi->ipbmap;
	struct inode *ipimap = sbi->ipimap;
	struct inode *ipaimap = sbi->ipaimap;
	struct inode *ipaimap2 = sbi->ipaimap2;
	struct jfs_log *log;
	int rc = 0;

	jfs_info("UnMount JFS: sb:0x%p", sb);

	if ((log = sbi->log))
		/*
		 * Wait for outstanding transactions to be written to log:
		 */
		jfs_flush_journal(log, 2);

	diUnmount(ipimap, 0);

	diFreeSpecial(ipimap);
	sbi->ipimap = NULL;

	ipaimap2 = sbi->ipaimap2;
	if (ipaimap2) {
		diUnmount(ipaimap2, 0);
		diFreeSpecial(ipaimap2);
		sbi->ipaimap2 = NULL;
	}

	ipaimap = sbi->ipaimap;
	diUnmount(ipaimap, 0);
	diFreeSpecial(ipaimap);
	sbi->ipaimap = NULL;

	dbUnmount(ipbmap, 0);

	diFreeSpecial(ipbmap);
	sbi->ipimap = NULL;

	filemap_write_and_wait(sbi->direct_inode->i_mapping);

	if (log) {		
		updateSuper(sb, FM_CLEAN);

		rc = lmLogClose(sb);
	}
	jfs_info("UnMount JFS Complete: rc = %d", rc);
	return rc;
}


int jfs_umount_rw(struct super_block *sb)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct jfs_log *log = sbi->log;

	if (!log)
		return 0;

	jfs_flush_journal(log, 2);

	dbSync(sbi->ipbmap);
	diSync(sbi->ipimap);

	filemap_write_and_wait(sbi->direct_inode->i_mapping);

	updateSuper(sb, FM_CLEAN);

	return lmLogClose(sb);
}
