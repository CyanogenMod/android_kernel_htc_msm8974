/*  *********************************************************************
    *  SB1250 Board Support Package
    *
    *  Global constants and macros		File: sb1250_defs.h
    *
    *  This file contains macros and definitions used by the other
    *  include files.
    *
    *  SB1250 specification level:  User's manual 1/02/02
    *
    *********************************************************************
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *
    *  This program is free software; you can redistribute it and/or
    *  modify it under the terms of the GNU General Public License as
    *  published by the Free Software Foundation; either version 2 of
    *  the License, or (at your option) any later version.
    *
    *  This program is distributed in the hope that it will be useful,
    *  but WITHOUT ANY WARRANTY; without even the implied warranty of
    *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    *  GNU General Public License for more details.
    *
    *  You should have received a copy of the GNU General Public License
    *  along with this program; if not, write to the Free Software
    *  Foundation, Inc., 59 Temple Place, Suite 330, Boston,
    *  MA 02111-1307 USA
    ********************************************************************* */

#ifndef _SB1250_DEFS_H
#define _SB1250_DEFS_H

#if !defined(__STDC__) && !defined(_MSC_VER)
#error SiByte headers require ANSI C89 support
#endif



#define	SIBYTE_HDR_FMASK_1250_ALL		0x000000ff
#define	SIBYTE_HDR_FMASK_1250_PASS1		0x00000001
#define	SIBYTE_HDR_FMASK_1250_PASS2		0x00000002
#define	SIBYTE_HDR_FMASK_1250_PASS3		0x00000004

#define	SIBYTE_HDR_FMASK_112x_ALL		0x00000f00
#define	SIBYTE_HDR_FMASK_112x_PASS1		0x00000100

#define SIBYTE_HDR_FMASK_1480_ALL		0x0000f000
#define SIBYTE_HDR_FMASK_1480_PASS1		0x00001000
#define SIBYTE_HDR_FMASK_1480_PASS2		0x00002000

#define	SIBYTE_HDR_FMASK(chip, pass)					\
    (SIBYTE_HDR_FMASK_ ## chip ## _ ## pass)
#define	SIBYTE_HDR_FMASK_ALLREVS(chip)					\
    (SIBYTE_HDR_FMASK_ ## chip ## _ALL)

#define	SIBYTE_HDR_FMASK_ALL						\
    (SIBYTE_HDR_FMASK_1250_ALL | SIBYTE_HDR_FMASK_112x_ALL		\
     | SIBYTE_HDR_FMASK_1480_ALL)

#define SIBYTE_HDR_FMASK_1250_112x_ALL					\
    (SIBYTE_HDR_FMASK_1250_ALL | SIBYTE_HDR_FMASK_112x_ALL)
#define SIBYTE_HDR_FMASK_1250_112x SIBYTE_HDR_FMASK_1250_112x_ALL

#ifndef SIBYTE_HDR_FEATURES
#define	SIBYTE_HDR_FEATURES			SIBYTE_HDR_FMASK_ALL
#endif


#define	SIBYTE_HDR_FMASK_BEFORE(chip, pass)				\
    ((SIBYTE_HDR_FMASK(chip, pass) - 1) & SIBYTE_HDR_FMASK_ALLREVS(chip))

#define	SIBYTE_HDR_FMASK_AFTER(chip, pass)				\
    (~(SIBYTE_HDR_FMASK(chip, pass)					\
     | (SIBYTE_HDR_FMASK(chip, pass) - 1)) & SIBYTE_HDR_FMASK_ALLREVS(chip))


#define SIBYTE_HDR_FEATURE_CHIP(chip)					\
    (!! (SIBYTE_HDR_FMASK_ALLREVS(chip) & SIBYTE_HDR_FEATURES))

#define SIBYTE_HDR_FEATURE_1250_112x \
      (SIBYTE_HDR_FEATURE_CHIP(1250) || SIBYTE_HDR_FEATURE_CHIP(112x))

#define SIBYTE_HDR_FEATURE(chip, pass)					\
    (!! ((SIBYTE_HDR_FMASK(chip, pass)					\
	  | SIBYTE_HDR_FMASK_AFTER(chip, pass)) & SIBYTE_HDR_FEATURES))

#define SIBYTE_HDR_FEATURE_EXACT(chip, pass)				\
    (!! (SIBYTE_HDR_FMASK(chip, pass) & SIBYTE_HDR_FEATURES))

#define SIBYTE_HDR_FEATURE_UP_TO(chip, pass)				\
    (!! ((SIBYTE_HDR_FMASK(chip, pass)					\
	 | SIBYTE_HDR_FMASK_BEFORE(chip, pass)) & SIBYTE_HDR_FEATURES))







#if !defined(__ASSEMBLY__)
#define _SB_MAKE64(x) ((uint64_t)(x))
#define _SB_MAKE32(x) ((uint32_t)(x))
#else
#define _SB_MAKE64(x) (x)
#define _SB_MAKE32(x) (x)
#endif



#define _SB_MAKEMASK1(n) (_SB_MAKE64(1) << _SB_MAKE64(n))
#define _SB_MAKEMASK1_32(n) (_SB_MAKE32(1) << _SB_MAKE32(n))


#define _SB_MAKEMASK(v, n) (_SB_MAKE64((_SB_MAKE64(1)<<(v))-1) << _SB_MAKE64(n))
#define _SB_MAKEMASK_32(v, n) (_SB_MAKE32((_SB_MAKE32(1)<<(v))-1) << _SB_MAKE32(n))


#define _SB_MAKEVALUE(v, n) (_SB_MAKE64(v) << _SB_MAKE64(n))
#define _SB_MAKEVALUE_32(v, n) (_SB_MAKE32(v) << _SB_MAKE32(n))

#define _SB_GETVALUE(v, n, m) ((_SB_MAKE64(v) & _SB_MAKE64(m)) >> _SB_MAKE64(n))
#define _SB_GETVALUE_32(v, n, m) ((_SB_MAKE32(v) & _SB_MAKE32(m)) >> _SB_MAKE32(n))



#if defined(__mips64) && !defined(__ASSEMBLY__)
#define SBWRITECSR(csr, val) *((volatile uint64_t *) PHYS_TO_K1(csr)) = (val)
#define SBREADCSR(csr) (*((volatile uint64_t *) PHYS_TO_K1(csr)))
#endif 

#endif
