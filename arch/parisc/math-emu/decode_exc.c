/*
 * Linux/PA-RISC Project (http://www.parisc-linux.org/)
 *
 * Floating-point emulation code
 *  Copyright (C) 2001 Hewlett-Packard (Paul Bame) <bame@debian.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include "float.h"
#include "sgl_float.h"
#include "dbl_float.h"
#include "cnv_float.h"
#include <asm/signal.h>
#include <asm/siginfo.h>

#undef Fpustatus_register
#define Fpustatus_register Fpu_register[0]

#define DOESTRAP 1
#define NOTRAP 0
#define SIGNALCODE(signal, code) ((signal) << 24 | (code))
#define copropbit	1<<31-2	
#define opclass		9	
#define fmt		11	
#define df		13	
#define twobits		3	
#define fivebits	31	
#define MAX_EXCP_REG	7	

#define Excp_type(index) Exceptiontype(Fpu_register[index])
#define Excp_instr(index) Instructionfield(Fpu_register[index])
#define Clear_excp_register(index) Allexception(Fpu_register[index]) = 0
#define Excp_format() \
    (current_ir >> ((current_ir>>opclass & twobits)==1 ? df : fmt) & twobits)

#define Fpu_sgl(index) Fpu_register[index*2]

#define Fpu_dblp1(index) Fpu_register[index*2]
#define Fpu_dblp2(index) Fpu_register[(index*2)+1]

#define Fpu_quadp1(index) Fpu_register[index*2]
#define Fpu_quadp2(index) Fpu_register[(index*2)+1]
#define Fpu_quadp3(index) Fpu_register[(index*2)+2]
#define Fpu_quadp4(index) Fpu_register[(index*2)+3]

#ifndef Sgl_decrement
# define Sgl_decrement(sgl_value) Sall(sgl_value)--
#endif

#ifndef Dbl_decrement
# define Dbl_decrement(dbl_valuep1,dbl_valuep2) \
    if ((Dallp2(dbl_valuep2)--) == 0) Dallp1(dbl_valuep1)-- 
#endif


#define update_trap_counts(Fpu_register, aflags, bflags, trap_counts) {	\
	aflags=(Fpu_register[0])>>27;		\
	Fpu_register[0] |= bflags;					\
}

u_int
decode_fpu(unsigned int Fpu_register[], unsigned int trap_counts[])
{
    unsigned int current_ir, excp;
    int target, exception_index = 1;
    boolean inexact;
    unsigned int aflags;
    unsigned int bflags;
    unsigned int excptype;



    bflags=(Fpu_register[0] & 0xf8000000);
    Fpu_register[0] &= 0x07ffffff;


    
    if (!Is_tbit_set()) {
	update_trap_counts(Fpu_register, aflags, bflags, trap_counts);
	return SIGNALCODE(SIGILL, ILL_COPROC);
    }

    for (exception_index=1; exception_index<=MAX_EXCP_REG; exception_index++) {
	current_ir = Excp_instr(exception_index);
	excptype = Excp_type(exception_index);
	if (excptype & UNIMPLEMENTEDEXCEPTION) {
		Clear_tbit();
		Clear_excp_register(exception_index);
		excp = fpudispatch(current_ir,excptype,0,Fpu_register);
		
		if (excp) {
			Set_tbit();
			Set_exceptiontype_and_instr_field(excp,current_ir,
			 Fpu_register[exception_index]);
			if (excp == UNIMPLEMENTEDEXCEPTION) {
				excp = excptype;
				update_trap_counts(Fpu_register, aflags, bflags, 
					   trap_counts);
				return SIGNALCODE(SIGILL, ILL_COPROC);
			}
			excptype = excp;
		}
		if (excp == NOEXCEPTION)
			
			break;
	}

	if (excptype & UNDERFLOWEXCEPTION) {
		
		if (Is_underflowtrap_enabled()) {
			update_trap_counts(Fpu_register, aflags, bflags, 
					   trap_counts);
			return SIGNALCODE(SIGFPE, FPE_FLTUND);
		} else {
		    target = current_ir & fivebits;
#ifndef lint
		    if (Ibit(Fpu_register[exception_index])) inexact = TRUE;
		    else inexact = FALSE;
#endif
		    switch (Excp_format()) {
		      case SGL:
		        if (Rabit(Fpu_register[exception_index])) 
				Sgl_decrement(Fpu_sgl(target));

			
			sgl_denormalize(&Fpu_sgl(target),&inexact,Rounding_mode());
		    	break;
		      case DBL:
		    	if (Rabit(Fpu_register[exception_index])) 
				Dbl_decrement(Fpu_dblp1(target),Fpu_dblp2(target));

			
			dbl_denormalize(&Fpu_dblp1(target),&Fpu_dblp2(target),
			  &inexact,Rounding_mode());
		    	break;
		    }
		    if (inexact) Set_underflowflag();
		    if (inexact && Is_inexacttrap_enabled()) {
		    	Set_exceptiontype(Fpu_register[exception_index],
			 INEXACTEXCEPTION);
			Set_parmfield(Fpu_register[exception_index],0);
			update_trap_counts(Fpu_register, aflags, bflags, 
					   trap_counts);
			return SIGNALCODE(SIGFPE, FPE_FLTRES);
		    }
		    else {
		    	Clear_excp_register(exception_index);
		    	if (inexact) Set_inexactflag();
		    }
		}
		continue;
	}
	switch(Excp_type(exception_index)) {
	  case OVERFLOWEXCEPTION:
	  case OVERFLOWEXCEPTION | INEXACTEXCEPTION:
		
			update_trap_counts(Fpu_register, aflags, bflags, 
					   trap_counts);
		if (Is_overflowtrap_enabled()) {
			update_trap_counts(Fpu_register, aflags, bflags, 
					   trap_counts);
			return SIGNALCODE(SIGFPE, FPE_FLTOVF);
		} else {
			target = current_ir & fivebits;
			switch (Excp_format()) {
			  case SGL: 
				Sgl_setoverflow(Fpu_sgl(target));
				break;
			  case DBL:
				Dbl_setoverflow(Fpu_dblp1(target),Fpu_dblp2(target));
				break;
			}
			Set_overflowflag();
			if (Is_inexacttrap_enabled()) {
				Set_exceptiontype(Fpu_register[exception_index],
				 INEXACTEXCEPTION);
				update_trap_counts(Fpu_register, aflags, bflags,
					   trap_counts);
				return SIGNALCODE(SIGFPE, FPE_FLTRES);
			}
			else {
				Clear_excp_register(exception_index);
				Set_inexactflag();
			}
		}
		break;
	  case INVALIDEXCEPTION:
	  case OPC_2E_INVALIDEXCEPTION:
		update_trap_counts(Fpu_register, aflags, bflags, trap_counts);
		return SIGNALCODE(SIGFPE, FPE_FLTINV);
	  case DIVISIONBYZEROEXCEPTION:
		update_trap_counts(Fpu_register, aflags, bflags, trap_counts);
		Clear_excp_register(exception_index);
	  	return SIGNALCODE(SIGFPE, FPE_FLTDIV);
	  case INEXACTEXCEPTION:
		update_trap_counts(Fpu_register, aflags, bflags, trap_counts);
		return SIGNALCODE(SIGFPE, FPE_FLTRES);
	  default:
		update_trap_counts(Fpu_register, aflags, bflags, trap_counts);
		printk("%s(%d) Unknown FPU exception 0x%x\n", __FILE__,
			__LINE__, Excp_type(exception_index));
		return SIGNALCODE(SIGILL, ILL_COPROC);
	  case NOEXCEPTION:	
		Clear_excp_register(exception_index);
		break;
	}
    }
    Clear_tbit();
    update_trap_counts(Fpu_register, aflags, bflags, trap_counts);
    return(NOTRAP);
}
