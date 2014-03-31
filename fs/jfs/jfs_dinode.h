/*
 *   Copyright (C) International Business Machines Corp., 2000-2001
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
#ifndef _H_JFS_DINODE
#define _H_JFS_DINODE


#define INODESLOTSIZE		128
#define L2INODESLOTSIZE		7
#define log2INODESIZE		9	


struct dinode {
	__le32 di_inostamp;	
	__le32 di_fileset;	
	__le32 di_number;	
	__le32 di_gen;		

	pxd_t di_ixpxd;		

	__le64 di_size;		
	__le64 di_nblocks;	

	__le32 di_nlink;	

	__le32 di_uid;		
	__le32 di_gid;		

	__le32 di_mode;		

	struct timestruc_t di_atime;	
	struct timestruc_t di_ctime;	
	struct timestruc_t di_mtime;	
	struct timestruc_t di_otime;	

	dxd_t di_acl;		

	dxd_t di_ea;		

	__le32 di_next_index;	

	__le32 di_acltype;	

	union {
		struct {
			struct dir_table_slot _table[12]; 

			dtroot_t _dtroot;		
		} _dir;					
#define di_dirtable	u._dir._table
#define di_dtroot	u._dir._dtroot
#define di_parent	di_dtroot.header.idotdot
#define di_DASD		di_dtroot.header.DASD

		struct {
			union {
				u8 _data[96];		
				struct {
					void *_imap;	
					__le32 _gengen;	
				} _imap;
			} _u1;				
#define di_gengen	u._file._u1._imap._gengen

			union {
				xtpage_t _xtroot;
				struct {
					u8 unused[16];	
					dxd_t _dxd;	
					union {
						__le32 _rdev;	
						u8 _fastsymlink[128];
					} _u;
					u8 _inlineea[128];
				} _special;
			} _u2;
		} _file;
#define di_xtroot	u._file._u2._xtroot
#define di_dxd		u._file._u2._special._dxd
#define di_btroot	di_xtroot
#define di_inlinedata	u._file._u2._special._u
#define di_rdev		u._file._u2._special._u._rdev
#define di_fastsymlink	u._file._u2._special._u._fastsymlink
#define di_inlineea	u._file._u2._special._inlineea
	} u;
};

#define IFJOURNAL	0x00010000	
#define ISPARSE		0x00020000	
#define INLINEEA	0x00040000	
#define ISWAPFILE	0x00800000	

#define IREADONLY	0x02000000	
#define IHIDDEN		0x04000000	
#define ISYSTEM		0x08000000	

#define IDIRECTORY	0x20000000	
#define IARCHIVE	0x40000000	
#define INEWNAME	0x80000000	

#define IRASH		0x4E000000	
#define ATTRSHIFT	25	


#define JFS_NOATIME_FL		0x00080000 

#define JFS_DIRSYNC_FL		0x00100000 
#define JFS_SYNC_FL		0x00200000 
#define JFS_SECRM_FL		0x00400000 
#define JFS_UNRM_FL		0x00800000 

#define JFS_APPEND_FL		0x01000000 
#define JFS_IMMUTABLE_FL	0x02000000 

#define JFS_FL_USER_VISIBLE	0x03F80000
#define JFS_FL_USER_MODIFIABLE	0x03F80000
#define JFS_FL_INHERIT		0x03C80000

#define JFS_IOC_GETFLAGS	_IOR('f', 1, long)
#define JFS_IOC_SETFLAGS	_IOW('f', 2, long)

#define JFS_IOC_GETFLAGS32	_IOR('f', 1, int)
#define JFS_IOC_SETFLAGS32	_IOW('f', 2, int)

#endif 
