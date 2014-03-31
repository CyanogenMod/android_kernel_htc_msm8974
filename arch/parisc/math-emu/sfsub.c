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
sgl_fsub(
	    sgl_floating_point *leftptr,
	    sgl_floating_point *rightptr,
	    sgl_floating_point *dstptr,
	    unsigned int *status)
    {
    register unsigned int left, right, result, extent;
    register unsigned int signless_upper_left, signless_upper_right, save;
    
    register int result_exponent, right_exponent, diff_exponent;
    register int sign_save, jumpsize;
    register boolean inexact = FALSE, underflowtrap;
        
    
    left = *leftptr;
    right = *rightptr;

    Sgl_xortointp1(left,right,save);

    if ((result_exponent = Sgl_exponent(left)) == SGL_INFINITY_EXPONENT)
	{
	if (Sgl_iszero_mantissa(left)) 
	    {
	    if (Sgl_isnotnan(right)) 
		{
		if (Sgl_isinfinity(right) && save==0) 
		    {
		    if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);
                    Set_invalidflag();
                    Sgl_makequietnan(result);
		    *dstptr = result;
		    return(NOEXCEPTION);
		    }
		*dstptr = left;
		return(NOEXCEPTION);
		}
	    }
	else 
	    {
            if (Sgl_isone_signaling(left)) 
		{
               	
		if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);
        	
        	Set_invalidflag();
        	Sgl_set_quiet(left);
        	}
	    else if (Sgl_is_signalingnan(right)) 
		{
        	
               	if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);
		
		Set_invalidflag();
		Sgl_set_quiet(right);
		*dstptr = right;
		return(NOEXCEPTION);
		}
 	    *dstptr = left;
 	    return(NOEXCEPTION);
	    }
	} 
    if (Sgl_isinfinity_exponent(right)) 
	{
	if (Sgl_iszero_mantissa(right)) 
	    {
	    
	    Sgl_invert_sign(right);
	    *dstptr = right;
	    return(NOEXCEPTION);
	    }
        if (Sgl_isone_signaling(right)) 
	    {
            
	    if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);
	    
	    Set_invalidflag();
	    Sgl_set_quiet(right);
	    }
	*dstptr = right;
	return(NOEXCEPTION);
    	} 

    

    
    Sgl_copytoint_exponentmantissa(left,signless_upper_left);
    Sgl_copytoint_exponentmantissa(right,signless_upper_right);

    
    if(Sgl_ismagnitudeless(signless_upper_left,signless_upper_right))
	{
	Sgl_xorfromintp1(save,right,right);
	Sgl_xorfromintp1(save,left,left);
	result_exponent = Sgl_exponent(left);
	Sgl_invert_sign(left);
	}
     

    if((right_exponent = Sgl_exponent(right)) == 0)
        {
	
	if(Sgl_iszero_mantissa(right)) 
	    {
	    
	    if(Sgl_iszero_exponentmantissa(left))
		{
		
		Sgl_invert_sign(right);
		if(Is_rounding_mode(ROUNDMINUS))
		    {
		    Sgl_or_signs(left,right);
		    }
		else
		    {
		    Sgl_and_signs(left,right);
		    }
		}
	    else 
		{
		if( (result_exponent == 0) && Is_underflowtrap_enabled() )
		    {
		    
	    	    sign_save = Sgl_signextendedsign(left);
		    Sgl_leftshiftby1(left);
		    Sgl_normalize(left,result_exponent);
		    Sgl_set_sign(left,sign_save);
                    Sgl_setwrapped_exponent(left,result_exponent,unfl);
		    *dstptr = left;
		    
		    return(UNDERFLOWEXCEPTION);
		    }
		}
	    *dstptr = left;
	    return(NOEXCEPTION);
	    }

	
	Sgl_clear_sign(right);	
	if(result_exponent == 0 )
	    {
	    if( (int) save >= 0 )
		{
		Sgl_subtract(left,right,result);
		if(Sgl_iszero_mantissa(result))
		    {
		    if(Is_rounding_mode(ROUNDMINUS))
			{
			Sgl_setone_sign(result);
			}
		    else
			{
			Sgl_setzero_sign(result);
			}
		    *dstptr = result;
		    return(NOEXCEPTION);
		    }
		}
	    else
		{
		Sgl_addition(left,right,result);
		if(Sgl_isone_hidden(result))
		    {
		    *dstptr = result;
		    return(NOEXCEPTION);
		    }
		}
	    if(Is_underflowtrap_enabled())
		{
		
	    	sign_save = Sgl_signextendedsign(result);
		Sgl_leftshiftby1(result);
		Sgl_normalize(result,result_exponent);
		Sgl_set_sign(result,sign_save);
                Sgl_setwrapped_exponent(result,result_exponent,unfl);
		*dstptr = result;
		
		return(UNDERFLOWEXCEPTION);
		}
	    *dstptr = result;
	    return(NOEXCEPTION);
	    }
	right_exponent = 1;	
	}
    else
	{
	Sgl_clear_signexponent_set_hidden(right);
	}
    Sgl_clear_exponent_set_hidden(left);
    diff_exponent = result_exponent - right_exponent;

    if(diff_exponent > SGL_THRESHOLD)
	{
	diff_exponent = SGL_THRESHOLD;
	}
    
    
    Sgl_right_align(right,diff_exponent,
      extent);

    
    if( (int) save >= 0 )
	{
	Sgl_subtract_withextension(left,right,extent,result);
	if(Sgl_iszero_hidden(result))
	    {
	    
	    sign_save = Sgl_signextendedsign(result);
            Sgl_leftshiftby1_withextent(result,extent,result);

    	    if(Sgl_iszero(result))
		
		{
		if(Is_rounding_mode(ROUNDMINUS)) Sgl_setone_sign(result);
		*dstptr = result;
		return(NOEXCEPTION);
		}
	    result_exponent--;
	    
	    if(Sgl_isone_hidden(result))
		{
		if(result_exponent==0)
		    {
		    goto underflow;
		    }
		else
		    {
		    
		    Sgl_set_sign(result,sign_save);
	    	    Ext_leftshiftby1(extent);
		    goto round;
		    }
		}

	    if(!(underflowtrap = Is_underflowtrap_enabled()) &&
	       result_exponent==0) goto underflow;

	    Ext_leftshiftby1(extent);

	    
	    while(Sgl_iszero_hiddenhigh7mantissa(result))
		{
		Sgl_leftshiftby8(result);
		if((result_exponent -= 8) <= 0  && !underflowtrap)
		    goto underflow;
		}
	    
	    if(Sgl_iszero_hiddenhigh3mantissa(result))
		{
		
		Sgl_leftshiftby4(result);
		if((result_exponent -= 4) <= 0 && !underflowtrap)
		    goto underflow;
		}
	    if((jumpsize = Sgl_hiddenhigh3mantissa(result)) > 7)
		{
		
		if(result_exponent <= 0) goto underflow;
		Sgl_set_sign(result,sign_save);
		Sgl_set_exponent(result,result_exponent);
		*dstptr = result;
		return(NOEXCEPTION);
		}
	    Sgl_sethigh4bits(result,sign_save);
	    switch(jumpsize) 
		{
		case 1:
		    {
		    Sgl_leftshiftby3(result);
		    result_exponent -= 3;
		    break;
		    }
		case 2:
		case 3:
		    {
		    Sgl_leftshiftby2(result);
		    result_exponent -= 2;
		    break;
		    }
		case 4:
		case 5:
		case 6:
		case 7:
		    {
		    Sgl_leftshiftby1(result);
		    result_exponent -= 1;
		    break;
		    }
		}
	    if(result_exponent > 0) 
		{
		Sgl_set_exponent(result,result_exponent);
		*dstptr = result;	
		return(NOEXCEPTION);
		}
	    
	  underflow:
	    if(Is_underflowtrap_enabled())
		{
		Sgl_set_sign(result,sign_save);
                Sgl_setwrapped_exponent(result,result_exponent,unfl);
		*dstptr = result;
		
		return(UNDERFLOWEXCEPTION);
		}
	    Sgl_right_align(result,(1-result_exponent),extent);
	    Sgl_clear_signexponent(result);
	    Sgl_set_sign(result,sign_save);
	    *dstptr = result;
	    return(NOEXCEPTION);
	    } 
	
	} 
    else 
	{
	
	Sgl_addition(left,right,result);
	if(Sgl_isone_hiddenoverflow(result))
	    {
	    
	    Sgl_rightshiftby1_withextent(result,extent,extent);
	    Sgl_arithrightshiftby1(result);
	    result_exponent++;
	    } 
	} 
    
  round:
    if(Ext_isnotzero(extent))
	{
	inexact = TRUE;
	switch(Rounding_mode())
	    {
	    case ROUNDNEAREST: 
	    if(Ext_isone_sign(extent))
		{
		
		if(Ext_isnotzero_lower(extent)  ||
		  Sgl_isone_lowmantissa(result))
		    {
		    
		    Sgl_increment(result);
		    }
		}
	    break;

	    case ROUNDPLUS:
	    if(Sgl_iszero_sign(result))
		{
		
		Sgl_increment(result);
		}
	    break;
	    
	    case ROUNDMINUS:
	    if(Sgl_isone_sign(result))
		{
		
		Sgl_increment(result);
		}
	    
	    case ROUNDZERO:;
	    
	    } 
	if(Sgl_isone_hiddenoverflow(result)) result_exponent++;
	}
    if(result_exponent == SGL_INFINITY_EXPONENT)
        {
        
        if(Is_overflowtrap_enabled())
	    {
	    Sgl_setwrapped_exponent(result,result_exponent,ovfl);
	    *dstptr = result;
	    if (inexact)
		if (Is_inexacttrap_enabled())
		    return(OVERFLOWEXCEPTION | INEXACTEXCEPTION);
		else Set_inexactflag();
	    return(OVERFLOWEXCEPTION);
	    }
        else
	    {
	    Set_overflowflag();
	    inexact = TRUE;
	    Sgl_setoverflow(result);
	    }
	}
    else Sgl_set_exponent(result,result_exponent);
    *dstptr = result;
    if(inexact) 
	if(Is_inexacttrap_enabled()) return(INEXACTEXCEPTION);
	else Set_inexactflag();
    return(NOEXCEPTION);
    }
