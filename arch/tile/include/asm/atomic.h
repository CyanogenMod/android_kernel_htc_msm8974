/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 *
 * Atomic primitives.
 */

#ifndef _ASM_TILE_ATOMIC_H
#define _ASM_TILE_ATOMIC_H

#include <asm/cmpxchg.h>

#ifndef __ASSEMBLY__

#include <linux/compiler.h>
#include <linux/types.h>

#define ATOMIC_INIT(i)	{ (i) }

static inline int atomic_read(const atomic_t *v)
{
	return ACCESS_ONCE(v->counter);
}

#define atomic_sub_return(i, v)		atomic_add_return((int)(-(i)), (v))

#define atomic_sub(i, v)		atomic_add((int)(-(i)), (v))

#define atomic_sub_and_test(i, v)	(atomic_sub_return((i), (v)) == 0)

#define atomic_inc_return(v)		atomic_add_return(1, (v))

#define atomic_dec_return(v)		atomic_sub_return(1, (v))

#define atomic_inc(v)			atomic_add(1, (v))

#define atomic_dec(v)			atomic_sub(1, (v))

#define atomic_dec_and_test(v)		(atomic_dec_return(v) == 0)

#define atomic_inc_and_test(v)		(atomic_inc_return(v) == 0)

#define atomic_add_negative(i, v)	(atomic_add_return((i), (v)) < 0)

#endif 

#ifndef __tilegx__
#include <asm/atomic_32.h>
#else
#include <asm/atomic_64.h>
#endif

#endif 
