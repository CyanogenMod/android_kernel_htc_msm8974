/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_MMAN_H
#define _ASM_TILE_MMAN_H

#include <asm-generic/mman-common.h>
#include <arch/chip.h>


#define MAP_POPULATE	0x0040		
#define MAP_NONBLOCK	0x0080		
#define MAP_GROWSDOWN	0x0100		
#define MAP_STACK	MAP_GROWSDOWN	
#define MAP_LOCKED	0x0200		
#define MAP_NORESERVE	0x0400		
#define MAP_DENYWRITE	0x0800		
#define MAP_EXECUTABLE	0x1000		
#define MAP_HUGETLB	0x4000		


#define MCL_CURRENT	1		
#define MCL_FUTURE	2		


#endif 
