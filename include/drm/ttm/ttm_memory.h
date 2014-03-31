/**************************************************************************
 *
 * Copyright (c) 2006-2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#ifndef TTM_MEMORY_H
#define TTM_MEMORY_H

#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/bug.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/kobject.h>
#include <linux/mm.h>


struct ttm_mem_shrink {
	int (*do_shrink) (struct ttm_mem_shrink *);
};


#define TTM_MEM_MAX_ZONES 2
struct ttm_mem_zone;
struct ttm_mem_global {
	struct kobject kobj;
	struct ttm_mem_shrink *shrink;
	struct workqueue_struct *swap_queue;
	struct work_struct work;
	wait_queue_head_t queue;
	spinlock_t lock;
	struct ttm_mem_zone *zones[TTM_MEM_MAX_ZONES];
	unsigned int num_zones;
	struct ttm_mem_zone *zone_kernel;
#ifdef CONFIG_HIGHMEM
	struct ttm_mem_zone *zone_highmem;
#else
	struct ttm_mem_zone *zone_dma32;
#endif
};


static inline void ttm_mem_init_shrink(struct ttm_mem_shrink *shrink,
				       int (*func) (struct ttm_mem_shrink *))
{
	shrink->do_shrink = func;
}


static inline int ttm_mem_register_shrink(struct ttm_mem_global *glob,
					  struct ttm_mem_shrink *shrink)
{
	spin_lock(&glob->lock);
	if (glob->shrink != NULL) {
		spin_unlock(&glob->lock);
		return -EBUSY;
	}
	glob->shrink = shrink;
	spin_unlock(&glob->lock);
	return 0;
}


static inline void ttm_mem_unregister_shrink(struct ttm_mem_global *glob,
					     struct ttm_mem_shrink *shrink)
{
	spin_lock(&glob->lock);
	BUG_ON(glob->shrink != shrink);
	glob->shrink = NULL;
	spin_unlock(&glob->lock);
}

extern int ttm_mem_global_init(struct ttm_mem_global *glob);
extern void ttm_mem_global_release(struct ttm_mem_global *glob);
extern int ttm_mem_global_alloc(struct ttm_mem_global *glob, uint64_t memory,
				bool no_wait, bool interruptible);
extern void ttm_mem_global_free(struct ttm_mem_global *glob,
				uint64_t amount);
extern int ttm_mem_global_alloc_page(struct ttm_mem_global *glob,
				     struct page *page,
				     bool no_wait, bool interruptible);
extern void ttm_mem_global_free_page(struct ttm_mem_global *glob,
				     struct page *page);
extern size_t ttm_round_pot(size_t size);
#endif
