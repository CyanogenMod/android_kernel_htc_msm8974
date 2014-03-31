/*
 *  Ptrace user space interface.
 *
 *    Copyright IBM Corp. 1999,2010
 *    Author(s): Denis Joseph Barrow
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/security.h>
#include <linux/audit.h>
#include <linux/signal.h>
#include <linux/elf.h>
#include <linux/regset.h>
#include <linux/tracehook.h>
#include <linux/seccomp.h>
#include <linux/compat.h>
#include <trace/syscall.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/switch_to.h>
#include "entry.h"

#ifdef CONFIG_COMPAT
#include "compat_ptrace.h"
#endif

#define CREATE_TRACE_POINTS
#include <trace/events/syscalls.h>

enum s390_regset {
	REGSET_GENERAL,
	REGSET_FP,
	REGSET_LAST_BREAK,
	REGSET_SYSTEM_CALL,
	REGSET_GENERAL_EXTENDED,
};

void update_per_regs(struct task_struct *task)
{
	struct pt_regs *regs = task_pt_regs(task);
	struct thread_struct *thread = &task->thread;
	struct per_regs old, new;

	
	new.control = thread->per_user.control;
	new.start = thread->per_user.start;
	new.end = thread->per_user.end;

	
	if (test_tsk_thread_flag(task, TIF_SINGLE_STEP)) {
		new.control |= PER_EVENT_IFETCH;
		new.start = 0;
		new.end = PSW_ADDR_INSN;
	}

	
	if (!(new.control & PER_EVENT_MASK)) {
		regs->psw.mask &= ~PSW_MASK_PER;
		return;
	}
	regs->psw.mask |= PSW_MASK_PER;
	__ctl_store(old, 9, 11);
	if (memcmp(&new, &old, sizeof(struct per_regs)) != 0)
		__ctl_load(new, 9, 11);
}

void user_enable_single_step(struct task_struct *task)
{
	set_tsk_thread_flag(task, TIF_SINGLE_STEP);
	if (task == current)
		update_per_regs(task);
}

void user_disable_single_step(struct task_struct *task)
{
	clear_tsk_thread_flag(task, TIF_SINGLE_STEP);
	if (task == current)
		update_per_regs(task);
}

void ptrace_disable(struct task_struct *task)
{
	memset(&task->thread.per_user, 0, sizeof(task->thread.per_user));
	memset(&task->thread.per_event, 0, sizeof(task->thread.per_event));
	clear_tsk_thread_flag(task, TIF_SINGLE_STEP);
	clear_tsk_thread_flag(task, TIF_PER_TRAP);
}

#ifndef CONFIG_64BIT
# define __ADDR_MASK 3
#else
# define __ADDR_MASK 7
#endif

static inline unsigned long __peek_user_per(struct task_struct *child,
					    addr_t addr)
{
	struct per_struct_kernel *dummy = NULL;

	if (addr == (addr_t) &dummy->cr9)
		
		return test_thread_flag(TIF_SINGLE_STEP) ?
			PER_EVENT_IFETCH : child->thread.per_user.control;
	else if (addr == (addr_t) &dummy->cr10)
		
		return test_thread_flag(TIF_SINGLE_STEP) ?
			0 : child->thread.per_user.start;
	else if (addr == (addr_t) &dummy->cr11)
		
		return test_thread_flag(TIF_SINGLE_STEP) ?
			PSW_ADDR_INSN : child->thread.per_user.end;
	else if (addr == (addr_t) &dummy->bits)
		
		return test_thread_flag(TIF_SINGLE_STEP) ?
			(1UL << (BITS_PER_LONG - 1)) : 0;
	else if (addr == (addr_t) &dummy->starting_addr)
		
		return child->thread.per_user.start;
	else if (addr == (addr_t) &dummy->ending_addr)
		
		return child->thread.per_user.end;
	else if (addr == (addr_t) &dummy->perc_atmid)
		
		return (unsigned long)
			child->thread.per_event.cause << (BITS_PER_LONG - 16);
	else if (addr == (addr_t) &dummy->address)
		
		return child->thread.per_event.address;
	else if (addr == (addr_t) &dummy->access_id)
		
		return (unsigned long)
			child->thread.per_event.paid << (BITS_PER_LONG - 8);
	return 0;
}

static unsigned long __peek_user(struct task_struct *child, addr_t addr)
{
	struct user *dummy = NULL;
	addr_t offset, tmp;

	if (addr < (addr_t) &dummy->regs.acrs) {
		tmp = *(addr_t *)((addr_t) &task_pt_regs(child)->psw + addr);
		if (addr == (addr_t) &dummy->regs.psw.mask)
			
			tmp = psw_user_bits | (tmp & PSW_MASK_USER);

	} else if (addr < (addr_t) &dummy->regs.orig_gpr2) {
		offset = addr - (addr_t) &dummy->regs.acrs;
#ifdef CONFIG_64BIT
		if (addr == (addr_t) &dummy->regs.acrs[15])
			tmp = ((unsigned long) child->thread.acrs[15]) << 32;
		else
#endif
		tmp = *(addr_t *)((addr_t) &child->thread.acrs + offset);

	} else if (addr == (addr_t) &dummy->regs.orig_gpr2) {
		tmp = (addr_t) task_pt_regs(child)->orig_gpr2;

	} else if (addr < (addr_t) &dummy->regs.fp_regs) {
		tmp = 0;

	} else if (addr < (addr_t) (&dummy->regs.fp_regs + 1)) {
		offset = addr - (addr_t) &dummy->regs.fp_regs;
		tmp = *(addr_t *)((addr_t) &child->thread.fp_regs + offset);
		if (addr == (addr_t) &dummy->regs.fp_regs.fpc)
			tmp &= (unsigned long) FPC_VALID_MASK
				<< (BITS_PER_LONG - 32);

	} else if (addr < (addr_t) (&dummy->regs.per_info + 1)) {
		addr -= (addr_t) &dummy->regs.per_info;
		tmp = __peek_user_per(child, addr);

	} else
		tmp = 0;

	return tmp;
}

static int
peek_user(struct task_struct *child, addr_t addr, addr_t data)
{
	addr_t tmp, mask;

	mask = __ADDR_MASK;
#ifdef CONFIG_64BIT
	if (addr >= (addr_t) &((struct user *) NULL)->regs.acrs &&
	    addr < (addr_t) &((struct user *) NULL)->regs.orig_gpr2)
		mask = 3;
#endif
	if ((addr & mask) || addr > sizeof(struct user) - __ADDR_MASK)
		return -EIO;

	tmp = __peek_user(child, addr);
	return put_user(tmp, (addr_t __user *) data);
}

static inline void __poke_user_per(struct task_struct *child,
				   addr_t addr, addr_t data)
{
	struct per_struct_kernel *dummy = NULL;

	if (addr == (addr_t) &dummy->cr9)
		
		child->thread.per_user.control =
			data & (PER_EVENT_MASK | PER_CONTROL_MASK);
	else if (addr == (addr_t) &dummy->starting_addr)
		
		child->thread.per_user.start = data;
	else if (addr == (addr_t) &dummy->ending_addr)
		
		child->thread.per_user.end = data;
}

static int __poke_user(struct task_struct *child, addr_t addr, addr_t data)
{
	struct user *dummy = NULL;
	addr_t offset;

	if (addr < (addr_t) &dummy->regs.acrs) {
		if (addr == (addr_t) &dummy->regs.psw.mask &&
		    ((data & ~PSW_MASK_USER) != psw_user_bits ||
		     ((data & PSW_MASK_EA) && !(data & PSW_MASK_BA))))
			
			return -EINVAL;
		*(addr_t *)((addr_t) &task_pt_regs(child)->psw + addr) = data;

	} else if (addr < (addr_t) (&dummy->regs.orig_gpr2)) {
		offset = addr - (addr_t) &dummy->regs.acrs;
#ifdef CONFIG_64BIT
		if (addr == (addr_t) &dummy->regs.acrs[15])
			child->thread.acrs[15] = (unsigned int) (data >> 32);
		else
#endif
		*(addr_t *)((addr_t) &child->thread.acrs + offset) = data;

	} else if (addr == (addr_t) &dummy->regs.orig_gpr2) {
		task_pt_regs(child)->orig_gpr2 = data;

	} else if (addr < (addr_t) &dummy->regs.fp_regs) {
		return 0;

	} else if (addr < (addr_t) (&dummy->regs.fp_regs + 1)) {
		if (addr == (addr_t) &dummy->regs.fp_regs.fpc &&
		    (data & ~((unsigned long) FPC_VALID_MASK
			      << (BITS_PER_LONG - 32))) != 0)
			return -EINVAL;
		offset = addr - (addr_t) &dummy->regs.fp_regs;
		*(addr_t *)((addr_t) &child->thread.fp_regs + offset) = data;

	} else if (addr < (addr_t) (&dummy->regs.per_info + 1)) {
		addr -= (addr_t) &dummy->regs.per_info;
		__poke_user_per(child, addr, data);

	}

	return 0;
}

static int poke_user(struct task_struct *child, addr_t addr, addr_t data)
{
	addr_t mask;

	mask = __ADDR_MASK;
#ifdef CONFIG_64BIT
	if (addr >= (addr_t) &((struct user *) NULL)->regs.acrs &&
	    addr < (addr_t) &((struct user *) NULL)->regs.orig_gpr2)
		mask = 3;
#endif
	if ((addr & mask) || addr > sizeof(struct user) - __ADDR_MASK)
		return -EIO;

	return __poke_user(child, addr, data);
}

long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	ptrace_area parea; 
	int copied, ret;

	switch (request) {
	case PTRACE_PEEKUSR:
		
		return peek_user(child, addr, data);

	case PTRACE_POKEUSR:
		
		return poke_user(child, addr, data);

	case PTRACE_PEEKUSR_AREA:
	case PTRACE_POKEUSR_AREA:
		if (copy_from_user(&parea, (void __force __user *) addr,
							sizeof(parea)))
			return -EFAULT;
		addr = parea.kernel_addr;
		data = parea.process_addr;
		copied = 0;
		while (copied < parea.len) {
			if (request == PTRACE_PEEKUSR_AREA)
				ret = peek_user(child, addr, data);
			else {
				addr_t utmp;
				if (get_user(utmp,
					     (addr_t __force __user *) data))
					return -EFAULT;
				ret = poke_user(child, addr, utmp);
			}
			if (ret)
				return ret;
			addr += sizeof(unsigned long);
			data += sizeof(unsigned long);
			copied += sizeof(unsigned long);
		}
		return 0;
	case PTRACE_GET_LAST_BREAK:
		put_user(task_thread_info(child)->last_break,
			 (unsigned long __user *) data);
		return 0;
	default:
		
		addr &= PSW_ADDR_INSN;
		return ptrace_request(child, request, addr, data);
	}
}

#ifdef CONFIG_COMPAT

static inline __u32 __peek_user_per_compat(struct task_struct *child,
					   addr_t addr)
{
	struct compat_per_struct_kernel *dummy32 = NULL;

	if (addr == (addr_t) &dummy32->cr9)
		
		return (__u32) test_thread_flag(TIF_SINGLE_STEP) ?
			PER_EVENT_IFETCH : child->thread.per_user.control;
	else if (addr == (addr_t) &dummy32->cr10)
		
		return (__u32) test_thread_flag(TIF_SINGLE_STEP) ?
			0 : child->thread.per_user.start;
	else if (addr == (addr_t) &dummy32->cr11)
		
		return test_thread_flag(TIF_SINGLE_STEP) ?
			PSW32_ADDR_INSN : child->thread.per_user.end;
	else if (addr == (addr_t) &dummy32->bits)
		
		return (__u32) test_thread_flag(TIF_SINGLE_STEP) ?
			0x80000000 : 0;
	else if (addr == (addr_t) &dummy32->starting_addr)
		
		return (__u32) child->thread.per_user.start;
	else if (addr == (addr_t) &dummy32->ending_addr)
		
		return (__u32) child->thread.per_user.end;
	else if (addr == (addr_t) &dummy32->perc_atmid)
		
		return (__u32) child->thread.per_event.cause << 16;
	else if (addr == (addr_t) &dummy32->address)
		
		return (__u32) child->thread.per_event.address;
	else if (addr == (addr_t) &dummy32->access_id)
		
		return (__u32) child->thread.per_event.paid << 24;
	return 0;
}

static u32 __peek_user_compat(struct task_struct *child, addr_t addr)
{
	struct compat_user *dummy32 = NULL;
	addr_t offset;
	__u32 tmp;

	if (addr < (addr_t) &dummy32->regs.acrs) {
		struct pt_regs *regs = task_pt_regs(child);
		if (addr == (addr_t) &dummy32->regs.psw.mask) {
			
			tmp = (__u32)(regs->psw.mask >> 32);
			tmp = psw32_user_bits | (tmp & PSW32_MASK_USER);
		} else if (addr == (addr_t) &dummy32->regs.psw.addr) {
			
			tmp = (__u32) regs->psw.addr |
				(__u32)(regs->psw.mask & PSW_MASK_BA);
		} else {
			
			tmp = *(__u32 *)((addr_t) &regs->psw + addr*2 + 4);
		}
	} else if (addr < (addr_t) (&dummy32->regs.orig_gpr2)) {
		offset = addr - (addr_t) &dummy32->regs.acrs;
		tmp = *(__u32*)((addr_t) &child->thread.acrs + offset);

	} else if (addr == (addr_t) (&dummy32->regs.orig_gpr2)) {
		tmp = *(__u32*)((addr_t) &task_pt_regs(child)->orig_gpr2 + 4);

	} else if (addr < (addr_t) &dummy32->regs.fp_regs) {
		tmp = 0;

	} else if (addr < (addr_t) (&dummy32->regs.fp_regs + 1)) {
	        offset = addr - (addr_t) &dummy32->regs.fp_regs;
		tmp = *(__u32 *)((addr_t) &child->thread.fp_regs + offset);

	} else if (addr < (addr_t) (&dummy32->regs.per_info + 1)) {
		addr -= (addr_t) &dummy32->regs.per_info;
		tmp = __peek_user_per_compat(child, addr);

	} else
		tmp = 0;

	return tmp;
}

static int peek_user_compat(struct task_struct *child,
			    addr_t addr, addr_t data)
{
	__u32 tmp;

	if (!is_compat_task() || (addr & 3) || addr > sizeof(struct user) - 3)
		return -EIO;

	tmp = __peek_user_compat(child, addr);
	return put_user(tmp, (__u32 __user *) data);
}

static inline void __poke_user_per_compat(struct task_struct *child,
					  addr_t addr, __u32 data)
{
	struct compat_per_struct_kernel *dummy32 = NULL;

	if (addr == (addr_t) &dummy32->cr9)
		
		child->thread.per_user.control =
			data & (PER_EVENT_MASK | PER_CONTROL_MASK);
	else if (addr == (addr_t) &dummy32->starting_addr)
		
		child->thread.per_user.start = data;
	else if (addr == (addr_t) &dummy32->ending_addr)
		
		child->thread.per_user.end = data;
}

static int __poke_user_compat(struct task_struct *child,
			      addr_t addr, addr_t data)
{
	struct compat_user *dummy32 = NULL;
	__u32 tmp = (__u32) data;
	addr_t offset;

	if (addr < (addr_t) &dummy32->regs.acrs) {
		struct pt_regs *regs = task_pt_regs(child);
		if (addr == (addr_t) &dummy32->regs.psw.mask) {
			
			if ((tmp & ~PSW32_MASK_USER) != psw32_user_bits)
				
				return -EINVAL;
			regs->psw.mask = (regs->psw.mask & ~PSW_MASK_USER) |
				(regs->psw.mask & PSW_MASK_BA) |
				(__u64)(tmp & PSW32_MASK_USER) << 32;
		} else if (addr == (addr_t) &dummy32->regs.psw.addr) {
			
			regs->psw.addr = (__u64) tmp & PSW32_ADDR_INSN;
			
			regs->psw.mask = (regs->psw.mask & ~PSW_MASK_BA) |
				(__u64)(tmp & PSW32_ADDR_AMODE);
		} else {
			
			*(__u32*)((addr_t) &regs->psw + addr*2 + 4) = tmp;
		}
	} else if (addr < (addr_t) (&dummy32->regs.orig_gpr2)) {
		offset = addr - (addr_t) &dummy32->regs.acrs;
		*(__u32*)((addr_t) &child->thread.acrs + offset) = tmp;

	} else if (addr == (addr_t) (&dummy32->regs.orig_gpr2)) {
		*(__u32*)((addr_t) &task_pt_regs(child)->orig_gpr2 + 4) = tmp;

	} else if (addr < (addr_t) &dummy32->regs.fp_regs) {
		return 0;

	} else if (addr < (addr_t) (&dummy32->regs.fp_regs + 1)) {
		if (addr == (addr_t) &dummy32->regs.fp_regs.fpc &&
		    (tmp & ~FPC_VALID_MASK) != 0)
			
			return -EINVAL;
	        offset = addr - (addr_t) &dummy32->regs.fp_regs;
		*(__u32 *)((addr_t) &child->thread.fp_regs + offset) = tmp;

	} else if (addr < (addr_t) (&dummy32->regs.per_info + 1)) {
		addr -= (addr_t) &dummy32->regs.per_info;
		__poke_user_per_compat(child, addr, data);
	}

	return 0;
}

static int poke_user_compat(struct task_struct *child,
			    addr_t addr, addr_t data)
{
	if (!is_compat_task() || (addr & 3) ||
	    addr > sizeof(struct compat_user) - 3)
		return -EIO;

	return __poke_user_compat(child, addr, data);
}

long compat_arch_ptrace(struct task_struct *child, compat_long_t request,
			compat_ulong_t caddr, compat_ulong_t cdata)
{
	unsigned long addr = caddr;
	unsigned long data = cdata;
	compat_ptrace_area parea;
	int copied, ret;

	switch (request) {
	case PTRACE_PEEKUSR:
		
		return peek_user_compat(child, addr, data);

	case PTRACE_POKEUSR:
		
		return poke_user_compat(child, addr, data);

	case PTRACE_PEEKUSR_AREA:
	case PTRACE_POKEUSR_AREA:
		if (copy_from_user(&parea, (void __force __user *) addr,
							sizeof(parea)))
			return -EFAULT;
		addr = parea.kernel_addr;
		data = parea.process_addr;
		copied = 0;
		while (copied < parea.len) {
			if (request == PTRACE_PEEKUSR_AREA)
				ret = peek_user_compat(child, addr, data);
			else {
				__u32 utmp;
				if (get_user(utmp,
					     (__u32 __force __user *) data))
					return -EFAULT;
				ret = poke_user_compat(child, addr, utmp);
			}
			if (ret)
				return ret;
			addr += sizeof(unsigned int);
			data += sizeof(unsigned int);
			copied += sizeof(unsigned int);
		}
		return 0;
	case PTRACE_GET_LAST_BREAK:
		put_user(task_thread_info(child)->last_break,
			 (unsigned int __user *) data);
		return 0;
	}
	return compat_ptrace_request(child, request, addr, data);
}
#endif

asmlinkage long do_syscall_trace_enter(struct pt_regs *regs)
{
	long ret = 0;

	
	secure_computing(regs->gprs[2]);

	if (test_thread_flag(TIF_SYSCALL_TRACE) &&
	    (tracehook_report_syscall_entry(regs) ||
	     regs->gprs[2] >= NR_syscalls)) {
		clear_thread_flag(TIF_SYSCALL);
		ret = -1;
	}

	if (unlikely(test_thread_flag(TIF_SYSCALL_TRACEPOINT)))
		trace_sys_enter(regs, regs->gprs[2]);

	audit_syscall_entry(is_compat_task() ?
				AUDIT_ARCH_S390 : AUDIT_ARCH_S390X,
			    regs->gprs[2], regs->orig_gpr2,
			    regs->gprs[3], regs->gprs[4],
			    regs->gprs[5]);
	return ret ?: regs->gprs[2];
}

asmlinkage void do_syscall_trace_exit(struct pt_regs *regs)
{
	audit_syscall_exit(regs);

	if (unlikely(test_thread_flag(TIF_SYSCALL_TRACEPOINT)))
		trace_sys_exit(regs, regs->gprs[2]);

	if (test_thread_flag(TIF_SYSCALL_TRACE))
		tracehook_report_syscall_exit(regs, 0);
}


static int s390_regs_get(struct task_struct *target,
			 const struct user_regset *regset,
			 unsigned int pos, unsigned int count,
			 void *kbuf, void __user *ubuf)
{
	if (target == current)
		save_access_regs(target->thread.acrs);

	if (kbuf) {
		unsigned long *k = kbuf;
		while (count > 0) {
			*k++ = __peek_user(target, pos);
			count -= sizeof(*k);
			pos += sizeof(*k);
		}
	} else {
		unsigned long __user *u = ubuf;
		while (count > 0) {
			if (__put_user(__peek_user(target, pos), u++))
				return -EFAULT;
			count -= sizeof(*u);
			pos += sizeof(*u);
		}
	}
	return 0;
}

static int s390_regs_set(struct task_struct *target,
			 const struct user_regset *regset,
			 unsigned int pos, unsigned int count,
			 const void *kbuf, const void __user *ubuf)
{
	int rc = 0;

	if (target == current)
		save_access_regs(target->thread.acrs);

	if (kbuf) {
		const unsigned long *k = kbuf;
		while (count > 0 && !rc) {
			rc = __poke_user(target, pos, *k++);
			count -= sizeof(*k);
			pos += sizeof(*k);
		}
	} else {
		const unsigned long  __user *u = ubuf;
		while (count > 0 && !rc) {
			unsigned long word;
			rc = __get_user(word, u++);
			if (rc)
				break;
			rc = __poke_user(target, pos, word);
			count -= sizeof(*u);
			pos += sizeof(*u);
		}
	}

	if (rc == 0 && target == current)
		restore_access_regs(target->thread.acrs);

	return rc;
}

static int s390_fpregs_get(struct task_struct *target,
			   const struct user_regset *regset, unsigned int pos,
			   unsigned int count, void *kbuf, void __user *ubuf)
{
	if (target == current)
		save_fp_regs(&target->thread.fp_regs);

	return user_regset_copyout(&pos, &count, &kbuf, &ubuf,
				   &target->thread.fp_regs, 0, -1);
}

static int s390_fpregs_set(struct task_struct *target,
			   const struct user_regset *regset, unsigned int pos,
			   unsigned int count, const void *kbuf,
			   const void __user *ubuf)
{
	int rc = 0;

	if (target == current)
		save_fp_regs(&target->thread.fp_regs);

	
	if (count > 0 && pos < offsetof(s390_fp_regs, fprs)) {
		u32 fpc[2] = { target->thread.fp_regs.fpc, 0 };
		rc = user_regset_copyin(&pos, &count, &kbuf, &ubuf, &fpc,
					0, offsetof(s390_fp_regs, fprs));
		if (rc)
			return rc;
		if ((fpc[0] & ~FPC_VALID_MASK) != 0 || fpc[1] != 0)
			return -EINVAL;
		target->thread.fp_regs.fpc = fpc[0];
	}

	if (rc == 0 && count > 0)
		rc = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
					target->thread.fp_regs.fprs,
					offsetof(s390_fp_regs, fprs), -1);

	if (rc == 0 && target == current)
		restore_fp_regs(&target->thread.fp_regs);

	return rc;
}

#ifdef CONFIG_64BIT

static int s390_last_break_get(struct task_struct *target,
			       const struct user_regset *regset,
			       unsigned int pos, unsigned int count,
			       void *kbuf, void __user *ubuf)
{
	if (count > 0) {
		if (kbuf) {
			unsigned long *k = kbuf;
			*k = task_thread_info(target)->last_break;
		} else {
			unsigned long  __user *u = ubuf;
			if (__put_user(task_thread_info(target)->last_break, u))
				return -EFAULT;
		}
	}
	return 0;
}

static int s390_last_break_set(struct task_struct *target,
			       const struct user_regset *regset,
			       unsigned int pos, unsigned int count,
			       const void *kbuf, const void __user *ubuf)
{
	return 0;
}

#endif

static int s390_system_call_get(struct task_struct *target,
				const struct user_regset *regset,
				unsigned int pos, unsigned int count,
				void *kbuf, void __user *ubuf)
{
	unsigned int *data = &task_thread_info(target)->system_call;
	return user_regset_copyout(&pos, &count, &kbuf, &ubuf,
				   data, 0, sizeof(unsigned int));
}

static int s390_system_call_set(struct task_struct *target,
				const struct user_regset *regset,
				unsigned int pos, unsigned int count,
				const void *kbuf, const void __user *ubuf)
{
	unsigned int *data = &task_thread_info(target)->system_call;
	return user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				  data, 0, sizeof(unsigned int));
}

static const struct user_regset s390_regsets[] = {
	[REGSET_GENERAL] = {
		.core_note_type = NT_PRSTATUS,
		.n = sizeof(s390_regs) / sizeof(long),
		.size = sizeof(long),
		.align = sizeof(long),
		.get = s390_regs_get,
		.set = s390_regs_set,
	},
	[REGSET_FP] = {
		.core_note_type = NT_PRFPREG,
		.n = sizeof(s390_fp_regs) / sizeof(long),
		.size = sizeof(long),
		.align = sizeof(long),
		.get = s390_fpregs_get,
		.set = s390_fpregs_set,
	},
#ifdef CONFIG_64BIT
	[REGSET_LAST_BREAK] = {
		.core_note_type = NT_S390_LAST_BREAK,
		.n = 1,
		.size = sizeof(long),
		.align = sizeof(long),
		.get = s390_last_break_get,
		.set = s390_last_break_set,
	},
#endif
	[REGSET_SYSTEM_CALL] = {
		.core_note_type = NT_S390_SYSTEM_CALL,
		.n = 1,
		.size = sizeof(unsigned int),
		.align = sizeof(unsigned int),
		.get = s390_system_call_get,
		.set = s390_system_call_set,
	},
};

static const struct user_regset_view user_s390_view = {
	.name = UTS_MACHINE,
	.e_machine = EM_S390,
	.regsets = s390_regsets,
	.n = ARRAY_SIZE(s390_regsets)
};

#ifdef CONFIG_COMPAT
static int s390_compat_regs_get(struct task_struct *target,
				const struct user_regset *regset,
				unsigned int pos, unsigned int count,
				void *kbuf, void __user *ubuf)
{
	if (target == current)
		save_access_regs(target->thread.acrs);

	if (kbuf) {
		compat_ulong_t *k = kbuf;
		while (count > 0) {
			*k++ = __peek_user_compat(target, pos);
			count -= sizeof(*k);
			pos += sizeof(*k);
		}
	} else {
		compat_ulong_t __user *u = ubuf;
		while (count > 0) {
			if (__put_user(__peek_user_compat(target, pos), u++))
				return -EFAULT;
			count -= sizeof(*u);
			pos += sizeof(*u);
		}
	}
	return 0;
}

static int s390_compat_regs_set(struct task_struct *target,
				const struct user_regset *regset,
				unsigned int pos, unsigned int count,
				const void *kbuf, const void __user *ubuf)
{
	int rc = 0;

	if (target == current)
		save_access_regs(target->thread.acrs);

	if (kbuf) {
		const compat_ulong_t *k = kbuf;
		while (count > 0 && !rc) {
			rc = __poke_user_compat(target, pos, *k++);
			count -= sizeof(*k);
			pos += sizeof(*k);
		}
	} else {
		const compat_ulong_t  __user *u = ubuf;
		while (count > 0 && !rc) {
			compat_ulong_t word;
			rc = __get_user(word, u++);
			if (rc)
				break;
			rc = __poke_user_compat(target, pos, word);
			count -= sizeof(*u);
			pos += sizeof(*u);
		}
	}

	if (rc == 0 && target == current)
		restore_access_regs(target->thread.acrs);

	return rc;
}

static int s390_compat_regs_high_get(struct task_struct *target,
				     const struct user_regset *regset,
				     unsigned int pos, unsigned int count,
				     void *kbuf, void __user *ubuf)
{
	compat_ulong_t *gprs_high;

	gprs_high = (compat_ulong_t *)
		&task_pt_regs(target)->gprs[pos / sizeof(compat_ulong_t)];
	if (kbuf) {
		compat_ulong_t *k = kbuf;
		while (count > 0) {
			*k++ = *gprs_high;
			gprs_high += 2;
			count -= sizeof(*k);
		}
	} else {
		compat_ulong_t __user *u = ubuf;
		while (count > 0) {
			if (__put_user(*gprs_high, u++))
				return -EFAULT;
			gprs_high += 2;
			count -= sizeof(*u);
		}
	}
	return 0;
}

static int s390_compat_regs_high_set(struct task_struct *target,
				     const struct user_regset *regset,
				     unsigned int pos, unsigned int count,
				     const void *kbuf, const void __user *ubuf)
{
	compat_ulong_t *gprs_high;
	int rc = 0;

	gprs_high = (compat_ulong_t *)
		&task_pt_regs(target)->gprs[pos / sizeof(compat_ulong_t)];
	if (kbuf) {
		const compat_ulong_t *k = kbuf;
		while (count > 0) {
			*gprs_high = *k++;
			*gprs_high += 2;
			count -= sizeof(*k);
		}
	} else {
		const compat_ulong_t  __user *u = ubuf;
		while (count > 0 && !rc) {
			unsigned long word;
			rc = __get_user(word, u++);
			if (rc)
				break;
			*gprs_high = word;
			*gprs_high += 2;
			count -= sizeof(*u);
		}
	}

	return rc;
}

static int s390_compat_last_break_get(struct task_struct *target,
				      const struct user_regset *regset,
				      unsigned int pos, unsigned int count,
				      void *kbuf, void __user *ubuf)
{
	compat_ulong_t last_break;

	if (count > 0) {
		last_break = task_thread_info(target)->last_break;
		if (kbuf) {
			unsigned long *k = kbuf;
			*k = last_break;
		} else {
			unsigned long  __user *u = ubuf;
			if (__put_user(last_break, u))
				return -EFAULT;
		}
	}
	return 0;
}

static int s390_compat_last_break_set(struct task_struct *target,
				      const struct user_regset *regset,
				      unsigned int pos, unsigned int count,
				      const void *kbuf, const void __user *ubuf)
{
	return 0;
}

static const struct user_regset s390_compat_regsets[] = {
	[REGSET_GENERAL] = {
		.core_note_type = NT_PRSTATUS,
		.n = sizeof(s390_compat_regs) / sizeof(compat_long_t),
		.size = sizeof(compat_long_t),
		.align = sizeof(compat_long_t),
		.get = s390_compat_regs_get,
		.set = s390_compat_regs_set,
	},
	[REGSET_FP] = {
		.core_note_type = NT_PRFPREG,
		.n = sizeof(s390_fp_regs) / sizeof(compat_long_t),
		.size = sizeof(compat_long_t),
		.align = sizeof(compat_long_t),
		.get = s390_fpregs_get,
		.set = s390_fpregs_set,
	},
	[REGSET_LAST_BREAK] = {
		.core_note_type = NT_S390_LAST_BREAK,
		.n = 1,
		.size = sizeof(long),
		.align = sizeof(long),
		.get = s390_compat_last_break_get,
		.set = s390_compat_last_break_set,
	},
	[REGSET_SYSTEM_CALL] = {
		.core_note_type = NT_S390_SYSTEM_CALL,
		.n = 1,
		.size = sizeof(compat_uint_t),
		.align = sizeof(compat_uint_t),
		.get = s390_system_call_get,
		.set = s390_system_call_set,
	},
	[REGSET_GENERAL_EXTENDED] = {
		.core_note_type = NT_S390_HIGH_GPRS,
		.n = sizeof(s390_compat_regs_high) / sizeof(compat_long_t),
		.size = sizeof(compat_long_t),
		.align = sizeof(compat_long_t),
		.get = s390_compat_regs_high_get,
		.set = s390_compat_regs_high_set,
	},
};

static const struct user_regset_view user_s390_compat_view = {
	.name = "s390",
	.e_machine = EM_S390,
	.regsets = s390_compat_regsets,
	.n = ARRAY_SIZE(s390_compat_regsets)
};
#endif

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
#ifdef CONFIG_COMPAT
	if (test_tsk_thread_flag(task, TIF_31BIT))
		return &user_s390_compat_view;
#endif
	return &user_s390_view;
}

static const char *gpr_names[NUM_GPRS] = {
	"r0", "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
};

unsigned long regs_get_register(struct pt_regs *regs, unsigned int offset)
{
	if (offset >= NUM_GPRS)
		return 0;
	return regs->gprs[offset];
}

int regs_query_register_offset(const char *name)
{
	unsigned long offset;

	if (!name || *name != 'r')
		return -EINVAL;
	if (strict_strtoul(name + 1, 10, &offset))
		return -EINVAL;
	if (offset >= NUM_GPRS)
		return -EINVAL;
	return offset;
}

const char *regs_query_register_name(unsigned int offset)
{
	if (offset >= NUM_GPRS)
		return NULL;
	return gpr_names[offset];
}

static int regs_within_kernel_stack(struct pt_regs *regs, unsigned long addr)
{
	unsigned long ksp = kernel_stack_pointer(regs);

	return (addr & ~(THREAD_SIZE - 1)) == (ksp & ~(THREAD_SIZE - 1));
}

unsigned long regs_get_kernel_stack_nth(struct pt_regs *regs, unsigned int n)
{
	unsigned long addr;

	addr = kernel_stack_pointer(regs) + n * sizeof(long);
	if (!regs_within_kernel_stack(regs, addr))
		return 0;
	return *(unsigned long *)addr;
}
