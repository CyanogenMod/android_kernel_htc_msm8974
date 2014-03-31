/*
 * sysen.h:  Bit fields within the "System Enable" register accessed via
 *           the ASI_CONTROL address space at address AC_SYSENABLE.
 *
 * Copyright (C) 1994 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC_SYSEN_H
#define _SPARC_SYSEN_H

#define SENABLE_DVMA   0x20   
#define SENABLE_CACHE  0x10   
#define SENABLE_RESET  0x04   

#endif 
