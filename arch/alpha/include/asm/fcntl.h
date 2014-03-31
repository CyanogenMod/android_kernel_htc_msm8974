#ifndef _ALPHA_FCNTL_H
#define _ALPHA_FCNTL_H

#define O_CREAT		 01000	
#define O_TRUNC		 02000	
#define O_EXCL		 04000	
#define O_NOCTTY	010000	

#define O_NONBLOCK	 00004
#define O_APPEND	 00010
#define O_DSYNC		040000	
#define O_DIRECTORY	0100000	
#define O_NOFOLLOW	0200000 
#define O_LARGEFILE	0400000 
#define O_DIRECT	02000000 
#define O_NOATIME	04000000
#define O_CLOEXEC	010000000 
#define __O_SYNC	020000000
#define O_SYNC		(__O_SYNC|O_DSYNC)

#define O_PATH		040000000

#define F_GETLK		7
#define F_SETLK		8
#define F_SETLKW	9

#define F_SETOWN	5	
#define F_GETOWN	6	
#define F_SETSIG	10	
#define F_GETSIG	11	

#define F_RDLCK		1
#define F_WRLCK		2
#define F_UNLCK		8

#define F_EXLCK		16	
#define F_SHLCK		32	

#include <asm-generic/fcntl.h>

#endif
