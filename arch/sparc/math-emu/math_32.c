/*
 * arch/sparc/math-emu/math.c
 *
 * Copyright (C) 1998 Peter Maydell (pmaydell@chiark.greenend.org.uk)
 * Copyright (C) 1997, 1999 Jakub Jelinek (jj@ultra.linux.cz)
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 *
 * This is a good place to start if you're trying to understand the
 * emulation code, because it's pretty simple. What we do is
 * essentially analyse the instruction to work out what the operation
 * is and which registers are involved. We then execute the appropriate
 * FXXXX function. [The floating point queue introduces a minor wrinkle;
 * see below...]
 * The fxxxxx.c files each emulate a single insn. They look relatively
 * simple because the complexity is hidden away in an unholy tangle
 * of preprocessor macros.
 *
 * The first layer of macros is single.h, double.h, quad.h. Generally
 * these files define macros for working with floating point numbers
 * of the three IEEE formats. FP_ADD_D(R,A,B) is for adding doubles,
 * for instance. These macros are usually defined as calls to more
 * generic macros (in this case _FP_ADD(D,2,R,X,Y) where the number
 * of machine words required to store the given IEEE format is passed
 * as a parameter. [double.h and co check the number of bits in a word
 * and define FP_ADD_D & co appropriately].
 * The generic macros are defined in op-common.h. This is where all
 * the grotty stuff like handling NaNs is coded. To handle the possible
 * word sizes macros in op-common.h use macros like _FP_FRAC_SLL_##wc()
 * where wc is the 'number of machine words' parameter (here 2).
 * These are defined in the third layer of macros: op-1.h, op-2.h
 * and op-4.h. These handle operations on floating point numbers composed
 * of 1,2 and 4 machine words respectively. [For example, on sparc64
 * doubles are one machine word so macros in double.h eventually use
 * constructs in op-1.h, but on sparc32 they use op-2.h definitions.]
 * soft-fp.h is on the same level as op-common.h, and defines some
 * macros which are independent of both word size and FP format.
 * Finally, sfp-machine.h is the machine dependent part of the
 * code: it defines the word size and what type a word is. It also
 * defines how _FP_MUL_MEAT_t() maps to _FP_MUL_MEAT_n_* : op-n.h
 * provide several possible flavours of multiply algorithm, most
 * of which require that you supply some form of asm or C primitive to
 * do the actual multiply. (such asm primitives should be defined
 * in sfp-machine.h too). udivmodti4.c is the same sort of thing.
 *
 * There may be some errors here because I'm working from a
 * SPARC architecture manual V9, and what I really want is V8...
 * Also, the insns which can generate exceptions seem to be a
 * greater subset of the FPops than for V9 (for example, FCMPED
 * has to be emulated on V8). So I think I'm going to have
 * to emulate them all just to be on the safe side...
 *
 * Emulation routines originate from soft-fp package, which is
 * part of glibc and has appropriate copyrights in it (allegedly).
 *
 * NB: on sparc int == long == 4 bytes, long long == 8 bytes.
 * Most bits of the kernel seem to go for long rather than int,
 * so we follow that practice...
 */


#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/perf_event.h>
#include <asm/uaccess.h>

#include "sfp-util_32.h"
#include <math-emu/soft-fp.h>
#include <math-emu/single.h>
#include <math-emu/double.h>
#include <math-emu/quad.h>

#define FLOATFUNC(x) extern int x(void *,void *,void *)

#define FSQRTQ	0x02b		
#define FADDQ	0x043		
#define FSUBQ	0x047		
#define FMULQ	0x04b		
#define FDIVQ	0x04f		
#define FDMULQ	0x06e		
#define FQTOS	0x0c7		
#define FQTOD	0x0cb		
#define FITOQ	0x0cc		
#define FSTOQ	0x0cd		
#define FDTOQ	0x0ce		
#define FQTOI	0x0d3		
#define FCMPQ	0x053		
#define FCMPEQ	0x057		
#define FSQRTS	0x029		
#define FSQRTD	0x02a		
#define FADDS	0x041		
#define FADDD	0x042		
#define FSUBS	0x045		
#define FSUBD	0x046		
#define FMULS	0x049		
#define FMULD	0x04a		
#define FDIVS	0x04d		
#define FDIVD	0x04e		
#define FSMULD	0x069		
#define FDTOS	0x0c6		
#define FSTOD	0x0c9		
#define FSTOI	0x0d1		
#define FDTOI	0x0d2		
#define FABSS	0x009		
#define FCMPS	0x051		
#define FCMPES	0x055		
#define FCMPD	0x052		
#define FCMPED	0x056		
#define FMOVS	0x001		
#define FNEGS	0x005		
#define FITOS	0x0c4		
#define FITOD	0x0c8		

#define FSR_TEM_SHIFT	23UL
#define FSR_TEM_MASK	(0x1fUL << FSR_TEM_SHIFT)
#define FSR_AEXC_SHIFT	5UL
#define FSR_AEXC_MASK	(0x1fUL << FSR_AEXC_SHIFT)
#define FSR_CEXC_SHIFT	0UL
#define FSR_CEXC_MASK	(0x1fUL << FSR_CEXC_SHIFT)

static int do_one_mathemu(u32 insn, unsigned long *fsr, unsigned long *fregs);

int do_mathemu(struct pt_regs *regs, struct task_struct *fpt)
{

	

	int i;
	int retcode = 0;                               
	unsigned long insn;

	perf_sw_event(PERF_COUNT_SW_EMULATION_FAULTS, 1, regs, 0);

#ifdef DEBUG_MATHEMU
	printk("In do_mathemu()... pc is %08lx\n", regs->pc);
	printk("fpqdepth is %ld\n", fpt->thread.fpqdepth);
	for (i = 0; i < fpt->thread.fpqdepth; i++)
		printk("%d: %08lx at %08lx\n", i, fpt->thread.fpqueue[i].insn,
		       (unsigned long)fpt->thread.fpqueue[i].insn_addr);
#endif

	if (fpt->thread.fpqdepth == 0) {                   
#ifdef DEBUG_MATHEMU
		printk("precise trap at %08lx\n", regs->pc);
#endif
		if (!get_user(insn, (u32 __user *) regs->pc)) {
			retcode = do_one_mathemu(insn, &fpt->thread.fsr, fpt->thread.float_regs);
			if (retcode) {
				
				regs->pc = regs->npc;
				regs->npc += 4;
			}
		}
		return retcode;
	}

	
	for (i = 0; i < fpt->thread.fpqdepth; i++) {
		retcode = do_one_mathemu(fpt->thread.fpqueue[i].insn, &(fpt->thread.fsr), fpt->thread.float_regs);
		if (!retcode)                               
			break;
	}
	
	if (retcode)
		fpt->thread.fsr &= ~(0x3000 | FSR_CEXC_MASK);
	else
		fpt->thread.fsr &= ~0x3000;
	fpt->thread.fpqdepth = 0;

	return retcode;
}

static inline int record_exception(unsigned long *pfsr, int eflag)
{
	unsigned long fsr = *pfsr;
	int would_trap;

	
	would_trap = (fsr & ((long)eflag << FSR_TEM_SHIFT)) != 0UL;

	
	if (would_trap != 0) {
		eflag &= ((fsr & FSR_TEM_MASK) >> FSR_TEM_SHIFT);
		if ((eflag & (eflag - 1)) != 0) {
			if (eflag & FP_EX_INVALID)
				eflag = FP_EX_INVALID;
			else if (eflag & FP_EX_OVERFLOW)
				eflag = FP_EX_OVERFLOW;
			else if (eflag & FP_EX_UNDERFLOW)
				eflag = FP_EX_UNDERFLOW;
			else if (eflag & FP_EX_DIVZERO)
				eflag = FP_EX_DIVZERO;
			else if (eflag & FP_EX_INEXACT)
				eflag = FP_EX_INEXACT;
		}
	}

	fsr &= ~(FSR_CEXC_MASK);
	fsr |= ((long)eflag << FSR_CEXC_SHIFT);

	if (would_trap == 0)
		fsr |= ((long)eflag << FSR_AEXC_SHIFT);

	
	if (would_trap != 0)
		fsr |= (1UL << 14);

	*pfsr = fsr;

	return (would_trap ? 0 : 1);
}

typedef union {
	u32 s;
	u64 d;
	u64 q[2];
} *argp;

static int do_one_mathemu(u32 insn, unsigned long *pfsr, unsigned long *fregs)
{
	
	int type = 0;
#define TYPE(dummy, r, ru, b, bu, a, au) type = (au << 2) | (a << 0) | (bu << 5) | (b << 3) | (ru << 8) | (r << 6)
	int freg;
	argp rs1 = NULL, rs2 = NULL, rd = NULL;
	FP_DECL_EX;
	FP_DECL_S(SA); FP_DECL_S(SB); FP_DECL_S(SR);
	FP_DECL_D(DA); FP_DECL_D(DB); FP_DECL_D(DR);
	FP_DECL_Q(QA); FP_DECL_Q(QB); FP_DECL_Q(QR);
	int IR;
	long fsr;

#ifdef DEBUG_MATHEMU
	printk("In do_mathemu(), emulating %08lx\n", insn);
#endif

	if ((insn & 0xc1f80000) == 0x81a00000)	 {
		switch ((insn >> 5) & 0x1ff) {
		case FSQRTQ: TYPE(3,3,1,3,1,0,0); break;
		case FADDQ:
		case FSUBQ:
		case FMULQ:
		case FDIVQ: TYPE(3,3,1,3,1,3,1); break;
		case FDMULQ: TYPE(3,3,1,2,1,2,1); break;
		case FQTOS: TYPE(3,1,1,3,1,0,0); break;
		case FQTOD: TYPE(3,2,1,3,1,0,0); break;
		case FITOQ: TYPE(3,3,1,1,0,0,0); break;
		case FSTOQ: TYPE(3,3,1,1,1,0,0); break;
		case FDTOQ: TYPE(3,3,1,2,1,0,0); break;
		case FQTOI: TYPE(3,1,0,3,1,0,0); break;
		case FSQRTS: TYPE(2,1,1,1,1,0,0); break;
		case FSQRTD: TYPE(2,2,1,2,1,0,0); break;
		case FADDD:
		case FSUBD:
		case FMULD:
		case FDIVD: TYPE(2,2,1,2,1,2,1); break;
		case FADDS:
		case FSUBS:
		case FMULS:
		case FDIVS: TYPE(2,1,1,1,1,1,1); break;
		case FSMULD: TYPE(2,2,1,1,1,1,1); break;
		case FDTOS: TYPE(2,1,1,2,1,0,0); break;
		case FSTOD: TYPE(2,2,1,1,1,0,0); break;
		case FSTOI: TYPE(2,1,0,1,1,0,0); break;
		case FDTOI: TYPE(2,1,0,2,1,0,0); break;
		case FITOS: TYPE(2,1,1,1,0,0,0); break;
		case FITOD: TYPE(2,2,1,1,0,0,0); break;
		case FMOVS:
		case FABSS:
		case FNEGS: TYPE(2,1,0,1,0,0,0); break;
		}
	} else if ((insn & 0xc1f80000) == 0x81a80000)	 {
		switch ((insn >> 5) & 0x1ff) {
		case FCMPS: TYPE(3,0,0,1,1,1,1); break;
		case FCMPES: TYPE(3,0,0,1,1,1,1); break;
		case FCMPD: TYPE(3,0,0,2,1,2,1); break;
		case FCMPED: TYPE(3,0,0,2,1,2,1); break;
		case FCMPQ: TYPE(3,0,0,3,1,3,1); break;
		case FCMPEQ: TYPE(3,0,0,3,1,3,1); break;
		}
	}

	if (!type) {	
#ifdef DEBUG_MATHEMU
		printk("attempt to emulate unrecognised FPop!\n");
#endif
		return 0;
	}

	
	freg = (*pfsr >> 14) & 0xf;

	*pfsr &= ~0x1c000;				
	
	freg = ((insn >> 14) & 0x1f);
	switch (type & 0x3) {				
	case 3:
		if (freg & 3) {				
							
			*pfsr |= (6 << 14);
			return 0;			
		}
	
	case 2:
		if (freg & 1) {				
			*pfsr |= (6 << 14);
			return 0;
		}
	}
	rs1 = (argp)&fregs[freg];
	switch (type & 0x7) {
	case 7: FP_UNPACK_QP (QA, rs1); break;
	case 6: FP_UNPACK_DP (DA, rs1); break;
	case 5: FP_UNPACK_SP (SA, rs1); break;
	}
	freg = (insn & 0x1f);
	switch ((type >> 3) & 0x3) {			
	case 3:
		if (freg & 3) {				
							
			*pfsr |= (6 << 14);
			return 0;			
		}
	
	case 2:
		if (freg & 1) {				
			*pfsr |= (6 << 14);
			return 0;
		}
	}
	rs2 = (argp)&fregs[freg];
	switch ((type >> 3) & 0x7) {
	case 7: FP_UNPACK_QP (QB, rs2); break;
	case 6: FP_UNPACK_DP (DB, rs2); break;
	case 5: FP_UNPACK_SP (SB, rs2); break;
	}
	freg = ((insn >> 25) & 0x1f);
	switch ((type >> 6) & 0x3) {			
	case 0:						
		if (freg) {				
							
			*pfsr |= (6 << 14);		
			return 0;			
		}
		break;
	case 3:
		if (freg & 3) {				
							
			*pfsr |= (6 << 14);
			return 0;			
		}
	
	case 2:
		if (freg & 1) {				
			*pfsr |= (6 << 14);
			return 0;
		}
	
	case 1:
		rd = (void *)&fregs[freg];
		break;
	}
#ifdef DEBUG_MATHEMU
	printk("executing insn...\n");
#endif
	
	switch ((insn >> 5) & 0x1ff) {
	
	case FADDS: FP_ADD_S (SR, SA, SB); break;
	case FADDD: FP_ADD_D (DR, DA, DB); break;
	case FADDQ: FP_ADD_Q (QR, QA, QB); break;
	
	case FSUBS: FP_SUB_S (SR, SA, SB); break;
	case FSUBD: FP_SUB_D (DR, DA, DB); break;
	case FSUBQ: FP_SUB_Q (QR, QA, QB); break;
	
	case FMULS: FP_MUL_S (SR, SA, SB); break;
	case FSMULD: FP_CONV (D, S, 2, 1, DA, SA);
		     FP_CONV (D, S, 2, 1, DB, SB);
	case FMULD: FP_MUL_D (DR, DA, DB); break;
	case FDMULQ: FP_CONV (Q, D, 4, 2, QA, DA);
		     FP_CONV (Q, D, 4, 2, QB, DB);
	case FMULQ: FP_MUL_Q (QR, QA, QB); break;
	
	case FDIVS: FP_DIV_S (SR, SA, SB); break;
	case FDIVD: FP_DIV_D (DR, DA, DB); break;
	case FDIVQ: FP_DIV_Q (QR, QA, QB); break;
	
	case FSQRTS: FP_SQRT_S (SR, SB); break;
	case FSQRTD: FP_SQRT_D (DR, DB); break;
	case FSQRTQ: FP_SQRT_Q (QR, QB); break;
	
	case FMOVS: rd->s = rs2->s; break;
	case FABSS: rd->s = rs2->s & 0x7fffffff; break;
	case FNEGS: rd->s = rs2->s ^ 0x80000000; break;
	
	case FSTOI: FP_TO_INT_S (IR, SB, 32, 1); break;
	case FDTOI: FP_TO_INT_D (IR, DB, 32, 1); break;
	case FQTOI: FP_TO_INT_Q (IR, QB, 32, 1); break;
	
	case FITOS: IR = rs2->s; FP_FROM_INT_S (SR, IR, 32, int); break;
	case FITOD: IR = rs2->s; FP_FROM_INT_D (DR, IR, 32, int); break;
	case FITOQ: IR = rs2->s; FP_FROM_INT_Q (QR, IR, 32, int); break;
	
	case FSTOD: FP_CONV (D, S, 2, 1, DR, SB); break;
	case FSTOQ: FP_CONV (Q, S, 4, 1, QR, SB); break;
	case FDTOQ: FP_CONV (Q, D, 4, 2, QR, DB); break;
	case FDTOS: FP_CONV (S, D, 1, 2, SR, DB); break;
	case FQTOS: FP_CONV (S, Q, 1, 4, SR, QB); break;
	case FQTOD: FP_CONV (D, Q, 2, 4, DR, QB); break;
	
	case FCMPS:
	case FCMPES:
		FP_CMP_S(IR, SB, SA, 3);
		if (IR == 3 &&
		    (((insn >> 5) & 0x1ff) == FCMPES ||
		     FP_ISSIGNAN_S(SA) ||
		     FP_ISSIGNAN_S(SB)))
			FP_SET_EXCEPTION (FP_EX_INVALID);
		break;
	case FCMPD:
	case FCMPED:
		FP_CMP_D(IR, DB, DA, 3);
		if (IR == 3 &&
		    (((insn >> 5) & 0x1ff) == FCMPED ||
		     FP_ISSIGNAN_D(DA) ||
		     FP_ISSIGNAN_D(DB)))
			FP_SET_EXCEPTION (FP_EX_INVALID);
		break;
	case FCMPQ:
	case FCMPEQ:
		FP_CMP_Q(IR, QB, QA, 3);
		if (IR == 3 &&
		    (((insn >> 5) & 0x1ff) == FCMPEQ ||
		     FP_ISSIGNAN_Q(QA) ||
		     FP_ISSIGNAN_Q(QB)))
			FP_SET_EXCEPTION (FP_EX_INVALID);
	}
	if (!FP_INHIBIT_RESULTS) {
		switch ((type >> 6) & 0x7) {
		case 0: fsr = *pfsr;
			if (IR == -1) IR = 2;
			
			fsr &= ~0xc00; fsr |= (IR << 10); break;
			*pfsr = fsr;
			break;
		case 1: rd->s = IR; break;
		case 5: FP_PACK_SP (rd, SR); break;
		case 6: FP_PACK_DP (rd, DR); break;
		case 7: FP_PACK_QP (rd, QR); break;
		}
	}
	if (_fex == 0)
		return 1;				
	return record_exception(pfsr, _fex);
}
