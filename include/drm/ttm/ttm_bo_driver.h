/**************************************************************************
 *
 * Copyright (c) 2006-2009 Vmware, Inc., Palo Alto, CA., USA
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
#ifndef _TTM_BO_DRIVER_H_
#define _TTM_BO_DRIVER_H_

#include "ttm/ttm_bo_api.h"
#include "ttm/ttm_memory.h"
#include "ttm/ttm_module.h"
#include "drm_mm.h"
#include "drm_global.h"
#include "linux/workqueue.h"
#include "linux/fs.h"
#include "linux/spinlock.h"

struct ttm_backend;

struct ttm_backend_func {
	int (*bind) (struct ttm_tt *ttm, struct ttm_mem_reg *bo_mem);

	int (*unbind) (struct ttm_tt *ttm);

	void (*destroy) (struct ttm_tt *ttm);
};

#define TTM_PAGE_FLAG_WRITE           (1 << 3)
#define TTM_PAGE_FLAG_SWAPPED         (1 << 4)
#define TTM_PAGE_FLAG_PERSISTENT_SWAP (1 << 5)
#define TTM_PAGE_FLAG_ZERO_ALLOC      (1 << 6)
#define TTM_PAGE_FLAG_DMA32           (1 << 7)

enum ttm_caching_state {
	tt_uncached,
	tt_wc,
	tt_cached
};


struct ttm_tt {
	struct ttm_bo_device *bdev;
	struct ttm_backend_func *func;
	struct page *dummy_read_page;
	struct page **pages;
	uint32_t page_flags;
	unsigned long num_pages;
	struct ttm_bo_global *glob;
	struct ttm_backend *be;
	struct file *swap_storage;
	enum ttm_caching_state caching_state;
	enum {
		tt_bound,
		tt_unbound,
		tt_unpopulated,
	} state;
};

struct ttm_dma_tt {
	struct ttm_tt ttm;
	dma_addr_t *dma_address;
	struct list_head pages_list;
};

#define TTM_MEMTYPE_FLAG_FIXED         (1 << 0)	
#define TTM_MEMTYPE_FLAG_MAPPABLE      (1 << 1)	
#define TTM_MEMTYPE_FLAG_CMA           (1 << 3)	

struct ttm_mem_type_manager;

struct ttm_mem_type_manager_func {
	int  (*init)(struct ttm_mem_type_manager *man, unsigned long p_size);

	int  (*takedown)(struct ttm_mem_type_manager *man);

	int  (*get_node)(struct ttm_mem_type_manager *man,
			 struct ttm_buffer_object *bo,
			 struct ttm_placement *placement,
			 struct ttm_mem_reg *mem);

	void (*put_node)(struct ttm_mem_type_manager *man,
			 struct ttm_mem_reg *mem);

	void (*debug)(struct ttm_mem_type_manager *man, const char *prefix);
};




struct ttm_mem_type_manager {
	struct ttm_bo_device *bdev;


	bool has_type;
	bool use_type;
	uint32_t flags;
	unsigned long gpu_offset;
	uint64_t size;
	uint32_t available_caching;
	uint32_t default_caching;
	const struct ttm_mem_type_manager_func *func;
	void *priv;
	struct mutex io_reserve_mutex;
	bool use_io_reserve_lru;
	bool io_reserve_fastpath;


	struct list_head io_reserve_lru;


	struct list_head lru;
};


struct ttm_bo_driver {
	struct ttm_tt *(*ttm_tt_create)(struct ttm_bo_device *bdev,
					unsigned long size,
					uint32_t page_flags,
					struct page *dummy_read_page);

	int (*ttm_tt_populate)(struct ttm_tt *ttm);

	void (*ttm_tt_unpopulate)(struct ttm_tt *ttm);


	int (*invalidate_caches) (struct ttm_bo_device *bdev, uint32_t flags);
	int (*init_mem_type) (struct ttm_bo_device *bdev, uint32_t type,
			      struct ttm_mem_type_manager *man);

	 void(*evict_flags) (struct ttm_buffer_object *bo,
				struct ttm_placement *placement);
	int (*move) (struct ttm_buffer_object *bo,
		     bool evict, bool interruptible,
		     bool no_wait_reserve, bool no_wait_gpu,
		     struct ttm_mem_reg *new_mem);

	int (*verify_access) (struct ttm_buffer_object *bo,
			      struct file *filp);


	bool (*sync_obj_signaled) (void *sync_obj, void *sync_arg);
	int (*sync_obj_wait) (void *sync_obj, void *sync_arg,
			      bool lazy, bool interruptible);
	int (*sync_obj_flush) (void *sync_obj, void *sync_arg);
	void (*sync_obj_unref) (void **sync_obj);
	void *(*sync_obj_ref) (void *sync_obj);

	void (*move_notify)(struct ttm_buffer_object *bo,
			    struct ttm_mem_reg *new_mem);
	int (*fault_reserve_notify)(struct ttm_buffer_object *bo);

	void (*swap_notify) (struct ttm_buffer_object *bo);

	int (*io_mem_reserve)(struct ttm_bo_device *bdev, struct ttm_mem_reg *mem);
	void (*io_mem_free)(struct ttm_bo_device *bdev, struct ttm_mem_reg *mem);
};


struct ttm_bo_global_ref {
	struct drm_global_reference ref;
	struct ttm_mem_global *mem_glob;
};


struct ttm_bo_global {


	struct kobject kobj;
	struct ttm_mem_global *mem_glob;
	struct page *dummy_read_page;
	struct ttm_mem_shrink shrink;
	struct mutex device_list_mutex;
	spinlock_t lru_lock;

	struct list_head device_list;

	struct list_head swap_lru;

	atomic_t bo_count;
};


#define TTM_NUM_MEM_TYPES 8

#define TTM_BO_PRIV_FLAG_MOVING  0	
#define TTM_BO_PRIV_FLAG_MAX 1

struct ttm_bo_device {

	struct list_head device_list;
	struct ttm_bo_global *glob;
	struct ttm_bo_driver *driver;
	rwlock_t vm_lock;
	struct ttm_mem_type_manager man[TTM_NUM_MEM_TYPES];
	spinlock_t fence_lock;
	struct rb_root addr_space_rb;
	struct drm_mm addr_space_mm;

	struct list_head ddestroy;
	uint32_t val_seq;


	bool nice_mode;
	struct address_space *dev_mapping;


	struct delayed_work wq;

	bool need_dma32;
};


static inline uint32_t
ttm_flag_masked(uint32_t *old, uint32_t new, uint32_t mask)
{
	*old ^= (*old ^ new) & mask;
	return *old;
}

extern int ttm_tt_init(struct ttm_tt *ttm, struct ttm_bo_device *bdev,
			unsigned long size, uint32_t page_flags,
			struct page *dummy_read_page);
extern int ttm_dma_tt_init(struct ttm_dma_tt *ttm_dma, struct ttm_bo_device *bdev,
			   unsigned long size, uint32_t page_flags,
			   struct page *dummy_read_page);

extern void ttm_tt_fini(struct ttm_tt *ttm);
extern void ttm_dma_tt_fini(struct ttm_dma_tt *ttm_dma);

extern int ttm_tt_bind(struct ttm_tt *ttm, struct ttm_mem_reg *bo_mem);

extern void ttm_tt_destroy(struct ttm_tt *ttm);

extern void ttm_tt_unbind(struct ttm_tt *ttm);

extern int ttm_tt_swapin(struct ttm_tt *ttm);

extern void ttm_tt_cache_flush(struct page *pages[], unsigned long num_pages);

extern int ttm_tt_set_placement_caching(struct ttm_tt *ttm, uint32_t placement);
extern int ttm_tt_swapout(struct ttm_tt *ttm,
			  struct file *persistent_swap_storage);


extern bool ttm_mem_reg_is_pci(struct ttm_bo_device *bdev,
				   struct ttm_mem_reg *mem);

extern int ttm_bo_mem_space(struct ttm_buffer_object *bo,
				struct ttm_placement *placement,
				struct ttm_mem_reg *mem,
				bool interruptible,
				bool no_wait_reserve, bool no_wait_gpu);

extern void ttm_bo_mem_put(struct ttm_buffer_object *bo,
			   struct ttm_mem_reg *mem);
extern void ttm_bo_mem_put_locked(struct ttm_buffer_object *bo,
				  struct ttm_mem_reg *mem);


extern int ttm_bo_wait_cpu(struct ttm_buffer_object *bo, bool no_wait);

extern void ttm_bo_global_release(struct drm_global_reference *ref);
extern int ttm_bo_global_init(struct drm_global_reference *ref);

extern int ttm_bo_device_release(struct ttm_bo_device *bdev);

extern int ttm_bo_device_init(struct ttm_bo_device *bdev,
			      struct ttm_bo_global *glob,
			      struct ttm_bo_driver *driver,
			      uint64_t file_page_offset, bool need_dma32);

extern void ttm_bo_unmap_virtual(struct ttm_buffer_object *bo);

extern void ttm_bo_unmap_virtual_locked(struct ttm_buffer_object *bo);

extern int ttm_mem_io_reserve_vm(struct ttm_buffer_object *bo);
extern void ttm_mem_io_free_vm(struct ttm_buffer_object *bo);
extern int ttm_mem_io_lock(struct ttm_mem_type_manager *man,
			   bool interruptible);
extern void ttm_mem_io_unlock(struct ttm_mem_type_manager *man);


extern int ttm_bo_reserve(struct ttm_buffer_object *bo,
			  bool interruptible,
			  bool no_wait, bool use_sequence, uint32_t sequence);


extern int ttm_bo_reserve_locked(struct ttm_buffer_object *bo,
				 bool interruptible,
				 bool no_wait, bool use_sequence,
				 uint32_t sequence);

extern void ttm_bo_unreserve(struct ttm_buffer_object *bo);

extern void ttm_bo_unreserve_locked(struct ttm_buffer_object *bo);

extern int ttm_bo_wait_unreserved(struct ttm_buffer_object *bo,
				  bool interruptible);



extern int ttm_bo_move_ttm(struct ttm_buffer_object *bo,
			   bool evict, bool no_wait_reserve,
			   bool no_wait_gpu, struct ttm_mem_reg *new_mem);


extern int ttm_bo_move_memcpy(struct ttm_buffer_object *bo,
			      bool evict, bool no_wait_reserve,
			      bool no_wait_gpu, struct ttm_mem_reg *new_mem);

extern void ttm_bo_free_old_node(struct ttm_buffer_object *bo);


extern int ttm_bo_move_accel_cleanup(struct ttm_buffer_object *bo,
				     void *sync_obj,
				     void *sync_obj_arg,
				     bool evict, bool no_wait_reserve,
				     bool no_wait_gpu,
				     struct ttm_mem_reg *new_mem);
extern pgprot_t ttm_io_prot(uint32_t caching_flags, pgprot_t tmp);

extern const struct ttm_mem_type_manager_func ttm_bo_manager_func;

#if (defined(CONFIG_AGP) || (defined(CONFIG_AGP_MODULE) && defined(MODULE)))
#define TTM_HAS_AGP
#include <linux/agp_backend.h>

extern struct ttm_tt *ttm_agp_tt_create(struct ttm_bo_device *bdev,
					struct agp_bridge_data *bridge,
					unsigned long size, uint32_t page_flags,
					struct page *dummy_read_page);
int ttm_agp_tt_populate(struct ttm_tt *ttm);
void ttm_agp_tt_unpopulate(struct ttm_tt *ttm);
#endif

#endif
