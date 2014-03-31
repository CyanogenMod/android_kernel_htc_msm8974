/*
 *  arch/s390/lib/string.c
 *    Optimized string functions
 *
 *  S390 version
 *    Copyright (C) 2004 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 */

#define IN_ARCH_STRING_C 1

#include <linux/types.h>
#include <linux/module.h>

static inline char *__strend(const char *s)
{
	register unsigned long r0 asm("0") = 0;

	asm volatile ("0: srst  %0,%1\n"
		      "   jo    0b"
		      : "+d" (r0), "+a" (s) :  : "cc" );
	return (char *) r0;
}

static inline char *__strnend(const char *s, size_t n)
{
	register unsigned long r0 asm("0") = 0;
	const char *p = s + n;

	asm volatile ("0: srst  %0,%1\n"
		      "   jo    0b"
		      : "+d" (p), "+a" (s) : "d" (r0) : "cc" );
	return (char *) p;
}

size_t strlen(const char *s)
{
#if __GNUC__ < 4
	return __strend(s) - s;
#else
	return __builtin_strlen(s);
#endif
}
EXPORT_SYMBOL(strlen);

size_t strnlen(const char * s, size_t n)
{
	return __strnend(s, n) - s;
}
EXPORT_SYMBOL(strnlen);

char *strcpy(char *dest, const char *src)
{
#if __GNUC__ < 4
	register int r0 asm("0") = 0;
	char *ret = dest;

	asm volatile ("0: mvst  %0,%1\n"
		      "   jo    0b"
		      : "+&a" (dest), "+&a" (src) : "d" (r0)
		      : "cc", "memory" );
	return ret;
#else
	return __builtin_strcpy(dest, src);
#endif
}
EXPORT_SYMBOL(strcpy);

size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = __strend(src) - src;

	if (size) {
		size_t len = (ret >= size) ? size-1 : ret;
		dest[len] = '\0';
		__builtin_memcpy(dest, src, len);
	}
	return ret;
}
EXPORT_SYMBOL(strlcpy);

char *strncpy(char *dest, const char *src, size_t n)
{
	size_t len = __strnend(src, n) - src;
	__builtin_memset(dest + len, 0, n - len);
	__builtin_memcpy(dest, src, len);
	return dest;
}
EXPORT_SYMBOL(strncpy);

char *strcat(char *dest, const char *src)
{
	register int r0 asm("0") = 0;
	unsigned long dummy;
	char *ret = dest;

	asm volatile ("0: srst  %0,%1\n"
		      "   jo    0b\n"
		      "1: mvst  %0,%2\n"
		      "   jo    1b"
		      : "=&a" (dummy), "+a" (dest), "+a" (src)
		      : "d" (r0), "0" (0UL) : "cc", "memory" );
	return ret;
}
EXPORT_SYMBOL(strcat);

size_t strlcat(char *dest, const char *src, size_t n)
{
	size_t dsize = __strend(dest) - dest;
	size_t len = __strend(src) - src;
	size_t res = dsize + len;

	if (dsize < n) {
		dest += dsize;
		n -= dsize;
		if (len >= n)
			len = n - 1;
		dest[len] = '\0';
		__builtin_memcpy(dest, src, len);
	}
	return res;
}
EXPORT_SYMBOL(strlcat);

char *strncat(char *dest, const char *src, size_t n)
{
	size_t len = __strnend(src, n) - src;
	char *p = __strend(dest);

	p[len] = '\0';
	__builtin_memcpy(p, src, len);
	return dest;
}
EXPORT_SYMBOL(strncat);

int strcmp(const char *cs, const char *ct)
{
	register int r0 asm("0") = 0;
	int ret = 0;

	asm volatile ("0: clst %2,%3\n"
		      "   jo   0b\n"
		      "   je   1f\n"
		      "   ic   %0,0(%2)\n"
		      "   ic   %1,0(%3)\n"
		      "   sr   %0,%1\n"
		      "1:"
		      : "+d" (ret), "+d" (r0), "+a" (cs), "+a" (ct)
		      : : "cc" );
	return ret;
}
EXPORT_SYMBOL(strcmp);

char * strrchr(const char * s, int c)
{
       size_t len = __strend(s) - s;

       if (len)
	       do {
		       if (s[len] == (char) c)
			       return (char *) s + len;
	       } while (--len > 0);
       return NULL;
}
EXPORT_SYMBOL(strrchr);

char * strstr(const char * s1,const char * s2)
{
	int l1, l2;

	l2 = __strend(s2) - s2;
	if (!l2)
		return (char *) s1;
	l1 = __strend(s1) - s1;
	while (l1-- >= l2) {
		register unsigned long r2 asm("2") = (unsigned long) s1;
		register unsigned long r3 asm("3") = (unsigned long) l2;
		register unsigned long r4 asm("4") = (unsigned long) s2;
		register unsigned long r5 asm("5") = (unsigned long) l2;
		int cc;

		asm volatile ("0: clcle %1,%3,0\n"
			      "   jo    0b\n"
			      "   ipm   %0\n"
			      "   srl   %0,28"
			      : "=&d" (cc), "+a" (r2), "+a" (r3),
			        "+a" (r4), "+a" (r5) : : "cc" );
		if (!cc)
			return (char *) s1;
		s1++;
	}
	return NULL;
}
EXPORT_SYMBOL(strstr);

void *memchr(const void *s, int c, size_t n)
{
	register int r0 asm("0") = (char) c;
	const void *ret = s + n;

	asm volatile ("0: srst  %0,%1\n"
		      "   jo    0b\n"
		      "   jl	1f\n"
		      "   la    %0,0\n"
		      "1:"
		      : "+a" (ret), "+&a" (s) : "d" (r0) : "cc" );
	return (void *) ret;
}
EXPORT_SYMBOL(memchr);

int memcmp(const void *cs, const void *ct, size_t n)
{
	register unsigned long r2 asm("2") = (unsigned long) cs;
	register unsigned long r3 asm("3") = (unsigned long) n;
	register unsigned long r4 asm("4") = (unsigned long) ct;
	register unsigned long r5 asm("5") = (unsigned long) n;
	int ret;

	asm volatile ("0: clcle %1,%3,0\n"
		      "   jo    0b\n"
		      "   ipm   %0\n"
		      "   srl   %0,28"
		      : "=&d" (ret), "+a" (r2), "+a" (r3), "+a" (r4), "+a" (r5)
		      : : "cc" );
	if (ret)
		ret = *(char *) r2 - *(char *) r4;
	return ret;
}
EXPORT_SYMBOL(memcmp);

void *memscan(void *s, int c, size_t n)
{
	register int r0 asm("0") = (char) c;
	const void *ret = s + n;

	asm volatile ("0: srst  %0,%1\n"
		      "   jo    0b\n"
		      : "+a" (ret), "+&a" (s) : "d" (r0) : "cc" );
	return (void *) ret;
}
EXPORT_SYMBOL(memscan);

void *memcpy(void *dest, const void *src, size_t n)
{
	return __builtin_memcpy(dest, src, n);
}
EXPORT_SYMBOL(memcpy);

void *memset(void *s, int c, size_t n)
{
	char *xs;

	if (c == 0)
		return __builtin_memset(s, 0, n);

	xs = (char *) s;
	if (n > 0)
		do {
			*xs++ = c;
		} while (--n > 0);
	return s;
}
EXPORT_SYMBOL(memset);
