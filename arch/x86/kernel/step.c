#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/ptrace.h>
#include <asm/desc.h>

unsigned long convert_ip_to_linear(struct task_struct *child, struct pt_regs *regs)
{
	unsigned long addr, seg;

	addr = regs->ip;
	seg = regs->cs & 0xffff;
	if (v8086_mode(regs)) {
		addr = (addr & 0xffff) + (seg << 4);
		return addr;
	}

	if ((seg & SEGMENT_TI_MASK) == SEGMENT_LDT) {
		struct desc_struct *desc;
		unsigned long base;

		seg &= ~7UL;

		mutex_lock(&child->mm->context.lock);
		if (unlikely((seg >> 3) >= child->mm->context.size))
			addr = -1L; 
		else {
			desc = child->mm->context.ldt + seg;
			base = get_desc_base(desc);

			
			if (!desc->d)
				addr &= 0xffff;
			addr += base;
		}
		mutex_unlock(&child->mm->context.lock);
	}

	return addr;
}

static int is_setting_trap_flag(struct task_struct *child, struct pt_regs *regs)
{
	int i, copied;
	unsigned char opcode[15];
	unsigned long addr = convert_ip_to_linear(child, regs);

	copied = access_process_vm(child, addr, opcode, sizeof(opcode), 0);
	for (i = 0; i < copied; i++) {
		switch (opcode[i]) {
		
		case 0x9d: case 0xcf:
			return 1;

			

		
		case 0x66: case 0x67:
			continue;
		
		case 0x26: case 0x2e:
		case 0x36: case 0x3e:
		case 0x64: case 0x65:
		case 0xf0: case 0xf2: case 0xf3:
			continue;

#ifdef CONFIG_X86_64
		case 0x40 ... 0x4f:
			if (!user_64bit_mode(regs))
				
				return 0;
			
			continue;
#endif

			

		case 0x9c:
		default:
			return 0;
		}
	}
	return 0;
}

static int enable_single_step(struct task_struct *child)
{
	struct pt_regs *regs = task_pt_regs(child);
	unsigned long oflags;

	if (unlikely(test_tsk_thread_flag(child, TIF_SINGLESTEP)))
		regs->flags |= X86_EFLAGS_TF;

	set_tsk_thread_flag(child, TIF_SINGLESTEP);

	oflags = regs->flags;

	
	regs->flags |= X86_EFLAGS_TF;

	if (is_setting_trap_flag(child, regs)) {
		clear_tsk_thread_flag(child, TIF_FORCED_TF);
		return 0;
	}

	if (oflags & X86_EFLAGS_TF)
		return test_tsk_thread_flag(child, TIF_FORCED_TF);

	set_tsk_thread_flag(child, TIF_FORCED_TF);

	return 1;
}

static void enable_step(struct task_struct *child, bool block)
{
	if (enable_single_step(child) && block) {
		unsigned long debugctl = get_debugctlmsr();

		debugctl |= DEBUGCTLMSR_BTF;
		update_debugctlmsr(debugctl);
		set_tsk_thread_flag(child, TIF_BLOCKSTEP);
	} else if (test_tsk_thread_flag(child, TIF_BLOCKSTEP)) {
		unsigned long debugctl = get_debugctlmsr();

		debugctl &= ~DEBUGCTLMSR_BTF;
		update_debugctlmsr(debugctl);
		clear_tsk_thread_flag(child, TIF_BLOCKSTEP);
	}
}

void user_enable_single_step(struct task_struct *child)
{
	enable_step(child, 0);
}

void user_enable_block_step(struct task_struct *child)
{
	enable_step(child, 1);
}

void user_disable_single_step(struct task_struct *child)
{
	if (test_tsk_thread_flag(child, TIF_BLOCKSTEP)) {
		unsigned long debugctl = get_debugctlmsr();

		debugctl &= ~DEBUGCTLMSR_BTF;
		update_debugctlmsr(debugctl);
		clear_tsk_thread_flag(child, TIF_BLOCKSTEP);
	}

	
	clear_tsk_thread_flag(child, TIF_SINGLESTEP);

	
	if (test_and_clear_tsk_thread_flag(child, TIF_FORCED_TF))
		task_pt_regs(child)->flags &= ~X86_EFLAGS_TF;
}
