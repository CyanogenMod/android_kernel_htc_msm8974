/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Guest Communications
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


#ifndef	_COMM_OS_H_
#define	_COMM_OS_H_

#define COMM_OS_4EVER_TO ((unsigned long long)(~0UL >> 1))

typedef int (*CommOSWaitConditionFunc)(void *arg1, void *arg2);

typedef unsigned int (*CommOSDispatchFunc)(void);

extern int  (*commOSModInit)(void *args);
extern void (*commOSModExit)(void);

#define COMM_OS_MOD_INIT(init, exit)        \
	int (*commOSModInit)(void *args) = init; \
	void (*commOSModExit)(void) = exit




#ifdef __linux__
#include "comm_os_linux.h"
#else
#error "Unsupported OS"
#endif

void CommOS_StopIO(void);
void CommOS_ScheduleDisp(void);
void CommOS_InitWork(CommOSWork *work, CommOSWorkFunc func);
int CommOS_ScheduleAIOWork(CommOSWork *work);
void CommOS_FlushAIOWork(CommOSWork *work);

int
CommOS_StartIO(const char *dispatchTaskName,
	       CommOSDispatchFunc dispatchHandler,
	       unsigned int interval,
	       unsigned int maxCycles,
	       const char *aioTaskName);


#endif  

