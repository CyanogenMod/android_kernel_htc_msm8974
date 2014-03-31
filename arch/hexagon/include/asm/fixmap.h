/*
 * Fixmap support for Hexagon - enough to support highmem features
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

#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H

#include <asm/mem-layout.h>

#define	__fix_to_virt(x)	(FIXADDR_TOP - ((x) << PAGE_SHIFT))
#define	__virt_to_fix(x)	((FIXADDR_TOP - ((x)&PAGE_MASK)) >> PAGE_SHIFT)

extern void __this_fixmap_does_not_exist(void);

static inline unsigned long fix_to_virt(const unsigned int idx)
{
	if (idx >= __end_of_fixed_addresses)
		__this_fixmap_does_not_exist();

	return __fix_to_virt(idx);
}

static inline unsigned long virt_to_fix(const unsigned long vaddr)
{
	BUG_ON(vaddr >= FIXADDR_TOP || vaddr < FIXADDR_START);
	return __virt_to_fix(vaddr);
}

#define kmap_get_fixmap_pte(vaddr) \
	pte_offset_kernel(pmd_offset(pud_offset(pgd_offset_k(vaddr), \
				(vaddr)), (vaddr)), (vaddr))

#endif
