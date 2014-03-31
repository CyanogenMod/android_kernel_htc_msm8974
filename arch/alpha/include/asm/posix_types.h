#ifndef _ALPHA_POSIX_TYPES_H
#define _ALPHA_POSIX_TYPES_H


typedef unsigned int	__kernel_ino_t;
#define __kernel_ino_t __kernel_ino_t

typedef unsigned int	__kernel_nlink_t;
#define __kernel_nlink_t __kernel_nlink_t

typedef unsigned long	__kernel_sigset_t;	

#include <asm-generic/posix_types.h>

#endif 
