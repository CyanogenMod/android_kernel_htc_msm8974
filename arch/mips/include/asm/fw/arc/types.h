/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright 1999 Ralf Baechle (ralf@gnu.org)
 * Copyright 1999 Silicon Graphics, Inc.
 */
#ifndef _ASM_ARC_TYPES_H
#define _ASM_ARC_TYPES_H


#ifdef CONFIG_ARC32

typedef char		CHAR;
typedef short		SHORT;
typedef long		LARGE_INTEGER __attribute__ ((__mode__ (__DI__)));
typedef	long		LONG __attribute__ ((__mode__ (__SI__)));
typedef unsigned char	UCHAR;
typedef unsigned short	USHORT;
typedef unsigned long	ULONG __attribute__ ((__mode__ (__SI__)));
typedef void		VOID;

typedef LONG		_PCHAR;
typedef LONG		_PSHORT;
typedef LONG		_PLARGE_INTEGER;
typedef	LONG		_PLONG;
typedef LONG		_PUCHAR;
typedef LONG		_PUSHORT;
typedef LONG		_PULONG;
typedef LONG		_PVOID;

#endif 

#ifdef CONFIG_ARC64

typedef char		CHAR;
typedef short		SHORT;
typedef long		LARGE_INTEGER __attribute__ ((__mode__ (__DI__)));
typedef	long		LONG __attribute__ ((__mode__ (__DI__)));
typedef unsigned char	UCHAR;
typedef unsigned short	USHORT;
typedef unsigned long	ULONG __attribute__ ((__mode__ (__DI__)));
typedef void		VOID;

typedef CHAR		*_PCHAR;
typedef SHORT		*_PSHORT;
typedef LARGE_INTEGER	*_PLARGE_INTEGER;
typedef	LONG		*_PLONG;
typedef UCHAR		*_PUCHAR;
typedef USHORT		*_PUSHORT;
typedef ULONG		*_PULONG;
typedef VOID		*_PVOID;

#endif 

typedef CHAR		*PCHAR;
typedef SHORT		*PSHORT;
typedef LARGE_INTEGER	*PLARGE_INTEGER;
typedef	LONG		*PLONG;
typedef UCHAR		*PUCHAR;
typedef USHORT		*PUSHORT;
typedef ULONG		*PULONG;
typedef VOID		*PVOID;

typedef struct {
	USHORT	CursorXPosition;
	USHORT	CursorYPosition;
	USHORT	CursorMaxXPosition;
	USHORT	CursorMaxYPosition;
	USHORT	ForegroundColor;
	USHORT	BackgroundColor;
	UCHAR	HighIntensity;
	UCHAR	Underscored;
	UCHAR	ReverseVideo;
} DISPLAY_STATUS;

#endif 
