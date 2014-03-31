/*
 * include/asm-xtensa/swab.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_SWAB_H
#define _XTENSA_SWAB_H

#include <linux/types.h>
#include <linux/compiler.h>

#define __SWAB_64_THRU_32__

static inline __attribute_const__ __u32 __arch_swab32(__u32 x)
{
    __u32 res;
    
    __asm__("ssai     8           \n\t"
	    "srli     %0, %1, 16  \n\t"
	    "src      %0, %0, %1  \n\t"
	    "src      %0, %0, %0  \n\t"
	    "src      %0, %1, %0  \n"
	    : "=&a" (res)
	    : "a" (x)
	    );
    return res;
}
#define __arch_swab32 __arch_swab32

static inline __attribute_const__ __u16 __arch_swab16(__u16 x)
{


    __u32 res;
    __u32 tmp;

    __asm__("extui    %1, %2, 8, 8\n\t"
	    "slli     %0, %2, 8   \n\t"
	    "or       %0, %0, %1  \n"
	    : "=&a" (res), "=&a" (tmp)
	    : "a" (x)
	    );

    return res;
}
#define __arch_swab16 __arch_swab16

#endif 
