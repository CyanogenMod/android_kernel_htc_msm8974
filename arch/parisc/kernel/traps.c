/*
 *  linux/arch/parisc/traps.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 1999, 2000  Philipp Rumpf <prumpf@tux.org>
 */


#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/console.h>
#include <linux/bug.h>

#include <asm/assembly.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/unaligned.h>
#include <linux/atomic.h>
#include <asm/smp.h>
#include <asm/pdc.h>
#include <asm/pdc_chassis.h>
#include <asm/unwind.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#include "../math-emu/math-emu.h"	

#define PRINT_USER_FAULTS 
			  

#if defined(CONFIG_SMP) || defined(CONFIG_DEBUG_SPINLOCK)
DEFINE_SPINLOCK(pa_dbit_lock);
#endif

static void parisc_show_stack(struct task_struct *task, unsigned long *sp,
	struct pt_regs *regs);

static int printbinary(char *buf, unsigned long x, int nbits)
{
	unsigned long mask = 1UL << (nbits - 1);
	while (mask != 0) {
		*buf++ = (mask & x ? '1' : '0');
		mask >>= 1;
	}
	*buf = '\0';

	return nbits;
}

#ifdef CONFIG_64BIT
#define RFMT "%016lx"
#else
#define RFMT "%08lx"
#endif
#define FFMT "%016llx"	

#define PRINTREGS(lvl,r,f,fmt,x)	\
	printk("%s%s%02d-%02d  " fmt " " fmt " " fmt " " fmt "\n",	\
		lvl, f, (x), (x+3), (r)[(x)+0], (r)[(x)+1],		\
		(r)[(x)+2], (r)[(x)+3])

static void print_gr(char *level, struct pt_regs *regs)
{
	int i;
	char buf[64];

	printk("%s\n", level);
	printk("%s     YZrvWESTHLNXBCVMcbcbcbcbOGFRQPDI\n", level);
	printbinary(buf, regs->gr[0], 32);
	printk("%sPSW: %s %s\n", level, buf, print_tainted());

	for (i = 0; i < 32; i += 4)
		PRINTREGS(level, regs->gr, "r", RFMT, i);
}

static void print_fr(char *level, struct pt_regs *regs)
{
	int i;
	char buf[64];
	struct { u32 sw[2]; } s;

	asm volatile ("fstd %%fr0,0(%1)	\n\t"
		      "fldd 0(%1),%%fr0	\n\t"
		      : "=m" (s) : "r" (&s) : "r0");

	printk("%s\n", level);
	printk("%s      VZOUICununcqcqcqcqcqcrmunTDVZOUI\n", level);
	printbinary(buf, s.sw[0], 32);
	printk("%sFPSR: %s\n", level, buf);
	printk("%sFPER1: %08x\n", level, s.sw[1]);

	
	for (i = 0; i < 32; i += 4)
		PRINTREGS(level, regs->fr, "fr", FFMT, i);
}

void show_regs(struct pt_regs *regs)
{
	int i, user;
	char *level;
	unsigned long cr30, cr31;

	user = user_mode(regs);
	level = user ? KERN_DEBUG : KERN_CRIT;

	print_gr(level, regs);

	for (i = 0; i < 8; i += 4)
		PRINTREGS(level, regs->sr, "sr", RFMT, i);

	if (user)
		print_fr(level, regs);

	cr30 = mfctl(30);
	cr31 = mfctl(31);
	printk("%s\n", level);
	printk("%sIASQ: " RFMT " " RFMT " IAOQ: " RFMT " " RFMT "\n",
	       level, regs->iasq[0], regs->iasq[1], regs->iaoq[0], regs->iaoq[1]);
	printk("%s IIR: %08lx    ISR: " RFMT "  IOR: " RFMT "\n",
	       level, regs->iir, regs->isr, regs->ior);
	printk("%s CPU: %8d   CR30: " RFMT " CR31: " RFMT "\n",
	       level, current_thread_info()->cpu, cr30, cr31);
	printk("%s ORIG_R28: " RFMT "\n", level, regs->orig_r28);

	if (user) {
		printk("%s IAOQ[0]: " RFMT "\n", level, regs->iaoq[0]);
		printk("%s IAOQ[1]: " RFMT "\n", level, regs->iaoq[1]);
		printk("%s RP(r2): " RFMT "\n", level, regs->gr[2]);
	} else {
		printk("%s IAOQ[0]: %pS\n", level, (void *) regs->iaoq[0]);
		printk("%s IAOQ[1]: %pS\n", level, (void *) regs->iaoq[1]);
		printk("%s RP(r2): %pS\n", level, (void *) regs->gr[2]);

		parisc_show_stack(current, NULL, regs);
	}
}


void dump_stack(void)
{
	show_stack(NULL, NULL);
}

EXPORT_SYMBOL(dump_stack);

static void do_show_stack(struct unwind_frame_info *info)
{
	int i = 1;

	printk(KERN_CRIT "Backtrace:\n");
	while (i <= 16) {
		if (unwind_once(info) < 0 || info->ip == 0)
			break;

		if (__kernel_text_address(info->ip)) {
			printk(KERN_CRIT " [<" RFMT ">] %pS\n",
				info->ip, (void *) info->ip);
			i++;
		}
	}
	printk(KERN_CRIT "\n");
}

static void parisc_show_stack(struct task_struct *task, unsigned long *sp,
	struct pt_regs *regs)
{
	struct unwind_frame_info info;
	struct task_struct *t;

	t = task ? task : current;
	if (regs) {
		unwind_frame_init(&info, t, regs);
		goto show_stack;
	}

	if (t == current) {
		unsigned long sp;

HERE:
		asm volatile ("copy %%r30, %0" : "=r"(sp));
		{
			struct pt_regs r;

			memset(&r, 0, sizeof(struct pt_regs));
			r.iaoq[0] = (unsigned long)&&HERE;
			r.gr[2] = (unsigned long)__builtin_return_address(0);
			r.gr[30] = sp;

			unwind_frame_init(&info, current, &r);
		}
	} else {
		unwind_frame_init_from_blocked_task(&info, t);
	}

show_stack:
	do_show_stack(&info);
}

void show_stack(struct task_struct *t, unsigned long *sp)
{
	return parisc_show_stack(t, sp, NULL);
}

int is_valid_bugaddr(unsigned long iaoq)
{
	return 1;
}

void die_if_kernel(char *str, struct pt_regs *regs, long err)
{
	if (user_mode(regs)) {
		if (err == 0)
			return; 

		printk(KERN_CRIT "%s (pid %d): %s (code %ld) at " RFMT "\n",
			current->comm, task_pid_nr(current), str, err, regs->iaoq[0]);
#ifdef PRINT_USER_FAULTS
		
		show_regs(regs);
#endif
		return;
	}

	oops_in_progress = 1;

	oops_enter();

	
	if (err) printk(KERN_CRIT
			"      _______________________________ \n"
			"     < Your System ate a SPARC! Gah! >\n"
			"      ------------------------------- \n"
			"             \\   ^__^\n"
			"                 (__)\\       )\\/\\\n"
			"                  U  ||----w |\n"
			"                     ||     ||\n");
	
	
	pdc_emergency_unlock();

	if (!console_drivers)
		pdc_console_restart();
	
	if (err)
		printk(KERN_CRIT "%s (pid %d): %s (code %ld)\n",
			current->comm, task_pid_nr(current), str, err);

	
	if (current->thread.flags & PARISC_KERNEL_DEATH) {
		printk(KERN_CRIT "%s() recursion detected.\n", __func__);
		local_irq_enable();
		while (1);
	}
	current->thread.flags |= PARISC_KERNEL_DEATH;

	show_regs(regs);
	dump_stack();
	add_taint(TAINT_DIE);

	if (in_interrupt())
		panic("Fatal exception in interrupt");

	if (panic_on_oops) {
		printk(KERN_EMERG "Fatal exception: panic in 5 seconds\n");
		ssleep(5);
		panic("Fatal exception");
	}

	oops_exit();
	do_exit(SIGSEGV);
}

int syscall_ipi(int (*syscall) (struct pt_regs *), struct pt_regs *regs)
{
	return syscall(regs);
}

#define GDB_BREAK_INSN 0x10004
static void handle_gdb_break(struct pt_regs *regs, int wot)
{
	struct siginfo si;

	si.si_signo = SIGTRAP;
	si.si_errno = 0;
	si.si_code = wot;
	si.si_addr = (void __user *) (regs->iaoq[0] & ~3);
	force_sig_info(SIGTRAP, &si, current);
}

static void handle_break(struct pt_regs *regs)
{
	unsigned iir = regs->iir;

	if (unlikely(iir == PARISC_BUG_BREAK_INSN && !user_mode(regs))) {
		
		enum bug_trap_type tt;
		tt = report_bug(regs->iaoq[0] & ~3, regs);
		if (tt == BUG_TRAP_TYPE_WARN) {
			regs->iaoq[0] += 4;
			regs->iaoq[1] += 4;
			return; 
		}
		die_if_kernel("Unknown kernel breakpoint", regs,
			(tt == BUG_TRAP_TYPE_NONE) ? 9 : 0);
	}

#ifdef PRINT_USER_FAULTS
	if (unlikely(iir != GDB_BREAK_INSN)) {
		printk(KERN_DEBUG "break %d,%d: pid=%d command='%s'\n",
			iir & 31, (iir>>13) & ((1<<13)-1),
			task_pid_nr(current), current->comm);
		show_regs(regs);
	}
#endif

	
	handle_gdb_break(regs, TRAP_BRKPT);
}

static void default_trap(int code, struct pt_regs *regs)
{
	printk(KERN_ERR "Trap %d on CPU %d\n", code, smp_processor_id());
	show_regs(regs);
}

void (*cpu_lpmc) (int code, struct pt_regs *regs) __read_mostly = default_trap;


void transfer_pim_to_trap_frame(struct pt_regs *regs)
{
    register int i;
    extern unsigned int hpmc_pim_data[];
    struct pdc_hpmc_pim_11 *pim_narrow;
    struct pdc_hpmc_pim_20 *pim_wide;

    if (boot_cpu_data.cpu_type >= pcxu) {

	pim_wide = (struct pdc_hpmc_pim_20 *)hpmc_pim_data;


	regs->gr[0] = pim_wide->cr[22];

	for (i = 1; i < 32; i++)
	    regs->gr[i] = pim_wide->gr[i];

	for (i = 0; i < 32; i++)
	    regs->fr[i] = pim_wide->fr[i];

	for (i = 0; i < 8; i++)
	    regs->sr[i] = pim_wide->sr[i];

	regs->iasq[0] = pim_wide->cr[17];
	regs->iasq[1] = pim_wide->iasq_back;
	regs->iaoq[0] = pim_wide->cr[18];
	regs->iaoq[1] = pim_wide->iaoq_back;

	regs->sar  = pim_wide->cr[11];
	regs->iir  = pim_wide->cr[19];
	regs->isr  = pim_wide->cr[20];
	regs->ior  = pim_wide->cr[21];
    }
    else {
	pim_narrow = (struct pdc_hpmc_pim_11 *)hpmc_pim_data;

	regs->gr[0] = pim_narrow->cr[22];

	for (i = 1; i < 32; i++)
	    regs->gr[i] = pim_narrow->gr[i];

	for (i = 0; i < 32; i++)
	    regs->fr[i] = pim_narrow->fr[i];

	for (i = 0; i < 8; i++)
	    regs->sr[i] = pim_narrow->sr[i];

	regs->iasq[0] = pim_narrow->cr[17];
	regs->iasq[1] = pim_narrow->iasq_back;
	regs->iaoq[0] = pim_narrow->cr[18];
	regs->iaoq[1] = pim_narrow->iaoq_back;

	regs->sar  = pim_narrow->cr[11];
	regs->iir  = pim_narrow->cr[19];
	regs->isr  = pim_narrow->cr[20];
	regs->ior  = pim_narrow->cr[21];
    }


    regs->ksp = 0;
    regs->kpc = 0;
    regs->orig_r28 = 0;
}


void parisc_terminate(char *msg, struct pt_regs *regs, int code, unsigned long offset)
{
	static DEFINE_SPINLOCK(terminate_lock);

	oops_in_progress = 1;

	set_eiem(0);
	local_irq_disable();
	spin_lock(&terminate_lock);

	
	pdc_emergency_unlock();

	
	if (!console_drivers)
		pdc_console_restart();

	
	switch(code){

	case 1:
		transfer_pim_to_trap_frame(regs);
		break;

	default:
		
		break;

	}
	    
	{
		
		struct unwind_frame_info info;
		unwind_frame_init(&info, current, regs);
		do_show_stack(&info);
	}

	printk("\n");
	printk(KERN_CRIT "%s: Code=%d regs=%p (Addr=" RFMT ")\n",
			msg, code, regs, offset);
	show_regs(regs);

	spin_unlock(&terminate_lock);

	pdc_soft_power_button(0);
	
	panic(msg);
}

void notrace handle_interruption(int code, struct pt_regs *regs)
{
	unsigned long fault_address = 0;
	unsigned long fault_space = 0;
	struct siginfo si;

	if (code == 1)
	    pdc_console_restart();  
	else
	    local_irq_enable();

	if (((unsigned long)regs->iaoq[0] & 3) &&
	    ((unsigned long)regs->iasq[0] != (unsigned long)regs->sr[7])) { 
	  	
	  	regs->iaoq[0] = 0 | 3;
		regs->iaoq[1] = regs->iaoq[0] + 4;
	 	regs->iasq[0] = regs->iasq[1] = regs->sr[7];
		regs->gr[0] &= ~PSW_B;
		return;
	}
	
#if 0
	printk(KERN_CRIT "Interruption # %d\n", code);
#endif

	switch(code) {

	case  1:
		
		
		
		pdc_chassis_send_status(PDC_CHASSIS_DIRECT_HPMC);
		    
	    	parisc_terminate("High Priority Machine Check (HPMC)",
				regs, code, 0);
		
		
	case  2:
		
		printk(KERN_CRIT "Power failure interrupt !\n");
		return;

	case  3:
		
		regs->gr[0] &= ~PSW_R;
		if (user_space(regs))
			handle_gdb_break(regs, TRAP_TRACE);
		
		return;

	case  5:
		
		pdc_chassis_send_status(PDC_CHASSIS_DIRECT_LPMC);
		
		flush_cache_all();
		flush_tlb_all();
		cpu_lpmc(5, regs);
		return;

	case  6:
		
		fault_address = regs->iaoq[0];
		fault_space   = regs->iasq[0];
		break;

	case  8:
		
		die_if_kernel("Illegal instruction", regs, code);
		si.si_code = ILL_ILLOPC;
		goto give_sigill;

	case  9:
		
		handle_break(regs);
		return;
	
	case 10:
		
		die_if_kernel("Privileged operation", regs, code);
		si.si_code = ILL_PRVOPC;
		goto give_sigill;
	
	case 11:
		
		if ((regs->iir & 0xffdfffe0) == 0x034008a0) {


			if (regs->iir & 0x00200000)
				regs->gr[regs->iir & 0x1f] = mfctl(27);
			else
				regs->gr[regs->iir & 0x1f] = mfctl(26);

			regs->iaoq[0] = regs->iaoq[1];
			regs->iaoq[1] += 4;
			regs->iasq[0] = regs->iasq[1];
			return;
		}

		die_if_kernel("Privileged register usage", regs, code);
		si.si_code = ILL_PRVREG;
	give_sigill:
		si.si_signo = SIGILL;
		si.si_errno = 0;
		si.si_addr = (void __user *) regs->iaoq[0];
		force_sig_info(SIGILL, &si, current);
		return;

	case 12:
		
		si.si_signo = SIGFPE;
		si.si_code = FPE_INTOVF;
		si.si_addr = (void __user *) regs->iaoq[0];
		force_sig_info(SIGFPE, &si, current);
		return;
		
	case 13:
		if(user_mode(regs)){
			si.si_signo = SIGFPE;
			si.si_code = 0;
			si.si_addr = (void __user *) regs->iaoq[0];
			force_sig_info(SIGFPE, &si, current);
			return;
		} 
		
		break;
		
	case 14:
		
		die_if_kernel("Floating point exception", regs, 0); 
		handle_fpe(regs);
		return;
		
	case 15:
		
		
	case 16:
		
		
	case 17:
		
			  
		fault_address = regs->ior;
		fault_space = regs->isr;
		break;

	case 18:
		
		
		if (check_unaligned(regs)) {
			handle_unaligned(regs);
			return;
		}
		
	case 26: 
		
		fault_address = regs->ior;
		fault_space   = regs->isr;
		break;

	case 19:
		
		regs->gr[0] |= PSW_X; 
		
	case 21:
		
		handle_gdb_break(regs, TRAP_HWBKPT);
		return;

	case 25:
		
		regs->gr[0] &= ~PSW_T;
		if (user_space(regs))
			handle_gdb_break(regs, TRAP_BRANCH);
		return;

	case  7:  
		
		


		if (user_mode(regs)) {
			struct vm_area_struct *vma;

			down_read(&current->mm->mmap_sem);
			vma = find_vma(current->mm,regs->iaoq[0]);
			if (vma && (regs->iaoq[0] >= vma->vm_start)
				&& (vma->vm_flags & VM_EXEC)) {

				fault_address = regs->iaoq[0];
				fault_space = regs->iasq[0];

				up_read(&current->mm->mmap_sem);
				break; 
			}
			up_read(&current->mm->mmap_sem);
		}
		
	case 27: 
		
		if (code == 27 && !user_mode(regs) &&
			fixup_exception(regs))
			return;

		die_if_kernel("Protection id trap", regs, code);
		si.si_code = SEGV_MAPERR;
		si.si_signo = SIGSEGV;
		si.si_errno = 0;
		if (code == 7)
		    si.si_addr = (void __user *) regs->iaoq[0];
		else
		    si.si_addr = (void __user *) regs->ior;
		force_sig_info(SIGSEGV, &si, current);
		return;

	case 28: 
		
		handle_unaligned(regs);
		return;

	default:
		if (user_mode(regs)) {
#ifdef PRINT_USER_FAULTS
			printk(KERN_DEBUG "\nhandle_interruption() pid=%d command='%s'\n",
			    task_pid_nr(current), current->comm);
			show_regs(regs);
#endif
			
			si.si_signo = SIGBUS;
			si.si_code = BUS_OBJERR;
			si.si_errno = 0;
			si.si_addr = (void __user *) regs->ior;
			force_sig_info(SIGBUS, &si, current);
			return;
		}
		pdc_chassis_send_status(PDC_CHASSIS_DIRECT_PANIC);
		
		parisc_terminate("Unexpected interruption", regs, code, 0);
		
	}

	if (user_mode(regs)) {
	    if ((fault_space >> SPACEID_SHIFT) != (regs->sr[7] >> SPACEID_SHIFT)) {
#ifdef PRINT_USER_FAULTS
		if (fault_space == 0)
			printk(KERN_DEBUG "User Fault on Kernel Space ");
		else
			printk(KERN_DEBUG "User Fault (long pointer) (fault %d) ",
			       code);
		printk(KERN_CONT "pid=%d command='%s'\n",
		       task_pid_nr(current), current->comm);
		show_regs(regs);
#endif
		si.si_signo = SIGSEGV;
		si.si_errno = 0;
		si.si_code = SEGV_MAPERR;
		si.si_addr = (void __user *) regs->ior;
		force_sig_info(SIGSEGV, &si, current);
		return;
	    }
	}
	else {


	    if (fault_space == 0) 
	    {
		pdc_chassis_send_status(PDC_CHASSIS_DIRECT_PANIC);
		parisc_terminate("Kernel Fault", regs, code, fault_address);
	
	    }
	}

	do_page_fault(regs, code, fault_address);
}


int __init check_ivt(void *iva)
{
	extern u32 os_hpmc_size;
	extern const u32 os_hpmc[];

	int i;
	u32 check = 0;
	u32 *ivap;
	u32 *hpmcp;
	u32 length;

	if (strcmp((char *)iva, "cows can fly"))
		return -1;

	ivap = (u32 *)iva;

	for (i = 0; i < 8; i++)
	    *ivap++ = 0;

	
	length = os_hpmc_size;
	ivap[7] = length;

	hpmcp = (u32 *)os_hpmc;

	for (i=0; i<length/4; i++)
	    check += *hpmcp++;

	for (i=0; i<8; i++)
	    check += ivap[i];

	ivap[5] = -check;

	return 0;
}
	
#ifndef CONFIG_64BIT
extern const void fault_vector_11;
#endif
extern const void fault_vector_20;

void __init trap_init(void)
{
	void *iva;

	if (boot_cpu_data.cpu_type >= pcxu)
		iva = (void *) &fault_vector_20;
	else
#ifdef CONFIG_64BIT
		panic("Can't boot 64-bit OS on PA1.1 processor!");
#else
		iva = (void *) &fault_vector_11;
#endif

	if (check_ivt(iva))
		panic("IVT invalid");
}
