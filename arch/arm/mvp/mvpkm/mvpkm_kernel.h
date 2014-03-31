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

#ifndef _MVPKM_KERNEL_H
#define _MVPKM_KERNEL_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#include <linux/rwsem.h>
#include <linux/kobject.h>
#include <linux/rbtree.h>

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#include "atomic.h"
#include "montimer_kernel.h"
#include "worldswitch.h"


struct MvpkmVM {
	struct kobject      kobj;        
	struct kset        *devicesKSet; 
	struct kset        *miscKSet;    
	_Bool               haveKObj;    
	struct rb_root      lockedRoot;  
	struct rw_semaphore lockedSem;   
	AtmUInt32           usedPages;   
	_Bool               isMonitorInited; 
	WorldSwitchPage    *wsp;             
	wait_queue_head_t   wfiWaitQ;        

	struct rw_semaphore wspSem;

	
	struct MonTimer     monTimer;

	
	MPN                 stubPageMPN;

	struct vm_struct   *wspHkvaArea;  
	HKVA                wspHKVADummyPage; 
#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock    wakeLock;        
#endif

	
	struct rw_semaphore monThreadTaskSem;

	struct task_struct *monThreadTask;
	struct timer_list   balloonWDTimer;  
	_Bool               balloonWDEnabled;
	_Bool               watchdogTriggered;
};

void Mvpkm_WakeGuest(struct MvpkmVM *vm, int why);
struct kset *Mvpkm_FindVMNamedKSet(int vmID, const char *name);

extern struct cpumask inMonitor;

#endif
