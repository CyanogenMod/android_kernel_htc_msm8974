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


#ifndef _ATOMIC_ARM_H
#define _ATOMIC_ARM_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#define INCLUDE_ALLOW_HOSTUSER
#define INCLUDE_ALLOW_GUESTUSER
#include "include_check.h"

#include "mvp_assert.h"

#define ATOMIC_ADDO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, add)

#define ATOMIC_ADDV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, add)

#define ATOMIC_ANDO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, and)

#define ATOMIC_ANDV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, and)

#define ATOMIC_GETO(atm) ({					\
	typeof((atm).atm_Normal) _oldval;			\
								\
	switch (sizeof(_oldval)) {				\
	case 4:							\
		asm volatile ("ldrex  %0, [%1]\n"		\
			      "clrex"				\
			      : "=&r" (_oldval)			\
			      : "r"   (&((atm).atm_Volatl)));	\
		break;						\
	case 8:							\
		asm volatile ("ldrexd %0, %H0, [%1]\n"		\
			      "clrex"				\
			      : "=&r" (_oldval)			\
			      : "r"   (&((atm).atm_Volatl)));	\
		break;						\
	default:						\
		FATAL();					\
	}							\
	_oldval;						\
})

#define ATOMIC_ORO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, orr)

#define ATOMIC_ORV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, orr)

#define ATOMIC_SETIF(atm, newval, oldval) ({		\
	int _failed;					\
	typeof((atm).atm_Normal) _newval = newval;	\
	typeof((atm).atm_Normal) _oldval = oldval;	\
							\
	ASSERT_ON_COMPILE(sizeof(_newval) == 4);	\
	asm volatile ("1: ldrex    %0, [%1]\n"		\
		      "   cmp      %0, %2\n"		\
		      "   mov      %0, #2\n"		\
		      "   IT       eq\n"		\
		      "   strexeq  %0, %3, [%1]\n"	\
		      "   cmp      %0, #1\n"		\
		      "   beq      1b\n"		\
		      "   clrex"			\
		      : "=&r" (_failed)			\
		      : "r"   (&((atm).atm_Volatl)),	\
		      "r"   (_oldval),			\
		      "r"   (_newval)			\
		      : "cc", "memory");		\
	!_failed;					\
})


#define ATOMIC_SETO(atm, newval) ({				\
	int _failed;						\
	typeof((atm).atm_Normal) _newval = newval;		\
	typeof((atm).atm_Normal) _oldval;			\
								\
	switch (sizeof(_newval)) {				\
	case 4:							\
		asm volatile ("1: ldrex   %0, [%2]\n"		\
			      "   strex   %1, %3, [%2]\n"	\
			      "   teq     %1, #0\n"		\
			      "   bne     1b"			\
			      : "=&r" (_oldval),		\
			      "=&r" (_failed)			\
			      : "r"   (&((atm).atm_Volatl)),	\
			      "r"   (_newval)			\
			      : "cc", "memory");		\
		break;						\
	case 8:							\
		asm volatile ("1: ldrexd  %0, %H0, [%2]\n"	\
			      "   strexd  %1, %3, %H3, [%2]\n"	\
			      "   teq     %1, #0\n"		\
			      "   bne     1b"			\
			      : "=&r" (_oldval),		\
			      "=&r" (_failed)			\
			      : "r"   (&((atm).atm_Volatl)),	\
			      "r"   (_newval)			\
			      : "cc", "memory");		\
		break;						\
	default:						\
		FATAL();					\
	}							\
	_oldval;						\
})

#define ATOMIC_SETV(atm, newval) ATOMIC_SETO((atm), (newval))

#define ATOMIC_SUBO(atm, modval) ATOMIC_OPO_PRIVATE(atm, modval, sub)

#define ATOMIC_SUBV(atm, modval) ATOMIC_OPV_PRIVATE(atm, modval, sub)

#define ATOMIC_OPO_PRIVATE(atm, modval, op) ({		\
	int _failed;					\
	typeof((atm).atm_Normal) _modval = modval;	\
	typeof((atm).atm_Normal) _oldval;		\
	typeof((atm).atm_Normal) _newval;		\
							\
	ASSERT_ON_COMPILE(sizeof(_modval) == 4);	\
	asm volatile ("1: ldrex    %0, [%3]\n"		\
		      #op "    %1, %0, %4\n"		\
		      "   strex    %2, %1, [%3]\n"	\
		      "   teq      %2, #0\n"		\
		      "   bne      1b"			\
		      : "=&r" (_oldval),		\
		      "=&r" (_newval),			\
		      "=&r" (_failed)			\
		      : "r"   (&((atm).atm_Volatl)),	\
		      "r"   (_modval)			\
		      : "cc", "memory");		\
	_oldval;					\
})

#define ATOMIC_OPV_PRIVATE(atm, modval, op) do {	\
	int _failed;					\
	typeof((atm).atm_Normal) _modval = modval;	\
	typeof((atm).atm_Normal) _sample;		\
							\
	ASSERT_ON_COMPILE(sizeof(_modval) == 4);	\
	asm volatile ("1: ldrex    %0, [%2]\n"		\
		      #op "    %0, %3\n"		\
		      "   strex    %1, %0, [%2]\n"	\
		      "   teq      %1, #0\n"		\
		      "   bne      1b"			\
		      : "=&r" (_sample),		\
		      "=&r" (_failed)			\
		      : "r"   (&((atm).atm_Volatl)),	\
		      "r"   (_modval)			\
		      : "cc", "memory");		\
} while (0)

#define ATOMIC_SINGLE_COPY_WRITE32(p, val) do {	\
	ASSERT(sizeof(val) == 4);		\
	ASSERT((MVA)(p) % sizeof(val) == 0);	\
	asm volatile("str %0, [%1]"		\
		     :				\
		     : "r" (val), "r" (p)	\
		     : "memory");		\
} while (0)


#define ATOMIC_SINGLE_COPY_READ32(p) ({		\
	ASSERT((MVA)(p) % sizeof(uint32) == 0);	\
	uint32 _val;				\
	asm volatile("ldr %0, [%1]"		\
		     : "=r" (_val)		\
		     : "r" (p));		\
	_val;					\
})

#define ATOMIC_SINGLE_COPY_WRITE64(p, val) do {		\
	ASSERT(sizeof(val) == 8);			\
	ASSERT((MVA)(p) % sizeof(val) == 0);		\
	asm volatile("mov  r0, %0\n"			\
		     "mov  r1, %1\n"			\
		     "strd r0, r1, [%2]"		\
		     :					\
		     : "r" ((uint32)(val)),		\
		     "r" (((uint64)(val)) >> 32),	\
		     "r" (p)				\
		     : "r0", "r1", "memory");		\
} while (0)

#endif
