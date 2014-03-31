
/*
 *	cache.c -- general ColdFire Cache maintenance code
 *
 *	Copyright (C) 2010, Greg Ungerer (gerg@snapgear.com)
 */


#include <linux/kernel.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>

#ifdef CACHE_PUSH


void mcf_cache_push(void)
{
	__asm__ __volatile__ (
		"clrl	%%d0\n\t"
		"1:\n\t"
		"movel	%%d0,%%a0\n\t"
		"2:\n\t"
		".word	0xf468\n\t"
		"addl	%0,%%a0\n\t"
		"cmpl	%1,%%a0\n\t"
		"blt	2b\n\t"
		"addql	#1,%%d0\n\t"
		"cmpil	%2,%%d0\n\t"
		"bne	1b\n\t"
		: 
		: "i" (CACHE_LINE_SIZE),
		  "i" (DCACHE_SIZE / CACHE_WAYS),
		  "i" (CACHE_WAYS)
		: "d0", "a0" );
}

#endif 
