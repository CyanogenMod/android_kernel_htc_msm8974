/*
 * jsflash.h: OS Flash SIMM support for JavaStations.
 *
 * Copyright (C) 1999  Pete Zaitcev
 */

#ifndef _SPARC_JSFLASH_H
#define _SPARC_JSFLASH_H

#ifndef _SPARC_TYPES_H
#include <linux/types.h>
#endif


#define JSFLASH_IDENT   (('F'<<8)|54)
struct jsflash_ident_arg {
	__u64 off;                
	__u32 size;
	char name[32];		
};

#define JSFLASH_ERASE   (('F'<<8)|55)

#define JSFLASH_PROGRAM (('F'<<8)|56)
struct jsflash_program_arg {
	__u64 data;		
	__u64 off;
	__u32 size;
};

#endif 
