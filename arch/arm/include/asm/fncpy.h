/*
 * arch/arm/include/asm/fncpy.h - helper macros for function body copying
 *
 * Copyright (C) 2011 Linaro Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#ifndef __ASM_FNCPY_H
#define __ASM_FNCPY_H

#include <linux/types.h>
#include <linux/string.h>

#include <asm/bug.h>
#include <asm/cacheflush.h>

#define FNCPY_ALIGN 8

#define fncpy(dest_buf, funcp, size) ({					\
	uintptr_t __funcp_address;					\
	typeof(funcp) __result;						\
									\
	asm("" : "=r" (__funcp_address) : "0" (funcp));			\
									\
								\
	BUG_ON((uintptr_t)(dest_buf) & (FNCPY_ALIGN - 1) ||		\
		(__funcp_address & ~(uintptr_t)1 & (FNCPY_ALIGN - 1)));	\
									\
	memcpy(dest_buf, (void const *)(__funcp_address & ~1), size);	\
	flush_icache_range((unsigned long)(dest_buf),			\
		(unsigned long)(dest_buf) + (size));			\
									\
	asm("" : "=r" (__result)					\
		: "0" ((uintptr_t)(dest_buf) | (__funcp_address & 1)));	\
									\
	__result;							\
})

#endif 
