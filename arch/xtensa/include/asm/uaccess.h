/*
 * include/asm-xtensa/uaccess.h
 *
 * User space memory access functions
 *
 * These routines provide basic accessing functions to the user memory
 * space for the kernel. This header file provides functions such as:
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_UACCESS_H
#define _XTENSA_UACCESS_H

#include <linux/errno.h>
#ifndef __ASSEMBLY__
#include <linux/prefetch.h>
#endif
#include <asm/types.h>

#define VERIFY_READ    0
#define VERIFY_WRITE   1

#ifdef __ASSEMBLY__

#include <asm/current.h>
#include <asm/asm-offsets.h>
#include <asm/processor.h>


#define KERNEL_DS	0
#define USER_DS		1

#define get_ds		(KERNEL_DS)

	.macro	get_fs	ad, sp
	GET_CURRENT(\ad,\sp)
	l32i	\ad, \ad, THREAD_CURRENT_DS
	.endm

	.macro	set_fs	at, av, sp
	GET_CURRENT(\at,\sp)
	s32i	\av, \at, THREAD_CURRENT_DS
	.endm


#if ((KERNEL_DS != 0) || (USER_DS == 0))
# error Assembly macro kernel_ok fails
#endif
	.macro	kernel_ok  at, sp, success
	get_fs	\at, \sp
	beqz	\at, \success
	.endm

	.macro	user_ok	aa, as, at, error
	movi	\at, __XTENSA_UL_CONST(TASK_SIZE)
	bgeu	\as, \at, \error
	sub	\at, \at, \as
	bgeu	\aa, \at, \error
	.endm

	.macro	access_ok  aa, as, at, sp, error
	kernel_ok  \at, \sp, .Laccess_ok_\@
	user_ok    \aa, \as, \at, \error
.Laccess_ok_\@:
	.endm

#else 

#include <linux/sched.h>


#define KERNEL_DS	((mm_segment_t) { 0 })
#define USER_DS		((mm_segment_t) { 1 })

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current->thread.current_ds)
#define set_fs(val)	(current->thread.current_ds = (val))

#define segment_eq(a,b)	((a).seg == (b).seg)

#define __kernel_ok (segment_eq(get_fs(), KERNEL_DS))
#define __user_ok(addr,size) (((size) <= TASK_SIZE)&&((addr) <= TASK_SIZE-(size)))
#define __access_ok(addr,size) (__kernel_ok || __user_ok((addr),(size)))
#define access_ok(type,addr,size) __access_ok((unsigned long)(addr),(size))

#define put_user(x,ptr)	__put_user_check((x),(ptr),sizeof(*(ptr)))
#define get_user(x,ptr) __get_user_check((x),(ptr),sizeof(*(ptr)))

#define __put_user(x,ptr) __put_user_nocheck((x),(ptr),sizeof(*(ptr)))
#define __get_user(x,ptr) __get_user_nocheck((x),(ptr),sizeof(*(ptr)))


extern long __put_user_bad(void);

#define __put_user_nocheck(x,ptr,size)			\
({							\
	long __pu_err;					\
	__put_user_size((x),(ptr),(size),__pu_err);	\
	__pu_err;					\
})

#define __put_user_check(x,ptr,size)				\
({								\
	long __pu_err = -EFAULT;				\
	__typeof__(*(ptr)) *__pu_addr = (ptr);			\
	if (access_ok(VERIFY_WRITE,__pu_addr,size))		\
		__put_user_size((x),__pu_addr,(size),__pu_err);	\
	__pu_err;						\
})

#define __put_user_size(x,ptr,size,retval)				\
do {									\
	int __cb;							\
	retval = 0;							\
	switch (size) {							\
        case 1: __put_user_asm(x,ptr,retval,1,"s8i",__cb);  break;	\
        case 2: __put_user_asm(x,ptr,retval,2,"s16i",__cb); break;	\
        case 4: __put_user_asm(x,ptr,retval,4,"s32i",__cb); break;	\
        case 8: {							\
		     __typeof__(*ptr) __v64 = x;			\
		     retval = __copy_to_user(ptr,&__v64,8);		\
		     break;						\
	        }							\
	default: __put_user_bad();					\
	}								\
} while (0)



#define __check_align_1  ""

#define __check_align_2				\
	"   _bbci.l %3,  0, 1f		\n"	\
	"   movi    %0, %4		\n"	\
	"   _j      2f			\n"

#define __check_align_4				\
	"   _bbsi.l %3,  0, 0f		\n"	\
	"   _bbci.l %3,  1, 1f		\n"	\
	"0: movi    %0, %4		\n"	\
	"   _j      2f			\n"


#define __put_user_asm(x, addr, err, align, insn, cb)	\
   __asm__ __volatile__(				\
	__check_align_##align				\
	"1: "insn"  %2, %3, 0		\n"		\
	"2:				\n"		\
	"   .section  .fixup,\"ax\"	\n"		\
	"   .align 4			\n"		\
	"4:				\n"		\
	"   .long  2b			\n"		\
	"5:				\n"		\
	"   l32r   %1, 4b		\n"		\
        "   movi   %0, %4		\n"		\
        "   jx     %1			\n"		\
	"   .previous			\n"		\
	"   .section  __ex_table,\"a\"	\n"		\
	"   .long	1b, 5b		\n"		\
	"   .previous"					\
	:"=r" (err), "=r" (cb)				\
	:"r" ((int)(x)), "r" (addr), "i" (-EFAULT), "0" (err))

#define __get_user_nocheck(x,ptr,size)				\
({								\
	long __gu_err, __gu_val;				\
	__get_user_size(__gu_val,(ptr),(size),__gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})

#define __get_user_check(x,ptr,size)					\
({									\
	long __gu_err = -EFAULT, __gu_val = 0;				\
	const __typeof__(*(ptr)) *__gu_addr = (ptr);			\
	if (access_ok(VERIFY_READ,__gu_addr,size))			\
		__get_user_size(__gu_val,__gu_addr,(size),__gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})

extern long __get_user_bad(void);

#define __get_user_size(x,ptr,size,retval)				\
do {									\
	int __cb;							\
	retval = 0;							\
        switch (size) {							\
          case 1: __get_user_asm(x,ptr,retval,1,"l8ui",__cb);  break;	\
          case 2: __get_user_asm(x,ptr,retval,2,"l16ui",__cb); break;	\
          case 4: __get_user_asm(x,ptr,retval,4,"l32i",__cb);  break;	\
          case 8: retval = __copy_from_user(&x,ptr,8);    break;	\
          default: (x) = __get_user_bad();				\
        }								\
} while (0)


#define __get_user_asm(x, addr, err, align, insn, cb) \
   __asm__ __volatile__(			\
	__check_align_##align			\
	"1: "insn"  %2, %3, 0		\n"	\
	"2:				\n"	\
	"   .section  .fixup,\"ax\"	\n"	\
	"   .align 4			\n"	\
	"4:				\n"	\
	"   .long  2b			\n"	\
	"5:				\n"	\
	"   l32r   %1, 4b		\n"	\
	"   movi   %2, 0		\n"	\
        "   movi   %0, %4		\n"	\
        "   jx     %1			\n"	\
	"   .previous			\n"	\
	"   .section  __ex_table,\"a\"	\n"	\
	"   .long	1b, 5b		\n"	\
	"   .previous"				\
	:"=r" (err), "=r" (cb), "=r" (x)	\
	:"r" (addr), "i" (-EFAULT), "0" (err))




extern unsigned __xtensa_copy_user(void *to, const void *from, unsigned n);
#define __copy_user(to,from,size) __xtensa_copy_user(to,from,size)


static inline unsigned long
__generic_copy_from_user_nocheck(void *to, const void *from, unsigned long n)
{
	return __copy_user(to,from,n);
}

static inline unsigned long
__generic_copy_to_user_nocheck(void *to, const void *from, unsigned long n)
{
	return __copy_user(to,from,n);
}

static inline unsigned long
__generic_copy_to_user(void *to, const void *from, unsigned long n)
{
	prefetch(from);
	if (access_ok(VERIFY_WRITE, to, n))
		return __copy_user(to,from,n);
	return n;
}

static inline unsigned long
__generic_copy_from_user(void *to, const void *from, unsigned long n)
{
	prefetchw(to);
	if (access_ok(VERIFY_READ, from, n))
		return __copy_user(to,from,n);
	else
		memset(to, 0, n);
	return n;
}

#define copy_to_user(to,from,n) __generic_copy_to_user((to),(from),(n))
#define copy_from_user(to,from,n) __generic_copy_from_user((to),(from),(n))
#define __copy_to_user(to,from,n) __generic_copy_to_user_nocheck((to),(from),(n))
#define __copy_from_user(to,from,n) __generic_copy_from_user_nocheck((to),(from),(n))
#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user



static inline unsigned long
__xtensa_clear_user(void *addr, unsigned long size)
{
	if ( ! memset(addr, 0, size) )
		return size;
	return 0;
}

static inline unsigned long
clear_user(void *addr, unsigned long size)
{
	if (access_ok(VERIFY_WRITE, addr, size))
		return __xtensa_clear_user(addr, size);
	return size ? -EFAULT : 0;
}

#define __clear_user  __xtensa_clear_user


extern long __strncpy_user(char *, const char *, long);
#define __strncpy_from_user __strncpy_user

static inline long
strncpy_from_user(char *dst, const char *src, long count)
{
	if (access_ok(VERIFY_READ, src, 1))
		return __strncpy_from_user(dst, src, count);
	return -EFAULT;
}


#define strlen_user(str) strnlen_user((str), TASK_SIZE - 1)

extern long __strnlen_user(const char *, long);

static inline long strnlen_user(const char *str, long len)
{
	unsigned long top = __kernel_ok ? ~0UL : TASK_SIZE - 1;

	if ((unsigned long)str > top)
		return 0;
	return __strnlen_user(str, len);
}


struct exception_table_entry
{
	unsigned long insn, fixup;
};


extern unsigned long search_exception_table(unsigned long addr);
extern void sort_exception_table(void);

#define fixup_exception(map_reg, fixup_unit, pc)                \
({                                                              \
	fixup_unit;                                             \
})

#endif	
#endif	
