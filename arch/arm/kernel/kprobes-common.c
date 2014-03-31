/*
 * arch/arm/kernel/kprobes-common.c
 *
 * Copyright (C) 2011 Jon Medhurst <tixy@yxit.co.uk>.
 *
 * Some contents moved here from arch/arm/include/asm/kprobes-arm.c which is
 * Copyright (C) 2006, 2007 Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <asm/system_info.h>

#include "kprobes.h"


#ifndef find_str_pc_offset


int str_pc_offset;

void __init find_str_pc_offset(void)
{
	int addr, scratch, ret;

	__asm__ (
		"sub	%[ret], pc, #4		\n\t"
		"str	pc, %[addr]		\n\t"
		"ldr	%[scr], %[addr]		\n\t"
		"sub	%[ret], %[scr], %[ret]	\n\t"
		: [ret] "=r" (ret), [scr] "=r" (scratch), [addr] "+m" (addr));

	str_pc_offset = ret;
}

#endif 


#ifndef test_load_write_pc_interworking

bool load_write_pc_interworks;

void __init test_load_write_pc_interworking(void)
{
	int arch = cpu_architecture();
	BUG_ON(arch == CPU_ARCH_UNKNOWN);
	load_write_pc_interworks = arch >= CPU_ARCH_ARMv5T;
}

#endif 


#ifndef test_alu_write_pc_interworking

bool alu_write_pc_interworks;

void __init test_alu_write_pc_interworking(void)
{
	int arch = cpu_architecture();
	BUG_ON(arch == CPU_ARCH_UNKNOWN);
	alu_write_pc_interworks = arch >= CPU_ARCH_ARMv7;
}

#endif 


void __init arm_kprobe_decode_init(void)
{
	find_str_pc_offset();
	test_load_write_pc_interworking();
	test_alu_write_pc_interworking();
}


static unsigned long __kprobes __check_eq(unsigned long cpsr)
{
	return cpsr & PSR_Z_BIT;
}

static unsigned long __kprobes __check_ne(unsigned long cpsr)
{
	return (~cpsr) & PSR_Z_BIT;
}

static unsigned long __kprobes __check_cs(unsigned long cpsr)
{
	return cpsr & PSR_C_BIT;
}

static unsigned long __kprobes __check_cc(unsigned long cpsr)
{
	return (~cpsr) & PSR_C_BIT;
}

static unsigned long __kprobes __check_mi(unsigned long cpsr)
{
	return cpsr & PSR_N_BIT;
}

static unsigned long __kprobes __check_pl(unsigned long cpsr)
{
	return (~cpsr) & PSR_N_BIT;
}

static unsigned long __kprobes __check_vs(unsigned long cpsr)
{
	return cpsr & PSR_V_BIT;
}

static unsigned long __kprobes __check_vc(unsigned long cpsr)
{
	return (~cpsr) & PSR_V_BIT;
}

static unsigned long __kprobes __check_hi(unsigned long cpsr)
{
	cpsr &= ~(cpsr >> 1); 
	return cpsr & PSR_C_BIT;
}

static unsigned long __kprobes __check_ls(unsigned long cpsr)
{
	cpsr &= ~(cpsr >> 1); 
	return (~cpsr) & PSR_C_BIT;
}

static unsigned long __kprobes __check_ge(unsigned long cpsr)
{
	cpsr ^= (cpsr << 3); 
	return (~cpsr) & PSR_N_BIT;
}

static unsigned long __kprobes __check_lt(unsigned long cpsr)
{
	cpsr ^= (cpsr << 3); 
	return cpsr & PSR_N_BIT;
}

static unsigned long __kprobes __check_gt(unsigned long cpsr)
{
	unsigned long temp = cpsr ^ (cpsr << 3); 
	temp |= (cpsr << 1);			 
	return (~temp) & PSR_N_BIT;
}

static unsigned long __kprobes __check_le(unsigned long cpsr)
{
	unsigned long temp = cpsr ^ (cpsr << 3); 
	temp |= (cpsr << 1);			 
	return temp & PSR_N_BIT;
}

static unsigned long __kprobes __check_al(unsigned long cpsr)
{
	return true;
}

kprobe_check_cc * const kprobe_condition_checks[16] = {
	&__check_eq, &__check_ne, &__check_cs, &__check_cc,
	&__check_mi, &__check_pl, &__check_vs, &__check_vc,
	&__check_hi, &__check_ls, &__check_ge, &__check_lt,
	&__check_gt, &__check_le, &__check_al, &__check_al
};


void __kprobes kprobe_simulate_nop(struct kprobe *p, struct pt_regs *regs)
{
}

void __kprobes kprobe_emulate_none(struct kprobe *p, struct pt_regs *regs)
{
	p->ainsn.insn_fn();
}

static void __kprobes simulate_ldm1stm1(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	int rn = (insn >> 16) & 0xf;
	int lbit = insn & (1 << 20);
	int wbit = insn & (1 << 21);
	int ubit = insn & (1 << 23);
	int pbit = insn & (1 << 24);
	long *addr = (long *)regs->uregs[rn];
	int reg_bit_vector;
	int reg_count;

	reg_count = 0;
	reg_bit_vector = insn & 0xffff;
	while (reg_bit_vector) {
		reg_bit_vector &= (reg_bit_vector - 1);
		++reg_count;
	}

	if (!ubit)
		addr -= reg_count;
	addr += (!pbit == !ubit);

	reg_bit_vector = insn & 0xffff;
	while (reg_bit_vector) {
		int reg = __ffs(reg_bit_vector);
		reg_bit_vector &= (reg_bit_vector - 1);
		if (lbit)
			regs->uregs[reg] = *addr++;
		else
			*addr++ = regs->uregs[reg];
	}

	if (wbit) {
		if (!ubit)
			addr -= reg_count;
		addr -= (!pbit == !ubit);
		regs->uregs[rn] = (long)addr;
	}
}

static void __kprobes simulate_stm1_pc(struct kprobe *p, struct pt_regs *regs)
{
	regs->ARM_pc = (long)p->addr + str_pc_offset;
	simulate_ldm1stm1(p, regs);
	regs->ARM_pc = (long)p->addr + 4;
}

static void __kprobes simulate_ldm1_pc(struct kprobe *p, struct pt_regs *regs)
{
	simulate_ldm1stm1(p, regs);
	load_write_pc(regs->ARM_pc, regs);
}

static void __kprobes
emulate_generic_r0_12_noflags(struct kprobe *p, struct pt_regs *regs)
{
	register void *rregs asm("r1") = regs;
	register void *rfn asm("lr") = p->ainsn.insn_fn;

	__asm__ __volatile__ (
		"stmdb	sp!, {%[regs], r11}	\n\t"
		"ldmia	%[regs], {r0-r12}	\n\t"
#if __LINUX_ARM_ARCH__ >= 6
		"blx	%[fn]			\n\t"
#else
		"str	%[fn], [sp, #-4]!	\n\t"
		"adr	lr, 1f			\n\t"
		"ldr	pc, [sp], #4		\n\t"
		"1:				\n\t"
#endif
		"ldr	lr, [sp], #4		\n\t" 
		"stmia	lr, {r0-r12}		\n\t"
		"ldr	r11, [sp], #4		\n\t"
		: [regs] "=r" (rregs), [fn] "=r" (rfn)
		: "0" (rregs), "1" (rfn)
		: "r0", "r2", "r3", "r4", "r5", "r6", "r7",
		  "r8", "r9", "r10", "r12", "memory", "cc"
		);
}

static void __kprobes
emulate_generic_r2_14_noflags(struct kprobe *p, struct pt_regs *regs)
{
	emulate_generic_r0_12_noflags(p, (struct pt_regs *)(regs->uregs+2));
}

static void __kprobes
emulate_ldm_r3_15(struct kprobe *p, struct pt_regs *regs)
{
	emulate_generic_r0_12_noflags(p, (struct pt_regs *)(regs->uregs+3));
	load_write_pc(regs->ARM_pc, regs);
}

enum kprobe_insn __kprobes
kprobe_decode_ldmstm(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	kprobe_insn_handler_t *handler = 0;
	unsigned reglist = insn & 0xffff;
	int is_ldm = insn & 0x100000;
	int rn = (insn >> 16) & 0xf;

	if (rn <= 12 && (reglist & 0xe000) == 0) {
		
		handler = emulate_generic_r0_12_noflags;

	} else if (rn >= 2 && (reglist & 0x8003) == 0) {
		
		rn -= 2;
		reglist >>= 2;
		handler = emulate_generic_r2_14_noflags;

	} else if (rn >= 3 && (reglist & 0x0007) == 0) {
		
		if (is_ldm && (reglist & 0x8000)) {
			rn -= 3;
			reglist >>= 3;
			handler = emulate_ldm_r3_15;
		}
	}

	if (handler) {
		
		asi->insn[0] = (insn & 0xfff00000) | (rn << 16) | reglist;
		asi->insn_handler = handler;
		return INSN_GOOD;
	}

	
	if (reglist & 0x8000)
		handler = is_ldm ? simulate_ldm1_pc : simulate_stm1_pc;
	else
		handler = simulate_ldm1stm1;
	asi->insn_handler = handler;
	return INSN_GOOD_NO_SLOT;
}


static kprobe_opcode_t __kprobes
prepare_emulated_insn(kprobe_opcode_t insn, struct arch_specific_insn *asi,
								bool thumb)
{
#ifdef CONFIG_THUMB2_KERNEL
	if (thumb) {
		u16 *thumb_insn = (u16 *)asi->insn;
		thumb_insn[1] = 0x4770; 
		thumb_insn[2] = 0x4770; 
		return insn;
	}
	asi->insn[1] = 0xe12fff1e; 
#else
	asi->insn[1] = 0xe1a0f00e; 
#endif
	
	if (insn < 0xe0000000)
		insn = (insn | 0xe0000000) & ~0x10000000;
	return insn;
}

static void  __kprobes
set_emulated_insn(kprobe_opcode_t insn, struct arch_specific_insn *asi,
								bool thumb)
{
#ifdef CONFIG_THUMB2_KERNEL
	if (thumb) {
		u16 *ip = (u16 *)asi->insn;
		if (is_wide_instruction(insn))
			*ip++ = insn >> 16;
		*ip++ = insn;
		return;
	}
#endif
	asi->insn[0] = insn;
}

#define INSN_NEW_BITS		0x00020103

#define INSN_SAMEAS16_BITS	0x22222222

static bool __kprobes decode_regs(kprobe_opcode_t* pinsn, u32 regs)
{
	kprobe_opcode_t insn = *pinsn;
	kprobe_opcode_t mask = 0xf; 

	for (; regs != 0; regs >>= 4, mask <<= 4) {

		kprobe_opcode_t new_bits = INSN_NEW_BITS;

		switch (regs & 0xf) {

		case REG_TYPE_NONE:
			
			continue;

		case REG_TYPE_ANY:
			
			break;

		case REG_TYPE_SAMEAS16:
			
			new_bits = INSN_SAMEAS16_BITS;
			break;

		case REG_TYPE_SP:
			
			if ((insn ^ 0xdddddddd) & mask)
				goto reject;
			break;

		case REG_TYPE_PC:
			
			if ((insn ^ 0xffffffff) & mask)
				goto reject;
			break;

		case REG_TYPE_NOSP:
			
			if (((insn ^ 0xdddddddd) & mask) == 0)
				goto reject;
			break;

		case REG_TYPE_NOSPPC:
		case REG_TYPE_NOSPPCX:
			
			if (((insn ^ 0xdddddddd) & 0xdddddddd & mask) == 0)
				goto reject;
			break;

		case REG_TYPE_NOPCWB:
			if (!is_writeback(insn))
				break; 
			
		case REG_TYPE_NOPC:
		case REG_TYPE_NOPCX:
			
			if (((insn ^ 0xffffffff) & mask) == 0)
				goto reject;
			break;
		}

		
		insn &= ~mask;
		insn |= new_bits & mask;
	}

	*pinsn = insn;
	return true;

reject:
	return false;
}

static const int decode_struct_sizes[NUM_DECODE_TYPES] = {
	[DECODE_TYPE_TABLE]	= sizeof(struct decode_table),
	[DECODE_TYPE_CUSTOM]	= sizeof(struct decode_custom),
	[DECODE_TYPE_SIMULATE]	= sizeof(struct decode_simulate),
	[DECODE_TYPE_EMULATE]	= sizeof(struct decode_emulate),
	[DECODE_TYPE_OR]	= sizeof(struct decode_or),
	[DECODE_TYPE_REJECT]	= sizeof(struct decode_reject)
};

int __kprobes
kprobe_decode_insn(kprobe_opcode_t insn, struct arch_specific_insn *asi,
				const union decode_item *table, bool thumb)
{
	const struct decode_header *h = (struct decode_header *)table;
	const struct decode_header *next;
	bool matched = false;

	insn = prepare_emulated_insn(insn, asi, thumb);

	for (;; h = next) {
		enum decode_type type = h->type_regs.bits & DECODE_TYPE_MASK;
		u32 regs = h->type_regs.bits >> DECODE_TYPE_BITS;

		if (type == DECODE_TYPE_END)
			return INSN_REJECTED;

		next = (struct decode_header *)
				((uintptr_t)h + decode_struct_sizes[type]);

		if (!matched && (insn & h->mask.bits) != h->value.bits)
			continue;

		if (!decode_regs(&insn, regs))
			return INSN_REJECTED;

		switch (type) {

		case DECODE_TYPE_TABLE: {
			struct decode_table *d = (struct decode_table *)h;
			next = (struct decode_header *)d->table.table;
			break;
		}

		case DECODE_TYPE_CUSTOM: {
			struct decode_custom *d = (struct decode_custom *)h;
			return (*d->decoder.decoder)(insn, asi);
		}

		case DECODE_TYPE_SIMULATE: {
			struct decode_simulate *d = (struct decode_simulate *)h;
			asi->insn_handler = d->handler.handler;
			return INSN_GOOD_NO_SLOT;
		}

		case DECODE_TYPE_EMULATE: {
			struct decode_emulate *d = (struct decode_emulate *)h;
			asi->insn_handler = d->handler.handler;
			set_emulated_insn(insn, asi, thumb);
			return INSN_GOOD;
		}

		case DECODE_TYPE_OR:
			matched = true;
			break;

		case DECODE_TYPE_REJECT:
		default:
			return INSN_REJECTED;
		}
		}
	}
