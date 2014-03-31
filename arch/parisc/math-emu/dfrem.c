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
#include "dbl_float.h"


int
dbl_frem (dbl_floating_point * srcptr1, dbl_floating_point * srcptr2,
	  dbl_floating_point * dstptr, unsigned int *status)
{
	register unsigned int opnd1p1, opnd1p2, opnd2p1, opnd2p2;
	register unsigned int resultp1, resultp2;
	register int opnd1_exponent, opnd2_exponent, dest_exponent, stepcount;
	register boolean roundup = FALSE;

	Dbl_copyfromptr(srcptr1,opnd1p1,opnd1p2);
	Dbl_copyfromptr(srcptr2,opnd2p1,opnd2p2);
	if ((opnd1_exponent = Dbl_exponent(opnd1p1)) == DBL_INFINITY_EXPONENT) {
		if (Dbl_iszero_mantissa(opnd1p1,opnd1p2)) {
			if (Dbl_isnotnan(opnd2p1,opnd2p2)) {
				
				if (Is_invalidtrap_enabled()) 
                                	return(INVALIDEXCEPTION);
                                Set_invalidflag();
                                Dbl_makequietnan(resultp1,resultp2);
				Dbl_copytoptr(resultp1,resultp2,dstptr);
				return(NOEXCEPTION);
			}
		}
		else {
                	if (Dbl_isone_signaling(opnd1p1)) {
                        	
                        	if (Is_invalidtrap_enabled()) 
                            		return(INVALIDEXCEPTION);
                        	
                        	Set_invalidflag();
                        	Dbl_set_quiet(opnd1p1);
                	}
			else if (Dbl_is_signalingnan(opnd2p1)) {
                        	
                        	if (Is_invalidtrap_enabled()) 
                            		return(INVALIDEXCEPTION);
                        	
                        	Set_invalidflag();
                        	Dbl_set_quiet(opnd2p1);
				Dbl_copytoptr(opnd2p1,opnd2p2,dstptr);
                		return(NOEXCEPTION);
			}
			Dbl_copytoptr(opnd1p1,opnd1p2,dstptr);
                	return(NOEXCEPTION);
		}
	} 
	if ((opnd2_exponent = Dbl_exponent(opnd2p1)) == DBL_INFINITY_EXPONENT) {
		if (Dbl_iszero_mantissa(opnd2p1,opnd2p2)) {
			Dbl_copytoptr(opnd1p1,opnd1p2,dstptr);
			return(NOEXCEPTION);
		}
                if (Dbl_isone_signaling(opnd2p1)) {
                        
                        if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);
                        
                        Set_invalidflag();
                        Dbl_set_quiet(opnd2p1);
                }
		Dbl_copytoptr(opnd2p1,opnd2p2,dstptr);
                return(NOEXCEPTION);
	}
	if (Dbl_iszero_exponentmantissa(opnd2p1,opnd2p2)) {
		
		if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);
                Set_invalidflag();
                Dbl_makequietnan(resultp1,resultp2);
		Dbl_copytoptr(resultp1,resultp2,dstptr);
		return(NOEXCEPTION);
	}

	resultp1 = opnd1p1;  

	if (opnd1_exponent == 0) {
		
		if (Dbl_iszero_mantissa(opnd1p1,opnd1p2)) {
			Dbl_copytoptr(opnd1p1,opnd1p2,dstptr);
			return(NOEXCEPTION);
		}
		
		opnd1_exponent = 1;
		Dbl_normalize(opnd1p1,opnd1p2,opnd1_exponent);
	}
	else {
		Dbl_clear_signexponent_set_hidden(opnd1p1);
	}
	if (opnd2_exponent == 0) {
		
		opnd2_exponent = 1;
		Dbl_normalize(opnd2p1,opnd2p2,opnd2_exponent);
	}
	else {
		Dbl_clear_signexponent_set_hidden(opnd2p1);
	}

	
	dest_exponent = opnd2_exponent - 1;
	stepcount = opnd1_exponent - opnd2_exponent;

	if (stepcount < 0) {
		if (stepcount == -1 && 
		    Dbl_isgreaterthan(opnd1p1,opnd1p2,opnd2p1,opnd2p2)) {
			
			Dbl_allp1(resultp1) = ~Dbl_allp1(resultp1);
			
			Dbl_leftshiftby1(opnd2p1,opnd2p2); 
			Dbl_subtract(opnd2p1,opnd2p2,opnd1p1,opnd1p2,
			 opnd2p1,opnd2p2);
			
                	while (Dbl_iszero_hidden(opnd2p1)) {
                        	Dbl_leftshiftby1(opnd2p1,opnd2p2);
                        	dest_exponent--;
			}
			Dbl_set_exponentmantissa(resultp1,resultp2,opnd2p1,opnd2p2);
			goto testforunderflow;
		}
		Dbl_set_exponentmantissa(resultp1,resultp2,opnd1p1,opnd1p2);
		dest_exponent = opnd1_exponent;
		goto testforunderflow;
	}

	while (stepcount-- > 0 && (Dbl_allp1(opnd1p1) || Dbl_allp2(opnd1p2))) {
		if (Dbl_isnotlessthan(opnd1p1,opnd1p2,opnd2p1,opnd2p2)) {
			Dbl_subtract(opnd1p1,opnd1p2,opnd2p1,opnd2p2,opnd1p1,opnd1p2);
		}
		Dbl_leftshiftby1(opnd1p1,opnd1p2);
	}
	if (Dbl_isnotlessthan(opnd1p1,opnd1p2,opnd2p1,opnd2p2)) {
		Dbl_subtract(opnd1p1,opnd1p2,opnd2p1,opnd2p2,opnd1p1,opnd1p2);
		roundup = TRUE;
	}
	if (stepcount > 0 || Dbl_iszero(opnd1p1,opnd1p2)) {
		
		Dbl_setzero_exponentmantissa(resultp1,resultp2);
		Dbl_copytoptr(resultp1,resultp2,dstptr);
		return(NOEXCEPTION);
	}

	Dbl_leftshiftby1(opnd1p1,opnd1p2);
	if (Dbl_isgreaterthan(opnd1p1,opnd1p2,opnd2p1,opnd2p2)) {
		Dbl_invert_sign(resultp1);
		Dbl_leftshiftby1(opnd2p1,opnd2p2);
		Dbl_subtract(opnd2p1,opnd2p2,opnd1p1,opnd1p2,opnd1p1,opnd1p2);
	}
	
	else if (Dbl_isequal(opnd1p1,opnd1p2,opnd2p1,opnd2p2) && roundup) { 
		Dbl_invert_sign(resultp1);
	}

	
        while (Dbl_iszero_hidden(opnd1p1)) {
                dest_exponent--;
                Dbl_leftshiftby1(opnd1p1,opnd1p2);
        }
	Dbl_set_exponentmantissa(resultp1,resultp2,opnd1p1,opnd1p2);

    testforunderflow:
	if (dest_exponent <= 0) {
                
                if (Is_underflowtrap_enabled()) {
                        Dbl_setwrapped_exponent(resultp1,dest_exponent,unfl);
			
			Dbl_copytoptr(resultp1,resultp2,dstptr);
			return(UNDERFLOWEXCEPTION);
                }
                if (dest_exponent >= (1 - DBL_P)) {
			Dbl_rightshift_exponentmantissa(resultp1,resultp2,
			 1-dest_exponent);
                }
                else {
			Dbl_setzero_exponentmantissa(resultp1,resultp2);
		}
	}
	else Dbl_set_exponent(resultp1,dest_exponent);
	Dbl_copytoptr(resultp1,resultp2,dstptr);
	return(NOEXCEPTION);
}
