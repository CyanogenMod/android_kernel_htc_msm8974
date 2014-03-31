/*
 * Copyright (c) Red Hat Inc.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie <airlied@redhat.com>
 *          Jerome Glisse <jglisse@redhat.com>
 *          Pauli Nieminen <suokkos@gmail.com>
 */


#define pr_fmt(fmt) "[TTM] " fmt

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/highmem.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/seq_file.h> 
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <linux/atomic.h>

#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_page_alloc.h"

#ifdef TTM_HAS_AGP
#include <asm/agp.h>
#endif

#define NUM_PAGES_TO_ALLOC		(PAGE_SIZE/sizeof(struct page *))
#define SMALL_ALLOCATION		16
#define FREE_ALL_PAGES			(~0U)
#define PAGE_FREE_INTERVAL		1000

struct ttm_page_pool {
	spinlock_t		lock;
	bool			fill_lock;
	struct list_head	list;
	gfp_t			gfp_flags;
	unsigned		npages;
	char			*name;
	unsigned long		nfrees;
	unsigned long		nrefills;
};


struct ttm_pool_opts {
	unsigned	alloc_size;
	unsigned	max_size;
	unsigned	small;
};

#define NUM_POOLS 4

struct ttm_pool_manager {
	struct kobject		kobj;
	struct shrinker		mm_shrink;
	struct ttm_pool_opts	options;

	union {
		struct ttm_page_pool	pools[NUM_POOLS];
		struct {
			struct ttm_page_pool	wc_pool;
			struct ttm_page_pool	uc_pool;
			struct ttm_page_pool	wc_pool_dma32;
			struct ttm_page_pool	uc_pool_dma32;
		} ;
	};
};

static struct attribute ttm_page_pool_max = {
	.name = "pool_max_size",
	.mode = S_IRUGO | S_IWUSR
};
static struct attribute ttm_page_pool_small = {
	.name = "pool_small_allocation",
	.mode = S_IRUGO | S_IWUSR
};
static struct attribute ttm_page_pool_alloc_size = {
	.name = "pool_allocation_size",
	.mode = S_IRUGO | S_IWUSR
};

static struct attribute *ttm_pool_attrs[] = {
	&ttm_page_pool_max,
	&ttm_page_pool_small,
	&ttm_page_pool_alloc_size,
	NULL
};

static void ttm_pool_kobj_release(struct kobject *kobj)
{
	struct ttm_pool_manager *m =
		container_of(kobj, struct ttm_pool_manager, kobj);
	kfree(m);
}

static ssize_t ttm_pool_store(struct kobject *kobj,
		struct attribute *attr, const char *buffer, size_t size)
{
	struct ttm_pool_manager *m =
		container_of(kobj, struct ttm_pool_manager, kobj);
	int chars;
	unsigned val;
	chars = sscanf(buffer, "%u", &val);
	if (chars == 0)
		return size;

	
	val = val / (PAGE_SIZE >> 10);

	if (attr == &ttm_page_pool_max)
		m->options.max_size = val;
	else if (attr == &ttm_page_pool_small)
		m->options.small = val;
	else if (attr == &ttm_page_pool_alloc_size) {
		if (val > NUM_PAGES_TO_ALLOC*8) {
			pr_err("Setting allocation size to %lu is not allowed. Recommended size is %lu\n",
			       NUM_PAGES_TO_ALLOC*(PAGE_SIZE >> 7),
			       NUM_PAGES_TO_ALLOC*(PAGE_SIZE >> 10));
			return size;
		} else if (val > NUM_PAGES_TO_ALLOC) {
			pr_warn("Setting allocation size to larger than %lu is not recommended\n",
				NUM_PAGES_TO_ALLOC*(PAGE_SIZE >> 10));
		}
		m->options.alloc_size = val;
	}

	return size;
}

static ssize_t ttm_pool_show(struct kobject *kobj,
		struct attribute *attr, char *buffer)
{
	struct ttm_pool_manager *m =
		container_of(kobj, struct ttm_pool_manager, kobj);
	unsigned val = 0;

	if (attr == &ttm_page_pool_max)
		val = m->options.max_size;
	else if (attr == &ttm_page_pool_small)
		val = m->options.small;
	else if (attr == &ttm_page_pool_alloc_size)
		val = m->options.alloc_size;

	val = val * (PAGE_SIZE >> 10);

	return snprintf(buffer, PAGE_SIZE, "%u\n", val);
}

static const struct sysfs_ops ttm_pool_sysfs_ops = {
	.show = &ttm_pool_show,
	.store = &ttm_pool_store,
};

static struct kobj_type ttm_pool_kobj_type = {
	.release = &ttm_pool_kobj_release,
	.sysfs_ops = &ttm_pool_sysfs_ops,
	.default_attrs = ttm_pool_attrs,
};

static struct ttm_pool_manager *_manager;

#ifndef CONFIG_X86
static int set_pages_array_wb(struct page **pages, int addrinarray)
{
#ifdef TTM_HAS_AGP
	int i;

	for (i = 0; i < addrinarray; i++)
		unmap_page_from_agp(pages[i]);
#endif
	return 0;
}

static int set_pages_array_wc(struct page **pages, int addrinarray)
{
#ifdef TTM_HAS_AGP
	int i;

	for (i = 0; i < addrinarray; i++)
		map_page_into_agp(pages[i]);
#endif
	return 0;
}

static int set_pages_array_uc(struct page **pages, int addrinarray)
{
#ifdef TTM_HAS_AGP
	int i;

	for (i = 0; i < addrinarray; i++)
		map_page_into_agp(pages[i]);
#endif
	return 0;
}
#endif

static struct ttm_page_pool *ttm_get_pool(int flags,
		enum ttm_caching_state cstate)
{
	int pool_index;

	if (cstate == tt_cached)
		return NULL;

	if (cstate == tt_wc)
		pool_index = 0x0;
	else
		pool_index = 0x1;

	if (flags & TTM_PAGE_FLAG_DMA32)
		pool_index |= 0x2;

	return &_manager->pools[pool_index];
}

static void ttm_pages_put(struct page *pages[], unsigned npages)
{
	unsigned i;
	if (set_pages_array_wb(pages, npages))
		pr_err("Failed to set %d pages to wb!\n", npages);
	for (i = 0; i < npages; ++i)
		__free_page(pages[i]);
}

static void ttm_pool_update_free_locked(struct ttm_page_pool *pool,
		unsigned freed_pages)
{
	pool->npages -= freed_pages;
	pool->nfrees += freed_pages;
}

static int ttm_page_pool_free(struct ttm_page_pool *pool, unsigned nr_free)
{
	unsigned long irq_flags;
	struct page *p;
	struct page **pages_to_free;
	unsigned freed_pages = 0,
		 npages_to_free = nr_free;

	if (NUM_PAGES_TO_ALLOC < nr_free)
		npages_to_free = NUM_PAGES_TO_ALLOC;

	pages_to_free = kmalloc(npages_to_free * sizeof(struct page *),
			GFP_KERNEL);
	if (!pages_to_free) {
		pr_err("Failed to allocate memory for pool free operation\n");
		return 0;
	}

restart:
	spin_lock_irqsave(&pool->lock, irq_flags);

	list_for_each_entry_reverse(p, &pool->list, lru) {
		if (freed_pages >= npages_to_free)
			break;

		pages_to_free[freed_pages++] = p;
		
		if (freed_pages >= NUM_PAGES_TO_ALLOC) {
			
			__list_del(p->lru.prev, &pool->list);

			ttm_pool_update_free_locked(pool, freed_pages);
			spin_unlock_irqrestore(&pool->lock, irq_flags);

			ttm_pages_put(pages_to_free, freed_pages);
			if (likely(nr_free != FREE_ALL_PAGES))
				nr_free -= freed_pages;

			if (NUM_PAGES_TO_ALLOC >= nr_free)
				npages_to_free = nr_free;
			else
				npages_to_free = NUM_PAGES_TO_ALLOC;

			freed_pages = 0;

			
			if (nr_free)
				goto restart;

			goto out;

		}
	}

	
	if (freed_pages) {
		__list_del(&p->lru, &pool->list);

		ttm_pool_update_free_locked(pool, freed_pages);
		nr_free -= freed_pages;
	}

	spin_unlock_irqrestore(&pool->lock, irq_flags);

	if (freed_pages)
		ttm_pages_put(pages_to_free, freed_pages);
out:
	kfree(pages_to_free);
	return nr_free;
}

static int ttm_pool_get_num_unused_pages(void)
{
	unsigned i;
	int total = 0;
	for (i = 0; i < NUM_POOLS; ++i)
		total += _manager->pools[i].npages;

	return total;
}

static int ttm_pool_mm_shrink(struct shrinker *shrink,
			      struct shrink_control *sc)
{
	static atomic_t start_pool = ATOMIC_INIT(0);
	unsigned i;
	unsigned pool_offset = atomic_add_return(1, &start_pool);
	struct ttm_page_pool *pool;
	int shrink_pages = sc->nr_to_scan;

	pool_offset = pool_offset % NUM_POOLS;
	
	for (i = 0; i < NUM_POOLS; ++i) {
		unsigned nr_free = shrink_pages;
		if (shrink_pages == 0)
			break;
		pool = &_manager->pools[(i + pool_offset)%NUM_POOLS];
		shrink_pages = ttm_page_pool_free(pool, nr_free);
	}
	
	return ttm_pool_get_num_unused_pages();
}

static void ttm_pool_mm_shrink_init(struct ttm_pool_manager *manager)
{
	manager->mm_shrink.shrink = &ttm_pool_mm_shrink;
	manager->mm_shrink.seeks = 1;
	register_shrinker(&manager->mm_shrink);
}

static void ttm_pool_mm_shrink_fini(struct ttm_pool_manager *manager)
{
	unregister_shrinker(&manager->mm_shrink);
}

static int ttm_set_pages_caching(struct page **pages,
		enum ttm_caching_state cstate, unsigned cpages)
{
	int r = 0;
	
	switch (cstate) {
	case tt_uncached:
		r = set_pages_array_uc(pages, cpages);
		if (r)
			pr_err("Failed to set %d pages to uc!\n", cpages);
		break;
	case tt_wc:
		r = set_pages_array_wc(pages, cpages);
		if (r)
			pr_err("Failed to set %d pages to wc!\n", cpages);
		break;
	default:
		break;
	}
	return r;
}

static void ttm_handle_caching_state_failure(struct list_head *pages,
		int ttm_flags, enum ttm_caching_state cstate,
		struct page **failed_pages, unsigned cpages)
{
	unsigned i;
	
	for (i = 0; i < cpages; ++i) {
		list_del(&failed_pages[i]->lru);
		__free_page(failed_pages[i]);
	}
}

static int ttm_alloc_new_pages(struct list_head *pages, gfp_t gfp_flags,
		int ttm_flags, enum ttm_caching_state cstate, unsigned count)
{
	struct page **caching_array;
	struct page *p;
	int r = 0;
	unsigned i, cpages;
	unsigned max_cpages = min(count,
			(unsigned)(PAGE_SIZE/sizeof(struct page *)));

	
	caching_array = kmalloc(max_cpages*sizeof(struct page *), GFP_KERNEL);

	if (!caching_array) {
		pr_err("Unable to allocate table for new pages\n");
		return -ENOMEM;
	}

	for (i = 0, cpages = 0; i < count; ++i) {
		p = alloc_page(gfp_flags);

		if (!p) {
			pr_err("Unable to get page %u\n", i);

			if (cpages) {
				r = ttm_set_pages_caching(caching_array,
							  cstate, cpages);
				if (r)
					ttm_handle_caching_state_failure(pages,
						ttm_flags, cstate,
						caching_array, cpages);
			}
			r = -ENOMEM;
			goto out;
		}

#ifdef CONFIG_HIGHMEM
		if (!PageHighMem(p))
#endif
		{
			caching_array[cpages++] = p;
			if (cpages == max_cpages) {

				r = ttm_set_pages_caching(caching_array,
						cstate, cpages);
				if (r) {
					ttm_handle_caching_state_failure(pages,
						ttm_flags, cstate,
						caching_array, cpages);
					goto out;
				}
				cpages = 0;
			}
		}

		list_add(&p->lru, pages);
	}

	if (cpages) {
		r = ttm_set_pages_caching(caching_array, cstate, cpages);
		if (r)
			ttm_handle_caching_state_failure(pages,
					ttm_flags, cstate,
					caching_array, cpages);
	}
out:
	kfree(caching_array);

	return r;
}

static void ttm_page_pool_fill_locked(struct ttm_page_pool *pool,
		int ttm_flags, enum ttm_caching_state cstate, unsigned count,
		unsigned long *irq_flags)
{
	struct page *p;
	int r;
	unsigned cpages = 0;
	if (pool->fill_lock)
		return;

	pool->fill_lock = true;

	if (count < _manager->options.small
		&& count > pool->npages) {
		struct list_head new_pages;
		unsigned alloc_size = _manager->options.alloc_size;

		spin_unlock_irqrestore(&pool->lock, *irq_flags);

		INIT_LIST_HEAD(&new_pages);
		r = ttm_alloc_new_pages(&new_pages, pool->gfp_flags, ttm_flags,
				cstate,	alloc_size);
		spin_lock_irqsave(&pool->lock, *irq_flags);

		if (!r) {
			list_splice(&new_pages, &pool->list);
			++pool->nrefills;
			pool->npages += alloc_size;
		} else {
			pr_err("Failed to fill pool (%p)\n", pool);
			
			list_for_each_entry(p, &pool->list, lru) {
				++cpages;
			}
			list_splice(&new_pages, &pool->list);
			pool->npages += cpages;
		}

	}
	pool->fill_lock = false;
}

static unsigned ttm_page_pool_get_pages(struct ttm_page_pool *pool,
					struct list_head *pages,
					int ttm_flags,
					enum ttm_caching_state cstate,
					unsigned count)
{
	unsigned long irq_flags;
	struct list_head *p;
	unsigned i;

	spin_lock_irqsave(&pool->lock, irq_flags);
	ttm_page_pool_fill_locked(pool, ttm_flags, cstate, count, &irq_flags);

	if (count >= pool->npages) {
		
		list_splice_init(&pool->list, pages);
		count -= pool->npages;
		pool->npages = 0;
		goto out;
	}
	if (count <= pool->npages/2) {
		i = 0;
		list_for_each(p, &pool->list) {
			if (++i == count)
				break;
		}
	} else {
		i = pool->npages + 1;
		list_for_each_prev(p, &pool->list) {
			if (--i == count)
				break;
		}
	}
	
	list_cut_position(pages, &pool->list, p);
	pool->npages -= count;
	count = 0;
out:
	spin_unlock_irqrestore(&pool->lock, irq_flags);
	return count;
}

static void ttm_put_pages(struct page **pages, unsigned npages, int flags,
			  enum ttm_caching_state cstate)
{
	unsigned long irq_flags;
	struct ttm_page_pool *pool = ttm_get_pool(flags, cstate);
	unsigned i;

	if (pool == NULL) {
		
		for (i = 0; i < npages; i++) {
			if (pages[i]) {
				if (page_count(pages[i]) != 1)
					pr_err("Erroneous page count. Leaking pages.\n");
				__free_page(pages[i]);
				pages[i] = NULL;
			}
		}
		return;
	}

	spin_lock_irqsave(&pool->lock, irq_flags);
	for (i = 0; i < npages; i++) {
		if (pages[i]) {
			if (page_count(pages[i]) != 1)
				pr_err("Erroneous page count. Leaking pages.\n");
			list_add_tail(&pages[i]->lru, &pool->list);
			pages[i] = NULL;
			pool->npages++;
		}
	}
	
	npages = 0;
	if (pool->npages > _manager->options.max_size) {
		npages = pool->npages - _manager->options.max_size;
		if (npages < NUM_PAGES_TO_ALLOC)
			npages = NUM_PAGES_TO_ALLOC;
	}
	spin_unlock_irqrestore(&pool->lock, irq_flags);
	if (npages)
		ttm_page_pool_free(pool, npages);
}

static int ttm_get_pages(struct page **pages, unsigned npages, int flags,
			 enum ttm_caching_state cstate)
{
	struct ttm_page_pool *pool = ttm_get_pool(flags, cstate);
	struct list_head plist;
	struct page *p = NULL;
	gfp_t gfp_flags = GFP_USER;
	unsigned count;
	int r;

	
	if (flags & TTM_PAGE_FLAG_ZERO_ALLOC)
		gfp_flags |= __GFP_ZERO;

	
	if (pool == NULL) {
		if (flags & TTM_PAGE_FLAG_DMA32)
			gfp_flags |= GFP_DMA32;
		else
			gfp_flags |= GFP_HIGHUSER;

		for (r = 0; r < npages; ++r) {
			p = alloc_page(gfp_flags);
			if (!p) {

				pr_err("Unable to allocate page\n");
				return -ENOMEM;
			}

			pages[r] = p;
		}
		return 0;
	}

	
	gfp_flags |= pool->gfp_flags;

	
	INIT_LIST_HEAD(&plist);
	npages = ttm_page_pool_get_pages(pool, &plist, flags, cstate, npages);
	count = 0;
	list_for_each_entry(p, &plist, lru) {
		pages[count++] = p;
	}

	
	if (flags & TTM_PAGE_FLAG_ZERO_ALLOC) {
		list_for_each_entry(p, &plist, lru) {
			clear_page(page_address(p));
		}
	}

	
	if (npages > 0) {
		INIT_LIST_HEAD(&plist);
		r = ttm_alloc_new_pages(&plist, gfp_flags, flags, cstate, npages);
		list_for_each_entry(p, &plist, lru) {
			pages[count++] = p;
		}
		if (r) {
			pr_err("Failed to allocate extra pages for large request\n");
			ttm_put_pages(pages, count, flags, cstate);
			return r;
		}
	}

	return 0;
}

static void ttm_page_pool_init_locked(struct ttm_page_pool *pool, int flags,
		char *name)
{
	spin_lock_init(&pool->lock);
	pool->fill_lock = false;
	INIT_LIST_HEAD(&pool->list);
	pool->npages = pool->nfrees = 0;
	pool->gfp_flags = flags;
	pool->name = name;
}

int ttm_page_alloc_init(struct ttm_mem_global *glob, unsigned max_pages)
{
	int ret;

	WARN_ON(_manager);

	pr_info("Initializing pool allocator\n");

	_manager = kzalloc(sizeof(*_manager), GFP_KERNEL);

	ttm_page_pool_init_locked(&_manager->wc_pool, GFP_HIGHUSER, "wc");

	ttm_page_pool_init_locked(&_manager->uc_pool, GFP_HIGHUSER, "uc");

	ttm_page_pool_init_locked(&_manager->wc_pool_dma32,
				  GFP_USER | GFP_DMA32, "wc dma");

	ttm_page_pool_init_locked(&_manager->uc_pool_dma32,
				  GFP_USER | GFP_DMA32, "uc dma");

	_manager->options.max_size = max_pages;
	_manager->options.small = SMALL_ALLOCATION;
	_manager->options.alloc_size = NUM_PAGES_TO_ALLOC;

	ret = kobject_init_and_add(&_manager->kobj, &ttm_pool_kobj_type,
				   &glob->kobj, "pool");
	if (unlikely(ret != 0)) {
		kobject_put(&_manager->kobj);
		_manager = NULL;
		return ret;
	}

	ttm_pool_mm_shrink_init(_manager);

	return 0;
}

void ttm_page_alloc_fini(void)
{
	int i;

	pr_info("Finalizing pool allocator\n");
	ttm_pool_mm_shrink_fini(_manager);

	for (i = 0; i < NUM_POOLS; ++i)
		ttm_page_pool_free(&_manager->pools[i], FREE_ALL_PAGES);

	kobject_put(&_manager->kobj);
	_manager = NULL;
}

int ttm_pool_populate(struct ttm_tt *ttm)
{
	struct ttm_mem_global *mem_glob = ttm->glob->mem_glob;
	unsigned i;
	int ret;

	if (ttm->state != tt_unpopulated)
		return 0;

	for (i = 0; i < ttm->num_pages; ++i) {
		ret = ttm_get_pages(&ttm->pages[i], 1,
				    ttm->page_flags,
				    ttm->caching_state);
		if (ret != 0) {
			ttm_pool_unpopulate(ttm);
			return -ENOMEM;
		}

		ret = ttm_mem_global_alloc_page(mem_glob, ttm->pages[i],
						false, false);
		if (unlikely(ret != 0)) {
			ttm_pool_unpopulate(ttm);
			return -ENOMEM;
		}
	}

	if (unlikely(ttm->page_flags & TTM_PAGE_FLAG_SWAPPED)) {
		ret = ttm_tt_swapin(ttm);
		if (unlikely(ret != 0)) {
			ttm_pool_unpopulate(ttm);
			return ret;
		}
	}

	ttm->state = tt_unbound;
	return 0;
}
EXPORT_SYMBOL(ttm_pool_populate);

void ttm_pool_unpopulate(struct ttm_tt *ttm)
{
	unsigned i;

	for (i = 0; i < ttm->num_pages; ++i) {
		if (ttm->pages[i]) {
			ttm_mem_global_free_page(ttm->glob->mem_glob,
						 ttm->pages[i]);
			ttm_put_pages(&ttm->pages[i], 1,
				      ttm->page_flags,
				      ttm->caching_state);
		}
	}
	ttm->state = tt_unpopulated;
}
EXPORT_SYMBOL(ttm_pool_unpopulate);

int ttm_page_alloc_debugfs(struct seq_file *m, void *data)
{
	struct ttm_page_pool *p;
	unsigned i;
	char *h[] = {"pool", "refills", "pages freed", "size"};
	if (!_manager) {
		seq_printf(m, "No pool allocator running.\n");
		return 0;
	}
	seq_printf(m, "%6s %12s %13s %8s\n",
			h[0], h[1], h[2], h[3]);
	for (i = 0; i < NUM_POOLS; ++i) {
		p = &_manager->pools[i];

		seq_printf(m, "%6s %12ld %13ld %8d\n",
				p->name, p->nrefills,
				p->nfrees, p->npages);
	}
	return 0;
}
EXPORT_SYMBOL(ttm_page_alloc_debugfs);
