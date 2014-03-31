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

#ifndef _TTM_BO_API_H_
#define _TTM_BO_API_H_

#include "drm_hashtab.h"
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/bitmap.h>

struct ttm_bo_device;

struct drm_mm_node;


struct ttm_placement {
	unsigned	fpfn;
	unsigned	lpfn;
	unsigned	num_placement;
	const uint32_t	*placement;
	unsigned	num_busy_placement;
	const uint32_t	*busy_placement;
};

struct ttm_bus_placement {
	void		*addr;
	unsigned long	base;
	unsigned long	size;
	unsigned long	offset;
	bool		is_iomem;
	bool		io_reserved_vm;
	uint64_t        io_reserved_count;
};



struct ttm_mem_reg {
	void *mm_node;
	unsigned long start;
	unsigned long size;
	unsigned long num_pages;
	uint32_t page_alignment;
	uint32_t mem_type;
	uint32_t placement;
	struct ttm_bus_placement bus;
};


enum ttm_bo_type {
	ttm_bo_type_device,
	ttm_bo_type_kernel
};

struct ttm_tt;


struct ttm_buffer_object {

	struct ttm_bo_global *glob;
	struct ttm_bo_device *bdev;
	unsigned long buffer_start;
	enum ttm_bo_type type;
	void (*destroy) (struct ttm_buffer_object *);
	unsigned long num_pages;
	uint64_t addr_space_offset;
	size_t acc_size;


	struct kref kref;
	struct kref list_kref;
	wait_queue_head_t event_queue;


	struct ttm_mem_reg mem;
	struct file *persistent_swap_storage;
	struct ttm_tt *ttm;
	bool evicted;

	/**
	 * Members protected by the bo::reserved lock only when written to.
	 */

	atomic_t cpu_writers;


	struct list_head lru;
	struct list_head ddestroy;
	struct list_head swap;
	struct list_head io_reserve_lru;
	uint32_t val_seq;
	bool seq_valid;

	/**
	 * Members protected by the bdev::lru_lock
	 * only when written to.
	 */

	atomic_t reserved;


	void *sync_obj_arg;
	void *sync_obj;
	unsigned long priv_flags;


	struct rb_node vm_rb;
	struct drm_mm_node *vm_node;


	/**
	 * Special members that are protected by the reserve lock
	 * and the bo::lock when written to. Can be read with
	 * either of these locks held.
	 */

	unsigned long offset;
	uint32_t cur_placement;
};


#define TTM_BO_MAP_IOMEM_MASK 0x80
struct ttm_bo_kmap_obj {
	void *virtual;
	struct page *page;
	enum {
		ttm_bo_map_iomap        = 1 | TTM_BO_MAP_IOMEM_MASK,
		ttm_bo_map_vmap         = 2,
		ttm_bo_map_kmap         = 3,
		ttm_bo_map_premapped    = 4 | TTM_BO_MAP_IOMEM_MASK,
	} bo_kmap_type;
	struct ttm_buffer_object *bo;
};


static inline struct ttm_buffer_object *
ttm_bo_reference(struct ttm_buffer_object *bo)
{
	kref_get(&bo->kref);
	return bo;
}

extern int ttm_bo_wait(struct ttm_buffer_object *bo, bool lazy,
		       bool interruptible, bool no_wait);
extern int ttm_bo_validate(struct ttm_buffer_object *bo,
				struct ttm_placement *placement,
				bool interruptible, bool no_wait_reserve,
				bool no_wait_gpu);

extern void ttm_bo_unref(struct ttm_buffer_object **bo);


extern void ttm_bo_list_ref_sub(struct ttm_buffer_object *bo, int count,
				bool never_free);

extern void ttm_bo_add_to_lru(struct ttm_buffer_object *bo);

extern int ttm_bo_del_from_lru(struct ttm_buffer_object *bo);


extern int ttm_bo_lock_delayed_workqueue(struct ttm_bo_device *bdev);

extern void ttm_bo_unlock_delayed_workqueue(struct ttm_bo_device *bdev,
					    int resched);

extern int
ttm_bo_synccpu_write_grab(struct ttm_buffer_object *bo, bool no_wait);

extern void ttm_bo_synccpu_write_release(struct ttm_buffer_object *bo);

size_t ttm_bo_acc_size(struct ttm_bo_device *bdev,
		       unsigned long bo_size,
		       unsigned struct_size);
size_t ttm_bo_dma_acc_size(struct ttm_bo_device *bdev,
			   unsigned long bo_size,
			   unsigned struct_size);


extern int ttm_bo_init(struct ttm_bo_device *bdev,
			struct ttm_buffer_object *bo,
			unsigned long size,
			enum ttm_bo_type type,
			struct ttm_placement *placement,
			uint32_t page_alignment,
			unsigned long buffer_start,
			bool interrubtible,
			struct file *persistent_swap_storage,
			size_t acc_size,
			void (*destroy) (struct ttm_buffer_object *));


extern int ttm_bo_create(struct ttm_bo_device *bdev,
				unsigned long size,
				enum ttm_bo_type type,
				struct ttm_placement *placement,
				uint32_t page_alignment,
				unsigned long buffer_start,
				bool interruptible,
				struct file *persistent_swap_storage,
				struct ttm_buffer_object **p_bo);

extern int ttm_bo_check_placement(struct ttm_buffer_object *bo,
					struct ttm_placement *placement);


extern int ttm_bo_init_mm(struct ttm_bo_device *bdev, unsigned type,
				unsigned long p_size);

extern int ttm_bo_clean_mm(struct ttm_bo_device *bdev, unsigned mem_type);


extern int ttm_bo_evict_mm(struct ttm_bo_device *bdev, unsigned mem_type);


static inline void *ttm_kmap_obj_virtual(struct ttm_bo_kmap_obj *map,
					 bool *is_iomem)
{
	*is_iomem = !!(map->bo_kmap_type & TTM_BO_MAP_IOMEM_MASK);
	return map->virtual;
}


extern int ttm_bo_kmap(struct ttm_buffer_object *bo, unsigned long start_page,
		       unsigned long num_pages, struct ttm_bo_kmap_obj *map);


extern void ttm_bo_kunmap(struct ttm_bo_kmap_obj *map);


extern int ttm_fbdev_mmap(struct vm_area_struct *vma,
			  struct ttm_buffer_object *bo);


extern int ttm_bo_mmap(struct file *filp, struct vm_area_struct *vma,
		       struct ttm_bo_device *bdev);


extern ssize_t ttm_bo_io(struct ttm_bo_device *bdev, struct file *filp,
			 const char __user *wbuf, char __user *rbuf,
			 size_t count, loff_t *f_pos, bool write);

extern void ttm_bo_swapout_all(struct ttm_bo_device *bdev);

#endif
