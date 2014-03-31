/*---------------------------------------------------------------------------+
 |  errors.c                                                                 |
 |                                                                           |
 |  The error handling functions for wm-FPU-emu                              |
 |                                                                           |
 | Copyright (C) 1992,1993,1994,1996                                         |
 |                  W. Metzenthen, 22 Parker St, Ormond, Vic 3163, Australia |
 |                  E-mail   billm@jacobi.maths.monash.edu.au                |
 |                                                                           |
 |                                                                           |
 +---------------------------------------------------------------------------*/


#include <linux/signal.h>

#include <asm/uaccess.h>

#include "fpu_emu.h"
#include "fpu_system.h"
#include "exception.h"
#include "status_w.h"
#include "control_w.h"
#include "reg_constant.h"
#include "version.h"

#undef PRINT_MESSAGES

#if 0
void Un_impl(void)
{
	u_char byte1, FPU_modrm;
	unsigned long address = FPU_ORIG_EIP;

	RE_ENTRANT_CHECK_OFF;
	
	printk("Unimplemented FPU Opcode at eip=%p : ", (void __user *)address);
	if (FPU_CS == __USER_CS) {
		while (1) {
			FPU_get_user(byte1, (u_char __user *) address);
			if ((byte1 & 0xf8) == 0xd8)
				break;
			printk("[%02x]", byte1);
			address++;
		}
		printk("%02x ", byte1);
		FPU_get_user(FPU_modrm, 1 + (u_char __user *) address);

		if (FPU_modrm >= 0300)
			printk("%02x (%02x+%d)\n", FPU_modrm, FPU_modrm & 0xf8,
			       FPU_modrm & 7);
		else
			printk("/%d\n", (FPU_modrm >> 3) & 7);
	} else {
		printk("cs selector = %04x\n", FPU_CS);
	}

	RE_ENTRANT_CHECK_ON;

	EXCEPTION(EX_Invalid);

}
#endif 

void FPU_illegal(void)
{
	math_abort(FPU_info, SIGILL);
}

void FPU_printall(void)
{
	int i;
	static const char *tag_desc[] = { "Valid", "Zero", "ERROR", "Empty",
		"DeNorm", "Inf", "NaN"
	};
	u_char byte1, FPU_modrm;
	unsigned long address = FPU_ORIG_EIP;

	RE_ENTRANT_CHECK_OFF;
	
	printk("At %p:", (void *)address);
	if (FPU_CS == __USER_CS) {
#define MAX_PRINTED_BYTES 20
		for (i = 0; i < MAX_PRINTED_BYTES; i++) {
			FPU_get_user(byte1, (u_char __user *) address);
			if ((byte1 & 0xf8) == 0xd8) {
				printk(" %02x", byte1);
				break;
			}
			printk(" [%02x]", byte1);
			address++;
		}
		if (i == MAX_PRINTED_BYTES)
			printk(" [more..]\n");
		else {
			FPU_get_user(FPU_modrm, 1 + (u_char __user *) address);

			if (FPU_modrm >= 0300)
				printk(" %02x (%02x+%d)\n", FPU_modrm,
				       FPU_modrm & 0xf8, FPU_modrm & 7);
			else
				printk(" /%d, mod=%d rm=%d\n",
				       (FPU_modrm >> 3) & 7,
				       (FPU_modrm >> 6) & 3, FPU_modrm & 7);
		}
	} else {
		printk("%04x\n", FPU_CS);
	}

	partial_status = status_word();

#ifdef DEBUGGING
	if (partial_status & SW_Backward)
		printk("SW: backward compatibility\n");
	if (partial_status & SW_C3)
		printk("SW: condition bit 3\n");
	if (partial_status & SW_C2)
		printk("SW: condition bit 2\n");
	if (partial_status & SW_C1)
		printk("SW: condition bit 1\n");
	if (partial_status & SW_C0)
		printk("SW: condition bit 0\n");
	if (partial_status & SW_Summary)
		printk("SW: exception summary\n");
	if (partial_status & SW_Stack_Fault)
		printk("SW: stack fault\n");
	if (partial_status & SW_Precision)
		printk("SW: loss of precision\n");
	if (partial_status & SW_Underflow)
		printk("SW: underflow\n");
	if (partial_status & SW_Overflow)
		printk("SW: overflow\n");
	if (partial_status & SW_Zero_Div)
		printk("SW: divide by zero\n");
	if (partial_status & SW_Denorm_Op)
		printk("SW: denormalized operand\n");
	if (partial_status & SW_Invalid)
		printk("SW: invalid operation\n");
#endif 

	printk(" SW: b=%d st=%d es=%d sf=%d cc=%d%d%d%d ef=%d%d%d%d%d%d\n", partial_status & 0x8000 ? 1 : 0,	
	       (partial_status & 0x3800) >> 11,	
	       partial_status & 0x80 ? 1 : 0,	
	       partial_status & 0x40 ? 1 : 0,	
	       partial_status & SW_C3 ? 1 : 0, partial_status & SW_C2 ? 1 : 0,	
	       partial_status & SW_C1 ? 1 : 0, partial_status & SW_C0 ? 1 : 0,	
	       partial_status & SW_Precision ? 1 : 0,
	       partial_status & SW_Underflow ? 1 : 0,
	       partial_status & SW_Overflow ? 1 : 0,
	       partial_status & SW_Zero_Div ? 1 : 0,
	       partial_status & SW_Denorm_Op ? 1 : 0,
	       partial_status & SW_Invalid ? 1 : 0);

	printk(" CW: ic=%d rc=%d%d pc=%d%d iem=%d     ef=%d%d%d%d%d%d\n",
	       control_word & 0x1000 ? 1 : 0,
	       (control_word & 0x800) >> 11, (control_word & 0x400) >> 10,
	       (control_word & 0x200) >> 9, (control_word & 0x100) >> 8,
	       control_word & 0x80 ? 1 : 0,
	       control_word & SW_Precision ? 1 : 0,
	       control_word & SW_Underflow ? 1 : 0,
	       control_word & SW_Overflow ? 1 : 0,
	       control_word & SW_Zero_Div ? 1 : 0,
	       control_word & SW_Denorm_Op ? 1 : 0,
	       control_word & SW_Invalid ? 1 : 0);

	for (i = 0; i < 8; i++) {
		FPU_REG *r = &st(i);
		u_char tagi = FPU_gettagi(i);
		switch (tagi) {
		case TAG_Empty:
			continue;
			break;
		case TAG_Zero:
		case TAG_Special:
			tagi = FPU_Special(r);
		case TAG_Valid:
			printk("st(%d)  %c .%04lx %04lx %04lx %04lx e%+-6d ", i,
			       getsign(r) ? '-' : '+',
			       (long)(r->sigh >> 16),
			       (long)(r->sigh & 0xFFFF),
			       (long)(r->sigl >> 16),
			       (long)(r->sigl & 0xFFFF),
			       exponent(r) - EXP_BIAS + 1);
			break;
		default:
			printk("Whoops! Error in errors.c: tag%d is %d ", i,
			       tagi);
			continue;
			break;
		}
		printk("%s\n", tag_desc[(int)(unsigned)tagi]);
	}

	RE_ENTRANT_CHECK_ON;

}

static struct {
	int type;
	const char *name;
} exception_names[] = {
	{
	EX_StackOver, "stack overflow"}, {
	EX_StackUnder, "stack underflow"}, {
	EX_Precision, "loss of precision"}, {
	EX_Underflow, "underflow"}, {
	EX_Overflow, "overflow"}, {
	EX_ZeroDiv, "divide by zero"}, {
	EX_Denormal, "denormalized operand"}, {
	EX_Invalid, "invalid operation"}, {
	EX_INTERNAL, "INTERNAL BUG in " FPU_VERSION}, {
	0, NULL}
};


asmlinkage void FPU_exception(int n)
{
	int i, int_type;

	int_type = 0;		
	if (n & EX_INTERNAL) {
		int_type = n - EX_INTERNAL;
		n = EX_INTERNAL;
		
		partial_status |= (SW_Exc_Mask | SW_Summary | SW_Backward);
	} else {
		
		n &= (SW_Exc_Mask);
		
		partial_status |= n;
		
		if (partial_status & ~control_word & CW_Exceptions)
			partial_status |= (SW_Summary | SW_Backward);
		if (n & (SW_Stack_Fault | EX_Precision)) {
			if (!(n & SW_C1))
				partial_status &= ~SW_C1;
		}
	}

	RE_ENTRANT_CHECK_OFF;
	if ((~control_word & n & CW_Exceptions) || (n == EX_INTERNAL)) {
#ifdef PRINT_MESSAGES
		
		printk(FPU_VERSION " " __DATE__ " (C) W. Metzenthen.\n");
#endif 

		
		for (i = 0; exception_names[i].type; i++)
			if ((exception_names[i].type & n) ==
			    exception_names[i].type)
				break;

		if (exception_names[i].type) {
#ifdef PRINT_MESSAGES
			printk("FP Exception: %s!\n", exception_names[i].name);
#endif 
		} else
			printk("FPU emulator: Unknown Exception: 0x%04x!\n", n);

		if (n == EX_INTERNAL) {
			printk("FPU emulator: Internal error type 0x%04x\n",
			       int_type);
			FPU_printall();
		}
#ifdef PRINT_MESSAGES
		else
			FPU_printall();
#endif 

	}
	RE_ENTRANT_CHECK_ON;

#ifdef __DEBUG__
	math_abort(FPU_info, SIGFPE);
#endif 

}

int real_1op_NaN(FPU_REG *a)
{
	int signalling, isNaN;

	isNaN = (exponent(a) == EXP_OVER) && (a->sigh & 0x80000000);

	signalling = isNaN && !(a->sigh & 0x40000000);

	if (!signalling) {
		if (!isNaN) {	
			if (control_word & CW_Invalid) {
				
				reg_copy(&CONST_QNaN, a);
			}
			EXCEPTION(EX_Invalid);
			return (!(control_word & CW_Invalid) ? FPU_Exception :
				0) | TAG_Special;
		}
		return TAG_Special;
	}

	if (control_word & CW_Invalid) {
		
		if (!(a->sigh & 0x80000000)) {	
			reg_copy(&CONST_QNaN, a);
		}
		
		a->sigh |= 0x40000000;
	}

	EXCEPTION(EX_Invalid);

	return (!(control_word & CW_Invalid) ? FPU_Exception : 0) | TAG_Special;
}

int real_2op_NaN(FPU_REG const *b, u_char tagb,
		 int deststnr, FPU_REG const *defaultNaN)
{
	FPU_REG *dest = &st(deststnr);
	FPU_REG const *a = dest;
	u_char taga = FPU_gettagi(deststnr);
	FPU_REG const *x;
	int signalling, unsupported;

	if (taga == TAG_Special)
		taga = FPU_Special(a);
	if (tagb == TAG_Special)
		tagb = FPU_Special(b);

	
	unsupported = ((taga == TW_NaN)
		       && !((exponent(a) == EXP_OVER)
			    && (a->sigh & 0x80000000)))
	    || ((tagb == TW_NaN)
		&& !((exponent(b) == EXP_OVER) && (b->sigh & 0x80000000)));
	if (unsupported) {
		if (control_word & CW_Invalid) {
			
			FPU_copy_to_regi(&CONST_QNaN, TAG_Special, deststnr);
		}
		EXCEPTION(EX_Invalid);
		return (!(control_word & CW_Invalid) ? FPU_Exception : 0) |
		    TAG_Special;
	}

	if (taga == TW_NaN) {
		x = a;
		if (tagb == TW_NaN) {
			signalling = !(a->sigh & b->sigh & 0x40000000);
			if (significand(b) > significand(a))
				x = b;
			else if (significand(b) == significand(a)) {
				x = defaultNaN;
			}
		} else {
			
			signalling = !(a->sigh & 0x40000000);
		}
	} else
#ifdef PARANOID
	if (tagb == TW_NaN)
#endif 
	{
		signalling = !(b->sigh & 0x40000000);
		x = b;
	}
#ifdef PARANOID
	else {
		signalling = 0;
		EXCEPTION(EX_INTERNAL | 0x113);
		x = &CONST_QNaN;
	}
#endif 

	if ((!signalling) || (control_word & CW_Invalid)) {
		if (!x)
			x = b;

		if (!(x->sigh & 0x80000000))	
			x = &CONST_QNaN;

		FPU_copy_to_regi(x, TAG_Special, deststnr);

		if (!signalling)
			return TAG_Special;

		
		dest->sigh |= 0x40000000;
	}

	EXCEPTION(EX_Invalid);

	return (!(control_word & CW_Invalid) ? FPU_Exception : 0) | TAG_Special;
}

asmlinkage int arith_invalid(int deststnr)
{

	EXCEPTION(EX_Invalid);

	if (control_word & CW_Invalid) {
		
		FPU_copy_to_regi(&CONST_QNaN, TAG_Special, deststnr);
	}

	return (!(control_word & CW_Invalid) ? FPU_Exception : 0) | TAG_Valid;

}

asmlinkage int FPU_divide_by_zero(int deststnr, u_char sign)
{
	FPU_REG *dest = &st(deststnr);
	int tag = TAG_Valid;

	if (control_word & CW_ZeroDiv) {
		
		FPU_copy_to_regi(&CONST_INF, TAG_Special, deststnr);
		setsign(dest, sign);
		tag = TAG_Special;
	}

	EXCEPTION(EX_ZeroDiv);

	return (!(control_word & CW_ZeroDiv) ? FPU_Exception : 0) | tag;

}

int set_precision_flag(int flags)
{
	if (control_word & CW_Precision) {
		partial_status &= ~(SW_C1 & flags);
		partial_status |= flags;	
		return 0;
	} else {
		EXCEPTION(flags);
		return 1;
	}
}

asmlinkage void set_precision_flag_up(void)
{
	if (control_word & CW_Precision)
		partial_status |= (SW_Precision | SW_C1);	
	else
		EXCEPTION(EX_Precision | SW_C1);
}

asmlinkage void set_precision_flag_down(void)
{
	if (control_word & CW_Precision) {	
		partial_status &= ~SW_C1;
		partial_status |= SW_Precision;
	} else
		EXCEPTION(EX_Precision);
}

asmlinkage int denormal_operand(void)
{
	if (control_word & CW_Denormal) {	
		partial_status |= SW_Denorm_Op;
		return TAG_Special;
	} else {
		EXCEPTION(EX_Denormal);
		return TAG_Special | FPU_Exception;
	}
}

asmlinkage int arith_overflow(FPU_REG *dest)
{
	int tag = TAG_Valid;

	if (control_word & CW_Overflow) {
		
		reg_copy(&CONST_INF, dest);
		tag = TAG_Special;
	} else {
		
		addexponent(dest, (-3 * (1 << 13)));
	}

	EXCEPTION(EX_Overflow);
	if (control_word & CW_Overflow) {
		
		EXCEPTION(EX_Precision | SW_C1);
		return tag;
	}

	return tag;

}

asmlinkage int arith_underflow(FPU_REG *dest)
{
	int tag = TAG_Valid;

	if (control_word & CW_Underflow) {
		
		if (exponent16(dest) <= EXP_UNDER - 63) {
			reg_copy(&CONST_Z, dest);
			partial_status &= ~SW_C1;	
			tag = TAG_Zero;
		} else {
			stdexp(dest);
		}
	} else {
		
		addexponent(dest, (3 * (1 << 13)) + EXTENDED_Ebias);
	}

	EXCEPTION(EX_Underflow);
	if (control_word & CW_Underflow) {
		
		EXCEPTION(EX_Precision);
		return tag;
	}

	return tag;

}

void FPU_stack_overflow(void)
{

	if (control_word & CW_Invalid) {
		
		top--;
		FPU_copy_to_reg0(&CONST_QNaN, TAG_Special);
	}

	EXCEPTION(EX_StackOver);

	return;

}

void FPU_stack_underflow(void)
{

	if (control_word & CW_Invalid) {
		
		FPU_copy_to_reg0(&CONST_QNaN, TAG_Special);
	}

	EXCEPTION(EX_StackUnder);

	return;

}

void FPU_stack_underflow_i(int i)
{

	if (control_word & CW_Invalid) {
		
		FPU_copy_to_regi(&CONST_QNaN, TAG_Special, i);
	}

	EXCEPTION(EX_StackUnder);

	return;

}

void FPU_stack_underflow_pop(int i)
{

	if (control_word & CW_Invalid) {
		
		FPU_copy_to_regi(&CONST_QNaN, TAG_Special, i);
		FPU_pop();
	}

	EXCEPTION(EX_StackUnder);

	return;

}
