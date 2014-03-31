#ifndef FS_CEPH_IOCTL_H
#define FS_CEPH_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define CEPH_IOCTL_MAGIC 0x97

/*
 * CEPH_IOC_GET_LAYOUT - get file layout or dir layout policy
 * CEPH_IOC_SET_LAYOUT - set file layout
 * CEPH_IOC_SET_LAYOUT_POLICY - set dir layout policy
 *
 * The file layout specifies how file data is striped over objects in
 * the distributed object store, which object pool they belong to (if
 * it differs from the default), and an optional 'preferred osd' to
 * store them on.
 *
 * Files get a new layout based on the policy set on the containing
 * directory or one of its ancestors.  The GET_LAYOUT ioctl will let
 * you examine the layout for a file or the policy on a directory.
 *
 * SET_LAYOUT will let you set a layout on a newly created file.  This
 * only works immediately after the file is created and before any
 * data is written to it.
 *
 * SET_LAYOUT_POLICY will let you set a layout policy (default layout)
 * on a directory that will apply to any new files created in that
 * directory (or any child directory that doesn't specify a layout of
 * its own).
 */

struct ceph_ioctl_layout {
	__u64 stripe_unit, stripe_count, object_size;
	__u64 data_pool;
	__s64 preferred_osd;
};

#define CEPH_IOC_GET_LAYOUT _IOR(CEPH_IOCTL_MAGIC, 1,		\
				   struct ceph_ioctl_layout)
#define CEPH_IOC_SET_LAYOUT _IOW(CEPH_IOCTL_MAGIC, 2,		\
				   struct ceph_ioctl_layout)
#define CEPH_IOC_SET_LAYOUT_POLICY _IOW(CEPH_IOCTL_MAGIC, 5,	\
				   struct ceph_ioctl_layout)

struct ceph_ioctl_dataloc {
	__u64 file_offset;           
	__u64 object_offset;         
	__u64 object_no;             
	__u64 object_size;           
	char object_name[64];        
	__u64 block_offset;          
	__u64 block_size;            
	__s64 osd;                   
	struct sockaddr_storage osd_addr; 
};

#define CEPH_IOC_GET_DATALOC _IOWR(CEPH_IOCTL_MAGIC, 3,	\
				   struct ceph_ioctl_dataloc)

#define CEPH_IOC_LAZYIO _IO(CEPH_IOCTL_MAGIC, 4)

#define CEPH_IOC_SYNCIO _IO(CEPH_IOCTL_MAGIC, 5)

#endif
