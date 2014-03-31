/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5


#ifndef _MMU_DEFS_H_
#define _MMU_DEFS_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#define ARM_ASID_BITS 8
#define ARM_ASID_NUM  (1 << ARM_ASID_BITS)
#define ARM_ASID_MASK (ARM_ASID_NUM - 1)

#define ARM_L1PT_ORDER        14
#define ARM_L2PT_FINE_ORDER   12
#define ARM_L2PT_COARSE_ORDER 10

#define ARM_L1D_SECTION_ORDER      20
#define ARM_L1D_SUPERSECTION_ORDER 24

#define ARM_L2D_SMALL_ORDER 12
#define ARM_L2D_LARGE_ORDER 16

#define ARM_L1PT_SIZE        (1 << ARM_L1PT_ORDER)
#define ARM_L2PT_FINE_SIZE   (1 << ARM_L2PT_FINE_ORDER)
#define ARM_L2PT_COARSE_SIZE (1 << ARM_L2PT_COARSE_ORDER)

#define ARM_L1D_SECTION_SIZE      (1 << ARM_L1D_SECTION_ORDER)
#define ARM_L1D_SUPERSECTION_SIZE (1 << ARM_L1D_SUPERSECTION_ORDER)

#define ARM_L2D_SMALL_SIZE (1 << ARM_L2D_SMALL_ORDER)
#define ARM_L2D_LARGE_SIZE (1 << ARM_L2D_LARGE_ORDER)

#define ARM_L2PT_COARSE_PER_PAGE (PAGE_SIZE / ARM_L2PT_COARSE_SIZE)

#define ARM_L1PT_ENTRIES        (ARM_L1PT_SIZE / sizeof(ARM_L1D))
#define ARM_L2PT_FINE_ENTRIES   (ARM_L2PT_FINE_SIZE / sizeof(ARM_L2D))
#define ARM_L2PT_COARSE_ENTRIES (ARM_L2PT_COARSE_SIZE / sizeof(ARM_L2D))

#define ARM_L1D_TYPE_INVALID         0
#define ARM_L1D_TYPE_COARSE          1
#define ARM_L1D_TYPE_SECTION         2
#define ARM_L1D_TYPE_SUPERSECTION    2

#define ARM_L1PT_INDX(addr)        MVP_EXTRACT_FIELD((addr), 20, 12)
#define ARM_L2PT_COARSE_INDX(addr) MVP_EXTRACT_FIELD((addr), 12, 8)

#define ARM_L1D_PTR_INDX(l1dp) MVP_BITS((uint32)(l1dp), 2, ARM_L1PT_ORDER - 1)
#define ARM_L2D_PTR_INDX(l2dp)	\
	MVP_BITS((uint32)(l2dp), 2, ARM_L2PT_COARSE_ORDER - 1)

#define ARM_L1D_BASE_ADDR(base) ((base) << ARM_L1PT_ORDER)
#define ARM_L1D_ADDR_BASE(addr) ((addr) >> ARM_L1PT_ORDER)

#define ARM_SUPER_SECTION_INDEX(addr) MVP_EXTRACT_FIELD((addr), 20, 4)

#define ARM_L1D_SUPERSECTION_BASE_ADDR(base)	\
	((base) << ARM_L1D_SUPERSECTION_ORDER)
#define ARM_L1D_SUPERSECTION_ADDR_BASE(addr)	\
	((addr) >> ARM_L1D_SUPERSECTION_ORDER)
#define ARM_L1D_SECTION_BASE_ADDR(base) ((base) << ARM_L1D_SECTION_ORDER)
#define ARM_L1D_SECTION_ADDR_BASE(addr) ((addr) >> ARM_L1D_SECTION_ORDER)
#define ARM_L1D_COARSE_BASE_ADDR(base)  ((base) << ARM_L2PT_COARSE_ORDER)
#define ARM_L1D_COARSE_ADDR_BASE(addr)  ((addr) >> ARM_L2PT_COARSE_ORDER)
#define ARM_L1D_FINE_BASE_ADDR(base)    ((base) << ARM_L2PT_FINE_ORDER)
#define ARM_L1D_FINE_ADDR_BASE(addr)    ((addr) >> ARM_L2PT_FINE_ORDER)

#define ARM_L1PT_PAGES (1<<(ARM_L1PT_ORDER - PAGE_ORDER))


#define ARM_L2D_TYPE_INVALID       0
#define ARM_L2D_TYPE_LARGE         0
#define ARM_L2D_TYPE_SMALL         1
#define ARM_L2D_XTYPE_LARGE        1
#define ARM_L2D_XTYPE_SMALL        2
#define ARM_L2D_XTYPE_SMALL_NX     3

#define ARM_L2D_LARGE_BASE_ADDR(base)    ((base) << ARM_L2D_LARGE_ORDER)
#define ARM_L2D_LARGE_ADDR_BASE(addr)    ((addr) >> ARM_L2D_LARGE_ORDER)
#define ARM_L2D_SMALL_BASE_ADDR(base)    ((base) << ARM_L2D_SMALL_ORDER)
#define ARM_L2D_SMALL_ADDR_BASE(addr)    ((addr) >> ARM_L2D_SMALL_ORDER)

#define ARM_L2D_SMALL_PAGE_NUMBER(addr)  ARM_L2D_SMALL_ADDR_BASE(addr)
#define ARM_L2D_SMALL_PAGE_OFFSET(addr)  ((addr) & (PAGE_SIZE - 1))

#define ARM_PERM_NONE       0
#define ARM_PERM_PRIV_RW    1
#define ARM_PERM_USER_RO    2
#define ARM_PERM_USER_RW    3

#define ARM_SIMPLE_PERM_KERN_RW 0
#define ARM_SIMPLE_PERM_USER_RW 1
#define ARM_SIMPLE_PERM_KERN_RO 2
#define ARM_SIMPLE_PERM_USER_RO 3

#define ARM_SIMPLE_PERM_AP_KERN 1
#define ARM_SIMPLE_PERM_AP_USER 3

#define ARM_SIMPLE_PERM_APX_RW 0
#define ARM_SIMPLE_PERM_APX_RO 1

#define ARM_SIMPLE_PERM_AP(x)  ((MVP_BIT(x, 0) << 1) | 1)
#define ARM_SIMPLE_PERM_APX(x) MVP_BIT(x, 1)

#define ARM_DOMAINS            16

#define ARM_DOMAIN_NOACCESS    0
#define ARM_DOMAIN_CLIENT      1
#define ARM_DOMAIN_RESERVED    2
#define ARM_DOMAIN_MANAGER     3

#define ARM_DOMAIN_INDEX(dacr, dom)    MVP_EXTRACT_FIELD((dacr), 2*(dom), 2)
#define ARM_DOMAIN_ACCESS(dom, access) ((access) << (2*(dom)))

#define ARM_CACHE_LEVELS_MAX    8
#define ARM_CACHE_LINE_SIZE_MAX 2048

#endif 

