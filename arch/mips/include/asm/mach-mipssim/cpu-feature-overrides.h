/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003, 2004 Chris Dearman
 */
#ifndef __ASM_MACH_SIM_CPU_FEATURE_OVERRIDES_H
#define __ASM_MACH_SIM_CPU_FEATURE_OVERRIDES_H


#ifdef CONFIG_CPU_MIPS32
#define cpu_has_tlb		1
#define cpu_has_4kex		1
#define cpu_has_4k_cache	1
#define cpu_has_fpu		0
#define cpu_has_counter		1
#define cpu_has_divec		1
#define cpu_has_vce		0
#define cpu_has_mcheck		1
#define cpu_has_llsc		1
#define cpu_has_clo_clz		1
#define cpu_has_nofpuex		0
#endif

#ifdef CONFIG_CPU_MIPS64
#define cpu_has_tlb		1
#define cpu_has_4kex		1
#define cpu_has_4k_cache	1
#define cpu_has_counter		1
#define cpu_has_divec		1
#define cpu_has_vce		0
#define cpu_has_mcheck		1
#define cpu_has_llsc		1
#define cpu_has_clo_clz		1
#define cpu_has_nofpuex		0
#endif

#endif 
