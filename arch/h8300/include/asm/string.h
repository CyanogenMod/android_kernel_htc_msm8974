#ifndef _H8300_STRING_H_
#define _H8300_STRING_H_

#ifdef __KERNEL__ 

#include <asm/setup.h>
#include <asm/page.h>

#define __HAVE_ARCH_MEMSET
extern void * memset(void * s, int c, size_t count);

#define __HAVE_ARCH_MEMCPY
extern void * memcpy(void *d, const void *s, size_t count);

#else 


#define __HAVE_ARCH_STRCPY 1
#define __HAVE_ARCH_STRNCPY 1
#define __HAVE_ARCH_STRCAT 1
#define __HAVE_ARCH_STRNCAT 1
#define __HAVE_ARCH_STRCMP 1
#define __HAVE_ARCH_STRNCMP 1
#define __HAVE_ARCH_STRNICMP 1
#define __HAVE_ARCH_STRCHR 1
#define __HAVE_ARCH_STRRCHR 1
#define __HAVE_ARCH_STRSTR 1
#define __HAVE_ARCH_STRLEN 1
#define __HAVE_ARCH_STRNLEN 1
#define __HAVE_ARCH_MEMSET 1
#define __HAVE_ARCH_MEMCPY 1
#define __HAVE_ARCH_MEMMOVE 1
#define __HAVE_ARCH_MEMSCAN 1
#define __HAVE_ARCH_MEMCMP 1
#define __HAVE_ARCH_MEMCHR 1
#define __HAVE_ARCH_STRTOK 1

#endif 

#endif 
