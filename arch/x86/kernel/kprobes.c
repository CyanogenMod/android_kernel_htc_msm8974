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
 * Copyright (C) IBM Corporation, 2002, 2004
 *
 * 2002-Oct	Created by Vamsi Krishna S <vamsi_krishna@in.ibm.com> Kernel
 *		Probes initial implementation ( includes contributions from
 *		Rusty Russell).
 * 2004-July	Suparna Bhattacharya <suparna@in.ibm.com> added jumper probes
 *		interface to access function arguments.
 * 2004-Oct	Jim Keniston <jkenisto@us.ibm.com> and Prasanna S Panchamukhi
 *		<prasanna@in.ibm.com> adapted for x86_64 from i386.
 * 2005-Mar	Roland McGrath <roland@redhat.com>
 *		Fixed to handle %rip-relative addressing mode correctly.
 * 2005-May	Hien Nguyen <hien@us.ibm.com>, Jim Keniston
 *		<jkenisto@us.ibm.com> and Prasanna S Panchamukhi
 *		<prasanna@in.ibm.com> added function-return probes.
 * 2005-May	Rusty Lynch <rusty.lynch@intel.com>
 *		Added function return probes functionality
 * 2006-Feb	Masami Hiramatsu <hiramatu@sdl.hitachi.co.jp> added
 *		kprobe-booster and kretprobe-booster for i386.
 * 2007-Dec	Masami Hiramatsu <mhiramat@redhat.com> added kprobe-booster
 *		and kretprobe-booster for x86-64
 * 2007-Dec	Masami Hiramatsu <mhiramat@redhat.com>, Arjan van de Ven
 *		<arjan@infradead.org> and Jim Keniston <jkenisto@us.ibm.com>
 *		unified x86 kprobes code.
 */
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/hardirq.h>
#include <linux/preempt.h>
#include <linux/module.h>
#include <linux/kdebug.h>
#include <linux/kallsyms.h>
#include <linux/ftrace.h>

#include <asm/cacheflush.h>
#include <asm/desc.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/alternative.h>
#include <asm/insn.h>
#include <asm/debugreg.h>

#include "kprobes-common.h"

void jprobe_return_end(void);

DEFINE_PER_CPU(struct kprobe *, current_kprobe) = NULL;
DEFINE_PER_CPU(struct kprobe_ctlblk, kprobe_ctlblk);

#define stack_addr(regs) ((unsigned long *)kernel_stack_pointer(regs))

#define W(row, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf)\
	(((b0##UL << 0x0)|(b1##UL << 0x1)|(b2##UL << 0x2)|(b3##UL << 0x3) |   \
	  (b4##UL << 0x4)|(b5##UL << 0x5)|(b6##UL << 0x6)|(b7##UL << 0x7) |   \
	  (b8##UL << 0x8)|(b9##UL << 0x9)|(ba##UL << 0xa)|(bb##UL << 0xb) |   \
	  (bc##UL << 0xc)|(bd##UL << 0xd)|(be##UL << 0xe)|(bf##UL << 0xf))    \
	 << (row % 32))
static volatile u32 twobyte_is_boostable[256 / 32] = {
	
	
	W(0x00, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0) | 
	W(0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) , 
	W(0x20, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) | 
	W(0x30, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) , 
	W(0x40, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1) | 
	W(0x50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) , 
	W(0x60, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1) | 
	W(0x70, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1) , 
	W(0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) | 
	W(0x90, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1) , 
	W(0xa0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1) | 
	W(0xb0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1) , 
	W(0xc0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1) | 
	W(0xd0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1) , 
	W(0xe0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1) | 
	W(0xf0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0)   
	
	
};
#undef W

struct kretprobe_blackpoint kretprobe_blacklist[] = {
	{"__switch_to", }, 
	{NULL, NULL}	
};

const int kretprobe_blacklist_size = ARRAY_SIZE(kretprobe_blacklist);

static void __kprobes __synthesize_relative_insn(void *from, void *to, u8 op)
{
	struct __arch_relative_insn {
		u8 op;
		s32 raddr;
	} __attribute__((packed)) *insn;

	insn = (struct __arch_relative_insn *)from;
	insn->raddr = (s32)((long)(to) - ((long)(from) + 5));
	insn->op = op;
}

void __kprobes synthesize_reljump(void *from, void *to)
{
	__synthesize_relative_insn(from, to, RELATIVEJUMP_OPCODE);
}

void __kprobes synthesize_relcall(void *from, void *to)
{
	__synthesize_relative_insn(from, to, RELATIVECALL_OPCODE);
}

static kprobe_opcode_t *__kprobes skip_prefixes(kprobe_opcode_t *insn)
{
	insn_attr_t attr;

	attr = inat_get_opcode_attribute((insn_byte_t)*insn);
	while (inat_is_legacy_prefix(attr)) {
		insn++;
		attr = inat_get_opcode_attribute((insn_byte_t)*insn);
	}
#ifdef CONFIG_X86_64
	if (inat_is_rex_prefix(attr))
		insn++;
#endif
	return insn;
}

int __kprobes can_boost(kprobe_opcode_t *opcodes)
{
	kprobe_opcode_t opcode;
	kprobe_opcode_t *orig_opcodes = opcodes;

	if (search_exception_tables((unsigned long)opcodes))
		return 0;	

retry:
	if (opcodes - orig_opcodes > MAX_INSN_SIZE - 1)
		return 0;
	opcode = *(opcodes++);

	
	if (opcode == 0x0f) {
		if (opcodes - orig_opcodes > MAX_INSN_SIZE - 1)
			return 0;
		return test_bit(*opcodes,
				(unsigned long *)twobyte_is_boostable);
	}

	switch (opcode & 0xf0) {
#ifdef CONFIG_X86_64
	case 0x40:
		goto retry; 
#endif
	case 0x60:
		if (0x63 < opcode && opcode < 0x67)
			goto retry; 
		
		return (opcode != 0x62 && opcode != 0x67);
	case 0x70:
		return 0; 
	case 0xc0:
		
		return (0xc1 < opcode && opcode < 0xcc) || opcode == 0xcf;
	case 0xd0:
		
		return (opcode == 0xd4 || opcode == 0xd5 || opcode == 0xd7);
	case 0xe0:
		
		return ((opcode & 0x04) || opcode == 0xea);
	case 0xf0:
		if ((opcode & 0x0c) == 0 && opcode != 0xf1)
			goto retry; 
		
		return (opcode == 0xf5 || (0xf7 < opcode && opcode < 0xfe));
	default:
		
		if (opcode == 0x26 || opcode == 0x36 || opcode == 0x3e)
			goto retry; 
		
		return (opcode != 0x2e && opcode != 0x9a);
	}
}

static unsigned long
__recover_probed_insn(kprobe_opcode_t *buf, unsigned long addr)
{
	struct kprobe *kp;

	kp = get_kprobe((void *)addr);
	
	if (!kp)
		return addr;

	/*
	 *  Basically, kp->ainsn.insn has an original instruction.
	 *  However, RIP-relative instruction can not do single-stepping
	 *  at different place, __copy_instruction() tweaks the displacement of
	 *  that instruction. In that case, we can't recover the instruction
	 *  from the kp->ainsn.insn.
	 *
	 *  On the other hand, kp->opcode has a copy of the first byte of
	 *  the probed instruction, which is overwritten by int3. And
	 *  the instruction at kp->addr is not modified by kprobes except
	 *  for the first byte, we can recover the original instruction
	 *  from it and kp->opcode.
	 */
	memcpy(buf, kp->addr, MAX_INSN_SIZE * sizeof(kprobe_opcode_t));
	buf[0] = kp->opcode;
	return (unsigned long)buf;
}

unsigned long recover_probed_instruction(kprobe_opcode_t *buf, unsigned long addr)
{
	unsigned long __addr;

	__addr = __recover_optprobed_insn(buf, addr);
	if (__addr != addr)
		return __addr;

	return __recover_probed_insn(buf, addr);
}

static int __kprobes can_probe(unsigned long paddr)
{
	unsigned long addr, __addr, offset = 0;
	struct insn insn;
	kprobe_opcode_t buf[MAX_INSN_SIZE];

	if (!kallsyms_lookup_size_offset(paddr, NULL, &offset))
		return 0;

	
	addr = paddr - offset;
	while (addr < paddr) {
		__addr = recover_probed_instruction(buf, addr);
		kernel_insn_init(&insn, (void *)__addr);
		insn_get_length(&insn);

		if (insn.opcode.bytes[0] == BREAKPOINT_INSTRUCTION)
			return 0;
		addr += insn.length;
	}

	return (addr == paddr);
}

static int __kprobes is_IF_modifier(kprobe_opcode_t *insn)
{
	
	insn = skip_prefixes(insn);

	switch (*insn) {
	case 0xfa:		
	case 0xfb:		
	case 0xcf:		
	case 0x9d:		
		return 1;
	}

	return 0;
}

int __kprobes __copy_instruction(u8 *dest, u8 *src)
{
	struct insn insn;
	kprobe_opcode_t buf[MAX_INSN_SIZE];

	kernel_insn_init(&insn, (void *)recover_probed_instruction(buf, (unsigned long)src));
	insn_get_length(&insn);
	
	if (insn.opcode.bytes[0] == BREAKPOINT_INSTRUCTION)
		return 0;
	memcpy(dest, insn.kaddr, insn.length);

#ifdef CONFIG_X86_64
	if (insn_rip_relative(&insn)) {
		s64 newdisp;
		u8 *disp;
		kernel_insn_init(&insn, dest);
		insn_get_displacement(&insn);
		newdisp = (u8 *) src + (s64) insn.displacement.value - (u8 *) dest;
		BUG_ON((s64) (s32) newdisp != newdisp); 
		disp = (u8 *) dest + insn_offset_displacement(&insn);
		*(s32 *) disp = (s32) newdisp;
	}
#endif
	return insn.length;
}

static void __kprobes arch_copy_kprobe(struct kprobe *p)
{
	
	__copy_instruction(p->ainsn.insn, p->addr);

	if (can_boost(p->ainsn.insn))
		p->ainsn.boostable = 0;
	else
		p->ainsn.boostable = -1;

	
	p->opcode = p->ainsn.insn[0];
}

int __kprobes arch_prepare_kprobe(struct kprobe *p)
{
	if (alternatives_text_reserved(p->addr, p->addr))
		return -EINVAL;

	if (!can_probe((unsigned long)p->addr))
		return -EILSEQ;
	
	p->ainsn.insn = get_insn_slot();
	if (!p->ainsn.insn)
		return -ENOMEM;
	arch_copy_kprobe(p);
	return 0;
}

void __kprobes arch_arm_kprobe(struct kprobe *p)
{
	text_poke(p->addr, ((unsigned char []){BREAKPOINT_INSTRUCTION}), 1);
}

void __kprobes arch_disarm_kprobe(struct kprobe *p)
{
	text_poke(p->addr, &p->opcode, 1);
}

void __kprobes arch_remove_kprobe(struct kprobe *p)
{
	if (p->ainsn.insn) {
		free_insn_slot(p->ainsn.insn, (p->ainsn.boostable == 1));
		p->ainsn.insn = NULL;
	}
}

static void __kprobes save_previous_kprobe(struct kprobe_ctlblk *kcb)
{
	kcb->prev_kprobe.kp = kprobe_running();
	kcb->prev_kprobe.status = kcb->kprobe_status;
	kcb->prev_kprobe.old_flags = kcb->kprobe_old_flags;
	kcb->prev_kprobe.saved_flags = kcb->kprobe_saved_flags;
}

static void __kprobes restore_previous_kprobe(struct kprobe_ctlblk *kcb)
{
	__this_cpu_write(current_kprobe, kcb->prev_kprobe.kp);
	kcb->kprobe_status = kcb->prev_kprobe.status;
	kcb->kprobe_old_flags = kcb->prev_kprobe.old_flags;
	kcb->kprobe_saved_flags = kcb->prev_kprobe.saved_flags;
}

static void __kprobes set_current_kprobe(struct kprobe *p, struct pt_regs *regs,
				struct kprobe_ctlblk *kcb)
{
	__this_cpu_write(current_kprobe, p);
	kcb->kprobe_saved_flags = kcb->kprobe_old_flags
		= (regs->flags & (X86_EFLAGS_TF | X86_EFLAGS_IF));
	if (is_IF_modifier(p->ainsn.insn))
		kcb->kprobe_saved_flags &= ~X86_EFLAGS_IF;
}

static void __kprobes clear_btf(void)
{
	if (test_thread_flag(TIF_BLOCKSTEP)) {
		unsigned long debugctl = get_debugctlmsr();

		debugctl &= ~DEBUGCTLMSR_BTF;
		update_debugctlmsr(debugctl);
	}
}

static void __kprobes restore_btf(void)
{
	if (test_thread_flag(TIF_BLOCKSTEP)) {
		unsigned long debugctl = get_debugctlmsr();

		debugctl |= DEBUGCTLMSR_BTF;
		update_debugctlmsr(debugctl);
	}
}

void __kprobes
arch_prepare_kretprobe(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	unsigned long *sara = stack_addr(regs);

	ri->ret_addr = (kprobe_opcode_t *) *sara;

	
	*sara = (unsigned long) &kretprobe_trampoline;
}

static void __kprobes
setup_singlestep(struct kprobe *p, struct pt_regs *regs, struct kprobe_ctlblk *kcb, int reenter)
{
	if (setup_detour_execution(p, regs, reenter))
		return;

#if !defined(CONFIG_PREEMPT)
	if (p->ainsn.boostable == 1 && !p->post_handler) {
		
		if (!reenter)
			reset_current_kprobe();
		regs->ip = (unsigned long)p->ainsn.insn;
		preempt_enable_no_resched();
		return;
	}
#endif
	if (reenter) {
		save_previous_kprobe(kcb);
		set_current_kprobe(p, regs, kcb);
		kcb->kprobe_status = KPROBE_REENTER;
	} else
		kcb->kprobe_status = KPROBE_HIT_SS;
	
	clear_btf();
	regs->flags |= X86_EFLAGS_TF;
	regs->flags &= ~X86_EFLAGS_IF;
	
	if (p->opcode == BREAKPOINT_INSTRUCTION)
		regs->ip = (unsigned long)p->addr;
	else
		regs->ip = (unsigned long)p->ainsn.insn;
}

static int __kprobes
reenter_kprobe(struct kprobe *p, struct pt_regs *regs, struct kprobe_ctlblk *kcb)
{
	switch (kcb->kprobe_status) {
	case KPROBE_HIT_SSDONE:
	case KPROBE_HIT_ACTIVE:
		kprobes_inc_nmissed_count(p);
		setup_singlestep(p, regs, kcb, 1);
		break;
	case KPROBE_HIT_SS:
		printk(KERN_WARNING "Unrecoverable kprobe detected at %p.\n",
		       p->addr);
		dump_kprobe(p);
		BUG();
	default:
		
		WARN_ON(1);
		return 0;
	}

	return 1;
}

static int __kprobes kprobe_handler(struct pt_regs *regs)
{
	kprobe_opcode_t *addr;
	struct kprobe *p;
	struct kprobe_ctlblk *kcb;

	addr = (kprobe_opcode_t *)(regs->ip - sizeof(kprobe_opcode_t));
	preempt_disable();

	kcb = get_kprobe_ctlblk();
	p = get_kprobe(addr);

	if (p) {
		if (kprobe_running()) {
			if (reenter_kprobe(p, regs, kcb))
				return 1;
		} else {
			set_current_kprobe(p, regs, kcb);
			kcb->kprobe_status = KPROBE_HIT_ACTIVE;

			if (!p->pre_handler || !p->pre_handler(p, regs))
				setup_singlestep(p, regs, kcb, 0);
			return 1;
		}
	} else if (*addr != BREAKPOINT_INSTRUCTION) {
		regs->ip = (unsigned long)addr;
		preempt_enable_no_resched();
		return 1;
	} else if (kprobe_running()) {
		p = __this_cpu_read(current_kprobe);
		if (p->break_handler && p->break_handler(p, regs)) {
			setup_singlestep(p, regs, kcb, 0);
			return 1;
		}
	} 

	preempt_enable_no_resched();
	return 0;
}

static void __used __kprobes kretprobe_trampoline_holder(void)
{
	asm volatile (
			".global kretprobe_trampoline\n"
			"kretprobe_trampoline: \n"
#ifdef CONFIG_X86_64
			
			"	pushq %rsp\n"
			"	pushfq\n"
			SAVE_REGS_STRING
			"	movq %rsp, %rdi\n"
			"	call trampoline_handler\n"
			
			"	movq %rax, 152(%rsp)\n"
			RESTORE_REGS_STRING
			"	popfq\n"
#else
			"	pushf\n"
			SAVE_REGS_STRING
			"	movl %esp, %eax\n"
			"	call trampoline_handler\n"
			
			"	movl 56(%esp), %edx\n"
			"	movl %edx, 52(%esp)\n"
			
			"	movl %eax, 56(%esp)\n"
			RESTORE_REGS_STRING
			"	popf\n"
#endif
			"	ret\n");
}

static __used __kprobes void *trampoline_handler(struct pt_regs *regs)
{
	struct kretprobe_instance *ri = NULL;
	struct hlist_head *head, empty_rp;
	struct hlist_node *node, *tmp;
	unsigned long flags, orig_ret_address = 0;
	unsigned long trampoline_address = (unsigned long)&kretprobe_trampoline;
	kprobe_opcode_t *correct_ret_addr = NULL;

	INIT_HLIST_HEAD(&empty_rp);
	kretprobe_hash_lock(current, &head, &flags);
	
#ifdef CONFIG_X86_64
	regs->cs = __KERNEL_CS;
#else
	regs->cs = __KERNEL_CS | get_kernel_rpl();
	regs->gs = 0;
#endif
	regs->ip = trampoline_address;
	regs->orig_ax = ~0UL;

	hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task != current)
			
			continue;

		orig_ret_address = (unsigned long)ri->ret_addr;

		if (orig_ret_address != trampoline_address)
			break;
	}

	kretprobe_assert(ri, orig_ret_address, trampoline_address);

	correct_ret_addr = ri->ret_addr;
	hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task != current)
			
			continue;

		orig_ret_address = (unsigned long)ri->ret_addr;
		if (ri->rp && ri->rp->handler) {
			__this_cpu_write(current_kprobe, &ri->rp->kp);
			get_kprobe_ctlblk()->kprobe_status = KPROBE_HIT_ACTIVE;
			ri->ret_addr = correct_ret_addr;
			ri->rp->handler(ri, regs);
			__this_cpu_write(current_kprobe, NULL);
		}

		recycle_rp_inst(ri, &empty_rp);

		if (orig_ret_address != trampoline_address)
			break;
	}

	kretprobe_hash_unlock(current, &flags);

	hlist_for_each_entry_safe(ri, node, tmp, &empty_rp, hlist) {
		hlist_del(&ri->hlist);
		kfree(ri);
	}
	return (void *)orig_ret_address;
}

static void __kprobes
resume_execution(struct kprobe *p, struct pt_regs *regs, struct kprobe_ctlblk *kcb)
{
	unsigned long *tos = stack_addr(regs);
	unsigned long copy_ip = (unsigned long)p->ainsn.insn;
	unsigned long orig_ip = (unsigned long)p->addr;
	kprobe_opcode_t *insn = p->ainsn.insn;

	
	insn = skip_prefixes(insn);

	regs->flags &= ~X86_EFLAGS_TF;
	switch (*insn) {
	case 0x9c:	
		*tos &= ~(X86_EFLAGS_TF | X86_EFLAGS_IF);
		*tos |= kcb->kprobe_old_flags;
		break;
	case 0xc2:	
	case 0xc3:
	case 0xca:
	case 0xcb:
	case 0xcf:
	case 0xea:	
		
		p->ainsn.boostable = 1;
		goto no_change;
	case 0xe8:	
		*tos = orig_ip + (*tos - copy_ip);
		break;
#ifdef CONFIG_X86_32
	case 0x9a:	
		*tos = orig_ip + (*tos - copy_ip);
		goto no_change;
#endif
	case 0xff:
		if ((insn[1] & 0x30) == 0x10) {
			*tos = orig_ip + (*tos - copy_ip);
			goto no_change;
		} else if (((insn[1] & 0x31) == 0x20) ||
			   ((insn[1] & 0x31) == 0x21)) {
			p->ainsn.boostable = 1;
			goto no_change;
		}
	default:
		break;
	}

	if (p->ainsn.boostable == 0) {
		if ((regs->ip > copy_ip) &&
		    (regs->ip - copy_ip) + 5 < MAX_INSN_SIZE) {
			synthesize_reljump((void *)regs->ip,
				(void *)orig_ip + (regs->ip - copy_ip));
			p->ainsn.boostable = 1;
		} else {
			p->ainsn.boostable = -1;
		}
	}

	regs->ip += orig_ip - copy_ip;

no_change:
	restore_btf();
}

static int __kprobes post_kprobe_handler(struct pt_regs *regs)
{
	struct kprobe *cur = kprobe_running();
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();

	if (!cur)
		return 0;

	resume_execution(cur, regs, kcb);
	regs->flags |= kcb->kprobe_saved_flags;

	if ((kcb->kprobe_status != KPROBE_REENTER) && cur->post_handler) {
		kcb->kprobe_status = KPROBE_HIT_SSDONE;
		cur->post_handler(cur, regs, 0);
	}

	
	if (kcb->kprobe_status == KPROBE_REENTER) {
		restore_previous_kprobe(kcb);
		goto out;
	}
	reset_current_kprobe();
out:
	preempt_enable_no_resched();

	if (regs->flags & X86_EFLAGS_TF)
		return 0;

	return 1;
}

int __kprobes kprobe_fault_handler(struct pt_regs *regs, int trapnr)
{
	struct kprobe *cur = kprobe_running();
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();

	switch (kcb->kprobe_status) {
	case KPROBE_HIT_SS:
	case KPROBE_REENTER:
		regs->ip = (unsigned long)cur->addr;
		regs->flags |= kcb->kprobe_old_flags;
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

		if (fixup_exception(regs))
			return 1;

		break;
	default:
		break;
	}
	return 0;
}

int __kprobes
kprobe_exceptions_notify(struct notifier_block *self, unsigned long val, void *data)
{
	struct die_args *args = data;
	int ret = NOTIFY_DONE;

	if (args->regs && user_mode_vm(args->regs))
		return ret;

	switch (val) {
	case DIE_INT3:
		if (kprobe_handler(args->regs))
			ret = NOTIFY_STOP;
		break;
	case DIE_DEBUG:
		if (post_kprobe_handler(args->regs)) {
			(*(unsigned long *)ERR_PTR(args->err)) &= ~DR_STEP;
			ret = NOTIFY_STOP;
		}
		break;
	case DIE_GPF:
		if (!preemptible() && kprobe_running() &&
		    kprobe_fault_handler(args->regs, args->trapnr))
			ret = NOTIFY_STOP;
		break;
	default:
		break;
	}
	return ret;
}

int __kprobes setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct jprobe *jp = container_of(p, struct jprobe, kp);
	unsigned long addr;
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();

	kcb->jprobe_saved_regs = *regs;
	kcb->jprobe_saved_sp = stack_addr(regs);
	addr = (unsigned long)(kcb->jprobe_saved_sp);

	memcpy(kcb->jprobes_stack, (kprobe_opcode_t *)addr,
	       MIN_STACK_SIZE(addr));
	regs->flags &= ~X86_EFLAGS_IF;
	trace_hardirqs_off();
	regs->ip = (unsigned long)(jp->entry);
	return 1;
}

void __kprobes jprobe_return(void)
{
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();

	asm volatile (
#ifdef CONFIG_X86_64
			"       xchg   %%rbx,%%rsp	\n"
#else
			"       xchgl   %%ebx,%%esp	\n"
#endif
			"       int3			\n"
			"       .globl jprobe_return_end\n"
			"       jprobe_return_end:	\n"
			"       nop			\n"::"b"
			(kcb->jprobe_saved_sp):"memory");
}

int __kprobes longjmp_break_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	u8 *addr = (u8 *) (regs->ip - 1);
	struct jprobe *jp = container_of(p, struct jprobe, kp);

	if ((addr > (u8 *) jprobe_return) &&
	    (addr < (u8 *) jprobe_return_end)) {
		if (stack_addr(regs) != kcb->jprobe_saved_sp) {
			struct pt_regs *saved_regs = &kcb->jprobe_saved_regs;
			printk(KERN_ERR
			       "current sp %p does not match saved sp %p\n",
			       stack_addr(regs), kcb->jprobe_saved_sp);
			printk(KERN_ERR "Saved registers for jprobe %p\n", jp);
			show_registers(saved_regs);
			printk(KERN_ERR "Current registers\n");
			show_registers(regs);
			BUG();
		}
		*regs = kcb->jprobe_saved_regs;
		memcpy((kprobe_opcode_t *)(kcb->jprobe_saved_sp),
		       kcb->jprobes_stack,
		       MIN_STACK_SIZE(kcb->jprobe_saved_sp));
		preempt_enable_no_resched();
		return 1;
	}
	return 0;
}

int __init arch_init_kprobes(void)
{
	return arch_init_optprobes();
}

int __kprobes arch_trampoline_kprobe(struct kprobe *p)
{
	return 0;
}
