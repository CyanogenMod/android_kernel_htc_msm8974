/*
 *  linux/arch/alpha/lib/memcpy.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 */


/*
 * Note that the C code is written to be optimized into good assembly. However,
 * at this point gcc is unable to sanely compile "if (n >= 0)", resulting in a
 * explicit compare against 0 (instead of just using the proper "blt reg, xx" or
 * "bge reg, xx"). I hope alpha-gcc will be fixed to notice this eventually..
 */

#include <linux/types.h>

#define ALIGN_DEST_TO8_UP(d,s,n) \
	while (d & 7) { \
		if (n <= 0) return; \
		n--; \
		*(char *) d = *(char *) s; \
		d++; s++; \
	}
#define ALIGN_DEST_TO8_DN(d,s,n) \
	while (d & 7) { \
		if (n <= 0) return; \
		n--; \
		d--; s--; \
		*(char *) d = *(char *) s; \
	}

#define DO_REST_UP(d,s,n) \
	while (n > 0) { \
		n--; \
		*(char *) d = *(char *) s; \
		d++; s++; \
	}
#define DO_REST_DN(d,s,n) \
	while (n > 0) { \
		n--; \
		d--; s--; \
		*(char *) d = *(char *) s; \
	}

#define DO_REST_ALIGNED_UP(d,s,n) DO_REST_UP(d,s,n)
#define DO_REST_ALIGNED_DN(d,s,n) DO_REST_DN(d,s,n)

static inline void __memcpy_unaligned_up (unsigned long d, unsigned long s,
					  long n)
{
	ALIGN_DEST_TO8_UP(d,s,n);
	n -= 8;			
	if (n >= 0) {
		unsigned long low_word, high_word;
		__asm__("ldq_u %0,%1":"=r" (low_word):"m" (*(unsigned long *) s));
		do {
			unsigned long tmp;
			__asm__("ldq_u %0,%1":"=r" (high_word):"m" (*(unsigned long *)(s+8)));
			n -= 8;
			__asm__("extql %1,%2,%0"
				:"=r" (low_word)
				:"r" (low_word), "r" (s));
			__asm__("extqh %1,%2,%0"
				:"=r" (tmp)
				:"r" (high_word), "r" (s));
			s += 8;
			*(unsigned long *) d = low_word | tmp;
			d += 8;
			low_word = high_word;
		} while (n >= 0);
	}
	n += 8;
	DO_REST_UP(d,s,n);
}

static inline void __memcpy_unaligned_dn (unsigned long d, unsigned long s,
					  long n)
{
	
	s += n;
	d += n;
	while (n--)
		* (char *) --d = * (char *) --s;
}

static inline void __memcpy_aligned_up (unsigned long d, unsigned long s,
					long n)
{
	ALIGN_DEST_TO8_UP(d,s,n);
	n -= 8;
	while (n >= 0) {
		unsigned long tmp;
		__asm__("ldq %0,%1":"=r" (tmp):"m" (*(unsigned long *) s));
		n -= 8;
		s += 8;
		*(unsigned long *) d = tmp;
		d += 8;
	}
	n += 8;
	DO_REST_ALIGNED_UP(d,s,n);
}
static inline void __memcpy_aligned_dn (unsigned long d, unsigned long s,
					long n)
{
	s += n;
	d += n;
	ALIGN_DEST_TO8_DN(d,s,n);
	n -= 8;
	while (n >= 0) {
		unsigned long tmp;
		s -= 8;
		__asm__("ldq %0,%1":"=r" (tmp):"m" (*(unsigned long *) s));
		n -= 8;
		d -= 8;
		*(unsigned long *) d = tmp;
	}
	n += 8;
	DO_REST_ALIGNED_DN(d,s,n);
}

void * memcpy(void * dest, const void *src, size_t n)
{
	if (!(((unsigned long) dest ^ (unsigned long) src) & 7)) {
		__memcpy_aligned_up ((unsigned long) dest, (unsigned long) src,
				     n);
		return dest;
	}
	__memcpy_unaligned_up ((unsigned long) dest, (unsigned long) src, n);
	return dest;
}

asm("__memcpy = memcpy; .globl __memcpy");
