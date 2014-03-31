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

#ifndef __BTRFS_ASYNC_THREAD_
#define __BTRFS_ASYNC_THREAD_

struct btrfs_worker_thread;

struct btrfs_work {
	void (*func)(struct btrfs_work *work);
	void (*ordered_func)(struct btrfs_work *work);
	void (*ordered_free)(struct btrfs_work *work);

	unsigned long flags;

	
	struct btrfs_worker_thread *worker;
	struct list_head list;
	struct list_head order_list;
};

struct btrfs_workers {
	
	int num_workers;

	int num_workers_starting;

	
	int max_workers;

	
	int idle_thresh;

	
	int ordered;

	
	int atomic_start_pending;

	struct btrfs_workers *atomic_worker_start;

	struct list_head worker_list;
	struct list_head idle_list;

	struct list_head order_list;
	struct list_head prio_order_list;

	
	spinlock_t lock;

	
	spinlock_t order_lock;

	
	char *name;
};

void btrfs_queue_worker(struct btrfs_workers *workers, struct btrfs_work *work);
int btrfs_start_workers(struct btrfs_workers *workers);
void btrfs_stop_workers(struct btrfs_workers *workers);
void btrfs_init_workers(struct btrfs_workers *workers, char *name, int max,
			struct btrfs_workers *async_starter);
void btrfs_requeue_work(struct btrfs_work *work);
void btrfs_set_work_high_prio(struct btrfs_work *work);
#endif
