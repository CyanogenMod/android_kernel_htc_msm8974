#ifndef _SPARC_FCNTL_H
#define _SPARC_FCNTL_H

#define O_APPEND	0x0008
#define FASYNC		0x0040	
#define O_CREAT		0x0200	
#define O_TRUNC		0x0400	
#define O_EXCL		0x0800	
#define O_DSYNC		0x2000	
#define O_NONBLOCK	0x4000
#if defined(__sparc__) && defined(__arch64__)
#define O_NDELAY	0x0004
#else
#define O_NDELAY	(0x0004 | O_NONBLOCK)
#endif
#define O_NOCTTY	0x8000	
#define O_LARGEFILE	0x40000
#define O_DIRECT        0x100000 
#define O_NOATIME	0x200000
#define O_CLOEXEC	0x400000
#define __O_SYNC	0x800000
#define O_SYNC		(__O_SYNC|O_DSYNC)

#define O_PATH		0x1000000

#define F_GETOWN	5	
#define F_SETOWN	6	
#define F_GETLK		7
#define F_SETLK		8
#define F_SETLKW	9

#define F_RDLCK		1
#define F_WRLCK		2
#define F_UNLCK		3

#define __ARCH_FLOCK_PAD	short __unused;
#define __ARCH_FLOCK64_PAD	short __unused;

#include <asm-generic/fcntl.h>

#endif
