#ifndef CEPH_RADOS_H
#define CEPH_RADOS_H


#include "msgr.h"

#define CEPH_OSDMAP_INC_VERSION     5
#define CEPH_OSDMAP_INC_VERSION_EXT 6
#define CEPH_OSDMAP_VERSION         5
#define CEPH_OSDMAP_VERSION_EXT     6

struct ceph_fsid {
	unsigned char fsid[16];
};

static inline int ceph_fsid_compare(const struct ceph_fsid *a,
				    const struct ceph_fsid *b)
{
	return memcmp(a, b, sizeof(*a));
}

typedef __le64 ceph_snapid_t;
#define CEPH_SNAPDIR ((__u64)(-1))  
#define CEPH_NOSNAP  ((__u64)(-2))  
#define CEPH_MAXSNAP ((__u64)(-3))  

struct ceph_timespec {
	__le32 tv_sec;
	__le32 tv_nsec;
} __attribute__ ((packed));


#define CEPH_OBJECT_LAYOUT_HASH     1
#define CEPH_OBJECT_LAYOUT_LINEAR   2
#define CEPH_OBJECT_LAYOUT_HASHINO  3

#define CEPH_PG_LAYOUT_CRUSH  0
#define CEPH_PG_LAYOUT_HASH   1
#define CEPH_PG_LAYOUT_LINEAR 2
#define CEPH_PG_LAYOUT_HYBRID 3

#define CEPH_PG_MAX_SIZE      16  

struct ceph_pg {
	__le16 preferred; 
	__le16 ps;        
	__le32 pool;      
} __attribute__ ((packed));

#define CEPH_PG_TYPE_REP     1
#define CEPH_PG_TYPE_RAID4   2
#define CEPH_PG_POOL_VERSION 2
struct ceph_pg_pool {
	__u8 type;                
	__u8 size;                
	__u8 crush_ruleset;       
	__u8 object_hash;         
	__le32 pg_num, pgp_num;   
	__le32 lpg_num, lpgp_num; 
	__le32 last_change;       
	__le64 snap_seq;          
	__le32 snap_epoch;        
	__le32 num_snaps;
	__le32 num_removed_snap_intervals; 
	__le64 auid;               
} __attribute__ ((packed));

static inline int ceph_stable_mod(int x, int b, int bmask)
{
	if ((x & bmask) < b)
		return x & bmask;
	else
		return x & (bmask >> 1);
}

struct ceph_object_layout {
	struct ceph_pg ol_pgid;   
	__le32 ol_stripe_unit;    
} __attribute__ ((packed));

struct ceph_eversion {
	__le32 epoch;
	__le64 version;
} __attribute__ ((packed));


#define CEPH_OSD_EXISTS 1
#define CEPH_OSD_UP     2

#define CEPH_OSD_IN  0x10000
#define CEPH_OSD_OUT 0


#define CEPH_OSDMAP_NEARFULL (1<<0)  
#define CEPH_OSDMAP_FULL     (1<<1)  
#define CEPH_OSDMAP_PAUSERD  (1<<2)  
#define CEPH_OSDMAP_PAUSEWR  (1<<3)  
#define CEPH_OSDMAP_PAUSEREC (1<<4)  

#define CEPH_OSD_OP_MODE       0xf000
#define CEPH_OSD_OP_MODE_RD    0x1000
#define CEPH_OSD_OP_MODE_WR    0x2000
#define CEPH_OSD_OP_MODE_RMW   0x3000
#define CEPH_OSD_OP_MODE_SUB   0x4000

#define CEPH_OSD_OP_TYPE       0x0f00
#define CEPH_OSD_OP_TYPE_LOCK  0x0100
#define CEPH_OSD_OP_TYPE_DATA  0x0200
#define CEPH_OSD_OP_TYPE_ATTR  0x0300
#define CEPH_OSD_OP_TYPE_EXEC  0x0400
#define CEPH_OSD_OP_TYPE_PG    0x0500

enum {
	
	
	CEPH_OSD_OP_READ      = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_DATA | 1,
	CEPH_OSD_OP_STAT      = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_DATA | 2,
	CEPH_OSD_OP_MAPEXT    = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_DATA | 3,

	
	CEPH_OSD_OP_MASKTRUNC   = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_DATA | 4,
	CEPH_OSD_OP_SPARSE_READ = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_DATA | 5,

	CEPH_OSD_OP_NOTIFY    = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_DATA | 6,
	CEPH_OSD_OP_NOTIFY_ACK = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_DATA | 7,

	
	CEPH_OSD_OP_ASSERT_VER = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_DATA | 8,

	
	CEPH_OSD_OP_WRITE     = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 1,
	CEPH_OSD_OP_WRITEFULL = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 2,
	CEPH_OSD_OP_TRUNCATE  = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 3,
	CEPH_OSD_OP_ZERO      = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 4,
	CEPH_OSD_OP_DELETE    = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 5,

	
	CEPH_OSD_OP_APPEND    = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 6,
	CEPH_OSD_OP_STARTSYNC = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 7,
	CEPH_OSD_OP_SETTRUNC  = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 8,
	CEPH_OSD_OP_TRIMTRUNC = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 9,

	CEPH_OSD_OP_TMAPUP  = CEPH_OSD_OP_MODE_RMW | CEPH_OSD_OP_TYPE_DATA | 10,
	CEPH_OSD_OP_TMAPPUT = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 11,
	CEPH_OSD_OP_TMAPGET = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_DATA | 12,

	CEPH_OSD_OP_CREATE  = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 13,
	CEPH_OSD_OP_ROLLBACK= CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 14,

	CEPH_OSD_OP_WATCH   = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_DATA | 15,

	
	
	CEPH_OSD_OP_GETXATTR  = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_ATTR | 1,
	CEPH_OSD_OP_GETXATTRS = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_ATTR | 2,
	CEPH_OSD_OP_CMPXATTR  = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_ATTR | 3,

	
	CEPH_OSD_OP_SETXATTR  = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_ATTR | 1,
	CEPH_OSD_OP_SETXATTRS = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_ATTR | 2,
	CEPH_OSD_OP_RESETXATTRS = CEPH_OSD_OP_MODE_WR|CEPH_OSD_OP_TYPE_ATTR | 3,
	CEPH_OSD_OP_RMXATTR   = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_ATTR | 4,

	
	CEPH_OSD_OP_PULL            = CEPH_OSD_OP_MODE_SUB | 1,
	CEPH_OSD_OP_PUSH            = CEPH_OSD_OP_MODE_SUB | 2,
	CEPH_OSD_OP_BALANCEREADS    = CEPH_OSD_OP_MODE_SUB | 3,
	CEPH_OSD_OP_UNBALANCEREADS  = CEPH_OSD_OP_MODE_SUB | 4,
	CEPH_OSD_OP_SCRUB           = CEPH_OSD_OP_MODE_SUB | 5,
	CEPH_OSD_OP_SCRUB_RESERVE   = CEPH_OSD_OP_MODE_SUB | 6,
	CEPH_OSD_OP_SCRUB_UNRESERVE = CEPH_OSD_OP_MODE_SUB | 7,
	CEPH_OSD_OP_SCRUB_STOP      = CEPH_OSD_OP_MODE_SUB | 8,

	
	CEPH_OSD_OP_WRLOCK    = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_LOCK | 1,
	CEPH_OSD_OP_WRUNLOCK  = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_LOCK | 2,
	CEPH_OSD_OP_RDLOCK    = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_LOCK | 3,
	CEPH_OSD_OP_RDUNLOCK  = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_LOCK | 4,
	CEPH_OSD_OP_UPLOCK    = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_LOCK | 5,
	CEPH_OSD_OP_DNLOCK    = CEPH_OSD_OP_MODE_WR | CEPH_OSD_OP_TYPE_LOCK | 6,

	
	CEPH_OSD_OP_CALL    = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_EXEC | 1,

	
	CEPH_OSD_OP_PGLS      = CEPH_OSD_OP_MODE_RD | CEPH_OSD_OP_TYPE_PG | 1,
};

static inline int ceph_osd_op_type_lock(int op)
{
	return (op & CEPH_OSD_OP_TYPE) == CEPH_OSD_OP_TYPE_LOCK;
}
static inline int ceph_osd_op_type_data(int op)
{
	return (op & CEPH_OSD_OP_TYPE) == CEPH_OSD_OP_TYPE_DATA;
}
static inline int ceph_osd_op_type_attr(int op)
{
	return (op & CEPH_OSD_OP_TYPE) == CEPH_OSD_OP_TYPE_ATTR;
}
static inline int ceph_osd_op_type_exec(int op)
{
	return (op & CEPH_OSD_OP_TYPE) == CEPH_OSD_OP_TYPE_EXEC;
}
static inline int ceph_osd_op_type_pg(int op)
{
	return (op & CEPH_OSD_OP_TYPE) == CEPH_OSD_OP_TYPE_PG;
}

static inline int ceph_osd_op_mode_subop(int op)
{
	return (op & CEPH_OSD_OP_MODE) == CEPH_OSD_OP_MODE_SUB;
}
static inline int ceph_osd_op_mode_read(int op)
{
	return (op & CEPH_OSD_OP_MODE) == CEPH_OSD_OP_MODE_RD;
}
static inline int ceph_osd_op_mode_modify(int op)
{
	return (op & CEPH_OSD_OP_MODE) == CEPH_OSD_OP_MODE_WR;
}

#define CEPH_OSD_TMAP_HDR 'h'
#define CEPH_OSD_TMAP_SET 's'
#define CEPH_OSD_TMAP_RM  'r'

extern const char *ceph_osd_op_name(int op);


enum {
	CEPH_OSD_FLAG_ACK = 1,          
	CEPH_OSD_FLAG_ONNVRAM = 2,      
	CEPH_OSD_FLAG_ONDISK = 4,       
	CEPH_OSD_FLAG_RETRY = 8,        
	CEPH_OSD_FLAG_READ = 16,        
	CEPH_OSD_FLAG_WRITE = 32,       
	CEPH_OSD_FLAG_ORDERSNAP = 64,   
	CEPH_OSD_FLAG_PEERSTAT = 128,   
	CEPH_OSD_FLAG_BALANCE_READS = 256,
	CEPH_OSD_FLAG_PARALLELEXEC = 512, 
	CEPH_OSD_FLAG_PGOP = 1024,      
	CEPH_OSD_FLAG_EXEC = 2048,      
	CEPH_OSD_FLAG_EXEC_PUBLIC = 4096, 
};

enum {
	CEPH_OSD_OP_FLAG_EXCL = 1,      
};

#define EOLDSNAPC    ERESTART  
#define EBLACKLISTED ESHUTDOWN 

enum {
	CEPH_OSD_CMPXATTR_OP_NOP = 0,
	CEPH_OSD_CMPXATTR_OP_EQ  = 1,
	CEPH_OSD_CMPXATTR_OP_NE  = 2,
	CEPH_OSD_CMPXATTR_OP_GT  = 3,
	CEPH_OSD_CMPXATTR_OP_GTE = 4,
	CEPH_OSD_CMPXATTR_OP_LT  = 5,
	CEPH_OSD_CMPXATTR_OP_LTE = 6
};

enum {
	CEPH_OSD_CMPXATTR_MODE_STRING = 1,
	CEPH_OSD_CMPXATTR_MODE_U64    = 2
};

#define RADOS_NOTIFY_VER	1

struct ceph_osd_op {
	__le16 op;           
	__le32 flags;        
	union {
		struct {
			__le64 offset, length;
			__le64 truncate_size;
			__le32 truncate_seq;
		} __attribute__ ((packed)) extent;
		struct {
			__le32 name_len;
			__le32 value_len;
			__u8 cmp_op;       
			__u8 cmp_mode;     
		} __attribute__ ((packed)) xattr;
		struct {
			__u8 class_len;
			__u8 method_len;
			__u8 argc;
			__le32 indata_len;
		} __attribute__ ((packed)) cls;
		struct {
			__le64 cookie, count;
		} __attribute__ ((packed)) pgls;
	        struct {
		        __le64 snapid;
	        } __attribute__ ((packed)) snap;
		struct {
			__le64 cookie;
			__le64 ver;
			__u8 flag;	
		} __attribute__ ((packed)) watch;
};
	__le32 payload_len;
} __attribute__ ((packed));

struct ceph_osd_request_head {
	__le32 client_inc;                 
	struct ceph_object_layout layout;  
	__le32 osdmap_epoch;               

	__le32 flags;

	struct ceph_timespec mtime;        
	struct ceph_eversion reassert_version; 

	__le32 object_len;     

	__le64 snapid;         
	__le64 snap_seq;       
	__le32 num_snaps;

	__le16 num_ops;
	struct ceph_osd_op ops[];  
} __attribute__ ((packed));

struct ceph_osd_reply_head {
	__le32 client_inc;                
	__le32 flags;
	struct ceph_object_layout layout;
	__le32 osdmap_epoch;
	struct ceph_eversion reassert_version; 

	__le32 result;                    

	__le32 object_len;                
	__le32 num_ops;
	struct ceph_osd_op ops[0];  
} __attribute__ ((packed));



#endif
