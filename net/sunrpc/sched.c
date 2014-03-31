/*
 * linux/net/sunrpc/sched.c
 *
 * Scheduling for synchronous and asynchronous RPC requests.
 *
 * Copyright (C) 1996 Olaf Kirch, <okir@monad.swb.de>
 *
 * TCP NFS related read + write fixes
 * (C) 1999 Dave Airlie, University of Limerick, Ireland <airlied@linux.ie>
 */

#include <linux/module.h>

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mempool.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/freezer.h>

#include <linux/sunrpc/clnt.h>

#include "sunrpc.h"

#ifdef RPC_DEBUG
#define RPCDBG_FACILITY		RPCDBG_SCHED
#endif

#define CREATE_TRACE_POINTS
#include <trace/events/sunrpc.h>

#define RPC_BUFFER_MAXSIZE	(2048)
#define RPC_BUFFER_POOLSIZE	(8)
#define RPC_TASK_POOLSIZE	(8)
static struct kmem_cache	*rpc_task_slabp __read_mostly;
static struct kmem_cache	*rpc_buffer_slabp __read_mostly;
static mempool_t	*rpc_task_mempool __read_mostly;
static mempool_t	*rpc_buffer_mempool __read_mostly;

static void			rpc_async_schedule(struct work_struct *);
static void			 rpc_release_task(struct rpc_task *task);
static void __rpc_queue_timer_fn(unsigned long ptr);

static struct rpc_wait_queue delay_queue;

struct workqueue_struct *rpciod_workqueue;

static void
__rpc_disable_timer(struct rpc_wait_queue *queue, struct rpc_task *task)
{
	if (task->tk_timeout == 0)
		return;
	dprintk("RPC: %5u disabling timer\n", task->tk_pid);
	task->tk_timeout = 0;
	list_del(&task->u.tk_wait.timer_list);
	if (list_empty(&queue->timer_list.list))
		del_timer(&queue->timer_list.timer);
}

static void
rpc_set_queue_timer(struct rpc_wait_queue *queue, unsigned long expires)
{
	queue->timer_list.expires = expires;
	mod_timer(&queue->timer_list.timer, expires);
}

static void
__rpc_add_timer(struct rpc_wait_queue *queue, struct rpc_task *task)
{
	if (!task->tk_timeout)
		return;

	dprintk("RPC: %5u setting alarm for %lu ms\n",
			task->tk_pid, task->tk_timeout * 1000 / HZ);

	task->u.tk_wait.expires = jiffies + task->tk_timeout;
	if (list_empty(&queue->timer_list.list) || time_before(task->u.tk_wait.expires, queue->timer_list.expires))
		rpc_set_queue_timer(queue, task->u.tk_wait.expires);
	list_add(&task->u.tk_wait.timer_list, &queue->timer_list.list);
}

static void __rpc_add_wait_queue_priority(struct rpc_wait_queue *queue,
		struct rpc_task *task,
		unsigned char queue_priority)
{
	struct list_head *q;
	struct rpc_task *t;

	INIT_LIST_HEAD(&task->u.tk_wait.links);
	q = &queue->tasks[queue_priority];
	if (unlikely(queue_priority > queue->maxpriority))
		q = &queue->tasks[queue->maxpriority];
	list_for_each_entry(t, q, u.tk_wait.list) {
		if (t->tk_owner == task->tk_owner) {
			list_add_tail(&task->u.tk_wait.list, &t->u.tk_wait.links);
			return;
		}
	}
	list_add_tail(&task->u.tk_wait.list, q);
}

static void __rpc_add_wait_queue(struct rpc_wait_queue *queue,
		struct rpc_task *task,
		unsigned char queue_priority)
{
	BUG_ON (RPC_IS_QUEUED(task));

	if (RPC_IS_PRIORITY(queue))
		__rpc_add_wait_queue_priority(queue, task, queue_priority);
	else if (RPC_IS_SWAPPER(task))
		list_add(&task->u.tk_wait.list, &queue->tasks[0]);
	else
		list_add_tail(&task->u.tk_wait.list, &queue->tasks[0]);
	task->tk_waitqueue = queue;
	queue->qlen++;
	rpc_set_queued(task);

	dprintk("RPC: %5u added to queue %p \"%s\"\n",
			task->tk_pid, queue, rpc_qname(queue));
}

static void __rpc_remove_wait_queue_priority(struct rpc_task *task)
{
	struct rpc_task *t;

	if (!list_empty(&task->u.tk_wait.links)) {
		t = list_entry(task->u.tk_wait.links.next, struct rpc_task, u.tk_wait.list);
		list_move(&t->u.tk_wait.list, &task->u.tk_wait.list);
		list_splice_init(&task->u.tk_wait.links, &t->u.tk_wait.links);
	}
}

static void __rpc_remove_wait_queue(struct rpc_wait_queue *queue, struct rpc_task *task)
{
	__rpc_disable_timer(queue, task);
	if (RPC_IS_PRIORITY(queue))
		__rpc_remove_wait_queue_priority(task);
	list_del(&task->u.tk_wait.list);
	queue->qlen--;
	dprintk("RPC: %5u removed from queue %p \"%s\"\n",
			task->tk_pid, queue, rpc_qname(queue));
}

static inline void rpc_set_waitqueue_priority(struct rpc_wait_queue *queue, int priority)
{
	queue->priority = priority;
	queue->count = 1 << (priority * 2);
}

static inline void rpc_set_waitqueue_owner(struct rpc_wait_queue *queue, pid_t pid)
{
	queue->owner = pid;
	queue->nr = RPC_BATCH_COUNT;
}

static inline void rpc_reset_waitqueue_priority(struct rpc_wait_queue *queue)
{
	rpc_set_waitqueue_priority(queue, queue->maxpriority);
	rpc_set_waitqueue_owner(queue, 0);
}

static void __rpc_init_priority_wait_queue(struct rpc_wait_queue *queue, const char *qname, unsigned char nr_queues)
{
	int i;

	spin_lock_init(&queue->lock);
	for (i = 0; i < ARRAY_SIZE(queue->tasks); i++)
		INIT_LIST_HEAD(&queue->tasks[i]);
	queue->maxpriority = nr_queues - 1;
	rpc_reset_waitqueue_priority(queue);
	queue->qlen = 0;
	setup_timer(&queue->timer_list.timer, __rpc_queue_timer_fn, (unsigned long)queue);
	INIT_LIST_HEAD(&queue->timer_list.list);
	rpc_assign_waitqueue_name(queue, qname);
}

void rpc_init_priority_wait_queue(struct rpc_wait_queue *queue, const char *qname)
{
	__rpc_init_priority_wait_queue(queue, qname, RPC_NR_PRIORITY);
}
EXPORT_SYMBOL_GPL(rpc_init_priority_wait_queue);

void rpc_init_wait_queue(struct rpc_wait_queue *queue, const char *qname)
{
	__rpc_init_priority_wait_queue(queue, qname, 1);
}
EXPORT_SYMBOL_GPL(rpc_init_wait_queue);

void rpc_destroy_wait_queue(struct rpc_wait_queue *queue)
{
	del_timer_sync(&queue->timer_list.timer);
}
EXPORT_SYMBOL_GPL(rpc_destroy_wait_queue);

static int rpc_wait_bit_killable(void *word)
{
	if (fatal_signal_pending(current))
		return -ERESTARTSYS;
	freezable_schedule();
	return 0;
}

#ifdef RPC_DEBUG
static void rpc_task_set_debuginfo(struct rpc_task *task)
{
	static atomic_t rpc_pid;

	task->tk_pid = atomic_inc_return(&rpc_pid);
}
#else
static inline void rpc_task_set_debuginfo(struct rpc_task *task)
{
}
#endif

static void rpc_set_active(struct rpc_task *task)
{
	trace_rpc_task_begin(task->tk_client, task, NULL);

	rpc_task_set_debuginfo(task);
	set_bit(RPC_TASK_ACTIVE, &task->tk_runstate);
}

static int rpc_complete_task(struct rpc_task *task)
{
	void *m = &task->tk_runstate;
	wait_queue_head_t *wq = bit_waitqueue(m, RPC_TASK_ACTIVE);
	struct wait_bit_key k = __WAIT_BIT_KEY_INITIALIZER(m, RPC_TASK_ACTIVE);
	unsigned long flags;
	int ret;

	trace_rpc_task_complete(task->tk_client, task, NULL);

	spin_lock_irqsave(&wq->lock, flags);
	clear_bit(RPC_TASK_ACTIVE, &task->tk_runstate);
	ret = atomic_dec_and_test(&task->tk_count);
	if (waitqueue_active(wq))
		__wake_up_locked_key(wq, TASK_NORMAL, &k);
	spin_unlock_irqrestore(&wq->lock, flags);
	return ret;
}

int __rpc_wait_for_completion_task(struct rpc_task *task, int (*action)(void *))
{
	if (action == NULL)
		action = rpc_wait_bit_killable;
	return out_of_line_wait_on_bit(&task->tk_runstate, RPC_TASK_ACTIVE,
			action, TASK_KILLABLE);
}
EXPORT_SYMBOL_GPL(__rpc_wait_for_completion_task);

static void rpc_make_runnable(struct rpc_task *task)
{
	rpc_clear_queued(task);
	if (rpc_test_and_set_running(task))
		return;
	if (RPC_IS_ASYNC(task)) {
		INIT_WORK(&task->u.tk_work, rpc_async_schedule);
		queue_work(rpciod_workqueue, &task->u.tk_work);
	} else
		wake_up_bit(&task->tk_runstate, RPC_TASK_QUEUED);
}

static void __rpc_sleep_on_priority(struct rpc_wait_queue *q,
		struct rpc_task *task,
		rpc_action action,
		unsigned char queue_priority)
{
	dprintk("RPC: %5u sleep_on(queue \"%s\" time %lu)\n",
			task->tk_pid, rpc_qname(q), jiffies);

	trace_rpc_task_sleep(task->tk_client, task, q);

	__rpc_add_wait_queue(q, task, queue_priority);

	BUG_ON(task->tk_callback != NULL);
	task->tk_callback = action;
	__rpc_add_timer(q, task);
}

void rpc_sleep_on(struct rpc_wait_queue *q, struct rpc_task *task,
				rpc_action action)
{
	
	BUG_ON(!RPC_IS_ACTIVATED(task));

	spin_lock_bh(&q->lock);
	__rpc_sleep_on_priority(q, task, action, task->tk_priority);
	spin_unlock_bh(&q->lock);
}
EXPORT_SYMBOL_GPL(rpc_sleep_on);

void rpc_sleep_on_priority(struct rpc_wait_queue *q, struct rpc_task *task,
		rpc_action action, int priority)
{
	
	BUG_ON(!RPC_IS_ACTIVATED(task));

	spin_lock_bh(&q->lock);
	__rpc_sleep_on_priority(q, task, action, priority - RPC_PRIORITY_LOW);
	spin_unlock_bh(&q->lock);
}

static void __rpc_do_wake_up_task(struct rpc_wait_queue *queue, struct rpc_task *task)
{
	dprintk("RPC: %5u __rpc_wake_up_task (now %lu)\n",
			task->tk_pid, jiffies);

	
	if (!RPC_IS_ACTIVATED(task)) {
		printk(KERN_ERR "RPC: Inactive task (%p) being woken up!\n", task);
		return;
	}

	trace_rpc_task_wakeup(task->tk_client, task, queue);

	__rpc_remove_wait_queue(queue, task);

	rpc_make_runnable(task);

	dprintk("RPC:       __rpc_wake_up_task done\n");
}

static void rpc_wake_up_task_queue_locked(struct rpc_wait_queue *queue, struct rpc_task *task)
{
	if (RPC_IS_QUEUED(task) && task->tk_waitqueue == queue)
		__rpc_do_wake_up_task(queue, task);
}

int rpc_queue_empty(struct rpc_wait_queue *queue)
{
	int res;

	spin_lock_bh(&queue->lock);
	res = queue->qlen;
	spin_unlock_bh(&queue->lock);
	return res == 0;
}
EXPORT_SYMBOL_GPL(rpc_queue_empty);

void rpc_wake_up_queued_task(struct rpc_wait_queue *queue, struct rpc_task *task)
{
	spin_lock_bh(&queue->lock);
	rpc_wake_up_task_queue_locked(queue, task);
	spin_unlock_bh(&queue->lock);
}
EXPORT_SYMBOL_GPL(rpc_wake_up_queued_task);

static struct rpc_task *__rpc_find_next_queued_priority(struct rpc_wait_queue *queue)
{
	struct list_head *q;
	struct rpc_task *task;

	q = &queue->tasks[queue->priority];
	if (!list_empty(q)) {
		task = list_entry(q->next, struct rpc_task, u.tk_wait.list);
		if (queue->owner == task->tk_owner) {
			if (--queue->nr)
				goto out;
			list_move_tail(&task->u.tk_wait.list, q);
		}
		if (--queue->count)
			goto new_owner;
	}

	do {
		if (q == &queue->tasks[0])
			q = &queue->tasks[queue->maxpriority];
		else
			q = q - 1;
		if (!list_empty(q)) {
			task = list_entry(q->next, struct rpc_task, u.tk_wait.list);
			goto new_queue;
		}
	} while (q != &queue->tasks[queue->priority]);

	rpc_reset_waitqueue_priority(queue);
	return NULL;

new_queue:
	rpc_set_waitqueue_priority(queue, (unsigned int)(q - &queue->tasks[0]));
new_owner:
	rpc_set_waitqueue_owner(queue, task->tk_owner);
out:
	return task;
}

static struct rpc_task *__rpc_find_next_queued(struct rpc_wait_queue *queue)
{
	if (RPC_IS_PRIORITY(queue))
		return __rpc_find_next_queued_priority(queue);
	if (!list_empty(&queue->tasks[0]))
		return list_first_entry(&queue->tasks[0], struct rpc_task, u.tk_wait.list);
	return NULL;
}

struct rpc_task *rpc_wake_up_first(struct rpc_wait_queue *queue,
		bool (*func)(struct rpc_task *, void *), void *data)
{
	struct rpc_task	*task = NULL;

	dprintk("RPC:       wake_up_first(%p \"%s\")\n",
			queue, rpc_qname(queue));
	spin_lock_bh(&queue->lock);
	task = __rpc_find_next_queued(queue);
	if (task != NULL) {
		if (func(task, data))
			rpc_wake_up_task_queue_locked(queue, task);
		else
			task = NULL;
	}
	spin_unlock_bh(&queue->lock);

	return task;
}
EXPORT_SYMBOL_GPL(rpc_wake_up_first);

static bool rpc_wake_up_next_func(struct rpc_task *task, void *data)
{
	return true;
}

struct rpc_task *rpc_wake_up_next(struct rpc_wait_queue *queue)
{
	return rpc_wake_up_first(queue, rpc_wake_up_next_func, NULL);
}
EXPORT_SYMBOL_GPL(rpc_wake_up_next);

void rpc_wake_up(struct rpc_wait_queue *queue)
{
	struct list_head *head;

	spin_lock_bh(&queue->lock);
	head = &queue->tasks[queue->maxpriority];
	for (;;) {
		while (!list_empty(head)) {
			struct rpc_task *task;
			task = list_first_entry(head,
					struct rpc_task,
					u.tk_wait.list);
			rpc_wake_up_task_queue_locked(queue, task);
		}
		if (head == &queue->tasks[0])
			break;
		head--;
	}
	spin_unlock_bh(&queue->lock);
}
EXPORT_SYMBOL_GPL(rpc_wake_up);

void rpc_wake_up_status(struct rpc_wait_queue *queue, int status)
{
	struct list_head *head;

	spin_lock_bh(&queue->lock);
	head = &queue->tasks[queue->maxpriority];
	for (;;) {
		while (!list_empty(head)) {
			struct rpc_task *task;
			task = list_first_entry(head,
					struct rpc_task,
					u.tk_wait.list);
			task->tk_status = status;
			rpc_wake_up_task_queue_locked(queue, task);
		}
		if (head == &queue->tasks[0])
			break;
		head--;
	}
	spin_unlock_bh(&queue->lock);
}
EXPORT_SYMBOL_GPL(rpc_wake_up_status);

static void __rpc_queue_timer_fn(unsigned long ptr)
{
	struct rpc_wait_queue *queue = (struct rpc_wait_queue *)ptr;
	struct rpc_task *task, *n;
	unsigned long expires, now, timeo;

	spin_lock(&queue->lock);
	expires = now = jiffies;
	list_for_each_entry_safe(task, n, &queue->timer_list.list, u.tk_wait.timer_list) {
		timeo = task->u.tk_wait.expires;
		if (time_after_eq(now, timeo)) {
			dprintk("RPC: %5u timeout\n", task->tk_pid);
			task->tk_status = -ETIMEDOUT;
			rpc_wake_up_task_queue_locked(queue, task);
			continue;
		}
		if (expires == now || time_after(expires, timeo))
			expires = timeo;
	}
	if (!list_empty(&queue->timer_list.list))
		rpc_set_queue_timer(queue, expires);
	spin_unlock(&queue->lock);
}

static void __rpc_atrun(struct rpc_task *task)
{
	task->tk_status = 0;
}

void rpc_delay(struct rpc_task *task, unsigned long delay)
{
	task->tk_timeout = delay;
	rpc_sleep_on(&delay_queue, task, __rpc_atrun);
}
EXPORT_SYMBOL_GPL(rpc_delay);

void rpc_prepare_task(struct rpc_task *task)
{
	task->tk_ops->rpc_call_prepare(task, task->tk_calldata);
}

static void
rpc_init_task_statistics(struct rpc_task *task)
{
	
	task->tk_garb_retry = 2;
	task->tk_cred_retry = 2;
	task->tk_rebind_retry = 2;

	
	task->tk_start = ktime_get();
}

static void
rpc_reset_task_statistics(struct rpc_task *task)
{
	task->tk_timeouts = 0;
	task->tk_flags &= ~(RPC_CALL_MAJORSEEN|RPC_TASK_KILLED|RPC_TASK_SENT);

	rpc_init_task_statistics(task);
}

void rpc_exit_task(struct rpc_task *task)
{
	task->tk_action = NULL;
	if (task->tk_ops->rpc_call_done != NULL) {
		task->tk_ops->rpc_call_done(task, task->tk_calldata);
		if (task->tk_action != NULL) {
			WARN_ON(RPC_ASSASSINATED(task));
			
			xprt_release(task);
			rpc_reset_task_statistics(task);
		}
	}
}

void rpc_exit(struct rpc_task *task, int status)
{
	task->tk_status = status;
	task->tk_action = rpc_exit_task;
	if (RPC_IS_QUEUED(task))
		rpc_wake_up_queued_task(task->tk_waitqueue, task);
}
EXPORT_SYMBOL_GPL(rpc_exit);

void rpc_release_calldata(const struct rpc_call_ops *ops, void *calldata)
{
	if (ops->rpc_release != NULL)
		ops->rpc_release(calldata);
}

static void __rpc_execute(struct rpc_task *task)
{
	struct rpc_wait_queue *queue;
	int task_is_async = RPC_IS_ASYNC(task);
	int status = 0;

	dprintk("RPC: %5u __rpc_execute flags=0x%x\n",
			task->tk_pid, task->tk_flags);

	BUG_ON(RPC_IS_QUEUED(task));

	for (;;) {
		void (*do_action)(struct rpc_task *);

		do_action = task->tk_callback;
		task->tk_callback = NULL;
		if (do_action == NULL) {
			do_action = task->tk_action;
			if (do_action == NULL)
				break;
		}
		trace_rpc_task_run_action(task->tk_client, task, task->tk_action);
		do_action(task);

		if (!RPC_IS_QUEUED(task))
			continue;
		queue = task->tk_waitqueue;
		spin_lock_bh(&queue->lock);
		if (!RPC_IS_QUEUED(task)) {
			spin_unlock_bh(&queue->lock);
			continue;
		}
		rpc_clear_running(task);
		spin_unlock_bh(&queue->lock);
		if (task_is_async)
			return;

		
		dprintk("RPC: %5u sync task going to sleep\n", task->tk_pid);
		status = out_of_line_wait_on_bit(&task->tk_runstate,
				RPC_TASK_QUEUED, rpc_wait_bit_killable,
				TASK_KILLABLE);
		if (status == -ERESTARTSYS) {
			dprintk("RPC: %5u got signal\n", task->tk_pid);
			task->tk_flags |= RPC_TASK_KILLED;
			rpc_exit(task, -ERESTARTSYS);
		}
		rpc_set_running(task);
		dprintk("RPC: %5u sync task resuming\n", task->tk_pid);
	}

	dprintk("RPC: %5u return %d, status %d\n", task->tk_pid, status,
			task->tk_status);
	
	rpc_release_task(task);
}

void rpc_execute(struct rpc_task *task)
{
	rpc_set_active(task);
	rpc_make_runnable(task);
	if (!RPC_IS_ASYNC(task))
		__rpc_execute(task);
}

static void rpc_async_schedule(struct work_struct *work)
{
	__rpc_execute(container_of(work, struct rpc_task, u.tk_work));
}

void *rpc_malloc(struct rpc_task *task, size_t size)
{
	struct rpc_buffer *buf;
	gfp_t gfp = RPC_IS_SWAPPER(task) ? GFP_ATOMIC : GFP_NOWAIT;

	size += sizeof(struct rpc_buffer);
	if (size <= RPC_BUFFER_MAXSIZE)
		buf = mempool_alloc(rpc_buffer_mempool, gfp);
	else
		buf = kmalloc(size, gfp);

	if (!buf)
		return NULL;

	buf->len = size;
	dprintk("RPC: %5u allocated buffer of size %zu at %p\n",
			task->tk_pid, size, buf);
	return &buf->data;
}
EXPORT_SYMBOL_GPL(rpc_malloc);

void rpc_free(void *buffer)
{
	size_t size;
	struct rpc_buffer *buf;

	if (!buffer)
		return;

	buf = container_of(buffer, struct rpc_buffer, data);
	size = buf->len;

	dprintk("RPC:       freeing buffer of size %zu at %p\n",
			size, buf);

	if (size <= RPC_BUFFER_MAXSIZE)
		mempool_free(buf, rpc_buffer_mempool);
	else
		kfree(buf);
}
EXPORT_SYMBOL_GPL(rpc_free);

static void rpc_init_task(struct rpc_task *task, const struct rpc_task_setup *task_setup_data)
{
	memset(task, 0, sizeof(*task));
	atomic_set(&task->tk_count, 1);
	task->tk_flags  = task_setup_data->flags;
	task->tk_ops = task_setup_data->callback_ops;
	task->tk_calldata = task_setup_data->callback_data;
	INIT_LIST_HEAD(&task->tk_task);

	task->tk_priority = task_setup_data->priority - RPC_PRIORITY_LOW;
	task->tk_owner = current->tgid;

	
	task->tk_workqueue = task_setup_data->workqueue;

	if (task->tk_ops->rpc_call_prepare != NULL)
		task->tk_action = rpc_prepare_task;

	rpc_init_task_statistics(task);

	dprintk("RPC:       new task initialized, procpid %u\n",
				task_pid_nr(current));
}

static struct rpc_task *
rpc_alloc_task(void)
{
	return (struct rpc_task *)mempool_alloc(rpc_task_mempool, GFP_NOFS);
}

struct rpc_task *rpc_new_task(const struct rpc_task_setup *setup_data)
{
	struct rpc_task	*task = setup_data->task;
	unsigned short flags = 0;

	if (task == NULL) {
		task = rpc_alloc_task();
		if (task == NULL) {
			rpc_release_calldata(setup_data->callback_ops,
					setup_data->callback_data);
			return ERR_PTR(-ENOMEM);
		}
		flags = RPC_TASK_DYNAMIC;
	}

	rpc_init_task(task, setup_data);
	task->tk_flags |= flags;
	dprintk("RPC:       allocated task %p\n", task);
	return task;
}

static void rpc_free_task(struct rpc_task *task)
{
	const struct rpc_call_ops *tk_ops = task->tk_ops;
	void *calldata = task->tk_calldata;

	if (task->tk_flags & RPC_TASK_DYNAMIC) {
		dprintk("RPC: %5u freeing task\n", task->tk_pid);
		mempool_free(task, rpc_task_mempool);
	}
	rpc_release_calldata(tk_ops, calldata);
}

static void rpc_async_release(struct work_struct *work)
{
	rpc_free_task(container_of(work, struct rpc_task, u.tk_work));
}

static void rpc_release_resources_task(struct rpc_task *task)
{
	if (task->tk_rqstp)
		xprt_release(task);
	if (task->tk_msg.rpc_cred) {
		put_rpccred(task->tk_msg.rpc_cred);
		task->tk_msg.rpc_cred = NULL;
	}
	rpc_task_release_client(task);
}

static void rpc_final_put_task(struct rpc_task *task,
		struct workqueue_struct *q)
{
	if (q != NULL) {
		INIT_WORK(&task->u.tk_work, rpc_async_release);
		queue_work(q, &task->u.tk_work);
	} else
		rpc_free_task(task);
}

static void rpc_do_put_task(struct rpc_task *task, struct workqueue_struct *q)
{
	if (atomic_dec_and_test(&task->tk_count)) {
		rpc_release_resources_task(task);
		rpc_final_put_task(task, q);
	}
}

void rpc_put_task(struct rpc_task *task)
{
	rpc_do_put_task(task, NULL);
}
EXPORT_SYMBOL_GPL(rpc_put_task);

void rpc_put_task_async(struct rpc_task *task)
{
	rpc_do_put_task(task, task->tk_workqueue);
}
EXPORT_SYMBOL_GPL(rpc_put_task_async);

static void rpc_release_task(struct rpc_task *task)
{
	dprintk("RPC: %5u release task\n", task->tk_pid);

	BUG_ON (RPC_IS_QUEUED(task));

	rpc_release_resources_task(task);

	if (atomic_read(&task->tk_count) != 1 + !RPC_IS_ASYNC(task)) {
		
		if (!rpc_complete_task(task))
			return;
	} else {
		if (!atomic_dec_and_test(&task->tk_count))
			return;
	}
	rpc_final_put_task(task, task->tk_workqueue);
}

int rpciod_up(void)
{
	return try_module_get(THIS_MODULE) ? 0 : -EINVAL;
}

void rpciod_down(void)
{
	module_put(THIS_MODULE);
}

static int rpciod_start(void)
{
	struct workqueue_struct *wq;

	dprintk("RPC:       creating workqueue rpciod\n");
	wq = alloc_workqueue("rpciod", WQ_MEM_RECLAIM, 0);
	rpciod_workqueue = wq;
	return rpciod_workqueue != NULL;
}

static void rpciod_stop(void)
{
	struct workqueue_struct *wq = NULL;

	if (rpciod_workqueue == NULL)
		return;
	dprintk("RPC:       destroying workqueue rpciod\n");

	wq = rpciod_workqueue;
	rpciod_workqueue = NULL;
	destroy_workqueue(wq);
}

void
rpc_destroy_mempool(void)
{
	rpciod_stop();
	if (rpc_buffer_mempool)
		mempool_destroy(rpc_buffer_mempool);
	if (rpc_task_mempool)
		mempool_destroy(rpc_task_mempool);
	if (rpc_task_slabp)
		kmem_cache_destroy(rpc_task_slabp);
	if (rpc_buffer_slabp)
		kmem_cache_destroy(rpc_buffer_slabp);
	rpc_destroy_wait_queue(&delay_queue);
}

int
rpc_init_mempool(void)
{
	rpc_init_wait_queue(&delay_queue, "delayq");
	if (!rpciod_start())
		goto err_nomem;

	rpc_task_slabp = kmem_cache_create("rpc_tasks",
					     sizeof(struct rpc_task),
					     0, SLAB_HWCACHE_ALIGN,
					     NULL);
	if (!rpc_task_slabp)
		goto err_nomem;
	rpc_buffer_slabp = kmem_cache_create("rpc_buffers",
					     RPC_BUFFER_MAXSIZE,
					     0, SLAB_HWCACHE_ALIGN,
					     NULL);
	if (!rpc_buffer_slabp)
		goto err_nomem;
	rpc_task_mempool = mempool_create_slab_pool(RPC_TASK_POOLSIZE,
						    rpc_task_slabp);
	if (!rpc_task_mempool)
		goto err_nomem;
	rpc_buffer_mempool = mempool_create_slab_pool(RPC_BUFFER_POOLSIZE,
						      rpc_buffer_slabp);
	if (!rpc_buffer_mempool)
		goto err_nomem;
	return 0;
err_nomem:
	rpc_destroy_mempool();
	return -ENOMEM;
}
