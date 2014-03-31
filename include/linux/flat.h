/*
 * Copyright (C) 2002-2003  David McCullough <davidm@snapgear.com>
 * Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
 *                          The Silver Hammer Group, Ltd.
 *
 * This file provides the definitions and structures needed to
 * support uClinux flat-format executables.
 */

#ifndef _LINUX_FLAT_H
#define _LINUX_FLAT_H

#ifdef __KERNEL__
#include <asm/flat.h>
#endif

#define	FLAT_VERSION			0x00000004L

#ifdef CONFIG_BINFMT_SHARED_FLAT
#define	MAX_SHARED_LIBS			(4)
#else
#define	MAX_SHARED_LIBS			(1)
#endif


struct flat_hdr {
	char magic[4];
	unsigned long rev;          
	unsigned long entry;        
	unsigned long data_start;   
	unsigned long data_end;     
	unsigned long bss_end;      

	

	unsigned long stack_size;   
	unsigned long reloc_start;  
	unsigned long reloc_count;  
	unsigned long flags;       
	unsigned long build_date;   
	unsigned long filler[5];    
};

#define FLAT_FLAG_RAM    0x0001 
#define FLAT_FLAG_GOTPIC 0x0002 
#define FLAT_FLAG_GZIP   0x0004 
#define FLAT_FLAG_GZDATA 0x0008 
#define FLAT_FLAG_KTRACE 0x0010 


#ifdef __KERNEL__ 

#include <asm/byteorder.h>

#define	OLD_FLAT_VERSION			0x00000002L
#define OLD_FLAT_RELOC_TYPE_TEXT	0
#define OLD_FLAT_RELOC_TYPE_DATA	1
#define OLD_FLAT_RELOC_TYPE_BSS		2

typedef union {
	unsigned long	value;
	struct {
# if defined(mc68000) && !defined(CONFIG_COLDFIRE)
		signed long offset : 30;
		unsigned long type : 2;
#   	define OLD_FLAT_FLAG_RAM    0x1 
# elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned long type : 2;
		signed long offset : 30;
#   	define OLD_FLAT_FLAG_RAM    0x1 
# elif defined(__LITTLE_ENDIAN_BITFIELD)
		signed long offset : 30;
		unsigned long type : 2;
#   	define OLD_FLAT_FLAG_RAM    0x1 
# else
#   	error "Unknown bitfield order for flat files."
# endif
	} reloc;
} flat_v2_reloc_t;

#endif 

#endif 
