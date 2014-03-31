/*
 * arch/arm/kernel/kprobes.h
 *
 * Copyright (C) 2011 Jon Medhurst <tixy@yxit.co.uk>.
 *
 * Some contents moved here from arch/arm/include/asm/kprobes.h which is
 * Copyright (C) 2006, 2007 Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _ARM_KERNEL_KPROBES_H
#define _ARM_KERNEL_KPROBES_H

#define KPROBE_ARM_BREAKPOINT_INSTRUCTION	0x07f001f8
#define KPROBE_THUMB16_BREAKPOINT_INSTRUCTION	0xde18
#define KPROBE_THUMB32_BREAKPOINT_INSTRUCTION	0xf7f0a018


enum kprobe_insn {
	INSN_REJECTED,
	INSN_GOOD,
	INSN_GOOD_NO_SLOT
};

typedef enum kprobe_insn (kprobe_decode_insn_t)(kprobe_opcode_t,
						struct arch_specific_insn *);

#ifdef CONFIG_THUMB2_KERNEL

enum kprobe_insn thumb16_kprobe_decode_insn(kprobe_opcode_t,
						struct arch_specific_insn *);
enum kprobe_insn thumb32_kprobe_decode_insn(kprobe_opcode_t,
						struct arch_specific_insn *);

#else 

enum kprobe_insn arm_kprobe_decode_insn(kprobe_opcode_t,
					struct arch_specific_insn *);
#endif

void __init arm_kprobe_decode_init(void);

extern kprobe_check_cc * const kprobe_condition_checks[16];


#if __LINUX_ARM_ARCH__ >= 7

#define str_pc_offset 8
#define find_str_pc_offset()

#else 

extern int str_pc_offset;
void __init find_str_pc_offset(void);

#endif


static inline unsigned long it_advance(unsigned long cpsr)
	{
	if ((cpsr & 0x06000400) == 0) {
		
		cpsr &= ~PSR_IT_MASK;
	} else {
		
		const unsigned long mask = 0x06001c00;  
		unsigned long it = cpsr & mask;
		it <<= 1;
		it |= it >> (27 - 10);  
		it &= mask;
		cpsr &= ~mask;
		cpsr |= it;
	}
	return cpsr;
}

static inline void __kprobes bx_write_pc(long pcv, struct pt_regs *regs)
{
	long cpsr = regs->ARM_cpsr;
	if (pcv & 0x1) {
		cpsr |= PSR_T_BIT;
		pcv &= ~0x1;
	} else {
		cpsr &= ~PSR_T_BIT;
		pcv &= ~0x2;	
	}
	regs->ARM_cpsr = cpsr;
	regs->ARM_pc = pcv;
}


#if __LINUX_ARM_ARCH__ >= 6

#define load_write_pc_interworks true
#define test_load_write_pc_interworking()

#else 

extern bool load_write_pc_interworks;
void __init test_load_write_pc_interworking(void);

#endif

static inline void __kprobes load_write_pc(long pcv, struct pt_regs *regs)
{
	if (load_write_pc_interworks)
		bx_write_pc(pcv, regs);
	else
		regs->ARM_pc = pcv;
}


#if __LINUX_ARM_ARCH__ >= 7

#define alu_write_pc_interworks true
#define test_alu_write_pc_interworking()

#elif __LINUX_ARM_ARCH__ <= 5

#define alu_write_pc_interworks false
#define test_alu_write_pc_interworking()

#else 

extern bool alu_write_pc_interworks;
void __init test_alu_write_pc_interworking(void);

#endif 

static inline void __kprobes alu_write_pc(long pcv, struct pt_regs *regs)
{
	if (alu_write_pc_interworks)
		bx_write_pc(pcv, regs);
	else
		regs->ARM_pc = pcv;
}


void __kprobes kprobe_simulate_nop(struct kprobe *p, struct pt_regs *regs);
void __kprobes kprobe_emulate_none(struct kprobe *p, struct pt_regs *regs);

enum kprobe_insn __kprobes
kprobe_decode_ldmstm(kprobe_opcode_t insn, struct arch_specific_insn *asi);

#define is_writeback(insn) ((insn ^ 0x01000000) & 0x01200000)


enum decode_type {
	DECODE_TYPE_END,
	DECODE_TYPE_TABLE,
	DECODE_TYPE_CUSTOM,
	DECODE_TYPE_SIMULATE,
	DECODE_TYPE_EMULATE,
	DECODE_TYPE_OR,
	DECODE_TYPE_REJECT,
	NUM_DECODE_TYPES 
};

#define DECODE_TYPE_BITS	4
#define DECODE_TYPE_MASK	((1 << DECODE_TYPE_BITS) - 1)

enum decode_reg_type {
	REG_TYPE_NONE = 0, 
	REG_TYPE_ANY,	   
	REG_TYPE_SAMEAS16, 
	REG_TYPE_SP,	   
	REG_TYPE_PC,	   
	REG_TYPE_NOSP,	   
	REG_TYPE_NOSPPC,   
	REG_TYPE_NOPC,	   
	REG_TYPE_NOPCWB,   

	REG_TYPE_NOPCX,	   
	REG_TYPE_NOSPPCX,  

	
	REG_TYPE_0 = REG_TYPE_NONE
};

#define REGS(r16, r12, r8, r4, r0)	\
	((REG_TYPE_##r16) << 16) +	\
	((REG_TYPE_##r12) << 12) +	\
	((REG_TYPE_##r8) << 8) +	\
	((REG_TYPE_##r4) << 4) +	\
	(REG_TYPE_##r0)

union decode_item {
	u32			bits;
	const union decode_item	*table;
	kprobe_insn_handler_t	*handler;
	kprobe_decode_insn_t	*decoder;
};


#define DECODE_END			\
	{.bits = DECODE_TYPE_END}


struct decode_header {
	union decode_item	type_regs;
	union decode_item	mask;
	union decode_item	value;
};

#define DECODE_HEADER(_type, _mask, _value, _regs)		\
	{.bits = (_type) | ((_regs) << DECODE_TYPE_BITS)},	\
	{.bits = (_mask)},					\
	{.bits = (_value)}


struct decode_table {
	struct decode_header	header;
	union decode_item	table;
};

#define DECODE_TABLE(_mask, _value, _table)			\
	DECODE_HEADER(DECODE_TYPE_TABLE, _mask, _value, 0),	\
	{.table = (_table)}


struct decode_custom {
	struct decode_header	header;
	union decode_item	decoder;
};

#define DECODE_CUSTOM(_mask, _value, _decoder)			\
	DECODE_HEADER(DECODE_TYPE_CUSTOM, _mask, _value, 0),	\
	{.decoder = (_decoder)}


struct decode_simulate {
	struct decode_header	header;
	union decode_item	handler;
};

#define DECODE_SIMULATEX(_mask, _value, _handler, _regs)		\
	DECODE_HEADER(DECODE_TYPE_SIMULATE, _mask, _value, _regs),	\
	{.handler = (_handler)}

#define DECODE_SIMULATE(_mask, _value, _handler)	\
	DECODE_SIMULATEX(_mask, _value, _handler, 0)


struct decode_emulate {
	struct decode_header	header;
	union decode_item	handler;
};

#define DECODE_EMULATEX(_mask, _value, _handler, _regs)			\
	DECODE_HEADER(DECODE_TYPE_EMULATE, _mask, _value, _regs),	\
	{.handler = (_handler)}

#define DECODE_EMULATE(_mask, _value, _handler)		\
	DECODE_EMULATEX(_mask, _value, _handler, 0)


struct decode_or {
	struct decode_header	header;
};

#define DECODE_OR(_mask, _value)				\
	DECODE_HEADER(DECODE_TYPE_OR, _mask, _value, 0)


struct decode_reject {
	struct decode_header	header;
};

#define DECODE_REJECT(_mask, _value)				\
	DECODE_HEADER(DECODE_TYPE_REJECT, _mask, _value, 0)


#ifdef CONFIG_THUMB2_KERNEL
extern const union decode_item kprobe_decode_thumb16_table[];
extern const union decode_item kprobe_decode_thumb32_table[];
#else
extern const union decode_item kprobe_decode_arm_table[];
#endif


int kprobe_decode_insn(kprobe_opcode_t insn, struct arch_specific_insn *asi,
			const union decode_item *table, bool thumb16);


#endif 
