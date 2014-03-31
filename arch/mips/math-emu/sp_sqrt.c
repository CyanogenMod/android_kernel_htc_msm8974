/*
 * MIPS floating point support
 * Copyright (C) 1994-2000 Algorithmics Ltd.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 */


#include "ieee754sp.h"

ieee754sp ieee754sp_sqrt(ieee754sp x)
{
	int ix, s, q, m, t, i;
	unsigned int r;
	COMPXSP;

	

	EXPLODEXSP;
	CLEARCX;
	FLUSHXSP;

	
	switch (xc) {
	case IEEE754_CLASS_QNAN:
		
		return ieee754sp_nanxcpt(x, "sqrt");
	case IEEE754_CLASS_SNAN:
		SETCX(IEEE754_INVALID_OPERATION);
		return ieee754sp_nanxcpt(ieee754sp_indef(), "sqrt");
	case IEEE754_CLASS_ZERO:
		
		return x;
	case IEEE754_CLASS_INF:
		if (xs) {
			
			SETCX(IEEE754_INVALID_OPERATION);
			return ieee754sp_nanxcpt(ieee754sp_indef(), "sqrt");
		}
		
		return x;
	case IEEE754_CLASS_DNORM:
	case IEEE754_CLASS_NORM:
		if (xs) {
			
			SETCX(IEEE754_INVALID_OPERATION);
			return ieee754sp_nanxcpt(ieee754sp_indef(), "sqrt");
		}
		break;
	}

	ix = x.bits;

	
	m = (ix >> 23);
	if (m == 0) {		
		for (i = 0; (ix & 0x00800000) == 0; i++)
			ix <<= 1;
		m -= i - 1;
	}
	m -= 127;		
	ix = (ix & 0x007fffff) | 0x00800000;
	if (m & 1)		
		ix += ix;
	m >>= 1;		

	
	ix += ix;
	q = s = 0;		
	r = 0x01000000;		

	while (r != 0) {
		t = s + r;
		if (t <= ix) {
			s = t + r;
			ix -= t;
			q += r;
		}
		ix += ix;
		r >>= 1;
	}

	if (ix != 0) {
		SETCX(IEEE754_INEXACT);
		switch (ieee754_csr.rm) {
		case IEEE754_RP:
			q += 2;
			break;
		case IEEE754_RN:
			q += (q & 1);
			break;
		}
	}
	ix = (q >> 1) + 0x3f000000;
	ix += (m << 23);
	x.bits = ix;
	return x;
}
