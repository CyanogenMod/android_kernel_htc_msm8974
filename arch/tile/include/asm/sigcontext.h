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

#ifndef _ASM_TILE_SIGCONTEXT_H
#define _ASM_TILE_SIGCONTEXT_H

#define __need_int_reg_t
#include <arch/abi.h>

struct sigcontext {
	__uint_reg_t gregs[53];	
	__uint_reg_t tp;	
	__uint_reg_t sp;	
	__uint_reg_t lr;	
	__uint_reg_t pc;	
	__uint_reg_t ics;	
	__uint_reg_t faultnum;	
	__uint_reg_t pad[5];
};

#endif 
