/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) 1999 by Ralf Baechle
 */
#ifndef _ASM_SN_TYPES_H
#define _ASM_SN_TYPES_H

#include <linux/types.h>

typedef unsigned long 	cpuid_t;
typedef unsigned long	cnodemask_t;
typedef signed short	nasid_t;	
typedef signed short	cnodeid_t;	
typedef signed char	partid_t;	
typedef signed short	moduleid_t;	
typedef signed short	cmoduleid_t;	
typedef unsigned char	clusterid_t;	
typedef unsigned long 	pfn_t;

typedef dev_t		vertex_hdl_t;	

#endif 
