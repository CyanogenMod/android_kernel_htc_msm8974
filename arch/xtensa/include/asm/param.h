/*
 * include/asm-xtensa/param.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_PARAM_H
#define _XTENSA_PARAM_H

#ifdef __KERNEL__
# define HZ		CONFIG_HZ	
# define USER_HZ	100		
# define CLOCKS_PER_SEC (USER_HZ)	
#else
# define HZ		100
#endif

#define EXEC_PAGESIZE	4096

#ifndef NGROUPS
#define NGROUPS		32
#endif

#ifndef NOGROUP
#define NOGROUP		(-1)
#endif

#define MAXHOSTNAMELEN	64	

#endif 
