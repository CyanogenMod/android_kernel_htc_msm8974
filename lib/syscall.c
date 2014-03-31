#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <asm/syscall.h>

static int collect_syscall(struct task_struct *target, long *callno,
			   unsigned long args[6], unsigned int maxargs,
			   unsigned long *sp, unsigned long *pc)
{
	struct pt_regs *regs = task_pt_regs(target);
	if (unlikely(!regs))
		return -EAGAIN;

	*sp = user_stack_pointer(regs);
	*pc = instruction_pointer(regs);

	*callno = syscall_get_nr(target, regs);
	if (*callno != -1L && maxargs > 0)
		syscall_get_arguments(target, regs, 0, maxargs, args);

	return 0;
}

int task_current_syscall(struct task_struct *target, long *callno,
			 unsigned long args[6], unsigned int maxargs,
			 unsigned long *sp, unsigned long *pc)
{
	long state;
	unsigned long ncsw;

	if (unlikely(maxargs > 6))
		return -EINVAL;

	if (target == current)
		return collect_syscall(target, callno, args, maxargs, sp, pc);

	state = target->state;
	if (unlikely(!state))
		return -EAGAIN;

	ncsw = wait_task_inactive(target, state);
	if (unlikely(!ncsw) ||
	    unlikely(collect_syscall(target, callno, args, maxargs, sp, pc)) ||
	    unlikely(wait_task_inactive(target, state) != ncsw))
		return -EAGAIN;

	return 0;
}
EXPORT_SYMBOL_GPL(task_current_syscall);
