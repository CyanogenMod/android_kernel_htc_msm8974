/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 96, 98, 99, 2000 by Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef _ASM_RESOURCE_H
#define _ASM_RESOURCE_H


#define RLIMIT_NOFILE		5	
#define RLIMIT_AS		6	
#define RLIMIT_RSS		7	
#define RLIMIT_NPROC		8	
#define RLIMIT_MEMLOCK		9	

#ifdef CONFIG_32BIT
# define RLIM_INFINITY		0x7fffffffUL
#endif

#include <asm-generic/resource.h>

#endif 
