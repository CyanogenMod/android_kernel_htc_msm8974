/*
 *  Copyright (C) 1995  Linus Torvalds
 *  Copyright (C) 2001, 2002 Andi Kleen, SuSE Labs.
 *  Copyright (C) 2008-2009, Red Hat Inc., Ingo Molnar
 */
#include <linux/magic.h>		
#include <linux/sched.h>		
#include <linux/kdebug.h>		
#include <linux/module.h>		
#include <linux/bootmem.h>		
#include <linux/kprobes.h>		
#include <linux/mmiotrace.h>		
#include <linux/perf_event.h>		
#include <linux/hugetlb.h>		
#include <linux/prefetch.h>		

#include <asm/traps.h>			
#include <asm/pgalloc.h>		
#include <asm/kmemcheck.h>		
#include <asm/fixmap.h>			

enum x86_pf_error_code {

	PF_PROT		=		1 << 0,
	PF_WRITE	=		1 << 1,
	PF_USER		=		1 << 2,
	PF_RSVD		=		1 << 3,
	PF_INSTR	=		1 << 4,
};

static inline int __kprobes
kmmio_fault(struct pt_regs *regs, unsigned long addr)
{
	if (unlikely(is_kmmio_active()))
		if (kmmio_handler(regs, addr) == 1)
			return -1;
	return 0;
}

static inline int __kprobes notify_page_fault(struct pt_regs *regs)
{
	int ret = 0;

	
	if (kprobes_built_in() && !user_mode_vm(regs)) {
		preempt_disable();
		if (kprobe_running() && kprobe_fault_handler(regs, 14))
			ret = 1;
		preempt_enable();
	}

	return ret;
}

static inline int
check_prefetch_opcode(struct pt_regs *regs, unsigned char *instr,
		      unsigned char opcode, int *prefetch)
{
	unsigned char instr_hi = opcode & 0xf0;
	unsigned char instr_lo = opcode & 0x0f;

	switch (instr_hi) {
	case 0x20:
	case 0x30:
		return ((instr_lo & 7) == 0x6);
#ifdef CONFIG_X86_64
	case 0x40:
		return (!user_mode(regs) || user_64bit_mode(regs));
#endif
	case 0x60:
		
		return (instr_lo & 0xC) == 0x4;
	case 0xF0:
		
		return !instr_lo || (instr_lo>>1) == 1;
	case 0x00:
		
		if (probe_kernel_address(instr, opcode))
			return 0;

		*prefetch = (instr_lo == 0xF) &&
			(opcode == 0x0D || opcode == 0x18);
		return 0;
	default:
		return 0;
	}
}

static int
is_prefetch(struct pt_regs *regs, unsigned long error_code, unsigned long addr)
{
	unsigned char *max_instr;
	unsigned char *instr;
	int prefetch = 0;

	if (error_code & PF_INSTR)
		return 0;

	instr = (void *)convert_ip_to_linear(current, regs);
	max_instr = instr + 15;

	if (user_mode(regs) && instr >= (unsigned char *)TASK_SIZE)
		return 0;

	while (instr < max_instr) {
		unsigned char opcode;

		if (probe_kernel_address(instr, opcode))
			break;

		instr++;

		if (!check_prefetch_opcode(regs, instr, opcode, &prefetch))
			break;
	}
	return prefetch;
}

static void
force_sig_info_fault(int si_signo, int si_code, unsigned long address,
		     struct task_struct *tsk, int fault)
{
	unsigned lsb = 0;
	siginfo_t info;

	info.si_signo	= si_signo;
	info.si_errno	= 0;
	info.si_code	= si_code;
	info.si_addr	= (void __user *)address;
	if (fault & VM_FAULT_HWPOISON_LARGE)
		lsb = hstate_index_to_shift(VM_FAULT_GET_HINDEX(fault)); 
	if (fault & VM_FAULT_HWPOISON)
		lsb = PAGE_SHIFT;
	info.si_addr_lsb = lsb;

	force_sig_info(si_signo, &info, tsk);
}

DEFINE_SPINLOCK(pgd_lock);
LIST_HEAD(pgd_list);

#ifdef CONFIG_X86_32
static inline pmd_t *vmalloc_sync_one(pgd_t *pgd, unsigned long address)
{
	unsigned index = pgd_index(address);
	pgd_t *pgd_k;
	pud_t *pud, *pud_k;
	pmd_t *pmd, *pmd_k;

	pgd += index;
	pgd_k = init_mm.pgd + index;

	if (!pgd_present(*pgd_k))
		return NULL;

	pud = pud_offset(pgd, address);
	pud_k = pud_offset(pgd_k, address);
	if (!pud_present(*pud_k))
		return NULL;

	pmd = pmd_offset(pud, address);
	pmd_k = pmd_offset(pud_k, address);
	if (!pmd_present(*pmd_k))
		return NULL;

	if (!pmd_present(*pmd))
		set_pmd(pmd, *pmd_k);
	else
		BUG_ON(pmd_page(*pmd) != pmd_page(*pmd_k));

	return pmd_k;
}

void vmalloc_sync_all(void)
{
	unsigned long address;

	if (SHARED_KERNEL_PMD)
		return;

	for (address = VMALLOC_START & PMD_MASK;
	     address >= TASK_SIZE && address < FIXADDR_TOP;
	     address += PMD_SIZE) {
		struct page *page;

		spin_lock(&pgd_lock);
		list_for_each_entry(page, &pgd_list, lru) {
			spinlock_t *pgt_lock;
			pmd_t *ret;

			
			pgt_lock = &pgd_page_get_mm(page)->page_table_lock;

			spin_lock(pgt_lock);
			ret = vmalloc_sync_one(page_address(page), address);
			spin_unlock(pgt_lock);

			if (!ret)
				break;
		}
		spin_unlock(&pgd_lock);
	}
}

static noinline __kprobes int vmalloc_fault(unsigned long address)
{
	unsigned long pgd_paddr;
	pmd_t *pmd_k;
	pte_t *pte_k;

	
	if (!(address >= VMALLOC_START && address < VMALLOC_END))
		return -1;

	WARN_ON_ONCE(in_nmi());

	pgd_paddr = read_cr3();
	pmd_k = vmalloc_sync_one(__va(pgd_paddr), address);
	if (!pmd_k)
		return -1;

	pte_k = pte_offset_kernel(pmd_k, address);
	if (!pte_present(*pte_k))
		return -1;

	return 0;
}

static inline void
check_v8086_mode(struct pt_regs *regs, unsigned long address,
		 struct task_struct *tsk)
{
	unsigned long bit;

	if (!v8086_mode(regs))
		return;

	bit = (address - 0xA0000) >> PAGE_SHIFT;
	if (bit < 32)
		tsk->thread.screen_bitmap |= 1 << bit;
}

static bool low_pfn(unsigned long pfn)
{
	return pfn < max_low_pfn;
}

static void dump_pagetable(unsigned long address)
{
	pgd_t *base = __va(read_cr3());
	pgd_t *pgd = &base[pgd_index(address)];
	pmd_t *pmd;
	pte_t *pte;

#ifdef CONFIG_X86_PAE
	printk("*pdpt = %016Lx ", pgd_val(*pgd));
	if (!low_pfn(pgd_val(*pgd) >> PAGE_SHIFT) || !pgd_present(*pgd))
		goto out;
#endif
	pmd = pmd_offset(pud_offset(pgd, address), address);
	printk(KERN_CONT "*pde = %0*Lx ", sizeof(*pmd) * 2, (u64)pmd_val(*pmd));

	if (!low_pfn(pmd_pfn(*pmd)) || !pmd_present(*pmd) || pmd_large(*pmd))
		goto out;

	pte = pte_offset_kernel(pmd, address);
	printk("*pte = %0*Lx ", sizeof(*pte) * 2, (u64)pte_val(*pte));
out:
	printk("\n");
}

#else 

void vmalloc_sync_all(void)
{
	sync_global_pgds(VMALLOC_START & PGDIR_MASK, VMALLOC_END);
}

static noinline __kprobes int vmalloc_fault(unsigned long address)
{
	pgd_t *pgd, *pgd_ref;
	pud_t *pud, *pud_ref;
	pmd_t *pmd, *pmd_ref;
	pte_t *pte, *pte_ref;

	
	if (!(address >= VMALLOC_START && address < VMALLOC_END))
		return -1;

	WARN_ON_ONCE(in_nmi());

	pgd = pgd_offset(current->active_mm, address);
	pgd_ref = pgd_offset_k(address);
	if (pgd_none(*pgd_ref))
		return -1;

	if (pgd_none(*pgd))
		set_pgd(pgd, *pgd_ref);
	else
		BUG_ON(pgd_page_vaddr(*pgd) != pgd_page_vaddr(*pgd_ref));


	pud = pud_offset(pgd, address);
	pud_ref = pud_offset(pgd_ref, address);
	if (pud_none(*pud_ref))
		return -1;

	if (pud_none(*pud) || pud_page_vaddr(*pud) != pud_page_vaddr(*pud_ref))
		BUG();

	pmd = pmd_offset(pud, address);
	pmd_ref = pmd_offset(pud_ref, address);
	if (pmd_none(*pmd_ref))
		return -1;

	if (pmd_none(*pmd) || pmd_page(*pmd) != pmd_page(*pmd_ref))
		BUG();

	pte_ref = pte_offset_kernel(pmd_ref, address);
	if (!pte_present(*pte_ref))
		return -1;

	pte = pte_offset_kernel(pmd, address);

	if (!pte_present(*pte) || pte_pfn(*pte) != pte_pfn(*pte_ref))
		BUG();

	return 0;
}

#ifdef CONFIG_CPU_SUP_AMD
static const char errata93_warning[] =
KERN_ERR 
"******* Your BIOS seems to not contain a fix for K8 errata #93\n"
"******* Working around it, but it may cause SEGVs or burn power.\n"
"******* Please consider a BIOS update.\n"
"******* Disabling USB legacy in the BIOS may also help.\n";
#endif

static inline void
check_v8086_mode(struct pt_regs *regs, unsigned long address,
		 struct task_struct *tsk)
{
}

static int bad_address(void *p)
{
	unsigned long dummy;

	return probe_kernel_address((unsigned long *)p, dummy);
}

static void dump_pagetable(unsigned long address)
{
	pgd_t *base = __va(read_cr3() & PHYSICAL_PAGE_MASK);
	pgd_t *pgd = base + pgd_index(address);
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if (bad_address(pgd))
		goto bad;

	printk("PGD %lx ", pgd_val(*pgd));

	if (!pgd_present(*pgd))
		goto out;

	pud = pud_offset(pgd, address);
	if (bad_address(pud))
		goto bad;

	printk("PUD %lx ", pud_val(*pud));
	if (!pud_present(*pud) || pud_large(*pud))
		goto out;

	pmd = pmd_offset(pud, address);
	if (bad_address(pmd))
		goto bad;

	printk("PMD %lx ", pmd_val(*pmd));
	if (!pmd_present(*pmd) || pmd_large(*pmd))
		goto out;

	pte = pte_offset_kernel(pmd, address);
	if (bad_address(pte))
		goto bad;

	printk("PTE %lx", pte_val(*pte));
out:
	printk("\n");
	return;
bad:
	printk("BAD\n");
}

#endif 

static int is_errata93(struct pt_regs *regs, unsigned long address)
{
#if defined(CONFIG_X86_64) && defined(CONFIG_CPU_SUP_AMD)
	if (boot_cpu_data.x86_vendor != X86_VENDOR_AMD
	    || boot_cpu_data.x86 != 0xf)
		return 0;

	if (address != regs->ip)
		return 0;

	if ((address >> 32) != 0)
		return 0;

	address |= 0xffffffffUL << 32;
	if ((address >= (u64)_stext && address <= (u64)_etext) ||
	    (address >= MODULES_VADDR && address <= MODULES_END)) {
		printk_once(errata93_warning);
		regs->ip = address;
		return 1;
	}
#endif
	return 0;
}

static int is_errata100(struct pt_regs *regs, unsigned long address)
{
#ifdef CONFIG_X86_64
	if ((regs->cs == __USER32_CS || (regs->cs & (1<<2))) && (address >> 32))
		return 1;
#endif
	return 0;
}

static int is_f00f_bug(struct pt_regs *regs, unsigned long address)
{
#ifdef CONFIG_X86_F00F_BUG
	unsigned long nr;

	if (boot_cpu_data.f00f_bug) {
		nr = (address - idt_descr.address) >> 3;

		if (nr == 6) {
			do_invalid_op(regs, 0);
			return 1;
		}
	}
#endif
	return 0;
}

static const char nx_warning[] = KERN_CRIT
"kernel tried to execute NX-protected page - exploit attempt? (uid: %d)\n";

static void
show_fault_oops(struct pt_regs *regs, unsigned long error_code,
		unsigned long address)
{
	if (!oops_may_print())
		return;

	if (error_code & PF_INSTR) {
		unsigned int level;

		pte_t *pte = lookup_address(address, &level);

		if (pte && pte_present(*pte) && !pte_exec(*pte))
			printk(nx_warning, current_uid());
	}

	printk(KERN_ALERT "BUG: unable to handle kernel ");
	if (address < PAGE_SIZE)
		printk(KERN_CONT "NULL pointer dereference");
	else
		printk(KERN_CONT "paging request");

	printk(KERN_CONT " at %p\n", (void *) address);
	printk(KERN_ALERT "IP:");
	printk_address(regs->ip, 1);

	dump_pagetable(address);
}

static noinline void
pgtable_bad(struct pt_regs *regs, unsigned long error_code,
	    unsigned long address)
{
	struct task_struct *tsk;
	unsigned long flags;
	int sig;

	flags = oops_begin();
	tsk = current;
	sig = SIGKILL;

	printk(KERN_ALERT "%s: Corrupted page table at address %lx\n",
	       tsk->comm, address);
	dump_pagetable(address);

	tsk->thread.cr2		= address;
	tsk->thread.trap_nr	= X86_TRAP_PF;
	tsk->thread.error_code	= error_code;

	if (__die("Bad pagetable", regs, error_code))
		sig = 0;

	oops_end(flags, regs, sig);
}

static noinline void
no_context(struct pt_regs *regs, unsigned long error_code,
	   unsigned long address, int signal, int si_code)
{
	struct task_struct *tsk = current;
	unsigned long *stackend;
	unsigned long flags;
	int sig;

	
	if (fixup_exception(regs)) {
		if (current_thread_info()->sig_on_uaccess_error && signal) {
			tsk->thread.trap_nr = X86_TRAP_PF;
			tsk->thread.error_code = error_code | PF_USER;
			tsk->thread.cr2 = address;

			
			force_sig_info_fault(signal, si_code, address, tsk, 0);
		}
		return;
	}

	if (is_prefetch(regs, error_code, address))
		return;

	if (is_errata93(regs, address))
		return;

	flags = oops_begin();

	show_fault_oops(regs, error_code, address);

	stackend = end_of_stack(tsk);
	if (tsk != &init_task && *stackend != STACK_END_MAGIC)
		printk(KERN_EMERG "Thread overran stack, or stack corrupted\n");

	tsk->thread.cr2		= address;
	tsk->thread.trap_nr	= X86_TRAP_PF;
	tsk->thread.error_code	= error_code;

	sig = SIGKILL;
	if (__die("Oops", regs, error_code))
		sig = 0;

	
	printk(KERN_DEFAULT "CR2: %016lx\n", address);

	oops_end(flags, regs, sig);
}

static inline void
show_signal_msg(struct pt_regs *regs, unsigned long error_code,
		unsigned long address, struct task_struct *tsk)
{
	if (!unhandled_signal(tsk, SIGSEGV))
		return;

	if (!printk_ratelimit())
		return;

	printk("%s%s[%d]: segfault at %lx ip %p sp %p error %lx",
		task_pid_nr(tsk) > 1 ? KERN_INFO : KERN_EMERG,
		tsk->comm, task_pid_nr(tsk), address,
		(void *)regs->ip, (void *)regs->sp, error_code);

	print_vma_addr(KERN_CONT " in ", regs->ip);

	printk(KERN_CONT "\n");
}

static void
__bad_area_nosemaphore(struct pt_regs *regs, unsigned long error_code,
		       unsigned long address, int si_code)
{
	struct task_struct *tsk = current;

	
	if (error_code & PF_USER) {
		local_irq_enable();

		if (is_prefetch(regs, error_code, address))
			return;

		if (is_errata100(regs, address))
			return;

#ifdef CONFIG_X86_64
		if (unlikely((error_code & PF_INSTR) &&
			     ((address & ~0xfff) == VSYSCALL_START))) {
			if (emulate_vsyscall(regs, address))
				return;
		}
#endif

		if (unlikely(show_unhandled_signals))
			show_signal_msg(regs, error_code, address, tsk);

		
		tsk->thread.cr2		= address;
		tsk->thread.error_code	= error_code | (address >= TASK_SIZE);
		tsk->thread.trap_nr	= X86_TRAP_PF;

		force_sig_info_fault(SIGSEGV, si_code, address, tsk, 0);

		return;
	}

	if (is_f00f_bug(regs, address))
		return;

	no_context(regs, error_code, address, SIGSEGV, si_code);
}

static noinline void
bad_area_nosemaphore(struct pt_regs *regs, unsigned long error_code,
		     unsigned long address)
{
	__bad_area_nosemaphore(regs, error_code, address, SEGV_MAPERR);
}

static void
__bad_area(struct pt_regs *regs, unsigned long error_code,
	   unsigned long address, int si_code)
{
	struct mm_struct *mm = current->mm;

	up_read(&mm->mmap_sem);

	__bad_area_nosemaphore(regs, error_code, address, si_code);
}

static noinline void
bad_area(struct pt_regs *regs, unsigned long error_code, unsigned long address)
{
	__bad_area(regs, error_code, address, SEGV_MAPERR);
}

static noinline void
bad_area_access_error(struct pt_regs *regs, unsigned long error_code,
		      unsigned long address)
{
	__bad_area(regs, error_code, address, SEGV_ACCERR);
}

static void
out_of_memory(struct pt_regs *regs, unsigned long error_code,
	      unsigned long address)
{
	up_read(&current->mm->mmap_sem);

	pagefault_out_of_memory();
}

static void
do_sigbus(struct pt_regs *regs, unsigned long error_code, unsigned long address,
	  unsigned int fault)
{
	struct task_struct *tsk = current;
	struct mm_struct *mm = tsk->mm;
	int code = BUS_ADRERR;

	up_read(&mm->mmap_sem);

	
	if (!(error_code & PF_USER)) {
		no_context(regs, error_code, address, SIGBUS, BUS_ADRERR);
		return;
	}

	
	if (is_prefetch(regs, error_code, address))
		return;

	tsk->thread.cr2		= address;
	tsk->thread.error_code	= error_code;
	tsk->thread.trap_nr	= X86_TRAP_PF;

#ifdef CONFIG_MEMORY_FAILURE
	if (fault & (VM_FAULT_HWPOISON|VM_FAULT_HWPOISON_LARGE)) {
		printk(KERN_ERR
	"MCE: Killing %s:%d due to hardware memory corruption fault at %lx\n",
			tsk->comm, tsk->pid, address);
		code = BUS_MCEERR_AR;
	}
#endif
	force_sig_info_fault(SIGBUS, code, address, tsk, fault);
}

static noinline int
mm_fault_error(struct pt_regs *regs, unsigned long error_code,
	       unsigned long address, unsigned int fault)
{
	if (fatal_signal_pending(current)) {
		if (!(fault & VM_FAULT_RETRY))
			up_read(&current->mm->mmap_sem);
		if (!(error_code & PF_USER))
			no_context(regs, error_code, address, 0, 0);
		return 1;
	}
	if (!(fault & VM_FAULT_ERROR))
		return 0;

	if (fault & VM_FAULT_OOM) {
		
		if (!(error_code & PF_USER)) {
			up_read(&current->mm->mmap_sem);
			no_context(regs, error_code, address,
				   SIGSEGV, SEGV_MAPERR);
			return 1;
		}

		out_of_memory(regs, error_code, address);
	} else {
		if (fault & (VM_FAULT_SIGBUS|VM_FAULT_HWPOISON|
			     VM_FAULT_HWPOISON_LARGE))
			do_sigbus(regs, error_code, address, fault);
		else
			BUG();
	}
	return 1;
}

static int spurious_fault_check(unsigned long error_code, pte_t *pte)
{
	if ((error_code & PF_WRITE) && !pte_write(*pte))
		return 0;

	if ((error_code & PF_INSTR) && !pte_exec(*pte))
		return 0;

	return 1;
}

static noinline __kprobes int
spurious_fault(unsigned long error_code, unsigned long address)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	int ret;

	
	if (error_code & (PF_USER | PF_RSVD))
		return 0;

	pgd = init_mm.pgd + pgd_index(address);
	if (!pgd_present(*pgd))
		return 0;

	pud = pud_offset(pgd, address);
	if (!pud_present(*pud))
		return 0;

	if (pud_large(*pud))
		return spurious_fault_check(error_code, (pte_t *) pud);

	pmd = pmd_offset(pud, address);
	if (!pmd_present(*pmd))
		return 0;

	if (pmd_large(*pmd))
		return spurious_fault_check(error_code, (pte_t *) pmd);

	pte = pte_offset_kernel(pmd, address);
	if (!(pte_flags(*pte) & _PAGE_PRESENT))
		return 0;

	ret = spurious_fault_check(error_code, pte);
	if (!ret)
		return 0;

	ret = spurious_fault_check(error_code, (pte_t *) pmd);
	WARN_ONCE(!ret, "PMD has incorrect permission bits\n");

	return ret;
}

int show_unhandled_signals = 1;

static inline int
access_error(unsigned long error_code, struct vm_area_struct *vma)
{
	if (error_code & PF_WRITE) {
		
		if (unlikely(!(vma->vm_flags & VM_WRITE)))
			return 1;
		return 0;
	}

	
	if (unlikely(error_code & PF_PROT))
		return 1;

	
	if (unlikely(!(vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE))))
		return 1;

	return 0;
}

static int fault_in_kernel_space(unsigned long address)
{
	return address >= TASK_SIZE_MAX;
}

dotraplinkage void __kprobes
do_page_fault(struct pt_regs *regs, unsigned long error_code)
{
	struct vm_area_struct *vma;
	struct task_struct *tsk;
	unsigned long address;
	struct mm_struct *mm;
	int fault;
	int write = error_code & PF_WRITE;
	unsigned int flags = FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_KILLABLE |
					(write ? FAULT_FLAG_WRITE : 0);

	tsk = current;
	mm = tsk->mm;

	
	address = read_cr2();

	if (kmemcheck_active(regs))
		kmemcheck_hide(regs);
	prefetchw(&mm->mmap_sem);

	if (unlikely(kmmio_fault(regs, address)))
		return;

	if (unlikely(fault_in_kernel_space(address))) {
		if (!(error_code & (PF_RSVD | PF_USER | PF_PROT))) {
			if (vmalloc_fault(address) >= 0)
				return;

			if (kmemcheck_fault(regs, address, error_code))
				return;
		}

		
		if (spurious_fault(error_code, address))
			return;

		
		if (notify_page_fault(regs))
			return;
		bad_area_nosemaphore(regs, error_code, address);

		return;
	}

	
	if (unlikely(notify_page_fault(regs)))
		return;
	if (user_mode_vm(regs)) {
		local_irq_enable();
		error_code |= PF_USER;
	} else {
		if (regs->flags & X86_EFLAGS_IF)
			local_irq_enable();
	}

	if (unlikely(error_code & PF_RSVD))
		pgtable_bad(regs, error_code, address);

	perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS, 1, regs, address);

	if (unlikely(in_atomic() || !mm)) {
		bad_area_nosemaphore(regs, error_code, address);
		return;
	}

	if (unlikely(!down_read_trylock(&mm->mmap_sem))) {
		if ((error_code & PF_USER) == 0 &&
		    !search_exception_tables(regs->ip)) {
			bad_area_nosemaphore(regs, error_code, address);
			return;
		}
retry:
		down_read(&mm->mmap_sem);
	} else {
		might_sleep();
	}

	vma = find_vma(mm, address);
	if (unlikely(!vma)) {
		bad_area(regs, error_code, address);
		return;
	}
	if (likely(vma->vm_start <= address))
		goto good_area;
	if (unlikely(!(vma->vm_flags & VM_GROWSDOWN))) {
		bad_area(regs, error_code, address);
		return;
	}
	if (error_code & PF_USER) {
		if (unlikely(address + 65536 + 32 * sizeof(unsigned long) < regs->sp)) {
			bad_area(regs, error_code, address);
			return;
		}
	}
	if (unlikely(expand_stack(vma, address))) {
		bad_area(regs, error_code, address);
		return;
	}

good_area:
	if (unlikely(access_error(error_code, vma))) {
		bad_area_access_error(regs, error_code, address);
		return;
	}

	fault = handle_mm_fault(mm, vma, address, flags);

	if (unlikely(fault & (VM_FAULT_RETRY|VM_FAULT_ERROR))) {
		if (mm_fault_error(regs, error_code, address, fault))
			return;
	}

	if (flags & FAULT_FLAG_ALLOW_RETRY) {
		if (fault & VM_FAULT_MAJOR) {
			tsk->maj_flt++;
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MAJ, 1,
				      regs, address);
		} else {
			tsk->min_flt++;
			perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MIN, 1,
				      regs, address);
		}
		if (fault & VM_FAULT_RETRY) {
			flags &= ~FAULT_FLAG_ALLOW_RETRY;
			goto retry;
		}
	}

	check_v8086_mode(regs, address, tsk);

	up_read(&mm->mmap_sem);
}
