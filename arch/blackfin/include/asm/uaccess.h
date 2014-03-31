/*
 * Copyright 2004-2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 *
 * Based on: include/asm-m68knommu/uaccess.h
 */

#ifndef __BLACKFIN_UACCESS_H
#define __BLACKFIN_UACCESS_H

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>

#include <asm/segment.h>
#include <asm/sections.h>

#define get_ds()        (KERNEL_DS)
#define get_fs()        (current_thread_info()->addr_limit)

static inline void set_fs(mm_segment_t fs)
{
	current_thread_info()->addr_limit = fs;
}

#define segment_eq(a,b) ((a) == (b))

#define VERIFY_READ	0
#define VERIFY_WRITE	1

#define access_ok(type, addr, size) _access_ok((unsigned long)(addr), (size))

static inline int is_in_rom(unsigned long addr)
{
	if ((addr < _ramstart) || (addr >= _ramend))
		return (1);

	
	return (0);
}


#ifndef CONFIG_ACCESS_CHECK
static inline int _access_ok(unsigned long addr, unsigned long size) { return 1; }
#else
extern int _access_ok(unsigned long addr, unsigned long size);
#endif


struct exception_table_entry {
	unsigned long insn, fixup;
};


#define put_user(x,p)						\
	({							\
		int _err = 0;					\
		typeof(*(p)) _x = (x);				\
		typeof(*(p)) *_p = (p);				\
		if (!access_ok(VERIFY_WRITE, _p, sizeof(*(_p)))) {\
			_err = -EFAULT;				\
		}						\
		else {						\
		switch (sizeof (*(_p))) {			\
		case 1:						\
			__put_user_asm(_x, _p, B);		\
			break;					\
		case 2:						\
			__put_user_asm(_x, _p, W);		\
			break;					\
		case 4:						\
			__put_user_asm(_x, _p,  );		\
			break;					\
		case 8: {					\
			long _xl, _xh;				\
			_xl = ((long *)&_x)[0];			\
			_xh = ((long *)&_x)[1];			\
			__put_user_asm(_xl, ((long *)_p)+0, );	\
			__put_user_asm(_xh, ((long *)_p)+1, );	\
		} break;					\
		default:					\
			_err = __put_user_bad();		\
			break;					\
		}						\
		}						\
		_err;						\
	})

#define __put_user(x,p) put_user(x,p)
static inline int bad_user_access_length(void)
{
	panic("bad_user_access_length");
	return -1;
}

#define __put_user_bad() (printk(KERN_INFO "put_user_bad %s:%d %s\n",\
                           __FILE__, __LINE__, __func__),\
                           bad_user_access_length(), (-EFAULT))


#define __ptr(x) ((unsigned long *)(x))

#define __put_user_asm(x,p,bhw)				\
	__asm__ (#bhw"[%1] = %0;\n\t"			\
		 : 			\
		 :"d" (x),"a" (__ptr(p)) : "memory")

#define get_user(x, ptr)					\
({								\
	int _err = 0;						\
	unsigned long _val = 0;					\
	const typeof(*(ptr)) __user *_p = (ptr);		\
	const size_t ptr_size = sizeof(*(_p));			\
	if (likely(access_ok(VERIFY_READ, _p, ptr_size))) {	\
		BUILD_BUG_ON(ptr_size >= 8);			\
		switch (ptr_size) {				\
		case 1:						\
			__get_user_asm(_val, _p, B,(Z));	\
			break;					\
		case 2:						\
			__get_user_asm(_val, _p, W,(Z));	\
			break;					\
		case 4:						\
			__get_user_asm(_val, _p,  , );		\
			break;					\
		}						\
	} else							\
		_err = -EFAULT;					\
	x = (typeof(*(ptr)))_val;				\
	_err;							\
})

#define __get_user(x,p) get_user(x,p)

#define __get_user_bad() (bad_user_access_length(), (-EFAULT))

#define __get_user_asm(x, ptr, bhw, option)	\
({						\
	__asm__ __volatile__ (			\
		"%0 =" #bhw "[%1]" #option ";"	\
		: "=d" (x)			\
		: "a" (__ptr(ptr)));		\
})

#define __copy_from_user(to, from, n) copy_from_user(to, from, n)
#define __copy_to_user(to, from, n) copy_to_user(to, from, n)
#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

#define copy_to_user_ret(to,from,n,retval) ({ if (copy_to_user(to,from,n))\
				                 return retval; })

#define copy_from_user_ret(to,from,n,retval) ({ if (copy_from_user(to,from,n))\
                                                   return retval; })

static inline unsigned long __must_check
copy_from_user(void *to, const void __user *from, unsigned long n)
{
	if (access_ok(VERIFY_READ, from, n))
		memcpy(to, (const void __force *)from, n);
	else
		return n;
	return 0;
}

static inline unsigned long __must_check
copy_to_user(void __user *to, const void *from, unsigned long n)
{
	if (access_ok(VERIFY_WRITE, to, n))
		memcpy((void __force *)to, from, n);
	else
		return n;
	return 0;
}


static inline long __must_check
strncpy_from_user(char *dst, const char *src, long count)
{
	char *tmp;
	if (!access_ok(VERIFY_READ, src, 1))
		return -EFAULT;
	strncpy(dst, src, count);
	for (tmp = dst; *tmp && count > 0; tmp++, count--) ;
	return (tmp - dst);
}

static inline long __must_check strnlen_user(const char *src, long n)
{
	if (!access_ok(VERIFY_READ, src, 1))
		return 0;
	return strnlen(src, n) + 1;
}

static inline long __must_check strlen_user(const char *src)
{
	if (!access_ok(VERIFY_READ, src, 1))
		return 0;
	return strlen(src) + 1;
}


static inline unsigned long __must_check
__clear_user(void *to, unsigned long n)
{
	if (!access_ok(VERIFY_WRITE, to, n))
		return n;
	memset(to, 0, n);
	return 0;
}

#define clear_user(to, n) __clear_user(to, n)

enum {
	BFIN_MEM_ACCESS_CORE = 0,
	BFIN_MEM_ACCESS_CORE_ONLY,
	BFIN_MEM_ACCESS_DMA,
	BFIN_MEM_ACCESS_IDMA,
	BFIN_MEM_ACCESS_ITEST,
};
int bfin_mem_access_type(unsigned long addr, unsigned long size);

#endif				
