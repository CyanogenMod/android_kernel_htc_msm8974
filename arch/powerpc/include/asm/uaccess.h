#ifndef _ARCH_POWERPC_UACCESS_H
#define _ARCH_POWERPC_UACCESS_H

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/asm-compat.h>
#include <asm/processor.h>
#include <asm/page.h>

#define VERIFY_READ	0
#define VERIFY_WRITE	1


#define MAKE_MM_SEG(s)  ((mm_segment_t) { (s) })

#define KERNEL_DS	MAKE_MM_SEG(~0UL)
#ifdef __powerpc64__
#define USER_DS		MAKE_MM_SEG(TASK_SIZE_USER64 - 1)
#else
#define USER_DS		MAKE_MM_SEG(TASK_SIZE - 1)
#endif

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current->thread.fs)
#define set_fs(val)	(current->thread.fs = (val))

#define segment_eq(a, b)	((a).seg == (b).seg)

#ifdef __powerpc64__
#define __access_ok(addr, size, segment)	\
	(((addr) <= (segment).seg) && ((size) <= (segment).seg))

#else

#define __access_ok(addr, size, segment)	\
	(((addr) <= (segment).seg) &&		\
	 (((size) == 0) || (((size) - 1) <= ((segment).seg - (addr)))))

#endif

#define access_ok(type, addr, size)		\
	(__chk_user_ptr(addr),			\
	 __access_ok((__force unsigned long)(addr), (size), get_fs()))


struct exception_table_entry {
	unsigned long insn;
	unsigned long fixup;
};

#define get_user(x, ptr) \
	__get_user_check((x), (ptr), sizeof(*(ptr)))
#define put_user(x, ptr) \
	__put_user_check((__typeof__(*(ptr)))(x), (ptr), sizeof(*(ptr)))

#define __get_user(x, ptr) \
	__get_user_nocheck((x), (ptr), sizeof(*(ptr)))
#define __put_user(x, ptr) \
	__put_user_nocheck((__typeof__(*(ptr)))(x), (ptr), sizeof(*(ptr)))

#ifndef __powerpc64__
#define __get_user64(x, ptr) \
	__get_user64_nocheck((x), (ptr), sizeof(*(ptr)))
#define __put_user64(x, ptr) __put_user(x, ptr)
#endif

#define __get_user_inatomic(x, ptr) \
	__get_user_nosleep((x), (ptr), sizeof(*(ptr)))
#define __put_user_inatomic(x, ptr) \
	__put_user_nosleep((__typeof__(*(ptr)))(x), (ptr), sizeof(*(ptr)))

#define __get_user_unaligned __get_user
#define __put_user_unaligned __put_user

extern long __put_user_bad(void);

#define __put_user_asm(x, addr, err, op)			\
	__asm__ __volatile__(					\
		"1:	" op " %1,0(%2)	# put_user\n"		\
		"2:\n"						\
		".section .fixup,\"ax\"\n"			\
		"3:	li %0,%3\n"				\
		"	b 2b\n"					\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
			PPC_LONG_ALIGN "\n"			\
			PPC_LONG "1b,3b\n"			\
		".previous"					\
		: "=r" (err)					\
		: "r" (x), "b" (addr), "i" (-EFAULT), "0" (err))

#ifdef __powerpc64__
#define __put_user_asm2(x, ptr, retval)				\
	  __put_user_asm(x, ptr, retval, "std")
#else 
#define __put_user_asm2(x, addr, err)				\
	__asm__ __volatile__(					\
		"1:	stw %1,0(%2)\n"				\
		"2:	stw %1+1,4(%2)\n"			\
		"3:\n"						\
		".section .fixup,\"ax\"\n"			\
		"4:	li %0,%3\n"				\
		"	b 3b\n"					\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
			PPC_LONG_ALIGN "\n"			\
			PPC_LONG "1b,4b\n"			\
			PPC_LONG "2b,4b\n"			\
		".previous"					\
		: "=r" (err)					\
		: "r" (x), "b" (addr), "i" (-EFAULT), "0" (err))
#endif 

#define __put_user_size(x, ptr, size, retval)			\
do {								\
	retval = 0;						\
	switch (size) {						\
	  case 1: __put_user_asm(x, ptr, retval, "stb"); break;	\
	  case 2: __put_user_asm(x, ptr, retval, "sth"); break;	\
	  case 4: __put_user_asm(x, ptr, retval, "stw"); break;	\
	  case 8: __put_user_asm2(x, ptr, retval); break;	\
	  default: __put_user_bad();				\
	}							\
} while (0)

#define __put_user_nocheck(x, ptr, size)			\
({								\
	long __pu_err;						\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);		\
	if (!is_kernel_addr((unsigned long)__pu_addr))		\
		might_sleep();					\
	__chk_user_ptr(ptr);					\
	__put_user_size((x), __pu_addr, (size), __pu_err);	\
	__pu_err;						\
})

#define __put_user_check(x, ptr, size)					\
({									\
	long __pu_err = -EFAULT;					\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);			\
	might_sleep();							\
	if (access_ok(VERIFY_WRITE, __pu_addr, size))			\
		__put_user_size((x), __pu_addr, (size), __pu_err);	\
	__pu_err;							\
})

#define __put_user_nosleep(x, ptr, size)			\
({								\
	long __pu_err;						\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);		\
	__chk_user_ptr(ptr);					\
	__put_user_size((x), __pu_addr, (size), __pu_err);	\
	__pu_err;						\
})


extern long __get_user_bad(void);

#define __get_user_asm(x, addr, err, op)		\
	__asm__ __volatile__(				\
		"1:	"op" %1,0(%2)	# get_user\n"	\
		"2:\n"					\
		".section .fixup,\"ax\"\n"		\
		"3:	li %0,%3\n"			\
		"	li %1,0\n"			\
		"	b 2b\n"				\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
			PPC_LONG_ALIGN "\n"		\
			PPC_LONG "1b,3b\n"		\
		".previous"				\
		: "=r" (err), "=r" (x)			\
		: "b" (addr), "i" (-EFAULT), "0" (err))

#ifdef __powerpc64__
#define __get_user_asm2(x, addr, err)			\
	__get_user_asm(x, addr, err, "ld")
#else 
#define __get_user_asm2(x, addr, err)			\
	__asm__ __volatile__(				\
		"1:	lwz %1,0(%2)\n"			\
		"2:	lwz %1+1,4(%2)\n"		\
		"3:\n"					\
		".section .fixup,\"ax\"\n"		\
		"4:	li %0,%3\n"			\
		"	li %1,0\n"			\
		"	li %1+1,0\n"			\
		"	b 3b\n"				\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
			PPC_LONG_ALIGN "\n"		\
			PPC_LONG "1b,4b\n"		\
			PPC_LONG "2b,4b\n"		\
		".previous"				\
		: "=r" (err), "=&r" (x)			\
		: "b" (addr), "i" (-EFAULT), "0" (err))
#endif 

#define __get_user_size(x, ptr, size, retval)			\
do {								\
	retval = 0;						\
	__chk_user_ptr(ptr);					\
	if (size > sizeof(x))					\
		(x) = __get_user_bad();				\
	switch (size) {						\
	case 1: __get_user_asm(x, ptr, retval, "lbz"); break;	\
	case 2: __get_user_asm(x, ptr, retval, "lhz"); break;	\
	case 4: __get_user_asm(x, ptr, retval, "lwz"); break;	\
	case 8: __get_user_asm2(x, ptr, retval);  break;	\
	default: (x) = __get_user_bad();			\
	}							\
} while (0)

#define __get_user_nocheck(x, ptr, size)			\
({								\
	long __gu_err;						\
	unsigned long __gu_val;					\
	const __typeof__(*(ptr)) __user *__gu_addr = (ptr);	\
	__chk_user_ptr(ptr);					\
	if (!is_kernel_addr((unsigned long)__gu_addr))		\
		might_sleep();					\
	__get_user_size(__gu_val, __gu_addr, (size), __gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})

#ifndef __powerpc64__
#define __get_user64_nocheck(x, ptr, size)			\
({								\
	long __gu_err;						\
	long long __gu_val;					\
	const __typeof__(*(ptr)) __user *__gu_addr = (ptr);	\
	__chk_user_ptr(ptr);					\
	if (!is_kernel_addr((unsigned long)__gu_addr))		\
		might_sleep();					\
	__get_user_size(__gu_val, __gu_addr, (size), __gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})
#endif 

#define __get_user_check(x, ptr, size)					\
({									\
	long __gu_err = -EFAULT;					\
	unsigned long  __gu_val = 0;					\
	const __typeof__(*(ptr)) __user *__gu_addr = (ptr);		\
	might_sleep();							\
	if (access_ok(VERIFY_READ, __gu_addr, (size)))			\
		__get_user_size(__gu_val, __gu_addr, (size), __gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})

#define __get_user_nosleep(x, ptr, size)			\
({								\
	long __gu_err;						\
	unsigned long __gu_val;					\
	const __typeof__(*(ptr)) __user *__gu_addr = (ptr);	\
	__chk_user_ptr(ptr);					\
	__get_user_size(__gu_val, __gu_addr, (size), __gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})



extern unsigned long __copy_tofrom_user(void __user *to,
		const void __user *from, unsigned long size);

#ifndef __powerpc64__

static inline unsigned long copy_from_user(void *to,
		const void __user *from, unsigned long n)
{
	unsigned long over;

	if (access_ok(VERIFY_READ, from, n))
		return __copy_tofrom_user((__force void __user *)to, from, n);
	if ((unsigned long)from < TASK_SIZE) {
		over = (unsigned long)from + n - TASK_SIZE;
		return __copy_tofrom_user((__force void __user *)to, from,
				n - over) + over;
	}
	return n;
}

static inline unsigned long copy_to_user(void __user *to,
		const void *from, unsigned long n)
{
	unsigned long over;

	if (access_ok(VERIFY_WRITE, to, n))
		return __copy_tofrom_user(to, (__force void __user *)from, n);
	if ((unsigned long)to < TASK_SIZE) {
		over = (unsigned long)to + n - TASK_SIZE;
		return __copy_tofrom_user(to, (__force void __user *)from,
				n - over) + over;
	}
	return n;
}

#else 

#define __copy_in_user(to, from, size) \
	__copy_tofrom_user((to), (from), (size))

extern unsigned long copy_from_user(void *to, const void __user *from,
				    unsigned long n);
extern unsigned long copy_to_user(void __user *to, const void *from,
				  unsigned long n);
extern unsigned long copy_in_user(void __user *to, const void __user *from,
				  unsigned long n);

#endif 

static inline unsigned long __copy_from_user_inatomic(void *to,
		const void __user *from, unsigned long n)
{
	if (__builtin_constant_p(n) && (n <= 8)) {
		unsigned long ret = 1;

		switch (n) {
		case 1:
			__get_user_size(*(u8 *)to, from, 1, ret);
			break;
		case 2:
			__get_user_size(*(u16 *)to, from, 2, ret);
			break;
		case 4:
			__get_user_size(*(u32 *)to, from, 4, ret);
			break;
		case 8:
			__get_user_size(*(u64 *)to, from, 8, ret);
			break;
		}
		if (ret == 0)
			return 0;
	}
	return __copy_tofrom_user((__force void __user *)to, from, n);
}

static inline unsigned long __copy_to_user_inatomic(void __user *to,
		const void *from, unsigned long n)
{
	if (__builtin_constant_p(n) && (n <= 8)) {
		unsigned long ret = 1;

		switch (n) {
		case 1:
			__put_user_size(*(u8 *)from, (u8 __user *)to, 1, ret);
			break;
		case 2:
			__put_user_size(*(u16 *)from, (u16 __user *)to, 2, ret);
			break;
		case 4:
			__put_user_size(*(u32 *)from, (u32 __user *)to, 4, ret);
			break;
		case 8:
			__put_user_size(*(u64 *)from, (u64 __user *)to, 8, ret);
			break;
		}
		if (ret == 0)
			return 0;
	}
	return __copy_tofrom_user(to, (__force const void __user *)from, n);
}

static inline unsigned long __copy_from_user(void *to,
		const void __user *from, unsigned long size)
{
	might_sleep();
	return __copy_from_user_inatomic(to, from, size);
}

static inline unsigned long __copy_to_user(void __user *to,
		const void *from, unsigned long size)
{
	might_sleep();
	return __copy_to_user_inatomic(to, from, size);
}

extern unsigned long __clear_user(void __user *addr, unsigned long size);

static inline unsigned long clear_user(void __user *addr, unsigned long size)
{
	might_sleep();
	if (likely(access_ok(VERIFY_WRITE, addr, size)))
		return __clear_user(addr, size);
	if ((unsigned long)addr < TASK_SIZE) {
		unsigned long over = (unsigned long)addr + size - TASK_SIZE;
		return __clear_user(addr, size - over) + over;
	}
	return size;
}

extern int __strncpy_from_user(char *dst, const char __user *src, long count);

static inline long strncpy_from_user(char *dst, const char __user *src,
		long count)
{
	might_sleep();
	if (likely(access_ok(VERIFY_READ, src, 1)))
		return __strncpy_from_user(dst, src, count);
	return -EFAULT;
}

extern int __strnlen_user(const char __user *str, long len, unsigned long top);

static inline int strnlen_user(const char __user *str, long len)
{
	unsigned long top = current->thread.fs.seg;

	if ((unsigned long)str > top)
		return 0;
	return __strnlen_user(str, len, top);
}

#define strlen_user(str)	strnlen_user((str), 0x7ffffffe)

#endif  
#endif 

#endif	
