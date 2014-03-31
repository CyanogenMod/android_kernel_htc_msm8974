#ifndef __ASM_SH_CMPXCHG_GRB_H
#define __ASM_SH_CMPXCHG_GRB_H

static inline unsigned long xchg_u32(volatile u32 *m, unsigned long val)
{
	unsigned long retval;

	__asm__ __volatile__ (
		"   .align 2              \n\t"
		"   mova    1f,   r0      \n\t" 
		"   nop                   \n\t"
		"   mov    r15,   r1      \n\t" 
		"   mov    #-4,   r15     \n\t" 
		"   mov.l  @%1,   %0      \n\t" 
		"   mov.l   %2,   @%1     \n\t" 
		"1: mov     r1,   r15     \n\t" 
		: "=&r" (retval),
		  "+r"  (m),
		  "+r"  (val)		
		:
		: "memory", "r0", "r1");

	return retval;
}

static inline unsigned long xchg_u8(volatile u8 *m, unsigned long val)
{
	unsigned long retval;

	__asm__ __volatile__ (
		"   .align  2             \n\t"
		"   mova    1f,   r0      \n\t" 
		"   mov    r15,   r1      \n\t" 
		"   mov    #-6,   r15     \n\t" 
		"   mov.b  @%1,   %0      \n\t" 
		"   extu.b  %0,   %0      \n\t" 
		"   mov.b   %2,   @%1     \n\t" 
		"1: mov     r1,   r15     \n\t" 
		: "=&r" (retval),
		  "+r"  (m),
		  "+r"  (val)		
		:
		: "memory" , "r0", "r1");

	return retval;
}

static inline unsigned long __cmpxchg_u32(volatile int *m, unsigned long old,
					  unsigned long new)
{
	unsigned long retval;

	__asm__ __volatile__ (
		"   .align  2             \n\t"
		"   mova    1f,   r0      \n\t" 
		"   nop                   \n\t"
		"   mov    r15,   r1      \n\t" 
		"   mov    #-8,   r15     \n\t" 
		"   mov.l  @%3,   %0      \n\t" 
		"   cmp/eq  %0,   %1      \n\t"
		"   bf            1f      \n\t" 
		"   mov.l   %2,   @%3     \n\t" 
		"1: mov     r1,   r15     \n\t" 
		: "=&r" (retval),
		  "+r"  (old), "+r"  (new) 
		:  "r"  (m)
		: "memory" , "r0", "r1", "t");

	return retval;
}

#endif 
