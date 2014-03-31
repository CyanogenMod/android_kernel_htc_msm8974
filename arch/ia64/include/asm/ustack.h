#ifndef _ASM_IA64_USTACK_H
#define _ASM_IA64_USTACK_H


#ifdef __KERNEL__
#include <asm/page.h>

#define MAX_USER_STACK_SIZE	(RGN_MAP_LIMIT/2)
#define STACK_TOP		(0x6000000000000000UL + RGN_MAP_LIMIT)
#define STACK_TOP_MAX		STACK_TOP
#endif

#define DEFAULT_USER_STACK_SIZE	(1UL << 31)

#endif 
