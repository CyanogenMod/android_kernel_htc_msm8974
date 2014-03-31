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


#include "ieee754int.h"
#include "ieee754sp.h"
#include "ieee754dp.h"

#define DP_EBIAS	1023
#define DP_EMIN		(-1022)
#define DP_EMAX		1023

#define SP_EBIAS	127
#define SP_EMIN		(-126)
#define SP_EMAX		127



#if (defined(BYTE_ORDER) && BYTE_ORDER == LITTLE_ENDIAN) || defined(__MIPSEL__)
#define SPSTR(s, b, m) {m, b, s}
#define DPSTR(s, b, mh, ml) {ml, mh, b, s}
#endif

#ifdef __MIPSEB__
#define SPSTR(s, b, m) {s, b, m}
#define DPSTR(s, b, mh, ml) {s, b, mh, ml}
#endif

const struct ieee754dp_konst __ieee754dp_spcvals[] = {
	DPSTR(0, DP_EMIN - 1 + DP_EBIAS, 0, 0),	
	DPSTR(1, DP_EMIN - 1 + DP_EBIAS, 0, 0),	
	DPSTR(0, DP_EBIAS, 0, 0),	
	DPSTR(1, DP_EBIAS, 0, 0),	
	DPSTR(0, 3 + DP_EBIAS, 0x40000, 0),	
	DPSTR(1, 3 + DP_EBIAS, 0x40000, 0),	
	DPSTR(0, DP_EMAX + 1 + DP_EBIAS, 0, 0),	
	DPSTR(1, DP_EMAX + 1 + DP_EBIAS, 0, 0),	
	DPSTR(0, DP_EMAX+1+DP_EBIAS, 0x7FFFF, 0xFFFFFFFF), 
	DPSTR(0, DP_EMAX + DP_EBIAS, 0xFFFFF, 0xFFFFFFFF),	
	DPSTR(1, DP_EMAX + DP_EBIAS, 0xFFFFF, 0xFFFFFFFF),	
	DPSTR(0, DP_EMIN + DP_EBIAS, 0, 0),	
	DPSTR(1, DP_EMIN + DP_EBIAS, 0, 0),	
	DPSTR(0, DP_EMIN - 1 + DP_EBIAS, 0, 1),	
	DPSTR(1, DP_EMIN - 1 + DP_EBIAS, 0, 1),	
	DPSTR(0, 31 + DP_EBIAS, 0, 0),	
	DPSTR(0, 63 + DP_EBIAS, 0, 0),	
};

const struct ieee754sp_konst __ieee754sp_spcvals[] = {
	SPSTR(0, SP_EMIN - 1 + SP_EBIAS, 0),	
	SPSTR(1, SP_EMIN - 1 + SP_EBIAS, 0),	
	SPSTR(0, SP_EBIAS, 0),	
	SPSTR(1, SP_EBIAS, 0),	
	SPSTR(0, 3 + SP_EBIAS, 0x200000),	
	SPSTR(1, 3 + SP_EBIAS, 0x200000),	
	SPSTR(0, SP_EMAX + 1 + SP_EBIAS, 0),	
	SPSTR(1, SP_EMAX + 1 + SP_EBIAS, 0),	
	SPSTR(0, SP_EMAX+1+SP_EBIAS, 0x3FFFFF),     
	SPSTR(0, SP_EMAX + SP_EBIAS, 0x7FFFFF),	
	SPSTR(1, SP_EMAX + SP_EBIAS, 0x7FFFFF),	
	SPSTR(0, SP_EMIN + SP_EBIAS, 0),	
	SPSTR(1, SP_EMIN + SP_EBIAS, 0),	
	SPSTR(0, SP_EMIN - 1 + SP_EBIAS, 1),	
	SPSTR(1, SP_EMIN - 1 + SP_EBIAS, 1),	
	SPSTR(0, 31 + SP_EBIAS, 0),	
	SPSTR(0, 63 + SP_EBIAS, 0),	
};


int ieee754si_xcpt(int r, const char *op, ...)
{
	struct ieee754xctx ax;

	if (!TSTX())
		return r;
	ax.op = op;
	ax.rt = IEEE754_RT_SI;
	ax.rv.si = r;
	va_start(ax.ap, op);
	ieee754_xcpt(&ax);
	va_end(ax.ap);
	return ax.rv.si;
}

s64 ieee754di_xcpt(s64 r, const char *op, ...)
{
	struct ieee754xctx ax;

	if (!TSTX())
		return r;
	ax.op = op;
	ax.rt = IEEE754_RT_DI;
	ax.rv.di = r;
	va_start(ax.ap, op);
	ieee754_xcpt(&ax);
	va_end(ax.ap);
	return ax.rv.di;
}
