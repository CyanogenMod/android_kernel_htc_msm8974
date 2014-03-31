/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2008 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/

#ifndef __CVMX_ASM_H__
#define __CVMX_ASM_H__

#include "octeon-model.h"

#define CVMX_SYNC asm volatile ("sync" : : : "memory")
#define CVMX_SYNCW_STR "syncw\nsyncw\n"
#ifdef __OCTEON__

#define CVMX_SYNCIO asm volatile ("nop")

#define CVMX_SYNCIOBDMA asm volatile ("synciobdma" : : : "memory")

#define CVMX_SYNCIOALL asm volatile ("nop")

#define CVMX_SYNCW asm volatile ("syncw\n\tsyncw" : : : "memory")

#define CVMX_SYNCWS CVMX_SYNCW
#define CVMX_SYNCS  CVMX_SYNC
#define CVMX_SYNCWS_STR CVMX_SYNCW_STR
#else
#define CVMX_SYNCIO asm volatile ("nop")

#define CVMX_SYNCIOBDMA asm volatile ("sync" : : : "memory")

#define CVMX_SYNCIOALL asm volatile ("nop")

#define CVMX_SYNCW asm volatile ("sync" : : : "memory")
#define CVMX_SYNCWS CVMX_SYNCW
#define CVMX_SYNCS  CVMX_SYNC
#define CVMX_SYNCWS_STR CVMX_SYNCW_STR
#endif

#define CVMX_PREPARE_FOR_STORE(address, offset) \
	asm volatile ("pref 30, " CVMX_TMP_STR(offset) "(%[rbase])" : : \
	[rbase] "d" (address))
#define CVMX_DONT_WRITE_BACK(address, offset) \
	asm volatile ("pref 29, " CVMX_TMP_STR(offset) "(%[rbase])" : : \
	[rbase] "d" (address))

#define CVMX_ICACHE_INVALIDATE \
	{ CVMX_SYNC; asm volatile ("synci 0($0)" : : ); }

#define CVMX_ICACHE_INVALIDATE2 \
	{ CVMX_SYNC; asm volatile ("cache 0, 0($0)" : : ); }

#define CVMX_DCACHE_INVALIDATE \
	{ CVMX_SYNC; asm volatile ("cache 9, 0($0)" : : ); }

#define CVMX_CACHE(op, address, offset)					\
	asm volatile ("cache " CVMX_TMP_STR(op) ", " CVMX_TMP_STR(offset) "(%[rbase])" \
		: : [rbase] "d" (address) )
#define CVMX_CACHE_LCKL2(address, offset) CVMX_CACHE(31, address, offset)
#define CVMX_CACHE_WBIL2(address, offset) CVMX_CACHE(23, address, offset)
#define CVMX_CACHE_WBIL2I(address, offset) CVMX_CACHE(3, address, offset)
#define CVMX_CACHE_LTGL2I(address, offset) CVMX_CACHE(7, address, offset)

#define CVMX_POP(result, input) \
	asm ("pop %[rd],%[rs]" : [rd] "=d" (result) : [rs] "d" (input))
#define CVMX_DPOP(result, input) \
	asm ("dpop %[rd],%[rs]" : [rd] "=d" (result) : [rs] "d" (input))

#define CVMX_RDHWR(result, regstr) \
	asm volatile ("rdhwr %[rt],$" CVMX_TMP_STR(regstr) : [rt] "=d" (result))
#define CVMX_RDHWRNV(result, regstr) \
	asm ("rdhwr %[rt],$" CVMX_TMP_STR(regstr) : [rt] "=d" (result))
#endif 
