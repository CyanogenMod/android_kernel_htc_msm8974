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
dbl_fcmp (dbl_floating_point * leftptr, dbl_floating_point * rightptr,
	  unsigned int cond, unsigned int *status)
                                           
                       
                         
    {
    register unsigned int leftp1, leftp2, rightp1, rightp2;
    register int xorresult;
        
    
    Dbl_copyfromptr(leftptr,leftp1,leftp2);
    Dbl_copyfromptr(rightptr,rightp1,rightp2);
    if(    (Dbl_exponent(leftp1) == DBL_INFINITY_EXPONENT)
        || (Dbl_exponent(rightp1) == DBL_INFINITY_EXPONENT) )
	{
        if( ((Dbl_exponent(leftp1) == DBL_INFINITY_EXPONENT)
	    && Dbl_isnotzero_mantissa(leftp1,leftp2) 
	    && (Exception(cond) || Dbl_isone_signaling(leftp1)))
	   ||
	    ((Dbl_exponent(rightp1) == DBL_INFINITY_EXPONENT)
	    && Dbl_isnotzero_mantissa(rightp1,rightp2) 
	    && (Exception(cond) || Dbl_isone_signaling(rightp1))) )
	    {
	    if( Is_invalidtrap_enabled() ) {
	    	Set_status_cbit(Unordered(cond));
		return(INVALIDEXCEPTION);
	    }
	    else Set_invalidflag();
	    Set_status_cbit(Unordered(cond));
	    return(NOEXCEPTION);
	    }
        else if( ((Dbl_exponent(leftp1) == DBL_INFINITY_EXPONENT)
	    && Dbl_isnotzero_mantissa(leftp1,leftp2))
	   ||
	    ((Dbl_exponent(rightp1) == DBL_INFINITY_EXPONENT)
	    && Dbl_isnotzero_mantissa(rightp1,rightp2)) )
	    {
	    
	    Set_status_cbit(Unordered(cond));
	    return(NOEXCEPTION);
	    }
	
	}
    Dbl_xortointp1(leftp1,rightp1,xorresult);
    if( xorresult < 0 )
        {
        if( Dbl_iszero_exponentmantissa(leftp1,leftp2) 
	  && Dbl_iszero_exponentmantissa(rightp1,rightp2) )
            {
	    Set_status_cbit(Equal(cond));
	    }
	else if( Dbl_isone_sign(leftp1) )
	    {
	    Set_status_cbit(Lessthan(cond));
	    }
	else
	    {
	    Set_status_cbit(Greaterthan(cond));
	    }
        }
    else if(Dbl_isequal(leftp1,leftp2,rightp1,rightp2))
        {
        Set_status_cbit(Equal(cond));
        }
    else if( Dbl_iszero_sign(leftp1) )
        {
        
	if( Dbl_allp1(leftp1) < Dbl_allp1(rightp1) )
	    {
	    Set_status_cbit(Lessthan(cond));
	    }
	else if( Dbl_allp1(leftp1) > Dbl_allp1(rightp1) )
	    {
	    Set_status_cbit(Greaterthan(cond));
	    }
	else
	    {
	    if( Dbl_allp2(leftp2) < Dbl_allp2(rightp2) )
		{
		Set_status_cbit(Lessthan(cond));
		}
	    else 
		{
		Set_status_cbit(Greaterthan(cond));
		}
	    }
	}
    else
        {
	if( Dbl_allp1(leftp1) > Dbl_allp1(rightp1) )
	    {
	    Set_status_cbit(Lessthan(cond));
	    }
	else if( Dbl_allp1(leftp1) < Dbl_allp1(rightp1) )
	    {
	    Set_status_cbit(Greaterthan(cond));
	    }
	else
	    {
	    if( Dbl_allp2(leftp2) > Dbl_allp2(rightp2) )
		{
		Set_status_cbit(Lessthan(cond));
		}
	    else 
		{
		Set_status_cbit(Greaterthan(cond));
		}
	    }
        }
	return(NOEXCEPTION);
    }
