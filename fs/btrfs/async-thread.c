/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/freezer.h>
#include "async-thread.h"

#define WORK_QUEUED_BIT 0
#define WORK_DONE_BIT 1
#define WORK_ORDER_DONE_BIT 2
#define WORK_HIGH_PRIO_BIT 3

struct btrfs_worker_thread {
	
	struct btrfs_workers *workers;

	
	struct list_head pending;
	struct list_head prio_pending;

	
	struct list_head worker_list;

	
	struct task_struct *task;

	
	atomic_t num_pending;

	
	atomic_t refs;

	unsigned long sequence;

	
	spinlock_t lock;

	
	int working;

	
	int idle;
};

static int __btrfs_start_workers(struct btrfs_workers *workers);

struct worker_start {
	struct btrfs_work work;
	struct btrfs_workers *queue;
};

static void start_new_worker_func(struct btrfs_work *work)
{
	struct worker_start *start;
	start = container_of(work, struct worker_start, work);
	__btrfs_start_workers(start->queue);
	kfree(start);
}

static void check_idle_worker(struct btrfs_worker_thread *worker)
{
	if (!worker->idle && atomic_read(&worker->num_pending) <
	    worker->workers->idle_thresh / 2) {
		unsigned long flags;
		spin_lock_irqsave(&worker->workers->lock, flags);
		worker->idle = 1;

		
		if (!list_empty(&worker->worker_list)) {
			list_move(&worker->worker_list,
				 &worker->workers->idle_list);
		}
		spin_unlock_irqrestore(&worker->workers->lock, flags);
	}
}

static void check_busy_worker(struct btrfs_worker_thread *worker)
{
	if (worker->idle && atomic_read(&worker->num_pending) >=
	    worker->workers->idle_thresh) {
		unsigned long flags;
		spin_lock_irqsave(&worker->workers->lock, flags);
		worker->idle = 0;

		if (!list_empty(&worker->worker_list)) {
			list_move_tail(&worker->worker_list,
				      &worker->workers->worker_list);
		}
		spin_unlock_irqrestore(&worker->workers->lock, flags);
	}
}

static void check_pending_worker_creates(struct btrfs_worker_thread *worker)
{
	struct btrfs_workers *workers = worker->workers;
	struct worker_start *start;
	unsigned long flags;

	rmb();
	if (!workers->atomic_start_pending)
		return;

	start = kzalloc(sizeof(*start), GFP_NOFS);
	if (!start)
		return;

	start->work.func = start_new_worker_func;
	start->queue = workers;

	spin_lock_irqsave(&workers->lock, flags);
	if (!workers->atomic_start_pending)
		goto out;

	workers->atomic_start_pending = 0;
	if (workers->num_workers + workers->num_workers_starting >=
	    workers->max_workers)
		goto out;

	workers->num_workers_starting += 1;
	spin_unlock_irqrestore(&workers->lock, flags);
	btrfs_queue_worker(workers->atomic_worker_start, &start->work);
	return;

out:
	kfree(start);
	spin_unlock_irqrestore(&workers->lock, flags);
}

static noinline void run_ordered_completions(struct btrfs_workers *workers,
					    struct btrfs_work *work)
{
	if (!workers->ordered)
		return;

	set_bit(WORK_DONE_BIT, &work->flags);

	spin_lock(&workers->order_lock);

	while (1) {
		if (!list_empty(&workers->prio_order_list)) {
			work = list_entry(workers->prio_order_list.next,
					  struct btrfs_work, order_list);
		} else if (!list_empty(&workers->order_list)) {
			work = list_entry(workers->order_list.next,
					  struct btrfs_work, order_list);
		} else {
			break;
		}
		if (!test_bit(WORK_DONE_BIT, &work->flags))
			break;

		if (test_and_set_bit(WORK_ORDER_DONE_BIT, &work->flags))
			break;

		spin_unlock(&workers->order_lock);

		work->ordered_func(work);

		
		spin_lock(&workers->order_lock);
		list_del(&work->order_list);
		work->ordered_free(work);
	}

	spin_unlock(&workers->order_lock);
}

static void put_worker(struct btrfs_worker_thread *worker)
{
	if (atomic_dec_and_test(&worker->refs))
		kfree(worker);
}

static int try_worker_shutdown(struct btrfs_worker_thread *worker)
{
	int freeit = 0;

	spin_lock_irq(&worker->lock);
	spin_lock(&worker->workers->lock);
	if (worker->workers->num_workers > 1 &&
	    worker->idle &&
	    !worker->working &&
	    !list_empty(&worker->worker_list) &&
	    list_empty(&worker->prio_pending) &&
	    list_empty(&worker->pending) &&
	    atomic_read(&worker->num_pending) == 0) {
		freeit = 1;
		list_del_init(&worker->worker_list);
		worker->workers->num_workers--;
	}
	spin_unlock(&worker->workers->lock);
	spin_unlock_irq(&worker->lock);

	if (freeit)
		put_worker(worker);
	return freeit;
}

static struct btrfs_work *get_next_work(struct btrfs_worker_thread *worker,
					struct list_head *prio_head,
					struct list_head *head)
{
	struct btrfs_work *work = NULL;
	struct list_head *cur = NULL;

	if(!list_empty(prio_head))
		cur = prio_head->next;

	smp_mb();
	if (!list_empty(&worker->prio_pending))
		goto refill;

	if (!list_empty(head))
		cur = head->next;

	if (cur)
		goto out;

refill:
	spin_lock_irq(&worker->lock);
	list_splice_tail_init(&worker->prio_pending, prio_head);
	list_splice_tail_init(&worker->pending, head);

	if (!list_empty(prio_head))
		cur = prio_head->next;
	else if (!list_empty(head))
		cur = head->next;
	spin_unlock_irq(&worker->lock);

	if (!cur)
		goto out_fail;

out:
	work = list_entry(cur, struct btrfs_work, list);

out_fail:
	return work;
}

static int worker_loop(void *arg)
{
	struct btrfs_worker_thread *worker = arg;
	struct list_head head;
	struct list_head prio_head;
	struct btrfs_work *work;

	INIT_LIST_HEAD(&head);
	INIT_LIST_HEAD(&prio_head);

	do {
again:
		while (1) {


			work = get_next_work(worker, &prio_head, &head);
			if (!work)
				break;

			list_del(&work->list);
			clear_bit(WORK_QUEUED_BIT, &work->flags);

			work->worker = worker;

			work->func(work);

			atomic_dec(&worker->num_pending);
			run_ordered_completions(worker->workers, work);

			check_pending_worker_creates(worker);
			cond_resched();
		}

		spin_lock_irq(&worker->lock);
		check_idle_worker(worker);

		if (freezing(current)) {
			worker->working = 0;
			spin_unlock_irq(&worker->lock);
			try_to_freeze();
		} else {
			spin_unlock_irq(&worker->lock);
			if (!kthread_should_stop()) {
				cpu_relax();
				smp_mb();
				if (!list_empty(&worker->pending) ||
				    !list_empty(&worker->prio_pending))
					continue;

				schedule_timeout(1);
				smp_mb();
				if (!list_empty(&worker->pending) ||
				    !list_empty(&worker->prio_pending))
					continue;

				if (kthread_should_stop())
					break;

				
				spin_lock_irq(&worker->lock);
				set_current_state(TASK_INTERRUPTIBLE);
				if (!list_empty(&worker->pending) ||
				    !list_empty(&worker->prio_pending)) {
					spin_unlock_irq(&worker->lock);
					set_current_state(TASK_RUNNING);
					goto again;
				}

				worker->working = 0;
				spin_unlock_irq(&worker->lock);

				if (!kthread_should_stop()) {
					schedule_timeout(HZ * 120);
					if (!worker->working &&
					    try_worker_shutdown(worker)) {
						return 0;
					}
				}
			}
			__set_current_state(TASK_RUNNING);
		}
	} while (!kthread_should_stop());
	return 0;
}

void btrfs_stop_workers(struct btrfs_workers *workers)
{
	struct list_head *cur;
	struct btrfs_worker_thread *worker;
	int can_stop;

	spin_lock_irq(&workers->lock);
	list_splice_init(&workers->idle_list, &workers->worker_list);
	while (!list_empty(&workers->worker_list)) {
		cur = workers->worker_list.next;
		worker = list_entry(cur, struct btrfs_worker_thread,
				    worker_list);

		atomic_inc(&worker->refs);
		workers->num_workers -= 1;
		if (!list_empty(&worker->worker_list)) {
			list_del_init(&worker->worker_list);
			put_worker(worker);
			can_stop = 1;
		} else
			can_stop = 0;
		spin_unlock_irq(&workers->lock);
		if (can_stop)
			kthread_stop(worker->task);
		spin_lock_irq(&workers->lock);
		put_worker(worker);
	}
	spin_unlock_irq(&workers->lock);
}

void btrfs_init_workers(struct btrfs_workers *workers, char *name, int max,
			struct btrfs_workers *async_helper)
{
	workers->num_workers = 0;
	workers->num_workers_starting = 0;
	INIT_LIST_HEAD(&workers->worker_list);
	INIT_LIST_HEAD(&workers->idle_list);
	INIT_LIST_HEAD(&workers->order_list);
	INIT_LIST_HEAD(&workers->prio_order_list);
	spin_lock_init(&workers->lock);
	spin_lock_init(&workers->order_lock);
	workers->max_workers = max;
	workers->idle_thresh = 32;
	workers->name = name;
	workers->ordered = 0;
	workers->atomic_start_pending = 0;
	workers->atomic_worker_start = async_helper;
}

static int __btrfs_start_workers(struct btrfs_workers *workers)
{
	struct btrfs_worker_thread *worker;
	int ret = 0;

	worker = kzalloc(sizeof(*worker), GFP_NOFS);
	if (!worker) {
		ret = -ENOMEM;
		goto fail;
	}

	INIT_LIST_HEAD(&worker->pending);
	INIT_LIST_HEAD(&worker->prio_pending);
	INIT_LIST_HEAD(&worker->worker_list);
	spin_lock_init(&worker->lock);

	atomic_set(&worker->num_pending, 0);
	atomic_set(&worker->refs, 1);
	worker->workers = workers;
	worker->task = kthread_run(worker_loop, worker,
				   "btrfs-%s-%d", workers->name,
				   workers->num_workers + 1);
	if (IS_ERR(worker->task)) {
		ret = PTR_ERR(worker->task);
		kfree(worker);
		goto fail;
	}
	spin_lock_irq(&workers->lock);
	list_add_tail(&worker->worker_list, &workers->idle_list);
	worker->idle = 1;
	workers->num_workers++;
	workers->num_workers_starting--;
	WARN_ON(workers->num_workers_starting < 0);
	spin_unlock_irq(&workers->lock);

	return 0;
fail:
	spin_lock_irq(&workers->lock);
	workers->num_workers_starting--;
	spin_unlock_irq(&workers->lock);
	return ret;
}

int btrfs_start_workers(struct btrfs_workers *workers)
{
	spin_lock_irq(&workers->lock);
	workers->num_workers_starting++;
	spin_unlock_irq(&workers->lock);
	return __btrfs_start_workers(workers);
}

static struct btrfs_worker_thread *next_worker(struct btrfs_workers *workers)
{
	struct btrfs_worker_thread *worker;
	struct list_head *next;
	int enforce_min;

	enforce_min = (workers->num_workers + workers->num_workers_starting) <
		workers->max_workers;

	if (!list_empty(&workers->idle_list)) {
		next = workers->idle_list.next;
		worker = list_entry(next, struct btrfs_worker_thread,
				    worker_list);
		return worker;
	}
	if (enforce_min || list_empty(&workers->worker_list))
		return NULL;

	next = workers->worker_list.next;
	worker = list_entry(next, struct btrfs_worker_thread, worker_list);
	worker->sequence++;

	if (worker->sequence % workers->idle_thresh == 0)
		list_move_tail(next, &workers->worker_list);
	return worker;
}

static struct btrfs_worker_thread *find_worker(struct btrfs_workers *workers)
{
	struct btrfs_worker_thread *worker;
	unsigned long flags;
	struct list_head *fallback;
	int ret;

	spin_lock_irqsave(&workers->lock, flags);
again:
	worker = next_worker(workers);

	if (!worker) {
		if (workers->num_workers + workers->num_workers_starting >=
		    workers->max_workers) {
			goto fallback;
		} else if (workers->atomic_worker_start) {
			workers->atomic_start_pending = 1;
			goto fallback;
		} else {
			workers->num_workers_starting++;
			spin_unlock_irqrestore(&workers->lock, flags);
			
			ret = __btrfs_start_workers(workers);
			spin_lock_irqsave(&workers->lock, flags);
			if (ret)
				goto fallback;
			goto again;
		}
	}
	goto found;

fallback:
	fallback = NULL;
	if (!list_empty(&workers->worker_list))
		fallback = workers->worker_list.next;
	if (!list_empty(&workers->idle_list))
		fallback = workers->idle_list.next;
	BUG_ON(!fallback);
	worker = list_entry(fallback,
		  struct btrfs_worker_thread, worker_list);
found:
	atomic_inc(&worker->num_pending);
	spin_unlock_irqrestore(&workers->lock, flags);
	return worker;
}

void btrfs_requeue_work(struct btrfs_work *work)
{
	struct btrfs_worker_thread *worker = work->worker;
	unsigned long flags;
	int wake = 0;

	if (test_and_set_bit(WORK_QUEUED_BIT, &work->flags))
		return;

	spin_lock_irqsave(&worker->lock, flags);
	if (test_bit(WORK_HIGH_PRIO_BIT, &work->flags))
		list_add_tail(&work->list, &worker->prio_pending);
	else
		list_add_tail(&work->list, &worker->pending);
	atomic_inc(&worker->num_pending);

	if (worker->idle) {
		spin_lock(&worker->workers->lock);
		worker->idle = 0;
		list_move_tail(&worker->worker_list,
			      &worker->workers->worker_list);
		spin_unlock(&worker->workers->lock);
	}
	if (!worker->working) {
		wake = 1;
		worker->working = 1;
	}

	if (wake)
		wake_up_process(worker->task);
	spin_unlock_irqrestore(&worker->lock, flags);
}

void btrfs_set_work_high_prio(struct btrfs_work *work)
{
	set_bit(WORK_HIGH_PRIO_BIT, &work->flags);
}

void btrfs_queue_worker(struct btrfs_workers *workers, struct btrfs_work *work)
{
	struct btrfs_worker_thread *worker;
	unsigned long flags;
	int wake = 0;

	
	if (test_and_set_bit(WORK_QUEUED_BIT, &work->flags))
		return;

	worker = find_worker(workers);
	if (workers->ordered) {
		spin_lock(&workers->order_lock);
		if (test_bit(WORK_HIGH_PRIO_BIT, &work->flags)) {
			list_add_tail(&work->order_list,
				      &workers->prio_order_list);
		} else {
			list_add_tail(&work->order_list, &workers->order_list);
		}
		spin_unlock(&workers->order_lock);
	} else {
		INIT_LIST_HEAD(&work->order_list);
	}

	spin_lock_irqsave(&worker->lock, flags);

	if (test_bit(WORK_HIGH_PRIO_BIT, &work->flags))
		list_add_tail(&work->list, &worker->prio_pending);
	else
		list_add_tail(&work->list, &worker->pending);
	check_busy_worker(worker);

	if (!worker->working)
		wake = 1;
	worker->working = 1;

	if (wake)
		wake_up_process(worker->task);
	spin_unlock_irqrestore(&worker->lock, flags);
}
