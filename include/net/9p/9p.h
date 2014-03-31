/*
 * include/net/9p/9p.h
 *
 * 9P protocol definitions.
 *
 *  Copyright (C) 2005 by Latchesar Ionkov <lucho@ionkov.net>
 *  Copyright (C) 2004 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 2002 by Ron Minnich <rminnich@lanl.gov>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#ifndef NET_9P_H
#define NET_9P_H


enum p9_debug_flags {
	P9_DEBUG_ERROR = 	(1<<0),
	P9_DEBUG_9P = 		(1<<2),
	P9_DEBUG_VFS =		(1<<3),
	P9_DEBUG_CONV =		(1<<4),
	P9_DEBUG_MUX =		(1<<5),
	P9_DEBUG_TRANS =	(1<<6),
	P9_DEBUG_SLABS =      	(1<<7),
	P9_DEBUG_FCALL =	(1<<8),
	P9_DEBUG_FID =		(1<<9),
	P9_DEBUG_PKT =		(1<<10),
	P9_DEBUG_FSC =		(1<<11),
	P9_DEBUG_VPKT =		(1<<12),
};

#ifdef CONFIG_NET_9P_DEBUG
extern unsigned int p9_debug_level;
__printf(3, 4)
void _p9_debug(enum p9_debug_flags level, const char *func,
	       const char *fmt, ...);
#define p9_debug(level, fmt, ...)			\
	_p9_debug(level, __func__, fmt, ##__VA_ARGS__)
#else
#define p9_debug(level, fmt, ...)			\
	no_printk(fmt, ##__VA_ARGS__)
#endif


enum p9_msg_t {
	P9_TLERROR = 6,
	P9_RLERROR,
	P9_TSTATFS = 8,
	P9_RSTATFS,
	P9_TLOPEN = 12,
	P9_RLOPEN,
	P9_TLCREATE = 14,
	P9_RLCREATE,
	P9_TSYMLINK = 16,
	P9_RSYMLINK,
	P9_TMKNOD = 18,
	P9_RMKNOD,
	P9_TRENAME = 20,
	P9_RRENAME,
	P9_TREADLINK = 22,
	P9_RREADLINK,
	P9_TGETATTR = 24,
	P9_RGETATTR,
	P9_TSETATTR = 26,
	P9_RSETATTR,
	P9_TXATTRWALK = 30,
	P9_RXATTRWALK,
	P9_TXATTRCREATE = 32,
	P9_RXATTRCREATE,
	P9_TREADDIR = 40,
	P9_RREADDIR,
	P9_TFSYNC = 50,
	P9_RFSYNC,
	P9_TLOCK = 52,
	P9_RLOCK,
	P9_TGETLOCK = 54,
	P9_RGETLOCK,
	P9_TLINK = 70,
	P9_RLINK,
	P9_TMKDIR = 72,
	P9_RMKDIR,
	P9_TRENAMEAT = 74,
	P9_RRENAMEAT,
	P9_TUNLINKAT = 76,
	P9_RUNLINKAT,
	P9_TVERSION = 100,
	P9_RVERSION,
	P9_TAUTH = 102,
	P9_RAUTH,
	P9_TATTACH = 104,
	P9_RATTACH,
	P9_TERROR = 106,
	P9_RERROR,
	P9_TFLUSH = 108,
	P9_RFLUSH,
	P9_TWALK = 110,
	P9_RWALK,
	P9_TOPEN = 112,
	P9_ROPEN,
	P9_TCREATE = 114,
	P9_RCREATE,
	P9_TREAD = 116,
	P9_RREAD,
	P9_TWRITE = 118,
	P9_RWRITE,
	P9_TCLUNK = 120,
	P9_RCLUNK,
	P9_TREMOVE = 122,
	P9_RREMOVE,
	P9_TSTAT = 124,
	P9_RSTAT,
	P9_TWSTAT = 126,
	P9_RWSTAT,
};


enum p9_open_mode_t {
	P9_OREAD = 0x00,
	P9_OWRITE = 0x01,
	P9_ORDWR = 0x02,
	P9_OEXEC = 0x03,
	P9_OTRUNC = 0x10,
	P9_OREXEC = 0x20,
	P9_ORCLOSE = 0x40,
	P9_OAPPEND = 0x80,
	P9_OEXCL = 0x1000,
};

enum p9_perm_t {
	P9_DMDIR = 0x80000000,
	P9_DMAPPEND = 0x40000000,
	P9_DMEXCL = 0x20000000,
	P9_DMMOUNT = 0x10000000,
	P9_DMAUTH = 0x08000000,
	P9_DMTMP = 0x04000000,
	P9_DMSYMLINK = 0x02000000,
	P9_DMLINK = 0x01000000,
	P9_DMDEVICE = 0x00800000,
	P9_DMNAMEDPIPE = 0x00200000,
	P9_DMSOCKET = 0x00100000,
	P9_DMSETUID = 0x00080000,
	P9_DMSETGID = 0x00040000,
	P9_DMSETVTX = 0x00010000,
};

#define P9_DOTL_RDONLY        00000000
#define P9_DOTL_WRONLY        00000001
#define P9_DOTL_RDWR          00000002
#define P9_DOTL_NOACCESS      00000003
#define P9_DOTL_CREATE        00000100
#define P9_DOTL_EXCL          00000200
#define P9_DOTL_NOCTTY        00000400
#define P9_DOTL_TRUNC         00001000
#define P9_DOTL_APPEND        00002000
#define P9_DOTL_NONBLOCK      00004000
#define P9_DOTL_DSYNC         00010000
#define P9_DOTL_FASYNC        00020000
#define P9_DOTL_DIRECT        00040000
#define P9_DOTL_LARGEFILE     00100000
#define P9_DOTL_DIRECTORY     00200000
#define P9_DOTL_NOFOLLOW      00400000
#define P9_DOTL_NOATIME       01000000
#define P9_DOTL_CLOEXEC       02000000
#define P9_DOTL_SYNC          04000000

#define P9_DOTL_AT_REMOVEDIR		0x200

#define P9_LOCK_TYPE_RDLCK 0
#define P9_LOCK_TYPE_WRLCK 1
#define P9_LOCK_TYPE_UNLCK 2

enum p9_qid_t {
	P9_QTDIR = 0x80,
	P9_QTAPPEND = 0x40,
	P9_QTEXCL = 0x20,
	P9_QTMOUNT = 0x10,
	P9_QTAUTH = 0x08,
	P9_QTTMP = 0x04,
	P9_QTSYMLINK = 0x02,
	P9_QTLINK = 0x01,
	P9_QTFILE = 0x00,
};

#define P9_NOTAG	(u16)(~0)
#define P9_NOFID	(u32)(~0)
#define P9_MAXWELEM	16

#define P9_IOHDRSZ	24

#define P9_READDIRHDRSZ	24

#define P9_ZC_HDR_SZ 4096


struct p9_qid {
	u8 type;
	u32 version;
	u64 path;
};


struct p9_wstat {
	u16 size;
	u16 type;
	u32 dev;
	struct p9_qid qid;
	u32 mode;
	u32 atime;
	u32 mtime;
	u64 length;
	char *name;
	char *uid;
	char *gid;
	char *muid;
	char *extension;	
	u32 n_uid;		
	u32 n_gid;		
	u32 n_muid;		
};

struct p9_stat_dotl {
	u64 st_result_mask;
	struct p9_qid qid;
	u32 st_mode;
	u32 st_uid;
	u32 st_gid;
	u64 st_nlink;
	u64 st_rdev;
	u64 st_size;
	u64 st_blksize;
	u64 st_blocks;
	u64 st_atime_sec;
	u64 st_atime_nsec;
	u64 st_mtime_sec;
	u64 st_mtime_nsec;
	u64 st_ctime_sec;
	u64 st_ctime_nsec;
	u64 st_btime_sec;
	u64 st_btime_nsec;
	u64 st_gen;
	u64 st_data_version;
};

#define P9_STATS_MODE		0x00000001ULL
#define P9_STATS_NLINK		0x00000002ULL
#define P9_STATS_UID		0x00000004ULL
#define P9_STATS_GID		0x00000008ULL
#define P9_STATS_RDEV		0x00000010ULL
#define P9_STATS_ATIME		0x00000020ULL
#define P9_STATS_MTIME		0x00000040ULL
#define P9_STATS_CTIME		0x00000080ULL
#define P9_STATS_INO		0x00000100ULL
#define P9_STATS_SIZE		0x00000200ULL
#define P9_STATS_BLOCKS		0x00000400ULL

#define P9_STATS_BTIME		0x00000800ULL
#define P9_STATS_GEN		0x00001000ULL
#define P9_STATS_DATA_VERSION	0x00002000ULL

#define P9_STATS_BASIC		0x000007ffULL 
#define P9_STATS_ALL		0x00003fffULL 


struct p9_iattr_dotl {
	u32 valid;
	u32 mode;
	u32 uid;
	u32 gid;
	u64 size;
	u64 atime_sec;
	u64 atime_nsec;
	u64 mtime_sec;
	u64 mtime_nsec;
};

#define P9_LOCK_SUCCESS 0
#define P9_LOCK_BLOCKED 1
#define P9_LOCK_ERROR 2
#define P9_LOCK_GRACE 3

#define P9_LOCK_FLAGS_BLOCK 1
#define P9_LOCK_FLAGS_RECLAIM 2


struct p9_flock {
	u8 type;
	u32 flags;
	u64 start;
	u64 length;
	u32 proc_id;
	char *client_id;
};


struct p9_getlock {
	u8 type;
	u64 start;
	u64 length;
	u32 proc_id;
	char *client_id;
};

struct p9_rstatfs {
	u32 type;
	u32 bsize;
	u64 blocks;
	u64 bfree;
	u64 bavail;
	u64 files;
	u64 ffree;
	u64 fsid;
	u32 namelen;
};


struct p9_fcall {
	u32 size;
	u8 id;
	u16 tag;

	size_t offset;
	size_t capacity;

	u8 *sdata;
};

struct p9_idpool;

int p9_errstr2errno(char *errstr, int len);

struct p9_idpool *p9_idpool_create(void);
void p9_idpool_destroy(struct p9_idpool *);
int p9_idpool_get(struct p9_idpool *p);
void p9_idpool_put(int id, struct p9_idpool *p);
int p9_idpool_check(int id, struct p9_idpool *p);

int p9_error_init(void);
int p9_trans_fd_init(void);
void p9_trans_fd_exit(void);
#endif 
