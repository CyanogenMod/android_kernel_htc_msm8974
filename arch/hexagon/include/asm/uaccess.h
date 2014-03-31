/*
 * User memory access support for Hexagon
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _ASM_UACCESS_H
#define _ASM_UACCESS_H
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/segment.h>
#include <asm/sections.h>

#define VERIFY_READ     0
#define VERIFY_WRITE    1

#define __access_ok(addr, size) \
	((get_fs().seg == KERNEL_DS.seg) || \
	(((unsigned long)addr < get_fs().seg) && \
	  (unsigned long)size < (get_fs().seg - (unsigned long)addr)))


unsigned long __copy_from_user_hexagon(void *to, const void __user *from,
				     unsigned long n);
unsigned long __copy_to_user_hexagon(void __user *to, const void *from,
				   unsigned long n);

#define __copy_from_user(to, from, n) __copy_from_user_hexagon(to, from, n)
#define __copy_to_user(to, from, n) __copy_to_user_hexagon(to, from, n)


__kernel_size_t __clear_user_hexagon(void __user *dest, unsigned long count);
#define __clear_user(a, s) __clear_user_hexagon((a), (s))

#define __strncpy_from_user(dst, src, n) hexagon_strncpy_from_user(dst, src, n)

#define __strnlen_user __strnlen_user

extern long __strnlen_user(const char __user *src, long n);

static inline long hexagon_strncpy_from_user(char *dst, const char __user *src,
					     long n);

#include <asm-generic/uaccess.h>

static inline long hexagon_strncpy_from_user(char *dst, const char __user *src,
					     long n)
{
	long res = __strnlen_user(src, n);

	

	if (res > n) {
		copy_from_user(dst, src, n);
		return n;
	} else {
		copy_from_user(dst, src, res);
		return res-1;
	}
}

#endif
