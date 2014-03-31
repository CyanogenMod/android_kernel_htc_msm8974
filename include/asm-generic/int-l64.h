
#ifndef _ASM_GENERIC_INT_L64_H
#define _ASM_GENERIC_INT_L64_H

#include <asm/bitsperlong.h>

#ifndef __ASSEMBLY__

typedef __signed__ char __s8;
typedef unsigned char __u8;

typedef __signed__ short __s16;
typedef unsigned short __u16;

typedef __signed__ int __s32;
typedef unsigned int __u32;

typedef __signed__ long __s64;
typedef unsigned long __u64;

#endif 

#ifdef __KERNEL__

#ifndef __ASSEMBLY__

typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long s64;
typedef unsigned long u64;

#define S8_C(x)  x
#define U8_C(x)  x ## U
#define S16_C(x) x
#define U16_C(x) x ## U
#define S32_C(x) x
#define U32_C(x) x ## U
#define S64_C(x) x ## L
#define U64_C(x) x ## UL

#else 

#define S8_C(x)  x
#define U8_C(x)  x
#define S16_C(x) x
#define U16_C(x) x
#define S32_C(x) x
#define U32_C(x) x
#define S64_C(x) x
#define U64_C(x) x

#endif 

#endif 

#endif 
