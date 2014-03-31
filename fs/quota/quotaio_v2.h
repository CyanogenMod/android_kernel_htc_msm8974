
#ifndef _LINUX_QUOTAIO_V2_H
#define _LINUX_QUOTAIO_V2_H

#include <linux/types.h>
#include <linux/quota.h>

#define V2_INITQMAGICS {\
	0xd9c01f11,	\
	0xd9c01927	\
}

#define V2_INITQVERSIONS {\
	1,		\
	1		\
}

struct v2_disk_dqheader {
	__le32 dqh_magic;	
	__le32 dqh_version;	
};

struct v2r0_disk_dqblk {
	__le32 dqb_id;		
	__le32 dqb_ihardlimit;	
	__le32 dqb_isoftlimit;	
	__le32 dqb_curinodes;	
	__le32 dqb_bhardlimit;	
	__le32 dqb_bsoftlimit;	
	__le64 dqb_curspace;	
	__le64 dqb_btime;	
	__le64 dqb_itime;	
};

struct v2r1_disk_dqblk {
	__le32 dqb_id;		
	__le32 dqb_pad;
	__le64 dqb_ihardlimit;	
	__le64 dqb_isoftlimit;	
	__le64 dqb_curinodes;	
	__le64 dqb_bhardlimit;	
	__le64 dqb_bsoftlimit;	
	__le64 dqb_curspace;	
	__le64 dqb_btime;	
	__le64 dqb_itime;	
};

struct v2_disk_dqinfo {
	__le32 dqi_bgrace;	
	__le32 dqi_igrace;	
	__le32 dqi_flags;	
	__le32 dqi_blocks;	
	__le32 dqi_free_blk;	
	__le32 dqi_free_entry;	
};

#define V2_DQINFOOFF	sizeof(struct v2_disk_dqheader)	
#define V2_DQBLKSIZE_BITS 10				

#endif 
