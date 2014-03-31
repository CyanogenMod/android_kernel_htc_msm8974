#ifndef __HEAD_BOOKE_H__
#define __HEAD_BOOKE_H__

#include <asm/ptrace.h>	

#define SET_IVOR(vector_number, vector_label)		\
		li	r26,vector_label@l; 		\
		mtspr	SPRN_IVOR##vector_number,r26;	\
		sync

#if (THREAD_SHIFT < 15)
#define ALLOC_STACK_FRAME(reg, val)			\
	addi reg,reg,val
#else
#define ALLOC_STACK_FRAME(reg, val)			\
	addis	reg,reg,val@ha;				\
	addi	reg,reg,val@l
#endif

#define THREAD_NORMSAVE(offset)	(THREAD_NORMSAVES + (offset * 4))

#define NORMAL_EXCEPTION_PROLOG						     \
	mtspr	SPRN_SPRG_WSCRATCH0, r10;		     \
	mfspr	r10, SPRN_SPRG_THREAD;					     \
	stw	r11, THREAD_NORMSAVE(0)(r10);				     \
	stw	r13, THREAD_NORMSAVE(2)(r10);				     \
	mfcr	r13;			\
	mfspr	r11,SPRN_SRR1;		\
	andi.	r11,r11,MSR_PR;						     \
	mr	r11, r1;						     \
	beq	1f;							     \
	       \
	lwz	r11, THREAD_INFO-THREAD(r10);				     \
	ALLOC_STACK_FRAME(r11, THREAD_SIZE);				     \
1 :	subi	r11, r11, INT_FRAME_SIZE;      \
	stw	r13, _CCR(r11);			     \
	stw	r12,GPR12(r11);						     \
	stw	r9,GPR9(r11);						     \
	mfspr	r13, SPRN_SPRG_RSCRATCH0;				     \
	stw	r13, GPR10(r11);					     \
	lwz	r12, THREAD_NORMSAVE(0)(r10);				     \
	stw	r12,GPR11(r11);						     \
	lwz	r13, THREAD_NORMSAVE(2)(r10); 		     \
	mflr	r10;							     \
	stw	r10,_LINK(r11);						     \
	mfspr	r12,SPRN_SRR0;						     \
	stw	r1, GPR1(r11);						     \
	mfspr	r9,SPRN_SRR1;						     \
	stw	r1, 0(r11);						     \
	mr	r1, r11;						     \
	rlwinm	r9,r9,0,14,12;		\
	stw	r0,GPR0(r11);						     \
	lis	r10, STACK_FRAME_REGS_MARKER@ha; \
	addi	r10, r10, STACK_FRAME_REGS_MARKER@l;			     \
	stw	r10, 8(r11);						     \
	SAVE_4GPRS(3, r11);						     \
	SAVE_2GPRS(7, r11)


#define MC_STACK_BASE		mcheckirq_ctx
#define CRIT_STACK_BASE		critirq_ctx

#define DBG_STACK_BASE		dbgirq_ctx

#define EXC_LVL_FRAME_OVERHEAD	(THREAD_SIZE - INT_FRAME_SIZE - EXC_LVL_SIZE)

#ifdef CONFIG_SMP
#define BOOKE_LOAD_EXC_LEVEL_STACK(level)		\
	mfspr	r8,SPRN_PIR;				\
	slwi	r8,r8,2;				\
	addis	r8,r8,level##_STACK_BASE@ha;		\
	lwz	r8,level##_STACK_BASE@l(r8);		\
	addi	r8,r8,EXC_LVL_FRAME_OVERHEAD;
#else
#define BOOKE_LOAD_EXC_LEVEL_STACK(level)		\
	lis	r8,level##_STACK_BASE@ha;		\
	lwz	r8,level##_STACK_BASE@l(r8);		\
	addi	r8,r8,EXC_LVL_FRAME_OVERHEAD;
#endif

#define EXC_LEVEL_EXCEPTION_PROLOG(exc_level, exc_level_srr0, exc_level_srr1) \
	mtspr	SPRN_SPRG_WSCRATCH_##exc_level,r8;			     \
	BOOKE_LOAD_EXC_LEVEL_STACK(exc_level); \
	stw	r9,GPR9(r8);		\
	mfcr	r9;			\
	stw	r10,GPR10(r8);						     \
	stw	r11,GPR11(r8);						     \
	stw	r9,_CCR(r8);		\
	mfspr	r10,exc_level_srr1;	\
	andi.	r10,r10,MSR_PR;						     \
	mfspr	r11,SPRN_SPRG_THREAD;	\
	lwz	r11,THREAD_INFO-THREAD(r11); \
	addi	r11,r11,EXC_LVL_FRAME_OVERHEAD;	\
	beq	1f;							     \
						     \
	stw	r9,_CCR(r11);		\
	lwz	r10,GPR10(r8);		\
	lwz	r9,GPR9(r8);						     \
	stw	r10,GPR10(r11);						     \
	lwz	r10,GPR11(r8);						     \
	stw	r9,GPR9(r11);						     \
	stw	r10,GPR11(r11);						     \
	b	2f;							     \
						     \
1:	lwz	r9,TI_FLAGS-EXC_LVL_FRAME_OVERHEAD(r11);		     \
	lwz	r10,TI_PREEMPT-EXC_LVL_FRAME_OVERHEAD(r11);		     \
	stw	r9,TI_FLAGS-EXC_LVL_FRAME_OVERHEAD(r8);			     \
	stw	r10,TI_PREEMPT-EXC_LVL_FRAME_OVERHEAD(r8);		     \
	lwz	r9,TI_TASK-EXC_LVL_FRAME_OVERHEAD(r11);			     \
	stw	r9,TI_TASK-EXC_LVL_FRAME_OVERHEAD(r8);			     \
	mr	r11,r8;							     \
2:	mfspr	r8,SPRN_SPRG_RSCRATCH_##exc_level;			     \
	stw	r12,GPR12(r11);		\
	mflr	r10;							     \
	stw	r10,_LINK(r11);						     \
	mfspr	r12,SPRN_DEAR;		\
	stw	r12,_DEAR(r11);		\
	mfspr	r9,SPRN_ESR;		\
	stw	r9,_ESR(r11);		\
	mfspr	r12,exc_level_srr0;					     \
	stw	r1,GPR1(r11);						     \
	mfspr	r9,exc_level_srr1;					     \
	stw	r1,0(r11);						     \
	mr	r1,r11;							     \
	rlwinm	r9,r9,0,14,12;		\
	stw	r0,GPR0(r11);						     \
	SAVE_4GPRS(3, r11);						     \
	SAVE_2GPRS(7, r11)

#define CRITICAL_EXCEPTION_PROLOG \
		EXC_LEVEL_EXCEPTION_PROLOG(CRIT, SPRN_CSRR0, SPRN_CSRR1)
#define DEBUG_EXCEPTION_PROLOG \
		EXC_LEVEL_EXCEPTION_PROLOG(DBG, SPRN_DSRR0, SPRN_DSRR1)
#define MCHECK_EXCEPTION_PROLOG \
		EXC_LEVEL_EXCEPTION_PROLOG(MC, SPRN_MCSRR0, SPRN_MCSRR1)

#define	START_EXCEPTION(label)						     \
        .align 5;              						     \
label:

#define FINISH_EXCEPTION(func)					\
	bl	transfer_to_handler_full;			\
	.long	func;						\
	.long	ret_from_except_full

#define EXCEPTION(n, label, hdlr, xfer)				\
	START_EXCEPTION(label);					\
	NORMAL_EXCEPTION_PROLOG;				\
	addi	r3,r1,STACK_FRAME_OVERHEAD;			\
	xfer(n, hdlr)

#define CRITICAL_EXCEPTION(n, label, hdlr)			\
	START_EXCEPTION(label);					\
	CRITICAL_EXCEPTION_PROLOG;				\
	addi	r3,r1,STACK_FRAME_OVERHEAD;			\
	EXC_XFER_TEMPLATE(hdlr, n+2, (MSR_KERNEL & ~(MSR_ME|MSR_DE|MSR_CE)), \
			  NOCOPY, crit_transfer_to_handler, \
			  ret_from_crit_exc)

#define MCHECK_EXCEPTION(n, label, hdlr)			\
	START_EXCEPTION(label);					\
	MCHECK_EXCEPTION_PROLOG;				\
	mfspr	r5,SPRN_ESR;					\
	stw	r5,_ESR(r11);					\
	addi	r3,r1,STACK_FRAME_OVERHEAD;			\
	EXC_XFER_TEMPLATE(hdlr, n+4, (MSR_KERNEL & ~(MSR_ME|MSR_DE|MSR_CE)), \
			  NOCOPY, mcheck_transfer_to_handler,   \
			  ret_from_mcheck_exc)

#define EXC_XFER_TEMPLATE(hdlr, trap, msr, copyee, tfer, ret)	\
	li	r10,trap;					\
	stw	r10,_TRAP(r11);					\
	lis	r10,msr@h;					\
	ori	r10,r10,msr@l;					\
	copyee(r10, r9);					\
	bl	tfer;		 				\
	.long	hdlr;						\
	.long	ret

#define COPY_EE(d, s)		rlwimi d,s,0,16,16
#define NOCOPY(d, s)

#define EXC_XFER_STD(n, hdlr)		\
	EXC_XFER_TEMPLATE(hdlr, n, MSR_KERNEL, NOCOPY, transfer_to_handler_full, \
			  ret_from_except_full)

#define EXC_XFER_LITE(n, hdlr)		\
	EXC_XFER_TEMPLATE(hdlr, n+1, MSR_KERNEL, NOCOPY, transfer_to_handler, \
			  ret_from_except)

#define EXC_XFER_EE(n, hdlr)		\
	EXC_XFER_TEMPLATE(hdlr, n, MSR_KERNEL, COPY_EE, transfer_to_handler_full, \
			  ret_from_except_full)

#define EXC_XFER_EE_LITE(n, hdlr)	\
	EXC_XFER_TEMPLATE(hdlr, n+1, MSR_KERNEL, COPY_EE, transfer_to_handler, \
			  ret_from_except)

#define DEBUG_DEBUG_EXCEPTION						      \
	START_EXCEPTION(DebugDebug);					      \
	DEBUG_EXCEPTION_PROLOG;						      \
									      \
								      \
	mfspr	r10,SPRN_DBSR;		  \
	andis.	r10,r10,(DBSR_IC|DBSR_BT)@h;				      \
	beq+	2f;							      \
									      \
	lis	r10,KERNELBASE@h;	   \
	ori	r10,r10,KERNELBASE@l;					      \
	cmplw	r12,r10;						      \
	blt+	2f;			    \
									      \
	lis	r10,DebugDebug@h;					      \
	ori	r10,r10,DebugDebug@l;					      \
	cmplw	r12,r10;						      \
	bgt+	2f;			    \
									      \
	     \
1:	rlwinm	r9,r9,0,~MSR_DE;	     \
	lis	r10,(DBSR_IC|DBSR_BT)@h;	      \
	mtspr	SPRN_DBSR,r10;						      \
						      \
	lwz	r10,_CCR(r11);						      \
	lwz	r0,GPR0(r11);						      \
	lwz	r1,GPR1(r11);						      \
	mtcrf	0x80,r10;						      \
	mtspr	SPRN_DSRR0,r12;						      \
	mtspr	SPRN_DSRR1,r9;						      \
	lwz	r9,GPR9(r11);						      \
	lwz	r12,GPR12(r11);						      \
	mtspr	SPRN_SPRG_WSCRATCH_DBG,r8;				      \
	BOOKE_LOAD_EXC_LEVEL_STACK(DBG);  \
	lwz	r10,GPR10(r8);						      \
	lwz	r11,GPR11(r8);						      \
	mfspr	r8,SPRN_SPRG_RSCRATCH_DBG;				      \
									      \
	PPC_RFDI;							      \
	b	.;							      \
									      \
			      \
2:	mfspr	r4,SPRN_DBSR;						      \
	addi	r3,r1,STACK_FRAME_OVERHEAD;				      \
	EXC_XFER_TEMPLATE(DebugException, 0x2008, (MSR_KERNEL & ~(MSR_ME|MSR_DE|MSR_CE)), NOCOPY, debug_transfer_to_handler, ret_from_debug_exc)

#define DEBUG_CRIT_EXCEPTION						      \
	START_EXCEPTION(DebugCrit);					      \
	CRITICAL_EXCEPTION_PROLOG;					      \
									      \
								      \
	mfspr	r10,SPRN_DBSR;		  \
	andis.	r10,r10,(DBSR_IC|DBSR_BT)@h;				      \
	beq+	2f;							      \
									      \
	lis	r10,KERNELBASE@h;	   \
	ori	r10,r10,KERNELBASE@l;					      \
	cmplw	r12,r10;						      \
	blt+	2f;			    \
									      \
	lis	r10,DebugCrit@h;					      \
	ori	r10,r10,DebugCrit@l;					      \
	cmplw	r12,r10;						      \
	bgt+	2f;			    \
									      \
	     \
1:	rlwinm	r9,r9,0,~MSR_DE;	     \
	lis	r10,(DBSR_IC|DBSR_BT)@h;	      \
	mtspr	SPRN_DBSR,r10;						      \
						      \
	lwz	r10,_CCR(r11);						      \
	lwz	r0,GPR0(r11);						      \
	lwz	r1,GPR1(r11);						      \
	mtcrf	0x80,r10;						      \
	mtspr	SPRN_CSRR0,r12;						      \
	mtspr	SPRN_CSRR1,r9;						      \
	lwz	r9,GPR9(r11);						      \
	lwz	r12,GPR12(r11);						      \
	mtspr	SPRN_SPRG_WSCRATCH_CRIT,r8;				      \
	BOOKE_LOAD_EXC_LEVEL_STACK(CRIT);   \
	lwz	r10,GPR10(r8);						      \
	lwz	r11,GPR11(r8);						      \
	mfspr	r8,SPRN_SPRG_RSCRATCH_CRIT;				      \
									      \
	rfci;								      \
	b	.;							      \
									      \
		      \
2:	mfspr	r4,SPRN_DBSR;						      \
	addi	r3,r1,STACK_FRAME_OVERHEAD;				      \
	EXC_XFER_TEMPLATE(DebugException, 0x2002, (MSR_KERNEL & ~(MSR_ME|MSR_DE|MSR_CE)), NOCOPY, crit_transfer_to_handler, ret_from_crit_exc)

#define DATA_STORAGE_EXCEPTION						      \
	START_EXCEPTION(DataStorage)					      \
	NORMAL_EXCEPTION_PROLOG;					      \
	mfspr	r5,SPRN_ESR;			      \
	stw	r5,_ESR(r11);						      \
	mfspr	r4,SPRN_DEAR;				      \
	EXC_XFER_LITE(0x0300, handle_page_fault)

#define INSTRUCTION_STORAGE_EXCEPTION					      \
	START_EXCEPTION(InstructionStorage)				      \
	NORMAL_EXCEPTION_PROLOG;					      \
	mfspr	r5,SPRN_ESR;			      \
	stw	r5,_ESR(r11);						      \
	mr      r4,r12;                 		      \
	li      r5,0;                   		      \
	EXC_XFER_LITE(0x0400, handle_page_fault)

#define ALIGNMENT_EXCEPTION						      \
	START_EXCEPTION(Alignment)					      \
	NORMAL_EXCEPTION_PROLOG;					      \
	mfspr   r4,SPRN_DEAR;           	      \
	stw     r4,_DEAR(r11);						      \
	addi    r3,r1,STACK_FRAME_OVERHEAD;				      \
	EXC_XFER_EE(0x0600, alignment_exception)

#define PROGRAM_EXCEPTION						      \
	START_EXCEPTION(Program)					      \
	NORMAL_EXCEPTION_PROLOG;					      \
	mfspr	r4,SPRN_ESR;			      \
	stw	r4,_ESR(r11);						      \
	addi	r3,r1,STACK_FRAME_OVERHEAD;				      \
	EXC_XFER_STD(0x0700, program_check_exception)

#define DECREMENTER_EXCEPTION						      \
	START_EXCEPTION(Decrementer)					      \
	NORMAL_EXCEPTION_PROLOG;					      \
	lis     r0,TSR_DIS@h;               \
	mtspr   SPRN_TSR,r0;			      \
	addi    r3,r1,STACK_FRAME_OVERHEAD;				      \
	EXC_XFER_LITE(0x0900, timer_interrupt)

#define FP_UNAVAILABLE_EXCEPTION					      \
	START_EXCEPTION(FloatingPointUnavailable)			      \
	NORMAL_EXCEPTION_PROLOG;					      \
	beq	1f;							      \
	bl	load_up_fpu;		   \
	b	fast_exception_return;					      \
1:	addi	r3,r1,STACK_FRAME_OVERHEAD;				      \
	EXC_XFER_EE_LITE(0x800, kernel_fp_unavailable_exception)

#ifndef __ASSEMBLY__
struct exception_regs {
	unsigned long mas0;
	unsigned long mas1;
	unsigned long mas2;
	unsigned long mas3;
	unsigned long mas6;
	unsigned long mas7;
	unsigned long srr0;
	unsigned long srr1;
	unsigned long csrr0;
	unsigned long csrr1;
	unsigned long dsrr0;
	unsigned long dsrr1;
	unsigned long saved_ksp_limit;
};

#define STACK_EXC_LVL_FRAME_SIZE	_ALIGN_UP(sizeof (struct exception_regs), 16)

#endif 
#endif 
