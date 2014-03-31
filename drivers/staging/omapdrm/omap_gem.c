/*
 * drivers/staging/omapdrm/omap_gem.c
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Rob Clark <rob.clark@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <linux/spinlock.h>
#include <linux/shmem_fs.h>

#include "omap_drv.h"
#include "omap_dmm_tiler.h"

struct page ** _drm_gem_get_pages(struct drm_gem_object *obj, gfp_t gfpmask);
void _drm_gem_put_pages(struct drm_gem_object *obj, struct page **pages,
		bool dirty, bool accessed);
int _drm_gem_create_mmap_offset_size(struct drm_gem_object *obj, size_t size);


#define to_omap_bo(x) container_of(x, struct omap_gem_object, base)

#define OMAP_BO_DMA			0x01000000	
#define OMAP_BO_EXT_SYNC	0x02000000	
#define OMAP_BO_EXT_MEM		0x04000000	


struct omap_gem_object {
	struct drm_gem_object base;

	struct list_head mm_list;

	uint32_t flags;

	
	uint16_t width, height;

	
	uint32_t roll;

	dma_addr_t paddr;

	uint32_t paddr_cnt;

	struct tiler_block *block;

	struct page **pages;

	
	dma_addr_t *addrs;

	void *vaddr;

	struct {
		uint32_t write_pending;
		uint32_t write_complete;
		uint32_t read_pending;
		uint32_t read_complete;
	} *sync;
};

static int get_pages(struct drm_gem_object *obj, struct page ***pages);
static uint64_t mmap_offset(struct drm_gem_object *obj);

#define NUM_USERGART_ENTRIES 2
struct usergart_entry {
	struct tiler_block *block;	
	dma_addr_t paddr;
	struct drm_gem_object *obj;	
	pgoff_t obj_pgoff;		
};
static struct {
	struct usergart_entry entry[NUM_USERGART_ENTRIES];
	int height;				
	int height_shift;		
	int slot_shift;			
	int stride_pfn;			
	int last;				
} *usergart;

static void evict_entry(struct drm_gem_object *obj,
		enum tiler_fmt fmt, struct usergart_entry *entry)
{
	if (obj->dev->dev_mapping) {
		struct omap_gem_object *omap_obj = to_omap_bo(obj);
		int n = usergart[fmt].height;
		size_t size = PAGE_SIZE * n;
		loff_t off = mmap_offset(obj) +
				(entry->obj_pgoff << PAGE_SHIFT);
		const int m = 1 + ((omap_obj->width << fmt) / PAGE_SIZE);
		if (m > 1) {
			int i;
			
			for (i = n; i > 0; i--) {
				unmap_mapping_range(obj->dev->dev_mapping,
						off, PAGE_SIZE, 1);
				off += PAGE_SIZE * m;
			}
		} else {
			unmap_mapping_range(obj->dev->dev_mapping, off, size, 1);
		}
	}

	entry->obj = NULL;
}

static void evict(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);

	if (omap_obj->flags & OMAP_BO_TILED) {
		enum tiler_fmt fmt = gem2fmt(omap_obj->flags);
		int i;

		if (!usergart)
			return;

		for (i = 0; i < NUM_USERGART_ENTRIES; i++) {
			struct usergart_entry *entry = &usergart[fmt].entry[i];
			if (entry->obj == obj)
				evict_entry(obj, fmt, entry);
		}
	}
}

static inline bool is_shmem(struct drm_gem_object *obj)
{
	return obj->filp != NULL;
}

static DEFINE_SPINLOCK(sync_lock);

static int omap_gem_attach_pages(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	struct page **pages;

	WARN_ON(omap_obj->pages);

	pages = _drm_gem_get_pages(obj, GFP_KERNEL);
	if (IS_ERR(pages)) {
		dev_err(obj->dev->dev, "could not get pages: %ld\n", PTR_ERR(pages));
		return PTR_ERR(pages);
	}

	if (omap_obj->flags & (OMAP_BO_WC|OMAP_BO_UNCACHED)) {
		int i, npages = obj->size >> PAGE_SHIFT;
		dma_addr_t *addrs = kmalloc(npages * sizeof(addrs), GFP_KERNEL);
		for (i = 0; i < npages; i++) {
			addrs[i] = dma_map_page(obj->dev->dev, pages[i],
					0, PAGE_SIZE, DMA_BIDIRECTIONAL);
		}
		omap_obj->addrs = addrs;
	}

	omap_obj->pages = pages;
	return 0;
}

static void omap_gem_detach_pages(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);

	if (omap_obj->flags & (OMAP_BO_WC|OMAP_BO_UNCACHED)) {
		int i, npages = obj->size >> PAGE_SHIFT;
		for (i = 0; i < npages; i++) {
			dma_unmap_page(obj->dev->dev, omap_obj->addrs[i],
					PAGE_SIZE, DMA_BIDIRECTIONAL);
		}
		kfree(omap_obj->addrs);
		omap_obj->addrs = NULL;
	}

	_drm_gem_put_pages(obj, omap_obj->pages, true, false);
	omap_obj->pages = NULL;
}

static uint64_t mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	if (!obj->map_list.map) {
		
		size_t size = omap_gem_mmap_size(obj);
		int ret = _drm_gem_create_mmap_offset_size(obj, size);

		if (ret) {
			dev_err(dev->dev, "could not allocate mmap offset\n");
			return 0;
		}
	}

	return (uint64_t)obj->map_list.hash.key << PAGE_SHIFT;
}

uint64_t omap_gem_mmap_offset(struct drm_gem_object *obj)
{
	uint64_t offset;
	mutex_lock(&obj->dev->struct_mutex);
	offset = mmap_offset(obj);
	mutex_unlock(&obj->dev->struct_mutex);
	return offset;
}

size_t omap_gem_mmap_size(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	size_t size = obj->size;

	if (omap_obj->flags & OMAP_BO_TILED) {
		size = tiler_vsize(gem2fmt(omap_obj->flags),
				omap_obj->width, omap_obj->height);
	}

	return size;
}


static int fault_1d(struct drm_gem_object *obj,
		struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	unsigned long pfn;
	pgoff_t pgoff;

	
	pgoff = ((unsigned long)vmf->virtual_address -
			vma->vm_start) >> PAGE_SHIFT;

	if (omap_obj->pages) {
		pfn = page_to_pfn(omap_obj->pages[pgoff]);
	} else {
		BUG_ON(!(omap_obj->flags & OMAP_BO_DMA));
		pfn = (omap_obj->paddr >> PAGE_SHIFT) + pgoff;
	}

	VERB("Inserting %p pfn %lx, pa %lx", vmf->virtual_address,
			pfn, pfn << PAGE_SHIFT);

	return vm_insert_mixed(vma, (unsigned long)vmf->virtual_address, pfn);
}

static int fault_2d(struct drm_gem_object *obj,
		struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	struct usergart_entry *entry;
	enum tiler_fmt fmt = gem2fmt(omap_obj->flags);
	struct page *pages[64];  
	unsigned long pfn;
	pgoff_t pgoff, base_pgoff;
	void __user *vaddr;
	int i, ret, slots;

	const int n = usergart[fmt].height;
	const int n_shift = usergart[fmt].height_shift;

	const int m = 1 + ((omap_obj->width << fmt) / PAGE_SIZE);

	
	pgoff = ((unsigned long)vmf->virtual_address -
			vma->vm_start) >> PAGE_SHIFT;

	base_pgoff = round_down(pgoff, m << n_shift);

	
	slots = omap_obj->width >> usergart[fmt].slot_shift;

	vaddr = vmf->virtual_address - ((pgoff - base_pgoff) << PAGE_SHIFT);

	entry = &usergart[fmt].entry[usergart[fmt].last];

	
	if (entry->obj)
		evict_entry(entry->obj, fmt, entry);

	entry->obj = obj;
	entry->obj_pgoff = base_pgoff;

	
	base_pgoff = (base_pgoff >> n_shift) * slots;

	
	if (m > 1) {
		int off = pgoff % m;
		entry->obj_pgoff += off;
		base_pgoff /= m;
		slots = min(slots - (off << n_shift), n);
		base_pgoff += off << n_shift;
		vaddr += off << PAGE_SHIFT;
	}

	memcpy(pages, &omap_obj->pages[base_pgoff],
			sizeof(struct page *) * slots);
	memset(pages + slots, 0,
			sizeof(struct page *) * (n - slots));

	ret = tiler_pin(entry->block, pages, ARRAY_SIZE(pages), 0, true);
	if (ret) {
		dev_err(obj->dev->dev, "failed to pin: %d\n", ret);
		return ret;
	}

	pfn = entry->paddr >> PAGE_SHIFT;

	VERB("Inserting %p pfn %lx, pa %lx", vmf->virtual_address,
			pfn, pfn << PAGE_SHIFT);

	for (i = n; i > 0; i--) {
		vm_insert_mixed(vma, (unsigned long)vaddr, pfn);
		pfn += usergart[fmt].stride_pfn;
		vaddr += PAGE_SIZE * m;
	}

	
	usergart[fmt].last = (usergart[fmt].last + 1) % NUM_USERGART_ENTRIES;

	return 0;
}

int omap_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	struct drm_device *dev = obj->dev;
	struct page **pages;
	int ret;

	mutex_lock(&dev->struct_mutex);

	
	ret = get_pages(obj, &pages);
	if (ret) {
		goto fail;
	}


	if (omap_obj->flags & OMAP_BO_TILED)
		ret = fault_2d(obj, vma, vmf);
	else
		ret = fault_1d(obj, vma, vmf);


fail:
	mutex_unlock(&dev->struct_mutex);
	switch (ret) {
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
}

int omap_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct omap_gem_object *omap_obj;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret) {
		DBG("mmap failed: %d", ret);
		return ret;
	}

	
	omap_obj = to_omap_bo(vma->vm_private_data);

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_flags |= VM_MIXEDMAP;

	if (omap_obj->flags & OMAP_BO_WC) {
		vma->vm_page_prot = pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
	} else if (omap_obj->flags & OMAP_BO_UNCACHED) {
		vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));
	} else {
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	}

	return ret;
}

int omap_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
		struct drm_mode_create_dumb *args)
{
	union omap_gem_size gsize;

	
	args->pitch = align_pitch(args->pitch, args->width, args->bpp);
	args->size = PAGE_ALIGN(args->pitch * args->height);

	gsize = (union omap_gem_size){
		.bytes = args->size,
	};

	return omap_gem_new_handle(dev, file, gsize,
			OMAP_BO_SCANOUT | OMAP_BO_WC, &args->handle);
}

int omap_gem_dumb_destroy(struct drm_file *file, struct drm_device *dev,
		uint32_t handle)
{
	
	return drm_gem_handle_delete(file, handle);
}

int omap_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
		uint32_t handle, uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;

	
	obj = drm_gem_object_lookup(dev, file, handle);
	if (obj == NULL) {
		ret = -ENOENT;
		goto fail;
	}

	*offset = omap_gem_mmap_offset(obj);

	drm_gem_object_unreference_unlocked(obj);

fail:
	return ret;
}

int omap_gem_roll(struct drm_gem_object *obj, uint32_t roll)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	uint32_t npages = obj->size >> PAGE_SHIFT;
	int ret = 0;

	if (roll > npages) {
		dev_err(obj->dev->dev, "invalid roll: %d\n", roll);
		return -EINVAL;
	}

	omap_obj->roll = roll;

	mutex_lock(&obj->dev->struct_mutex);

	
	if (omap_obj->block) {
		struct page **pages;
		ret = get_pages(obj, &pages);
		if (ret)
			goto fail;
		ret = tiler_pin(omap_obj->block, pages, npages, roll, true);
		if (ret)
			dev_err(obj->dev->dev, "could not repin: %d\n", ret);
	}

fail:
	mutex_unlock(&obj->dev->struct_mutex);

	return ret;
}

int omap_gem_get_paddr(struct drm_gem_object *obj,
		dma_addr_t *paddr, bool remap)
{
	struct omap_drm_private *priv = obj->dev->dev_private;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = 0;

	mutex_lock(&obj->dev->struct_mutex);

	if (remap && is_shmem(obj) && priv->has_dmm) {
		if (omap_obj->paddr_cnt == 0) {
			struct page **pages;
			uint32_t npages = obj->size >> PAGE_SHIFT;
			enum tiler_fmt fmt = gem2fmt(omap_obj->flags);
			struct tiler_block *block;

			BUG_ON(omap_obj->block);

			ret = get_pages(obj, &pages);
			if (ret)
				goto fail;

			if (omap_obj->flags & OMAP_BO_TILED) {
				block = tiler_reserve_2d(fmt,
						omap_obj->width,
						omap_obj->height, 0);
			} else {
				block = tiler_reserve_1d(obj->size);
			}

			if (IS_ERR(block)) {
				ret = PTR_ERR(block);
				dev_err(obj->dev->dev,
					"could not remap: %d (%d)\n", ret, fmt);
				goto fail;
			}

			
			ret = tiler_pin(block, pages, npages,
					omap_obj->roll, true);
			if (ret) {
				tiler_release(block);
				dev_err(obj->dev->dev,
						"could not pin: %d\n", ret);
				goto fail;
			}

			omap_obj->paddr = tiler_ssptr(block);
			omap_obj->block = block;

			DBG("got paddr: %08x", omap_obj->paddr);
		}

		omap_obj->paddr_cnt++;

		*paddr = omap_obj->paddr;
	} else if (omap_obj->flags & OMAP_BO_DMA) {
		*paddr = omap_obj->paddr;
	} else {
		ret = -EINVAL;
	}

fail:
	mutex_unlock(&obj->dev->struct_mutex);

	return ret;
}

int omap_gem_put_paddr(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = 0;

	mutex_lock(&obj->dev->struct_mutex);
	if (omap_obj->paddr_cnt > 0) {
		omap_obj->paddr_cnt--;
		if (omap_obj->paddr_cnt == 0) {
			ret = tiler_unpin(omap_obj->block);
			if (ret) {
				dev_err(obj->dev->dev,
					"could not unpin pages: %d\n", ret);
				goto fail;
			}
			ret = tiler_release(omap_obj->block);
			if (ret) {
				dev_err(obj->dev->dev,
					"could not release unmap: %d\n", ret);
			}
			omap_obj->block = NULL;
		}
	}
fail:
	mutex_unlock(&obj->dev->struct_mutex);
	return ret;
}

static int get_pages(struct drm_gem_object *obj, struct page ***pages)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = 0;

	if (is_shmem(obj) && !omap_obj->pages) {
		ret = omap_gem_attach_pages(obj);
		if (ret) {
			dev_err(obj->dev->dev, "could not attach pages\n");
			return ret;
		}
	}

	
	*pages = omap_obj->pages;

	return 0;
}

int omap_gem_get_pages(struct drm_gem_object *obj, struct page ***pages)
{
	int ret;
	mutex_lock(&obj->dev->struct_mutex);
	ret = get_pages(obj, pages);
	mutex_unlock(&obj->dev->struct_mutex);
	return ret;
}

int omap_gem_put_pages(struct drm_gem_object *obj)
{
	return 0;
}

void *omap_gem_vaddr(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	WARN_ON(! mutex_is_locked(&obj->dev->struct_mutex));
	if (!omap_obj->vaddr) {
		struct page **pages;
		int ret = get_pages(obj, &pages);
		if (ret)
			return ERR_PTR(ret);
		omap_obj->vaddr = vmap(pages, obj->size >> PAGE_SHIFT,
				VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	}
	return omap_obj->vaddr;
}

#ifdef CONFIG_DEBUG_FS
void omap_gem_describe(struct drm_gem_object *obj, struct seq_file *m)
{
	struct drm_device *dev = obj->dev;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	uint64_t off = 0;

	WARN_ON(! mutex_is_locked(&dev->struct_mutex));

	if (obj->map_list.map)
		off = (uint64_t)obj->map_list.hash.key;

	seq_printf(m, "%08x: %2d (%2d) %08llx %08Zx (%2d) %p %4d",
			omap_obj->flags, obj->name, obj->refcount.refcount.counter,
			off, omap_obj->paddr, omap_obj->paddr_cnt,
			omap_obj->vaddr, omap_obj->roll);

	if (omap_obj->flags & OMAP_BO_TILED) {
		seq_printf(m, " %dx%d", omap_obj->width, omap_obj->height);
		if (omap_obj->block) {
			struct tcm_area *area = &omap_obj->block->area;
			seq_printf(m, " (%dx%d, %dx%d)",
					area->p0.x, area->p0.y,
					area->p1.x, area->p1.y);
		}
	} else {
		seq_printf(m, " %d", obj->size);
	}

	seq_printf(m, "\n");
}

void omap_gem_describe_objects(struct list_head *list, struct seq_file *m)
{
	struct omap_gem_object *omap_obj;
	int count = 0;
	size_t size = 0;

	list_for_each_entry(omap_obj, list, mm_list) {
		struct drm_gem_object *obj = &omap_obj->base;
		seq_printf(m, "   ");
		omap_gem_describe(obj, m);
		count++;
		size += obj->size;
	}

	seq_printf(m, "Total %d objects, %zu bytes\n", count, size);
}
#endif


struct omap_gem_sync_waiter {
	struct list_head list;
	struct omap_gem_object *omap_obj;
	enum omap_gem_op op;
	uint32_t read_target, write_target;
	
	void (*notify)(void *arg);
	void *arg;
};

static LIST_HEAD(waiters);

static inline bool is_waiting(struct omap_gem_sync_waiter *waiter)
{
	struct omap_gem_object *omap_obj = waiter->omap_obj;
	if ((waiter->op & OMAP_GEM_READ) &&
			(omap_obj->sync->read_complete < waiter->read_target))
		return true;
	if ((waiter->op & OMAP_GEM_WRITE) &&
			(omap_obj->sync->write_complete < waiter->write_target))
		return true;
	return false;
}

#define SYNCDBG 0
#define SYNC(fmt, ...) do { if (SYNCDBG) \
		printk(KERN_ERR "%s:%d: "fmt"\n", \
				__func__, __LINE__, ##__VA_ARGS__); \
	} while (0)


static void sync_op_update(void)
{
	struct omap_gem_sync_waiter *waiter, *n;
	list_for_each_entry_safe(waiter, n, &waiters, list) {
		if (!is_waiting(waiter)) {
			list_del(&waiter->list);
			SYNC("notify: %p", waiter);
			waiter->notify(waiter->arg);
			kfree(waiter);
		}
	}
}

static inline int sync_op(struct drm_gem_object *obj,
		enum omap_gem_op op, bool start)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = 0;

	spin_lock(&sync_lock);

	if (!omap_obj->sync) {
		omap_obj->sync = kzalloc(sizeof(*omap_obj->sync), GFP_ATOMIC);
		if (!omap_obj->sync) {
			ret = -ENOMEM;
			goto unlock;
		}
	}

	if (start) {
		if (op & OMAP_GEM_READ)
			omap_obj->sync->read_pending++;
		if (op & OMAP_GEM_WRITE)
			omap_obj->sync->write_pending++;
	} else {
		if (op & OMAP_GEM_READ)
			omap_obj->sync->read_complete++;
		if (op & OMAP_GEM_WRITE)
			omap_obj->sync->write_complete++;
		sync_op_update();
	}

unlock:
	spin_unlock(&sync_lock);

	return ret;
}

void omap_gem_op_update(void)
{
	spin_lock(&sync_lock);
	sync_op_update();
	spin_unlock(&sync_lock);
}

int omap_gem_op_start(struct drm_gem_object *obj, enum omap_gem_op op)
{
	return sync_op(obj, op, true);
}

int omap_gem_op_finish(struct drm_gem_object *obj, enum omap_gem_op op)
{
	return sync_op(obj, op, false);
}

static DECLARE_WAIT_QUEUE_HEAD(sync_event);

static void sync_notify(void *arg)
{
	struct task_struct **waiter_task = arg;
	*waiter_task = NULL;
	wake_up_all(&sync_event);
}

int omap_gem_op_sync(struct drm_gem_object *obj, enum omap_gem_op op)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = 0;
	if (omap_obj->sync) {
		struct task_struct *waiter_task = current;
		struct omap_gem_sync_waiter *waiter =
				kzalloc(sizeof(*waiter), GFP_KERNEL);

		if (!waiter) {
			return -ENOMEM;
		}

		waiter->omap_obj = omap_obj;
		waiter->op = op;
		waiter->read_target = omap_obj->sync->read_pending;
		waiter->write_target = omap_obj->sync->write_pending;
		waiter->notify = sync_notify;
		waiter->arg = &waiter_task;

		spin_lock(&sync_lock);
		if (is_waiting(waiter)) {
			SYNC("waited: %p", waiter);
			list_add_tail(&waiter->list, &waiters);
			spin_unlock(&sync_lock);
			ret = wait_event_interruptible(sync_event,
					(waiter_task == NULL));
			spin_lock(&sync_lock);
			if (waiter_task) {
				SYNC("interrupted: %p", waiter);
				
				list_del(&waiter->list);
				waiter_task = NULL;
			} else {
				
				waiter = NULL;
			}
		}
		spin_unlock(&sync_lock);

		if (waiter) {
			kfree(waiter);
		}
	}
	return ret;
}

int omap_gem_op_async(struct drm_gem_object *obj, enum omap_gem_op op,
		void (*fxn)(void *arg), void *arg)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	if (omap_obj->sync) {
		struct omap_gem_sync_waiter *waiter =
				kzalloc(sizeof(*waiter), GFP_ATOMIC);

		if (!waiter) {
			return -ENOMEM;
		}

		waiter->omap_obj = omap_obj;
		waiter->op = op;
		waiter->read_target = omap_obj->sync->read_pending;
		waiter->write_target = omap_obj->sync->write_pending;
		waiter->notify = fxn;
		waiter->arg = arg;

		spin_lock(&sync_lock);
		if (is_waiting(waiter)) {
			SYNC("waited: %p", waiter);
			list_add_tail(&waiter->list, &waiters);
			spin_unlock(&sync_lock);
			return 0;
		}

		spin_unlock(&sync_lock);
	}

	
	fxn(arg);

	return 0;
}

int omap_gem_set_sync_object(struct drm_gem_object *obj, void *syncobj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = 0;

	spin_lock(&sync_lock);

	if ((omap_obj->flags & OMAP_BO_EXT_SYNC) && !syncobj) {
		
		syncobj = kzalloc(sizeof(*omap_obj->sync), GFP_ATOMIC);
		if (!syncobj) {
			ret = -ENOMEM;
			goto unlock;
		}
		memcpy(syncobj, omap_obj->sync, sizeof(*omap_obj->sync));
		omap_obj->flags &= ~OMAP_BO_EXT_SYNC;
		omap_obj->sync = syncobj;
	} else if (syncobj && !(omap_obj->flags & OMAP_BO_EXT_SYNC)) {
		
		if (omap_obj->sync) {
			memcpy(syncobj, omap_obj->sync, sizeof(*omap_obj->sync));
			kfree(omap_obj->sync);
		}
		omap_obj->flags |= OMAP_BO_EXT_SYNC;
		omap_obj->sync = syncobj;
	}

unlock:
	spin_unlock(&sync_lock);
	return ret;
}

int omap_gem_init_object(struct drm_gem_object *obj)
{
	return -EINVAL;          
}

void omap_gem_free_object(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);

	evict(obj);

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	list_del(&omap_obj->mm_list);

	if (obj->map_list.map) {
		drm_gem_free_mmap_offset(obj);
	}

	WARN_ON(omap_obj->paddr_cnt > 0);

	
	if (!(omap_obj->flags & OMAP_BO_EXT_MEM)) {
		if (omap_obj->pages) {
			omap_gem_detach_pages(obj);
		}
		if (!is_shmem(obj)) {
			dma_free_writecombine(dev->dev, obj->size,
					omap_obj->vaddr, omap_obj->paddr);
		} else if (omap_obj->vaddr) {
			vunmap(omap_obj->vaddr);
		}
	}

	
	if (!(omap_obj->flags & OMAP_BO_EXT_SYNC)) {
		kfree(omap_obj->sync);
	}

	drm_gem_object_release(obj);

	kfree(obj);
}

int omap_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		union omap_gem_size gsize, uint32_t flags, uint32_t *handle)
{
	struct drm_gem_object *obj;
	int ret;

	obj = omap_gem_new(dev, gsize, flags);
	if (!obj)
		return -ENOMEM;

	ret = drm_gem_handle_create(file, obj, handle);
	if (ret) {
		drm_gem_object_release(obj);
		kfree(obj); 
		return ret;
	}

	
	drm_gem_object_unreference_unlocked(obj);

	return 0;
}

struct drm_gem_object *omap_gem_new(struct drm_device *dev,
		union omap_gem_size gsize, uint32_t flags)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_gem_object *omap_obj;
	struct drm_gem_object *obj = NULL;
	size_t size;
	int ret;

	if (flags & OMAP_BO_TILED) {
		if (!usergart) {
			dev_err(dev->dev, "Tiled buffers require DMM\n");
			goto fail;
		}

		flags &= ~OMAP_BO_SCANOUT;

		flags &= ~(OMAP_BO_CACHED|OMAP_BO_UNCACHED);
		flags |= OMAP_BO_WC;

		
		tiler_align(gem2fmt(flags),
				&gsize.tiled.width, &gsize.tiled.height);

		
		size = tiler_size(gem2fmt(flags),
				gsize.tiled.width, gsize.tiled.height);
	} else {
		size = PAGE_ALIGN(gsize.bytes);
	}

	omap_obj = kzalloc(sizeof(*omap_obj), GFP_KERNEL);
	if (!omap_obj) {
		dev_err(dev->dev, "could not allocate GEM object\n");
		goto fail;
	}

	list_add(&omap_obj->mm_list, &priv->obj_list);

	obj = &omap_obj->base;

	if ((flags & OMAP_BO_SCANOUT) && !priv->has_dmm) {
		omap_obj->vaddr =  dma_alloc_writecombine(dev->dev, size,
				&omap_obj->paddr, GFP_KERNEL);
		if (omap_obj->vaddr) {
			flags |= OMAP_BO_DMA;
		}
	}

	omap_obj->flags = flags;

	if (flags & OMAP_BO_TILED) {
		omap_obj->width = gsize.tiled.width;
		omap_obj->height = gsize.tiled.height;
	}

	if (flags & (OMAP_BO_DMA|OMAP_BO_EXT_MEM)) {
		ret = drm_gem_private_object_init(dev, obj, size);
	} else {
		ret = drm_gem_object_init(dev, obj, size);
	}

	if (ret) {
		goto fail;
	}

	return obj;

fail:
	if (obj) {
		omap_gem_free_object(obj);
	}
	return NULL;
}

void omap_gem_init(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;
	const enum tiler_fmt fmts[] = {
			TILFMT_8BIT, TILFMT_16BIT, TILFMT_32BIT
	};
	int i, j;

	if (!dmm_is_initialized()) {
		
		dev_warn(dev->dev, "DMM not available, disable DMM support\n");
		return;
	}

	usergart = kzalloc(3 * sizeof(*usergart), GFP_KERNEL);
	if (!usergart) {
		dev_warn(dev->dev, "could not allocate usergart\n");
		return;
	}

	
	for (i = 0; i < ARRAY_SIZE(fmts); i++) {
		uint16_t h = 1, w = PAGE_SIZE >> i;
		tiler_align(fmts[i], &w, &h);
		usergart[i].height = h;
		usergart[i].height_shift = ilog2(h);
		usergart[i].stride_pfn = tiler_stride(fmts[i]) >> PAGE_SHIFT;
		usergart[i].slot_shift = ilog2((PAGE_SIZE / h) >> i);
		for (j = 0; j < NUM_USERGART_ENTRIES; j++) {
			struct usergart_entry *entry = &usergart[i].entry[j];
			struct tiler_block *block =
					tiler_reserve_2d(fmts[i], w, h,
							PAGE_SIZE);
			if (IS_ERR(block)) {
				dev_err(dev->dev,
						"reserve failed: %d, %d, %ld\n",
						i, j, PTR_ERR(block));
				return;
			}
			entry->paddr = tiler_ssptr(block);
			entry->block = block;

			DBG("%d:%d: %dx%d: paddr=%08x stride=%d", i, j, w, h,
					entry->paddr,
					usergart[i].stride_pfn << PAGE_SHIFT);
		}
	}

	priv->has_dmm = true;
}

void omap_gem_deinit(struct drm_device *dev)
{
	kfree(usergart);
}
