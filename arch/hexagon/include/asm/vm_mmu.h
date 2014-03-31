/*
 * Hexagon VM page table entry definitions
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _ASM_VM_MMU_H
#define _ASM_VM_MMU_H


#define	__HVM_PDE_S		(0x7 << 0)
#define __HVM_PDE_S_4KB		0
#define __HVM_PDE_S_16KB	1
#define __HVM_PDE_S_64KB	2
#define __HVM_PDE_S_256KB	3
#define __HVM_PDE_S_1MB		4
#define __HVM_PDE_S_4MB		5
#define __HVM_PDE_S_16MB	6
#define __HVM_PDE_S_INVALID	7

#define __HVM_PDE_PTMASK_4KB	0xfffff000
#define __HVM_PDE_PTMASK_16KB	0xfffffc00
#define __HVM_PDE_PTMASK_64KB	0xffffff00
#define __HVM_PDE_PTMASK_256KB	0xffffffc0
#define __HVM_PDE_PTMASK_1MB	0xfffffff0

#define __HVM_PTE_T		(1<<4)
#define __HVM_PTE_U		(1<<5)
#define	__HVM_PTE_C		(0x7<<6)
#define __HVM_PTE_CVAL(pte)	(((pte) & __HVM_PTE_C) >> 6)
#define __HVM_PTE_R		(1<<9)
#define __HVM_PTE_W		(1<<10)
#define __HVM_PTE_X		(1<<11)


#define __HEXAGON_C_WB		0x0	
#define	__HEXAGON_C_WT		0x1	
#define	__HEXAGON_C_DEV		0x4	
#define	__HEXAGON_C_WT_L2	0x5	
#if defined(CONFIG_HEXAGON_COMET) || defined(CONFIG_QDSP6_ST1)
#define __HEXAGON_C_UNC		__HEXAGON_C_DEV
#else
#define	__HEXAGON_C_UNC		0x6	
#endif
#define	__HEXAGON_C_WB_L2	0x7	


#define	CACHE_DEFAULT	__HEXAGON_C_WB_L2


#define __HVM_PTE_PGMASK_4KB	0xfffff000
#define __HVM_PTE_PGMASK_16KB	0xffffc000
#define __HVM_PTE_PGMASK_64KB	0xffff0000
#define __HVM_PTE_PGMASK_256KB	0xfffc0000
#define __HVM_PTE_PGMASK_1MB	0xfff00000


#define __HVM_PTE_PGMASK_4MB	0xffc00000
#define __HVM_PTE_PGMASK_16MB	0xff000000


#define BIG_KERNEL_PAGE_SHIFT 24
#define BIG_KERNEL_PAGE_SIZE (1 << BIG_KERNEL_PAGE_SHIFT)



#endif 
