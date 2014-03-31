#ifndef _ASM_X86_POSIX_TYPES_64_H
#define _ASM_X86_POSIX_TYPES_64_H


typedef unsigned short __kernel_old_uid_t;
typedef unsigned short __kernel_old_gid_t;
#define __kernel_old_uid_t __kernel_old_uid_t

typedef unsigned long	__kernel_old_dev_t;
#define __kernel_old_dev_t __kernel_old_dev_t

#include <asm-generic/posix_types.h>

#endif 
