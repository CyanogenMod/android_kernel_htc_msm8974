/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SGI UV architectural definitions
 *
 * Copyright (C) 2008 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef __ASM_IA64_UV_HUB_H__
#define __ASM_IA64_UV_HUB_H__

#include <linux/numa.h>
#include <linux/percpu.h>
#include <asm/types.h>
#include <asm/percpu.h>




#define UV_MAX_NUMALINK_BLADES	16384

#define UV_MAX_SSI_BLADES	1

#define UV_MAX_NASID_VALUE	(UV_MAX_NUMALINK_NODES * 2)

struct uv_hub_info_s {
	unsigned long	global_mmr_base;
	unsigned long	gpa_mask;
	unsigned long	gnode_upper;
	unsigned long	lowmem_remap_top;
	unsigned long	lowmem_remap_base;
	unsigned short	pnode;
	unsigned short	pnode_mask;
	unsigned short	coherency_domain_number;
	unsigned short	numa_blade_id;
	unsigned char	blade_processor_id;
	unsigned char	m_val;
	unsigned char	n_val;
};
DECLARE_PER_CPU(struct uv_hub_info_s, __uv_hub_info);
#define uv_hub_info 		(&__get_cpu_var(__uv_hub_info))
#define uv_cpu_hub_info(cpu)	(&per_cpu(__uv_hub_info, cpu))

#define UV_NASID_TO_PNODE(n)		(((n) >> 1) & uv_hub_info->pnode_mask)
#define UV_PNODE_TO_NASID(p)		(((p) << 1) | uv_hub_info->gnode_upper)

#define UV_LOCAL_MMR_BASE		0xf4000000UL
#define UV_GLOBAL_MMR32_BASE		0xf8000000UL
#define UV_GLOBAL_MMR64_BASE		(uv_hub_info->global_mmr_base)

#define UV_GLOBAL_MMR32_PNODE_SHIFT	15
#define UV_GLOBAL_MMR64_PNODE_SHIFT	26

#define UV_GLOBAL_MMR32_PNODE_BITS(p)	((p) << (UV_GLOBAL_MMR32_PNODE_SHIFT))

#define UV_GLOBAL_MMR64_PNODE_BITS(p)					\
	((unsigned long)(p) << UV_GLOBAL_MMR64_PNODE_SHIFT)


static inline unsigned long uv_soc_phys_ram_to_gpa(unsigned long paddr)
{
	if (paddr < uv_hub_info->lowmem_remap_top)
		paddr += uv_hub_info->lowmem_remap_base;
	return paddr | uv_hub_info->gnode_upper;
}


static inline unsigned long uv_gpa(void *v)
{
	return __pa(v) | uv_hub_info->gnode_upper;
}

static inline void *uv_vgpa(void *v)
{
	return (void *)uv_gpa(v);
}

static inline void *uv_va(unsigned long gpa)
{
	return __va(gpa & uv_hub_info->gpa_mask);
}

static inline void *uv_pnode_offset_to_vaddr(int pnode, unsigned long offset)
{
	return __va(((unsigned long)pnode << uv_hub_info->m_val) | offset);
}


static inline unsigned long *uv_global_mmr32_address(int pnode,
				unsigned long offset)
{
	return __va(UV_GLOBAL_MMR32_BASE |
		       UV_GLOBAL_MMR32_PNODE_BITS(pnode) | offset);
}

static inline void uv_write_global_mmr32(int pnode, unsigned long offset,
				 unsigned long val)
{
	*uv_global_mmr32_address(pnode, offset) = val;
}

static inline unsigned long uv_read_global_mmr32(int pnode,
						 unsigned long offset)
{
	return *uv_global_mmr32_address(pnode, offset);
}

static inline unsigned long *uv_global_mmr64_address(int pnode,
				unsigned long offset)
{
	return __va(UV_GLOBAL_MMR64_BASE |
		    UV_GLOBAL_MMR64_PNODE_BITS(pnode) | offset);
}

static inline void uv_write_global_mmr64(int pnode, unsigned long offset,
				unsigned long val)
{
	*uv_global_mmr64_address(pnode, offset) = val;
}

static inline unsigned long uv_read_global_mmr64(int pnode,
						 unsigned long offset)
{
	return *uv_global_mmr64_address(pnode, offset);
}

static inline unsigned long *uv_local_mmr_address(unsigned long offset)
{
	return __va(UV_LOCAL_MMR_BASE | offset);
}

static inline unsigned long uv_read_local_mmr(unsigned long offset)
{
	return *uv_local_mmr_address(offset);
}

static inline void uv_write_local_mmr(unsigned long offset, unsigned long val)
{
	*uv_local_mmr_address(offset) = val;
}


static inline int uv_blade_processor_id(void)
{
	return smp_processor_id();
}

static inline int uv_numa_blade_id(void)
{
	return 0;
}

static inline int uv_cpu_to_blade_id(int cpu)
{
	return 0;
}

static inline int uv_node_to_blade_id(int nid)
{
	return 0;
}

static inline int uv_blade_to_pnode(int bid)
{
	return 0;
}

static inline int uv_blade_nr_possible_cpus(int bid)
{
	return num_possible_cpus();
}

static inline int uv_blade_nr_online_cpus(int bid)
{
	return num_online_cpus();
}

static inline int uv_cpu_to_pnode(int cpu)
{
	return 0;
}

static inline int uv_node_to_pnode(int nid)
{
	return 0;
}

static inline int uv_num_possible_blades(void)
{
	return 1;
}

static inline void uv_hub_send_ipi(int pnode, int apicid, int vector)
{
	
}


#endif 

