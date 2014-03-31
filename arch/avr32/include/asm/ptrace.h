/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_PTRACE_H
#define __ASM_AVR32_PTRACE_H

#define PTRACE_GETREGS		12
#define PTRACE_SETREGS		13

#define SR_H		0x20000000
#define SR_J		0x10000000
#define SR_DM		0x08000000
#define SR_D		0x04000000
#define MODE_NMI	0x01c00000
#define MODE_EXCEPTION	0x01800000
#define MODE_INT3	0x01400000
#define MODE_INT2	0x01000000
#define MODE_INT1	0x00c00000
#define MODE_INT0	0x00800000
#define MODE_SUPERVISOR	0x00400000
#define MODE_USER	0x00000000
#define MODE_MASK	0x01c00000
#define SR_EM		0x00200000
#define SR_I3M		0x00100000
#define SR_I2M		0x00080000
#define SR_I1M		0x00040000
#define SR_I0M		0x00020000
#define SR_GM		0x00010000

#define SR_H_BIT	29
#define SR_J_BIT	28
#define SR_DM_BIT	27
#define SR_D_BIT	26
#define MODE_SHIFT	22
#define SR_EM_BIT	21
#define SR_I3M_BIT	20
#define SR_I2M_BIT	19
#define SR_I1M_BIT	18
#define SR_I0M_BIT	17
#define SR_GM_BIT	16

#define SR_L		0x00000020
#define SR_Q		0x00000010
#define SR_V		0x00000008
#define SR_N		0x00000004
#define SR_Z		0x00000002
#define SR_C		0x00000001

#define SR_L_BIT	5
#define SR_Q_BIT	4
#define SR_V_BIT	3
#define SR_N_BIT	2
#define SR_Z_BIT	1
#define SR_C_BIT	0

#define FRAME_SIZE_FULL 72
#define REG_R12_ORIG	68
#define REG_R0		64
#define REG_R1		60
#define REG_R2		56
#define REG_R3		52
#define REG_R4		48
#define REG_R5		44
#define REG_R6		40
#define REG_R7		36
#define REG_R8		32
#define REG_R9		28
#define REG_R10		24
#define REG_R11		20
#define REG_R12		16
#define REG_SP		12
#define REG_LR		 8

#define FRAME_SIZE_MIN	 8
#define REG_PC		 4
#define REG_SR		 0

#ifndef __ASSEMBLY__
struct pt_regs {
	
	unsigned long sr;
	unsigned long pc;

	
	unsigned long lr;
	unsigned long sp;
	unsigned long r12;
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long r7;
	unsigned long r6;
	unsigned long r5;
	unsigned long r4;
	unsigned long r3;
	unsigned long r2;
	unsigned long r1;
	unsigned long r0;

	
	unsigned long r12_orig;
};

#ifdef __KERNEL__

#include <asm/ocd.h>

#define arch_has_single_step()		(1)

#define arch_ptrace_attach(child)       ocd_enable(child)

#define user_mode(regs)                 (((regs)->sr & MODE_MASK) == MODE_USER)
#define instruction_pointer(regs)       ((regs)->pc)
#define profile_pc(regs)                instruction_pointer(regs)

static __inline__ int valid_user_regs(struct pt_regs *regs)
{
	if ((regs->sr & 0xffff0000) == 0)
		return 1;

	regs->sr &= 0x0000ffff;
	return 0;
}


#endif 

#endif 

#endif 
