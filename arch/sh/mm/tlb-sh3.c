/*
 * arch/sh/mm/tlb-sh3.c
 *
 * SH-3 specific TLB operations
 *
 * Copyright (C) 1999  Niibe Yutaka
 * Copyright (C) 2002  Paul Mundt
 *
 * Released under the terms of the GNU GPL v2.0.
 */
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>
#include <asm/cacheflush.h>

void __update_tlb(struct vm_area_struct *vma, unsigned long address, pte_t pte)
{
	unsigned long flags, pteval, vpn;

	if (vma && current->active_mm != vma->vm_mm)
		return;

	local_irq_save(flags);

	
	vpn = (address & MMU_VPN_MASK) | get_asid();
	__raw_writel(vpn, MMU_PTEH);

	pteval = pte_val(pte);

	
	pteval &= _PAGE_FLAGS_HARDWARE_MASK; 
	
	__raw_writel(pteval, MMU_PTEL);

	
	asm volatile("ldtlb":  :  : "memory");
	local_irq_restore(flags);
}

void local_flush_tlb_one(unsigned long asid, unsigned long page)
{
	unsigned long addr, data;
	int i, ways = MMU_NTLB_WAYS;

	addr = MMU_TLB_ADDRESS_ARRAY | (page & 0x1F000);
	data = (page & 0xfffe0000) | asid; 

	if ((current_cpu_data.flags & CPU_HAS_MMU_PAGE_ASSOC)) {
		addr |= MMU_PAGE_ASSOC_BIT;
		ways = 1;	
	}

	for (i = 0; i < ways; i++)
		__raw_writel(data, addr + (i << 8));
}

void local_flush_tlb_all(void)
{
	unsigned long flags, status;

	local_irq_save(flags);
	status = __raw_readl(MMUCR);
	status |= 0x04;
	__raw_writel(status, MMUCR);
	ctrl_barrier();
	local_irq_restore(flags);
}
