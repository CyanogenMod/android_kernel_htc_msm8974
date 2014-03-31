/*
 * OpenRISC Linux
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * OpenRISC implementation:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 * et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ASM_OPENRISC_UACCESS_H
#define __ASM_OPENRISC_UACCESS_H

#include <linux/errno.h>
#include <linux/thread_info.h>
#include <linux/prefetch.h>
#include <linux/string.h>
#include <asm/page.h>

#define VERIFY_READ	0
#define VERIFY_WRITE	1



#define KERNEL_DS	(~0UL)
#define get_ds()	(KERNEL_DS)

#define USER_DS		(TASK_SIZE)
#define get_fs()	(current_thread_info()->addr_limit)
#define set_fs(x)	(current_thread_info()->addr_limit = (x))

#define segment_eq(a, b)	((a) == (b))

#define __range_ok(addr, size) (size <= get_fs() && addr <= (get_fs()-size))

#define __addr_ok(addr) ((unsigned long) addr < get_fs())

#define access_ok(type, addr, size) \
	__range_ok((unsigned long)addr, (unsigned long)size)


struct exception_table_entry {
	unsigned long insn, fixup;
};

extern unsigned long search_exception_table(unsigned long);
extern void sort_exception_table(void);

#define get_user(x, ptr) \
	__get_user_check((x), (ptr), sizeof(*(ptr)))
#define put_user(x, ptr) \
	__put_user_check((__typeof__(*(ptr)))(x), (ptr), sizeof(*(ptr)))

#define __get_user(x, ptr) \
	__get_user_nocheck((x), (ptr), sizeof(*(ptr)))
#define __put_user(x, ptr) \
	__put_user_nocheck((__typeof__(*(ptr)))(x), (ptr), sizeof(*(ptr)))

extern long __put_user_bad(void);

#define __put_user_nocheck(x, ptr, size)		\
({							\
	long __pu_err;					\
	__put_user_size((x), (ptr), (size), __pu_err);	\
	__pu_err;					\
})

#define __put_user_check(x, ptr, size)					\
({									\
	long __pu_err = -EFAULT;					\
	__typeof__(*(ptr)) *__pu_addr = (ptr);				\
	if (access_ok(VERIFY_WRITE, __pu_addr, size))			\
		__put_user_size((x), __pu_addr, (size), __pu_err);	\
	__pu_err;							\
})

#define __put_user_size(x, ptr, size, retval)				\
do {									\
	retval = 0;							\
	switch (size) {							\
	case 1: __put_user_asm(x, ptr, retval, "l.sb"); break;		\
	case 2: __put_user_asm(x, ptr, retval, "l.sh"); break;		\
	case 4: __put_user_asm(x, ptr, retval, "l.sw"); break;		\
	case 8: __put_user_asm2(x, ptr, retval); break;			\
	default: __put_user_bad();					\
	}								\
} while (0)

struct __large_struct {
	unsigned long buf[100];
};
#define __m(x) (*(struct __large_struct *)(x))

#define __put_user_asm(x, addr, err, op)			\
	__asm__ __volatile__(					\
		"1:	"op" 0(%2),%1\n"			\
		"2:\n"						\
		".section .fixup,\"ax\"\n"			\
		"3:	l.addi %0,r0,%3\n"			\
		"	l.j 2b\n"				\
		"	l.nop\n"				\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 2\n"				\
		"	.long 1b,3b\n"				\
		".previous"					\
		: "=r"(err)					\
		: "r"(x), "r"(addr), "i"(-EFAULT), "0"(err))

#define __put_user_asm2(x, addr, err)				\
	__asm__ __volatile__(					\
		"1:	l.sw 0(%2),%1\n"			\
		"2:	l.sw 4(%2),%H1\n"			\
		"3:\n"						\
		".section .fixup,\"ax\"\n"			\
		"4:	l.addi %0,r0,%3\n"			\
		"	l.j 3b\n"				\
		"	l.nop\n"				\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 2\n"				\
		"	.long 1b,4b\n"				\
		"	.long 2b,4b\n"				\
		".previous"					\
		: "=r"(err)					\
		: "r"(x), "r"(addr), "i"(-EFAULT), "0"(err))

#define __get_user_nocheck(x, ptr, size)			\
({								\
	long __gu_err, __gu_val;				\
	__get_user_size(__gu_val, (ptr), (size), __gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})

#define __get_user_check(x, ptr, size)					\
({									\
	long __gu_err = -EFAULT, __gu_val = 0;				\
	const __typeof__(*(ptr)) * __gu_addr = (ptr);			\
	if (access_ok(VERIFY_READ, __gu_addr, size))			\
		__get_user_size(__gu_val, __gu_addr, (size), __gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})

extern long __get_user_bad(void);

#define __get_user_size(x, ptr, size, retval)				\
do {									\
	retval = 0;							\
	switch (size) {							\
	case 1: __get_user_asm(x, ptr, retval, "l.lbz"); break;		\
	case 2: __get_user_asm(x, ptr, retval, "l.lhz"); break;		\
	case 4: __get_user_asm(x, ptr, retval, "l.lwz"); break;		\
	case 8: __get_user_asm2(x, ptr, retval);			\
	default: (x) = __get_user_bad();				\
	}								\
} while (0)

#define __get_user_asm(x, addr, err, op)		\
	__asm__ __volatile__(				\
		"1:	"op" %1,0(%2)\n"		\
		"2:\n"					\
		".section .fixup,\"ax\"\n"		\
		"3:	l.addi %0,r0,%3\n"		\
		"	l.addi %1,r0,0\n"		\
		"	l.j 2b\n"			\
		"	l.nop\n"			\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
		"	.align 2\n"			\
		"	.long 1b,3b\n"			\
		".previous"				\
		: "=r"(err), "=r"(x)			\
		: "r"(addr), "i"(-EFAULT), "0"(err))

#define __get_user_asm2(x, addr, err)			\
	__asm__ __volatile__(				\
		"1:	l.lwz %1,0(%2)\n"		\
		"2:	l.lwz %H1,4(%2)\n"		\
		"3:\n"					\
		".section .fixup,\"ax\"\n"		\
		"4:	l.addi %0,r0,%3\n"		\
		"	l.addi %1,r0,0\n"		\
		"	l.addi %H1,r0,0\n"		\
		"	l.j 3b\n"			\
		"	l.nop\n"			\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
		"	.align 2\n"			\
		"	.long 1b,4b\n"			\
		"	.long 2b,4b\n"			\
		".previous"				\
		: "=r"(err), "=&r"(x)			\
		: "r"(addr), "i"(-EFAULT), "0"(err))


extern unsigned long __must_check
__copy_tofrom_user(void *to, const void *from, unsigned long size);

#define __copy_from_user(to, from, size) \
	__copy_tofrom_user(to, from, size)
#define __copy_to_user(to, from, size) \
	__copy_tofrom_user(to, from, size)

#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

static inline unsigned long
copy_from_user(void *to, const void *from, unsigned long n)
{
	unsigned long over;

	if (access_ok(VERIFY_READ, from, n))
		return __copy_tofrom_user(to, from, n);
	if ((unsigned long)from < TASK_SIZE) {
		over = (unsigned long)from + n - TASK_SIZE;
		return __copy_tofrom_user(to, from, n - over) + over;
	}
	return n;
}

static inline unsigned long
copy_to_user(void *to, const void *from, unsigned long n)
{
	unsigned long over;

	if (access_ok(VERIFY_WRITE, to, n))
		return __copy_tofrom_user(to, from, n);
	if ((unsigned long)to < TASK_SIZE) {
		over = (unsigned long)to + n - TASK_SIZE;
		return __copy_tofrom_user(to, from, n - over) + over;
	}
	return n;
}

extern unsigned long __clear_user(void *addr, unsigned long size);

static inline __must_check unsigned long
clear_user(void *addr, unsigned long size)
{

	if (access_ok(VERIFY_WRITE, addr, size))
		return __clear_user(addr, size);
	if ((unsigned long)addr < TASK_SIZE) {
		unsigned long over = (unsigned long)addr + size - TASK_SIZE;
		return __clear_user(addr, size - over) + over;
	}
	return size;
}

extern int __strncpy_from_user(char *dst, const char *src, long count);

static inline long strncpy_from_user(char *dst, const char *src, long count)
{
	if (access_ok(VERIFY_READ, src, 1))
		return __strncpy_from_user(dst, src, count);
	return -EFAULT;
}


extern int __strnlen_user(const char *str, long len, unsigned long top);

static inline long strnlen_user(const char __user *str, long len)
{
	unsigned long top = (unsigned long)get_fs();
	unsigned long res = 0;

	if (__addr_ok(str))
		res = __strnlen_user(str, len, top);

	return res;
}

#define strlen_user(str) strnlen_user(str, TASK_SIZE-1)

#endif 
