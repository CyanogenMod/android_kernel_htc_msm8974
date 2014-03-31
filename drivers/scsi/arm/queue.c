/*
 *  linux/drivers/acorn/scsi/queue.c: queue handling primitives
 *
 *  Copyright (C) 1997-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   15-Sep-1997 RMK	Created.
 *   11-Oct-1997 RMK	Corrected problem with queue_remove_exclude
 *			not updating internal linked list properly
 *			(was causing commands to go missing).
 *   30-Aug-2000 RMK	Use Linux list handling and spinlocks
 */
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/init.h>

#include "../scsi.h"

#define DEBUG

typedef struct queue_entry {
	struct list_head   list;
	struct scsi_cmnd   *SCpnt;
#ifdef DEBUG
	unsigned long	   magic;
#endif
} QE_t;

#ifdef DEBUG
#define QUEUE_MAGIC_FREE	0xf7e1c9a3
#define QUEUE_MAGIC_USED	0xf7e1cc33

#define SET_MAGIC(q,m)	((q)->magic = (m))
#define BAD_MAGIC(q,m)	((q)->magic != (m))
#else
#define SET_MAGIC(q,m)	do { } while (0)
#define BAD_MAGIC(q,m)	(0)
#endif

#include "queue.h"

#define NR_QE	32

int queue_initialise (Queue_t *queue)
{
	unsigned int nqueues = NR_QE;
	QE_t *q;

	spin_lock_init(&queue->queue_lock);
	INIT_LIST_HEAD(&queue->head);
	INIT_LIST_HEAD(&queue->free);

	queue->alloc = q = kmalloc(sizeof(QE_t) * nqueues, GFP_KERNEL);
	if (q) {
		for (; nqueues; q++, nqueues--) {
			SET_MAGIC(q, QUEUE_MAGIC_FREE);
			q->SCpnt = NULL;
			list_add(&q->list, &queue->free);
		}
	}

	return queue->alloc != NULL;
}

void queue_free (Queue_t *queue)
{
	if (!list_empty(&queue->head))
		printk(KERN_WARNING "freeing non-empty queue %p\n", queue);
	kfree(queue->alloc);
}
     

int __queue_add(Queue_t *queue, struct scsi_cmnd *SCpnt, int head)
{
	unsigned long flags;
	struct list_head *l;
	QE_t *q;
	int ret = 0;

	spin_lock_irqsave(&queue->queue_lock, flags);
	if (list_empty(&queue->free))
		goto empty;

	l = queue->free.next;
	list_del(l);

	q = list_entry(l, QE_t, list);
	BUG_ON(BAD_MAGIC(q, QUEUE_MAGIC_FREE));

	SET_MAGIC(q, QUEUE_MAGIC_USED);
	q->SCpnt = SCpnt;

	if (head)
		list_add(l, &queue->head);
	else
		list_add_tail(l, &queue->head);

	ret = 1;
empty:
	spin_unlock_irqrestore(&queue->queue_lock, flags);
	return ret;
}

static struct scsi_cmnd *__queue_remove(Queue_t *queue, struct list_head *ent)
{
	QE_t *q;

	list_del(ent);
	q = list_entry(ent, QE_t, list);
	BUG_ON(BAD_MAGIC(q, QUEUE_MAGIC_USED));

	SET_MAGIC(q, QUEUE_MAGIC_FREE);
	list_add(ent, &queue->free);

	return q->SCpnt;
}

struct scsi_cmnd *queue_remove_exclude(Queue_t *queue, unsigned long *exclude)
{
	unsigned long flags;
	struct list_head *l;
	struct scsi_cmnd *SCpnt = NULL;

	spin_lock_irqsave(&queue->queue_lock, flags);
	list_for_each(l, &queue->head) {
		QE_t *q = list_entry(l, QE_t, list);
		if (!test_bit(q->SCpnt->device->id * 8 + q->SCpnt->device->lun, exclude)) {
			SCpnt = __queue_remove(queue, l);
			break;
		}
	}
	spin_unlock_irqrestore(&queue->queue_lock, flags);

	return SCpnt;
}

struct scsi_cmnd *queue_remove(Queue_t *queue)
{
	unsigned long flags;
	struct scsi_cmnd *SCpnt = NULL;

	spin_lock_irqsave(&queue->queue_lock, flags);
	if (!list_empty(&queue->head))
		SCpnt = __queue_remove(queue, queue->head.next);
	spin_unlock_irqrestore(&queue->queue_lock, flags);

	return SCpnt;
}

struct scsi_cmnd *queue_remove_tgtluntag(Queue_t *queue, int target, int lun,
					 int tag)
{
	unsigned long flags;
	struct list_head *l;
	struct scsi_cmnd *SCpnt = NULL;

	spin_lock_irqsave(&queue->queue_lock, flags);
	list_for_each(l, &queue->head) {
		QE_t *q = list_entry(l, QE_t, list);
		if (q->SCpnt->device->id == target && q->SCpnt->device->lun == lun &&
		    q->SCpnt->tag == tag) {
			SCpnt = __queue_remove(queue, l);
			break;
		}
	}
	spin_unlock_irqrestore(&queue->queue_lock, flags);

	return SCpnt;
}

void queue_remove_all_target(Queue_t *queue, int target)
{
	unsigned long flags;
	struct list_head *l;

	spin_lock_irqsave(&queue->queue_lock, flags);
	list_for_each(l, &queue->head) {
		QE_t *q = list_entry(l, QE_t, list);
		if (q->SCpnt->device->id == target)
			__queue_remove(queue, l);
	}
	spin_unlock_irqrestore(&queue->queue_lock, flags);
}

int queue_probetgtlun (Queue_t *queue, int target, int lun)
{
	unsigned long flags;
	struct list_head *l;
	int found = 0;

	spin_lock_irqsave(&queue->queue_lock, flags);
	list_for_each(l, &queue->head) {
		QE_t *q = list_entry(l, QE_t, list);
		if (q->SCpnt->device->id == target && q->SCpnt->device->lun == lun) {
			found = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&queue->queue_lock, flags);

	return found;
}

int queue_remove_cmd(Queue_t *queue, struct scsi_cmnd *SCpnt)
{
	unsigned long flags;
	struct list_head *l;
	int found = 0;

	spin_lock_irqsave(&queue->queue_lock, flags);
	list_for_each(l, &queue->head) {
		QE_t *q = list_entry(l, QE_t, list);
		if (q->SCpnt == SCpnt) {
			__queue_remove(queue, l);
			found = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&queue->queue_lock, flags);

	return found;
}

EXPORT_SYMBOL(queue_initialise);
EXPORT_SYMBOL(queue_free);
EXPORT_SYMBOL(__queue_add);
EXPORT_SYMBOL(queue_remove);
EXPORT_SYMBOL(queue_remove_exclude);
EXPORT_SYMBOL(queue_remove_tgtluntag);
EXPORT_SYMBOL(queue_remove_cmd);
EXPORT_SYMBOL(queue_remove_all_target);
EXPORT_SYMBOL(queue_probetgtlun);

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("SCSI command queueing");
MODULE_LICENSE("GPL");
