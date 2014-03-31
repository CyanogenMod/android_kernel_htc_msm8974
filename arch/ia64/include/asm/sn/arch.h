/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SGI specific setup.
 *
 * Copyright (C) 1995-1997,1999,2001-2005 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (C) 1999 Ralf Baechle (ralf@gnu.org)
 */
#ifndef _ASM_IA64_SN_ARCH_H
#define _ASM_IA64_SN_ARCH_H

#include <linux/numa.h>
#include <asm/types.h>
#include <asm/percpu.h>
#include <asm/sn/types.h>
#include <asm/sn/sn_cpuid.h>

#define MAX_TIO_NODES		MAX_NUMNODES
#define MAX_COMPACT_NODES	(MAX_NUMNODES + MAX_TIO_NODES)

#define MAX_NUMALINK_NODES	16384

struct sn_hub_info_s {
	u8 shub2;
	u8 nasid_shift;
	u8 as_shift;
	u8 shub_1_1_found;
	u16 nasid_bitmask;
};
DECLARE_PER_CPU(struct sn_hub_info_s, __sn_hub_info);
#define sn_hub_info 	(&__get_cpu_var(__sn_hub_info))
#define is_shub2()	(sn_hub_info->shub2)
#define is_shub1()	(sn_hub_info->shub2 == 0)

#define enable_shub_wars_1_1()	(sn_hub_info->shub_1_1_found)


DECLARE_PER_CPU(short, __sn_cnodeid_to_nasid[MAX_COMPACT_NODES]);
#define sn_cnodeid_to_nasid	(&__get_cpu_var(__sn_cnodeid_to_nasid[0]))


extern u8 sn_partition_id;
extern u8 sn_system_size;
extern u8 sn_sharing_domain_size;
extern u8 sn_region_size;

extern void sn_flush_all_caches(long addr, long bytes);
extern bool sn_cpu_disable_allowed(int cpu);

#endif 
