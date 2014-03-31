#ifndef _PARISC_SECTIONS_H
#define _PARISC_SECTIONS_H

#include <asm-generic/sections.h>

#ifdef CONFIG_64BIT
#undef dereference_function_descriptor
void *dereference_function_descriptor(void *);
#endif

#endif
