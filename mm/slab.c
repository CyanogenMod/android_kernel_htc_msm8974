/*
 * linux/mm/slab.c
 * Written by Mark Hemment, 1996/97.
 * (markhe@nextd.demon.co.uk)
 *
 * kmem_cache_destroy() + some cleanup - 1999 Andrea Arcangeli
 *
 * Major cleanup, different bufctl logic, per-cpu arrays
 *	(c) 2000 Manfred Spraul
 *
 * Cleanup, make the head arrays unconditional, preparation for NUMA
 * 	(c) 2002 Manfred Spraul
 *
 * An implementation of the Slab Allocator as described in outline in;
 *	UNIX Internals: The New Frontiers by Uresh Vahalia
 *	Pub: Prentice Hall	ISBN 0-13-101908-2
 * or with a little more detail in;
 *	The Slab Allocator: An Object-Caching Kernel Memory Allocator
 *	Jeff Bonwick (Sun Microsystems).
 *	Presented at: USENIX Summer 1994 Technical Conference
 *
 * The memory is organized in caches, one cache for each object type.
 * (e.g. inode_cache, dentry_cache, buffer_head, vm_area_struct)
 * Each cache consists out of many slabs (they are small (usually one
 * page long) and always contiguous), and each slab contains multiple
 * initialized objects.
 *
 * This means, that your constructor is used only for newly allocated
 * slabs and you must pass objects with the same initializations to
 * kmem_cache_free.
 *
 * Each cache can only support one memory type (GFP_DMA, GFP_HIGHMEM,
 * normal). If you need a special memory type, then must create a new
 * cache for that memory type.
 *
 * In order to reduce fragmentation, the slabs are sorted in 3 groups:
 *   full slabs with 0 free objects
 *   partial slabs
 *   empty slabs with no allocated objects
 *
 * If partial slabs exist, then new allocations come from these slabs,
 * otherwise from empty slabs or new slabs are allocated.
 *
 * kmem_cache_destroy() CAN CRASH if you try to allocate from the cache
 * during kmem_cache_destroy(). The caller must prevent concurrent allocs.
 *
 * Each cache has a short per-cpu head array, most allocs
 * and frees go into that array, and if that array overflows, then 1/2
 * of the entries in the array are given back into the global cache.
 * The head array is strictly LIFO and should improve the cache hit rates.
 * On SMP, it additionally reduces the spinlock operations.
 *
 * The c_cpuarray may not be read with enabled local interrupts -
 * it's changed with a smp_call_function().
 *
 * SMP synchronization:
 *  constructors and destructors are called without any locking.
 *  Several members in struct kmem_cache and struct slab never change, they
 *	are accessed without any locking.
 *  The per-cpu arrays are never accessed from the wrong cpu, no locking,
 *  	and local interrupts are disabled so slab code is preempt-safe.
 *  The non-constant members are protected with a per-cache irq spinlock.
 *
 * Many thanks to Mark Hemment, who wrote another per-cpu slab patch
 * in 2000 - many ideas in the current implementation are derived from
 * his patch.
 *
 * Further notes from the original documentation:
 *
 * 11 April '97.  Started multi-threading - markhe
 *	The global cache-chain is protected by the mutex 'cache_chain_mutex'.
 *	The sem is only needed when accessing/extending the cache-chain, which
 *	can never happen inside an interrupt (kmem_cache_create(),
 *	kmem_cache_shrink() and kmem_cache_reap()).
 *
 *	At present, each engine can be growing a cache.  This should be blocked.
 *
 * 15 March 2005. NUMA slab allocator.
 *	Shai Fultheim <shai@scalex86.org>.
 *	Shobhit Dayal <shobhit@calsoftinc.com>
 *	Alok N Kataria <alokk@calsoftinc.com>
 *	Christoph Lameter <christoph@lameter.com>
 *
 *	Modified the slab allocator to be node aware on NUMA systems.
 *	Each node has its own list of partial, free and full slabs.
 *	All object allocations for a node occur from node specific slab lists.
 */

#include	<linux/slab.h>
#include	<linux/mm.h>
#include	<linux/poison.h>
#include	<linux/swap.h>
#include	<linux/cache.h>
#include	<linux/interrupt.h>
#include	<linux/init.h>
#include	<linux/compiler.h>
#include	<linux/cpuset.h>
#include	<linux/proc_fs.h>
#include	<linux/seq_file.h>
#include	<linux/notifier.h>
#include	<linux/kallsyms.h>
#include	<linux/cpu.h>
#include	<linux/sysctl.h>
#include	<linux/module.h>
#include	<linux/rcupdate.h>
#include	<linux/string.h>
#include	<linux/uaccess.h>
#include	<linux/nodemask.h>
#include	<linux/kmemleak.h>
#include	<linux/mempolicy.h>
#include	<linux/mutex.h>
#include	<linux/fault-inject.h>
#include	<linux/rtmutex.h>
#include	<linux/reciprocal_div.h>
#include	<linux/debugobjects.h>
#include	<linux/kmemcheck.h>
#include	<linux/memory.h>
#include	<linux/prefetch.h>

#include	<asm/cacheflush.h>
#include	<asm/tlbflush.h>
#include	<asm/page.h>

#include <trace/events/kmem.h>


#ifdef CONFIG_DEBUG_SLAB
#define	DEBUG		1
#define	STATS		1
#define	FORCED_DEBUG	1
#else
#define	DEBUG		0
#define	STATS		0
#define	FORCED_DEBUG	0
#endif

#define	BYTES_PER_WORD		sizeof(void *)
#define	REDZONE_ALIGN		max(BYTES_PER_WORD, __alignof__(unsigned long long))

#ifndef ARCH_KMALLOC_FLAGS
#define ARCH_KMALLOC_FLAGS SLAB_HWCACHE_ALIGN
#endif

#if DEBUG
# define CREATE_MASK	(SLAB_RED_ZONE | \
			 SLAB_POISON | SLAB_HWCACHE_ALIGN | \
			 SLAB_CACHE_DMA | \
			 SLAB_STORE_USER | \
			 SLAB_RECLAIM_ACCOUNT | SLAB_PANIC | \
			 SLAB_DESTROY_BY_RCU | SLAB_MEM_SPREAD | \
			 SLAB_DEBUG_OBJECTS | SLAB_NOLEAKTRACE | SLAB_NOTRACK)
#else
# define CREATE_MASK	(SLAB_HWCACHE_ALIGN | \
			 SLAB_CACHE_DMA | \
			 SLAB_RECLAIM_ACCOUNT | SLAB_PANIC | \
			 SLAB_DESTROY_BY_RCU | SLAB_MEM_SPREAD | \
			 SLAB_DEBUG_OBJECTS | SLAB_NOLEAKTRACE | SLAB_NOTRACK)
#endif


typedef unsigned int kmem_bufctl_t;
#define BUFCTL_END	(((kmem_bufctl_t)(~0U))-0)
#define BUFCTL_FREE	(((kmem_bufctl_t)(~0U))-1)
#define	BUFCTL_ACTIVE	(((kmem_bufctl_t)(~0U))-2)
#define	SLAB_LIMIT	(((kmem_bufctl_t)(~0U))-3)

struct slab_rcu {
	struct rcu_head head;
	struct kmem_cache *cachep;
	void *addr;
};

struct slab {
	union {
		struct {
			struct list_head list;
			unsigned long colouroff;
			void *s_mem;		
			unsigned int inuse;	
			kmem_bufctl_t free;
			unsigned short nodeid;
		};
		struct slab_rcu __slab_cover_slab_rcu;
	};
};

struct array_cache {
	unsigned int avail;
	unsigned int limit;
	unsigned int batchcount;
	unsigned int touched;
	spinlock_t lock;
	void *entry[];	
};

#define BOOT_CPUCACHE_ENTRIES	1
struct arraycache_init {
	struct array_cache cache;
	void *entries[BOOT_CPUCACHE_ENTRIES];
};

struct kmem_list3 {
	struct list_head slabs_partial;	
	struct list_head slabs_full;
	struct list_head slabs_free;
	unsigned long free_objects;
	unsigned int free_limit;
	unsigned int colour_next;	
	spinlock_t list_lock;
	struct array_cache *shared;	
	struct array_cache **alien;	
	unsigned long next_reap;	
	int free_touched;		
};

#define NUM_INIT_LISTS (3 * MAX_NUMNODES)
static struct kmem_list3 __initdata initkmem_list3[NUM_INIT_LISTS];
#define	CACHE_CACHE 0
#define	SIZE_AC MAX_NUMNODES
#define	SIZE_L3 (2 * MAX_NUMNODES)

static int drain_freelist(struct kmem_cache *cache,
			struct kmem_list3 *l3, int tofree);
static void free_block(struct kmem_cache *cachep, void **objpp, int len,
			int node);
static int enable_cpucache(struct kmem_cache *cachep, gfp_t gfp);
static void cache_reap(struct work_struct *unused);

static __always_inline int index_of(const size_t size)
{
	extern void __bad_size(void);

	if (__builtin_constant_p(size)) {
		int i = 0;

#define CACHE(x) \
	if (size <=x) \
		return i; \
	else \
		i++;
#include <linux/kmalloc_sizes.h>
#undef CACHE
		__bad_size();
	} else
		__bad_size();
	return 0;
}

static int slab_early_init = 1;

#define INDEX_AC index_of(sizeof(struct arraycache_init))
#define INDEX_L3 index_of(sizeof(struct kmem_list3))

static void kmem_list3_init(struct kmem_list3 *parent)
{
	INIT_LIST_HEAD(&parent->slabs_full);
	INIT_LIST_HEAD(&parent->slabs_partial);
	INIT_LIST_HEAD(&parent->slabs_free);
	parent->shared = NULL;
	parent->alien = NULL;
	parent->colour_next = 0;
	spin_lock_init(&parent->list_lock);
	parent->free_objects = 0;
	parent->free_touched = 0;
}

#define MAKE_LIST(cachep, listp, slab, nodeid)				\
	do {								\
		INIT_LIST_HEAD(listp);					\
		list_splice(&(cachep->nodelists[nodeid]->slab), listp);	\
	} while (0)

#define	MAKE_ALL_LISTS(cachep, ptr, nodeid)				\
	do {								\
	MAKE_LIST((cachep), (&(ptr)->slabs_full), slabs_full, nodeid);	\
	MAKE_LIST((cachep), (&(ptr)->slabs_partial), slabs_partial, nodeid); \
	MAKE_LIST((cachep), (&(ptr)->slabs_free), slabs_free, nodeid);	\
	} while (0)

#define CFLGS_OFF_SLAB		(0x80000000UL)
#define	OFF_SLAB(x)	((x)->flags & CFLGS_OFF_SLAB)

#define BATCHREFILL_LIMIT	16
#define REAPTIMEOUT_CPUC	(2*HZ)
#define REAPTIMEOUT_LIST3	(4*HZ)

#if STATS
#define	STATS_INC_ACTIVE(x)	((x)->num_active++)
#define	STATS_DEC_ACTIVE(x)	((x)->num_active--)
#define	STATS_INC_ALLOCED(x)	((x)->num_allocations++)
#define	STATS_INC_GROWN(x)	((x)->grown++)
#define	STATS_ADD_REAPED(x,y)	((x)->reaped += (y))
#define	STATS_SET_HIGH(x)						\
	do {								\
		if ((x)->num_active > (x)->high_mark)			\
			(x)->high_mark = (x)->num_active;		\
	} while (0)
#define	STATS_INC_ERR(x)	((x)->errors++)
#define	STATS_INC_NODEALLOCS(x)	((x)->node_allocs++)
#define	STATS_INC_NODEFREES(x)	((x)->node_frees++)
#define STATS_INC_ACOVERFLOW(x)   ((x)->node_overflow++)
#define	STATS_SET_FREEABLE(x, i)					\
	do {								\
		if ((x)->max_freeable < i)				\
			(x)->max_freeable = i;				\
	} while (0)
#define STATS_INC_ALLOCHIT(x)	atomic_inc(&(x)->allochit)
#define STATS_INC_ALLOCMISS(x)	atomic_inc(&(x)->allocmiss)
#define STATS_INC_FREEHIT(x)	atomic_inc(&(x)->freehit)
#define STATS_INC_FREEMISS(x)	atomic_inc(&(x)->freemiss)
#else
#define	STATS_INC_ACTIVE(x)	do { } while (0)
#define	STATS_DEC_ACTIVE(x)	do { } while (0)
#define	STATS_INC_ALLOCED(x)	do { } while (0)
#define	STATS_INC_GROWN(x)	do { } while (0)
#define	STATS_ADD_REAPED(x,y)	do { (void)(y); } while (0)
#define	STATS_SET_HIGH(x)	do { } while (0)
#define	STATS_INC_ERR(x)	do { } while (0)
#define	STATS_INC_NODEALLOCS(x)	do { } while (0)
#define	STATS_INC_NODEFREES(x)	do { } while (0)
#define STATS_INC_ACOVERFLOW(x)   do { } while (0)
#define	STATS_SET_FREEABLE(x, i) do { } while (0)
#define STATS_INC_ALLOCHIT(x)	do { } while (0)
#define STATS_INC_ALLOCMISS(x)	do { } while (0)
#define STATS_INC_FREEHIT(x)	do { } while (0)
#define STATS_INC_FREEMISS(x)	do { } while (0)
#endif

#if DEBUG

static int obj_offset(struct kmem_cache *cachep)
{
	return cachep->obj_offset;
}

static int obj_size(struct kmem_cache *cachep)
{
	return cachep->obj_size;
}

static unsigned long long *dbg_redzone1(struct kmem_cache *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_RED_ZONE));
	return (unsigned long long*) (objp + obj_offset(cachep) -
				      sizeof(unsigned long long));
}

static unsigned long long *dbg_redzone2(struct kmem_cache *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_RED_ZONE));
	if (cachep->flags & SLAB_STORE_USER)
		return (unsigned long long *)(objp + cachep->buffer_size -
					      sizeof(unsigned long long) -
					      REDZONE_ALIGN);
	return (unsigned long long *) (objp + cachep->buffer_size -
				       sizeof(unsigned long long));
}

static void **dbg_userword(struct kmem_cache *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_STORE_USER));
	return (void **)(objp + cachep->buffer_size - BYTES_PER_WORD);
}

#else

#define obj_offset(x)			0
#define obj_size(cachep)		(cachep->buffer_size)
#define dbg_redzone1(cachep, objp)	({BUG(); (unsigned long long *)NULL;})
#define dbg_redzone2(cachep, objp)	({BUG(); (unsigned long long *)NULL;})
#define dbg_userword(cachep, objp)	({BUG(); (void **)NULL;})

#endif

#ifdef CONFIG_TRACING
size_t slab_buffer_size(struct kmem_cache *cachep)
{
	return cachep->buffer_size;
}
EXPORT_SYMBOL(slab_buffer_size);
#endif

#define	SLAB_MAX_ORDER_HI	1
#define	SLAB_MAX_ORDER_LO	0
static int slab_max_order = SLAB_MAX_ORDER_LO;
static bool slab_max_order_set __initdata;

static inline void page_set_cache(struct page *page, struct kmem_cache *cache)
{
	page->lru.next = (struct list_head *)cache;
}

static inline struct kmem_cache *page_get_cache(struct page *page)
{
	page = compound_head(page);
	BUG_ON(!PageSlab(page));
	return (struct kmem_cache *)page->lru.next;
}

static inline void page_set_slab(struct page *page, struct slab *slab)
{
	page->lru.prev = (struct list_head *)slab;
}

static inline struct slab *page_get_slab(struct page *page)
{
	BUG_ON(!PageSlab(page));
	return (struct slab *)page->lru.prev;
}

static inline struct kmem_cache *virt_to_cache(const void *obj)
{
	struct page *page = virt_to_head_page(obj);
	return page_get_cache(page);
}

static inline struct slab *virt_to_slab(const void *obj)
{
	struct page *page = virt_to_head_page(obj);
	return page_get_slab(page);
}

static inline void *index_to_obj(struct kmem_cache *cache, struct slab *slab,
				 unsigned int idx)
{
	return slab->s_mem + cache->buffer_size * idx;
}

static inline unsigned int obj_to_index(const struct kmem_cache *cache,
					const struct slab *slab, void *obj)
{
	u32 offset = (obj - slab->s_mem);
	return reciprocal_divide(offset, cache->reciprocal_buffer_size);
}

struct cache_sizes malloc_sizes[] = {
#define CACHE(x) { .cs_size = (x) },
#include <linux/kmalloc_sizes.h>
	CACHE(ULONG_MAX)
#undef CACHE
};
EXPORT_SYMBOL(malloc_sizes);

struct cache_names {
	char *name;
	char *name_dma;
};

static struct cache_names __initdata cache_names[] = {
#define CACHE(x) { .name = "size-" #x, .name_dma = "size-" #x "(DMA)" },
#include <linux/kmalloc_sizes.h>
	{NULL,}
#undef CACHE
};

static struct arraycache_init initarray_cache __initdata =
    { {0, BOOT_CPUCACHE_ENTRIES, 1, 0} };
static struct arraycache_init initarray_generic =
    { {0, BOOT_CPUCACHE_ENTRIES, 1, 0} };

static struct kmem_list3 *cache_cache_nodelists[MAX_NUMNODES];
static struct kmem_cache cache_cache = {
	.nodelists = cache_cache_nodelists,
	.batchcount = 1,
	.limit = BOOT_CPUCACHE_ENTRIES,
	.shared = 1,
	.buffer_size = sizeof(struct kmem_cache),
	.name = "kmem_cache",
};

#define BAD_ALIEN_MAGIC 0x01020304ul

static enum {
	NONE,
	PARTIAL_AC,
	PARTIAL_L3,
	EARLY,
	LATE,
	FULL
} g_cpucache_up;

int slab_is_available(void)
{
	return g_cpucache_up >= EARLY;
}

#ifdef CONFIG_LOCKDEP

static struct lock_class_key on_slab_l3_key;
static struct lock_class_key on_slab_alc_key;

static struct lock_class_key debugobj_l3_key;
static struct lock_class_key debugobj_alc_key;

static void slab_set_lock_classes(struct kmem_cache *cachep,
		struct lock_class_key *l3_key, struct lock_class_key *alc_key,
		int q)
{
	struct array_cache **alc;
	struct kmem_list3 *l3;
	int r;

	l3 = cachep->nodelists[q];
	if (!l3)
		return;

	lockdep_set_class(&l3->list_lock, l3_key);
	alc = l3->alien;
	if (!alc || (unsigned long)alc == BAD_ALIEN_MAGIC)
		return;
	for_each_node(r) {
		if (alc[r])
			lockdep_set_class(&alc[r]->lock, alc_key);
	}
}

static void slab_set_debugobj_lock_classes_node(struct kmem_cache *cachep, int node)
{
	slab_set_lock_classes(cachep, &debugobj_l3_key, &debugobj_alc_key, node);
}

static void slab_set_debugobj_lock_classes(struct kmem_cache *cachep)
{
	int node;

	for_each_online_node(node)
		slab_set_debugobj_lock_classes_node(cachep, node);
}

static void init_node_lock_keys(int q)
{
	struct cache_sizes *s = malloc_sizes;

	if (g_cpucache_up < LATE)
		return;

	for (s = malloc_sizes; s->cs_size != ULONG_MAX; s++) {
		struct kmem_list3 *l3;

		l3 = s->cs_cachep->nodelists[q];
		if (!l3 || OFF_SLAB(s->cs_cachep))
			continue;

		slab_set_lock_classes(s->cs_cachep, &on_slab_l3_key,
				&on_slab_alc_key, q);
	}
}

static inline void init_lock_keys(void)
{
	int node;

	for_each_node(node)
		init_node_lock_keys(node);
}
#else
static void init_node_lock_keys(int q)
{
}

static inline void init_lock_keys(void)
{
}

static void slab_set_debugobj_lock_classes_node(struct kmem_cache *cachep, int node)
{
}

static void slab_set_debugobj_lock_classes(struct kmem_cache *cachep)
{
}
#endif

static DEFINE_MUTEX(cache_chain_mutex);
static struct list_head cache_chain;

static DEFINE_PER_CPU(struct delayed_work, slab_reap_work);

static inline struct array_cache *cpu_cache_get(struct kmem_cache *cachep)
{
	return cachep->array[smp_processor_id()];
}

static inline struct kmem_cache *__find_general_cachep(size_t size,
							gfp_t gfpflags)
{
	struct cache_sizes *csizep = malloc_sizes;

#if DEBUG
	BUG_ON(malloc_sizes[INDEX_AC].cs_cachep == NULL);
#endif
	if (!size)
		return ZERO_SIZE_PTR;

	while (size > csizep->cs_size)
		csizep++;

#ifdef CONFIG_ZONE_DMA
	if (unlikely(gfpflags & GFP_DMA))
		return csizep->cs_dmacachep;
#endif
	return csizep->cs_cachep;
}

static struct kmem_cache *kmem_find_general_cachep(size_t size, gfp_t gfpflags)
{
	return __find_general_cachep(size, gfpflags);
}

static size_t slab_mgmt_size(size_t nr_objs, size_t align)
{
	return ALIGN(sizeof(struct slab)+nr_objs*sizeof(kmem_bufctl_t), align);
}

static void cache_estimate(unsigned long gfporder, size_t buffer_size,
			   size_t align, int flags, size_t *left_over,
			   unsigned int *num)
{
	int nr_objs;
	size_t mgmt_size;
	size_t slab_size = PAGE_SIZE << gfporder;

	if (flags & CFLGS_OFF_SLAB) {
		mgmt_size = 0;
		nr_objs = slab_size / buffer_size;

		if (nr_objs > SLAB_LIMIT)
			nr_objs = SLAB_LIMIT;
	} else {
		nr_objs = (slab_size - sizeof(struct slab)) /
			  (buffer_size + sizeof(kmem_bufctl_t));

		if (slab_mgmt_size(nr_objs, align) + nr_objs*buffer_size
		       > slab_size)
			nr_objs--;

		if (nr_objs > SLAB_LIMIT)
			nr_objs = SLAB_LIMIT;

		mgmt_size = slab_mgmt_size(nr_objs, align);
	}
	*num = nr_objs;
	*left_over = slab_size - nr_objs*buffer_size - mgmt_size;
}

#define slab_error(cachep, msg) __slab_error(__func__, cachep, msg)

static void __slab_error(const char *function, struct kmem_cache *cachep,
			char *msg)
{
	printk(KERN_ERR "slab error in %s(): cache `%s': %s\n",
	       function, cachep->name, msg);
	dump_stack();
}


static int use_alien_caches __read_mostly = 1;
static int __init noaliencache_setup(char *s)
{
	use_alien_caches = 0;
	return 1;
}
__setup("noaliencache", noaliencache_setup);

static int __init slab_max_order_setup(char *str)
{
	get_option(&str, &slab_max_order);
	slab_max_order = slab_max_order < 0 ? 0 :
				min(slab_max_order, MAX_ORDER - 1);
	slab_max_order_set = true;

	return 1;
}
__setup("slab_max_order=", slab_max_order_setup);

#ifdef CONFIG_NUMA
static DEFINE_PER_CPU(unsigned long, slab_reap_node);

static void init_reap_node(int cpu)
{
	int node;

	node = next_node(cpu_to_mem(cpu), node_online_map);
	if (node == MAX_NUMNODES)
		node = first_node(node_online_map);

	per_cpu(slab_reap_node, cpu) = node;
}

static void next_reap_node(void)
{
	int node = __this_cpu_read(slab_reap_node);

	node = next_node(node, node_online_map);
	if (unlikely(node >= MAX_NUMNODES))
		node = first_node(node_online_map);
	__this_cpu_write(slab_reap_node, node);
}

#else
#define init_reap_node(cpu) do { } while (0)
#define next_reap_node(void) do { } while (0)
#endif

static void __cpuinit start_cpu_timer(int cpu)
{
	struct delayed_work *reap_work = &per_cpu(slab_reap_work, cpu);

	if (keventd_up() && reap_work->work.func == NULL) {
		init_reap_node(cpu);
		INIT_DELAYED_WORK_DEFERRABLE(reap_work, cache_reap);
		schedule_delayed_work_on(cpu, reap_work,
					__round_jiffies_relative(HZ, cpu));
	}
}

static struct array_cache *alloc_arraycache(int node, int entries,
					    int batchcount, gfp_t gfp)
{
	int memsize = sizeof(void *) * entries + sizeof(struct array_cache);
	struct array_cache *nc = NULL;

	nc = kmalloc_node(memsize, gfp, node);
	kmemleak_no_scan(nc);
	if (nc) {
		nc->avail = 0;
		nc->limit = entries;
		nc->batchcount = batchcount;
		nc->touched = 0;
		spin_lock_init(&nc->lock);
	}
	return nc;
}

static int transfer_objects(struct array_cache *to,
		struct array_cache *from, unsigned int max)
{
	
	int nr = min3(from->avail, max, to->limit - to->avail);

	if (!nr)
		return 0;

	memcpy(to->entry + to->avail, from->entry + from->avail -nr,
			sizeof(void *) *nr);

	from->avail -= nr;
	to->avail += nr;
	return nr;
}

#ifndef CONFIG_NUMA

#define drain_alien_cache(cachep, alien) do { } while (0)
#define reap_alien(cachep, l3) do { } while (0)

static inline struct array_cache **alloc_alien_cache(int node, int limit, gfp_t gfp)
{
	return (struct array_cache **)BAD_ALIEN_MAGIC;
}

static inline void free_alien_cache(struct array_cache **ac_ptr)
{
}

static inline int cache_free_alien(struct kmem_cache *cachep, void *objp)
{
	return 0;
}

static inline void *alternate_node_alloc(struct kmem_cache *cachep,
		gfp_t flags)
{
	return NULL;
}

static inline void *____cache_alloc_node(struct kmem_cache *cachep,
		 gfp_t flags, int nodeid)
{
	return NULL;
}

#else	

static void *____cache_alloc_node(struct kmem_cache *, gfp_t, int);
static void *alternate_node_alloc(struct kmem_cache *, gfp_t);

static struct array_cache **alloc_alien_cache(int node, int limit, gfp_t gfp)
{
	struct array_cache **ac_ptr;
	int memsize = sizeof(void *) * nr_node_ids;
	int i;

	if (limit > 1)
		limit = 12;
	ac_ptr = kzalloc_node(memsize, gfp, node);
	if (ac_ptr) {
		for_each_node(i) {
			if (i == node || !node_online(i))
				continue;
			ac_ptr[i] = alloc_arraycache(node, limit, 0xbaadf00d, gfp);
			if (!ac_ptr[i]) {
				for (i--; i >= 0; i--)
					kfree(ac_ptr[i]);
				kfree(ac_ptr);
				return NULL;
			}
		}
	}
	return ac_ptr;
}

static void free_alien_cache(struct array_cache **ac_ptr)
{
	int i;

	if (!ac_ptr)
		return;
	for_each_node(i)
	    kfree(ac_ptr[i]);
	kfree(ac_ptr);
}

static void __drain_alien_cache(struct kmem_cache *cachep,
				struct array_cache *ac, int node)
{
	struct kmem_list3 *rl3 = cachep->nodelists[node];

	if (ac->avail) {
		spin_lock(&rl3->list_lock);
		if (rl3->shared)
			transfer_objects(rl3->shared, ac, ac->limit);

		free_block(cachep, ac->entry, ac->avail, node);
		ac->avail = 0;
		spin_unlock(&rl3->list_lock);
	}
}

static void reap_alien(struct kmem_cache *cachep, struct kmem_list3 *l3)
{
	int node = __this_cpu_read(slab_reap_node);

	if (l3->alien) {
		struct array_cache *ac = l3->alien[node];

		if (ac && ac->avail && spin_trylock_irq(&ac->lock)) {
			__drain_alien_cache(cachep, ac, node);
			spin_unlock_irq(&ac->lock);
		}
	}
}

static void drain_alien_cache(struct kmem_cache *cachep,
				struct array_cache **alien)
{
	int i = 0;
	struct array_cache *ac;
	unsigned long flags;

	for_each_online_node(i) {
		ac = alien[i];
		if (ac) {
			spin_lock_irqsave(&ac->lock, flags);
			__drain_alien_cache(cachep, ac, i);
			spin_unlock_irqrestore(&ac->lock, flags);
		}
	}
}

static inline int cache_free_alien(struct kmem_cache *cachep, void *objp)
{
	struct slab *slabp = virt_to_slab(objp);
	int nodeid = slabp->nodeid;
	struct kmem_list3 *l3;
	struct array_cache *alien = NULL;
	int node;

	node = numa_mem_id();

	if (likely(slabp->nodeid == node))
		return 0;

	l3 = cachep->nodelists[node];
	STATS_INC_NODEFREES(cachep);
	if (l3->alien && l3->alien[nodeid]) {
		alien = l3->alien[nodeid];
		spin_lock(&alien->lock);
		if (unlikely(alien->avail == alien->limit)) {
			STATS_INC_ACOVERFLOW(cachep);
			__drain_alien_cache(cachep, alien, nodeid);
		}
		alien->entry[alien->avail++] = objp;
		spin_unlock(&alien->lock);
	} else {
		spin_lock(&(cachep->nodelists[nodeid])->list_lock);
		free_block(cachep, &objp, 1, nodeid);
		spin_unlock(&(cachep->nodelists[nodeid])->list_lock);
	}
	return 1;
}
#endif

static int init_cache_nodelists_node(int node)
{
	struct kmem_cache *cachep;
	struct kmem_list3 *l3;
	const int memsize = sizeof(struct kmem_list3);

	list_for_each_entry(cachep, &cache_chain, next) {
		if (!cachep->nodelists[node]) {
			l3 = kmalloc_node(memsize, GFP_KERNEL, node);
			if (!l3)
				return -ENOMEM;
			kmem_list3_init(l3);
			l3->next_reap = jiffies + REAPTIMEOUT_LIST3 +
			    ((unsigned long)cachep) % REAPTIMEOUT_LIST3;

			cachep->nodelists[node] = l3;
		}

		spin_lock_irq(&cachep->nodelists[node]->list_lock);
		cachep->nodelists[node]->free_limit =
			(1 + nr_cpus_node(node)) *
			cachep->batchcount + cachep->num;
		spin_unlock_irq(&cachep->nodelists[node]->list_lock);
	}
	return 0;
}

static void __cpuinit cpuup_canceled(long cpu)
{
	struct kmem_cache *cachep;
	struct kmem_list3 *l3 = NULL;
	int node = cpu_to_mem(cpu);
	const struct cpumask *mask = cpumask_of_node(node);

	list_for_each_entry(cachep, &cache_chain, next) {
		struct array_cache *nc;
		struct array_cache *shared;
		struct array_cache **alien;

		
		nc = cachep->array[cpu];
		cachep->array[cpu] = NULL;
		l3 = cachep->nodelists[node];

		if (!l3)
			goto free_array_cache;

		spin_lock_irq(&l3->list_lock);

		
		l3->free_limit -= cachep->batchcount;
		if (nc)
			free_block(cachep, nc->entry, nc->avail, node);

		if (!cpumask_empty(mask)) {
			spin_unlock_irq(&l3->list_lock);
			goto free_array_cache;
		}

		shared = l3->shared;
		if (shared) {
			free_block(cachep, shared->entry,
				   shared->avail, node);
			l3->shared = NULL;
		}

		alien = l3->alien;
		l3->alien = NULL;

		spin_unlock_irq(&l3->list_lock);

		kfree(shared);
		if (alien) {
			drain_alien_cache(cachep, alien);
			free_alien_cache(alien);
		}
free_array_cache:
		kfree(nc);
	}
	list_for_each_entry(cachep, &cache_chain, next) {
		l3 = cachep->nodelists[node];
		if (!l3)
			continue;
		drain_freelist(cachep, l3, l3->free_objects);
	}
}

static int __cpuinit cpuup_prepare(long cpu)
{
	struct kmem_cache *cachep;
	struct kmem_list3 *l3 = NULL;
	int node = cpu_to_mem(cpu);
	int err;

	err = init_cache_nodelists_node(node);
	if (err < 0)
		goto bad;

	list_for_each_entry(cachep, &cache_chain, next) {
		struct array_cache *nc;
		struct array_cache *shared = NULL;
		struct array_cache **alien = NULL;

		nc = alloc_arraycache(node, cachep->limit,
					cachep->batchcount, GFP_KERNEL);
		if (!nc)
			goto bad;
		if (cachep->shared) {
			shared = alloc_arraycache(node,
				cachep->shared * cachep->batchcount,
				0xbaadf00d, GFP_KERNEL);
			if (!shared) {
				kfree(nc);
				goto bad;
			}
		}
		if (use_alien_caches) {
			alien = alloc_alien_cache(node, cachep->limit, GFP_KERNEL);
			if (!alien) {
				kfree(shared);
				kfree(nc);
				goto bad;
			}
		}
		cachep->array[cpu] = nc;
		l3 = cachep->nodelists[node];
		BUG_ON(!l3);

		spin_lock_irq(&l3->list_lock);
		if (!l3->shared) {
			l3->shared = shared;
			shared = NULL;
		}
#ifdef CONFIG_NUMA
		if (!l3->alien) {
			l3->alien = alien;
			alien = NULL;
		}
#endif
		spin_unlock_irq(&l3->list_lock);
		kfree(shared);
		free_alien_cache(alien);
		if (cachep->flags & SLAB_DEBUG_OBJECTS)
			slab_set_debugobj_lock_classes_node(cachep, node);
	}
	init_node_lock_keys(node);

	return 0;
bad:
	cpuup_canceled(cpu);
	return -ENOMEM;
}

static int __cpuinit cpuup_callback(struct notifier_block *nfb,
				    unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
	int err = 0;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		mutex_lock(&cache_chain_mutex);
		err = cpuup_prepare(cpu);
		mutex_unlock(&cache_chain_mutex);
		break;
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		start_cpu_timer(cpu);
		break;
#ifdef CONFIG_HOTPLUG_CPU
  	case CPU_DOWN_PREPARE:
  	case CPU_DOWN_PREPARE_FROZEN:
		cancel_delayed_work_sync(&per_cpu(slab_reap_work, cpu));
		
		per_cpu(slab_reap_work, cpu).work.func = NULL;
  		break;
  	case CPU_DOWN_FAILED:
  	case CPU_DOWN_FAILED_FROZEN:
		start_cpu_timer(cpu);
  		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		
#endif
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
		mutex_lock(&cache_chain_mutex);
		cpuup_canceled(cpu);
		mutex_unlock(&cache_chain_mutex);
		break;
	}
	return notifier_from_errno(err);
}

static struct notifier_block __cpuinitdata cpucache_notifier = {
	&cpuup_callback, NULL, 0
};

#if defined(CONFIG_NUMA) && defined(CONFIG_MEMORY_HOTPLUG)
static int __meminit drain_cache_nodelists_node(int node)
{
	struct kmem_cache *cachep;
	int ret = 0;

	list_for_each_entry(cachep, &cache_chain, next) {
		struct kmem_list3 *l3;

		l3 = cachep->nodelists[node];
		if (!l3)
			continue;

		drain_freelist(cachep, l3, l3->free_objects);

		if (!list_empty(&l3->slabs_full) ||
		    !list_empty(&l3->slabs_partial)) {
			ret = -EBUSY;
			break;
		}
	}
	return ret;
}

static int __meminit slab_memory_callback(struct notifier_block *self,
					unsigned long action, void *arg)
{
	struct memory_notify *mnb = arg;
	int ret = 0;
	int nid;

	nid = mnb->status_change_nid;
	if (nid < 0)
		goto out;

	switch (action) {
	case MEM_GOING_ONLINE:
		mutex_lock(&cache_chain_mutex);
		ret = init_cache_nodelists_node(nid);
		mutex_unlock(&cache_chain_mutex);
		break;
	case MEM_GOING_OFFLINE:
		mutex_lock(&cache_chain_mutex);
		ret = drain_cache_nodelists_node(nid);
		mutex_unlock(&cache_chain_mutex);
		break;
	case MEM_ONLINE:
	case MEM_OFFLINE:
	case MEM_CANCEL_ONLINE:
	case MEM_CANCEL_OFFLINE:
		break;
	}
out:
	return notifier_from_errno(ret);
}
#endif 

static void __init init_list(struct kmem_cache *cachep, struct kmem_list3 *list,
				int nodeid)
{
	struct kmem_list3 *ptr;

	ptr = kmalloc_node(sizeof(struct kmem_list3), GFP_NOWAIT, nodeid);
	BUG_ON(!ptr);

	memcpy(ptr, list, sizeof(struct kmem_list3));
	spin_lock_init(&ptr->list_lock);

	MAKE_ALL_LISTS(cachep, ptr, nodeid);
	cachep->nodelists[nodeid] = ptr;
}

static void __init set_up_list3s(struct kmem_cache *cachep, int index)
{
	int node;

	for_each_online_node(node) {
		cachep->nodelists[node] = &initkmem_list3[index + node];
		cachep->nodelists[node]->next_reap = jiffies +
		    REAPTIMEOUT_LIST3 +
		    ((unsigned long)cachep) % REAPTIMEOUT_LIST3;
	}
}

void __init kmem_cache_init(void)
{
	size_t left_over;
	struct cache_sizes *sizes;
	struct cache_names *names;
	int i;
	int order;
	int node;

	if (num_possible_nodes() == 1)
		use_alien_caches = 0;

	for (i = 0; i < NUM_INIT_LISTS; i++) {
		kmem_list3_init(&initkmem_list3[i]);
		if (i < MAX_NUMNODES)
			cache_cache.nodelists[i] = NULL;
	}
	set_up_list3s(&cache_cache, CACHE_CACHE);

	if (!slab_max_order_set && totalram_pages > (32 << 20) >> PAGE_SHIFT)
		slab_max_order = SLAB_MAX_ORDER_HI;


	node = numa_mem_id();

	
	INIT_LIST_HEAD(&cache_chain);
	list_add(&cache_cache.next, &cache_chain);
	cache_cache.colour_off = cache_line_size();
	cache_cache.array[smp_processor_id()] = &initarray_cache.cache;
	cache_cache.nodelists[node] = &initkmem_list3[CACHE_CACHE + node];

	cache_cache.buffer_size = offsetof(struct kmem_cache, array[nr_cpu_ids]) +
				  nr_node_ids * sizeof(struct kmem_list3 *);
#if DEBUG
	cache_cache.obj_size = cache_cache.buffer_size;
#endif
	cache_cache.buffer_size = ALIGN(cache_cache.buffer_size,
					cache_line_size());
	cache_cache.reciprocal_buffer_size =
		reciprocal_value(cache_cache.buffer_size);

	for (order = 0; order < MAX_ORDER; order++) {
		cache_estimate(order, cache_cache.buffer_size,
			cache_line_size(), 0, &left_over, &cache_cache.num);
		if (cache_cache.num)
			break;
	}
	BUG_ON(!cache_cache.num);
	cache_cache.gfporder = order;
	cache_cache.colour = left_over / cache_cache.colour_off;
	cache_cache.slab_size = ALIGN(cache_cache.num * sizeof(kmem_bufctl_t) +
				      sizeof(struct slab), cache_line_size());

	
	sizes = malloc_sizes;
	names = cache_names;


	sizes[INDEX_AC].cs_cachep = kmem_cache_create(names[INDEX_AC].name,
					sizes[INDEX_AC].cs_size,
					ARCH_KMALLOC_MINALIGN,
					ARCH_KMALLOC_FLAGS|SLAB_PANIC,
					NULL);

	if (INDEX_AC != INDEX_L3) {
		sizes[INDEX_L3].cs_cachep =
			kmem_cache_create(names[INDEX_L3].name,
				sizes[INDEX_L3].cs_size,
				ARCH_KMALLOC_MINALIGN,
				ARCH_KMALLOC_FLAGS|SLAB_PANIC,
				NULL);
	}

	slab_early_init = 0;

	while (sizes->cs_size != ULONG_MAX) {
		if (!sizes->cs_cachep) {
			sizes->cs_cachep = kmem_cache_create(names->name,
					sizes->cs_size,
					ARCH_KMALLOC_MINALIGN,
					ARCH_KMALLOC_FLAGS|SLAB_PANIC,
					NULL);
		}
#ifdef CONFIG_ZONE_DMA
		sizes->cs_dmacachep = kmem_cache_create(
					names->name_dma,
					sizes->cs_size,
					ARCH_KMALLOC_MINALIGN,
					ARCH_KMALLOC_FLAGS|SLAB_CACHE_DMA|
						SLAB_PANIC,
					NULL);
#endif
		sizes++;
		names++;
	}
	
	{
		struct array_cache *ptr;

		ptr = kmalloc(sizeof(struct arraycache_init), GFP_NOWAIT);

		BUG_ON(cpu_cache_get(&cache_cache) != &initarray_cache.cache);
		memcpy(ptr, cpu_cache_get(&cache_cache),
		       sizeof(struct arraycache_init));
		spin_lock_init(&ptr->lock);

		cache_cache.array[smp_processor_id()] = ptr;

		ptr = kmalloc(sizeof(struct arraycache_init), GFP_NOWAIT);

		BUG_ON(cpu_cache_get(malloc_sizes[INDEX_AC].cs_cachep)
		       != &initarray_generic.cache);
		memcpy(ptr, cpu_cache_get(malloc_sizes[INDEX_AC].cs_cachep),
		       sizeof(struct arraycache_init));
		spin_lock_init(&ptr->lock);

		malloc_sizes[INDEX_AC].cs_cachep->array[smp_processor_id()] =
		    ptr;
	}
	
	{
		int nid;

		for_each_online_node(nid) {
			init_list(&cache_cache, &initkmem_list3[CACHE_CACHE + nid], nid);

			init_list(malloc_sizes[INDEX_AC].cs_cachep,
				  &initkmem_list3[SIZE_AC + nid], nid);

			if (INDEX_AC != INDEX_L3) {
				init_list(malloc_sizes[INDEX_L3].cs_cachep,
					  &initkmem_list3[SIZE_L3 + nid], nid);
			}
		}
	}

	g_cpucache_up = EARLY;
}

void __init kmem_cache_init_late(void)
{
	struct kmem_cache *cachep;

	g_cpucache_up = LATE;

	
	init_lock_keys();

	
	mutex_lock(&cache_chain_mutex);
	list_for_each_entry(cachep, &cache_chain, next)
		if (enable_cpucache(cachep, GFP_NOWAIT))
			BUG();
	mutex_unlock(&cache_chain_mutex);

	
	g_cpucache_up = FULL;

	register_cpu_notifier(&cpucache_notifier);

#ifdef CONFIG_NUMA
	hotplug_memory_notifier(slab_memory_callback, SLAB_CALLBACK_PRI);
#endif

}

static int __init cpucache_init(void)
{
	int cpu;

	for_each_online_cpu(cpu)
		start_cpu_timer(cpu);
	return 0;
}
__initcall(cpucache_init);

static noinline void
slab_out_of_memory(struct kmem_cache *cachep, gfp_t gfpflags, int nodeid)
{
	struct kmem_list3 *l3;
	struct slab *slabp;
	unsigned long flags;
	int node;

	printk(KERN_WARNING
		"SLAB: Unable to allocate memory on node %d (gfp=0x%x)\n",
		nodeid, gfpflags);
	printk(KERN_WARNING "  cache: %s, object size: %d, order: %d\n",
		cachep->name, cachep->buffer_size, cachep->gfporder);

	for_each_online_node(node) {
		unsigned long active_objs = 0, num_objs = 0, free_objects = 0;
		unsigned long active_slabs = 0, num_slabs = 0;

		l3 = cachep->nodelists[node];
		if (!l3)
			continue;

		spin_lock_irqsave(&l3->list_lock, flags);
		list_for_each_entry(slabp, &l3->slabs_full, list) {
			active_objs += cachep->num;
			active_slabs++;
		}
		list_for_each_entry(slabp, &l3->slabs_partial, list) {
			active_objs += slabp->inuse;
			active_slabs++;
		}
		list_for_each_entry(slabp, &l3->slabs_free, list)
			num_slabs++;

		free_objects += l3->free_objects;
		spin_unlock_irqrestore(&l3->list_lock, flags);

		num_slabs += active_slabs;
		num_objs = num_slabs * cachep->num;
		printk(KERN_WARNING
			"  node %d: slabs: %ld/%ld, objs: %ld/%ld, free: %ld\n",
			node, active_slabs, num_slabs, active_objs, num_objs,
			free_objects);
	}
}

static void *kmem_getpages(struct kmem_cache *cachep, gfp_t flags, int nodeid)
{
	struct page *page;
	int nr_pages;
	int i;

#ifndef CONFIG_MMU
	flags |= __GFP_COMP;
#endif

	flags |= cachep->gfpflags;
	if (cachep->flags & SLAB_RECLAIM_ACCOUNT)
		flags |= __GFP_RECLAIMABLE;

	page = alloc_pages_exact_node(nodeid, flags | __GFP_NOTRACK, cachep->gfporder);
	if (!page) {
		if (!(flags & __GFP_NOWARN) && printk_ratelimit())
			slab_out_of_memory(cachep, flags, nodeid);
		return NULL;
	}

	nr_pages = (1 << cachep->gfporder);
	if (cachep->flags & SLAB_RECLAIM_ACCOUNT)
		add_zone_page_state(page_zone(page),
			NR_SLAB_RECLAIMABLE, nr_pages);
	else
		add_zone_page_state(page_zone(page),
			NR_SLAB_UNRECLAIMABLE, nr_pages);
	for (i = 0; i < nr_pages; i++)
		__SetPageSlab(page + i);

	if (kmemcheck_enabled && !(cachep->flags & SLAB_NOTRACK)) {
		kmemcheck_alloc_shadow(page, cachep->gfporder, flags, nodeid);

		if (cachep->ctor)
			kmemcheck_mark_uninitialized_pages(page, nr_pages);
		else
			kmemcheck_mark_unallocated_pages(page, nr_pages);
	}

	return page_address(page);
}

static void kmem_freepages(struct kmem_cache *cachep, void *addr)
{
	unsigned long i = (1 << cachep->gfporder);
	struct page *page = virt_to_page(addr);
	const unsigned long nr_freed = i;

	kmemcheck_free_shadow(page, cachep->gfporder);

	if (cachep->flags & SLAB_RECLAIM_ACCOUNT)
		sub_zone_page_state(page_zone(page),
				NR_SLAB_RECLAIMABLE, nr_freed);
	else
		sub_zone_page_state(page_zone(page),
				NR_SLAB_UNRECLAIMABLE, nr_freed);
	while (i--) {
		BUG_ON(!PageSlab(page));
		__ClearPageSlab(page);
		page++;
	}
	if (current->reclaim_state)
		current->reclaim_state->reclaimed_slab += nr_freed;
	free_pages((unsigned long)addr, cachep->gfporder);
}

static void kmem_rcu_free(struct rcu_head *head)
{
	struct slab_rcu *slab_rcu = (struct slab_rcu *)head;
	struct kmem_cache *cachep = slab_rcu->cachep;

	kmem_freepages(cachep, slab_rcu->addr);
	if (OFF_SLAB(cachep))
		kmem_cache_free(cachep->slabp_cache, slab_rcu);
}

#if DEBUG

#ifdef CONFIG_DEBUG_PAGEALLOC
static void store_stackinfo(struct kmem_cache *cachep, unsigned long *addr,
			    unsigned long caller)
{
	int size = obj_size(cachep);

	addr = (unsigned long *)&((char *)addr)[obj_offset(cachep)];

	if (size < 5 * sizeof(unsigned long))
		return;

	*addr++ = 0x12345678;
	*addr++ = caller;
	*addr++ = smp_processor_id();
	size -= 3 * sizeof(unsigned long);
	{
		unsigned long *sptr = &caller;
		unsigned long svalue;

		while (!kstack_end(sptr)) {
			svalue = *sptr++;
			if (kernel_text_address(svalue)) {
				*addr++ = svalue;
				size -= sizeof(unsigned long);
				if (size <= sizeof(unsigned long))
					break;
			}
		}

	}
	*addr++ = 0x87654321;
}
#endif

static void poison_obj(struct kmem_cache *cachep, void *addr, unsigned char val)
{
	int size = obj_size(cachep);
	addr = &((char *)addr)[obj_offset(cachep)];

	memset(addr, val, size);
	*(unsigned char *)(addr + size - 1) = POISON_END;
}

static void dump_line(char *data, int offset, int limit)
{
	int i;
	unsigned char error = 0;
	int bad_count = 0;

	printk(KERN_ERR "%03x: ", offset);
	for (i = 0; i < limit; i++) {
		if (data[offset + i] != POISON_FREE) {
			error = data[offset + i];
			bad_count++;
		}
	}
	print_hex_dump(KERN_CONT, "", 0, 16, 1,
			&data[offset], limit, 1);

	if (bad_count == 1) {
		error ^= POISON_FREE;
		if (!(error & (error - 1))) {
			printk(KERN_ERR "Single bit error detected. Probably "
					"bad RAM.\n");
#ifdef CONFIG_X86
			printk(KERN_ERR "Run memtest86+ or a similar memory "
					"test tool.\n");
#else
			printk(KERN_ERR "Run a memory test tool.\n");
#endif
		}
	}
}
#endif

#if DEBUG

static void print_objinfo(struct kmem_cache *cachep, void *objp, int lines)
{
	int i, size;
	char *realobj;

	if (cachep->flags & SLAB_RED_ZONE) {
		printk(KERN_ERR "Redzone: 0x%llx/0x%llx.\n",
			*dbg_redzone1(cachep, objp),
			*dbg_redzone2(cachep, objp));
	}

	if (cachep->flags & SLAB_STORE_USER) {
		printk(KERN_ERR "Last user: [<%p>]",
			*dbg_userword(cachep, objp));
		print_symbol("(%s)",
				(unsigned long)*dbg_userword(cachep, objp));
		printk("\n");
	}
	realobj = (char *)objp + obj_offset(cachep);
	size = obj_size(cachep);
	for (i = 0; i < size && lines; i += 16, lines--) {
		int limit;
		limit = 16;
		if (i + limit > size)
			limit = size - i;
		dump_line(realobj, i, limit);
	}
}

static void check_poison_obj(struct kmem_cache *cachep, void *objp)
{
	char *realobj;
	int size, i;
	int lines = 0;

	realobj = (char *)objp + obj_offset(cachep);
	size = obj_size(cachep);

	for (i = 0; i < size; i++) {
		char exp = POISON_FREE;
		if (i == size - 1)
			exp = POISON_END;
		if (realobj[i] != exp) {
			int limit;
			
			
			if (lines == 0) {
				printk(KERN_ERR
					"Slab corruption (%s): %s start=%p, len=%d\n",
					print_tainted(), cachep->name, realobj, size);
				print_objinfo(cachep, objp, 0);
			}
			
			i = (i / 16) * 16;
			limit = 16;
			if (i + limit > size)
				limit = size - i;
			dump_line(realobj, i, limit);
			i += 16;
			lines++;
			
			if (lines > 5)
				break;
		}
	}
	if (lines != 0) {
		struct slab *slabp = virt_to_slab(objp);
		unsigned int objnr;

		objnr = obj_to_index(cachep, slabp, objp);
		if (objnr) {
			objp = index_to_obj(cachep, slabp, objnr - 1);
			realobj = (char *)objp + obj_offset(cachep);
			printk(KERN_ERR "Prev obj: start=%p, len=%d\n",
			       realobj, size);
			print_objinfo(cachep, objp, 2);
		}
		if (objnr + 1 < cachep->num) {
			objp = index_to_obj(cachep, slabp, objnr + 1);
			realobj = (char *)objp + obj_offset(cachep);
			printk(KERN_ERR "Next obj: start=%p, len=%d\n",
			       realobj, size);
			print_objinfo(cachep, objp, 2);
		}
	}
}
#endif

#if DEBUG
static void slab_destroy_debugcheck(struct kmem_cache *cachep, struct slab *slabp)
{
	int i;
	for (i = 0; i < cachep->num; i++) {
		void *objp = index_to_obj(cachep, slabp, i);

		if (cachep->flags & SLAB_POISON) {
#ifdef CONFIG_DEBUG_PAGEALLOC
			if (cachep->buffer_size % PAGE_SIZE == 0 &&
					OFF_SLAB(cachep))
				kernel_map_pages(virt_to_page(objp),
					cachep->buffer_size / PAGE_SIZE, 1);
			else
				check_poison_obj(cachep, objp);
#else
			check_poison_obj(cachep, objp);
#endif
		}
		if (cachep->flags & SLAB_RED_ZONE) {
			if (*dbg_redzone1(cachep, objp) != RED_INACTIVE)
				slab_error(cachep, "start of a freed object "
					   "was overwritten");
			if (*dbg_redzone2(cachep, objp) != RED_INACTIVE)
				slab_error(cachep, "end of a freed object "
					   "was overwritten");
		}
	}
}
#else
static void slab_destroy_debugcheck(struct kmem_cache *cachep, struct slab *slabp)
{
}
#endif

static void slab_destroy(struct kmem_cache *cachep, struct slab *slabp)
{
	void *addr = slabp->s_mem - slabp->colouroff;

	slab_destroy_debugcheck(cachep, slabp);
	if (unlikely(cachep->flags & SLAB_DESTROY_BY_RCU)) {
		struct slab_rcu *slab_rcu;

		slab_rcu = (struct slab_rcu *)slabp;
		slab_rcu->cachep = cachep;
		slab_rcu->addr = addr;
		call_rcu(&slab_rcu->head, kmem_rcu_free);
	} else {
		kmem_freepages(cachep, addr);
		if (OFF_SLAB(cachep))
			kmem_cache_free(cachep->slabp_cache, slabp);
	}
}

static void __kmem_cache_destroy(struct kmem_cache *cachep)
{
	int i;
	struct kmem_list3 *l3;

	for_each_online_cpu(i)
	    kfree(cachep->array[i]);

	
	for_each_online_node(i) {
		l3 = cachep->nodelists[i];
		if (l3) {
			kfree(l3->shared);
			free_alien_cache(l3->alien);
			kfree(l3);
		}
	}
	kmem_cache_free(&cache_cache, cachep);
}


static size_t calculate_slab_order(struct kmem_cache *cachep,
			size_t size, size_t align, unsigned long flags)
{
	unsigned long offslab_limit;
	size_t left_over = 0;
	int gfporder;

	for (gfporder = 0; gfporder <= KMALLOC_MAX_ORDER; gfporder++) {
		unsigned int num;
		size_t remainder;

		cache_estimate(gfporder, size, align, flags, &remainder, &num);
		if (!num)
			continue;

		if (flags & CFLGS_OFF_SLAB) {
			offslab_limit = size - sizeof(struct slab);
			offslab_limit /= sizeof(kmem_bufctl_t);

 			if (num > offslab_limit)
				break;
		}

		
		cachep->num = num;
		cachep->gfporder = gfporder;
		left_over = remainder;

		if (flags & SLAB_RECLAIM_ACCOUNT)
			break;

		if (gfporder >= slab_max_order)
			break;

		if (left_over * 8 <= (PAGE_SIZE << gfporder))
			break;
	}
	return left_over;
}

static int __init_refok setup_cpu_cache(struct kmem_cache *cachep, gfp_t gfp)
{
	if (g_cpucache_up == FULL)
		return enable_cpucache(cachep, gfp);

	if (g_cpucache_up == NONE) {
		cachep->array[smp_processor_id()] = &initarray_generic.cache;

		set_up_list3s(cachep, SIZE_AC);
		if (INDEX_AC == INDEX_L3)
			g_cpucache_up = PARTIAL_L3;
		else
			g_cpucache_up = PARTIAL_AC;
	} else {
		cachep->array[smp_processor_id()] =
			kmalloc(sizeof(struct arraycache_init), gfp);

		if (g_cpucache_up == PARTIAL_AC) {
			set_up_list3s(cachep, SIZE_L3);
			g_cpucache_up = PARTIAL_L3;
		} else {
			int node;
			for_each_online_node(node) {
				cachep->nodelists[node] =
				    kmalloc_node(sizeof(struct kmem_list3),
						gfp, node);
				BUG_ON(!cachep->nodelists[node]);
				kmem_list3_init(cachep->nodelists[node]);
			}
		}
	}
	cachep->nodelists[numa_mem_id()]->next_reap =
			jiffies + REAPTIMEOUT_LIST3 +
			((unsigned long)cachep) % REAPTIMEOUT_LIST3;

	cpu_cache_get(cachep)->avail = 0;
	cpu_cache_get(cachep)->limit = BOOT_CPUCACHE_ENTRIES;
	cpu_cache_get(cachep)->batchcount = 1;
	cpu_cache_get(cachep)->touched = 0;
	cachep->batchcount = 1;
	cachep->limit = BOOT_CPUCACHE_ENTRIES;
	return 0;
}

struct kmem_cache *
kmem_cache_create (const char *name, size_t size, size_t align,
	unsigned long flags, void (*ctor)(void *))
{
	size_t left_over, slab_size, ralign;
	struct kmem_cache *cachep = NULL, *pc;
	gfp_t gfp;

	if (!name || in_interrupt() || (size < BYTES_PER_WORD) ||
	    size > KMALLOC_MAX_SIZE) {
		printk(KERN_ERR "%s: Early error in slab %s\n", __func__,
				name);
		BUG();
	}

	if (slab_is_available()) {
		get_online_cpus();
		mutex_lock(&cache_chain_mutex);
	}

	list_for_each_entry(pc, &cache_chain, next) {
		char tmp;
		int res;

		res = probe_kernel_address(pc->name, tmp);
		if (res) {
			printk(KERN_ERR
			       "SLAB: cache with size %d has lost its name\n",
			       pc->buffer_size);
			continue;
		}

		if (!strcmp(pc->name, name)) {
			printk(KERN_ERR
			       "kmem_cache_create: duplicate cache %s\n", name);
			dump_stack();
			goto oops;
		}
	}

#if DEBUG
	WARN_ON(strchr(name, ' '));	
#if FORCED_DEBUG
	if (size < 4096 || fls(size - 1) == fls(size-1 + REDZONE_ALIGN +
						2 * sizeof(unsigned long long)))
		flags |= SLAB_RED_ZONE | SLAB_STORE_USER;
	if (!(flags & SLAB_DESTROY_BY_RCU))
		flags |= SLAB_POISON;
#endif
	if (flags & SLAB_DESTROY_BY_RCU)
		BUG_ON(flags & SLAB_POISON);
#endif
	BUG_ON(flags & ~CREATE_MASK);

	if (size & (BYTES_PER_WORD - 1)) {
		size += (BYTES_PER_WORD - 1);
		size &= ~(BYTES_PER_WORD - 1);
	}

	

	
	if (flags & SLAB_HWCACHE_ALIGN) {
		ralign = cache_line_size();
		while (size <= ralign / 2)
			ralign /= 2;
	} else {
		ralign = BYTES_PER_WORD;
	}

	if (flags & SLAB_STORE_USER)
		ralign = BYTES_PER_WORD;

	if (flags & SLAB_RED_ZONE) {
		ralign = REDZONE_ALIGN;
		size += REDZONE_ALIGN - 1;
		size &= ~(REDZONE_ALIGN - 1);
	}

	
	if (ralign < ARCH_SLAB_MINALIGN) {
		ralign = ARCH_SLAB_MINALIGN;
	}
	
	if (ralign < align) {
		ralign = align;
	}
	
	if (ralign > __alignof__(unsigned long long))
		flags &= ~(SLAB_RED_ZONE | SLAB_STORE_USER);
	align = ralign;

	if (slab_is_available())
		gfp = GFP_KERNEL;
	else
		gfp = GFP_NOWAIT;

	
	cachep = kmem_cache_zalloc(&cache_cache, gfp);
	if (!cachep)
		goto oops;

	cachep->nodelists = (struct kmem_list3 **)&cachep->array[nr_cpu_ids];
#if DEBUG
	cachep->obj_size = size;

	if (flags & SLAB_RED_ZONE) {
		
		cachep->obj_offset += sizeof(unsigned long long);
		size += 2 * sizeof(unsigned long long);
	}
	if (flags & SLAB_STORE_USER) {
		if (flags & SLAB_RED_ZONE)
			size += REDZONE_ALIGN;
		else
			size += BYTES_PER_WORD;
	}
#if FORCED_DEBUG && defined(CONFIG_DEBUG_PAGEALLOC)
	if (size >= malloc_sizes[INDEX_L3 + 1].cs_size
	    && cachep->obj_size > cache_line_size() && ALIGN(size, align) < PAGE_SIZE) {
		cachep->obj_offset += PAGE_SIZE - ALIGN(size, align);
		size = PAGE_SIZE;
	}
#endif
#endif

	if ((size >= (PAGE_SIZE >> 3)) && !slab_early_init &&
	    !(flags & SLAB_NOLEAKTRACE))
		flags |= CFLGS_OFF_SLAB;

	size = ALIGN(size, align);

	left_over = calculate_slab_order(cachep, size, align, flags);

	if (!cachep->num) {
		printk(KERN_ERR
		       "kmem_cache_create: couldn't create cache %s.\n", name);
		kmem_cache_free(&cache_cache, cachep);
		cachep = NULL;
		goto oops;
	}
	slab_size = ALIGN(cachep->num * sizeof(kmem_bufctl_t)
			  + sizeof(struct slab), align);

	if (flags & CFLGS_OFF_SLAB && left_over >= slab_size) {
		flags &= ~CFLGS_OFF_SLAB;
		left_over -= slab_size;
	}

	if (flags & CFLGS_OFF_SLAB) {
		
		slab_size =
		    cachep->num * sizeof(kmem_bufctl_t) + sizeof(struct slab);

#ifdef CONFIG_PAGE_POISONING
		if (size % PAGE_SIZE == 0 && flags & SLAB_POISON)
			flags &= ~(SLAB_RED_ZONE | SLAB_STORE_USER);
#endif
	}

	cachep->colour_off = cache_line_size();
	
	if (cachep->colour_off < align)
		cachep->colour_off = align;
	cachep->colour = left_over / cachep->colour_off;
	cachep->slab_size = slab_size;
	cachep->flags = flags;
	cachep->gfpflags = 0;
	if (CONFIG_ZONE_DMA_FLAG && (flags & SLAB_CACHE_DMA))
		cachep->gfpflags |= GFP_DMA;
	cachep->buffer_size = size;
	cachep->reciprocal_buffer_size = reciprocal_value(size);

	if (flags & CFLGS_OFF_SLAB) {
		cachep->slabp_cache = kmem_find_general_cachep(slab_size, 0u);
		BUG_ON(ZERO_OR_NULL_PTR(cachep->slabp_cache));
	}
	cachep->ctor = ctor;
	cachep->name = name;

	if (setup_cpu_cache(cachep, gfp)) {
		__kmem_cache_destroy(cachep);
		cachep = NULL;
		goto oops;
	}

	if (flags & SLAB_DEBUG_OBJECTS) {
		WARN_ON_ONCE(flags & SLAB_DESTROY_BY_RCU);

		slab_set_debugobj_lock_classes(cachep);
	}

	
	list_add(&cachep->next, &cache_chain);
oops:
	if (!cachep && (flags & SLAB_PANIC))
		panic("kmem_cache_create(): failed to create slab `%s'\n",
		      name);
	if (slab_is_available()) {
		mutex_unlock(&cache_chain_mutex);
		put_online_cpus();
	}
	return cachep;
}
EXPORT_SYMBOL(kmem_cache_create);

#if DEBUG
static void check_irq_off(void)
{
	BUG_ON(!irqs_disabled());
}

static void check_irq_on(void)
{
	BUG_ON(irqs_disabled());
}

static void check_spinlock_acquired(struct kmem_cache *cachep)
{
#ifdef CONFIG_SMP
	check_irq_off();
	assert_spin_locked(&cachep->nodelists[numa_mem_id()]->list_lock);
#endif
}

static void check_spinlock_acquired_node(struct kmem_cache *cachep, int node)
{
#ifdef CONFIG_SMP
	check_irq_off();
	assert_spin_locked(&cachep->nodelists[node]->list_lock);
#endif
}

#else
#define check_irq_off()	do { } while(0)
#define check_irq_on()	do { } while(0)
#define check_spinlock_acquired(x) do { } while(0)
#define check_spinlock_acquired_node(x, y) do { } while(0)
#endif

static void drain_array(struct kmem_cache *cachep, struct kmem_list3 *l3,
			struct array_cache *ac,
			int force, int node);

static void do_drain(void *arg)
{
	struct kmem_cache *cachep = arg;
	struct array_cache *ac;
	int node = numa_mem_id();

	check_irq_off();
	ac = cpu_cache_get(cachep);
	spin_lock(&cachep->nodelists[node]->list_lock);
	free_block(cachep, ac->entry, ac->avail, node);
	spin_unlock(&cachep->nodelists[node]->list_lock);
	ac->avail = 0;
}

static void drain_cpu_caches(struct kmem_cache *cachep)
{
	struct kmem_list3 *l3;
	int node;

	on_each_cpu(do_drain, cachep, 1);
	check_irq_on();
	for_each_online_node(node) {
		l3 = cachep->nodelists[node];
		if (l3 && l3->alien)
			drain_alien_cache(cachep, l3->alien);
	}

	for_each_online_node(node) {
		l3 = cachep->nodelists[node];
		if (l3)
			drain_array(cachep, l3, l3->shared, 1, node);
	}
}

static int drain_freelist(struct kmem_cache *cache,
			struct kmem_list3 *l3, int tofree)
{
	struct list_head *p;
	int nr_freed;
	struct slab *slabp;

	nr_freed = 0;
	while (nr_freed < tofree && !list_empty(&l3->slabs_free)) {

		spin_lock_irq(&l3->list_lock);
		p = l3->slabs_free.prev;
		if (p == &l3->slabs_free) {
			spin_unlock_irq(&l3->list_lock);
			goto out;
		}

		slabp = list_entry(p, struct slab, list);
#if DEBUG
		BUG_ON(slabp->inuse);
#endif
		list_del(&slabp->list);
		l3->free_objects -= cache->num;
		spin_unlock_irq(&l3->list_lock);
		slab_destroy(cache, slabp);
		nr_freed++;
	}
out:
	return nr_freed;
}

static int __cache_shrink(struct kmem_cache *cachep)
{
	int ret = 0, i = 0;
	struct kmem_list3 *l3;

	drain_cpu_caches(cachep);

	check_irq_on();
	for_each_online_node(i) {
		l3 = cachep->nodelists[i];
		if (!l3)
			continue;

		drain_freelist(cachep, l3, l3->free_objects);

		ret += !list_empty(&l3->slabs_full) ||
			!list_empty(&l3->slabs_partial);
	}
	return (ret ? 1 : 0);
}

int kmem_cache_shrink(struct kmem_cache *cachep)
{
	int ret;
	BUG_ON(!cachep || in_interrupt());

	get_online_cpus();
	mutex_lock(&cache_chain_mutex);
	ret = __cache_shrink(cachep);
	mutex_unlock(&cache_chain_mutex);
	put_online_cpus();
	return ret;
}
EXPORT_SYMBOL(kmem_cache_shrink);

void kmem_cache_destroy(struct kmem_cache *cachep)
{
	BUG_ON(!cachep || in_interrupt());

	
	get_online_cpus();
	mutex_lock(&cache_chain_mutex);
	list_del(&cachep->next);
	if (__cache_shrink(cachep)) {
		slab_error(cachep, "Can't free all objects");
		list_add(&cachep->next, &cache_chain);
		mutex_unlock(&cache_chain_mutex);
		put_online_cpus();
		return;
	}

	if (unlikely(cachep->flags & SLAB_DESTROY_BY_RCU))
		rcu_barrier();

	__kmem_cache_destroy(cachep);
	mutex_unlock(&cache_chain_mutex);
	put_online_cpus();
}
EXPORT_SYMBOL(kmem_cache_destroy);

static struct slab *alloc_slabmgmt(struct kmem_cache *cachep, void *objp,
				   int colour_off, gfp_t local_flags,
				   int nodeid)
{
	struct slab *slabp;

	if (OFF_SLAB(cachep)) {
		
		slabp = kmem_cache_alloc_node(cachep->slabp_cache,
					      local_flags, nodeid);
		kmemleak_scan_area(&slabp->list, sizeof(struct list_head),
				   local_flags);
		if (!slabp)
			return NULL;
	} else {
		slabp = objp + colour_off;
		colour_off += cachep->slab_size;
	}
	slabp->inuse = 0;
	slabp->colouroff = colour_off;
	slabp->s_mem = objp + colour_off;
	slabp->nodeid = nodeid;
	slabp->free = 0;
	return slabp;
}

static inline kmem_bufctl_t *slab_bufctl(struct slab *slabp)
{
	return (kmem_bufctl_t *) (slabp + 1);
}

static void cache_init_objs(struct kmem_cache *cachep,
			    struct slab *slabp)
{
	int i;

	for (i = 0; i < cachep->num; i++) {
		void *objp = index_to_obj(cachep, slabp, i);
#if DEBUG
		
		if (cachep->flags & SLAB_POISON)
			poison_obj(cachep, objp, POISON_FREE);
		if (cachep->flags & SLAB_STORE_USER)
			*dbg_userword(cachep, objp) = NULL;

		if (cachep->flags & SLAB_RED_ZONE) {
			*dbg_redzone1(cachep, objp) = RED_INACTIVE;
			*dbg_redzone2(cachep, objp) = RED_INACTIVE;
		}
		if (cachep->ctor && !(cachep->flags & SLAB_POISON))
			cachep->ctor(objp + obj_offset(cachep));

		if (cachep->flags & SLAB_RED_ZONE) {
			if (*dbg_redzone2(cachep, objp) != RED_INACTIVE)
				slab_error(cachep, "constructor overwrote the"
					   " end of an object");
			if (*dbg_redzone1(cachep, objp) != RED_INACTIVE)
				slab_error(cachep, "constructor overwrote the"
					   " start of an object");
		}
		if ((cachep->buffer_size % PAGE_SIZE) == 0 &&
			    OFF_SLAB(cachep) && cachep->flags & SLAB_POISON)
			kernel_map_pages(virt_to_page(objp),
					 cachep->buffer_size / PAGE_SIZE, 0);
#else
		if (cachep->ctor)
			cachep->ctor(objp);
#endif
		slab_bufctl(slabp)[i] = i + 1;
	}
	slab_bufctl(slabp)[i - 1] = BUFCTL_END;
}

static void kmem_flagcheck(struct kmem_cache *cachep, gfp_t flags)
{
	if (CONFIG_ZONE_DMA_FLAG) {
		if (flags & GFP_DMA)
			BUG_ON(!(cachep->gfpflags & GFP_DMA));
		else
			BUG_ON(cachep->gfpflags & GFP_DMA);
	}
}

static void *slab_get_obj(struct kmem_cache *cachep, struct slab *slabp,
				int nodeid)
{
	void *objp = index_to_obj(cachep, slabp, slabp->free);
	kmem_bufctl_t next;

	slabp->inuse++;
	next = slab_bufctl(slabp)[slabp->free];
#if DEBUG
	slab_bufctl(slabp)[slabp->free] = BUFCTL_FREE;
	WARN_ON(slabp->nodeid != nodeid);
#endif
	slabp->free = next;

	return objp;
}

static void slab_put_obj(struct kmem_cache *cachep, struct slab *slabp,
				void *objp, int nodeid)
{
	unsigned int objnr = obj_to_index(cachep, slabp, objp);

#if DEBUG
	
	WARN_ON(slabp->nodeid != nodeid);

	if (slab_bufctl(slabp)[objnr] + 1 <= SLAB_LIMIT + 1) {
		printk(KERN_ERR "slab: double free detected in cache "
				"'%s', objp %p\n", cachep->name, objp);
		BUG();
	}
#endif
	slab_bufctl(slabp)[objnr] = slabp->free;
	slabp->free = objnr;
	slabp->inuse--;
}

static void slab_map_pages(struct kmem_cache *cache, struct slab *slab,
			   void *addr)
{
	int nr_pages;
	struct page *page;

	page = virt_to_page(addr);

	nr_pages = 1;
	if (likely(!PageCompound(page)))
		nr_pages <<= cache->gfporder;

	do {
		page_set_cache(page, cache);
		page_set_slab(page, slab);
		page++;
	} while (--nr_pages);
}

static int cache_grow(struct kmem_cache *cachep,
		gfp_t flags, int nodeid, void *objp)
{
	struct slab *slabp;
	size_t offset;
	gfp_t local_flags;
	struct kmem_list3 *l3;

	BUG_ON(flags & GFP_SLAB_BUG_MASK);
	local_flags = flags & (GFP_CONSTRAINT_MASK|GFP_RECLAIM_MASK);

	
	check_irq_off();
	l3 = cachep->nodelists[nodeid];
	spin_lock(&l3->list_lock);

	
	offset = l3->colour_next;
	l3->colour_next++;
	if (l3->colour_next >= cachep->colour)
		l3->colour_next = 0;
	spin_unlock(&l3->list_lock);

	offset *= cachep->colour_off;

	if (local_flags & __GFP_WAIT)
		local_irq_enable();

	kmem_flagcheck(cachep, flags);

	if (!objp)
		objp = kmem_getpages(cachep, local_flags, nodeid);
	if (!objp)
		goto failed;

	
	slabp = alloc_slabmgmt(cachep, objp, offset,
			local_flags & ~GFP_CONSTRAINT_MASK, nodeid);
	if (!slabp)
		goto opps1;

	slab_map_pages(cachep, slabp, objp);

	cache_init_objs(cachep, slabp);

	if (local_flags & __GFP_WAIT)
		local_irq_disable();
	check_irq_off();
	spin_lock(&l3->list_lock);

	
	list_add_tail(&slabp->list, &(l3->slabs_free));
	STATS_INC_GROWN(cachep);
	l3->free_objects += cachep->num;
	spin_unlock(&l3->list_lock);
	return 1;
opps1:
	kmem_freepages(cachep, objp);
failed:
	if (local_flags & __GFP_WAIT)
		local_irq_disable();
	return 0;
}

#if DEBUG

static void kfree_debugcheck(const void *objp)
{
	if (!virt_addr_valid(objp)) {
		printk(KERN_ERR "kfree_debugcheck: out of range ptr %lxh.\n",
		       (unsigned long)objp);
		BUG();
	}
}

static inline void verify_redzone_free(struct kmem_cache *cache, void *obj)
{
	unsigned long long redzone1, redzone2;

	redzone1 = *dbg_redzone1(cache, obj);
	redzone2 = *dbg_redzone2(cache, obj);

	if (redzone1 == RED_ACTIVE && redzone2 == RED_ACTIVE)
		return;

	if (redzone1 == RED_INACTIVE && redzone2 == RED_INACTIVE)
		slab_error(cache, "double free detected");
	else
		slab_error(cache, "memory outside object was overwritten");

	printk(KERN_ERR "%p: redzone 1:0x%llx, redzone 2:0x%llx.\n",
			obj, redzone1, redzone2);
}

static void *cache_free_debugcheck(struct kmem_cache *cachep, void *objp,
				   void *caller)
{
	struct page *page;
	unsigned int objnr;
	struct slab *slabp;

	BUG_ON(virt_to_cache(objp) != cachep);

	objp -= obj_offset(cachep);
	kfree_debugcheck(objp);
	page = virt_to_head_page(objp);

	slabp = page_get_slab(page);

	if (cachep->flags & SLAB_RED_ZONE) {
		verify_redzone_free(cachep, objp);
		*dbg_redzone1(cachep, objp) = RED_INACTIVE;
		*dbg_redzone2(cachep, objp) = RED_INACTIVE;
	}
	if (cachep->flags & SLAB_STORE_USER)
		*dbg_userword(cachep, objp) = caller;

	objnr = obj_to_index(cachep, slabp, objp);

	BUG_ON(objnr >= cachep->num);
	BUG_ON(objp != index_to_obj(cachep, slabp, objnr));

#ifdef CONFIG_DEBUG_SLAB_LEAK
	slab_bufctl(slabp)[objnr] = BUFCTL_FREE;
#endif
	if (cachep->flags & SLAB_POISON) {
#ifdef CONFIG_DEBUG_PAGEALLOC
		if ((cachep->buffer_size % PAGE_SIZE)==0 && OFF_SLAB(cachep)) {
			store_stackinfo(cachep, objp, (unsigned long)caller);
			kernel_map_pages(virt_to_page(objp),
					 cachep->buffer_size / PAGE_SIZE, 0);
		} else {
			poison_obj(cachep, objp, POISON_FREE);
		}
#else
		poison_obj(cachep, objp, POISON_FREE);
#endif
	}
	return objp;
}

static void check_slabp(struct kmem_cache *cachep, struct slab *slabp)
{
	kmem_bufctl_t i;
	int entries = 0;

	
	for (i = slabp->free; i != BUFCTL_END; i = slab_bufctl(slabp)[i]) {
		entries++;
		if (entries > cachep->num || i >= cachep->num)
			goto bad;
	}
	if (entries != cachep->num - slabp->inuse) {
bad:
		printk(KERN_ERR "slab: Internal list corruption detected in "
			"cache '%s'(%d), slabp %p(%d). Tainted(%s). Hexdump:\n",
			cachep->name, cachep->num, slabp, slabp->inuse,
			print_tainted());
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_OFFSET, 16, 1, slabp,
			sizeof(*slabp) + cachep->num * sizeof(kmem_bufctl_t),
			1);
		BUG();
	}
}
#else
#define kfree_debugcheck(x) do { } while(0)
#define cache_free_debugcheck(x,objp,z) (objp)
#define check_slabp(x,y) do { } while(0)
#endif

static void *cache_alloc_refill(struct kmem_cache *cachep, gfp_t flags)
{
	int batchcount;
	struct kmem_list3 *l3;
	struct array_cache *ac;
	int node;

retry:
	check_irq_off();
	node = numa_mem_id();
	ac = cpu_cache_get(cachep);
	batchcount = ac->batchcount;
	if (!ac->touched && batchcount > BATCHREFILL_LIMIT) {
		batchcount = BATCHREFILL_LIMIT;
	}
	l3 = cachep->nodelists[node];

	BUG_ON(ac->avail > 0 || !l3);
	spin_lock(&l3->list_lock);

	
	if (l3->shared && transfer_objects(ac, l3->shared, batchcount)) {
		l3->shared->touched = 1;
		goto alloc_done;
	}

	while (batchcount > 0) {
		struct list_head *entry;
		struct slab *slabp;
		
		entry = l3->slabs_partial.next;
		if (entry == &l3->slabs_partial) {
			l3->free_touched = 1;
			entry = l3->slabs_free.next;
			if (entry == &l3->slabs_free)
				goto must_grow;
		}

		slabp = list_entry(entry, struct slab, list);
		check_slabp(cachep, slabp);
		check_spinlock_acquired(cachep);

		BUG_ON(slabp->inuse >= cachep->num);

		while (slabp->inuse < cachep->num && batchcount--) {
			STATS_INC_ALLOCED(cachep);
			STATS_INC_ACTIVE(cachep);
			STATS_SET_HIGH(cachep);

			ac->entry[ac->avail++] = slab_get_obj(cachep, slabp,
							    node);
		}
		check_slabp(cachep, slabp);

		
		list_del(&slabp->list);
		if (slabp->free == BUFCTL_END)
			list_add(&slabp->list, &l3->slabs_full);
		else
			list_add(&slabp->list, &l3->slabs_partial);
	}

must_grow:
	l3->free_objects -= ac->avail;
alloc_done:
	spin_unlock(&l3->list_lock);

	if (unlikely(!ac->avail)) {
		int x;
		x = cache_grow(cachep, flags | GFP_THISNODE, node, NULL);

		
		ac = cpu_cache_get(cachep);
		if (!x && ac->avail == 0)	
			return NULL;

		if (!ac->avail)		
			goto retry;
	}
	ac->touched = 1;
	return ac->entry[--ac->avail];
}

static inline void cache_alloc_debugcheck_before(struct kmem_cache *cachep,
						gfp_t flags)
{
	might_sleep_if(flags & __GFP_WAIT);
#if DEBUG
	kmem_flagcheck(cachep, flags);
#endif
}

#if DEBUG
static void *cache_alloc_debugcheck_after(struct kmem_cache *cachep,
				gfp_t flags, void *objp, void *caller)
{
	if (!objp)
		return objp;
	if (cachep->flags & SLAB_POISON) {
#ifdef CONFIG_DEBUG_PAGEALLOC
		if ((cachep->buffer_size % PAGE_SIZE) == 0 && OFF_SLAB(cachep))
			kernel_map_pages(virt_to_page(objp),
					 cachep->buffer_size / PAGE_SIZE, 1);
		else
			check_poison_obj(cachep, objp);
#else
		check_poison_obj(cachep, objp);
#endif
		poison_obj(cachep, objp, POISON_INUSE);
	}
	if (cachep->flags & SLAB_STORE_USER)
		*dbg_userword(cachep, objp) = caller;

	if (cachep->flags & SLAB_RED_ZONE) {
		if (*dbg_redzone1(cachep, objp) != RED_INACTIVE ||
				*dbg_redzone2(cachep, objp) != RED_INACTIVE) {
			slab_error(cachep, "double free, or memory outside"
						" object was overwritten");
			printk(KERN_ERR
				"%p: redzone 1:0x%llx, redzone 2:0x%llx\n",
				objp, *dbg_redzone1(cachep, objp),
				*dbg_redzone2(cachep, objp));
		}
		*dbg_redzone1(cachep, objp) = RED_ACTIVE;
		*dbg_redzone2(cachep, objp) = RED_ACTIVE;
	}
#ifdef CONFIG_DEBUG_SLAB_LEAK
	{
		struct slab *slabp;
		unsigned objnr;

		slabp = page_get_slab(virt_to_head_page(objp));
		objnr = (unsigned)(objp - slabp->s_mem) / cachep->buffer_size;
		slab_bufctl(slabp)[objnr] = BUFCTL_ACTIVE;
	}
#endif
	objp += obj_offset(cachep);
	if (cachep->ctor && cachep->flags & SLAB_POISON)
		cachep->ctor(objp);
	if (ARCH_SLAB_MINALIGN &&
	    ((unsigned long)objp & (ARCH_SLAB_MINALIGN-1))) {
		printk(KERN_ERR "0x%p: not aligned to ARCH_SLAB_MINALIGN=%d\n",
		       objp, (int)ARCH_SLAB_MINALIGN);
	}
	return objp;
}
#else
#define cache_alloc_debugcheck_after(a,b,objp,d) (objp)
#endif

static bool slab_should_failslab(struct kmem_cache *cachep, gfp_t flags)
{
	if (cachep == &cache_cache)
		return false;

	return should_failslab(obj_size(cachep), flags, cachep->flags);
}

static inline void *____cache_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	void *objp;
	struct array_cache *ac;

	check_irq_off();

	ac = cpu_cache_get(cachep);
	if (likely(ac->avail)) {
		STATS_INC_ALLOCHIT(cachep);
		ac->touched = 1;
		objp = ac->entry[--ac->avail];
	} else {
		STATS_INC_ALLOCMISS(cachep);
		objp = cache_alloc_refill(cachep, flags);
		ac = cpu_cache_get(cachep);
	}
	if (objp)
		kmemleak_erase(&ac->entry[ac->avail]);
	return objp;
}

#ifdef CONFIG_NUMA
static void *alternate_node_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	int nid_alloc, nid_here;

	if (in_interrupt() || (flags & __GFP_THISNODE))
		return NULL;
	nid_alloc = nid_here = numa_mem_id();
	if (cpuset_do_slab_mem_spread() && (cachep->flags & SLAB_MEM_SPREAD))
		nid_alloc = cpuset_slab_spread_node();
	else if (current->mempolicy)
		nid_alloc = slab_node(current->mempolicy);
	if (nid_alloc != nid_here)
		return ____cache_alloc_node(cachep, flags, nid_alloc);
	return NULL;
}

static void *fallback_alloc(struct kmem_cache *cache, gfp_t flags)
{
	struct zonelist *zonelist;
	gfp_t local_flags;
	struct zoneref *z;
	struct zone *zone;
	enum zone_type high_zoneidx = gfp_zone(flags);
	void *obj = NULL;
	int nid;
	unsigned int cpuset_mems_cookie;

	if (flags & __GFP_THISNODE)
		return NULL;

	local_flags = flags & (GFP_CONSTRAINT_MASK|GFP_RECLAIM_MASK);

retry_cpuset:
	cpuset_mems_cookie = get_mems_allowed();
	zonelist = node_zonelist(slab_node(current->mempolicy), flags);

retry:
	for_each_zone_zonelist(zone, z, zonelist, high_zoneidx) {
		nid = zone_to_nid(zone);

		if (cpuset_zone_allowed_hardwall(zone, flags) &&
			cache->nodelists[nid] &&
			cache->nodelists[nid]->free_objects) {
				obj = ____cache_alloc_node(cache,
					flags | GFP_THISNODE, nid);
				if (obj)
					break;
		}
	}

	if (!obj) {
		if (local_flags & __GFP_WAIT)
			local_irq_enable();
		kmem_flagcheck(cache, flags);
		obj = kmem_getpages(cache, local_flags, numa_mem_id());
		if (local_flags & __GFP_WAIT)
			local_irq_disable();
		if (obj) {
			nid = page_to_nid(virt_to_page(obj));
			if (cache_grow(cache, flags, nid, obj)) {
				obj = ____cache_alloc_node(cache,
					flags | GFP_THISNODE, nid);
				if (!obj)
					goto retry;
			} else {
				
				obj = NULL;
			}
		}
	}

	if (unlikely(!put_mems_allowed(cpuset_mems_cookie) && !obj))
		goto retry_cpuset;
	return obj;
}

static void *____cache_alloc_node(struct kmem_cache *cachep, gfp_t flags,
				int nodeid)
{
	struct list_head *entry;
	struct slab *slabp;
	struct kmem_list3 *l3;
	void *obj;
	int x;

	l3 = cachep->nodelists[nodeid];
	BUG_ON(!l3);

retry:
	check_irq_off();
	spin_lock(&l3->list_lock);
	entry = l3->slabs_partial.next;
	if (entry == &l3->slabs_partial) {
		l3->free_touched = 1;
		entry = l3->slabs_free.next;
		if (entry == &l3->slabs_free)
			goto must_grow;
	}

	slabp = list_entry(entry, struct slab, list);
	check_spinlock_acquired_node(cachep, nodeid);
	check_slabp(cachep, slabp);

	STATS_INC_NODEALLOCS(cachep);
	STATS_INC_ACTIVE(cachep);
	STATS_SET_HIGH(cachep);

	BUG_ON(slabp->inuse == cachep->num);

	obj = slab_get_obj(cachep, slabp, nodeid);
	check_slabp(cachep, slabp);
	l3->free_objects--;
	
	list_del(&slabp->list);

	if (slabp->free == BUFCTL_END)
		list_add(&slabp->list, &l3->slabs_full);
	else
		list_add(&slabp->list, &l3->slabs_partial);

	spin_unlock(&l3->list_lock);
	goto done;

must_grow:
	spin_unlock(&l3->list_lock);
	x = cache_grow(cachep, flags | GFP_THISNODE, nodeid, NULL);
	if (x)
		goto retry;

	return fallback_alloc(cachep, flags);

done:
	return obj;
}

static __always_inline void *
__cache_alloc_node(struct kmem_cache *cachep, gfp_t flags, int nodeid,
		   void *caller)
{
	unsigned long save_flags;
	void *ptr;
	int slab_node = numa_mem_id();

	flags &= gfp_allowed_mask;

	lockdep_trace_alloc(flags);

	if (slab_should_failslab(cachep, flags))
		return NULL;

	cache_alloc_debugcheck_before(cachep, flags);
	local_irq_save(save_flags);

	if (nodeid == NUMA_NO_NODE)
		nodeid = slab_node;

	if (unlikely(!cachep->nodelists[nodeid])) {
		
		ptr = fallback_alloc(cachep, flags);
		goto out;
	}

	if (nodeid == slab_node) {
		ptr = ____cache_alloc(cachep, flags);
		if (ptr)
			goto out;
	}
	
	ptr = ____cache_alloc_node(cachep, flags, nodeid);
  out:
	local_irq_restore(save_flags);
	ptr = cache_alloc_debugcheck_after(cachep, flags, ptr, caller);
	kmemleak_alloc_recursive(ptr, obj_size(cachep), 1, cachep->flags,
				 flags);

	if (likely(ptr))
		kmemcheck_slab_alloc(cachep, flags, ptr, obj_size(cachep));

	if (unlikely((flags & __GFP_ZERO) && ptr))
		memset(ptr, 0, obj_size(cachep));

	return ptr;
}

static __always_inline void *
__do_cache_alloc(struct kmem_cache *cache, gfp_t flags)
{
	void *objp;

	if (unlikely(current->flags & (PF_SPREAD_SLAB | PF_MEMPOLICY))) {
		objp = alternate_node_alloc(cache, flags);
		if (objp)
			goto out;
	}
	objp = ____cache_alloc(cache, flags);

	if (!objp)
		objp = ____cache_alloc_node(cache, flags, numa_mem_id());

  out:
	return objp;
}
#else

static __always_inline void *
__do_cache_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	return ____cache_alloc(cachep, flags);
}

#endif 

static __always_inline void *
__cache_alloc(struct kmem_cache *cachep, gfp_t flags, void *caller)
{
	unsigned long save_flags;
	void *objp;

	flags &= gfp_allowed_mask;

	lockdep_trace_alloc(flags);

	if (slab_should_failslab(cachep, flags))
		return NULL;

	cache_alloc_debugcheck_before(cachep, flags);
	local_irq_save(save_flags);
	objp = __do_cache_alloc(cachep, flags);
	local_irq_restore(save_flags);
	objp = cache_alloc_debugcheck_after(cachep, flags, objp, caller);
	kmemleak_alloc_recursive(objp, obj_size(cachep), 1, cachep->flags,
				 flags);
	prefetchw(objp);

	if (likely(objp))
		kmemcheck_slab_alloc(cachep, flags, objp, obj_size(cachep));

	if (unlikely((flags & __GFP_ZERO) && objp))
		memset(objp, 0, obj_size(cachep));

	return objp;
}

static void free_block(struct kmem_cache *cachep, void **objpp, int nr_objects,
		       int node)
{
	int i;
	struct kmem_list3 *l3;

	for (i = 0; i < nr_objects; i++) {
		void *objp = objpp[i];
		struct slab *slabp;

		slabp = virt_to_slab(objp);
		l3 = cachep->nodelists[node];
		list_del(&slabp->list);
		check_spinlock_acquired_node(cachep, node);
		check_slabp(cachep, slabp);
		slab_put_obj(cachep, slabp, objp, node);
		STATS_DEC_ACTIVE(cachep);
		l3->free_objects++;
		check_slabp(cachep, slabp);

		
		if (slabp->inuse == 0) {
			if (l3->free_objects > l3->free_limit) {
				l3->free_objects -= cachep->num;
				slab_destroy(cachep, slabp);
			} else {
				list_add(&slabp->list, &l3->slabs_free);
			}
		} else {
			list_add_tail(&slabp->list, &l3->slabs_partial);
		}
	}
}

static void cache_flusharray(struct kmem_cache *cachep, struct array_cache *ac)
{
	int batchcount;
	struct kmem_list3 *l3;
	int node = numa_mem_id();

	batchcount = ac->batchcount;
#if DEBUG
	BUG_ON(!batchcount || batchcount > ac->avail);
#endif
	check_irq_off();
	l3 = cachep->nodelists[node];
	spin_lock(&l3->list_lock);
	if (l3->shared) {
		struct array_cache *shared_array = l3->shared;
		int max = shared_array->limit - shared_array->avail;
		if (max) {
			if (batchcount > max)
				batchcount = max;
			memcpy(&(shared_array->entry[shared_array->avail]),
			       ac->entry, sizeof(void *) * batchcount);
			shared_array->avail += batchcount;
			goto free_done;
		}
	}

	free_block(cachep, ac->entry, batchcount, node);
free_done:
#if STATS
	{
		int i = 0;
		struct list_head *p;

		p = l3->slabs_free.next;
		while (p != &(l3->slabs_free)) {
			struct slab *slabp;

			slabp = list_entry(p, struct slab, list);
			BUG_ON(slabp->inuse);

			i++;
			p = p->next;
		}
		STATS_SET_FREEABLE(cachep, i);
	}
#endif
	spin_unlock(&l3->list_lock);
	ac->avail -= batchcount;
	memmove(ac->entry, &(ac->entry[batchcount]), sizeof(void *)*ac->avail);
}

static inline void __cache_free(struct kmem_cache *cachep, void *objp,
    void *caller)
{
	struct array_cache *ac = cpu_cache_get(cachep);

	check_irq_off();
	kmemleak_free_recursive(objp, cachep->flags);
	objp = cache_free_debugcheck(cachep, objp, caller);

	kmemcheck_slab_free(cachep, objp, obj_size(cachep));

	if (nr_online_nodes > 1 && cache_free_alien(cachep, objp))
		return;

	if (likely(ac->avail < ac->limit)) {
		STATS_INC_FREEHIT(cachep);
	} else {
		STATS_INC_FREEMISS(cachep);
		cache_flusharray(cachep, ac);
	}

	ac->entry[ac->avail++] = objp;
}

void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	void *ret = __cache_alloc(cachep, flags, __builtin_return_address(0));

	trace_kmem_cache_alloc(_RET_IP_, ret,
			       obj_size(cachep), cachep->buffer_size, flags);

	return ret;
}
EXPORT_SYMBOL(kmem_cache_alloc);

#ifdef CONFIG_TRACING
void *
kmem_cache_alloc_trace(size_t size, struct kmem_cache *cachep, gfp_t flags)
{
	void *ret;

	ret = __cache_alloc(cachep, flags, __builtin_return_address(0));

	trace_kmalloc(_RET_IP_, ret,
		      size, slab_buffer_size(cachep), flags);
	return ret;
}
EXPORT_SYMBOL(kmem_cache_alloc_trace);
#endif

#ifdef CONFIG_NUMA
void *kmem_cache_alloc_node(struct kmem_cache *cachep, gfp_t flags, int nodeid)
{
	void *ret = __cache_alloc_node(cachep, flags, nodeid,
				       __builtin_return_address(0));

	trace_kmem_cache_alloc_node(_RET_IP_, ret,
				    obj_size(cachep), cachep->buffer_size,
				    flags, nodeid);

	return ret;
}
EXPORT_SYMBOL(kmem_cache_alloc_node);

#ifdef CONFIG_TRACING
void *kmem_cache_alloc_node_trace(size_t size,
				  struct kmem_cache *cachep,
				  gfp_t flags,
				  int nodeid)
{
	void *ret;

	ret = __cache_alloc_node(cachep, flags, nodeid,
				  __builtin_return_address(0));
	trace_kmalloc_node(_RET_IP_, ret,
			   size, slab_buffer_size(cachep),
			   flags, nodeid);
	return ret;
}
EXPORT_SYMBOL(kmem_cache_alloc_node_trace);
#endif

static __always_inline void *
__do_kmalloc_node(size_t size, gfp_t flags, int node, void *caller)
{
	struct kmem_cache *cachep;

	cachep = kmem_find_general_cachep(size, flags);
	if (unlikely(ZERO_OR_NULL_PTR(cachep)))
		return cachep;
	return kmem_cache_alloc_node_trace(size, cachep, flags, node);
}

#if defined(CONFIG_DEBUG_SLAB) || defined(CONFIG_TRACING)
void *__kmalloc_node(size_t size, gfp_t flags, int node)
{
	return __do_kmalloc_node(size, flags, node,
			__builtin_return_address(0));
}
EXPORT_SYMBOL(__kmalloc_node);

void *__kmalloc_node_track_caller(size_t size, gfp_t flags,
		int node, unsigned long caller)
{
	return __do_kmalloc_node(size, flags, node, (void *)caller);
}
EXPORT_SYMBOL(__kmalloc_node_track_caller);
#else
void *__kmalloc_node(size_t size, gfp_t flags, int node)
{
	return __do_kmalloc_node(size, flags, node, NULL);
}
EXPORT_SYMBOL(__kmalloc_node);
#endif 
#endif 

static __always_inline void *__do_kmalloc(size_t size, gfp_t flags,
					  void *caller)
{
	struct kmem_cache *cachep;
	void *ret;

	cachep = __find_general_cachep(size, flags);
	if (unlikely(ZERO_OR_NULL_PTR(cachep)))
		return cachep;
	ret = __cache_alloc(cachep, flags, caller);

	trace_kmalloc((unsigned long) caller, ret,
		      size, cachep->buffer_size, flags);

	return ret;
}


#if defined(CONFIG_DEBUG_SLAB) || defined(CONFIG_TRACING)
void *__kmalloc(size_t size, gfp_t flags)
{
	return __do_kmalloc(size, flags, __builtin_return_address(0));
}
EXPORT_SYMBOL(__kmalloc);

void *__kmalloc_track_caller(size_t size, gfp_t flags, unsigned long caller)
{
	return __do_kmalloc(size, flags, (void *)caller);
}
EXPORT_SYMBOL(__kmalloc_track_caller);

#else
void *__kmalloc(size_t size, gfp_t flags)
{
	return __do_kmalloc(size, flags, NULL);
}
EXPORT_SYMBOL(__kmalloc);
#endif

void kmem_cache_free(struct kmem_cache *cachep, void *objp)
{
	unsigned long flags;

	local_irq_save(flags);
	debug_check_no_locks_freed(objp, obj_size(cachep));
	if (!(cachep->flags & SLAB_DEBUG_OBJECTS))
		debug_check_no_obj_freed(objp, obj_size(cachep));
	__cache_free(cachep, objp, __builtin_return_address(0));
	local_irq_restore(flags);

	trace_kmem_cache_free(_RET_IP_, objp);
}
EXPORT_SYMBOL(kmem_cache_free);

void kfree(const void *objp)
{
	struct kmem_cache *c;
	unsigned long flags;

	trace_kfree(_RET_IP_, objp);

	if (unlikely(ZERO_OR_NULL_PTR(objp)))
		return;
	local_irq_save(flags);
	kfree_debugcheck(objp);
	c = virt_to_cache(objp);
	debug_check_no_locks_freed(objp, obj_size(c));
	debug_check_no_obj_freed(objp, obj_size(c));
	__cache_free(c, (void *)objp, __builtin_return_address(0));
	local_irq_restore(flags);
}
EXPORT_SYMBOL(kfree);

unsigned int kmem_cache_size(struct kmem_cache *cachep)
{
	return obj_size(cachep);
}
EXPORT_SYMBOL(kmem_cache_size);

static int alloc_kmemlist(struct kmem_cache *cachep, gfp_t gfp)
{
	int node;
	struct kmem_list3 *l3;
	struct array_cache *new_shared;
	struct array_cache **new_alien = NULL;

	for_each_online_node(node) {

                if (use_alien_caches) {
                        new_alien = alloc_alien_cache(node, cachep->limit, gfp);
                        if (!new_alien)
                                goto fail;
                }

		new_shared = NULL;
		if (cachep->shared) {
			new_shared = alloc_arraycache(node,
				cachep->shared*cachep->batchcount,
					0xbaadf00d, gfp);
			if (!new_shared) {
				free_alien_cache(new_alien);
				goto fail;
			}
		}

		l3 = cachep->nodelists[node];
		if (l3) {
			struct array_cache *shared = l3->shared;

			spin_lock_irq(&l3->list_lock);

			if (shared)
				free_block(cachep, shared->entry,
						shared->avail, node);

			l3->shared = new_shared;
			if (!l3->alien) {
				l3->alien = new_alien;
				new_alien = NULL;
			}
			l3->free_limit = (1 + nr_cpus_node(node)) *
					cachep->batchcount + cachep->num;
			spin_unlock_irq(&l3->list_lock);
			kfree(shared);
			free_alien_cache(new_alien);
			continue;
		}
		l3 = kmalloc_node(sizeof(struct kmem_list3), gfp, node);
		if (!l3) {
			free_alien_cache(new_alien);
			kfree(new_shared);
			goto fail;
		}

		kmem_list3_init(l3);
		l3->next_reap = jiffies + REAPTIMEOUT_LIST3 +
				((unsigned long)cachep) % REAPTIMEOUT_LIST3;
		l3->shared = new_shared;
		l3->alien = new_alien;
		l3->free_limit = (1 + nr_cpus_node(node)) *
					cachep->batchcount + cachep->num;
		cachep->nodelists[node] = l3;
	}
	return 0;

fail:
	if (!cachep->next.next) {
		
		node--;
		while (node >= 0) {
			if (cachep->nodelists[node]) {
				l3 = cachep->nodelists[node];

				kfree(l3->shared);
				free_alien_cache(l3->alien);
				kfree(l3);
				cachep->nodelists[node] = NULL;
			}
			node--;
		}
	}
	return -ENOMEM;
}

struct ccupdate_struct {
	struct kmem_cache *cachep;
	struct array_cache *new[0];
};

static void do_ccupdate_local(void *info)
{
	struct ccupdate_struct *new = info;
	struct array_cache *old;

	check_irq_off();
	old = cpu_cache_get(new->cachep);

	new->cachep->array[smp_processor_id()] = new->new[smp_processor_id()];
	new->new[smp_processor_id()] = old;
}

static int do_tune_cpucache(struct kmem_cache *cachep, int limit,
				int batchcount, int shared, gfp_t gfp)
{
	struct ccupdate_struct *new;
	int i;

	new = kzalloc(sizeof(*new) + nr_cpu_ids * sizeof(struct array_cache *),
		      gfp);
	if (!new)
		return -ENOMEM;

	for_each_online_cpu(i) {
		new->new[i] = alloc_arraycache(cpu_to_mem(i), limit,
						batchcount, gfp);
		if (!new->new[i]) {
			for (i--; i >= 0; i--)
				kfree(new->new[i]);
			kfree(new);
			return -ENOMEM;
		}
	}
	new->cachep = cachep;

	on_each_cpu(do_ccupdate_local, (void *)new, 1);

	check_irq_on();
	cachep->batchcount = batchcount;
	cachep->limit = limit;
	cachep->shared = shared;

	for_each_online_cpu(i) {
		struct array_cache *ccold = new->new[i];
		if (!ccold)
			continue;
		spin_lock_irq(&cachep->nodelists[cpu_to_mem(i)]->list_lock);
		free_block(cachep, ccold->entry, ccold->avail, cpu_to_mem(i));
		spin_unlock_irq(&cachep->nodelists[cpu_to_mem(i)]->list_lock);
		kfree(ccold);
	}
	kfree(new);
	return alloc_kmemlist(cachep, gfp);
}

static int enable_cpucache(struct kmem_cache *cachep, gfp_t gfp)
{
	int err;
	int limit, shared;

	if (cachep->buffer_size > 131072)
		limit = 1;
	else if (cachep->buffer_size > PAGE_SIZE)
		limit = 8;
	else if (cachep->buffer_size > 1024)
		limit = 24;
	else if (cachep->buffer_size > 256)
		limit = 54;
	else
		limit = 120;

	shared = 0;
	if (cachep->buffer_size <= PAGE_SIZE && num_possible_cpus() > 1)
		shared = 8;

#if DEBUG
	if (limit > 32)
		limit = 32;
#endif
	err = do_tune_cpucache(cachep, limit, (limit + 1) / 2, shared, gfp);
	if (err)
		printk(KERN_ERR "enable_cpucache failed for %s, error %d.\n",
		       cachep->name, -err);
	return err;
}

static void drain_array(struct kmem_cache *cachep, struct kmem_list3 *l3,
			 struct array_cache *ac, int force, int node)
{
	int tofree;

	if (!ac || !ac->avail)
		return;
	if (ac->touched && !force) {
		ac->touched = 0;
	} else {
		spin_lock_irq(&l3->list_lock);
		if (ac->avail) {
			tofree = force ? ac->avail : (ac->limit + 4) / 5;
			if (tofree > ac->avail)
				tofree = (ac->avail + 1) / 2;
			free_block(cachep, ac->entry, tofree, node);
			ac->avail -= tofree;
			memmove(ac->entry, &(ac->entry[tofree]),
				sizeof(void *) * ac->avail);
		}
		spin_unlock_irq(&l3->list_lock);
	}
}

static void cache_reap(struct work_struct *w)
{
	struct kmem_cache *searchp;
	struct kmem_list3 *l3;
	int node = numa_mem_id();
	struct delayed_work *work = to_delayed_work(w);

	if (!mutex_trylock(&cache_chain_mutex))
		
		goto out;

	list_for_each_entry(searchp, &cache_chain, next) {
		check_irq_on();

		l3 = searchp->nodelists[node];

		reap_alien(searchp, l3);

		drain_array(searchp, l3, cpu_cache_get(searchp), 0, node);

		if (time_after(l3->next_reap, jiffies))
			goto next;

		l3->next_reap = jiffies + REAPTIMEOUT_LIST3;

		drain_array(searchp, l3, l3->shared, 0, node);

		if (l3->free_touched)
			l3->free_touched = 0;
		else {
			int freed;

			freed = drain_freelist(searchp, l3, (l3->free_limit +
				5 * searchp->num - 1) / (5 * searchp->num));
			STATS_ADD_REAPED(searchp, freed);
		}
next:
		cond_resched();
	}
	check_irq_on();
	mutex_unlock(&cache_chain_mutex);
	next_reap_node();
out:
	
	schedule_delayed_work(work, round_jiffies_relative(REAPTIMEOUT_CPUC));
}

#ifdef CONFIG_SLABINFO

static void print_slabinfo_header(struct seq_file *m)
{
#if STATS
	seq_puts(m, "slabinfo - version: 2.1 (statistics)\n");
#else
	seq_puts(m, "slabinfo - version: 2.1\n");
#endif
	seq_puts(m, "# name            <active_objs> <num_objs> <objsize> "
		 "<objperslab> <pagesperslab>");
	seq_puts(m, " : tunables <limit> <batchcount> <sharedfactor>");
	seq_puts(m, " : slabdata <active_slabs> <num_slabs> <sharedavail>");
#if STATS
	seq_puts(m, " : globalstat <listallocs> <maxobjs> <grown> <reaped> "
		 "<error> <maxfreeable> <nodeallocs> <remotefrees> <alienoverflow>");
	seq_puts(m, " : cpustat <allochit> <allocmiss> <freehit> <freemiss>");
#endif
	seq_putc(m, '\n');
}

static void *s_start(struct seq_file *m, loff_t *pos)
{
	loff_t n = *pos;

	mutex_lock(&cache_chain_mutex);
	if (!n)
		print_slabinfo_header(m);

	return seq_list_start(&cache_chain, *pos);
}

static void *s_next(struct seq_file *m, void *p, loff_t *pos)
{
	return seq_list_next(p, &cache_chain, pos);
}

static void s_stop(struct seq_file *m, void *p)
{
	mutex_unlock(&cache_chain_mutex);
}

static int s_show(struct seq_file *m, void *p)
{
	struct kmem_cache *cachep = list_entry(p, struct kmem_cache, next);
	struct slab *slabp;
	unsigned long active_objs;
	unsigned long num_objs;
	unsigned long active_slabs = 0;
	unsigned long num_slabs, free_objects = 0, shared_avail = 0;
	const char *name;
	char *error = NULL;
	int node;
	struct kmem_list3 *l3;

	active_objs = 0;
	num_slabs = 0;
	for_each_online_node(node) {
		l3 = cachep->nodelists[node];
		if (!l3)
			continue;

		check_irq_on();
		spin_lock_irq(&l3->list_lock);

		list_for_each_entry(slabp, &l3->slabs_full, list) {
			if (slabp->inuse != cachep->num && !error)
				error = "slabs_full accounting error";
			active_objs += cachep->num;
			active_slabs++;
		}
		list_for_each_entry(slabp, &l3->slabs_partial, list) {
			if (slabp->inuse == cachep->num && !error)
				error = "slabs_partial inuse accounting error";
			if (!slabp->inuse && !error)
				error = "slabs_partial/inuse accounting error";
			active_objs += slabp->inuse;
			active_slabs++;
		}
		list_for_each_entry(slabp, &l3->slabs_free, list) {
			if (slabp->inuse && !error)
				error = "slabs_free/inuse accounting error";
			num_slabs++;
		}
		free_objects += l3->free_objects;
		if (l3->shared)
			shared_avail += l3->shared->avail;

		spin_unlock_irq(&l3->list_lock);
	}
	num_slabs += active_slabs;
	num_objs = num_slabs * cachep->num;
	if (num_objs - active_objs != free_objects && !error)
		error = "free_objects accounting error";

	name = cachep->name;
	if (error)
		printk(KERN_ERR "slab: cache %s error: %s\n", name, error);

	seq_printf(m, "%-17s %6lu %6lu %6u %4u %4d",
		   name, active_objs, num_objs, cachep->buffer_size,
		   cachep->num, (1 << cachep->gfporder));
	seq_printf(m, " : tunables %4u %4u %4u",
		   cachep->limit, cachep->batchcount, cachep->shared);
	seq_printf(m, " : slabdata %6lu %6lu %6lu",
		   active_slabs, num_slabs, shared_avail);
#if STATS
	{			
		unsigned long high = cachep->high_mark;
		unsigned long allocs = cachep->num_allocations;
		unsigned long grown = cachep->grown;
		unsigned long reaped = cachep->reaped;
		unsigned long errors = cachep->errors;
		unsigned long max_freeable = cachep->max_freeable;
		unsigned long node_allocs = cachep->node_allocs;
		unsigned long node_frees = cachep->node_frees;
		unsigned long overflows = cachep->node_overflow;

		seq_printf(m, " : globalstat %7lu %6lu %5lu %4lu "
			   "%4lu %4lu %4lu %4lu %4lu",
			   allocs, high, grown,
			   reaped, errors, max_freeable, node_allocs,
			   node_frees, overflows);
	}
	
	{
		unsigned long allochit = atomic_read(&cachep->allochit);
		unsigned long allocmiss = atomic_read(&cachep->allocmiss);
		unsigned long freehit = atomic_read(&cachep->freehit);
		unsigned long freemiss = atomic_read(&cachep->freemiss);

		seq_printf(m, " : cpustat %6lu %6lu %6lu %6lu",
			   allochit, allocmiss, freehit, freemiss);
	}
#endif
	seq_putc(m, '\n');
	return 0;
}


static const struct seq_operations slabinfo_op = {
	.start = s_start,
	.next = s_next,
	.stop = s_stop,
	.show = s_show,
};

#define MAX_SLABINFO_WRITE 128
static ssize_t slabinfo_write(struct file *file, const char __user *buffer,
		       size_t count, loff_t *ppos)
{
	char kbuf[MAX_SLABINFO_WRITE + 1], *tmp;
	int limit, batchcount, shared, res;
	struct kmem_cache *cachep;

	if (count > MAX_SLABINFO_WRITE)
		return -EINVAL;
	if (copy_from_user(&kbuf, buffer, count))
		return -EFAULT;
	kbuf[MAX_SLABINFO_WRITE] = '\0';

	tmp = strchr(kbuf, ' ');
	if (!tmp)
		return -EINVAL;
	*tmp = '\0';
	tmp++;
	if (sscanf(tmp, " %d %d %d", &limit, &batchcount, &shared) != 3)
		return -EINVAL;

	
	mutex_lock(&cache_chain_mutex);
	res = -EINVAL;
	list_for_each_entry(cachep, &cache_chain, next) {
		if (!strcmp(cachep->name, kbuf)) {
			if (limit < 1 || batchcount < 1 ||
					batchcount > limit || shared < 0) {
				res = 0;
			} else {
				res = do_tune_cpucache(cachep, limit,
						       batchcount, shared,
						       GFP_KERNEL);
			}
			break;
		}
	}
	mutex_unlock(&cache_chain_mutex);
	if (res >= 0)
		res = count;
	return res;
}

static int slabinfo_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &slabinfo_op);
}

static const struct file_operations proc_slabinfo_operations = {
	.open		= slabinfo_open,
	.read		= seq_read,
	.write		= slabinfo_write,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#ifdef CONFIG_DEBUG_SLAB_LEAK

static void *leaks_start(struct seq_file *m, loff_t *pos)
{
	mutex_lock(&cache_chain_mutex);
	return seq_list_start(&cache_chain, *pos);
}

static inline int add_caller(unsigned long *n, unsigned long v)
{
	unsigned long *p;
	int l;
	if (!v)
		return 1;
	l = n[1];
	p = n + 2;
	while (l) {
		int i = l/2;
		unsigned long *q = p + 2 * i;
		if (*q == v) {
			q[1]++;
			return 1;
		}
		if (*q > v) {
			l = i;
		} else {
			p = q + 2;
			l -= i + 1;
		}
	}
	if (++n[1] == n[0])
		return 0;
	memmove(p + 2, p, n[1] * 2 * sizeof(unsigned long) - ((void *)p - (void *)n));
	p[0] = v;
	p[1] = 1;
	return 1;
}

static void handle_slab(unsigned long *n, struct kmem_cache *c, struct slab *s)
{
	void *p;
	int i;
	if (n[0] == n[1])
		return;
	for (i = 0, p = s->s_mem; i < c->num; i++, p += c->buffer_size) {
		if (slab_bufctl(s)[i] != BUFCTL_ACTIVE)
			continue;
		if (!add_caller(n, (unsigned long)*dbg_userword(c, p)))
			return;
	}
}

static void show_symbol(struct seq_file *m, unsigned long address)
{
#ifdef CONFIG_KALLSYMS
	unsigned long offset, size;
	char modname[MODULE_NAME_LEN], name[KSYM_NAME_LEN];

	if (lookup_symbol_attrs(address, &size, &offset, modname, name) == 0) {
		seq_printf(m, "%s+%#lx/%#lx", name, offset, size);
		if (modname[0])
			seq_printf(m, " [%s]", modname);
		return;
	}
#endif
	seq_printf(m, "%p", (void *)address);
}

static int leaks_show(struct seq_file *m, void *p)
{
	struct kmem_cache *cachep = list_entry(p, struct kmem_cache, next);
	struct slab *slabp;
	struct kmem_list3 *l3;
	const char *name;
	unsigned long *n = m->private;
	int node;
	int i;

	if (!(cachep->flags & SLAB_STORE_USER))
		return 0;
	if (!(cachep->flags & SLAB_RED_ZONE))
		return 0;

	

	n[1] = 0;

	for_each_online_node(node) {
		l3 = cachep->nodelists[node];
		if (!l3)
			continue;

		check_irq_on();
		spin_lock_irq(&l3->list_lock);

		list_for_each_entry(slabp, &l3->slabs_full, list)
			handle_slab(n, cachep, slabp);
		list_for_each_entry(slabp, &l3->slabs_partial, list)
			handle_slab(n, cachep, slabp);
		spin_unlock_irq(&l3->list_lock);
	}
	name = cachep->name;
	if (n[0] == n[1]) {
		
		mutex_unlock(&cache_chain_mutex);
		m->private = kzalloc(n[0] * 4 * sizeof(unsigned long), GFP_KERNEL);
		if (!m->private) {
			
			m->private = n;
			mutex_lock(&cache_chain_mutex);
			return -ENOMEM;
		}
		*(unsigned long *)m->private = n[0] * 2;
		kfree(n);
		mutex_lock(&cache_chain_mutex);
		
		m->count = m->size;
		return 0;
	}
	for (i = 0; i < n[1]; i++) {
		seq_printf(m, "%s: %lu ", name, n[2*i+3]);
		show_symbol(m, n[2*i+2]);
		seq_putc(m, '\n');
	}

	return 0;
}

static const struct seq_operations slabstats_op = {
	.start = leaks_start,
	.next = s_next,
	.stop = s_stop,
	.show = leaks_show,
};

static int slabstats_open(struct inode *inode, struct file *file)
{
	unsigned long *n = kzalloc(PAGE_SIZE, GFP_KERNEL);
	int ret = -ENOMEM;
	if (n) {
		ret = seq_open(file, &slabstats_op);
		if (!ret) {
			struct seq_file *m = file->private_data;
			*n = PAGE_SIZE / (2 * sizeof(unsigned long));
			m->private = n;
			n = NULL;
		}
		kfree(n);
	}
	return ret;
}

static const struct file_operations proc_slabstats_operations = {
	.open		= slabstats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release_private,
};
#endif

static int __init slab_proc_init(void)
{
	proc_create("slabinfo",S_IWUSR|S_IRUSR,NULL,&proc_slabinfo_operations);
#ifdef CONFIG_DEBUG_SLAB_LEAK
	proc_create("slab_allocators", 0, NULL, &proc_slabstats_operations);
#endif
	return 0;
}
module_init(slab_proc_init);
#endif

size_t ksize(const void *objp)
{
	BUG_ON(!objp);
	if (unlikely(objp == ZERO_SIZE_PTR))
		return 0;

	return obj_size(virt_to_cache(objp));
}
EXPORT_SYMBOL(ksize);
