/*
 *  linux/mm/memory_hotplug.c
 *
 *  Copyright (C)
 */

#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/bootmem.h>
#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/pagevec.h>
#include <linux/writeback.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/cpu.h>
#include <linux/memory.h>
#include <linux/memory_hotplug.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/migrate.h>
#include <linux/page-isolation.h>
#include <linux/pfn.h>
#include <linux/suspend.h>
#include <linux/mm_inline.h>
#include <linux/firmware-map.h>

#include <asm/tlbflush.h>

#include "internal.h"


static void generic_online_page(struct page *page);

static online_page_callback_t online_page_callback = generic_online_page;

DEFINE_MUTEX(mem_hotplug_mutex);

void lock_memory_hotplug(void)
{
	mutex_lock(&mem_hotplug_mutex);

	
	lock_system_sleep();
}

void unlock_memory_hotplug(void)
{
	unlock_system_sleep();
	mutex_unlock(&mem_hotplug_mutex);
}


static struct resource *register_memory_resource(u64 start, u64 size)
{
	struct resource *res;
	res = kzalloc(sizeof(struct resource), GFP_KERNEL);
	BUG_ON(!res);

	res->name = "System RAM";
	res->start = start;
	res->end = start + size - 1;
	res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;
	if (request_resource(&iomem_resource, res) < 0) {
		printk("System RAM resource %llx - %llx cannot be added\n",
		(unsigned long long)res->start, (unsigned long long)res->end);
		kfree(res);
		res = NULL;
	}
	return res;
}

static void release_memory_resource(struct resource *res)
{
	if (!res)
		return;
	release_resource(res);
	kfree(res);
	return;
}

#ifdef CONFIG_MEMORY_HOTPLUG_SPARSE
#ifndef CONFIG_SPARSEMEM_VMEMMAP
static void get_page_bootmem(unsigned long info,  struct page *page,
			     unsigned long type)
{
	page->lru.next = (struct list_head *) type;
	SetPagePrivate(page);
	set_page_private(page, info);
	atomic_inc(&page->_count);
}

void __ref put_page_bootmem(struct page *page)
{
	unsigned long type;

	type = (unsigned long) page->lru.next;
	BUG_ON(type < MEMORY_HOTPLUG_MIN_BOOTMEM_TYPE ||
	       type > MEMORY_HOTPLUG_MAX_BOOTMEM_TYPE);

	if (atomic_dec_return(&page->_count) == 1) {
		ClearPagePrivate(page);
		set_page_private(page, 0);
		INIT_LIST_HEAD(&page->lru);
		__free_pages_bootmem(page, 0);
	}

}

static void register_page_bootmem_info_section(unsigned long start_pfn)
{
	unsigned long *usemap, mapsize, page_mapsize, section_nr, i, j;
	struct mem_section *ms;
	struct page *page, *memmap, *page_page;
	int memmap_page_valid;

	if (!pfn_valid(start_pfn))
		return;

	section_nr = pfn_to_section_nr(start_pfn);
	ms = __nr_to_section(section_nr);

	
	memmap = sparse_decode_mem_map(ms->section_mem_map, section_nr);

	page = virt_to_page(memmap);
	mapsize = sizeof(struct page) * PAGES_PER_SECTION;
	mapsize = PAGE_ALIGN(mapsize) >> PAGE_SHIFT;

	page_mapsize = PAGE_SIZE/sizeof(struct page);

	
	for (i = 0; i < mapsize; i++, page++) {
		memmap_page_valid = 0;
		page_page = __va(page_to_pfn(page) << PAGE_SHIFT);
		for (j = 0; j < page_mapsize; j++, page_page++) {
			if (early_pfn_valid(page_to_pfn(page_page))) {
				memmap_page_valid = 1;
				break;
			}
		}
		if (memmap_page_valid)
			get_page_bootmem(section_nr, page, SECTION_INFO);
	}

	usemap = __nr_to_section(section_nr)->pageblock_flags;
	page = virt_to_page(usemap);

	mapsize = PAGE_ALIGN(usemap_size()) >> PAGE_SHIFT;

	for (i = 0; i < mapsize; i++, page++)
		get_page_bootmem(section_nr, page, MIX_SECTION_INFO);

}

void register_page_bootmem_info_node(struct pglist_data *pgdat)
{
	unsigned long i, pfn, end_pfn, nr_pages;
	int node = pgdat->node_id;
	struct page *page;
	struct zone *zone;

	nr_pages = PAGE_ALIGN(sizeof(struct pglist_data)) >> PAGE_SHIFT;
	page = virt_to_page(pgdat);

	for (i = 0; i < nr_pages; i++, page++)
		get_page_bootmem(node, page, NODE_INFO);

	zone = &pgdat->node_zones[0];
	for (; zone < pgdat->node_zones + MAX_NR_ZONES - 1; zone++) {
		if (zone->wait_table) {
			nr_pages = zone->wait_table_hash_nr_entries
				* sizeof(wait_queue_head_t);
			nr_pages = PAGE_ALIGN(nr_pages) >> PAGE_SHIFT;
			page = virt_to_page(zone->wait_table);

			for (i = 0; i < nr_pages; i++, page++)
				get_page_bootmem(node, page, NODE_INFO);
		}
	}

	pfn = pgdat->node_start_pfn;
	end_pfn = pfn + pgdat->node_spanned_pages;

	
	for (; pfn < end_pfn; pfn += PAGES_PER_SECTION)
		register_page_bootmem_info_section(pfn);

}
#endif 

static void grow_zone_span(struct zone *zone, unsigned long start_pfn,
			   unsigned long end_pfn)
{
	unsigned long old_zone_end_pfn;

	zone_span_writelock(zone);

	old_zone_end_pfn = zone->zone_start_pfn + zone->spanned_pages;
	if (start_pfn < zone->zone_start_pfn)
		zone->zone_start_pfn = start_pfn;

	zone->spanned_pages = max(old_zone_end_pfn, end_pfn) -
				zone->zone_start_pfn;

	zone_span_writeunlock(zone);
}

static void grow_pgdat_span(struct pglist_data *pgdat, unsigned long start_pfn,
			    unsigned long end_pfn)
{
	unsigned long old_pgdat_end_pfn =
		pgdat->node_start_pfn + pgdat->node_spanned_pages;

	if (start_pfn < pgdat->node_start_pfn)
		pgdat->node_start_pfn = start_pfn;

	pgdat->node_spanned_pages = max(old_pgdat_end_pfn, end_pfn) -
					pgdat->node_start_pfn;
}

static int __meminit __add_zone(struct zone *zone, unsigned long phys_start_pfn)
{
	struct pglist_data *pgdat = zone->zone_pgdat;
	int nr_pages = PAGES_PER_SECTION;
	int nid = pgdat->node_id;
	int zone_type;
	unsigned long flags;

	zone_type = zone - pgdat->node_zones;
	if (!zone->wait_table) {
		int ret;

		ret = init_currently_empty_zone(zone, phys_start_pfn,
						nr_pages, MEMMAP_HOTPLUG);
		if (ret)
			return ret;
	}
	pgdat_resize_lock(zone->zone_pgdat, &flags);
	grow_zone_span(zone, phys_start_pfn, phys_start_pfn + nr_pages);
	grow_pgdat_span(zone->zone_pgdat, phys_start_pfn,
			phys_start_pfn + nr_pages);
	pgdat_resize_unlock(zone->zone_pgdat, &flags);
	memmap_init_zone(nr_pages, nid, zone_type,
			 phys_start_pfn, MEMMAP_HOTPLUG);
	return 0;
}

static int __meminit __add_section(int nid, struct zone *zone,
					unsigned long phys_start_pfn)
{
	int nr_pages = PAGES_PER_SECTION;
	int ret;

	if (pfn_valid(phys_start_pfn))
		return -EEXIST;

	ret = sparse_add_one_section(zone, phys_start_pfn, nr_pages);

	if (ret < 0)
		return ret;

	ret = __add_zone(zone, phys_start_pfn);

	if (ret < 0)
		return ret;

	return register_new_memory(nid, __pfn_to_section(phys_start_pfn));
}

#ifdef CONFIG_SPARSEMEM_VMEMMAP
static int __remove_section(struct zone *zone, struct mem_section *ms)
{
	return -EBUSY;
}
#else
static int __remove_section(struct zone *zone, struct mem_section *ms)
{
	unsigned long flags;
	struct pglist_data *pgdat = zone->zone_pgdat;
	int ret = -EINVAL;

	if (!valid_section(ms))
		return ret;

	ret = unregister_memory_section(ms);
	if (ret)
		return ret;

	pgdat_resize_lock(pgdat, &flags);
	sparse_remove_one_section(zone, ms);
	pgdat_resize_unlock(pgdat, &flags);
	return 0;
}
#endif

int __ref __add_pages(int nid, struct zone *zone, unsigned long phys_start_pfn,
			unsigned long nr_pages)
{
	unsigned long i;
	int err = 0;
	int start_sec, end_sec;
	
	start_sec = pfn_to_section_nr(phys_start_pfn);
	end_sec = pfn_to_section_nr(phys_start_pfn + nr_pages - 1);

	for (i = start_sec; i <= end_sec; i++) {
		err = __add_section(nid, zone, i << PFN_SECTION_SHIFT);

		if (err && (err != -EEXIST))
			break;
		err = 0;
	}

	return err;
}
EXPORT_SYMBOL_GPL(__add_pages);

int __remove_pages(struct zone *zone, unsigned long phys_start_pfn,
		 unsigned long nr_pages)
{
	unsigned long i, ret = 0;
	int sections_to_remove;

	BUG_ON(phys_start_pfn & ~PAGE_SECTION_MASK);
	BUG_ON(nr_pages % PAGES_PER_SECTION);

	sections_to_remove = nr_pages / PAGES_PER_SECTION;
	for (i = 0; i < sections_to_remove; i++) {
		unsigned long pfn = phys_start_pfn + i*PAGES_PER_SECTION;
		release_mem_region(pfn << PAGE_SHIFT,
				   PAGES_PER_SECTION << PAGE_SHIFT);
		ret = __remove_section(zone, __pfn_to_section(pfn));
		if (ret)
			break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(__remove_pages);

int set_online_page_callback(online_page_callback_t callback)
{
	int rc = -EINVAL;

	lock_memory_hotplug();

	if (online_page_callback == generic_online_page) {
		online_page_callback = callback;
		rc = 0;
	}

	unlock_memory_hotplug();

	return rc;
}
EXPORT_SYMBOL_GPL(set_online_page_callback);

int restore_online_page_callback(online_page_callback_t callback)
{
	int rc = -EINVAL;

	lock_memory_hotplug();

	if (online_page_callback == callback) {
		online_page_callback = generic_online_page;
		rc = 0;
	}

	unlock_memory_hotplug();

	return rc;
}
EXPORT_SYMBOL_GPL(restore_online_page_callback);

void __online_page_set_limits(struct page *page)
{
	unsigned long pfn = page_to_pfn(page);

	totalram_pages++;
#ifdef CONFIG_FIX_MOVABLE_ZONE
	if (zone_idx(page_zone(page)) != ZONE_MOVABLE)
		total_unmovable_pages++;
#endif
	if (pfn >= num_physpages)
		num_physpages = pfn + 1;
}
EXPORT_SYMBOL_GPL(__online_page_set_limits);

void __online_page_increment_counters(struct page *page)
{
	totalram_pages++;

#ifdef CONFIG_HIGHMEM
	if (PageHighMem(page))
		totalhigh_pages++;
#endif
}
EXPORT_SYMBOL_GPL(__online_page_increment_counters);

void __online_page_free(struct page *page)
{
	ClearPageReserved(page);
	init_page_count(page);
	__free_page(page);
}
EXPORT_SYMBOL_GPL(__online_page_free);

static void generic_online_page(struct page *page)
{
	__online_page_set_limits(page);
	__online_page_increment_counters(page);
	__online_page_free(page);
}

static int online_pages_range(unsigned long start_pfn, unsigned long nr_pages,
			void *arg)
{
	unsigned long i;
	unsigned long onlined_pages = *(unsigned long *)arg;
	struct page *page;
	if (PageReserved(pfn_to_page(start_pfn)))
		for (i = 0; i < nr_pages; i++) {
			page = pfn_to_page(start_pfn + i);
			(*online_page_callback)(page);
			onlined_pages++;
		}
	*(unsigned long *)arg = onlined_pages;
	return 0;
}


int __ref online_pages(unsigned long pfn, unsigned long nr_pages)
{
	unsigned long onlined_pages = 0;
	struct zone *zone;
	int need_zonelists_rebuild = 0;
	int nid;
	int ret;
	struct memory_notify arg;

	lock_memory_hotplug();
	arg.start_pfn = pfn;
	arg.nr_pages = nr_pages;
	arg.status_change_nid = -1;

	nid = page_to_nid(pfn_to_page(pfn));
	if (node_present_pages(nid) == 0)
		arg.status_change_nid = nid;

	ret = memory_notify(MEM_GOING_ONLINE, &arg);
	ret = notifier_to_errno(ret);
	if (ret) {
		memory_notify(MEM_CANCEL_ONLINE, &arg);
		unlock_memory_hotplug();
		return ret;
	}
	zone = page_zone(pfn_to_page(pfn));
	mutex_lock(&zonelists_mutex);
	if (!populated_zone(zone))
		need_zonelists_rebuild = 1;

	ret = walk_system_ram_range(pfn, nr_pages, &onlined_pages,
		online_pages_range);
	if (ret) {
		mutex_unlock(&zonelists_mutex);
		printk(KERN_DEBUG "online_pages %lx at %lx failed\n",
			nr_pages, pfn);
		memory_notify(MEM_CANCEL_ONLINE, &arg);
		unlock_memory_hotplug();
		return ret;
	}

	zone->present_pages += onlined_pages;
	zone->zone_pgdat->node_present_pages += onlined_pages;
	drain_all_pages();
	if (need_zonelists_rebuild)
		build_all_zonelists(zone);
	else
		zone_pcp_update(zone);

	mutex_unlock(&zonelists_mutex);

	init_per_zone_wmark_min();

	if (onlined_pages) {
		kswapd_run(zone_to_nid(zone));
		node_set_state(zone_to_nid(zone), N_HIGH_MEMORY);
	}

	vm_total_pages = nr_free_pagecache_pages();

	writeback_set_ratelimit();

	if (onlined_pages)
		memory_notify(MEM_ONLINE, &arg);
	unlock_memory_hotplug();

	return 0;
}
#endif 

static pg_data_t __ref *hotadd_new_pgdat(int nid, u64 start)
{
	struct pglist_data *pgdat;
	unsigned long zones_size[MAX_NR_ZONES] = {0};
	unsigned long zholes_size[MAX_NR_ZONES] = {0};
	unsigned long start_pfn = start >> PAGE_SHIFT;

	pgdat = arch_alloc_nodedata(nid);
	if (!pgdat)
		return NULL;

	arch_refresh_nodedata(nid, pgdat);

	

	
	free_area_init_node(nid, zones_size, start_pfn, zholes_size);

	mutex_lock(&zonelists_mutex);
	build_all_zonelists(NULL);
	mutex_unlock(&zonelists_mutex);

	return pgdat;
}

static void rollback_node_hotadd(int nid, pg_data_t *pgdat)
{
	arch_refresh_nodedata(nid, NULL);
	arch_free_nodedata(pgdat);
	return;
}


int mem_online_node(int nid)
{
	pg_data_t	*pgdat;
	int	ret;

	lock_memory_hotplug();
	pgdat = hotadd_new_pgdat(nid, 0);
	if (!pgdat) {
		ret = -ENOMEM;
		goto out;
	}
	node_set_online(nid);
	ret = register_one_node(nid);
	BUG_ON(ret);

out:
	unlock_memory_hotplug();
	return ret;
}

int __ref add_memory(int nid, u64 start, u64 size)
{
	pg_data_t *pgdat = NULL;
	int new_pgdat = 0;
	struct resource *res;
	int ret;

	lock_memory_hotplug();

	res = register_memory_resource(start, size);
	ret = -EEXIST;
	if (!res)
		goto out;

	if (!node_online(nid)) {
		pgdat = hotadd_new_pgdat(nid, start);
		ret = -ENOMEM;
		if (!pgdat)
			goto out;
		new_pgdat = 1;
	}

	
	ret = arch_add_memory(nid, start, size);

	if (ret < 0)
		goto error;

	
	node_set_online(nid);

	if (new_pgdat) {
		ret = register_one_node(nid);
		BUG_ON(ret);
	}

	
	firmware_map_add_hotplug(start, start + size, "System RAM");

	goto out;

error:
	
	if (new_pgdat)
		rollback_node_hotadd(nid, pgdat);
	if (res)
		release_memory_resource(res);

out:
	unlock_memory_hotplug();
	return ret;
}
EXPORT_SYMBOL_GPL(add_memory);

int __ref physical_remove_memory(u64 start, u64 size)
{
	int ret;
	struct resource *res, *res_old;
	res = kzalloc(sizeof(struct resource), GFP_KERNEL);
	BUG_ON(!res);

	ret = arch_physical_remove_memory(start, size);
	if (!ret) {
		kfree(res);
		return 0;
	}

	res->name = "System RAM";
	res->start = start;
	res->end = start + size - 1;
	res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;

	res_old = locate_resource(&iomem_resource, res);
	if (res_old) {
		release_resource(res_old);
		if (PageSlab(virt_to_head_page(res_old)))
			kfree(res_old);
	}
	kfree(res);

	return ret;
}
EXPORT_SYMBOL_GPL(physical_remove_memory);

int __ref physical_active_memory(u64 start, u64 size)
{
	int ret;

	ret = arch_physical_active_memory(start, size);
	return ret;
}
EXPORT_SYMBOL_GPL(physical_active_memory);

int __ref physical_low_power_memory(u64 start, u64 size)
{
	int ret;

	ret = arch_physical_low_power_memory(start, size);
	return ret;
}
EXPORT_SYMBOL_GPL(physical_low_power_memory);

#ifdef CONFIG_MEMORY_HOTREMOVE
static inline int pageblock_free(struct page *page)
{
	return PageBuddy(page) && page_order(page) >= pageblock_order;
}

static struct page *next_active_pageblock(struct page *page)
{
	
	BUG_ON(page_to_pfn(page) & (pageblock_nr_pages - 1));

	
	if (pageblock_free(page)) {
		int order;
		
		order = page_order(page);
		if ((order < MAX_ORDER) && (order >= pageblock_order))
			return page + (1 << order);
	}

	return page + pageblock_nr_pages;
}

int is_mem_section_removable(unsigned long start_pfn, unsigned long nr_pages)
{
	struct page *page = pfn_to_page(start_pfn);
	struct page *end_page = page + nr_pages;

	
	for (; page < end_page; page = next_active_pageblock(page)) {
		if (!is_pageblock_removable_nolock(page))
			return 0;
		cond_resched();
	}

	
	return 1;
}

static int test_pages_in_a_zone(unsigned long start_pfn, unsigned long end_pfn)
{
	unsigned long pfn;
	struct zone *zone = NULL;
	struct page *page;
	int i;
	for (pfn = start_pfn;
	     pfn < end_pfn;
	     pfn += MAX_ORDER_NR_PAGES) {
		i = 0;
		
		while ((i < MAX_ORDER_NR_PAGES) && !pfn_valid_within(pfn + i))
			i++;
		if (i == MAX_ORDER_NR_PAGES)
			continue;
		page = pfn_to_page(pfn + i);
		if (zone && page_zone(page) != zone)
			return 0;
		zone = page_zone(page);
	}
	return 1;
}

static unsigned long scan_lru_pages(unsigned long start, unsigned long end)
{
	unsigned long pfn;
	struct page *page;
	for (pfn = start; pfn < end; pfn++) {
		if (pfn_valid(pfn)) {
			page = pfn_to_page(pfn);
			if (PageLRU(page))
				return pfn;
		}
	}
	return 0;
}

static struct page *
hotremove_migrate_alloc(struct page *page, unsigned long private, int **x)
{
	
	return alloc_page(GFP_HIGHUSER_MOVABLE | __GFP_NORETRY | __GFP_NOWARN |
				__GFP_NOMEMALLOC);
}

#define NR_OFFLINE_AT_ONCE_PAGES	(256)
static int
do_migrate_range(unsigned long start_pfn, unsigned long end_pfn)
{
	unsigned long pfn;
	struct page *page;
	int move_pages = NR_OFFLINE_AT_ONCE_PAGES;
	int not_managed = 0;
	int ret = 0;
	LIST_HEAD(source);

	for (pfn = start_pfn; pfn < end_pfn && move_pages > 0; pfn++) {
		if (!pfn_valid(pfn))
			continue;
		page = pfn_to_page(pfn);
		if (!get_page_unless_zero(page))
			continue;
		ret = isolate_lru_page(page);
		if (!ret) { 
			put_page(page);
			list_add_tail(&page->lru, &source);
			move_pages--;
			inc_zone_page_state(page, NR_ISOLATED_ANON +
					    page_is_file_cache(page));

		} else {
#ifdef CONFIG_DEBUG_VM
			printk(KERN_ALERT "removing pfn %lx from LRU failed\n",
			       pfn);
			dump_page(page);
#endif
			put_page(page);
			if (page_count(page)) {
				not_managed++;
				ret = -EBUSY;
				break;
			}
		}
	}
	if (!list_empty(&source)) {
		if (not_managed) {
			putback_lru_pages(&source);
			goto out;
		}
		
		ret = migrate_pages(&source, hotremove_migrate_alloc, 0,
							true, MIGRATE_SYNC);
		if (ret)
			putback_lru_pages(&source);
	}
out:
	return ret;
}

static int
offline_isolated_pages_cb(unsigned long start, unsigned long nr_pages,
			void *data)
{
	__offline_isolated_pages(start, start + nr_pages);
	return 0;
}

static void
offline_isolated_pages(unsigned long start_pfn, unsigned long end_pfn)
{
	walk_system_ram_range(start_pfn, end_pfn - start_pfn, NULL,
				offline_isolated_pages_cb);
}

static int
check_pages_isolated_cb(unsigned long start_pfn, unsigned long nr_pages,
			void *data)
{
	int ret;
	long offlined = *(long *)data;
	ret = test_pages_isolated(start_pfn, start_pfn + nr_pages);
	offlined = nr_pages;
	if (!ret)
		*(long *)data += offlined;
	return ret;
}

static long
check_pages_isolated(unsigned long start_pfn, unsigned long end_pfn)
{
	long offlined = 0;
	int ret;

	ret = walk_system_ram_range(start_pfn, end_pfn - start_pfn, &offlined,
			check_pages_isolated_cb);
	if (ret < 0)
		offlined = (long)ret;
	return offlined;
}

static int __ref offline_pages(unsigned long start_pfn,
		  unsigned long end_pfn, unsigned long timeout)
{
	unsigned long pfn, nr_pages, expire;
	long offlined_pages;
	int ret, drain, retry_max, node;
	struct zone *zone;
	struct memory_notify arg;

	BUG_ON(start_pfn >= end_pfn);
	
	if (!IS_ALIGNED(start_pfn, pageblock_nr_pages))
		return -EINVAL;
	if (!IS_ALIGNED(end_pfn, pageblock_nr_pages))
		return -EINVAL;
	if (!test_pages_in_a_zone(start_pfn, end_pfn))
		return -EINVAL;

	lock_memory_hotplug();

	zone = page_zone(pfn_to_page(start_pfn));
	node = zone_to_nid(zone);
	nr_pages = end_pfn - start_pfn;

	
	ret = start_isolate_page_range(start_pfn, end_pfn, MIGRATE_MOVABLE);
	if (ret)
		goto out;

	arg.start_pfn = start_pfn;
	arg.nr_pages = nr_pages;
	arg.status_change_nid = -1;
	if (nr_pages >= node_present_pages(node))
		arg.status_change_nid = node;

	ret = memory_notify(MEM_GOING_OFFLINE, &arg);
	ret = notifier_to_errno(ret);
	if (ret)
		goto failed_removal;

	pfn = start_pfn;
	expire = jiffies + timeout;
	drain = 0;
	retry_max = 5;
repeat:
	
	ret = -EAGAIN;
	if (time_after(jiffies, expire))
		goto failed_removal;
	ret = -EINTR;
	if (signal_pending(current))
		goto failed_removal;
	ret = 0;
	if (drain) {
		lru_add_drain_all();
		cond_resched();
		drain_all_pages();
	}

	pfn = scan_lru_pages(start_pfn, end_pfn);
	if (pfn) { 
		ret = do_migrate_range(pfn, end_pfn);
		if (!ret) {
			drain = 1;
			goto repeat;
		} else {
			if (ret < 0)
				if (--retry_max == 0)
					goto failed_removal;
			yield();
			drain = 1;
			goto repeat;
		}
	}
	
	lru_add_drain_all();
	yield();
	
	drain_all_pages();
	
	offlined_pages = check_pages_isolated(start_pfn, end_pfn);
	if (offlined_pages < 0) {
		ret = -EBUSY;
		goto failed_removal;
	}
	printk(KERN_INFO "Offlined Pages %ld\n", offlined_pages);
	offline_isolated_pages(start_pfn, end_pfn);
	
	undo_isolate_page_range(start_pfn, end_pfn, MIGRATE_MOVABLE);
	
	if (offlined_pages > zone->present_pages)
		zone->present_pages = 0;
	else
		zone->present_pages -= offlined_pages;
	zone->zone_pgdat->node_present_pages -= offlined_pages;
	totalram_pages -= offlined_pages;

#ifdef CONFIG_FIX_MOVABLE_ZONE
	if (zone_idx(zone) != ZONE_MOVABLE)
		total_unmovable_pages -= offlined_pages;
#endif
	init_per_zone_wmark_min();

	if (!node_present_pages(node)) {
		node_clear_state(node, N_HIGH_MEMORY);
		kswapd_stop(node);
	}

	vm_total_pages = nr_free_pagecache_pages();
	writeback_set_ratelimit();

	memory_notify(MEM_OFFLINE, &arg);
	unlock_memory_hotplug();
	return 0;

failed_removal:
	printk(KERN_INFO "memory offlining %lx to %lx failed\n",
		start_pfn, end_pfn);
	memory_notify(MEM_CANCEL_OFFLINE, &arg);
	
	undo_isolate_page_range(start_pfn, end_pfn, MIGRATE_MOVABLE);

out:
	unlock_memory_hotplug();
	return ret;
}

int remove_memory(u64 start, u64 size)
{
	unsigned long start_pfn, end_pfn;

	start_pfn = PFN_DOWN(start);
	end_pfn = start_pfn + PFN_DOWN(size);
	return offline_pages(start_pfn, end_pfn, 120 * HZ);
}

#else
int remove_memory(u64 start, u64 size)
{
	return -EINVAL;
}
#endif 
EXPORT_SYMBOL_GPL(remove_memory);
