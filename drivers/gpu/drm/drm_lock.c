
/*
 * Created: Tue Feb  2 08:37:54 1999 by faith@valinux.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/export.h>
#include "drmP.h"

static int drm_notifier(void *priv);

static int drm_lock_take(struct drm_lock_data *lock_data, unsigned int context);

int drm_lock(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	DECLARE_WAITQUEUE(entry, current);
	struct drm_lock *lock = data;
	struct drm_master *master = file_priv->master;
	int ret = 0;

	++file_priv->lock_count;

	if (lock->context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
			  task_pid_nr(current), lock->context);
		return -EINVAL;
	}

	DRM_DEBUG("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
		  lock->context, task_pid_nr(current),
		  master->lock.hw_lock->lock, lock->flags);

	if (drm_core_check_feature(dev, DRIVER_DMA_QUEUE))
		if (lock->context < 0)
			return -EINVAL;

	add_wait_queue(&master->lock.lock_queue, &entry);
	spin_lock_bh(&master->lock.spinlock);
	master->lock.user_waiters++;
	spin_unlock_bh(&master->lock.spinlock);

	for (;;) {
		__set_current_state(TASK_INTERRUPTIBLE);
		if (!master->lock.hw_lock) {
			
			send_sig(SIGTERM, current, 0);
			ret = -EINTR;
			break;
		}
		if (drm_lock_take(&master->lock, lock->context)) {
			master->lock.file_priv = file_priv;
			master->lock.lock_time = jiffies;
			atomic_inc(&dev->counts[_DRM_STAT_LOCKS]);
			break;	
		}

		
		mutex_unlock(&drm_global_mutex);
		schedule();
		mutex_lock(&drm_global_mutex);
		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}
	}
	spin_lock_bh(&master->lock.spinlock);
	master->lock.user_waiters--;
	spin_unlock_bh(&master->lock.spinlock);
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&master->lock.lock_queue, &entry);

	DRM_DEBUG("%d %s\n", lock->context,
		  ret ? "interrupted" : "has lock");
	if (ret) return ret;

	if (!file_priv->is_master) {
		sigemptyset(&dev->sigmask);
		sigaddset(&dev->sigmask, SIGSTOP);
		sigaddset(&dev->sigmask, SIGTSTP);
		sigaddset(&dev->sigmask, SIGTTIN);
		sigaddset(&dev->sigmask, SIGTTOU);
		dev->sigdata.context = lock->context;
		dev->sigdata.lock = master->lock.hw_lock;
		block_all_signals(drm_notifier, &dev->sigdata, &dev->sigmask);
	}

	if (dev->driver->dma_quiescent && (lock->flags & _DRM_LOCK_QUIESCENT))
	{
		if (dev->driver->dma_quiescent(dev)) {
			DRM_DEBUG("%d waiting for DMA quiescent\n",
				  lock->context);
			return -EBUSY;
		}
	}

	return 0;
}

int drm_unlock(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_lock *lock = data;
	struct drm_master *master = file_priv->master;

	if (lock->context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
			  task_pid_nr(current), lock->context);
		return -EINVAL;
	}

	atomic_inc(&dev->counts[_DRM_STAT_UNLOCKS]);

	if (drm_lock_free(&master->lock, lock->context)) {
		
	}

	unblock_all_signals();
	return 0;
}

static
int drm_lock_take(struct drm_lock_data *lock_data,
		  unsigned int context)
{
	unsigned int old, new, prev;
	volatile unsigned int *lock = &lock_data->hw_lock->lock;

	spin_lock_bh(&lock_data->spinlock);
	do {
		old = *lock;
		if (old & _DRM_LOCK_HELD)
			new = old | _DRM_LOCK_CONT;
		else {
			new = context | _DRM_LOCK_HELD |
				((lock_data->user_waiters + lock_data->kernel_waiters > 1) ?
				 _DRM_LOCK_CONT : 0);
		}
		prev = cmpxchg(lock, old, new);
	} while (prev != old);
	spin_unlock_bh(&lock_data->spinlock);

	if (_DRM_LOCKING_CONTEXT(old) == context) {
		if (old & _DRM_LOCK_HELD) {
			if (context != DRM_KERNEL_CONTEXT) {
				DRM_ERROR("%d holds heavyweight lock\n",
					  context);
			}
			return 0;
		}
	}

	if ((_DRM_LOCKING_CONTEXT(new)) == context && (new & _DRM_LOCK_HELD)) {
		
		return 1;
	}
	return 0;
}

static int drm_lock_transfer(struct drm_lock_data *lock_data,
			     unsigned int context)
{
	unsigned int old, new, prev;
	volatile unsigned int *lock = &lock_data->hw_lock->lock;

	lock_data->file_priv = NULL;
	do {
		old = *lock;
		new = context | _DRM_LOCK_HELD;
		prev = cmpxchg(lock, old, new);
	} while (prev != old);
	return 1;
}

int drm_lock_free(struct drm_lock_data *lock_data, unsigned int context)
{
	unsigned int old, new, prev;
	volatile unsigned int *lock = &lock_data->hw_lock->lock;

	spin_lock_bh(&lock_data->spinlock);
	if (lock_data->kernel_waiters != 0) {
		drm_lock_transfer(lock_data, 0);
		lock_data->idle_has_lock = 1;
		spin_unlock_bh(&lock_data->spinlock);
		return 1;
	}
	spin_unlock_bh(&lock_data->spinlock);

	do {
		old = *lock;
		new = _DRM_LOCKING_CONTEXT(old);
		prev = cmpxchg(lock, old, new);
	} while (prev != old);

	if (_DRM_LOCK_IS_HELD(old) && _DRM_LOCKING_CONTEXT(old) != context) {
		DRM_ERROR("%d freed heavyweight lock held by %d\n",
			  context, _DRM_LOCKING_CONTEXT(old));
		return 1;
	}
	wake_up_interruptible(&lock_data->lock_queue);
	return 0;
}

static int drm_notifier(void *priv)
{
	struct drm_sigdata *s = (struct drm_sigdata *) priv;
	unsigned int old, new, prev;

	
	if (!s->lock || !_DRM_LOCK_IS_HELD(s->lock->lock)
	    || _DRM_LOCKING_CONTEXT(s->lock->lock) != s->context)
		return 1;

	do {
		old = s->lock->lock;
		new = old | _DRM_LOCK_CONT;
		prev = cmpxchg(&s->lock->lock, old, new);
	} while (prev != old);
	return 0;
}


void drm_idlelock_take(struct drm_lock_data *lock_data)
{
	int ret = 0;

	spin_lock_bh(&lock_data->spinlock);
	lock_data->kernel_waiters++;
	if (!lock_data->idle_has_lock) {

		spin_unlock_bh(&lock_data->spinlock);
		ret = drm_lock_take(lock_data, DRM_KERNEL_CONTEXT);
		spin_lock_bh(&lock_data->spinlock);

		if (ret == 1)
			lock_data->idle_has_lock = 1;
	}
	spin_unlock_bh(&lock_data->spinlock);
}
EXPORT_SYMBOL(drm_idlelock_take);

void drm_idlelock_release(struct drm_lock_data *lock_data)
{
	unsigned int old, prev;
	volatile unsigned int *lock = &lock_data->hw_lock->lock;

	spin_lock_bh(&lock_data->spinlock);
	if (--lock_data->kernel_waiters == 0) {
		if (lock_data->idle_has_lock) {
			do {
				old = *lock;
				prev = cmpxchg(lock, old, DRM_KERNEL_CONTEXT);
			} while (prev != old);
			wake_up_interruptible(&lock_data->lock_queue);
			lock_data->idle_has_lock = 0;
		}
	}
	spin_unlock_bh(&lock_data->spinlock);
}
EXPORT_SYMBOL(drm_idlelock_release);

int drm_i_have_hw_lock(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_master *master = file_priv->master;
	return (file_priv->lock_count && master->lock.hw_lock &&
		_DRM_LOCK_IS_HELD(master->lock.hw_lock->lock) &&
		master->lock.file_priv == file_priv);
}
