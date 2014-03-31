
#ifndef _LINUX_QUOTA_TREE_H
#define _LINUX_QUOTA_TREE_H

#include <linux/types.h>
#include <linux/quota.h>

struct qt_disk_dqdbheader {
	__le32 dqdh_next_free;	
	__le32 dqdh_prev_free;	
	__le16 dqdh_entries;	
	__le16 dqdh_pad1;
	__le32 dqdh_pad2;
};

#define QT_TREEOFF	1		

#endif 
