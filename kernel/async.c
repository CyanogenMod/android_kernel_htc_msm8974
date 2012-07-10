/*
 * async.c: Asynchronous function calls for boot performance
 *
 * (C) Copyright 2009 Intel Corporation
 * Author: Arjan van de Ven <arjan@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */



#include <linux/async.h>
#include <linux/atomic.h>
#include <linux/ktime.h>
#include <linux/export.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

static async_cookie_t next_cookie = 1;

#define MAX_WORK	32768

static LIST_HEAD(async_pending);
static ASYNC_DOMAIN(async_running);
static DEFINE_SPINLOCK(async_lock);

struct async_entry {
	struct list_head	list;
	struct work_struct	work;
	async_cookie_t		cookie;
	async_func_ptr		*func;
	void			*data;
	struct async_domain	*running;
};

static DECLARE_WAIT_QUEUE_HEAD(async_done);

static atomic_t entry_count;


/*
 * MUST be called with the lock held!
 */
static async_cookie_t  __lowest_in_progress(struct async_domain *running)
{
	struct async_entry *entry;

	if (!list_empty(&running->domain)) {
		entry = list_first_entry(&running->domain, typeof(*entry), list);
		return entry->cookie;
	}

	list_for_each_entry(entry, &async_pending, list)
		if (entry->running == running)
			return entry->cookie;

	return next_cookie;	
}

static async_cookie_t  lowest_in_progress(struct async_domain *running)
{
	unsigned long flags;
	async_cookie_t ret;

	spin_lock_irqsave(&async_lock, flags);
	ret = __lowest_in_progress(running);
	spin_unlock_irqrestore(&async_lock, flags);
	return ret;
}

static void async_run_entry_fn(struct work_struct *work)
{
	struct async_entry *entry =
		container_of(work, struct async_entry, work);
	unsigned long flags;
	ktime_t uninitialized_var(calltime), delta, rettime;
	struct async_domain *running = entry->running;

	
	spin_lock_irqsave(&async_lock, flags);
	list_move_tail(&entry->list, &running->domain);
	spin_unlock_irqrestore(&async_lock, flags);

	
	if (initcall_debug && system_state == SYSTEM_BOOTING) {
		printk(KERN_DEBUG "calling  %lli_%pF @ %i\n",
			(long long)entry->cookie,
			entry->func, task_pid_nr(current));
		calltime = ktime_get();
	}
	entry->func(entry->data, entry->cookie);
	if (initcall_debug && system_state == SYSTEM_BOOTING) {
		rettime = ktime_get();
		delta = ktime_sub(rettime, calltime);
	}

	
	spin_lock_irqsave(&async_lock, flags);
	list_del(&entry->list);

	
	kfree(entry);
	atomic_dec(&entry_count);

	spin_unlock_irqrestore(&async_lock, flags);

	
	wake_up(&async_done);
}

static async_cookie_t __async_schedule(async_func_ptr *ptr, void *data, struct async_domain *running)
{
	struct async_entry *entry;
	unsigned long flags;
	async_cookie_t newcookie;

	
	entry = kzalloc(sizeof(struct async_entry), GFP_ATOMIC);

	if (!entry || atomic_read(&entry_count) > MAX_WORK) {
		kfree(entry);
		spin_lock_irqsave(&async_lock, flags);
		newcookie = next_cookie++;
		spin_unlock_irqrestore(&async_lock, flags);

		
		ptr(data, newcookie);
		return newcookie;
	}
	INIT_WORK(&entry->work, async_run_entry_fn);
	entry->func = ptr;
	entry->data = data;
	entry->running = running;

	spin_lock_irqsave(&async_lock, flags);
	newcookie = entry->cookie = next_cookie++;
	list_add_tail(&entry->list, &async_pending);
	atomic_inc(&entry_count);
	spin_unlock_irqrestore(&async_lock, flags);

	
	queue_work(system_unbound_wq, &entry->work);

	return newcookie;
}

async_cookie_t async_schedule(async_func_ptr *ptr, void *data)
{
	return __async_schedule(ptr, data, &async_running);
}
EXPORT_SYMBOL_GPL(async_schedule);

async_cookie_t async_schedule_domain(async_func_ptr *ptr, void *data,
				     struct async_domain *running)
{
	return __async_schedule(ptr, data, running);
}
EXPORT_SYMBOL_GPL(async_schedule_domain);

void async_synchronize_full(void)
{
	do {
		async_synchronize_cookie(next_cookie);
	} while (!list_empty(&async_running.domain) || !list_empty(&async_pending));
}
EXPORT_SYMBOL_GPL(async_synchronize_full);

/**
 * async_synchronize_full_domain - synchronize all asynchronous function within a certain domain
 * @domain: running list to synchronize on
 *
 * This function waits until all asynchronous function calls for the
 * synchronization domain specified by the running list @domain have been done.
 */
void async_synchronize_full_domain(struct async_domain *domain)
{
	async_synchronize_cookie_domain(next_cookie, domain);
}
EXPORT_SYMBOL_GPL(async_synchronize_full_domain);

/**
 * async_synchronize_cookie_domain - synchronize asynchronous function calls within a certain domain with cookie checkpointing
 * @cookie: async_cookie_t to use as checkpoint
 * @running: running list to synchronize on
 *
 * This function waits until all asynchronous function calls for the
 * synchronization domain specified by running list @running submitted
 * prior to @cookie have been done.
 */
void async_synchronize_cookie_domain(async_cookie_t cookie, struct async_domain *running)
{
	ktime_t uninitialized_var(starttime), delta, endtime;

	if (initcall_debug && system_state == SYSTEM_BOOTING) {
		printk(KERN_DEBUG "async_waiting @ %i\n", task_pid_nr(current));
		starttime = ktime_get();
	}

	wait_event(async_done, lowest_in_progress(running) >= cookie);

	if (initcall_debug && system_state == SYSTEM_BOOTING) {
		endtime = ktime_get();
		delta = ktime_sub(endtime, starttime);

		printk(KERN_DEBUG "async_continuing @ %i after %lli usec\n",
			task_pid_nr(current),
			(long long)ktime_to_ns(delta) >> 10);
	}
}
EXPORT_SYMBOL_GPL(async_synchronize_cookie_domain);

void async_synchronize_cookie(async_cookie_t cookie)
{
	async_synchronize_cookie_domain(cookie, &async_running);
}
EXPORT_SYMBOL_GPL(async_synchronize_cookie);
