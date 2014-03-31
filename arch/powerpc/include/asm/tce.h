/*
 * Copyright (C) 2001 Mike Corrigan & Dave Engebretsen, IBM Corporation
 * Rewrite, cleanup:
 * Copyright (C) 2004 Olof Johansson <olof@lixom.net>, IBM Corporation
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef _ASM_POWERPC_TCE_H
#define _ASM_POWERPC_TCE_H
#ifdef __KERNEL__

#include <asm/iommu.h>

#define TCE_VB			0
#define TCE_PCI			1
#define TCE_PCI_SWINV_CREATE	2
#define TCE_PCI_SWINV_FREE	4
#define TCE_PCI_SWINV_PAIR	8


#define TCE_SHIFT	12
#define TCE_PAGE_SIZE	(1 << TCE_SHIFT)

#define TCE_ENTRY_SIZE		8		

#define TCE_RPN_MASK		0xfffffffffful  
#define TCE_RPN_SHIFT		12
#define TCE_VALID		0x800		
#define TCE_ALLIO		0x400		
#define TCE_PCI_WRITE		0x2		
#define TCE_PCI_READ		0x1		
#define TCE_VB_WRITE		0x1		

#endif 
#endif 
