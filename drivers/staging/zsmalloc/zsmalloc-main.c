/*
 * zsmalloc memory allocator
 *
 * Copyright (C) 2011  Nitin Gupta
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the license that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 */



#ifdef CONFIG_ZSMALLOC_DEBUG
#define DEBUG
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/tlbflush.h>
#include <asm/pgtable.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/vmalloc.h>
#include <linux/hardirq.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "zsmalloc.h"

#define ZS_ALIGN		8

#define ZS_MAX_ZSPAGE_ORDER 2
#define ZS_MAX_PAGES_PER_ZSPAGE (_AC(1, UL) << ZS_MAX_ZSPAGE_ORDER)


#ifndef MAX_PHYSMEM_BITS
#ifdef CONFIG_HIGHMEM64G
#define MAX_PHYSMEM_BITS 36
#else 
#define MAX_PHYSMEM_BITS BITS_PER_LONG
#endif
#endif
#define _PFN_BITS		(MAX_PHYSMEM_BITS - PAGE_SHIFT)
#define OBJ_INDEX_BITS	(BITS_PER_LONG - _PFN_BITS)
#define OBJ_INDEX_MASK	((_AC(1, UL) << OBJ_INDEX_BITS) - 1)

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define ZS_MIN_ALLOC_SIZE \
	MAX(32, (ZS_MAX_PAGES_PER_ZSPAGE << PAGE_SHIFT >> OBJ_INDEX_BITS))
#define ZS_MAX_ALLOC_SIZE	PAGE_SIZE

#define ZS_SIZE_CLASS_DELTA	(PAGE_SIZE >> 8)
#define ZS_SIZE_CLASSES		((ZS_MAX_ALLOC_SIZE - ZS_MIN_ALLOC_SIZE) / \
					ZS_SIZE_CLASS_DELTA + 1)

enum fullness_group {
	ZS_ALMOST_FULL,
	ZS_ALMOST_EMPTY,
	_ZS_NR_FULLNESS_GROUPS,

	ZS_EMPTY,
	ZS_FULL
};

static const int fullness_threshold_frac = 4;

struct size_class {
	int size;
	unsigned int index;

	
	int pages_per_zspage;

	spinlock_t lock;

	
	u64 pages_allocated;

	struct page *fullness_list[_ZS_NR_FULLNESS_GROUPS];
};

struct link_free {
	
	void *next;
};

struct zs_pool {
	struct size_class size_class[ZS_SIZE_CLASSES];

	gfp_t flags;	
};

#define CLASS_IDX_BITS	28
#define FULLNESS_BITS	4
#define CLASS_IDX_MASK	((1 << CLASS_IDX_BITS) - 1)
#define FULLNESS_MASK	((1 << FULLNESS_BITS) - 1)

#if defined(CONFIG_ARM) && !defined(MODULE)
#define USE_PGTABLE_MAPPING
#endif

struct mapping_area {
#ifdef USE_PGTABLE_MAPPING
	struct vm_struct *vm; 
#else
	char *vm_buf; 
#endif
	char *vm_addr; 
	enum zs_mapmode vm_mm; 
};


static DEFINE_PER_CPU(struct mapping_area, zs_map_area);

static int is_first_page(struct page *page)
{
	return PagePrivate(page);
}

static int is_last_page(struct page *page)
{
	return PagePrivate2(page);
}

static void get_zspage_mapping(struct page *page, unsigned int *class_idx,
				enum fullness_group *fullness)
{
	unsigned long m;
	BUG_ON(!is_first_page(page));

	m = (unsigned long)page->mapping;
	*fullness = m & FULLNESS_MASK;
	*class_idx = (m >> FULLNESS_BITS) & CLASS_IDX_MASK;
}

static void set_zspage_mapping(struct page *page, unsigned int class_idx,
				enum fullness_group fullness)
{
	unsigned long m;
	BUG_ON(!is_first_page(page));

	m = ((class_idx & CLASS_IDX_MASK) << FULLNESS_BITS) |
			(fullness & FULLNESS_MASK);
	page->mapping = (struct address_space *)m;
}

static int get_size_class_index(int size)
{
	int idx = 0;

	if (likely(size > ZS_MIN_ALLOC_SIZE))
		idx = DIV_ROUND_UP(size - ZS_MIN_ALLOC_SIZE,
				ZS_SIZE_CLASS_DELTA);

	return idx;
}

static enum fullness_group get_fullness_group(struct page *page)
{
	int inuse, max_objects;
	enum fullness_group fg;
	BUG_ON(!is_first_page(page));

	inuse = page->inuse;
	max_objects = page->objects;

	if (inuse == 0)
		fg = ZS_EMPTY;
	else if (inuse == max_objects)
		fg = ZS_FULL;
	else if (inuse <= max_objects / fullness_threshold_frac)
		fg = ZS_ALMOST_EMPTY;
	else
		fg = ZS_ALMOST_FULL;

	return fg;
}

static void insert_zspage(struct page *page, struct size_class *class,
				enum fullness_group fullness)
{
	struct page **head;

	BUG_ON(!is_first_page(page));

	if (fullness >= _ZS_NR_FULLNESS_GROUPS)
		return;

	head = &class->fullness_list[fullness];
	if (*head)
		list_add_tail(&page->lru, &(*head)->lru);

	*head = page;
}

static void remove_zspage(struct page *page, struct size_class *class,
				enum fullness_group fullness)
{
	struct page **head;

	BUG_ON(!is_first_page(page));

	if (fullness >= _ZS_NR_FULLNESS_GROUPS)
		return;

	head = &class->fullness_list[fullness];
	BUG_ON(!*head);
	if (list_empty(&(*head)->lru))
		*head = NULL;
	else if (*head == page)
		*head = (struct page *)list_entry((*head)->lru.next,
					struct page, lru);

	list_del_init(&page->lru);
}

static enum fullness_group fix_fullness_group(struct zs_pool *pool,
						struct page *page)
{
	int class_idx;
	struct size_class *class;
	enum fullness_group currfg, newfg;

	BUG_ON(!is_first_page(page));

	get_zspage_mapping(page, &class_idx, &currfg);
	newfg = get_fullness_group(page);
	if (newfg == currfg)
		goto out;

	class = &pool->size_class[class_idx];
	remove_zspage(page, class, currfg);
	insert_zspage(page, class, newfg);
	set_zspage_mapping(page, class_idx, newfg);

out:
	return newfg;
}

static int get_pages_per_zspage(int class_size)
{
	int i, max_usedpc = 0;
	
	int max_usedpc_order = 1;

	for (i = 1; i <= ZS_MAX_PAGES_PER_ZSPAGE; i++) {
		int zspage_size;
		int waste, usedpc;

		zspage_size = i * PAGE_SIZE;
		waste = zspage_size % class_size;
		usedpc = (zspage_size - waste) * 100 / zspage_size;

		if (usedpc > max_usedpc) {
			max_usedpc = usedpc;
			max_usedpc_order = i;
		}
	}

	return max_usedpc_order;
}

static struct page *get_first_page(struct page *page)
{
	if (is_first_page(page))
		return page;
	else
		return page->first_page;
}

static struct page *get_next_page(struct page *page)
{
	struct page *next;

	if (is_last_page(page))
		next = NULL;
	else if (is_first_page(page))
		next = (struct page *)page_private(page);
	else
		next = list_entry(page->lru.next, struct page, lru);

	return next;
}

static void *obj_location_to_handle(struct page *page, unsigned long obj_idx)
{
	unsigned long handle;

	if (!page) {
		BUG_ON(obj_idx);
		return NULL;
	}

	handle = page_to_pfn(page) << OBJ_INDEX_BITS;
	handle |= ((obj_idx + 1) & OBJ_INDEX_MASK);

	return (void *)handle;
}

static void obj_handle_to_location(unsigned long handle, struct page **page,
				unsigned long *obj_idx)
{
	*page = pfn_to_page(handle >> OBJ_INDEX_BITS);
	*obj_idx = (handle & OBJ_INDEX_MASK) - 1;
}

static unsigned long obj_idx_to_offset(struct page *page,
				unsigned long obj_idx, int class_size)
{
	unsigned long off = 0;

	if (!is_first_page(page))
		off = page->index;

	return off + obj_idx * class_size;
}

static void reset_page(struct page *page)
{
	clear_bit(PG_private, &page->flags);
	clear_bit(PG_private_2, &page->flags);
	set_page_private(page, 0);
	page->mapping = NULL;
	page->freelist = NULL;
	reset_page_mapcount(page);
}

static void free_zspage(struct page *first_page)
{
	struct page *nextp, *tmp, *head_extra;

	BUG_ON(!is_first_page(first_page));
	BUG_ON(first_page->inuse);

	head_extra = (struct page *)page_private(first_page);

	reset_page(first_page);
	__free_page(first_page);

	
	if (!head_extra)
		return;

	list_for_each_entry_safe(nextp, tmp, &head_extra->lru, lru) {
		list_del(&nextp->lru);
		reset_page(nextp);
		__free_page(nextp);
	}
	reset_page(head_extra);
	__free_page(head_extra);
}

static void init_zspage(struct page *first_page, struct size_class *class)
{
	unsigned long off = 0;
	struct page *page = first_page;

	BUG_ON(!is_first_page(first_page));
	while (page) {
		struct page *next_page;
		struct link_free *link;
		unsigned int i, objs_on_page;

		if (page != first_page)
			page->index = off;

		link = (struct link_free *)kmap_atomic(page) +
						off / sizeof(*link);
		objs_on_page = (PAGE_SIZE - off) / class->size;

		for (i = 1; i <= objs_on_page; i++) {
			off += class->size;
			if (off < PAGE_SIZE) {
				link->next = obj_location_to_handle(page, i);
				link += class->size / sizeof(*link);
			}
		}

		next_page = get_next_page(page);
		link->next = obj_location_to_handle(next_page, 0);
		kunmap_atomic(link);
		page = next_page;
		off = (off + class->size) % PAGE_SIZE;
	}
}

static struct page *alloc_zspage(struct size_class *class, gfp_t flags)
{
	int i, error;
	struct page *first_page = NULL, *uninitialized_var(prev_page);

	error = -ENOMEM;
	for (i = 0; i < class->pages_per_zspage; i++) {
		struct page *page;

		page = alloc_page(flags);
		if (!page)
			goto cleanup;

		INIT_LIST_HEAD(&page->lru);
		if (i == 0) {	
			SetPagePrivate(page);
			set_page_private(page, 0);
			first_page = page;
			first_page->inuse = 0;
		}
		if (i == 1)
			set_page_private(first_page, (unsigned long)page);
		if (i >= 1)
			page->first_page = first_page;
		if (i >= 2)
			list_add(&page->lru, &prev_page->lru);
		if (i == class->pages_per_zspage - 1)	
			SetPagePrivate2(page);
		prev_page = page;
	}

	init_zspage(first_page, class);

	first_page->freelist = obj_location_to_handle(first_page, 0);
	
	first_page->objects = class->pages_per_zspage * PAGE_SIZE / class->size;

	error = 0; 

cleanup:
	if (unlikely(error) && first_page) {
		free_zspage(first_page);
		first_page = NULL;
	}

	return first_page;
}

static struct page *find_get_zspage(struct size_class *class)
{
	int i;
	struct page *page;

	for (i = 0; i < _ZS_NR_FULLNESS_GROUPS; i++) {
		page = class->fullness_list[i];
		if (page)
			break;
	}

	return page;
}

#ifdef USE_PGTABLE_MAPPING
static inline int __zs_cpu_up(struct mapping_area *area)
{
	if (area->vm)
		return 0;
	area->vm = alloc_vm_area(PAGE_SIZE * 2, NULL);
	if (!area->vm)
		return -ENOMEM;
	return 0;
}

static inline void __zs_cpu_down(struct mapping_area *area)
{
	if (area->vm)
		free_vm_area(area->vm);
	area->vm = NULL;
}

static inline void *__zs_map_object(struct mapping_area *area,
				struct page *pages[2], int off, int size)
{
	BUG_ON(map_vm_area(area->vm, PAGE_KERNEL, &pages));
	area->vm_addr = area->vm->addr;
	return area->vm_addr + off;
}

static inline void __zs_unmap_object(struct mapping_area *area,
				struct page *pages[2], int off, int size)
{
	unsigned long addr = (unsigned long)area->vm_addr;

	unmap_kernel_range(addr, PAGE_SIZE * 2);
}

#else 

static inline int __zs_cpu_up(struct mapping_area *area)
{
	if (area->vm_buf)
		return 0;
	area->vm_buf = (char *)__get_free_page(GFP_KERNEL);
	if (!area->vm_buf)
		return -ENOMEM;
	return 0;
}

static inline void __zs_cpu_down(struct mapping_area *area)
{
	if (area->vm_buf)
		free_page((unsigned long)area->vm_buf);
	area->vm_buf = NULL;
}

static void *__zs_map_object(struct mapping_area *area,
			struct page *pages[2], int off, int size)
{
	int sizes[2];
	void *addr;
	char *buf = area->vm_buf;

	
	pagefault_disable();

	
	if (area->vm_mm == ZS_MM_WO)
		goto out;

	sizes[0] = PAGE_SIZE - off;
	sizes[1] = size - sizes[0];

	
	addr = kmap_atomic(pages[0]);
	memcpy(buf, addr + off, sizes[0]);
	kunmap_atomic(addr);
	addr = kmap_atomic(pages[1]);
	memcpy(buf + sizes[0], addr, sizes[1]);
	kunmap_atomic(addr);
out:
	return area->vm_buf;
}

static void __zs_unmap_object(struct mapping_area *area,
			struct page *pages[2], int off, int size)
{
	int sizes[2];
	void *addr;
	char *buf = area->vm_buf;

	
	if (area->vm_mm == ZS_MM_RO)
		goto out;

	sizes[0] = PAGE_SIZE - off;
	sizes[1] = size - sizes[0];

	
	addr = kmap_atomic(pages[0]);
	memcpy(addr + off, buf, sizes[0]);
	kunmap_atomic(addr);
	addr = kmap_atomic(pages[1]);
	memcpy(addr, buf + sizes[0], sizes[1]);
	kunmap_atomic(addr);

out:
	
	pagefault_enable();
}

#endif 

static int zs_cpu_notifier(struct notifier_block *nb, unsigned long action,
				void *pcpu)
{
	int ret, cpu = (long)pcpu;
	struct mapping_area *area;

	switch (action) {
	case CPU_UP_PREPARE:
		area = &per_cpu(zs_map_area, cpu);
		ret = __zs_cpu_up(area);
		if (ret)
			return notifier_from_errno(ret);
		break;
	case CPU_DEAD:
	case CPU_UP_CANCELED:
		area = &per_cpu(zs_map_area, cpu);
		__zs_cpu_down(area);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block zs_cpu_nb = {
	.notifier_call = zs_cpu_notifier
};

static void zs_exit(void)
{
	int cpu;

	for_each_online_cpu(cpu)
		zs_cpu_notifier(NULL, CPU_DEAD, (void *)(long)cpu);
	unregister_cpu_notifier(&zs_cpu_nb);
}

static int zs_init(void)
{
	int cpu, ret;

	register_cpu_notifier(&zs_cpu_nb);
	for_each_online_cpu(cpu) {
		ret = zs_cpu_notifier(NULL, CPU_UP_PREPARE, (void *)(long)cpu);
		if (notifier_to_errno(ret))
			goto fail;
	}
	return 0;
fail:
	zs_exit();
	return notifier_to_errno(ret);
}

struct zs_pool *zs_create_pool(gfp_t flags)
{
	int i, ovhd_size;
	struct zs_pool *pool;

	ovhd_size = roundup(sizeof(*pool), PAGE_SIZE);
	pool = kzalloc(ovhd_size, GFP_KERNEL);
	if (!pool)
		return NULL;

	for (i = 0; i < ZS_SIZE_CLASSES; i++) {
		int size;
		struct size_class *class;

		size = ZS_MIN_ALLOC_SIZE + i * ZS_SIZE_CLASS_DELTA;
		if (size > ZS_MAX_ALLOC_SIZE)
			size = ZS_MAX_ALLOC_SIZE;

		class = &pool->size_class[i];
		class->size = size;
		class->index = i;
		spin_lock_init(&class->lock);
		class->pages_per_zspage = get_pages_per_zspage(size);

	}

	pool->flags = flags;

	return pool;
}
EXPORT_SYMBOL_GPL(zs_create_pool);

void zs_destroy_pool(struct zs_pool *pool)
{
	int i;

	for (i = 0; i < ZS_SIZE_CLASSES; i++) {
		int fg;
		struct size_class *class = &pool->size_class[i];

		for (fg = 0; fg < _ZS_NR_FULLNESS_GROUPS; fg++) {
			if (class->fullness_list[fg]) {
				pr_info("Freeing non-empty class with size %db, fullness group %d\n",
					class->size, fg);
			}
		}
	}
	kfree(pool);
}
EXPORT_SYMBOL_GPL(zs_destroy_pool);

unsigned long zs_malloc(struct zs_pool *pool, size_t size)
{
	unsigned long obj;
	struct link_free *link;
	int class_idx;
	struct size_class *class;

	struct page *first_page, *m_page;
	unsigned long m_objidx, m_offset;

	if (unlikely(!size || size > ZS_MAX_ALLOC_SIZE))
		return 0;

	class_idx = get_size_class_index(size);
	class = &pool->size_class[class_idx];
	BUG_ON(class_idx != class->index);

	spin_lock(&class->lock);
	first_page = find_get_zspage(class);

	if (!first_page) {
		spin_unlock(&class->lock);
		first_page = alloc_zspage(class, pool->flags);
		if (unlikely(!first_page))
			return 0;

		set_zspage_mapping(first_page, class->index, ZS_EMPTY);
		spin_lock(&class->lock);
		class->pages_allocated += class->pages_per_zspage;
	}

	obj = (unsigned long)first_page->freelist;
	obj_handle_to_location(obj, &m_page, &m_objidx);
	m_offset = obj_idx_to_offset(m_page, m_objidx, class->size);

	link = (struct link_free *)kmap_atomic(m_page) +
					m_offset / sizeof(*link);
	first_page->freelist = link->next;
	memset(link, POISON_INUSE, sizeof(*link));
	kunmap_atomic(link);

	first_page->inuse++;
	
	fix_fullness_group(pool, first_page);
	spin_unlock(&class->lock);

	return obj;
}
EXPORT_SYMBOL_GPL(zs_malloc);

void zs_free(struct zs_pool *pool, unsigned long obj)
{
	struct link_free *link;
	struct page *first_page, *f_page;
	unsigned long f_objidx, f_offset;

	int class_idx;
	struct size_class *class;
	enum fullness_group fullness;

	if (unlikely(!obj))
		return;

	obj_handle_to_location(obj, &f_page, &f_objidx);
	first_page = get_first_page(f_page);

	get_zspage_mapping(first_page, &class_idx, &fullness);
	class = &pool->size_class[class_idx];
	f_offset = obj_idx_to_offset(f_page, f_objidx, class->size);

	spin_lock(&class->lock);

	
	link = (struct link_free *)((unsigned char *)kmap_atomic(f_page)
							+ f_offset);
	link->next = first_page->freelist;
	kunmap_atomic(link);
	first_page->freelist = (void *)obj;

	first_page->inuse--;
	fullness = fix_fullness_group(pool, first_page);

	if (fullness == ZS_EMPTY)
		class->pages_allocated -= class->pages_per_zspage;

	spin_unlock(&class->lock);

	if (fullness == ZS_EMPTY)
		free_zspage(first_page);
}
EXPORT_SYMBOL_GPL(zs_free);

void *zs_map_object(struct zs_pool *pool, unsigned long handle,
			enum zs_mapmode mm)
{
	struct page *page;
	unsigned long obj_idx, off;

	unsigned int class_idx;
	enum fullness_group fg;
	struct size_class *class;
	struct mapping_area *area;
	struct page *pages[2];

	BUG_ON(!handle);

	BUG_ON(in_interrupt());

	obj_handle_to_location(handle, &page, &obj_idx);
	get_zspage_mapping(get_first_page(page), &class_idx, &fg);
	class = &pool->size_class[class_idx];
	off = obj_idx_to_offset(page, obj_idx, class->size);

	area = &get_cpu_var(zs_map_area);
	area->vm_mm = mm;
	if (off + class->size <= PAGE_SIZE) {
		
		area->vm_addr = kmap_atomic(page);
		return area->vm_addr + off;
	}

	
	pages[0] = page;
	pages[1] = get_next_page(page);
	BUG_ON(!pages[1]);

	return __zs_map_object(area, pages, off, class->size);
}
EXPORT_SYMBOL_GPL(zs_map_object);

void zs_unmap_object(struct zs_pool *pool, unsigned long handle)
{
	struct page *page;
	unsigned long obj_idx, off;

	unsigned int class_idx;
	enum fullness_group fg;
	struct size_class *class;
	struct mapping_area *area;

	BUG_ON(!handle);

	obj_handle_to_location(handle, &page, &obj_idx);
	get_zspage_mapping(get_first_page(page), &class_idx, &fg);
	class = &pool->size_class[class_idx];
	off = obj_idx_to_offset(page, obj_idx, class->size);

	area = &__get_cpu_var(zs_map_area);
	if (off + class->size <= PAGE_SIZE)
		kunmap_atomic(area->vm_addr);
	else {
		struct page *pages[2];

		pages[0] = page;
		pages[1] = get_next_page(page);
		BUG_ON(!pages[1]);

		__zs_unmap_object(area, pages, off, class->size);
	}
	put_cpu_var(zs_map_area);
}
EXPORT_SYMBOL_GPL(zs_unmap_object);

u64 zs_get_total_size_bytes(struct zs_pool *pool)
{
	int i;
	u64 npages = 0;

	for (i = 0; i < ZS_SIZE_CLASSES; i++)
		npages += pool->size_class[i].pages_allocated;

	return npages << PAGE_SHIFT;
}
EXPORT_SYMBOL_GPL(zs_get_total_size_bytes);

module_init(zs_init);
module_exit(zs_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nitin Gupta <ngupta@vflare.org>");
