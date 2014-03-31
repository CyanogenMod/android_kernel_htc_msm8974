/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (c) 2002 NEC Corp.
 * Copyright (c) 2002 Erich Focht <efocht@ess.nec.de>
 * Copyright (c) 2002 Kimio Suganuma <k-suganuma@da.jp.nec.com>
 */
#ifndef _ASM_IA64_NODEDATA_H
#define _ASM_IA64_NODEDATA_H

#include <linux/numa.h>

#include <asm/percpu.h>
#include <asm/mmzone.h>

#ifdef CONFIG_NUMA


struct pglist_data;
struct ia64_node_data {
	short			active_cpu_count;
	short			node;
	struct pglist_data	*pg_data_ptrs[MAX_NUMNODES];
};


#define local_node_data		(local_cpu_data->node_data)

#define NODE_DATA(nid)		(local_node_data->pg_data_ptrs[nid])

#define LOCAL_DATA_ADDR(pgdat)  			\
	((struct ia64_node_data *)((u64)(pgdat) + 	\
				   L1_CACHE_ALIGN(sizeof(struct pglist_data))))

#endif 

#endif 
