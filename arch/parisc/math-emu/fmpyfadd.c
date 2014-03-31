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
#include "dbl_float.h"



int
dbl_fmpyfadd(
	    dbl_floating_point *src1ptr,
	    dbl_floating_point *src2ptr,
	    dbl_floating_point *src3ptr,
	    unsigned int *status,
	    dbl_floating_point *dstptr)
{
	unsigned int opnd1p1, opnd1p2, opnd2p1, opnd2p2, opnd3p1, opnd3p2;
	register unsigned int tmpresp1, tmpresp2, tmpresp3, tmpresp4;
	unsigned int rightp1, rightp2, rightp3, rightp4;
	unsigned int resultp1, resultp2 = 0, resultp3 = 0, resultp4 = 0;
	register int mpy_exponent, add_exponent, count;
	boolean inexact = FALSE, is_tiny = FALSE;

	unsigned int signlessleft1, signlessright1, save;
	register int result_exponent, diff_exponent;
	int sign_save, jumpsize;
	
	Dbl_copyfromptr(src1ptr,opnd1p1,opnd1p2);
	Dbl_copyfromptr(src2ptr,opnd2p1,opnd2p2);
	Dbl_copyfromptr(src3ptr,opnd3p1,opnd3p2);

	if (Dbl_sign(opnd1p1) ^ Dbl_sign(opnd2p1)) 
		Dbl_setnegativezerop1(resultp1); 
	else Dbl_setzerop1(resultp1);

	mpy_exponent = Dbl_exponent(opnd1p1) + Dbl_exponent(opnd2p1) - DBL_BIAS;

	if (Dbl_isinfinity_exponent(opnd1p1)) {
		if (Dbl_iszero_mantissa(opnd1p1,opnd1p2)) {
			if (Dbl_isnotnan(opnd2p1,opnd2p2) &&
			    Dbl_isnotnan(opnd3p1,opnd3p2)) {
				if (Dbl_iszero_exponentmantissa(opnd2p1,opnd2p2)) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Dbl_makequietnan(resultp1,resultp2);
					Dbl_copytoptr(resultp1,resultp2,dstptr);
					return(NOEXCEPTION);
				}
				if (Dbl_isinfinity(opnd3p1,opnd3p2) &&
				    (Dbl_sign(resultp1) ^ Dbl_sign(opnd3p1))) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Dbl_makequietnan(resultp1,resultp2);
					Dbl_copytoptr(resultp1,resultp2,dstptr);
					return(NOEXCEPTION);
				}

				Dbl_setinfinity_exponentmantissa(resultp1,resultp2);
				Dbl_copytoptr(resultp1,resultp2,dstptr);
				return(NOEXCEPTION);
			}
		}
		else {
			if (Dbl_isone_signaling(opnd1p1)) {
				
				if (Is_invalidtrap_enabled()) 
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd1p1);
			}
			else if (Dbl_is_signalingnan(opnd2p1)) {
				
				if (Is_invalidtrap_enabled())
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd2p1);
				Dbl_copytoptr(opnd2p1,opnd2p2,dstptr);
				return(NOEXCEPTION);
			}
			else if (Dbl_is_signalingnan(opnd3p1)) {
				
				if (Is_invalidtrap_enabled())
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd3p1);
				Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
				return(NOEXCEPTION);
			}
			Dbl_copytoptr(opnd1p1,opnd1p2,dstptr);
			return(NOEXCEPTION);
		}
	}

	if (Dbl_isinfinity_exponent(opnd2p1)) {
		if (Dbl_iszero_mantissa(opnd2p1,opnd2p2)) {
			if (Dbl_isnotnan(opnd3p1,opnd3p2)) {
				if (Dbl_iszero_exponentmantissa(opnd1p1,opnd1p2)) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Dbl_makequietnan(opnd2p1,opnd2p2);
					Dbl_copytoptr(opnd2p1,opnd2p2,dstptr);
					return(NOEXCEPTION);
				}

				if (Dbl_isinfinity(opnd3p1,opnd3p2) &&
				    (Dbl_sign(resultp1) ^ Dbl_sign(opnd3p1))) {
					if (Is_invalidtrap_enabled())
				       		return(OPC_2E_INVALIDEXCEPTION);
				       	Set_invalidflag();
				       	Dbl_makequietnan(resultp1,resultp2);
					Dbl_copytoptr(resultp1,resultp2,dstptr);
					return(NOEXCEPTION);
				}

				Dbl_setinfinity_exponentmantissa(resultp1,resultp2);
				Dbl_copytoptr(resultp1,resultp2,dstptr);
				return(NOEXCEPTION);
			}
		}
		else {
			if (Dbl_isone_signaling(opnd2p1)) {
				
				if (Is_invalidtrap_enabled())
					return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd2p1);
			}
			else if (Dbl_is_signalingnan(opnd3p1)) {
			       	
			       	if (Is_invalidtrap_enabled())
				   		return(OPC_2E_INVALIDEXCEPTION);
			       	
			       	Set_invalidflag();
			       	Dbl_set_quiet(opnd3p1);
				Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
		       		return(NOEXCEPTION);
			}
			Dbl_copytoptr(opnd2p1,opnd2p2,dstptr);
			return(NOEXCEPTION);
		}
	}

	if (Dbl_isinfinity_exponent(opnd3p1)) {
		if (Dbl_iszero_mantissa(opnd3p1,opnd3p2)) {
			
			Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
			return(NOEXCEPTION);
		} else {
			if (Dbl_isone_signaling(opnd3p1)) {
				
				if (Is_invalidtrap_enabled())
					return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd3p1);
			}
			Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
			return(NOEXCEPTION);
		}
    	}

	if (Dbl_isnotzero_exponent(opnd1p1)) {
		
		Dbl_clear_signexponent_set_hidden(opnd1p1);
	}
	else {
		
		if (Dbl_iszero_mantissa(opnd1p1,opnd1p2)) {
			if (Dbl_iszero_exponentmantissa(opnd3p1,opnd3p2)) {
				if (Is_rounding_mode(ROUNDMINUS)) {
					Dbl_or_signs(opnd3p1,resultp1);
				} else {
					Dbl_and_signs(opnd3p1,resultp1);
				}
			}
			else if (Dbl_iszero_exponent(opnd3p1) &&
			         Is_underflowtrap_enabled()) {
                    		
                    		sign_save = Dbl_signextendedsign(opnd3p1);
				result_exponent = 0;
                    		Dbl_leftshiftby1(opnd3p1,opnd3p2);
                    		Dbl_normalize(opnd3p1,opnd3p2,result_exponent);
                    		Dbl_set_sign(opnd3p1,sign_save);
                    		Dbl_setwrapped_exponent(opnd3p1,result_exponent,
							unfl);
                    		Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
                    		
                    		return(OPC_2E_UNDERFLOWEXCEPTION);
			}
			Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
			return(NOEXCEPTION);
		}
		
		Dbl_clear_signexponent(opnd1p1);
		Dbl_leftshiftby1(opnd1p1,opnd1p2);
		Dbl_normalize(opnd1p1,opnd1p2,mpy_exponent);
	}
	
	if (Dbl_isnotzero_exponent(opnd2p1)) {
		Dbl_clear_signexponent_set_hidden(opnd2p1);
	}
	else {
		
		if (Dbl_iszero_mantissa(opnd2p1,opnd2p2)) {
			if (Dbl_iszero_exponentmantissa(opnd3p1,opnd3p2)) {
				if (Is_rounding_mode(ROUNDMINUS)) {
					Dbl_or_signs(opnd3p1,resultp1);
				} else {
					Dbl_and_signs(opnd3p1,resultp1);
				}
			}
			else if (Dbl_iszero_exponent(opnd3p1) &&
			    Is_underflowtrap_enabled()) {
                    		
                    		sign_save = Dbl_signextendedsign(opnd3p1);
				result_exponent = 0;
                    		Dbl_leftshiftby1(opnd3p1,opnd3p2);
                    		Dbl_normalize(opnd3p1,opnd3p2,result_exponent);
                    		Dbl_set_sign(opnd3p1,sign_save);
                    		Dbl_setwrapped_exponent(opnd3p1,result_exponent,
							unfl);
                    		Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
                    		
				return(OPC_2E_UNDERFLOWEXCEPTION);
			}
			Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
			return(NOEXCEPTION);
		}
		
		Dbl_clear_signexponent(opnd2p1);
		Dbl_leftshiftby1(opnd2p1,opnd2p2);
		Dbl_normalize(opnd2p1,opnd2p2,mpy_exponent);
	}

	

	Dblext_setzero(tmpresp1,tmpresp2,tmpresp3,tmpresp4);

 
	for (count = DBL_P-1; count >= 0; count -= 4) {
		Dblext_rightshiftby4(tmpresp1,tmpresp2,tmpresp3,tmpresp4);
		if (Dbit28p2(opnd1p2)) {
	 		
			Fourword_add(tmpresp1, tmpresp2, tmpresp3, tmpresp4, 
			 opnd2p1<<3 | opnd2p2>>29, opnd2p2<<3, 0, 0);
		}
		if (Dbit29p2(opnd1p2)) {
			Fourword_add(tmpresp1, tmpresp2, tmpresp3, tmpresp4,
			 opnd2p1<<2 | opnd2p2>>30, opnd2p2<<2, 0, 0);
		}
		if (Dbit30p2(opnd1p2)) {
			Fourword_add(tmpresp1, tmpresp2, tmpresp3, tmpresp4,
			 opnd2p1<<1 | opnd2p2>>31, opnd2p2<<1, 0, 0);
		}
		if (Dbit31p2(opnd1p2)) {
			Fourword_add(tmpresp1, tmpresp2, tmpresp3, tmpresp4,
			 opnd2p1, opnd2p2, 0, 0);
		}
		Dbl_rightshiftby4(opnd1p1,opnd1p2);
	}
	if (Is_dexthiddenoverflow(tmpresp1)) {
		
		mpy_exponent++;
		Dblext_rightshiftby1(tmpresp1,tmpresp2,tmpresp3,tmpresp4);
	}

	Dblext_set_sign(tmpresp1,Dbl_sign(resultp1));


	add_exponent = Dbl_exponent(opnd3p1);

	if (add_exponent == 0) {
		
		if (Dbl_iszero_mantissa(opnd3p1,opnd3p2)) {
			
			result_exponent = mpy_exponent;
			Dblext_copy(tmpresp1,tmpresp2,tmpresp3,tmpresp4,
				resultp1,resultp2,resultp3,resultp4);
			sign_save = Dbl_signextendedsign(resultp1);
			goto round;
		}

		sign_save = Dbl_signextendedsign(opnd3p1);	
		Dbl_clear_signexponent(opnd3p1);
		Dbl_leftshiftby1(opnd3p1,opnd3p2);
		Dbl_normalize(opnd3p1,opnd3p2,add_exponent);
		Dbl_set_sign(opnd3p1,sign_save);	
	} else {
		Dbl_clear_exponent_set_hidden(opnd3p1);
	}
	Dbl_copyto_dblext(opnd3p1,opnd3p2,rightp1,rightp2,rightp3,rightp4);

	Dblext_xortointp1(tmpresp1,rightp1,save);

	Dblext_copytoint_exponentmantissap1(tmpresp1,signlessleft1);
	Dblext_copytoint_exponentmantissap1(rightp1,signlessright1);
	if (mpy_exponent < add_exponent || mpy_exponent == add_exponent &&
	    Dblext_ismagnitudeless(tmpresp2,rightp2,signlessleft1,signlessright1)){
		Dblext_xorfromintp1(save,rightp1,rightp1);
		Dblext_xorfromintp1(save,tmpresp1,tmpresp1);
		Dblext_swap_lower(tmpresp2,tmpresp3,tmpresp4,
			rightp2,rightp3,rightp4);
		
		diff_exponent = add_exponent - mpy_exponent;
		result_exponent = add_exponent;
	} else {
		
		diff_exponent = mpy_exponent - add_exponent;
		result_exponent = mpy_exponent;
	}
	

	if (diff_exponent > DBLEXT_THRESHOLD) {
		diff_exponent = DBLEXT_THRESHOLD;
	}

	
	Dblext_clear_sign(rightp1);
	Dblext_right_align(rightp1,rightp2,rightp3,rightp4,
		diff_exponent);
	
	
	if ((int)save < 0) {
		Dblext_subtract(tmpresp1,tmpresp2,tmpresp3,tmpresp4,
			rightp1,rightp2,rightp3,rightp4,
			resultp1,resultp2,resultp3,resultp4);
		sign_save = Dbl_signextendedsign(resultp1);
		if (Dbl_iszero_hidden(resultp1)) {
			
			Dblext_leftshiftby1(resultp1,resultp2,resultp3,
				resultp4);

			 if(Dblext_iszero(resultp1,resultp2,resultp3,resultp4)){
				
				if (Is_rounding_mode(ROUNDMINUS))
					Dbl_setone_sign(resultp1);
				Dbl_copytoptr(resultp1,resultp2,dstptr);
				return(NOEXCEPTION);
			}
			result_exponent--;

			
			if (Dbl_isone_hidden(resultp1)) {
				
				goto round;
			}

			
			while (Dbl_iszero_hiddenhigh7mantissa(resultp1)) {
				Dblext_leftshiftby8(resultp1,resultp2,resultp3,resultp4);
				result_exponent -= 8;
			}
			
			if (Dbl_iszero_hiddenhigh3mantissa(resultp1)) {
				Dblext_leftshiftby4(resultp1,resultp2,resultp3,resultp4);
				result_exponent -= 4;
			}
			jumpsize = Dbl_hiddenhigh3mantissa(resultp1);
			if (jumpsize <= 7) switch(jumpsize) {
			case 1:
				Dblext_leftshiftby3(resultp1,resultp2,resultp3,
					resultp4);
				result_exponent -= 3;
				break;
			case 2:
			case 3:
				Dblext_leftshiftby2(resultp1,resultp2,resultp3,
					resultp4);
				result_exponent -= 2;
				break;
			case 4:
			case 5:
			case 6:
			case 7:
				Dblext_leftshiftby1(resultp1,resultp2,resultp3,
					resultp4);
				result_exponent -= 1;
				break;
			}
		} 
	
	} 
	else {
		
		Dblext_addition(tmpresp1,tmpresp2,tmpresp3,tmpresp4,
			rightp1,rightp2,rightp3,rightp4,
			resultp1,resultp2,resultp3,resultp4);
		sign_save = Dbl_signextendedsign(resultp1);
		if (Dbl_isone_hiddenoverflow(resultp1)) {
	    		
	    		Dblext_arithrightshiftby1(resultp1,resultp2,resultp3,
				resultp4);
	    		result_exponent++;
		} 
	} 

  round:
	if (result_exponent <= 0 && !Is_underflowtrap_enabled()) {
		Dblext_denormalize(resultp1,resultp2,resultp3,resultp4,
			result_exponent,is_tiny);
	}
	Dbl_set_sign(resultp1,sign_save);
	if (Dblext_isnotzero_mantissap3(resultp3) || 
	    Dblext_isnotzero_mantissap4(resultp4)) {
		inexact = TRUE;
		switch(Rounding_mode()) {
		case ROUNDNEAREST: 
			if (Dblext_isone_highp3(resultp3)) {
				
				if (Dblext_isnotzero_low31p3(resultp3) ||
				    Dblext_isnotzero_mantissap4(resultp4) ||
				    Dblext_isone_lowp2(resultp2)) {
					Dbl_increment(resultp1,resultp2);
				}
			}
	    		break;

		case ROUNDPLUS:
	    		if (Dbl_iszero_sign(resultp1)) {
				
				Dbl_increment(resultp1,resultp2);
			}
			break;
	    
		case ROUNDMINUS:
	    		if (Dbl_isone_sign(resultp1)) {
				
				Dbl_increment(resultp1,resultp2);
			}
	    
		case ROUNDZERO:;
			
		} 
		if (Dbl_isone_hiddenoverflow(resultp1)) result_exponent++;
	}
	if (result_exponent >= DBL_INFINITY_EXPONENT) {
                
                if (Is_overflowtrap_enabled()) {
                        Dbl_setwrapped_exponent(resultp1,result_exponent,ovfl);
                        Dbl_copytoptr(resultp1,resultp2,dstptr);
                        if (inexact)
                            if (Is_inexacttrap_enabled())
                                return (OPC_2E_OVERFLOWEXCEPTION |
					OPC_2E_INEXACTEXCEPTION);
                            else Set_inexactflag();
                        return (OPC_2E_OVERFLOWEXCEPTION);
                }
                inexact = TRUE;
                Set_overflowflag();
                
                Dbl_setoverflow(resultp1,resultp2);

	} else if (result_exponent <= 0) {	
		if (Is_underflowtrap_enabled()) {
                	Dbl_setwrapped_exponent(resultp1,result_exponent,unfl);
			Dbl_copytoptr(resultp1,resultp2,dstptr);
                        if (inexact)
                            if (Is_inexacttrap_enabled())
                                return (OPC_2E_UNDERFLOWEXCEPTION |
					OPC_2E_INEXACTEXCEPTION);
                            else Set_inexactflag();
	    		return(OPC_2E_UNDERFLOWEXCEPTION);
		}
		else if (inexact && is_tiny) Set_underflowflag();
	}
	else Dbl_set_exponent(resultp1,result_exponent);
	Dbl_copytoptr(resultp1,resultp2,dstptr);
	if (inexact) 
		if (Is_inexacttrap_enabled()) return(OPC_2E_INEXACTEXCEPTION);
		else Set_inexactflag();
    	return(NOEXCEPTION);
}


dbl_fmpynfadd(src1ptr,src2ptr,src3ptr,status,dstptr)

dbl_floating_point *src1ptr, *src2ptr, *src3ptr, *dstptr;
unsigned int *status;
{
	unsigned int opnd1p1, opnd1p2, opnd2p1, opnd2p2, opnd3p1, opnd3p2;
	register unsigned int tmpresp1, tmpresp2, tmpresp3, tmpresp4;
	unsigned int rightp1, rightp2, rightp3, rightp4;
	unsigned int resultp1, resultp2 = 0, resultp3 = 0, resultp4 = 0;
	register int mpy_exponent, add_exponent, count;
	boolean inexact = FALSE, is_tiny = FALSE;

	unsigned int signlessleft1, signlessright1, save;
	register int result_exponent, diff_exponent;
	int sign_save, jumpsize;
	
	Dbl_copyfromptr(src1ptr,opnd1p1,opnd1p2);
	Dbl_copyfromptr(src2ptr,opnd2p1,opnd2p2);
	Dbl_copyfromptr(src3ptr,opnd3p1,opnd3p2);

	if (Dbl_sign(opnd1p1) ^ Dbl_sign(opnd2p1)) 
		Dbl_setzerop1(resultp1);
	else
		Dbl_setnegativezerop1(resultp1); 

	mpy_exponent = Dbl_exponent(opnd1p1) + Dbl_exponent(opnd2p1) - DBL_BIAS;

	if (Dbl_isinfinity_exponent(opnd1p1)) {
		if (Dbl_iszero_mantissa(opnd1p1,opnd1p2)) {
			if (Dbl_isnotnan(opnd2p1,opnd2p2) &&
			    Dbl_isnotnan(opnd3p1,opnd3p2)) {
				if (Dbl_iszero_exponentmantissa(opnd2p1,opnd2p2)) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Dbl_makequietnan(resultp1,resultp2);
					Dbl_copytoptr(resultp1,resultp2,dstptr);
					return(NOEXCEPTION);
				}
				if (Dbl_isinfinity(opnd3p1,opnd3p2) &&
				    (Dbl_sign(resultp1) ^ Dbl_sign(opnd3p1))) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Dbl_makequietnan(resultp1,resultp2);
					Dbl_copytoptr(resultp1,resultp2,dstptr);
					return(NOEXCEPTION);
				}

				Dbl_setinfinity_exponentmantissa(resultp1,resultp2);
				Dbl_copytoptr(resultp1,resultp2,dstptr);
				return(NOEXCEPTION);
			}
		}
		else {
			if (Dbl_isone_signaling(opnd1p1)) {
				
				if (Is_invalidtrap_enabled()) 
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd1p1);
			}
			else if (Dbl_is_signalingnan(opnd2p1)) {
				
				if (Is_invalidtrap_enabled())
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd2p1);
				Dbl_copytoptr(opnd2p1,opnd2p2,dstptr);
				return(NOEXCEPTION);
			}
			else if (Dbl_is_signalingnan(opnd3p1)) {
				
				if (Is_invalidtrap_enabled())
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd3p1);
				Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
				return(NOEXCEPTION);
			}
			Dbl_copytoptr(opnd1p1,opnd1p2,dstptr);
			return(NOEXCEPTION);
		}
	}

	if (Dbl_isinfinity_exponent(opnd2p1)) {
		if (Dbl_iszero_mantissa(opnd2p1,opnd2p2)) {
			if (Dbl_isnotnan(opnd3p1,opnd3p2)) {
				if (Dbl_iszero_exponentmantissa(opnd1p1,opnd1p2)) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Dbl_makequietnan(opnd2p1,opnd2p2);
					Dbl_copytoptr(opnd2p1,opnd2p2,dstptr);
					return(NOEXCEPTION);
				}

				if (Dbl_isinfinity(opnd3p1,opnd3p2) &&
				    (Dbl_sign(resultp1) ^ Dbl_sign(opnd3p1))) {
					if (Is_invalidtrap_enabled())
				       		return(OPC_2E_INVALIDEXCEPTION);
				       	Set_invalidflag();
				       	Dbl_makequietnan(resultp1,resultp2);
					Dbl_copytoptr(resultp1,resultp2,dstptr);
					return(NOEXCEPTION);
				}

				Dbl_setinfinity_exponentmantissa(resultp1,resultp2);
				Dbl_copytoptr(resultp1,resultp2,dstptr);
				return(NOEXCEPTION);
			}
		}
		else {
			if (Dbl_isone_signaling(opnd2p1)) {
				
				if (Is_invalidtrap_enabled())
					return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd2p1);
			}
			else if (Dbl_is_signalingnan(opnd3p1)) {
			       	
			       	if (Is_invalidtrap_enabled())
				   		return(OPC_2E_INVALIDEXCEPTION);
			       	
			       	Set_invalidflag();
			       	Dbl_set_quiet(opnd3p1);
				Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
		       		return(NOEXCEPTION);
			}
			Dbl_copytoptr(opnd2p1,opnd2p2,dstptr);
			return(NOEXCEPTION);
		}
	}

	if (Dbl_isinfinity_exponent(opnd3p1)) {
		if (Dbl_iszero_mantissa(opnd3p1,opnd3p2)) {
			
			Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
			return(NOEXCEPTION);
		} else {
			if (Dbl_isone_signaling(opnd3p1)) {
				
				if (Is_invalidtrap_enabled())
					return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Dbl_set_quiet(opnd3p1);
			}
			Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
			return(NOEXCEPTION);
		}
    	}

	if (Dbl_isnotzero_exponent(opnd1p1)) {
		
		Dbl_clear_signexponent_set_hidden(opnd1p1);
	}
	else {
		
		if (Dbl_iszero_mantissa(opnd1p1,opnd1p2)) {
			if (Dbl_iszero_exponentmantissa(opnd3p1,opnd3p2)) {
				if (Is_rounding_mode(ROUNDMINUS)) {
					Dbl_or_signs(opnd3p1,resultp1);
				} else {
					Dbl_and_signs(opnd3p1,resultp1);
				}
			}
			else if (Dbl_iszero_exponent(opnd3p1) &&
			         Is_underflowtrap_enabled()) {
                    		
                    		sign_save = Dbl_signextendedsign(opnd3p1);
				result_exponent = 0;
                    		Dbl_leftshiftby1(opnd3p1,opnd3p2);
                    		Dbl_normalize(opnd3p1,opnd3p2,result_exponent);
                    		Dbl_set_sign(opnd3p1,sign_save);
                    		Dbl_setwrapped_exponent(opnd3p1,result_exponent,
							unfl);
                    		Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
                    		
                    		return(OPC_2E_UNDERFLOWEXCEPTION);
			}
			Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
			return(NOEXCEPTION);
		}
		
		Dbl_clear_signexponent(opnd1p1);
		Dbl_leftshiftby1(opnd1p1,opnd1p2);
		Dbl_normalize(opnd1p1,opnd1p2,mpy_exponent);
	}
	
	if (Dbl_isnotzero_exponent(opnd2p1)) {
		Dbl_clear_signexponent_set_hidden(opnd2p1);
	}
	else {
		
		if (Dbl_iszero_mantissa(opnd2p1,opnd2p2)) {
			if (Dbl_iszero_exponentmantissa(opnd3p1,opnd3p2)) {
				if (Is_rounding_mode(ROUNDMINUS)) {
					Dbl_or_signs(opnd3p1,resultp1);
				} else {
					Dbl_and_signs(opnd3p1,resultp1);
				}
			}
			else if (Dbl_iszero_exponent(opnd3p1) &&
			    Is_underflowtrap_enabled()) {
                    		
                    		sign_save = Dbl_signextendedsign(opnd3p1);
				result_exponent = 0;
                    		Dbl_leftshiftby1(opnd3p1,opnd3p2);
                    		Dbl_normalize(opnd3p1,opnd3p2,result_exponent);
                    		Dbl_set_sign(opnd3p1,sign_save);
                    		Dbl_setwrapped_exponent(opnd3p1,result_exponent,
							unfl);
                    		Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
                    		
                    		return(OPC_2E_UNDERFLOWEXCEPTION);
			}
			Dbl_copytoptr(opnd3p1,opnd3p2,dstptr);
			return(NOEXCEPTION);
		}
		
		Dbl_clear_signexponent(opnd2p1);
		Dbl_leftshiftby1(opnd2p1,opnd2p2);
		Dbl_normalize(opnd2p1,opnd2p2,mpy_exponent);
	}

	

	Dblext_setzero(tmpresp1,tmpresp2,tmpresp3,tmpresp4);

 
	for (count = DBL_P-1; count >= 0; count -= 4) {
		Dblext_rightshiftby4(tmpresp1,tmpresp2,tmpresp3,tmpresp4);
		if (Dbit28p2(opnd1p2)) {
	 		
			Fourword_add(tmpresp1, tmpresp2, tmpresp3, tmpresp4, 
			 opnd2p1<<3 | opnd2p2>>29, opnd2p2<<3, 0, 0);
		}
		if (Dbit29p2(opnd1p2)) {
			Fourword_add(tmpresp1, tmpresp2, tmpresp3, tmpresp4,
			 opnd2p1<<2 | opnd2p2>>30, opnd2p2<<2, 0, 0);
		}
		if (Dbit30p2(opnd1p2)) {
			Fourword_add(tmpresp1, tmpresp2, tmpresp3, tmpresp4,
			 opnd2p1<<1 | opnd2p2>>31, opnd2p2<<1, 0, 0);
		}
		if (Dbit31p2(opnd1p2)) {
			Fourword_add(tmpresp1, tmpresp2, tmpresp3, tmpresp4,
			 opnd2p1, opnd2p2, 0, 0);
		}
		Dbl_rightshiftby4(opnd1p1,opnd1p2);
	}
	if (Is_dexthiddenoverflow(tmpresp1)) {
		
		mpy_exponent++;
		Dblext_rightshiftby1(tmpresp1,tmpresp2,tmpresp3,tmpresp4);
	}

	Dblext_set_sign(tmpresp1,Dbl_sign(resultp1));


	add_exponent = Dbl_exponent(opnd3p1);

	if (add_exponent == 0) {
		
		if (Dbl_iszero_mantissa(opnd3p1,opnd3p2)) {
			
			result_exponent = mpy_exponent;
			Dblext_copy(tmpresp1,tmpresp2,tmpresp3,tmpresp4,
				resultp1,resultp2,resultp3,resultp4);
			sign_save = Dbl_signextendedsign(resultp1);
			goto round;
		}

		sign_save = Dbl_signextendedsign(opnd3p1);	
		Dbl_clear_signexponent(opnd3p1);
		Dbl_leftshiftby1(opnd3p1,opnd3p2);
		Dbl_normalize(opnd3p1,opnd3p2,add_exponent);
		Dbl_set_sign(opnd3p1,sign_save);	
	} else {
		Dbl_clear_exponent_set_hidden(opnd3p1);
	}
	Dbl_copyto_dblext(opnd3p1,opnd3p2,rightp1,rightp2,rightp3,rightp4);

	Dblext_xortointp1(tmpresp1,rightp1,save);

	Dblext_copytoint_exponentmantissap1(tmpresp1,signlessleft1);
	Dblext_copytoint_exponentmantissap1(rightp1,signlessright1);
	if (mpy_exponent < add_exponent || mpy_exponent == add_exponent &&
	    Dblext_ismagnitudeless(tmpresp2,rightp2,signlessleft1,signlessright1)){
		Dblext_xorfromintp1(save,rightp1,rightp1);
		Dblext_xorfromintp1(save,tmpresp1,tmpresp1);
		Dblext_swap_lower(tmpresp2,tmpresp3,tmpresp4,
			rightp2,rightp3,rightp4);
		
		diff_exponent = add_exponent - mpy_exponent;
		result_exponent = add_exponent;
	} else {
		
		diff_exponent = mpy_exponent - add_exponent;
		result_exponent = mpy_exponent;
	}
	

	if (diff_exponent > DBLEXT_THRESHOLD) {
		diff_exponent = DBLEXT_THRESHOLD;
	}

	
	Dblext_clear_sign(rightp1);
	Dblext_right_align(rightp1,rightp2,rightp3,rightp4,
		diff_exponent);
	
	
	if ((int)save < 0) {
		Dblext_subtract(tmpresp1,tmpresp2,tmpresp3,tmpresp4,
			rightp1,rightp2,rightp3,rightp4,
			resultp1,resultp2,resultp3,resultp4);
		sign_save = Dbl_signextendedsign(resultp1);
		if (Dbl_iszero_hidden(resultp1)) {
			
			Dblext_leftshiftby1(resultp1,resultp2,resultp3,
				resultp4);

			 if (Dblext_iszero(resultp1,resultp2,resultp3,resultp4)) {
				
				if (Is_rounding_mode(ROUNDMINUS))
					Dbl_setone_sign(resultp1);
				Dbl_copytoptr(resultp1,resultp2,dstptr);
				return(NOEXCEPTION);
			}
			result_exponent--;

			
			if (Dbl_isone_hidden(resultp1)) {
				
				goto round;
			}

			
			while (Dbl_iszero_hiddenhigh7mantissa(resultp1)) {
				Dblext_leftshiftby8(resultp1,resultp2,resultp3,resultp4);
				result_exponent -= 8;
			}
			
			if (Dbl_iszero_hiddenhigh3mantissa(resultp1)) {
				Dblext_leftshiftby4(resultp1,resultp2,resultp3,resultp4);
				result_exponent -= 4;
			}
			jumpsize = Dbl_hiddenhigh3mantissa(resultp1);
			if (jumpsize <= 7) switch(jumpsize) {
			case 1:
				Dblext_leftshiftby3(resultp1,resultp2,resultp3,
					resultp4);
				result_exponent -= 3;
				break;
			case 2:
			case 3:
				Dblext_leftshiftby2(resultp1,resultp2,resultp3,
					resultp4);
				result_exponent -= 2;
				break;
			case 4:
			case 5:
			case 6:
			case 7:
				Dblext_leftshiftby1(resultp1,resultp2,resultp3,
					resultp4);
				result_exponent -= 1;
				break;
			}
		} 
	
	} 
	else {
		
		Dblext_addition(tmpresp1,tmpresp2,tmpresp3,tmpresp4,
			rightp1,rightp2,rightp3,rightp4,
			resultp1,resultp2,resultp3,resultp4);
		sign_save = Dbl_signextendedsign(resultp1);
		if (Dbl_isone_hiddenoverflow(resultp1)) {
	    		
	    		Dblext_arithrightshiftby1(resultp1,resultp2,resultp3,
				resultp4);
	    		result_exponent++;
		} 
	} 

  round:
	if (result_exponent <= 0 && !Is_underflowtrap_enabled()) {
		Dblext_denormalize(resultp1,resultp2,resultp3,resultp4,
			result_exponent,is_tiny);
	}
	Dbl_set_sign(resultp1,sign_save);
	if (Dblext_isnotzero_mantissap3(resultp3) || 
	    Dblext_isnotzero_mantissap4(resultp4)) {
		inexact = TRUE;
		switch(Rounding_mode()) {
		case ROUNDNEAREST: 
			if (Dblext_isone_highp3(resultp3)) {
				
				if (Dblext_isnotzero_low31p3(resultp3) ||
				    Dblext_isnotzero_mantissap4(resultp4) ||
				    Dblext_isone_lowp2(resultp2)) {
					Dbl_increment(resultp1,resultp2);
				}
			}
	    		break;

		case ROUNDPLUS:
	    		if (Dbl_iszero_sign(resultp1)) {
				
				Dbl_increment(resultp1,resultp2);
			}
			break;
	    
		case ROUNDMINUS:
	    		if (Dbl_isone_sign(resultp1)) {
				
				Dbl_increment(resultp1,resultp2);
			}
	    
		case ROUNDZERO:;
			
		} 
		if (Dbl_isone_hiddenoverflow(resultp1)) result_exponent++;
	}
	if (result_exponent >= DBL_INFINITY_EXPONENT) {
		
		if (Is_overflowtrap_enabled()) {
                        Dbl_setwrapped_exponent(resultp1,result_exponent,ovfl);
                        Dbl_copytoptr(resultp1,resultp2,dstptr);
                        if (inexact)
                            if (Is_inexacttrap_enabled())
                                return (OPC_2E_OVERFLOWEXCEPTION |
					OPC_2E_INEXACTEXCEPTION);
                            else Set_inexactflag();
                        return (OPC_2E_OVERFLOWEXCEPTION);
		}
		inexact = TRUE;
		Set_overflowflag();
		Dbl_setoverflow(resultp1,resultp2);
	} else if (result_exponent <= 0) {	
		if (Is_underflowtrap_enabled()) {
                	Dbl_setwrapped_exponent(resultp1,result_exponent,unfl);
			Dbl_copytoptr(resultp1,resultp2,dstptr);
                        if (inexact)
                            if (Is_inexacttrap_enabled())
                                return (OPC_2E_UNDERFLOWEXCEPTION |
					OPC_2E_INEXACTEXCEPTION);
                            else Set_inexactflag();
	    		return(OPC_2E_UNDERFLOWEXCEPTION);
		}
		else if (inexact && is_tiny) Set_underflowflag();
	}
	else Dbl_set_exponent(resultp1,result_exponent);
	Dbl_copytoptr(resultp1,resultp2,dstptr);
	if (inexact) 
		if (Is_inexacttrap_enabled()) return(OPC_2E_INEXACTEXCEPTION);
		else Set_inexactflag();
    	return(NOEXCEPTION);
}


sgl_fmpyfadd(src1ptr,src2ptr,src3ptr,status,dstptr)

sgl_floating_point *src1ptr, *src2ptr, *src3ptr, *dstptr;
unsigned int *status;
{
	unsigned int opnd1, opnd2, opnd3;
	register unsigned int tmpresp1, tmpresp2;
	unsigned int rightp1, rightp2;
	unsigned int resultp1, resultp2 = 0;
	register int mpy_exponent, add_exponent, count;
	boolean inexact = FALSE, is_tiny = FALSE;

	unsigned int signlessleft1, signlessright1, save;
	register int result_exponent, diff_exponent;
	int sign_save, jumpsize;
	
	Sgl_copyfromptr(src1ptr,opnd1);
	Sgl_copyfromptr(src2ptr,opnd2);
	Sgl_copyfromptr(src3ptr,opnd3);

	if (Sgl_sign(opnd1) ^ Sgl_sign(opnd2)) 
		Sgl_setnegativezero(resultp1); 
	else Sgl_setzero(resultp1);

	mpy_exponent = Sgl_exponent(opnd1) + Sgl_exponent(opnd2) - SGL_BIAS;

	if (Sgl_isinfinity_exponent(opnd1)) {
		if (Sgl_iszero_mantissa(opnd1)) {
			if (Sgl_isnotnan(opnd2) && Sgl_isnotnan(opnd3)) {
				if (Sgl_iszero_exponentmantissa(opnd2)) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Sgl_makequietnan(resultp1);
					Sgl_copytoptr(resultp1,dstptr);
					return(NOEXCEPTION);
				}
				if (Sgl_isinfinity(opnd3) &&
				    (Sgl_sign(resultp1) ^ Sgl_sign(opnd3))) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Sgl_makequietnan(resultp1);
					Sgl_copytoptr(resultp1,dstptr);
					return(NOEXCEPTION);
				}

				Sgl_setinfinity_exponentmantissa(resultp1);
				Sgl_copytoptr(resultp1,dstptr);
				return(NOEXCEPTION);
			}
		}
		else {
			if (Sgl_isone_signaling(opnd1)) {
				
				if (Is_invalidtrap_enabled()) 
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd1);
			}
			else if (Sgl_is_signalingnan(opnd2)) {
				
				if (Is_invalidtrap_enabled())
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd2);
				Sgl_copytoptr(opnd2,dstptr);
				return(NOEXCEPTION);
			}
			else if (Sgl_is_signalingnan(opnd3)) {
				
				if (Is_invalidtrap_enabled())
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd3);
				Sgl_copytoptr(opnd3,dstptr);
				return(NOEXCEPTION);
			}
			Sgl_copytoptr(opnd1,dstptr);
			return(NOEXCEPTION);
		}
	}

	if (Sgl_isinfinity_exponent(opnd2)) {
		if (Sgl_iszero_mantissa(opnd2)) {
			if (Sgl_isnotnan(opnd3)) {
				if (Sgl_iszero_exponentmantissa(opnd1)) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Sgl_makequietnan(opnd2);
					Sgl_copytoptr(opnd2,dstptr);
					return(NOEXCEPTION);
				}

				if (Sgl_isinfinity(opnd3) &&
				    (Sgl_sign(resultp1) ^ Sgl_sign(opnd3))) {
					if (Is_invalidtrap_enabled())
				       		return(OPC_2E_INVALIDEXCEPTION);
				       	Set_invalidflag();
				       	Sgl_makequietnan(resultp1);
					Sgl_copytoptr(resultp1,dstptr);
					return(NOEXCEPTION);
				}

				Sgl_setinfinity_exponentmantissa(resultp1);
				Sgl_copytoptr(resultp1,dstptr);
				return(NOEXCEPTION);
			}
		}
		else {
			if (Sgl_isone_signaling(opnd2)) {
				
				if (Is_invalidtrap_enabled())
					return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd2);
			}
			else if (Sgl_is_signalingnan(opnd3)) {
			       	
			       	if (Is_invalidtrap_enabled())
				   		return(OPC_2E_INVALIDEXCEPTION);
			       	
			       	Set_invalidflag();
			       	Sgl_set_quiet(opnd3);
				Sgl_copytoptr(opnd3,dstptr);
		       		return(NOEXCEPTION);
			}
			Sgl_copytoptr(opnd2,dstptr);
			return(NOEXCEPTION);
		}
	}

	if (Sgl_isinfinity_exponent(opnd3)) {
		if (Sgl_iszero_mantissa(opnd3)) {
			
			Sgl_copytoptr(opnd3,dstptr);
			return(NOEXCEPTION);
		} else {
			if (Sgl_isone_signaling(opnd3)) {
				
				if (Is_invalidtrap_enabled())
					return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd3);
			}
			Sgl_copytoptr(opnd3,dstptr);
			return(NOEXCEPTION);
		}
    	}

	if (Sgl_isnotzero_exponent(opnd1)) {
		
		Sgl_clear_signexponent_set_hidden(opnd1);
	}
	else {
		
		if (Sgl_iszero_mantissa(opnd1)) {
			if (Sgl_iszero_exponentmantissa(opnd3)) {
				if (Is_rounding_mode(ROUNDMINUS)) {
					Sgl_or_signs(opnd3,resultp1);
				} else {
					Sgl_and_signs(opnd3,resultp1);
				}
			}
			else if (Sgl_iszero_exponent(opnd3) &&
			         Is_underflowtrap_enabled()) {
                    		
                    		sign_save = Sgl_signextendedsign(opnd3);
				result_exponent = 0;
                    		Sgl_leftshiftby1(opnd3);
                    		Sgl_normalize(opnd3,result_exponent);
                    		Sgl_set_sign(opnd3,sign_save);
                    		Sgl_setwrapped_exponent(opnd3,result_exponent,
							unfl);
                    		Sgl_copytoptr(opnd3,dstptr);
                    		
                    		return(OPC_2E_UNDERFLOWEXCEPTION);
			}
			Sgl_copytoptr(opnd3,dstptr);
			return(NOEXCEPTION);
		}
		
		Sgl_clear_signexponent(opnd1);
		Sgl_leftshiftby1(opnd1);
		Sgl_normalize(opnd1,mpy_exponent);
	}
	
	if (Sgl_isnotzero_exponent(opnd2)) {
		Sgl_clear_signexponent_set_hidden(opnd2);
	}
	else {
		
		if (Sgl_iszero_mantissa(opnd2)) {
			if (Sgl_iszero_exponentmantissa(opnd3)) {
				if (Is_rounding_mode(ROUNDMINUS)) {
					Sgl_or_signs(opnd3,resultp1);
				} else {
					Sgl_and_signs(opnd3,resultp1);
				}
			}
			else if (Sgl_iszero_exponent(opnd3) &&
			    Is_underflowtrap_enabled()) {
                    		
                    		sign_save = Sgl_signextendedsign(opnd3);
				result_exponent = 0;
                    		Sgl_leftshiftby1(opnd3);
                    		Sgl_normalize(opnd3,result_exponent);
                    		Sgl_set_sign(opnd3,sign_save);
                    		Sgl_setwrapped_exponent(opnd3,result_exponent,
							unfl);
                    		Sgl_copytoptr(opnd3,dstptr);
                    		
                    		return(OPC_2E_UNDERFLOWEXCEPTION);
			}
			Sgl_copytoptr(opnd3,dstptr);
			return(NOEXCEPTION);
		}
		
		Sgl_clear_signexponent(opnd2);
		Sgl_leftshiftby1(opnd2);
		Sgl_normalize(opnd2,mpy_exponent);
	}

	

	Sglext_setzero(tmpresp1,tmpresp2);

 
	for (count = SGL_P-1; count >= 0; count -= 4) {
		Sglext_rightshiftby4(tmpresp1,tmpresp2);
		if (Sbit28(opnd1)) {
	 		
			Twoword_add(tmpresp1, tmpresp2, opnd2<<3, 0);
		}
		if (Sbit29(opnd1)) {
			Twoword_add(tmpresp1, tmpresp2, opnd2<<2, 0);
		}
		if (Sbit30(opnd1)) {
			Twoword_add(tmpresp1, tmpresp2, opnd2<<1, 0);
		}
		if (Sbit31(opnd1)) {
			Twoword_add(tmpresp1, tmpresp2, opnd2, 0);
		}
		Sgl_rightshiftby4(opnd1);
	}
	if (Is_sexthiddenoverflow(tmpresp1)) {
		
		mpy_exponent++;
		Sglext_rightshiftby4(tmpresp1,tmpresp2);
	} else {
		Sglext_rightshiftby3(tmpresp1,tmpresp2);
	}

	Sglext_set_sign(tmpresp1,Sgl_sign(resultp1));


	add_exponent = Sgl_exponent(opnd3);

	if (add_exponent == 0) {
		
		if (Sgl_iszero_mantissa(opnd3)) {
			
			result_exponent = mpy_exponent;
			Sglext_copy(tmpresp1,tmpresp2,resultp1,resultp2);
			sign_save = Sgl_signextendedsign(resultp1);
			goto round;
		}

		sign_save = Sgl_signextendedsign(opnd3);	
		Sgl_clear_signexponent(opnd3);
		Sgl_leftshiftby1(opnd3);
		Sgl_normalize(opnd3,add_exponent);
		Sgl_set_sign(opnd3,sign_save);		
	} else {
		Sgl_clear_exponent_set_hidden(opnd3);
	}
	Sgl_copyto_sglext(opnd3,rightp1,rightp2);

	Sglext_xortointp1(tmpresp1,rightp1,save);

	Sglext_copytoint_exponentmantissa(tmpresp1,signlessleft1);
	Sglext_copytoint_exponentmantissa(rightp1,signlessright1);
	if (mpy_exponent < add_exponent || mpy_exponent == add_exponent &&
	    Sglext_ismagnitudeless(signlessleft1,signlessright1)) {
		Sglext_xorfromintp1(save,rightp1,rightp1);
		Sglext_xorfromintp1(save,tmpresp1,tmpresp1);
		Sglext_swap_lower(tmpresp2,rightp2);
		
		diff_exponent = add_exponent - mpy_exponent;
		result_exponent = add_exponent;
	} else {
		
		diff_exponent = mpy_exponent - add_exponent;
		result_exponent = mpy_exponent;
	}
	

	if (diff_exponent > SGLEXT_THRESHOLD) {
		diff_exponent = SGLEXT_THRESHOLD;
	}

	
	Sglext_clear_sign(rightp1);
	Sglext_right_align(rightp1,rightp2,diff_exponent);
	
	
	if ((int)save < 0) {
		Sglext_subtract(tmpresp1,tmpresp2, rightp1,rightp2,
			resultp1,resultp2);
		sign_save = Sgl_signextendedsign(resultp1);
		if (Sgl_iszero_hidden(resultp1)) {
			
			Sglext_leftshiftby1(resultp1,resultp2);

			 if (Sglext_iszero(resultp1,resultp2)) {
				
				if (Is_rounding_mode(ROUNDMINUS))
					Sgl_setone_sign(resultp1);
				Sgl_copytoptr(resultp1,dstptr);
				return(NOEXCEPTION);
			}
			result_exponent--;

			
			if (Sgl_isone_hidden(resultp1)) {
				
				goto round;
			}

			
			while (Sgl_iszero_hiddenhigh7mantissa(resultp1)) {
				Sglext_leftshiftby8(resultp1,resultp2);
				result_exponent -= 8;
			}
			
			if (Sgl_iszero_hiddenhigh3mantissa(resultp1)) {
				Sglext_leftshiftby4(resultp1,resultp2);
				result_exponent -= 4;
			}
			jumpsize = Sgl_hiddenhigh3mantissa(resultp1);
			if (jumpsize <= 7) switch(jumpsize) {
			case 1:
				Sglext_leftshiftby3(resultp1,resultp2);
				result_exponent -= 3;
				break;
			case 2:
			case 3:
				Sglext_leftshiftby2(resultp1,resultp2);
				result_exponent -= 2;
				break;
			case 4:
			case 5:
			case 6:
			case 7:
				Sglext_leftshiftby1(resultp1,resultp2);
				result_exponent -= 1;
				break;
			}
		} 
	
	} 
	else {
		
		Sglext_addition(tmpresp1,tmpresp2,
			rightp1,rightp2, resultp1,resultp2);
		sign_save = Sgl_signextendedsign(resultp1);
		if (Sgl_isone_hiddenoverflow(resultp1)) {
	    		
	    		Sglext_arithrightshiftby1(resultp1,resultp2);
	    		result_exponent++;
		} 
	} 

  round:
	if (result_exponent <= 0 && !Is_underflowtrap_enabled()) {
		Sglext_denormalize(resultp1,resultp2,result_exponent,is_tiny);
	}
	Sgl_set_sign(resultp1,sign_save);
	if (Sglext_isnotzero_mantissap2(resultp2)) {
		inexact = TRUE;
		switch(Rounding_mode()) {
		case ROUNDNEAREST: 
			if (Sglext_isone_highp2(resultp2)) {
				
				if (Sglext_isnotzero_low31p2(resultp2) ||
				    Sglext_isone_lowp1(resultp1)) {
					Sgl_increment(resultp1);
				}
			}
	    		break;

		case ROUNDPLUS:
	    		if (Sgl_iszero_sign(resultp1)) {
				
				Sgl_increment(resultp1);
			}
			break;
	    
		case ROUNDMINUS:
	    		if (Sgl_isone_sign(resultp1)) {
				
				Sgl_increment(resultp1);
			}
	    
		case ROUNDZERO:;
			
		} 
		if (Sgl_isone_hiddenoverflow(resultp1)) result_exponent++;
	}
	if (result_exponent >= SGL_INFINITY_EXPONENT) {
		
		if (Is_overflowtrap_enabled()) {
                        Sgl_setwrapped_exponent(resultp1,result_exponent,ovfl);
                        Sgl_copytoptr(resultp1,dstptr);
                        if (inexact)
                            if (Is_inexacttrap_enabled())
                                return (OPC_2E_OVERFLOWEXCEPTION |
					OPC_2E_INEXACTEXCEPTION);
                            else Set_inexactflag();
                        return (OPC_2E_OVERFLOWEXCEPTION);
		}
		inexact = TRUE;
		Set_overflowflag();
		Sgl_setoverflow(resultp1);
	} else if (result_exponent <= 0) {	
		if (Is_underflowtrap_enabled()) {
                	Sgl_setwrapped_exponent(resultp1,result_exponent,unfl);
			Sgl_copytoptr(resultp1,dstptr);
                        if (inexact)
                            if (Is_inexacttrap_enabled())
                                return (OPC_2E_UNDERFLOWEXCEPTION |
					OPC_2E_INEXACTEXCEPTION);
                            else Set_inexactflag();
	    		return(OPC_2E_UNDERFLOWEXCEPTION);
		}
		else if (inexact && is_tiny) Set_underflowflag();
	}
	else Sgl_set_exponent(resultp1,result_exponent);
	Sgl_copytoptr(resultp1,dstptr);
	if (inexact) 
		if (Is_inexacttrap_enabled()) return(OPC_2E_INEXACTEXCEPTION);
		else Set_inexactflag();
    	return(NOEXCEPTION);
}


sgl_fmpynfadd(src1ptr,src2ptr,src3ptr,status,dstptr)

sgl_floating_point *src1ptr, *src2ptr, *src3ptr, *dstptr;
unsigned int *status;
{
	unsigned int opnd1, opnd2, opnd3;
	register unsigned int tmpresp1, tmpresp2;
	unsigned int rightp1, rightp2;
	unsigned int resultp1, resultp2 = 0;
	register int mpy_exponent, add_exponent, count;
	boolean inexact = FALSE, is_tiny = FALSE;

	unsigned int signlessleft1, signlessright1, save;
	register int result_exponent, diff_exponent;
	int sign_save, jumpsize;
	
	Sgl_copyfromptr(src1ptr,opnd1);
	Sgl_copyfromptr(src2ptr,opnd2);
	Sgl_copyfromptr(src3ptr,opnd3);

	if (Sgl_sign(opnd1) ^ Sgl_sign(opnd2)) 
		Sgl_setzero(resultp1);
	else 
		Sgl_setnegativezero(resultp1); 

	mpy_exponent = Sgl_exponent(opnd1) + Sgl_exponent(opnd2) - SGL_BIAS;

	if (Sgl_isinfinity_exponent(opnd1)) {
		if (Sgl_iszero_mantissa(opnd1)) {
			if (Sgl_isnotnan(opnd2) && Sgl_isnotnan(opnd3)) {
				if (Sgl_iszero_exponentmantissa(opnd2)) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Sgl_makequietnan(resultp1);
					Sgl_copytoptr(resultp1,dstptr);
					return(NOEXCEPTION);
				}
				if (Sgl_isinfinity(opnd3) &&
				    (Sgl_sign(resultp1) ^ Sgl_sign(opnd3))) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Sgl_makequietnan(resultp1);
					Sgl_copytoptr(resultp1,dstptr);
					return(NOEXCEPTION);
				}

				Sgl_setinfinity_exponentmantissa(resultp1);
				Sgl_copytoptr(resultp1,dstptr);
				return(NOEXCEPTION);
			}
		}
		else {
			if (Sgl_isone_signaling(opnd1)) {
				
				if (Is_invalidtrap_enabled()) 
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd1);
			}
			else if (Sgl_is_signalingnan(opnd2)) {
				
				if (Is_invalidtrap_enabled())
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd2);
				Sgl_copytoptr(opnd2,dstptr);
				return(NOEXCEPTION);
			}
			else if (Sgl_is_signalingnan(opnd3)) {
				
				if (Is_invalidtrap_enabled())
			    		return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd3);
				Sgl_copytoptr(opnd3,dstptr);
				return(NOEXCEPTION);
			}
			Sgl_copytoptr(opnd1,dstptr);
			return(NOEXCEPTION);
		}
	}

	if (Sgl_isinfinity_exponent(opnd2)) {
		if (Sgl_iszero_mantissa(opnd2)) {
			if (Sgl_isnotnan(opnd3)) {
				if (Sgl_iszero_exponentmantissa(opnd1)) {
					if (Is_invalidtrap_enabled())
						return(OPC_2E_INVALIDEXCEPTION);
					Set_invalidflag();
					Sgl_makequietnan(opnd2);
					Sgl_copytoptr(opnd2,dstptr);
					return(NOEXCEPTION);
				}

				if (Sgl_isinfinity(opnd3) &&
				    (Sgl_sign(resultp1) ^ Sgl_sign(opnd3))) {
					if (Is_invalidtrap_enabled())
				       		return(OPC_2E_INVALIDEXCEPTION);
				       	Set_invalidflag();
				       	Sgl_makequietnan(resultp1);
					Sgl_copytoptr(resultp1,dstptr);
					return(NOEXCEPTION);
				}

				Sgl_setinfinity_exponentmantissa(resultp1);
				Sgl_copytoptr(resultp1,dstptr);
				return(NOEXCEPTION);
			}
		}
		else {
			if (Sgl_isone_signaling(opnd2)) {
				
				if (Is_invalidtrap_enabled())
					return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd2);
			}
			else if (Sgl_is_signalingnan(opnd3)) {
			       	
			       	if (Is_invalidtrap_enabled())
				   		return(OPC_2E_INVALIDEXCEPTION);
			       	
			       	Set_invalidflag();
			       	Sgl_set_quiet(opnd3);
				Sgl_copytoptr(opnd3,dstptr);
		       		return(NOEXCEPTION);
			}
			Sgl_copytoptr(opnd2,dstptr);
			return(NOEXCEPTION);
		}
	}

	if (Sgl_isinfinity_exponent(opnd3)) {
		if (Sgl_iszero_mantissa(opnd3)) {
			
			Sgl_copytoptr(opnd3,dstptr);
			return(NOEXCEPTION);
		} else {
			if (Sgl_isone_signaling(opnd3)) {
				
				if (Is_invalidtrap_enabled())
					return(OPC_2E_INVALIDEXCEPTION);
				
				Set_invalidflag();
				Sgl_set_quiet(opnd3);
			}
			Sgl_copytoptr(opnd3,dstptr);
			return(NOEXCEPTION);
		}
    	}

	if (Sgl_isnotzero_exponent(opnd1)) {
		
		Sgl_clear_signexponent_set_hidden(opnd1);
	}
	else {
		
		if (Sgl_iszero_mantissa(opnd1)) {
			if (Sgl_iszero_exponentmantissa(opnd3)) {
				if (Is_rounding_mode(ROUNDMINUS)) {
					Sgl_or_signs(opnd3,resultp1);
				} else {
					Sgl_and_signs(opnd3,resultp1);
				}
			}
			else if (Sgl_iszero_exponent(opnd3) &&
			         Is_underflowtrap_enabled()) {
                    		
                    		sign_save = Sgl_signextendedsign(opnd3);
				result_exponent = 0;
                    		Sgl_leftshiftby1(opnd3);
                    		Sgl_normalize(opnd3,result_exponent);
                    		Sgl_set_sign(opnd3,sign_save);
                    		Sgl_setwrapped_exponent(opnd3,result_exponent,
							unfl);
                    		Sgl_copytoptr(opnd3,dstptr);
                    		
                    		return(OPC_2E_UNDERFLOWEXCEPTION);
			}
			Sgl_copytoptr(opnd3,dstptr);
			return(NOEXCEPTION);
		}
		
		Sgl_clear_signexponent(opnd1);
		Sgl_leftshiftby1(opnd1);
		Sgl_normalize(opnd1,mpy_exponent);
	}
	
	if (Sgl_isnotzero_exponent(opnd2)) {
		Sgl_clear_signexponent_set_hidden(opnd2);
	}
	else {
		
		if (Sgl_iszero_mantissa(opnd2)) {
			if (Sgl_iszero_exponentmantissa(opnd3)) {
				if (Is_rounding_mode(ROUNDMINUS)) {
					Sgl_or_signs(opnd3,resultp1);
				} else {
					Sgl_and_signs(opnd3,resultp1);
				}
			}
			else if (Sgl_iszero_exponent(opnd3) &&
			    Is_underflowtrap_enabled()) {
                    		
                    		sign_save = Sgl_signextendedsign(opnd3);
				result_exponent = 0;
                    		Sgl_leftshiftby1(opnd3);
                    		Sgl_normalize(opnd3,result_exponent);
                    		Sgl_set_sign(opnd3,sign_save);
                    		Sgl_setwrapped_exponent(opnd3,result_exponent,
							unfl);
                    		Sgl_copytoptr(opnd3,dstptr);
                    		
                    		return(OPC_2E_UNDERFLOWEXCEPTION);
			}
			Sgl_copytoptr(opnd3,dstptr);
			return(NOEXCEPTION);
		}
		
		Sgl_clear_signexponent(opnd2);
		Sgl_leftshiftby1(opnd2);
		Sgl_normalize(opnd2,mpy_exponent);
	}

	

	Sglext_setzero(tmpresp1,tmpresp2);

 
	for (count = SGL_P-1; count >= 0; count -= 4) {
		Sglext_rightshiftby4(tmpresp1,tmpresp2);
		if (Sbit28(opnd1)) {
	 		
			Twoword_add(tmpresp1, tmpresp2, opnd2<<3, 0);
		}
		if (Sbit29(opnd1)) {
			Twoword_add(tmpresp1, tmpresp2, opnd2<<2, 0);
		}
		if (Sbit30(opnd1)) {
			Twoword_add(tmpresp1, tmpresp2, opnd2<<1, 0);
		}
		if (Sbit31(opnd1)) {
			Twoword_add(tmpresp1, tmpresp2, opnd2, 0);
		}
		Sgl_rightshiftby4(opnd1);
	}
	if (Is_sexthiddenoverflow(tmpresp1)) {
		
		mpy_exponent++;
		Sglext_rightshiftby4(tmpresp1,tmpresp2);
	} else {
		Sglext_rightshiftby3(tmpresp1,tmpresp2);
	}

	Sglext_set_sign(tmpresp1,Sgl_sign(resultp1));


	add_exponent = Sgl_exponent(opnd3);

	if (add_exponent == 0) {
		
		if (Sgl_iszero_mantissa(opnd3)) {
			
			result_exponent = mpy_exponent;
			Sglext_copy(tmpresp1,tmpresp2,resultp1,resultp2);
			sign_save = Sgl_signextendedsign(resultp1);
			goto round;
		}

		sign_save = Sgl_signextendedsign(opnd3);	
		Sgl_clear_signexponent(opnd3);
		Sgl_leftshiftby1(opnd3);
		Sgl_normalize(opnd3,add_exponent);
		Sgl_set_sign(opnd3,sign_save);		
	} else {
		Sgl_clear_exponent_set_hidden(opnd3);
	}
	Sgl_copyto_sglext(opnd3,rightp1,rightp2);

	Sglext_xortointp1(tmpresp1,rightp1,save);

	Sglext_copytoint_exponentmantissa(tmpresp1,signlessleft1);
	Sglext_copytoint_exponentmantissa(rightp1,signlessright1);
	if (mpy_exponent < add_exponent || mpy_exponent == add_exponent &&
	    Sglext_ismagnitudeless(signlessleft1,signlessright1)) {
		Sglext_xorfromintp1(save,rightp1,rightp1);
		Sglext_xorfromintp1(save,tmpresp1,tmpresp1);
		Sglext_swap_lower(tmpresp2,rightp2);
		
		diff_exponent = add_exponent - mpy_exponent;
		result_exponent = add_exponent;
	} else {
		
		diff_exponent = mpy_exponent - add_exponent;
		result_exponent = mpy_exponent;
	}
	

	if (diff_exponent > SGLEXT_THRESHOLD) {
		diff_exponent = SGLEXT_THRESHOLD;
	}

	
	Sglext_clear_sign(rightp1);
	Sglext_right_align(rightp1,rightp2,diff_exponent);
	
	
	if ((int)save < 0) {
		Sglext_subtract(tmpresp1,tmpresp2, rightp1,rightp2,
			resultp1,resultp2);
		sign_save = Sgl_signextendedsign(resultp1);
		if (Sgl_iszero_hidden(resultp1)) {
			
			Sglext_leftshiftby1(resultp1,resultp2);

			 if (Sglext_iszero(resultp1,resultp2)) {
				
				if (Is_rounding_mode(ROUNDMINUS))
					Sgl_setone_sign(resultp1);
				Sgl_copytoptr(resultp1,dstptr);
				return(NOEXCEPTION);
			}
			result_exponent--;

			
			if (Sgl_isone_hidden(resultp1)) {
				
				goto round;
			}

			
			while (Sgl_iszero_hiddenhigh7mantissa(resultp1)) {
				Sglext_leftshiftby8(resultp1,resultp2);
				result_exponent -= 8;
			}
			
			if (Sgl_iszero_hiddenhigh3mantissa(resultp1)) {
				Sglext_leftshiftby4(resultp1,resultp2);
				result_exponent -= 4;
			}
			jumpsize = Sgl_hiddenhigh3mantissa(resultp1);
			if (jumpsize <= 7) switch(jumpsize) {
			case 1:
				Sglext_leftshiftby3(resultp1,resultp2);
				result_exponent -= 3;
				break;
			case 2:
			case 3:
				Sglext_leftshiftby2(resultp1,resultp2);
				result_exponent -= 2;
				break;
			case 4:
			case 5:
			case 6:
			case 7:
				Sglext_leftshiftby1(resultp1,resultp2);
				result_exponent -= 1;
				break;
			}
		} 
	
	} 
	else {
		
		Sglext_addition(tmpresp1,tmpresp2,
			rightp1,rightp2, resultp1,resultp2);
		sign_save = Sgl_signextendedsign(resultp1);
		if (Sgl_isone_hiddenoverflow(resultp1)) {
	    		
	    		Sglext_arithrightshiftby1(resultp1,resultp2);
	    		result_exponent++;
		} 
	} 

  round:
	if (result_exponent <= 0 && !Is_underflowtrap_enabled()) {
		Sglext_denormalize(resultp1,resultp2,result_exponent,is_tiny);
	}
	Sgl_set_sign(resultp1,sign_save);
	if (Sglext_isnotzero_mantissap2(resultp2)) {
		inexact = TRUE;
		switch(Rounding_mode()) {
		case ROUNDNEAREST: 
			if (Sglext_isone_highp2(resultp2)) {
				
				if (Sglext_isnotzero_low31p2(resultp2) ||
				    Sglext_isone_lowp1(resultp1)) {
					Sgl_increment(resultp1);
				}
			}
	    		break;

		case ROUNDPLUS:
	    		if (Sgl_iszero_sign(resultp1)) {
				
				Sgl_increment(resultp1);
			}
			break;
	    
		case ROUNDMINUS:
	    		if (Sgl_isone_sign(resultp1)) {
				
				Sgl_increment(resultp1);
			}
	    
		case ROUNDZERO:;
			
		} 
		if (Sgl_isone_hiddenoverflow(resultp1)) result_exponent++;
	}
	if (result_exponent >= SGL_INFINITY_EXPONENT) {
		
		if (Is_overflowtrap_enabled()) {
                        Sgl_setwrapped_exponent(resultp1,result_exponent,ovfl);
                        Sgl_copytoptr(resultp1,dstptr);
                        if (inexact)
                            if (Is_inexacttrap_enabled())
                                return (OPC_2E_OVERFLOWEXCEPTION |
					OPC_2E_INEXACTEXCEPTION);
                            else Set_inexactflag();
                        return (OPC_2E_OVERFLOWEXCEPTION);
		}
		inexact = TRUE;
		Set_overflowflag();
		Sgl_setoverflow(resultp1);
	} else if (result_exponent <= 0) {	
		if (Is_underflowtrap_enabled()) {
                	Sgl_setwrapped_exponent(resultp1,result_exponent,unfl);
			Sgl_copytoptr(resultp1,dstptr);
                        if (inexact)
                            if (Is_inexacttrap_enabled())
                                return (OPC_2E_UNDERFLOWEXCEPTION |
					OPC_2E_INEXACTEXCEPTION);
                            else Set_inexactflag();
	    		return(OPC_2E_UNDERFLOWEXCEPTION);
		}
		else if (inexact && is_tiny) Set_underflowflag();
	}
	else Sgl_set_exponent(resultp1,result_exponent);
	Sgl_copytoptr(resultp1,dstptr);
	if (inexact) 
		if (Is_inexacttrap_enabled()) return(OPC_2E_INEXACTEXCEPTION);
		else Set_inexactflag();
    	return(NOEXCEPTION);
}

