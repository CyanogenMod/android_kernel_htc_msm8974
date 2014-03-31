/*
 *  arch/arm/mach-clps711x/include/mach/memory.h
 *
 *  Copyright (C) 1999 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define PLAT_PHYS_OFFSET	UL(0xc0000000)

#if !defined(CONFIG_ARCH_CDB89712) && !defined (CONFIG_ARCH_AUTCPU12)

#define __virt_to_bus(x)	((x) - PAGE_OFFSET)
#define __bus_to_virt(x)	((x) + PAGE_OFFSET)
#define __pfn_to_bus(x)		(__pfn_to_phys(x) - PHYS_OFFSET)
#define __bus_to_pfn(x)		__phys_to_pfn((x) + PHYS_OFFSET)

#endif




#define SECTION_SIZE_BITS	24
#define MAX_PHYSMEM_BITS	32

#endif

