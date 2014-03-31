/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SGI IP27 specific setup.
 *
 * Copyright (C) 1995 - 1997, 1999 Silcon Graphics, Inc.
 * Copyright (C) 1999 Ralf Baechle (ralf@gnu.org)
 */
#ifndef _ASM_SN_SN0_ARCH_H
#define _ASM_SN_SN0_ARCH_H


#ifndef SN0XXL  
#define MAX_COMPACT_NODES       64

#define MAXCPUS                 128

#else 

#define MAX_COMPACT_NODES       128
#define MAXCPUS                 256

#endif 

#define MAX_NASIDS		256

#define	MAX_REGIONS		64
#define MAX_NONPREMIUM_REGIONS  16
#define MAX_PREMIUM_REGIONS     MAX_REGIONS

#define MAX_PARTITIONS		MAX_REGIONS

#define NASID_MASK_BYTES	((MAX_NASIDS + 7) / 8)

#ifdef CONFIG_SGI_SN_N_MODE
#define MAX_MEM_SLOTS   16                      
#else 
#define MAX_MEM_SLOTS   32                      
#endif 

#define SLOT_SHIFT      	(27)
#define SLOT_MIN_MEM_SIZE	(32*1024*1024)

#define CPUS_PER_NODE		2	
#define CPUS_PER_NODE_SHFT	1	
#define CPUS_PER_SUBNODE	2	

#endif 
