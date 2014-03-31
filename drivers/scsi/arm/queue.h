/*
 *  linux/drivers/acorn/scsi/queue.h: queue handling
 *
 *  Copyright (C) 1997 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef QUEUE_H
#define QUEUE_H

typedef struct {
	struct list_head head;
	struct list_head free;
	spinlock_t queue_lock;
	void *alloc;			
} Queue_t;

extern int queue_initialise (Queue_t *queue);

extern void queue_free (Queue_t *queue);

extern struct scsi_cmnd *queue_remove (Queue_t *queue);

extern struct scsi_cmnd *queue_remove_exclude(Queue_t *queue,
					      unsigned long *exclude);

#define queue_add_cmd_ordered(queue,SCpnt) \
	__queue_add(queue,SCpnt,(SCpnt)->cmnd[0] == REQUEST_SENSE)
#define queue_add_cmd_tail(queue,SCpnt) \
	__queue_add(queue,SCpnt,0)
extern int __queue_add(Queue_t *queue, struct scsi_cmnd *SCpnt, int head);

extern struct scsi_cmnd *queue_remove_tgtluntag(Queue_t *queue, int target,
						int lun, int tag);

extern void queue_remove_all_target(Queue_t *queue, int target);

extern int queue_probetgtlun (Queue_t *queue, int target, int lun);

int queue_remove_cmd(Queue_t *queue, struct scsi_cmnd *SCpnt);

#endif 
