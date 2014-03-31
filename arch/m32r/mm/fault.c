/*
 *  linux/arch/m32r/mm/fault.c
 *
 *  Copyright (c) 2001, 2002  Hitoshi Yamamoto, and H. Kondo
 *  Copyright (c) 2004  Naoto Sugai, NIIBE Yutaka
 *
 *  Some code taken from i386 version.
 *    Copyright (C) 1995  Linus Torvalds
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
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/vt_kern.h>		
#include <linux/highmem.h>
#include <linux/module.h>

#include <asm/m32r.h>
#include <asm/uaccess.h>
#include <asm/hardirq.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>

extern void die(const char *, struct pt_regs *, long);

#ifndef CONFIG_SMP
asmlinkage unsigned int tlb_entry_i_dat;
asmlinkage unsigned int tlb_entry_d_dat;
#define tlb_entry_i tlb_entry_i_dat
#define tlb_entry_d tlb_entry_d_dat
#else
unsigned int tlb_entry_i_dat[NR_CPUS];
unsigned int tlb_entry_d_dat[NR_CPUS];
#define tlb_entry_i tlb_entry_i_dat[smp_processor_id()]
#define tlb_entry_d tlb_entry_d_dat[smp_processor_id()]
#endif

extern void init_tlb(void);

#define ACE_PROTECTION		1
#define ACE_WRITE		2
#define ACE_USERMODE		4
#define ACE_INSTRUCTION		8

asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long error_code,
  unsigned long address)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct * vma;
	unsigned long page, addr;
	int write;
	int fault;
	siginfo_t info;

	if (regs->psw & M32R_PSW_BIE)
		local_irq_enable();

	tsk = current;

	info.si_code = SEGV_MAPERR;

	if (address >= TASK_SIZE && !(error_code & ACE_USERMODE))
		goto vmalloc_fault;

	mm = tsk->mm;

	if (in_atomic() || !mm)
		goto bad_area_nosemaphore;

	if (!down_read_trylock(&mm->mmap_sem)) {
		if ((error_code & ACE_USERMODE) == 0 &&
		    !search_exception_tables(regs->psw))
			goto bad_area_nosemaphore;
		down_read(&mm->mmap_sem);
	}

	vma = find_vma(mm, address);
	if (!vma)
		goto bad_area;
	if (vma->vm_start <= address)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;

	if (error_code & ACE_USERMODE) {
		if (address + 4 < regs->spu)
			goto bad_area;
	}

	if (expand_stack(vma, address))
		goto bad_area;
good_area:
	info.si_code = SEGV_ACCERR;
	write = 0;
	switch (error_code & (ACE_WRITE|ACE_PROTECTION)) {
		default:	
			
		case ACE_WRITE:	
			if (!(vma->vm_flags & VM_WRITE))
				goto bad_area;
			write++;
			break;
		case ACE_PROTECTION:	
		case 0:		
			if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
				goto bad_area;
	}

	if ((error_code & ACE_INSTRUCTION) && !(vma->vm_flags & VM_EXEC))
	  goto bad_area;

	addr = (address & PAGE_MASK);
	set_thread_fault_code(error_code);
	fault = handle_mm_fault(mm, vma, addr, write ? FAULT_FLAG_WRITE : 0);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto do_sigbus;
		BUG();
	}
	if (fault & VM_FAULT_MAJOR)
		tsk->maj_flt++;
	else
		tsk->min_flt++;
	set_thread_fault_code(0);
	up_read(&mm->mmap_sem);
	return;

bad_area:
	up_read(&mm->mmap_sem);

bad_area_nosemaphore:
	
	if (error_code & ACE_USERMODE) {
		tsk->thread.address = address;
		tsk->thread.error_code = error_code | (address >= TASK_SIZE);
		tsk->thread.trap_no = 14;
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		
		info.si_addr = (void __user *)address;
		force_sig_info(SIGSEGV, &info, tsk);
		return;
	}

no_context:
	
	if (fixup_exception(regs))
		return;


	bust_spinlocks(1);

	if (address < PAGE_SIZE)
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
	else
		printk(KERN_ALERT "Unable to handle kernel paging request");
	printk(" at virtual address %08lx\n",address);
	printk(KERN_ALERT " printing bpc:\n");
	printk("%08lx\n", regs->bpc);
	page = *(unsigned long *)MPTB;
	page = ((unsigned long *) page)[address >> PGDIR_SHIFT];
	printk(KERN_ALERT "*pde = %08lx\n", page);
	if (page & _PAGE_PRESENT) {
		page &= PAGE_MASK;
		address &= 0x003ff000;
		page = ((unsigned long *) __va(page))[address >> PAGE_SHIFT];
		printk(KERN_ALERT "*pte = %08lx\n", page);
	}
	die("Oops", regs, error_code);
	bust_spinlocks(0);
	do_exit(SIGKILL);

out_of_memory:
	up_read(&mm->mmap_sem);
	if (!(error_code & ACE_USERMODE))
		goto no_context;
	pagefault_out_of_memory();
	return;

do_sigbus:
	up_read(&mm->mmap_sem);

	
	if (!(error_code & ACE_USERMODE))
		goto no_context;

	tsk->thread.address = address;
	tsk->thread.error_code = error_code;
	tsk->thread.trap_no = 14;
	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void __user *)address;
	force_sig_info(SIGBUS, &info, tsk);
	return;

vmalloc_fault:
	{
		int offset = pgd_index(address);
		pgd_t *pgd, *pgd_k;
		pmd_t *pmd, *pmd_k;
		pte_t *pte_k;

		pgd = (pgd_t *)*(unsigned long *)MPTB;
		pgd = offset + (pgd_t *)pgd;
		pgd_k = init_mm.pgd + offset;

		if (!pgd_present(*pgd_k))
			goto no_context;


		pmd = pmd_offset(pgd, address);
		pmd_k = pmd_offset(pgd_k, address);
		if (!pmd_present(*pmd_k))
			goto no_context;
		set_pmd(pmd, *pmd_k);

		pte_k = pte_offset_kernel(pmd_k, address);
		if (!pte_present(*pte_k))
			goto no_context;

		addr = (address & PAGE_MASK);
		set_thread_fault_code(error_code);
		update_mmu_cache(NULL, addr, pte_k);
		set_thread_fault_code(0);
		return;
	}
}

#define TLB_MASK	(NR_TLB_ENTRIES - 1)
#define ITLB_END	(unsigned long *)(ITLB_BASE + (NR_TLB_ENTRIES * 8))
#define DTLB_END	(unsigned long *)(DTLB_BASE + (NR_TLB_ENTRIES * 8))
void update_mmu_cache(struct vm_area_struct *vma, unsigned long vaddr,
	pte_t *ptep)
{
	volatile unsigned long *entry1, *entry2;
	unsigned long pte_data, flags;
	unsigned int *entry_dat;
	int inst = get_thread_fault_code() & ACE_INSTRUCTION;
	int i;

	
	if (vma && current->active_mm != vma->vm_mm)
		return;

	local_irq_save(flags);

	vaddr = (vaddr & PAGE_MASK) | get_asid();

	pte_data = pte_val(*ptep);

#ifdef CONFIG_CHIP_OPSP
	entry1 = (unsigned long *)ITLB_BASE;
	for (i = 0; i < NR_TLB_ENTRIES; i++) {
		if (*entry1++ == vaddr) {
			set_tlb_data(entry1, pte_data);
			break;
		}
		entry1++;
	}
	entry2 = (unsigned long *)DTLB_BASE;
	for (i = 0; i < NR_TLB_ENTRIES; i++) {
		if (*entry2++ == vaddr) {
			set_tlb_data(entry2, pte_data);
			break;
		}
		entry2++;
	}
#else
	__asm__ __volatile__ (
		"seth	%0, #high(%4)	\n\t"
		"st	%2, @(%5, %0)	\n\t"
		"ldi	%1, #1		\n\t"
		"st	%1, @(%6, %0)	\n\t"
		"add3	r4, %0, %7	\n\t"
		".fillinsn		\n"
		"1:			\n\t"
		"ld	%1, @(%6, %0)	\n\t"
		"bnez	%1, 1b		\n\t"
		"ld	%0, @r4+	\n\t"
		"ld	%1, @r4		\n\t"
		"st	%3, @+%0	\n\t"
		"st	%3, @+%1	\n\t"
		: "=&r" (entry1), "=&r" (entry2)
		: "r" (vaddr), "r" (pte_data), "i" (MMU_REG_BASE),
		"i" (MSVA_offset), "i" (MTOP_offset), "i" (MIDXI_offset)
		: "r4", "memory"
	);
#endif

	if ((!inst && entry2 >= DTLB_END) || (inst && entry1 >= ITLB_END))
		goto notfound;

found:
	local_irq_restore(flags);

	return;

	
notfound:
	if (!inst) {
		entry2 = (unsigned long *)DTLB_BASE;
		entry_dat = &tlb_entry_d;
	} else {
		entry2 = (unsigned long *)ITLB_BASE;
		entry_dat = &tlb_entry_i;
	}
	entry1 = entry2 + (((*entry_dat - 1) & TLB_MASK) << 1);

	for (i = 0 ; i < NR_TLB_ENTRIES ; i++) {
		if (!(entry1[1] & 2))	
			break;

		if (entry1 != entry2)
			entry1 -= 2;
		else
			entry1 += TLB_MASK << 1;
	}

	if (i >= NR_TLB_ENTRIES) {	
		entry1 = entry2 + (*entry_dat << 1);
		*entry_dat = (*entry_dat + 1) & TLB_MASK;
	}
	*entry1++ = vaddr;	
	set_tlb_data(entry1, pte_data);

	goto found;
}

void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long page)
{
	if (vma->vm_mm && mm_context(vma->vm_mm) != NO_CONTEXT) {
		unsigned long flags;

		local_irq_save(flags);
		page &= PAGE_MASK;
		page |= (mm_context(vma->vm_mm) & MMU_CONTEXT_ASID_MASK);
		__flush_tlb_page(page);
		local_irq_restore(flags);
	}
}

void local_flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
	unsigned long end)
{
	struct mm_struct *mm;

	mm = vma->vm_mm;
	if (mm_context(mm) != NO_CONTEXT) {
		unsigned long flags;
		int size;

		local_irq_save(flags);
		size = (end - start + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		if (size > (NR_TLB_ENTRIES / 4)) { 
			mm_context(mm) = NO_CONTEXT;
			if (mm == current->mm)
				activate_context(mm);
		} else {
			unsigned long asid;

			asid = mm_context(mm) & MMU_CONTEXT_ASID_MASK;
			start &= PAGE_MASK;
			end += (PAGE_SIZE - 1);
			end &= PAGE_MASK;

			start |= asid;
			end   |= asid;
			while (start < end) {
				__flush_tlb_page(start);
				start += PAGE_SIZE;
			}
		}
		local_irq_restore(flags);
	}
}

void local_flush_tlb_mm(struct mm_struct *mm)
{
	
	
	if (mm_context(mm) != NO_CONTEXT) {
		unsigned long flags;

		local_irq_save(flags);
		mm_context(mm) = NO_CONTEXT;
		if (mm == current->mm)
			activate_context(mm);
		local_irq_restore(flags);
	}
}

void local_flush_tlb_all(void)
{
	unsigned long flags;

	local_irq_save(flags);
	__flush_tlb_all();
	local_irq_restore(flags);
}

void __init init_mmu(void)
{
	tlb_entry_i = 0;
	tlb_entry_d = 0;
	mmu_context_cache = MMU_CONTEXT_FIRST_VERSION;
	set_asid(mmu_context_cache & MMU_CONTEXT_ASID_MASK);
	*(volatile unsigned long *)MPTB = (unsigned long)swapper_pg_dir;
}
