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

#ifdef __NO_PA_HDRS
    PA header file -- do not include this header file for non-PA builds.
#endif



#ifndef HOSTWDSZ
#define	HOSTWDSZ	32
#endif



#define Bitfield_extract(start, length, object) 	\
    ((object) >> (HOSTWDSZ - (start) - (length)) & 	\
    ((unsigned)-1 >> (HOSTWDSZ - (length))))

#define Bitfield_signed_extract(start, length, object) \
    ((int)((object) << start) >> (HOSTWDSZ - (length)))

#define Bitfield_mask(start, len, object)		\
    ((object) & (((unsigned)-1 >> (HOSTWDSZ-len)) << (HOSTWDSZ-start-len)))

#define Bitfield_deposit(value,start,len,object)  object = \
    ((object) & ~(((unsigned)-1 >> (HOSTWDSZ-len)) << (HOSTWDSZ-start-len))) | \
    (((value) & ((unsigned)-1 >> (HOSTWDSZ-len))) << (HOSTWDSZ-start-len))
