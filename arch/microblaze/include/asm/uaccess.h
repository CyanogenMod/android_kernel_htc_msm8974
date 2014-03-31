/*
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2008-2009 PetaLogix
 * Copyright (C) 2006 Atmark Techno, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifndef _ASM_MICROBLAZE_UACCESS_H
#define _ASM_MICROBLAZE_UACCESS_H

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h> 
#include <linux/mm.h>

#include <asm/mmu.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <linux/string.h>

#define VERIFY_READ	0
#define VERIFY_WRITE	1

# define MAKE_MM_SEG(s)       ((mm_segment_t) { (s) })

#  ifndef CONFIG_MMU
#  define KERNEL_DS	MAKE_MM_SEG(0)
#  define USER_DS	KERNEL_DS
#  else
#  define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFF)
#  define USER_DS	MAKE_MM_SEG(TASK_SIZE - 1)
#  endif

# define get_ds()	(KERNEL_DS)
# define get_fs()	(current_thread_info()->addr_limit)
# define set_fs(val)	(current_thread_info()->addr_limit = (val))

# define segment_eq(a, b)	((a).seg == (b).seg)

struct exception_table_entry {
	unsigned long insn, fixup;
};

extern unsigned long search_exception_table(unsigned long);

#ifndef CONFIG_MMU

static inline int ___range_ok(unsigned long addr, unsigned long size)
{
	return ((addr < memory_start) ||
		((addr + size - 1) > (memory_start + memory_size - 1)));
}

#define __range_ok(addr, size) \
		___range_ok((unsigned long)(addr), (unsigned long)(size))

#define access_ok(type, addr, size) (__range_ok((addr), (size)) == 0)

#else

#define access_ok(type, addr, size) \
	(get_fs().seg >= (((unsigned long)(addr)) | \
		(size) | ((unsigned long)(addr) + (size))))


#endif

#ifdef CONFIG_MMU
# define __FIXUP_SECTION	".section .fixup,\"ax\"\n"
# define __EX_TABLE_SECTION	".section __ex_table,\"a\"\n"
#else
# define __FIXUP_SECTION	".section .discard,\"ax\"\n"
# define __EX_TABLE_SECTION	".section .discard,\"a\"\n"
#endif

extern unsigned long __copy_tofrom_user(void __user *to,
		const void __user *from, unsigned long size);

static inline unsigned long __must_check __clear_user(void __user *to,
							unsigned long n)
{
	
	__asm__ __volatile__ (				\
			"1:	sb	r0, %1, r0;"	\
			"	addik	%0, %0, -1;"	\
			"	bneid	%0, 1b;"	\
			"	addik	%1, %1, 1;"	\
			"2:			"	\
			__EX_TABLE_SECTION		\
			".word	1b,2b;"			\
			".previous;"			\
		: "=r"(n), "=r"(to)			\
		: "0"(n), "1"(to)
	);
	return n;
}

static inline unsigned long __must_check clear_user(void __user *to,
							unsigned long n)
{
	might_sleep();
	if (unlikely(!access_ok(VERIFY_WRITE, to, n)))
		return n;

	return __clear_user(to, n);
}

extern long __user_bad(void);

#define __get_user_asm(insn, __gu_ptr, __gu_val, __gu_err)	\
({								\
	__asm__ __volatile__ (					\
			"1:"	insn	" %1, %2, r0;"		\
			"	addk	%0, r0, r0;"		\
			"2:			"		\
			__FIXUP_SECTION				\
			"3:	brid	2b;"			\
			"	addik	%0, r0, %3;"		\
			".previous;"				\
			__EX_TABLE_SECTION			\
			".word	1b,3b;"				\
			".previous;"				\
		: "=&r"(__gu_err), "=r"(__gu_val)		\
		: "r"(__gu_ptr), "i"(-EFAULT)			\
	);							\
})

#define get_user(x, ptr)						\
	__get_user_check((x), (ptr), sizeof(*(ptr)))

#define __get_user_check(x, ptr, size)					\
({									\
	unsigned long __gu_val = 0;					\
	const typeof(*(ptr)) __user *__gu_addr = (ptr);			\
	int __gu_err = 0;						\
									\
	if (access_ok(VERIFY_READ, __gu_addr, size)) {			\
		switch (size) {						\
		case 1:							\
			__get_user_asm("lbu", __gu_addr, __gu_val,	\
				       __gu_err);			\
			break;						\
		case 2:							\
			__get_user_asm("lhu", __gu_addr, __gu_val,	\
				       __gu_err);			\
			break;						\
		case 4:							\
			__get_user_asm("lw", __gu_addr, __gu_val,	\
				       __gu_err);			\
			break;						\
		default:						\
			__gu_err = __user_bad();			\
			break;						\
		}							\
	} else {							\
		__gu_err = -EFAULT;					\
	}								\
	x = (typeof(*(ptr)))__gu_val;					\
	__gu_err;							\
})

#define __get_user(x, ptr)						\
({									\
	unsigned long __gu_val;						\
			\
	long __gu_err;							\
	switch (sizeof(*(ptr))) {					\
	case 1:								\
		__get_user_asm("lbu", (ptr), __gu_val, __gu_err);	\
		break;							\
	case 2:								\
		__get_user_asm("lhu", (ptr), __gu_val, __gu_err);	\
		break;							\
	case 4:								\
		__get_user_asm("lw", (ptr), __gu_val, __gu_err);	\
		break;							\
	default:							\
		 __gu_err = __user_bad();\
	}								\
	x = (__typeof__(*(ptr))) __gu_val;				\
	__gu_err;							\
})


#define __put_user_asm(insn, __gu_ptr, __gu_val, __gu_err)	\
({								\
	__asm__ __volatile__ (					\
			"1:"	insn	" %1, %2, r0;"		\
			"	addk	%0, r0, r0;"		\
			"2:			"		\
			__FIXUP_SECTION				\
			"3:	brid	2b;"			\
			"	addik	%0, r0, %3;"		\
			".previous;"				\
			__EX_TABLE_SECTION			\
			".word	1b,3b;"				\
			".previous;"				\
		: "=&r"(__gu_err)				\
		: "r"(__gu_val), "r"(__gu_ptr), "i"(-EFAULT)	\
	);							\
})

#define __put_user_asm_8(__gu_ptr, __gu_val, __gu_err)		\
({								\
	__asm__ __volatile__ ("	lwi	%0, %1, 0;"		\
			"1:	swi	%0, %2, 0;"		\
			"	lwi	%0, %1, 4;"		\
			"2:	swi	%0, %2, 4;"		\
			"	addk	%0, r0, r0;"		\
			"3:			"		\
			__FIXUP_SECTION				\
			"4:	brid	3b;"			\
			"	addik	%0, r0, %3;"		\
			".previous;"				\
			__EX_TABLE_SECTION			\
			".word	1b,4b,2b,4b;"			\
			".previous;"				\
		: "=&r"(__gu_err)				\
		: "r"(&__gu_val), "r"(__gu_ptr), "i"(-EFAULT)	\
		);						\
})

#define put_user(x, ptr)						\
	__put_user_check((x), (ptr), sizeof(*(ptr)))

#define __put_user_check(x, ptr, size)					\
({									\
	typeof(*(ptr)) __pu_val;					\
	typeof(*(ptr)) __user *__pu_addr = (ptr);			\
	int __pu_err = 0;						\
									\
	__pu_val = (x);							\
	if (access_ok(VERIFY_WRITE, __pu_addr, size)) {			\
		switch (size) {						\
		case 1:							\
			__put_user_asm("sb", __pu_addr, __pu_val,	\
				       __pu_err);			\
			break;						\
		case 2:							\
			__put_user_asm("sh", __pu_addr, __pu_val,	\
				       __pu_err);			\
			break;						\
		case 4:							\
			__put_user_asm("sw", __pu_addr, __pu_val,	\
				       __pu_err);			\
			break;						\
		case 8:							\
			__put_user_asm_8(__pu_addr, __pu_val, __pu_err);\
			break;						\
		default:						\
			__pu_err = __user_bad();			\
			break;						\
		}							\
	} else {							\
		__pu_err = -EFAULT;					\
	}								\
	__pu_err;							\
})

#define __put_user(x, ptr)						\
({									\
	__typeof__(*(ptr)) volatile __gu_val = (x);			\
	long __gu_err = 0;						\
	switch (sizeof(__gu_val)) {					\
	case 1:								\
		__put_user_asm("sb", (ptr), __gu_val, __gu_err);	\
		break;							\
	case 2:								\
		__put_user_asm("sh", (ptr), __gu_val, __gu_err);	\
		break;							\
	case 4:								\
		__put_user_asm("sw", (ptr), __gu_val, __gu_err);	\
		break;							\
	case 8:								\
		__put_user_asm_8((ptr), __gu_val, __gu_err);		\
		break;							\
	default:							\
			__gu_err = __user_bad();	\
	}								\
	__gu_err;							\
})


#define __copy_from_user(to, from, n)	\
	__copy_tofrom_user((__force void __user *)(to), \
				(void __user *)(from), (n))
#define __copy_from_user_inatomic(to, from, n) \
		__copy_from_user((to), (from), (n))

static inline long copy_from_user(void *to,
		const void __user *from, unsigned long n)
{
	might_sleep();
	if (access_ok(VERIFY_READ, from, n))
		return __copy_from_user(to, from, n);
	return n;
}

#define __copy_to_user(to, from, n)	\
		__copy_tofrom_user((void __user *)(to), \
			(__force const void __user *)(from), (n))
#define __copy_to_user_inatomic(to, from, n) __copy_to_user((to), (from), (n))

static inline long copy_to_user(void __user *to,
		const void *from, unsigned long n)
{
	might_sleep();
	if (access_ok(VERIFY_WRITE, to, n))
		return __copy_to_user(to, from, n);
	return n;
}

extern int __strncpy_user(char *to, const char __user *from, int len);

#define __strncpy_from_user	__strncpy_user

static inline long
strncpy_from_user(char *dst, const char __user *src, long count)
{
	if (!access_ok(VERIFY_READ, src, 1))
		return -EFAULT;
	return __strncpy_from_user(dst, src, count);
}

extern int __strnlen_user(const char __user *sstr, int len);

static inline long strnlen_user(const char __user *src, long n)
{
	if (!access_ok(VERIFY_READ, src, 1))
		return 0;
	return __strnlen_user(src, n);
}

#endif  
#endif 

#endif 
