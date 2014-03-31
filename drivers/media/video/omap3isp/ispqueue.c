/*
 * ispqueue.c
 *
 * TI OMAP3 ISP - Video buffers queue handling
 *
 * Copyright (C) 2010 Nokia Corporation
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "ispqueue.h"


#define ISP_CACHE_FLUSH_PAGES_MAX       0

static void isp_video_buffer_cache_sync(struct isp_video_buffer *buf)
{
	if (buf->skip_cache)
		return;

	if (buf->vbuf.m.userptr == 0 || buf->npages == 0 ||
	    buf->npages > ISP_CACHE_FLUSH_PAGES_MAX)
		flush_cache_all();
	else {
		dmac_map_area((void *)buf->vbuf.m.userptr, buf->vbuf.length,
			      DMA_FROM_DEVICE);
		outer_inv_range(buf->vbuf.m.userptr,
				buf->vbuf.m.userptr + buf->vbuf.length);
	}
}

static int isp_video_buffer_lock_vma(struct isp_video_buffer *buf, int lock)
{
	struct vm_area_struct *vma;
	unsigned long start;
	unsigned long end;
	int ret = 0;

	if (buf->vbuf.memory == V4L2_MEMORY_MMAP)
		return 0;

	if (!current || !current->mm)
		return lock ? -EINVAL : 0;

	start = buf->vbuf.m.userptr;
	end = buf->vbuf.m.userptr + buf->vbuf.length - 1;

	down_write(&current->mm->mmap_sem);
	spin_lock(&current->mm->page_table_lock);

	do {
		vma = find_vma(current->mm, start);
		if (vma == NULL) {
			ret = -EFAULT;
			goto out;
		}

		if (lock)
			vma->vm_flags |= VM_LOCKED;
		else
			vma->vm_flags &= ~VM_LOCKED;

		start = vma->vm_end + 1;
	} while (vma->vm_end < end);

	if (lock)
		buf->vm_flags |= VM_LOCKED;
	else
		buf->vm_flags &= ~VM_LOCKED;

out:
	spin_unlock(&current->mm->page_table_lock);
	up_write(&current->mm->mmap_sem);
	return ret;
}

static int isp_video_buffer_sglist_kernel(struct isp_video_buffer *buf)
{
	struct scatterlist *sglist;
	unsigned int npages;
	unsigned int i;
	void *addr;

	addr = buf->vaddr;
	npages = PAGE_ALIGN(buf->vbuf.length) >> PAGE_SHIFT;

	sglist = vmalloc(npages * sizeof(*sglist));
	if (sglist == NULL)
		return -ENOMEM;

	sg_init_table(sglist, npages);

	for (i = 0; i < npages; ++i, addr += PAGE_SIZE) {
		struct page *page = vmalloc_to_page(addr);

		if (page == NULL || PageHighMem(page)) {
			vfree(sglist);
			return -EINVAL;
		}

		sg_set_page(&sglist[i], page, PAGE_SIZE, 0);
	}

	buf->sglen = npages;
	buf->sglist = sglist;

	return 0;
}

static int isp_video_buffer_sglist_user(struct isp_video_buffer *buf)
{
	struct scatterlist *sglist;
	unsigned int offset = buf->offset;
	unsigned int i;

	sglist = vmalloc(buf->npages * sizeof(*sglist));
	if (sglist == NULL)
		return -ENOMEM;

	sg_init_table(sglist, buf->npages);

	for (i = 0; i < buf->npages; ++i) {
		if (PageHighMem(buf->pages[i])) {
			vfree(sglist);
			return -EINVAL;
		}

		sg_set_page(&sglist[i], buf->pages[i], PAGE_SIZE - offset,
			    offset);
		offset = 0;
	}

	buf->sglen = buf->npages;
	buf->sglist = sglist;

	return 0;
}

static int isp_video_buffer_sglist_pfnmap(struct isp_video_buffer *buf)
{
	struct scatterlist *sglist;
	unsigned int offset = buf->offset;
	unsigned long pfn = buf->paddr >> PAGE_SHIFT;
	unsigned int i;

	sglist = vmalloc(buf->npages * sizeof(*sglist));
	if (sglist == NULL)
		return -ENOMEM;

	sg_init_table(sglist, buf->npages);

	for (i = 0; i < buf->npages; ++i, ++pfn) {
		sg_set_page(&sglist[i], pfn_to_page(pfn), PAGE_SIZE - offset,
			    offset);
		sg_dma_address(&sglist[i]) = (pfn << PAGE_SHIFT) + offset;
		offset = 0;
	}

	buf->sglen = buf->npages;
	buf->sglist = sglist;

	return 0;
}

static void isp_video_buffer_cleanup(struct isp_video_buffer *buf)
{
	enum dma_data_direction direction;
	unsigned int i;

	if (buf->queue->ops->buffer_cleanup)
		buf->queue->ops->buffer_cleanup(buf);

	if (!(buf->vm_flags & VM_PFNMAP)) {
		direction = buf->vbuf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE
			  ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
		dma_unmap_sg(buf->queue->dev, buf->sglist, buf->sglen,
			     direction);
	}

	vfree(buf->sglist);
	buf->sglist = NULL;
	buf->sglen = 0;

	if (buf->pages != NULL) {
		isp_video_buffer_lock_vma(buf, 0);

		for (i = 0; i < buf->npages; ++i)
			page_cache_release(buf->pages[i]);

		vfree(buf->pages);
		buf->pages = NULL;
	}

	buf->npages = 0;
	buf->skip_cache = false;
}

/*
 * isp_video_buffer_prepare_user - Pin userspace VMA pages to memory.
 *
 * This function creates a list of pages for a userspace VMA. The number of
 * pages is first computed based on the buffer size, and pages are then
 * retrieved by a call to get_user_pages.
 *
 * Pages are pinned to memory by get_user_pages, making them available for DMA
 * transfers. However, due to memory management optimization, it seems the
 * get_user_pages doesn't guarantee that the pinned pages will not be written
 * to swap and removed from the userspace mapping(s). When this happens, a page
 * fault can be generated when accessing those unmapped pages.
 *
 * If the fault is triggered by a page table walk caused by VIPT cache
 * management operations, the page fault handler might oops if the MM semaphore
 * is held, as it can't handle kernel page faults in that case. To fix that, a
 * fixup entry needs to be added to the cache management code, or the userspace
 * VMA must be locked to avoid removing pages from the userspace mapping in the
 * first place.
 *
 * If the number of pages retrieved is smaller than the number required by the
 * buffer size, the function returns -EFAULT.
 */
static int isp_video_buffer_prepare_user(struct isp_video_buffer *buf)
{
	unsigned long data;
	unsigned int first;
	unsigned int last;
	int ret;

	data = buf->vbuf.m.userptr;
	first = (data & PAGE_MASK) >> PAGE_SHIFT;
	last = ((data + buf->vbuf.length - 1) & PAGE_MASK) >> PAGE_SHIFT;

	buf->offset = data & ~PAGE_MASK;
	buf->npages = last - first + 1;
	buf->pages = vmalloc(buf->npages * sizeof(buf->pages[0]));
	if (buf->pages == NULL)
		return -ENOMEM;

	down_read(&current->mm->mmap_sem);
	ret = get_user_pages(current, current->mm, data & PAGE_MASK,
			     buf->npages,
			     buf->vbuf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE, 0,
			     buf->pages, NULL);
	up_read(&current->mm->mmap_sem);

	if (ret != buf->npages) {
		buf->npages = ret < 0 ? 0 : ret;
		isp_video_buffer_cleanup(buf);
		return -EFAULT;
	}

	ret = isp_video_buffer_lock_vma(buf, 1);
	if (ret < 0)
		isp_video_buffer_cleanup(buf);

	return ret;
}

static int isp_video_buffer_prepare_pfnmap(struct isp_video_buffer *buf)
{
	struct vm_area_struct *vma;
	unsigned long prev_pfn;
	unsigned long this_pfn;
	unsigned long start;
	unsigned long end;
	dma_addr_t pa;
	int ret = -EFAULT;

	start = buf->vbuf.m.userptr;
	end = buf->vbuf.m.userptr + buf->vbuf.length - 1;

	buf->offset = start & ~PAGE_MASK;
	buf->npages = (end >> PAGE_SHIFT) - (start >> PAGE_SHIFT) + 1;
	buf->pages = NULL;

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, start);
	if (vma == NULL || vma->vm_end < end)
		goto done;

	for (prev_pfn = 0; start <= end; start += PAGE_SIZE) {
		ret = follow_pfn(vma, start, &this_pfn);
		if (ret)
			goto done;

		if (prev_pfn == 0)
			pa = this_pfn << PAGE_SHIFT;
		else if (this_pfn != prev_pfn + 1) {
			ret = -EFAULT;
			goto done;
		}

		prev_pfn = this_pfn;
	}

	buf->paddr = pa + buf->offset;
	ret = 0;

done:
	up_read(&current->mm->mmap_sem);
	return ret;
}

static int isp_video_buffer_prepare_vm_flags(struct isp_video_buffer *buf)
{
	struct vm_area_struct *vma;
	pgprot_t vm_page_prot;
	unsigned long start;
	unsigned long end;
	int ret = -EFAULT;

	start = buf->vbuf.m.userptr;
	end = buf->vbuf.m.userptr + buf->vbuf.length - 1;

	down_read(&current->mm->mmap_sem);

	do {
		vma = find_vma(current->mm, start);
		if (vma == NULL)
			goto done;

		if (start == buf->vbuf.m.userptr) {
			buf->vm_flags = vma->vm_flags;
			vm_page_prot = vma->vm_page_prot;
		}

		if ((buf->vm_flags ^ vma->vm_flags) & VM_PFNMAP)
			goto done;

		if (vm_page_prot != vma->vm_page_prot)
			goto done;

		start = vma->vm_end + 1;
	} while (vma->vm_end < end);

	if (vm_page_prot == pgprot_noncached(vm_page_prot) ||
	    vm_page_prot == pgprot_writecombine(vm_page_prot))
		buf->skip_cache = true;

	ret = 0;

done:
	up_read(&current->mm->mmap_sem);
	return ret;
}

static int isp_video_buffer_prepare(struct isp_video_buffer *buf)
{
	enum dma_data_direction direction;
	int ret;

	switch (buf->vbuf.memory) {
	case V4L2_MEMORY_MMAP:
		ret = isp_video_buffer_sglist_kernel(buf);
		break;

	case V4L2_MEMORY_USERPTR:
		ret = isp_video_buffer_prepare_vm_flags(buf);
		if (ret < 0)
			return ret;

		if (buf->vm_flags & VM_PFNMAP) {
			ret = isp_video_buffer_prepare_pfnmap(buf);
			if (ret < 0)
				return ret;

			ret = isp_video_buffer_sglist_pfnmap(buf);
		} else {
			ret = isp_video_buffer_prepare_user(buf);
			if (ret < 0)
				return ret;

			ret = isp_video_buffer_sglist_user(buf);
		}
		break;

	default:
		return -EINVAL;
	}

	if (ret < 0)
		goto done;

	if (!(buf->vm_flags & VM_PFNMAP)) {
		direction = buf->vbuf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE
			  ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
		ret = dma_map_sg(buf->queue->dev, buf->sglist, buf->sglen,
				 direction);
		if (ret != buf->sglen) {
			ret = -EFAULT;
			goto done;
		}
	}

	if (buf->queue->ops->buffer_prepare)
		ret = buf->queue->ops->buffer_prepare(buf);

done:
	if (ret < 0) {
		isp_video_buffer_cleanup(buf);
		return ret;
	}

	return ret;
}

static void isp_video_buffer_query(struct isp_video_buffer *buf,
				   struct v4l2_buffer *vbuf)
{
	memcpy(vbuf, &buf->vbuf, sizeof(*vbuf));

	if (buf->vma_use_count)
		vbuf->flags |= V4L2_BUF_FLAG_MAPPED;

	switch (buf->state) {
	case ISP_BUF_STATE_ERROR:
		vbuf->flags |= V4L2_BUF_FLAG_ERROR;
	case ISP_BUF_STATE_DONE:
		vbuf->flags |= V4L2_BUF_FLAG_DONE;
	case ISP_BUF_STATE_QUEUED:
	case ISP_BUF_STATE_ACTIVE:
		vbuf->flags |= V4L2_BUF_FLAG_QUEUED;
		break;
	case ISP_BUF_STATE_IDLE:
	default:
		break;
	}
}

static int isp_video_buffer_wait(struct isp_video_buffer *buf, int nonblocking)
{
	if (nonblocking) {
		return (buf->state != ISP_BUF_STATE_QUEUED &&
			buf->state != ISP_BUF_STATE_ACTIVE)
			? 0 : -EAGAIN;
	}

	return wait_event_interruptible(buf->wait,
		buf->state != ISP_BUF_STATE_QUEUED &&
		buf->state != ISP_BUF_STATE_ACTIVE);
}


static int isp_video_queue_free(struct isp_video_queue *queue)
{
	unsigned int i;

	if (queue->streaming)
		return -EBUSY;

	for (i = 0; i < queue->count; ++i) {
		if (queue->buffers[i]->vma_use_count != 0)
			return -EBUSY;
	}

	for (i = 0; i < queue->count; ++i) {
		struct isp_video_buffer *buf = queue->buffers[i];

		isp_video_buffer_cleanup(buf);

		vfree(buf->vaddr);
		buf->vaddr = NULL;

		kfree(buf);
		queue->buffers[i] = NULL;
	}

	INIT_LIST_HEAD(&queue->queue);
	queue->count = 0;
	return 0;
}

static int isp_video_queue_alloc(struct isp_video_queue *queue,
				 unsigned int nbuffers,
				 unsigned int size, enum v4l2_memory memory)
{
	struct isp_video_buffer *buf;
	unsigned int i;
	void *mem;
	int ret;

	
	ret = isp_video_queue_free(queue);
	if (ret < 0)
		return ret;

	
	if (nbuffers == 0)
		return 0;

	
	for (i = 0; i < nbuffers; ++i) {
		buf = kzalloc(queue->bufsize, GFP_KERNEL);
		if (buf == NULL)
			break;

		if (memory == V4L2_MEMORY_MMAP) {
			mem = vmalloc_32_user(PAGE_ALIGN(size));
			if (mem == NULL) {
				kfree(buf);
				break;
			}

			buf->vbuf.m.offset = i * PAGE_ALIGN(size);
			buf->vaddr = mem;
		}

		buf->vbuf.index = i;
		buf->vbuf.length = size;
		buf->vbuf.type = queue->type;
		buf->vbuf.field = V4L2_FIELD_NONE;
		buf->vbuf.memory = memory;

		buf->queue = queue;
		init_waitqueue_head(&buf->wait);

		queue->buffers[i] = buf;
	}

	if (i == 0)
		return -ENOMEM;

	queue->count = i;
	return nbuffers;
}

int omap3isp_video_queue_cleanup(struct isp_video_queue *queue)
{
	return isp_video_queue_free(queue);
}

int omap3isp_video_queue_init(struct isp_video_queue *queue,
			      enum v4l2_buf_type type,
			      const struct isp_video_queue_operations *ops,
			      struct device *dev, unsigned int bufsize)
{
	INIT_LIST_HEAD(&queue->queue);
	mutex_init(&queue->lock);
	spin_lock_init(&queue->irqlock);

	queue->type = type;
	queue->ops = ops;
	queue->dev = dev;
	queue->bufsize = bufsize;

	return 0;
}


int omap3isp_video_queue_reqbufs(struct isp_video_queue *queue,
				 struct v4l2_requestbuffers *rb)
{
	unsigned int nbuffers = rb->count;
	unsigned int size;
	int ret;

	if (rb->type != queue->type)
		return -EINVAL;

	queue->ops->queue_prepare(queue, &nbuffers, &size);
	if (size == 0)
		return -EINVAL;

	nbuffers = min_t(unsigned int, nbuffers, ISP_VIDEO_MAX_BUFFERS);

	mutex_lock(&queue->lock);

	ret = isp_video_queue_alloc(queue, nbuffers, size, rb->memory);
	if (ret < 0)
		goto done;

	rb->count = ret;
	ret = 0;

done:
	mutex_unlock(&queue->lock);
	return ret;
}

int omap3isp_video_queue_querybuf(struct isp_video_queue *queue,
				  struct v4l2_buffer *vbuf)
{
	struct isp_video_buffer *buf;
	int ret = 0;

	if (vbuf->type != queue->type)
		return -EINVAL;

	mutex_lock(&queue->lock);

	if (vbuf->index >= queue->count) {
		ret = -EINVAL;
		goto done;
	}

	buf = queue->buffers[vbuf->index];
	isp_video_buffer_query(buf, vbuf);

done:
	mutex_unlock(&queue->lock);
	return ret;
}

int omap3isp_video_queue_qbuf(struct isp_video_queue *queue,
			      struct v4l2_buffer *vbuf)
{
	struct isp_video_buffer *buf;
	unsigned long flags;
	int ret = -EINVAL;

	if (vbuf->type != queue->type)
		goto done;

	mutex_lock(&queue->lock);

	if (vbuf->index >= queue->count)
		goto done;

	buf = queue->buffers[vbuf->index];

	if (vbuf->memory != buf->vbuf.memory)
		goto done;

	if (buf->state != ISP_BUF_STATE_IDLE)
		goto done;

	if (vbuf->memory == V4L2_MEMORY_USERPTR &&
	    vbuf->length < buf->vbuf.length)
		goto done;

	if (vbuf->memory == V4L2_MEMORY_USERPTR &&
	    vbuf->m.userptr != buf->vbuf.m.userptr) {
		isp_video_buffer_cleanup(buf);
		buf->vbuf.m.userptr = vbuf->m.userptr;
		buf->prepared = 0;
	}

	if (!buf->prepared) {
		ret = isp_video_buffer_prepare(buf);
		if (ret < 0)
			goto done;
		buf->prepared = 1;
	}

	isp_video_buffer_cache_sync(buf);

	buf->state = ISP_BUF_STATE_QUEUED;
	list_add_tail(&buf->stream, &queue->queue);

	if (queue->streaming) {
		spin_lock_irqsave(&queue->irqlock, flags);
		queue->ops->buffer_queue(buf);
		spin_unlock_irqrestore(&queue->irqlock, flags);
	}

	ret = 0;

done:
	mutex_unlock(&queue->lock);
	return ret;
}

int omap3isp_video_queue_dqbuf(struct isp_video_queue *queue,
			       struct v4l2_buffer *vbuf, int nonblocking)
{
	struct isp_video_buffer *buf;
	int ret;

	if (vbuf->type != queue->type)
		return -EINVAL;

	mutex_lock(&queue->lock);

	if (list_empty(&queue->queue)) {
		ret = -EINVAL;
		goto done;
	}

	buf = list_first_entry(&queue->queue, struct isp_video_buffer, stream);
	ret = isp_video_buffer_wait(buf, nonblocking);
	if (ret < 0)
		goto done;

	list_del(&buf->stream);

	isp_video_buffer_query(buf, vbuf);
	buf->state = ISP_BUF_STATE_IDLE;
	vbuf->flags &= ~V4L2_BUF_FLAG_QUEUED;

done:
	mutex_unlock(&queue->lock);
	return ret;
}

int omap3isp_video_queue_streamon(struct isp_video_queue *queue)
{
	struct isp_video_buffer *buf;
	unsigned long flags;

	mutex_lock(&queue->lock);

	if (queue->streaming)
		goto done;

	queue->streaming = 1;

	spin_lock_irqsave(&queue->irqlock, flags);
	list_for_each_entry(buf, &queue->queue, stream)
		queue->ops->buffer_queue(buf);
	spin_unlock_irqrestore(&queue->irqlock, flags);

done:
	mutex_unlock(&queue->lock);
	return 0;
}

void omap3isp_video_queue_streamoff(struct isp_video_queue *queue)
{
	struct isp_video_buffer *buf;
	unsigned long flags;
	unsigned int i;

	mutex_lock(&queue->lock);

	if (!queue->streaming)
		goto done;

	queue->streaming = 0;

	spin_lock_irqsave(&queue->irqlock, flags);
	for (i = 0; i < queue->count; ++i) {
		buf = queue->buffers[i];

		if (buf->state == ISP_BUF_STATE_ACTIVE)
			wake_up(&buf->wait);

		buf->state = ISP_BUF_STATE_IDLE;
	}
	spin_unlock_irqrestore(&queue->irqlock, flags);

	INIT_LIST_HEAD(&queue->queue);

done:
	mutex_unlock(&queue->lock);
}

void omap3isp_video_queue_discard_done(struct isp_video_queue *queue)
{
	struct isp_video_buffer *buf;
	unsigned int i;

	mutex_lock(&queue->lock);

	if (!queue->streaming)
		goto done;

	for (i = 0; i < queue->count; ++i) {
		buf = queue->buffers[i];

		if (buf->state == ISP_BUF_STATE_DONE)
			buf->state = ISP_BUF_STATE_ERROR;
	}

done:
	mutex_unlock(&queue->lock);
}

static void isp_video_queue_vm_open(struct vm_area_struct *vma)
{
	struct isp_video_buffer *buf = vma->vm_private_data;

	buf->vma_use_count++;
}

static void isp_video_queue_vm_close(struct vm_area_struct *vma)
{
	struct isp_video_buffer *buf = vma->vm_private_data;

	buf->vma_use_count--;
}

static const struct vm_operations_struct isp_video_queue_vm_ops = {
	.open = isp_video_queue_vm_open,
	.close = isp_video_queue_vm_close,
};

int omap3isp_video_queue_mmap(struct isp_video_queue *queue,
			 struct vm_area_struct *vma)
{
	struct isp_video_buffer *uninitialized_var(buf);
	unsigned long size;
	unsigned int i;
	int ret = 0;

	mutex_lock(&queue->lock);

	for (i = 0; i < queue->count; ++i) {
		buf = queue->buffers[i];
		if ((buf->vbuf.m.offset >> PAGE_SHIFT) == vma->vm_pgoff)
			break;
	}

	if (i == queue->count) {
		ret = -EINVAL;
		goto done;
	}

	size = vma->vm_end - vma->vm_start;

	if (buf->vbuf.memory != V4L2_MEMORY_MMAP ||
	    size != PAGE_ALIGN(buf->vbuf.length)) {
		ret = -EINVAL;
		goto done;
	}

	ret = remap_vmalloc_range(vma, buf->vaddr, 0);
	if (ret < 0)
		goto done;

	vma->vm_ops = &isp_video_queue_vm_ops;
	vma->vm_private_data = buf;
	isp_video_queue_vm_open(vma);

done:
	mutex_unlock(&queue->lock);
	return ret;
}

unsigned int omap3isp_video_queue_poll(struct isp_video_queue *queue,
				       struct file *file, poll_table *wait)
{
	struct isp_video_buffer *buf;
	unsigned int mask = 0;

	mutex_lock(&queue->lock);
	if (list_empty(&queue->queue)) {
		mask |= POLLERR;
		goto done;
	}
	buf = list_first_entry(&queue->queue, struct isp_video_buffer, stream);

	poll_wait(file, &buf->wait, wait);
	if (buf->state == ISP_BUF_STATE_DONE ||
	    buf->state == ISP_BUF_STATE_ERROR) {
		if (queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
			mask |= POLLIN | POLLRDNORM;
		else
			mask |= POLLOUT | POLLWRNORM;
	}

done:
	mutex_unlock(&queue->lock);
	return mask;
}
