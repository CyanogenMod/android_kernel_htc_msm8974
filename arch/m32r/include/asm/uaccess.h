#ifndef _ASM_M32R_UACCESS_H
#define _ASM_M32R_UACCESS_H

/*
 *  linux/include/asm-m32r/uaccess.h
 *
 *  M32R version.
 *    Copyright (C) 2004, 2006  Hirokazu Takata <takata at linux-m32r.org>
 */

#include <linux/errno.h>
#include <linux/thread_info.h>
#include <asm/page.h>
#include <asm/setup.h>

#define VERIFY_READ 0
#define VERIFY_WRITE 1


#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })

#ifdef CONFIG_MMU

#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFF)
#define USER_DS		MAKE_MM_SEG(PAGE_OFFSET)
#define get_ds()	(KERNEL_DS)
#define get_fs()	(current_thread_info()->addr_limit)
#define set_fs(x)	(current_thread_info()->addr_limit = (x))

#else 

#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFF)
#define USER_DS		MAKE_MM_SEG(0xFFFFFFFF)
#define get_ds()	(KERNEL_DS)

static inline mm_segment_t get_fs(void)
{
	return USER_DS;
}

static inline void set_fs(mm_segment_t s)
{
}

#endif 

#define segment_eq(a,b)	((a).seg == (b).seg)

#define __addr_ok(addr) \
	((unsigned long)(addr) < (current_thread_info()->addr_limit.seg))

#define __range_ok(addr,size) ({					\
	unsigned long flag, roksum; 					\
	__chk_user_ptr(addr);						\
	asm ( 								\
		"	cmpu	%1, %1    ; clear cbit\n"		\
		"	addx	%1, %3    ; set cbit if overflow\n"	\
		"	subx	%0, %0\n"				\
		"	cmpu	%4, %1\n"				\
		"	subx	%0, %5\n"				\
		: "=&r" (flag), "=r" (roksum)				\
		: "1" (addr), "r" ((int)(size)), 			\
		  "r" (current_thread_info()->addr_limit.seg), "r" (0)	\
		: "cbit" );						\
	flag; })

#ifdef CONFIG_MMU
#define access_ok(type,addr,size) (likely(__range_ok(addr,size) == 0))
#else
static inline int access_ok(int type, const void *addr, unsigned long size)
{
	unsigned long val = (unsigned long)addr;

	return ((val >= memory_start) && ((val + size) < memory_end));
}
#endif 


struct exception_table_entry
{
	unsigned long insn, fixup;
};

extern int fixup_exception(struct pt_regs *regs);


#define get_user(x,ptr)							\
	__get_user_check((x),(ptr),sizeof(*(ptr)))

#define put_user(x,ptr)							\
	__put_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

#define __get_user(x,ptr) \
	__get_user_nocheck((x),(ptr),sizeof(*(ptr)))

#define __get_user_nocheck(x,ptr,size)					\
({									\
	long __gu_err = 0;						\
	unsigned long __gu_val;						\
	might_sleep();							\
	__get_user_size(__gu_val,(ptr),(size),__gu_err);		\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})

#define __get_user_check(x,ptr,size)					\
({									\
	long __gu_err = -EFAULT;					\
	unsigned long __gu_val = 0;					\
	const __typeof__(*(ptr)) __user *__gu_addr = (ptr);		\
	might_sleep();							\
	if (access_ok(VERIFY_READ,__gu_addr,size))			\
		__get_user_size(__gu_val,__gu_addr,(size),__gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})

extern long __get_user_bad(void);

#define __get_user_size(x,ptr,size,retval)				\
do {									\
	retval = 0;							\
	__chk_user_ptr(ptr);						\
	switch (size) {							\
	  case 1: __get_user_asm(x,ptr,retval,"ub"); break;		\
	  case 2: __get_user_asm(x,ptr,retval,"uh"); break;		\
	  case 4: __get_user_asm(x,ptr,retval,""); break;		\
	  default: (x) = __get_user_bad();				\
	}								\
} while (0)

#define __get_user_asm(x, addr, err, itype)				\
	__asm__ __volatile__(						\
		"	.fillinsn\n"					\
		"1:	ld"itype" %1,@%2\n"				\
		"	.fillinsn\n"					\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"	.balign 4\n"					\
		"3:	ldi %0,%3\n"					\
		"	seth r14,#high(2b)\n"				\
		"	or3 r14,r14,#low(2b)\n"				\
		"	jmp r14\n"					\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		"	.balign 4\n"					\
		"	.long 1b,3b\n"					\
		".previous"						\
		: "=&r" (err), "=&r" (x)				\
		: "r" (addr), "i" (-EFAULT), "0" (err)			\
		: "r14", "memory")

#define __put_user(x,ptr) \
	__put_user_nocheck((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))


#define __put_user_nocheck(x,ptr,size)					\
({									\
	long __pu_err;							\
	might_sleep();							\
	__put_user_size((x),(ptr),(size),__pu_err);			\
	__pu_err;							\
})


#define __put_user_check(x,ptr,size)					\
({									\
	long __pu_err = -EFAULT;					\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);			\
	might_sleep();							\
	if (access_ok(VERIFY_WRITE,__pu_addr,size))			\
		__put_user_size((x),__pu_addr,(size),__pu_err);		\
	__pu_err;							\
})

#if defined(__LITTLE_ENDIAN__)
#define __put_user_u64(x, addr, err)					\
        __asm__ __volatile__(						\
                "       .fillinsn\n"					\
                "1:     st %L1,@%2\n"					\
                "       .fillinsn\n"					\
                "2:     st %H1,@(4,%2)\n"				\
                "       .fillinsn\n"					\
                "3:\n"							\
                ".section .fixup,\"ax\"\n"				\
                "       .balign 4\n"					\
                "4:     ldi %0,%3\n"					\
                "       seth r14,#high(3b)\n"				\
                "       or3 r14,r14,#low(3b)\n"				\
                "       jmp r14\n"					\
                ".previous\n"						\
                ".section __ex_table,\"a\"\n"				\
                "       .balign 4\n"					\
                "       .long 1b,4b\n"					\
                "       .long 2b,4b\n"					\
                ".previous"						\
                : "=&r" (err)						\
                : "r" (x), "r" (addr), "i" (-EFAULT), "0" (err)		\
                : "r14", "memory")

#elif defined(__BIG_ENDIAN__)
#define __put_user_u64(x, addr, err)					\
	__asm__ __volatile__(						\
		"	.fillinsn\n"					\
		"1:	st %H1,@%2\n"					\
		"	.fillinsn\n"					\
		"2:	st %L1,@(4,%2)\n"				\
		"	.fillinsn\n"					\
		"3:\n"							\
		".section .fixup,\"ax\"\n"				\
		"	.balign 4\n"					\
		"4:	ldi %0,%3\n"					\
		"	seth r14,#high(3b)\n"				\
		"	or3 r14,r14,#low(3b)\n"				\
		"	jmp r14\n"					\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		"	.balign 4\n"					\
		"	.long 1b,4b\n"					\
		"	.long 2b,4b\n"					\
		".previous"						\
		: "=&r" (err)						\
		: "r" (x), "r" (addr), "i" (-EFAULT), "0" (err)		\
		: "r14", "memory")
#else
#error no endian defined
#endif

extern void __put_user_bad(void);

#define __put_user_size(x,ptr,size,retval)				\
do {									\
	retval = 0;							\
	__chk_user_ptr(ptr);						\
	switch (size) {							\
	  case 1: __put_user_asm(x,ptr,retval,"b"); break;		\
	  case 2: __put_user_asm(x,ptr,retval,"h"); break;		\
	  case 4: __put_user_asm(x,ptr,retval,""); break;		\
	  case 8: __put_user_u64((__typeof__(*ptr))(x),ptr,retval); break;\
	  default: __put_user_bad();					\
	}								\
} while (0)

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct *)(x))

#define __put_user_asm(x, addr, err, itype)				\
	__asm__ __volatile__(						\
		"	.fillinsn\n"					\
		"1:	st"itype" %1,@%2\n"				\
		"	.fillinsn\n"					\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"	.balign 4\n"					\
		"3:	ldi %0,%3\n"					\
		"	seth r14,#high(2b)\n"				\
		"	or3 r14,r14,#low(2b)\n"				\
		"	jmp r14\n"					\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		"	.balign 4\n"					\
		"	.long 1b,3b\n"					\
		".previous"						\
		: "=&r" (err)						\
		: "r" (x), "r" (addr), "i" (-EFAULT), "0" (err)		\
		: "r14", "memory")



#define __copy_user(to,from,size)					\
do {									\
	unsigned long __dst, __src, __c;				\
	__asm__ __volatile__ (						\
		"	mv	r14, %0\n"				\
		"	or	r14, %1\n"				\
		"	beq	%0, %1, 9f\n"				\
		"	beqz	%2, 9f\n"				\
		"	and3	r14, r14, #3\n"				\
		"	bnez	r14, 2f\n"				\
		"	and3	%2, %2, #3\n"				\
		"	beqz	%3, 2f\n"				\
		"	addi	%0, #-4		; word_copy \n"		\
		"	.fillinsn\n"					\
		"0:	ld	r14, @%1+\n"				\
		"	addi	%3, #-1\n"				\
		"	.fillinsn\n"					\
		"1:	st	r14, @+%0\n"				\
		"	bnez	%3, 0b\n"				\
		"	beqz	%2, 9f\n"				\
		"	addi	%0, #4\n"				\
		"	.fillinsn\n"					\
		"2:	ldb	r14, @%1	; byte_copy \n"		\
		"	.fillinsn\n"					\
		"3:	stb	r14, @%0\n"				\
		"	addi	%1, #1\n"				\
		"	addi	%2, #-1\n"				\
		"	addi	%0, #1\n"				\
		"	bnez	%2, 2b\n"				\
		"	.fillinsn\n"					\
		"9:\n"							\
		".section .fixup,\"ax\"\n"				\
		"	.balign 4\n"					\
		"5:	addi	%3, #1\n"				\
		"	addi	%1, #-4\n"				\
		"	.fillinsn\n"					\
		"6:	slli	%3, #2\n"				\
		"	add	%2, %3\n"				\
		"	addi	%0, #4\n"				\
		"	.fillinsn\n"					\
		"7:	seth	r14, #high(9b)\n"			\
		"	or3	r14, r14, #low(9b)\n"			\
		"	jmp	r14\n"					\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		"	.balign 4\n"					\
		"	.long 0b,6b\n"					\
		"	.long 1b,5b\n"					\
		"	.long 2b,9b\n"					\
		"	.long 3b,9b\n"					\
		".previous\n"						\
		: "=&r" (__dst), "=&r" (__src), "=&r" (size),		\
		  "=&r" (__c)						\
		: "0" (to), "1" (from), "2" (size), "3" (size / 4)	\
		: "r14", "memory");					\
} while (0)

#define __copy_user_zeroing(to,from,size)				\
do {									\
	unsigned long __dst, __src, __c;				\
	__asm__ __volatile__ (						\
		"	mv	r14, %0\n"				\
		"	or	r14, %1\n"				\
		"	beq	%0, %1, 9f\n"				\
		"	beqz	%2, 9f\n"				\
		"	and3	r14, r14, #3\n"				\
		"	bnez	r14, 2f\n"				\
		"	and3	%2, %2, #3\n"				\
		"	beqz	%3, 2f\n"				\
		"	addi	%0, #-4		; word_copy \n"		\
		"	.fillinsn\n"					\
		"0:	ld	r14, @%1+\n"				\
		"	addi	%3, #-1\n"				\
		"	.fillinsn\n"					\
		"1:	st	r14, @+%0\n"				\
		"	bnez	%3, 0b\n"				\
		"	beqz	%2, 9f\n"				\
		"	addi	%0, #4\n"				\
		"	.fillinsn\n"					\
		"2:	ldb	r14, @%1	; byte_copy \n"		\
		"	.fillinsn\n"					\
		"3:	stb	r14, @%0\n"				\
		"	addi	%1, #1\n"				\
		"	addi	%2, #-1\n"				\
		"	addi	%0, #1\n"				\
		"	bnez	%2, 2b\n"				\
		"	.fillinsn\n"					\
		"9:\n"							\
		".section .fixup,\"ax\"\n"				\
		"	.balign 4\n"					\
		"5:	addi	%3, #1\n"				\
		"	addi	%1, #-4\n"				\
		"	.fillinsn\n"					\
		"6:	slli	%3, #2\n"				\
		"	add	%2, %3\n"				\
		"	addi	%0, #4\n"				\
		"	.fillinsn\n"					\
		"7:	ldi	r14, #0		; store zero \n"	\
		"	.fillinsn\n"					\
		"8:	addi	%2, #-1\n"				\
		"	stb	r14, @%0	; ACE? \n"		\
		"	addi	%0, #1\n"				\
		"	bnez	%2, 8b\n"				\
		"	seth	r14, #high(9b)\n"			\
		"	or3	r14, r14, #low(9b)\n"			\
		"	jmp	r14\n"					\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		"	.balign 4\n"					\
		"	.long 0b,6b\n"					\
		"	.long 1b,5b\n"					\
		"	.long 2b,7b\n"					\
		"	.long 3b,7b\n"					\
		".previous\n"						\
		: "=&r" (__dst), "=&r" (__src), "=&r" (size),		\
		  "=&r" (__c)						\
		: "0" (to), "1" (from), "2" (size), "3" (size / 4)	\
		: "r14", "memory");					\
} while (0)


static inline unsigned long __generic_copy_from_user_nocheck(void *to,
	const void __user *from, unsigned long n)
{
	__copy_user_zeroing(to,from,n);
	return n;
}

static inline unsigned long __generic_copy_to_user_nocheck(void __user *to,
	const void *from, unsigned long n)
{
	__copy_user(to,from,n);
	return n;
}

unsigned long __generic_copy_to_user(void __user *, const void *, unsigned long);
unsigned long __generic_copy_from_user(void *, const void __user *, unsigned long);

#define __copy_to_user(to,from,n)			\
	__generic_copy_to_user_nocheck((to),(from),(n))

#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

#define copy_to_user(to,from,n)				\
({							\
	might_sleep();					\
	__generic_copy_to_user((to),(from),(n));	\
})

#define __copy_from_user(to,from,n)			\
	__generic_copy_from_user_nocheck((to),(from),(n))

#define copy_from_user(to,from,n)			\
({							\
	might_sleep();					\
	__generic_copy_from_user((to),(from),(n));	\
})

long __must_check strncpy_from_user(char *dst, const char __user *src,
				long count);
long __must_check __strncpy_from_user(char *dst,
				const char __user *src, long count);

unsigned long __clear_user(void __user *mem, unsigned long len);

unsigned long clear_user(void __user *mem, unsigned long len);

#define strlen_user(str) strnlen_user(str, ~0UL >> 1)
long strnlen_user(const char __user *str, long n);

#endif 
