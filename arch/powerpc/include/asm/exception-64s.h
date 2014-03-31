#ifndef _ASM_POWERPC_EXCEPTION_H
#define _ASM_POWERPC_EXCEPTION_H
/*
 * Extracted from head_64.S
 *
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Rewritten by Cort Dougan (cort@cs.nmt.edu) for PReP
 *    Copyright (C) 1996 Cort Dougan <cort@cs.nmt.edu>
 *  Adapted for Power Macintosh by Paul Mackerras.
 *  Low-level exception handlers and MMU support
 *  rewritten by Paul Mackerras.
 *    Copyright (C) 1996 Paul Mackerras.
 *
 *  Adapted for 64bit PowerPC by Dave Engebretsen, Peter Bergner, and
 *    Mike Corrigan {engebret|bergner|mikejc}@us.ibm.com
 *
 *  This file contains the low-level support and setup for the
 *  PowerPC-64 platform, including trap and interrupt dispatch.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#define EX_R9		0
#define EX_R10		8
#define EX_R11		16
#define EX_R12		24
#define EX_R13		32
#define EX_SRR0		40
#define EX_DAR		48
#define EX_DSISR	56
#define EX_CCR		60
#define EX_R3		64
#define EX_LR		72
#define EX_CFAR		80

#define LOAD_HANDLER(reg, label)					\
	addi	reg,reg,(label)-_stext;	

#define EXC_HV	H
#define EXC_STD

#define __EXCEPTION_PROLOG_1(area, extra, vec)				\
	GET_PACA(r13);							\
	std	r9,area+EX_R9(r13);			\
	std	r10,area+EX_R10(r13);					\
	BEGIN_FTR_SECTION_NESTED(66);					\
	mfspr	r10,SPRN_CFAR;						\
	std	r10,area+EX_CFAR(r13);					\
	END_FTR_SECTION_NESTED(CPU_FTR_CFAR, CPU_FTR_CFAR, 66);		\
	mfcr	r9;							\
	extra(vec);							\
	std	r11,area+EX_R11(r13);					\
	std	r12,area+EX_R12(r13);					\
	GET_SCRATCH0(r10);						\
	std	r10,area+EX_R13(r13)
#define EXCEPTION_PROLOG_1(area, extra, vec)				\
	__EXCEPTION_PROLOG_1(area, extra, vec)

#define __EXCEPTION_PROLOG_PSERIES_1(label, h)				\
	ld	r12,PACAKBASE(r13);		\
	ld	r10,PACAKMSR(r13);		\
	mfspr	r11,SPRN_##h##SRR0;				\
	LOAD_HANDLER(r12,label)						\
	mtspr	SPRN_##h##SRR0,r12;					\
	mfspr	r12,SPRN_##h##SRR1;				\
	mtspr	SPRN_##h##SRR1,r10;					\
	h##rfid;							\
	b	.	
#define EXCEPTION_PROLOG_PSERIES_1(label, h)				\
	__EXCEPTION_PROLOG_PSERIES_1(label, h)

#define EXCEPTION_PROLOG_PSERIES(area, label, h, extra, vec)		\
	EXCEPTION_PROLOG_1(area, extra, vec);				\
	EXCEPTION_PROLOG_PSERIES_1(label, h);

#define __KVMTEST(n)							\
	lbz	r10,HSTATE_IN_GUEST(r13);			\
	cmpwi	r10,0;							\
	bne	do_kvm_##n

#define __KVM_HANDLER(area, h, n)					\
do_kvm_##n:								\
	ld	r10,area+EX_R10(r13);					\
	stw	r9,HSTATE_SCRATCH1(r13);			\
	ld	r9,area+EX_R9(r13);					\
	std	r12,HSTATE_SCRATCH0(r13);			\
	li	r12,n;							\
	b	kvmppc_interrupt

#define __KVM_HANDLER_SKIP(area, h, n)					\
do_kvm_##n:								\
	cmpwi	r10,KVM_GUEST_MODE_SKIP;				\
	ld	r10,area+EX_R10(r13);					\
	beq	89f;							\
	stw	r9,HSTATE_SCRATCH1(r13);			\
	ld	r9,area+EX_R9(r13);					\
	std	r12,HSTATE_SCRATCH0(r13);			\
	li	r12,n;							\
	b	kvmppc_interrupt;					\
89:	mtocrf	0x80,r9;						\
	ld	r9,area+EX_R9(r13);					\
	b	kvmppc_skip_##h##interrupt

#ifdef CONFIG_KVM_BOOK3S_64_HANDLER
#define KVMTEST(n)			__KVMTEST(n)
#define KVM_HANDLER(area, h, n)		__KVM_HANDLER(area, h, n)
#define KVM_HANDLER_SKIP(area, h, n)	__KVM_HANDLER_SKIP(area, h, n)

#else
#define KVMTEST(n)
#define KVM_HANDLER(area, h, n)
#define KVM_HANDLER_SKIP(area, h, n)
#endif

#ifdef CONFIG_KVM_BOOK3S_PR
#define KVMTEST_PR(n)			__KVMTEST(n)
#define KVM_HANDLER_PR(area, h, n)	__KVM_HANDLER(area, h, n)
#define KVM_HANDLER_PR_SKIP(area, h, n)	__KVM_HANDLER_SKIP(area, h, n)

#else
#define KVMTEST_PR(n)
#define KVM_HANDLER_PR(area, h, n)
#define KVM_HANDLER_PR_SKIP(area, h, n)
#endif

#define NOTEST(n)

#define EXCEPTION_PROLOG_COMMON(n, area)				   \
	andi.	r10,r12,MSR_PR;		 \
	mr	r10,r1;			 \
	subi	r1,r1,INT_FRAME_SIZE;	 \
	beq-	1f;							   \
	ld	r1,PACAKSAVE(r13);	 \
1:	cmpdi	cr1,r1,0;		 \
	blt+	cr1,3f;			 \
	li	r1,(n);			 \
	sth	r1,PACA_TRAP_SAVE(r13);					   \
	std	r3,area+EX_R3(r13);					   \
	addi	r3,r13,area;			   \
	b	bad_stack;						   \
3:	std	r9,_CCR(r1);		 \
	std	r11,_NIP(r1);		 \
	std	r12,_MSR(r1);		 \
	std	r10,0(r1);		 \
	std	r0,GPR0(r1);		 \
	std	r10,GPR1(r1);		 \
	ACCOUNT_CPU_USER_ENTRY(r9, r10);				   \
	std	r2,GPR2(r1);		 \
	SAVE_4GPRS(3, r1);		 \
	SAVE_2GPRS(7, r1);		 \
	ld	r9,area+EX_R9(r13);	 \
	ld	r10,area+EX_R10(r13);					   \
	std	r9,GPR9(r1);						   \
	std	r10,GPR10(r1);						   \
	ld	r9,area+EX_R11(r13);	 \
	ld	r10,area+EX_R12(r13);					   \
	ld	r11,area+EX_R13(r13);					   \
	std	r9,GPR11(r1);						   \
	std	r10,GPR12(r1);						   \
	std	r11,GPR13(r1);						   \
	BEGIN_FTR_SECTION_NESTED(66);					   \
	ld	r10,area+EX_CFAR(r13);					   \
	std	r10,ORIG_GPR3(r1);					   \
	END_FTR_SECTION_NESTED(CPU_FTR_CFAR, CPU_FTR_CFAR, 66);		   \
	ld	r2,PACATOC(r13);	 \
	mflr	r9;			 \
	std	r9,_LINK(r1);						   \
	mfctr	r10;			 \
	std	r10,_CTR(r1);						   \
	lbz	r10,PACASOFTIRQEN(r13);				   \
	mfspr	r11,SPRN_XER;		 \
	std	r10,SOFTE(r1);						   \
	std	r11,_XER(r1);						   \
	li	r9,(n)+1;						   \
	std	r9,_TRAP(r1);		 \
	li	r10,0;							   \
	ld	r11,exception_marker@toc(r2);				   \
	std	r10,RESULT(r1);		 \
	std	r11,STACK_FRAME_OVERHEAD-16(r1);  \
	ACCOUNT_STOLEN_TIME

#define STD_EXCEPTION_PSERIES(loc, vec, label)		\
	. = loc;					\
	.globl label##_pSeries;				\
label##_pSeries:					\
	HMT_MEDIUM;					\
	SET_SCRATCH0(r13);				\
	EXCEPTION_PROLOG_PSERIES(PACA_EXGEN, label##_common,	\
				 EXC_STD, KVMTEST_PR, vec)

#define STD_EXCEPTION_HV(loc, vec, label)		\
	. = loc;					\
	.globl label##_hv;				\
label##_hv:						\
	HMT_MEDIUM;					\
	SET_SCRATCH0(r13);				\
	EXCEPTION_PROLOG_PSERIES(PACA_EXGEN, label##_common,	\
				 EXC_HV, KVMTEST, vec)

#define SOFTEN_VALUE_0x500	PACA_IRQ_EE
#define SOFTEN_VALUE_0x502	PACA_IRQ_EE
#define SOFTEN_VALUE_0x900	PACA_IRQ_DEC
#define SOFTEN_VALUE_0x982	PACA_IRQ_DEC

#define __SOFTEN_TEST(h, vec)						\
	lbz	r10,PACASOFTIRQEN(r13);					\
	cmpwi	r10,0;							\
	li	r10,SOFTEN_VALUE_##vec;					\
	beq	masked_##h##interrupt
#define _SOFTEN_TEST(h, vec)	__SOFTEN_TEST(h, vec)

#define SOFTEN_TEST_PR(vec)						\
	KVMTEST_PR(vec);						\
	_SOFTEN_TEST(EXC_STD, vec)

#define SOFTEN_TEST_HV(vec)						\
	KVMTEST(vec);							\
	_SOFTEN_TEST(EXC_HV, vec)

#define SOFTEN_TEST_HV_201(vec)						\
	KVMTEST(vec);							\
	_SOFTEN_TEST(EXC_STD, vec)

#define __MASKABLE_EXCEPTION_PSERIES(vec, label, h, extra)		\
	HMT_MEDIUM;							\
	SET_SCRATCH0(r13);    				\
	__EXCEPTION_PROLOG_1(PACA_EXGEN, extra, vec);		\
	EXCEPTION_PROLOG_PSERIES_1(label##_common, h);
#define _MASKABLE_EXCEPTION_PSERIES(vec, label, h, extra)		\
	__MASKABLE_EXCEPTION_PSERIES(vec, label, h, extra)

#define MASKABLE_EXCEPTION_PSERIES(loc, vec, label)			\
	. = loc;							\
	.globl label##_pSeries;						\
label##_pSeries:							\
	_MASKABLE_EXCEPTION_PSERIES(vec, label,				\
				    EXC_STD, SOFTEN_TEST_PR)

#define MASKABLE_EXCEPTION_HV(loc, vec, label)				\
	. = loc;							\
	.globl label##_hv;						\
label##_hv:								\
	_MASKABLE_EXCEPTION_PSERIES(vec, label,				\
				    EXC_HV, SOFTEN_TEST_HV)


#define DISABLE_INTS	SOFT_DISABLE_INTS(r10,r11)

#define ADD_NVGPRS				\
	bl	.save_nvgprs

#define RUNLATCH_ON				\
BEGIN_FTR_SECTION				\
	clrrdi	r3,r1,THREAD_SHIFT;		\
	ld	r4,TI_LOCAL_FLAGS(r3);		\
	andi.	r0,r4,_TLF_RUNLATCH;		\
	beql	ppc64_runlatch_on_trampoline;	\
END_FTR_SECTION_IFSET(CPU_FTR_CTRL)

#define EXCEPTION_COMMON(trap, label, hdlr, ret, additions)	\
	.align	7;						\
	.globl label##_common;					\
label##_common:							\
	EXCEPTION_PROLOG_COMMON(trap, PACA_EXGEN);		\
	additions;						\
	addi	r3,r1,STACK_FRAME_OVERHEAD;			\
	bl	hdlr;						\
	b	ret

#define STD_EXCEPTION_COMMON(trap, label, hdlr)			\
	EXCEPTION_COMMON(trap, label, hdlr, ret_from_except,	\
			 ADD_NVGPRS;DISABLE_INTS)

#define STD_EXCEPTION_COMMON_ASYNC(trap, label, hdlr)		  \
	EXCEPTION_COMMON(trap, label, hdlr, ret_from_except_lite, \
			 FINISH_NAP;RUNLATCH_ON;DISABLE_INTS)

#ifdef CONFIG_PPC_970_NAP
#define FINISH_NAP				\
BEGIN_FTR_SECTION				\
	clrrdi	r11,r1,THREAD_SHIFT;		\
	ld	r9,TI_LOCAL_FLAGS(r11);		\
	andi.	r10,r9,_TLF_NAPPING;		\
	bnel	power4_fixup_nap;		\
END_FTR_SECTION_IFSET(CPU_FTR_CAN_NAP)
#else
#define FINISH_NAP
#endif

#endif	
