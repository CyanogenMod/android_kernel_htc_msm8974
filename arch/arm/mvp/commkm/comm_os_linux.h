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


#ifndef	_COMM_OS_LINUX_H_
#define	_COMM_OS_LINUX_H_

#include <linux/types.h>
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cpu.h>



typedef atomic_t CommOSAtomic;
typedef spinlock_t CommOSSpinlock;
typedef struct mutex CommOSMutex;
typedef wait_queue_head_t CommOSWaitQueue;
typedef struct delayed_work CommOSWork;
typedef void (*CommOSWorkFunc)(CommOSWork *work);
typedef struct list_head CommOSList;
typedef struct module *CommOSModule;



#define CommOSSpinlock_Define DEFINE_SPINLOCK


#define COMM_OS_DOLOG(...) pr_info(__VA_ARGS__)



#if defined(COMM_OS_DEBUG)
   #define CommOS_Debug(args) do { COMM_OS_DOLOG args ; } while (0)
#else
   #define CommOS_Debug(args)
#endif



#define CommOS_Log(args) do { COMM_OS_DOLOG args ; } while (0)



#if defined(COMM_OS_TRACE)
#define TRACE(ptr) \
	CommOS_Debug(("%p:%s: at [%s:%d] with arg ptr [0x%p].\n", current, \
		      __func__, __FILE__, __LINE__, (ptr)))
#else
#define TRACE(ptr)
#endif



static inline void
CommOS_WriteAtomic(CommOSAtomic *atomic,
		   int val)
{
	atomic_set(atomic, val);
}



static inline int
CommOS_ReadAtomic(CommOSAtomic *atomic)
{
	return atomic_read(atomic);
}



static inline int
CommOS_AddReturnAtomic(CommOSAtomic *atomic,
		       int val)
{
	return atomic_add_return(val, atomic);
}



static inline int
CommOS_SubReturnAtomic(CommOSAtomic *atomic,
		       int val)
{
	return atomic_sub_return(val, atomic);
}



static inline void
CommOS_SpinlockInit(CommOSSpinlock *lock)
{
	spin_lock_init(lock);
}



static inline void
CommOS_SpinLockBH(CommOSSpinlock *lock)
{
	spin_lock_bh(lock);
}



static inline int
CommOS_SpinTrylockBH(CommOSSpinlock *lock)
{
	return !spin_trylock_bh(lock);
}



static inline void
CommOS_SpinUnlockBH(CommOSSpinlock *lock)
{
	spin_unlock_bh(lock);
}



static inline void
CommOS_SpinLock(CommOSSpinlock *lock)
{
	spin_lock(lock);
}



static inline int
CommOS_SpinTrylock(CommOSSpinlock *lock)
{
	return !spin_trylock(lock);
}



static inline void
CommOS_SpinUnlock(CommOSSpinlock *lock)
{
	spin_unlock(lock);
}



static inline void
CommOS_MutexInit(CommOSMutex *mutex)
{
	mutex_init(mutex);
}



static inline int
CommOS_MutexLock(CommOSMutex *mutex)
{
	return mutex_lock_interruptible(mutex);
}



static inline void
CommOS_MutexLockUninterruptible(CommOSMutex *mutex)
{
	mutex_lock(mutex);
}



static inline int
CommOS_MutexTrylock(CommOSMutex *mutex)
{
	return !mutex_trylock(mutex);
}



static inline void
CommOS_MutexUnlock(CommOSMutex *mutex)
{
	mutex_unlock(mutex);
}



static inline void
CommOS_WaitQueueInit(CommOSWaitQueue *wq)
{
	init_waitqueue_head(wq);
}



static inline int
CommOS_DoWait(CommOSWaitQueue *wq,
	      CommOSWaitConditionFunc cond,
	      void *condArg1,
	      void *condArg2,
	      unsigned long long *timeoutMillis,
	      int interruptible)
{
	int rc;
	DEFINE_WAIT(wait);
	long timeout;
#if defined(COMM_OS_LINUX_WAIT_WORKAROUND)
	long tmpTimeout;
	long retTimeout;
	const unsigned int interval = 50;
#endif

	if (!timeoutMillis)
		return -1;

	rc = cond(condArg1, condArg2);
	if (rc != 0)
		return rc;

#if defined(COMM_OS_LINUX_WAIT_WORKAROUND)
	timeout = msecs_to_jiffies(interval < *timeoutMillis ?
				       interval : (unsigned int)*timeoutMillis);
	retTimeout = msecs_to_jiffies((unsigned int)(*timeoutMillis));

	for (; retTimeout >= 0; ) {
		prepare_to_wait(wq, &wait,
		  (interruptible ? TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE));
		rc = cond(condArg1, condArg2);
		if (rc)
			break;

		if (interruptible && signal_pending(current)) {
			rc = -EINTR;
			break;
		}
		tmpTimeout = schedule_timeout(timeout);
		if (tmpTimeout)
			retTimeout -= (timeout - tmpTimeout);
		else
			retTimeout -= timeout;

		if (retTimeout < 0)
			retTimeout = 0;
	}
	finish_wait(wq, &wait);
	if (rc == 0) {
		rc = cond(condArg1, condArg2);
		if (rc && (retTimeout == 0))
			retTimeout = 1;
	}
	*timeoutMillis = (unsigned long long)jiffies_to_msecs(retTimeout);
#else 
	timeout = msecs_to_jiffies((unsigned int)(*timeoutMillis));

	for (;;) {
		prepare_to_wait(wq, &wait,
		  (interruptible ? TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE));
		rc = cond(condArg1, condArg2);
		if (rc != 0)
			break;

		if (interruptible && signal_pending(current)) {
			rc = -EINTR;
			break;
		}
		timeout = schedule_timeout(timeout);
		if (timeout == 0) {
			rc = 0;
			break;
		}
	}
	finish_wait(wq, &wait);
	if (rc == 0) {
		rc = cond(condArg1, condArg2);
		if (rc && (timeout == 0))
			timeout = 1;
	}
	*timeoutMillis = (unsigned long long)jiffies_to_msecs(timeout);
#endif

	return rc;
}



static inline int
CommOS_Wait(CommOSWaitQueue *wq,
	    CommOSWaitConditionFunc cond,
	    void *condArg1,
	    void *condArg2,
	    unsigned long long *timeoutMillis)
{
	return CommOS_DoWait(wq, cond, condArg1, condArg2, timeoutMillis, 1);
}



static inline int
CommOS_WaitUninterruptible(CommOSWaitQueue *wq,
			   CommOSWaitConditionFunc cond,
			   void *condArg1,
			   void *condArg2,
			   unsigned long long *timeoutMillis)
{
	return CommOS_DoWait(wq, cond, condArg1, condArg2, timeoutMillis, 0);
}



static inline void
CommOS_WakeUp(CommOSWaitQueue *wq)
{
	wake_up(wq);
}



static inline void *
CommOS_KmallocNoSleep(unsigned int size)
{
	return kmalloc(size, GFP_ATOMIC);
}



static inline void *
CommOS_Kmalloc(unsigned int size)
{
	return kmalloc(size, GFP_KERNEL);
}



static inline void
CommOS_Kfree(void *obj)
{
	kfree(obj);
}



static inline void
CommOS_Yield(void)
{
	cond_resched();
}



static inline unsigned long long
CommOS_GetCurrentMillis(void)
{
	return (unsigned long long)jiffies_to_msecs(jiffies);
}



static inline void
CommOS_ListInit(CommOSList *list)
{
	INIT_LIST_HEAD(list);
}



#define CommOS_ListEmpty(list) list_empty((list))



#define CommOS_ListAdd(list, elem) list_add((elem), (list))



#define CommOS_ListAddTail(list, elem) list_add_tail((elem), (list))



#define CommOS_ListDel(elem)		\
	do {				\
		list_del((elem));	\
		INIT_LIST_HEAD((elem));	\
	} while (0)



#define CommOS_ListForEach(list, item, itemListFieldName) \
	list_for_each_entry((item), (list), itemListFieldName)



#define CommOS_ListForEachSafe(list, item, tmpItem, itemListFieldName) \
	list_for_each_entry_safe((item), (tmpItem), (list), itemListFieldName)



#define CommOS_ListSplice(list, list2) list_splice((list2), (list))



#define CommOS_ListSpliceTail(list, list2) list_splice_tail((list2), (list))



static inline CommOSModule
CommOS_ModuleSelf(void)
{
	return THIS_MODULE;
}



static inline int
CommOS_ModuleGet(CommOSModule module)
{
	int rc = 0;

	if (!module)
		goto out;

	if (!try_module_get(module))
		rc = -1;

out:
	return rc;
}



static inline void
CommOS_ModulePut(CommOSModule module)
{
	if (module)
		module_put(module);
}



#define CommOS_MemBarrier smp_mb

#endif  

