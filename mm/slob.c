
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/swap.h> 
#include <linux/cache.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/rcupdate.h>
#include <linux/list.h>
#include <linux/kmemleak.h>

#include <trace/events/kmem.h>

#include <linux/atomic.h>

#if PAGE_SIZE <= (32767 * 2)
typedef s16 slobidx_t;
#else
typedef s32 slobidx_t;
#endif

struct slob_block {
	slobidx_t units;
};
typedef struct slob_block slob_t;

struct slob_page {
	union {
		struct {
			unsigned long flags;	
			atomic_t _count;	
			slobidx_t units;	
			unsigned long pad[2];
			slob_t *free;		
			struct list_head list;	
		};
		struct page page;
	};
};
static inline void struct_slob_page_wrong_size(void)
{ BUILD_BUG_ON(sizeof(struct slob_page) != sizeof(struct page)); }

static inline void free_slob_page(struct slob_page *sp)
{
	reset_page_mapcount(&sp->page);
	sp->page.mapping = NULL;
}

#define SLOB_BREAK1 256
#define SLOB_BREAK2 1024
static LIST_HEAD(free_slob_small);
static LIST_HEAD(free_slob_medium);
static LIST_HEAD(free_slob_large);

static inline int is_slob_page(struct slob_page *sp)
{
	return PageSlab((struct page *)sp);
}

static inline void set_slob_page(struct slob_page *sp)
{
	__SetPageSlab((struct page *)sp);
}

static inline void clear_slob_page(struct slob_page *sp)
{
	__ClearPageSlab((struct page *)sp);
}

static inline struct slob_page *slob_page(const void *addr)
{
	return (struct slob_page *)virt_to_page(addr);
}

static inline int slob_page_free(struct slob_page *sp)
{
	return PageSlobFree((struct page *)sp);
}

static void set_slob_page_free(struct slob_page *sp, struct list_head *list)
{
	list_add(&sp->list, list);
	__SetPageSlobFree((struct page *)sp);
}

static inline void clear_slob_page_free(struct slob_page *sp)
{
	list_del(&sp->list);
	__ClearPageSlobFree((struct page *)sp);
}

#define SLOB_UNIT sizeof(slob_t)
#define SLOB_UNITS(size) (((size) + SLOB_UNIT - 1)/SLOB_UNIT)
#define SLOB_ALIGN L1_CACHE_BYTES

struct slob_rcu {
	struct rcu_head head;
	int size;
};

static DEFINE_SPINLOCK(slob_lock);

static void set_slob(slob_t *s, slobidx_t size, slob_t *next)
{
	slob_t *base = (slob_t *)((unsigned long)s & PAGE_MASK);
	slobidx_t offset = next - base;

	if (size > 1) {
		s[0].units = size;
		s[1].units = offset;
	} else
		s[0].units = -offset;
}

static slobidx_t slob_units(slob_t *s)
{
	if (s->units > 0)
		return s->units;
	return 1;
}

static slob_t *slob_next(slob_t *s)
{
	slob_t *base = (slob_t *)((unsigned long)s & PAGE_MASK);
	slobidx_t next;

	if (s[0].units < 0)
		next = -s[0].units;
	else
		next = s[1].units;
	return base+next;
}

static int slob_last(slob_t *s)
{
	return !((unsigned long)slob_next(s) & ~PAGE_MASK);
}

static void *slob_new_pages(gfp_t gfp, int order, int node)
{
	void *page;

#ifdef CONFIG_NUMA
	if (node != -1)
		page = alloc_pages_exact_node(node, gfp, order);
	else
#endif
		page = alloc_pages(gfp, order);

	if (!page)
		return NULL;

	return page_address(page);
}

static void slob_free_pages(void *b, int order)
{
	if (current->reclaim_state)
		current->reclaim_state->reclaimed_slab += 1 << order;
	free_pages((unsigned long)b, order);
}

static void *slob_page_alloc(struct slob_page *sp, size_t size, int align)
{
	slob_t *prev, *cur, *aligned = NULL;
	int delta = 0, units = SLOB_UNITS(size);

	for (prev = NULL, cur = sp->free; ; prev = cur, cur = slob_next(cur)) {
		slobidx_t avail = slob_units(cur);

		if (align) {
			aligned = (slob_t *)ALIGN((unsigned long)cur, align);
			delta = aligned - cur;
		}
		if (avail >= units + delta) { 
			slob_t *next;

			if (delta) { 
				next = slob_next(cur);
				set_slob(aligned, avail - delta, next);
				set_slob(cur, delta, aligned);
				prev = cur;
				cur = aligned;
				avail = slob_units(cur);
			}

			next = slob_next(cur);
			if (avail == units) { 
				if (prev)
					set_slob(prev, slob_units(prev), next);
				else
					sp->free = next;
			} else { 
				if (prev)
					set_slob(prev, slob_units(prev), cur + units);
				else
					sp->free = cur + units;
				set_slob(cur + units, avail - units, next);
			}

			sp->units -= units;
			if (!sp->units)
				clear_slob_page_free(sp);
			return cur;
		}
		if (slob_last(cur))
			return NULL;
	}
}

static void *slob_alloc(size_t size, gfp_t gfp, int align, int node)
{
	struct slob_page *sp;
	struct list_head *prev;
	struct list_head *slob_list;
	slob_t *b = NULL;
	unsigned long flags;

	if (size < SLOB_BREAK1)
		slob_list = &free_slob_small;
	else if (size < SLOB_BREAK2)
		slob_list = &free_slob_medium;
	else
		slob_list = &free_slob_large;

	spin_lock_irqsave(&slob_lock, flags);
	
	list_for_each_entry(sp, slob_list, list) {
#ifdef CONFIG_NUMA
		if (node != -1 && page_to_nid(&sp->page) != node)
			continue;
#endif
		
		if (sp->units < SLOB_UNITS(size))
			continue;

		
		prev = sp->list.prev;
		b = slob_page_alloc(sp, size, align);
		if (!b)
			continue;

		if (prev != slob_list->prev &&
				slob_list->next != prev->next)
			list_move_tail(slob_list, prev->next);
		break;
	}
	spin_unlock_irqrestore(&slob_lock, flags);

	
	if (!b) {
		b = slob_new_pages(gfp & ~__GFP_ZERO, 0, node);
		if (!b)
			return NULL;
		sp = slob_page(b);
		set_slob_page(sp);

		spin_lock_irqsave(&slob_lock, flags);
		sp->units = SLOB_UNITS(PAGE_SIZE);
		sp->free = b;
		INIT_LIST_HEAD(&sp->list);
		set_slob(b, SLOB_UNITS(PAGE_SIZE), b + SLOB_UNITS(PAGE_SIZE));
		set_slob_page_free(sp, slob_list);
		b = slob_page_alloc(sp, size, align);
		BUG_ON(!b);
		spin_unlock_irqrestore(&slob_lock, flags);
	}
	if (unlikely((gfp & __GFP_ZERO) && b))
		memset(b, 0, size);
	return b;
}

static void slob_free(void *block, int size)
{
	struct slob_page *sp;
	slob_t *prev, *next, *b = (slob_t *)block;
	slobidx_t units;
	unsigned long flags;
	struct list_head *slob_list;

	if (unlikely(ZERO_OR_NULL_PTR(block)))
		return;
	BUG_ON(!size);

	sp = slob_page(block);
	units = SLOB_UNITS(size);

	spin_lock_irqsave(&slob_lock, flags);

	if (sp->units + units == SLOB_UNITS(PAGE_SIZE)) {
		
		if (slob_page_free(sp))
			clear_slob_page_free(sp);
		spin_unlock_irqrestore(&slob_lock, flags);
		clear_slob_page(sp);
		free_slob_page(sp);
		slob_free_pages(b, 0);
		return;
	}

	if (!slob_page_free(sp)) {
		
		sp->units = units;
		sp->free = b;
		set_slob(b, units,
			(void *)((unsigned long)(b +
					SLOB_UNITS(PAGE_SIZE)) & PAGE_MASK));
		if (size < SLOB_BREAK1)
			slob_list = &free_slob_small;
		else if (size < SLOB_BREAK2)
			slob_list = &free_slob_medium;
		else
			slob_list = &free_slob_large;
		set_slob_page_free(sp, slob_list);
		goto out;
	}

	sp->units += units;

	if (b < sp->free) {
		if (b + units == sp->free) {
			units += slob_units(sp->free);
			sp->free = slob_next(sp->free);
		}
		set_slob(b, units, sp->free);
		sp->free = b;
	} else {
		prev = sp->free;
		next = slob_next(prev);
		while (b > next) {
			prev = next;
			next = slob_next(prev);
		}

		if (!slob_last(prev) && b + units == next) {
			units += slob_units(next);
			set_slob(b, units, slob_next(next));
		} else
			set_slob(b, units, next);

		if (prev + slob_units(prev) == b) {
			units = slob_units(b) + slob_units(prev);
			set_slob(prev, units, slob_next(b));
		} else
			set_slob(prev, slob_units(prev), b);
	}
out:
	spin_unlock_irqrestore(&slob_lock, flags);
}


void *__kmalloc_node(size_t size, gfp_t gfp, int node)
{
	unsigned int *m;
	int align = max(ARCH_KMALLOC_MINALIGN, ARCH_SLAB_MINALIGN);
	void *ret;

	gfp &= gfp_allowed_mask;

	lockdep_trace_alloc(gfp);

	if (size < PAGE_SIZE - align) {
		if (!size)
			return ZERO_SIZE_PTR;

		m = slob_alloc(size + align, gfp, align, node);

		if (!m)
			return NULL;
		*m = size;
		ret = (void *)m + align;

		trace_kmalloc_node(_RET_IP_, ret,
				   size, size + align, gfp, node);
	} else {
		unsigned int order = get_order(size);

		if (likely(order))
			gfp |= __GFP_COMP;
		ret = slob_new_pages(gfp, order, node);
		if (ret) {
			struct page *page;
			page = virt_to_page(ret);
			page->private = size;
		}

		trace_kmalloc_node(_RET_IP_, ret,
				   size, PAGE_SIZE << order, gfp, node);
	}

	kmemleak_alloc(ret, size, 1, gfp);
	return ret;
}
EXPORT_SYMBOL(__kmalloc_node);

void kfree(const void *block)
{
	struct slob_page *sp;

	trace_kfree(_RET_IP_, block);

	if (unlikely(ZERO_OR_NULL_PTR(block)))
		return;
	kmemleak_free(block);

	sp = slob_page(block);
	if (is_slob_page(sp)) {
		int align = max(ARCH_KMALLOC_MINALIGN, ARCH_SLAB_MINALIGN);
		unsigned int *m = (unsigned int *)(block - align);
		slob_free(m, *m + align);
	} else
		put_page(&sp->page);
}
EXPORT_SYMBOL(kfree);

size_t ksize(const void *block)
{
	struct slob_page *sp;

	BUG_ON(!block);
	if (unlikely(block == ZERO_SIZE_PTR))
		return 0;

	sp = slob_page(block);
	if (is_slob_page(sp)) {
		int align = max(ARCH_KMALLOC_MINALIGN, ARCH_SLAB_MINALIGN);
		unsigned int *m = (unsigned int *)(block - align);
		return SLOB_UNITS(*m) * SLOB_UNIT;
	} else
		return sp->page.private;
}
EXPORT_SYMBOL(ksize);

struct kmem_cache {
	unsigned int size, align;
	unsigned long flags;
	const char *name;
	void (*ctor)(void *);
};

struct kmem_cache *kmem_cache_create(const char *name, size_t size,
	size_t align, unsigned long flags, void (*ctor)(void *))
{
	struct kmem_cache *c;

	c = slob_alloc(sizeof(struct kmem_cache),
		GFP_KERNEL, ARCH_KMALLOC_MINALIGN, -1);

	if (c) {
		c->name = name;
		c->size = size;
		if (flags & SLAB_DESTROY_BY_RCU) {
			
			c->size += sizeof(struct slob_rcu);
		}
		c->flags = flags;
		c->ctor = ctor;
		
		c->align = (flags & SLAB_HWCACHE_ALIGN) ? SLOB_ALIGN : 0;
		if (c->align < ARCH_SLAB_MINALIGN)
			c->align = ARCH_SLAB_MINALIGN;
		if (c->align < align)
			c->align = align;
	} else if (flags & SLAB_PANIC)
		panic("Cannot create slab cache %s\n", name);

	kmemleak_alloc(c, sizeof(struct kmem_cache), 1, GFP_KERNEL);
	return c;
}
EXPORT_SYMBOL(kmem_cache_create);

void kmem_cache_destroy(struct kmem_cache *c)
{
	kmemleak_free(c);
	if (c->flags & SLAB_DESTROY_BY_RCU)
		rcu_barrier();
	slob_free(c, sizeof(struct kmem_cache));
}
EXPORT_SYMBOL(kmem_cache_destroy);

void *kmem_cache_alloc_node(struct kmem_cache *c, gfp_t flags, int node)
{
	void *b;

	flags &= gfp_allowed_mask;

	lockdep_trace_alloc(flags);

	if (c->size < PAGE_SIZE) {
		b = slob_alloc(c->size, flags, c->align, node);
		trace_kmem_cache_alloc_node(_RET_IP_, b, c->size,
					    SLOB_UNITS(c->size) * SLOB_UNIT,
					    flags, node);
	} else {
		b = slob_new_pages(flags, get_order(c->size), node);
		trace_kmem_cache_alloc_node(_RET_IP_, b, c->size,
					    PAGE_SIZE << get_order(c->size),
					    flags, node);
	}

	if (c->ctor)
		c->ctor(b);

	kmemleak_alloc_recursive(b, c->size, 1, c->flags, flags);
	return b;
}
EXPORT_SYMBOL(kmem_cache_alloc_node);

static void __kmem_cache_free(void *b, int size)
{
	if (size < PAGE_SIZE)
		slob_free(b, size);
	else
		slob_free_pages(b, get_order(size));
}

static void kmem_rcu_free(struct rcu_head *head)
{
	struct slob_rcu *slob_rcu = (struct slob_rcu *)head;
	void *b = (void *)slob_rcu - (slob_rcu->size - sizeof(struct slob_rcu));

	__kmem_cache_free(b, slob_rcu->size);
}

void kmem_cache_free(struct kmem_cache *c, void *b)
{
	kmemleak_free_recursive(b, c->flags);
	if (unlikely(c->flags & SLAB_DESTROY_BY_RCU)) {
		struct slob_rcu *slob_rcu;
		slob_rcu = b + (c->size - sizeof(struct slob_rcu));
		slob_rcu->size = c->size;
		call_rcu(&slob_rcu->head, kmem_rcu_free);
	} else {
		__kmem_cache_free(b, c->size);
	}

	trace_kmem_cache_free(_RET_IP_, b);
}
EXPORT_SYMBOL(kmem_cache_free);

unsigned int kmem_cache_size(struct kmem_cache *c)
{
	return c->size;
}
EXPORT_SYMBOL(kmem_cache_size);

int kmem_cache_shrink(struct kmem_cache *d)
{
	return 0;
}
EXPORT_SYMBOL(kmem_cache_shrink);

static unsigned int slob_ready __read_mostly;

int slab_is_available(void)
{
	return slob_ready;
}

void __init kmem_cache_init(void)
{
	slob_ready = 1;
}

void __init kmem_cache_init_late(void)
{
	
}
