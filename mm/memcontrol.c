/* memcontrol.c - Memory Controller
 *
 * Copyright IBM Corporation, 2007
 * Author Balbir Singh <balbir@linux.vnet.ibm.com>
 *
 * Copyright 2007 OpenVZ SWsoft Inc
 * Author: Pavel Emelianov <xemul@openvz.org>
 *
 * Memory thresholds
 * Copyright (C) 2009 Nokia Corporation
 * Author: Kirill A. Shutemov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/res_counter.h>
#include <linux/memcontrol.h>
#include <linux/cgroup.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/pagemap.h>
#include <linux/smp.h>
#include <linux/page-flags.h>
#include <linux/backing-dev.h>
#include <linux/bit_spinlock.h>
#include <linux/rcupdate.h>
#include <linux/limits.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/spinlock.h>
#include <linux/eventfd.h>
#include <linux/sort.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <linux/mm_inline.h>
#include <linux/page_cgroup.h>
#include <linux/cpu.h>
#include <linux/oom.h>
#include "internal.h"
#include <net/sock.h>
#include <net/tcp_memcontrol.h>

#include <asm/uaccess.h>

#include <trace/events/vmscan.h>

struct cgroup_subsys mem_cgroup_subsys __read_mostly;
#define MEM_CGROUP_RECLAIM_RETRIES	5
struct mem_cgroup *root_mem_cgroup __read_mostly;

#ifdef CONFIG_CGROUP_MEM_RES_CTLR_SWAP
int do_swap_account __read_mostly;

#ifdef CONFIG_CGROUP_MEM_RES_CTLR_SWAP_ENABLED
static int really_do_swap_account __initdata = 1;
#else
static int really_do_swap_account __initdata = 0;
#endif

#else
#define do_swap_account		(0)
#endif


enum mem_cgroup_stat_index {
	MEM_CGROUP_STAT_CACHE, 	   
	MEM_CGROUP_STAT_RSS,	   
	MEM_CGROUP_STAT_FILE_MAPPED,  
	MEM_CGROUP_STAT_SWAPOUT, 
	MEM_CGROUP_STAT_DATA, 
	MEM_CGROUP_STAT_NSTATS,
};

enum mem_cgroup_events_index {
	MEM_CGROUP_EVENTS_PGPGIN,	
	MEM_CGROUP_EVENTS_PGPGOUT,	
	MEM_CGROUP_EVENTS_COUNT,	
	MEM_CGROUP_EVENTS_PGFAULT,	
	MEM_CGROUP_EVENTS_PGMAJFAULT,	
	MEM_CGROUP_EVENTS_NSTATS,
};
enum mem_cgroup_events_target {
	MEM_CGROUP_TARGET_THRESH,
	MEM_CGROUP_TARGET_SOFTLIMIT,
	MEM_CGROUP_TARGET_NUMAINFO,
	MEM_CGROUP_NTARGETS,
};
#define THRESHOLDS_EVENTS_TARGET (128)
#define SOFTLIMIT_EVENTS_TARGET (1024)
#define NUMAINFO_EVENTS_TARGET	(1024)

struct mem_cgroup_stat_cpu {
	long count[MEM_CGROUP_STAT_NSTATS];
	unsigned long events[MEM_CGROUP_EVENTS_NSTATS];
	unsigned long targets[MEM_CGROUP_NTARGETS];
};

struct mem_cgroup_reclaim_iter {
	
	int position;
	
	unsigned int generation;
};

struct mem_cgroup_per_zone {
	struct lruvec		lruvec;
	unsigned long		lru_size[NR_LRU_LISTS];

	struct mem_cgroup_reclaim_iter reclaim_iter[DEF_PRIORITY + 1];

	struct zone_reclaim_stat reclaim_stat;
	struct rb_node		tree_node;	
	unsigned long long	usage_in_excess;
						
	bool			on_tree;
	struct mem_cgroup	*memcg;		
						
};

struct mem_cgroup_per_node {
	struct mem_cgroup_per_zone zoneinfo[MAX_NR_ZONES];
};

struct mem_cgroup_lru_info {
	struct mem_cgroup_per_node *nodeinfo[MAX_NUMNODES];
};


struct mem_cgroup_tree_per_zone {
	struct rb_root rb_root;
	spinlock_t lock;
};

struct mem_cgroup_tree_per_node {
	struct mem_cgroup_tree_per_zone rb_tree_per_zone[MAX_NR_ZONES];
};

struct mem_cgroup_tree {
	struct mem_cgroup_tree_per_node *rb_tree_per_node[MAX_NUMNODES];
};

static struct mem_cgroup_tree soft_limit_tree __read_mostly;

struct mem_cgroup_threshold {
	struct eventfd_ctx *eventfd;
	u64 threshold;
};

struct mem_cgroup_threshold_ary {
	
	int current_threshold;
	
	unsigned int size;
	
	struct mem_cgroup_threshold entries[0];
};

struct mem_cgroup_thresholds {
	
	struct mem_cgroup_threshold_ary *primary;
	struct mem_cgroup_threshold_ary *spare;
};

struct mem_cgroup_eventfd_list {
	struct list_head list;
	struct eventfd_ctx *eventfd;
};

static void mem_cgroup_threshold(struct mem_cgroup *memcg);
static void mem_cgroup_oom_notify(struct mem_cgroup *memcg);

struct mem_cgroup {
	struct cgroup_subsys_state css;
	struct res_counter res;

	union {
		struct res_counter memsw;

		struct rcu_head rcu_freeing;
		struct work_struct work_freeing;
	};

	struct mem_cgroup_lru_info info;
	int last_scanned_node;
#if MAX_NUMNODES > 1
	nodemask_t	scan_nodes;
	atomic_t	numainfo_events;
	atomic_t	numainfo_updating;
#endif
	bool use_hierarchy;

	bool		oom_lock;
	atomic_t	under_oom;

	atomic_t	refcnt;

	int	swappiness;
	
	int		oom_kill_disable;

	
	bool		memsw_is_minimum;

	
	struct mutex thresholds_lock;

	
	struct mem_cgroup_thresholds thresholds;

	
	struct mem_cgroup_thresholds memsw_thresholds;

	
	struct list_head oom_notify;

	unsigned long 	move_charge_at_immigrate;
	atomic_t	moving_account;
	
	spinlock_t	move_lock;
	struct mem_cgroup_stat_cpu *stat;
	struct mem_cgroup_stat_cpu nocpu_base;
	spinlock_t pcp_counter_lock;

#ifdef CONFIG_INET
	struct tcp_memcontrol tcp_mem;
#endif
};

enum move_type {
	MOVE_CHARGE_TYPE_ANON,	
	MOVE_CHARGE_TYPE_FILE,	
	NR_MOVE_TYPE,
};

static struct move_charge_struct {
	spinlock_t	  lock; 
	struct mem_cgroup *from;
	struct mem_cgroup *to;
	unsigned long precharge;
	unsigned long moved_charge;
	unsigned long moved_swap;
	struct task_struct *moving_task;	
	wait_queue_head_t waitq;		
} mc = {
	.lock = __SPIN_LOCK_UNLOCKED(mc.lock),
	.waitq = __WAIT_QUEUE_HEAD_INITIALIZER(mc.waitq),
};

static bool move_anon(void)
{
	return test_bit(MOVE_CHARGE_TYPE_ANON,
					&mc.to->move_charge_at_immigrate);
}

static bool move_file(void)
{
	return test_bit(MOVE_CHARGE_TYPE_FILE,
					&mc.to->move_charge_at_immigrate);
}

#define	MEM_CGROUP_MAX_RECLAIM_LOOPS		(100)
#define	MEM_CGROUP_MAX_SOFT_LIMIT_RECLAIM_LOOPS	(2)

enum charge_type {
	MEM_CGROUP_CHARGE_TYPE_CACHE = 0,
	MEM_CGROUP_CHARGE_TYPE_MAPPED,
	MEM_CGROUP_CHARGE_TYPE_SHMEM,	
	MEM_CGROUP_CHARGE_TYPE_FORCE,	
	MEM_CGROUP_CHARGE_TYPE_SWAPOUT,	
	MEM_CGROUP_CHARGE_TYPE_DROP,	
	NR_CHARGE_TYPE,
};

#define _MEM			(0)
#define _MEMSWAP		(1)
#define _OOM_TYPE		(2)
#define MEMFILE_PRIVATE(x, val)	(((x) << 16) | (val))
#define MEMFILE_TYPE(val)	(((val) >> 16) & 0xffff)
#define MEMFILE_ATTR(val)	((val) & 0xffff)
#define OOM_CONTROL		(0)

#define MEM_CGROUP_RECLAIM_NOSWAP_BIT	0x0
#define MEM_CGROUP_RECLAIM_NOSWAP	(1 << MEM_CGROUP_RECLAIM_NOSWAP_BIT)
#define MEM_CGROUP_RECLAIM_SHRINK_BIT	0x1
#define MEM_CGROUP_RECLAIM_SHRINK	(1 << MEM_CGROUP_RECLAIM_SHRINK_BIT)

static void mem_cgroup_get(struct mem_cgroup *memcg);
static void mem_cgroup_put(struct mem_cgroup *memcg);

#ifdef CONFIG_CGROUP_MEM_RES_CTLR_KMEM
#include <net/sock.h>
#include <net/ip.h>

static bool mem_cgroup_is_root(struct mem_cgroup *memcg);
void sock_update_memcg(struct sock *sk)
{
	if (mem_cgroup_sockets_enabled) {
		struct mem_cgroup *memcg;

		BUG_ON(!sk->sk_prot->proto_cgroup);

		if (sk->sk_cgrp) {
			BUG_ON(mem_cgroup_is_root(sk->sk_cgrp->memcg));
			mem_cgroup_get(sk->sk_cgrp->memcg);
			return;
		}

		rcu_read_lock();
		memcg = mem_cgroup_from_task(current);
		if (!mem_cgroup_is_root(memcg)) {
			mem_cgroup_get(memcg);
			sk->sk_cgrp = sk->sk_prot->proto_cgroup(memcg);
		}
		rcu_read_unlock();
	}
}
EXPORT_SYMBOL(sock_update_memcg);

void sock_release_memcg(struct sock *sk)
{
	if (mem_cgroup_sockets_enabled && sk->sk_cgrp) {
		struct mem_cgroup *memcg;
		WARN_ON(!sk->sk_cgrp->memcg);
		memcg = sk->sk_cgrp->memcg;
		mem_cgroup_put(memcg);
	}
}

#ifdef CONFIG_INET
struct cg_proto *tcp_proto_cgroup(struct mem_cgroup *memcg)
{
	if (!memcg || mem_cgroup_is_root(memcg))
		return NULL;

	return &memcg->tcp_mem.cg_proto;
}
EXPORT_SYMBOL(tcp_proto_cgroup);
#endif 
#endif 

static void drain_all_stock_async(struct mem_cgroup *memcg);

static struct mem_cgroup_per_zone *
mem_cgroup_zoneinfo(struct mem_cgroup *memcg, int nid, int zid)
{
	return &memcg->info.nodeinfo[nid]->zoneinfo[zid];
}

struct cgroup_subsys_state *mem_cgroup_css(struct mem_cgroup *memcg)
{
	return &memcg->css;
}

static struct mem_cgroup_per_zone *
page_cgroup_zoneinfo(struct mem_cgroup *memcg, struct page *page)
{
	int nid = page_to_nid(page);
	int zid = page_zonenum(page);

	return mem_cgroup_zoneinfo(memcg, nid, zid);
}

static struct mem_cgroup_tree_per_zone *
soft_limit_tree_node_zone(int nid, int zid)
{
	return &soft_limit_tree.rb_tree_per_node[nid]->rb_tree_per_zone[zid];
}

static struct mem_cgroup_tree_per_zone *
soft_limit_tree_from_page(struct page *page)
{
	int nid = page_to_nid(page);
	int zid = page_zonenum(page);

	return &soft_limit_tree.rb_tree_per_node[nid]->rb_tree_per_zone[zid];
}

static void
__mem_cgroup_insert_exceeded(struct mem_cgroup *memcg,
				struct mem_cgroup_per_zone *mz,
				struct mem_cgroup_tree_per_zone *mctz,
				unsigned long long new_usage_in_excess)
{
	struct rb_node **p = &mctz->rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct mem_cgroup_per_zone *mz_node;

	if (mz->on_tree)
		return;

	mz->usage_in_excess = new_usage_in_excess;
	if (!mz->usage_in_excess)
		return;
	while (*p) {
		parent = *p;
		mz_node = rb_entry(parent, struct mem_cgroup_per_zone,
					tree_node);
		if (mz->usage_in_excess < mz_node->usage_in_excess)
			p = &(*p)->rb_left;
		else if (mz->usage_in_excess >= mz_node->usage_in_excess)
			p = &(*p)->rb_right;
	}
	rb_link_node(&mz->tree_node, parent, p);
	rb_insert_color(&mz->tree_node, &mctz->rb_root);
	mz->on_tree = true;
}

static void
__mem_cgroup_remove_exceeded(struct mem_cgroup *memcg,
				struct mem_cgroup_per_zone *mz,
				struct mem_cgroup_tree_per_zone *mctz)
{
	if (!mz->on_tree)
		return;
	rb_erase(&mz->tree_node, &mctz->rb_root);
	mz->on_tree = false;
}

static void
mem_cgroup_remove_exceeded(struct mem_cgroup *memcg,
				struct mem_cgroup_per_zone *mz,
				struct mem_cgroup_tree_per_zone *mctz)
{
	spin_lock(&mctz->lock);
	__mem_cgroup_remove_exceeded(memcg, mz, mctz);
	spin_unlock(&mctz->lock);
}


static void mem_cgroup_update_tree(struct mem_cgroup *memcg, struct page *page)
{
	unsigned long long excess;
	struct mem_cgroup_per_zone *mz;
	struct mem_cgroup_tree_per_zone *mctz;
	int nid = page_to_nid(page);
	int zid = page_zonenum(page);
	mctz = soft_limit_tree_from_page(page);

	for (; memcg; memcg = parent_mem_cgroup(memcg)) {
		mz = mem_cgroup_zoneinfo(memcg, nid, zid);
		excess = res_counter_soft_limit_excess(&memcg->res);
		if (excess || mz->on_tree) {
			spin_lock(&mctz->lock);
			
			if (mz->on_tree)
				__mem_cgroup_remove_exceeded(memcg, mz, mctz);
			__mem_cgroup_insert_exceeded(memcg, mz, mctz, excess);
			spin_unlock(&mctz->lock);
		}
	}
}

static void mem_cgroup_remove_from_trees(struct mem_cgroup *memcg)
{
	int node, zone;
	struct mem_cgroup_per_zone *mz;
	struct mem_cgroup_tree_per_zone *mctz;

	for_each_node(node) {
		for (zone = 0; zone < MAX_NR_ZONES; zone++) {
			mz = mem_cgroup_zoneinfo(memcg, node, zone);
			mctz = soft_limit_tree_node_zone(node, zone);
			mem_cgroup_remove_exceeded(memcg, mz, mctz);
		}
	}
}

static struct mem_cgroup_per_zone *
__mem_cgroup_largest_soft_limit_node(struct mem_cgroup_tree_per_zone *mctz)
{
	struct rb_node *rightmost = NULL;
	struct mem_cgroup_per_zone *mz;

retry:
	mz = NULL;
	rightmost = rb_last(&mctz->rb_root);
	if (!rightmost)
		goto done;		

	mz = rb_entry(rightmost, struct mem_cgroup_per_zone, tree_node);
	__mem_cgroup_remove_exceeded(mz->memcg, mz, mctz);
	if (!res_counter_soft_limit_excess(&mz->memcg->res) ||
		!css_tryget(&mz->memcg->css))
		goto retry;
done:
	return mz;
}

static struct mem_cgroup_per_zone *
mem_cgroup_largest_soft_limit_node(struct mem_cgroup_tree_per_zone *mctz)
{
	struct mem_cgroup_per_zone *mz;

	spin_lock(&mctz->lock);
	mz = __mem_cgroup_largest_soft_limit_node(mctz);
	spin_unlock(&mctz->lock);
	return mz;
}

static long mem_cgroup_read_stat(struct mem_cgroup *memcg,
				 enum mem_cgroup_stat_index idx)
{
	long val = 0;
	int cpu;

	get_online_cpus();
	for_each_online_cpu(cpu)
		val += per_cpu(memcg->stat->count[idx], cpu);
#ifdef CONFIG_HOTPLUG_CPU
	spin_lock(&memcg->pcp_counter_lock);
	val += memcg->nocpu_base.count[idx];
	spin_unlock(&memcg->pcp_counter_lock);
#endif
	put_online_cpus();
	return val;
}

static void mem_cgroup_swap_statistics(struct mem_cgroup *memcg,
					 bool charge)
{
	int val = (charge) ? 1 : -1;
	this_cpu_add(memcg->stat->count[MEM_CGROUP_STAT_SWAPOUT], val);
}

static unsigned long mem_cgroup_read_events(struct mem_cgroup *memcg,
					    enum mem_cgroup_events_index idx)
{
	unsigned long val = 0;
	int cpu;

	for_each_online_cpu(cpu)
		val += per_cpu(memcg->stat->events[idx], cpu);
#ifdef CONFIG_HOTPLUG_CPU
	spin_lock(&memcg->pcp_counter_lock);
	val += memcg->nocpu_base.events[idx];
	spin_unlock(&memcg->pcp_counter_lock);
#endif
	return val;
}

static void mem_cgroup_charge_statistics(struct mem_cgroup *memcg,
					 bool anon, int nr_pages)
{
	preempt_disable();

	if (anon)
		__this_cpu_add(memcg->stat->count[MEM_CGROUP_STAT_RSS],
				nr_pages);
	else
		__this_cpu_add(memcg->stat->count[MEM_CGROUP_STAT_CACHE],
				nr_pages);

	
	if (nr_pages > 0)
		__this_cpu_inc(memcg->stat->events[MEM_CGROUP_EVENTS_PGPGIN]);
	else {
		__this_cpu_inc(memcg->stat->events[MEM_CGROUP_EVENTS_PGPGOUT]);
		nr_pages = -nr_pages; 
	}

	__this_cpu_add(memcg->stat->events[MEM_CGROUP_EVENTS_COUNT], nr_pages);

	preempt_enable();
}

unsigned long
mem_cgroup_zone_nr_lru_pages(struct mem_cgroup *memcg, int nid, int zid,
			unsigned int lru_mask)
{
	struct mem_cgroup_per_zone *mz;
	enum lru_list lru;
	unsigned long ret = 0;

	mz = mem_cgroup_zoneinfo(memcg, nid, zid);

	for_each_lru(lru) {
		if (BIT(lru) & lru_mask)
			ret += mz->lru_size[lru];
	}
	return ret;
}

static unsigned long
mem_cgroup_node_nr_lru_pages(struct mem_cgroup *memcg,
			int nid, unsigned int lru_mask)
{
	u64 total = 0;
	int zid;

	for (zid = 0; zid < MAX_NR_ZONES; zid++)
		total += mem_cgroup_zone_nr_lru_pages(memcg,
						nid, zid, lru_mask);

	return total;
}

static unsigned long mem_cgroup_nr_lru_pages(struct mem_cgroup *memcg,
			unsigned int lru_mask)
{
	int nid;
	u64 total = 0;

	for_each_node_state(nid, N_HIGH_MEMORY)
		total += mem_cgroup_node_nr_lru_pages(memcg, nid, lru_mask);
	return total;
}

static bool mem_cgroup_event_ratelimit(struct mem_cgroup *memcg,
				       enum mem_cgroup_events_target target)
{
	unsigned long val, next;

	val = __this_cpu_read(memcg->stat->events[MEM_CGROUP_EVENTS_COUNT]);
	next = __this_cpu_read(memcg->stat->targets[target]);
	
	if ((long)next - (long)val < 0) {
		switch (target) {
		case MEM_CGROUP_TARGET_THRESH:
			next = val + THRESHOLDS_EVENTS_TARGET;
			break;
		case MEM_CGROUP_TARGET_SOFTLIMIT:
			next = val + SOFTLIMIT_EVENTS_TARGET;
			break;
		case MEM_CGROUP_TARGET_NUMAINFO:
			next = val + NUMAINFO_EVENTS_TARGET;
			break;
		default:
			break;
		}
		__this_cpu_write(memcg->stat->targets[target], next);
		return true;
	}
	return false;
}

static void memcg_check_events(struct mem_cgroup *memcg, struct page *page)
{
	preempt_disable();
	
	if (unlikely(mem_cgroup_event_ratelimit(memcg,
						MEM_CGROUP_TARGET_THRESH))) {
		bool do_softlimit;
		bool do_numainfo __maybe_unused;

		do_softlimit = mem_cgroup_event_ratelimit(memcg,
						MEM_CGROUP_TARGET_SOFTLIMIT);
#if MAX_NUMNODES > 1
		do_numainfo = mem_cgroup_event_ratelimit(memcg,
						MEM_CGROUP_TARGET_NUMAINFO);
#endif
		preempt_enable();

		mem_cgroup_threshold(memcg);
		if (unlikely(do_softlimit))
			mem_cgroup_update_tree(memcg, page);
#if MAX_NUMNODES > 1
		if (unlikely(do_numainfo))
			atomic_inc(&memcg->numainfo_events);
#endif
	} else
		preempt_enable();
}

struct mem_cgroup *mem_cgroup_from_cont(struct cgroup *cont)
{
	return container_of(cgroup_subsys_state(cont,
				mem_cgroup_subsys_id), struct mem_cgroup,
				css);
}

struct mem_cgroup *mem_cgroup_from_task(struct task_struct *p)
{
	if (unlikely(!p))
		return NULL;

	return container_of(task_subsys_state(p, mem_cgroup_subsys_id),
				struct mem_cgroup, css);
}

struct mem_cgroup *try_get_mem_cgroup_from_mm(struct mm_struct *mm)
{
	struct mem_cgroup *memcg = NULL;

	if (!mm)
		return NULL;
	rcu_read_lock();
	do {
		memcg = mem_cgroup_from_task(rcu_dereference(mm->owner));
		if (unlikely(!memcg))
			break;
	} while (!css_tryget(&memcg->css));
	rcu_read_unlock();
	return memcg;
}

struct mem_cgroup *mem_cgroup_iter(struct mem_cgroup *root,
				   struct mem_cgroup *prev,
				   struct mem_cgroup_reclaim_cookie *reclaim)
{
	struct mem_cgroup *memcg = NULL;
	int id = 0;

	if (mem_cgroup_disabled())
		return NULL;

	if (!root)
		root = root_mem_cgroup;

	if (prev && !reclaim)
		id = css_id(&prev->css);

	if (prev && prev != root)
		css_put(&prev->css);

	if (!root->use_hierarchy && root != root_mem_cgroup) {
		if (prev)
			return NULL;
		return root;
	}

	while (!memcg) {
		struct mem_cgroup_reclaim_iter *uninitialized_var(iter);
		struct cgroup_subsys_state *css;

		if (reclaim) {
			int nid = zone_to_nid(reclaim->zone);
			int zid = zone_idx(reclaim->zone);
			struct mem_cgroup_per_zone *mz;

			mz = mem_cgroup_zoneinfo(root, nid, zid);
			iter = &mz->reclaim_iter[reclaim->priority];
			if (prev && reclaim->generation != iter->generation)
				return NULL;
			id = iter->position;
		}

		rcu_read_lock();
		css = css_get_next(&mem_cgroup_subsys, id + 1, &root->css, &id);
		if (css) {
			if (css == &root->css || css_tryget(css))
				memcg = container_of(css,
						     struct mem_cgroup, css);
		} else
			id = 0;
		rcu_read_unlock();

		if (reclaim) {
			iter->position = id;
			if (!css)
				iter->generation++;
			else if (!prev && memcg)
				reclaim->generation = iter->generation;
		}

		if (prev && !css)
			return NULL;
	}
	return memcg;
}

void mem_cgroup_iter_break(struct mem_cgroup *root,
			   struct mem_cgroup *prev)
{
	if (!root)
		root = root_mem_cgroup;
	if (prev && prev != root)
		css_put(&prev->css);
}

#define for_each_mem_cgroup_tree(iter, root)		\
	for (iter = mem_cgroup_iter(root, NULL, NULL);	\
	     iter != NULL;				\
	     iter = mem_cgroup_iter(root, iter, NULL))

#define for_each_mem_cgroup(iter)			\
	for (iter = mem_cgroup_iter(NULL, NULL, NULL);	\
	     iter != NULL;				\
	     iter = mem_cgroup_iter(NULL, iter, NULL))

static inline bool mem_cgroup_is_root(struct mem_cgroup *memcg)
{
	return (memcg == root_mem_cgroup);
}

void mem_cgroup_count_vm_event(struct mm_struct *mm, enum vm_event_item idx)
{
	struct mem_cgroup *memcg;

	if (!mm)
		return;

	rcu_read_lock();
	memcg = mem_cgroup_from_task(rcu_dereference(mm->owner));
	if (unlikely(!memcg))
		goto out;

	switch (idx) {
	case PGFAULT:
		this_cpu_inc(memcg->stat->events[MEM_CGROUP_EVENTS_PGFAULT]);
		break;
	case PGMAJFAULT:
		this_cpu_inc(memcg->stat->events[MEM_CGROUP_EVENTS_PGMAJFAULT]);
		break;
	default:
		BUG();
	}
out:
	rcu_read_unlock();
}
EXPORT_SYMBOL(mem_cgroup_count_vm_event);

struct lruvec *mem_cgroup_zone_lruvec(struct zone *zone,
				      struct mem_cgroup *memcg)
{
	struct mem_cgroup_per_zone *mz;

	if (mem_cgroup_disabled())
		return &zone->lruvec;

	mz = mem_cgroup_zoneinfo(memcg, zone_to_nid(zone), zone_idx(zone));
	return &mz->lruvec;
}


struct lruvec *mem_cgroup_lru_add_list(struct zone *zone, struct page *page,
				       enum lru_list lru)
{
	struct mem_cgroup_per_zone *mz;
	struct mem_cgroup *memcg;
	struct page_cgroup *pc;

	if (mem_cgroup_disabled())
		return &zone->lruvec;

	pc = lookup_page_cgroup(page);
	memcg = pc->mem_cgroup;

	if (!PageCgroupUsed(pc) && memcg != root_mem_cgroup)
		pc->mem_cgroup = memcg = root_mem_cgroup;

	mz = page_cgroup_zoneinfo(memcg, page);
	
	mz->lru_size[lru] += 1 << compound_order(page);
	return &mz->lruvec;
}

void mem_cgroup_lru_del_list(struct page *page, enum lru_list lru)
{
	struct mem_cgroup_per_zone *mz;
	struct mem_cgroup *memcg;
	struct page_cgroup *pc;

	if (mem_cgroup_disabled())
		return;

	pc = lookup_page_cgroup(page);
	memcg = pc->mem_cgroup;
	VM_BUG_ON(!memcg);
	mz = page_cgroup_zoneinfo(memcg, page);
	
	VM_BUG_ON(mz->lru_size[lru] < (1 << compound_order(page)));
	mz->lru_size[lru] -= 1 << compound_order(page);
}

struct lruvec *mem_cgroup_lru_move_lists(struct zone *zone,
					 struct page *page,
					 enum lru_list from,
					 enum lru_list to)
{
	
	mem_cgroup_lru_del_list(page, from);
	return mem_cgroup_lru_add_list(zone, page, to);
}

bool __mem_cgroup_same_or_subtree(const struct mem_cgroup *root_memcg,
				  struct mem_cgroup *memcg)
{
	if (root_memcg == memcg)
		return true;
	if (!root_memcg->use_hierarchy)
		return false;
	return css_is_ancestor(&memcg->css, &root_memcg->css);
}

static bool mem_cgroup_same_or_subtree(const struct mem_cgroup *root_memcg,
				       struct mem_cgroup *memcg)
{
	bool ret;

	rcu_read_lock();
	ret = __mem_cgroup_same_or_subtree(root_memcg, memcg);
	rcu_read_unlock();
	return ret;
}

int task_in_mem_cgroup(struct task_struct *task, const struct mem_cgroup *memcg)
{
	int ret;
	struct mem_cgroup *curr = NULL;
	struct task_struct *p;

	p = find_lock_task_mm(task);
	if (p) {
		curr = try_get_mem_cgroup_from_mm(p->mm);
		task_unlock(p);
	} else {
		task_lock(task);
		curr = mem_cgroup_from_task(task);
		if (curr)
			css_get(&curr->css);
		task_unlock(task);
	}
	if (!curr)
		return 0;
	ret = mem_cgroup_same_or_subtree(memcg, curr);
	css_put(&curr->css);
	return ret;
}

int mem_cgroup_inactive_anon_is_low(struct mem_cgroup *memcg, struct zone *zone)
{
	unsigned long inactive_ratio;
	int nid = zone_to_nid(zone);
	int zid = zone_idx(zone);
	unsigned long inactive;
	unsigned long active;
	unsigned long gb;

	inactive = mem_cgroup_zone_nr_lru_pages(memcg, nid, zid,
						BIT(LRU_INACTIVE_ANON));
	active = mem_cgroup_zone_nr_lru_pages(memcg, nid, zid,
					      BIT(LRU_ACTIVE_ANON));

	gb = (inactive + active) >> (30 - PAGE_SHIFT);
	if (gb)
		inactive_ratio = int_sqrt(10 * gb);
	else
		inactive_ratio = 1;

	return inactive * inactive_ratio < active;
}

int mem_cgroup_inactive_file_is_low(struct mem_cgroup *memcg, struct zone *zone)
{
	unsigned long active;
	unsigned long inactive;
	int zid = zone_idx(zone);
	int nid = zone_to_nid(zone);

	inactive = mem_cgroup_zone_nr_lru_pages(memcg, nid, zid,
						BIT(LRU_INACTIVE_FILE));
	active = mem_cgroup_zone_nr_lru_pages(memcg, nid, zid,
					      BIT(LRU_ACTIVE_FILE));

	return (active > inactive);
}

struct zone_reclaim_stat *mem_cgroup_get_reclaim_stat(struct mem_cgroup *memcg,
						      struct zone *zone)
{
	int nid = zone_to_nid(zone);
	int zid = zone_idx(zone);
	struct mem_cgroup_per_zone *mz = mem_cgroup_zoneinfo(memcg, nid, zid);

	return &mz->reclaim_stat;
}

struct zone_reclaim_stat *
mem_cgroup_get_reclaim_stat_from_page(struct page *page)
{
	struct page_cgroup *pc;
	struct mem_cgroup_per_zone *mz;

	if (mem_cgroup_disabled())
		return NULL;

	pc = lookup_page_cgroup(page);
	if (!PageCgroupUsed(pc))
		return NULL;
	
	smp_rmb();
	mz = page_cgroup_zoneinfo(pc->mem_cgroup, page);
	return &mz->reclaim_stat;
}

#define mem_cgroup_from_res_counter(counter, member)	\
	container_of(counter, struct mem_cgroup, member)

static unsigned long mem_cgroup_margin(struct mem_cgroup *memcg)
{
	unsigned long long margin;

	margin = res_counter_margin(&memcg->res);
	if (do_swap_account)
		margin = min(margin, res_counter_margin(&memcg->memsw));
	return margin >> PAGE_SHIFT;
}

int mem_cgroup_swappiness(struct mem_cgroup *memcg)
{
	struct cgroup *cgrp = memcg->css.cgroup;

	
	if (cgrp->parent == NULL)
		return vm_swappiness;

	return memcg->swappiness;
}


atomic_t memcg_moving __read_mostly;

static void mem_cgroup_start_move(struct mem_cgroup *memcg)
{
	atomic_inc(&memcg_moving);
	atomic_inc(&memcg->moving_account);
	synchronize_rcu();
}

static void mem_cgroup_end_move(struct mem_cgroup *memcg)
{
	if (memcg) {
		atomic_dec(&memcg_moving);
		atomic_dec(&memcg->moving_account);
	}
}

/*
 * 2 routines for checking "mem" is under move_account() or not.
 *
 * mem_cgroup_stolen() -  checking whether a cgroup is mc.from or not. This
 *			  is used for avoiding races in accounting.  If true,
 *			  pc->mem_cgroup may be overwritten.
 *
 * mem_cgroup_under_move() - checking a cgroup is mc.from or mc.to or
 *			  under hierarchy of moving cgroups. This is for
 *			  waiting at hith-memory prressure caused by "move".
 */

static bool mem_cgroup_stolen(struct mem_cgroup *memcg)
{
	VM_BUG_ON(!rcu_read_lock_held());
	return atomic_read(&memcg->moving_account) > 0;
}

static bool mem_cgroup_under_move(struct mem_cgroup *memcg)
{
	struct mem_cgroup *from;
	struct mem_cgroup *to;
	bool ret = false;
	spin_lock(&mc.lock);
	from = mc.from;
	to = mc.to;
	if (!from)
		goto unlock;

	ret = mem_cgroup_same_or_subtree(memcg, from)
		|| mem_cgroup_same_or_subtree(memcg, to);
unlock:
	spin_unlock(&mc.lock);
	return ret;
}

static bool mem_cgroup_wait_acct_move(struct mem_cgroup *memcg)
{
	if (mc.moving_task && current != mc.moving_task) {
		if (mem_cgroup_under_move(memcg)) {
			DEFINE_WAIT(wait);
			prepare_to_wait(&mc.waitq, &wait, TASK_INTERRUPTIBLE);
			
			if (mc.moving_task)
				schedule();
			finish_wait(&mc.waitq, &wait);
			return true;
		}
	}
	return false;
}

static void move_lock_mem_cgroup(struct mem_cgroup *memcg,
				  unsigned long *flags)
{
	spin_lock_irqsave(&memcg->move_lock, *flags);
}

static void move_unlock_mem_cgroup(struct mem_cgroup *memcg,
				unsigned long *flags)
{
	spin_unlock_irqrestore(&memcg->move_lock, *flags);
}

void mem_cgroup_print_oom_info(struct mem_cgroup *memcg, struct task_struct *p)
{
	struct cgroup *task_cgrp;
	struct cgroup *mem_cgrp;
	static char memcg_name[PATH_MAX];
	int ret;

	if (!memcg || !p)
		return;

	rcu_read_lock();

	mem_cgrp = memcg->css.cgroup;
	task_cgrp = task_cgroup(p, mem_cgroup_subsys_id);

	ret = cgroup_path(task_cgrp, memcg_name, PATH_MAX);
	if (ret < 0) {
		rcu_read_unlock();
		goto done;
	}
	rcu_read_unlock();

	printk(KERN_INFO "Task in %s killed", memcg_name);

	rcu_read_lock();
	ret = cgroup_path(mem_cgrp, memcg_name, PATH_MAX);
	if (ret < 0) {
		rcu_read_unlock();
		goto done;
	}
	rcu_read_unlock();

	printk(KERN_CONT " as a result of limit of %s\n", memcg_name);
done:

	printk(KERN_INFO "memory: usage %llukB, limit %llukB, failcnt %llu\n",
		res_counter_read_u64(&memcg->res, RES_USAGE) >> 10,
		res_counter_read_u64(&memcg->res, RES_LIMIT) >> 10,
		res_counter_read_u64(&memcg->res, RES_FAILCNT));
	printk(KERN_INFO "memory+swap: usage %llukB, limit %llukB, "
		"failcnt %llu\n",
		res_counter_read_u64(&memcg->memsw, RES_USAGE) >> 10,
		res_counter_read_u64(&memcg->memsw, RES_LIMIT) >> 10,
		res_counter_read_u64(&memcg->memsw, RES_FAILCNT));
}

static int mem_cgroup_count_children(struct mem_cgroup *memcg)
{
	int num = 0;
	struct mem_cgroup *iter;

	for_each_mem_cgroup_tree(iter, memcg)
		num++;
	return num;
}

u64 mem_cgroup_get_limit(struct mem_cgroup *memcg)
{
	u64 limit;
	u64 memsw;

	limit = res_counter_read_u64(&memcg->res, RES_LIMIT);
	limit += total_swap_pages << PAGE_SHIFT;

	memsw = res_counter_read_u64(&memcg->memsw, RES_LIMIT);
	return min(limit, memsw);
}

static unsigned long mem_cgroup_reclaim(struct mem_cgroup *memcg,
					gfp_t gfp_mask,
					unsigned long flags)
{
	unsigned long total = 0;
	bool noswap = false;
	int loop;

	if (flags & MEM_CGROUP_RECLAIM_NOSWAP)
		noswap = true;
	if (!(flags & MEM_CGROUP_RECLAIM_SHRINK) && memcg->memsw_is_minimum)
		noswap = true;

	for (loop = 0; loop < MEM_CGROUP_MAX_RECLAIM_LOOPS; loop++) {
		if (loop)
			drain_all_stock_async(memcg);
		total += try_to_free_mem_cgroup_pages(memcg, gfp_mask, noswap);
		if (total && (flags & MEM_CGROUP_RECLAIM_SHRINK))
			break;
		if (mem_cgroup_margin(memcg))
			break;
		if (loop && !total)
			break;
	}
	return total;
}

static bool test_mem_cgroup_node_reclaimable(struct mem_cgroup *memcg,
		int nid, bool noswap)
{
	if (mem_cgroup_node_nr_lru_pages(memcg, nid, LRU_ALL_FILE))
		return true;
	if (noswap || !total_swap_pages)
		return false;
	if (mem_cgroup_node_nr_lru_pages(memcg, nid, LRU_ALL_ANON))
		return true;
	return false;

}
#if MAX_NUMNODES > 1

static void mem_cgroup_may_update_nodemask(struct mem_cgroup *memcg)
{
	int nid;
	if (!atomic_read(&memcg->numainfo_events))
		return;
	if (atomic_inc_return(&memcg->numainfo_updating) > 1)
		return;

	
	memcg->scan_nodes = node_states[N_HIGH_MEMORY];

	for_each_node_mask(nid, node_states[N_HIGH_MEMORY]) {

		if (!test_mem_cgroup_node_reclaimable(memcg, nid, false))
			node_clear(nid, memcg->scan_nodes);
	}

	atomic_set(&memcg->numainfo_events, 0);
	atomic_set(&memcg->numainfo_updating, 0);
}

int mem_cgroup_select_victim_node(struct mem_cgroup *memcg)
{
	int node;

	mem_cgroup_may_update_nodemask(memcg);
	node = memcg->last_scanned_node;

	node = next_node(node, memcg->scan_nodes);
	if (node == MAX_NUMNODES)
		node = first_node(memcg->scan_nodes);
	if (unlikely(node == MAX_NUMNODES))
		node = numa_node_id();

	memcg->last_scanned_node = node;
	return node;
}

bool mem_cgroup_reclaimable(struct mem_cgroup *memcg, bool noswap)
{
	int nid;

	if (!nodes_empty(memcg->scan_nodes)) {
		for (nid = first_node(memcg->scan_nodes);
		     nid < MAX_NUMNODES;
		     nid = next_node(nid, memcg->scan_nodes)) {

			if (test_mem_cgroup_node_reclaimable(memcg, nid, noswap))
				return true;
		}
	}
	for_each_node_state(nid, N_HIGH_MEMORY) {
		if (node_isset(nid, memcg->scan_nodes))
			continue;
		if (test_mem_cgroup_node_reclaimable(memcg, nid, noswap))
			return true;
	}
	return false;
}

#else
int mem_cgroup_select_victim_node(struct mem_cgroup *memcg)
{
	return 0;
}

bool mem_cgroup_reclaimable(struct mem_cgroup *memcg, bool noswap)
{
	return test_mem_cgroup_node_reclaimable(memcg, 0, noswap);
}
#endif

static int mem_cgroup_soft_reclaim(struct mem_cgroup *root_memcg,
				   struct zone *zone,
				   gfp_t gfp_mask,
				   unsigned long *total_scanned)
{
	struct mem_cgroup *victim = NULL;
	int total = 0;
	int loop = 0;
	unsigned long excess;
	unsigned long nr_scanned;
	struct mem_cgroup_reclaim_cookie reclaim = {
		.zone = zone,
		.priority = 0,
	};

	excess = res_counter_soft_limit_excess(&root_memcg->res) >> PAGE_SHIFT;

	while (1) {
		victim = mem_cgroup_iter(root_memcg, victim, &reclaim);
		if (!victim) {
			loop++;
			if (loop >= 2) {
				if (!total)
					break;
				if (total >= (excess >> 2) ||
					(loop > MEM_CGROUP_MAX_RECLAIM_LOOPS))
					break;
			}
			continue;
		}
		if (!mem_cgroup_reclaimable(victim, false))
			continue;
		total += mem_cgroup_shrink_node_zone(victim, gfp_mask, false,
						     zone, &nr_scanned);
		*total_scanned += nr_scanned;
		if (!res_counter_soft_limit_excess(&root_memcg->res))
			break;
	}
	mem_cgroup_iter_break(root_memcg, victim);
	return total;
}

static bool mem_cgroup_oom_lock(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter, *failed = NULL;

	for_each_mem_cgroup_tree(iter, memcg) {
		if (iter->oom_lock) {
			failed = iter;
			mem_cgroup_iter_break(memcg, iter);
			break;
		} else
			iter->oom_lock = true;
	}

	if (!failed)
		return true;

	for_each_mem_cgroup_tree(iter, memcg) {
		if (iter == failed) {
			mem_cgroup_iter_break(memcg, iter);
			break;
		}
		iter->oom_lock = false;
	}
	return false;
}

static int mem_cgroup_oom_unlock(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter;

	for_each_mem_cgroup_tree(iter, memcg)
		iter->oom_lock = false;
	return 0;
}

static void mem_cgroup_mark_under_oom(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter;

	for_each_mem_cgroup_tree(iter, memcg)
		atomic_inc(&iter->under_oom);
}

static void mem_cgroup_unmark_under_oom(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter;

	for_each_mem_cgroup_tree(iter, memcg)
		atomic_add_unless(&iter->under_oom, -1, 0);
}

static DEFINE_SPINLOCK(memcg_oom_lock);
static DECLARE_WAIT_QUEUE_HEAD(memcg_oom_waitq);

struct oom_wait_info {
	struct mem_cgroup *memcg;
	wait_queue_t	wait;
};

static int memcg_oom_wake_function(wait_queue_t *wait,
	unsigned mode, int sync, void *arg)
{
	struct mem_cgroup *wake_memcg = (struct mem_cgroup *)arg;
	struct mem_cgroup *oom_wait_memcg;
	struct oom_wait_info *oom_wait_info;

	oom_wait_info = container_of(wait, struct oom_wait_info, wait);
	oom_wait_memcg = oom_wait_info->memcg;

	if (!mem_cgroup_same_or_subtree(oom_wait_memcg, wake_memcg)
		&& !mem_cgroup_same_or_subtree(wake_memcg, oom_wait_memcg))
		return 0;
	return autoremove_wake_function(wait, mode, sync, arg);
}

static void memcg_wakeup_oom(struct mem_cgroup *memcg)
{
	
	__wake_up(&memcg_oom_waitq, TASK_NORMAL, 0, memcg);
}

static void memcg_oom_recover(struct mem_cgroup *memcg)
{
	if (memcg && atomic_read(&memcg->under_oom))
		memcg_wakeup_oom(memcg);
}

bool mem_cgroup_handle_oom(struct mem_cgroup *memcg, gfp_t mask, int order)
{
	struct oom_wait_info owait;
	bool locked, need_to_kill;

	owait.memcg = memcg;
	owait.wait.flags = 0;
	owait.wait.func = memcg_oom_wake_function;
	owait.wait.private = current;
	INIT_LIST_HEAD(&owait.wait.task_list);
	need_to_kill = true;
	mem_cgroup_mark_under_oom(memcg);

	
	spin_lock(&memcg_oom_lock);
	locked = mem_cgroup_oom_lock(memcg);
	prepare_to_wait(&memcg_oom_waitq, &owait.wait, TASK_KILLABLE);
	if (!locked || memcg->oom_kill_disable)
		need_to_kill = false;
	if (locked)
		mem_cgroup_oom_notify(memcg);
	spin_unlock(&memcg_oom_lock);

	if (need_to_kill) {
		finish_wait(&memcg_oom_waitq, &owait.wait);
		mem_cgroup_out_of_memory(memcg, mask, order);
	} else {
		schedule();
		finish_wait(&memcg_oom_waitq, &owait.wait);
	}
	spin_lock(&memcg_oom_lock);
	if (locked)
		mem_cgroup_oom_unlock(memcg);
	memcg_wakeup_oom(memcg);
	spin_unlock(&memcg_oom_lock);

	mem_cgroup_unmark_under_oom(memcg);

	if (test_thread_flag(TIF_MEMDIE) || fatal_signal_pending(current))
		return false;
	
	schedule_timeout_uninterruptible(1);
	return true;
}


void __mem_cgroup_begin_update_page_stat(struct page *page,
				bool *locked, unsigned long *flags)
{
	struct mem_cgroup *memcg;
	struct page_cgroup *pc;

	pc = lookup_page_cgroup(page);
again:
	memcg = pc->mem_cgroup;
	if (unlikely(!memcg || !PageCgroupUsed(pc)))
		return;
	if (!mem_cgroup_stolen(memcg))
		return;

	move_lock_mem_cgroup(memcg, flags);
	if (memcg != pc->mem_cgroup || !PageCgroupUsed(pc)) {
		move_unlock_mem_cgroup(memcg, flags);
		goto again;
	}
	*locked = true;
}

void __mem_cgroup_end_update_page_stat(struct page *page, unsigned long *flags)
{
	struct page_cgroup *pc = lookup_page_cgroup(page);

	move_unlock_mem_cgroup(pc->mem_cgroup, flags);
}

void mem_cgroup_update_page_stat(struct page *page,
				 enum mem_cgroup_page_stat_item idx, int val)
{
	struct mem_cgroup *memcg;
	struct page_cgroup *pc = lookup_page_cgroup(page);
	unsigned long uninitialized_var(flags);

	if (mem_cgroup_disabled())
		return;

	memcg = pc->mem_cgroup;
	if (unlikely(!memcg || !PageCgroupUsed(pc)))
		return;

	switch (idx) {
	case MEMCG_NR_FILE_MAPPED:
		idx = MEM_CGROUP_STAT_FILE_MAPPED;
		break;
	default:
		BUG();
	}

	this_cpu_add(memcg->stat->count[idx], val);
}

#define CHARGE_BATCH	32U
struct memcg_stock_pcp {
	struct mem_cgroup *cached; 
	unsigned int nr_pages;
	struct work_struct work;
	unsigned long flags;
#define FLUSHING_CACHED_CHARGE	(0)
};
static DEFINE_PER_CPU(struct memcg_stock_pcp, memcg_stock);
static DEFINE_MUTEX(percpu_charge_mutex);

static bool consume_stock(struct mem_cgroup *memcg)
{
	struct memcg_stock_pcp *stock;
	bool ret = true;

	stock = &get_cpu_var(memcg_stock);
	if (memcg == stock->cached && stock->nr_pages)
		stock->nr_pages--;
	else 
		ret = false;
	put_cpu_var(memcg_stock);
	return ret;
}

static void drain_stock(struct memcg_stock_pcp *stock)
{
	struct mem_cgroup *old = stock->cached;

	if (stock->nr_pages) {
		unsigned long bytes = stock->nr_pages * PAGE_SIZE;

		res_counter_uncharge(&old->res, bytes);
		if (do_swap_account)
			res_counter_uncharge(&old->memsw, bytes);
		stock->nr_pages = 0;
	}
	stock->cached = NULL;
}

static void drain_local_stock(struct work_struct *dummy)
{
	struct memcg_stock_pcp *stock = &__get_cpu_var(memcg_stock);
	drain_stock(stock);
	clear_bit(FLUSHING_CACHED_CHARGE, &stock->flags);
}

static void refill_stock(struct mem_cgroup *memcg, unsigned int nr_pages)
{
	struct memcg_stock_pcp *stock = &get_cpu_var(memcg_stock);

	if (stock->cached != memcg) { 
		drain_stock(stock);
		stock->cached = memcg;
	}
	stock->nr_pages += nr_pages;
	put_cpu_var(memcg_stock);
}

static void drain_all_stock(struct mem_cgroup *root_memcg, bool sync)
{
	int cpu, curcpu;

	
	get_online_cpus();
	curcpu = get_cpu();
	for_each_online_cpu(cpu) {
		struct memcg_stock_pcp *stock = &per_cpu(memcg_stock, cpu);
		struct mem_cgroup *memcg;

		memcg = stock->cached;
		if (!memcg || !stock->nr_pages)
			continue;
		if (!mem_cgroup_same_or_subtree(root_memcg, memcg))
			continue;
		if (!test_and_set_bit(FLUSHING_CACHED_CHARGE, &stock->flags)) {
			if (cpu == curcpu)
				drain_local_stock(&stock->work);
			else
				schedule_work_on(cpu, &stock->work);
		}
	}
	put_cpu();

	if (!sync)
		goto out;

	for_each_online_cpu(cpu) {
		struct memcg_stock_pcp *stock = &per_cpu(memcg_stock, cpu);
		if (test_bit(FLUSHING_CACHED_CHARGE, &stock->flags))
			flush_work(&stock->work);
	}
out:
 	put_online_cpus();
}

static void drain_all_stock_async(struct mem_cgroup *root_memcg)
{
	if (!mutex_trylock(&percpu_charge_mutex))
		return;
	drain_all_stock(root_memcg, false);
	mutex_unlock(&percpu_charge_mutex);
}

static void drain_all_stock_sync(struct mem_cgroup *root_memcg)
{
	
	mutex_lock(&percpu_charge_mutex);
	drain_all_stock(root_memcg, true);
	mutex_unlock(&percpu_charge_mutex);
}

static void mem_cgroup_drain_pcp_counter(struct mem_cgroup *memcg, int cpu)
{
	int i;

	spin_lock(&memcg->pcp_counter_lock);
	for (i = 0; i < MEM_CGROUP_STAT_DATA; i++) {
		long x = per_cpu(memcg->stat->count[i], cpu);

		per_cpu(memcg->stat->count[i], cpu) = 0;
		memcg->nocpu_base.count[i] += x;
	}
	for (i = 0; i < MEM_CGROUP_EVENTS_NSTATS; i++) {
		unsigned long x = per_cpu(memcg->stat->events[i], cpu);

		per_cpu(memcg->stat->events[i], cpu) = 0;
		memcg->nocpu_base.events[i] += x;
	}
	spin_unlock(&memcg->pcp_counter_lock);
}

static int __cpuinit memcg_cpu_hotplug_callback(struct notifier_block *nb,
					unsigned long action,
					void *hcpu)
{
	int cpu = (unsigned long)hcpu;
	struct memcg_stock_pcp *stock;
	struct mem_cgroup *iter;

	if (action == CPU_ONLINE)
		return NOTIFY_OK;

	if (action != CPU_DEAD && action != CPU_DEAD_FROZEN)
		return NOTIFY_OK;

	for_each_mem_cgroup(iter)
		mem_cgroup_drain_pcp_counter(iter, cpu);

	stock = &per_cpu(memcg_stock, cpu);
	drain_stock(stock);
	return NOTIFY_OK;
}


enum {
	CHARGE_OK,		
	CHARGE_RETRY,		
	CHARGE_NOMEM,		
	CHARGE_WOULDBLOCK,	
	CHARGE_OOM_DIE,		
};

static int mem_cgroup_do_charge(struct mem_cgroup *memcg, gfp_t gfp_mask,
				unsigned int nr_pages, bool oom_check)
{
	unsigned long csize = nr_pages * PAGE_SIZE;
	struct mem_cgroup *mem_over_limit;
	struct res_counter *fail_res;
	unsigned long flags = 0;
	int ret;

	ret = res_counter_charge(&memcg->res, csize, &fail_res);

	if (likely(!ret)) {
		if (!do_swap_account)
			return CHARGE_OK;
		ret = res_counter_charge(&memcg->memsw, csize, &fail_res);
		if (likely(!ret))
			return CHARGE_OK;

		res_counter_uncharge(&memcg->res, csize);
		mem_over_limit = mem_cgroup_from_res_counter(fail_res, memsw);
		flags |= MEM_CGROUP_RECLAIM_NOSWAP;
	} else
		mem_over_limit = mem_cgroup_from_res_counter(fail_res, res);
	if (nr_pages == CHARGE_BATCH)
		return CHARGE_RETRY;

	if (!(gfp_mask & __GFP_WAIT))
		return CHARGE_WOULDBLOCK;

	ret = mem_cgroup_reclaim(mem_over_limit, gfp_mask, flags);
	if (mem_cgroup_margin(mem_over_limit) >= nr_pages)
		return CHARGE_RETRY;
	if (nr_pages == 1 && ret)
		return CHARGE_RETRY;

	if (mem_cgroup_wait_acct_move(mem_over_limit))
		return CHARGE_RETRY;

	
	if (!oom_check)
		return CHARGE_NOMEM;
	
	if (!mem_cgroup_handle_oom(mem_over_limit, gfp_mask, get_order(csize)))
		return CHARGE_OOM_DIE;

	return CHARGE_RETRY;
}

static int __mem_cgroup_try_charge(struct mm_struct *mm,
				   gfp_t gfp_mask,
				   unsigned int nr_pages,
				   struct mem_cgroup **ptr,
				   bool oom)
{
	unsigned int batch = max(CHARGE_BATCH, nr_pages);
	int nr_oom_retries = MEM_CGROUP_RECLAIM_RETRIES;
	struct mem_cgroup *memcg = NULL;
	int ret;

	if (unlikely(test_thread_flag(TIF_MEMDIE)
		     || fatal_signal_pending(current)))
		goto bypass;

	if (!*ptr && !mm)
		*ptr = root_mem_cgroup;
again:
	if (*ptr) { 
		memcg = *ptr;
		VM_BUG_ON(css_is_removed(&memcg->css));
		if (mem_cgroup_is_root(memcg))
			goto done;
		if (nr_pages == 1 && consume_stock(memcg))
			goto done;
		css_get(&memcg->css);
	} else {
		struct task_struct *p;

		rcu_read_lock();
		p = rcu_dereference(mm->owner);
		memcg = mem_cgroup_from_task(p);
		if (!memcg)
			memcg = root_mem_cgroup;
		if (mem_cgroup_is_root(memcg)) {
			rcu_read_unlock();
			goto done;
		}
		if (nr_pages == 1 && consume_stock(memcg)) {
			rcu_read_unlock();
			goto done;
		}
		
		if (!css_tryget(&memcg->css)) {
			rcu_read_unlock();
			goto again;
		}
		rcu_read_unlock();
	}

	do {
		bool oom_check;

		
		if (fatal_signal_pending(current)) {
			css_put(&memcg->css);
			goto bypass;
		}

		oom_check = false;
		if (oom && !nr_oom_retries) {
			oom_check = true;
			nr_oom_retries = MEM_CGROUP_RECLAIM_RETRIES;
		}

		ret = mem_cgroup_do_charge(memcg, gfp_mask, batch, oom_check);
		switch (ret) {
		case CHARGE_OK:
			break;
		case CHARGE_RETRY: 
			batch = nr_pages;
			css_put(&memcg->css);
			memcg = NULL;
			goto again;
		case CHARGE_WOULDBLOCK: 
			css_put(&memcg->css);
			goto nomem;
		case CHARGE_NOMEM: 
			if (!oom) {
				css_put(&memcg->css);
				goto nomem;
			}
			
			nr_oom_retries--;
			break;
		case CHARGE_OOM_DIE: 
			css_put(&memcg->css);
			goto bypass;
		}
	} while (ret != CHARGE_OK);

	if (batch > nr_pages)
		refill_stock(memcg, batch - nr_pages);
	css_put(&memcg->css);
done:
	*ptr = memcg;
	return 0;
nomem:
	*ptr = NULL;
	return -ENOMEM;
bypass:
	*ptr = root_mem_cgroup;
	return -EINTR;
}

static void __mem_cgroup_cancel_charge(struct mem_cgroup *memcg,
				       unsigned int nr_pages)
{
	if (!mem_cgroup_is_root(memcg)) {
		unsigned long bytes = nr_pages * PAGE_SIZE;

		res_counter_uncharge(&memcg->res, bytes);
		if (do_swap_account)
			res_counter_uncharge(&memcg->memsw, bytes);
	}
}

static struct mem_cgroup *mem_cgroup_lookup(unsigned short id)
{
	struct cgroup_subsys_state *css;

	
	if (!id)
		return NULL;
	css = css_lookup(&mem_cgroup_subsys, id);
	if (!css)
		return NULL;
	return container_of(css, struct mem_cgroup, css);
}

struct mem_cgroup *try_get_mem_cgroup_from_page(struct page *page)
{
	struct mem_cgroup *memcg = NULL;
	struct page_cgroup *pc;
	unsigned short id;
	swp_entry_t ent;

	VM_BUG_ON(!PageLocked(page));

	pc = lookup_page_cgroup(page);
	lock_page_cgroup(pc);
	if (PageCgroupUsed(pc)) {
		memcg = pc->mem_cgroup;
		if (memcg && !css_tryget(&memcg->css))
			memcg = NULL;
	} else if (PageSwapCache(page)) {
		ent.val = page_private(page);
		id = lookup_swap_cgroup_id(ent);
		rcu_read_lock();
		memcg = mem_cgroup_lookup(id);
		if (memcg && !css_tryget(&memcg->css))
			memcg = NULL;
		rcu_read_unlock();
	}
	unlock_page_cgroup(pc);
	return memcg;
}

static void __mem_cgroup_commit_charge(struct mem_cgroup *memcg,
				       struct page *page,
				       unsigned int nr_pages,
				       enum charge_type ctype,
				       bool lrucare)
{
	struct page_cgroup *pc = lookup_page_cgroup(page);
	struct zone *uninitialized_var(zone);
	bool was_on_lru = false;
	bool anon;

	lock_page_cgroup(pc);
	if (unlikely(PageCgroupUsed(pc))) {
		unlock_page_cgroup(pc);
		__mem_cgroup_cancel_charge(memcg, nr_pages);
		return;
	}

	if (lrucare) {
		zone = page_zone(page);
		spin_lock_irq(&zone->lru_lock);
		if (PageLRU(page)) {
			ClearPageLRU(page);
			del_page_from_lru_list(zone, page, page_lru(page));
			was_on_lru = true;
		}
	}

	pc->mem_cgroup = memcg;
	smp_wmb();
	SetPageCgroupUsed(pc);

	if (lrucare) {
		if (was_on_lru) {
			VM_BUG_ON(PageLRU(page));
			SetPageLRU(page);
			add_page_to_lru_list(zone, page, page_lru(page));
		}
		spin_unlock_irq(&zone->lru_lock);
	}

	if (ctype == MEM_CGROUP_CHARGE_TYPE_MAPPED)
		anon = true;
	else
		anon = false;

	mem_cgroup_charge_statistics(memcg, anon, nr_pages);
	unlock_page_cgroup(pc);

	memcg_check_events(memcg, page);
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE

#define PCGF_NOCOPY_AT_SPLIT ((1 << PCG_LOCK) | (1 << PCG_MIGRATION))
void mem_cgroup_split_huge_fixup(struct page *head)
{
	struct page_cgroup *head_pc = lookup_page_cgroup(head);
	struct page_cgroup *pc;
	int i;

	if (mem_cgroup_disabled())
		return;
	for (i = 1; i < HPAGE_PMD_NR; i++) {
		pc = head_pc + i;
		pc->mem_cgroup = head_pc->mem_cgroup;
		smp_wmb();
		pc->flags = head_pc->flags & ~PCGF_NOCOPY_AT_SPLIT;
	}
}
#endif 

static int mem_cgroup_move_account(struct page *page,
				   unsigned int nr_pages,
				   struct page_cgroup *pc,
				   struct mem_cgroup *from,
				   struct mem_cgroup *to,
				   bool uncharge)
{
	unsigned long flags;
	int ret;
	bool anon = PageAnon(page);

	VM_BUG_ON(from == to);
	VM_BUG_ON(PageLRU(page));
	ret = -EBUSY;
	if (nr_pages > 1 && !PageTransHuge(page))
		goto out;

	lock_page_cgroup(pc);

	ret = -EINVAL;
	if (!PageCgroupUsed(pc) || pc->mem_cgroup != from)
		goto unlock;

	move_lock_mem_cgroup(from, &flags);

	if (!anon && page_mapped(page)) {
		
		preempt_disable();
		__this_cpu_dec(from->stat->count[MEM_CGROUP_STAT_FILE_MAPPED]);
		__this_cpu_inc(to->stat->count[MEM_CGROUP_STAT_FILE_MAPPED]);
		preempt_enable();
	}
	mem_cgroup_charge_statistics(from, anon, -nr_pages);
	if (uncharge)
		
		__mem_cgroup_cancel_charge(from, nr_pages);

	
	pc->mem_cgroup = to;
	mem_cgroup_charge_statistics(to, anon, nr_pages);
	move_unlock_mem_cgroup(from, &flags);
	ret = 0;
unlock:
	unlock_page_cgroup(pc);
	memcg_check_events(to, page);
	memcg_check_events(from, page);
out:
	return ret;
}


static int mem_cgroup_move_parent(struct page *page,
				  struct page_cgroup *pc,
				  struct mem_cgroup *child,
				  gfp_t gfp_mask)
{
	struct cgroup *cg = child->css.cgroup;
	struct cgroup *pcg = cg->parent;
	struct mem_cgroup *parent;
	unsigned int nr_pages;
	unsigned long uninitialized_var(flags);
	int ret;

	
	if (!pcg)
		return -EINVAL;

	ret = -EBUSY;
	if (!get_page_unless_zero(page))
		goto out;
	if (isolate_lru_page(page))
		goto put;

	nr_pages = hpage_nr_pages(page);

	parent = mem_cgroup_from_cont(pcg);
	ret = __mem_cgroup_try_charge(NULL, gfp_mask, nr_pages, &parent, false);
	if (ret)
		goto put_back;

	if (nr_pages > 1)
		flags = compound_lock_irqsave(page);

	ret = mem_cgroup_move_account(page, nr_pages, pc, child, parent, true);
	if (ret)
		__mem_cgroup_cancel_charge(parent, nr_pages);

	if (nr_pages > 1)
		compound_unlock_irqrestore(page, flags);
put_back:
	putback_lru_page(page);
put:
	put_page(page);
out:
	return ret;
}

static int mem_cgroup_charge_common(struct page *page, struct mm_struct *mm,
				gfp_t gfp_mask, enum charge_type ctype)
{
	struct mem_cgroup *memcg = NULL;
	unsigned int nr_pages = 1;
	bool oom = true;
	int ret;

	if (PageTransHuge(page)) {
		nr_pages <<= compound_order(page);
		VM_BUG_ON(!PageTransHuge(page));
		oom = false;
	}

	ret = __mem_cgroup_try_charge(mm, gfp_mask, nr_pages, &memcg, oom);
	if (ret == -ENOMEM)
		return ret;
	__mem_cgroup_commit_charge(memcg, page, nr_pages, ctype, false);
	return 0;
}

int mem_cgroup_newpage_charge(struct page *page,
			      struct mm_struct *mm, gfp_t gfp_mask)
{
	if (mem_cgroup_disabled())
		return 0;
	VM_BUG_ON(page_mapped(page));
	VM_BUG_ON(page->mapping && !PageAnon(page));
	VM_BUG_ON(!mm);
	return mem_cgroup_charge_common(page, mm, gfp_mask,
					MEM_CGROUP_CHARGE_TYPE_MAPPED);
}

static void
__mem_cgroup_commit_charge_swapin(struct page *page, struct mem_cgroup *ptr,
					enum charge_type ctype);

int mem_cgroup_cache_charge(struct page *page, struct mm_struct *mm,
				gfp_t gfp_mask)
{
	struct mem_cgroup *memcg = NULL;
	enum charge_type type = MEM_CGROUP_CHARGE_TYPE_CACHE;
	int ret;

	if (mem_cgroup_disabled())
		return 0;
	if (PageCompound(page))
		return 0;

	if (unlikely(!mm))
		mm = &init_mm;
	if (!page_is_file_cache(page))
		type = MEM_CGROUP_CHARGE_TYPE_SHMEM;

	if (!PageSwapCache(page))
		ret = mem_cgroup_charge_common(page, mm, gfp_mask, type);
	else { 
		ret = mem_cgroup_try_charge_swapin(mm, page, gfp_mask, &memcg);
		if (!ret)
			__mem_cgroup_commit_charge_swapin(page, memcg, type);
	}
	return ret;
}

int mem_cgroup_try_charge_swapin(struct mm_struct *mm,
				 struct page *page,
				 gfp_t mask, struct mem_cgroup **memcgp)
{
	struct mem_cgroup *memcg;
	int ret;

	*memcgp = NULL;

	if (mem_cgroup_disabled())
		return 0;

	if (!do_swap_account)
		goto charge_cur_mm;
	if (!PageSwapCache(page))
		goto charge_cur_mm;
	memcg = try_get_mem_cgroup_from_page(page);
	if (!memcg)
		goto charge_cur_mm;
	*memcgp = memcg;
	ret = __mem_cgroup_try_charge(NULL, mask, 1, memcgp, true);
	css_put(&memcg->css);
	if (ret == -EINTR)
		ret = 0;
	return ret;
charge_cur_mm:
	if (unlikely(!mm))
		mm = &init_mm;
	ret = __mem_cgroup_try_charge(mm, mask, 1, memcgp, true);
	if (ret == -EINTR)
		ret = 0;
	return ret;
}

static void
__mem_cgroup_commit_charge_swapin(struct page *page, struct mem_cgroup *memcg,
					enum charge_type ctype)
{
	if (mem_cgroup_disabled())
		return;
	if (!memcg)
		return;
	cgroup_exclude_rmdir(&memcg->css);

	__mem_cgroup_commit_charge(memcg, page, 1, ctype, true);
	if (do_swap_account && PageSwapCache(page)) {
		swp_entry_t ent = {.val = page_private(page)};
		struct mem_cgroup *swap_memcg;
		unsigned short id;

		id = swap_cgroup_record(ent, 0);
		rcu_read_lock();
		swap_memcg = mem_cgroup_lookup(id);
		if (swap_memcg) {
			if (!mem_cgroup_is_root(swap_memcg))
				res_counter_uncharge(&swap_memcg->memsw,
						     PAGE_SIZE);
			mem_cgroup_swap_statistics(swap_memcg, false);
			mem_cgroup_put(swap_memcg);
		}
		rcu_read_unlock();
	}
	cgroup_release_and_wakeup_rmdir(&memcg->css);
}

void mem_cgroup_commit_charge_swapin(struct page *page,
				     struct mem_cgroup *memcg)
{
	__mem_cgroup_commit_charge_swapin(page, memcg,
					  MEM_CGROUP_CHARGE_TYPE_MAPPED);
}

void mem_cgroup_cancel_charge_swapin(struct mem_cgroup *memcg)
{
	if (mem_cgroup_disabled())
		return;
	if (!memcg)
		return;
	__mem_cgroup_cancel_charge(memcg, 1);
}

static void mem_cgroup_do_uncharge(struct mem_cgroup *memcg,
				   unsigned int nr_pages,
				   const enum charge_type ctype)
{
	struct memcg_batch_info *batch = NULL;
	bool uncharge_memsw = true;

	
	if (!do_swap_account || ctype == MEM_CGROUP_CHARGE_TYPE_SWAPOUT)
		uncharge_memsw = false;

	batch = &current->memcg_batch;
	if (!batch->memcg)
		batch->memcg = memcg;

	if (!batch->do_batch || test_thread_flag(TIF_MEMDIE))
		goto direct_uncharge;

	if (nr_pages > 1)
		goto direct_uncharge;

	if (batch->memcg != memcg)
		goto direct_uncharge;
	
	batch->nr_pages++;
	if (uncharge_memsw)
		batch->memsw_nr_pages++;
	return;
direct_uncharge:
	res_counter_uncharge(&memcg->res, nr_pages * PAGE_SIZE);
	if (uncharge_memsw)
		res_counter_uncharge(&memcg->memsw, nr_pages * PAGE_SIZE);
	if (unlikely(batch->memcg != memcg))
		memcg_oom_recover(memcg);
}

static struct mem_cgroup *
__mem_cgroup_uncharge_common(struct page *page, enum charge_type ctype)
{
	struct mem_cgroup *memcg = NULL;
	unsigned int nr_pages = 1;
	struct page_cgroup *pc;
	bool anon;

	if (mem_cgroup_disabled())
		return NULL;

	if (PageSwapCache(page))
		return NULL;

	if (PageTransHuge(page)) {
		nr_pages <<= compound_order(page);
		VM_BUG_ON(!PageTransHuge(page));
	}
	pc = lookup_page_cgroup(page);
	if (unlikely(!PageCgroupUsed(pc)))
		return NULL;

	lock_page_cgroup(pc);

	memcg = pc->mem_cgroup;

	if (!PageCgroupUsed(pc))
		goto unlock_out;

	anon = PageAnon(page);

	switch (ctype) {
	case MEM_CGROUP_CHARGE_TYPE_MAPPED:
		anon = true;
		
	case MEM_CGROUP_CHARGE_TYPE_DROP:
		
		if (page_mapped(page) || PageCgroupMigration(pc))
			goto unlock_out;
		break;
	case MEM_CGROUP_CHARGE_TYPE_SWAPOUT:
		if (!PageAnon(page)) {	
			if (page->mapping && !page_is_file_cache(page))
				goto unlock_out;
		} else if (page_mapped(page)) 
				goto unlock_out;
		break;
	default:
		break;
	}

	mem_cgroup_charge_statistics(memcg, anon, -nr_pages);

	ClearPageCgroupUsed(pc);

	unlock_page_cgroup(pc);
	memcg_check_events(memcg, page);
	if (do_swap_account && ctype == MEM_CGROUP_CHARGE_TYPE_SWAPOUT) {
		mem_cgroup_swap_statistics(memcg, true);
		mem_cgroup_get(memcg);
	}
	if (!mem_cgroup_is_root(memcg))
		mem_cgroup_do_uncharge(memcg, nr_pages, ctype);

	return memcg;

unlock_out:
	unlock_page_cgroup(pc);
	return NULL;
}

void mem_cgroup_uncharge_page(struct page *page)
{
	
	if (page_mapped(page))
		return;
	VM_BUG_ON(page->mapping && !PageAnon(page));
	__mem_cgroup_uncharge_common(page, MEM_CGROUP_CHARGE_TYPE_MAPPED);
}

void mem_cgroup_uncharge_cache_page(struct page *page)
{
	VM_BUG_ON(page_mapped(page));
	VM_BUG_ON(page->mapping);
	__mem_cgroup_uncharge_common(page, MEM_CGROUP_CHARGE_TYPE_CACHE);
}


void mem_cgroup_uncharge_start(void)
{
	current->memcg_batch.do_batch++;
	
	if (current->memcg_batch.do_batch == 1) {
		current->memcg_batch.memcg = NULL;
		current->memcg_batch.nr_pages = 0;
		current->memcg_batch.memsw_nr_pages = 0;
	}
}

void mem_cgroup_uncharge_end(void)
{
	struct memcg_batch_info *batch = &current->memcg_batch;

	if (!batch->do_batch)
		return;

	batch->do_batch--;
	if (batch->do_batch) 
		return;

	if (!batch->memcg)
		return;
	if (batch->nr_pages)
		res_counter_uncharge(&batch->memcg->res,
				     batch->nr_pages * PAGE_SIZE);
	if (batch->memsw_nr_pages)
		res_counter_uncharge(&batch->memcg->memsw,
				     batch->memsw_nr_pages * PAGE_SIZE);
	memcg_oom_recover(batch->memcg);
	
	batch->memcg = NULL;
}

#ifdef CONFIG_SWAP
void
mem_cgroup_uncharge_swapcache(struct page *page, swp_entry_t ent, bool swapout)
{
	struct mem_cgroup *memcg;
	int ctype = MEM_CGROUP_CHARGE_TYPE_SWAPOUT;

	if (!swapout) 
		ctype = MEM_CGROUP_CHARGE_TYPE_DROP;

	memcg = __mem_cgroup_uncharge_common(page, ctype);

	if (do_swap_account && swapout && memcg)
		swap_cgroup_record(ent, css_id(&memcg->css));
}
#endif

#ifdef CONFIG_CGROUP_MEM_RES_CTLR_SWAP
void mem_cgroup_uncharge_swap(swp_entry_t ent)
{
	struct mem_cgroup *memcg;
	unsigned short id;

	if (!do_swap_account)
		return;

	id = swap_cgroup_record(ent, 0);
	rcu_read_lock();
	memcg = mem_cgroup_lookup(id);
	if (memcg) {
		if (!mem_cgroup_is_root(memcg))
			res_counter_uncharge(&memcg->memsw, PAGE_SIZE);
		mem_cgroup_swap_statistics(memcg, false);
		mem_cgroup_put(memcg);
	}
	rcu_read_unlock();
}

static int mem_cgroup_move_swap_account(swp_entry_t entry,
		struct mem_cgroup *from, struct mem_cgroup *to, bool need_fixup)
{
	unsigned short old_id, new_id;

	old_id = css_id(&from->css);
	new_id = css_id(&to->css);

	if (swap_cgroup_cmpxchg(entry, old_id, new_id) == old_id) {
		mem_cgroup_swap_statistics(from, false);
		mem_cgroup_swap_statistics(to, true);
		mem_cgroup_get(to);
		if (need_fixup) {
			if (!mem_cgroup_is_root(from))
				res_counter_uncharge(&from->memsw, PAGE_SIZE);
			mem_cgroup_put(from);
			if (!mem_cgroup_is_root(to))
				res_counter_uncharge(&to->res, PAGE_SIZE);
		}
		return 0;
	}
	return -EINVAL;
}
#else
static inline int mem_cgroup_move_swap_account(swp_entry_t entry,
		struct mem_cgroup *from, struct mem_cgroup *to, bool need_fixup)
{
	return -EINVAL;
}
#endif

int mem_cgroup_prepare_migration(struct page *page,
	struct page *newpage, struct mem_cgroup **memcgp, gfp_t gfp_mask)
{
	struct mem_cgroup *memcg = NULL;
	struct page_cgroup *pc;
	enum charge_type ctype;
	int ret = 0;

	*memcgp = NULL;

	VM_BUG_ON(PageTransHuge(page));
	if (mem_cgroup_disabled())
		return 0;

	pc = lookup_page_cgroup(page);
	lock_page_cgroup(pc);
	if (PageCgroupUsed(pc)) {
		memcg = pc->mem_cgroup;
		css_get(&memcg->css);
		if (PageAnon(page))
			SetPageCgroupMigration(pc);
	}
	unlock_page_cgroup(pc);
	if (!memcg)
		return 0;

	*memcgp = memcg;
	ret = __mem_cgroup_try_charge(NULL, gfp_mask, 1, memcgp, false);
	css_put(&memcg->css);
	if (ret) {
		if (PageAnon(page)) {
			lock_page_cgroup(pc);
			ClearPageCgroupMigration(pc);
			unlock_page_cgroup(pc);
			mem_cgroup_uncharge_page(page);
		}
		
		return -ENOMEM;
	}
	if (PageAnon(page))
		ctype = MEM_CGROUP_CHARGE_TYPE_MAPPED;
	else if (page_is_file_cache(page))
		ctype = MEM_CGROUP_CHARGE_TYPE_CACHE;
	else
		ctype = MEM_CGROUP_CHARGE_TYPE_SHMEM;
	__mem_cgroup_commit_charge(memcg, newpage, 1, ctype, false);
	return ret;
}

void mem_cgroup_end_migration(struct mem_cgroup *memcg,
	struct page *oldpage, struct page *newpage, bool migration_ok)
{
	struct page *used, *unused;
	struct page_cgroup *pc;
	bool anon;

	if (!memcg)
		return;
	
	cgroup_exclude_rmdir(&memcg->css);
	if (!migration_ok) {
		used = oldpage;
		unused = newpage;
	} else {
		used = newpage;
		unused = oldpage;
	}
	pc = lookup_page_cgroup(oldpage);
	lock_page_cgroup(pc);
	ClearPageCgroupMigration(pc);
	unlock_page_cgroup(pc);
	anon = PageAnon(used);
	__mem_cgroup_uncharge_common(unused,
		anon ? MEM_CGROUP_CHARGE_TYPE_MAPPED
		     : MEM_CGROUP_CHARGE_TYPE_CACHE);

	if (anon)
		mem_cgroup_uncharge_page(used);
	cgroup_release_and_wakeup_rmdir(&memcg->css);
}

void mem_cgroup_replace_page_cache(struct page *oldpage,
				  struct page *newpage)
{
	struct mem_cgroup *memcg;
	struct page_cgroup *pc;
	enum charge_type type = MEM_CGROUP_CHARGE_TYPE_CACHE;

	if (mem_cgroup_disabled())
		return;

	pc = lookup_page_cgroup(oldpage);
	
	lock_page_cgroup(pc);
	memcg = pc->mem_cgroup;
	mem_cgroup_charge_statistics(memcg, false, -1);
	ClearPageCgroupUsed(pc);
	unlock_page_cgroup(pc);

	if (PageSwapBacked(oldpage))
		type = MEM_CGROUP_CHARGE_TYPE_SHMEM;

	__mem_cgroup_commit_charge(memcg, newpage, 1, type, true);
}

#ifdef CONFIG_DEBUG_VM
static struct page_cgroup *lookup_page_cgroup_used(struct page *page)
{
	struct page_cgroup *pc;

	pc = lookup_page_cgroup(page);
	if (likely(pc) && PageCgroupUsed(pc))
		return pc;
	return NULL;
}

bool mem_cgroup_bad_page_check(struct page *page)
{
	if (mem_cgroup_disabled())
		return false;

	return lookup_page_cgroup_used(page) != NULL;
}

void mem_cgroup_print_bad_page(struct page *page)
{
	struct page_cgroup *pc;

	pc = lookup_page_cgroup_used(page);
	if (pc) {
		printk(KERN_ALERT "pc:%p pc->flags:%lx pc->mem_cgroup:%p\n",
		       pc, pc->flags, pc->mem_cgroup);
	}
}
#endif

static DEFINE_MUTEX(set_limit_mutex);

static int mem_cgroup_resize_limit(struct mem_cgroup *memcg,
				unsigned long long val)
{
	int retry_count;
	u64 memswlimit, memlimit;
	int ret = 0;
	int children = mem_cgroup_count_children(memcg);
	u64 curusage, oldusage;
	int enlarge;

	retry_count = MEM_CGROUP_RECLAIM_RETRIES * children;

	oldusage = res_counter_read_u64(&memcg->res, RES_USAGE);

	enlarge = 0;
	while (retry_count) {
		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}
		mutex_lock(&set_limit_mutex);
		memswlimit = res_counter_read_u64(&memcg->memsw, RES_LIMIT);
		if (memswlimit < val) {
			ret = -EINVAL;
			mutex_unlock(&set_limit_mutex);
			break;
		}

		memlimit = res_counter_read_u64(&memcg->res, RES_LIMIT);
		if (memlimit < val)
			enlarge = 1;

		ret = res_counter_set_limit(&memcg->res, val);
		if (!ret) {
			if (memswlimit == val)
				memcg->memsw_is_minimum = true;
			else
				memcg->memsw_is_minimum = false;
		}
		mutex_unlock(&set_limit_mutex);

		if (!ret)
			break;

		mem_cgroup_reclaim(memcg, GFP_KERNEL,
				   MEM_CGROUP_RECLAIM_SHRINK);
		curusage = res_counter_read_u64(&memcg->res, RES_USAGE);
		
  		if (curusage >= oldusage)
			retry_count--;
		else
			oldusage = curusage;
	}
	if (!ret && enlarge)
		memcg_oom_recover(memcg);

	return ret;
}

static int mem_cgroup_resize_memsw_limit(struct mem_cgroup *memcg,
					unsigned long long val)
{
	int retry_count;
	u64 memlimit, memswlimit, oldusage, curusage;
	int children = mem_cgroup_count_children(memcg);
	int ret = -EBUSY;
	int enlarge = 0;

	
 	retry_count = children * MEM_CGROUP_RECLAIM_RETRIES;
	oldusage = res_counter_read_u64(&memcg->memsw, RES_USAGE);
	while (retry_count) {
		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}
		mutex_lock(&set_limit_mutex);
		memlimit = res_counter_read_u64(&memcg->res, RES_LIMIT);
		if (memlimit > val) {
			ret = -EINVAL;
			mutex_unlock(&set_limit_mutex);
			break;
		}
		memswlimit = res_counter_read_u64(&memcg->memsw, RES_LIMIT);
		if (memswlimit < val)
			enlarge = 1;
		ret = res_counter_set_limit(&memcg->memsw, val);
		if (!ret) {
			if (memlimit == val)
				memcg->memsw_is_minimum = true;
			else
				memcg->memsw_is_minimum = false;
		}
		mutex_unlock(&set_limit_mutex);

		if (!ret)
			break;

		mem_cgroup_reclaim(memcg, GFP_KERNEL,
				   MEM_CGROUP_RECLAIM_NOSWAP |
				   MEM_CGROUP_RECLAIM_SHRINK);
		curusage = res_counter_read_u64(&memcg->memsw, RES_USAGE);
		
		if (curusage >= oldusage)
			retry_count--;
		else
			oldusage = curusage;
	}
	if (!ret && enlarge)
		memcg_oom_recover(memcg);
	return ret;
}

unsigned long mem_cgroup_soft_limit_reclaim(struct zone *zone, int order,
					    gfp_t gfp_mask,
					    unsigned long *total_scanned)
{
	unsigned long nr_reclaimed = 0;
	struct mem_cgroup_per_zone *mz, *next_mz = NULL;
	unsigned long reclaimed;
	int loop = 0;
	struct mem_cgroup_tree_per_zone *mctz;
	unsigned long long excess;
	unsigned long nr_scanned;

	if (order > 0)
		return 0;

	mctz = soft_limit_tree_node_zone(zone_to_nid(zone), zone_idx(zone));
	do {
		if (next_mz)
			mz = next_mz;
		else
			mz = mem_cgroup_largest_soft_limit_node(mctz);
		if (!mz)
			break;

		nr_scanned = 0;
		reclaimed = mem_cgroup_soft_reclaim(mz->memcg, zone,
						    gfp_mask, &nr_scanned);
		nr_reclaimed += reclaimed;
		*total_scanned += nr_scanned;
		spin_lock(&mctz->lock);

		next_mz = NULL;
		if (!reclaimed) {
			do {
				next_mz =
				__mem_cgroup_largest_soft_limit_node(mctz);
				if (next_mz == mz)
					css_put(&next_mz->memcg->css);
				else 
					break;
			} while (1);
		}
		__mem_cgroup_remove_exceeded(mz->memcg, mz, mctz);
		excess = res_counter_soft_limit_excess(&mz->memcg->res);
		
		__mem_cgroup_insert_exceeded(mz->memcg, mz, mctz, excess);
		spin_unlock(&mctz->lock);
		css_put(&mz->memcg->css);
		loop++;
		if (!nr_reclaimed &&
			(next_mz == NULL ||
			loop > MEM_CGROUP_MAX_SOFT_LIMIT_RECLAIM_LOOPS))
			break;
	} while (!nr_reclaimed);
	if (next_mz)
		css_put(&next_mz->memcg->css);
	return nr_reclaimed;
}

static int mem_cgroup_force_empty_list(struct mem_cgroup *memcg,
				int node, int zid, enum lru_list lru)
{
	struct mem_cgroup_per_zone *mz;
	unsigned long flags, loop;
	struct list_head *list;
	struct page *busy;
	struct zone *zone;
	int ret = 0;

	zone = &NODE_DATA(node)->node_zones[zid];
	mz = mem_cgroup_zoneinfo(memcg, node, zid);
	list = &mz->lruvec.lists[lru];

	loop = mz->lru_size[lru];
	
	loop += 256;
	busy = NULL;
	while (loop--) {
		struct page_cgroup *pc;
		struct page *page;

		ret = 0;
		spin_lock_irqsave(&zone->lru_lock, flags);
		if (list_empty(list)) {
			spin_unlock_irqrestore(&zone->lru_lock, flags);
			break;
		}
		page = list_entry(list->prev, struct page, lru);
		if (busy == page) {
			list_move(&page->lru, list);
			busy = NULL;
			spin_unlock_irqrestore(&zone->lru_lock, flags);
			continue;
		}
		spin_unlock_irqrestore(&zone->lru_lock, flags);

		pc = lookup_page_cgroup(page);

		ret = mem_cgroup_move_parent(page, pc, memcg, GFP_KERNEL);
		if (ret == -ENOMEM || ret == -EINTR)
			break;

		if (ret == -EBUSY || ret == -EINVAL) {
			
			busy = page;
			cond_resched();
		} else
			busy = NULL;
	}

	if (!ret && !list_empty(list))
		return -EBUSY;
	return ret;
}

static int mem_cgroup_force_empty(struct mem_cgroup *memcg, bool free_all)
{
	int ret;
	int node, zid, shrink;
	int nr_retries = MEM_CGROUP_RECLAIM_RETRIES;
	struct cgroup *cgrp = memcg->css.cgroup;

	css_get(&memcg->css);

	shrink = 0;
	
	if (free_all)
		goto try_to_free;
move_account:
	do {
		ret = -EBUSY;
		if (cgroup_task_count(cgrp) || !list_empty(&cgrp->children))
			goto out;
		ret = -EINTR;
		if (signal_pending(current))
			goto out;
		
		lru_add_drain_all();
		drain_all_stock_sync(memcg);
		ret = 0;
		mem_cgroup_start_move(memcg);
		for_each_node_state(node, N_HIGH_MEMORY) {
			for (zid = 0; !ret && zid < MAX_NR_ZONES; zid++) {
				enum lru_list lru;
				for_each_lru(lru) {
					ret = mem_cgroup_force_empty_list(memcg,
							node, zid, lru);
					if (ret)
						break;
				}
			}
			if (ret)
				break;
		}
		mem_cgroup_end_move(memcg);
		memcg_oom_recover(memcg);
		
		if (ret == -ENOMEM)
			goto try_to_free;
		cond_resched();
	
	} while (res_counter_read_u64(&memcg->res, RES_USAGE) > 0 || ret);
out:
	css_put(&memcg->css);
	return ret;

try_to_free:
	
	if (cgroup_task_count(cgrp) || !list_empty(&cgrp->children) || shrink) {
		ret = -EBUSY;
		goto out;
	}
	
	lru_add_drain_all();
	
	shrink = 1;
	while (nr_retries && res_counter_read_u64(&memcg->res, RES_USAGE) > 0) {
		int progress;

		if (signal_pending(current)) {
			ret = -EINTR;
			goto out;
		}
		progress = try_to_free_mem_cgroup_pages(memcg, GFP_KERNEL,
						false);
		if (!progress) {
			nr_retries--;
			
			congestion_wait(BLK_RW_ASYNC, HZ/10);
		}

	}
	lru_add_drain();
	
	goto move_account;
}

int mem_cgroup_force_empty_write(struct cgroup *cont, unsigned int event)
{
	return mem_cgroup_force_empty(mem_cgroup_from_cont(cont), true);
}


static u64 mem_cgroup_hierarchy_read(struct cgroup *cont, struct cftype *cft)
{
	return mem_cgroup_from_cont(cont)->use_hierarchy;
}

static int mem_cgroup_hierarchy_write(struct cgroup *cont, struct cftype *cft,
					u64 val)
{
	int retval = 0;
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cont);
	struct cgroup *parent = cont->parent;
	struct mem_cgroup *parent_memcg = NULL;

	if (parent)
		parent_memcg = mem_cgroup_from_cont(parent);

	cgroup_lock();
	if ((!parent_memcg || !parent_memcg->use_hierarchy) &&
				(val == 1 || val == 0)) {
		if (list_empty(&cont->children))
			memcg->use_hierarchy = val;
		else
			retval = -EBUSY;
	} else
		retval = -EINVAL;
	cgroup_unlock();

	return retval;
}


static unsigned long mem_cgroup_recursive_stat(struct mem_cgroup *memcg,
					       enum mem_cgroup_stat_index idx)
{
	struct mem_cgroup *iter;
	long val = 0;

	
	for_each_mem_cgroup_tree(iter, memcg)
		val += mem_cgroup_read_stat(iter, idx);

	if (val < 0) 
		val = 0;
	return val;
}

static inline u64 mem_cgroup_usage(struct mem_cgroup *memcg, bool swap)
{
	u64 val;

	if (!mem_cgroup_is_root(memcg)) {
		if (!swap)
			return res_counter_read_u64(&memcg->res, RES_USAGE);
		else
			return res_counter_read_u64(&memcg->memsw, RES_USAGE);
	}

	val = mem_cgroup_recursive_stat(memcg, MEM_CGROUP_STAT_CACHE);
	val += mem_cgroup_recursive_stat(memcg, MEM_CGROUP_STAT_RSS);

	if (swap)
		val += mem_cgroup_recursive_stat(memcg, MEM_CGROUP_STAT_SWAPOUT);

	return val << PAGE_SHIFT;
}

static u64 mem_cgroup_read(struct cgroup *cont, struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cont);
	u64 val;
	int type, name;

	type = MEMFILE_TYPE(cft->private);
	name = MEMFILE_ATTR(cft->private);
	switch (type) {
	case _MEM:
		if (name == RES_USAGE)
			val = mem_cgroup_usage(memcg, false);
		else
			val = res_counter_read_u64(&memcg->res, name);
		break;
	case _MEMSWAP:
		if (name == RES_USAGE)
			val = mem_cgroup_usage(memcg, true);
		else
			val = res_counter_read_u64(&memcg->memsw, name);
		break;
	default:
		BUG();
	}
	return val;
}
static int mem_cgroup_write(struct cgroup *cont, struct cftype *cft,
			    const char *buffer)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cont);
	int type, name;
	unsigned long long val;
	int ret;

	type = MEMFILE_TYPE(cft->private);
	name = MEMFILE_ATTR(cft->private);
	switch (name) {
	case RES_LIMIT:
		if (mem_cgroup_is_root(memcg)) { 
			ret = -EINVAL;
			break;
		}
		
		ret = res_counter_memparse_write_strategy(buffer, &val);
		if (ret)
			break;
		if (type == _MEM)
			ret = mem_cgroup_resize_limit(memcg, val);
		else
			ret = mem_cgroup_resize_memsw_limit(memcg, val);
		break;
	case RES_SOFT_LIMIT:
		ret = res_counter_memparse_write_strategy(buffer, &val);
		if (ret)
			break;
		if (type == _MEM)
			ret = res_counter_set_soft_limit(&memcg->res, val);
		else
			ret = -EINVAL;
		break;
	default:
		ret = -EINVAL; 
		break;
	}
	return ret;
}

static void memcg_get_hierarchical_limit(struct mem_cgroup *memcg,
		unsigned long long *mem_limit, unsigned long long *memsw_limit)
{
	struct cgroup *cgroup;
	unsigned long long min_limit, min_memsw_limit, tmp;

	min_limit = res_counter_read_u64(&memcg->res, RES_LIMIT);
	min_memsw_limit = res_counter_read_u64(&memcg->memsw, RES_LIMIT);
	cgroup = memcg->css.cgroup;
	if (!memcg->use_hierarchy)
		goto out;

	while (cgroup->parent) {
		cgroup = cgroup->parent;
		memcg = mem_cgroup_from_cont(cgroup);
		if (!memcg->use_hierarchy)
			break;
		tmp = res_counter_read_u64(&memcg->res, RES_LIMIT);
		min_limit = min(min_limit, tmp);
		tmp = res_counter_read_u64(&memcg->memsw, RES_LIMIT);
		min_memsw_limit = min(min_memsw_limit, tmp);
	}
out:
	*mem_limit = min_limit;
	*memsw_limit = min_memsw_limit;
}

static int mem_cgroup_reset(struct cgroup *cont, unsigned int event)
{
	struct mem_cgroup *memcg;
	int type, name;

	memcg = mem_cgroup_from_cont(cont);
	type = MEMFILE_TYPE(event);
	name = MEMFILE_ATTR(event);
	switch (name) {
	case RES_MAX_USAGE:
		if (type == _MEM)
			res_counter_reset_max(&memcg->res);
		else
			res_counter_reset_max(&memcg->memsw);
		break;
	case RES_FAILCNT:
		if (type == _MEM)
			res_counter_reset_failcnt(&memcg->res);
		else
			res_counter_reset_failcnt(&memcg->memsw);
		break;
	}

	return 0;
}

static u64 mem_cgroup_move_charge_read(struct cgroup *cgrp,
					struct cftype *cft)
{
	return mem_cgroup_from_cont(cgrp)->move_charge_at_immigrate;
}

#ifdef CONFIG_MMU
static int mem_cgroup_move_charge_write(struct cgroup *cgrp,
					struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgrp);

	if (val >= (1 << NR_MOVE_TYPE))
		return -EINVAL;
	cgroup_lock();
	memcg->move_charge_at_immigrate = val;
	cgroup_unlock();

	return 0;
}
#else
static int mem_cgroup_move_charge_write(struct cgroup *cgrp,
					struct cftype *cft, u64 val)
{
	return -ENOSYS;
}
#endif


enum {
	MCS_CACHE,
	MCS_RSS,
	MCS_FILE_MAPPED,
	MCS_PGPGIN,
	MCS_PGPGOUT,
	MCS_SWAP,
	MCS_PGFAULT,
	MCS_PGMAJFAULT,
	MCS_INACTIVE_ANON,
	MCS_ACTIVE_ANON,
	MCS_INACTIVE_FILE,
	MCS_ACTIVE_FILE,
	MCS_UNEVICTABLE,
	NR_MCS_STAT,
};

struct mcs_total_stat {
	s64 stat[NR_MCS_STAT];
};

struct {
	char *local_name;
	char *total_name;
} memcg_stat_strings[NR_MCS_STAT] = {
	{"cache", "total_cache"},
	{"rss", "total_rss"},
	{"mapped_file", "total_mapped_file"},
	{"pgpgin", "total_pgpgin"},
	{"pgpgout", "total_pgpgout"},
	{"swap", "total_swap"},
	{"pgfault", "total_pgfault"},
	{"pgmajfault", "total_pgmajfault"},
	{"inactive_anon", "total_inactive_anon"},
	{"active_anon", "total_active_anon"},
	{"inactive_file", "total_inactive_file"},
	{"active_file", "total_active_file"},
	{"unevictable", "total_unevictable"}
};


static void
mem_cgroup_get_local_stat(struct mem_cgroup *memcg, struct mcs_total_stat *s)
{
	s64 val;

	
	val = mem_cgroup_read_stat(memcg, MEM_CGROUP_STAT_CACHE);
	s->stat[MCS_CACHE] += val * PAGE_SIZE;
	val = mem_cgroup_read_stat(memcg, MEM_CGROUP_STAT_RSS);
	s->stat[MCS_RSS] += val * PAGE_SIZE;
	val = mem_cgroup_read_stat(memcg, MEM_CGROUP_STAT_FILE_MAPPED);
	s->stat[MCS_FILE_MAPPED] += val * PAGE_SIZE;
	val = mem_cgroup_read_events(memcg, MEM_CGROUP_EVENTS_PGPGIN);
	s->stat[MCS_PGPGIN] += val;
	val = mem_cgroup_read_events(memcg, MEM_CGROUP_EVENTS_PGPGOUT);
	s->stat[MCS_PGPGOUT] += val;
	if (do_swap_account) {
		val = mem_cgroup_read_stat(memcg, MEM_CGROUP_STAT_SWAPOUT);
		s->stat[MCS_SWAP] += val * PAGE_SIZE;
	}
	val = mem_cgroup_read_events(memcg, MEM_CGROUP_EVENTS_PGFAULT);
	s->stat[MCS_PGFAULT] += val;
	val = mem_cgroup_read_events(memcg, MEM_CGROUP_EVENTS_PGMAJFAULT);
	s->stat[MCS_PGMAJFAULT] += val;

	
	val = mem_cgroup_nr_lru_pages(memcg, BIT(LRU_INACTIVE_ANON));
	s->stat[MCS_INACTIVE_ANON] += val * PAGE_SIZE;
	val = mem_cgroup_nr_lru_pages(memcg, BIT(LRU_ACTIVE_ANON));
	s->stat[MCS_ACTIVE_ANON] += val * PAGE_SIZE;
	val = mem_cgroup_nr_lru_pages(memcg, BIT(LRU_INACTIVE_FILE));
	s->stat[MCS_INACTIVE_FILE] += val * PAGE_SIZE;
	val = mem_cgroup_nr_lru_pages(memcg, BIT(LRU_ACTIVE_FILE));
	s->stat[MCS_ACTIVE_FILE] += val * PAGE_SIZE;
	val = mem_cgroup_nr_lru_pages(memcg, BIT(LRU_UNEVICTABLE));
	s->stat[MCS_UNEVICTABLE] += val * PAGE_SIZE;
}

static void
mem_cgroup_get_total_stat(struct mem_cgroup *memcg, struct mcs_total_stat *s)
{
	struct mem_cgroup *iter;

	for_each_mem_cgroup_tree(iter, memcg)
		mem_cgroup_get_local_stat(iter, s);
}

#ifdef CONFIG_NUMA
static int mem_control_numa_stat_show(struct seq_file *m, void *arg)
{
	int nid;
	unsigned long total_nr, file_nr, anon_nr, unevictable_nr;
	unsigned long node_nr;
	struct cgroup *cont = m->private;
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cont);

	total_nr = mem_cgroup_nr_lru_pages(memcg, LRU_ALL);
	seq_printf(m, "total=%lu", total_nr);
	for_each_node_state(nid, N_HIGH_MEMORY) {
		node_nr = mem_cgroup_node_nr_lru_pages(memcg, nid, LRU_ALL);
		seq_printf(m, " N%d=%lu", nid, node_nr);
	}
	seq_putc(m, '\n');

	file_nr = mem_cgroup_nr_lru_pages(memcg, LRU_ALL_FILE);
	seq_printf(m, "file=%lu", file_nr);
	for_each_node_state(nid, N_HIGH_MEMORY) {
		node_nr = mem_cgroup_node_nr_lru_pages(memcg, nid,
				LRU_ALL_FILE);
		seq_printf(m, " N%d=%lu", nid, node_nr);
	}
	seq_putc(m, '\n');

	anon_nr = mem_cgroup_nr_lru_pages(memcg, LRU_ALL_ANON);
	seq_printf(m, "anon=%lu", anon_nr);
	for_each_node_state(nid, N_HIGH_MEMORY) {
		node_nr = mem_cgroup_node_nr_lru_pages(memcg, nid,
				LRU_ALL_ANON);
		seq_printf(m, " N%d=%lu", nid, node_nr);
	}
	seq_putc(m, '\n');

	unevictable_nr = mem_cgroup_nr_lru_pages(memcg, BIT(LRU_UNEVICTABLE));
	seq_printf(m, "unevictable=%lu", unevictable_nr);
	for_each_node_state(nid, N_HIGH_MEMORY) {
		node_nr = mem_cgroup_node_nr_lru_pages(memcg, nid,
				BIT(LRU_UNEVICTABLE));
		seq_printf(m, " N%d=%lu", nid, node_nr);
	}
	seq_putc(m, '\n');
	return 0;
}
#endif 

static int mem_control_stat_show(struct cgroup *cont, struct cftype *cft,
				 struct cgroup_map_cb *cb)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cont);
	struct mcs_total_stat mystat;
	int i;

	memset(&mystat, 0, sizeof(mystat));
	mem_cgroup_get_local_stat(memcg, &mystat);


	for (i = 0; i < NR_MCS_STAT; i++) {
		if (i == MCS_SWAP && !do_swap_account)
			continue;
		cb->fill(cb, memcg_stat_strings[i].local_name, mystat.stat[i]);
	}

	
	{
		unsigned long long limit, memsw_limit;
		memcg_get_hierarchical_limit(memcg, &limit, &memsw_limit);
		cb->fill(cb, "hierarchical_memory_limit", limit);
		if (do_swap_account)
			cb->fill(cb, "hierarchical_memsw_limit", memsw_limit);
	}

	memset(&mystat, 0, sizeof(mystat));
	mem_cgroup_get_total_stat(memcg, &mystat);
	for (i = 0; i < NR_MCS_STAT; i++) {
		if (i == MCS_SWAP && !do_swap_account)
			continue;
		cb->fill(cb, memcg_stat_strings[i].total_name, mystat.stat[i]);
	}

#ifdef CONFIG_DEBUG_VM
	{
		int nid, zid;
		struct mem_cgroup_per_zone *mz;
		unsigned long recent_rotated[2] = {0, 0};
		unsigned long recent_scanned[2] = {0, 0};

		for_each_online_node(nid)
			for (zid = 0; zid < MAX_NR_ZONES; zid++) {
				mz = mem_cgroup_zoneinfo(memcg, nid, zid);

				recent_rotated[0] +=
					mz->reclaim_stat.recent_rotated[0];
				recent_rotated[1] +=
					mz->reclaim_stat.recent_rotated[1];
				recent_scanned[0] +=
					mz->reclaim_stat.recent_scanned[0];
				recent_scanned[1] +=
					mz->reclaim_stat.recent_scanned[1];
			}
		cb->fill(cb, "recent_rotated_anon", recent_rotated[0]);
		cb->fill(cb, "recent_rotated_file", recent_rotated[1]);
		cb->fill(cb, "recent_scanned_anon", recent_scanned[0]);
		cb->fill(cb, "recent_scanned_file", recent_scanned[1]);
	}
#endif

	return 0;
}

static u64 mem_cgroup_swappiness_read(struct cgroup *cgrp, struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgrp);

	return mem_cgroup_swappiness(memcg);
}

static int mem_cgroup_swappiness_write(struct cgroup *cgrp, struct cftype *cft,
				       u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgrp);
	struct mem_cgroup *parent;

	if (val > 100)
		return -EINVAL;

	if (cgrp->parent == NULL)
		return -EINVAL;

	parent = mem_cgroup_from_cont(cgrp->parent);

	cgroup_lock();

	
	if ((parent->use_hierarchy) ||
	    (memcg->use_hierarchy && !list_empty(&cgrp->children))) {
		cgroup_unlock();
		return -EINVAL;
	}

	memcg->swappiness = val;

	cgroup_unlock();

	return 0;
}

static void __mem_cgroup_threshold(struct mem_cgroup *memcg, bool swap)
{
	struct mem_cgroup_threshold_ary *t;
	u64 usage;
	int i;

	rcu_read_lock();
	if (!swap)
		t = rcu_dereference(memcg->thresholds.primary);
	else
		t = rcu_dereference(memcg->memsw_thresholds.primary);

	if (!t)
		goto unlock;

	usage = mem_cgroup_usage(memcg, swap);

	i = t->current_threshold;

	for (; i >= 0 && unlikely(t->entries[i].threshold > usage); i--)
		eventfd_signal(t->entries[i].eventfd, 1);

	
	i++;

	for (; i < t->size && unlikely(t->entries[i].threshold <= usage); i++)
		eventfd_signal(t->entries[i].eventfd, 1);

	
	t->current_threshold = i - 1;
unlock:
	rcu_read_unlock();
}

static void mem_cgroup_threshold(struct mem_cgroup *memcg)
{
	while (memcg) {
		__mem_cgroup_threshold(memcg, false);
		if (do_swap_account)
			__mem_cgroup_threshold(memcg, true);

		memcg = parent_mem_cgroup(memcg);
	}
}

static int compare_thresholds(const void *a, const void *b)
{
	const struct mem_cgroup_threshold *_a = a;
	const struct mem_cgroup_threshold *_b = b;

	return _a->threshold - _b->threshold;
}

static int mem_cgroup_oom_notify_cb(struct mem_cgroup *memcg)
{
	struct mem_cgroup_eventfd_list *ev;

	list_for_each_entry(ev, &memcg->oom_notify, list)
		eventfd_signal(ev->eventfd, 1);
	return 0;
}

static void mem_cgroup_oom_notify(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter;

	for_each_mem_cgroup_tree(iter, memcg)
		mem_cgroup_oom_notify_cb(iter);
}

static int mem_cgroup_usage_register_event(struct cgroup *cgrp,
	struct cftype *cft, struct eventfd_ctx *eventfd, const char *args)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgrp);
	struct mem_cgroup_thresholds *thresholds;
	struct mem_cgroup_threshold_ary *new;
	int type = MEMFILE_TYPE(cft->private);
	u64 threshold, usage;
	int i, size, ret;

	ret = res_counter_memparse_write_strategy(args, &threshold);
	if (ret)
		return ret;

	mutex_lock(&memcg->thresholds_lock);

	if (type == _MEM)
		thresholds = &memcg->thresholds;
	else if (type == _MEMSWAP)
		thresholds = &memcg->memsw_thresholds;
	else
		BUG();

	usage = mem_cgroup_usage(memcg, type == _MEMSWAP);

	
	if (thresholds->primary)
		__mem_cgroup_threshold(memcg, type == _MEMSWAP);

	size = thresholds->primary ? thresholds->primary->size + 1 : 1;

	
	new = kmalloc(sizeof(*new) + size * sizeof(struct mem_cgroup_threshold),
			GFP_KERNEL);
	if (!new) {
		ret = -ENOMEM;
		goto unlock;
	}
	new->size = size;

	
	if (thresholds->primary) {
		memcpy(new->entries, thresholds->primary->entries, (size - 1) *
				sizeof(struct mem_cgroup_threshold));
	}

	
	new->entries[size - 1].eventfd = eventfd;
	new->entries[size - 1].threshold = threshold;

	
	sort(new->entries, size, sizeof(struct mem_cgroup_threshold),
			compare_thresholds, NULL);

	
	new->current_threshold = -1;
	for (i = 0; i < size; i++) {
		if (new->entries[i].threshold < usage) {
			++new->current_threshold;
		}
	}

	
	kfree(thresholds->spare);
	thresholds->spare = thresholds->primary;

	rcu_assign_pointer(thresholds->primary, new);

	
	synchronize_rcu();

unlock:
	mutex_unlock(&memcg->thresholds_lock);

	return ret;
}

static void mem_cgroup_usage_unregister_event(struct cgroup *cgrp,
	struct cftype *cft, struct eventfd_ctx *eventfd)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgrp);
	struct mem_cgroup_thresholds *thresholds;
	struct mem_cgroup_threshold_ary *new;
	int type = MEMFILE_TYPE(cft->private);
	u64 usage;
	int i, j, size;

	mutex_lock(&memcg->thresholds_lock);
	if (type == _MEM)
		thresholds = &memcg->thresholds;
	else if (type == _MEMSWAP)
		thresholds = &memcg->memsw_thresholds;
	else
		BUG();

	if (!thresholds->primary)
		goto unlock;

	usage = mem_cgroup_usage(memcg, type == _MEMSWAP);

	
	__mem_cgroup_threshold(memcg, type == _MEMSWAP);

	
	size = 0;
	for (i = 0; i < thresholds->primary->size; i++) {
		if (thresholds->primary->entries[i].eventfd != eventfd)
			size++;
	}

	new = thresholds->spare;

	
	if (!size) {
		kfree(new);
		new = NULL;
		goto swap_buffers;
	}

	new->size = size;

	
	new->current_threshold = -1;
	for (i = 0, j = 0; i < thresholds->primary->size; i++) {
		if (thresholds->primary->entries[i].eventfd == eventfd)
			continue;

		new->entries[j] = thresholds->primary->entries[i];
		if (new->entries[j].threshold < usage) {
			++new->current_threshold;
		}
		j++;
	}

swap_buffers:
	
	thresholds->spare = thresholds->primary;
	
	if (!new) {
		kfree(thresholds->spare);
		thresholds->spare = NULL;
	}

	rcu_assign_pointer(thresholds->primary, new);

	
	synchronize_rcu();
unlock:
	mutex_unlock(&memcg->thresholds_lock);
}

static int mem_cgroup_oom_register_event(struct cgroup *cgrp,
	struct cftype *cft, struct eventfd_ctx *eventfd, const char *args)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgrp);
	struct mem_cgroup_eventfd_list *event;
	int type = MEMFILE_TYPE(cft->private);

	BUG_ON(type != _OOM_TYPE);
	event = kmalloc(sizeof(*event),	GFP_KERNEL);
	if (!event)
		return -ENOMEM;

	spin_lock(&memcg_oom_lock);

	event->eventfd = eventfd;
	list_add(&event->list, &memcg->oom_notify);

	
	if (atomic_read(&memcg->under_oom))
		eventfd_signal(eventfd, 1);
	spin_unlock(&memcg_oom_lock);

	return 0;
}

static void mem_cgroup_oom_unregister_event(struct cgroup *cgrp,
	struct cftype *cft, struct eventfd_ctx *eventfd)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgrp);
	struct mem_cgroup_eventfd_list *ev, *tmp;
	int type = MEMFILE_TYPE(cft->private);

	BUG_ON(type != _OOM_TYPE);

	spin_lock(&memcg_oom_lock);

	list_for_each_entry_safe(ev, tmp, &memcg->oom_notify, list) {
		if (ev->eventfd == eventfd) {
			list_del(&ev->list);
			kfree(ev);
		}
	}

	spin_unlock(&memcg_oom_lock);
}

static int mem_cgroup_oom_control_read(struct cgroup *cgrp,
	struct cftype *cft,  struct cgroup_map_cb *cb)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgrp);

	cb->fill(cb, "oom_kill_disable", memcg->oom_kill_disable);

	if (atomic_read(&memcg->under_oom))
		cb->fill(cb, "under_oom", 1);
	else
		cb->fill(cb, "under_oom", 0);
	return 0;
}

static int mem_cgroup_oom_control_write(struct cgroup *cgrp,
	struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgrp);
	struct mem_cgroup *parent;

	
	if (!cgrp->parent || !((val == 0) || (val == 1)))
		return -EINVAL;

	parent = mem_cgroup_from_cont(cgrp->parent);

	cgroup_lock();
	
	if ((parent->use_hierarchy) ||
	    (memcg->use_hierarchy && !list_empty(&cgrp->children))) {
		cgroup_unlock();
		return -EINVAL;
	}
	memcg->oom_kill_disable = val;
	if (!val)
		memcg_oom_recover(memcg);
	cgroup_unlock();
	return 0;
}

#ifdef CONFIG_NUMA
static const struct file_operations mem_control_numa_stat_file_operations = {
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int mem_control_numa_stat_open(struct inode *unused, struct file *file)
{
	struct cgroup *cont = file->f_dentry->d_parent->d_fsdata;

	file->f_op = &mem_control_numa_stat_file_operations;
	return single_open(file, mem_control_numa_stat_show, cont);
}
#endif 

#ifdef CONFIG_CGROUP_MEM_RES_CTLR_KMEM
static int register_kmem_files(struct cgroup *cont, struct cgroup_subsys *ss)
{
	return mem_cgroup_sockets_init(cont, ss);
};

static void kmem_cgroup_destroy(struct cgroup *cont)
{
	mem_cgroup_sockets_destroy(cont);
}
#else
static int register_kmem_files(struct cgroup *cont, struct cgroup_subsys *ss)
{
	return 0;
}

static void kmem_cgroup_destroy(struct cgroup *cont)
{
}
#endif

static struct cftype mem_cgroup_files[] = {
	{
		.name = "usage_in_bytes",
		.private = MEMFILE_PRIVATE(_MEM, RES_USAGE),
		.read_u64 = mem_cgroup_read,
		.register_event = mem_cgroup_usage_register_event,
		.unregister_event = mem_cgroup_usage_unregister_event,
	},
	{
		.name = "max_usage_in_bytes",
		.private = MEMFILE_PRIVATE(_MEM, RES_MAX_USAGE),
		.trigger = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read,
	},
	{
		.name = "limit_in_bytes",
		.private = MEMFILE_PRIVATE(_MEM, RES_LIMIT),
		.write_string = mem_cgroup_write,
		.read_u64 = mem_cgroup_read,
	},
	{
		.name = "soft_limit_in_bytes",
		.private = MEMFILE_PRIVATE(_MEM, RES_SOFT_LIMIT),
		.write_string = mem_cgroup_write,
		.read_u64 = mem_cgroup_read,
	},
	{
		.name = "failcnt",
		.private = MEMFILE_PRIVATE(_MEM, RES_FAILCNT),
		.trigger = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read,
	},
	{
		.name = "stat",
		.read_map = mem_control_stat_show,
	},
	{
		.name = "force_empty",
		.trigger = mem_cgroup_force_empty_write,
	},
	{
		.name = "use_hierarchy",
		.write_u64 = mem_cgroup_hierarchy_write,
		.read_u64 = mem_cgroup_hierarchy_read,
	},
	{
		.name = "swappiness",
		.read_u64 = mem_cgroup_swappiness_read,
		.write_u64 = mem_cgroup_swappiness_write,
	},
	{
		.name = "move_charge_at_immigrate",
		.read_u64 = mem_cgroup_move_charge_read,
		.write_u64 = mem_cgroup_move_charge_write,
	},
	{
		.name = "oom_control",
		.read_map = mem_cgroup_oom_control_read,
		.write_u64 = mem_cgroup_oom_control_write,
		.register_event = mem_cgroup_oom_register_event,
		.unregister_event = mem_cgroup_oom_unregister_event,
		.private = MEMFILE_PRIVATE(_OOM_TYPE, OOM_CONTROL),
	},
#ifdef CONFIG_NUMA
	{
		.name = "numa_stat",
		.open = mem_control_numa_stat_open,
		.mode = S_IRUGO,
	},
#endif
};

#ifdef CONFIG_CGROUP_MEM_RES_CTLR_SWAP
static struct cftype memsw_cgroup_files[] = {
	{
		.name = "memsw.usage_in_bytes",
		.private = MEMFILE_PRIVATE(_MEMSWAP, RES_USAGE),
		.read_u64 = mem_cgroup_read,
		.register_event = mem_cgroup_usage_register_event,
		.unregister_event = mem_cgroup_usage_unregister_event,
	},
	{
		.name = "memsw.max_usage_in_bytes",
		.private = MEMFILE_PRIVATE(_MEMSWAP, RES_MAX_USAGE),
		.trigger = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read,
	},
	{
		.name = "memsw.limit_in_bytes",
		.private = MEMFILE_PRIVATE(_MEMSWAP, RES_LIMIT),
		.write_string = mem_cgroup_write,
		.read_u64 = mem_cgroup_read,
	},
	{
		.name = "memsw.failcnt",
		.private = MEMFILE_PRIVATE(_MEMSWAP, RES_FAILCNT),
		.trigger = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read,
	},
};

static int register_memsw_files(struct cgroup *cont, struct cgroup_subsys *ss)
{
	if (!do_swap_account)
		return 0;
	return cgroup_add_files(cont, ss, memsw_cgroup_files,
				ARRAY_SIZE(memsw_cgroup_files));
};
#else
static int register_memsw_files(struct cgroup *cont, struct cgroup_subsys *ss)
{
	return 0;
}
#endif

static int alloc_mem_cgroup_per_zone_info(struct mem_cgroup *memcg, int node)
{
	struct mem_cgroup_per_node *pn;
	struct mem_cgroup_per_zone *mz;
	enum lru_list lru;
	int zone, tmp = node;
	if (!node_state(node, N_NORMAL_MEMORY))
		tmp = -1;
	pn = kzalloc_node(sizeof(*pn), GFP_KERNEL, tmp);
	if (!pn)
		return 1;

	for (zone = 0; zone < MAX_NR_ZONES; zone++) {
		mz = &pn->zoneinfo[zone];
		for_each_lru(lru)
			INIT_LIST_HEAD(&mz->lruvec.lists[lru]);
		mz->usage_in_excess = 0;
		mz->on_tree = false;
		mz->memcg = memcg;
	}
	memcg->info.nodeinfo[node] = pn;
	return 0;
}

static void free_mem_cgroup_per_zone_info(struct mem_cgroup *memcg, int node)
{
	kfree(memcg->info.nodeinfo[node]);
}

static struct mem_cgroup *mem_cgroup_alloc(void)
{
	struct mem_cgroup *memcg;
	int size = sizeof(struct mem_cgroup);

	
	if (size < PAGE_SIZE)
		memcg = kzalloc(size, GFP_KERNEL);
	else
		memcg = vzalloc(size);

	if (!memcg)
		return NULL;

	memcg->stat = alloc_percpu(struct mem_cgroup_stat_cpu);
	if (!memcg->stat)
		goto out_free;
	spin_lock_init(&memcg->pcp_counter_lock);
	return memcg;

out_free:
	if (size < PAGE_SIZE)
		kfree(memcg);
	else
		vfree(memcg);
	return NULL;
}

static void vfree_work(struct work_struct *work)
{
	struct mem_cgroup *memcg;

	memcg = container_of(work, struct mem_cgroup, work_freeing);
	vfree(memcg);
}
static void vfree_rcu(struct rcu_head *rcu_head)
{
	struct mem_cgroup *memcg;

	memcg = container_of(rcu_head, struct mem_cgroup, rcu_freeing);
	INIT_WORK(&memcg->work_freeing, vfree_work);
	schedule_work(&memcg->work_freeing);
}


static void __mem_cgroup_free(struct mem_cgroup *memcg)
{
	int node;

	mem_cgroup_remove_from_trees(memcg);
	free_css_id(&mem_cgroup_subsys, &memcg->css);

	for_each_node(node)
		free_mem_cgroup_per_zone_info(memcg, node);

	free_percpu(memcg->stat);
	if (sizeof(struct mem_cgroup) < PAGE_SIZE)
		kfree_rcu(memcg, rcu_freeing);
	else
		call_rcu(&memcg->rcu_freeing, vfree_rcu);
}

static void mem_cgroup_get(struct mem_cgroup *memcg)
{
	atomic_inc(&memcg->refcnt);
}

static void __mem_cgroup_put(struct mem_cgroup *memcg, int count)
{
	if (atomic_sub_and_test(count, &memcg->refcnt)) {
		struct mem_cgroup *parent = parent_mem_cgroup(memcg);
		__mem_cgroup_free(memcg);
		if (parent)
			mem_cgroup_put(parent);
	}
}

static void mem_cgroup_put(struct mem_cgroup *memcg)
{
	__mem_cgroup_put(memcg, 1);
}

struct mem_cgroup *parent_mem_cgroup(struct mem_cgroup *memcg)
{
	if (!memcg->res.parent)
		return NULL;
	return mem_cgroup_from_res_counter(memcg->res.parent, res);
}
EXPORT_SYMBOL(parent_mem_cgroup);

#ifdef CONFIG_CGROUP_MEM_RES_CTLR_SWAP
static void __init enable_swap_cgroup(void)
{
	if (!mem_cgroup_disabled() && really_do_swap_account)
		do_swap_account = 1;
}
#else
static void __init enable_swap_cgroup(void)
{
}
#endif

static int mem_cgroup_soft_limit_tree_init(void)
{
	struct mem_cgroup_tree_per_node *rtpn;
	struct mem_cgroup_tree_per_zone *rtpz;
	int tmp, node, zone;

	for_each_node(node) {
		tmp = node;
		if (!node_state(node, N_NORMAL_MEMORY))
			tmp = -1;
		rtpn = kzalloc_node(sizeof(*rtpn), GFP_KERNEL, tmp);
		if (!rtpn)
			goto err_cleanup;

		soft_limit_tree.rb_tree_per_node[node] = rtpn;

		for (zone = 0; zone < MAX_NR_ZONES; zone++) {
			rtpz = &rtpn->rb_tree_per_zone[zone];
			rtpz->rb_root = RB_ROOT;
			spin_lock_init(&rtpz->lock);
		}
	}
	return 0;

err_cleanup:
	for_each_node(node) {
		if (!soft_limit_tree.rb_tree_per_node[node])
			break;
		kfree(soft_limit_tree.rb_tree_per_node[node]);
		soft_limit_tree.rb_tree_per_node[node] = NULL;
	}
	return 1;

}

static struct cgroup_subsys_state * __ref
mem_cgroup_create(struct cgroup *cont)
{
	struct mem_cgroup *memcg, *parent;
	long error = -ENOMEM;
	int node;

	memcg = mem_cgroup_alloc();
	if (!memcg)
		return ERR_PTR(error);

	for_each_node(node)
		if (alloc_mem_cgroup_per_zone_info(memcg, node))
			goto free_out;

	
	if (cont->parent == NULL) {
		int cpu;
		enable_swap_cgroup();
		parent = NULL;
		if (mem_cgroup_soft_limit_tree_init())
			goto free_out;
		root_mem_cgroup = memcg;
		for_each_possible_cpu(cpu) {
			struct memcg_stock_pcp *stock =
						&per_cpu(memcg_stock, cpu);
			INIT_WORK(&stock->work, drain_local_stock);
		}
		hotcpu_notifier(memcg_cpu_hotplug_callback, 0);
	} else {
		parent = mem_cgroup_from_cont(cont->parent);
		memcg->use_hierarchy = parent->use_hierarchy;
		memcg->oom_kill_disable = parent->oom_kill_disable;
	}

	if (parent && parent->use_hierarchy) {
		res_counter_init(&memcg->res, &parent->res);
		res_counter_init(&memcg->memsw, &parent->memsw);
		mem_cgroup_get(parent);
	} else {
		res_counter_init(&memcg->res, NULL);
		res_counter_init(&memcg->memsw, NULL);
	}
	memcg->last_scanned_node = MAX_NUMNODES;
	INIT_LIST_HEAD(&memcg->oom_notify);

	if (parent)
		memcg->swappiness = mem_cgroup_swappiness(parent);
	atomic_set(&memcg->refcnt, 1);
	memcg->move_charge_at_immigrate = 0;
	mutex_init(&memcg->thresholds_lock);
	spin_lock_init(&memcg->move_lock);
	return &memcg->css;
free_out:
	__mem_cgroup_free(memcg);
	return ERR_PTR(error);
}

static int mem_cgroup_pre_destroy(struct cgroup *cont)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cont);

	return mem_cgroup_force_empty(memcg, false);
}

static void mem_cgroup_destroy(struct cgroup *cont)
{
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cont);

	kmem_cgroup_destroy(cont);

	mem_cgroup_put(memcg);
}

static int mem_cgroup_populate(struct cgroup_subsys *ss,
				struct cgroup *cont)
{
	int ret;

	ret = cgroup_add_files(cont, ss, mem_cgroup_files,
				ARRAY_SIZE(mem_cgroup_files));

	if (!ret)
		ret = register_memsw_files(cont, ss);

	if (!ret)
		ret = register_kmem_files(cont, ss);

	return ret;
}

#ifdef CONFIG_MMU
#define PRECHARGE_COUNT_AT_ONCE	256
static int mem_cgroup_do_precharge(unsigned long count)
{
	int ret = 0;
	int batch_count = PRECHARGE_COUNT_AT_ONCE;
	struct mem_cgroup *memcg = mc.to;

	if (mem_cgroup_is_root(memcg)) {
		mc.precharge += count;
		
		return ret;
	}
	
	if (count > 1) {
		struct res_counter *dummy;
		if (res_counter_charge(&memcg->res, PAGE_SIZE * count, &dummy))
			goto one_by_one;
		if (do_swap_account && res_counter_charge(&memcg->memsw,
						PAGE_SIZE * count, &dummy)) {
			res_counter_uncharge(&memcg->res, PAGE_SIZE * count);
			goto one_by_one;
		}
		mc.precharge += count;
		return ret;
	}
one_by_one:
	
	while (count--) {
		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}
		if (!batch_count--) {
			batch_count = PRECHARGE_COUNT_AT_ONCE;
			cond_resched();
		}
		ret = __mem_cgroup_try_charge(NULL,
					GFP_KERNEL, 1, &memcg, false);
		if (ret)
			
			return ret;
		mc.precharge++;
	}
	return ret;
}

union mc_target {
	struct page	*page;
	swp_entry_t	ent;
};

enum mc_target_type {
	MC_TARGET_NONE = 0,
	MC_TARGET_PAGE,
	MC_TARGET_SWAP,
};

static struct page *mc_handle_present_pte(struct vm_area_struct *vma,
						unsigned long addr, pte_t ptent)
{
	struct page *page = vm_normal_page(vma, addr, ptent);

	if (!page || !page_mapped(page))
		return NULL;
	if (PageAnon(page)) {
		
		if (!move_anon())
			return NULL;
	} else if (!move_file())
		
		return NULL;
	if (!get_page_unless_zero(page))
		return NULL;

	return page;
}

#ifdef CONFIG_SWAP
static struct page *mc_handle_swap_pte(struct vm_area_struct *vma,
			unsigned long addr, pte_t ptent, swp_entry_t *entry)
{
	struct page *page = NULL;
	swp_entry_t ent = pte_to_swp_entry(ptent);

	if (!move_anon() || non_swap_entry(ent))
		return NULL;
	page = find_get_page(&swapper_space, ent.val);
	if (do_swap_account)
		entry->val = ent.val;

	return page;
}
#else
static struct page *mc_handle_swap_pte(struct vm_area_struct *vma,
			unsigned long addr, pte_t ptent, swp_entry_t *entry)
{
	return NULL;
}
#endif

static struct page *mc_handle_file_pte(struct vm_area_struct *vma,
			unsigned long addr, pte_t ptent, swp_entry_t *entry)
{
	struct page *page = NULL;
	struct inode *inode;
	struct address_space *mapping;
	pgoff_t pgoff;

	if (!vma->vm_file) 
		return NULL;
	if (!move_file())
		return NULL;

	inode = vma->vm_file->f_path.dentry->d_inode;
	mapping = vma->vm_file->f_mapping;
	if (pte_none(ptent))
		pgoff = linear_page_index(vma, addr);
	else 
		pgoff = pte_to_pgoff(ptent);

	
	page = find_get_page(mapping, pgoff);

#ifdef CONFIG_SWAP
	
	if (radix_tree_exceptional_entry(page)) {
		swp_entry_t swap = radix_to_swp_entry(page);
		if (do_swap_account)
			*entry = swap;
		page = find_get_page(&swapper_space, swap.val);
	}
#endif
	return page;
}

static enum mc_target_type get_mctgt_type(struct vm_area_struct *vma,
		unsigned long addr, pte_t ptent, union mc_target *target)
{
	struct page *page = NULL;
	struct page_cgroup *pc;
	enum mc_target_type ret = MC_TARGET_NONE;
	swp_entry_t ent = { .val = 0 };

	if (pte_present(ptent))
		page = mc_handle_present_pte(vma, addr, ptent);
	else if (is_swap_pte(ptent))
		page = mc_handle_swap_pte(vma, addr, ptent, &ent);
	else if (pte_none(ptent) || pte_file(ptent))
		page = mc_handle_file_pte(vma, addr, ptent, &ent);

	if (!page && !ent.val)
		return ret;
	if (page) {
		pc = lookup_page_cgroup(page);
		if (PageCgroupUsed(pc) && pc->mem_cgroup == mc.from) {
			ret = MC_TARGET_PAGE;
			if (target)
				target->page = page;
		}
		if (!ret || !target)
			put_page(page);
	}
	
	if (ent.val && !ret &&
			css_id(&mc.from->css) == lookup_swap_cgroup_id(ent)) {
		ret = MC_TARGET_SWAP;
		if (target)
			target->ent = ent;
	}
	return ret;
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static enum mc_target_type get_mctgt_type_thp(struct vm_area_struct *vma,
		unsigned long addr, pmd_t pmd, union mc_target *target)
{
	struct page *page = NULL;
	struct page_cgroup *pc;
	enum mc_target_type ret = MC_TARGET_NONE;

	page = pmd_page(pmd);
	VM_BUG_ON(!page || !PageHead(page));
	if (!move_anon())
		return ret;
	pc = lookup_page_cgroup(page);
	if (PageCgroupUsed(pc) && pc->mem_cgroup == mc.from) {
		ret = MC_TARGET_PAGE;
		if (target) {
			get_page(page);
			target->page = page;
		}
	}
	return ret;
}
#else
static inline enum mc_target_type get_mctgt_type_thp(struct vm_area_struct *vma,
		unsigned long addr, pmd_t pmd, union mc_target *target)
{
	return MC_TARGET_NONE;
}
#endif

static int mem_cgroup_count_precharge_pte_range(pmd_t *pmd,
					unsigned long addr, unsigned long end,
					struct mm_walk *walk)
{
	struct vm_area_struct *vma = walk->private;
	pte_t *pte;
	spinlock_t *ptl;

	if (pmd_trans_huge_lock(pmd, vma) == 1) {
		if (get_mctgt_type_thp(vma, addr, *pmd, NULL) == MC_TARGET_PAGE)
			mc.precharge += HPAGE_PMD_NR;
		spin_unlock(&vma->vm_mm->page_table_lock);
		return 0;
	}

	if (pmd_trans_unstable(pmd))
		return 0;
	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	for (; addr != end; pte++, addr += PAGE_SIZE)
		if (get_mctgt_type(vma, addr, *pte, NULL))
			mc.precharge++;	
	pte_unmap_unlock(pte - 1, ptl);
	cond_resched();

	return 0;
}

static unsigned long mem_cgroup_count_precharge(struct mm_struct *mm)
{
	unsigned long precharge;
	struct vm_area_struct *vma;

	down_read(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		struct mm_walk mem_cgroup_count_precharge_walk = {
			.pmd_entry = mem_cgroup_count_precharge_pte_range,
			.mm = mm,
			.private = vma,
		};
		if (is_vm_hugetlb_page(vma))
			continue;
		walk_page_range(vma->vm_start, vma->vm_end,
					&mem_cgroup_count_precharge_walk);
	}
	up_read(&mm->mmap_sem);

	precharge = mc.precharge;
	mc.precharge = 0;

	return precharge;
}

static int mem_cgroup_precharge_mc(struct mm_struct *mm)
{
	unsigned long precharge = mem_cgroup_count_precharge(mm);

	VM_BUG_ON(mc.moving_task);
	mc.moving_task = current;
	return mem_cgroup_do_precharge(precharge);
}

static void __mem_cgroup_clear_mc(void)
{
	struct mem_cgroup *from = mc.from;
	struct mem_cgroup *to = mc.to;

	
	if (mc.precharge) {
		__mem_cgroup_cancel_charge(mc.to, mc.precharge);
		mc.precharge = 0;
	}
	if (mc.moved_charge) {
		__mem_cgroup_cancel_charge(mc.from, mc.moved_charge);
		mc.moved_charge = 0;
	}
	
	if (mc.moved_swap) {
		
		if (!mem_cgroup_is_root(mc.from))
			res_counter_uncharge(&mc.from->memsw,
						PAGE_SIZE * mc.moved_swap);
		__mem_cgroup_put(mc.from, mc.moved_swap);

		if (!mem_cgroup_is_root(mc.to)) {
			res_counter_uncharge(&mc.to->res,
						PAGE_SIZE * mc.moved_swap);
		}
		
		mc.moved_swap = 0;
	}
	memcg_oom_recover(from);
	memcg_oom_recover(to);
	wake_up_all(&mc.waitq);
}

static void mem_cgroup_clear_mc(void)
{
	struct mem_cgroup *from = mc.from;

	mc.moving_task = NULL;
	__mem_cgroup_clear_mc();
	spin_lock(&mc.lock);
	mc.from = NULL;
	mc.to = NULL;
	spin_unlock(&mc.lock);
	mem_cgroup_end_move(from);
}

static int mem_cgroup_can_attach(struct cgroup *cgroup,
				 struct cgroup_taskset *tset)
{
	struct task_struct *p = cgroup_taskset_first(tset);
	int ret = 0;
	struct mem_cgroup *memcg = mem_cgroup_from_cont(cgroup);

	if (memcg->move_charge_at_immigrate) {
		struct mm_struct *mm;
		struct mem_cgroup *from = mem_cgroup_from_task(p);

		VM_BUG_ON(from == memcg);

		mm = get_task_mm(p);
		if (!mm)
			return 0;
		
		if (mm->owner == p) {
			VM_BUG_ON(mc.from);
			VM_BUG_ON(mc.to);
			VM_BUG_ON(mc.precharge);
			VM_BUG_ON(mc.moved_charge);
			VM_BUG_ON(mc.moved_swap);
			mem_cgroup_start_move(from);
			spin_lock(&mc.lock);
			mc.from = from;
			mc.to = memcg;
			spin_unlock(&mc.lock);
			

			ret = mem_cgroup_precharge_mc(mm);
			if (ret)
				mem_cgroup_clear_mc();
		}
		mmput(mm);
	}
	return ret;
}

static void mem_cgroup_cancel_attach(struct cgroup *cgroup,
				     struct cgroup_taskset *tset)
{
	mem_cgroup_clear_mc();
}

static int mem_cgroup_move_charge_pte_range(pmd_t *pmd,
				unsigned long addr, unsigned long end,
				struct mm_walk *walk)
{
	int ret = 0;
	struct vm_area_struct *vma = walk->private;
	pte_t *pte;
	spinlock_t *ptl;
	enum mc_target_type target_type;
	union mc_target target;
	struct page *page;
	struct page_cgroup *pc;

	if (pmd_trans_huge_lock(pmd, vma) == 1) {
		if (mc.precharge < HPAGE_PMD_NR) {
			spin_unlock(&vma->vm_mm->page_table_lock);
			return 0;
		}
		target_type = get_mctgt_type_thp(vma, addr, *pmd, &target);
		if (target_type == MC_TARGET_PAGE) {
			page = target.page;
			if (!isolate_lru_page(page)) {
				pc = lookup_page_cgroup(page);
				if (!mem_cgroup_move_account(page, HPAGE_PMD_NR,
							     pc, mc.from, mc.to,
							     false)) {
					mc.precharge -= HPAGE_PMD_NR;
					mc.moved_charge += HPAGE_PMD_NR;
				}
				putback_lru_page(page);
			}
			put_page(page);
		}
		spin_unlock(&vma->vm_mm->page_table_lock);
		return 0;
	}

	if (pmd_trans_unstable(pmd))
		return 0;
retry:
	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	for (; addr != end; addr += PAGE_SIZE) {
		pte_t ptent = *(pte++);
		swp_entry_t ent;

		if (!mc.precharge)
			break;

		switch (get_mctgt_type(vma, addr, ptent, &target)) {
		case MC_TARGET_PAGE:
			page = target.page;
			if (isolate_lru_page(page))
				goto put;
			pc = lookup_page_cgroup(page);
			if (!mem_cgroup_move_account(page, 1, pc,
						     mc.from, mc.to, false)) {
				mc.precharge--;
				
				mc.moved_charge++;
			}
			putback_lru_page(page);
put:			
			put_page(page);
			break;
		case MC_TARGET_SWAP:
			ent = target.ent;
			if (!mem_cgroup_move_swap_account(ent,
						mc.from, mc.to, false)) {
				mc.precharge--;
				
				mc.moved_swap++;
			}
			break;
		default:
			break;
		}
	}
	pte_unmap_unlock(pte - 1, ptl);
	cond_resched();

	if (addr != end) {
		ret = mem_cgroup_do_precharge(1);
		if (!ret)
			goto retry;
	}

	return ret;
}

static void mem_cgroup_move_charge(struct mm_struct *mm)
{
	struct vm_area_struct *vma;

	lru_add_drain_all();
retry:
	if (unlikely(!down_read_trylock(&mm->mmap_sem))) {
		__mem_cgroup_clear_mc();
		cond_resched();
		goto retry;
	}
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		int ret;
		struct mm_walk mem_cgroup_move_charge_walk = {
			.pmd_entry = mem_cgroup_move_charge_pte_range,
			.mm = mm,
			.private = vma,
		};
		if (is_vm_hugetlb_page(vma))
			continue;
		ret = walk_page_range(vma->vm_start, vma->vm_end,
						&mem_cgroup_move_charge_walk);
		if (ret)
			break;
	}
	up_read(&mm->mmap_sem);
}

static void mem_cgroup_move_task(struct cgroup *cont,
				 struct cgroup_taskset *tset)
{
	struct task_struct *p = cgroup_taskset_first(tset);
	struct mm_struct *mm = get_task_mm(p);

	if (mm) {
		if (mc.to)
			mem_cgroup_move_charge(mm);
		mmput(mm);
	}
	if (mc.to)
		mem_cgroup_clear_mc();
}
#else	
static int mem_cgroup_can_attach(struct cgroup *cgroup,
				 struct cgroup_taskset *tset)
{
	return 0;
}
static void mem_cgroup_cancel_attach(struct cgroup *cgroup,
				     struct cgroup_taskset *tset)
{
}
static void mem_cgroup_move_task(struct cgroup *cont,
				 struct cgroup_taskset *tset)
{
}
#endif

struct cgroup_subsys mem_cgroup_subsys = {
	.name = "memory",
	.subsys_id = mem_cgroup_subsys_id,
	.create = mem_cgroup_create,
	.pre_destroy = mem_cgroup_pre_destroy,
	.destroy = mem_cgroup_destroy,
	.populate = mem_cgroup_populate,
	.can_attach = mem_cgroup_can_attach,
	.cancel_attach = mem_cgroup_cancel_attach,
	.attach = mem_cgroup_move_task,
	.early_init = 0,
	.use_id = 1,
};

#ifdef CONFIG_CGROUP_MEM_RES_CTLR_SWAP
static int __init enable_swap_account(char *s)
{
	
	if (!strcmp(s, "1"))
		really_do_swap_account = 1;
	else if (!strcmp(s, "0"))
		really_do_swap_account = 0;
	return 1;
}
__setup("swapaccount=", enable_swap_account);

#endif
