/**************************************************************************
 *
 * Copyright (c) 2007-2009 VMware, Inc., Palo Alto, CA., USA
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


#ifndef _TTM_LOCK_H_
#define _TTM_LOCK_H_

#include "ttm/ttm_object.h"
#include <linux/wait.h>
#include <linux/atomic.h>


struct ttm_lock {
	struct ttm_base_object base;
	wait_queue_head_t queue;
	spinlock_t lock;
	int32_t rw;
	uint32_t flags;
	bool kill_takers;
	int signal;
	struct ttm_object_file *vt_holder;
};


extern void ttm_lock_init(struct ttm_lock *lock);

extern void ttm_read_unlock(struct ttm_lock *lock);

extern int ttm_read_lock(struct ttm_lock *lock, bool interruptible);

extern int ttm_read_trylock(struct ttm_lock *lock, bool interruptible);

extern void ttm_write_unlock(struct ttm_lock *lock);

extern int ttm_write_lock(struct ttm_lock *lock, bool interruptible);

extern void ttm_lock_downgrade(struct ttm_lock *lock);

extern void ttm_suspend_lock(struct ttm_lock *lock);

extern void ttm_suspend_unlock(struct ttm_lock *lock);

extern int ttm_vt_lock(struct ttm_lock *lock, bool interruptible,
		       struct ttm_object_file *tfile);

extern int ttm_vt_unlock(struct ttm_lock *lock);

extern void ttm_write_unlock(struct ttm_lock *lock);

extern int ttm_write_lock(struct ttm_lock *lock, bool interruptible);

static inline void ttm_lock_set_kill(struct ttm_lock *lock, bool val,
				     int signal)
{
	lock->kill_takers = val;
	if (val)
		lock->signal = signal;
}

#endif
