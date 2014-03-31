/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004 Silicon Graphics, Inc. All rights reserved.
 *
 * Data types used by the SN_SAL_HWPERF_OP SAL call for monitoring
 * SGI Altix node and router hardware
 *
 * Mark Goodwin <markgw@sgi.com> Mon Aug 30 12:23:46 EST 2004
 */

#ifndef SN_HWPERF_H
#define SN_HWPERF_H

#define SN_HWPERF_MAXSTRING		128
struct sn_hwperf_object_info {
	u32 id;
	union {
		struct {
			u64 this_part:1;
			u64 is_shared:1;
		} fields;
		struct {
			u64 flags;
			u64 reserved;
		} b;
	} f;
	char name[SN_HWPERF_MAXSTRING];
	char location[SN_HWPERF_MAXSTRING];
	u32 ports;
};

#define sn_hwp_this_part	f.fields.this_part
#define sn_hwp_is_shared	f.fields.is_shared
#define sn_hwp_flags		f.b.flags

#define SN_HWPERF_IS_NODE(x)		((x) && strstr((x)->name, "SHub"))
#define SN_HWPERF_IS_NODE_SHUB2(x)	((x) && strstr((x)->name, "SHub 2."))
#define SN_HWPERF_IS_IONODE(x)		((x) && strstr((x)->name, "TIO"))
#define SN_HWPERF_IS_NL3ROUTER(x)	((x) && strstr((x)->name, "NL3Router"))
#define SN_HWPERF_IS_NL4ROUTER(x)	((x) && strstr((x)->name, "NL4Router"))
#define SN_HWPERF_IS_OLDROUTER(x)	((x) && strstr((x)->name, "Router"))
#define SN_HWPERF_IS_ROUTER(x)		(SN_HWPERF_IS_NL3ROUTER(x) || 		\
					 	SN_HWPERF_IS_NL4ROUTER(x) || 	\
					 	SN_HWPERF_IS_OLDROUTER(x))
#define SN_HWPERF_FOREIGN(x)		((x) && !(x)->sn_hwp_this_part && !(x)->sn_hwp_is_shared)
#define SN_HWPERF_SAME_OBJTYPE(x,y)	((SN_HWPERF_IS_NODE(x) && SN_HWPERF_IS_NODE(y)) ||\
					(SN_HWPERF_IS_IONODE(x) && SN_HWPERF_IS_IONODE(y)) ||\
					(SN_HWPERF_IS_ROUTER(x) && SN_HWPERF_IS_ROUTER(y)))

struct sn_hwperf_port_info {
	u32 port;
	u32 conn_id;
	u32 conn_port;
};

struct sn_hwperf_data {
	u64 addr;
	u64 data;
};

struct sn_hwperf_ioctl_args {
        u64 arg;		
        u64 sz;                 
        void *ptr;              
        u32 v0;			
};

#define SN_HWPERF_ARG_ANY_CPU		0x7fffffffUL
#define SN_HWPERF_ARG_CPU_MASK		0x7fffffff00000000ULL
#define SN_HWPERF_ARG_USE_IPI_MASK	0x8000000000000000ULL
#define SN_HWPERF_ARG_OBJID_MASK	0x00000000ffffffffULL

#define SN_HWPERF_OP_MEM_COPYIN		0x1000
#define SN_HWPERF_OP_MEM_COPYOUT	0x2000
#define SN_HWPERF_OP_MASK		0x0fff

#define	SN_HWPERF_GET_HEAPSIZE		1

#define SN_HWPERF_INSTALL_HEAP		2

#define SN_HWPERF_OBJECT_COUNT		(10|SN_HWPERF_OP_MEM_COPYOUT)

#define SN_HWPERF_OBJECT_DISTANCE	(11|SN_HWPERF_OP_MEM_COPYOUT)

#define SN_HWPERF_ENUM_OBJECTS		(12|SN_HWPERF_OP_MEM_COPYOUT)

#define SN_HWPERF_ENUM_PORTS		(13|SN_HWPERF_OP_MEM_COPYOUT)

#define SN_HWPERF_SET_MMRS		(14|SN_HWPERF_OP_MEM_COPYIN)
#define SN_HWPERF_GET_MMRS		(15|SN_HWPERF_OP_MEM_COPYOUT| \
					    SN_HWPERF_OP_MEM_COPYIN)
#define SN_HWPERF_ACQUIRE		16

#define SN_HWPERF_RELEASE		17

#define SN_HWPERF_FORCE_RELEASE		18


#define SN_HWPERF_GET_CPU_INFO		(100|SN_HWPERF_OP_MEM_COPYOUT)

#define SN_HWPERF_GET_OBJ_NODE		(101|SN_HWPERF_OP_MEM_COPYOUT)

#define SN_HWPERF_GET_NODE_NASID	(102|SN_HWPERF_OP_MEM_COPYOUT)

extern int sn_hwperf_get_nearest_node(cnodeid_t node,
	cnodeid_t *near_mem, cnodeid_t *near_cpu);

#define SN_HWPERF_OP_OK			0
#define SN_HWPERF_OP_NOMEM		1
#define SN_HWPERF_OP_NO_PERM		2
#define SN_HWPERF_OP_IO_ERROR		3
#define SN_HWPERF_OP_BUSY		4
#define SN_HWPERF_OP_RECONFIGURE	253
#define SN_HWPERF_OP_INVAL		254

int sn_topology_open(struct inode *inode, struct file *file);
int sn_topology_release(struct inode *inode, struct file *file);
#endif				
