/*
 * Copyright (c) 2006-2007 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __XFS_MRU_CACHE_H__
#define __XFS_MRU_CACHE_H__


typedef void (*xfs_mru_cache_free_func_t)(unsigned long, void*);

typedef struct xfs_mru_cache
{
	struct radix_tree_root	store;     
	struct list_head	*lists;    
	struct list_head	reap_list; 
	spinlock_t		lock;      
	unsigned int		grp_count; 
	unsigned int		grp_time;  
	unsigned int		lru_grp;   
	unsigned long		time_zero; 
	xfs_mru_cache_free_func_t free_func; 
	struct delayed_work	work;      
	unsigned int		queued;	   
} xfs_mru_cache_t;

int xfs_mru_cache_init(void);
void xfs_mru_cache_uninit(void);
int xfs_mru_cache_create(struct xfs_mru_cache **mrup, unsigned int lifetime_ms,
			     unsigned int grp_count,
			     xfs_mru_cache_free_func_t free_func);
void xfs_mru_cache_destroy(struct xfs_mru_cache *mru);
int xfs_mru_cache_insert(struct xfs_mru_cache *mru, unsigned long key,
				void *value);
void * xfs_mru_cache_remove(struct xfs_mru_cache *mru, unsigned long key);
void xfs_mru_cache_delete(struct xfs_mru_cache *mru, unsigned long key);
void *xfs_mru_cache_lookup(struct xfs_mru_cache *mru, unsigned long key);
void xfs_mru_cache_done(struct xfs_mru_cache *mru);

#endif 
