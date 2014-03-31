#ifndef _LINUX_QUOTAIO_V1_H
#define _LINUX_QUOTAIO_V1_H

#include <linux/types.h>

#define MAX_IQ_TIME  604800	
#define MAX_DQ_TIME  604800	

struct v1_disk_dqblk {
	__u32 dqb_bhardlimit;	
	__u32 dqb_bsoftlimit;	
	__u32 dqb_curblocks;	
	__u32 dqb_ihardlimit;	
	__u32 dqb_isoftlimit;	
	__u32 dqb_curinodes;	
	time_t dqb_btime;	
	time_t dqb_itime;	
};

#define v1_dqoff(UID)      ((loff_t)((UID) * sizeof (struct v1_disk_dqblk)))

#endif	
