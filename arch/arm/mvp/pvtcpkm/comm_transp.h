/*
 * Linux 2.6.32 and later Kernel module for VMware MVP PVTCP Server
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5


#ifndef _COMM_TRANSP_H_
#define _COMM_TRANSP_H_

#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#include "include_check.h"


#define COMM_TRANSP_ID_8_ANY ((unsigned char)-1)
#define COMM_TRANSP_ID_32_ANY ((unsigned int)-1)
#define COMM_TRANSP_ID_64_ANY ((unsigned long long)-1)


typedef struct CommTranspID {
	union {
		unsigned char d8[8];
		unsigned int d32[2];
		unsigned long long d64;
	};
} CommTranspID;



typedef enum CommTranspInitMode {
	COMM_TRANSP_INIT_CREATE = 0x0,
	COMM_TRANSP_INIT_ATTACH = 0x1
} CommTranspInitMode;

typedef struct CommTranspInitArgs {
	unsigned int capacity;      
	unsigned int type;          
	CommTranspID id;            
	CommTranspInitMode mode;    
} CommTranspInitArgs;



static inline unsigned int
CommTransp_GetType(const char *str)
{
	unsigned int hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; 

	return hash;
}

#endif 

