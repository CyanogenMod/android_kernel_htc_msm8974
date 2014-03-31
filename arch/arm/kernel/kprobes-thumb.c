/*
 * arch/arm/kernel/kprobes-thumb.c
 *
 * Copyright (C) 2011 Jon Medhurst <tixy@yxit.co.uk>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>

#include "kprobes.h"


#define in_it_block(cpsr)	((cpsr & 0x06000c00) != 0x00000000)

#define current_cond(cpsr)	((cpsr >> 12) & 0xf)

static inline unsigned long __kprobes thumb_probe_pc(struct kprobe *p)
{
	return (unsigned long)p->addr - 1 + 4;
}

static void __kprobes
t32_simulate_table_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;

	unsigned long rnv = (rn == 15) ? pc : regs->uregs[rn];
	unsigned long rmv = regs->uregs[rm];
	unsigned int halfwords;

	if (insn & 0x10) 
		halfwords = ((u16 *)rnv)[rmv];
	else 
		halfwords = ((u8 *)rnv)[rmv];

	regs->ARM_pc = pc + 2 * halfwords;
}

static void __kprobes
t32_simulate_mrs(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 8) & 0xf;
	unsigned long mask = 0xf8ff03df; 
	regs->uregs[rd] = regs->ARM_cpsr & mask;
}

static void __kprobes
t32_simulate_cond_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);

	long offset = insn & 0x7ff;		
	offset += (insn & 0x003f0000) >> 5;	
	offset += (insn & 0x00002000) << 4;	
	offset += (insn & 0x00000800) << 7;	
	offset -= (insn & 0x04000000) >> 7;	

	regs->ARM_pc = pc + (offset * 2);
}

static enum kprobe_insn __kprobes
t32_decode_cond_branch(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	int cc = (insn >> 22) & 0xf;
	asi->insn_check_cc = kprobe_condition_checks[cc];
	asi->insn_handler = t32_simulate_cond_branch;
	return INSN_GOOD_NO_SLOT;
}

static void __kprobes
t32_simulate_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);

	long offset = insn & 0x7ff;		
	offset += (insn & 0x03ff0000) >> 5;	
	offset += (insn & 0x00002000) << 9;	
	offset += (insn & 0x00000800) << 10;	
	if (insn & 0x04000000)
		offset -= 0x00800000; 
	else
		offset ^= 0x00600000; 

	if (insn & (1 << 14)) {
		
		regs->ARM_lr = (unsigned long)p->addr + 4;
		if (!(insn & (1 << 12))) {
			
			regs->ARM_cpsr &= ~PSR_T_BIT;
			pc &= ~3;
		}
	}

	regs->ARM_pc = pc + (offset * 2);
}

static void __kprobes
t32_simulate_ldr_literal(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long addr = thumb_probe_pc(p) & ~3;
	int rt = (insn >> 12) & 0xf;
	unsigned long rtv;

	long offset = insn & 0xfff;
	if (insn & 0x00800000)
		addr += offset;
	else
		addr -= offset;

	if (insn & 0x00400000) {
		
		rtv = *(unsigned long *)addr;
		if (rt == 15) {
			bx_write_pc(rtv, regs);
			return;
		}
	} else if (insn & 0x00200000) {
		
		if (insn & 0x01000000)
			rtv = *(s16 *)addr;
		else
			rtv = *(u16 *)addr;
	} else {
		
		if (insn & 0x01000000)
			rtv = *(s8 *)addr;
		else
			rtv = *(u8 *)addr;
	}

	regs->uregs[rt] = rtv;
}

static enum kprobe_insn __kprobes
t32_decode_ldmstm(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	enum kprobe_insn ret = kprobe_decode_ldmstm(insn, asi);

	
	insn = asi->insn[0];
	((u16 *)asi->insn)[0] = insn >> 16;
	((u16 *)asi->insn)[1] = insn & 0xffff;

	return ret;
}

static void __kprobes
t32_emulate_ldrdstrd(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p) & ~3;
	int rt1 = (insn >> 12) & 0xf;
	int rt2 = (insn >> 8) & 0xf;
	int rn = (insn >> 16) & 0xf;

	register unsigned long rt1v asm("r0") = regs->uregs[rt1];
	register unsigned long rt2v asm("r1") = regs->uregs[rt2];
	register unsigned long rnv asm("r2") = (rn == 15) ? pc
							  : regs->uregs[rn];

	__asm__ __volatile__ (
		"blx    %[fn]"
		: "=r" (rt1v), "=r" (rt2v), "=r" (rnv)
		: "0" (rt1v), "1" (rt2v), "2" (rnv), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	if (rn != 15)
		regs->uregs[rn] = rnv; 
	regs->uregs[rt1] = rt1v;
	regs->uregs[rt2] = rt2v;
}

static void __kprobes
t32_emulate_ldrstr(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rt = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;

	register unsigned long rtv asm("r0") = regs->uregs[rt];
	register unsigned long rnv asm("r2") = regs->uregs[rn];
	register unsigned long rmv asm("r3") = regs->uregs[rm];

	__asm__ __volatile__ (
		"blx    %[fn]"
		: "=r" (rtv), "=r" (rnv)
		: "0" (rtv), "1" (rnv), "r" (rmv), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	regs->uregs[rn] = rnv; 
	if (rt == 15) 
		bx_write_pc(rtv, regs);
	else
		regs->uregs[rt] = rtv;
}

static void __kprobes
t32_emulate_rd8rn16rm0_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 8) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;

	register unsigned long rdv asm("r1") = regs->uregs[rd];
	register unsigned long rnv asm("r2") = regs->uregs[rn];
	register unsigned long rmv asm("r3") = regs->uregs[rm];
	unsigned long cpsr = regs->ARM_cpsr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"blx    %[fn]			\n\t"
		"mrs	%[cpsr], cpsr		\n\t"
		: "=r" (rdv), [cpsr] "=r" (cpsr)
		: "0" (rdv), "r" (rnv), "r" (rmv),
		  "1" (cpsr), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	regs->uregs[rd] = rdv;
	regs->ARM_cpsr = (regs->ARM_cpsr & ~APSR_MASK) | (cpsr & APSR_MASK);
}

static void __kprobes
t32_emulate_rd8pc16_noflags(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	int rd = (insn >> 8) & 0xf;

	register unsigned long rdv asm("r1") = regs->uregs[rd];
	register unsigned long rnv asm("r2") = pc & ~3;

	__asm__ __volatile__ (
		"blx    %[fn]"
		: "=r" (rdv)
		: "0" (rdv), "r" (rnv), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	regs->uregs[rd] = rdv;
}

static void __kprobes
t32_emulate_rd8rn16_noflags(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 8) & 0xf;
	int rn = (insn >> 16) & 0xf;

	register unsigned long rdv asm("r1") = regs->uregs[rd];
	register unsigned long rnv asm("r2") = regs->uregs[rn];

	__asm__ __volatile__ (
		"blx    %[fn]"
		: "=r" (rdv)
		: "0" (rdv), "r" (rnv), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	regs->uregs[rd] = rdv;
}

static void __kprobes
t32_emulate_rdlo12rdhi8rn16rm0_noflags(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rdlo = (insn >> 12) & 0xf;
	int rdhi = (insn >> 8) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;

	register unsigned long rdlov asm("r0") = regs->uregs[rdlo];
	register unsigned long rdhiv asm("r1") = regs->uregs[rdhi];
	register unsigned long rnv asm("r2") = regs->uregs[rn];
	register unsigned long rmv asm("r3") = regs->uregs[rm];

	__asm__ __volatile__ (
		"blx    %[fn]"
		: "=r" (rdlov), "=r" (rdhiv)
		: "0" (rdlov), "1" (rdhiv), "r" (rnv), "r" (rmv),
		  [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	regs->uregs[rdlo] = rdlov;
	regs->uregs[rdhi] = rdhiv;
}

#define t32_emulate_rd8rn16rm0ra12_noflags \
		t32_emulate_rdlo12rdhi8rn16rm0_noflags

static const union decode_item t32_table_1110_100x_x0xx[] = {
	

	
	DECODE_REJECT	(0xfe4f0000, 0xe80f0000),

	
	
	DECODE_REJECT	(0xffc00000, 0xe8000000),
	
	
	DECODE_REJECT	(0xffc00000, 0xe9800000),

	
	DECODE_REJECT	(0xfe508000, 0xe8008000),
	
	DECODE_REJECT	(0xfe50c000, 0xe810c000),
	
	DECODE_REJECT	(0xfe402000, 0xe8002000),

	
	
	
	
	DECODE_CUSTOM	(0xfe400000, 0xe8000000, t32_decode_ldmstm),

	DECODE_END
};

static const union decode_item t32_table_1110_100x_x1xx[] = {
	

	
	
	DECODE_OR	(0xff600000, 0xe8600000),
	
	
	DECODE_EMULATEX	(0xff400000, 0xe9400000, t32_emulate_ldrdstrd,
						 REGS(NOPCWB, NOSPPC, NOSPPC, 0, 0)),

	
	
	DECODE_SIMULATEX(0xfff000e0, 0xe8d00000, t32_simulate_table_branch,
						 REGS(NOSP, 0, 0, 0, NOSPPC)),

	
	
	
	
	
	
	
	
	
	DECODE_END
};

static const union decode_item t32_table_1110_101x[] = {
	

	
	
	DECODE_EMULATEX	(0xff700f00, 0xea100f00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, 0, 0, NOSPPC)),

	
	DECODE_OR	(0xfff00f00, 0xeb100f00),
	
	DECODE_EMULATEX	(0xfff00f00, 0xebb00f00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOPC, 0, 0, 0, NOSPPC)),

	
	
	DECODE_EMULATEX	(0xffcf0000, 0xea4f0000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(0, 0, NOSPPC, 0, NOSPPC)),

	
	
	DECODE_REJECT	(0xffa00000, 0xeaa00000),
	
	DECODE_REJECT	(0xffe00000, 0xeb200000),
	
	DECODE_REJECT	(0xffe00000, 0xeb800000),
	
	DECODE_REJECT	(0xffe00000, 0xebe00000),

	
	
	DECODE_EMULATEX	(0xff4f7f30, 0xeb0d0d00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(SP, 0, SP, 0, NOSPPC)),

	
	
	DECODE_REJECT	(0xff4f0f00, 0xeb0d0d00),

	
	
	DECODE_EMULATEX	(0xff4f0000, 0xeb0d0000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(SP, 0, NOPC, 0, NOSPPC)),

	
	
	
	
	
	
	
	
	
	
	
	DECODE_EMULATEX	(0xfe000000, 0xea000000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, NOSPPC)),

	DECODE_END
};

static const union decode_item t32_table_1111_0x0x___0[] = {
	

	
	
	DECODE_EMULATEX	(0xfb708f00, 0xf0100f00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, 0, 0, 0)),

	
	DECODE_OR	(0xfbf08f00, 0xf1100f00),
	
	DECODE_EMULATEX	(0xfbf08f00, 0xf1b00f00, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOPC, 0, 0, 0, 0)),

	
	
	DECODE_EMULATEX	(0xfbcf8000, 0xf04f0000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(0, 0, NOSPPC, 0, 0)),

	
	DECODE_REJECT	(0xfbe08000, 0xf0a00000),
	
	
	DECODE_REJECT	(0xfbc08000, 0xf0c00000),
	
	DECODE_REJECT	(0xfbe08000, 0xf1200000),
	
	DECODE_REJECT	(0xfbe08000, 0xf1800000),
	
	DECODE_REJECT	(0xfbe08000, 0xf1e00000),

	
	
	DECODE_EMULATEX	(0xfb4f8000, 0xf10d0000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(SP, 0, NOPC, 0, 0)),

	
	
	
	
	
	
	
	
	
	
	DECODE_EMULATEX	(0xfa008000, 0xf0000000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, 0)),

	DECODE_END
};

static const union decode_item t32_table_1111_0x1x___0[] = {
	

	
	DECODE_OR	(0xfbff8000, 0xf20f0000),
	
	DECODE_EMULATEX	(0xfbff8000, 0xf2af0000, t32_emulate_rd8pc16_noflags,
						 REGS(PC, 0, NOSPPC, 0, 0)),

	
	DECODE_OR	(0xfbff8f00, 0xf20d0d00),
	
	DECODE_EMULATEX	(0xfbff8f00, 0xf2ad0d00, t32_emulate_rd8rn16_noflags,
						 REGS(SP, 0, SP, 0, 0)),

	
	DECODE_OR	(0xfbf08000, 0xf2000000),
	
	DECODE_EMULATEX	(0xfbf08000, 0xf2a00000, t32_emulate_rd8rn16_noflags,
						 REGS(NOPCX, 0, NOSPPC, 0, 0)),

	
	
	DECODE_EMULATEX	(0xfb708000, 0xf2400000, t32_emulate_rd8rn16_noflags,
						 REGS(0, 0, NOSPPC, 0, 0)),

	
	
	
	
	DECODE_EMULATEX	(0xfb508000, 0xf3000000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, 0)),

	
	
	DECODE_EMULATEX	(0xfb708000, 0xf3400000, t32_emulate_rd8rn16_noflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, 0)),

	
	DECODE_EMULATEX	(0xfbff8000, 0xf36f0000, t32_emulate_rd8rn16_noflags,
						 REGS(0, 0, NOSPPC, 0, 0)),

	
	DECODE_EMULATEX	(0xfbf08000, 0xf3600000, t32_emulate_rd8rn16_noflags,
						 REGS(NOSPPCX, 0, NOSPPC, 0, 0)),

	DECODE_END
};

static const union decode_item t32_table_1111_0xxx___1[] = {
	

	
	DECODE_OR	(0xfff0d7ff, 0xf3a08001),
	
	DECODE_EMULATE	(0xfff0d7ff, 0xf3a08004, kprobe_emulate_none),
	
	
	
	DECODE_SIMULATE	(0xfff0d7fc, 0xf3a08000, kprobe_simulate_nop),

	
	DECODE_SIMULATEX(0xfff0d000, 0xf3e08000, t32_simulate_mrs,
						 REGS(0, 0, NOSPPC, 0, 0)),

	DECODE_REJECT	(0xfb80d000, 0xf3808000),

	
	DECODE_CUSTOM	(0xf800d000, 0xf0008000, t32_decode_cond_branch),

	
	DECODE_OR	(0xf800d001, 0xf000c000),
	
	
	DECODE_SIMULATE	(0xf8009000, 0xf0009000, t32_simulate_branch),

	DECODE_END
};

static const union decode_item t32_table_1111_100x_x0x1__1111[] = {
	

	
	
	DECODE_SIMULATE	(0xfe7ff000, 0xf81ff000, kprobe_simulate_nop),

	
	DECODE_OR	(0xffd0f000, 0xf890f000),
	
	DECODE_OR	(0xffd0ff00, 0xf810fc00),
	
	DECODE_OR	(0xfff0f000, 0xf990f000),
	
	DECODE_SIMULATEX(0xfff0ff00, 0xf910fc00, kprobe_simulate_nop,
						 REGS(NOPCX, 0, 0, 0, 0)),

	
	DECODE_OR	(0xffd0ffc0, 0xf810f000),
	
	DECODE_SIMULATEX(0xfff0ffc0, 0xf910f000, kprobe_simulate_nop,
						 REGS(NOPCX, 0, 0, 0, NOSPPC)),

	
	DECODE_END
};

static const union decode_item t32_table_1111_100x[] = {
	

	
	DECODE_REJECT	(0xfe600000, 0xf8600000),

	
	DECODE_REJECT	(0xfff00000, 0xf9500000),

	
	DECODE_REJECT	(0xfe800d00, 0xf8000800),

	
	
	
	
	
	
	
	
	DECODE_REJECT	(0xfe800f00, 0xf8000e00),

	
	DECODE_REJECT	(0xff1f0000, 0xf80f0000),

	
	DECODE_REJECT	(0xff10f000, 0xf800f000),

	
	DECODE_SIMULATEX(0xff7f0000, 0xf85f0000, t32_simulate_ldr_literal,
						 REGS(PC, ANY, 0, 0, 0)),

	
	
	DECODE_OR	(0xffe00800, 0xf8400800),
	
	
	DECODE_EMULATEX	(0xffe00000, 0xf8c00000, t32_emulate_ldrstr,
						 REGS(NOPCX, ANY, 0, 0, 0)),

	
	
	DECODE_EMULATEX	(0xffe00fc0, 0xf8400000, t32_emulate_ldrstr,
						 REGS(NOPCX, ANY, 0, 0, NOSPPC)),

	
	
	
	
	DECODE_EMULATEX	(0xfe5f0000, 0xf81f0000, t32_simulate_ldr_literal,
						 REGS(PC, NOSPPCX, 0, 0, 0)),

	
	
	
	
	
	
	DECODE_OR	(0xfec00800, 0xf8000800),
	
	
	
	
	
	
	DECODE_EMULATEX	(0xfec00000, 0xf8800000, t32_emulate_ldrstr,
						 REGS(NOPCX, NOSPPCX, 0, 0, 0)),

	
	
	
	
	
	
	DECODE_EMULATEX	(0xfe800fc0, 0xf8000000, t32_emulate_ldrstr,
						 REGS(NOPCX, NOSPPCX, 0, 0, NOSPPC)),

	
	DECODE_END
};

static const union decode_item t32_table_1111_1010___1111[] = {
	

	
	DECODE_REJECT	(0xffe0f080, 0xfa60f080),

	
	
	
	
	
	
	DECODE_EMULATEX	(0xff8ff080, 0xfa0ff080, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(0, 0, NOSPPC, 0, NOSPPC)),


	
	DECODE_REJECT	(0xff80f0b0, 0xfa80f030),
	
	DECODE_REJECT	(0xffb0f080, 0xfab0f000),

	
	
	
	
	
	

	
	
	
	
	
	

	
	
	
	
	
	

	
	
	
	
	
	

	
	
	
	
	
	

	
	
	
	
	
	
	DECODE_OR	(0xff80f080, 0xfa80f000),

	
	
	
	
	
	
	DECODE_OR	(0xff80f080, 0xfa00f080),

	
	
	
	
	DECODE_OR	(0xfff0f0c0, 0xfa80f080),

	
	DECODE_OR	(0xfff0f0f0, 0xfaa0f080),

	
	
	
	
	DECODE_EMULATEX	(0xff80f0f0, 0xfa00f000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, NOSPPC)),

	
	DECODE_OR	(0xfff0f0f0, 0xfab0f080),

	
	
	
	
	DECODE_EMULATEX	(0xfff0f0c0, 0xfa90f080, t32_emulate_rd8rn16_noflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, SAMEAS16)),

	
	DECODE_END
};

static const union decode_item t32_table_1111_1011_0[] = {
	

	
	DECODE_REJECT	(0xfff0f0f0, 0xfb00f010),
	
	DECODE_REJECT	(0xfff0f0f0, 0xfb70f010),

	
	DECODE_OR	(0xfff0f0c0, 0xfb10f000),
	
	
	
	
	
	
	DECODE_EMULATEX	(0xff80f0e0, 0xfb00f000, t32_emulate_rd8rn16rm0_rwflags,
						 REGS(NOSPPC, 0, NOSPPC, 0, NOSPPC)),

	
	DECODE_REJECT	(0xfff000f0, 0xfb700010),

	
	DECODE_OR	(0xfff000c0, 0xfb100000),
	
	
	
	
	
	
	
	
	DECODE_EMULATEX	(0xff8000c0, 0xfb000000, t32_emulate_rd8rn16rm0ra12_noflags,
						 REGS(NOSPPC, NOSPPCX, NOSPPC, 0, NOSPPC)),

	
	DECODE_END
};

static const union decode_item t32_table_1111_1011_1[] = {
	

	
	DECODE_OR	(0xfff000f0, 0xfbe00060),
	
	DECODE_OR	(0xfff000c0, 0xfbc00080),
	
	
	DECODE_OR	(0xffe000e0, 0xfbc000c0),
	
	
	
	
	DECODE_EMULATEX	(0xff9000f0, 0xfb800000, t32_emulate_rdlo12rdhi8rn16rm0_noflags,
						 REGS(NOSPPC, NOSPPC, NOSPPC, 0, NOSPPC)),

	
	
	
	DECODE_END
};

const union decode_item kprobe_decode_thumb32_table[] = {

	DECODE_TABLE	(0xfe400000, 0xe8000000, t32_table_1110_100x_x0xx),

	DECODE_TABLE	(0xfe400000, 0xe8400000, t32_table_1110_100x_x1xx),

	DECODE_TABLE	(0xfe000000, 0xea000000, t32_table_1110_101x),

	DECODE_REJECT	(0xfc000000, 0xec000000),

	DECODE_TABLE	(0xfa008000, 0xf0000000, t32_table_1111_0x0x___0),

	DECODE_TABLE	(0xfa008000, 0xf2000000, t32_table_1111_0x1x___0),

	DECODE_TABLE	(0xf8008000, 0xf0008000, t32_table_1111_0xxx___1),

	DECODE_REJECT	(0xff100000, 0xf9000000),

	DECODE_TABLE	(0xfe50f000, 0xf810f000, t32_table_1111_100x_x0x1__1111),

	DECODE_TABLE	(0xfe000000, 0xf8000000, t32_table_1111_100x),

	DECODE_TABLE	(0xff00f000, 0xfa00f000, t32_table_1111_1010___1111),

	DECODE_TABLE	(0xff800000, 0xfb000000, t32_table_1111_1011_0),

	DECODE_TABLE	(0xff800000, 0xfb800000, t32_table_1111_1011_1),

	DECODE_END
};
#ifdef CONFIG_ARM_KPROBES_TEST_MODULE
EXPORT_SYMBOL_GPL(kprobe_decode_thumb32_table);
#endif

static void __kprobes
t16_simulate_bxblx(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	int rm = (insn >> 3) & 0xf;
	unsigned long rmv = (rm == 15) ? pc : regs->uregs[rm];

	if (insn & (1 << 7)) 
		regs->ARM_lr = (unsigned long)p->addr + 2;

	bx_write_pc(rmv, regs);
}

static void __kprobes
t16_simulate_ldr_literal(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long* base = (unsigned long *)(thumb_probe_pc(p) & ~3);
	long index = insn & 0xff;
	int rt = (insn >> 8) & 0x7;
	regs->uregs[rt] = base[index];
}

static void __kprobes
t16_simulate_ldrstr_sp_relative(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long* base = (unsigned long *)regs->ARM_sp;
	long index = insn & 0xff;
	int rt = (insn >> 8) & 0x7;
	if (insn & 0x800) 
		regs->uregs[rt] = base[index];
	else 
		base[index] = regs->uregs[rt];
}

static void __kprobes
t16_simulate_reladr(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long base = (insn & 0x800) ? regs->ARM_sp
					    : (thumb_probe_pc(p) & ~3);
	long offset = insn & 0xff;
	int rt = (insn >> 8) & 0x7;
	regs->uregs[rt] = base + offset * 4;
}

static void __kprobes
t16_simulate_add_sp_imm(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	long imm = insn & 0x7f;
	if (insn & 0x80) 
		regs->ARM_sp -= imm * 4;
	else 
		regs->ARM_sp += imm * 4;
}

static void __kprobes
t16_simulate_cbz(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rn = insn & 0x7;
	kprobe_opcode_t nonzero = regs->uregs[rn] ? insn : ~insn;
	if (nonzero & 0x800) {
		long i = insn & 0x200;
		long imm5 = insn & 0xf8;
		unsigned long pc = thumb_probe_pc(p);
		regs->ARM_pc = pc + (i >> 3) + (imm5 >> 2);
	}
}

static void __kprobes
t16_simulate_it(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long cpsr = regs->ARM_cpsr;
	cpsr &= ~PSR_IT_MASK;
	cpsr |= (insn & 0xfc) << 8;
	cpsr |= (insn & 0x03) << 25;
	regs->ARM_cpsr = cpsr;
}

static void __kprobes
t16_singlestep_it(struct kprobe *p, struct pt_regs *regs)
{
	regs->ARM_pc += 2;
	t16_simulate_it(p, regs);
}

static enum kprobe_insn __kprobes
t16_decode_it(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	asi->insn_singlestep = t16_singlestep_it;
	return INSN_GOOD_NO_SLOT;
}

static void __kprobes
t16_simulate_cond_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	long offset = insn & 0x7f;
	offset -= insn & 0x80; 
	regs->ARM_pc = pc + (offset * 2);
}

static enum kprobe_insn __kprobes
t16_decode_cond_branch(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	int cc = (insn >> 8) & 0xf;
	asi->insn_check_cc = kprobe_condition_checks[cc];
	asi->insn_handler = t16_simulate_cond_branch;
	return INSN_GOOD_NO_SLOT;
}

static void __kprobes
t16_simulate_branch(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	long offset = insn & 0x3ff;
	offset -= insn & 0x400; 
	regs->ARM_pc = pc + (offset * 2);
}

static unsigned long __kprobes
t16_emulate_loregs(struct kprobe *p, struct pt_regs *regs)
{
	unsigned long oldcpsr = regs->ARM_cpsr;
	unsigned long newcpsr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[oldcpsr]	\n\t"
		"ldmia	%[regs], {r0-r7}	\n\t"
		"blx	%[fn]			\n\t"
		"stmia	%[regs], {r0-r7}	\n\t"
		"mrs	%[newcpsr], cpsr	\n\t"
		: [newcpsr] "=r" (newcpsr)
		: [oldcpsr] "r" (oldcpsr), [regs] "r" (regs),
		  [fn] "r" (p->ainsn.insn_fn)
		: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
		  "lr", "memory", "cc"
		);

	return (oldcpsr & ~APSR_MASK) | (newcpsr & APSR_MASK);
}

static void __kprobes
t16_emulate_loregs_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	regs->ARM_cpsr = t16_emulate_loregs(p, regs);
}

static void __kprobes
t16_emulate_loregs_noitrwflags(struct kprobe *p, struct pt_regs *regs)
{
	unsigned long cpsr = t16_emulate_loregs(p, regs);
	if (!in_it_block(cpsr))
		regs->ARM_cpsr = cpsr;
}

static void __kprobes
t16_emulate_hiregs(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	unsigned long pc = thumb_probe_pc(p);
	int rdn = (insn & 0x7) | ((insn & 0x80) >> 4);
	int rm = (insn >> 3) & 0xf;

	register unsigned long rdnv asm("r1");
	register unsigned long rmv asm("r0");
	unsigned long cpsr = regs->ARM_cpsr;

	rdnv = (rdn == 15) ? pc : regs->uregs[rdn];
	rmv = (rm == 15) ? pc : regs->uregs[rm];

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"blx    %[fn]			\n\t"
		"mrs	%[cpsr], cpsr		\n\t"
		: "=r" (rdnv), [cpsr] "=r" (cpsr)
		: "0" (rdnv), "r" (rmv), "1" (cpsr), [fn] "r" (p->ainsn.insn_fn)
		: "lr", "memory", "cc"
	);

	if (rdn == 15)
		rdnv &= ~1;

	regs->uregs[rdn] = rdnv;
	regs->ARM_cpsr = (regs->ARM_cpsr & ~APSR_MASK) | (cpsr & APSR_MASK);
}

static enum kprobe_insn __kprobes
t16_decode_hiregs(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	insn &= ~0x00ff;
	insn |= 0x001; 
	((u16 *)asi->insn)[0] = insn;
	asi->insn_handler = t16_emulate_hiregs;
	return INSN_GOOD;
}

static void __kprobes
t16_emulate_push(struct kprobe *p, struct pt_regs *regs)
{
	__asm__ __volatile__ (
		"ldr	r9, [%[regs], #13*4]	\n\t"
		"ldr	r8, [%[regs], #14*4]	\n\t"
		"ldmia	%[regs], {r0-r7}	\n\t"
		"blx	%[fn]			\n\t"
		"str	r9, [%[regs], #13*4]	\n\t"
		:
		: [regs] "r" (regs), [fn] "r" (p->ainsn.insn_fn)
		: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9",
		  "lr", "memory", "cc"
		);
}

static enum kprobe_insn __kprobes
t16_decode_push(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	((u16 *)asi->insn)[0] = 0xe929;		
	((u16 *)asi->insn)[1] = insn & 0x1ff;	
	asi->insn_handler = t16_emulate_push;
	return INSN_GOOD;
}

static void __kprobes
t16_emulate_pop_nopc(struct kprobe *p, struct pt_regs *regs)
{
	__asm__ __volatile__ (
		"ldr	r9, [%[regs], #13*4]	\n\t"
		"ldmia	%[regs], {r0-r7}	\n\t"
		"blx	%[fn]			\n\t"
		"stmia	%[regs], {r0-r7}	\n\t"
		"str	r9, [%[regs], #13*4]	\n\t"
		:
		: [regs] "r" (regs), [fn] "r" (p->ainsn.insn_fn)
		: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r9",
		  "lr", "memory", "cc"
		);
}

static void __kprobes
t16_emulate_pop_pc(struct kprobe *p, struct pt_regs *regs)
{
	register unsigned long pc asm("r8");

	__asm__ __volatile__ (
		"ldr	r9, [%[regs], #13*4]	\n\t"
		"ldmia	%[regs], {r0-r7}	\n\t"
		"blx	%[fn]			\n\t"
		"stmia	%[regs], {r0-r7}	\n\t"
		"str	r9, [%[regs], #13*4]	\n\t"
		: "=r" (pc)
		: [regs] "r" (regs), [fn] "r" (p->ainsn.insn_fn)
		: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r9",
		  "lr", "memory", "cc"
		);

	bx_write_pc(pc, regs);
}

static enum kprobe_insn __kprobes
t16_decode_pop(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	((u16 *)asi->insn)[0] = 0xe8b9;		
	((u16 *)asi->insn)[1] = insn & 0x1ff;	
	asi->insn_handler = insn & 0x100 ? t16_emulate_pop_pc
					 : t16_emulate_pop_nopc;
	return INSN_GOOD;
}

static const union decode_item t16_table_1011[] = {
	

	
	
	DECODE_SIMULATE	(0xff00, 0xb000, t16_simulate_add_sp_imm),

	
	
	DECODE_SIMULATE	(0xf500, 0xb100, t16_simulate_cbz),

	
	
	
	
	
	
	
	
	DECODE_REJECT	(0xffc0, 0xba80),
	DECODE_EMULATE	(0xf500, 0xb000, t16_emulate_loregs_rwflags),

	
	DECODE_CUSTOM	(0xfe00, 0xb400, t16_decode_push),
	
	DECODE_CUSTOM	(0xfe00, 0xbc00, t16_decode_pop),


	
	DECODE_OR	(0xffff, 0xbf10),
	
	DECODE_EMULATE	(0xffff, 0xbf40, kprobe_emulate_none),
	
	
	
	DECODE_SIMULATE	(0xffcf, 0xbf00, kprobe_simulate_nop),
	
	DECODE_REJECT	(0xff0f, 0xbf00),
	
	DECODE_CUSTOM	(0xff00, 0xbf00, t16_decode_it),

	
	
	
	
	DECODE_END
};

const union decode_item kprobe_decode_thumb16_table[] = {


	
	DECODE_EMULATE	(0xf800, 0x2800, t16_emulate_loregs_rwflags),

	
	
	
	
	
	
	
	
	
	
	DECODE_EMULATE	(0xc000, 0x0000, t16_emulate_loregs_noitrwflags),


	
	DECODE_EMULATE	(0xffc0, 0x4200, t16_emulate_loregs_rwflags),
	
	
	DECODE_EMULATE	(0xff80, 0x4280, t16_emulate_loregs_rwflags),
	
	
	
	
	
	
	
	
	
	
	
	
	
	DECODE_EMULATE	(0xfc00, 0x4000, t16_emulate_loregs_noitrwflags),


	
	DECODE_REJECT	(0xfff8, 0x47f8),

	
	
	DECODE_SIMULATE (0xff00, 0x4700, t16_simulate_bxblx),

	
	DECODE_REJECT	(0xffff, 0x44ff),

	
	
	
	DECODE_CUSTOM	(0xfc00, 0x4400, t16_decode_hiregs),

	DECODE_SIMULATE	(0xf800, 0x4800, t16_simulate_ldr_literal),


	
	
	
	
	
	
	
	
	
	
	
	
	DECODE_EMULATE	(0xc000, 0x4000, t16_emulate_loregs_rwflags),
	
	
	DECODE_EMULATE	(0xf000, 0x8000, t16_emulate_loregs_rwflags),
	
	
	DECODE_SIMULATE	(0xf000, 0x9000, t16_simulate_ldrstr_sp_relative),

	DECODE_SIMULATE	(0xf000, 0xa000, t16_simulate_reladr),

	DECODE_TABLE	(0xf000, 0xb000, t16_table_1011),

	
	
	DECODE_EMULATE	(0xf000, 0xc000, t16_emulate_loregs_rwflags),


	
	
	DECODE_REJECT	(0xfe00, 0xde00),

	
	DECODE_CUSTOM	(0xf000, 0xd000, t16_decode_cond_branch),

	DECODE_SIMULATE	(0xf800, 0xe000, t16_simulate_branch),

	DECODE_END
};
#ifdef CONFIG_ARM_KPROBES_TEST_MODULE
EXPORT_SYMBOL_GPL(kprobe_decode_thumb16_table);
#endif

static unsigned long __kprobes thumb_check_cc(unsigned long cpsr)
{
	if (unlikely(in_it_block(cpsr)))
		return kprobe_condition_checks[current_cond(cpsr)](cpsr);
	return true;
}

static void __kprobes thumb16_singlestep(struct kprobe *p, struct pt_regs *regs)
{
	regs->ARM_pc += 2;
	p->ainsn.insn_handler(p, regs);
	regs->ARM_cpsr = it_advance(regs->ARM_cpsr);
}

static void __kprobes thumb32_singlestep(struct kprobe *p, struct pt_regs *regs)
{
	regs->ARM_pc += 4;
	p->ainsn.insn_handler(p, regs);
	regs->ARM_cpsr = it_advance(regs->ARM_cpsr);
}

enum kprobe_insn __kprobes
thumb16_kprobe_decode_insn(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	asi->insn_singlestep = thumb16_singlestep;
	asi->insn_check_cc = thumb_check_cc;
	return kprobe_decode_insn(insn, asi, kprobe_decode_thumb16_table, true);
}

enum kprobe_insn __kprobes
thumb32_kprobe_decode_insn(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	asi->insn_singlestep = thumb32_singlestep;
	asi->insn_check_cc = thumb_check_cc;
	return kprobe_decode_insn(insn, asi, kprobe_decode_thumb32_table, true);
}
