/*
 * arch/sh/mm/tlb-urb.c
 *
 * TLB entry wiring helpers for URB-equipped parts.
 *
 * Copyright (C) 2010  Matt Fleming
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/mm.h>
#include <linux/io.h>
#include <asm/tlb.h>
#include <asm/mmu_context.h>

void tlb_wire_entry(struct vm_area_struct *vma, unsigned long addr, pte_t pte)
{
	unsigned long status, flags;
	int urb;

	local_irq_save(flags);

	status = __raw_readl(MMUCR);
	urb = (status & MMUCR_URB) >> MMUCR_URB_SHIFT;
	status &= ~MMUCR_URC;

	BUG_ON(!--urb);

	urb = urb % MMUCR_URB_NENTRIES;

	status |= (urb << MMUCR_URC_SHIFT);
	__raw_writel(status, MMUCR);
	ctrl_barrier();

	
	__update_tlb(vma, addr, pte);

	
	status = __raw_readl(MMUCR);

	status &= ~MMUCR_URB;
	status |= (urb << MMUCR_URB_SHIFT);

	__raw_writel(status, MMUCR);
	ctrl_barrier();

	local_irq_restore(flags);
}

void tlb_unwire_entry(void)
{
	unsigned long status, flags;
	int urb;

	local_irq_save(flags);

	status = __raw_readl(MMUCR);
	urb = (status & MMUCR_URB) >> MMUCR_URB_SHIFT;
	status &= ~MMUCR_URB;

	BUG_ON(urb++ == MMUCR_URB_NENTRIES);

	urb = urb % MMUCR_URB_NENTRIES;

	status |= (urb << MMUCR_URB_SHIFT);
	__raw_writel(status, MMUCR);
	ctrl_barrier();

	local_irq_restore(flags);
}
