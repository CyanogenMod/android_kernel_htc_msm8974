/*
 * ceph_fs.h - Ceph constants and data types to share between kernel and
 * user space.
 *
 * Most types in this file are defined as little-endian, and are
 * primarily intended to describe data structures that pass over the
 * wire or that are stored on disk.
 *
 * LGPL2
 */

#ifndef CEPH_FS_H
#define CEPH_FS_H

#include "msgr.h"
#include "rados.h"

#define CEPH_OSD_PROTOCOL     8 
#define CEPH_MDS_PROTOCOL    12 
#define CEPH_MON_PROTOCOL     5 
#define CEPH_OSDC_PROTOCOL   24 
#define CEPH_MDSC_PROTOCOL   32 
#define CEPH_MONC_PROTOCOL   15 


#define CEPH_INO_ROOT  1
#define CEPH_INO_CEPH  2        

#define CEPH_MAX_MON   31


#define CEPH_FEATURE_UID            (1<<0)
#define CEPH_FEATURE_NOSRCADDR      (1<<1)
#define CEPH_FEATURE_MONCLOCKCHECK  (1<<2)
#define CEPH_FEATURE_FLOCK          (1<<3)
#define CEPH_FEATURE_SUBSCRIBE2     (1<<4)
#define CEPH_FEATURE_MONNAMES       (1<<5)
#define CEPH_FEATURE_RECONNECT_SEQ  (1<<6)
#define CEPH_FEATURE_DIRLAYOUTHASH  (1<<7)


struct ceph_file_layout {
	
	__le32 fl_stripe_unit;     
	__le32 fl_stripe_count;    
	__le32 fl_object_size;     
	__le32 fl_cas_hash;        

	
	__le32 fl_object_stripe_unit;  

	
	__le32 fl_pg_preferred; 
	__le32 fl_pg_pool;      
} __attribute__ ((packed));

#define CEPH_MIN_STRIPE_UNIT 65536

int ceph_file_layout_is_valid(const struct ceph_file_layout *layout);

struct ceph_dir_layout {
	__u8   dl_dir_hash;   
	__u8   dl_unused1;
	__u16  dl_unused2;
	__u32  dl_unused3;
} __attribute__ ((packed));

#define CEPH_CRYPTO_NONE 0x0
#define CEPH_CRYPTO_AES  0x1

#define CEPH_AES_IV "cephsageyudagreg"

#define CEPH_AUTH_UNKNOWN	0x0
#define CEPH_AUTH_NONE	 	0x1
#define CEPH_AUTH_CEPHX	 	0x2

#define CEPH_AUTH_UID_DEFAULT ((__u64) -1)




#define CEPH_MSG_SHUTDOWN               1
#define CEPH_MSG_PING                   2

#define CEPH_MSG_MON_MAP                4
#define CEPH_MSG_MON_GET_MAP            5
#define CEPH_MSG_STATFS                 13
#define CEPH_MSG_STATFS_REPLY           14
#define CEPH_MSG_MON_SUBSCRIBE          15
#define CEPH_MSG_MON_SUBSCRIBE_ACK      16
#define CEPH_MSG_AUTH			17
#define CEPH_MSG_AUTH_REPLY		18

#define CEPH_MSG_MDS_MAP                21

#define CEPH_MSG_CLIENT_SESSION         22
#define CEPH_MSG_CLIENT_RECONNECT       23

#define CEPH_MSG_CLIENT_REQUEST         24
#define CEPH_MSG_CLIENT_REQUEST_FORWARD 25
#define CEPH_MSG_CLIENT_REPLY           26
#define CEPH_MSG_CLIENT_CAPS            0x310
#define CEPH_MSG_CLIENT_LEASE           0x311
#define CEPH_MSG_CLIENT_SNAP            0x312
#define CEPH_MSG_CLIENT_CAPRELEASE      0x313

#define CEPH_MSG_POOLOP_REPLY           48
#define CEPH_MSG_POOLOP                 49


#define CEPH_MSG_OSD_MAP                41
#define CEPH_MSG_OSD_OP                 42
#define CEPH_MSG_OSD_OPREPLY            43
#define CEPH_MSG_WATCH_NOTIFY           44


enum {
  WATCH_NOTIFY				= 1, 
  WATCH_NOTIFY_COMPLETE			= 2, 
};


enum {
  POOL_OP_CREATE			= 0x01,
  POOL_OP_DELETE			= 0x02,
  POOL_OP_AUID_CHANGE			= 0x03,
  POOL_OP_CREATE_SNAP			= 0x11,
  POOL_OP_DELETE_SNAP			= 0x12,
  POOL_OP_CREATE_UNMANAGED_SNAP		= 0x21,
  POOL_OP_DELETE_UNMANAGED_SNAP		= 0x22,
};

struct ceph_mon_request_header {
	__le64 have_version;
	__le16 session_mon;
	__le64 session_mon_tid;
} __attribute__ ((packed));

struct ceph_mon_statfs {
	struct ceph_mon_request_header monhdr;
	struct ceph_fsid fsid;
} __attribute__ ((packed));

struct ceph_statfs {
	__le64 kb, kb_used, kb_avail;
	__le64 num_objects;
} __attribute__ ((packed));

struct ceph_mon_statfs_reply {
	struct ceph_fsid fsid;
	__le64 version;
	struct ceph_statfs st;
} __attribute__ ((packed));

const char *ceph_pool_op_name(int op);

struct ceph_mon_poolop {
	struct ceph_mon_request_header monhdr;
	struct ceph_fsid fsid;
	__le32 pool;
	__le32 op;
	__le64 auid;
	__le64 snapid;
	__le32 name_len;
} __attribute__ ((packed));

struct ceph_mon_poolop_reply {
	struct ceph_mon_request_header monhdr;
	struct ceph_fsid fsid;
	__le32 reply_code;
	__le32 epoch;
	char has_data;
	char data[0];
} __attribute__ ((packed));

struct ceph_mon_unmanaged_snap {
	__le64 snapid;
} __attribute__ ((packed));

struct ceph_osd_getmap {
	struct ceph_mon_request_header monhdr;
	struct ceph_fsid fsid;
	__le32 start;
} __attribute__ ((packed));

struct ceph_mds_getmap {
	struct ceph_mon_request_header monhdr;
	struct ceph_fsid fsid;
} __attribute__ ((packed));

struct ceph_client_mount {
	struct ceph_mon_request_header monhdr;
} __attribute__ ((packed));

#define CEPH_SUBSCRIBE_ONETIME    1  

struct ceph_mon_subscribe_item {
	__le64 have_version;    __le64 have;
	__u8 onetime;
} __attribute__ ((packed));

struct ceph_mon_subscribe_ack {
	__le32 duration;         
	struct ceph_fsid fsid;
} __attribute__ ((packed));

#define CEPH_MDS_STATE_DNE          0  
#define CEPH_MDS_STATE_STOPPED     -1  
#define CEPH_MDS_STATE_BOOT        -4  
#define CEPH_MDS_STATE_STANDBY     -5  
#define CEPH_MDS_STATE_CREATING    -6  
#define CEPH_MDS_STATE_STARTING    -7  
#define CEPH_MDS_STATE_STANDBY_REPLAY -8 

#define CEPH_MDS_STATE_REPLAY       8  
#define CEPH_MDS_STATE_RESOLVE      9  
#define CEPH_MDS_STATE_RECONNECT    10 
#define CEPH_MDS_STATE_REJOIN       11 
#define CEPH_MDS_STATE_CLIENTREPLAY 12 
#define CEPH_MDS_STATE_ACTIVE       13 
#define CEPH_MDS_STATE_STOPPING     14 

extern const char *ceph_mds_state_name(int s);


#define CEPH_LOCK_DVERSION    1
#define CEPH_LOCK_DN          2
#define CEPH_LOCK_ISNAP       16
#define CEPH_LOCK_IVERSION    32    
#define CEPH_LOCK_IFILE       64
#define CEPH_LOCK_IAUTH       128
#define CEPH_LOCK_ILINK       256
#define CEPH_LOCK_IDFT        512   
#define CEPH_LOCK_INEST       1024  
#define CEPH_LOCK_IXATTR      2048
#define CEPH_LOCK_IFLOCK      4096  
#define CEPH_LOCK_INO         8192  

enum {
	CEPH_SESSION_REQUEST_OPEN,
	CEPH_SESSION_OPEN,
	CEPH_SESSION_REQUEST_CLOSE,
	CEPH_SESSION_CLOSE,
	CEPH_SESSION_REQUEST_RENEWCAPS,
	CEPH_SESSION_RENEWCAPS,
	CEPH_SESSION_STALE,
	CEPH_SESSION_RECALL_STATE,
};

extern const char *ceph_session_op_name(int op);

struct ceph_mds_session_head {
	__le32 op;
	__le64 seq;
	struct ceph_timespec stamp;
	__le32 max_caps, max_leases;
} __attribute__ ((packed));

#define CEPH_MDS_OP_WRITE        0x001000
enum {
	CEPH_MDS_OP_LOOKUP     = 0x00100,
	CEPH_MDS_OP_GETATTR    = 0x00101,
	CEPH_MDS_OP_LOOKUPHASH = 0x00102,
	CEPH_MDS_OP_LOOKUPPARENT = 0x00103,
	CEPH_MDS_OP_LOOKUPINO  = 0x00104,

	CEPH_MDS_OP_SETXATTR   = 0x01105,
	CEPH_MDS_OP_RMXATTR    = 0x01106,
	CEPH_MDS_OP_SETLAYOUT  = 0x01107,
	CEPH_MDS_OP_SETATTR    = 0x01108,
	CEPH_MDS_OP_SETFILELOCK= 0x01109,
	CEPH_MDS_OP_GETFILELOCK= 0x00110,
	CEPH_MDS_OP_SETDIRLAYOUT=0x0110a,

	CEPH_MDS_OP_MKNOD      = 0x01201,
	CEPH_MDS_OP_LINK       = 0x01202,
	CEPH_MDS_OP_UNLINK     = 0x01203,
	CEPH_MDS_OP_RENAME     = 0x01204,
	CEPH_MDS_OP_MKDIR      = 0x01220,
	CEPH_MDS_OP_RMDIR      = 0x01221,
	CEPH_MDS_OP_SYMLINK    = 0x01222,

	CEPH_MDS_OP_CREATE     = 0x01301,
	CEPH_MDS_OP_OPEN       = 0x00302,
	CEPH_MDS_OP_READDIR    = 0x00305,

	CEPH_MDS_OP_LOOKUPSNAP = 0x00400,
	CEPH_MDS_OP_MKSNAP     = 0x01400,
	CEPH_MDS_OP_RMSNAP     = 0x01401,
	CEPH_MDS_OP_LSSNAP     = 0x00402,
};

extern const char *ceph_mds_op_name(int op);


#define CEPH_SETATTR_MODE   1
#define CEPH_SETATTR_UID    2
#define CEPH_SETATTR_GID    4
#define CEPH_SETATTR_MTIME  8
#define CEPH_SETATTR_ATIME 16
#define CEPH_SETATTR_SIZE  32
#define CEPH_SETATTR_CTIME 64

union ceph_mds_request_args {
	struct {
		__le32 mask;                 
	} __attribute__ ((packed)) getattr;
	struct {
		__le32 mode;
		__le32 uid;
		__le32 gid;
		struct ceph_timespec mtime;
		struct ceph_timespec atime;
		__le64 size, old_size;       
		__le32 mask;                 
	} __attribute__ ((packed)) setattr;
	struct {
		__le32 frag;                 
		__le32 max_entries;          
		__le32 max_bytes;
	} __attribute__ ((packed)) readdir;
	struct {
		__le32 mode;
		__le32 rdev;
	} __attribute__ ((packed)) mknod;
	struct {
		__le32 mode;
	} __attribute__ ((packed)) mkdir;
	struct {
		__le32 flags;
		__le32 mode;
		__le32 stripe_unit;          
		__le32 stripe_count;         
		__le32 object_size;
		__le32 file_replication;
		__le32 preferred;
	} __attribute__ ((packed)) open;
	struct {
		__le32 flags;
	} __attribute__ ((packed)) setxattr;
	struct {
		struct ceph_file_layout layout;
	} __attribute__ ((packed)) setlayout;
	struct {
		__u8 rule; 
		__u8 type; 
		__le64 pid; 
		__le64 pid_namespace;
		__le64 start; 
		__le64 length; 
		__u8 wait; 
	} __attribute__ ((packed)) filelock_change;
} __attribute__ ((packed));

#define CEPH_MDS_FLAG_REPLAY        1  
#define CEPH_MDS_FLAG_WANT_DENTRY   2  

struct ceph_mds_request_head {
	__le64 oldest_client_tid;
	__le32 mdsmap_epoch;           
	__le32 flags;                  
	__u8 num_retry, num_fwd;       
	__le16 num_releases;           
	__le32 op;                     
	__le32 caller_uid, caller_gid;
	__le64 ino;                    
	union ceph_mds_request_args args;
} __attribute__ ((packed));

struct ceph_mds_request_release {
	__le64 ino, cap_id;            
	__le32 caps, wanted;           
	__le32 seq, issue_seq, mseq;
	__le32 dname_seq;              
	__le32 dname_len;              
} __attribute__ ((packed));

struct ceph_mds_reply_head {
	__le32 op;
	__le32 result;
	__le32 mdsmap_epoch;
	__u8 safe;                     
	__u8 is_dentry, is_target;     
} __attribute__ ((packed));

struct ceph_frag_tree_split {
	__le32 frag;                   
	__le32 by;                     
} __attribute__ ((packed));

struct ceph_frag_tree_head {
	__le32 nsplits;                
	struct ceph_frag_tree_split splits[];
} __attribute__ ((packed));

struct ceph_mds_reply_cap {
	__le32 caps, wanted;           
	__le64 cap_id;
	__le32 seq, mseq;
	__le64 realm;                  
	__u8 flags;                    
} __attribute__ ((packed));

#define CEPH_CAP_FLAG_AUTH  1          

struct ceph_mds_reply_inode {
	__le64 ino;
	__le64 snapid;
	__le32 rdev;
	__le64 version;                
	__le64 xattr_version;          
	struct ceph_mds_reply_cap cap; 
	struct ceph_file_layout layout;
	struct ceph_timespec ctime, mtime, atime;
	__le32 time_warp_seq;
	__le64 size, max_size, truncate_size;
	__le32 truncate_seq;
	__le32 mode, uid, gid;
	__le32 nlink;
	__le64 files, subdirs, rbytes, rfiles, rsubdirs;  
	struct ceph_timespec rctime;
	struct ceph_frag_tree_head fragtree;  
} __attribute__ ((packed));

struct ceph_mds_reply_lease {
	__le16 mask;            
	__le32 duration_ms;     
	__le32 seq;
} __attribute__ ((packed));

struct ceph_mds_reply_dirfrag {
	__le32 frag;            
	__le32 auth;            
	__le32 ndist;           
	__le32 dist[];
} __attribute__ ((packed));

#define CEPH_LOCK_FCNTL    1
#define CEPH_LOCK_FLOCK    2

#define CEPH_LOCK_SHARED   1
#define CEPH_LOCK_EXCL     2
#define CEPH_LOCK_UNLOCK   4

struct ceph_filelock {
	__le64 start;
	__le64 length; 
	__le64 client; 
	__le64 pid; 
	__le64 pid_namespace;
	__u8 type; 
} __attribute__ ((packed));


#define CEPH_FILE_MODE_PIN        0
#define CEPH_FILE_MODE_RD         1
#define CEPH_FILE_MODE_WR         2
#define CEPH_FILE_MODE_RDWR       3  
#define CEPH_FILE_MODE_LAZY       4  
#define CEPH_FILE_MODE_NUM        8  

int ceph_flags_to_mode(int flags);


#define CEPH_CAP_PIN         1  

#define CEPH_CAP_GSHARED     1  
#define CEPH_CAP_GEXCL       2  
#define CEPH_CAP_GCACHE      4  
#define CEPH_CAP_GRD         8  
#define CEPH_CAP_GWR        16  
#define CEPH_CAP_GBUFFER    32  
#define CEPH_CAP_GWREXTEND  64  
#define CEPH_CAP_GLAZYIO   128  

#define CEPH_CAP_SAUTH      2
#define CEPH_CAP_SLINK      4
#define CEPH_CAP_SXATTR     6
#define CEPH_CAP_SFILE      8
#define CEPH_CAP_SFLOCK    20 

#define CEPH_CAP_BITS       22

#define CEPH_CAP_AUTH_SHARED  (CEPH_CAP_GSHARED  << CEPH_CAP_SAUTH)
#define CEPH_CAP_AUTH_EXCL     (CEPH_CAP_GEXCL     << CEPH_CAP_SAUTH)
#define CEPH_CAP_LINK_SHARED  (CEPH_CAP_GSHARED  << CEPH_CAP_SLINK)
#define CEPH_CAP_LINK_EXCL     (CEPH_CAP_GEXCL     << CEPH_CAP_SLINK)
#define CEPH_CAP_XATTR_SHARED (CEPH_CAP_GSHARED  << CEPH_CAP_SXATTR)
#define CEPH_CAP_XATTR_EXCL    (CEPH_CAP_GEXCL     << CEPH_CAP_SXATTR)
#define CEPH_CAP_FILE(x)    (x << CEPH_CAP_SFILE)
#define CEPH_CAP_FILE_SHARED   (CEPH_CAP_GSHARED   << CEPH_CAP_SFILE)
#define CEPH_CAP_FILE_EXCL     (CEPH_CAP_GEXCL     << CEPH_CAP_SFILE)
#define CEPH_CAP_FILE_CACHE    (CEPH_CAP_GCACHE    << CEPH_CAP_SFILE)
#define CEPH_CAP_FILE_RD       (CEPH_CAP_GRD       << CEPH_CAP_SFILE)
#define CEPH_CAP_FILE_WR       (CEPH_CAP_GWR       << CEPH_CAP_SFILE)
#define CEPH_CAP_FILE_BUFFER   (CEPH_CAP_GBUFFER   << CEPH_CAP_SFILE)
#define CEPH_CAP_FILE_WREXTEND (CEPH_CAP_GWREXTEND << CEPH_CAP_SFILE)
#define CEPH_CAP_FILE_LAZYIO   (CEPH_CAP_GLAZYIO   << CEPH_CAP_SFILE)
#define CEPH_CAP_FLOCK_SHARED  (CEPH_CAP_GSHARED   << CEPH_CAP_SFLOCK)
#define CEPH_CAP_FLOCK_EXCL    (CEPH_CAP_GEXCL     << CEPH_CAP_SFLOCK)


#define CEPH_STAT_CAP_INODE    CEPH_CAP_PIN
#define CEPH_STAT_CAP_TYPE     CEPH_CAP_PIN  
#define CEPH_STAT_CAP_SYMLINK  CEPH_CAP_PIN
#define CEPH_STAT_CAP_UID      CEPH_CAP_AUTH_SHARED
#define CEPH_STAT_CAP_GID      CEPH_CAP_AUTH_SHARED
#define CEPH_STAT_CAP_MODE     CEPH_CAP_AUTH_SHARED
#define CEPH_STAT_CAP_NLINK    CEPH_CAP_LINK_SHARED
#define CEPH_STAT_CAP_LAYOUT   CEPH_CAP_FILE_SHARED
#define CEPH_STAT_CAP_MTIME    CEPH_CAP_FILE_SHARED
#define CEPH_STAT_CAP_SIZE     CEPH_CAP_FILE_SHARED
#define CEPH_STAT_CAP_ATIME    CEPH_CAP_FILE_SHARED  
#define CEPH_STAT_CAP_XATTR    CEPH_CAP_XATTR_SHARED
#define CEPH_STAT_CAP_INODE_ALL (CEPH_CAP_PIN |			\
				 CEPH_CAP_AUTH_SHARED |	\
				 CEPH_CAP_LINK_SHARED |	\
				 CEPH_CAP_FILE_SHARED |	\
				 CEPH_CAP_XATTR_SHARED)

#define CEPH_CAP_ANY_SHARED (CEPH_CAP_AUTH_SHARED |			\
			      CEPH_CAP_LINK_SHARED |			\
			      CEPH_CAP_XATTR_SHARED |			\
			      CEPH_CAP_FILE_SHARED)
#define CEPH_CAP_ANY_RD   (CEPH_CAP_ANY_SHARED | CEPH_CAP_FILE_RD |	\
			   CEPH_CAP_FILE_CACHE)

#define CEPH_CAP_ANY_EXCL (CEPH_CAP_AUTH_EXCL |		\
			   CEPH_CAP_LINK_EXCL |		\
			   CEPH_CAP_XATTR_EXCL |	\
			   CEPH_CAP_FILE_EXCL)
#define CEPH_CAP_ANY_FILE_WR (CEPH_CAP_FILE_WR | CEPH_CAP_FILE_BUFFER |	\
			      CEPH_CAP_FILE_EXCL)
#define CEPH_CAP_ANY_WR   (CEPH_CAP_ANY_EXCL | CEPH_CAP_ANY_FILE_WR)
#define CEPH_CAP_ANY      (CEPH_CAP_ANY_RD | CEPH_CAP_ANY_EXCL | \
			   CEPH_CAP_ANY_FILE_WR | CEPH_CAP_FILE_LAZYIO | \
			   CEPH_CAP_PIN)

#define CEPH_CAP_LOCKS (CEPH_LOCK_IFILE | CEPH_LOCK_IAUTH | CEPH_LOCK_ILINK | \
			CEPH_LOCK_IXATTR)

int ceph_caps_for_mode(int mode);

enum {
	CEPH_CAP_OP_GRANT,         
	CEPH_CAP_OP_REVOKE,        
	CEPH_CAP_OP_TRUNC,         
	CEPH_CAP_OP_EXPORT,        
	CEPH_CAP_OP_IMPORT,        
	CEPH_CAP_OP_UPDATE,        
	CEPH_CAP_OP_DROP,          
	CEPH_CAP_OP_FLUSH,         
	CEPH_CAP_OP_FLUSH_ACK,     
	CEPH_CAP_OP_FLUSHSNAP,     
	CEPH_CAP_OP_FLUSHSNAP_ACK, 
	CEPH_CAP_OP_RELEASE,       
	CEPH_CAP_OP_RENEW,         
};

extern const char *ceph_cap_op_name(int op);

struct ceph_mds_caps {
	__le32 op;                  
	__le64 ino, realm;
	__le64 cap_id;
	__le32 seq, issue_seq;
	__le32 caps, wanted, dirty; 
	__le32 migrate_seq;
	__le64 snap_follows;
	__le32 snap_trace_len;

	
	__le32 uid, gid, mode;

	
	__le32 nlink;

	
	__le32 xattr_len;
	__le64 xattr_version;

	
	__le64 size, max_size, truncate_size;
	__le32 truncate_seq;
	struct ceph_timespec mtime, atime, ctime;
	struct ceph_file_layout layout;
	__le32 time_warp_seq;
} __attribute__ ((packed));

struct ceph_mds_cap_release {
	__le32 num;                
} __attribute__ ((packed));

struct ceph_mds_cap_item {
	__le64 ino;
	__le64 cap_id;
	__le32 migrate_seq, seq;
} __attribute__ ((packed));

#define CEPH_MDS_LEASE_REVOKE           1  
#define CEPH_MDS_LEASE_RELEASE          2  
#define CEPH_MDS_LEASE_RENEW            3  
#define CEPH_MDS_LEASE_REVOKE_ACK       4  

extern const char *ceph_lease_op_name(int o);

struct ceph_mds_lease {
	__u8 action;            
	__le16 mask;            
	__le64 ino;
	__le64 first, last;     
	__le32 seq;
	__le32 duration_ms;     
} __attribute__ ((packed));

struct ceph_mds_cap_reconnect {
	__le64 cap_id;
	__le32 wanted;
	__le32 issued;
	__le64 snaprealm;
	__le64 pathbase;        
	__le32 flock_len;       
} __attribute__ ((packed));

struct ceph_mds_cap_reconnect_v1 {
	__le64 cap_id;
	__le32 wanted;
	__le32 issued;
	__le64 size;
	struct ceph_timespec mtime, atime;
	__le64 snaprealm;
	__le64 pathbase;        
} __attribute__ ((packed));

struct ceph_mds_snaprealm_reconnect {
	__le64 ino;     
	__le64 seq;     
	__le64 parent;  
} __attribute__ ((packed));

enum {
	CEPH_SNAP_OP_UPDATE,  
	CEPH_SNAP_OP_CREATE,
	CEPH_SNAP_OP_DESTROY,
	CEPH_SNAP_OP_SPLIT,
};

extern const char *ceph_snap_op_name(int o);

struct ceph_mds_snap_head {
	__le32 op;                
	__le64 split;             
	__le32 num_split_inos;    
	__le32 num_split_realms;  
	__le32 trace_len;         
} __attribute__ ((packed));

struct ceph_mds_snap_realm {
	__le64 ino;           
	__le64 created;       
	__le64 parent;        
	__le64 parent_since;  
	__le64 seq;           
	__le32 num_snaps;
	__le32 num_prior_parent_snaps;
} __attribute__ ((packed));

#endif
