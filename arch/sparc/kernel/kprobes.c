/* arch/sparc64/kernel/kprobes.c
 *
 * Copyright (C) 2004 David S. Miller <davem@davemloft.net>
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/kdebug.h>
#include <linux/slab.h>
#include <asm/signal.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>


DEFINE_PER_CPU(struct kprobe *, current_kprobe) = NULL;
DEFINE_PER_CPU(struct kprobe_ctlblk, kprobe_ctlblk);

struct kretprobe_blackpoint kretprobe_blacklist[] = {{NULL, NULL}};

int __kprobes arch_prepare_kprobe(struct kprobe *p)
{
	if ((unsigned long) p->addr & 0x3UL)
		return -EILSEQ;

	p->ainsn.insn[0] = *p->addr;
	flushi(&p->ainsn.insn[0]);

	p->ainsn.insn[1] = BREAKPOINT_INSTRUCTION_2;
	flushi(&p->ainsn.insn[1]);

	p->opcode = *p->addr;
	return 0;
}

void __kprobes arch_arm_kprobe(struct kprobe *p)
{
	*p->addr = BREAKPOINT_INSTRUCTION;
	flushi(p->addr);
}

void __kprobes arch_disarm_kprobe(struct kprobe *p)
{
	*p->addr = p->opcode;
	flushi(p->addr);
}

static void __kprobes save_previous_kprobe(struct kprobe_ctlblk *kcb)
{
	kcb->prev_kprobe.kp = kprobe_running();
	kcb->prev_kprobe.status = kcb->kprobe_status;
	kcb->prev_kprobe.orig_tnpc = kcb->kprobe_orig_tnpc;
	kcb->prev_kprobe.orig_tstate_pil = kcb->kprobe_orig_tstate_pil;
}

static void __kprobes restore_previous_kprobe(struct kprobe_ctlblk *kcb)
{
	__get_cpu_var(current_kprobe) = kcb->prev_kprobe.kp;
	kcb->kprobe_status = kcb->prev_kprobe.status;
	kcb->kprobe_orig_tnpc = kcb->prev_kprobe.orig_tnpc;
	kcb->kprobe_orig_tstate_pil = kcb->prev_kprobe.orig_tstate_pil;
}

static void __kprobes set_current_kprobe(struct kprobe *p, struct pt_regs *regs,
				struct kprobe_ctlblk *kcb)
{
	__get_cpu_var(current_kprobe) = p;
	kcb->kprobe_orig_tnpc = regs->tnpc;
	kcb->kprobe_orig_tstate_pil = (regs->tstate & TSTATE_PIL);
}

static void __kprobes prepare_singlestep(struct kprobe *p, struct pt_regs *regs,
			struct kprobe_ctlblk *kcb)
{
	regs->tstate |= TSTATE_PIL;

	
	if (p->opcode == BREAKPOINT_INSTRUCTION) {
		regs->tpc = (unsigned long) p->addr;
		regs->tnpc = kcb->kprobe_orig_tnpc;
	} else {
		regs->tpc = (unsigned long) &p->ainsn.insn[0];
		regs->tnpc = (unsigned long) &p->ainsn.insn[1];
	}
}

static int __kprobes kprobe_handler(struct pt_regs *regs)
{
	struct kprobe *p;
	void *addr = (void *) regs->tpc;
	int ret = 0;
	struct kprobe_ctlblk *kcb;

	preempt_disable();
	kcb = get_kprobe_ctlblk();

	if (kprobe_running()) {
		p = get_kprobe(addr);
		if (p) {
			if (kcb->kprobe_status == KPROBE_HIT_SS) {
				regs->tstate = ((regs->tstate & ~TSTATE_PIL) |
					kcb->kprobe_orig_tstate_pil);
				goto no_kprobe;
			}
			save_previous_kprobe(kcb);
			set_current_kprobe(p, regs, kcb);
			kprobes_inc_nmissed_count(p);
			kcb->kprobe_status = KPROBE_REENTER;
			prepare_singlestep(p, regs, kcb);
			return 1;
		} else {
			if (*(u32 *)addr != BREAKPOINT_INSTRUCTION) {
				ret = 1;
				goto no_kprobe;
			}
			p = __get_cpu_var(current_kprobe);
			if (p->break_handler && p->break_handler(p, regs))
				goto ss_probe;
		}
		goto no_kprobe;
	}

	p = get_kprobe(addr);
	if (!p) {
		if (*(u32 *)addr != BREAKPOINT_INSTRUCTION) {
			ret = 1;
		}
		
		goto no_kprobe;
	}

	set_current_kprobe(p, regs, kcb);
	kcb->kprobe_status = KPROBE_HIT_ACTIVE;
	if (p->pre_handler && p->pre_handler(p, regs))
		return 1;

ss_probe:
	prepare_singlestep(p, regs, kcb);
	kcb->kprobe_status = KPROBE_HIT_SS;
	return 1;

no_kprobe:
	preempt_enable_no_resched();
	return ret;
}

static unsigned long __kprobes relbranch_fixup(u32 insn, struct kprobe *p,
					       struct pt_regs *regs)
{
	unsigned long real_pc = (unsigned long) p->addr;

	
	if (regs->tnpc == regs->tpc + 0x4UL)
		return real_pc + 0x8UL;

	if ((insn & 0xc0000000) == 0x40000000 ||
	    (insn & 0xc1c00000) == 0x00400000 ||
	    (insn & 0xc1c00000) == 0x00800000) {
		unsigned long ainsn_addr;

		ainsn_addr = (unsigned long) &p->ainsn.insn[0];

		return (real_pc + (regs->tnpc - ainsn_addr));
	}

	return regs->tnpc;
}

static void __kprobes retpc_fixup(struct pt_regs *regs, u32 insn,
				  unsigned long real_pc)
{
	unsigned long *slot = NULL;

	
	if ((insn & 0xc0000000) == 0x40000000) {
		slot = &regs->u_regs[UREG_I7];
	}

	
	if ((insn & 0xc1f80000) == 0x81c00000) {
		unsigned long rd = ((insn >> 25) & 0x1f);

		if (rd <= 15) {
			slot = &regs->u_regs[rd];
		} else {
			
			flushw_all();

			rd -= 16;
			slot = (unsigned long *)
				(regs->u_regs[UREG_FP] + STACK_BIAS);
			slot += rd;
		}
	}
	if (slot != NULL)
		*slot = real_pc;
}

static void __kprobes resume_execution(struct kprobe *p,
		struct pt_regs *regs, struct kprobe_ctlblk *kcb)
{
	u32 insn = p->ainsn.insn[0];

	regs->tnpc = relbranch_fixup(insn, p, regs);

	
	regs->tpc = kcb->kprobe_orig_tnpc;

	retpc_fixup(regs, insn, (unsigned long) p->addr);

	regs->tstate = ((regs->tstate & ~TSTATE_PIL) |
			kcb->kprobe_orig_tstate_pil);
}

static int __kprobes post_kprobe_handler(struct pt_regs *regs)
{
	struct kprobe *cur = kprobe_running();
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();

	if (!cur)
		return 0;

	if ((kcb->kprobe_status != KPROBE_REENTER) && cur->post_handler) {
		kcb->kprobe_status = KPROBE_HIT_SSDONE;
		cur->post_handler(cur, regs, 0);
	}

	resume_execution(cur, regs, kcb);

	
	if (kcb->kprobe_status == KPROBE_REENTER) {
		restore_previous_kprobe(kcb);
		goto out;
	}
	reset_current_kprobe();
out:
	preempt_enable_no_resched();

	return 1;
}

int __kprobes kprobe_fault_handler(struct pt_regs *regs, int trapnr)
{
	struct kprobe *cur = kprobe_running();
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	const struct exception_table_entry *entry;

	switch(kcb->kprobe_status) {
	case KPROBE_HIT_SS:
	case KPROBE_REENTER:
		regs->tpc = (unsigned long)cur->addr;
		regs->tnpc = kcb->kprobe_orig_tnpc;
		regs->tstate = ((regs->tstate & ~TSTATE_PIL) |
				kcb->kprobe_orig_tstate_pil);
		if (kcb->kprobe_status == KPROBE_REENTER)
			restore_previous_kprobe(kcb);
		else
			reset_current_kprobe();
		preempt_enable_no_resched();
		break;
	case KPROBE_HIT_ACTIVE:
	case KPROBE_HIT_SSDONE:
		kprobes_inc_nmissed_count(cur);

		if (cur->fault_handler && cur->fault_handler(cur, regs, trapnr))
			return 1;


		entry = search_exception_tables(regs->tpc);
		if (entry) {
			regs->tpc = entry->fixup;
			regs->tnpc = regs->tpc + 4;
			return 1;
		}

		break;
	default:
		break;
	}

	return 0;
}

int __kprobes kprobe_exceptions_notify(struct notifier_block *self,
				       unsigned long val, void *data)
{
	struct die_args *args = (struct die_args *)data;
	int ret = NOTIFY_DONE;

	if (args->regs && user_mode(args->regs))
		return ret;

	switch (val) {
	case DIE_DEBUG:
		if (kprobe_handler(args->regs))
			ret = NOTIFY_STOP;
		break;
	case DIE_DEBUG_2:
		if (post_kprobe_handler(args->regs))
			ret = NOTIFY_STOP;
		break;
	default:
		break;
	}
	return ret;
}

asmlinkage void __kprobes kprobe_trap(unsigned long trap_level,
				      struct pt_regs *regs)
{
	BUG_ON(trap_level != 0x170 && trap_level != 0x171);

	if (user_mode(regs)) {
		local_irq_enable();
		bad_trap(regs, trap_level);
		return;
	}

	if (notify_die((trap_level == 0x170) ? DIE_DEBUG : DIE_DEBUG_2,
		       (trap_level == 0x170) ? "debug" : "debug_2",
		       regs, 0, trap_level, SIGTRAP) != NOTIFY_STOP)
		bad_trap(regs, trap_level);
}

int __kprobes setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct jprobe *jp = container_of(p, struct jprobe, kp);
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();

	memcpy(&(kcb->jprobe_saved_regs), regs, sizeof(*regs));

	regs->tpc  = (unsigned long) jp->entry;
	regs->tnpc = ((unsigned long) jp->entry) + 0x4UL;
	regs->tstate |= TSTATE_PIL;

	return 1;
}

void __kprobes jprobe_return(void)
{
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	register unsigned long orig_fp asm("g1");

	orig_fp = kcb->jprobe_saved_regs.u_regs[UREG_FP];
	__asm__ __volatile__("\n"
"1:	cmp		%%sp, %0\n\t"
	"blu,a,pt	%%xcc, 1b\n\t"
	" restore\n\t"
	".globl		jprobe_return_trap_instruction\n"
"jprobe_return_trap_instruction:\n\t"
	"ta		0x70"
	: 
	: "r" (orig_fp));
}

extern void jprobe_return_trap_instruction(void);

int __kprobes longjmp_break_handler(struct kprobe *p, struct pt_regs *regs)
{
	u32 *addr = (u32 *) regs->tpc;
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();

	if (addr == (u32 *) jprobe_return_trap_instruction) {
		memcpy(regs, &(kcb->jprobe_saved_regs), sizeof(*regs));
		preempt_enable_no_resched();
		return 1;
	}
	return 0;
}

void __kprobes arch_prepare_kretprobe(struct kretprobe_instance *ri,
				      struct pt_regs *regs)
{
	ri->ret_addr = (kprobe_opcode_t *)(regs->u_regs[UREG_RETPC] + 8);

	
	regs->u_regs[UREG_RETPC] =
		((unsigned long)kretprobe_trampoline) - 8;
}

int __kprobes trampoline_probe_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kretprobe_instance *ri = NULL;
	struct hlist_head *head, empty_rp;
	struct hlist_node *node, *tmp;
	unsigned long flags, orig_ret_address = 0;
	unsigned long trampoline_address =(unsigned long)&kretprobe_trampoline;

	INIT_HLIST_HEAD(&empty_rp);
	kretprobe_hash_lock(current, &head, &flags);

	hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task != current)
			
			continue;

		if (ri->rp && ri->rp->handler)
			ri->rp->handler(ri, regs);

		orig_ret_address = (unsigned long)ri->ret_addr;
		recycle_rp_inst(ri, &empty_rp);

		if (orig_ret_address != trampoline_address)
			break;
	}

	kretprobe_assert(ri, orig_ret_address, trampoline_address);
	regs->tpc = orig_ret_address;
	regs->tnpc = orig_ret_address + 4;

	reset_current_kprobe();
	kretprobe_hash_unlock(current, &flags);
	preempt_enable_no_resched();

	hlist_for_each_entry_safe(ri, node, tmp, &empty_rp, hlist) {
		hlist_del(&ri->hlist);
		kfree(ri);
	}
	return 1;
}

void kretprobe_trampoline_holder(void)
{
	asm volatile(".global kretprobe_trampoline\n"
		     "kretprobe_trampoline:\n"
		     "\tnop\n"
		     "\tnop\n");
}
static struct kprobe trampoline_p = {
	.addr = (kprobe_opcode_t *) &kretprobe_trampoline,
	.pre_handler = trampoline_probe_handler
};

int __init arch_init_kprobes(void)
{
	return register_kprobe(&trampoline_p);
}

int __kprobes arch_trampoline_kprobe(struct kprobe *p)
{
	if (p->addr == (kprobe_opcode_t *)&kretprobe_trampoline)
		return 1;

	return 0;
}
