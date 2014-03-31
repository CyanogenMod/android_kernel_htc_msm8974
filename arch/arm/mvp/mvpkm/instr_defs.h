/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5


#ifndef _INSTR_DEFS_H_
#define _INSTR_DEFS_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include "utils.h"

#define ARM_REG_R0  0
#define ARM_REG_R1  1
#define ARM_REG_R2  2
#define ARM_REG_R3  3
#define ARM_REG_R4  4
#define ARM_REG_R5  5
#define ARM_REG_R6  6
#define ARM_REG_R7  7
#define ARM_REG_R8  8
#define ARM_REG_R9  9
#define ARM_REG_R10 10
#define ARM_REG_FP  11
#define ARM_REG_IP  12
#define ARM_REG_SP  13
#define ARM_REG_LR  14
#define ARM_REG_PC  15

#define ARM_INSTR_RN_BIT_POS 16
#define ARM_INSTR_RD_BIT_POS 12
#define ARM_INSTR_RS_BIT_POS 8
#define ARM_INSTR_RM_BIT_POS 0

#define ARM_INSTR_RN_LENGTH 4
#define ARM_INSTR_RD_LENGTH 4
#define ARM_INSTR_RS_LENGTH 4
#define ARM_INSTR_RM_LENGTH 4

#define ARM_INSTR_RN(r) \
	MVP_EXTRACT_FIELD((r), ARM_INSTR_RN_BIT_POS, ARM_INSTR_RN_LENGTH)
#define ARM_INSTR_RD(r) \
	MVP_EXTRACT_FIELD((r), ARM_INSTR_RD_BIT_POS, ARM_INSTR_RD_LENGTH)
#define ARM_INSTR_RS(r) \
	MVP_EXTRACT_FIELD((r), ARM_INSTR_RS_BIT_POS, ARM_INSTR_RS_LENGTH)
#define ARM_INSTR_RM(r) \
	MVP_EXTRACT_FIELD((r), ARM_INSTR_RM_BIT_POS, ARM_INSTR_RM_LENGTH)

#define ARM_INSTR_RN_SHIFT(word)  ((word) << ARM_INSTR_RN_BIT_POS)
#define ARM_INSTR_RD_SHIFT(word)  ((word) << ARM_INSTR_RD_BIT_POS)
#define ARM_INSTR_RS_SHIFT(word)  ((word) << ARM_INSTR_RS_BIT_POS)
#define ARM_INSTR_RM_SHIFT(word)  ((word) << ARM_INSTR_RM_BIT_POS)

#define ARM_INSTR_RN_MASK (~(ARM_INSTR_RN_SHIFT(0xf)))
#define ARM_INSTR_RD_MASK (~(ARM_INSTR_RD_SHIFT(0xf)))
#define ARM_INSTR_RS_MASK (~(ARM_INSTR_RS_SHIFT(0xf)))
#define ARM_INSTR_RM_MASK (~(ARM_INSTR_RM_SHIFT(0xf)))

#define ARM_INSTR_COND(instr) (MVP_EXTRACT_FIELD(instr, 28, 4))

#define ARM_INSTR_COND_EQ 0x0   
#define ARM_INSTR_COND_NE 0x1   
#define ARM_INSTR_COND_CS 0x2   
#define ARM_INSTR_COND_CC 0x3   
#define ARM_INSTR_COND_MI 0x4   
#define ARM_INSTR_COND_PL 0x5   
#define ARM_INSTR_COND_VS 0x6   
#define ARM_INSTR_COND_VC 0x7   
#define ARM_INSTR_COND_HI 0x8   
#define ARM_INSTR_COND_LS 0x9   
#define ARM_INSTR_COND_GE 0xa   
#define ARM_INSTR_COND_LT 0xb   
#define ARM_INSTR_COND_GT 0xc   
#define ARM_INSTR_COND_LE 0xd   
#define ARM_INSTR_COND_AL 0xe   
#define ARM_INSTR_COND_NV 0xf   


#define ARM_INSTR_LDST_IBIT(instr) (MVP_BIT(instr, 25))

#define ARM_INSTR_LDST_UBIT(instr) (MVP_BIT(instr, 23))

#define ARM_INSTR_LDST_BBIT(instr) (MVP_BIT(instr, 22))

#define ARM_INSTR_LDST_LBIT(instr) (MVP_BIT(instr, 20))

#define ARM_INSTR_LDST_SHIFTER_OPERAND(instr) (MVP_EXTRACT_FIELD(instr, 0, 12))

#define ARM_INSTR_LDST_IMMEDIATE(instr) (MVP_EXTRACT_FIELD(instr, 0, 12))

#define ARM_INSTR_LDMSTM_REGLIST(instr) (MVP_EXTRACT_FIELD(instr, 0, 16))

#define ARM_INSTR_MISC_LDST_IMM_OFFSET(instr) \
	((MVP_EXTRACT_FIELD(instr, 8, 4) << 4) | MVP_EXTRACT_FIELD(instr, 0, 4))



#define ARM_THUMB_INSTR_LDST_LBIT(instr) (MVP_BIT(instr, 20))

#define ARM_THUMB_INSTR_LDST_WBIT(instr) (MVP_BIT(instr, 22))

#define ARM_THUMB_INSTR_LDST_IMMEDIATE(instr) (MVP_EXTRACT_FIELD(instr, 0, 8))


#define ARM_OP_BR_A1            0x0a000000
#define ARM_OP_BX_A1            0x012fff10
#define ARM_OP_LDR_LIT_A1       0x051f0000
#define ARM_OP_MOV_A1           0x01a00000
#define ARM_OP_MOVW_A2          0x03000000
#define ARM_OP_MOVT_A1          0x03400000
#define ARM_OP_MRS_T1           0x8000f3e0
#define ARM_OP_MRS_A1           0x01000000
#define ARM_OP_MSR_T1           0x8000f380
#define ARM_OP_MSR_A1           0x0120f000
#define ARM_OP_HVC_A1           0x01400070
#define ARM_OP_ERET_T1          0x8f00f3de
#define ARM_OP_ERET_A1          0x0160006e

#define ARM_OP_MRS_EXT_T1       (ARM_OP_MRS_T1 | (1 << 21))
#define ARM_OP_MRS_EXT_A1       (ARM_OP_MRS_A1 | (1 << 9))
#define ARM_OP_MSR_EXT_T1       (ARM_OP_MSR_T1 | (1 << 21))
#define ARM_OP_MSR_EXT_A1       (ARM_OP_MSR_A1 | (1 << 9))

#define ARM_OP_I                0x02000000
#define ARM_OP_S                0x00100000
#define ARM_OP_W                0x00200000

#define ARM_INSTR_CLASS(instr) MVP_BITS(instr, 25, 27)

#define ARM_INSTR_CLASS_BRANCH 0x5

#define ARM_INSTR_OPCODE(instr) MVP_EXTRACT_FIELD(instr, 20, 8)

#define ARM_INSTR_OPCODE_EQ(instr1, instr2) \
	(ARM_INSTR_OPCODE(instr1) == ARM_INSTR_OPCODE(instr2))

#define ARM_INSTR_BRANCH_TARGET(inst) (((int32)(inst) << 8) >> 6)

#define ARM_INSTR_BRANCH_TARGET_OVERFLOWS(v) ((v) + (1 << 25) >= (1 << 26))

#define ARM_INSTR_BRANCH_UPDATE_OFFSET(ins, offset) \
	(((ins) & MVP_MASK(24, 8))  | (((offset) >> 2) & MVP_MASK(0, 24)))

#define ARM_INSTR_BR_ENC(cond, offset) \
	(((cond) << 28) | ARM_OP_BR_A1 | MVP_BITS(((uint32)offset) >> 2, 0, 23))

#define ARM_INSTR_BX_ENC(cond, rm) \
	(((cond) << 28) | ARM_OP_BX_A1 | (rm))

#define ARM_INSTR_LDR_LIT_ADD_ENC(cond, reg, offset)		\
	(((cond) << 28) | ARM_OP_LDR_LIT_A1 | (1 << 23) |	\
	 ((reg) << 12) | (offset))

#define ARM_INSTR_MOV_A1_ENC(cond, rd, rn) \
	((((cond) << 28) | ARM_OP_MOV_A1 | ((rd) << 12) | (rn)))

#define ARM_INSTR_MOVTW_IMMED(instr) \
	(MVP_BITS(instr, 0, 11) | (MVP_BITS(instr, 16, 19) << 12))

#define ARM_INSTR_MOVW_A2_ENC(cond, rd, immed)				     \
	(((cond) << 28) | ARM_OP_MOVW_A2 | (MVP_BITS(immed, 12, 15) << 16) | \
	 ((rd) << 12) | MVP_BITS(immed, 0, 11))

#define ARM_INSTR_MOVT_A1_ENC(cond, rd, immed)			\
	(((cond) << 28) | ARM_OP_MOVT_A1 |			\
	 (MVP_BITS(((immed) >> 16), 12, 15) << 16) |		\
	 ((rd) << 12) | MVP_BITS(((immed) >> 16), 0, 11))

#define ARM_INSTR_BKPT_ENC(immed)			\
	(0xe1200070 |					\
	 MVP_EXTRACT_FIELD(immed, 0, 4) |		\
	 (MVP_EXTRACT_FIELD(immed, 4, 12) << 8))

#define ARM_INSTR_HVC_A1_ENC(immed)			\
	((ARM_INSTR_COND_AL << 28) | ARM_OP_HVC_A1 |	\
	 MVP_EXTRACT_FIELD(immed, 0, 4) |		\
	 (MVP_EXTRACT_FIELD(immed, 4, 12) << 8))

#define ARM_INSTR_ERET_A1_ENC(cond) \
	(((cond) << 28) | ARM_OP_ERET_A1)

#define ARM_REG_R8_USR  0
#define ARM_REG_R9_USR  1
#define ARM_REG_R10_USR 2
#define ARM_REG_R11_USR 3
#define ARM_REG_R12_USR 4
#define ARM_REG_SP_USR  5
#define ARM_REG_LR_USR  6
#define ARM_REG_R8_FIQ  8
#define ARM_REG_R9_FIQ  9
#define ARM_REG_R10_FIQ 10
#define ARM_REG_FP_FIQ 11
#define ARM_REG_IP_FIQ 12
#define ARM_REG_SP_FIQ  13
#define ARM_REG_LR_FIQ  14
#define ARM_REG_LR_IRQ  16
#define ARM_REG_SP_IRQ  17
#define ARM_REG_LR_SVC  18
#define ARM_REG_SP_SVC  19
#define ARM_REG_LR_ABT  20
#define ARM_REG_SP_ABT  21
#define ARM_REG_LR_UND  22
#define ARM_REG_SP_UND  23
#define ARM_REG_LR_MON  28
#define ARM_REG_SP_MON  29
#define ARM_REG_ELR_HYP 30
#define ARM_REG_SP_HYP  31

#define R_EXTEND(x) ((1 << 5) | (x))
#define ARM_REG_SPSR_FIQ R_EXTEND(ARM_REG_LR_FIQ)
#define ARM_REG_SPSR_IRQ R_EXTEND(ARM_REG_LR_IRQ)
#define ARM_REG_SPSR_SVC R_EXTEND(ARM_REG_LR_SVC)
#define ARM_REG_SPSR_ABT R_EXTEND(ARM_REG_LR_ABT)
#define ARM_REG_SPSR_UND R_EXTEND(ARM_REG_LR_UND)
#define ARM_REG_SPSR_MON R_EXTEND(ARM_REG_LR_MON)
#define ARM_REG_SPSR_HYP R_EXTEND(ARM_REG_ELR_HYP)

#define ARM_INSTR_MSR_EXT_T1_ENC(rm, rn)				\
	(ARM_OP_MSR_EXT_T1 | (MVP_BIT(rm, 5) << 4) |			\
	 (MVP_BIT(rm, 4) << 20) | (MVP_EXTRACT_FIELD(rm, 0, 4) << 24) |	\
	 ((rn) << 0))

#define ARM_INSTR_MSR_EXT_A1_ENC(cond, rm, rn)				\
	(((cond) << 28) | ARM_OP_MSR_EXT_A1 | (MVP_BIT(rm, 5) << 22) |	\
	 (MVP_BIT(rm, 4) << 8) | (MVP_EXTRACT_FIELD(rm, 0, 4) << 16) |	\
	 ((rn) << 0))

#define ARM_INSTR_MRS_EXT_T1_ENC(rm, rd)				\
	(ARM_OP_MRS_EXT_T1 | (MVP_BIT(rm, 5) << 4) |			\
	 (MVP_BIT(rm, 4) << 20) | (MVP_EXTRACT_FIELD(rm, 0, 4) << 0) |	\
	 ((rd) << 24))

#define ARM_INSTR_MRS_EXT_A1_ENC(cond, rd, rm)				\
	(((cond) << 28) | ARM_OP_MRS_EXT_A1 | (MVP_BIT(rm, 5) << 22) |	\
	 (MVP_BIT(rm, 4) << 8) | (MVP_EXTRACT_FIELD(rm, 0, 4) << 16) |	\
	 ((rd) << 12))

#define ARM_INSTR_COPROC_CR_LEN     4
#define ARM_INSTR_COPROC_CR_MAX     (1 << ARM_INSTR_COPROC_CR_LEN)
#define ARM_INSTR_COPROC_OPCODE_LEN 3
#define ARM_INSTR_COPROC_OPCODE_MAX (1 << ARM_INSTR_COPROC_OPCODE_LEN)

#define ARM_INSTR_COPROC_CRM(instr)     MVP_EXTRACT_FIELD(instr, 0, 4)
#define ARM_INSTR_COPROC_CRN(instr)     MVP_EXTRACT_FIELD(instr, 16, 4)
#define ARM_INSTR_COPROC_OPCODE1(instr) MVP_EXTRACT_FIELD(instr, 21, 3)
#define ARM_INSTR_COPROC_OPCODE2(instr) MVP_EXTRACT_FIELD(instr, 5, 3)
#define ARM_INSTR_COPROC_OPCODE(instr)  MVP_EXTRACT_FIELD(instr, 4, 4)
#define ARM_INSTR_COPROC_CPNUM(instr)   MVP_EXTRACT_FIELD(instr, 8, 4)

#define ARM_INSTR_IS_VMRS(instr)			\
	((MVP_EXTRACT_FIELD(instr, 0, 12) == 0xa10) &&	\
	 (ARM_INSTR_OPCODE(instr) == 0xef))

#define ARM_INSTR_IS_VMSR(instr)			\
	((MVP_EXTRACT_FIELD(instr, 0, 12) == 0xa10) &&	\
	 (ARM_INSTR_OPCODE(instr) == 0xee))

#define ARM_INSTR_VMRS_SPECREG(instr) MVP_EXTRACT_FIELD(instr, 16, 4)
#define ARM_INSTR_VMRS_RT(instr)      MVP_EXTRACT_FIELD(instr, 12, 4)

#define ARM_INSTR_VMSR_SPECREG(instr) MVP_EXTRACT_FIELD(instr, 16, 4)
#define ARM_INSTR_VMSR_RT(instr)      MVP_EXTRACT_FIELD(instr, 12, 4)

#define ARM_INSTR_IS_SWP(instr) ((instr & 0x0fb00ff0) == 0x01000090)

#endif 
