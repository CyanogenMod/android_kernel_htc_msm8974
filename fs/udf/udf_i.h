#ifndef _UDF_I_H
#define _UDF_I_H


struct udf_inode_info {
	struct timespec		i_crtime;
	
	struct kernel_lb_addr		i_location;
	__u64			i_unique;
	__u32			i_lenEAttr;
	__u32			i_lenAlloc;
	__u64			i_lenExtents;
	__u32			i_next_alloc_block;
	__u32			i_next_alloc_goal;
	__u32			i_checkpoint;
	unsigned		i_alloc_type : 3;
	unsigned		i_efe : 1;	
	unsigned		i_use : 1;	
	unsigned		i_strat4096 : 1;
	unsigned		reserved : 26;
	union {
		struct short_ad	*i_sad;
		struct long_ad		*i_lad;
		__u8		*i_data;
	} i_ext;
	struct rw_semaphore	i_data_sem;
	struct inode vfs_inode;
};

static inline struct udf_inode_info *UDF_I(struct inode *inode)
{
	return list_entry(inode, struct udf_inode_info, vfs_inode);
}

#endif 
