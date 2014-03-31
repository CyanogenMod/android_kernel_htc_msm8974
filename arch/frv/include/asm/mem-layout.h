/* mem-layout.h: memory layout
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_MEM_LAYOUT_H
#define _ASM_MEM_LAYOUT_H

#ifndef __ASSEMBLY__
#define __UL(X)	((unsigned long) (X))
#else
#define __UL(X)	(X)
#endif

#define PAGE_SHIFT			14

#ifndef __ASSEMBLY__
#define PAGE_SIZE			(1UL << PAGE_SHIFT)
#else
#define PAGE_SIZE			(1 << PAGE_SHIFT)
#endif

#define PAGE_MASK			(~(PAGE_SIZE-1))

#define	ARCH_DMA_MINALIGN		L1_CACHE_BYTES
#define	ARCH_SLAB_MINALIGN		L1_CACHE_BYTES

#define PAGE_OFFSET			((unsigned long) &__page_offset)

#ifdef CONFIG_MMU

#define KERNEL_LOWMEM_START		__UL(0xc0000000)
#define KERNEL_LOWMEM_END		__UL(0xd0000000)
#define VMALLOC_START			__UL(0xd0000000)
#define VMALLOC_END			__UL(0xd8000000)
#define PKMAP_BASE			__UL(0xd8000000)
#define PKMAP_END			__UL(0xdc000000)
#define KMAP_ATOMIC_SECONDARY_FRAME	__UL(0xdc000000)
#define KMAP_ATOMIC_PRIMARY_FRAME	__UL(0xdd000000)

#endif

#define KERNEL_IO_START			__UL(0xe0000000)


#define BRK_BASE			__UL(2 * 1024 * 1024 + PAGE_SIZE)
#define STACK_TOP			__UL(2 * 1024 * 1024)
#define STACK_TOP_MAX			__UL(0xc0000000)

#ifdef CONFIG_MMU
#define TASK_SIZE			(PAGE_OFFSET)
#else
#define TASK_SIZE			__UL(0xFFFFFFFFUL)
#endif

#ifdef CONFIG_BINFMT_ELF_FDPIC
#define TASK_UNMAPPED_BASE		__UL(16 * 1024 * 1024)
#else
#define TASK_UNMAPPED_BASE		__UL(TASK_SIZE / 3)
#endif

#endif 
