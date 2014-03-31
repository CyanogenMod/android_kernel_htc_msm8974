/*
 *   Copyright (C) International Business Machines Corp., 2000-2004
 *   Portions Copyright (C) Christoph Hellwig, 2001-2002
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
#ifndef _H_JFS_INCORE
#define _H_JFS_INCORE

#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include "jfs_types.h"
#include "jfs_xtree.h"
#include "jfs_dtree.h"

#define JFS_SUPER_MAGIC 0x3153464a 

struct jfs_inode_info {
	int	fileset;	
	uint	mode2;		
	uint	saved_uid;	
	uint	saved_gid;	
	pxd_t	ixpxd;		
	dxd_t	acl;		
	dxd_t	ea;		
	time_t	otime;		
	uint	next_index;	
	int	acltype;	
	short	btorder;	
	short	btindex;	
	struct inode *ipimap;	
	unsigned long cflag;	
	u64	agstart;	
	u16	bxflag;		
	unchar	pad;
	signed char active_ag;	
	lid_t	blid;		
	lid_t	atlhead;	
	lid_t	atltail;	
	spinlock_t ag_lock;	
	struct list_head anon_inode_list; 
	struct rw_semaphore rdwrlock;
	struct mutex commit_mutex;
	
	struct rw_semaphore xattr_sem;
	lid_t	xtlid;		
	union {
		struct {
			xtpage_t _xtroot;	
			struct inomap *_imap;	
		} file;
		struct {
			struct dir_table_slot _table[12]; 
			dtroot_t _dtroot;	
		} dir;
		struct {
			unchar _unused[16];	
			dxd_t _dxd;		
			unchar _inline[128];	
			unchar _inline_ea[128];	
		} link;
	} u;
	u32 dev;	
	struct inode	vfs_inode;
};
#define i_xtroot u.file._xtroot
#define i_imap u.file._imap
#define i_dirtable u.dir._table
#define i_dtroot u.dir._dtroot
#define i_inline u.link._inline
#define i_inline_ea u.link._inline_ea

#define IREAD_LOCK(ip, subclass) \
	down_read_nested(&JFS_IP(ip)->rdwrlock, subclass)
#define IREAD_UNLOCK(ip)	up_read(&JFS_IP(ip)->rdwrlock)
#define IWRITE_LOCK(ip, subclass) \
	down_write_nested(&JFS_IP(ip)->rdwrlock, subclass)
#define IWRITE_UNLOCK(ip)	up_write(&JFS_IP(ip)->rdwrlock)

enum cflags {
	COMMIT_Nolink,		
	COMMIT_Inlineea,	
	COMMIT_Freewmap,	
	COMMIT_Dirty,		
	COMMIT_Dirtable,	
	COMMIT_Stale,		
	COMMIT_Synclist,	
};

enum commit_mutex_class
{
	COMMIT_MUTEX_PARENT,
	COMMIT_MUTEX_CHILD,
	COMMIT_MUTEX_SECOND_PARENT,	
	COMMIT_MUTEX_VICTIM		
};

enum rdwrlock_class
{
	RDWRLOCK_NORMAL,
	RDWRLOCK_IMAP,
	RDWRLOCK_DMAP
};

#define set_cflag(flag, ip)	set_bit(flag, &(JFS_IP(ip)->cflag))
#define clear_cflag(flag, ip)	clear_bit(flag, &(JFS_IP(ip)->cflag))
#define test_cflag(flag, ip)	test_bit(flag, &(JFS_IP(ip)->cflag))
#define test_and_clear_cflag(flag, ip) \
	test_and_clear_bit(flag, &(JFS_IP(ip)->cflag))
struct jfs_sb_info {
	struct super_block *sb;		
	unsigned long	mntflag;	
	struct inode	*ipbmap;	
	struct inode	*ipaimap;	
	struct inode	*ipaimap2;	
	struct inode	*ipimap;	
	struct jfs_log	*log;		
	struct list_head log_list;	
	short		bsize;		
	short		l2bsize;	
	short		nbperpage;	
	short		l2nbperpage;	
	short		l2niperblk;	
	dev_t		logdev;		
	uint		aggregate;	
	pxd_t		logpxd;		
	pxd_t		fsckpxd;	
	pxd_t		ait2;		
	char		uuid[16];	
	char		loguuid[16];	
	int		commit_state;	
	
	uint		gengen;		
	uint		inostamp;	

	
	struct bmap	*bmap;		
	struct nls_table *nls_tab;	
	struct inode *direct_inode;	
	uint		state;		
	unsigned long	flag;		
	uint		p_state;	
	uint		uid;		
	uint		gid;		
	uint		umask;		
};

#define IN_LAZYCOMMIT 1

static inline struct jfs_inode_info *JFS_IP(struct inode *inode)
{
	return list_entry(inode, struct jfs_inode_info, vfs_inode);
}

static inline int jfs_dirtable_inline(struct inode *inode)
{
	return (JFS_IP(inode)->next_index <= (MAX_INLINE_DIRTABLE_ENTRY + 1));
}

static inline struct jfs_sb_info *JFS_SBI(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline int isReadOnly(struct inode *inode)
{
	if (JFS_SBI(inode->i_sb)->log)
		return 0;
	return 1;
}
#endif 
