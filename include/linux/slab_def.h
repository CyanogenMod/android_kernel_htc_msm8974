#ifndef _LINUX_SLAB_DEF_H
#define	_LINUX_SLAB_DEF_H


#include <linux/init.h>
#include <asm/page.h>		
#include <asm/cache.h>		
#include <linux/compiler.h>


struct kmem_cache {
	unsigned int batchcount;
	unsigned int limit;
	unsigned int shared;

	unsigned int buffer_size;
	u32 reciprocal_buffer_size;

	unsigned int flags;		
	unsigned int num;		

	
	unsigned int gfporder;

	
	gfp_t gfpflags;

	size_t colour;			
	unsigned int colour_off;	
	struct kmem_cache *slabp_cache;
	unsigned int slab_size;
	unsigned int dflags;		

	
	void (*ctor)(void *obj);

	const char *name;
	struct list_head next;

#ifdef CONFIG_DEBUG_SLAB
	unsigned long num_active;
	unsigned long num_allocations;
	unsigned long high_mark;
	unsigned long grown;
	unsigned long reaped;
	unsigned long errors;
	unsigned long max_freeable;
	unsigned long node_allocs;
	unsigned long node_frees;
	unsigned long node_overflow;
	atomic_t allochit;
	atomic_t allocmiss;
	atomic_t freehit;
	atomic_t freemiss;

	int obj_offset;
	int obj_size;
#endif 

	struct kmem_list3 **nodelists;
	struct array_cache *array[NR_CPUS];
};

struct cache_sizes {
	size_t		 	cs_size;
	struct kmem_cache	*cs_cachep;
#ifdef CONFIG_ZONE_DMA
	struct kmem_cache	*cs_dmacachep;
#endif
};
extern struct cache_sizes malloc_sizes[];

void *kmem_cache_alloc(struct kmem_cache *, gfp_t);
void *__kmalloc(size_t size, gfp_t flags);

#ifdef CONFIG_TRACING
extern void *kmem_cache_alloc_trace(size_t size,
				    struct kmem_cache *cachep, gfp_t flags);
extern size_t slab_buffer_size(struct kmem_cache *cachep);
#else
static __always_inline void *
kmem_cache_alloc_trace(size_t size, struct kmem_cache *cachep, gfp_t flags)
{
	return kmem_cache_alloc(cachep, flags);
}
static inline size_t slab_buffer_size(struct kmem_cache *cachep)
{
	return 0;
}
#endif

static __always_inline void *kmalloc(size_t size, gfp_t flags)
{
	struct kmem_cache *cachep;
	void *ret;

	if (__builtin_constant_p(size)) {
		int i = 0;

		if (!size)
			return ZERO_SIZE_PTR;

#define CACHE(x) \
		if (size <= x) \
			goto found; \
		else \
			i++;
#include <linux/kmalloc_sizes.h>
#undef CACHE
		return NULL;
found:
#ifdef CONFIG_ZONE_DMA
		if (flags & GFP_DMA)
			cachep = malloc_sizes[i].cs_dmacachep;
		else
#endif
			cachep = malloc_sizes[i].cs_cachep;

		ret = kmem_cache_alloc_trace(size, cachep, flags);

		return ret;
	}
	return __kmalloc(size, flags);
}

#ifdef CONFIG_NUMA
extern void *__kmalloc_node(size_t size, gfp_t flags, int node);
extern void *kmem_cache_alloc_node(struct kmem_cache *, gfp_t flags, int node);

#ifdef CONFIG_TRACING
extern void *kmem_cache_alloc_node_trace(size_t size,
					 struct kmem_cache *cachep,
					 gfp_t flags,
					 int nodeid);
#else
static __always_inline void *
kmem_cache_alloc_node_trace(size_t size,
			    struct kmem_cache *cachep,
			    gfp_t flags,
			    int nodeid)
{
	return kmem_cache_alloc_node(cachep, flags, nodeid);
}
#endif

static __always_inline void *kmalloc_node(size_t size, gfp_t flags, int node)
{
	struct kmem_cache *cachep;

	if (__builtin_constant_p(size)) {
		int i = 0;

		if (!size)
			return ZERO_SIZE_PTR;

#define CACHE(x) \
		if (size <= x) \
			goto found; \
		else \
			i++;
#include <linux/kmalloc_sizes.h>
#undef CACHE
		return NULL;
found:
#ifdef CONFIG_ZONE_DMA
		if (flags & GFP_DMA)
			cachep = malloc_sizes[i].cs_dmacachep;
		else
#endif
			cachep = malloc_sizes[i].cs_cachep;

		return kmem_cache_alloc_node_trace(size, cachep, flags, node);
	}
	return __kmalloc_node(size, flags, node);
}

#endif	

#endif	
