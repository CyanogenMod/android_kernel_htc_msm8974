/*
 * A Remote Heap.  Remote means that we don't touch the memory that the
 * heap points to. Normal heap implementations use the memory they manage
 * to place their list. We cannot do that because the memory we manage may
 * have special properties, for example it is uncachable or of different
 * endianess.
 *
 * Author: Pantelis Antoniou <panto@intracom.gr>
 *
 * 2004 (c) INTRACOM S.A. Greece. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <asm/rheap.h>

static inline void fixup(unsigned long s, unsigned long e, int d,
			 struct list_head *l)
{
	unsigned long *pp;

	pp = (unsigned long *)&l->next;
	if (*pp >= s && *pp < e)
		*pp += d;

	pp = (unsigned long *)&l->prev;
	if (*pp >= s && *pp < e)
		*pp += d;
}

static int grow(rh_info_t * info, int max_blocks)
{
	rh_block_t *block, *blk;
	int i, new_blocks;
	int delta;
	unsigned long blks, blke;

	if (max_blocks <= info->max_blocks)
		return -EINVAL;

	new_blocks = max_blocks - info->max_blocks;

	block = kmalloc(sizeof(rh_block_t) * max_blocks, GFP_ATOMIC);
	if (block == NULL)
		return -ENOMEM;

	if (info->max_blocks > 0) {

		
		memcpy(block, info->block,
		       sizeof(rh_block_t) * info->max_blocks);

		delta = (char *)block - (char *)info->block;

		
		blks = (unsigned long)info->block;
		blke = (unsigned long)(info->block + info->max_blocks);

		for (i = 0, blk = block; i < info->max_blocks; i++, blk++)
			fixup(blks, blke, delta, &blk->list);

		fixup(blks, blke, delta, &info->empty_list);
		fixup(blks, blke, delta, &info->free_list);
		fixup(blks, blke, delta, &info->taken_list);

		
		if ((info->flags & RHIF_STATIC_BLOCK) == 0)
			kfree(info->block);
	}

	info->block = block;
	info->empty_slots += new_blocks;
	info->max_blocks = max_blocks;
	info->flags &= ~RHIF_STATIC_BLOCK;

	
	blk = block + info->max_blocks - new_blocks;
	for (i = 0; i < new_blocks; i++, blk++)
		list_add(&blk->list, &info->empty_list);

	return 0;
}

static int assure_empty(rh_info_t * info, int slots)
{
	int max_blocks;

	
	if (slots >= 4)
		return -EINVAL;

	
	if (info->empty_slots >= slots)
		return 0;

	
	max_blocks = ((info->max_blocks + slots) + 15) & ~15;

	return grow(info, max_blocks);
}

static rh_block_t *get_slot(rh_info_t * info)
{
	rh_block_t *blk;

	
	
	if (info->empty_slots == 0) {
		printk(KERN_ERR "rh: out of slots; crash is imminent.\n");
		return NULL;
	}

	
	blk = list_entry(info->empty_list.next, rh_block_t, list);
	list_del_init(&blk->list);
	info->empty_slots--;

	
	blk->start = 0;
	blk->size = 0;
	blk->owner = NULL;

	return blk;
}

static inline void release_slot(rh_info_t * info, rh_block_t * blk)
{
	list_add(&blk->list, &info->empty_list);
	info->empty_slots++;
}

static void attach_free_block(rh_info_t * info, rh_block_t * blkn)
{
	rh_block_t *blk;
	rh_block_t *before;
	rh_block_t *after;
	rh_block_t *next;
	int size;
	unsigned long s, e, bs, be;
	struct list_head *l;

	
	size = blkn->size;
	s = blkn->start;
	e = s + size;

	before = NULL;
	after = NULL;
	next = NULL;

	list_for_each(l, &info->free_list) {
		blk = list_entry(l, rh_block_t, list);

		bs = blk->start;
		be = bs + blk->size;

		if (next == NULL && s >= bs)
			next = blk;

		if (be == s)
			before = blk;

		if (e == bs)
			after = blk;

		
		if (before != NULL && after != NULL)
			break;
	}

	
	if (before && s != (before->start + before->size))
		before = NULL;

	if (after && e != after->start)
		after = NULL;

	
	if (before == NULL && after == NULL) {

		if (next != NULL)
			list_add(&blkn->list, &next->list);
		else
			list_add(&blkn->list, &info->free_list);

		return;
	}

	
	release_slot(info, blkn);

	
	if (before != NULL && after == NULL) {
		before->size += size;
		return;
	}

	
	if (before == NULL && after != NULL) {
		after->start -= size;
		after->size += size;
		return;
	}

	
	before->size += size + after->size;
	list_del(&after->list);
	release_slot(info, after);
}

static void attach_taken_block(rh_info_t * info, rh_block_t * blkn)
{
	rh_block_t *blk;
	struct list_head *l;

	
	list_for_each(l, &info->taken_list) {
		blk = list_entry(l, rh_block_t, list);
		if (blk->start > blkn->start) {
			list_add_tail(&blkn->list, &blk->list);
			return;
		}
	}

	list_add_tail(&blkn->list, &info->taken_list);
}

rh_info_t *rh_create(unsigned int alignment)
{
	rh_info_t *info;

	
	if ((alignment & (alignment - 1)) != 0)
		return ERR_PTR(-EINVAL);

	info = kmalloc(sizeof(*info), GFP_ATOMIC);
	if (info == NULL)
		return ERR_PTR(-ENOMEM);

	info->alignment = alignment;

	
	info->block = NULL;
	info->max_blocks = 0;
	info->empty_slots = 0;
	info->flags = 0;

	INIT_LIST_HEAD(&info->empty_list);
	INIT_LIST_HEAD(&info->free_list);
	INIT_LIST_HEAD(&info->taken_list);

	return info;
}
EXPORT_SYMBOL_GPL(rh_create);

void rh_destroy(rh_info_t * info)
{
	if ((info->flags & RHIF_STATIC_BLOCK) == 0 && info->block != NULL)
		kfree(info->block);

	if ((info->flags & RHIF_STATIC_INFO) == 0)
		kfree(info);
}
EXPORT_SYMBOL_GPL(rh_destroy);

void rh_init(rh_info_t * info, unsigned int alignment, int max_blocks,
	     rh_block_t * block)
{
	int i;
	rh_block_t *blk;

	
	if ((alignment & (alignment - 1)) != 0)
		return;

	info->alignment = alignment;

	
	info->block = block;
	info->max_blocks = max_blocks;
	info->empty_slots = max_blocks;
	info->flags = RHIF_STATIC_INFO | RHIF_STATIC_BLOCK;

	INIT_LIST_HEAD(&info->empty_list);
	INIT_LIST_HEAD(&info->free_list);
	INIT_LIST_HEAD(&info->taken_list);

	
	for (i = 0, blk = block; i < max_blocks; i++, blk++)
		list_add(&blk->list, &info->empty_list);
}
EXPORT_SYMBOL_GPL(rh_init);

int rh_attach_region(rh_info_t * info, unsigned long start, int size)
{
	rh_block_t *blk;
	unsigned long s, e, m;
	int r;

	
	s = start;
	e = s + size;
	m = info->alignment - 1;

	
	s = (s + m) & ~m;

	
	e = e & ~m;

	if (IS_ERR_VALUE(e) || (e < s))
		return -ERANGE;

	
	start = s;
	size = e - s;

	
	r = assure_empty(info, 1);
	if (r < 0)
		return r;

	blk = get_slot(info);
	blk->start = start;
	blk->size = size;
	blk->owner = NULL;

	attach_free_block(info, blk);

	return 0;
}
EXPORT_SYMBOL_GPL(rh_attach_region);

unsigned long rh_detach_region(rh_info_t * info, unsigned long start, int size)
{
	struct list_head *l;
	rh_block_t *blk, *newblk;
	unsigned long s, e, m, bs, be;

	
	if (size <= 0)
		return (unsigned long) -EINVAL;

	
	s = start;
	e = s + size;
	m = info->alignment - 1;

	
	s = (s + m) & ~m;

	
	e = e & ~m;

	if (assure_empty(info, 1) < 0)
		return (unsigned long) -ENOMEM;

	blk = NULL;
	list_for_each(l, &info->free_list) {
		blk = list_entry(l, rh_block_t, list);
		
		bs = blk->start;
		be = blk->start + blk->size;
		if (s >= bs && e <= be)
			break;
		blk = NULL;
	}

	if (blk == NULL)
		return (unsigned long) -ENOMEM;

	
	if (bs == s && be == e) {
		
		list_del(&blk->list);
		release_slot(info, blk);
		return s;
	}

	
	if (bs == s || be == e) {
		if (bs == s)
			blk->start += size;
		blk->size -= size;

	} else {
		
		blk->size = s - bs;

		
		newblk = get_slot(info);
		newblk->start = e;
		newblk->size = be - e;

		list_add(&newblk->list, &blk->list);
	}

	return s;
}
EXPORT_SYMBOL_GPL(rh_detach_region);

unsigned long rh_alloc_align(rh_info_t * info, int size, int alignment, const char *owner)
{
	struct list_head *l;
	rh_block_t *blk;
	rh_block_t *newblk;
	unsigned long start, sp_size;

	
	if (size <= 0 || (alignment & (alignment - 1)) != 0)
		return (unsigned long) -EINVAL;

	
	size = (size + (info->alignment - 1)) & ~(info->alignment - 1);

	if (assure_empty(info, 2) < 0)
		return (unsigned long) -ENOMEM;

	blk = NULL;
	list_for_each(l, &info->free_list) {
		blk = list_entry(l, rh_block_t, list);
		if (size <= blk->size) {
			start = (blk->start + alignment - 1) & ~(alignment - 1);
			if (start + size <= blk->start + blk->size)
				break;
		}
		blk = NULL;
	}

	if (blk == NULL)
		return (unsigned long) -ENOMEM;

	
	if (blk->size == size) {
		
		list_del(&blk->list);
		newblk = blk;
	} else {
		
		
		sp_size = start - blk->start;
		if (sp_size) {
			rh_block_t *spblk;

			spblk = get_slot(info);
			spblk->start = blk->start;
			spblk->size = sp_size;
			
			list_add(&spblk->list, blk->list.prev);
		}
		newblk = get_slot(info);
		newblk->start = start;
		newblk->size = size;

		blk->start = start + size;
		blk->size -= sp_size + size;
		
		if (blk->size == 0) {
			list_del(&blk->list);
			release_slot(info, blk);
		}
	}

	newblk->owner = owner;
	attach_taken_block(info, newblk);

	return start;
}
EXPORT_SYMBOL_GPL(rh_alloc_align);

unsigned long rh_alloc(rh_info_t * info, int size, const char *owner)
{
	return rh_alloc_align(info, size, info->alignment, owner);
}
EXPORT_SYMBOL_GPL(rh_alloc);

unsigned long rh_alloc_fixed(rh_info_t * info, unsigned long start, int size, const char *owner)
{
	struct list_head *l;
	rh_block_t *blk, *newblk1, *newblk2;
	unsigned long s, e, m, bs = 0, be = 0;

	
	if (size <= 0)
		return (unsigned long) -EINVAL;

	
	s = start;
	e = s + size;
	m = info->alignment - 1;

	
	s = (s + m) & ~m;

	
	e = e & ~m;

	if (assure_empty(info, 2) < 0)
		return (unsigned long) -ENOMEM;

	blk = NULL;
	list_for_each(l, &info->free_list) {
		blk = list_entry(l, rh_block_t, list);
		
		bs = blk->start;
		be = blk->start + blk->size;
		if (s >= bs && e <= be)
			break;
		blk = NULL;
	}

	if (blk == NULL)
		return (unsigned long) -ENOMEM;

	
	if (bs == s && be == e) {
		
		list_del(&blk->list);
		blk->owner = owner;

		start = blk->start;
		attach_taken_block(info, blk);

		return start;

	}

	
	if (bs == s || be == e) {
		if (bs == s)
			blk->start += size;
		blk->size -= size;

	} else {
		
		blk->size = s - bs;

		
		newblk2 = get_slot(info);
		newblk2->start = e;
		newblk2->size = be - e;

		list_add(&newblk2->list, &blk->list);
	}

	newblk1 = get_slot(info);
	newblk1->start = s;
	newblk1->size = e - s;
	newblk1->owner = owner;

	start = newblk1->start;
	attach_taken_block(info, newblk1);

	return start;
}
EXPORT_SYMBOL_GPL(rh_alloc_fixed);

int rh_free(rh_info_t * info, unsigned long start)
{
	rh_block_t *blk, *blk2;
	struct list_head *l;
	int size;

	
	blk = NULL;
	list_for_each(l, &info->taken_list) {
		blk2 = list_entry(l, rh_block_t, list);
		if (start < blk2->start)
			break;
		blk = blk2;
	}

	if (blk == NULL || start > (blk->start + blk->size))
		return -EINVAL;

	
	list_del(&blk->list);

	
	size = blk->size;
	attach_free_block(info, blk);

	return size;
}
EXPORT_SYMBOL_GPL(rh_free);

int rh_get_stats(rh_info_t * info, int what, int max_stats, rh_stats_t * stats)
{
	rh_block_t *blk;
	struct list_head *l;
	struct list_head *h;
	int nr;

	switch (what) {

	case RHGS_FREE:
		h = &info->free_list;
		break;

	case RHGS_TAKEN:
		h = &info->taken_list;
		break;

	default:
		return -EINVAL;
	}

	
	nr = 0;
	list_for_each(l, h) {
		blk = list_entry(l, rh_block_t, list);
		if (stats != NULL && nr < max_stats) {
			stats->start = blk->start;
			stats->size = blk->size;
			stats->owner = blk->owner;
			stats++;
		}
		nr++;
	}

	return nr;
}
EXPORT_SYMBOL_GPL(rh_get_stats);

int rh_set_owner(rh_info_t * info, unsigned long start, const char *owner)
{
	rh_block_t *blk, *blk2;
	struct list_head *l;
	int size;

	
	blk = NULL;
	list_for_each(l, &info->taken_list) {
		blk2 = list_entry(l, rh_block_t, list);
		if (start < blk2->start)
			break;
		blk = blk2;
	}

	if (blk == NULL || start > (blk->start + blk->size))
		return -EINVAL;

	blk->owner = owner;
	size = blk->size;

	return size;
}
EXPORT_SYMBOL_GPL(rh_set_owner);

void rh_dump(rh_info_t * info)
{
	static rh_stats_t st[32];	
	int maxnr;
	int i, nr;

	maxnr = ARRAY_SIZE(st);

	printk(KERN_INFO
	       "info @0x%p (%d slots empty / %d max)\n",
	       info, info->empty_slots, info->max_blocks);

	printk(KERN_INFO "  Free:\n");
	nr = rh_get_stats(info, RHGS_FREE, maxnr, st);
	if (nr > maxnr)
		nr = maxnr;
	for (i = 0; i < nr; i++)
		printk(KERN_INFO
		       "    0x%lx-0x%lx (%u)\n",
		       st[i].start, st[i].start + st[i].size,
		       st[i].size);
	printk(KERN_INFO "\n");

	printk(KERN_INFO "  Taken:\n");
	nr = rh_get_stats(info, RHGS_TAKEN, maxnr, st);
	if (nr > maxnr)
		nr = maxnr;
	for (i = 0; i < nr; i++)
		printk(KERN_INFO
		       "    0x%lx-0x%lx (%u) %s\n",
		       st[i].start, st[i].start + st[i].size,
		       st[i].size, st[i].owner != NULL ? st[i].owner : "");
	printk(KERN_INFO "\n");
}
EXPORT_SYMBOL_GPL(rh_dump);

void rh_dump_blk(rh_info_t * info, rh_block_t * blk)
{
	printk(KERN_INFO
	       "blk @0x%p: 0x%lx-0x%lx (%u)\n",
	       blk, blk->start, blk->start + blk->size, blk->size);
}
EXPORT_SYMBOL_GPL(rh_dump_blk);

