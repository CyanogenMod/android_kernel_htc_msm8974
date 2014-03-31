/* Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2006 Rusty Russell IBM Corporation
 *
 * Author: Michael S. Tsirkin <mst@redhat.com>
 *
 * Inspiration, some code, and most witty comments come from
 * Documentation/virtual/lguest/lguest.c, by Rusty Russell
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 *
 * Generic code for virtio server in host kernel.
 */

#include <linux/eventfd.h>
#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <linux/mm.h>
#include <linux/mmu_context.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/cgroup.h>

#include <linux/net.h>
#include <linux/if_packet.h>
#include <linux/if_arp.h>

#include "vhost.h"

enum {
	VHOST_MEMORY_MAX_NREGIONS = 64,
	VHOST_MEMORY_F_LOG = 0x1,
};

static unsigned vhost_zcopy_mask __read_mostly;

#define vhost_used_event(vq) ((u16 __user *)&vq->avail->ring[vq->num])
#define vhost_avail_event(vq) ((u16 __user *)&vq->used->ring[vq->num])

static void vhost_poll_func(struct file *file, wait_queue_head_t *wqh,
			    poll_table *pt)
{
	struct vhost_poll *poll;

	poll = container_of(pt, struct vhost_poll, table);
	poll->wqh = wqh;
	add_wait_queue(wqh, &poll->wait);
}

static int vhost_poll_wakeup(wait_queue_t *wait, unsigned mode, int sync,
			     void *key)
{
	struct vhost_poll *poll = container_of(wait, struct vhost_poll, wait);

	if (!((unsigned long)key & poll->mask))
		return 0;

	vhost_poll_queue(poll);
	return 0;
}

static void vhost_work_init(struct vhost_work *work, vhost_work_fn_t fn)
{
	INIT_LIST_HEAD(&work->node);
	work->fn = fn;
	init_waitqueue_head(&work->done);
	work->flushing = 0;
	work->queue_seq = work->done_seq = 0;
}

void vhost_poll_init(struct vhost_poll *poll, vhost_work_fn_t fn,
		     unsigned long mask, struct vhost_dev *dev)
{
	init_waitqueue_func_entry(&poll->wait, vhost_poll_wakeup);
	init_poll_funcptr(&poll->table, vhost_poll_func);
	poll->mask = mask;
	poll->dev = dev;

	vhost_work_init(&poll->work, fn);
}

void vhost_poll_start(struct vhost_poll *poll, struct file *file)
{
	unsigned long mask;

	mask = file->f_op->poll(file, &poll->table);
	if (mask)
		vhost_poll_wakeup(&poll->wait, 0, 0, (void *)mask);
}

void vhost_poll_stop(struct vhost_poll *poll)
{
	remove_wait_queue(poll->wqh, &poll->wait);
}

static bool vhost_work_seq_done(struct vhost_dev *dev, struct vhost_work *work,
				unsigned seq)
{
	int left;

	spin_lock_irq(&dev->work_lock);
	left = seq - work->done_seq;
	spin_unlock_irq(&dev->work_lock);
	return left <= 0;
}

static void vhost_work_flush(struct vhost_dev *dev, struct vhost_work *work)
{
	unsigned seq;
	int flushing;

	spin_lock_irq(&dev->work_lock);
	seq = work->queue_seq;
	work->flushing++;
	spin_unlock_irq(&dev->work_lock);
	wait_event(work->done, vhost_work_seq_done(dev, work, seq));
	spin_lock_irq(&dev->work_lock);
	flushing = --work->flushing;
	spin_unlock_irq(&dev->work_lock);
	BUG_ON(flushing < 0);
}

void vhost_poll_flush(struct vhost_poll *poll)
{
	vhost_work_flush(poll->dev, &poll->work);
}

static inline void vhost_work_queue(struct vhost_dev *dev,
				    struct vhost_work *work)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->work_lock, flags);
	if (list_empty(&work->node)) {
		list_add_tail(&work->node, &dev->work_list);
		work->queue_seq++;
		wake_up_process(dev->worker);
	}
	spin_unlock_irqrestore(&dev->work_lock, flags);
}

void vhost_poll_queue(struct vhost_poll *poll)
{
	vhost_work_queue(poll->dev, &poll->work);
}

static void vhost_vq_reset(struct vhost_dev *dev,
			   struct vhost_virtqueue *vq)
{
	vq->num = 1;
	vq->desc = NULL;
	vq->avail = NULL;
	vq->used = NULL;
	vq->last_avail_idx = 0;
	vq->avail_idx = 0;
	vq->last_used_idx = 0;
	vq->signalled_used = 0;
	vq->signalled_used_valid = false;
	vq->used_flags = 0;
	vq->log_used = false;
	vq->log_addr = -1ull;
	vq->vhost_hlen = 0;
	vq->sock_hlen = 0;
	vq->private_data = NULL;
	vq->log_base = NULL;
	vq->error_ctx = NULL;
	vq->error = NULL;
	vq->kick = NULL;
	vq->call_ctx = NULL;
	vq->call = NULL;
	vq->log_ctx = NULL;
	vq->upend_idx = 0;
	vq->done_idx = 0;
	vq->ubufs = NULL;
}

static int vhost_worker(void *data)
{
	struct vhost_dev *dev = data;
	struct vhost_work *work = NULL;
	unsigned uninitialized_var(seq);

	use_mm(dev->mm);

	for (;;) {
		
		set_current_state(TASK_INTERRUPTIBLE);

		spin_lock_irq(&dev->work_lock);
		if (work) {
			work->done_seq = seq;
			if (work->flushing)
				wake_up_all(&work->done);
		}

		if (kthread_should_stop()) {
			spin_unlock_irq(&dev->work_lock);
			__set_current_state(TASK_RUNNING);
			break;
		}
		if (!list_empty(&dev->work_list)) {
			work = list_first_entry(&dev->work_list,
						struct vhost_work, node);
			list_del_init(&work->node);
			seq = work->queue_seq;
		} else
			work = NULL;
		spin_unlock_irq(&dev->work_lock);

		if (work) {
			__set_current_state(TASK_RUNNING);
			work->fn(work);
			if (need_resched())
				schedule();
		} else
			schedule();

	}
	unuse_mm(dev->mm);
	return 0;
}

static void vhost_vq_free_iovecs(struct vhost_virtqueue *vq)
{
	kfree(vq->indirect);
	vq->indirect = NULL;
	kfree(vq->log);
	vq->log = NULL;
	kfree(vq->heads);
	vq->heads = NULL;
	kfree(vq->ubuf_info);
	vq->ubuf_info = NULL;
}

void vhost_enable_zcopy(int vq)
{
	vhost_zcopy_mask |= 0x1 << vq;
}

static long vhost_dev_alloc_iovecs(struct vhost_dev *dev)
{
	int i;
	bool zcopy;

	for (i = 0; i < dev->nvqs; ++i) {
		dev->vqs[i].indirect = kmalloc(sizeof *dev->vqs[i].indirect *
					       UIO_MAXIOV, GFP_KERNEL);
		dev->vqs[i].log = kmalloc(sizeof *dev->vqs[i].log * UIO_MAXIOV,
					  GFP_KERNEL);
		dev->vqs[i].heads = kmalloc(sizeof *dev->vqs[i].heads *
					    UIO_MAXIOV, GFP_KERNEL);
		zcopy = vhost_zcopy_mask & (0x1 << i);
		if (zcopy)
			dev->vqs[i].ubuf_info =
				kmalloc(sizeof *dev->vqs[i].ubuf_info *
					UIO_MAXIOV, GFP_KERNEL);
		if (!dev->vqs[i].indirect || !dev->vqs[i].log ||
			!dev->vqs[i].heads ||
			(zcopy && !dev->vqs[i].ubuf_info))
			goto err_nomem;
	}
	return 0;

err_nomem:
	for (; i >= 0; --i)
		vhost_vq_free_iovecs(&dev->vqs[i]);
	return -ENOMEM;
}

static void vhost_dev_free_iovecs(struct vhost_dev *dev)
{
	int i;

	for (i = 0; i < dev->nvqs; ++i)
		vhost_vq_free_iovecs(&dev->vqs[i]);
}

long vhost_dev_init(struct vhost_dev *dev,
		    struct vhost_virtqueue *vqs, int nvqs)
{
	int i;

	dev->vqs = vqs;
	dev->nvqs = nvqs;
	mutex_init(&dev->mutex);
	dev->log_ctx = NULL;
	dev->log_file = NULL;
	dev->memory = NULL;
	dev->mm = NULL;
	spin_lock_init(&dev->work_lock);
	INIT_LIST_HEAD(&dev->work_list);
	dev->worker = NULL;

	for (i = 0; i < dev->nvqs; ++i) {
		dev->vqs[i].log = NULL;
		dev->vqs[i].indirect = NULL;
		dev->vqs[i].heads = NULL;
		dev->vqs[i].ubuf_info = NULL;
		dev->vqs[i].dev = dev;
		mutex_init(&dev->vqs[i].mutex);
		vhost_vq_reset(dev, dev->vqs + i);
		if (dev->vqs[i].handle_kick)
			vhost_poll_init(&dev->vqs[i].poll,
					dev->vqs[i].handle_kick, POLLIN, dev);
	}

	return 0;
}

long vhost_dev_check_owner(struct vhost_dev *dev)
{
	
	return dev->mm == current->mm ? 0 : -EPERM;
}

struct vhost_attach_cgroups_struct {
	struct vhost_work work;
	struct task_struct *owner;
	int ret;
};

static void vhost_attach_cgroups_work(struct vhost_work *work)
{
	struct vhost_attach_cgroups_struct *s;

	s = container_of(work, struct vhost_attach_cgroups_struct, work);
	s->ret = cgroup_attach_task_all(s->owner, current);
}

static int vhost_attach_cgroups(struct vhost_dev *dev)
{
	struct vhost_attach_cgroups_struct attach;

	attach.owner = current;
	vhost_work_init(&attach.work, vhost_attach_cgroups_work);
	vhost_work_queue(dev, &attach.work);
	vhost_work_flush(dev, &attach.work);
	return attach.ret;
}

static long vhost_dev_set_owner(struct vhost_dev *dev)
{
	struct task_struct *worker;
	int err;

	
	if (dev->mm) {
		err = -EBUSY;
		goto err_mm;
	}

	
	dev->mm = get_task_mm(current);
	worker = kthread_create(vhost_worker, dev, "vhost-%d", current->pid);
	if (IS_ERR(worker)) {
		err = PTR_ERR(worker);
		goto err_worker;
	}

	dev->worker = worker;
	wake_up_process(worker);	

	err = vhost_attach_cgroups(dev);
	if (err)
		goto err_cgroup;

	err = vhost_dev_alloc_iovecs(dev);
	if (err)
		goto err_cgroup;

	return 0;
err_cgroup:
	kthread_stop(worker);
	dev->worker = NULL;
err_worker:
	if (dev->mm)
		mmput(dev->mm);
	dev->mm = NULL;
err_mm:
	return err;
}

long vhost_dev_reset_owner(struct vhost_dev *dev)
{
	struct vhost_memory *memory;

	
	memory = kmalloc(offsetof(struct vhost_memory, regions), GFP_KERNEL);
	if (!memory)
		return -ENOMEM;

	vhost_dev_cleanup(dev, true);

	memory->nregions = 0;
	RCU_INIT_POINTER(dev->memory, memory);
	return 0;
}

int vhost_zerocopy_signal_used(struct vhost_virtqueue *vq)
{
	int i;
	int j = 0;

	for (i = vq->done_idx; i != vq->upend_idx; i = (i + 1) % UIO_MAXIOV) {
		if ((vq->heads[i].len == VHOST_DMA_DONE_LEN)) {
			vq->heads[i].len = VHOST_DMA_CLEAR_LEN;
			vhost_add_used_and_signal(vq->dev, vq,
						  vq->heads[i].id, 0);
			++j;
		} else
			break;
	}
	if (j)
		vq->done_idx = i;
	return j;
}

void vhost_dev_cleanup(struct vhost_dev *dev, bool locked)
{
	int i;

	for (i = 0; i < dev->nvqs; ++i) {
		if (dev->vqs[i].kick && dev->vqs[i].handle_kick) {
			vhost_poll_stop(&dev->vqs[i].poll);
			vhost_poll_flush(&dev->vqs[i].poll);
		}
		
		if (dev->vqs[i].ubufs)
			vhost_ubuf_put_and_wait(dev->vqs[i].ubufs);

		
		vhost_zerocopy_signal_used(&dev->vqs[i]);

		if (dev->vqs[i].error_ctx)
			eventfd_ctx_put(dev->vqs[i].error_ctx);
		if (dev->vqs[i].error)
			fput(dev->vqs[i].error);
		if (dev->vqs[i].kick)
			fput(dev->vqs[i].kick);
		if (dev->vqs[i].call_ctx)
			eventfd_ctx_put(dev->vqs[i].call_ctx);
		if (dev->vqs[i].call)
			fput(dev->vqs[i].call);
		vhost_vq_reset(dev, dev->vqs + i);
	}
	vhost_dev_free_iovecs(dev);
	if (dev->log_ctx)
		eventfd_ctx_put(dev->log_ctx);
	dev->log_ctx = NULL;
	if (dev->log_file)
		fput(dev->log_file);
	dev->log_file = NULL;
	
	kfree(rcu_dereference_protected(dev->memory,
					locked ==
						lockdep_is_held(&dev->mutex)));
	RCU_INIT_POINTER(dev->memory, NULL);
	WARN_ON(!list_empty(&dev->work_list));
	if (dev->worker) {
		kthread_stop(dev->worker);
		dev->worker = NULL;
	}
	if (dev->mm)
		mmput(dev->mm);
	dev->mm = NULL;
}

static int log_access_ok(void __user *log_base, u64 addr, unsigned long sz)
{
	u64 a = addr / VHOST_PAGE_SIZE / 8;

	
	if (a > ULONG_MAX - (unsigned long)log_base ||
	    a + (unsigned long)log_base > ULONG_MAX)
		return 0;

	return access_ok(VERIFY_WRITE, log_base + a,
			 (sz + VHOST_PAGE_SIZE * 8 - 1) / VHOST_PAGE_SIZE / 8);
}

static int vq_memory_access_ok(void __user *log_base, struct vhost_memory *mem,
			       int log_all)
{
	int i;

	if (!mem)
		return 0;

	for (i = 0; i < mem->nregions; ++i) {
		struct vhost_memory_region *m = mem->regions + i;
		unsigned long a = m->userspace_addr;
		if (m->memory_size > ULONG_MAX)
			return 0;
		else if (!access_ok(VERIFY_WRITE, (void __user *)a,
				    m->memory_size))
			return 0;
		else if (log_all && !log_access_ok(log_base,
						   m->guest_phys_addr,
						   m->memory_size))
			return 0;
	}
	return 1;
}

static int memory_access_ok(struct vhost_dev *d, struct vhost_memory *mem,
			    int log_all)
{
	int i;

	for (i = 0; i < d->nvqs; ++i) {
		int ok;
		mutex_lock(&d->vqs[i].mutex);
		
		if (d->vqs[i].private_data)
			ok = vq_memory_access_ok(d->vqs[i].log_base, mem,
						 log_all);
		else
			ok = 1;
		mutex_unlock(&d->vqs[i].mutex);
		if (!ok)
			return 0;
	}
	return 1;
}

static int vq_access_ok(struct vhost_dev *d, unsigned int num,
			struct vring_desc __user *desc,
			struct vring_avail __user *avail,
			struct vring_used __user *used)
{
	size_t s = vhost_has_feature(d, VIRTIO_RING_F_EVENT_IDX) ? 2 : 0;
	return access_ok(VERIFY_READ, desc, num * sizeof *desc) &&
	       access_ok(VERIFY_READ, avail,
			 sizeof *avail + num * sizeof *avail->ring + s) &&
	       access_ok(VERIFY_WRITE, used,
			sizeof *used + num * sizeof *used->ring + s);
}

int vhost_log_access_ok(struct vhost_dev *dev)
{
	struct vhost_memory *mp;

	mp = rcu_dereference_protected(dev->memory,
				       lockdep_is_held(&dev->mutex));
	return memory_access_ok(dev, mp, 1);
}

static int vq_log_access_ok(struct vhost_dev *d, struct vhost_virtqueue *vq,
			    void __user *log_base)
{
	struct vhost_memory *mp;
	size_t s = vhost_has_feature(d, VIRTIO_RING_F_EVENT_IDX) ? 2 : 0;

	mp = rcu_dereference_protected(vq->dev->memory,
				       lockdep_is_held(&vq->mutex));
	return vq_memory_access_ok(log_base, mp,
			    vhost_has_feature(vq->dev, VHOST_F_LOG_ALL)) &&
		(!vq->log_used || log_access_ok(log_base, vq->log_addr,
					sizeof *vq->used +
					vq->num * sizeof *vq->used->ring + s));
}

int vhost_vq_access_ok(struct vhost_virtqueue *vq)
{
	return vq_access_ok(vq->dev, vq->num, vq->desc, vq->avail, vq->used) &&
		vq_log_access_ok(vq->dev, vq, vq->log_base);
}

static long vhost_set_memory(struct vhost_dev *d, struct vhost_memory __user *m)
{
	struct vhost_memory mem, *newmem, *oldmem;
	unsigned long size = offsetof(struct vhost_memory, regions);

	if (copy_from_user(&mem, m, size))
		return -EFAULT;
	if (mem.padding)
		return -EOPNOTSUPP;
	if (mem.nregions > VHOST_MEMORY_MAX_NREGIONS)
		return -E2BIG;
	newmem = kmalloc(size + mem.nregions * sizeof *m->regions, GFP_KERNEL);
	if (!newmem)
		return -ENOMEM;

	memcpy(newmem, &mem, size);
	if (copy_from_user(newmem->regions, m->regions,
			   mem.nregions * sizeof *m->regions)) {
		kfree(newmem);
		return -EFAULT;
	}

	if (!memory_access_ok(d, newmem,
			      vhost_has_feature(d, VHOST_F_LOG_ALL))) {
		kfree(newmem);
		return -EFAULT;
	}
	oldmem = rcu_dereference_protected(d->memory,
					   lockdep_is_held(&d->mutex));
	rcu_assign_pointer(d->memory, newmem);
	synchronize_rcu();
	kfree(oldmem);
	return 0;
}

static long vhost_set_vring(struct vhost_dev *d, int ioctl, void __user *argp)
{
	struct file *eventfp, *filep = NULL,
		    *pollstart = NULL, *pollstop = NULL;
	struct eventfd_ctx *ctx = NULL;
	u32 __user *idxp = argp;
	struct vhost_virtqueue *vq;
	struct vhost_vring_state s;
	struct vhost_vring_file f;
	struct vhost_vring_addr a;
	u32 idx;
	long r;

	r = get_user(idx, idxp);
	if (r < 0)
		return r;
	if (idx >= d->nvqs)
		return -ENOBUFS;

	vq = d->vqs + idx;

	mutex_lock(&vq->mutex);

	switch (ioctl) {
	case VHOST_SET_VRING_NUM:
		if (vq->private_data) {
			r = -EBUSY;
			break;
		}
		if (copy_from_user(&s, argp, sizeof s)) {
			r = -EFAULT;
			break;
		}
		if (!s.num || s.num > 0xffff || (s.num & (s.num - 1))) {
			r = -EINVAL;
			break;
		}
		vq->num = s.num;
		break;
	case VHOST_SET_VRING_BASE:
		if (vq->private_data) {
			r = -EBUSY;
			break;
		}
		if (copy_from_user(&s, argp, sizeof s)) {
			r = -EFAULT;
			break;
		}
		if (s.num > 0xffff) {
			r = -EINVAL;
			break;
		}
		vq->last_avail_idx = s.num;
		
		vq->avail_idx = vq->last_avail_idx;
		break;
	case VHOST_GET_VRING_BASE:
		s.index = idx;
		s.num = vq->last_avail_idx;
		if (copy_to_user(argp, &s, sizeof s))
			r = -EFAULT;
		break;
	case VHOST_SET_VRING_ADDR:
		if (copy_from_user(&a, argp, sizeof a)) {
			r = -EFAULT;
			break;
		}
		if (a.flags & ~(0x1 << VHOST_VRING_F_LOG)) {
			r = -EOPNOTSUPP;
			break;
		}
		if ((u64)(unsigned long)a.desc_user_addr != a.desc_user_addr ||
		    (u64)(unsigned long)a.used_user_addr != a.used_user_addr ||
		    (u64)(unsigned long)a.avail_user_addr != a.avail_user_addr) {
			r = -EFAULT;
			break;
		}
		if ((a.avail_user_addr & (sizeof *vq->avail->ring - 1)) ||
		    (a.used_user_addr & (sizeof *vq->used->ring - 1)) ||
		    (a.log_guest_addr & (sizeof *vq->used->ring - 1))) {
			r = -EINVAL;
			break;
		}

		if (vq->private_data) {
			if (!vq_access_ok(d, vq->num,
				(void __user *)(unsigned long)a.desc_user_addr,
				(void __user *)(unsigned long)a.avail_user_addr,
				(void __user *)(unsigned long)a.used_user_addr)) {
				r = -EINVAL;
				break;
			}

			
			if ((a.flags & (0x1 << VHOST_VRING_F_LOG)) &&
			    !log_access_ok(vq->log_base, a.log_guest_addr,
					   sizeof *vq->used +
					   vq->num * sizeof *vq->used->ring)) {
				r = -EINVAL;
				break;
			}
		}

		vq->log_used = !!(a.flags & (0x1 << VHOST_VRING_F_LOG));
		vq->desc = (void __user *)(unsigned long)a.desc_user_addr;
		vq->avail = (void __user *)(unsigned long)a.avail_user_addr;
		vq->log_addr = a.log_guest_addr;
		vq->used = (void __user *)(unsigned long)a.used_user_addr;
		break;
	case VHOST_SET_VRING_KICK:
		if (copy_from_user(&f, argp, sizeof f)) {
			r = -EFAULT;
			break;
		}
		eventfp = f.fd == -1 ? NULL : eventfd_fget(f.fd);
		if (IS_ERR(eventfp)) {
			r = PTR_ERR(eventfp);
			break;
		}
		if (eventfp != vq->kick) {
			pollstop = filep = vq->kick;
			pollstart = vq->kick = eventfp;
		} else
			filep = eventfp;
		break;
	case VHOST_SET_VRING_CALL:
		if (copy_from_user(&f, argp, sizeof f)) {
			r = -EFAULT;
			break;
		}
		eventfp = f.fd == -1 ? NULL : eventfd_fget(f.fd);
		if (IS_ERR(eventfp)) {
			r = PTR_ERR(eventfp);
			break;
		}
		if (eventfp != vq->call) {
			filep = vq->call;
			ctx = vq->call_ctx;
			vq->call = eventfp;
			vq->call_ctx = eventfp ?
				eventfd_ctx_fileget(eventfp) : NULL;
		} else
			filep = eventfp;
		break;
	case VHOST_SET_VRING_ERR:
		if (copy_from_user(&f, argp, sizeof f)) {
			r = -EFAULT;
			break;
		}
		eventfp = f.fd == -1 ? NULL : eventfd_fget(f.fd);
		if (IS_ERR(eventfp)) {
			r = PTR_ERR(eventfp);
			break;
		}
		if (eventfp != vq->error) {
			filep = vq->error;
			vq->error = eventfp;
			ctx = vq->error_ctx;
			vq->error_ctx = eventfp ?
				eventfd_ctx_fileget(eventfp) : NULL;
		} else
			filep = eventfp;
		break;
	default:
		r = -ENOIOCTLCMD;
	}

	if (pollstop && vq->handle_kick)
		vhost_poll_stop(&vq->poll);

	if (ctx)
		eventfd_ctx_put(ctx);
	if (filep)
		fput(filep);

	if (pollstart && vq->handle_kick)
		vhost_poll_start(&vq->poll, vq->kick);

	mutex_unlock(&vq->mutex);

	if (pollstop && vq->handle_kick)
		vhost_poll_flush(&vq->poll);
	return r;
}

long vhost_dev_ioctl(struct vhost_dev *d, unsigned int ioctl, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct file *eventfp, *filep = NULL;
	struct eventfd_ctx *ctx = NULL;
	u64 p;
	long r;
	int i, fd;

	
	if (ioctl == VHOST_SET_OWNER) {
		r = vhost_dev_set_owner(d);
		goto done;
	}

	
	r = vhost_dev_check_owner(d);
	if (r)
		goto done;

	switch (ioctl) {
	case VHOST_SET_MEM_TABLE:
		r = vhost_set_memory(d, argp);
		break;
	case VHOST_SET_LOG_BASE:
		if (copy_from_user(&p, argp, sizeof p)) {
			r = -EFAULT;
			break;
		}
		if ((u64)(unsigned long)p != p) {
			r = -EFAULT;
			break;
		}
		for (i = 0; i < d->nvqs; ++i) {
			struct vhost_virtqueue *vq;
			void __user *base = (void __user *)(unsigned long)p;
			vq = d->vqs + i;
			mutex_lock(&vq->mutex);
			
			if (vq->private_data && !vq_log_access_ok(d, vq, base))
				r = -EFAULT;
			else
				vq->log_base = base;
			mutex_unlock(&vq->mutex);
		}
		break;
	case VHOST_SET_LOG_FD:
		r = get_user(fd, (int __user *)argp);
		if (r < 0)
			break;
		eventfp = fd == -1 ? NULL : eventfd_fget(fd);
		if (IS_ERR(eventfp)) {
			r = PTR_ERR(eventfp);
			break;
		}
		if (eventfp != d->log_file) {
			filep = d->log_file;
			ctx = d->log_ctx;
			d->log_ctx = eventfp ?
				eventfd_ctx_fileget(eventfp) : NULL;
		} else
			filep = eventfp;
		for (i = 0; i < d->nvqs; ++i) {
			mutex_lock(&d->vqs[i].mutex);
			d->vqs[i].log_ctx = d->log_ctx;
			mutex_unlock(&d->vqs[i].mutex);
		}
		if (ctx)
			eventfd_ctx_put(ctx);
		if (filep)
			fput(filep);
		break;
	default:
		r = vhost_set_vring(d, ioctl, argp);
		break;
	}
done:
	return r;
}

static const struct vhost_memory_region *find_region(struct vhost_memory *mem,
						     __u64 addr, __u32 len)
{
	struct vhost_memory_region *reg;
	int i;

	for (i = 0; i < mem->nregions; ++i) {
		reg = mem->regions + i;
		if (reg->guest_phys_addr <= addr &&
		    reg->guest_phys_addr + reg->memory_size - 1 >= addr)
			return reg;
	}
	return NULL;
}

static int set_bit_to_user(int nr, void __user *addr)
{
	unsigned long log = (unsigned long)addr;
	struct page *page;
	void *base;
	int bit = nr + (log % PAGE_SIZE) * 8;
	int r;

	r = get_user_pages_fast(log, 1, 1, &page);
	if (r < 0)
		return r;
	BUG_ON(r != 1);
	base = kmap_atomic(page);
	set_bit(bit, base);
	kunmap_atomic(base);
	set_page_dirty_lock(page);
	put_page(page);
	return 0;
}

static int log_write(void __user *log_base,
		     u64 write_address, u64 write_length)
{
	u64 write_page = write_address / VHOST_PAGE_SIZE;
	int r;

	if (!write_length)
		return 0;
	write_length += write_address % VHOST_PAGE_SIZE;
	for (;;) {
		u64 base = (u64)(unsigned long)log_base;
		u64 log = base + write_page / 8;
		int bit = write_page % 8;
		if ((u64)(unsigned long)log != log)
			return -EFAULT;
		r = set_bit_to_user(bit, (void __user *)(unsigned long)log);
		if (r < 0)
			return r;
		if (write_length <= VHOST_PAGE_SIZE)
			break;
		write_length -= VHOST_PAGE_SIZE;
		write_page += 1;
	}
	return r;
}

int vhost_log_write(struct vhost_virtqueue *vq, struct vhost_log *log,
		    unsigned int log_num, u64 len)
{
	int i, r;

	/* Make sure data written is seen before log. */
	smp_wmb();
	for (i = 0; i < log_num; ++i) {
		u64 l = min(log[i].len, len);
		r = log_write(vq->log_base, log[i].addr, l);
		if (r < 0)
			return r;
		len -= l;
		if (!len) {
			if (vq->log_ctx)
				eventfd_signal(vq->log_ctx, 1);
			return 0;
		}
	}
	/* Length written exceeds what we have stored. This is a bug. */
	BUG();
	return 0;
}

static int vhost_update_used_flags(struct vhost_virtqueue *vq)
{
	void __user *used;
	if (__put_user(vq->used_flags, &vq->used->flags) < 0)
		return -EFAULT;
	if (unlikely(vq->log_used)) {
		
		smp_wmb();
		
		used = &vq->used->flags;
		log_write(vq->log_base, vq->log_addr +
			  (used - (void __user *)vq->used),
			  sizeof vq->used->flags);
		if (vq->log_ctx)
			eventfd_signal(vq->log_ctx, 1);
	}
	return 0;
}

static int vhost_update_avail_event(struct vhost_virtqueue *vq, u16 avail_event)
{
	if (__put_user(vq->avail_idx, vhost_avail_event(vq)))
		return -EFAULT;
	if (unlikely(vq->log_used)) {
		void __user *used;
		
		smp_wmb();
		
		used = vhost_avail_event(vq);
		log_write(vq->log_base, vq->log_addr +
			  (used - (void __user *)vq->used),
			  sizeof *vhost_avail_event(vq));
		if (vq->log_ctx)
			eventfd_signal(vq->log_ctx, 1);
	}
	return 0;
}

int vhost_init_used(struct vhost_virtqueue *vq)
{
	int r;
	if (!vq->private_data)
		return 0;

	r = vhost_update_used_flags(vq);
	if (r)
		return r;
	vq->signalled_used_valid = false;
	return get_user(vq->last_used_idx, &vq->used->idx);
}

static int translate_desc(struct vhost_dev *dev, u64 addr, u32 len,
			  struct iovec iov[], int iov_size)
{
	const struct vhost_memory_region *reg;
	struct vhost_memory *mem;
	struct iovec *_iov;
	u64 s = 0;
	int ret = 0;

	rcu_read_lock();

	mem = rcu_dereference(dev->memory);
	while ((u64)len > s) {
		u64 size;
		if (unlikely(ret >= iov_size)) {
			ret = -ENOBUFS;
			break;
		}
		reg = find_region(mem, addr, len);
		if (unlikely(!reg)) {
			ret = -EFAULT;
			break;
		}
		_iov = iov + ret;
		size = reg->memory_size - addr + reg->guest_phys_addr;
		_iov->iov_len = min((u64)len, size);
		_iov->iov_base = (void __user *)(unsigned long)
			(reg->userspace_addr + addr - reg->guest_phys_addr);
		s += size;
		addr += size;
		++ret;
	}

	rcu_read_unlock();
	return ret;
}

static unsigned next_desc(struct vring_desc *desc)
{
	unsigned int next;

	
	if (!(desc->flags & VRING_DESC_F_NEXT))
		return -1U;

	
	next = desc->next;
	
	read_barrier_depends();

	return next;
}

static int get_indirect(struct vhost_dev *dev, struct vhost_virtqueue *vq,
			struct iovec iov[], unsigned int iov_size,
			unsigned int *out_num, unsigned int *in_num,
			struct vhost_log *log, unsigned int *log_num,
			struct vring_desc *indirect)
{
	struct vring_desc desc;
	unsigned int i = 0, count, found = 0;
	int ret;

	
	if (unlikely(indirect->len % sizeof desc)) {
		vq_err(vq, "Invalid length in indirect descriptor: "
		       "len 0x%llx not multiple of 0x%zx\n",
		       (unsigned long long)indirect->len,
		       sizeof desc);
		return -EINVAL;
	}

	ret = translate_desc(dev, indirect->addr, indirect->len, vq->indirect,
			     UIO_MAXIOV);
	if (unlikely(ret < 0)) {
		vq_err(vq, "Translation failure %d in indirect.\n", ret);
		return ret;
	}

	read_barrier_depends();

	count = indirect->len / sizeof desc;
	if (unlikely(count > USHRT_MAX + 1)) {
		vq_err(vq, "Indirect buffer length too big: %d\n",
		       indirect->len);
		return -E2BIG;
	}

	do {
		unsigned iov_count = *in_num + *out_num;
		if (unlikely(++found > count)) {
			vq_err(vq, "Loop detected: last one at %u "
			       "indirect size %u\n",
			       i, count);
			return -EINVAL;
		}
		if (unlikely(memcpy_fromiovec((unsigned char *)&desc,
					      vq->indirect, sizeof desc))) {
			vq_err(vq, "Failed indirect descriptor: idx %d, %zx\n",
			       i, (size_t)indirect->addr + i * sizeof desc);
			return -EINVAL;
		}
		if (unlikely(desc.flags & VRING_DESC_F_INDIRECT)) {
			vq_err(vq, "Nested indirect descriptor: idx %d, %zx\n",
			       i, (size_t)indirect->addr + i * sizeof desc);
			return -EINVAL;
		}

		ret = translate_desc(dev, desc.addr, desc.len, iov + iov_count,
				     iov_size - iov_count);
		if (unlikely(ret < 0)) {
			vq_err(vq, "Translation failure %d indirect idx %d\n",
			       ret, i);
			return ret;
		}
		
		if (desc.flags & VRING_DESC_F_WRITE) {
			*in_num += ret;
			if (unlikely(log)) {
				log[*log_num].addr = desc.addr;
				log[*log_num].len = desc.len;
				++*log_num;
			}
		} else {
			if (unlikely(*in_num)) {
				vq_err(vq, "Indirect descriptor "
				       "has out after in: idx %d\n", i);
				return -EINVAL;
			}
			*out_num += ret;
		}
	} while ((i = next_desc(&desc)) != -1);
	return 0;
}

int vhost_get_vq_desc(struct vhost_dev *dev, struct vhost_virtqueue *vq,
		      struct iovec iov[], unsigned int iov_size,
		      unsigned int *out_num, unsigned int *in_num,
		      struct vhost_log *log, unsigned int *log_num)
{
	struct vring_desc desc;
	unsigned int i, head, found = 0;
	u16 last_avail_idx;
	int ret;

	
	last_avail_idx = vq->last_avail_idx;
	if (unlikely(__get_user(vq->avail_idx, &vq->avail->idx))) {
		vq_err(vq, "Failed to access avail idx at %p\n",
		       &vq->avail->idx);
		return -EFAULT;
	}

	if (unlikely((u16)(vq->avail_idx - last_avail_idx) > vq->num)) {
		vq_err(vq, "Guest moved used index from %u to %u",
		       last_avail_idx, vq->avail_idx);
		return -EFAULT;
	}

	
	if (vq->avail_idx == last_avail_idx)
		return vq->num;

	
	smp_rmb();

	if (unlikely(__get_user(head,
				&vq->avail->ring[last_avail_idx % vq->num]))) {
		vq_err(vq, "Failed to read head: idx %d address %p\n",
		       last_avail_idx,
		       &vq->avail->ring[last_avail_idx % vq->num]);
		return -EFAULT;
	}

	
	if (unlikely(head >= vq->num)) {
		vq_err(vq, "Guest says index %u > %u is available",
		       head, vq->num);
		return -EINVAL;
	}

	
	*out_num = *in_num = 0;
	if (unlikely(log))
		*log_num = 0;

	i = head;
	do {
		unsigned iov_count = *in_num + *out_num;
		if (unlikely(i >= vq->num)) {
			vq_err(vq, "Desc index is %u > %u, head = %u",
			       i, vq->num, head);
			return -EINVAL;
		}
		if (unlikely(++found > vq->num)) {
			vq_err(vq, "Loop detected: last one at %u "
			       "vq size %u head %u\n",
			       i, vq->num, head);
			return -EINVAL;
		}
		ret = __copy_from_user(&desc, vq->desc + i, sizeof desc);
		if (unlikely(ret)) {
			vq_err(vq, "Failed to get descriptor: idx %d addr %p\n",
			       i, vq->desc + i);
			return -EFAULT;
		}
		if (desc.flags & VRING_DESC_F_INDIRECT) {
			ret = get_indirect(dev, vq, iov, iov_size,
					   out_num, in_num,
					   log, log_num, &desc);
			if (unlikely(ret < 0)) {
				vq_err(vq, "Failure detected "
				       "in indirect descriptor at idx %d\n", i);
				return ret;
			}
			continue;
		}

		ret = translate_desc(dev, desc.addr, desc.len, iov + iov_count,
				     iov_size - iov_count);
		if (unlikely(ret < 0)) {
			vq_err(vq, "Translation failure %d descriptor idx %d\n",
			       ret, i);
			return ret;
		}
		if (desc.flags & VRING_DESC_F_WRITE) {
			*in_num += ret;
			if (unlikely(log)) {
				log[*log_num].addr = desc.addr;
				log[*log_num].len = desc.len;
				++*log_num;
			}
		} else {
			if (unlikely(*in_num)) {
				vq_err(vq, "Descriptor has out after in: "
				       "idx %d\n", i);
				return -EINVAL;
			}
			*out_num += ret;
		}
	} while ((i = next_desc(&desc)) != -1);

	
	vq->last_avail_idx++;

	BUG_ON(!(vq->used_flags & VRING_USED_F_NO_NOTIFY));
	return head;
}

void vhost_discard_vq_desc(struct vhost_virtqueue *vq, int n)
{
	vq->last_avail_idx -= n;
}

int vhost_add_used(struct vhost_virtqueue *vq, unsigned int head, int len)
{
	struct vring_used_elem __user *used;

	used = &vq->used->ring[vq->last_used_idx % vq->num];
	if (__put_user(head, &used->id)) {
		vq_err(vq, "Failed to write used id");
		return -EFAULT;
	}
	if (__put_user(len, &used->len)) {
		vq_err(vq, "Failed to write used len");
		return -EFAULT;
	}
	/* Make sure buffer is written before we update index. */
	smp_wmb();
	if (__put_user(vq->last_used_idx + 1, &vq->used->idx)) {
		vq_err(vq, "Failed to increment used idx");
		return -EFAULT;
	}
	if (unlikely(vq->log_used)) {
		
		smp_wmb();
		
		log_write(vq->log_base,
			  vq->log_addr +
			   ((void __user *)used - (void __user *)vq->used),
			  sizeof *used);
		
		log_write(vq->log_base,
			  vq->log_addr + offsetof(struct vring_used, idx),
			  sizeof vq->used->idx);
		if (vq->log_ctx)
			eventfd_signal(vq->log_ctx, 1);
	}
	vq->last_used_idx++;
	if (unlikely(vq->last_used_idx == vq->signalled_used))
		vq->signalled_used_valid = false;
	return 0;
}

static int __vhost_add_used_n(struct vhost_virtqueue *vq,
			    struct vring_used_elem *heads,
			    unsigned count)
{
	struct vring_used_elem __user *used;
	u16 old, new;
	int start;

	start = vq->last_used_idx % vq->num;
	used = vq->used->ring + start;
	if (__copy_to_user(used, heads, count * sizeof *used)) {
		vq_err(vq, "Failed to write used");
		return -EFAULT;
	}
	if (unlikely(vq->log_used)) {
		
		smp_wmb();
		
		log_write(vq->log_base,
			  vq->log_addr +
			   ((void __user *)used - (void __user *)vq->used),
			  count * sizeof *used);
	}
	old = vq->last_used_idx;
	new = (vq->last_used_idx += count);
	if (unlikely((u16)(new - vq->signalled_used) < (u16)(new - old)))
		vq->signalled_used_valid = false;
	return 0;
}

int vhost_add_used_n(struct vhost_virtqueue *vq, struct vring_used_elem *heads,
		     unsigned count)
{
	int start, n, r;

	start = vq->last_used_idx % vq->num;
	n = vq->num - start;
	if (n < count) {
		r = __vhost_add_used_n(vq, heads, n);
		if (r < 0)
			return r;
		heads += n;
		count -= n;
	}
	r = __vhost_add_used_n(vq, heads, count);

	/* Make sure buffer is written before we update index. */
	smp_wmb();
	if (put_user(vq->last_used_idx, &vq->used->idx)) {
		vq_err(vq, "Failed to increment used idx");
		return -EFAULT;
	}
	if (unlikely(vq->log_used)) {
		
		log_write(vq->log_base,
			  vq->log_addr + offsetof(struct vring_used, idx),
			  sizeof vq->used->idx);
		if (vq->log_ctx)
			eventfd_signal(vq->log_ctx, 1);
	}
	return r;
}

static bool vhost_notify(struct vhost_dev *dev, struct vhost_virtqueue *vq)
{
	__u16 old, new, event;
	bool v;
	smp_mb();

	if (vhost_has_feature(dev, VIRTIO_F_NOTIFY_ON_EMPTY) &&
	    unlikely(vq->avail_idx == vq->last_avail_idx))
		return true;

	if (!vhost_has_feature(dev, VIRTIO_RING_F_EVENT_IDX)) {
		__u16 flags;
		if (__get_user(flags, &vq->avail->flags)) {
			vq_err(vq, "Failed to get flags");
			return true;
		}
		return !(flags & VRING_AVAIL_F_NO_INTERRUPT);
	}
	old = vq->signalled_used;
	v = vq->signalled_used_valid;
	new = vq->signalled_used = vq->last_used_idx;
	vq->signalled_used_valid = true;

	if (unlikely(!v))
		return true;

	if (get_user(event, vhost_used_event(vq))) {
		vq_err(vq, "Failed to get used event idx");
		return true;
	}
	return vring_need_event(event, new, old);
}

void vhost_signal(struct vhost_dev *dev, struct vhost_virtqueue *vq)
{
	
	if (vq->call_ctx && vhost_notify(dev, vq))
		eventfd_signal(vq->call_ctx, 1);
}

void vhost_add_used_and_signal(struct vhost_dev *dev,
			       struct vhost_virtqueue *vq,
			       unsigned int head, int len)
{
	vhost_add_used(vq, head, len);
	vhost_signal(dev, vq);
}

void vhost_add_used_and_signal_n(struct vhost_dev *dev,
				 struct vhost_virtqueue *vq,
				 struct vring_used_elem *heads, unsigned count)
{
	vhost_add_used_n(vq, heads, count);
	vhost_signal(dev, vq);
}

bool vhost_enable_notify(struct vhost_dev *dev, struct vhost_virtqueue *vq)
{
	u16 avail_idx;
	int r;

	if (!(vq->used_flags & VRING_USED_F_NO_NOTIFY))
		return false;
	vq->used_flags &= ~VRING_USED_F_NO_NOTIFY;
	if (!vhost_has_feature(dev, VIRTIO_RING_F_EVENT_IDX)) {
		r = vhost_update_used_flags(vq);
		if (r) {
			vq_err(vq, "Failed to enable notification at %p: %d\n",
			       &vq->used->flags, r);
			return false;
		}
	} else {
		r = vhost_update_avail_event(vq, vq->avail_idx);
		if (r) {
			vq_err(vq, "Failed to update avail event index at %p: %d\n",
			       vhost_avail_event(vq), r);
			return false;
		}
	}
	/* They could have slipped one in as we were doing that: make
	 * sure it's written, then check again. */
	smp_mb();
	r = __get_user(avail_idx, &vq->avail->idx);
	if (r) {
		vq_err(vq, "Failed to check avail idx at %p: %d\n",
		       &vq->avail->idx, r);
		return false;
	}

	return avail_idx != vq->avail_idx;
}

void vhost_disable_notify(struct vhost_dev *dev, struct vhost_virtqueue *vq)
{
	int r;

	if (vq->used_flags & VRING_USED_F_NO_NOTIFY)
		return;
	vq->used_flags |= VRING_USED_F_NO_NOTIFY;
	if (!vhost_has_feature(dev, VIRTIO_RING_F_EVENT_IDX)) {
		r = vhost_update_used_flags(vq);
		if (r)
			vq_err(vq, "Failed to enable notification at %p: %d\n",
			       &vq->used->flags, r);
	}
}

static void vhost_zerocopy_done_signal(struct kref *kref)
{
	struct vhost_ubuf_ref *ubufs = container_of(kref, struct vhost_ubuf_ref,
						    kref);
	wake_up(&ubufs->wait);
}

struct vhost_ubuf_ref *vhost_ubuf_alloc(struct vhost_virtqueue *vq,
					bool zcopy)
{
	struct vhost_ubuf_ref *ubufs;
	
	if (!zcopy)
		return NULL;
	ubufs = kmalloc(sizeof *ubufs, GFP_KERNEL);
	if (!ubufs)
		return ERR_PTR(-ENOMEM);
	kref_init(&ubufs->kref);
	init_waitqueue_head(&ubufs->wait);
	ubufs->vq = vq;
	return ubufs;
}

void vhost_ubuf_put(struct vhost_ubuf_ref *ubufs)
{
	kref_put(&ubufs->kref, vhost_zerocopy_done_signal);
}

void vhost_ubuf_put_and_wait(struct vhost_ubuf_ref *ubufs)
{
	kref_put(&ubufs->kref, vhost_zerocopy_done_signal);
	wait_event(ubufs->wait, !atomic_read(&ubufs->kref.refcount));
	kfree(ubufs);
}

void vhost_zerocopy_callback(struct ubuf_info *ubuf)
{
	struct vhost_ubuf_ref *ubufs = ubuf->ctx;
	struct vhost_virtqueue *vq = ubufs->vq;

	
	vq->heads[ubuf->desc].len = VHOST_DMA_DONE_LEN;
	kref_put(&ubufs->kref, vhost_zerocopy_done_signal);
}
