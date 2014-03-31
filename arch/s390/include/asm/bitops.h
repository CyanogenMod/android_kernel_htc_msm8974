#ifndef _S390_BITOPS_H
#define _S390_BITOPS_H

/*
 *  include/asm-s390/bitops.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "include/asm-i386/bitops.h"
 *    Copyright (C) 1992, Linus Torvalds
 *
 */

#ifdef __KERNEL__

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <linux/compiler.h>


extern const char _oi_bitmap[];
extern const char _ni_bitmap[];
extern const char _zb_findmap[];
extern const char _sb_findmap[];

#ifndef __s390x__

#define __BITOPS_ALIGN		3
#define __BITOPS_WORDSIZE	32
#define __BITOPS_OR		"or"
#define __BITOPS_AND		"nr"
#define __BITOPS_XOR		"xr"

#define __BITOPS_LOOP(__old, __new, __addr, __val, __op_string)	\
	asm volatile(						\
		"	l	%0,%2\n"			\
		"0:	lr	%1,%0\n"			\
		__op_string "	%1,%3\n"			\
		"	cs	%0,%1,%2\n"			\
		"	jl	0b"				\
		: "=&d" (__old), "=&d" (__new),			\
		  "=Q" (*(unsigned long *) __addr)		\
		: "d" (__val), "Q" (*(unsigned long *) __addr)	\
		: "cc");

#else 

#define __BITOPS_ALIGN		7
#define __BITOPS_WORDSIZE	64
#define __BITOPS_OR		"ogr"
#define __BITOPS_AND		"ngr"
#define __BITOPS_XOR		"xgr"

#define __BITOPS_LOOP(__old, __new, __addr, __val, __op_string)	\
	asm volatile(						\
		"	lg	%0,%2\n"			\
		"0:	lgr	%1,%0\n"			\
		__op_string "	%1,%3\n"			\
		"	csg	%0,%1,%2\n"			\
		"	jl	0b"				\
		: "=&d" (__old), "=&d" (__new),			\
		  "=Q" (*(unsigned long *) __addr)		\
		: "d" (__val), "Q" (*(unsigned long *) __addr)	\
		: "cc");

#endif 

#define __BITOPS_WORDS(bits) (((bits)+__BITOPS_WORDSIZE-1)/__BITOPS_WORDSIZE)
#define __BITOPS_BARRIER() asm volatile("" : : : "memory")

#ifdef CONFIG_SMP
static inline void set_bit_cs(unsigned long nr, volatile unsigned long *ptr)
{
        unsigned long addr, old, new, mask;

	addr = (unsigned long) ptr;
	
	addr += (nr ^ (nr & (__BITOPS_WORDSIZE - 1))) >> 3;
	
	mask = 1UL << (nr & (__BITOPS_WORDSIZE - 1));
	
	__BITOPS_LOOP(old, new, addr, mask, __BITOPS_OR);
}

static inline void clear_bit_cs(unsigned long nr, volatile unsigned long *ptr)
{
        unsigned long addr, old, new, mask;

	addr = (unsigned long) ptr;
	
	addr += (nr ^ (nr & (__BITOPS_WORDSIZE - 1))) >> 3;
	
	mask = ~(1UL << (nr & (__BITOPS_WORDSIZE - 1)));
	
	__BITOPS_LOOP(old, new, addr, mask, __BITOPS_AND);
}

static inline void change_bit_cs(unsigned long nr, volatile unsigned long *ptr)
{
        unsigned long addr, old, new, mask;

	addr = (unsigned long) ptr;
	
	addr += (nr ^ (nr & (__BITOPS_WORDSIZE - 1))) >> 3;
	
	mask = 1UL << (nr & (__BITOPS_WORDSIZE - 1));
	
	__BITOPS_LOOP(old, new, addr, mask, __BITOPS_XOR);
}

static inline int
test_and_set_bit_cs(unsigned long nr, volatile unsigned long *ptr)
{
        unsigned long addr, old, new, mask;

	addr = (unsigned long) ptr;
	
	addr += (nr ^ (nr & (__BITOPS_WORDSIZE - 1))) >> 3;
	
	mask = 1UL << (nr & (__BITOPS_WORDSIZE - 1));
	
	__BITOPS_LOOP(old, new, addr, mask, __BITOPS_OR);
	__BITOPS_BARRIER();
	return (old & mask) != 0;
}

static inline int
test_and_clear_bit_cs(unsigned long nr, volatile unsigned long *ptr)
{
        unsigned long addr, old, new, mask;

	addr = (unsigned long) ptr;
	
	addr += (nr ^ (nr & (__BITOPS_WORDSIZE - 1))) >> 3;
	
	mask = ~(1UL << (nr & (__BITOPS_WORDSIZE - 1)));
	
	__BITOPS_LOOP(old, new, addr, mask, __BITOPS_AND);
	__BITOPS_BARRIER();
	return (old ^ new) != 0;
}

static inline int
test_and_change_bit_cs(unsigned long nr, volatile unsigned long *ptr)
{
        unsigned long addr, old, new, mask;

	addr = (unsigned long) ptr;
	
	addr += (nr ^ (nr & (__BITOPS_WORDSIZE - 1))) >> 3;
	
	mask = 1UL << (nr & (__BITOPS_WORDSIZE - 1));
	
	__BITOPS_LOOP(old, new, addr, mask, __BITOPS_XOR);
	__BITOPS_BARRIER();
	return (old & mask) != 0;
}
#endif 

static inline void __set_bit(unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long addr;

	addr = (unsigned long) ptr + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	asm volatile(
		"	oc	%O0(1,%R0),%1"
		: "=Q" (*(char *) addr) : "Q" (_oi_bitmap[nr & 7]) : "cc" );
}

static inline void 
__constant_set_bit(const unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long addr;

	addr = ((unsigned long) ptr) + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	*(unsigned char *) addr |= 1 << (nr & 7);
}

#define set_bit_simple(nr,addr) \
(__builtin_constant_p((nr)) ? \
 __constant_set_bit((nr),(addr)) : \
 __set_bit((nr),(addr)) )

static inline void 
__clear_bit(unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long addr;

	addr = (unsigned long) ptr + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	asm volatile(
		"	nc	%O0(1,%R0),%1"
		: "=Q" (*(char *) addr) : "Q" (_ni_bitmap[nr & 7]) : "cc" );
}

static inline void 
__constant_clear_bit(const unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long addr;

	addr = ((unsigned long) ptr) + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	*(unsigned char *) addr &= ~(1 << (nr & 7));
}

#define clear_bit_simple(nr,addr) \
(__builtin_constant_p((nr)) ? \
 __constant_clear_bit((nr),(addr)) : \
 __clear_bit((nr),(addr)) )

static inline void __change_bit(unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long addr;

	addr = (unsigned long) ptr + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	asm volatile(
		"	xc	%O0(1,%R0),%1"
		: "=Q" (*(char *) addr) : "Q" (_oi_bitmap[nr & 7]) : "cc" );
}

static inline void 
__constant_change_bit(const unsigned long nr, volatile unsigned long *ptr) 
{
	unsigned long addr;

	addr = ((unsigned long) ptr) + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	*(unsigned char *) addr ^= 1 << (nr & 7);
}

#define change_bit_simple(nr,addr) \
(__builtin_constant_p((nr)) ? \
 __constant_change_bit((nr),(addr)) : \
 __change_bit((nr),(addr)) )

static inline int
test_and_set_bit_simple(unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long addr;
	unsigned char ch;

	addr = (unsigned long) ptr + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	ch = *(unsigned char *) addr;
	asm volatile(
		"	oc	%O0(1,%R0),%1"
		: "=Q" (*(char *) addr)	: "Q" (_oi_bitmap[nr & 7])
		: "cc", "memory");
	return (ch >> (nr & 7)) & 1;
}
#define __test_and_set_bit(X,Y)		test_and_set_bit_simple(X,Y)

static inline int
test_and_clear_bit_simple(unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long addr;
	unsigned char ch;

	addr = (unsigned long) ptr + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	ch = *(unsigned char *) addr;
	asm volatile(
		"	nc	%O0(1,%R0),%1"
		: "=Q" (*(char *) addr)	: "Q" (_ni_bitmap[nr & 7])
		: "cc", "memory");
	return (ch >> (nr & 7)) & 1;
}
#define __test_and_clear_bit(X,Y)	test_and_clear_bit_simple(X,Y)

static inline int
test_and_change_bit_simple(unsigned long nr, volatile unsigned long *ptr)
{
	unsigned long addr;
	unsigned char ch;

	addr = (unsigned long) ptr + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	ch = *(unsigned char *) addr;
	asm volatile(
		"	xc	%O0(1,%R0),%1"
		: "=Q" (*(char *) addr)	: "Q" (_oi_bitmap[nr & 7])
		: "cc", "memory");
	return (ch >> (nr & 7)) & 1;
}
#define __test_and_change_bit(X,Y)	test_and_change_bit_simple(X,Y)

#ifdef CONFIG_SMP
#define set_bit             set_bit_cs
#define clear_bit           clear_bit_cs
#define change_bit          change_bit_cs
#define test_and_set_bit    test_and_set_bit_cs
#define test_and_clear_bit  test_and_clear_bit_cs
#define test_and_change_bit test_and_change_bit_cs
#else
#define set_bit             set_bit_simple
#define clear_bit           clear_bit_simple
#define change_bit          change_bit_simple
#define test_and_set_bit    test_and_set_bit_simple
#define test_and_clear_bit  test_and_clear_bit_simple
#define test_and_change_bit test_and_change_bit_simple
#endif



static inline int __test_bit(unsigned long nr, const volatile unsigned long *ptr)
{
	unsigned long addr;
	unsigned char ch;

	addr = (unsigned long) ptr + ((nr ^ (__BITOPS_WORDSIZE - 8)) >> 3);
	ch = *(volatile unsigned char *) addr;
	return (ch >> (nr & 7)) & 1;
}

static inline int 
__constant_test_bit(unsigned long nr, const volatile unsigned long *addr) {
    return (((volatile char *) addr)
	    [(nr^(__BITOPS_WORDSIZE-8))>>3] & (1<<(nr&7))) != 0;
}

#define test_bit(nr,addr) \
(__builtin_constant_p((nr)) ? \
 __constant_test_bit((nr),(addr)) : \
 __test_bit((nr),(addr)) )


static inline unsigned long __ffz_word_loop(const unsigned long *addr,
					    unsigned long size)
{
	typedef struct { long _[__BITOPS_WORDS(size)]; } addrtype;
	unsigned long bytes = 0;

	asm volatile(
#ifndef __s390x__
		"	ahi	%1,-1\n"
		"	sra	%1,5\n"
		"	jz	1f\n"
		"0:	c	%2,0(%0,%3)\n"
		"	jne	1f\n"
		"	la	%0,4(%0)\n"
		"	brct	%1,0b\n"
		"1:\n"
#else
		"	aghi	%1,-1\n"
		"	srag	%1,%1,6\n"
		"	jz	1f\n"
		"0:	cg	%2,0(%0,%3)\n"
		"	jne	1f\n"
		"	la	%0,8(%0)\n"
		"	brct	%1,0b\n"
		"1:\n"
#endif
		: "+&a" (bytes), "+&d" (size)
		: "d" (-1UL), "a" (addr), "m" (*(addrtype *) addr)
		: "cc" );
	return bytes;
}

static inline unsigned long __ffs_word_loop(const unsigned long *addr,
					    unsigned long size)
{
	typedef struct { long _[__BITOPS_WORDS(size)]; } addrtype;
	unsigned long bytes = 0;

	asm volatile(
#ifndef __s390x__
		"	ahi	%1,-1\n"
		"	sra	%1,5\n"
		"	jz	1f\n"
		"0:	c	%2,0(%0,%3)\n"
		"	jne	1f\n"
		"	la	%0,4(%0)\n"
		"	brct	%1,0b\n"
		"1:\n"
#else
		"	aghi	%1,-1\n"
		"	srag	%1,%1,6\n"
		"	jz	1f\n"
		"0:	cg	%2,0(%0,%3)\n"
		"	jne	1f\n"
		"	la	%0,8(%0)\n"
		"	brct	%1,0b\n"
		"1:\n"
#endif
		: "+&a" (bytes), "+&a" (size)
		: "d" (0UL), "a" (addr), "m" (*(addrtype *) addr)
		: "cc" );
	return bytes;
}

static inline unsigned long __ffz_word(unsigned long nr, unsigned long word)
{
#ifdef __s390x__
	if ((word & 0xffffffff) == 0xffffffff) {
		word >>= 32;
		nr += 32;
	}
#endif
	if ((word & 0xffff) == 0xffff) {
		word >>= 16;
		nr += 16;
	}
	if ((word & 0xff) == 0xff) {
		word >>= 8;
		nr += 8;
	}
	return nr + _zb_findmap[(unsigned char) word];
}

static inline unsigned long __ffs_word(unsigned long nr, unsigned long word)
{
#ifdef __s390x__
	if ((word & 0xffffffff) == 0) {
		word >>= 32;
		nr += 32;
	}
#endif
	if ((word & 0xffff) == 0) {
		word >>= 16;
		nr += 16;
	}
	if ((word & 0xff) == 0) {
		word >>= 8;
		nr += 8;
	}
	return nr + _sb_findmap[(unsigned char) word];
}


static inline unsigned long __load_ulong_be(const unsigned long *p,
					    unsigned long offset)
{
	p = (unsigned long *)((unsigned long) p + offset);
	return *p;
}

static inline unsigned long __load_ulong_le(const unsigned long *p,
					    unsigned long offset)
{
	unsigned long word;

	p = (unsigned long *)((unsigned long) p + offset);
#ifndef __s390x__
	asm volatile(
		"	ic	%0,%O1(%R1)\n"
		"	icm	%0,2,%O1+1(%R1)\n"
		"	icm	%0,4,%O1+2(%R1)\n"
		"	icm	%0,8,%O1+3(%R1)"
		: "=&d" (word) : "Q" (*p) : "cc");
#else
	asm volatile(
		"	lrvg	%0,%1"
		: "=d" (word) : "m" (*p) );
#endif
	return word;
}


static inline unsigned long ffz(unsigned long word)
{
	return __ffz_word(0, word);
}

static inline unsigned long __ffs (unsigned long word)
{
	return __ffs_word(0, word);
}

static inline int ffs(int x)
{
	if (!x)
		return 0;
	return __ffs_word(1, x);
}

static inline unsigned long find_first_zero_bit(const unsigned long *addr,
						unsigned long size)
{
	unsigned long bytes, bits;

        if (!size)
                return 0;
	bytes = __ffz_word_loop(addr, size);
	bits = __ffz_word(bytes*8, __load_ulong_be(addr, bytes));
	return (bits < size) ? bits : size;
}
#define find_first_zero_bit find_first_zero_bit

static inline unsigned long find_first_bit(const unsigned long * addr,
					   unsigned long size)
{
	unsigned long bytes, bits;

        if (!size)
                return 0;
	bytes = __ffs_word_loop(addr, size);
	bits = __ffs_word(bytes*8, __load_ulong_be(addr, bytes));
	return (bits < size) ? bits : size;
}
#define find_first_bit find_first_bit

static inline int find_next_zero_bit (const unsigned long * addr,
				      unsigned long size,
				      unsigned long offset)
{
        const unsigned long *p;
	unsigned long bit, set;

	if (offset >= size)
		return size;
	bit = offset & (__BITOPS_WORDSIZE - 1);
	offset -= bit;
	size -= offset;
	p = addr + offset / __BITOPS_WORDSIZE;
	if (bit) {
		set = __ffz_word(bit, *p >> bit);
		if (set >= size)
			return size + offset;
		if (set < __BITOPS_WORDSIZE)
			return set + offset;
		offset += __BITOPS_WORDSIZE;
		size -= __BITOPS_WORDSIZE;
		p++;
	}
	return offset + find_first_zero_bit(p, size);
}
#define find_next_zero_bit find_next_zero_bit

static inline int find_next_bit (const unsigned long * addr,
				 unsigned long size,
				 unsigned long offset)
{
        const unsigned long *p;
	unsigned long bit, set;

	if (offset >= size)
		return size;
	bit = offset & (__BITOPS_WORDSIZE - 1);
	offset -= bit;
	size -= offset;
	p = addr + offset / __BITOPS_WORDSIZE;
	if (bit) {
		set = __ffs_word(0, *p & (~0UL << bit));
		if (set >= size)
			return size + offset;
		if (set < __BITOPS_WORDSIZE)
			return set + offset;
		offset += __BITOPS_WORDSIZE;
		size -= __BITOPS_WORDSIZE;
		p++;
	}
	return offset + find_first_bit(p, size);
}
#define find_next_bit find_next_bit

static inline int sched_find_first_bit(unsigned long *b)
{
	return find_first_bit(b, 140);
}

#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/fls64.h>

#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/lock.h>


static inline int find_first_zero_bit_le(void *vaddr, unsigned int size)
{
	unsigned long bytes, bits;

        if (!size)
                return 0;
	bytes = __ffz_word_loop(vaddr, size);
	bits = __ffz_word(bytes*8, __load_ulong_le(vaddr, bytes));
	return (bits < size) ? bits : size;
}
#define find_first_zero_bit_le find_first_zero_bit_le

static inline int find_next_zero_bit_le(void *vaddr, unsigned long size,
					  unsigned long offset)
{
        unsigned long *addr = vaddr, *p;
	unsigned long bit, set;

        if (offset >= size)
                return size;
	bit = offset & (__BITOPS_WORDSIZE - 1);
	offset -= bit;
	size -= offset;
	p = addr + offset / __BITOPS_WORDSIZE;
        if (bit) {
		set = __ffz_word(bit, __load_ulong_le(p, 0) >> bit);
		if (set >= size)
			return size + offset;
		if (set < __BITOPS_WORDSIZE)
			return set + offset;
		offset += __BITOPS_WORDSIZE;
		size -= __BITOPS_WORDSIZE;
		p++;
        }
	return offset + find_first_zero_bit_le(p, size);
}
#define find_next_zero_bit_le find_next_zero_bit_le

static inline unsigned long find_first_bit_le(void *vaddr, unsigned long size)
{
	unsigned long bytes, bits;

	if (!size)
		return 0;
	bytes = __ffs_word_loop(vaddr, size);
	bits = __ffs_word(bytes*8, __load_ulong_le(vaddr, bytes));
	return (bits < size) ? bits : size;
}
#define find_first_bit_le find_first_bit_le

static inline int find_next_bit_le(void *vaddr, unsigned long size,
				     unsigned long offset)
{
	unsigned long *addr = vaddr, *p;
	unsigned long bit, set;

	if (offset >= size)
		return size;
	bit = offset & (__BITOPS_WORDSIZE - 1);
	offset -= bit;
	size -= offset;
	p = addr + offset / __BITOPS_WORDSIZE;
	if (bit) {
		set = __ffs_word(0, __load_ulong_le(p, 0) & (~0UL << bit));
		if (set >= size)
			return size + offset;
		if (set < __BITOPS_WORDSIZE)
			return set + offset;
		offset += __BITOPS_WORDSIZE;
		size -= __BITOPS_WORDSIZE;
		p++;
	}
	return offset + find_first_bit_le(p, size);
}
#define find_next_bit_le find_next_bit_le

#include <asm-generic/bitops/le.h>

#include <asm-generic/bitops/ext2-atomic-setbit.h>


#endif 

#endif 
