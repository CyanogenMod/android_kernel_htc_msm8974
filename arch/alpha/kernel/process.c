/*
 *  linux/arch/alpha/kernel/process.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 */


#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/time.h>
#include <linux/major.h>
#include <linux/stat.h>
#include <linux/vt.h>
#include <linux/mman.h>
#include <linux/elfcore.h>
#include <linux/reboot.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/slab.h>

#include <asm/reg.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/hwrpb.h>
#include <asm/fpu.h>

#include "proto.h"
#include "pci_impl.h"

void (*pm_power_off)(void) = machine_power_off;
EXPORT_SYMBOL(pm_power_off);

void
cpu_idle(void)
{
	set_thread_flag(TIF_POLLING_NRFLAG);

	while (1) {

		while (!need_resched())
			cpu_relax();
		schedule();
	}
}


struct halt_info {
	int mode;
	char *restart_cmd;
};

static void
common_shutdown_1(void *generic_ptr)
{
	struct halt_info *how = (struct halt_info *)generic_ptr;
	struct percpu_struct *cpup;
	unsigned long *pflags, flags;
	int cpuid = smp_processor_id();

	
	local_irq_disable();

	cpup = (struct percpu_struct *)
			((unsigned long)hwrpb + hwrpb->processor_offset
			 + hwrpb->processor_size * cpuid);
	pflags = &cpup->flags;
	flags = *pflags;

	
	flags &= ~0x00ff0001UL;

#ifdef CONFIG_SMP
	
	if (cpuid != boot_cpuid) {
		flags |= 0x00040000UL; 
		*pflags = flags;
		set_cpu_present(cpuid, false);
		set_cpu_possible(cpuid, false);
		halt();
	}
#endif

	if (how->mode == LINUX_REBOOT_CMD_RESTART) {
		if (!how->restart_cmd) {
			flags |= 0x00020000UL; 
		} else {
			flags |= 0x00030000UL; 
		}
	} else {
		flags |= 0x00040000UL; 
	}
	*pflags = flags;

#ifdef CONFIG_SMP
	
	set_cpu_present(boot_cpuid, false);
	set_cpu_possible(boot_cpuid, false);
	while (cpumask_weight(cpu_present_mask))
		barrier();
#endif

	
	if (alpha_using_srm) {
#ifdef CONFIG_DUMMY_CONSOLE
		if (in_interrupt())
			irq_exit();
		
		take_over_console(&dummy_con, 0, MAX_NR_CONSOLES-1, 1);
#endif
		pci_restore_srm_config();
		set_hae(srm_hae);
	}

	if (alpha_mv.kill_arch)
		alpha_mv.kill_arch(how->mode);

	if (! alpha_using_srm && how->mode != LINUX_REBOOT_CMD_RESTART) {
		return;
	}

	if (alpha_using_srm)
		srm_paging_stop();

	halt();
}

static void
common_shutdown(int mode, char *restart_cmd)
{
	struct halt_info args;
	args.mode = mode;
	args.restart_cmd = restart_cmd;
	on_each_cpu(common_shutdown_1, &args, 0);
}

void
machine_restart(char *restart_cmd)
{
	common_shutdown(LINUX_REBOOT_CMD_RESTART, restart_cmd);
}


void
machine_halt(void)
{
	common_shutdown(LINUX_REBOOT_CMD_HALT, NULL);
}


void
machine_power_off(void)
{
	common_shutdown(LINUX_REBOOT_CMD_POWER_OFF, NULL);
}



void
show_regs(struct pt_regs *regs)
{
	dik_show_regs(regs, NULL);
}

void
start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp)
{
	regs->pc = pc;
	regs->ps = 8;
	wrusp(sp);
}
EXPORT_SYMBOL(start_thread);

void
exit_thread(void)
{
}

void
flush_thread(void)
{
	current_thread_info()->ieee_state = 0;
	wrfpcr(FPCR_DYN_NORMAL | ieee_swcr_to_fpcr(0));

	
	current_thread_info()->pcb.unique = 0;
}

void
release_thread(struct task_struct *dead_task)
{
}

int
alpha_clone(unsigned long clone_flags, unsigned long usp,
	    int __user *parent_tid, int __user *child_tid,
	    unsigned long tls_value, struct pt_regs *regs)
{
	if (!usp)
		usp = rdusp();

	return do_fork(clone_flags, usp, regs, 0, parent_tid, child_tid);
}

int
alpha_vfork(struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, rdusp(),
		       regs, 0, NULL, NULL);
}


int
copy_thread(unsigned long clone_flags, unsigned long usp,
	    unsigned long unused,
	    struct task_struct * p, struct pt_regs * regs)
{
	extern void ret_from_fork(void);

	struct thread_info *childti = task_thread_info(p);
	struct pt_regs * childregs;
	struct switch_stack * childstack, *stack;
	unsigned long stack_offset, settls;

	stack_offset = PAGE_SIZE - sizeof(struct pt_regs);
	if (!(regs->ps & 8))
		stack_offset = (PAGE_SIZE-1) & (unsigned long) regs;
	childregs = (struct pt_regs *)
	  (stack_offset + PAGE_SIZE + task_stack_page(p));
		
	*childregs = *regs;
	settls = regs->r20;
	childregs->r0 = 0;
	childregs->r19 = 0;
	childregs->r20 = 1;	
	regs->r20 = 0;
	stack = ((struct switch_stack *) regs) - 1;
	childstack = ((struct switch_stack *) childregs) - 1;
	*childstack = *stack;
	childstack->r26 = (unsigned long) ret_from_fork;
	childti->pcb.usp = usp;
	childti->pcb.ksp = (unsigned long) childstack;
	childti->pcb.flags = 1;	

	if (clone_flags & CLONE_SETTLS)
		childti->pcb.unique = settls;

	return 0;
}

void
dump_elf_thread(elf_greg_t *dest, struct pt_regs *pt, struct thread_info *ti)
{
	
	struct switch_stack * sw = ((struct switch_stack *) pt) - 1;

	dest[ 0] = pt->r0;
	dest[ 1] = pt->r1;
	dest[ 2] = pt->r2;
	dest[ 3] = pt->r3;
	dest[ 4] = pt->r4;
	dest[ 5] = pt->r5;
	dest[ 6] = pt->r6;
	dest[ 7] = pt->r7;
	dest[ 8] = pt->r8;
	dest[ 9] = sw->r9;
	dest[10] = sw->r10;
	dest[11] = sw->r11;
	dest[12] = sw->r12;
	dest[13] = sw->r13;
	dest[14] = sw->r14;
	dest[15] = sw->r15;
	dest[16] = pt->r16;
	dest[17] = pt->r17;
	dest[18] = pt->r18;
	dest[19] = pt->r19;
	dest[20] = pt->r20;
	dest[21] = pt->r21;
	dest[22] = pt->r22;
	dest[23] = pt->r23;
	dest[24] = pt->r24;
	dest[25] = pt->r25;
	dest[26] = pt->r26;
	dest[27] = pt->r27;
	dest[28] = pt->r28;
	dest[29] = pt->gp;
	dest[30] = ti == current_thread_info() ? rdusp() : ti->pcb.usp;
	dest[31] = pt->pc;

	dest[32] = ti->pcb.unique;
}
EXPORT_SYMBOL(dump_elf_thread);

int
dump_elf_task(elf_greg_t *dest, struct task_struct *task)
{
	dump_elf_thread(dest, task_pt_regs(task), task_thread_info(task));
	return 1;
}
EXPORT_SYMBOL(dump_elf_task);

int
dump_elf_task_fp(elf_fpreg_t *dest, struct task_struct *task)
{
	struct switch_stack *sw = (struct switch_stack *)task_pt_regs(task) - 1;
	memcpy(dest, sw->fp, 32 * 8);
	return 1;
}
EXPORT_SYMBOL(dump_elf_task_fp);

asmlinkage int
do_sys_execve(const char __user *ufilename,
	      const char __user *const __user *argv,
	      const char __user *const __user *envp, struct pt_regs *regs)
{
	int error;
	char *filename;

	filename = getname(ufilename);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, argv, envp, regs);
	putname(filename);
out:
	return error;
}


unsigned long
thread_saved_pc(struct task_struct *t)
{
	unsigned long base = (unsigned long)task_stack_page(t);
	unsigned long fp, sp = task_thread_info(t)->pcb.ksp;

	if (sp > base && sp+6*8 < base + 16*1024) {
		fp = ((unsigned long*)sp)[6];
		if (fp > sp && fp < base + 16*1024)
			return *(unsigned long *)fp;
	}

	return 0;
}

unsigned long
get_wchan(struct task_struct *p)
{
	unsigned long schedule_frame;
	unsigned long pc;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	pc = thread_saved_pc(p);
	if (in_sched_functions(pc)) {
		schedule_frame = ((unsigned long *)task_thread_info(p)->pcb.ksp)[6];
		return ((unsigned long *)schedule_frame)[12];
	}
	return pc;
}
