#ifndef __ASM_SH_ATOMIC_LLSC_H
#define __ASM_SH_ATOMIC_LLSC_H

static inline void atomic_add(int i, atomic_t *v)
{
	unsigned long tmp;

	__asm__ __volatile__ (
"1:	movli.l @%2, %0		! atomic_add	\n"
"	add	%1, %0				\n"
"	movco.l	%0, @%2				\n"
"	bf	1b				\n"
	: "=&z" (tmp)
	: "r" (i), "r" (&v->counter)
	: "t");
}

static inline void atomic_sub(int i, atomic_t *v)
{
	unsigned long tmp;

	__asm__ __volatile__ (
"1:	movli.l @%2, %0		! atomic_sub	\n"
"	sub	%1, %0				\n"
"	movco.l	%0, @%2				\n"
"	bf	1b				\n"
	: "=&z" (tmp)
	: "r" (i), "r" (&v->counter)
	: "t");
}

static inline int atomic_add_return(int i, atomic_t *v)
{
	unsigned long temp;

	__asm__ __volatile__ (
"1:	movli.l @%2, %0		! atomic_add_return	\n"
"	add	%1, %0					\n"
"	movco.l	%0, @%2					\n"
"	bf	1b					\n"
"	synco						\n"
	: "=&z" (temp)
	: "r" (i), "r" (&v->counter)
	: "t");

	return temp;
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
	unsigned long temp;

	__asm__ __volatile__ (
"1:	movli.l @%2, %0		! atomic_sub_return	\n"
"	sub	%1, %0					\n"
"	movco.l	%0, @%2					\n"
"	bf	1b					\n"
"	synco						\n"
	: "=&z" (temp)
	: "r" (i), "r" (&v->counter)
	: "t");

	return temp;
}

static inline void atomic_clear_mask(unsigned int mask, atomic_t *v)
{
	unsigned long tmp;

	__asm__ __volatile__ (
"1:	movli.l @%2, %0		! atomic_clear_mask	\n"
"	and	%1, %0					\n"
"	movco.l	%0, @%2					\n"
"	bf	1b					\n"
	: "=&z" (tmp)
	: "r" (~mask), "r" (&v->counter)
	: "t");
}

static inline void atomic_set_mask(unsigned int mask, atomic_t *v)
{
	unsigned long tmp;

	__asm__ __volatile__ (
"1:	movli.l @%2, %0		! atomic_set_mask	\n"
"	or	%1, %0					\n"
"	movco.l	%0, @%2					\n"
"	bf	1b					\n"
	: "=&z" (tmp)
	: "r" (mask), "r" (&v->counter)
	: "t");
}

#endif 
