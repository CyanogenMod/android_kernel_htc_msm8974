/*
 * Copyright (C) 2000-2003, Axis Communications AB.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/signal.h>
#include <linux/security.h>

#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>

#define DCCR_MASK 0x0000001f     

inline long get_reg(struct task_struct *task, unsigned int regno)
{

	if (regno == PT_USP)
		return task->thread.usp;
	else if (regno < PT_MAX)
		return ((unsigned long *)task_pt_regs(task))[regno];
	else
		return 0;
}

inline int put_reg(struct task_struct *task, unsigned int regno,
			  unsigned long data)
{
	if (regno == PT_USP)
		task->thread.usp = data;
	else if (regno < PT_MAX)
		((unsigned long *)task_pt_regs(task))[regno] = data;
	else
		return -1;
	return 0;
}

void 
ptrace_disable(struct task_struct *child)
{
       
       clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
}

/* 
 * Note that this implementation of ptrace behaves differently from vanilla
 * ptrace.  Contrary to what the man page says, in the PTRACE_PEEKTEXT,
 * PTRACE_PEEKDATA, and PTRACE_PEEKUSER requests the data variable is not
 * ignored.  Instead, the data variable is expected to point at a location
 * (in user space) where the result of the ptrace call is written (instead of
 * being returned).
 */
long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	int ret;
	unsigned int regno = addr >> 2;
	unsigned long __user *datap = (unsigned long __user *)data;

	switch (request) {
		 
		case PTRACE_PEEKTEXT:
		case PTRACE_PEEKDATA:
			ret = generic_ptrace_peekdata(child, addr, data);
			break;

		
		case PTRACE_PEEKUSR: {
			unsigned long tmp;

			ret = -EIO;
			if ((addr & 3) || regno > PT_MAX)
				break;

			tmp = get_reg(child, regno);
			ret = put_user(tmp, datap);
			break;
		}
		
		
		case PTRACE_POKETEXT:
		case PTRACE_POKEDATA:
			ret = generic_ptrace_pokedata(child, addr, data);
			break;
 
 		
		case PTRACE_POKEUSR:
			ret = -EIO;
			if ((addr & 3) || regno > PT_MAX)
				break;

			if (regno == PT_DCCR) {
				data &= DCCR_MASK;
				data |= get_reg(child, PT_DCCR) & ~DCCR_MASK;
			}
			if (put_reg(child, regno, data))
				break;
			ret = 0;
			break;

		
		case PTRACE_GETREGS: {
		  	int i;
			unsigned long tmp;
			
			ret = 0;
			for (i = 0; i <= PT_MAX; i++) {
				tmp = get_reg(child, i);
				
				if (put_user(tmp, datap)) {
					ret = -EFAULT;
					break;
				}
				
				datap++;
			}

			break;
		}

		
		case PTRACE_SETREGS: {
			int i;
			unsigned long tmp;
			
			ret = 0;
			for (i = 0; i <= PT_MAX; i++) {
				if (get_user(tmp, datap)) {
					ret = -EFAULT;
					break;
				}
				
				if (i == PT_DCCR) {
					tmp &= DCCR_MASK;
					tmp |= get_reg(child, PT_DCCR) & ~DCCR_MASK;
				}
				
				put_reg(child, i, tmp);
				datap++;
			}
			
			break;
		}

		default:
			ret = ptrace_request(child, request, addr, data);
			break;
	}

	return ret;
}

void do_syscall_trace(void)
{
	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		return;
	
	if (!(current->ptrace & PT_PTRACED))
		return;
	
	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
				 ? 0x80 : 0));
	
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}
