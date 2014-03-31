#ifndef _ASM_X86_UACCESS_32_H
#define _ASM_X86_UACCESS_32_H

#include <linux/errno.h>
#include <linux/thread_info.h>
#include <linux/string.h>
#include <asm/asm.h>
#include <asm/page.h>

unsigned long __must_check __copy_to_user_ll
		(void __user *to, const void *from, unsigned long n);
unsigned long __must_check __copy_from_user_ll
		(void *to, const void __user *from, unsigned long n);
unsigned long __must_check __copy_from_user_ll_nozero
		(void *to, const void __user *from, unsigned long n);
unsigned long __must_check __copy_from_user_ll_nocache
		(void *to, const void __user *from, unsigned long n);
unsigned long __must_check __copy_from_user_ll_nocache_nozero
		(void *to, const void __user *from, unsigned long n);


static __always_inline unsigned long __must_check
__copy_to_user_inatomic(void __user *to, const void *from, unsigned long n)
{
	if (__builtin_constant_p(n)) {
		unsigned long ret;

		switch (n) {
		case 1:
			__put_user_size(*(u8 *)from, (u8 __user *)to,
					1, ret, 1);
			return ret;
		case 2:
			__put_user_size(*(u16 *)from, (u16 __user *)to,
					2, ret, 2);
			return ret;
		case 4:
			__put_user_size(*(u32 *)from, (u32 __user *)to,
					4, ret, 4);
			return ret;
		}
	}
	return __copy_to_user_ll(to, from, n);
}

static __always_inline unsigned long __must_check
__copy_to_user(void __user *to, const void *from, unsigned long n)
{
	might_fault();
	return __copy_to_user_inatomic(to, from, n);
}

static __always_inline unsigned long
__copy_from_user_inatomic(void *to, const void __user *from, unsigned long n)
{
	if (__builtin_constant_p(n)) {
		unsigned long ret;

		switch (n) {
		case 1:
			__get_user_size(*(u8 *)to, from, 1, ret, 1);
			return ret;
		case 2:
			__get_user_size(*(u16 *)to, from, 2, ret, 2);
			return ret;
		case 4:
			__get_user_size(*(u32 *)to, from, 4, ret, 4);
			return ret;
		}
	}
	return __copy_from_user_ll_nozero(to, from, n);
}

static __always_inline unsigned long
__copy_from_user(void *to, const void __user *from, unsigned long n)
{
	might_fault();
	if (__builtin_constant_p(n)) {
		unsigned long ret;

		switch (n) {
		case 1:
			__get_user_size(*(u8 *)to, from, 1, ret, 1);
			return ret;
		case 2:
			__get_user_size(*(u16 *)to, from, 2, ret, 2);
			return ret;
		case 4:
			__get_user_size(*(u32 *)to, from, 4, ret, 4);
			return ret;
		}
	}
	return __copy_from_user_ll(to, from, n);
}

static __always_inline unsigned long __copy_from_user_nocache(void *to,
				const void __user *from, unsigned long n)
{
	might_fault();
	if (__builtin_constant_p(n)) {
		unsigned long ret;

		switch (n) {
		case 1:
			__get_user_size(*(u8 *)to, from, 1, ret, 1);
			return ret;
		case 2:
			__get_user_size(*(u16 *)to, from, 2, ret, 2);
			return ret;
		case 4:
			__get_user_size(*(u32 *)to, from, 4, ret, 4);
			return ret;
		}
	}
	return __copy_from_user_ll_nocache(to, from, n);
}

static __always_inline unsigned long
__copy_from_user_inatomic_nocache(void *to, const void __user *from,
				  unsigned long n)
{
       return __copy_from_user_ll_nocache_nozero(to, from, n);
}

unsigned long __must_check copy_to_user(void __user *to,
					const void *from, unsigned long n);
unsigned long __must_check _copy_from_user(void *to,
					  const void __user *from,
					  unsigned long n);


extern void copy_from_user_overflow(void)
#ifdef CONFIG_DEBUG_STRICT_USER_COPY_CHECKS
	__compiletime_error("copy_from_user() buffer size is not provably correct")
#else
	__compiletime_warning("copy_from_user() buffer size is not provably correct")
#endif
;

static inline unsigned long __must_check copy_from_user(void *to,
					  const void __user *from,
					  unsigned long n)
{
	int sz = __compiletime_object_size(to);

	if (likely(sz == -1 || sz >= n))
		n = _copy_from_user(to, from, n);
	else
		copy_from_user_overflow();

	return n;
}

#define strlen_user(str) strnlen_user(str, LONG_MAX)

long strnlen_user(const char __user *str, long n);
unsigned long __must_check clear_user(void __user *mem, unsigned long len);
unsigned long __must_check __clear_user(void __user *mem, unsigned long len);

#endif 
