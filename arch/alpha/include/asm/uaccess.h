#ifndef __ALPHA_UACCESS_H
#define __ALPHA_UACCESS_H

#include <linux/errno.h>
#include <linux/sched.h>



#define KERNEL_DS	((mm_segment_t) { 0UL })
#define USER_DS		((mm_segment_t) { -0x40000000000UL })

#define VERIFY_READ	0
#define VERIFY_WRITE	1

#define get_fs()  (current_thread_info()->addr_limit)
#define get_ds()  (KERNEL_DS)
#define set_fs(x) (current_thread_info()->addr_limit = (x))

#define segment_eq(a,b)	((a).seg == (b).seg)

#define __access_ok(addr,size,segment) \
	(((segment).seg & (addr | size | (addr+size))) == 0)

#define access_ok(type,addr,size)				\
({								\
	__chk_user_ptr(addr);					\
	__access_ok(((unsigned long)(addr)),(size),get_fs());	\
})

#define put_user(x,ptr) \
  __put_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)),get_fs())
#define get_user(x,ptr) \
  __get_user_check((x),(ptr),sizeof(*(ptr)),get_fs())

#define __put_user(x,ptr) \
  __put_user_nocheck((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))
#define __get_user(x,ptr) \
  __get_user_nocheck((x),(ptr),sizeof(*(ptr)))
  

extern void __get_user_unknown(void);

#define __get_user_nocheck(x,ptr,size)				\
({								\
	long __gu_err = 0;					\
	unsigned long __gu_val;					\
	__chk_user_ptr(ptr);					\
	switch (size) {						\
	  case 1: __get_user_8(ptr); break;			\
	  case 2: __get_user_16(ptr); break;			\
	  case 4: __get_user_32(ptr); break;			\
	  case 8: __get_user_64(ptr); break;			\
	  default: __get_user_unknown(); break;			\
	}							\
	(x) = (__typeof__(*(ptr))) __gu_val;			\
	__gu_err;						\
})

#define __get_user_check(x,ptr,size,segment)				\
({									\
	long __gu_err = -EFAULT;					\
	unsigned long __gu_val = 0;					\
	const __typeof__(*(ptr)) __user *__gu_addr = (ptr);		\
	if (__access_ok((unsigned long)__gu_addr,size,segment)) {	\
		__gu_err = 0;						\
		switch (size) {						\
		  case 1: __get_user_8(__gu_addr); break;		\
		  case 2: __get_user_16(__gu_addr); break;		\
		  case 4: __get_user_32(__gu_addr); break;		\
		  case 8: __get_user_64(__gu_addr); break;		\
		  default: __get_user_unknown(); break;			\
		}							\
	}								\
	(x) = (__typeof__(*(ptr))) __gu_val;				\
	__gu_err;							\
})

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct __user *)(x))

#define __get_user_64(addr)				\
	__asm__("1: ldq %0,%2\n"			\
	"2:\n"						\
	".section __ex_table,\"a\"\n"			\
	"	.long 1b - .\n"				\
	"	lda %0, 2b-1b(%1)\n"			\
	".previous"					\
		: "=r"(__gu_val), "=r"(__gu_err)	\
		: "m"(__m(addr)), "1"(__gu_err))

#define __get_user_32(addr)				\
	__asm__("1: ldl %0,%2\n"			\
	"2:\n"						\
	".section __ex_table,\"a\"\n"			\
	"	.long 1b - .\n"				\
	"	lda %0, 2b-1b(%1)\n"			\
	".previous"					\
		: "=r"(__gu_val), "=r"(__gu_err)	\
		: "m"(__m(addr)), "1"(__gu_err))

#ifdef __alpha_bwx__

#define __get_user_16(addr)				\
	__asm__("1: ldwu %0,%2\n"			\
	"2:\n"						\
	".section __ex_table,\"a\"\n"			\
	"	.long 1b - .\n"				\
	"	lda %0, 2b-1b(%1)\n"			\
	".previous"					\
		: "=r"(__gu_val), "=r"(__gu_err)	\
		: "m"(__m(addr)), "1"(__gu_err))

#define __get_user_8(addr)				\
	__asm__("1: ldbu %0,%2\n"			\
	"2:\n"						\
	".section __ex_table,\"a\"\n"			\
	"	.long 1b - .\n"				\
	"	lda %0, 2b-1b(%1)\n"			\
	".previous"					\
		: "=r"(__gu_val), "=r"(__gu_err)	\
		: "m"(__m(addr)), "1"(__gu_err))
#else

#define __get_user_16(addr)						\
{									\
	long __gu_tmp;							\
	__asm__("1: ldq_u %0,0(%3)\n"					\
	"2:	ldq_u %1,1(%3)\n"					\
	"	extwl %0,%3,%0\n"					\
	"	extwh %1,%3,%1\n"					\
	"	or %0,%1,%0\n"						\
	"3:\n"								\
	".section __ex_table,\"a\"\n"					\
	"	.long 1b - .\n"						\
	"	lda %0, 3b-1b(%2)\n"					\
	"	.long 2b - .\n"						\
	"	lda %0, 3b-2b(%2)\n"					\
	".previous"							\
		: "=&r"(__gu_val), "=&r"(__gu_tmp), "=r"(__gu_err)	\
		: "r"(addr), "2"(__gu_err));				\
}

#define __get_user_8(addr)						\
	__asm__("1: ldq_u %0,0(%2)\n"					\
	"	extbl %0,%2,%0\n"					\
	"2:\n"								\
	".section __ex_table,\"a\"\n"					\
	"	.long 1b - .\n"						\
	"	lda %0, 2b-1b(%1)\n"					\
	".previous"							\
		: "=&r"(__gu_val), "=r"(__gu_err)			\
		: "r"(addr), "1"(__gu_err))
#endif

extern void __put_user_unknown(void);

#define __put_user_nocheck(x,ptr,size)				\
({								\
	long __pu_err = 0;					\
	__chk_user_ptr(ptr);					\
	switch (size) {						\
	  case 1: __put_user_8(x,ptr); break;			\
	  case 2: __put_user_16(x,ptr); break;			\
	  case 4: __put_user_32(x,ptr); break;			\
	  case 8: __put_user_64(x,ptr); break;			\
	  default: __put_user_unknown(); break;			\
	}							\
	__pu_err;						\
})

#define __put_user_check(x,ptr,size,segment)				\
({									\
	long __pu_err = -EFAULT;					\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);			\
	if (__access_ok((unsigned long)__pu_addr,size,segment)) {	\
		__pu_err = 0;						\
		switch (size) {						\
		  case 1: __put_user_8(x,__pu_addr); break;		\
		  case 2: __put_user_16(x,__pu_addr); break;		\
		  case 4: __put_user_32(x,__pu_addr); break;		\
		  case 8: __put_user_64(x,__pu_addr); break;		\
		  default: __put_user_unknown(); break;			\
		}							\
	}								\
	__pu_err;							\
})

#define __put_user_64(x,addr)					\
__asm__ __volatile__("1: stq %r2,%1\n"				\
	"2:\n"							\
	".section __ex_table,\"a\"\n"				\
	"	.long 1b - .\n"					\
	"	lda $31,2b-1b(%0)\n"				\
	".previous"						\
		: "=r"(__pu_err)				\
		: "m" (__m(addr)), "rJ" (x), "0"(__pu_err))

#define __put_user_32(x,addr)					\
__asm__ __volatile__("1: stl %r2,%1\n"				\
	"2:\n"							\
	".section __ex_table,\"a\"\n"				\
	"	.long 1b - .\n"					\
	"	lda $31,2b-1b(%0)\n"				\
	".previous"						\
		: "=r"(__pu_err)				\
		: "m"(__m(addr)), "rJ"(x), "0"(__pu_err))

#ifdef __alpha_bwx__

#define __put_user_16(x,addr)					\
__asm__ __volatile__("1: stw %r2,%1\n"				\
	"2:\n"							\
	".section __ex_table,\"a\"\n"				\
	"	.long 1b - .\n"					\
	"	lda $31,2b-1b(%0)\n"				\
	".previous"						\
		: "=r"(__pu_err)				\
		: "m"(__m(addr)), "rJ"(x), "0"(__pu_err))

#define __put_user_8(x,addr)					\
__asm__ __volatile__("1: stb %r2,%1\n"				\
	"2:\n"							\
	".section __ex_table,\"a\"\n"				\
	"	.long 1b - .\n"					\
	"	lda $31,2b-1b(%0)\n"				\
	".previous"						\
		: "=r"(__pu_err)				\
		: "m"(__m(addr)), "rJ"(x), "0"(__pu_err))
#else

#define __put_user_16(x,addr)					\
{								\
	long __pu_tmp1, __pu_tmp2, __pu_tmp3, __pu_tmp4;	\
	__asm__ __volatile__(					\
	"1:	ldq_u %2,1(%5)\n"				\
	"2:	ldq_u %1,0(%5)\n"				\
	"	inswh %6,%5,%4\n"				\
	"	inswl %6,%5,%3\n"				\
	"	mskwh %2,%5,%2\n"				\
	"	mskwl %1,%5,%1\n"				\
	"	or %2,%4,%2\n"					\
	"	or %1,%3,%1\n"					\
	"3:	stq_u %2,1(%5)\n"				\
	"4:	stq_u %1,0(%5)\n"				\
	"5:\n"							\
	".section __ex_table,\"a\"\n"				\
	"	.long 1b - .\n"					\
	"	lda $31, 5b-1b(%0)\n"				\
	"	.long 2b - .\n"					\
	"	lda $31, 5b-2b(%0)\n"				\
	"	.long 3b - .\n"					\
	"	lda $31, 5b-3b(%0)\n"				\
	"	.long 4b - .\n"					\
	"	lda $31, 5b-4b(%0)\n"				\
	".previous"						\
		: "=r"(__pu_err), "=&r"(__pu_tmp1),		\
		  "=&r"(__pu_tmp2), "=&r"(__pu_tmp3),		\
		  "=&r"(__pu_tmp4)				\
		: "r"(addr), "r"((unsigned long)(x)), "0"(__pu_err)); \
}

#define __put_user_8(x,addr)					\
{								\
	long __pu_tmp1, __pu_tmp2;				\
	__asm__ __volatile__(					\
	"1:	ldq_u %1,0(%4)\n"				\
	"	insbl %3,%4,%2\n"				\
	"	mskbl %1,%4,%1\n"				\
	"	or %1,%2,%1\n"					\
	"2:	stq_u %1,0(%4)\n"				\
	"3:\n"							\
	".section __ex_table,\"a\"\n"				\
	"	.long 1b - .\n"					\
	"	lda $31, 3b-1b(%0)\n"				\
	"	.long 2b - .\n"					\
	"	lda $31, 3b-2b(%0)\n"				\
	".previous"						\
		: "=r"(__pu_err),				\
	  	  "=&r"(__pu_tmp1), "=&r"(__pu_tmp2)		\
		: "r"((unsigned long)(x)), "r"(addr), "0"(__pu_err)); \
}
#endif



#ifdef MODULE
#define __module_address(sym)		"r"(sym),
#define __module_call(ra, arg, sym)	"jsr $" #ra ",(%" #arg ")," #sym
#else
#define __module_address(sym)
#define __module_call(ra, arg, sym)	"bsr $" #ra "," #sym " !samegp"
#endif

extern void __copy_user(void);

extern inline long
__copy_tofrom_user_nocheck(void *to, const void *from, long len)
{
	register void * __cu_to __asm__("$6") = to;
	register const void * __cu_from __asm__("$7") = from;
	register long __cu_len __asm__("$0") = len;

	__asm__ __volatile__(
		__module_call(28, 3, __copy_user)
		: "=r" (__cu_len), "=r" (__cu_from), "=r" (__cu_to)
		: __module_address(__copy_user)
		  "0" (__cu_len), "1" (__cu_from), "2" (__cu_to)
		: "$1","$2","$3","$4","$5","$28","memory");

	return __cu_len;
}

extern inline long
__copy_tofrom_user(void *to, const void *from, long len, const void __user *validate)
{
	if (__access_ok((unsigned long)validate, len, get_fs()))
		len = __copy_tofrom_user_nocheck(to, from, len);
	return len;
}

#define __copy_to_user(to,from,n)					\
({									\
	__chk_user_ptr(to);						\
	__copy_tofrom_user_nocheck((__force void *)(to),(from),(n));	\
})
#define __copy_from_user(to,from,n)					\
({									\
	__chk_user_ptr(from);						\
	__copy_tofrom_user_nocheck((to),(__force void *)(from),(n));	\
})

#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user


extern inline long
copy_to_user(void __user *to, const void *from, long n)
{
	return __copy_tofrom_user((__force void *)to, from, n, to);
}

extern inline long
copy_from_user(void *to, const void __user *from, long n)
{
	return __copy_tofrom_user(to, (__force void *)from, n, from);
}

extern void __do_clear_user(void);

extern inline long
__clear_user(void __user *to, long len)
{
	register void __user * __cl_to __asm__("$6") = to;
	register long __cl_len __asm__("$0") = len;
	__asm__ __volatile__(
		__module_call(28, 2, __do_clear_user)
		: "=r"(__cl_len), "=r"(__cl_to)
		: __module_address(__do_clear_user)
		  "0"(__cl_len), "1"(__cl_to)
		: "$1","$2","$3","$4","$5","$28","memory");
	return __cl_len;
}

extern inline long
clear_user(void __user *to, long len)
{
	if (__access_ok((unsigned long)to, len, get_fs()))
		len = __clear_user(to, len);
	return len;
}

#undef __module_address
#undef __module_call


extern long __strncpy_from_user(char *__to, const char __user *__from, long __to_len);

extern inline long
strncpy_from_user(char *to, const char __user *from, long n)
{
	long ret = -EFAULT;
	if (__access_ok((unsigned long)from, 0, get_fs()))
		ret = __strncpy_from_user(to, from, n);
	return ret;
}

extern long __strlen_user(const char __user *);

extern inline long strlen_user(const char __user *str)
{
	return access_ok(VERIFY_READ,str,0) ? __strlen_user(str) : 0;
}

extern long __strnlen_user(const char __user *, long);

extern inline long strnlen_user(const char __user *str, long n)
{
	return access_ok(VERIFY_READ,str,0) ? __strnlen_user(str, n) : 0;
}


struct exception_table_entry
{
	signed int insn;
	union exception_fixup {
		unsigned unit;
		struct {
			signed int nextinsn : 16;
			unsigned int errreg : 5;
			unsigned int valreg : 5;
		} bits;
	} fixup;
};

#define fixup_exception(map_reg, _fixup, pc)			\
({								\
	if ((_fixup)->fixup.bits.valreg != 31)			\
		map_reg((_fixup)->fixup.bits.valreg) = 0;	\
	if ((_fixup)->fixup.bits.errreg != 31)			\
		map_reg((_fixup)->fixup.bits.errreg) = -EFAULT;	\
	(pc) + (_fixup)->fixup.bits.nextinsn;			\
})

#define ARCH_HAS_SORT_EXTABLE
#define ARCH_HAS_SEARCH_EXTABLE

#endif 
