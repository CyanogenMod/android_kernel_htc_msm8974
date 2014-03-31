/*
 *  Kernel Probes Jump Optimization (Optprobes)
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
 * Copyright (C) Hitachi Ltd., 2012
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

unsigned long __recover_optprobed_insn(kprobe_opcode_t *buf, unsigned long addr)
{
	struct optimized_kprobe *op;
	struct kprobe *kp;
	long offs;
	int i;

	for (i = 0; i < RELATIVEJUMP_SIZE; i++) {
		kp = get_kprobe((void *)addr - i);
		
		if (kp && kprobe_optimized(kp)) {
			op = container_of(kp, struct optimized_kprobe, kp);
			
			if (list_empty(&op->list))
				goto found;
		}
	}

	return addr;
found:
	/*
	 * If the kprobe can be optimized, original bytes which can be
	 * overwritten by jump destination address. In this case, original
	 * bytes must be recovered from op->optinsn.copied_insn buffer.
	 */
	memcpy(buf, (void *)addr, MAX_INSN_SIZE * sizeof(kprobe_opcode_t));
	if (addr == (unsigned long)kp->addr) {
		buf[0] = kp->opcode;
		memcpy(buf + 1, op->optinsn.copied_insn, RELATIVE_ADDR_SIZE);
	} else {
		offs = addr - (unsigned long)kp->addr - 1;
		memcpy(buf, op->optinsn.copied_insn + offs, RELATIVE_ADDR_SIZE - offs);
	}

	return (unsigned long)buf;
}

static void __kprobes synthesize_set_arg1(kprobe_opcode_t *addr, unsigned long val)
{
#ifdef CONFIG_X86_64
	*addr++ = 0x48;
	*addr++ = 0xbf;
#else
	*addr++ = 0xb8;
#endif
	*(unsigned long *)addr = val;
}

static void __used __kprobes kprobes_optinsn_template_holder(void)
{
	asm volatile (
			".global optprobe_template_entry\n"
			"optprobe_template_entry:\n"
#ifdef CONFIG_X86_64
			
			"	pushq %rsp\n"
			"	pushfq\n"
			SAVE_REGS_STRING
			"	movq %rsp, %rsi\n"
			".global optprobe_template_val\n"
			"optprobe_template_val:\n"
			ASM_NOP5
			ASM_NOP5
			".global optprobe_template_call\n"
			"optprobe_template_call:\n"
			ASM_NOP5
			
			"	movq 144(%rsp), %rdx\n"
			"	movq %rdx, 152(%rsp)\n"
			RESTORE_REGS_STRING
			
			"	addq $8, %rsp\n"
			"	popfq\n"
#else 
			"	pushf\n"
			SAVE_REGS_STRING
			"	movl %esp, %edx\n"
			".global optprobe_template_val\n"
			"optprobe_template_val:\n"
			ASM_NOP5
			".global optprobe_template_call\n"
			"optprobe_template_call:\n"
			ASM_NOP5
			RESTORE_REGS_STRING
			"	addl $4, %esp\n"	
			"	popf\n"
#endif
			".global optprobe_template_end\n"
			"optprobe_template_end:\n");
}

#define TMPL_MOVE_IDX \
	((long)&optprobe_template_val - (long)&optprobe_template_entry)
#define TMPL_CALL_IDX \
	((long)&optprobe_template_call - (long)&optprobe_template_entry)
#define TMPL_END_IDX \
	((long)&optprobe_template_end - (long)&optprobe_template_entry)

#define INT3_SIZE sizeof(kprobe_opcode_t)

static void __kprobes optimized_callback(struct optimized_kprobe *op, struct pt_regs *regs)
{
	struct kprobe_ctlblk *kcb = get_kprobe_ctlblk();
	unsigned long flags;

	
	if (kprobe_disabled(&op->kp))
		return;

	local_irq_save(flags);
	if (kprobe_running()) {
		kprobes_inc_nmissed_count(&op->kp);
	} else {
		
#ifdef CONFIG_X86_64
		regs->cs = __KERNEL_CS;
#else
		regs->cs = __KERNEL_CS | get_kernel_rpl();
		regs->gs = 0;
#endif
		regs->ip = (unsigned long)op->kp.addr + INT3_SIZE;
		regs->orig_ax = ~0UL;

		__this_cpu_write(current_kprobe, &op->kp);
		kcb->kprobe_status = KPROBE_HIT_ACTIVE;
		opt_pre_handler(&op->kp, regs);
		__this_cpu_write(current_kprobe, NULL);
	}
	local_irq_restore(flags);
}

static int __kprobes copy_optimized_instructions(u8 *dest, u8 *src)
{
	int len = 0, ret;

	while (len < RELATIVEJUMP_SIZE) {
		ret = __copy_instruction(dest + len, src + len);
		if (!ret || !can_boost(dest + len))
			return -EINVAL;
		len += ret;
	}
	
	if (ftrace_text_reserved(src, src + len - 1) ||
	    alternatives_text_reserved(src, src + len - 1) ||
	    jump_label_text_reserved(src, src + len - 1))
		return -EBUSY;

	return len;
}

static int __kprobes insn_is_indirect_jump(struct insn *insn)
{
	return ((insn->opcode.bytes[0] == 0xff &&
		(X86_MODRM_REG(insn->modrm.value) & 6) == 4) || 
		insn->opcode.bytes[0] == 0xea);	
}

static int insn_jump_into_range(struct insn *insn, unsigned long start, int len)
{
	unsigned long target = 0;

	switch (insn->opcode.bytes[0]) {
	case 0xe0:	
	case 0xe1:	
	case 0xe2:	
	case 0xe3:	
	case 0xe9:	
	case 0xeb:	
		break;
	case 0x0f:
		if ((insn->opcode.bytes[1] & 0xf0) == 0x80) 
			break;
		return 0;
	default:
		if ((insn->opcode.bytes[0] & 0xf0) == 0x70) 
			break;
		return 0;
	}
	target = (unsigned long)insn->next_byte + insn->immediate.value;

	return (start <= target && target <= start + len);
}

static int __kprobes can_optimize(unsigned long paddr)
{
	unsigned long addr, size = 0, offset = 0;
	struct insn insn;
	kprobe_opcode_t buf[MAX_INSN_SIZE];

	
	if (!kallsyms_lookup_size_offset(paddr, &size, &offset))
		return 0;

	if ((paddr >= (unsigned long)__entry_text_start) &&
	    (paddr <  (unsigned long)__entry_text_end))
		return 0;

	
	if (size - offset < RELATIVEJUMP_SIZE)
		return 0;

	
	addr = paddr - offset;
	while (addr < paddr - offset + size) { 
		if (search_exception_tables(addr))
			return 0;
		kernel_insn_init(&insn, (void *)recover_probed_instruction(buf, addr));
		insn_get_length(&insn);
		
		if (insn.opcode.bytes[0] == BREAKPOINT_INSTRUCTION)
			return 0;
		
		insn.kaddr = (void *)addr;
		insn.next_byte = (void *)(addr + insn.length);
		
		if (insn_is_indirect_jump(&insn) ||
		    insn_jump_into_range(&insn, paddr + INT3_SIZE,
					 RELATIVE_ADDR_SIZE))
			return 0;
		addr += insn.length;
	}

	return 1;
}

int __kprobes arch_check_optimized_kprobe(struct optimized_kprobe *op)
{
	int i;
	struct kprobe *p;

	for (i = 1; i < op->optinsn.size; i++) {
		p = get_kprobe(op->kp.addr + i);
		if (p && !kprobe_disabled(p))
			return -EEXIST;
	}

	return 0;
}

int __kprobes
arch_within_optimized_kprobe(struct optimized_kprobe *op, unsigned long addr)
{
	return ((unsigned long)op->kp.addr <= addr &&
		(unsigned long)op->kp.addr + op->optinsn.size > addr);
}

static __kprobes
void __arch_remove_optimized_kprobe(struct optimized_kprobe *op, int dirty)
{
	if (op->optinsn.insn) {
		free_optinsn_slot(op->optinsn.insn, dirty);
		op->optinsn.insn = NULL;
		op->optinsn.size = 0;
	}
}

void __kprobes arch_remove_optimized_kprobe(struct optimized_kprobe *op)
{
	__arch_remove_optimized_kprobe(op, 1);
}

int __kprobes arch_prepare_optimized_kprobe(struct optimized_kprobe *op)
{
	u8 *buf;
	int ret;
	long rel;

	if (!can_optimize((unsigned long)op->kp.addr))
		return -EILSEQ;

	op->optinsn.insn = get_optinsn_slot();
	if (!op->optinsn.insn)
		return -ENOMEM;

	rel = (long)op->optinsn.insn - (long)op->kp.addr + RELATIVEJUMP_SIZE;
	if (abs(rel) > 0x7fffffff)
		return -ERANGE;

	buf = (u8 *)op->optinsn.insn;

	
	ret = copy_optimized_instructions(buf + TMPL_END_IDX, op->kp.addr);
	if (ret < 0) {
		__arch_remove_optimized_kprobe(op, 0);
		return ret;
	}
	op->optinsn.size = ret;

	
	memcpy(buf, &optprobe_template_entry, TMPL_END_IDX);

	
	synthesize_set_arg1(buf + TMPL_MOVE_IDX, (unsigned long)op);

	
	synthesize_relcall(buf + TMPL_CALL_IDX, optimized_callback);

	
	synthesize_reljump(buf + TMPL_END_IDX + op->optinsn.size,
			   (u8 *)op->kp.addr + op->optinsn.size);

	flush_icache_range((unsigned long) buf,
			   (unsigned long) buf + TMPL_END_IDX +
			   op->optinsn.size + RELATIVEJUMP_SIZE);
	return 0;
}

#define MAX_OPTIMIZE_PROBES 256
static struct text_poke_param *jump_poke_params;
static struct jump_poke_buffer {
	u8 buf[RELATIVEJUMP_SIZE];
} *jump_poke_bufs;

static void __kprobes setup_optimize_kprobe(struct text_poke_param *tprm,
					    u8 *insn_buf,
					    struct optimized_kprobe *op)
{
	s32 rel = (s32)((long)op->optinsn.insn -
			((long)op->kp.addr + RELATIVEJUMP_SIZE));

	
	memcpy(op->optinsn.copied_insn, op->kp.addr + INT3_SIZE,
	       RELATIVE_ADDR_SIZE);

	insn_buf[0] = RELATIVEJUMP_OPCODE;
	*(s32 *)(&insn_buf[1]) = rel;

	tprm->addr = op->kp.addr;
	tprm->opcode = insn_buf;
	tprm->len = RELATIVEJUMP_SIZE;
}

void __kprobes arch_optimize_kprobes(struct list_head *oplist)
{
	struct optimized_kprobe *op, *tmp;
	int c = 0;

	list_for_each_entry_safe(op, tmp, oplist, list) {
		WARN_ON(kprobe_disabled(&op->kp));
		
		setup_optimize_kprobe(&jump_poke_params[c],
				      jump_poke_bufs[c].buf, op);
		list_del_init(&op->list);
		if (++c >= MAX_OPTIMIZE_PROBES)
			break;
	}

	text_poke_smp_batch(jump_poke_params, c);
}

static void __kprobes setup_unoptimize_kprobe(struct text_poke_param *tprm,
					      u8 *insn_buf,
					      struct optimized_kprobe *op)
{
	
	insn_buf[0] = BREAKPOINT_INSTRUCTION;
	memcpy(insn_buf + 1, op->optinsn.copied_insn, RELATIVE_ADDR_SIZE);

	tprm->addr = op->kp.addr;
	tprm->opcode = insn_buf;
	tprm->len = RELATIVEJUMP_SIZE;
}

extern void arch_unoptimize_kprobes(struct list_head *oplist,
				    struct list_head *done_list)
{
	struct optimized_kprobe *op, *tmp;
	int c = 0;

	list_for_each_entry_safe(op, tmp, oplist, list) {
		
		setup_unoptimize_kprobe(&jump_poke_params[c],
					jump_poke_bufs[c].buf, op);
		list_move(&op->list, done_list);
		if (++c >= MAX_OPTIMIZE_PROBES)
			break;
	}

	text_poke_smp_batch(jump_poke_params, c);
}

void __kprobes arch_unoptimize_kprobe(struct optimized_kprobe *op)
{
	u8 buf[RELATIVEJUMP_SIZE];

	
	buf[0] = BREAKPOINT_INSTRUCTION;
	memcpy(buf + 1, op->optinsn.copied_insn, RELATIVE_ADDR_SIZE);
	text_poke_smp(op->kp.addr, buf, RELATIVEJUMP_SIZE);
}

int  __kprobes
setup_detour_execution(struct kprobe *p, struct pt_regs *regs, int reenter)
{
	struct optimized_kprobe *op;

	if (p->flags & KPROBE_FLAG_OPTIMIZED) {
		
		op = container_of(p, struct optimized_kprobe, kp);
		
		regs->ip = (unsigned long)op->optinsn.insn + TMPL_END_IDX;
		if (!reenter)
			reset_current_kprobe();
		preempt_enable_no_resched();
		return 1;
	}
	return 0;
}

int __kprobes arch_init_optprobes(void)
{
	
	jump_poke_bufs = kmalloc(sizeof(struct jump_poke_buffer) *
				 MAX_OPTIMIZE_PROBES, GFP_KERNEL);
	if (!jump_poke_bufs)
		return -ENOMEM;

	jump_poke_params = kmalloc(sizeof(struct text_poke_param) *
				   MAX_OPTIMIZE_PROBES, GFP_KERNEL);
	if (!jump_poke_params) {
		kfree(jump_poke_bufs);
		jump_poke_bufs = NULL;
		return -ENOMEM;
	}

	return 0;
}
