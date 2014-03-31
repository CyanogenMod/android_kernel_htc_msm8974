/*
 *  linux/arch/m32r/mm/fault.c
 *
 *  Copyright (c) 2001, 2002  Hitoshi Yamamoto, and H. Kondo
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
#include <linux/vt_kern.h>              

#include <asm/m32r.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/hardirq.h>
#include <asm/mmu_context.h>

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

void do_BUG(const char *file, int line)
{
	bust_spinlocks(1);
	printk("kernel BUG at %s:%d!\n", file, line);
}

asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long error_code,
  unsigned long address)
{


	bust_spinlocks(1);

	if (address < PAGE_SIZE)
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
	else
		printk(KERN_ALERT "Unable to handle kernel paging request");
	printk(" at virtual address %08lx\n",address);
	printk(" printing bpc:\n");
	printk(KERN_ALERT "bpc = %08lx\n", regs->bpc);

	die("Oops", regs, error_code);
	bust_spinlocks(0);
	do_exit(SIGKILL);
}

void update_mmu_cache(struct vm_area_struct *vma, unsigned long addr,
	pte_t *ptep)
{
	BUG();
}

void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long page)
{
	BUG();
}

void local_flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
	unsigned long end)
{
	BUG();
}

void local_flush_tlb_mm(struct mm_struct *mm)
{
	BUG();
}

void local_flush_tlb_all(void)
{
	BUG();
}
