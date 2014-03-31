/*---------------------------------------------------------------------------+
 |  reg_mul.c                                                                |
 |                                                                           |
 | Multiply one FPU_REG by another, put the result in a destination FPU_REG. |
 |                                                                           |
 | Copyright (C) 1992,1993,1997                                              |
 |                  W. Metzenthen, 22 Parker St, Ormond, Vic 3163, Australia |
 |                  E-mail   billm@suburbia.net                              |
 |                                                                           |
 | Returns the tag of the result if no exceptions or errors occurred.        |
 |                                                                           |
 +---------------------------------------------------------------------------*/


#include "fpu_emu.h"
#include "exception.h"
#include "reg_constant.h"
#include "fpu_system.h"

int FPU_mul(FPU_REG const *b, u_char tagb, int deststnr, int control_w)
{
	FPU_REG *a = &st(deststnr);
	FPU_REG *dest = a;
	u_char taga = FPU_gettagi(deststnr);
	u_char saved_sign = getsign(dest);
	u_char sign = (getsign(a) ^ getsign(b));
	int tag;

	if (!(taga | tagb)) {
		

		tag =
		    FPU_u_mul(a, b, dest, control_w, sign,
			      exponent(a) + exponent(b));
		if (tag < 0) {
			setsign(dest, saved_sign);
			return tag;
		}
		FPU_settagi(deststnr, tag);
		return tag;
	}

	if (taga == TAG_Special)
		taga = FPU_Special(a);
	if (tagb == TAG_Special)
		tagb = FPU_Special(b);

	if (((taga == TAG_Valid) && (tagb == TW_Denormal))
	    || ((taga == TW_Denormal) && (tagb == TAG_Valid))
	    || ((taga == TW_Denormal) && (tagb == TW_Denormal))) {
		FPU_REG x, y;
		if (denormal_operand() < 0)
			return FPU_Exception;

		FPU_to_exp16(a, &x);
		FPU_to_exp16(b, &y);
		tag = FPU_u_mul(&x, &y, dest, control_w, sign,
				exponent16(&x) + exponent16(&y));
		if (tag < 0) {
			setsign(dest, saved_sign);
			return tag;
		}
		FPU_settagi(deststnr, tag);
		return tag;
	} else if ((taga <= TW_Denormal) && (tagb <= TW_Denormal)) {
		if (((tagb == TW_Denormal) || (taga == TW_Denormal))
		    && (denormal_operand() < 0))
			return FPU_Exception;

		FPU_copy_to_regi(&CONST_Z, TAG_Zero, deststnr);
		setsign(dest, sign);
		return TAG_Zero;
	}
	
	else if ((taga == TW_NaN) || (tagb == TW_NaN)) {
		return real_2op_NaN(b, tagb, deststnr, &st(0));
	} else if (((taga == TW_Infinity) && (tagb == TAG_Zero))
		   || ((tagb == TW_Infinity) && (taga == TAG_Zero))) {
		return arith_invalid(deststnr);	
	} else if (((taga == TW_Denormal) || (tagb == TW_Denormal))
		   && (denormal_operand() < 0)) {
		return FPU_Exception;
	} else if (taga == TW_Infinity) {
		FPU_copy_to_regi(a, TAG_Special, deststnr);
		setsign(dest, sign);
		return TAG_Special;
	} else if (tagb == TW_Infinity) {
		FPU_copy_to_regi(b, TAG_Special, deststnr);
		setsign(dest, sign);
		return TAG_Special;
	}
#ifdef PARANOID
	else {
		EXCEPTION(EX_INTERNAL | 0x102);
		return FPU_Exception;
	}
#endif 

	return 0;
}
