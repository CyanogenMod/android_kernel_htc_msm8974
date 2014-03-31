/* MN10300 64-bit division
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_DIV64
#define _ASM_DIV64

#include <linux/types.h>

extern void ____unhandled_size_in_do_div___(void);

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
# define CLOBBER_MDR_CC		"mdr", "cc"
#else
# define CLOBBER_MDR_CC		"cc"
#endif

#define do_div(n, base)							\
({									\
	unsigned __rem = 0;						\
	if (sizeof(n) <= 4) {						\
		asm("mov	%1,mdr	\n"				\
		    "divu	%2,%0	\n"				\
		    "mov	mdr,%1	\n"				\
		    : "+r"(n), "=d"(__rem)				\
		    : "r"(base), "1"(__rem)				\
		    : CLOBBER_MDR_CC					\
		    );							\
	} else if (sizeof(n) <= 8) {					\
		union {							\
			unsigned long long l;				\
			u32 w[2];					\
		} __quot;						\
		__quot.l = n;						\
		asm("mov	%0,mdr	\n"			\
		    "divu	%3,%1	\n"				\
		    		\
		    			\
		    "divu	%3,%2	\n"				\
		    		\
		    			\
		    "mov	mdr,%0	\n"				\
		    : "=d"(__rem), "=r"(__quot.w[1]), "=r"(__quot.w[0])	\
		    : "r"(base), "0"(__rem), "1"(__quot.w[1]),		\
		      "2"(__quot.w[0])					\
		    : CLOBBER_MDR_CC					\
		    );							\
		n = __quot.l;						\
	} else {							\
		____unhandled_size_in_do_div___();			\
	}								\
	__rem;								\
})

static inline __attribute__((const))
unsigned __muldiv64u(unsigned val, unsigned mult, unsigned div)
{
	unsigned result;

	asm("mulu	%2,%0	\n"	
	    "divu	%3,%0	\n"	
	    : "=r"(result)
	    : "0"(val), "ir"(mult), "r"(div)
	    : CLOBBER_MDR_CC
	    );

	return result;
}

static inline __attribute__((const))
signed __muldiv64s(signed val, signed mult, signed div)
{
	signed result;

	asm("mul	%2,%0	\n"	
	    "div	%3,%0	\n"	
	    : "=r"(result)
	    : "0"(val), "ir"(mult), "r"(div)
	    : CLOBBER_MDR_CC
	    );

	return result;
}

#endif 
