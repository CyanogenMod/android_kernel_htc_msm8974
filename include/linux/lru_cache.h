/*
   lru_cache.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2003-2008, LINBIT Information Technologies GmbH.
   Copyright (C) 2003-2008, Philipp Reisner <philipp.reisner@linbit.com>.
   Copyright (C) 2003-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.

   drbd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   drbd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 */

#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/string.h> 
#include <linux/seq_file.h>


struct lc_element {
	struct hlist_node colision;
	struct list_head list;		 
	unsigned refcnt;
	unsigned lc_index;
	unsigned lc_number;

	
#define LC_FREE (~0U)
};

struct lru_cache {
	
	struct list_head lru;
	struct list_head free;
	struct list_head in_use;

	
	struct kmem_cache *lc_cache;

	
	size_t element_size;
	
	size_t element_off;

	
	unsigned int  nr_elements;
#define LC_MAX_ACTIVE	(1<<24)

	
	unsigned used; 
	unsigned long hits, misses, starving, dirty, changed;

	
	unsigned long flags;

	
	unsigned int  new_number;

	
	struct lc_element *changing_element;

	void  *lc_private;
	const char *name;

	
	struct hlist_head *lc_slot;
	struct lc_element **lc_element;
};


enum {
	__LC_PARANOIA,
	__LC_DIRTY,
	__LC_STARVING,
};
#define LC_PARANOIA (1<<__LC_PARANOIA)
#define LC_DIRTY    (1<<__LC_DIRTY)
#define LC_STARVING (1<<__LC_STARVING)

extern struct lru_cache *lc_create(const char *name, struct kmem_cache *cache,
		unsigned e_count, size_t e_size, size_t e_off);
extern void lc_reset(struct lru_cache *lc);
extern void lc_destroy(struct lru_cache *lc);
extern void lc_set(struct lru_cache *lc, unsigned int enr, int index);
extern void lc_del(struct lru_cache *lc, struct lc_element *element);

extern struct lc_element *lc_try_get(struct lru_cache *lc, unsigned int enr);
extern struct lc_element *lc_find(struct lru_cache *lc, unsigned int enr);
extern struct lc_element *lc_get(struct lru_cache *lc, unsigned int enr);
extern unsigned int lc_put(struct lru_cache *lc, struct lc_element *e);
extern void lc_changed(struct lru_cache *lc, struct lc_element *e);

struct seq_file;
extern size_t lc_seq_printf_stats(struct seq_file *seq, struct lru_cache *lc);

extern void lc_seq_dump_details(struct seq_file *seq, struct lru_cache *lc, char *utext,
				void (*detail) (struct seq_file *, struct lc_element *));

static inline int lc_try_lock(struct lru_cache *lc)
{
	return !test_and_set_bit(__LC_DIRTY, &lc->flags);
}

static inline void lc_unlock(struct lru_cache *lc)
{
	clear_bit(__LC_DIRTY, &lc->flags);
	smp_mb__after_clear_bit();
}

static inline int lc_is_used(struct lru_cache *lc, unsigned int enr)
{
	struct lc_element *e = lc_find(lc, enr);
	return e && e->refcnt;
}

#define lc_entry(ptr, type, member) \
	container_of(ptr, type, member)

extern struct lc_element *lc_element_by_index(struct lru_cache *lc, unsigned i);
extern unsigned int lc_index_of(struct lru_cache *lc, struct lc_element *e);

#endif
