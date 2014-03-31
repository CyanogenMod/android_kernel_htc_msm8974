/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5


#ifndef _MVPTYPES_H
#define _MVPTYPES_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#define INCLUDE_ALLOW_WORKSTATION
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long long  uint64;

typedef signed char         int8;
typedef short               int16;
typedef int                 int32;
typedef long long           int64;

typedef uint32 CVA;      
typedef uint32 GVA;      
typedef uint32 MVA;      
typedef uint32 HKVA;     
typedef uint32 HUVA;     
typedef uint64 PA;       
typedef uint32 MA;       

typedef uint32 PPN;       
typedef uint32 MPN;       

typedef uint64 cycle_t;

typedef struct {
	uint16 off;
	uint16 len;
} PageSeg;


#if defined(__GNUC__)
# define PRINTF_DECL(fmtPos, varPos) \
	__attribute__((__format__(__printf__, fmtPos, varPos)))
#else
# define PRINTF_DECL(fmtPos, varPos)
#endif

#if defined(__GNUC__)
# define SCANF_DECL(fmtPos, varPos) \
	__attribute__((__format__(__scanf__, fmtPos, varPos)))
#else
# define SCANF_DECL(fmtPos, varPos)
#endif

#endif 
