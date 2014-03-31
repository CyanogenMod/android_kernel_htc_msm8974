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


#include <linux/kernel.h>

#include <linux/string.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/hardirq.h>

#include "mvp.h"

#include "arm_inline.h"
#include "coproc_defs.h"
#include "mutex_kernel.h"

#define POLL_IN_PROGRESS_FLAG (1<<(30-MUTEX_CVAR_MAX))

#define INITWAITQ(waitQ) init_waitqueue_head((wait_queue_head_t *)(waitQ))

#define WAKEUPALL(waitQ) wake_up_all((wait_queue_head_t *)(waitQ))

#define WAKEUPONE(waitQ) wake_up((wait_queue_head_t *)(waitQ))

void
Mutex_Init(Mutex *mutex)
{
	wait_queue_head_t *wq;
	int i;

	wq = kcalloc(MUTEX_CVAR_MAX + 1, sizeof(wait_queue_head_t), 0);
	FATAL_IF(wq == NULL);

	memset(mutex, 0, sizeof(*mutex));
	mutex->mtxHKVA = (HKVA)mutex;
	mutex->lockWaitQ = (HKVA)&wq[0];
	INITWAITQ(mutex->lockWaitQ);

	for (i = 0; i < MUTEX_CVAR_MAX; i++) {
		mutex->cvarWaitQs[i] = (HKVA)&wq[i + 1];
		INITWAITQ(mutex->cvarWaitQs[i]);
	}
}

static void
MutexCheckSleep(const char *file, int line)
{
#ifdef MVP_DEVEL
	static unsigned long prev_jiffy;        

#ifdef CONFIG_PREEMPT
	if (preemptible() && !irqs_disabled())
		return;
#else
	if (!irqs_disabled())
		return;
#endif
	if (time_before(jiffies, prev_jiffy + HZ) && prev_jiffy)
		return;

	prev_jiffy = jiffies;
	pr_err("BUG: sleeping function called from invalid context at %s:%d\n",
	       file, line);
	pr_err("irqs_disabled(): %d, preemtible(): %d, pid: %d, name: %s\n",
	       irqs_disabled(),
	       preemptible(),
	       current->pid, current->comm);
	dump_stack();
#endif
}

void
Mutex_Destroy(Mutex *mutex)
{
	kfree((void *)mutex->lockWaitQ);
}

int
Mutex_LockLine(Mutex *mutex,
	       MutexMode mode,
	       const char *file,
	       int line)
{
	Mutex_State newState, oldState;

	MutexCheckSleep(file, line);

	do {
lock_start:
		oldState.state = ATOMIC_GETO(mutex->state);
		newState.mode  = oldState.mode + mode;
		newState.blck  = oldState.blck;

		if ((uint32)newState.mode >= (uint32)mode) {
			if (!ATOMIC_SETIF(mutex->state, newState.state,
					  oldState.state))
				goto lock_start;

			DMB();
			mutex->line    = line;
			mutex->lineUnl = -1;
			return 0;
		}

		newState.mode = oldState.mode;
		newState.blck = oldState.blck + 1;
	} while (!ATOMIC_SETIF(mutex->state, newState.state, oldState.state));

	ATOMIC_ADDV(mutex->blocked, 1);

	 do {
		DEFINE_WAIT(waiter);

		prepare_to_wait((wait_queue_head_t *)mutex->lockWaitQ,
				&waiter,
				TASK_INTERRUPTIBLE);


set_new_state:
		oldState.state = ATOMIC_GETO(mutex->state);
		newState.mode  = oldState.mode + mode;
		newState.blck  = oldState.blck - 1;
		ASSERT(oldState.blck);

		if ((uint32)newState.mode >= (uint32)mode) {
			if (!ATOMIC_SETIF(mutex->state,
					  newState.state, oldState.state))
				goto set_new_state;

			finish_wait((wait_queue_head_t *)mutex->lockWaitQ,
				    &waiter);
			DMB();
			mutex->line    = line;
			mutex->lineUnl = -1;
			return 0;
		}

		WARN(!schedule_timeout(10*HZ),
		     "Mutex_Lock: soft lockup - stuck for 10s!\n");
		finish_wait((wait_queue_head_t *)mutex->lockWaitQ, &waiter);
	} while (!signal_pending(current));

	do {
		oldState.state = ATOMIC_GETO(mutex->state);
		newState.mode  = oldState.mode;
		newState.blck  = oldState.blck - 1;

		ASSERT(oldState.blck);
	} while (!ATOMIC_SETIF(mutex->state, newState.state, oldState.state));

	return -ERESTARTSYS;
}


void
Mutex_UnlockLine(Mutex *mutex,
		 MutexMode mode,
		 int line)
{
	Mutex_State newState, oldState;

	DMB();
	do {
		oldState.state = ATOMIC_GETO(mutex->state);
		newState.mode  = oldState.mode - mode;
		newState.blck  = oldState.blck;
		mutex->lineUnl = line;

		ASSERT(oldState.mode >= mode);
	} while (!ATOMIC_SETIF(mutex->state, newState.state, oldState.state));

	if (oldState.blck) {
		if (mode == MutexModeSH)
			WAKEUPONE(mutex->lockWaitQ);
		else
			WAKEUPALL(mutex->lockWaitQ);
	}
}


int
Mutex_UnlSleepLine(Mutex *mutex,
		   MutexMode mode,
		   uint32 cvi,
		   const char *file,
		   int line)
{
	return Mutex_UnlSleepTestLine(mutex, mode, cvi, NULL, 0, file, line);
}

int
Mutex_UnlSleepTestLine(Mutex *mutex,
		       MutexMode mode,
		       uint32 cvi,
		       AtmUInt32 *test,
		       uint32 mask,
		       const char *file,
		       int line)
{
	DEFINE_WAIT(waiter);

	MutexCheckSleep(file, line);

	ASSERT(cvi < MUTEX_CVAR_MAX);

	ATOMIC_ADDV(mutex->waiters, 1);

	prepare_to_wait_exclusive((wait_queue_head_t *)mutex->cvarWaitQs[cvi],
				  &waiter,
				  TASK_INTERRUPTIBLE);

	Mutex_Unlock(mutex, mode);

	if (test == NULL || (ATOMIC_GETO(*test) & mask) == 0)
		schedule();
	finish_wait((wait_queue_head_t *)mutex->cvarWaitQs[cvi], &waiter);

	ATOMIC_SUBV(mutex->waiters, 1);

	if (signal_pending(current))
		return -ERESTARTSYS;

	return 0;
}


void
Mutex_UnlPoll(Mutex *mutex,
	      MutexMode mode,
	      uint32 cvi,
	      void *filp,
	      void *wait)
{
	ASSERT(cvi < MUTEX_CVAR_MAX);

	poll_wait(filp, (wait_queue_head_t *)mutex->cvarWaitQs[cvi], wait);

	DMB();
	ATOMIC_ORO(mutex->waiters, (POLL_IN_PROGRESS_FLAG << cvi));

	Mutex_Unlock(mutex, mode);
}


void
Mutex_UnlWake(Mutex *mutex,
	      MutexMode mode,
	      uint32 cvi,
	      _Bool all)
{
	Mutex_Unlock(mutex, mode);
	Mutex_CondSig(mutex, cvi, all);
}


void
Mutex_CondSig(Mutex *mutex,
	      uint32 cvi,
	      _Bool all)
{
	uint32 waiters;

	ASSERT(cvi < MUTEX_CVAR_MAX);

	waiters = ATOMIC_GETO(mutex->waiters);
	if (waiters != 0) {
		wait_queue_head_t *wq =
			(wait_queue_head_t *)mutex->cvarWaitQs[cvi];

		if ((waiters >= POLL_IN_PROGRESS_FLAG) &&
		    !waitqueue_active(wq))
			ATOMIC_ANDO(mutex->waiters,
				    ~(POLL_IN_PROGRESS_FLAG << cvi));

		DMB();

		if (all)
			WAKEUPALL(mutex->cvarWaitQs[cvi]);
		else
			WAKEUPONE(mutex->cvarWaitQs[cvi]);
	}
}
