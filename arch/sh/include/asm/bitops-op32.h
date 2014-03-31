#ifndef __ASM_SH_BITOPS_OP32_H
#define __ASM_SH_BITOPS_OP32_H

#if defined(__BIG_ENDIAN)
#define BITOP_LE_SWIZZLE	((BITS_PER_LONG-1) & ~0x7)
#define BYTE_NUMBER(nr)		((nr ^ BITOP_LE_SWIZZLE) / BITS_PER_BYTE)
#define BYTE_OFFSET(nr)		((nr ^ BITOP_LE_SWIZZLE) % BITS_PER_BYTE)
#else
#define BYTE_NUMBER(nr)		((nr) / BITS_PER_BYTE)
#define BYTE_OFFSET(nr)		((nr) % BITS_PER_BYTE)
#endif

#define IS_IMMEDIATE(nr)	(__builtin_constant_p(nr))

static inline void __set_bit(int nr, volatile unsigned long *addr)
{
	if (IS_IMMEDIATE(nr)) {
		__asm__ __volatile__ (
			"bset.b %1, @(%O2,%0)		! __set_bit\n\t"
			: "+r" (addr)
			: "i" (BYTE_OFFSET(nr)), "i" (BYTE_NUMBER(nr))
			: "t", "memory"
		);
	} else {
		unsigned long mask = BIT_MASK(nr);
		unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

		*p |= mask;
	}
}

static inline void __clear_bit(int nr, volatile unsigned long *addr)
{
	if (IS_IMMEDIATE(nr)) {
		__asm__ __volatile__ (
			"bclr.b %1, @(%O2,%0)		! __clear_bit\n\t"
			: "+r" (addr)
			: "i" (BYTE_OFFSET(nr)),
			  "i" (BYTE_NUMBER(nr))
			: "t", "memory"
		);
	} else {
		unsigned long mask = BIT_MASK(nr);
		unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

		*p &= ~mask;
	}
}

static inline void __change_bit(int nr, volatile unsigned long *addr)
{
	if (IS_IMMEDIATE(nr)) {
		__asm__ __volatile__ (
			"bxor.b %1, @(%O2,%0)		! __change_bit\n\t"
			: "+r" (addr)
			: "i" (BYTE_OFFSET(nr)),
			  "i" (BYTE_NUMBER(nr))
			: "t", "memory"
		);
	} else {
		unsigned long mask = BIT_MASK(nr);
		unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

		*p ^= mask;
	}
}

static inline int __test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old | mask;
	return (old & mask) != 0;
}

static inline int __test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old & ~mask;
	return (old & mask) != 0;
}

static inline int __test_and_change_bit(int nr,
					    volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old ^ mask;
	return (old & mask) != 0;
}

static inline int test_bit(int nr, const volatile unsigned long *addr)
{
	return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
}

#endif 
