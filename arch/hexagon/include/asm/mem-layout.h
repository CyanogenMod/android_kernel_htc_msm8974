/*
 * Memory layout definitions for the Hexagon architecture
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

#ifndef _ASM_HEXAGON_MEM_LAYOUT_H
#define _ASM_HEXAGON_MEM_LAYOUT_H

#include <linux/const.h>


#define PAGE_OFFSET			_AC(0xc0000000, UL)


#ifndef LOAD_ADDRESS
#define LOAD_ADDRESS			0x00000000
#endif

#define TASK_SIZE			(PAGE_OFFSET)

#define STACK_TOP			TASK_SIZE
#define STACK_TOP_MAX			TASK_SIZE

#ifndef __ASSEMBLY__
enum fixed_addresses {
	FIX_KMAP_BEGIN,
	FIX_KMAP_END,  
	__end_of_fixed_addresses
};

#define MIN_KERNEL_SEG 0x300   
extern int max_kernel_seg;


#define VMALLOC_START (PAGE_OFFSET + VMALLOC_OFFSET + \
	(unsigned long)high_memory)

#define VMALLOC_OFFSET PAGE_SIZE


#define FIXADDR_TOP     0xfe000000
#define FIXADDR_SIZE    (__end_of_fixed_addresses << PAGE_SHIFT)
#define FIXADDR_START   (FIXADDR_TOP - FIXADDR_SIZE)


#define LAST_PKMAP	PTRS_PER_PTE
#define LAST_PKMAP_MASK	(LAST_PKMAP - 1)
#define PKMAP_NR(virt)	((virt - PKMAP_BASE) >> PAGE_SHIFT)
#define PKMAP_ADDR(nr)	(PKMAP_BASE + ((nr) << PAGE_SHIFT))

#define PKMAP_BASE (FIXADDR_START-PAGE_SIZE*LAST_PKMAP)

#define VMALLOC_END (PKMAP_BASE-PAGE_SIZE*2)
#endif 


#endif 
