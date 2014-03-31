/*
 * resource.h: Resource definitions.
 *
 * Copyright (C) 1995,1996 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC_RESOURCE_H
#define _SPARC_RESOURCE_H

#define RLIMIT_NOFILE		6	
#define RLIMIT_NPROC		7	

#if defined(__sparc__) && defined(__arch64__)
#else
#define RLIM_INFINITY		0x7fffffff
#endif

#include <asm-generic/resource.h>

#endif 
