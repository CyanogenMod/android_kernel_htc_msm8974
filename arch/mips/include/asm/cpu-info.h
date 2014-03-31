/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 Waldorf GMBH
 * Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2003 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 2004  Maciej W. Rozycki
 */
#ifndef __ASM_CPU_INFO_H
#define __ASM_CPU_INFO_H

#include <linux/types.h>

#include <asm/cache.h>

struct cache_desc {
	unsigned int waysize;	
	unsigned short sets;	
	unsigned char ways;	
	unsigned char linesz;	
	unsigned char waybit;	
	unsigned char flags;	
};

#define MIPS_CACHE_NOT_PRESENT	0x00000001
#define MIPS_CACHE_VTAG		0x00000002	
#define MIPS_CACHE_ALIASES	0x00000004	
#define MIPS_CACHE_IC_F_DC	0x00000008	
#define MIPS_IC_SNOOPS_REMOTE	0x00000010	
#define MIPS_CACHE_PINDEX	0x00000020	

struct cpuinfo_mips {
	unsigned int		udelay_val;
	unsigned int		asid_cache;

	unsigned long		options;
	unsigned long		ases;
	unsigned int		processor_id;
	unsigned int		fpu_id;
	unsigned int		cputype;
	int			isa_level;
	int			tlbsize;
	struct cache_desc	icache;	
	struct cache_desc	dcache;	
	struct cache_desc	scache;	
	struct cache_desc	tcache;	
	int			srsets;	
	int			core;	
#ifdef CONFIG_64BIT
	int			vmbits;	
#endif
#if defined(CONFIG_MIPS_MT_SMP) || defined(CONFIG_MIPS_MT_SMTC)
	int			vpe_id;  
#endif
#ifdef CONFIG_MIPS_MT_SMTC
	int			tc_id;   
#endif
	void 			*data;	
	unsigned int		watch_reg_count;   
	unsigned int		watch_reg_use_cnt; 
#define NUM_WATCH_REGS 4
	u16			watch_reg_masks[NUM_WATCH_REGS];
	unsigned int		kscratch_mask; 
} __attribute__((aligned(SMP_CACHE_BYTES)));

extern struct cpuinfo_mips cpu_data[];
#define current_cpu_data cpu_data[smp_processor_id()]
#define raw_current_cpu_data cpu_data[raw_smp_processor_id()]

extern void cpu_probe(void);
extern void cpu_report(void);

extern const char *__cpu_name[];
#define cpu_name_string()	__cpu_name[smp_processor_id()]

#endif 
