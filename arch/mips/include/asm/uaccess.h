/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998, 1999, 2000, 03, 04 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 2007  Maciej W. Rozycki
 */
#ifndef _ASM_UACCESS_H
#define _ASM_UACCESS_H

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/thread_info.h>

#ifdef CONFIG_32BIT

#define __UA_LIMIT	0x80000000UL

#define __UA_ADDR	".word"
#define __UA_LA		"la"
#define __UA_ADDU	"addu"
#define __UA_t0		"$8"
#define __UA_t1		"$9"

#endif 

#ifdef CONFIG_64BIT

extern u64 __ua_limit;

#define __UA_LIMIT	__ua_limit

#define __UA_ADDR	".dword"
#define __UA_LA		"dla"
#define __UA_ADDU	"daddu"
#define __UA_t0		"$12"
#define __UA_t1		"$13"

#endif 


#define KERNEL_DS	((mm_segment_t) { 0UL })
#define USER_DS		((mm_segment_t) { __UA_LIMIT })

#define VERIFY_READ    0
#define VERIFY_WRITE   1

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current_thread_info()->addr_limit)
#define set_fs(x)	(current_thread_info()->addr_limit = (x))

#define segment_eq(a, b)	((a).seg == (b).seg)


#define __ua_size(size)							\
	((__builtin_constant_p(size) && (signed long) (size) > 0) ? 0 : (size))


#define __access_mask get_fs().seg

#define __access_ok(addr, size, mask)					\
({									\
	unsigned long __addr = (unsigned long) (addr);			\
	unsigned long __size = size;					\
	unsigned long __mask = mask;					\
	unsigned long __ok;						\
									\
	__chk_user_ptr(addr);						\
	__ok = (signed long)(__mask & (__addr | (__addr + __size) |	\
		__ua_size(__size)));					\
	__ok == 0;							\
})

#define access_ok(type, addr, size)					\
	likely(__access_ok((addr), (size), __access_mask))

#define put_user(x,ptr)	\
	__put_user_check((x), (ptr), sizeof(*(ptr)))

#define get_user(x,ptr) \
	__get_user_check((x), (ptr), sizeof(*(ptr)))

#define __put_user(x,ptr) \
	__put_user_nocheck((x), (ptr), sizeof(*(ptr)))

#define __get_user(x,ptr) \
	__get_user_nocheck((x), (ptr), sizeof(*(ptr)))

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct __user *)(x))

#ifdef CONFIG_32BIT
#define __GET_USER_DW(val, ptr) __get_user_asm_ll32(val, ptr)
#endif
#ifdef CONFIG_64BIT
#define __GET_USER_DW(val, ptr) __get_user_asm(val, "ld", ptr)
#endif

extern void __get_user_unknown(void);

#define __get_user_common(val, size, ptr)				\
do {									\
	switch (size) {							\
	case 1: __get_user_asm(val, "lb", ptr); break;			\
	case 2: __get_user_asm(val, "lh", ptr); break;			\
	case 4: __get_user_asm(val, "lw", ptr); break;			\
	case 8: __GET_USER_DW(val, ptr); break;				\
	default: __get_user_unknown(); break;				\
	}								\
} while (0)

#define __get_user_nocheck(x, ptr, size)				\
({									\
	int __gu_err;							\
									\
	__chk_user_ptr(ptr);						\
	__get_user_common((x), size, ptr);				\
	__gu_err;							\
})

#define __get_user_check(x, ptr, size)					\
({									\
	int __gu_err = -EFAULT;						\
	const __typeof__(*(ptr)) __user * __gu_ptr = (ptr);		\
									\
	might_fault();							\
	if (likely(access_ok(VERIFY_READ,  __gu_ptr, size)))		\
		__get_user_common((x), size, __gu_ptr);			\
									\
	__gu_err;							\
})

#define __get_user_asm(val, insn, addr)					\
{									\
	long __gu_tmp;							\
									\
	__asm__ __volatile__(						\
	"1:	" insn "	%1, %3				\n"	\
	"2:							\n"	\
	"	.section .fixup,\"ax\"				\n"	\
	"3:	li	%0, %4					\n"	\
	"	j	2b					\n"	\
	"	.previous					\n"	\
	"	.section __ex_table,\"a\"			\n"	\
	"	"__UA_ADDR "\t1b, 3b				\n"	\
	"	.previous					\n"	\
	: "=r" (__gu_err), "=r" (__gu_tmp)				\
	: "0" (0), "o" (__m(addr)), "i" (-EFAULT));			\
									\
	(val) = (__typeof__(*(addr))) __gu_tmp;				\
}

#define __get_user_asm_ll32(val, addr)					\
{									\
	union {								\
		unsigned long long	l;				\
		__typeof__(*(addr))	t;				\
	} __gu_tmp;							\
									\
	__asm__ __volatile__(						\
	"1:	lw	%1, (%3)				\n"	\
	"2:	lw	%D1, 4(%3)				\n"	\
	"3:	.section	.fixup,\"ax\"			\n"	\
	"4:	li	%0, %4					\n"	\
	"	move	%1, $0					\n"	\
	"	move	%D1, $0					\n"	\
	"	j	3b					\n"	\
	"	.previous					\n"	\
	"	.section	__ex_table,\"a\"		\n"	\
	"	" __UA_ADDR "	1b, 4b				\n"	\
	"	" __UA_ADDR "	2b, 4b				\n"	\
	"	.previous					\n"	\
	: "=r" (__gu_err), "=&r" (__gu_tmp.l)				\
	: "0" (0), "r" (addr), "i" (-EFAULT));				\
									\
	(val) = __gu_tmp.t;						\
}

#ifdef CONFIG_32BIT
#define __PUT_USER_DW(ptr) __put_user_asm_ll32(ptr)
#endif
#ifdef CONFIG_64BIT
#define __PUT_USER_DW(ptr) __put_user_asm("sd", ptr)
#endif

#define __put_user_nocheck(x, ptr, size)				\
({									\
	__typeof__(*(ptr)) __pu_val;					\
	int __pu_err = 0;						\
									\
	__chk_user_ptr(ptr);						\
	__pu_val = (x);							\
	switch (size) {							\
	case 1: __put_user_asm("sb", ptr); break;			\
	case 2: __put_user_asm("sh", ptr); break;			\
	case 4: __put_user_asm("sw", ptr); break;			\
	case 8: __PUT_USER_DW(ptr); break;				\
	default: __put_user_unknown(); break;				\
	}								\
	__pu_err;							\
})

#define __put_user_check(x, ptr, size)					\
({									\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);			\
	__typeof__(*(ptr)) __pu_val = (x);				\
	int __pu_err = -EFAULT;						\
									\
	might_fault();							\
	if (likely(access_ok(VERIFY_WRITE,  __pu_addr, size))) {	\
		switch (size) {						\
		case 1: __put_user_asm("sb", __pu_addr); break;		\
		case 2: __put_user_asm("sh", __pu_addr); break;		\
		case 4: __put_user_asm("sw", __pu_addr); break;		\
		case 8: __PUT_USER_DW(__pu_addr); break;		\
		default: __put_user_unknown(); break;			\
		}							\
	}								\
	__pu_err;							\
})

#define __put_user_asm(insn, ptr)					\
{									\
	__asm__ __volatile__(						\
	"1:	" insn "	%z2, %3		# __put_user_asm\n"	\
	"2:							\n"	\
	"	.section	.fixup,\"ax\"			\n"	\
	"3:	li	%0, %4					\n"	\
	"	j	2b					\n"	\
	"	.previous					\n"	\
	"	.section	__ex_table,\"a\"		\n"	\
	"	" __UA_ADDR "	1b, 3b				\n"	\
	"	.previous					\n"	\
	: "=r" (__pu_err)						\
	: "0" (0), "Jr" (__pu_val), "o" (__m(ptr)),			\
	  "i" (-EFAULT));						\
}

#define __put_user_asm_ll32(ptr)					\
{									\
	__asm__ __volatile__(						\
	"1:	sw	%2, (%3)	# __put_user_asm_ll32	\n"	\
	"2:	sw	%D2, 4(%3)				\n"	\
	"3:							\n"	\
	"	.section	.fixup,\"ax\"			\n"	\
	"4:	li	%0, %4					\n"	\
	"	j	3b					\n"	\
	"	.previous					\n"	\
	"	.section	__ex_table,\"a\"		\n"	\
	"	" __UA_ADDR "	1b, 4b				\n"	\
	"	" __UA_ADDR "	2b, 4b				\n"	\
	"	.previous"						\
	: "=r" (__pu_err)						\
	: "0" (0), "r" (__pu_val), "r" (ptr),				\
	  "i" (-EFAULT));						\
}

extern void __put_user_unknown(void);

#define put_user_unaligned(x,ptr)	\
	__put_user_unaligned_check((x),(ptr),sizeof(*(ptr)))

#define get_user_unaligned(x,ptr) \
	__get_user_unaligned_check((x),(ptr),sizeof(*(ptr)))

#define __put_user_unaligned(x,ptr) \
	__put_user_unaligned_nocheck((x),(ptr),sizeof(*(ptr)))

#define __get_user_unaligned(x,ptr) \
	__get_user__unalignednocheck((x),(ptr),sizeof(*(ptr)))

#ifdef CONFIG_32BIT
#define __GET_USER_UNALIGNED_DW(val, ptr)				\
	__get_user_unaligned_asm_ll32(val, ptr)
#endif
#ifdef CONFIG_64BIT
#define __GET_USER_UNALIGNED_DW(val, ptr)				\
	__get_user_unaligned_asm(val, "uld", ptr)
#endif

extern void __get_user_unaligned_unknown(void);

#define __get_user_unaligned_common(val, size, ptr)			\
do {									\
	switch (size) {							\
	case 1: __get_user_asm(val, "lb", ptr); break;			\
	case 2: __get_user_unaligned_asm(val, "ulh", ptr); break;	\
	case 4: __get_user_unaligned_asm(val, "ulw", ptr); break;	\
	case 8: __GET_USER_UNALIGNED_DW(val, ptr); break;		\
	default: __get_user_unaligned_unknown(); break;			\
	}								\
} while (0)

#define __get_user_unaligned_nocheck(x,ptr,size)			\
({									\
	int __gu_err;							\
									\
	__get_user_unaligned_common((x), size, ptr);			\
	__gu_err;							\
})

#define __get_user_unaligned_check(x,ptr,size)				\
({									\
	int __gu_err = -EFAULT;						\
	const __typeof__(*(ptr)) __user * __gu_ptr = (ptr);		\
									\
	if (likely(access_ok(VERIFY_READ,  __gu_ptr, size)))		\
		__get_user_unaligned_common((x), size, __gu_ptr);	\
									\
	__gu_err;							\
})

#define __get_user_unaligned_asm(val, insn, addr)			\
{									\
	long __gu_tmp;							\
									\
	__asm__ __volatile__(						\
	"1:	" insn "	%1, %3				\n"	\
	"2:							\n"	\
	"	.section .fixup,\"ax\"				\n"	\
	"3:	li	%0, %4					\n"	\
	"	j	2b					\n"	\
	"	.previous					\n"	\
	"	.section __ex_table,\"a\"			\n"	\
	"	"__UA_ADDR "\t1b, 3b				\n"	\
	"	"__UA_ADDR "\t1b + 4, 3b			\n"	\
	"	.previous					\n"	\
	: "=r" (__gu_err), "=r" (__gu_tmp)				\
	: "0" (0), "o" (__m(addr)), "i" (-EFAULT));			\
									\
	(val) = (__typeof__(*(addr))) __gu_tmp;				\
}

#define __get_user_unaligned_asm_ll32(val, addr)			\
{									\
        unsigned long long __gu_tmp;					\
									\
	__asm__ __volatile__(						\
	"1:	ulw	%1, (%3)				\n"	\
	"2:	ulw	%D1, 4(%3)				\n"	\
	"	move	%0, $0					\n"	\
	"3:	.section	.fixup,\"ax\"			\n"	\
	"4:	li	%0, %4					\n"	\
	"	move	%1, $0					\n"	\
	"	move	%D1, $0					\n"	\
	"	j	3b					\n"	\
	"	.previous					\n"	\
	"	.section	__ex_table,\"a\"		\n"	\
	"	" __UA_ADDR "	1b, 4b				\n"	\
	"	" __UA_ADDR "	1b + 4, 4b			\n"	\
	"	" __UA_ADDR "	2b, 4b				\n"	\
	"	" __UA_ADDR "	2b + 4, 4b			\n"	\
	"	.previous					\n"	\
	: "=r" (__gu_err), "=&r" (__gu_tmp)				\
	: "0" (0), "r" (addr), "i" (-EFAULT));				\
	(val) = (__typeof__(*(addr))) __gu_tmp;				\
}

#ifdef CONFIG_32BIT
#define __PUT_USER_UNALIGNED_DW(ptr) __put_user_unaligned_asm_ll32(ptr)
#endif
#ifdef CONFIG_64BIT
#define __PUT_USER_UNALIGNED_DW(ptr) __put_user_unaligned_asm("usd", ptr)
#endif

#define __put_user_unaligned_nocheck(x,ptr,size)			\
({									\
	__typeof__(*(ptr)) __pu_val;					\
	int __pu_err = 0;						\
									\
	__pu_val = (x);							\
	switch (size) {							\
	case 1: __put_user_asm("sb", ptr); break;			\
	case 2: __put_user_unaligned_asm("ush", ptr); break;		\
	case 4: __put_user_unaligned_asm("usw", ptr); break;		\
	case 8: __PUT_USER_UNALIGNED_DW(ptr); break;			\
	default: __put_user_unaligned_unknown(); break;			\
	}								\
	__pu_err;							\
})

#define __put_user_unaligned_check(x,ptr,size)				\
({									\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);			\
	__typeof__(*(ptr)) __pu_val = (x);				\
	int __pu_err = -EFAULT;						\
									\
	if (likely(access_ok(VERIFY_WRITE,  __pu_addr, size))) {	\
		switch (size) {						\
		case 1: __put_user_asm("sb", __pu_addr); break;		\
		case 2: __put_user_unaligned_asm("ush", __pu_addr); break; \
		case 4: __put_user_unaligned_asm("usw", __pu_addr); break; \
		case 8: __PUT_USER_UNALGINED_DW(__pu_addr); break;	\
		default: __put_user_unaligned_unknown(); break;		\
		}							\
	}								\
	__pu_err;							\
})

#define __put_user_unaligned_asm(insn, ptr)				\
{									\
	__asm__ __volatile__(						\
	"1:	" insn "	%z2, %3		# __put_user_unaligned_asm\n" \
	"2:							\n"	\
	"	.section	.fixup,\"ax\"			\n"	\
	"3:	li	%0, %4					\n"	\
	"	j	2b					\n"	\
	"	.previous					\n"	\
	"	.section	__ex_table,\"a\"		\n"	\
	"	" __UA_ADDR "	1b, 3b				\n"	\
	"	.previous					\n"	\
	: "=r" (__pu_err)						\
	: "0" (0), "Jr" (__pu_val), "o" (__m(ptr)),			\
	  "i" (-EFAULT));						\
}

#define __put_user_unaligned_asm_ll32(ptr)				\
{									\
	__asm__ __volatile__(						\
	"1:	sw	%2, (%3)	# __put_user_unaligned_asm_ll32	\n" \
	"2:	sw	%D2, 4(%3)				\n"	\
	"3:							\n"	\
	"	.section	.fixup,\"ax\"			\n"	\
	"4:	li	%0, %4					\n"	\
	"	j	3b					\n"	\
	"	.previous					\n"	\
	"	.section	__ex_table,\"a\"		\n"	\
	"	" __UA_ADDR "	1b, 4b				\n"	\
	"	" __UA_ADDR "	1b + 4, 4b			\n"	\
	"	" __UA_ADDR "	2b, 4b				\n"	\
	"	" __UA_ADDR "	2b + 4, 4b			\n"	\
	"	.previous"						\
	: "=r" (__pu_err)						\
	: "0" (0), "r" (__pu_val), "r" (ptr),				\
	  "i" (-EFAULT));						\
}

extern void __put_user_unaligned_unknown(void);

#ifdef MODULE
#define __MODULE_JAL(destination)					\
	".set\tnoat\n\t"						\
	__UA_LA "\t$1, " #destination "\n\t" 				\
	"jalr\t$1\n\t"							\
	".set\tat\n\t"
#else
#define __MODULE_JAL(destination)					\
	"jal\t" #destination "\n\t"
#endif

#ifndef CONFIG_CPU_DADDI_WORKAROUNDS
#define DADDI_SCRATCH "$0"
#else
#define DADDI_SCRATCH "$3"
#endif

extern size_t __copy_user(void *__to, const void *__from, size_t __n);

#define __invoke_copy_to_user(to, from, n)				\
({									\
	register void __user *__cu_to_r __asm__("$4");			\
	register const void *__cu_from_r __asm__("$5");			\
	register long __cu_len_r __asm__("$6");				\
									\
	__cu_to_r = (to);						\
	__cu_from_r = (from);						\
	__cu_len_r = (n);						\
	__asm__ __volatile__(						\
	__MODULE_JAL(__copy_user)					\
	: "+r" (__cu_to_r), "+r" (__cu_from_r), "+r" (__cu_len_r)	\
	:								\
	: "$8", "$9", "$10", "$11", "$12", "$15", "$24", "$31",		\
	  DADDI_SCRATCH, "memory");					\
	__cu_len_r;							\
})

#define __copy_to_user(to, from, n)					\
({									\
	void __user *__cu_to;						\
	const void *__cu_from;						\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	might_fault();							\
	__cu_len = __invoke_copy_to_user(__cu_to, __cu_from, __cu_len);	\
	__cu_len;							\
})

extern size_t __copy_user_inatomic(void *__to, const void *__from, size_t __n);

#define __copy_to_user_inatomic(to, from, n)				\
({									\
	void __user *__cu_to;						\
	const void *__cu_from;						\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	__cu_len = __invoke_copy_to_user(__cu_to, __cu_from, __cu_len);	\
	__cu_len;							\
})

#define __copy_from_user_inatomic(to, from, n)				\
({									\
	void *__cu_to;							\
	const void __user *__cu_from;					\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	__cu_len = __invoke_copy_from_user_inatomic(__cu_to, __cu_from,	\
	                                            __cu_len);		\
	__cu_len;							\
})

#define copy_to_user(to, from, n)					\
({									\
	void __user *__cu_to;						\
	const void *__cu_from;						\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	if (access_ok(VERIFY_WRITE, __cu_to, __cu_len)) {		\
		might_fault();						\
		__cu_len = __invoke_copy_to_user(__cu_to, __cu_from,	\
		                                 __cu_len);		\
	}								\
	__cu_len;							\
})

#define __invoke_copy_from_user(to, from, n)				\
({									\
	register void *__cu_to_r __asm__("$4");				\
	register const void __user *__cu_from_r __asm__("$5");		\
	register long __cu_len_r __asm__("$6");				\
									\
	__cu_to_r = (to);						\
	__cu_from_r = (from);						\
	__cu_len_r = (n);						\
	__asm__ __volatile__(						\
	".set\tnoreorder\n\t"						\
	__MODULE_JAL(__copy_user)					\
	".set\tnoat\n\t"						\
	__UA_ADDU "\t$1, %1, %2\n\t"					\
	".set\tat\n\t"							\
	".set\treorder"							\
	: "+r" (__cu_to_r), "+r" (__cu_from_r), "+r" (__cu_len_r)	\
	:								\
	: "$8", "$9", "$10", "$11", "$12", "$15", "$24", "$31",		\
	  DADDI_SCRATCH, "memory");					\
	__cu_len_r;							\
})

#define __invoke_copy_from_user_inatomic(to, from, n)			\
({									\
	register void *__cu_to_r __asm__("$4");				\
	register const void __user *__cu_from_r __asm__("$5");		\
	register long __cu_len_r __asm__("$6");				\
									\
	__cu_to_r = (to);						\
	__cu_from_r = (from);						\
	__cu_len_r = (n);						\
	__asm__ __volatile__(						\
	".set\tnoreorder\n\t"						\
	__MODULE_JAL(__copy_user_inatomic)				\
	".set\tnoat\n\t"						\
	__UA_ADDU "\t$1, %1, %2\n\t"					\
	".set\tat\n\t"							\
	".set\treorder"							\
	: "+r" (__cu_to_r), "+r" (__cu_from_r), "+r" (__cu_len_r)	\
	:								\
	: "$8", "$9", "$10", "$11", "$12", "$15", "$24", "$31",		\
	  DADDI_SCRATCH, "memory");					\
	__cu_len_r;							\
})

#define __copy_from_user(to, from, n)					\
({									\
	void *__cu_to;							\
	const void __user *__cu_from;					\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	might_fault();							\
	__cu_len = __invoke_copy_from_user(__cu_to, __cu_from,		\
	                                   __cu_len);			\
	__cu_len;							\
})

#define copy_from_user(to, from, n)					\
({									\
	void *__cu_to;							\
	const void __user *__cu_from;					\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	if (access_ok(VERIFY_READ, __cu_from, __cu_len)) {		\
		might_fault();						\
		__cu_len = __invoke_copy_from_user(__cu_to, __cu_from,	\
		                                   __cu_len);		\
	}								\
	__cu_len;							\
})

#define __copy_in_user(to, from, n)					\
({									\
	void __user *__cu_to;						\
	const void __user *__cu_from;					\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	might_fault();							\
	__cu_len = __invoke_copy_from_user(__cu_to, __cu_from,		\
	                                   __cu_len);			\
	__cu_len;							\
})

#define copy_in_user(to, from, n)					\
({									\
	void __user *__cu_to;						\
	const void __user *__cu_from;					\
	long __cu_len;							\
									\
	__cu_to = (to);							\
	__cu_from = (from);						\
	__cu_len = (n);							\
	if (likely(access_ok(VERIFY_READ, __cu_from, __cu_len) &&	\
	           access_ok(VERIFY_WRITE, __cu_to, __cu_len))) {	\
		might_fault();						\
		__cu_len = __invoke_copy_from_user(__cu_to, __cu_from,	\
		                                   __cu_len);		\
	}								\
	__cu_len;							\
})

static inline __kernel_size_t
__clear_user(void __user *addr, __kernel_size_t size)
{
	__kernel_size_t res;

	might_fault();
	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		"move\t$5, $0\n\t"
		"move\t$6, %2\n\t"
		__MODULE_JAL(__bzero)
		"move\t%0, $6"
		: "=r" (res)
		: "r" (addr), "r" (size)
		: "$4", "$5", "$6", __UA_t0, __UA_t1, "$31");

	return res;
}

#define clear_user(addr,n)						\
({									\
	void __user * __cl_addr = (addr);				\
	unsigned long __cl_size = (n);					\
	if (__cl_size && access_ok(VERIFY_WRITE,			\
					__cl_addr, __cl_size))		\
		__cl_size = __clear_user(__cl_addr, __cl_size);		\
	__cl_size;							\
})

static inline long
__strncpy_from_user(char *__to, const char __user *__from, long __len)
{
	long res;

	might_fault();
	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		"move\t$5, %2\n\t"
		"move\t$6, %3\n\t"
		__MODULE_JAL(__strncpy_from_user_nocheck_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (__to), "r" (__from), "r" (__len)
		: "$2", "$3", "$4", "$5", "$6", __UA_t0, "$31", "memory");

	return res;
}

static inline long
strncpy_from_user(char *__to, const char __user *__from, long __len)
{
	long res;

	might_fault();
	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		"move\t$5, %2\n\t"
		"move\t$6, %3\n\t"
		__MODULE_JAL(__strncpy_from_user_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (__to), "r" (__from), "r" (__len)
		: "$2", "$3", "$4", "$5", "$6", __UA_t0, "$31", "memory");

	return res;
}

static inline long __strlen_user(const char __user *s)
{
	long res;

	might_fault();
	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		__MODULE_JAL(__strlen_user_nocheck_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (s)
		: "$2", "$4", __UA_t0, "$31");

	return res;
}

static inline long strlen_user(const char __user *s)
{
	long res;

	might_fault();
	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		__MODULE_JAL(__strlen_user_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (s)
		: "$2", "$4", __UA_t0, "$31");

	return res;
}

static inline long __strnlen_user(const char __user *s, long n)
{
	long res;

	might_fault();
	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		"move\t$5, %2\n\t"
		__MODULE_JAL(__strnlen_user_nocheck_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (s), "r" (n)
		: "$2", "$4", "$5", __UA_t0, "$31");

	return res;
}

static inline long strnlen_user(const char __user *s, long n)
{
	long res;

	might_fault();
	__asm__ __volatile__(
		"move\t$4, %1\n\t"
		"move\t$5, %2\n\t"
		__MODULE_JAL(__strnlen_user_asm)
		"move\t%0, $2"
		: "=r" (res)
		: "r" (s), "r" (n)
		: "$2", "$4", "$5", __UA_t0, "$31");

	return res;
}

struct exception_table_entry
{
	unsigned long insn;
	unsigned long nextinsn;
};

extern int fixup_exception(struct pt_regs *regs);

#endif 
