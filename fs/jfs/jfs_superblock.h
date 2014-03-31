/*
 *   Copyright (C) International Business Machines Corp., 2000-2003
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
#ifndef	_H_JFS_SUPERBLOCK
#define _H_JFS_SUPERBLOCK

#define JFS_MAGIC	"JFS1"	

#define JFS_VERSION	2	

#define LV_NAME_SIZE	11	

struct jfs_superblock {
	char s_magic[4];	
	__le32 s_version;	

	__le64 s_size;		
	__le32 s_bsize;		
	__le16 s_l2bsize;	
	__le16 s_l2bfactor;	
	__le32 s_pbsize;	
	__le16 s_l2pbsize;	
	__le16 pad;		

	__le32 s_agsize;	

	__le32 s_flag;		
	__le32 s_state;		
	__le32 s_compress;		

	pxd_t s_ait2;		

	pxd_t s_aim2;		
	__le32 s_logdev;		
	__le32 s_logserial;	
	pxd_t s_logpxd;		

	pxd_t s_fsckpxd;	

	struct timestruc_t s_time;	

	__le32 s_fsckloglen;	
	s8 s_fscklog;		
	char s_fpack[11];	

	
	__le64 s_xsize;		
	pxd_t s_xfsckpxd;	
	pxd_t s_xlogpxd;	
	

	char s_uuid[16];	
	char s_label[16];	
	char s_loguuid[16];	

};

extern int readSuper(struct super_block *, struct buffer_head **);
extern int updateSuper(struct super_block *, uint);
extern void jfs_error(struct super_block *, const char *, ...);
extern int jfs_mount(struct super_block *);
extern int jfs_mount_rw(struct super_block *, int);
extern int jfs_umount(struct super_block *);
extern int jfs_umount_rw(struct super_block *);
extern int jfs_extendfs(struct super_block *, s64, int);

extern struct task_struct *jfsIOthread;
extern struct task_struct *jfsSyncThread;

#endif 
