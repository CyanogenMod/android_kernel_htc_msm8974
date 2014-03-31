
#ifndef _S390_TYPES_H
#define _S390_TYPES_H

#include <asm-generic/int-ll64.h>

#ifndef __ASSEMBLY__

typedef unsigned long addr_t; 
typedef __signed__ long saddr_t;

#endif 

#ifdef __KERNEL__

#ifndef __ASSEMBLY__

#ifndef __s390x__
typedef union {
	unsigned long long pair;
	struct {
		unsigned long even;
		unsigned long odd;
	} subreg;
} register_pair;

#endif 
#endif 
#endif 
#endif 
