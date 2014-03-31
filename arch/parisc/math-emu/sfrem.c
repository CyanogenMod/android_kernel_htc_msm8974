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
sgl_frem (sgl_floating_point * srcptr1, sgl_floating_point * srcptr2,
	  sgl_floating_point * dstptr, unsigned int *status)
{
	register unsigned int opnd1, opnd2, result;
	register int opnd1_exponent, opnd2_exponent, dest_exponent, stepcount;
	register boolean roundup = FALSE;

	opnd1 = *srcptr1;
	opnd2 = *srcptr2;
	if ((opnd1_exponent = Sgl_exponent(opnd1)) == SGL_INFINITY_EXPONENT) {
		if (Sgl_iszero_mantissa(opnd1)) {
			if (Sgl_isnotnan(opnd2)) {
				
				if (Is_invalidtrap_enabled()) 
                                	return(INVALIDEXCEPTION);
                                Set_invalidflag();
                                Sgl_makequietnan(result);
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
	if ((opnd2_exponent = Sgl_exponent(opnd2)) == SGL_INFINITY_EXPONENT) {
		if (Sgl_iszero_mantissa(opnd2)) {
                	*dstptr = opnd1;
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
	if (Sgl_iszero_exponentmantissa(opnd2)) {
		
		if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);
                Set_invalidflag();
                Sgl_makequietnan(result);
		*dstptr = result;
		return(NOEXCEPTION);
	}

	result = opnd1;  

	if (opnd1_exponent == 0) {
		
		if (Sgl_iszero_mantissa(opnd1)) {
			*dstptr = opnd1;
			return(NOEXCEPTION);
		}
		
		opnd1_exponent = 1;
		Sgl_normalize(opnd1,opnd1_exponent);
	}
	else {
		Sgl_clear_signexponent_set_hidden(opnd1);
	}
	if (opnd2_exponent == 0) {
		
		opnd2_exponent = 1;
		Sgl_normalize(opnd2,opnd2_exponent);
	}
	else {
		Sgl_clear_signexponent_set_hidden(opnd2);
	}

	
	dest_exponent = opnd2_exponent - 1;
	stepcount = opnd1_exponent - opnd2_exponent;

	if (stepcount < 0) {
		if (stepcount == -1 && Sgl_isgreaterthan(opnd1,opnd2)) {
			Sgl_all(result) = ~Sgl_all(result);   
			
			Sgl_leftshiftby1(opnd2); 
			Sgl_subtract(opnd2,opnd1,opnd2);
			
                	while (Sgl_iszero_hidden(opnd2)) {
                        	Sgl_leftshiftby1(opnd2);
                        	dest_exponent--;
			}
			Sgl_set_exponentmantissa(result,opnd2);
			goto testforunderflow;
		}
		Sgl_set_exponentmantissa(result,opnd1);
		dest_exponent = opnd1_exponent;
		goto testforunderflow;
	}

	while (stepcount-- > 0 && Sgl_all(opnd1)) {
		if (Sgl_isnotlessthan(opnd1,opnd2))
			Sgl_subtract(opnd1,opnd2,opnd1);
		Sgl_leftshiftby1(opnd1);
	}
	if (Sgl_isnotlessthan(opnd1,opnd2)) {
		Sgl_subtract(opnd1,opnd2,opnd1);
		roundup = TRUE;
	}
	if (stepcount > 0 || Sgl_iszero(opnd1)) {
		
		Sgl_setzero_exponentmantissa(result);
		*dstptr = result;
		return(NOEXCEPTION);
	}

	Sgl_leftshiftby1(opnd1);
	if (Sgl_isgreaterthan(opnd1,opnd2)) {
		Sgl_invert_sign(result);
		Sgl_subtract((opnd2<<1),opnd1,opnd1);
	}
	
	else if (Sgl_isequal(opnd1,opnd2) && roundup) { 
		Sgl_invert_sign(result);
	}

	
        while (Sgl_iszero_hidden(opnd1)) {
                dest_exponent--;
                Sgl_leftshiftby1(opnd1);
        }
	Sgl_set_exponentmantissa(result,opnd1);

    testforunderflow:
	if (dest_exponent <= 0) {
                
                if (Is_underflowtrap_enabled()) {
                        Sgl_setwrapped_exponent(result,dest_exponent,unfl);
			*dstptr = result;
			
			return(UNDERFLOWEXCEPTION);
                }
                if (dest_exponent >= (1 - SGL_P)) {
			Sgl_rightshift_exponentmantissa(result,1-dest_exponent);
                }
                else {
			Sgl_setzero_exponentmantissa(result);
		}
	}
	else Sgl_set_exponent(result,dest_exponent);
	*dstptr = result;
	return(NOEXCEPTION);
}
