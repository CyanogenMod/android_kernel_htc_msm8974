/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
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


#ifndef _MUTEX_H
#define _MUTEX_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#define MUTEX_CVAR_MAX 2  
			  

typedef enum MutexMode MutexMode;
typedef struct HKWaitQ HKWaitQ;
typedef struct Mutex Mutex;

enum MutexMode {
	MutexModeSH = 1,	
				
				
				
				
				

	MutexModeEX = 0xFFFF	
				
				
				
};

#include "atomic.h"

typedef union Mutex_State {
	uint32 state;	
	struct {
		uint16 mode;	
		uint16 blck;	
	};
} Mutex_State;

struct Mutex {
	HKVA mtxHKVA;		
	AtmUInt32 state;	
				
				
	AtmUInt32 waiters;	
				
				
	AtmUInt32 blocked;	
	HKVA lockWaitQ;		
	HKVA cvarWaitQs[MUTEX_CVAR_MAX]; 

	int    line;
	int    lineUnl;
	uint32 pad3;
	uint32 pad4;
	uint32 pad5;
	uint32 pad6;
};

#define Mutex_Lock(a, b) Mutex_LockLine(a, b, __FILE__, __LINE__)
#define Mutex_Unlock(a, b) Mutex_UnlockLine(a, b, __LINE__)
#define Mutex_UnlSleep(a, b, c) Mutex_UnlSleepLine(a, b, c, __FILE__, __LINE__)
#define Mutex_UnlSleepTest(a, b, c, d, e) \
	Mutex_UnlSleepTestLine(a, b, c, d, e, __FILE__, __LINE__)
int   Mutex_LockLine(Mutex *mutex, MutexMode mode, const char *file, int line);
void  Mutex_UnlockLine(Mutex *mutex, MutexMode mode, int line);
int
Mutex_UnlSleepLine(Mutex *mutex,
		   MutexMode mode,
		   uint32 cvi,
		   const char *file,
		   int line);
int
Mutex_UnlSleepTestLine(Mutex *mutex,
		       MutexMode mode,
		       uint32 cvi,
		       AtmUInt32 *test,
		       uint32 mask,
		       const char *file,
		       int line);
void  Mutex_UnlWake(Mutex *mutex, MutexMode mode, uint32 cvi, _Bool all);

#endif
