/*
 * params.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * This file defines host and target properties for all machines
 * supported by the dynamic loader.  To be tedious...
 *
 * host: the machine on which the dynamic loader runs
 * target: the machine that the dynamic loader is loading
 *
 * Host and target may or may not be the same, depending upon the particular
 * use.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#define BITS_PER_BYTE 8		
#define LOG_BITS_PER_BYTE 3	
#define BYTE_MASK ((1U<<BITS_PER_BYTE)-1)

#if defined(__TMS320C55X__) || defined(_TMS320C5XX)
#define BITS_PER_AU 16
#define LOG_BITS_PER_AU 4
 
#define FMT_UI32 "0x%lx"
#define FMT8_UI32 "%08lx"	
#else
#define BITS_PER_AU 8
#define LOG_BITS_PER_AU 3
#define FMT_UI32 "0x%x"
#define FMT8_UI32 "%08x"
#endif

#define SWAP32BY16(zz) (((zz) << 16) | ((zz) >> 16))
#define SWAP16BY8(zz) (((zz) << 8) | ((zz) >> 8))



#if TMS32060
#define MEMORG          0x0L	
#define MEMSIZE         0x0L	

#define CINIT_ALIGN     8	
#define CINIT_COUNT	4	
#define CINIT_ADDRESS	4	
#define CINIT_PAGE_BITS	0	

#define LENIENT_SIGNED_RELEXPS 0	

#undef TARGET_ENDIANNESS	

#define TARGET_WORD_ALIGN(zz) (((zz) + 0x3) & -0x4)
#endif

#ifndef TARGET_AU_BITS
#define TARGET_AU_BITS 8	
#define LOG_TARGET_AU_BITS 3	
#endif

#ifndef CINIT_DEFAULT_PAGE
#define CINIT_DEFAULT_PAGE 0	
#endif

#ifndef DATA_RUN2LOAD
#define DATA_RUN2LOAD(zz) (zz)	
#endif

#ifndef DBG_LIST_PAGE
#define DBG_LIST_PAGE 0		
#endif

#ifndef TARGET_WORD_ALIGN
#define TARGET_WORD_ALIGN(zz) (zz)
#endif

#ifndef TDATA_TO_TADDR
#define TDATA_TO_TADDR(zz) (zz)	
#define TADDR_TO_TDATA(zz) (zz)	
#define TDATA_AU_BITS	TARGET_AU_BITS	
#define LOG_TDATA_AU_BITS	LOG_TARGET_AU_BITS
#endif


#if LOG_BITS_PER_AU == LOG_TARGET_AU_BITS
#define TADDR_TO_HOST(x) (x)
#define HOST_TO_TADDR(x) (x)
#elif LOG_BITS_PER_AU > LOG_TARGET_AU_BITS
#define TADDR_TO_HOST(x) ((x) >> (LOG_BITS_PER_AU-LOG_TARGET_AU_BITS))
#define HOST_TO_TADDR(x) ((x) << (LOG_BITS_PER_AU-LOG_TARGET_AU_BITS))
#else
#define TADDR_TO_HOST(x) ((x) << (LOG_TARGET_AU_BITS-LOG_BITS_PER_AU))
#define HOST_TO_TADDR(x) ((x) >> (LOG_TARGET_AU_BITS-LOG_BITS_PER_AU))
#endif

#if LOG_BITS_PER_AU == LOG_TDATA_AU_BITS
#define TDATA_TO_HOST(x) (x)
#define HOST_TO_TDATA(x) (x)
#define HOST_TO_TDATA_ROUND(x) (x)
#define BYTE_TO_HOST_TDATA_ROUND(x) BYTE_TO_HOST_ROUND(x)
#elif LOG_BITS_PER_AU > LOG_TDATA_AU_BITS
#define TDATA_TO_HOST(x) ((x) >> (LOG_BITS_PER_AU-LOG_TDATA_AU_BITS))
#define HOST_TO_TDATA(x) ((x) << (LOG_BITS_PER_AU-LOG_TDATA_AU_BITS))
#define HOST_TO_TDATA_ROUND(x) ((x) << (LOG_BITS_PER_AU-LOG_TDATA_AU_BITS))
#define BYTE_TO_HOST_TDATA_ROUND(x) BYTE_TO_HOST_ROUND(x)
#else
#define TDATA_TO_HOST(x) ((x) << (LOG_TDATA_AU_BITS-LOG_BITS_PER_AU))
#define HOST_TO_TDATA(x) ((x) >> (LOG_TDATA_AU_BITS-LOG_BITS_PER_AU))
#define HOST_TO_TDATA_ROUND(x) (((x) +\
				(1<<(LOG_TDATA_AU_BITS-LOG_BITS_PER_AU))-1) >>\
				(LOG_TDATA_AU_BITS-LOG_BITS_PER_AU))
#define BYTE_TO_HOST_TDATA_ROUND(x) (BYTE_TO_HOST((x) +\
	(1<<(LOG_TDATA_AU_BITS-LOG_BITS_PER_BYTE))-1) &\
	-(TDATA_AU_BITS/BITS_PER_AU))
#endif

#if LOG_BITS_PER_AU == LOG_BITS_PER_BYTE
#define BYTE_TO_HOST(x) (x)
#define BYTE_TO_HOST_ROUND(x) (x)
#define HOST_TO_BYTE(x) (x)
#elif LOG_BITS_PER_AU >= LOG_BITS_PER_BYTE
#define BYTE_TO_HOST(x) ((x) >> (LOG_BITS_PER_AU - LOG_BITS_PER_BYTE))
#define BYTE_TO_HOST_ROUND(x) ((x + (BITS_PER_AU/BITS_PER_BYTE-1)) >>\
			      (LOG_BITS_PER_AU - LOG_BITS_PER_BYTE))
#define HOST_TO_BYTE(x) ((x) << (LOG_BITS_PER_AU - LOG_BITS_PER_BYTE))
#else
#endif

#if LOG_TARGET_AU_BITS == LOG_BITS_PER_BYTE
#define TADDR_TO_BYTE(x) (x)
#define BYTE_TO_TADDR(x) (x)
#elif LOG_TARGET_AU_BITS > LOG_BITS_PER_BYTE
#define TADDR_TO_BYTE(x) ((x) << (LOG_TARGET_AU_BITS-LOG_BITS_PER_BYTE))
#define BYTE_TO_TADDR(x) ((x) >> (LOG_TARGET_AU_BITS-LOG_BITS_PER_BYTE))
#else
#endif

#ifdef _BIG_ENDIAN
#define HOST_ENDIANNESS 1
#else
#define HOST_ENDIANNESS 0
#endif

#ifdef TARGET_ENDIANNESS
#define TARGET_ENDIANNESS_DIFFERS(rtend) (HOST_ENDIANNESS^TARGET_ENDIANNESS)
#elif HOST_ENDIANNESS
#define TARGET_ENDIANNESS_DIFFERS(rtend) (!(rtend))
#else
#define TARGET_ENDIANNESS_DIFFERS(rtend) (rtend)
#endif

#if TARGET_AU_BITS <= 8
typedef u8 tgt_au_t;
#elif TARGET_AU_BITS <= 16
typedef u16 tgt_au_t;
#else
typedef u32 tgt_au_t;
#endif

#if TARGET_AU_BITS < BITS_PER_AU
#define TGTAU_BITS BITS_PER_AU
#define LOG_TGTAU_BITS LOG_BITS_PER_AU
#else
#define TGTAU_BITS TARGET_AU_BITS
#define LOG_TGTAU_BITS LOG_TARGET_AU_BITS
#endif
