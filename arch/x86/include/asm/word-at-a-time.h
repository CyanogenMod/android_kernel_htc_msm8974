#ifndef _ASM_WORD_AT_A_TIME_H
#define _ASM_WORD_AT_A_TIME_H


#ifdef CONFIG_64BIT

static inline long count_masked_bytes(unsigned long mask)
{
	return mask*0x0001020304050608ul >> 56;
}

#else	

static inline long count_masked_bytes(long mask)
{
	
	long a = (0x0ff0001+mask) >> 23;
	
	return a & mask;
}

#endif

#define REPEAT_BYTE(x)	((~0ul / 0xff) * (x))

static inline unsigned long has_zero(unsigned long a)
{
	return ((a - REPEAT_BYTE(0x01)) & ~a) & REPEAT_BYTE(0x80);
}

static inline unsigned long load_unaligned_zeropad(const void *addr)
{
	unsigned long ret, dummy;

	asm(
		"1:\tmov %2,%0\n"
		"2:\n"
		".section .fixup,\"ax\"\n"
		"3:\t"
		"lea %2,%1\n\t"
		"and %3,%1\n\t"
		"mov (%1),%0\n\t"
		"leal %2,%%ecx\n\t"
		"andl %4,%%ecx\n\t"
		"shll $3,%%ecx\n\t"
		"shr %%cl,%0\n\t"
		"jmp 2b\n"
		".previous\n"
		_ASM_EXTABLE(1b, 3b)
		:"=&r" (ret),"=&c" (dummy)
		:"m" (*(unsigned long *)addr),
		 "i" (-sizeof(unsigned long)),
		 "i" (sizeof(unsigned long)-1));
	return ret;
}

#endif 
