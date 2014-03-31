

#ifndef _CRIS_UACCESS_H
#define _CRIS_UACCESS_H

#ifndef __ASSEMBLY__
#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/processor.h>
#include <asm/page.h>

#define VERIFY_READ	0
#define VERIFY_WRITE	1


#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })


#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFF)
#define USER_DS		MAKE_MM_SEG(TASK_SIZE)

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current_thread_info()->addr_limit)
#define set_fs(x)	(current_thread_info()->addr_limit = (x))

#define segment_eq(a,b)	((a).seg == (b).seg)

#define __kernel_ok (segment_eq(get_fs(), KERNEL_DS))
#define __user_ok(addr,size) (((size) <= TASK_SIZE)&&((addr) <= TASK_SIZE-(size)))
#define __access_ok(addr,size) (__kernel_ok || __user_ok((addr),(size)))
#define access_ok(type,addr,size) __access_ok((unsigned long)(addr),(size))

#include <arch/uaccess.h>


struct exception_table_entry
{
	unsigned long insn, fixup;
};

#define get_user(x,ptr) \
  __get_user_check((x),(ptr),sizeof(*(ptr)))
#define put_user(x,ptr) \
  __put_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

#define __get_user(x,ptr) \
  __get_user_nocheck((x),(ptr),sizeof(*(ptr)))
#define __put_user(x,ptr) \
  __put_user_nocheck((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

extern long __put_user_bad(void);

#define __put_user_size(x,ptr,size,retval)			\
do {								\
	retval = 0;						\
	switch (size) {						\
	  case 1: __put_user_asm(x,ptr,retval,"move.b"); break;	\
	  case 2: __put_user_asm(x,ptr,retval,"move.w"); break;	\
	  case 4: __put_user_asm(x,ptr,retval,"move.d"); break;	\
	  case 8: __put_user_asm_64(x,ptr,retval); break;	\
	  default: __put_user_bad();				\
	}							\
} while (0)

#define __get_user_size(x,ptr,size,retval)			\
do {								\
	retval = 0;						\
	switch (size) {						\
	  case 1: __get_user_asm(x,ptr,retval,"move.b"); break;	\
	  case 2: __get_user_asm(x,ptr,retval,"move.w"); break;	\
	  case 4: __get_user_asm(x,ptr,retval,"move.d"); break;	\
	  case 8: __get_user_asm_64(x,ptr,retval); break;	\
	  default: (x) = __get_user_bad();			\
	}							\
} while (0)

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

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct *)(x))



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


extern unsigned long __copy_user(void __user *to, const void *from, unsigned long n);
extern unsigned long __copy_user_zeroing(void *to, const void __user *from, unsigned long n);
extern unsigned long __do_clear_user(void __user *to, unsigned long n);

static inline unsigned long
__generic_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	if (access_ok(VERIFY_WRITE, to, n))
		return __copy_user(to,from,n);
	return n;
}

static inline unsigned long
__generic_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	if (access_ok(VERIFY_READ, from, n))
		return __copy_user_zeroing(to,from,n);
	return n;
}

static inline unsigned long
__generic_clear_user(void __user *to, unsigned long n)
{
	if (access_ok(VERIFY_WRITE, to, n))
		return __do_clear_user(to,n);
	return n;
}

static inline long
__strncpy_from_user(char *dst, const char __user *src, long count)
{
	return __do_strncpy_from_user(dst, src, count);
}

static inline long
strncpy_from_user(char *dst, const char __user *src, long count)
{
	long res = -EFAULT;
	if (access_ok(VERIFY_READ, src, 1))
		res = __do_strncpy_from_user(dst, src, count);
	return res;
}



static inline unsigned long
__constant_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	unsigned long ret = 0;
	if (n == 0)
		;
	else if (n == 1)
		__asm_copy_from_user_1(to, from, ret);
	else if (n == 2)
		__asm_copy_from_user_2(to, from, ret);
	else if (n == 3)
		__asm_copy_from_user_3(to, from, ret);
	else if (n == 4)
		__asm_copy_from_user_4(to, from, ret);
	else if (n == 5)
		__asm_copy_from_user_5(to, from, ret);
	else if (n == 6)
		__asm_copy_from_user_6(to, from, ret);
	else if (n == 7)
		__asm_copy_from_user_7(to, from, ret);
	else if (n == 8)
		__asm_copy_from_user_8(to, from, ret);
	else if (n == 9)
		__asm_copy_from_user_9(to, from, ret);
	else if (n == 10)
		__asm_copy_from_user_10(to, from, ret);
	else if (n == 11)
		__asm_copy_from_user_11(to, from, ret);
	else if (n == 12)
		__asm_copy_from_user_12(to, from, ret);
	else if (n == 13)
		__asm_copy_from_user_13(to, from, ret);
	else if (n == 14)
		__asm_copy_from_user_14(to, from, ret);
	else if (n == 15)
		__asm_copy_from_user_15(to, from, ret);
	else if (n == 16)
		__asm_copy_from_user_16(to, from, ret);
	else if (n == 20)
		__asm_copy_from_user_20(to, from, ret);
	else if (n == 24)
		__asm_copy_from_user_24(to, from, ret);
	else
		ret = __generic_copy_from_user(to, from, n);

	return ret;
}


static inline unsigned long
__constant_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	unsigned long ret = 0;
	if (n == 0)
		;
	else if (n == 1)
		__asm_copy_to_user_1(to, from, ret);
	else if (n == 2)
		__asm_copy_to_user_2(to, from, ret);
	else if (n == 3)
		__asm_copy_to_user_3(to, from, ret);
	else if (n == 4)
		__asm_copy_to_user_4(to, from, ret);
	else if (n == 5)
		__asm_copy_to_user_5(to, from, ret);
	else if (n == 6)
		__asm_copy_to_user_6(to, from, ret);
	else if (n == 7)
		__asm_copy_to_user_7(to, from, ret);
	else if (n == 8)
		__asm_copy_to_user_8(to, from, ret);
	else if (n == 9)
		__asm_copy_to_user_9(to, from, ret);
	else if (n == 10)
		__asm_copy_to_user_10(to, from, ret);
	else if (n == 11)
		__asm_copy_to_user_11(to, from, ret);
	else if (n == 12)
		__asm_copy_to_user_12(to, from, ret);
	else if (n == 13)
		__asm_copy_to_user_13(to, from, ret);
	else if (n == 14)
		__asm_copy_to_user_14(to, from, ret);
	else if (n == 15)
		__asm_copy_to_user_15(to, from, ret);
	else if (n == 16)
		__asm_copy_to_user_16(to, from, ret);
	else if (n == 20)
		__asm_copy_to_user_20(to, from, ret);
	else if (n == 24)
		__asm_copy_to_user_24(to, from, ret);
	else
		ret = __generic_copy_to_user(to, from, n);

	return ret;
}


static inline unsigned long
__constant_clear_user(void __user *to, unsigned long n)
{
	unsigned long ret = 0;
	if (n == 0)
		;
	else if (n == 1)
		__asm_clear_1(to, ret);
	else if (n == 2)
		__asm_clear_2(to, ret);
	else if (n == 3)
		__asm_clear_3(to, ret);
	else if (n == 4)
		__asm_clear_4(to, ret);
	else if (n == 8)
		__asm_clear_8(to, ret);
	else if (n == 12)
		__asm_clear_12(to, ret);
	else if (n == 16)
		__asm_clear_16(to, ret);
	else if (n == 20)
		__asm_clear_20(to, ret);
	else if (n == 24)
		__asm_clear_24(to, ret);
	else
		ret = __generic_clear_user(to, n);

	return ret;
}


#define clear_user(to, n)			\
(__builtin_constant_p(n) ?			\
 __constant_clear_user(to, n) :			\
 __generic_clear_user(to, n))

#define copy_from_user(to, from, n)		\
(__builtin_constant_p(n) ?			\
 __constant_copy_from_user(to, from, n) :	\
 __generic_copy_from_user(to, from, n))

#define copy_to_user(to, from, n)		\
(__builtin_constant_p(n) ?			\
 __constant_copy_to_user(to, from, n) :		\
 __generic_copy_to_user(to, from, n))


static inline unsigned long
__generic_copy_from_user_nocheck(void *to, const void __user *from,
				 unsigned long n)
{
	return __copy_user_zeroing(to,from,n);
}

static inline unsigned long
__generic_copy_to_user_nocheck(void __user *to, const void *from,
			       unsigned long n)
{
	return __copy_user(to,from,n);
}

static inline unsigned long
__generic_clear_user_nocheck(void __user *to, unsigned long n)
{
	return __do_clear_user(to,n);
}


#define __copy_to_user(to,from,n)   __generic_copy_to_user_nocheck((to),(from),(n))
#define __copy_from_user(to,from,n) __generic_copy_from_user_nocheck((to),(from),(n))
#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user
#define __clear_user(to,n) __generic_clear_user_nocheck((to),(n))

#define strlen_user(str)	strnlen_user((str), 0x7ffffffe)

#endif  

#endif	
