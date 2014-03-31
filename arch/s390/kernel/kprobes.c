/*
 *  Kernel Probes (KProbes)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) IBM Corporation, 2002, 2006
 *
 * s390 port, used ppc64 as template. Mike Grundy <grundym@us.ibm.com>
 */

#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/preempt.h>
#include <linux/stop_machine.h>
#include <linux/kdebug.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/sections.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hardirq.h>

DEFINE_PER_CPU(struct kprobe *, current_kprobe);
DEFINE_PER_CPU(struct kprobe_ctlblk, kprobe_ctlblk);

struct kretprobe_blackpoint kretprobe_blacklist[] = { };

static int __kprobes is_prohibited_opcode(kprobe_opcode_t *insn)
{
	switch (insn[0] >> 8) {
	case 0x0c:	
	case 0x0b:	
	case 0x83:	
	case 0x44:	
	case 0xac:	
	case 0xad:	
		return -EINVAL;
	}
	switch (insn[0]) {
	case 0x0101:	
	case 0xb25a:	
	case 0xb240:	
	case 0xb258:	
	case 0xb218:	
	case 0xb228:	
	case 0xb98d:	
		return -EINVAL;
	}
	return 0;
}

static int __kprobes get_fixup_type(kprobe_opcode_t *insn)
{
	
	int fixup = FIXUP_PSW_NORMAL;

	switch (insn[0] >> 8) {
	case 0x05:	
	case 0x0d:	
		fixup = FIXUP_RETURN_REGISTER;
		
		if ((insn[0] & 0x0f) == 0)
			fixup |= FIXUP_BRANCH_NOT_TAKEN;
		break;
	case 0x06:	
	case 0x07:	
		fixup = FIXUP_BRANCH_NOT_TAKEN;
		break;
	case 0x45:	
	case 0x4d:	
		fixup = FIXUP_RETURN_REGISTER;
		break;
	case 0x47:	
	case 0x46:	
	case 0x86:	
	case 0x87:	
		fixup = FIXUP_BRANCH_NOT_TAKEN;
		break;
	case 0x82:	
		fixup = FIXUP_NOT_REQUIRED;
		break;
	case 0xb2:	
		if ((insn[0] & 0xff) == 0xb2)
			fixup = FIXUP_NOT_REQUIRED;
		break;
	case 0xa7:	
		if ((insn[0] & 0x0f) == 0x05)
			fixup |= FIXUP_RETURN_REGISTER;
		break;
	case 0xc0:
		if ((insn[0] & 0x0f) == 0x00 ||	
		    (insn[0] & 0x0f) == 0x05)	
		fixup |= FIXUP_RETURN_REGISTER;
		break;
	case 0xeb:
		if ((insn[2] & 0xff) == 0x44 ||	
		    (insn[2] & 0xff) == 0x45)	
			fixup = FIXUP_BRANCH_NOT_TAKEN;
		break;
	case 0xe3:	
		if ((insn[2] & 0xff) == 0x46)
			fixup = FIXUP_BRANCH_NOT_TAKEN;
		break;
	}
	return fixup;
}

int __kprobes arch_prepare_kprobe(struct kprobe *p)
{
	if ((unsigned long) p->addr & 0x01)
		return -EINVAL;

	
	if (is_prohibited_opcode(p->addr))
		return -EINVAL;

	p->opcode = *p->addr;
	memcpy(p->ainsn.insn, p->addr, ((p->opcode >> 14) + 3) & -2);

	return 0;
}

struct ins_replace_args {
	kprobe_opcode_t *ptr;
	kprobe_opcode_t opcode;
};

static int __kprobes swap_instruction(void *aref)
{
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	unsigned long status = kcb->kprobe_status;
	struct ins_replace_args *args = aref;

	kcb->kprobe_status = KPROBE_SWAP_INST;
	probe_kernel_write(args->ptr, &args->opcode, sizeof(args->opcode));
	kcb->kprobe_status = status;
	return 0;
}

void __kprobes arch_arm_kprobe(struct kprobe *p)
{
	struct ins_replace_args args;

	args.ptr = p->addr;
	args.opcode = BREAKPOINT_INSTRUCTION;
	stop_machine(swap_instruction, &args, NULL);
}

void __kprobes arch_disarm_kprobe(struct kprobe *p)
{
	struct ins_replace_args args;

	args.ptr = p->addr;
	args.opcode = p->opcode;
	stop_machine(swap_instruction, &args, NULL);
}

void __kprobes arch_remove_kprobe(struct kprobe *p)
{
}

static void __kprobes enable_singlestep(struct kprobe_ctlblk *kcb,
					struct pt_regs *regs,
					unsigned long ip)
{
	struct per_regs per_kprobe;

	
	per_kprobe.control = PER_EVENT_IFETCH;
	per_kprobe.start = ip;
	per_kprobe.end = ip;

	
	__ctl_store(kcb->kprobe_saved_ctl, 9, 11);
	kcb->kprobe_saved_imask = regs->psw.mask &
		(PSW_MASK_PER | PSW_MASK_IO | PSW_MASK_EXT);

	
	__ctl_load(per_kprobe, 9, 11);
	regs->psw.mask |= PSW_MASK_PER;
	regs->psw.mask &= ~(PSW_MASK_IO | PSW_MASK_EXT);
	regs->psw.addr = ip | PSW_ADDR_AMODE;
}

static void __kprobes disable_singlestep(struct kprobe_ctlblk *kcb,
					 struct pt_regs *regs,
					 unsigned long ip)
{
	
	__ctl_load(kcb->kprobe_saved_ctl, 9, 11);
	regs->psw.mask &= ~PSW_MASK_PER;
	regs->psw.mask |= kcb->kprobe_saved_imask;
	regs->psw.addr = ip | PSW_ADDR_AMODE;
}

static void __kprobes push_kprobe(struct kprobe_ctlblk *kcb, struct kprobe *p)
{
	kcb->prev_kprobe.kp = __get_cpu_var(current_kprobe);
	kcb->prev_kprobe.status = kcb->kprobe_status;
	__get_cpu_var(current_kprobe) = p;
}

static void __kprobes pop_kprobe(struct kprobe_ctlblk *kcb)
{
	__get_cpu_var(current_kprobe) = kcb->prev_kprobe.kp;
	kcb->kprobe_status = kcb->prev_kprobe.status;
}

void __kprobes arch_prepare_kretprobe(struct kretprobe_instance *ri,
					struct pt_regs *regs)
{
	ri->ret_addr = (kprobe_opcode_t *) regs->gprs[14];

	
	regs->gprs[14] = (unsigned long) &kretprobe_trampoline;
}

static void __kprobes kprobe_reenter_check(struct kprobe_ctlblk *kcb,
					   struct kprobe *p)
{
	switch (kcb->kprobe_status) {
	case KPROBE_HIT_SSDONE:
	case KPROBE_HIT_ACTIVE:
		kprobes_inc_nmissed_count(p);
		break;
	case KPROBE_HIT_SS:
	case KPROBE_REENTER:
	default:
		printk(KERN_EMERG "Invalid kprobe detected at %p.\n", p->addr);
		dump_kprobe(p);
		BUG();
	}
}

static int __kprobes kprobe_handler(struct pt_regs *regs)
{
	struct kprobe_ctlblk *kcb;
	struct kprobe *p;

	preempt_disable();
	kcb = get_kprobe_ctlblk();
	p = get_kprobe((void *)((regs->psw.addr & PSW_ADDR_INSN) - 2));

	if (p) {
		if (kprobe_running()) {
			kprobe_reenter_check(kcb, p);
			push_kprobe(kcb, p);
			kcb->kprobe_status = KPROBE_REENTER;
		} else {
			push_kprobe(kcb, p);
			kcb->kprobe_status = KPROBE_HIT_ACTIVE;
			if (p->pre_handler && p->pre_handler(p, regs))
				return 1;
			kcb->kprobe_status = KPROBE_HIT_SS;
		}
		enable_singlestep(kcb, regs, (unsigned long) p->ainsn.insn);
		return 1;
	} else if (kprobe_running()) {
		p = __get_cpu_var(current_kprobe);
		if (p->break_handler && p->break_handler(p, regs)) {
			kcb->kprobe_status = KPROBE_HIT_SS;
			enable_singlestep(kcb, regs,
					  (unsigned long) p->ainsn.insn);
			return 1;
		} 
	} 
	preempt_enable_no_resched();
	return 0;
}

static void __used kretprobe_trampoline_holder(void)
{
	asm volatile(".global kretprobe_trampoline\n"
		     "kretprobe_trampoline: bcr 0,0\n");
}

static int __kprobes trampoline_probe_handler(struct kprobe *p,
					      struct pt_regs *regs)
{
	struct kretprobe_instance *ri;
	struct hlist_head *head, empty_rp;
	struct hlist_node *node, *tmp;
	unsigned long flags, orig_ret_address;
	unsigned long trampoline_address;
	kprobe_opcode_t *correct_ret_addr;

	INIT_HLIST_HEAD(&empty_rp);
	kretprobe_hash_lock(current, &head, &flags);

	ri = NULL;
	orig_ret_address = 0;
	correct_ret_addr = NULL;
	trampoline_address = (unsigned long) &kretprobe_trampoline;
	hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task != current)
			
			continue;

		orig_ret_address = (unsigned long) ri->ret_addr;

		if (orig_ret_address != trampoline_address)
			break;
	}

	kretprobe_assert(ri, orig_ret_address, trampoline_address);

	correct_ret_addr = ri->ret_addr;
	hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task != current)
			
			continue;

		orig_ret_address = (unsigned long) ri->ret_addr;

		if (ri->rp && ri->rp->handler) {
			ri->ret_addr = correct_ret_addr;
			ri->rp->handler(ri, regs);
		}

		recycle_rp_inst(ri, &empty_rp);

		if (orig_ret_address != trampoline_address)
			break;
	}

	regs->psw.addr = orig_ret_address | PSW_ADDR_AMODE;

	pop_kprobe(get_kprobe_ctlblk());
	kretprobe_hash_unlock(current, &flags);
	preempt_enable_no_resched();

	hlist_for_each_entry_safe(ri, node, tmp, &empty_rp, hlist) {
		hlist_del(&ri->hlist);
		kfree(ri);
	}
	return 1;
}

static void __kprobes resume_execution(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	unsigned long ip = regs->psw.addr & PSW_ADDR_INSN;
	int fixup = get_fixup_type(p->ainsn.insn);

	if (fixup & FIXUP_PSW_NORMAL)
		ip += (unsigned long) p->addr - (unsigned long) p->ainsn.insn;

	if (fixup & FIXUP_BRANCH_NOT_TAKEN) {
		int ilen = ((p->ainsn.insn[0] >> 14) + 3) & -2;
		if (ip - (unsigned long) p->ainsn.insn == ilen)
			ip = (unsigned long) p->addr + ilen;
	}

	if (fixup & FIXUP_RETURN_REGISTER) {
		int reg = (p->ainsn.insn[0] & 0xf0) >> 4;
		regs->gprs[reg] += (unsigned long) p->addr -
				   (unsigned long) p->ainsn.insn;
	}

	disable_singlestep(kcb, regs, ip);
}

static int __kprobes post_kprobe_handler(struct pt_regs *regs)
{
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	struct kprobe *p = kprobe_running();

	if (!p)
		return 0;

	if (kcb->kprobe_status != KPROBE_REENTER && p->post_handler) {
		kcb->kprobe_status = KPROBE_HIT_SSDONE;
		p->post_handler(p, regs, 0);
	}

	resume_execution(p, regs);
	pop_kprobe(kcb);
	preempt_enable_no_resched();

	if (regs->psw.mask & PSW_MASK_PER)
		return 0;

	return 1;
}

static int __kprobes kprobe_trap_handler(struct pt_regs *regs, int trapnr)
{
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	struct kprobe *p = kprobe_running();
	const struct exception_table_entry *entry;

	switch(kcb->kprobe_status) {
	case KPROBE_SWAP_INST:
		
		return 0;
	case KPROBE_HIT_SS:
	case KPROBE_REENTER:
		disable_singlestep(kcb, regs, (unsigned long) p->addr);
		pop_kprobe(kcb);
		preempt_enable_no_resched();
		break;
	case KPROBE_HIT_ACTIVE:
	case KPROBE_HIT_SSDONE:
		kprobes_inc_nmissed_count(p);

		if (p->fault_handler && p->fault_handler(p, regs, trapnr))
			return 1;

		entry = search_exception_tables(regs->psw.addr & PSW_ADDR_INSN);
		if (entry) {
			regs->psw.addr = entry->fixup | PSW_ADDR_AMODE;
			return 1;
		}

		break;
	default:
		break;
	}
	return 0;
}

int __kprobes kprobe_fault_handler(struct pt_regs *regs, int trapnr)
{
	int ret;

	if (regs->psw.mask & (PSW_MASK_IO | PSW_MASK_EXT))
		local_irq_disable();
	ret = kprobe_trap_handler(regs, trapnr);
	if (regs->psw.mask & (PSW_MASK_IO | PSW_MASK_EXT))
		local_irq_restore(regs->psw.mask & ~PSW_MASK_PER);
	return ret;
}

int __kprobes kprobe_exceptions_notify(struct notifier_block *self,
				       unsigned long val, void *data)
{
	struct die_args *args = (struct die_args *) data;
	struct pt_regs *regs = args->regs;
	int ret = NOTIFY_DONE;

	if (regs->psw.mask & (PSW_MASK_IO | PSW_MASK_EXT))
		local_irq_disable();

	switch (val) {
	case DIE_BPT:
		if (kprobe_handler(regs))
			ret = NOTIFY_STOP;
		break;
	case DIE_SSTEP:
		if (post_kprobe_handler(regs))
			ret = NOTIFY_STOP;
		break;
	case DIE_TRAP:
		if (!preemptible() && kprobe_running() &&
		    kprobe_trap_handler(regs, args->trapnr))
			ret = NOTIFY_STOP;
		break;
	default:
		break;
	}

	if (regs->psw.mask & (PSW_MASK_IO | PSW_MASK_EXT))
		local_irq_restore(regs->psw.mask & ~PSW_MASK_PER);

	return ret;
}

int __kprobes setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct jprobe *jp = container_of(p, struct jprobe, kp);
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	unsigned long stack;

	memcpy(&kcb->jprobe_saved_regs, regs, sizeof(struct pt_regs));

	
	regs->psw.addr = (unsigned long) jp->entry | PSW_ADDR_AMODE;
	regs->psw.mask &= ~(PSW_MASK_IO | PSW_MASK_EXT);

	
	stack = (unsigned long) regs->gprs[15];

	memcpy(kcb->jprobes_stack, (void *) stack, MIN_STACK_SIZE(stack));
	return 1;
}

void __kprobes jprobe_return(void)
{
	asm volatile(".word 0x0002");
}

static void __used __kprobes jprobe_return_end(void)
{
	asm volatile("bcr 0,0");
}

int __kprobes longjmp_break_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	unsigned long stack;

	stack = (unsigned long) kcb->jprobe_saved_regs.gprs[15];

	
	memcpy(regs, &kcb->jprobe_saved_regs, sizeof(struct pt_regs));
	
	memcpy((void *) stack, kcb->jprobes_stack, MIN_STACK_SIZE(stack));
	preempt_enable_no_resched();
	return 1;
}

static struct kprobe trampoline = {
	.addr = (kprobe_opcode_t *) &kretprobe_trampoline,
	.pre_handler = trampoline_probe_handler
};

int __init arch_init_kprobes(void)
{
	return register_kprobe(&trampoline);
}

int __kprobes arch_trampoline_kprobe(struct kprobe *p)
{
	return p->addr == (kprobe_opcode_t *) &kretprobe_trampoline;
}
