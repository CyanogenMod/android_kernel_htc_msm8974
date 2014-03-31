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

ieee754sp ieee754sp_mul(ieee754sp x, ieee754sp y)
{
	COMPXSP;
	COMPYSP;

	EXPLODEXSP;
	EXPLODEYSP;

	CLEARCX;

	FLUSHXSP;
	FLUSHYSP;

	switch (CLPAIR(xc, yc)) {
	case CLPAIR(IEEE754_CLASS_SNAN, IEEE754_CLASS_QNAN):
	case CLPAIR(IEEE754_CLASS_QNAN, IEEE754_CLASS_SNAN):
	case CLPAIR(IEEE754_CLASS_SNAN, IEEE754_CLASS_SNAN):
	case CLPAIR(IEEE754_CLASS_ZERO, IEEE754_CLASS_SNAN):
	case CLPAIR(IEEE754_CLASS_NORM, IEEE754_CLASS_SNAN):
	case CLPAIR(IEEE754_CLASS_DNORM, IEEE754_CLASS_SNAN):
	case CLPAIR(IEEE754_CLASS_INF, IEEE754_CLASS_SNAN):
	case CLPAIR(IEEE754_CLASS_SNAN, IEEE754_CLASS_ZERO):
	case CLPAIR(IEEE754_CLASS_SNAN, IEEE754_CLASS_NORM):
	case CLPAIR(IEEE754_CLASS_SNAN, IEEE754_CLASS_DNORM):
	case CLPAIR(IEEE754_CLASS_SNAN, IEEE754_CLASS_INF):
		SETCX(IEEE754_INVALID_OPERATION);
		return ieee754sp_nanxcpt(ieee754sp_indef(), "mul", x, y);

	case CLPAIR(IEEE754_CLASS_ZERO, IEEE754_CLASS_QNAN):
	case CLPAIR(IEEE754_CLASS_NORM, IEEE754_CLASS_QNAN):
	case CLPAIR(IEEE754_CLASS_DNORM, IEEE754_CLASS_QNAN):
	case CLPAIR(IEEE754_CLASS_INF, IEEE754_CLASS_QNAN):
		return y;

	case CLPAIR(IEEE754_CLASS_QNAN, IEEE754_CLASS_QNAN):
	case CLPAIR(IEEE754_CLASS_QNAN, IEEE754_CLASS_ZERO):
	case CLPAIR(IEEE754_CLASS_QNAN, IEEE754_CLASS_NORM):
	case CLPAIR(IEEE754_CLASS_QNAN, IEEE754_CLASS_DNORM):
	case CLPAIR(IEEE754_CLASS_QNAN, IEEE754_CLASS_INF):
		return x;


		

	case CLPAIR(IEEE754_CLASS_INF, IEEE754_CLASS_ZERO):
	case CLPAIR(IEEE754_CLASS_ZERO, IEEE754_CLASS_INF):
		SETCX(IEEE754_INVALID_OPERATION);
		return ieee754sp_xcpt(ieee754sp_indef(), "mul", x, y);

	case CLPAIR(IEEE754_CLASS_NORM, IEEE754_CLASS_INF):
	case CLPAIR(IEEE754_CLASS_DNORM, IEEE754_CLASS_INF):
	case CLPAIR(IEEE754_CLASS_INF, IEEE754_CLASS_NORM):
	case CLPAIR(IEEE754_CLASS_INF, IEEE754_CLASS_DNORM):
	case CLPAIR(IEEE754_CLASS_INF, IEEE754_CLASS_INF):
		return ieee754sp_inf(xs ^ ys);

	case CLPAIR(IEEE754_CLASS_ZERO, IEEE754_CLASS_ZERO):
	case CLPAIR(IEEE754_CLASS_ZERO, IEEE754_CLASS_NORM):
	case CLPAIR(IEEE754_CLASS_ZERO, IEEE754_CLASS_DNORM):
	case CLPAIR(IEEE754_CLASS_NORM, IEEE754_CLASS_ZERO):
	case CLPAIR(IEEE754_CLASS_DNORM, IEEE754_CLASS_ZERO):
		return ieee754sp_zero(xs ^ ys);


	case CLPAIR(IEEE754_CLASS_DNORM, IEEE754_CLASS_DNORM):
		SPDNORMX;

	case CLPAIR(IEEE754_CLASS_NORM, IEEE754_CLASS_DNORM):
		SPDNORMY;
		break;

	case CLPAIR(IEEE754_CLASS_DNORM, IEEE754_CLASS_NORM):
		SPDNORMX;
		break;

	case CLPAIR(IEEE754_CLASS_NORM, IEEE754_CLASS_NORM):
		break;
	}
	
	assert(xm & SP_HIDDEN_BIT);
	assert(ym & SP_HIDDEN_BIT);

	{
		int re = xe + ye;
		int rs = xs ^ ys;
		unsigned rm;

		
		xm <<= 32 - (SP_MBITS + 1);
		ym <<= 32 - (SP_MBITS + 1);

		{
			unsigned short lxm = xm & 0xffff;
			unsigned short hxm = xm >> 16;
			unsigned short lym = ym & 0xffff;
			unsigned short hym = ym >> 16;
			unsigned lrm;
			unsigned hrm;

			lrm = lxm * lym;	
			hrm = hxm * hym;	

			{
				unsigned t = lxm * hym;	
				{
					unsigned at = lrm + (t << 16);
					hrm += at < lrm;
					lrm = at;
				}
				hrm = hrm + (t >> 16);
			}

			{
				unsigned t = hxm * lym;	
				{
					unsigned at = lrm + (t << 16);
					hrm += at < lrm;
					lrm = at;
				}
				hrm = hrm + (t >> 16);
			}
			rm = hrm | (lrm != 0);
		}

		if ((int) rm < 0) {
			rm = (rm >> (32 - (SP_MBITS + 1 + 3))) |
			    ((rm << (SP_MBITS + 1 + 3)) != 0);
			re++;
		} else {
			rm = (rm >> (32 - (SP_MBITS + 1 + 3 + 1))) |
			    ((rm << (SP_MBITS + 1 + 3 + 1)) != 0);
		}
		assert(rm & (SP_HIDDEN_BIT << 3));

		SPNORMRET2(rs, re, rm, "mul", x, y);
	}
}
