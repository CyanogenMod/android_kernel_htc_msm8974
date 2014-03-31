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


#include "ieee754dp.h"

int ieee754dp_class(ieee754dp x)
{
	COMPXDP;
	EXPLODEXDP;
	return xc;
}

int ieee754dp_isnan(ieee754dp x)
{
	return ieee754dp_class(x) >= IEEE754_CLASS_SNAN;
}

int ieee754dp_issnan(ieee754dp x)
{
	assert(ieee754dp_isnan(x));
	return ((DPMANT(x) & DP_MBIT(DP_MBITS-1)) == DP_MBIT(DP_MBITS-1));
}


ieee754dp ieee754dp_xcpt(ieee754dp r, const char *op, ...)
{
	struct ieee754xctx ax;
	if (!TSTX())
		return r;

	ax.op = op;
	ax.rt = IEEE754_RT_DP;
	ax.rv.dp = r;
	va_start(ax.ap, op);
	ieee754_xcpt(&ax);
	va_end(ax.ap);
	return ax.rv.dp;
}

ieee754dp ieee754dp_nanxcpt(ieee754dp r, const char *op, ...)
{
	struct ieee754xctx ax;

	assert(ieee754dp_isnan(r));

	if (!ieee754dp_issnan(r))	
		return r;

	if (!SETANDTESTCX(IEEE754_INVALID_OPERATION)) {
		
		DPMANT(r) &= (~DP_MBIT(DP_MBITS-1));
		if (ieee754dp_isnan(r))
			return r;
		else
			return ieee754dp_indef();
	}

	ax.op = op;
	ax.rt = 0;
	ax.rv.dp = r;
	va_start(ax.ap, op);
	ieee754_xcpt(&ax);
	va_end(ax.ap);
	return ax.rv.dp;
}

ieee754dp ieee754dp_bestnan(ieee754dp x, ieee754dp y)
{
	assert(ieee754dp_isnan(x));
	assert(ieee754dp_isnan(y));

	if (DPMANT(x) > DPMANT(y))
		return x;
	else
		return y;
}


static u64 get_rounding(int sn, u64 xm)
{
	if (xm & (DP_MBIT(3) - 1)) {
		switch (ieee754_csr.rm) {
		case IEEE754_RZ:
			break;
		case IEEE754_RN:
			xm += 0x3 + ((xm >> 3) & 1);
			
			break;
		case IEEE754_RU:	
			if (!sn)	
				xm += 0x8;
			break;
		case IEEE754_RD:	
			if (sn)	
				xm += 0x8;
			break;
		}
	}
	return xm;
}


ieee754dp ieee754dp_format(int sn, int xe, u64 xm)
{
	assert(xm);		

	assert((xm >> (DP_MBITS + 1 + 3)) == 0);	
	assert(xm & (DP_HIDDEN_BIT << 3));

	if (xe < DP_EMIN) {
		
		int es = DP_EMIN - xe;

		if (ieee754_csr.nod) {
			SETCX(IEEE754_UNDERFLOW);
			SETCX(IEEE754_INEXACT);

			switch(ieee754_csr.rm) {
			case IEEE754_RN:
			case IEEE754_RZ:
				return ieee754dp_zero(sn);
			case IEEE754_RU:    
				if(sn == 0)
					return ieee754dp_min(0);
				else
					return ieee754dp_zero(1);
			case IEEE754_RD:    
				if(sn == 0)
					return ieee754dp_zero(0);
				else
					return ieee754dp_min(1);
			}
		}

		if (xe == DP_EMIN - 1
				&& get_rounding(sn, xm) >> (DP_MBITS + 1 + 3))
		{
			
			SETCX(IEEE754_INEXACT);
			xm = get_rounding(sn, xm);
			xm >>= 1;
			
			xm &= ~(DP_MBIT(3) - 1);
			xe++;
		}
		else {
			xm = XDPSRS(xm, es);
			xe += es;
			assert((xm & (DP_HIDDEN_BIT << 3)) == 0);
			assert(xe == DP_EMIN);
		}
	}
	if (xm & (DP_MBIT(3) - 1)) {
		SETCX(IEEE754_INEXACT);
		if ((xm & (DP_HIDDEN_BIT << 3)) == 0) {
			SETCX(IEEE754_UNDERFLOW);
		}

		xm = get_rounding(sn, xm);
		if (xm >> (DP_MBITS + 3 + 1)) {
			
			xm >>= 1;
			xe++;
		}
	}
	
	xm >>= 3;

	assert((xm >> (DP_MBITS + 1)) == 0);	
	assert(xe >= DP_EMIN);

	if (xe > DP_EMAX) {
		SETCX(IEEE754_OVERFLOW);
		SETCX(IEEE754_INEXACT);
		
		switch (ieee754_csr.rm) {
		case IEEE754_RN:
			return ieee754dp_inf(sn);
		case IEEE754_RZ:
			return ieee754dp_max(sn);
		case IEEE754_RU:	
			if (sn == 0)
				return ieee754dp_inf(0);
			else
				return ieee754dp_max(1);
		case IEEE754_RD:	
			if (sn == 0)
				return ieee754dp_max(0);
			else
				return ieee754dp_inf(1);
		}
	}
	

	if ((xm & DP_HIDDEN_BIT) == 0) {
		
		assert(xe == DP_EMIN);
		if (ieee754_csr.mx & IEEE754_UNDERFLOW)
			SETCX(IEEE754_UNDERFLOW);
		return builddp(sn, DP_EMIN - 1 + DP_EBIAS, xm);
	} else {
		assert((xm >> (DP_MBITS + 1)) == 0);	
		assert(xm & DP_HIDDEN_BIT);

		return builddp(sn, xe + DP_EBIAS, xm & ~DP_HIDDEN_BIT);
	}
}
