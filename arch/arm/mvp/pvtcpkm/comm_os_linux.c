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


#include "comm_os.h"

#define DISPATCH_MAX_CYCLES 8192


typedef struct workqueue_struct CommOSWorkQueue;



static int running;
static CommOSWorkQueue *dispatchWQ;
static CommOSDispatchFunc dispatch;
static CommOSWork dispatchWorksNow[NR_CPUS];
static CommOSWork dispatchWorks[NR_CPUS];
static unsigned int dispatchInterval = 1;
static unsigned int dispatchMaxCycles = 2048;
static CommOSWorkQueue *aioWQ;



static inline CommOSWorkQueue *
CreateWorkqueue(const char *name)
{
	return alloc_workqueue(name, WQ_MEM_RECLAIM, 0);
}



static inline void
DestroyWorkqueue(CommOSWorkQueue *wq)
{
	destroy_workqueue(wq);
}



static inline void
FlushDelayedWork(CommOSWork *work)
{
	flush_delayed_work(work);
}



static inline int
QueueDelayedWorkOn(int cpu,
		   CommOSWorkQueue *wq,
		   CommOSWork *work,
		   unsigned long jif)
{
	return !queue_delayed_work_on(cpu, wq, work, jif) ? -1 : 0;
}



static inline int
QueueDelayedWork(CommOSWorkQueue *wq,
		 CommOSWork *work,
		 unsigned long jif)
{
	return !queue_delayed_work(wq, work, jif) ? -1 : 0;
}



static inline void
WaitForDelayedWork(CommOSWork *work)
{
	cancel_delayed_work_sync(work);
}



static inline void
FlushWorkqueue(CommOSWorkQueue *wq)
{
	flush_workqueue(wq);
}



void
CommOS_ScheduleDisp(void)
{
	CommOSWork *work = &dispatchWorksNow[raw_smp_processor_id()];

	if (running)
		QueueDelayedWork(dispatchWQ, work, 0);
}



static void
DispatchWrapper(CommOSWork *work)
{
	unsigned int misses;

	for (misses = 0; running && (misses < dispatchMaxCycles); ) {
		

		if (!dispatch()) {
			

			misses++;
			if ((misses % 32) == 0)
				CommOS_Yield();
		} else {
			misses = 0;
		}
	}

	if (running &&
	    (work >= &dispatchWorks[0]) &&
	    (work <= &dispatchWorks[NR_CPUS - 1])) {

		QueueDelayedWork(dispatchWQ, work, dispatchInterval);
	}
}

#ifdef CONFIG_HOTPLUG_CPU


static int __cpuinit
CpuCallback(struct notifier_block *nfb,
	    unsigned long action,
	    void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		if (running)
			QueueDelayedWorkOn(cpu, dispatchWQ,
					   &dispatchWorks[cpu],
					   dispatchInterval);

		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		WaitForDelayedWork(&dispatchWorksNow[cpu]);
		WaitForDelayedWork(&dispatchWorks[cpu]);
		break;
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
		if (running)
			QueueDelayedWorkOn(cpu, dispatchWQ,
					   &dispatchWorks[cpu],
					   dispatchInterval);

		break;
	}

	return NOTIFY_OK;
}


static struct notifier_block __refdata CpuNotifier = {
	.notifier_call = CpuCallback,
};

#endif



void
CommOS_InitWork(CommOSWork *work,
		CommOSWorkFunc func)
{
	INIT_DELAYED_WORK(work, (work_func_t)func);
}


void
CommOS_FlushAIOWork(CommOSWork *work)
{
	if (aioWQ && work)
		FlushDelayedWork(work);
}



int
CommOS_ScheduleAIOWork(CommOSWork *work)
{
	if (running && aioWQ && work)
		return QueueDelayedWork(aioWQ, work, 0);

	return -1;
}



int
CommOS_StartIO(const char *dispatchTaskName,    
	       CommOSDispatchFunc dispatchFunc, 
	       unsigned int intervalMillis,     
	       unsigned int maxCycles,          
	       const char *aioTaskName)         
{
	int cpu;

	if (running) {
		CommOS_Debug(("%s: I/O tasks already running.\n", __func__));
		return 0;
	}


	if (!dispatchFunc) {
		CommOS_Log(("%s: a NULL Dispatch handler was passed.\n",
			    __func__));
		return -1;
	}
	dispatch = dispatchFunc;

	if (intervalMillis == 0)
		intervalMillis = 4;

	dispatchInterval = msecs_to_jiffies(intervalMillis);
	if (dispatchInterval < 1)
		dispatchInterval = 1;

	if (maxCycles > DISPATCH_MAX_CYCLES)
		dispatchMaxCycles = DISPATCH_MAX_CYCLES;
	else if (maxCycles > 0)
		dispatchMaxCycles = maxCycles;

	CommOS_Debug(("%s: Interval millis %u (jif:%u).\n", __func__,
		      intervalMillis, dispatchInterval));
	CommOS_Debug(("%s: Max cycles %u.\n", __func__, dispatchMaxCycles));

	dispatchWQ = CreateWorkqueue(dispatchTaskName);
	if (!dispatchWQ) {
		CommOS_Log(("%s: Couldn't create %s task(s).\n", __func__,
			    dispatchTaskName));
		return -1;
	}

	if (aioTaskName) {
		aioWQ = CreateWorkqueue(aioTaskName);
		if (!aioWQ) {
			CommOS_Log(("%s: Couldn't create %s task(s).\n",
				    __func__, aioTaskName));
			DestroyWorkqueue(dispatchWQ);
			return -1;
		}
	} else {
		aioWQ = NULL;
	}

	running = 1;
	for_each_possible_cpu(cpu) {
		CommOS_InitWork(&dispatchWorksNow[cpu], DispatchWrapper);
		CommOS_InitWork(&dispatchWorks[cpu], DispatchWrapper);
	}

#ifdef CONFIG_HOTPLUG_CPU
	register_hotcpu_notifier(&CpuNotifier);
#endif

	get_online_cpus();
	for_each_online_cpu(cpu)
		QueueDelayedWorkOn(cpu, dispatchWQ,
				   &dispatchWorks[cpu],
				   dispatchInterval);
	put_online_cpus();
	CommOS_Log(("%s: Created I/O task(s) successfully.\n", __func__));
	return 0;
}



void
CommOS_StopIO(void)
{
	int cpu;

	if (running) {
		running = 0;
		if (aioWQ) {
			FlushWorkqueue(aioWQ);
			DestroyWorkqueue(aioWQ);
			aioWQ = NULL;
		}
		FlushWorkqueue(dispatchWQ);
#ifdef CONFIG_HOTPLUG_CPU
		unregister_hotcpu_notifier(&CpuNotifier);
#endif
		for_each_possible_cpu(cpu) {
			WaitForDelayedWork(&dispatchWorksNow[cpu]);
			WaitForDelayedWork(&dispatchWorks[cpu]);
		}
		DestroyWorkqueue(dispatchWQ);
		dispatchWQ = NULL;
		CommOS_Log(("%s: I/O tasks stopped.\n", __func__));
	}
}

