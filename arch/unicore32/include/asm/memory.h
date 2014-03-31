/*
 * linux/arch/unicore32/include/asm/memory.h
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Note: this file should not be included by non-asm/.h files
 */
#ifndef __UNICORE_MEMORY_H__
#define __UNICORE_MEMORY_H__

#include <linux/compiler.h>
#include <linux/const.h>
#include <asm/sizes.h>
#include <mach/memory.h>

#define UL(x) _AC(x, UL)

#define PAGE_OFFSET		UL(0xC0000000)
#define TASK_SIZE		(PAGE_OFFSET - UL(0x41000000))
#define TASK_UNMAPPED_BASE	(PAGE_OFFSET / 3)

#define MODULES_VADDR		(PAGE_OFFSET - 16*1024*1024)
#if TASK_SIZE > MODULES_VADDR
#error Top of user space clashes with start of module space
#endif

#define MODULES_END		(PAGE_OFFSET)

#define IOREMAP_MAX_ORDER	24

#ifndef __virt_to_phys
#define __virt_to_phys(x)	((x) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt(x)	((x) - PHYS_OFFSET + PAGE_OFFSET)
#endif

#define	__phys_to_pfn(paddr)	((paddr) >> PAGE_SHIFT)
#define	__pfn_to_phys(pfn)	((pfn) << PAGE_SHIFT)

#define page_to_phys(page)	(__pfn_to_phys(page_to_pfn(page)))
#define phys_to_page(phys)	(pfn_to_page(__phys_to_pfn(phys)))

#ifndef __ASSEMBLY__

#ifndef arch_adjust_zones
#define arch_adjust_zones(size, holes) do { } while (0)
#endif

#define PHYS_PFN_OFFSET	(PHYS_OFFSET >> PAGE_SHIFT)

#define __pa(x)			__virt_to_phys((unsigned long)(x))
#define __va(x)			((void *)__phys_to_virt((unsigned long)(x)))
#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)

#define ARCH_PFN_OFFSET		PHYS_PFN_OFFSET

#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define virt_addr_valid(kaddr)	((unsigned long)(kaddr) >= PAGE_OFFSET && \
		(unsigned long)(kaddr) < (unsigned long)high_memory)

#endif

#include <asm-generic/memory_model.h>

#endif
