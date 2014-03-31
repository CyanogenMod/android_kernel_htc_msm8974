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


#ifndef _ATOMIC_H
#define _ATOMIC_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#include "include_check.h"

#define ATOMIC(t) union { t atm_Normal; t volatile atm_Volatl; }

#define ATOMIC_INI(v) { .atm_Normal = v }

typedef ATOMIC(int32)  AtmSInt32 __attribute__((aligned(4)));
typedef ATOMIC(uint32) AtmUInt32 __attribute__((aligned(4)));
typedef ATOMIC(uint64) AtmUInt64 __attribute__((aligned(8)));

#if defined(__COVERITY__)
#include "atomic_coverity.h"
#elif defined(__arm__)
#include "atomic_arm.h"
#elif defined(__i386) || defined(__x86_64)
#include "atomic_x86.h"
#endif

#endif
