/*
 * linux/include/asm-xtensa/pci.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_PCI_H
#define _XTENSA_PCI_H

#ifdef __KERNEL__


#define pcibios_assign_all_busses()	0

extern struct pci_controller* pcibios_alloc_controller(void);

static inline void pcibios_penalize_isa_irq(int irq)
{
	
}


#define PCIBIOS_MIN_IO		0x2000
#define PCIBIOS_MIN_MEM		0x10000000


#include <linux/types.h>
#include <linux/slab.h>
#include <asm/scatterlist.h>
#include <linux/string.h>
#include <asm/io.h>

struct pci_dev;


#define PCI_DMA_BUS_IS_PHYS	(1)

int pci_mmap_page_range(struct pci_dev *pdev, struct vm_area_struct *vma,
                        enum pci_mmap_state mmap_state, int write_combine);

#define HAVE_PCI_MMAP	1

#endif 

#include <asm-generic/pci-dma-compat.h>

#include <asm-generic/pci.h>

#endif	
