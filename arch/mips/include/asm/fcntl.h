/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 96, 97, 98, 99, 2003, 05 Ralf Baechle
 */
#ifndef _ASM_FCNTL_H
#define _ASM_FCNTL_H


#define O_APPEND	0x0008
#define O_DSYNC		0x0010	
#define O_NONBLOCK	0x0080
#define O_CREAT         0x0100	
#define O_TRUNC		0x0200	
#define O_EXCL		0x0400	
#define O_NOCTTY	0x0800	
#define FASYNC		0x1000	
#define O_LARGEFILE	0x2000	
#define __O_SYNC	0x4000
#define O_SYNC		(__O_SYNC|O_DSYNC)
#define O_DIRECT	0x8000	

#define F_GETLK		14
#define F_SETLK		6
#define F_SETLKW	7

#define F_SETOWN	24	
#define F_GETOWN	23	

#ifndef __mips64
#define F_GETLK64	33	
#define F_SETLK64	34
#define F_SETLKW64	35
#endif


#ifdef CONFIG_32BIT
#include <linux/types.h>

struct flock {
	short	l_type;
	short	l_whence;
	off_t	l_start;
	off_t	l_len;
	long	l_sysid;
	__kernel_pid_t l_pid;
	long	pad[4];
};

#define HAVE_ARCH_STRUCT_FLOCK

#endif 

#include <asm-generic/fcntl.h>

#endif 
