#ifndef __SCORE_UACCESS_H
#define __SCORE_UACCESS_H

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/thread_info.h>

#define VERIFY_READ		0
#define VERIFY_WRITE		1

#define get_ds()		(KERNEL_DS)
#define get_fs()		(current_thread_info()->addr_limit)
#define segment_eq(a, b)	((a).seg == (b).seg)

#define __ua_size(size)							\
	((__builtin_constant_p(size) && (signed long) (size) > 0) ? 0 : (size))


#define __access_ok(addr, size)					\
	(((long)((get_fs().seg) &				\
		 ((addr) | ((addr) + (size)) |			\
		  __ua_size(size)))) == 0)

#define access_ok(type, addr, size)				\
	likely(__access_ok((unsigned long)(addr), (size)))

#define put_user(x, ptr) __put_user_check((x), (ptr), sizeof(*(ptr)))

#define get_user(x, ptr) __get_user_check((x), (ptr), sizeof(*(ptr)))

#define __put_user(x, ptr) __put_user_nocheck((x), (ptr), sizeof(*(ptr)))

#define __get_user(x, ptr) __get_user_nocheck((x), (ptr), sizeof(*(ptr)))

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct __user *)(x))

extern void __get_user_unknown(void);

#define __get_user_common(val, size, ptr)				\
do {									\
	switch (size) {							\
	case 1:								\
		__get_user_asm(val, "lb", ptr);				\
		break;							\
	case 2:								\
		__get_user_asm(val, "lh", ptr);				\
		 break;							\
	case 4:								\
		__get_user_asm(val, "lw", ptr);				\
		 break;							\
	case 8: 							\
		if ((copy_from_user((void *)&val, ptr, 8)) == 0)	\
			__gu_err = 0;					\
		else							\
			__gu_err = -EFAULT;				\
		break;							\
	default:							\
		__get_user_unknown();					\
		break;							\
	}								\
} while (0)

#define __get_user_nocheck(x, ptr, size)				\
({									\
	long __gu_err = 0;						\
	__get_user_common((x), size, ptr);				\
	__gu_err;							\
})

#define __get_user_check(x, ptr, size)					\
({									\
	long __gu_err = -EFAULT;					\
	const __typeof__(*(ptr)) __user *__gu_ptr = (ptr);		\
									\
	if (likely(access_ok(VERIFY_READ, __gu_ptr, size)))		\
		__get_user_common((x), size, __gu_ptr);			\
									\
	__gu_err;							\
})

#define __get_user_asm(val, insn, addr)					\
{									\
	long __gu_tmp;							\
									\
	__asm__ __volatile__(						\
		"1:" insn " %1, %3\n"					\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"3:li	%0, %4\n"					\
		"j	2b\n"						\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		".word	1b, 3b\n"					\
		".previous\n"						\
		: "=r" (__gu_err), "=r" (__gu_tmp)			\
		: "0" (0), "o" (__m(addr)), "i" (-EFAULT));		\
									\
		(val) = (__typeof__(*(addr))) __gu_tmp;			\
}

#define __put_user_nocheck(val, ptr, size)				\
({									\
	__typeof__(*(ptr)) __pu_val;					\
	long __pu_err = 0;						\
									\
	__pu_val = (val);						\
	switch (size) {							\
	case 1:								\
		__put_user_asm("sb", ptr);				\
		break;							\
	case 2:								\
		__put_user_asm("sh", ptr);				\
		break;							\
	case 4:								\
		__put_user_asm("sw", ptr);				\
		break;							\
	case 8: 							\
		if ((__copy_to_user((void *)ptr, &__pu_val, 8)) == 0)	\
			__pu_err = 0;					\
		else							\
			__pu_err = -EFAULT;				\
		break;							\
	default:							\
		 __put_user_unknown();					\
		 break;							\
	}								\
	__pu_err;							\
})


#define __put_user_check(val, ptr, size)				\
({									\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);			\
	__typeof__(*(ptr)) __pu_val = (val);				\
	long __pu_err = -EFAULT;					\
									\
	if (likely(access_ok(VERIFY_WRITE, __pu_addr, size))) {		\
		switch (size) {						\
		case 1:							\
			__put_user_asm("sb", __pu_addr);		\
			break;						\
		case 2:							\
			__put_user_asm("sh", __pu_addr);		\
			break;						\
		case 4:							\
			__put_user_asm("sw", __pu_addr);		\
			break;						\
		case 8: 						\
			if ((__copy_to_user((void *)__pu_addr, &__pu_val, 8)) == 0)\
				__pu_err = 0;				\
			else						\
				__pu_err = -EFAULT;			\
			break;						\
		default:						\
			__put_user_unknown();				\
			break;						\
		}							\
	}								\
	__pu_err;							\
})

#define __put_user_asm(insn, ptr)					\
	__asm__ __volatile__(						\
		"1:" insn " %2, %3\n"					\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"3:li %0, %4\n"						\
		"j 2b\n"						\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		".word 1b, 3b\n"					\
		".previous\n"						\
		: "=r" (__pu_err)					\
		: "0" (0), "r" (__pu_val), "o" (__m(ptr)),		\
		  "i" (-EFAULT));

extern void __put_user_unknown(void);
extern int __copy_tofrom_user(void *to, const void *from, unsigned long len);

static inline unsigned long
copy_from_user(void *to, const void *from, unsigned long len)
{
	unsigned long over;

	if (access_ok(VERIFY_READ, from, len))
		return __copy_tofrom_user(to, from, len);

	if ((unsigned long)from < TASK_SIZE) {
		over = (unsigned long)from + len - TASK_SIZE;
		return __copy_tofrom_user(to, from, len - over) + over;
	}
	return len;
}

static inline unsigned long
copy_to_user(void *to, const void *from, unsigned long len)
{
	unsigned long over;

	if (access_ok(VERIFY_WRITE, to, len))
		return __copy_tofrom_user(to, from, len);

	if ((unsigned long)to < TASK_SIZE) {
		over = (unsigned long)to + len - TASK_SIZE;
		return __copy_tofrom_user(to, from, len - over) + over;
	}
	return len;
}

#define __copy_from_user(to, from, len)	\
		__copy_tofrom_user((to), (from), (len))

#define __copy_to_user(to, from, len)		\
		__copy_tofrom_user((to), (from), (len))

static inline unsigned long
__copy_to_user_inatomic(void *to, const void *from, unsigned long len)
{
	return __copy_to_user(to, from, len);
}

static inline unsigned long
__copy_from_user_inatomic(void *to, const void *from, unsigned long len)
{
	return __copy_from_user(to, from, len);
}

#define __copy_in_user(to, from, len)	__copy_from_user(to, from, len)

static inline unsigned long
copy_in_user(void *to, const void *from, unsigned long len)
{
	if (access_ok(VERIFY_READ, from, len) &&
		      access_ok(VERFITY_WRITE, to, len))
		return copy_from_user(to, from, len);
}

extern unsigned long __clear_user(void __user *src, unsigned long size);

static inline unsigned long clear_user(char *src, unsigned long size)
{
	if (access_ok(VERIFY_WRITE, src, size))
		return __clear_user(src, size);

	return -EFAULT;
}
extern int __strncpy_from_user(char *dst, const char *src, long len);

static inline int strncpy_from_user(char *dst, const char *src, long len)
{
	if (access_ok(VERIFY_READ, src, 1))
		return __strncpy_from_user(dst, src, len);

	return -EFAULT;
}

extern int __strlen_user(const char *src);
static inline long strlen_user(const char __user *src)
{
	return __strlen_user(src);
}

extern int __strnlen_user(const char *str, long len);
static inline long strnlen_user(const char __user *str, long len)
{
	if (!access_ok(VERIFY_READ, str, 0))
		return 0;
	else		
		return __strnlen_user(str, len);
}

struct exception_table_entry {
	unsigned long insn;
	unsigned long fixup;
};

extern int fixup_exception(struct pt_regs *regs);

#endif 

