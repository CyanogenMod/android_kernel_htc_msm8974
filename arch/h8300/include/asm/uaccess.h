#ifndef __H8300_UACCESS_H
#define __H8300_UACCESS_H

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>

#include <asm/segment.h>

#define VERIFY_READ	0
#define VERIFY_WRITE	1

#define access_ok(type, addr, size) __access_ok((unsigned long)addr,size)
static inline int __access_ok(unsigned long addr, unsigned long size)
{
#define	RANGE_CHECK_OK(addr, size, lower, upper) \
	(((addr) >= (lower)) && (((addr) + (size)) < (upper)))

	extern unsigned long _ramend;
	return(RANGE_CHECK_OK(addr, size, 0L, (unsigned long)&_ramend));
}


struct exception_table_entry
{
	unsigned long insn, fixup;
};

extern unsigned long search_exception_table(unsigned long);



#define put_user(x, ptr)				\
({							\
    int __pu_err = 0;					\
    typeof(*(ptr)) __pu_val = (x);			\
    switch (sizeof (*(ptr))) {				\
    case 1:						\
    case 2:						\
    case 4:						\
	*(ptr) = (__pu_val);   	        		\
	break;						\
    case 8:						\
	memcpy(ptr, &__pu_val, sizeof (*(ptr)));        \
	break;						\
    default:						\
	__pu_err = __put_user_bad();			\
	break;						\
    }							\
    __pu_err;						\
})
#define __put_user(x, ptr) put_user(x, ptr)

extern int __put_user_bad(void);


#define __ptr(x) ((unsigned long *)(x))


#define get_user(x, ptr)					\
({								\
    int __gu_err = 0;						\
    typeof(*(ptr)) __gu_val = *ptr;				\
    switch (sizeof(*(ptr))) {					\
    case 1:							\
    case 2:							\
    case 4:							\
    case 8: 							\
	break;							\
    default:							\
	__gu_err = __get_user_bad();				\
	__gu_val = 0;						\
	break;							\
    }								\
    (x) = __gu_val;						\
    __gu_err;							\
})
#define __get_user(x, ptr) get_user(x, ptr)

extern int __get_user_bad(void);

#define copy_from_user(to, from, n)		(memcpy(to, from, n), 0)
#define copy_to_user(to, from, n)		(memcpy(to, from, n), 0)

#define __copy_from_user(to, from, n) copy_from_user(to, from, n)
#define __copy_to_user(to, from, n) copy_to_user(to, from, n)
#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

#define copy_to_user_ret(to,from,n,retval) ({ if (copy_to_user(to,from,n)) return retval; })

#define copy_from_user_ret(to,from,n,retval) ({ if (copy_from_user(to,from,n)) return retval; })


static inline long
strncpy_from_user(char *dst, const char *src, long count)
{
	char *tmp;
	strncpy(dst, src, count);
	for (tmp = dst; *tmp && count > 0; tmp++, count--)
		;
	return(tmp - dst); 
}

static inline long strnlen_user(const char *src, long n)
{
	return(strlen(src) + 1); 
}

#define strlen_user(str) strnlen_user(str, 32767)


static inline unsigned long
clear_user(void *to, unsigned long n)
{
	memset(to, 0, n);
	return 0;
}

#endif 
