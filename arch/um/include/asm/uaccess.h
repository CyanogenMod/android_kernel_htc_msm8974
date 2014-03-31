/* 
 * Copyright (C) 2002 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __UM_UACCESS_H
#define __UM_UACCESS_H

typedef struct {
	unsigned long seg;
} mm_segment_t;

#include <linux/thread_info.h>
#include <linux/errno.h>
#include <asm/processor.h>
#include <asm/elf.h>

#define VERIFY_READ 0
#define VERIFY_WRITE 1


#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })

#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFF)
#define USER_DS		MAKE_MM_SEG(TASK_SIZE)

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current_thread_info()->addr_limit)
#define set_fs(x)	(current_thread_info()->addr_limit = (x))

#define segment_eq(a, b) ((a).seg == (b).seg)

#define __under_task_size(addr, size) \
	(((unsigned long) (addr) < TASK_SIZE) && \
	 (((unsigned long) (addr) + (size)) < TASK_SIZE))

#define __access_ok_vsyscall(type, addr, size) \
	 ((type == VERIFY_READ) && \
	  ((unsigned long) (addr) >= FIXADDR_USER_START) && \
	  ((unsigned long) (addr) + (size) <= FIXADDR_USER_END) && \
	  ((unsigned long) (addr) + (size) >= (unsigned long)(addr)))

#define __addr_range_nowrap(addr, size) \
	((unsigned long) (addr) <= ((unsigned long) (addr) + (size)))

#define access_ok(type, addr, size) \
	(__addr_range_nowrap(addr, size) && \
	 (__under_task_size(addr, size) || \
	  __access_ok_vsyscall(type, addr, size) || \
	  segment_eq(get_fs(), KERNEL_DS)))

extern int copy_from_user(void *to, const void __user *from, int n);
extern int copy_to_user(void __user *to, const void *from, int n);


extern int strncpy_from_user(char *dst, const char __user *src, int count);

extern int __clear_user(void __user *mem, int len);

extern int clear_user(void __user *mem, int len);

extern int strnlen_user(const void __user *str, int len);

#define __copy_from_user(to, from, n) copy_from_user(to, from, n)

#define __copy_to_user(to, from, n) copy_to_user(to, from, n)

#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

#define __get_user(x, ptr) \
({ \
	const __typeof__(*(ptr)) __user *__private_ptr = (ptr);	\
	__typeof__(x) __private_val;			\
	int __private_ret = -EFAULT;			\
	(x) = (__typeof__(*(__private_ptr)))0;				\
	if (__copy_from_user((__force void *)&__private_val, (__private_ptr),\
			     sizeof(*(__private_ptr))) == 0) {		\
		(x) = (__typeof__(*(__private_ptr))) __private_val;	\
		__private_ret = 0;					\
	}								\
	__private_ret;							\
}) 

#define get_user(x, ptr) \
({ \
        const __typeof__((*(ptr))) __user *private_ptr = (ptr); \
        (access_ok(VERIFY_READ, private_ptr, sizeof(*private_ptr)) ? \
	 __get_user(x, private_ptr) : ((x) = (__typeof__(*ptr))0, -EFAULT)); \
})

#define __put_user(x, ptr) \
({ \
        __typeof__(*(ptr)) __user *__private_ptr = ptr; \
        __typeof__(*(__private_ptr)) __private_val; \
        int __private_ret = -EFAULT; \
        __private_val = (__typeof__(*(__private_ptr))) (x); \
        if (__copy_to_user((__private_ptr), &__private_val, \
			   sizeof(*(__private_ptr))) == 0) { \
		__private_ret = 0; \
	} \
        __private_ret; \
})

#define put_user(x, ptr) \
({ \
        __typeof__(*(ptr)) __user *private_ptr = (ptr); \
        (access_ok(VERIFY_WRITE, private_ptr, sizeof(*private_ptr)) ? \
	 __put_user(x, private_ptr) : -EFAULT); \
})

#define strlen_user(str) strnlen_user(str, ~0U >> 1)

struct exception_table_entry
{
        unsigned long insn;
	unsigned long fixup;
};

#endif
