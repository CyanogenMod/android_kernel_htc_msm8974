#ifndef _ASM_POWERPC_UNALIGNED_H
#define _ASM_POWERPC_UNALIGNED_H

#ifdef __KERNEL__

#include <linux/unaligned/access_ok.h>
#include <linux/unaligned/generic.h>

#define get_unaligned	__get_unaligned_be
#define put_unaligned	__put_unaligned_be

#endif	
#endif	
