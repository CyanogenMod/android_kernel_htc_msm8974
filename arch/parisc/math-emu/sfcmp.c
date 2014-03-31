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
sgl_fcmp (sgl_floating_point * leftptr, sgl_floating_point * rightptr,
	  unsigned int cond, unsigned int *status)
                                           
                       
                         
    {
    register unsigned int left, right;
    register int xorresult;
        
    
    left = *leftptr;
    right = *rightptr;

    if(    (Sgl_exponent(left) == SGL_INFINITY_EXPONENT)
        || (Sgl_exponent(right) == SGL_INFINITY_EXPONENT) )
	{
        if( (  (Sgl_exponent(left) == SGL_INFINITY_EXPONENT)
	    && Sgl_isnotzero_mantissa(left) 
	    && (Exception(cond) || Sgl_isone_signaling(left)))
	   ||
	    (  (Sgl_exponent(right) == SGL_INFINITY_EXPONENT)
	    && Sgl_isnotzero_mantissa(right) 
	    && (Exception(cond) || Sgl_isone_signaling(right)) ) )
	    {
	    if( Is_invalidtrap_enabled() ) {
	    	Set_status_cbit(Unordered(cond));
		return(INVALIDEXCEPTION);
	    }
	    else Set_invalidflag();
	    Set_status_cbit(Unordered(cond));
	    return(NOEXCEPTION);
	    }
        else if( ((Sgl_exponent(left) == SGL_INFINITY_EXPONENT)
	    && Sgl_isnotzero_mantissa(left))
	   ||
	    ((Sgl_exponent(right) == SGL_INFINITY_EXPONENT)
	    && Sgl_isnotzero_mantissa(right)) )
	    {
	    
	    Set_status_cbit(Unordered(cond));
	    return(NOEXCEPTION);
	    }
	
	}
    Sgl_xortointp1(left,right,xorresult);
    if( xorresult < 0 )
        {
        if( Sgl_iszero_exponentmantissa(left) 
	  && Sgl_iszero_exponentmantissa(right) )
            {
	    Set_status_cbit(Equal(cond));
	    }
	else if( Sgl_isone_sign(left) )
	    {
	    Set_status_cbit(Lessthan(cond));
	    }
	else
	    {
	    Set_status_cbit(Greaterthan(cond));
	    }
        }
    else if( Sgl_all(left) == Sgl_all(right) )
        {
        Set_status_cbit(Equal(cond));
        }
    else if( Sgl_iszero_sign(left) )
        {
        
        if( Sgl_all(left) < Sgl_all(right) )
	    {
	    Set_status_cbit(Lessthan(cond));
	    }
	else
	    {
	    Set_status_cbit(Greaterthan(cond));
	    }
	}
    else
        {
        if( Sgl_all(left) > Sgl_all(right) )
	    {
	    Set_status_cbit(Lessthan(cond));
	    }
        else
	    {
	    Set_status_cbit(Greaterthan(cond));
	    }
        }
	return(NOEXCEPTION);
    }
