
#ifndef __ARCH_S390_POSIX_TYPES_H
#define __ARCH_S390_POSIX_TYPES_H


typedef unsigned long   __kernel_size_t;
#define __kernel_size_t __kernel_size_t

typedef unsigned short	__kernel_old_dev_t;
#define __kernel_old_dev_t __kernel_old_dev_t

#ifndef __s390x__

typedef unsigned long   __kernel_ino_t;
typedef unsigned short  __kernel_mode_t;
typedef unsigned short  __kernel_nlink_t;
typedef unsigned short  __kernel_ipc_pid_t;
typedef unsigned short  __kernel_uid_t;
typedef unsigned short  __kernel_gid_t;
typedef int             __kernel_ssize_t;
typedef int             __kernel_ptrdiff_t;

#else 

typedef unsigned int    __kernel_ino_t;
typedef unsigned int    __kernel_mode_t;
typedef unsigned int    __kernel_nlink_t;
typedef int             __kernel_ipc_pid_t;
typedef unsigned int    __kernel_uid_t;
typedef unsigned int    __kernel_gid_t;
typedef long            __kernel_ssize_t;
typedef long            __kernel_ptrdiff_t;
typedef unsigned long   __kernel_sigset_t;      

#endif 

#define __kernel_ino_t  __kernel_ino_t
#define __kernel_mode_t __kernel_mode_t
#define __kernel_nlink_t __kernel_nlink_t
#define __kernel_ipc_pid_t __kernel_ipc_pid_t
#define __kernel_uid_t __kernel_uid_t
#define __kernel_gid_t __kernel_gid_t

#include <asm-generic/posix_types.h>

#endif
