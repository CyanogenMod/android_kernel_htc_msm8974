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


#include "float.h"
#include "sgl_float.h"


int
sgl_fmpy(
    sgl_floating_point *srcptr1,
    sgl_floating_point *srcptr2,
    sgl_floating_point *dstptr,
    unsigned int *status)
{
	register unsigned int opnd1, opnd2, opnd3, result;
	register int dest_exponent, count;
	register boolean inexact = FALSE, guardbit = FALSE, stickybit = FALSE;
	boolean is_tiny;

	opnd1 = *srcptr1;
	opnd2 = *srcptr2;
	if (Sgl_sign(opnd1) ^ Sgl_sign(opnd2)) Sgl_setnegativezero(result);  
	else Sgl_setzero(result);
	if (Sgl_isinfinity_exponent(opnd1)) {
		if (Sgl_iszero_mantissa(opnd1)) {
			if (Sgl_isnotnan(opnd2)) {
				if (Sgl_iszero_exponentmantissa(opnd2)) {
					if (Is_invalidtrap_enabled()) 
                                		return(INVALIDEXCEPTION);
                                	Set_invalidflag();
                                	Sgl_makequietnan(result);
					*dstptr = result;
					return(NOEXCEPTION);
				}
				Sgl_setinfinity_exponentmantissa(result);
				*dstptr = result;
				return(NOEXCEPTION);
			}
		}
		else {
                	if (Sgl_isone_signaling(opnd1)) {
                        	
                        	if (Is_invalidtrap_enabled()) 
                            		return(INVALIDEXCEPTION);
                        	
                        	Set_invalidflag();
                        	Sgl_set_quiet(opnd1);
                	}
			else if (Sgl_is_signalingnan(opnd2)) {
                        	
                        	if (Is_invalidtrap_enabled()) 
                            		return(INVALIDEXCEPTION);
                        	
                        	Set_invalidflag();
                        	Sgl_set_quiet(opnd2);
                		*dstptr = opnd2;
                		return(NOEXCEPTION);
			}
                	*dstptr = opnd1;
                	return(NOEXCEPTION);
		}
	}
	if (Sgl_isinfinity_exponent(opnd2)) {
		if (Sgl_iszero_mantissa(opnd2)) {
			if (Sgl_iszero_exponentmantissa(opnd1)) {
				
				if (Is_invalidtrap_enabled()) 
                                	return(INVALIDEXCEPTION);
                                Set_invalidflag();
                                Sgl_makequietnan(opnd2);
				*dstptr = opnd2;
				return(NOEXCEPTION);
			}
			Sgl_setinfinity_exponentmantissa(result);
			*dstptr = result;
			return(NOEXCEPTION);
		}
                if (Sgl_isone_signaling(opnd2)) {
                        
                        if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);

                        
                        Set_invalidflag();
                        Sgl_set_quiet(opnd2);
                }
                *dstptr = opnd2;
                return(NOEXCEPTION);
	}
	dest_exponent = Sgl_exponent(opnd1) + Sgl_exponent(opnd2) - SGL_BIAS;

	if (Sgl_isnotzero_exponent(opnd1)) {
		
		Sgl_clear_signexponent_set_hidden(opnd1);
	}
	else {
		
		if (Sgl_iszero_mantissa(opnd1)) {
			Sgl_setzero_exponentmantissa(result);
			*dstptr = result;
			return(NOEXCEPTION);
		}
                
                Sgl_clear_signexponent(opnd1);
		Sgl_leftshiftby1(opnd1);
		Sgl_normalize(opnd1,dest_exponent);
	}
	
	if (Sgl_isnotzero_exponent(opnd2)) {
		Sgl_clear_signexponent_set_hidden(opnd2);
	}
	else {
		
		if (Sgl_iszero_mantissa(opnd2)) {
			Sgl_setzero_exponentmantissa(result);
			*dstptr = result;
			return(NOEXCEPTION);
		}
                
                Sgl_clear_signexponent(opnd2);
                Sgl_leftshiftby1(opnd2);
		Sgl_normalize(opnd2,dest_exponent);
	}

	

	Sgl_leftshiftby4(opnd2);     
	Sgl_setzero(opnd3);
	for (count=1;count<SGL_P;count+=4) {
		stickybit |= Slow4(opnd3);
		Sgl_rightshiftby4(opnd3);
		if (Sbit28(opnd1)) Sall(opnd3) += (Sall(opnd2) << 3);
		if (Sbit29(opnd1)) Sall(opnd3) += (Sall(opnd2) << 2);
		if (Sbit30(opnd1)) Sall(opnd3) += (Sall(opnd2) << 1);
		if (Sbit31(opnd1)) Sall(opnd3) += Sall(opnd2);
		Sgl_rightshiftby4(opnd1);
	}
	
	if (Sgl_iszero_sign(opnd3)) {
		Sgl_leftshiftby1(opnd3);
	}
	else {
		
		dest_exponent++;
	}
	
	while (Sgl_iszero_sign(opnd3)) {
		Sgl_leftshiftby1(opnd3);
		dest_exponent--;
	}
	stickybit |= Sgl_all(opnd3) << (SGL_BITLENGTH - SGL_EXP_LENGTH + 1);
	guardbit = Sbit24(opnd3);
	inexact = guardbit | stickybit;

	
	Sgl_rightshiftby8(opnd3);

	if (inexact && (dest_exponent>0 || Is_underflowtrap_enabled())) {
		Sgl_clear_signexponent(opnd3);
		switch (Rounding_mode()) {
			case ROUNDPLUS: 
				if (Sgl_iszero_sign(result)) 
					Sgl_increment(opnd3);
				break;
			case ROUNDMINUS: 
				if (Sgl_isone_sign(result)) 
					Sgl_increment(opnd3);
				break;
			case ROUNDNEAREST:
				if (guardbit) {
			   	if (stickybit || Sgl_isone_lowmantissa(opnd3))
			      	Sgl_increment(opnd3);
				}
		}
		if (Sgl_isone_hidden(opnd3)) dest_exponent++;
	}
	Sgl_set_mantissa(result,opnd3);

	if (dest_exponent >= SGL_INFINITY_EXPONENT) {
                
                if (Is_overflowtrap_enabled()) {
			Sgl_setwrapped_exponent(result,dest_exponent,ovfl);
			*dstptr = result;
			if (inexact) 
			    if (Is_inexacttrap_enabled())
				return(OVERFLOWEXCEPTION | INEXACTEXCEPTION);
			    else Set_inexactflag();
			return(OVERFLOWEXCEPTION);
                }
		inexact = TRUE;
		Set_overflowflag();
                
		Sgl_setoverflow(result);
	}
	else if (dest_exponent <= 0) {
                
                if (Is_underflowtrap_enabled()) {
			Sgl_setwrapped_exponent(result,dest_exponent,unfl);
			*dstptr = result;
			if (inexact) 
			    if (Is_inexacttrap_enabled())
				return(UNDERFLOWEXCEPTION | INEXACTEXCEPTION);
			    else Set_inexactflag();
			return(UNDERFLOWEXCEPTION);
                }

		
		is_tiny = TRUE;
		if (dest_exponent == 0 && inexact) {
			switch (Rounding_mode()) {
			case ROUNDPLUS: 
				if (Sgl_iszero_sign(result)) {
					Sgl_increment(opnd3);
					if (Sgl_isone_hiddenoverflow(opnd3))
                			    is_tiny = FALSE;
					Sgl_decrement(opnd3);
				}
				break;
			case ROUNDMINUS: 
				if (Sgl_isone_sign(result)) {
					Sgl_increment(opnd3);
					if (Sgl_isone_hiddenoverflow(opnd3))
                			    is_tiny = FALSE;
					Sgl_decrement(opnd3);
				}
				break;
			case ROUNDNEAREST:
				if (guardbit && (stickybit || 
				    Sgl_isone_lowmantissa(opnd3))) {
				      	Sgl_increment(opnd3);
					if (Sgl_isone_hiddenoverflow(opnd3))
                			    is_tiny = FALSE;
					Sgl_decrement(opnd3);
				}
				break;
			}
		}

		stickybit = inexact;
		Sgl_denormalize(opnd3,dest_exponent,guardbit,stickybit,inexact);

		
		if (inexact) {
			switch (Rounding_mode()) {
			case ROUNDPLUS: 
				if (Sgl_iszero_sign(result)) {
					Sgl_increment(opnd3);
				}
				break;
			case ROUNDMINUS: 
				if (Sgl_isone_sign(result)) {
					Sgl_increment(opnd3);
				}
				break;
			case ROUNDNEAREST:
				if (guardbit && (stickybit || 
				    Sgl_isone_lowmantissa(opnd3))) {
			      		Sgl_increment(opnd3);
				}
				break;
			}
                if (is_tiny) Set_underflowflag();
		}
		Sgl_set_exponentmantissa(result,opnd3);
	}
	else Sgl_set_exponent(result,dest_exponent);
	*dstptr = result;

	
	if (inexact) {
		if (Is_inexacttrap_enabled()) return(INEXACTEXCEPTION);
		else Set_inexactflag();
	}
	return(NOEXCEPTION);
}
