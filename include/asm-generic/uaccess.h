#ifndef __ASM_GENERIC_UACCESS_H
#define __ASM_GENERIC_UACCESS_H

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>

#include <asm/segment.h>

#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })

#ifndef KERNEL_DS
#define KERNEL_DS	MAKE_MM_SEG(~0UL)
#endif

#ifndef USER_DS
#define USER_DS		MAKE_MM_SEG(TASK_SIZE - 1)
#endif

#ifndef get_fs
#define get_ds()	(KERNEL_DS)
#define get_fs()	(current_thread_info()->addr_limit)

static inline void set_fs(mm_segment_t fs)
{
	current_thread_info()->addr_limit = fs;
}
#endif

#define segment_eq(a, b) ((a).seg == (b).seg)

#define VERIFY_READ	0
#define VERIFY_WRITE	1

#define access_ok(type, addr, size) __access_ok((unsigned long)(addr),(size))

#ifndef __access_ok
static inline int __access_ok(unsigned long addr, unsigned long size)
{
	return 1;
}
#endif


struct exception_table_entry
{
	unsigned long insn, fixup;
};

extern unsigned long search_exception_table(unsigned long);

#ifndef __copy_from_user
static inline __must_check long __copy_from_user(void *to,
		const void __user * from, unsigned long n)
{
	if (__builtin_constant_p(n)) {
		switch(n) {
		case 1:
			*(u8 *)to = *(u8 __force *)from;
			return 0;
		case 2:
			*(u16 *)to = *(u16 __force *)from;
			return 0;
		case 4:
			*(u32 *)to = *(u32 __force *)from;
			return 0;
#ifdef CONFIG_64BIT
		case 8:
			*(u64 *)to = *(u64 __force *)from;
			return 0;
#endif
		default:
			break;
		}
	}

	memcpy(to, (const void __force *)from, n);
	return 0;
}
#endif

#ifndef __copy_to_user
static inline __must_check long __copy_to_user(void __user *to,
		const void *from, unsigned long n)
{
	if (__builtin_constant_p(n)) {
		switch(n) {
		case 1:
			*(u8 __force *)to = *(u8 *)from;
			return 0;
		case 2:
			*(u16 __force *)to = *(u16 *)from;
			return 0;
		case 4:
			*(u32 __force *)to = *(u32 *)from;
			return 0;
#ifdef CONFIG_64BIT
		case 8:
			*(u64 __force *)to = *(u64 *)from;
			return 0;
#endif
		default:
			break;
		}
	}

	memcpy((void __force *)to, from, n);
	return 0;
}
#endif

#define __put_user(x, ptr) \
({								\
	__typeof__(*(ptr)) __x = (x);				\
	int __pu_err = -EFAULT;					\
        __chk_user_ptr(ptr);                                    \
	switch (sizeof (*(ptr))) {				\
	case 1:							\
	case 2:							\
	case 4:							\
	case 8:							\
		__pu_err = __put_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		break;						\
	default:						\
		__put_user_bad();				\
		break;						\
	 }							\
	__pu_err;						\
})

#define put_user(x, ptr)					\
({								\
	might_sleep();						\
	access_ok(VERIFY_WRITE, ptr, sizeof(*ptr)) ?		\
		__put_user(x, ptr) :				\
		-EFAULT;					\
})

static inline int __put_user_fn(size_t size, void __user *ptr, void *x)
{
	size = __copy_to_user(ptr, x, size);
	return size ? -EFAULT : size;
}

extern int __put_user_bad(void) __attribute__((noreturn));

#define __get_user(x, ptr)					\
({								\
	int __gu_err = -EFAULT;					\
	__chk_user_ptr(ptr);					\
	switch (sizeof(*(ptr))) {				\
	case 1: {						\
		unsigned char __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	case 2: {						\
		unsigned short __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	case 4: {						\
		unsigned int __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	case 8: {						\
		unsigned long long __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	default:						\
		__get_user_bad();				\
		break;						\
	}							\
	__gu_err;						\
})

#define get_user(x, ptr)					\
({								\
	might_sleep();						\
	access_ok(VERIFY_READ, ptr, sizeof(*ptr)) ?		\
		__get_user(x, ptr) :				\
		-EFAULT;					\
})

static inline int __get_user_fn(size_t size, const void __user *ptr, void *x)
{
	size = __copy_from_user(x, ptr, size);
	return size ? -EFAULT : size;
}

extern int __get_user_bad(void) __attribute__((noreturn));

#ifndef __copy_from_user_inatomic
#define __copy_from_user_inatomic __copy_from_user
#endif

#ifndef __copy_to_user_inatomic
#define __copy_to_user_inatomic __copy_to_user
#endif

static inline long copy_from_user(void *to,
		const void __user * from, unsigned long n)
{
	might_sleep();
	if (access_ok(VERIFY_READ, from, n))
		return __copy_from_user(to, from, n);
	else
		return n;
}

static inline long copy_to_user(void __user *to,
		const void *from, unsigned long n)
{
	might_sleep();
	if (access_ok(VERIFY_WRITE, to, n))
		return __copy_to_user(to, from, n);
	else
		return n;
}

#ifndef __strncpy_from_user
static inline long
__strncpy_from_user(char *dst, const char __user *src, long count)
{
	char *tmp;
	strncpy(dst, (const char __force *)src, count);
	for (tmp = dst; *tmp && count > 0; tmp++, count--)
		;
	return (tmp - dst);
}
#endif

static inline long
strncpy_from_user(char *dst, const char __user *src, long count)
{
	if (!access_ok(VERIFY_READ, src, 1))
		return -EFAULT;
	return __strncpy_from_user(dst, src, count);
}

#ifndef __strnlen_user
#define __strnlen_user(s, n) (strnlen((s), (n)) + 1)
#endif

static inline long strnlen_user(const char __user *src, long n)
{
	if (!access_ok(VERIFY_READ, src, 1))
		return 0;
	return __strnlen_user(src, n);
}

static inline long strlen_user(const char __user *src)
{
	return strnlen_user(src, 32767);
}

#ifndef __clear_user
static inline __must_check unsigned long
__clear_user(void __user *to, unsigned long n)
{
	memset((void __force *)to, 0, n);
	return 0;
}
#endif

static inline __must_check unsigned long
clear_user(void __user *to, unsigned long n)
{
	might_sleep();
	if (!access_ok(VERIFY_WRITE, to, n))
		return n;

	return __clear_user(to, n);
}

#endif 
