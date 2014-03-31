/* 
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#ifndef __USER_H__
#define __USER_H__

#include <generated/asm-offsets.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stddef.h>
#endif

extern void panic(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

#ifdef UML_CONFIG_PRINTK
extern int printk(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
#else
static inline int printk(const char *fmt, ...)
{
	return 0;
}
#endif

extern int in_aton(char *str);
extern size_t strlcpy(char *, const char *, size_t);
extern size_t strlcat(char *, const char *, size_t);

#define barrier() __asm__ __volatile__("": : :"memory")

#endif
