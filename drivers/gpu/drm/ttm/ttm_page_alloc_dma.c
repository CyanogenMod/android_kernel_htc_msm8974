/*
 * Copyright 2011 (c) Oracle Corp.

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
 * Author: Konrad Rzeszutek Wilk <konrad.wilk@oracle.com>
 */


#define pr_fmt(fmt) "[TTM] " fmt

#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/seq_file.h> 
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/highmem.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_page_alloc.h"
#ifdef TTM_HAS_AGP
#include <asm/agp.h>
#endif

#define NUM_PAGES_TO_ALLOC		(PAGE_SIZE/sizeof(struct page *))
#define SMALL_ALLOCATION		4
#define FREE_ALL_PAGES			(~0U)
#define IS_UNDEFINED			(0)
#define IS_WC				(1<<1)
#define IS_UC				(1<<2)
#define IS_CACHED			(1<<3)
#define IS_DMA32			(1<<4)

enum pool_type {
	POOL_IS_UNDEFINED,
	POOL_IS_WC = IS_WC,
	POOL_IS_UC = IS_UC,
	POOL_IS_CACHED = IS_CACHED,
	POOL_IS_WC_DMA32 = IS_WC | IS_DMA32,
	POOL_IS_UC_DMA32 = IS_UC | IS_DMA32,
	POOL_IS_CACHED_DMA32 = IS_CACHED | IS_DMA32,
};
struct dma_pool {
	struct list_head pools; 
	enum pool_type type;
	spinlock_t lock;
	struct list_head inuse_list;
	struct list_head free_list;
	struct device *dev;
	unsigned size;
	unsigned npages_free;
	unsigned npages_in_use;
	unsigned long nfrees; 
	unsigned long nrefills; 
	gfp_t gfp_flags;
	char name[13]; 
	char dev_name[64]; 
};

struct dma_page {
	struct list_head page_list;
	void *vaddr;
	struct page *p;
	dma_addr_t dma;
};


struct ttm_pool_opts {
	unsigned	alloc_size;
	unsigned	max_size;
	unsigned	small;
};

struct device_pools {
	struct list_head pools;
	struct device *dev;
	struct dma_pool *pool;
};

struct ttm_pool_manager {
	struct mutex		lock;
	struct list_head	pools;
	struct ttm_pool_opts	options;
	unsigned		npools;
	struct shrinker		mm_shrink;
	struct kobject		kobj;
};

static struct ttm_pool_manager *_manager;

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

static ssize_t ttm_pool_store(struct kobject *kobj, struct attribute *attr,
			      const char *buffer, size_t size)
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

static ssize_t ttm_pool_show(struct kobject *kobj, struct attribute *attr,
			     char *buffer)
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

static int ttm_set_pages_caching(struct dma_pool *pool,
				 struct page **pages, unsigned cpages)
{
	int r = 0;
	
	if (pool->type & IS_UC) {
		r = set_pages_array_uc(pages, cpages);
		if (r)
			pr_err("%s: Failed to set %d pages to uc!\n",
			       pool->dev_name, cpages);
	}
	if (pool->type & IS_WC) {
		r = set_pages_array_wc(pages, cpages);
		if (r)
			pr_err("%s: Failed to set %d pages to wc!\n",
			       pool->dev_name, cpages);
	}
	return r;
}

static void __ttm_dma_free_page(struct dma_pool *pool, struct dma_page *d_page)
{
	dma_addr_t dma = d_page->dma;
	dma_free_coherent(pool->dev, pool->size, d_page->vaddr, dma);

	kfree(d_page);
	d_page = NULL;
}
static struct dma_page *__ttm_dma_alloc_page(struct dma_pool *pool)
{
	struct dma_page *d_page;

	d_page = kmalloc(sizeof(struct dma_page), GFP_KERNEL);
	if (!d_page)
		return NULL;

	d_page->vaddr = dma_alloc_coherent(pool->dev, pool->size,
					   &d_page->dma,
					   pool->gfp_flags);
	if (d_page->vaddr)
		d_page->p = virt_to_page(d_page->vaddr);
	else {
		kfree(d_page);
		d_page = NULL;
	}
	return d_page;
}
static enum pool_type ttm_to_type(int flags, enum ttm_caching_state cstate)
{
	enum pool_type type = IS_UNDEFINED;

	if (flags & TTM_PAGE_FLAG_DMA32)
		type |= IS_DMA32;
	if (cstate == tt_cached)
		type |= IS_CACHED;
	else if (cstate == tt_uncached)
		type |= IS_UC;
	else
		type |= IS_WC;

	return type;
}

static void ttm_pool_update_free_locked(struct dma_pool *pool,
					unsigned freed_pages)
{
	pool->npages_free -= freed_pages;
	pool->nfrees += freed_pages;

}

static void ttm_dma_pages_put(struct dma_pool *pool, struct list_head *d_pages,
			      struct page *pages[], unsigned npages)
{
	struct dma_page *d_page, *tmp;

	
	if (npages && !(pool->type & IS_CACHED) &&
	    set_pages_array_wb(pages, npages))
		pr_err("%s: Failed to set %d pages to wb!\n",
		       pool->dev_name, npages);

	list_for_each_entry_safe(d_page, tmp, d_pages, page_list) {
		list_del(&d_page->page_list);
		__ttm_dma_free_page(pool, d_page);
	}
}

static void ttm_dma_page_put(struct dma_pool *pool, struct dma_page *d_page)
{
	
	if (!(pool->type & IS_CACHED) && set_pages_array_wb(&d_page->p, 1))
		pr_err("%s: Failed to set %d pages to wb!\n",
		       pool->dev_name, 1);

	list_del(&d_page->page_list);
	__ttm_dma_free_page(pool, d_page);
}

static unsigned ttm_dma_page_pool_free(struct dma_pool *pool, unsigned nr_free)
{
	unsigned long irq_flags;
	struct dma_page *dma_p, *tmp;
	struct page **pages_to_free;
	struct list_head d_pages;
	unsigned freed_pages = 0,
		 npages_to_free = nr_free;

	if (NUM_PAGES_TO_ALLOC < nr_free)
		npages_to_free = NUM_PAGES_TO_ALLOC;
#if 0
	if (nr_free > 1) {
		pr_debug("%s: (%s:%d) Attempting to free %d (%d) pages\n",
			 pool->dev_name, pool->name, current->pid,
			 npages_to_free, nr_free);
	}
#endif
	pages_to_free = kmalloc(npages_to_free * sizeof(struct page *),
			GFP_KERNEL);

	if (!pages_to_free) {
		pr_err("%s: Failed to allocate memory for pool free operation\n",
		       pool->dev_name);
		return 0;
	}
	INIT_LIST_HEAD(&d_pages);
restart:
	spin_lock_irqsave(&pool->lock, irq_flags);

	
	list_for_each_entry_safe_reverse(dma_p, tmp, &pool->free_list,
					 page_list) {
		if (freed_pages >= npages_to_free)
			break;

		
		list_move(&dma_p->page_list, &d_pages);

		pages_to_free[freed_pages++] = dma_p->p;
		
		if (freed_pages >= NUM_PAGES_TO_ALLOC) {

			ttm_pool_update_free_locked(pool, freed_pages);
			spin_unlock_irqrestore(&pool->lock, irq_flags);

			ttm_dma_pages_put(pool, &d_pages, pages_to_free,
					  freed_pages);

			INIT_LIST_HEAD(&d_pages);

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
		ttm_pool_update_free_locked(pool, freed_pages);
		nr_free -= freed_pages;
	}

	spin_unlock_irqrestore(&pool->lock, irq_flags);

	if (freed_pages)
		ttm_dma_pages_put(pool, &d_pages, pages_to_free, freed_pages);
out:
	kfree(pages_to_free);
	return nr_free;
}

static void ttm_dma_free_pool(struct device *dev, enum pool_type type)
{
	struct device_pools *p;
	struct dma_pool *pool;

	if (!dev)
		return;

	mutex_lock(&_manager->lock);
	list_for_each_entry_reverse(p, &_manager->pools, pools) {
		if (p->dev != dev)
			continue;
		pool = p->pool;
		if (pool->type != type)
			continue;

		list_del(&p->pools);
		kfree(p);
		_manager->npools--;
		break;
	}
	list_for_each_entry_reverse(pool, &dev->dma_pools, pools) {
		if (pool->type != type)
			continue;
		
		ttm_dma_page_pool_free(pool, FREE_ALL_PAGES);
		WARN_ON(((pool->npages_in_use + pool->npages_free) != 0));
		list_del(&pool->pools);
		kfree(pool);
		break;
	}
	mutex_unlock(&_manager->lock);
}

static void ttm_dma_pool_release(struct device *dev, void *res)
{
	struct dma_pool *pool = *(struct dma_pool **)res;

	if (pool)
		ttm_dma_free_pool(dev, pool->type);
}

static int ttm_dma_pool_match(struct device *dev, void *res, void *match_data)
{
	return *(struct dma_pool **)res == match_data;
}

static struct dma_pool *ttm_dma_pool_init(struct device *dev, gfp_t flags,
					  enum pool_type type)
{
	char *n[] = {"wc", "uc", "cached", " dma32", "unknown",};
	enum pool_type t[] = {IS_WC, IS_UC, IS_CACHED, IS_DMA32, IS_UNDEFINED};
	struct device_pools *sec_pool = NULL;
	struct dma_pool *pool = NULL, **ptr;
	unsigned i;
	int ret = -ENODEV;
	char *p;

	if (!dev)
		return NULL;

	ptr = devres_alloc(ttm_dma_pool_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return NULL;

	ret = -ENOMEM;

	pool = kmalloc_node(sizeof(struct dma_pool), GFP_KERNEL,
			    dev_to_node(dev));
	if (!pool)
		goto err_mem;

	sec_pool = kmalloc_node(sizeof(struct device_pools), GFP_KERNEL,
				dev_to_node(dev));
	if (!sec_pool)
		goto err_mem;

	INIT_LIST_HEAD(&sec_pool->pools);
	sec_pool->dev = dev;
	sec_pool->pool =  pool;

	INIT_LIST_HEAD(&pool->free_list);
	INIT_LIST_HEAD(&pool->inuse_list);
	INIT_LIST_HEAD(&pool->pools);
	spin_lock_init(&pool->lock);
	pool->dev = dev;
	pool->npages_free = pool->npages_in_use = 0;
	pool->nfrees = 0;
	pool->gfp_flags = flags;
	pool->size = PAGE_SIZE;
	pool->type = type;
	pool->nrefills = 0;
	p = pool->name;
	for (i = 0; i < 5; i++) {
		if (type & t[i]) {
			p += snprintf(p, sizeof(pool->name) - (p - pool->name),
				      "%s", n[i]);
		}
	}
	*p = 0;
	snprintf(pool->dev_name, sizeof(pool->dev_name), "%s %s",
		 dev_driver_string(dev), dev_name(dev));
	mutex_lock(&_manager->lock);
	
	list_add(&sec_pool->pools, &_manager->pools);
	_manager->npools++;
	
	list_add(&pool->pools, &dev->dma_pools);
	mutex_unlock(&_manager->lock);

	*ptr = pool;
	devres_add(dev, ptr);

	return pool;
err_mem:
	devres_free(ptr);
	kfree(sec_pool);
	kfree(pool);
	return ERR_PTR(ret);
}

static struct dma_pool *ttm_dma_find_pool(struct device *dev,
					  enum pool_type type)
{
	struct dma_pool *pool, *tmp, *found = NULL;

	if (type == IS_UNDEFINED)
		return found;

	list_for_each_entry_safe(pool, tmp, &dev->dma_pools, pools) {
		if (pool->type != type)
			continue;
		found = pool;
		break;
	}
	return found;
}

static void ttm_dma_handle_caching_state_failure(struct dma_pool *pool,
						 struct list_head *d_pages,
						 struct page **failed_pages,
						 unsigned cpages)
{
	struct dma_page *d_page, *tmp;
	struct page *p;
	unsigned i = 0;

	p = failed_pages[0];
	if (!p)
		return;
	
	list_for_each_entry_safe(d_page, tmp, d_pages, page_list) {
		if (d_page->p != p)
			continue;
		
		list_del(&d_page->page_list);
		__ttm_dma_free_page(pool, d_page);
		if (++i < cpages)
			p = failed_pages[i];
		else
			break;
	}

}

static int ttm_dma_pool_alloc_new_pages(struct dma_pool *pool,
					struct list_head *d_pages,
					unsigned count)
{
	struct page **caching_array;
	struct dma_page *dma_p;
	struct page *p;
	int r = 0;
	unsigned i, cpages;
	unsigned max_cpages = min(count,
			(unsigned)(PAGE_SIZE/sizeof(struct page *)));

	
	caching_array = kmalloc(max_cpages*sizeof(struct page *), GFP_KERNEL);

	if (!caching_array) {
		pr_err("%s: Unable to allocate table for new pages\n",
		       pool->dev_name);
		return -ENOMEM;
	}

	if (count > 1) {
		pr_debug("%s: (%s:%d) Getting %d pages\n",
			 pool->dev_name, pool->name, current->pid, count);
	}

	for (i = 0, cpages = 0; i < count; ++i) {
		dma_p = __ttm_dma_alloc_page(pool);
		if (!dma_p) {
			pr_err("%s: Unable to get page %u\n",
			       pool->dev_name, i);

			if (cpages) {
				r = ttm_set_pages_caching(pool, caching_array,
							  cpages);
				if (r)
					ttm_dma_handle_caching_state_failure(
						pool, d_pages, caching_array,
						cpages);
			}
			r = -ENOMEM;
			goto out;
		}
		p = dma_p->p;
#ifdef CONFIG_HIGHMEM
		if (!PageHighMem(p))
#endif
		{
			caching_array[cpages++] = p;
			if (cpages == max_cpages) {
				
				r = ttm_set_pages_caching(pool, caching_array,
						 cpages);
				if (r) {
					ttm_dma_handle_caching_state_failure(
						pool, d_pages, caching_array,
						cpages);
					goto out;
				}
				cpages = 0;
			}
		}
		list_add(&dma_p->page_list, d_pages);
	}

	if (cpages) {
		r = ttm_set_pages_caching(pool, caching_array, cpages);
		if (r)
			ttm_dma_handle_caching_state_failure(pool, d_pages,
					caching_array, cpages);
	}
out:
	kfree(caching_array);
	return r;
}

static int ttm_dma_page_pool_fill_locked(struct dma_pool *pool,
					 unsigned long *irq_flags)
{
	unsigned count = _manager->options.small;
	int r = pool->npages_free;

	if (count > pool->npages_free) {
		struct list_head d_pages;

		INIT_LIST_HEAD(&d_pages);

		spin_unlock_irqrestore(&pool->lock, *irq_flags);

		r = ttm_dma_pool_alloc_new_pages(pool, &d_pages, count);

		spin_lock_irqsave(&pool->lock, *irq_flags);
		if (!r) {
			
			list_splice(&d_pages, &pool->free_list);
			++pool->nrefills;
			pool->npages_free += count;
			r = count;
		} else {
			struct dma_page *d_page;
			unsigned cpages = 0;

			pr_err("%s: Failed to fill %s pool (r:%d)!\n",
			       pool->dev_name, pool->name, r);

			list_for_each_entry(d_page, &d_pages, page_list) {
				cpages++;
			}
			list_splice_tail(&d_pages, &pool->free_list);
			pool->npages_free += cpages;
			r = cpages;
		}
	}
	return r;
}

static int ttm_dma_pool_get_pages(struct dma_pool *pool,
				  struct ttm_dma_tt *ttm_dma,
				  unsigned index)
{
	struct dma_page *d_page;
	struct ttm_tt *ttm = &ttm_dma->ttm;
	unsigned long irq_flags;
	int count, r = -ENOMEM;

	spin_lock_irqsave(&pool->lock, irq_flags);
	count = ttm_dma_page_pool_fill_locked(pool, &irq_flags);
	if (count) {
		d_page = list_first_entry(&pool->free_list, struct dma_page, page_list);
		ttm->pages[index] = d_page->p;
		ttm_dma->dma_address[index] = d_page->dma;
		list_move_tail(&d_page->page_list, &ttm_dma->pages_list);
		r = 0;
		pool->npages_in_use += 1;
		pool->npages_free -= 1;
	}
	spin_unlock_irqrestore(&pool->lock, irq_flags);
	return r;
}

int ttm_dma_populate(struct ttm_dma_tt *ttm_dma, struct device *dev)
{
	struct ttm_tt *ttm = &ttm_dma->ttm;
	struct ttm_mem_global *mem_glob = ttm->glob->mem_glob;
	struct dma_pool *pool;
	enum pool_type type;
	unsigned i;
	gfp_t gfp_flags;
	int ret;

	if (ttm->state != tt_unpopulated)
		return 0;

	type = ttm_to_type(ttm->page_flags, ttm->caching_state);
	if (ttm->page_flags & TTM_PAGE_FLAG_DMA32)
		gfp_flags = GFP_USER | GFP_DMA32;
	else
		gfp_flags = GFP_HIGHUSER;
	if (ttm->page_flags & TTM_PAGE_FLAG_ZERO_ALLOC)
		gfp_flags |= __GFP_ZERO;

	pool = ttm_dma_find_pool(dev, type);
	if (!pool) {
		pool = ttm_dma_pool_init(dev, gfp_flags, type);
		if (IS_ERR_OR_NULL(pool)) {
			return -ENOMEM;
		}
	}

	INIT_LIST_HEAD(&ttm_dma->pages_list);
	for (i = 0; i < ttm->num_pages; ++i) {
		ret = ttm_dma_pool_get_pages(pool, ttm_dma, i);
		if (ret != 0) {
			ttm_dma_unpopulate(ttm_dma, dev);
			return -ENOMEM;
		}

		ret = ttm_mem_global_alloc_page(mem_glob, ttm->pages[i],
						false, false);
		if (unlikely(ret != 0)) {
			ttm_dma_unpopulate(ttm_dma, dev);
			return -ENOMEM;
		}
	}

	if (unlikely(ttm->page_flags & TTM_PAGE_FLAG_SWAPPED)) {
		ret = ttm_tt_swapin(ttm);
		if (unlikely(ret != 0)) {
			ttm_dma_unpopulate(ttm_dma, dev);
			return ret;
		}
	}

	ttm->state = tt_unbound;
	return 0;
}
EXPORT_SYMBOL_GPL(ttm_dma_populate);

static int ttm_dma_pool_get_num_unused_pages(void)
{
	struct device_pools *p;
	unsigned total = 0;

	mutex_lock(&_manager->lock);
	list_for_each_entry(p, &_manager->pools, pools)
		total += p->pool->npages_free;
	mutex_unlock(&_manager->lock);
	return total;
}

void ttm_dma_unpopulate(struct ttm_dma_tt *ttm_dma, struct device *dev)
{
	struct ttm_tt *ttm = &ttm_dma->ttm;
	struct dma_pool *pool;
	struct dma_page *d_page, *next;
	enum pool_type type;
	bool is_cached = false;
	unsigned count = 0, i, npages = 0;
	unsigned long irq_flags;

	type = ttm_to_type(ttm->page_flags, ttm->caching_state);
	pool = ttm_dma_find_pool(dev, type);
	if (!pool)
		return;

	is_cached = (ttm_dma_find_pool(pool->dev,
		     ttm_to_type(ttm->page_flags, tt_cached)) == pool);

	
	list_for_each_entry(d_page, &ttm_dma->pages_list, page_list) {
		ttm->pages[count] = d_page->p;
		count++;
	}

	spin_lock_irqsave(&pool->lock, irq_flags);
	pool->npages_in_use -= count;
	if (is_cached) {
		pool->nfrees += count;
	} else {
		pool->npages_free += count;
		list_splice(&ttm_dma->pages_list, &pool->free_list);
		npages = count;
		if (pool->npages_free > _manager->options.max_size) {
			npages = pool->npages_free - _manager->options.max_size;
			if (npages < NUM_PAGES_TO_ALLOC)
				npages = NUM_PAGES_TO_ALLOC;
		}
	}
	spin_unlock_irqrestore(&pool->lock, irq_flags);

	if (is_cached) {
		list_for_each_entry_safe(d_page, next, &ttm_dma->pages_list, page_list) {
			ttm_mem_global_free_page(ttm->glob->mem_glob,
						 d_page->p);
			ttm_dma_page_put(pool, d_page);
		}
	} else {
		for (i = 0; i < count; i++) {
			ttm_mem_global_free_page(ttm->glob->mem_glob,
						 ttm->pages[i]);
		}
	}

	INIT_LIST_HEAD(&ttm_dma->pages_list);
	for (i = 0; i < ttm->num_pages; i++) {
		ttm->pages[i] = NULL;
		ttm_dma->dma_address[i] = 0;
	}

	
	if (npages)
		ttm_dma_page_pool_free(pool, npages);
	ttm->state = tt_unpopulated;
}
EXPORT_SYMBOL_GPL(ttm_dma_unpopulate);

static int ttm_dma_pool_mm_shrink(struct shrinker *shrink,
				  struct shrink_control *sc)
{
	static atomic_t start_pool = ATOMIC_INIT(0);
	unsigned idx = 0;
	unsigned pool_offset = atomic_add_return(1, &start_pool);
	unsigned shrink_pages = sc->nr_to_scan;
	struct device_pools *p;

	if (list_empty(&_manager->pools))
		return 0;

	mutex_lock(&_manager->lock);
	pool_offset = pool_offset % _manager->npools;
	list_for_each_entry(p, &_manager->pools, pools) {
		unsigned nr_free;

		if (!p->dev)
			continue;
		if (shrink_pages == 0)
			break;
		
		if (++idx < pool_offset)
			continue;
		nr_free = shrink_pages;
		shrink_pages = ttm_dma_page_pool_free(p->pool, nr_free);
		pr_debug("%s: (%s:%d) Asked to shrink %d, have %d more to go\n",
			 p->pool->dev_name, p->pool->name, current->pid,
			 nr_free, shrink_pages);
	}
	mutex_unlock(&_manager->lock);
	
	return ttm_dma_pool_get_num_unused_pages();
}

static void ttm_dma_pool_mm_shrink_init(struct ttm_pool_manager *manager)
{
	manager->mm_shrink.shrink = &ttm_dma_pool_mm_shrink;
	manager->mm_shrink.seeks = 1;
	register_shrinker(&manager->mm_shrink);
}

static void ttm_dma_pool_mm_shrink_fini(struct ttm_pool_manager *manager)
{
	unregister_shrinker(&manager->mm_shrink);
}

int ttm_dma_page_alloc_init(struct ttm_mem_global *glob, unsigned max_pages)
{
	int ret = -ENOMEM;

	WARN_ON(_manager);

	pr_info("Initializing DMA pool allocator\n");

	_manager = kzalloc(sizeof(*_manager), GFP_KERNEL);
	if (!_manager)
		goto err_manager;

	mutex_init(&_manager->lock);
	INIT_LIST_HEAD(&_manager->pools);

	_manager->options.max_size = max_pages;
	_manager->options.small = SMALL_ALLOCATION;
	_manager->options.alloc_size = NUM_PAGES_TO_ALLOC;

	
	ret = kobject_init_and_add(&_manager->kobj, &ttm_pool_kobj_type,
				   &glob->kobj, "dma_pool");
	if (unlikely(ret != 0)) {
		kobject_put(&_manager->kobj);
		goto err;
	}
	ttm_dma_pool_mm_shrink_init(_manager);
	return 0;
err_manager:
	kfree(_manager);
	_manager = NULL;
err:
	return ret;
}

void ttm_dma_page_alloc_fini(void)
{
	struct device_pools *p, *t;

	pr_info("Finalizing DMA pool allocator\n");
	ttm_dma_pool_mm_shrink_fini(_manager);

	list_for_each_entry_safe_reverse(p, t, &_manager->pools, pools) {
		dev_dbg(p->dev, "(%s:%d) Freeing.\n", p->pool->name,
			current->pid);
		WARN_ON(devres_destroy(p->dev, ttm_dma_pool_release,
			ttm_dma_pool_match, p->pool));
		ttm_dma_free_pool(p->dev, p->pool->type);
	}
	kobject_put(&_manager->kobj);
	_manager = NULL;
}

int ttm_dma_page_alloc_debugfs(struct seq_file *m, void *data)
{
	struct device_pools *p;
	struct dma_pool *pool = NULL;
	char *h[] = {"pool", "refills", "pages freed", "inuse", "available",
		     "name", "virt", "busaddr"};

	if (!_manager) {
		seq_printf(m, "No pool allocator running.\n");
		return 0;
	}
	seq_printf(m, "%13s %12s %13s %8s %8s %8s\n",
		   h[0], h[1], h[2], h[3], h[4], h[5]);
	mutex_lock(&_manager->lock);
	list_for_each_entry(p, &_manager->pools, pools) {
		struct device *dev = p->dev;
		if (!dev)
			continue;
		pool = p->pool;
		seq_printf(m, "%13s %12ld %13ld %8d %8d %8s\n",
				pool->name, pool->nrefills,
				pool->nfrees, pool->npages_in_use,
				pool->npages_free,
				pool->dev_name);
	}
	mutex_unlock(&_manager->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(ttm_dma_page_alloc_debugfs);
