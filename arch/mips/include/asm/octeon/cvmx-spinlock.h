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


#ifndef __CVMX_SPINLOCK_H__
#define __CVMX_SPINLOCK_H__

#include "cvmx-asm.h"



typedef struct {
	volatile uint32_t value;
} cvmx_spinlock_t;

#define  CVMX_SPINLOCK_UNLOCKED_VAL  0
#define  CVMX_SPINLOCK_LOCKED_VAL    1

#define CVMX_SPINLOCK_UNLOCKED_INITIALIZER  {CVMX_SPINLOCK_UNLOCKED_VAL}

static inline void cvmx_spinlock_init(cvmx_spinlock_t *lock)
{
	lock->value = CVMX_SPINLOCK_UNLOCKED_VAL;
}

static inline int cvmx_spinlock_locked(cvmx_spinlock_t *lock)
{
	return lock->value != CVMX_SPINLOCK_UNLOCKED_VAL;
}

static inline void cvmx_spinlock_unlock(cvmx_spinlock_t *lock)
{
	CVMX_SYNCWS;
	lock->value = 0;
	CVMX_SYNCWS;
}


static inline unsigned int cvmx_spinlock_trylock(cvmx_spinlock_t *lock)
{
	unsigned int tmp;

	__asm__ __volatile__(".set noreorder         \n"
			     "1: ll   %[tmp], %[val] \n"
			
			     "   bnez %[tmp], 2f     \n"
			     "   li   %[tmp], 1      \n"
			     "   sc   %[tmp], %[val] \n"
			     "   beqz %[tmp], 1b     \n"
			     "   li   %[tmp], 0      \n"
			     "2:                     \n"
			     ".set reorder           \n" :
			[val] "+m"(lock->value), [tmp] "=&r"(tmp)
			     : : "memory");

	return tmp != 0;		
}

static inline void cvmx_spinlock_lock(cvmx_spinlock_t *lock)
{
	unsigned int tmp;

	__asm__ __volatile__(".set noreorder         \n"
			     "1: ll   %[tmp], %[val]  \n"
			     "   bnez %[tmp], 1b     \n"
			     "   li   %[tmp], 1      \n"
			     "   sc   %[tmp], %[val] \n"
			     "   beqz %[tmp], 1b     \n"
			     "   nop                \n"
			     ".set reorder           \n" :
			[val] "+m"(lock->value), [tmp] "=&r"(tmp)
			: : "memory");

}


static inline void cvmx_spinlock_bit_lock(uint32_t *word)
{
	unsigned int tmp;
	unsigned int sav;

	__asm__ __volatile__(".set noreorder         \n"
			     ".set noat              \n"
			     "1: ll    %[tmp], %[val]  \n"
			     "   bbit1 %[tmp], 31, 1b    \n"
			     "   li    $at, 1      \n"
			     "   ins   %[tmp], $at, 31, 1  \n"
			     "   sc    %[tmp], %[val] \n"
			     "   beqz  %[tmp], 1b     \n"
			     "   nop                \n"
			     ".set at              \n"
			     ".set reorder           \n" :
			[val] "+m"(*word), [tmp] "=&r"(tmp), [sav] "=&r"(sav)
			     : : "memory");

}

static inline unsigned int cvmx_spinlock_bit_trylock(uint32_t *word)
{
	unsigned int tmp;

	__asm__ __volatile__(".set noreorder\n\t"
			     ".set noat\n"
			     "1: ll    %[tmp], %[val] \n"
			
			     "   bbit1 %[tmp], 31, 2f     \n"
			     "   li    $at, 1      \n"
			     "   ins   %[tmp], $at, 31, 1  \n"
			     "   sc    %[tmp], %[val] \n"
			     "   beqz  %[tmp], 1b     \n"
			     "   li    %[tmp], 0      \n"
			     "2:                     \n"
			     ".set at              \n"
			     ".set reorder           \n" :
			[val] "+m"(*word), [tmp] "=&r"(tmp)
			: : "memory");

	return tmp != 0;		
}

static inline void cvmx_spinlock_bit_unlock(uint32_t *word)
{
	CVMX_SYNCWS;
	*word &= ~(1UL << 31);
	CVMX_SYNCWS;
}

#endif 
