/*
 * Hexagon Virtual Machine TLB functions
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

#include <linux/mm.h>
#include <asm/page.h>
#include <asm/hexagon_vm.h>

void flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
			unsigned long end)
{
	struct mm_struct *mm = vma->vm_mm;

	if (mm->context.ptbase == current->active_mm->context.ptbase)
		__vmclrmap((void *)start, end - start);
}

void flush_tlb_one(unsigned long vaddr)
{
	__vmclrmap((void *)vaddr, PAGE_SIZE);
}

void tlb_flush_all(void)
{
	
	__vmclrmap(0, 0xffff0000);
}

void flush_tlb_mm(struct mm_struct *mm)
{
	
	if (current->active_mm->context.ptbase == mm->context.ptbase)
		tlb_flush_all();
}

void flush_tlb_page(struct vm_area_struct *vma, unsigned long vaddr)
{
	struct mm_struct *mm = vma->vm_mm;

	if (mm->context.ptbase  == current->active_mm->context.ptbase)
		__vmclrmap((void *)vaddr, PAGE_SIZE);
}

void flush_tlb_kernel_range(unsigned long start, unsigned long end)
{
		__vmclrmap((void *)start, end - start);
}
