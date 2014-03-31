/*
 * fs/logfs/gc.c	- garbage collection code
 *
 * As should be obvious for Linux kernel code, license is GPLv2
 *
 * Copyright (c) 2005-2008 Joern Engel <joern@logfs.org>
 */
#include "logfs.h"
#include <linux/sched.h>
#include <linux/slab.h>

#define WL_DELTA 397
#define WL_RATELIMIT 100
#define MAX_OBJ_ALIASES	2600
#define SCAN_RATIO 512	
#define LIST_SIZE 64	
#define SCAN_ROUNDS 128	
#define SCAN_ROUNDS_HIGH 4 

static int no_free_segments(struct super_block *sb)
{
	struct logfs_super *super = logfs_super(sb);

	return super->s_free_list.count;
}

static u8 root_distance(struct super_block *sb, gc_level_t __gc_level)
{
	struct logfs_super *super = logfs_super(sb);
	u8 gc_level = (__force u8)__gc_level;

	switch (gc_level) {
	case 0: 
	case 1: 
	case 2: 
	case 3:
		
		return super->s_ifile_levels + super->s_iblock_levels - gc_level;
	case 6: 
	case 7: 
	case 8: 
	case 9:
		
		return super->s_ifile_levels - (gc_level - 6);
	default:
		printk(KERN_ERR"LOGFS: segment of unknown level %x found\n",
				gc_level);
		WARN_ON(1);
		return super->s_ifile_levels + super->s_iblock_levels;
	}
}

static int segment_is_reserved(struct super_block *sb, u32 segno)
{
	struct logfs_super *super = logfs_super(sb);
	struct logfs_area *area;
	void *reserved;
	int i;

	
	reserved = btree_lookup32(&super->s_reserved_segments, segno);
	if (reserved)
		return 1;

	
	for_each_area(i) {
		area = super->s_area[i];
		if (area->a_is_open && area->a_segno == segno)
			return 1;
	}

	return 0;
}

static void logfs_mark_segment_bad(struct super_block *sb, u32 segno)
{
	BUG();
}

static u32 logfs_valid_bytes(struct super_block *sb, u32 segno, u32 *ec,
		gc_level_t *gc_level)
{
	struct logfs_segment_entry se;
	u32 ec_level;

	logfs_get_segment_entry(sb, segno, &se);
	if (se.ec_level == cpu_to_be32(BADSEG) ||
			se.valid == cpu_to_be32(RESERVED))
		return RESERVED;

	ec_level = be32_to_cpu(se.ec_level);
	*ec = ec_level >> 4;
	*gc_level = GC_LEVEL(ec_level & 0xf);
	return be32_to_cpu(se.valid);
}

static void logfs_cleanse_block(struct super_block *sb, u64 ofs, u64 ino,
		u64 bix, gc_level_t gc_level)
{
	struct inode *inode;
	int err, cookie;

	inode = logfs_safe_iget(sb, ino, &cookie);
	err = logfs_rewrite_block(inode, bix, ofs, gc_level, 0);
	BUG_ON(err);
	logfs_safe_iput(inode, cookie);
}

static u32 logfs_gc_segment(struct super_block *sb, u32 segno)
{
	struct logfs_super *super = logfs_super(sb);
	struct logfs_segment_header sh;
	struct logfs_object_header oh;
	u64 ofs, ino, bix;
	u32 seg_ofs, logical_segno, cleaned = 0;
	int err, len, valid;
	gc_level_t gc_level;

	LOGFS_BUG_ON(segment_is_reserved(sb, segno), sb);

	btree_insert32(&super->s_reserved_segments, segno, (void *)1, GFP_NOFS);
	err = wbuf_read(sb, dev_ofs(sb, segno, 0), sizeof(sh), &sh);
	BUG_ON(err);
	gc_level = GC_LEVEL(sh.level);
	logical_segno = be32_to_cpu(sh.segno);
	if (sh.crc != logfs_crc32(&sh, sizeof(sh), 4)) {
		logfs_mark_segment_bad(sb, segno);
		cleaned = -1;
		goto out;
	}

	for (seg_ofs = LOGFS_SEGMENT_HEADERSIZE;
			seg_ofs + sizeof(oh) < super->s_segsize; ) {
		ofs = dev_ofs(sb, logical_segno, seg_ofs);
		err = wbuf_read(sb, dev_ofs(sb, segno, seg_ofs), sizeof(oh),
				&oh);
		BUG_ON(err);

		if (!memchr_inv(&oh, 0xff, sizeof(oh)))
			break;

		if (oh.crc != logfs_crc32(&oh, sizeof(oh) - 4, 4)) {
			logfs_mark_segment_bad(sb, segno);
			cleaned = super->s_segsize - 1;
			goto out;
		}

		ino = be64_to_cpu(oh.ino);
		bix = be64_to_cpu(oh.bix);
		len = sizeof(oh) + be16_to_cpu(oh.len);
		valid = logfs_is_valid_block(sb, ofs, ino, bix, gc_level);
		if (valid == 1) {
			logfs_cleanse_block(sb, ofs, ino, bix, gc_level);
			cleaned += len;
		} else if (valid == 2) {
			
			cleaned += len;
		}
		seg_ofs += len;
	}
out:
	btree_remove32(&super->s_reserved_segments, segno);
	return cleaned;
}

static struct gc_candidate *add_list(struct gc_candidate *cand,
		struct candidate_list *list)
{
	struct rb_node **p = &list->rb_tree.rb_node;
	struct rb_node *parent = NULL;
	struct gc_candidate *cur;
	int comp;

	cand->list = list;
	while (*p) {
		parent = *p;
		cur = rb_entry(parent, struct gc_candidate, rb_node);

		if (list->sort_by_ec)
			comp = cand->erase_count < cur->erase_count;
		else
			comp = cand->valid < cur->valid;

		if (comp)
			p = &parent->rb_left;
		else
			p = &parent->rb_right;
	}
	rb_link_node(&cand->rb_node, parent, p);
	rb_insert_color(&cand->rb_node, &list->rb_tree);

	if (list->count <= list->maxcount) {
		list->count++;
		return NULL;
	}
	cand = rb_entry(rb_last(&list->rb_tree), struct gc_candidate, rb_node);
	rb_erase(&cand->rb_node, &list->rb_tree);
	cand->list = NULL;
	return cand;
}

static void remove_from_list(struct gc_candidate *cand)
{
	struct candidate_list *list = cand->list;

	rb_erase(&cand->rb_node, &list->rb_tree);
	list->count--;
}

static void free_candidate(struct super_block *sb, struct gc_candidate *cand)
{
	struct logfs_super *super = logfs_super(sb);

	btree_remove32(&super->s_cand_tree, cand->segno);
	kfree(cand);
}

u32 get_best_cand(struct super_block *sb, struct candidate_list *list, u32 *ec)
{
	struct gc_candidate *cand;
	u32 segno;

	BUG_ON(list->count == 0);

	cand = rb_entry(rb_first(&list->rb_tree), struct gc_candidate, rb_node);
	remove_from_list(cand);
	segno = cand->segno;
	if (ec)
		*ec = cand->erase_count;
	free_candidate(sb, cand);
	return segno;
}

static void __add_candidate(struct super_block *sb, struct gc_candidate *cand)
{
	struct logfs_super *super = logfs_super(sb);
	u32 full = super->s_segsize - LOGFS_SEGMENT_RESERVE;

	if (cand->valid == 0) {
		
		log_gc_noisy("add reserve segment %x (ec %x) at %llx\n",
				cand->segno, cand->erase_count,
				dev_ofs(sb, cand->segno, 0));
		cand = add_list(cand, &super->s_reserve_list);
		if (cand) {
			log_gc_noisy("add free segment %x (ec %x) at %llx\n",
					cand->segno, cand->erase_count,
					dev_ofs(sb, cand->segno, 0));
			cand = add_list(cand, &super->s_free_list);
		}
	} else {
		
		if (cand->valid < full)
			cand = add_list(cand, &super->s_low_list[cand->dist]);
		/* good candidates for wear leveling,
		 * segments that were recently written get ignored */
		if (cand)
			cand = add_list(cand, &super->s_ec_list);
	}
	if (cand)
		free_candidate(sb, cand);
}

static int add_candidate(struct super_block *sb, u32 segno, u32 valid, u32 ec,
		u8 dist)
{
	struct logfs_super *super = logfs_super(sb);
	struct gc_candidate *cand;

	cand = kmalloc(sizeof(*cand), GFP_NOFS);
	if (!cand)
		return -ENOMEM;

	cand->segno = segno;
	cand->valid = valid;
	cand->erase_count = ec;
	cand->dist = dist;

	btree_insert32(&super->s_cand_tree, segno, cand, GFP_NOFS);
	__add_candidate(sb, cand);
	return 0;
}

static void remove_segment_from_lists(struct super_block *sb, u32 segno)
{
	struct logfs_super *super = logfs_super(sb);
	struct gc_candidate *cand;

	cand = btree_lookup32(&super->s_cand_tree, segno);
	if (cand) {
		remove_from_list(cand);
		free_candidate(sb, cand);
	}
}

static void scan_segment(struct super_block *sb, u32 segno)
{
	u32 valid, ec = 0;
	gc_level_t gc_level = 0;
	u8 dist;

	if (segment_is_reserved(sb, segno))
		return;

	remove_segment_from_lists(sb, segno);
	valid = logfs_valid_bytes(sb, segno, &ec, &gc_level);
	if (valid == RESERVED)
		return;

	dist = root_distance(sb, gc_level);
	add_candidate(sb, segno, valid, ec, dist);
}

static struct gc_candidate *first_in_list(struct candidate_list *list)
{
	if (list->count == 0)
		return NULL;
	return rb_entry(rb_first(&list->rb_tree), struct gc_candidate, rb_node);
}

static struct gc_candidate *get_candidate(struct super_block *sb)
{
	struct logfs_super *super = logfs_super(sb);
	int i, max_dist;
	struct gc_candidate *cand = NULL, *this;

	max_dist = min(no_free_segments(sb), LOGFS_NO_AREAS - 1);

	for (i = max_dist; i >= 0; i--) {
		this = first_in_list(&super->s_low_list[i]);
		if (!this)
			continue;
		if (!cand)
			cand = this;
		if (this->valid + LOGFS_MAX_OBJECTSIZE <= cand->valid)
			cand = this;
	}
	return cand;
}

static int __logfs_gc_once(struct super_block *sb, struct gc_candidate *cand)
{
	struct logfs_super *super = logfs_super(sb);
	gc_level_t gc_level;
	u32 cleaned, valid, segno, ec;
	u8 dist;

	if (!cand) {
		log_gc("GC attempted, but no candidate found\n");
		return 0;
	}

	segno = cand->segno;
	dist = cand->dist;
	valid = logfs_valid_bytes(sb, segno, &ec, &gc_level);
	free_candidate(sb, cand);
	log_gc("GC segment #%02x at %llx, %x required, %x free, %x valid, %llx free\n",
			segno, (u64)segno << super->s_segshift,
			dist, no_free_segments(sb), valid,
			super->s_free_bytes);
	cleaned = logfs_gc_segment(sb, segno);
	log_gc("GC segment #%02x complete - now %x valid\n", segno,
			valid - cleaned);
	BUG_ON(cleaned != valid);
	return 1;
}

static int logfs_gc_once(struct super_block *sb)
{
	struct gc_candidate *cand;

	cand = get_candidate(sb);
	if (cand)
		remove_from_list(cand);
	return __logfs_gc_once(sb, cand);
}

static int logfs_scan_some(struct super_block *sb)
{
	struct logfs_super *super = logfs_super(sb);
	u32 segno;
	int i, ret = 0;

	segno = super->s_sweeper;
	for (i = SCAN_RATIO; i > 0; i--) {
		segno++;
		if (segno >= super->s_no_segs) {
			segno = 0;
			ret = 1;
			break;
		}

		scan_segment(sb, segno);
	}
	super->s_sweeper = segno;
	return ret;
}

static void __logfs_gc_pass(struct super_block *sb, int target)
{
	struct logfs_super *super = logfs_super(sb);
	struct logfs_block *block;
	int round, progress, last_progress = 0;

	if (super->s_shadow_tree.no_shadowed_segments >= MAX_OBJ_ALIASES)
		logfs_write_anchor(sb);

	if (no_free_segments(sb) >= target &&
			super->s_no_object_aliases < MAX_OBJ_ALIASES)
		return;

	log_gc("__logfs_gc_pass(%x)\n", target);
	for (round = 0; round < SCAN_ROUNDS; ) {
		if (no_free_segments(sb) >= target)
			goto write_alias;

		logfs_write_anchor(sb);
		round += logfs_scan_some(sb);
		if (no_free_segments(sb) >= target)
			goto write_alias;
		progress = logfs_gc_once(sb);
		if (progress)
			last_progress = round;
		else if (round - last_progress > 2)
			break;
		continue;

write_alias:
		if (super->s_no_object_aliases < MAX_OBJ_ALIASES)
			return;
		if (list_empty(&super->s_object_alias)) {
			
			return;
		}
		log_gc("Write back one alias\n");
		block = list_entry(super->s_object_alias.next,
				struct logfs_block, alias_list);
		block->ops->write_block(block);
		round = 0;
	}
	LOGFS_BUG(sb);
}

static int wl_ratelimit(struct super_block *sb, u64 *next_event)
{
	struct logfs_super *super = logfs_super(sb);

	if (*next_event < super->s_gec) {
		*next_event = super->s_gec + WL_RATELIMIT;
		return 0;
	}
	return 1;
}

static void logfs_wl_pass(struct super_block *sb)
{
	struct logfs_super *super = logfs_super(sb);
	struct gc_candidate *wl_cand, *free_cand;

	if (wl_ratelimit(sb, &super->s_wl_gec_ostore))
		return;

	wl_cand = first_in_list(&super->s_ec_list);
	if (!wl_cand)
		return;
	free_cand = first_in_list(&super->s_free_list);
	if (!free_cand)
		return;

	if (wl_cand->erase_count < free_cand->erase_count + WL_DELTA) {
		remove_from_list(wl_cand);
		__logfs_gc_once(sb, wl_cand);
	}
}

/*
 * The journal needs wear leveling as well.  But moving the journal is an
 * expensive operation so we try to avoid it as much as possible.  And if we
 * have to do it, we move the whole journal, not individual segments.
 *
 * Ratelimiting is not strictly necessary here, it mainly serves to avoid the
 * calculations.  First we check whether moving the journal would be a
 * significant improvement.  That means that a) the current journal segments
 * have more wear than the future journal segments and b) the current journal
 * segments have more wear than normal ostore segments.
 * Rationale for b) is that we don't have to move the journal if it is aging
 * less than the ostore, even if the reserve segments age even less (they are
 * excluded from wear leveling, after all).
 * Next we check that the superblocks have less wear than the journal.  Since
 * moving the journal requires writing the superblocks, we have to protect the
 * superblocks even more than the journal.
 *
 * Also we double the acceptable wear difference, compared to ostore wear
 * leveling.  Journal data is read and rewritten rapidly, comparatively.  So
 * soft errors have much less time to accumulate and we allow the journal to
 * be a bit worse than the ostore.
 */
static void logfs_journal_wl_pass(struct super_block *sb)
{
	struct logfs_super *super = logfs_super(sb);
	struct gc_candidate *cand;
	u32 min_journal_ec = -1, max_reserve_ec = 0;
	int i;

	if (wl_ratelimit(sb, &super->s_wl_gec_journal))
		return;

	if (super->s_reserve_list.count < super->s_no_journal_segs) {
		
		return;
	}

	journal_for_each(i)
		if (super->s_journal_seg[i])
			min_journal_ec = min(min_journal_ec,
					super->s_journal_ec[i]);
	cand = rb_entry(rb_first(&super->s_free_list.rb_tree),
			struct gc_candidate, rb_node);
	max_reserve_ec = cand->erase_count;
	for (i = 0; i < 2; i++) {
		struct logfs_segment_entry se;
		u32 segno = seg_no(sb, super->s_sb_ofs[i]);
		u32 ec;

		logfs_get_segment_entry(sb, segno, &se);
		ec = be32_to_cpu(se.ec_level) >> 4;
		max_reserve_ec = max(max_reserve_ec, ec);
	}

	if (min_journal_ec > max_reserve_ec + 2 * WL_DELTA) {
		do_logfs_journal_wl_pass(sb);
	}
}

void logfs_gc_pass(struct super_block *sb)
{
	struct logfs_super *super = logfs_super(sb);

	
	if (super->s_dirty_used_bytes + super->s_dirty_free_bytes
			+ LOGFS_MAX_OBJECTSIZE >= super->s_free_bytes)
		logfs_write_anchor(sb);
	__logfs_gc_pass(sb, super->s_total_levels);
	logfs_wl_pass(sb);
	logfs_journal_wl_pass(sb);
}

static int check_area(struct super_block *sb, int i)
{
	struct logfs_super *super = logfs_super(sb);
	struct logfs_area *area = super->s_area[i];
	gc_level_t gc_level;
	u32 cleaned, valid, ec;
	u32 segno = area->a_segno;
	u64 ofs = dev_ofs(sb, area->a_segno, area->a_written_bytes);

	if (!area->a_is_open)
		return 0;

	if (super->s_devops->can_write_buf(sb, ofs) == 0)
		return 0;

	printk(KERN_INFO"LogFS: Possibly incomplete write at %llx\n", ofs);
	/*
	 * The device cannot write back the write buffer.  Most likely the
	 * wbuf was already written out and the system crashed at some point
	 * before the journal commit happened.  In that case we wouldn't have
	 * to do anything.  But if the crash happened before the wbuf was
	 * written out correctly, we must GC this segment.  So assume the
	 * worst and always do the GC run.
	 */
	area->a_is_open = 0;
	valid = logfs_valid_bytes(sb, segno, &ec, &gc_level);
	cleaned = logfs_gc_segment(sb, segno);
	if (cleaned != valid)
		return -EIO;
	return 0;
}

int logfs_check_areas(struct super_block *sb)
{
	int i, err;

	for_each_area(i) {
		err = check_area(sb, i);
		if (err)
			return err;
	}
	return 0;
}

static void logfs_init_candlist(struct candidate_list *list, int maxcount,
		int sort_by_ec)
{
	list->count = 0;
	list->maxcount = maxcount;
	list->sort_by_ec = sort_by_ec;
	list->rb_tree = RB_ROOT;
}

int logfs_init_gc(struct super_block *sb)
{
	struct logfs_super *super = logfs_super(sb);
	int i;

	btree_init_mempool32(&super->s_cand_tree, super->s_btree_pool);
	logfs_init_candlist(&super->s_free_list, LIST_SIZE + SCAN_RATIO, 1);
	logfs_init_candlist(&super->s_reserve_list,
			super->s_bad_seg_reserve, 1);
	for_each_area(i)
		logfs_init_candlist(&super->s_low_list[i], LIST_SIZE, 0);
	logfs_init_candlist(&super->s_ec_list, LIST_SIZE, 1);
	return 0;
}

static void logfs_cleanup_list(struct super_block *sb,
		struct candidate_list *list)
{
	struct gc_candidate *cand;

	while (list->count) {
		cand = rb_entry(list->rb_tree.rb_node, struct gc_candidate,
				rb_node);
		remove_from_list(cand);
		free_candidate(sb, cand);
	}
	BUG_ON(list->rb_tree.rb_node);
}

void logfs_cleanup_gc(struct super_block *sb)
{
	struct logfs_super *super = logfs_super(sb);
	int i;

	if (!super->s_free_list.count)
		return;

	btree_grim_visitor32(&super->s_cand_tree, 0, NULL);
	logfs_cleanup_list(sb, &super->s_free_list);
	logfs_cleanup_list(sb, &super->s_reserve_list);
	for_each_area(i)
		logfs_cleanup_list(sb, &super->s_low_list[i]);
	logfs_cleanup_list(sb, &super->s_ec_list);
}
