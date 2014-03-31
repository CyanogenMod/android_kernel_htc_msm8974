#ifndef __ARCH_PARISC_POSIX_TYPES_H
#define __ARCH_PARISC_POSIX_TYPES_H


typedef unsigned short		__kernel_mode_t;
#define __kernel_mode_t __kernel_mode_t

typedef unsigned short		__kernel_nlink_t;
#define __kernel_nlink_t __kernel_nlink_t

typedef unsigned short		__kernel_ipc_pid_t;
#define __kernel_ipc_pid_t __kernel_ipc_pid_t

typedef int			__kernel_suseconds_t;
#define __kernel_suseconds_t __kernel_suseconds_t

typedef long long		__kernel_off64_t;
typedef unsigned long long	__kernel_ino64_t;

#include <asm-generic/posix_types.h>

#endif
