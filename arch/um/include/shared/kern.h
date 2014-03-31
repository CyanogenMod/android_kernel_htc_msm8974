/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __KERN_H__
#define __KERN_H__


extern int printf(const char *fmt, ...);
extern void *sbrk(int increment);
extern int pause(void);
extern void exit(int);

#endif

